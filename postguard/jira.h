#ifndef __POSTGUARD_JIRA_H__
#define __POSTGUARD_JIRA_H__
// Copyright (c) 2014 - Cody Cutrer

#include <memory>
#include <string>

#include "mordor/uri.h"

namespace Mordor {
class IOManager;

namespace HTTP {
class RequestBroker;
}

}

namespace Postguard {

class Jira
{
public:
    Jira(Mordor::IOManager &ioManager, const Mordor::URI &baseUri, const std::string &username,
        const std::string &password);

    bool issueExists(const std::string &key);

private:
    const Mordor::URI m_baseUri;
    const std::string m_username, m_password;
    std::shared_ptr<Mordor::HTTP::RequestBroker> m_requestBroker;
};

}

#endif

