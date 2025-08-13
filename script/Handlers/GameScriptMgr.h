//
// name:		GameScriptMgr.h
// description:	Project specific script handler manager
// written by:	John Gurney
//


#ifndef GAME_SCRIPT_MGR_H
#define GAME_SCRIPT_MGR_H

#include "fwscript/scriptHandlerMgr.h"

// rage headers
#include "atl/array.h"
#include "atl/map.h"
#include "bank/combo.h"

// game headers
#include "renderer/RenderTargetMgr.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptIds.h"

class GtaThread;
class CNetworkBandwidthManager;
class CNetGamePlayer;

class CGameScriptHandlerMgr : public scriptHandlerMgr
{
public:

	static const unsigned MAX_REMOTE_ACTIVE_SCRIPTS = 320;
	static const unsigned MAX_REMOTE_TERMINATED_SCRIPTS = 64;
	static unsigned NUM_VALID_NETWORK_SCRIPTS;

	// the maximum streaming memory that the scripts are allowed to use (in Kb)
	//////////////////////////////////////////////////////////////////////////
#if RSG_ORBIS || RSG_DURANGO
	//////////////////////////////////////////////////////////////////////////
	static const unsigned VIRTUAL_STREAMING_MEMORY_LIMIT				= (40 << 10);
	static const unsigned PHYSICAL_STREAMING_MEMORY_LIMIT				= (85 << 10);

	static const unsigned VIRTUAL_STREAMING_FREEMODE_MEMORY_LIMIT		= (40 << 10);
	static const unsigned PHYSICAL_STREAMING_FREEMODE_MEMORY_LIMIT		= (85 << 10);
#if !__FINAL
	static const unsigned VIRTUAL_STREAMING_MEMORY_LIMIT_DBG			= (50 << 10);
	static const unsigned PHYSICAL_STREAMING_MEMORY_LIMIT_DBG			= (95 << 10);

	static const unsigned VIRTUAL_STREAMING_FREEMODE_MEMORY_LIMIT_DBG	= (50 << 10);
	static const unsigned PHYSICAL_STREAMING_FREEMODE_MEMORY_LIMIT_DBG	= (95 << 10);
#endif
	//////////////////////////////////////////////////////////////////////////
#else
	//////////////////////////////////////////////////////////////////////////
	//PC
	static const unsigned VIRTUAL_STREAMING_MEMORY_LIMIT				= (240 << 10);
	static const unsigned PHYSICAL_STREAMING_MEMORY_LIMIT				= (560 << 10);

	static const unsigned VIRTUAL_STREAMING_FREEMODE_MEMORY_LIMIT		= (240 << 10);
	static const unsigned PHYSICAL_STREAMING_FREEMODE_MEMORY_LIMIT		= (560 << 10);
#if !__FINAL
	static const unsigned VIRTUAL_STREAMING_MEMORY_LIMIT_DBG			= (320 << 10);
	static const unsigned PHYSICAL_STREAMING_MEMORY_LIMIT_DBG			= (640 << 10);

	static const unsigned VIRTUAL_STREAMING_FREEMODE_MEMORY_LIMIT_DBG	= (320 << 10);
	static const unsigned PHYSICAL_STREAMING_FREEMODE_MEMORY_LIMIT_DBG	= (640 << 10);
#endif

	//////////////////////////////////////////////////////////////////////////
#endif

	static const unsigned TOTAL_STREAMING_REQUEST_PRIO_LIMIT = (20 << 10);
	static const unsigned TOTAL_STREAMING_REQUEST_TOTAL_LIMIT = (40 << 10);
	static const unsigned STREAMING_MEM_CHECK_TIME = 1000; // the time between each check to see if the script streaming memory exceeds its limit

public:

	//PURPOSE
	//	Info on a script running remotely but not locally
	class CRemoteScriptInfo
	{
		friend CGameScriptHandlerMgr;

	public:

		CRemoteScriptInfo() :
			m_ActiveParticipants(0),
			m_HostToken(0),
			m_Host(NULL),
			m_TimeInactive(0),
			m_ReservedPeds(0),
			m_ReservedVehicles(0),
			m_ReservedObjects(0),
			m_CreatedPeds(0),
			m_CreatedVehicles(0),
			m_CreatedObjects(0),
			m_RunningLocally(false)
		{
		}

		CRemoteScriptInfo(const CGameScriptId& scriptId,
							PlayerFlags activeParticipants,
							HostToken hostToken,
							const netPlayer* pHost,
							u32 reservedPeds,
							u32 reservedVehicles,
							u32 reservedObjects) :
			m_ScriptId(scriptId),
			m_ActiveParticipants(activeParticipants),
			m_HostToken(hostToken),
			m_Host(pHost),
			m_TimeInactive(0),
			m_ReservedPeds((u8)reservedPeds),
			m_ReservedVehicles((u8)reservedVehicles),
			m_ReservedObjects((u8)reservedObjects),
			m_CreatedPeds(0),
			m_CreatedVehicles(0),
			m_CreatedObjects(0),
			m_RunningLocally(false)
		{
			if (m_ActiveParticipants == 0)
			{
				m_TimeInactive = GetCurrentTime();
			}
		}

		CRemoteScriptInfo(const CRemoteScriptInfo& info) :
			m_ScriptId(info.m_ScriptId),
			m_ActiveParticipants(info.m_ActiveParticipants),
			m_HostToken(info.m_HostToken),
			m_Host(info.m_Host),
			m_TimeInactive(info.m_TimeInactive),
			m_ReservedPeds(info.m_ReservedPeds),
			m_ReservedVehicles(info.m_ReservedVehicles),
			m_ReservedObjects(info.m_ReservedObjects),
			m_CreatedPeds(info.m_CreatedPeds),
			m_CreatedVehicles(info.m_CreatedVehicles),
			m_CreatedObjects(info.m_CreatedObjects),
			m_RunningLocally(info.m_RunningLocally)
		{
		}

		const CGameScriptId&	GetScriptId() const								{ return m_ScriptId; }
		PlayerFlags				GetActiveParticipants() const					{ return m_ActiveParticipants; }
		HostToken				GetHostToken() const							{ return m_HostToken; }
		void					SetHostToken(HostToken token)					{ m_HostToken = token; }
		const netPlayer*		GetHost() const									{ return m_Host; }
		void					SetHost(const netPlayer* pHost)					{ m_Host = pHost; }
		bool					IsPlayerAParticipant(PhysicalPlayerIndex player) const	{ return (m_ActiveParticipants & (1<<player)) != 0; }
		void					ReservePeds(u32 peds)							{ m_ReservedPeds = (u8)peds; }
		void					ReserveVehicles(u32 vehicles)					{ m_ReservedVehicles = (u8)vehicles; }
		void					ReserveObjects(u32 objects)						{ m_ReservedObjects = (u8)objects; }
		u32						GetReservedPeds() const							{ return m_ReservedPeds; }
		u32						GetReservedVehicles() const						{ return m_ReservedVehicles; }
		u32						GetReservedObjects() const						{ return m_ReservedObjects; }
		u32						GetCreatedPeds() const							{ return m_CreatedPeds; }
		u32						GetCreatedVehicles() const						{ return m_CreatedVehicles; }
		u32						GetCreatedObjects() const						{ return m_CreatedObjects; }
		void					SetCreatedPeds(u32 peds)						{ m_CreatedPeds = (u8) peds; }
		void					SetCreatedVehicles(u32 vehicles)				{ m_CreatedVehicles = (u8) vehicles; }
		void					SetCreatedObjects(u32 objects)					{ m_CreatedObjects = (u8) objects; }
		void					AddCreatedPed()									{ m_CreatedPeds++; }
		void					AddCreatedVehicle()								{ m_CreatedVehicles++; }
		void					AddCreatedObject()								{ m_CreatedObjects++; }
		void					RemoveCreatedPed()								{ if (m_CreatedPeds>0) m_CreatedPeds--; }
		void					RemoveCreatedVehicle()							{ if (m_CreatedVehicles>0) m_CreatedVehicles--; }
		void					RemoveCreatedObject()							{ if (m_CreatedObjects>0) m_CreatedObjects--; }
		void					SetRunningLocally(bool b)						{ m_RunningLocally = b; }
		bool					GetRunningLocally() const						{ return m_RunningLocally; }

