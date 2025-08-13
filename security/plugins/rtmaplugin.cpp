#include "security/ragesecengine.h"

#if USE_RAGESEC

#if RSG_PC
#include <wchar.h>
#endif

//Rage headers
#include "string/stringhash.h"

//Framework headers
#include "security/ragesecwinapi.h"
#include "system/threadtype.h"

//Game headers
#include "Network/Live/NetworkTelemetry.h"
#include "Network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkAssetVerifier.h"
#include "Network/Sessions/NetworkSession.h"
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
#include "security/plugins/rtmaplugin.h"
#include "security/ragesechashutility.h"
#include "security/ragesecgameinterface.h"

using namespace rage;

#if RAGE_SEC_TASK_RTMA
// #defines
#define RTMA_PERFORMANCE_MS_THRESHOLD	20
#define RTMA_PAGES_TO_CHECK				256
#define RTMA_POLLING_MS					15*1000
#define RTMA_ID							0x6F996781
#define RTMA_SIZE_OF_KB					sizeof(unsigned char)*1024
#if RSG_PC
#define RTMA_SIZE_OF_PAGE				RTMA_SIZE_OF_KB * 4	// Pages indicated as 4 KB according to MSDN (aside from weird exceptions).
#elif RSG_ORBIS
#define RTMA_SIZE_OF_PAGE				RTMA_SIZE_OF_KB * 4 * 4
#endif

#if RSG_PC
static int	sm_maxRegionSizeToCheck = 0x10000;
#endif
static int sm_nextCRCIndex = 0;
static const size_t sm_maxFailedCRCs = 32;
static sysCriticalSectionToken m_cs;
static u32 sm_trashValue = 0;

#if RSG_PC || RSG_ORBIS
class MemoryRegionCheckFn
{
public:
	MemoryRegionCheckFn() :	m_numProcessed(0),	m_pMem(NULL),	m_memConstraintFlags(0)	{}

	void Reset() 
	{
		m_numProcessed = 0;
		m_pMem = NULL;
	}

#if RSG_PC
	virtual bool Check(MEMORY_BASIC_INFORMATION& info, bool& failed) = 0;
	DWORD m_memConstraintFlags;
#elif RSG_ORBIS
	virtual bool Check(SceKernelVirtualQueryInfo& info, bool& failed) = 0;
	unsigned int m_memConstraintFlags;
#endif
	int m_numProcessed;
	u8* m_pMem; 
};
#endif

