#include "TaskVehicleGoToHelicopter.h"

// Rage headers
#include "math/angmath.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Vehicles\Heli.h"
#include "Vehicles\vehicle.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleFlying.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "vehicleAi\VehicleNavigationHelper.h"
#include "scene\world\GameWorldHeightMap.h"
#include "vehicles/VehicleGadgets.h"

#include "Profile/timebars.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

#define HELICOPTER_PROFILING 0
#if HELICOPTER_PROFILING
PF_PAGE(TaskVehicleGoToHelicopter, "TaskVehicleGoToHelicopter");
PF_GROUP(Functions);
PF_LINK(TaskVehicleGoToHelicopter, Functions);
PF_TIMER(Heli_SteerToTarget, Functions);
PF_TIMER(SteerToVelocity, Functions);
PF_TIMER(UpdateAvoidance, Functions);
PF_TIMER(UpdateAvoidance_ProcessProbes, Functions);
PF_TIMER(UpdateAvoidance_GenerateProbes, Functions);
PF_TIMER(UpdateAvoidance_submit_probes, Functions);
#endif // HELICOPTER_PROFILING


//////////////////////////////////////////////////////////////////////////
//
// Spatial Heli Tuning
//
//////////////////////////////////////////////////////////////////////////

struct SpatialHeliTuning
{
	SpatialHeliTuning( const spdAABB& in_Region, float in_FarProbesScalar ) 
		: Region(in_Region)
		, FarProbeScalar(in_FarProbesScalar) 
	{ 
	}

	float FarProbeScalar;
	spdAABB Region;
};

static const SpatialHeliTuning s_DefaultSpatialHeliTuning = SpatialHeliTuning(spdAABB(), 1.5f);
static const SpatialHeliTuning s_NarrowSpatialHeliTuning = SpatialHeliTuning(spdAABB(Vec3V(-900.0f, 200.0f, 0.0f), Vec3V(-500.0f, 400.0f, 250.0f)), 1.0f);

const SpatialHeliTuning& QuerySpatialHeliTuning(const Vector3& in_Position)
{
	if ( s_NarrowSpatialHeliTuning.Region.ContainsPoint(VECTOR3_TO_VEC3V(in_Position)) )
	{
		return s_NarrowSpatialHeliTuning;
	}
	return s_DefaultSpatialHeliTuning;
}


CTaskVehicleGoToHelicopter::Tunables CTaskVehicleGoToHelicopter::sm_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleGoToHelicopter, 0xc56800af);

///////////////////////////////////////////////////////////////////////////////

CTaskVehicleGoToHelicopter::CTaskVehicleGoToHelicopter(const sVehicleMissionParams& params,
													   const s32 iHeliFlags,
													   const float fHeliRequestedOrientation /*= -1.0f*/, 
													   const s32 iMinHeightAboveTerrain /*= 20*/ )
: CTaskVehicleGoTo( params)
, m_avoidanceTestResults()
, m_fAvoidanceTimerRightLeft(0.0f)
, m_fAvoidanceTimerUpDown(0.0f)
, m_fHeliRequestedOrientation(-1.0f)
, m_fSlowDownDistance(sm_Tunables.m_slowDistance)
, m_fSlowDownDistanceMin(0.0f)
, m_iMinHeightAboveTerrain(iMinHeightAboveTerrain)
, m_bHasAvoidanceDir(false)
, m_bTimesliceShouldSteerToPos(false)
, m_iHeliFlags(iHeliFlags)
, m_uTimesliceLastAvoidTime(0)
, m_fPreviousHeadingChange(0.0f)
, m_fPreviousPitchChange(0.0f)
, m_fFlyingVehicleAvoidanceScalar(1.0f)
, m_bRunningLandingTask(false)
{
#if DEBUG_DRAW
	m_fPrevDesiredSpeed = 0.0f;
	probeDesc = rage_new WorldProbe::CShapeTestCapsuleDesc[AD_COUNT];
	probeHits = rage_new CTaskVehicleGoToHelicopter::debugHitData[AD_COUNT];
#endif

	m_vBrakeVector.Zero();
	m_avoidanceTestResults = rage_new WorldProbe::CShapeTestSingleResult[AD_COUNT];

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER);

	// clamp desired orientation
	if (fHeliRequestedOrientation != -1.0f)
	{
		SetOrientation(fHeliRequestedOrientation);
	}
}

///////////////////////////////////////////////////////////////////////////////

CTaskVehicleGoToHelicopter::~CTaskVehicleGoToHelicopter()
{
#if DEBUG_DRAW
	delete[] probeDesc;
	delete[] probeHits;
#endif

	delete[] m_avoidanceTestResults;
}

///////////////////////////////////////////////////////////////////////////////

aiTask* CTaskVehicleGoToHelicopter::Copy() const
{
	//Create the task.
	CTaskVehicleGoToHelicopter* pTask = rage_new CTaskVehicleGoToHelicopter(m_Params, m_iHeliFlags, m_fHeliRequestedOrientation, m_iMinHeightAboveTerrain);
	
	//Set the slow down distance.
	pTask->SetSlowDownDistance(m_fSlowDownDistance);
	pTask->SetFlyingVehicleAvoidanceScalar(m_fFlyingVehicleAvoidanceScalar);
	
	return pTask;
}

///////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToHelicopter::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	Assert( GetVehicle()->InheritsFromHeli() );
	CHeli *pHeli = static_cast<CHeli*>(GetVehicle());//Get the heli ptr.

	FSM_Begin
		// 
		FSM_State(State_GoTo)
			FSM_OnEnter
				Goto_OnEnter(pHeli);
			FSM_OnUpdate
				return Goto_OnUpdate(pHeli);
			FSM_OnExit
				Goto_OnExit(pHeli);

		FSM_State(State_Land)
			FSM_OnEnter
				Land_OnEnter(pHeli);
			FSM_OnUpdate
				return Land_OnUpdate(pHeli);

	FSM_End
}

