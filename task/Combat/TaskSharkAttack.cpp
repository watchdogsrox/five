// Class Header
#include "Task/Combat/TaskSharkAttack.h"

// Rage Headers
#include "crclip/clip.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwmaths/angle.h"
#include "fwmaths/random.h"

// Game Headers
#include "animation/EventTags.h"
#include "audio/speechaudioentity.h"
#include "Camera/Base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "Camera/Scripted/ScriptDirector.h"
#include "Event/Events.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PedMotionData.h"
#include "Peds/rendering/PedDamage.h"
#include "Physics/WaterTestHelper.h"
#include "Physics/WorldProbe/shapetestmgr.h"
#include "Physics/WorldProbe/shapetestcapsuledesc.h"
#include "Physics/WorldProbe/shapetestresults.h"
#include "Physics/WorldProbe/wpcommon.h"
#include "system/controlMgr.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Combat/TaskCombatMounted.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskFishLocomotion.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Response/TaskFlee.h"
#include "Vfx/Systems/VfxBlood.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskSharkCircle
//////////////////////////////////////////////////////////////////////////

CTaskSharkCircle::Tunables CTaskSharkCircle::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSharkCircle, 0xee0de2fd);

CTaskSharkCircle::CTaskSharkCircle(float fMBR, const CAITarget& rTarget, float fRadius, float fAngularSpeed)
: CTaskMoveBase(fMBR)
, m_Target(rTarget)
, m_fAngleOffset(0.0f)
, m_fRadius(fRadius)
, m_fAngularSpeed(fAngularSpeed)
, m_bSetupPattern(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SHARK_CIRCLE);
}

bool CTaskSharkCircle::ProcessMove(CPed* pPed)
{
	// Early out if there is no target.
	if (!m_Target.GetIsValid())
	{
		return true;
	}

	if (!m_bSetupPattern)
	{
		// Project the initial point along the line connecting the shark to the victim.
		ProjectInitialPoint();

		// Decide on clockwise or counterclockwise circling.
		PickCirclingDirection();
	}

	if (IsCloseToCirclePoint())
	{
		// Move the circling point along the circle.
		float fChange = m_bCirclePositively ? 1.0f : -1.0f;
		fChange *= DtoR * m_fAngularSpeed * GetTimeStep();
		m_fAngleOffset += fChange;
		m_fAngleOffset = fwAngle::LimitRadianAngleSafe(m_fAngleOffset);
	}

	pPed->GetMotionData()->SetDesiredMoveRateOverride(sm_Tunables.m_MoveRateOverride);

	// Project out the point for the shark to aim towards.
	ProjectPoint(m_fAngleOffset);

	// Point the shark in the direction of the projected point.
	m_fTheta = fwAngle::GetRadianAngleBetweenPoints(m_vCurrentTarget.x, m_vCurrentTarget.y, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf());
	
	// Compute the pitch.
	// Compute the delta to the target, the squared magnitude in the XY plane,
	// and the squared magnitude of the whole vector.
	static const float fEps = 0.0001f;
	const float negDeltaX = pPed->GetTransform().GetPosition().GetXf() - m_vCurrentTarget.x;
	const float deltaY = m_vCurrentTarget.y - pPed->GetTransform().GetPosition().GetYf();
	const float deltaZ = m_vCurrentTarget.z - pPed->GetTransform().GetPosition().GetZf();
	const float deltaMagXYSq = negDeltaX*negDeltaX + deltaY*deltaY;
	const float deltaMagSq = deltaMagXYSq + deltaZ*deltaZ;

	// Compute the cosine of the pitch value. We clamp using fEps here to avoid
	// division by zero, but in that case, the result won't actually be used anyway.
	const float cosPitch = Min(sqrtf(deltaMagXYSq/Max(deltaMagSq, fEps)), 1.0f);

	// Get the absolute value of the pitch angle, and then set the appropriate sign.
	const float absPitch = acosf(cosPitch);
	m_fPitch = Selectf(deltaZ, absPitch, -absPitch);

	// Circling is performed at the MBR passed in by the user.
	pPed->GetMotionData()->SetDesiredMoveBlendRatio(GetMoveBlendRatio());
	
	// Circling is never finished, so return false.
	return false;
}

// The point is m_fRadius away from the victim.
void CTaskSharkCircle::ProjectPoint(float fDegrees)
{
	Vector3 vPos;
	m_Target.GetPosition(vPos);
	
	vPos.x -= m_fRadius * rage::Sinf(fDegrees);
	vPos.y += m_fRadius * rage::Cosf(fDegrees);

	// You never want to circle the target out of the water.
	CTaskSharkAttack::CapHeightForWater(RC_VEC3V(vPos));

	m_vCurrentTarget = vPos;
}

// Initially the projected point is the point on the circle aligned with the vector between the shark and victim.
void CTaskSharkCircle::ProjectInitialPoint()
{
	Vector3 vVictim;
	m_Target.GetPosition(vVictim);

	// You never want to circle the target out of the water.
	CTaskSharkAttack::CapHeightForWater(RC_VEC3V(vVictim));

	CPed* pShark = GetPed();
	float fDegrees = fwAngle::GetRadianAngleBetweenPoints(pShark->GetTransform().GetPosition().GetXf(), pShark->GetTransform().GetPosition().GetYf(), vVictim.x, vVictim.y);

	ProjectPoint(fDegrees);
	m_fAngleOffset = fDegrees;
	m_bSetupPattern = true;
}

