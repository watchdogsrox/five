//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================

//Rage headers
#include "file/packfile.h"
#include "net/http.h"
#include "parsercore/stream.h"
#include "parser/manager.h"
#include "streaming/packfilemanager.h"
#include "string/stringutil.h"
#include "system/criticalsection.h"
#include "system/new.h"

//framework
#include "fwsys/timer.h"
#include "fwsys/fileExts.h"

//Game Headers
#include "game/BackgroundScripts/BackgroundScripts.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "optimisations.h"

#include "script/gta_thread.h"
#include "script/script.h"
#include "script/streamedscripts.h"

#include "system/appcontent.h"

#if __XENON
#include "system/xtl.h"
#endif

#include <time.h>

RAGE_DEFINE_CHANNEL(bgScript, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_DISPLAY, DIAG_SEVERITY_ERROR)

NETWORK_OPTIMISATIONS()
SCRIPT_OPTIMISATIONS()

const int BACKGROUNDSCRIPTS_SCRIPT_POOL_SIZE = 25 * 1024;
const int BACKGROUNDSCRIPTS_SCRIPT_INFO_POOL_SIZE = 25 * 1024; // Was 190k when last checked.

#define MAX_BG_CONTEXT_COUNT 25
#define MAX_RAW_SCRIPT_COUNT 250

#define UPDATES_PER_FRAME 100

#if __FINAL
#define BG_CLOUD_FILE_NAME_JA	"bgscripts/bg_ng_jp.rpf"
#define BG_CLOUD_FILE_NAME	"bgscripts/bg_ng.rpf"
#else
#define BG_CLOUD_FILE_NAME_JA	"bgscripts/bg_ng_dev_jp.rpf"
#define BG_CLOUD_FILE_NAME	"bgscripts/bg_ng_dev.rpf"
#endif

#define BG_CLOUD_FILE_SIZE		10 * 1024

#define BGSCRIPT_CLOUD_MOUNT_PATH "bgscripts_cloud:/"

#define BGSCRIPT_XML_FILE_PATH "%sbgscripts.xml"

#define BGSCRIPT_RPF_FILE "BgScript.rpf"

BANK_ONLY(PARAM(bgscriptswindow, "Makes a window for the BG Script system.");)

#if !__FINAL
#define BGSCRIPT_TEST_MOUNT_PATH "bgscripts_test:/"
#define TEST_BGSCRIPT_PACK_PATH "platform:/data/bg/"
PARAM(noBGScripts, "Disables BG Scripts from updating/running.");
PARAM(bgscriptsloadpack, "Specify a test BGScript pack to load, relative to platform:/data/bg/");
PARAM(bgscriptsnoCloud, "Don't request the BGScript Cloud file");
PARAM(bgscriptscloudfile, "Specifies cloud file");
PARAM(bgscriptsusememberspace, "Specifies cloud file to use member space.");
PARAM(bgscriptssimulateblock, "Ignore result of cloud request");
#endif

// ================================================================================================================
// BGScriptInfo
// ================================================================================================================

BGScriptInfo::BGScriptInfo()
{
	m_flags = 0;
	Init();
}

void BGScriptInfo::Init()
{
	// We don't want to clear these complete flag out (as the Cloud might download after this is set)
	m_flags &= (FLAG_IS_COMPLETE);
	m_scriptThdId = THREAD_INVALID;
	m_iPTProgramId = srcpidNONE;
	m_msExitTime = 0;
	m_nameHash = 0;
	m_category = 0;
}

bool BGScriptInfo::IsRunning() const
{
	return scrThread::GetThread(m_scriptThdId) != NULL;
}

bool BGScriptInfo::RequestedEnd()
{
	if( const GtaThread *pScrThread = static_cast<const GtaThread*>(scrThread::GetThread(m_scriptThdId)) )
	{
		return pScrThread->IsValid() && pScrThread->IsExitFlagResponseSet();
	}

	return false;
}

void BGScriptInfo::UpdateStatus(const ContextMap& contextMap, const u64& posixTime, const StatFlags gameMode)
{
	const bool isRunning = IsRunning();
	//IF the thread was running and the Exit flag has been set, it may have requested to finish.
	if(isRunning && RequestedEnd())
	{
		bgScriptDisplayf( "Stopping %s (for %s) because the exit flag has been set.", GetProgramName(), GetName());
		Stop();
	}
	else
	{
		const bool shouldBeRunning = ShouldBeRunning(contextMap, posixTime, gameMode);
		if( shouldBeRunning && !isRunning )
		{
			bgScriptDisplayf( "Starting %s (for %s)", GetProgramName(), GetName());
			Boot();
		}
		else if( !shouldBeRunning && isRunning )
		{
			bgScriptDisplayf( "Stopping %s (for %s)", GetProgramName(), GetName());
			Stop();
		}
	}
}

void BGScriptInfo::SetName( const char *name )
{
	BANK_ONLY(SBackgroundScripts::GetInstance().GetDebug().AddName(name);)
	SetNameHash(atStringHash(name));
}

#if !__NO_OUTPUT
const char* BGScriptInfo::GetName() const
{
	const char* pName = NULL;
	BANK_ONLY(pName = SBackgroundScripts::GetInstance().GetDebug().GetName(m_nameHash);)
	return pName ? pName : "BGScriptInfoName";
}
#endif	// __NO_OUTPUT

const BGScriptInfo::LaunchParam* CommonScriptInfo::GetLaunchParam(u32 hash) const
{
	const LaunchParam* pParam = NULL;

	for(int i=0; i<m_aLaunchParams.size(); ++i)
	{
		if(m_aLaunchParams[i].m_nameHash == hash)
		{
			pParam = &m_aLaunchParams[i];
			break;
		}
	}
 
	return pParam;
}

int BGScriptInfo::LaunchParam::GetValueFromData() const
{
	int retVal = 0;

	switch (m_header.type)
	{
	case INT:
		retVal = m_iData;
		break;
	default:
		bgScriptAssertf(0, "Unhandled header type: %d", m_header.type);
	}

	return retVal;
}

bool BGScriptInfo::LaunchParam::CompareValueAgainstData(int value) const
{
	bgScriptAssertf(m_header.isScore, "Trying to compare the score, when this isn't a score param.");

	bool retVal = false;
	int data = GetValueFromData();

	switch (m_header.compare)
	{
	case LESS_THAN:
		retVal = (value < data);
		break;
	case GREATER_THAN:
		retVal = (value > data);
		break;
	case EQUAL_TO:
		retVal = (value == data);
		break;
	case LESS_THAN_EQUAL_TO:
		retVal = (value <= data);
		break;
	case GREATER_THAN_EQUAL_TO:
		retVal = (value >= data);
		break;
	case NOT_EQUAL_TO:
		retVal = (value != data);
		break;
	default:
		bgScriptAssertf(0, "Unhandled ScoreParam compare type(%d)", m_header.compare);
	}

	return retVal;
}

void BGScriptInfo::LaunchParam::Reset()
{
	m_nameHash = 0;
	m_iData = 0;
	m_uAward = 0;
	m_header.type = INT;
	m_header.award_type = CASH;
	m_header.compare = GREATER_THAN_EQUAL_TO;
	m_header.isScore = false;
}

