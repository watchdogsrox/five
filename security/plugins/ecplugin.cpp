#include "security/ragesecengine.h"

#if USE_RAGESEC
  
#if RAGE_SEC_TASK_EC
 
#include <psapi.h>
#include <wchar.h>
//Rage headers
#include "string/stringhash.h"
#include "string/stringutil.h"
 
//Framework headers
#include "security/ragesecwinapi.h"
#include "system/threadtype.h"
 
//Game headers
#include "Network/Live/NetworkTelemetry.h"
#include "Network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkAssetVerifier.h"
#include "script/script.h"
#include "system/InfoState.h"
#include "system/memorycheck.h"
#include "streaming/defragmentation.h"
#include "stats/StatsInterface.h"
 
// Security headers
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "script/thread.h"
#include "security/plugins/ecplugin.h"
#include "security/ragesecgameinterface.h"
 
using namespace rage;
 
// #defines
#define EC_PAGES_TO_CHECK				256
#define EC_POLLING_MS					15*1000
#define EC_ID							0xD47E8701
#define EC_SIZE_OF_KB					sizeof(unsigned char)*1024
#define EC_SIZE_OF_PAGE					EC_SIZE_OF_KB * 4	// Pages indicated as 4 KB according to MSDN (aside from weird exceptions).

static u32 sm_ecTrashValue = 0;

#if __BANK
static atArray<MemoryCheck> sm_memoryCheckList;
 
bool IsBankMemCheckIndex(u32 index)
{
	return index < sm_memoryCheckList.GetCount();
}
 
