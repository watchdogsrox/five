//
// filename:	BugstarIntegration.h
// description: Allow the game to inteact with a bugstar client over the network
// 
// This allows the game to retieve lists of bugs matching specified constraints 
// in addition to the usual add bug facility.
//
//

#ifndef INC_BUGSTAR_INTEGRATION_H_
#define INC_BUGSTAR_INTEGRATION_H_

// --- Include Files ------------------------------------------------------------

// C headers

// Bugstar Headers
#include "./bugstar/BugstarIntegrationSwitch.h"
#include "./bugstar/BugstarAssetLookup.h"
#include "./bugstar/MinimalBug.h"


// Rage headers
#include "atl/array.h"
#include "atl/map.h"
#include "atl/string.h"
#include "data/growbuffer.h"
#include "vector/color32.h"
#include "vector/colors.h"
#include "vector/vector3.h"
#include "system/messagequeue.h"
#include "net/http.h"
#include "fwnet/HttpQuery.h"

// --- Defines ------------------------------------------------------------------


#if ( BUGSTAR_INTEGRATION_ACTIVE )

#define MAX_COORDINATE_STRING_LENGTH 255
#define MAX_SIZE_STRING_LENGTH 20
#define MAX_BUG_AREA_SIZE 1000
#define MAX_BUG_MARKER_SIZE 20

#define BUG_NUMBER_STRING_LENGTH 20
#define BUG_SUMMARY_STRING_LENGTH 255
#define BUG_LOGIN_STRING_LENGTH 64
#define BUG_LOGIN_SEARCH_LENGTH 1024

// See https://rsgedibgs8.rockstar.t2.corp:8443/BugstarRestService-1.0/rest/Projects/ for project ID's

#define BUGSTAR_GTA_PROJECT_ID   "282246"

#define BUGSTAR_GTA_PROJECT_PATH "Projects/" BUGSTAR_GTA_PROJECT_ID "/"

#define MAX_ASSERT_QUERIES 8

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------


// Forward declare Rage classes
namespace rage {
	class parTree;
	class parTreeNode;
	class sysNamedPipe;
}

// Core Forward Declarations
class CLocalBugsSettings;

// class for managing
class CSavedBugstarRequestList
{
public:
	void Init();
	class parTree* LoadBugstarRequest(const char* pLocalFilename);

	void Get(const char* pLocalFilename, const char* pRequest, int renewTimeInMinutes=60, int bufferSize=32*1024);
	void PostXml(const char* pLocalFilename, const char* pRequest, const char* pXml, int xmlLength, int renewTimeInMinutes=60, int bufferSize=32*1024);
};

// class for processing the bugstar assert updates
class CBugstarAssertUpdateQuery
{
public:
	struct BugDesc {
		int assertId;
		char* logBuffer;
		int logSize;
		Vector3 position;
		int buildId;
		int mapId;
		int missionId;
		float timeSinceLastBug;
#if __WIN32PC
		char* dxDiagBuffer;
		int dxDiagSize;
#endif
	};

	CBugstarAssertUpdateQuery(const char* errorMsg, const char* detailedErrorMsg, const char* callstack, const char* gamerTag, const BugDesc& bug, bool showBug);
	~CBugstarAssertUpdateQuery();

	void AddQueryToQueue();

	static void InitAssertQueryQueue();

private:
	const atString m_errorMsg;
	const atString m_detailedErrorMsg;
	const atString m_gamerTag;
	const atString m_callstack;
	const BugDesc m_bugDesc;
	bool m_showBug;
	CBugstarAssertUpdateQuery* m_pNextAssert;

	static atMap<int, bool> sm_assertIdAlreadyAdded;
	static sysMessageQueue<CBugstarAssertUpdateQuery*, MAX_ASSERT_QUERIES> sm_messageQueue;

	void SetNextAssert(CBugstarAssertUpdateQuery* pNextAssert) {m_pNextAssert = pNextAssert;}
	void Execute();
	int AddBug();
	void GetSubsequentBugs();
	void CreateBugDescription(char* pBuffer, int bufferSize, const char* logName);
#if __WIN32PC
	bool WriteDxDiagLog(int bugId, int seen);
#endif

	static void BugstarAssertUpdateThread(void* );
	static bool sm_BugstarWorking;
};


// Needed to be able to resolve component ID's to their associated strings
class CBugstarComponentLookup
{
public:

#define MAX_COMPONENT_NAME_LENGTH			(64)		// The longest name of an individual component
#define MAX_COMPONENT_DESCRIPTION_LENGTH	(128)		// The longest name of the entire heirarchy of a component (all parents pre-pended)

	static void		Init();
	static void		Shutdown();
	static char		*GetComponentString(int id);

