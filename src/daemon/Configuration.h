#pragma once

#include <cpprest/json.h>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class RestHandler;
class User;
class Label;
class Application;

/// <summary>
/// Configuration file <appsvc.json> parse/update
/// </summary>
class Configuration
{
public:
	struct JsonSsl
	{
		static std::shared_ptr<JsonSsl> FromJson(const web::json::value &jsonObj);
		web::json::value AsJson() const;
		bool m_sslEnabled;
		std::string m_certFile;
		std::string m_certKeyFile;
		JsonSsl();
	};

	struct JsonJwt
	{
		JsonJwt();
		static std::shared_ptr<JsonJwt> FromJson(const web::json::value &jsonObj);
		web::json::value AsJson() const;

		bool m_jwtEnabled;
		std::string m_jwtSalt;
		std::string m_jwtInterface;
	};

	struct JsonRest
	{
		JsonRest();
		static std::shared_ptr<JsonRest> FromJson(const web::json::value &jsonObj);
		web::json::value AsJson() const;

		bool m_restEnabled;
		int m_httpThreadPoolSize;
		int m_restListenPort;
		int m_promListenPort;
		std::string m_restListenAddress;
		int m_separateRestInternalPort;
		std::string m_dockerProxyListenAddr;
		std::shared_ptr<JsonSsl> m_ssl;
		std::shared_ptr<JsonJwt> m_jwt;
	};

	struct JsonConsul
	{
		JsonConsul();
		static std::shared_ptr<JsonConsul> FromJson(const web::json::value &jsonObj, int appmeshRestPort, bool sslEnabled);
		web::json::value AsJson() const;
		bool consulEnabled() const;
		bool consulSecurityEnabled() const;
		const std::string appmeshUrl() const;

		bool m_isMaster;
		bool m_isWorker;
		// http://consul.service.consul:8500
		std::string m_consulUrl;
		// appmesh proxy url, used to report to Consul to expose local appmesh listen port address
		std::string m_proxyUrl;
		// in case of not set m_proxyUrl, use default dynamic value https://localhost:6060
		std::string m_defaultProxyUrl;
		// TTL (string: "") - Specifies the number of seconds (between 10s and 86400s).
		int m_ttl;
		bool m_securitySync;
		std::string m_basicAuthUser;
		std::string m_basicAuthPass;
	};

	Configuration();
	virtual ~Configuration();

	static std::shared_ptr<Configuration> instance();
	static void instance(std::shared_ptr<Configuration> config);
	static std::string readConfiguration();
	static void handleSignal();

	static std::shared_ptr<Configuration> FromJson(const std::string &str, bool applyEnv = false) noexcept(false);
	web::json::value AsJson(bool returnRuntimeInfo, const std::string &user);
	void deSerializeApp(const web::json::value &jsonObj);
	void saveConfigToDisk();
	void hotUpdate(const web::json::value &config);
	static void readConfigFromEnv(web::json::value &jsonConfig);
	static bool applyEnvConfig(web::json::value &jsonValue, std::string envValue);
	void registerPrometheus();

	std::vector<std::shared_ptr<Application>> getApps() const;
	std::shared_ptr<Application> addApp(const web::json::value &jsonApp);
	void removeApp(const std::string &appName);
	std::shared_ptr<Application> parseApp(const web::json::value &jsonApp);

	int getScheduleInterval();
	int getRestListenPort();
	int getPromListenPort();
	std::string getRestListenAddress();
	std::string getDockerProxyAddress() const;
	int getSeparateRestInternalPort();
	web::json::value serializeApplication(bool returnRuntimeInfo, const std::string &user) const;
	std::shared_ptr<Application> getApp(const std::string &appName) const noexcept(false);
	bool isAppExist(const std::string &appName);
	void disableApp(const std::string &appName);
	void enableApp(const std::string &appName);
	const web::json::value getDockerProxyAppJson() const;

	std::shared_ptr<Label> getLabel() { return m_label; }

	const std::string getLogLevel() const;
	const std::string getDefaultExecUser() const;
	const std::string getDefaultWorkDir() const;
	bool getSslEnabled() const;
	std::string getSSLCertificateFile() const;
	std::string getSSLCertificateKeyFile() const;
	bool getRestEnabled() const;
	bool getJwtEnabled() const;
	std::size_t getThreadPoolSize() const;
	const std::string getDescription() const;

	const std::shared_ptr<Configuration::JsonConsul> getConsul() const;
	const std::shared_ptr<JsonJwt> getJwt() const;
	bool checkOwnerPermission(const std::string &user, const std::shared_ptr<User> &appOwner, int appPermission, bool requestWrite) const;

	void dump();

private:
	void addApp2Map(std::shared_ptr<Application> app);

private:
	std::vector<std::shared_ptr<Application>> m_apps;
	std::string m_hostDescription;
	std::string m_defaultExecUser;
	std::string m_defaultWorkDir;
	int m_scheduleInterval;
	std::shared_ptr<JsonRest> m_rest;
	std::shared_ptr<JsonConsul> m_consul;

	std::string m_logLevel;

	mutable std::recursive_mutex m_appMutex;
	mutable std::recursive_mutex m_hotupdateMutex;
	std::string m_jsonFilePath;

	std::shared_ptr<Label> m_label;

	static std::shared_ptr<Configuration> m_instance;
};
