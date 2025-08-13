//
// name:        NetworkDebug.h
// description: Network debug widget functionality
// written by:  Daniel Yelland
//

#ifndef NETWORK_DEBUG_H
#define NETWORK_DEBUG_H

namespace rage
{
    class phBound;
    class msgSyncDRDisplay;
    class netPlayer;
}

class CNetObjPed;
class CNetObjVehicle;
class CPed;
class CEntity;

#include "rline/clan/rlclancommon.h"
#include "net/message.h"
#include "fwnet/netinterface.h"
#include "fwnet/netplayermgrbase.h"
#include "Task/System/TaskTypes.h"
#include "vectormath/mat34v.h"

#if __BANK
enum eDebugBreakType
{
	BREAK_TYPE_FOCUS_CREATE,
	BREAK_TYPE_FOCUS_UPDATE,
	BREAK_TYPE_FOCUS_WRITE_SYNC,
	BREAK_TYPE_FOCUS_WRITE_SYNC_NODE,
	BREAK_TYPE_FOCUS_READ_SYNC,
	BREAK_TYPE_FOCUS_READ_SYNC_NODE,
	BREAK_TYPE_FOCUS_CLONE_CREATE,
	BREAK_TYPE_FOCUS_CLONE_REMOVE,
	BREAK_TYPE_FOCUS_TRY_TO_MIGRATE,
	BREAK_TYPE_FOCUS_MIGRATE,
	BREAK_TYPE_FOCUS_DESTROY,
	BREAK_TYPE_FOCUS_NO_LONGER_NEEDED,
	BREAK_TYPE_FOCUS_COLLISION_OFF,
	BREAK_TYPE_FOCUS_SYNC_NODE_READY,
	NUM_BREAK_TYPES
};

#define NETWORK_DEBUG_BREAK(breakType)\
	do {if (NetworkDebug::CanDebugBreak( breakType )) __debugbreak();} while (0);
#define NETWORK_DEBUG_BREAK_FOR_FOCUS(breakType, entity)\
	do {if (NetworkDebug::CanDebugBreakForFocus( breakType , entity )) __debugbreak();} while (0);
#define NETWORK_DEBUG_BREAK_FOR_FOCUS_ID(breakType, objectId)\
	do {if (NetworkDebug::CanDebugBreakForFocusId( breakType , objectId )) __debugbreak();} while (0);

#else // __BANK

#define NETWORK_DEBUG_BREAK(breakType)
#define NETWORK_DEBUG_BREAK_FOR_FOCUS(breakType, entity) 
#define NETWORK_DEBUG_BREAK_FOR_FOCUS_ID(breakType, objectId)

#endif // __BANK

enum NetworkMemoryCategory
{
	NET_MEM_GAME_ARRAY_HANDLERS,
	NET_MEM_SCRIPT_ARRAY_HANDLERS,
	NET_MEM_BOTS,
    NET_MEM_CONNECTION_MGR_HEAP,
    NET_MEM_RLINE_HEAP,
	NET_MEM_LIVE_MANAGER_AND_SESSIONS,
	NET_MEM_MISC,
	NET_MEM_PLAYER_MGR,
	NET_MEM_OBJECT_MANAGER,
	NET_MEM_EVENT_MANAGER,
	NET_MEM_ARRAY_MANAGER,
	NET_MEM_BANDWIDTH_MANAGER,
	NET_MEM_ROAMING_BUBBLE_MANAGER,
	NET_MEM_NETWORK_OBJECTS,
	NET_MEM_NETWORK_BLENDERS,
	NET_MEM_VOICE,
	NET_MEM_PLAYER_SYNC_DATA,
	NET_MEM_PED_SYNC_DATA,
	NET_MEM_VEHICLE_SYNC_DATA,
	NET_MEM_OBJECT_SYNC_DATA,
	NET_MEM_PICKUP_SYNC_DATA,
	NET_MEM_PICKUP_PLACEMENT_SYNC_DATA,
	NET_MEM_DOOR_SYNC_DATA,
	NET_MEM_GLASS_PANE_SYNC_DATA,
	NET_MEM_LEADERBOARD_DATA,
	NET_MEM_NUM_MANAGERS
};

