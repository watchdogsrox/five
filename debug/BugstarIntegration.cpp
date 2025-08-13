#include "debug/bugstar/BugstarIntegrationSwitch.h"

#if(BUGSTAR_INTEGRATION_ACTIVE)
// bugstar headers
#include "BugstarIntegration.h"
#include "debug/bugstar/MissionLookup.h"

// rage headers
#include "atl/guid.h"
#include "file/remote.h"
#include "parsercore/streamxml.h"
#include "string/stringutil.h"
#include "streaming/streamingallocator.h"
#include "system/bootmgr.h"
#include "system/exception.h"

// Game headers
#include "debug/Debug.h"
#include "camera/Viewports/Viewport.h"
#include "camera/Viewports/ViewportManager.h"
#include "debug/BlockView.h"
#include "peds/ped.h"
#include "peds/pedfactory.h"
#include "physics/physics.h"
#include "scene/world/gameworld.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclefactory.h"
#include "objects/DummyObject.h"
#include "parser/manager.h"						// XML
#include "parser/tree.h"
#include "system/exec.h"
#include "system/nelem.h"
#include "grcore/device.h"
#include "grcore/light.h"
#include "grcore/stateblock.h"
#include "qa/bugstarinterface.h"

#define BUGSTAR_IP_ADDRESS	"rest.bugstar.rockstargames.com:8443"

#define BUGSTAR_DEFAULT_ART_TEAM		"*Default Art*"
#define BUGSTAR_DEFAULT_AUDIO_TEAM		"*Default Audio Code*"
#define BUGSTAR_DEFAULT_LEVELS_TEAM		"*Default Levels*"
#define BUGSTAR_DEFAULT_AI_CODE_TEAM	"*Default AI Code*"
#define BUGSTAR_DEFAULT_CODE_TEAM		"*Default Code*"

// Full list at:-
// https://rsgedibgs1.rockstar.t2.corp:8443/BugstarRestService-1.0/rest/Projects/1546/States 
// To add new ones, add a new member
CBugstarIntegration::StatusFilter	CBugstarIntegration::m_StatusFilters[] =
{
	{"Dev (Fixing)",					true },
	{"Dev (Fixed, Awaiting Build)",		true },
	{"Test (Verifying)",				true },
	{"Test (Not Seen)",					true }
};

// Fill List at:-
// https://rsgedibgs8.rockstar.t2.corp:8443/BugstarRestService-1.0/rest/Projects/1546/Components
// (as a tree)
// To add new ones, add a new member
CBugstarIntegration::ComponentFilter	CBugstarIntegration::m_ComponentFilters[] =
{
	{"Animation",						true },
	{"Audio",							true },
	{"Art",								true },
	{"Art : Collision",					true },	// Art ones copied from an email, if they're miss-spelled it'll go wrong
	{"Art : General",					true },
	{"Art : Geometry",					true },	
	{"Art : Interior",					true },	
	{"Art : Legal",						true },	
	{"Art : Lighting",					true },	
	{"Art : LOD",						true },	
	{"Art : Maps : Structures",			true },	
	{"Art : Maps : Terrain",			true },	
	{"Art : Positioning",				true },	
	{"Art : Props",						true },	
	{"Art : Seam",						true },	
	{"Art : Texture",					true },	
	{"Art : Timeline",					true },	
	{"Art : VFX",						true },	
	{"Art : Z-fighting",				true },	
	{"Character",						true },
	{"Code",							true },
	{"Interface",						true },
	{"Road",							true },
	{"Script",							true },
	{"Slowdown",						true },
	{"Social Club",						true },
	{"Streaming",						true },
	{"Text",							true },
	{"Tools",							true },
	{"TRC",								true },
	{"Vehicles",						true }
};

XPARAM(bugstarusername);

PARAM(SetBugstarProduct, "Force use of specified bugstar database");
PARAM(attachConsoleLogsToBugsInMP, "Attach console log file(s) in MP");
PARAM(forceRenewBugstarRequests, "Forces retrieving requests from bugstar rather than using cached files");
PARAM(bugstarStreamingVideo, "[bugstar] Initialize bugstar console streaming video. If a parameter is specified it uses that as the url rather than the default");

bool			CBugstarIntegration::m_FilterByCurrentUser = false;
atHashString	CBugstarIntegration::m_CurrentUsername;
s32				CBugstarIntegration::m_CurrentUserID = 0;

char	CBugstarIntegration::m_sAreaCenter[MAX_COORDINATE_STRING_LENGTH];
float	CBugstarIntegration::m_fAreaSize = 50.0;
float	CBugstarIntegration::m_fBugMarkerSize = 1.0;
bool	CBugstarIntegration::m_bBugMarkersOn = true;
bool	CBugstarIntegration::m_bBugClassFilters[CMinimalBug::kBugCategoryNum] = { true, true, true, true, true, true, true };
char	CBugstarIntegration::m_sFetchBugNumber[BUG_NUMBER_STRING_LENGTH];
char	CBugstarIntegration::m_sUsername[BUG_LOGIN_STRING_LENGTH]="gamestats";
char	CBugstarIntegration::m_sPassword[BUG_LOGIN_STRING_LENGTH]="game5tats";
char	CBugstarIntegration::m_sAssertUsername[BUG_LOGIN_STRING_LENGTH]="gameasserts";
char	CBugstarIntegration::m_sAssertPassword[BUG_LOGIN_STRING_LENGTH]="gamea$$ert5";
char	CBugstarIntegration::m_sUsernameVis[BUG_LOGIN_STRING_LENGTH];
char	CBugstarIntegration::m_sPasswordVis[BUG_LOGIN_STRING_LENGTH];
bool	CBugstarIntegration::m_bProcessScreenshot = false;
int		CBugstarIntegration::m_iScreenshotFrameDelay = 0;

//bug display
int		CBugstarIntegration::m_iSelectedBugIndex = 0;
char	CBugstarIntegration::m_sBugNumber[BUG_NUMBER_STRING_LENGTH];
char	CBugstarIntegration::m_sBugSummary[BUG_SUMMARY_STRING_LENGTH];

int		CBugstarIntegration::sm_currentUserId = -1;
atMap<atHashString, int> CBugstarIntegration::sm_teamIdMap;

atArray<CMinimalBug> CBugstarIntegration::m_Buglist;
atArray<CMinimalBug*> CBugstarIntegration::m_FilteredBugList;
CSavedBugstarRequestList CBugstarIntegration::m_savedBugstarRequestList;
CBugstarAssetLookup CBugstarIntegration::sm_buildLookup;
CBugstarAssetLookup CBugstarIntegration::sm_mapLookup;
char CBugstarIntegration::sm_streamingVideoId[128];
char CBugstarIntegration::sm_streamingVideoBaseUri[128];
char const* CBugstarIntegration::sm_streamingVideoRootDirectory = "x:\\BugstarStreamingVideo";
char const* CBugstarIntegration::sm_streamingVideoErrorMessage = nullptr;
bool CBugstarIntegration::sm_isStreamingVideoEnabled = false;

sysMessageQueue<CBugstarAssertUpdateQuery*, MAX_ASSERT_QUERIES> CBugstarAssertUpdateQuery::sm_messageQueue;
atMap<int, bool> CBugstarAssertUpdateQuery::sm_assertIdAlreadyAdded;
bool CBugstarAssertUpdateQuery::sm_BugstarWorking = true;

#define BUGSTAR_REST_SERVER     "https://rest.bugstar.rockstargames.com:8443/BugstarRestService-1.0/rest/"
#define BUGSTAR_LOG_DIRECTORY   "N:\\RSGEDI\\GameLogs\\GTA V\\"

static const char* const gDxDiagLogDirectory = "\\\\rockstar.t2.corp\\Network\\RSGEDI\\GameLogs\\GTA V\\";
static const char* const gDxDiagLogURL = "file://rockstar.t2.corp/Network/RSGEDI/GameLogs/GTA%20V/";

//Reduced from 60, use a lower timeout to avoid holding up queued http requests for too long
const static unsigned s_BugstarRequestDefaultTimeout = 30;

// ----- CSavedBugstarRequestList --------------------------------------------------------------

// I've given up trying to make this accurate as it doesnt matter that much
int GetTimeDifferenceInMinutes(fiDevice::SystemTime time1, fiDevice::SystemTime time2)
{
	u32 minutes1 = time1.wMinute+(time1.wHour+(time1.wDay+(time1.wMonth+(time1.wYear-2005)*12)*31)*24)*60;
	u32 minutes2 = time2.wMinute+(time2.wHour+(time2.wDay+(time2.wMonth+(time2.wYear-2005)*12)*31)*24)*60;

	return minutes2 - minutes1;
}

static void GetLocalFilename(const char* pFilename, char fullFilename[RAGE_MAX_PATH])
{
	// save file to local machine
	sprintf(fullFilename, "common:/data/debug/%s", pFilename);
}
#define DATETEST_FILENAME "datetest.txt"

void CSavedBugstarRequestList::Init()
{
	// given up trying to compare system time and file time. Now I'm going to create a file on the same filesystem and compare times
	if (!sysBootManager::IsPackagedBuild())
	{
		char filename[RAGE_MAX_PATH];
		GetLocalFilename(DATETEST_FILENAME, filename);
		fiStream* pStream = ASSET.Create(filename, "");
		if (pStream)
			pStream->Close();
	}
}

parTree* CSavedBugstarRequestList::LoadBugstarRequest(const char* pLocalFilename)
{
	char filename[RAGE_MAX_PATH];
	GetLocalFilename(pLocalFilename, filename);

	fiStream* pStream = ASSET.Open(filename, "");
	parTree* pTree = NULL;
	if(pStream)
	{
		pTree = PARSER.LoadTree(pStream);
		pStream->Close();
	}

	return pTree;
}

void CSavedBugstarRequestList::Get(const char* pLocalFilename, const char* pRequest, int renewTimeInMinutes, int bufferSize)
{
	char filename[RAGE_MAX_PATH];
	char datetestFilename[RAGE_MAX_PATH];
	GetLocalFilename(pLocalFilename, filename);
	GetLocalFilename(DATETEST_FILENAME, datetestFilename);

	// check filetimes to see if we need to renew this load
	const fiDevice* pDevice = fiDevice::GetDevice(filename);

	if (!pDevice)
	{
		// Can't get a device for this, so the file won't exist either - let's forget about it.
		return;
	}

	u64 fileTime64 = pDevice->GetFileTime(filename);
	u64 dateTestFileTime64 = pDevice->GetFileTime(datetestFilename);
	u64 diff = dateTestFileTime64 - fileTime64;
	
	// convert to minutes
	diff /= 600000000LL;

/*	fiDevice::SystemTime fileTime;
	fiDevice::SystemTime systemTime;

	fiDevice::ConvertFileTimeToSystemTime(fileTime64, fileTime);
	// Horrible cludge to get round the fact PS3 return UTC based filetimes 
#if __PS3
	CellRtcTick utcTick;
	CellRtcTick localTick;
	utcTick.tick = fileTime64 / 10LL;	// divide by 10 to convert from win32 time to cell ticks

	cellRtcConvertUtcToLocalTime(&utcTick, &localTick);
	fileTime64 = localTick.tick * 10LL;
#endif
	fiDevice::ConvertFileTimeToSystemTime(fileTime64, fileTime);
	fiDevice::GetLocalSystemTime(systemTime);
	int diff = GetTimeDifferenceInMinutes(fileTime, systemTime);*/

	if(renewTimeInMinutes < diff
        || PARAM_forceRenewBugstarRequests.Get())
	{
		Displayf("File %s is %d minutes old , so resubmitting", filename, (int)diff);
		CHttpQuerySaveToFile* pQuery = CBugstarIntegration::CreateSaveHttpQuery(filename);
		pQuery->Init(pRequest, s_BugstarRequestDefaultTimeout, bufferSize);
	}
}