void CTaskVehicleGoToHelicopter::ProcessTimeslicedUpdate()
{
	CVehicle* pVehicle = GetVehicle();
	if(m_bTimesliceShouldSteerToPos && pVehicle)
	{
		Assert(pVehicle->InheritsFromHeli());
		if(m_bTimesliceShouldSteerToPos)
		{
			CHeli *pHeli = static_cast<CHeli*>(pVehicle);

			// Normally we don't do avoidance when timesliced, except for during the full updates.
			// If we found some obstruction during a full update, though, we force updates to happen
			// every frame but it may still take a few frames while we are considered to be in
			// timesliced LOD, during which we probably will want to use avoidance.
			bool useAvoidance = WasAvoidingRecently();

			SteerToTarget(pHeli, RCC_VECTOR3(m_vTimesliceCurrentSteerToPos), useAvoidance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToHelicopter::Goto_OnEnter(CHeli *pHeli)
{
	pHeli->SwitchEngineOn((m_iHeliFlags & HF_StartEngineImmediately) != 0);

	if( pHeli->GetIsDrone() )
	{
		m_fFlyingVehicleAvoidanceScalar = 0.25f;
	}

	m_flyingAvoidanceHelper.Init(pHeli, m_fFlyingVehicleAvoidanceScalar);

	m_bHasAvoidanceDir = false;
	m_fPreviousTargetHeight = m_Params.GetTargetPosition().z;

	Vector3 vHeliPos = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());
	Vector3 vStartPos(vHeliPos.x, vHeliPos.y, vHeliPos.z + (float)m_iMinHeightAboveTerrain);
	Vector3 vEndPos(vHeliPos.x, vHeliPos.y, vHeliPos.z - (float)m_iMinHeightAboveTerrain);
	WorldProbe::CShapeTestHitPoint hitPoint;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_RIVER_TYPE);
	probeDesc.SetExcludeInstance(pHeli->GetCurrentPhysicsInst());
	probeDesc.SetResultsStructure(&m_startLocationTestResults);
	probeDesc.SetMaxNumResultsToUse(1);
	probeDesc.SetStartAndEnd(vStartPos, vEndPos);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST); // @TODO: WorldProbe::PERFORM_SYNCHRONOUS_TEST


	// on the ground lets make sure we go up first
	if ( !pHeli->IsInAir() )
	{
		const float s_defaultUpTimer = 1.0f;
		m_fAvoidanceTimerRightLeft = s_defaultUpTimer;
		m_fAvoidanceTimerUpDown = s_defaultUpTimer;

		m_bHasAvoidanceDir = true;
		m_vAvoidanceDir = g_UnitUp;		
	}

	//retract landing gear in three seconds
	pHeli->GetHeliIntelligence()->SetRetractLandingGearTime(fwTimer::GetTimeInMilliseconds() + 3000);
}

//////////////////////////////////////////////////////////////////////////////////////


CTask::FSM_Return CTaskVehicleGoToHelicopter::Goto_OnUpdate	(CHeli *pHeli)
{	
	m_bTimesliceShouldSteerToPos = false;	// Will be set again if needed.

	//update the target coords
	FindTargetCoors(pHeli, m_Params);
	Vector3 vTargetPos = m_Params.GetTargetPosition();

	if (pHeli->GetHeliIntelligence()->bHasCombatOffset)
	{
		m_iHeliFlags |= HF_DontDoAvoidance;
	}

	const bool bAllowAvoidance = (m_iHeliFlags & HF_DontDoAvoidance) == 0;
	if ( bAllowAvoidance )
	{
		CFlyingVehicleAvoidanceManager::SlideDestinationTarget(vTargetPos, RegdVeh(pHeli), vTargetPos, m_fFlyingVehicleAvoidanceScalar);
	}
	
	Vector3 vHeliPos = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());

	const bool bDisableAllHeightMapAvoidance = (m_iHeliFlags & HF_DisableAllHeightMapAvoidance) != 0;
	if (!bDisableAllHeightMapAvoidance)
	{
		vTargetPos.z = ComputeTargetZToAvoidTerrain(*pHeli, vTargetPos);
	}

	float dist = (vTargetPos - vHeliPos).Mag();
	if (dist < m_Params.m_fTargetArriveDist)
	{
		if ((m_iHeliFlags & HF_LandOnArrival) != 0)
		{
			SetState(State_Land);
			return FSM_Continue;
		}
		else if(!IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved))
		{
			m_bMissionAchieved = true;
			return FSM_Quit;
		}
	}

	SteerToTarget(pHeli, vTargetPos, bAllowAvoidance);

	if(m_iHeliFlags & HF_EnableTimeslicingWhenPossible)
	{
		if(!GetParent()								// If we are not the top level task, ProcessTimeslicedUpdate() won't get called properly.
			&& (dist > sm_Tunables.m_TimesliceMinDistToTarget)	// Make sure we are not too close to the destination.
			&& !WasAvoidingRecently())				// Make sure we are not avoiding.
		{
			pHeli->m_nVehicleFlags.bLodCanUseTimeslicing = true;
		}
	}

	//check landing gear
	if(pHeli->IsInAir() )
	{
		if ( pHeli->GetHeliIntelligence()->CheckTimeToRetractLandingGear())
		{
			pHeli->GetHeliIntelligence()->SetRetractLandingGearTime(0xffffffff);
			CLandingGear::eLandingGearPublicStates eGearState = pHeli->GetLandingGear().GetPublicState();
			if ( eGearState == CLandingGear::STATE_LOCKED_DOWN || eGearState == CLandingGear::STATE_DEPLOYING )
			{
				pHeli->GetLandingGear().ControlLandingGear(pHeli, CLandingGear::COMMAND_RETRACT);
			}
		}
	}
	else
	{
		pHeli->GetHeliIntelligence()->ExtendLandingGearTime(fwTimer::GetTimeStepInMilliseconds());
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToHelicopter::Goto_OnExit(CHeli* /*pHeli*/)
{
	m_bTimesliceShouldSteerToPos = false;
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToHelicopter::Land_OnEnter								(CHeli *pHeli)
{
	Vector3 targetPos = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());
	targetPos.z = -50.0f;

	// we don't want to be above the terrain
	m_iMinHeightAboveTerrain = -10;
	m_Params.m_fTargetArriveDist = -1.0f; // don't arrive using this task.  wait for landing
	m_Params.m_fCruiseSpeed = Max(m_Params.m_fCruiseSpeed, 20.0f);

	SetTargetPosition(&targetPos);

	SetNewTask(rage_new CTaskVehicleLand(m_Params, m_iHeliFlags, m_fHeliRequestedOrientation));
}


//////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleGoToHelicopter::Land_OnUpdate								(CHeli * UNUSED_PARAM(pHeli))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_LAND))
	{
		m_bMissionAchieved = true;
		return FSM_Quit;
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToHelicopter::SetOrientationRequested(bool bOrientationRequested)
{
	if( bOrientationRequested )
	{
		m_iHeliFlags |= HF_AttainRequestedOrientation;
	}
	else
	{
		m_iHeliFlags &= ~HF_AttainRequestedOrientation;
	}
}


//////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToHelicopter::SetShouldLandOnArrival(bool bShouldLand)
{
	if( bShouldLand )
	{
		m_iHeliFlags |= HF_LandOnArrival;
	}
	else
	{
		m_iHeliFlags &= ~HF_LandOnArrival;
	}
}

void CTaskVehicleGoToHelicopter::SetFlyingVehicleAvoidanceScalar(float in_fFlyingVehicleAvoidanceScalar)
{ 
	m_fFlyingVehicleAvoidanceScalar = in_fFlyingVehicleAvoidanceScalar; 
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SteerToTarget
// PURPOSE :  Makes an AI plane go towards its target coordinates.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToHelicopter::SteerToTarget(CHeli* pHeli, const Vector3& in_vTargetPos, bool bAvoidObstacles)
{
#if HELICOPTER_PROFILING
	PF_AUTO_PUSH_TIMEBAR("Heli_SteerToTarget");
	PF_FUNC(Heli_SteerToTarget);
#endif // #if HELICOPTER_PROFILING

	Assertf(pHeli, "SteerToTargetCoors expected a valid vehicle.");

	// Store off our current target position, for use if we are timesliced.
	m_vTimesliceCurrentSteerToPos = VECTOR3_TO_VEC3V(in_vTargetPos);
	m_bTimesliceShouldSteerToPos = true;

	//spdSphere compositeSphere = pHeli->GetBoundSphere();
	//const Vector3 vHeliPos = VEC3V_TO_VECTOR3(compositeSphere.GetCenter());
	const Vector3 vHeliPos = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());

	Vector3 vTargetDelta = in_vTargetPos - vHeliPos;
	float fTargetDist = vTargetDelta.Mag();

	//Calculate the desired speed.
	float fDesiredSpeed = GetCruiseSpeed();

	//Check if the slow down distance is valid.
	if(m_fSlowDownDistance > 0.0f)
	{
		//Slow down as we approach the target.
		fDesiredSpeed *= Min(1.0f, Max( fTargetDist - m_fSlowDownDistanceMin, 0.0f ) / m_fSlowDownDistance);
	}

	// only update steering if we either want to move or we're not on the ground
	if ( fDesiredSpeed > 0.0f || !pHeli->HasContactWheels() )
	{

	#if DEBUG_DRAW
		m_vModifiedTarget = in_vTargetPos;
	#endif

		//do we want to drop to the terrain
		if((m_iHeliFlags & HF_MaintainHeightAboveTerrain) && vTargetDelta.z < 0.0f && m_fAvoidanceTimerRightLeft <= 0.0f && m_fAvoidanceTimerUpDown <= 0.0f)
		{			
			float absZDelta = fabsf(vTargetDelta.z);
			float fSlope = vTargetDelta.XYMag() / absZDelta;
			if( fSlope > 1.0f )
			{
				//if we're further away from target than our height difference we move target position towards us along xy based on how far we are above target Z
				Vector3 ourPosAtTargetHeight = Vector3(vHeliPos.x, vHeliPos.y, in_vTargetPos.z);
				Vector3 xyTargetDir = Vector3(vTargetDelta.x, vTargetDelta.y, 0.0f);
				xyTargetDir.Normalize();
				//get point on xy line to target at absZDelta away from our position
				Vector3 pointAlongLineAtZHeight = ourPosAtTargetHeight + (xyTargetDir * absZDelta);
				//delta from point on xy line to target position
				Vector3 modifiedPosToTarget = in_vTargetPos - pointAlongLineAtZHeight;

				float toTargetZRampedValue = RampValue(absZDelta, 0.0f, sm_Tunables.m_maintainHeightMaxZDelta, 1.0f, 0.0f);
				if(toTargetZRampedValue > 0.0f && fSlope > sm_Tunables.m_maintainHeightMaxZDelta)
				{
					//if our slope to target is large (small Z - large XY), limit target distance to ensure we don't flatten out too early
					modifiedPosToTarget.Normalize();
					modifiedPosToTarget *= 100.0f;
				}
				//calculate new position between xy point on line and original target
				Vector3 finalPos = pointAlongLineAtZHeight + (modifiedPosToTarget * toTargetZRampedValue);
				vTargetDelta = finalPos - vHeliPos;
				fTargetDist = vTargetDelta.Mag();
			}
		}

		Vector3 vTargetDir = (fTargetDist > FLOAT_EPSILON) ? (vTargetDelta * (1.0f / fTargetDist)) : VEC3V_TO_VECTOR3(pHeli->GetTransform().GetForward());

		// make sure we are above the terrain
		// or probes will make us go down
		bool bAboveTerrain = vHeliPos.z >= CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vHeliPos.x, vHeliPos.y);
				
		if (bAvoidObstacles && bAboveTerrain)
		{
			// Note: intentionally use fwTimer::GetTimeStep() here: when timeslicing, we do this
			// every frame.
			const float timeStep = fwTimer::GetTimeStep();

			// Detect obstacles
			UpdateAvoidance(pHeli, vTargetDir, fTargetDist, timeStep, fDesiredSpeed);

			float fBrakeMag = m_vBrakeVector.Mag();
			float fBrakeStrength = fDesiredSpeed > 0.0f ? Min( fBrakeMag / fDesiredSpeed, 0.9f) : 0.0f;
			float fDotTarget = (1.0f + m_vAvoidanceDir.Dot(vTargetDir))/2.0f;
			float fDotUp = (1.0f + m_vAvoidanceDir.Dot(g_UnitUp))/2.0f;
			float fDot = Max(fDotTarget, fDotUp); 
			Vector3 vDesiredVelocity = m_vAvoidanceDir * fDesiredSpeed * ( 1.0f - fBrakeStrength) * fDot + m_vBrakeVector * fBrakeStrength;
			Vector3 vModifiedVelocity = vDesiredVelocity;

			// steer to avoid other flying vehicles
			bool bPreventYaw = false;
			bool bSteeringChanged = false;
			if ( !(m_iHeliFlags & HF_DontDoAvoidance) )
			{		
				bSteeringChanged = m_flyingAvoidanceHelper.SteerAvoidCollisions(vModifiedVelocity, bPreventYaw, vDesiredVelocity, in_vTargetPos);
			}

			// If we are avoiding, store a time stamp.
			if(bSteeringChanged || m_fAvoidanceTimerRightLeft > 0.0f || m_fAvoidanceTimerUpDown > 0.0f)
			{
				m_uTimesliceLastAvoidTime = fwTimer::GetTimeInMilliseconds();

				// Make sure that we keep getting full updates if we are avoiding.
				CVehicleAILod& lod = pHeli->GetVehicleAiLod();
				if(lod.IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
				{
					pHeli->m_nVehicleFlags.bLodForceUpdateThisTimeslice = true;
				}
			}

			// Fly along direction that appears to be clear
			SteerToVelocity(pHeli, vModifiedVelocity, bPreventYaw);
		}
		else if (fTargetDist > FLOAT_EPSILON)
		{
			if (m_bRunningLandingTask && pHeli->m_bUseDesiredZCruiseSpeedForLanding)
			{
				pHeli->DisableTurbulenceThisFrame(true);

				vTargetDir.SetZ(0.0f);
				vTargetDir.Normalize();

				Vector3 vDesiredVelocity = vTargetDir * fDesiredSpeed; //xy movementspeed is scaled to help nail the target in XY axis
				vDesiredVelocity.SetZ(-GetCruiseSpeed()); //z movement should be at the full cruise speed

				// Fly directly to target
				SteerToVelocity(pHeli, vDesiredVelocity, false);
			}
			else
			{
				Vector3 vDesiredVelocity = vTargetDir * fDesiredSpeed;
				Vector3 vModifiedVelocity = vDesiredVelocity;

				// Fly directly to target
				SteerToVelocity(pHeli, vModifiedVelocity, false);
			}
		}
	}

#if DEBUG_DRAW
	m_fPrevDesiredSpeed = fDesiredSpeed;
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SteerToVelocity
// PURPOSE :  Makes an AI heli go along a direction vector.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleGoToHelicopter::SteerToVelocity(CHeli *pHeli, const Vector3& vDesiredVelocity, bool preventYaw)
{
#if HELICOPTER_PROFILING
	PF_AUTO_PUSH_TIMEBAR("SteerToVelocity");
	PF_FUNC(SteerToVelocity);
#endif // #if HELICOPTER_PROFILING

	// Initialise PID controllers here for tuning
	pHeli->GetHeliIntelligence()->GetPitchController().Setup(sm_Tunables.m_leanKp, sm_Tunables.m_leanKi, sm_Tunables.m_leanKd);
	pHeli->GetHeliIntelligence()->GetRollController().Setup(sm_Tunables.m_leanKp, sm_Tunables.m_leanKi, sm_Tunables.m_leanKd);
	pHeli->GetHeliIntelligence()->GetYawController().Setup(sm_Tunables.m_yawKp, sm_Tunables.m_yawKi, sm_Tunables.m_yawKd);
	pHeli->GetHeliIntelligence()->GetThrottleController().Setup(sm_Tunables.m_throttleKp, sm_Tunables.m_throttleKi, sm_Tunables.m_throttleKd);

	// Set pitch, roll and throttle using PID controllers with velocity delta as the input
	const Vector3 vHeliVelocity = pHeli->GetVelocity();
	const Vector3 vVelocityDelta = vDesiredVelocity - vHeliVelocity;
	const Vector3 vHeliForward = VEC3V_TO_VECTOR3(pHeli->GetVehicleForwardDirection());

#if DEBUG_DRAW
	m_vVelocityDelta = vVelocityDelta;
#endif
	
	if(!(m_iHeliFlags&HF_DontModifyPitch))
	{
		const float fPitchInput = vHeliForward.Dot(vVelocityDelta);
		const float fPitch = -pHeli->GetHeliIntelligence()->GetPitchController().Update(fPitchInput);
		const float fClampedPitch = Clamp(fPitch, -sm_Tunables.m_maxPitchRoll, sm_Tunables.m_maxPitchRoll);
		pHeli->SetPitchControl(fClampedPitch);
	}

	if(!(m_iHeliFlags&HF_DontModifyRoll))
	{
		const Vector3 vHeliRight = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetRight());
		const float fRollInput = vHeliRight.Dot(vVelocityDelta);
		const float fRoll = -pHeli->GetHeliIntelligence()->GetRollController().Update(fRollInput);
		const float fClampedRoll = Clamp(fRoll, -sm_Tunables.m_maxPitchRoll, sm_Tunables.m_maxPitchRoll);
		pHeli->SetRollControl(fClampedRoll);
	}

	if(!(m_iHeliFlags&HF_DontModifyThrottle))
	{
		const Vector3 vHeliUp = VEC3V_TO_VECTOR3(pHeli->GetVehicleUpDirection());
		const float fThrottleInput = vHeliUp.Dot(vVelocityDelta);
		const float fThrottle = pHeli->GetHeliIntelligence()->GetThrottleController().Update(fThrottleInput);
		const float fClampedThrottle = Clamp(fThrottle, -sm_Tunables.m_maxThrottle, sm_Tunables.m_maxThrottle);
		pHeli->SetThrottleControl(1.0f + fClampedThrottle);
	}

	if (!(m_iHeliFlags&HF_DontModifyOrientation))
	{
		dev_float s_MinMag2ToChangeOrientation = 1.5f;
		const float fDesiredVelocityXYMag2 = vDesiredVelocity.XYMag2();
		const float fCurrentOrientation = fwAngle::GetATanOfXY(vHeliForward.x, vHeliForward.y);
		const float fDesiredOrientation = m_iHeliFlags & HF_AttainRequestedOrientation 
			? fwAngle::LimitRadianAngleSafe(m_fHeliRequestedOrientation) 
			: ( fDesiredVelocityXYMag2 > s_MinMag2ToChangeOrientation ?	fwAngle::GetATanOfXY(vDesiredVelocity.x, vDesiredVelocity.y) : fCurrentOrientation );
		const float fOrientationDelta = fwAngle::LimitRadianAngleSafe(fDesiredOrientation - fCurrentOrientation);

		const float fYawControl = -pHeli->GetHeliIntelligence()->GetYawController().Update(fOrientationDelta);
		const float fYawControlClamped = Clamp(fYawControl, -1.0f, 1.0f);
		pHeli->SetYawControl(fYawControlClamped);
	}

	if(preventYaw)
	{
		pHeli->SetYawControl(0.0f);
	}
}
 
//
// sample the heightmap and return the largest slope amongst the samples
// 
float CTaskVehicleGoToHelicopter::ComputeTargetZToAvoidTerrain(CHeli& in_Heli, const Vector3& vTargetPosition)
{
	Vector3 vHeliPos = VEC3V_TO_VECTOR3(in_Heli.GetTransform().GetPosition());
	Vector3 vHeliDirXY = vTargetPosition - vHeliPos;
	float fDistXY = vHeliDirXY.XYMag();
	float targetZ = vTargetPosition.z;

	const float s_fFutureTime = 3.0f;
	Vector3 vHeliFuturePos = VEC3V_TO_VECTOR3(in_Heli.GetTransform().GetPosition()) + (in_Heli.GetVelocity() * s_fFutureTime); 

	bool maintainingTerrainHeight = (m_iHeliFlags & HF_MaintainHeightAboveTerrain) != 0;
	bool maintainHeight = !(m_iHeliFlags & HF_ForceHeightMapAvoidance) && (maintainingTerrainHeight || fDistXY < sm_Tunables.m_DistanceXYToUseHeightMapAvoidance);

	// check if we should use ground probes or height map
	if (in_Heli.m_nVehicleFlags.bDisableHeightMapAvoidance ||
			( maintainHeight && in_Heli.IsCollisionLoadedAroundPosition() &&
			  g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(vHeliFuturePos), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER)
			)
		)
	{
		// avoid terrain mins.  should avoid mountains and stuff.  also should project for long distances
		targetZ = VehicleNavigationHelper::ComputeTargetZToAvoidHeightMap(vHeliPos, in_Heli.GetAiXYSpeed(), vTargetPosition, (float)m_iMinHeightAboveTerrain, 
												CGameWorldHeightMap::GetMinHeightFromWorldHeightMap, !maintainingTerrainHeight, !maintainingTerrainHeight );

		// Clamp to min height above ground
		if (m_iMinHeightAboveTerrain > 0)
		{
			if (m_targetLocationTestResults.GetResultsReady())
			{
				m_fPreviousTargetHeight = vTargetPosition.z;
				if (m_targetLocationTestResults.GetNumHits() > 0)
				{	
					m_fPreviousTargetHeight = m_targetLocationTestResults[0].GetPosition().GetZf() + static_cast<float>(m_iMinHeightAboveTerrain);
				}

				float fWaterHeight = -LARGE_FLOAT;
				if (Water::GetWaterLevelNoWaves(vTargetPosition, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
				{
					m_fPreviousTargetHeight = rage::Max(m_fPreviousTargetHeight, fWaterHeight + m_iMinHeightAboveTerrain);
				}

				m_targetLocationTestResults.Reset();
			}

			if (m_startLocationTestResults.GetResultsReady())
			{
				float fDesiredStartHeight = vHeliPos.z;
				if (m_startLocationTestResults.GetNumHits() > 0)
				{	
					fDesiredStartHeight = m_startLocationTestResults[0].GetPosition().GetZf() + static_cast<float>(m_iMinHeightAboveTerrain);
				}

				float fWaterHeight = -LARGE_FLOAT;
				if (Water::GetWaterLevelNoWaves(vHeliPos, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
				{
					fDesiredStartHeight = rage::Max(fDesiredStartHeight, fWaterHeight + m_iMinHeightAboveTerrain);
				}

				float fDeltaZ = fDesiredStartHeight - vHeliPos.z;
				if ( fDeltaZ >= 1.0f )
				{
					dev_float s_DeltaZPerSecond = 15.0f;
					m_fAvoidanceTimerRightLeft = Max(0.0f, (fDesiredStartHeight - vHeliPos.z) / s_DeltaZPerSecond );
					m_fAvoidanceTimerUpDown = Max(0.0f, (fDesiredStartHeight - vHeliPos.z) / s_DeltaZPerSecond );

					m_bHasAvoidanceDir = true;
					m_vAvoidanceDir = g_UnitUp;			
				}

				m_startLocationTestResults.Reset();
			}

			if (!m_targetLocationTestResults.GetWaitingOnResults() )
			{	
				Vector3 vStartPos(vTargetPosition.x, vTargetPosition.y, vTargetPosition.z + (float)m_iMinHeightAboveTerrain);
				Vector3 vEndPos(vTargetPosition.x, vTargetPosition.y, vTargetPosition.z - (float)m_iMinHeightAboveTerrain);
				WorldProbe::CShapeTestHitPoint hitPoint;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_RIVER_TYPE);
				probeDesc.SetExcludeInstance(in_Heli.GetCurrentPhysicsInst());
				probeDesc.SetResultsStructure(&m_targetLocationTestResults);
				probeDesc.SetMaxNumResultsToUse(1);
				probeDesc.SetStartAndEnd(vStartPos, vEndPos);
				WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST); // @TODO: WorldProbe::PERFORM_SYNCHRONOUS_TEST
			}

			if(maintainingTerrainHeight)
			{				
				//will lerp between height above terrain and target height when slope to target Z is greater than 45%
// 				if(m_fPreviousTargetHeight > targetZ)
// 				{
// 					float targetZDesiredDiff = m_fPreviousTargetHeight - targetZ;
// 					float slopeToTarget = fDistXY / targetZDesiredDiff;
// 					targetZ = RampValue(slopeToTarget, 0.1f, 1.0f, m_fPreviousTargetHeight, targetZ);
// 				}
			}
			else
			{
				targetZ = Max( targetZ, m_fPreviousTargetHeight );
			}			
		}
	}
	else
	{
		targetZ = VehicleNavigationHelper::ComputeTargetZToAvoidHeightMap(vHeliPos, in_Heli.GetAiXYSpeed(), vTargetPosition, (float)m_iMinHeightAboveTerrain, 
																			CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap);
	}
		
	return targetZ;
}

	
spdSphere& HelperComputeCompositeSphere(spdSphere& o_Sphere, const CEntity& in_Entity)
{
	if ( in_Entity.GetIsTypeVehicle() )
	{
		const CVehicle& vehicle = static_cast<const CVehicle&>(in_Entity);	
		const CEntity* pEntity = vehicle.GetEntityBeingTowed();
		if ( pEntity )
		{
			o_Sphere.GrowSphere(pEntity->GetBoundSphere());
			HelperComputeCompositeSphere(o_Sphere, *pEntity);
		}
	}

	// grab the attachments
	CEntity* pChild = static_cast<CEntity*>(in_Entity.GetChildAttachment());
	while ( pChild )
	{
		o_Sphere.GrowSphere(pChild->GetBoundSphere());
		HelperComputeCompositeSphere(o_Sphere, *pChild);
		pChild = static_cast<CEntity*>(pChild->GetSiblingAttachment());				
	}

	return o_Sphere;
}


/////////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateAvoidance
// PURPOSE :  Updates direction to move in to avoid obstacles.
/////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleGoToHelicopter::UpdateAvoidance(CHeli* pHeli, const Vector3& vTargetDir, const float fTargetDist, float timeStep, float desiredSpeed)
{
#if HELICOPTER_PROFILING
	PF_AUTO_PUSH_TIMEBAR("UpdateAvoidance");
	PF_FUNC(UpdateAvoidance);
#endif // #if HELICOPTER_PROFILING

	spdSphere compositeSphere = pHeli->GetBoundSphere();
	//HelperComputeCompositeSphere(compositeSphere, *pHeli);
	const Vector3 vHeliCenter = VEC3V_TO_VECTOR3(compositeSphere.GetCenter());
	const Vector3 vHeliPos = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition()); //needed as terrain comparisons use pos, not bound center

	if (m_fAvoidanceTimerRightLeft > 0.0f)
	{
		m_fAvoidanceTimerRightLeft = Max(0.0f, m_fAvoidanceTimerRightLeft - timeStep);
	}

	if (m_fAvoidanceTimerUpDown > 0.0f)
	{
		m_fAvoidanceTimerUpDown = Max(0.0f, m_fAvoidanceTimerUpDown - timeStep);
	}

	if (!m_bHasAvoidanceDir)
	{
		m_vBrakeVector.Zero();
		m_vAvoidanceDir = vTargetDir;
		if (m_vAvoidanceDir.Mag2() <= FLOAT_EPSILON)
		{
			m_vAvoidanceDir = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetForward());
		}

		m_fAvoidanceTimerRightLeft = 0.0f;
		m_fAvoidanceTimerUpDown = 0.0f;
		m_bHasAvoidanceDir = true;
		m_avoidanceTestResults[AD_Forward].Reset();
		m_avoidanceTestResults[AD_Left].Reset();
		m_avoidanceTestResults[AD_Right].Reset();
		m_avoidanceTestResults[AD_Down].Reset();
		m_avoidanceTestResults[AD_Up].Reset();

		return;
	}

	Vector3 vAvoidanceDirRight, vAvoidanceDirUp;
	vAvoidanceDirRight.Cross(m_vAvoidanceDir, Vector3(0.0f, 0.0f, 1.0f));
	vAvoidanceDirRight.NormalizeSafe(VEC3V_TO_VECTOR3(pHeli->GetTransform().GetRight()));
	vAvoidanceDirUp.Cross(vAvoidanceDirRight, m_vAvoidanceDir);
	vAvoidanceDirUp.NormalizeSafe(VEC3V_TO_VECTOR3(pHeli->GetTransform().GetUp()));
	const Vector3 vHeliVelocity = pHeli->GetVelocity();

	if (m_avoidanceTestResults[AD_Forward].GetResultsReady() && 
		m_avoidanceTestResults[AD_Left].GetResultsReady() && 
		m_avoidanceTestResults[AD_Right].GetResultsReady() && 
		m_avoidanceTestResults[AD_Down].GetResultsReady() &&
		m_avoidanceTestResults[AD_Up].GetResultsReady() )
		{
	#if HELICOPTER_PROFILING
			PF_AUTO_PUSH_TIMEBAR("UpdateAvoidance.ProcessProbes");		
			PF_FUNC(UpdateAvoidance_ProcessProbes);		
	#endif // #if HELICOPTER_PROFILING
		float fTargetDist2 = ((m_iHeliFlags & HF_DontClampProbesToDestination) > 0) ? 10000.0f : square(fTargetDist);
		float fBestDistance2[AD_COUNT] = { fTargetDist2, fTargetDist2, fTargetDist2, fTargetDist2, fTargetDist2 };
		float fDownLockTimer = sm_Tunables.m_avoidLockDuration;
		int hitCount = 0;
		for(int i = 1; i < AD_COUNT; i++ )
		{
			if ( m_avoidanceTestResults[i][0].GetHitDetected())
			{
				if ( i == AD_Left || i == AD_Right )
				{
					m_fAvoidanceTimerRightLeft = sm_Tunables.m_avoidLockDuration;
				}
				else if ( i == AD_Up )
				{
					m_fAvoidanceTimerUpDown = sm_Tunables.m_avoidLockDuration;
				}
				else if(i == AD_Down)
				{
					//only react to down hits that are closer than our min height to terrain
					Vector3 toHit = vHeliPos - VEC3V_TO_VECTOR3(m_avoidanceTestResults[i][0].GetHitPositionV());
					if( toHit.z < m_iMinHeightAboveTerrain )
					{
						if(m_iHeliFlags & HF_MaintainHeightAboveTerrain)
						{
							float fZDelta = pHeli->GetTransform().GetForward().GetZf();
							float fToHitZDelta = m_iMinHeightAboveTerrain - toHit.z;
							float fTerrainHeightBuffer = RampValue(fZDelta, sm_Tunables.m_downHitZDeltaBuffer, 0.0f, 0.0f, 1.5f);

							if(fZDelta > sm_Tunables.m_downHitZDeltaBuffer && fToHitZDelta < fTerrainHeightBuffer)
							{
								//we have just skimmed our target height, use shorter lock so we don't rise too high
								m_fAvoidanceTimerUpDown = sm_Tunables.m_downAvoidLockDurationMaintain;
								fDownLockTimer = sm_Tunables.m_downAvoidLockDurationMaintain;
							}
							else
							{
								m_fAvoidanceTimerUpDown = sm_Tunables.m_avoidLockDuration;
							}
						}
						else
						{
							m_fAvoidanceTimerUpDown = sm_Tunables.m_avoidLockDuration;
						}
					}
				}

				//count the number of hits that are below us
				if(m_avoidanceTestResults[i][0].GetHitPositionV().GetZf() < vHeliCenter.z)
				{
					++hitCount;
				}
				
				fBestDistance2[i] = DistSquared(m_avoidanceTestResults[i][0].GetHitPositionV(), VECTOR3_TO_VEC3V(vHeliCenter) ).Getf();

#if DEBUG_DRAW
				probeHits[i].bHit = true;
				probeHits[i].vHitLocation = VEC3V_TO_VECTOR3(m_avoidanceTestResults[i][0].GetHitPositionV());
#endif
			}
#if DEBUG_DRAW
			else{ probeHits[i].bHit = false; }
#endif
			
			//
			// let's do some hacky water probing here
			// 
			if ( i == AD_Down )
			{
				float fWaterHeight = -LARGE_FLOAT;
				if (Water::GetWaterLevelNoWaves(m_vDownProbeEndPoint, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
				{
					if ( m_vDownProbeEndPoint.z < fWaterHeight )
					{
						float fDist2 = square(vHeliPos.z - fWaterHeight);

						if ( fDist2 < fBestDistance2[i] && (vHeliPos.z - fWaterHeight) < m_iMinHeightAboveTerrain )
						{
							++hitCount;
							m_fAvoidanceTimerUpDown = sm_Tunables.m_avoidLockDuration;
							fBestDistance2[i] = fDist2;
#if DEBUG_DRAW
							probeHits[i].bHit = true;
							probeHits[i].vHitLocation = m_vDownProbeEndPoint;
							probeHits[i].vHitLocation.z = fWaterHeight;
#endif
						}		
					}
				}
			}	
		}

		bool isStabalising = false;
		if ( m_avoidanceTestResults[AD_Forward][0].GetHitDetected() )
		{
			//check the case where most of our probes are hitting the ground beneath us and we are falling
			if(hitCount > 2 && vHeliVelocity.z < 0.0f)
			{
				static dev_float groundLimit = 400.0f;
				//if the down probe is close to the ground as well, just stabilize and go up
				if( fBestDistance2[AD_Down] < groundLimit)
				{
					m_vAvoidanceDir = Vector3(0.0f,0.0f,1.0f);
					isStabalising = true;
				}
			}
			else
			{
				const static float minSpeed = 1.0f;
				if( vHeliVelocity.XYMag() < minSpeed)
				{
					//we are hardly moving, but still hitting object, so move away from direction of hit
					m_vBrakeVector = VEC3V_TO_VECTOR3(Normalize(VECTOR3_TO_VEC3V(vHeliCenter) - m_avoidanceTestResults[AD_Forward][0].GetHitPositionV()));
					m_vBrakeVector *= desiredSpeed * 0.75f;
				}
				else
				{
					m_vBrakeVector = -vHeliVelocity;
				}

				m_vBrakeVector.z = Max(0.0f, m_vBrakeVector.z);
#if DEBUG_DRAW
				probeHits[AD_Forward].bHit = true;
				probeHits[AD_Forward].vHitLocation = VEC3V_TO_VECTOR3(m_avoidanceTestResults[AD_Forward][0].GetHitPositionV());
#endif
			}
		}
		else
		{  
			dev_float s_BreakDecayInSeconds = 1.0f;
			m_vBrakeVector *= (1.0f - (timeStep / s_BreakDecayInSeconds));
#if DEBUG_DRAW
			probeHits[AD_Forward].bHit = false;
#endif
		}

		if(!isStabalising)
		{
			Quaternion qRotation(Quaternion::IdentityType);
			if ( m_fAvoidanceTimerRightLeft <= 0.0f && m_fAvoidanceTimerUpDown <= 0.0f )
			{		
				dev_float s_AxisAngleDegreesPerSecond = 360.0f;
				Vector3 vAxis;
				vAxis.Cross(m_vAvoidanceDir, vTargetDir);
				qRotation.FromRotation(vAxis, s_AxisAngleDegreesPerSecond * DtoR * timeStep);
			}
			else
			{
				Quaternion qHeading(Quaternion::IdentityType);
				Quaternion qPitch(Quaternion::IdentityType);

				float headingAngle = sm_Tunables.m_avoidHeadingChangeSpeed * DtoR * timeStep;
				float pitchAngle = sm_Tunables.m_avoidPitchChangeSpeed * DtoR * timeStep;

				const float s_DeltaThreshold = 4.0f;
				const float s_UpThreshold = 0.707f;

				if (  m_fAvoidanceTimerRightLeft <= 0.0f && fabsf(m_vAvoidanceDir.Dot(g_UnitUp)) <= s_UpThreshold )
				{
					Vector3 vTargetDirProj = vTargetDir - vTargetDir.Dot(vAvoidanceDirUp)*vAvoidanceDirUp; 
					headingAngle = Min(m_vAvoidanceDir.Angle(vTargetDirProj), headingAngle);
					m_fPreviousHeadingChange = vAvoidanceDirRight.Dot(vTargetDirProj) >= 0.0f ? -headingAngle : headingAngle;
					qHeading.FromRotation(vAvoidanceDirUp, m_fPreviousHeadingChange );
				}
				else if ( m_fAvoidanceTimerRightLeft == sm_Tunables.m_avoidLockDuration )
				{
					//when we are dropping to terrain we don't want to react to left/right probes as much as as
					//they'll be hitting the ground more than objects that are actually in the way
					bool droppingToTerrain = (m_iHeliFlags & HF_MaintainHeightAboveTerrain) && vHeliVelocity.z < 0.0f;
					float droppingDelta = droppingToTerrain ? 100.0f : s_DeltaThreshold;
					float headingChange = droppingToTerrain ? (headingAngle*0.5f) : headingAngle;

					if ( fabsf(fBestDistance2[AD_Right] - fBestDistance2[AD_Left]) >= droppingDelta )
					{
						m_fPreviousHeadingChange = (fBestDistance2[AD_Right] > fBestDistance2[AD_Left]) ? -headingChange : headingChange;
					}

					qHeading.FromRotation(vAvoidanceDirUp, m_fPreviousHeadingChange);
				}

				if (  m_fAvoidanceTimerUpDown <= 0.0f )
				{
					Vector3 vTargetDirProj = vTargetDir - vTargetDir.Dot(vAvoidanceDirRight)*vAvoidanceDirRight; 
					pitchAngle = Min(m_vAvoidanceDir.Angle(vTargetDirProj), pitchAngle);
					m_fPreviousPitchChange = vAvoidanceDirRight.Dot(vTargetDir) >= 0.0f ? -pitchAngle : pitchAngle;
					qPitch.FromRotation(vAvoidanceDirRight, m_fPreviousPitchChange);
				}
				else if ( m_fAvoidanceTimerUpDown == fDownLockTimer )
				{
					if ( fabsf(fBestDistance2[AD_Up] - fBestDistance2[AD_Down]) >= s_DeltaThreshold )
					{
						m_fPreviousPitchChange = (fBestDistance2[AD_Down] > fBestDistance2[AD_Up]) ? -pitchAngle : pitchAngle;	
					}

					qPitch.FromRotation(vAvoidanceDirRight, m_fPreviousPitchChange);
				}

				qRotation.Multiply(qPitch, qHeading);	
			}

			Vector3 vResult;
			qRotation.Transform(m_vAvoidanceDir, vResult);
			m_vAvoidanceDir.Normalize(vResult);
		}

		m_avoidanceTestResults[AD_Forward].Reset();
		m_avoidanceTestResults[AD_Left].Reset();
		m_avoidanceTestResults[AD_Right].Reset();
		m_avoidanceTestResults[AD_Down].Reset();
		m_avoidanceTestResults[AD_Up].Reset();
	}

	if (!m_avoidanceTestResults[AD_Forward].GetWaitingOnResults() && 
		!m_avoidanceTestResults[AD_Left].GetWaitingOnResults() && 
		!m_avoidanceTestResults[AD_Right].GetWaitingOnResults() && 
		!m_avoidanceTestResults[AD_Down].GetWaitingOnResults() &&
		!m_avoidanceTestResults[AD_Up].GetWaitingOnResults())
		{

	#if HELICOPTER_PROFILING
			PF_AUTO_PUSH_TIMEBAR("UpdateAvoidance.GenerateProbes");
			PF_FUNC(UpdateAvoidance_GenerateProbes);		
	#endif // #if HELICOPTER_PROFILING

			// Extend the forward whisker as the helicopter moves faster
			//const Vector3 vHeliForward = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetForward());

			float fForwardTestTime = sm_Tunables.m_whiskerForwardSpeedScale;

			// If timesliced, bump up the probe distance based on the additional time
			// it may take between the timesliced updates.
			if(pHeli->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing))
			{
				fForwardTestTime += CVehicleAILodManager::GetExpectedWorstTimeslicedTimestep();
			}

			float fForwardTestDist = vHeliVelocity.Mag() * fForwardTestTime;
		 
			const SpatialHeliTuning& spatialHeliTuning = QuerySpatialHeliTuning(vHeliPos);

			// Shorten whiskers as we approach the target
			// @TODO: Improve this
			static dev_float s_BoundRadiusNearTunable = 0.5f;
			float fBoundRadiusFarTunable = spatialHeliTuning.FarProbeScalar;
			
			const float fBoundRadius = compositeSphere.GetRadiusf();
			const float fMaxTestDist = fTargetDist;

			// should we clamp probes to dist to target
			if ( !(HF_DontClampProbesToDestination & m_iHeliFlags) )
			{
				fForwardTestDist = Min(fForwardTestDist, fMaxTestDist);
			}
		
			float fNearBoundRadiusScalar = fBoundRadius * s_BoundRadiusNearTunable;
			float fFarBoundRadiusScalar = Max( Min(fBoundRadius * fBoundRadiusFarTunable, fMaxTestDist), fNearBoundRadiusScalar + sm_Tunables.m_avoidMinFarExtension);

			Vector3 vVelocityDir;
			vVelocityDir.Normalize(vHeliVelocity);
			Vector3 vVelocityProbeEndPoint = vHeliPos + (vVelocityDir * sm_Tunables.m_avoidForwardExtraOffset) + (fForwardTestDist * vVelocityDir);
			Vector3 vProbeEndPoint = vHeliPos + m_vAvoidanceDir * fForwardTestDist;

			// Setup and submit the whisker probes
#if !DEBUG_DRAW
		WorldProbe::CShapeTestCapsuleDesc probeDesc[AD_COUNT];
#endif

		//increase radius of probe if going slowly to provide more coverage around vehicle
		float fRadius = RampValue(vHeliVelocity.Mag(), 1.0f, 5.0f, 3.5f, 2.0f);

		phInst* pHookInst = NULL;
		if(pHeli->GetIsCargobob())
		{
			//exclude cargobob hook, if it has one attached
			for(int j = 0; j < pHeli->GetNumberOfVehicleGadgets(); j++)
			{
				CVehicleGadget *pVehicleGadget = pHeli->GetVehicleGadget(j);
				if(pVehicleGadget && (pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET))
				{
					CVehicleGadgetPickUpRope* pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
					if(pPickUpRope)
					{
						const CEntity* pHookEntity = pPickUpRope->GetPropObject();
						if(pHookEntity)
						{
							pHookInst = pHookEntity->GetCurrentPhysicsInst();
						}
					}
				}
			}
		}

		bool bLimitProbeDown = (m_iHeliFlags & HF_MaintainHeightAboveTerrain) != 0 && vHeliVelocity.z < 0.0f;
		for(int i = 0; i < AD_COUNT; i++)
		{
			//if flying close to terrain, only avoid terrain, else avoid all object types as well
			probeDesc[i].SetIncludeFlags((m_iHeliFlags & HF_MaintainHeightAboveTerrain) != 0 ? ArchetypeFlags::GTA_ALL_MAP_TYPES : ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			probeDesc[i].SetExcludeInstance(pHeli->GetCurrentPhysicsInst());
			VehicleNavigationHelper::HelperExcludeCollisionEntities(probeDesc[i], *pHeli);
			probeDesc[i].SetResultsStructure(&m_avoidanceTestResults[i]);
			probeDesc[i].SetMaxNumResultsToUse(1);
			probeDesc[i].SetIsDirected(true);
			probeDesc[i].SetDoInitialSphereCheck(true);
			if(pHookInst)
			{
				probeDesc[i].AddExludeInstance(pHookInst);
			}

			if(i == AD_Forward)
			{
				probeDesc[i].SetCapsule(vHeliPos + (vVelocityDir * sm_Tunables.m_avoidForwardExtraOffset), vVelocityProbeEndPoint, fRadius);
			}
			else if(i == AD_Left)
			{
				Vector3 probeEndPoint = vProbeEndPoint - (vAvoidanceDirRight * fFarBoundRadiusScalar);
				if( bLimitProbeDown )
				{
					probeEndPoint.z = Max(probeEndPoint.z, vHeliPos.z + vHeliVelocity.z);
				}			
				probeDesc[i].SetCapsule(vHeliPos - (vAvoidanceDirRight * fNearBoundRadiusScalar), probeEndPoint, fRadius);
			}
			else if(i == AD_Right)
			{
				Vector3 probeEndPoint = vProbeEndPoint + (vAvoidanceDirRight * fFarBoundRadiusScalar);
				if( bLimitProbeDown )
				{
					probeEndPoint.z = Max(probeEndPoint.z, vHeliPos.z + vHeliVelocity.z);
				}
				probeDesc[i].SetCapsule(vHeliPos + (vAvoidanceDirRight * fNearBoundRadiusScalar), probeEndPoint, fRadius);
			}
			else if(i == AD_Down)
			{   
				m_vDownProbeEndPoint = vProbeEndPoint - (vAvoidanceDirUp * fFarBoundRadiusScalar);
				probeDesc[i].SetCapsule(vHeliPos - (vAvoidanceDirUp * fNearBoundRadiusScalar), m_vDownProbeEndPoint, fRadius);
			}
			else if(i == AD_Up)
			{   
				probeDesc[i].SetCapsule(vHeliPos + (vAvoidanceDirUp * fNearBoundRadiusScalar), vProbeEndPoint + (vAvoidanceDirUp * fFarBoundRadiusScalar), fRadius);
			}

#if HELICOPTER_PROFILING
			PF_PUSH_TIMEBAR("UpdateAvoidance.submit_probes");
			PF_START(UpdateAvoidance_submit_probes);
#endif // #if HELICOPTER_PROFILING
			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc[i], WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

#if HELICOPTER_PROFILING
			PF_STOP(UpdateAvoidance_submit_probes);
			PF_POP_TIMEBAR();
#endif 
		}
	}
}
 
bool CTaskVehicleGoToHelicopter::WasAvoidingRecently() const
{
	// Check if the avoidance time stamp was set, and if that happened within the last few seconds.
	return m_uTimesliceLastAvoidTime && (fwTimer::GetTimeInMilliseconds() - m_uTimesliceLastAvoidTime <= sm_Tunables.m_TimesliceTimeAfterAvoidanceMs);
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleGoToHelicopter::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_GoTo&&iState<=State_Land);
	static const char* aStateNames[] = 
	{
		"State_GoTo",
		"State_Land"
	};

	return aStateNames[iState];
}

void CTaskVehicleGoToHelicopter::Debug() const
{
#if DEBUG_DRAW

	Vector3 target;
	FindTargetCoors(GetVehicle(), target);

	//Vector3 position = VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition());

	Vec3V vHeliPos = GetVehicle()->GetTransform().GetPosition();
	Vec3V vTargetPos = VECTOR3_TO_VEC3V(target);		

	grcDebugDraw::Arrow(vHeliPos, vTargetPos, 1.0f, Color_white);
	grcDebugDraw::Arrow(vHeliPos, vHeliPos + VECTOR3_TO_VEC3V(m_vVelocityDelta), 1.0f, Color_blue);
	grcDebugDraw::Arrow(vHeliPos, vHeliPos + VECTOR3_TO_VEC3V(m_vBrakeVector), 1.0f, Color_red);

	static bool s_DrawSpatialHeliTuning = false;
	if ( s_DrawSpatialHeliTuning )
	{
		grcDebugDraw::BoxAxisAligned(s_NarrowSpatialHeliTuning.Region.GetMin(), s_NarrowSpatialHeliTuning.Region.GetMax(), Color_red, false );
	}

	static bool s_DrawModifiedTarget = false;
	if ( s_DrawModifiedTarget )
	{
		Vector3 vModifiedTarget;
		CFlyingVehicleAvoidanceManager::SlideDestinationTarget(vModifiedTarget, 
										RegdVeh(const_cast<CVehicle*>(GetVehicle())), VEC3V_TO_VECTOR3(vTargetPos), m_fFlyingVehicleAvoidanceScalar, false);
		grcDebugDraw::Arrow(vTargetPos, VECTOR3_TO_VEC3V(vModifiedTarget), 1.0f, Color_black);
	}

	if ( m_iHeliFlags & HF_AttainRequestedOrientation  )
	{
		Vector3 vDirection(3.0f, 0.0f, 0.0f);
		Vec3V vOffsetDown = Vec3V(0.0f, 0.0f, -2.0f);
		vDirection.RotateZ(m_fHeliRequestedOrientation);
		grcDebugDraw::Arrow(vHeliPos + vOffsetDown, vHeliPos + vOffsetDown + VECTOR3_TO_VEC3V(vDirection), 1.0f, Color_green2);
	}

	for (u32 i = 0; i < AD_COUNT; ++i)
	{
		Color32 colors[AD_COUNT] = { Color_yellow, Color_red, Color_green, Color_purple, Color_blue };

		static bool s_DrawProbes = true;
		if( s_DrawProbes )
		{
			static bool s_SolidFill = false;		
			grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(probeDesc[i].GetStart()), VECTOR3_TO_VEC3V(probeDesc[i].GetEnd()), probeDesc[i].GetRadius(),  colors[i], s_SolidFill);
		}

		static bool s_DrawProbeHits = true;
		if ( s_DrawProbeHits && probeHits[i].bHit )
		{
			const float s_HitSize = 0.5f;
			grcDebugDraw::Sphere(probeHits[i].vHitLocation, s_HitSize, colors[i]);
		}
	}

	static bool s_DrawAvoidanceDir = true;
	if(s_DrawAvoidanceDir)
	{
		grcDebugDraw::Arrow(vHeliPos, vHeliPos + (VECTOR3_TO_VEC3V(m_vAvoidanceDir * 2.0f)), 1.0f, Color_orange2);
	}

	static bool s_DrawInput = false;
	if( s_DrawInput ){
		char speedText[32];
		sprintf(speedText, "Speed: %.3f, Desired: %.3f", GetVehicle()->GetVelocity().Mag(), m_fPrevDesiredSpeed);
		grcDebugDraw::Text(vHeliPos, Color_orange2, 0, 10, speedText);

		CHeliIntelligence* pHeliIntelligence = static_cast<CHeliIntelligence*>(GetVehicle()->GetIntelligence());
		CVehPIDController& pitchContr = pHeliIntelligence->GetPitchController();
		CVehPIDController& rollContr = pHeliIntelligence->GetRollController();
		CVehPIDController& yawContr = pHeliIntelligence->GetYawController();
		CVehPIDController& throttleContr = pHeliIntelligence->GetThrottleController();

		char controllerDebugText[128];

		sprintf(controllerDebugText, "Pitch: %.3f, %.3f, %.3f -> %.3f",
			pitchContr.GetPreviousInput(), pitchContr.GetIntegral(), pitchContr.GetPreviousDerivative(), pitchContr.GetPreviousOutput());
		grcDebugDraw::Text(vHeliPos, Color_black, 0, 20, controllerDebugText);

		sprintf(controllerDebugText, "Roll: %.3f, %.3f, %.3f -> %.3f",
			rollContr.GetPreviousInput(), rollContr.GetIntegral(), rollContr.GetPreviousDerivative(), rollContr.GetPreviousOutput());
		grcDebugDraw::Text(vHeliPos, Color_black, 0, 30, controllerDebugText);

		sprintf(controllerDebugText, "Yaw: %.3f, %.3f, %.3f -> %.3f",
			yawContr.GetPreviousInput(), yawContr.GetIntegral(), yawContr.GetPreviousDerivative(), yawContr.GetPreviousOutput());
		grcDebugDraw::Text(vHeliPos, Color_black, 0, 40, controllerDebugText);

		sprintf(controllerDebugText, "Throttle: %.3f, %.3f, %.3f -> %.3f",
			throttleContr.GetPreviousInput(), throttleContr.GetIntegral(), throttleContr.GetPreviousDerivative(), throttleContr.GetPreviousOutput());
		grcDebugDraw::Text(vHeliPos, Color_black, 0, 50, controllerDebugText);

		sprintf(controllerDebugText, "IsLanding Blocked %s", pHeliIntelligence->IsLandingAreaBlocked() ? "True" : "False");
		grcDebugDraw::Text(vHeliPos, Color_black, 0, 60, controllerDebugText);

		sprintf(controllerDebugText, "DefaultSpatialHeliTuning %s", &QuerySpatialHeliTuning(VEC3V_TO_VECTOR3(vHeliPos)) == &s_DefaultSpatialHeliTuning ? "True" : "False");
		grcDebugDraw::Text(vHeliPos, Color_black, 0, 70, controllerDebugText);
	}

	static bool s_DrawFutureLocations = true;
	if ( s_DrawFutureLocations )
	{
		grcDebugDraw::Sphere(m_vModifiedTarget, Max(m_Params.m_fTargetArriveDist, 1.0f), Color_BlueViolet, false);		
	}
	   
#endif //__BANK

	CTaskVehicleGoTo::Debug();
}

#endif