// Decide to go clockwise or counterclockwise.
void CTaskSharkCircle::PickCirclingDirection()
{
	Vec3V vForward = GetPed()->GetTransform().GetForward();
	vForward.SetZ(ScalarV(V_ZERO));
	vForward = NormalizeSafe(vForward, Vec3V(V_ZERO));

	// Compute a positive tangent vector to the circle - 
	// this is the vector in 2D space that is from the initial target point to another point dA degrees along the circle.
	const float fDA = 10.0f * DtoR;

	Vec3V vCircle1 = VECTOR3_TO_VEC3V(m_vCurrentTarget);
	vCircle1.SetZ(ScalarV(V_ZERO));
	vCircle1 = NormalizeSafe(vCircle1, Vec3V(V_ZERO));

	Vector3 vNext;
	m_Target.GetPosition(vNext);
	vNext.x -= m_fRadius * rage::Sinf(m_fAngleOffset + fDA);
	vNext.y += m_fRadius * rage::Cosf(m_fAngleOffset + fDA);
	vNext.z = 0.0f;

	Vec3V vCircle2 = VECTOR3_TO_VEC3V(vNext);
	vCircle2 = NormalizeSafe(vCircle2, Vec3V(V_ZERO));

	Vec3V vPositiveTangent = vCircle2 - vCircle1;
	vPositiveTangent = NormalizeSafe(vPositiveTangent, Vec3V(V_ZERO), Vec3V(V_FLT_EPSILON));

	// If the forward direction lies in the same direction as the positive tangent vector, then circle positively.
	if (IsGreaterThanAll(Dot(vForward, vPositiveTangent), ScalarV(V_ZERO)))
	{
		m_bCirclePositively = true;
	}
	else
	{
		m_bCirclePositively = false;
	}
}

// Return true if the local target point is close to the shark (only in the XY plane).
bool CTaskSharkCircle::IsCloseToCirclePoint() const
{
	Vec3V vShark = GetPed()->GetTransform().GetPosition();
	vShark.SetZ(ScalarV(V_ZERO));
	Vec3V vTarget = VECTOR3_TO_VEC3V(m_vCurrentTarget);
	vTarget.SetZ(ScalarV(V_ZERO));

	if (IsLessThanAll(DistSquared(vTarget, vShark), ScalarV(sm_Tunables.m_AdvanceDistanceSquared)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

// Return the position of the ped being circled.
Vector3 CTaskSharkCircle::GetTarget() const
{
	Vector3 vPos;
	m_Target.GetPosition(vPos);
	return vPos;
}

Vector3 CTaskSharkCircle::GetLookAheadTarget() const
{
	return GetTarget();
}

void CTaskSharkCircle::SetTarget(const CPed* pPed, Vec3V_In vTarget, const bool UNUSED_PARAM(bForce))
{
	if (pPed)
	{
		m_Target.SetEntity(pPed);
	}
	else
	{
		m_Target.SetPosition(VEC3V_TO_VECTOR3(vTarget));
	}
}

// Return the local projected circling point.
const Vector3& CTaskSharkCircle::GetCurrentTarget()
{
	return m_vCurrentTarget;
}

void CTaskSharkCircle::CleanUp()
{
	CPed* pPed = GetPed();
	CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, false);
}


#if !__FINAL
void CTaskSharkCircle::Debug() const
{
#if DEBUG_DRAW
	Vector3 vCircleArea;
	m_Target.GetPosition(vCircleArea);
	const Vector3& vLocalTarget = m_vCurrentTarget;
	
	grcDebugDraw::Circle(vCircleArea, m_fRadius, Color_white, XAXIS, YAXIS);
	grcDebugDraw::Sphere(vLocalTarget, 1.0f, Color_white, false);

#endif
}
#endif

//////////////////////////////////////////////////////////////////////////
// CTaskSharkAttack
//////////////////////////////////////////////////////////////////////////

CTaskSharkAttack::Tunables CTaskSharkAttack::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSharkAttack, 0xa3b85000);

const strStreamingObjectName CTaskSharkAttack::ms_CameraAnimation("attack_cam",0x392B39BC);
const strStreamingObjectName CTaskSharkAttack::ms_SharkBiteDict("creatures@shark@move",0xA943497F);

s32 CTaskSharkAttack::sm_NumActiveSharks = 0;

CTaskSharkAttack::CTaskSharkAttack(CPed* pTarget)
: m_pTarget(pTarget)
, m_fCirclingCounter(0.0f)
, m_vCachedLungeLocation(V_ZERO)
, m_vFleePosition()
, m_SyncedSceneID(INVALID_SYNCED_SCENE_ID)
, m_iNumberOfFakeApproachesLeft(0)
, m_bHasAppliedPedDamage(false)
, m_bIsFleeingShore(false)
, m_bSharkCountNeedsCleanup(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SHARK_ATTACK);
}

CTaskSharkAttack::~CTaskSharkAttack()
{
}

