#include "security/ragesecengine.h"

#if USE_RAGESEC

#if RAGE_SEC_TASK_API_CHECK
#include "wolfssl/wolfcrypt/sha256.h"
#include "net/status.h"
#include "net/task.h"
#include "network/Network.h"
#include "script/commands_misc.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecwinapi.h"
#include "security/plugins/apicheckplugin.h"
#include "system/bootmgr.h"

using namespace rage;

RAGE_DEFINE_SUBCHANNEL(net, apicheck, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_apicheck

#define API_CHECK_POLLING_MS 1000*45
#define API_CHECK_ID 0x43ABFDDD 

static u32 sm_apicTrashValue	= 0;

#if RSG_PC
#include <psapi.h>

#define API_NUMBER_OS_MODULES_TRACKED 5
enum OSModuleList {
	NTDLL = 0,
	GAME = 1,
	DXGI = 2,
	KERNEL32 = 3
};
OSModule *g_OSModules;
OSModuleInitializer g_OsModuleInitializer;
OSModuleInitializer::OSModuleInitializer()
{
	//@@: location OSMODULEINITIALIZER_INITIALIZE_ENTRY
	HANDLE hProcess = GetCurrentProcess();
	//@@: range OSMODULEINITIALIZER_INITIALIZE {
	g_OSModules = rage_new OSModule[API_NUMBER_OS_MODULES_TRACKED];

	// Get information regarding ntdll
	HMODULE ntHandle = GetModuleHandle("ntdll.dll");
	MODULEINFO ntInfo;
	GetModuleInformation(hProcess, ntHandle, &ntInfo, sizeof(MODULEINFO));

	g_OSModules[NTDLL].ModuleHandle		= (u64)ntHandle;
	g_OSModules[NTDLL].ModuleImageBase	= (u64)ntInfo.lpBaseOfDll;
	g_OSModules[NTDLL].ModuleImageSize	= (u64)ntInfo.SizeOfImage;

	// Get information regarding kernel32
	HMODULE k32Handle = GetModuleHandle("kernel32.dll");
	MODULEINFO k32Info;
	GetModuleInformation(hProcess, k32Handle, &k32Info, sizeof(MODULEINFO));

	g_OSModules[KERNEL32].ModuleHandle		= (u64)k32Handle;
	g_OSModules[KERNEL32].ModuleImageBase	= (u64)k32Info.lpBaseOfDll;
	g_OSModules[KERNEL32].ModuleImageSize	= (u64)k32Info.SizeOfImage;

	// Get the game information
	HMODULE gameHandle = GetCurrentModule();
	MODULEINFO gameInfo;
	GetModuleInformation(hProcess, gameHandle, &gameInfo, sizeof(MODULEINFO));
	g_OSModules[GAME].ModuleHandle	  = (u64)gameHandle;
	g_OSModules[GAME].ModuleImageBase = (u64)gameInfo.lpBaseOfDll;
	g_OSModules[GAME].ModuleImageSize = (u64)gameInfo.SizeOfImage;
	//@@: } OSMODULEINITIALIZER_INITIALIZE

	// Close the handle
	//@@: location OSMODULEINITIALIZER_INITIALIZE_EXIT
	CloseHandle(hProcess);
}


#if !__FINAL
bool IS_MODULE_OUT_OF_BOUNDARIES(OSModuleList a, u64 b)
{
	if((b < g_OSModules[a].ModuleImageBase) || (b > (g_OSModules[a].ModuleImageBase + g_OSModules[a].ModuleImageSize)))
	{
		gnetDebug3("0x%08X - Found API outside of expected ranges: Function Address: [%016" I64FMT "X] ModuleImageBase: [%016" I64FMT "X] ModuleImageSize:[%016" I64FMT "X]", API_CHECK_ID, b, g_OSModules[a].ModuleImageBase.Get(), g_OSModules[a].ModuleImageSize.Get());
		return true;
	}
	return false;
}
bool IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList a, u64 b)
{
	if(IS_MODULE_OUT_OF_BOUNDARIES(a,b))
	{
		return true;
	}
	
	u8 *ptr = (u8*)(b);
	// Naive check for an E9 byte code.
	// Added a check for a NOP to account for the event an attacker inserted a NOP
	// before a proceeding JMP to bypass this check. This could be made more dynamic
	// at the cost of performance (scan).
	if(*ptr == 0xFF || *ptr == 0xEA || *ptr == 0xE9 || *ptr == 0xEB || *ptr == 0x90)
	{
		gnetDebug3("0x%08X - API's first byte is a jump. Function Address: [%016" I64FMT "X] ModuleImageBase: [%016" I64FMT "X] ModuleImageSize:[%016" I64FMT "X] Pointer Value [0x%02X]", API_CHECK_ID, b, g_OSModules[a].ModuleImageBase.Get(), g_OSModules[a].ModuleImageSize.Get(), *ptr);
		return true;
	}
	return false;
}
#else
#define IS_MODULE_OUT_OF_BOUNDARIES(a,b) (																								\
		(																																\
		(b < g_OSModules[a].ModuleImageBase)																							\
		|| (b > (g_OSModules[a].ModuleImageBase + g_OSModules[a].ModuleImageSize))														\
		)																																\
	)
