// Copyright (c) 2014 - Cody Cutrer

#include "postguard/pgpass.h"

#include <sys/stat.h>

#include <boost/lexical_cast.hpp>

#include <mordor/exception.h>
#include <mordor/streams/buffered.h>
#include <mordor/streams/file.h>
#include <mordor/string.h>
#include <mordor/util.h>

using namespace Mordor;

namespace Postguard {

PgPassEntry::PgPassEntry(const std::string &entry)
{
    int index = 0;
    const char *start = entry.c_str();
    const char *end = start + entry.length();
    const char *current = start;
    while (true) {
        if (current == end)
            break;
        // skip quoted chars
        if (*current == '\\')
          current += 2;
        if (*current == ':') {
            switch (index) {
                case 0:
                    m_host = std::string(start, current - start);
                    break;
                case 1:
                    if (current - start == 1 && *start == '*')
                        m_port = 0u;
                    else
                        m_port = boost::lexical_cast<unsigned short>(std::string(start, current - start));
                    break;
                case 2:
                    m_database = std::string(start, current - start);
                    break;
                case 3:
                    m_user = std::string(start, current - start);
                    break;
                default:
                    MORDOR_THROW_EXCEPTION(std::runtime_error("Too many fields in pgpass line"));
            }
            ++index;
            start = current + 1;
        }
        ++current;
    }

    if (index != 4)
        MORDOR_THROW_EXCEPTION(std::runtime_error("Too few fields in pgpass line"));
    m_password = std::string(start, current - start);
}


bool
PgPassEntry::matches(const std::string &host, unsigned short port,
    const std::string &database, const std::string &user) const
{
    return (m_host == "*" || m_host == host) &&
           (m_port == 0u || m_port == port) &&
           (m_database == "*" || m_database == database) &&
           (m_user == "*" || m_user == user);
}

void
PgPassFile::load(const std::string &filename)
{
    std::string filename2 = filename, home;
    std::map<std::string, std::string>::const_iterator it;
    if (filename2.empty()) {
        if ( (it = env().find("PGPASSFILE")) != env().end())
            filename2 = it->second;
        else
            filename2 = "~/.pgpass";
    }
    if ( (it = env().find("HOME")) != env().end())
        replace(filename2, "~", it->second);
    struct stat stats;
    if (stat(filename2.c_str(), &stats) == 0 && (stats.st_mode & 0777) == 0600) {
        Stream::ptr pgpass(new FileStream(filename2, FileStream::READ));
        pgpass.reset(new BufferedStream(pgpass));
        while (true) {
            std::string line = pgpass->getDelimited('\n', true);
            if (line.empty())
                break;
            bool eof = (line.back() != '\n');
            if (!eof)
                line = line.substr(0, line.length() - 1);
            push_back(PgPassEntry(line));
            if (eof)
                break;
        }
    }
}

PgPassFile::const_iterator
PgPassFile::find(const std::string &host, unsigned short port,
    const std::string &database, const std::string &user) const
{
    return std::find_if(begin(), end(),
        [&, host, port, database, user](const PgPassEntry &entry)
        { return entry.matches(host, port, database, user); });
}

}

