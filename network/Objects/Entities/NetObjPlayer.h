//
// name:        NetObjPlayer.h
// description: Derived from netObject, this class handles all player-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#ifndef NETOBJPLAYER_H
#define NETOBJPLAYER_H

// Rage headers
#include "grcore/viewport.h"

// Framework headers
#include "fwnet/netplayer.h"

// Game headers
#include "frontend/hud_colour.h"
#include "Peds/Ped.h"
#include "network/objects/entities/NetObjPed.h"
#include "network/objects/synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "network/objects/synchronisation/SyncNodes/PlayerSyncNodes.h"
#include "Network/Live/PlayerCardDataManager.h"

enum eWantedLevel;

// ================================================================================================================
// CNetViewPortWrapper
// ================================================================================================================

class CNetViewPortWrapper
{
public:

    FW_REGISTER_CLASS_POOL(CNetViewPortWrapper);

    grcViewport *GetViewPort() { return &m_ViewPort; }

private:

    grcViewport m_ViewPort;
};

// ================================================================================================================
// CNetObjPlayer
// ================================================================================================================

class CNetObjPlayer : public CNetObjPed, public IPlayerNodeDataAccessor
{
public:
	typedef CPlayerCardXMLDataManager::PlayerCard PlayerCard;

#if __BANK
    // debug update rate that can be forced on local player via the widgets
    static s32 ms_updateRateOverride;
#endif

    struct CPlayerScopeData : public netScopeData
    {
        CPlayerScopeData()
        {
            m_scopeDistance         = 1000.0f; // players are always cloned so we just set this to a large value
            m_scopeDistanceInterior = 1000.0f;
            m_syncDistanceNear      = 10.0;
            m_syncDistanceFar       = 50.0f;
        }
    };

	static const int PLAYER_SYNC_DATA_NUM_NODES = 32;
	static const int PLAYER_SYNC_DATA_BUFFER_SIZE = 1725;
	static const unsigned ALLOW_PLAYER_GENERATE_SCENARIO_PED = ~0u;

    class CPlayerSyncData : public netSyncData_Static<MAX_NUM_PHYSICAL_PLAYERS, PLAYER_SYNC_DATA_NUM_NODES, PLAYER_SYNC_DATA_BUFFER_SIZE, CNetworkSyncDataULBase::NUM_UPDATE_LEVELS>
    {
    public:
       FW_REGISTER_CLASS_POOL(CPlayerSyncData);
    };

    static const unsigned MAX_PLAYER_DISPLAY_NAME      = 256;
	static const unsigned TIME_BETWEEN_LOOK_AT_CHECKS  = 1000;
    static const unsigned SOLO_TUTORIAL_INDEX          = eTEAM_SOLO_TUTORIAL;
    static const unsigned INVALID_TUTORIAL_INDEX       = eTEAM_INVALID_TUTORIAL;
    static const unsigned INVALID_TUTORIAL_INSTANCE_ID = MAX_NUM_PHYSICAL_PLAYERS*2;
	static const unsigned TIME_PENDING_TUTORIAL_CHANGE  = 500;
	static const unsigned TIME_PENDING_TUTORIAL_CLONE_POPULATION_REMOVED = 4500;  //can take a few seconds
	static const unsigned POST_TUTORIAL_CHANGE_HOLD_OFF_PED_GEN = 2000;  //B* 1631776 about 2 secs seem sabout right
	static const unsigned POST_WARP_HOLD_OFF_PED_GEN = 4000;  //B* 2170027 when a player warps into an area he does not start alone he waits for received scenario gen to stabilise

#if FPS_MODE_SUPPORTED
	static bool ms_bUseFPIdleMovementOnClones;
#endif

public:

    CNetObjPlayer(class CPed* pPlayer,
                const NetworkObjectType type,
                const ObjectId objectID,
                const PhysicalPlayerIndex playerIndex,
                const NetObjFlags localFlags,
                const NetObjFlags globalFlags);

    CNetObjPlayer() :
    m_pViewportWrapper(0),
    m_RespawnNetObjId(NETWORK_INVALID_OBJECT_ID),
	m_SpectatorObjectId(NETWORK_INVALID_OBJECT_ID),
	m_AntagonisticPlayerIndex(INVALID_PLAYER_INDEX),
    m_UseKinematicPhysics(false),
	m_nLookAtIntervalTimer(0),
	m_nLookAtTime(0),
	m_pLookAtTarget(NULL),
	m_bSyncLookAt(false),
	m_bIsCloneLooking(false),
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
    m_TargetVehicleIDForAnimStreaming(NETWORK_INVALID_OBJECT_ID),
    m_TargetVehicleEntryPoint(-1),
    m_UsingFreeCam(false),
	m_FadePlayerToZero(false),
	m_MaxPlayerFadeOutTime(0),
	m_bIsFriendlyFireAllowed(true),
	m_bIsPassive(false),
    m_PRF_BlockRemotePlayerRecording(false),
	m_PRF_UseScriptedWeaponFirePosition(false),
    m_PRF_IsSuperJumpEnabled(false),
    m_PRF_IsShockedFromDOTEffect(false),
    m_PRF_IsChokingFromDOTEffect(false),
	m_statVersion(0),
	m_SecondaryPartialSlowBlendDuration(false),
	m_newSecondaryAnim(false),
	m_fVoiceLoudness(0.f),
	m_uThrottleVoiceLoudnessTime(0),
	m_bCopsWereSearching(false),
	m_bInsideVehicle(false),
	m_bBattleAware(false),
    m_VehicleJumpDown(false),
	m_bUseExtendedPopulationRange(false),
	m_vExtendedPopulationRangeCenter(VEC3_ZERO),
	m_networkedDamagePack(0),
	m_lastReceivedHealthTimestamp(0),
	m_pDeathResponseTask(NULL),
	m_addToPendingDamageCalled(false),
	m_remoteHasMoney(true),
	m_bRespawnInvincibilityTimerRespot(false),
	m_bRespawnInvincibilityTimerWasSet(false),
	m_bRemoteReapplyColors(false),
    m_MyVehicleIsMyInteresting(false),
    m_DisableRespawnFlashing(false),
	m_bDisableLeavePedBehind(false),
    m_UsingLeftTriggerAimMode(false),
	m_bCloneRespawnInvincibilityTimerCountdown(false),
	m_bWantedVisibleFromPlayer(false),
	m_bIgnoreTaskSpecificData(false),
	m_remoteHasScarData(false),
	m_sentScarCount(0),
    m_CityDensitySetting(1.0f),
	m_remoteFakeWantedLevel(0),
	m_pendingVehicleWeaponIndex(-1),
	m_HasToApplyWeaponToVehicle(false)
#if FPS_MODE_SUPPORTED
	,m_bUsingFirstPersonCamera(false)
	,m_bUsingFirstPersonVehicleCamera(false)
	,m_bInFirstPersonIdle(false)
	,m_bStickWithinStrafeAngle(false)
	,m_bUsingSwimMotionTask(false)
	,m_bOnSlope(false)
#endif
#if !__FINAL
	,m_bDebugInvincible(false)
#endif
	,m_LatestTimeCanSpawnScenarioPed(ALLOW_PLAYER_GENERATE_SCENARIO_PED)
	, m_LockOnTargetID(NETWORK_INVALID_OBJECT_ID)
	, m_LockOnState(0)
	,m_vehicleShareMultiplier(1.0f)
	, m_ghostPlayerFlags(0)
	, m_PlayerWaypointOwner(NETWORK_INVALID_OBJECT_ID)
	, m_PlayerWaypointColor(eHUD_COLOURS::HUD_COLOUR_INVALID)
	, m_ConcealedOnOwner(false)
    {
        SetObjectType(NET_OBJ_TYPE_PLAYER);
        safecpy(m_DisplayName, "", MAX_PLAYER_DISPLAY_NAME);
		memset(m_stats, 0, sizeof(m_stats));
		ms_forceStatUpdate = true; // Should force the local player to update/upload the stats whenever a new player is added.
    }

