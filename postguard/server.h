// Copyright (c) 2013 - Cody Cutrer

#include <string>

#include <boost/shared_ptr.hpp>

#include "postguard/connection.h"

namespace Mordor {
class IOManager;
struct URI;
}

namespace Postguard {

class Server : public Connection
{
public:
    typedef boost::shared_ptr<Server> ptr;

private:
    Server(boost::shared_ptr<Mordor::Stream> stream);

public:
    static ptr connect(Mordor::IOManager &ioManager,
        const std::map<std::string, std::string> &parameters);

    unsigned int pid() const { return m_pid; }
    unsigned int secretKey() const { return m_secretKey; }
    Status status() const { return m_status; }
    const std::map<std::string, std::string> &parameters() const { return m_parameters; }

private:
    void connect(const std::string &host, const std::string &sslMode,
        const std::map<std::string, std::string> &parameters);
    void startSSL(const std::string &host, const std::string &sslMode);
    static std::map<ErrorCode, std::string> readErrorMessages(Mordor::Buffer &message);
    static bool clientParameter(const std::string &name);

private:
    unsigned int m_pid, m_secretKey;
    Status m_status;
    std::map<std::string, std::string> m_parameters;
};

}
