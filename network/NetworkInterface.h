//
// name:        NetworkInterface.h
// description: This is the new interface the game should use to access the network code. It is intended to reduce the
//              number of network header files that are included by game systems to an absolute minimum
// written by:  Daniel Yelland
//

#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

// rage headers
#include "atl/delegate.h"
#include "net/packet.h"
#include "rline/ros/rlros.h"

// framework headers
#include "fwnet/netinterface.h"
#include "fwnet/netgameobjectbase.h"

// game headers
#include "network/Network.h"
#include "network/NetworkTypes.h"
#include "network/Live/livemanager.h"
#include "network/Objects/NetworkObjectMgr.h"
#include "network/Players/NetworkPlayerMgr.h"
#include "peds/pedDefines.h"
#include "renderer/HierarchyIds.h"
#include "task/Physics/NmDefines.h"
#include "frontend/hud_colour.h"

class CDummyPed;
class CEntity;
class CDoor;
class CDoorSystemData;
class CDynamicEntity;
class CGameArrayMgr;
class CGameScriptHandler;
class CGameScriptId;
class CNetGamePlayer;
class CNetObjDoor;
class CNetObjGame;
class CNetSessionStatsMgr;
class CNetworkBandwidthManager;
class CNetworkParty;
class CNetworkScratchpads;
class CNetworkSession;
class CNetworkTextChat;
class CNetworkVoice;
class CPed;
class CPhysical;
class CProjectBaseSyncDataNode;
class CVehicle;
class InviteMgr;
class NetworkGameConfig;
class NetworkGameFilter;

namespace rage
{
    class aiTask;
    class fwBoxStreamerSearch;
    class fwEvent;
    class netConnectionManager;
    class netEvent;
    class netEventMgr;
    class netLoggingInterface;
    class netObject;
    class netPlayer;
    class netRequestHandler;
    class netResponseHandler;
    class netRequest;
	class netScriptMgrBase;
    class netTimeSync;
    class netTransactionInfo;
    class rlFriend;
    class rlGamerId;
	class rlGamerHandle;
	class rlGamerInfo;
	class rlPeerInfo;
	class rlPresence;
    class Vector3;
	class richPresenceMgr;
	enum rlNetworkMode;
}

class NetworkInterface
{
public:

	CompileTimeAssert(sizeof(PlayerFlags)*8  >= (size_t) MAX_NUM_PHYSICAL_PLAYERS);

	//Return references to subsystem interfaces.
	static CNetworkPlayerMgr&           GetPlayerMgr()					{ return CNetwork::GetPlayerMgr(); }
	static CNetworkVoice&               GetVoice()						{ return CNetwork::GetVoice(); }
	static netTimeSync&                 GetNetworkClock()				{ return CNetwork::GetNetworkClock(); }
	static CNetworkObjectMgr&           GetObjectManager()				{ return CNetwork::GetObjectManager(); }
	static netEventMgr&					GetEventManager()				{ return CNetwork::GetEventManager();}
	static CGameArrayMgr&				GetArrayManager()				{ return CNetwork::GetArrayManager(); }
	static netLoggingInterface&			GetObjectManagerLog()			{ return CNetwork::GetObjectManager().GetLog(); }
	static netLoggingInterface&			GetMessageLog()					{ return CNetwork::GetMessageLog(); }
	static CNetworkScratchpads&			GetScriptScratchpads()			{ return CNetwork::GetScriptScratchpads(); }
	static CNetworkBandwidthManager&	GetBandwidthManager()			{ return CNetwork::GetBandwidthManager(); }
	static unsigned						GetNetworkTime()				{ return CNetwork::GetNetworkTime(); }
	static unsigned						GetSyncedTimeInMilliseconds()	{ return CNetwork::GetSyncedTimeInMilliseconds(); }
	static CNetGamePlayer*				GetLocalPlayer()				{ return (CNetGamePlayer *)GetPlayerMgr().GetMyPlayer(); }
	static PhysicalPlayerIndex			GetLocalPhysicalPlayerIndex()	{ return GetLocalPlayer()->GetPhysicalPlayerIndex(); }
	

	#if __WIN32PC
	static CNetworkTextChat&   			GetTextChat()					{ return CNetwork::GetTextChat(); }
	static NetworkExitFlow&				GetNetworkExitFlow()			{ return CNetwork::GetNetworkExitFlow(); }
	#endif

	static const char*					GetLocalPlayerName()			{ return GetPlayerMgr().GetMyPlayer() ? GetPlayerMgr().GetMyPlayer()->GetLogName() : ""; }

	static InviteMgr&					GetInviteMgr();
	static richPresenceMgr&				GetRichPresenceMgr();

	// local player queries
	static int GetLocalGamerIndex();
	static bool IsLocalPlayerOnline(bool bLanCounts = true);
	static bool IsLocalPlayerGuest();
	static bool IsLocalPlayerSignedIn();
	static const char* GetLocalGamerName();
	static const rlGamerHandle& GetLocalGamerHandle();
	static const rlGamerInfo* GetActiveGamerInfo();

	static bool IsCloudAvailable(int localGamerIndex = GAMER_INDEX_LOCAL);
	static bool IsOnlineRos();

