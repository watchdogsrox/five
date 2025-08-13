#include "security/ragesecengine.h"
#ifndef SECURITY_LINKDATAREPORTERPLUGIN_H
#define SECURITY_LINKDATAREPORTERPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_LINK_DATA_REPORTER

#include "atl/queue.h"
#include "atl/map.h"
#include "fwmaths/random.h"
#define MAX_QUEUED 128

bool LinkDataReporterPlugin_Create();
bool LinkDataReporterPlugin_Configure();
bool LinkDataReporterPlugin_Init();
bool LinkDataReporterPlugin_Work();



// Now we're inserting data-mismatching stuff.
enum LinkDataReporterCategories
{
	LDRC_HEALTH			= 0x9EC57FA5,
	LDRC_ARMOR			= 0x0FF229CB,
	LDRC_WEAPON_DMG		= 0x471835F2,
	LDRC_GENERAL		= 0xB970F44B,
	LDRC_PLAYERRESET	= 0x080D46FA,
	LDRC_INVALID		= 0x0324E5D7,
};

struct LinkDataReport
{
	LinkDataReport()
	{
		Data = fwRandom::GetRandomNumber();
		Category = (unsigned int)LDRC_INVALID;
	}
	LinkDataReport(unsigned int data, LinkDataReporterCategories category)
	{
		Data = data;
		Category = (unsigned int)category;
	};
	sysObfuscated<unsigned int> Data;
	sysObfuscated<unsigned int> Category;
};

void LinkDataReporterPlugin_Report(LinkDataReport);
#endif // RAGE_SEC_TASK_REVOLVING_CHECKER

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H