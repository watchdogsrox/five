#ifndef EVENT_DAMAGE_H
#define EVENT_DAMAGE_H

#include "event/events.h"
#include "Peds/PlayerArcadeInformation.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "task/Weapons/WeaponTarget.h"
#include "weapons/info/AmmoInfo.h"

// Time in ms before ped WeaponDamageEntity can be overridden by any entity type
#define PLAYER_WEAPON_DAMAGE_TIMEOUT	(1000)

class CPedDamageResponse
{
public:
	CPedDamageResponse() : m_bCalculated(false), m_bClonePedKillCheck(false) {}
	virtual ~CPedDamageResponse(){}

	void ResetDamage()
	{
		m_fHealthLost = 0.0f;
		m_fArmourLost = 0.0f;
		m_fEnduranceLost = 0.0f;
		m_bCalculated = false;
		m_bAffectsPed = false;
		m_bKilled = false;
		m_bInjured = false;
		m_bForceInstantKill = false;
		m_bForceInstantKO = false;
		m_bHeadShot = false;
		m_bHitTorso = false;
		m_bInvincible = false;
		m_bIncapacitated = false;
	};

	bool m_bCalculated :1;
	bool m_bAffectsPed :1;
	bool m_bKilled :1;
	bool m_bInjured :1;
	bool m_bForceInstantKill :1;
	bool m_bForceInstantKO :1;
	bool m_bClonePedKillCheck :1;
	bool m_bHeadShot :1;
	bool m_bHitTorso : 1;
	bool m_bInvincible : 1;
	bool m_bIncapacitated : 1;

	float m_fHealthLost;
	float m_fArmourLost;
	float m_fEnduranceLost;
};

#if !__FINAL
static const char* g_sDamageRejectionStrings[] =
{
	"DRR_None",
	"DRR_Invincible",
	"DRR_DebugInvincible",
	"DRR_MigratingClone",
	"DRR_RageSpecialAbility",
	"DRR_ImmuneToDrownInWater",
	"DRR_ImmuneToDrownInVehicle",
	"DRR_HitBulletProofVest",
	"DRR_PlayerCantBeDamaged",
	"DRR_FireProof",
	"DRR_ImmuneToWaterCannon",
	"DRR_InflictorSameGroup",
	"DRR_CantBeShotInVehicle",
	"DRR_WaterCannonGhosted",
	"DRR_AttachedPortablePickup",
	"DRR_NetworkClone",
	"DRR_NotDamagedByAnything",
	"DRR_FallingAttachedToNonVeh",
	"DRR_NeverFallOffSkis",
	"DRR_TamperInvincibleCop",
	"DRR_AttachedToInflictor",
	"DRR_OnlyDamagedByOtherEnt",
	"DRR_ODBP_NullInflictor",
	"DRR_ODBP_NotPlayer",
	"DRR_ODBP_NonPlayerVehicle",
	"DRR_OnlyDamagedByPlayer",
	"DRR_ODBRG_NullInflictor",
	"DRR_ODBRG_Mismatch",
	"DRR_ODBRG_VehNoDriver",
	"DRR_ODBRG_DriverMismatch",
	"DRR_OnlyDamagedByRelGroup",
	"DRR_NotDamagedByRelGroup",
	"DRR_NotDamagedByRelGroupDriver",
	"DRR_OnlyDamagedOnScript",
	"DRR_OnlyDamagedOnScriptRSI",
	"DRR_NotDamagedByMelee",
	"DRR_DisableDamagingOwner",
	"DRR_NotDamagedByBullets",
	"DRR_IgnoresExplosions",
	"DRR_NotDamagedByFlames",
	"DRR_NotDamagedBySmoke",
	"DRR_NotDamagedByCollisions",
	"DRR_NotDamagedByAnythingBHR",
	"DRR_DontApplyDamage",
	"DRR_NotDamagableByThisPlayer",
	"DRR_Respawning",
	"DRR_ZeroPlayerMod",
	"DRR_ZeroPedMod",
	"DRR_NotDamagedByPed",
	"DRR_ZeroSpecialMod",
	"DRR_ZeroMeleeMod",
	"DRR_ZeroPlayerWeaponMod",
	"DRR_ZeroAiWeaponMod",
	"DRR_ZeroDefenseMod",
	"DRR_ZeroTakedownDefenseMod",
	"DRR_ZeroPlayerMeleeDefMod",
	"DRR_ZeroPlayerWeaponDefMod",
	"DRR_ZeroPlayerVehDefMod",
	"DRR_ZeroNetLimbMod",
	"DRR_ZeroLimbMod",
	"DRR_ZeroLightArmorMod",
	"DRR_ZeroPlayeMinigunDefMod",
	"DRR_Tranquilizer",
	"DRR_Friendly",
	"DRR_OutOfRange",
	"DRR_ImpactsDisabledInMP",
	"DRR_NoFriendlyFire"
};
#endif //__FINAL

