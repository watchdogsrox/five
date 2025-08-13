//
// name:        NetObjPlayer.cpp
// description: Derived from netObject, this class handles all player-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjPlayer.h"

// rage headers
#include "diag/output.h"

// framework headers
#include "grcore/debugdraw.h"
#include "fwnet/neteventmgr.h"
#include "fwnet/NetLogUtils.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscript/scriptinterface.h"
#include "fwdecorator/decoratorExtension.h"
#include "vfx/ptfx/ptfxattach.h"

// game headers
#include "audio/scriptaudioentity.h"
#include "camera/camInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "event/EventWeapon.h"
#include "frontend/MiniMap.h"
#include "frontend/MultiplayerGamerTagHud.h"
#include "game/Wanted.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Arrays/NetworkArrayHandlerTypes.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "network/Live/NetworkTelemetry.h"
#include "network/Debug/NetworkDebug.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectTypes.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "network/players/NetworkPlayerMgr.h"
#include "network/roaming/RoamingBubbleMgr.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Voice/NetworkVoice.h"
#include "PedGroup/PedGroup.h"
#include "peds/pedfactory.h"
#include "peds/PedIKSettings.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "peds/PedPhoneComponent.h"
#include "peds/pedpopulation.h"             // For CPedPopulation::AddPedToPopulationCount().
#include "peds/rendering/PedOverlay.h"
#include "peds/rendering/PedVariationDS.h"
#include "Peds/rendering/PedVariationStream.h"
#include "physics/physics.h"
#include "pickups/Pickup.h"
#include "renderer/DrawLists/drawListNY.h"
#include "scene/FocusEntity.h"
#include "scene/world/GameWorld.h"
#include "script/script_hud.h"
#include "stats/stats_channel.h"
#include "Stats/MoneyInterface.h"
#include "Stats/StatsInterface.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"            // For CStreaming::HasObjectLoaded().
#include "task/Animation/TaskScriptedAnimation.h"
#include "task/Animation/TaskMoVEScripted.h"
#include "task/Combat/TaskDamageDeath.h"
#include "task/Default/TaskPlayer.h"
#include "task/Default/TaskWander.h"
#include "vehicles/vehiclepopulation.h" // matching header
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "peds/PedWeapons/PedTargetEvaluator.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Peds/PedHelmetComponent.h"
#include "system/InfoState.h"

//OPTIMISATIONS_OFF()
NETWORK_OPTIMISATIONS()

// this is declared in speechaudio entity
extern const u32 g_NoVoiceHash;

static const u32 gs_nTalkersLookAtHash = ATSTRINGHASH("LookAtTalkers", 0x531865D);
static const u32 gs_nCloneLookAtHash = ATSTRINGHASH("LookAtClone", 0x893F3129);

#define STAT_UPDATE_RATE (240) //In Frames
u16 CNetObjPlayer::ms_statUpdateCounter = 0;
bool CNetObjPlayer::ms_forceStatUpdate = false;
bool CNetObjPlayer::ms_IsCorrupt = false;

static const float MIN_FOV                           = 7.5f;               // this is the minimum value of the FOV
static const float MIN_FOV_NO_SCOPE                  = 45.0f;              // this is the minimum value of the FOV wito
static const float MAX_FOV                           = 70.0f;              // this is the maximum value of the FOV
//static const float MAX_FOV_RANGE              = MAX_FOV - MIN_FOV;  // this is the range (max-min) of the FOV synced

static const unsigned MAX_TIME_TO_FADE_PLAYER_OUT = 3000;
static const unsigned TIME_TO_DELAY_BLENDING      = 100;

static const unsigned TIME_TO_THROTTLE_VOICELOUDNESS = 2000;

static const unsigned MAX_TIME_SEARCHLASTREFOCUSED_FLIPPING = 500;

#if __BANK
s32 CNetObjPlayer::ms_updateRateOverride = -1;
#endif

#if FPS_MODE_SUPPORTED
bool CNetObjPlayer::ms_bUseFPIdleMovementOnClones = false;
#endif

// if we are supporting network bots we need sync data for all players,
// in the final game we only need to sync our local player
#if ENABLE_NETWORK_BOTS
static const unsigned MAX_PLAYER_SYNC_DATA = MAX_NUM_PHYSICAL_PLAYERS;
#else
static const unsigned MAX_PLAYER_SYNC_DATA = 1;
#endif // ENABLE_NETWORK_BOTS

FW_INSTANTIATE_CLASS_POOL(CNetObjPlayer::CPlayerSyncData, MAX_PLAYER_SYNC_DATA, atHashString("CPlayerSyncData",0x81b3ac65));

CNetObjPlayer::CPlayerScopeData         CNetObjPlayer::ms_playerScopeData;
CPlayerSyncTree                        *CNetObjPlayer::ms_playerSyncTree;
eWantedLevel							CNetObjPlayer::ms_HighestWantedLevelInArea = WANTED_CLEAN;

// =======================
// Static helper functions
// =======================

#if __BANK
struct extents_t { Vec3V extents[8]; };

static void RenderViewport(const extents_t &e)
{
    grcDebugDraw::Line(e.extents[0], e.extents[1], Color_red);
    grcDebugDraw::Line(e.extents[1], e.extents[3], Color_red);
    grcDebugDraw::Line(e.extents[3], e.extents[2], Color_red);
    grcDebugDraw::Line(e.extents[2], e.extents[0], Color_red);

    grcDebugDraw::Line(e.extents[4], e.extents[5], Color_red);
    grcDebugDraw::Line(e.extents[5], e.extents[7], Color_red);
    grcDebugDraw::Line(e.extents[7], e.extents[6], Color_red);
    grcDebugDraw::Line(e.extents[6], e.extents[4], Color_red);

    grcDebugDraw::Line(e.extents[0], e.extents[4], Color_red);
    grcDebugDraw::Line(e.extents[1], e.extents[5], Color_red);
    grcDebugDraw::Line(e.extents[2], e.extents[6], Color_red);
    grcDebugDraw::Line(e.extents[3], e.extents[7], Color_red);

    grcDebugDraw::Line(e.extents[0], e.extents[3], Color_red);
    grcDebugDraw::Line(e.extents[1], e.extents[2], Color_red);
}
#endif//__BANK

// ================================================================================================================
// CNetViewPortWrapper
// ================================================================================================================

FW_INSTANTIATE_CLASS_POOL(CNetViewPortWrapper, MAX_NUM_PHYSICAL_PLAYERS, atHashString("CNetViewPortWrapper",0xc25f23df));

// ================================================================================================================
// CNetObjPlayer
// ================================================================================================================

static const unsigned int NET_PLAYER_DEFAULT_VIEWPORT_WIDTH        = 800;
static const unsigned int NET_PLAYER_DEFAULT_VIEWPORT_HEIGHT       = 600;
static const float        NET_PLAYER_DEFAULT_VIEWPORT_FOV          = 90.0f;
static const float        NET_PLAYER_DEFAULT_VIEWPORT_ASPECT_RATIO = static_cast<float>(NET_PLAYER_DEFAULT_VIEWPORT_WIDTH) / static_cast<float>(NET_PLAYER_DEFAULT_VIEWPORT_HEIGHT);
static const float        NET_PLAYER_DEFAULT_VIEWPORT_NEAR_CLIP    = 0.1f;
static const float        NET_PLAYER_DEFAULT_VIEWPORT_FAR_CLIP     = 1000.0f;
static const float        NET_PLAYER_MAX_VIEWPORT_FAR_CLIP         = 1000.0f;

// default looking at talkers to true
bool CNetObjPlayer::ms_bLookAtTalkers = true;

s16 CNetObjPlayer::m_postTutorialSessionChangeHoldOffPedGenTimer=0;

RegdPickup CNetObjPlayer::ms_pLocalPlayerWeaponPickup;
RegdObj CNetObjPlayer::ms_pLocalPlayerWeaponObject;

CPedHeadBlendData CNetObjPlayer::ms_cachedLocalPlayerHeadBlendData;
bool CNetObjPlayer::ms_hasCachedLocalPlayerHeadBlendData = 0;
CPedHeadBlendData CNetObjPlayer::ms_cachedPlayerHeadBlendData[MAX_NUM_PHYSICAL_PLAYERS]; // the default player head blend data without masks, etc
PlayerFlags CNetObjPlayer::ms_hasCachedPlayerHeadBlendData = 0;

CGameScriptId CNetObjPlayer::ms_ghostScriptId; 

CNetObjPlayer::CNetObjPlayer(CPed        *player,
                             const NetworkObjectType type,
                             const ObjectId          objectID,
                             const PhysicalPlayerIndex   playerIndex,
                             const NetObjFlags       localFlags,
                             const NetObjFlags       globalFlags) :
CNetObjPed(player, type, objectID, playerIndex, localFlags, globalFlags),
m_visibleOnscreen(false),
m_standingOnStreamedInNetworkObjectIntervalTimer(0),
m_respawnInvincibilityTimer(0),
m_TimeOfLastVisibilityCheck(0),
m_VisibilityQueryTimer(0),
m_RespawnNetObjId(NETWORK_INVALID_OBJECT_ID),
m_SpectatorObjectId(NETWORK_INVALID_OBJECT_ID),
m_AntagonisticPlayerIndex(INVALID_PLAYER_INDEX),
m_UseKinematicPhysics(false),
m_nLookAtIntervalTimer(0),
m_nLookAtTime(0),
m_pLookAtTarget(NULL),
m_nLookAtStartDelayTime(0),
m_lastResurrectionTime(0),
m_tutorialIndex(INVALID_TUTORIAL_INDEX),
m_tutorialInstanceID(INVALID_TUTORIAL_INSTANCE_ID),
m_pendingTutorialIndex(INVALID_TUTORIAL_INDEX),
m_pendingTutorialInstanceID(INVALID_TUTORIAL_INSTANCE_ID),
m_pendingTutorialSessionChangeTimer(0),
m_pendingTutorialPopulationClearedTimer(0),
m_bPendingTutorialSessionChange(false),
m_bWaitingPopulationClearedInTutorialSession(false),
m_AllowedPedModelStartOffset(0),
m_AllowedVehicleModelStartOffset(0),
m_bAlwaysClonePlayerVehicle(false),
m_WaypointLocalDirtyTimestamp(0),
m_bHasActiveWaypoint(false),
m_bOwnsWaypoint(false),
m_fVoiceLoudness(0.0f),
m_uThrottleVoiceLoudnessTime(0),
m_TargetVehicleIDForAnimStreaming(NETWORK_INVALID_OBJECT_ID),
m_TargetVehicleEntryPoint(-1),
m_UsingFreeCam(false),
m_UsingCinematicVehCam(false),
m_FadePlayerToZero(false),
m_MaxPlayerFadeOutTime(0),
m_bSyncLookAt(false),
m_bIsCloneLooking(false),
m_DecorationListCachedIndex(-1),
m_DecorationListTimeStamp(-1.0f),
m_DecorationListCrewEmblemVariation(-1),
m_bIsFriendlyFireAllowed(true),
m_bIsPassive(false),
m_PRF_BlockRemotePlayerRecording(false),
m_PRF_UseScriptedWeaponFirePosition(false),
m_PRF_IsSuperJumpEnabled(false),
m_PRF_IsShockedFromDOTEffect(false),
m_PRF_IsChokingFromDOTEffect(false),
m_statVersion(0),
m_prevTargetEntityCache(NULL),
m_pendingCameraDataToSet(false),
m_pendingTargetDataToSet(false),
m_SecondaryPartialSlowBlendDuration(false),
m_newSecondaryAnim(false),
m_bCopsWereSearching(false),
m_incidentNum(0),
m_bInsideVehicle(false),
m_bCrewPedColorSet(false),
m_bBattleAware(false),
m_VehicleJumpDown(false),
m_pendingDamage(0),
m_lastReceivedHealth(0),
m_lastReceivedHealthTimestamp(0),
m_bAbortNMShotReaction(false),
m_lastDamagePosition(VEC3_ZERO),
m_lastDamageDirection(VEC3_ZERO),
m_lastHitComponent(0),
m_TargetAimPosition(VEC3_ZERO),
m_bUseExtendedPopulationRange(false),
m_vExtendedPopulationRangeCenter(VEC3_ZERO),
m_networkedDamagePack(0),
m_canOwnerMoveWhileAiming(false),
m_pDeathResponseTask(NULL),
m_remoteHasMoney(true),
m_bRespawnInvincibilityTimerRespot(false),
m_bRespawnInvincibilityTimerWasSet(false),
m_addToPendingDamageCalled(false),
m_bRemoteReapplyColors(false),
m_MyVehicleIsMyInteresting(false),
m_DisableRespawnFlashing(false),
m_bDisableLeavePedBehind(false),
m_UsingLeftTriggerAimMode(false),
m_bCloneRespawnInvincibilityTimerCountdown(false),
m_bLastVisibilityStateReceived(true),
m_visibilityMismatchTimer(0),
m_bWantedVisibleFromPlayer(false),
m_bIgnoreTaskSpecificData(false),
m_remoteHasScarData(false),
m_sentScarCount(0),
m_remoteFakeWantedLevel(0),
m_pendingVehicleWeaponIndex(-1),
m_HasToApplyWeaponToVehicle(false)
#if FPS_MODE_SUPPORTED
, m_bUsingFirstPersonCamera(false)
, m_bUsingFirstPersonVehicleCamera(false)
, m_bInFirstPersonIdle(false)
, m_bStickWithinStrafeAngle(false)
, m_bUsingSwimMotionTask(false)
, m_bOnSlope(false)
#endif
#if !__FINAL
, m_bDebugInvincible(false)
#endif
, m_CityDensitySetting(1.0f)
, m_LatestTimeCanSpawnScenarioPed(ALLOW_PLAYER_GENERATE_SCENARIO_PED)
, m_LockOnTargetID(NETWORK_INVALID_OBJECT_ID)
, m_LockOnState(0)
, m_vehicleShareMultiplier(1.0f)
, m_ghostPlayerFlags(0)
#if __BANK
, m_ghostScriptInfo("")
#endif
, m_PlayerWaypointOwner(NETWORK_INVALID_OBJECT_ID)
, m_ConcealedOnOwner(false)
{
    m_pViewportWrapper = rage_new CNetViewPortWrapper();

    if (player)
    {
        m_viewportOffset = -player->GetTransform().GetB();
        m_viewportOffset = Add(m_viewportOffset, Vec3V(V_Z_AXIS_WZERO));
    }
    else
    {
        m_viewportOffset = Vec3V(VEC3_ZERO);
    }

    grcViewport *pViewport = GetViewport();

    if(AssertVerify(pViewport))
    {
        pViewport->SetWindow(0, 0, NET_PLAYER_DEFAULT_VIEWPORT_WIDTH, NET_PLAYER_DEFAULT_VIEWPORT_HEIGHT);
        pViewport->Perspective(NET_PLAYER_DEFAULT_VIEWPORT_FOV, NET_PLAYER_DEFAULT_VIEWPORT_ASPECT_RATIO, NET_PLAYER_DEFAULT_VIEWPORT_NEAR_CLIP, NET_PLAYER_DEFAULT_VIEWPORT_FAR_CLIP);
    }

    safecpy(m_DisplayName, "", MAX_PLAYER_DISPLAY_NAME);
	memset(m_stats, 0, sizeof(m_stats));

	//Activate Damage tracking - now this is active by default for player's
	ActivateDamageTracking();

    UpdateCachedTutorialSessionState();

	// Whenever anyone joins, reset the countdown (there's a lot going on at this time, unimportant messages like this tend to get dropped.)
	ms_statUpdateCounter = 0;
	ms_forceStatUpdate = true; // Should force the local player to update/upload the stats whenever a new player is added.
	
	memset(m_DecorationListCachedBits,0,sizeof(u32)*NUM_DECORATION_BITFIELDS);
}

CNetObjPlayer::~CNetObjPlayer()
{
#if __BANK
	CEntity::TraceNetworkVisFlagsRemovePlayer(GetObjectID());
#endif
    delete m_pViewportWrapper;
    m_pViewportWrapper = 0;

	if (m_pDeathResponseTask)
	{
		delete m_pDeathResponseTask;
	}
}

void CNetObjPlayer::CreateSyncTree()
{
    ms_playerSyncTree = rage_new CPlayerSyncTree(PLAYER_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjPlayer::DestroySyncTree()
{
    ms_playerSyncTree->ShutdownTree();
    delete ms_playerSyncTree;
    ms_playerSyncTree = 0;
}

netINodeDataAccessor *CNetObjPlayer::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IPlayerNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IPlayerNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjPed::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

//
// Name         :   SetRespawnInvincibilityTimer
// Purpose      :   sets a time interval for the player to be invincible for after a respawn
//
void CNetObjPlayer::SetRespawnInvincibilityTimer(s16 timer)
{
	respawnDebugf3("CNetObjPlayer::SetRespawnInvincibilityTimer timer[%d]",timer);

    m_respawnInvincibilityTimer = timer;

    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), GetLogName(), "%s\t%d\r\n", "SETTING_RESPAWN_TIMER", timer);
}

void CNetObjPlayer::SetDisableRespawnFlashing(bool disable)
{
    if(m_DisableRespawnFlashing != disable)
    {
        m_DisableRespawnFlashing = disable;

        if(m_DisableRespawnFlashing)
        {
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), GetLogName(), "%s\r\n", "DISABLING_RESPAWN_FLASHING");
        }
        else
        {
            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), GetLogName(), "%s\r\n", "ENABLING_RESPAWN_FLASHING");
        }
    }
}

//
// Name         :   GetAccurateRemotePlayerPosition
// Purpose      :   returns the most accurate position for a player ped based on network updates received (avoids blending errors)
//
void CNetObjPlayer::GetAccurateRemotePlayerPosition(Vector3 &position)
{
    CPed *playerPed = GetPlayerPed();

    if(playerPed)
    {
        if(!IsClone())
        {
            position = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
        }
        else
        {
            if(!playerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
            {
                CNetBlenderPed *netBlender = static_cast<CNetBlenderPed *>(GetNetBlender());

                if(netBlender)
                {
                    position = netBlender->GetLastPositionReceived();
                }
            }
            else
            {
                CVehicle *playerVehicle = playerPed->GetMyVehicle();

                if(playerVehicle)
                {
                    if(playerVehicle->IsNetworkClone())
                    {
                        CNetObjVehicle      *netObjVehicle = static_cast<CNetObjVehicle *>(playerVehicle->GetNetworkObject());
                        CNetBlenderPhysical *netBlender    = static_cast<CNetBlenderPhysical *>(netObjVehicle->GetNetBlender());

                        if(netBlender)
                        {
                            position = netBlender->GetLastPositionReceived();
                        }
                    }
                    else
                    {
                        position = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
                    }
                }
            }
        }
    }
}

// Name         :   IsSpectating
// Purpose      :   Returns true if the player is visible on screen.
//                  ** This cannot be called once, because the visibility test is only done periodically, so it is presumed that this function
//                  will be called every frame for a period of time **
bool CNetObjPlayer::IsVisibleOnscreen()
{
    if (IsClone())
    {
        m_VisibilityQueryTimer = PLAYER_VISIBILITY_TEST_FREQUENCY;
        return m_visibleOnscreen;
    }

    return true;
}

static void FlagForRemovalPostTutorialChange(netObject *networkObject, void *UNUSED_PARAM(param))
{
    if(networkObject && IsAPendingTutorialSessionReassignType(networkObject->GetObjectType()))
    {
		if(!networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
		{
			// never remove the vehicle the local player is in
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

			if (pLocalPlayer && pLocalPlayer->GetIsInVehicle() && pLocalPlayer->GetMyVehicle())
			{
				if (networkObject == pLocalPlayer->GetMyVehicle()->GetNetworkObject())
				{
					return;
				}
			}

            NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FLAG_POST_TUTORIAL_REMOVAL", networkObject->GetLogName());
            networkObject->SetLocalFlag(CNetObjGame::LOCALFLAG_REMOVE_POST_TUTORIAL_CHANGE, true);
        }
    }
}

static void CheckIfFlaggedTutorialObjectsStillRemain(netObject *networkObject, void *param)
{
	if(gnetVerifyf(param,"NULL parameter passed to CheckIfFlaggedTutorialObjectsStillRemain!"))
	{
		bool* pbFlaggedTutorialObjectsRemain = ((bool*)param);

		if( !*pbFlaggedTutorialObjectsRemain && 
			networkObject && 
			IsAPendingTutorialSessionReassignType(networkObject->GetObjectType()) && 
			networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_REMOVE_POST_TUTORIAL_CHANGE))
		{
			if(!networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
			{
				*pbFlaggedTutorialObjectsRemain = true;
			}
		}
	}
}

void CNetObjPlayer::SetUpPendingTutorialChange()
{
	m_bPendingTutorialSessionChange = ( m_tutorialIndex != m_pendingTutorialIndex) || ( m_tutorialIndex!= SOLO_TUTORIAL_INDEX && m_tutorialInstanceID != m_pendingTutorialInstanceID );

	if(	!m_bPendingTutorialSessionChange )
	{
		return;
	}

    NetworkInterface::GetObjectManager().ForAllObjects(FlagForRemovalPostTutorialChange, 0);

	m_pendingTutorialSessionChangeTimer = TIME_PENDING_TUTORIAL_CHANGE;
	m_pendingTutorialPopulationClearedTimer = TIME_PENDING_TUTORIAL_CLONE_POPULATION_REMOVED;
	m_bWaitingPopulationClearedInTutorialSession = true;
#if !__FINAL
	Displayf("%s SetUpPendingTutorialChange m_pendingTutorialIndex %d m_pendingTutorialInstanceID %d. Frame: %d\n", GetLogName(), m_pendingTutorialIndex, m_pendingTutorialInstanceID,fwTimer::GetFrameCount() );
#endif

	CPedPopulation::StopSpawningPeds();
	CVehiclePopulation::StopSpawningVehs();
}

void CNetObjPlayer::StartSoloTutorial()
{
    NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "STARTING_SOLO_TUTORIAL", GetLogName());

	gnetAssertf(IsTutorialSessionChangePending()==false,"Don't expect StartSoloTutorial while tutorial session change pending. m_pendingTutorialIndex %u, m_pendingTutorialInstanceID %u", m_pendingTutorialIndex, m_pendingTutorialInstanceID);
    m_pendingTutorialIndex = SOLO_TUTORIAL_INDEX;
	SetUpPendingTutorialChange(); //Wait for timer or reassigning all local objects
}

void CNetObjPlayer::StartGangTutorial(int team, int instanceID)
{
	gnetAssertf(IsTutorialSessionChangePending() == false, "Don't expect StartGangTutorial while tutorial session change pending. m_pendingTutorialIndex %u, m_pendingTutorialInstanceID %u", m_pendingTutorialIndex, m_pendingTutorialInstanceID);

	m_pendingTutorialIndex = static_cast<u8>(team);
	m_pendingTutorialInstanceID = static_cast<u8>(instanceID);

    NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "STARTING_GANG_TUTORIAL", GetLogName());
    NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Team",        "%d", m_pendingTutorialIndex);
    NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Instance ID", "%d", m_pendingTutorialInstanceID);

	SetUpPendingTutorialChange(); //Wait for timer or reassigning all local objects
}

void CNetObjPlayer::EndTutorial()
{
    NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "ENDING_TUTORIAL", GetLogName());

	gnetAssertf(IsTutorialSessionChangePending()==false,"Don't expect EndTutorial while tutorial session change pending. m_pendingTutorialIndex %u, m_pendingTutorialInstanceID %u", m_pendingTutorialIndex, m_pendingTutorialInstanceID);
    m_pendingTutorialIndex      = INVALID_TUTORIAL_INDEX;
    m_pendingTutorialInstanceID = INVALID_TUTORIAL_INSTANCE_ID;
	SetUpPendingTutorialChange(); //Wait for timer or reassigning all local objects
}

void CNetObjPlayer::ChangeToPendingTutorialSession()
{
    // build a list of players currently in a different tutorial session
    unsigned        numPlayersInDifferentSessions = 0;
    CNetGamePlayer *playersInDifferentSessions[MAX_NUM_PHYSICAL_PLAYERS];

    unsigned           numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        CNetGamePlayer *remotePlayer = SafeCast(CNetGamePlayer, remotePhysicalPlayers[index]);

        if(remotePlayer && remotePlayer->IsInDifferentTutorialSession() && gnetVerifyf(numPlayersInDifferentSessions < MAX_NUM_PHYSICAL_PLAYERS, "Too many players in different tutorial session!"))
        {
            playersInDifferentSessions[numPlayersInDifferentSessions] = remotePlayer;
            numPlayersInDifferentSessions++;
        }
    }

	m_tutorialIndex      = m_pendingTutorialIndex;
	m_tutorialInstanceID = m_pendingTutorialInstanceID;

	m_pendingTutorialSessionChangeTimer = 0;
	CNetwork::ClearPopulation(false);
	CPedPopulation::StartSpawningPeds();
	CVehiclePopulation::StartSpawningVehs();

	//Start cool off time out so we dont start generating scenario peds before we get up to speed about clones
	m_postTutorialSessionChangeHoldOffPedGenTimer = POST_TUTORIAL_CHANGE_HOLD_OFF_PED_GEN;

#if !__FINAL
	Displayf("%s ChangeToPendingTutorialSession m_pendingTutorialIndex %d m_pendingTutorialInstanceID %d. Frame: %d\n", GetLogName(), m_pendingTutorialIndex, m_pendingTutorialInstanceID,fwTimer::GetFrameCount() );
#endif

    UpdateCachedTutorialSessionState();

    // ensure any players that are now in our tutorial session are unhidden
    for(unsigned index = 0; index < numPlayersInDifferentSessions; index++)
    {
        CNetGamePlayer *remotePlayer = playersInDifferentSessions[index];

        if(remotePlayer && !remotePlayer->IsInDifferentTutorialSession())
        {
            CPed          *playerPed    = remotePlayer->GetPlayerPed();
            CNetObjPlayer *netObjPlayer = playerPed ? static_cast<CNetObjPlayer *>(playerPed->GetNetworkObject()) : 0;

            if(netObjPlayer)
            {
                if(!netObjPlayer->IsUsingRagdoll() && netObjPlayer->GetNetBlender())
                {
                    netObjPlayer->GetNetBlender()->GoStraightToTarget();
                }

				if (!playerPed->GetIsInVehicle())
				{
					playerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);                
					playerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);                 				
					playerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
				}   

				playerPed->ClearWetClothing();
            }
        }
    }
}

void CNetObjPlayer::UpdatePendingTutorialSession()
{
	bool bPendingTutorialSessionChange = ( m_tutorialIndex != m_pendingTutorialIndex) || ( m_tutorialIndex!= SOLO_TUTORIAL_INDEX && m_tutorialInstanceID != m_pendingTutorialInstanceID );

	if (m_postTutorialSessionChangeHoldOffPedGenTimer > 0 )
	{
		m_postTutorialSessionChangeHoldOffPedGenTimer -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());
	}


	if (m_pendingTutorialSessionChangeTimer > 0 &&
		NetworkInterface::GetObjectManager().AnyLocalObjectPendingTutorialSessionReassign())
	{
		m_pendingTutorialSessionChangeTimer -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());
	}
	else if(bPendingTutorialSessionChange)
	{
		ChangeToPendingTutorialSession();
		bPendingTutorialSessionChange = false;
	}

	if ( !m_bPendingTutorialSessionChange && m_pendingTutorialPopulationClearedTimer > 0 )
	{
		m_pendingTutorialPopulationClearedTimer -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());

		bool bFlaggedTutorialObjectsRemain = false;
		NetworkInterface::GetObjectManager().ForAllObjects(CheckIfFlaggedTutorialObjectsStillRemain, (void*)&bFlaggedTutorialObjectsRemain);

		if(!bFlaggedTutorialObjectsRemain || m_pendingTutorialPopulationClearedTimer<=0 )
		{
#if !__FINAL
			gnetDebug2("\n All flagged tutorial population is cleared %s, or timer has timed out %d  FC: %d\n\n",bFlaggedTutorialObjectsRemain?"NO":"YES",m_pendingTutorialPopulationClearedTimer, fwTimer::GetFrameCount());
#endif

			//all population is cleared or timer has timed out
			m_pendingTutorialPopulationClearedTimer =0;
			m_bWaitingPopulationClearedInTutorialSession = false;
		}
	}

	m_bPendingTutorialSessionChange = bPendingTutorialSessionChange;
}

void CNetObjPlayer::UpdateVehicleWantedDirtyState()
{
	CPed* pPlayerPed = GetPlayerPed();
	if (pPlayerPed)
	{
		if (pPlayerPed->GetVehiclePedInside())
		{
			if (!m_bInsideVehicle)
			{
				m_bInsideVehicle = true;

				//Force a resend of this players wanted level when they enter a vehicle if they have a wanted level
				if (pPlayerPed->GetPlayerWanted() && (pPlayerPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN))
				{
					// Reset the wanted level so that it invokes the wanted level on all players in the vehicle when we get in...
					// set the delay to 1 so it doesn't induce an UpdateLevel and refocusing within SetWantedLevel
					wantedDebugf3("CNetObjPlayer::UpdateVehicleWantedDirtyState re-invoke SetWantedLevel");
					switch (CCrime::sm_eReportCrimeMethod)
					{
					case CCrime::REPORT_CRIME_DEFAULT:
					{
						SetWantedLevelFrom eSetWantedLevelFrom = pPlayerPed->GetPlayerWanted()->GetWantedLevelLastSetFrom();
						pPlayerPed->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), pPlayerPed->GetPlayerWanted()->GetWantedLevel(), 1, eSetWantedLevelFrom, true);
						break;
					}
					case CCrime::REPORT_CRIME_ARCADE_CNC:
						// RUFFIAN RM: SetWantedLevelFrom is set to WL_FROM_NETWORK so that we know this call has been made as part of network object update
						// (i.e. not due to a crime having been committed, a native call via script etc.)
						pPlayerPed->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), pPlayerPed->GetPlayerWanted()->GetWantedLevel(), 1, WL_FROM_NETWORK, true);
						break;
					}
					
				}
			}

			return;
		}
	}

	m_bInsideVehicle = false;
}

void CNetObjPlayer::SanityCheckVisibility()
{
	CPed* pPlayer = GetPlayerPed();
	
	// ** HACK **
	// This is code to check the current visibility state of the clone player, compared to the last received visibility state over the network, and 
	// restore is after a certain amount of time if it differs. There are some nasty edge-case bugs where clone player can be left invisible and
	// it is too risky to change the visibility code at this stage.
	if (pPlayer && m_bLastVisibilityStateReceived != pPlayer->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT))
	{
		static const u8 RESTORE_VISIBILITY_TIME = 60; // 60 frames (~2 secs)

		bool bRestoreCachedVisibility = false;

		bool bVisible;
		if (!IsLocalEntityVisibilityOverriden(bVisible) && !IsAlphaFading() && !IsAlphaRamping())
		{
			bRestoreCachedVisibility = true;

			CVehicle* pPlayersVehicle = pPlayer->GetIsInVehicle() ? pPlayer->GetMyVehicle() : NULL;

			if (pPlayersVehicle && m_bLastVisibilityStateReceived && !pPlayersVehicle->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT))
			{
				// don't restore cached visibility if the player is in an invisible vehicle
				bRestoreCachedVisibility = false;
			}

			if (bRestoreCachedVisibility)
			{
				m_visibilityMismatchTimer++;

				if (m_visibilityMismatchTimer >= RESTORE_VISIBILITY_TIME)
				{
#if ENABLE_NETWORK_LOGGING
					NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "RESTORING_CACHED_VISIBILITY", GetLogName());
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Visible", "%s", m_bLastVisibilityStateReceived ? "True" : "False");
#endif // ENABLE_NETWORK_LOGGING

					SetIsVisibleForModuleSafe(pPlayer, SETISVISIBLE_MODULE_SCRIPT, m_bLastVisibilityStateReceived);

					m_visibilityMismatchTimer = 0;
				}
			}
		}
		else
		{
			m_visibilityMismatchTimer = 0;
		}
	}
}

bool CNetObjPlayer::ShouldFixAndDisableCollisionDueToControls() const
{
    if(IsClone())
    {
        CPed *pPlayer = GetPlayerPed();

        if(pPlayer->GetPlayerInfo() && pPlayer->GetPlayerInfo()->IsControlsScriptDisabled() && !pPlayer->GetIsVisible() && !IsVisibleToScript())
        {
            return true;
        }
    }

    return false;
}