    ~CNetObjPlayer();

    virtual const char *GetObjectTypeName() const { return "PLAYER"; }

    CPed		*GetPlayerPed()          const { return (CPed*)GetGameObject(); }
    grcViewport *GetViewport()           const { return m_pViewportWrapper ? m_pViewportWrapper->GetViewPort() : 0; }

    const char  *GetPlayerDisplayName() const { return m_DisplayName; }
    void         SetPlayerDisplayName(const char *playerName) { safecpy(m_DisplayName, playerName, MAX_PLAYER_DISPLAY_NAME); }

	s16          GetRespawnInvincibilityTimer() { return m_respawnInvincibilityTimer; }
    void         SetRespawnInvincibilityTimer(s16 timer);
    void         SetDisableRespawnFlashing(bool disable);

	void		 SetCloneRespawnInvincibilityTimerCountdown(bool bValue) { m_bCloneRespawnInvincibilityTimerCountdown = bValue; };

#if !__FINAL
	bool		IsDebugInvincible() const { return m_bDebugInvincible; }
#endif

    float       GetCityDensitySetting() const { return m_CityDensitySetting; }

	s16			 GetPendingTutorialSessionChangeTimer() { return  m_pendingTutorialSessionChangeTimer; }
	void         SetPendingTutorialSessionChangeTimer(s16 timer);

	unsigned	 GetLatestTimeCanSpawnScenarioPed() const { return  m_LatestTimeCanSpawnScenarioPed; }
	void         SetLatestTimeCanSpawnScenarioPed(unsigned latestTimeCanSpawnScenarioPed) { m_LatestTimeCanSpawnScenarioPed = latestTimeCanSpawnScenarioPed;}

	static bool		 IsInPostTutorialSessionChangeHoldOffPedGenTime() { return  m_postTutorialSessionChangeHoldOffPedGenTimer >0; }
	static void		 SetHoldOffPedGenTime() {m_postTutorialSessionChangeHoldOffPedGenTimer = POST_TUTORIAL_CHANGE_HOLD_OFF_PED_GEN;}
	static void		 SetHoldOffPedGenTimePostWarp() {m_postTutorialSessionChangeHoldOffPedGenTimer = POST_WARP_HOLD_OFF_PED_GEN;}

    void         GetAccurateRemotePlayerPosition(Vector3 &position);

    bool         IsVisibleOnscreen();
 
    bool         IsUsingFreeCam() const  { return m_UsingFreeCam; }
	bool         IsUsingCinematicVehicleCam() const  { gnetAssertf(IsClone(), "This function is only valid to be called on clones!"); return m_UsingCinematicVehCam; }
    void         SetUsingFreeCam(bool usingFreeCam) { m_UsingFreeCam = usingFreeCam; }

    void         StartSoloTutorial();
    void         StartGangTutorial(int team, int instanceID);
    void         EndTutorial();
    u8           GetTutorialIndex()      const			{ return m_tutorialIndex; }
    bool         IsInTutorialSession()   const			{ return m_tutorialIndex != INVALID_TUTORIAL_INDEX; }
    u8           GetTutorialInstanceID() const			{ return m_tutorialInstanceID; }
	bool         IsTutorialSessionChangePending() const { return m_bPendingTutorialSessionChange; }
	bool         WaitingPopulationClearedInTutorialSession() const { return m_bWaitingPopulationClearedInTutorialSession; }
	void         SetUpPendingTutorialChange();
	void		 ChangeToPendingTutorialSession();
	void		 UpdatePendingTutorialSession();

    u32          GetAllowedPedModelStartOffset()     const { return m_AllowedPedModelStartOffset; }
    u32          GetAllowedVehicleModelStartOffset() const { return m_AllowedVehicleModelStartOffset; }

	void		 SetAlwaysClonePlayerVehicle(bool bAlwaysClone) { m_bAlwaysClonePlayerVehicle = bAlwaysClone; }
	bool         GetAlwaysClonePlayerVehicle() const { return m_bAlwaysClonePlayerVehicle; }

    ObjectId     GetTargetVehicleIDForAnimStreaming() const { return m_TargetVehicleIDForAnimStreaming; }
    s32          GetTargetVehicleEntryPoint() const { return m_TargetVehicleEntryPoint; }

    bool         IsVehicleJumpDown()               const { return m_VehicleJumpDown; }
    bool         IsMyVehicleMyInteresting()        const { return m_MyVehicleIsMyInteresting; }
    bool         IsRemoteUsingLeftTriggerAimMode() const { return m_UsingLeftTriggerAimMode; }

#if FPS_MODE_SUPPORTED
	bool		 IsUsingFirstPersonCamera()		   const { return m_bUsingFirstPersonCamera; }
	bool		 IsUsingFirstPersonVehicleCamera() const { return m_bUsingFirstPersonVehicleCamera; }
	bool		 IsInFirstPersonIdle()			   const { return m_bInFirstPersonIdle; }
	bool		 IsStickWithinStrafeAngle()		   const { return m_bStickWithinStrafeAngle; }
	bool		 IsUsingSwimMotionTask()		   const { return m_bUsingSwimMotionTask; }
	bool		 IsOnSlope()					   const { return m_bOnSlope; }
#endif

	virtual void SetAsGhost(bool bSet BANK_ONLY(, unsigned reason, const char* scriptInfo = ""))
	{
		CNetObjPed::SetAsGhost(bSet BANK_ONLY(, reason, scriptInfo));

		if (!bSet)
		{
			m_ghostPlayerFlags = 0;
		}
	}

	PlayerFlags GetGhostPlayerFlags() const { return m_ghostPlayerFlags; }
	void SetGhostPlayerFlags(PlayerFlags playerFlags BANK_ONLY(, const char* scriptInfo))
	{
		m_ghostPlayerFlags = playerFlags;
#if __BANK 
		m_ghostScriptInfo = scriptInfo;
#endif
		SetAsGhost(m_ghostPlayerFlags != 0 BANK_ONLY(, SPG_SET_GHOST_PLAYER_FLAGS)); 
	}

    static void CreateSyncTree();
    static void DestroySyncTree();

    // see NetworkObject.h for a description of these functions
    static  CProjectSyncTree*  GetStaticSyncTree()  { return ms_playerSyncTree; }
    virtual CProjectSyncTree*  GetSyncTree()        { return ms_playerSyncTree; }
    virtual netScopeData&      GetScopeData()       { return ms_playerScopeData; }
    virtual netScopeData&      GetScopeData() const { return ms_playerScopeData; }
    static  netScopeData&      GetStaticScopeData() { return ms_playerScopeData; }
    virtual netSyncDataBase*   CreateSyncData()     { return rage_new CPlayerSyncData(); }

    virtual netINodeDataAccessor *GetDataAccessor(u32 dataAccessorType);

