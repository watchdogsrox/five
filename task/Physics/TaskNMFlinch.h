// Filename   :	TaskNMFlinch.h
// Description:	Natural Motion flinch class (FSM version)

#ifndef INC_TASKNMFLINCH_H_
#define INC_TASKNMFLINCH_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task CTaskNMFlinch
//
class CTaskNMFlinch : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		//////////////////////////////////////////////////////////////////////////
		// start of behaviour tuning sets
		//////////////////////////////////////////////////////////////////////////

		// base types
		CNmTuningSet m_Start;
		CNmTuningSet m_Passive;
		CNmTuningSet m_WaterCannon;

		// hit target types
		CNmTuningSet m_Armoured;
		CNmTuningSet m_BoundAnkles;
		CNmTuningSet m_FatallyInjured;
		CNmTuningSet m_PlayerDeath;
		CNmTuningSet m_OnStairs;
		CNmTuningSet m_HoldingTwoHandedWeapon;
		CNmTuningSet m_HoldingSingleHandedWeapon;

		// per weapon tuning
		CNmTuningSetGroup m_WeaponSets;

		// per melee move tuning
		CNmTuningSetGroup m_ActionSets;


		//////////////////////////////////////////////////////////////////////////
		// Update tuning sets
		//////////////////////////////////////////////////////////////////////////
		CNmTuningSet m_Update;

		CNmTuningSet m_OnBalanceFailed;
		CNmTuningSet m_OnBalanceFailedStairs;

		//////////////////////////////////////////////////////////////////////////
		// Generic tuning
		//////////////////////////////////////////////////////////////////////////

		bool m_RandomiseLeadingHand;
		u32 m_MinLeanInDirectionTime;
		u32 m_MaxLeanInDirectionTime;

		float m_fImpulseReductionScaleMax;
		float m_fSpecialAbilityRageKickImpulseModifier;
		float m_fCounterImpulseScale;

		PAR_PARSABLE;
	};


	enum eFlinchType {
		FLINCHTYPE_MELEE,
		FLINCHTYPE_MELEE_PASSIVE,
		FLINCHTYPE_WATER_CANNON,

		NUM_FLINCH_TYPES,
	};

	CTaskNMFlinch(u32 nMinTime, u32 nMaxTime, const Vector3& flinchPos, CEntity* pFlinchEntity = NULL, eFlinchType nFlinchType = FLINCHTYPE_MELEE, u32 nWeaponHash = 0, 
		s32 nHitComponent = -1, bool bLocalHitPointInfo = false, const CPed* pPed = NULL, const Vector3* vHitLocation = NULL, const Vector3* vHitNormal = NULL, const Vector3* vHitImpulse = NULL);
	~CTaskNMFlinch();
protected:
	CTaskNMFlinch(const CTaskNMFlinch& otherTask);
public:


	void SetType(eFlinchType eType) { m_nFlinchType = eType; };
	eFlinchType GetType() const { return m_nFlinchType; }

	bool HasBalanceFailed() const { return m_bBalanceFailed; }

	void UpdateFlinch(CPed* pPed, const Vector3& flinchPos, CEntity* pFlinchEntity = NULL, eFlinchType nFlinchType = FLINCHTYPE_MELEE, u32 nWeaponHash = 0, 
		s32 nHitComponent = -1, bool bLocalHitPointInfo = false, const Vector3* vHitLocation = NULL, const Vector3* vHitNormal = NULL, const Vector3* vHitImpulse = NULL);
	
	static void AppendWeaponSet(CNmTuningSet* newItem, atHashString key)
	{
		sm_Tunables.m_WeaponSets.Append(newItem, key);
	}
	
	static void AppendActionSet(CNmTuningSet* newItem, atHashString key)
	{
		sm_Tunables.m_ActionSets.Append(newItem, key);
	}

	static void RevertWeaponSet(atHashString key)
	{
		sm_Tunables.m_WeaponSets.Revert(key);
	}

	static void RevertActionSet(atHashString key)
	{
		sm_Tunables.m_ActionSets.Revert(key);
	}
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMFlinch");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMFlinch(*this);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_FLINCH;}

	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

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

	Vector3		GetFlinchPos() const	{ return m_vecFlinchPos; }
	CEntity*	GetFlinchEntity() const { return m_pFlinchEntity; }

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	//////////////////////////
	// Local functions and data
