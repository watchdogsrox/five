
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    script.h
// PURPOSE : Scripting and stuff.
// AUTHOR :  Obbe. A big thanks to Brian Baird. Most of this stuff is based on GTA2's
//			 scripting.
// CREATED : 12-11-99
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCRIPT_H_
	#define _SCRIPT_H_

// Rage headers
#include "parser/tree.h"
#include "rline/ugc/rlugccommon.h"	//	For RLUGC_MAX_CONTENTID_CHARS
#include "script/thread.h"
#include "vector/vector3.h"

// Framework headers
#include "fwscene/Ipl.h"
#include "fwscript/scriptguid.h"

// Game headers
#include "debug/DebugRecorder.h"
#include "scene/RegdRefTypes.h"
#include "script/gta_thread.h"
#include "script/handlers/GameScriptMgr.h"

#define NULL_IN_SCRIPTING_LANGUAGE	(0)		//	was -1 in SA - is 0 in Rage scripting language
#define DUMMY_ENTRY_IN_MODEL_ENUM_FOR_SCRIPTING_LANGUAGE	(0)		//	Rage script compiler complains if a command that expects a model enum entry is passed -1 or NULL so we have a special entry (= 0) in model_enums.sc

#define INVALID_MINIMUM_WORLD_Z (-200.0f)	//if changed please change the define in commands_entity.sch.

// Forward declarations
class CItemSet;
class CGameScriptHandlerNetwork;
class CScriptBrainDispatcher;
class CScriptsForBrains;
class sveFile;

class CMissionReplayStats
{
public:
	void Init(unsigned initMode);

//	When a Replay Mission is passed
	void BeginReplayStatsStructure(s32 missionId, s32 missionType);
	void AddStatValueToReplayStatsStructure(s32 valueOfStat);
	void EndReplayStatsStructure();
//	End of "When a Replay Mission is passed"

//	When a Load happens and the script detects that it’s a Replay save file
	bool HaveReplayStatsBeenStored() { return m_bStatsHaveBeenStored; }
	s32 GetReplayStatMissionId() { return m_nMissionId; }
	s32 GetReplayStatMissionType() { return m_nMissionType; }
	s32 GetNumberOfMissionStats();
	s32 GetStatValueAtIndex(s32 arrayIndex);
	void ClearReplayStatsStructure();
//	End of "When a Load happens and the script detects that it’s a Replay save file"

#if __ASSERT
	bool IsReplayStatsStructureBeingConstructed() { return m_bConstructingStructureOfStats; }
#endif	//	__ASSERT

private:
	bool m_bStatsHaveBeenStored;
	bool m_bConstructingStructureOfStats;
	s32 m_nMissionId;
	s32 m_nMissionType;

	atArray<s32> m_ArrayOfMissionStats;
};

class CHiddenObjects
{
	struct hidden_object_struct
	{
		spdSphere m_SearchVolume;
		s32 m_ModelHash;

		RegdDummyObject m_pRelatedDummyForObjectGrabbedFromTheWorld;
		fwInteriorLocation m_InteriorLocationOfRelatedDummyForObjectGrabbedFromTheWorld;
		s32 m_IplIndexOfObjectGrabbedFromTheWorld;
	};

public:
	void Init();
	void AddToHiddenObjectMap(s32 ScriptGUIDOfEntity, spdSphere &SearchVolume, s32 ModelHash, 
		CDummyObject *pDummyObj, fwInteriorLocation DummyInteriorLocation, s32 IplIndex);

	void ReregisterInHiddenObjectMap(s32 oldScriptGUIDOfEntity, s32 newScriptGUIDOfEntity);

	bool RemoveFromHiddenObjectMap(s32 ScriptGUIDOfEntity, bool bHandleRelatedDummy);
	void UndoHiddenObjects();

private:
	atMap<s32, hidden_object_struct> m_MapOfHiddenObjects;

	CDummyObject *FindClosestMatchingDummy(hidden_object_struct *pHideData, const CObject *pObject);

	//	Debug function to print reason for failure of FindClosestMatchingDummy
	void DisplayFailReason(const char *pFailReason, const hidden_object_struct *pHideData, const CObject *pObject);

#if !__NO_OUTPUT
	void DisplayInteriorDetails(fwInteriorLocation &interiorLoc);
#endif	//	!__NO_OUTPUT
};


