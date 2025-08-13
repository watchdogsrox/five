//
// Task/TaskDropDown.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_DROPDOWN_H
#define TASK_DROPDOWN_H

#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"
#include "network/General/NetworkUtil.h"
#include "Task/Movement/Climbing/DropDownDetector.h"

////////////////////////////////////////////////////////////////////////////////

class CTaskDropDown : public CTaskFSMClone
{
	friend class CClonedTaskDropDownInfo;

public:

	CTaskDropDown(const CDropDown &dropDown, float fBlendInTime = NORMAL_BLEND_DURATION);
	
	virtual ~CTaskDropDown();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_DROP_DOWN; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskDropDown(m_DropDown); }

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Display debug information specific to this task
	virtual void Debug() const;

	// PURPOSE: Get the name of the specified state
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	// Clone task implementation for CTaskDropDown
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed));
	virtual bool                OverridesNetworkAttachment(CPed *UNUSED_PARAM(pPed)) { return true; }
	virtual bool				OverridesVehicleState() const	{ return true; }
    virtual bool                UseInAirBlender() const { return GetState() == State_Fall; }
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual bool                AllowsLocalHitReactions() const { return false; }

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Called after position is updated
	virtual bool ProcessPostMovement();

	////////////////////////////////////////////////////////////////////////////////

	// RETURNS: Returns true if we can break out to perform a new movement task.
	bool CanBreakoutToMovementTask() const;

	// PURPOSE: Get current drop down data.
	const CDropDown &GetDropDown() const { return m_DropDown; }

	enum eDropDownSpeed
	{
		DropDownSpeed_Stand = 0,
		DropDownSpeed_Walk,
		DropDownSpeed_Run,
	};

	// PURPOSE: Modify the range in which we scale the desired velocity.
	eDropDownSpeed GetDropDownSpeed() const;

#if __BANK
	// PURPOSE: Expose variables to RAG
	static void AddWidgets(bkBank& bank);
	void DumpToLog();
#endif // __BANK

	// PURPOSE: Check if we are using drop downs.
	static bank_bool GetUsingDropDowns();

protected:

	enum State
	{
		State_Init,
		State_DropDown,
		State_Fall,
		State_Quit,
	};

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Determine whether or not the task should abort
	// RETURNS: True if should abort
	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

private:

	enum eHandIK
	{
		HandIK_Left = 0,
		HandIK_Right,
		HandIK_Max
	};

	////////////////////////////////////////////////////////////////////////////////

	FSM_Return StateInit_OnEnter();
	FSM_Return StateInit_OnUpdate();
	FSM_Return StateDropDown_OnEnter();
	FSM_Return StateDropDown_OnUpdate();
	FSM_Return StateFall_OnEnter();
	FSM_Return StateFall_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Send the MoVE signals
	void UpdateMoVE();

	// PURPOSE: Init distance we need to scale ped to meet start point.
	void InitVelocityScaling();

	// PURPOSE: Modify the desired velocity to get where we want to go
	bool ScaleDesiredVelocity();

	// PURPOSE: Modify the range in which we scale the desired velocity.
	void SetDesiredVelocityAdjustmentRange(float fStart, float fEnd);

	// PURPOSE: Hand IK.
	void UpdateHandIK(eHandIK handIndex, int nHandBone, int nHelperBone);
	void ProcessHandIK();

	bool GetIsAttachedToPhysical() const;

	void ShowWeapon(bool show);

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	// PURPOSE: Add Ref for clip set.
	fwClipSetRequestHelper m_ClipSetRequestHelper;

	CDropDown m_DropDown;

	Quaternion m_TargetAttachOrientation;

	Vector3 m_vTargetDist;
	Vector3 m_vCloneStartPosition;

	Vector3 m_vHandIKPosition;

	float m_fInitialMBR;

	float m_fBlendInTime;

	float m_fClipAdjustStartPhase;
	float m_fClipAdjustEndPhase;
	float m_fClipLastPhase;

	// PURPOSE: Has network blended in yet?
	bool m_bNetworkBlendedIn : 1;

	bool m_bHandIKSet : 1;
	bool m_bHandIKFinished : 1;
	bool m_bCloneTaskNoLongerRunningOnOwner :1;
	
	// PURPOSE: Is clone attempting to run. Used to blend into correct locomotion state.
	eDropDownSpeed m_eDropDownSpeed;

	// PURPOSE: Use auto drop down
	static bank_bool ms_bUseDropDowns;

	static const fwMvClipSetId ms_DropDownClipSet;
	static const fwMvFlagId ms_RunDropId;
	static const fwMvFlagId ms_LeftHandedDropId;
	static const fwMvFlagId ms_SmallDropId;
	static const fwMvFlagId ms_LeftHandDropId;
	static const fwMvFloatId ms_DropHeightId;
	static const fwMvBooleanId ms_EnterDropDownId;
	static const fwMvBooleanId ms_ExitDropDownId;
	static const fwMvClipId ms_DropDownClipId;
	static const fwMvFloatId ms_DropDownPhaseId;
};