    virtual bool        Update();
	virtual void        StartSynchronising();
	virtual bool		CanClone(const netPlayer& player, unsigned *resultCode = 0) const;
    virtual bool        CanSync(const netPlayer& player) const;
	virtual bool		CanSynchronise(bool bOnRegistration) const;
	virtual bool        ProcessControl();
	virtual void        CleanUpGameObject(bool bDestroyObject);
    virtual bool        CanPassControl(const netPlayer& player, eMigrationType migrationType, unsigned *resultCode = 0) const;
    virtual bool        NetworkBlenderIsOverridden(unsigned *resultCode = 0) const;
    virtual bool        CanBlendWhenFixed() const;
    virtual void        ChangeOwner(const netPlayer& player, eMigrationType migrationType);
    virtual bool        NeedsReassigning() const { return false;}
    virtual bool        IsInScope(const netPlayer& player, unsigned *scopeReason = NULL) const;
	virtual void        ManageUpdateLevel(const netPlayer& player);
    virtual bool        IsScriptObject(bool bIncludePlayer = false) const;
	virtual void        PlayerHasJoined(const netPlayer& player);
	virtual void        PlayerHasLeft(const netPlayer& player);
	virtual bool		IsImportant() const { return true; }
    virtual void		PostCreate();
	virtual bool		ShouldStopPlayerJitter();
	virtual void		SetIsVisible(bool isVisible, const char* reason, bool bNetworkUpdate = false);
	virtual void		DisableCollision();
	virtual void		ForceSendOfCutsceneState();
	virtual void		OnUnregistered();

    virtual void TryToPassControlProximity();

	virtual bool ShouldObjectBeHiddenForCutscene() const;
	virtual void HideForCutscene();
	virtual void ExposeAfterCutscene();

	virtual bool  NeedsPlayerTeleportControl() const { return false; }
	virtual bool  IsConcealed() const { if(m_ConcealedOnOwner) return true; return CNetObjPed::IsConcealed(); }
    // called on the local player when their model is being changed
	bool ChangePlayerModel(const u32 newModelIndex);

    virtual void DisplayNetworkInfo();

    // sync parser creation functions
    void GetPlayerCreateData(CPlayerCreationDataNode& data) const;
    void SetPlayerCreateData(const CPlayerCreationDataNode& data);

    // sync parser getter functions
    void GetPlayerSectorPosData(CPlayerSectorPosNode& data) const;

	void GetPlayerGameStateData(CPlayerGameStateDataNode& data);

    void GetPlayerAppearanceData(CPlayerAppearanceDataNode& data);

    void GetCameraData(CPlayerCameraDataNode& data);
    void GetPlayerPedGroupData(CPlayerPedGroupDataNode& data);
	void GetPlayerLOSVisibility(u32& playerFlags);
	void GetPlayerWantedAndLOSData(CPlayerWantedAndLOSDataNode& data);
    void GetAmbientModelStreamingData(CPlayerAmbientModelStreamingNode& data);
	void GetPlayerGamerData(CPlayerGamerDataNode& data);
	void GetPlayerExtendedGameStateData(CPlayerExtendedGameStateNode& data);

    // sync parser setter functions
	virtual void SetPedHealthData(const CPedHealthDataNode& data);
	virtual void SetTaskTreeData(const CPedTaskTreeDataNode& data);
	virtual void SetTaskSpecificData(const CPedTaskSpecificDataNode& data);

	virtual int GetVehicleWeaponIndex() { return m_pendingVehicleWeaponIndex; }

	void SetPlayerSectorPosData(const CPlayerSectorPosNode& data);

    void SetPlayerGameStateData(const CPlayerGameStateDataNode& data);

    void SetPlayerAppearanceData(CPlayerAppearanceDataNode& data);

    void SetCameraData(const CPlayerCameraDataNode& data);
	void UpdatePendingCameraData();

	void UpdateWeaponTarget();
	void UpdatePendingTargetData();
	void UpdateTargettedEntityPosition();
	void UpdateWeaponAimPosition();
	void ConvertEntityTargetPosToWeaponRangePos(Vector3 const& entityOffsetPos, Vector3& weaponRangePos);
	bool GetActualWeaponAimPosition(Vector3 &vAimPos);

    void SetPlayerPedGroupData(CPlayerPedGroupDataNode& data);
	void SetPlayerWantedAndLOSData(const CPlayerWantedAndLOSDataNode& data);
    void SetAmbientModelStreamingData(const CPlayerAmbientModelStreamingNode& data);
	void SetPlayerGamerData(const CPlayerGamerDataNode& data);
	void SetPlayerExtendedGameStateData(const CPlayerExtendedGameStateNode& data);

	void SetFriendlyFireAllowed(const bool bValue) { m_bIsFriendlyFireAllowed = bValue; };
	const bool IsFriendlyFireAllowed() { return CNetwork::IsFriendlyFireAllowed() && m_bIsFriendlyFireAllowed; };

	void SetPassiveMode(const bool bValue) { m_bIsPassive = bValue; };
	const bool IsPassiveMode() { return m_bIsPassive; };

	//This function returns true if the Player ped can be left behind before a warp.
	bool  CanRespawnLocalPlayerPed();
	bool  CanRespawnClonePlayerPed(const ObjectId respawnNetObjId);

	ObjectId GetWaypointOverrideOwner()	{ return m_PlayerWaypointOwner; }
	void SetWaypointOverrideOwner(ObjectId player)  { m_PlayerWaypointOwner = player; }
	eHUD_COLOURS GetWaypointOverrideColor()	{ return m_PlayerWaypointColor; }
	void SetWaypointOverrideColor(eHUD_COLOURS color)	{ m_PlayerWaypointColor = color; }

	//PURPOSE
	//  Used to swap the player ped for a new ped. This is executed for local and remote peds 
	//   because the network object already exists.
	//PARAMS 
	//  newPlayerCoors - new player coordinates.
	//  killPed - true if the ped must be killed.
	//  respawnNetObjId - used for clones to create a network object with that id.
	bool  RespawnPlayerPed(const Vector3& newPlayerCoors, const bool killPed, const ObjectId respawnNetObjId);
	ObjectId GetRespawnNetObjId() const { return m_RespawnNetObjId; }

	// Resurrection
	void  Resurrect(const Vector3& newCoors, const bool triggerNetworkRespawnEvent);
	u32   GetLastResurrectionTime() const { return m_lastResurrectionTime; }
	virtual void  ResurrectClone();

	//Get the Physical index of the player we are spectating.
	ObjectId  GetSpectatorObjectId() const { return m_SpectatorObjectId; }
	//Set the Physical index of the player we are spectating.
	void  SetSpectatorObjectId(const ObjectId spectatorId);
	//Returns true if player is in spectator mode.
	bool  IsSpectating( ) const { return (NETWORK_INVALID_OBJECT_ID != m_SpectatorObjectId); }

	//Get the Physical index of the player we are antagonistic to.
	PhysicalPlayerIndex  GetAntagonisticPlayerIndex() const { return m_AntagonisticPlayerIndex; }
	//Set the Physical index of the player we are antagonistic to.
	void  SetAntagonisticPlayerIndex(PhysicalPlayerIndex antagonisticPlayerIndex);
	//Returns true if player has an Antagonist.
	bool  IsAntagonisticToPlayer( ) const { return (INVALID_PLAYER_INDEX != m_AntagonisticPlayerIndex); }

