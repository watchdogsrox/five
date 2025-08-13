#include "security/ragesecengine.h"
#ifndef SECURITY_APICHECKPLUGIN_H
#define SECURITY_APICHECKPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_API_CHECK
#include "security/obfuscatedtypes.h"

bool ApiCheckPlugin_Init();
bool ApiCheckPlugin_Work();

extern "C" void ApiCheckPlugin_StartNativeHash();
void ApiCheckPlugin_StopNativeHash();
extern "C" void ApiCheckPlugin_AddNative(u64);

struct OSModule
{
	sysObfuscated<u64> ModuleHandle;
	sysObfuscated<u64> ModuleImageBase;
	sysObfuscated<u64> ModuleImageSize;
};

class OSModuleInitializer
{
public:
	OSModuleInitializer();
};



#else
extern "C" void ApiCheckPlugin_StartNativeHash() { };
extern "C" void ApiCheckPlugin_StopNativeHash() { };
extern "C" void ApiCheckPlugin_AddNative(unsigned long long int) { };
#endif // RAGE_SEC_TASK_API_CHECK

#endif  // USE_RAGESEC

#endif  // SECURITY_APICHECKPLUGIN_H