void CTaskSharkAttack::CleanUp()
{
	//cleanup the camera 
	camInterface::GetSyncedSceneDirector().StopAnimatingCamera(camSyncedSceneDirector::TASK_SHARK_ATTACK_CLIENT, 2000); 
	
	// Cleanup collision.
	CPed* pPed = GetPed();
	pPed->SetNoCollision(NULL, 0);

	// Cleanup local avoidance.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SteersAroundPeds, true);

	// Cleanup the pitch.
	pPed->GetMotionData()->SetDesiredPitch(0.0f);

	// Let them die for being out of water again.
	CTaskFishLocomotion::ToggleFishSurvivesBeingOutOfWater(pPed, false);

	// Reset the shark count if necessary.
	if (m_bSharkCountNeedsCleanup)
	{
		sm_NumActiveSharks -= 1;
		m_bSharkCountNeedsCleanup = false;
	}
}

CTask::FSM_Return CTaskSharkAttack::ProcessPreFSM()
{
	// Target no longer exists.
	// TODO - Enable this task for things other than the local player.
	if(!m_pTarget || !m_pTarget->IsLocalPlayer() || m_pTarget->IsInjured() || m_pTarget->GetIsDeadOrDying())
	{
		return FSM_Quit;
	}

	CPed* pPed = GetPed();
	
	// Never slow for cornering.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_SuppressSlowingForCorners, true);

	// Disable timeslicing while running this task.
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskSharkAttack::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Surface)
			FSM_OnEnter
				Surface_OnEnter();
			FSM_OnUpdate
				return Surface_OnUpdate();

		FSM_State(State_Follow)
			FSM_OnEnter
				Follow_OnEnter();
			FSM_OnUpdate
				return Follow_OnUpdate();

		FSM_State(State_Circle)
			FSM_OnEnter
				Circle_OnEnter();
			FSM_OnUpdate
				return Circle_OnUpdate();

		FSM_State(State_Dive)
			FSM_OnEnter
				Dive_OnEnter();
			FSM_OnUpdate
				return Dive_OnUpdate();

		FSM_State(State_FakeLunge)
			FSM_OnEnter
				FakeLunge_OnEnter();
			FSM_OnUpdate
				return FakeLunge_OnUpdate();

		FSM_State(State_Lunge)
			FSM_OnEnter
				Lunge_OnEnter();
			FSM_OnUpdate
				return Lunge_OnUpdate();
			FSM_OnExit
				Lunge_OnExit();

		FSM_State(State_Bite)
			FSM_OnEnter
				Bite_OnEnter();
			FSM_OnUpdate
				return Bite_OnUpdate();
			FSM_OnExit
				Bite_OnExit();

		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskSharkAttack::Start_OnUpdate()
{
	// Sharks never die when out of water while in this task.
	CTaskFishLocomotion::ToggleFishSurvivesBeingOutOfWater(GetPed(), true);

	m_iNumberOfFakeApproachesLeft = rage::round(fwRandom::GetRandomNumberInRange((float)sm_Tunables.m_MinNumberFakeApproaches, (float)sm_Tunables.m_MaxNumberFakeApproaches));

	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (TargetInVehicle())
	{
		SetState(State_Follow);
	}
	else
	{
		if (IsTargetUnderwater())
		{
			SetState(State_Circle);
		}
		else
		{
			SetState(State_Surface);
		}
	}

	// Note that a shark is attacking the player.
	sm_NumActiveSharks += 1;
	m_bSharkCountNeedsCleanup = true;

	return FSM_Continue;
}

void CTaskSharkAttack::Surface_OnEnter()
{
	CPed* pPed = GetPed();

	// Attempt to get near the surface so the player can see the fin.
	Vec3V vPed = pPed->GetTransform().GetPosition();
	Vec3V vForward = pPed->GetTransform().GetForward();

	float fTargetZ = m_pTarget->m_Buoyancy.GetAvgAbsWaterLevel() - sm_Tunables.m_SurfaceZOffset;
	Vec3V vSurfaceTarget(vPed.GetXf() + (vForward.GetXf() * sm_Tunables.m_SurfaceProjectionDistance), 
		vPed.GetYf() + (vForward.GetYf() * sm_Tunables.m_SurfaceProjectionDistance), fTargetZ);
	CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, VEC3V_TO_VECTOR3(vSurfaceTarget), 0.1f);

	// Don't stick to the surface when surfacing.
	CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, false);

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
}

CTask::FSM_Return CTaskSharkAttack::Surface_OnUpdate()
{
	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (TargetInVehicle())
	{
		SetState(State_Follow);
	}
	else if (IsTargetUnderwater())
	{
		SetState(State_Circle);
	}
	else if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Circle);
	}
	else
	{
		// Check if we have achieved our target depth.
		float fWaterZ = 0.0f;
		CPed* pPed = GetPed();
		Vec3V vPed = pPed->GetTransform().GetPosition();
		if (Verifyf(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VEC3V_TO_VECTOR3(vPed), &fWaterZ, true), "Shark was mysteriously out of the water while surfacing."))
		{
			float fDesiredZ = pPed->GetTaskData().GetSurfaceSwimmingDepthOffset();
			if (fWaterZ - fDesiredZ <= vPed.GetZf())
			{
				SetState(State_Circle);
			}
		}
	}
	return FSM_Continue;
}