bool BGScriptInfo::ShouldBeRunning(const ContextMap& UNUSED_PARAM(rContextMap), const u64& UNUSED_PARAM(posixTime), const StatFlags gameMode) const
{	
	return !GetAreAllFlagsSet(FLAG_FORCENORUN) && GetIsAnyFlagSet(gameMode);
}

void BGScriptInfo::RequestStreamedScript(const char* pMountPath, bool bBlockingLoad)
{
	Assertf(pMountPath, "Empty mount path");

	//See if we have a program of the given name in the current packfile
	char programFilename[64];
	char programPackfileName[128];

	formatf(programFilename, "%s.%s", GetProgramName(), SCRIPT_FILE_EXT );
	formatf(programPackfileName, "%s%s", pMountPath, programFilename );

	bgScriptDebug1("Loading script program for %s from %s", GetName(), programPackfileName);

	bgScriptAssertf(!m_imageIndex.IsValid(), "Possible scripts streaming requests leak! RequestStreamedScript called more than once on the same BGScriptInfo");

	m_imageIndex = strPackfileManager::RegisterIndividualFile(programPackfileName, true, programFilename, false);
	strStreamingEngine::GetInfo().SetBGScriptIdx(m_imageIndex);

	if ( bgScriptVerifyf(m_imageIndex.IsValid(),"%s file(%s) for %s wasn't found at %s", SCRIPT_FILE_EXT, GetProgramName(), GetName(), programPackfileName) )
	{ 
		g_StreamedScripts.StoreBGScriptPackStreamIndex(m_imageIndex);
		bgScriptVerifyf(strStreamingEngine::GetInfo().RequestObject(m_imageIndex, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED|STRFLAG_DONTDELETE), "Failed to request %s", GetProgramName());

		if(bBlockingLoad)
		{
			strStreamingEngine::GetLoader().LoadAllRequestedObjects(true);
			bgScriptAssertf(strStreamingEngine::GetInfo().HasObjectLoaded(m_imageIndex), "%s:Script hasn't loaded", GetProgramName());
		}
	}
}

void BGScriptInfo::Boot()
{
	if( scrThread::GetThread(m_scriptThdId) == NULL)
	{
		if(bgScriptVerifyf(m_imageIndex.IsValid(), "Trying to boot a non streaming requested script") && strStreamingEngine::GetInfo().HasObjectLoaded(m_imageIndex))
		{
			if( m_iPTProgramId == srcpidNONE )
			{
				if( scrProgram *psc = g_StreamedScripts.Get(strLocalIndex(g_StreamedScripts.GetObjectIndex(m_imageIndex)) ) )
				{
					m_iPTProgramId = (scrProgramId)(size_t)psc->GetHashCode();
				}
				else
				{
					bgScriptAssertf(false, "Couldn't find %s in g_StreamedScripts. Do we need to request its load?", GetProgramName());
				}

				if( Unlikely(m_iPTProgramId == srcpidNONE) )
				{
					// must have had an error loading the file
					bgScriptError("Couldn't load %s", GetProgramName());
					SetFlag(FLAG_FORCENORUN);
					OnFailedToLoad();
					return;
				}
			}

			// PRE: At this point we know m_iPTProgramId!=srcpidNONE
			// If params are the wrong size, then this won't start.
			if (scrProgram::GetProgram(m_iPTProgramId)->GetArgStructSize()*sizeof(scrValue) == sizeof(m_launchData))
			{
				m_scriptThdId = scrThread::CreateThread(m_iPTProgramId, &m_launchData, sizeof(m_launchData), GtaThread::GetDefaultStackSize());

				bgScriptDebug1("Starting script %s using thread %i", GetName(), m_scriptThdId);
				// we have the program, so even if this fails for some reason, we are allowed to try again until it works.
				if( bgScriptVerify(m_scriptThdId != THREAD_INVALID) )
				{
					// initialize the script
					if( GtaThread *pBGScript= static_cast<GtaThread *> (scrThread::GetThread(m_scriptThdId)) )
					{
						pBGScript->Initialise();

						CTheScripts::GetScriptHandlerMgr().RegisterScript(*pBGScript);
					}
					else
					{
						bgScriptAssertf(false, "Failed to initialize %s", GetProgramName());
					}
				}
			}
			else
			{
				bgScriptWarning("BackgroundScript (%s) is incompatible with the game (wrong number of parameters, script(%s)=%lu bytes, code=%lu bytes), disabling", GetName(), GetProgramName(), scrProgram::GetProgram(m_iPTProgramId)->GetArgStructSize()*sizeof(scrValue), sizeof(m_launchData));
				SetFlag(FLAG_FORCENORUN);
			}
		}
	}
}

// Function shutdown the script and remove its streaming info right now
void BGScriptInfo::Shutdown()
{
	GtaThread* pScrThread = static_cast<GtaThread*>(scrThread::GetThread(m_scriptThdId));

	bgScriptDebug1("Shutting down script %s in thread %i", GetName(), m_scriptThdId);
	bgScriptAssert(m_imageIndex.IsValid());

	if(m_imageIndex.IsValid() && strStreamingEngine::GetInfo().GetStreamingInfoRef(m_imageIndex).GetStatus() == STRINFO_LOADED)
	{
		if (pScrThread)
		{
			// clear out the program reference so its memory can be freed
			if(m_iPTProgramId != srcpidNONE && scrProgram::GetProgram(m_iPTProgramId))
			{
				scrProgram::GetProgram(m_iPTProgramId)->Release();
				m_iPTProgramId = srcpidNONE;
			}
		}

		strStreamingEngine::GetInfo().ClearRequiredFlag(m_imageIndex, STRFLAG_MISSION_REQUIRED|STRFLAG_DONTDELETE);
		strStreamingEngine::GetInfo().RemoveObject(m_imageIndex);
	}
	else
	{
		if (pScrThread)
		{
			// clear out the program reference so its memory can be freed
			if(m_iPTProgramId != srcpidNONE && scrProgram::GetProgram(m_iPTProgramId))
			{
				scrProgram::GetProgram(m_iPTProgramId)->Release();
				m_iPTProgramId = srcpidNONE;
			}
		}

	}

	if (m_imageIndex.IsValid() && strStreamingEngine::GetInfo().GetStreamingInfoRef(m_imageIndex).GetStatus() == STRINFO_NOTLOADED)
	{
		if (pScrThread)
		{
			scrThread::KillThread(m_scriptThdId);
			m_scriptThdId = THREAD_INVALID;
		}

		// Unregister streaming object only after killing the thread, and if this script is not meant to run ever again
		bgScriptVerifyf(strStreamingEngine::GetInfo().UnregisterObject(m_imageIndex), "Failed unregistering object for %s", GetProgramName());
		m_imageIndex = strIndex(strIndex::INVALID_INDEX);
	}
	else
	{
		if (pScrThread)
		{
			scrThread::KillThread(m_scriptThdId);
			m_scriptThdId = THREAD_INVALID;
		}
	}
}