	static bool		IsInitialised() { return m_Initialised; }

private:

	static void		CreateComponentLookupTable();

	typedef struct
	{
		int m_ID;
		char m_Name[MAX_COMPONENT_DESCRIPTION_LENGTH];
	}	ComponentData;

	static atArray<ComponentData> m_ComponentLookupData;

	// Used for creating the lookup data
	typedef struct
	{
		int	m_ID;
		int m_ParentID;
		char m_Name[MAX_COMPONENT_NAME_LENGTH];
	}	ComponentWorkData;
	static ComponentWorkData *FindComponentData(atArray<ComponentWorkData> &lookupData, int componentID);
	// Used for creating the lookup data

	static bool		m_Initialised;
};


// Needed to be able to resolve the state member to strings like "Test (Need More Info)"
class CBugstarStateLookup
{
public:

#define MAX_DESCRIPTION_LENGTH	(64)

	static void		Init();
	static void		Shutdown();

	static char *GetDescriptionFromState(char *state);

	static bool		IsInitialised() { return m_Initialised; }

private:

	static void		CreateStateLookupTable();

	typedef struct
	{
		atHashString	m_Name;		// Hash this, to improve lookup
		char			m_Description[MAX_DESCRIPTION_LENGTH];
	}	StateData;

	static atArray<StateData> m_StateLookupData;

	static bool			m_Initialised;
};


class CBugstarUserLookup
{
public:
	static void		Init();
	static void		Shutdown();
	static bool		IsInitialised() { return m_Initialised; }
	static void		CreateUserLookupTable();
	static u32		GetUserIDFromNameHash(u32 hash);

private:

	typedef struct
	{
		atHashString	m_Name;		// Hash of username
		u32				m_ID;		// Bugstar user ID
	}	UserData;

	static atArray<UserData> m_UserLookupData;
	static bool			m_Initialised;
};


class CBugstarIntegration
{
public:
	static void		Init( );
	static void		Shutdown( );

	static const char* GetRockstarTargetManagerConfig();

	static atArray<CMinimalBug>& GetCurrentBugList();
	static atArray<CMinimalBug*>& GetFilteredBugList();
	static void RenderTheBuglist( );
	static void RenderMarker(float x,float y, float z,float r,Color32 colour,bool highlight = false,const char* label = NULL);
	static void RenderBug(CMinimalBug* bug,bool highlight = false);

	static bool PassesStatusFilter(CMinimalBug *pBug);
	static bool PassesComponentFilter(CMinimalBug *pBug);
	static bool PassesOwnerFilter(CMinimalBug *pBug);

	static void UpdateFilteredBugList();

	static parTree* CreateXmlTreeFromHttpQuery(const char* search);
	static parTree* CreateXmlTreeFromHttpQuery(const char* search, const char* username, const char* password);
	static parTree* CreateXmlTreeFromHttpQuery(const char* search, const char* postData);
	static parTree* CreateXmlTreeFromHttpQuery(const char* search, const char* postData, const char* username, const char* password);
	static bool PostHttpQuery(const char* search, const char* postData, const char* username, const char* password);

#if __BANK
	static void		InitWidgets(bkBank& bank);
	static void		InitLiveEditWidgets(bkBank& bank);
	static void		LoginAndClear( );
#endif

	static void		LoadDevBugList( );
	static void		LoadBugNumber( );

	static bool		LoadBug(int bugNumber);

	static bool		LoadBugList();	
	static void		AreaCenter( );
	static void		AreaSize( );

	static void		UpdateSelectedBugDetails();
	static void		NextBug( );
	static void		PreviousBug( );
	static void		SelectNearestBug();
	static void		WarpToSelectedBug();
	static void		WarpToBug(CMinimalBug* bug );
	static void		AddScreenshot();
	static bool		IsTakingScreenshot() { return (m_iScreenshotFrameDelay > 0); }
	static void		ProcessScreenShot();
	static bool		GetStreamingVideoId(char* id, size_t idSize);

	static int		GetBugCountForAssertId(unsigned int assertId);
	static int		CreateNewAssertBug(const char* summary, const char* description, const CBugstarAssertUpdateQuery::BugDesc& bugDesc);
	static parTree*	GetMostRecentOpenBug(unsigned int assertId);
	static bool		PostBugChanges(int bugId, parTree* pTree);
	static bool		PostBugFieldChange(int bugId, const char* field, const char* value);
	static bool		ChangeField(int bugId, const char* field, const char* value);
	static int		IncrementBugSeenCount(int bugId, int buildId, rage::parTree* pBugTree);
	static bool		AddComment(int bugId, const char* pComment);
	static bool		ConvertToMinimalBug(rage::parTree* pBugTree, CMinimalBug& bug);

