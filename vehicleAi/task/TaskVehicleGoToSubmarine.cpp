#include "TaskVehicleGoToSubmarine.h"

// Game headers
#include "Vehicles/Submarine.h"
#include "debug/DebugScene.h"
#include "scene/world/gameWorld.h"
#include "peds/Ped.h"
#include "physics\PhysicsHelpers.h"
#include "system/ControlMgr.h"
#include "vehicleAi\task\TaskVehiclePark.h"
#include "vehicleai/task/taskvehicleanimation.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

CTaskVehicleGoToSubmarine::Tunables CTaskVehicleGoToSubmarine::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleGoToSubmarine, 0xb28def4b);

CTaskVehicleGoToSubmarine::CTaskVehicleGoToSubmarine( const sVehicleMissionParams& params, const u32 iSubFlags, s32 minHeightAboveTerrain, float RequestedOrientation) :
CTaskVehicleGoTo(params),
m_avoidanceTestResults(),
m_downTestResults(),
m_iSubFlags(iSubFlags),
m_minHeightAboveTerrain(minHeightAboveTerrain),
m_fDistToForwardObject(1000.0f),
m_fLocalWaterHeight(0.0f),
m_fSlowDownDistance(sm_Tunables.m_slowdownDist),
m_fSlowDownDistanceMin(5.0f),
m_fRequestedOrientation(RequestedOrientation),
m_iNumDownProbesUsed(s_NumDownProbes)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE);

	m_avoidanceTestResults = rage_new WorldProbe::CShapeTestSingleResult[AD_COUNT];
	m_downTestResults = rage_new WorldProbe::CShapeTestSingleResult[s_NumDownProbes];

#if DEBUG_DRAW
	probeDesc = rage_new WorldProbe::CShapeTestCapsuleDesc[AD_COUNT];
	probeHits = rage_new CTaskVehicleGoToSubmarine::debugHitData[AD_COUNT];
	downProbeHits = rage_new CTaskVehicleGoToSubmarine::debugHitData[s_NumDownProbes];
	heightMapProbe = rage_new WorldProbe::CShapeTestProbeDesc[s_NumDownProbes];
#endif
}

aiTask* CTaskVehicleGoToSubmarine::Copy() const
{
	//Create the task.
	CTaskVehicleGoToSubmarine* pTask = rage_new CTaskVehicleGoToSubmarine(m_Params, m_iSubFlags, m_minHeightAboveTerrain);
	pTask->SetSlowDownDistance(m_fSlowDownDistance);
	return pTask;
}

CTaskVehicleGoToSubmarine::~CTaskVehicleGoToSubmarine()
{
	delete[] m_avoidanceTestResults;
	delete[] m_downTestResults;

#if DEBUG_DRAW
	delete[] probeDesc;
	delete[] probeHits;
	delete[] downProbeHits;
	delete[] heightMapProbe;	
#endif
}

void CTaskVehicleGoToSubmarine::CleanUp()
{
	CVehicle *pVehicle = GetVehicle();
	//Clear dive control when we cleanup.
	if (pVehicle && pVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINE)
	{
		CSubmarineHandling* subHandling = pVehicle->GetSubHandling();
		if(subHandling)
		{
			subHandling->SetDiveControl(0.0f);
		}

		// Script can run this task without a pilot via a parameter in TASK_SUBMARINE_GOTO_AND_STOP. We need to clear this off the vehicle when any other task is set.
		if(m_iSubFlags & SF_HoverAtEnd)
		{
			pVehicle->GetIntelligence()->SetUsingScriptAutopilot(false);
		}
	}
}

CTask::FSM_Return CTaskVehicleGoToSubmarine::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle();

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pVehicle);
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);
		FSM_State(State_GoTo)
			FSM_OnEnter
				Goto_OnEnter(pVehicle);
			FSM_OnUpdate
				return Goto_OnUpdate(pVehicle);
		FSM_State(State_Rotate)
				FSM_OnEnter
				Rotate_OnEnter(pVehicle);
			FSM_OnUpdate
				return Rotate_OnUpdate(pVehicle);
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);
	FSM_End
}

void CTaskVehicleGoToSubmarine::Rotate_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	m_fRequestedOrientation = fwAngle::LimitRadianAngle0to2PiSafe(m_fRequestedOrientation);
	SetNewTask(rage_new CTaskVehicleStop());
}