#define MAX_NUMBER_OF_PATROL_NODES (16)
#define MAX_NUMBER_OF_PATROL_LINKS (64)

class CScriptPatrol
{
public:
	void Init();

	void OpenPatrolRoute(const char* RouteName);
	void ClosePatrolRoute();

	void AddPatrolNode(int NodeId, const char* NodeType, Vector3 &vNodePos, Vector3 &vNodeHeading, int Duration);
	void AddPatrolLink(int NodeID1, int NodeID2);

	void CreatePatrolRoute();

private:
	int ScriptPatrolNodeIndex;
	int ScriptPatrolLinkIndex; 
	bool bPatrolRouteOpen;
	fwIplPatrolNode ScriptPatrolNodes[MAX_NUMBER_OF_PATROL_NODES];
	fwIplPatrolLink ScriptPatrolLinks[MAX_NUMBER_OF_PATROL_LINKS];
	int ScriptPatrolNodeIDs[MAX_NUMBER_OF_PATROL_NODES];
	u32 ScriptPatrolRouteHash;
};

/*
class CPersistentScriptGlobals
{
private:
	int m_nSizeOfPersistentGlobalsBuffer;			//	measured in bytes - needs to be saved
	u8 *m_pBackupOfPersistentGlobalsBuffer;		//	should probably be set to NULL when loading a saved game
	int m_nOffsetToStartOfPersistentGlobalsBuffer;	//	needs to be saved

public:

	void Init();

	void Clear();	//	free the buffer if the pointer is non-NULL
	void RegisterPersistentGlobalVariables(int *pStartOfPersistentGlobals, int nSizeOfBuffer);	//	Within this function, calculate offset

	bool StorePersistentGlobalsInBackupBuffer();
	bool RestorePersistentGlobalsFromBackupBuffer();
};
*/

//
// name: CScriptOp
// description: Base class for temporary script objects that may exist between frames but will need to be removed if the game is shutdown. 
//				eg Async operations that are started and need to be flushed if the game is restarted, When the game is restart all active 
//				CScriptOps are flushed
class CScriptOp
{
public:
	virtual ~CScriptOp() {}
	virtual void Flush() = 0;

	inlist_node<CScriptOp> m_node;
};

//
// name: CScriptOpList
// description: List of temporary script objects that need cleanup when the game restarts
class CScriptOpList
{
public:
	typedef inlist<CScriptOp, &CScriptOp::m_node> LinkList;
	void AddOp(CScriptOp* operation) {
		m_scriptOps.push_front(operation);
	}
	void RemoveOp(CScriptOp* operation) {
		m_scriptOps.erase(operation);
	}
	void Flush() {
		// flush function assumes ops remove themselves from the list
		while(!m_scriptOps.empty())
			m_scriptOps.begin()->Flush();
	}
private:
	LinkList m_scriptOps;
};

class CTheScripts
{
public:

	// these are used as parameters to the functions that get an entity ptr from a GUID (GetPedFromScriptGUID, etc)
	enum 
	{
		GUID_ASSERT_FLAG_ENTITY_EXISTS	= BIT(0),	// assert if there is no entity
		GUID_ASSERT_FLAG_DEAD_CHECK		= BIT(1),	// assert if the entity has not been checked for dead
		GUID_ASSERT_FLAG_NOT_CLONE		= BIT(2),	// assert if the entity is a network clone
		GUID_ASSERT_LAST_FLAG			= BIT(3)
	};

	static const unsigned GUID_ASSERT_FLAGS_ALL = GUID_ASSERT_LAST_FLAG-1; 
	static const unsigned GUID_ASSERT_FLAGS_NO_DEAD_CHECK = GUID_ASSERT_FLAGS_ALL & (~GUID_ASSERT_FLAG_DEAD_CHECK); 
	static const unsigned GUID_ASSERT_FLAGS_INCLUDE_CLONES = GUID_ASSERT_FLAGS_ALL & (~GUID_ASSERT_FLAG_NOT_CLONE); 
	static const unsigned GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK = GUID_ASSERT_FLAGS_ALL & (~(GUID_ASSERT_FLAG_NOT_CLONE | GUID_ASSERT_FLAG_DEAD_CHECK));

public:
// Public functions

// Accessor functions for private data
	static CGameScriptHandlerMgr& GetScriptHandlerMgr() { return ms_ScriptHandlerMgr; }