		bool IsTerminated() const;

		void AddActiveParticipant(PhysicalPlayerIndex player);

		void SetParticipants(PlayerFlags participants)
		{
			m_ActiveParticipants = participants;

			if (m_ActiveParticipants == 0)
			{
				if (m_TimeInactive == 0)
					m_TimeInactive = GetCurrentTime();
			}
			else
			{
				m_TimeInactive = 0;
			}
		}

		void RemoveParticipant(PhysicalPlayerIndex player);

		u32 GetNumParticipants() const
		{
			u32 numParticipants = 0;

			for (u32 p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
			{
				if (m_ActiveParticipants & (1<<p))
				{
					numParticipants++;
				}
			}

			return numParticipants;
		}

		void GetReservations(u32& reservedPeds, u32& reservedVehicles, u32& reservedObjects) const
		{
			reservedPeds = m_ReservedPeds;
			reservedVehicles = m_ReservedVehicles;
			reservedObjects = m_ReservedObjects;
		}

		void ResetReservations()
		{
			m_ReservedPeds = m_ReservedVehicles = m_ReservedObjects = 0;
		}

		void GetNumRequiredEntities(u32& requiredPeds, u32& requiredVehicles, u32& requiredObjects) const
		{
			requiredPeds		= (m_ReservedPeds > m_CreatedPeds) ? (m_ReservedPeds-m_CreatedPeds) : 0;
			requiredVehicles	= (m_ReservedVehicles > m_CreatedVehicles) ? (m_ReservedVehicles-m_CreatedVehicles) : 0;
			requiredObjects		= (m_ReservedObjects > m_CreatedObjects) ? (m_ReservedObjects-m_CreatedObjects) : 0;
		}

		CRemoteScriptInfo& operator=(const CRemoteScriptInfo& that)
		{
			m_ScriptId				= that.m_ScriptId;
			m_ActiveParticipants	= that.m_ActiveParticipants;
			m_HostToken				= that.m_HostToken;
			m_Host					= that.m_Host;
			m_TimeInactive			= that.m_TimeInactive;
			m_ReservedPeds			= that.m_ReservedPeds;
			m_ReservedVehicles		= that.m_ReservedVehicles;
			m_ReservedObjects		= that.m_ReservedObjects;
			m_CreatedPeds			= that.m_CreatedPeds;
			m_CreatedVehicles		= that.m_CreatedVehicles;
			m_CreatedObjects		= that.m_CreatedObjects;
			m_RunningLocally		= that.m_RunningLocally;

			return *this;
		}

		bool operator==(const CRemoteScriptInfo& that) const
		{
			// only check the members that we are interesting in broadcasting, this is called by CRemoteScriptInfoEvent
			return (m_ScriptId == that.m_ScriptId &&
					m_ActiveParticipants == that.m_ActiveParticipants &&
					m_HostToken == that.m_HostToken &&
					m_ReservedPeds == that.m_ReservedPeds &&
					m_ReservedVehicles == that.m_ReservedVehicles &&
					m_ReservedObjects == that.m_ReservedObjects);
		}


		void UpdateInfo(const CRemoteScriptInfo& that);

		void Serialise(CSyncDataBase& serialiser);

	protected:

		u32 GetCurrentTime() const;

		CGameScriptId		m_ScriptId;				// the unique id of the script
		PlayerFlags			m_ActiveParticipants;	// flags indicating which players are running the script
		const netPlayer*	m_Host;					// the current host of the script
		u32					m_TimeInactive;			// the first time this script info was on the active queue with no participants
		HostToken			m_HostToken;			// the current host token of the host (used to reject messages from a previous host)
		u8					m_ReservedPeds;			// the current number of peds reserved by the host of the script
		u8					m_ReservedVehicles;		// the current number of vehicles reserved by the host of the script
		u8					m_ReservedObjects;		// the current number of objects reserved by the host of the script
		u8					m_CreatedPeds;			// the current number of peds created by the script on the local machine
		u8					m_CreatedVehicles;		// the current number of vehicles created by the script on the local machine
		u8					m_CreatedObjects;		// the current number of objects created by the script on the local machine
		bool				m_RunningLocally;		// set when this script is running locally and has a script handler
	};

public:

	CGameScriptHandlerMgr(CNetworkBandwidthManager* bandwidthMgr = NULL);

	virtual scriptIdBase& GetScriptId(const scrThread& scriptThread) const;

	virtual scriptHandler* CreateScriptHandler(scrThread& scriptThread);

	virtual void Init();
	virtual void Shutdown();
	virtual void Update();

	virtual void NetworkInit();
	virtual void NetworkShutdown();
	virtual void NetworkUpdate();

	virtual scriptHandler* GetScriptHandler(const scriptIdBase& scrId)
	{
		scriptHandler* pHandler = scriptHandlerMgr::GetScriptHandler(scrId);

		// the timestamp is ignored by the hashing function, so we need to check for it here
		if (pHandler)
		{
			const CGameScriptId& gameId1 = static_cast<const CGameScriptId&>(pHandler->GetScriptId());
			const CGameScriptId& gameId2 = static_cast<const CGameScriptId&>(scrId);

			if (gameId1.GetTimeStamp() != 0 &&
				gameId2.GetTimeStamp() != 0 &&
				gameId1.GetTimeStamp() != gameId2.GetTimeStamp())
			{
				Assert(gameId1.GetScriptNameHash() == gameId2.GetScriptNameHash());
				pHandler = NULL;
			}
#if __ASSERT
			else if (pHandler->GetScriptId() != scrId)
			{
				char name1[200];
				char name2[200];

				// have to cache both names as GetLogName() uses a static string and so the same log name is displayed twice in the assert below
				formatf(name1, 200, pHandler->GetScriptId().GetLogName());
				formatf(name2, 200, scrId.GetLogName());

				Assertf(pHandler->GetScriptId() == scrId, "Script handler hash conflict - %s was returned for a hash generated by %s", name1, name2);
			}
#endif
		}

		return pHandler;
	}

	void PlayerHasLeft(const netPlayer& player);

#if __BANK
	void InitWidgets(bkBank* pBank);
	void ShutdownWidgets(bkBank* pBank);
#endif

	virtual void RegisterScript(scrThread& scriptThread);
	virtual void UnregisterScript(scrThread& scriptThread);

	void SetScriptAsANetworkScript(GtaThread& scriptThread, int instanceId = -1);
	bool TryToSetScriptAsANetworkScript(GtaThread& scriptThread, int instanceId = -1);
	void SetScriptAsSafeToRunInMP(GtaThread& scriptThread);

	// fills the given array with all entities held on all the current script handler lists. Returns the total number of entities.
	u32 GetAllScriptHandlerEntities(class CEntity* objArray[], u32 arrayLen);

