// Copyright (c) 2013 - Cody Cutrer

#include <set>
#include <vector>

#include <openssl/ssl.h>

#include "pgpass.h"

namespace Mordor {
class IOManager;
class Socket;
}

namespace Postguard {

class Client;
class Jira;

class Postguard
{
public:
    Postguard(Mordor::IOManager &ioManager, const std::string &path,
      Jira &jira,
      SSL_CTX *sslCtx = NULL);

    void stop();

    SSL_CTX *sslCtx();

    const PgPassFile &pgPassFile() const { return m_pg_pass_file; }

// internal:
    void closed(std::shared_ptr<Client> client);
    Jira &jira() { return m_jira; }

private:
    void listen();

private:
    Mordor::IOManager &m_ioManager;
    Jira &m_jira;
    std::shared_ptr<Mordor::Socket> m_listen;
    std::set<std::shared_ptr<Client> > m_clients;
    PgPassFile m_pg_pass_file;
    SSL_CTX *m_sslCtx;
};

}
