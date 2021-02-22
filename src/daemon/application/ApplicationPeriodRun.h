#pragma once

#include "ApplicationShortRun.h"

//////////////////////////////////////////////////////////////////////////
/// An Period Application will start period but keep running all the time.
//////////////////////////////////////////////////////////////////////////
class ApplicationPeriodRun : public ApplicationShortRun
{
public:
	ApplicationPeriodRun();
	virtual ~ApplicationPeriodRun();

	static void FromJson(const std::shared_ptr<ApplicationPeriodRun> &app, const web::json::value &jsonObj) noexcept(false);
	virtual web::json::value AsJson(bool returnRuntimeInfo) override;
	virtual void dump() override;

protected:
	virtual void refreshPid() override;
	virtual void checkAndUpdateHealth() override;
};
