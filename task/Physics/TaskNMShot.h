// Filename   :	TaskNMShot.h
// Description:	Natural Motion shot class (FSM version)

#ifndef INC_TASKNMSHOT_H_
#define INC_TASKNMSHOT_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/ThreatenedHelper.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMShot
//
class CClonedNMShotInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMShotInfo( CEntity* pShotBy, u32 nWeaponHash, int nComponent, const Vector3& vecHitPositionWorldSpace, const Vector3& vecHitImpulseWorldSpace, 
		const Vector3& vecHitPolyNormalWorldSpace);
	CClonedNMShotInfo();
	~CClonedNMShotInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_SHOT;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bHasShotByEntity, "Has shot by entity");

		if (m_bHasShotByEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_shotByEntity, "Shot by entity");
		}

		SERIALISE_UNSIGNED(serialiser, m_nWeaponHash, SIZEOF_WEAPONHASH, "Weapon hash");
		u32 component = (u32)m_nComponent;
		SERIALISE_UNSIGNED(serialiser, component, SIZEOF_COMPONENT, "Component");
		m_nComponent = (u8)component;
		SERIALISE_VECTOR(serialiser, m_vecOriginalHitImpulseWorldSpace, 600.0f, SIZEOF_HITFORCE, "Hit world impulse");
		SERIALISE_VECTOR(serialiser, m_vecOriginalHitPositionLocalSpace, LOCAL_POS_MAG_LIMIT, SIZEOF_HITPOS, "Hit local pos");
		SERIALISE_VECTOR(serialiser, m_vecOriginalHitPolyNormalWorldSpace, 1.1f, SIZEOF_HITPOLYNORMAL, "Hit world poly normal");
	}

	u32 GetWeaponHash() const { return m_nWeaponHash; }
	u8 GetComponent() const { return m_nComponent; }
	const Vector3& GetHitImpulse() const { return m_vecOriginalHitImpulseWorldSpace; }

private:

	CClonedNMShotInfo(const CClonedNMShotInfo &);
	CClonedNMShotInfo &operator=(const CClonedNMShotInfo &);

	static const unsigned int SIZEOF_WEAPONHASH			= 32;
	static const unsigned int SIZEOF_COMPONENT			= 8;
	static const unsigned int SIZEOF_HITPOS				= 16;
	static const unsigned int SIZEOF_HITPOLYNORMAL		= 16;
	static const unsigned int SIZEOF_HITFORCE			= 20;

	Vector3		m_vecOriginalHitPositionLocalSpace;
	Vector3		m_vecOriginalHitPolyNormalWorldSpace;
	Vector3		m_vecOriginalHitImpulseWorldSpace;
	ObjectId	m_shotByEntity;
	u32			m_nWeaponHash;
	u8			m_nComponent;
	bool		m_bHasShotByEntity;
};

//// be hit
// triggers:	getting shot
//				taking a melee hit
class CTaskNMShot : public CTaskNMBehaviour
{
public:

	////////////////////////////////
	// SHOT
	////////////////////////////////
	enum 
	{
		SHOT_DEFAULT,
		SHOT_NORMAL,
		SHOT_SHOTGUN,
		SHOT_UNDERWATER,
		SHOT_DANGLING,
		SHOT_AGAINST_WALL,
	};

	struct Tunables : CTuning
	{
		Tunables();

		s32 m_MinimumShotReactionTimePlayerMS;
		s32	m_MinimumShotReactionTimeAIMS;
		bool m_bUseClipPoseHelper;
		bool m_bEnableDebugDraw;
		float m_fImpactConeAngleFront; // ... in degrees!
		float m_fImpactConeAngleBack; // ... in degrees!
		RagdollComponent m_eImpactConeRagdollComponent;
		int m_iShotMinTimeBeforeGunThreaten;
		int m_iShotMaxTimeBeforeGunThreaten;
		int m_iShotMinTimeBetweenFireGun;
		int m_iShotMaxTimeBetweenFireGun;

		int m_iShotMaxBlindFireTimeL;
		int m_iShotMaxBlindFireTimeH;
		float m_fShotBlindFireProbability;
		float m_fShotHeadAngleToFireGun;
		float m_fShotWeaponAngleToFireGun;
		float m_fFireWeaponStrengthForceMultiplier;

