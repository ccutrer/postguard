// Copyright (c) 2013 - Cody Cutrer

#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>

#include <openssl/ssl.h>

namespace Mordor {
class IOManager;
class Socket;
}

namespace Postguard {

class Client;

class Postguard
{
public:
    Postguard(Mordor::IOManager &ioManager, const std::string &path,
      SSL_CTX *sslCtx = NULL);

    void stop();

    SSL_CTX *sslCtx();

private:
    void listen();

private:
    Mordor::IOManager &m_ioManager;
    boost::shared_ptr<Mordor::Socket> m_listen;
    std::set<boost::shared_ptr<Client> > m_clients;
    SSL_CTX *m_sslCtx;
};

}
