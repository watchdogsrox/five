// Filename   :	TaskNMBrace.h
// Description:	Natural Motion brace class (FSM version)

#ifndef INC_TASKNMBRACE_H_
#define INC_TASKNMBRACE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Peds\ped.h"

#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/ThreatenedHelper.h"
#include "Network/General/NetworkUtil.h"
// ------------------------------------------------------------------------------

//
// Task CTaskNMBrace
//
class CTaskNMBrace : public CTaskNMBehaviour
{
public:
	enum eBraceType
	{
		BRACE_DEFAULT,
		BRACE_WEAK,
		BRACE_SIDE_SWIPE,
		BRACE_CAPSULE_HIT,

		NUM_BRACE_TYPES
	};

	struct Tunables : CTuning
	{
		Tunables();

		struct VelocityLimits
		{
			bool m_Apply;
			Vector3 m_Constant;
			Vector3 m_Velocity;
			Vector3 m_Velocity2;
			float m_Max;
			u32	m_Delay;

			PAR_SIMPLE_PARSABLE;
		};

		struct InitialForce
		{
			float m_VelocityMin;
			float m_VelocityMax;
			float m_ForceAtMinVelocity;
			float m_ForceAtMaxVelocity;
			bool m_ScaleWithUpright;

			PAR_SIMPLE_PARSABLE;
		};

		struct ApplyForce
		{
			
			bool ShouldApply(u32 startTime, bool inContact);

			// Pass in a normalised direction vector for the force.
			void GetForceVec(Vec3V_InOut forceVec, Vec3V_In velVec, const CPhysical* pPhys, const CPed* pHitPed);

			bool m_Apply;

			bool m_ScaleWithVelocity;
			float m_MinVelThreshold;
			float m_MaxVelThreshold;
			float m_MinVelMag;
			float m_MaxVelMag;

			bool m_ScaleWithMass;
			bool m_ScaleWithUpright;
			bool m_OnlyInContact;
			bool m_OnlyNotInContact;
			bool m_ReduceWithPedVelocity;
			bool m_ReduceWithPedAngularVelocity;
			float m_ForceMag;
			float m_MinMag;
			float m_MaxMag;
			u32 m_Duration;

			PAR_SIMPLE_PARSABLE;
		};

		struct StuckOnVehicle
		{
			float m_VelocityThreshold;
			u32 m_InitialDelay;
			float m_ContinuousContactTime;
			float m_UnderVehicleVelocityThreshold;
			u32 m_UnderVehicleInitialDelay;
			float m_UnderVehicleContinuousContactTime;
			float m_UnderCarMaxVelocity;

			CNmTuningSet m_StuckOnVehicle;
			CNmTuningSet m_EndStuckOnVehicle;
			CNmTuningSet m_UpdateOnVehicle;
			CNmTuningSet m_StuckUnderVehicle;
			CNmTuningSet m_EndStuckUnderVehicle;

			CNmTuningSet m_StuckOnVehiclePlayer;
			CNmTuningSet m_EndStuckOnVehiclePlayer;
			CNmTuningSet m_UpdateOnVehiclePlayer;
			CNmTuningSet m_StuckUnderVehiclePlayer;
			CNmTuningSet m_EndStuckUnderVehiclePlayer;

			PAR_SIMPLE_PARSABLE;
		};

		struct VehicleTypeOverrides
		{
			atHashString m_Id;

			bool m_OverrideInverseMassScales;

			CTaskNMBehaviour::Tunables::InverseMassScales m_InverseMassScales;

			ApplyForce m_LateralForce;

			bool m_OverrideReactionType;
			bool m_ForceUnderVehicle;
			bool m_ForceOverVehicle;
			float m_VehicleCentreZOffset;

			bool m_OverrideRootLiftForce;
			ApplyForce m_RootLiftForce;

			bool m_OverrideFlipForce;
			ApplyForce m_FlipForce;			

			bool m_OverrideInitialForce;
			InitialForce m_InitialForce;

			bool m_OverrideElasticity;
			float m_VehicleCollisionElasticityMult;
			bool m_OverrideFriction;
			float m_VehicleCollisionFrictionMult;

			bool m_OverrideStuckOnVehicleSets;
			bool m_AddToBaseStuckOnVehicleSets;
			StuckOnVehicle m_StuckOnVehicle;

			PAR_SIMPLE_PARSABLE;
		};

		class VehicleTypeTunables
		{
		public:
			void Append(VehicleTypeOverrides& newItem);
			void Revert(VehicleTypeOverrides& newItem);

			VehicleTypeOverrides* Get(CVehicle* pVeh)
			{
				if (!pVeh)
					return NULL;
				CVehicleModelInfo* pModelInfo = pVeh->GetVehicleModelInfo();
				if (!pModelInfo)
					return NULL;

				atHashString setName = pModelInfo->GetNmBraceOverrideSet();

				return Get(setName);
			}