namespace NetworkDebug
{
	class NetworkDebugMessageHandler
	{
	public:
		NetworkDebugMessageHandler()
		{
			m_Dlgt.Bind(this, &NetworkDebugMessageHandler::OnMessageReceived);
		}

		void Init()
		{
			netInterface::AddDelegate(&m_Dlgt);
		}
		void Shutdown()
		{
			netInterface::RemoveDelegate(&m_Dlgt);
		}

		template<typename T>
		void SendMessageToPlayer(const netPlayer& player, T& msg)
		{
			u8 buf[netFrame::MAX_BYTE_SIZEOF_PAYLOAD];
			unsigned size;

			if (AssertVerify(msg.Export(buf, sizeof(buf), &size)))
			{
				netInterface::GetPlayerMgr().SendBuffer(&player, buf, size, NET_SEND_RELIABLE);

                BANK_ONLY(netInterface::GetPlayerMgr().IncreaseMiscReliableMessagesSent());
			}
		}

		template<typename T>
		void SendMessageToAllPlayers(T& msg)
		{
			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
            const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
            {
		        const netPlayer *remotePlayer = remotePhysicalPlayers[index];

				if(remotePlayer->GetConnectionId() >= 0 && !remotePlayer->IsBot())
				{
					SendMessageToPlayer(*remotePlayer, msg);
				}
			}
		}

		void OnMessageReceived(const ReceivedMessageData &messageData);

	private:

		// a message handler to be used for handling debug messages
		NetworkPlayerEventDelegate m_Dlgt;
	};
	
	void Init();
    void Shutdown();
    void InitSession();
    void ShutdownSession();
    void Update();
	BANK_ONLY( void CredentialsUpdated(const int localGamerIndex); )

    void OpenNetwork();
    void CloseNetwork();

	void PlayerHasJoined(const netPlayer& player);

    unsigned GetNumOutgoingReliables();

	void PrintLocalUserId();
	void PrintNetworkInfo();
	void PrintPhysicsInfo();
	void DisplayNetworkScripts();

#if __BANK
    void RenderDebug();
#endif // __BANK

	//Call Register the Fail Mark and send mark to all participants.
	void AddFailMark(const char* failStr);
	//Register the Fail Mark in the logs.
	void RegisterFailMark(const char* failStr, const bool bVerboseFailmark);
	
	//Sync the debug recording frame on all machines
	void SyncDRFrame(msgSyncDRDisplay &msg);

	void DisplayDebugMessage(const char* message);

	void DisplayHttpReqTrackerMessage(const char* message);

	bool DisplayDebugEntityDamageInfo();

	void FlushDebugLogs(bool waitForFlush);

#if !__FINAL
	void DisplayPauseMessage(const char* message);
	void BroadcastDebugTimeSettings();
#endif

#if __BANK

	void PerformPedBindPoseCheckDuringUpdate(bool owners, bool clones, bool cops, bool noncops, CTaskTypes::eTaskType const taskType = CTaskTypes::TASK_INVALID_ID);

	bool FailCloneRespawnNetworkEvent();
	bool FailRespawnNetworkEvent();

	bool DebugWeaponDamageEvent();

	bool CanDebugBreak(eDebugBreakType breakType);
	bool CanDebugBreakForFocus(eDebugBreakType breakType, netObject& netObj);
	bool CanDebugBreakForFocusId(eDebugBreakType breakType, ObjectId objectId);

	void UpdateFocusEntity(bool bNewEntityChosen);

    const char *GetMemCategoryName(NetworkMemoryCategory category);
    void RegisterMemoryAllocated(NetworkMemoryCategory category, u32 memoryAllocated);
    void RegisterMemoryDeallocated(NetworkMemoryCategory category, u32 memoryDeallocated);

	void InitWidgets();

	// contains settings for displaying debug information
    // over network objects on screen for debugging purposes
    struct NetworkDisplaySettings
    {
        struct NetworkDisplayTargets
        {
            NetworkDisplayTargets() :
            m_displayObjects(false),
            m_displayPlayers(false),
            m_displayPeds(false),
            m_displayVehicles(false),
            m_displayTrailers(false)
            {
            };

