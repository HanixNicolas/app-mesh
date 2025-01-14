#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include "../../src/common/DateTime.h"
#include "../../src/common/Utility.h"
#include "../../src/daemon/security/ldapplugin/ldapcpp/cldap.h"
#include "../catch.hpp"
#include <ace/Init_ACE.h>
#include <ace/OS.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <log4cpp/Appender.hh>
#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Priority.hh>
#include <log4cpp/RollingFileAppender.hh>
#include <set>
#include <string>
#include <thread>
#include <time.h>

TEST_CASE("ldapcpp Test", "[security]")
{

    SECTION("password verification")
    {
        Ldap::Server ldap;
        bool success;
        ldap.Connect("ldap://127.0.0.1:389");
        std::cout << "ldap connect: " << ldap.Message() << std::endl;

        success = ldap.Bind("cn=admin,ou=users,dc=example,dc=org", "Admin123");
        std::cout << "user <admin> bind success: " << success << std::endl;

        success = ldap.Bind("cn=user,ou=users,dc=example,dc=org", "User123");
        std::cout << "user <user> bind success: " << success << std::endl;

        success = ldap.Bind("cn=test,ou=users,dc=example,dc=org", "123");
        std::cout << "user <test> bind success: " << success << std::endl;
    }

    SECTION("search")
    {
        Ldap::Server ldap;
        bool success;
        ldap.Connect("ldap://127.0.0.1:389");
        std::cout << "ldap connect: " << ldap.Message() << std::endl;

        success = ldap.Bind("cn=admin,dc=example,dc=org", "admin");
        std::cout << "user <admin> bind success: " << success << std::endl;

        //Base64::SetBinaryOnly(true);
        auto result = ldap.Search("ou=users,dc=example,dc=org", Ldap::ScopeTree, "sn=*");
        std::cout << "search developers: " << result.size() << std::endl;
        for (auto &entry : result)
        {
            std::cout << "user: " << entry.DN() << std::endl;
            std::cout << " - sn:" << entry.GetStringValue("sn") << std::endl;
            std::cout << " - gidNumber:" << entry.GetStringValue("gidNumber") << std::endl;
        }
    }
}