	static CScriptsForBrains& GetScriptsForBrains() { return ms_ScriptsForBrains; }
	static CScriptBrainDispatcher& GetScriptBrainDispatcher() { return ms_ScriptBrainDispatcher; }

	static CMissionReplayStats &GetMissionReplayStats() { return ms_MissionReplayStats; }

	static CHiddenObjects &GetHiddenObjects() { return ms_HiddenObjects; }

	static CScriptPatrol& GetScriptPatrol() { return ms_ScriptPatrol; }

	static parTree *GetXmlTree() { return ms_pXmlTree; }
	static void SetXmlTree(parTree *pTree) { ms_pXmlTree = pTree; }

	static parTreeNode *GetCurrentXmlTreeNode() { return ms_pCurrentXmlTreeNode; }
	static void SetCurrentXmlTreeNode(parTreeNode *pTreeNode) { ms_pCurrentXmlTreeNode = pTreeNode; }

	static bool GetUpdatingTheThreads() { return ms_bUpdatingScriptThreads; }

	static void SetProcessingTheScriptsDuringGameInit(bool bProcessingDuringInit) { ms_bProcessingTheScriptsDuringGameInit = bProcessingDuringInit; }
	static bool GetProcessingTheScriptsDuringGameInit() { return ms_bProcessingTheScriptsDuringGameInit; }

	static bool GetPlayerIsOnAMission(void);
	static void SetPlayerIsOnAMission(bool bPlayerOnMission) { ms_bPlayerIsOnAMission = bPlayerOnMission; }

	static bool GetPlayerIsOnARandomEvent() { return ms_bPlayerIsOnARandomEvent; }
	static void SetPlayerIsOnARandomEvent(bool bPlayerOnRandomEvent) { ms_bPlayerIsOnARandomEvent = bPlayerOnRandomEvent; }

	static bool IsPlayerPlaying(CPed *pPlayerPed);

	static void SetPlayerIsInAnimalForm(bool bPlayerInAnimalForm) { ms_bPlayerIsInAnimalForm = bPlayerInAnimalForm; }
	static bool GetPlayerIsInAnimalForm() { return ms_bPlayerIsInAnimalForm; }

	static bool GetPlayerIsRepeatingAMission() { return ms_bPlayerIsRepeatingAMission; }
	static void SetPlayerIsRepeatingAMission(bool bRepeatingAMission) { ms_bPlayerIsRepeatingAMission = bRepeatingAMission; }

	static bool GetIsInDirectorMode() { return ms_bIsInDirectorMode; }
	static void SetIsInDirectorMode(bool bVal) { ms_bIsInDirectorMode = bVal; }

	static bool GetIsDirectorModeAvailable() { return ms_directorModeAvailable; }
	static void SetDirectorModeAvailable(bool const c_val) { ms_directorModeAvailable = c_val; }

	static int GetNumberOfMiniGamesInProgress() { return ms_NumberOfMiniGamesInProgress; }
	static void SetNumberOfMiniGamesInProgress(int NumberOfMiniGames) { ms_NumberOfMiniGamesInProgress = NumberOfMiniGames; }

	static bool GetAllowGameToPauseForStreaming() { return ms_bAllowGameToPauseForStreaming; }
	static void SetAllowGameToPauseForStreaming(bool bAllowPauseForStreaming) { ms_bAllowGameToPauseForStreaming = bAllowPauseForStreaming; }

	static bool GetScenarioPedsCanBeGrabbedByScript() { return ms_bScenarioPedsCanBeGrabbedByScript; }
	static void SetScenarioPedsCanBeGrabbedByScript(bool bScenarioPedsCanBeGrabbed) { ms_bScenarioPedsCanBeGrabbedByScript = bScenarioPedsCanBeGrabbed; }

