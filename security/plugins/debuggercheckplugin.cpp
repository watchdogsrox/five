#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "security/ragesecwinapi.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "system/bootmgr.h"
#include "security/plugins//debuggercheckplugin.h"
using namespace rage;

#if RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK
#pragma warning( disable : 4100)

PARAM(enable_debugger_detection_tests, "N/A");

#define DEBUGGER_CHECK_POLLING_MS 60*1000
RAGE_DEFINE_SUBCHANNEL(net, debugger_check, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_debugger_check
#define DEBUGGER_CHECK_ID  0x9E2BC798 

static unsigned int sm_ddTrashValue	= 0;
enum Detections
{
	DD_NONE                             = 0xB54CE0B3,
	DD_ISDEBUGGER                       = 0xEB672B51,	// Needs testing/Support
	DD_ISDEBUGPEB                       = 0x40B85F02,	// Needs testing
	DD_TIMEDSTEPOVER                    = 0xE88A8F02,
	DD_NTQUERYINF                       = 0x7C4989AA,
	DD_NTGLOBALFLAGS                    = 0x8624A9A6,	// Needs testing
	DD_DEBUGREGISTERS                   = 0xEDA03F2F,	// Needs testing/Support
	DD_PROCESSDEBUGFLAGS                = 0xDFD5974A,	// Implemented
	DD_PARENTPROCESS                    = 0x802F2597,
	DD_SYSTEMKERNELDEBUGGERINFORMATION  = 0x942B6D4D,	// Implemented
	DD_DEBUGOBJECTFOUND                 = 0xC95A8A84,	// Implemented
	DD_VEHBREAKPOINT                    = 0xF07A7FC3,
	DD_IATHOOK                          = 0x2015F6E4,
	DD_CLOSEHANDLE                      = 0xC96CB241,	// Implemented
	DD_INT3                             = 0x11493BE8,	// Implemented
	DD_INT2C                            = 0x0153572B,	// Implemented
	DD_DEBUGACTIVEPROCESS               = 0x59E8C2FB,
	DD_HEAPFLAGS                        = 0x1D5D7268,	// Implemented
	DD_HEAPFORCEFLAGS                   = 0x5C21E644,	// Implemented
	DD_OUTPUTDEBUGSTRING                = 0xC7999CC7,
	DD_FINDWINDOWCLASS                  = 0x979E36B5,
	DD_RAISE_RIP_EXCEPTION              = 0xF5F55140,	// Implemented                                     
	DD_ALL = 0xADC0EF8D                           
};                                        

bool DebuggerCheckPlugin_BeingDebuggedPeb()
{
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	if(pPeb64)
	{
		PBYTE BeingDebugged = (PBYTE)pPeb64 + 0x2;
		if(*BeingDebugged)
		{
			return true;
		}
	}
	return false;
}

bool DebuggerCheckPlugin_NtGlobalFlag()
{
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	if (pPeb64){
		PDWORD pNtGlobalFlag = (PDWORD)((PBYTE)pPeb64 + 0xBC);
		if ((*pNtGlobalFlag & 0x70) == 0x70)
		{
			return true;
		}
	}
	return false;
}

bool DebuggerCheckPlugin_CheckDebugObjects()
{
	HANDLE hDebugObject = 0;
	NTSTATUS status = (NtDll::NtQueryInformationProcess)(
		GetCurrentProcess(), 
		0x1E, 
		&hDebugObject, 
		8, 
		0);
	if (status == 0 && NULL != hDebugObject) 
	{
		return true;
	}
	return false;
}

bool DebuggerCheckPlugin_CheckProcessDebugFlags()
{
	DWORD NoDebugInherit = 0; 
	NTSTATUS status = (NtDll::NtQueryInformationProcess)(
		GetCurrentProcess(), 
		0x1F, 
		&NoDebugInherit, 
		8, 
		0);
	if (status == 0 && NoDebugInherit == FALSE) 
	{
		return true;
	}
	return false;
}

bool DebuggerCheckPlugin_CheckSystemKernelDebuggerInformation()
{
	typedef long NTSTATUS;
	SYSTEM_KERNEL_DEBUGGER_INFORMATION Info;

	if (0 == (NtDll::NtQuerySystemInformation)(
		SystemKernelDebuggerInformation, 
		&Info, 
		sizeof(Info), 
		NULL)) 
	{
		if (Info.DebuggerEnabled && !Info.DebuggerNotPresent) 
		{
			return true;
		}
	}
	return false;
}

bool DebuggerCheckPlugin_CheckINT3Handler()
{
	__try 
	{
		__debugbreak(); 
	}
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{ 
		return false;;
	}

	return true;
}

#pragma warning(disable : 4996)
bool DebuggerCheckPlugin_CheckINT2CHandler()
{
	OSVERSIONINFO osvi;	
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);


	GetVersionEx(&osvi);

	if (osvi.dwMajorVersion < 6) //checks to see if we are on at least vista: NT 6.0
		return false;

	__try
	{
		__int2c();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}
#pragma warning(default: 4996)

bool DebuggerCheckPlugin_CheckHeapFlags()
{
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	PVOID heap = 0;
	PDWORD heapFlagsPtr = 0;

	if (pPeb64)
	{
		heap = (PVOID)*(PDWORD_PTR)((PBYTE)pPeb64 + 0x30);
		heapFlagsPtr = (PDWORD)((PBYTE)heap + 0x70);

		if (*heapFlagsPtr == 0x40000062) // Source: Ultimate Anti-Reversing Reference
		{
			return true;
		}
	}
	return false;
}

bool DebuggerCheckPlugin_CheckHeapForceFlags()
{
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	PVOID heap = 0;
	PDWORD heapForceFlagsPtr = 0;

	if (pPeb64)
	{
		heap = (PVOID)*(PDWORD_PTR)((PBYTE)pPeb64 + 0x30);
		heapForceFlagsPtr = (PDWORD)((PBYTE)heap + 0x74);

		if (*heapForceFlagsPtr == 0x40000060) //Found abnormal heap flag
		{
			return true;
		}
	}
	return false;
}

bool DebuggerCheckPlugin_CheckRaiseException()
{
	__try
	{
		RaiseException(DBG_RIPEXCEPTION, 0, 0, 0);
	}
	__except (1)
	{
		return false;
	}
	return true;
}

bool DebuggerCheckPlugin_CheckCloseHandle()
{
	HANDLE Handle = (HANDLE)0x8000;
	__try
	{
		CloseHandle(Handle);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return true;
	}
	return false;
}

// Testing functions for the RAG interface
// Designed to test the anti-debugger measures without generating a
// RageSecGameReactionObject for minimal footprint.
#if __BANK
// Tests checks relying on the BeingDebugged flag in the PEB
bool DebuggerCheckPlugin_Test_BeingDebugged()
{
	bool result = true;
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	if (pPeb64)
	{

		PBYTE pBeingDebugged = (PBYTE)pPeb64 + 0x2;
		BYTE prevValue = *pBeingDebugged;
		*pBeingDebugged = 0x1;
		if(!DebuggerCheckPlugin_BeingDebuggedPeb())
		{
			RageSecDebugf3("Failed BeingDebuggedPeb Test");
			result = false;
		}
		*pBeingDebugged = prevValue;
	}
	return result;
}

// Tests checks relying on the Heap related flags in the PEB
bool DebuggerCheckPlugin_Test_HeapFlags()
{
	bool result = true;
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	if (pPeb64)
	{
		PDWORD pHeap = (PDWORD)((PBYTE)pPeb64 + 0x30);
		PDWORD pHeapFlags = (PDWORD)((PBYTE)pHeap + 0x70);
		PDWORD pHeapForceFlags = (PDWORD)((PBYTE)pHeap + 0x74);

		DWORD prevHeapFlags = *pHeapFlags;
		DWORD prevHeapForceFlags = *pHeapForceFlags;

		*pHeapFlags = 0x0;
		*pHeapForceFlags = 0x1;

		if (!DebuggerCheckPlugin_CheckHeapFlags())
		{
			RageSecDebugf3("Failed Heap Flag Test");
			result = false;
		}
		if (!DebuggerCheckPlugin_CheckHeapForceFlags())
		{
			RageSecDebugf3("Failed Heap Force Flag Test");
			result = false;
		}
		*pHeapFlags = prevHeapFlags;
		*pHeapForceFlags = prevHeapForceFlags;
	}
	return result;
}

// Tests checks relying on the NtGlobalFlag in the PEB
bool DebuggerCheckPlugin_Test_NtGlobalFlag()
{
	bool result = true;
	DWORD_PTR pPeb64 = __readgsqword(0x60);
	if (pPeb64)
	{
		PDWORD pNtGlobalFlag = (PDWORD)((PBYTE)pPeb64 + 0xbc);
		DWORD prevNtGlobalFlag = *pNtGlobalFlag;
		*pNtGlobalFlag = 0x70;
		if (!DebuggerCheckPlugin_NtGlobalFlag())
		{
			RageSecDebugf3("Failed NtGlobalFlag Test");
			result = false;
		}
		*pNtGlobalFlag = prevNtGlobalFlag;
	}
	return result;
}

void DebuggerCheckPlugin_Test()
{
	RageSecDebugf2("PERFORMING ANTI DEBUGGER TESTS");
	RageSecDebugf2("[*] BeingDebugged Test......%s", DebuggerCheckPlugin_Test_BeingDebugged() ? "SUCCESS" : "FAILED");
	RageSecDebugf2("[*] Heap Flag Test......%s", DebuggerCheckPlugin_Test_HeapFlags() ? "SUCCESS" : "FAILED");
	RageSecDebugf2("[*] NtGlobal Flag Test......%s", DebuggerCheckPlugin_Test_NtGlobalFlag() ? "SUCCESS" : "FAILED");
	RageSecDebugf2("COMPLETED ANTI DEBUGGER TESTS");
	return;
}
#endif //__BANK

bool DebuggerCheckPlugin_Work()
{
	//@@: location DEBUGGERCHECKPLUGIN_WORK
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress();
	 
	//@@: range DEBUGGERCHECKPLUGIN_WORK_BODY {
	if(isMultiplayer)
	{
		//@@: location DEBUGGERCHECKPLUGIN_WORK_BODY_ENTRY
		RageSecPluginGameReactionObject obj(
			REACT_GAMESERVER, 
			CLOCK_GUARD_ID, 
			fwRandom::GetRandomNumber(),
			fwRandom::GetRandomNumber(),
			0);

		gnetDebug3("0x%08X - Running Tests", DEBUGGER_CHECK_ID);	

#if !__NO_OUTPUT
		if(PARAM_enable_debugger_detection_tests.Get()) 
		{
#endif
			if(DebuggerCheckPlugin_BeingDebuggedPeb())
			{
				obj.data = (u32)DD_ISDEBUGPEB;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
			}

			if(DebuggerCheckPlugin_NtGlobalFlag())
			{
				obj.data = (u32)DD_NTGLOBALFLAGS;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
			}

			if(DebuggerCheckPlugin_CheckDebugObjects())
			{
				obj.data = (u32)DD_DEBUGOBJECTFOUND;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
			}
			
			if(DebuggerCheckPlugin_CheckProcessDebugFlags())
			{
				obj.data = (u32)DD_PROCESSDEBUGFLAGS;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
			}

			if(DebuggerCheckPlugin_CheckSystemKernelDebuggerInformation())
			{
				obj.data = (u32)DD_SYSTEMKERNELDEBUGGERINFORMATION;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
			}

            //@@: location DEBUGGERCHECKPLUGIN_WORK_BODY_MID
			if(DebuggerCheckPlugin_CheckHeapFlags())
			{
				obj.data = (u32)DD_HEAPFLAGS;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());
			}

			if(DebuggerCheckPlugin_CheckHeapForceFlags())
			{
				obj.data = (u32)DD_HEAPFORCEFLAGS;
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				gnetDebug3("0x%08X - Debugger Status 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
			}
			

#if !__NO_OUTPUT
		}
#endif
		gnetAssertf(obj.data.Get() == 0, "0x%08X - Debugger Detected 0x%016X", DEBUGGER_CHECK_ID, obj.data.Get());	
	}
	return true;
	//@@: } DEBUGGERCHECKPLUGIN_WORK_BODY
}


bool DebuggerCheckPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", DEBUGGER_CHECK_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location DEBUGGERCHECKPLUGIN_ONSUCCESS
	sm_ddTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
bool DebuggerCheckPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", DEBUGGER_CHECK_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location DEBUGGERCHECKPLUGIN_ONFAILURE
	sm_ddTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool DebuggerCheckPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= DEBUGGER_CHECK_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= NULL;
	rs.Configure		= NULL;
	rs.Destroy			= NULL;
	rs.DoWork			= &DebuggerCheckPlugin_Work;
	rs.OnSuccess		= &DebuggerCheckPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &DebuggerCheckPlugin_OnFailure;
	
	// Register the function
	rageSecPluginManager::RegisterPluginFunction(&rs, DEBUGGER_CHECK_ID);
	
	return true;
}

#pragma warning( error : 4100)
#endif // RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK

#endif // RAGE_SEC