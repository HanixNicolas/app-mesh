#include <atomic>
#include <chrono>
#include <thread>
#include <termios.h>
#include <unistd.h>

#include <ace/Signal.h>
#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <cpprest/filestream.h>
#include <cpprest/json.h>

#include "../common/DateTime.h"
#include "../common/DurationParse.h"
#include "../common/Utility.h"
#include "../common/jwt-cpp/jwt.h"
#include "../common/os/chown.hpp"
#include "../common/os/linux.hpp"
#include "ArgumentParser.h"

#define OPTION_URL \
	("url,b", po::value<std::string>()->default_value(DEFAULT_SERVER_URL), "server URL")

#define COMMON_OPTIONS \
	OPTION_URL         \
	("user,u", po::value<std::string>(), "Specifies the name of the user to connect to App Mesh for this command.") \
	("password,x", po::value<std::string>(), "Specifies the user password to connect to App Mesh for this command.")

#define GET_USER_NAME_PASS                                                                \
	if (m_commandLineVariables.count("password") && m_commandLineVariables.count("user")) \
	{                                                                                     \
		m_username = m_commandLineVariables["user"].as<std::string>();                    \
		m_userpwd = m_commandLineVariables["password"].as<std::string>();                 \
	}
#define HELP_ARG_CHECK_WITH_RETURN                \
	GET_USER_NAME_PASS                            \
	if (m_commandLineVariables.count("help") > 0) \
	{                                             \
		std::cout << desc << std::endl;           \
		return;                                   \
	}                                             \
	m_url = m_commandLineVariables["url"].as<std::string>();

// Each user should have its own token path
const static std::string m_tokenFilePrefix = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/._appmesh_";
static std::string m_jwtToken;
extern char **environ;

// Global variable for appc exec
static bool SIGINIT_BREAKING = false;
static std::string APPC_EXEC_APP_NAME;
static ArgumentParser *WORK_PARSE = nullptr;
// command line help width
static size_t BOOST_DESC_WIDTH = 130;

ArgumentParser::ArgumentParser(int argc, const char *argv[])
	: m_argc(argc), m_argv(argv), m_tokenTimeoutSeconds(0)
{
	WORK_PARSE = this;
	po::options_description global("Global options", BOOST_DESC_WIDTH);
	global.add_options()
	("command", po::value<std::string>(), "command to execute")
	("subargs", po::value<std::vector<std::string>>(), "arguments for command");

	po::positional_options_description pos;
	pos.add("command", 1).add("subargs", -1);

	// parse [command] and all other arguments in [subargs]
	auto parsed = po::command_line_parser(argc, argv).options(global).positional(pos).allow_unregistered().run();
	m_parsedOptions = parsed.options;
	po::store(parsed, m_commandLineVariables);
	po::notify(m_commandLineVariables);
}

ArgumentParser::~ArgumentParser()
{
	unregSignal();
	WORK_PARSE = nullptr;
}

void ArgumentParser::parse()
{
	if (m_commandLineVariables.size() == 0)
	{
		printMainHelp();
		return;
	}

	std::string cmd = m_commandLineVariables["command"].as<std::string>();
	if (cmd == "logon")
	{
		processLogon();
	}
	else if (cmd == "logoff")
	{
		processLogoff();
	}
	else if (cmd == "loginfo")
	{
		processLoginfo();
	}
	else if (cmd == "reg")
	{
		processAppAdd();
	}
	else if (cmd == "unreg")
	{
		processAppDel();
	}
	else if (cmd == "view")
	{
		processAppView();
	}
	else if (cmd == "cloud")
	{
		processCloudAppView();
	}
	else if (cmd == "nodes")
	{
		processCloudNodesView();
	}
	else if (cmd == "resource")
	{
		processResource();
	}
	else if (cmd == "enable")
	{
		processAppControl(true);
	}
	else if (cmd == "disable")
	{
		processAppControl(false);
	}
	else if (cmd == "restart")
	{
		auto tmpOpts = m_parsedOptions;
		processAppControl(false);
		m_parsedOptions = tmpOpts;
		processAppControl(true);
	}
	else if (cmd == "run")
	{
		processAppRun();
	}
	else if (cmd == "exec")
	{
		processExec();
	}
	else if (cmd == "get")
	{
		processFileDownload();
	}
	else if (cmd == "put")
	{
		processFileUpload();
	}
	else if (cmd == "label")
	{
		processTags();
	}
	else if (cmd == "log")
	{
		processLoglevel();
	}
	else if (cmd == "config")
	{
		processConfigView();
	}
	else if (cmd == "passwd")
	{
		processUserChangePwd();
	}
	else if (cmd == "lock")
	{
		processUserLock();
	}
	else if (cmd == "join")
	{
		processCloudJoinMaster();
	}
	else if (cmd == "appmgpwd")
	{
		processUserPwdEncrypt();
	}
	else
	{
		printMainHelp();
	}
}

void ArgumentParser::printMainHelp()
{
	std::cout << "Commands:" << std::endl;
	std::cout << "  logon       Log on to App Mesh for a specific time period." << std::endl;
	std::cout << "  logoff      Clear current login user information" << std::endl;
	std::cout << "  loginfo     Print current logon user" << std::endl;
	std::cout << std::endl;

	std::cout << "  view        List application[s]" << std::endl;
	std::cout << "  reg         Add a new application" << std::endl;
	std::cout << "  unreg       Remove an application" << std::endl;
	std::cout << "  enable      Enable a application" << std::endl;
	std::cout << "  disable     Disable a application" << std::endl;
	std::cout << "  restart     Restart a application" << std::endl;
	std::cout << std::endl;

	std::cout << "  join        Join to a Consul cluster" << std::endl;
	std::cout << "  cloud       List cloud application[s]" << std::endl;
	std::cout << "  nodes       List cloud nodes" << std::endl;
	std::cout << std::endl;

	std::cout << "  run         Run commands or an existing application and get output" << std::endl;
	std::cout << "  exec        Run command by appmesh and impersonate current shell context" << std::endl;
	std::cout << std::endl;

	std::cout << "  resource    Display host resources" << std::endl;
	std::cout << "  label       Manage host labels" << std::endl;
	std::cout << "  config      Manage basic configurations" << std::endl;
	std::cout << "  log         Set log level" << std::endl;
	std::cout << std::endl;

	std::cout << "  get         Download remote file to local" << std::endl;
	std::cout << "  put         Upload local file to App Mesh server" << std::endl;
	std::cout << std::endl;

	std::cout << "  passwd      Change user password" << std::endl;
	std::cout << "  lock        Lock/Unlock a user" << std::endl;
	std::cout << std::endl;

	std::cout << "Run 'appc COMMAND --help' for more information on a command." << std::endl;
	std::cout << "Use '-b $hostname','-B $port' to run remote command." << std::endl;

	std::cout << std::endl;
	std::cout << "Usage:  appc [COMMAND] [ARG...] [flags]" << std::endl;
}