#if RSG_PC || RSG_ORBIS
// check for a string/pattern/signature in a range
// TODO: Optimize this by utilizing the PEB_LDR_DATA to locate key
//		 memory locations
class MemoryRegionCheckPageByteString : public MemoryRegionCheckFn
{
public:
#if RSG_PC
	void Setup(u32 hash, u32 startAddr, u32 size, DWORD flags) 
#elif RSG_ORBIS
	void Setup(u32 hash, u32 startAddr, u32 size, unsigned int flags) 
#endif
	{
		// Lets de-serialize the fuckery we did in MemoryHasher
		// The best kind of deserializations are the hard-coded and non-abstracted ones
		// Inversion from MemoryHasher
		m_lowPage		= startAddr & 0xFFFF;
		m_highPage		= startAddr >> 16;
		m_sizeOfModule	= (size & 0x0003FFFF);
		m_length		= (size & 0x00FC0000) >> 18;
		m_firstByte		= (size & 0xFF000000) >> 24;
		m_hash			= hash;
		m_memConstraintFlags = flags >> ACTION_FLAG_MEM_CONSTRAINT_SHIFT;;
	}
#if RSG_PC
	virtual bool Check(MEMORY_BASIC_INFORMATION& info, bool& failed)
	{
		m_numProcessed++;
		// if memory region is of matching permissions (constraint flags)
		if (
			(info.State == MEM_COMMIT) && 
			// Check flags, that we're in commited memory
			(info.Protect & m_memConstraintFlags & 0xFF) == (m_memConstraintFlags & 0xFF)
			)
		{
			// Now since we have a basic filter, lets also check the sizes
			u32 sizeInKb = m_sizeOfModule * (RTMA_SIZE_OF_KB);
			u32 lowSizeThreshold	= (u32)(sizeInKb *0.9);
			u32 highSizeThreshold	= (u32)(sizeInKb *1.1);
			// Return true if we're beyond our threshold, but don't do any more processing.
			// Indicate we've seen it, and we've processed it, but it doesn't apply.
			if(info.RegionSize < lowSizeThreshold || info.RegionSize > highSizeThreshold){return true;}

			// Now lets take our page indices, and calculate our start / end address
			u64 lowPageOffset  = (u64)info.BaseAddress + RTMA_SIZE_OF_PAGE * m_lowPage;
			u64 highPageOffset = (u64)info.BaseAddress + RTMA_SIZE_OF_PAGE * m_highPage;

            // Check to be sure that we're not starting at the last region size.
            // url:bugstar:3220889 
            if( ((u64)info.BaseAddress + (u64)info.RegionSize) <= lowPageOffset) { return true;}
            
			bool found = false;
			// What we're doing here is going through the specified range, and looking for the 
			// first byte that we've specified from the mem hasher. This lets us index into
			// the byte that we care about to do the hashing.
			// The reason we check the		m_size - mstringSize is because we don't have to 
			// worry about the portion of code where we correctly detect the first byte, but 
			// the size happens to be less than is supported in the whole length
			gnetDebug3("0x%08X - [Page] Checking base address of [%016" I64FMT "X]", RTMA_ID, info.BaseAddress);
			for(u64 chkstart=0; chkstart < (highPageOffset - lowPageOffset - m_length); chkstart++)
			{
				if(lowPageOffset+chkstart >= (u64)info.BaseAddress + (u64)info.RegionSize)
				{
					// No matter what the lowPageOffset + index is, we don't want to go outside the boundary
					// of our current memory block.
					found = false;
					break;
				}

				// Check the indexed address against our first packed byte
				if(((const u8*)lowPageOffset)[chkstart] == m_firstByte)
				{
					// CRC the range
					u32 actualVal = crcRange((const u8*)lowPageOffset + chkstart, (const u8*)lowPageOffset + chkstart + m_length);
					if(actualVal == m_hash)
					{
						found = true;
						// Set our index to be something past the iterative loop
						chkstart = highPageOffset;
					}
				}
			}

			if(found)
			{
				failed = true;
				gnetDebug3("0x%08X - [Page] Module *did* match signature check.", RTMA_ID);
			}
			else
			{
				gnetDebug3("0x%08X - [Page] Module did not match signature check.", RTMA_ID);
			}
		}
		return true;
	}
#elif RSG_ORBIS
	virtual bool Check(SceKernelVirtualQueryInfo& sceInfo, bool& failed)
	{ 
		m_numProcessed++;

		// if memory region is executable and of a certain size, check for presence of string of length stringsize represented by hash
		u64 regionSize = (u64)sceInfo.end - (u64)sceInfo.start;

		u32 sizeInKb = m_sizeOfModule * (RTMA_SIZE_OF_KB);
		u32 lowSizeThreshold	= (u32)(sizeInKb *0.9);
		u32 highSizeThreshold	= (u32)(sizeInKb *1.1);
		gnetDebug3("0x%08X - [Page] Sizes [%016x]|[%016x]", RTMA_ID, m_sizeOfModule, sizeInKb);
		// Return true if we're beyond our threshold, but don't do any more processing.
		// Indicate we've seen it, and we've processed it, but it doesn't apply.
		if(regionSize < lowSizeThreshold || regionSize > highSizeThreshold) 
		{
			gnetDebug3("0x%08X - [Page] Size Short Circuit [%016lx]|[%016x]|[%016x]", RTMA_ID, regionSize, lowSizeThreshold, highSizeThreshold);
			return true;
		}

		// Now lets take our page indices, and calculate our start / end address
		u64 lowPageOffset  = (u64)sceInfo.start + RTMA_SIZE_OF_PAGE * m_lowPage;
		u64 highPageOffset = (u64)sceInfo.start + RTMA_SIZE_OF_PAGE * m_highPage;

		// Check to be sure that we're not starting at the last region size.
		// url:bugstar:3220889 
		if( ((u64)sceInfo.start + regionSize) <= lowPageOffset) { return true;}

		bool found = false;		

		gnetDebug3("0x%08X - [Page] Checking base address of [%p]", RTMA_ID, sceInfo.start);
		for(u64 chkstart=0; chkstart < (highPageOffset - lowPageOffset - m_length); chkstart++)
		{
			if(lowPageOffset+chkstart >= (u64)sceInfo.end)
			{
				// No matter what the lowPageOffset + index is, we don't want to go outside the boundary
				// of our current memory block.
				found = false;
				break;
			}

			// Check the indexed address against our first packed byte
			if(((const u8*)lowPageOffset)[chkstart] == m_firstByte)
			{
				// CRC the range
				u32 actualVal = crcRange((const u8*)lowPageOffset + chkstart, (const u8*)lowPageOffset + chkstart + m_length);
				if(actualVal == m_hash)
				{
					found = true;
					// Set our index to be something past the iterative loop
					chkstart = highPageOffset;
				}
			}
		}

		if(found)
		{
			failed = true;
			gnetDebug3("0x%08X - [Page] Module *did* match signature check.", RTMA_ID);
		}
		else
		{
			gnetDebug3("0x%08X - [Page] Module did not match signature check.", RTMA_ID);
		}
		return true;
	}
#endif

