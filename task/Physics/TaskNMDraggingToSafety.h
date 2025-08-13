// Filename   :	TaskNMDraggingToSafety.h
// Description:	An NM behavior for a ped being dragged to safety

#ifndef INC_TASKNMDRAGGING_TO_SAFETY_H_
#define INC_TASKNMDRAGGING_TO_SAFETY_H_

// Rage headers
#include "fwanimation/boneids.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskNMDraggingToSafety
////////////////////////////////////////////////////////////////////////////////

class CTaskNMDraggingToSafety : public CTaskNMBehaviour
{

public:

	struct Tunables : CTuning
	{
		struct Stiffness
		{
			Stiffness()
			{}
			
			float	m_Relaxation;
			float	m_HeadAndNeck;
			float	m_AnkleAndWrist;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct DraggerArmIk
		{
			DraggerArmIk()
			{}
			
			bool	m_Enabled;
			Vector3	m_LeftBoneOffset;
			Vector3	m_RightBoneOffset;
			
			PAR_SIMPLE_PARSABLE;
		};

		struct Constraints
		{
			Constraints()
			{}

			float m_MaxDistance;

			PAR_SIMPLE_PARSABLE;
		};

		struct Forces
		{
			Forces()
			{}

			bool	m_Enabled;
			Vector3	m_LeftHandOffset;
			Vector3	m_RightHandOffset;

			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		Stiffness		m_Stiffness;
		DraggerArmIk	m_DraggerArmIk;
		Constraints		m_Constraints;
		Forces			m_Forces;
		
		PAR_PARSABLE;
	};
	
public:

	CTaskNMDraggingToSafety(CPed* pDragged, CPed* pDragger);
	~CTaskNMDraggingToSafety();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMDraggingToSafety");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMDraggingToSafety(GetDragged(), GetDragger());}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_DRAGGING_TO_SAFETY;}
	
	virtual	void CleanUp();
	
	virtual FSM_Return ProcessPreFSM();
	
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	CPed* GetDragged() const { return m_pDragged; }
	CPed* GetDragger() const { return m_pDragger; }

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);
	
private:

	bool CalculateBonePositionForPed(const CPed& rPed, eAnimBoneTag nBoneTag, Vector3& vOffset, Vector3& vPosition) const;
	bool CalculateLeftHandPositionForDraggerArmIk(Vector3& vPosition) const;
	bool CalculateRightHandPositionForDraggerArmIk(Vector3& vPosition) const;
	void ControlDraggerArmIk();
	void CreateConstraint(phConstraintHandle& rHandle, const u16 uDraggedComponent, const u16 uDraggerComponent);
	void ResetConstraint(phConstraintHandle& rHandle);
	
private:

	RegdPed				m_pDragged;
	RegdPed				m_pDragger;
	phConstraintHandle	m_HandleL;
	phConstraintHandle	m_HandleR;
	
private:

	static Tunables	sm_Tunables;
	
};

#endif // ! INC_TASKNMDRAGGING_TO_SAFETY_H_
