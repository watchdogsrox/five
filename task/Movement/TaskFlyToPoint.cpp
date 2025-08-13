// File header
#include "Task/Movement/TaskFlyToPoint.h"

// Rage headers
#include "math/angmath.h"

// Game headers
#include "AI/stats.h"
#include "Peds/PedIntelligence.h"
#include "Peds/WildlifeManager.h"
#include "Scene/world/GameWorldHeightMap.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskBirdLocomotion.h"

using namespace AIStats;

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskFlyToPoint
//////////////////////////////////////////////////////////////////////////

CTaskFlyToPoint::Tunables CTaskFlyToPoint::sm_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskFlyToPoint, 0x4156878b);

const float CTaskFlyToPoint::ms_fTargetRadius=0.5f;

#if __ASSERT
void CTaskFlyToPoint::AssertIsValidGoToTarget(const Vector3 & vPos) const
{
	Assert(rage::FPIsFinite(vPos.x)&&rage::FPIsFinite(vPos.y)&&rage::FPIsFinite(vPos.z));
}
#endif

CTaskFlyToPoint::CTaskFlyToPoint(const float fMoveBlend, const Vector3 &vTarget, const float fTargetRadius, bool bIgnoreLandingRadius) :
	CTaskMoveBase(fMoveBlend)

,	m_vStartingPosition(0.0f, 0.0f, 0.0f)
,	m_vTarget(vTarget)
,	m_fTargetRadius(fTargetRadius)
,	m_fAvoidanceAngle(0.0f)
,	m_fAvoidanceTimer(0.0f)
,	m_bIgnoreLandingRadius(bIgnoreLandingRadius)
,	m_bIsAvoidingTerrain(false)
{
	Init();
	SetInternalTaskType(CTaskTypes::TASK_FLY_TO_POINT);
}

void CTaskFlyToPoint::Init()
{
	m_bFirstTime = true;
	m_bAborting = false;

	SetTarget(NULL, VECTOR3_TO_VEC3V(m_vTarget), true);
}


CTaskFlyToPoint::~CTaskFlyToPoint()
{
}

#if !__FINAL
void CTaskFlyToPoint::Debug() const
{
#if DEBUG_DRAW
	const Vector3 v1= VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	const Vector3& v2=m_vTarget;
	grcDebugDraw::Line(v1,v2,Color_BlueViolet);
	grcDebugDraw::Sphere(v2,m_fTargetRadius,Color_blue,false);

	// Ped-to- lowest position on height map
	Vector3 v3 = v1;
	v3.z = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(v1.x, v1.y);
	grcDebugDraw::Line(v1, v3, (v1.z < v3.z) ? Color_LightCyan : Color_SpringGreen);
#endif
}
#endif

void CTaskFlyToPoint::SetTarget(const CPed* UNUSED_PARAM(pPed), Vec3V_In vTarget, const bool bForce)
{
#if __ASSERT
	AssertIsValidGoToTarget(VEC3V_TO_VECTOR3(vTarget));
#endif

	if ((bForce)|| (m_vTarget != VEC3V_TO_VECTOR3(vTarget)))
	{
		m_vTarget = VEC3V_TO_VECTOR3(vTarget);
	}
}