bool CNetObjPlayer::ShouldOverrideBlenderForSecondaryAnim() const
{
    if(gnetVerifyf(IsClone(), "Calling ShouldOverrideBlenderForSecondaryAnim() on the local player!"))
    {
        if( mSecondaryAnimData.m_SecondaryPartialAnimDictHash!=0 &&
            mSecondaryAnimData.SecondaryPartialAnimClipHash()!=0 )
        {
            if( mSecondaryAnimData.SecondaryBoneMaskHash() == 0 || 
                mSecondaryAnimData.SecondaryBoneMaskHash() == BONEMASK_ALL)
            {
                CPed* pPlayerPed = GetPlayerPed();

                if(pPlayerPed->GetIsVisible() || !pPlayerPed->GetIsAnyFixedFlagSet())
                {
                    Vector3 vCloneExpectedPosition		= NetworkInterface::GetLastPosReceivedOverNetwork(pPlayerPed);
                    Vector3 vCloneActualPosition		= VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
                    float fCloneDistanceFromExpectedSq	= (vCloneExpectedPosition - vCloneActualPosition).Mag2();
                    const float MAX_PLAYER_CLONE_DISTANCE_FROM_EXPECTED_PLACEMENT_POSITION_WHILE_FULL_BODY_SECONDARY = 10.0f;

                    if(fCloneDistanceFromExpectedSq < rage::square(MAX_PLAYER_CLONE_DISTANCE_FROM_EXPECTED_PLACEMENT_POSITION_WHILE_FULL_BODY_SECONDARY) )
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void CNetObjPlayer::UpdateInitialPedCrewColor()
{
	if (!m_bCrewPedColorSet)
	{
		const CNetGamePlayer* pPlayer = GetPlayerOwner();
		if (gnetVerify(pPlayer))
		{
			const rlClanDesc& curClanDesc = pPlayer->GetClanDesc();
			Color32 clanColor(curClanDesc.m_clanColor);
			CPed* ped = GetPlayerPed();
			if (ped)
			{
				for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
				{
					m_bCrewPedColorSet |= ped->SetHeadBlendPaletteColor(clanColor.GetRed(),clanColor.GetGreen(),clanColor.GetBlue(), i);
				}
			}
		}
	}
}

void CNetObjPlayer::UpdateLocalWantedSystemFromRemoteVisibility()
{
	if (m_bWantedVisibleFromPlayer)
	{
		u32 fc = fwTimer::GetFrameCount();
		if (fc % 30 == 0)
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			CWanted* pLocalWanted = pLocalPlayer ? pLocalPlayer->GetPlayerWanted() : NULL;
			if (pLocalWanted && (pLocalWanted->GetWantedLevel() > WANTED_CLEAN))
			{
				wantedDebugf3("CNetObjPlayer::UpdateLocalWantedSystemFromRemoteVisibility LOCALPLAYER ReportPoliceSpottingPlayer fc[%u]",fc); 
				pLocalWanted->ReportPoliceSpottingPlayer(NULL,VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()),pLocalWanted->GetWantedLevel(),true,true);
			}
			else
			{
				wantedDebugf3("CNetObjPlayer::UpdateLocalWantedSystemFromRemoteVisibility (!pLocalWanted || (pLocalWanted->GetWantedLevel() <= WANTED_CLEAN)) -- reset m_bWantedVisibleFromPlayer -- fc[%u]",fc); 
				m_bWantedVisibleFromPlayer = false; //reset if the wanted level fell
			}
		}
	}
}

// name:        Update
// description: Called once a frame to do special network object related stuff
// Returns      :   True if the object wants to unregister itself
bool CNetObjPlayer::Update()
{
    CPed* pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

    // sanity check the ped control code knows this is a player
    gnetAssert(!pPlayer || pPlayer->IsPlayer());

	// sanity check the ped flag - CPED_CONFIG_FLAG_BlockNonTemporaryEvents
	gnetAssert(!pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockNonTemporaryEvents));

	if(IsClone())
    {
		if(m_ConcealedOnOwner)
		{
			HideUnderMap();
		}

		// Update player position when its stood on object is later streamed in
		UpdateCloneStandingOnObjectStreamIn();

		// Update the pending camera data (deals with frame culling inside (i.e. only updating every X'th frame)...
		UpdatePendingCameraData();

		// Update players weapon target...
		UpdateWeaponTarget();

        if(m_UseKinematicPhysics)
        {
            pPlayer->SetShouldUseKinematicPhysics(true);
        }

        grcViewport *pViewport = GetViewport();

        if(pViewport && !m_UsingFreeCam)
        {
            Mat34V camM = pViewport->GetCameraMtx();
            camM.SetCol3(pPlayer->GetTransform().GetPosition() + m_viewportOffset);

            pViewport->SetCameraMtx(camM);
        }

        DoOnscreenVisibilityTest();
       
        // count timer up until it is set to 0 from a network update
        if (m_respawnInvincibilityTimer != 0)
        {
			if (!m_bCloneRespawnInvincibilityTimerCountdown)
			{
				//countup
				m_respawnInvincibilityTimer += static_cast<s16>(fwTimer::GetSystemTimeStepInMilliseconds());
			}
			else
			{
				//countdown - happens on respawn to ensure that the respawn invisibility isn't cleared inappropriately.
				m_respawnInvincibilityTimer -= static_cast<s16>(fwTimer::GetSystemTimeStepInMilliseconds());
				m_respawnInvincibilityTimer = MAX(m_respawnInvincibilityTimer, 0);

				//ensure that the countdown flag is cleared on hitting zero
				if (m_respawnInvincibilityTimer == 0)
					m_bCloneRespawnInvincibilityTimerCountdown = false;
			}
        }

		UpdateRemotePlayerFade();
		UpdateSecondaryPartialAnimTask();

		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsSpectatingPed(pPlayer) && !CTheScripts::GetNumberOfMiniGamesInProgress())
		{
			CWanted* wanted = pPlayer->GetPlayerWanted();
			if (wanted)
			{
				wanted->GetWantedBlips().EnablePoliceRadarBlips(true);
				wanted->Update( VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), pPlayer->GetVehiclePedInside(), false );
			}
		}

        // if blending has been switched off by script ensure the position and orientation
        // of the object don't drift away from the last received values. This should really
        // be done in the network blender itself but I don't want to do a refactor of this
        // functionality at this point
        if(IsLocalFlagSet(CNetObjGame::LOCALFLAG_DISABLE_BLENDING) && !pPlayer->GetUsingRagdoll() && !NetworkBlenderIsOverridden())
        {
            CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

            if(netBlenderPed)
            {
                if(!NetworkHeadingBlenderIsOverridden())
                {
                    float currentHeading = pPlayer->GetCurrentHeading();
                    float targetHeading  = netBlenderPed->GetLastHeadingReceived();

                    static const float MAX_HEADING_DIFF = 0.0872f; // 5 degrees
                    if(fabs(currentHeading - targetHeading) > MAX_HEADING_DIFF)
                    {
                        pPlayer->SetHeading(targetHeading);
                    }
                }

                Vector3 currentPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
                Vector3 targetPos  = netBlenderPed->GetLastPositionReceived();

                static const float MAX_POSITION_DIFF_SQR = rage::square(0.1f);

                if((currentPos - targetPos).Mag2() >= MAX_POSITION_DIFF_SQR)
                {
                    pPlayer->Teleport(targetPos, pPlayer->GetCurrentHeading());
                }
            }
        }

		if (m_bRemoteReapplyColors)
		{
			m_bRemoteReapplyColors = false; //reset

			for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
			{
				m_bRemoteReapplyColors |= !pPlayer->SetHeadBlendPaletteColor( m_remoteColor[i].GetRed(), m_remoteColor[i].GetGreen(), m_remoteColor[i].GetBlue(), i, true);
			}
		}

		SanityCheckVisibility();

		UpdateLocalWantedSystemFromRemoteVisibility();

		UpdateRemotePlayerAppearance();

        if(ShouldOverrideBlenderForSecondaryAnim() && !pPlayer->GetUsingRagdoll() && !pPlayer->GetIsAttached())
        {
            // we need to blend the heading here - as we are overriding the network blender
            // but only want to override the position of the ped
            NetworkInterface::UpdateHeadingBlending(*pPlayer);
        }

		pPlayer->SetPedResetFlag(CPED_RESET_FLAG_BlockRemotePlayerRecording, m_PRF_BlockRemotePlayerRecording);
		pPlayer->SetPedResetFlag(CPED_RESET_FLAG_UseScriptedWeaponFirePosition, m_PRF_UseScriptedWeaponFirePosition);
        
        if(m_PRF_IsSuperJumpEnabled)
        {
            pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_SUPER_JUMP_ON);
        }
		if(m_PRF_IsShockedFromDOTEffect)
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DOT_SHOCKED);
		}
		if(m_PRF_IsChokingFromDOTEffect)
		{
			pPlayer->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DOT_CHOKING);
		}

		if(!pPlayer->GetIsInVehicle())
		{
			ObjectId objStandingOn;
			Vector3 offset;
			CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, pPlayer->GetNetworkObject()->GetNetBlender());
			if(netBlenderPed->IsStandingOnObject(objStandingOn, offset))
			{
				netObject* obj = NetworkInterface::GetNetworkObject(objStandingOn);
				if(!obj)
				{
					SetOverridingLocalVisibility(false, "Vehicle is not cloned");
				}
			}
		}

        // override the collision while we are fixed by network
        if(!pPlayer->GetIsAttached() && !pPlayer->GetUsingRagdoll() && pPlayer->GetIsAnyFixedFlagSet() && CanBlendWhenFixed())
        {
			if (!pPlayer->GetPedResetFlag(CPED_RESET_FLAG_EnableCollisionOnNetworkCloneWhenFixed))
			{
				SetOverridingLocalCollision(false, "Doing fixed physics network blending");
			}
        }

		if(m_bHasActiveWaypoint && !CMiniMap::IsActiveWaypoint(Vector2(m_fxWaypoint, m_fyWaypoint), m_WaypointObjectId))
		{
			CPed* pLocalPed = CGameWorld::FindLocalPlayer();
			if(pLocalPed && pLocalPed->GetNetworkObject())
			{
				if(SafeCast(CNetObjPlayer, pLocalPed->GetNetworkObject())->GetWaypointOverrideOwner())
				{
					ObjectId playerId = GetObjectID();
					if(playerId != NETWORK_INVALID_OBJECT_ID && SafeCast(CNetObjPlayer, pLocalPed->GetNetworkObject())->GetWaypointOverrideOwner() == playerId && SafeCast(CNetObjPlayer, pLocalPed->GetNetworkObject())->OwnsWaypoint())
					{
						CMiniMap::SwitchOffWaypoint(false);
						CMiniMap::SwitchOnWaypoint(m_fxWaypoint, m_fyWaypoint, m_WaypointObjectId, false, false, NetworkInterface::GetWaypointOverrideColor());
					}
				}
			}
		}

		if(m_HasToApplyWeaponToVehicle)
		{
			const CVehicle* pVehicle = pPlayer->GetVehiclePedInside();
			if(pVehicle)
			{
				m_HasToApplyWeaponToVehicle = false;
				if(!m_weaponObjectExists || m_inVehicle)
				{
					// either the vehicle weapon or the normal weapon must be set. lastsyncedweaponhash should always have a value
					if(m_pendingVehicleWeaponIndex != -1 || m_lastSyncedWeaponHash != 0)
					{
						bool saveCanAlter = m_CanAlterPedInventoryData;
						m_CanAlterPedInventoryData = true;
						bool hasApplied = pPlayer->GetWeaponManager()->EquipWeapon(m_pendingVehicleWeaponIndex == -1 ? m_lastSyncedWeaponHash : 0, m_pendingVehicleWeaponIndex);
						m_HasToApplyWeaponToVehicle = !hasApplied;
						m_CanAlterPedInventoryData = saveCanAlter;
					}
				}
			}
		}
	}
    else
    {
		if(m_respawnInvincibilityTimer > 0)
        {
            m_respawnInvincibilityTimer -= static_cast<s16>(fwTimer::GetSystemTimeStepInMilliseconds());
            m_respawnInvincibilityTimer = MAX(m_respawnInvincibilityTimer, 0);

			//As soon as the player fires he becomes vulnerable
			if (pPlayer && pPlayer->GetTimeSinceLastShotFired() == 0)
			{
				respawnDebugf3("(pPlayer && pPlayer->GetTimeSinceLastShotFired() == 0) reset m_respawnInvincibilityTimer");
				m_respawnInvincibilityTimer = 0;
			}

            if(m_respawnInvincibilityTimer == 0)
            {
                NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), GetLogName(), "%s\r\n", "RESPAWN_TIMER_ELAPSED");
            }
        }

		UpdateVehicleWantedDirtyState();

        UpdateNonPhysicalPlayerData();
        UpdateAnimVehicleStreamingTarget();

		if (ms_pLocalPlayerWeaponPickup || ms_pLocalPlayerWeaponObject)
		{
			if (pPlayer->IsDeadByMelee() || pPlayer->GetIsDeadOrDying())
			{
				if (ms_pLocalPlayerWeaponPickup)
				{
					NetworkInterface::LocallyHideEntityForAFrame(*ms_pLocalPlayerWeaponPickup, false, "Hiding pickup from dead local player");
				}
			}
			else
			{
				ms_pLocalPlayerWeaponPickup = NULL;

				if (ms_pLocalPlayerWeaponObject)
				{
					ms_pLocalPlayerWeaponObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
					ms_pLocalPlayerWeaponObject = NULL;
				}
			}
		}
    }

	bool bProcessRamping = false;
	if (m_respawnInvincibilityTimer > 0)
	{
        if(IsClone() && !m_DisableRespawnFlashing)
        {
		    bProcessRamping = true;

		    if (pPlayer->GetIsInVehicle() && !pPlayer->GetIsDrivingVehicle() && pPlayer->GetMyVehicle()->ContainsLocalPlayer())
			    bProcessRamping = false;
        }
	}

    // flash the player's remote health bar if he is invincible
    if (bProcessRamping)
    {
		bool bJustJoined = false;
		const netPlayer *remotePlayer = pPlayer->GetNetworkObject()->GetPlayerOwner();
		if (remotePlayer && remotePlayer->GetTimeInSession() < 10000)
			bJustJoined = true;

		if (!IsAlphaRamping())
		{
			if (bJustJoined)
			{
				SetAlphaRampFadeInAndQuit(false);
			}
			else
			{
				SetAlphaRampingContinuous(false);
			}
		}

		if (pPlayer->GetIsDrivingVehicle() && pPlayer->IsLocalPlayer())
		{
			pPlayer->GetMyVehicle()->SetRespotCounter(m_respawnInvincibilityTimer);
			m_bRespawnInvincibilityTimerRespot = true;
		}

		m_bRespawnInvincibilityTimerWasSet = true;
    }
    else
    {
        SetIsVisibleForModuleSafe(pPlayer, SETISVISIBLE_MODULE_PLAYER, true);
		SetIsVisibleForModuleSafe(pPlayer, SETISVISIBLE_MODULE_RESPAWN,true,true);

		if (m_bRespawnInvincibilityTimerWasSet && IsAlphaRamping())
		{
			if (m_bRespawnInvincibilityTimerRespot)
			{
				StopAlphaRamping();

				if (pPlayer->GetIsDrivingVehicle() && pPlayer->IsLocalPlayer() && pPlayer->GetMyVehicle()->IsBeingRespotted())
					pPlayer->GetMyVehicle()->SetRespotCounter(0);

				m_bRespawnInvincibilityTimerRespot = false;
			}
			else 
			{
				//Allow vehicle respotting to control alpha if we are in a vehicle that is being respotted, and the respotting wasn't initiated through invincibility above.
				if (!pPlayer->GetIsInVehicle() || !pPlayer->GetMyVehicle()->IsBeingRespotted())
					StopAlphaRamping();
			}
		}

		m_bRespawnInvincibilityTimerWasSet = false;
	}

    bool success = CNetObjPed::Update();

    // this needs to be done after the ped update as it can change the uses collision flag incorrectly
    if(ShouldFixAndDisableCollisionDueToControls())
    {
        if(!ShouldObjectBeHiddenForTutorial())
        {
            SetOverridingLocalCollision(false, "Player controls disabled");
        }
    }

	UpdateLookAt();

	UpdateStats();

    return success;
}

void CNetObjPlayer::StartSynchronising()
{
	CPed* pPlayer = GetPlayerPed();
	
	CNetObjPed::StartSynchronising();

	// if the player is already flagged to be invisible (i.e. if he is a spectator), we need to forcibly dirty the entity script game state node here
	// so that an update is sent out with the visibility state
	if (pPlayer && GetSyncData() && (!pPlayer->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT) || !pPlayer->IsCollisionEnabled()))
	{
		gnetWarning("CNetObjPlayer::StartSynchronising() :: Player is invisible/uncollideable when starting to sync");

		CPlayerSyncTree* pSyncTree = static_cast<CPlayerSyncTree*>(GetSyncTree());
		pSyncTree->DirtyNode(this, *pSyncTree->GetScriptGameStateNode());		// for the collision state
		pSyncTree->DirtyNode(this, *pSyncTree->GetPhysicalGameStateNode());		// for visibility state
		pSyncTree->DirtyNode(this, *pSyncTree->GetPlayerGameStateDataNode());	// for the m_bCollisionsDisabledByScript flag
	}
}


bool CNetObjPlayer::CanClone(const netPlayer& player, unsigned *resultCode) const
{
    // don't create the player ped on other machines while it is using the normal player model,
    // the network game is not set up to support this and it will cause crashes

    /*--------------------------------------------------------------*/
    // CPlayerCreationDataNode::GetCanApplyData() also checks to see if the
    //   single player model is being used in the network game.
    /*--------------------------------------------------------------*/
	
    if (HasSinglePlayerModel())
    {
        if(resultCode)
        {
            *resultCode = CC_HAS_SP_MODEL;
        }

        return false;
    }
	
    return CNetObjPed::CanClone(player, resultCode);
}

bool CNetObjPlayer::CanSync(const netPlayer& player) const
{
    gnetAssertf(player.GetPhysicalPlayerIndex() < MAX_NUM_PHYSICAL_PLAYERS, "Invalid physical player index!");

    // don't send sync updates while we have an unacked respawn event
    CRespawnPlayerPedEvent *networkEvent = static_cast<CRespawnPlayerPedEvent *>(NetworkInterface::GetEventManager().GetExistingEventFromEventList(RESPAWN_PLAYER_PED_EVENT));

    if(networkEvent && networkEvent->GetEventId().MustSendToPlayer(player.GetPhysicalPlayerIndex()))
    {
        return false;
    }

    return true;
}


bool CNetObjPlayer::CanSynchronise(bool bOnRegistration) const
{
    if (HasSinglePlayerModel())
    {
        return false;
    }
	
    return CNetObjPed::CanSynchronise(bOnRegistration);
}

bool CNetObjPlayer::ProcessControl()
{
    CPed* pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

	if (IsClone())
	{
		if (m_bAbortNMShotReaction)
		{
			// the player is doing a local shot reaction and is not going to die, remove the shot reaction
			CTaskDyingDead* pDyingTask = SafeCast(CTaskDyingDead, pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));

			if (pDyingTask && pDyingTask->IsRunningLocally())
			{
				pPlayer->GetPedIntelligence()->FlushImmediately(false, false, true, true);
				pPlayer->GetPedIntelligence()->RecalculateCloneTasks();
			}
		}
		else if (pPlayer->GetPedIntelligence()->NetworkLocalHitReactionsAllowed() && m_pDeathResponseTask && m_addToPendingDamageCalled)
		{
			// the player is going to die, apply the death response task
			pPlayer->GetPedIntelligence()->AddLocalCloneTask(static_cast<CTaskFSMClone*>(m_pDeathResponseTask->Copy()), PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);

			m_bRunningDeathResponseTask = true;
		}
	
		if (m_pDeathResponseTask)
		{
			delete m_pDeathResponseTask;
			m_pDeathResponseTask = NULL;
		}

		m_addToPendingDamageCalled = false;

		if (m_bRunningDeathResponseTask)
		{
			CTask* pTask = pPlayer->GetPedIntelligence()->GetActiveCloneTask();

			if (!pTask || pTask->GetTaskType() != CTaskTypes::TASK_DYING_DEAD)
			{
				m_bRunningDeathResponseTask = false;
			}
		}

		UpdateLockOnTarget();
	}

    CNetObjPed::ProcessControl();

    if(pPlayer && pPlayer->GetPlayerInfo() && IsClone())
    {
        pPlayer->GetPlayerInfo()->DecayStealthNoise(fwTimer::GetTimeStep());
		pPlayer->GetPlayerInfo()->ProcessPlayerVehicleSpeedTimer();
		pPlayer->GetPlayerInfo()->GetPlayerResetFlags().UnsetFlag(CPlayerResetFlags::PRF_FIRE_AMMO_ON);
    }

	m_bAbortNMShotReaction = false;

    return true;
}

//
// name:        CleanUpGameObject
// description: Cleans up the pointers between this network object and the
//              game object which owns it
void CNetObjPlayer::CleanUpGameObject(bool bDestroyObject)
{
    CPed* pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

    // delete the player
    if (bDestroyObject)
    {
		// need to restore the game heap here in case the below functions deallocate memory, as this
        // function is called while the network heap is active
        sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
        sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

        CProjectileManager::DestroyAllStickyProjectilesForPed(GetPlayerPed());

        // if the player has any peds in his group, remove them
        CPedGroups::ms_groups[CPedGroups::GetPlayersGroup(pPlayer)].GetGroupMembership()->Flush(false);
	
        CPlayerInfo *pPlayerInfo = pPlayer->GetPlayerInfo();

        if(AssertVerify(pPlayerInfo))
        {
            // the network player representing this player ped needs to keep the player info
            CNetGamePlayer *playerOwner = GetPlayerOwner();

            if (playerOwner && playerOwner->IsValid())
            {
                gnetAssert(playerOwner->GetPlayerInfo() == pPlayerInfo);
 
                // update the player manager that player has been removed
                NetworkInterface::GetPlayerMgr().UpdatePlayerPhysicalStatus(playerOwner);
            }

            pPlayerInfo->SetPlayerState(CPlayerInfo::PLAYERSTATE_LEFTGAME);
         }

        sysMemAllocator::SetCurrent(*oldHeap);

		if (HasCachedPlayerHeadBlendData(GetPhysicalPlayerIndex()))
		{
			ClearCachedPlayerHeadBlendData(GetPhysicalPlayerIndex());
		}
    }

    CNetObjPed::CleanUpGameObject(bDestroyObject);
}

// name:        CanPassControl
// description: returns true if local control of this object can be passed to the given
//              remote player
// Parameters   :   pPlayer - the player we are trying to pass control to
//                  bProximityMigrate - if true this object is trying to migrate by proximity
//                  bFromScript - this is being called from a GiveControlObject event triggered by a script
bool CNetObjPlayer::CanPassControl(const netPlayer& UNUSED_PARAM(player), eMigrationType migrationType, unsigned *resultCode) const
{
    if (migrationType == MIGRATE_SCRIPT)
    {
        gnetAssertf(0, "Cannot pass control of %s - players can never migrate", GetLogName());
    }

    if(resultCode)
    {
        *resultCode = CPC_FAIL_IS_PLAYER;
    }
    // players can never migrate
    return false;
}

bool CNetObjPlayer::NetworkBlenderIsOverridden(unsigned *resultCode) const
{
    bool overridden = CNetObjPed::NetworkBlenderIsOverridden(resultCode);

    if(GetPlayerOwner() && GetPlayerOwner()->IsInDifferentTutorialSession())
    {
        if(resultCode)
        {
            *resultCode = NBO_DIFFERENT_TUTORIAL_SESSION;
        }

        overridden = true;
    }

    if(ShouldOverrideBlenderForSecondaryAnim())
    {
        if(resultCode)
        {
            *resultCode = NBO_FULL_BODY_SECONDARY_ANIM;
        }

        overridden = true;
    }

    return overridden;
}

bool CNetObjPlayer::CanBlendWhenFixed() const
{
    if(IsClone())
    {
        return true;
    }

    return CNetObjPed::CanBlendWhenFixed();
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   playerIndex          - playerIndex of new owner
//                  bProximityMigrate - ownership is being changed during proximity migration
// Returns      :   void
void CNetObjPlayer::ChangeOwner(const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
    // players can never migrate
    gnetAssertf(0, "Trying to change owner of %s!", GetLogName());
}

//
// name:        IsInScope
// description: This is used by the object manager to determine whether we need to create a
//              clone to represent this object on a remote machine. The decision is made using
//              the player that is passed into the method - this decision is usually based on
//              the distance between this player's players and the network object, but other criterion can
//              be used.
// Parameters:  pPlayer - the player that scope is being tested against
bool CNetObjPlayer::IsInScope(const netPlayer& playerToCheck, unsigned *scopeReason) const
{
    gnetAssert(dynamic_cast<const CNetGamePlayer *>(&playerToCheck));
    const CNetGamePlayer &player = static_cast<const CNetGamePlayer &>(playerToCheck);

	if (scopeReason)
	{
		*scopeReason = SCOPE_PLAYER_BUBBLE;
	}

	return GetPlayerOwner() && GetPlayerOwner()->IsInSameRoamingBubble(static_cast<const CNetGamePlayer&>(player));
}


// name:        ManageUpdateLevel
// description: Decides what the default update level should be for this object
void CNetObjPlayer::ManageUpdateLevel(const netPlayer& player)
{
#if __BANK
    if(CNetObjPlayer::ms_updateRateOverride >= 0)
    {
        SetUpdateLevel( player.GetPhysicalPlayerIndex(), static_cast<u8>(CNetObjPlayer::ms_updateRateOverride) );
        return;
    }
#endif

    CNetObjPhysical::ManageUpdateLevel(player);
}

bool CNetObjPlayer::IsScriptObject(bool bIncludePlayer) const
{
    if(!bIncludePlayer)
    {
        return false;
    }

    return CNetObjPed::IsScriptObject(bIncludePlayer);
}

//
// name:        CNetObjPlayer::TryToPassControlProximity
// description: Tries to pass control of a network object to another machine if a player from that machine is closer
void CNetObjPlayer::TryToPassControlProximity()
{
    // never pass control of players based on proximity
}

bool CNetObjPlayer::ShouldObjectBeHiddenForCutscene() const
{
	if (IsClone() && 
		!GetRemotelyVisibleInCutscene() &&
		GetPlayerOwner() && 
		GetPlayerOwner()->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
	{
		if(!IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE))
		{
			return true;
		}
		else
		{
			gnetDebug3("%s is in a cutscene and is marked LOCALFLAG_SHOWINCUTSCENE", GetLogName());
		}
	}

	return CNetObjPed::ShouldObjectBeHiddenForCutscene();
}

//
// name:        CNetObjPlayer::HideForCutscene
// description:
void CNetObjPlayer::HideForCutscene()
{
    CNetObjPed::HideForCutscene();

    // make our player invisible on remote machines during a cutscene
    if (!GetRemotelyVisibleInCutscene() && GetPlayerOwner() && GetPlayerOwner()->IsMyPlayer())
    {
	   SetOverridingRemoteVisibility(true, false, "Hide remotely during cutscene");
       SetOverridingRemoteCollision(true, false, "Hide remotely during cutscene");
    }
}

// name:        CNetObjPlayer::ExposeAfterCutscene
// description:
//
void CNetObjPlayer::ExposeAfterCutscene()
{
	if (IsConcealed())
	{
		return;
	}

	bool bMyPlayer = GetPlayerOwner() && GetPlayerOwner()->IsMyPlayer();

	if (!GetRemotelyVisibleInCutscene() && bMyPlayer)
    {
        SetOverridingRemoteVisibility(false, true, "Expose local player after cutscene");
        SetOverridingRemoteCollision(false, true, "Expose local player after cutscene");
    }

    CNetObjPed::ExposeAfterCutscene();

	if (bMyPlayer)
	{
		// make sure the local player is left unfixed
		SetFixedByNetwork(false, FBN_EXPOSE_LOCAL_PLAYER_AFTER_CUTSCENE, NPFBN_NONE);
	}
}

// name:        ChangePlayerModel
// description: called on a remote playerto change their model
bool CNetObjPlayer::ChangePlayerModel(const u32 newModelIndex)
{
    gnetAssert(IsClone());

    CPed  *playerPed  = GetPlayerPed();
    CPlayerInfo *playerInfo = playerPed ? playerPed->GetPlayerInfo() : 0;

    gnetAssertf(playerPed,  "No player ped available when changing a peds model!");
    gnetAssertf(playerInfo, "No player info available when changing a peds model!");

    if(!playerInfo)
        return false;

    NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "CHANGING_PLAYER_MODEL", GetLogName());
    NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Player:", "%s", IsClone() ? "Clone" : "Local");

    // move the ped directly to it's blend target before removing the ped and losing the data
    if(GetNetBlender())
    {
        GetNetBlender()->GoStraightToTarget();
    }

    // cache the weapon slot data
    u32 chosenWeaponType         = 0;
	u8     weaponObjectTintIndex = 0;
    bool   weaponObjectExists    = false;
    bool   weaponObjectVisible   = false;
	bool   weaponObjectFlashLightOn = false;
	bool   weaponObjectHasAmmo = true;
	bool   weaponObjectAttachLeft = false;
    GetWeaponData(chosenWeaponType, weaponObjectExists, weaponObjectVisible, weaponObjectTintIndex, weaponObjectFlashLightOn, weaponObjectHasAmmo, weaponObjectAttachLeft);
    u32 equippedGadgets[MAX_SYNCED_GADGETS];
    u32 numGadgets = GetGadgetData(equippedGadgets);
	u8	weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS];
	u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS];
	u32 numWeaponComponents = GetWeaponComponentData(weaponComponents, weaponComponentsTint);

    // cache the AI data
	CPedAIDataNode pedAIDataNode;
    GetAIData(pedAIDataNode);

    // cache the peds inventory
	CPedInventoryDataNode pedInventoryDataNode;
    GetPedInventoryData(pedInventoryDataNode);

    // remove the old ped and replace it with one with the new model
    fwModelId newModelId((strLocalIndex(newModelIndex)));
    CGameWorld::ChangePedModel(*playerPed, newModelId);

    // restore the AI data
    SetAIData(pedAIDataNode);

    // restore the weapon slots for the ped
	SetWeaponComponentData(chosenWeaponType, weaponComponents, weaponComponentsTint, numWeaponComponents);
    SetWeaponData(chosenWeaponType, weaponObjectExists, weaponObjectVisible, weaponObjectTintIndex, weaponObjectFlashLightOn, weaponObjectHasAmmo, weaponObjectAttachLeft);
	SetGadgetData(equippedGadgets, numGadgets);

    // restore the inventory for the ped
    SetPedInventoryData(pedInventoryDataNode);

    // re-create the blender for the new ped
    if (GetNetBlender())
    {
        DestroyNetBlender();
        CreateNetBlender();
    }

	// setup this ped's AI as a network clone
	SetupPedAIAsClone();

    return true;
}

// name:        ResurrectClone
// description: Called on the clone when the main player has resurrected
void CNetObjPlayer::ResurrectClone()
{
	CPed* pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);
    gnetAssert(IsClone());

	respawnDebugf3("CNetObjPlayer::ResurrectClone ped[%p][%s][%s]",pPlayerPed,pPlayerPed ? pPlayerPed->GetModelName() : "",GetLogName());

    m_lastResurrectionTime = CNetwork::GetSyncedTimeInMilliseconds();

	CNetObjPed::ResurrectClone( );

	weaponDebugf3("## RESURRECT PLAYER. Pending damage = 0.0f ##");

	// Ensure that the death info gets cleared when resurrecting a clone since it won't get synced again until
	// the player next dies
	pPlayerPed->ClearDeathInfo();

	// while the helmet / hat prop will get synced through the props, we need to call this to wipe the helmet related config flags
	if (pPlayerPed->GetHelmetComponent())
		pPlayerPed->GetHelmetComponent()->DisableHelmet();

	m_pendingDamage = 0;
	m_lastReceivedHealthTimestamp = 0;
	m_lastReceivedHealth = (u16)pPlayerPed->GetHealth();

	// ignore task specific data (ie dead dying task data) until we receive the next task tree update. This is because we can receive the resurrect
	// event just before an old sync, which causes a bunch of asserts ("Trying to set task specific data for an incorrect task", etc)
	m_bIgnoreTaskSpecificData = true;
}

void CNetObjPlayer::DisplayNetworkInfo()
{
    CPed *pPlayerPed = GetPlayerPed();
    CNetObjPed::DisplayNetworkInfo();

	if (!HasGameObject())
		return;

	// render player names
    if(strlen(m_DisplayName) && pPlayerPed && NetworkUtils::CanDisplayPlayerName(pPlayerPed))
    {
        float scale = 1.0f;
        Vector2 screenCoords;
        if(NetworkUtils::GetScreenCoordinatesForOHD(GetPosition(), screenCoords, scale))
        {
            NetworkColours::NetworkColour colour = NetworkColours::INVALID_COLOUR;

            if(!GetPlayerOwner()->HasTeam() || (!IsClone() && CScriptHud::bUsePlayerColourInsteadOfTeamColour))
            {
                colour = NetworkUtils::GetDebugColourForPlayer(GetPlayerOwner());
            }
            else
            {
                colour = NetworkColours::GetTeamColour(GetPlayerOwner()->GetTeam());
            }

            Color32 playerColour = (colour == NetworkColours::NETWORK_COLOUR_CUSTOM) ? NetworkColours::GetCustomTeamColour(GetPlayerOwner()->GetTeam()) : NetworkColours::GetNetworkColour(colour);

            //If it is a player and is inside a flying vehicle.
            CVehicle* vehicle = (pPlayerPed && pPlayerPed->GetIsInVehicle() ? pPlayerPed->GetMyVehicle() : NULL);
            if (vehicle && (vehicle->GetIsAircraft() || vehicle->GetIsRotaryAircraft()))
            {
                BANK_ONLY(screenCoords.y += (GetNetworkInfoStartYOffset() * (-0.151f * 1.0f * 0.5f)));  // TODO - Should GetNetworkInfoStartYOffset exist in all builds???
            }

            // if we are in the front or back RIGHT seat of a vehicle, then move the position of the player name info up by about 1 line
            if (vehicle)
            {
                CPed *pPedInFrontRtSeat = vehicle->GetSeatManager()->GetPedInSeat(1);
                CPed *pPedInRearRtSeat = vehicle->GetSeatManager()->GetPedInSeat(3);

                if (pPlayerPed == pPedInFrontRtSeat || pPlayerPed == pPedInRearRtSeat)
                {
    #define __AMOUNT_TO_MOVE_UP_BY_WHEN_DRIVER (0.025f)
                    screenCoords.y -= __AMOUNT_TO_MOVE_UP_BY_WHEN_DRIVER;
                }
            }

            DLC ( CDrawNetworkPlayerName_NY, (screenCoords, playerColour, scale, m_DisplayName));
        }
    }

#if __BANK
    grcViewport *pViewport = GetViewport();

    if (NetworkDebug::GetDebugDisplaySettings().m_renderPlayerCameras && IsClone() && pViewport)
    {
        extents_t e;
        pViewport->DebugGetExtents(e.extents);
        RenderViewport(e);
    }
#endif // __BANK
}

#if __BANK
void CNetObjPlayer::GetHealthDisplayString(char *buffer) const
{
    if(buffer)
    {
        CPed *ped = GetPed();

        if(ped)
        {
            float maxArmour = ped->GetPlayerInfo() ? static_cast<float>(ped->GetPlayerInfo()->MaxArmour) : 0.0f;
            sprintf(buffer, "Hlth: %.2f, Arm: %.2f, MaxHlth: %.2f, MaxArm: %.2f", ped->GetHealth(), ped->GetArmour(), ped->GetMaxHealth(), maxArmour);
        }
    }
}

void CNetObjPlayer::DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement)
{
	CNetObjPed::DisplayNetworkInfoForObject(col, scale, screenCoords, debugTextYIncrement);

	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	const unsigned int MAX_STRING = 200;
	char str[MAX_STRING];

	if(displaySettings.m_displayVisibilityInfo)
	{
		if (m_tutorialIndex != INVALID_TUTORIAL_INDEX)
		{
			sprintf(str, "In tutorial. Index = %d", m_tutorialIndex);
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
			screenCoords.y += debugTextYIncrement;
		}

		if (GetPlayerOwner()->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
		{
			sprintf(str, "In cutscene");
			DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str));
			screenCoords.y += debugTextYIncrement;
		}
	}

	if (displaySettings.m_displayGhostInfo)
	{
		if (m_bGhost)
		{
			if (!IsClone() && ms_ghostScriptId.IsValid())
			{
				snprintf(str, MAX_STRING, "Ghost script: %s", ms_ghostScriptId.GetLogName());
				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
				screenCoords.y += debugTextYIncrement;	
			}

			if (m_ghostPlayerFlags != 0)
			{
				snprintf(str, MAX_STRING, "Ghost player flags: %d", m_ghostPlayerFlags);
				DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, col, scale, str) );
				screenCoords.y += debugTextYIncrement;	
			}
		}
	}
}

#endif
void CNetObjPlayer::UpdateCachedTutorialSessionState()
{
    const CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();

    if(localPlayer && localPlayer->GetPlayerPed() && localPlayer->GetPlayerPed()->GetNetworkObject())
    {
        unsigned           numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            CNetGamePlayer *remotePlayer = SafeCast(CNetGamePlayer, remotePhysicalPlayers[index]);

            if(remotePlayer->GetPlayerPed() && remotePlayer->GetPlayerPed()->GetNetworkObject() && remotePlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
            {
                const CNetObjPlayer *localNetObjPlayer  = SafeCast(const CNetObjPlayer, localPlayer->GetPlayerPed()->GetNetworkObject());
                const CNetObjPlayer *remoteNetObjPlayer = SafeCast(const CNetObjPlayer, remotePlayer->GetPlayerPed()->GetNetworkObject());

                if((localNetObjPlayer->GetTutorialIndex()      == CNetObjPlayer::SOLO_TUTORIAL_INDEX)          ||
                   (remoteNetObjPlayer->GetTutorialIndex()     == CNetObjPlayer::SOLO_TUTORIAL_INDEX)          ||
                   (localNetObjPlayer->GetTutorialIndex()      != remoteNetObjPlayer->GetTutorialIndex())      ||
                   (localNetObjPlayer->GetTutorialInstanceID() != remoteNetObjPlayer->GetTutorialInstanceID()))
                {
                    remotePlayer->SetIsInDifferentTutorialSession(true);
                }
                else
                {
                    remotePlayer->SetIsInDifferentTutorialSession(false);
                }
            }
        }
    }
}

bool CNetObjPlayer::ApplyHeadBlendData(CPed& ped, CPedHeadBlendData& headBlendData)
{
	bool bApplied = true;

	headBlendData.m_async = true;

	// ped head blend data
	const CPedHeadBlendData *pedHeadBlendData = ped.GetExtensionList().GetExtension<CPedHeadBlendData>();

	if (pedHeadBlendData && headBlendData.HasDuplicateHeadData(*pedHeadBlendData))
	{
		LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("## CNetObjPlayer::ApplyHeadBlendData ignored : Data not changed ##\r\n"));
		return true;
	}
	
	//Previously there was a quick out here if the head blend data was already finalized.  But, this prevents subsequent application of data - when data has really changed - don't bail if Finalized already.  lavalley.

	if (bApplied && !ped.SetHeadBlendData(headBlendData, true)) //push this through as forced to ensure the head blend is updated - otherwise modifications in the if (bApplied) section below will not get applied. lavalley.
	{
		LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("## CNetObjPlayer::ApplyHeadBlendData failed : SetHeadBlendData ##\r\n"));
		bApplied = false;
	}

	if (bApplied)
	{
		//Please ensure this remains on dev_ng as it is needed. lavalley
		ped.SetHeadBlendEyeColor(headBlendData.m_eyeColor);

		// Moved this from previous location above - to here where it was originally - important - as the headblend for the ped might not be established yet where this was previously located.
		// If the headblend isn't present for the ped when this was above, the hair color will never be applied.  I have reproed this problem and this is a confirmed/tested solution.
		// This is the right place for this method call. lavalley
		ped.SetHairTintIndex(headBlendData.m_hairTintIndex,headBlendData.m_hairTintIndex2);

		for (s32 i = 0; i < HOS_MAX; ++i)
		{
			if (headBlendData.m_overlayTex[i] != 255)
			{
				ped.SetHeadOverlay((eHeadOverlaySlots)i, headBlendData.m_overlayTex[i], headBlendData.m_overlayAlpha[i], headBlendData.m_overlayNormAlpha[i]);
			}
			else
			{
				ped.SetHeadOverlay((eHeadOverlaySlots)i, 0, 0.0f, 0.0f);
			}

			ped.SetHeadOverlayTintIndex((eHeadOverlaySlots)i, (eRampType) headBlendData.m_overlayRampType[i], headBlendData.m_overlayTintIndex[i], headBlendData.m_overlayTintIndex2[i]);
		}

		for (s32 i = 0; i < MMT_MAX; ++i)
		{
			ped.MicroMorph((eMicroMorphType)i,headBlendData.m_microMorphBlends[i]);
		}

		if (headBlendData.m_usePaletteRgb)
		{
			for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
			{
				if (!ped.SetHeadBlendPaletteColor(headBlendData.m_paletteRed[i], headBlendData.m_paletteGreen[i], headBlendData.m_paletteBlue[i], i))
				{
					LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().Log("## CNetObjPlayer::ApplyHeadBlendData failed : SetHeadBlendPaletteColor ##\r\n"));
					bApplied = false;
				}
			}
		}
		else
		{
			ped.SetHeadBlendPaletteColorDisabled();
		}

		ped.FinalizeHeadBlend();
	}

	return bApplied;
}

// sync parser creation functions
void CNetObjPlayer::GetPlayerCreateData(CPlayerCreationDataNode& data) const
{
    CPed* pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

    CBaseModelInfo *modelInfo = pPlayer->GetBaseModelInfo();
    if(gnetVerifyf(modelInfo, "No model info for networked object!"))
    {
		// really means "did a helmet get put on via the CPedHelmetComponent" - only need to set this once, then it's locally set...
		data.m_wearingAHelmet = pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet );

        data.m_ModelHash = modelInfo->GetHashKey();

        GetScarAndBloodData(data.m_NumBloodMarks, data.m_BloodMarkData, data.m_NumScars, data.m_ScarData);
    }
}