void CSavedBugstarRequestList::PostXml(const char* pLocalFilename, const char* pRequest, const char* pXml, int xmlLength, int renewTimeInMinutes, int bufferSize)
{
	char filename[RAGE_MAX_PATH];
	GetLocalFilename(pLocalFilename, filename);

	// check filetimes to see if we need to renew this load
	const fiDevice* pDevice = fiDevice::GetDevice(filename);
	u64 fileTime64 = pDevice->GetFileTime(filename);
	fiDevice::SystemTime fileTime;
	fiDevice::SystemTime systemTime;
	fiDevice::ConvertFileTimeToSystemTime(fileTime64, fileTime);
	fiDevice::GetLocalSystemTime(systemTime);

	int diff = GetTimeDifferenceInMinutes(fileTime, systemTime);

	if(renewTimeInMinutes < diff
        || PARAM_forceRenewBugstarRequests.Get())
	{
		Displayf("File %s is %d minutes old , so resubmitting", filename, diff);
		CHttpQuerySaveToFile* pQuery = CBugstarIntegration::CreateSaveHttpQuery(filename);
		pQuery->InitPost(pRequest, s_BugstarRequestDefaultTimeout, bufferSize);
		pQuery->AppendXmlContent(pXml, xmlLength);
		pQuery->CommitPost();
	}
}

// ----- CBugstarAssertUpdateQuery -------------------------------------------------------------

CBugstarAssertUpdateQuery::CBugstarAssertUpdateQuery(const char* errorMsg, const char* detailedErrorMsg, const char* callstack, const char* gamerTag, const BugDesc& bug, bool showBug)
	: m_errorMsg(errorMsg), 
	m_detailedErrorMsg(detailedErrorMsg), 
	m_callstack(callstack), 
	m_gamerTag(gamerTag),
	m_bugDesc(bug),
	m_showBug(showBug),
	m_pNextAssert(NULL)
{
}

CBugstarAssertUpdateQuery::~CBugstarAssertUpdateQuery()
{
	if(m_bugDesc.logBuffer)
		delete[] m_bugDesc.logBuffer;
	if(m_pNextAssert)
		delete m_pNextAssert;

#if __WIN32PC
	if(m_bugDesc.dxDiagBuffer)
		delete[] m_bugDesc.dxDiagBuffer;
#endif
}

void CBugstarAssertUpdateQuery::AddQueryToQueue()
{
	sm_messageQueue.Push(this);
}

void CBugstarAssertUpdateQuery::CreateBugDescription(char* pBuffer, int bufferSize, const char* logName)
{
	if(m_pNextAssert == NULL)
	{
		if(NetworkInterface::IsGameInProgress())
		{
			formatf(pBuffer, bufferSize, "GamerTag: %s\n\nLog: %s\nSteps:\n\nOutcome:\n%s\n%s", m_gamerTag.c_str(), logName, m_detailedErrorMsg.c_str(), m_callstack.c_str());
		}
		else
		{
			formatf(pBuffer, bufferSize, "Log: %s\nSteps:\n\nOutcome:\n%s\n%s", logName, m_detailedErrorMsg.c_str(), m_callstack.c_str());
		}
	}
	else
	{
		if(NetworkInterface::IsGameInProgress())
		{
			formatf(pBuffer, bufferSize, "GamerTag: %s\n\nLog: %s\nSteps:\n\nOutcome:\nMultiple Asserts\n%s\n%s", m_gamerTag.c_str(), logName, m_detailedErrorMsg.c_str(), m_callstack.c_str());
		}
		else
		{
			formatf(pBuffer, bufferSize, "Log: %s\nSteps:\n\nOutcome:\nMultiple Asserts\n%s\n%s", logName, m_detailedErrorMsg.c_str(), m_callstack.c_str());
		}
		
		CBugstarAssertUpdateQuery* pQuery = m_pNextAssert;
		while(pQuery)
		{
			// add additional callstacks
			safecatf(pBuffer, bufferSize, "\n%s\n%s", pQuery->m_detailedErrorMsg.c_str(), pQuery->m_callstack.c_str());
			pQuery = pQuery->m_pNextAssert;
		}
	}
}

int CBugstarAssertUpdateQuery::AddBug()
{
	// Construct description
	char logComment[128]="";
	char bugDescription[8192];
	CreateBugDescription(bugDescription, sizeof(bugDescription), logComment);

	// No bugs exist yet, create a new one
	Displayf("Adding bug %d", m_bugDesc.assertId);
	int bugId = CBugstarIntegration::CreateNewAssertBug(m_errorMsg, bugDescription, m_bugDesc);

	GetSubsequentBugs();

	// The last bug in the list is the one we take the log from
	CBugstarAssertUpdateQuery* pLogQuery = this;
	while(pLogQuery->m_pNextAssert)
	{
		pLogQuery = pLogQuery->m_pNextAssert;
	}

	if(bugId != 0) 
	{
		bool rebuildBugDescription = false;
		if(pLogQuery->m_bugDesc.logBuffer)
		{
			char logFilename[128];
			// saving log file to network
			formatf(logFilename, sizeof(logFilename), "%s%d/assert.log", BUGSTAR_LOG_DIRECTORY, bugId);
			// commented out until we can get the path into the bug easily
			ASSET.CreateLeadingPath(logFilename);
			fiStream* pStream = ASSET.Create(logFilename, "");
			if(pStream)
			{
				pStream->Write(pLogQuery->m_bugDesc.logBuffer, pLogQuery->m_bugDesc.logSize);
				pStream->Close();

				formatf(logComment, sizeof(logComment), "\"%s\"", logFilename);
				rebuildBugDescription = true;
			}
		}

		// if multiple asserts
		if(m_pNextAssert)
			rebuildBugDescription = true;
		
		if(rebuildBugDescription)
		{
			CreateBugDescription(bugDescription, sizeof(bugDescription), logComment);
			CBugstarIntegration::ChangeField(bugId, "description", bugDescription);
		}
		
	}


#if __WIN32PC
	if (bugId != 0 && m_bugDesc.dxDiagBuffer)
	{
		if (WriteDxDiagLog(bugId, 1))
		{
			char logComment[256];

			Assert(strlen(gDxDiagLogDirectory) + strlen(gDxDiagLogURL) + 21 < 256); // +20 for 2 * digits in MAX_INT32, +1 for \0

			sprintf(logComment, "DxDiag logs are in: \"%s%d\"\n%s%d", gDxDiagLogDirectory, bugId, gDxDiagLogURL, bugId);

			CBugstarIntegration::AddComment(bugId, logComment);
		}
	}
#endif
	return bugId;
}

void CBugstarAssertUpdateQuery::Execute()
{
	bool bAlreadyAdded = false;
	int bugId = 0;
	int bugCount = 0;

	// If the game fails to connect to bugstar it stops adding bugs
	if(!sm_BugstarWorking)
		return;

	USE_DEBUG_MEMORY();

	// If already added flag as already added and don't try and create a new bug
	if(sm_assertIdAlreadyAdded.Access(m_bugDesc.assertId))
	{
		Displayf("bug %d already added", m_bugDesc.assertId);
		bAlreadyAdded = true;
	}
	// set as already added
	sm_assertIdAlreadyAdded.Insert(m_bugDesc.assertId, true);

	// only get bug count if we havent already added it
	if(!bAlreadyAdded)
		bugCount = CBugstarIntegration::GetBugCountForAssertId(m_bugDesc.assertId);

	// bug count of -1 indicates an error
	if(bugCount == -1)
	{
		fiRemoteShowMessageBox("Assert Bot failed to communicate with bugstar, disabling the assert bot", sysParam::GetProgramName(), MB_OK, 0);
		sm_BugstarWorking = false;
		return;
	}
	else if (bugCount == 0 && !bAlreadyAdded)
	{
		bugId = AddBug();
		if(bugId == 0)
		{
			fiRemoteShowMessageBox("Assert Bot failed to communicate with bugstar, disabling the assert bot", sysParam::GetProgramName(), MB_OK, 0);
			sm_BugstarWorking = false;
		}
	}
	else
	{
		// Query bugstar for a list of the bugs that exist and check if there is one that isn't closed
		rage::parTree* openBugTree = CBugstarIntegration::GetMostRecentOpenBug(m_bugDesc.assertId);

		// clear out subsequent bugs so we aren't creating bugs for asserts we didnt before
		GetSubsequentBugs();

		if (openBugTree != NULL)
		{
			CMinimalBug bug;
			CBugstarIntegration::ConvertToMinimalBug(openBugTree, bug);
			bugId = bug.Number();

			// Open bug exists, increment its Seen count
#if __WIN32PC
			int count = 
#endif
				CBugstarIntegration::IncrementBugSeenCount(bugId, m_bugDesc.buildId, openBugTree);

#if __WIN32PC
			WriteDxDiagLog(bugId, count);
#endif

			delete openBugTree;
		}
		else if(!bAlreadyAdded)
		{
			bugId = AddBug();
		}
	}

	// Check whether the user wishes to see the bug
	if (bugId != 0 && m_showBug)
	{
		qaBug bug;
		bug.SetID(bugId);

		qaBugstarInterface BUGSTAR;
		BUGSTAR.OpenBugForEdit(bug);
	}
}

void CBugstarAssertUpdateQuery::GetSubsequentBugs()
{
	CBugstarAssertUpdateQuery* pQuery = this;
	CBugstarAssertUpdateQuery* pNextQuery;
	while(sm_messageQueue.GetTail(pNextQuery))
	{
		if(pNextQuery->m_bugDesc.timeSinceLastBug >= 0.034f)
			break;
		pNextQuery = sm_messageQueue.Pop();
		pQuery->SetNextAssert(pNextQuery);
		pQuery = pNextQuery;
		// If this bug has been set to shown then set parent bug to be shown
		if(pQuery->m_showBug)
			m_showBug = true;
	}
}

#if __WIN32PC
bool CBugstarAssertUpdateQuery::WriteDxDiagLog(int bugId, int seen)
{
	if (!m_bugDesc.dxDiagBuffer)
		return false;

	char logFilename[RAGE_MAX_PATH];
	sprintf(logFilename, "%s%d/dxdiag-%04d.log", gDxDiagLogDirectory, bugId, seen);
	ASSET.CreateLeadingPath(logFilename);
	fiStream* pStream = ASSET.Create(logFilename, "");
	if(pStream)
	{
		pStream->Write(m_bugDesc.dxDiagBuffer, m_bugDesc.dxDiagSize);
		pStream->Close();
		return true;
	}

	return false;
}
#endif

void CBugstarAssertUpdateQuery::InitAssertQueryQueue()
{
	sysIpcCreateThread(&BugstarAssertUpdateThread, NULL, 40*1024, PRIO_NORMAL, "BugstarAssertLogging" );
}

void CBugstarAssertUpdateQuery::BugstarAssertUpdateThread(void*)
{
	CBugstarAssertUpdateQuery* pQuery;
	while( true )
	{
		pQuery = sm_messageQueue.Pop();
		pQuery->Execute();
		delete pQuery;
	}
}


// ----- CBugstarIntegration -------------------------------------------------------------
//bool CreateGameData(const char *pGameName, const char* pGameVersion);

void CBugstarIntegration::Init( )
{


	const char* pSetDbase = NULL;
	PARAM_SetBugstarProduct.Get(pSetDbase);

	if (pSetDbase)
	{
		qaBugstarInterface::SetProduct(pSetDbase);
	}
	else
	{
		qaBugstarInterface::SetProduct("GTA 5 DLC");
	}


	{
		USE_DEBUG_MEMORY();

		sysGetEnv("USERNAME",m_sUsernameVis,sizeof(m_sUsernameVis));
		strcpy(m_sPasswordVis,"");
	}

	fwHttpQuery::InitHttpQueryQueue();
	CBugstarAssertUpdateQuery::InitAssertQueryQueue();

	CSavedBugstarRequestList().Init();
	CMissionLookup::Init();
	sm_mapLookup.Init(BUGSTAR_GTA_PROJECT_PATH "Maps", "maps.xml", "name");
	sm_buildLookup.Init(BUGSTAR_GTA_PROJECT_PATH "Builds", "builds.xml", "buildId");

	// Get the current username
	const char* pUserName=NULL;
	if(PARAM_bugstarusername.Get(pUserName))
	{
		m_CurrentUsername = pUserName;
	}
	else
	{
		char userName[64];
		sysGetEnv("USERNAME",userName,64);
		m_CurrentUsername = userName;
	}

	// Nonsense just to ensure that GetRockstarTargetManagerConfig is never dead stripped
	if ((const char*(*)(void))pUserName == GetRockstarTargetManagerConfig)
	{
		m_CurrentUsername = (char*)GetRockstarTargetManagerConfig;
	}

	if (PARAM_bugstarStreamingVideo.Get())
	{
		InitStreamingVideo();
	}

#if EXCEPTION_HANDLING && SYSTMCMD_ENABLE
	sysException::SetBugstarConfig(GetRockstarTargetManagerConfig());
#endif
}

