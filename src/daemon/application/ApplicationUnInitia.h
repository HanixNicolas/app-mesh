#pragma once

#include <memory>
#include <string>

#include "Application.h"

//////////////////////////////////////////////////////////////////////////
/// An UnInitialize Application is pre-start cmd
///  and change to normal app when finished.
//////////////////////////////////////////////////////////////////////////
class ApplicationUnInitia : public Application
{
public:
	ApplicationUnInitia();
	virtual ~ApplicationUnInitia();

	static void FromJson(const std::shared_ptr<ApplicationUnInitia> &app, const web::json::value &jsonObj) noexcept(false);
	virtual web::json::value AsJson(bool returnRuntimeInfo) override;
	virtual void dump() override;

	virtual void enable() override;
	virtual void disable() override;
	virtual bool available() override;
	virtual void invoke(void *ptree = nullptr) override;

protected:
	web::json::value m_application;
	bool m_executed;
};
