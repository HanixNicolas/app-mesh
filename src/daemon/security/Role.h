#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <cpprest/json.h>

//////////////////////////////////////////////////////////////////////////
/// Role
//////////////////////////////////////////////////////////////////////////
class Role
{
public:
	explicit Role(const std::string &name);
	virtual ~Role();

	// serialize
	web::json::value AsJson() const;
	static std::shared_ptr<Role> FromJson(std::string roleName, web::json::value &obj) noexcept(false);

	// get infomation
	bool hasPermission(std::string permission);
	const std::set<std::string> getPermissions();
	const std::string getName() const;

private:
	std::set<std::string> m_permissions;
	std::string m_name;
	mutable std::recursive_mutex m_mutex;
};

class Roles
{
public:
	Roles();
	virtual ~Roles();

	std::shared_ptr<Role> getRole(const std::string &roleName);

	web::json::value AsJson() const;
	static const std::shared_ptr<Roles> FromJson(const web::json::value &obj) noexcept(false);

	void addRole(const web::json::value &obj, std::string name);
	void delRole(std::string name);

private:
	std::map<std::string, std::shared_ptr<Role>> m_roles;
	mutable std::recursive_mutex m_mutex;
};
