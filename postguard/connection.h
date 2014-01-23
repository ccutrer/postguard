// Copyright (c) 2013 - Cody Cutrer

#include <boost/noncopyable.hpp>

#include <mordor/streams/buffer.h>
#include <mordor/exception.h>

namespace Mordor {
class Stream;
}

namespace Postguard {

class Postguard;

class Connection : boost::noncopyable
{
public:
    struct MessageTooShort : virtual Mordor::Exception {};

protected:
    enum V2MessageType
    {
        SSL_REQUEST         = 80877103,
        STARTUP_REQUEST_V2  = 0x00020000,
        STARTUP_REQUEST_V3  = 0x00030000
    };

    enum V3MessageType
    {
        AUTHENTICATION  =  'R',
        ERROR_RESPONSE  =  'E',
        READY_FOR_QUERY = 'Z',
        TERMINATE       = 'X'
    };

    enum ErrorCode
    {
        SEVERITY = 'S',
        CODE     = 'C',
        MESSAGE  = 'M'
    };

public:
    virtual ~Connection() {}

    virtual void close();

protected:
    Connection(Postguard &postguard, boost::shared_ptr<Mordor::Stream> stream);

    void readV2Message(V2MessageType &type, Mordor::Buffer &message);
    void readV3Message(V3MessageType &type, Mordor::Buffer &message);
    void writeV3Message(V3MessageType type, const Mordor::Buffer &message);

    void writeError(const std::string &severity, const std::string &code, const std::string &message);

    template <class T> static void put(Mordor::Buffer &buffer, const T &value) {
        buffer.copyIn(&value, sizeof(value));
    }
    
protected:
    Postguard &m_postguard;
    boost::shared_ptr<Mordor::Stream> m_stream;
};

template <> void Connection::put<std::string>(Mordor::Buffer &message, const std::string &value);

}