void ArgumentParser::processLogon()
{
	po::options_description desc("Log on to App Mesh:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("timeout,t", po::value<std::string>()->default_value(std::to_string(DEFAULT_TOKEN_EXPIRE_SECONDS)), "Specifies the command session duration in 'seconds' or 'ISO 8601 durations'.")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	m_tokenTimeoutSeconds = DurationParse::parse(m_commandLineVariables["timeout"].as<std::string>());
	if (!m_commandLineVariables.count("user"))
	{
		std::cout << "User: ";
		std::cin >> m_username;
	}
	else
	{
		m_username = m_commandLineVariables["user"].as<std::string>();
	}

	if (!m_commandLineVariables.count("password"))
	{
		if (!m_commandLineVariables.count("user"))
		{
			std::cin.clear();
			std::cin.ignore(1024, '\n');
		}
		std::cout << "Password: ";
		char buffer[256] = {0};
		char *str = buffer;
		FILE *fp = stdin;
		inputSecurePasswd(&str, sizeof(buffer), '*', fp);
		m_userpwd = buffer;
		std::cout << std::endl;
	}

	std::string tokenFile = std::string(m_tokenFilePrefix) + web::uri(m_url).host();
	// clear token first
	if (Utility::isFileExist(tokenFile))
	{
		std::ofstream ofs(tokenFile, std::ios::trunc);
		ofs.close();
	}
	// get token from REST
	m_jwtToken = getAuthenToken();

	// write token to disk
	if (m_jwtToken.length())
	{
		std::ofstream ofs(tokenFile, std::ios::trunc);
		if (ofs.is_open())
		{
			ofs << m_jwtToken;
			ofs.close();
			std::cout << "User <" << m_username << "> logon to " << m_url << " success." << std::endl;
		}
		else
		{
			std::cout << "Failed to open token file " << tokenFile << std::endl;
		}
	}
}

void ArgumentParser::processLogoff()
{
	po::options_description desc("Logoff to App Mesh:", BOOST_DESC_WIDTH);
	desc.add_options()
		OPTION_URL
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	std::string tokenFile = std::string(m_tokenFilePrefix) + m_url;
	if (Utility::isFileExist(tokenFile))
	{
		std::ofstream ofs(tokenFile, std::ios::trunc);
		ofs.close();
	}
	std::cout << "User logoff from " << m_url << " success." << std::endl;
}

void ArgumentParser::processLoginfo()
{
	po::options_description desc("Print logon user:", BOOST_DESC_WIDTH);
	desc.add_options()
		OPTION_URL
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	auto token = getAuthenToken();
	if (token.length())
	{
		auto decoded_token = jwt::decode(token);
		if (decoded_token.has_payload_claim(HTTP_HEADER_JWT_name))
		{
			// get user info
			auto userName = decoded_token.get_payload_claim(HTTP_HEADER_JWT_name).as_string();
			std::cout << userName << std::endl;
		}
	}
}

// appName is null means this is a normal application (not a shell application)
void ArgumentParser::processAppAdd()
{
	po::options_description desc("Register a new application", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("name,n", po::value<std::string>(), "application name")
		("metadata,g", po::value<std::string>(), "metadata string/JSON (input for application, pass to process stdin), '@' allowed to read from file")
		("perm", po::value<int>(), "application user permission, value is 2 bit integer: [group & other], each bit can be deny:1, read:2, write: 3.")
		("cmd,c", po::value<std::string>(), "full command line with arguments")
		("shell_mode,S", "use shell mode, cmd can be more commands")
		("health_check,l", po::value<std::string>(), "health check script command (e.g., sh -x 'curl host:port/health', return 0 is health)")
		("docker_image,d", po::value<std::string>(), "docker image which used to run command line (for docker container application)")
		("workdir,w", po::value<std::string>(), "working directory")
		("status,s", po::value<bool>()->default_value(true), "initial application status (true is enable, false is disabled)")
		("start_time,t", po::value<std::string>(), "start date time for app (ISO8601 time format, e.g., '2020-10-11T09:22:05')")
		("end_time,E", po::value<std::string>(), "end date time for app (ISO8601 time format, e.g., '2020-10-11T10:22:05')")
		("daily_start,j", po::value<std::string>(), "daily start time (e.g., '09:00:00')")
		("daily_end,y", po::value<std::string>(), "daily end time (e.g., '20:00:00')")
		("memory,m", po::value<int>(), "memory limit in MByte")
		("pid,p", po::value<int>(), "process id used to attach")
		("stdout_cache_num,O", po::value<int>(), "stdout file cache number")
		("virtual_memory,v", po::value<int>(), "virtual memory limit in MByte")
		("cpu_shares,r", po::value<int>(), "CPU shares (relative weight)")
		("env,e", po::value<std::vector<std::string>>(), "environment variables (e.g., -e env1=value1 -e env2=value2, APP_DOCKER_OPTS is used to input docker run parameters)")
		("sec_env", po::value<std::vector<std::string>>(), "security environment variables, encrypt in server side with application owner's cipher")
		("interval,i", po::value<std::string>(), "start interval seconds for short running app, support ISO 8601 durations and cron expression (e.g., 'P1Y2M3DT4H5M6S' 'P5W' '* */5 * * * *')")
		("cron", "indicate interval parameter use cron expression")
		("retention,q", po::value<std::string>()->default_value(std::to_string(DEFAULT_RUN_APP_RETENTION_DURATION)), "retention duration after run finished (default 10s), app will be cleaned after the retention period, support ISO 8601 durations (e.g., 'P1Y2M3DT4H5M6S' 'P5W').")
		("exit", po::value<std::string>()->default_value(JSON_KEY_APP_behavior_standby), "exit behavior [restart,standby,keepalive,remove]")
		("timezone,z", po::value<std::string>(), "posix timezone for the application, reflect [start_time|daily_start|daily_end] (e.g., 'GMT+08:00' is Beijing Time)")
		("force,f", "force without confirm")
		("stdin", "accept json from stdin")
		("help,h", "Prints command usage to stdout and exits");

	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;
	if (m_commandLineVariables.count("stdin") == 0 && (m_commandLineVariables.count("name") == 0 ||
		(m_commandLineVariables.count("docker_image") == 0 && m_commandLineVariables.count("cmd") == 0)))
	{
		std::cout << desc << std::endl;
		return;
	}

	if (m_commandLineVariables.count("interval") > 0 && m_commandLineVariables.count("extra_time") > 0)
	{
		if (DurationParse::parse(m_commandLineVariables["interval"].as<std::string>()) <=
			DurationParse::parse(m_commandLineVariables["extra_time"].as<std::string>()))
		{
			std::cout << "The extra_time seconds must less than interval." << std::endl;
			return;
		}
	}
	web::json::value jsonObj;
	if (m_commandLineVariables.count("stdin"))
	{
		jsonObj = web::json::value::parse(Utility::readStdin2End());
	}
	
	std::string appName;
	if (m_commandLineVariables.count("stdin"))
	{
		if (!HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_name))
		{
			std::cout << "Can not find application name" << std::endl;
			return;
		}
		appName = GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_name);
	}
	else
	{
		if (m_commandLineVariables.count("name") == 0)
		{
			std::cout << "Can not find application name" << std::endl;
			return;
		}
		appName = m_commandLineVariables["name"].as<std::string>();
	}

	if (isAppExist(appName))
	{
		if (m_commandLineVariables.count("force") == 0)
		{
			std::cout << "Application already exist, are you sure you want to update the application <" << appName << ">?" << std::endl;
			if (m_commandLineVariables.count("stdin") || !confirmInput("[y/n]:"))
			{
				return;
			}
		}
	}

	if (m_commandLineVariables.count("exit"))
	{
		auto exit = m_commandLineVariables["exit"].as<std::string>();
		if (exit == JSON_KEY_APP_behavior_standby ||
			exit == JSON_KEY_APP_behavior_restart ||
			exit == JSON_KEY_APP_behavior_keepalive ||
			exit == JSON_KEY_APP_behavior_remove)
		{
			web::json::value jsonBehavior;
			jsonBehavior[JSON_KEY_APP_behavior_exit] = web::json::value::string(exit);
			jsonObj[JSON_KEY_APP_behavior] = jsonBehavior;
		}
		else
		{
			throw std::invalid_argument(Utility::stringFormat("invalid behavior <%s> for <exit> event", exit.c_str()));
		}
	}
	if (m_commandLineVariables.count("name"))
		jsonObj[JSON_KEY_APP_name] = web::json::value::string(m_commandLineVariables["name"].as<std::string>());
	if (m_commandLineVariables.count("cmd"))
		jsonObj[JSON_KEY_APP_command] = web::json::value::string(m_commandLineVariables["cmd"].as<std::string>());
	if (m_commandLineVariables.count("shell_mode"))
		jsonObj[JSON_KEY_APP_shell_mode] = web::json::value::boolean(true);
	if (m_commandLineVariables.count("health_check"))
		jsonObj[JSON_KEY_APP_health_check_cmd] = web::json::value::string(m_commandLineVariables["health_check"].as<std::string>());
	if (m_commandLineVariables.count("perm"))
		jsonObj[JSON_KEY_APP_owner_permission] = web::json::value::number(m_commandLineVariables["perm"].as<int>());
	if (m_commandLineVariables.count("workdir"))
		jsonObj[JSON_KEY_APP_working_dir] = web::json::value::string(m_commandLineVariables["workdir"].as<std::string>());
	if (m_commandLineVariables.count("status"))
		jsonObj[JSON_KEY_APP_status] = web::json::value::number(m_commandLineVariables["status"].as<bool>() ? 1 : 0);
	if (m_commandLineVariables.count(JSON_KEY_APP_metadata))
	{
		auto metaData = m_commandLineVariables[JSON_KEY_APP_metadata].as<std::string>();
		if (metaData.length())
		{
			if (metaData[0] == '@')
			{
				auto fileName = metaData.substr(1);
				if (!Utility::isFileExist(fileName))
				{
					throw std::invalid_argument(Utility::stringFormat("input file %s does not exist", fileName.c_str()));
				}
				metaData = Utility::readFile(fileName);
			}
			try
			{
				// try to load as JSON first
				jsonObj[JSON_KEY_APP_metadata] = web::json::value::parse(metaData);
			}
			catch (...)
			{
				// use text field in case of not JSON format
				jsonObj[JSON_KEY_APP_metadata] = web::json::value::string(metaData);
			}
		}
	}
	if (m_commandLineVariables.count("docker_image"))
		jsonObj[JSON_KEY_APP_docker_image] = web::json::value::string(m_commandLineVariables["docker_image"].as<std::string>());
	if (m_commandLineVariables.count("timezone"))
		jsonObj[JSON_KEY_APP_posix_timezone] = web::json::value::string(m_commandLineVariables["timezone"].as<std::string>());
	if (m_commandLineVariables.count("start_time"))
		jsonObj[JSON_KEY_SHORT_APP_start_time] = web::json::value::string(m_commandLineVariables["start_time"].as<std::string>());
	if (m_commandLineVariables.count("end_time"))
		jsonObj[JSON_KEY_SHORT_APP_end_time] = web::json::value::string(m_commandLineVariables["end_time"].as<std::string>());
	if (m_commandLineVariables.count("interval"))
	{
		jsonObj[JSON_KEY_SHORT_APP_start_interval_seconds] = web::json::value::string(m_commandLineVariables["interval"].as<std::string>());
		if (m_commandLineVariables.count("cron"))
			jsonObj[JSON_KEY_SHORT_APP_cron_interval] = web::json::value::boolean(true);
	}
	if (m_commandLineVariables.count(JSON_KEY_APP_retention))
		jsonObj[JSON_KEY_APP_retention] = web::json::value::string(m_commandLineVariables["retention"].as<std::string>());
	if (m_commandLineVariables.count("stdout_cache_num"))
		jsonObj[JSON_KEY_APP_stdout_cache_num] = web::json::value::number(m_commandLineVariables["stdout_cache_num"].as<int>());
	if (m_commandLineVariables.count("daily_start") && m_commandLineVariables.count("daily_end"))
	{
		web::json::value objDailyLimitation = web::json::value::object();
		objDailyLimitation[JSON_KEY_DAILY_LIMITATION_daily_start] = web::json::value::string(m_commandLineVariables["daily_start"].as<std::string>());
		objDailyLimitation[JSON_KEY_DAILY_LIMITATION_daily_end] = web::json::value::string(m_commandLineVariables["daily_end"].as<std::string>());
		jsonObj[JSON_KEY_APP_daily_limitation] = objDailyLimitation;
	}

	if (m_commandLineVariables.count("memory") || m_commandLineVariables.count("virtual_memory") ||
		m_commandLineVariables.count("cpu_shares"))
	{
		web::json::value objResourceLimitation = web::json::value::object();
		if (m_commandLineVariables.count("memory"))
			objResourceLimitation[JSON_KEY_RESOURCE_LIMITATION_memory_mb] = web::json::value::number(m_commandLineVariables["memory"].as<int>());
		if (m_commandLineVariables.count("virtual_memory"))
			objResourceLimitation[JSON_KEY_RESOURCE_LIMITATION_memory_virt_mb] = web::json::value::number(m_commandLineVariables["virtual_memory"].as<int>());
		if (m_commandLineVariables.count("cpu_shares"))
			objResourceLimitation[JSON_KEY_RESOURCE_LIMITATION_cpu_shares] = web::json::value::number(m_commandLineVariables["cpu_shares"].as<int>());
		jsonObj[JSON_KEY_APP_resource_limit] = objResourceLimitation;
	}

	if (m_commandLineVariables.count(JSON_KEY_APP_env))
	{
		std::vector<std::string> envs = m_commandLineVariables[JSON_KEY_APP_env].as<std::vector<std::string>>();
		if (envs.size())
		{
			web::json::value objEnvs = web::json::value::object();
			for (auto env : envs)
			{
				auto find = env.find_first_of('=');
				if (find != std::string::npos)
				{
					auto key = Utility::stdStringTrim(env.substr(0, find));
					auto val = Utility::stdStringTrim(env.substr(find + 1));
					objEnvs[key] = web::json::value::string(val);
				}
			}
			jsonObj[JSON_KEY_APP_env] = objEnvs;
		}
	}
	if (m_commandLineVariables.count(JSON_KEY_APP_sec_env))
	{
		std::vector<std::string> envs = m_commandLineVariables[JSON_KEY_APP_sec_env].as<std::vector<std::string>>();
		if (envs.size())
		{
			web::json::value objEnvs = web::json::value::object();
			for (auto env : envs)
			{
				auto find = env.find_first_of('=');
				if (find != std::string::npos)
				{
					auto key = Utility::stdStringTrim(env.substr(0, find));
					auto val = Utility::stdStringTrim(env.substr(find + 1));
					objEnvs[key] = web::json::value::string(val);
				}
			}
			jsonObj[JSON_KEY_APP_sec_env] = objEnvs;
		}
	}
	if (m_commandLineVariables.count("pid"))
		jsonObj[JSON_KEY_APP_pid] = web::json::value::number(m_commandLineVariables["pid"].as<int>());
	std::string restPath = std::string("/appmesh/app/") + appName;
	auto resp = requestHttp(true, methods::PUT, restPath, jsonObj);
	std::cout << Utility::prettyJson(resp.extract_json(true).get().serialize()) << std::endl;
}

