// Title	:	Ped.h
// Author	:	Gordon Speirs
// Started	:	02/09/99

#ifndef _PED_H_
#define _PED_H_

// Rage headers
#include "rmcore/lodgroup.h"
#include "math/amath.h"
#include "physics/constrainthandle.h"
#include "physics/gtaMaterialManager.h"
#include "system/stack_collector.h"

// Framework headers
#include "fwtl/pool.h"

// Game headers
#include "ai/spatialarray.h"
#include "Audio/PedAudioEntity.h"
#include "Audio/SpeechAudioEntity.h"
#include "debug/debugrecorder.h"
#include "Ik/IkManager.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDefines.h"
#include "Peds/PedFlags.h"
#include "Peds/TaskData.h"
#include "Peds/PedIntelligence/PedAILod.h"
#include "Peds/PedFootstepHelper.h"
#include "Peds/PedMotionData.h"
#include "Peds/PedMoveBlend/PedMoveBlendBase.h"
#include "Peds/PedShockingEvents.h"
#include "Peds/PedType.h"
#include "Peds/PlayerInfo.h"
#include "Peds/Relationships.h"
#include "Physics/gtaInst.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "Scene/Physical.h"
#include "security/obfuscatedtypes.h"
#include "security/ragesecgameinterface.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Animation/AnimTaskFlags.h"
#include "timecycle/TimeCycleConfig.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Systems/VfxLiquid.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxPed.h"
#include "Weapons/Inventory/PedInventory.h"
#include "Weapons/Inventory/PedWeaponManager.h"
#include "Task/Movement/JetpackObject.h"
#include "Peds/Horse/horseDefines.h"

#if GTA_REPLAY
#include "control/replay/ped/PedPacket.h"
#endif // GTA_REPLAY

// Network headers
#include "Network/Objects/Entities/NetObjPed.h"
#include "Profile/Profiler.h"

RAGE_DECLARE_CHANNEL(viseme)

#define visemeAssert(cond)						RAGE_ASSERT(viseme,cond)
#define visemeAssertf(cond,fmt,...)				RAGE_ASSERTF(viseme,cond,fmt,##__VA_ARGS__)
#define visemeVerifyf(cond,fmt,...)				RAGE_VERIFYF(viseme,cond,fmt,##__VA_ARGS__)
#define visemeErrorf(fmt,...)					RAGE_ERRORF(viseme,fmt,##__VA_ARGS__)
#define visemeWarningf(fmt,...)					RAGE_WARNINGF(viseme,fmt,##__VA_ARGS__)
#define visemeDisplayf(fmt,...)					RAGE_DISPLAYF(viseme,fmt,##__VA_ARGS__)
#define visemeDebugf1(fmt,...)					RAGE_DEBUGF1(viseme,fmt,##__VA_ARGS__)
#define visemeDebugf2(fmt,...)					RAGE_DEBUGF2(viseme,fmt,##__VA_ARGS__)
#define visemeDebugf3(fmt,...)					RAGE_DEBUGF3(viseme,fmt,##__VA_ARGS__)
#define visemeLogf(severity,fmt,...)			RAGE_LOGF(viseme,severity,fmt,##__VA_ARGS__)
#define visemeCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,viseme,severity,fmt,##__VA_ARGS__)
#if !__NO_OUTPUT && !__FINAL
#define visemeCondDebugf3(cond,fmt,...)			visemeCondLogf(cond,::rage::DIAG_SEVERITY_DEBUG3,fmt,##__VA_ARGS__)
#else
#define visemeCondDebugf3(cond,fmt,...)
#endif

#define ENABLE_DRUNK 0
#define ENABLE_VISEMES 1
#define DR_RECORD_PED_FLAGS DR_ENABLED && 0

class CAmbientModelVariations;
class CAnimPlayer;
class CAnimStateSustainInfo;
class CComponentReservationManager;
class CCoverPoint;
class CEntryExit;
class CFacialDataComponent;
class CHorseComponent;
class CRagdollConstraintComponent;
class CModelIndex;
struct sMotionTaskData;
class CMotionTaskDataSet;
struct sDefaultTaskData;
class CDefaultTaskDataSet;
class CMovePed;
class CPed;
class CBaseCapsuleInfo;
class CPedComponentClothInfo;
class CPedComponentSetInfo;
class CPedDamageResponse;
class CPedHelmetComponent;
class CPedIntelligence;
class CPedPhoneComponent;
class CPedPropData;
class CPedVariationData;
class CPlayerInfo;
class CPlayerPedData;
class CQueriableInterface;
class CTask;
class CWanted;
class fragInstNMGta;
class CPedStreamRequestGfx;
class CPedVfx;
class CSeatManager;
class CTaskMotionBase;
class CTaskMotionPed;
class CPlayerSpecialAbility;
class CPedIKSettingsInfo;
class CTaskFlagsData;
struct CQuadrupedSpringTuning;
class naEnvironmentGroup;
class CHealthConfigInfo;
class CPedPropRequestGfx;
struct CPopCycleConditions;
class CPedClothCollision;

namespace rage
{
	class characterClothController;
}

enum ePedLodState
{
	PLS_HD_NA			= 0,		//	- cannot go into HD at all
	PLS_HD_NONE			= 1,		// 	- no HD resources currently Loaded
	PLS_HD_REQUESTED	= 2,		//  - HD resource requests are in flight
	PLS_HD_AVAILABLE	= 3,		//  - HD resources available & can be used
	PLS_HD_REMOVING		= 4,		//  - HD not available & scheduled for removal
	PLS_HD_MAX = PLS_HD_REMOVING
};
CompileTimeAssert(PLS_HD_MAX < BIT(3));

typedef struct pedLight {
	ConfigLightSettings m_lightSettings;
	Vec4V m_lightOffset;
	Vec4V m_lightVolumeColor;
	float m_lightVolumeIntensity;
	float m_lightVolumeSize;
	float m_lightVolumeExponent;
	float m_lightFade;
	float m_lightShadowFade;
	float m_lightSpecularFade;
} pedLight;

class CControlledByInfo
{
public:
	CControlledByInfo(bool isNetwork, bool isPlayer)
		:
		m_isControlledByNetwork(isNetwork),
		m_isPlayer(isPlayer)
	{;}
	CControlledByInfo(const CControlledByInfo& rhs)
		:
	m_isControlledByNetwork(rhs.m_isControlledByNetwork),
		m_isPlayer(rhs.m_isPlayer)
	{;}

    void Set(const CControlledByInfo& rhs)
    {
        m_isControlledByNetwork = rhs.m_isControlledByNetwork;
        m_isPlayer = rhs.m_isPlayer;
    }

	bool IsControlledByNetwork() const				{ return m_isControlledByNetwork; }
	bool IsControlledByLocalAi() const				{ return (!m_isControlledByNetwork && !m_isPlayer); }
	bool IsControlledByNetworkAi() const			{ return (m_isControlledByNetwork && !m_isPlayer); }
	bool IsControlledByLocalOrNetworkAi() const		{ return !m_isPlayer; }
	bool IsControlledByLocalPlayer() const			{ return (!m_isControlledByNetwork && m_isPlayer); }
	bool IsControlledByNetworkPlayer()	 const		{ return (m_isControlledByNetwork && m_isPlayer); }
	bool IsControlledByLocalOrNetworkPlayer() const	{ return m_isPlayer; }

protected:
	bool	m_isControlledByNetwork : 1;
	bool	m_isPlayer : 1;
};

class CRewardedVehicleExtension : public fwExtension
{
public:
	EXTENSIONID_DECL(CRewardedVehicleExtension, fwExtension);

	CRewardedVehicleExtension() {}
	~CRewardedVehicleExtension() {}

	void GivePedRewards(CPed* pPed, const CVehicle* pVehicle);

	u32	GetNumRewardedVehicles()				const	{ return m_RewardedVehicles.GetCount(); }
	const CVehicle* GetRewardedVehicle(u32 index)		const	{ Assert(index < (u32)(m_RewardedVehicles.GetCount())); return m_RewardedVehicles[index].Get(); }

private:
	void AddRewardedVehicle(const CVehicle* pVehicle)		{Assert(pVehicle); m_RewardedVehicles.PushAndGrow(RegdConstVeh(pVehicle));}
	void RemoveRewardedVehicle(u32 index)			{Assert(index < (u32)(m_RewardedVehicles.GetCount())); m_RewardedVehicles.Delete(index);}

	atArray<RegdConstVeh> m_RewardedVehicles;	// Store the vehicles which the ped has already got rewards from by entering it.
};

void ReportArmorMismatch(u32, u32);

class CPed : public CPhysical
{
	DECLARE_RTTI_DERIVED_CLASS(CPed, CPhysical);

	friend struct CPedOffsetInfo;
	friend class CPlayerPedSaveStructure;
	friend class CNetObjPed;
	friend class CPedVfx;
#if GTA_REPLAY
	friend class CPacketSpeechPain;
	friend class CPacketSpeechStop;
#endif

	// This is PROTECTED as only CPedFactory is allowed to call this - make sure you use CPedFactory!
	// DONT make this public!
protected:
	explicit CPed(const eEntityOwnedBy ownedBy, const CControlledByInfo& pedControlInfo, u32 modelIndex);
	friend class CPedFactory;

	void Init(bool reinit=false, const CPedModelInfo* pPedModelInfo=NULL);

public:
	~CPed();

	FW_REGISTER_CLASS_POOL(CPed);

	// EJ: Memory Optimization
	virtual u8 GetNumProps() const;

	virtual void SetModelId(fwModelId modelId);
	void SetupSkeletonData(crSkeletonData& skelData) const;
	void SetModelId(fwModelId modelId, const fwMvClipSetId &overrideMotionClipSet, bool bBlendIdle=true);

	void						SetPedType			(ePedType val)			{ m_nPedType = val; }
	ePedType					GetPedType			() const				{ return m_nPedType; }

	virtual bool				CreateDrawable();
	virtual void				DeleteDrawable();

	virtual bool				RequiresProcessControl() const						{return true;}
	virtual bool				ProcessControl();
	virtual void				ProcessFrozen();// Called when the entity is frozen instead of process control
	        void				ProcessControl_Turret();

#if __BANK
    void                        ProcessHeadBlendDebug();
	void						ProcessStreamingDebug();
	virtual void				InitDebugName();
#endif // __BANK

	bool						CanBeFrozen() const;

	virtual bool				CanBeDeleted() const								{ return CanBeDeleted(false, true); }
	bool						CanBeDeleted(bool bPedsInCarsCanBeDeleted, bool bDoNetworkChecks = true, bool bMissionPedsCanBeDeleted = false) const;
	void						UpdatePedCountTracking();

	virtual void				FlagToDestroyWhenNextProcessed();
	void						FlagToDestroyWhenNextProcessed(bool bChangeCarStatus);

	//////////////////////////////////////////////////////////////////////////
	// network object setup functions (need custom implementations rather than using the NETWORK_OBJECT_TYPE_DECL helper macro)
	virtual netObject* CreateNetworkObject(	const ObjectId objectID,
		const PhysicalPlayerIndex playerIndex,
		const NetObjFlags localFlags,
		const NetObjFlags globalFlags,
		const unsigned numPlayers);
	static netObject* StaticCreateNetworkObject( const ObjectId objectID,
		const PhysicalPlayerIndex playerIndex,
		const NetObjFlags globalFlags,
		const unsigned numPlayers);
	static netObject* StaticCreatePlayerNetworkObject( const ObjectId objectID,
		const PhysicalPlayerIndex playerIndex,
		const NetObjFlags globalFlags,
		const unsigned numPlayers);
	virtual unsigned GetNetworkObjectType();

	//////////////////////////////////////////////////////////////////////////
	// Position and heading
	virtual void SetHeading(float new_heading);	
	virtual void SetMatrix(const Matrix34& mat, bool bUpdateGameWorld = true, bool bUpdatePhysics = true, bool bWarp = false);
	void SetPitch(float fPitch);
	void SetHeadingAndPitch(float fHeading, float fPitch);	

	// Heading change rate accessors
	void	SetHeadingChangeRate(const float fRate) { m_MotionData.SetHeadingChangeRate(fRate); }
	float	GetHeadingChangeRate() const;
	void	RestoreHeadingChangeRate();

	// Desired heading
	void	SetDesiredHeading(const float fDesiredHeading);
	void	SetDesiredHeading(const Vector3 & vTarget);
	float	GetDesiredHeading() const { return m_MotionData.GetDesiredHeading(); }

	// Current heading	
	float	GetCurrentHeading() const { return m_MotionData.GetCurrentHeading(); }

	// Facing bone heading - used for strafe transitions/independent mover
	float	GetFacingDirectionHeading() const;

	// Velocity heading
	bool	GetVelocityHeading(float& fVelocityHeading) const;

	// Desired pitch
	void	SetDesiredPitch(const float fDesiredPitch);
	void	SetDesiredPitch(const Vector3 & vTarget);
	float	GetDesiredPitch() const { return m_MotionData.GetDesiredPitch(); }

	// Current pitch	
	float GetCurrentPitch() const { return m_MotionData.GetCurrentPitch(); }

	void SetBoundPitch(float fBoundPitch) { m_MotionData.SetPedBoundPitch(fBoundPitch);}
	float GetBoundPitch() const { return m_MotionData.GetBoundPitch(); }
	void SetDesiredBoundPitch(float fBoundPitch) { m_MotionData.SetDesiredPedBoundPitch(fBoundPitch);}
	float GetDesiredBoundPitch() const { return m_MotionData.GetDesiredBoundPitch(); }

	void SetBoundHeading(float fBoundHeading) { m_MotionData.SetPedBoundHeading(fBoundHeading);}
	float GetBoundHeading() const { return m_MotionData.GetBoundHeading(); }
	void SetDesiredBoundHeading(float fBoundHeading) { m_MotionData.SetDesiredPedBoundHeading(fBoundHeading);}
	float GetDesiredBoundHeading() const { return m_MotionData.GetDesiredBoundHeading(); }
	
	void SetBoundOffset(const Vector3& vBoundOffset) { m_MotionData.SetPedBoundOffset(vBoundOffset);}
	const Vector3& GetBoundOffset() const { return m_MotionData.GetBoundOffset(); }
	void SetDesiredBoundOffset(const Vector3& vBoundOffset) { m_MotionData.SetDesiredPedBoundOffset(vBoundOffset);}
	const Vector3& GetDesiredBoundOffset() const { return m_MotionData.GetDesiredBoundOffset(); }

	float GetCurrentMainMoverCapsuleRadius() const { return m_fCurrentMainMoverCapsuleRadius; }
	float GetDesiredMainMoverCapsuleRadius() const { return m_fDesiredMainMoverCapsuleRadius; }
	float GetCurrentCapsuleHeightOffset() const { return m_fCurrentCapsultHeightOffset; }
	void  OverrideCapsuleRadiusGrowSpeed(float f) {m_fOverrideCapsuleRadiusGrowSpeed = f;}
	void  ResizePlayerLegBound(bool b) {m_bResizePlayerLegBound = b;}
	void ResetProcessBoundsCountdown() { m_ProcessBoundsCountdown = 0; }

	// Teleport the ped, deals with updating the physics capsule
	virtual void Teleport(const Vector3& vecSetCoors, float fSetHeading, bool bKeepTasks=false, bool bTriggerPortalRescan = true, bool bKeepIK=false, bool bWarp=true, bool bKeepRagdoll = false, bool bResetPlants=true);
	bool PositionAnyPedOutOfCollision();	// only used by ped group teleport atm

	void SetInitialState();
	void Resurrect(const Vector3& NewCoors, float fNewHeading, const bool triggerNetworkRespawnEvent = false, const bool bDoTeleport = true);

	// Externally controlled velocity
	virtual const Vector3& GetVelocity() const;
	virtual void SetVelocity(const Vector3& vecMoveSpeed);
	virtual const Vector3& GetAngVelocity() const;

	// Externally driven DOF
	void SetupExternallyDrivenDOFs();
	void DeleteExternallyDrivenDOFs();
	void ProcessExternallyDrivenDOFs();
	void InvalidateExternallyDrivenDOFs();
	void InvalidateUpperBodyFixupDOFs();
	void ProcessColarDOFs();
	crFrame* GetExternallyDrivenDofFrame() { return m_pExternallyDrivenDOFFrame; }

	float GetHighHeelExpressionDOF();
	float GetHighHeelExpressionMetaDOF();

	// First person camera DOF
	bool GetFirstPersonCameraDOFs(Vec3V_InOut vTrans, QuatV_InOut qRot, float &fMinX, float &fMaxX, float &fMinY, float &fMaxY, float &fMinZ, float &fMaxZ, float &fWeight, float &fov) const;

	bool GetConstraintHelperDOFs(bool rightHand, float& weight, Vec3V_InOut translation, QuatV_InOut rotation);

	// fun times with ammo depletion 
	s32 GetAmmoDiminishingCount( void ) { return m_nAmmoDiminishingCount; }
	void SetAmmoDiminishingCount( s32 nNewAmmoCount ) { m_nAmmoDiminishingCount = nNewAmmoCount; }

	//////////////////////////////////////////////////////////////////////////
	// Setup
	void SetDefaultDecisionMaker();
	void SetCharParamsBasedOnManagerType();

	// Adjust the cull range for the ped based on model info.
	void SetDefaultRemoveRangeMultiplier();

	void SetSpecialNetworkLeave();
	bool IsSpecialNetworkLeaveActive() { return m_specialNetworkLeaveActive && m_specialNetworkLeaveTimeRemaining > 0; }

	void SetSpecialNetworkLeaveWhenDead();

#if !__SPU
	virtual fwMove *CreateMoveObject();
	virtual const fwMvNetworkDefId &GetAnimNetworkMoveInfo() const;
	virtual void SetActivePoseFromSkel();
	virtual float GetUpdateFrequencyThreshold() const;
	virtual u8 GetMotionTreePriority(fwAnimDirectorComponent::ePhase phase) const;
	virtual bool InitIkRequest(crmtRequest &request, crmtRequestIk &ikRequest);
	virtual bool InitExpressions(crExpressionsDictionary&);
#endif // !__SPU

#if !__NO_OUTPUT
	static void PrintSkeletonData();
#endif

	//////////////////////////////////////////////////////////////////////////
	// Tasks (moved from CPedData)
	CTask* ComputeDefaultDrivingTask(const CPed& ped, CVehicle* pVehicle, bool bUseExistingNodes );
	CTask* ComputeDefaultTask(const CPed& ped);
	//Compute the wander task for the ped type.

	CTask* ComputeWanderTask(const CPed& ped, CPopCycleConditions* pSpawnConditions = NULL);

	CTask* ComputePrimaryMotionTask(const bool bLowLod = false) const;
	CTask* GetMotionTaskFromType(const sMotionTaskData* pMotionTaskData, const bool bLowLod = false) const;

#if FPS_MODE_SUPPORTED
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);
	bool IsFirstPersonShooterModeEnabledForPlayer(const bool bCheckStrafe = true, const bool bIncludeClone = false, const bool bDisableForDominantScriptedCams = false, const bool bDisableForDominantCutsceneCams = true, const bool bCheckFlags = true) const;
	bool IsFirstPersonCameraOrFirstPersonSniper() const;
	bool IsFirstPersonShooterModeStickWithinStrafeAngleThreshold() const;
	bool UseFirstPersonUpperBodyAnims(bool bCheckStrafe) const;
	virtual const Matrix34& GetThirdPersonSkeletonObjectMtx(int boneIdx) const; // from fwEntity
#endif // FPS_MODE_SUPPORTED

	void RenderLightOnPhone(const CEntity* entity);
	void RenderPedLight(bool forceOn);
	void RenderPedFeetLights(bool forceOn);

	//////////////////////////////////////////////////////////////////////////
	// Accessors
	const CHealthConfigInfo* GetPedHealthInfo() const;
	inline CPedModelInfo* GetPedModelInfo() const {return (CPedModelInfo*)GetBaseModelInfo();}

	s32							GetAge() const;		
	bool						IsMale() const;
	bool						IsFiring() const;
	bool						IsUsingFiringVariation() const;
	
	float						GetAttackStrength() const;
	u32							GetMotivation() const;
	bool						CheckBraveryFlags(u32 flags) const;
    bool                        CheckCriminalityFlags(u32 flags) const;
	bool						CheckSexinessFlags(u32 flags) const;
	bool						CheckAgilityFlags(u32 flags) const;

	// Inventory/weapon manager
	const CPedInventory*		GetInventory() const { return m_inventory; }
	CPedInventory*				GetInventory() { return m_inventory; }
	const CPedWeaponManager*	GetWeaponManager() const { return m_weaponManager; }
	CPedWeaponManager*			GetWeaponManager() { return m_weaponManager; }
	const CWeaponInfo*			GetEquippedWeaponInfo() const { return m_weaponManager ? m_weaponManager->GetEquippedWeaponInfo() : NULL; }
	const CWeaponInfo*			GetSelectedWeaponInfo() const { return m_weaponManager ? m_weaponManager->GetSelectedWeaponInfo() : NULL; }
	const CWeaponInfo*			GetCurrentWeaponInfoForHeldObject() const { return m_weaponManager ? m_weaponManager->GetCurrentWeaponInfoForHeldObject() : NULL; }

#if USE_TARGET_SEQUENCES
	// Target sequence accessors
	CTargetSequenceHelper*			FindActiveTargetSequence() const;
#endif // USE_TARGET_SEQUENCES

	// Facial Data
	const CFacialDataComponent*	GetFacialData() const { return m_facialData; }
	CFacialDataComponent*		GetFacialData() { return m_facialData; }
	void						HandleFacialAnimEvent( const crProperty* pProp );

	// Hand/Ankle cuff Data
	const CRagdollConstraintComponent*	GetRagdollConstraintData() const { return m_ragdollConstraintData; }
	CRagdollConstraintComponent*		GetRagdollConstraintData() { return m_ragdollConstraintData; }

	// VFX Data
	const CPedVfx*				GetPedVfx() const { return m_pPedVfx; }
	CPedVfx*					GetPedVfx() { return m_pPedVfx; }

	static void					ClearPedsThatCanGetWetThisFrame();
	static void					AddPedThatCanGetWetThisFrame(CPed* pPed);
	static s32					GetNumPedsThatCanGetWetThisFrame();
	static CPed*				GetPedThatCanGetWetThisFrame(s32 idx);

	float						GetHeatScaleOverride() const { return (float)(m_heatScaleOverride)/255.0f; }
	u8							GetHeatScaleOverrideU8() const { return m_heatScaleOverride; }
	void						SetHeatScaleOverride(u8 heatScale) { m_heatScaleOverride = heatScale; }

	CPlayerInfo *GetPlayerInfo() const { return m_pPlayerInfo; }
	void SetPlayerInfo(CPlayerInfo* pInfo);
	CWanted* GetPlayerWanted() const {return (m_pPlayerInfo!=NULL ? (&m_pPlayerInfo->GetWanted()) : NULL);}

	// Control(er) accessors (returns NULL on non player controlled peds)
	const CControl * GetControlFromPlayer() const;
	CControl * GetControlFromPlayer();

	CPedIntelligence* GetPedIntelligence() const {return m_pPedIntelligence;}

	// Health/armour/endurance
	void InitHealth();

	inline void		SetArmour(const float fNewArmour) 
	{ 
		m_nArmourLost = Max(m_nArmourLost + m_nArmour - fNewArmour, 0.f);
		m_nArmour = fNewArmour; 
	}
	inline float	GetArmourLost() const { return m_nArmourLost; }
	inline float	GetArmour() const { return m_nArmour; }
	inline float	GetHealthLost() const { return (GetMaxHealth() - GetHealth()) - GetHealthLostScript(); }	// Ignore health lost that script cause
	inline void		SetHealthLostScript(float f) { m_nHealthLostScript = f; }
	inline float	GetHealthLostScript() const { return m_nHealthLostScript; }

	inline float	GetFatiguedHealthThreshold() const { return m_fFatiguedHealthThreshold; }
	inline float	GetInjuredHealthThreshold() const { return m_fInjuredHealthThreshold; }
	inline float	GetDyingHealthThreshold() const { return m_fDyingHealthThreshold; }

	// This is quite "horrible" but if MaxHealth is within a reasonable range we calculate the injured threshold as a percentage
	// instead of an absolute value since peds might spawn or get less than 200 in MaxHealth, could be so low it actually triggers
	// the bleeding state
	inline float	GetHurtHealthThreshold() const 
	{
	//	const float DEFAULT_MAX_HEALTH = CPlayerInfo::PLAYER_DEFAULT_MAX_HEALTH;
	//	if (GetMaxHealth() >= DEFAULT_MAX_HEALTH)
	//		return m_fHurtHealthThreshold;

		//const float DEFAULT_HURT_HEALTH = CPlayerInfo::PLAYER_DEFAULT_MAX_HEALTH - m_fHurtHealthThreshold;
		const float CurrentHurtHealth = GetMaxHealth() - m_fHurtHealthThreshold;

		// Ok we should always be hurt? When taking some damage again we get hurt then
		if (CurrentHurtHealth <= 0.f)
			return GetMaxHealth() - 5.f;

		return m_fHurtHealthThreshold;
		//return (CurrentHurtHealth / DEFAULT_HURT_HEALTH) * (m_fHurtHealthThreshold - m_fInjuredHealthThreshold) + m_fInjuredHealthThreshold;
	}

	inline void		SetHurtEndTime(u32 TimeMs) { m_HurtEndTime = TimeMs; }
	inline u32		GetHurtEndTime() const { return m_HurtEndTime; }	

	void			SetEndurance(float fNewEndurance, bool bNetCall = false);
	inline float	GetEndurance() const { return m_fEndurance; }
	void			SetMaxEndurance(float fNewEndurance, bool bNetCall = false);
	inline float	GetMaxEndurance() const { return m_fMaxEndurance; }