	static bool GetCodeRequestedAutoSave() { return ms_bCodeRequestedAutosave; }
	static void SetCodeRequestedAutoSave(bool bRequestAutosave) { ms_bCodeRequestedAutosave = bRequestAutosave; }

	static s32 GetCustomMPHudColor() { return ms_customMPHudColor; }
	static void SetCustomMPHudColor(s32 customHudColor) {ms_customMPHudColor = customHudColor; }

#if !__PPU
	static void SetSinglePlayerRestoreHasJustOccurred(bool bRestoreJustOccurred);
	static bool GetSinglePlayerRestoreHasJustOccurred();
#endif

	static void SetContentIdOfCurrentUGCMission(const char *pContentIdString);
	static const char *GetContentIdOfCurrentUGCMission() { return ms_ContentIdOfCurrentUGCMission; }

#if __BANK
	static bool DisplayScriptProcessMessagesWhileScreenIsFaded() { return (sm_fTimeFadedOut > 30.0f); }
#endif	//	__BANK

	static void	Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();

	static bool ShouldBeProcessed();

	static void	Process(void);
	static void	InternalProcess(void);	// Update the scripts. Execute Commands etc. 
	
	static void RegisterEntity(CPhysical* pEntity, bool bScriptHostObject=false, bool bNetworked=false, bool bNewEntity=true); // registers an entity with the script handler code
	static void UnregisterEntity(CPhysical* pEntity, bool bCleanupScriptState); // if bCleanupScriptState is set, the entity will be reverted back to an ambient entity

	static bool HasEntityBeenRegisteredWithCurrentThread(const CPhysical* pPhysical);
	static int GetIdForEntityRegisteredWithCurrentThread(const CPhysical* pPhysical);

	static CEntity** GetAllScriptEntities(u32& numEntities); // returns an array of CEntity, and the number of entities in it

	static void GtaFaultHandler(const char *CauseOfFault, scrThread*, scrThreadFaultID);
	// KS - WR 156 - added stacksize argument
	static scrThreadId GtaStartNewThreadOverride(const char *progname,const void *argStruct,int argStructSize, int stackSize);
	static scrThreadId GtaStartNewThreadWithNameHashOverride(atHashValue hashOfProgName,const void *argStruct,int argStructSize, int stackSize);
	static scrThreadId GtaStartNewThreadWithProgramId(scrProgramId progId,const void *argStruct,int argStructSize,int stackSize, const char* progname);

	static GtaThread *GetCurrentGtaScriptThread(void);
	static u32 GetCurrentScriptHash(void);
	static const char *GetCurrentScriptName(void);
#if !__NO_OUTPUT
	static const char *GetCurrentScriptNameAndProgramCounter(void);
#endif // !__NO_OUTPUT

	static CGameScriptHandler *GetCurrentGtaScriptHandler(void);
	static CGameScriptHandlerNetwork *GetCurrentGtaScriptHandlerNetwork(void);

	static void PrioritizeRequestsFromMissionScript(s32 &streamingFlags);
	
	// player id conversion and access:
	static CPed*			FindLocalPlayerPed( int playerIndex, bool bAssert = true );
	static CPed*			FindNetworkPlayerPed( int playerIndex, bool bAssert = true );
	static CNetGamePlayer*	FindNetworkPlayer( int playerIndex, bool bAssert = true );

	static bool GetClosestObjectCB(CEntity* pEntity, void* data);
	static bool CountMissionEntitiesInAreaCB(CEntity* pEntity, void* data);
	static bool IsMissionEntity(CEntity* pEntity);

#if __ASSERT
	static bool ExportGamerHandle(const rlGamerHandle* phGamer, int& handleData, int sizeOfData, const char* szCommand);
	static bool ImportGamerHandle(rlGamerHandle* phGamer, int& handleData, int sizeOfData, const char* szCommand);
	static bool ImportGamerHandle(rlGamerHandle* phGamer, u8* pData, const char* szCommand);
#else
	static bool ExportGamerHandle(const rlGamerHandle* phGamer, int& handleData, int sizeOfData);
	static bool ImportGamerHandle(rlGamerHandle* phGamer, int& handleData, int sizeOfData);
	static bool ImportGamerHandle(rlGamerHandle* phGamer, u8* pData);
#endif // __ASSERT

#if __DEV
	static void ClearAllEntityCheckedForDeadFlags(void);

#endif

#if __ASSERT
	static bool IsValidGlobalVariable(int *AddressToTest);
	static void DoEntityModifyChecks(const fwEntity* pEntity, unsigned assertFlags);
#endif
#if __BANK
	static const char *GetPopTypeName(ePopType PopType);
#endif