class CPedDamageCalculator
{
public:

#if !__FINAL
	enum eDamageRejectionReasons
	{
		DRR_None = 0,
		DRR_Invincible,
		DRR_DebugInvincible,
		DRR_MigratingClone,
		DRR_RageAbility,
		DRR_ImmuneToDrown,
		DRR_ImmuneToDrownInVehicle,
		DRR_BulletProofVest,
		DRR_PlayerCantBeDamaged,
		DRR_FireProof,
		DRR_ImmuneToWaterCannon,
		DRR_InflictorSameGroup,
		DRR_CantBeShotInVehicle,
		DRR_WaterCannonGhosted,
		DRR_AttachedPortablePickup,
		DRR_NetworkClone,
		DRR_NotDamagedByAnything,
		DRR_FallingAttachedToNonVeh,
		DRR_NeverFallOffSkis,
		DRR_TamperInvincibleCop,
		DRR_AttachedToInflictor,
		DRR_OnlyDamagedByOtherEnt,
		DRR_OnlyDamagedByPlayerNullInflictor,
		DRR_OnlyDamagedByPlayerNotPlayer,
		DRR_OnlyDamagedByPlayerNonPlayerVehicle,
		DRR_OnlyDamagedByPlayer,
		DRR_OnlyDamagedByRelGroupNullInflictor,
		DRR_OnlyDamagedByRelGroupMismatch,
		DRR_OnlyDamagedByRelGroupVehNoDriver,
		DRR_OnlyDamagedByRelGroupVehDriverGroupMismatch,
		DRR_OnlyDamagedByRelGroup,
		DRR_NotDamagedByRelGroup,
		DRR_NotDamagedByRelGroupDriver,
		DRR_OnlyDamagedOnScript,
		DRR_OnlyDamagedOnScriptRSI,
		DRR_NotDamagedByMelee,
		DRR_DisableDamagingOwner,
		DRR_NotDamagedByBullets,
		DRR_IgnoresExplosions,
		DRR_NotDamagedByFlames,
		DRR_NotDamagedBySmoke,
		DRR_NotDamagedByCollisions,
		DRR_NotDamagedByAnythingButHasReactions,
		DRR_DontApplyDamage,
		DRR_NotDamagableByThisPlayer,
		DRR_Respawning,
		DRR_ZeroPlayerMod,
		DRR_ZeroPedMod,
		DRR_NotDamagedByPed,
		DRR_ZeroSpecialMod,
		DRR_ZeroMeleeMod,
		DRR_ZeroPlayerWeaponMod,
		DRR_ZeroAiWeaponMod,
		DRR_ZeroDefenseMod,
		DRR_ZeroTakedownDefenseMod,
		DRR_ZeroPlayerMeleeDefMod,
		DRR_ZeroPlayerWeaponDefMod,
		DRR_ZeroPlayerVehDefMod,
		DRR_ZeroNetLimbMod,
		DRR_ZeroLimbMod,
		DRR_ZeroLightArmorMod,
		DRR_ZeroPlayerWeaponMinigunDefMod,
		DRR_Tranquilizer,
		DRR_Friendly,
		DRR_OutOfRange,
		DRR_ImpactsDisabledInMP,
		DRR_NoFriendlyFire
	};
#endif //__FINAL