	static bool HasValidRosCredentials(const int localGamerIndex = GAMER_INDEX_LOCAL);
	static bool IsRefreshingRosCredentials(const int localGamerIndex = GAMER_INDEX_LOCAL);
	static bool HasCredentialsResult(const int localGamerIndex = GAMER_INDEX_LOCAL);
	static bool HasValidRockstarId();
	static bool HasRosPrivilege(rlRosPrivilegeId id, const int localGamerIndex = GAMER_INDEX_LOCAL);

	static bool IsRockstarQA();
    static bool CheckOnlinePrivileges();
    static bool HadOnlinePermissionsPromotionWithinMs(const unsigned nWithinMs);
	static bool HasNetworkAccess();

	static bool IsReadyToCheckLandingPageStats();
	static bool IsReadyToCheckProfileStats();
	static bool IsReadyToCheckStats(); 
	static bool HasActiveCharacter();
	static int GetActiveCharacterIndex();
	static bool HasCompletedMultiplayerIntro();

#if RSG_SCE
    static bool HasRawPlayStationPlusPrivileges();
	static bool HasActivePlayStationPlusEvent();
    static bool IsPlayStationPlusPromotionEnabled();
	static bool IsPlayStationPlusCustomUpsellEnabled();
    static bool DoesTunableDisablePlusLoadingButton();
    static bool DoesTunableDisablePlusEntryPrompt();
    static bool DoesTunableDisablePauseStoreRedirect();
	static bool IsPlayingInBackCompatOnPS5();
#endif

	__forceinline static bool IsMigrationAvailable();

	// NB: we want to forceinline this, so hackers can't stub the function to "return true;"
	// (See url:bugstar:1997858; this is exactly what they've done).
	// Certainly not bullet-proof, but better than nothing.
	__forceinline static bool IsRockstarDev();
	static int GetGamerIndexFromCLiveManager(); // Needed for IsRockstarDev to be inlined

	// network queries
	static bool IsMultiplayerLocked()	{ return CNetwork::IsMultiplayerLocked(); }
	static bool IsNetworkOpening()		{ return CNetwork::IsNetworkOpening(); }
	static bool IsNetworkOpen()			{ return CNetwork::IsNetworkOpen(); }
	static bool IsNetworkClosing()		{ return CNetwork::IsNetworkClosing(); }
	static bool IsNetworkClosePending()	{ return CNetwork::IsNetworkClosePending(); }
    static bool IsGameInProgress()		{ return CNetwork::IsGameInProgress(); }
	static bool IsForcingCrashOnShutdownWhileProcessing() { return CNetwork::IsGameInProgress() ? CNetwork::GetObjectManager().GetForceCrashOnShutdownWhileProcessing() : false; }
	static bool IsTransitioning();
    static bool IsActivitySession();
    static bool IsLaunchingTransition();
    static bool IsTransitionActive();
	static bool CanPause();
	static bool IsApplyingMultiplayerGlobalClockAndWeather();

	// network access
	static bool IsReadyToCheckNetworkAccess(const eNetworkAccessArea accessArea, IsReadyToCheckAccessResult* pResult = nullptr);
	static bool CanAccessNetworkFeatures(const eNetworkAccessArea accessArea, eNetworkAccessCode* nAccessCode = nullptr);
	static eNetworkAccessCode CheckNetworkAccess(const eNetworkAccessArea accessArea, u64* endPosixTime = nullptr);
	static bool IsUsingCompliance();

	// bailing
	static bool CanBail(); 
	static void Bail(eBailReason nBailReason, int nBailContext);

	// session state queries
	static bool IsSessionActive();
	static bool IsInSession();
	static bool IsSessionEstablished();
	static bool IsSessionLeaving();
	static bool IsSessionBusy();
    
    // multiplayer checks
	static bool IsAnySessionActive();
	static bool IsInAnyMultiplayer();
	static bool IsInOrTranitioningToAnyMultiplayer();
	static bool IsTransitioningToMultiPlayer();
	static bool IsTransitioningToSinglePlayer();
	static bool IsInSoloMultiplayer();
	static bool IsInClosedSession();
	static bool IsInPrivateSession();
	static bool AreAllSessionsNonVisible();

	// checks if session is in suitable state before performing operations
	static bool CanSessionStart();
	static bool CanSessionLeave();

	// used for telemetry metrics
	static SessionTypeForTelemetry GetSessionTypeForTelemetry();

	// match functions
	static const NetworkGameConfig& GetMatchConfig();
	static void NewHost();

	// session functions
	static bool DoQuickMatch(const int nGameMode, const int nQueryID, const unsigned nMaxPlayers, unsigned matchmakingFlags);
	static bool DoSocialMatchmaking(const char* szQuery, const char* szParams, const int nGameMode, const int nQueryID, const unsigned nMaxPlayers, unsigned matchmakingFlags);
	static bool DoActivityQuickmatch(const int nGameMode, const unsigned nMaxPlayers, const int nActivityType, const int nActivityId, unsigned matchmakingFlags);
	static bool HostSession(const int nGameMode, const int nMaxPlayers, const unsigned nHostFlags);
	static bool LeaveSession(bool bReturnToLobby, bool bBlacklist);
	static bool LeaveSession(const unsigned leaveFlags);
	static void SetScriptValidateJoin();
	static void ValidateJoin(bool bJoinSuccessful);