	// *** call this from script commands that can modify the state of an entity ***
	template <class T>
	static T* GetEntityToModifyFromGUID(int guid, unsigned ASSERT_ONLY(assertFlags) = GUID_ASSERT_FLAGS_ALL)
	{
		T* pEntity = fwScriptGuid::FromGuid<T>(guid);

		ASSERT_ONLY(DoEntityModifyChecks(pEntity, assertFlags));
		DR_DEV_ONLY(debugPlayback::TrackScriptGetsEntityForMod(pEntity));
		return pEntity;
	}

	// *** call this from script commands that can modify the state of an entity where we don't want to track the event (for instance check for dead)***
	template <class T>
	static T* GetEntityToModifyFromGUID_NoTracking(int guid, unsigned ASSERT_ONLY(assertFlags) = GUID_ASSERT_FLAGS_ALL)
	{
		T* pEntity = fwScriptGuid::FromGuid<T>(guid);
		ASSERT_ONLY(DoEntityModifyChecks(pEntity, assertFlags));
		return pEntity;
	}

	static fwExtensibleBase* GetExtensibleBaseToModifyFromGUID(int guid, const char* commandName = NULL, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL);
	static CItemSet*		GetItemSetToModifyFromGUID(int guid, const char* commandName = NULL, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL);
	static CScriptedCoverPoint* GetScriptedCoverPointToModifyFromGUID(int guid, const char* commandName = NULL, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL);

	// *** call this from script commands that only query the state of an entity ***
	template <class T>
	static const T* GetEntityToQueryFromGUID(int guid, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL)
	{
		const T* pEntity = fwScriptGuid::FromGuid<T>(guid);
		assertFlags &= (~GUID_ASSERT_FLAG_NOT_CLONE);
		ASSERT_ONLY(DoEntityModifyChecks(pEntity, assertFlags));
		return pEntity;
	}

	static const fwExtensibleBase* GetExtensibleBaseToQueryFromGUID(int guid, const char* commandName = NULL, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL);
	static const CItemSet*	GetItemSetToQueryFromGUID(int guid, const char* commandName = NULL, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL);
	static const CScriptedCoverPoint* GetScriptedCoverPointToQueryFromGUID(int guid, const char* commandName = NULL, unsigned assertFlags = GUID_ASSERT_FLAGS_ALL);

	static int				GetGUIDFromEntity(fwEntity& entity);
	static int				GetGUIDFromExtensibleBase(fwExtensibleBase& ext);
	static int				GetGUIDFromPreferredCoverPoint(CScriptedCoverPoint& preferredCoverPoint);
#if !__FINAL
	static const char* 		GetScriptTaskName( s32 iCommandIndex );
	static const char* 		GetScriptStatusName( u8 iScriptStatus );
	static bool				GetScriptLaunched() { return ms_bScriptLaunched; }
#endif //!__FINAL

	static CScriptOpList& GetScriptOpList() {return ms_scriptOpList;}

	static sysTimer& GetScriptTimer() { return sm_ScriptTimer; }

	__forceinline static bool IsFracked() {return !!(ms_Frack & 0x1);}
	__forceinline static void UnFrack() { ms_Frack &= ~0x1; }
	__forceinline static bool Frack() {FastAssert(scrThread::GetActiveThread());ms_Frack |= !scrThread::GetActiveThread(); return true;}
	__forceinline static bool Frack2() {FastAssert(scrThread::IsThisThreadRunningAScript());ms_Frack |= !scrThread::IsThisThreadRunningAScript(); return true;}
	__forceinline static void ReFrack() {ms_Frack = (rand() << 1) | (ms_Frack & 0x1);}

private:
	static CGameScriptHandlerMgr ms_ScriptHandlerMgr;

