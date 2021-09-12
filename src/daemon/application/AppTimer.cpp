#include <ace/OS.h>

#include "../../common/DateTime.h"
#include "../../common/Utility.h"
#include "../DailyLimitation.h"
#include "AppTimer.h"

std::chrono::system_clock::time_point AppTimer::EPOCH_ZERO_TIME;

//////////////////////////////////////////////////////////////////////////
/// Calculate Application next start time
//////////////////////////////////////////////////////////////////////////
AppTimer::AppTimer(const std::chrono::system_clock::time_point &startTime, const std::chrono::system_clock::time_point &endTime,
                   std::shared_ptr<DailyLimitation> dailyLimit)
    : m_startTime(startTime == EPOCH_ZERO_TIME ? std::chrono::system_clock::now() : startTime),
      m_endTime(endTime == EPOCH_ZERO_TIME ? std::chrono::system_clock::now() + std::chrono::hours(24 * 365 * 10) : endTime),
      m_dailyLimit(dailyLimit)
{
}

std::chrono::system_clock::time_point AppTimer::nextTime(const std::chrono::system_clock::time_point &now)
{
    auto nextTime = checkStartTime(now);
    // check end
    if (nextTime > m_endTime)
    {
        return EPOCH_ZERO_TIME;
    }
    return nextTime;
}

std::chrono::system_clock::time_point AppTimer::adjustDailyTimeRange(std::chrono::system_clock::time_point target)
{
    const static char fname[] = "Application::adjustDailyTimeRange() ";
    if (m_dailyLimit != nullptr)
    {
        // Convert now to day time [%H:%M:%S], less than 24h
        auto now = DateTime::pickDayTimeUtcDuration(target);
        if (m_dailyLimit->m_startTimeValue < m_dailyLimit->m_endTimeValue)
        {
            // Start less than End means valid range should between start and end.
            if (now < m_dailyLimit->m_startTimeValue || now >= m_dailyLimit->m_endTimeValue)
            {
                if (now < m_dailyLimit->m_startTimeValue)
                {
                    auto diff = (m_dailyLimit->m_startTimeValue - now).total_seconds();
                    target += std::chrono::seconds(diff);
                    LOG_DBG << fname << "target: " << now << " with startTime: " << m_dailyLimit->m_startTimeValue << ", endTime: " << m_dailyLimit->m_endTimeValue << " adjust seconds: <" << diff << ">";
                }
                else if (now > m_dailyLimit->m_endTimeValue)
                {
                    auto diff = std::chrono::hours(24) - std::chrono::seconds((m_dailyLimit->m_endTimeValue - now).total_seconds());
                    target += diff;
                    LOG_DBG << fname << "target: " << now << " with startTime: " << m_dailyLimit->m_startTimeValue << ", endTime: " << m_dailyLimit->m_endTimeValue << " adjust seconds: <" << diff.count() << ">";
                }
            }
        }
        else if (m_dailyLimit->m_startTimeValue > m_dailyLimit->m_endTimeValue)
        {
            // Start greater than End means from end to start is invalid range (the valid range is across 0:00).
            if (now >= m_dailyLimit->m_endTimeValue || now < m_dailyLimit->m_startTimeValue)
            {
                if (now < m_dailyLimit->m_startTimeValue)
                {
                    auto diff = (m_dailyLimit->m_startTimeValue - now).total_seconds();
                    target += std::chrono::seconds(diff);
                    LOG_DBG << fname << "target: " << now << " with startTime: " << m_dailyLimit->m_startTimeValue << ", endTime: " << m_dailyLimit->m_endTimeValue << " adjust seconds: <" << diff << ">";
                }
                else if (now > m_dailyLimit->m_endTimeValue)
                {
                    auto diff = std::chrono::hours(24) - std::chrono::seconds((m_dailyLimit->m_endTimeValue + now).total_seconds());
                    target += diff;
                    LOG_DBG << fname << "target: " << now << " with startTime: " << m_dailyLimit->m_startTimeValue << ", endTime: " << m_dailyLimit->m_endTimeValue << " adjust seconds: <" << diff.count() << ">";
                }
            }
        }
    }
    return target;
}

