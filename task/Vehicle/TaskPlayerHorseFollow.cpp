#include "Task/Vehicle/TaskPlayerHorseFollow.h"

#if ENABLE_HORSE

#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Movement/TaskGotoPoint.h"
#include "task/Vehicle/PlayerHorseSpeedMatch.h"
#include "task/Motion/TaskMotionBase.h"
#include "task/Vehicle/HelperFunctions.h"
#include "vector/vector3.h"
#include "Vehicles/vehicle.h"
#include "Peds/Ped.h"
#include "Task/Vehicle/HelperFunctions.h"
#include "Peds\Horse\horseComponent.h"
#include "Peds/Horse/horseSpeed.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Peds/PedIntelligence.h"

AI_OPTIMISATIONS()


CTaskPlayerHorseFollow::CTaskPlayerHorseFollow()
{
	m_WasAlongSideTarget = false;
	m_WasMatching = false;
	m_CurrentSpeedMatchSpeedNormalized = -1.0f;
	m_fDistTargetAhead = 0.f;

	SetInternalTaskType(CTaskTypes::TASK_PLAYER_HORSE_FOLLOW);
}

CTaskPlayerHorseFollow::~CTaskPlayerHorseFollow()
{
}

aiTask::FSM_Return CTaskPlayerHorseFollow::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if (!pPlayerInfo)
		return FSM_Quit;

	CPlayerHorseSpeedMatch* pInputHorse = pPlayerInfo->GetInputHorse();

	//has the user met the button requirements to begin following?
	m_FollowReady = pInputHorse->IsNewHorseFollowReady();

	//tell the player we don't have a target
	pInputHorse->ResetCurrentHorseFollowTarget();

	ResetCurrentSpeedMatchSpeedNormalized();

	//look for a target to match with
	if (!SelectIdealTarget(m_CurrentTarget, m_WasMatching))
	{
		m_CurrentTarget.Reset();
		ResetLastTarget();
		return FSM_Continue;
	}
	
	m_WasMatching = m_FollowReady;

	//tell the player we have a target to match with
	pInputHorse->SetCurrentHorseFollowTarget(m_CurrentTarget.target);

	return FSM_Continue;
}

aiTask::FSM_Return CTaskPlayerHorseFollow::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_NoTarget)
			FSM_OnUpdate
				return StateNoTarget_OnUpdate();

		FSM_State(State_OnlyMatchSpeed)
			FSM_OnEnter
				return StateOnlyMatchSpeed_OnEnter();
			FSM_OnUpdate
				return StateOnlyMatchSpeed_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return StateFinish_OnUpdate();

	FSM_End
}

aiTask::FSM_Return CTaskPlayerHorseFollow::ProcessPostFSM()
{
	CPed* pPed = GetMovementTaskPed();

	//set this last, since SetupParamForFollowObject/AlongPath needs to compare 
	//currentTarget with m_LastTarget
	m_LastTarget = m_CurrentTarget;

	s32 state = GetState();
	if ((state != State_NoTarget) && (state != State_Finish))
	{
		CTask* pTask = GetMovementTask();
		if (pTask && pTask->IsMoveTask())
		{
			CTaskMove* pTaskMove = static_cast<CTaskMove*>(pTask);
			pTaskMove->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vNavTarget));
			pTaskMove->SetLookAheadTarget(pPed, m_vNavTarget);
		}
		else
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

bool CTaskPlayerHorseFollow::ShouldAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* UNUSED_PARAM(pEvent))
{
	ResetLastTarget();
	m_OnlyMatchSpeed = false;
	ResetCurrentSpeedMatchSpeedNormalized();

	return true;
}

s32 CTaskPlayerHorseFollow::GetDesiredState() const
{
	if (m_CurrentTarget.target)
	{
		if (m_OnlyMatchSpeed)
		{
			return State_OnlyMatchSpeed;
		}
	}
	else
	{
		if (!m_FollowReady)
		{
			return State_Finish;
		}
	}

	return State_NoTarget;
}

aiTask::FSM_Return CTaskPlayerHorseFollow::StateNoTarget_OnUpdate()
{
	s32 desiredState = GetDesiredState();
	if (desiredState != State_NoTarget)
	{
		SetState(desiredState);
	}
	return FSM_Continue;
}