void CBugstarIntegration::Shutdown( )
{
	CBugstarComponentLookup::Shutdown();
	CBugstarStateLookup::Shutdown();
	CBugstarUserLookup::Shutdown();
}

// Data for Rockstar Target Manager.
const char* CBugstarIntegration::GetRockstarTargetManagerConfig()
{
#	define INCLUDE_ROOT_DIR             (RSG_DURANGO || RSG_XENON)
#	if INCLUDE_ROOT_DIR
#		define INCLUDE_ROOT_DIR_ONLY(...)   __VA_ARGS__
#	else
#		define INCLUDE_ROOT_DIR_ONLY(...)
#	endif
	static const char fmt[] =
		"<bugstar_config>"
			"<rest_server>" BUGSTAR_REST_SERVER "</rest_server>"
			"<log_directory>" BUGSTAR_LOG_DIRECTORY "</log_directory>"
			"<project_id>" BUGSTAR_GTA_PROJECT_ID "</project_id>"
			"<build_id>%s</build_id>"
			INCLUDE_ROOT_DIR_ONLY("<root_dir>%s</root_dir>")
		"</bugstar_config>";
	static char xml[sizeof(fmt)+128];
	snprintf(xml, sizeof(xml), fmt, CDebug::GetVersionNumber()
		INCLUDE_ROOT_DIR_ONLY(,CFileMgr::GetRootDirectory()));
	Assert(strlen(xml) < sizeof(xml));
	return xml;
#	undef INCLUDE_ROOT_DIR_ONLY
#	undef INCLUDE_ROOT_DIR
}

void CBugstarIntegration::ProcessScreenShot()
{
	// Process screenshot requests
	if (!m_bProcessScreenshot)
		return;

	if (m_iScreenshotFrameDelay > 0)
	{
		m_iScreenshotFrameDelay--;
		return;
	}

	atArray<CMinimalBug*>& list = GetFilteredBugList();
	CMinimalBug* bug = list[m_iSelectedBugIndex];
	const char *shotName = "X:/lastBug.jpg";
#if __D3D11
	GRCDEVICE.LockContext();
#endif // __D3D11
	GRCDEVICE.CaptureScreenShotToJpegFile(shotName);
#if __D3D11
	GRCDEVICE.UnlockContext();
#endif // __D3D11
	qaBugstarInterface BUGSTAR;
	qaBug BUG;
	BUGSTAR.CreateBug(BUG,shotName,bug->Number());

	m_bProcessScreenshot = false;
}



atArray<CMinimalBug>& CBugstarIntegration::GetCurrentBugList()
{
	return m_Buglist;
}

atArray<CMinimalBug*>& CBugstarIntegration::GetFilteredBugList()
{
	return m_FilteredBugList;
}


#if __BANK

char s_DebugAssertMsg[128] = "";
int s_DebugAssertTimes = 1;

#if RSG_ORBIS
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

void TestAssertBot()
{
	if(s_DebugAssertTimes > 1)
	{
		int length = istrlen(s_DebugAssertMsg);
		for(int i=0; i<s_DebugAssertTimes; i++)
		{
			sprintf(s_DebugAssertMsg+length, "%d", i+1);
			// AF: Don't change this back to "%s", s_DebugAssertMsg as it breaks my assert bot testing as fmt keeps coming thru as "%s"
			diagLogf(Channel_debug, DIAG_SEVERITY_ASSERT, __FILE__, __LINE__, s_DebugAssertMsg);
		}
	}
	else
	{
		// AF: Don't change this back to "%s", s_DebugAssertMsg as it breaks my assert bot testing as fmt keeps coming thru as "%s"
		diagLogf(Channel_debug, DIAG_SEVERITY_ASSERT, __FILE__, __LINE__, s_DebugAssertMsg);
	}
};

#if RSG_ORBIS
#pragma GCC diagnostic error "-Wformat-security"
#endif

void CBugstarIntegration::InitWidgets(bkBank& bank)
{
	bank.PushGroup( "Login" );
	bank.AddText( "Username", m_sUsernameVis, NELEM(m_sUsernameVis), false );
	bank.AddText( "Password", m_sPasswordVis, NELEM(m_sPasswordVis), false );
	bank.AddButton( "Login", datCallback(CFA(LoginAndClear)) );
	bank.PopGroup( );

	bank.PushGroup( "Assert Bot" );
	bank.AddText( "Message", s_DebugAssertMsg, sizeof(s_DebugAssertMsg), false );
	bank.AddSlider( "Times", &s_DebugAssertTimes, 1, 5, 1 );
	bank.AddButton( "Assert", datCallback(CFA(TestAssertBot)) );
	bank.PopGroup( );

	bank.PushGroup( "List Bugs In Area" );
	bank.AddButton( "Load Open BugList", datCallback(CFA(LoadDevBugList)) );
	bank.AddText( "Current Area Center", m_sAreaCenter, NELEM(m_sAreaCenter), true );
	bank.AddSlider( "Area Size", &m_fAreaSize, 0,MAX_BUG_AREA_SIZE,1.0f );
	bank.PopGroup( );

	bank.PushGroup( "Get Bug by Number" );
	bank.AddText("Enter Bug #",m_sFetchBugNumber,NELEM(m_sFetchBugNumber),false);
	bank.AddButton("Fetch",datCallback(CFA(LoadBugNumber)));
	bank.PopGroup( );

	bank.PushGroup( "Bugs",false );
	bank.AddButton( "Next Bug", datCallback(CFA(NextBug)) );
	bank.AddButton( "Previous Bug", datCallback(CFA(PreviousBug)) );
	bank.AddButton( "Nearest Bug", datCallback(CFA(SelectNearestBug)) );

	bank.AddButton( "Warp to Bug", datCallback(CFA(WarpToSelectedBug)) );
	bank.AddText( "Bug #", m_sBugNumber, NELEM(m_sBugNumber), true, datCallback(CFA(CBugstarIntegration::UpdateSelectedBugDetails)) );
	bank.AddText( "", m_sBugSummary, NELEM(m_sBugSummary), true, datCallback(CFA(CBugstarIntegration::UpdateSelectedBugDetails)) );
	bank.PopGroup( );

	bank.PushGroup( "Edit Bug",false );
	bank.AddButton( "Add Screenshot", datCallback(CFA(AddScreenshot)) );
	bank.PopGroup( );

	bank.PushGroup( "Settings" ,false);	
	bank.AddSlider( "Set Bug Marker Size:", &m_fBugMarkerSize, 0,MAX_BUG_MARKER_SIZE,0.1f );
	bank.AddToggle( "Show Bug Markers",&m_bBugMarkersOn);
	bank.AddToggle( "Don't Attach Screenshots To Space Bar Bugs", &CDebug::ms_disableSpaceBugScreenshots);
	bank.AddSeparator();
	bank.AddTitle( "Filter Bug Classes" );
	bank.AddToggle( "A",&m_bBugClassFilters[CMinimalBug::kA], datCallback(CFA(UpdateFilteredBugList)));
	bank.AddToggle( "B",&m_bBugClassFilters[CMinimalBug::kB], datCallback(CFA(UpdateFilteredBugList)));
	bank.AddToggle( "C",&m_bBugClassFilters[CMinimalBug::kC], datCallback(CFA(UpdateFilteredBugList)));
	bank.AddToggle( "D",&m_bBugClassFilters[CMinimalBug::kD], datCallback(CFA(UpdateFilteredBugList)));
	bank.AddToggle( "Task",&m_bBugClassFilters[CMinimalBug::kTask], datCallback(CFA(UpdateFilteredBugList)));
	bank.AddToggle( "Todo",&m_bBugClassFilters[CMinimalBug::kTodo], datCallback(CFA(UpdateFilteredBugList)));
	bank.AddToggle( "Wish",&m_bBugClassFilters[CMinimalBug::kWish], datCallback(CFA(UpdateFilteredBugList)));

	bank.AddTitle( "Filter Bug Status" );
	int statusFilterCount = sizeof(m_StatusFilters)/sizeof(m_StatusFilters[0]);
	for(int i=0;i<statusFilterCount;i++)
	{
		bank.AddToggle(m_StatusFilters[i].name, &m_StatusFilters[i].bInclude, datCallback(CFA(UpdateFilteredBugList)));
	}

	bank.AddTitle( "Filter Bug Component" );
	int componentFilterCount = sizeof(m_ComponentFilters)/sizeof(m_ComponentFilters[0]);
	for(int i=0;i<componentFilterCount;i++)
	{
		bank.AddToggle(m_ComponentFilters[i].name, &m_ComponentFilters[i].bInclude, datCallback(CFA(UpdateFilteredBugList)));
	}

	bank.AddTitle( "Filter Bug User" );
	bank.AddToggle("Bugs only for current user", &m_FilterByCurrentUser, datCallback(CFA(UpdateFilteredBugList)));

	bank.PopGroup( );

	// start the tcp worker thread now otherwise fetching bugs via RAG leads to a deadlock.
	// Pressing 'Fetch' in RAG will enter a critical section, which calls the LoadBug() callback
	// which fires off an HTTP query, which starts a thread. Starting a thread adds a bank widget
	// which also enters a critical section using the same token, causing a deadlock.
	// By starting the worker thread here, it avoids the problem.
	netTcp::StartWorker(0);
}

void CBugstarIntegration::InitLiveEditWidgets(bkBank& bank)
{
	bank.AddText("Enter Bug #",m_sFetchBugNumber,NELEM(m_sFetchBugNumber),false);
	bank.AddButton("Fetch",datCallback(CFA(LoadBugNumber)));
	bank.AddButton( "Add Screenshot", datCallback(CFA(AddScreenshot)) );
}

void CBugstarIntegration::LoginAndClear( )
{
	//strcpy(m_sUsername,m_sUsernameVis);

	sprintf(m_sUsername,"%s@rockstar.t2.corp",m_sUsernameVis);
	strcpy(m_sPassword,m_sPasswordVis);
	strcpy(m_sUsernameVis,"");
	strcpy(m_sPasswordVis,"");
}

#endif // __BANK


void CBugstarIntegration::LoadDevBugList( )
{
	if (LoadBugList())
	{
		UpdateFilteredBugList();
	}
}

void CBugstarIntegration::LoadBugNumber( )
{
	int number = 0;

	number = atoi(m_sFetchBugNumber);

	if(number > 0)
	{
		if (LoadBug(number))
		{
			// reset filtered buglist and add bug to list
			m_FilteredBugList.ResetCount();
			m_FilteredBugList.PushAndGrow(&m_Buglist[0]);
		}
	}
}

parTree* CBugstarIntegration::CreateXmlTreeFromHttpQuery(const char* search)
{
	return CreateXmlTreeFromHttpQuery(search, m_sUsername, m_sPassword);
}

parTree* CBugstarIntegration::CreateXmlTreeFromHttpQuery(const char* search, const char* username, const char* password)
{
	fwHttpQuery* pXmlSearch = CreateHttpQuery(username, password);
	parTree* pTree = NULL;

	if(pXmlSearch->Init(search))
	{
		while(pXmlSearch->Pending())
		{
			sysIpcSleep(100);
		}
		if(pXmlSearch->Succeeded())
			pTree = pXmlSearch->CreateTree();
		else
		{
			pXmlSearch->WriteBufferToFile("restError.html");
		}
	}
	delete pXmlSearch;
	return pTree;
}

parTree* CBugstarIntegration::CreateXmlTreeFromHttpQuery(const char* search, const char* postData)
{
	return CreateXmlTreeFromHttpQuery(search, postData, m_sUsername, m_sPassword);
}

