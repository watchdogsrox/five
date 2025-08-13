#ifndef TASK_MOVE_H
#define TASK_MOVE_H

// Game headers
#include "Peds/PedDefines.h"
#include "Task/System/Task.h"

//-------------------------------------------------------------------------
// Movement interface, standard interface for all movement tasks (complex and simple)
//-------------------------------------------------------------------------
class CTaskMoveInterface
{
public:

	// Constructor
	CTaskMoveInterface( const float fMoveBlendRatio )
		: m_fMoveBlendRatio( fMoveBlendRatio )
		, m_bNewMoveSpeed( false )
#if !__FINAL
		, m_pOwner(NULL)
#endif // !__FINAL
	{
	}

	virtual ~CTaskMoveInterface() {}

	virtual void SetMoveBlendRatio( const float fMoveBlendRatio, const bool bFlagAsChanged = true );
	virtual float GetMoveBlendRatio() const { return m_fMoveBlendRatio; }

	// Used to determine if this movement task is still actively being used,
	// If not used it will be terminated
	virtual void SetCheckedThisFrame( const bool bChecked ) { m_bCheckedThisFrame=bChecked; }
	virtual int GetCheckedThisFrame() const { return m_bCheckedThisFrame; }

	inline bool GetHasNewMoveSpeed() const { return m_bNewMoveSpeed; }
	inline void SetHasNewMoveSpeed( const bool b ) { m_bNewMoveSpeed = b; }

	//Implement this function for sub-classes to return an appropriate target point.
	virtual bool HasTarget() const = 0;			// Whether this is a move task which has a target (some don't)
	virtual Vector3 GetTarget() const = 0;	// Gets the target.  HasTarget() should be called first to check if this call is valid.
	virtual Vector3 GetLookAheadTarget() const = 0;	// Lookahead is somewhere further in the path than we are aiming for
	virtual float GetTargetRadius() const = 0;
	virtual void SetTarget( const CPed* UNUSED_PARAM(pPed), Vec3V_In UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce)=false ) {}
	virtual void SetTargetRadius( const float UNUSED_PARAM(fTargetRadius ) ) {}
	virtual void SetSlowDownDistance( const float UNUSED_PARAM(fSlowDownDistance ) ) {}
	virtual void SetLookAheadTarget( const CPed* UNUSED_PARAM(pPed), const Vector3& UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce)=false ) {}

	// Updates the status of the passed task so this tasks current status.
	// Used by control movement tasks to keep the stored version they have up to date, so that
	// in the case that this task is removed it can re-instate one at the same progress.
	virtual void UpdateProgress( class aiTask* ) {}

#if !__FINAL
	void SetOwner( aiTask* pOwner ) { m_pOwner = pOwner; }
#endif // !__FINAL

protected:

	f32 m_fMoveBlendRatio;	// (0.0 .. 3.0) : still, walk, run, sprint
	u32 m_bNewMoveSpeed : 1;
	u32 m_bCheckedThisFrame :1;
private:
#if !__FINAL
	// Deliberate because it's floods the regdref value with Add/Remove - 
	// delete protection should stop us from being nastily cleaned up.  
	// Removed the gettor to stop us being used in appropriately.
	aiTask *m_pOwner;
#endif // !__FINAL
};

//-------------------------------------------------------------------------
// FSM base movement task, all movement tasks should derive from this.
// This replaces the previous CTaskSimpleMove and CTaskComplexMove.
//-------------------------------------------------------------------------
class CTaskMove : public CTask, public CTaskMoveInterface
{
public:

	CTaskMove(const float fMoveBlendRatio);
	virtual ~CTaskMove();

	virtual bool IsMoveTask() const { return true; }
	virtual bool IsTaskMoving() const;
	virtual CTaskMoveInterface* GetMoveInterface() { return this; }
	virtual const CTaskMoveInterface* GetMoveInterface() const { return this; }