void CTaskSharkAttack::Follow_OnEnter()
{
	// Goto a point below and behind the target.
	m_vCachedLungeLocation = GenerateFollowLocation();
	Vector3 vTarget = VEC3V_TO_VECTOR3(m_vCachedLungeLocation);
	CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_SPRINT, vTarget, sm_Tunables.m_LungeTargetRadius);

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
}

CTask::FSM_Return CTaskSharkAttack::Follow_OnUpdate()
{
	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (!TargetInVehicle())
	{
		if (IsTargetUnderwater())
		{
			SetState(State_Circle);
		}
		else
		{
			SetState(State_Surface);
		}
	}
	else if (GetTimeInState() > sm_Tunables.m_FollowTimeout)
	{
		SetState(State_Flee);
	}
	else
	{
		if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			SetState(State_Circle);
		}
		else
		{
			CPed* pPed = GetPed();

			// Enable surface sticking if close.
			CTaskFishLocomotion::CheckAndRespectSwimNearSurface(pPed);

			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
			{
				// Check if the target has moved, if so update the position of the goto point.		
				Vec3V vNewPosition = GenerateFollowLocation();
				if (IsGreaterThanAll(DistSquared(m_vCachedLungeLocation, vNewPosition), ScalarV(sm_Tunables.m_LungeChangeDistance * sm_Tunables.m_LungeChangeDistance)))
				{
					m_vCachedLungeLocation = vNewPosition;
					CTaskComplexControlMovement* pCCM = static_cast<CTaskComplexControlMovement*>(GetSubTask());
					pCCM->SetTarget(m_pTarget, VEC3V_TO_VECTOR3(m_vCachedLungeLocation));
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskSharkAttack::Circle_OnEnter()
{
	m_fCirclingCounter = 0.0f;
	
	CPed* pPed = GetPed();
	// Turn off avoidance.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SteersAroundPeds, false);

	float fRadius = Clamp(Dist(pPed->GetTransform().GetPosition(), m_pTarget->GetTransform().GetPosition()).Getf(), sm_Tunables.m_MinCircleRadius, sm_Tunables.m_MaxCircleRadius);
	float fAngularSpeed = sm_Tunables.m_CirclingAngularSpeed;
	CAITarget target;
	target.SetEntity(m_pTarget);
	// Circle wider if the target is in/on a vehicle.
	CTaskSharkCircle* pTaskCircle = rage_new CTaskSharkCircle(sm_Tunables.m_CirclingMBR, target, TargetInVehicle() ? fRadius * 2.0f : fRadius , fAngularSpeed);
	SetNewTask(rage_new CTaskComplexControlMovement(pTaskCircle));
}

CTask::FSM_Return CTaskSharkAttack::Circle_OnUpdate()
{
	const bool bTargetInVehicle = TargetInVehicle();
	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (bTargetInVehicle && !ChasingAStationaryVehicle())
	{
		SetState(State_Follow);
	}
	else if (!bTargetInVehicle)
	{	
		// Otherwise increment the circling counter.
		m_fCirclingCounter += GetTimeStep();
	}

	// When circling, stay near the surface if possible.
	CTaskFishLocomotion::CheckAndRespectSwimNearSurface(GetPed());

	if (m_fCirclingCounter > sm_Tunables.m_TimeToCircle)
	{
		if (ShouldDoAFakeLunge())
		{
			SetState(State_FakeLunge);
		}
		else
		{
			SetState(State_Dive);
		}
	}
	return FSM_Continue;
}

void CTaskSharkAttack::Dive_OnEnter()
{
	CPed* pPed = GetPed();
	Vec3V vPed = pPed->GetTransform().GetPosition();
	Vec3V vForward = pPed->GetTransform().GetForward();
	Vector3 vTarget(vPed.GetXf() + (vForward.GetXf() * sm_Tunables.m_DiveProjectionDistance), 
		vPed.GetYf() + (vForward.GetYf() * sm_Tunables.m_DiveProjectionDistance), vPed.GetZf() - sm_Tunables.m_DiveDepth);
	CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint(sm_Tunables.m_DiveMBR, vTarget, 0.5f);

	// Don't stick near the surface when diving.
	CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, false);

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
}

CTask::FSM_Return CTaskSharkAttack::Dive_OnUpdate()
{
	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (TargetInVehicle())
	{
		SetState(State_Follow);
	}
	else if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Check if we have achieved our target depth - if so lunge towards the target.
		SetState(State_Lunge);
	}
	return FSM_Continue;
}

// A place to lunge based on the target's current position.
// Slightly under the target and behind the target.
Vec3V_Out CTaskSharkAttack::GenerateLungeLocation() const
{
	Vec3V vPosition = m_pTarget->GetTransform().GetPosition();
	vPosition.SetZf(vPosition.GetZf() - sm_Tunables.m_LungeZOffset);

	Vec3V vForward = m_pTarget->GetTransform().GetForward();
	vForward = Scale(vForward, ScalarV(sm_Tunables.m_LungeForwardOffset));
	vPosition -= vForward;
	return vPosition;
}

// A place to head to when following based on the target's current position.
// Slightly under and behind.
Vec3V_Out CTaskSharkAttack::GenerateFollowLocation() const
{
	Vec3V vPosition = m_pTarget->GetTransform().GetPosition();
	vPosition.SetZf(vPosition.GetZf() - sm_Tunables.m_FollowZOffset);

	Vec3V vForward = m_pTarget->GetTransform().GetForward();
	vForward = Scale(vForward, ScalarV(sm_Tunables.m_FollowYOffset));
	vPosition -= vForward;

	// We never want to try to follow something out of the water.
	// The locomotion task is trying to stick to the waves, but it lerps to the right Z which takes a few frames.
	// It's possible for the shark to outrun the lerping, thus launching into the air.
	CapHeightForWater(vPosition);

	return vPosition;
}

void CTaskSharkAttack::FakeLunge_OnEnter()
{
	CPed* pShark = GetPed();

	// Compute a target point along the vector from the shark to the prey, but slightly past the prey.
	Vec3V vPrey = m_pTarget->GetTransform().GetPosition();
	Vec3V vShark = pShark->GetTransform().GetPosition();
	Vec3V vToPrey = vPrey - vShark;
	vToPrey = NormalizeSafe(vToPrey, Vec3V(V_ZERO));
	ScalarV vDist = Dist(vPrey, vShark) + ScalarV(sm_Tunables.m_FakeLungeOffset);

	Vec3V vPos = Scale(vToPrey, vDist) + vShark;

	// Adjust the target slightly to the right or left.
	Vec3V vSideOffset = m_pTarget->GetTransform().GetRight();
	if (fwRandom::GetRandomTrueFalse())
	{
		vPos += vSideOffset;
	}
	else
	{
		vPos -= vSideOffset;
	}

	CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_SPRINT, VEC3V_TO_VECTOR3(vPos), sm_Tunables.m_LungeTargetRadius);

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));

	// Note that we have attempted a fake lunge.
	m_iNumberOfFakeApproachesLeft -= 1;
}