void ArgumentParser::processAppDel()
{
	po::options_description desc("Unregister and remove an application", BOOST_DESC_WIDTH);
	desc.add_options()
		("help,h", "Prints command usage to stdout and exits")
		COMMON_OPTIONS
		("name,n", po::value<std::vector<std::string>>(), "application name[s]")
		("force,f", "force without confirm.");

	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (m_commandLineVariables.count("name") == 0)
	{
		std::cout << desc << std::endl;
		return;
	}

	auto appNames = m_commandLineVariables["name"].as<std::vector<std::string>>();
	for (auto appName : appNames)
	{
		if (isAppExist(appName))
		{
			if (m_commandLineVariables.count("force") == 0)
			{
				std::string msg = std::string("Are you sure you want to remove the application <") + appName + "> ? [y/n]";
				if (!confirmInput(msg.c_str()))
				{
					return;
				}
			}
			std::string restPath = std::string("/appmesh/app/") + appName;
			auto response = requestHttp(true, methods::DEL, restPath);
			std::cout << parseOutputMessage(response) << std::endl;
		}
		else
		{
			throw std::invalid_argument(Utility::stringFormat("No such application <%s>", appName.c_str()));
		}
	}
}

void ArgumentParser::processAppView()
{
	po::options_description desc("List application[s]", BOOST_DESC_WIDTH);
	desc.add_options()
		("help,h", "Prints command usage to stdout and exits")
		COMMON_OPTIONS
		("name,n", po::value<std::string>(), "application name.")
		("long,l", "display the complete information without reduce")
		("output,o", "view the application output")
		("stdout_index,O", po::value<int>(), "application output index")
		("tail,t", "continue view the application output");

	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	bool reduce = !(m_commandLineVariables.count("long"));
	if (m_commandLineVariables.count("name") > 0)
	{
		if (!m_commandLineVariables.count("output"))
		{
			// view app info
			std::string restPath = std::string("/appmesh/app/") + m_commandLineVariables["name"].as<std::string>();
			auto resp = requestHttp(true, methods::GET, restPath);
			std::cout << Utility::prettyJson(resp.extract_json(true).get().serialize()) << std::endl;
		}
		else
		{
			// view app output
			int index = 0;
			std::string restPath = std::string("/appmesh/app/") + m_commandLineVariables["name"].as<std::string>() + "/output";
			if (m_commandLineVariables.count("stdout_index"))
			{
				index = m_commandLineVariables["stdout_index"].as<int>();
			}
			long outputPosition = 0;
			bool exit = false;
			std::map<std::string, std::string> query;
			query[HTTP_QUERY_KEY_stdout_index] = std::to_string(index);
			while (!exit)
			{
				query[HTTP_QUERY_KEY_stdout_position] = std::to_string(outputPosition);
				auto response = requestHttp(true, methods::GET, restPath, query);
				std::cout << response.extract_utf8string(true).get();
				if (response.headers().has(HTTP_HEADER_KEY_output_pos))
				{
					outputPosition = std::atol(response.headers().find(HTTP_HEADER_KEY_output_pos)->second.c_str());
				}
				// check continues failure
				exit = response.headers().has(HTTP_HEADER_KEY_exit_code);
				if (!exit)
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
	}
	else
	{
		std::string restPath = "/appmesh/applications";
		auto response = requestHttp(true, methods::GET, restPath);
		printApps(response.extract_json(true).get(), reduce);
	}
}

void ArgumentParser::processCloudAppView()
{
	po::options_description desc("List cloud applications usage:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	std::string restPath = "/appmesh/cloud/applications";
	auto resp = requestHttp(true, methods::GET, restPath);
	std::cout << Utility::prettyJson(resp.extract_json(true).get().serialize()) << std::endl;
}

void ArgumentParser::processCloudNodesView()
{
	po::options_description desc("List cluster nodes usage:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	std::string restPath = "/appmesh/cloud/nodes";
	auto resp = requestHttp(true, methods::GET, restPath);
	std::cout << Utility::prettyJson(resp.extract_json(true).get().serialize()) << std::endl;
}

void ArgumentParser::processResource()
{
	po::options_description desc("View host resource usage:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	std::string restPath = "/appmesh/resources";
	auto resp = requestHttp(true, methods::GET, restPath);
	std::cout << Utility::prettyJson(resp.extract_json(true).get().serialize()) << std::endl;
}

void ArgumentParser::processAppControl(bool start)
{
	po::options_description desc("Start application:", BOOST_DESC_WIDTH);
	desc.add_options()
		("help,h", "Prints command usage to stdout and exits")
		COMMON_OPTIONS
		("all,a", "apply for all applications")
		("name,n", po::value<std::vector<std::string>>(), "application name[s] to enable or disable.");

	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;
	if (m_commandLineVariables.empty() || (!m_commandLineVariables.count("all") && !m_commandLineVariables.count("name")))
	{
		std::cout << desc << std::endl;
		return;
	}
	std::vector<std::string> appList;
	bool all = m_commandLineVariables.count("all");
	if (all)
	{
		auto appMap = this->getAppList();
		std::for_each(appMap.begin(), appMap.end(), [&appList, &start](const std::pair<std::string, bool> &pair) {
			if (start != pair.second)
			{
				appList.push_back(pair.first);
			}
		});
	}
	else
	{
		auto appNames = m_commandLineVariables["name"].as<std::vector<std::string>>();
		for (auto appName : appNames)
		{
			if (!isAppExist(appName))
			{
				throw std::invalid_argument(Utility::stringFormat("No such application <%s>", appName.c_str()));
			}
			appList.push_back(appName);
		}
	}
	for (auto app : appList)
	{
		std::string restPath = std::string("/appmesh/app/") + app + +"/" + (start ? HTTP_QUERY_KEY_action_start : HTTP_QUERY_KEY_action_stop);
		auto response = requestHttp(true, methods::POST, restPath);
		std::cout << parseOutputMessage(response) << std::endl;
	}
	if (appList.size() == 0)
	{
		std::cout << "No application processed." << std::endl;
	}
}

void ArgumentParser::processAppRun()
{
	po::options_description desc("Run commands or application:", BOOST_DESC_WIDTH);
	desc.add_options()
		("help,h", "Prints command usage to stdout and exits")
		COMMON_OPTIONS
		("cmd,c", po::value<std::string>(), "full command line with arguments (run application do not need specify command line)")
		("name,n", po::value<std::string>(), "existing application name to run or specify a application name for run, empty will generate a random name in server")
		("metadata,g", po::value<std::string>(), "application metadata string/JSON (input for application, pass to application process stdin)")
		("workdir,w", po::value<std::string>(), "working directory (default '/opt/appmesh/work', used for run commands)")
		("env,e", po::value<std::vector<std::string>>(), "environment variables (e.g., -e env1=value1 -e env2=value2)")
		("timeout,t", po::value<std::string>()->default_value(std::to_string(DEFAULT_RUN_APP_TIMEOUT_SECONDS)), "max time[seconds] for the shell command run. Greater than 0 means output can be print repeatedly, less than 0 means output will be print until process exited, support ISO 8601 durations (e.g., 'P1Y2M3DT4H5M6S' 'P5W').")
		("retention,r", po::value<std::string>()->default_value(std::to_string(DEFAULT_RUN_APP_RETENTION_DURATION)), "retention time[seconds] for app cleanup after finished (default 10s), support ISO 8601 durations (e.g., 'P1Y2M3DT4H5M6S' 'P5W').");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (m_commandLineVariables.count("help") || (m_commandLineVariables.count("name") == 0 && m_commandLineVariables.count("cmd") == 0))
	{
		std::cout << desc << std::endl;
		return;
	}

	std::map<std::string, std::string> query;
	int timeout = DurationParse::parse(m_commandLineVariables["timeout"].as<std::string>());
	if (m_commandLineVariables.count("timeout"))
		query[HTTP_QUERY_KEY_timeout] = std::to_string(timeout);

	web::json::value jsonObj;
	web::json::value jsonBehavior;
	jsonBehavior[JSON_KEY_APP_behavior_exit] = web::json::value::string(JSON_KEY_APP_behavior_remove);
	jsonObj[JSON_KEY_APP_behavior] = jsonBehavior;
	jsonObj[JSON_KEY_APP_shell_mode] = web::json::value::boolean(true);
	if (m_commandLineVariables.count("cmd"))
	{
		jsonObj[JSON_KEY_APP_command] = web::json::value::string(m_commandLineVariables["cmd"].as<std::string>());
	}
	if (m_commandLineVariables.count(JSON_KEY_APP_retention))
	{
		jsonObj[JSON_KEY_APP_retention] = web::json::value::string(m_commandLineVariables["retention"].as<std::string>());
	}
	if (m_commandLineVariables.count(JSON_KEY_APP_name))
	{
		jsonObj[JSON_KEY_APP_name] = web::json::value::string(m_commandLineVariables["name"].as<std::string>());
	}
	if (m_commandLineVariables.count(JSON_KEY_APP_metadata))
	{
		auto metaData = m_commandLineVariables[JSON_KEY_APP_metadata].as<std::string>();
		if (metaData.length())
		{
			if (metaData[0] == '@')
			{
				auto fileName = metaData.substr(1);
				if (!Utility::isFileExist(fileName))
				{
					throw std::invalid_argument(Utility::stringFormat("input file %s does not exist", fileName.c_str()));
				}
				metaData = Utility::readFile(fileName);
			}
			try
			{
				// try to load as JSON first
				jsonObj[JSON_KEY_APP_metadata] = web::json::value::parse(metaData);
			}
			catch(...)
			{
				// use text field in case of not JSON format
				jsonObj[JSON_KEY_APP_metadata] = web::json::value::string(metaData);
			}
		}
	}
	if (m_commandLineVariables.count("workdir"))
		jsonObj[JSON_KEY_APP_working_dir] = web::json::value::string(m_commandLineVariables["workdir"].as<std::string>());
	if (m_commandLineVariables.count("env"))
	{
		std::vector<std::string> envs = m_commandLineVariables["env"].as<std::vector<std::string>>();
		if (envs.size())
		{
			web::json::value objEnvs = web::json::value::object();
			for (auto env : envs)
			{
				auto find = env.find_first_of('=');
				if (find != std::string::npos)
				{
					auto key = Utility::stdStringTrim(env.substr(0, find));
					auto val = Utility::stdStringTrim(env.substr(find + 1));
					objEnvs[key] = web::json::value::string(val);
				}
			}
			jsonObj[JSON_KEY_APP_env] = objEnvs;
		}
	}

	if (timeout < 0)
	{
		// Use syncrun directly
		// /app/syncrun?timeout=5
		std::string restPath = "/appmesh/app/syncrun";
		auto response = requestHttp(true, methods::POST, restPath, query, &jsonObj);
		std::cout << response.extract_utf8string(true).get();
	}
	else
	{
		// Use run and output
		// /app/run?timeout=5
		if (m_commandLineVariables.count(HTTP_QUERY_KEY_timeout))
			query[HTTP_QUERY_KEY_timeout] = m_commandLineVariables[HTTP_QUERY_KEY_timeout].as<std::string>();
		std::string restPath = "/appmesh/app/run";
		auto response = requestHttp(true, methods::POST, restPath, query, &jsonObj);
		auto result = response.extract_json(true).get();
		auto appName = result[JSON_KEY_APP_name].as_string();
		auto process_uuid = result[HTTP_QUERY_KEY_process_uuid].as_string();
		std::atomic<int> continueFailure(0);
		long outputPosition = 0;
		while (process_uuid.length() && continueFailure < 3)
		{
			// /app/testapp/output?process_uuid=ABDJDD-DJKSJDKF
			restPath = std::string("/appmesh/app/").append(appName).append("/output");
			query.clear();
			query[HTTP_QUERY_KEY_process_uuid] = process_uuid;
			query[HTTP_QUERY_KEY_stdout_position] = std::to_string(outputPosition);
			response = requestHttp(false, methods::GET, restPath, query);
			std::cout << response.extract_utf8string(true).get();
			if (response.headers().has(HTTP_HEADER_KEY_output_pos))
			{
				outputPosition = std::atol(response.headers().find(HTTP_HEADER_KEY_output_pos)->second.c_str());
			}

			// check continues failure
			if (response.status_code() != http::status_codes::OK && !response.headers().has(HTTP_HEADER_KEY_exit_code))
			{
				continueFailure++;
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}
			if (response.headers().has(HTTP_HEADER_KEY_exit_code) || response.status_code() != http::status_codes::OK)
			{
				break;
			}
			continueFailure = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		// delete
		restPath = std::string("/appmesh/app/").append(appName);
		response = requestHttp(false, methods::DEL, restPath);
		if (response.status_code() != status_codes::OK)
		{
			std::cerr << response.extract_utf8string(true).get() << std::endl;
		}
	}
}

void SIGINT_Handler(int signo)
{
	// make sure we only process SIGINT here
	// SIGINT 	ctrl - c
	assert(signo == SIGINT);
	if (SIGINIT_BREAKING)
	{
		//std::cout << "You pressed SIGINT(Ctrl+C) twice, session will exit." << std::endl;
		auto restPath = std::string("/appmesh/app/").append(APPC_EXEC_APP_NAME);
		WORK_PARSE->requestHttp(false, methods::DEL, restPath);
		// if ctrl+c typed twice, just exit current
		ACE_OS::_exit(SIGINT);
	}
	else
	{
		//std::cout << "You pressed SIGINT(Ctrl+C)" << std::endl;
		SIGINIT_BREAKING = true;
		auto restPath = std::string("/appmesh/app/").append(APPC_EXEC_APP_NAME).append("/disable");
		WORK_PARSE->requestHttp(false, methods::POST, restPath);
	}
}

std::string ArgumentParser::parseOutputMessage(http_response &resp)
{
	std::string result;
	try
	{
		auto respJson = resp.extract_json().get();
		if (HAS_JSON_FIELD(respJson, REST_TEXT_MESSAGE_JSON_KEY))
		{
			result = respJson.at(REST_TEXT_MESSAGE_JSON_KEY).as_string();
		}
		else
		{
			result = respJson.serialize();
		}
	}
	catch(...)
	{
		result = resp.extract_utf8string().get();
	}
	return result;
}

void ArgumentParser::regSignal()
{
	m_sigAction = std::make_shared<ACE_Sig_Action>();
	m_sigAction->handler(SIGINT_Handler);
	m_sigAction->register_action(SIGINT);
}

void ArgumentParser::unregSignal()
{
	if (m_sigAction)
		m_sigAction = nullptr;
}

void ArgumentParser::processExec()
{
	m_url = DEFAULT_SERVER_URL;
	// Get current session id (bash pid)
	auto bashId = getppid();
	// Get appmesh user
	auto appmeshUser = getAuthenUser();
	// Get current user name
	auto osUser = getOsUser();
	// Unique session id as appname
	APPC_EXEC_APP_NAME = appmeshUser + "_" + osUser + "_" + std::to_string(bashId);

	// Get current command line, use raw argv here
	std::string initialCmd;
	for (int i = 2; i < m_argc; i++)
	{
		initialCmd.append(m_argv[i]).append(" ");
	}

	// Get current ENV
	web::json::value objEnvs = web::json::value::object();
	for (char **var = environ; *var != nullptr; var++)
	{
		std::string e = *var;
		auto vec = Utility::splitString(e, "=");
		if (vec.size() > 0)
		{
			objEnvs[vec[0]] = web::json::value::string(vec.size() > 1 ? vec[1] : std::string());
		}
	}

	char buff[MAX_COMMAND_LINE_LENGTH] = {0};
	web::json::value jsonObj;
	jsonObj[JSON_KEY_APP_name] = web::json::value::string(APPC_EXEC_APP_NAME);
	jsonObj[JSON_KEY_APP_shell_mode] = web::json::value::boolean(true);
	jsonObj[JSON_KEY_APP_command] = web::json::value::string(initialCmd);
	jsonObj[JSON_KEY_APP_env] = objEnvs;
	jsonObj[JSON_KEY_APP_working_dir] = web::json::value::string(getcwd(buff, sizeof(buff)));
	web::json::value behavior;
	behavior[JSON_KEY_APP_behavior_exit] = web::json::value::string(JSON_KEY_APP_behavior_remove);
	jsonObj[JSON_KEY_APP_behavior] = behavior;

	std::string process_uuid;
	long outputPosition = 0;
	bool currentRunFinished = true; // one submitted run finished
	bool runOnce = false;			// if appc exec specify one cmd, then just run once
	SIGINIT_BREAKING = false;		// if ctrl + c is triggered, stop run and start read input from stdin
	// clean first
	requestHttp(false, methods::DEL, std::string("/appmesh/app/").append(APPC_EXEC_APP_NAME));
	if (initialCmd.length())
	{
		runOnce = true;
		std::map<std::string, std::string> query = {{HTTP_QUERY_KEY_timeout, std::to_string(-1)}}; // disable timeout
		std::string restPath = "/appmesh/app/run";
		auto response = requestHttp(false, methods::POST, restPath, query, &jsonObj);
		if (response.status_code() == http::status_codes::OK)
		{
			auto result = response.extract_json(true).get();
			process_uuid = result[HTTP_QUERY_KEY_process_uuid].as_string();
			currentRunFinished = false;
		}
		else
		{
			std::cout << parseOutputMessage(response) << std::endl;
		}
	}
	else
	{
		// only capture SIGINT in continues mode
		this->regSignal();
		runOnce = false;
	}

	while (true)
	{
		// no need read stdin when run for once
		if (!runOnce && (SIGINIT_BREAKING || currentRunFinished))
		{
			SIGINIT_BREAKING = false;
			std::string input;
			std::cout << "appmesh # ";
			while (std::getline(std::cin, input) && input.length() > 0)
			{
				requestHttp(false, methods::DEL, std::string("/appmesh/app/").append(APPC_EXEC_APP_NAME));
				if (input == "exit")
				{
					ACE_OS::_exit(0);
				}
				process_uuid.clear();
				outputPosition = 0;
				jsonObj[JSON_KEY_APP_command] = web::json::value::string(input);
				std::map<std::string, std::string> query = {{HTTP_QUERY_KEY_timeout, std::to_string(-1)}}; // disable timeout
				std::string restPath = "/appmesh/app/run";
				auto response = requestHttp(false, methods::POST, restPath, query, &jsonObj);
				if (response.status_code() == http::status_codes::OK)
				{
					auto result = response.extract_json(true).get();
					process_uuid = result[HTTP_QUERY_KEY_process_uuid].as_string();
					currentRunFinished = false;
				}
				else
				{
					std::cout << parseOutputMessage(response) << std::endl;
					currentRunFinished = true;
					process_uuid.clear();
				}
				// always exit loop when get one input
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
		// Process Read
		if (!process_uuid.empty())
		{
			std::map<std::string, std::string> query = {{HTTP_QUERY_KEY_process_uuid, process_uuid}, {HTTP_QUERY_KEY_stdout_position, std::to_string(outputPosition)}};
			auto restPath = Utility::stringFormat("/appmesh/app/%s/output", APPC_EXEC_APP_NAME.c_str());
			auto response = requestHttp(false, methods::GET, restPath, query);
			std::cout << response.extract_utf8string(true).get();
			if (response.headers().has(HTTP_HEADER_KEY_output_pos))
			{
				outputPosition = std::atol(response.headers().find(HTTP_HEADER_KEY_output_pos)->second.c_str());
			}
			if (response.headers().has(HTTP_HEADER_KEY_exit_code) || response.status_code() != http::status_codes::OK)
			{
				currentRunFinished = true;
				process_uuid.clear();
				if (runOnce)
				{
					break;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}
	// clean
	requestHttp(false, methods::DEL, std::string("/appmesh/app/").append(APPC_EXEC_APP_NAME));
}

void ArgumentParser::processFileDownload()
{
	po::options_description desc("Download file:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("remote,r", po::value<std::string>(), "remote file path to download")
		("local,l", po::value<std::string>(), "local file path to save")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (m_commandLineVariables.count("remote") == 0 || m_commandLineVariables.count("local") == 0)
	{
		std::cout << desc << std::endl;
		return;
	}

	std::string restPath = "/appmesh/file/download";
	auto file = m_commandLineVariables["remote"].as<std::string>();
	auto local = m_commandLineVariables["local"].as<std::string>();
	std::map<std::string, std::string> query, headers;
	headers[HTTP_HEADER_KEY_file_path] = file;
	auto response = requestHttp(true, methods::GET, restPath, query, nullptr, &headers);

	auto stream = concurrency::streams::fstream::open_ostream(local, std::ios::out | std::ios::binary | std::ios::trunc).get();
	response.body().read_to_end(stream.streambuf()).wait();

	std::cout << "Download file <" << local << "> size <" << Utility::humanReadableSize(stream.streambuf().size()) << ">" << std::endl;
	stream.close();

	if (response.headers().has(HTTP_HEADER_KEY_file_mode))
		os::fileChmod(local, std::stoi(response.headers().find(HTTP_HEADER_KEY_file_mode)->second));
	if (response.headers().has(HTTP_HEADER_KEY_file_user) && response.headers().has(HTTP_HEADER_KEY_file_group))
		os::chown(std::stoi(response.headers().find(HTTP_HEADER_KEY_file_user)->second),
				  std::stoi(response.headers().find(HTTP_HEADER_KEY_file_group)->second),
				  local, false);
}

void ArgumentParser::processFileUpload()
{
	po::options_description desc("Upload file:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("remote,r", po::value<std::string>(), "remote file path to save")
		("local,l", po::value<std::string>(), "local file to upload")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (m_commandLineVariables.count("remote") == 0 || m_commandLineVariables.count("local") == 0)
	{
		std::cout << desc << std::endl;
		return;
	}

	auto file = m_commandLineVariables["remote"].as<std::string>();
	auto local = m_commandLineVariables["local"].as<std::string>();

	if (!Utility::isFileExist(local))
	{
		std::cout << "local file not exist" << std::endl;
		return;
	}
	// https://msdn.microsoft.com/en-us/magazine/dn342869.aspx

	auto fileStream = concurrency::streams::file_stream<uint8_t>::open_istream(local, std::ios_base::binary).get();
	// Get the content length, which is used to set the
	// Content-Length property
	fileStream.seek(0, std::ios::end);
	auto length = static_cast<std::size_t>(fileStream.tell());
	fileStream.seek(0, std::ios::beg);
	auto fileInfo = os::fileStat(local);

	std::map<std::string, std::string> query, header;
	header[HTTP_HEADER_KEY_file_path] = file;
	header[HTTP_HEADER_KEY_file_mode] = std::to_string(std::get<0>(fileInfo));
	header[HTTP_HEADER_KEY_file_user] = std::to_string(std::get<1>(fileInfo));
	header[HTTP_HEADER_KEY_file_group] = std::to_string(std::get<2>(fileInfo));

	// Create http_client to send the request.
	http_client_config config;
	config.set_timeout(std::chrono::seconds(200));
	config.set_validate_certificates(false);
	http_client client(m_url, config);
	std::string restPath = "/appmesh/file/upload";
	auto request = createRequest(methods::POST, restPath, query, &header);
	request.set_body(fileStream, length);
	http_response response = client.request(request).get();
	fileStream.close();
	std::cout << parseOutputMessage(response) << std::endl;
}

void ArgumentParser::processTags()
{
	po::options_description desc("Manage labels:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("view,v", "list labels")
		("add,a", "add labels")
		("remove,r", "remove labels")
		("label,l", po::value<std::vector<std::string>>(), "labels (e.g., -l os=linux -t arch=arm64)")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	std::vector<std::string> inputTags;
	if (m_commandLineVariables.count("label"))
		inputTags = m_commandLineVariables["label"].as<std::vector<std::string>>();

	if (m_commandLineVariables.count("add") &&
		!m_commandLineVariables.count("remove") && !m_commandLineVariables.count("view"))
	{
		// Process add
		if (inputTags.empty())
		{
			std::cout << "No label specified" << std::endl;
			return;
		}
		for (auto str : inputTags)
		{
			std::vector<std::string> envVec = Utility::splitString(str, "=");
			if (envVec.size() == 2)
			{
				std::string restPath = std::string("/appmesh/label/").append(envVec.at(0));
				std::map<std::string, std::string> query = {{"value", envVec.at(1)}};
				requestHttp(true, methods::PUT, restPath, query, nullptr, nullptr);
			}
		}
	}
	else if (m_commandLineVariables.count("remove") &&
			 !m_commandLineVariables.count("add") && !m_commandLineVariables.count("view"))
	{
		// Process remove
		if (inputTags.empty())
		{
			std::cout << "No label specified" << std::endl;
			return;
		}
		for (auto str : inputTags)
		{
			std::vector<std::string> envVec = Utility::splitString(str, "=");
			std::string restPath = std::string("/appmesh/label/").append(envVec.at(0));
			auto resp = requestHttp(true, methods::DEL, restPath);
		}
	}
	else if (m_commandLineVariables.count("view") &&
			 !m_commandLineVariables.count("remove") && !m_commandLineVariables.count("add"))
	{
		// view
	}
	else
	{
		std::cout << desc << std::endl;
		return;
	}

	std::string restPath = "/appmesh/labels";
	http_response response = requestHttp(true, methods::GET, restPath);
	// Finally print current
	auto tags = response.extract_json().get().as_object();
	for (auto tag : tags)
	{
		std::cout << tag.first << "=" << tag.second.as_string() << std::endl;
	}
}

void ArgumentParser::processLoglevel()
{
	po::options_description desc("Set log level:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("level,l", po::value<std::string>(), "log level (e.g., DEBUG,INFO,NOTICE,WARN,ERROR)")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (m_commandLineVariables.size() == 0 || m_commandLineVariables.count("level") == 0)
	{
		std::cout << desc << std::endl;
		return;
	}

	auto level = m_commandLineVariables["level"].as<std::string>();

	web::json::value jsonObj;
	jsonObj[JSON_KEY_LogLevel] = web::json::value::string(level);
	// /app-manager/config
	auto restPath = std::string("/appmesh/config");
	auto response = requestHttp(true, methods::POST, restPath, jsonObj);
	std::cout << "Log level set to: " << response.extract_json(true).get().at(JSON_KEY_LogLevel).as_string() << std::endl;
}

void ArgumentParser::processCloudJoinMaster()
{
	po::options_description desc("Join App Mesh cluster:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("consul,c", po::value<std::string>(), "Consul url (e.g., http://localhost:8500)")
		("main,m", "Join as main node")
		("worker,w", "Join as worker node")
		("proxy,r", po::value<std::string>()->default_value(""), "appmesh_proxy_url")
		("user,u", po::value<std::string>()->default_value(""), "Basic auth user name for Consul REST")
		("pass,p", po::value<std::string>()->default_value(""), "Basic auth user password for Consul REST")
		("ttl,l", po::value<std::int16_t>()->default_value(30), "Consul session TTL seconds")
		("security,s", "Enable Consul security (security persist will use Consul storage)")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (m_commandLineVariables.size() == 0 || m_commandLineVariables.count("consul") == 0)
	{
		std::cout << desc << std::endl;
		return;
	}

	web::json::value jsonObj;
	web::json::value jsonConsul;
	jsonConsul[JSON_KEY_CONSUL_URL] = web::json::value::string(m_commandLineVariables["consul"].as<std::string>());
	jsonConsul[JSON_KEY_CONSUL_IS_MAIN] = web::json::value::boolean(m_commandLineVariables.count("main"));
	jsonConsul[JSON_KEY_CONSUL_IS_WORKER] = web::json::value::boolean(m_commandLineVariables.count("worker"));
	jsonConsul[JSON_KEY_CONSUL_APPMESH_PROXY_URL] = web::json::value::string(m_commandLineVariables["proxy"].as<std::string>());
	jsonConsul[JSON_KEY_CONSUL_SESSION_TTL] = web::json::value::number(m_commandLineVariables["ttl"].as<std::int16_t>());
	jsonConsul[JSON_KEY_CONSUL_SECURITY] = web::json::value::boolean(m_commandLineVariables.count("security"));
	jsonConsul[JSON_KEY_CONSUL_AUTH_USER] = web::json::value::string(m_commandLineVariables["user"].as<std::string>());
	jsonConsul[JSON_KEY_CONSUL_AUTH_PASS] = web::json::value::string(m_commandLineVariables["pass"].as<std::string>());
	jsonObj[JSON_KEY_CONSUL] = jsonConsul;

	// /app-manager/config
	auto restPath = std::string("/appmesh/config");
	auto response = requestHttp(true, methods::POST, restPath, jsonObj);
	std::cout << "App Mesh will join cluster with parameter: " << std::endl << Utility::prettyJson(response.extract_json(true).get().at(JSON_KEY_CONSUL).serialize()) << std::endl;
}

void ArgumentParser::processConfigView()
{
	po::options_description desc("View configurations:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("view,v", "view basic configurations with json output")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	std::string restPath = "/appmesh/config";
	http_response resp = requestHttp(true, methods::GET, restPath);
	std::cout << Utility::prettyJson(resp.extract_json(true).get().serialize()) << std::endl;
}

void ArgumentParser::processUserChangePwd()
{
	po::options_description desc("Change password:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("target,t", po::value<std::string>(), "target user to change passwd")
		("newpasswd,p", po::value<std::string>(), "new password")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (!m_commandLineVariables.count("target") || !m_commandLineVariables.count("newpasswd"))
	{
		std::cout << desc << std::endl;
		return;
	}

	auto user = m_commandLineVariables["target"].as<std::string>();
	auto passwd = m_commandLineVariables["newpasswd"].as<std::string>();

	std::string restPath = std::string("/appmesh/user/") + user + "/passwd";
	std::map<std::string, std::string> query, headers;
	headers[HTTP_HEADER_JWT_new_password] = Utility::encode64(passwd);
	http_response response = requestHttp(true, methods::POST, restPath, query, nullptr, &headers);
	std::cout << parseOutputMessage(response) << std::endl;
}

void ArgumentParser::processUserLock()
{
	po::options_description desc("Manage users:", BOOST_DESC_WIDTH);
	desc.add_options()
		COMMON_OPTIONS
		("target,t", po::value<std::string>(), "target user")
		("lock,k", po::value<bool>(), "lock or unlock user, 'true' for lock, 'false' for unlock")
		("help,h", "Prints command usage to stdout and exits");
	shiftCommandLineArgs(desc);
	HELP_ARG_CHECK_WITH_RETURN;

	if (!m_commandLineVariables.count("target") || !m_commandLineVariables.count("lock"))
	{
		std::cout << desc << std::endl;
		return;
	}

	auto user = m_commandLineVariables["target"].as<std::string>();
	auto lock = !m_commandLineVariables["lock"].as<bool>();

	std::string restPath = std::string("/appmesh/user/") + user + (lock ? "/lock" : "/unlock");
	http_response response = requestHttp(true, methods::POST, restPath);
	std::cout << parseOutputMessage(response) << std::endl;
}

void ArgumentParser::processUserPwdEncrypt()
{
	std::vector<std::string> opts = po::collect_unrecognized(m_parsedOptions, po::include_positional);
	if (opts.size())
		opts.erase(opts.begin());

	std::string str;
	if (opts.size() == 0)
	{
		std::cin >> str;
		while (str.size())
		{
			std::cout << std::hash<std::string>()(str) << std::endl;
			std::cin >> str;
		}
	}
	else
	{
		for (auto optStr : opts)
		{
			std::cout << std::hash<std::string>()(optStr) << std::endl;
		}
	}
}

bool ArgumentParser::confirmInput(const char *msg)
{
	std::cout << msg;
	std::string result;
	std::cin >> result;
	return result == "y";
}

http_response ArgumentParser::requestHttp(bool throwAble, const method &mtd, const std::string &path)
{
	std::map<std::string, std::string> query;
	return requestHttp(throwAble, mtd, path, query);
}

http_response ArgumentParser::requestHttp(bool throwAble, const method &mtd, const std::string &path, web::json::value &body)
{
	std::map<std::string, std::string> query;
	return requestHttp(throwAble, mtd, path, query, &body);
}

http_response ArgumentParser::requestHttp(bool throwAble, const method &mtd, const std::string &path, std::map<std::string, std::string> &query, web::json::value *body, std::map<std::string, std::string> *header)
{
	// Create http_client to send the request.
	web::http::client::http_client_config config;
	config.set_timeout(std::chrono::seconds(65));
	config.set_validate_certificates(false);
	web::http::client::http_client client(m_url, config);
	http_request request = createRequest(mtd, path, query, header);
	if (body != nullptr)
	{
		request.set_body(*body);
	}
	http_response response = client.request(request).get();
	if (throwAble && response.status_code() != status_codes::OK)
	{
		throw std::invalid_argument(parseOutputMessage(response));
	}
	return response;
}

http_request ArgumentParser::createRequest(const method &mtd, const std::string &path, std::map<std::string, std::string> &query, std::map<std::string, std::string> *header)
{
	// Build request URI and start the request.
	uri_builder builder(GET_STRING_T(path));
	std::for_each(query.begin(), query.end(), [&builder](const std::pair<std::string, std::string> &pair)
				  { builder.append_query(GET_STRING_T(pair.first), GET_STRING_T(pair.second)); });

	http_request request(mtd);
	if (header)
	{
		for (auto h : *header)
		{
			request.headers().add(h.first, h.second);
		}
	}
	auto jwtToken = getAuthenToken();
	request.headers().add(HTTP_HEADER_JWT_Authorization, std::string(HTTP_HEADER_JWT_BearerSpace) + jwtToken);
	request.set_request_uri(builder.to_uri());
	return request;
}

bool ArgumentParser::isAppExist(const std::string &appName)
{
	static auto apps = getAppList();
	return apps.find(appName) != apps.end();
}

std::map<std::string, bool> ArgumentParser::getAppList()
{
	std::map<std::string, bool> apps;
	std::string restPath = "/appmesh/applications";
	auto response = requestHttp(true, methods::GET, restPath);
	auto jsonValue = response.extract_json(true).get();
	auto arr = jsonValue.as_array();
	for (auto iter = arr.begin(); iter != arr.end(); iter++)
	{
		auto jsonObj = *iter;
		apps[GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_name)] = GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_status) == 1;
	}
	return apps;
}

std::string ArgumentParser::getAuthenToken()
{
	std::string token;
	// 1. try to get from REST
	if (m_username.length() && m_userpwd.length())
	{
		token = requestToken(m_username, m_userpwd);
	}
	else
	{
		// 2. try to read from token file
		token = readAuthenToken();

		// 3. try to get get default token from REST
		if (token.empty())
		{
			token = requestToken(std::string(JWT_USER_NAME), std::string(JWT_USER_KEY));
		}
	}
	return token;
}

std::string ArgumentParser::getAuthenUser()
{
	std::string token;
	// 1. try to get from REST
	if (m_username.length())
	{
		return m_username;
	}
	else
	{
		// 2. try to read from token file
		token = readAuthenToken();
		// 3. try to get get default token from REST
		if (token.empty())
		{
			token = requestToken(std::string(JWT_USER_NAME), std::string(JWT_USER_KEY));
		}
		auto decoded_token = jwt::decode(token);
		if (decoded_token.has_payload_claim(HTTP_HEADER_JWT_name))
		{
			// get user info
			auto userName = decoded_token.get_payload_claim(HTTP_HEADER_JWT_name).as_string();
			return userName;
		}
		throw std::invalid_argument("Failed to get token");
	}
}

std::string ArgumentParser::getOsUser()
{
	std::string userName;
	struct passwd *pw_ptr;
	if ((pw_ptr = getpwuid(getuid())) != NULL)
	{
		userName = pw_ptr->pw_name;
	}
	else
	{
		throw std::runtime_error("Failed to get current user name");
	}
	return userName;
}

std::string ArgumentParser::readAuthenToken()
{
	std::string jwtToken;
	auto hostName = web::uri(m_url).host();
	std::string tokenFile = std::string(m_tokenFilePrefix) + hostName;
	if (Utility::isFileExist(tokenFile) && hostName.length())
	{
		std::ifstream ifs(tokenFile);
		if (ifs.is_open())
		{
			ifs >> jwtToken;
			ifs.close();
		}
	}
	return jwtToken;
}

std::string ArgumentParser::requestToken(const std::string &user, const std::string &passwd)
{
	http_client_config config;
	config.set_validate_certificates(false);
	http_client client(m_url, config);
	http_request requestLogin(web::http::methods::POST);

	requestLogin.set_request_uri("/appmesh/login");
	requestLogin.headers().add(HTTP_HEADER_JWT_username, Utility::encode64(user));
	requestLogin.headers().add(HTTP_HEADER_JWT_password, Utility::encode64(passwd));
	if (m_tokenTimeoutSeconds)
		requestLogin.headers().add(HTTP_HEADER_JWT_expire_seconds, std::to_string(m_tokenTimeoutSeconds));
	http_response response = client.request(requestLogin).get();
	if (response.status_code() != status_codes::OK)
	{
		throw std::invalid_argument(Utility::stringFormat("Login failed: %s", parseOutputMessage(response).c_str()));
	}
	else
	{
		auto jwtContent = response.extract_json(true).get();
		return GET_JSON_STR_VALUE(jwtContent, HTTP_HEADER_JWT_access_token);
	}
}

void ArgumentParser::printApps(web::json::value json, bool reduce)
{
	boost::io::ios_all_saver guard(std::cout);
	// Title:
	std::cout << std::left;
	std::cout
		<< std::setw(3) << Utility::strToupper("id")
		<< std::setw(12) << Utility::strToupper(JSON_KEY_APP_name)
		<< std::setw(6) << Utility::strToupper(JSON_KEY_APP_owner)
		<< std::setw(9) << Utility::strToupper(JSON_KEY_APP_status)
		<< std::setw(7) << Utility::strToupper(JSON_KEY_APP_health)
		<< std::setw(8) << Utility::strToupper(JSON_KEY_APP_pid)
		<< std::setw(9) << Utility::strToupper(JSON_KEY_APP_memory)
		<< std::setw(5) << std::string("%").append(Utility::strToupper(JSON_KEY_APP_cpu))
		<< std::setw(7) << Utility::strToupper(JSON_KEY_APP_return)
		<< std::setw(7) << Utility::strToupper("age")
		<< std::setw(9) << Utility::strToupper("duration")
		<< std::setw(7) << Utility::strToupper(JSON_KEY_APP_starts)
		<< Utility::strToupper(JSON_KEY_APP_command)
		<< std::endl;

	int index = 1;
	auto jsonArr = json.as_array();
	auto reduceFunc = std::bind(&ArgumentParser::reduceStr, this, std::placeholders::_1, std::placeholders::_2);
	std::for_each(jsonArr.begin(), jsonArr.end(), [&index, &reduceFunc, reduce](web::json::value &jsonObj)
				  {
					  const char *slash = "-";
					  auto name = GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_name);
					  if (reduce)
						  name = reduceFunc(name, 12);
					  else if (name.length() >= 12)
						  name += " ";
					  std::cout << std::setw(3) << index++;
					  std::cout << std::setw(12) << name;
					  std::cout << std::setw(6) << reduceFunc(GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_owner), 6);
					  std::cout << std::setw(9) << GET_STATUS_STR(GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_status));
					  std::cout << std::setw(7) << GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_health);
					  std::cout << std::setw(8);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_pid))
							  std::cout << GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_pid);
						  else
							  std::cout << slash;
					  }
					  std::cout << std::setw(9);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_memory))
							  std::cout << Utility::humanReadableSize(GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_memory));
						  else
							  std::cout << slash;
					  }
					  std::cout << std::setw(5);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_cpu))
						  {
							  std::stringstream ss;
							  ss << (int)GET_JSON_DOUBLE_VALUE(jsonObj, JSON_KEY_APP_cpu);
							  std::cout << ss.str();
						  }
						  else
							  std::cout << slash;
					  }
					  std::cout << std::setw(7);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_return))
							  std::cout << GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_return);
						  else
							  std::cout << slash;
					  }
					  std::cout << std::setw(7);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_REG_TIME))
							  std::cout << Utility::humanReadableDuration(DateTime::parseISO8601DateTime(GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_REG_TIME)));
						  else
							  std::cout << slash;
					  }
					  std::cout << std::setw(9);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_last_start))
							  std::cout << Utility::humanReadableDuration(DateTime::parseISO8601DateTime(GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_last_start)));
						  else
							  std::cout << slash;
					  }
					  std::cout << std::setw(7);
					  {
						  if (HAS_JSON_FIELD(jsonObj, JSON_KEY_APP_starts))
							  std::cout << GET_JSON_INT_VALUE(jsonObj, JSON_KEY_APP_starts);
						  else
							  std::cout << slash;
					  }
					  std::cout << GET_JSON_STR_VALUE(jsonObj, JSON_KEY_APP_command);
					  std::cout << std::endl;
				  });
}