	static fwHttpQuery*				CreateHttpQuery();
	static fwHttpQuery*				CreateHttpQuery(const char* username, const char* password);
	static CHttpQuerySaveToFile*	CreateSaveHttpQuery(const char* filename);

	static CSavedBugstarRequestList& GetSavedBugstarRequestList() {return m_savedBugstarRequestList;}
	static const CBugstarAssetLookup& GetBuildLookup() {return sm_buildLookup;}
	static const CBugstarAssetLookup& GetMapLookup() {return sm_mapLookup;}

	static bool		AddConsoleLogsToBug(void);

	// PURPOSE: Reports whether Bugstar streaming video is enabled or not.
	static bool			IsStreamingVideoEnabled() { return sm_isStreamingVideoEnabled; }

	// PURPOSE: Return a status message that's displayed on the screen.
	static char const*	GetStreamingVideoMessage();

	// PURPOSE: Sets the streaming video settings for Bugstar usage.
	static bool			SetStreamingVideoSettings();

	// PURPOSE: Queries the hardware platform to see if it's actually streaming a video.
	// If it cannot be queried then it's assumed it is.
	static bool			IsStreamingVideo();

protected:
	static atArray<CMinimalBug> m_Buglist;
	static atArray<CMinimalBug*> m_FilteredBugList;
	static CSavedBugstarRequestList m_savedBugstarRequestList;

	typedef struct
	{
		const char *name;
		bool bInclude;
	}	StatusFilter;

	static StatusFilter	m_StatusFilters[];

	typedef struct
	{
		const char *name;
		bool bInclude;
	}	ComponentFilter;	// Currently similar to StatusFilter, but may change so kept separate

	static ComponentFilter	m_ComponentFilters[];

	static bool		m_FilterByCurrentUser;

	static atHashString	m_CurrentUsername;
	static s32			m_CurrentUserID;

	static float	m_fAreaSize;
	static float	m_fBugMarkerSize;
	static bool		m_bBugMarkersOn;
	static bool		m_bBugClassFilters[CMinimalBug::kBugCategoryNum];
	static char		m_sFetchBugNumber[BUG_NUMBER_STRING_LENGTH];
	static int		m_iSelectedBugIndex;
	static int		m_iBugCount;
	static char**	m_pBugStringList;
	static char		m_sBugNumber[BUG_NUMBER_STRING_LENGTH];
	static char		m_sBugSummary[BUG_SUMMARY_STRING_LENGTH];
	static char		m_sUsername[BUG_LOGIN_STRING_LENGTH];
	static char		m_sPassword[BUG_LOGIN_STRING_LENGTH];
	static char		m_sAssertUsername[BUG_LOGIN_STRING_LENGTH];
	static char		m_sAssertPassword[BUG_LOGIN_STRING_LENGTH];
	static bool		m_bProcessScreenshot;
	static int		m_iScreenshotFrameDelay;
	static bool		m_bDisableSpaceBugScreenshots;
	static char		m_sUsernameVis[BUG_LOGIN_STRING_LENGTH];
	static char		m_sPasswordVis[BUG_LOGIN_STRING_LENGTH];

	static char		m_sAreaCenter[MAX_COORDINATE_STRING_LENGTH];

	static parTree* PerformFulltextSearch(unsigned int assertId, bool countOnly=true);
	static parTree* GetBug(unsigned int bugId);

	static int		GetCurrentUserId();
	static int		GetUserByName(const char* username);
	static int		GetTeamByName(const char* username);

private:

	// PURPOSE: Initialises the streaming video settings for Bugstar.
	static void		InitStreamingVideo();

	static bool		ValidateStreamingVideoSettings();

	static bool		LoadStreamingVideoSettings();

#if RSG_ORBIS
	static void		AddSettingOrbis(parTreeNode* rootNode, char const* key, char const* dest, char const* size, char const* type, char const* value, char const* eng);
	static bool		SetStreamingVideoSettingsOrbis();
	static bool		ValidateStreamingVideoSettingsOrbis();
#endif // RSG_ORBIS

	static int		sm_currentUserId;
	static atMap<atHashString, int> sm_teamIdMap;
	static CBugstarAssetLookup sm_buildLookup;
	static CBugstarAssetLookup sm_mapLookup;
	static char sm_streamingVideoId[128];
	static char sm_streamingVideoBaseUri[128];
	static char const* sm_streamingVideoRootDirectory;
	static char const* sm_streamingVideoErrorMessage;
	static bool sm_isStreamingVideoEnabled;
};

// --- Globals ------------------------------------------------------------------

#endif // BUGSTAR_INTEGRATION_ACTIVE

#endif //INC_BUGSTAR_INTEGRATION_H_