	bool IsAntagonisticToThisPlayer(const CNetObjPlayer * pNetObjPlayer);

	// Access custom Waypoint data
	bool HasActiveWaypoint() const { return m_bHasActiveWaypoint; }
	bool OwnsWaypoint() const { return m_bOwnsWaypoint; }
	float GetWaypointX() const { return m_fxWaypoint; }
	float GetWaypointY() const { return m_fyWaypoint; }
	ObjectId GetWaypointObjectId() const { return m_WaypointObjectId; }

	// get voice loudness (valid for local or remote)
	float GetLoudness() const { return m_fVoiceLoudness; }

	// sync look at - whether we sync look at position
	void SetSyncLookAt(bool bSyncLookAt) { m_bSyncLookAt = bSyncLookAt; }

	// toggle for the player ped looking at other talking players
	static void SetLookAtTalkers(bool bLookAtTalkers);

	//---

	// is fading remote player to 0.
	inline bool   IsFadingPlayerToZero() { return m_FadePlayerToZero; }

	void ReceivedStatEvent(const float data[PlayerCard::NETSTAT_SYNCED_STAT_COUNT]);
	float GetStatValue(PlayerCard::eNetworkedStatIds stat);
	u8 GetStatVersion() const {return m_statVersion;}

    void GetScarAndBloodData(unsigned &numBloodMarks, CPedBloodNetworkData bloodMarkData[CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED],
                             unsigned &numScars,      CPedScarNetworkData  scarData[CPlayerCreationDataNode::MAX_SCARS_SYNCED]) const;

    void SetScarAndBloodData(unsigned numBloodMarks, const CPedBloodNetworkData bloodMarkData[CPlayerCreationDataNode::MAX_BLOOD_MARK_SYNCED],
                             unsigned numScars,      const CPedScarNetworkData  scarData[CPlayerCreationDataNode::MAX_SCARS_SYNCED]);


	void GetSecondaryPartialAnimData(CSyncedPlayerSecondaryPartialAnim& pspaData);
	void SetSecondaryPartialAnimData(const CSyncedPlayerSecondaryPartialAnim& pspaData);
	void UpdateSecondaryPartialAnimTask();

	float GetExtendedScopeFOVMultiplier(float fFOV);

	void SetClanDecorationHashes(atHashString collectionHash, atHashString presetHash) { m_clandecoration_collectionhash = collectionHash; m_clandecoration_presethash = presetHash; };
	void PlaceClanDecoration();

	void IncrementIncidentNum() { m_incidentNum++; } // it doesn't matter if this wraps as it is only used to distinguish between 2 difference incidents assigned to the same player in the incidents array handler
	u32 GetIncidentNum() const { return (u32)m_incidentNum; }

	void SetPendingDamage(u32 damage) { m_pendingDamage = (u16)damage; }

	void  AddToPendingDamage(u32 damage, bool bKilled) 
	{ 
		m_pendingDamage += (u16)damage; 
		m_bAbortNMShotReaction = !bKilled; 
		m_addToPendingDamageCalled = true; 
	}

	u32	  GetPendingDamage() const { return (u32) m_pendingDamage; }
	void  AbortNMShotReaction() { m_bAbortNMShotReaction = true; m_pendingDamage = 0; }

	void  DeductPendingDamage(u32 damage) 
	{ 
		if (m_pendingDamage >= (u16)damage) 
			m_pendingDamage -= (u16)damage; 
		else
			m_pendingDamage = 0;
	}

	void SetNetworkedDamagePack(atHashString damagePack) { 	m_networkedDamagePack = damagePack.GetHash(); }
	void ClearNetworkedDamagePack() { m_networkedDamagePack = 0; }

	void  SetLastReceivedHealth(u32 health, u32 timestamp) 
	{
		if (timestamp==0)
			timestamp = 1;

		u32 diff = timestamp - m_lastReceivedHealthTimestamp; 
		bool bNewTimeLessThanBefore = (diff & 0x80000000u) != 0;

		if (m_lastReceivedHealthTimestamp == 0 || 
			bNewTimeLessThanBefore || 
			(timestamp >= m_lastReceivedHealthTimestamp)) 
		{
			m_lastReceivedHealth = (u16)health; 
			m_lastReceivedHealthTimestamp = timestamp; 
			weaponDebugf3("## SET LAST RECEIVED HEALTH %s - health = %d. Timestamp = %d ##", GetLogName(), m_lastReceivedHealth, m_lastReceivedHealthTimestamp);
		}
	}

	u32 GetLastReceivedHealth() const { return (u32)m_lastReceivedHealth; }
	u32 GetLastReceivedHealthTimestamp() const { return m_lastReceivedHealthTimestamp; }

	void SetLastDamageInfo(const Vector3& position, const Vector3& direction, u32 hitComponent)
	{
		m_lastDamagePosition = position;
		m_lastDamageDirection = direction;
		m_lastHitComponent = (u16)hitComponent;
	}

	bool SetDeathResponseTask(CTask* pTask) 
	{ 
		if (!m_pDeathResponseTask) 
		{ 
			m_pDeathResponseTask = pTask; 
			return true; 
		}

		return false;
	}

	CTask* GetDeathResponseTask() const { return m_pDeathResponseTask; }
	bool IsRunningDeathResponseTask() const { return m_bRunningDeathResponseTask; }

	bool HasMoney();
	void SetRemoteHasMoney(bool bValue) { m_remoteHasMoney = bValue; }
	bool GetRemoteHasMoney() { return m_remoteHasMoney; }

	const bool HasRemoteScarData() { return m_remoteHasScarData; }
	void SetRemoteScarData(const bool& bHasScarData, const float& u, const float& v, const float& scale) { m_remoteHasScarData = bHasScarData; m_remoteScarDataU = u; m_remoteScarDataV = v; m_remoteScarDataScale = scale; }
	void GetRemoteScarData(float& u, float& v, float& scale) { u = m_remoteScarDataU; v = m_remoteScarDataV; scale = m_remoteScarDataScale; }

	const u8 GetSentScarCount() { return m_sentScarCount; }
	void SetSentScarCount(const u32 count) { m_sentScarCount = (u8) count; }

	const u8 GetRemoteFakeWantedLevel() { return m_remoteFakeWantedLevel; }
	void SetRemoteFakeWantedLevel(u8 level) { m_remoteFakeWantedLevel = level; }

#if __DEV

	void DebugRenderTargetData(void);

#endif /* __DEV */

	inline bool GetCanOwnerMoveWhileAiming(void) const {	return m_canOwnerMoveWhileAiming;	}

	bool  GetDisableLeavePedBehind() const {return m_bDisableLeavePedBehind;}
	void  SetDisableLeavePedBehind(const bool disableLeavePedBehind) {m_bDisableLeavePedBehind=disableLeavePedBehind;}

	void  SetLocalPlayerWeaponPickupAndObject(CPickup* pWeaponPickup, CObject* pWeaponObject);