		float m_fMinFinisherShotgunTotalImpulseNormal;
		float m_fMinFinisherShotgunTotalImpulseBraced;
		float m_fFinisherShotgunBonusArmedSpeedModifier;

		bool m_ScaleSnapWithSpineOrientation;
		float m_MinSnap; // minimum snap amount

		int m_BlendOutDelayStanding;
		int m_BlendOutDelayBalanceFailed;

		struct ShotAgainstWall
		{
			ShotAgainstWall()
			{}

			float m_HealthRatioLimit; // shot against wall will only be used on peds whose health ratio is below this range
			float m_WallProbeRadius;	// Radius of the swept sphere probe
			float m_WallProbeDistance; // Distance of the swept sphere probe
			float m_ProbeHeightAbovePelvis; // height above the pelvis to perform the probe
			float m_ImpulseMult;
			float m_MaxWallAngle; // The maximum allowed angle between shot direction and wall normal

			PAR_SIMPLE_PARSABLE;
		};

		ShotAgainstWall m_ShotAgainstWall;

		float m_BCRExclusionZone;

		struct Impulses
		{
			// Sniper-specific impulse tuning requested by Jason Bone.  Is being used to tune hits from sniper rifles to stop peds from flipping over from hits
			// to the upper body (specifically the clavicles, neck and upper spine).  Should really be using the existing weapon force tuning functionality to set
			// lower forces for the bones that are causing issues but this ended up being the preferred solution...
			struct SniperImpulses
			{
				SniperImpulses() {}


				float m_MaxHealthImpulseMult;
				float m_MinHealthImpulseMult;

				float m_MaxDamageTakenImpulseMult;
				float m_MinDamageTakenImpulseMult;
				float m_MaxDamageTakenThreshold;
				float m_MinDamageTakenThreshold;

				float m_DefaultKillShotImpulseMult;
				float m_DefaultMPKillShotImpulseMult;

				PAR_SIMPLE_PARSABLE;
			};

			Impulses()
			{}

			float m_MaxArmourImpulseMult;
			float m_MinArmourImpulseMult;

			float m_MaxHealthImpulseMult;
			float m_MinHealthImpulseMult;

			float m_MaxDamageTakenImpulseMult;
			float m_MinDamageTakenImpulseMult;
			float m_MaxDamageTakenThreshold;
			float m_MinDamageTakenThreshold;

			float m_DefaultKillShotImpulseMult;
			float m_DefaultRapidFireKillShotImpulseMult;
			float m_DefaultMPKillShotImpulseMult;
			float m_DefaultMPRapidFireKillShotImpulseMult;

			float m_RapidFireBoostShotImpulseMult;
			int	  m_RapidFireBoostShotMinRandom;
			int	  m_RapidFireBoostShotMaxRandom;

			float m_ShotgunMaxSpeedForLiftImpulse;
			float m_ShotgunMaxLiftImpulse;
			float m_ShotgunLiftNearThreshold;
			float m_ShotgunChanceToMoveSpine3ImpulseToSpine2;
			float m_ShotgunChanceToMoveNeckImpulseToSpine2;
			float m_ShotgunChanceToMoveHeadImpulseToSpine2;

			float m_EqualizeAmount;

			float m_COMImpulseScale;
			u32 m_COMImpulseComponent;
			float m_COMImpulseMaxRootVelocityMagnitude;
			bool m_COMImpulseOnlyWhileBalancing;

			float m_HeadShotImpulseMultiplier;
			float m_HeadShotMPImpulseMultiplier;
			bool m_ScaleHeadShotImpulseWithSpineOrientation;
			float m_MinHeadShotImpulseMultiplier; 

			float m_AutomaticInitialSnapMult;
			float m_BurstFireInitialSnapMult;

			float m_FinalShotImpulseClampMax;

			float m_RunningAgainstBulletImpulseMult;
			float m_RunningAgainstBulletImpulseMultMax;
			float m_RunningWithBulletImpulseMult;

			float m_LegShotFallRootImpulseMinUpright;
			float m_LegShotFallRootImpulseMult;