			VehicleTypeOverrides* Get(const atHashString& setName)
			{
				for (s32 i=0; i<m_sets.GetCount(); i++)
				{
					if (m_sets[i].m_Id.GetHash()==setName.GetHash())
					{
						return &m_sets[i];
					}
				}
				return NULL;
			}
		private:
			atArray< VehicleTypeOverrides > m_sets;
			
			PAR_SIMPLE_PARSABLE;
		};

		VehicleTypeTunables m_VehicleOverrides;

		CTaskNMBehaviour::Tunables::InverseMassScales m_InverseMassScales;

		ApplyForce m_CapsuleHitForce;
		ApplyForce m_SideSwipeForce;

		ApplyForce m_ChestForce;
		ApplyForce m_FeetLiftForce;
		ApplyForce m_RootLiftForce;
		ApplyForce m_FlipForce;
		float m_ChestForcePitch;

		bool m_ForceUnderVehicle;
		bool m_ForceOverVehicle;

		InitialForce m_InitialForce;

		VelocityLimits m_AngularVelocityLimits;

		bool m_AllowWarningActivations;

		float m_LowVelocityReactionThreshold;
		float m_FallingSpeedForHighFall;

		float m_VehicleCollisionElasticityMult;
		float m_VehicleCollisionFrictionMult;
		float m_VehicleCollisionNormalPitchOverVehicle;
		float m_VehicleCollisionNormalPitchUnderVehicle;

		StuckOnVehicle m_StuckOnVehicle;

		float m_StuckUnderVehicleMaxUpright;

		u32 m_AiClearedVehicleDelay;
		u32 m_AiClearedVehicleSmartFallDelay;

		u32 m_PlayerClearedVehicleDelay;
		u32 m_PlayerClearedVehicleSmartFallDelay;

		CNmTuningSet m_Start;
		CNmTuningSet m_OnStairs;
		CNmTuningSet m_Weak;

		CNmTuningSet m_OnBalanceFailed;
		CNmTuningSet m_OnBalanceFailedStairs;

		CNmTuningSet m_HighVelocity;

		CNmTuningSet m_Update;
		CNmTuningSet m_Dead;

		CNmTuningSet m_OverVehicle;
		CNmTuningSet m_UnderVehicle;

		CNmTuningSet m_ClearedVehicle;

		CTaskNMBehaviour::Tunables::StandardBlendOutThresholds m_HighVelocityBlendOut;

		PAR_PARSABLE;
	};

	CTaskNMBrace(u32 nMinTime, u32 nMaxTime, CEntity* pBraceEntity, eBraceType eType = BRACE_DEFAULT, const Vector3& vInitialPedVelocity = VEC3_ZERO);
	~CTaskNMBrace();
protected:
	CTaskNMBrace(const CTaskNMBrace& otherTask);
public:
	
	static void AppendVehicleOverrides(Tunables::VehicleTypeOverrides& newItem)
	{
		sm_Tunables.m_VehicleOverrides.Append(newItem);
	}
	static void RevertVehicleOverrides(Tunables::VehicleTypeOverrides& newItem)
	{
		sm_Tunables.m_VehicleOverrides.Revert(newItem);
	}
	void SetType(eBraceType eType) { m_eType = eType; };
	eBraceType GetType() const { return m_eType; }

	void SetSideSwipeImpulsePos(Vec3V_In worldPos, const CPed* pPed);

	bool HasBalanceFailed() const { return m_bBalanceFailed; }
	virtual bool HandlesDeadPed();

	virtual void Cleanup();

	////////////////////////
	// CTaskSimple functions
#if !__NO_OUTPUT
	virtual atString GetName() const {return atString("CTaskNMBrace");}
	virtual void Debug() const;
#endif

	virtual aiTask* Copy() const {
		return rage_new CTaskNMBrace(*this);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_BRACE;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	//////////////////////////
	// Local functions and data
public:
	CEntity* GetBraceEntity() const { return m_pBraceEntity; }

	virtual bool ShouldContinueAfterDeath() const {return true;}
	virtual bool ShouldAbortForWeaponDamage(CEntity* UNUSED_PARAM(pFiringEntity), const CWeaponInfo* pWeaponInfo, const f32 UNUSED_PARAM(fWeaponDamage), const fwFlags32& UNUSED_PARAM(flags), 
		const bool UNUSED_PARAM(bWasKilledOrInjured), const Vector3& UNUSED_PARAM(vStart), WorldProbe::CShapeTestHitPoint* UNUSED_PARAM(pResult), 
		const Vector3& UNUSED_PARAM(vRagdollImpulseDir), const f32 UNUSED_PARAM(fRagdollImpulseMag)) 
	{ 
		// Don't abort high fall for unknown damage, even when dead (because we're going to continue running after death)
		if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_UNKNOWN)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	virtual bool HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);

