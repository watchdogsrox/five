// Filename   :	TaskNMOnFire.h
// Description:	Natural Motion on fire class (FSM version)

#ifndef INC_TASKNMONFIRE_H_
#define INC_TASKNMONFIRE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMOnFire
//
class CClonedNMOnFireInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMOnFireInfo(float fLeanAmount, const Vector3* pVecLeanTarget, u32 onFireType);
	CClonedNMOnFireInfo();
	~CClonedNMOnFireInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_ONFIRE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fLeanAmount,  1.0f, SIZEOF_LEAN_AMOUNT, "Lean amount");

		SERIALISE_BOOL(serialiser, m_bHasLeanTarget, "Has lean target");

		if (m_bHasLeanTarget || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_vecLeanTarget, 1.0f, SIZEOF_LEAN_TARGET, "Lean target");
		}

		SERIALISE_UNSIGNED(serialiser, m_onFireType, SIZEOF_ONFIRE_TYPE, "On fire type");
	}

private:

	CClonedNMOnFireInfo(const CClonedNMOnFireInfo &);
	CClonedNMOnFireInfo &operator=(const CClonedNMOnFireInfo &);

	static const unsigned int SIZEOF_LEAN_TARGET	= 16;
	static const unsigned int SIZEOF_LEAN_AMOUNT	= 16;
	static const unsigned int SIZEOF_ONFIRE_TYPE	= 1;

	Vector3							m_vecLeanTarget; 
	float							m_fLeanAmount; 
	u32							m_onFireType;
	bool							m_bHasLeanTarget;
};


//
// Task CTaskNMOnFire
//
class CTaskNMOnFire : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		CNmTuningSet m_Start;
		CNmTuningSet m_Weak;

		CNmTuningSet m_Update;
						
		CNmTuningSet m_OnBalanceFailed;

		PAR_PARSABLE;
	};

	enum eOnFireType {
		ONFIRE_DEFAULT,
		ONFIRE_WEAK,

		NUM_ONFIRE_TYPES
	};

	CTaskNMOnFire(u32 nMinTime, u32 nMaxTime, float fLeanAmount=0.0f, const Vector3* pVecLeanTarget=NULL, eOnFireType eType = ONFIRE_DEFAULT);
	~CTaskNMOnFire();

	void SetType(eOnFireType eType) { m_eType = eType; };
	eOnFireType GetType() const { return m_eType; }
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMOnFire");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMOnFire(m_nMinTime, m_nMaxTime, 0.0f, NULL, m_eType);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_ONFIRE;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMOnFireInfo(0.0f,  NULL , static_cast<u32>(m_eType));
	}

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

	virtual bool ShouldAbortForWeaponDamage(CEntity* UNUSED_PARAM(pFiringEntity), const CWeaponInfo* UNUSED_PARAM(pWeaponInfo), const f32 UNUSED_PARAM(fWeaponDamage), const fwFlags32& UNUSED_PARAM(flags), 
		const bool bWasKilledOrInjured, const Vector3& UNUSED_PARAM(vStart), WorldProbe::CShapeTestHitPoint* UNUSED_PARAM(pResult), 
		const Vector3& UNUSED_PARAM(vRagdollImpulseDir), const f32 UNUSED_PARAM(fRagdollImpulseMag)) 
	{ 
		// Only exit if the ped was killed
		return bWasKilledOrInjured;
	}
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	void CreateAnimatedFallbackTask(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	//////////////////////////
	// Local functions and data
private:
	class CWritheClipSetRequestHelper* m_pWritheClipSetRequestHelper;
	bool m_bBalanceFailed;
	eOnFireType m_eType;

public:

	static Tunables sm_Tunables;

	static const char* ms_TypeToString[NUM_ONFIRE_TYPES];
};

#endif // ! INC_TASKNMONFIRE_H_