	enum eDamageFlags
	{
		DF_None								= 0,
		DF_IsAccurate						= BIT0,
		DF_MeleeDamage						= BIT1,
		DF_SelfDamage						= BIT2,
		DF_ForceMeleeDamage					= BIT3,
		DF_IgnorePedFlags					= BIT4,
		DF_ForceInstantKill					= BIT5,
		DF_IgnoreArmor						= BIT6,
		DF_IgnoreStatModifiers				= BIT7,
		DF_FatalMeleeDamage					= BIT8,
		DF_AllowHeadShot					= BIT9,
		DF_AllowDriverKill					= BIT10,
		DF_KillPriorToClearedWantedLevel	= BIT11,
		DF_SuppressImpactAudio				= BIT12,
		DF_ExpectedPlayerKill				= BIT13,
		DF_DontReportCrimes					= BIT14,
		DF_PtFxOnly							= BIT15,
		DF_UsePlayerPendingDamage			= BIT16,
		DF_AllowCloneMeleeDamage			= BIT17,
		DF_NoAnimatedMeleeReaction			= BIT18,
		DF_IgnoreRemoteDistCheck			= BIT19,
		DF_VehicleMeleeHit					= BIT20,
		DF_EnduranceDamageOnly				= BIT21,
		DF_HealthDamageOnly					= BIT22,
		DF_DamageFromBentBullet				= BIT23

#if !__ASSERT
		,
		DF_LastDamageFlag					= BIT23
#else
		,
		DF_DontAssertOnNullInflictor		= BIT24,
		DF_LastDamageFlag					= BIT24
#endif // __ASSERT
	};

	CPedDamageCalculator(const CEntity* pInflictor,	 const float fRawDamage,
		const u32 nWeaponUsedHash, const int nComponent,
		const bool bJumpedOutOfMovingCar=false,
		const u8 nDamageAggregationCount = 0);
	~CPedDamageCalculator();

	// Cloud tunables
	static void InitTunables(); 

	void SetComponentFromPedBoneTag(CPed* pVictim, eAnimBoneTag nBoneTag);
	void ApplyDamageAndComputeResponse(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& flags, const u32 uParentMeleeActionResultID = 0, const float fDist = FLT_MAX);

	float GetRawDamage() const { return m_fRawDamage; }

	static float GetAiMeleeWeaponDamageModifier()	{ return sm_fAiMeleeWeaponDamageModifier; }
	static float GetAiWeaponDamageModifier()		{ return sm_fAiWeaponDamageModifier; }

	static float* GetAiMeleeWeaponDamageModifierPtr()	{ return &sm_fAiMeleeWeaponDamageModifier; }
	static float* GetAiWeaponDamageModifierPtr()		{ return &sm_fAiWeaponDamageModifier; }

	static void ResetAiMeleeWeaponDamageModifier()	{ sm_fAiMeleeWeaponDamageModifier = sm_fAiMeleeWeaponDamageModifierDefault; }
	static void ResetAiWeaponDamageModifier()		{ sm_fAiWeaponDamageModifier = sm_fAiWeaponDamageModifierDefault; }

	static void SetAiMeleeWeaponDamageModifier(float fModifier) { sm_fAiMeleeWeaponDamageModifier = fModifier; }
	static void SetAiWeaponDamageModifier(float fModifier)		{ sm_fAiWeaponDamageModifier = fModifier; }

