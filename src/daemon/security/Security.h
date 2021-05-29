#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <cpprest/json.h>

#include "User.h"

//////////////////////////////////////////////////////////////////////////
/// Security base implementation based on local JSON file
//////////////////////////////////////////////////////////////////////////
class Security
{
private:
    Security();

public:
    virtual ~Security();
    virtual web::json::value AsJson() const;
    static std::shared_ptr<Security> FromJson(const web::json::value &obj) noexcept(false);
    virtual void save();
    virtual bool encryptKey();
    static std::shared_ptr<Security> init();
    static std::shared_ptr<Security> instance();
    static void instance(std::shared_ptr<Security> instance);

public:
    virtual bool verifyUserKey(const std::string &userName, const std::string &userKey);
    virtual void changeUserPasswd(const std::string &userName, const std::string &newPwd);

    virtual std::shared_ptr<User> getUserInfo(const std::string &userName) const;
    virtual std::shared_ptr<Users> UsersObject() const;
    virtual std::shared_ptr<Roles> RolesObject() const;
    virtual std::set<std::string> getUserPermissions(const std::string &userName);
    virtual std::set<std::string> getAllPermissions();

private:
    std::shared_ptr<JsonSecurity> m_securityConfig;
    static std::shared_ptr<Security> m_instance;
    static std::recursive_mutex m_mutex;
};