aiTask::FSM_Return CTaskPlayerHorseFollow::StateOnlyMatchSpeed_OnEnter()
{
	CreateMoveTask(CTaskTypes::TASK_MOVE_GO_TO_POINT);
	return FSM_Continue;
}

aiTask::FSM_Return CTaskPlayerHorseFollow::StateOnlyMatchSpeed_OnUpdate()
{
	s32 desiredState = GetDesiredState();
	if (desiredState == State_OnlyMatchSpeed)
	{
		//not sure.  just set everything up like we're doing a move goto
		//but don't call into the behavior, just set up our speed in the packet.  hacky!
		SetupParamForMoveGoto(m_CurrentTarget);
		SetupSpeed(m_CurrentTarget.target);
	}
	else
	{
		SetState(desiredState);
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskPlayerHorseFollow::StateFinish_OnUpdate()
{
	ResetCurrentSpeedMatchSpeedNormalized();
	return FSM_Quit;
}

bool HelperGetTargetMatrix(const CPhysical* target, Matrix34& mtrxOut)
{
	Assert(target);
	if (!target)
		return false;

	mtrxOut = MAT34V_TO_MATRIX34(target->GetMatrix());

	Vector3 y = mtrxOut.b;

	float m2 = y.XYMag2();
	if(m2 >= SMALL_FLOAT)
	{
		const float invm = invsqrtf(m2);

		const float xx = y.x*invm;
		const float yy = y.y*invm;

		mtrxOut.a.Set( yy, -xx, 0.f);
		mtrxOut.b.Set( xx,  yy, 0.f);
		mtrxOut.c.Set(0.f, 0.f, 1.f);

		return true;
	}

	return false;
}

bool HelperTargetIsTrain(const CPhysical* target)
{
	Assert(target);
	if (target && target->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(target);
		return pVehicle->InheritsFromTrain();
	}

	return false;
}

// Y-offset is negated!  y > 0 means we are behind the target.
bool CTaskPlayerHorseFollow::HelperShouldFollowAlongSide(float offsAlongXAxis, float offsAlongYAxis, CPlayerHorseFollowTargets::FollowTargetEntry& target)
{
	//if we're in front of the target, don't choose to follow behind
	if (offsAlongYAxis < 0.5f)
	{
		return true;
	}

	//we should never try to follow trains from behind
	if (HelperTargetIsTrain(target.target))
	{
		return true;
	}

	//otherwise, use some hysteresis to determine to follow alongside or not
	const float slope = offsAlongYAxis * 0.25f;

	if (m_WasAlongSideTarget && target.target == m_LastTarget.target)
	{
		//we were previously following this target alongside, favor the sides
		return fabsf(offsAlongXAxis) > (fabsf(target.offsetX * 0.25f) + slope)
			|| offsAlongYAxis < 1.5f;
	}
	else if (target.target == m_LastTarget.target)
	{
		//we were previously following this target behind, favor behind
		return fabsf(offsAlongXAxis) > (fabsf(target.offsetX * 0.95f) + slope);
	}
	else
	{
		//new target, just choose the ideal position
		return fabsf(offsAlongXAxis) > (fabsf(target.offsetX * 0.75f) + slope);
	}
}

// Y-offset is negated!  y > 0 means we are behind the target.
void CTaskPlayerHorseFollow::UpdateFollowFormation(CPlayerHorseFollowTargets::FollowTargetEntry& target
	, float& offsetXOut, float& offsetYOut, float& offsAlongYAxisOut, float baseOffsetX)
{
	CPed* pPed = GetMovementTaskPed();
	Vector3 myPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Matrix34 tgtMatrix;

	//the target needs to have a valid matrix in order for us to know where we are
	//relative to it
	const bool gotTgtMatrix = HelperGetTargetMatrix(target.target, tgtMatrix);

	Vector3 delta;
	delta.Subtract(myPosition, tgtMatrix.d);
	const float offsAlongXAxis = delta.Dot(tgtMatrix.a);
	// Minus is needed to make the old code work
	const float offsAlongYAxis = -delta.Dot(tgtMatrix.b);

	offsAlongYAxisOut = offsAlongYAxis;

	bool shouldFollowAlongSide = gotTgtMatrix && target.mode == CPlayerHorseFollowTargets::kModeSideOnly;

	if (gotTgtMatrix && target.mode == CPlayerHorseFollowTargets::kModeAuto)
	{
		//check our current position to determine if we should follow behind or alongside
		if (HelperShouldFollowAlongSide(offsAlongXAxis, offsAlongYAxis, target))
		{
			shouldFollowAlongSide = true;
		}
	}

	//if we think we should follow alongside, but aren't in a position to,
	//set our x offset to the default x offset and y offset to the behind offset,
	//so we fall back along the side and then shift over, instead of shifting to the middle
	//while we may be in front/next to the target
	const bool shouldFallBack = target.mode == CPlayerHorseFollowTargets::kModeBehindAndSide
		|| (!shouldFollowAlongSide && offsAlongYAxis < target.offsetY * 0.75f);

	if (shouldFallBack)
	{
		offsetYOut = target.offsetY;
	}
	else if (shouldFollowAlongSide)
	{
		offsetYOut = 0.25f;
	}
	else	//I guess it's safest to default to behind following
	{
		offsetYOut = target.offsetY;
	}

	offsetXOut = offsAlongXAxis > 0.0f ? target.offsetX : -target.offsetX;

	//if the target is following his path at an x offset, account for it here
	offsetXOut += baseOffsetX;

	m_WasAlongSideTarget = shouldFollowAlongSide;
}

void CTaskPlayerHorseFollow::SetupParamForMoveGoto(CPlayerHorseFollowTargets::FollowTargetEntry& target)
{
	Matrix34 tgtMatrix;

	Assert(target.target);

	Vector3 dest, offset;//, fakeDestHorseSpace;

	// 	//the target needs to have a valid matrix in order for us to know where we are
	// 	//relative to it
	HelperGetTargetMatrix(target.target, tgtMatrix);

	//Assert(gotTgtMatrix);

	float offsAlongYAxis = -1.0f;

	UpdateFollowFormation(target, offset.x, offset.y, offsAlongYAxis);

	//initialize z position
	offset.z = 0.0f;

	m_fDistTargetAhead  = offsAlongYAxis - offset.y;

	//if our target has a velocity, extend our target position in that direction
	Vector3 tgtVel = target.target->GetVelocity();

	//if our target's velocity is slow enough, fall back to its forward axis
	if (tgtVel.Mag2() > 0.1f)
	{
		// y offset is back to RDR3 to make Transform work correctly
		offset.y = -offset.y;

		//translate offset to worldspace
		tgtMatrix.Transform(offset, dest);

#if DEBUG_DRAW
		m_vDesiredNavTarget = dest;
#endif

		tgtVel.z = 0.0f;
		tgtVel.NormalizeSafe(tgtMatrix.b);
		dest += tgtVel*10.0f;
	}
	else	//otherwise use the target's forward direction
	{
		offset.y -= 10.0f;	//stick the destination out in front of the target

		// y offset is back to RDR3 to make Transform work correctly
		offset.y = -offset.y;

		//translate offset to worldspace
		tgtMatrix.Transform(offset, dest);
	}

	m_vNavTarget = dest;
}

void CTaskPlayerHorseFollow::ResetLastTarget()
{
	m_WasAlongSideTarget = false;
	m_LastTarget.Reset();
	m_WasMatching = false;
}

void CTaskPlayerHorseFollow::SetupSpeed(const CPhysical* target)
{
	const CPed* pPed = GetMovementTaskPed();
	const float fSpeed = HelperFunctions::UpdateSpeedMatchSimple(m_fDistTargetAhead, pPed, target);
	const float fNormalizedSpeed = HelperFunctions::ComputeNormalizedSpeed(pPed, fSpeed);
	const float fClampedNormalizedSpeed = HelperFunctions::ClampNormalizedSpeed(fNormalizedSpeed, pPed, target);
	m_CurrentSpeedMatchSpeedNormalized = fClampedNormalizedSpeed;
}

bool IsVehicleOrRidingVehicle(const CPhysical* pTarget)
{
	if (pTarget->GetIsTypeVehicle())
		return true;

	if (!pTarget->GetIsTypePed())
		return false;

	const CPed* pPed = static_cast<const CPed*>(pTarget);
	return (pPed->GetVehiclePedInside() != NULL);
}

bool IsRidingOrRideable(const CPhysical* pTarget)
{
	if (!pTarget->GetIsTypePed())
		return false;

	const CPed* pPed = static_cast<const CPed*>(pTarget);
	return (pPed->GetMountPedOn() != NULL) || (pPed->GetHorseComponent() != NULL);
}

float HelperGetIdealXOffsetForTarget(CPlayerHorseFollowTargets::FollowTargetEntry& target)
{
	//determine the correct lateral offset
	const CPhysical* pTarget = target.target;
	if (pTarget)
	{
		if (HelperTargetIsTrain(pTarget))
		{
			return 4.5f;
		}
		else if (IsVehicleOrRidingVehicle(pTarget))
		{
			return 3.0f;
		}
		else if (IsRidingOrRideable(pTarget))
		{
			return 1.75f;
		}
		else
		{
			return 1.5f;
		}
	}

	//TODO:  if we're here, that means we're following a non-actor object
	//come up with a better default
	return 1.5f;
}

float HelperGetIdealYOffsetForTarget(CPlayerHorseFollowTargets::FollowTargetEntry& target)
{
	//determine the correct rear offset
	const CPhysical* pTarget = target.target;
	if (pTarget)
	{
		if (HelperTargetIsTrain(pTarget))
		{
			return 0.0f;
		}
		else if (IsVehicleOrRidingVehicle(pTarget) && target.mode ==  CPlayerHorseFollowTargets::kModeBehindClose)
		{
			return 6.0f;
		}
		else if (IsVehicleOrRidingVehicle(pTarget))
		{
			return 10.0f;
		}
		else if (IsRidingOrRideable(pTarget) && target.mode == CPlayerHorseFollowTargets::kModeBehindClose)
		{
			return 2.5f;
		}
		else if (IsRidingOrRideable(pTarget))
		{
			return 6.0f;
		}
		else if (target.mode == CPlayerHorseFollowTargets::kModeBehindClose)
		{
			return 2.0f;
		}
		else
		{
			return 3.0f;
		}
	}

	//TODO:  if we're here, that means we're following a non-actor object
	//come up with a better default
	return 4.0f;
}

bool CTaskPlayerHorseFollow::SelectIdealTarget(CPlayerHorseFollowTargets::FollowTargetEntry& targetOut, bool hadTarget)
{
	CPlayerInfo* pPlayerInfo = GetPed()->GetPlayerInfo();
	if (!pPlayerInfo)
		return false;

	CPed* pPed = GetMovementTaskPed();

	//Get a reference to our data structure that holds info about our possible targets
	CPlayerHorseFollowTargets& targetList = pPlayerInfo->GetInputHorse()->GetHorseFollowTargets();

	if (targetList.GetNumTargets() == 0)
	{
		return false;
	}

	bool foundTarget = false;
	CPlayerHorseFollowTargets::FollowTargetEntry currentBest;
	Matrix34 tempMatrix;
	float tempDistance2 = FLT_MAX;
	Vector3 myPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	float targetScores[CPlayerHorseFollowTargets::kMaxFollowTargets];

	const CPhysical* lastTarget = GetLastTarget();

	for (int i = 0; i < targetList.GetNumTargets(); ++i)
	{
		//initialize this score
		targetScores[i] = 0.0f;

		const CPhysical* tempTarget = targetList.GetTarget(i).target;
		if (!tempTarget)
		{
			targetList.RemoveTarget(i);
			--i;	//so we evaluate the element that gets put in our current slot next time around
			continue;
		}

		//Get the position of the current follow candidate
		tempMatrix = MAT34V_TO_MATRIX34(tempTarget->GetMatrix());

		//if we're supposed to check if our target is mounted, do it here
		if (targetList.GetTarget(i).disableWhenNotMounted)
		{
			if (HelperIsDismounted(targetList.GetTarget(i).target))
			{
				continue;
			}
		}

		tempDistance2 = myPos.Dist2(tempMatrix.d);

		//use a larger distance check for targets that we are already following
		const bool isNewTarget = !lastTarget || tempTarget != lastTarget || !hadTarget;
		
		TUNE_GROUP_FLOAT(HORSE_FOLLOW, fFollowActivationRange, 40.f, 0.f, 50.f, 1.f);
		TUNE_GROUP_FLOAT(HORSE_FOLLOW, fFollowOuterRange, 80.f, 0.f, 100.f, 1.f);
		const float activationRange = isNewTarget ? fFollowActivationRange : fFollowOuterRange;

		if (tempDistance2 > activationRange * activationRange)
		{
			continue;
		}

		Vector3 delta;
		delta.Subtract(tempMatrix.d, myPos);

		const float offsAlongYAxis = delta.Dot(tempMatrix.b);
		const bool isTargetInFrontOfPlayer = offsAlongYAxis < 1.0f;	//extra 1 meter padding;

		//score the target based on 
		//	--its priority
		//	--whether or not it is in front of the horse
		//	--how close it is to the target
		//	--give a boost to the target we followed last
		targetScores[i] = 10000.0f * targetList.GetTarget(i).priority 
			+ 1000.0f * isTargetInFrontOfPlayer 
			+ 100.0f * (1.0f / Max(tempDistance2, 0.01f))
			+ 5.0f * !isNewTarget;

		foundTarget = true;	
	}

	if (!foundTarget)
	{
		return false;
	}

	int bestTargetIndex = CPlayerHorseFollowTargets::kMaxFollowTargets;
	float bestTargetScore = 0.0f;

	for (int i = 0; i < targetList.GetNumTargets(); ++i)
	{
		if (targetScores[i] > bestTargetScore)
		{
			bestTargetIndex = i;
			bestTargetScore = targetScores[i];
		}
	}

	AssertMsg(bestTargetIndex != CPlayerHorseFollowTargets::kMaxFollowTargets, "We found a target but for some reason didn't score it");
	currentBest = targetList.GetTarget(bestTargetIndex);

	//We've got the target we want, now clean up his X and Z offsets in case the user had specified a negative value,
	//meaning he wants us to figure it out
	if (currentBest.offsetX < 0.0f)
	{
		currentBest.offsetX = HelperGetIdealXOffsetForTarget(currentBest);
	}
	if (currentBest.offsetY < 0.0f)
	{
		currentBest.offsetY = HelperGetIdealYOffsetForTarget(currentBest);
	}

	targetOut = currentBest;

	return true;
}

void CTaskPlayerHorseFollow::CreateMoveTask(int iSubTaskType)
{
	CPed* pPed = GetMovementTaskPed();

	CTaskMove* pTaskMove;
	switch (iSubTaskType)
	{
	case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			const fwTransform& trans = pPed->GetTransform();
			Vec3V target = trans.GetPosition() + ScalarV(V_TEN) * trans.GetForward();
			// Debug
			pTaskMove = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_RUN, VEC3V_TO_VECTOR3(target));

			CTaskMoveGoToPoint* pTaskMoveGotoPoint = static_cast<CTaskMoveGoToPoint*>(pTaskMove);

			pTaskMoveGotoPoint->SetAllowNavmeshAvoidance(true);
			pTaskMoveGotoPoint->SetDistanceFromPathToEnableNavMeshAvoidance(0.0f); 
		}
		break;

	default:
		Assert(false);
		return;
	}

	CTask* pSubTask = rage_new CTaskComplexControlMovement(pTaskMove);
	SetNewTask(pSubTask);
}