	inline float GetNormalPitch(){ 
		if (m_GoUnderVehicle)
			return sm_Tunables.m_VehicleCollisionNormalPitchUnderVehicle;
		else if (m_GoOverVehicle)
			return sm_Tunables.m_VehicleCollisionNormalPitchOverVehicle;
		else
			return 0.0f;
	}

	bool IsUsingUnderVehicleReaction() const { return m_GoUnderVehicle; }

	float GetCollisionFrictionMult() { return m_pOverrides && m_pOverrides->m_OverrideFriction ? m_pOverrides->m_VehicleCollisionFrictionMult : sm_Tunables.m_VehicleCollisionFrictionMult; }
	float GetCollisionElasticityMult() { return m_pOverrides && m_pOverrides->m_OverrideElasticity ? m_pOverrides->m_VehicleCollisionElasticityMult : sm_Tunables.m_VehicleCollisionElasticityMult; }

	static inline bool ExcludeComponentForVehicleTrap(s32 ragdollComponent)
	{
		return ragdollComponent == RAGDOLL_SHIN_LEFT
			|| ragdollComponent == RAGDOLL_FOOT_LEFT
			|| ragdollComponent == RAGDOLL_SHIN_RIGHT
			|| ragdollComponent == RAGDOLL_FOOT_RIGHT
			|| ragdollComponent == RAGDOLL_LOWER_ARM_RIGHT
			|| ragdollComponent == RAGDOLL_HAND_RIGHT
			|| ragdollComponent == RAGDOLL_LOWER_ARM_LEFT
			|| ragdollComponent == RAGDOLL_HAND_LEFT;
	}

	static bool ShouldUseUnderVehicleReaction(CPed* pHitPed, CEntity* pBraceEntity);

	static void ApplyInverseMassScales(phContactIterator& impacts, CVehicle* pVeh);

private:
	
	CTaskFallOver::eFallDirection PickCarHitDirection(CPed* pPed);

	void UpdateBracePosition(CPed* pPed);

	bool IsStuckUnderVehicle(CPed* pPed, CVehicle* pVehicle);

	void StartStuckOnVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet);
	void UpdateStuckOnVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet);
	void EndStuckOnVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet);
	
	void StartStuckUnderVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet);
	void EndStuckUnderVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet);

	CThreatenedHelper m_threatenedHelper;
	RegdEnt m_pBraceEntity;
	eBraceType m_eType;
	bool m_bBalanceFailed : 1;
	bool m_bRollingDownStairs : 1;
	bool m_HasStartedCatchfall : 1;
	bool m_StuckOnVehicle : 1; //are we currently stuck on the vehicle
	bool m_StuckUnderVehicle : 1;	// are we currently stuck under the vehicle
	bool m_bHighVelocityReaction : 1;
	bool m_GoUnderVehicle : 1;	// are we attempting to push the ped under the vehicle.
	bool m_GoOverVehicle : 1;	// are we attempting to push the ped over the vehicle.
	bool m_ClearedVehicle : 1;

	float m_InitialMaxAngSpeed;

	u32 m_LastHitCarTime;

	Tunables::VehicleTypeOverrides* m_pOverrides;

	Vec3V m_LocalSideSwipeImpulsePos;
	Vec3V m_InitialPedVelocity;

	// tunable by widgets
public:

	static const char* ms_TypeToString[NUM_BRACE_TYPES];
	static dev_u32 snBracePlayerTimeoutBeforeVelCheck;
	static dev_u32 snBraceTimeoutBeforeVelCheckForCatchfall;

	static Tunables	sm_Tunables;
};

//
// Task info for CTaskNMBrace
//
class CClonedNMBraceInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMBraceInfo(CEntity* pBraceEntity, u32 braceType);
	CClonedNMBraceInfo();
	~CClonedNMBraceInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_BRACE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool hasBraceEntity = m_bHasBraceEntity;
		bool hasBraceType	= m_bHasBraceType;

		SERIALISE_BOOL(serialiser, hasBraceEntity);

		if (hasBraceEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_braceEntity, "Brace entity");
		}

		SERIALISE_BOOL(serialiser, hasBraceType);

		if (hasBraceType || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 braceType = (u32)m_braceType;
			SERIALISE_UNSIGNED(serialiser, braceType, SIZEOF_BRACE_TYPE, "Brace type");
			m_braceType = (u8) braceType;
		}

		m_bHasBraceEntity	= hasBraceEntity;
		m_bHasBraceType		= hasBraceType;
	}

private:

	CClonedNMBraceInfo(const CClonedNMBraceInfo &);
	CClonedNMBraceInfo &operator=(const CClonedNMBraceInfo &);

	static const unsigned int SIZEOF_BRACE_TYPE	= datBitsNeeded<CTaskNMBrace::NUM_BRACE_TYPES>::COUNT;

	ObjectId	m_braceEntity;
	u8			m_braceType;
	bool		m_bHasBraceEntity : 1;
	bool		m_bHasBraceType : 1;
};

#endif // ! INC_TASKNMBRACE_H_