#if !__FINAL
	bool IsDebugInvincible() const;
#endif

	inline float GetTimeSinceLastShotFired() const		{ return m_fTimeSinceLastShotFired; }
	inline void SetTimeSinceLastShotFired(float val)	{ m_fTimeSinceLastShotFired = val; }

	inline s32	GetMinOnGroundTimeForStunGun() const				{ return m_MinOnGroundTimeForStunGun; }
	inline void SetMinOnGroundTimeForStunGun(s32 timeInMillisecs)	{ m_MinOnGroundTimeForStunGun = timeInMillisecs; }

	static bool IsHeadShot(s32 nHitBoneTag) { return NetworkInterface::IsGameInProgress() ? nHitBoneTag==BONETAG_HEAD : (nHitBoneTag==BONETAG_HEAD || nHitBoneTag==BONETAG_NECK); }
	static bool IsTorsoShot(s32 nHitBoneTag) { return (nHitBoneTag==BONETAG_SPINE0 || nHitBoneTag==BONETAG_SPINE1 || nHitBoneTag==BONETAG_SPINE2 || nHitBoneTag==BONETAG_SPINE3 || nHitBoneTag==BONETAG_NECK); }
	
	enum eDamagedBodyPart
	{
		DAMAGED_NONE		= 0,
		DAMAGED_UNKNOWN		= BIT0,
		DAMAGED_LEFT_LEG	= BIT1,
		DAMAGED_RIGHT_LEG	= BIT2,
		DAMAGED_LEFT_ARM	= BIT3,
		DAMAGED_RIGHT_ARM	= BIT4,
		DAMAGED_TORSO		= BIT5,
		DAMAGED_HEAD		= BIT6,
		DAMAGED_LUNGS		= BIT7,
		DAMAGED_STUN		= BIT8,
		DAMAGED_GOTOWRITHE	= BIT9,	// Kind of a trigger if this damage should trigger writhe straight away, to save some cycles!
		DAMAGED_ANY_VALID	= DAMAGED_LEFT_LEG | DAMAGED_RIGHT_LEG | DAMAGED_LEFT_ARM | DAMAGED_RIGHT_ARM | DAMAGED_TORSO | DAMAGED_HEAD,
		DAMAGED_HAS_BEEN	= DAMAGED_UNKNOWN | DAMAGED_LEFT_LEG | DAMAGED_RIGHT_LEG | DAMAGED_LEFT_ARM | DAMAGED_RIGHT_ARM | DAMAGED_TORSO | DAMAGED_HEAD | DAMAGED_LUNGS | DAMAGED_STUN,
	};

#if !__FINAL
	atString GetBodyPartDamageDbgStr() const;
#endif

	static u16 ConvertBoneToBodyPart(s32 nHitBoneTag);
	inline void ReportBodyPartDamage(u16 nDamagedBodyPart) { m_DamagedBodyParts |= nDamagedBodyPart; }
	inline void ClearBodyPartDamage(u16 nDamagedBodyPart) { m_DamagedBodyParts &= ~nDamagedBodyPart; }
	inline bool IsBodyPartDamaged(u16 nDamagedBodyPart) const { return ((m_DamagedBodyParts & nDamagedBodyPart) ? true : false); }
	inline u16 GetBodyPartDamage() const { return m_DamagedBodyParts; }

	// Coverpoints
	inline CCoverPoint * GetCoverPoint() const	{ return m_pCoverPoint; }
	void SetCoverPoint(CCoverPoint * val);
	void ReleaseCoverPoint();

	inline CCoverPoint * GetDesiredCoverPoint() const { return m_pDesiredCoverPoint; }
	void SetDesiredCoverPoint(CCoverPoint * val);
	void ReleaseDesiredCoverPoint();

	void SetWeaponDamageComponent(int Component){ m_weaponDamageComponent = Component; }
	int GetWeaponDamageComponent() const		{ return m_weaponDamageComponent; }

	u32 GetDamageDelayTimer() const			{ return m_nDamageDelayTimer;	}
	void  SetDamageDelayTimer(u32 iTimer)	{ m_nDamageDelayTimer = iTimer; }

	// This timer counts the time since falling off a bike due to exhaustion and determines when the ragdoll damage scanner
	// stops going easy on this ped.
	u32 GetExhaustionDamageTimer() const { return m_nExhaustionDamageTimer; }
	void SetExhaustionDamageTimer(u32 nTimeNow) { m_nExhaustionDamageTimer = nTimeNow; }

	// This time allows us to ramp up the perceived river flow speed on peds after entering a section of water defined as "rapids" over
	// a short period of time.
	u32 GetRiverRapidsTimer() const { return m_nRiverRapidsTimer; };
	void ResetRiverRapidsTimer() { m_nRiverRapidsTimer = fwTimer::GetTimeInMilliseconds(); }

	float GetTimeSincePedInWater() const		{ return m_fTimeSincePedInWater; }
	void SetTimeSincePedInWater(float val)		{ m_fTimeSincePedInWater = val; }
	
	u8 GetModelLodIndex() const;

	// Targeting
	virtual float GetLockOnTargetableDistance() const { return m_fTargetableDistance; }
	virtual void SetLockOnTargetableDistance(float fTargetableDistance) { m_fTargetableDistance = fTargetableDistance; }

	float GetTargetThreatOverride() const { return m_fTargetThreatOverride; }
	void SetTargetThreatOverride(float fThreat) { m_fTargetThreatOverride = fThreat; }

	//////////////////////////////////////////////////////////////////////////
	// Physics
	virtual int  InitPhys();
	virtual void AddPhysics();
	virtual void RemovePhysics();
	virtual void DeActivatePhysics();
	void DeactivatePhysicsAndDealWithRagdoll(bool ragdollIsAsleep = false);

	virtual void EnableCollision(phInst* pInst=NULL, u32 nIncludeflags = 0);
	virtual void DisableCollision(phInst* pInst=NULL, bool bCompletelyDisabled=false);

	virtual void UpdateEntityFromPhysics(phInst *pInst, int nLoop);

	void SimulatePhysicsForLowLod();

	virtual void ProcessPrePhysics();
	virtual ePhysicsResult ProcessPhysics(float TimeStep, bool bCanPostpone,int nTimeSlice);
	bool ProcessIsAsleep(bool bNeedsToBeAwake);

	// Updates last matrix from current, and sets current matrix to entity matrix
	// Don't want to call it every time SetHeading / Position / Matrix are called!!
	void UpdateRagdollMatrix(bool bWarp);

	// Helper function for updating the flags used to determine if a player ped should ignore collisions with another ped
	// using kinematic physics mode.
	void CheckForPlayerBeingSquashedByKinematicPed(const phContactIterator impacts, Vec3V_Ref kinematicPedNormal, Vec3V_Ref nonKinematicNormal);

	virtual void ProcessProbes(const Matrix34& testMat);
	void ProcessProbesNonVertical(); //called from processProbes when required, so doesn't need to be virtual
	bool ValidateGroundPhysical(const phInst &groundInstance, Vec3V_In vGroundPos, int iComponent) const;
	void DoKickPed(phContactIterator &impacts, bool bValidGround = false);
	void DoHitPed(phContactIterator &impacts);
	
	bool GetHitByDoor() const { return m_bHitByDoor; }
	bool GetPreviouslyHitByDoor() const { return m_bPreviouslyHitByDoor; }
	void SetHitByDoor(bool bHitByDoor) { m_bHitByDoor = bHitByDoor; }
	void SetPreviouslyHitByDoor(bool bPreviouslyHitByDoor) { m_bPreviouslyHitByDoor = bPreviouslyHitByDoor; }
	u32 GetRagdollDoorContactTime() const { return m_uRagdollDoorContactTime; }
	void SetRagdollDoorContactTime(u32 uRagdollDoorContactTime) { m_uRagdollDoorContactTime = uRagdollDoorContactTime; }

	static void LoadVisualSettings();

	// B*2983081
	// We save the clanId when the playerInfo is set on a player ped.
	// this allows us to still have a valid clanId for decals on the ped when the player leaves the game.
	rlClanId GetSavedClanId() const { return m_SavedClanId; }
	void SetSavedClanId(const rlClanId& clan) { m_SavedClanId = clan; }

	bool GetIsKeepingCurrentHelmetInVehicle() const { return m_bKeepingCurrentHelmetInVehicle; }

private:
#if __BANK
	// Toggle processing of PreComputeImpacts via RAG.
	static bool sm_PreComputeImpacts;	
	static bool sm_PreComputeCapsuleProbeImpact;
	static bool sm_PreComputeHorseLowerLegImpact;
	static bool sm_PreComputeHorsePropBlockerImpact;
	static bool sm_PreComputeImpactsForMover;
	static bool sm_PreComputeImpactsForRagdoll;
	static bool sm_PreComputeMainCapsuleImpact;
	static bool sm_PreComputePlayerLowerLegImpact;
	static bool sm_PreComputeQuadrupedLowerLegImpact;
	static bool sm_EnableRagdollForHighVelImpactsWhileOnTrain;
public:
	static bool sm_deadRagdollDebug;
private:
#endif // __BANK
	// Light info
	static pedLight sm_mainLight;
	static pedLight sm_fpsLight;
	static pedLight sm_footLight;

public:
	void ProcessPreComputeImpacts(phContactIterator impacts);
	void ProcessPreComputeImpactsForMover(phContactIterator &impacts);
	void ProcessPreComputeImpactsForRagdoll(phContactIterator &impacts);

	bool ProcessPreComputeMainCapsuleImpact(phContactIterator &impacts, bool &bCoverageFirstImpactFound, Vec3V_InOut vCoveredLeft, Vec3V_InOut vCoveredRight);

#if PED_USE_CAPSULE_PROBES
	bool ProcessPreComputeCapsuleProbeImpact(phContactIterator &impacts, bool &frontImpactComputed);
#endif // PED_USE_CAPSULE_PROBES
	
	void ProcessPreComputeHorseLowerLegImpact(phContactIterator &impacts);
	void ProcessPreComputeHorsePropBlockerImpact(phContactIterator &impacts);

	void ProcessPreComputeQuadrupedLowerLegImpact(phContactIterator &impacts, bool &rearImpactComputed);

	bool ShouldObjectBeImpossibleToPush(const CEntity& entity, const phInst& instance, const phCollider* pCollider, int component);

#if PLAYER_USE_LOWER_LEG_BOUND || PED_USE_CAPSULE_PROBES
	bool CanPedMoveObjectWithLowerLegs(const phInst* instance, const phCollider* collider, int component);
#endif // PLAYER_USE_LOWER_LEG_BOUND || PED_USE_CAPSULE_PROBES
#if PLAYER_USE_LOWER_LEG_BOUND
	void ProcessPreComputePlayerLowerLegImpact(phContactIterator &impacts);
#endif // PLAYER_USE_LOWER_LEG_BOUND

	bool DoKeepImpactForMover(const phContactIterator &impacts, int& otherInstClassTypeOut) const;
	bool CanDisableImpactForMover(phContactIterator &impacts);
	bool CanDisableImpactForEnterExitVehicle(const CEntity* pOtherEntity, phInst* pOtherInstance);
	bool CanActivateRagdollOnCollisionDuringClimb(const CEntity* pOtherEntity);
	bool CanActivateRagdollOnCollisionWhenOnVehicle(const CEntity* pOtherEntity, const bool bDontDetachOnWorldOrPedCollision);
	bool ZeroOutImpactZ(phContactIterator &impacts, ScalarV epsilon = ScalarV(V_FLT_SMALL_5));

	// Handles switching between standard ped bounds and smaller crouched bounds
	bool ProcessBounds(float fTimeStep, bool bForceUpdate = false);

	// Orient the main mover capsule to match the ped's spine bones
	void OrientBoundToSpine(float fBlendInDelta = NORMAL_BLEND_IN_DELTA, float fExtraZOffset = 0.0f);

	// Offset the main mover capsule bound to the furthest bone (in object space) in the XY plane in the given direction
	void OffsetBoundToFurthestBoneInXYDirection(Vec2V_In vDirection);

	// Clear the bound of any orientation/offset
	void ClearBound();

	inline void SetDesiredVelocity(const Vector3 & vVel) 
	{ 	
		Assert(vVel==vVel);
		Assertf(vVel.Mag2() < DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT, "Desired velocity Magnitude2 was invalid (%f) for %s", vVel.Mag2(), GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Not networked!");
		m_vDesiredVelocity = vVel; 

		PDR_ONLY(debugPlayback::RecordPedDesiredVelocity(GetCurrentPhysicsInst(), RCC_VEC3V(vVel), RCC_VEC3V(vVel)));
	}
	inline void SetDesiredAngularVelocity(const Vector3 & vAngVel) { 	Assert(vAngVel==vAngVel); m_vDesiredAngularVelocity = vAngVel; }

	void ProcessFallCollision();

	void ProcessPreCamera();
	bool ProcessPostCamera();
	void ProcessPostPhysics();

	// Get the bone tag from ragdoll component
	// Returns BONETAG_ROOT if the bone tag does not exist
	eAnimBoneTag GetBoneTagFromRagdollComponent(const int component) const;

	// Get the bone index from ragdoll component
	// Returns -1 if the bone tag does not exist
	s32 GetBoneIndexFromRagdollComponent(const int component) const;

	// Get the ragdoll component from the bone tag
	// Returns 0 if the bone tag does not exist
	s32 GetRagdollComponentFromBoneTag(const eAnimBoneTag boneTag) const;

	void CalcCameraAttachPositionForRagdoll(Vector3& pos) const;

	virtual bool CanUseKinematicPhysics() const;

	void SetPropRequestGfx(CPedPropRequestGfx* newGfx);
	CPedPropRequestGfx* GetPropRequestGfx() const { return m_propRequestGfx; }

	virtual const phInst* GetInstForKinematicMode() const { return GetAnimatedInst(); }

	void SetDesiredMainMoverCapsuleRadius(float fDesiredMainMoverCapsuleRadius, bool bIgnoreSizeCheck = false) 
	{ 
		if(bIgnoreSizeCheck || fDesiredMainMoverCapsuleRadius > m_fDesiredMainMoverCapsuleRadius) 
			m_fDesiredMainMoverCapsuleRadius = fDesiredMainMoverCapsuleRadius; 
	}

	void ClearPedBrainWhenDeletingPed();

	void CheckComponentCloth();

	void QueueClothPackageIndex(u8 packageIndex);
	void QueueClothPoseIndex(u8 poseIndex);
	u8 GetQueuedClothPackageIndex() const { return m_PackageIndex; }
	u8 GetQueuedClothPoseIndex() const { return m_PoseIndex; }
	void ClearClothController();
	void SetClothPackageIndex(u8 packageIndex, u8 transitionFrames);
	void SetClothForcePin(u8 framesToPin);
	void SetClothPoseIndex(u8 poseIndex);
	void SetClothLockCounter(u8 count);
	void SetClothPinRadiusScale(float radiusScale);
	void SetClothWindScale(float windScale);
	void SetClothNegateForce(bool bNegateForce);
	void SetClothForceSkin(bool bForceSkin);
	void SetClothIsFalling(bool bIsFalling);
	void SetClothIsSkydiving(bool bIsSkydiving);
	void SetClothIsProne(bool bIsProne);
	bool CalculateWeaponBound(float& capsuleLen, float& capsuleRadius, Mat34V_InOut boundMatrix);
#if GTA_REPLAY
	characterClothController *GetClothController(u32 idx) { replayAssertf(idx < PV_MAX_COMP, "CPed::GetClothController()....Index out ouf range."); return m_CClothController[idx]; }
#endif // GTA_REPLAY

	// Forced update of animation and AI
	void InstantAIUpdate(bool bForceFullUpdate = false);
	bool InstantAnimUpdateStart();
	void InstantAnimUpdateEnd();

	void InstantResetCloneDesiredMainMoverCapsuleData();
	void InstantResetDesiredMainMoverCapsuleDataForCutscene();

	u8 GetWeaponAnimationsIndex() const { return m_uWeaponAnimationsIndex; }

	void ResetDesiredMainMoverCapsuleData( bool bInstant = false );
	
protected:
	// This is a helper method used by ProcessBounds... don't want other classes calling it
	void SetBoundSize( const CBipedCapsuleInfo* pBipedCapsuleInfo, float fNewHeadHeight, float fNewHeightOffset, float fDesiredBoundSize, float fTimeStep );
	void SetDynamicEntityFlagsForWeapon(CObject* pWeaponObject, bool value);
	void SetDynamicEntityFlagsForWeapon(bool value);

#if __BANK
	#define ProcessAttachment(...) DebugProcessAttachment(__FILE__, __FUNCTION__, __LINE__)
	virtual void DebugProcessAttachment(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine);
#else
	void ProcessAttachment();
#endif

	enum PedConstraintType
	{
		PCT_None,
		PCT_RollAndPitch,
		PCT_Cylindrical,
		PCT_CylindricalFake,
		PCT_Max = PCT_Cylindrical
	};
	CompileTimeAssert(PCT_Max < BIT(2));

	// Controls the ped's rotational constraint
	PedConstraintType GetConstraintType() const;
	void SetConstraintType(PedConstraintType constraintType);
	bool IsConstraintValid();
	void ProcessRotationalConstraint();
	void AddRotationalConstraint();
public:
	void RemoveRotationalConstraint();
	void RemoveOverhangConstraint();
protected:
	virtual phInst* GetInstForKinematicMode() { return GetAnimatedInst(); }

	// Only the ped can change this
	// Don't want external forces editing these
	// To change the ped pitch and heading instantly use SetHeading
	// Most code should go through the desired heading and move blender will drive to it
	void SetCurrentPitch(const float fCurrentPitch);
	void SetCurrentHeading(const float fCurrentHeading);

	void ApplyQuadrupedSpring(float fInvTimeStep, float fOffset, const Vector3 &rvSpringPos, const Vector3 &rvForcePos, const Vector3 &rvGroundNormal, float &fOldSpringForceInOut, const CQuadrupedSpringTuning &rTuning);
	void ApplyHorseSpring(float fInvTimeStep, float fOffset, const Vector3 &rvSpringPos, const Vector3 &rvForcePos, const Vector3 &rvGroundNormal, float &fOldSpringForceInOut, const CQuadrupedSpringTuning &rTuning);

#if __DEV
	// Call process on any related ped data
	bool ProcessControl_Debug();
#endif // __DEV
	// Update the component reservations.
	void ProcessControl_ComponentReservations();
	// Update the peds inventory, weapons and any other accessories that need to be done before the AI update
	void ProcessControl_WeaponsAndAccessoriesPreIntelligence();
	// Main per frame update of the AI
	void ProcessControl_Intelligence(bool fullUpdate, float fOverrideTimeStep = -1.0f);
	// Update the peds inventory, weapons and any other accessories that need to be done after the AI update
	void ProcessControl_WeaponsAndAccessoriesPostIntelligence(bool fullUpdate);
	// reset a load of flags before the main intelligence update
	void ProcessControl_ResetVariables();
	// Update any audio related members, variables
	void ProcessControl_Audio();
	// Block if necessary and wait for the anim state to be updated
	void ProcessControl_AnimStateUpdate();
	// Call process on any related ped data
	void ProcessControl_Data();
	// Process control animation update, contains gestures and facial animation activation
	void ProcessControl_Animation();
	// Graphics update including damage mapping and vfx
	void ProcessControl_Graphics(bool fullUpdate);
	// Cloth update
	void ProcessControl_Cloth();
	// Any function calls relating to ped population
	void ProcessControl_Population();
	// Any functions, variables related to physics that are updated during process control
	void ProcessControl_Physics();
	// Process charge events for special abilities for this frame
	void ProcessControl_SpecialAbilityChargeEvents();
	// Process special abilities for this frame
	void ProcessControl_SpecialAbilities();

	virtual void UpdatePhysicsFromEntity(bool bWarp=false);

	virtual bool IsCollisionLoadedAroundPosition();

	// Glass collision shapetest
	void ProcessGlassCollisionTest();
public:

	struct WoundData
	{
		rage::Vector3 localHitLocation;
		rage::Vector3 localHitNormal;
		int component;
		float impulse;
		int numHits;
		bool valid;  // Is there a wound here?
	};
	
	struct DeathInfo
	{
		void Clear()
		{
			m_Source = NULL;
			m_Weapon = 0;
			m_Time = 0;
		}

		RegdEnt	m_Source;
		u32		m_Weapon;
		u32		m_Time;
	};
	
	void ClearDeathInfo() { m_DeathInfo.Clear(); }
	
	CEntity* GetSourceOfDeath() const { return m_DeathInfo.m_Source; }
	void SetSourceOfDeath(CEntity* pSource) { m_DeathInfo.m_Source = pSource; }
	
	u32 GetCauseOfDeath() const { return m_DeathInfo.m_Weapon; }
	void SetCauseOfDeath(const u32 uWeapon) { m_DeathInfo.m_Weapon = uWeapon; }
	
	u32 GetTimeOfDeath() const { return m_DeathInfo.m_Time; }
	void SetTimeOfDeath(const u32 uTime) { m_DeathInfo.m_Time = uTime; }

	// sleep
	PPUVIRTUAL void OnActivate(phInst* pInst, phInst* pOtherInst);
	PPUVIRTUAL void OnDeactivate(phInst* pInst);
	
	// ground
	void AttachToGround();
	bool CanAttachToGround() const;
	void DetachFromGround();
	bool IsEligibleForGroundAttachment() const;
	bool IsGroundEligibleForAttachment() const;
	void OnAttachToGround();
	void OnDetachFromGround();
	void ProcessGroundMovement(float fTimeStep);
	
	void GetGroundData(CPhysical*& pGroundPhysical, Vec3V& vGroundVelocity, Vec3V& vGroundVelocityIntegrated, Vec3V& vGroundAngularVelocity, Vec3V& vGroundOffset, Vec3V& vGroundNormalLocal, int &iGroundPhysicalComponenet)
	{
		pGroundPhysical = m_pGroundPhysical;
		vGroundVelocity = m_vGroundVelocity;
		vGroundVelocityIntegrated = m_vGroundVelocityIntegrated;
		vGroundAngularVelocity = m_vGroundAngularVelocity;
		vGroundOffset = m_vGroundOffset;
		vGroundNormalLocal = m_vGroundNormalLocal;
		iGroundPhysicalComponenet = m_groundPhysicalComponent;
	}

	void SetGroundData(CPhysical* pGroundPhysical, const Vec3V& vGroundVelocity, const Vec3V& vGroundVelocityIntegrated, const Vec3V& vGroundAngularVelocity, const Vec3V& vGroundOffset, const Vec3V& vGroundNormalLocal, const int &iGroundPhysicalComponenet)
	{
		m_pGroundPhysical = pGroundPhysical;
		m_groundPhysicalComponent = iGroundPhysicalComponenet;
		m_vGroundVelocity = vGroundVelocity;
		m_vGroundVelocityIntegrated = vGroundVelocityIntegrated;
		m_vGroundAngularVelocity = vGroundAngularVelocity;
		m_vGroundOffset = vGroundOffset;
		m_vGroundNormalLocal = vGroundNormalLocal;
	}

private:
	void RecomputeGroundPhysical();
	bool IsGroundBetterThanCurrent(	const CEntity* groundEntity, const phInst* groundInstance, Vec3V_In groundPosition, Vec3V_In groundNormal, int groundComponentIndex,
									const CEntity* currentGroundEntity, Vec3V_In currentGroundPosition, bool currentNormalIsValid);
	void SetGround(CEntity* groundEntity, const phInst* groundInstance, Vec3V_In groundPosition, Vec3V_In groundNormal, int groundComponentIndex, phPolygon::Index groundPrimitiveIndex, phMaterialMgr::Id groundMaterialId);
public:
	// bumps
	void BumpedByEntity(const CEntity& rEntity, float fEntitySpeedSq = -1.0f);

	void ProcessPedStanding(float fTimeStep, phCollider* pCollider);
	void ProcessLowLodPhysics();
	void ProcessOverridePhysics();
	void DecayAccumulatedFire(float fTimeStep);
	bool ProcessFireResistance(float fPenetrationDepth, float fTimeStep);
	bool IsFireResistant() const;
	float GetAccumulatedFire() const { return m_fAccumulatedFire; }

	// update m_fCurrentHeading from m_fDesiredHeading, and m_vecCurrentVelocity from m_extractedVelocity (plus ik slope requests)
	void ApplyMovementRequests(float fTimestep, bool bUpdateVelocity = true, bool bUpdateAngularVelocity = true);			

	// Apply forces from water
	void ProcessBuoyancyProbe(bool bProcess = true);
	void ProcessBuoyancy(float fTimeStep, bool lastTimeSlice);
	virtual void UpdatePossiblyTouchesWaterFlag();
	// Returns fraction which the ped is submerged (from 0.0f to 1.0f) Note: pretty unreliable for ragdolling peds!
	float GetWaterLevelOnPed() const;
	//Set the max time allowed in water
	void ResetWaterTimers();

	// access to phInst's
	void SetAnimatedInst(phInst *pInst);
	phInst* GetAnimatedInst() const {return m_pAnimatedInst;}
	void SetRagdollInst(fragInstNMGta *pInst);
	fragInstNMGta* GetRagdollInst() const {return m_pRagdollInst;}
	virtual fragInst* GetFragInst() const { return reinterpret_cast<fragInst*>(m_pRagdollInst); }
	float GetMassForApplyVehicleForce() const;

	// ragdoll stuff
#if GTA_REPLAY
	void SetReplayUsingRagdoll(bool isUsingRagdoll) { m_replayUsingRagdoll = isUsingRagdoll; }
	bool GetReplayUsingRagdoll() const { return CReplayMgr::IsEditModeActive() && (GetUsingRagdoll() || m_replayUsingRagdoll); }

	void SetReplayEnteringVehicle(bool isEnteringVehicle) { m_replayIsEnteringVehicle = isEnteringVehicle; }
	bool GetReplayEnteringVehicle() const { return CReplayMgr::IsEditModeActive() && m_replayIsEnteringVehicle; }
#endif 

	bool GetUsingRagdoll() const {return (GetCurrentPhysicsInst() && GetCurrentPhysicsInst()==((phInst*)GetRagdollInst()));}
	eRagdollState GetRagdollState() const {return m_nRagdollState;}

	void SetActivateRagdollOnCollisionEvent( CEvent& event )
	{
		ClearActivateRagdollOnCollisionEvent();

		Assertf(event.RequiresAbortForRagdoll(), "Must provide a ragdoll event to SetActivateRagdollOnCollisionEvent!");
		if (event.RequiresAbortForRagdoll())
		{
			m_pActivateRagdollOnCollisionEvent = (CEvent*)event.Clone();
		}
	}

	const CEvent* GetActivateRagdollOnCollisionEvent() const
	{
		return m_pActivateRagdollOnCollisionEvent;
	}

	void ClearActivateRagdollOnCollisionEvent()
	{
		if (m_pActivateRagdollOnCollisionEvent)
		{
			delete m_pActivateRagdollOnCollisionEvent;
		}
		m_pActivateRagdollOnCollisionEvent = NULL;
	}

	void SetActivateRagdollOnCollision(bool bActivateOnCollision, bool bAllowFixed = true)
	{
		bool bValueChanged = (m_bShouldActivateRagdollOnCollision!=bActivateOnCollision);
		m_bShouldActivateRagdollOnCollision = bActivateOnCollision;
		m_bAllowedRagdollFixed = bAllowFixed;
		
		if (bValueChanged)
		{
			if (m_pRagdollInst && m_pRagdollInst->IsInLevel())
			{
				s32 levelIndex = m_pRagdollInst->GetLevelIndex();
				if (bActivateOnCollision)
				{
					PHLEVEL->SetInactiveCollidesAgainstInactive(levelIndex, true);
					PHLEVEL->SetInactiveCollidesAgainstFixed(levelIndex, true);
					PHLEVEL->FindAndAddOverlappingPairs(levelIndex);
				}
				else
				{
					PHLEVEL->SetInactiveCollidesAgainstInactive(levelIndex, false);
					PHLEVEL->SetInactiveCollidesAgainstFixed(levelIndex, false);
				}
			}

			// If disabling then reset values to defaults
			if (!bActivateOnCollision)
			{
				m_fAllowedRagdollPenetration = 0.0f;
				m_fAllowedRagdollSlope = ms_fActivateRagdollOnCollisionDefaultAllowedSlope;
				m_iAllowedRagdollPartsMask = 0xFFFFFFFF;
			}
		}
	}

	bool GetKeepInactiveRagdollContacts() const { return m_bKeepInactiveRagdollContacts; }

	void SetKeepInactiveRagdollContacts(bool enable)
	{
		if(enable != m_bKeepInactiveRagdollContacts)
		{
			m_bKeepInactiveRagdollContacts = enable;
			if(enable)
			{
				// This mode should only be used when the ragdoll isn't already in control
				Assert(GetCurrentPhysicsInst() != GetRagdollInst());
				Assert(!GetIsAttached());

				Assert(!GetActivateRagdollOnCollision());
				SetActivateRagdollOnCollision(true,false);

				Assert(!GetRagdollInst()->GetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL));
				GetRagdollInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,true);
			}
			else
			{
				// Undo everything we had before
				SetActivateRagdollOnCollision(false,false);

				Assert(GetRagdollInst()->GetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL));
				GetRagdollInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,false);
			}
		}
	}

	bool GetActivateRagdollOnCollision() const { return m_bShouldActivateRagdollOnCollision; }
	bool GetAllowedRagdollFixed() const { return m_bAllowedRagdollFixed; }

	void SetRagdollOnCollisionIgnorePhysical( CPhysical* pIgnorePhysical ) { m_pIgnorePhysical = pIgnorePhysical; }
	const CPhysical* GetRagdollOnCollisionIgnorePhysical() const { return m_pIgnorePhysical; }

	float GetRagdollOnCollisionAllowedPenetration( ) const { return m_fAllowedRagdollPenetration; }
	void SetRagdollOnCollisionAllowedPenetration( float allowedPenetration ) { m_fAllowedRagdollPenetration = allowedPenetration; }
	float GetRagdollOnCollisionAllowedSlope () const { return m_fAllowedRagdollSlope; }
	void SetRagdollOnCollisionAllowedSlope( float allowedSlope ) { m_fAllowedRagdollSlope = allowedSlope; }
	s32 GetRagdollOnCollisionAllowedPartsMask( ) const { return m_iAllowedRagdollPartsMask; }
	void SetRagdollOnCollisionAllowedPartsMask( int allowedPartsMask ) { m_iAllowedRagdollPartsMask = allowedPartsMask; }

	virtual void ProcessCollision(
		phInst const * myInst, 
		CEntity* pHitEnt, 
		phInst const* hitInst,
		const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos,
		float fImpulseMag,
		const Vector3& vMyNormal,
		int iMyComponent,
		int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial,
		bool bIsPositiveDepth,
		bool bIsNewContact);