	static CScriptsForBrains ms_ScriptsForBrains;
	static CScriptBrainDispatcher ms_ScriptBrainDispatcher;

	static CMissionReplayStats ms_MissionReplayStats;

	static CHiddenObjects ms_HiddenObjects;

	static CScriptPatrol ms_ScriptPatrol;

	static parTree* ms_pXmlTree;
	static parTreeNode* ms_pCurrentXmlTreeNode;

	static bool ms_bProcessingTheScriptsDuringGameInit;

	static bool ms_bUpdatingScriptThreads;	//gets set true when script threads get updated and set false after.

	static bool ms_bKillingAllScriptThreads;	//	This will only be TRUE during scrThread::KillAllThreads()

	static bool ms_bPlayerIsOnAMission;
	static bool ms_bPlayerIsOnARandomEvent;
	static u32	ms_Frack;
	static int ms_NumberOfMiniGamesInProgress;		// assert that this is 0 when saving

	static bool ms_bPlayerIsRepeatingAMission;
	static bool ms_bPlayerIsInAnimalForm;
	static bool ms_bIsInDirectorMode;
	static bool ms_directorModeAvailable;
//	static bool bAllowGangRelationshipsToBeChanged;

	static s32 ms_customMPHudColor;

	static bool ms_bAllowGameToPauseForStreaming;	//	This should only be false when running the end credits to avoid the scrolling of the credits being paused
												//	by LoadAllRequestedObjects when the player warps when the screen is faded
	static bool ms_bScenarioPedsCanBeGrabbedByScript;

	static bool ms_bCodeRequestedAutosave;

	static CScriptOpList ms_scriptOpList;
#if !__PPU
	static bool ms_bSinglePlayerRestoreHasJustOccurred;
#endif

#if __BANK
	static f32 sm_fTimeFadedOut;
#endif	//	__BANK

#if !__FINAL
	static bool ms_bScriptLaunched;
#endif

	static sysTimer sm_ScriptTimer;	//	used for precise timings within scripts using GET_SCRIPT_TIME_WITHIN_FRAME_IN_MICROSECONDS command

	static char ms_ContentIdOfCurrentUGCMission[RLUGC_MAX_CONTENTID_CHARS];

#if GEN9_STANDALONE_ENABLED
	enum class InitState
	{
		Uninitialized,
		Initialized,
		Running
	};

	static InitState ms_initState;
#endif // GEN9_STANDALONE_ENABLED

private:
//	Private functions
	static void RegisterScriptCommands();

	static void ScrambleAndRegister();

	static void LaunchStartupScript();
};


enum eCommandApplyForceTypes
{
	APPLY_TYPE_FORCE = 0,
	APPLY_TYPE_IMPULSE,
	APPLY_TYPE_EXTERNAL_FORCE,
	APPLY_TYPE_EXTERNAL_IMPULSE,
	APPLY_TYPE_TORQUE,
	APPLY_TYPE_ANGULAR_IMPULSE
};


// used by GetClosestObjectCB
struct ClosestObjectDataStruct
{
	float fClosestDistanceSquared;
	CEntity *pClosestObject;

	Vector3 CoordToCalculateDistanceFrom;

	u32 ModelIndexToCheck;
	bool bCheckModelIndex;
	bool bCheckStealableFlag;

	s32 nOwnedByMask;
};

// A temporary helper class that composes and decomposes matrices from/to Euler angles, based upon an enumerated rotation order.
// - This class should be stripped out once the RAGE matrix classes universally support enumerated rotation order.
class CScriptEulers
{
public:
	static void		MatrixFromEulers(Matrix34& matrix, const Vector3& eulers, EulerAngleOrder rotationOrder = EULER_YXZ);
	static Vector3	MatrixToEulers(const Matrix34& matrix, EulerAngleOrder rotationOrder = EULER_YXZ);

	static void		QuaternionFromEulers(Quaternion& quat, const Vector3& eulers, EulerAngleOrder rotationOrder = EULER_YXZ);
	static Vector3	QuaternionToEulers(Quaternion& quat, EulerAngleOrder rotationOrder = EULER_YXZ);
};