void BGScriptInfo::Stop()
{
	if( GtaThread *pScrThread = static_cast<GtaThread*>(scrThread::GetThread(m_scriptThdId)) )
	{
#if __ASSERT
		scrProgram::AllowDelete(true);
#endif
		
		bool bIsSafeToKillThread = true;

		//If this was a run only once script, change it's force No run now
		if(GetAreAllFlagsSet(FLAG_ONLYRUNONCE))
		{
			bgScriptDebug1("%s set to never run again.", GetName());
			SetFlag(FLAG_FORCENORUN);
		}

		//If we haven't set the exit flag, notifying the script of termination, set it, and wait
		if( /*m_msExitTime == 0*/ !pScrThread->IsExitFlagSet() )
		{
			bgScriptDebug1("Setting exit flag to script %s in thread id %i (%X)", GetName(), m_scriptThdId, pScrThread->m_HashOfScriptName);
			pScrThread->SetExitFlag();

			m_msExitTime = fwTimer::GetSystemTimeInMilliseconds();
		}
		//Else, the script has been notified
		else
		{
			//The script will signal that is has shut down. Otherwise, it waits 30 seconds.
			const u32 shutdownTime = fwTimer::GetSystemTimeInMilliseconds() - m_msExitTime;
			if( pScrThread->IsExitFlagResponseSet() || pScrThread->GetState()==scrThread::ABORTED || shutdownTime > 30000 ) //timeout after thirty seconds and then force kill the thread
			{
				bgScriptDebug1("Stopping script %s in thread %i", GetName(), m_scriptThdId);
				bgScriptAssertf(pScrThread->IsExitFlagResponseSet(), "%s (%s) is not calling BG_SET_EXITFLAG_RESPONSE() on exit", pScrThread->GetScriptName(), GetProgramName());
				bgScriptAssert(m_imageIndex.IsValid());

				// If this script is not going to run again, we can safely remove all its resources
				if( GetAreAllFlagsSet(FLAG_FORCENORUN) )
				{
					bIsSafeToKillThread = false;

					if(strStreamingEngine::GetInfo().GetStreamingInfoRef(m_imageIndex).GetStatus() == STRINFO_LOADED)
					{
						// clear out the program reference so its memory can be freed
						if(m_iPTProgramId != srcpidNONE)
						{
							scrProgram::GetProgram(m_iPTProgramId)->Release();
							m_iPTProgramId = srcpidNONE;
						}

						strStreamingEngine::GetInfo().ClearRequiredFlag(m_imageIndex, STRFLAG_MISSION_REQUIRED|STRFLAG_DONTDELETE);
						strStreamingEngine::GetInfo().RemoveObject(m_imageIndex);
					}

					// All resources have been removed. Now is safe to finally kill the thread
					bIsSafeToKillThread = strStreamingEngine::GetInfo().GetStreamingInfoRef(m_imageIndex).GetStatus() == STRINFO_NOTLOADED;
				}

				if(bIsSafeToKillThread)
				{
					if( GtaThread *pBGScript = static_cast<GtaThread *> (scrThread::GetThread(m_scriptThdId)) )
					{
						if (pBGScript)
						{
							if (pBGScript->m_Handler)
							{
								CTheScripts::GetScriptHandlerMgr().UnregisterScript(*pBGScript);
							}
						}
					}

					scrThread::KillThread(m_scriptThdId);
					m_scriptThdId = THREAD_INVALID;

					// Unregister streaming object only after killing the thread, and if this script is not meant to run ever again
					if( GetAreAllFlagsSet(FLAG_FORCENORUN) )
					{
						bgScriptVerifyf(strStreamingEngine::GetInfo().UnregisterObject(m_imageIndex), "Failed unregistering object for %s", GetProgramName());
						m_imageIndex = strIndex(strIndex::INVALID_INDEX);
					}
				}
			}
		}

#if __ASSERT
		scrProgram::AllowDelete(false);
#endif
	}
	else
	{
		m_scriptThdId = THREAD_INVALID;
	}
}


// ================================================================================================================
// CommonScriptInfo
// ================================================================================================================

CommonScriptInfo::CommonScriptInfo()
{
	Init();
}

void CommonScriptInfo::Init()
{
	BGScriptInfo::Init();

	m_szPTProgramName[0] = '\0';
	m_aLaunchParams.Reset();
	m_aValidContexts.Reset();
}

void CommonScriptInfo::AddLaunchParam(LaunchParam& param)
{
	if(bgScriptVerifyf(!m_aLaunchParams.IsFull(), "%s has too many ScoreParams and LaunchParams", GetName()))
	{
		m_aLaunchParams.Push(param);
	}
}

bool CommonScriptInfo::ShouldBeRunning(const ContextMap& rContextMap, const u64& posixTime, const StatFlags gameMode) const
{	
	return BGScriptInfo::ShouldBeRunning(rContextMap, posixTime, gameMode) && HasActiveContext(rContextMap);
}

bool CommonScriptInfo::HasActiveContext(const ContextMap& rContextMap) const
{
	// check for contexts
	bool hasValidContext = false;
	
	// AND all contexts
	if(GetIsAnyFlagSet(FLAG_NEED_ALL_CONTEXTS))
	{
		hasValidContext = true;
		for( const u32 *contextHash = m_aValidContexts.begin(); contextHash != m_aValidContexts.end(); ++contextHash )
		{
			const Context* pContext = rContextMap.Access(*contextHash);
			if( pContext == NULL )
			{
				hasValidContext = false;
				break;
			}
		}
	}
	else // OR all contexts
	{
		hasValidContext = false;
		for( const u32 *contextHash = m_aValidContexts.begin(); contextHash != m_aValidContexts.end(); ++contextHash )
		{
			const Context* pContext = rContextMap.Access(*contextHash);
			if( pContext )
			{
				hasValidContext = true;
				break;
			}
		}
	}

	return hasValidContext;
}

void CommonScriptInfo::SetProgramName( const char *programName )
{
	safecpy( m_szPTProgramName, programName );
}

void CommonScriptInfo::AddContext( u32 contextHash )
{
	u32 &rHash = m_aValidContexts.Append();
	rHash = contextHash;
}


// ================================================================================================================
// UtilityScriptInfo
// ================================================================================================================

UtilityScriptInfo::UtilityScriptInfo()
:	CommonScriptInfo()
{
	Init();
}

void UtilityScriptInfo::Init()
{
	CommonScriptInfo::Init();

	m_launchData.scriptType.Int = SCRIPT_TYPE_UTIL;
	m_iStartTime = 0;
	m_iEndTime = 0;
}

void UtilityScriptInfo::SetTimes( u64 start, u64 end )
{
	m_iStartTime = start;
	m_iEndTime = end;
}

bool UtilityScriptInfo::ShouldBeRunning(const ContextMap& rContextMap, const u64& posixTime, const StatFlags gameMode) const
{
	if(CommonScriptInfo::ShouldBeRunning(rContextMap, posixTime, gameMode))
	{
		// if too early
		if (m_iStartTime && m_iStartTime > posixTime )
		{
			return false;
		}
		// if too late
		if (m_iEndTime && m_iEndTime < posixTime )
		{
			return false;
		}

		return true;
	}

	return false;
}

// ================================================================================================================
// BackgroundScripts
// ================================================================================================================

u32 BackgroundScripts::m_uCloudHash = 0U;

BackgroundScripts::BackgroundScripts()
:	m_contextMap(atMapHashFn<u32>(),atMapEquals<u32>(), m_contextMemoryPool.m_Functor)
,	m_pScriptInfoHeapBuffer(NULL)
,	m_pScriptInfoAllocator(NULL)
,	m_pCurrentParsingScript(NULL)
,	m_bParsingCloud(false)
,	m_bHasCloudScripts(false)
,	m_nCloudRequestID(INVALID_CLOUD_REQUEST_ID)
,   m_nModifiedTime(0)
,	m_bIsInitialised(false)
,   m_bHasRequestedCloudScripts(false)
{
}