	u32 m_lowPage;
	u32 m_highPage;
	u32 m_length;
	u32 m_sizeOfModule;
	u32 m_hash;
	u8  m_firstByte;
};
#endif

#if RSG_PC || RSG_ORBIS
bool EnumerateMemoryRegions(MemoryRegionCheckFn& fn, bool& failed)
{
#if RSG_PC
	HANDLE process = Kernel32::GetCurrentProcess();
	MEMORY_BASIC_INFORMATION info;
	bool bFinished = true;
	unsigned int numDelayed = 0;
	gnetDebug3("0x%08X - Enumerating Memory Regions", RTMA_ID);
	// For every memory region in the user space
	unsigned int aggregate = 0;
	while(!failed && aggregate < RTMA_PERFORMANCE_MS_THRESHOLD)
	{
		size_t dummy;

		unsigned int start = sysTimer::GetSystemMsTime();
		NTSTATUS queryStatus = NtDll::NtQueryVirtualMemory(process, fn.m_pMem, MemoryBasicInformation, &info, sizeof(info), &dummy);
		if( queryStatus >= 0)
		{
			//gnetDebug3("0x%08X - NtQueryVirtualMemory Succeeded", RTMA_ID);
			bFinished = false;

			// Verify we are getting useful info from NtVirtualQueryMemoryFn
			if(fn.m_pMem != info.BaseAddress)
			{
				gnetDebug3("0x%08X - Base Addresses don't match", RTMA_ID);
				failed = true;
			}
			else
			{
				fn.m_pMem += info.RegionSize;
				unsigned int delta = sysTimer::GetSystemMsTime()-start;
				//gnetDebug3("0x%08X - Checking - %"I64FMT"X - %d", RTMA_ID,fn.m_pMem, delta);
				fn.Check(info, failed);
				numDelayed++;
				aggregate+=delta;
			}
		}
		else
		{
			// I imagine there would be at least 3 executable blocks if not a load more. Game in dev has over 100
			gnetDebug3("0x%08X - NtQueryVirtualMemory Failed - 0x%08X ", RTMA_ID, queryStatus);
			if(fn.m_numProcessed < 3)
				failed = true;
			break;
		}
	}
#elif RSG_ORBIS
	SceKernelVirtualQueryInfo sceInfo;
	bool bFinished = true;
	unsigned int numDelayed = 0;
	gnetDebug3("0x%08X - Enumerating Memory Regions", RTMA_ID);
	// For every memory region in the user space
	unsigned int aggregate = 0;
	while(!failed && aggregate < RTMA_PERFORMANCE_MS_THRESHOLD)
	{
		unsigned int start = sysTimer::GetSystemMsTime();
		int virtualQueryResult = sceKernelVirtualQuery(fn.m_pMem, SCE_KERNEL_VQ_FIND_NEXT, &sceInfo, sizeof(sceInfo));
		if (virtualQueryResult == SCE_KERNEL_ERROR_EACCES) 
		{ 
			// Ensuring that we set the finished to true, if we've gotten a kernel acces error
			bFinished = true;
			break;
		}

		// Okay, now according to Sony, here are the only cases to which memory is valid for 
		// querying
		if ((sceInfo.isFlexibleMemory || sceInfo.isDirectMemory) && (sceInfo.protection & SCE_KERNEL_PROT_CPU_READ))
		{
			bFinished = false;
			// Set the end pointer
			fn.m_pMem = (u8*)sceInfo.end;
			// Create our delta time
			unsigned int delta = sysTimer::GetSystemMsTime()-start;
			gnetDebug3("0x%08X - Checking - %p - %d", RTMA_ID,fn.m_pMem, delta);
			// Perform the checj
			fn.Check(sceInfo, failed);
			// Aggregate
			numDelayed++;
			aggregate+=delta;
		}
		else
		{
			gnetDebug3("0x%08X - sceKernelVirtualQuery Failed - 0x%08X ", RTMA_ID, virtualQueryResult);
			if(fn.m_numProcessed < 3)
				failed = true;
			break;
		}
	}
#endif
    
	if(failed)
	{
		bFinished = true;
		gnetDebug3("0x%08X - Memory check failed! This indicates that the executable has been tampered with.", RTMA_ID);
	}
	else
	{
		gnetDebug3("0x%08X - Memory check did NOT fail!", RTMA_ID);		
	}
    
	// If moving onto next check, reset values
	if(bFinished)
	{
		fn.Reset();
	}

	gnetDebug3("0x%08X - Finished Enumerating Memory Regions - %d", RTMA_ID, aggregate);
	return bFinished;
}
#endif