parTree* CBugstarIntegration::CreateXmlTreeFromHttpQuery(const char* search, const char* postData, const char* username, const char* password)
{
	parTree* pTree = NULL;

	// Create and run the query
	fwHttpQuery* postQuery = CreateHttpQuery(username, password);
	postQuery->InitPost(search, s_BugstarRequestDefaultTimeout);
	postQuery->AppendXmlContent(postData, istrlen(postData));

	if(postQuery->CommitPost())
	{
		while(postQuery->Pending())
		{
			sysIpcSleep(100);
		}
		if(postQuery->Succeeded())
		{
			pTree = postQuery->CreateTree();
		}
		else
		{
			postQuery->WriteBufferToFile("restError.html");
		}
	}
	delete postQuery;
	return pTree;
}

bool CBugstarIntegration::PostHttpQuery(const char* search, const char* postData, const char* username, const char* password)
{
	// Create and run the query
	bool rt = false;
	fwHttpQuery* postQuery = CreateHttpQuery(username, password);
	postQuery->InitPost(search, s_BugstarRequestDefaultTimeout);
	postQuery->AppendXmlContent(postData, istrlen(postData));

	if(postQuery->CommitPost())
	{
		while(postQuery->Pending())
		{
			sysIpcSleep(100);
		}
		if(postQuery->Succeeded())
		{
			rt = true;
		}
		else
		{
			postQuery->WriteBufferToFile("restError.html");
		}
	}
	delete postQuery;
	return rt;
}




bool CBugstarIntegration::LoadBugList()
{
	// update the area center
	char search[BUG_LOGIN_SEARCH_LENGTH];

	Vec3V posn = gVpMan.GetGameViewport()->GetGrcViewport().GetCameraPosition();
	int x = (int)posn.GetXf();
	int y = (int)posn.GetYf();
	int z = (int)posn.GetZf();
	sprintf(m_sAreaCenter,"(%d,%d,%d)", x, y, z);

//	sprintf(search,"%sBugs/Searches/OpenByLocation?radius=%d&x=%d&y=%d&z=%d&fields=Id,Summary,X,Y,Z,Priority,Category,Component,State",BUGSTAR_GTA_PROJECT_PATH,(int)m_fAreaSize,x,y,z);
	sprintf(search,"%sBugs/Searches/NotClosedByLocation?radius=%d&x=%d&y=%d&z=%d&fields=Id,Summary,X,Y,Z,Priority,Category,Component,State,Developer&states=df,dfab,tns,tmi,tv",BUGSTAR_GTA_PROJECT_PATH,(int)m_fAreaSize,x,y,z);

	parTree* pTree = CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		m_Buglist.Reset();

		{
			parTreeNode* root = pTree->GetRoot();
			parTreeNode::ChildNodeIterator i = root->BeginChildren();

			int size = 0;

			for(; i != root->EndChildren(); ++i) {

				parTreeNode* id = (*i)->FindFromXPath("id");
				if (id)
				{
					size++;
				}
			}

			m_Buglist.Resize(size);
			size = 0;

			for(i = root->BeginChildren(); i != root->EndChildren(); ++i) 
			{
				parTreeNode* id = (*i)->FindFromXPath("id");							// B* Number

				if (id)
				{
					parTreeNode* summary = (*i)->FindFromXPath("summary");				// B* Description
					parTreeNode* xloc = (*i)->FindFromXPath("x");						// Position
					parTreeNode* yloc = (*i)->FindFromXPath("y");
					parTreeNode* zloc = (*i)->FindFromXPath("z");
					parTreeNode* priority = (*i)->FindFromXPath("priority");			// 1/2/3/4/5
					parTreeNode* category = (*i)->FindFromXPath("category");			// A/B/C/D/Task/TODO/Wish
					parTreeNode* component = (*i)->FindFromXPath("component");			// Art/Code:Graphics/Code:Systems/etc... (seems to return a number)
					parTreeNode* state = (*i)->FindFromXPath("state");					// Dev(fixing)/etc.... (Seems to be "DEV")
					parTreeNode* owner = (*i)->FindFromXPath("developer");				// Current Owner ID

					CMinimalBug& addbug = m_Buglist[size];

					addbug.SetNumber(atoi((char*)id->GetData()));
					if(summary && summary->GetData())
						addbug.SetSummary((char*)summary->GetData());
					if(xloc && xloc->GetData())
						addbug.SetX((float)atof((char*)xloc->GetData()));
					if(yloc && yloc->GetData())
						addbug.SetY((float)atof((char*)yloc->GetData()));
					if(zloc && zloc->GetData())
						addbug.SetZ((float)atof((char*)zloc->GetData()));
					if(priority && priority->GetData())
						addbug.SetSeverity(atoi((char*)priority->GetData()));
					if(category && category->GetData())
						addbug.SetCategory((char*)category->GetData());
					if(component && component->GetData())
						addbug.SetComponent((int)atoi((char*)component->GetData()));
					if(state && state->GetData())
						addbug.SetState((char*)state->GetData());
					if(owner && owner->GetData())
						addbug.SetOwner(atoi((char*)owner->GetData()));

				}
				size++;
			}
		}

		delete pTree;
		return true;
	}

	return false;
}

bool CBugstarIntegration::LoadBug(int bugNumber)
{
	char search[BUG_LOGIN_SEARCH_LENGTH];

	sprintf(search,"%sBugs/%d", BUGSTAR_GTA_PROJECT_PATH,bugNumber);

	parTree* pTree = CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		if(strcmp(pTree->GetRoot()->GetElement().GetName(), "bug"))
		{
			Errorf("Bugstar Integration - Bug cannot be found, make sure you're trying to access the correct Bugstar project (set in BugstarIntegration.h BUGSTAR_GTA_PROJECT_PATH)");
			return false;
		}
		m_Buglist.Reset();
		m_Buglist.Resize(1);

		parTreeNode* root = pTree->GetRoot();
		parTreeNode* id = root->FindFromXPath("id");
		parTreeNode* summary = root->FindFromXPath("summary");
		parTreeNode* xloc = root->FindFromXPath("x");
		parTreeNode* yloc = root->FindFromXPath("y");
		parTreeNode* zloc = root->FindFromXPath("z");
		parTreeNode* priority = root->FindFromXPath("priority");
		parTreeNode* category = root->FindFromXPath("category");
		parTreeNode* component = root->FindFromXPath("component");
		parTreeNode* state = root->FindFromXPath("state");

		CMinimalBug& addbug = m_Buglist[0];

		addbug.SetNumber(atoi((char*)id->GetData()));
		if(summary && summary->GetData())
			addbug.SetSummary((char*)summary->GetData());
		if(xloc && xloc->GetData())
			addbug.SetX((float)atof((char*)xloc->GetData()));
		if(yloc && yloc->GetData())
			addbug.SetY((float)atof((char*)yloc->GetData()));
		if(zloc && zloc->GetData())
			addbug.SetZ((float)atof((char*)zloc->GetData()));
		if(priority && priority->GetData())
			addbug.SetSeverity(atoi((char*)priority->GetData()));
		if(category && category->GetData())
			addbug.SetCategory((char*)category->GetData());
		if(component && component->GetData())
			addbug.SetComponent((int)atoi((char*)component->GetData()));
		if(state && state->GetData())
			addbug.SetState((char*)state->GetData());

		delete pTree;
		return true;
	}

	return false;
}

void CBugstarIntegration::UpdateSelectedBugDetails()
{
	atArray<CMinimalBug*>& list = GetFilteredBugList();
	sprintf(m_sBugNumber,"###");
	sprintf(m_sBugSummary,"No bugs loaded");

	if(list.size() > m_iSelectedBugIndex && m_iSelectedBugIndex >= 0)
	{
		CMinimalBug* bug = list[m_iSelectedBugIndex];

		memset(m_sBugNumber,0,BUG_NUMBER_STRING_LENGTH);
		sprintf(m_sBugNumber,"%d",bug->Number());
		
		memset(m_sBugSummary,0,BUG_SUMMARY_STRING_LENGTH);
		sprintf(m_sBugSummary,"%s",bug->Summary());
	}
}

void CBugstarIntegration::NextBug( )
{
	atArray<CMinimalBug*>& list = GetFilteredBugList();

	if(list.size() > 0)
	{
		if(list.size() > (m_iSelectedBugIndex + 1))
		{
			m_iSelectedBugIndex++;
		}
		else
		{
			m_iSelectedBugIndex = 0;
		}
	}
	else
	{
		m_iSelectedBugIndex = -1;
	}
	UpdateSelectedBugDetails();
}

void CBugstarIntegration::PreviousBug( )
{
	atArray<CMinimalBug*>& list = GetFilteredBugList();

	if(list.size() > 0)
	{
		if( (m_iSelectedBugIndex - 1) >= 0)
		{
			m_iSelectedBugIndex--;
		}
		else
		{
			m_iSelectedBugIndex = (list.size() -1);
		}
	}
	else
	{
		m_iSelectedBugIndex = -1;
	}

	UpdateSelectedBugDetails();
}

void CBugstarIntegration::SelectNearestBug( )
{
	Vector3 pos = CGameWorld::GetMainPlayerInfo()->GetPos();

	int cur_nearest = -1;
	float cur_dist = -1.0;

	atArray<CMinimalBug*>& list = GetFilteredBugList();

	if(list.size() > 0)
	{
		CMinimalBug* bug = list[0];
		Vector3 bug_pos(bug->X(),bug->Y(),bug->Z());
		cur_dist = pos.Dist(bug_pos);

		for(int i = 0; i < list.size(); i++)
		{
			CMinimalBug* bug2 = list[i];
			Vector3 bug_pos(bug2->X(),bug2->Y(),bug2->Z());
			float tmp = pos.Dist(bug_pos);
			if(tmp < cur_dist)
			{
				cur_dist = tmp;
				cur_nearest = i;
			}
		}

		m_iSelectedBugIndex = cur_nearest;
	}

	UpdateSelectedBugDetails();
}

void CBugstarIntegration::WarpToSelectedBug( )
{
	atArray<CMinimalBug*>& list = GetFilteredBugList();

	if(list.size() > m_iSelectedBugIndex && m_iSelectedBugIndex >= 0)
	{
		WarpToBug(list[m_iSelectedBugIndex]);
	}
}

void CBugstarIntegration::WarpToBug(CMinimalBug* bug )
{
	Vector3 vBugPosition(bug->X(),bug->Y(),bug->Z());
	if (FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true))
	{
		FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true)->Teleport(vBugPosition, 0.0f, false);
	}
	else if(AssertVerify(FindPlayerPed()))
	{
		FindPlayerPed()->Teleport(vBugPosition, 0.0f);
	}

	g_SceneStreamerMgr.LoadScene(vBugPosition);
}

void CBugstarIntegration::AddScreenshot()
{
	atArray<CMinimalBug*>& list = GetFilteredBugList();

	if(list.size() > m_iSelectedBugIndex && m_iSelectedBugIndex >= 0)
	{
		m_bProcessScreenshot = true;
		m_iScreenshotFrameDelay = 3;
	}
}

int CBugstarIntegration::GetBugCountForAssertId(unsigned int assertId)
{
	USE_DEBUG_MEMORY();
	int bugCount = 0;

	parTree* pResultTree = PerformFulltextSearch(assertId);
	if (pResultTree)
	{
		// Process the results to get at the bug count
		parTreeNode* root = pResultTree->GetRoot();
		if (root)
		{
			parAttribute* countAttr = root->FindAttributeFromXPath("@count");
			if (countAttr)
			{
				bugCount = atoi(countAttr->GetStringValue());
			}
		}
		delete pResultTree;
	}
	else
	{
		bugCount = -1;
	}

	return bugCount;
}