	//Register a kill with the game stats - Needs to be called for clone peds in network code.
	static void RegisterKill(const CPed* pPed, const CEntity* pInflictor, const u32 nWeaponUsedHash, const bool bHeadShot, const int weaponDamageComponent, const bool bWithMeleeWeapon, const fwFlags32& flags);

	static void RegisterIncapacitated(const CPed* pPed, const CEntity* pInflictor, const u32 nWeaponUsedHash, const bool bHeadShot, const int weaponDamageComponent, const bool bWithMeleeWeapon, const fwFlags32& flags);

	static void TriggerFriendlyAudioForPlayerKill(const CPed& rPlayer, const CPed& rTarget);
	static void TriggerSuspectKilledAudio(CPed& rLawPed, const CPed& rPlayer);

	static bool WasDamageFromEntityBeforeWantedLevelCleared(CEntity* pInflictor, CPed* pVictim);

#if !__FINAL
	static const char* GetRejectionReasonString(u32 uRejectionReason) { return g_sDamageRejectionStrings[uRejectionReason]; }
#endif //__FINAL

private:
	bool AccountForPedFlags(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& damageFlags);
	void AccountForPedDamageStats(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& flags, const float fDist);
	void AccountForPedAmmoTypes(CPed* pVictim);
	void AccountForPedArmourAndBlocking(CPed* pVictim, const CWeaponInfo* pWeaponInfo, CPedDamageResponse& damageResponse, const fwFlags32& flags);
	void ComputeWillKillPed(CPed* pVictim, const CWeaponInfo* pWeaponInfo, CPedDamageResponse& damageResponse, const fwFlags32& flags, const u32 uParentMeleeActionResultID = 0);
	bool ComputeWillForceDeath(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& flags);

	float GetEnduranceDamage(const CPed* pPed, const CWeaponInfo* pWeaponInfo, const fwFlags32& flags) const;
	void ApplyEnduranceDamage(CPed* pPed, const CWeaponInfo* pWeaponInfo, CPedDamageResponse& damageResponse, const fwFlags32& flags, bool& out_bBlockHealthDamage);

	void ApplyPadShake(const CPed* pVictim, const bool bIsPlayer, const float fDamage);
	static void ApplyTriggerShake(const u32 weaponUsedHash, const bool bIsKilled, CControl *pControl);

	//This updates all PED damage tracking.
	void  UpdateDamageTrackers(CPed* pVictim, CPedDamageResponse& damageResponse, const bool bPedWasDeadOrDying, const fwFlags32& flags);
	bool  ComputeWillUpdateDamageTrackers(CPed* pVictim,  CEntity* damager, u32& weaponUsedHash, const float totalDamage, const fwFlags32& flags);

#if __ASSERT
	void  CheckForAssertsWhenVictimIsKilled(CPed* pVictim, CPedDamageResponse& damageResponse, bool bDontAssertOnNullInflictor);
#endif // __ASSERT

	RegdEnt m_pInflictor;
	float m_fRawDamage;
	u8 m_nDamageAggregationCount;
	int m_nHitComponent;
	u32 m_nWeaponUsedHash;
	bool m_bJumpedOutOfMovingCar;
	CAmmoInfo::SpecialType m_eSpecialAmmoType;

#if !__FINAL
	static u32 m_uRejectionReason;
#endif //__FINAL

	static float sm_fAiMeleeWeaponDamageModifierDefault;
	static float sm_fAiMeleeWeaponDamageModifier;
	static float sm_fAiWeaponDamageModifierDefault;
	static float sm_fAiWeaponDamageModifier;

	// Cloud tunables for special ammo behaviour

