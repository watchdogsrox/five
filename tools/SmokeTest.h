#ifndef SMOKETEST_H
#define SMOKETEST_H

#if !__FINAL

#include "diag/channel.h"

// Maybe this channel should be declared in another file, but this is good for now. - DW
RAGE_DECLARE_CHANNEL(AssetTest)
#define atDisplayf(fmt,...)					RAGE_DISPLAYF(AssetTest,fmt,##__VA_ARGS__)

namespace SmokeTests 
{
	struct SmokeTestDef 
	{
		const char* pName;		
		void (*InitFn)();
		void (*UpdateFn)();
		const char* pDesc;
	};

	//*********************************************************
	//	Functions for setting up smoke tests.
	void ProcessSmokeTest();
	bool HasSmokeTestFinished();

	SmokeTestDef* GetSmokeTestDef(const char* pStr);
	void DisplaySmokeTestStarted(const char* pStr);
	void DisplaySmokeTestFinished();

	bool SmokeTestStarted();
	void SmokeTestEnd();

	extern int g_automatedSmokeTest;
}

#endif // !__FINAL


#endif	// SMOKETEST_H
