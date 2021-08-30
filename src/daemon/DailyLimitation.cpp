#include "DailyLimitation.h"
#include "../common/DateTime.h"
#include "../common/Utility.h"

DailyLimitation::DailyLimitation()
{
}

DailyLimitation::~DailyLimitation()
{
}

bool DailyLimitation::operator==(const std::shared_ptr<DailyLimitation> &obj) const
{
	if (obj == nullptr)
		return false;
	return (m_startTime == obj->m_startTime && m_endTime == obj->m_endTime);
}

void DailyLimitation::dump()
{
	const static char fname[] = "DailyLimitation::dump() ";

	LOG_DBG << fname << "m_startTime:" << m_startTime;
	LOG_DBG << fname << "m_endTime:" << m_endTime;
}

web::json::value DailyLimitation::AsJson() const
{
	web::json::value result = web::json::value::object();
	result[JSON_KEY_DAILY_LIMITATION_daily_start] = web::json::value::string(m_startTime);
	result[JSON_KEY_DAILY_LIMITATION_daily_end] = web::json::value::string(m_endTime);
	return result;
}

std::shared_ptr<DailyLimitation> DailyLimitation::FromJson(const web::json::value &jsonObj, const std::string &posixTimeZone)
{
	std::shared_ptr<DailyLimitation> result;
	if (!jsonObj.is_null())
	{
		if (!(HAS_JSON_FIELD(jsonObj, JSON_KEY_DAILY_LIMITATION_daily_start) && HAS_JSON_FIELD(jsonObj, JSON_KEY_DAILY_LIMITATION_daily_end)))
		{
			throw std::invalid_argument("should both have daily_start and daily_end parameter");
		}
		result = std::make_shared<DailyLimitation>();
		result->m_startTime = GET_JSON_STR_VALUE(jsonObj, JSON_KEY_DAILY_LIMITATION_daily_start);
		result->m_endTime = GET_JSON_STR_VALUE(jsonObj, JSON_KEY_DAILY_LIMITATION_daily_end);
		result->m_startTimeValue = DateTime::parseDayTimeUtcDuration(result->m_startTime, posixTimeZone);
		result->m_endTimeValue = DateTime::parseDayTimeUtcDuration(result->m_endTime, posixTimeZone);
	}
	return result;
}