void CNetObjPlayer::SetPlayerCreateData(const CPlayerCreationDataNode& data)
{
    // ensure that the model is loaded and ready for drawing for this ped
    fwModelId modelId;
    CModelInfo::GetBaseModelInfoFromHashKey(data.m_ModelHash, &modelId);

    if (AssertVerify(CModelInfo::HaveAssetsLoaded(modelId)) && AssertVerify(GetPlayerOwner()))
    {
        Matrix34 tempMat;
        tempMat.Identity();

#if __DEV
        // set the x,y and z components to random numbers to avoid a physics assert
        tempMat.d.x = fwRandom::GetRandomNumberInRange(0.1f, 1.0f);
        tempMat.d.y = fwRandom::GetRandomNumberInRange(0.1f, 1.0f);
        tempMat.d.z = fwRandom::GetRandomNumberInRange(0.1f, 1.0f);
#endif // __DEV

        const CControlledByInfo networkPlayerControl(true, true);
        CPed* pPlayerPed = CPedFactory::GetFactory()->CreatePlayerPed(networkPlayerControl, modelId.GetModelIndex(), &tempMat, GetPlayerOwner()->GetPlayerInfo());

        NETWORK_QUITF(pPlayerPed, "Failed to create a cloned player ped. Cannot continue.");

        // players are always set as permanent
        pPlayerPed->PopTypeSet(POPTYPE_PERMANENT);

        SetGameObject((void*) pPlayerPed);
        pPlayerPed->SetNetworkObject(this);

        CGameWorld::AddPlayerToWorld(pPlayerPed, VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));

		// setup this ped's AI as a network clone
		SetupPedAIAsClone();

        // setup the players default variations
        pPlayerPed->SetVarDefault();

        // setup the players group now that there is a network object for the player
        pPlayerPed->GetPlayerInfo()->SetupPlayerGroup();

        m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID;

		// Let the player know if the helmet was attached using the inventory or the CPedHelmetComponent
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet, data.m_wearingAHelmet );

        // add the player scars and blood marks
        SetScarAndBloodData(data.m_NumBloodMarks, data.m_BloodMarkData, data.m_NumScars, data.m_ScarData);
    }
}

// sync parser getter functions
void CNetObjPlayer::GetPlayerSectorPosData(CPlayerSectorPosNode& data) const
{
    CPed *pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);

    GetSectorPosition(data.m_SectorPosX, data.m_SectorPosY, data.m_SectorPosZ);

    data.m_IsRagdolling = pPlayerPed->GetUsingRagdoll();
    data.m_IsOnStairs   = pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected);
	data.m_StealthNoise = pPlayerPed->GetPlayerInfo()->GetStealthNoise();

    // check whether the player is standing on another network object
    GetStandingOnObjectData(data.m_StandingOnNetworkObjectID, data.m_LocalOffset);
}

void GetDecoratorList(CPlayerGameStateDataNode& data, fwDecoratorExtension* pExt)
{
	u32 count = 0;
	if( pExt != NULL )
	{
		// Decorator sync
		count = pExt->GetCount();

		gnetAssertf(count<=CPlayerGameStateDataNode::MAX_PLAYERINFO_DECOR_ENTRIES, "PlayerInfo Decorator count[%d] exceeds list max[%d]",count,CPlayerGameStateDataNode::MAX_PLAYERINFO_DECOR_ENTRIES);

		if( count > CPlayerGameStateDataNode::MAX_PLAYERINFO_DECOR_ENTRIES )
		{
			count = CPlayerGameStateDataNode::MAX_PLAYERINFO_DECOR_ENTRIES;
		}

		fwDecorator* pDec = pExt->GetRoot();
		for(u32 decor = data.m_decoratorListCount; decor < count; decor++)
		{
			data.m_decoratorList[decor].Type = pDec->GetType();

			atHashWithStringBank tempKey = pDec->GetKey();
			data.m_decoratorList[decor].Key = tempKey.GetHash();

			atHashWithStringBank String;
			switch( data.m_decoratorList[decor].Type )
			{
			case fwDecorator::TYPE_FLOAT:
				pDec->Get(data.m_decoratorList[decor].Float);
				break;

			case fwDecorator::TYPE_BOOL:
				pDec->Get(data.m_decoratorList[decor].Bool);
				break;

			case fwDecorator::TYPE_INT:
				pDec->Get(data.m_decoratorList[decor].Int);
				break;

			case fwDecorator::TYPE_STRING:
				pDec->Get(String);
				data.m_decoratorList[decor].String = String.GetHash();
				break;

			case fwDecorator::TYPE_TIME:
				{
					fwDecorator::Time temp = fwDecorator::UNDEF_TIME;
					pDec->Get(temp);
					data.m_decoratorList[decor].Int = (int)temp;
				}
				break;

			default:
				gnetAssertf(false, "Unknown decorator type?");
				break;
			}
			pDec = pDec->m_Next;
		}
	}

	data.m_decoratorListCount = count;
}

void CNetObjPlayer::GetPlayerGameStateData(CPlayerGameStateDataNode& data)
{
    CPed *pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);

    if (IsSpectating())
    {
        data.m_SpectatorId = m_SpectatorObjectId;
    }
	data.m_bIsSCTVSpectating = GetPlayerOwner()->IsSpectator(); // SCTV spectator mode

    if (IsAntagonisticToPlayer())
    {
        gnetAssertf(m_AntagonisticPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Invalid index \"%d\"", m_AntagonisticPlayerIndex);
        data.m_AntagonisticPlayerIndex = m_AntagonisticPlayerIndex;
    }

    data.m_PlayerState = static_cast<u32>(pPlayerPed->GetPlayerInfo()->GetPlayerState() + 1); // player state is synced +1 to reduce bandwidth

    data.m_GameStateFlags.controlsDisabledByScript = (pPlayerPed->GetPlayerInfo()->IsControlsScriptDisabled());

    const u32 mobileRingType = pPlayerPed->GetPhoneComponent() ? pPlayerPed->GetPhoneComponent()->GetMobileRingType() : AUD_CELLPHONE_SILENT;
    if(mobileRingType != AUD_CELLPHONE_SILENT && mobileRingType != AUD_CELLPHONE_VIBE)
    {
        data.m_MobileRingState = pPlayerPed->GetPhoneComponent()->GetMobileRingState();
    }
	else
	{
		data.m_MobileRingState = CPedPhoneComponent::DEFAULT_RING_STATE;
	}

	if (!AssertVerify(GetPlayerOwner()))
		return;

    data.m_PlayerTeam                              = GetPlayerOwner()->GetTeam();
    data.m_AirDragMult                             = pPlayerPed->GetPlayerInfo()->m_fForceAirDragMult;
    data.m_MaxHealth                               = static_cast<u32>(pPlayerPed->GetMaxHealth());
    data.m_MaxArmour                               = pPlayerPed->GetPlayerInfo()->MaxArmour;
	data.m_GameStateFlags.newMaxHealthArmour       = data.m_MaxHealth != CPlayerInfo::PLAYER_DEFAULT_MAX_HEALTH || data.m_MaxArmour != CPlayerInfo::PLAYER_DEFAULT_MAX_ARMOUR;
	data.m_GameStateFlags.bHasMaxHealth		       = (pPlayerPed->GetHealth() >= pPlayerPed->GetMaxHealth());
    data.m_GameStateFlags.myVehicleIsMyInteresting = pPlayerPed->GetMyVehicle() == CVehiclePopulation::GetInterestingVehicle();
	data.m_GameStateFlags.cantBeKnockedOffBike	   = static_cast<u8>(pPlayerPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle());
	data.m_JackSpeed							   = pPlayerPed->GetPlayerInfo()->JackSpeed;
	data.m_GameStateFlags.hasSetJackSpeed		   = data.m_JackSpeed != CPlayerInfo::NETWORK_PLAYER_DEFAULT_JACK_SPEED;
    data.m_GameStateFlags.isSpectating             = IsSpectating();
    data.m_GameStateFlags.isAntagonisticToPlayer   = IsAntagonisticToPlayer();
    data.m_GameStateFlags.neverTarget              = pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed );
    data.m_GameStateFlags.useKinematicPhysics      = pPlayerPed->ShouldUseKinematicPhysics();
    data.m_GameStateFlags.inTutorial               = m_tutorialIndex != INVALID_TUTORIAL_INDEX;
	data.m_GameStateFlags.pendingTutorialSessionChange = IsTutorialSessionChangePending();
    data.m_TutorialIndex                           = m_tutorialIndex;
    data.m_TutorialInstanceID                      = m_tutorialInstanceID;

    NetworkInterface::GetVoice().BuildReceiveBitfield(data.m_OverrideReceiveChat);
	NetworkInterface::GetVoice().BuildSendBitfield(data.m_OverrideSendChat);
    data.m_bOverrideTutorialChat = NetworkInterface::GetVoice().IsOverrideTutorialSession(); 
    data.m_bOverrideTransitionChat = NetworkInterface::GetVoice().IsOverrideTransitionChat();

    data.m_GameStateFlags.respawning								= m_respawnInvincibilityTimer > 0;
	data.m_GameStateFlags.willJackAnyPlayer							= pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillJackAnyPlayer );
	data.m_GameStateFlags.willJackWantedPlayersRatherThanStealCar	= pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillJackWantedPlayersRatherThanStealCar );
	data.m_GameStateFlags.dontDragMeOutOfCar						= pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar );
	data.m_GameStateFlags.playersDontDragMeOutOfCar					= pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar );
    data.m_GameStateFlags.randomPedsFlee							= pPlayerPed->GetPlayerWanted()->m_AllRandomsFlee;
	data.m_GameStateFlags.everybodyBackOff							= pPlayerPed->GetPlayerWanted()->m_EverybodyBackOff;
	data.m_GameStateFlags.bHelmetHasBeenShot						= pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HelmetHasBeenShot );
    data.m_GameStateFlags.notDamagedByBullets                       = pPlayerPed->m_nPhysicalFlags.bNotDamagedByBullets;
    data.m_GameStateFlags.notDamagedByFlames                        = pPlayerPed->m_nPhysicalFlags.bNotDamagedByFlames;
    data.m_GameStateFlags.ignoresExplosions                         = pPlayerPed->m_nPhysicalFlags.bIgnoresExplosions;
    data.m_GameStateFlags.notDamagedByCollisions                    = pPlayerPed->m_nPhysicalFlags.bNotDamagedByCollisions;
    data.m_GameStateFlags.notDamagedByMelee                         = pPlayerPed->m_nPhysicalFlags.bNotDamagedByMelee;
    data.m_GameStateFlags.notDamagedBySteam                         = pPlayerPed->m_nPhysicalFlags.bNotDamagedBySteam;
    data.m_GameStateFlags.notDamagedBySmoke                         = pPlayerPed->m_nPhysicalFlags.bNotDamagedBySmoke;
	data.m_GameStateFlags.playerIsWeird								= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerIsWeird);
	data.m_GameStateFlags.noCriticalHits							= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NoCriticalHits);
	data.m_GameStateFlags.disableHomingMissileLockForVehiclePedInside	= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHomingMissileLockForVehiclePedInside);
	data.m_GameStateFlags.ignoreMeleeFistWeaponDamageMult			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreMeleeFistWeaponDamageMult);
	data.m_GameStateFlags.lawPedsCanFleeFromNonWantedPlayer			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_LawPedsCanFleeFromNonWantedPlayer);
	data.m_GameStateFlags.hasHelmet									= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet);
	data.m_GameStateFlags.dontTakeOffHelmet							= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontTakeOffHelmet);
	data.m_GameStateFlags.isSwitchingHelmetVisor					= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor);
	data.m_GameStateFlags.forceHelmetVisorSwitch					= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch);
	data.m_GameStateFlags.isPerformingVehicleMelee					= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee);
	data.m_GameStateFlags.useOverrideFootstepPtFx					= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseOverrideFootstepPtFx);
	data.m_GameStateFlags.disableVehicleCombat						= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableVehicleCombat);
	data.m_GameStateFlags.treatFriendlyTargettingAndDamage			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamage);
	data.m_GameStateFlags.useKinematicModeWhenStationary			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseKinematicModeWhenStationary);
	data.m_GameStateFlags.dontActivateRagdollFromExplosions			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions);
	data.m_GameStateFlags.allowBikeAlternateAnimations				= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowBikeAlternateAnimations);
	data.m_GameStateFlags.useLockpickVehicleEntryAnimations			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations);
	data.m_GameStateFlags.m_PlayerPreferFrontSeat					= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerPreferFrontSeatMP);
	data.m_GameStateFlags.ignoreInteriorCheckForSprinting			= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting);
	data.m_GameStateFlags.PRF_BlockRemotePlayerRecording			= pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_BlockRemotePlayerRecording );
	data.m_GameStateFlags.dontActivateRagdollFromVehicleImpact		= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact);
	data.m_GameStateFlags.swatHeliSpawnWithinLastSpottedLocation	= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwatHeliSpawnWithinLastSpottedLocation);
	data.m_GameStateFlags.disableStartEngine						= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableStartEngine);
	data.m_GameStateFlags.lawOnlyAttackIfPlayerIsWanted				= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_LawWillOnlyAttackIfPlayerIsWanted);
	data.m_GameStateFlags.disableHelmetArmor						= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHelmetArmor);
	data.m_GameStateFlags.PRF_UseScriptedWeaponFirePosition			= pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_UseScriptedWeaponFirePosition);
	data.m_GameStateFlags.pedIsArresting							= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting);
	data.m_GameStateFlags.isScuba									= pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba);
	
	data.m_IsTargettableByTeam   = m_isTargettableByTeam;

	// voice loudness
	const CNetGamePlayer* pPlayer = GetPlayerOwner();
	data.m_GameStateFlags.bHasMicrophone = NetworkInterface::GetVoice().GamerHasHeadset(pPlayer->GetGamerInfo().GetGamerHandle());
	const u32 uSystemTimeInMilliseconds = fwTimer::GetSystemTimeInMilliseconds();
	if (uSystemTimeInMilliseconds > m_uThrottleVoiceLoudnessTime)
	{
		m_fVoiceLoudness = data.m_GameStateFlags.bHasMicrophone ? NetworkInterface::GetVoice().GetPlayerLoudness(NetworkInterface::GetLocalGamerIndex()) : 0.0f;
		m_uThrottleVoiceLoudnessTime = uSystemTimeInMilliseconds + TIME_TO_THROTTLE_VOICELOUDNESS;
	}

	data.m_fVoiceLoudness = m_fVoiceLoudness;

	// voice channel 
	data.m_nVoiceChannel = pPlayer->GetVoiceChannel();

    // voice proximity
    data.m_bHasVoiceProximityOverride = pPlayer->HasVoiceProximityOverride();
    if(data.m_bHasVoiceProximityOverride)
        pPlayer->GetVoiceProximity(data.m_vVoiceProximityOverride);
    else
        data.m_vVoiceProximityOverride = Vector3(VEC3_ZERO);

	data.m_bHasScriptedWeaponFirePos = pPlayerPed->GetPlayerInfo()->GetHasScriptedWeaponFirePos();
	if (data.m_bHasScriptedWeaponFirePos)
		data.m_ScriptedWeaponFirePos = VEC3V_TO_VECTOR3(pPlayerPed->GetPlayerInfo()->GetScriptedWeaponFiringPos());
	else
		data.m_ScriptedWeaponFirePos = Vector3(VEC3_ZERO);

	data.m_sizeOfNetArrayData = GetPlayerOwner()->GetSizeOfNetArrayData();

	data.m_GameStateFlags.bInvincible = pPlayerPed->m_nPhysicalFlags.bNotDamagedByAnything;

#if !__FINAL
	if(pPlayerPed->IsLocalPlayer())
	{
		data.m_GameStateFlags.bDebugInvincible = CPlayerInfo::IsPlayerInvincible();
	}
#endif
	
	data.m_vehicleweaponindex = pPlayerPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();

	// Decorator sync
	data.m_decoratorListCount = 0;

	fwDecoratorExtension* pExt = pPlayerPed->GetPlayerInfo() ? pPlayerPed->GetPlayerInfo()->GetExtension<fwDecoratorExtension>() : NULL; 
	GetDecoratorList(data,pExt);

    const CControl *control = pPlayerPed->GetControlFromPlayer();

    data.m_GarageInstanceIndex    = pPlayer->GetGarageInstanceIndex();
	data.m_nPropertyID			  = pPlayer->GetPropertyID();
	data.m_nMentalState			  = pPlayer->GetMentalState();
	WIN32PC_ONLY(data.m_nPedDensity			  = pPlayer->GetPedDensity();)
	data.m_bIsFriendlyFireAllowed = IsFriendlyFireAllowed();
	data.m_bIsPassiveMode		  = IsPassiveMode();
	data.m_bBattleAware			  = pPlayerPed->GetPedIntelligence()->IsBattleAware();

    if(pPlayerPed->GetIsInVehicle() && pPlayerPed->GetMyVehicle() && pPlayerPed->GetMyVehicle()->InheritsFromBike() && control)
    {
        data.m_VehicleJumpDown = control->GetVehicleJump().IsDown();
    }
    else
    {
        data.m_VehicleJumpDown = false;
    }

	data.m_WeaponDefenseModifier  = pPlayerPed->GetPlayerInfo()->GetPlayerWeaponDefenseModifier();

	data.m_WeaponMinigunDefenseModifier  = pPlayerPed->GetPlayerInfo()->GetPlayerWeaponMinigunDefenseModifier();

	data.m_bUseExtendedPopulationRange = CPedPopulation::ms_useTempPopControlSphereThisFrame;
	if(CPedPopulation::ms_useTempPopControlSphereThisFrame)
	{
		data.m_vExtendedPopulationRangeCenter = CFocusEntityMgr::GetMgr().GetPopPos();
	}

    data.m_nCharacterRank = pPlayer->GetCharacterRank();

	data.m_bDisableLeavePedBehind = m_bDisableLeavePedBehind;

	data.m_bCollisionsDisabledByScript = pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision);

	data.m_bInCutscene = IsInCutsceneLocally();
	data.m_bGhost = m_bGhost;

	data.m_LockOnTargetID = m_TargetCloningInfo.GetLockOnTargetID(pPlayerPed);
	if(data.m_LockOnTargetID == NETWORK_INVALID_OBJECT_ID)
	{
		data.m_LockOnTargetID = m_TargetCloningInfo.GetScriptAimTargetID();
	}

	data.m_LockOnState = m_TargetCloningInfo.GetLockOnTargetState(pPlayerPed);
	if(data.m_LockOnState == static_cast<unsigned>(CEntity::HLOnS_NONE))
	{
		data.m_LockOnState = m_TargetCloningInfo.GetScriptLockOnState();
	}

	data.m_VehicleShareMultiplier = rage::Min(CVehiclePopulation::CalculateVehicleShareMultiplier(), 10.0f);

	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
	data.m_WeaponDamageModifier = pPlayerInfo->GetPlayerWeaponDamageModifier();
	data.m_MeleeDamageModifier = pPlayerInfo->GetPlayerMeleeWeaponDamageModifier();
	data.m_MeleeUnarmedDamageModifier = pPlayerInfo->GetPlayerMeleeUnarmedDamageModifier();
	data.m_bIsSuperJump = pPlayerInfo->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON);
	data.m_EnableCrewEmblem = pPlayerPed->GetEnableCrewEmblem();

	data.m_ConcealedOnOwner = m_entityConcealedLocally;

	const CPlayerArcadeInformation& pPlayerArcadeInfo = pPlayerInfo->GetArcadeInformation();
	data.m_arcadeTeamInt = static_cast<u8>(pPlayerArcadeInfo.GetTeam());
	data.m_arcadeRoleInt = static_cast<u8>(pPlayerArcadeInfo.GetRole());
	data.m_arcadeCNCVOffender = pPlayerArcadeInfo.GetCNCVOffender();

	data.m_arcadePassiveAbilityFlags = pPlayerInfo->GetArcadeAbilityEffects().GetArcadePassiveAbilityEffectsFlags();

	data.m_bIsShockedFromDOTEffect = pPlayerInfo->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_DOT_SHOCKED);
	data.m_bIsChokingFromDOTEffect = pPlayerInfo->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_DOT_CHOKING);

}

void CNetObjPlayer::GetPlayerAppearanceData(CPlayerAppearanceDataNode& data)
{
    CPed *pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);
    CBaseModelInfo *modelInfo = pPlayerPed->GetBaseModelInfo();
    if(gnetVerifyf(modelInfo, "No model info for networked object!"))
    {
        data.m_NewModelHash = modelInfo->GetHashKey();
    }
	data.m_VoiceHash = (pPlayerPed && pPlayerPed->GetSpeechAudioEntity()) ? pPlayerPed->GetSpeechAudioEntity()->GetAmbientVoiceName() : g_NoVoiceHash;
    data.m_RespawnNetObjId = m_RespawnNetObjId; //NETWORK_INVALID_OBJECT_ID;

	//Only set the respawn network Object Id if the respawn is still in progress.
	//  So that the appearance can be applied to the player.
	if (NETWORK_INVALID_OBJECT_ID != m_RespawnNetObjId)
	{
		netObject* networkObject = CNetwork::GetObjectManager().GetNetworkObject(m_RespawnNetObjId, true);
		if (networkObject && networkObject->IsLocalFlagSet(CNetObjGame::LOCALFLAG_RESPAWNPED))
		{
			data.m_RespawnNetObjId = m_RespawnNetObjId;
		}
	}

    CNetObjPed::GetPedAppearanceData(data.m_PedProps, data.m_VariationData, data.m_phoneMode, data.m_parachuteTintIndex, data.m_parachutePackTintIndex, data.m_facialClipSetId, data.m_facialIdleAnimOverrideClipNameHash, data.m_facialIdleAnimOverrideClipDictNameHash, data.m_isAttachingHelmet, data.m_isRemovingHelmet, data.m_isWearingHelmet, data.m_helmetTextureId, data.m_helmetProp, data.m_visorPropUp, data.m_visorPropDown, data.m_visorIsUp, data.m_supportsVisor, data.m_isVisorSwitching, data.m_targetVisorState);
	GetSecondaryPartialAnimData(data.m_secondaryPartialAnim);
    // ped head blend data
    CPedHeadBlendData *pedHeadBlendData = pPlayerPed->GetExtensionList().GetExtension<CPedHeadBlendData>();

    if(!pedHeadBlendData)
    {
        data.m_HasHeadBlendData = false;
    }
    else
    {
        data.m_HasHeadBlendData = true;
		data.m_HeadBlendData = *pedHeadBlendData;
	}

    // medals/tattoos - ped decorations, not to be confused with script ped decorators, which are something else!
	
	PedDecorationManager &decorationMgr = PedDecorationManager::GetInstance();

	// see if we need to make a new decorations list, or if the cached copy is good enough
	s32 listIndex=-1;
	float timeStamp=-1;
	decorationMgr.GetDecorationListIndexAndTimeStamp(pPlayerPed, listIndex, timeStamp);

	CPedVariationData& pedVarData = pPlayerPed->GetPedDrawHandler().GetVarData();		
	data.m_crewLogoTxdHash = pedVarData.GetOverrideCrewLogoTxdHash();
	data.m_crewLogoTexHash = pedVarData.GetOverrideCrewLogoTexHash();
	
	if (listIndex<0 || timeStamp<=0.0f)
	{
		memset(data.m_PackedDecorations, 0, sizeof(u32)*NUM_DECORATION_BITFIELDS);
		m_DecorationListCachedIndex=listIndex;
		m_DecorationListTimeStamp=timeStamp;
	}
	else if (listIndex==m_DecorationListCachedIndex && timeStamp==m_DecorationListTimeStamp)
	{
		memcpy(data.m_PackedDecorations, m_DecorationListCachedBits, sizeof(u32)*NUM_DECORATION_BITFIELDS);
	}
	else
	{
		data.m_HasDecorations = decorationMgr.GetDecorationsBitfieldFromLocalPed(pPlayerPed, data.m_PackedDecorations, NUM_DECORATION_BITFIELDS, m_DecorationListCrewEmblemVariation);

		if(!data.m_HasDecorations)
			memset(data.m_PackedDecorations, 0, sizeof(u32)*NUM_DECORATION_BITFIELDS);

		memcpy(m_DecorationListCachedBits, data.m_PackedDecorations, sizeof(u32)*NUM_DECORATION_BITFIELDS);
		m_DecorationListCachedIndex=listIndex;
		m_DecorationListTimeStamp=timeStamp;
	}

	data.m_networkedDamagePack = m_networkedDamagePack;
	data.m_crewEmblemVariation = m_DecorationListCrewEmblemVariation;
 }

void CNetObjPlayer::GetCameraData(CPlayerCameraDataNode& data)
{
    CPed* pPlayerPed = GetPlayerPed();
	gnetAssert(pPlayerPed);
	Vector3 playerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

    grcViewport *pViewport = 0;

    if (IsClone())
        pViewport = GetViewport();
    else
        pViewport = &gVpMan.GetGameViewport()->GetNonConstGrcViewport();

    gnetAssert(pViewport);
	
    Matrix34 camM = RCC_MATRIX34(pViewport->GetCameraMtx());

    Vector3 eulers;
    camM.ToEulersXYZ(eulers);

#if ENABLE_NETWORK_BOTS
	if(GetPlayerOwner() && GetPlayerOwner()->IsBot())
	{
		eulers.z = pPlayerPed->GetCurrentHeading();
	}
#endif // ENABLE_NETWORK_BOTS

    while(eulers.x < 0.0f) eulers.x += TWO_PI;
    while(eulers.x > TWO_PI) eulers.x -= TWO_PI;
    while(eulers.z < 0.0f) eulers.z += TWO_PI;
    while(eulers.z > TWO_PI) eulers.z -= TWO_PI;

    data.m_eulersX = eulers.x;
    data.m_eulersZ = eulers.z;

    // check if we are using a non-gameplay camera (such as cinematic etc...) This may be offset
    // from the player enough for entities to be created/removed while on screen by remote players
    bool usingGameplayCamera	= camInterface::IsRenderingDirector(camInterface::GetGameplayDirector());
	bool usingDebugCamera		= camInterface::GetDebugDirector().IsFreeCamActive();
	bool inFPcam				= pPlayerPed ? pPlayerPed->IsInFirstPersonVehicleCamera() : false;

	data.m_UsingFreeCamera = m_UsingFreeCam || IsSpectating() || (!usingGameplayCamera && !usingDebugCamera && !inFPcam);
	data.m_UsingCinematicVehCamera = false;

	// IMPORTANT: if we are sending data.m_UsingFreeCamera we need to ensure we are sending the right coordinates, use data.m_UsingFreeCamera as the discrimanator here rather than m_UsingFreeCamera.
    if(data.m_UsingFreeCamera)
    {
		data.m_UsingCinematicVehCamera  = m_UsingCinematicVehCam || (camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera() && pPlayerPed && pPlayerPed->GetIsInVehicle());
        data.m_Position              = camM.d;
        data.m_largeOffset           = false;
        data.m_aiming                = false;
        data.m_longRange             = false;
        data.m_freeAimLockedOnTarget = false;
        data.m_targetId              = NETWORK_INVALID_OBJECT_ID;

        data.m_playerToTargetAimOffset.Zero();
        data.m_lockOnTargetOffset.Zero();

		data.m_canOwnerMoveWhileAiming	 = false;
    }
    else
    {
        data.m_Position = camM.d - VEC3V_TO_VECTOR3(GetPlayerPed()->GetTransform().GetPosition());

		// We are aiming if the weapon target code says we are and we've actually set some data and
		// if the player is using TaskGun or other target setting tasks the target info gets updated every frame so the player should never be more than 5m away 
		// from the point the target info is recorded unless the player been teleported or the task has ended (so target updating stops), the player has moved 
		// to a different point and this is the final time the info will be sent before the target info is wiped. either way, it's invalid if the player is too far
		// away from where we recorded the target info.
		bool playerTeleported = (m_TargetCloningInfo.GetCurrentPlayerPos().Dist2(playerPos) > (5.0f * 5.0f));
		bool targetTeleported = false;
		if(m_TargetCloningInfo.GetCurrentAimTargetEntity())
		{
			Vector3 currentEntityPos = VEC3V_TO_VECTOR3(m_TargetCloningInfo.GetCurrentAimTargetEntity()->GetTransform().GetPosition());
			targetTeleported = m_TargetCloningInfo.GetCurrentAimTargetPos().Dist2(currentEntityPos) > (50.0f * 50.0f);
		}

		// The player can change weapon (without using TaskSwapWeapon in TaskCover) while the target information isn't updated so the ranges can be wrong - if we've changed weapon, data is invalid..
		bool changedWeapon	  = pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash() != m_TargetCloningInfo.GetCurrentWeaponHash();

	    // From weapons.meta - all weapons fall under 
	    // 150m unless it's a sniper rifle and then it's 1500.f
	    // No remote sniper in MP
	    const CWeapon* pWeapon = NULL;
	    const CObject* pWeaponObject = NULL;
		
	    if(pPlayerPed->GetWeaponManager())
	    {
		    pWeapon			= pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
		    pWeaponObject	= pPlayerPed->GetWeaponManager()->GetEquippedWeaponObject();
	    }

		bool isArmed	  					= pWeapon && pWeaponObject;

	    data.m_aiming						= isArmed && !changedWeapon && !playerTeleported && !targetTeleported && m_TargetCloningInfo.HasValidTarget();
		data.m_canOwnerMoveWhileAiming		= CTaskMotionAiming::GetCanMove(pPlayerPed);

	    data.m_targetId						= NETWORK_INVALID_OBJECT_ID;
		data.m_bAimTargetEntity				= false;
	    data.m_longRange					= false;
	    data.m_freeAimLockedOnTarget		= false;
	    data.m_playerToTargetAimOffset.x	= data.m_playerToTargetAimOffset.y = data.m_playerToTargetAimOffset.z = 0.0f;
	    data.m_lockOnTargetOffset.x			= data.m_lockOnTargetOffset.y = data.m_lockOnTargetOffset.z	= 0.0f;
    	
	    if (AssertVerify(pPlayerPed) && data.m_aiming)
	    {
		    if(pWeapon)
		    {
			    float fWeaponRange = pWeapon->GetRange();

			    if(fWeaponRange <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE)
			    {
				    data.m_longRange = false;
			    }
			    else if((fWeaponRange > CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE) && (fWeaponRange <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE))
			    {
				    data.m_longRange = true;
			    }
			    else
			    {
				    gnetAssertf(false, "ERROR : CNetObjPlayer::GetCameraData : Invalid Weapon Range For MP!");
			    }
		    }

			//----

		    // Pick the ped we're generating the offset from...
		    CEntity* targetEntity = NULL;
		    Vector3 finallockOnTargetOffset(VEC3_ZERO);
		    if(m_TargetCloningInfo.GetCurrentAimTargetEntity())
		    {
			    // if we have a target, use that entity...
			    targetEntity					= m_TargetCloningInfo.GetCurrentAimTargetEntity();

				data.m_bAimTargetEntity			= true;
				
				// we need to know if we're locked on to the target or free aim targetting it (set in SetAimTarget on owner / SetCameraData on clones)...
			    data.m_freeAimLockedOnTarget	= m_TargetCloningInfo.IsFreeAimLockedOnTarget();
		    }
		    else
		    {
				// first try and generate an offset from a player if they are within weapon range 
				// players > nearby ped range will get missed out below but we really need to take players into accout
				// even if they aren't close by otherwise we can get spurious hits players will notice
				unsigned                 numPhysicalPlayers		= netInterface::GetNumRemotePhysicalPlayers();
				const netPlayer * const *remotePhysicalPlayers	= netInterface::GetRemotePhysicalPlayers();

				for(unsigned index = 0; index < numPhysicalPlayers; index++)
				{
					const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

					if(remotePlayer)
					{
						CPed const* remotePlayerPed = remotePlayer->GetPlayerPed();
						if(remotePlayerPed && !remotePlayerPed->GetIsInVehicle())
						{
							Vector3 remotePlayerPos = VEC3V_TO_VECTOR3(remotePlayerPed->GetTransform().GetPosition());	

							float weaponRangeSq = data.m_longRange ? (CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE * CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE) : (CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE * CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE);
							if(remotePlayerPos.Dist2(playerPos) < weaponRangeSq)
							{
								if(IsTargetEntitySuitableForOffsetAiming((CEntity*)remotePlayerPed, finallockOnTargetOffset))
								{
									targetEntity = (CEntity*)remotePlayerPed;
									break;
								}
							}
						}
					}
				}

				// not managed to generate an offset from a player, 
				// go through all nearby peds and see if we've got one we can use....
				if(!targetEntity)
				{
					CEntityScannerIterator nearbyPeds = pPlayerPed->GetPedIntelligence()->GetNearbyPeds();
					for(CEntity* pEntity = nearbyPeds.GetFirst(); pEntity; pEntity = nearbyPeds.GetNext())
					{
						CPed* pPed = static_cast<CPed*>(pEntity);
						if(pPed && !pPed->GetIsInVehicle() && IsTargetEntitySuitableForOffsetAiming(pEntity, finallockOnTargetOffset))
						{
							targetEntity = pEntity;
							break;
						}
					}
				}
		    }

			//----

			// range check for a vehicle or non-ped entity we're locked onto...
		    if(targetEntity)
		    {
				// if we're not suitable for aiming with an offset...
			    if(!IsTargetEntitySuitableForOffsetAiming(targetEntity, finallockOnTargetOffset))
			    {
					// Generate the offset anyway as it will be used in generating a world space position below...
					GenerateTargetForOffsetAiming(targetEntity, finallockOnTargetOffset);

					// if we've been aiming at an entity with no network object, the GetCurrentAimTargetPos will be pointing at the object pos
					// We have to change this to object pos + offset so the gun points at the right part of the object using a world space pos.
					//if(!NetworkUtils::GetNetworkObjectFromEntity(targetEntity))
					{
						Vector3 entityPos = VEC3V_TO_VECTOR3(targetEntity->GetTransform().GetPosition());
						Vector3 finalPos = entityPos + finallockOnTargetOffset;

						Vector3 startPos = playerPos;
		    			
						if(pWeapon && pWeaponObject)
						{
							Matrix34 const& weaponMatrix = MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix());
							pWeapon->GetMuzzlePosition(weaponMatrix, startPos);
						}

						Vector3 direction = finalPos - startPos;
						direction.Normalize();

						finalPos = startPos + (direction * (data.m_longRange ? CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE : CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE));

						SetAimTarget(finalPos);
					}

				    targetEntity = NULL;
			    }
		    }

			//----

			// not all entities have network objects....
			if(targetEntity && NetworkUtils::GetNetworkObjectFromEntity(targetEntity))
		    {
			    data.m_lockOnTargetOffset = finallockOnTargetOffset;

				data.m_targetId			  = NetworkUtils::GetNetworkObjectFromEntity(targetEntity)->GetObjectID();
		    }
		    else
		    {
			    // Targets are selected based on range from gun muzzle to target not player to target...
			    // but we may not have a weapon so we use the ped data as default...

			    // if the player is in a vehicle but is not driving then 
			    // the positional updates for the player are coming from elsewhere.
			    // this can invalidate the targetting:
			    // Frame X
			    // Game::Update
			    //		Network::Update
			    //			SetDataRecievedFromOwners	// 2. Vehicle position is updated and moves the car and local player a large distance in a single frame
			    //			GetDataToSendToClones 		// 3. Try to get weapon target data from local player to send � target is now > acceptable range so assert
			    //		GameWorld::Process
			    //			WeaponTarget::Update 		// 1. Weapon target is updated to something within acceptable range
			    // Frame X + 1
			    // so we update the target to now point where we are firing taking into account vehicle movements....

			    if(pPlayerPed->GetMyVehicle() && !pPlayerPed->GetIsDrivingVehicle())
			    {
				    CTaskGun* taskGun = (CTaskGun*)pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN);
				    if(taskGun)
				    {
					    taskGun->UpdateTarget(pPlayerPed);
				    }
			    }

			    Vector3 startPos		= playerPos;
    			
			    if(pWeapon && pWeaponObject)
			    {
				    Matrix34 const& weaponMatrix = MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix());
				    pWeapon->GetMuzzlePosition(weaponMatrix, startPos);
			    }
    			
				data.m_playerToTargetAimOffset = startPos - m_TargetCloningInfo.GetCurrentAimTargetPos();

				if(data.m_longRange)
				{
					data.m_playerToTargetAimOffset.x = Clamp(data.m_playerToTargetAimOffset.x, -CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE, CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE);
					data.m_playerToTargetAimOffset.y = Clamp(data.m_playerToTargetAimOffset.y, -CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE, CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE);
					data.m_playerToTargetAimOffset.z = Clamp(data.m_playerToTargetAimOffset.z, -CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE, CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE);
				}
				else
				{
					data.m_playerToTargetAimOffset.x = Clamp(data.m_playerToTargetAimOffset.x, -CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE, CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE);
					data.m_playerToTargetAimOffset.y = Clamp(data.m_playerToTargetAimOffset.y, -CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE, CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE);
					data.m_playerToTargetAimOffset.z = Clamp(data.m_playerToTargetAimOffset.z, -CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE, CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE);
				}
		    }
	    }
    }

	m_TargetCloningInfo.Clear();	

	// sync look at when specified by script or when in a vehicle
	bool bSyncLookAt = m_bSyncLookAt;
	bSyncLookAt |= pPlayerPed->GetIsInVehicle(); 

	// we are looking only if the player is looking, not looking at an entity and we have specified to sync look at
	data.m_isLooking = bSyncLookAt && pPlayerPed->IsLocalPlayer() && pPlayerPed->GetIkManager().IsLooking() && !pPlayerPed->GetIkManager().GetLookAtEntity();
	if(data.m_isLooking)
	{
#if !__FINAL
		// This function can trigger for clones when debugging. Don't mess synced m_vLookAtPosition in that case
		if(IsClone())
		{
			pPlayerPed->GetIkManager().GetLookAtTargetPosition(data.m_LookAtPosition);
			data.m_LookAtPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().UnTransform(RCC_VEC3V(data.m_LookAtPosition)));
		}
		else