CTask::FSM_Return CTaskSharkAttack::FakeLunge_OnUpdate()
{
	CPed* pPed = GetPed();

	// If possible, stick near the surface during this state.
	if (IsTargetUnderwater() && (m_pTarget->GetTransform().GetPosition().GetZf() < pPed->GetTransform().GetPosition().GetZf()))
	{
		CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, false);
	}
	else
	{
		CTaskFishLocomotion::CheckAndRespectSwimNearSurface(pPed);
	}

	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Circle);
	}

	// Apply extra steering to get the shark at the lunge location.
	if (!pPed->GetUsingRagdoll())
	{
		float fCurrentHeading = pPed->GetCurrentHeading();
		float fDesiredHeading = pPed->GetDesiredHeading();
		float fDelta = CTaskMotionBase::InterpValue(fCurrentHeading, fDesiredHeading, 1.0f, true);

		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDelta);
	}

	return FSM_Continue;
}

void CTaskSharkAttack::Lunge_OnEnter()
{
	m_vCachedLungeLocation = GenerateLungeLocation();
	Vector3 vTarget = VEC3V_TO_VECTOR3(m_vCachedLungeLocation);
	
	CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_SPRINT, vTarget, sm_Tunables.m_LungeTargetRadius);

	// Turn the shark invincible during it's final approach.
	GetPed()->m_nPhysicalFlags.bNotDamagedByAnything = true;

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
}

CTask::FSM_Return CTaskSharkAttack::Lunge_OnUpdate()
{
	if (ImminentCollisionWithShore())
	{
		SetState(State_Flee);
	}
	else if (TargetInVehicle())
	{
		SetState(State_Follow);
	}
	else if (!CanTryToKillTarget())
	{
		// The player is invincible.  Reset the number of times to do fake lunges and return to circling.
		m_iNumberOfFakeApproachesLeft = rage::round(fwRandom::GetRandomNumberInRange((float)sm_Tunables.m_MinNumberFakeApproaches, (float)sm_Tunables.m_MaxNumberFakeApproaches));
		SetState(State_Circle);
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_pTarget->GetUsingRagdoll())
		{
			// If the ped is already ragdolling then they won't be able to accept the sync scene task.
			// Go back to circling.
			SetState(State_Circle);
		}
		else
		{
			// Arrived at the target, play the animation and kill the prey.
			SetState(State_Bite);
		}
	}
	else if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CPed* pPed = GetPed();

		// If possible, stick near the surface during this state.
		if (IsTargetUnderwater() && (m_pTarget->GetTransform().GetPosition().GetZf() < pPed->GetTransform().GetPosition().GetZf()))
		{
			CTaskFishLocomotion::TogglePedSwimNearSurface(pPed, false);
		}
		else
		{
			CTaskFishLocomotion::CheckAndRespectSwimNearSurface(pPed);
		}

		Vec3V vNewPosition = GenerateLungeLocation();
		if (IsGreaterThanAll(DistSquared(m_vCachedLungeLocation, vNewPosition), ScalarV(sm_Tunables.m_LungeChangeDistance * sm_Tunables.m_LungeChangeDistance)))
		{
			m_vCachedLungeLocation = vNewPosition;
			CTaskComplexControlMovement* pCCM = static_cast<CTaskComplexControlMovement*>(GetSubTask());
			pCCM->SetTarget(m_pTarget, VEC3V_TO_VECTOR3(m_vCachedLungeLocation));
		}

		// Apply extra steering to get the shark at the lunge location.
		if (!pPed->GetUsingRagdoll())
		{
			float fCurrentHeading = pPed->GetCurrentHeading();
			float fDesiredHeading = pPed->GetDesiredHeading();
			float fDelta = CTaskMotionBase::InterpValue(fCurrentHeading, fDesiredHeading, 1.0f, true);

			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDelta);
		}
	}

	return FSM_Continue;
}