#if __BANK
	static const char * GetRagdollStateName(eRagdollState state)
	{
		switch (state)
		{
			case RAGDOLL_STATE_ANIM_LOCKED :	return  "RAGDOLL_STATE_ANIM_LOCKED";
			case RAGDOLL_STATE_ANIM_PRESETUP :	return	"RAGDOLL_STATE_ANIM_PRESETUP";
			case RAGDOLL_STATE_ANIM :			return	"RAGDOLL_STATE_ANIM";
			case RAGDOLL_STATE_ANIM_DRIVEN :	return	"RAGDOLL_STATE_ANIM_DRIVEN";
			case RAGDOLL_STATE_PHYS_ACTIVATE :	return	"RAGDOLL_STATE_PHYS_ACTIVATE";
			case RAGDOLL_STATE_PHYS :			return	"RAGDOLL_STATE_PHYS";
			default :							return  "UNKNOWN RAGDOLL STATE";
		}
	}
#endif //__BANK

	virtual void UpdatePaused();

	//////////////////////////////////////////////////////////////////////////
	// Assert mechanisms to help detect unmanaged ragdoll activations
	//////////////////////////////////////////////////////////////////////////
#if __ASSERT
	enum eRagdollStackType
	{
		kStackPrepareForActivation = 0,
		kStackSwitchToRagdoll
	};
	s32 GetLastRagdollControllingTaskType() const { return (s32)m_LastRagdollControllingTaskType; }
	void SpewRagdollTaskInfo() const;
	void CollectRagdollStack(eRagdollStackType type);
	bool VerifyRagdollHandled();
private:
	u32 CalcRagdollStackKey(eRagdollStackType type) const { return (u32)(size_t)this + (u32)type; }
public:
#endif //__ASSERT

	void SetRagdollStateInternal(eRagdollState nState);
	void SetRagdollState(eRagdollState nState, bool bUnlocking = false);
	void ProcessRagdollState();

	void SetDesiredRagdollPool(CTaskNMBehaviour::eRagdollPool pool) { m_DesiredRagdollPool = pool; }
	CTaskNMBehaviour::eRagdollPool GetDesiredRagdollPool() const { return m_DesiredRagdollPool; }

	void SetCurrentRagdollPool(CTaskNMBehaviour::eRagdollPool pool) { m_CurrentRagdollPool = pool; }
	CTaskNMBehaviour::eRagdollPool GetCurrentRagdollPool() const { return m_CurrentRagdollPool; }

	// PURPOSE: Must be called every frame by the task currently managing the ragdoll. If this is not called for some minimum period of time whilst the 
	//			ped is in ragdoll (see PED_RAGDOLL_MAXTIME_WITHOUT_CONTROL), the game will assert and then reset the ped to animation.
	// PARAMS:	nCurrentTime - the current time in milliseconds.
	//			controllingTask - The task currently in control of the ragdoll (We track this in __BANK builds to more easily identify the task
	//			responsible for leaving the ped in ragdoll).
	void TickRagdollStateFromTask(CTask& ASSERT_ONLY(controllingTask)) 
	{
		Assert(GetRagdollState()==RAGDOLL_STATE_PHYS);
		m_nRagdollTimer = fwTimer::GetTimeInMilliseconds();
#if __ASSERT
		m_LastRagdollControllingTaskType = ((CTaskTypes::eTaskType)controllingTask.GetTaskType());
#endif //__ASSERT
	}
	
	// PURPOSE: Switch the ped to ragdoll. Only to be used by tasks that are in direct control of the ragdoll
	// PARAMS:  A reference to the task responsible for the ragdoll. This must only be used by tasks that intend to control
	//			the ragdoll themselves (such as natural motion behaviours). If you want to kick off a new natural motion behaviour
	//			that takes over from your task, use the override below.
	void SwitchToRagdoll(CTask& controllingTask);

	// PURPOSE: Switch the ped to ragdoll.
	// PARAMS: event:	You must provide an event that returns RequiresAbortForRagdoll()==true
	//					This is to ensure that an appropriate a.i. task will be created to handle
	//					ragdoll. If your event does not require abort for ragdoll, this function 
	//					will assert and use a default nm task instead.
	CEvent* SwitchToRagdoll(CEvent& event);

	// PURPOSE: A special case method to activate the ragdoll in the physics post sim update
	//			DO NOT call this outside of CPhysics::PostSimUpdate()!
	void SwitchToRagdollPostPhysicsSimUpdate();

	float GetCarInsideAmount( const CEntity*& vehicle ) const;

	void CalculateDynamicAmbientScalesWithVehicleScaler();
	void CalculateDynamicAmbientScalesWithVehicleScaler(float incaredNess);

private:

	CTaskMotionBase* CalculateCurrentMotionTask() const;

	void SwitchToRagdollInternal();

	// apply actual forces to the collider to achieve m_vecCurrentVelocity
	void ApplyDesiredVelocity(float fTimeStep);									//UpdatePosition(bool bHeadingOnly = false);	

private: // Hey, guess what, we're now private private...

	//Can entity we are standing on disable buoyancy processing?
	bool CanGroundPhysicalDisableBuoyancy() const;

	//Check if the entity we are standing on is taking ped under water.
	bool IsGroundPhysicalPullingPedUnderWater() const;
	
	//Returns the inflictor for the damage applied in the ped when it is killed by being handcuffed in a vehicle
	// in deep water.
	CEntity* GetInflictorForDrowningHandCuffedInVehicle();

public:	

	// updates attachment offsets from animated velocities
	void ApplyAnimatedVelocityWhilstAttached(float fTimeStep);

	// PURPOSE: Called when switching to nm in order to seed the balancer and point gun with the
	// correct legs apart distance, hip height and pitch. The balancer will then aim to maintain these 
	// values when balancing. Also used to set the upper body aiming pose for point gun to maintain
	void GetZeroPoseForNM(crSkeleton& skel, bool& bHasWeaponInLeft, bool& bHasWeaponInRight);
	
	float GetAccumulatedSideSwipeImpulseMag() const { return m_fNMAccumulate; }

	void SwitchToStaticFrame(float blendDuration, bool poseFromSkeleton = true);

	void SwitchToAnimated(bool bAddAnimatedPhysics=true, bool bDeactivateRagdollPhysics=true, bool bFixMatrix=true, bool bRemoveArticulatedBodyFromCache=false, bool bSwitchMoveNetwork = true, bool bForceMotionState = true, bool bSwitchToStaticFrame = false);
	void SwitchToAnimDrivenRagdoll();
	void ResetRagdollMatrices();
	void ApplyRagdollBlockingFlags(const eRagdollBlockingFlagsBitSet& flags);
	void ClearRagdollBlockingFlags(const eRagdollBlockingFlagsBitSet& flags);
	const eRagdollBlockingFlagsBitSet GetRagdollBlockingFlags();

	// Capsule Info
	inline const CBaseCapsuleInfo* GetCapsuleInfo() const { return m_pCapsuleInfo; }
	void SetCapsuleInfo(const CBaseCapsuleInfo* pCapsuleInfo) { m_pCapsuleInfo = pCapsuleInfo; }

	const CPedComponentClothInfo* GetComponentClothInfo() const { return m_pComponentClothInfo; }

	// Component Set Info
	const CPedComponentSetInfo* GetComponentSetInfo() const { return m_pComponentSetInfo; }

	// Motion task data set info
	const CMotionTaskDataSet* GetMotionTaskDataSet() const { return m_pMotionTaskDataSet; }

	// Default task data set info
	const CDefaultTaskDataSet* GetDefaultTaskDataSet() const { return m_pDefaultTaskDataSet; }

	//////////////////////////////////////////////////////////////////////////
	// Attachment stuff
	ASSERT_ONLY(static bool ASSERT_IF_DETACHED_FROM_VEHICLE;)
#if __BANK
	#define AttachPedToPhysical(...) DebugAttachPedToPhysical(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachPedToGround(...) DebugAttachPedToGround(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachPedToWorld(...) DebugAttachPedToWorld(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachPedToEnterCar(...) DebugAttachPedToEnterCar(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
	#define AttachPedToMountAnimal(...) DebugAttachPedToMountAnimal(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

	void DebugAttachPedToPhysical(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOffset, float fDefaultHeading, float fHeadingLimit);
	void DebugAttachPedToGround(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, float fDefaultHeading, float fHeadingLimit);
	void DebugAttachPedToWorld(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, u32 nAttachFlags, const Vector3 &vecWorldPos, float fDefaultHeading, float fHeadingLimit);
	void DebugAttachPedToEnterCar(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, u32 nAttachFlags, int nSeatIndex, int nEntryExitPoint, const Vector3* pVecOffset = NULL, const Quaternion* pRotOffset = NULL);
	void DebugAttachPedToMountAnimal(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPed* pMount, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, float fDefaultHeading, float fHeadingLimit);
#else // __BANK
	void AttachPedToPhysical(CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOffset, float fDefaultHeading, float fHeadingLimit);
	void AttachPedToGround(CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, float fDefaultHeading, float fHeadingLimit);
	void AttachPedToWorld(u32 nAttachFlags, const Vector3 &vecWorldPos, float fDefaultHeading, float fHeadingLimit);
	void AttachPedToEnterCar(CPhysical* pPhysical, u32 nAttachFlags, int nSeatIndex, int nEntryExitPoint, const Vector3* pVecOffset = NULL, const Quaternion* pRotOffset = NULL);
	void AttachPedToMountAnimal(CPed* pMount, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, float fDefaultHeading, float fHeadingLimit);
#endif

	virtual void DetachFromParent(u16 nDetachFlags);
	void DetachFromCarInSafePosition(u16 nDetachFlags);
	// vOffset is the mover translation of the anim relative to the translation at the 'origin phase'
	// The car seat attachment stuff usually wants an anim phase offset relative to the 'OpenDoorMatrix', ie relative to the point in the anim where the ped is outside
	// This is different depending on whether this is a get in or get out animation
	bool IsInOrAttachedToCar() const;

	bool CalculateDetachPosition( DetachPosition detachPos, const Matrix34& mEnterCarMat, Matrix34& mDetachPos );
	bool CheckPositionNearEntityIsValid( Matrix34& mat, CEntity* pEntity, const bool bIncludeEntity = true ) const;

	// Ped Damage stuff
	u8	 GetDamageSetID() const				{ return m_damageSetID;}
	void SetDamageSetID(u8 ID);
	void UpdateDamageCustomShaderVars();
	void UpdateDamagePriority(bool bIsVisibleInMainViewport);
	u8	 GetCompressedDamageSetID() const	{ return m_compressedDamageSetID;}
	void SetCompressedDamageSetID(u8 ID)	{ m_compressedDamageSetID = ID;}

	void ReleaseDamageSet();
	void ClearDamage();
	void ClearDamageAndScars();
	void ClearDamage(int zone);
	void HideBloodDamage(int zone, bool enable); // hide (enable==true), unhide (enable==false) blood damage for a specific zone, or all zones if zone == kDamageZoneInvalid
	void LimitBloodDamage(int zone, int limit);  // limit the blood damage drawn specific zone (-1== no limit), or all zones if zone == kDamageZoneInvalid
	void SetBloodDamageClearInfo(float redIntensity, float alphaIntensity); // 0.0f to 1.0f to mess with the blood clear color/alpha for damaged peds (-1.0 resets to the default values)
	void ClearDecorations();
	void PromoteBloodToScars();
	void CloneDamage(const CPed * source, bool bCloneCompressedDamage);

	//////////////////////////////////////////////////////////////////////////
	// Rendering
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PostPreRender();
	virtual void PostPreRenderAfterAttachments();
	void ForceRagdollPreSetup();
	void PreRenderShaders();
	void UpdateRagdollBoundsFromAnimatedSkel(bool updateBounds=true);
	void UpdateRagdollRootTransformFromAnimatedSkel();

	virtual CDynamicEntity*	GetProcessControlOrderParent() const;
	virtual u32 GetStartAnimSceneUpdateFlag() const;

	virtual fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);

	virtual void UpdatePortalTracker();

	bool IsNearMirror(ScalarV_In vThreshold) const;

	bool CanPutOnHelmet() const;
	void GetBikeHelmetOffsets(Vector3& vPosOffset, Quaternion& qRotOffset) const;

	// B*1949041 - Armoured Helmet hit counter
	void IncrementNumArmouredHelmetHits(int iNumHits) { m_iNumArmouredHelmetHits += iNumHits; }
	void ResetNumArmouredHelmetHits() { m_iNumArmouredHelmetHits = 0; }
	int GetNumArmouredHelmetHits() const { return m_iNumArmouredHelmetHits; }

	// Knock this ped off any open vehicle (i.e. also works for jetskis).
	// N.B. - This function assumes the caller has made any relevant checks as to whether this ped
	//        is allowed to be knocked off this vehicle.
	// If "bForce" is set to false, the function will respect flags like KNOCKOFFVEHICLE_NEVER.
	void KnockPedOffVehicle(bool bForce, float fOptionalDamage=0.0f, u32 iMinTime=2000);

	virtual void Update_HD_Models();	// handle requests for high detail models
	virtual inline bool GetIsCurrentlyHD() const { return (m_pedLodState == PLS_HD_AVAILABLE && GetPedModelInfo()->GetAreHDFilesLoaded()); }
	void SetPedLodState(ePedLodState state) { m_pedLodState = state; }

	//////////////////////////////////////////////////////////////////////////
	// Movement
	
	void BlendOutAnyNonMovementAnims(const float fBlendDelta);	// use INSTANT_BLEND_OUT_DELTA for instant
	void RemoveMovementAnims();

	// Returns whether this ped will enter water.  This returns false if the ped drowns in water, and
	// their survival time is less than fMinSurvivalTime.
	bool WillPedEnterWater(const float fMinSurvivalTime=PED_MAX_TIME_IN_WATER) const;

	inline bool IsStrafing(void) const { return GetMotionData()->GetIsStrafing(); }
	bool SetIsStrafing(bool s);

	// Whether the ped is currently moving at all (ie. moving, turning or in the process of stopping)
	bool IsInMotion(void) const;
	// Forces the ped to stop moving/turning, and blends out all associated movement anims
	void StopAllMotion(bool bImmediately = false);

	// Returns whether this ped is crouching, by examining the clip set currently in use
	bool GetIsCrouching(void) const;
	// Sets the ped as crouching, and sets their active clip set appropriately
	void SetIsCrouching(bool bCrouched, s32 iTimeInMs = -1, bool bDoStandUpCheck = true, bool bForceAllow = false);
	bool CanPedCrouch(void) const;
	bool CanPedStealth(void) const;
	bool CanPedStandUp();

	//////////////////////////////////////////////////////////////////////////
	// General AI
	bool OurPedCanSeeThisEntity(const CEntity *pEntity, bool bForTargetingPurposes = false, u32 nIncludeFlags=ArchetypeFlags::GTA_ALL_MAP_TYPES, bool bAdjustForCover=false) const;
	void SortPeds(CPed* apPeds[], s32 nStartIndex, s32 nEndIndex);
	s32 GetLocalDirection(const Vector2& dir);
	bool IsPedInControl();

	bool CanSeeEntity(CEntity *pEntity, float viewAngle = PI/3.0f);
	bool CanSeePosition(const Vector3 & vPos, float viewAngle = PI/3.0f);

	void ForceDeath(bool bRagdollDeath, CEntity* pForceInflictor = NULL, u32 uForceDamageWeapon = 0);

	// Death state, alive, dead or dying
	void SetDeathState(eDeathState deathState);
	eDeathState GetDeathState()	const	{ return m_deathState; }
	bool GetIsDeadOrDying()	const		{ return (m_deathState > DeathState_Alive);}
	u32 GetDeathTime() const			{return m_deathTime;}

	bool IsFatallyInjured() const	{ return GetHealth() < GetDyingHealthThreshold(); }
	bool IsInjured() const			{ return GetHealth() < GetInjuredHealthThreshold(); }
	bool IsFatigued() const			{ return GetHealth() < GetFatiguedHealthThreshold(); }
	bool IsPedBleeding() const;
	bool IsHurtHealthThreshold() const;			// Fairly expensive and only to be used to trigger hurt, use HasHurtStarted() to see if we are in hurtmode or not!
	bool CanBeInHurt() const;
	bool CanDoHurtTransition() const;
	bool ShouldDoHurtTransition() const	{  return (IsBodyPartDamaged(DAMAGED_GOTOWRITHE) || GetHurtEndTime() > 0 || HasHurtStarted()) && CanDoHurtTransition() &&  !GetPedConfigFlag( CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured ) && !GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested); }
	bool HasHurtStarted() const		{ return GetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted); }
	bool IsDead() const				{ return GetIsDeadOrDying() && ShouldBeDead(); }	// Note: would be nice to get rid of ShouldBeDead() here for performance reasons, but apparently it's needed in MP.
	bool ShouldBeDead() const		{ return GetHealth() <= 0.0f; }
	bool IsDeadByMelee() const 		{ return GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) || GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown) || GetPedConfigFlag(CPED_CONFIG_FLAG_Knockedout) || GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStandardMelee); }
	bool ShouldUseInjuredMovement() const;

	// Arrest state, arrested or not
	void SetArrestState(eArrestState arrestState);
	eArrestState GetArrestState() const				{ return m_arrestState; }
	bool GetIsArrested() const						{ return GetArrestState() == ArrestState_Arrested; }

	CPed* GetCustodian() const;
	void SetCustodian(CPed* pPed);
	void SetInCustody(bool bInCustody, CPed *pCustodian);

	CPed *GetArrestTarget() const;
	CPed *GetUncuffTarget() const;

	void SetCustodyFollowDistanceOverride(float fOverride) { m_fCustodyFollowDistanceOverride = fOverride; }
	float GetCustodyFollowDistanceOverride() const { return m_fCustodyFollowDistanceOverride; }

	void SetIsAHighPriorityTarget(bool bHighPriority);

	class CPedGroup* GetPedsGroup() const;	// wrapper around CPedGroups::GetPedsGroup(this)

	bool WillAttackInjuredPeds() const;
	bool IsAllowedToDamageEntity(const CWeaponInfo* pWeaponInfo, const CEntity* pTarget NOTFINAL_ONLY(, u32* uRejectionReason = NULL)) const;