CTask::FSM_Return CTaskVehicleGoToSubmarine::Rotate_OnUpdate(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		const Vector3   vehDriveDir (VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
		CSubmarineHandling* subHandling = pVehicle->GetSubHandling();

		const float fCurrentOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
		const float fOrientationDelta = fwAngle::LimitRadianAngleSafe(m_fRequestedOrientation - fCurrentOrientation);
		if(fOrientationDelta < 0.05f)
		{
			if(!IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved))
			{
				m_bMissionAchieved = true;
				SetState(State_Stop);
				return FSM_Continue;
			}
			else if(m_iSubFlags & SF_HoverAtEnd)
			{
				SetState(State_Stop);
				return FSM_Continue;
			}
		}
		else
		{
			Vector3 vehDriveDirXY(vehDriveDir.x, vehDriveDir.y, 0.0f);
			vehDriveDirXY.Normalize();
			Vector3 steeringDeltaXY(rage::Cosf( m_fRequestedOrientation ), rage::Cosf( m_fRequestedOrientation ), 0.0f);
			steeringDeltaXY.Normalize();
			Vector3 turnVec(0.0f, 0.0f, 0.0f);
			turnVec.Cross(vehDriveDirXY, steeringDeltaXY);
			float leftRightControl = turnVec.z * HALF_PI;
			float fYawControl = rage::Clamp(leftRightControl, -HALF_PI, HALF_PI);
			subHandling->SetYawControl(fYawControl);
		}
	}

	return FSM_Continue;
}

void CTaskVehicleGoToSubmarine::Start_OnEnter(CVehicle* pVehicle)
{
	Assert(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE || ( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR));

	if(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR && !pVehicle->IsInSubmarineMode() && !(m_iSubFlags & SF_DontSwitchToSubMode))
	{
		SetNewTask(rage_new CTaskVehicleTransformToSubmarine());
	}
}

CTask::FSM_Return CTaskVehicleGoToSubmarine::Start_OnUpdate(CVehicle* pVehicle)
{
	if(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR && !(m_iSubFlags & SF_DontSwitchToSubMode))
	{
		if( pVehicle->IsInSubmarineMode() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_TRANSFORM_TO_SUBMARINE))
		{
			SetState(State_GoTo);
		}		
	}
	else
	{
		SetState(State_GoTo);
	}

	return FSM_Continue;
}

void CTaskVehicleGoToSubmarine::Goto_OnEnter(CVehicle* pVehicle)
{
	FindTargetCoors(pVehicle, m_Params);

	//always initialize nav support
	m_navigationHelper.Init(GetVehicle(), m_Params.IsTargetEntityValid() ? m_Params.GetTargetEntity().GetEntity() : NULL, m_Params.GetTargetPosition());

	if(!(m_iSubFlags & SF_DontUseNavSupport))
	{		
		m_navigationHelper.Update(fwTimer::GetTimeStep(), m_Params.GetTargetPosition(), true);
	}

	//assume water height will always be the same for the body of water we're in
	if(!Water::GetWaterLevelNoWaves(m_Params.GetTargetPosition(), &m_fLocalWaterHeight, POOL_DEPTH, 999999.9f, NULL))
	{
		m_fLocalWaterHeight = 0.0f;
	}
}

CTask::FSM_Return CTaskVehicleGoToSubmarine::Goto_OnUpdate(CVehicle* pVehicle)
{
	Assert(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE || ( pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR && pVehicle->IsInSubmarineMode()));

	//update the target coords
	FindTargetCoors(pVehicle, m_Params);
	Vector3 vCurrentTarget = m_Params.GetTargetPosition();

	if(!(m_iSubFlags & SF_DontUseNavSupport))
	{
		m_navigationHelper.Update(fwTimer::GetTimeStep(), m_Params.GetTargetPosition());
		vCurrentTarget = m_navigationHelper.GetGoToTarget();
	}

	//always use final target position here
	if ((m_Params.GetTargetPosition() - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())).Mag() < m_Params.m_fTargetArriveDist)
	{
		if(m_iSubFlags & SF_AttainRequestedOrientation)
		{
			SetState(State_Rotate);
			return FSM_Continue;
		}
		else
		{
			if(!IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved))
			{
				m_bMissionAchieved = true;
				SetState(State_Stop);
				return FSM_Continue;
			}
			else if(m_iSubFlags & SF_HoverAtEnd)
			{
				SetState(State_Stop);
				return FSM_Continue;
			}
		}
	}

	Submarine_SteerToTarget((CSubmarine*)pVehicle, vCurrentTarget, &pVehicle->m_vehControls);

	return FSM_Continue;
}

void CTaskVehicleGoToSubmarine::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	u32 stopFlags = (m_iSubFlags & SF_HoverAtEnd) ? CTaskVehicleStop::SF_DontTerminateWhenStopped : 0;
	SetNewTask(rage_new CTaskVehicleStop(stopFlags));
}

CTask::FSM_Return CTaskVehicleGoToSubmarine::Stop_OnUpdate(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}

	if((m_iSubFlags & SF_HoverAtEnd))
	{
		FindTargetCoors(pVehicle, m_Params);
		Vector3 vCurrentTarget = m_Params.GetTargetPosition();
		UpdateHoverControl((CSubmarine*)pVehicle, vCurrentTarget);
	}

	return FSM_Continue;
}

