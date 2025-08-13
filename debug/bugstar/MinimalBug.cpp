#include "MinimalBug.h"

#include <stdio.h>

#include "string\string.h"

#include "system\new.h"
#include "debug\bugstar\BugstarIntegrationSwitch.h"

#include <time.h>

#if(BUGSTAR_INTEGRATION_ACTIVE)

CMinimalBug::CMinimalBug()
: m_number(-1)
, m_summary(NULL)
, m_x(0.0)
, m_y(0.0)
, m_z(0.0)
, m_severity(0)
, m_state(NULL)
, m_collectedOn(0)
, m_fixedOn(0)
, m_ownerId(0)
, m_qaOwnerId(0)
, m_component(0)
{
}

CMinimalBug::~CMinimalBug(void)
{
	if(m_summary != NULL)
	{
		delete m_summary;
		m_summary = NULL;
	}

	if(m_state != NULL)
	{
		delete m_state;
		m_state = NULL;
	}
}

// bug interface methods

int CMinimalBug::Number() const
{
	return m_number;
}

char* CMinimalBug::Summary() const
{
	return m_summary;
}

float CMinimalBug::X() const
{
	return m_x;
}

float CMinimalBug::Y() const
{
	return m_y;
}

float CMinimalBug::Z() const
{
	return m_z;
}

char* CMinimalBug::State() const
{
	return m_state;
}

int CMinimalBug::Severity() const
{
	return m_severity;
}

CMinimalBug::eBugCategory CMinimalBug::Category() const
{
	return m_category;
}

time_t CMinimalBug::CollectedOn() const
{
	return m_collectedOn;
}

time_t CMinimalBug::FixedOn() const
{
	return m_fixedOn;
}

int CMinimalBug::Owner() const
{
	return m_ownerId;
}

int CMinimalBug::QaOwner() const
{
	return m_qaOwnerId;
}


void CMinimalBug::SetNumber(int value)
{
	m_number = value;
}

void CMinimalBug::SetSummary(const char* value)
{
	if(m_summary != NULL)
	{
		delete m_summary;
		m_summary = NULL;
	}

	int len = istrlen(value);
	m_summary = rage_new char[len + 1];
	memset(m_summary,0,len+1);

	strcpy(m_summary,value);
}

void CMinimalBug::SetX(float value)
{
	m_x = value;
}

void CMinimalBug::SetY(float value)
{
	m_y = value;
}

void CMinimalBug::SetZ(float value)
{
	m_z = value;
}

void CMinimalBug::SetSeverity(int value)
{
	m_severity = value;
}

void CMinimalBug::SetState(const char* value)
{
	if(m_state != NULL)
	{
		delete m_state;
		m_state = NULL;
	}

	int len = istrlen(value);
	m_state = rage_new char[len + 1];
	memset(m_state,0,len+1);

	strcpy(m_state,value);
}

void CMinimalBug::SetCategory(const char* value)
{
	eBugCategory category = kC;

	if(stricmp(value, "A") == 0)
		category = kA;
	else if (stricmp(value, "B") == 0)
		category = kB;
	else if (stricmp(value, "C") == 0)
		category = kC;
	else if (stricmp(value, "D") == 0)
		category = kD;
	else if (stricmp(value, "TASK") == 0)
		category = kTask;
	else if (stricmp(value, "TODO") == 0)
		category = kTodo;
	else if (stricmp(value, "WISH") == 0)
		category = kWish;

	m_category = category;
}

void CMinimalBug::SetCategory(eBugCategory value)
{
	m_category = value;
}

void CMinimalBug::SetCollectedOn(time_t value)
{
	m_collectedOn = value;
}

void CMinimalBug::SetCollectedOn(const char* value)
{
	m_collectedOn = ConvertStringToTime(value);
}

void CMinimalBug::SetFixedOn(time_t value)
{
	m_fixedOn = value;
}

void CMinimalBug::SetFixedOn(const char* value)
{
	m_fixedOn = ConvertStringToTime(value);
}

void CMinimalBug::SetOwner(int value)
{
	m_ownerId = value;
}

void CMinimalBug::SetQaOwner(int value)
{
	m_qaOwnerId = value;
}

CMinimalBug& CMinimalBug::operator =(const CMinimalBug& other)
{
	if (this == &other)
	{
		return *this;
	}

	m_number = other.m_number;
	m_x = other.m_x;
	m_y	= other.m_y;
	m_z = other.m_z;
	m_severity = other.m_severity;
	m_category = other.m_category;
	m_collectedOn = other.m_collectedOn;
	m_fixedOn = other.m_fixedOn;
	m_ownerId = other.m_ownerId;
	m_qaOwnerId = other.m_qaOwnerId;
	m_component = other.m_component;

	if(m_summary != NULL)
	{
		delete m_summary;
		m_summary = NULL;
	}

	int len = istrlen(other.m_summary);
	m_summary = rage_new char[len + 1];
	memset(m_summary,0,len+1);
	strcpy(m_summary,other.m_summary);

	if(m_state != NULL)
	{
		delete m_state;
		m_state = NULL;
	}

	len = istrlen(other.m_state);
	m_state = rage_new char[len + 1];
	memset(m_state,0,len+1);
	strcpy(m_state,other.m_state);

	return *this;
}

time_t CMinimalBug::ConvertStringToTime(const char* timeString)
{
	int year, month, day, hour, minutes, seconds;
	sscanf(timeString, "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minutes, &seconds);

	struct tm when;
	when.tm_year = year - 1900;
	when.tm_mon = month - 1;
	when.tm_mday = day;
	when.tm_hour = hour;
	when.tm_min = minutes;
	when.tm_sec = seconds;
	return mktime(&when);
}

#endif