			SniperImpulses m_SniperImpulses;

			PAR_SIMPLE_PARSABLE;
		};

		Impulses m_Impulses;

		struct HitPointRandomisation
		{
			bool m_Enable;
			float m_TopSpread;
			float m_BottomSpread;
			float m_Blend;
			RagdollComponent m_TopComponent;
			RagdollComponent m_BottomComponent;
			float m_BiasSide;
			u32 m_BiasSideTime;
			
			bool ShouldRandomiseHitPoint(RagdollComponent component) const
			{
				switch(component)
				{
				case RAGDOLL_SPINE0:
				case RAGDOLL_SPINE1:
				case RAGDOLL_SPINE2:
				case RAGDOLL_SPINE3:
					return true;
				default:
					return false;
				}
			}

			bool GetRandomHitPoint(Vector3& vRandomHitPoint, const CPed* pPed, RagdollComponent eComponent, bool bBiasSideLeft) const;

			PAR_SIMPLE_PARSABLE;
		};

		HitPointRandomisation m_HitRandomisation;
		HitPointRandomisation m_HitRandomisationAutomatic;

		struct ArmShot
		{
			ArmShot()
			{}

			int m_MinLookAtArmWoundTime;
			int m_MaxLookAtArmWoundTime;

			float m_UpperArmImpulseCap;
			float m_LowerArmImpulseCap;

			float m_UpperArmNoTorsoHitImpulseCap;
			float m_LowerArmNoTorseHitImpulseCap;

			float m_ClavicleImpulseScale;

			PAR_SIMPLE_PARSABLE;
		};

		struct StayUpright
		{
			StayUpright()
			{}

			float m_HoldingWeaponBonus;
			float m_UnarmedBonus;
			float m_ArmouredBonus;
			float m_MovingMultiplierBonus;
			float m_HealthMultiplierBonus;

			PAR_SIMPLE_PARSABLE;
		};

		StayUpright m_StayUpright;

		ArmShot m_ArmShot;

		float m_FallingSpeedForHighFall;

		float m_ChanceOfFallToKneesOnCollapse;
		float m_ChanceOfFallToKneesAfterLastStand;

		float m_ChanceForGutShotKnockdown;

		float m_LastStandMaxTotalTime;
		float m_LastStandMaxArmouredTotalTime;

		s32 m_RapidHitCount;
		s32 m_ArmouredRapidHitCount;

		bool m_AllowArmouredLegShot;
		bool m_AllowArmouredKnockdown;

		bool m_ReduceDownedTimeByPerformanceTime;
		s32 m_MinimumDownedTime;

		bool m_DisableReachForWoundOnHeadShot;
		s32 m_DisableReachForWoundOnHeadShotMinDelay;
		s32 m_DisableReachForWoundOnHeadShotMaxDelay;

		bool m_DisableReachForWoundOnNeckShot;
		s32 m_DisableReachForWoundOnNeckShotMinDelay;
		s32 m_DisableReachForWoundOnNeckShotMaxDelay;

		CNmTuningSetGroup m_WeaponSets;

		struct ParamSets
		{
			// always sent
			CNmTuningSet m_Base;
			
			// weapon type
			CNmTuningSet m_Melee;
			CNmTuningSet m_Electrocute;

			// hit location
			CNmTuningSet m_SprintingLegShot;
			CNmTuningSet m_SprintingDeath;
			CNmTuningSet m_Sprinting;
			CNmTuningSet m_AutomaticHeadShot;
			CNmTuningSet m_HeadShot;
			CNmTuningSet m_AutomaticNeckShot;
			CNmTuningSet m_NeckShot;
			CNmTuningSet m_LegShot;
			CNmTuningSet m_SniperLegShot;
			CNmTuningSet m_ArmShot;
			CNmTuningSet m_BackShot;

			// target state
			CNmTuningSet m_Underwater;
			CNmTuningSet m_UnderwaterRelax;
			CNmTuningSet m_Armoured;
			CNmTuningSet m_BoundAnkles;
			CNmTuningSet m_FatallyInjured;
			CNmTuningSet m_PlayerDeathSP;
			CNmTuningSet m_PlayerDeathMP;
			CNmTuningSet m_OnStairs;