bool CTaskFlyToPoint::CheckHasAchievedTarget(const Vector3 & vTarget, CPed * pPed)
{
	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	const bool bStopping = ( pTask && pTask->IsOnFoot() && (pTask->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION) && ((CTaskMotionBasicLocomotion*)pTask)->GetIsStopping());

	//Compute the direction and distance from ped to target.
	const Vector3 vPedPos= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vDiff = vTarget-vPedPos;

	Vector2 vDir;
	vDiff.GetVector2XY(vDir);
	const float fDistSqr = vDiff.XYMag2();
	vDir.Normalize();

	float fExtraRadius = 0.0f;

	if (!m_bIgnoreLandingRadius)
	{
		fExtraRadius += pPed->GetCurrentMotionTask()->GetExtraRadiusForGotoPoint();
		CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
		if (pMotionTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_BIRD)
		{
			CTaskBirdLocomotion* pBird = static_cast<CTaskBirdLocomotion*>(pMotionTask);
			fExtraRadius += rage::Max(0.0f, pBird->GetDescendClipRatio() * vDiff.z);
		}
	}

	float fTargetRadiusSqr = (m_fTargetRadius+fExtraRadius)*(m_fTargetRadius+fExtraRadius);

	// Compute if the ped is in range (on the 2D plane)
	const bool bInRange = (fDistSqr < fTargetRadiusSqr);
	if (bInRange)
	{
		pPed->GetMotionData()->SetIsNotFlying();
		return true;
	}

	// Calculate the vector from the target to the projected position of ped next frame
	Vector3 projDiff;
	projDiff=vPedPos + (pPed->GetVelocity() * GetTimeStep());
	projDiff=vTarget - projDiff;

	// If dot product of two vectors is negative then they point in different directions
	// and the next frame the ped will pass over the target point
	float d1 = vDiff.x * projDiff.x + vDiff.y * projDiff.y;
	if (d1 <= 0.0f)
	{
		if(!bStopping)
		{
#if __DEV
			//Displayf("%s Moved over point\n", pPed->GetModelName());
#endif
			return true;
		}
	}

	return false;
}

bool CTaskFlyToPoint::ProcessMove(CPed* pPed)
{
	if (m_bFirstTime)
	{
		m_vStartingPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() );
	}

	if(m_bAborting || CheckHasAchievedTarget(m_vTarget, pPed))
	{
		// Pass word to the locomotion task that we're finished flying
		pPed->GetMotionData()->SetIsNotFlying();
		return true;
	}

	// Set the moveblend ratio to be the initial value.  It will be over-ridden below if necessary (for cornering, slowing for peds, etc)
	m_fMoveBlendRatio = m_fInitialMoveBlendRatio;

	ComputeHeadingAndPitch(pPed, m_vTarget);
      
	if(m_bFirstTime)
	{
		pPed->GetPedIntelligence()->GetEventScanner()->ScanForEventsNow(*pPed, CEventScanner::VEHICLE_POTENTIAL_COLLISION_SCAN);
		m_bFirstTime = false;
	}

    //Not finished yet.
    return false;
}

void CTaskFlyToPoint::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Pass word to the locomotion task that we're finished flying
	if (pPed)
	{
		pPed->GetMotionData()->SetIsNotFlying();
	}

	m_bAborting=true;

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}

void CTaskFlyToPoint::ComputeHeadingAndPitch(CPed *pPed, const Vector3 &vTarget)
{
	// Track this since we look up the heightmap so much.
	PF_FUNC(ComputeHeadingAndPitch);

	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	Vec3V vForward = pPed->GetTransform().GetForward();
	const float fLookupDist = sm_Tunables.m_HeightMapLookAheadDist;

	float fAscendRatio = 1.0f;
	CTask* pMotionTask = pPed->GetPrimaryMotionTask();
	if (pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_BIRD)
	{
		fAscendRatio = static_cast<CTaskBirdLocomotion*>(pMotionTask)->GetAscendClipRatio();
	}

	CTaskFlyToPoint::FlightOptions flightPlan = CheckFlightPlanAgainstHeightMap(vPedPos, vForward, fLookupDist, fAscendRatio);

	bool bAvoidedLastTime = m_bIsAvoidingTerrain;
	m_bIsAvoidingTerrain = false;

	switch(flightPlan)
	{
		case FO_Ascend:
		{
			pPed->GetMotionData()->SetIsFlyingUpwards();
			break;
		}
		case FO_Descend:
		{
			pPed->GetMotionData()->SetIsFlyingDownwards();
			break;
		}
		case FO_TurnFromCollision:
		{
			pPed->GetMotionData()->SetIsFlyingUpwards();
			// Always steer to the left - this could be varied potentially.
			if (!bAvoidedLastTime)
			{
				m_fAvoidanceAngle = sm_Tunables.m_InitialTerrainAvoidanceAngleD * DtoR;
			}
			m_bIsAvoidingTerrain = true;
			break;
		}
		case FO_StayLevel:
		default:
		{
			pPed->GetMotionData()->SetIsFlyingForwards();
			break;
		}
	}

	// Maybe the initial avoidance angle wasn't good enough.
	// In that case, we keep bumping it up over time.
	if (m_bIsAvoidingTerrain)
	{
		m_fAvoidanceTimer += GetTimeStep();
		if (m_fAvoidanceTimer >= sm_Tunables.m_TimeBetweenIncreasingAvoidanceAngle)
		{
			m_fAvoidanceAngle += sm_Tunables.m_ProgressiveTerrainAvoidanceAngleD * DtoR;
			m_fAvoidanceTimer = 0.0f;
		}
	}
	else
	{
		m_fAvoidanceTimer = 0.0f;
	}

	//Set the desired heading angle for the ped.
	m_fTheta = fwAngle::GetRadianAngleBetweenPoints(vTarget.x, vTarget.y, vPedPos.GetXf(), vPedPos.GetYf());
	m_fTheta += m_fAvoidanceAngle;
	m_fTheta = fwAngle::LimitRadianAngleSafe(m_fTheta);

}