#endif
		{
			// Update position only once every ~400s, or when position was reset by code
			if((fwTimer::GetSystemFrameCount() %12 ) == 0 || m_vLookAtPosition==VEC3_ZERO) 
			{
				pPlayerPed->GetIkManager().GetLookAtTargetPosition(m_vLookAtPosition);
				m_vLookAtPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().UnTransform(RCC_VEC3V(m_vLookAtPosition)));

				// data.m_LookAtPosition is in player space, but the serialiser deals with it thinking it's in world space. Instead of transforming/untransforming it
				// before and after serialising, or serialising it normalised with its original magnitude, let's just clamp the vector to something big enough.
				// We assume that even being a position this is safe to normalise, because player space means is kind of a vector from player origin
				const float fLookAtMagnitude = m_vLookAtPosition.Mag();
				const float MAX_LOOK_AT_DIST_SYNC_MAG = 1000.0f;
				if( Unlikely(fLookAtMagnitude > MAX_LOOK_AT_DIST_SYNC_MAG) )
				{
					m_vLookAtPosition.Normalize();
					m_vLookAtPosition.Scale(MAX_LOOK_AT_DIST_SYNC_MAG);
				}
			}

			data.m_LookAtPosition = m_vLookAtPosition;
		}
	}
	else
	{
		m_vLookAtPosition = VEC3_ZERO;
		data.m_LookAtPosition = Vector3(0.0f, 1.0f, 0.6f);
	}

    data.m_UsingLeftTriggerAimMode = false;

    const CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming *>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));

	if(pAimingTask && pAimingTask->UsingLeftTriggerAimMode())
    {
        data.m_UsingLeftTriggerAimMode = true;
    }

#if FPS_MODE_SUPPORTED
	float fSlopePitch;

	data.m_usingFirstPersonCamera	= IsClone() ? m_bUsingFirstPersonCamera : pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	data.m_usingFirstPersonVehicleCamera = IsClone() ? m_bUsingFirstPersonVehicleCamera : pPlayerPed->IsInFirstPersonVehicleCamera();
	data.m_inFirstPersonIdle		= IsClone() ? m_bInFirstPersonIdle : pPlayerPed->GetMotionData()->GetIsFPSIdle();
	data.m_stickWithinStrafeAngle	= IsClone() ? m_bStickWithinStrafeAngle : pPlayerPed->IsFirstPersonShooterModeStickWithinStrafeAngleThreshold();
	data.m_usingSwimMotionTask		= IsClone() ? m_bUsingSwimMotionTask : pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask);
	data.m_onSlope					= IsClone() ? m_bOnSlope : CTaskHumanLocomotion::IsOnSlopesAndStairs(pPlayerPed, false, fSlopePitch);
#endif
}

void CNetObjPlayer::UpdateBlenderData()
{
#if FPS_MODE_SUPPORTED
    if(m_bUsingFirstPersonCamera)
    {
        GetNetBlender()->SetBlenderData(ms_pedBlenderDataFirstPerson);
    }
    else
#endif // FPS_MODE_SUPPORTED
    {
        CNetObjPed::UpdateBlenderData();
    }
}

bool CNetObjPlayer::IsTargetEntitySuitableForOffsetAiming(CEntity const* entity, Vector3& offset)
{
	// else try and find a target within our gun firing cone (in the camFwd dir)
	float headingAngularLimitRads	= ((15.f * PI) / 180.f);

	// Store owners don't have network objects but are peds but we may want an offset from the object anyway 
	if(entity && (NetworkUtils::GetNetworkObjectFromEntity(entity)))
	{
		Vector3 pos = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());

		Vector3 camPos = camInterface::GetPos();
		Vector3 camFwd = camInterface::GetFront();

		Vector3 camToTarget		= pos - camPos;
		Vector3 camToTargetDir	= camToTarget;
		camToTargetDir.Normalize();

		float angular_dot	= camToTargetDir.Dot(camFwd);
		float angle			= acos(angular_dot);

		// are we within angular range (are we pointing the gun roughly in this peds direction?
		if(angle < headingAngularLimitRads)
		{
			Vector3 finallockOnTargetOffset(VEC3_ZERO);

			if(GenerateTargetForOffsetAiming(entity, finallockOnTargetOffset))
			{
				bool inRange = ((finallockOnTargetOffset.x > -CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET) && (finallockOnTargetOffset.x < CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET) &&
								(finallockOnTargetOffset.y > -CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET) && (finallockOnTargetOffset.y < CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET) &&
									(finallockOnTargetOffset.z > -CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET) && (finallockOnTargetOffset.z < CPlayerCameraDataNode::MAX_LOCKON_TARGET_OFFSET));
				if(inRange)
				{
					// we've found a ped that we can use for aiming at....
					offset = finallockOnTargetOffset;
					return true;
				}
			}
		}
	}

	return false;
}

bool CNetObjPlayer::GenerateTargetForOffsetAiming(CEntity const* entity, Vector3& offset)
{
	if(entity)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());

		Vector3 camPos = camInterface::GetPos();
		Vector3 camFwd = camInterface::GetFront();

		Vector3 camToTarget		= pos - camPos;
		Vector3 camToTargetDir	= camToTarget;
		camToTargetDir.Normalize();

		// where are we aiming at in relation to this ped?
		float offsetlen_dot = camToTarget.Dot(camFwd);
		Vector3 camToTargetOffsetPos	= (camFwd * offsetlen_dot);
		Vector3 targetOffsetPos			= camPos + camToTargetOffsetPos;

		offset	= targetOffsetPos - pos;

		return true;
	}

	return false;
}

void CNetObjPlayer::GetPlayerPedGroupData(CPlayerPedGroupDataNode& data)
{
    gnetAssert(GetPlayerPed());

    if (GetPlayerPed()->GetPedsGroup())
    {
        data.m_pedGroup.CopyNetworkInfo(*GetPlayerPed()->GetPedsGroup(), false);
    }
	else
	{
		data.m_pedGroup.Flush(false, false);
	}
}

void CNetObjPlayer::GetPlayerWantedAndLOSData(CPlayerWantedAndLOSDataNode& data)
{
    CPed *pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);

    const CWanted& wanted = pPlayerPed->GetPlayerInfo()->GetWanted();

    data.m_wantedLevel = wanted.m_WantedLevel;
	data.m_WantedLevelBeforeParole = wanted.m_WantedLevelBeforeParole;
	data.m_bIsOutsideCircle = wanted.m_bIsOutsideCircle;
    data.m_lastSpottedLocation = wanted.m_LastSpottedByPolice;
    data.m_searchAreaCentre = wanted.m_CurrentSearchAreaCentre;
    data.m_timeLastSpotted = wanted.m_iTimeSearchLastRefocused;
	data.m_HasLeftInitialSearchArea = wanted.m_HasLeftInitialSearchArea;
	data.m_copsAreSearching = wanted.CopsAreSearching();
	data.m_timeFirstSpotted = wanted.m_iTimeFirstSpotted;

	if (pPlayerPed->GetNetworkObject() && pPlayerPed->GetNetworkObject()->GetPhysicalPlayerIndex() == wanted.m_CausedByPlayerPhysicalIndex)
	{
		data.m_causedByThisPlayer = true;
		data.m_causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX;
	}
	else
	{
		data.m_causedByThisPlayer = false;		
		data.m_causedByPlayerPhysicalIndex = wanted.m_CausedByPlayerPhysicalIndex;
	}	

	data.m_fakeWantedLevel = CScriptHud::iFakeWantedLevel > 0 ? (u8) CScriptHud::iFakeWantedLevel : 0;
	m_remoteFakeWantedLevel = data.m_fakeWantedLevel; //cache it locally too

    data.m_visiblePlayers = 0;

	static const u32 uAllowedDelayVisibleRemotePlayers = 150;

	bool bProcessVisiblePlayers = !pPlayerPed->IsDead(); 
	if (bProcessVisiblePlayers)
	{
		for (u8 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
		{
			CNetGamePlayer* pNetGamePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(i);
			if (pNetGamePlayer && pNetGamePlayer->IsActive() && !pNetGamePlayer->IsLocal() && pNetGamePlayer->GetPlayerInfo())
			{
				//Note: Not many data structures are reliable on the remote wanted structure, but the time since last spotted should be.
				//		We are using this time to approximate whether the player is 'visible' on this machine.  Then, on the local player's
				//		machine, we run through the 'spotted' code to ensure the correct wanted logic is used.

				CWanted& remotePlayerWanted = pNetGamePlayer->GetPlayerInfo()->GetWanted();
				if ((remotePlayerWanted.GetWantedLevel() > WANTED_CLEAN) && remotePlayerWanted.HasBeenSpotted() &&
					(remotePlayerWanted.GetTimeSinceLastSpotted() < uAllowedDelayVisibleRemotePlayers))
				{
					 data.m_visiblePlayers |= 1 << i;
				}
			}
		}
	}
}

void CNetObjPlayer::GetAmbientModelStreamingData(CPlayerAmbientModelStreamingNode& data)
{
    data.m_AllowedPedModelStartOffset     = gPopStreaming.GetAllowedNetworkPedModelStartOffset();
    data.m_AllowedVehicleModelStartOffset = gPopStreaming.GetAllowedNetworkVehicleModelStartOffset();
    data.m_TargetVehicleForAnimStreaming  = m_TargetVehicleIDForAnimStreaming;
    data.m_TargetVehicleEntryPoint        = m_TargetVehicleEntryPoint;
}

void CNetObjPlayer::GetPlayerGamerData(CPlayerGamerDataNode& data)
{
	const CNetGamePlayer* pPlayer = GetPlayerOwner();
	if (gnetVerify(pPlayer))
	{
		data.m_clanMembership = pPlayer->GetClanMembershipInfo();
		data.m_bIsRockstarDev = pPlayer->IsRockstarDev();
		data.m_bIsRockstarQA = pPlayer->IsRockstarQA();
		data.m_bIsCheater = pPlayer->IsCheater(); 
        data.m_nMatchMakingGroup = static_cast<unsigned>(pPlayer->GetMatchmakingGroup());

        // setup communication Privileges
        data.m_hasCommunicationPrivileges = CLiveManager::CheckVoiceCommunicationPrivileges();

		data.m_bHasStartedTransition = pPlayer->HasStartedTransition();
		data.m_bHasTransitionInfo = false;
		if(data.m_bHasStartedTransition)
		{
			const rlSessionInfo& hInfo = pPlayer->GetTransitionSessionInfo();
			if(hInfo.IsValid())
			{
				hInfo.ToString(data.m_aInfoBuffer);
				data.m_bHasTransitionInfo = true;
			}
		}

		pPlayer->GetMuteData(data.m_muteCount, data.m_muteTotalTalkersCount);

		safecpy(data.m_crewRankTitle, pPlayer->GetCrewRankTitle());

		data.m_kickVotes = pPlayer->GetKickVotes();

		data.m_playerAccountId = pPlayer->GetPlayerAccountId();
	}
}

void CNetObjPlayer::GetPlayerExtendedGameStateData(CPlayerExtendedGameStateNode& data)
{
	data.m_WaypointLocalDirtyTimestamp = CMiniMap::GetWaypointLocalDirtyTimestamp();
	data.m_bHasActiveWaypoint = m_bHasActiveWaypoint = CMiniMap::IsWaypointActive();
	if(data.m_bHasActiveWaypoint)
	{
		data.m_bOwnsWaypoint = m_bOwnsWaypoint = CMiniMap::IsWaypointLocallyOwned();

		sWaypointStruct vCurrentWaypoint;
		CMiniMap::GetActiveWaypoint(vCurrentWaypoint);

		// we clamp the waypoint position to the world limits
		data.m_fxWaypoint = m_fxWaypoint = Clamp(vCurrentWaypoint.vPos.x, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX);
		data.m_fyWaypoint = m_fyWaypoint = Clamp(vCurrentWaypoint.vPos.y, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX);
		data.m_WaypointObjectId = m_WaypointObjectId = vCurrentWaypoint.ObjectId;
	}
	else
	{
		data.m_bOwnsWaypoint = false;
	}

	grcViewport *pViewport = 0;

	if (IsClone())
		pViewport = GetViewport();
	else
		pViewport = &gVpMan.GetGameViewport()->GetNonConstGrcViewport();

	gnetAssert(pViewport);

	float fov = pViewport->GetFOVY();
	fov       = Clamp(fov, MIN_FOV, MAX_FOV);
	data.m_fovRatio = (fov / MAX_FOV);
	data.m_aspectRatio = pViewport->GetAspectRatio();

    data.m_CityDensity = CSettingsManager::GetInstance().GetSettings().m_graphics.m_CityDensity;

	data.m_ghostPlayers = m_ghostPlayerFlags;

	data.m_MaxExplosionDamage = GetPlayerPed()->GetPlayerInfo()->GetPlayerMaxExplosiveDamage();
}

// sync parser setter functions
void CNetObjPlayer::SetPedHealthData(const CPedHealthDataNode& data)
{
	CNetObjPed::SetPedHealthData(data);
	
	SetLastReceivedHealth(data.m_hasMaxHealth ? static_cast<u32>(GetPed()->GetMaxHealth()) : data.m_health, GetNetBlender() ? GetNetBlender()->GetLastSyncMessageTime() : 0);

	weaponDebugf3("## GOT PLAYER HEALTH UPDATE - health = %d. Timestamp = %d. Last received health = %d ##", data.m_health, GetNetBlender() ? GetNetBlender()->GetLastSyncMessageTime() : 0, m_lastReceivedHealth);
}

void CNetObjPlayer::SetTaskTreeData(const CPedTaskTreeDataNode& data)
{
	CNetObjPed::SetTaskTreeData(data);

	m_bIgnoreTaskSpecificData = false;
}

void CNetObjPlayer::SetTaskSpecificData(const CPedTaskSpecificDataNode& data)
{
	if (m_bIgnoreTaskSpecificData)
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, m_bIgnoreTaskSpecificData set");	
	}
	else
	{
		CNetObjPed::SetTaskSpecificData(data);
	}
}

void CNetObjPlayer::SetPlayerSectorPosData(const CPlayerSectorPosNode& data)
{
	CPed *pPlayerPed = GetPlayerPed();
	gnetAssert(pPlayerPed);

    SetSectorPosition(data.m_SectorPosX, data.m_SectorPosY, data.m_SectorPosZ);

	if (pPlayerPed && pPlayerPed->GetPlayerInfo())
	{
		pPlayerPed->GetPlayerInfo()->ReportStealthNoise(data.m_StealthNoise);
	}

    CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

    if(netBlenderPed)
    {
        netBlenderPed->SetIsRagdolling(data.m_IsRagdolling);
        netBlenderPed->SetIsOnStairs(data.m_IsOnStairs);
        netBlenderPed->UpdateStandingData(data.m_StandingOnNetworkObjectID, data.m_LocalOffset, netBlenderPed->GetLastSyncMessageTime());
    }
}

void SetDecoratorList(const CPlayerGameStateDataNode& data, fwExtensibleBase* pBase)
{
	// Decorator sync
	if( pBase )
	{
		fwDecoratorExtension::RemoveAllFrom((*pBase));
		if( data.m_decoratorListCount > 0 )
		{
			for(u32 decor = 0; decor < data.m_decoratorListCount; decor++)
			{
				atHashWithStringBank hash = data.m_decoratorList[decor].Key;
				atHashWithStringBank String;
				switch( data.m_decoratorList[decor].Type ) 
				{
				case fwDecorator::TYPE_FLOAT:
					fwDecoratorExtension::Set((*pBase), hash, data.m_decoratorList[decor].Float);
					break;
	
				case fwDecorator::TYPE_BOOL:
					fwDecoratorExtension::Set((*pBase), hash, data.m_decoratorList[decor].Bool);
					break;
	
				case fwDecorator::TYPE_INT:
					fwDecoratorExtension::Set((*pBase), hash, data.m_decoratorList[decor].Int);
					break;
	
				case fwDecorator::TYPE_STRING:
					String = data.m_decoratorList[decor].String;
					fwDecoratorExtension::Set((*pBase), hash, String);
					break;
	
				case fwDecorator::TYPE_TIME:
					{
						fwDecorator::Time temp = (fwDecorator::Time)data.m_decoratorList[decor].Int;
						fwDecoratorExtension::Set((*pBase), hash, temp);
					}
					break;
	
				default:
					gnetAssertf(false, "Unknown decorator type");
					break;
	
				}
			}
		}
	}
}

void CNetObjPlayer::SetPlayerGameStateData(const CPlayerGameStateDataNode& data)
{
    CPed* pPlayerPed = GetPlayerPed();
	if (!AssertVerify(pPlayerPed))
		return;

    if (data.m_GameStateFlags.isSpectating)
    {
        m_SpectatorObjectId = data.m_SpectatorId;
    }
    else
    {
        m_SpectatorObjectId = NETWORK_INVALID_OBJECT_ID;
    }
	GetPlayerOwner()->SetSpectator(data.m_bIsSCTVSpectating); // SCTV spectator mode

    if (data.m_GameStateFlags.isAntagonisticToPlayer)
    {
        gnetAssertf(data.m_AntagonisticPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Invalid index \"%d\"", data.m_AntagonisticPlayerIndex);
        m_AntagonisticPlayerIndex = data.m_AntagonisticPlayerIndex;
    }
    else
    {
        m_AntagonisticPlayerIndex = INVALID_PLAYER_INDEX;
    }

	if (!AssertVerify(pPlayerPed && pPlayerPed->GetPlayerInfo()))
		return;

	const int previousState = (int)pPlayerPed->GetPlayerInfo()->GetPlayerState();

	//Player state is synced +1 to reduce bandwidth
    pPlayerPed->GetPlayerInfo()->SetPlayerState(static_cast<CPlayerInfo::State>(data.m_PlayerState - 1));

	//Check if clone has to be faded out
	CheckAndStartClonePlayerFadeOut(previousState, data.m_PlayerState - 1);

    if(data.m_GameStateFlags.controlsDisabledByScript)
    {
        pPlayerPed->GetPlayerInfo()->SetPlayerControl(false, true, 10.0f, true, false);
    }
    else
    {
        bool needsPositionCorrection = ShouldFixAndDisableCollisionDueToControls();

        pPlayerPed->GetPlayerInfo()->SetPlayerControl(false, false, 10.0f, true, false);

        if(GetNetBlender() && needsPositionCorrection && CanBlend())
        {
            GetNetBlender()->GoStraightToTarget();
        }
    }

	if (pPlayerPed->GetPhoneComponent())
	{
		if (data.m_MobileRingState == CPedPhoneComponent::DEFAULT_RING_STATE)
		{
			pPlayerPed->GetPhoneComponent()->StopMobileRinging();
		}
		else if (pPlayerPed->GetPhoneComponent()->GetMobileRingState() != data.m_MobileRingState)
		{
			pPlayerPed->GetPhoneComponent()->StartMobileRinging(data.m_MobileRingState);
		}
	}

    pPlayerPed->GetPlayerInfo()->m_fForceAirDragMult = data.m_AirDragMult;

	if (!AssertVerify(GetPlayerOwner()))
		return;

    GetPlayerOwner()->SetTeam(data.m_PlayerTeam);

    if (data.m_GameStateFlags.newMaxHealthArmour)
    {
        pPlayerPed->SetMaxHealth(static_cast<float>(data.m_MaxHealth));
        pPlayerPed->GetPlayerInfo()->MaxArmour = static_cast<u16>(data.m_MaxArmour);
    }
    else
    {
        pPlayerPed->SetMaxHealth(static_cast<float>(CPlayerInfo::PLAYER_DEFAULT_MAX_HEALTH));
        pPlayerPed->GetPlayerInfo()->MaxArmour = static_cast<u16>(CPlayerInfo::PLAYER_DEFAULT_MAX_ARMOUR);
    }

	m_bBattleAware = data.m_bBattleAware;

	//Ensure players that have maximum health get their remote health set when it is maxxed out.
	//Prior to this the remote would not get the SetPedHealthData in NetObjPed because it was not sent because the
	//ped had maximum health initially.  So the remote SetHealth for players that had altered Max Health wasn't set,
	//and therefore appeared lower.  This change allows the remote health of players to be set when the local authority
	//has maximum health that has been set above to a value. lavalley.
	if (data.m_GameStateFlags.bHasMaxHealth)
	{
		pPlayerPed->SetHealth(pPlayerPed->GetMaxHealth(),true);

		SetLastReceivedHealth((u32)pPlayerPed->GetMaxHealth(), GetNetBlender() ? GetNetBlender()->GetLastSyncMessageTime() : 0);

		weaponDebugf3("## GOT PLAYER MAX HEALTH UPDATE - health = %d. Timestamp = %d. Last received health = %d ##", (u32)pPlayerPed->GetMaxHealth(), GetNetBlender() ? GetNetBlender()->GetLastSyncMessageTime() : 0, m_lastReceivedHealth);
	}

    m_MyVehicleIsMyInteresting = data.m_GameStateFlags.myVehicleIsMyInteresting;
	pPlayerPed->m_PedConfigFlags.SetCantBeKnockedOffVehicle(data.m_GameStateFlags.cantBeKnockedOffBike);

	if (data.m_GameStateFlags.hasSetJackSpeed)
	{
		pPlayerPed->GetPlayerInfo()->JackSpeed = static_cast<u16>(data.m_JackSpeed);
	}
	else
	{
		pPlayerPed->GetPlayerInfo()->JackSpeed = static_cast<u16>(CPlayerInfo::NETWORK_PLAYER_DEFAULT_JACK_SPEED);
	}

    pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_NeverEverTargetThisPed, data.m_GameStateFlags.neverTarget );

    m_UseKinematicPhysics = data.m_GameStateFlags.useKinematicPhysics;

    if((m_tutorialIndex      != data.m_TutorialIndex) ||
	   (m_tutorialInstanceID != data.m_TutorialInstanceID))
    {
		// remove all local sticky bombs attached to this player
		CProjectileManager::RemoveAllStickyBombsAttachedToEntity(pPlayerPed);

#if ENABLE_NETWORK_LOGGING
		if(data.m_TutorialIndex == INVALID_TUTORIAL_INDEX)
        {
            NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "CLONE_ENDING_TUTORIAL", GetLogName());
        }
        else if(data.m_TutorialIndex == SOLO_TUTORIAL_INDEX)
        {
            NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "CLONE_STARTING_SOLO_TUTORIAL", GetLogName());
        }
        else
        {
            NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "CLONE_STARTING_GANG_TUTORIAL", GetLogName());
            NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Team",        "%d", data.m_TutorialIndex);
            NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Instance ID", "%d", data.m_TutorialInstanceID);
        }
#endif // ENABLE_NETWORK_LOGGING   
	}

    bool wasInDifferentTutorialSession = GetPlayerOwner() && GetPlayerOwner()->IsInDifferentTutorialSession();

    if (data.m_GameStateFlags.inTutorial)
    {
		m_tutorialIndex      = data.m_TutorialIndex;
		m_tutorialInstanceID = data.m_TutorialInstanceID;
		m_pendingTutorialIndex      = data.m_TutorialIndex;
		m_pendingTutorialInstanceID = data.m_TutorialInstanceID;
    }
    else
    {
        m_tutorialIndex      = INVALID_TUTORIAL_INDEX;
        m_tutorialInstanceID = INVALID_TUTORIAL_INSTANCE_ID;
		m_pendingTutorialIndex      = INVALID_TUTORIAL_INDEX;
		m_pendingTutorialInstanceID = INVALID_TUTORIAL_INSTANCE_ID;
    }

	m_bPendingTutorialSessionChange = data.m_GameStateFlags.pendingTutorialSessionChange;

    UpdateCachedTutorialSessionState();

    // if this player was in a tutorial session, but now isn't, we need to force them to the correct position
    bool isInDifferentTutorialSession = GetPlayerOwner() && GetPlayerOwner()->IsInDifferentTutorialSession();

    if(!isInDifferentTutorialSession && wasInDifferentTutorialSession)
    {
        if(GetNetBlender())
        {
            GetNetBlender()->GoStraightToTarget();
        }

        pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
        pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); 
        pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
        pPlayerPed->ClearWetClothing();

		// resend all sticky bombs when the player re-enters our tutorial
		CStickyBombsArrayHandler* pStickyBombsHandler = NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler();

		if (pStickyBombsHandler && GetPlayerOwner())
		{
			pStickyBombsHandler->ResendAllStickyBombsToPlayer(*GetPlayerOwner());
		}
    }

	if(m_ConcealedOnOwner && !data.m_ConcealedOnOwner)
	{
		gnetDebug2("Player %s un-concealed themselves locally", pPlayerPed->GetLogName());
		pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
		pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); 
		pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		pPlayerPed->ClearWetClothing();
	}
#if __BANK
	if(!m_ConcealedOnOwner && data.m_ConcealedOnOwner)
	{
		gnetDebug2("Player %s concealed themselves locally", pPlayerPed->GetLogName());
	}
#endif // __BANK
	m_ConcealedOnOwner = data.m_ConcealedOnOwner;

    NetworkInterface::GetVoice().ApplyReceiveBitfield(GetPhysicalPlayerIndex(), data.m_OverrideReceiveChat);
	NetworkInterface::GetVoice().ApplySendBitfield(GetPhysicalPlayerIndex(), data.m_OverrideSendChat);
    NetworkInterface::GetVoice().SetOverrideTutorialRestrictions(GetPhysicalPlayerIndex(), data.m_bOverrideTutorialChat);
    NetworkInterface::GetVoice().SetOverrideTransitionRestrictions(GetPhysicalPlayerIndex(), data.m_bOverrideTransitionChat);

	if (data.m_GameStateFlags.respawning)
	{
		respawnDebugf3("data.m_GameStateFlags.respawning m_respawnInvincibilityTimer[%d]",m_respawnInvincibilityTimer);
		if (m_respawnInvincibilityTimer == 0)
		{
			// set to non-zero so it gets incremented on the clone
			respawnDebugf3("data.m_GameStateFlags.respawning set m_respawnInvincibilityTimer = 1");
			m_respawnInvincibilityTimer = 1;

			//ensure this is set to false on receipt, making sure that the remote will count up by default
			m_bCloneRespawnInvincibilityTimerCountdown = false; //reset clone countdown to false
		}
	}
	else if ((m_respawnInvincibilityTimer != 0) && (!m_bCloneRespawnInvincibilityTimerCountdown))
	{
		respawnDebugf3("!data.m_GameStateFlags.respawning reset m_respawnInvincibilityTimer");
		m_respawnInvincibilityTimer = 0;
	}

	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillJackAnyPlayer, data.m_GameStateFlags.willJackAnyPlayer);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillJackWantedPlayersRatherThanStealCar, data.m_GameStateFlags.willJackWantedPlayersRatherThanStealCar);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontDragMeOutCar,  data.m_GameStateFlags.dontDragMeOutOfCar);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar,  data.m_GameStateFlags.playersDontDragMeOutOfCar);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HelmetHasBeenShot,  data.m_GameStateFlags.bHelmetHasBeenShot);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayerIsWeird, data.m_GameStateFlags.playerIsWeird);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_NoCriticalHits, data.m_GameStateFlags.noCriticalHits);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableHomingMissileLockForVehiclePedInside, data.m_GameStateFlags.disableHomingMissileLockForVehiclePedInside);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreMeleeFistWeaponDamageMult, data.m_GameStateFlags.ignoreMeleeFistWeaponDamageMult);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LawPedsCanFleeFromNonWantedPlayer, data.m_GameStateFlags.lawPedsCanFleeFromNonWantedPlayer);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet, data.m_GameStateFlags.hasHelmet);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontTakeOffHelmet, data.m_GameStateFlags.dontTakeOffHelmet);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor, data.m_GameStateFlags.isSwitchingHelmetVisor);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch, data.m_GameStateFlags.forceHelmetVisorSwitch);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee, data.m_GameStateFlags.isPerformingVehicleMelee);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseOverrideFootstepPtFx, data.m_GameStateFlags.useOverrideFootstepPtFx);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableVehicleCombat, data.m_GameStateFlags.disableVehicleCombat);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_TreatAsFriendlyForTargetingAndDamage, data.m_GameStateFlags.treatFriendlyTargettingAndDamage);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseKinematicModeWhenStationary, data.m_GameStateFlags.useKinematicModeWhenStationary);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions, data.m_GameStateFlags.dontActivateRagdollFromExplosions);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_AllowBikeAlternateAnimations, data.m_GameStateFlags.allowBikeAlternateAnimations);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations, data.m_GameStateFlags.useLockpickVehicleEntryAnimations);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayerPreferFrontSeatMP, data.m_GameStateFlags.m_PlayerPreferFrontSeat);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting, data.m_GameStateFlags.ignoreInteriorCheckForSprinting);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact, data.m_GameStateFlags.dontActivateRagdollFromVehicleImpact);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SwatHeliSpawnWithinLastSpottedLocation, data.m_GameStateFlags.swatHeliSpawnWithinLastSpottedLocation);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableStartEngine, data.m_GameStateFlags.disableStartEngine);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LawWillOnlyAttackIfPlayerIsWanted, data.m_GameStateFlags.lawOnlyAttackIfPlayerIsWanted);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableHelmetArmor, data.m_GameStateFlags.disableHelmetArmor);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting, data.m_GameStateFlags.pedIsArresting);
	pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba, data.m_GameStateFlags.isScuba);
	m_PRF_BlockRemotePlayerRecording = data.m_GameStateFlags.PRF_BlockRemotePlayerRecording;
	m_PRF_UseScriptedWeaponFirePosition = data.m_GameStateFlags.PRF_UseScriptedWeaponFirePosition;

    pPlayerPed->m_nPhysicalFlags.bNotDamagedByBullets    = data.m_GameStateFlags.notDamagedByBullets;
    pPlayerPed->m_nPhysicalFlags.bNotDamagedByFlames     = data.m_GameStateFlags.notDamagedByFlames;
    pPlayerPed->m_nPhysicalFlags.bIgnoresExplosions      = data.m_GameStateFlags.ignoresExplosions;
    pPlayerPed->m_nPhysicalFlags.bNotDamagedByCollisions = data.m_GameStateFlags.notDamagedByCollisions;
    pPlayerPed->m_nPhysicalFlags.bNotDamagedByMelee      = data.m_GameStateFlags.notDamagedByMelee;
    pPlayerPed->m_nPhysicalFlags.bNotDamagedBySteam      = data.m_GameStateFlags.notDamagedBySteam;
    pPlayerPed->m_nPhysicalFlags.bNotDamagedBySmoke      = data.m_GameStateFlags.notDamagedBySmoke;

	if (!AssertVerify(pPlayerPed->GetPlayerWanted()))
		return;

    pPlayerPed->GetPlayerWanted()->m_AllRandomsFlee		= data.m_GameStateFlags.randomPedsFlee;
	pPlayerPed->GetPlayerWanted()->m_EverybodyBackOff	= data.m_GameStateFlags.everybodyBackOff;

    m_isTargettableByTeam   = data.m_IsTargettableByTeam;

	// voice loudness
	if(data.m_GameStateFlags.bHasMicrophone)
		m_fVoiceLoudness = data.m_fVoiceLoudness;
	else
		m_fVoiceLoudness = 0.0f;

	// voice channel
	GetPlayerOwner()->SetVoiceChannel(data.m_nVoiceChannel);

    if(data.m_bHasVoiceProximityOverride)
        GetPlayerOwner()->ApplyVoiceProximityOverride(data.m_vVoiceProximityOverride);
    else
        GetPlayerOwner()->ClearVoiceProximityOverride();

	if (data.m_bHasScriptedWeaponFirePos)
		pPlayerPed->GetPlayerInfo()->SetScriptedWeaponFiringPos(VECTOR3_TO_VEC3V(data.m_ScriptedWeaponFirePos));
	else
		pPlayerPed->GetPlayerInfo()->ClearScriptedWeaponFiringPos();

	GetPlayerOwner()->SetSizeOfNetArrayData(data.m_sizeOfNetArrayData);

	pPlayerPed->m_nPhysicalFlags.bNotDamagedByAnything = data.m_GameStateFlags.bInvincible;
	
#if !__FINAL
	m_bDebugInvincible = data.m_GameStateFlags.bDebugInvincible;
#endif

	if (pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() != data.m_vehicleweaponindex)
	{
		m_pendingVehicleWeaponIndex = data.m_vehicleweaponindex;
		if(!m_weaponObjectExists || m_inVehicle)
		{
			if(m_pendingVehicleWeaponIndex != -1 || m_lastSyncedWeaponHash != 0)
			{
				bool saveCanAlter = m_CanAlterPedInventoryData;
				m_CanAlterPedInventoryData = true;
				bool hasApplied = pPlayerPed->GetWeaponManager()->EquipWeapon(m_pendingVehicleWeaponIndex == -1 ? m_lastSyncedWeaponHash : 0, m_pendingVehicleWeaponIndex);
				m_HasToApplyWeaponToVehicle = !hasApplied;
				m_CanAlterPedInventoryData = saveCanAlter;
			}
		}
	}

	// Decorator sync
	fwExtensibleBase* pBase = (fwExtensibleBase*)pPlayerPed->GetPlayerInfo();
	SetDecoratorList(data,pBase);

    CNetGamePlayer *netGamePlayer = SafeCast(CNetGamePlayer, GetPlayerOwner());

    netGamePlayer->SetGarageInstanceIndex(data.m_GarageInstanceIndex);
	netGamePlayer->SetPropertyID(data.m_nPropertyID);
	netGamePlayer->SetMentalState(data.m_nMentalState);
	WIN32PC_ONLY(netGamePlayer->SetPedDensity(data.m_nPedDensity);)

	SetFriendlyFireAllowed(data.m_bIsFriendlyFireAllowed);
	SetPassiveMode(data.m_bIsPassiveMode);

    m_VehicleJumpDown = data.m_VehicleJumpDown;


	float defenceModifier = data.m_WeaponDefenseModifier;
	if(defenceModifier < 0.1f)
	{
		defenceModifier = 0.1f;
	}
	pPlayerPed->GetPlayerInfo()->SetPlayerWeaponDefenseModifier(defenceModifier);

	pPlayerPed->GetPlayerInfo()->SetPlayerWeaponMinigunDefenseModifier(data.m_WeaponMinigunDefenseModifier);

	m_bUseExtendedPopulationRange = data.m_bUseExtendedPopulationRange;
	if(m_bUseExtendedPopulationRange)
	{
		m_vExtendedPopulationRangeCenter = data.m_vExtendedPopulationRangeCenter;
	}

    netGamePlayer->SetCharacterRank(data.m_nCharacterRank);

	m_bDisableLeavePedBehind = data.m_bDisableLeavePedBehind;

	if (data.m_bCollisionsDisabledByScript)
	{
		bool  bWasDisabled = pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision);

		pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasDisabledCollision, true);
		pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision, true);

		if (!bWasDisabled && !pPlayerPed->IsCollisionEnabled())
		{
			// have to call disable again here, so that it is completely disabled
			DisableCollision();
		}
	}
	else
	{
		pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasDisabledCollision, false);
		pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision, false);
	}

	SetIsInCutsceneRemotely(data.m_bInCutscene);
	SetAsGhost(data.m_bGhost BANK_ONLY(, SPG_PLAYER_STATE));

	m_LockOnTargetID = data.m_LockOnTargetID;
	m_LockOnState    = data.m_LockOnState;
	m_vehicleShareMultiplier = data.m_VehicleShareMultiplier;
	pPlayerPed->GetPlayerInfo()->SetPlayerWeaponDamageModifier(data.m_WeaponDamageModifier);
	pPlayerPed->GetPlayerInfo()->SetPlayerMeleeWeaponDamageModifier(data.m_MeleeDamageModifier);
	pPlayerPed->GetPlayerInfo()->SetPlayerMeleeUnarmedDamageModifier(data.m_MeleeUnarmedDamageModifier);

	UpdateLockOnTarget();

    m_PRF_IsSuperJumpEnabled = data.m_bIsSuperJump;
	pPlayerPed->SetEnableCrewEmblem(data.m_EnableCrewEmblem);

	pPlayerPed->GetPlayerInfo()->AccessArcadeInformation().SetTeamAndRole(eArcadeTeam(data.m_arcadeTeamInt), eArcadeRole(data.m_arcadeRoleInt));
	pPlayerPed->GetPlayerInfo()->AccessArcadeAbilityEffects().SetArcadePassiveAbilityEffectsFlags(data.m_arcadePassiveAbilityFlags);
	pPlayerPed->GetPlayerInfo()->AccessArcadeInformation().SetCNCVOffender(data.m_arcadeCNCVOffender);

	m_PRF_IsShockedFromDOTEffect = data.m_bIsShockedFromDOTEffect;
	m_PRF_IsChokingFromDOTEffect = data.m_bIsChokingFromDOTEffect;
}

