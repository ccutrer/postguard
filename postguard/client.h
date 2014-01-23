// Copyright (c) 2013 - Cody Cutrer

#include <string>

#include <boost/shared_ptr.hpp>

#include "postguard/connection.h"

namespace Mordor {
class Stream;
}

namespace Postguard {

class Client : public Connection
{
public:
    typedef boost::shared_ptr<Client> ptr;
    struct InvalidProtocolException : virtual Mordor::Exception {};

public:
    Client(Postguard &postguard, boost::shared_ptr<Mordor::Stream> stream,
           const std::string &user);

    void run();

private:
    void startup();

private:
    std::string m_user;
};

}
