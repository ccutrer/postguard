// Copyright (c) 2013 - Cody Cutrer

#include <mordor/predef.h>

#include "postguard/server.h"

#include <map>

#include <boost/lexical_cast.hpp>

#include <mordor/endian.h>
#include <mordor/log.h>
#include <mordor/streams/buffer.h>
#include <mordor/streams/buffered.h>
#include <mordor/streams/socket.h>
#include <mordor/streams/ssl.h>
#include <mordor/streams/stream.h>

#include "postguard/postguard.h"

using namespace Mordor;

static Logger::ptr g_log = Log::lookup("postguard:server");

namespace Postguard {

Server::Server(Stream::ptr stream)
    : Connection(stream)
{}

Server::ptr
Server::connect(IOManager &ioManager, const std::map<std::string, std::string> &parameters)
{
    std::string host, hostaddr, sslmode;
    unsigned short port;
    std::map<std::string, std::string>::const_iterator it;

    if ( (it = parameters.find("port")) != parameters.end()) {
        port = boost::lexical_cast<unsigned short>(it->second);
    } else {
        port = 5432;
    }

    if ( (it = parameters.find("host")) != parameters.end()) {
        host = it->second;
    } else {
        host = "/tmp";
    }

    if ( (it = parameters.find("hostaddr")) != parameters.end()) {
        hostaddr = it->second;
    }

    if ( (it = parameters.find("sslmode")) != parameters.end()) {
        sslmode = it->second;
    } else {
        sslmode = "prefer";
    }

    std::vector<Address::ptr> addresses;
    if (!hostaddr.empty()) {
       addresses.push_back(IPAddress::create(hostaddr.c_str(), port));
    } else if (!host.empty() && host[0] == '/') {
       std::ostringstream path;
       path << host << "/.s.PGSQL." << port;
       UnixAddress::ptr address(new UnixAddress(path.str()));
       addresses.push_back(address);
       sslmode = "disable";
    } else {
        addresses = Address::lookup(host, AF_UNSPEC, SOCK_STREAM, 0);
        for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
            it != addresses.end();
            ++it) {
            boost::static_pointer_cast<IPAddress>(*it)->port(port);
        }
    }

    Stream::ptr stream;
    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();) {
        Socket::ptr socket = (*it)->createSocket(ioManager, SOCK_STREAM);
        try {
            socket->connect(*it);
            stream.reset(new SocketStream(socket));
            break;
        } catch (...) {
            if (++it == addresses.end())
                throw;
            continue;
        }
    }

    Server::ptr server(new Server(stream));
    server->connect(host, sslmode, parameters);
    return server;
}