BackgroundScripts::~BackgroundScripts()
{
	bgScriptDebug1("~BackgroundScripts");
}

void BackgroundScripts::InitClass(unsigned initType)
{
	// only on core initialisation
	if(initType == INIT_CORE)
	{
		m_uCloudHash = atDataHash(BGSCRIPT_CLOUD_MOUNT_PATH, strlen(BGSCRIPT_CLOUD_MOUNT_PATH), 0U);
		m_uCloudHash = atDataHash(BG_CLOUD_FILE_NAME, strlen(BG_CLOUD_FILE_NAME), m_uCloudHash);
		m_uCloudHash = atDataHash(BGSCRIPT_XML_FILE_PATH, strlen(BGSCRIPT_XML_FILE_PATH), m_uCloudHash);
		m_uCloudHash = atDataHash(BGSCRIPT_RPF_FILE, strlen(BGSCRIPT_RPF_FILE), m_uCloudHash);

		Assertf(!SBackgroundScripts::IsInstantiated(), "BackgroundScripts::InitClass called more than once. This is leaking memory");
		SBackgroundScripts::Instantiate();
		SBackgroundScripts::GetInstance().AquireMemoryPools();
	}
	else if(initType == INIT_SESSION)
	{
		SBackgroundScripts::GetInstance().Init();
	}
}

void BackgroundScripts::UpdateClass()
{
	SBackgroundScripts::GetInstance().Update();
}

void BackgroundScripts::ShutdownClass(unsigned shutdownType)
{
	if(shutdownType == SHUTDOWN_SESSION)
	{
		SBackgroundScripts::GetInstance().Shutdown();
	}
	else if(shutdownType == SHUTDOWN_CORE)
	{
		SBackgroundScripts::GetInstance().ReleaseMemoryPools();
		SBackgroundScripts::Destroy();
	}
}

void BackgroundScripts::LoadResourcedSCFile(BGScriptInfo* pInfo)
{
	if(bgScriptVerify(pInfo) && pInfo->GetProgramName())
	{
		// We only want 1 of each program script
		bool bDupeFound = false;
		for(int i=0; i<m_aScriptInfos.GetCount(); ++i)
		{
			const BGScriptInfo* pCurrentInfo = m_aScriptInfos[i];
			bDupeFound |= ( pCurrentInfo!=pInfo && !strncmp(pCurrentInfo->GetProgramName(), pInfo->GetProgramName(), CommonScriptInfo::PROGRAM_NAME_SIZE) );
		}

		if(bgScriptVerifyf( !bDupeFound, "We only want 1 of each program script! dupe: %s", pInfo->GetProgramName()) )
		{
			bgScriptAssert(!pInfo->IsRunning());

			pInfo->RequestStreamedScript(m_pCurrentMountPath, m_bParsingCloud);
		}
		else
		{
			pInfo->SetFlag(BGScriptInfo::FLAG_FORCENORUN);
		}
	}
}

void BackgroundScripts::LoadDataFromPackFile(const char* pMountPath)
{
	m_pCurrentMountPath = pMountPath;

	bgScriptDisplayf("Loading in bgscripts.xml from mount path(%s)", pMountPath);

	INIT_PARSER;
	char buffer[50];
	formatf(buffer, sizeof(buffer), BGSCRIPT_XML_FILE_PATH, pMountPath);
	parStreamIn* streamIn = PARSER.OpenInputStream(buffer, "");

	if (Verifyf(streamIn, "Unable to open %s", buffer))
	{
		m_pCurrentParsingScript = NULL;

		streamIn->SetBeginElementCallback( parStreamIn::BeginElementCB(this, &BackgroundScripts::cbEnterElement) );
		streamIn->SetEndElementCallback( parStreamIn::EndElementCB	(this, &BackgroundScripts::cbLeaveElement) );
		streamIn->SetDataCallback ( parStreamIn::DataCB (this, &BackgroundScripts::cbHandleData) );
		streamIn->ReadWithCallbacks();
		streamIn->Close();
		delete streamIn;
	}
	bgScriptDisplayf("Initialised BGScript settings from XML" );

	m_pCurrentMountPath = NULL;
}

void BackgroundScripts::LoadData( const char* filename, const char* mountPath )
{
	bgScriptDebug1("Loading background scripts data from %s (cloud? %d)", filename, m_bParsingCloud);

	//Quick verification that the file we are trying to open exists.
	char fullname[RAGE_MAX_PATH];
	if( bgScriptVerifyf(ASSET.FullReadPath(fullname,sizeof(fullname),filename,""), "Unable to open %s", fullname) )
	{
		//Load the package that is already on disk
		fiPackfile* pLocalPackFile = rage_new fiPackfile;

		bool bDeletePackFile = true;
	
		if( bgScriptVerify(pLocalPackFile->Init(filename, true, fiPackfile::CACHE_NONE)) )
		{
			if ( bgScriptVerify(pLocalPackFile->MountAs(mountPath)))
			{
				LoadDataFromPackFile(mountPath);
	
				if(m_bParsingCloud)
				{
					fiDevice::Unmount(mountPath);
				}

				// Don't delete rpf if this was a successful DLC mount
				bDeletePackFile = m_bParsingCloud;
			}

			if(bDeletePackFile)
			{
				pLocalPackFile->Shutdown();
			}
		}

		if(bDeletePackFile)
		{
			delete pLocalPackFile;
		}
	}
}

void BackgroundScripts::Init()
{
	bgScriptDebug1("Init :: Initialised: %s", m_bIsInitialised ? "True" : "False");
	if(!bgScriptVerifyf(!m_bIsInitialised, "Init :: Already initialised!"))
		return;

	BANK_ONLY(GetDebug().Init();)
	SetGameMode(BGScriptInfo::FLAG_MODE_SP);

	BG_SCRIPTS_CRITICAL_SECTION;
	BG_SCRIPTS_RECENT_UPDATE_CRITICAL_SECTION;

#if __BANK
	if (PARAM_bgscriptswindow.Get())
		BANKMGR.CreateOutputWindow("bgscripts", "NamedColor:Black"); 
#endif

	m_bParsingCloud = false;

#if !__FINAL
	const char* testPackFile;
	if(PARAM_bgscriptsloadpack.Get(testPackFile))
	{
		char testPackPath[128];

		formatf(testPackPath, "%s%s", TEST_BGSCRIPT_PACK_PATH, testPackFile );
	
		bgScriptWarning("Loading test pack from %s", testPackPath);

		LoadData(testPackPath, BGSCRIPT_TEST_MOUNT_PATH);
	} 
#endif

    m_bHasRequestedCloudScripts = false;

	FixupScriptInfos();

	StartContext( 
#if __NO_OUTPUT 
		ATSTRINGHASH("GLOBAL",0xdb982272)
#else
		"GLOBAL"
#endif
		);

	m_bIsInitialised = true; 
}

void BackgroundScripts::FixupScriptInfos()
{
	for(int i=0; i<m_aScriptInfos.size(); ++i)
	{
		BGScriptInfo* pInfo = m_aScriptInfos[i];
		pInfo->Fixup();
	}
}

