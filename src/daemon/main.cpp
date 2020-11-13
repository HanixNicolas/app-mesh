#include <stdio.h>

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <set>
#include <fstream>

#include <ace/Init_ACE.h>
#include <ace/OS.h>
#include <pplx/threadpool.h>

#include "application/Application.h"
#include "process/AppProcess.h"
#include "Configuration.h"
#include "rest/ConsulConnection.h"
#include "HealthCheckTask.h"
#include "PersistManager.h"
#include "rest/PrometheusRest.h"
#include "ResourceCollection.h"
#include "rest/RestHandler.h"
#include "TimerHandler.h"
#include "../common/os/linux.hpp"
#include "../common/Utility.h"
#include "../common/PerfLog.h"
#ifndef NDEBUG
#include "../common/Valgrind.h"
#endif

std::set<std::shared_ptr<RestHandler>> m_restList;

int main(int argc, char *argv[])
{
	const static char fname[] = "main() ";
	PRINT_VERSION();
#ifndef NDEBUG
	// enable valgrind in debug mode
	VALGRIND_ENTRYPOINT_ONE_TIME(argv);
#endif

	try
	{
		ACE::init();

		// init log
		Utility::initLogging();
		LOG_INF << fname << "Entered working dir: " << getcwd(NULL, 0);

		// catch SIGHUP for 'systemctl reload'
		Configuration::handleSignal();

		// Resource init
		ResourceCollection::instance()->getHostResource();
		ResourceCollection::instance()->dump();

		// get configuration
		const auto configTxt = Configuration::readConfiguration();
		auto config = Configuration::FromJson(configTxt, true);
		Configuration::instance(config);
		auto configJsonValue = web::json::value::parse(GET_STRING_T(configTxt));
		if (HAS_JSON_FIELD(configJsonValue, JSON_KEY_Applications))
		{
			config->deSerializeApp(configJsonValue.at(JSON_KEY_Applications));
		}

		// working dir
		Utility::createDirectory(config->getDefaultWorkDir(), 00655);
		ACE_OS::chdir(config->getDefaultWorkDir().c_str());

		// set log level
		Utility::setLogLevel(config->getLogLevel());
		Configuration::instance()->dump();

		std::shared_ptr<RestHandler> httpServerIp4;
		std::shared_ptr<RestHandler> httpServerIp6;
		if (config->getRestEnabled())
		{
			// Thread pool: 6 threads
			crossplat::threadpool::initialize_with_threads(config->getThreadPoolSize());
			LOG_INF << fname << "initialize_with_threads:" << config->getThreadPoolSize();

			// Init Prometheus Exporter
			PrometheusRest::instance(std::make_shared<PrometheusRest>(config->getRestListenAddress(), config->getPromListenPort()));

			// Init REST
			if (!config->getRestListenAddress().empty())
			{
				// just enable for specified address
				httpServerIp4 = std::make_shared<RestHandler>(config->getRestListenAddress(), config->getRestListenPort());
				m_restList.insert(httpServerIp4);
			}
			else
			{
				// enable for both ipv6 and ipv4
				httpServerIp4 = std::make_shared<RestHandler>("0.0.0.0", config->getRestListenPort());
				m_restList.insert(httpServerIp4);
				try
				{
					httpServerIp6 = std::make_shared<RestHandler>(MY_HOST_NAME, config->getRestListenPort());
					m_restList.insert(httpServerIp6);
				}
				catch (const std::exception &e)
				{
					LOG_ERR << fname << e.what();
				}
				catch (...)
				{
					LOG_ERR << fname << "unknown exception";
				}
			}
		}

		// HA attach process to App
		auto snap = std::make_shared<Snapshot>();
		auto apps = config->getApps();
		auto snapfile = Utility::readFileCpp(SNAPSHOT_FILE_NAME);
		try
		{
			snap = Snapshot::FromJson(web::json::value::parse(snapfile.length() ? snapfile : std::string("{}")));
		}
		catch (...)
		{
			LOG_ERR << "recover snapshot failed with error " << std::strerror(errno);
		}
		std::for_each(apps.begin(), apps.end(), [&snap](std::vector<std::shared_ptr<Application>>::reference p) {
			if (snap && snap->m_apps.count(p->getName()))
			{
				auto &appSnapshot = snap->m_apps.find(p->getName())->second;
				auto stat = os::status(appSnapshot.m_pid);
				if (stat && appSnapshot.m_startTime == (int64_t)stat->starttime)
					p->attach(appSnapshot.m_pid);
			}
		});
		// reg prometheus
		config->registerPrometheus();

		// start one thread for timer (application & process event & healthcheck & consul report event)
		auto timerThreadA = std::make_unique<std::thread>(std::bind(&TimerHandler::runReactorEvent, ACE_Reactor::instance()));
		// increase thread here
		//auto timerThreadB = std::make_unique<std::thread>(std::bind(&TimerHandler::runReactorEvent, ACE_Reactor::instance()));

		// init consul
		std::string recoverConsulSsnId = snap ? snap->m_consulSessionId : "";
		ConsulConnection::instance()->initTimer(recoverConsulSsnId);

		// monitor applications
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(Configuration::instance()->getScheduleInterval()));
			PerfLog perf(fname);

			// monitor application
			auto allApp = Configuration::instance()->getApps();
			for (const auto &app : allApp)
			{
				app->invoke();
			}

			PersistManager::instance()->persistSnapshot();
			// health-check
			HealthCheckTask::instance()->doHealthCheck();
		}
	}
	catch (const std::exception &e)
	{
		LOG_ERR << fname << e.what();
	}
	catch (...)
	{
		LOG_ERR << fname << "unknown exception";
	}
	LOG_ERR << fname << "ERROR exited";
	ACE::fini();
	_exit(0);
	return 0;
}