#if ENABLE_DRUNK
	bool IsDrunk() const { return GetPedConfigFlag(CPED_CONFIG_FLAG_IsDrunk) && !GetPedResetFlag(CPED_RESET_FLAG_BlockDrunkBehaviour); }
#endif // ENABLE_DRUNK
	bool IsProne() const;
	bool CanCheckIsProne() const { return (m_pRagdollInst && m_pRagdollInst->GetARTAssetID() >=0); }

	enum PedVehicleFlags
	{
		// In vehicle flags
		PVF_UseExistingNodes					= BIT0,
		PVF_IfForcedTestVehConversionCollision	= BIT1,
		PVF_KeepInVehicleMoveBlender			= BIT2,
		// Out vehicle flags
		PVF_SwitchOffEngine						= BIT3,
		PVF_DontResetDefaultTasks				= BIT4,
		PVF_Warp								= BIT5,
		PVF_IgnoreSafetyPositionCheck			= BIT6,
		PVF_IsBeingJacked						= BIT7,
		PVF_NoCollisionUntilClear				= BIT8,
		PVF_DontNullVelocity					= BIT9,
		PVF_ExcludeVehicleFromSafetyCheck		= BIT10,
		PVF_DontDestroyWeaponObject				= BIT11,
		PVF_DontApplyEnterVehicleForce			= BIT12,
		PVF_DontSnapMatrixUpright				= BIT13,
		PVF_KeepCurrentOffset					= BIT14,
		PVF_IgnoreSettingJustLeftVehicle		= BIT15,
		PVF_IsUpsideDownExit					= BIT16,
		PVF_ClearTaskNetwork					= BIT17,
		PVF_InheritVehicleVelocity				= BIT18,
        PVF_NoNetBlenderTeleport                = BIT19,
		PVF_DontAllowDetach						= BIT20,
		PVF_DontRestartVehicleMotionState		= BIT21
	};
	// Set the ped inside or outside a vehicle
	void SetPedInVehicle(CVehicle* pVehicle, const s32 iTargetSeat, u32 iFlags);
	void SetPedOutOfVehicle(u32 iFlags = 0, s32 nNumFramesJustLeftOverride = -1);

	void SetPedOnMount(CPed* pMount, const s32 iTargetSeat=-1, bool bAnimated=false, bool bSwitchMotionTask = true);
	void SetPedOffMount(bool bClearHorseTasks = false);

	void RestoreHatPropOnVehicleExit();

	CVehicle* GetVehiclePedInside() const { return ( GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) ? m_pMyVehicle.Get() : NULL ); }
	CVehicle* GetVehiclePedEntering() const;
	bool GetIsEnteringVehicle(bool bJustTestForEnterTask = false) const;
	bool GetIsDrivingVehicle() const ;
	bool GetIsInVehicle() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pMyVehicle.Get(); }
	
	bool IsBeingJackedFromVehicle() const;
	bool IsExitingVehicle() const;

#if ENABLE_HORSE
	CPed* GetMountPedOn() const { return ( GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) ? m_pMyMount.Get() : NULL ); }
#else
	CPed* GetMountPedOn() const { return NULL; }
#endif

	bool GetIsOnMount() const { return (GetMountPedOn() != NULL); }

	bool CanShovePed(CPed* pPedToShove) const;
	void SetLastPedShoved(CPed* pPedShoved);
	const CPed* GetLastPedShoved() const { return m_pLastPedShoved.Get(); }
	
	bool GetIsOnFoot() const { return (!GetVehiclePedInside() && !GetMyMount()); }

	void SetCarJacker(CPed* pPed) {m_pCarJacker = pPed;}
	CPed* GetCarJacker() const { return m_pCarJacker;}

	//////////////////////////////////////////////////////////////////////////
	// Flee panic control 

	bool WillCowerInInteriors();

	// TODO: Below could move inside ped intelligence if Neil's per ped flags are in correct place.
	bool CanPutHandsUp( int responseFleeFlags ) const;
	bool CanUseCover( int responseFleeFlags ) const;
	bool WillReturnToPositionPostFlee( int responseFleeFlags ) const;

	bool GetIsPanicking() const;

	//////////////////////////////////////////////////////////////////////////
	// FX
	virtual void ProcessCollisionVfx(VfxCollisionInfo_s& vfxColnInfo);


	void ProcessNMFootCollision(bool isLeft, phMaterialMgr::Id collisionMtlId, float mtlDepth);

	//////////////////////////////////////////////////////////////////////////
	// 

	void CreateDeadPedPickups();	

	//////////////////////////////////////////////////////////////////////////
	// Script
	virtual void SetupMissionState();
	virtual void CleanupMissionState();

	//////////////////////////////////////////////////////////////////////////
	// Audio
	// Returns the voice group, by querying the ModelInfo
	u32 GetPedVoiceGroup(bool bCreateIfUnassigned);
	atHashString GetPedAudioID(u8 slotId);
	atHashString GetSecondPedAudioID(u8 slotId);

	void SetSpeakerListenedTo(CEntity *speaker);
	// Remove both the speaker AND global speaker listened to
	void RemoveSpeakerListenedTo();
	void SetGlobalSpeakerListenedTo(audSpeechAudioEntity *globalSpeaker);
	void SetSpeakerListenedToSecondary(CEntity *speaker);
	void RemoveSpeakerListenedToSecondary();
	const CEntity* GetSpeakerListenedTo() const {return m_SpeakerListenedTo.Get();}
	const CEntity* GetSpeakerListenedToSecondary() const {return m_SpeakerListenedToSecondary.Get();}

	void PlayFootSteps();

	audSpeechAudioEntity *GetSpeechAudioEntity()			 {return m_pSpeechAudioEntity;}
	const audSpeechAudioEntity *GetSpeechAudioEntity() const {return m_pSpeechAudioEntity;}
	audPedAudioEntity *GetPedAudioEntity()					 {return &m_PedAudioEntity;}
	const audPedAudioEntity *GetPedAudioEntity() const		 {return &m_PedAudioEntity;}
	naEnvironmentGroup* GetWeaponEnvironmentGroup(bool shouldCreate = false){return m_PedAudioEntity.GetEnvironmentGroup(shouldCreate);}
	//const naEnvironmentGroup* GetWeaponEnvironmentGroup() const {return m_PedAudioEntity.GetEnvironmentGroup();}
	bool HasAnimalAudioParams() {return GetSpeechAudioEntity() && GetSpeechAudioEntity()->IsAnimal();}
	float GetVisemeDuration() const { return m_fVisemeDuration; }
	void SetContactedFoliageHash(u32 foliageHash)				{ m_ContactedFoliageHash = foliageHash; }
	const u32 GetContactedFoliageHash() const					{ return m_ContactedFoliageHash; }


	audPlaceableTracker *GetPlaceableTracker()				 {return &m_PlaceableTracker;}
	const audPlaceableTracker *GetPlaceableTracker() const		 { return & m_PlaceableTracker;}

	void GetPedRadioCategory(s32 &firstRadioCategory, s32 &secondRadioCategory) const;	// just getting from PedModelInfo

#if	!__FINAL
	void LogSpeech(const char* context);
#endif

	bool NewSay(const char* context, u32 voiceNameHash = 0, bool forcePlay = false, bool allowRepeat = false, s32 delay = -1, 
		CPed* replyingPed = NULL, u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, u32 requestedVolume = 0, 
		bool preloadOnly = false, u32 preloadTimeoutMS = 30000, bool fromScript = false);

	//Like NewSay, but takes the name of a speechParams game object.  This object contains settings for some of the other arguments
	bool NewSayWithParams(const char* context, const char* speechParams = "SPEECH_PARAMS_STANDARD", u32 voiceNameHash = 0, s32 delay = -1, 
						CPed* replyingPed = NULL, u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, bool fromScript = false);

	// Same as the above except it only calls the say command randomly depending on fRandom
	bool RandomlySay(const char* context, float fRandom = 0.05f, bool forcePlay = false, bool allowRepeat = false);

	bool NewSay(u32 contextHash, u32 voiceNameHash = 0, bool forcePlay = false, bool allowRepeat = false, s32 delay = -1, 
		CPed* replyingPed = NULL, u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, u32 requestedVolume = 0, 
		bool preloadOnly = false, u32 preloadTimeoutMS = 30000, bool fromScript = false);

	//Like NewSay, but takes the name of a speechParams game object.  This object contains settings for some of the other arguments
	bool NewSayWithParams(u32 contextHash, const char* speechParams = "SPEECH_PARAMS_STANDARD", u32 voiceNameHash = 0, s32 delay = -1, 
		CPed* replyingPed = NULL, u32 replyingContext = 0, s32 replyingDelay = -1, f32 replyProbability = 1.0f, bool fromScript = false);

	// Same as the above except it only calls the say command randomly depending on fRandom
	bool RandomlySay(u32 contextHash, float fRandom = 0.05f, bool forcePlay = false, bool allowRepeat = false);

	inline void SetIsWaitingForSpeechToPreload(bool isWaiting) {m_WaitingForSpeechToPreload = isWaiting;}
	inline bool GetIsWaitingForSpeechToPreload() const {return m_WaitingForSpeechToPreload;}
	inline void SetLastTimeStartedAutoFireSound(u32 timeStarted) { m_LastTimeStartedAutoFireSound = timeStarted; }
	inline u32 GetLastTimeStartedAutoFireSound() { return m_LastTimeStartedAutoFireSound; }
	inline void SetLastTimeWeBargedThroughDoor(u32 timeStarted) { m_LastTimeWeBargedThroughDoor = timeStarted; }
	inline u32 GetLastTimeWeBargedThroughDoor() const { return m_LastTimeWeBargedThroughDoor; }

	//////////////////////////////////////////////////////////////////////////
	// IK and Animation
	const CIkManager &GetIkManager() const {return m_IkManager;}
	CIkManager &GetIkManager() {return m_IkManager;}

	// Ik the left hand to the gun grip (call from ProcessPreRender2)
	void ProcessLeftHandGripIk(const bool bAllow1HandedAttachToRightGrip, const bool bAllow2HandedAttachToStockGrip = false, float fOverrideBlendInDuration = -1.0f, float fOverrideBlendOutDuration = -1.0f);

	const CPedIKSettingsInfo & GetIKSettings();
	const CTaskDataInfo & GetTaskData() const;
	void SetBehaviorFromTaskData();

	virtual void StartAnimUpdate(float fTimeStep);
	virtual void StartAnimUpdateAfterCamera(float fTimeStep);
	virtual void EndAnimUpdate(float fTimeStep);
	virtual void UpdateVelocityAndAngularVelocity(float fTimeStep);

	void SetPreviousAnimatedVelocity(const Vector3& vPreviousAnimatedVel) { m_vPreviousAnimatedVelocity = vPreviousAnimatedVel; }
	const Vector3& GetPreviousAnimatedVelocity() const { return m_vPreviousAnimatedVelocity; }

#if FPS_MODE_SUPPORTED
	void UpdateFpsCameraRelativeMatrix();
#endif // FPS_MODE_SUPPORTED

	// Should head ik be blocked
	bool BlockConversationLookAt(CPed* pTargetPed);
	// Only for in-car conversation look at.
	static void ProcessInCarConversationLookAtDuringWait(CPed* pPed, bool bDriver, u32 uPauseDuration, s32 uBlendInTime, s32 uBlendOutTime);
	static u32 ComputeConversationLookAtHoldTimeAndFlags(CPed* pPed, bool bDriver, bool bPassenger, u32 uHoldTimeDefault, u32 uHoldTimeMin, s32 uHoldTimeMax, float fMaxSpeed, float fMaxHoldTimeScale, float fFastTurnSpeedThreshold, u32* uOutFlags);

	// kick off the appropriate primary motion task on the motion
	// task tree
	CTaskMotionBase* StartPrimaryMotionTask() const;

	//find the current motion task
	// TODO - enforce that all tasks applied to the motion task tree inherit from CTaskMotionBase
	CTaskMotionBase* GetCurrentMotionTask(bool bStartIfNoTask = true) const;

	// find the primary (or base level ) motion task
	// This should be controlling the base state of the peds movement
	CTaskMotionBase* GetPrimaryMotionTask() const;
	CTaskMotionBase* GetPrimaryMotionTaskIfExists() const;

	void SetCurrentMotionTaskDirty() { m_bCachedMotionTaskDirty = true; }

	// PURPOSE: Forces a known motion state on the ped by rebuilding its motion task tree in a known state
	// PARAMS:	state - the known motion state to force. See CPedMotionStates::eMotionState
	//			restartState - whether or not the state should be restarted if the motion task tree is already in that state
	// RETURNS: True if the state force was successful, false if the motion state is not supported on the ped.
	bool ForceMotionStateThisFrame(CPedMotionStates::eMotionState state, bool restartState = false);

	// PURPOSE: Specific call for network clone peds
	void RequestNetworkClonePedMotionTaskStateChange(s32 state);

	// Used for script calls to stop motion state getting forced when not allowed
	static bool IsAllowedToForceMotionState(const CPedMotionStates::eMotionState state, const CPed* pPed);

	// returns the data used to drive the motion task tree behaviours
	inline CPedMotionData * GetMotionData() { return &m_MotionData; }
	inline const CPedMotionData * GetMotionData() const { return &m_MotionData; }

	CMovePed &GetMovePed() const			{ Assert(GetAnimDirector()); return (CMovePed&) (GetAnimDirector()->GetMove()); }

	void DoPostTaskUpdateAnimChecks();

	void SetUseExtractedZ(const bool bUseExtractedZ);
	inline bool GetUseExtractedZ() const { return m_MotionData.GetUseExtractedZ(); }

	// Returns the current gesture animation set for this ped
	inline fwMvClipSetId GetGestureClipSet(void) const { return m_GestureClipSet; }
	void SetGestureClipSet(const fwMvClipSetId &gestureClipSet) { Assert((gestureClipSet != CLIP_SET_ID_INVALID)); m_GestureClipSet = gestureClipSet;	}

	void ProcessWindyClothing();
	void SetWindyClothingScale(float fWindyClothingScale); 
	float GetWindyClothingScale() const { return m_fWindyClothingScale; }

	void SetScriptCapsuleRadius(float fRadius) {m_fScriptCapsuleRadius = fRadius;}
	void SetTaskCapsuleRadius(float fRadius) {m_fTaskCapsuleRadius = fRadius;}	
	void BlockCapsuleResize(u32 timeInMS) {m_BlockCapsuleResizeTime = fwTimer::GetTimeInMilliseconds()+timeInMS;}
	
	void ProcessSpecialNetworkLeave();

	void ProcessWetClothing();
	void ClearWetClothing();
	void SetWetClothingHeight(float height);
	void SetWetClothingLevel(float level);
	bool IsClothingWet() const					{return (m_uWetClothingFlags&kClothesAreWet)==kClothesAreWet;}
	const Vector4 GetWetClothingData() const	
	{
		float fadeScale = m_hiLodFadeValue/255.0f;
		Vector4 scaledData = m_vecWetClothingData;
		scaledData.z *= fadeScale;
		scaledData.w *= fadeScale;
		return scaledData;
	}

#if GTA_REPLAY
	const Vector4& GetWetClothingDataForReplay() const		{	return m_vecWetClothingData;	}
	u8 GetWetClothingFlagsForReplay() const					{	return m_uWetClothingFlags;		}

	void SetWetClothingDataForReplay(const Vector4& data, u8 flags)	{	m_vecWetClothingData = data; m_uWetClothingFlags = flags;	 }
#endif // GTA_REPLAY

	void IncWetClothing(float rate);
private:
	void ProcessWetClothingInWater();
	void DecWetClothing(float rate);
	inline void SetWetClothingLowerHeight(float val) {m_vecWetClothingData.x = val;}
	inline float GetWetClothingLowerHeight() {return m_vecWetClothingData.x;}
	inline void SetWetClothingUpperHeight(float val) {m_vecWetClothingData.y = val;}
	inline float GetWetClothingUpperHeight() {return m_vecWetClothingData.y;}
	inline void SetWetClothingLowerWetness(float val) {m_vecWetClothingData.z = val;}
	inline float GetWetClothingLowerWetness() {return m_vecWetClothingData.z;}
	inline void SetWetClothingUpperWetness(float val) {m_vecWetClothingData.w = val;}
	inline float GetWetClothingUpperWetness() {return m_vecWetClothingData.w;}
	inline void TargetWetClothingLowerHeight(float target, float rate, float time) {Approach(m_vecWetClothingData.x, target, rate, time);}
	inline void TargetWetClothingUpperHeight(float target, float rate, float time) {Approach(m_vecWetClothingData.y, target, rate, time);}
	inline void TargetWetClothingLowerWetness(float target, float rate, float time) {Approach(m_vecWetClothingData.z, target, rate, time);}
	inline void TargetWetClothingUpperWetness(float target, float rate, float time) {Approach(m_vecWetClothingData.w, target, rate, time);}
public:
	bool GetIsShelteredInVehicle();
	bool GetIsShelteredFromRain();
	void SetStubbleGrowth( float growth );
	float GetCurrentStubbleGrowth() const;

	// delay rendering flags.
	enum PedRenderDelayFlag {
		PRDF_DontRenderUntilNextTaskUpdate = BIT0,
		PRDF_DontRenderUntilNextAnimUpdate = BIT1,
	};

	void SetRenderDelayFlag(u8 uFlag);
	void ClearRenderDelayFlag(u8 uFlag);
	fwFlags8 GetRenderDelayFlags() const { return m_uRenderDelayFlags; }

	// Animations can contain events to trigger a lookats of the primary and secondary lookat peds
	void SetPrimaryLookAt(const CPed* pPrimaryLookAt) { m_pPrimaryLookAt = pPrimaryLookAt; }
	const CPed* GetPrimaryLookAt() const { return m_pPrimaryLookAt.Get(); }
	void SetSecondaryLookAt(const CPed* pSecondaryLookAt) { m_pSecondaryLookAt = pSecondaryLookAt; }
	const CPed* GetSecondaryLookAt() const { return m_pSecondaryLookAt.Get(); }

	enum ActionModeEnabled
	{
		AME_Wanted = 0,
		AME_Script,
		AME_Combat,
		AME_RespondingToOrder,
		AME_Network,
		AME_Investigate,
		AME_Equipment,
		AME_Melee,
		AME_MeleeTransition,
		AME_VehicleEntry,
		AME_CopSeenCrookCNC,
#if __BANK
		AME_Debug,
#endif // __BANK
		AME_Max,
	};

	static const u32 s_nInvalidMovementModeHash = 0xFFFFFFFF;

	// Enable/Disable action mode
	void SetUsingActionMode(const bool b, const ActionModeEnabled reason, const float fTime = -1.f, bool bForcingRun = true, bool bStreamAnimsThenExit = false);
	void SetMovementModeOverrideHash(u32 iHash);
	bool IsMovementModeEnabled() const;
	void EnableMovementMode(bool bEnable);
	
	//! Purpose: Indicates ped is actively running with action mode anims fully streamed in.
	bool IsUsingActionMode() const;
	//! Purpose: Indicates ped is attempting to stream anims, but they may or may not be active on ped.
	bool WantsToUseActionMode(const bool bCheckFPS = true) const;
	//! Check if the weapon has a valid mode
	bool HasValidMovementModeForWeapon(u32 uWeaponHash) const;
	//! Purpose: Indicates an external system has activated action mode for a particular "reason". This doesn't necessarily mean 
	//! that the ped is in action mode (e.g. as they may be in a vehicle).
	bool IsAnyActionModeReasonActive() const;

	bool IsStreamingActionModeClips() const;
	float GetActionModeTimer() const;
	bool IsActionModeReasonActive(const ActionModeEnabled reason) const;
	u32 GetActionModeForcingRunExpiryTime() const { return m_nActionModeForcingRunExpiryTime; }

	u32 GetActionModeReasonTime(const ActionModeEnabled reason) const { return m_nActionModeTimes[reason]; }
	bool GetActionModeIdleActive() const { return m_bActionModeIdleActive; }

#if __BANK
	const char *GetActionModeReasonString(const ActionModeEnabled reason) const;
#endif

	bool IsUsingStealthMode() const;
	bool HasStreamedStealthModeClips() const;

	const CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets& GetMovementModeData() const { return m_MovementModeClipSets; }
	const CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets& GetMovementModeDataForWeaponHash(u32 uWeaponHash) const;
	const fwMvClipSetId GetMovementModeClipSet() const;
	const fwMvClipSetId GetMovementModeIdleTransitionClipSet() const;
	const CPedModelInfo::PersonalityMovementModes* GetPersonalityMovementMode() const { return m_pMovementModes; }
	u32 GetMovementModeOverrideHash() const { return m_iMovementModeOverrideHash; }
	
	bool IsHighEnergyMovementMode() const;
	bool IsBattleEventBlockingActionModeIdle() const;

	bool IsBeingForcedToRun() const;
	void SetForcedRunDelay(u32 uDelayMS);

	void CachePersonalityMovementMode();

	static bool IsActionModeJogTestEnabled();

	virtual bool CanStraddlePortals() const;

	void SetEnableCrewEmblem(bool enable);
	bool GetEnableCrewEmblem() const;

protected:

	void RecomputeCachedBoneOffsets();

	//// Returns a pointer to the first attached object found with the given model name.
	//// Returns null no matching object is found
	//CPhysical * FindAttachedObjectByHash( u32 modelNameHash );