	static bool IsMigrating();

	// session queries
	static bool IsHost();
    static bool IsHost(const rlSessionInfo& hInfo);
    static bool IsASpectatingPlayer(CPed *ped);
	static bool IsSpectatingLocalPlayer(const PhysicalPlayerIndex playerIdx);
	static bool IsInSpectatorMode();
	static bool IsPlayerPedSpectatingPlayerPed(const CPed* spectatorPlayerPed, const CPed* otherplayerped);
	static bool IsSpectatingPed(const CPed* otherplayerped);
	static bool IsInMPTutorial() {	return CNetwork::IsInMPTutorial(); }
	static bool IsInMPCutscene() { return CNetwork::IsInMPCutscene(); }
	static bool IsEntityHiddenForMPCutscene(const CEntity* pEntity);
	static bool IsEntityHiddenNetworkedPlayer(const CEntity* pEntity);
	static bool AreCutsceneEntitiesNetworked() { return CNetwork::AreCutsceneEntitiesNetworked(); }
	static bool LanSessionsOnly();
	static rlNetworkMode GetNetworkMode();
	static bool AllowNaturalMotion();
    static bool NetworkClockHasSynced();
    static bool IsTeamGame();
	static bool IsLocalPlayerOnSCTVTeam();
	static bool IsOverrideSpectatedVehicleRadioSet();
	static void SetOverrideSpectatedVehicleRadio(bool bValue);
    static bool FriendlyFireAllowed();
	static bool	IsFriendlyFireAllowed(const CPed* victimPed, const CEntity* inflictorEntity = NULL);
	static bool	IsFriendlyFireAllowed(const CVehicle* victimVehicle, const CEntity* inflictorEntity = NULL);
	static const CPed* FindDriverPedForVehicle(const CVehicle* victimVehicle);
	static bool IsPedAllowedToDamagePed(const CPed& inflictorPed, const CPed& victimPed);
	static bool AllowCrimeOrThreatResponsePlayerJustResurrected(const CPed* pSourcePed, const CEntity* pTargetEntity);
    static bool IsInFreeMode();
	static bool IsInArcadeMode();
	static bool IsInCopsAndCrooks();
	static bool IsInFakeMultiplayerMode();
	static u16 GetNetworkGameMode();
	static bool CanRegisterLocalAmbientPeds(const unsigned numPeds);
    static bool CanRegisterObject(const unsigned objectType, const bool isMissionObject);
    static bool CanRegisterObjects(const unsigned numPeds, const unsigned numVehicles, const unsigned numObjects, const unsigned numPickups, const unsigned numDoors, const bool areMissionObjects);
    static bool CanRegisterRemoteObjectOfType(const unsigned objectType, const bool isMissionObject, bool isReservedObject);
    static bool IsCloseToAnyPlayer(const Vector3& position, const float radius, const netPlayer*&pClosestPlayer);
    static bool IsCloseToAnyRemotePlayer(const Vector3& position, const float radius, const netPlayer*&pClosestPlayer);
    static bool IsVisibleOrCloseToAnyPlayer(const Vector3& position, const float radius, const float distance, const float minDistance, const netPlayer** ppVisiblePlayer = NULL, const bool predictFuturePosition = false, const bool bIgnoreExtendedRange = false);
    static bool IsVisibleOrCloseToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, const float minDistance, const netPlayer** ppVisiblePlayer = NULL, const bool predictFuturePosition = false, const bool bIgnoreExtendedRange = false);
    static bool IsVisibleToAnyRemotePlayer(const Vector3& position, const float radius, const float distance, const netPlayer** ppVisiblePlayer = NULL);
	static bool IsVisibleToPlayer(const CNetGamePlayer* pPlayer, const Vector3& position, const float radius, const float distance, const bool bIgnoreExtendedRange = false);
	static unsigned GetClosestRemotePlayers(const Vector3& position, const float radius, const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS], const bool sortResults = true, bool alwaysGetPlayerPos = false);

	static unsigned GetNumActivePlayers();
    static unsigned GetNumPhysicalPlayers();

	static CNetGamePlayer* GetPlayerFromConnectionId(const int cxnId);
    static CNetGamePlayer* GetPlayerFromEndpointId(const EndpointId endpointId);
    static CNetGamePlayer* GetPlayerFromGamerId(const rlGamerId& gamerId);
	static ActivePlayerIndex GetActivePlayerIndexFromGamerHandle(const rlGamerHandle& gamerHandle);
	static PhysicalPlayerIndex GetPhysicalPlayerIndexFromGamerHandle(const rlGamerHandle& gamerHandle);
	static CNetGamePlayer* GetPlayerFromGamerHandle(const rlGamerHandle& gamerHandle);
    static CNetGamePlayer* GetPlayerFromPeerId(const u64 peerId);
	static CNetGamePlayer* GetActivePlayerFromIndex(const ActivePlayerIndex playerIndex);
	static CNetGamePlayer* GetPhysicalPlayerFromIndex(const PhysicalPlayerIndex playerIndex);
	static CNetGamePlayer* GetLocalPlayerFromPeerPlayerIndex(const unsigned peerPlayerIndex);
    static CNetGamePlayer* GetPlayerFromPeerPlayerIndex(const unsigned peerPlayerIndex, const rlPeerInfo &peerInfo);

	static CNetGamePlayer*	GetHostPlayer();
	static const char* GetHostName();

	static const char* GetFreemodeHostName();

	// party functions
	static bool HostParty();
	static bool LeaveParty();
	static bool JoinParty(const rlSessionInfo& sessionInfo);
	static bool IsInParty();
	static bool IsPartyLeader();
	static bool IsPartyMember(const rlGamerHandle* pGamerHandle);
	static bool IsPartyMember(const CPed* pPed);
	static unsigned GetPartyMemberCount();
	static unsigned GetPartyMembers(rlGamerInfo* pInfoBuffer, const unsigned maxMembers);
	static bool IsPartyInvitable();
	static bool InviteToParty(const rlGamerHandle* pHandles, const unsigned numGamers);
	static bool IsPartyStatusPending();
	static bool NotifyPartyEnteringGame(const rlSessionInfo& sessionInfo);
	static bool NotifyPartyLeavingGame();

	// transition checks
	static bool IsOnLoadingScreen();
	static bool IsOnLandingPage();
	static bool IsOnInitialInteractiveScreen();
	static bool IsOnBootUi();
	static bool IsOnUnskippableBootUi();
	static bool IsInPlayerSwitch();

	// UI access for compliance
	static void CloseAllUiForInvites();

	//Returns the that player focus position. If the player is in spectator mode
	//then it will return the position of the player it is spectating.
	static Vector3 GetPlayerFocusPosition(const netPlayer& player, bool* bPlayerPedAtFocus = NULL, bool alwaysGetPlayerPos = false, unsigned *playerFocus = nullptr);

	static Vector3 GetLocalPlayerFocusPosition(bool* bPlayerPedAtFocus = NULL)
	{
		return GetPlayerFocusPosition(*GetLocalPlayer(), bPlayerPedAtFocus);
	}

	//Returns the that player camera position. This is not necessarily the same as the focus position. In some cases it can differ (eg if the player
	// is a passenger in a vehicle while in cinematic cam. The focus position is the player ped here. )
	static Vector3 GetPlayerCameraPosition(const netPlayer& player);

	static bool RegisterObject(netGameObjectWrapper<CDynamicEntity> pGameObject,
                        const NetObjFlags localFlags = 0,
                        const NetObjFlags globalFlags = 0,
                        const bool        makeSpaceForObject = false);

    static void UnregisterObject(netGameObjectWrapper<CDynamicEntity> pGameObject, bool bForce = false);

	static netObject *GetNetworkObject(const ObjectId objectID);

	//PURPOSE
	//  Sets/UnSets the player in Spectator mode.
	static bool CanSpectateObjectId(netObject* netobj, bool skipTutorialCheck);
	static void SetSpectatorObjectId(const ObjectId objectId);
	static ObjectId GetSpectatorObjectId();

	//  Sets/UnSets the local player in a multiplayer cutscene
	static void SetInMPCutscene(bool bInCutscene, bool bMakePlayerInvincible = false);
	static void SetInMPCutsceneDebug(bool bInCutscene);

    static void SetProcessLocalPopulationLimits(bool processLimits);

	static void SetUsingExtendedPopulationRange(const Vector3& center);
	static void ClearUsingExtendedPopulationRange();

    static bool HasBeenDamagedBy(netObject *networkObject, const CPhysical *physical);
    static void ClearLastDamageObject(netObject *networkObject);

	static void SwitchRoadNodesOnOverNetwork(const CGameScriptId &scriptID,
		const Vector3       &regionMin,
		const Vector3       &regionMax);

	static void SwitchRoadNodesOffOverNetwork(const CGameScriptId &scriptID,
		const Vector3       &regionMin,
		const Vector3       &regionMax);

	static void SetRoadNodesToOriginalOverNetwork(const CGameScriptId &scriptID,
		const Vector3       &regionMin,
		const Vector3       &regionMax);

	static void SwitchRoadNodesOnOverNetwork(const CGameScriptId &scriptID,
		const Vector3       &regionStart,
		const Vector3       &regionEnd,
		float				 regionWidth);

	static void SwitchRoadNodesOffOverNetwork(const CGameScriptId &scriptID,
		const Vector3       &regionStart,
		const Vector3       &regionEnd,
		float				 regionWidth);

	static void SetRoadNodesToOriginalOverNetwork(const CGameScriptId &scriptID,
		const Vector3       &regionMin,
		const Vector3       &regionMax,
		float				 regionWidth);

	static void AddPopMultiplierAreaOverNetwork(const CGameScriptId &scriptID,
		const Vector3		&_minWS,
		const Vector3		&_maxWS,
		const float			 pedDensity,
		const float			 trafficDensity,
		const bool			 sphere,
		const bool			 bCameraGlobalMultiplier);

	static bool FindPopMultiplierAreaOverNetwork(const CGameScriptId &scriptID,
		const Vector3		&_minWS,
		const Vector3		&_maxWS,
		const float			 pedDensity,
		const float			 trafficDensity,
		const bool			 sphere,
		const bool			 bCameraGlobalMultiplier);

	static void RemovePopMultiplierAreaOverNetwork(const CGameScriptId &scriptID,
		const Vector3		&_minWS,
		const Vector3		&_maxWS,
		const float			_pedDensity,
		const float			_trafficDensity,
		const bool			_sphere,
		const bool			 bCameraGlobalMultiplier);
	
	static void SetModelHashPlayerLock_OverNetwork(const CGameScriptId &scriptID,
		int const PlayerIndex, 
		u32 const ModelId, 
		bool const Lock,
		u32 const index);

	static void SetModelInstancePlayerLock_OverNetwork(const CGameScriptId &scriptID,
		int const PlayerIndex, 
		int const VehicleIndex, 
		bool const Lock,
		u32 const index);

	static void ClearModelHashPlayerLockForPlayer_OverNetwork(const CGameScriptId& scriptID, u32 const PlayerIndex, u32 const ModelHash, bool const lock, u32 const index);
	static void ClearModelInstancePlayerLockForPlayer_OverNetwork(const CGameScriptId& scriptID, u32 const PlayerIndex, u32 const InstanceIndex, bool const lock, u32 const index);

	static void RegisterScriptDoor(CDoorSystemData& doorSystemData, bool network);
	static void UnregisterScriptDoor(CDoorSystemData& doorSystemData);

    static void SwitchCarGeneratorsOnOverNetwork(const CGameScriptId &scriptID,
                                          const Vector3       &regionMin,
                                          const Vector3       &regionMax);

    static void SwitchCarGeneratorsOffOverNetwork(const CGameScriptId &scriptID,
                                           const Vector3       &regionMin,
                                           const Vector3       &regionMax);

    static void OverridePopGroupPercentageOverNetwork(const CGameScriptId &scriptID,
                                               int                  popSchedule,
                                               u32                  popGroupHash,
                                               u32                  percentage);

    static void RemovePopGroupPercentageOverrideOverNetwork(const CGameScriptId &scriptID,
                                                     int                  popSchedule,
                                                     u32                  popGroupHash,
                                                     u32                  percentage);

    static void CreateRopeOverNetwork(const CGameScriptId &scriptID,
                               int                  ropeID,
                               const Vector3       &position,
                               const Vector3       &rotation,
                               float                maxLength,
                               int                  type,
                               float                initLength,
                               float                minLength,
                               float                lengthChangeRate,
                               bool                 ppuOnly,
                               bool                 collisionOn,
                               bool                 lockFromFront,
                               float                timeMultiplier,
                               bool                 breakable,
							   bool					pinned
							   );
    static void DeleteRopeOverNetwork(const CGameScriptId &scriptID, int ropeID);

    static void StartPtFXOverNetwork(const CGameScriptId &scriptID,
                              u32                  ptFXHash,
                              u32                  ptFXAssetHash,
                              const Vector3       &fxPos,
                              const Vector3       &fxRot,
                              float                scale,
                              u8                   invertAxes,
                              int                  scriptPTFX,
							  CEntity*			   pEntity = NULL,
							  int				   boneIndex = -1,
							  float				   r = 1.0f,
							  float				   g = 1.0f,
							  float				   b = 1.0f,
							  bool				   terminateOnOwnerLeave = false,
							  u64                  ownerPeerID = 0);

	static void ModifyEvolvePtFxOverNetwork(const CGameScriptId &scriptID, u32 evoHash, float evoVal, int scriptPTFX);

	static void ModifyPtFXColourOverNetwork(const CGameScriptId &scriptID, float r, float g, float b, int scriptPTFX);

    static void StopPtFXOverNetwork(const CGameScriptId &scriptID,
                             int                  scriptPTFX);

    static void AddScenarioBlockingAreaOverNetwork(const CGameScriptId &scriptID,
                                            const Vector3       &regionMin,
                                            const Vector3       &regionMax,
                                            const int            blockingAreaID,
											bool				bCancelActive,
											bool				bBlocksPeds,
											bool				bBlocksVehicles);

    static void RemoveScenarioBlockingAreaOverNetwork(const CGameScriptId &scriptID,
                                               const int            blockingAreaID);

    static int GetLocalRopeIDFromNetworkID(int networkRopeID);

    static int GetNetworkRopeIDFromLocalID(int localRopeID);

    static bool IsPosLocallyControlled(const Vector3 &position);
	static bool AreThereAnyConcealedPlayers();

    static void ForceSynchronisation(CDynamicEntity *entity);

    static bool IsPedInVehicleOnOwnerMachine(CPed *ped);

	static bool IsVehicleLockedForPlayer(const CVehicle* pVeh, const CPed* pPlayerPed);

	static void SetTargetLockOn(const CVehicle* vehicle, unsigned lockOnState);

	static bool IsInvalidVehicleForDriving(const CPed* pPed, const CVehicle* pVehicle);

	static bool IsInvalidVehicleForAmbientPedDriving(const CVehicle* pVehicle);

    static bool IsVehicleInterestingToAnyPlayer(const CVehicle *pVehicle);

    static void IgnoreVehicleUpdates(CPed *ped, u16 timeToIgnore);

    static void ForceVehicleForPedUpdate(CPed *ped);

    static bool CanUseRagdoll(CPed *ped, eRagdollTriggerTypes trigger, CEntity *entityResponsible, float pushValue);

    static void OnSwitchToRagdoll(CPed &ped, bool pedSetOutOfVehicle);

	static void ForceMotionStateUpdate(CPed &ped);
	static void ForceTaskStateUpdate(CPed &ped);
	static void ForceHealthNodeUpdate(CVehicle &vehicle);
	static void ForceScriptGameStateNodeUpdate(CVehicle &vehicle);
	
    static void ForceCameraUpdate(CPed &ped);

    static void OnEntitySmashed(CPhysical *physical, s32 componentId, bool smashed);
	
    static void OnExplosionImpact(CPhysical *physical, float fForceMag);
	
	static void ForceHighUpdateLevel(CPhysical* physical);

    static void OnVehiclePartDamage(CVehicle *vehicle, eHierarchyId ePart, u8 state);

	static bool ShouldTriggerDamageEventForZeroDamage(CPhysical* physical);

    static void GivePedScriptedTask(CPed *ped, aiTask *task, const char* commandName);

    static int GetPendingScriptedTaskStatus(const CPed *ped);

    static Vector3  GetLastPosReceivedOverNetwork (const CDynamicEntity *entity);
    static Vector3  GetLastVelReceivedOverNetwork (const CDynamicEntity *entity);
    static Matrix34 GetLastMatrixReceivedOverNetwork(const CVehicle *vehicle);
    static Vector3  GetPredictedVelocity          (const CDynamicEntity *entity, const float maxSpeedToPredict);
    static Vector3  GetPredictedAccelerationVector(const CDynamicEntity *entity);

    static float GetLastHeadingReceivedOverNetwork(const CPed *ped);

    static void GoStraightToTargetPosition(const CDynamicEntity *entity);
	static void OverrideNetworkBlenderForTime(const CPhysical *physical, unsigned timeToOverrideBlender);
    static void OverrideNetworkBlenderUntilAcceptingObjects(const CPhysical *physical);
    static void OverrideNetworkBlenderTargetPosition(const CPed *ped, const Vector3 &position);
	static void DisableNetworkZBlendingForRagdolls(const CPed &ped);
    static void UpdateHeadingBlending(const CPed &ped);
    static void UseAnimatedRagdollFallbackBlending(const CPed &ped);
    static void OnDesiredVelocityCalculated(const CPed &ped);

    static void OverrideMoveBlendRatiosThisFrame(CPed &ped, float mbrX, float mbrY);

	static bool IsInSinglePlayerPrivateSession();
	static bool IsInTutorialSession();
	static bool IsTutorialSessionChangePending();
	static bool IsPlayerInTutorialSession(const CNetGamePlayer &player);
	static u8 GetPlayerTutorialSessionIndex(const CNetGamePlayer &player);
	static bool IsPlayerTutorialSessionChangePending(const CNetGamePlayer &player);
	static bool ArePlayersInDifferentTutorialSessions(const CNetGamePlayer &firstPlayer, const CNetGamePlayer &secondPlayer);

    static bool IsEntityConcealed(const CEntity &entity);

    static bool IsClockTimeOverridden();

	static eHUD_COLOURS GetWaypointOverrideColor();
	static void ResolveWaypoint(); 
	static bool CanSetWaypoint(CPed* pPed);
	static bool IsRelevantToWaypoint(CPed* pPed);
	static void IgnoreRemoteWaypoints();

	static void RegisterHeadShotWithNetworkTracker(CPed const* ped);
	static void RegisterKillWithNetworkTracker(CPhysical const* physical);

	static bool	RequestNetworkAnimRequest(CPed const* pPed, u32 const animHash, bool bRemoveExistingRequests = false);
	static bool	RemoveAllNetworkAnimRequests(CPed const* pPed);

    static bool IsRemotePlayerInNonClonedVehicle(const CPed *ped);
    static void GetPlayerPositionForBlip(const CPed &playerPed, Vector3 &blipPosition);

    static u32 GetNumVehiclesOutsidePopulationRange();

    static bool GetLowestPedModelStartOffset(u32 &lowestStartOffset, const char **playerName = 0);
    static bool GetLowestVehicleModelStartOffset(u32 &lowestStartOffset, const char **playerName = 0);

    static void OnQueriableStateBuilt(CPed &ped);

    static void RegisterCollision(CPhysical &physical, CEntity *hitEntity, float impulseMag);

    static bool IsRemotePlayerStandingOnObject(const CPed &ped, const CPhysical &physical);

	static void UpdateBeforeScripts();
	static void UpdateAfterScripts();

    static void AddBoxStreamerSearches(atArray<fwBoxStreamerSearch>& searchList);

    static void DelayMigrationForTime(const CPhysical *physical, unsigned timeToDelayMigration);

    static void  GetRemotePlayerCameraMatrix(CPed *playerPed, Matrix34 &cameraMatrix);
    static float GetRemotePlayerCameraFov(CPed *playerPed);
	static bool  IsRemotePlayerUsingFreeCam(const CPed *playerPed);

	static  void  LocalPlayerVoteToKickPlayer(netPlayer& player);
	static  void  LocalPlayerUnvoteToKickPlayer(netPlayer& player);

	static bool IsLocalPlayersTurn(Vec3V_In vPosition, float fMaxDistance, float fDurationOfTurns, float fTimeBetweenTurns);

	static CPed* FindClosestWantedPlayer(const Vector3& vPosition, const int minWantedLevel);

    static void OnAddedToGameWorld(netObject *networkObject);

    static void OnClonePedDetached(netObject *networkObject);

    static bool AreCarGeneratorsDisabled();

    static bool IsPlayerUsingLeftTriggerAimMode(const CPed *pPed);

	static bool IsDamageDisabledInMP(const CEntity& entity1, const CEntity& entity2);
	static bool AreInteractionsDisabledInMP(const CEntity& entity1, const CEntity& entity2);
	static bool AreBulletsImpactsDisabledInMP(const CEntity& firingEntity, const CEntity& hitEntity);
	static void RegisterDisabledCollisionInMP(const CEntity& entity1, const CEntity& entity2);

	static bool IsAGhostVehicle(const CVehicle& vehicle, bool bIsFullyGhosted = true);

    static bool CanSwitchToSwimmingMotion(CPed *pPed);

	static bool CanMakeVehicleIntoDummy(CVehicle* pVehicle);

    static bool CanScriptModifyRemoteAttachment(const CPhysical &physical);
    static void SetScriptModifyRemoteAttachment(const CPhysical &physical, bool canModify);

	static void OnAIEventAdded(const fwEvent& event);

	static bool IsEntityPendingCloning(const CEntity* pEntity);
	static bool AreEntitiesClonedOnTheSameMachines(const CEntity* pEntity1, const CEntity* pEntity2);

	static void LocallyHideEntityForAFrame(const CEntity& entity, bool bIncludeCollision, const char* reason);

	static void CutsceneStartedOnEntity(CEntity& entity);
	static void CutsceneFinishedOnEntity(CEntity& entity);

	static void SetLocalDeadPlayerWeaponPickupAndObject(class CPickup* pWeaponPickup, CObject* pWeaponObject);

	static void NetworkedPedPlacedOutOfVehicle(CPed& ped, CVehicle* pVehicle);

	static bool HasCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex);
	static void CachePlayerHeadBlendData(PhysicalPlayerIndex playerIndex, class CPedHeadBlendData& headBlendData);
	static const CPedHeadBlendData& GetCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex);
	static bool ApplyCachedPlayerHeadBlendData(CPed& ped, PhysicalPlayerIndex playerIndex);
	static void ClearCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex);
	static void BroadcastCachedLocalPlayerHeadBlendData(const netPlayer *pPlayerToBroadcastTo = NULL);

	static bool IsPlayerUsingScopedWeapon(const netPlayer& player, bool* lookingThroughScope = NULL, bool* scopeZoomed = NULL, float* weaponRange = NULL);

    // Connection Metrics
    static bool  IsConnectedViaRelay(PhysicalPlayerIndex playerIndex);
    static float GetAverageSyncLatency(PhysicalPlayerIndex playerIndex);
    static float GetAverageSyncRTT(PhysicalPlayerIndex playerIndex);
    static float GetAveragePacketLoss(PhysicalPlayerIndex playerIndex);
    static unsigned GetNumUnAckedReliables(PhysicalPlayerIndex playerIndex);
    static unsigned GetUnreliableResendCount(PhysicalPlayerIndex playerIndex);
    static unsigned GetHighestReliableResendCount(PhysicalPlayerIndex playerIndex);

	static void ForceSortNetworkObjectMap();
	static void AllowOtherThreadsAccessToNetworkManager(bool bAllowAccess);

	static void ForceVeryHighUpdateLevelOnVehicle(CVehicle* vehicle, u16 timer);
	static void SetSuperDummyLaunchedIntoAir(CVehicle* vehicle, u16 timer);
	static bool IsSuperDummyStillLaunchedIntoAir(const CVehicle* vehicle);