int CBugstarIntegration::CreateNewAssertBug(const char* summary, const char* description, const CBugstarAssertUpdateQuery::BugDesc& bugDesc)
{
	USE_DEBUG_MEMORY();
	int newBugId = 0;
	char buffer[64];

	// Generate the tree that represents the xml to post
	parTree* pTree = rage_new parTree();
	parTreeNode* pRoot = rage_new parTreeNode("bug");
	pTree->SetRoot(pRoot);

	parTreeNode* pSummary = rage_new parTreeNode("summary");
	pSummary->SetData(summary, ustrlen(summary));
	pSummary->AppendAsChildOf(pRoot);

	parTreeNode* pDescription = rage_new parTreeNode("description");
	pDescription->SetData(description, ustrlen(description));
	pDescription->AppendAsChildOf(pRoot);

	sprintf(buffer, "%f", bugDesc.position.x);
	parTreeNode* pX = rage_new parTreeNode("x");
	pX->SetData(buffer, ustrlen(buffer));
	pX->AppendAsChildOf(pRoot);

	sprintf(buffer, "%f", bugDesc.position.y);
	parTreeNode* pY = rage_new parTreeNode("y");
	pY->SetData(buffer, ustrlen(buffer));
	pY->AppendAsChildOf(pRoot);

	sprintf(buffer, "%f", bugDesc.position.z);
	parTreeNode* pZ = rage_new parTreeNode("z");
	pZ->SetData(buffer, ustrlen(buffer));
	pZ->AppendAsChildOf(pRoot);

	// Check whether we should assign this to default art or default code
	int developerId = -1;
	// The QA owner should be the local user
	int testerId = GetCurrentUserId();

	// Check that we got a valid user
	if(testerId == -1)
	{
		delete pTree;
		return 0;
	}

	developerId = testerId;

	sprintf(buffer, "%d", developerId);
	parTreeNode* pDeveloper = rage_new parTreeNode("developer");
	pDeveloper->SetData(buffer, ustrlen(buffer));
	pDeveloper->AppendAsChildOf(pRoot);

	sprintf(buffer, "%d", testerId);
	parTreeNode* pTester = rage_new parTreeNode("tester");
	pTester->SetData(buffer, ustrlen(buffer));
	pTester->AppendAsChildOf(pRoot);

	sprintf(buffer, "%d", bugDesc.buildId);
	parTreeNode* pBuild = rage_new parTreeNode("foundIn");
	pBuild->SetData(buffer, ustrlen(buffer));
	pBuild->AppendAsChildOf(pRoot);

	sprintf(buffer, "%d", bugDesc.mapId);
	parTreeNode* pMap = rage_new parTreeNode("map");
	pMap->SetData(buffer, ustrlen(buffer));
	pMap->AppendAsChildOf(pRoot);

	sprintf(buffer, "%d", bugDesc.missionId);
	parTreeNode* pMission = rage_new parTreeNode("mission");
	pMission->SetData(buffer, ustrlen(buffer));
	pMission->AppendAsChildOf(pRoot);

	parTreeNode* pCategory = rage_new parTreeNode("category");
	pCategory->SetData("A", 1);
	pCategory->AppendAsChildOf(pRoot);

	parTreeNode* pTagList = rage_new parTreeNode("tagList");
	pTagList->AppendAsChildOf(pRoot);

	parTreeNode* pAssertTag = rage_new parTreeNode("tag");
	pAssertTag->SetData("assert", 7);
	pAssertTag->AppendAsChildOf(pTagList);
#if __XENON
	parTreeNode* pPlatformTag = rage_new parTreeNode("tag");
	pPlatformTag->SetData("XBOX360", 8);
	pPlatformTag->AppendAsChildOf(pTagList);
#elif __PS3
	parTreeNode* pPlatformTag = rage_new parTreeNode("tag");
	pPlatformTag->SetData("PS3", 4);
	pPlatformTag->AppendAsChildOf(pTagList);
#elif __WIN32PC
	parTreeNode* pPlatformTag = rage_new parTreeNode("tag");
	pPlatformTag->SetData("PC", 3);
	pPlatformTag->AppendAsChildOf(pTagList);
#elif RSG_DURANGO
	parTreeNode* pPlatformTag = rage_new parTreeNode("tag");
	pPlatformTag->SetData("DURANGO", 3);
	pPlatformTag->AppendAsChildOf(pTagList);
#elif RSG_ORBIS
	parTreeNode* pPlatformTag = rage_new parTreeNode("tag");
	pPlatformTag->SetData("ORBIS", 3);
	pPlatformTag->AppendAsChildOf(pTagList);
#else
	#error "Not Implemented"
#endif

	parTreeNode* pSeen = rage_new parTreeNode("seen");
	pSeen->SetData("1", 2);
	pSeen->AppendAsChildOf(pRoot);

	parTreeNode* pCCList = rage_new parTreeNode("ccList");
	pCCList->AppendAsChildOf(pRoot);

	sprintf(buffer, "Assert ID: %u", bugDesc.assertId);
	parTreeNode* pAssertComment = rage_new parTreeNode("addcomment");
	parTreeNode* pText = rage_new parTreeNode("text");
	pText->SetData(buffer, ustrlen(buffer));
	pText->AppendAsChildOf(pAssertComment);
	pAssertComment->AppendAsChildOf(pRoot);

	// Convert the tree to an xml string
	datGrowBuffer gb;
	gb.Init(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL), datGrowBuffer::NULL_TERMINATE);

	char gbName[RAGE_MAX_PATH];
	fiDevice::MakeGrowBufferFileName(gbName, RAGE_MAX_PATH, &gb);
	fiStream* stream = ASSET.Create(gbName, "");
	PARSER.SaveTree(stream, pTree, parManager::XML);
	stream->Flush();
	stream->Close();

	// Run the query
	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sBugs", BUGSTAR_GTA_PROJECT_PATH);

	parTree* pResultTree = CreateXmlTreeFromHttpQuery(search, (char*)gb.GetBuffer(), m_sAssertUsername, m_sAssertPassword);
	if (pResultTree)
	{
		// Process the results to get at the bug id
		parTreeNode* root = pResultTree->GetRoot();
		if (root)
		{
			parTreeNode* idNode = root->FindFromXPath("id");
			if (idNode)
			{
				newBugId = atoi((char*)idNode->GetData());
			}
		}
		delete pResultTree;
	}

	delete pTree;
	return newBugId;
}

parTree* CBugstarIntegration::GetMostRecentOpenBug(unsigned int assertId)
{
	USE_DEBUG_MEMORY();
	parTree* openBugTree = NULL;
	CMinimalBug openMinimalBug;

	// Get the list of bugs for this assert
	parTree* pSearchTree = PerformFulltextSearch(assertId, false);
	if (pSearchTree)
	{
		// Process the results to get at the bug count
		parTreeNode* searchTreeRoot = pSearchTree->GetRoot();
		if (searchTreeRoot)
		{
			//for(parTreeNode::ChildNodeIterator itr = searchTreeRoot->BeginChildren(); itr != searchTreeRoot->EndChildren(); ++itr) 
			for (int i = 0; i < searchTreeRoot->FindNumChildren(); ++i)
			{
				parTreeNode* child = searchTreeRoot->FindChildWithIndex(i);
				parAttribute* idAttr = child->FindAttributeFromXPath("@id");

				// Query bugstar for the full bug information based on the id attribute
				//parAttribute* idAttr = (*itr)->FindAttributeFromXPath("@id");
				parTree* bugTree = GetBug(atoi((char*)idAttr->GetStringValue()));

				if (bugTree)
				{
					CMinimalBug minimalBug;
					ConvertToMinimalBug(bugTree, minimalBug);
					// bugs which aren't closed and aren't with admin for review are considered open
					bool isBugOpen = ((strncmp (minimalBug.State(), "CLOSED", 6) != 0) && (strncmp (minimalBug.State(), "REVIEW", 6) != 0));

					if (isBugOpen && difftime(minimalBug.CollectedOn(), openMinimalBug.CollectedOn()) > 0)
					{
						// If both bugs are in the same state, keep track of the more recent one
						if (openBugTree != NULL)
						{
							delete openBugTree;
						}
						openBugTree = bugTree;
						openMinimalBug = minimalBug;
					}
					else
					{
						// We don't need to keep track of this bug so delete the tree
						delete bugTree;
					}
				}
			}
		}
		delete pSearchTree;
	}

	return openBugTree;
}

//
// name: PostBugChanges
// description: Edit bug by posting to REST interface
bool CBugstarIntegration::PostBugChanges(int bugId, parTree* pBugTree)
{
	// Generate the query for sending the updated bug data
	datGrowBuffer gb;
	gb.Init(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL), datGrowBuffer::NULL_TERMINATE);

	char gbName[RAGE_MAX_PATH];
	fiDevice::MakeGrowBufferFileName(gbName, RAGE_MAX_PATH, &gb);
	fiStream* stream = ASSET.Create(gbName, "");
	PARSER.SaveTree(stream, pBugTree, parManager::XML);
	stream->Flush();
	stream->Close();

	// Run the query
	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sBugs/%d", BUGSTAR_GTA_PROJECT_PATH, bugId);

	parTree* pResultTree = CreateXmlTreeFromHttpQuery(search, (char*)gb.GetBuffer(), m_sAssertUsername, m_sAssertPassword);
	if (pResultTree)
	{
		delete pResultTree;
		return true;
	}
	else
	{
		return false;
	}

}

//
// name: PostBugChanges
// description: Edit bug by posting to REST interface
bool CBugstarIntegration::PostBugFieldChange(int bugId, const char* field, const char* value)
{
	// Run the query
	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sBugs/%d/%s", BUGSTAR_GTA_PROJECT_PATH, bugId, field);
	return PostHttpQuery(search, value, m_sAssertUsername, m_sAssertPassword);
}

int CBugstarIntegration::IncrementBugSeenCount(int bugId, int buildId, parTree* pBugTree)
{
	USE_DEBUG_MEMORY();

	parTreeNode* verificationNode = pBugTree->GetRoot()->FindFromXPath("seen");
	int currentVerificationCount = 0;
	if (verificationNode != NULL)
		currentVerificationCount = atoi((char*)verificationNode->GetData());
	++currentVerificationCount;

	char buffer[64];
	sprintf(buffer, "%d", currentVerificationCount);

	//return PostBugFieldChange(bugId, "seen", buffer);

	parTreeNode* newVerificationNode = rage_new parTreeNode("seen");
	newVerificationNode->SetData(buffer, ustrlen(buffer));
	verificationNode->SwapWith(*newVerificationNode);
	delete newVerificationNode;

	// Only update build if build hasn't been set already
	if(buildId != 0)
	{
		parTreeNode* buildNode = pBugTree->GetRoot()->FindFromXPath("foundIn");
		if (buildNode == NULL)
		{
			sprintf(buffer, "%d", buildId);
			parTreeNode* newBuildNode = rage_new parTreeNode("foundIn");
			newBuildNode->SetData(buffer, ustrlen(buffer));
			newBuildNode->AppendAsChildOf(pBugTree->GetRoot());
		}
	}

	PostBugChanges(bugId, pBugTree);

	return currentVerificationCount;
}

bool CBugstarIntegration::ChangeField(int bugId, const char* field, const char* value)
{
	USE_DEBUG_MEMORY();

	bool rt = false;
	parTree* pTree = GetBug(bugId);
	if(pTree)
	{
		parTreeNode* node = pTree->GetRoot()->FindFromXPath(field);
		if(node == NULL)
		{
			node = rage_new parTreeNode(field);
			node->AppendAsChildOf(pTree->GetRoot());
		}
		node->SetData(value, ustrlen(value));

		rt = PostBugChanges(bugId, pTree);
		delete pTree;
	}
	return rt;
}

bool CBugstarIntegration::AddComment(int bugId, const char* comment)
{
	USE_DEBUG_MEMORY();

	bool rt = false;
	parTree* pTree = GetBug(bugId);
	if(pTree)
	{
		parTreeNode* commentNode = rage_new parTreeNode("addcomment");
		parTreeNode* pText = rage_new parTreeNode("text");
		pText->SetData(comment, ustrlen(comment));
		pText->AppendAsChildOf(commentNode);
		commentNode->AppendAsChildOf(pTree->GetRoot());


		rt = PostBugChanges(bugId, pTree);
		delete pTree;
	}
	return rt;
}