public:

	void ControlSecondaryTaskBlend();

	//////////////////////////////////////////////////////////////////////////
	// Gestures & Visemes
	enum GestureContext
	{
		GC_DEFAULT = 0,
		GC_TWO_HANDED_WEAPON,
		GC_CONVERSATION_PHONE,
		GC_CONVERSATION_HANGOUT,
		GC_OBJECT_LEFT_HAND,
		GC_OBJECT_RIGHT_HAND,
		GC_VEHICLE_DRIVER,
		GC_VEHICLE_PASSENGER,
		GC_WALKING,
		GC_SITTING,
		GC_LOCAL_PLAYER,
		GC_COMBAT_TASK,

		GC_MAX
	};
	static const char *GestureContextNames[];
	static const char *GetGestureContextName(GestureContext gestureContext)
	{
		if(gestureContext >= GC_DEFAULT && gestureContext < GC_MAX)
		{
			return GestureContextNames[gestureContext];
		}
		return "UNKNOWN";
	}
	void BlendInGesture(const crClip* pGestureClip, fwMvFilterId& gestureFilterId, float gestureExpressionWeight, const audGestureData *pGestureData, GestureContext gestureContext);
	void BlendOutGestures(bool bBlockGestures);
	// Should gestures currently be blocked
	bool BlockGestures(BANK_ONLY(bool bTryingToPlayAGesture = true)) const;
	// Determin the current gesture context
	GestureContext GetGestureContext() const;
	// Given a valid gesture animation index and animation hash key, play the gesture animation
	void ProcessGestureHashKey(const audGestureData *gestureData, bool fromAudio = false, bool speaker = true);
	// returns true if a gesture animation was requested this frame (note it may in fact be blocked)
	bool IsPlayingAGestureAnim() const;
	const crClip *GetGestureClip() const { return m_pGestureClip; }
#if	__BANK
	void PrintGestureInformation(const fwMvClipSetId &clipSetId, const audGestureData *gestureData, const char* result, bool fromAudio, bool speaker);
	const char *GetGestureBlockReason() const { return m_GestureBlockReason.c_str(); }
#endif // __BANK
	// Audio assets can be tagged to trigger gestures and/or facial gestures
	void ProcessBodyAndFacialGesturesEmbeddedInAudioAssets();
	// Process any viseme animation embedded in the audio
	void ProcessVisemes();
	// Process a particular aspect of the audio associated animation
	void BlendInVisemeBodyAdditive();
	void BlendOutVisemeBodyAdditive();
	// Process voice driven mouth movement
	void ProcessVoiceDrivenMouthMovement();
	void StartVoiceDrivenMouthMovement();
	void StopVoiceDrivenMouthMovement();
	// Process conversation driven lookats
	void ProcessConversationDrivenLookAts();

	//////////////////////////////////////////////////////////////////////////
	//
	void ProcessInjuredClipSetRequestHelper();

	//////////////////////////////////////////////////////////////////////////
	//
	void	DummyHeightBlendBegin(float dummyHeight);
	float	DummyHeightBlendGetBlendPortion();
	float	DummyHeightBlendGetDummyHeight();

	//////////////////////////////////////////////////////////////////////////
	// Controlled By
	CControlledByInfo GetControlledByInfo() const	{ return CControlledByInfo(m_isControlledByNetwork, m_isControlledByPlayer); }
    void SetControlledByInfo(const CControlledByInfo& controlledBy);
	bool IsControlledByNetwork() const				{ return m_isControlledByNetwork; }
	bool IsControlledByLocalAi() const				{ return (!m_isControlledByNetwork && !m_isControlledByPlayer); }
	bool IsControlledByNetworkAi() const			{ return (m_isControlledByNetwork && !m_isControlledByPlayer); }
	bool IsControlledByLocalOrNetworkAi() const		{ return !m_isControlledByPlayer; }
	bool IsControlledByLocalPlayer() const			{ return (!m_isControlledByNetwork && m_isControlledByPlayer); }
	bool IsControlledByNetworkPlayer()	 const		{ return (m_isControlledByNetwork && m_isControlledByPlayer); }
	bool IsControlledByLocalOrNetworkPlayer() const	{ return m_isControlledByPlayer; }

	bool IsPlayer() const									{ return IsControlledByLocalOrNetworkPlayer(); }
	bool IsLocalPlayer() const								{ return IsControlledByLocalPlayer(); }
	bool IsNetworkPlayer() const							{ return IsControlledByNetworkPlayer(); }
	bool IsAPlayerPed() const								{ return m_pPlayerInfo ? true : false; }
	bool IsNetworkBot() const;

	//////////////////////////////////////////////////////////////////////////
	// Rendering data

	CPedDrawHandler&			GetPedDrawHandler		()		 {return static_cast<CPedDrawHandler&>(GetDrawHandler());} 
	const CPedDrawHandler&		GetPedDrawHandler		() const {return static_cast<const CPedDrawHandler&>(GetDrawHandler());} 

	void						UpdateLodState				();

	// Variation data
	rmcDrawable*				GetComponent(ePedVarComp eComponent);
	void						SetVarRandom(u32 setFlags, u32 clearFlags, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, const CAmbientModelVariations* variations, ePVRaceType race, u32 streamingFlags = 0);
	bool						SetVariation(ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId, s32 streamFlags, bool force);
	bool						IsVariationValid(ePedVarComp slotId, u32 drawblId, u32 texId);
	bool						IsVariationInRange(ePedVarComp slotId, s32 drawblId, s32 texId);
	void						SetVarDefault(u32 streamingFlags = 0);
	void						ApplySelectionSet(u32 index);
	void						ApplySelectionSetByHash(u32 hash);
	bool						HasVariation(ePedVarComp slotId, u32 drawblId, u32 texId);
	bool						IsBoneHelmeted(eAnimBoneTag boneTag);
	bool						IsBoneArmoured(eAnimBoneTag boneTag);
	bool						IsBoneLightlyArmoured(eAnimBoneTag boneTag) const;
	// Head blend data
	void						SetRandomHeadBlendData(bool async);
    void                        RequestBlendFromParents(CPed* parent0, CPed* parent1, float geomBlend, float texBlend);
	void						UpdateChildBlend();
	bool						SetHeadBlendData(const CPedHeadBlendData& data, bool bForce = false);
	bool						SetHeadOverlay(eHeadOverlaySlots slot, u32 tex, float blend, float normBlend);
	bool						UpdateHeadOverlayBlend(eHeadOverlaySlots slot, float blend, float normBlend);
	s32							GetHeadOverlay(eHeadOverlaySlots slot);
	bool						SetHeadOverlayTintIndex(eHeadOverlaySlots slot, eRampType rampType, u32 tint, u32 tint2);
	void						SetNewHeadBlendValues(float headBlend, float texBlend, float varBlend);
	bool						HasHeadBlendFinished();
	void						FinalizeHeadBlend();
	void						CloneHeadBlend(const CPed* cloneFromPed, bool linkBlends);
	bool						HasHeadBlend() const;
    bool                        SetHeadBlendPaletteColor(u8 r, u8 g, u8 b, u8 colorIndex, bool bforce = false);
	void						GetHeadBlendPaletteColor(u8& r, u8& g, u8& b, u8 colorIndex);
	bool						SetHeadBlendRefreshPaletteColor();
    void                        SetHeadBlendPaletteColorDisabled();
    void                        SetHeadBlendEyeColor(s32 colorIndex);
    s32							GetHeadBlendEyeColor();
    void                        MicroMorph(eMicroMorphType type, float blend);
	void						SetHairTintIndex(u32 index, u32 index2);
	bool						GetHairTintIndex(u32& outIndex, u32& outIndex2);

	// preload data
	u32                         SetPreloadData(ePedVarComp slotId, u32 drawableId, u32 textureId, u32 streamingFlags = 0);
	bool						HasPreloadDataFinished();
	bool						HasPreloadDataFinished(u32 handle);
	void						CleanUpPreloadData();
	void						CleanUpPreloadData(u32 handle);

    ePVRaceType                 GetPedRace() const;

	// Removal timers and flags
	void						DelayedRemovalTimeReset		(u32 extraTimeMs = 0);
	bool						DelayedRemovalTimeHasPassed	(float removalRateScale = 1.0f, u32 removalTimeExtraMs = 0) const;
	float						DelayedRemovalTimeGetPortion(float removalRateScale = 1.0f) const;
	void						DelayedConversionTimeReset		(u32 extraTimeMs = 0);
	bool						DelayedConversionTimeHasPassed	(float ConversionRateScale = 1.0f) const;
	float						DelayedConversionTimeGetPortion	(float ConversionRateScale = 1.0f) const;

	bool						GetRemoveAsSoonAsPossible	() const	{ return m_bRemoveAsSoonAsPossible; }
	void						SetRemoveAsSoonAsPossible	(bool val);
	bool						RemovalIsPriority			(fwModelId modelId) const;
	
	void TouchInFovTime(u32 extraTimeMs = 0);
	bool HasInFovTimePassed();

	void						SetIsInOnFootPedCount		(bool val)	{ m_isInOnFootPedCount = val; }
	bool						IsInOnFootPedCount			() const	{ return m_isInOnFootPedCount; }
	void						SetIsInInVehPedCount		(bool val)	{ m_isInInVehPedCount = val; }
	bool						IsInInVehPedCount			() const	{ return m_isInInVehPedCount; }
	virtual void				PopTypeSet					(ePopType val);

	u32							GetCreationTime				() const	{ return m_creationTime; }
	

	u32							GetAssociatedCreationData	() const	{ return m_uiAssociatedCreationData; }
	void						SetAssociatedCreationData	(u32 val)	{ m_uiAssociatedCreationData = val; }
	void						SetRemoveRangeMultiplier	(float val) { m_fRemoveRangeMultiplier = val; }
	float						GetRemoveRangeMultiplier	() const	{ return m_fRemoveRangeMultiplier; }

	bool IsArmyPed() const { return CPedType::IsArmyType(GetPedType()); }
	bool IsGoonPed() const;
	bool IsBossPed() const;
	int	 GetBossID() const;
	bool IsCivilianPed() const;
	bool IsCopPed() const { return CPedType::IsCopType(GetPedType()); }
	bool IsGangPed() const;
	bool IsSecurityPed() const;
	bool IsLawEnforcementPed() const { return CPedType::IsLawEnforcementType(GetPedType()); }
	bool IsMedicPed() const { return CPedType::IsMedicType(GetPedType()); }
	bool IsRegularCop() const { return IsCopPed(); }
	bool IsSwatPed() const { return CPedType::IsSwatType(GetPedType()); }
	bool ShouldBehaveLikeLaw() const;

	//////////////////////////////////////////////////////////////////////////
	// Phone stuff (could be moved to PedWeaponMgr or inventory perhaps)
	const CPedPhoneComponent *	GetPhoneComponent			() const	{ return m_pPhoneComponent; }
	CPedPhoneComponent *		GetPhoneComponent			()			{ return m_pPhoneComponent; }

	//////////////////////////////////////////////////////////////////////////
	// Helmet component
	const CPedHelmetComponent *	GetHelmetComponent			() const	{ return m_pHelmetComponent; }
	CPedHelmetComponent *		GetHelmetComponent			()			{ return m_pHelmetComponent; }


	const CPedFootStepHelper &	GetFootstepHelper			() const	{ return m_PedFootStepHelper; }
	CPedFootStepHelper &		GetFootstepHelper			()			{ return m_PedFootStepHelper; }
	//////////////////////////////////////////////////////////////////////////
	// Bone matrix and position functions
	// Get the global position/global matrix of the specified bone
	// if the bone doesn't exist get the global position/global matrix of the entity 
	// Return true if the bone exists otherwise return false
	bool GetBonePosition(Vector3& posn, eAnimBoneTag boneTag) const;
	bool GetBonePositionVec3V(Vec3V_InOut posn, eAnimBoneTag boneTag) const;
	bool GetBoneMatrix(Matrix34& matResult, eAnimBoneTag boneTag) const;

	// Get a cached bone position, to prevent stalls on the animation thread
	// Note: try to use GetBonePositionCachedV() for better performance, since it should return
	// the result in a vector register. Would be good to eliminate GetBonePositionCached() eventually.
	Vector3 GetBonePositionCached(eAnimBoneTag boneTag) const { return VEC3V_TO_VECTOR3(GetBonePositionCachedV(boneTag)); }
	Vec3V_Out GetBonePositionCachedV(eAnimBoneTag boneTag) const;
	bool GetBoneMatrixCached(Matrix34& matResult, eAnimBoneTag boneTag) const;

	// Get matrix positions from ragdoll components, rather than from skeleton
	void GetBoneMatrixFromRagdollComponent(Matrix34& matResult, int nComponent) const;
	bool GetRagdollComponentMatrix(Matrix34& matResult, int nComponent) const;

	// Get the lock on target position (usually bone pos plus an offset)
	virtual void GetLockOnTargetAimAtPos(Vector3& aimAtPos) const;
	void GetPlayerLockOnTargetVelocityOffset(Vector3& aimAtPos, const Vector3& vPlayerPos) const;

	// Types and query function for estimating the peds pose from the given animation
	EstimatedPose	EstimatePose				( void ) const;								// Estimates a pose from the peds final global bone matrices
	EstimatedPose	EstimatePoseFromSpineMatrix	( const Matrix34* pSpineMatrix ) const;		// Helper function, takes a pre-calculated spine matrix
	spdAABB			EstimateLocalBBoxFromPose	( void ) const;								// Estimates a bounding box from the peds final global bone matrices.

	//////////////////////////////////////////////////////////////////////////

	// Blocks or allows all non-temporary events (Script only!)
	void SetBlockingOfNonTemporaryEvents( bool bTrue ){ SetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents, bTrue ); }
	// Returns whether non-temporary events should be blocked
	bool GetBlockingOfNonTemporaryEvents() const { return (GetPedConfigFlag( CPED_CONFIG_FLAG_BlockNonTemporaryEvents ) || GetBlockingOfNonTemporaryEventsThisFrame()); }
	bool GetBlockingOfNonTemporaryEventsThisFrame() const;

	//////////////////////////////////////////////////////////////////////////
	// Flags access
#if DR_RECORD_PED_FLAGS
	void SetPedConfigFlag(const enum ePedConfigFlags flag, bool value);
	void SetPedResetFlag(const enum ePedResetFlags flag, bool value);
#else
	inline void SetPedConfigFlag(const enum ePedConfigFlags flag, bool value)	{ m_PedConfigFlags.SetFlag( flag, value ); }
	inline void SetPedResetFlag(const enum ePedResetFlags flag, bool value)		{ m_PedResetFlags.SetFlag( flag, value ); }
#endif

	inline void PrefetchPedConfigFlags() { PrefetchObject(&m_PedConfigFlags); }
	inline bool AreConfigFlagsSet(const ePedConfigFlagsBitSet& configFlags)	const { return m_PedConfigFlags.AreConfigFlagsSet(configFlags); }
	inline bool GetPedConfigFlag(const enum ePedConfigFlags flag) const			{ return m_PedConfigFlags.GetFlag( flag ); }
	inline bool GetPedResetFlag(const enum ePedResetFlags flag) const			{ return m_PedResetFlags.GetFlag( flag ); }

	inline void ClearPlayerResetFlag(const enum CPlayerResetFlags::PlayerResetFlags flag)	{ if (Verifyf(GetPlayerInfo(), "Player info was NULL")) GetPlayerInfo()->GetPlayerResetFlags().UnsetFlag(flag); }
	inline void SetPlayerResetFlag(const enum CPlayerResetFlags::PlayerResetFlags flag)	{ if (Verifyf(GetPlayerInfo(), "Player info was NULL")) GetPlayerInfo()->GetPlayerResetFlags().SetFlag(flag); }
	inline bool GetPlayerResetFlag(const enum CPlayerResetFlags::PlayerResetFlags flag) const { return GetPlayerInfo() ? GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(flag) : false; }

	// Flags access (to remove)
	void SetIsStanding(bool bIsStanding) { SetPedConfigFlag( CPED_CONFIG_FLAG_IsStanding, bIsStanding ); }
	bool GetIsStanding() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_IsStanding ); }
	void SetWasStanding(bool bWasStanding) { SetPedConfigFlag( CPED_CONFIG_FLAG_WasStanding, bWasStanding ); }
	bool GetWasStanding() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_WasStanding ); }

	void ResetStandData();

	void SetIsSwimming(bool bIsSwimming) { SetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming, bIsSwimming ); }
	bool GetIsSwimming() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ); }
	void SetWasSwimming(bool bWasSwimming) { SetPedConfigFlag( CPED_CONFIG_FLAG_WasSwimming, bWasSwimming ); }
	bool GetWasSwimming() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_WasSwimming ); }
	bool GetIsFPSSwimming() const;
	bool GetIsFPSSwimmingUnderwater() const;
	bool GetCanUseHighDetailWaterLevel() const;

	void SetIsSkiing(bool bIsSkiing) { SetPedConfigFlag( CPED_CONFIG_FLAG_IsSkiing, bIsSkiing ); }
	bool GetIsSkiing() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_IsSkiing ); }
	
	bool GetIsParachuting() const { return GetPedResetFlag( CPED_RESET_FLAG_IsParachuting ); }

	bool IsUsingJetpack() const { return GetPedResetFlag( CPED_RESET_FLAG_IsUsingJetpack ); }
	bool GetHasJetpackEquipped() const;

#if ENABLE_JETPACK
	void ProcessJetpack();
	bool CreateJetpack(CJetpack *pJetpack);
	void DestroyJetpack();
	void AttachJetpackToPed();
	void DetachJetpackFromPed();
	void PickupJetpackObject(CPickup *pObject);
	void DropJetpackObject();
	CJetpack *GetJetpack() const { return m_pJetpack; }
#endif

	void SetIsMeleeDisabled(bool bIsMeleeDisabled) { SetPedConfigFlag( CPED_CONFIG_FLAG_DisableMelee, bIsMeleeDisabled ); }
	bool GetIsMeleeDisabled() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_DisableMelee ); }

	void SetIsSitting(bool bIsSitting) { SetPedConfigFlag( CPED_CONFIG_FLAG_IsSitting, bIsSitting ); }
	bool GetIsSitting() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_IsSitting ); }

	bool IsHoldingProp() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_IsHoldingProp ); }

	bool GetHasJustLeftVehicle() const { return m_PedResetFlags.GetHasJustLeftVehicle(); }
	void SetHasJustLeftVehicle(u32 iNumFramesToConsiderJustLeft)	{ m_PedResetFlags.SetHasJustLeftVehicle(iNumFramesToConsiderJustLeft); SetPedConfigFlag(CPED_CONFIG_FLAG_JustLeftVehicleNeedsReset, true); }

	bool GetIsInCover() const { return m_PedResetFlags.GetIsInCover(); }
	void SetIsInCover(u32 iNumFramesToConsiderInCover)	{ m_PedResetFlags.SetIsInCover(iNumFramesToConsiderInCover); }

	bool GetHeadIkBlocked() const { return m_PedResetFlags.GetHeadIkBlocked(); }
	void SetHeadIkBlocked()	{ m_PedResetFlags.SetHeadIkBlocked(); }

	bool GetCodeHeadIkBlocked() const { return m_PedResetFlags.GetCodeHeadIkBlocked(); }
	void SetCodeHeadIkBlocked() { m_PedResetFlags.SetCodeHeadIkBlocked(); }

	bool GetGestureAnimsAllowed() const { return GetPedResetFlag( CPED_RESET_FLAG_GestureAnimsAllowed ); }
	void SetGestureAnimsAllowed(bool bGestureAnimsAllowed) { SetPedResetFlag( CPED_RESET_FLAG_GestureAnimsAllowed, bGestureAnimsAllowed ); }

	bool GetGestureAnimsBlockedFromScript() const { return GetPedResetFlag( CPED_RESET_FLAG_GestureAnimsBlockedFromScript ); }
	void SetGestureAnimsBlockedFromScript(bool bGestureAnimsAllowed) { SetPedResetFlag( CPED_RESET_FLAG_GestureAnimsBlockedFromScript, bGestureAnimsAllowed ); }

	bool GetVisemeAnimsBlocked() const { return GetPedResetFlag( CPED_RESET_FLAG_VisemeAnimsBlocked ); }
	void SetVisemeAnimsBlocked(bool bVisemeAnimsBlocked) { SetPedResetFlag( CPED_RESET_FLAG_VisemeAnimsBlocked, bVisemeAnimsBlocked ); }

	bool GetVisemeAnimsAudioBlocked() const { return GetPedResetFlag( CPED_RESET_FLAG_VisemeAnimsAudioBlocked ); }
	void SetVisemeAnimsAudioBlocked(bool bVisemeAnimsAudioBlocked) { SetPedResetFlag( CPED_RESET_FLAG_VisemeAnimsAudioBlocked, bVisemeAnimsAudioBlocked ); }

	bool GetAmbientAnimsBlocked() const { return GetPedResetFlag( CPED_RESET_FLAG_AmbientAnimsBlocked ); }
	void SetAmbientAnimsBlocked(bool bAnimsBlocked) { SetPedResetFlag( CPED_RESET_FLAG_AmbientAnimsBlocked, bAnimsBlocked ); }

	bool GetAmbientIdleAndBaseAnimsBlocked() const { return GetPedResetFlag( CPED_RESET_FLAG_AmbientIdleAndBaseAnimsBlocked ); }
	void SetAmbientIdleAndBaseAnimsBlocked(bool bAnimsBlocked) { SetPedResetFlag( CPED_RESET_FLAG_AmbientIdleAndBaseAnimsBlocked, bAnimsBlocked ); }

	bool GetDisableArmSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableArmSolver ); }
	void SetDisableArmSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableArmSolver, bDisableSolver ); }

	bool GetDisableHeadSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableHeadSolver ); }
	void SetDisableHeadSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableHeadSolver, bDisableSolver ); }

	bool GetDisableLegSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableLegSolver ); }
	void SetDisableLegSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableLegSolver, bDisableSolver ); }

	bool GetDisableTorsoSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableTorsoSolver ); }
	void SetDisableTorsoSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableTorsoSolver, bDisableSolver ); }

	bool GetDisableTorsoReactSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableTorsoReactSolver ); }
	void SetDisableTorsoReactSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableTorsoReactSolver, bDisableSolver ); }

	bool GetDisableTorsoVehicleSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableTorsoVehicleSolver ); }
	void SetDisableTorsoVehicleSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableTorsoVehicleSolver, bDisableSolver ); }

	bool GetDisableRootSlopeFixupSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableRootSlopeFixupSolver ); }
	void SetDisableRootSlopeFixupSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableRootSlopeFixupSolver, bDisableSolver ); }

	bool GetDisableBodyLookSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableBodyLookSolver ); }
	void SetDisableBodyLookSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableBodyLookSolver, bDisableSolver ); }

	bool GetDisableBodyRecoilSolver() const { return GetPedResetFlag( CPED_RESET_FLAG_DisableBodyRecoilSolver ); }
	void SetDisableBodyRecoilSolver(bool bDisableSolver) { SetPedResetFlag( CPED_RESET_FLAG_DisableBodyRecoilSolver, bDisableSolver ); }

	bool GetAutoConversationLookAts() const { return GetPedConfigFlag( CPED_CONFIG_FLAG_AutoConversationLookAts ); }
	void SetAutoConversationLookAts(bool bAutoConversationLookAts) { SetPedConfigFlag( CPED_CONFIG_FLAG_AutoConversationLookAts, bAutoConversationLookAts ); }

	bool GetTargetWhenInjuredAllowed() const { 	return GetPedConfigFlag( CPED_CONFIG_FLAG_TargetWhenInjuredAllowed ); }
	void SetTargetWhenInjuredAllowed(bool bTargetWhenInjuredAllowed) { SetPedConfigFlag( CPED_CONFIG_FLAG_TargetWhenInjuredAllowed, bTargetWhenInjuredAllowed ); }

	bool GetNMTwoHandedWeaponBothHandsConstrained() const {return GetPedConfigFlag( CPED_CONFIG_FLAG_NMTwoHandedWeaponBothHandsConstrained );}
	void SetNMTwoHandedWeaponBothHandsConstrained(bool b) { SetPedConfigFlag( CPED_CONFIG_FLAG_NMTwoHandedWeaponBothHandsConstrained, b ); }

	bool GetIgnoredByAutoOpenDoors() const { return GetPedResetFlag( CPED_RESET_FLAG_IgnoredByAutoOpenDoors ) || GetPedConfigFlag( CPED_CONFIG_FLAG_IgnoredByAutoOpenDoors ); }
	void SetIgnoredByAutoOpenDoors(bool bIgnored) { SetPedResetFlag( CPED_RESET_FLAG_IgnoredByAutoOpenDoors, bIgnored ); }

	bool GetAllowTasksIncompatibleWithMotion() const { return GetPedResetFlag( CPED_RESET_FLAG_AllowTasksIncompatibleWithMotion ); }

	bool CanPlayInCarIdles() const { return  GetPedConfigFlag(CPED_CONFIG_FLAG_CanPlayInCarIdles); }
	void SetCanPlayInCarIdles(bool bOnOff) { SetPedConfigFlag(CPED_CONFIG_FLAG_CanPlayInCarIdles, bOnOff); }

	inline static void SetAllowHurtForAllMissionPeds( bool bState )	{ ms_bAllowHurtForAllMissionPeds = bState; }

	inline static void SetRandomPedsDropMoney( bool bState )		{ ms_bRandomPedsDropMoney = bState; }
	inline static bool GetRandomPedsDropMoney()						{ return ms_bRandomPedsDropMoney; }
	inline static void SetRandomPedsDropWeapons( bool bState )		{ ms_bRandomPedsDropWeapons = bState; }
	inline static bool GetRandomPedsDropWeapons()					{ return ms_bRandomPedsDropWeapons; }

	inline static void SetRandomPedsBlockingNonTempEventsThisFrame( bool bState )		{ ms_bRandomPedsBlockingNonTempEventsThisFrame = bState; }
	inline static bool GetRandomPedsBlockingNonTempEventsThisFrame()					{ return ms_bRandomPedsBlockingNonTempEventsThisFrame; }

	inline CVehicle * GetMyVehicle() const { return m_pMyVehicle; }
	void SetMyVehicle(CVehicle * pVehicle);

	inline bool CanBeMounted() const { return GetSeatManager() != NULL; }