// PURPOSE:  Return true if a given flight is going to impact anywhere on the heightmap.
// Basically, this algorithm draws a line from the start position and sees if it is going to hit anywhere in the heightmap.
// If it does impact somewhere, it decides if that obstacle can be flown over using the ascend ratio.  If it can't be flown over,
// then the only option really is to steer to avoid.
// Note that this function never recommends descending, which isn't a big deal aesthetically, at least as long as we don't want to spend time on probes.
CTaskFlyToPoint::FlightOptions CTaskFlyToPoint::CheckFlightPlanAgainstHeightMap(Vec3V_In vStartPos, Vec3V_In vDirection, float fDistance, float fAscendRatio)
{
	static const int sf_NumSamples = 10;

	// Default to staying level.
	CTaskFlyToPoint::FlightOptions option = FO_StayLevel;

	// Discretize the line over some number of samples.
	// Start at one not zero so we actually get some distance.
	for(int i=1; i <= sf_NumSamples; i++)
	{
		float fGroundDist = fDistance * (i / (float)sf_NumSamples);
		Vec3V vTranslation = vDirection * ScalarV(fGroundDist);

		// Determine the sample point - this is done by traveling along the direction vector.
		Vec3V vNewPoint = vTranslation + vStartPos;
		ScalarV vFlatDist = DistXY(vStartPos, vNewPoint);
		float fPointHeight = GetFlightHeightAtLocation(vNewPoint.GetXf(), vNewPoint.GetYf());

		// Determine the highest possible point the bird can be if ascending fully.
		float fMaxZOfBird = vStartPos.GetZf() + (vFlatDist.Getf() * fAscendRatio);

		if (fPointHeight > fMaxZOfBird)
		{
			// There is no way the bird can achieve that height.
			return CTaskFlyToPoint::FO_TurnFromCollision;
		}
		else if (fPointHeight > vStartPos.GetZf())
		{
			// It is possible to avoid that obstacle by flying upwards.
			option = FO_Ascend;
		}
	}

	return option;
}

// Helper function to look up the minimum safe flight height at an X,Y coordinate.
float CTaskFlyToPoint::GetFlightHeightAtLocation(float fX, float fY)
{
	const float fScriptHeightOverride = CWildlifeManager::GetInstance().GetScriptMinBirdHeight();
	const float fHeightMapZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(fX, fY) + sm_Tunables.m_HeightMapDelta;
	const float fDesiredZ = (fScriptHeightOverride < 0.0f || fHeightMapZ > fScriptHeightOverride) ? fHeightMapZ : fScriptHeightOverride;
	return fDesiredZ;
}