u32 BackgroundScripts::GetBGScriptCloudHash()
{
	return m_uCloudHash;
}

bool BackgroundScripts::RequestCloudFile()
{
    // mark when this is called once
    m_bHasRequestedCloudScripts = true;

#if !__FINAL
    if(PARAM_bgscriptsloadpack.Get())
    {
        bgScriptDisplayf("Ignore Request to cloud file due to bgscriptsloadpack");
        return false;
    }
	else if(PARAM_bgscriptsnoCloud.Get())
	{
		bgScriptDisplayf("Ignore Request to cloud file due to bgscriptsnoCloud");
		return false;
	}
#endif

	// if we're currently processing no request
	if(!CloudManager::GetInstance().IsRequestActive(m_nCloudRequestID))
	{
        const char* szCloudPath = NULL;
#if !__FINAL
        if(!PARAM_bgscriptscloudfile.Get(szCloudPath))
#endif
        {
			if(sysAppContent::IsJapaneseBuild())
	            szCloudPath = BG_CLOUD_FILE_NAME_JA;
			else
	            szCloudPath = BG_CLOUD_FILE_NAME;
        }

#if !__FINAL
		if(PARAM_bgscriptsusememberspace.Get())
		{
			// make cloud request
			bgScriptDebug1("Requesting cloud file from member space.");
			m_nCloudRequestID = CloudManager::GetInstance().RequestGetMemberFile(NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(), szCloudPath, BG_CLOUD_FILE_SIZE, eRequest_CacheAddAndEncrypt);	//	last param is bCache
		}
		else
#endif
		{
			unsigned nFlags = eRequest_AlwaysQueue | eRequest_CacheAddAndEncrypt | eRequest_CheckMemberSpace | eRequest_Critical;
#if !__FINAL
			if(!PARAM_bgscriptscloudfile.Get())
#endif
				 nFlags |= eRequest_UseBuildVersion;

			// make cloud request
			bgScriptDebug1("Requesting cloud file");
			m_nCloudRequestID = CloudManager::GetInstance().RequestGetTitleFile(szCloudPath, BG_CLOUD_FILE_SIZE, nFlags);
        }

		// request made
		return true;
	}

	// No request created
	return false;
}

void BackgroundScripts::OnCloudEvent(const CloudEvent* pEvent)
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_nCloudRequestID)
				return;

			// request complete
            m_nCloudRequestID = INVALID_CLOUD_REQUEST_ID; 

#if !__FINAL
            // simulate block - just bail early
            if(PARAM_bgscriptssimulateblock.Get())
                return;
#endif

            // success if we got something
			m_bHasCloudScripts = pEventData->bDidSucceed;
			
			// did we get the file...
			if(m_bHasCloudScripts)
			{
				bgScriptDebug1("Loading cloud bgscripts");

				BG_SCRIPTS_CRITICAL_SECTION;
				BG_SCRIPTS_RECENT_UPDATE_CRITICAL_SECTION;

				m_bParsingCloud = true;

				char tmpFileName[RAGE_MAX_PATH];
				fiDevice::MakeMemoryFileName(tmpFileName, sizeof(tmpFileName), pEventData->pData, pEventData->nDataSize, false, BGSCRIPT_RPF_FILE);

				//Load the data directly from the memory device we just downloaded from.
				LoadData(tmpFileName, BGSCRIPT_CLOUD_MOUNT_PATH);
				FixupScriptInfos();

				m_bParsingCloud = false;
                m_nModifiedTime = pEventData->nModifiedTime;

                GetEventScriptNetworkGroup()->Add(CEventNetworkCloudEvent(CEventNetworkCloudEvent::CLOUD_EVENT_BGSCRIPT_ADDED, 1));
			}
			else
			{
				// we treat a 404 as 'successful'
				m_bHasCloudScripts = (pEventData->nResultCode == NET_HTTPSTATUS_NOT_FOUND);
                m_nModifiedTime = 0;

				if (m_bHasCloudScripts)
				{
					bgScriptDebug1("Background scripts cloud data request complete with no file");
                    GetEventScriptNetworkGroup()->Add(CEventNetworkCloudEvent(CEventNetworkCloudEvent::CLOUD_EVENT_BGSCRIPT_ADDED, 0));
				}
#if !__NO_OUTPUT
				else
				{
					bgScriptWarning("Background scripts cloud request failed");
				}
#endif
			}
		}
		break;

	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		{
			// grab event data
			if(m_bIsInitialised)
			{
				const CloudEvent::sAvailabilityChangedEvent* pEventData = pEvent->GetAvailabilityChangedData();

				// if we now have cloud and don't have the cloud file, request it
				if(pEventData->bIsAvailable && !m_bHasCloudScripts)
				{
					bgScriptDebug1("Availability Changed - Requesting Cloud File");
					RequestCloudFile();
				}
			}
		}
		break;
	}
}

void BackgroundScripts::Shutdown()
{
	bgScriptDebug1("BackgroundScripts::Shutdown :: Initialised: %s", m_bIsInitialised ? "True" : "False");

	BG_SCRIPTS_CRITICAL_SECTION;
	BG_SCRIPTS_RECENT_UPDATE_CRITICAL_SECTION;

	//Stop all the running programs and tell them to release their memory.
	for( int i = 0; i < m_aScriptInfos.GetCount(); ++i )
	{
		m_aScriptInfos[i]->SetFlag(BGScriptInfo::FLAG_ONLYRUNONCE);	//< So "Stop" frees any related resources
		m_aScriptInfos[i]->Shutdown();
		m_aScriptInfos[i]->~BGScriptInfo();
		m_pScriptInfoAllocator->Free(m_aScriptInfos[i]);
	}

	m_aScriptInfos.Reset();
	m_bHasCloudScripts = false;
	m_bIsInitialised = false; 
}

void BackgroundScripts::Update()
{
    // wait 
    if(m_bIsInitialised && !m_bHasRequestedCloudScripts && NetworkInterface::HasValidRosCredentials())
        RequestCloudFile();

#if !__FINAL
	if(!PARAM_noBGScripts.Get())
#endif
	{
		const u64 posixTime = rlGetPosixTime();

		for(u32 updateIndex = 0; updateIndex < m_aScriptInfos.GetCount(); ++updateIndex)
		{
			m_aScriptInfos[updateIndex]->UpdateStatus(m_contextMap, posixTime, m_currentGameMode);
		}
	}
}

bool BackgroundScripts::HasCloudScripts()
{
#if !__FINAL
    // just return true here - otherwise we'll block MP
    if(PARAM_bgscriptsnoCloud.Get())
        return true;
#endif

    return m_bHasCloudScripts;
}

bool BackgroundScripts::IsCloudRequestPending()
{
	return CloudManager::GetInstance().IsRequestActive(m_nCloudRequestID);
}

#if __ASSERT
void BackgroundScripts::Validate() const
{
	for(int i=0; i<m_aScriptInfos.size(); ++i)
	{
		if(m_aScriptInfos[i]->GetIsAnyFlagSet(m_currentGameMode))
		{
			m_aScriptInfos[i]->Validate();
		}
	}
}
#endif

