// Copyright (c) 2013 - Cody Cutrer

#include <mordor/predef.h>

#include "postguard/postguard.h"

#include <pwd.h>

#include <mordor/assert.h>
#include <mordor/iomanager.h>
#include <mordor/socket.h>
#include <mordor/streams/socket.h>

#include "postguard/client.h"

using namespace Mordor;

namespace Postguard {

Postguard::Postguard(IOManager &ioManager, const std::string &path,
    Jira &jira, SSL_CTX *sslCtx)
    : m_ioManager(ioManager),
      m_jira(jira),
      m_sslCtx(sslCtx)
{
    m_pg_pass_file.load();

    UnixAddress address(path);
    m_listen = address.createSocket(ioManager, SOCK_STREAM);
    unlink(path.c_str());
    m_listen->bind(address);
    m_listen->listen();
    ioManager.schedule(std::bind(&Postguard::listen, this));
}

void
Postguard::stop()
{
    m_listen->cancelAccept();
    unlink(std::static_pointer_cast<UnixAddress>(m_listen->localAddress())->path().c_str());
    for (std::set<Client::ptr>::const_iterator it(m_clients.begin());
        it != m_clients.end();
        ++it) {
        (*it)->close();
    }
}

void
Postguard::closed(Client::ptr client)
{
    MORDOR_ASSERT(m_clients.find(client) != m_clients.end());
    m_clients.erase(client);
}

void
Postguard::listen()
{
    while (true) {
        Socket::ptr socket;
        try {
            socket = m_listen->accept();
        } catch (OperationAbortedException &) {
            return;
       }
       Stream::ptr stream(new SocketStream(socket));

       struct ucred creds;
       size_t len = sizeof(struct ucred);
       socket->getOption(SOL_SOCKET, SO_PEERCRED, &creds, &len);

       struct passwd passwd, *result;
       boost::scoped_ptr<char> buffer;
       len = sysconf(_SC_GETPW_R_SIZE_MAX);
       buffer.reset(new char[len]);
       int rc = getpwuid_r(creds.uid, &passwd, buffer.get(), len, &result);
       if (rc)
           MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getpwuid_r");
       std::string user;
       if (result)
           user = passwd.pw_name;

       Client::ptr client(new Client(*this, m_ioManager, stream, user));
       m_clients.insert(client);
       m_ioManager.schedule(std::bind(&Client::run, client));
    }
}

SSL_CTX *
Postguard::sslCtx()
{
    return m_sslCtx;
}

}