void CTaskVehicleGoToSubmarine::Submarine_SteerToTarget(CSubmarine* pSub, Vector3 targetPosition, CVehControls* pVehControls)
{
	Assertf(pSub, "SteerToTarget expected a valid vehicle.");
	Assertf(pVehControls, "SteerToTarget expected a valid set of vehicle controls.");

	TUNE_GROUP_BOOL		(SUB_AI, justUsePlayerInput,			false);
	if(justUsePlayerInput)
	{
		CControl* pControl = CGameWorld::FindLocalPlayer()->GetControlFromPlayer();
		Assertf(pControl,"Expected valid control.");

		Vector2 vPlayerInput;
		pControl->SetVehicleSubSteeringExclusive();
		vPlayerInput.y = pControl->GetVehicleSubPitchUpDown().GetNorm();
		vPlayerInput.x = -pControl->GetVehicleSubTurnLeftRight().GetNorm();

		CFlightModelHelper::MakeCircularInputSquare(vPlayerInput);
		CSubmarineHandling* subHandling = pSub->GetSubHandling();

		subHandling->SetPitchControl(vPlayerInput.y);
		subHandling->SetYawControl(vPlayerInput.x);

		float fThrottle = pControl->GetVehicleSubThrottleUp().GetNorm01() - pControl->GetVehicleSubThrottleDown().GetNorm01();
		fThrottle = rage::Clamp(fThrottle,-1.0f,1.0f);
		pVehControls->SetThrottle(fThrottle);

		subHandling->SetDiveControl(pControl->GetVehicleSubAscend().GetNorm01() - pControl->GetVehicleSubDescend().GetNorm01());
	}
	else
	{
		UpdateAISteering(pSub, targetPosition, pVehControls);
	}
}

// void CTaskVehicleGoToSubmarine::UpdateEntityAvoidance(CSubmarine& pSub, Vector3& vTargetPosition)
// {
// 	//TODO implement
// }

