// Copyright (c) 2014 - Cody Cutrer

#include "jira.h"

#include "mordor/http/basic.h"
#include "mordor/http/broker.h"
#include "mordor/http/client.h"

using namespace Mordor;
using namespace Postguard;

Jira::Jira(IOManager &ioManager, const URI& baseUri, const std::string &username,
    const std::string &password)
  : m_baseUri(baseUri),
    m_username(username),
    m_password(password)
{
    HTTP::RequestBrokerOptions options;
    options.ioManager = &ioManager;
    m_requestBroker = HTTP::createRequestBroker(options).first;
}

bool
Jira::issueExists(const std::string &key)
{
    HTTP::Request requestHeaders;
    requestHeaders.requestLine.method = HTTP::HEAD;
    requestHeaders.requestLine.uri = m_baseUri;
    requestHeaders.requestLine.uri.path = "/rest/api/2/issue/";
    requestHeaders.requestLine.uri.path.append(key);
    requestHeaders.request.host = m_baseUri.authority.host();
    HTTP::BasicAuth::authorize(requestHeaders.request.authorization, m_username,
        m_password);

    HTTP::ClientRequest::ptr request = m_requestBroker->request(requestHeaders);
    if (request->response().status.status == HTTP::NOT_FOUND)
        return false;

    if (request->response().status.status != HTTP::OK) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("Unknown response from JIRA"));
    }
    return true;
}
