#ifndef TASK_WEAPON_BLOCKED_H
#define TASK_WEAPON_BLOCKED_H

// Game headers
#include "Task/System/Task.h"
#include "Task/Weapons/WeaponController.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"

// Forward declarations
class CWeaponController;

//////////////////////////////////////////////////////////////////////////
// CTaskWeaponBlocked
//////////////////////////////////////////////////////////////////////////

class CTaskWeaponBlocked : public CTaskFSMClone
{
public:

	CTaskWeaponBlocked(const CWeaponController::WeaponControllerType weaponControllerType);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskWeaponBlocked(m_weaponControllerType); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_WEAPON_BLOCKED; }

	// Check if the weapon is blocked
	static bool IsWeaponBlocked(const CPed* pPed, Vector3& vTargetPos, Vector3* pvOverridenStart = NULL, float fProbeLengthMultiplier = 1.0f, float fRadiusMultiplier = 1.0f, bool bCheckAlternativePos = false, bool bWantsToFire = false, bool bOverrideProbeLength = false, float fOverridenProbeLength = 0.5f);

	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();

	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed));
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	// Returns the weapon ClipId
	static bool GetWallBlockClip(const CPed* pPed, const CWeaponInfo* pWeaponInfo, fwMvClipSetId& appropriateWeaponClipSetId_Out, fwMvClipId& wallBlockClipId_Out);

#if FPS_MODE_SUPPORTED
	bool IsUsingFPSLeftHandGripIK() const { return m_bUsingLeftHandGripIKFPS; }
#endif 

public:

	typedef enum
	{
		State_Blocked = 0,
		State_Blocked_FPS,
		State_Quit,
	} State;

protected:

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	FSM_Return StateBlockedOnEnter(CPed* pPed);
	FSM_Return StateBlockedOnUpdate(CPed* pPed);
	FSM_Return StateBlockedOnUpdateClone(CPed* pPed);
	FSM_Return StateBlockedFPSOnEnter(CPed* pPed);
	FSM_Return StateBlockedFPSOnUpdate(CPed* pPed);

	void SetUpFPSMoveNetwork();

	//
	// Members
	//

	// Gun controller
	CWeaponController::WeaponControllerType m_weaponControllerType;

	CMoveNetworkHelper m_MoveWeaponBlockedFPSNetworkHelper;

	const static fwMvClipId ms_WeaponBlockedClipId;
	const static fwMvClipId ms_WeaponBlockedNewClipId;

	const static fwMvNetworkDefId ms_NetworkWeaponBlockedFPS;
	const static fwMvFloatId ms_PitchId;

#if FPS_MODE_SUPPORTED
	bool m_bUsingLeftHandGripIKFPS;
#endif
};

class CClonedWeaponBlockedInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams
		(
			s32 const state,
			CWeaponController::WeaponControllerType const  weaponControllerType
		)
		:
			m_state(state),
			m_type(weaponControllerType)
		{}
		
		u32	m_state;
		CWeaponController::WeaponControllerType m_type;
	};


public:
    CClonedWeaponBlockedInfo( InitParams const& initParams );

    CClonedWeaponBlockedInfo();
    ~CClonedWeaponBlockedInfo();

	virtual s32							GetTaskInfoType( ) const		{ return INFO_TYPE_WEAPON_BLOCKED;		}

	virtual CTaskFSMClone*				CreateCloneFSMTask();

    virtual bool						HasState() const				{ return true; }
	virtual u32							GetSizeOfState() const			{ return datBitsNeeded<CTaskWeaponBlocked::State_Quit>::COUNT; }
    virtual const char*						GetStatusName(u32) const		{ return "Status";}

	CWeaponController::WeaponControllerType GetType(void) const			{ return m_weaponControllerType; }

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_weaponControllerType), SIZEOF_WEAPON_CONTROLLER,  "Weapon Controller");

		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

	virtual CTask*	CreateLocalTask(fwEntity *pEntity);

private:

	static const u32 SIZEOF_WEAPON_CONTROLLER = datBitsNeeded<CWeaponController::WCT_Max>::COUNT;

    CClonedWeaponBlockedInfo(const CClonedWeaponBlockedInfo &);
    CClonedWeaponBlockedInfo &operator=(const CClonedWeaponBlockedInfo &);

	CWeaponController::WeaponControllerType m_weaponControllerType;
};

#endif // TASK_WEAPON_BLOCKED_H