#if ENABLE_HORSE
	inline CPed * GetMyMount() const { return m_pMyMount; }
	inline void SetMyMount(CPed * pMount) { m_pMyMount = pMount; }
	inline CComponentReservationManager* GetComponentReservationMgr() {return m_pComponentReservationMgr;}
	inline CSeatManager* GetSeatManager() { return m_pSeatManager; }
	inline const CSeatManager* GetSeatManager() const {return m_pSeatManager;}
	inline CHorseComponent* GetHorseComponent() const {return m_pHorseComponent;}
#else
	inline CPed * GetMyMount() const { return NULL; }
	inline void SetMyMount(CPed * UNUSED_PARAM(pMount)) { }
	inline CComponentReservationManager* GetComponentReservationMgr() {return NULL;}
	inline CSeatManager* GetSeatManager() { return NULL; }
	inline const CSeatManager* GetSeatManager() const {return NULL;}
	//inline CHorseComponent* GetHorseComponent() const {return NULL;}
#endif

	// Add ped in the given seat
	bool AddPedInSeat(CPed *pPed, s32 iSeat);
	bool WantsToMoveQuickly();

	bool IsInFirstPersonDriverCamera() const;
	bool IsInFirstPersonVehicleCamera(bool bCheckIsDominant = false, bool bIncludeClones = false) const;
	void UpdateFirstPersonFlags(bool isFps);

	// Vehicle entry script config api
	void CreateVehicleEntryConfigIfRequired();
	const CSyncedPedVehicleEntryScriptConfig* GetVehicleEntryConfig() const { return m_pVehicleEntryScriptConfig; }
	void CopyVehicleEntryConfigData(const CSyncedPedVehicleEntryScriptConfig& rOther);
	void SetForcedSeatUsage(s32 iSlot, s32 iFlags, CVehicle* pVehicle, s32 iSeatIndex);
	void SetForcedSeatUsage(s32 iSlot, s32 iFlags, ObjectId vehicleId, s32 iSeatIndex);
	void SetForcedSeatUsageCommon(s32 iSlot, s32 iFlags, s32 iSeatIndex);
	void DeleteVehicleEntryConfig();
	void ClearForcedSeatUsage();

	bool AddWoundData(const WoundData &wound, CEntity *pShotBy = NULL, bool forcePrimary = false);
	WoundData * GetPrimaryWoundData() { return &m_WoundPrimary; } 
	WoundData * GetSecondaryWoundData() { return &m_WoundSecondary; } 
	CEntity * GetAttacker();

	inline s32 GetMoneyCarried() const { return m_MoneyCarried; }
	inline void SetMoneyCarried(const s32 i) { m_MoneyCarried = i; }

	u8 GetStickyCount() const { return m_uStickyCount; }
	void IncrementStickyCount() { m_uStickyCount++; }
	void DecrementStickyCount();

	u8 GetStickToPedProjectileCount() const { return m_uStickToPedProjCount; }
	void IncrementStickToPedProjectileCount() { m_uStickToPedProjCount++; }
	void DecrementStickToPedProjectileCount();

	u8 GetFlareGunProjectileCount() const { return m_uFlareGunProjCount; }
	void IncrementFlareGunProjectileCount() { m_uFlareGunProjCount++; }
	void DecrementFlareGunProjectileCount();

	void SetTimeOfFirstBeingUnderAnotherRagdoll(u32 set) { m_TimeOfFirstBeingUnderAnotherRagdoll = set; }
	u32 GetTimeOfFirstBeingUnderAnotherRagdoll() const { return m_TimeOfFirstBeingUnderAnotherRagdoll; }

	float GetSlideSpeed() const { return m_SlideSpeed; }
	
	inline Vec3V_ConstRef GetGroundVelocity() const { return m_vGroundVelocity; }
	inline Vec3V_ConstRef GetGroundVelocityIntegrated() const { return m_vGroundVelocityIntegrated; }
	inline Vec3V_ConstRef GetGroundAngularVelocity() const { return m_vGroundAngularVelocity; }
	inline Vec3V_ConstRef GetGroundAcceleration() const { return m_vGroundAcceleration; }

	void CacheGroundVelocityForJump();
	inline void ResetGroundVelocityForJump() { m_vGroundVelocityForJump.Zero(); }
	inline const Vector3 & GetGroundVelocityForJump() const { return m_vGroundVelocityForJump; }

	inline Vector3 GetGroundPos() const 
	{
		if(GetGroundPhysical())
		{
			return VEC3V_TO_VECTOR3(GetGroundPhysical()->GetTransform().Transform(m_vecGroundPosLocal));
		}

		return VEC3V_TO_VECTOR3(m_vecGroundPos);
	}
	inline void SetGroundPos(const Vector3 & vPos) { m_vecGroundPos = VECTOR3_TO_VEC3V(vPos); }

	inline bool GetIsStandingOnMovableObject() const { return m_bIsStandingOnMovableObject; }
	inline void SetIsStandingOnMovableObject(bool isStandingOnMovingObject) { m_bIsStandingOnMovableObject = isStandingOnMovingObject; }
	inline CPhysical * GetLastValidGroundPhysical() const { return m_pLastValidGroundPhysical; }
	inline u32 GetLastValidGroundPhysicalTime() const { return m_uLastValidGroundPhysicalTime; }
	inline CPhysical * GetGroundPhysical() const { return m_pGroundPhysical; }
	inline void SetGroundPhysical(CPhysical* pGroundPhysical)
	{ 
		Assertf(pGroundPhysical != NULL || m_pGroundPhysical == NULL || !GetIsAttachedToGround(), "Attempting to clear this ped's ground physical without first detaching it.");
		m_pGroundPhysical = pGroundPhysical;
	}
	inline s32 GetGroundPhysicalComponent() const { return m_groundPhysicalComponent; }
	inline void SetGroundPhysicalComponent(s32 component) { m_groundPhysicalComponent = component; }
	inline const Vector3 & GetGroundNormal() const { return m_vecGroundNormal; }
	inline const Vec3V & GetMaxGroundNormal() const { return m_vecMaxGroundNormal; }
	inline const Vec3V & GetMinOverhangNormal() const { return m_vecMinOverhangNormal; }
	inline void SetGroundNormal(const Vector3 & vNormal) { m_vecGroundNormal = vNormal; }
#if FPS_MODE_SUPPORTED
	inline const float GetLowCoverHeightOffsetFromMover() const { return m_fLowCoverHeightOffsetFromMover; }
	void SetLowCoverHeightOffsetFromMover(float fLowCoverHeightOffsetFromMover) { m_fLowCoverHeightOffsetFromMover = fLowCoverHeightOffsetFromMover; }
	float ComputeLowCoverHeightOffsetFromMover(float fInitialZOffset, float fFinalZOffset, float fIncrementZOffset, float fProbeLength);
#endif
	inline const Vector3 & GetLocalOffsetToCoverPoint() const { return m_vLocalOffsetToCoverPoint; }
	inline void SetLocalOffsetToCoverPoint(const Vector3 & vOffset) { m_vLocalOffsetToCoverPoint = vOffset; }
	inline const Vector3 & GetDesiredVelocity() const { return m_vDesiredVelocity; }	
	inline const Vector3 & GetDesiredAngularVelocity() const { return m_vDesiredAngularVelocity; }
	inline bool GetIsValidGroundNormal() const { return m_bValidGroundNormal; }
	bool IsOnGround() const;
	void SetRidingOnTrain(bool bRiding, CPhysical* pTrain);
	inline CPhysical * GetTrainRidingOn() const { return m_pTrainRidingOn; }

	inline const bool GetAoBlobRendering() const { return m_bRenderAoBlobs; }
	inline void SetAoBlobRendering(bool value) { m_bRenderAoBlobs = value; }

	inline void SetSlopeNormal(const Vector3& v) { Assert(v==v); m_vSlopeNormal = v; }
	inline const Vector3& GetSlopeNormal() const { return m_vSlopeNormal; }

	inline const Vec3V& GetRearGroundPos() const { return m_vecRearGroundPos; }
	inline const Vec3V& GetRearGroundNormal() const { return m_vecRearGroundNormal; }

	void SetStairFlags(const WorldProbe::CShapeTestHitPoint* pIntersection, const phInst* pInst, const s32 iInstComponent, phMaterialMgr::Id materialId);

	CPhysical * GetClimbPhysical(bool bAcceptVaultingState = false) const;

	float GetTimeCapsulePushedByVehicle() const { return m_TimeCapsulePushedByVehicle;} 
	u32 GetTimeCapsuleFirstPushedByPlayerCapsule() const { return m_TimeCapsuleFirstPushedByPlayerCapsule;} 


	// Use to modify the desired velocity the physics capsule wants to travel at
	// Remember the accel limit is generally time dependent
	// Only works if called within ProcessPhysics
	void SetDesiredVelocityClamped(const Vector3& vVel, float fAccelLimit);	
	inline float GetGroundOffsetForPhysics() const { return m_fGroundOffsetForPhysics; }
	inline void SetGroundOffsetForPhysics(const float f) { m_fGroundOffsetForPhysics = f; }
	inline float GetGroundZFromImpact() const { return m_fGroundZFromImpact; }
	inline const Vector3 & GetSteepSlopePos() const { return m_vecSteepSlopePos; }
	inline void SetSteepSlopePos(const Vector3 & vPos) { m_vecSteepSlopePos = vPos; }
	inline float GetFallingHeight() const { return m_fFallingHeight; }
	inline void SetFallingHeight(const float fNewFallingHeight) { m_fFallingHeight = fNewFallingHeight; }

	inline bool IsUsingLowLodPhysics() const { return GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics); }
	void SetUsingLowLodPhysics(bool b);

#if __TRACK_PEDS_IN_NAVMESH
	inline CNavMeshTrackedObject & GetNavMeshTracker() { return m_NavMeshTracker; }
	inline const CNavMeshTrackedObject & GetNavMeshTracker() const { return m_NavMeshTracker; }
#endif
	
	void SetUsingLowLodMotionTask(bool b);
	bool HasLowLodMotionTask() const;

	// Note: Ground material is only valid if GetIsStanding() returns true
	inline phMaterialMgr::Id GetUnpackedGroundMaterialId() const { return PGTAMATERIALMGR->UnpackMtlId(m_PackedGroundMaterialId); }	// Includes per poly flag. Unpack using gta material mgr
	inline phMaterialMgr::Id GetPackedGroundMaterialId() const { return m_PackedGroundMaterialId; }
	inline void SetGroundMaterialId(phMaterialMgr::Id id) { m_PackedGroundMaterialId = id; }

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	//Get distance from contact point on interpolated surface triangle to the undeformed top triangle.
	inline float GetSecondSurfaceDepth() const {return m_secondSurfaceDepth;}
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	inline const CVfxDeepSurfaceInfo& GetDeepSurfaceInfo() const {return m_deepSurfaceInfo;}
	inline CVfxDeepSurfaceInfo& GetDeepSurfaceInfo() {return m_deepSurfaceInfo;}

	inline float GetAttachHeadingLimit() const { return m_fAttachHeadingLimit; }
	inline void SetAttachHeadingLimit(const float f) { m_fAttachHeadingLimit = f; }
	inline float GetAttachHeading() const { return m_fAttachHeading; }
	inline void SetAttachHeading(const float f) { m_fAttachHeading = f; }

	inline void SetPedGroupIndex(const u32 i) { Assert(i<=255); m_iPedGroupIndex = static_cast<u8>(i); }
	inline u32 GetPedGroupIndex() const { return m_iPedGroupIndex; }

	inline float GetMaxTimeInWater() const { return (m_OverrideMaxTimeInWater != -1) ?  m_OverrideMaxTimeInWater : m_fMaxTimeInWater; }
	inline void SetMaxTimeInWater(const float f, bool bDefault = true) { bDefault ? m_fMaxTimeInWater = f : m_OverrideMaxTimeInWater = f; }
	float GetMaxTimeUnderwater() const;
	void SetMaxTimeUnderwater(const float f);
	
	inline u16 GetInteriorFreezeWasInsideCounter() const { return m_nInteriorFreeze_WasInsideCounter; }
	inline void SetInteriorFreezeWasInsideCounter(const u16 i) { m_nInteriorFreeze_WasInsideCounter = i; }

	inline void SetStreamedScriptBrainToLoad(const s16 i) { m_StreamedScriptBrainToLoad = i; }
	inline s16 GetStreamedScriptBrainToLoad() const { return m_StreamedScriptBrainToLoad; }

	inline int GetAttachCarEntryExitPoint() const { return m_nAttachCarEntryExitPoint; }
	inline void SetAttachCarEntryExitPoint(const int i) { m_nAttachCarEntryExitPoint = (s16)i; }

	inline int GetAttachCarSeatIndex() const { return m_nAttachCarSeatIndex; }
	inline void SetAttachCarSeatIndex(const s32 i) { m_nAttachCarSeatIndex = (s16)i; }

	inline s32 GetLastSignificantShotBoneTag() const { return m_lastSignificantShotBoneTag; }
	inline void SetLastSignificantShotBoneTag(const s32 i) { m_lastSignificantShotBoneTag = i; }

	inline u8 GetDangerTimer() const { return m_InDangerTimer; }
	inline void SetDangerTimer(const u8 i) { m_InDangerTimer = i; }

	inline u32 GetWeaponBlockDelay() const { return m_WeaponBlockingDelay; }
	inline void SetWeaponBlockDelay(const u32 i) { m_WeaponBlockingDelay = i; }

	inline void SetCorpseRagdollFriction(float friction) { m_RagdollCorpseFriction = friction; }

	const CPlayerSpecialAbility* GetSpecialAbility(ePlayerSpecialAbilitySlot abilitySlot = PSAS_PRIMARY) const;
	CPlayerSpecialAbility* GetSpecialAbility(ePlayerSpecialAbilitySlot abilitySlot = PSAS_PRIMARY);
	SpecialAbilityType GetSpecialAbilityType(ePlayerSpecialAbilitySlot abilitySlot = PSAS_PRIMARY) const;
	void InitSpecialAbility(ePlayerSpecialAbilitySlot abilitySlot = PSAS_PRIMARY);
	// returns true if there is a valid special ability in any slot
	const bool HasSpecialAbility() const;

	const ePlayerSpecialAbilitySlot GetSelectedAbilitySlot() const { return m_selectedAbilitySlot; }
	bool SetSelectedAbilitySlot(ePlayerSpecialAbilitySlot newSlot);

	// Get the next valid ability slot, wrapping back to the start 
	ePlayerSpecialAbilitySlot GetDesiredAbilitySlot();

	// check to see if the current ability is disabled and if we should switch to the other available one	
	static bool IsValidSpecialAbilitySlot(ePlayerSpecialAbilitySlot abilitySlot);
	
public:

	const atHashWithStringNotFinal GetBrawlingStyle( void ) const { return m_BrawlingStyle; }
	void SetBrawlingStyle( const char* pszBrawlingStyle ) { return m_BrawlingStyle.SetFromString( pszBrawlingStyle ); }

	const u32 GetDefaultUnarmedWeaponHash( void ) const { return m_DefaultUnarmedWeapon; }

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Related to animated ped bound update disabling
	inline void SetRagdollBoundsUpToDate(bool set) { m_bRagdollBoundsUpToDate = set; }
	inline bool AreRagdollBoundsUpToDate() const { return m_bRagdollBoundsUpToDate; }
	inline void RequestRagdollBoundsUpdate(u16 numFramesToUpdate = 10) { m_bRagdollBoundsUpdateRequestFrames = numFramesToUpdate; }
	inline bool IsRagdollBoundsUpdateRequested() const { return m_bRagdollBoundsUpdateRequestFrames > 0; }
	inline u16 GetRagdollBoundsUpdateRequestedFrames() const { return m_bRagdollBoundsUpdateRequestFrames; }
#endif

	//////////////////////////////////////////////////////////////////////////
	// peds occlusion queries
	inline bool GetUseVehicleBoundingBox() const { return m_bUseVehicleBoundingBox; }
	inline void SetUseVehicleBoundingBox(const bool b) { m_bUseVehicleBoundingBox = b; }

	inline bool GetUseRestrictedVehicleBoundingBox() const { return m_bUseRestrictedVehicleBoundingBox; }
	inline void SetUseRestrictedVehicleBoundingBox(const bool b) { m_bUseRestrictedVehicleBoundingBox = b; }

	inline void EnableFootLight(const bool b)				{ m_bEnableFootLight = b; }
	inline bool GetFootLightEnabled() const					{ return m_bEnableFootLight; }

	inline bool GetUseHnSBoundingBox() const { return m_bUseHnSBoundingBox; }
	inline void SetUseHnSBoundingBox(const bool b) { m_bUseHnSBoundingBox = b; }

	CEntity *GetOQBoundingBox(Vec3V_InOut min, Vec3V_InOut max);

	bool IsPedVisible();
	int GetPedPixelCount();
	
	bool HaveAllStreamingReqsCompleted();

	u32 GetRagdollTimer() const { return m_nRagdollTimer; };

	void AppendAssetStreamingIndices(atArray<strIndex>& deps, const strIndex* ignoreList, s32 numIgnores);
	
	void SetIsInPopulationCache(bool val) { m_bIsInPopulationCache = val; }
	bool GetIsInPopulationCache() const { return m_bIsInPopulationCache; }

	void SetLodMultiplier(float multiplier) { m_fLodMult = multiplier; }
	float GetLodMultiplier() const { return m_fLodMult + GetLodMultiplierBias() ; }

	void SetPrevConversationLookAtTime(u32 time) {m_uPrevConversationLookAtTime = time;}
	u32 GetPrevConversationLookAtTime() const {return m_uPrevConversationLookAtTime;}
	void SetPrevConversationLookAtDuration(u32 duration) {m_uPrevConversationLookAtDuration = duration;}
	u32 GetPrevConversationLookAtDuration() const {return m_uPrevConversationLookAtDuration;}
	void SetConversationLookAtWaitTime(u32 time) {m_uConversationLookAtWaitTime = time;}
	u32 GetConversationLookAtWaitTime() const {return m_uConversationLookAtWaitTime;}
	void SetWasSpeaking(bool val) { m_bWasSpeaking = val;}
	bool GetWasSpeaking() const { return m_bWasSpeaking;}
	void SetLookingRandomPosition(bool val) {m_bLookingRandomPosition = val;}
	bool GetLookingRandomPosition() const {return m_bLookingRandomPosition;}
	void SetLookingCarMovingDirection(bool val) {m_bLookingCarMovingDirection = val;}
	bool GetLookingCarMovingDirection() const {return m_bLookingCarMovingDirection;}

	//////////////////////////////////////////////////////////////////////////
	// needs to go
	void KillPedWithCar(CVehicle *pVehicle, float fImpulse, const CCollisionRecord* pColRecord);
	s32 CountNearbyHostilePedsTryingToEnterMyVehicle(float fDistance);

	// Shock events
	CPedShockingEvent * GetShockingEvent()				{ return m_pShockingEvent; }
	const CPedShockingEvent * GetShockingEvent() const	{ return m_pShockingEvent; }

	static u32 GetLoadingObjectIndex( const CDynamicEntity& dynamicEntity );
	static fwMvClipSetId GetDefaultClipSet( const CDynamicEntity& dynamicEntity );

	// Action mode
	void UpdateMovementMode(const bool bCheckFps = true);
	void ResetMovementMode();
	CPedModelInfo::PersonalityMovementModes::MovementModes GetMovementModeType(const bool bCheckFps = true) const;
	void ProcessActionModeLookAt();
	u32 GetMovementModeWeaponHash() const;

	Color32 GetDirtColor() const { return Color32(m_dirtColRedf, m_dirtColGreenf, m_dirtColBluef, m_dirtColAlphaf); }
	float GetDirtScale() const { return m_dirtColScale; }
	void SetDirtScale(float v) { m_dirtColScale = v; }
	void SetCustomDirtScale(float v) { m_dirtColScaleCustom = v; }
	float GetCustomDirtScale() { return m_dirtColScaleCustom; }
	void ClearCustomDirt();

	float GetSweatScale() { return m_sweatScale; }
	void SetSweatScale(float sweatiness) { m_sweatScale = sweatiness; }

	void SetOverrideWeaponAnimations(atHashString OverrideWeaponAnimations) { m_OverrideWeaponAnimations = OverrideWeaponAnimations; }
	atHashString GetOverrideWeaponAnimations() const { return m_OverrideWeaponAnimations; }

	void SetExitVehicleOnChangeOwner(bool bValue) { m_bExitVehicleOnChangeOwner = bValue; }
	bool GetExitVehicleOnChangeOwner() const { return m_bExitVehicleOnChangeOwner; }

	void InitAudio();

#if __ASSERT
	bool IsPedAllowedInMultiplayer(void) const;
#endif /* __ASSERT */

	bool GetLodBucketInfoExists(u32 lod) const { Assert(lod < LOD_COUNT); return m_renderBucketInfoPerLODs[lod].m_bLodBucketInfoExists; }
	bool GetLodHasAlpha(u32 lod) const { Assert(lod < LOD_COUNT); return m_renderBucketInfoPerLODs[lod].m_bLodHasAlpha; }
	bool GetLodHasDecal(u32 lod) const { Assert(lod < LOD_COUNT); return m_renderBucketInfoPerLODs[lod].m_bLodHasDecal; }
	bool GetLodHasCutout(u32 lod) const { Assert(lod < LOD_COUNT); return m_renderBucketInfoPerLODs[lod].m_bLodHasCutout; }

	void SetLodBucketInfoExists(u32 lod, bool bLodBucketInfoExists) { Assert(lod < LOD_COUNT); m_renderBucketInfoPerLODs[lod].m_bLodBucketInfoExists = bLodBucketInfoExists; }
	void SetLodHasAlpha(u32 lod, bool bLodHasAlpha) { Assert(lod < LOD_COUNT); m_renderBucketInfoPerLODs[lod].m_bLodHasAlpha = bLodHasAlpha; }
	void SetLodHasDecal(u32 lod, bool bLodHasDecal) { Assert(lod < LOD_COUNT); m_renderBucketInfoPerLODs[lod].m_bLodHasDecal = bLodHasDecal; }
	void SetLodHasCutout(u32 lod, bool bLodHasCutout) { Assert(lod < LOD_COUNT); m_renderBucketInfoPerLODs[lod].m_bLodHasCutout = bLodHasCutout; }
	