// Purpose: Checks the integrity of supporting modules and libraries
// Return true if supporting modules are not compromised, false otherwise
//			For Windows this is NtDll and Kernel32
__forceinline bool RtmaPlugin_VerifySupportModules()
{
#if RSG_PC
	if(NetworkInterface::IsGameInProgress())
	{
		// Check if NtDll is flagged as compromised
		if(NtDll::HasBeenHacked())
		{
			RageSecPluginGameReactionObject obj(REACT_GAMESERVER, RTMA_ID, 0x668FF11A, 0x84B0395B, 0x1FD96845);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			return false;
		}

		if(Kernel32::HasBeenHacked())
		{
			RageSecPluginGameReactionObject obj(REACT_GAMESERVER, RTMA_ID, 0x668FF11A, 0x84B0395B, 0x6DDB9C21);
			rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
			return false;
		}
	}
#endif
	return true;
}

// Purpose: Check if Tunables is loaded properly
// Return true if Tunables is loaded, false otherwise
__forceinline bool RtmaPlugin_VerifyTunableLoad()
{
	if(!Tunables::GetInstance().HaveTunablesLoaded())
	{
		gnetDebug3("0x%08X - Tunables have not been loaded", RTMA_ID);
		return false;
	}
	return true;
}

// Purpose: Check the CRC Checksum of the MemoryCheck object to detect tamper
// Return true if MemoryCheck is not modified, false otherwise
__forceinline bool RtmaPlugin_VerifyMemoryCheck(MemoryCheck *memoryCheck)
{
#if RSG_PC
	if( memoryCheck->GetXorCrc() !=  
		(memoryCheck->GetVersionAndType()	^ 
		memoryCheck->GetAddressStart()		^ 
		memoryCheck->GetSize()				^
		// This is a dirty hack. Below, we set the processed flag with an OR operator
		// which was causing this tamper metric to incorrectly send.
		// And since that flag is well outside my range, I'm just going to && this
		// accordingly, so that we still get the correct XOR check.
		memoryCheck->GetFlags()	^ 
		memoryCheck->GetValue()))
	{
		// What do I fill in here?
		CNetworkTelemetry::AppendInfoMetric(0x73617303, 0x5598B7EA, 0xB70A4BCB);
		gnetDebug3("0x%08X - Memory Check %d doesn't match the XOR'ing it would expect", RTMA_ID, sm_nextCRCIndex);
		return false; 
	}
#else
	(void)memoryCheck;  //Suppressing compiler error
#endif //RSG_PC
	return true;
}

// Purpose: Check if the game session is in multiplayer or not
// Return true if game is in multiplayer, false otherwise
__forceinline bool RtmaPlugin_VerifyMultiplayer()
{
	bool isMultiplayerManual =  CNetwork::IsNetworkSessionValid() && 
		NetworkInterface::IsGameInProgress() &&
		(CNetwork::GetNetworkSession().IsSessionActive() 
		|| CNetwork::GetNetworkSession().IsTransitionActive()
		|| CNetwork::GetNetworkSession().IsTransitionMatchmaking()
		|| CNetwork::GetNetworkSession().IsRunningDetailQuery());

	return isMultiplayerManual;
}