void CTaskSharkAttack::Lunge_OnExit()
{
	// Make the shark vulnerable again.
	GetPed()->m_nPhysicalFlags.bNotDamagedByAnything = false;
}

// Start the synched scene between the biter and the bitee.
void CTaskSharkAttack::Bite_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->SetNoCollision(m_pTarget, NO_COLLISION_NEEDS_RESET);

	// Turn the shark invincible during the bite.
	pPed->m_nPhysicalFlags.bNotDamagedByAnything = true;

	CreateSyncedScene();

	eSyncedSceneFlagsBitSet flags;
	flags.BitSet().Set(SYNCED_SCENE_USE_KINEMATIC_PHYSICS, true);
	flags.BitSet().Set(SYNCED_SCENE_DONT_INTERRUPT, true);
	flags.BitSet().Set(SYNCED_SCENE_PRESERVE_VELOCITY, true);

	if (m_SyncedSceneID != INVALID_SYNCED_SCENE_ID)
	{
		// TODO - SynchronizedScene takes a partial hash, which doesn't seem to like being passed atHashString objects which don't have c-strings in final builds.
		// The clip names would otherwise be able to be pulled into a static const somewhere, or preferably even reside in metadata.
		CTaskSynchronizedScene* pSharkTask = rage_new CTaskSynchronizedScene(m_SyncedSceneID, ATPARTIALSTRINGHASH("attack", 0xED1199FE), ms_SharkBiteDict, NORMAL_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, flags, CTaskSynchronizedScene::RBF_NONE, SLOW_BLEND_IN_DELTA);
		SetNewTask(pSharkTask);

		CTaskSynchronizedScene* pVictimTask = rage_new CTaskSynchronizedScene(m_SyncedSceneID, ATPARTIALSTRINGHASH("attack_player", 0x99F875AC), ms_SharkBiteDict, NORMAL_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, flags, CTaskSynchronizedScene::RBF_NONE, SLOW_BLEND_IN_DELTA);
		CEventGivePedTask sceneEvent(PED_TASK_PRIORITY_PRIMARY, pVictimTask);
		m_pTarget->GetPedIntelligence()->AddEvent(sceneEvent);

		if (m_pTarget->IsLocalPlayer())
		{
			CreateSyncedSceneCamera();
		}

		// Trigger VFX
		g_vfxBlood.TriggerPtFxBloodSharkAttack(m_pTarget);

		if(pPed->GetSpeechAudioEntity())
			pPed->GetSpeechAudioEntity()->TriggerSharkAttackSound(m_pTarget);
	}
}

// Kill the victim when the scene is finished playing.
CTask::FSM_Return CTaskSharkAttack::Bite_OnUpdate()
{
	if (!fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneID))
	{
		// Something went wrong, so abort out.
		SetState(State_Finish);
	}
	else
	{
		float fPhase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_SyncedSceneID);

		// Possibly cause rumble on the player's controller.
		ProcessRumble();

		// Possibly trigger ped damage effects.
		ProcessPedDamage();

		// Block ragdolling - if the ped ragdolled while the sync scene played it would end which looks quite bad.
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_NeverRagdoll, true);
		m_pTarget->SetPedResetFlag(CPED_RESET_FLAG_NeverRagdoll, true);

		if (fPhase >= 1.0f)
		{
			// The animation has finished playing.
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskSharkAttack::Bite_OnExit()
{
	CPed* pPed = GetPed();

	// Clear the flags so both peds can ragdoll this frame if they need to.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_NeverRagdoll, false);
	m_pTarget->SetPedResetFlag(CPED_RESET_FLAG_NeverRagdoll, false);

	if (Verifyf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedSceneID), "Invalid synced scene!"))
	{
		// Release the synced scene ref.
		fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedSceneID);
		m_SyncedSceneID = INVALID_SYNCED_SCENE_ID;

		// Release the camera if one was created.
		camInterface::GetSyncedSceneDirector().StopAnimatingCamera(camSyncedSceneDirector::TASK_SHARK_ATTACK_CLIENT, 2000); 

		// Kill the target.
		CDeathSourceInfo info(pPed);
		CTask* pTaskDie = rage_new CTaskDyingDead(&info, CTaskDyingDead::Flag_ragdollDeath);
		CEventGivePedTask deathEvent(PED_TASK_PRIORITY_PRIMARY, pTaskDie);
		m_pTarget->GetPedIntelligence()->AddEvent(deathEvent);
	}

	// Make the shark vulnerable again.
	pPed->m_nPhysicalFlags.bNotDamagedByAnything = false;
}