            bool IsAnyFlagSet() const
            {
                return m_displayObjects || m_displayPlayers || m_displayPeds || m_displayVehicles || m_displayTrailers;
            }

            bool m_displayObjects;
            bool m_displayPlayers;
            bool m_displayPeds;
            bool m_displayVehicles;
            bool m_displayTrailers;
        };

        struct NetworkDisplayInformation
        {
            NetworkDisplayInformation() :
			m_displayPosition(false),
            m_displayVelocity(false),
            m_displayAngVelocity(false),
            m_displayOrientation(false),
            m_displayDamageTrackers(false),
		    m_displayUpdateRate(false),
		    m_updateRatePlayer(0),
		    m_displayVehicle(false),
            m_displayVehicleDamage(false),
		    m_displayPedRagdoll(false),
            m_displayEstimatedUpdateLevel(false),
			m_displayPickupRegeneration(false),
            m_displayCollisionTimer(false),
            m_displayPredictionInfo(false),
			m_displayInteriorInfo(false),
            m_displayFixedByNetwork(false),
            m_displayCanPassControl(false),
            m_displayCanAcceptControl(false),
			m_displayStealthNoise(false)
            {
            };

            bool IsAnyFlagSet() const
            {
                return m_displayPosition || m_displayVelocity || m_displayAngVelocity || m_displayOrientation || m_displayDamageTrackers || m_displayUpdateRate || m_displayVehicle || m_displayVehicleDamage || m_displayPedRagdoll || m_displayEstimatedUpdateLevel || m_displayPickupRegeneration || m_displayCollisionTimer || m_displayPredictionInfo || m_displayInteriorInfo || m_displayFixedByNetwork || m_displayCanPassControl || m_displayCanAcceptControl || m_displayStealthNoise;
            }

			bool m_displayPosition;
            bool m_displayVelocity;
            bool m_displayAngVelocity;
            bool m_displayOrientation;
            bool m_displayDamageTrackers;
		    bool m_displayUpdateRate;
		    s8   m_updateRatePlayer;
		    bool m_displayVehicle;
		    bool m_displayVehicleDamage;
		    bool m_displayPedRagdoll;
            bool m_displayEstimatedUpdateLevel;
			bool m_displayPickupRegeneration;
            bool m_displayCollisionTimer;
			bool m_displayPredictionInfo;
			bool m_displayInteriorInfo;
            bool m_displayFixedByNetwork;
            bool m_displayCanPassControl;
            bool m_displayCanAcceptControl;
			bool m_displayStealthNoise;
        };

        NetworkDisplaySettings() :
		m_displayNetInfo(false),
		m_displayBasicInfo(true),
        m_displayNetMemoryUsage(false),
		m_displayObjectInfo(false),
        m_DoObjectInfoRoomChecks(true),
		m_displayObjectScriptInfo(false),
        m_displayPlayerInfoDebug(false),
		m_displayPlayerScripts(false),
        m_displayPredictionInfo(false),
		m_renderPlayerCameras(false),
		m_displayHostBroadcastDataAllocations(false),
		m_displayPlayerBroadcastDataAllocations(false),
        m_displayPortablePickupInfo(false),
        m_displayDispatchInfo(false),
        m_displayObjectPopulationReservationInfo(false),
		m_displayVisibilityInfo(false),
		m_displayGhostInfo(false),
        m_displayConnectionMetrics(false),
		m_displayChallengeFlyableAreas(false)
		{
        };

		bool                      m_displayNetInfo;
		bool                      m_displayBasicInfo;
		bool                      m_displayNetMemoryUsage;
		bool                      m_displayObjectInfo;
        bool                      m_DoObjectInfoRoomChecks;
		bool                      m_displayObjectScriptInfo;
        bool                      m_displayPlayerInfoDebug;
		bool                      m_displayPlayerScripts;
        bool                      m_renderPlayerCameras;
        bool                      m_displayPredictionInfo;
		bool                      m_displayHostBroadcastDataAllocations;
		bool                      m_displayPlayerBroadcastDataAllocations;
		bool                      m_displayPortablePickupInfo;
		bool                      m_displayDispatchInfo;
		bool                      m_displayObjectPopulationReservationInfo;
		bool                      m_displayVisibilityInfo;
        bool                      m_displayConnectionMetrics;
		bool                      m_displayGhostInfo;
		bool					  m_displayChallengeFlyableAreas;
		NetworkDisplayTargets     m_displayTargets;
        NetworkDisplayInformation m_displayInformation;
    };