#if FPS_MODE_SUPPORTED
	float ComputeExternallyDrivenPelvisOffsetForFPSStealth() const;
#endif // FPS_MODE_SUPPORTED

	//////////////////////////////////////////////////////////////////////////
	// Static ped functions
public:
	static void InitSystem();
	static void ShutdownSystem();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void UpdateVisualDataSettings();

	static void EnableBikeHelmetsForAllPeds(bool bEnable) { ms_bBikeHelmetsDisabledForAllPeds = bEnable; }

	static void ComputeMatrixFromHeadingAndPitch(Mat34V_Ref mtrxOut, const float& heading, const float& pitch, Vec3V_In posV);

	// Set the weapon hash we are ignoring on material collision
	static void SetIgnoredMaterialCollisionWeaponHash(u32 uHash) { ms_uIgnoredMaterialCollisionWeaponHash = uHash; }

	// Misc static accessors for Dynamic entities (all check that the dynamic entity is a ped).  Candidate for cleanup
	static s32						GetScenarioType( const CDynamicEntity& dynamicEntity, const bool bMustBeRunning = false );
	static CScenarioPoint const*	GetScenarioPoint( const CDynamicEntity& dynamicEntity, const bool bMustBeRunning = false);
	static u32	GetWeaponHash( const CDynamicEntity& dynamicEntity );
	static const CObject* GetHeldObject( const CDynamicEntity& dynamicEntity );

	static bool IsSuperLodFromIndex(u32 index);
	static bool IsSuperLodFromHash(u32 hash);

	static bool IsStairs(const WorldProbe::CShapeTestHitPoint* pIntersection, const phInst* pInst, const s32 iInstComponent, phMaterialMgr::Id materialId);
	static bool IsStairSlope(const WorldProbe::CShapeTestHitPoint* pIntersection, const phInst* pInst, const s32 iInstComponent);

	static void InitTunables();

#if __BANK
	static void InitBank();
	static void CreateBank();
	static void ShutdownBank();

	// Pointer to the main RAG bank
	static bkBank*		ms_pBank;
	static bkButton*	ms_pCreateButton;

	static void InitDebugData();

	static bool IsAnyPedInInterior(CInteriorInst* pIntInst);

	static void DrawDebugVisibilityQuery();
#endif//__BANK
#if __DEV 
	static void PoolFullCallback(void* pItem);
#endif // __DEV

	// Fixed waiting for collision update
	virtual bool ShouldFixIfNoCollisionLoadedAroundPosition();
	virtual void OnFixIfNoCollisionLoadedAroundPosition(bool bFix);

	// Ped lodding control class
	const CPedAILod& GetPedAiLod() const { return m_pedAiLod; }
	CPedAILod& GetPedAiLod() { return m_pedAiLod; }

	// PURPOSE:	Copy the flags from fwSceneUpdateExtension to m_CachedSceneUpdateFlags.
	void UpdateCachedSceneUpdateFlags();
#if __ASSERT
	// PURPOSE:	Make sure the relevant flags in m_CachedSceneUpdateFlags match the ones in fwSceneUpdateExtension.
	void ValidateCachedSceneUpdateFlags();
#endif

	static void StartAiUpdate();
	void UpdateInSpatialArray();
	void UpdateSpatialArrayTypeFlags();
	u32 FindDesiredSpatialArrayTypeFlags() const;

	// PURPOSE:	Type flags for the ped spatial array.
	enum
	{
		kSpatialArrayTypeFlagInVehicle = BIT0,			// Currently in a vehicle.
		kSpatialArrayTypeFlagUsingScenario = BIT1,		// Currently using a scenario.
		kSpatialArrayTypeFlagVisibleOnScreen = BIT2,	// Currently visible on screen, as determined by CPedAILodManager.
		kSpatialArrayTypeFlagHorse = BIT3,				// A horse
	};

	static CPed* GetPedFromSpatialArrayNode(CSpatialArrayNode* node)
	{
		return reinterpret_cast<CPed*>( reinterpret_cast<char*>(node) - OffsetOf(CPed, m_spatialArrayNode) );
	}

	static const CPed* GetPedFromSpatialArrayNode(const CSpatialArrayNode* node)
	{
		return reinterpret_cast<const CPed*>( reinterpret_cast<const char*>(node) - OffsetOf(CPed, m_spatialArrayNode) );
	}

	CSpatialArrayNode& GetSpatialArrayNode() { return m_spatialArrayNode; }
	const CSpatialArrayNode& GetSpatialArrayNode() const { return m_spatialArrayNode; }
	static CSpatialArray& GetSpatialArray() { return *ms_spatialArray; }

	crFrame* GetIndependentMoverFrame();

	void ForcePedToHaveZeroMassInCollisionsThisFrame() { m_forcePedHasNoMassInImpacts = true; }

	void SetDisableHighFallInstantDeath( bool disableFallDeath ) { m_disableHighFallInstantDeath = disableFallDeath; }
	bool GetHighFallInstantDeathDisabled() { return m_disableHighFallInstantDeath; }

#if !__FINAL
	struct sDamageRecord
	{
		sDamageRecord() {}
		sDamageRecord(const CEntity* pInflictor, u32 uTimeInflicted, u32 uWeaponHash, f32 fHealthLost, f32 fArmourLost, f32 fEnduranceLost, f32 fHealth, s32 iComponent, bool bInstantKill, bool bInvincible, u32 uRejectionReason = 0)
			: pInflictor(pInflictor)
			, uTimeInflicted(uTimeInflicted)
			, uWeaponHash(uWeaponHash)
			, fHealthLost(fHealthLost)
			, fArmourLost(fArmourLost)
			, fEnduranceLost(fEnduranceLost)
			, fHealth(fHealth)
			, iComponent(iComponent)
			, fDistance(0.f)
			, bInstantKill(bInstantKill)
			, bInvincible(bInvincible)
			, uRejectionReason(uRejectionReason)
		{
		}

		RegdConstEnt pInflictor;
		u32 uRejectionReason;
		u32 uTimeInflicted;
		u32 uWeaponHash;
		f32 fHealthLost;
		f32 fArmourLost;
		f32 fEnduranceLost;
		f32 fHealth;
		s32 iComponent;
		f32 fDistance;
		bool bInstantKill : 1;
		bool bInvincible : 1;
	};
	void AddDamageRecord(const sDamageRecord& dr);
#endif // !__FINAL

#if GTA_REPLAY
	void SetPedClanIdForReplay(rlClanId clanId) { m_ClanIdForReplay = clanId; }
	rlClanId GetPedClanIdForReplay() const { return m_ClanIdForReplay;}
	ReplayRelevantAITaskInfo& GetReplayRelevantAITaskInfo() { return m_replayRelevantAITaskInfo; }
#endif //GTA_REPLAY

	//////////////////////////////////////////////////////////////////////////
	// Member variables

protected:
	Vector4	m_vecWetClothingData;		// x&y are the low/high water marks, z&w are the low/high wet values 	

	// movement vectors
	Vector3 m_vDesiredVelocity;
	Vector3 m_vDesiredAngularVelocity;

	Vector3 m_vPreviousAnimatedVelocity;

	Vector3 m_vGroundVelocityForJump;

	Vec3V	m_vecGroundPos;
	Vec3V	m_vecGroundPosLocal;
	Vector3 m_vecGroundPosHistory[PED_GROUND_POS_HISTORY_SIZE];
	Vector3 m_vecSteepSlopePos;
	Vector3 m_vecGroundNormal;
	Vec3V	m_vecMaxGroundNormal;
	Vec3V	m_vecInterpMaxGroundNormal;
	Vec3V	m_vecMinOverhangNormal;
	Vec3V	m_vecRearGroundPos;
	Vec3V	m_vecRearGroundNormal; //may be different from ground normal for quadrupeds

	Vector3 m_vSlopeNormal; //calculated externally 

	// A local offset used to move around and away from coverpoints.
	// While still maintaining a desired relative position should the whole cover point (or entity it is attached to) move
	Vector3	m_vLocalOffsetToCoverPoint;

	enum posOffsets{ kOffsetHead, kOffsetPHLeftFoot, kOffsetPHRightFoot, kOffsetLeftFoot, kOffsetRightFoot, kOffsetLeftHand, kOffsetRightHand, kOffsetNeck, kOffsetSpine1,kOffsetCount };
	Vec3V	m_vecOffsets[kOffsetCount];

	enum rotOffsets{ kRotOffsetHead, REPLAY_ONLY(kRotOffsetPHLeftFoot,kRotOffsetPHRightFoot,) kRotOffsetCount };
	QuatV m_rotOffsets[kRotOffsetCount];

	Vec3V m_vGroundVelocity;
	Vec3V m_vGroundVelocityIntegrated;
	Vec3V m_vGroundAngularVelocity;
	Vec3V m_vGroundAcceleration;
	Vec3V m_vGroundOffset;
	Vec3V m_vGroundNormalLocal;

	CPedMotionData m_MotionData;

	CPedFootStepHelper m_PedFootStepHelper;

	audPedAudioEntity m_PedAudioEntity;	
	u32 m_ContactedFoliageHash; // only valid when CPED_RESET_FLAG_InContactWithFoliage or CPED_RESET_FLAG_InContactWithBIGFoliage set

	WoundData m_WoundPrimary;
	WoundData m_WoundSecondary;

	CVfxDeepSurfaceInfo m_deepSurfaceInfo;

	bool m_bStartOutOfWaterTimer;
	float m_fTimeOutOfWater;

private:
	Vector3 m_vCurrentBoundOffset;

	CIkManager m_IkManager;

protected:

	// -----------------------------------------------------------------------
	//
	// WARNING - START
	// THE FOLLOW CODE IS OPTIMIZED!  DON'T FUCK WITH IT!

	// put this stuff at the top as it's the most useful data for debugging AI
	bool m_isControlledByNetwork : 1;
	bool m_isControlledByPlayer : 1;

	// ped counts and population data (moved from CPedBase)
	bool m_isInOnFootPedCount : 1;
	bool m_isInInVehPedCount : 1;
	bool m_bRemoveAsSoonAsPossible : 1;

	bool m_dummyHeightBlendInProgress : 1;
	bool m_WaitingForSpeechToPreload : 1;
	bool m_bKeepInactiveRagdollContacts : 1;
	bool m_bShouldActivateRagdollOnCollision : 1;

// 8

	bool m_bAllowedRagdollFixed : 1;
	bool m_bDeathTimeHasSet : 1;
	bool m_bMoverCapsuleIsChanging : 1;

	bool m_bIsInPopulationCache : 1;
	bool m_bUseVehicleBoundingBox : 1;
	bool m_bUseHnSBoundingBox : 1;

	// Movement mode applied
	bool m_bMovementModeApplied : 1;

	bool m_MovementModeStreamAnimsThenExit : 1;

// 16

	// Animated velocity tracking
	bool m_bResetPreviousAnimatedVelocity : 1;

	// Does the cached current motion task pointer need to be updated
	mutable bool m_bCachedMotionTaskDirty : 1;

	// used to flag a ground collision point as valid.
	bool m_bValidGroundNormal : 1;
	bool m_bIsStandingOnMovableObject : 1;

	// Handle being kicked by the player or being hit by a door
	bool m_bPreviouslyKickedByPlayer : 1;
	bool m_bKickedByPlayer : 1;
	bool m_bPreviouslyHitByDoor : 1;
	bool m_bHitByDoor : 1;

// 24

	bool m_bRenderAoBlobs : 1;
	bool m_NeedToInitAudio : 1;
	bool m_bResizePlayerLegBound : 1;

private:
	bool m_bPedCapsuleFullStop : 1;
	bool m_bNewGesture : 1;
	bool m_bVoiceGroupSelected : 1;
	bool m_bPickupInRange : 1;	// if the ped touches a pickup
	bool m_bMainBoundIntersectsPedThatCausesActivations : 1;

// 32
	
	ASSERT_ONLY(bool m_PreRenderLock : 1;)

// 33/32

protected:
	ePedLodState m_pedLodState : 4;
	eRagdollState m_nRagdollState : 4;	

// 41/40

	// Arrest state, set by the tasks, records when the ped is arrested
	eArrestState	m_arrestState : 3;
	// Death state, set by the tasks, records when the ped is dead or dying
	eDeathState		m_deathState : 3;

// 47/46
	
	ePedType m_nPedType : 7;

// 54/53
	
	CTaskNMBehaviour::eRagdollPool m_DesiredRagdollPool : 4;
	CTaskNMBehaviour::eRagdollPool m_CurrentRagdollPool : 4;

// 62/61
	
	PedConstraintType m_ConstraintType : 3;

// 65/64 .. nice .. off by one...

	// info for the new ped damage system	
	u8 m_damageSetID;	//  TODO: find a good place to put this for alignment
	u8 m_compressedDamageSetID;
	u8 m_heatScaleOverride;
	u8 m_ProcessBoundsCountdown;	// Counts down to avoid having to update on every frame.	

	// 
	// WARNING - END
	//
	// -----------------------------------------------------------------------

	CPedIntelligence*	m_pPedIntelligence;
	CPlayerInfo*		m_pPlayerInfo;
	CPedInventory*		m_inventory;
	CPedWeaponManager*	m_weaponManager;
	CFacialDataComponent* m_facialData;
	CRagdollConstraintComponent* m_ragdollConstraintData;
	CPedPhoneComponent*	m_pPhoneComponent;
	CPedHelmetComponent* m_pHelmetComponent;
	audSpeechAudioEntity * m_pSpeechAudioEntity;
	audSpeechAudioEntity* m_SpeakerSpeechAudioEntity;	

	// special physics collider for peds
	phInst* m_pAnimatedInst;

	audPlaceableTracker m_PlaceableTracker;	
	u32 m_nRagdollTimer;	
	CPedAILod			m_pedAiLod;
	phConstraintHandle m_OverhangConstraint;

	// Record when the ped gets killed. In milliseconds
	u32 m_deathTime;	

	//	Used by the scripts to determine which body part was last damaged
	int				m_weaponDamageComponent;

	// Depletion count
	s32				m_nAmmoDiminishingCount;

	RegdEnt m_Attacker;

	float m_fAllowedRagdollPenetration;
	float m_fAllowedRagdollSlope;
	int m_iAllowedRagdollPartsMask;
	
	RegdEvent m_pActivateRagdollOnCollisionEvent;
	RegdPhy	m_pIgnorePhysical; 

	// How long ago the last bullet was fired.
	float m_fTimeSinceLastShotFired;
	float	m_fRemoveRangeMultiplier;

	float	m_dirtColRedf;
	float	m_dirtColGreenf;
	float	m_dirtColBluef;
	float	m_dirtColAlphaf;
	float	m_dirtColScale;
	float   m_dirtColScaleCustom;

	float   m_sweatScale;

	float	m_dummyHeightBlendHeight;
	u32		m_dummyHeightBlendStartTimeMs;	

	u32		m_delayedRemovalResetTimeMs;
	u32		m_delayedRemovalAmountMs;
	u32		m_delayedConversionResetTimeMs;
	u32		m_delayedConversionAmountMs;
	u32		m_uiAssociatedCreationData;
	u32		m_InFovTime;	
	u32		m_creationTime;

	// ClanId set in the ped so we still have the information when the player leaves the session and is converted to an ambiant ped
	rlClanId		m_SavedClanId;

private:
	pgRef<const crClip> m_pGestureClip;
	fwMvFilterId m_GestureFilterId;
	const audGestureData* m_pGestureData;
	audWaveSlot* m_WaveslotHeldForGestures;

	fwMvClipSetId m_GestureClipSet;
	GestureContext m_GestureContext;

#if __BANK
	mutable atString m_GestureBlockReason;
	bool m_bCachedGestureAnimsAllowed;
#endif

	pgRef<const crClip> m_pVisemeBodyAdditiveClip;
	float m_fVisemeBodyAdditiveWeight;

	fwClipSetRequestHelper m_injuredRequestHelper;
	CGetupSetRequestHelper m_injuredGetupRequestHelper;

	const CPedIKSettingsInfo* m_pIKSettings;
	mutable const CTaskDataInfo* m_pTaskData;

	Matrix34 m_mCurrentWeaponLeftGripBoneOffset;
	float m_fElapsedBlendOutTime;

	// Allow script to specify seating preferences on a few vehicles
	CSyncedPedVehicleEntryScriptConfig* m_pVehicleEntryScriptConfig;
	
	// Used to decide how much to jitter the global matrices to mimic windy clothing (could be in CIkManager?)
	float m_fWindyClothingScale;

	//used to override the ped's capsule radius
	float	m_fScriptCapsuleRadius; 	
	float	m_fTaskCapsuleRadius; 	
	u32		m_BlockCapsuleResizeTime;	

	// Each ped has an instance of their archetypes so we are free to change its bounds, etc.
	phArchetype* m_pPhysicsArchetype;
	const CBaseCapsuleInfo* m_pCapsuleInfo;			// This is the capsule info that stores radius, knee height, head height, etc. // NOT OWNED
	const CPedComponentClothInfo* m_pComponentClothInfo;	// Component cloth info that stores which components we should initialize/use for this ped. // NOT OWNED
	const CPedComponentSetInfo* m_pComponentSetInfo;	// Component set info that stores which components we should initialize/use for this ped. // NOT OWNED
	const CMotionTaskDataSet* m_pMotionTaskDataSet;	// points to the motion task data set our ped uses (i.e. what on foot type they use)
	const CDefaultTaskDataSet* m_pDefaultTaskDataSet;	// points to the default task data set our ped uses (i.e. what default task they use)

	CCoverPoint* m_pCoverPoint; // Pointer to the cover node this ped is trying to hide behind.
	CCoverPoint* m_pDesiredCoverPoint; // This is the cover point we would like to use (can be the same as m_pCoverPoint).

#if FPS_MODE_SUPPORTED
	float m_fLowCoverHeightOffsetFromMover;
#endif

	float m_fTargetableDistance;	// The los targeting checks for lock on pass within this distance
	float m_fTargetThreatOverride;	// Override threat value for targeting purposes.
	RegdPed m_pLastPedShoved;		//to stop the ped constantly playing the shove anim
	u32 m_nLastPedShoveTime;

	RegdEnt m_SpeakerListenedTo;
	RegdEnt m_SpeakerListenedToSecondary;
	audSpeechAudioEntity* m_GlobalSpeakerListenedTo;
	float m_fPrevVisemeTime;
	float m_fVisemeDuration;
	u32 m_uPrevConversationLookAtTime;
	u32 m_uPrevConversationLookAtDuration;
	u32 m_uConversationLookAtWaitTime;
	s16 m_specialNetworkLeaveTimeRemaining;
	s16 m_iAlphaOverride;
	RegdConstPed m_pPrimaryLookAt;
	RegdConstPed m_pSecondaryLookAt;

	// Once a voice group has been selected, it is stored here.	
	u32 m_selectedVoiceGroup;

	u32 m_LastTimeStartedAutoFireSound;
	u32 m_LastTimeWeBargedThroughDoor;

	u32 m_nDamageDelayTimer;		// stops damage anim playing too frequently (e.g. so the player doesn't get stuck)
	float m_fTimeSincePedInWater;	// Counts up since the last time a ped was in water, to a max of 60 seconds

	u32 m_nExhaustionDamageTimer;   // Counts the time since falling off a bike due to exhaustion. Less ragdoll damage during a short period.

	u32 m_nRiverRapidsTimer;		// Counts the time time since entering a section of river rapids. Clamped at the max lerp duration time.

	float m_fCurrentBoundPitch;		// Tracks what the ped's bound is currently angled at
	float m_fCurrentBoundHeading;	// The variable in the move blend data (m_fBoundPitch/m_fBoundHeading) is what the move blender wants

	CPedShockingEvent*			m_pShockingEvent;	// optional component

	//Ptr to rage rotational constraint that constrains the rotation of the ped around all 3 axes 
	//in the rage physics update.
	//The rotational constraint needs updated when the ped's matrix is updated from its heading direction.

	// Need a constraint for each axis in order to specify different min / max
	phConstraintHandle m_RotationalConstraintX;		// Stops ped pitching
	phConstraintHandle m_RotationalConstraintY;		// Stops ped rolling
	phConstraintHandle m_CylindricalConstraint; // Stops ped roll and pitch	

	CPlayerSpecialAbility*		m_specialAbilities[PSAS_MAX];
	ePlayerSpecialAbilitySlot	m_selectedAbilitySlot;	

	atHashWithStringNotFinal	m_BrawlingStyle;
	atHashWithStringNotFinal	m_DefaultUnarmedWeapon;

	WorldProbe::CShapeTestResults	m_RagdollGroundProbeResults;	// Used for asynch probing of the ground while ragdolling
	
	// Used to override the weapon animation in pedinfomodel
	atHashString m_OverrideWeaponAnimations;

	// Carjacked by this ped.
	RegdPed			m_pCarJacker;
	
	bool m_bKeepingCurrentHelmetInVehicle;
	bool m_bHidingHelmetInVehicle;

	struct RenderBucketInfo
	{
		bool m_bLodBucketInfoExists : 1;
		bool m_bLodHasAlpha : 1;
		bool m_bLodHasDecal : 1;
		bool m_bLodHasCutout : 1;
	};

	RenderBucketInfo m_renderBucketInfoPerLODs[LOD_COUNT];

#if GTA_REPLAY
	rlClanId m_ClanIdForReplay;
	bool m_replayUsingRagdoll : 1;
	bool m_replayIsEnteringVehicle :1;
	ReplayRelevantAITaskInfo m_replayRelevantAITaskInfo;
#endif //GTA_REPLAY

public:
	CPedClothCollision*	m_ClothCollision;
	characterClothController* m_CClothController[PV_MAX_COMP];

	fragInstNMGta* m_pRagdollInst; // Yuk, Made it public so we can get the address of the member for some SPU code 

	// PURPOSE:	A copy of the scene update flags in fwSceneUpdateExtension, cached for performance reasons.
	u32				m_CachedSceneUpdateFlags;

	CPedConfigFlags m_PedConfigFlags;
	CPedResetFlags	m_PedResetFlags;

#if DEBUG_DRAW
	bool m_bDrownDisabledByScript :  1;
#endif

#if __ASSERT
	bool m_CanUseRagdollSuccessfull;
	mutable u32 m_uLastRagdollInfoTime;
#endif //__ASSERT

#if __BANK
	float m_processControlTime;
	Vec3V m_vecInitialSpawnPosition;
#endif

	float GetCurrentHairScale() const { return m_fCurrentHairScale; }
	void SetTargetHairScale(float fTargetHairScale) { m_fTargetHairScale = fTargetHairScale; }
	float GetTargetHairScale() const { return m_fTargetHairScale; }
	void SetHairScaleLerp(bool bHairScaleLerp) { m_bHairScaleLerp = bHairScaleLerp; }
	bool GetHairScaleLerp() const { return m_bHairScaleLerp; }
	float GetHairHeight() const { return m_fHairHeight; }

	void EnableMpLight(bool e)			{ m_bMpLightEnabled = e; }
	bool GetMpLightEnabled() const		{ return m_bMpLightEnabled; }

	int GetPedPhonePaletteIdx() const { return m_iPhonePaletteIdx; }
	void SetPedPhonePaletteIdx(int iPhonePalIdx) { m_iPhonePaletteIdx = (u8)(iPhonePalIdx&0xff); }