void CTaskSharkAttack::Flee_OnEnter()
{
	// Swim away - either from the shore or the ped we are trying to kill.
	CAITarget fleeTarget;
	if (m_bIsFleeingShore)
	{
		fleeTarget.SetPosition(m_vFleePosition);
	}
	else
	{
		fleeTarget.SetEntity(m_pTarget);
	}

	CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(fleeTarget, sm_Tunables.m_SharkFleeDist);
	pFleeTask->SetQuitTaskIfOutOfRange(true);
	pFleeTask->GetConfigFlags().SetFlag(CTaskSmartFlee::CF_NeverLeaveWater | CTaskSmartFlee::CF_FleeAtCurrentHeight);

	SetNewTask(pFleeTask);
}

CTask::FSM_Return CTaskSharkAttack::Flee_OnUpdate()
{
	// Swim near the surface if possible.
	CTaskFishLocomotion::CheckAndRespectSwimNearSurface(GetPed());

	// The shark was fleeing the player, but an obstruction happened while swimming away.
	// Change the target of the flee to be the shore by restarting this state.
	if (!m_bIsFleeingShore && ImminentCollisionWithShore())
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	// When the flee task ends, quit this task.
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskSharkAttack::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_Surface:				return "State_Surface";
		case State_Follow:				return "State_Follow";
		case State_Circle:				return "State_Circle";
		case State_Dive:				return "State_Dive";
		case State_FakeLunge:			return "State_FakeLunge";
		case State_Lunge:				return "State_Lunge";
		case State_Bite:				return "State_Bite";
		case State_Flee:				return "State_Flee";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

// A target can be lunged at if it is swimming or in the water.
bool CTaskSharkAttack::CanTargetBeLungedAt() const
{
	return m_pTarget->GetIsInWater() || m_pTarget->GetIsSwimming();
}

// A target is underwater if they are at least one meter below the surface.
bool CTaskSharkAttack::IsTargetUnderwater() const
{
	// In a boat.
	if (m_pTarget->GetIsInVehicle())
	{
		return false;
	}

	// Standing on a box.
	if (m_pTarget->GetGroundPhysical())
	{
		return false;
	}

	static const float s_fWaterHeightTolerance = 1.0f;
	const float fTargetZ = m_pTarget->GetTransform().GetPosition().GetZf();
	const float fHeight = m_pTarget->m_Buoyancy.GetAbsWaterLevel() - fTargetZ;

	return fHeight > s_fWaterHeightTolerance;
}

bool CTaskSharkAttack::ImminentCollisionWithShore()
{
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	Vector3 vStart = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	Vector3 vEnd = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetForward()) * sm_Tunables.m_LandProbeLength;
	vEnd += vStart;
	capsuleDesc.SetCapsule(vStart, vEnd, 0.4f);
	capsuleDesc.SetResultsStructure(&probeResults);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	capsuleDesc.SetContext(WorldProbe::EMovementAI);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		// Note the position of the impact so that the shark can flee this point.
		if (probeResults.GetNumHits() >= 1)
		{
			m_vFleePosition = probeResults[0].GetHitPosition();
			m_bIsFleeingShore = true;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskSharkAttack::TargetInVehicle() const
{
	// Check for being in a vehicle.
	if (m_pTarget->GetIsInVehicle())
	{
		return true;
	}

	// Check for entering in a vehicle, which we treat as in a vehicle
	// (i.e. we shouldn't attack while in this state)
	if (m_pTarget->GetIsEnteringVehicle(true))
	{
		return true;
	}

	// Similarly, check for the exit task.
	const bool bExitTaskRunning = m_pTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE, true);
	if (bExitTaskRunning)
	{
		return true;
	}

	// Check for standing on something, which we treat as in a vehicle.
	CPhysical* pPhysical = m_pTarget->GetGroundPhysical();
	if (pPhysical)
	{
		return true;
	}

	return false;
}

bool CTaskSharkAttack::ChasingAStationaryVehicle() const
{
	CVehicle* pVehicle = m_pTarget->GetVehiclePedInside();
	
	if (!pVehicle)
	{
		CPhysical* pPhysical = m_pTarget->GetGroundPhysical();
		if (pPhysical && pPhysical->GetIsTypeVehicle())
		{
			pVehicle = static_cast<CVehicle*>(pPhysical);
		}
	}

	if (!pVehicle)
	{
		return false;
	}
	else
	{
		Vector3 vVel = pVehicle->GetVelocity();
		if (vVel.Mag2() < sm_Tunables.m_MovingVehicleVelocityThreshold)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool CTaskSharkAttack::ShouldDoAFakeLunge() const
{
	return m_iNumberOfFakeApproachesLeft > 0;
}

bool CTaskSharkAttack::CanTryToKillTarget() const
{
	// Respect debug invulnerability.
	if (false NOTFINAL_ONLY(|| (m_pTarget->IsDebugInvincible())))
	{
		return false;
	}

	// If the player is outside the world extents, they can always be killed.
	if (m_pTarget->GetPlayerInfo() && m_pTarget->GetPlayerInfo()->HasPlayerLeftTheWorld())
	{
		return true;
	}

	// Respect cheat commands.
	if (m_pTarget->m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return false;
	}

	return true;
}

void CTaskSharkAttack::CreateSyncedScene()
{
	// Place the root at the victim's position and heading, offset downwards slightly to prevent waves causing the scene to play out above the waterline.
	CPed* pPed = m_pTarget;
	Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vPos.z -= 0.9f;
	Vector3 vEulers(0.0f, 0.0f, pPed->GetCurrentHeading());
	Quaternion qRot;
	qRot.FromEulers(vEulers, "xyz");

	fwSyncedSceneId sceneID = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();

	if (Verifyf(sceneID != INVALID_SYNCED_SCENE_ID, "Unable to create new synchronized scene! Synced scene pool may be full."))
	{
		// Center it.
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(sceneID, vPos, qRot);
		
		// Increment ref count
		fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(sceneID);

		// Set it up to play once and then end.
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(sceneID, false);
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneHoldLastFrame(sceneID, false);
	}
	m_SyncedSceneID = sceneID;
}

void CTaskSharkAttack::CreateSyncedSceneCamera()
{
	const s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(ms_SharkBiteDict).Get();
	const crClip* clip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex, ms_CameraAnimation.GetHash());
	if (taskVerifyf(clip, "Couldn't find camera animation for shark attack synced scene"))
	{
		// Animate the camera as part of the synced scene.
		Matrix34 sceneOrigin;
		fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(m_SyncedSceneID, sceneOrigin);
				
		camInterface::GetSyncedSceneDirector().AnimateCamera(ms_SharkBiteDict, *clip, sceneOrigin, m_SyncedSceneID, 0 ,camSyncedSceneDirector::TASK_SHARK_ATTACK_CLIENT); 
	}
}

// Possibly apply ped damage based on the phase.
void CTaskSharkAttack::ProcessPedDamage()
{
	if (m_bHasAppliedPedDamage)
	{
		return;
	}

	fwAnimDirector* pDirector = GetPed()->GetAnimDirector();
	float fPhase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_SyncedSceneID);

	if (pDirector && pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>())
	{
		const crClip* pClip = pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->GetSyncedSceneClip();
		if (pClip)
		{
			float fDamagePhase = 1.0f;
			if(CClipEventTags::FindEventPhase(pClip, CClipEventTags::SharkAttackPedDamage, fDamagePhase))
			{
				if (fPhase >= fDamagePhase)
				{
					// Add ped damage.
					PEDDAMAGEMANAGER.AddPedDamagePack(m_pTarget, "SCR_SHARK", 0.0f, 1.0f);

					// Note that we have applied ped damage.
					m_bHasAppliedPedDamage = true;
				}
			}
		}
	}
}
// Possibly apply pad rumble based on the phase.
void CTaskSharkAttack::ProcessRumble()
{
	if (m_bHasAppliedPedDamage)
	{
		return;
	}

	fwAnimDirector* pDirector = GetPed()->GetAnimDirector();
	float fPhase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_SyncedSceneID);

	if (pDirector && pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>())
	{
		const crClip* pClip = pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->GetSyncedSceneClip();
		if (pClip)
		{
			float fDamagePhase = 1.0f;
			if(CClipEventTags::FindEventPhase(pClip, CClipEventTags::SharkAttackPedDamage, fDamagePhase))
			{
				if (fPhase >= fDamagePhase)
				{
					CControlMgr::StartPlayerPadShakeByIntensity(1200, 0.5f);
				}
			}
		}
	}
}