void CTaskVehicleGoToSubmarine::UpdateObjectAvoidance(CSubmarine& pSub, Vector3& vTargetPosition)
{
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pSub.GetTransform().GetPosition());
	const Vector3 vVelocity = pSub.GetVelocity();
	Vector3 vVelocityDir;
	vVelocityDir.Normalize(vVelocity);
	Vector3 vBumperOffset = vVelocityDir * pSub.GetBoundingBoxMax().y;
	Vector3 vBumperPosition = vBumperOffset + vVehiclePos;
	Vector3 vTargetDir = vTargetPosition - vVehiclePos;
	const float fTargetDist = vTargetDir.Mag();
	vTargetDir.Normalize();

	if (m_avoidanceTestResults[AD_ForwardVel].GetResultsReady() &&
		m_avoidanceTestResults[AD_Forward].GetResultsReady() &&
		m_avoidanceTestResults[AD_Left].GetResultsReady() &&
		m_avoidanceTestResults[AD_Right].GetResultsReady())
	{
		bool hitForward = false;
		bool hitLeft = false;
		bool hitRight = false;

		float fTargetDist2 = square(fTargetDist);
		float fLeftDist = fTargetDist2;
		float fRightDist = fTargetDist2;
		Vector3 vBumperPosition = vBumperOffset + vVehiclePos;
		for(int i = 0; i < AD_COUNT; i++ )
		{
			if ( m_avoidanceTestResults[i][0].GetHitDetected())
			{
				if ( i == AD_ForwardVel )
				{
					hitForward = true;	
				}
				else if ( i == AD_Forward )
				{
					m_fDistToForwardObject = (vBumperPosition - VEC3V_TO_VECTOR3(m_avoidanceTestResults[i][0].GetHitPositionV())).Mag();
				}
				else if( i == AD_Left)
				{
					hitLeft = true;
					fLeftDist = DistSquared(m_avoidanceTestResults[i][0].GetHitPositionV(), VECTOR3_TO_VEC3V(vBumperPosition) ).Getf();
				}
				else if( i == AD_Right)
				{
					hitRight = true;
					fRightDist= DistSquared(m_avoidanceTestResults[i][0].GetHitPositionV(), VECTOR3_TO_VEC3V(vBumperPosition) ).Getf();
				}

#if DEBUG_DRAW
				probeHits[i].bHit = true;
				probeHits[i].vHitLocation = VEC3V_TO_VECTOR3(m_avoidanceTestResults[i][0].GetHitPositionV());
#endif
			}
			else
			{ 
				if ( i == AD_Forward )
				{
					m_fDistToForwardObject = 1000.0f;
				}
#if DEBUG_DRAW
				probeHits[i].bHit = false; 
#endif
			}
		}

		//only bother turning if we're moving forward
		if(pSub.GetVelocity().XYMag2() > 4.0f)
		{
			//need to turn if we've hit left/right and not forward - or if hit forward and one of left/right
			bool hitBoth = hitLeft && hitRight;
			bool hitOne = hitLeft != hitRight;
			if((hitBoth && !hitForward) || hitOne)
			{
				Vector3 toHitPosition;
				float fDistance;

				if(fLeftDist < fRightDist)
				{
					toHitPosition = VEC3V_TO_VECTOR3(m_avoidanceTestResults[AD_Left][0].GetHitPositionV()) - vVehiclePos;
					fDistance = fLeftDist;
				}
				else
				{
					toHitPosition = VEC3V_TO_VECTOR3(m_avoidanceTestResults[AD_Right][0].GetHitPositionV()) - vVehiclePos;
					fDistance = fRightDist;
				}

				Vector3 toHitPositionXY(toHitPosition.x, toHitPosition.y, 0.0f);
				toHitPositionXY.Normalize();
				const Vector3   vehDriveDir (VEC3V_TO_VECTOR3(pSub.GetTransform().GetB()));
				const Vector3	vehRightDir	(VEC3V_TO_VECTOR3(pSub.GetTransform().GetRight()));
				float fHeadingToTarget = fwAngle::GetATanOfXY(toHitPositionXY.x, toHitPositionXY.y);
				float fCurrentHeading = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);
				float fAngleDiff = SubtractAngleShorter(fHeadingToTarget, fCurrentHeading);
				if( vehRightDir.Dot(toHitPositionXY) > 0.0f)
				{
					fAngleDiff *= -1;
				}
				float fScaler = Min(1.0f, 1.0f / sqrt(fDistance));
				vTargetPosition.RotateZ(fAngleDiff * fScaler);
			}
		}

		m_avoidanceTestResults[AD_ForwardVel].Reset();
		m_avoidanceTestResults[AD_Forward].Reset();
		m_avoidanceTestResults[AD_Left].Reset();
		m_avoidanceTestResults[AD_Right].Reset();
	}

	if (!m_avoidanceTestResults[AD_ForwardVel].GetWaitingOnResults() && 
		!m_avoidanceTestResults[AD_Forward].GetWaitingOnResults() && 
		!m_avoidanceTestResults[AD_Left].GetWaitingOnResults() &&
		!m_avoidanceTestResults[AD_Right].GetWaitingOnResults())
	{
		float fForwardTestTime = sm_Tunables.m_wingProbeTimeDistance;
		if(pSub.GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
		{
			fForwardTestTime += CVehicleAILodManager::GetExpectedWorstTimeslicedTimestep();
		}

#if !DEBUG_DRAW
		WorldProbe::CShapeTestCapsuleDesc probeDesc[AD_COUNT];
#endif

		float fForwardTestDist = Max(2.0f, vVelocity.Mag() * fForwardTestTime);
		float fRadius = sm_Tunables.m_forwardProbeRadius;
		Vector3 forwardOffset = vVelocityDir * 3.0f;
		Vector3 vForwardTestOffset = fForwardTestDist * vVelocityDir;
		float fWingOffset = pSub.GetBoundingBoxMax().x;
		for(int i = 0; i < AD_COUNT; i++)
		{
			probeDesc[i].SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc[i].SetExcludeInstance(pSub.GetCurrentPhysicsInst());
			VehicleNavigationHelper::HelperExcludeCollisionEntities(probeDesc[i], pSub);
			probeDesc[i].SetResultsStructure(&m_avoidanceTestResults[i]);
			probeDesc[i].SetMaxNumResultsToUse(1);
			probeDesc[i].SetIsDirected(true);
			probeDesc[i].SetDoInitialSphereCheck(true);

			if(i == AD_ForwardVel)
			{
				probeDesc[i].SetCapsule(vBumperPosition, vBumperPosition + (vForwardTestOffset * 2.5f), fRadius);
			}
			else if(i == AD_Forward)
			{
				const Vector3 vehDriveDir (VEC3V_TO_VECTOR3(pSub.GetTransform().GetB()));
				probeDesc[i].SetCapsule(vBumperPosition, vBumperPosition + 
									vehDriveDir * (sm_Tunables.m_maxForwardSlowdownDist + (vVelocity.XYMag() * sm_Tunables.m_forwardProbeVelScaler)), fRadius);
			}
			else if(i == AD_Left)
			{
				Vector3 right = VEC3V_TO_VECTOR3(pSub.GetTransform().GetRight());
				Vector3 startPoint = vBumperPosition + forwardOffset + (right * -fWingOffset);
				probeDesc[i].SetCapsule(startPoint, startPoint + vForwardTestOffset, fRadius);
			}
			else if(i == AD_Right)
			{
				Vector3 right = VEC3V_TO_VECTOR3(pSub.GetTransform().GetRight());
				Vector3 startPoint = vBumperPosition + forwardOffset + (right * fWingOffset);
				probeDesc[i].SetCapsule(startPoint, startPoint + vForwardTestOffset, fRadius);
			}

			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc[i], WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}
	}
}