#if !__NO_OUTPUT
bool BackgroundScripts::StartContext( const char* contextName )
{
	const u32 contextHash = atStringHash(contextName);
	
	if( StartContext(contextHash) )
	{
		bgScriptDebug1("Starting context(%s, 0x%x).", contextName, contextHash);
		BANK_ONLY(GetDebug().AddContext(contextName););
		return true;
	}

	return false;
}
#endif	// !__NO_OUTPUT

bool BackgroundScripts::StartContext( u32 contextHash )
{
	bgScriptDebug1("Starting context(0x%x).", contextHash);

	if( m_contextMap.Access(contextHash) == NULL )
	{
		if(bgScriptVerifyf(m_contextMap.GetNumUsed() < MAX_BG_CONTEXT_COUNT, "Trying to start context(0x%x), but we're out of context slots.", contextHash))
		{
			Context context;
			m_contextMap.Insert(contextHash, context);
			return true;
		}
	}

	return false;
}

#if !__NO_OUTPUT
bool BackgroundScripts::EndContext( const char* contextName )
{
	u32 contextHash = atStringHash(contextName);
	bgScriptDebug1("Ending context(%s, 0x%x).", contextName, contextHash);
	return EndContext(contextHash);
}
#endif	// !__NO_OUTPUT

bool BackgroundScripts::EndContext( u32 contextHash )
{
	bgScriptDebug1("Ending context(0x%x).", contextHash);

	if(m_contextMap.Delete(contextHash))
	{
		return true;
	}

	bgScriptDebug1("Failed to end context(0x%x), because it wasn't running.", contextHash);
	return false;
}

void BackgroundScripts::EndAllContexts()
{
	bgScriptDebug1("Ending all contexts");
	m_contextMap.Reset();

	for( int i = 0; i < m_aScriptInfos.GetCount(); ++i )
	{
		m_aScriptInfos[i]->Stop();
	}
	
	// Global should always exist.
	StartContext( 
#if __NO_OUTPUT 
		ATSTRINGHASH("GLOBAL",0xdb982272)
#else
		"GLOBAL"
#endif
		);
}

BGScriptInfo* BackgroundScripts::GetScriptInfo(int scriptId)
{
	if(bgScriptVerifyf((u32)scriptId < (u32)m_aScriptInfos.size(), "BG Script Id=%d, Num BG Scripts=%d", scriptId, m_aScriptInfos.size()))
	{
		return m_aScriptInfos[scriptId];
	}

	return NULL;
}

BGScriptInfo* BackgroundScripts::GetScriptInfo(const char* scriptInfoName)
{
	return GetScriptInfoFromHash(atStringHash(scriptInfoName));
}

BGScriptInfo* BackgroundScripts::GetScriptInfoFromHash(u32 scriptNameHash)
{
	for(int i=0; i<m_aScriptInfos.size(); ++i)
	{
		if(m_aScriptInfos[i]->GetNameHash() == scriptNameHash)
		{
			return m_aScriptInfos[i];
		}
	}

	return NULL;
}

int BackgroundScripts::GetScriptIDFromHash(u32 scriptNameHash)
{
	int retVal = -1;
	for(int i=0; i<m_aScriptInfos.GetCount(); ++i)
	{
		if(m_aScriptInfos[i]->GetNameHash() == scriptNameHash)
		{
			retVal = i;
			break;
		}
	}

	return retVal;
}

static const char* s_xmlElementNames[] = 
{
	"Root",
	"Scripts",
	"Contexts",
	"Context",
	"ScriptInfo",
	"AssetInfo",
	"LaunchParams",
	"LaunchParam",
	"UtilityScripts",
	"Utility",
};

#if __BANK
void BackgroundScripts::DumpXMLError(parElement& OUTPUT_ONLY(elt))
{
	bgScriptError("Error with %s while in state %s", elt.GetName(), s_xmlElementNames[m_XmlStateStack.Top()]);
}
#endif

BGScriptInfo* BackgroundScripts::AddBGScriptInfo(ScriptType scriptType, u32 nameHash)
{
	BGScriptInfo* pRetVal = NULL;

#define ALLOCATE_BG_SCRIPT_INFO(type) \
	void* pBuffer = m_pScriptInfoAllocator->Allocate(sizeof(type), 4); \
	if(pBuffer) \
	{ \
		pRetVal = rage_placement_new(pBuffer) type(); \
	}

	//Create a new entry for this guy
	switch(scriptType)
	{
	case SCRIPT_TYPE_UTIL:
		{
			ALLOCATE_BG_SCRIPT_INFO(UtilityScriptInfo)
		}
		break;
	default:
		Assertf(0, "Trying to allocate an unknown script type(%d)", scriptType);
		break;
	}

	if(pRetVal)
	{
		m_aScriptInfos.Push(pRetVal);
		pRetVal->SetScriptId(m_aScriptInfos.size() - 1);
		pRetVal->SetNameHash(nameHash);
	}
	else
	{
		BANK_ONLY(GetDebug().PrintScriptCounts();)
		bgScriptError("Failed to allocate for a new BGScriptInfo of type %d. BGScriptInfo count = %d", scriptType, m_aScriptInfos.size());
	}

	return pRetVal;
}

void BackgroundScripts::ProcessScriptInfo(parElement& elt, ScriptType scriptType)
{
	const int cNameSize = 128;
	char szPTName[ cNameSize ] = {0};
	
	if( GetStringAttrib( szPTName, elt, "name" ) )
	{
		int nameLen = istrlen(szPTName);
		for(int i=0; i<nameLen; ++i)
		{
			if(szPTName[i] == ' ')
			{
				szPTName[i] = '_';
			}
		}

		u32 nameHash = atStringHash(szPTName);

		m_pCurrentParsingScript = NULL;
		for(int i=0; i<m_aScriptInfos.GetCount(); ++i)
		{
			if(nameHash == m_aScriptInfos[i]->GetNameHash())
			{
				bgScriptDebug1("Updating entry for %s", szPTName);
				m_pCurrentParsingScript = m_aScriptInfos[i];
			}
		}

		if(!m_pCurrentParsingScript)
		{
			bgScriptDebug1("Adding new entry for %s (0x%x %d)", szPTName, nameHash, nameHash);
			m_pCurrentParsingScript = AddBGScriptInfo(scriptType, nameHash);
		}

		if(!m_pCurrentParsingScript)
		{
			return;
		}

		if(m_pCurrentParsingScript->GetScriptType() != scriptType)
		{
			bgScriptError("It seems that %s already exists as a different type. The current type = %d. The new type = %d", szPTName, m_pCurrentParsingScript->GetScriptType(), scriptType);
			return;
		}

		m_pCurrentParsingScript->Init();
		m_pCurrentParsingScript->SetName( szPTName );

		if(m_pCurrentParsingScript->IsTypeUtility())
		{
			time_t rawtime = (time_t)rlGetPosixTime();

			tm *TM = localtime( &rawtime );
			bool gotTime = GetTimeDateAttrib( TM, elt, "startTime" );
			time_t start = mktime( TM );
	
			gotTime = gotTime && GetTimeDateAttrib( TM, elt, "endTime" );
			time_t end = mktime( TM );
	
			if( gotTime  )
			{
				bgScriptDebug1("Time activity information: %s : starts=%u, ends=%u", m_pCurrentParsingScript->GetName() ? m_pCurrentParsingScript->GetName():"", (unsigned int)start, (unsigned int)end);
				m_pCurrentParsingScript->SetTimes( (u64)start, (u64)end );
			}
			else
			{
				m_pCurrentParsingScript->SetTimes( 0, 0 );
			}
		}

		if(GetBoolAttrib(elt, "forceNoRun"))
		{
			m_pCurrentParsingScript->SetFlag(BGScriptInfo::FLAG_FORCENORUN);
		}

		if(GetBoolAttrib(elt, "onlyRunOnce"))
		{
			m_pCurrentParsingScript->SetFlag(BGScriptInfo::FLAG_ONLYRUNONCE);
		}

		if(GetBoolAttrib(elt, "needAllContexts"))
		{
			m_pCurrentParsingScript->SetFlag(BGScriptInfo::FLAG_NEED_ALL_CONTEXTS);
		}

		char gameModes[ 128 ] = {0};
		if( GetStringAttrib( gameModes, elt, "gameModes" ) )
		{
			m_pCurrentParsingScript->ClearFlag(BGScriptInfo::FLAG_MODE_ALL);

			if(stristr(gameModes, "MP"))
			{
				m_pCurrentParsingScript->SetFlag(BGScriptInfo::FLAG_MODE_MP);
			}
			if(stristr(gameModes, "SP"))
			{
				m_pCurrentParsingScript->SetFlag(BGScriptInfo::FLAG_MODE_SP);
			}
		}
		else
		{
			m_pCurrentParsingScript->SetFlag(BGScriptInfo::FLAG_MODE_ALL);
		}
	}
}

