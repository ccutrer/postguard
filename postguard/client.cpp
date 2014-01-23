// Copyright (c) 2013 - Cody Cutrer

#include <mordor/predef.h>

#include "postguard/client.h"

#include <map>

#include <mordor/endian.h>
#include <mordor/log.h>
#include <mordor/streams/buffer.h>
#include <mordor/streams/buffered.h>
#include <mordor/streams/ssl.h>
#include <mordor/streams/stream.h>

#include "postguard/postguard.h"

using namespace Mordor;

static Logger::ptr g_log = Log::lookup("postguard:client");

namespace Postguard {

Client::Client(Postguard &postguard, Stream::ptr stream, const std::string &user)
    : Connection(postguard, stream),
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
    // TODO: notify the postguard
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

            BufferedStream::ptr bufferedStream = boost::dynamic_pointer_cast<BufferedStream>(m_stream);
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
    put(message, (uint32_t)0u);
    writeV3Message(AUTHENTICATION, message);

    // TODO: establish backend connection

    return true;
}

bool
Client::readyForQuery()
{
    Buffer message;
    put(message, 'I');
    writeV3Message(READY_FOR_QUERY, message);
    m_stream->flush();

    V3MessageType type;
    readV3Message(type, message);

    switch (type) {
        case TERMINATE:
            m_stream->close();
            return false;
        default:
            writeError("ERROR", "08P01", "Unknown message");
            break;
    }
    return true;
}

}