#define IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(a,b) (																						\
		IS_MODULE_OUT_OF_BOUNDARIES(a,b)																								\
		||																																\
		(																																\
			(*((u8*)(b))==0xFF) || (*((u8*)(b))==0xEA) || (*((u8*)(b))==0xE9) || (*((u8*)(b))==0xEB) || (*((u8*)(b))==0x90)				\
		)																																\
	)		
#endif //!__FINAL
#endif //RSG_PC

/* Native Hash Bucket Checking */
const int BTSTraversalSize = 7;
const int BTSTopLevelSize = 0x100;
// Bucket Traversal Struct
// Same as the one found here:
// x:\gta5\src\dev_ng\rage\script\src\script\thread.h
// Without the templating so I can iterate through them quickly
// and minimize source code changes
struct BTS {
	// Data arranged this way for optimal packing on x64
	sysObfuscated<BTS *, false> Next;
	void *Data[BTSTraversalSize];
	sysObfuscated<u32, false> Count;
	sysObfuscated<u64, false> Hashes[BTSTraversalSize];
};
extern scrCommandHash<scrCmd> s_CommandHash;
__forceinline void ApiCheckPlugin_CheckHashBucket(RageSecPluginGameReactionObject* obj)
{
#if RSG_PC
	s_CommandHash.Lookup(0);
	BTS ** bts = (BTS**)(&s_CommandHash);
	gnetDebug3("0x%08X - Start Time: %d", API_CHECK_ID, sysTimer::GetSystemMsTime());

	// Use the top-level size value, so that I get the right iterations
	for(int i =0; i < BTSTopLevelSize ; i++)
	{
		BTS* currBucket = bts[i];
		for (currBucket; currBucket!=NULL; currBucket= currBucket->Next)
		{
			for (int k = 0; k < currBucket->Count; k++)
			{
				//gnetDebug3("0x%08X - Iterating : Address: [%016" I64FMT "X] / Hash: [%016" I64FMT "X]", API_CHECK_ID, (u64)currBucket->Data[k], (u64)currBucket->Hashes[k]);
				if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::GAME, (u64)currBucket->Data[k]))
				{
					// Here, we want to check something extra; we want to be sure that the jump is still within the image,
					// because as it turns out, a lot of the script natives are jumps. </awesome>

					// The good news is, the game only does E9's, so if it's anything else, I can roll right to the report
					// Get a pointer to the handler
					u8 *handlerPtr = (u8*)currBucket->Data[k];
					// Check to see if it's the type of jump that we'll care about
					if(*handlerPtr == 0xE9) 
					{
						// Add one to access its relative offset
						u8 *handlerRelAddr = handlerPtr+1;
						// Cast it to an s64, so we get the signed-ed-ness; this will give me a true relativity
						s32 *handlerRelAddrSI = (s32*) handlerRelAddr;
						// Check it's range; though it actually can be an E9 also, so don't do the byte check
						if(!IS_MODULE_OUT_OF_BOUNDARIES(OSModuleList::GAME, (u64)(0x5+(s64)(*handlerRelAddrSI)+(u64)(currBucket->Data[k]))))
						{
							// If it's not out of bounds, just mosey on
							continue;
						}
					}
					else if(*handlerPtr == 0xEB) 
					{
						// Add one to access its relative offset
						u8 *handlerRelAddr = handlerPtr+1;
						// Cast it to an s64, so we get the signed-ed-ness; this will give me a true relativity
						s8 *handlerRelAddrSI = (s8*) handlerRelAddr;
						// Check it's range; though it actually can be an E9 also, so don't do the byte check
						if(!IS_MODULE_OUT_OF_BOUNDARIES(OSModuleList::GAME, (u64)(0x2+(s64)(*handlerRelAddrSI)+(u64)(currBucket->Data[k]))))
						{
							// If it's not out of bounds, just mosey on
							continue;
						}
					}
					else
					{
						gnetDebug3("0x%08X - Script Function [%016" I64FMT "X] / [%016" I64FMT "X] had an unhandled first-byte", API_CHECK_ID, currBucket->Data[k], currBucket->Hashes[k]);
					}
					// Otherwise, report
					gnetAssertf(false, "0x%08X - Script Function was unexpectedly hooked. [%016" I64FMT "X]. Please create a B* on Amir Soofi", API_CHECK_ID, currBucket->Data[k]);
					obj->data.Set(0x578C4DF0);	
					rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
				}
			}
		}
	}