bool BackgroundScripts::ProcessScriptInfoData(parElement& elt)
{
	const char* elemName = elt.GetName();
	if(!stricmp( elemName, s_xmlElementNames[kState_Contexts])) 
	{ 
		m_XmlStateStack.Push(kState_Contexts);
		return true;
	}
	else if(!stricmp( elemName, s_xmlElementNames[kState_ScriptInfo])) 
	{ 
		m_XmlStateStack.Push(kState_ScriptInfo); 
		char szPTProgramName[ 128 ];
		if( GetStringAttrib(szPTProgramName, elt, "scr" ) )
		{
			m_pCurrentParsingScript->SetProgramName( szPTProgramName );
			LoadResourcedSCFile(m_pCurrentParsingScript);
		}
		return true;
	}
	else if(!stricmp( elemName, s_xmlElementNames[kState_AssetInfo])) 
	{ 
		m_XmlStateStack.Push(kState_AssetInfo);

		// parse category first (used in dictionary)
		char szCategory[64];
		if(!GetStringAttrib(szCategory, elt, "category" ))
		{
			strncpy(szCategory, "BG_CATEGORY_MISC", 64);
		}

		u32 categoryHash = atStringHash(szCategory);
		const CategoryString* pCategory = m_categories.Access(categoryHash);

		if(!pCategory)
		{
			m_categories.Insert(categoryHash, CategoryString(szCategory));
		}
		else
		{
			bgScriptAssertf((*pCategory) == szCategory, "This hash(%d) of string(%s) is already used by a different string(%s)", categoryHash, szCategory, pCategory->c_str());
		}
		m_pCurrentParsingScript->SetCategory(categoryHash);

		return true;
	}
	else if(!stricmp( elemName, s_xmlElementNames[kState_LaunchParams]))
	{
		m_XmlStateStack.Push(kState_LaunchParams);
		return true;
	}

	return false;
}

bool BackgroundScripts::ProcessScriptLaunchParam(parElement& elt)
{
	const char* elemName = elt.GetName();
	BGScriptInfo::LaunchParam launchParam;

	if(!stricmp( elemName, "ScoreParam"))
	{
		launchParam.m_header.isScore = true;
	}
	else if(!stricmp( elemName, "LaunchParam"))
	{
		launchParam.m_header.isScore = false;
	}
	else
	{
		bgScriptAssertf(0, "Unknown Param(%s).", elemName);
		return false;
	}

	m_XmlStateStack.Push(kState_LaunchParam);

	char name[50] = {0};
	if(GetStringAttrib(name, elt, "name" ))
	{
		launchParam.m_nameHash = atStringHash(name);
	}
	else
	{
		bgScriptAssertf(0, "%s missing the 'name' attribute.", elemName);
		return false;
	}

	char buff[50] = {0};
	if(GetStringAttrib(buff, elt, "compare" ))
	{
		bgScriptAssertf(launchParam.m_header.isScore, "%s isn't a score param, can't use the 'compare' field.", name);

		if(!stricmp(buff, "less_than"))
		{
			launchParam.m_header.compare = BGScriptInfo::LaunchParam::LESS_THAN;
		}
		else if(!stricmp(buff, "greater_than"))
		{
			launchParam.m_header.compare = BGScriptInfo::LaunchParam::GREATER_THAN;
		}
		else if(!stricmp(buff, "equal_to"))
		{
			launchParam.m_header.compare = BGScriptInfo::LaunchParam::EQUAL_TO;
		}
		else if(!stricmp(buff, "less_than_equal_to"))
		{
			launchParam.m_header.compare = BGScriptInfo::LaunchParam::LESS_THAN_EQUAL_TO;
		}
		else if(!stricmp(buff, "greater_than_equal_to"))
		{
			launchParam.m_header.compare = BGScriptInfo::LaunchParam::GREATER_THAN_EQUAL_TO;
		}
		else if(!stricmp(buff, "not_equal_to"))
		{
			launchParam.m_header.compare = BGScriptInfo::LaunchParam::NOT_EQUAL_TO;
		}
		else
		{
			bgScriptAssertf(0, "%s(%s) has a bad compare type(%s); needs to be 1 of the following, 'less_than', 'greater_than', 'equal_to', 'less_than_equal_to', 'greater_than_equal_to', or 'not_equal_to'.", elemName, name, buff);
		}
	}

	int typeCount = 0;

	if(GetIntAttrib(launchParam.m_iData, elt, "int" ))
	{
		launchParam.m_header.type = BGScriptInfo::LaunchParam::INT;
		typeCount++;
	}

	if( Unlikely(typeCount !=1) )
	{
		bgScriptAssertf(typeCount>=1, "%s(%s) missing params; needs 1 of the following: 'int'", elemName, name);
		bgScriptAssertf(typeCount<=1, "%s(%s) too many params; needs 1 of the following: 'int'", elemName, name);
		return false;
	}

	typeCount = 0;
	int awardValue=0;

	if(GetIntAttrib(awardValue, elt, "cash" ))
	{
		launchParam.m_iAward = awardValue;
		launchParam.m_header.award_type = BGScriptInfo::LaunchParam::CASH;
		typeCount++;
	}
	if(GetIntAttrib(awardValue, elt, "xp" ))
	{
		launchParam.m_iAward = awardValue;
		launchParam.m_header.award_type = BGScriptInfo::LaunchParam::XP;
		typeCount++;
	}

	if(typeCount <= 1)
	{
		m_pCurrentParsingScript->AddLaunchParam(launchParam);
		return true;
	}

	bgScriptAssertf(0, "%s(%s) too many reward params; needs 1 of the following, 'cash', or 'xp'.", elemName, name);
	return false;
}

