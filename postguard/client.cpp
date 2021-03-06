// Copyright (c) 2013 - Cody Cutrer

#include <mordor/predef.h>

#include "postguard/client.h"

#include <map>
#include <regex>

#include <mordor/endian.h>
#include <mordor/fiber.h>
#include <mordor/log.h>
#include <mordor/parallel.h>
#include <mordor/streams/buffer.h>
#include <mordor/streams/buffered.h>
#include <mordor/streams/null.h>
#include <mordor/streams/ssl.h>
#include <mordor/streams/stream.h>
#include <mordor/streams/transfer.h>

#include "postguard/jira.h"
#include "postguard/postguard.h"
#include "postguard/server.h"

using namespace Mordor;

static Logger::ptr g_log = Log::lookup("postguard:client");

namespace Postguard {

Client::Client(Postguard &postguard, IOManager &ioManager, Stream::ptr stream,
    const std::string &user)
    : Connection(stream),
      m_postguard(postguard),
      m_ioManager(ioManager),
      m_user(user)
{}

void
Client::run()
{
    try {
        if (!startup()) {
            return;
        }
        while (readyForQuery());
    } catch(OperationAbortedException &) {
    } catch(...) {
        MORDOR_LOG_ERROR(g_log) << this << " Unexpected exception: " << boost::current_exception_diagnostic_information();
    }
    m_postguard.closed(shared_from_this());
}

bool
Client::startup()
{
    V2MessageType type;
    Buffer message;

    MORDOR_LOG_VERBOSE(g_log) << this << " starting connection from " << m_user;

    readV2Message(type, message);

    if (type == SSL_REQUEST) {
        if (message.readAvailable() != 0) {
            writeError("ERROR", "08P01", "Invalid SSLRequest message");
            m_stream->close();
            return false;
        }

        if (m_postguard.sslCtx()) {
            MORDOR_LOG_VERBOSE(g_log) << this << " accepting SSL request";
            m_stream->write("S", 1u);
            m_stream->flush();

            BufferedStream::ptr bufferedStream = std::dynamic_pointer_cast<BufferedStream>(m_stream);
            // optimize the buffering on top of the socket for SSL packets
            bufferedStream->allowPartialReads(true);
            bufferedStream->flushMultiplesOfBuffer(true);
            bufferedStream->bufferSize(16384);

            SSLStream::ptr sslStream(new SSLStream(m_stream, false, true, m_postguard.sslCtx()));
            sslStream->accept();
            sslStream->flush();
            m_stream.reset(new BufferedStream(sslStream));
        } else {
            MORDOR_LOG_VERBOSE(g_log) << this << " rejecting SSL request";
            m_stream->write("N", 1u);
            m_stream->flush();
        }

        readV2Message(type, message);
    }

    if (type != STARTUP_REQUEST_V3) {
        writeError("ERROR", "08P01", "Expected StartupMessage");
        m_stream->close();
        return false;
    }

    std::map<std::string, std::string> parameters;
    while (true) {
        std::string name = message.getDelimited('\0');
        if (name.length() == 1)
            break;
        name.resize(name.size() - 1);
        std::string value = message.getDelimited('\0', false, false);

        parameters[name] = value;
        MORDOR_LOG_VERBOSE(g_log) << this << " received parameter " << name
            << ": " << value;
    }

    if (message.readAvailable() != 0) {
        writeError("ERROR", "08P01", "Invalid StartupMessage message");
        m_stream->close();
        return false;
    }

    if (parameters.find("user") == parameters.end()) {
        writeError("ERROR", "28P01", "Must provide a username");
        m_stream->close();
        return false;
    }

    const std::string &user = parameters["user"];
    if (!std::equal(m_user.begin(), m_user.end(), user.begin())) {
        writeError("ERROR", "28P01", "Requested user is not a suffix of your Unix username");
        m_stream->close();
        return false;
    }

    message.clear();
    put(message, AUTHENTICATION_OK);
    writeV3Message(AUTHENTICATION, message);

    std::map<std::string, std::string> server_parameters;
    Server::applyEnvironmentVariables(server_parameters);

    // Translate on-wire "database" to "dbname" used in connection string
    std::map<std::string, std::string>::iterator it;
    if ( (it = parameters.find("database")) != parameters.end()) {
        parameters["dbname"] = it->second;
        parameters.erase(it);
    }

    server_parameters.insert(parameters.begin(), parameters.end());
    try {
        m_server = Server::connect(m_ioManager, server_parameters,
            &m_postguard.pgPassFile());
    } catch (...) {
        MORDOR_LOG_ERROR(g_log) << this << " Unable to connect to server: " <<
            boost::current_exception_diagnostic_information();
        writeError("ERROR", "08000", "Unable to connect to server");
        m_stream->close();
        return false;
    }

    message.clear();
    put(message, m_server->pid());
    put(message, m_server->secretKey());
    writeV3Message(BACKEND_KEY_DATA, message);

    for (std::map<std::string, std::string>::const_iterator it(m_server->parameters().begin());
        it != m_server->parameters().end();
        ++it) {
        message.clear();
        message.copyIn(it->first);
        message.copyIn("\0", 1u);
        message.copyIn(it->second);
        message.copyIn("\0", 1u);
        writeV3Message(PARAMETER_STATUS, message);
    }

    return true;
}

static void transfer(Stream::ptr from, Stream::ptr to)
{
    transferStream(from, to);
}

bool
Client::readyForQuery()
{
    Buffer message;
    put(message, (char)IDLE);
    writeV3Message(READY_FOR_QUERY, message);
    m_stream->flush();

    V3MessageType type;
    message.clear();
    readV3Message(type, message);

    switch (type) {
        case QUERY:
        {
            static const std::regex show_set_query("^(?:SHOW|SET)[^;]+;?$", std::regex::icase);
            static const std::regex go_query("^GO ([A-Z]+-[0-9]+);?$", std::regex::icase);
            std::string query = message.getDelimited('\0', false, false);
            std::smatch what;
            if (message.readAvailable() != 0u) {
                writeError("ERROR", "08P01", "Malformed Query message");
            } else if (std::regex_match(query, show_set_query)) {
                proxyQuery(query);
            } else if (std::regex_match(query, what, go_query)) {
                return go(what[1]);
            } else {
                writeError("ERROR", "42601", "Postguard only understands \"GO JIRA-1\"");
            }
            break;
        }
        case TERMINATE:
            m_stream->close();
            return false;
        default:
            writeError("ERROR", "08P01", "Unknown message");
            break;
    }
    return true;
}

void
Client::proxyQuery(const std::string &query)
{
    Buffer buffer;
    put(buffer, query);
    m_server->writeV3Message(QUERY, buffer);
    m_server->stream()->flush();

    while (true) {
        V3MessageType type;
        buffer.clear();
        m_server->readV3Message(type, buffer);

        switch (type) {
            case READY_FOR_QUERY:
                return;
            default:
                writeV3Message(type, buffer);
        }
    }
}

bool
Client::go(const std::string &key)
{
    Buffer message;
    bool issueExists;
    try {
        issueExists = m_postguard.jira().issueExists(key);
    } catch(...) {
        MORDOR_LOG_ERROR(g_log) << this << " Could not determine if " << key << " exists: " <<
            boost::current_exception_diagnostic_information();
        writeError("ERROR", "58030", "Unable to contact JIRA");
        return true;
    }

    if (issueExists) {
        MORDOR_LOG_INFO(g_log) << this << " " << m_user << " referenced issue " << key;
        put(message, "GO");
        writeV3Message(COMMAND_COMPLETE, message);
        message.clear();
        put(message, (char)IDLE);
        writeV3Message(READY_FOR_QUERY, message);
        m_stream->flush();
        m_server->stream()->flush();
        FilterStream::ptr clientBuffered = std::static_pointer_cast<FilterStream>(m_stream);
        FilterStream::ptr serverBuffered = std::static_pointer_cast<FilterStream>(m_server->stream());
        Stream::ptr client = clientBuffered->parent();
        Stream::ptr server = serverBuffered->parent();
        clientBuffered->parent(NullStream::get_ptr());
        serverBuffered->parent(NullStream::get_ptr());
        transferStream(clientBuffered, server);
        transferStream(serverBuffered, client);
        std::vector<std::function<void ()> > dgs;
        dgs.push_back(boost::bind(&transfer, server, client));
        dgs.push_back(boost::bind(&transfer, client, server));
        parallel_do(dgs);
        return false;
    } else {
        MORDOR_LOG_WARNING(g_log) << this << " " << m_user << " referenced non-existent issue " << key;
        std::ostringstream os;
        os << "Issue " << key << " does not exist";
        writeError("ERROR", "42704", os.str());
        return true;
    }
}

}