// Cap the Z-position of a target based on the water height.
void CTaskSharkAttack::CapHeightForWater(Vec3V_InOut vPosition)
{
	float fWaterHeight = vPosition.GetZf();

	if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(RCC_VECTOR3(vPosition), &fWaterHeight, true))
	{
		vPosition.SetZf(Min(vPosition.GetZf(), fWaterHeight));
	}
}

/////////////////////////////////////////////////////////////////////////////
// Clone task implementation.
// Sharks aren't networked yet but they could be in the future,
// this is mainly being done to get the queriable interface working with it.
/////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskSharkAttack::CreateQueriableState() const
{
	CTaskInfo* pInfo = rage_new CClonedTaskSharkAttackInfo(GetState(), m_pTarget);
	return pInfo;
}

void CTaskSharkAttack::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SHARK_ATTACK);
	CClonedTaskSharkAttackInfo* pSharkAttackInfo = static_cast<CClonedTaskSharkAttackInfo*>(pTaskInfo);

	m_pTarget = static_cast<CPed*>(pSharkAttackInfo->GetTarget());

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskFSMClone* CTaskSharkAttack::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskSharkAttack* pTask = rage_new CTaskSharkAttack(m_pTarget);

	pTask->SetState(GetState());

	return pTask;
}

CTaskFSMClone* CTaskSharkAttack::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

CTask::FSM_Return CTaskSharkAttack::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Task info for shark attack.
/////////////////////////////////////////////////////////////////////////////////////////////
CClonedTaskSharkAttackInfo::CClonedTaskSharkAttackInfo(s32 iState, CPed* pTargetPed)
: m_TargetPed(pTargetPed)
{
	SetStatusFromMainTaskState(iState);
}

CTaskFSMClone* CClonedTaskSharkAttackInfo::CreateCloneFSMTask()
{
	CTaskSharkAttack* pAttackTask = rage_new CTaskSharkAttack(m_TargetPed.GetPed());
	return pAttackTask;
}
