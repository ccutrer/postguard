// Copyright (c) 2013 - Cody Cutrer

#include <mordor/predef.h>

#include "postguard/connection.h"

#include <mordor/assert.h>
#include <mordor/streams/buffered.h>
#include <mordor/endian.h>
#include <mordor/log.h>

using namespace Mordor;

static Logger::ptr g_log = Log::lookup("postguard:connection");

namespace Postguard {

Connection::Connection(Postguard &postguard, Stream::ptr stream)
    : m_postguard(postguard)
{
    MORDOR_ASSERT(stream->supportsRead());
    MORDOR_ASSERT(stream->supportsWrite());
    MORDOR_ASSERT(!stream->supportsSeek());
    m_stream.reset(new BufferedStream(stream));
}


void
Connection::close()
{
    m_stream->cancelRead();
}

void
Connection::readV2Message(V2MessageType &type, Buffer &message)
{
    unsigned int length;
    m_stream->read(&length, 4u);
    length = byteswap(length);
    if (length < 8)
        MORDOR_THROW_EXCEPTION(MessageTooShort());

    m_stream->read(&type, 4u);
    type = byteswap(type);

    if (length > 8)
        m_stream->read(message, length - 8);
}

void
Connection::readV3Message(V3MessageType &type, Buffer &message)
{
    char typeChar;
    m_stream->read(&typeChar, 1u);
    MORDOR_LOG_DEBUG(g_log) << this << " read message " << typeChar;
    type = (V3MessageType)typeChar;
    unsigned int length;
    m_stream->read(&length, 4u);
    length = byteswap(length);
    if (length < 4)
        MORDOR_THROW_EXCEPTION(MessageTooShort());

    if (length > 4)
        m_stream->read(message, length - 4);
}

void
Connection::writeV3Message(V3MessageType type, const Buffer &message)
{
    char typeChar = (char)type;
    MORDOR_LOG_DEBUG(g_log) << this << " sending message " << typeChar;
    m_stream->write(&typeChar, 1u);

    unsigned int length = message.readAvailable() + 4;
    length = byteswap(length);
    m_stream->write(&length, 4u);

    if (message.readAvailable())
        m_stream->write(message, message.readAvailable());
}

void
Connection::writeError(const std::string &severity, const std::string &code,
    const std::string &message)
{
    Buffer buffer;
    put(buffer, (char)SEVERITY);
    put(buffer, severity);
    put(buffer, (char)CODE);
    put(buffer, code);
    put(buffer, (char)MESSAGE);
    put(buffer, message);
    put(buffer, (char)0);
    writeV3Message(ERROR_RESPONSE, buffer);
    m_stream->flush();
}

template <>
void
Connection::put<std::string>(Buffer &buffer, const std::string &value)
{
    buffer.copyIn(value.c_str(), value.length() + 1);
}

}
