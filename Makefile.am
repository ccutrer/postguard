SUBDIRS=mordor

ACLOCAL_AMFLAGS=-I m4 -I mordor/m4
AUTOMAKE_OPTIONS=nostdinc subdir-objects
AM_CPPFLAGS=$(BOOST_CPPFLAGS) -I$(srcdir)/mordor -I$(top_srcdir)
AM_CXXFLAGS=-Wall -Werror -fno-strict-aliasing -std=c++11
#AM_LDFLAGS=-rdynamic

sbin_PROGRAMS=			\
	postguard/postguard

nobase_include_HEADERS=			\
	postguard/client.h		\
	postguard/connection.h		\
	postguard/jira.h		\
	postguard/pgpass.h		\
	postguard/postguard.h		\
	postguard/server.h

postguard_postguard_SOURCES=		\
	postguard/client.cpp		\
	postguard/connection.cpp	\
	postguard/jira.cpp		\
	postguard/main.cpp		\
	postguard/pgpass.cpp		\
	postguard/postguard.cpp		\
	postguard/server.cpp
postguard_postguard_LDADD=			\
	mordor/mordor/libmordor.la		\
	$(COREFOUNDATION_FRAMEWORK_LIBS)