			// special
			CNmTuningSet m_ShotAgainstWall;
			CNmTuningSet m_LastStand;
			CNmTuningSet m_LastStandArmoured;
			CNmTuningSet m_HeadLook;
			
			CNmTuningSet m_FallToKnees;
			CNmTuningSet m_StaggerFall;
			CNmTuningSet m_CatchFall;
			CNmTuningSet m_SetFallingReactionHealthy;
			CNmTuningSet m_SetFallingReactionInjured;
			CNmTuningSet m_SetFallingReactionFallOverWall;
			CNmTuningSet m_SetFallingReactionFallOverVehicle;
			CNmTuningSet m_RubberBulletKnockdown;
			CNmTuningSet m_Teeter;

			CNmTuningSet m_HoldingTwoHandedWeapon;
			CNmTuningSet m_HoldingSingleHandedWeapon;

			CNmTuningSet m_CrouchedOrLowCover;

			CNmTuningSet m_Female;

			PAR_SIMPLE_PARSABLE;
		};

		ParamSets m_ParamSets;

		CTaskNMBehaviour::Tunables::StandardBlendOutThresholds m_BlendOutThreshold;
		CTaskNMBehaviour::Tunables::BlendOutThreshold m_SubmergedBlendOutThreshold;

		PAR_PARSABLE;
	};

	CTaskNMShot(CPed* pPed, u32 nMinTime, u32 nMaxTime, CEntity* pShotBy, u32 nWeaponHash, int nComponent, const Vector3& vecHitPosWorldSpace, 
		const Vector3& vecHitImpulseWorldSpace, const Vector3& vecHitPolyNormalWorldSpace, s32 m_hitCountThisPerformance = 0, bool bShotgunMessageSent = false, 
		bool bHeldWeapon = false, bool hitPosInLocalSpace = false, u32 lastStandStartTimeMS = 0, u32 lastConfigureBulletsCall = 0, 
		bool lastStandActive = false, bool knockDownStarted = false);
	~CTaskNMShot();

protected:
	CTaskNMShot(const CTaskNMShot& otherTask);

public:
	bool HasBalanceFailed() const { return m_bBalanceFailed; }

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskAndStateName;}
#endif

#if __BANK
	const char* GetShotTypeName() const;
#endif //__BANK

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_SHOT;}

	virtual void CleanUp();
	virtual aiTask* Copy() const { return rage_new CTaskNMShot(*this); }
	static void AppendWeaponSets(CNmTuningSet* newItem, atHashString key)
	{
		sm_Tunables.m_WeaponSets.Append(newItem,key);
	}
	static void RevertWeaponSets(atHashString key)
	{
		sm_Tunables.m_WeaponSets.Revert(key);
	}
	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	void CreateAnimatedFallbackTask(CPed* pPed, bool bFalling);
	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	//////////////////////////
	// Local functions and data
public:
	void UpdateShot(CPed* pThisPed, CEntity* pShotBy, u32 nWeaponHash, int nComponent, const Vector3& vecHitPos, const Vector3& vecHitImpulse, 
		const Vector3& vecHitPolyNormal);

	u32 GetWeaponHash() const	{ return m_nWeaponHash; }
	int GetComponent() const	{ return m_nComponent; }
	CEntity* GetShotBy() const	{ return m_pShotBy; }
	const Vector3 & GetWorldImpulse() const	{ return m_vecOriginalHitImpulseWorldSpace; }
	void GetWorldHitPosition(CPed *pPed, Vector3 & worldHitPos) const;
	bool IsSprinting() const { return m_bSprinting; }

	virtual bool ShouldContinueAfterDeath() const {return true;}
	virtual bool ShouldAbortForWeaponDamage(CEntity* pFiringEntity, const CWeaponInfo* pWeaponInfo, const f32 UNUSED_PARAM(fWeaponDamage), const fwFlags32& UNUSED_PARAM(flags), 
		const bool UNUSED_PARAM(bWasKilledOrInjured), const Vector3& UNUSED_PARAM(vStart), WorldProbe::CShapeTestHitPoint* pResult, 
		const Vector3& vRagdollImpulseDir, const f32 fRagdollImpulseMag) 
	{ 
		// Fire and explosion damage override shot reactions
		if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_FIRE || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{
			return true;
		}
		// Otherwise, if there's a shape test result, continue this shot reaction
		else if ( pResult )
		{
			UpdateShot(GetPed(), pFiringEntity, pWeaponInfo->GetHash(), pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());
			return false;
		}
		// This is some unknown damage source we need to abort for.
		else
		{
			return false;
		}
	}

	virtual bool HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);

