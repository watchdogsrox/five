// Filename   :	TaskNMThroughWindscreen.h
// Description:	Natural Motion through windscreen class (FSM version)

#ifndef INC_TASKNMTHROUGHWINDSCREEN_H_
#define INC_TASKNMTHROUGHWINDSCREEN_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMThroughWindscreen
//
class CClonedNMThroughWindscreenInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMThroughWindscreenInfo(const Vector3 &initVelocity, bool extraGravity) { m_InitVelocity = initVelocity; m_bExtraGravity = extraGravity; }
	CClonedNMThroughWindscreenInfo() { m_bExtraGravity = false; }
	~CClonedNMThroughWindscreenInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_THROUGH_WINDSCREEN;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

        static const float    MAX_VELOCITY_COMPONENT    = 150.0f;
        static const unsigned SIZEOF_VELOCITY_COMPONENT = 16;

        SERIALISE_PACKED_FLOAT(serialiser, m_InitVelocity.x, MAX_VELOCITY_COMPONENT, SIZEOF_VELOCITY_COMPONENT, "Initial Velocity X");
        SERIALISE_PACKED_FLOAT(serialiser, m_InitVelocity.y, MAX_VELOCITY_COMPONENT, SIZEOF_VELOCITY_COMPONENT, "Initial Velocity Y");
        SERIALISE_PACKED_FLOAT(serialiser, m_InitVelocity.z, MAX_VELOCITY_COMPONENT, SIZEOF_VELOCITY_COMPONENT, "Initial Velocity Z");

		SERIALISE_BOOL(serialiser, m_bExtraGravity, "Using extra gravity to avoid high pops");
	}

private:

	CClonedNMThroughWindscreenInfo(const CClonedNMThroughWindscreenInfo &);
	CClonedNMThroughWindscreenInfo &operator=(const CClonedNMThroughWindscreenInfo &);

    Vector3 m_InitVelocity;
	bool    m_bExtraGravity;
};

//
// Task CTaskNMThroughWindscreen
//
class CTaskNMThroughWindscreen : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		// Defines the min and max time to spend in the stunned state at the end of the behaviour if the ped survives.
		float	m_GravityScale;
		float	m_StartForceDownHeight;

		CTaskNMBehaviour::Tunables::InverseMassScales m_DefaultInverseMassScales;
		CTaskNMBehaviour::Tunables::InverseMassScales m_BicycleInverseMassScales;
		CTaskNMBehaviour::Tunables::InverseMassScales m_BikeInverseMassScales;

		u32 m_ClearVehicleTimeMS;

		float m_KnockOffBikeForwardMinComponent;
		float m_KnockOffBikeForwardMaxComponent;
		float m_KnockOffBikeUpMinComponent;
		float m_KnockOffBikeUpMaxComponent;
		float m_KnockOffBikePitchMinComponent;
		float m_KnockOffBikePitchMaxComponent;
		float m_KnockOffBikeMinSpeed;
		float m_KnockOffBikeMaxSpeed;
		float m_KnockOffBikeMinUpright;
		float m_KnockOffBikeMaxUpright;

		float m_KnockOffBikeEjectMaxImpactDepth;
		float m_KnockOffBikeEjectImpactFriction;

		CNmTuningSet m_Start;
		CNmTuningSet m_Update;

		CNmTuningSet m_StartBrace;
		CNmTuningSet m_StartFalling;
		CNmTuningSet m_StartRolling;
		CNmTuningSet m_StartCatchFall;

		PAR_PARSABLE;
	};

	CTaskNMThroughWindscreen(u32 nMinTime, u32 nMaxTime, bool extraGravity, CVehicle* pVehicle);
	~CTaskNMThroughWindscreen();

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMThroughWindscreen");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMThroughWindscreen(m_nMinTime, m_nMaxTime, m_ExtraGravity, m_pVehicle);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_THROUGH_WINDSCREEN;}
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);

	virtual bool ShouldContinueAfterDeath() const { return true; }
	virtual bool HandlesDeadPed(){ return true; }

	CVehicle* GetEjectVehicle() { return m_pVehicle; }

	u32 GetStartTime(){ return m_nStartTime; }

	bool GetAllowDisablingContacts() { return m_bAllowDisablingContacts; }

    void SetInitialVelocity(const Vector3 &vecInitialVelocity) { m_vecInitialVelocity = vecInitialVelocity; }

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMThroughWindscreenInfo(m_vecInitialVelocity, m_ExtraGravity);
	}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	//////////////////////////
	// Local functions and data
private:

    void UpdateClonePedInitialVelocity(CPed* pPed);

	Vector3 m_vecInitialPosition;
    Vector3 m_vecInitialVelocity;
	bool	m_bBehaviourTriggered;
	bool	m_bCatchfallTriggered;
	bool	m_ExtraGravity;
	bool	m_bAllowDisablingContacts;

	RegdVeh m_pVehicle;

public:

	static Tunables	sm_Tunables;

};

#endif // ! INC_TASKNMTHROUGHWINDSCREEN_H_