bool CBugstarIntegration::ConvertToMinimalBug(parTree* pBugTree, CMinimalBug& bug)
{
	if (pBugTree == NULL || pBugTree->GetRoot() == NULL)
	{
		return false;
	}

	parTreeNode* pRoot = pBugTree->GetRoot();

	parTreeNode* id = pRoot->FindFromXPath("id");
	parTreeNode* summary = pRoot->FindFromXPath("summary");
	parTreeNode* xloc = pRoot->FindFromXPath("x");
	parTreeNode* yloc = pRoot->FindFromXPath("y");
	parTreeNode* zloc = pRoot->FindFromXPath("z");
	parTreeNode* state = pRoot->FindFromXPath("state");
	parTreeNode* priority = pRoot->FindFromXPath("priority");
	parTreeNode* category = pRoot->FindFromXPath("category");
	parTreeNode* collectedOn = pRoot->FindFromXPath("collectedOn");
	parTreeNode* fixedOn = pRoot->FindFromXPath("fixedOn");
	parTreeNode* owner = pRoot->FindFromXPath("developer");
	parTreeNode* qaOwner = pRoot->FindFromXPath("tester");

	bug.SetNumber(atoi((char*)id->GetData()));
	if(summary && summary->GetData())
		bug.SetSummary((char*)summary->GetData());
	if(xloc && xloc->GetData())
		bug.SetX((float)atof((char*)xloc->GetData()));
	if(yloc && yloc->GetData())
		bug.SetY((float)atof((char*)yloc->GetData()));
	if(zloc && zloc->GetData())
		bug.SetZ((float)atof((char*)zloc->GetData()));
	if(state && state->GetData())
		bug.SetState((char*)state->GetData());
	if(priority && priority->GetData())
		bug.SetSeverity(atoi((char*)priority->GetData()));
	if(category && category->GetData())
		bug.SetCategory((char*)category->GetData());
	if(collectedOn && collectedOn->GetData())
		bug.SetCollectedOn((char*)collectedOn->GetData());
	if(fixedOn && fixedOn->GetData())
		bug.SetFixedOn((char*)fixedOn->GetData());
	if(owner && owner->GetData())
		bug.SetOwner(atoi((char*)owner->GetData()));
	if(qaOwner && qaOwner->GetData())
		bug.SetQaOwner(atoi((char*)qaOwner->GetData()));

	return true;
}

fwHttpQuery* CBugstarIntegration::CreateHttpQuery()
{
	return CreateHttpQuery(m_sUsername, m_sPassword);
}

fwHttpQuery* CBugstarIntegration::CreateHttpQuery(const char* username, const char* password)
{
	return rage_new fwHttpQuery(BUGSTAR_IP_ADDRESS, BUGSTAR_REST_SERVER, username, password, true);
}

CHttpQuerySaveToFile* CBugstarIntegration::CreateSaveHttpQuery(const char* filename)
{
	return rage_new CHttpQuerySaveToFile(BUGSTAR_IP_ADDRESS, BUGSTAR_REST_SERVER, m_sUsername, m_sPassword, filename, true);
}

void CBugstarIntegration::RenderTheBuglist( )
{
	static	sysSpinLockToken	SpinLock;
	SYS_SPINLOCK_ENTER(SpinLock);

	if(m_bBugMarkersOn)
	{
		atArray<CMinimalBug*>& list = GetFilteredBugList();

		for(int i = 0; i < list.size(); i++)
		{
			if(i == m_iSelectedBugIndex)
			{
				RenderBug(list[i],true);
			}
			else
			{
				RenderBug(list[i],false);
			}
		}		
	}
	
	SYS_SPINLOCK_EXIT(SpinLock);
}

void CBugstarIntegration::RenderMarker(float x,float y, float z,float r,Color32 colour,bool highlight,const char* label)
{	
	bool bSolid = true;
	if(highlight == true)
	{
		r = r*2;
		//colour =  Color32(200,200,200,255);
		bSolid = false;
	}

	s32 num_slices = 2;
	s32 num_segments = 4;
	s32 bit_num_segments = 1;
	s32 quart_num_slices = 1;
	s32 quart_num_segments = 1;

	s32 i;
	float fSliceAngleInc = PI/(float)num_slices;
	float fSegmentAngleInc = 2*PI/(float)num_segments;
	
	s32 nFirstFaceSegment = (num_segments/2)+1 - bit_num_segments;
	s32 nLastFaceSegment = (num_segments/2)+1 + bit_num_segments;
	float fFaceSliceInc = 0.5f / (float)quart_num_slices;
	float fFaceSegmentInc = 1.0f / (float)quart_num_segments;
	
	s32 nSlice;
	float fNormL, fNormH;
	float fHeightL, fHeightH;
	float fRadiusFracL, fRadiusFracH;
	
	grcColor(colour);	
	bool wasOverride = grcStateBlock::SetWireframeOverride(!bSolid);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	// do top slices
	for(nSlice = 1; nSlice <= num_slices; nSlice++)
	{
		fNormL = rage::Cosf((nSlice-1)*fSliceAngleInc);
		fRadiusFracL = rage::Sinf((nSlice-1)*fSliceAngleInc);
		fHeightL = z + r*fNormL;
		
		fNormH = rage::Cosf(nSlice*fSliceAngleInc);
		fRadiusFracH = rage::Sinf(nSlice*fSliceAngleInc);
		fHeightH = z + r*fNormH;
		
		grcBegin(drawTriStrip, (num_segments+1) * 2);
		
		for(i=0; i <= num_segments;i++)
		{
			
			float u,v;
			if(i<nFirstFaceSegment)
				u = 0.0f;
			else if(i>nLastFaceSegment)
				u = 1.0f;
			else
				u = (i-nFirstFaceSegment)*fFaceSegmentInc;
			
			if(nSlice < quart_num_slices)
				v = 0.0f;
			else if(nSlice > num_slices-quart_num_slices)
				v = 1.0f;
			else
				v = 0.5f + (nSlice)*fFaceSliceInc;

			grcTexCoord2f(u, v);
			grcNormal3f(fRadiusFracH*rage::Sinf(i*fSegmentAngleInc), fRadiusFracH*rage::Cosf(i*fSegmentAngleInc), fNormH);
			grcVertex3f(x+r*fRadiusFracH*rage::Sinf(i*fSegmentAngleInc), y+r*fRadiusFracH*rage::Cosf(i*fSegmentAngleInc), fHeightH);

			// lower vertex
			if(i<nFirstFaceSegment)
				u = 0.0f;
			else if(i>nLastFaceSegment)
				u = 1.0f;
			else
				u = (i-nFirstFaceSegment)*fFaceSegmentInc;
			
			if(nSlice < quart_num_slices)
				v = 0.0f;
			else if(nSlice > num_slices-quart_num_slices)
				v = 1.0f;
			else
				v = 0.5f + (nSlice-1)*fFaceSliceInc;
			
			grcTexCoord2f(u, v);
			grcNormal3f(fRadiusFracL*rage::Sinf(i*fSegmentAngleInc), fRadiusFracL*rage::Cosf(i*fSegmentAngleInc), fNormL);
			grcVertex3f(x+r*fRadiusFracL*rage::Sinf(i*fSegmentAngleInc), y+r*fRadiusFracL*rage::Cosf(i*fSegmentAngleInc), fHeightL);
        }

		grcEnd();
	}
	grcStateBlock::SetWireframeOverride(wasOverride);

	grcBindTexture(NULL);
	grcColor(Color_white);
	grcDrawLabelf(Vector3(x,y,z),label);

}

void CBugstarIntegration::RenderBug(CMinimalBug* bug,bool highlight)
{
	float x = bug->X();
	float y = bug->Y();
	float z = bug->Z();
	float r = m_fBugMarkerSize;

	Color32 colour;
	
	switch(bug->Severity())
	{
	case 0:
		colour = Color32(255,0,0,255); // A
		break;
	case 1:
		colour = Color32(255,150,0,255); // B
		break;
	case 2:
		colour = Color32(255,222,100,255); // C
		break;
	case 3:
		colour = Color32(188,255,50,255); // D
		break;
	case 4:
		colour = Color32(0,255,0,255); // wish
		break;
	case 5:
		colour = Color32(255,0,255,255); // todo
		break;
	case 6:
		colour = Color32(0,255,230,255); // task
		break;
	default:
		colour = Color32(0,0,0,255);
		break;

	}
	grcBindTexture(NULL);
	grcWorldIdentity();
	grcLightState::SetEnabled(false);

	char idlabel[64];
	sprintf(idlabel,"%d",bug->Number());

	RenderMarker(x,y,z,r,colour,highlight,idlabel);
	
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
}

bool CBugstarIntegration::PassesStatusFilter(CMinimalBug *pBug)
{
	char *pProperStatusString = CBugstarStateLookup::GetDescriptionFromState(pBug->State());

	int statusFilterCount = sizeof(m_StatusFilters)/sizeof(m_StatusFilters[0]);
	for(int i=0;i<statusFilterCount;i++)
	{
		StatusFilter *pStatFilter = &m_StatusFilters[i];
		if( pStatFilter->bInclude == true)
		{
			if( strcmp(pProperStatusString, pStatFilter->name) == 0 )
			{
				return true;
			}
		}
	}
	return false;
}

bool CBugstarIntegration::PassesComponentFilter(CMinimalBug *pBug)
{
	// Special case, no component has been set to test against, so return true
	if(pBug->Component() == 0)
	{
		return true;
	}

	char *pComponentString = CBugstarComponentLookup::GetComponentString(pBug->Component());
	if( pComponentString )
	{
		int componentFilterCount = sizeof(m_ComponentFilters)/sizeof(m_ComponentFilters[0]);
		for(int i=0;i<componentFilterCount;i++)
		{
			ComponentFilter *pCompFilter = &m_ComponentFilters[i];
			if( pCompFilter->bInclude == true)
			{
				// Only check as many chars as the filter contains, so if we select "Art", "Art : Monkeys" will still pass
				if( strncmp(pComponentString, pCompFilter->name, strlen(pCompFilter->name) ) == 0 )
				{
					return true;
				}
			}
		}
	}
	return false;
}

// Check the bug against the current owner
bool CBugstarIntegration::PassesOwnerFilter(CMinimalBug *pBug)
{
	// We're not filtering by user
	if( !m_FilterByCurrentUser )
		return true;

	// Check bug owner is current User.. return true if so.
	if(pBug->Owner() == m_CurrentUserID)
	{
		return true;
	}
	return false;
}

void CBugstarIntegration::UpdateFilteredBugList()
{
	if(!CBugstarComponentLookup::IsInitialised())
	{
		CBugstarComponentLookup::Init();
	}

	if(!CBugstarStateLookup::IsInitialised())
	{
		CBugstarStateLookup::Init();
	}

	if(!CBugstarUserLookup::IsInitialised())
	{
		CBugstarUserLookup::Init();
		m_CurrentUserID = CBugstarUserLookup::GetUserIDFromNameHash(m_CurrentUsername.GetHash());
	}

	m_FilteredBugList.Reset();

	int filteredBugsCount = 0;
	for(int i = 0; i < m_Buglist.size(); i++)
	{
		CMinimalBug *pBug = &m_Buglist[i];

		if (m_bBugClassFilters[pBug->Category()])
		{
			// Check the status filter
			if(PassesStatusFilter(pBug) && PassesComponentFilter(pBug) && PassesOwnerFilter(pBug))
			{
				++filteredBugsCount;
			}
		}
	}

	if (filteredBugsCount > 0)
	{
		m_FilteredBugList.Resize(filteredBugsCount);
		int idx = 0;

		for(int i = 0; i < m_Buglist.size(); i++)
		{
			CMinimalBug *pBug = &m_Buglist[i];

			if (m_bBugClassFilters[pBug->Category()])
			{
				if(PassesStatusFilter(pBug) && PassesComponentFilter(pBug) && PassesOwnerFilter(pBug))
				{
					m_FilteredBugList[idx++] = &m_Buglist[i];
				}
			}
		}

		m_iSelectedBugIndex = 0;
	}
	else
	{
		m_iSelectedBugIndex = -1;
	}

	UpdateSelectedBugDetails();
}