void CTaskVehicleGoToSubmarine::UpdateAvoidance(CSubmarine& pSub, Vector3& vTargetPosition)
{
	if(!(m_iSubFlags & SF_DontObjectAvoidance))
	{
		UpdateObjectAvoidance(pSub, vTargetPosition);
	}

	//not yet implemented
// 	if(!(m_iSubFlags & SF_DontEntityAvoidance))
// 	{
// 		UpdateEntityAvoidance(pSub, vTargetPosition);
// 	}

#if DEBUG_DRAW
	m_vModifiedTarget = vTargetPosition;
#endif
}

//no height map data in water - have to use probes to get data
float CTaskVehicleGoToSubmarine::ComputeTargetZToAvoidHeightMap(CSubmarine& pSub, const Vector3& currentPos, float currentSpeed, const Vector3& targetPos)
{
	bool resultsReady = true;
	for(int i = 0; i < m_iNumDownProbesUsed; ++i)
	{
		resultsReady &= m_downTestResults[i].GetResultsReady();
	}

	float currentZ = currentPos.z;
	Vector3 vVehicleDir = targetPos - currentPos;
	float fDistXY = vVehicleDir.XYMag();
	float maxSlope = (m_iSubFlags & SF_SinkToTerrain) ? 0.0f : Max(0.0f, vVehicleDir.z / fDistXY);
	float targetZ = (m_iSubFlags & SF_SinkToTerrain) ? currentZ : targetPos.z;

	if (resultsReady)
	{
		for(int i = 1; i < m_iNumDownProbesUsed; i++ ) //start at 1; 0 is used to check current height
		{
			if ( m_downTestResults[i][0].GetHitDetected())
			{
				Vector3 hitPosition = VEC3V_TO_VECTOR3(m_downTestResults[i][0].GetHitPositionV());
				hitPosition.z += m_minHeightAboveTerrain;

				Vector3 toHitPos = hitPosition - currentPos;
				float xyDist = toHitPos.XYMag();
				float currentSlope = toHitPos.z / xyDist;
				if( currentSlope > maxSlope)
				{
					maxSlope = currentSlope;
					targetZ = hitPosition.z;
				}
#if DEBUG_DRAW
				downProbeHits[i].bHit = true;
				downProbeHits[i].vHitLocation = hitPosition;
#endif
			}
			else
			{
#if DEBUG_DRAW
				downProbeHits[i].bHit = false;
#endif	
			}
		}

		if ( m_downTestResults[0][0].GetHitDetected())
		{
			float fRequiredDepth = m_downTestResults[0][0].GetHitPosition().z + m_minHeightAboveTerrain;
			if( currentZ < fRequiredDepth )
			{
				targetZ = Max(targetZ, fRequiredDepth);
			}
			else
			{
				//if we don't need to go up, do we want to sink to terrain?
				if((m_iSubFlags & SF_SinkToTerrain) && (targetZ - currentZ) < FLT_EPSILON )
				{
					if(fDistXY > 10.0f)
					{
						targetZ = fRequiredDepth;
					}
				}
			}
		}

		m_fLastZCheck = targetZ;

		for(int i = 0; i < s_NumDownProbes; ++i)
		{
			m_downTestResults[i].Reset();
		}
	}
	else
	{
		targetZ = m_fLastZCheck;
	}

	bool waitingOnResults = false;
	for(int i = 0; i < m_iNumDownProbesUsed; ++i)
	{
		waitingOnResults |= m_downTestResults[i].GetWaitingOnResults();
	}

	if (!waitingOnResults)
	{	
		vVehicleDir.z = 0;
		vVehicleDir.Normalize();
		m_iNumDownProbesUsed = 0;

#if !DEBUG_DRAW
		WorldProbe::CShapeTestProbeDesc heightMapProbe[s_NumDownProbes];
#endif

		TUNE_GROUP_FLOAT(SUB_AI, MINPROBEDIST,	0.5f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(SUB_AI, MAXPROBEDIST,	5.0f, 0.0f, 100.0f, 0.1f);
		float fProbeGap = Clamp(currentSpeed, MINPROBEDIST, MAXPROBEDIST);
		for(int i = 0; i < s_NumDownProbes; i++)
		{		
			heightMapProbe[i].SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			heightMapProbe[i].SetExcludeInstance(pSub.GetCurrentPhysicsInst());
			heightMapProbe[i].SetResultsStructure(&m_downTestResults[i]);
			heightMapProbe[i].SetMaxNumResultsToUse(1);
			heightMapProbe[i].SetIsDirected(true);

			float xyLength = i * fProbeGap + ( i != 0 ? 2.0f : 0.0f);
			// don't check beyond target
			if ( xyLength >= fDistXY )
			{
				break;
			}

			++m_iNumDownProbesUsed;

			Vector3 vSamplePosition = currentPos + vVehicleDir * xyLength;
			vSamplePosition.z = m_fLocalWaterHeight;
			Vector3 vEndPos = Vector3(vSamplePosition.x, vSamplePosition.y, vSamplePosition.z - 500.0f);
			heightMapProbe[i].SetStartAndEnd(vSamplePosition, vEndPos);

			WorldProbe::GetShapeTestManager()->SubmitTest(heightMapProbe[i], WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}

#if DEBUG_DRAW
		for(int i = m_iNumDownProbesUsed; i < s_NumDownProbes; i++)
		{		
			downProbeHits[i].bHit = false;
		}
#endif
	}

	return targetZ;
}

float CTaskVehicleGoToSubmarine::ComputeTargetZToAvoidTerrain(CSubmarine& pSub, Vector3 vTargetPosition)
{
	Vector3	vehPos(VEC3V_TO_VECTOR3(pSub.GetTransform().GetPosition()));
	return ComputeTargetZToAvoidHeightMap(pSub, vehPos, pSub.GetAiXYSpeed(), vTargetPosition);
}

void CTaskVehicleGoToSubmarine::UpdateAISteering(CSubmarine* pSub, Vector3 targetPosition, CVehControls* pVehControls)
{
	TUNE_GROUP_BOOL		(SUB_AI, useDampenedSteerVel,			true);

	//calculates absolute Z height we should be aiming for
	targetPosition.z = ComputeTargetZToAvoidTerrain(*pSub, targetPosition);

	UpdateAvoidance(*pSub, targetPosition);

	targetPosition.z = Min(m_fLocalWaterHeight, targetPosition.z);

	// Get the vehicles current driving info.
	Vector3	vehPos						(VEC3V_TO_VECTOR3(pSub->GetTransform().GetPosition()));
	const Vector3	vehDriveDir			(VEC3V_TO_VECTOR3(pSub->GetTransform().GetB()));
	const Vector3	vehCurrVel			(pSub->GetVelocity());
	const float		vehCruiseSpeed		= GetCruiseSpeed();

	// Get some info about how we want to move...
	const Vector3	target				(targetPosition);
	const Vector3	toTargetDelta		(target - vehPos);
	const float		toTargetDist		= toTargetDelta.Mag();
	Vector3			toTargetDir			(toTargetDelta);
	toTargetDir.NormalizeSafe(vehDriveDir, 0.01f);

	// Determine the desired steering.
	Vector3 steeringDelta(0.0f, 0.0f, 0.0f);
	if(toTargetDist > 0.0f)
	{
		Vector3 steeringDesiredVel(toTargetDir);
		steeringDesiredVel *= vehCruiseSpeed;

		float fVelStrength = 1.0f;
		if(useDampenedSteerVel)
		{
			Vector3 vSteeringVelNorm = steeringDesiredVel;
			vSteeringVelNorm.Normalize();
			fVelStrength = DotProduct(vSteeringVelNorm, vehDriveDir);
			fVelStrength = Clamp(fVelStrength, -1.0f, 1.0f);

			//! invert.
			fVelStrength *= -1.0f;

			//! Normalize to 0-1.
			fVelStrength = (fVelStrength + 1.0f) / 2.0f;
		}

		steeringDelta = steeringDesiredVel - (vehCurrVel*fVelStrength);
	}

	SteerToVelocity(pSub, pVehControls, targetPosition, steeringDelta);
}

void CTaskVehicleGoToSubmarine::UpdateHoverControl(CSubmarine* pSub, Vector3 targetPosition)
{
	const Vector3 vehPos (VEC3V_TO_VECTOR3(pSub->GetTransform().GetPosition()));
	CSubmarineHandling* subHandling = pSub->GetSubHandling();

	//calculates absolute Z height we should be aiming for
	targetPosition.z = ComputeTargetZToAvoidTerrain(*pSub, targetPosition);

	UpdateAvoidance(*pSub, targetPosition);

	targetPosition.z = Min(m_fLocalWaterHeight, targetPosition.z);

	Vector3 toTarget = targetPosition - vehPos;
	float zDist = toTarget.GetZ();

	//calculate pitch and dive controls
	Vector3 vVelocity = pSub->GetVelocity();
	if( zDist < 0.0f && zDist > -10.0f)
	{
		zDist -= (vVelocity.z * sm_Tunables.m_downDiveScaler);
	}
	float diveControl = rage::Clamp(zDist/10.0f, -1.0f, 1.0f);
	subHandling->SetDiveControl(diveControl);
	subHandling->SetPitchControl(0.0f); //or do we want to flatten out?
}

void CTaskVehicleGoToSubmarine::SteerToVelocity(CSubmarine* pSub, CVehControls* pVehControls, const Vector3& vTargetPos, Vector3 steeringDelta)
{
	const Vector3 vehPos (VEC3V_TO_VECTOR3(pSub->GetTransform().GetPosition()));
	const Vector3 vehDriveDir (VEC3V_TO_VECTOR3(pSub->GetTransform().GetB()));

	CSubmarineHandling* subHandling = pSub->GetSubHandling();
	Vector3 toTarget = vTargetPos - vehPos;
	Vector3 toTargetNorm;
	toTargetNorm.Normalize(toTarget);
	float dotTarget = toTargetNorm.Dot(vehDriveDir);
	float zDist = toTarget.GetZ();
	float xyDist = toTarget.XYMag();

	Vec3f VehForwardVector;
	LoadAsScalar(VehForwardVector, pSub->GetVehicleForwardDirectionRef());
	VehForwardVector.SetZ(0.0f);
	Vec3f VehVelocity;
	LoadAsScalar(VehVelocity, pSub->GetAiVelocityConstRef());
	VehVelocity.SetZ(0.0f);
	float forwardSpeed = Dot(VehVelocity, VehForwardVector);

	//process yaw
	Vector3 vehDriveDirXY(vehDriveDir.x, vehDriveDir.y, 0.0f);
	vehDriveDirXY.Normalize();
	const Vector3 steeringDeltaXY(steeringDelta.x, steeringDelta.y, 0.0f);
	Vector3 steeringDirXY(steeringDeltaXY);
	steeringDirXY.Normalize();
	Vector3 turnVec(0.0f, 0.0f, 0.0f);
	turnVec.Cross(vehDriveDirXY, steeringDirXY);
	float leftRightControl = turnVec.z * HALF_PI;
	float fYawControl = rage::Clamp(leftRightControl, -HALF_PI, HALF_PI);
	if( dotTarget < -0.5f)
	{
		fYawControl = leftRightControl < 0.0f ? -HALF_PI : HALF_PI;
	}

	subHandling->SetYawControl(fYawControl);

	//calculate pitch and dive controls
	Vector3 vVelocity = pSub->GetVelocity();
	if( zDist < 0.0f && zDist > -10.0f)
	{
		zDist -= (vVelocity.z * sm_Tunables.m_downDiveScaler);
	}
	float diveControl = rage::Clamp(zDist/10.0f, -1.0f, 1.0f);
	subHandling->SetDiveControl(diveControl);

	if(xyDist < m_Params.m_fTargetArriveDist)
	{
		//close in xy, but not in z - just use dive control to get us there
		subHandling->SetPitchControl(0.0f); //or do we want to flatten out?
	}
	else
	{
		float currentPitch = fabsf(pSub->GetTransform().GetPitch()) * RtoD;
		float fPitchDelta = currentPitch - sm_Tunables.m_maxPitchAngle;
		if(fPitchDelta > 0.0f)
		{
			float zDiff = -vehDriveDir.z;
			float zDirDesired = zDiff * 0.5f;
			float pitchControl = zDirDesired * HALF_PI;
			subHandling->SetPitchControl(rage::Clamp(pitchControl, -HALF_PI, HALF_PI));
		}
		else
		{
			Vector3 steeringDir(vTargetPos);
			steeringDir.Normalize();
			float zDiff = steeringDir.z - vehDriveDir.z;
			static dev_float pitchScaler = 10.0f;
			zDiff *= pitchScaler;
			float zSpeedDiff = Lerp(RampValue(pSub->GetAiXYSpeed(), 5.0f, 10.0f, 0.0f, 1.0f),-vehDriveDir.z, zDiff);
			float zDirDesired = zSpeedDiff * 0.5f;
			float pitchControl = zDirDesired * HALF_PI;
			subHandling->SetPitchControl(rage::Clamp(pitchControl, -HALF_PI, HALF_PI));			
		}
	}

	//calculate throttle
	float desiredSpeed = GetCruiseSpeed();

	//Slow down as we approach the target.
	if(!(m_iSubFlags & SF_DontArrivalSlowdown) && m_fSlowDownDistance > 0.0f)
	{
		desiredSpeed *= Min(1.0f, Max( xyDist - m_fSlowDownDistanceMin, 0.0f ) / m_fSlowDownDistance);
	}

	if( m_fDistToForwardObject < sm_Tunables.m_maxForwardSlowdownDist)
	{
		//adjust desired speed based on our forward distance to object
		desiredSpeed = RampValue(m_fDistToForwardObject, sm_Tunables.m_minForwardSlowdownDist, 
			sm_Tunables.m_maxForwardSlowdownDist + (vVelocity.XYMag() * sm_Tunables.m_forwardProbeVelScaler), sm_Tunables.m_maxReverseSpeed, Min(5.0f, desiredSpeed));
	}

	const float speedDiffDesired = desiredSpeed - forwardSpeed;
	float throttle = 0.0f;

	//smooth out throttle
	if( vehDriveDir.Dot(steeringDelta) > 0.0f)
	{
		if( speedDiffDesired > 0.0f )
		{
			TUNE_GROUP_FLOAT	(SUB_AI,			ACC_TO_THROTTLE_MAPPING_SLOW,		4.0f, 0.0f, 100.0f, 0.1f);
			TUNE_GROUP_FLOAT	(SUB_AI,			ACC_TO_THROTTLE_MAPPING_MED,		8.0f, 0.0f, 100.0f, 0.1f);
			TUNE_GROUP_FLOAT	(SUB_AI,			ACC_TO_THROTTLE_MAPPING_HIGH,		2.0f, 0.0f, 100.0f, 0.1f);
			TUNE_GROUP_FLOAT	(SUB_AI,			LOW_THROTTLE_MAPPING_THRESHOLD,		2.0f, 0.0f, 100.0f, 0.1f);
			TUNE_GROUP_FLOAT	(SUB_AI,			MED_THROTTLE_MAPPING_THRESHOLD,		10.0f, 0.0f, 100.0f, 0.1f);
			
			if(forwardSpeed < LOW_THROTTLE_MAPPING_THRESHOLD)
			{
				throttle = speedDiffDesired / ACC_TO_THROTTLE_MAPPING_SLOW;
			}
			else if(forwardSpeed < MED_THROTTLE_MAPPING_THRESHOLD)
			{
				throttle = speedDiffDesired / ACC_TO_THROTTLE_MAPPING_MED;
			}
			else
			{
				throttle = speedDiffDesired / ACC_TO_THROTTLE_MAPPING_HIGH;
			}
		}
		else
		{
			TUNE_GROUP_FLOAT	(SUB_AI,			REVERSE_POWER_MAPPING,				5.0f, 0.0f, 100.0f, 0.1f);
			throttle = speedDiffDesired / REVERSE_POWER_MAPPING;
		}
	}
	
	pVehControls->SetThrottle(rage::Clamp(throttle, -1.0f, 1.0f));
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleGoToSubmarine::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GoTo",
		"State_Rotate",
		"State_Stop"
	};

	return aStateNames[iState];
}

void CTaskVehicleGoToSubmarine::Debug() const
{
#if DEBUG_DRAW
	Vector3 target;
	FindTargetCoors(GetVehicle(), target);

	Vec3V vSubPos = GetVehicle()->GetTransform().GetPosition();
	Vec3V vTargetPos = VECTOR3_TO_VEC3V(target);		
	Vec3V vModifiedTarget = VECTOR3_TO_VEC3V(m_vModifiedTarget);	

	Vector3 desiredZ = Vector3(vSubPos.GetXf(), vSubPos.GetYf(),vModifiedTarget.GetZf());
	grcDebugDraw::Sphere(desiredZ, 0.25f, Color_yellow);

	if(!(m_iSubFlags & SF_DontUseNavSupport))
	{
		Vec3V vActualTarget = VECTOR3_TO_VEC3V(m_navigationHelper.GetGoToTarget());

		grcDebugDraw::Arrow(vSubPos, vTargetPos, 1.0f, Color_white);
		if(!IsEqualAll(vActualTarget, vTargetPos))
		{
			grcDebugDraw::Arrow(vSubPos, vActualTarget, 1.0f, Color_green);
		}
		if(!IsEqualAll(vActualTarget, vModifiedTarget))
		{
			grcDebugDraw::Arrow(vSubPos, vModifiedTarget, 1.0f, Color_red);
		}

		m_navigationHelper.Debug();
	}

	static bool s_DrawProbes = true;
	static bool s_DrawProbeHits = true;

	for (u32 i = 0; i < AD_COUNT; ++i)
	{
		Color32 colors[AD_COUNT] = { Color_yellow, Color_red, Color_green, Color_orange };
	
		if( s_DrawProbes )
		{
			grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(probeDesc[i].GetStart()), VECTOR3_TO_VEC3V(probeDesc[i].GetEnd()), probeDesc[i].GetRadius(),  colors[i], false);
		}
	
		if ( s_DrawProbeHits && probeHits[i].bHit )
		{
			grcDebugDraw::Sphere(probeHits[i].vHitLocation, 0.25f, colors[i]);
		}
	}

	for (u32 i = 1; i < m_iNumDownProbesUsed; ++i)
	{
		if( s_DrawProbes )
		{
			grcDebugDraw::Line(VECTOR3_TO_VEC3V(heightMapProbe[i].GetStart()), VECTOR3_TO_VEC3V(heightMapProbe[i].GetEnd()), Color_white);
		}

		if ( s_DrawProbeHits && downProbeHits[i].bHit )
		{
			grcDebugDraw::Sphere(downProbeHits[i].vHitLocation, 0.25f, Color_white);
		}
	}

#endif //__BANK

	CTaskVehicleGoTo::Debug();
}

#endif