CPed* CTaskPlayerHorseFollow::GetMovementTaskPed()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();
	return pMount ? pMount : pPed;
}

const CPed* CTaskPlayerHorseFollow::GetMovementTaskPed() const
{
	const CPed* pPed = GetPed();
	const CPed* pMount = pPed->GetMyMount();
	return pMount ? pMount : pPed;
}

CTask* CTaskPlayerHorseFollow::GetMovementTask()
{
	CPed* pPed = GetMovementTaskPed();
	return pPed->GetPedIntelligence()->GetGeneralMovementTask();
}

#if !__FINAL
void CTaskPlayerHorseFollow::Debug() const
{
#if DEBUG_DRAW
	const CPed* pPed = GetPed();
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if (taskVerifyf(pPlayerInfo, "No player info in CTaskPlayerHorseFollow::Debug()"))
	{
		//Get a reference to our data structure that holds info about our possible targets
		CPlayerHorseFollowTargets& targetList = pPlayerInfo->GetInputHorse()->GetHorseFollowTargets();

		if (targetList.GetNumTargets() > 0)
		{
			const CPhysical* lastTarget = GetLastTarget();

			//iterate through every target in our list and draw a something around them
			for (int i=0; i < targetList.GetNumTargets(); ++i)
			{
				const CPhysical* thisTarget = targetList.GetTarget(i).target;
				if (thisTarget)
				{
					Vector3 thisPosition = VEC3V_TO_VECTOR3(thisTarget->GetTransform().GetPosition());
					Color32 color;
					if (thisTarget == lastTarget)
					{
						color = Color32(1.0f, 0.0f, 0.0f, 1.0f);
					}
					else
					{
						color = Color32(0.0f, 1.0f, 0.0f, 1.0f);
					}
					grcDebugDraw::Sphere(thisPosition, 0.25f, color);
				}
			}
		}
	}

	grcDebugDraw::Sphere(m_vDesiredNavTarget, 0.25f, Color32(0.0f, 0.0f, 1.0f, 1.0f));
	grcDebugDraw::Text(m_vDesiredNavTarget, Color32(0.0f, 0.0f, 1.0f, 1.0f), "X_Target");

	const CPhysical* pTarget = m_CurrentTarget.target.Get();
	if (pTarget)
	{
		Vec3V pedPos = pPed->GetTransform().GetPosition();
		char str[100];
		int y = 10;

		Vec3V targetPos = pTarget->GetTransform().GetPosition();
		formatf(str, "Following: %s (%.2f, %.2f, %.2f)", pTarget->GetBaseModelInfo()->GetModelName(), VEC3V_ARGS(targetPos));
		grcDebugDraw::Text(pedPos, Color_white, 0, y, str);
		y += 10;

		formatf(str, "m_fDistTargetAhead: %.2f", m_fDistTargetAhead);
		grcDebugDraw::Text(pedPos, Color_white, 0, y, str);
		y += 10;

		{
			float pedSpeed = Mag(VECTOR3_TO_VEC3V(pPed->GetVelocity())).Getf();
			float pedNormSpeed = HelperFunctions::ComputeNormalizedSpeed(pPed, pedSpeed);
			const char* pedGait = HelperFunctions::GetGaitName(pedNormSpeed);
			formatf(str, "Ped: %5.2f m/s (%.2f, %s)", pedSpeed, pedNormSpeed, pedGait);
			grcDebugDraw::Text(pedPos, Color_white, 0, y, str);
		}
		y += 10;

		{
			float tgtSpeed = Mag(VECTOR3_TO_VEC3V(pTarget->GetVelocity())).Getf();
			if (pTarget->GetIsTypePed())
			{
				const CPed* pTargetPed = static_cast<const CPed*>(pTarget);
				float tgtNormSpeed = HelperFunctions::ComputeNormalizedSpeed(pTargetPed, tgtSpeed);
				const char* tgtGait = HelperFunctions::GetGaitName(tgtNormSpeed);
				formatf(str, "Tgt: %5.2f m/s (%.2f, %s)", tgtSpeed, tgtNormSpeed, tgtGait);
			}
			else
			{
				formatf(str, "Tgt: %5.2f m/s", tgtSpeed);
			}
			grcDebugDraw::Text(pedPos, Color_white, 0, y, str);
		}
		y += 10;

		{
			const CPed* pMovementTaskPed = GetMovementTaskPed();
			float speed = HelperFunctions::UpdateSpeedMatchSimple(m_fDistTargetAhead, pMovementTaskPed, m_CurrentTarget.target);
			float normSpeed = HelperFunctions::ComputeNormalizedSpeed(pPed, speed);
			float clampedNormSpeed = HelperFunctions::ClampNormalizedSpeed(normSpeed, pMovementTaskPed, pTarget);
			const char* gait = HelperFunctions::GetGaitName(clampedNormSpeed);
			formatf(str, "Sgt: %.2f m/s (use %.2f, %s)", speed, clampedNormSpeed, gait);
			grcDebugDraw::Text(pedPos, Color_white, 0, y, str);
		}
		y += 10;
	}
#endif	// #if DEBUG_DRAW

	const CTask* pSubTask = GetSubTask();
	if (pSubTask)
	{
		pSubTask->Debug();
	}
}

const char* CTaskPlayerHorseFollow::GetStaticStateName(s32 iState)
{
	switch (iState)
	{
	case State_NoTarget:				return "No_Target";
	case State_OnlyMatchSpeed:			return "Only_Match_Speed";
	case State_Finish:					return "Finish";
	}

	taskAssert(false);
	return "Invalid!";
}
#endif	// #if !__FINAL

#endif