#if __BANK
	bool GetIsVisemePlaying() const { return m_bVisemePlaying; }
#endif //__BANK
protected:


	float m_fLodMult;

	// External DOF
	crFrame* m_pExternallyDrivenDOFFrame;
	float m_fHighHeelExpressionBlend;
	float m_fCurrentHairScale;
	float m_fTargetHairScale;
	float m_fHairHeight;
	
	u8 m_PoseIndex;
	u8 m_PackageIndex;
	bool m_bAlphaIncreasing;
	bool m_specialNetworkLeaveActive : 1;
	bool m_specialNetworkLeaveWhenDead : 1;
	bool m_bHairScaleLerp : 1;
	bool m_bMpLightEnabled : 1;

private:
	bool m_bWasSpeaking : 1;
	bool m_bLookingRandomPosition : 1;
	bool m_bLookingCarMovingDirection : 1;	
	bool m_bVisemePlaying : 1;

protected:
	u8		m_PushedByObjectInCoverFrameCount;	

	float m_nHealthLostScript;

	float m_fMaxEndurance;
	float m_fEndurance;

#if RSG_PC
	sysLinkedData <float, ReportArmorMismatch> m_nArmour;
#else // RSG_PC
	float m_nArmour;
#endif // RSG_PC

	float m_nArmourLost;	// Could probably be u16
	float m_fFatiguedHealthThreshold;
	float m_fInjuredHealthThreshold;
	float m_fDyingHealthThreshold;
	float m_fHurtHealthThreshold;
	float m_fNMAccumulate;
	u32 m_cachedBoneSignature;

	// used for seek, pursue, arrive
	RegdVeh m_pMyVehicle;
#if ENABLE_HORSE
	RegdPed m_pMyMount;
#endif

#if ENABLE_JETPACK
	RegdJetpack m_pJetpack;
#endif

	// B*1949041 - Counter for number of hits ped's armoured helmet has taken
	int m_iNumArmouredHelmetHits;

	// cached bone offsets to prevent a stall on the animation thread
	int m_cachedBoneIndices[kOffsetCount];		
	
	float m_fCustodyFollowDistanceOverride;

	// If a player ped is colliding with another ped using kinematic physics mode, the other ped will be recorded here.
	RegdPed m_pKinematicPed;

	// This is the ped that has taken this ped into custody (so you follow them about).
	RegdPed m_pCustodianPed;

	// for recording ground collisions
	RegdPhy m_pGroundPhysical;
	RegdPhy m_pLastValidGroundPhysical;
	RegdPhy m_pTrainRidingOn;
	
	u32 m_uLastValidGroundPhysicalTime;
	u32 m_uTimeGroundPhysicalWasSet;
	s32 m_groundPhysicalComponent;
	s32 m_lastGroundPhysicalComponent;

	float m_fHeightFudgeRecoveryFactor[2];	//Quadrupeds

	float	m_CurrentSpringForceZ;	// When calling ProcessPedStanding() only for the first physics step, this is the vertical force to apply during the other physics steps.
	float m_fSpringForceFront;
	float m_fSpringForceBack;
	float m_fGroundZFromImpact;
	float m_fRearGroundZFromImpact;
	float m_fGroundOffsetForPhysics;
	float m_fFallingHeight;

	bool m_lastValidResult : 1;
	bool m_bActionModeIdleActive : 1;
	bool m_bExitVehicleOnChangeOwner : 1;
	bool m_bPortalTrackerUsingRagdollorRagdolFrame : 1;
	bool m_bUseRestrictedVehicleBoundingBox : 1;
	bool m_bEnableFootLight : 1;

	//////////////////////////////////////////////////////////////
	//
	// There are 3.375 bytes free here!!!!
	//
	//////////////////////////////////////////////////////////////
	
	phMaterialMgr::Id m_PackedGroundMaterialId;
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	float m_secondSurfaceDepth;
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	// attach to entity stuff, used to fix peds relative to entities (usually in script)
	float m_fAttachHeading;			// default direction, relative to entity we're attached to
	float m_fAttachHeadingLimit;	// limit player rotation from default direction
	s16 m_nAttachCarSeatIndex;
	s16 m_nAttachCarEntryExitPoint;

	CSpatialArrayNode m_spatialArrayNode;
	friend class CSpatialArray;

	u16		m_DamagedBodyParts;
	u8 m_uWetClothingFlags;		// flags are kClothesAreWet, kClothesAreInWater
	u8 m_uSuppressKinematicModeTimer;

	bool m_bEnableCrewEmblem;

	// Action modes that are currently forcing run.
	fwFlags16 m_uActionModeForcingRun;

	// PURPOSE:	Counter used for determining when we have to reorthonormalize in SimulatePhysicsForLowLod().
	u8		m_SimPhysForLowLodMtrxMulCtr;

	// VFX data --------------------
	CPedVfx*	m_pPedVfx;
	// -----------------------------

#if ENABLE_HORSE
	//Ridable ped info
	CSeatManager* m_pSeatManager;
	CComponentReservationManager* m_pComponentReservationMgr;
	CHorseComponent* m_pHorseComponent; //ridable animal specific control information
#endif

	fwFlags8 m_uRenderDelayFlags;
	u32 m_uRenderFlagFrameCount;

	// wet cloth effect info 
	enum WetClothesFlags {
		kClothesAreWet		= BIT0,
		kClothesAreInWater	= BIT1,
		kClothesGotWetterThisFrame	= BIT2,
	};

	int		m_ShaveTimeInSeconds;		// time that last had a shave
	u32		m_HurtEndTime;			// Could even be s8 or s16 I think
	
	DeathInfo m_DeathInfo;

	//store the amount of fire a ped has accumulated
	//fire-resistant peds take a little while to catch on fire
	float m_fAccumulatedFire;

	u32 m_MoneyCarried;		// The amount of money this ped carries around

	// stops peds getting frozen immediately after coming out of an interior
	u16 m_nInteriorFreeze_WasInsideCounter;

	//	the following variable is used by peds created by ped generators so that they aren't deleted as soon as they are created
	s16 m_StreamedScriptBrainToLoad;

	// Times before ped starts taking damage in water. Longer for player
	float m_fMaxTimeInWater;
	float m_fMaxTimeUnderwater;

	// Used 
	float m_OverrideMaxTimeInWater;

	s32 m_MinOnGroundTimeForStunGun;	// The minimum time the ped should spend on the ground when hit with a stun gun (in milliseconds)
										// -1 to use the weapon's default value.
	
	s32 m_lastSignificantShotBoneTag;	

	u8 m_iPedGroupIndex;		// Which group this ped is in.  This is set from CPedGroups::Process() at the start of each frame.
	u8 m_InDangerTimer;
	u8 m_OverhangConstraintCountdown;	
	u8 m_hiLodFadeValue;

	// Used to grow/shrink ped bound radius
	float m_fCurrentMainMoverCapsuleRadius;
	float m_fDesiredMainMoverCapsuleRadius;
	float m_fCurrentCapsultHeightOffset;
	float m_fOverrideCapsuleRadiusGrowSpeed;

	float m_SlideSpeed; 
	float m_RagdollCorpseFriction;

#if __TRACK_PEDS_IN_NAVMESH
	// This object is used to track the ped's movement within the navmesh.
	CNavMeshTrackedObject m_NavMeshTracker;
#endif

#if USE_PHYSICAL_HISTORY
	CPhysicalHistory m_physicalHistory;
#endif

	CPedPropRequestGfx* m_propRequestGfx;

	u32 m_TimeOfFirstBeingUnderAnotherRagdoll;
	u32 m_uRagdollPlayerLegContactTime;
	u32 m_uRagdollDoorContactTime;

	mutable taskRegdRef<CTaskMotionBase> m_pCachedMotionTask;

	// Current movement modes group.
	const CPedModelInfo::PersonalityMovementModes* m_pMovementModes;
	// Current selected weapon for movement mode.
	u32 m_uMovementModeWeaponHash;
	// Movement mode type we are currently using
	CPedModelInfo::PersonalityMovementModes::MovementModes m_iMovementModeType;
	// Movement mode we are currently using
	s32 m_iMovementModeIndex;
	// Movement mode hash. Allows us to override movement mode clips (or enable for a ped that doesn't normally use movement modes)
	u32 m_iMovementModeOverrideHash;

	// Selected Idle transition clip set ID.
	fwMvClipSetId m_IdleTransitionClipSetID;
	// Movement mode clip set
	CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets m_MovementModeClipSets;
	// Helpers for streaming in the clip sets
	fwClipSetRequestHelper m_MovementModeMovementClipRequestHelper;
	fwClipSetRequestHelper m_MovementModeWeaponClipRequestHelper;
	fwClipSetRequestHelper m_MovementModeIdleTransitionsClipRequestHelper;
	fwClipSetRequestHelper m_MovementModeUnholsterClipRequestHelper;
	
	// Action mode Head IK Timer (to limit how often we test).
	u32 m_iLastActionModeHeadIKTest;
	// Time in action mode
	u32 m_nActionModeTimes[AME_Max];
	// Action mode flags
	u32 m_nActionModeExpiryTime;
	// If game time is < than this, we don't force run.
	u32 m_uForcedRunDelayTime;
	// Expiry time of auto run.
	u32 m_nActionModeForcingRunExpiryTime;

	float m_TimeCapsulePushedByVehicle;
	float m_TimeRampDownCapsulePushedByVehicle;
	u32 m_TimeCapsuleFirstPushedByPlayerCapsule;

	u8 m_uStickyCount;			//how many sticky bombs this ped has deployed
	u8 m_uStickToPedProjCount;	//how many stick-to-ped projectiles (ie harpoon projectiles) has this ped shot
	u8 m_uFlareGunProjCount;	//how many flare gun projectiles has this ped shot 
	s8 m_iFailedQPedRagdollTests;			//Countdown to ragdoll falling for quadrupeds
	u8 m_modelLodIdx;
	u8 m_uWeaponAnimationsIndex;

	u8 m_iPhonePaletteIdx;

	crFrame* m_pIndependentMoverFrame;
	u32 m_WeaponBlockingDelay;

	bool m_forcePedHasNoMassInImpacts	: 1;
	bool m_disableHighFallInstantDeath	: 1;

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Related to animated ped bound update disabling
	bool m_bRagdollBoundsUpToDate : 1;
	u16 m_bRagdollBoundsUpdateRequestFrames;
#endif

	///////////////////////
	// Debug stuff
#if __ASSERT
	CTaskTypes::eTaskType m_LastRagdollControllingTaskType;
	static sysStackCollector ms_RagdollStackCollector;
#endif //__ASSERT

#if !__FINAL	
public:
	s32 m_StartTimeSay;
	s32 m_SayWhat;
	// Debug name that can be set from script and displayed to help designers debug problems.
	char m_debugPedName[16];

	// Keep track of damage taken
	static const s32 MAX_DAMAGE_RECORDS = 10;
	typedef atFixedArray<sDamageRecord, MAX_DAMAGE_RECORDS> DamageRecords;
	DamageRecords m_damageRecords;
	bool m_bSwitchedMoveBlenderThisFrame;
#endif

#if __BANK
private:
	int		m_lastLevelIndex;
	size_t	m_lastAddPhysicsCallstack[32];
public:
	struct VisemeDebugInfo
	{
		VisemeDebugInfo()
			: m_clipName((u32)0)
			, m_time(0)
			, m_preDelay(0)
			, m_preDelayRemaining(0)
		{

		}

		atHashString m_clipName;
		u32 m_time;
		u32 m_preDelay;
		u32 m_preDelayRemaining;
	};
private:

	VisemeDebugInfo m_visemeDebugInfo;

public:
	const VisemeDebugInfo& GetVisemeDebugInfo() const { return m_visemeDebugInfo; }
	float GetPreviousVisemeTime() const { return m_fPrevVisemeTime; }
	float GetVisemeBodyAdditiveWeight() const { return m_fVisemeBodyAdditiveWeight; }

public:
	void SetLastPlayerTargetingRejectionString(const char *pString);
	const char *GetLastPlayerTargetingRejectionString() const { return m_pDebugPlayerTargetingRejectionString; }
	u32 GetLastPlayerTargetingRejectedFrame() const { return m_nPlayerTargetingLastRejectedFrame; } 
private:
	char *m_pDebugPlayerTargetingRejectionString;
	u32 m_nPlayerTargetingLastRejectedFrame;
#endif

	//////////////////////////////////////////////////////////////////////////
	// static members
public:

	static s32		ms_MoneyCarriedByAllNewPeds;				// If this is -1 all rage_new peds are given some money. If >=0 this amount is given to all peds.
	static u32		ms_HealthInSnackCarriedByAllNewPeds;		// The amount of health given by a health snack pickup dropped by a dead ped
	static float	ms_ProbabilityPedsWillDropHealthSnacks;	// This is set in script and determines how frequently a dead ped will drop a snack health pickup

	// Accuracy 
	static float ms_shortLockonAccuracyModifier;
	static float ms_shortLockonTime;

	// The extra heading change is calculated by the movement system to make up the difference between
	// the desired turn rate of a ped & that which is extracted from the animation.
	static dev_bool ms_bApplyExtraHeadingChangeToPeds;

	static bank_u32 ms_uMaxRagdollPlayerLegContactTime[4];
	static bank_u32 ms_uMaxRagdollAILegContactTime[4];
	static bank_u32 ms_uMaxRagdollDoorContactTime;
	static bank_float ms_CounterRagdollImpulseStrength;
	static bank_float ms_CounterLegRagdollImpulseStrength;
	static bank_float ms_DeadPedCounterRagdollImpulseStrengthModifier;
	static bank_float ms_fMinimumKickDirectionAngle;
	static bank_float ms_fMinimumKickDirectionAngleSprintScaler;
	static bank_float ms_fForceKickDirectionAngle;
	static bank_u32 ms_RagdollWeaponDisableCollisionMask;

#if !__FINAL
	static bool ms_bUseHurtCombat;
	static bool ms_bActivateRagdollOnCollision;
#endif
	static bank_float ms_fMinDepthForFixStuckInGeometry;
#if __BANK
	static bool ms_bAlwaysFixStuckInGeometry;
#endif // __BANK
	static bank_float ms_fActivateRagdollOnCollisionDefaultAllowedSlope;

	// Windy clothing
#if __DEV
	static bool ms_bWindyClothingOverride;
	static float ms_fWindyClothingOverride;
#endif // __DEV
	static float ms_fWindyClothingMinVehicleSpeed;
	static float ms_fWindyClothingMaxVehicleSpeed;
	static float ms_fWindyClothingMinInAirSpeed;
	static float ms_fWindyClothingMaxInAirSpeed;
	static float ms_fWindyClothingMinFallSpeed;
	static float ms_fWindyClothingMaxFallSpeed;
	static float ms_fWindyClothingCullDistance;
	static float ms_fWindyClothingMinDistance;
	static float ms_fWindyClothingMaxDistance;
	static float ms_fWindyClothingScaleAtMinDistance;
	static float ms_fWindyClothingScaleAtMaxDistance;
	static float ms_fWindyClothingClavicleScale;
	static float ms_fWindyClothingForearmScale;
	static float ms_fWindyClothingThighScale;
	static float ms_fWindyClothingCalfScale;
	static float ms_fWindyClothingMinError;
	static float ms_fWindyClothingMaxError;


#if __BANK
	static bank_bool ms_bToggleCollidesAgainstGlassRagdoll;
	static bank_bool ms_bToggleCollidesAgainstGlassWeapon;
	static bank_bool ms_bEnableFakeCylindricalConstraints;
	static bank_bool ms_bShowAOVolumeDebug;
#endif
	static bank_bool ms_bAllowNormalFlattening;
	static bank_bool ms_bOnlyFlattenMovable;
	static bank_float ms_bNormalFlatteningMinDepth;
	static bank_float ms_bNormalFlatteningMaxDepth;
	static bank_bool ms_bAllowNormalVerticalizing;
	static bank_float ms_fNormalVerticalizingFactor;
	static bank_float ms_MinStandingHalfCapsuleWidthRatio;
	static bank_bool ms_bRecomputeGroundPhysicalOnVehicles;
	static bank_bool ms_bEnableGroundPhysicalCounterForce;
	static bank_bool ms_bDeleteProbeCapsuleImpacts;
	static bank_s32 ms_nCapsuleAsleepTicks;
	static bank_s32 ms_nCapsuleMotionlessTicks;

	static bank_float ms_fVolumeOfExpectedHardToPushObjects;
	static bank_float ms_fMassOfExpectedHardToPushObjects;

	static bank_float ms_fLargePedMass;
	static bank_float ms_fLargeComponentMass;

	static bank_float ms_fMaxSideNormalForSideNM;
	static bank_float ms_fVelThroughNormalForSideNM;
	static bank_float ms_fMaxNMAccumulateForSideNM;
	static bank_bool  ms_bTriggerNMInCover;
	static bank_float ms_fVelLimitForKnockFromCover;
	static bank_float ms_fVelLimitForHitByGateKnockFromCover;
	static bank_float ms_fMinObjectVelForKnockFromCover;
	static s32		  ms_iMinKnockFromCoverFrames;
	static bank_float ms_fSizeToSpeedTolForCoverGroundInstance;

	// are bike helmets disabled for all peds?
	static bool ms_bBikeHelmetsDisabledForAllPeds;
	static bool ms_bAllowHurtForAllMissionPeds;

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	static CPhysical::CSecondSurfaceConfig ms_ragdollSecondSurfaceConfig;
	static CPhysical::CSecondSurfaceConfig ms_pedSecondSurfaceConfig;
	static CPhysical::CSecondSurfaceConfig ms_skiSecondSurfaceConfig;
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	// We are ignoring this weapon hash if we come across it when colliding agains a material
	static u32 ms_uIgnoredMaterialCollisionWeaponHash;

	static u32 ms_uMissionPedLodDistance;
	static u32 ms_uDefaultPedLodDistance;

	// Scaling constants.
	static dev_float ms_minAmbientDrawingScaleFactor;
	static dev_float ms_maxAmbientDrawingScaleFactor;

	static CSpatialArray* ms_spatialArray;

	static bank_float ms_fLargeFoliageRadius;

	static bank_float ms_fOnFireDelayTime;
	static bank_float ms_fOnFireDelayTimeResistant;

	// Set the ped bias on global scaling [0.0 = normal = x (1.0f + bias)]
	static void SetLodMultiplierBias(float multiplierBias )	{ ms_LodMultiplierBias = multiplierBias; }

private:
	static bool ms_bRandomPedsDropMoney;
	static bool ms_bRandomPedsDropWeapons;
	static bool ms_bRandomPedsBlockingNonTempEventsThisFrame;

	static const fwMvFrameId ms_ExternallyDrivenDOFFrameId;

	static const fwMvFloatId ms_secondaryWeightId;
	static const fwMvFloatId ms_secondaryWeightOutId;

	static const fwMvClipId ms_gestureClipId;
	static const fwMvFloatId ms_gesturePhaseId;
	static const fwMvFloatId ms_gestureRateId;
	static const fwMvFloatId ms_gesturePhaseOutId;
	static const fwMvFloatId ms_gestureMaxWeightId;
	static const fwMvFilterId ms_gestureFilterId;
	static const fwMvFloatId ms_gestureExpressionWeightId;
	static const fwMvRequestId ms_gestureRequestId;
	static const fwMvFloatId ms_gestureDurationId;
	static const fwMvFlagId ms_gestureBlendInFlagId;
	static const fwMvFlagId ms_gestureBlendOutFlagId;

	static const fwMvClipId ms_visemeBodyAdditiveClipId;
	static const fwMvFloatId ms_visemeBodyAdditivePhaseId;
	static const fwMvRequestId ms_visemeBodyAdditiveRequestId;
	static const fwMvFloatId ms_visemeBodyAdditiveWeightId;
	static const fwMvFloatId ms_visemeBodyAdditiveDurationId;
	static const fwMvFlagId ms_visemeBodyAdditiveBlendInFlagId;
	static const fwMvFlagId ms_visemeBodyAdditiveBlendOutFlagId;

	static const s32 MAX_PEDS_THAT_CAN_GET_WET_THIS_FRAME = 8;
	static atFixedArray<CPed*, MAX_PEDS_THAT_CAN_GET_WET_THIS_FRAME> ms_pedsThatCanGetWetThisFrame;

	static float ms_LodMultiplierBias;
	static float GetLodMultiplierBias() 						{ return ms_LodMultiplierBias; }

	static const u16 ms_cachedBoneTags[kOffsetCount];

	static bool HasEnabledFixFor7458166();
	static bool HasEnabledFixFor7459244();

	static bool sbForceRagDollIfPedForcedThroughWall;
	static bool ms_DisableFixFor7458166;
	static bool ms_DisableFixFor7459244;
};

template<> inline CPed* fwPool<CPed>::GetSlot(s32 index)
{
	CPed* ped = (CPed*)fwBasePool::GetSlot(index);
	if (!ped || ped->GetIsInPopulationCache())
		return NULL;

	return ped;
}

template<> inline const CPed* fwPool<CPed>::GetSlot(s32 index) const
{
	CPed* ped = (CPed*)fwBasePool::GetSlot(index);
	if (!ped || ped->GetIsInPopulationCache())
		return NULL;

	return ped;
}

inline bool CPed::IsInOrAttachedToCar() const
{
	if (GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return true;
	}

	if (const fwAttachmentEntityExtension *extension = GetAttachmentExtension())
	{
		fwEntity *entity = extension->GetAttachParent();
		return (entity && entity->GetType()==ENTITY_TYPE_VEHICLE);
	}

	return false;
}

//
// name:		GetPosition
// description:	Get position from matrix (should exist for ped)
/*inline const Vector3& CPed::GetPosition() const 
{
	Assert(m_pMat);
	return m_pMat->GetVector(3);
}

//
// name:		GetPosition
// description:	Get position from matrix (should exist for ped)
inline Vector3& CPed::GetPosition() 
{
	Assert(m_pMat);
	return *(Vector3*)&m_pMat->GetVector(3);
}
*/

//////////////////////////////////////////////////////////////////////////
// CPedVfx
//////////////////////////////////////////////////////////////////////////

class CPedVfx
{
	#define	PEDVFX_NUM_PREV_DOWNWARD_SPEEDS		(3)

public:

	CPedVfx();
	~CPedVfx();

	void Init(CPed* pPed);

	inline float GetBreathRate() const {return m_breathRate;}
	inline void SetBreathRate(const float f) {m_breathRate = f;}
	inline bool GetIsWadeSpineIntersecting() const {return m_isWadeSpineIntersecting;}
	inline void SetIsWadeSpineIntersecting(bool val) {m_isWadeSpineIntersecting = val;}
	inline void SetFootWetnessInfo(const int isLeftFoot, const VfxLiquidType_e liquidType) {m_footLiquidAttachInfo[isLeftFoot].Set(VFXMTL_WET_MODE_FOOT, liquidType);}
	void ResetOutOfWaterBoneTimers();
	void SetOutOfWaterBoneTimer(int vfxSkeletonBoneIdx, float timer);

	void ProcessVfxPed();
	void ProcessVfxFootStep(bool isLeft, bool isHind, Mat34V_In vFootMtx, const u32 foot, Vec3V_In vGroundNormal);
	void UpdateFootWetness();
	bool GetBloodVfxDisabled() const;

	bool ProcessCollisionVfx(VfxCollisionInfo_s& vfxColnInfo);

	void SetPrevDownwardSpeed(int idx, float actualSpeed);
	float GetPrevDownwardSpeed(int idx);


private:

	float m_breathTimer;				// breath effects
	float m_breathRate;
	bool  m_isWadeSpineIntersecting;	// second surface wading flag
	u8 m_prevDownwardSpeed[PEDVFX_NUM_PREV_DOWNWARD_SPEEDS];
	CVfxLiquidAttachInfo m_footLiquidAttachInfo[2];
	int m_numOutOfWaterBoneTimers;
	float* m_pOutOfWaterBoneTimers;
	float m_camOutOfWaterTimer;

	// Owner ped
	CPed* m_pPed;
}; 

#endif