class CDLCScript
{
public:
	enum eDLCScriptState { DLC_SCR_NONE = 0, DLC_SCR_REQUESTED, DLC_SCR_LOADED, DLC_SCR_SHUTTING_DOWN, DLC_SCR_STOPPED };

	CDLCScript() { Reset(); }
	~CDLCScript() { }

	void Reset()
	{
		m_state = DLC_SCR_NONE;
		m_request.Release();
		m_startupScript = 0;
		m_contentHash = 0;
		m_thread = THREAD_INVALID;
		m_scriptCallstackSize = 1024;
	}
	void InitScript();
	void UpdateScript();
	void ShutdownScript();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();

	static void RemoveScript(CDLCScript* dlcScript);
	static void RemoveScriptData(const char* fileName);
	static void AddScriptData(const char* fileName);

	static bool ContainsScript(atLiteralHashValue nameHash);

	atFinalHashString	GetScriptName(){return m_startupScript;}
	atLiteralHashString	GetScriptLiteralName(){return atLiteralHashString(m_startupScript.GetCStr());}
	eDLCScriptState		GetState(){return m_state;}
	void				SetState(eDLCScriptState state){m_state = state;}
	scrThreadId			GetThreadId(){return m_thread;}
	strRequest			GetRequest(){return m_request;}
	s32					GetStackSize(){return m_scriptCallstackSize;}
	u32					GetContentHash(){return m_contentHash;}
private:
	typedef atBinaryMap<CDLCScript*,u32>::Iterator dlcScriptIterator;
	static atBinaryMap<CDLCScript*,u32> ms_scripts;
	atFinalHashString m_startupScript;
	eDLCScriptState m_state;
	scrThreadId m_thread;
	strRequest m_request;
	u32 m_contentHash;
	s32 m_scriptCallstackSize;
	PAR_SIMPLE_PARSABLE;
};

#define ENABLE_SCRIPT_TAMPER_CHECKER 1
#if RSG_PC && ENABLE_SCRIPT_TAMPER_CHECKER

#if RSG_FINAL
	#define csgtc_inline __forceinline
#else
	#define csgtc_inline 
#endif

class CScriptGlobalTamperCheckerTunablesListener : public TunablesListener
{
public:
#if !__NO_OUTPUT
	CScriptGlobalTamperCheckerTunablesListener() : TunablesListener("CScriptGlobalTamperCheckerTunablesListener"), m_IsDisabled(0) {}
#endif
	virtual void OnTunablesRead();
	csgtc_inline bool IsDisabled() { return m_IsDisabled !=0 ;}
private:
	sysObfuscated<int> m_IsDisabled;
};

class CScriptGlobalTamperChecker
{
public:
	static void Initialize();
	static void Reset();
	csgtc_inline static bool CurrentGlobalsDifferFromPreviousFrame(int &page);
	static void Update();
	static void FakeTamper(const s32 offsetIndex);
	csgtc_inline static bool TamperingDetected(void){return ms_bTamperingDetected;}

	//light class which created a BeginUncheckedSection/EndUncheckedSection within it's scope
	struct UncheckedScope
	{
		UncheckedScope();
		~UncheckedScope();
	};

private:
	static bool ResetGlobalWriteTracking(s32 pageIndex);
	csgtc_inline static bool HaveGlobalsBeenWrittenToSinceReset(s32 pageIndex);
	csgtc_inline static bool ShouldCheckGlobalBlock(s32 pageIndex);
	static void BeginUncheckedSection();
	static void EndUncheckedSection();
	static void LockGlobalsMemory();
	static void UnlockGlobalsMemory();
	static void LockGlobalsMemory(s32 pageIndex);
	static void UnlockGlobalsMemory(s32 pageIndex);
	static void FakeTamper(s32 pageIndex, const s32 offsetIndex);
	static CScriptGlobalTamperCheckerTunablesListener ms_pScriptGlobalTamperTunablesListener;	

	static atRangeArray<bool, scrProgram::MAX_GLOBAL_BLOCKS> ms_abValidPage;
	static bool ms_bTamperingDetected;
	static s32	ms_cUncheckedSection;
};

#endif	//RSG_PC

#endif	//	_SCRIPT_H_
