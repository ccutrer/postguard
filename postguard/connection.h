#ifndef __POSTGUARD_CONNECTION_H__
#define __POSTGUARD_CONNECTION_H__

// Copyright (c) 2013 - Cody Cutrer

#include <boost/noncopyable.hpp>

#include <mordor/streams/buffer.h>
#include <mordor/exception.h>

namespace Mordor {
class Stream;
}

namespace Postguard {

class Connection : boost::noncopyable
{
public:
    struct MessageTooShort : virtual Mordor::Exception {};

// internal:
    enum V2MessageType
    {
        SSL_REQUEST         = 80877103,
        STARTUP_REQUEST_V2  = 0x00020000,
        STARTUP_REQUEST_V3  = 0x00030000
    };

    enum V3MessageType
    {
        AUTHENTICATION   = 'R',
        BACKEND_KEY_DATA = 'K',
        COMMAND_COMPLETE = 'C',
        ERROR_RESPONSE   = 'E',
        NOTICE_RESPONSE  = 'N',
        PARAMETER_STATUS = 'S',
        PASSWORD_MESSAGE = 'p',
        QUERY            = 'Q',
        READY_FOR_QUERY  = 'Z',
        TERMINATE        = 'X'
    };

    enum ErrorCode
    {
        SEVERITY = 'S',
        CODE     = 'C',
        MESSAGE  = 'M'
    };

    enum AuthenticationType
    {
        AUTHENTICATION_OK           = 0,
        AUTHENTICATION_MD5_PASSWORD = 5
    };

    enum Status
    {
        IDLE               = 'I',
        IN_TRANSACTION     = 'T',
        TRANSACTION_FAILED = 'E'
    };

public:
    virtual ~Connection() {}

    virtual void close();

    boost::shared_ptr<Mordor::Stream> stream() { return m_stream; }

// internal:
    void readV3Message(V3MessageType &type, Mordor::Buffer &message);
    void writeV3Message(V3MessageType type, const Mordor::Buffer &message);

protected:
    Connection(boost::shared_ptr<Mordor::Stream> stream);

    void readV2Message(V2MessageType &type, Mordor::Buffer &message);

    void writeError(const std::string &severity, const std::string &code, const std::string &message);

    template <class T> static void put(Mordor::Buffer &buffer, const T &value) {
        buffer.copyIn(&value, sizeof(value));
    }
    
protected:
    boost::shared_ptr<Mordor::Stream> m_stream;
};

template <> void Connection::put<std::string>(Mordor::Buffer &message, const std::string &value);

}

#endif