	//Implemented only in CTaskNavBase to enable safe casting from higher classes within that task
	virtual bool InheritsFromTaskNavBase() const { return false; }

	// Retrieve the backup movmement task in active controlling CTaskComplexControlMovement
	CTaskMove * GetControlMovementBackup(const CPed * pPed);

	static Vector2 ProcessStrafeInputWithRestrictions(CPed* pPed, const CEntity* pTargetEntity, float fDistanceRestriction, float& rfModifiedStickAngle, bool& rbStickAngleTweaked);
};


//----------------------------------------------------------------------------------
// CLASS : CTaskMoveBase
// PURPOSE : Base class from which lowest level locomotion tasks should be derived

class CTaskMoveBase : public CTaskMove
{
public:

	CTaskMoveBase(const float fMoveBlend);
	virtual ~CTaskMoveBase();

	enum { State_Running };

	//--------------------------------------
	// Virtual members inherited From CTask

	virtual s32 GetDefaultStateAfterAbort() const { return State_Running; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32 UNUSED_PARAM(iState)) { return "Running"; }
	virtual void Debug() const {};
#endif

	static float ComputeMoveBlend(CPed& ped, const float fTargetSpeed);

	inline void SetDisableHeadingUpdate(const bool b) { m_bDisableHeadingUpdate = b; }
	inline bool GetDisableHeadingUpdate() const { return m_bDisableHeadingUpdate; }

	//Implement this function for sub-classes in order to set the heading and move speed.
	virtual bool ProcessMove(CPed* pPed)=0;

	//Not all sub-classes will have a target radius so just return 1.0 as a default.
	virtual float GetTargetRadius() const {return 1.0f;}

	//Blend in the appropriate anims given a heading and move speed computed in ProcessMove();
	void MoveAtSpeedInDirection(CPed* pPed);

	//Compute the heading and move speed then get the ped to move at that speed in the given direction.
    FSM_Return StateRunning_OnUpdate(CPed* pPed);

	virtual float GetMoveBlendRatio() const
	{
		return m_fInitialMoveBlendRatio;
	}

	virtual bool IsMoveTask() const { return true; }

	virtual inline void SetMoveBlendRatio(const float fMBR, const bool UNUSED_PARAM(bFlagAsChanged)=false)
	{
		Assertf(fMBR >= 0.0f && fMBR <= 3.0f, "m_fMoveBlendRatio was %.2f; it must be between 0.0f & 3.0f inclusive", fMBR);

		m_fMoveBlendRatio = Clamp(fMBR, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
		m_fInitialMoveBlendRatio = m_fMoveBlendRatio;
	}

	// Given a target speed in mps, this will set the required m_fMoveBlend we need
	void SetSpeed(const float fSpeedInMetresPerSecond);

	// Helper function used by CTaskMoveBase and CTaskMoveStandStill, for using
	// CTaskMoveMountedSpurControl as a subtask. Returns false if this task is not running.
	static bool UpdateRideControlSubTasks(CTaskMove& moveTask, float mbr);

	// Check if a ped should run CTaskMoveMountedSpurControl as a subtask of movement
	// tasks, rather than setting the move blend ratio directly.
	static bool ShouldRunRideControlSubTasks(CPed& ped);

private:

	void ScanWorld(CPed* pPed);
	void SetPedHeadingAndPitch(CPed* pPed);

protected:

	float m_fTheta;						// Move heading
	float m_fPitch;						// Move pitch
	float m_fInitialMoveBlendRatio;		// Base movement speed for the task
	bool m_bDisableHeadingUpdate;		// Prevent the task from updating the ped's m_fDesiredHeading

	float ComputePlayerMoveBlendRatio(const CPed& ped, const Vector3 *pvecTarget) const;

	// Get the actual target we're heading for.  This might not be the task's target, it may
	// be a temporary position calculated by the avoidance code.
	virtual const Vector3 & GetCurrentTarget() = 0;
};

#endif // TASK_MOVE_H