#else 
	(void)obj; //Suppress warning
#endif //RSG_PC
}

/* Registered Native Function Checking */
atSList<sysObfuscated<u64>> g_NativeArray;
wc_Sha256 g_NativeShaObject;
u8 g_NativeHash[WC_SHA256_DIGEST_SIZE];
void ApiCheckPlugin_StartNativeHash() 
{  
	wc_InitSha256(&g_NativeShaObject);
}
void ApiCheckPlugin_StopNativeHash()
{
	wc_Sha256Final(&g_NativeShaObject, g_NativeHash);
}
void ApiCheckPlugin_AddNative(u64 nativeAddr) 
{	
	// Create my node
	atSNode<sysObfuscated<u64>> *nde = rage_new atSNode<sysObfuscated<u64>>();
	// Assign the value
	nde->Data = nativeAddr;
	// Append the node
	g_NativeArray.Append(*nde);
	// Fetch the pointer so I can hash the bytes at that address
	u32 *ptr = (u32*)nativeAddr;
	gnetDebug3("0x%08X - SHA - AddNative - %p - %08X", API_CHECK_ID, ptr, *ptr);
	// Only hash the first four bytes, because the last four get unexpectedly 
	// mismatched depending on the size of the call being issued (particularly
	// if it's a call to near-rel)
	wc_Sha256Update(&g_NativeShaObject, (const u8*)ptr, sizeof(u32));
}
__forceinline void ApiCheckPlugin_CheckNativeAddress(RageSecPluginGameReactionObject* obj)
{
	obj->data.Set(0xE1072A5B);

	// Create the SHA object and initialize it
	wc_Sha256 lclCommandShaObj;
	wc_InitSha256(&lclCommandShaObj);
	// Create our output hash buffer
	u8 lclCommandHash[WC_SHA256_DIGEST_SIZE] = { 0 };

	// Iterate over each command, check if it's out of bounds, and hash it
	atSNode<sysObfuscated<u64>> *itr = g_NativeArray.GetHead();
	static bool printOnce = true;
		
	while(itr!= NULL)
	{
		u64 nativeAddr = itr->Data;
		u32 * nativePtr = (u32*)nativeAddr;
		// Hash the bytes at the location of the command
		if (printOnce) { gnetDebug3("0x%08X - SHA - CheckNative - %p - %08X", API_CHECK_ID, nativePtr, *nativePtr); }

		wc_Sha256Update(&lclCommandShaObj, (const byte*)nativePtr, sizeof(u32));
#if RSG_PC
		// Check to be sure it's in our process space
		// Do NOT change this to IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP; optimized builds will have some jumps.
		// This is intentional.
		if(IS_MODULE_OUT_OF_BOUNDARIES(OSModuleList::GAME, nativeAddr))
		{
			gnetAssertf(false, "0x%08X - Native with address 0x%016X not contained without game's memory. Please create a B* on Amir Soofi", API_CHECK_ID, nativeAddr);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
		}
#endif //RSG_PC
		itr = itr->GetNext();
	}
	printOnce = false;
	// Finalize the hash
	wc_Sha256Final(&lclCommandShaObj, lclCommandHash);

	// Compare the two, and report if necessary
	if (memcmp(lclCommandHash, g_NativeHash, WC_SHA256_DIGEST_SIZE) != 0)
	{
		gnetAssertf(false, "0x%08X - Native SHA Mismatch. Please create a B* on Amir Soofi", API_CHECK_ID);
		obj->data.Set(0x5359FE32);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
}



__forceinline bool ApiCheckPlugin_CheckMultiplayer()
{
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress() 
		&& !NetworkInterface::IsInSinglePlayerPrivateSession();

	bool isMultiplayerManual =  CNetwork::IsNetworkSessionValid() && 
		NetworkInterface::IsGameInProgress() &&
		(CNetwork::GetNetworkSession().IsSessionActive() 
		|| CNetwork::GetNetworkSession().IsTransitionActive()
		|| CNetwork::GetNetworkSession().IsTransitionMatchmaking()
		|| CNetwork::GetNetworkSession().IsRunningDetailQuery());

	if(isMultiplayer!=isMultiplayerManual)
		isMultiplayer=true;

	return isMultiplayer;
}

__forceinline void ApiCheckPlugin_CheckOSFunctionCalls(RageSecPluginGameReactionObject* obj)
{
#if RSG_PC
	if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::NTDLL, (u64)NtDll::NtQueryVirtualMemory))
	{
		gnetAssertf(false, "0x%08X - NtQueryVirtualMemory was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		RageSecInduceStreamerCrash();
	}

	if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::NTDLL, (u64)NtDll::NtQuerySystemInformation))
	{
		obj->data.Set(0xBAF7BDBF);
		gnetAssertf(false, "0x%08X - NtQuerySystemInformation was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}

	if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::NTDLL, (u64)NtDll::NtOpenProcess))
	{
		obj->data.Set(0x3376B403);
		gnetAssertf(false, "0x%08X - NtOpenProcess was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}

	// Kernel32 checks - Function addresses will be determined here until a design similar to the fwsys ntdll module is implemented.
	HMODULE k32Module = (HMODULE)g_OSModules[KERNEL32].ModuleHandle.Get();
	u64* getThreadContextAddr = (u64*)GetProcAddress(k32Module, "GetThreadContext");
	u64* setThreadContextAddr = (u64*)GetProcAddress(k32Module, "SetThreadContext");
	u64* getModuleFileNameWAddr = (u64*)GetProcAddress(k32Module, "GetModuleFileNameW");
	u64* getModuleFileNameAAddr = (u64*)GetProcAddress(k32Module, "GetModuleFileNameA");
		
	obj->version.Set(0x97071054);
	if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::KERNEL32, (u64)getThreadContextAddr))
	{
		obj->data.Set(0x7640f8e4);
		gnetAssertf(false, "0x%08X - GetThreadContext was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}

	if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::KERNEL32, (u64)setThreadContextAddr))
	{
		obj->data.Set(0xe6e99e24);
		gnetAssertf(false, "0x%08X - SetThreadContext was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}

	if(IS_MODULE_OUT_OF_BOUNDARIES(OSModuleList::KERNEL32, (u64)getModuleFileNameWAddr))
	{
		obj->data.Set(0x13450470);
		gnetAssertf(false, "0x%08X - GetModuleFileNameW was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}

	if(IS_MODULE_OUT_OF_BOUNDARIES(OSModuleList::KERNEL32, (u64)getModuleFileNameAAddr))
	{
		obj->data.Set(0x13450470);
		gnetAssertf(false, "0x%08X - GetModuleFileNameA was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	return;
#else 
	(void)obj; //Suppress warning
#endif//RSG_PC
}

bool ApiCheckPlugin_Create()
{
#if RSG_PC
	//@@: location APICHECKPLUGIN_CREATE_ENTRY 
	HANDLE hProcess = GetCurrentProcess();
	
	// Get information regarding DXGI
	//@@: range APICHECKPLUGIN_CREATE_BODY {
	HMODULE dxgiHandle = GetModuleHandle("dxgi.dll");
	MODULEINFO dxgiInfo;
	GetModuleInformation(hProcess, dxgiHandle, &dxgiInfo, sizeof(MODULEINFO));
	g_OSModules[DXGI].ModuleHandle	  = (u64)dxgiHandle;
	g_OSModules[DXGI].ModuleImageBase = (u64)dxgiInfo.lpBaseOfDll;
	g_OSModules[DXGI].ModuleImageSize = (u64)dxgiInfo.SizeOfImage;

	//@@: } APICHECKPLUGIN_CREATE_BODY
	// Close the handle
	//@@: location APICHECKPLUGIN_CREATE_EXIT
	CloseHandle(hProcess);
#endif //RSG_PC
	return true;
}

bool ApiCheckPlugin_Work()
{
	gnetDebug3("0x%08X - Work", API_CHECK_ID);
	//@@: location APICHECKPLUGIN_WORK_BODY_ENTRY
	bool isMultiplayer = ApiCheckPlugin_CheckMultiplayer();

	if(isMultiplayer)
	{
		//@@: range APICHECKPLUGIN_WORK_BODY {
		// Create the generic object
		RageSecPluginGameReactionObject obj(
			REACT_GAMESERVER, 
			API_CHECK_ID, 
			0x369920AC,
			0x74B87564,
			0x9F03843F);

		// 0x369920AC Will serve as an identifier for NTDLL, as those are the ones we're going to crush first

#if RSG_PC
		/* Check #1: Check if Ntdll and Kernel32 APIs are outside their respective module boundaries */
		ApiCheckPlugin_CheckOSFunctionCalls(&obj);

		/* Check #2: Check if DXGI APIs are outside the DXGI module boundaries */
		grcSwapChain *chain = grcDevice::GetBackupSwapChain();
		gnetAssertf(chain!=NULL, "0x%08X - Unable to read backup swap chain. Please create a B* on Amir Soofi", API_CHECK_ID);
	
		//@@: location APICHECKPLUGIN_WORK_BODY_RESOLVING_VTABLE
		u64* vtableEntry = (u64*)chain;
		u64* vtableEntryPresent = (u64*)(*vtableEntry);
		vtableEntryPresent+=8;

		//@@: location APICHECKPLUGIN_WORK_BODY_RESOLVING_FIRST_BYTE
		obj.version.Set(0x8D130FF2);
		if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::DXGI, (u64)vtableEntryPresent))
		{
			obj.data.Set(0x2123091B);
			gnetAssertf(false, "0x%08X - DXGI / SwapChain.Present() A was unexpectedly hooked [%016" I64FMT "X]. Please create a B* on Amir Soofi", API_CHECK_ID, (u64)vtableEntryPresent);;
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}

		/* Check #3: Check if game functions for reporting are outside the game module boundaries */
		obj.version.Set(0xEE4EC951);
		if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::GAME, (u64)(&CNetworkTelemetry::AppendInfoMetric)))
		{
			obj.data.Set(0x92096114);
			gnetAssertf(false, "0x%08X - CNetworkTelemetry::AppendInfoMetric was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
			//@@: location APICHECKPLUGIN_WORK_BODY_QUEUE_CRASH_A
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}


		if(IS_MODULE_OUT_OF_BOUNDARIES_OR_JUMP(OSModuleList::GAME, (u64)(&CNetwork::Bail)))
		{
			obj.data.Set(0x2C3D2DA7);
			gnetAssertf(false, "0x%08X - CNetwork::Bail was unexpectedly hooked. Please create a B* on Amir Soofi", API_CHECK_ID);
			//@@: location APICHECKPLUGIN_WORK_BODY_QUEUE_CRASH_B
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
		}

		/* Check #4: Check if addresses stored in the script hash buckets are outside the game module boundaries */
		ApiCheckPlugin_CheckHashBucket(&obj);
#endif //RSG_PC
		/* Check #5: Check if the hash value of registered script native addresses has changed */
		ApiCheckPlugin_CheckNativeAddress(&obj);

		gnetDebug3("0x%08X - End Time: %d", API_CHECK_ID, sysTimer::GetSystemMsTime());

		//@@: } APICHECKPLUGIN_WORK_BODY
	}
	return true;
}

bool ApiCheckPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", API_CHECK_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location APICHECKPLUGIN_ONSUCCESS
	sm_apicTrashValue	   -= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	return true;
}

bool ApiCheckPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", API_CHECK_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location APICHECKPLUGIN_ONFAILURE
	sm_apicTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool ApiCheckPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= API_CHECK_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &ApiCheckPlugin_Create;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &ApiCheckPlugin_Work;
	rs.OnSuccess		= &ApiCheckPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &ApiCheckPlugin_OnFailure;
	
	// Register the function
	//@@: location APICHECKPLUGIN_INIT_REGISTER_PLUGIN
	rageSecPluginManager::RegisterPluginFunction(&rs, API_CHECK_ID);
	return true;
}


#endif // RAGE_SEC_TASK_API_CHECK

#endif // RAGE_SEC