//-------------------------------------------------------------------------
// Task info for drop down
//-------------------------------------------------------------------------
class CClonedTaskDropDownInfo : public CSerialisedFSMTaskInfo
{
public:

	static const int s_SIZEOF_DROPDOWN_TYPE = 2;

	CClonedTaskDropDownInfo();
	CClonedTaskDropDownInfo(s32 state, const CDropDown &dropDown, const Vector3 &vPedStartPosition, u8 iDropDownSpeed);
	~CClonedTaskDropDownInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_DROP_DOWN;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskDropDown::State_Quit>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Drop Down State";}

	const CDropDown &GetDropDown() const		{ return m_DropDown; }
	const Vector3 &GetPedStartPosition() const	{ return m_vPedStartPosition; }
	u8 GetDropDownSpeed() const					{ return m_iDropDownSpeed; }
	CEntity *GetPhysicalEntity()				{ return m_pPhysicalEntity.GetEntity(); }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bHasEntity = m_pPhysicalEntity.GetEntity()!=NULL;

		SERIALISE_BOOL(serialiser, bHasEntity, "Has attach entity");
		if(bHasEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			ObjectId targetID = m_pPhysicalEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "attach entity");
			m_pPhysicalEntity.SetEntityID(targetID);
		}

		SERIALISE_POSITION(serialiser, m_vPedStartPosition,	"Ped start position" );
		SERIALISE_POSITION(serialiser, m_DropDown.m_vDropDownStartPoint,	"Drop Down start point" );
		SERIALISE_POSITION(serialiser, m_DropDown.m_vDropDownEdge,		"Drop Down Edge" );
		SERIALISE_POSITION(serialiser, m_DropDown.m_vDropDownLand,		"Drop Down Land" );
		SERIALISE_VECTOR(serialiser, m_DropDown.m_vDropDownHeading,		1.1f, SIZEOF_HEADING, "Drop Down Heading" );
		SERIALISE_PACKED_FLOAT(serialiser, m_DropDown.m_fDropHeight, WORLDLIMITS_ZMAX, 32, "Drop Down Height" );
		SERIALISE_BOOL(serialiser, m_DropDown.m_bSmallDrop,				"Small Drop Down" );
		SERIALISE_BOOL(serialiser, m_DropDown.m_bLeftHandDrop,			"Left Hand Drop Down" );
		SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_DropDown.m_eDropType), s_SIZEOF_DROPDOWN_TYPE, "Drop Down Type" );
		SERIALISE_UNSIGNED(serialiser, m_iDropDownSpeed, SIZEOF_DROPDOWN_SPEED, "speed");
	}

private:

	static const unsigned int SIZEOF_DROPDOWN_SPEED = 3;
	static const unsigned int SIZEOF_HEADING = 10;

	CDropDown	m_DropDown;
	CSyncedEntity m_pPhysicalEntity;
	Vector3		m_vPedStartPosition;
	u8		m_iDropDownSpeed;
};

////////////////////////////////////////////////////////////////////////////////

#endif // TASK_DROPDOWN_H
