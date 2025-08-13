#include "security/ragesecengine.h"
#ifndef SECURITY_SCRIPTHOOKPLUGIN_H
#define SECURITY_SCRIPTHOOKPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_SCRIPTHOOK

// Include RTMA for the CRC Range function
#include "security/ragesechashutility.h"

bool ScriptHookPlugin_Init();
bool ScriptHookPlugin_Work();
extern atMap<int, int> sm_runtimeRsp;
__forceinline void SetRuntimeRsp(void * input)
{
	u8* ptr = (u8*)input;
	u8* ptrEnd = ptr+8;
	int crc = crcRange(ptr,ptrEnd);
	if(sm_runtimeRsp.Access(crc)== NULL)
	{
		sm_runtimeRsp.Insert(crc, fwRandom::GetRandomNumber());
	}
}
extern __forceinline void ScriptHookPlugin_CheckCallingFunction(void* addr);
#define SCRIPT_CHECK_CALLING_FUNCTION ScriptHookPlugin_CheckCallingFunction(_AddressOfReturnAddress());
#else // RAGE_SEC_TASK_SCRIPTHOOK
#define SCRIPT_CHECK_CALLING_FUNCTION 
#endif // RAGE_SEC_TASK_SCRIPTHOOK_CHECK

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H