parTree* CBugstarIntegration::PerformFulltextSearch(unsigned int assertId, bool countOnly)
{
	USE_DEBUG_MEMORY();

	// Post something along the lines of the following:
	// <Search query="tag:assert" countOnly="false"/>
	parTree* pTree = rage_new parTree();
	parTreeNode* pRoot = rage_new parTreeNode("Search");
	pTree->SetRoot(pRoot);

	// Create the query
	char query[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(query,"comment:\"Assert ID: %u\"", assertId);

	pRoot->GetElement().AddAttribute("query", query);
	pRoot->GetElement().AddAttribute("countOnly", countOnly);

	// Convert the tree to an xml string
	datGrowBuffer gb;
	gb.Init(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL), datGrowBuffer::NULL_TERMINATE);

	char gbName[RAGE_MAX_PATH];
	fiDevice::MakeGrowBufferFileName(gbName, RAGE_MAX_PATH, &gb);
	fiStream* stream = ASSET.Create(gbName, "");
	PARSER.SaveTree(stream, pTree, parManager::XML);
	stream->Flush();
	stream->Close();
	delete pTree;

	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sFulltextSearch", BUGSTAR_GTA_PROJECT_PATH);

	// Run the query
	return CreateXmlTreeFromHttpQuery(search, (char*)gb.GetBuffer());
}

parTree* CBugstarIntegration::GetBug(unsigned int bugId)
{
	USE_DEBUG_MEMORY();
	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sBugs/%d", BUGSTAR_GTA_PROJECT_PATH, bugId);

	return CreateXmlTreeFromHttpQuery(search);
}

int CBugstarIntegration::GetCurrentUserId()
{
	// Check whether we already know what the id is
	if (sm_currentUserId != -1)
	{
		return sm_currentUserId;
	}

	// Get the user name
	char userName[64];
	sysGetEnv("USERNAME", userName, 64);

	sm_currentUserId = GetUserByName(userName);
	return sm_currentUserId;
}

int CBugstarIntegration::GetUserByName(const char* username)
{
	int userId = -1;

	// Create the query
	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sUsers/Search?username=%s", BUGSTAR_GTA_PROJECT_PATH, username);

	parTree* pTree = CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		parTreeNode* root = pTree->GetRoot();
		parTreeNode* firstUser = root->GetChild();
		if (firstUser)
		{
			parTreeNode* id = firstUser->FindFromXPath("id");
			userId = atoi((char*)id->GetData());
		}
		delete pTree;
	}

	return userId;
}

int CBugstarIntegration::GetTeamByName(const char* teamname)
{
	// is team name in map
	if(sm_teamIdMap.Access(teamname))
	{
		return sm_teamIdMap[teamname];
	}

	int teamId = -1;

	// Create the query
	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sTeams/Search?name=%s", BUGSTAR_GTA_PROJECT_PATH, teamname);

	parTree* pTree = CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		parTreeNode* root = pTree->GetRoot();
		parTreeNode* firstTeam = root->GetChild();
		if (firstTeam)
		{
			parTreeNode* id = firstTeam->FindFromXPath("id");
			teamId = atoi((char*)id->GetData());
		}
		delete pTree;
	}

	// store teamname in map
	sm_teamIdMap[teamname] = teamId;

	return teamId;
}

bool CBugstarIntegration::AddConsoleLogsToBug(void)
{
	return NetworkInterface::IsGameInProgress() && PARAM_attachConsoleLogsToBugsInMP.Get();
}

bool CBugstarIntegration::GetStreamingVideoId(char* id, size_t idSize)
{
	if (sm_isStreamingVideoEnabled == false)
	{
		return false;
	}

	if (*sm_streamingVideoId == '\0')
	{
		return false;
	}

	if (IsStreamingVideo() == false)
	{
		return false;
	}

	Assertf(strlen(sm_streamingVideoId) < idSize, "Bugstar streaming video buffer size %i is too small", int(idSize));

	strncpy(id, sm_streamingVideoId, idSize);

	// Null terminate the string in case the whole buffer was filled.
	sm_streamingVideoId[idSize - 1] = '\0';

	return true;
}

void CBugstarIntegration::InitStreamingVideo()
{
	sm_isStreamingVideoEnabled	= false;
	*sm_streamingVideoBaseUri	= '\0';
	*sm_streamingVideoId		= '\0';

	USE_DEBUG_MEMORY();
	
	if (fiRemoteServerIsRunning == false)
	{
		sm_streamingVideoErrorMessage = "sysTrayRfs is required for streaming video";
		return;
	}

	if (ValidateStreamingVideoSettings() == false)
	{
		sm_streamingVideoErrorMessage = "Invalid streaming video setup";
		return;
	}

	if (LoadStreamingVideoSettings() == false)
	{
		sm_streamingVideoErrorMessage = "Invalid streaming video settings file";
		return;
	}

	if (SetStreamingVideoSettings() == false)
	{
		sm_streamingVideoErrorMessage = "Failed to set streaming video settings";
		return;
	}

	sm_isStreamingVideoEnabled = true;

	Displayf("Bugstar streaming video is enabled.");
}

bool CBugstarIntegration::ValidateStreamingVideoSettings()
{

#if RSG_ORBIS
	return ValidateStreamingVideoSettingsOrbis();
#else
	return true;
#endif

}

bool CBugstarIntegration::LoadStreamingVideoSettings()
{
	char filePath[MAX_PATH];
	
	formatf(filePath, "%s\\Settings.xml", sm_streamingVideoRootDirectory);

	fiStream* stream = fiStream::Open(filePath, true);

	if (stream == nullptr)
	{
		Errorf("Failed to load '%s'. Bugstar streaming video is disabled.", filePath);
		return false;
	}

	parTree* tree = PARSER.LoadTree(stream, nullptr);

	if (tree == nullptr)
	{
		Errorf("Failed to parse XML file '%s'. Tree failed to load. Bugstar streaming video is disabled.", filePath);

		return false;
	}

	parTreeNode* root = tree->GetRoot();

	if (root == nullptr)
	{
		Errorf("Failed to parse XML file '%s'. No root element. Bugstar streaming video is disabled.", filePath);

		delete tree;
		return false;
	}

	parElement& element = root->GetElement();

	char const* rootName = element.GetName();

	char const* rootTag		= "BugstarStreamingVideo";
	char const* baseUriTag	= "BaseUri";

	if (strcmp(rootName, rootTag) != 0)
	{
		Errorf("Failed to parse XML file '%s'. Missing root element '%s'. Bugstar streaming video is disabled.", filePath, rootTag);
		delete tree;
		return false;
	}

	for (parTreeNode* child = root->GetChild() ; child != nullptr ; child = child->GetSibling())
	{
		parElement& element = child->GetElement();

		char const* name = element.GetName();
		char const* data = child->GetData();

		if (strcmp(name, baseUriTag) == 0)
		{
			safecpy(sm_streamingVideoBaseUri, data);
			continue;
		}
	}

	// Check for compulsory values.

	if (*sm_streamingVideoBaseUri == '\0')
	{
		Errorf("Failed to parse XML file '%s'. Missing element '%s'. Bugstar streaming video is disabled.", filePath, baseUriTag);
		delete tree;
		return false;
	}

	Displayf("Loaded bugstar streaming video settings '%s'.", filePath);
	delete tree;

	return true;
}

bool CBugstarIntegration::IsStreamingVideo()
{

#if RSG_ORBIS
	return NetworkInterface::IsLiveStreamingEnabled();
#else
	return true; // Assume it's streaming video by default.
#endif 

}

char const* CBugstarIntegration::GetStreamingVideoMessage()
{
	if (PARAM_bugstarStreamingVideo.Get() == false)
	{
		return nullptr;
	}

	if (sm_streamingVideoErrorMessage != nullptr)
	{
		return sm_streamingVideoErrorMessage;
	}

	if (IsStreamingVideoEnabled() == false)
	{
		return "B* streaming video disabled";
	}
	
	if (IsStreamingVideo() == false)
	{
		return "Enable streaming";
	}

	return nullptr;
}

bool CBugstarIntegration::SetStreamingVideoSettings()
{
	if (IsStreamingVideo() == true)
	{
		Errorf("Cannot set bugstar streaming video setting while currently streaming video.");
		return false;
	}

	char guidStr[64];

	rage::atGuid guid = rage::atGuid::Generate();

	guid.GetAsString(guidStr, sizeof(guidStr), rage::sysGuidUtil::kGuidStringFormatNonDecorated);

	char forename[64];

	char* firstDot = strchr(m_sUsernameVis, '.');

	if (firstDot != nullptr)
	{
		int forenameLength = int(firstDot - m_sUsernameVis);

		memcpy(forename, m_sUsernameVis, forenameLength);

		forename[forenameLength] = '\0';
	}
	else
	{
		strcpy(forename, m_sUsernameVis);
	}

	snprintf(sm_streamingVideoId, sizeof(sm_streamingVideoId), "%s.Continuous.%s", forename, guidStr);

	sm_streamingVideoId[sizeof(sm_streamingVideoId) - 1] = '\0';

#if RSG_ORBIS
	return SetStreamingVideoSettingsOrbis();
#else 
	return true;
#endif

}

#if RSG_ORBIS
bool CBugstarIntegration::ValidateStreamingVideoSettingsOrbis()
{
	enum Comparison
	{
		COMPARISON_EQUALS			= 0,
		COMPARISON_GREATER_EQUALS,
	};

	struct OrbisSetting
	{
		char const* key;
		Comparison	comparison;
		int			value;
		char const* name;
	};

	// These are the orbis settings and their expected values that we are validating.

	static OrbisSetting const settings[] = 
	{
		{ "0x32010000",	COMPARISON_EQUALS,			0,	"Live Streaming Mode"	},
		{ "0x32020000",	COMPARISON_EQUALS,			0,	"Social Feedback Mode"	},
		{ "0x32890000",	COMPARISON_GREATER_EQUALS,	5,	"Live Quality: Debug"	},
	};

	bool foundSettings[COUNTOF(settings)];

	for (int index = 0 ; index < COUNTOF(foundSettings) ; ++index)
	{
		foundSettings[index] = false;
	}

	char commandline[256];

	char filePath[MAX_PATH];

	formatf(filePath, "%s\\ExportedOrbisSettings.xml", sm_streamingVideoRootDirectory);

	snprintf(commandline, sizeof(commandline), "orbis-ctrl.exe settings-export %s", filePath);

	int commandResult = sysExec(commandline);

	if (commandResult != 0)
	{
		Assertf(false, "Error: Failed to get orbis settings. command:'%s'", commandline);
		return false;
	}

	fiStream* stream = fiStream::Open(filePath, true);

	if (stream == nullptr)
	{
		Displayf("Failed to load '%s'. Bugstar streaming video is disabled.", filePath);
		return false;
	}

	// Set the parser settings to truncate long attributes as there are some very long names and descriptions
	// written out of orbis-ctrl that can exceed the maximum parcodegen attribute size.
	parSettings parserSettings = parSettings::sm_StandardSettings;

	parserSettings.SetFlag(parSettings::TRUNCATE_ATTRIBUTES, true);

	parStreamInXml parserStream(stream, parserSettings);
	
	parTree* tree = PARSER.LoadTree(&parserStream);

	if (tree == nullptr)
	{
		Errorf("Failed to parse XML file '%s'. Tree failed to load. Bugstar streaming video is disabled.", filePath);
		return false;
	}

	parTreeNode* root = tree->GetRoot();

	if (root == nullptr)
	{
		Errorf("Failed to parse XML file '%s'. No root element. Bugstar streaming video is disabled.", filePath);

		delete tree;
		return false;
	}

	parElement& element = root->GetElement();

	char const* rootName = element.GetName();

	char const* rootTag		= "settings";
	char const* settingTag	= "setting";
	char const*	keyAttr		= "key";
	char const*	valueAttr	= "value";

	if (strcmp(rootName, rootTag) != 0)
	{
		Errorf("Failed to parse XML file '%s'. Missing root element '%s'. Bugstar streaming video is disabled.", filePath, rootTag);
		delete tree;
		return false;
	}

	for (parTreeNode* child = root->GetChild() ; child != nullptr ; child = child->GetSibling())
	{
		parElement& element = child->GetElement();

		char const* name = element.GetName();

		if (strcmp(name, settingTag) != 0)
		{
			continue;
		}

		atArray<parAttribute> const& attributes = element.GetAttributes();

		int attributeCount = attributes.GetCount();

		// Get the key attribute.

		int foundSettingIndex = -1;

		for (int index = 0 ; index < attributeCount ; ++index)
		{
			parAttribute const& attr = attributes[index];

			char const* name = attr.GetName();

			if (stricmp(name, keyAttr) != 0)
			{
				continue;
			}

			// See if this key is one of the settings we are looking for.

			char const* currentKey = attr.GetStringValue();

			for (int settingIndex = 0 ; settingIndex < COUNTOF(settings) ; ++settingIndex)
			{
				if (foundSettings[settingIndex] == true)
				{
					continue;
				}

				OrbisSetting const& currentSetting = settings[settingIndex];

				if (stricmp(currentKey, currentSetting.key) != 0)
				{
					continue;
				}

				foundSettingIndex = settingIndex;
				break;
			}
			break;
		}

		if (foundSettingIndex == -1)
		{
			continue;
		}

		OrbisSetting const& currentSetting = settings[foundSettingIndex];

		int valueInt = -1;

		for (int index = 0 ; index < attributeCount ; ++index)
		{
			parAttribute const& attr = attributes[index];

			char const* name = attr.GetName();

			if (stricmp(name, valueAttr) != 0)
			{
				continue;
			}

			int type = attr.GetType();

			switch (type)
			{

			case parAttribute::INT64:
				{
					valueInt = attr.GetIntValue();
				}
				break;

			case parAttribute::STRING:
				{
					char const* value = attr.GetStringValue();

					if (strncmp(value, "0x", 2) == 0)
					{
						sscanf(value, "0x%x", &valueInt);
					}
					else
					{
						sscanf(value, "%i", &valueInt);
					}
				}
				break;

			default:
				{
					Errorf("Failed to parse value for orbis setting %s (%s). Bugstar streaming video is disabled.", currentSetting.key, currentSetting.name);
					delete tree;
				}
				return false;
			}

			break;
		}

		bool passed = false;

		switch (currentSetting.comparison)
		{

		case COMPARISON_EQUALS:
			passed = valueInt == currentSetting.value;
			break;

		case COMPARISON_GREATER_EQUALS:
			passed = valueInt >= currentSetting.value;
			break;

		}
	
		if (passed == false)
		{
			Errorf("Error: Orbis setting %s (%s) has incorrect value %i. Streaming video is disabled.", currentSetting.key, currentSetting.name, valueInt);
			delete tree;
			return false;
		}

		foundSettings[foundSettingIndex] = true;
	}

	for (int index = 0 ; index < COUNTOF(foundSettings) ; ++index)
	{
		if (foundSettings[index] == false)
		{
			Errorf("Error: Did not find orbis setting %s (%s). Streaming video is disabled.", settings[index].key, settings[index].name);
			delete tree;
			return false;
		}
	}

	Displayf("Validated orbis streaming video settings '%s'.", filePath);

	delete tree;

	return true;
}