	// Gunrunning
	static float sm_fSpecialAmmoArmorPiercingDamageBonusPercentToApplyToArmor;
	static bool  sm_bSpecialAmmoArmorPiercingIgnoresArmoredHelmets;
	static float sm_fSpecialAmmoIncendiaryPedDamageMultiplier;
	static float sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier;
	static float sm_fSpecialAmmoIncendiaryPedFireChanceOnHit;
	static float sm_fSpecialAmmoIncendiaryPlayerFireChanceOnHit;
	static float sm_fSpecialAmmoIncendiaryPedFireChanceOnKill;
	static float sm_fSpecialAmmoIncendiaryPlayerFireChanceOnKill;
	static float sm_fSpecialAmmoFMJPedDamageMultiplier;
	static float sm_fSpecialAmmoFMJPlayerDamageMultiplier;
	static float sm_fSpecialAmmoHollowPointPedDamageMultiplierForNoArmor;
	static float sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForNoArmor;
	static float sm_fSpecialAmmoHollowPointPedDamageMultiplierForHavingArmor;
	static float sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForHavingArmor;

	// Xmas17
	static bool  sm_bSpecialAmmoIncendiaryForceForWeaponTypes;
	static float sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnHit;
	static float sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnHit;
	static float sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnKill;
	static float sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnKill;	

	// Assault Course
	static float sm_fSpecialAmmoHollowPointRevolverPedDamageMultiplierForNoArmor;
	static float sm_fSpecialAmmoHollowPointRevolverPlayerDamageMultiplierForNoArmor;

	// Endurance
	static float sm_fFallEnduranceDamageMultiplier;
	static float sm_fRammedByVehicleEnduranceDamageMultiplier;
	static float sm_fRunOverByVehicleEnduranceDamageMultiplier;
	static float sm_fVehicleCollidedWithEnvironmentEnduranceDamageMultiplier;
};

//The ped has experienced damage.
class CEventDamage : public CEventEditableResponse
{
public:
	CEventDamage(CEntity* pInflictor,
		const u32 nTime,
		const u32 nWeaponUsedHash);

	CEventDamage(const CEventDamage& src);
	virtual ~CEventDamage();

	CEventDamage& operator=(const CEventDamage& src);

	virtual int GetEventType() const {return EVENT_DAMAGE;}
	virtual int GetEventPriority() const;
	virtual CEventEditableResponse* CloneEditable() const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CPed* GetPlayerCriminal() const;
	virtual void OnEventAdded(CPed *pPed) const;
	virtual CEntity* GetSourceEntity() const;
	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool RequiresAbortForRagdoll() const {return m_bRagdollResponse;}
	virtual bool RequiresAbortForMelee(const CPed* pPed) const;
	virtual void ClearRagdollResponse() { m_bRagdollResponse = false; }
	virtual void ComputePersonalityResponseToDamage(CPed* pPed, CEventResponse& response) const;

	virtual bool CanDamageEventInterruptNM(const CPed* pPed) const;
	virtual bool CanBeInterruptedBySameEvent() const { return true; }


#if !__FINAL
	virtual void Debug(const CPed& ped) const;
#endif

	bool IsSameEventForAI(CEventDamage *pEvent) const;

	bool HasKilledPed() const {return (m_pedDamageResponse.m_bKilled && GetWitnessedFirstHand());}
	bool HasInjuredPed() const {return (m_pedDamageResponse.m_bInjured && GetWitnessedFirstHand());}
	bool HasDoneInstantKill() const {return (m_pedDamageResponse.m_bForceInstantKill && GetWitnessedFirstHand());}
	bool HasFallenDown() const;

	// returns true if we should ragdoll when we're able to
	void ComputeDamageAnim(CPed *pPed, CEntity* pInflictor, float fHitDirn, int nHitBoneTag, fwMvClipSetId &clipSetId, fwMvClipId &clipId, bool& bNeedsTask, bool& bNeedsFallTask);
	//	void ComputeHeadShot(bool& bHeadShot) const;
	//	void ComputeBodyPartToRemove(int& iBodyPartToRemove) const;
	CEntity* GetInflictor() const {return m_pInflictor.Get();}
	u32 GetDamageTime() const{return m_nDamageTime;}
	u32 GetWeaponUsedHash() const {return m_nWeaponUsedHash;}