	// returns the total number of entities that locally running scripts have reserved
	void GetNumReservedScriptEntities(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

	// returns the total number of entities that locally running  scripts are created
	void GetNumCreatedScriptEntities(u32 &numPeds, u32 &numVehicles, u32 &numObjects) const;

	// returns the total number of entities that the local and remote scripts are potentially going to create (the object pop mgr needs to create space for these objects)
	void GetNumRequiredScriptEntities(u32 &numPeds, u32 &numVehicles, u32 &numObjects, CGameScriptHandler* pHandlerToIgnore = NULL) const;

	//	Adds info on a script running either remotely or locally
	void AddRemoteScriptInfo(CRemoteScriptInfo& scriptInfo, const netPlayer* sendingPlayer = 0);

	// Adds a player to the participant flags of a remote script.
	void AddPlayerToRemoteScript(const CGameScriptId& scriptId, const netPlayer& player);

	// Removes a player from the participant flags of a remote script.
	void RemovePlayerFromRemoteScript(const CGameScriptId& scriptId, const netPlayer& player, bool bCalledFromHandler);

	// moves an active script info matching the given script id onto the terminated queue
	void TerminateActiveScriptInfo(const CGameScriptId& scriptId);

	// Sets a new host of a remote script.
	void SetNewHostOfRemoteScript(const CGameScriptId& scriptId, const netPlayer& newHost, HostToken newHostToken, PlayerFlags participantFlags);

	// returns the closest participant of the given remote script
	CNetGamePlayer* GetClosestParticipantOfRemoteScript(const CRemoteScriptInfo& remoteScriptInfo);

	//	Gets info on a new script running remotely or locally
	CRemoteScriptInfo* GetRemoteScriptInfo(const CGameScriptId& scriptId, bool bActiveOnly);
	const CRemoteScriptInfo* GetRemoteScriptInfo(const CGameScriptId& scriptId, bool bActiveOnly) const;

	// returns true if the given script is running, either locally or remotely. if bMatchOnNameOnly is set, only the script names
	// are used in the comparisons, and the position, instance and timestamp are ignored.
	bool IsScriptActive(const CGameScriptId& scriptId, bool bMatchOnNameOnly = false);

	// returns true if the given script has been terminated
	bool IsScriptTerminated(const CGameScriptId& scriptId);

	// returns true if the given player is a participant in the given script, either local or remote
	bool IsPlayerAParticipant(const CGameScriptId& scriptId, const netPlayer& player);

    // returns true if the local machine is host of the specified script
    bool IsNetworkHost(const CGameScriptId& scriptId);

	// returns the number of participants running a local or remote script
	u32 GetNumParticipantsOfScript(const CGameScriptId& scriptId);

	// called when the script id for a handler changes
	void RecalculateScriptId(CGameScriptHandler& handler);

	//clears the custcene flags for all scripts
	void SetNotInMPCutscene();

	// informs the remote script infos that an entity has been created, so the info for the script that created the entity can be updated
	void AddToReservationCount(CNetObjGame& scriptEntityObj);

	// informs the remote script infos that an entity has been removed, so the info for the script that created the entity can be updated
	void RemoveFromReservationCount(CNetObjGame& scriptEntityObj);

	// returns the number of reserved and created entities by remote scripts not running locally, relative to the local player. If pScopePosition is set, that is used instead of the player's position
	void GetRemoteScriptEntityReservations(u32& reservedPeds, u32& reservedVehicles, u32& reservedObjects, u32& createdPeds, u32& createdVehicles, u32& createdObjects, Vector3* pScopePosition = nullptr) const;

	void LogCurrentReservations(netLoggingInterface& log) const;

#if __SCRIPT_MEM_CALC
	// gets the time since the last script streaming memory emergency
	float GetMsTimeSinceLastEmergency() { return m_lastStreamingEmergencyTimer.GetMsTime();}
	// time since last script streaming request emergency
	float GetMsTimeSinceLastRequestEmergency() { return m_lastStreamingReqEmergencyTimer.GetMsTime();}
#endif // __SCRIPT_MEM_CALC

#if __BANK
	// displays info on the current selected script handler
	void DisplayScriptHandlerInfo() const;

	// displays debug info on all the script handler objects and resources
	void DisplayObjectAndResourceInfo() const;

	// displays what entities are reserved for local and remote scripts
	void DisplayReservationInfo() const;

#endif	//	__BANK

#if __SCRIPT_MEM_CALC
	void CalculateMemoryUsage();

	void AddToModelsUsedByMovieMeshes(u32 ModelIndex);
	void AddToMoviesUsedByMovieMeshes(u32 MovieHandle);
	void AddToRenderTargetsUsedByMovieMeshes(CRenderTargetMgr::namedRendertarget *pNamedRenderTarget);

	u32 GetTotalScriptVirtualMemory() { return m_TotalScriptVirtualMemory; }
	u32 GetTotalScriptPhysicalMemory() { return m_TotalScriptPhysicalMemory; }
	u32 GetTotalScriptVirtualMemoryPeak() { return m_TotalScriptVirtualMemoryPeak; }
	u32 GetTotalScriptPhysicalMemoryPeak() { return m_TotalScriptPhysicalMemoryPeak; }
#endif	//	__SCRIPT_MEM_CALC

#if __BANK

	void GetScriptDependencyList(atArray<strIndex> &theArray);

	void UpdateScriptHandlerCombo();

	const CGameScriptHandler* GetScriptHandlerFromComboName(const char* scriptName) const;
    CGameScriptHandler*       GetScriptHandlerFromComboName(const char* scriptName);

	// these are public because they are used by the network bot widgets
	static const char*    ms_Handlers[CGameScriptHandler::MAX_NUM_SCRIPT_HANDLERS];
	static unsigned int   ms_NumHandlers;
	static int            ms_HandlerComboIndex;
#endif

protected:

	scriptIdBase& GetMsgScriptId() { return m_MsgScriptId; }

	// gets the streaming objects to ignore when calculating or displaying resource memory useage
	void GetResourceIgnoreList(const strIndex** ignoreList, s32& numIgnores) const;

	void WriteRemoteScriptHeader(const CGameScriptId& scriptId, const char *headerText, ...);

	// possibly handles the case where we have 2 scripts running with different timestamps
	bool HandleTimestampConflict(const CRemoteScriptInfo& scriptInfo);

	void MoveActiveScriptToTerminatedQueue(u32 activeScriptSlot);

	CGameScriptId	m_MsgScriptId;

	// a queue holding info on active remote scripts
	atQueue<CRemoteScriptInfo, MAX_REMOTE_ACTIVE_SCRIPTS> m_remoteActiveScripts;

	// a queue holding info on terminated remote scripts
	atQueue<CRemoteScriptInfo, MAX_REMOTE_TERMINATED_SCRIPTS> m_remoteTerminatedScripts;

#if __SCRIPT_MEM_CALC
	// last streaming emergency timer for the script streaming memory
	rage::sysTimer	m_lastStreamingEmergencyTimer;

	// last streaming request emergency timer for script requests
	rage::sysTimer	m_lastStreamingReqEmergencyTimer;

	// last time that script streaming memory was checked to see if it exceeds its limit
	rage::sysTimer	m_streamingMemTestTimer;
#endif // __SCRIPT_MEM_CALC


#if __BANK
	static bkCombo*		  ms_pHandlerCombo;
	static bool			  ms_DisplayHandlerInfo;
	static bool			  ms_DisplayAllMPHandlers;
#endif

private:
#if __SCRIPT_MEM_CALC
	void ClearMemoryDebugInfoForMovieMeshes(bool bTrackMemoryForMovieMeshes) const;
	void CalculateMemoryUsageForMovieMeshes(const CGameScriptHandler* pScriptHandler, u32 &VirtualTotal, u32 &PhysicalTotal, bool bDisplayScriptDetails) const;

	void CalculateMemoryUsageForNamedRenderTargets(const CGameScriptHandler* pScriptHandler, u32 &virtualMemory, u32 &physicalMemory, bool bDisplayScriptDetails) const;

	mutable bool m_bTrackMemoryForMovieMeshes;
	mutable atArray<u32> m_ArrayOfModelsUsedByMovieMeshes;
	mutable atMap<u32,u32> m_MapOfMoviesUsedByMovieMeshes;
	mutable atMap<CRenderTargetMgr::namedRendertarget*, u32> m_MapOfRenderTargetsUsedByMovieMeshes;

	u32 m_TotalScriptVirtualMemory;
	u32 m_TotalScriptPhysicalMemory;
	u32 m_TotalScriptVirtualMemoryPeak;
	u32 m_TotalScriptPhysicalMemoryPeak;
#endif	//	__SCRIPT_MEM_CALC

	bool IsScriptValidForNetworking(atHashValue scriptName);

	static u32 ms_validNetworkScriptHashes[];
};

#endif  // GAME_SCRIPT_MGR_H
