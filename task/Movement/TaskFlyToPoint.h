#ifndef TASK_FLY_TO_POINT_H
#define TASK_FLY_TO_POINT_H

// Game headers
#include "TaskGoTo.h"

#include "AI/AITarget.h"
#include "Peds/PedDefines.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

//////////////////////////////////////////////////////////////////////////
// CTaskFlyToPoint
//////////////////////////////////////////////////////////////////////////

class CTaskFlyToPoint : public CTaskMoveBase
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_HeightMapDelta;
		float m_HeightMapLookAheadDist;
		float m_InitialTerrainAvoidanceAngleD;
		float m_ProgressiveTerrainAvoidanceAngleD;
		float m_TimeBetweenIncreasingAvoidanceAngle;

		PAR_PARSABLE;
	};

	static const float ms_fTargetRadius;

	CTaskFlyToPoint(const float fMoveBlend, const Vector3& vTarget, const float fTargetRadius=ms_fTargetRadius, bool bIgnoreLandingRadius=false);
	virtual ~CTaskFlyToPoint();

	void Init();

	virtual aiTask* Copy() const
	{
		CTaskFlyToPoint * pClone = rage_new CTaskFlyToPoint(
			m_fMoveBlendRatio,
			m_vTarget,
			m_fTargetRadius,
			m_bIgnoreLandingRadius
			);
		return pClone;
	}

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_FLY_TO_POINT;}

#if !__FINAL
	virtual void Debug() const;
#endif

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget(void) const {return m_vTarget;}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const {return m_fTargetRadius;}

	virtual bool ProcessMove(CPed* pPed);

	bool CheckHasAchievedTarget(const Vector3 & vTarget, CPed * pPed);

#if __ASSERT
	void AssertIsValidGoToTarget(const Vector3 & vPos) const;
#endif

	virtual void SetTarget(const CPed* UNUSED_PARAM(pPed), Vec3V_In vTarget, const bool bForce=false); 

	virtual void SetTargetRadius(const float fTargetRadius ) 
	{
		if(m_fTargetRadius != fTargetRadius)
		{
			m_fTargetRadius = fTargetRadius;
		}
	}

	// Flight options.
	enum FlightOptions
	{
		FO_TurnFromCollision,
		FO_Ascend,
		FO_Descend,
		FO_StayLevel
	};

	// PURPOSE:  Return true if a given flight is going to impact anywhere on the heightmap.
	// fAscendRatio = ratio of a bird's ascend speed over the bird's ground (XY) speed.
	static FlightOptions CheckFlightPlanAgainstHeightMap(Vec3V_In vStartPos, Vec3V_In vDirection, float fDistance, float fAscendRatio);

	// Helper function to look up the minimum safe flight height at an X,Y coordinate.
	static float GetFlightHeightAtLocation(float fX, float fY);

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual void ComputeHeadingAndPitch(CPed* pPed, const Vector3 & vTarget);
	virtual const Vector3 & GetCurrentTarget() { return m_vTarget; }

	Vector3 m_vStartingPosition;	// The position that the ped was as when they first started this task
	Vector3 m_vTarget;				// The target position

	// The radius that the ped must get to the target for the task to complete
	float m_fTargetRadius;

	static Tunables	sm_Tunables;

private:

	float		m_fAvoidanceAngle;
	float		m_fAvoidanceTimer;						

	bool		m_bFirstTime							:1;
	bool		m_bAborting								:1;
	bool		m_bIgnoreLandingRadius					:1;
	bool		m_bIsAvoidingTerrain					:1;
};

#endif // TASK_FLY_TO_POINT_H