void BackgroundScripts::cbEnterElement(parElement& elt, bool UNUSED_PARAM(isLeaf) )
{
	CompileTimeAssert(BackgroundScripts::kNumXmlStates == NELEM(s_xmlElementNames));
	
	const char*	elemName = elt.GetName();

	ePTXMLStates currentState = kState_Root;
	if( m_XmlStateStack.GetCount() > 0 )
		currentState = m_XmlStateStack.Top();

	bgScriptDebug3("Entering element <%s> while in state %s", elemName, s_xmlElementNames[currentState] );
	
	switch (currentState)
	{
	case kState_Root:
		if(!stricmp( elemName, s_xmlElementNames[kState_Scripts])) { m_XmlStateStack.Push(kState_Scripts); }
		else { BANK_ONLY(DumpXMLError(elt);) }
		break;
	case kState_Scripts:
		if(!stricmp( elemName, s_xmlElementNames[kState_UtilityScripts])) { m_XmlStateStack.Push(kState_UtilityScripts); }
		else { BANK_ONLY(DumpXMLError(elt);)  }
		break;
	case kState_Contexts:
		if(!stricmp( elemName, s_xmlElementNames[kState_Context])) 
		{ 
			m_XmlStateStack.Push(kState_Context);
		}
		break;
	case kState_LaunchParams:
		if(!ProcessScriptLaunchParam(elt))
		{
			BANK_ONLY(DumpXMLError(elt);)
		}
		break;
	case kState_UtilityScripts: 
		if(!stricmp( elemName, s_xmlElementNames[kState_Utility])) 
		{
			m_XmlStateStack.Push(kState_Utility);
			ProcessScriptInfo(elt, SCRIPT_TYPE_UTIL);
		}
		else { BANK_ONLY(DumpXMLError(elt);)  }
		break;
	case kState_Utility:
		if (! ProcessScriptInfoData(elt))
		{
			BANK_ONLY(DumpXMLError(elt);)
		}
		break;
	case kState_Context:
	case kState_ScriptInfo:
	case kState_AssetInfo: 
	case kState_LaunchParam:
	case kNumXmlStates:
		//NEVER ENTER THIS STATE
		bgScriptError("SHOULD NEVER GET NEW ELEMENT %s WHILE PROCESSING %s", s_xmlElementNames[currentState], elt.GetName());
		BANK_ONLY(DumpXMLError(elt));
		break;
	}
}

void BackgroundScripts::cbLeaveElement(bool UNUSED_PARAM(isLeaf))
{
	if (m_XmlStateStack.GetCount() > 0)
	{
		bgScriptDebug3("Leaving <%s>", s_xmlElementNames[m_XmlStateStack.Top()]);
		m_XmlStateStack.Pop();
	}
}

void BackgroundScripts::cbHandleData( char* data, int UNUSED_PARAM(size), bool UNUSED_PARAM(dataIncomplete) )
{	
	ePTXMLStates currentState = kState_Root;
	if( m_XmlStateStack.GetCount() > 0 )
	{
		currentState = m_XmlStateStack.Top();
	}

	switch(currentState)
	{
	case kState_Context:
		{
			bgScriptDebug2( "Adding context \"%s\" to BackgroundScripts %s", data, m_pCurrentParsingScript->GetName() );
			m_pCurrentParsingScript->AddContext( atStringHash( data ) );
			BANK_ONLY(GetDebug().AddContext(data);)
		}
		break;
	default:
		bgScriptError("No data handling in state %s", s_xmlElementNames[currentState]);
		break;
	}
}

template <size_t _Size>
bool BackgroundScripts::GetStringAttrib( char (&outString)[_Size], class parElement& elt, const char *reference )
{
	const parAttribute* attr = elt.FindAttributeAnyCase(reference);
	if( attr && attr->GetType() == parAttribute::STRING)
	{
		bgScriptAssertf(StringLength(attr->GetStringValue()) < _Size, "The max size for a attribute(%s) is %" SIZETFMT "d, and entered string(%s) is size of %d.", reference, _Size-1, attr->GetStringValue(), StringLength(attr->GetStringValue()));
		safecpy( outString, attr->GetStringValue() );
		return true;
	}
	
	outString[0] = 0;
	return false;
}

bool BackgroundScripts::GetIntAttrib( int &outInt, class parElement& elt, const char *reference )
{
	if( const parAttribute* attr = elt.FindAttributeAnyCase(reference) )
	{
		if (attr->GetType() == parAttribute::STRING)
		{
			const char* str = attr->GetStringValue();
			outInt = atoi(str);
		}
		else
		{
			outInt = attr->GetIntValue();
		}
		
		return true;
	}
	
	outInt = 0;
	return false;
}

bool BackgroundScripts::GetTimeDateAttrib( tm *outTM, class parElement& elt, const char *reference )
{
	if( const parAttribute* attr = elt.FindAttributeAnyCase(reference) )
	{
		if (attr->GetType() == parAttribute::STRING)
		{
			const char* str = attr->GetStringValue();
			const char *exampleString = "MM/DD/YYYY@HH:MM"; // exact length, force zero padding
			if( strlen( str ) == strlen( exampleString ) )
			{
				int month,day, year, hour, minute;
				sscanf(str, "%u/%u/%u@%u:%u", &month, &day, &year, &hour, &minute );
				outTM->tm_mon = month - 1;
				outTM->tm_mday = day;
				outTM->tm_year = year - 1900;
				outTM->tm_hour = hour;
				outTM->tm_min = minute;
				outTM->tm_sec = 0;
				return true;
			}
		}
	}

	return false;
}

bool BackgroundScripts::GetBoolAttrib( parElement& elt, const char *reference )
{
	const parAttribute* attr = elt.FindAttributeAnyCase(reference);
	if( attr && attr->GetType() == parAttribute::STRING )
	{
		const char* str = attr->GetStringValue();
		return (str && (str[0] == 't' || str[0] == 'T'));
	}

	return false;
}

void BackgroundScripts::AquireMemoryPools()
{
	bgScriptDebug1("BackgroundScripts::AquireMemoryPools");

	sysMemAllocator* pAllocator = sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_VIRTUAL);

	m_pScriptInfoHeapBuffer = pAllocator->RAGE_LOG_ALLOCATE_HEAP(BACKGROUNDSCRIPTS_SCRIPT_INFO_POOL_SIZE, 16, MEMTYPE_GAME_VIRTUAL);
	m_pScriptInfoAllocator = rage_new sysMemSimpleAllocator(m_pScriptInfoHeapBuffer, BACKGROUNDSCRIPTS_SCRIPT_INFO_POOL_SIZE, sysMemSimpleAllocator::HEAP_APP_1);
	m_pScriptInfoAllocator->SetQuitOnFail(false);

	m_contextMemoryPool.Create<ContextMapEntry>( MAX_BG_CONTEXT_COUNT );
	m_contextMap.Create(MAX_BG_CONTEXT_COUNT, false);
}

void BackgroundScripts::ReleaseMemoryPools()
{
	bgScriptDebug1("BackgroundScripts::ReleaseMemoryPools");

	m_contextMap.Kill();
	m_contextMemoryPool.Destroy();

	delete m_pScriptInfoAllocator;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_GAME_VIRTUAL)->Free(m_pScriptInfoHeapBuffer);
	m_pScriptInfoHeapBuffer = NULL;
	m_pScriptInfoAllocator = NULL;
}