	static bool HasCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex);
	static const CPedHeadBlendData* GetCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex);
	static void CachePlayerHeadBlendData(PhysicalPlayerIndex playerIndex, CPedHeadBlendData& headBlendData);
	static bool ApplyCachedPlayerHeadBlendData(CPed& ped, PhysicalPlayerIndex playerIndex);
	static void ClearCachedPlayerHeadBlendData(PhysicalPlayerIndex playerIndex);
	static void ClearAllCachedPlayerHeadBlendData();
	static void ClearAllRemoteCachedPlayerHeadBlendData();

	float GetVehicleShareMultiplier() const { return m_vehicleShareMultiplier; }

	virtual bool CanShowGhostAlpha() const;
	virtual void ProcessGhosting();

	// sets up the player to be ghosted for other players not participating in the script, so they cannot interfere with him. Can only be called on the local player.
	void SetPlayerGhostedForNonScriptPartipants(CGameScriptId& scriptId BANK_ONLY(, const char* scriptInfo = ""));
	void ResetPlayerGhostedForNonScriptPartipants(BANK_ONLY(const char* scriptInfo = ""));

	void ForceSendPlayerAppearanceSyncNode();

private:

    virtual void UpdateBlenderData();

	//---

	// Tests to see if we can use a ped to generate a target offset from for target syncing....
	bool IsTargetEntitySuitableForOffsetAiming(CEntity const* entity, Vector3& offset);
	bool GenerateTargetForOffsetAiming(CEntity const* entity, Vector3& offset);

	// returns true if the player ped is still using the single player model
	bool HasSinglePlayerModel() const;

	virtual bool FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const;

	//Creates the network object used for player ped swap, returns true if the network object was created.
	bool  CreateRespawnNetObj(const ObjectId respawnNetObjId);

    // this function is used to update the non physical player data, for the local player
    // this is data that is used to determine when to switch players between physical and non-physical
    // representations in the world
    void UpdateNonPhysicalPlayerData();

    // preforms some tests to determine whether the player ped is visible and unobstructed onscreen
    void DoOnscreenVisibilityTest();

	// makes player's head turn to look at the nearest talking player
	void LookAtNearestTalkingPlayer();

    // updates the current vehicle and entry point we want to prestream on remote machines
    void UpdateAnimVehicleStreamingTarget();

	void UpdateInitialPedCrewColor();

	void UpdateLocalWantedSystemFromRemoteVisibility();

    bool AllowFixByNetwork() const;
    bool UseBoxStreamerForFixByNetworkCheck() const;
    bool CheckBoxStreamerBeforeFixingDueToDistance() const { return true; }
    void UpdateFixedByNetwork(bool fixByNetwork, unsigned reason, unsigned npfbnReason);

	// During Player Respawn State the player is faded out to 0
	void  UpdateRemotePlayerFade();
	void  CheckAndStartClonePlayerFadeOut(const int previousState, const int newState);

	// manage look at for network players
	void UpdateLookAt();

	void UpdateRemotePlayerAppearance();

	static const unsigned STANDING_ON_OBJECT_STREAM_IN_CHECK_INTERVAL = 200;
	void UpdateCloneStandingOnObjectStreamIn();
	void UpdateLockOnTarget();

#if __BANK
    virtual u32 GetNetworkInfoStartYOffset();
    virtual bool ShouldDisplayAdditionalDebugInfo();
    virtual void GetHealthDisplayString(char *buffer) const;
	virtual void DisplayNetworkInfoForObject(const Color32 &col, float scale, Vector2 &screenCoords, const float debugTextYIncrement);