private:
	Vector3 GetFlinchPos(CPed* pPed) const;

	void CalculateHitPointInfo(const CPed* pPed, const Vector3* vHitLocation, const Vector3* vHitNormal, const Vector3* vHitImpulse, bool bLocalHitPointInfo);

	void SetFlinchMessage(CPed* pPed, bool bStartBehaviour);
	
	eFlinchType m_nFlinchType;
	RegdEnt	m_pFlinchEntity;
	Vector3		m_vecFlinchPos;
	bool		m_bBalanceFailed;
	bool		m_bRollingDownStairs;
	u32			m_nTimeToStopLean;

	u32			m_nWeaponHash;
	s32			m_nHitComponent;
	Vector3		m_vLocalHitLocation;
	Vector3		m_vWorldHitNormal;
	Vector3		m_vLocalHitImpulse;
	Vector3		m_vWorldHitImpulse;

public:

	static const char* ms_TypeToString[NUM_FLINCH_TYPES];

	static Tunables sm_Tunables;
#if __DEV && DEBUG_DRAW
	static bool sm_bDebugDraw;
#endif
};


//
// Task info for CTaskNMFlinch
//
class CClonedNMFlinchInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMFlinchInfo(const Vector3& flinchPos, CEntity* pFlinchEntity, u32 nFlinchType, u32 nWeaponHash, int nComponent, const Vector3& flinchLocation, const Vector3& flinchNormal, const Vector3& flinchImpulse);
	CClonedNMFlinchInfo();
	~CClonedNMFlinchInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_FLINCH;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_flinchPos, "Flinch position");

		bool bHasFlinchImpulse = m_flinchImpulse.Mag2() > 0.01f;
		bool bHasFlinchLocation = m_flinchLocation.Mag2() > 0.01f;
		bool bHasFlinchNormal = m_flinchNormal.Mag2() >= square(0.99f);

		SERIALISE_BOOL(serialiser, bHasFlinchImpulse);
		SERIALISE_BOOL(serialiser, bHasFlinchLocation);
		SERIALISE_BOOL(serialiser, bHasFlinchNormal);

		if (bHasFlinchImpulse || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_flinchImpulse, 250.0f, SIZEOF_HITFORCE, "Hit world impulse");
		}
		else
		{
			m_flinchImpulse = Vector3::ZeroType;
		}

		if (bHasFlinchLocation || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_flinchLocation, LOCAL_POS_MAG_LIMIT, SIZEOF_HITPOS, "Hit local pos");
		}
		else
		{
			m_flinchLocation = Vector3::ZeroType;
		}

		if (bHasFlinchNormal || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_flinchNormal, 1.1f, SIZEOF_HITPOLYNORMAL, "Hit world poly normal");
		}
		else
		{
			m_flinchNormal = Vector3::ZeroType;
		}

		SERIALISE_BOOL(serialiser, m_bHasFlinchEntity);

		if (m_bHasFlinchEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_flinchEntity, "Flinch entity");
		}

		SERIALISE_UNSIGNED(serialiser, m_nWeaponHash, SIZEOF_WEAPONHASH, "Weapon hash");

		bool bHasComponent = m_nComponent != -1;

		SERIALISE_BOOL(serialiser, bHasComponent);

		if (bHasComponent || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 component = (u32)m_nComponent;
			SERIALISE_UNSIGNED(serialiser, component, SIZEOF_COMPONENT, "Component");
			m_nComponent = (int)component;
		}
		else
		{
			m_nComponent = -1;
		}

		SERIALISE_UNSIGNED(serialiser, m_flinchType, SIZEOF_FLINCH_TYPE, "Flinch type");
	}

private:

	CClonedNMFlinchInfo(const CClonedNMFlinchInfo &);
	CClonedNMFlinchInfo &operator=(const CClonedNMFlinchInfo &);

	static const unsigned int SIZEOF_FLINCH_TYPE	= datBitsNeeded<CTaskNMFlinch::NUM_FLINCH_TYPES>::COUNT;
	static const unsigned int SIZEOF_WEAPONHASH		= 32;
	static const unsigned int SIZEOF_COMPONENT		= 8;
	static const unsigned int SIZEOF_HITPOS			= 16;
	static const unsigned int SIZEOF_HITPOLYNORMAL	= 16;
	static const unsigned int SIZEOF_HITFORCE		= 20;

	Vector3     m_flinchPos;
	Vector3		m_flinchImpulse;
	Vector3		m_flinchLocation;
	Vector3		m_flinchNormal;
	ObjectId    m_flinchEntity;
	u32	    	m_flinchType;
	u32			m_nWeaponHash;
	int			m_nComponent;
	bool		m_bHasFlinchEntity;
};


#endif // ! INC_TASKNMFLINCH_H_