void CNetObjPlayer::SetPlayerAppearanceData(CPlayerAppearanceDataNode& data)
{
    CPed* pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);
	if (!pPlayerPed)
		return;

	pedDebugf3("CNetObjPlayer::SetPlayerAppearanceData pPed[%p][%s] player[%s] isplayer[%d]",pPlayerPed,pPlayerPed->GetModelName(), GetPlayerOwner() ? GetPlayerOwner()->GetLogName() : "",pPlayerPed->IsPlayer());

    // check if the model index has changed
    fwModelId modelId;
    CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(data.m_NewModelHash, &modelId);
	if(modelInfo && modelInfo->GetModelType() != MI_TYPE_PED)
	{
		gnetAssertf(0, "%s trying to apply non ped model type", GetLogName());
		return;
	}
    const u32 newModelIndex = modelId.GetModelIndex();
    const u32 modelIndex    = pPlayerPed->GetModelIndex();

    if (gnetVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "SetPlayerAppearanceData: New player model not loaded!!"))
    {
        if (IsClone())
        {
            m_RespawnNetObjId = data.m_RespawnNetObjId;
        }

        //Player model has changed
        if(newModelIndex != modelIndex)
        {
            ChangePlayerModel(newModelIndex);

            //We need to regrab the player ped pointer as it may have changed
            pPlayerPed = GetPlayerPed();
            gnetAssert(pPlayerPed);
			if (!pPlayerPed)
				return;

			gnetAssert(pPlayerPed->GetSpeechAudioEntity());
			if (pPlayerPed->GetSpeechAudioEntity())
	            pPlayerPed->GetSpeechAudioEntity()->SetAmbientVoiceName(data.m_VoiceHash);
        }

        NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManager().GetLog(), GetPhysicalPlayerIndex(), "SET_PLAYER_APPEARANCE", GetLogName());
        NOTFINAL_ONLY(NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Model", "%s", CModelInfo::GetBaseModelInfoName(pPlayerPed->GetModelId()));)

        CNetObjPed::SetPedAppearanceData(data.m_PedProps, data.m_VariationData, data.m_phoneMode, data.m_parachuteTintIndex, data.m_parachutePackTintIndex, data.m_facialClipSetId, data.m_facialIdleAnimOverrideClipNameHash, data.m_facialIdleAnimOverrideClipDictNameHash, data.m_isAttachingHelmet, data.m_isRemovingHelmet, data.m_isWearingHelmet, data.m_helmetTextureId, data.m_helmetProp, data.m_visorPropUp, data.m_visorPropDown, data.m_visorIsUp, data.m_supportsVisor, data.m_isVisorSwitching, data.m_targetVisorState, newModelIndex != modelIndex);
		SetSecondaryPartialAnimData(data.m_secondaryPartialAnim);
		
		CPedVariationData& pedVarData = pPlayerPed->GetPedDrawHandler().GetVarData();
		const u32 prevTxd = pedVarData.GetOverrideCrewLogoTxdHash();
		const u32 prevTex = pedVarData.GetOverrideCrewLogoTexHash();

		pedVarData.SetOverrideCrewLogoTxdHash(data.m_crewLogoTxdHash);
		pedVarData.SetOverrideCrewLogoTexHash(data.m_crewLogoTexHash);

		// url:bugstar:3042656 - If the user has UGC restrictions we need to refresh as otherwise the rendering won't be re-enabled
		// due to the added check in RequestStreamPedFilesInternal (look for CheckUserContentPrivileges)
		if ((prevTxd != pedVarData.GetOverrideCrewLogoTxdHash() || prevTex != pedVarData.GetOverrideCrewLogoTexHash())
			&& !CLiveManager::CheckUserContentPrivileges())
		{
			CPedVariationStream::RequestStreamPedFiles(pPlayerPed, &pedVarData);
		}
		
        // apply head settings
		if (data.m_HasHeadBlendData && !ApplyHeadBlendData(*pPlayerPed, data.m_HeadBlendData))
		{
			if (data.m_HeadBlendData.m_usePaletteRgb)
			{
				for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
				{
					m_remoteColor[i].Set(data.m_HeadBlendData.m_paletteRed[i], data.m_HeadBlendData.m_paletteGreen[i], data.m_HeadBlendData.m_paletteBlue[i]);
				}

				m_bRemoteReapplyColors = true;
			}
        }

        // apply ped decorations
        PedDecorationManager &decorationMgr = PedDecorationManager::GetInstance();

        if(!data.m_HasDecorations)
        {
            if(decorationMgr.GetPedDecorationCount(pPlayerPed) > 0)
            {
                decorationMgr.ClearCompressedPedDecorations(pPlayerPed);                
            }
        }
        else
        {
            decorationMgr.ProcessDecorationsBitfieldForRemotePed(pPlayerPed, data.m_PackedDecorations, NUM_DECORATION_BITFIELDS, data.m_crewEmblemVariation);
        }


        const atArray<CPedBloodNetworkData> *bloodDataArray = 0;
        PEDDAMAGEMANAGER.GetBloodDataForLocalPed(pPlayerPed, bloodDataArray);

		if(bloodDataArray)
		{
			for(int index = 0; index < bloodDataArray->GetCount(); index++)
			{
				const CPedBloodNetworkData &bloodData = (*bloodDataArray)[index];

				PEDDAMAGEMANAGER.AddPedBlood(pPlayerPed, bloodData.zone, bloodData.uvPos, bloodData.rotation, bloodData.scale, bloodData.bloodNameHash, false, bloodData.age);
			}			
		}

		if (data.m_networkedDamagePack && (data.m_networkedDamagePack != m_networkedDamagePack))
		{
			PEDDAMAGEMANAGER.AddPedDamagePack(pPlayerPed, data.m_networkedDamagePack, 0.0f, 1.0f);
		}
		m_networkedDamagePack = data.m_networkedDamagePack;
    }

	pedDebugf3("CNetObjPlayer::SetPlayerAppearanceData--complete");
}

void CNetObjPlayer::SetCameraData(const CPlayerCameraDataNode& data)
{
	m_pendingCameraTargetData.CopyPlayerCameraDataNode(data);
	m_pendingCameraDataToSet	= true;
	m_pendingTargetDataToSet	= true;

	m_canOwnerMoveWhileAiming		= data.m_canOwnerMoveWhileAiming;

	// apply look at position if provided
	m_bIsCloneLooking = data.m_isLooking;
	if(data.m_isLooking)
		m_vLookAtPosition = data.m_LookAtPosition;

    m_UsingLeftTriggerAimMode = data.m_UsingLeftTriggerAimMode;

#if FPS_MODE_SUPPORTED
    bool wasUsingFirstPerson    = m_bUsingFirstPersonCamera;
	m_bUsingFirstPersonCamera	= data.m_usingFirstPersonCamera;
	m_bUsingFirstPersonVehicleCamera = data.m_usingFirstPersonVehicleCamera;
	m_bInFirstPersonIdle		= data.m_inFirstPersonIdle;
	m_bStickWithinStrafeAngle	= data.m_stickWithinStrafeAngle;
	m_bUsingSwimMotionTask		= data.m_usingSwimMotionTask;
	m_bOnSlope					= data.m_onSlope;

    // update the blender data if the camera mode has changed
    if(wasUsingFirstPerson != m_bUsingFirstPersonCamera)
    {
        UpdateBlenderData();
    }

#endif
}

void CNetObjPlayer::UpdatePendingCameraData(void)
{
	//---

	// nothing to set...
	if(!m_pendingCameraDataToSet)
	{
		return;
	}

    if(!GetPlayerOwner())
    {
        return;
    }

	// if we're not the right update group for this frame...
	PhysicalPlayerIndex ppi = GetPlayerOwner()->GetPhysicalPlayerIndex();
	if( ! ((int)( fwTimer::GetSystemFrameCount() % 4 ) == (int)(ppi % 4)) )
	{
		return;
	}
	
	m_pendingCameraDataToSet = false;

	//---

    CPed* pPlayerPed = GetPlayerPed();

	grcViewport *pViewport = IsClone() ? GetViewport() : &gVpMan.GetGameViewport()->GetNonConstGrcViewport();
    gnetAssert(pViewport);

    Vector3 eulers(m_pendingCameraTargetData.m_eulersX, 0.0f, m_pendingCameraTargetData.m_eulersZ);
    Matrix34 camM;
    camM.FromEulersXYZ(eulers);

	m_UsingFreeCam = m_pendingCameraTargetData.m_UsingFreeCamera;

    if(m_pendingCameraTargetData.m_UsingFreeCamera)
    {
		m_UsingCinematicVehCam = m_pendingCameraTargetData.m_UsingCinematicVehCamera;
        camM.d = m_pendingCameraTargetData.m_Position;
        m_viewportOffset.ZeroComponents();
    }
    else
    {
		m_UsingCinematicVehCam = false;

		//use player offset unless we've been given a large offset
		if(m_pendingCameraTargetData.m_largeOffset)
		{
			m_viewportOffset = VECTOR3_TO_VEC3V(m_pendingCameraTargetData.m_Position);
		}
		else
		{			
			m_viewportOffset = -pPlayerPed->GetTransform().GetB();
			m_viewportOffset = Add(m_viewportOffset, Vec3V(V_Z_AXIS_WZERO));
		}

        camM.d = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()) + RCC_VECTOR3(m_viewportOffset);
    }

    pViewport->SetCameraMtx(RCC_MAT34V(camM));
}

void CNetObjPlayer::UpdateWeaponTarget()
{
	// Update the pending target data (deals with frame culling inside (i.e. only updating every X'th frame)...
	UpdatePendingTargetData();

	// if we're aiming at an entity, get the latest position...
	UpdateTargettedEntityPosition();

	// Update the aim position with the latest entity pos or just set the aim pos using the positon set by UpdatePendingTargetData...
	UpdateWeaponAimPosition();	
}

void CNetObjPlayer::UpdatePendingTargetData()
{
	//---

	// nothing to set...
	if(!m_pendingTargetDataToSet)
	{
		return;
	}
	
	m_pendingTargetDataToSet = false;

	//---

	bool wasAiming						= m_TargetCloningInfo.IsAiming();
	rage::ObjectId previousAimTargetID	= m_TargetCloningInfo.GetCurrentAimTargetID();

	m_TargetCloningInfo.Clear();	

	m_TargetCloningInfo.SetWasAiming(wasAiming);

	if(!m_pendingCameraTargetData.m_UsingFreeCamera && m_pendingCameraTargetData.m_aiming)
	{
		CPed* pPlayerPed = GetPlayerPed();

		if (pPlayerPed && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash() != 0)
		{
			gnetAssertf(!pPlayerPed->GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeTorso),"%s is trying to aim with no gun IK Solver enabled. Weapon Hash %d", GetLogName(),pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash());

			if(pPlayerPed->GetWeaponManager())
			{
				m_TargetCloningInfo.SetIsAiming(true);

				m_TargetAimPosition.Zero();

				if(NETWORK_INVALID_OBJECT_ID != m_pendingCameraTargetData.m_targetId)
				{
					CEntity* entity = NULL;

					if(m_pendingCameraTargetData.m_targetId != previousAimTargetID)
					{
						netObject* pObj			= NetworkInterface::GetNetworkObject(m_pendingCameraTargetData.m_targetId);
						entity					= pObj ? pObj->GetEntity() : NULL;

						m_prevTargetEntityCache	= entity;
					}
					else
					{
						entity = m_prevTargetEntityCache;
					}

					if (entity)
					{

						m_TargetAimPosition = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) + m_pendingCameraTargetData.m_lockOnTargetOffset;

						if (m_pendingCameraTargetData.m_bAimTargetEntity)
						{
							m_TargetCloningInfo.SetCurrentAimTargetID(m_pendingCameraTargetData.m_targetId);
							m_TargetCloningInfo.SetLockOnTargetAimOffset(m_pendingCameraTargetData.m_lockOnTargetOffset);

							if (previousAimTargetID != m_TargetCloningInfo.GetCurrentAimTargetID())
							{
								if(entity->GetIsTypePed())
								{
									CPed* pTargetPed = (CPed*) entity;
									if (pTargetPed && !pTargetPed->IsNetworkClone())
									{
										CWeapon* pEquippedWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
										if (pEquippedWeapon && pEquippedWeapon->GetWeaponInfo())
										{
											// Melee events have CEventMeleeAction as their targeting event.
											if (pEquippedWeapon->GetWeaponInfo()->GetIsMelee() || pEquippedWeapon->GetWeaponInfo()->GetIsUnarmed())
											{
												if (pTargetPed->GetPedIntelligence() && pTargetPed->GetPedIntelligence()->GetPedPerception().IsInSeeingRange(pPlayerPed))
												{
													bool bComputeFOVVisibility = pTargetPed->GetPedIntelligence()->GetPedPerception().ComputeFOVVisibility(pPlayerPed,NULL);
													weaponDebugf3("CNetObjPlayer::UpdatePendingTargetData -- weapon[%s] player[%p][%s][%s] target[%p][%s][%s] -- Melee/UnArmed -- IsInSeeingRange -- bComputeFOVVisibility[%d]",pEquippedWeapon->GetWeaponInfo()->GetModelName(),pPlayerPed,pPlayerPed->GetModelName(),pPlayerPed->GetNetworkObject() ? pPlayerPed->GetNetworkObject()->GetLogName() : "",pTargetPed,pTargetPed->GetModelName(),pTargetPed->GetNetworkObject() ? pTargetPed->GetNetworkObject()->GetLogName() : "",bComputeFOVVisibility);
													if (bComputeFOVVisibility)
													{
														CEventMeleeAction eventMeleeAction( pPlayerPed, pTargetPed, true );
														pTargetPed->GetPedIntelligence()->AddEvent(eventMeleeAction);
													}
												}
											}
											// Otherwise use gun aimed at.
											else if(!pEquippedWeapon->GetWeaponInfo()->GetIsNonViolent() )
											{
												if (CEventGunAimedAt::IsAimingInTargetSeeingRange(*pTargetPed, *pPlayerPed, *pEquippedWeapon->GetWeaponInfo()))
												{
													bool bComputeFOVVisibility = pTargetPed->GetPedIntelligence()->GetPedPerception().ComputeFOVVisibility(pPlayerPed,NULL);
													weaponDebugf3("CNetObjPlayer::UpdatePendingTargetData -- weapon[%s] player[%p][%s][%s] target[%p][%s][%s] -- !IsNonViolent -- IsInSeeingRange -- bComputeFOVVisibility[%d]",pEquippedWeapon->GetWeaponInfo()->GetModelName(),pPlayerPed,pPlayerPed->GetModelName(),pPlayerPed->GetNetworkObject() ? pPlayerPed->GetNetworkObject()->GetLogName() : "",pTargetPed,pTargetPed->GetModelName(),pTargetPed->GetNetworkObject() ? pTargetPed->GetNetworkObject()->GetLogName() : "",bComputeFOVVisibility);
													if (bComputeFOVVisibility)
													{
														CEventGunAimedAt event(pPlayerPed);
														pTargetPed->GetPedIntelligence()->AddEvent(event);

														CEventFriendlyAimedAt eventFriendlyAimedAt(pPlayerPed);
														pTargetPed->GetPedIntelligence()->AddEvent(eventFriendlyAimedAt);
													}
												}
											}
										}
									}
								}
							}
						}
					}
					else
					{
						// ped out of scope, set it to free aim in the direction it's facing....
						Vector3 pedDir = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());
						Vector3 pedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

						CWeapon const* equippedWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();

						float range = equippedWeapon ? equippedWeapon->GetRange() : CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE;
						m_TargetAimPosition = pedPos + (pedDir * range);
					}

					m_TargetCloningInfo.SetHasValidTarget(TargetCloningInformation::TargetMode_Entity);
				}
				else
				{
					// From weapons.meta - all weapons fall under 
					// 150m unless it's a sniper rifle and then it's 1500.f
					// No remote sniper in MP
					if(m_pendingCameraTargetData.m_longRange)
					{
						gnetAssertf(m_pendingCameraTargetData.m_playerToTargetAimOffset.x <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE, "ERROR : CNetObjPlayer::SetCameraData : Invalid x offset %f", m_pendingCameraTargetData.m_playerToTargetAimOffset.x);
						gnetAssertf(m_pendingCameraTargetData.m_playerToTargetAimOffset.y <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE, "ERROR : CNetObjPlayer::SetCameraData : Invalid y offset %f", m_pendingCameraTargetData.m_playerToTargetAimOffset.y);
						gnetAssertf(m_pendingCameraTargetData.m_playerToTargetAimOffset.z <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_LONG_RANGE, "ERROR : CNetObjPlayer::SetCameraData : Invalid z offset %f", m_pendingCameraTargetData.m_playerToTargetAimOffset.z);
					}
					else
					{
						gnetAssertf(m_pendingCameraTargetData.m_playerToTargetAimOffset.x <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE, "ERROR : CNetObjPlayer::SetCameraData : Invalid x offset %f", m_pendingCameraTargetData.m_playerToTargetAimOffset.x);
						gnetAssertf(m_pendingCameraTargetData.m_playerToTargetAimOffset.y <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE, "ERROR : CNetObjPlayer::SetCameraData : Invalid y offset %f", m_pendingCameraTargetData.m_playerToTargetAimOffset.y);
						gnetAssertf(m_pendingCameraTargetData.m_playerToTargetAimOffset.z <= CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE, "ERROR : CNetObjPlayer::SetCameraData : Invalid z offset %f", m_pendingCameraTargetData.m_playerToTargetAimOffset.z);				
					}
					
					Vector3 playerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
					m_TargetAimPosition = playerPos - m_pendingCameraTargetData.m_playerToTargetAimOffset;

					m_TargetCloningInfo.SetHasValidTarget(TargetCloningInformation::TargetMode_PosWS);
				}

				m_TargetCloningInfo.SetFreeAimLockedOnTarget(m_pendingCameraTargetData.m_freeAimLockedOnTarget); // we can be free aim locked onto a ped we don't know about....
			}
		}
	}
}

void CNetObjPlayer::UpdateTargettedEntityPosition(void)
{
	CPed* pPlayer = GetPlayerPed();

	// Clear out the ped we think we're aiming at...
	m_TargetCloningInfo.SetCurrentAimTargetEntity(NULL);

	if(m_TargetCloningInfo.IsAiming())
	{
		// if we're locked onto a target, update the aim position with the target pos...
		if(m_TargetCloningInfo.GetCurrentAimTargetID() != NETWORK_INVALID_OBJECT_ID && pPlayer->GetWeaponManager())
		{
			netObject* pObj = NetworkInterface::GetNetworkObject(m_TargetCloningInfo.GetCurrentAimTargetID());
			if(pObj)
			{
				CEntity* entity = pObj->GetEntity();
				if(entity)
				{
					m_TargetCloningInfo.SetCurrentAimTargetEntity( entity );

					Vector3 targetPos = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
					Vector3 aimingPos = targetPos + m_TargetCloningInfo.GetLockOnTargetAimOffset();

					m_TargetAimPosition = aimingPos;
				}
			}
			else
			{
				// target ped out of scope, set player clone to free aim in the direction it's facing....
				Vector3 pedDir = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetForward());
				Vector3 pedPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

				CWeapon const* equippedWeapon = pPlayer->GetWeaponManager()->GetEquippedWeapon();

				float range = equippedWeapon ? equippedWeapon->GetRange() : CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE;
				Vector3 aimPosition = pedPos + (pedDir * range);			

				m_TargetAimPosition = aimPosition;
			}
		}
	}
}

void CNetObjPlayer::UpdateWeaponAimPosition(void)
{
	CPed* pPlayer = GetPlayerPed();

	if(pPlayer && pPlayer->GetWeaponManager())
	{
		if(m_TargetCloningInfo.IsAiming())
		{
			Vector3 FinalAimPos = m_TargetAimPosition;

			if(m_TargetCloningInfo.GetCurrentAimTargetID() != NETWORK_INVALID_OBJECT_ID)
			{
				ConvertEntityTargetPosToWeaponRangePos(m_TargetAimPosition, FinalAimPos);
			}

			// if we've just started aiming again, apply the position directly - we don't want to blend from 0,0,0 or out last aiming pos to the new pos...
			if(!m_TargetCloningInfo.WasAiming())
			{
				pPlayer->GetWeaponManager()->SetWeaponAimPosition(FinalAimPos);
			}
			else
			{
				if(!pPlayer->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_VEHICLE_GUN))
				{
					float AIM_LERP_SPEED = 0.125f;

					TUNE_GROUP_BOOL(NET_PLAYER_AIMING, bEnableFasterLerpSpeedForLockOnAndArresting, true);
					if (bEnableFasterLerpSpeedForLockOnAndArresting)
					{
						bool bIsLockedOn = false;
						if(m_LockOnTargetID != NETWORK_INVALID_OBJECT_ID)
						{
							netObject* targetObject  = NetworkInterface::GetNetworkObject(m_LockOnTargetID);
							if (targetObject)
							{
								bIsLockedOn = true;
							}
						}

						// Increase the aim position lerp speed if the player is locked-on or is arresting.
						if (bIsLockedOn || pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
						{
							TUNE_GROUP_FLOAT(NET_PLAYER_AIMING, fLockedOnOrArrestingLerpSpeed, 0.33f, AIM_LERP_SPEED, (AIM_LERP_SPEED * 10.0f), 0.01f);
							AIM_LERP_SPEED = fLockedOnOrArrestingLerpSpeed;
						}
					}

					// blend to our new target position....
					Vector3 currentAimPosition = pPlayer->GetWeaponManager()->GetWeaponAimPosition();
					pPlayer->GetWeaponManager()->SetWeaponAimPosition(Lerp(AIM_LERP_SPEED, currentAimPosition, FinalAimPos));
				}
			}
		}
	}
}

bool CNetObjPlayer::GetActualWeaponAimPosition(Vector3 &vAimPos)
{
	CPed* pPlayer = GetPlayerPed();

	if(pPlayer && pPlayer->GetWeaponManager() && m_TargetCloningInfo.IsAiming())
	{
		vAimPos = m_TargetAimPosition;
		return true;
	}

	return false;
}

// To save network bandwidth, the player target can just be an offset from a ped
// however the gun is not really aiming at that point, it is aiming at a point 
// on that line at weapon range distance away. When lerping between one of these
// targets or an actually full range world space point we get a noticable slow down
// in the clone target syncing. This converts all targets to the full weapon range away
void CNetObjPlayer::ConvertEntityTargetPosToWeaponRangePos(Vector3 const& entityOffsetPos, Vector3& weaponRangePos)
{
	CPed* pPlayerPed = GetPlayerPed();

	if(!pPlayerPed || !pPlayerPed->IsNetworkClone())
		return;

	if(!pPlayerPed->GetWeaponManager())
		return;

	CWeapon const* pWeapon			= pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
	CObject const* pWeaponObject	= pPlayerPed->GetWeaponManager()->GetEquippedWeaponObject();

	if((!pWeapon) || (!pWeaponObject))
		return;

	if(!m_TargetCloningInfo.IsAiming())
		return;

	// aiming at a world space position anyway
	if(m_TargetCloningInfo.GetCurrentAimTargetID() == NETWORK_INVALID_OBJECT_ID)
		return;

	Mat34V weaponMatrix = pWeaponObject->GetMatrix();

	Vector3 muzzlePosition(VEC3_ZERO); 
	pWeapon->GetMuzzlePosition(RC_MATRIX34(weaponMatrix), muzzlePosition);

	Vector3 direction = entityOffsetPos - muzzlePosition;

	// Muzzle is too close to the entity position (can cause some aim hysteresis/jitter).
	float fDirectionMagSq = direction.Mag2();
	if (fDirectionMagSq < 1.0f)
	{
		weaponRangePos = entityOffsetPos;
		return;
	}

	direction.Normalize();

	float fWeaponRange = pWeapon ? pWeapon->GetRange() : CPlayerCameraDataNode::MAX_WEAPON_RANGE_SHORT_RANGE;

	weaponRangePos = muzzlePosition + (direction * fWeaponRange);
}

void CNetObjPlayer::SetPlayerPedGroupData(CPlayerPedGroupDataNode& data)
{
    gnetAssert(GetPlayerPed());

    if (gnetVerify(GetPlayerPed()->GetPedsGroup()))
    {
        GetPlayerPed()->GetPedsGroup()->CopyNetworkInfo(data.m_pedGroup, true);
    }
}

void CNetObjPlayer::SetPlayerWantedAndLOSData(const CPlayerWantedAndLOSDataNode& data)
{
    CPed *pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);

    CWanted& wanted = pPlayerPed->GetPlayerInfo()->GetWanted();

	u32 uLatency = (u32) NetworkInterface::GetAverageSyncLatency(GetPhysicalPlayerIndex());
	wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData fromPlayer[%d][%s] uLatency[%u]",GetPhysicalPlayerIndex(),GetLogName(),uLatency);

	bool bReducedToZero = (wanted.m_WantedLevel && (data.m_wantedLevel == 0));

	if (bReducedToZero)
	{
		wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData -- pPlayerPed[%p][%s][%s] -- bReducedToZero --> MakeAnonymousDamageCB/set m_nWantedLevel=0/UpdateWantedLevel",pPlayerPed,pPlayerPed ? pPlayerPed->GetModelName() : "",pPlayerPed && pPlayerPed->GetNetworkObject() ? pPlayerPed->GetNetworkObject()->GetLogName() : "");

		wanted.m_nWantedLevel = 0; //also ensure the scored wanted level is reduced to zero - so it will properly update the wanted level in the UpdateWantedLevel below
		wanted.UpdateWantedLevel(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()),true,false);
	}

	wanted.m_WantedLevel = (eWantedLevel) data.m_wantedLevel;
	wanted.m_WantedLevelBeforeParole = (eWantedLevel)data.m_WantedLevelBeforeParole;
	wanted.m_bIsOutsideCircle = data.m_bIsOutsideCircle;

	m_remoteFakeWantedLevel = data.m_fakeWantedLevel;

	//Only allow respotting if within allowed possible latency - otherwise disregard. Use average computed latency in the calculation.
	static const u32 uAllowedDelayNetworkRefocusing = 200;
	bool bPlayerRespotted = data.m_timeLastSpotted > (wanted.m_iTimeSearchLastRefocused + uAllowedDelayNetworkRefocusing + uLatency);
	if (bPlayerRespotted)
	{
		wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData bPlayerRespotted set m_iTimeSearchLastRefocused=data.m_timeLastSpotted[%u] wanted.m_iTimeSearchLastRefocused[%u]",data.m_timeLastSpotted,wanted.m_iTimeSearchLastRefocused);

		wanted.m_LastSpottedByPolice         = data.m_lastSpottedLocation;
		wanted.m_CurrentSearchAreaCentre     = data.m_searchAreaCentre;
		wanted.m_iTimeSearchLastRefocused    = data.m_timeLastSpotted;
		wanted.m_HasLeftInitialSearchArea    = data.m_HasLeftInitialSearchArea;		
	}

	wanted.m_iTimeFirstSpotted				= data.m_timeFirstSpotted;

	if (data.m_causedByThisPlayer == true)
	{
		wanted.m_CausedByPlayerPhysicalIndex = pPlayerPed->GetNetworkObject()->GetPhysicalPlayerIndex();
	}
	else
	{
		wanted.m_CausedByPlayerPhysicalIndex = data.m_causedByPlayerPhysicalIndex;
	}

	bool bCopsAreSearching				= data.m_copsAreSearching;
	bool bCopsStoppedSearching			= m_bCopsWereSearching && !bCopsAreSearching;

	m_bCopsWereSearching = false; //clear

	//Note: if the player is respawning he will likely still have his myvehicle set and if he was in the same vehicle as the localplayer the localplayer's wanted level will get reset, prevent this if we are in the respawn invincibility time.
	if (m_respawnInvincibilityTimer > 0)
		return;

	bool bLocalPlayerInSameVehicleAsFromPlayer = false;

	//If any driver or passenger (in the same car) has his wanted level reduced to zero, then reduce the other passengers wanted level to zero at the same time. lavalley.
	//Also want to modify cops are searching remote attributes if the cops are searching for one of the players - so that the flashing effects them all.
	if (bReducedToZero || bCopsAreSearching || bCopsStoppedSearching || bPlayerRespotted)
	{
		wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData fc[%d] netttime[%u] bReducedToZero[%d] bCopsAreSearching[%d] bCopsStoppedSearching[%d] bPlayerRespotted[%d]",fwTimer::GetFrameCount(),NetworkInterface::GetNetworkTime(),bReducedToZero,bCopsAreSearching,bCopsStoppedSearching,bPlayerRespotted);

		if (pPlayerPed && pPlayerPed->GetIsInVehicle() && pPlayerPed->GetMyVehicle() && pPlayerPed->GetMyVehicle()->GetNetworkObject())
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			if (pLocalPlayer && pLocalPlayer->GetIsInVehicle() && pLocalPlayer->GetMyVehicle() && pLocalPlayer->GetMyVehicle()->GetNetworkObject() && (pLocalPlayer->GetMyVehicle()->GetNetworkObject()->GetObjectID() == pPlayerPed->GetMyVehicle()->GetNetworkObject()->GetObjectID()))
			{
				bLocalPlayerInSameVehicleAsFromPlayer = true;

				CWanted* pWanted = pLocalPlayer->GetPlayerWanted();
				if (pWanted)
				{
					if (bReducedToZero && !pLocalPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_DontClearLocalPassengersWantedLevel))
					{
						wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData bReducedToZero --> invoke WANTED_CLEAN");
						pWanted->SetWantedLevel(VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_REMOVING, true, INVALID_PLAYER_INDEX);
					}
					else
					{
						bool bWantedCopsAreSearching = pWanted->CopsAreSearching();
						wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData bWantedCopsAreSearching[%d]",bWantedCopsAreSearching);

						if (bWantedCopsAreSearching || bPlayerRespotted)
						{
							if (bCopsStoppedSearching || bPlayerRespotted)
							{
								eWantedLevel setWantedLevel = pWanted->GetWantedLevel();
								if (wanted.m_WantedLevel > setWantedLevel)
									setWantedLevel = wanted.m_WantedLevel;

								if (pWanted->GetWantedLevel() != setWantedLevel)
								{
									wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData (bCopsStoppedSearching || bPlayerRespotted) --> invoke SetWantedLevel[%d] WL_FROM_NETWORK",setWantedLevel);
									pWanted->SetWantedLevel(VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()), setWantedLevel, 0, WL_FROM_NETWORK, false, wanted.m_CausedByPlayerPhysicalIndex);									
								}
	
								wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData (bCopsStoppedSearching || bPlayerRespotted)--> invoke ReportPoliceSpottingPlayer");
								// "bPlayerRespotted" is poorly named; this is actually whether wanted search was last re-focused, not actually seen. We don't always want to automatically report the player as seen when this happens.
								// Quick, hacky, "safe" fix for now; just ensure the player has at least been spotted once before doing this. Ideally we'd sync whether we were actually spotted (perhaps time last spotted, though this may also be wrong if we are in stealth evasion mode).
								bool bShouldFlagAsSeen = data.m_timeFirstSpotted > 0.0f;	
								pWanted->ReportPoliceSpottingPlayer(NULL,VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()),wanted.m_WantedLevel,bShouldFlagAsSeen,bShouldFlagAsSeen);
							}
						}
						else
						{
							if (bCopsAreSearching)
							{
								//Don't immediately set cops are searching to true - if the local has recently refocused then retain it here - fixes problems with flashing stars flipping
								if (pWanted->GetTimeSinceSearchLastRefocused() > MAX_TIME_SEARCHLASTREFOCUSED_FLIPPING)
								{
									wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData bCopsAreSearching--> invoke SetCopsAreSearching(true)");
									pWanted->SetCopsAreSearching(true);
								}
							}
						}

						m_bCopsWereSearching = bCopsAreSearching;
					}
				}
			}
		}
	}
	
	static const u32 uAllowedDelayNetworkVisibleFromPlayer = 250;

	u8 localPhysicalPlayerIndex = NetworkInterface::GetLocalPhysicalPlayerIndex();
	if ( (data.m_visiblePlayers & (1<<localPhysicalPlayerIndex)) != 0 )
	{
		if (!bLocalPlayerInSameVehicleAsFromPlayer)
		{
			if (uLatency < uAllowedDelayNetworkVisibleFromPlayer)
			{
				CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
				CWanted* pLocalWanted = pLocalPlayer ? pLocalPlayer->GetPlayerWanted() : NULL;
				if (pLocalWanted && (pLocalWanted->GetWantedLevel() > WANTED_CLEAN))
				{
					//record that the from players wanted system thinks this player should be visible
					m_bWantedVisibleFromPlayer = true;

					wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData incoming LOCALPLAYER ReportPoliceSpottingPlayer"); 
					pLocalWanted->ReportPoliceSpottingPlayer(NULL,VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()),pLocalWanted->GetWantedLevel(),true,true);
				}
			}
		}
	}
	else if (m_bWantedVisibleFromPlayer)
	{
		//clear that the from players wanted system thinks this player should be visible
		if ( (data.m_visiblePlayers & (1<<localPhysicalPlayerIndex)) == 0 )
		{
			wantedDebugf3("CNetObjPlayer::SetPlayerWantedAndLOSData incoming LOCALPLAYER m_bWantedVisibleFromPlayer == 1 reset m_bWantedVisibleFromPlayer"); 
			m_bWantedVisibleFromPlayer = false;
		}
	}
}

void CNetObjPlayer::SetAmbientModelStreamingData(const CPlayerAmbientModelStreamingNode& data)
{
    m_AllowedPedModelStartOffset      = data.m_AllowedPedModelStartOffset;
    m_AllowedVehicleModelStartOffset  = data.m_AllowedVehicleModelStartOffset;
    m_TargetVehicleIDForAnimStreaming = data.m_TargetVehicleForAnimStreaming;
    m_TargetVehicleEntryPoint         = data.m_TargetVehicleEntryPoint;
}

void CNetObjPlayer::SetPlayerGamerData(const CPlayerGamerDataNode& data)
{
	CNetGamePlayer* pPlayer = GetPlayerOwner();
	if(gnetVerify(pPlayer))
	{
        //@@: location CNETOBJPLAYER_SETPLAYERGAMERDATA_SET_CLAN_MEMBERSHIP
		pPlayer->SetClanMembershipInfo(data.m_clanMembership);
		PlaceClanDecoration();

		const bool bIsRockstarDev = pPlayer->IsRockstarDev();
		// Report remote player is a R* DEV
		if (bIsRockstarDev != data.m_bIsRockstarDev && data.m_bIsRockstarDev)
		{
			CNetworkTelemetry::AppendMetricRockstarDEV(pPlayer->GetGamerInfo().GetGamerHandle());
		}

		const bool bIsRockstarQA = pPlayer->IsRockstarQA();
		// Report remote player is a R* QA
		if (bIsRockstarQA != data.m_bIsRockstarQA && data.m_bIsRockstarQA)
		{
			CNetworkTelemetry::AppendMetricRockstarQA(pPlayer->GetGamerInfo().GetGamerHandle());
		}

		//@@: range CNETOBJPLAYER_SETPLAYERGAMERDATA_SET_QUALIFYING_PROPERTIES {
		pPlayer->SetIsRockstarDev(data.m_bIsRockstarDev);
		pPlayer->SetIsRockstarQA(data.m_bIsRockstarQA);
		pPlayer->SetIsCheater(data.m_bIsCheater);
		pPlayer->SetMatchmakingGroup(static_cast<eMatchmakingGroup>(data.m_nMatchMakingGroup), true); 
		//@@: } CNETOBJPLAYER_SETPLAYERGAMERDATA_SET_QUALIFYING_PROPERTIES

		pPlayer->SetCommunicationPrivileges(data.m_hasCommunicationPrivileges);

		pPlayer->SetStartedTransition(data.m_bHasStartedTransition);
		if(data.m_bHasTransitionInfo)
		{
			rlSessionInfo hInfo;
			hInfo.FromString(data.m_aInfoBuffer);
			gnetAssert(hInfo.IsValid());
			pPlayer->SetTransitionSessionInfo(hInfo);
		}
		else
		{
			pPlayer->InvalidateTransitionSessionInfo();
		}

		pPlayer->SetMuteData(data.m_muteCount, data.m_muteTotalTalkersCount);

		pPlayer->SetCrewRankTitle(data.m_crewRankTitle);

		pPlayer->SetKickVotes(data.m_kickVotes);

		pPlayer->SetPlayerAccountId(data.m_playerAccountId);
	}
}

void CNetObjPlayer::SetPlayerExtendedGameStateData(const CPlayerExtendedGameStateNode& data)
{
	// grab the player ped
	CPed* pPed = GetPlayerPed();

	m_bOwnsWaypoint = data.m_bOwnsWaypoint;
	m_bHasActiveWaypoint = data.m_bHasActiveWaypoint;

	// if this guy is allowed to modify the waypoint and has dirty data
	if(data.m_WaypointLocalDirtyTimestamp > 0 && 
	   data.m_WaypointLocalDirtyTimestamp > m_WaypointLocalDirtyTimestamp && 
	   NetworkInterface::IsRelevantToWaypoint(pPed) && 
	   NetworkInterface::CanSetWaypoint(pPed))
	{
		if(data.m_bHasActiveWaypoint)
		{
			if(data.m_bOwnsWaypoint)
			{
				if(!CMiniMap::IsActiveWaypoint(Vector2(data.m_fxWaypoint, data.m_fyWaypoint), data.m_WaypointObjectId))
				{
					CMiniMap::SwitchOffWaypoint(false);
					CMiniMap::SwitchOnWaypoint(data.m_fxWaypoint, data.m_fyWaypoint, data.m_WaypointObjectId, false, false, NetworkInterface::GetWaypointOverrideColor());
				}
			}
		}
		// account for narrow edge case where a remote update to remove could wipe the just applied local waypoint
		else
        {            
			CMiniMap::SwitchOffWaypoint(false);
        }
	}

	// update this players waypoint data
	m_WaypointLocalDirtyTimestamp = data.m_WaypointLocalDirtyTimestamp;
	m_fxWaypoint = data.m_fxWaypoint;
	m_fyWaypoint = data.m_fyWaypoint;
	m_WaypointObjectId = data.m_WaypointObjectId;

	grcViewport *pViewport = IsClone() ? GetViewport() : &gVpMan.GetGameViewport()->GetNonConstGrcViewport();
	gnetAssert(pViewport);

	float newFOV = data.m_fovRatio * MAX_FOV;
	newFOV = Clamp(newFOV, MIN_FOV, MAX_FOV);
	float newFarClip = NET_PLAYER_MAX_VIEWPORT_FAR_CLIP - ((NET_PLAYER_MAX_VIEWPORT_FAR_CLIP - NET_PLAYER_DEFAULT_VIEWPORT_FAR_CLIP) * data.m_fovRatio);

	pViewport->Perspective(newFOV, data.m_aspectRatio, pViewport->GetNearClip(), newFarClip);

    m_CityDensitySetting = data.m_CityDensity;

	m_ghostPlayerFlags = data.m_ghostPlayers;

	pPed->GetPlayerInfo()->SetPlayerMaxExplosiveDamage(data.m_MaxExplosionDamage);
}