#endif // __BANK

    // update the bitmask of the players in the same tutorial session as the local player
    static void UpdateCachedTutorialSessionState();

	static bool ApplyHeadBlendData(CPed& ped, CPedHeadBlendData& headBlendData);

    // Antagonistic player index. Default antagonistic player index is INVALID_PLAYER_INDEX.
	PhysicalPlayerIndex     m_AntagonisticPlayerIndex;

    // the tutorial index is used to split players into a fake session involving just other players playing the same tutorial
    u8 m_tutorialIndex;

    // tutorial instance ID, used to distinguish between different instances of gang tutorials which one the player is in
    u8 m_tutorialInstanceID;

	// pending  tutorial index is cached here until object ownership is managed and it can be copied to m_tutorialinstanceid
	u8 m_pendingTutorialIndex;

	// pending tutorial instance ID is cached here until object ownership is managed and it can be copied to m_tutorialInstanceID
	u8 m_pendingTutorialInstanceID;

	//count down before force into the pending tutorial state index and instance 
	s16 m_pendingTutorialSessionChangeTimer;
	//count down to allow time for all clones to be removed to signal to scripts that nothing will disappear on screen
	s16 m_pendingTutorialPopulationClearedTimer;

	//Time after tutorial session change to prevent local player generating scenario point peds
	static s16 m_postTutorialSessionChangeHoldOffPedGenTimer;

	u16	m_pendingDamage;					// the amount of damage waiting to be sent and acked in weapon damage events to this player

    Vector3	m_LastSpottedByPolice;		// The coordinates where this player was last spotted. This defines the radius the player has to escape.
	Vector3 m_CurrentSearchAreaCentre;
	u32	m_iTimeSinceLastSpotted;		// The amount of time since the player was last spotted in milliseconds

	s32	m_pendingVehicleWeaponIndex;

    //Reference to the network global object ID of the network ped created for team swapping purposes. 
	ObjectId     m_RespawnNetObjId;

    bool                  m_bAlwaysClonePlayerVehicle;	// if set, the vehicle the player is in will always be cloned
    bool                  m_visibleOnscreen               : 1; // set when the player is visible onscreen, relative to the game camera
	bool                  m_UsingFreeCam                  : 1; // set when the player is using freecam
	bool                  m_UsingCinematicVehCam             : 1; // set when the player is using cinematic cam
    bool                  m_UseKinematicPhysics           : 1;
    bool                  m_bPendingTutorialSessionChange : 1;
	bool                  m_bWaitingPopulationClearedInTutorialSession : 1;
	bool				  m_bIsFriendlyFireAllowed	:1;
	bool				  m_bIsPassive				:1;

	// PRF
	bool				   m_PRF_BlockRemotePlayerRecording	: 1;
	bool				   m_PRF_UseScriptedWeaponFirePosition : 1;
    bool                   m_PRF_IsSuperJumpEnabled         : 1;
	bool				   m_PRF_IsShockedFromDOTEffect : 1;
	bool				   m_PRF_IsChokingFromDOTEffect : 1;

	bool				   m_HasToApplyWeaponToVehicle : 1;

	bool				   m_ConcealedOnOwner : 1;	// true if the owner of the player concealed themselves
    // custom waypoint settings
	unsigned			  m_WaypointLocalDirtyTimestamp;
    bool                  m_bHasActiveWaypoint            : 1;
    bool                  m_bOwnsWaypoint                 : 1;
	float                 m_fxWaypoint;
	float                 m_fyWaypoint;
	ObjectId              m_WaypointObjectId;

    struct CameraDataNodeCache
	{
		CameraDataNodeCache()
		{
			Clear();
		}

		void CopyPlayerCameraDataNode(CPlayerCameraDataNode const& dataNode)
		{
			m_Position					= dataNode.m_Position;
			m_playerToTargetAimOffset	= dataNode.m_playerToTargetAimOffset;
			m_lockOnTargetOffset		= dataNode.m_lockOnTargetOffset;
			m_eulersX					= dataNode.m_eulersX;
			m_eulersZ					= dataNode.m_eulersZ;
		
			m_UsingFreeCamera			= dataNode.m_UsingFreeCamera;
			m_UsingCinematicVehCamera	= dataNode.m_UsingCinematicVehCamera;
			m_morePrecision				= dataNode.m_morePrecision;
			m_largeOffset				= dataNode.m_largeOffset;
			m_aiming					= dataNode.m_aiming;

			m_longRange					= dataNode.m_longRange;
			m_freeAimLockedOnTarget		= dataNode.m_freeAimLockedOnTarget;

			m_targetId					= dataNode.m_targetId;
			m_bAimTargetEntity			= dataNode.m_bAimTargetEntity;
		}

		void Clear(void)
		{
			m_Position.Zero();
			m_playerToTargetAimOffset.Zero();
			m_lockOnTargetOffset.Zero();

			m_eulersX					= 0.0f; 
			m_eulersZ					= 0.0f; 

			m_UsingFreeCamera			= false;
			m_UsingCinematicVehCamera	= false;
			m_morePrecision				= false;  
			m_largeOffset				= false;    
			m_aiming					= false;
			m_longRange					= false;
			m_freeAimLockedOnTarget		= false;

			m_targetId					= NETWORK_INVALID_OBJECT_ID;
			m_bAimTargetEntity			= false;
		}

		Vector3  m_Position;  			        // the position offset of the camera if aiming - or absolute position if using a free camera
		Vector3  m_playerToTargetAimOffset;		// position we're aiming at (used to compute pitch and yaw on the clone).
		Vector3  m_lockOnTargetOffset;			// if locked onto a target, offset from target position to actual lock on pos.
		float    m_eulersX;         			// camera matrix euler angles
		float    m_eulersZ;         			// camera matrix euler angles
		ObjectId m_targetId;					// if we're aiming at a target we pass that info instead of pitch and yaw.
		bool     m_UsingFreeCamera;             // if set, this player is controlling a free camera
		bool     m_UsingCinematicVehCamera;		// if set, this player using the cinematic vehicle camera
		bool     m_morePrecision;   			// if set, more precise camera data is used
		bool     m_largeOffset;     			// if set, the camera is far away from the player
		bool	 m_aiming;						// if the player is currently aiming a weapon
		bool	 m_longRange;					// if the player is aiming a long range weapon (sniper rifle - 1500m range) or short range (<150m)
		bool	 m_freeAimLockedOnTarget;		// if the player is free aim locked onto a target...
		bool	 m_bAimTargetEntity;
	};

	CameraDataNodeCache   m_pendingCameraTargetData;
	bool                  m_pendingCameraDataToSet:1;
	bool                  m_pendingTargetDataToSet:1;
    bool                  m_FadePlayerToZero;
    s16					  m_standingOnStreamedInNetworkObjectIntervalTimer;	
	u32                   m_MaxPlayerFadeOutTime;

    //Default spectator object Id is NETWORK_INVALID_OBJECT_ID, that means that the player is not in spectator mode.
	ObjectId              m_SpectatorObjectId;

    s16                   m_respawnInvincibilityTimer;

    CNetViewPortWrapper  *m_pViewportWrapper;
	Vec3V				  m_viewportOffset;

    char                  m_DisplayName[MAX_PLAYER_DISPLAY_NAME];

    // onscreen visiblity testing:
    static const unsigned PLAYER_VISIBILITY_TEST_FREQUENCY = 200;
    static const unsigned PLAYER_VISIBILITY_TEST_RANGE     = 200;
	WorldProbe::CShapeTestSingleResult	m_VisibilityProbeHandle;      // used for probing the player's visibility
	u32                   m_TimeOfLastVisibilityCheck;			// the time a visibility check was last done
	u32                   m_VisibilityQueryTimer;				// set when something is interested in the visibility of the player

	// flag whether look at position should be synced
	bool    m_bSyncLookAt;
	bool    m_bIsCloneLooking;
    s16     m_nLookAtTime;
    RegdPed m_pLookAtTarget;
	s16     m_nLookAtIntervalTimer; 
	s16     m_nLookAtStartDelayTime;
	Vector3 m_vLookAtPosition;
	Vector3 m_lastDamagePosition;
	Vector3 m_lastDamageDirection;

    Vector3 m_vExtendedPopulationRangeCenter;

    // ambient model streaming
    u32 m_AllowedPedModelStartOffset;     // The currently allowed ped model start offset for this player (see populationstreaming.cpp/.h)
    u32 m_AllowedVehicleModelStartOffset; // The currently allowed vehicle model start offset for this player (see populationstreaming.cpp/.h)
	
	// cached decal decoration list info
	s32 m_DecorationListCachedIndex;
	float m_DecorationListTimeStamp;
	u32 m_DecorationListCachedBits[NUM_DECORATION_BITFIELDS];
	int m_DecorationListCrewEmblemVariation;

    // vehicle anim streaming variables
    s32      m_TargetVehicleEntryPoint;
    ObjectId m_TargetVehicleIDForAnimStreaming;

    bool	 m_newSecondaryAnim : 1;
	bool	 m_SecondaryPartialSlowBlendDuration : 1;

	// used to ensure the player crew color variations are initially set on the ped.
	bool m_bCopsWereSearching : 1;  // Retain if the cops were last searching for this player
	bool m_bCrewPedColorSet : 1;
	bool m_bAbortNMShotReaction : 1;

	bool m_remoteHasMoney : 1;

	bool m_bRespawnInvincibilityTimerRespot : 1;

	u8   m_statVersion;         // This increments everytime the stats change, that way menus can know to refresh.

	static const unsigned INVALID_SECONDARY_TASK_SEQUENCE_ID = MAX_TASK_SEQUENCE_ID;
	static const unsigned MAX_SECONDARY_TASK_SEQUENCE_ID     = MAX_TASK_SEQUENCE_ID-1;

	struct sSecondaryAnimData
	{
		sSecondaryAnimData()
		{
			Reset();
		}

		void Reset()
		{
			m_bMoVEScripted = false;
			m_SecondaryPartialAnimDictHash =0;
			mCommonData.mTaskMoveAnimData.Reset();

			m_pCurrentSecondaryTaskScripted = NULL;
			m_SecondaryTaskSequenceID = 0;
		};

		//Task scripted accessors
		u32& SecondaryBoneMaskHash () { return mCommonData.mTaskScriptedData.m_SecondaryBoneMaskHash; }
		const u32& SecondaryBoneMaskHash () const { return mCommonData.mTaskScriptedData.m_SecondaryBoneMaskHash; }
		u32& SecondaryPartialAnimClipHash () { return mCommonData.mTaskScriptedData.m_SecondaryPartialAnimClipHash; }
		const u32& SecondaryPartialAnimClipHash () const { return mCommonData.mTaskScriptedData.m_SecondaryPartialAnimClipHash; }
		u32& SecondaryPartialAnimFlags () { return mCommonData.mTaskScriptedData.m_SecondaryPartialAnimFlags; }

		//Task MoVE accessors
		u32& MoveNetDefHash () { return mCommonData.mTaskMoveAnimData.m_MoveNetDefHash; }
		u32& StatePartialHash () { return mCommonData.mTaskMoveAnimData.m_StatePartialHash; }
		u32& FloatHash0 () { return mCommonData.mTaskMoveAnimData.m_FloatHash0; }
		u32& FloatHash1 () { return mCommonData.mTaskMoveAnimData.m_FloatHash1; }
		u32& FloatHash2 () { return mCommonData.mTaskMoveAnimData.m_FloatHash2; }
		float& Float0 () { return mCommonData.mTaskMoveAnimData.m_Float0; }
		float& Float1 () { return mCommonData.mTaskMoveAnimData.m_Float1; }
		float& Float2 () { return mCommonData.mTaskMoveAnimData.m_Float2; }
		bool& IsBlocked () { return mCommonData.mTaskMoveAnimData.m_IsBlocked; }

		struct sTaskScriptedAnimData
		{
			void Reset()
			{
				m_SecondaryBoneMaskHash =0;
				m_SecondaryPartialAnimClipHash =0;
				m_SecondaryPartialAnimFlags =0;
			};

			u32		 m_SecondaryBoneMaskHash;
			u32		 m_SecondaryPartialAnimClipHash;
			u32		 m_SecondaryPartialAnimFlags;
		};

		struct sTaskMoVEAnimData
		{
			void Reset()
			{
				m_FloatHash0 =0;
				m_FloatHash1 =0;
				m_FloatHash2 =0;
				m_StatePartialHash =0;
				m_MoveNetDefHash   =0;
				m_Float0 =0.0f;
				m_Float1 =0.0f;
				m_Float2 =0.0f;
				m_IsBlocked = false;
			};

			u32		m_FloatHash0;
			u32		m_FloatHash1;
			u32		m_FloatHash2;
			u32		m_StatePartialHash;
			u32		m_MoveNetDefHash;
			float	m_Float0;
			float	m_Float1;
			float	m_Float2;
			bool	m_IsBlocked;
		};

		union {sTaskScriptedAnimData mTaskScriptedData; sTaskMoVEAnimData mTaskMoveAnimData;} mCommonData;
		u32		 m_SecondaryPartialAnimDictHash;
		bool	 m_bMoVEScripted; //Indicates whether this data is syncing a CTaskScriptedAnimation or CTaskMoVEScripted secondary task

		CTaskFSMClone*     m_pCurrentSecondaryTaskScripted;  //keeps tracked of synced task for local player, so sequence ID increment can be maintained if it changes with no gap between
		u32				   m_SecondaryTaskSequenceID;

	};

	sSecondaryAnimData mSecondaryAnimData;

	atHashString	m_clandecoration_collectionhash;
	atHashString	m_clandecoration_presethash;

	u32				m_networkedDamagePack;

    // voice loudness
	float m_fVoiceLoudness;
	u32   m_uThrottleVoiceLoudnessTime;
  
	u16		m_lastHitComponent;
	u16		m_lastReceivedHealth;				// the last health received via a network update or in a weapon damage event ack

	taskRegdRef<CTask>  m_pDeathResponseTask;

	float m_remoteScarDataScale;

	u8 m_visibilityMismatchTimer; // used to restore cached visibilty when the local visibility of a clone differs (*hack!*)

	u8 m_sentScarCount;

	u8 m_remoteFakeWantedLevel;

	bool m_remoteHasScarData : 1;

	//----
	// Information sent over the network to sync up aiming....
	struct TargetCloningInformation
	{
		enum TargetMode
		{
			TargetMode_Invalid	= 0,
			TargetMode_PosWS	= 1,
			TargetMode_Entity	= 2
		};

		TargetCloningInformation()
		:
			m_CurrentPlayerPos(VEC3_ZERO),
			m_lockOnTargetAimOffset(VEC3_ZERO),
			m_CurrentAimTargetPos(VEC3_ZERO),	
			m_CurrentAimTargetEntity(NULL),	
			m_CurrentWeaponHash(0),
			m_targetMode(TargetMode_Invalid),	
			m_FreeAimLockedOnTarget(false),
			m_CurrentAimTargetID(NETWORK_INVALID_OBJECT_ID),
			m_WasAiming(false),
			m_IsAiming(false),
			m_ScriptLockOnState((unsigned)CEntity::HLOnS_NONE),
			m_ScriptAimTargetID(NETWORK_INVALID_OBJECT_ID)
		{}

		void			SetAimTarget(CPed const* playerPed, Vector3 const& targetPos);
		void			SetAimTarget(CPed const* playerPed, const CEntity& targetEntity);
		void			Clear(void);
		void			SetCurrentAimTargetID(ObjectId const& targetId)			{ m_CurrentAimTargetID = targetId;		}
		void			SetCurrentAimTargetEntity(CEntity* aimTarget)			{ m_CurrentAimTargetEntity = aimTarget;	}
		void			SetLockOnTargetAimOffset(Vector3 const& offset)			{ m_lockOnTargetAimOffset = offset;		}
		void			SetFreeAimLockedOnTarget(bool const locked)				{ m_FreeAimLockedOnTarget = locked;		}
		void			SetHasValidTarget(TargetMode const targetMode)			{ m_targetMode = targetMode;			}
		void			SetCurrentWeaponHash(u32 const weaponHash)				{ m_CurrentWeaponHash = weaponHash;		}
		void			SetWasAiming(bool wasAiming)							{ m_WasAiming = wasAiming;				}
		void			SetIsAiming(bool isAiming)								{ m_IsAiming = isAiming;				}

		Vector3 const&	GetCurrentPlayerPos() const			{ return m_CurrentPlayerPos;		}
		Vector3 const&	GetLockOnTargetAimOffset() const	{ return m_lockOnTargetAimOffset;				}
		Vector3 const&	GetCurrentAimTargetPos() const		{ return m_CurrentAimTargetPos;					}
		RegdEnt const&	GetCurrentAimTargetEntity() const	{ return m_CurrentAimTargetEntity;				}
		bool			HasValidTarget() const				{ return m_targetMode != TargetMode_Invalid;	}
		TargetMode		GetTargetMode() const				{ return m_targetMode;							}
		bool			IsFreeAimLockedOnTarget() const		{ return m_FreeAimLockedOnTarget;				}
		ObjectId		GetCurrentAimTargetID() const		{ return m_CurrentAimTargetID;					}
		u32				GetCurrentWeaponHash() const		{ return m_CurrentWeaponHash;					}
		bool			IsAiming(void) const				{ return m_IsAiming;							}
		bool			WasAiming(void) const				{ return m_WasAiming;							}

		void			SetScriptAimTargetID(ObjectId targetId) { m_ScriptAimTargetID = targetId; }
		ObjectId		GetScriptAimTargetID() { return m_ScriptAimTargetID; }

		void			SetScriptLockOnState(u32 lockOnState) { m_ScriptLockOnState = lockOnState; }
		unsigned		GetScriptLockOnState()	{ return m_ScriptLockOnState; }

		unsigned		GetLockOnTargetState(CPed const* playerPed) const;
		ObjectId		GetLockOnTargetID(CPed const* playerPed) const;
#if __DEV

		void			DebugPrint(Vector3 const& pos) const;

#endif /*__DEV */

	private:

		// CLONE ONLY...
		Vector3		m_lockOnTargetAimOffset;	// Store the offset sent over the network...
		ObjectId	m_CurrentAimTargetID;		// Sent over the network but used by clones only to find an object and set weapon aiming position from it.
		bool		m_WasAiming;				// Was I aiming during the last update?
		bool		m_IsAiming;					// Am I aiming with this update?

		// OWNER ONLY...
		Vector3		m_CurrentPlayerPos;			// cache the position the player is at when the target was set so if the player is teleported / setpos'ed or set transform'ed we know the aiming data is invalid...
		Vector3		m_CurrentAimTargetPos;		// the current target position for the player, set by various gun tasks
		RegdEnt		m_CurrentAimTargetEntity;	// if the player is locked onto something, we use that rather than a pos - object Id is sent over the network
		u32			m_CurrentWeaponHash;		// what weapon we're we holding when the target info was set? sudden weapon changes can cause asserts to fire...
		ObjectId	m_ScriptAimTargetID;
		u32			m_ScriptLockOnState;
		// BOTH...
		TargetMode	m_targetMode;				// set when the player has a valid aim target on the owner and used to update weapon aim position on clones when target is sent across.
		bool		m_FreeAimLockedOnTarget;	// set when the player is locked onto it's target - used by CommandIsPlayerFreeAimingAtEntity()
	};

	TargetCloningInformation	m_TargetCloningInfo;
	Vector3						m_TargetAimPosition;
	RegdEnt						m_prevTargetEntityCache;