bool IsCharHex(const char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
 
bool IsCharSpace(const char c)
{
	return c == ' ';
}
 
u8* CreateByteArray(const char* input, u32& outLength)
{
	u8* bytes = nullptr;
	const u32 inputLength = ustrlen(input);
	const u32 maxLength = 1024;
	char sanitizedString[maxLength] = { '\0' };
 
	// The string is null/empty
	if (StringNullOrEmpty(input))
		return bytes;
 
	// The string is too large
	if (inputLength > maxLength)
		return bytes;
 
	for (u32 i = 0; i < inputLength; i++)
	{
		char c = input[i];
 
		// Skip spaces
		if (IsCharSpace(c))
			continue;
 
		// The input string contains an invalid character
		if (!IsCharHex(c))
			return bytes;
 
		// Enforce uppercase values for consistency
		c = (char)toupper(c);
 
		// Save the sanitized character
		sanitizedString[i] = c;
	}
 
	const u32 sanitizedStringLength = ustrlen(sanitizedString);
	// The input contains an incomplete byte
	if (sanitizedStringLength % 2 != 0)
		return bytes;
 
	bytes = rage_new u8[sanitizedStringLength];
	outLength = 0;
	for (u32 i = 0; i < sanitizedStringLength; i += 2)
	{
		char* start = &sanitizedString[i];
		char* end = start + 1;
		u8 byte = (u8)strtoul(start, &end, 16);
		bytes[outLength++] = byte;
	}
 
	return bytes;
}
 
bool EcPlugin_InsertMemoryCheck(u32 versionNumber, u8 sku, const int pageLow, const int pageHigh, const int actionFlags, const char* inputBytes)
{
	// Sanitize and hash byte string
	u32 bytesLength = 0;
	u8* bytes = CreateByteArray(inputBytes, bytesLength);
	if (bytes == nullptr)
		return false;
	u32 hash = crcRange(bytes, bytes + bytesLength);
 
	// Create EC memory check
	MemoryCheck check;
	check.SetValue(hash);
	check.setVersionAndType(versionNumber, sku, MemoryCheck::CHECK_ExecutableChecker);
 
	// Compute start address using the page boundaries
	u32 rawAddressStart = ((pageHigh << 16) | (pageLow & 0xFFFF));
	check.SetAddressStart(rawAddressStart ^ hash ^ MemoryCheck::GetMagicXorValue());
 
	// Pack the first byte in the upper 8 bits
	// Pack the length of the data in the lower 24 bits
	u32 rawSize = 
		((bytes[0]		<< 24) & 0xFF000000 ) |
		((bytesLength	<< 18) & 0x00FC0000 );
	check.SetSize(rawSize ^ hash ^ MemoryCheck::GetMagicXorValue());
	check.SetFlags(actionFlags ^ hash ^ MemoryCheck::GetMagicXorValue());
 
	check.SetXorCrc((
		check.GetVersionAndType()	^ 
		check.GetAddressStart()		^ 
		check.GetSize()				^
		check.GetFlags()			^ 
		check.GetValue()));
 
	sm_memoryCheckList.PushAndGrow(check);
 
	// Free the allocated byte array.
	delete[] bytes;
 
	return true;
}
 
void EcPlugin_GetMemoryChecksFromTunables()
{
	const u32 numChecks = Tunables::GetInstance().GetNumMemoryChecks();
	for (u32 i = 0; i < numChecks; i++)
	{
		MemoryCheck check;
		Tunables::GetInstance().GetMemoryCheck(i, check);
		sm_memoryCheckList.PushAndGrow(check);
	}
}
#endif //__BANK
 
 
__forceinline void EcPlugin_GetMemoryCheck(int index, MemoryCheck& check)
{
#if __BANK
	if (!rageSecGameVerifyf(index >= 0 && index < EcPlugin_GetNumMemoryChecks(),
		"EcPlugin_GetMemoryCheck :: Invalid index of %d. Count: %d", index, EcPlugin_GetNumMemoryChecks()))
	{
		return;
	}
	else if (IsBankMemCheckIndex(index))
	{
		check = sm_memoryCheckList[index];
	}
	else
#endif //BANK
	{
		Tunables::GetInstance().GetMemoryCheck(index, check);
	}
}
__forceinline u32 EcPlugin_GetNumMemoryChecks()
{
	u32 numChecks = Tunables::GetInstance().GetNumMemoryChecks();
#if __BANK
	numChecks += sm_memoryCheckList.GetCount();
#endif //BANK
	return numChecks;
}

// PURPOSE: Obtains base address and size of .text/code section,
//			and populates the values passed by reference in args
// RETURNS: TRUE for success, FALSE for failure

bool EcPlugin_GetCodePageRange(u64 &address, u64 &size)
{
	//@@: range ECPLUGIN_GET_CODE_RANGE {
	PPEB peb = (PPEB)papi::GetPeb();
	PLIST_ENTRY moduleList = &peb->Ldr->InMemoryOrderLinks;
	PLIST_ENTRY forwardLink = moduleList->Flink;
	PLDR_DATA_TABLE_ENTRY ldrData = CONTAINING_RECORD(forwardLink, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
	
	address = (u64)ldrData->BaseAddress;
	size = (u64)ldrData->SizeOfImage;
	return true;
	//@@: } ECPLUGIN_GET_CODE_RANGE
}

bool EcPlugin_Create()
{
	gnetDebug3("0x%08X - Created", EC_ID);
	//@@: location ECPLUGIN_CREATE
	sm_ecTrashValue += sysTimer::GetSystemMsTime();
	return true;
}
 
bool EcPlugin_Configure()
{
	gnetDebug3("0x%08X - Configure", EC_ID);
	//@@: location ECPLUGIN_CONFIGURE
	sm_ecTrashValue -= sysTimer::GetSystemMsTime();
	return true;
}

bool EcPlugin_Work()
{
	gnetDebug3("0x%08X - Work Entry", EC_ID);
	//@@: location ECPLUGIN_WORK_ENTRY
	bool isMultiplayerManual =  CNetwork::IsNetworkSessionValid() && 
		NetworkInterface::IsGameInProgress() &&
		(CNetwork::GetNetworkSession().IsSessionActive() 
		|| CNetwork::GetNetworkSession().IsTransitionActive()
		|| CNetwork::GetNetworkSession().IsTransitionMatchmaking()
		|| CNetwork::GetNetworkSession().IsRunningDetailQuery());
 
	u64 baseAddress = 0;
	u64 regionSize = 0;
	if(!EcPlugin_GetCodePageRange(baseAddress, regionSize))
	{
		return true;
	}

	if(isMultiplayerManual)
	{
		// No-op if no memory checks in tunables
		//@@: location ECPLUGIN_WORK_BODY_FETCH_CHECKS
		const int numChecks = EcPlugin_GetNumMemoryChecks();
 
		//@@: range ECPLUGIN_WORK_BODY {

		// Fetch the memory checks for this. 
		for(int i = 0; i < numChecks; i++)
		{
			MemoryCheck memoryCheck;
			EcPlugin_GetMemoryCheck(i, memoryCheck);
 
			// Check the XOR CRC, in case people just stomp on memory, report it and return
			if( memoryCheck.GetXorCrc() !=  
				(memoryCheck.GetVersionAndType()	^ 
				memoryCheck.GetAddressStart()		^ 
				memoryCheck.GetSize()				^
				memoryCheck.GetFlags()	^ 
				memoryCheck.GetValue()))
			{
				RageSecPluginGameReactionObject obj(
					REACT_GAMESERVER, 
					EC_ID, 
					0x158BFC8F,
					0x8C74089F,
					0x466AEB2D);
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				return true; 
			}
 
			MemoryCheck::CheckType currType = memoryCheck.getCheckType();
 
			if(currType!=MemoryCheck::CHECK_ExecutableChecker)
			{
				// Nothing to do here
				continue;
			}
 
			//@@: location ECPLUGIN_WORK_BODY_XOR_MEMCHECK
			
			u32 xorValue = memoryCheck.GetValue() ^ MemoryCheck::GetMagicXorValue();
			u32 startAddr = memoryCheck.GetAddressStart() ^ xorValue;
			u32 size = memoryCheck.GetSize() ^ xorValue;
			//@@: range ECPLUGIN_WORK_BODY_RESOLVE_DATA {
			u32 m_lowPage		= startAddr & 0xFFFF;
			u32 m_highPage		= startAddr >> 16;
			u32 m_length		= (size & 0x00FC0000) >> 18;
			u32 m_firstByte		= (size & 0xFF000000) >> 24;
			u32 m_hash			= memoryCheck.GetValue();
			//@@: } ECPLUGIN_WORK_BODY_RESOLVE_DATA
			// Now lets take our page indices, and calculate our start / end address
			u64 lowPageOffset  = 0;
			u64 highPageOffset = 0;
			
			lowPageOffset	= baseAddress + EC_SIZE_OF_PAGE * m_lowPage;
			highPageOffset	= baseAddress + EC_SIZE_OF_PAGE * m_highPage;

			// Check to be sure that we're not starting at the last region size.
			// url:bugstar:3220889 
			
			//@@: location ECPLUGIN_WORK_BODY_BOUNDARY_CHECK
			bool found = false;

			if(baseAddress + regionSize <= lowPageOffset) {return true;}
			gnetDebug3("0x%08X - [Page] Checking base address of [%016" I64FMT "X]", EC_ID, baseAddress);
			
			//@@: range ECPLUGIN_WORK_BODY_SEARCH {
			for(u64 chkstart=0; chkstart < (highPageOffset - lowPageOffset - m_length); chkstart++)
			{
				if(lowPageOffset + chkstart >= baseAddress + regionSize)
				{
					break;
				}

				// Check the indexed address against our first packed byte
				if(((const u8*)lowPageOffset)[chkstart] == m_firstByte)
				{
					// CRC the range
					u32 actualVal = crcRange((const u8*)lowPageOffset + chkstart, (const u8*)lowPageOffset + chkstart + m_length);
					if(actualVal == m_hash)
					{
						//@@: location ECPLUGIN_WORK_BODY_FOUND
						found = true;
						// Set our index to be something past the iterative loop
						chkstart = highPageOffset;
					}
				}
			}
			//@@: } ECPLUGIN_WORK_BODY_SEARCH
 
			if(found == false)
			{
				//@@: location ECPLUGIN_WORK_BODY_REPORT
				RageSecPluginGameReactionObject obj(
					REACT_GAMESERVER, 
					EC_ID, 
					memoryCheck.GetVersionAndType() ^ xorValue,
					memoryCheck.GetAddressStart() ^ xorValue,
					m_hash);
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj); 
			}
		}
		//@@: } ECPLUGIN_WORK_BODY 
	}
	return true;
}
 
bool EcPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", EC_ID);
	//@@: location ECPLUGIN_ONSUCCESS
	sm_ecTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
bool EcPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", EC_ID);
	//@@: location ECPLUGIN_ONFAILURE
	sm_ecTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
 
 
bool EcPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly
 
	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
 
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= EC_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;
 
	rs.Create			= &EcPlugin_Create;
	rs.Configure		= &EcPlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &EcPlugin_Work;
	rs.OnSuccess		= &EcPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &EcPlugin_OnFailure;
 
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs, EC_ID);
 
	return true;
}
 
 
#endif // RAGE_SEC_TASK_EC
 
#endif // RAGE_SEC