#if FPS_MODE_SUPPORTED
	static bool IsRemotePlayerInFirstPersonMode(const CPed& playerPed, bool* bInFirstPersonIdle = NULL, bool* bStickWithinStrafeAngle = NULL, bool* bOnSlope = NULL);
	static bool UseFirstPersonIdleMovementForRemotePlayer(const CPed& playerPed);
#endif

#if RSG_ORBIS
	static void EnableLiveStreaming(bool bEnable);
	static bool IsLiveStreamingEnabled();

	static void EnableSharePlay(bool bEnable);
	static bool IsSharePlayEnabled();
#endif

#if __DEV
	static CPed *GetCameraTargetPed();
#endif // __DEV

    static void RenderNetworkInfo(); // display OHD debug text over network objects
#if __BANK
    static void RegisterNodeWithBandwidthRecorder(CProjectBaseSyncDataNode &node, const char *recorderName);

	static void PrintNetworkInfo();  // display debug text for the network code

    static bool ShouldForceCloneTasksOutOfScope(const CPed *ped);

    static const char *GetLastRegistrationFailureReason();

#if __DEV
    static void DisplayNetworkingVectorMap();
#endif // __DEV
#endif // __BANK

    //////////////////////////////////////////////////////////////////////////
    // Messaging
    //////////////////////////////////////////////////////////////////////////

    //PURPOSE
    //  Sends a reliable message to the given player
    template<typename T>
    static bool SendReliableMessage(const netPlayer* player,
                            const T& msg,
                            netSequence* seq = NULL)
    {
        return SendMsg(player, msg, NET_SEND_RELIABLE, seq);
    }

    //PURPOSE
    //  Sends a message to the given player
    template<typename T>
    static bool SendMsg(const netPlayer* player,
                    const T& msg,
                    const unsigned sendFlags,
                    netSequence* seq = NULL)
    {
        return netInterface::SendMsg(player, msg, sendFlags, seq);
    }

    //PURPOSE
    //  Broadcasts a message to all players.
    template<typename T>
    static void BroadcastMsg(const T& msg,
                        const unsigned sendFlags)
    {
        u8 buf[netFrame::MAX_BYTE_SIZEOF_PAYLOAD];
        unsigned size;
        if(AssertVerify(msg.Export(buf, sizeof(buf), &size)))
        {
            unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
            const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();
            
	        for(unsigned index = 0; index < numRemoteActivePlayers; index++)
            {
                const netPlayer *player = remoteActivePlayers[index];
                netInterface::SendBuffer(player, buf, size, sendFlags);

#if __BANK
                if((sendFlags & NET_SEND_RELIABLE) != 0)
                {
                    GetPlayerMgr().IncreaseMiscReliableMessagesSent();
                }
                else
                {
                    GetPlayerMgr().IncreaseMiscUnreliableMessagesSent();
                }
#endif // __BANK
            }
        }
    }

    /*void AddDelegate(NetworkPlayerEventDelegate* dlgt);
    void RemoveDelegate(NetworkPlayerEventDelegate* dlgt);*/

    //////////////////////////////////////////////////////////////////////////
    // Transactions
    //////////////////////////////////////////////////////////////////////////

	static bool SendRequestBuffer(const netPlayer* player,
                            const void* buffer,
                            const unsigned numBytes,
                    		const unsigned timeout,
		                    netResponseHandler* handler);

	static bool SendResponseBuffer(const netTransactionInfo& txInfo,
                            const void* buffer,
                            const unsigned numBytes);

	template<typename T>
	static bool SendRequest(const netPlayer* player,
		            const T& msg,
		            const unsigned timeout,
		            netResponseHandler* handler)
	{
        u8 buf[netFrame::MAX_BYTE_SIZEOF_PAYLOAD];
        unsigned size;
        return msg.Export(buf, sizeof(buf), &size)
            && SendRequestBuffer(player, buf, size, timeout, handler);
	}

	template<typename T>
	static bool SendResponse(const netTransactionInfo& txInfo,
		            const T& msg)
	{
        u8 buf[netFrame::MAX_BYTE_SIZEOF_PAYLOAD];
        unsigned size;
        return msg.Export(buf, sizeof(buf), &size)
            && SendResponseBuffer(txInfo, buf, size);
	}

    ////PURPOSE
    ////  Adds/removes transaction request handlers (callbacks).
    //void AddRequestHandler(NetworkPlayerRequestHandler* rqstHandler);
    //void RemoveRequestHandler(NetworkPlayerRequestHandler* rqstHandler);

	//PURPOSE
	// Returns true when there are objects that we dont know if we need to pass control
	// or remove during respawn.
	static bool  PendingPopulationTransition();

	//////////////////////////////////////////////////////////////////////////
	// Logging
	//////////////////////////////////////////////////////////////////////////
	static void  WriteDataPopulationLog(const char* generatorname, const char* functionname, const char* eventName, const char* extraData, ...);

	static u64 GetExeSize( );
	static bool UsedVoiceChat();

	static void HACK_CheckAllPlayersInteriorFlag();

