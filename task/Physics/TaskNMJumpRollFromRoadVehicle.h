// Filename   :	TaskNMJumpRollFromRoadVehicle.h
// Description:	Natural Motion jump and roll from a road vehicle class (FSM version)

#ifndef INC_TASKNMJUMPROLLFROMROADVEHICLE_H_
#define INC_TASKNMJUMPROLLFROMROADVEHICLE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMJumpRollFromRoadVehicle
//
class CClonedNMJumpRollInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMJumpRollInfo(bool extraGravity) { m_bExtraGravity = extraGravity; }
	CClonedNMJumpRollInfo() { m_bExtraGravity = false; }
	~CClonedNMJumpRollInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_JUMPROLL;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bExtraGravity, "Using extra gravity to avoid high pops");
	}

private:

	CClonedNMJumpRollInfo(const CClonedNMJumpRollInfo &);
	CClonedNMJumpRollInfo &operator=(const CClonedNMJumpRollInfo &);

	bool		m_bExtraGravity;
};

//
// Task CTaskNMJumpRollFromRoadVehicle
//
class CTaskNMJumpRollFromRoadVehicle : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		// Defines the min and max time to spend in the stunned state at the end of the behaviour if the ped survives.
		float	m_GravityScale;
		float	m_StartForceDownHeight;

		CNmTuningSet m_Start;
		CNmTuningSetGroup m_EntryPointSets;

		CTaskNMBehaviour::Tunables::StandardBlendOutThresholds m_BlendOut;
		CTaskNMBehaviour::Tunables::StandardBlendOutThresholds m_QuickBlendOut;

		PAR_PARSABLE;
	};

	CTaskNMJumpRollFromRoadVehicle(u32 nMinTime, u32 nMaxTime, bool extraGravity, bool blendOutQuickly = false, atHashString m_EntryPointAnimInfoHash  = ((u32)0), CVehicle* pVehicle = NULL);
	~CTaskNMJumpRollFromRoadVehicle();

	// used in MP:
	void DisableZBlending() { m_DisableZBlending = true; }
	bool IsZBlendingDisabled() const { return m_DisableZBlending;}

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMJumpRollFromRoadVehicle");}
#endif
	virtual aiTask* Copy() const 
	{
		return rage_new CTaskNMJumpRollFromRoadVehicle(m_nMinTime, m_nMaxTime, m_ExtraGravity, m_BlendOutQuickly, m_EntryPointAnimInfoHash, m_pVehicle);
	}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE;}
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool HasBalanceFailed() const { return true; }

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMJumpRollInfo(m_ExtraGravity);
	}

	static void AppendEntryPointSets(CNmTuningSet* newItem, atHashString key)
	{
		sm_Tunables.m_EntryPointSets.Append(newItem,key);
	}

	static void RevertEntryPointSets(atHashString key)
	{
		sm_Tunables.m_EntryPointSets.Revert(key);
	}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);
	virtual bool FinishConditions(CPed* pPed);

	virtual void AddStartMessages(CPed* pPed, CNmMessageList& list);

	//////////////////////////
	// Local functions and data
private:

	Vector3 m_vecInitialVelocity;
	Vector3 m_vecInitialPosition;
	bool	m_bBehaviourTriggered;
	bool	m_bCatchfallTriggered;
	bool	m_ExtraGravity;
	bool	m_BlendOutQuickly;
	bool	m_DisableZBlending;

	atHashString m_EntryPointAnimInfoHash;

	RegdVeh m_pVehicle;

	static Tunables	sm_Tunables;

};

#endif // ! INC_TASKNMJUMPROLLFROMROADVEHICLE_H_