void CBugstarIntegration::AddSettingOrbis(parTreeNode* rootNode, char const* key, char const* dest, char const* size, char const* type, char const* value, char const* eng)
{
	parTreeNode* node = rage_new parTreeNode("setting");

	parElement& element = node->GetElement();

	element.AddAttribute("key", key);
	element.AddAttribute("dest", dest);
	element.AddAttribute("size", size);
	element.AddAttribute("type", type);
	element.AddAttribute("value", value);
	element.AddAttribute("eng", eng);

	node->AppendAsChildOf(rootNode);
}

bool CBugstarIntegration::SetStreamingVideoSettingsOrbis()
{
	parTree *tree = rage_new parTree();

	parTreeNode* rootNode = rage_new parTreeNode("settings");

	tree->SetRoot(rootNode);

	parElement& rootElement = rootNode->GetElement();

	rootElement.AddAttribute("version", 1);
	rootElement.AddAttribute("target", "orbis");

	char url[256];

	formatf(url, "%s/%s", sm_streamingVideoBaseUri, sm_streamingVideoId);

	AddSettingOrbis(rootNode, "0x32030000", "0x00", "256", "0x1", url,		"Broadcast URL");
	AddSettingOrbis(rootNode, "0x37070000", "0x00",   "4", "0x0", "0x1",	"SNS Contents Sharing Test from Share Button");

	char filePath[MAX_PATH];

	formatf(filePath, "%s\\GameOrbisSettings.xml", sm_streamingVideoRootDirectory);

	PARSER.SaveTree(filePath, "", tree, rage::parManager::XML);

	delete tree;

	char commandline[256];

	snprintf(commandline, sizeof(commandline), "orbis-ctrl.exe settings-import %s", filePath);

	int commandResult = sysExec(commandline);

	Assertf(commandResult == 0, "Failed to execute command '%s'", commandline);

	if (commandResult == 0)
	{
		Displayf("Updated streaming video settings. url:'%s' command:'%s'", url, commandline);
	}
	else
	{
		Displayf("Error: Failed to update streaming video settings. command:'%s'", commandline);
	}

	return commandResult == 0;
}
#endif // RSG_ORBIS

//***********************************
// CBugstarComponentLookup
//***********************************

atArray<CBugstarComponentLookup::ComponentData> CBugstarComponentLookup::m_ComponentLookupData;
bool	CBugstarComponentLookup::m_Initialised = false;

void	CBugstarComponentLookup::Init()
{
	CreateComponentLookupTable();
}

void	CBugstarComponentLookup::Shutdown()
{
	m_ComponentLookupData.Reset();
	m_Initialised = false;
}

char	*CBugstarComponentLookup::GetComponentString(int id)
{
	for(int i=0;i<m_ComponentLookupData.size();i++)
	{
		if(m_ComponentLookupData[i].m_ID == id)
		{
			return m_ComponentLookupData[i].m_Name;
		}
	}
	return NULL;
}


// Create the component lookup table.
void CBugstarComponentLookup::CreateComponentLookupTable()
{
	USE_DEBUG_MEMORY();

	atArray<ComponentWorkData> workData;

	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sComponents",BUGSTAR_GTA_PROJECT_PATH);
	parTree* pTree = CBugstarIntegration::CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		parTreeNode* root = pTree->GetRoot();
		parTreeNode::ChildNodeIterator i = root->BeginChildren();
		for(; i != root->EndChildren(); ++i)
		{
			parTreeNode* id = (*i)->FindFromXPath("id");					// The component ID (number as a string)
			if(id)
			{
				ComponentWorkData thisData;

				parTreeNode *name = (*i)->FindFromXPath("name");			// The name of this component (as a string)
				parTreeNode *parentID = (*i)->FindFromXPath("parentId");	// The name of this components parent (number as a string)

				thisData.m_ID = atoi((char*)id->GetData());
				// ParentID is optional
				thisData.m_ParentID = 0;		// Let hope 0 is unused... suspect it is!
				if(parentID)
				{
					thisData.m_ParentID = atoi((char*)parentID->GetData());
				}
				strncpy(thisData.m_Name, (char*)name->GetData(), MAX_COMPONENT_NAME_LENGTH);
				workData.Grow() = thisData;
			}
		}
	}


	m_ComponentLookupData.Reset();

	for(int i=0;i<workData.size();i++)
	{
		ComponentWorkData *pData = &workData[i];

		ComponentData thisData;
		thisData.m_ID = pData->m_ID;

		int	nameIDX = 0;
		char constructedName[2][512];

		strcpy(constructedName[nameIDX], pData->m_Name);
		while( pData->m_ParentID != 0 )
		{
			nameIDX = 1-nameIDX;		// Toggle buffer to use
			pData = FindComponentData(workData, pData->m_ParentID);

			Assertf(pData, "Component Parent Doesn't Exist. It's going to go wrong :(");

			sprintf(constructedName[nameIDX],"%s : %s", pData->m_Name, constructedName[1-nameIDX]);
		}
		strncpy(thisData.m_Name, constructedName[nameIDX], MAX_COMPONENT_DESCRIPTION_LENGTH );
		m_ComponentLookupData.Grow() = thisData;
	}

	m_Initialised = true;
}

// Find a particular component from it's ID.
CBugstarComponentLookup::ComponentWorkData *CBugstarComponentLookup::FindComponentData(atArray<ComponentWorkData> &lookupData, int componentID)
{
	for(int i=0;i<lookupData.size();i++)
	{
		ComponentWorkData *pData = &lookupData[i];

		if(pData->m_ID == componentID)
		{
			return pData;
		}
	}
	return NULL;
}
//***********************************

//***********************************
// CBugstarStateLookup
//***********************************

atArray<CBugstarStateLookup::StateData> CBugstarStateLookup::m_StateLookupData;
bool	CBugstarStateLookup::m_Initialised = false;

void CBugstarStateLookup::Init()
{
	CreateStateLookupTable();
}

void CBugstarStateLookup::Shutdown()
{
	m_StateLookupData.Reset();
	m_Initialised = false;
}

void CBugstarStateLookup::CreateStateLookupTable()
{
	USE_DEBUG_MEMORY();

	m_StateLookupData.Reset();


	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sStates",BUGSTAR_GTA_PROJECT_PATH);
	parTree* pTree = CBugstarIntegration::CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		parTreeNode* root = pTree->GetRoot();
		parTreeNode::ChildNodeIterator i = root->BeginChildren();
		for(; i != root->EndChildren(); ++i)
		{
			parTreeNode* id = (*i)->FindFromXPath("name");						// The name of this state
			if(id)
			{
				StateData thisData;

				parTreeNode *description = (*i)->FindFromXPath("description");	// The name of this state (the thing that is printed in B*)

				thisData.m_Name = atHashString((char*)id->GetData());
				strncpy(thisData.m_Description, (char*)description->GetData(), MAX_DESCRIPTION_LENGTH);
				m_StateLookupData.Grow() = thisData;
			}
		}
	}

	m_Initialised = true;
}

char *CBugstarStateLookup::GetDescriptionFromState(char *state)
{
	atHashString stateHash(state);

	for(int i=0;i<m_StateLookupData.size();i++)
	{
		if( m_StateLookupData[i].m_Name == stateHash )
		{
			return m_StateLookupData[i].m_Description;
		}
	}
	return NULL;
}
//***********************************

//***********************************
// CBugstarUserLookup
//***********************************

atArray<CBugstarUserLookup::UserData> CBugstarUserLookup::m_UserLookupData;
bool	CBugstarUserLookup::m_Initialised = false;

void CBugstarUserLookup::Init()
{
	CreateUserLookupTable();
}

void CBugstarUserLookup::Shutdown()
{
	m_UserLookupData.Reset();
	m_Initialised = false;
}

void CBugstarUserLookup::CreateUserLookupTable()
{
	USE_DEBUG_MEMORY();

	m_UserLookupData.Reset();

	char search[BUG_LOGIN_SEARCH_LENGTH];
	sprintf(search,"%sUsers",BUGSTAR_GTA_PROJECT_PATH);
	parTree* pTree = CBugstarIntegration::CreateXmlTreeFromHttpQuery(search);
	if(pTree)
	{
		parTreeNode* root = pTree->GetRoot();
		parTreeNode::ChildNodeIterator i = root->BeginChildren();
		for(; i != root->EndChildren(); ++i)
		{
			parTreeNode* id = (*i)->FindFromXPath("id");						// userID
			if(id)
			{
				UserData thisData;

				parTreeNode *userName = (*i)->FindFromXPath("username");		// The name of this user

				thisData.m_Name = atHashString((char*)userName->GetData());
				thisData.m_ID = atoi((char*)id->GetData());
				m_UserLookupData.Grow() = thisData;
			}
		}
	}
	m_Initialised = true;
}

u32	CBugstarUserLookup::GetUserIDFromNameHash(u32 hash)
{
	for(int i=0;i<m_UserLookupData.size();i++)
	{
		UserData &thisUserData = m_UserLookupData[i];
		if(thisUserData.m_Name.GetHash() == hash)
		{
			return thisUserData.m_ID;
		}
	}
	return 0;
}




#endif //BUGSTAR_INTEGRATION_ACTIVE