    NetworkDisplaySettings &GetDebugDisplaySettings();

    const netPlayer *GetPlayerFromUpdateRateIndex(int playerIndex);
    bool OverrideSpawnPoint();
    void ClearOverrideSpawnPoint();

    void AddPedNavmeshPosSyncRescan();
    void AddPedNavmeshPosGroundProbe();

    void AddManageUpdateLevelCall();

    void AddPedCloneCreate();
    void AddVehicleCloneCreate();
    void AddOtherCloneCreate();
    void AddPedCloneRemove();
    void AddVehicleCloneRemove();
    void AddOtherCloneRemove();

    void AddCloneCreateDuration(float time);
    void AddCloneRemoveDuration(float time);

    void AddInboundLatencySample(unsigned latencyMS);
    void AddOutboundLatencyCloneSyncSample(unsigned latencyMS);
    void AddOutboundLatencyConnectionSyncSample(unsigned latencyMS);

    bool IsPredictionFocusEntityID(ObjectId objectID);
    u32  GetPredictionFocusEntityUpdateAverageTime();
    void     AddPredictionPop(ObjectId objectID, NetworkObjectType objectType, bool fixedByNetwork);
    void     AddPredictionBBCheck();
    void     AddNumBBChecksBeforeWarp(u32 numChecks, ObjectId objectID);
    unsigned GetNumPredictionPopsPerSecond();
    unsigned GetNumPredictionBBChecksPerSecond();
    unsigned GetNumPredictionCollisionDisabled();
    unsigned GetMaxTimeWithNoCollision();
    unsigned GetMaxBBChecksBeforeWarp();

    void AddOrientationBlendFailure(phBound *bound1, phBound *bound2, Mat34V_In matrix1, Mat34V_In matrix2);

    void DisplayPredictionFocusOnVectorMap();

	bool IsTaskMotionPedDebuggingEnabled();
	bool IsTaskDebugStateLoggingEnabled();
	bool IsBindPoseCatchingEnabled();
    bool IsOwnershipChangeAllowed();
    bool IsProximityOwnershipAllowed();
    bool UsingPlayerCameras();
    bool ShouldSkipAllSyncs();
    bool ShouldIgnoreGiveControlEvents();
    bool ShouldIgnoreVisibilityChecks();
    bool IsPredictionLoggingEnabled();
	bool ArePhysicsOptimisationsEnabled();

	void ToggleDisplayObjectInfo();
	void ToggleDisplayObjectVisibilityInfo();

	void  ToggleLogExtraDebugNetworkPopulation();
	void  UnToggleLogExtraDebugNetworkPopulation();
	bool  LogExtraDebugNetworkPopulation();
	bool*  GetLogExtraDebugNetworkPopulation();

	rlClanId NetworkBank_GetLeaderboardClanId();

    bool ShouldForceCloneTasksOutOfScope(const CPed *ped);

    void LookAtAndPause(ObjectId objectID);

    void LogVehiclesUsingInappropriateModels();
    void LogPedsUsingInappropriateModels();

	bool DebugPedMissingDamageEvents();

	u32 RemotePlayerFadeOutGetTimer();

	bool ShouldForceNetworkCompliance();

	static u32 g_NetworkHeapUsage[NET_MEM_NUM_MANAGERS];

#else

	inline bool IsOwnershipChangeAllowed()      { return true;  }
    inline bool IsProximityOwnershipAllowed()   { return true;  }
    inline bool UsingPlayerCameras()            { return true;  }
    inline bool ShouldSkipAllSyncs()            { return false; }
    inline bool ShouldIgnoreGiveControlEvents() { return false; }
    inline bool ShouldIgnoreVisibilityChecks()  { return false; }
    inline bool IsPredictionLoggingEnabled()    { return false; }

#endif // __BANK

#if __DEV
    CPed *GetCameraTargetPed();
#endif // ___DEV

}

#endif  // NETWORK_DEBUG_H
