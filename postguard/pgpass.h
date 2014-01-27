#ifndef __POSTGUARD_PGPASS_H__
#define __POSTGUARD_PGPASS_H__
// Copyright (c) 2014 - Cody Cutrer

#include <string>
#include <vector>

namespace Postguard {

struct PgPassEntry
{
public:
    PgPassEntry(const std::string &entry);

    bool matches(const std::string &host, unsigned short port,
                 const std::string &database, const std::string &user) const;

    std::string password() const { return m_password; }

private:
     std::string m_host, m_database, m_user, m_password;
     unsigned short m_port;
};

class PgPassFile : public std::vector<PgPassEntry>
{
public:
    PgPassFile() {};

    void load(const std::string &filename = std::string());

    const_iterator find(const std::string &host, unsigned short port,
                        const std::string &database, const std::string &user) const;
};

}


#endif