void
Server::connect(const std::string &host, const std::string &sslmode,
    const std::map<std::string, std::string> &parameters)
{
    if (sslmode == "prefer" || sslmode == "require" || sslmode == "verify-ca" || sslmode == "verify-full") {
        startSSL(host, sslmode);
    }

    Buffer message;
    put(message, byteswap(STARTUP_REQUEST_V3));
    for (std::map<std::string, std::string>::const_iterator it(parameters.begin());
        it != parameters.end();
        ++it) {
        if (clientParameter(it->first))
            continue;
        put(message, it->first);
        put(message, it->second);
    }
    put(message, '\0');

    unsigned int length = message.readAvailable() + 4;
    length = byteswap(length);
    m_stream->write(&length, 4);
    m_stream->write(message, message.readAvailable());
    m_stream->flush();

    V3MessageType type;
    bool more = true;
    AuthenticationType authenticationType;
    while (more) {
        message.clear();
        readV3Message(type, message);

        switch (type) {
            case AUTHENTICATION:
                if (message.readAvailable() != 4u)
                    MORDOR_THROW_EXCEPTION(std::runtime_error("malformed Authentication message"));
                message.copyOut(&authenticationType, 4u);
                switch (authenticationType) {
                    case AUTHENTICATION_OK:
                        more = false;
                        break;
                    default:
                        // Call a callback for credentials
                        MORDOR_THROW_EXCEPTION(std::runtime_error("Unsupported authentication type required"));
                }
                break;
            case ERROR_RESPONSE:
                MORDOR_THROW_EXCEPTION(std::runtime_error("got an error"));
            default:
                MORDOR_THROW_EXCEPTION(std::runtime_error("unknown response from server"));
        }
    }

    while (true) {
        message.clear();
        readV3Message(type, message);

        switch (type) {
            case BACKEND_KEY_DATA:
                if (message.readAvailable() != 8u)
                    MORDOR_THROW_EXCEPTION(std::runtime_error("malformed BackendKeyData message"));
                message.copyOut(&m_pid, 4u);
                message.copyOut(&m_secretKey, 4u);
                break;
            case PARAMETER_STATUS:
            {
                std::string name = message.getDelimited('\0', false, false);
                m_parameters[name] = message.getDelimited('\0', false, false);
                if (message.readAvailable() != 0u)
                    MORDOR_THROW_EXCEPTION(std::runtime_error("malformed ParameterStatus message"));
                break;
            }
            case NOTICE_RESPONSE:
            {
                std::map<ErrorCode, std::string> messages = readErrorMessages(message);
                MORDOR_LOG_INFO(g_log) << this << messages[SEVERITY] << ":  " << messages[MESSAGE];
                continue;
            }
            case READY_FOR_QUERY:
                if (message.readAvailable() != 1u)
                    MORDOR_THROW_EXCEPTION(std::runtime_error("malformed ReadyForQuery message"));
                char status;
                message.copyOut(&status, 1u);
                m_status = (Status)status;
                return;
            case ERROR_RESPONSE:
            {
                std::map<ErrorCode, std::string> messages = readErrorMessages(message);
                MORDOR_THROW_EXCEPTION(std::runtime_error(messages[MESSAGE]));
            }
            default:
                MORDOR_THROW_EXCEPTION(std::runtime_error("unknown response from server"));
        }
    }
}

void
Server::startSSL(const std::string &host, const std::string &sslmode)
{
    unsigned int length = 8;
    length = byteswap(length);
    m_stream->write(&length, 4);
    V2MessageType type = SSL_REQUEST;
    m_stream->write(&type, 4);
    m_stream->flush();

    char response;
    m_stream->read(&response, 1);
    if (response == 'S') {
        BufferedStream::ptr bufferedStream = boost::dynamic_pointer_cast<BufferedStream>(m_stream);
        // optimize the buffering on top of the socket for SSL packets
        bufferedStream->allowPartialReads(true);
        bufferedStream->flushMultiplesOfBuffer(true);
        bufferedStream->bufferSize(16384);

        SSLStream::ptr sslStream(new SSLStream(m_stream));
        if (!host.empty())
            sslStream->serverNameIndication(host);
        sslStream->connect();
        sslStream->flush();
        if (sslmode == "verify-ca")
            sslStream->verifyPeerCertificate();
        else if (sslmode == "verify-full")
            sslStream->verifyPeerCertificate(host);
        m_stream.reset(new BufferedStream(sslStream));
    } else if (response == 'N') {
        if (sslmode == "prefer")
            return;
        MORDOR_THROW_EXCEPTION(std::runtime_error("SSL required, but not available"));
    } else {
        MORDOR_THROW_EXCEPTION(std::runtime_error("Invalid response to SSL request"));
    }
}

std::map<Connection::ErrorCode, std::string>
Server::readErrorMessages(Buffer &message)
{
   std::map<ErrorCode, std::string> messages;
   while (true) {
       if (message.readAvailable() == 0u)
           MORDOR_THROW_EXCEPTION(std::runtime_error("Malformed ErrorResponse message"));

       char code;
       message.copyOut(&code, 1u);
       message.consume(1u);
       if (code == '\0') {
           if (message.readAvailable() != 0u)
               MORDOR_THROW_EXCEPTION(std::runtime_error("Malformed ErrorResponse message"));
           return messages;
       }
       messages[(ErrorCode)code] = message.getDelimited('\0', false, false);
    }
}

bool
Server::clientParameter(const std::string &name)
{
    return name == "host" ||
           name == "hostaddr" ||
           name == "port" ||
           name == "sslmode";
}

}