bool CNetObjPlayer::HasSinglePlayerModel() const
{
    CPed *pPlayerPed = GetPlayerPed();
    gnetAssert(pPlayerPed);
    CPedModelInfo* pedModelInfo = pPlayerPed ? pPlayerPed->GetPedModelInfo() : NULL;
    if(pedModelInfo && pedModelInfo->GetIsPlayerModel())
    {
        return true;
    }
	
    return false;
}

bool CNetObjPlayer::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
	bool bFix = CNetObjPed::FixPhysicsWhenNoMapOrInterior(npfbnReason);

	// players in a cutscene and not collideable on our machine should be fixed
	if (IsClone() && 
		GetPlayerOwner() &&
		GetPlayerOwner()->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE &&
		!GetPlayerPed()->IsCollisionEnabled())
	{
		bFix = true;
	}

	return bFix;
}

void CNetObjPlayer::PlayerHasJoined(const netPlayer& player)
{
	if (!player.IsLocal())
	{
		CNetworkCheckExeSizeEvent::Trigger( player.GetPhysicalPlayerIndex(), (s64)(NetworkInterface::GetExeSize()) );
	}

	CNetObjPed::PlayerHasJoined(player);
}

void CNetObjPlayer::PlayerHasLeft(const netPlayer& player)
{
    //If we are spectating the player that has left stop spectating
    CNetGamePlayer* netGamePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(player.GetPhysicalPlayerIndex());
    if (IsSpectating() && gnetVerify(netGamePlayer) && netGamePlayer->GetPlayerPed())
    {
        CPed* ped = netGamePlayer->GetPlayerPed();
        if (gnetVerify(ped && ped->GetNetworkObject()) && ped->GetNetworkObject()->GetObjectID() == m_SpectatorObjectId)
        {
            NetworkInterface::SetSpectatorObjectId(NETWORK_INVALID_OBJECT_ID);
        }
    }

    //If we are antagonistic to the player that has left stop being so
    if (IsAntagonisticToPlayer() && player.GetPhysicalPlayerIndex() == m_AntagonisticPlayerIndex)
    {
        SetAntagonisticPlayerIndex(INVALID_PLAYER_INDEX);
    }

    CNetObjPed::PlayerHasLeft(player);
}

void CNetObjPlayer::PostCreate()
{
    CNetObjPed::PostCreate();

    if(IsClone())
    {
        // the blender won't have been created until after the player sector pos node was applied,
        // so we need to do this after the creation has completed as well
        CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());

        if(netBlenderPed)
        {
            CPlayerSectorPosNode &data = *(static_cast<CPlayerSyncTree *>(GetSyncTree())->GetPlayerSectorPosNode());
            netBlenderPed->SetIsRagdolling(data.m_IsRagdolling);
            netBlenderPed->UpdateStandingData(data.m_StandingOnNetworkObjectID, data.m_LocalOffset, netBlenderPed->GetLastSyncMessageTime());
        }
    }
}

bool CNetObjPlayer::ShouldStopPlayerJitter()
{
	unsigned resultCode=0;

	if(!CanBlend(&resultCode) && resultCode == CB_FAIL_BLENDING_DISABLED)
	{
		CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());
		CPed* pPlayerPed = GetPlayerPed();

		if(netBlenderPed && taskVerifyf(pPlayerPed, "Null player ped") )
		{
			Vector3 currentPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			Vector3 targetPos  = netBlenderPed->GetLastPositionReceived();

			static const float MAX_POSITION_DIFF_SQR = rage::square(0.1f);

			if((currentPos - targetPos).Mag2() <= MAX_POSITION_DIFF_SQR)
			{
				return true;
			}
		}
	}

	return false;
}

void CNetObjPlayer::SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate)
{
	CNetObjPed::SetIsVisible(isVisible, reason, bNetworkUpdate);

	if (bNetworkUpdate)
	{
		m_bLastVisibilityStateReceived = isVisible;
	}
}

void CNetObjPlayer::DisableCollision()
{
	CPed *pPlayerPed = GetPlayerPed();

	if (pPlayerPed)
	{
		pPlayerPed->DisableCollision(NULL, pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ScriptHasCompletelyDisabledCollision));
	}
}

void CNetObjPlayer::ForceSendOfCutsceneState()
{
	if (!IsClone() && GetSyncData())
	{
		CPlayerSyncTree *syncTree = SafeCast(CPlayerSyncTree, GetSyncTree());

		if(syncTree)
		{
			syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetPlayerGameStateDataNode());
			syncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *syncTree->GetScriptGameStateNode());
		}
	}
}

void CNetObjPlayer::OnUnregistered()
{
	if (!IsClone())
	{
		ms_pLocalPlayerWeaponObject = NULL;
		ms_pLocalPlayerWeaponPickup = NULL;
	}

	CNetObjPed::OnUnregistered();
}

void CNetObjPlayer::UpdateNonPhysicalPlayerData()
{
    if(gnetVerifyf(!IsClone(), "This function should not be called on clone players!"))
    {
        CNetGamePlayer *pPlayer = GetPlayerOwner();

        if(gnetVerifyf(pPlayer, "The local network player has no player owner!"))
        {
            CNonPhysicalPlayerData *nonPhysicalData = pPlayer ? static_cast<CNonPhysicalPlayerData *>(pPlayer->GetNonPhysicalData()) : 0;

            if(gnetVerifyf(nonPhysicalData, "The local network player has no non-physical data!"))
            {
                if(GetPlayerPed())
                {
                    nonPhysicalData->SetRoamingBubbleMemberInfo(pPlayer->GetRoamingBubbleMemberInfo());
                    nonPhysicalData->SetPosition(VEC3V_TO_VECTOR3(GetPlayerPed()->GetTransform().GetPosition()));
                }
            }
        }
    }
}

// determines whether the player is visible, periodically
void CNetObjPlayer::DoOnscreenVisibilityTest()
{
	if (m_VisibilityQueryTimer > 0)
	{
		u32 timestep = fwTimer::GetTimeStepInMilliseconds();

		if (m_VisibilityQueryTimer <= timestep)
		{
			// the visibility has not been queried for a while, reset the visibility state
			m_VisibilityQueryTimer = 0;
			m_VisibilityProbeHandle.Reset();
			m_visibleOnscreen = 0;
			return;
		}
		else
		{
			m_VisibilityQueryTimer -= timestep;
		}

		// are the results of a previous probe ready?
		if (m_VisibilityProbeHandle.GetResultsReady())
		{
			m_visibleOnscreen = m_VisibilityProbeHandle.GetNumHits() == 0;
			m_VisibilityProbeHandle.Reset();
		}
		else if (GetPlayerPed() && !m_VisibilityProbeHandle.GetWaitingOnResults())
		{
			bool bTimeToTest = true;

			if (m_visibleOnscreen)
			{
				if (fwTimer::GetTimeInMilliseconds() >= m_TimeOfLastVisibilityCheck)
				{
					bTimeToTest = (fwTimer::GetTimeInMilliseconds()) - m_TimeOfLastVisibilityCheck >= PLAYER_VISIBILITY_TEST_FREQUENCY;
				}
			}

			if (bTimeToTest)
			{
				bool onScreen = false;

				m_TimeOfLastVisibilityCheck = fwTimer::GetTimeInMilliseconds();

				// use the player's head for the visibility check
				Matrix34 matHead;
				GetPlayerPed()->GetGlobalMtx(GetPlayerPed()->GetBoneIndexFromBoneTag(BONETAG_HEAD), matHead);

				Vector3 playerPos = matHead.d;

				if(gVpMan.GetCurrentViewport())
				{
					const float MAX_DISTANCE_TO_CHECK_SQUARED = static_cast<float>(PLAYER_VISIBILITY_TEST_RANGE * PLAYER_VISIBILITY_TEST_RANGE);
					const float TEST_RADIUS                   = 1.5f;

					const grcViewport &grcVp = gVpMan.GetCurrentViewport()->GetGrcViewport();

					if((VEC3V_TO_VECTOR3(grcVp.GetCameraPosition()) - playerPos).Mag2() < MAX_DISTANCE_TO_CHECK_SQUARED)
					{
						onScreen = (grcVp.IsSphereVisible(playerPos.x, playerPos.y, playerPos.z, TEST_RADIUS) != cullOutside);
					}
				}

				if (onScreen)
				{
					WorldProbe::CShapeTestProbeDesc probeData;
					probeData.SetResultsStructure(&m_VisibilityProbeHandle);
					probeData.SetStartAndEnd(camInterface::GetPos(),playerPos);
					probeData.SetContext(WorldProbe::ENetwork);
					probeData.SetOptions(WorldProbe::LOS_IGNORE_SHOOT_THRU | WorldProbe::LOS_IGNORE_SEE_THRU | WorldProbe::LOS_GO_THRU_GLASS);
					probeData.SetIsDirected(true);

					// ignore the vehicle this player is in, and our own
					CPed*		pLocalPlayer		= CGameWorld::FindLocalPlayer();
					CVehicle*	pOurVehicle			= pLocalPlayer ? pLocalPlayer->GetVehiclePedInside() : NULL;
					CVehicle*	pOtherPlayerVehicle = GetPlayerPed()->GetVehiclePedInside();

					const CEntity* entityExcludes[2];
					u32 numEntityExcludes = 0;

					if (pOurVehicle)
					{
						entityExcludes[numEntityExcludes++] = pOurVehicle;
					}
					if (pOtherPlayerVehicle)
					{
						entityExcludes[numEntityExcludes++] = pOtherPlayerVehicle;
					}

					if (numEntityExcludes > 0)
					{
						probeData.SetExcludeEntities(entityExcludes, numEntityExcludes);
					}

					probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);

					WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
				}
				else
				{
					m_VisibilityProbeHandle.Reset();
					m_visibleOnscreen = false;
				}
			}
		}
    }
}

bool 
CNetObjPlayer::CreateRespawnNetObj(const ObjectId respawnNetObjId)
{
	bool result = false;

#if ENABLE_NETWORK_BOTS
    CNetGamePlayer* player = NetworkInterface::GetPhysicalPlayerFromIndex(GetPhysicalPlayerIndex());
    if (player && !gnetVerify(!player->IsBot()))
    {
		return result;
	}
#endif // ENABLE_NETWORK_BOTS

	netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "CREATE_TEAM_SWAP", GetLogName());
	const char* outcome = "Created.";

    CPed* playerPed = GetPlayerPed();

    if (playerPed)
    {
		if(!gnetVerifyf(!IsClone() || (NETWORK_INVALID_OBJECT_ID != respawnNetObjId), "Clone Player %s - Respawn obj id %d doesnt yet exist.", GetLogName(), respawnNetObjId))
        {
            return result;
        }

		if (NETWORK_INVALID_OBJECT_ID != respawnNetObjId)
		{
			netObject* netObj = CNetwork::GetObjectManager().GetNetworkObject(respawnNetObjId, true);

			//Network object already exists - Respawn has already been done
			if (netObj)
			{
				if (IsClone())
				{
					m_RespawnNetObjId = respawnNetObjId;
					result = true;
				}
				else
				{
					networkLog.WriteDataValue("Create net obj", "Failed: (Object already exists, %s)", netObj->GetLogName());
				}

				return result;
			}
		}

        if (!CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_PED, true))
        {
            NetworkInterface::GetObjectManager().TryToMakeSpaceForObject(NET_OBJ_TYPE_PED);
        }

        if (CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_PED, true))
        {
            m_RespawnNetObjId = respawnNetObjId;
			if (m_RespawnNetObjId == NETWORK_INVALID_OBJECT_ID)
			{
				m_RespawnNetObjId = NetworkInterface::GetObjectManager().GetObjectIDManager().GetNewObjectId();
			}

            netObject* netObj = CPed::StaticCreateNetworkObject(m_RespawnNetObjId
                                                                ,GetPhysicalPlayerIndex()
																,CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|CNetObjGame::GLOBALFLAG_PERSISTENTOWNER|CNetObjGame::GLOBALFLAG_CLONEALWAYS
                                                                ,MAX_NUM_PHYSICAL_PLAYERS);

            if (gnetVerify(netObj) && gnetVerify(!netObj->HasGameObject()))
            {
				//Set flag to avoid aborting the ragdoll during setup.
				networkLog.WriteDataValue("Flag Set:", "%s", "LOCALFLAG_RESPAWNPED");
				netObj->SetLocalFlag(CNetObjGame::LOCALFLAG_RESPAWNPED, true);
				netObj->SetLocalFlag(CNetObjGame::LOCALFLAG_NOREASSIGN, true);

                networkLog.WriteDataValue("Object Id:", "%d", m_RespawnNetObjId);
                NetworkInterface::GetObjectManager().RegisterNetworkObject(netObj);
				result = true;
            }
            else if(netObj && netObj->HasGameObject())
            {
				outcome = "Object alerady has a Game Object.";
                delete netObj;
            }
			else if(!netObj)
			{
				outcome = "To create network Object.";
				delete netObj;
			}
        }
        else
        {
			outcome = "Can NOT register Object.";
        }
    }

	networkLog.WriteDataValue("Create net obj", "%s: %s (Id=%d)", result ? "Failed":"Success", outcome, m_RespawnNetObjId);

	return result;
}

bool
CNetObjPlayer::CanRespawnLocalPlayerPed( )
{
    bool canRespawn = false;

	netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "CHECK_LOCAL_TEAM_SWAP", GetLogName());

	const char* outcome = "No player ped.";

	CPed* ped = GetPlayerPed();
	VALIDATE_GAME_OBJECT(ped);

	if (ped && gnetVerify(!IsClone()))
	{
		outcome = "Allowed to leave ped behind.";

		static float s_PedTransferDistOnScreen = 60.0f;
		static float s_PedTransferDistOffScreen = 20.0f;
		canRespawn = NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition())
																			,ped->GetBoundRadius()
																			,s_PedTransferDistOnScreen
																			,s_PedTransferDistOffScreen);

		if (canRespawn)
		{
			//Check that there is space to register the network object.
			canRespawn = CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_PED, true);

			if (!canRespawn)
			{
				NetworkInterface::GetObjectManager().TryToMakeSpaceForObject(NET_OBJ_TYPE_PED);
			}

			canRespawn = CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_PED, true);

			if (!canRespawn) 
			{
				outcome = "No space to register network object.";
			}
		}
		else
		{
			outcome = "No remote player's close.";
		}
    }

	networkLog.WriteDataValue("Team Swap", "%s: (%s)", canRespawn ? "Success" : "Failed", outcome);

    // ensure the object ID is always reset when we can't respawn
    if(!canRespawn)
    {
        m_RespawnNetObjId = NETWORK_INVALID_OBJECT_ID;
    }

    return canRespawn;
}


bool
CNetObjPlayer::CanRespawnClonePlayerPed(const ObjectId respawnNetObjId)
{
	bool canRespawn = false;

	netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "CHECK_CLONE_TEAM_SWAP", GetLogName());

	const char* outcome = "No player ped.";

	CPed* ped = GetPlayerPed();
	VALIDATE_GAME_OBJECT(ped);

	if (ped && gnetVerify(IsClone()))
	{
		canRespawn = ((NETWORK_INVALID_OBJECT_ID != respawnNetObjId) && (m_RespawnNetObjId != respawnNetObjId));

		if (canRespawn)
		{
			outcome = "Allowed to leave ped behind.";

			netObject* netObj = CNetwork::GetObjectManager().GetNetworkObject(respawnNetObjId, true);

			if (!netObj)
			{
				outcome = "Network Object doesn't exist.";
				canRespawn = false;
			}
			else if (netObj->HasGameObject())
			{
				outcome = "Network Object already has a game object.";
				canRespawn = false;
			}
		}
		else if (NETWORK_INVALID_OBJECT_ID == respawnNetObjId)
		{
			outcome = "Invalid Network Object Id.";
		}
		else if (m_RespawnNetObjId == respawnNetObjId)
		{
			outcome = "Network objects id's are equal.";
		}
	}

	networkLog.WriteDataValue("Object Id:", "PED_%d", respawnNetObjId);
	networkLog.WriteDataValue("Team Swap", "%s: (%s)", canRespawn ? "Success" : "Failed", outcome);

    return canRespawn;
}

bool
CNetObjPlayer::RespawnPlayerPed(const Vector3& newPlayerCoors, bool killPed, const ObjectId respawnNetObjId)
{
	respawnDebugf3("CNetObjPlayer::RespawnPlayerPed");

	bool result = false;

#if ENABLE_NETWORK_BOTS
    CNetGamePlayer* player = NetworkInterface::GetPhysicalPlayerFromIndex(GetPhysicalPlayerIndex());
    if (player)
    {
        gnetAssert(!player->IsBot());
    }
#endif // ENABLE_NETWORK_BOTS

	bool networkObjCreated = true;
	const ObjectId previousRespawnNetObjId = m_RespawnNetObjId;

	//Create the network object.
	if (!IsClone() || respawnNetObjId != NETWORK_INVALID_OBJECT_ID)
	{
		networkObjCreated = CreateRespawnNetObj(respawnNetObjId);
	}

    CNetObjPed* pedNetObj = static_cast<CNetObjPed *> (NetworkInterface::GetNetworkObject(m_RespawnNetObjId));

    CPed* ped = GetPlayerPed();
    VALIDATE_GAME_OBJECT(ped);

	netLoggingInterface& networkLog = NetworkInterface::GetObjectManager().GetLog();
	NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "DO_TEAM_SWAP", GetLogName());

#if ENABLE_NETWORK_LOGGING
	networkLog.WriteDataValue("Player:", "%s:%s", GetLogName(), IsClone() ? "Clone" : "Local");
	if (m_RespawnNetObjId != NETWORK_INVALID_OBJECT_ID)
		networkLog.WriteDataValue("Object Id:", "PED_%d", m_RespawnNetObjId);
	else
		networkLog.WriteDataValue("Object Id:", "NETWORK_INVALID_OBJECT_ID");
	networkLog.WriteDataValue("Previous Object Id:", "PED_%d", previousRespawnNetObjId);

	const CQueriableInterface* qi = ped->GetPedIntelligence()->GetQueriableInterface();
	const bool runningExitVehicle = (qi->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) 
										|| qi->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
	networkLog.WriteDataValue("Running task exit vehicle:", "%s", runningExitVehicle ? "True" : "False");
#endif // ENABLE_NETWORK_LOGGING

	const char* outcome = "Unknown.";
	bool success = false;

    //If the ped already has a GameObject then surely the
    //command was already called previously if the
    if (networkObjCreated
		&& m_RespawnNetObjId != previousRespawnNetObjId
        && pedNetObj
        && gnetVerify(ped)
		&& gnetVerify(!pedNetObj->HasGameObject()))
    {
        CNetObjPlayer* playerNetObj = static_cast<CNetObjPlayer *> (ped->GetNetworkObject());
        gnetAssert(playerNetObj);

        //Create the game object
        if (gnetVerify(playerNetObj))
        {
            //Cache Player Position
            Vector3 oldPlayerPos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

            networkLog.WriteDataValue("Current Player pos:", "%f %f %f", oldPlayerPos.x, oldPlayerPos.x, oldPlayerPos.z);

            const CControlledByInfo networkNpcControl(true, false);

            Matrix34 tempMat;
            tempMat.Identity();
            tempMat.d = oldPlayerPos;

			bool bApplyDefaultVariation = false;
			if(ped->GetModelNameHash() == ATSTRINGHASH("p_franklin_02", 0xAF10BD56)
			|| ped->GetModelNameHash() == ATSTRINGHASH("ig_lamardavis_02", 0x1924A05E))
			{
				bApplyDefaultVariation = true;
			}

			CPed* newPlayerPed = CPedFactory::GetFactory()->CreatePed(networkNpcControl, ped->GetModelId(), &tempMat, bApplyDefaultVariation, false, false); //changed param bApplyDefaultVariation from true to false - fix problem where GetAlternates is called - lavalley - url:bugstar:1977243
            if (gnetVerify(newPlayerPed))
            {
				success = true;

                gnetAssert(!newPlayerPed->GetNetworkObject());
				pedNetObj->SetGameObject(newPlayerPed);
				newPlayerPed->SetNetworkObject(pedNetObj);
				pedNetObj->DestroyNetBlender();
				pedNetObj->CreateNetBlender();

				//---- SETUP PhysicalFlags
				newPlayerPed->m_nPhysicalFlags.bNotDamagedByAnything = ped->m_nPhysicalFlags.bNotDamagedByAnything;
				ped->m_nPhysicalFlags.bNotDamagedByAnything = false;

				//---- SETUP ePedConfigFlags - TEZ -> Can we put this in a data file - flags to preverse upon death?
				newPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisablePlayerLockon, ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePlayerLockon));
				newPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillFlyThroughWindscreen, ped->GetPedConfigFlag(CPED_CONFIG_FLAG_WillFlyThroughWindscreen) );
				newPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillJackAnyPlayer, ped->GetPedConfigFlag(CPED_CONFIG_FLAG_WillJackAnyPlayer) );
				newPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayerIsWeird, ped->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerIsWeird) );
				newPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInBikes, ped->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInBikes) );

                //Player vehicle information
                CNetObjVehicle* vehicleNetObj = NULL;
                CVehicle* vehicle = NULL;
                u32 seat = 0;

                if (ped->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
                {
                    ObjectId vehicleID = NETWORK_INVALID_OBJECT_ID;
                    playerNetObj->GetVehicleData(vehicleID, seat);
                    seat--; //Because Seat_Invalid is used

                    vehicle = ped->GetMyVehicle();
                    if (gnetVerify(vehicle))
                    {
                        vehicleNetObj = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());
                    }

					networkLog.WriteDataValue("Player in vehicle:", "%s", vehicleNetObj ? vehicleNetObj->GetLogName() : "None");
					networkLog.WriteDataValue("Player in seat:", "%d", seat);

					CComponentReservationManager* pCRM = vehicle->GetComponentReservationMgr();
					if (pCRM)
					{
						for(u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex++)
						{
							CComponentReservation* componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);
							if(gnetVerify(componentReservation))
							{
								CPed* pPed = componentReservation->GetPedUsingComponent();
								if (pPed && pPed == ped)
								{
									networkLog.WriteDataValue("Player using vehicle Component ID:", "%d", componentIndex);
								}
							}
						}
					}

					pCRM = ped->GetComponentReservationMgr();
					if (pCRM)
					{
						for(u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex++)
						{
							CComponentReservation *componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);
							if(gnetVerify(componentReservation))
							{
								CPed* pPed = componentReservation->GetPedUsingComponent();
								if (pPed && pPed == ped)
								{
									networkLog.WriteDataValue("Player using Component ID:", "%d", componentIndex);
								}
							}
						}
					}
                }

                //Player interior location
				const fwInteriorLocation playerInteriorLocation = ped->GetInteriorLocation();
                if (IsInInterior())
                {
					CDynamicEntityGameStateDataNode dynentitynode;
                    GetDynamicEntityGameState(dynentitynode);
                    pedNetObj->SetDynamicEntityGameState(dynentitynode);
                }

                // Cache the position
                u16 sectorX, sectorY, sectorZ;
                float sectorPosX, sectorPosY, sectorPosZ;
                GetSector(sectorX, sectorY, sectorZ);
                GetSectorPosition(sectorPosX, sectorPosY, sectorPosZ);

                //Cache Appearance Data
				bool isAttachingHelmet = false;
				bool isRemovingHelmet = false;
				bool isWearingHelmet = false;
				bool isVisorUp = false;
				bool supportsVisor = false;
				bool isVisorSwitching = false;
				u16 visorUp = 0;
				u16 visorDown = 0;
				u8 helmetTexture = 0;
				u8 targetVisorState = 0;
				u16 helmetProp = 0;
				u8 parachuteTintIndex = 0;
				u8 parachutePackTintIndex = 0;
				u32 phoneMode = 0;
                CPackedPedProps pedProps;
                CSyncedPedVarData pedVariationData;
				fwMvClipSetId facialClipSet;
				u32 facialClipNameHash = 0;
				u32 facialClipDictNameHash = 0;
                GetPedAppearanceData(pedProps, pedVariationData, phoneMode, parachuteTintIndex, parachutePackTintIndex, facialClipSet, facialClipNameHash, facialClipDictNameHash, isAttachingHelmet, isRemovingHelmet, isWearingHelmet, helmetTexture, helmetProp, visorUp, visorDown, isVisorUp, supportsVisor, isVisorSwitching, targetVisorState);
                CPedPropsMgr::SetPedPropsPacked(newPlayerPed, pedProps);

				pedVariationData.ApplyToPed(*newPlayerPed);
 
				//Copy Health and Armour Data
				newPlayerPed->SetHealth(ped->GetHealth(), true, false);
				newPlayerPed->SetArmour(ped->GetArmour());

				//Copy Endurance Data
				newPlayerPed->SetEndurance(ped->GetEndurance(), true);

                //cache the weapon slot data
                u32    chosenWeaponType         = 0;
				u8     weaponObjectTintIndex    = 0;
                bool   weaponObjectExists       = false;
                bool   weaponObjectVisible      = false;
				bool   weaponObjectFlashLightOn = false;
				bool   weaponObjectHasAmmo      = true;
				bool   weaponObjectAttachLeft   = false;
				
                GetWeaponData(chosenWeaponType, weaponObjectExists, weaponObjectVisible, weaponObjectTintIndex, weaponObjectFlashLightOn, weaponObjectHasAmmo, weaponObjectAttachLeft);
				
				CWeapon *equippedWeapon = ped->GetWeaponManager()->GetEquippedWeapon();						
				// GetWeaponData() above can sometimes return invalid params (ie: valid weapon hash(chosenWeaponType), but invalid tint or other attributes)
				// This happens because the weapon object can be null, but the equipped weapon hash can still be valid 
				// (i.e. when a player is melee-insta-killed, he drops the weapon model to clutch his head, but still has a valid equipped weapon hash).
				// If that is the case, make sure the hash and other attributes are reset:
				if(!equippedWeapon)
				{
					chosenWeaponType         = 0;
					weaponObjectTintIndex    = 0;
					weaponObjectExists       = false;
					weaponObjectVisible      = false;
					weaponObjectFlashLightOn = false;
					weaponObjectHasAmmo      = true;
					weaponObjectAttachLeft   = false;		
				}

		        u32 equippedGadgets[MAX_SYNCED_GADGETS];
    		    u32 numGadgets = GetGadgetData(equippedGadgets);
				u8  weaponComponentsTint[MAX_SYNCED_WEAPON_COMPONENTS];
				u32 weaponComponents[MAX_SYNCED_WEAPON_COMPONENTS];
				u32 numWeaponComponents = GetWeaponComponentData(weaponComponents, weaponComponentsTint);

                //cache the AI data
				CPedAIDataNode pedAIDataNode;
                GetAIData(pedAIDataNode);

                //cache inventory data
				CPedInventoryDataNode pedInventoryDataNode;
                GetPedInventoryData(pedInventoryDataNode);

				//cache the taskslot
				TaskSlotCache taskSlotData;
				GetTaskSlotData(taskSlotData);

                //Call AI Player Ped change
                CGameWorld::ChangePlayerPed(*ped, *newPlayerPed);

				//copy over effects when mutliplayer players change
				//some effects want to persist (e.g. glows on angels/devils) and get created on remote machines before the player respawns
				ptfxAttach::UpdateBrokenFrag(ped, newPlayerPed);

				//ped head blend data
				newPlayerPed->CloneHeadBlend(ped, true);

				newPlayerPed->FinalizeHeadBlend();

				//Medals/Tattoos - ped decorations, not to be confused with script ped decorators, which are something else!
				u32 packedDecorations[NUM_DECORATION_BITFIELDS];
				PedDecorationManager &decorationMgr = PedDecorationManager::GetInstance();
				int crewEmblemVariation;
				const bool hasDecorations = decorationMgr.GetDecorationsBitfieldFromLocalPed(ped, packedDecorations, NUM_DECORATION_BITFIELDS, crewEmblemVariation);
				if(hasDecorations)
				{
					decorationMgr.ProcessDecorationsBitfieldForRemotePed(newPlayerPed, packedDecorations, NUM_DECORATION_BITFIELDS, crewEmblemVariation);
				}

				//Clear the commonly used script object global flags
				pedNetObj->SetLocalFlag(LOCALFLAG_NOREASSIGN, false);
				pedNetObj->SetLocalFlag(LOCALFLAG_NOLONGERNEEDED, false);
				if (!pedNetObj->IsClone())
				{
					pedNetObj->StartSynchronising();
					pedNetObj->SetGlobalFlag(GLOBALFLAG_PERSISTENTOWNER, false);
					pedNetObj->SetGlobalFlag(GLOBALFLAG_CLONEALWAYS, false);
					pedNetObj->SetGlobalFlag(GLOBALFLAG_SCRIPTOBJECT, false);
				}

#if ENABLE_NETWORK_LOGGING
				NetworkLogUtils::WritePlayerText(networkLog, GetPhysicalPlayerIndex(), "DO_TEAM_SWAP", GetLogName());

                Vector3 diff(VEC3_ZERO);

                if (FindPlayerPed())
                {
                    diff = oldPlayerPos - VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
                    networkLog.WriteDataValue("Dist from local player", "%f", diff.GetHorizontalMag());
                }

                networkLog.WriteDataValue("Ragdolling", "%s", ped->GetUsingRagdoll() ? "true" : "false");
                networkLog.WriteDataValue("Low lod", "%s", ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) ? "true" : "false");
                networkLog.WriteDataValue("Capsule disabled", "%s", ped->GetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule) ? "true" : "false");

                Vector3 pedPos = oldPlayerPos;
                Vector3 vStart = pedPos + Vector3(0.0f, 0.0f, 1.0f);
                Vector3 vEnd = vStart - Vector3(0.0f, 0.0f, 2.0f);
                WorldProbe::CShapeTestProbeDesc probeDesc;
                WorldProbe::CShapeTestFixedResults<> probeResult;
                probeDesc.SetResultsStructure(&probeResult);
                probeDesc.SetStartAndEnd(vStart, vEnd);
                probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
                probeDesc.SetContext(WorldProbe::ENetwork);

                if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
                {
                    Vector3 hitPos = probeResult[0].GetHitPosition();

                    networkLog.WriteDataValue("Ground Z:", "%f", hitPos.z);
                }
                else
                {
                    networkLog.WriteDataValue("Ground Z:", "** none found! **");
                }
#endif // ENABLE_NETWORK_LOGGING

                //Restore cached data
				SetSector(sectorX, sectorY, sectorZ);
				SetSectorPosition(sectorPosX, sectorPosY, sectorPosZ);
				SetAIData(pedAIDataNode);

				SetWeaponComponentData(chosenWeaponType, weaponComponents, weaponComponentsTint, numWeaponComponents);
				SetWeaponData(chosenWeaponType, weaponObjectExists, weaponObjectVisible, weaponObjectTintIndex, weaponObjectFlashLightOn, weaponObjectHasAmmo, weaponObjectAttachLeft);				
				SetGadgetData(equippedGadgets, numGadgets);

				SetPedInventoryData(pedInventoryDataNode);
				UpdateWeaponSlots(0);	

                newPlayerPed->SetPosition(newPlayerCoors, true, !newPlayerPed->GetUsingRagdoll(), true);
                //Fix blender issues for player
                if (IsClone())
                {
                    static_cast< CNetBlenderPed* >(GetNetBlender())->Reset();
                    GetNetBlender()->GoStraightToTarget();
                }

				if (!CGameWorld::HasEntityBeenAdded(newPlayerPed))
				{
	                CGameWorld::Add(newPlayerPed, playerInteriorLocation);
				}

                //Post Player Setup
                if (vehicleNetObj)
                {
                    newPlayerPed->SetPedOutOfVehicle(CPed::PVF_Warp);
                    newPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle, false);
                    newPlayerPed->SetMyVehicle(NULL);

                    if (IsClone())
                    {
                        SetVehicleData(NETWORK_INVALID_OBJECT_ID, 0);
                    }
                }

				pedNetObj->PostSetupForRespawnPed(vehicle, seat, killPed, taskSlotData);
				if(vehicleNetObj)
				{
					vehicleNetObj->PostSetupForRespawnPed(ped);
				}

				if (CNetwork::IsLastDamageEventPendingOverrideEnabled() && CNetObjPed::ms_playerMaxHealthPendingLastDamageEventOverride)
				{
					SetLastDamageEventPending(true);
				}

                if (!IsClone())
                {
                    //Force a send of all dirty nodes the player so that the new model, tasks and position all arrive in one update message
                    CPedSyncTreeBase* playerSyncTree = static_cast<CPedSyncTreeBase*>(playerNetObj->GetSyncTree());
                    if (gnetVerify(playerSyncTree))
                    {
						playerSyncTree->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());
                        playerSyncTree->ForceSendOfSyncUpdateNodes(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), playerNetObj);
                    }

					// force the health node dirty for the dead ped left behind, this is so that a health of 0 is broadcast
					CPedSyncTreeBase* pedSyncTree = static_cast<CPedSyncTreeBase*>(pedNetObj->GetSyncTree());
					if (gnetVerify(pedSyncTree))
					{
						pedSyncTree->DirtyNode(pedNetObj, *pedSyncTree->GetPedHealthNode());
					}
				}

				respawnDebugf3("check newPlayerPed->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY)[%d]",newPlayerPed->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY));
				if (!gnetVerifyf(newPlayerPed->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY), "When Respawning the Player Ped it was not set to visible for the Gameplay Module"))
				{
					respawnDebugf3("newPlayerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, false)");
					SetIsVisibleForModuleSafe(newPlayerPed, SETISVISIBLE_MODULE_GAMEPLAY, true, false);
				}

				outcome = "Team Swap Completed.";

				//Add network event informing the ped id.
				CEventNetworkPedLeftBehind scrEvent(ped);
				GetEventScriptNetworkGroup()->Add(scrEvent);

				result = true;
            }
			else
			{
				outcome = "Factory failed to create PED.";
			}
        }
		else
		{
			outcome = "No CNetObjPlayer.";
		}

		networkLog.WriteDataValue("Respawn Team swap", "%s:%s", success ? "Success":"Failed", outcome);
    }
	else
	{
		if (m_RespawnNetObjId == previousRespawnNetObjId)
		{
			networkLog.WriteDataValue("Respawn Team swap", "Failed: Previous object and new object are the same.");
		}
		else if (!ped)
		{
			networkLog.WriteDataValue("Respawn Team swap", "Failed: No player Ped.");
		}
		else if (!pedNetObj)
		{
			networkLog.WriteDataValue("Respawn Team swap", "Failed: to find the Network Object.");
		}
		else if (pedNetObj->HasGameObject())
		{
			networkLog.WriteDataValue("Respawn Team swap", "Failed: already has a GameObject.");
		}
		else if (!networkObjCreated)
		{
			networkLog.WriteDataValue("Respawn Team swap", "Failed: to create network object.");
		}
		else
		{
			networkLog.WriteDataValue("Respawn Team swap", "Failed: Unknown Reason.");
		}
	}

    return result;
}

void
CNetObjPlayer::Resurrect(const Vector3& newCoors, const bool triggerNetworkRespawnEvent)
{
    CPed* pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

	respawnDebugf3("CNetObjPlayer::Resurrect pPlayer[%p][%s][%s] IsClone[%d] newCoors[%f %f %f] triggerNetworkRespawnEvent[%d]",pPlayer,pPlayer ? pPlayer->GetModelName() : "",GetLogName(), IsClone(), newCoors.x,newCoors.y,newCoors.z,triggerNetworkRespawnEvent);

    if (AssertVerify(pPlayer) && AssertVerify(pPlayer->IsPlayer()) && AssertVerify(!IsClone()))
    {
		//Reset last damage event
		SetLastDamageEventPending( true );

		//Ensure the wanted level is cleared on resurrect - there are some spurious script events that are coming through under some situations that can end up setting the wanted level incorrectly.
		//When a player resurrects he should be clear of wanted level - ensure this is correct. lavalley.
		CWanted* pWanted = pPlayer->GetPlayerWanted();
		if (pWanted && (pWanted->GetWantedLevel() > WANTED_CLEAN))
		{
			pWanted->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_REMOVING);
		}

		pPlayer->GetPedIntelligence()->BuildQueriableState();

        UpdateLocalTaskSlotCacheForSync();

        // force all the states altered by the resurrection to be sent together in the next update to other players
        if(GetSyncData())
        {
			GetSyncTree()->Update(this, GetActivationFlags(), netInterface::GetSynchronisationTime());
            GetSyncTree()->ForceSendOfSyncUpdateNodes(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetActivationFlags(), this);
        }

		//---------------

		const atArray<CPedScarNetworkData> *localScarData = 0;
		PEDDAMAGEMANAGER.GetScarDataForLocalPed(pPlayer, localScarData);

		bool bHasScarData = false;
		float u = 0.f, v = 0.f, scale = 0.f;

		if(localScarData && localScarData->GetCount())
		{
			u32 scarCount = (u32) localScarData->GetCount();

			if (GetSentScarCount() != scarCount)
			{
				bHasScarData = true;

				const CPedScarNetworkData &scarDataFrom = (*localScarData)[0];

				u = scarDataFrom.uvPos.x;
				v = scarDataFrom.uvPos.y;
				scale = scarDataFrom.scale;

				SetSentScarCount(scarCount);
			}
		}

		//---------------

		CRespawnPlayerPedEvent::Trigger(newCoors, triggerNetworkRespawnEvent ? m_RespawnNetObjId : NETWORK_INVALID_OBJECT_ID, NetworkInterface::GetNetworkTime(), true, false, HasMoney(), bHasScarData, u, v, scale);
    }

	ResetDamageDealtByAllPlayers();

    m_lastResurrectionTime = CNetwork::GetSyncedTimeInMilliseconds();

	m_killedWithHeadShot = false;
	m_killedWithMeleeDamage = false;
}