void ArgumentParser::shiftCommandLineArgs(po::options_description &desc)
{
	m_commandLineVariables.clear();
	std::vector<std::string> opts = po::collect_unrecognized(m_parsedOptions, po::include_positional);
	// remove [command] option and parse all others in m_commandLineVariables
	if (opts.size())
		opts.erase(opts.begin());
	po::store(po::command_line_parser(opts).options(desc).run(), m_commandLineVariables);
	po::notify(m_commandLineVariables);
}

std::string ArgumentParser::reduceStr(std::string source, int limit)
{
	if (source.length() >= (std::size_t)limit)
	{
		return source.substr(0, limit - 2).append("*");
	}
	else
	{
		return source;
	}
}

std::size_t ArgumentParser::inputSecurePasswd(char **pw, std::size_t sz, int mask, FILE *fp)
{
	if (!pw || !sz || !fp)
		return -1; /* validate input   */
#ifdef MAXPW
	if (sz > MAXPW)
		sz = MAXPW;
#endif

	if (*pw == NULL)
	{
		/* reallocate if no address */
		void *tmp = realloc(*pw, sz * sizeof **pw);
		if (!tmp)
			return -1;
		memset(tmp, 0, sz); /* initialize memory to 0   */
		*pw = (char *)tmp;
	}

	std::size_t idx = 0; /* index, number of chars in read   */
	int c = 0;

	struct termios old_kbd_mode; /* orig keyboard settings   */
	struct termios new_kbd_mode;

	if (tcgetattr(0, &old_kbd_mode))
	{
		/* save orig settings   */
		fprintf(stderr, "%s() error: tcgetattr failed.\n", __func__);
		return -1;
	}
	/* copy old to new */
	memcpy(&new_kbd_mode, &old_kbd_mode, sizeof(struct termios));

	new_kbd_mode.c_lflag &= ~(ICANON | ECHO); /* new kbd flags */
	new_kbd_mode.c_cc[VTIME] = 0;
	new_kbd_mode.c_cc[VMIN] = 1;
	if (tcsetattr(0, TCSANOW, &new_kbd_mode))
	{
		fprintf(stderr, "%s() error: tcsetattr failed.\n", __func__);
		return -1;
	}

	/* read chars from fp, mask if valid char specified */
	while (((c = fgetc(fp)) != '\n' && c != EOF && idx < sz - 1) ||
		   (idx == sz - 1 && c == 127))
	{
		if (c != 127)
		{
			if (31 < mask && mask < 127) /* valid ascii char */
				fputc(mask, stdout);
			(*pw)[idx++] = c;
		}
		else if (idx > 0)
		{
			/* handle backspace (del)   */
			if (31 < mask && mask < 127)
			{
				fputc(0x8, stdout);
				fputc(' ', stdout);
				fputc(0x8, stdout);
			}
			(*pw)[--idx] = 0;
		}
	}
	(*pw)[idx] = 0; /* null-terminate   */

	/* reset original keyboard  */
	if (tcsetattr(0, TCSANOW, &old_kbd_mode))
	{
		fprintf(stderr, "%s() error: tcsetattr failed.\n", __func__);
		return -1;
	}

	if (idx == sz - 1 && c != '\n') /* warn if pw truncated */
		fprintf(stderr, " (%s() warning: truncated at %zu chars.)\n",
				__func__, sz - 1);

	return idx; /* number of chars in passwd    */
}