private:

#if __ASSERT
	static void IsGameObjectAllowedInMultiplayer(netGameObjectWrapper<CDynamicEntity> pGameObject);
	static bool IsPedAllowedInMultiplayer(netGameObjectWrapper<CDynamicEntity> pGameObject);
	static bool IsVehicleAllowedInMultiplayer(netGameObjectWrapper<CDynamicEntity> pGameObject);
#endif /* __ASSERT */

#if !__FINAL
	static bool DebugIsRockstarDev();
#endif
};

__forceinline bool NetworkInterface::IsRockstarDev()
{
	// do not compound with RosGamerSettings check
	const int localGamerIndex = GetGamerIndexFromCLiveManager();
	if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlRos::HasPrivilege(localGamerIndex, RLROS_PRIVILEGEID_DEVELOPER))
		return true;

#if !__FINAL
	// unless in non-FINAL
	if(DebugIsRockstarDev())
		return true;
#endif

	return false;
}

__forceinline bool NetworkInterface::IsMigrationAvailable()
{
#if __BANK
	if (CNetwork::IsSaveMigrationEnabled())
	{
		return true;
	}
#endif // __BANK

	bool result = false;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("ENABLE_SAVE_MIGRATION", 0x04190252), result))
	{
		return result;
	}
	return false;
}

#if !ENABLE_NETWORK_LOGGING
inline void  NetworkInterface::WriteDataPopulationLog(const char*, const char*, const char*, const char*, ...) {}
#endif 

#endif  // NETWORK_INTERFACE_H