bool AppTimer::isInDailyTimeRange(const std::chrono::system_clock::time_point &target)
{
    //const static char fname[] = "Application::isInDailyTimeRange() ";
    // 1. check date range
    if (target < m_startTime || target > m_endTime)
    {
        return false;
    }
    // 2. check daily range
    if (m_dailyLimit != nullptr)
    {
        // Convert now to day time [%H:%M:%S], less than 24h
        auto now = DateTime::pickDayTimeUtcDuration(target);
        //LOG_DBG << fname << "now: " << now << ", startTime: " << m_dailyLimit->m_startTimeValue << ", endTime: " << m_dailyLimit->m_endTimeValue;
        if (m_dailyLimit->m_startTimeValue < m_dailyLimit->m_endTimeValue)
        {
            // Start less than End means valid range should between start and end.
            return (now >= m_dailyLimit->m_startTimeValue && now < m_dailyLimit->m_endTimeValue);
        }
        else if (m_dailyLimit->m_startTimeValue > m_dailyLimit->m_endTimeValue)
        {
            // Start greater than End means from end to start is invalid range (the valid range is across 0:00).
            return !(now >= m_dailyLimit->m_endTimeValue && now < m_dailyLimit->m_startTimeValue);
        }
    }
    return true;
}

std::chrono::system_clock::time_point AppTimer::checkStartTime(const std::chrono::system_clock::time_point &target)
{
    // 1. check start
    auto nextTime = target;
    if (nextTime < m_startTime)
    {
        nextTime = m_startTime;
    }

    // 2. adjust daily limitation
    nextTime = adjustDailyTimeRange(nextTime);
    return nextTime;
}

//////////////////////////////////////////////////////////////////////////
/// Calculate Application next start time for periodic run
//////////////////////////////////////////////////////////////////////////
AppTimerPeriod::AppTimerPeriod(const std::chrono::system_clock::time_point &startTime, const std::chrono::system_clock::time_point &endTime,
                               std::shared_ptr<DailyLimitation> dailyLimit, int intervalSeconds)
    : AppTimer(startTime, endTime, dailyLimit), m_intervalSeconds(intervalSeconds)
{
}

std::chrono::system_clock::time_point AppTimerPeriod::nextTime(const std::chrono::system_clock::time_point &now)
{
    auto nextTime = checkStartTime(now);
    // check end
    if (nextTime < m_endTime)
    {
        auto startTime = std::chrono::system_clock::to_time_t(m_startTime);
        auto nowTime = std::chrono::system_clock::to_time_t(nextTime);
        auto diff = m_intervalSeconds % startTime;
        nextTime = std::chrono::system_clock::from_time_t(nowTime + diff);
        // again, make sure the target is in daily range
        nextTime = adjustDailyTimeRange(nextTime);
        if (nextTime <= m_endTime)
        {
            return nextTime;
        }
    }
    return EPOCH_ZERO_TIME;
}

//////////////////////////////////////////////////////////////////////////
/// Calculate Application next start time for cron schedule
//////////////////////////////////////////////////////////////////////////
AppTimerCron::AppTimerCron(const std::chrono::system_clock::time_point &startTime, const std::chrono::system_clock::time_point &endTime,
                           std::shared_ptr<DailyLimitation> dailyLimit, const std::string &cronExpr, int intervalSeconds)
    : AppTimerPeriod(startTime, endTime, dailyLimit, intervalSeconds), m_cronExpr(cronExpr)
{
    m_cron = cron::make_cron(m_cronExpr);
}

std::chrono::system_clock::time_point AppTimerCron::nextTime(const std::chrono::system_clock::time_point &now)
{
    auto nextTime = checkStartTime(now);
    // check end
    if (nextTime < m_endTime)
    {
        auto nextStartTimeT = std::chrono::system_clock::to_time_t(nextTime);
        auto nextTimeT = cron::cron_next(m_cron, nextStartTimeT);
        auto diffSeconds = std::abs(nextTimeT - nextStartTimeT);
        if (diffSeconds == 1)
        {
            // cron min unit is 1 minute, add 1 minutes to start to calculate again
            auto beginTime = nextTime + std::chrono::minutes(1);
            nextTimeT = cron::cron_next(m_cron, std::chrono::system_clock::to_time_t(beginTime));
        }
        // again, make sure the target is in daily range
        nextTime = adjustDailyTimeRange(std::chrono::system_clock::from_time_t(nextTimeT));
        if (nextTime <= m_endTime)
        {
            return nextTime;
        }
    }
    return EPOCH_ZERO_TIME;
}