	float GetDamageApplied() const {return m_pedDamageResponse.m_fHealthLost + m_pedDamageResponse.m_fArmourLost;}

	//	ePedPieceTypes GetHitZone() const {return m_eHitZone;}
	//	void SetHitZone(ePedPieceTypes eHitZone) {m_eHitZone = eHitZone;}
	//	u8 GetPedHitDir() const {return m_iPedHitDir;}

	//	bool GetFallDown() {return m_bFallDown;}
	//	bool GetIsUpperBodyAnim();

	void SetIsStealthMode(const bool b) {m_bStealthMode=b;}
	bool GetIsStealthMode() const {return m_bStealthMode;}

	CPedDamageResponse& GetDamageResponseData() {return m_pedDamageResponse;}

	bool HasRagdollResponseTask() const {return m_bRagdollResponse;}
	bool HasPhysicalResponseTask() const {return m_pPhysicalResponseTask != NULL;}
	int GetPhysicalResponseTaskType() const;

	// Used by the task couter, anywhere where teh task is going to be used, use retrieve response task
	const aiTask* GetPhysicalResponseTask() const {return m_pPhysicalResponseTask;}
	aiTask* GetPhysicalResponseTask() {return m_pPhysicalResponseTask;}

	//CTask* RetrievePhysicalResponseTask() {CTask* pReturnTask = m_pPhysicalResponseTask; m_pPhysicalResponseTask=NULL; return pReturnTask;}
	aiTask* ClonePhysicalResponseTask() const;

	void SetMeleeResponse(bool bMeleeResponse) { m_bMeleeResponse = bMeleeResponse; }
	bool GetIsMeleeResponse() const { return m_bMeleeResponse; }

	void SetMeleeDamage(bool bMeleeDamage) { m_bMeleeDamage = bMeleeDamage; }
	bool GetIsMeleeDamage() const { return m_bMeleeDamage; }

	void SetCanReportCrimes(bool bReportCrimes) { m_bCanReportCrimes = bReportCrimes; }
	bool GetCanReportCrimes() const { return m_bCanReportCrimes; }

	void SetPhysicalResponseTask(aiTask* pTask, bool bRagdollTask)
	{
		// if we already have a physical response task, delete it
		// need to handle this here as events of the same type 
		// can be reused by the event queue
		if (m_pPhysicalResponseTask)
		{
			delete m_pPhysicalResponseTask;
		}

		m_pPhysicalResponseTask = pTask;
		m_bRagdollResponse = bRagdollTask;
	}


private:
	void From(const CEventDamage& src);



private:
	RegdEnt m_pInflictor;
	u32 m_nDamageTime;

	u32 m_nWeaponUsedHash;
	CPedDamageResponse m_pedDamageResponse;

	RegdaiTask m_pPhysicalResponseTask;

	u8 m_bStealthMode :1;
	u8 m_bRagdollResponse :1;
	u8 m_bMeleeResponse :1;
	u8 m_bMeleeDamage : 1;
	u8 m_bCanReportCrimes : 1;

	static dev_float sm_fHitByVehicleAgitationThreshold;
};

#if CNC_MODE_ENABLED
class CEventIncapacitated : public CEvent
{
public:
	CEventIncapacitated()
	{
	}

	virtual ~CEventIncapacitated()
	{
	}

	virtual int GetEventType() const { return EVENT_INCAPACITATED; }
	virtual int GetEventPriority() const;
	virtual CEvent* Clone() const;
	virtual bool CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const;
	virtual bool CanBeInterruptedBySameEvent() const { return true; }
	virtual bool RequiresAbortForRagdoll() const { return true; }
};
#endif

//The ped knows it is about to die.
class CEventDeath : public CEvent
{
public:
	CEventDeath(const bool bHasDrowned, const bool bUseRagdoll = false);
	CEventDeath(const bool bHasDrowned, const int iTimeOfDeath, const bool bUSeRagdoll = false);
	virtual ~CEventDeath();

