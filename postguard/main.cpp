// Copyright (c) 2013 - Cody Cutrer

#include "mordor/predef.h"

#include <iostream>

#include <mordor/config.h>
#include <mordor/daemon.h>
#include <mordor/iomanager.h>
#include <mordor/main.h>
#include <mordor/streams/ssl.h>

#include "postguard/jira.h"
#include "postguard/postguard.h"

using namespace Mordor;

static ConfigVar<std::string>::ptr g_jiraUri =
    Config::lookup("jira.uri", std::string(), "JIRA URI");
static ConfigVar<std::string>::ptr g_jiraUser =
    Config::lookup("jira.user", std::string(), "JIRA URI");
static ConfigVar<std::string>::ptr g_jiraPassword =
    Config::lookup("jira.password", std::string(), "JIRA Password");

static ConfigVar<std::string>::ptr g_listenPath =
    Config::lookup("postguard.listen",
        std::string("/tmp/.s.PGSQL.5432"),
        "Listen socket");

namespace Postguard {

static int daemonMain(int argc, char *argv[])
{
    try {
        IOManager ioManager(8);
        std::shared_ptr<SSL_CTX> sslCtx(SSLStream::generateSelfSignedCertificate());
        Jira jira(ioManager, g_jiraUri->val(), g_jiraUser->val(), g_jiraPassword->val());
        Postguard postguard(ioManager, g_listenPath->val(), jira, sslCtx.get());
        Daemon::onTerminate.connect(std::bind(&Postguard::stop, &postguard));

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