protected:

	void HandleNormalShotReaction(CPed* pPed, bool bStart, CNmMessageList& list);
	void HandleShotgunReaction(CPed* pPed, bool bStart, CNmMessageList& list);
	void HandleUnderwaterReaction(CPed* pPed, bool bStart, CNmMessageList& list);
	void HandleBoundAnklesReaction(CPed* pPed, bool bStart, CNmMessageList& list);
	void HandleMeleeReaction(CPed* pPed, bool bStart, CNmMessageList& list);
	void HandleRunningReaction(CPed* pPed, bool bStart, CNmMessageList& list);

	void GetLocalNormal(CPed* pPed, Vector3 &localHitNormal);
	void CheckState(CPed* pPed);
	void SetShotMessage(CPed* pPed, bool bStartBehaviour, CNmMessageList& list);
	void ImpulseOverride(CPed* pPed);
	void GatherSituationalData(CPed* pPed);
	bool CheckForObjectInPath(CPed* pPed, Vector3 &objectBehindVictimPos, Vector3 &objectBehindVictimNormal);
	void CalculateHealth(CPed* pPed);
	void CheckInputNormalAndHitPosition();

	void HandleConfigureBalance(CPed* pPed, CNmMessageList& list);
	void HandleConfigureBullets(CPed* pPed, bool bStart = true);
	void HandlePointGun(CPed* pPed, bool bStart, CNmMessageList& list);
	void HandleApplyBulletImpulse(CPed* pPed, bool bSendNewBulletMessage = true, bool bStart = true, bool doKillShot = false, bool wallInImpulsePath = false);
	void HandleNewBullet(CPed* pPed, Vector3 &posLocal, const Vector3 &normalLocal, const Vector3 &impulseWorld);
	void HandleHeadLook(CPed* pPed, CNmMessageList& list);
	void HandleSnap(CPed* pPed, CNmMessageList& list);

	// Clamps the local hit position to the closest surface point on the bound
	void ClampHitToSurfaceOfBound(CPed* pPed, Vector3 &localPos, int component);

	void StartAnimatedReachForWound();
	bool ShouldReachForWound(bool& withLeft);

	bool IsArmoured(CPed* pPed)
	{
		return pPed && (pPed->GetArmour()>0.0f);
	}

	float GetArmouredRatio(CPed* pPed)
	{
		float armour = 0.0f;
		if (pPed)
		{
			armour = pPed->GetArmour() + pPed->GetArmourLost();
			if (armour>0.0f)
			{
				armour = pPed->GetArmour() / armour;
			}
			armour = ClampRange(armour, 0.0f, 1.0f);
		}
		return armour;
	} 

	float GetHealthRatio(CPed* pPed)
	{
		float healthRatio = 0.0f;
		if (pPed)
		{
			healthRatio = pPed->GetMaxHealth() - pPed->GetInjuredHealthThreshold();
			if (healthRatio>0.0f)
			{
				healthRatio = 1.0f - (pPed->GetHealthLost() / healthRatio);
			}
			healthRatio = ClampRange(healthRatio, 0.0f, 1.0f);
		}
		return healthRatio;
	}

	bool IsCloneDying(const CPed* pPed) const;

	bool ShouldBlindFireWeapon(CPed* pPed);
	bool ShouldBlindFireWeaponNetworkCheck(CPed* pPed, Vector3 const& vecStart, Vector3 const& vecEnd);

	bool IsAutomatic(const atHashString& shotTuningSet) const
	{
		if (shotTuningSet == ATSTRINGHASH("Automatic", 0x74D3AC3B) || 
			shotTuningSet == ATSTRINGHASH("AutomaticMG", 0xBD26D537) ||
			shotTuningSet == ATSTRINGHASH("AutomaticSMG", 0x7183C85F) ||
			shotTuningSet == ATSTRINGHASH("Mini", 0x1289C7C8) ||
			shotTuningSet == ATSTRINGHASH("BurstFire", 0xC48A0A41))
		{
			return true;
		}

		return false;
	}