public:

	// setting of the current aim target used by various weapon tasks
	void					SetAimTarget(Vector3& targetPos);
	void					SetAimTarget(const CEntity& targetEntity);
	bool					HasValidTarget() const						{ return m_TargetCloningInfo.HasValidTarget(); }
	inline CEntity const*	GetAimingTarget(void)				const	{ return m_TargetCloningInfo.GetCurrentAimTargetEntity();	}
	inline bool				IsFreeAimingLockedOnTarget(void)	const	{ return m_TargetCloningInfo.IsFreeAimLockedOnTarget();	}
	Vector3 const&			GetCurrentAimTargetPos() const				{ return m_TargetCloningInfo.GetCurrentAimTargetPos(); }

	void					SetScriptAimTargetObjectId(ObjectId targetId) { m_TargetCloningInfo.SetScriptAimTargetID(targetId); }
	void					SetScriptAimTargetLockOnState(unsigned lockOnState) { m_TargetCloningInfo.SetScriptLockOnState(lockOnState); }

	static void				TriggerStatUpdate() {ms_forceStatUpdate = true;}
	
	// is corrupt (wrt: team, i.e. dirty cop)
	static bool IsCorrupt()  { return ms_IsCorrupt; }
	static void SetIsCorrupt(bool bIsCorrupt) { ms_IsCorrupt = bIsCorrupt; }

	//----

	inline bool IsBattleAware(void) const { gnetAssertf(GetPlayerPed() && GetPlayerPed()->IsNetworkClone(), "Error : Function should only be used on clones - use CPedIntelligence::IsBattleAware for local player"); return m_bBattleAware; }

	//----

	static void UpdateHighestWantedLevelInArea();
	static eWantedLevel GetHighestWantedLevelInArea();
	inline bool IsUsingExtendedPopulationRange(void) const { return m_bUseExtendedPopulationRange; }
	inline Vector3 const& GetExtendedPopulationRangeCenter(void) const { return m_vExtendedPopulationRangeCenter; }

	inline void SetUsingExtendedPopulationRange(const Vector3& center) { m_bUseExtendedPopulationRange = true; m_vExtendedPopulationRangeCenter = center; }
	inline void ClearUsingExtendedPopulationRange()					   { m_bUseExtendedPopulationRange = false; }

