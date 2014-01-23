// Copyright (c) 2013 - Cody Cutrer

#include <mordor/predef.h>

#include "postguard/postguard.h"

#include <mordor/iomanager.h>
#include <mordor/socket.h>
#include <mordor/streams/socket.h>

using namespace Mordor;

namespace Postguard {

Postguard::Postguard(IOManager &ioManager, const std::string &path,
    SSL_CTX *sslCtx)
    :// m_ioManager(ioManager),
      m_sslCtx(sslCtx)
{
    UnixAddress address(path);
    m_listen = address.createSocket(ioManager, SOCK_STREAM);
    m_listen->bind(address);
    m_listen->listen();
    ioManager.schedule(boost::bind(&Postguard::listen, this));
}

void
Postguard::stop()
{
    m_listen->cancelAccept();
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
    }
}

SSL_CTX *
Postguard::sslCtx()
{
    return m_sslCtx;
}

}
