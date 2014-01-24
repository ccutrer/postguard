// Copyright (c) 2013 - Cody Cutrer

#include <string>

#include <boost/shared_ptr.hpp>

#include "postguard/connection.h"

namespace Mordor {
class IOManager;
class Stream;
}

namespace Postguard {

class Postguard;
class Server;

class Client : public Connection
{
public:
    typedef boost::shared_ptr<Client> ptr;

public:
    Client(Postguard &postguard, Mordor::IOManager &ioManager,
           boost::shared_ptr<Mordor::Stream> stream,
           const std::string &user);

    void run();

private:
    bool startup();
    bool readyForQuery();
    void proxyQuery(const std::string &query);

private:
    Postguard &m_postguard;
    Mordor::IOManager &m_ioManager;
    std::string m_user;
    boost::shared_ptr<Server> m_server;
};

}