private:

	// store whether we are battle aware or not....
	bool m_bBattleAware;

	static bool ms_IsCorrupt; // flag whether player is corrupt 

    // static bool to toggle looking at talkers
	static bool ms_bLookAtTalkers;
	static bool ms_forceStatUpdate;
	static u16 ms_statUpdateCounter;

    // Functions to check for and hide objects when the local player or the object is in a tutorial
	virtual bool ShouldObjectBeHiddenForTutorial() const;
	virtual void HideForTutorial();
	void UpdateStats();

	float CalcStatValue(const CPlayerCardXMLDataManager::PlayerCard& rCard, int stat, int characterSlot);
	float CalcStatValueHelper(u32 statHash);

	void UpdateVehicleWantedDirtyState();

	void SanityCheckVisibility();

    bool ShouldFixAndDisableCollisionDueToControls() const;
    bool ShouldOverrideBlenderForSecondaryAnim() const;

    static CPlayerSyncTree *ms_playerSyncTree;
    static CPlayerScopeData ms_playerScopeData;

	static eWantedLevel ms_HighestWantedLevelInArea;

	bool m_bInsideVehicle  : 1;
    bool m_VehicleJumpDown : 1;
	bool m_bUseExtendedPopulationRange : 1;
	bool m_canOwnerMoveWhileAiming:1; // Can the owner move while aiming?
	bool m_addToPendingDamageCalled:1; 

	bool m_bRemoteReapplyColors : 1;
    bool m_MyVehicleIsMyInteresting : 1;

#if FPS_MODE_SUPPORTED
	bool m_bUsingFirstPersonCamera : 1;
	bool m_bUsingFirstPersonVehicleCamera : 1;
#endif

#if !__FINAL
	bool	m_bDebugInvincible : 1;
#endif

	u8	 m_incidentNum;             // The number of incidents assigned to this player, used enabling multiple incidents to be assigned to the same player in the incidents array handler
    bool m_DisableRespawnFlashing;  // Used to disable respawn flashing for this player

	float m_stats[PlayerCard::NETSTAT_SYNCED_STAT_COUNT];

	Color32 m_remoteColor[MAX_PALETTE_COLORS];

    // the time this player was last resurrected after death - used to reject damage events during resurrect.
	u32 m_lastResurrectionTime;

	u32	m_lastReceivedHealthTimestamp;		// the timestamp of the last health received

	//True when we want to disable leave ped behind when a remote player leaves the session
	bool m_bDisableLeavePedBehind  : 1;

    bool m_UsingLeftTriggerAimMode : 1;

	bool m_bCloneRespawnInvincibilityTimerCountdown : 1;
	bool m_bRunningDeathResponseTask : 1;
	bool m_bRespawnInvincibilityTimerWasSet : 1;
	bool m_bLastVisibilityStateReceived : 1;

	bool m_bWantedVisibleFromPlayer : 1; // Track whether the local player is visible in the from players wanted system
	bool m_bIgnoreTaskSpecificData : 1;  // ignore task specific data after resurrection

#if FPS_MODE_SUPPORTED
	bool m_bInFirstPersonIdle : 1; 
	bool m_bStickWithinStrafeAngle : 1;
	bool m_bUsingSwimMotionTask : 1;
	bool m_bOnSlope : 1;
#endif
	ObjectId m_PlayerWaypointOwner;
	eHUD_COLOURS  m_PlayerWaypointColor;
	ObjectId m_LockOnTargetID;
	
	unsigned m_LockOnState;

	float m_vehicleShareMultiplier;

	float m_remoteScarDataU;
	float m_remoteScarDataV;

    float m_CityDensitySetting;

	unsigned m_LatestTimeCanSpawnScenarioPed;

	PlayerFlags m_ghostPlayerFlags; // used when the player is a ghost we only want to have ghost interactions between certain players
#if __BANK
	const char* m_ghostScriptInfo;
#endif

	static CGameScriptId ms_ghostScriptId; // used in ghost mode to prevent interactions between players running the script and other non-participants

	static RegdPickup ms_pLocalPlayerWeaponPickup;
	static RegdObj ms_pLocalPlayerWeaponObject;

	static CPedHeadBlendData ms_cachedLocalPlayerHeadBlendData;
	static bool ms_hasCachedLocalPlayerHeadBlendData;
	static CPedHeadBlendData ms_cachedPlayerHeadBlendData[MAX_NUM_PHYSICAL_PLAYERS]; // the default player head blend data without masks, etc
	static PlayerFlags ms_hasCachedPlayerHeadBlendData;
};

#endif  // NETOBJPLAYER_H