void  CNetObjPlayer::SetSpectatorObjectId(const ObjectId id)
{
    m_SpectatorObjectId = NETWORK_INVALID_OBJECT_ID;

    if (id != NETWORK_INVALID_OBJECT_ID)
    {
        m_SpectatorObjectId = id;
    }
}

void  CNetObjPlayer::SetAntagonisticPlayerIndex(PhysicalPlayerIndex antagonisticPlayerIndex)
{
    m_AntagonisticPlayerIndex = INVALID_PLAYER_INDEX;

    if (antagonisticPlayerIndex != INVALID_PLAYER_INDEX)
    {
        gnetAssertf(antagonisticPlayerIndex < MAX_NUM_PHYSICAL_PLAYERS, "Invalid index \"%d\"", antagonisticPlayerIndex);
        m_AntagonisticPlayerIndex = antagonisticPlayerIndex;
    }
}

bool  CNetObjPlayer::IsAntagonisticToThisPlayer(const CNetObjPlayer * pNetObjPlayer)
{
    gnetAssert(pNetObjPlayer);
	if (!pNetObjPlayer)
		return false;

    if( (GetAntagonisticPlayerIndex() == pNetObjPlayer->GetPhysicalPlayerIndex()) ||
        (pNetObjPlayer->GetAntagonisticPlayerIndex() == GetPhysicalPlayerIndex()) )
    {
        return true;
    }

    return false;
}

#if __DEV

void CNetObjPlayer::TargetCloningInformation::DebugPrint(Vector3 const& pos) const
{
	char buffer[2048] = "\0";
	formatf(buffer, 2048, "isAiming = %d\nwasAiming = %d\nlockOnTargetAimOffset = %f %f %f\nCurrentAimTargetPos = %f %f %f\nCurrentAimTargetPed = %p\nCurrentWeaponHash = %d\ntargetMode = %d\nFreeAimLockedOnTarget = %d\nCurrentAimTargetID = %d",
	m_IsAiming, m_WasAiming,
	m_lockOnTargetAimOffset.x, m_lockOnTargetAimOffset.y, m_lockOnTargetAimOffset.z, 
	m_CurrentAimTargetPos.x, m_CurrentAimTargetPos.y, m_CurrentAimTargetPos.z,
	m_CurrentAimTargetEntity.Get(),
	m_CurrentWeaponHash,
	m_targetMode,
	m_FreeAimLockedOnTarget,
	m_CurrentAimTargetID);

	grcDebugDraw::Text(pos, Color_white, buffer, true);
}

#endif /* __DEV */

void CNetObjPlayer::TargetCloningInformation::Clear(void)
{
	m_CurrentPlayerPos			= VEC3_ZERO;
	m_CurrentAimTargetPos		= VEC3_ZERO;
	m_CurrentAimTargetEntity	= NULL;
	m_CurrentAimTargetID		= NETWORK_INVALID_OBJECT_ID;
	m_CurrentWeaponHash			= 0;
	m_lockOnTargetAimOffset		= VEC3_ZERO;
	m_targetMode				= TargetMode_Invalid;
	m_FreeAimLockedOnTarget		= false;		
	m_IsAiming					= false;
	m_WasAiming					= false;
}

void CNetObjPlayer::TargetCloningInformation::SetAimTarget(CPed const* playerPed, Vector3 const& targetPosWS)
{
	Clear();

	if(playerPed && playerPed->GetWeaponManager())
	{
		m_CurrentWeaponHash			= playerPed->GetWeaponManager()->GetEquippedWeaponHash();
		m_CurrentPlayerPos			= VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
		m_CurrentAimTargetPos		= targetPosWS;
		m_CurrentAimTargetEntity	= NULL;
		m_FreeAimLockedOnTarget		= false;
		m_targetMode				= TargetMode_PosWS;
	}
}

void CNetObjPlayer::TargetCloningInformation::SetAimTarget(CPed const* playerPed, const CEntity& targetEntity)
{
	gnetAssertf(targetEntity.GetIsDynamic(), "CNetObjPlayer::TargetCloningInformation::SetAimTarget : targetEntity is not dynamic - won't be able to get network ID from it for target info cloning?!" );

	Clear();

	if(playerPed && playerPed->GetWeaponManager())
	{
		m_CurrentWeaponHash			= playerPed->GetWeaponManager()->GetEquippedWeaponHash();
		m_CurrentPlayerPos			= VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
		m_CurrentAimTargetEntity	= (CEntity*)&targetEntity;

		// We store the position here too as the entity we're targetting might fall outside our max range for passing an entity and an offset so we have to revert 
		// to sending the positional offset from the player pos to the target pos instead...
		m_CurrentAimTargetPos		= VEC3V_TO_VECTOR3(targetEntity.GetTransform().GetPosition());

		// we need to know if we're locked on to the target or free aim targetting it...
		const CPlayerPedTargeting & rPlayerTargeting = playerPed->GetPlayerInfo()->GetTargeting();
		m_FreeAimLockedOnTarget		= rPlayerTargeting.GetFreeAimTarget() == m_CurrentAimTargetEntity || rPlayerTargeting.GetFreeAimTargetRagdoll() == m_CurrentAimTargetEntity; 	
		
		m_targetMode				= TargetMode_Entity;
	}
}

ObjectId CNetObjPlayer::TargetCloningInformation::GetLockOnTargetID(CPed const* playerPed) const
{
	ObjectId objectID = NETWORK_INVALID_OBJECT_ID;

	if(playerPed && playerPed->GetPlayerInfo())
	{
		CPlayerPedTargeting & rPlayerTargeting = playerPed->GetPlayerInfo()->GetTargeting();
		CEntity const* lockOnTarget			= rPlayerTargeting.GetLockOnTarget();

		if(NetworkUtils::GetNetworkObjectFromEntity(lockOnTarget))
		{
			objectID = NetworkUtils::GetNetworkObjectFromEntity(lockOnTarget)->GetObjectID();
		}
	}

	return objectID;
}

unsigned CNetObjPlayer::TargetCloningInformation::GetLockOnTargetState(CPed const* playerPed) const
{
	unsigned lockOnState =  static_cast<unsigned>(CEntity::HLOnS_NONE);

	if(playerPed && playerPed->GetPlayerInfo())
	{
		CPlayerPedTargeting & rPlayerTargeting = playerPed->GetPlayerInfo()->GetTargeting();
		switch(rPlayerTargeting.GetOnFootHomingLockOnState(playerPed))
		{
		case CPlayerPedTargeting::OFH_ACQUIRING_LOCK_ON:
			lockOnState = static_cast<unsigned>(CEntity::HLOnS_ACQUIRING);
			break;
		case CPlayerPedTargeting::OFH_LOCKED_ON:
			lockOnState = static_cast<unsigned>(CEntity::HLOnS_ACQUIRED);
			break;
		default:
			lockOnState = static_cast<unsigned>(CEntity::HLOnS_NONE);
			break;
		}
	}

	return lockOnState;
}

void CNetObjPlayer::SetAimTarget(Vector3& targetPos)
{
	gnetAssertf(GetPlayerPed() && !GetPlayerPed()->IsNetworkClone(), "ERROR : Should not be setting aim target on network clone! / non player!");

	m_TargetCloningInfo.SetAimTarget(GetPlayerPed(), targetPos);
}

void CNetObjPlayer::SetAimTarget(const CEntity& targetEntity)
{
	gnetAssertf(GetPlayerPed() && !GetPlayerPed()->IsNetworkClone(), "ERROR : Should not be setting aim target on network clone! / non player!");
	m_TargetCloningInfo.SetAimTarget(GetPlayerPed(), targetEntity);
}

void CNetObjPlayer::SetLookAtTalkers(bool bLookAtTalkers)
{
	ms_bLookAtTalkers = bLookAtTalkers;
}

void CNetObjPlayer::LookAtNearestTalkingPlayer()
{
	CPed* pPed = GetPlayerPed();
	if(!gnetVerifyf(pPed, "LookAtNearestTalkingPlayer :: No player ped!"))
		return;

	CPlayerInfo* pInfo = pPed->GetPlayerInfo();

	// ignore players in cutscenes
	if(pInfo && pInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
		return;

    // not when ragdolling
    if(pPed->GetUsingRagdoll())
        return; 

    // not when aiming
    if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun))
        return;
        
	// information on the passed player
	bool bPlayerTalking = GetPlayerOwner() && NetworkInterface::GetVoice().IsGamerTalking(GetPlayerOwner()->GetGamerInfo().GetGamerId());
	
    // get current vehicle
    CVehicle* pPlayerVehicle = pPed->GetIsInVehicle() ? pPed->GetMyVehicle() : NULL;

    // no look at when driving
	bool bPlayerDriving = (pPlayerVehicle && pPlayerVehicle->GetDriver() == pPed);
    if(bPlayerDriving)
        return;

	// disable in turret seats
	if (pPlayerVehicle)
	{
		const CSeatManager* pSeatMgr = pPlayerVehicle->GetSeatManager();
		s32 iSeatIndex = pSeatMgr ? pSeatMgr->GetPedsSeatIndex(pPed) : -1;
		if (pPlayerVehicle->IsTurretSeat(iSeatIndex))
		{
			return;
		}
	}

    // grab position
	Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// tracking of 'best' target
	CPed* pTargetPlayer = NULL;
	float fTargetDist = 0.0f;
	bool bTargetPlayerInSameCar = false;
	bool bTargetPlayerTalking = false;

	static float LOOK_AT_DIST_ON_FOOT = 4.0f * 4.0f;	// max look at dist between 2 players on foot
	static float LOOK_AT_DIST_IN_VEH = 2.0f * 2.0f;		// max look at dist between 2 players if at least one of them is in a car
	static float LOOK_AT_DIST_Z = 2.0f;					// max look at height difference

    unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
		const CNetGamePlayer* pOtherPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

		CPed* pOtherPed = pOtherPlayer->GetPlayerPed();
		if(pOtherPed && pOtherPed != pPed)
		{
			bool bOtherPlayerInCutscene = (pOtherPlayer->GetPlayerInfo() && pOtherPlayer->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE);
			if(bOtherPlayerInCutscene)
				continue; 

			bool bOtherPlayerTalking = NetworkInterface::GetVoice().IsGamerTalking(pOtherPlayer->GetGamerInfo().GetGamerId());
			bool bEitherTalking = (bPlayerTalking || bOtherPlayerTalking);

			if(bEitherTalking && !pOtherPlayer->IsInDifferentTutorialSession())
			{
				CVehicle* pOtherPlayerVehicle = pOtherPed->GetIsInVehicle() ? pOtherPed->GetMyVehicle() : NULL;
                bool bPlayerInSameCar = pOtherPlayerVehicle && pOtherPlayerVehicle == pPlayerVehicle;

                // don't look at talking players in other cars.
				if(pOtherPlayerVehicle && !bPlayerInSameCar)
					continue;
				
				// check if this player is not a priority
				bool bIgnoreThisPlayer = (!bOtherPlayerTalking && bTargetPlayerTalking) || (!bPlayerInSameCar && bTargetPlayerInSameCar);
				if(bIgnoreThisPlayer)
                    continue;

				Vector3 vOtherPlayerPos = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());
				float fDistance = (vPlayerPos - vOtherPlayerPos).XYMag2();

				// ignore if outside target distance
                float fLookAtDist = (pPlayerVehicle || pOtherPlayerVehicle) ? LOOK_AT_DIST_IN_VEH : LOOK_AT_DIST_ON_FOOT;
                if(fDistance > fLookAtDist)
					continue; 

				// ignore if outside target z distance
				float fzDistance = fabs(vPlayerPos.z - vOtherPlayerPos.z);
				if(fzDistance > LOOK_AT_DIST_Z)
					continue; 

				// check if current player is preferable
				bool bFavourThisPlayer = (bOtherPlayerTalking && !bTargetPlayerTalking) || (bPlayerInSameCar && !bTargetPlayerInSameCar);
				if(!bFavourThisPlayer && (pTargetPlayer && fDistance >= fTargetDist))
					continue;

				// we have a candidate
				pTargetPlayer = pOtherPed;
				fTargetDist = fDistance;
				bTargetPlayerInSameCar = bPlayerInSameCar;
				bTargetPlayerTalking = bOtherPlayerTalking;
			}
		}
	}

	// if we have a target
	if(pTargetPlayer)
	{
		// Reset the start delay timer only if the look at time has expired. If there's still look at time remaining, keep the look IK solver
		// blended in if the same player is still talking (instead of blending the solver out and blending it back in which looks bad since
		// the head will turn away and then back to the player). Even if a different player is talking, keep the solver blended in to allow
		// it to interpolate to the new target naturally.
		if (m_nLookAtTime <= 0)
		{
			m_nLookAtStartDelayTime = static_cast<s16>(fwRandom::GetRandomNumberInRange(0, 1000));
		}
		m_nLookAtTime = static_cast<s16>(fwRandom::GetRandomNumberInRange(1500, 3000));
		m_pLookAtTarget = pTargetPlayer;

		// drivers look at chatting players less often
		m_nLookAtIntervalTimer = bPlayerDriving ? static_cast<s16>(fwRandom::GetRandomNumberInRange(3000, 4000)) : TIME_BETWEEN_LOOK_AT_CHECKS;
	}
	else // refresh sooner if we're currently talking
	{
		m_nLookAtIntervalTimer = bPlayerTalking ? TIME_BETWEEN_LOOK_AT_CHECKS : (TIME_BETWEEN_LOOK_AT_CHECKS * 2);
	}
}

void CNetObjPlayer::UpdateAnimVehicleStreamingTarget()
{
    m_TargetVehicleIDForAnimStreaming = NETWORK_INVALID_OBJECT_ID;
    m_TargetVehicleEntryPoint         = -1;

    CPed *playerPed = GetPlayerPed();

    if(playerPed && playerPed->IsLocalPlayer())
    {
        aiTask *task = playerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);

        if(task)
	    {
		    const CVehicleClipRequestHelper &clipRequestHelper = playerPed->GetPlayerInfo()->GetVehicleClipRequestHelper();

            const CVehicle *targetVehicle = clipRequestHelper.GetTargetVehicle();

            if(targetVehicle && targetVehicle->GetNetworkObject())
            {
                m_TargetVehicleIDForAnimStreaming = targetVehicle->GetNetworkObject()->GetObjectID();
                m_TargetVehicleEntryPoint         = clipRequestHelper.GetTargetEntryPointIndex();
            }
        }
    }
}

bool CNetObjPlayer::AllowFixByNetwork() const 
{ 
	if (IsConcealed())
	{
		return true;
	}

	// local players hidden in cutscenes need to be fixed
	if (IsClone() || ShouldObjectBeHiddenForCutscene())
	{
		return true;
	}

	return false;
}

bool CNetObjPlayer::UseBoxStreamerForFixByNetworkCheck() const
{
    TUNE_FLOAT(USE_BOX_STREAMER_LOD_RANGE_SCALE_THRESHOLD, 1.5f, 1.0f, 10.0f, 0.1f);
    return CVehicleAILodManager::GetLodRangeScale() >= USE_BOX_STREAMER_LOD_RANGE_SCALE_THRESHOLD;
}

void CNetObjPlayer::UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason)
{
	CPed* pPlayer = GetPlayerPed();

	bool bInVehicle = pPlayer && pPlayer->GetIsInVehicle();

    if(fixByNetwork && !bInVehicle && GetPlayerOwner() && GetPlayerOwner()->GetPlayerInfo() && GetPlayerOwner()->GetPlayerInfo()->GetPlayerState() != CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
    {
        // if we've moved into fix by network range, make the player invisible
        SetOverridingLocalVisibility(false, NetworkUtils::GetFixedByNetworkReason(reason));
    }

    if(ShouldFixAndDisableCollisionDueToControls())
    {
        if(!ShouldObjectBeHiddenForTutorial())
        {
			reason = FBN_PLAYER_CONTROLS_DISABLED;
            fixByNetwork = true;
        }
    }

    CNetObjPed::UpdateFixedByNetwork(fixByNetwork, reason, npfbnReason);
}

// Name         :   CheckAndStartClonePlayerFadeOut
// Purpose      :   If the player state has been changed to PLAYERSTATE_RESPAWN we need to start fade out.
void CNetObjPlayer::CheckAndStartClonePlayerFadeOut(const int previousState, const int newState)
{
	if (previousState != (int)CPlayerInfo::PLAYERSTATE_PLAYING)
		return;

	if (newState != (int)CPlayerInfo::PLAYERSTATE_RESPAWN)
		return;

	if (m_FadePlayerToZero)
		return;

	CPed* pPlayer = GetPlayerPed();
	gnetAssert(pPlayer);

	if(IsClone() && pPlayer)
	{
		bool overrideBlender = false;

		if (!pPlayer->GetLodData().GetFadeToZero())
		{
			if (IsVisibleToScript() && pPlayer->IsVisible() && pPlayer->GetLodData().IsVisible())
			{
				m_MaxPlayerFadeOutTime = fwTimer::GetSystemTimeInMilliseconds() + MAX_TIME_TO_FADE_PLAYER_OUT;

#if __BANK
				if (NetworkDebug::RemotePlayerFadeOutGetTimer())
				{
					m_MaxPlayerFadeOutTime = fwTimer::GetSystemTimeInMilliseconds() + NetworkDebug::RemotePlayerFadeOutGetTimer();
				}
#endif // __BANK

				m_FadePlayerToZero = true;
				overrideBlender    = true;
				FadeOutPed(true);
			}
		}

		//Reject position changes until fade is done.
		if (overrideBlender)
		{
			NetworkInterface::OverrideNetworkBlenderForTime(pPlayer, TIME_TO_DELAY_BLENDING);
			SetOverridingLocalVisibility(true, "CNetObjPlayer::UpdateRemotePlayerFade");
		}
	}
}

// Name         :   UpdateRemotePlayerFade
// Purpose      :   During Player Respawn State the player is faded out to 0
void CNetObjPlayer::UpdateRemotePlayerFade()
{
	CPed* pPlayer = GetPlayerPed();
	gnetAssert(pPlayer);

	if(IsClone() && pPlayer)
	{
		CPlayerInfo* pi = pPlayer->GetPlayerInfo();
		if (pi)
		{
			bool overrideBlender = false;

			if (m_FadePlayerToZero)
			{
				overrideBlender = IsFadingOut();
				const u32 currTime = fwTimer::GetSystemTimeInMilliseconds();

				//We have just started this
				if (m_MaxPlayerFadeOutTime == currTime + MAX_TIME_TO_FADE_PLAYER_OUT)
				{
					overrideBlender = true;
				}
#if __BANK
				if (NetworkDebug::RemotePlayerFadeOutGetTimer())
				{
					if (m_MaxPlayerFadeOutTime < currTime)
					{
						m_MaxPlayerFadeOutTime = 0;
						overrideBlender = m_FadePlayerToZero = false;
						FadeOutPed(false);
					}
				}
				else
#endif // __BANK
				if ((!overrideBlender && pi->GetPlayerState() != CPlayerInfo::PLAYERSTATE_RESPAWN) || m_MaxPlayerFadeOutTime < currTime)
				{
					m_MaxPlayerFadeOutTime = 0;
					overrideBlender = m_FadePlayerToZero = false;
					FadeOutPed(false);
				}
			}

			//Reject position changes until fade is done.
			if (overrideBlender)
			{
				NetworkInterface::OverrideNetworkBlenderForTime(pPlayer, TIME_TO_DELAY_BLENDING);
				SetOverridingLocalVisibility(true, "CNetObjPlayer::UpdateRemotePlayerFade");
			}
		}
	}
}

bool CNetObjPlayer::CanShowGhostAlpha() const
{
	CPed* pPlayerPed = GetPlayerPed();

	bool bShowLocalPlayerAsGhosted = GetInvertGhosting();

	if (pPlayerPed && pPlayerPed->IsLocalPlayer())
	{
		return bShowLocalPlayerAsGhosted;
	}

	if (NetworkInterface::IsSpectatingPed(pPlayerPed))
	{
		return bShowLocalPlayerAsGhosted;
	}

	if (bShowLocalPlayerAsGhosted)
	{
		return false;
	}

	return CNetObjPed::CanShowGhostAlpha();
}

void CNetObjPlayer::ProcessGhosting()
{	
	CPed* pPlayerPed = GetPlayerPed();

	if (!IsClone())
	{
		if (m_bGhost)
		{
			if (pPlayerPed)
			{
				CVehicle* pVehicle = pPlayerPed->GetMyVehicle();

				// make the vehicle the player is driving a ghost too
				if (pVehicle && pVehicle->GetDriver() == pPlayerPed && !NetworkUtils::IsNetworkCloneOrMigrating(pVehicle) && pVehicle->GetNetworkObject())
				{
					static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->SetAsGhost(true BANK_ONLY(, SPG_PLAYER_VEHICLE, m_ghostScriptInfo));
				}
			}
		}
		else if (CNetObjPhysical::GetInvertGhosting() && CanShowGhostAlpha() && CNetworkGhostCollisions::IsLocalPlayerInAnyGhostCollision())
		{
			ShowGhostAlpha();
		}

		if (ms_ghostScriptId.IsValid())
		{
			if (scriptInterface::IsScriptRegistered(ms_ghostScriptId))
			{
				// we set the ghost flags for non-participants
				m_ghostPlayerFlags = ~scriptInterface::GetParticipants(ms_ghostScriptId);
				SetAsGhost(m_ghostPlayerFlags != 0 BANK_ONLY(, SPG_NON_PARTICIPANTS, m_ghostScriptInfo));
			}
			else
			{
				ResetPlayerGhostedForNonScriptPartipants(BANK_ONLY(m_ghostScriptInfo));
			}
		}
	}
	else 
	{
		if (CanShowGhostAlpha())
		{
			// show the player as ghosted if it will be treated as a ghost collision due to the local player being a ghost
			if (pPlayerPed && CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(*pPlayerPed))
			{
				ShowGhostAlpha();
			}
		}
	}

	CNetObjPed::ProcessGhosting();
}

void CNetObjPlayer::SetPlayerGhostedForNonScriptPartipants(CGameScriptId& scriptId BANK_ONLY(, const char* scriptInfo))
{
	if (AssertVerify(!IsClone()))
	{
		if (ms_ghostScriptId.IsValid() && ms_ghostScriptId != scriptId)
		{
			gnetAssertf(0, "Only one script can call SET_NON_PARTICIPANTS_OF_THIS_SCRIPT_AS_GHOSTS at once, %s is trying to set this, when %s already has it set", scriptId.GetLogName(), ms_ghostScriptId.GetLogName());
			return;
		}

		ms_ghostScriptId = scriptId;

		if (AssertVerify(scriptInterface::IsScriptRegistered(ms_ghostScriptId)))
		{
			// we set the ghost flags for non-participants
			m_ghostPlayerFlags = ~scriptInterface::GetParticipants(ms_ghostScriptId);

			SetAsGhost(m_ghostPlayerFlags != 0 BANK_ONLY(, SPG_RESET_GHOSTING_NON_PARTICIPANTS, scriptInfo));
		}
	}
}

void CNetObjPlayer::ResetPlayerGhostedForNonScriptPartipants(BANK_ONLY(const char* scriptInfo))
{
	ms_ghostScriptId.Invalidate();
	m_ghostPlayerFlags = 0;
	SetAsGhost(false BANK_ONLY(, SPG_RESET_GHOSTING, scriptInfo));
}

void CNetObjPlayer::ForceSendPlayerAppearanceSyncNode()
{
	if (HasSyncData())
	{
		CPlayerSyncTree* pSyncTree = SafeCast(CPlayerSyncTree, GetSyncTree());
		if (pSyncTree)
		{
			pSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetActivationFlags(), this, *pSyncTree->GetPlayerAppearanceNode());
		}
	}
}

void CNetObjPlayer::UpdateLookAt()
{
	CPed* pPlayerPed = GetPlayerPed();
	gnetAssert(pPlayerPed);

	// manage looking at other talking players
	if(ms_bLookAtTalkers && !m_bIsCloneLooking)
	{
		if(m_nLookAtStartDelayTime > 0)
		{
			m_nLookAtStartDelayTime -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());
		}
		else if(m_nLookAtIntervalTimer > 0)
		{
			m_nLookAtIntervalTimer -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());
		}

		// get chatting players to look at each other
		if(m_nLookAtIntervalTimer <= 0 && NetworkInterface::GetVoice().IsAnyGamerTalking())
		{
			LookAtNearestTalkingPlayer();
		}
	}

	// manage current look at
	if(m_nLookAtStartDelayTime <= 0 && m_nLookAtTime > 0)
	{
		m_nLookAtTime -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());

		bool bStopLooking = false;

		// if time has expired
		if(m_nLookAtTime <= 0)
			bStopLooking = true;
		// if look at talkers has been toggled off
		else if(!ms_bLookAtTalkers)
			bStopLooking = true;
		// if target ped has bailed
		else if(!m_pLookAtTarget)
			bStopLooking = true;
		// if look at is being overridden by owning player
		else if(m_bIsCloneLooking)
			bStopLooking = true;
		// if the player has started aiming
		else if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun))
			bStopLooking = true;

		// if we should stop looking, kill the tracking variables
		if(bStopLooking)
		{
			m_pLookAtTarget = NULL;
			m_nLookAtTime = 0;
		}
	}

	if(m_bIsCloneLooking || (m_pLookAtTarget && m_nLookAtStartDelayTime <= 0) )
	{
		static u16 nLookTime = 100;
		CIkRequestBodyLook sLookAtRequest;
		sLookAtRequest.SetHoldTime(nLookTime);
		sLookAtRequest.SetRequestPriority(CIkManager::IK_LOOKAT_HIGH);
		sLookAtRequest.SetTurnRate(LOOKIK_TURN_RATE_NORMAL);
		sLookAtRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
		sLookAtRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);
		sLookAtRequest.SetBlendInRate(LOOKIK_BLEND_RATE_NORMAL);
		sLookAtRequest.SetRefDirHead(LOOKIK_REF_DIR_FACING);
		sLookAtRequest.SetRefDirNeck(LOOKIK_REF_DIR_FACING);
		sLookAtRequest.SetFlags(LOOKIK_USE_FOV);

		// get our target position
		if(m_bIsCloneLooking)
		{
			sLookAtRequest.SetHashKey(gs_nCloneLookAtHash);

			// LookAt position is sync'd relative to the player
			sLookAtRequest.SetTargetOffset(VECTOR3_TO_VEC3V(m_vLookAtPosition));
			sLookAtRequest.SetTargetEntity(pPlayerPed);
		}
		else
		{
			sLookAtRequest.SetHashKey(gs_nTalkersLookAtHash);
			sLookAtRequest.SetTargetEntity(m_pLookAtTarget.Get());
			sLookAtRequest.SetTargetBoneTag(BONETAG_HEAD);
			sLookAtRequest.SetTargetOffset(Vec3V(V_ZERO));
		}

		pPlayerPed->GetIkManager().Request(sLookAtRequest);
	}
}

void CNetObjPlayer::UpdateRemotePlayerAppearance()
{
	CPed* pPlayer = GetPlayerPed();

	if (pPlayer && pPlayer->GetPlayerInfo() && GetPlayerOwner())
	{
		// information on the passed player
		bool bPlayerTalking = NetworkInterface::GetVoice().IsGamerTalking(GetPlayerOwner()->GetGamerInfo().GetGamerId());

		pPlayer->GetPlayerInfo()->SetEnableBlueToothHeadset(bPlayerTalking);
	}
}

bool CNetObjPlayer::ShouldObjectBeHiddenForTutorial()  const
{
	// always show the local player
	if(!IsClone())
		return false;

	// if in a different tutorial session, hide this player
	CNetGamePlayer* thisPlayer = GetPlayerOwner();
	if(thisPlayer && thisPlayer->IsInDifferentTutorialSession())
		return true;

    if(IsConcealed())
        return true;

	// don't call up
	return false;
}

void CNetObjPlayer::HideForTutorial()
{
	if(!gnetVerifyf(IsClone(), "HideInTutorial should only be called on remote players!"))
		return;

	CPed* pPlayerPed = GetPlayerPed();
	gnetAssert(pPlayerPed);

	if(pPlayerPed && !pPlayerPed->GetIsInVehicle() && !pPlayerPed->GetIsAttached())
	{
		if(pPlayerPed->GetUsingRagdoll())
		{
			nmEntityDebugf(pPlayerPed, "CNetObjPlayer::HideForTutorial switching to animated");
			pPlayerPed->SwitchToAnimated();
		}

		// some aspects of game interact with invisible objects so park under the map
		float fHEIGHT_TO_HIDE_IN_TUTORIAL_SESSION = -190.0f;
		Vector3 vPosition = NetworkInterface::GetLastPosReceivedOverNetwork(pPlayerPed);
		vPosition.z = fHEIGHT_TO_HIDE_IN_TUTORIAL_SESSION;
		pPlayerPed->SetPosition(vPosition, true, true, true);
	}

	CNetObjPed::HideForTutorial();
}

void CNetObjPlayer::UpdateStats()
{
	if(IsClone())
	{
		return;
	}

	if(++ms_statUpdateCounter >= STAT_UPDATE_RATE)
	{
		//Unfortunately, this function can be hit while the player is still has their SP ped when entering MP.
		int characterSlot = StatsInterface::GetCharacterIndex();
		if(characterSlot < CHAR_MP_START || CHAR_MP_END < characterSlot)
		{
			return;
		}
		characterSlot -= CHAR_MP_START;

		const CPlayerCardXMLDataManager::PlayerCard& rCard = SPlayerCardXMLDataManager::GetInstance().GetPlayerCard(CPlayerCardXMLDataManager::PCTYPE_FREEMODE);
		bool needsUpdate = false;

		float newValue = 0;

		// Loops over all of the stats shown in the player card, and sets their values in the buffer.
		for(int i=PlayerCard::NETSTAT_START_PROFILESTATS; i<PlayerCard::NETSTAT_SYNCED_STAT_COUNT; ++i)
		{
			newValue = CalcStatValue(rCard, i, characterSlot);

			if(m_stats[i] != newValue)
			{
				needsUpdate = true;
			}

			m_stats[i] = newValue;
		}

		for(int i=PlayerCard::NETSTAT_START_TITLE_RATIO_STATS; i<=PlayerCard::NETSTAT_END_TITLE_RATIO_STATS; ++i)
		{
			newValue = CalcStatValue(rCard, i, characterSlot);

			if(m_stats[i] != newValue)
			{
				needsUpdate = true;
			}

			m_stats[i] = newValue;
		}

		{
			u32 val = (static_cast<u32>(CalcStatValue(rCard, PlayerCard::NETSTAT_PLANE_ACCESS, characterSlot)) << PlayerCard::NETSTAT_PLANE_ACCESS_BIT) +
				(static_cast<u32>(CalcStatValue(rCard, PlayerCard::NETSTAT_BOAT_ACCESS, characterSlot)) << PlayerCard::NETSTAT_BOAT_ACCESS_BIT) +
				(static_cast<u32>(CalcStatValue(rCard, PlayerCard::NETSTAT_CAR_ACCESS, characterSlot)) << PlayerCard::NETSTAT_CAR_ACCESS_BIT) +
				(static_cast<u32>(CalcStatValue(rCard, PlayerCard::NETSTAT_HELI_ACCESS, characterSlot)) << PlayerCard::NETSTAT_HELI_ACCESS_BIT) +
				(static_cast<u32>(CalcStatValue(rCard, PlayerCard::NETSTAT_TUT_COMPLETE, characterSlot)) << PlayerCard::NETSTAT_TUT_COMPLETE_BIT);

			// Hacky Magic - Bit packing into a float, so treating it as a u32.
			u32* statsAsInt = reinterpret_cast<u32*>(m_stats);

			if(statsAsInt[PlayerCard::NETSTAT_PACKED_BOOLS] != val)
			{
					needsUpdate = true;
			}

			statsAsInt[PlayerCard::NETSTAT_PACKED_BOOLS] = val;
		}

		if(needsUpdate || ms_forceStatUpdate)
		{
			//Should never be 0 after getting set for the first time.
			if(++m_statVersion == 0)
			{
				m_statVersion = 1;
			}

#if !__NO_OUTPUT
			if ((Channel_stat.FileLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_stat.TtyLevel >= DIAG_SEVERITY_DISPLAY))
			{
				for(int i=0; i<PlayerCard::NETSTAT_SYNCED_STAT_COUNT; ++i)
				{
					statDisplayf("CNetObjPlayer::UpdateStats - stat %d = %f", i, m_stats[i]);
				}
			}
#endif // !__NO_OUTPUT

			CPlayerCardStatEvent::Trigger(m_stats);
		}

		ms_statUpdateCounter = 0;
		ms_forceStatUpdate = false;
	}
}