// Purpose: Check if known malicious byte string is present in memory
// Return true if successfully completed, false otherwise
bool RtmaPlugin_PageBytestringHandler(MemoryCheck *memoryCheck, bool &failed)
{
	bool retVal = true;
#if RSG_PC || RSG_ORBIS
	u32 xorValue = memoryCheck->GetValue() ^ MemoryCheck::GetMagicXorValue();
	u32 startAddr = memoryCheck->GetAddressStart() ^ xorValue;
	u32 len = memoryCheck->GetSize() ^ xorValue;
	gnetDebug3("0x%08X - Starting Page Byte String Check", RTMA_ID);
	static MemoryRegionCheckPageByteString memoryPageStringCheck;

	//@@: location RTMAPLUGIN_WORK_DETERMINE_CHECK_PAGE_BYTE_STRING_SETUP
	memoryPageStringCheck.Setup(memoryCheck->GetValue(), startAddr, len, memoryCheck->GetFlags() ^ xorValue);
	retVal = EnumerateMemoryRegions(memoryPageStringCheck, failed);
#else
	(void)memoryCheck;  //Suppressing compiler error
	(void)failed;		//Suppressing compiler error
#endif // RSG_PC | RSG_ORBIS

	return retVal;
}

// Purpose: Queue RageSecPluginGameReactionObjects to central queue depending on MemoryCheck flag values
// Returns nothing
void RtmaPlugin_QueueReactions(RageSecPluginGameReactionObject* obj, u32 flags)
{
	if (flags & MemoryCheck::FLAG_Telemetry)		
	{ 
		gnetDebug3("0x%08X - Queuing Telem", RTMA_ID);			
		obj->type = REACT_TELEMETRY; 
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj); 
	}
	if (flags & MemoryCheck::FLAG_Report)
	{
		gnetDebug3("0x%08X - Queuing P2P", RTMA_ID);				
		obj->type = REACT_P2P_REPORT;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	if (flags & MemoryCheck::FLAG_KickRandomly)
	{
		gnetDebug3("0x%08X - Queuing Random Kick", RTMA_ID);		
		obj->type = REACT_KICK_RANDOMLY;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	if (flags & MemoryCheck::FLAG_NoMatchmake)
	{
		gnetDebug3("0x%08X - Queuing MatchMake Bust", RTMA_ID);	
		obj->type = REACT_DESTROY_MATCHMAKING;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	if (flags & MemoryCheck::FLAG_Crash)
	{ 
		gnetDebug3("0x%08X - Invoking pgStreamer Crash", RTMA_ID);	
		RageSecInduceStreamerCrash();
	}
	if (flags & MemoryCheck::FLAG_Gameserver)
	{
		gnetDebug3("0x%08X - Queuing Gameserver Report", RTMA_ID);	
		obj->type = REACT_GAMESERVER;
		rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(*obj);
	}
	return;
}

bool RtmaPlugin_Create()
{
	gnetDebug3("0x%08X - Created", RTMA_ID);
	//@@: location RTMAPLUGIN_CREATE
	sm_trashValue += sysTimer::GetSystemMsTime();
	return true;
}

bool RtmaPlugin_Configure()
{
	gnetDebug3("0x%08X - Configure", RTMA_ID);
	//@@: location RTMAPLUGIN_CONFIGURE
	sm_trashValue -= sysTimer::GetSystemMsTime();
	return true;
}

bool RtmaPlugin_Work()
{
	gnetDebug3("0x%08X - Work Entry", RTMA_ID);
	//@@: range RTMAPLUGIN_WORK_SHORT_CIRCUIT {
	if(!CGame::IsSessionInitialized()) { return true; }
	if(!RtmaPlugin_VerifySupportModules()) { return true; } // Check if dependency modules are compromised
	if(!RtmaPlugin_VerifyTunableLoad()) { return true; } // Check if Tunables is loaded

#if RSG_PC
	sm_maxRegionSizeToCheck = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_REGION_SIZE_TO_CHECK", 0x7cb5e69d), 0x10000);
#endif //RSG_PC

	int numChecks = Tunables::GetInstance().GetNumMemoryChecks();
	if(numChecks == 0)
	{
		// Pass iteration until Tunables has memory checks available
		gnetDebug3("0x%08X - No MemoryChecks loaded by tunables.", RTMA_ID);
		return true;
	}

	MemoryCheck memoryCheck;
	Tunables::GetInstance().GetMemoryCheck(sm_nextCRCIndex, memoryCheck);
	gnetDebug3("0x%08X - Fetching Memory Check %d", RTMA_ID, sm_nextCRCIndex);

	if(!RtmaPlugin_VerifyMemoryCheck(&memoryCheck)) { return true; } // Verify MemoryCheck CRC
	
	u32 xorValue = memoryCheck.GetValue() ^ MemoryCheck::GetMagicXorValue();

	gnetDebug3("0x%08X - Number Other Players:%d", RTMA_ID, netInterface::GetNumPhysicalPlayers());
	bool bMoveOntoNextCheck = true;

	bool isMultiplayer = RtmaPlugin_VerifyMultiplayer();
	//@@: } RTMAPLUGIN_WORK_SHORT_CIRCUIT
	//@@: location RTMAPLUGIN_WORK_CHECK_ENTER
	sm_trashValue /= sysTimer::GetSystemMsTime();

	if(isMultiplayer && !((memoryCheck.GetFlags() ^ xorValue) & MemoryCheck::FLAG_MP_Processed))
	{
		bool failed = false;
		
		//@@: range RTMAPLUGIN_WORK_DETERMINE_CHECK {
		MemoryCheck::CheckType currType = memoryCheck.getCheckType();
		if(currType == MemoryCheck::CHECK_PageByteString)
		{
			bMoveOntoNextCheck = RtmaPlugin_PageBytestringHandler(&memoryCheck, failed);			
		}
		//@@: } RTMAPLUGIN_WORK_DETERMINE_CHECK


		//@@: location RTMAPLUGIN_WORK_CHECK_FAILED
		if(failed)
		{
			// Check failed, add to list
			gnetDebug3("0x%08X - Code CRC failed! Queuing Reaction Object", RTMA_ID);
			u32 actualVal = memoryCheck.GetValue();

			SYS_CS_SYNC(m_cs);
			//@@: location RTMAPLUGIN_WORK_ADD_TO_FAILED_CRCS
			RageSecPluginGameReactionObject obj(
				REACT_INVALID, 
				RTMA_ID, 
				memoryCheck.GetVersionAndType() ^ xorValue,
				memoryCheck.GetAddressStart() ^ xorValue,
				actualVal);
			u32 flags = memoryCheck.GetFlags() ^ xorValue;
			
			//@@: range RTMAPLUGIN_WORK_QUEUE_OBJECT {
			RtmaPlugin_QueueReactions(&obj, flags);

			// Flag this memory check as processed
			//@@: location RTMAPLUGIN_WORK_SET_PROCESSED
			flags |= MemoryCheck::FLAG_MP_Processed;
			//@@: } RTMAPLUGIN_WORK_QUEUE_OBJECT

			// Calling the instance so it sets the correct memoryCheck, instead of our locally accessed
			// one we're just using to read information. The better solution is to change
			// that into a pointer, so I can read/write to it.
			gnetDebug3("0x%08X - Setting memory index %d to processed", RTMA_ID, sm_nextCRCIndex);
			//@@: location RTMAPLUGIN_WORK_SET_MEMORY_FLAGS
			Tunables::GetInstance().SetMemoryCheckFlags(sm_nextCRCIndex, flags^xorValue);
		}
		else
		{
			gnetDebug3("0x%08X - CRC Check %d did not fail.", RTMA_ID, sm_nextCRCIndex);
		}
	}
	
	//@@: location RTMAPLUGIN_WORK_MOVEONTONEXTCHECK
	if(bMoveOntoNextCheck)
	{
		// Move to next function
		//@@: range RTMAPLUGIN_WORK_MOVEONTONEXTCHECK_BODY {
		gnetDebug3("0x%08X - Moving onto index %d of %d",RTMA_ID, sm_nextCRCIndex, numChecks-1);
		++sm_nextCRCIndex;
		if(sm_nextCRCIndex >= numChecks)
		{
			sm_nextCRCIndex = 0;
		}
		//@@: } RTMAPLUGIN_WORK_MOVEONTONEXTCHECK_BODY
	}

	return true;
}

bool RtmaPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;

	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= RTMA_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &RtmaPlugin_Create;
	rs.Configure		= &RtmaPlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &RtmaPlugin_Work;
	rs.OnSuccess		= NULL;
	rs.OnCancel			= NULL;
	rs.OnFailure		= NULL;

	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	rageSecPluginManager::RegisterPluginFunction(&rs, RTMA_ID);

	return true;
}


#endif // RAGE_SEC_TASK_RTMA

#endif // RAGE_SEC