	virtual int GetEventType() const {return EVENT_DEATH;}
	virtual int GetEventPriority() const;
	virtual CEvent* Clone() const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual bool RequiresAbortForRagdoll() const {return m_bUseRagdoll;}

	int GetTimeOfDeath() const {return m_iTimeOfDeath;}
	bool HasDrowned() const {return m_bHasDrowned;}

	void SetClipSet(u32 clipSetId, u32 clipId, float startPhase=0.0f) 
	{
		m_clipSetId = clipSetId;
		m_clipId = clipId;
		m_startPhase = startPhase;
	}

	// PURPOSE: Instructs the dead task to go directly to the ragdoll frame when it first runs
	//			Only use this in cases where you have specifically animated the ped into a good death pose.
	void SetUseRagdollFrame(bool bUseRagdollFrame) { m_bUseRagdollFrame = bUseRagdollFrame; }
	void SetDisableVehicleImpactsWhilstDying(bool bDisable)  { m_bDisableVehicleImpactsWhilstDying = bDisable; }
	virtual bool CanBeInterruptedBySameEvent() const { return true; }
	void SetNaturalMotionTask(aiTask* pTask) { 
		taskAssert(m_bUseRagdoll);
		taskAssert(pTask && pTask->GetTaskType()==CTaskTypes::TASK_NM_CONTROL);
		if (m_pNaturalMotionTask)
			delete m_pNaturalMotionTask;
		m_pNaturalMotionTask=pTask;
	}

#if !__FINAL
	virtual void Debug(const CPed&  ) const {}
#endif

private:
	u32	m_clipSetId;	// could be an clip set id or dictionary hash - see Flag_clipsOverridenByClipSetHash and Flag_clipsOverridenByDictHash
	u32	m_clipId;		// Clip ids and clip hash keys are now the same thing... huzzah!
	float m_startPhase;
	int m_iTimeOfDeath;
	bool m_bHasDrowned;
	bool m_bUseRagdoll;
	bool m_bUseRagdollFrame;
	bool m_bDisableVehicleImpactsWhilstDying;
	RegdaiTask m_pNaturalMotionTask;
};



//Car upsidedown

//inline int CEventDamage::GetEventPriority() const {return  m_bRagdollResponse ? E_PRIORITY_DAMAGE_WITH_RAGDOLL : E_PRIORITY_DAMAGE;}
inline int CEventDeath::GetEventPriority() const {return (m_bUseRagdoll ? E_PRIORITY_DEATH_WITH_RAGDOLL : E_PRIORITY_DEATH);}


class CEventWrithe : public CEvent
{
public:
	CEventWrithe(const CWeaponTarget& Inflictor, bool bFromGetUp = false);
	virtual ~CEventWrithe();

	virtual int GetEventType() const {return EVENT_WRITHE;}
	virtual int GetEventPriority() const { return E_PRIORITY_WRITHE; }
	virtual CEvent* Clone() const {return rage_new CEventWrithe(m_Inflictor, m_bFromGetUp);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual bool RequiresAbortForRagdoll() const {return false;}

#if !__FINAL    
	virtual void Debug(const CPed&  ) const {}
#endif

private:
	CWeaponTarget m_Inflictor;
	bool m_bFromGetUp;

};

class CEventHurtTransition : public CEvent
{
public:
	CEventHurtTransition(CEntity* pTarget);
	virtual ~CEventHurtTransition();

	virtual int GetEventType() const {return EVENT_HURT_TRANSITION;}
	virtual int GetEventPriority() const { return E_PRIORITY_HURT_TRANSITION; }
	virtual CEvent* Clone() const {return rage_new CEventHurtTransition(m_pTarget);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& out_response) const;

private:
	RegdEnt m_pTarget;
};


#endif //#ifndef EVENT_DAMAGE_H
