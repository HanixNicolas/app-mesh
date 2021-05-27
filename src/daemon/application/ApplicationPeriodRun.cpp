#include "ApplicationPeriodRun.h"
#include "../../common/Utility.h"
#include "../Configuration.h"
#include "../process/AppProcess.h"

ApplicationPeriodRun::ApplicationPeriodRun()
{
	const static char fname[] = "ApplicationPeriodRun::ApplicationPeriodRun() ";
	LOG_DBG << fname << "Entered.";
}

ApplicationPeriodRun::~ApplicationPeriodRun()
{
	const static char fname[] = "ApplicationPeriodRun::~ApplicationPeriodRun() ";
	LOG_DBG << fname << "Entered.";
}

void ApplicationPeriodRun::refreshPid(void* ptree)
{
	// 1. Do the same thing with short running app (refresh pid and return code)
	ApplicationShortRun::refreshPid(ptree);

	// 2. Start again when the short running app exited
	std::lock_guard<std::recursive_mutex> guard(m_appMutex);
	if (this->available() && !m_process->running())
	{
		this->invokeNow(0);
	}
}

void ApplicationPeriodRun::checkAndUpdateHealth()
{
	// same with long running application
	Application::checkAndUpdateHealth();
}

void ApplicationPeriodRun::FromJson(const std::shared_ptr<ApplicationPeriodRun> &app, const web::json::value &jsonObj)
{
	std::shared_ptr<ApplicationShortRun> fatherApp = app;
	ApplicationShortRun::FromJson(fatherApp, jsonObj);
}

web::json::value ApplicationPeriodRun::AsJson(bool returnRuntimeInfo)
{
	const static char fname[] = "ApplicationPeriodRun::AsJson() ";
	LOG_DBG << fname << "Entered.";

	web::json::value result = ApplicationShortRun::AsJson(returnRuntimeInfo);
	result[JSON_KEY_PERIOD_APP_keep_running] = web::json::value::boolean(true);
	return result;
}

void ApplicationPeriodRun::dump()
{
	const static char fname[] = "ApplicationPeriodRun::dump() ";

	ApplicationShortRun::dump();
	LOG_DBG << fname << "keep_running:true";
}