float CNetObjPlayer::CalcStatValue(const CPlayerCardXMLDataManager::PlayerCard& rCard, int statIndex, int characterSlot)
{
	float retVal = 0;

	PlayerCard::eNetworkedStatIds statId = static_cast<PlayerCard::eNetworkedStatIds>(statIndex);
	u32 statHash = rCard.GetStatHashByNetId(statId, characterSlot, false);
	u32 extraStatHash = rCard.GetStatHashByNetId(statId, characterSlot, true);

	if(StatsInterface::IsKeyValid(statHash))
	{
		if(StatsInterface::GetIsBaseType(statHash, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(statHash, STAT_TYPE_FLOAT))
		{
			float num = CalcStatValueHelper(statHash);

			if(extraStatHash && statHash != extraStatHash)
			{
				float denom = CalcStatValueHelper(extraStatHash);
				if(denom > 0)
				{
					num /= denom;
				}
			}

			retVal = num;
		}
		else if(StatsInterface::GetIsBaseType(statHash, STAT_TYPE_BOOLEAN))
		{
			retVal = StatsInterface::GetBooleanStat(statHash) ? 1.0f : 0.0f;
		}
	}

	return retVal;
}

float CNetObjPlayer::CalcStatValueHelper(u32 statHash)
{
	float retVal = 0.0f;

	if(StatsInterface::IsKeyValid(statHash))
	{
		if(StatsInterface::GetIsBaseType(statHash, STAT_TYPE_INT))
		{
			retVal = static_cast<float>(StatsInterface::GetIntStat(statHash));
		}
		else if(StatsInterface::GetIsBaseType(statHash, STAT_TYPE_FLOAT))
		{
			// Floats only need the first 2 decimal points.
			retVal = static_cast<float>(StatsInterface::GetFloatStat(statHash));
		}
	}

	return retVal;
}

float CNetObjPlayer::GetStatValue(PlayerCard::eNetworkedStatIds stat)
{
	if(0 <= stat && stat < PlayerCard::NETSTAT_SYNCED_STAT_COUNT)
	{
		return m_stats[stat];
	}
	else
	{
		bool isPackedBool = true;
		u32 packedBit = 0;

		if(stat == PlayerCard::NETSTAT_CAR_ACCESS)
		{
			packedBit = PlayerCard::NETSTAT_CAR_ACCESS_BIT;
		}
		else if(stat == PlayerCard::NETSTAT_PLANE_ACCESS)
		{
			packedBit = PlayerCard::NETSTAT_PLANE_ACCESS_BIT;
		}
		else if(stat == PlayerCard::NETSTAT_HELI_ACCESS)
		{
			packedBit = PlayerCard::NETSTAT_HELI_ACCESS_BIT;
		}
		else if(stat == PlayerCard::NETSTAT_BOAT_ACCESS)
		{
			packedBit = PlayerCard::NETSTAT_BOAT_ACCESS_BIT;
		}
		else if(stat == PlayerCard::NETSTAT_TUT_COMPLETE)
		{
			packedBit = PlayerCard::NETSTAT_TUT_COMPLETE_BIT;
		}
		else
		{
			isPackedBool = false;
		}

		if(isPackedBool)
		{
			// Get bit packed values out of a float.
			u32* statsAsInt = reinterpret_cast<u32*>(m_stats);
			return (statsAsInt[PlayerCard::NETSTAT_PACKED_BOOLS] & (1<<packedBit)) ? 1.0f : 0.0f;
		}
	}

	return 0.0f;
}

void CNetObjPlayer::ReceivedStatEvent(const float data[CPlayerCardXMLDataManager::PlayerCard::NETSTAT_SYNCED_STAT_COUNT])
{
	//Should never be 0 after getting set for the first time.
	if(++m_statVersion == 0)
	{
		m_statVersion = 1;
	}

	memcpy(m_stats, data, sizeof(m_stats));

#if !__NO_OUTPUT
	if ((Channel_stat.FileLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_stat.TtyLevel >= DIAG_SEVERITY_DISPLAY))
	{
		for(int i=0; i<PlayerCard::NETSTAT_SYNCED_STAT_COUNT; ++i)
		{
			statDisplayf("CNetObjPlayer::ReceivedStatEvent - stat %d = %f", i, m_stats[i]);
		}
	}
#endif
}

void CNetObjPlayer::GetScarAndBloodData(unsigned &numBloodMarks, CPedBloodNetworkData bloodMarkData[CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED],
                                        unsigned &numScars,      CPedScarNetworkData  scarData[CPlayerCreationDataNode::MAX_SCARS_SYNCED]) const
{
    CPed* pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

    const atArray<CPedBloodNetworkData> *bloodData = 0;
	PEDDAMAGEMANAGER.GetBloodDataForLocalPed(pPlayer, bloodData);

	numBloodMarks = 0;

	if(bloodData)
	{
		for(int index = 0; index < bloodData->GetCount() && index < CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED; index++)
		{
			const CPedBloodNetworkData &bloodDataFrom = (*bloodData)[index];
			CPedBloodNetworkData       &bloodDataTo  = bloodMarkData[index];

			bloodDataTo.age				= bloodDataFrom.age;
			bloodDataTo.bloodNameHash	= bloodDataFrom.bloodNameHash;
			bloodDataTo.rotation		= bloodDataFrom.rotation;
			bloodDataTo.scale			= bloodDataFrom.scale;
			bloodDataTo.uvPos			= bloodDataFrom.uvPos;
			bloodDataTo.uvFlags			= bloodDataFrom.uvFlags;
			bloodDataTo.zone			= bloodDataFrom.zone;

			numBloodMarks++;
		}
	}
	
    const atArray<CPedScarNetworkData> *localScarData = 0;
    PEDDAMAGEMANAGER.GetScarDataForLocalPed(pPlayer, localScarData);

    numScars = 0;

    if(localScarData)
    {
        for(int index = 0; index < localScarData->GetCount() && index < CPlayerCreationDataNode::MAX_SCARS_SYNCED; index++)
        {
            const CPedScarNetworkData &scarDataFrom = (*localScarData)[index];
            CPedScarNetworkData       &scarDataTo   = scarData[index];

            scarDataTo.uvPos        = scarDataFrom.uvPos;
			scarDataTo.uvFlags		= scarDataFrom.uvFlags;
            scarDataTo.rotation     = scarDataFrom.rotation;
            scarDataTo.scale        = scarDataFrom.scale;
            scarDataTo.forceFrame   = scarDataFrom.forceFrame;
			scarDataTo.age			= scarDataFrom.age;
            scarDataTo.scarNameHash = scarDataFrom.scarNameHash;
            scarDataTo.zone         = scarDataFrom.zone;

            numScars++;
        }
    }
}

void CNetObjPlayer::SetScarAndBloodData(unsigned numBloodMarks, const CPedBloodNetworkData bloodMarkData[CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED],
                                        unsigned numScars,      const CPedScarNetworkData  scarDataIn[CPlayerCreationDataNode::MAX_SCARS_SYNCED])
{
    CPed *pPlayer = GetPlayerPed();
    gnetAssert(pPlayer);

    if(pPlayer)
    {
        // add the player scars
        for(unsigned index = 0; index < numScars; index++)
        {
            const CPedScarNetworkData &scarData = scarDataIn[index];

            PEDDAMAGEMANAGER.AddPedScar(pPlayer, scarData.zone, scarData.uvPos, scarData.rotation, scarData.scale, scarData.scarNameHash, true, scarData.forceFrame, false, scarData.age, -1.0f, scarData.uvFlags);
        }

	    // add the blood marks
	    for(unsigned index = 0; index < numBloodMarks; index++)
	    {
		    const CPedBloodNetworkData &bloodData = bloodMarkData[index];

		    PEDDAMAGEMANAGER.AddPedBlood(pPlayer, bloodData.zone, bloodData.uvPos, bloodData.rotation, bloodData.scale, bloodData.bloodNameHash, false, bloodData.age, -1, false, bloodData.uvFlags);
	    }
    }
}

void CNetObjPlayer::GetSecondaryPartialAnimData(CSyncedPlayerSecondaryPartialAnim& pspaData)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);
	CTask* pTask = NULL;
	CTaskScriptedAnimation* pTaskScripted =NULL;

	pspaData.Reset(); //ensure this is fully reset otherwise we can get junk from last receive in the set - yikes

	if (pPed->IsPlayer() && pPed->GetPedIntelligence())
	{
		pTask = pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim();
		if (pTask && ( (pTask->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION && static_cast<CTaskScriptedAnimation*>(pTask)->GetState()==CTaskScriptedAnimation::State_Running) || pTask->GetTaskType()==CTaskTypes::TASK_MOVE_SCRIPTED || pTask->GetTaskType()==CTaskTypes::TASK_MOBILE_PHONE) )
		{
			pspaData.m_MoVEScripted			= false;
			pspaData.m_Active				= true;
			pspaData.m_SlowBlendDuration 	= false;

			if(pTask->GetTaskType()==CTaskTypes::TASK_MOVE_SCRIPTED)
			{
				pspaData.m_MoVEScripted = true;

				CTaskMoVEScripted* pTaskMOVEScripted = static_cast<CTaskMoVEScripted*>(pTask);
				pspaData.m_DictHash 			= pTaskMOVEScripted->GetCloneSyncDictHash();
				pspaData.m_MoveNetDefHash 		= pTaskMOVEScripted->GetCloneNetworkDefHash();
				pspaData.m_StatePartialHash 	= pTaskMOVEScripted->GetStatePartialHash();
				pspaData.m_SlowBlendDuration	= pTaskMOVEScripted->GetIsSlowBlend();

				pTaskMOVEScripted->GetCloneMappedFloatSignal(pspaData.m_FloatHash0,pspaData.m_Float0, 0);
				pTaskMOVEScripted->GetCloneMappedFloatSignal(pspaData.m_FloatHash1,pspaData.m_Float1, 1);
				pTaskMOVEScripted->GetCloneMappedFloatSignal(pspaData.m_FloatHash2,pspaData.m_Float2, 2);

				pTaskMOVEScripted->GetCloneMappedBoolSignal(atHashWithStringNotFinal("isBlocked"),pspaData.m_IsBlocked);
			}
			else if (pTask->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION)
			{
				pTaskScripted = static_cast<CTaskScriptedAnimation*>(pTask);

				pspaData.m_BoneMaskHash 		= pTaskScripted->GetBoneMaskHash();
				pspaData.m_DictHash 			= pTaskScripted->GetDict().GetHash();
				pspaData.m_ClipHash 			= pTaskScripted->GetClip().GetHash();
				pspaData.m_Flags				= pTaskScripted->GetFlagsForSingleAnim();
			}
			else if (pTask->GetTaskType()==CTaskTypes::TASK_MOBILE_PHONE)
			{
				CTaskMobilePhone* pTaskMobilePhone = static_cast<CTaskMobilePhone*>(pTask);
				if (pTaskMobilePhone)
				{
					pspaData.m_Phone = true;
					if (pTaskMobilePhone->GetIsPlayingAdditionalSecondaryAnim())
						pTaskMobilePhone->GetPlayerSecondaryPartialAnimData(pspaData);
				}
			}
		}
		else
		{
			CTaskPlayerOnFoot* pTask = static_cast<CTaskPlayerOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));

			if(pTask && pTask->GetState() == CTaskPlayerOnFoot::STATE_PLAYER_IDLES)
			{
				if(pTask->GetIdleWeaponBlocked())
				{
					if(pPed->GetWeaponManager())
					{
						CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
						const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

						if (pWeaponInfo)
						{
							const fwMvClipId ms_WeaponBlockedClipId(ATSTRINGHASH("wall_block_idle", 0x0f9b74953));
							const fwMvClipId ms_WeaponBlockedNewClipId(ATSTRINGHASH("wall_block", 0x0ea90630e));

							fwMvClipId wallBlockClipId = CLIP_ID_INVALID;
							const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
							if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedNewClipId))
							{
								wallBlockClipId = ms_WeaponBlockedNewClipId;
							}
							else if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedClipId))
							{
								wallBlockClipId = ms_WeaponBlockedClipId;
							}

							if(wallBlockClipId != CLIP_ID_INVALID) 
							{
								atHashString clipDictHash;
								atHashString clipHash;
								taskVerifyf(fwClipSetManager::GetClipDictionaryNameAndClipName(appropriateWeaponClipSetId, wallBlockClipId, clipDictHash, clipHash, false), "");

								pspaData.m_Active				= true;
								pspaData.m_SlowBlendDuration 	= true;
								pspaData.m_DictHash 			= clipDictHash.GetHash();
								pspaData.m_ClipHash 			= clipHash.GetHash();
								pspaData.m_Flags				= (1 << AF_SECONDARY) | (1 << AF_HOLD_LAST_FRAME);
							}
						}
					}
				}
			}
		}
	}

	//Update the secondary task sequence ID here - currently only needed for TaskScriptedAnimations see B*2203666
	if(pTaskScripted)
	{
		if(pTaskScripted != mSecondaryAnimData.m_pCurrentSecondaryTaskScripted )
		{
			//CTaskScriptedAnimation Task instance has changed for this player since last update
			//so cycle the sequence ID
			if (mSecondaryAnimData.m_SecondaryTaskSequenceID == MAX_SECONDARY_TASK_SEQUENCE_ID)
			{
				mSecondaryAnimData.m_SecondaryTaskSequenceID = 0;
			}
			else
			{
				mSecondaryAnimData.m_SecondaryTaskSequenceID++;
			}
		}

		pspaData.m_taskSequenceId = mSecondaryAnimData.m_SecondaryTaskSequenceID;
	}

	mSecondaryAnimData.m_pCurrentSecondaryTaskScripted = pTaskScripted;
}

void CNetObjPlayer::SetSecondaryPartialAnimData(const CSyncedPlayerSecondaryPartialAnim& pspaData)
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	// If the last synced update had not started playing because the dictionary needed to be loaded
	// then m_newSecondaryAnim could still be true when a new sync update comes through.
	// So before we check if this new sync is a repeat we need to cache m_newSecondaryAnim here
	// and if this sync update is a repeat (with same task sequence) we make sure to keep m_newSecondaryAnim true rather than clear it
	bool cacheNewSecondaryAnim = m_newSecondaryAnim; 

	if(pPed->IsPlayer() && pspaData.m_Active)
	{
		if (pspaData.m_Phone)
		{
			CTask* pTask = pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim();
			if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_MOBILE_PHONE)
			{
				CTaskMobilePhone* pTaskMobilePhone = static_cast<CTaskMobilePhone*>(pTask);
				if (pTaskMobilePhone)
				{
					u32 uCurrentHashKey = pTaskMobilePhone->GetIsPlayingAdditionalSecondaryAnim() ? pTaskMobilePhone->GetAdditionalSecondaryAnimHashKey() : 0;

					if (pspaData.m_PhoneSecondary && pspaData.m_ClipHash != 0)
					{
						if (pspaData.m_ClipHash != uCurrentHashKey)
						{
							pTaskMobilePhone->RemoteRequestAdditionalSecondaryAnims(pspaData);
						}
					}
					else if (uCurrentHashKey != 0)
					{
						pTaskMobilePhone->ClearAdditionalSecondaryAnimation();
					}
				}
			}
		}
		else
		{
			mSecondaryAnimData.m_bMoVEScripted						= pspaData.m_MoVEScripted;
			mSecondaryAnimData.m_SecondaryPartialAnimDictHash		= pspaData.m_DictHash;
			m_SecondaryPartialSlowBlendDuration						= pspaData.m_SlowBlendDuration;
			m_newSecondaryAnim = true;

			if(pspaData.m_MoVEScripted)
			{
				mSecondaryAnimData.MoveNetDefHash()		= pspaData.m_MoveNetDefHash;
				mSecondaryAnimData.StatePartialHash()	= pspaData.m_StatePartialHash;
				mSecondaryAnimData.FloatHash0()			= pspaData.m_FloatHash0;
				mSecondaryAnimData.FloatHash1()			= pspaData.m_FloatHash1;
				mSecondaryAnimData.FloatHash2()			= pspaData.m_FloatHash2;
				mSecondaryAnimData.Float0()				= pspaData.m_Float0;
				mSecondaryAnimData.Float1()				= pspaData.m_Float1;
				mSecondaryAnimData.Float2()				= pspaData.m_Float2;
				mSecondaryAnimData.IsBlocked()			= pspaData.m_IsBlocked;
			}
			else
			{
				//And compare against last received
				if( mSecondaryAnimData.m_SecondaryTaskSequenceID	== pspaData.m_taskSequenceId )
				{
					//Player is currently running the same secondary TaskScriptedAnimation anim as being synced, 
					//so leave it running
					m_newSecondaryAnim = cacheNewSecondaryAnim; //set this to whatever status the last received m_newSecondaryAnim was in case it was pending dictionary load
				}
				else
				{
					mSecondaryAnimData.SecondaryPartialAnimClipHash()		= pspaData.m_ClipHash;
					mSecondaryAnimData.SecondaryPartialAnimFlags()			= pspaData.m_Flags;
					mSecondaryAnimData.SecondaryBoneMaskHash()				= pspaData.m_BoneMaskHash;
					mSecondaryAnimData.m_SecondaryTaskSequenceID			= pspaData.m_taskSequenceId ;
				}
			}
		}
	}
	else
	{
		if( mSecondaryAnimData.m_SecondaryPartialAnimDictHash!=0 &&
			mSecondaryAnimData.SecondaryPartialAnimClipHash()!=0 )
		{
			m_newSecondaryAnim = true;//inform update the secondary task needs clearing if still on
		}

		mSecondaryAnimData.m_SecondaryPartialAnimDictHash = 0;
		mSecondaryAnimData.SecondaryPartialAnimClipHash() = 0;
		mSecondaryAnimData.SecondaryPartialAnimFlags() = 0;
		m_SecondaryPartialSlowBlendDuration = false;
		mSecondaryAnimData.SecondaryBoneMaskHash() = 0;

		mSecondaryAnimData.m_SecondaryTaskSequenceID = INVALID_SECONDARY_TASK_SEQUENCE_ID; 
	}
}

void CNetObjPlayer::UpdateSecondaryPartialAnimTask()
{
	CPed *pPed = GetPed();
	VALIDATE_GAME_OBJECT(pPed);

	if(m_newSecondaryAnim && taskVerifyf(IsClone(),"Expect clone here"))
	{
		gnetAssertf(pPed->IsPlayer(), "Should only get secondary anims for players");

		CTask					*pCurrentSecondaryTask		= pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim(); 
		CTaskScriptedAnimation	*pCurrentScriptedAnimTask	= NULL; 
		CTaskMoVEScripted		*pCurrentMoVEScriptedTask	= NULL; 

		if (pCurrentSecondaryTask)
		{
			if(pCurrentSecondaryTask->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION)
			{
				pCurrentScriptedAnimTask = static_cast<CTaskScriptedAnimation*>(pCurrentSecondaryTask);
			}
			else if(pCurrentSecondaryTask->GetTaskType()==CTaskTypes::TASK_MOVE_SCRIPTED)
			{
				pCurrentMoVEScriptedTask = static_cast<CTaskMoVEScripted*>(pCurrentSecondaryTask);
			}
		}

		if( mSecondaryAnimData.m_SecondaryPartialAnimDictHash!=0 )
		{
			strLocalIndex clipDictIndex = fwAnimManager::FindSlotFromHashKey(mSecondaryAnimData.m_SecondaryPartialAnimDictHash);
			if(taskVerifyf(clipDictIndex!=-1,"Unrecognized clip dictionary for secondary anim %u",mSecondaryAnimData.m_SecondaryPartialAnimDictHash))
			{
				fwClipDictionaryLoader loader(clipDictIndex.Get(), false);

				// Get clip dictionary
				crClipDictionary *clipDictionary = loader.GetDictionary();

				if(clipDictionary==NULL)			
				{	//if not loaded put in a request for it and exit
					g_ClipDictionaryStore.StreamingRequest(clipDictIndex, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
					return;
				}

				if(mSecondaryAnimData.m_bMoVEScripted)
				{
					if( !pCurrentMoVEScriptedTask ||
						pCurrentMoVEScriptedTask->GetCloneNetworkDefHash() != mSecondaryAnimData.MoveNetDefHash())
					{   
						u32 iFlags = CTaskMoVEScripted::Flag_Secondary;

						float blendInDelta	= INSTANT_BLEND_DURATION;
						if(m_SecondaryPartialSlowBlendDuration)
						{
							blendInDelta	= REALLY_SLOW_BLEND_DURATION;
						}

						pCurrentMoVEScriptedTask=rage_new CTaskMoVEScripted(fwMvNetworkDefId(mSecondaryAnimData.MoveNetDefHash()), iFlags, blendInDelta );

						pPed->GetPedIntelligence()->AddTaskSecondary( pCurrentMoVEScriptedTask, PED_TASK_SECONDARY_PARTIAL_ANIM );

						if(!pCurrentMoVEScriptedTask->GetIsNetworkActive())
						{
							return; //return leaving m_newSecondaryAnim	set true waiting for network to be ready	
						}
					}

					if(taskVerifyf(pCurrentMoVEScriptedTask,"%s Should have a valid pCurrentMoVEScriptedTask here", pPed->GetDebugName()))
					{
						const float CLONE_ANIMATION_LERP = 0.05f;

						if(mSecondaryAnimData.FloatHash0())
						{
							pCurrentMoVEScriptedTask->SetCloneSignalFloat(mSecondaryAnimData.FloatHash0(), mSecondaryAnimData.Float0(), CLONE_ANIMATION_LERP);
						}

						if(mSecondaryAnimData.FloatHash1())
						{
							pCurrentMoVEScriptedTask->SetCloneSignalFloat(mSecondaryAnimData.FloatHash1(), mSecondaryAnimData.Float1(), CLONE_ANIMATION_LERP);
						}

						if(mSecondaryAnimData.FloatHash2())
						{
							pCurrentMoVEScriptedTask->SetCloneSignalFloat(mSecondaryAnimData.FloatHash2(), mSecondaryAnimData.Float2(), CLONE_ANIMATION_LERP);
						}

						pCurrentMoVEScriptedTask->SetSignalBool(atHashWithStringNotFinal("isBlocked"),mSecondaryAnimData.IsBlocked(),false);
					}
				}
				else
				{
					eScriptedAnimFlagsBitSet flags;
					flags.BitSet().SetBits(mSecondaryAnimData.SecondaryPartialAnimFlags());

					u32 iBoneMask = BONEMASK_ALL;
					if (flags.BitSet().IsSet(AF_UPPERBODY))
					{
						iBoneMask = BONEMASK_UPPERONLY;
					}

					//Override bonemask if hash non zero
					if(mSecondaryAnimData.SecondaryBoneMaskHash())
					{
						iBoneMask = mSecondaryAnimData.SecondaryBoneMaskHash();
					}

					bool bSecondary	= flags.BitSet().IsSet(AF_SECONDARY);

					float blendInDelta	= NORMAL_BLEND_IN_DELTA;
					float blendOutDelta = NORMAL_BLEND_OUT_DELTA;

					if(m_SecondaryPartialSlowBlendDuration)
					{
						blendInDelta  = SLOW_BLEND_IN_DELTA;
						blendOutDelta = SLOW_BLEND_OUT_DELTA;
					}

					CTask* pTask;

					pTask = rage_new CTaskScriptedAnimation(atHashWithStringNotFinal(mSecondaryAnimData.m_SecondaryPartialAnimDictHash), 
						atHashWithStringNotFinal(mSecondaryAnimData.SecondaryPartialAnimClipHash()), 
						CTaskScriptedAnimation::kPriorityLow, iBoneMask, 
						fwAnimHelpers::CalcBlendDuration(blendInDelta), 
						fwAnimHelpers::CalcBlendDuration(blendOutDelta), 
						-1,		//nTimeToPlay, 
						flags, 
						0.0f	//fStartPhase 
						);
					if(bSecondary && pPed)
					{
						if(pCurrentScriptedAnimTask)
						{   //TaskScriptedAnimation needs to know it is swapping to to another TaskScriptedAnimation on a clone
							//to make sure there isn't a sudden blend out when it loses move network
							pCurrentScriptedAnimTask->SetCloneLeaveMoveNetwork();
						}
						pPed->GetPedIntelligence()->AddTaskSecondary( pTask, PED_TASK_SECONDARY_PARTIAL_ANIM );
					}
					else
					{
						gnetAssertf(bSecondary, "Expect flags to have AF_SECONDARY set. Flags 0x%x ", mSecondaryAnimData.SecondaryPartialAnimFlags());
					}
				}

				m_newSecondaryAnim = false; //clear this now the task has started
			}
		}
		else
		{
			m_newSecondaryAnim = false;
			if (pCurrentSecondaryTask) //Syncing has transitioned to no secondary task so ensure any current secondary is cleared
			{
				pPed->GetPedIntelligence()->AddTaskSecondary(NULL, PED_TASK_SECONDARY_PARTIAL_ANIM);					
			}
		}
	}
}

void CNetObjPlayer::PlaceClanDecoration()
{
	if (m_clandecoration_collectionhash && m_clandecoration_presethash)
	{
		PedDecorationManager::GetInstance().ClearClanDecorations(GetPlayerPed());
		PedDecorationManager::GetInstance().PlaceDecoration(GetPlayerPed(), m_clandecoration_collectionhash, m_clandecoration_presethash);
	}
}

eWantedLevel CNetObjPlayer::GetHighestWantedLevelInArea()
{
	return ms_HighestWantedLevelInArea;
}

void CNetObjPlayer::UpdateHighestWantedLevelInArea()
{
	CPed *pLocalPed = CGameWorld::FindLocalPlayer();
	if(!pLocalPed)
	{
		return;
	}

	const float NEARBY_PLAYER_WANTED_LEVEL_RADIUS_SQ = 300.0f*300.0f;
	const Vec3V center = pLocalPed->GetTransform().GetPosition();

	ms_HighestWantedLevelInArea = pLocalPed->GetPlayerWanted() ? pLocalPed->GetPlayerWanted()->GetWantedLevel() : WANTED_CLEAN;

	unsigned           numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		CNetGamePlayer *remotePlayer = SafeCast(CNetGamePlayer, remotePhysicalPlayers[index]);
		CPed *remotePlayerPed = remotePlayer ? remotePlayer->GetPlayerPed() : NULL;
		CWanted *remotePlayerWanted = remotePlayerPed ? remotePlayerPed->GetPlayerWanted() : NULL;

		if(remotePlayerWanted && DistSquared(remotePlayerPed->GetTransform().GetPosition(), center).Getf() <= NEARBY_PLAYER_WANTED_LEVEL_RADIUS_SQ)
		{
			ms_HighestWantedLevelInArea = Max(ms_HighestWantedLevelInArea, remotePlayerWanted->GetWantedLevel());
		}
	}
}

bool CNetObjPlayer::HasMoney()
{
	if (!IsClone())
	{
		//local player
		if ((MoneyInterface::GetVCWalletBalance() > 0) || (MoneyInterface::GetVCBankBalance() > 0))
			return true;
	}
	else if (GetRemoteHasMoney())
	{
		//remote player
		return true;
	}

	return false;
}

void  CNetObjPlayer::SetLocalPlayerWeaponPickupAndObject(CPickup* pWeaponPickup, CObject* pWeaponObject)
{
	CPed* pPlayerPed = GetPlayerPed();

	if (pPlayerPed)
	{
		gnetAssertf(pPlayerPed->IsDeadByMelee() || pPlayerPed->GetIsDeadOrDying(), "CNetObjPlayer::SetLocalPlayerWeaponPickupAndObject - player is not dead");

		ms_pLocalPlayerWeaponPickup = pWeaponPickup;
		ms_pLocalPlayerWeaponObject = pWeaponObject;

		if (ms_pLocalPlayerWeaponPickup)
		{
			NetworkInterface::LocallyHideEntityForAFrame(*ms_pLocalPlayerWeaponPickup, false, "Hiding pickup from dead local player");
		}
	}
}

bool CNetObjPlayer::HasCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex) 
{
	if (gnetVerify(playerIndex < MAX_NUM_PHYSICAL_PLAYERS))
	{
		if (playerIndex == NetworkInterface::GetLocalPhysicalPlayerIndex())
		{
			return ms_hasCachedLocalPlayerHeadBlendData;
		}
		else
		{
			return ((ms_hasCachedPlayerHeadBlendData & (1<<playerIndex)) != 0);
		}
	}

	return false;
}

const CPedHeadBlendData* CNetObjPlayer::GetCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex)
{
	if (playerIndex == NetworkInterface::GetLocalPhysicalPlayerIndex())
	{
		if (ms_hasCachedLocalPlayerHeadBlendData)
		{
			return &ms_cachedLocalPlayerHeadBlendData;
		}
	}
	else if (HasCachedPlayerHeadBlendData(playerIndex))
	{
		return &ms_cachedPlayerHeadBlendData[playerIndex];
	}

	return NULL;
}

void CNetObjPlayer::CachePlayerHeadBlendData(PhysicalPlayerIndex playerIndex, CPedHeadBlendData& headBlendData)
{
	if (gnetVerify(playerIndex < MAX_NUM_PHYSICAL_PLAYERS))
	{
		CNetGamePlayer* pGamePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(playerIndex);

		if (gnetVerifyf(pGamePlayer, "Trying to cache player head blend data for an invalid player %d", playerIndex))
		{
			LOGGING_ONLY(NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CACHING_PLAYER_HEAD_BLEND_DATA", pGamePlayer->GetLogName()));

			if (playerIndex == NetworkInterface::GetLocalPhysicalPlayerIndex())
			{
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Local player", "true"));
				ms_cachedLocalPlayerHeadBlendData = headBlendData;
				ms_hasCachedLocalPlayerHeadBlendData = true;
			}
			else
			{
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Player index", "%d", playerIndex));
				ms_cachedPlayerHeadBlendData[playerIndex] = headBlendData;
				ms_hasCachedPlayerHeadBlendData |= (1<<playerIndex);
			}
		}
	}
}

bool CNetObjPlayer::ApplyCachedPlayerHeadBlendData(CPed& ped, PhysicalPlayerIndex playerIndex)
{
	if (gnetVerify(HasCachedPlayerHeadBlendData(playerIndex)))
	{
		CNetGamePlayer* pGamePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(playerIndex);

		if (gnetVerifyf(pGamePlayer, "Trying to apply cached player head blend data for an invalid player %d", playerIndex))
		{
#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "APPLY_CACHED_PLAYER_HEAD_BLEND_DATA", pGamePlayer->GetLogName());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Apply to ped", "0x%p (%s)", &ped, ped.GetNetworkObject() ? ped.GetNetworkObject()->GetLogName() : "??");
#endif
			if (playerIndex == NetworkInterface::GetLocalPhysicalPlayerIndex())
			{
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Local player", "true"));
				return ApplyHeadBlendData(ped, ms_cachedLocalPlayerHeadBlendData);
			}
			else
			{
				LOGGING_ONLY(NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Player index", "%d", playerIndex));
				return ApplyHeadBlendData(ped, ms_cachedPlayerHeadBlendData[playerIndex]);
			}
		}
	}

	return false;
}

void CNetObjPlayer::ClearCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex) 
{
	if (gnetVerify(playerIndex < MAX_NUM_PHYSICAL_PLAYERS))
	{
		CNetGamePlayer* pGamePlayer = NetworkInterface::GetPhysicalPlayerFromIndex(playerIndex);

		if (pGamePlayer)
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CLEARING_PLAYER_HEAD_BLEND_DATA", pGamePlayer ? pGamePlayer->GetLogName() : "-left-");
		}

		if (playerIndex == NetworkInterface::GetLocalPhysicalPlayerIndex())
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Local player", "true");
			ms_cachedLocalPlayerHeadBlendData.Reset();
			ms_hasCachedLocalPlayerHeadBlendData = false;
		}
		else
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Player index", "%d", playerIndex);
			ms_cachedPlayerHeadBlendData[playerIndex].Reset();
			ms_hasCachedPlayerHeadBlendData &= ~(1<<playerIndex);
		}
	}
}

void CNetObjPlayer::ClearAllCachedPlayerHeadBlendData()
{
	ms_cachedLocalPlayerHeadBlendData.Reset();
	ms_hasCachedLocalPlayerHeadBlendData = false;

	for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
	{
		ms_cachedPlayerHeadBlendData[i].Reset();
	}

	ms_hasCachedPlayerHeadBlendData = 0;
}

void CNetObjPlayer::ClearAllRemoteCachedPlayerHeadBlendData()
{
	for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
	{
		ms_cachedPlayerHeadBlendData[i].Reset();
	}

	ms_hasCachedPlayerHeadBlendData = 0;
}

float CNetObjPlayer::GetExtendedScopeFOVMultiplier(float fFOV)
{
	float fMultiplier = 1.0f;
	CPed *pPlayerPed = GetPlayerPed();

	if (pPlayerPed && fFOV < MIN_FOV_NO_SCOPE)
	{
		const CWeapon* pWeapon = pPlayerPed->GetWeaponManager() ? pPlayerPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if (pWeapon)
		{
			bool bIsScopedWeapon = pWeapon->GetScopeComponent() ? true : false;
			if (!bIsScopedWeapon)
			{
				bIsScopedWeapon =  pWeapon->GetHasFirstPersonScope();
			}

			if (bIsScopedWeapon)
				fMultiplier = RampValue(fFOV, MIN_FOV, MIN_FOV_NO_SCOPE, 4.0f, 1.0f);
		}
	}

	return fMultiplier;
}

// If clone has synced an object it is standing on check object is streamed in
// and clone is on top of it
void CNetObjPlayer::UpdateCloneStandingOnObjectStreamIn()
{
    if(taskVerifyf(IsClone(),"%s Expected to be a clone",GetLogName()) && taskVerifyf(GetPlayerPed(),"%s Expected ped",GetLogName()))
    {
        if(!NetworkBlenderIsOverridden())
        {
            CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, GetNetBlender());
            if(netBlenderPed)
            {
                ObjectId	objectStoodOnID		= NETWORK_INVALID_OBJECT_ID;
                Vector3		objectStoodOnOffset;

                if(netBlenderPed->IsStandingOnObject(objectStoodOnID, objectStoodOnOffset))
                {
                    CPhysical *groundPhysical = GetPlayerPed()->GetGroundPhysical();

                    if( groundPhysical && 
                        groundPhysical->GetNetworkObject() &&
                        groundPhysical->GetNetworkObject()->GetObjectID() == objectStoodOnID )
                    {
                        //Player is standing on the the object so leave
                        return;
                    }

                    if(m_standingOnStreamedInNetworkObjectIntervalTimer > 0)
                    {
                        //Wait a bit between calls to NetworkInterface::GetNetworkObject
                        m_standingOnStreamedInNetworkObjectIntervalTimer -= static_cast<s16>(fwTimer::GetTimeStepInMilliseconds());
                        return;
                    }
                
                    m_standingOnStreamedInNetworkObjectIntervalTimer = STANDING_ON_OBJECT_STREAM_IN_CHECK_INTERVAL;

                    netObject *networkObject = NetworkInterface::GetNetworkObject(objectStoodOnID);

                    if(networkObject)
                    {
						CNetObjVehicle* netObjVeh = static_cast<CNetObjVehicle*>(networkObject);

						if (netObjVeh && netObjVeh->GetVehicle() && netObjVeh->GetVehicle()->GetIsInWater())
						{
							return;
						}

                        if(!IsUsingRagdoll() && !GetPlayerPed()->GetIsAttached())
                        {
                            //this object must have just streamed in so put player on top
                            netBlenderPed->GoStraightToTarget();
                        }
                    }
                }
            }
        }
    }
}

void CNetObjPlayer::UpdateLockOnTarget()
{
	CPed* pPlayerPed = GetPlayerPed();
	if(pPlayerPed)
	{
		if(m_LockOnTargetID != NETWORK_INVALID_OBJECT_ID)
		{
			netObject *targetObject  = NetworkInterface::GetNetworkObject(m_LockOnTargetID);
			CVehicle  *targetVehicle = targetObject ? NetworkUtils::GetVehicleFromNetworkObject(targetObject) : 0;

			if(targetVehicle)
			{
				CEntity::eHomingLockOnState lockOnState = static_cast<CEntity::eHomingLockOnState>(m_LockOnState);
				targetVehicle->UpdateHomingLockedOntoState(lockOnState);
			}
		}
	}
}


#if __DEV

void CNetObjPlayer::DebugRenderTargetData(void)
{
	CPed* pPlayerPed = GetPlayerPed();
	if(pPlayerPed)
	{
		CPedWeaponManager const* weaponManager = pPlayerPed->GetWeaponManager();
		if(weaponManager)
		{
			Vector3 aimTarget				= weaponManager->GetWeaponAimPosition();
			CWeapon const* weapon			= weaponManager->GetEquippedWeapon();
			CObject const* weaponObject		= weaponManager->GetEquippedWeaponObject();

			if(weapon && weaponObject)
			{
				Mat34V weaponMatrix = weaponObject->GetMatrix();

				Vector3 muzzlePosition(VEC3_ZERO); 
				weapon->GetMuzzlePosition(RC_MATRIX34(weaponMatrix), muzzlePosition);

				grcDebugDraw::Line(muzzlePosition, aimTarget, Color_aquamarine, Color_aquamarine);
				grcDebugDraw::Sphere(aimTarget, 0.2f, Color_aquamarine, false);	

				//---

				m_TargetCloningInfo.DebugPrint(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));

				//---

				if(IsClone())
				{
					if(m_TargetCloningInfo.GetCurrentAimTargetID() != NETWORK_INVALID_OBJECT_ID)
					{
						netObject* pObj = NetworkInterface::GetNetworkObject(m_TargetCloningInfo.GetCurrentAimTargetID());
						if(pObj)
						{
							CEntity* entity = pObj->GetEntity();
							if(entity)
							{
								Color32 color = m_TargetCloningInfo.IsFreeAimLockedOnTarget() ? Color_red : Color_orange;

								Vector3 targetPos = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
								grcDebugDraw::Sphere(targetPos, 0.2f, color, false);
								grcDebugDraw::Sphere(targetPos + m_TargetCloningInfo.GetLockOnTargetAimOffset(), 0.2f, color, false);
								grcDebugDraw::Line(targetPos, targetPos + m_TargetCloningInfo.GetLockOnTargetAimOffset(), color, color);

		 						grcDebugDraw::Line(muzzlePosition, aimTarget, color, color);
								grcDebugDraw::Sphere(aimTarget, 0.2f, color, false);

								char buffer[256];
								sprintf(buffer, "len %f", m_TargetCloningInfo.GetLockOnTargetAimOffset().Mag());
								Vector3 half = Lerp(0.5f, targetPos, targetPos + m_TargetCloningInfo.GetLockOnTargetAimOffset());
								grcDebugDraw::Text(half, Color_white, buffer, true);
							}
						}
					}
				}
				else
				{
					const CPlayerPedTargeting & rPlayerTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();
					
					CEntity const* freeAimTarget		= rPlayerTargeting.GetFreeAimTarget();
					CEntity const* freeAimTargetRagdoll = rPlayerTargeting.GetFreeAimTargetRagdoll();
					CEntity const* lockOnTarget			= rPlayerTargeting.GetLockOnTarget();

					if(freeAimTarget)
					{
						Vector3 targetPos = VEC3V_TO_VECTOR3(freeAimTarget->GetTransform().GetPosition());
						grcDebugDraw::Sphere(targetPos, 0.2f, Color_orange, false);
						grcDebugDraw::Line(muzzlePosition, targetPos, Color_orange, Color_orange);
					}

					if(freeAimTargetRagdoll)
					{
						Vector3 targetPos = VEC3V_TO_VECTOR3(freeAimTargetRagdoll->GetTransform().GetPosition());
						grcDebugDraw::Sphere(targetPos, 0.225f, Color_green, false);
						grcDebugDraw::Line(muzzlePosition, targetPos, Color_green, Color_green);
					}

					if(lockOnTarget)
					{
						Vector3 targetPos = VEC3V_TO_VECTOR3(lockOnTarget->GetTransform().GetPosition());
						grcDebugDraw::Sphere(targetPos, 0.25f, Color_purple, false);
						grcDebugDraw::Line(muzzlePosition, targetPos, Color_purple, Color_purple);
					}

					if(m_TargetCloningInfo.GetCurrentAimTargetEntity())
					{
						Vector3 targetPos = VEC3V_TO_VECTOR3(m_TargetCloningInfo.GetCurrentAimTargetEntity()->GetTransform().GetPosition());
						grcDebugDraw::Sphere(targetPos, 0.275f, Color_yellow, false);
						grcDebugDraw::Line(muzzlePosition, targetPos, Color_yellow, Color_yellow);					
					}

					if(pPlayerPed)
					{
						if(m_TargetCloningInfo.GetTargetMode() == TargetCloningInformation::TargetMode_PosWS)
						{
							Vector3 startPos				= VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			    			
							const CWeapon* pWeapon			= NULL;
							const CObject* pWeaponObject	= NULL;
				    		
							if(pPlayerPed->GetWeaponManager())
							{
								pWeapon			= pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
								pWeaponObject	= pPlayerPed->GetWeaponManager()->GetEquippedWeaponObject();
							}

							if(pWeapon && pWeaponObject)
							{
								Matrix34 const& weaponMatrix = MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix());
								pWeapon->GetMuzzlePosition(weaponMatrix, startPos);
							}
			    			
							Vector3 offset = startPos - m_TargetCloningInfo.GetCurrentAimTargetPos(); 

							char buffer[256];
							sprintf(buffer, "len %f : x %f : y %f : z %f", offset.Mag(), offset.x, offset.y, offset.z);
							grcDebugDraw::Text(startPos, Color_white, buffer, true);
						}
					}
				}
			}
		}
	}
}

#endif /* __DEV */

#if __BANK

u32 CNetObjPlayer::GetNetworkInfoStartYOffset()
{
    u32 offset = 0;

    if((strlen(m_DisplayName)) && GetEntity()->GetIsVisible())
    {
        offset++;
    }

    return offset;
}

bool CNetObjPlayer::ShouldDisplayAdditionalDebugInfo()
{
    const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	if (displaySettings.m_displayTargets.m_displayPlayers && GetPlayerPed())
	{
		return true;
	}

	return CNetObjPed::ShouldDisplayAdditionalDebugInfo();
}

#endif // __BANK