private:
	enum eStates // for internal state machine.
	{
		State_STANDING,
		State_STAGGERING,
		State_FALLING,
		State_ON_GROUND,
		State_PARTIALLY_SUBMERGED,	// In water
		State_SUBMERGED,			// In water
		State_TEETERING
	};
	eStates m_eState;

	u32 m_eType;

private:
	CThreatenedHelper m_threatenedHelper;

	WorldProbe::CShapeTestResults m_probeResult;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskAndStateName;
#endif // !__FINAL

	Vec3V	m_vecOriginalHitPositionBoneSpace; // store the local space position of the wound for later use in animated reach for wound.

	Vector3 m_vecOriginalHitPositionLocalSpace;  
	Vector3	m_vecOriginalHitPolyNormalWorldSpace;
	Vector3 m_vecOriginalHitImpulseWorldSpace; 

	Vector3 m_vEdgeLeft, m_vEdgeRight;
	bool m_FirstTeeterProbeDetectedDrop;

	RegdEnt m_pShotBy;
	int m_nComponent;
	u32 m_nWeaponHash;

	u32 m_LastConfigureBulletsCall;

	u32 m_BiasSideTime;

	float m_healthRatio;

	u32 m_nTimeToStopBlindFire;
	u32 m_nTimeTillBlindFireNextShot;
	u32 m_LastStandStartTimeMS;
	u32 m_DisableReachForWoundTimeMS;

	u16 m_nLookAtArmWoundTime;
	u16 m_CountdownToCatchfall;

	s16 m_hitCountThisPerformance;

	u8 m_ThrownOutImpulseCount;
	u8 m_ShotsPerBurst;

	bool m_bUsingBalance : 1;
	bool m_bBalanceFailed : 1;
	bool m_bUseHeadLook : 1;
	bool m_bUsePointGun : 1;
	bool m_StaggerFallStarted : 1;
	bool m_bSprinting : 1;

	bool m_bFront : 1;
	bool m_bUpright : 1;
	bool m_RapidFiring : 1;
	bool m_RapidFiringBoostFrame : 1;

	bool m_ElectricDamage : 1;
	bool m_SettingShotMessageForShotgun : 1;
	bool m_ShotgunMessageSent : 1;
	bool m_HeldWeapon : 1;
	bool m_bMinTimeExtended : 1;
	bool m_stepInInSupportApplied : 1;
	bool m_bLastStandActive : 1;
	bool m_bKnockDownStarted : 1;
	bool m_bKillShotUsed : 1;
	bool m_bFallingToKnees : 1;
	bool m_ReachingWithLeft : 1;
	bool m_ReachingWithRight : 1;
	bool m_DoLegShotFall : 1;
	bool m_FallingOverWall : 1;
	bool m_bTeeterInitd : 1;
	bool m_BiasSide : 1;
	bool m_WasCrouchedOrInLowCover : 1;

#if __DEV
	bool m_selected : 1;
#endif

	// tunable by widgets
public:

	void ChooseParameterSet();

	static bool IsPoseAppropriateForFiring(CPed* pFiringPed, CAITarget& target, CWeapon* pWeapon, CObject* pWeaponObject);

#if !__FINAL
#if DEBUG_DRAW
	void DebugVisualise (const CPed* pPed) const; // Put here so that it can be called while the game is paused.
#endif // DEBUG_DRAW
	virtual void Debug() const
	{
#if DEBUG_DRAW
		// Visualise the hit point and impact vector for debugging.
		if(sm_Tunables.m_bEnableDebugDraw)
		{
			DebugVisualise(GetPed());
		}
#endif // DEBUG_DRAW
		CTask::Debug();
	}
#endif // !__FINAL

	static Tunables	sm_Tunables;
};

#endif // ! INC_TASKNMSHOT_H_
