// Copyright (c) 2013 - Cody Cutrer

#include "mordor/predef.h"

#include <iostream>

#include <mordor/config.h>
#include <mordor/daemon.h>
#include <mordor/iomanager.h>
#include <mordor/main.h>
#include <mordor/streams/ssl.h>

#include "postguard/postguard.h"

using namespace Mordor;

namespace Postguard {

static int daemonMain(int argc, char *argv[])
{
    try {
        IOManager ioManager(8);
        boost::shared_ptr<SSL_CTX> sslCtx(SSLStream::generateSelfSignedCertificate());
        Postguard postguard(ioManager, "/tmp/.s.PGSQL.5432", sslCtx.get());
        Daemon::onTerminate.connect(boost::bind(&Postguard::stop, &postguard));

        ioManager.stop();
        return 0;
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return -1;
    }
}

}

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        Config::loadFromEnvironment();
        return Daemon::run(argc, argv, &Postguard::daemonMain);
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return -1;
    }
}
