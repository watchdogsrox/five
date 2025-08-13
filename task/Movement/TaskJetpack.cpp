#include "Task/Movement/TaskJetpack.h"

#include "Peds/Ped.h"
#include "grcore/debugdraw.h"
#include "Task/Vehicle/TaskMountAnimalWeapon.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/gameplay/follow/FollowPedCamera.h"
#include "task/Motion/TaskMotionBase.h"
#include "Task/System/MotionTaskData.h"
#include "Task/Movement/TaskFall.h"
#include "system/control.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/JetpackObject.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Vehicle/TaskCar.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "Vfx/Systems/VfxGadget.h"
#include "scene\world\GameWorldHeightMap.h"
#include "Pickups/Pickup.h"
#include "vehicleAi/VehicleNavigationHelper.h"

AI_OPTIMISATIONS()

#if ENABLE_JETPACK

CTaskJetpack::Tunables CTaskJetpack::sm_Tunables;

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskJetpack, 0xaf8a8260);

#if __BANK

bool CTaskJetpack::ms_bDebugControls = false;
bool CTaskJetpack::ms_bDebugAngles = false;
bool CTaskJetpack::ms_bDebugAI = false;
Vector3 CTaskJetpack::ms_vTargetPositionGoTo(-3.5f, 74.0f, 30.0f);
Vector3 CTaskJetpack::ms_vTargetPositionShoot(20.0f, 74.0f, 30.0f);
bool CTaskJetpack::ms_bGoToCoords(true);
bool CTaskJetpack::ms_bShootAtCoords(true);
float CTaskJetpack::ms_fAbortRange = 0.0f;
float CTaskJetpack::ms_fProbeHeight = 50.0f;
Vector3 CTaskJetpack::ms_vTargetLocationTestStart(0.0f, 0.0f, 0.0f);
Vector3 CTaskJetpack::ms_vTargetLocationTestEnd(0.0f, 0.0f, 0.0f);
Vector3 CTaskJetpack::ms_vHeightmapSamplePosition(0.0f, 0.0f ,0.0f);
Vector3 CTaskJetpack::ms_vJetpackTestPoint1(-3.5f, 74.0f, 20.0f);
Vector3 CTaskJetpack::ms_vJetpackTestPoint2(-47.0f, 203.0f, 15.0f);
Vector3 CTaskJetpack::ms_vJetpackTestPoint3(-9.0f, 143.0f, 15.0f);
float CTaskJetpack::ms_fMinHeightAboveSurface(20.0f);
bool CTaskJetpack::ms_bDisableThrustAtDestination = false;
bool CTaskJetpack::ms_bUseRandomTimerWhenClose = false;
#endif

// FLIGHT MODE
CFlightModelHelper CTaskJetpack::ms_FlightModeHelper(
	0.0f,	// Thrust
	0.03f,	// Yaw rate
	0.01f,	// Pitch rate
	0.01f,	// Roll rate
	0.01f,	// Yaw stabilise
	0.01f,	// Pitch stabilise
	0.01f,	// Roll stabilise
	0.001f,	// Form lift mult
	0.015f,	// Attack lift mult
	0.015f, // Attack dive mult
	0.0f,	// Side slip mult
	0.0f,	// throttle fall off,
	Vector3(0.0f,0.0f,0.0f),	// ConstV
	Vector3(0.0f,0.0f,0.0f),	// LinearV
	Vector3(0.0f,0.0f,0.0f),	// SquaredV
	Vector3(1.0f,1.0f,1.0f),	// ConstAV
	Vector3(1.0f,1.0f,1.0f),	// LinearAV
	Vector3(0.0f,1.0f,0.0f)		// SquaredAV
	);

const fwMvRequestId CTaskJetpack::ms_Hover("Hover",0x9578b4fe); 
const fwMvRequestId CTaskJetpack::ms_Flight("Flight",0x2570a839); 

const fwMvBooleanId CTaskJetpack::ms_HoverOnEnter("Hover_OnEnter",0xa219885a);
const fwMvBooleanId CTaskJetpack::ms_FlightOnEnter("Flight_OnEnter",0xfa892f70);

const fwMvStateId CTaskJetpack::ms_HoverState("HoverState",0x19e57ec3);
const fwMvStateId CTaskJetpack::ms_FlightState("FlightState",0xfeb33318);

const fwMvFloatId CTaskJetpack::ms_PitchId("Pitch",0x3f4bb8cc);
const fwMvFloatId CTaskJetpack::ms_RollId("Roll",0xcb2448c6);
const fwMvFloatId CTaskJetpack::ms_YawId("Yaw",0xfdffbe30);
const fwMvFloatId CTaskJetpack::ms_PitchAngleId("PitchAngle",0x809a226d);
const fwMvFloatId CTaskJetpack::ms_RollAngleId("RollAngle",0x94fc4d71);
const fwMvFloatId CTaskJetpack::ms_YawAngleId("YawAngle",0x15de02b1);
const fwMvFloatId CTaskJetpack::ms_AngVelocityXId("AngVelocityX",0x07eef1c4);
const fwMvFloatId CTaskJetpack::ms_AngVelocityYId("AngVelocityY",0x32a9c739);
const fwMvFloatId CTaskJetpack::ms_AngVelocityZId("AngVelocityZ",0x2463aaad);
const fwMvFloatId CTaskJetpack::ms_ThrusterLeftId("ThrusterLeft",0x121a5976);
const fwMvFloatId CTaskJetpack::ms_ThrusterRightId("ThrusterRight",0x4bd3baa2);
const fwMvFloatId CTaskJetpack::ms_SpeedId("Speed",0xf997622b);
const fwMvFloatId CTaskJetpack::ms_HoverModeBoostId("HoverModeBoost",0x22993062);
const fwMvFloatId CTaskJetpack::ms_CombinedThrustId("CombinedThrust",0x0963dcd6);

CTaskJetpack::CTaskJetpack(fwJetpackFlags flags) 
 : m_iFlags(flags)
 , m_fThrusterLeft(0.0f)
 , m_fThrusterRight(0.0f)
 , m_fThrusterLeftInput(0.0f)
 , m_fThrusterRightInput(0.0f)
 , m_fPitch(0.0f)
 , m_fYaw(0.0f)
 , m_fRoll(0.0f)
 , m_fNearGroundTime(0.0f)
 , m_bCrashLanded(false)
 , m_fHoverModeBoost(0.0f)
 , m_fHolsterTimer(0.0f)
 , m_vAngularVelocity(Vector3::ZeroType)
 , m_vAvoidTargetPosition(Vector3::ZeroType)
 , m_vTargetPositionGoTo(V_ZERO)
 , m_vTargetPositionShootAt(V_ZERO)
 , m_vCachedTargetPositionGoToDelay(V_ZERO)
 , m_pTargetEntityGoTo(NULL)
 , m_pTargetEntityShootAt(NULL)
 , m_bDisableThrustAtDestination(false)
 , m_bIsAiming(false)
 , m_bRestartShootingTask(false)
 , m_bTerminateDrivebyTask(false)
 , m_bArrivedAtDestination(false)
 , m_bStopRotationToTarget(false)
 , m_bIsAvoidingEntity(false)
 , m_rotateForAvoid(false)
 , m_fAbortRange(0.0f)
 , m_iFrequencyPercentage(0)
 , m_iFiringPatternHash(0)
 , m_fMinHeightAboveSurface(0.0f)
 , m_fProbeHeight(50.0f)
 , m_fHitObstacleDistance(-1.0f)
 , m_bUseRandomTimerWhenClose(false)
 , m_bFlyingPedAvoidance(true)
 , m_fMovementDelayTime(-1.0f)
 , m_fMovementTimer(-1.0f)
{
	m_targetLocationTestResults = rage_new WorldProbe::CShapeTestSingleResult[NUM_VERTICAL_CAPSULE_TESTS];

	SetInternalTaskType(CTaskTypes::TASK_JETPACK);
}

CTaskJetpack::~CTaskJetpack()
{
	delete[] m_targetLocationTestResults;
}

void CTaskJetpack::CleanUp()
{
	CPed* pPed = GetPed();

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, false);

	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack, false);

	pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, SLOW_BLEND_DURATION);
	m_networkHelper.ReleaseNetworkPlayer();

	//we're done flying
	CFlyingPedCollector::RemovePed(RegdPed(pPed));
}

bool CTaskJetpack::ShouldStickToFloor() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Land:
		{
			return true;
		}
		default:
		break;
	}	

	return false;
}

bool CTaskJetpack::ShouldUseAiControls() const
{
	//Ensure the target is valid.
	if(!m_iFlags.IsFlagSet(JPF_HasTarget) && !m_iFlags.IsFlagSet(JPF_HasShootAtTarget))
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the ped is not a player.
	if(!pPed->IsPlayer())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskJetpack::ShouldUsePlayerControls() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is a local player.
	if(!pPed->IsLocalPlayer())
	{
		return false;
	}

	return true;
}

void CTaskJetpack::SetTargetGoTo(const CPhysical* pEntity, Vec3V_In vPosition)
{
	//Reset arrived flag if position is different
	if (!IsEqualAll(vPosition, m_vTargetPositionGoTo))
	{
		m_bArrivedAtDestination = false;
	}

	//Assign the entity.
	m_pTargetEntityGoTo = pEntity;

	//Assign the position.
	m_vTargetPositionGoTo = vPosition;

	//Check if the target is valid.
	bool bHasTarget = false; 
	if(m_pTargetEntityGoTo)
	{
		bHasTarget = true;
	}
	else if(!IsEqualAll(m_vTargetPositionGoTo, Vec3V(V_ZERO)))
	{
		bHasTarget = true;
	}

	//Change the flag.
	m_iFlags.ChangeFlag(JPF_HasTarget, bHasTarget);
}

void CTaskJetpack::SetTargetEntityGoTo(const CPhysical* pEntity)
{
	SetTargetGoTo(pEntity, Vec3V(V_ZERO));
}

void CTaskJetpack::SetTargetPositionGoTo(Vec3V_In vPosition)
{
	SetTargetGoTo(NULL, vPosition);

	Vector3 vJetpackPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	Vector3 vStartPos(vJetpackPos.x, vJetpackPos.y, vJetpackPos.z + m_fMinHeightAboveSurface);
	Vector3 vEndPos(vJetpackPos.x, vJetpackPos.y, vJetpackPos.z - m_fMinHeightAboveSurface);
	WorldProbe::CShapeTestHitPoint hitPoint;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_RIVER_TYPE);
	probeDesc.SetExcludeInstance(GetPed()->GetCurrentPhysicsInst());
	probeDesc.SetResultsStructure(&m_startLocationTestResults);
	probeDesc.SetMaxNumResultsToUse(1);
	probeDesc.SetStartAndEnd(vStartPos, vEndPos);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST); 
}

void CTaskJetpack::SetTargetShootAt(const CPhysical* pEntity, Vec3V_In vPosition)
{
	//Assign the entity.
	m_pTargetEntityShootAt = pEntity;

	//Assign the position.
	m_vTargetPositionShootAt = vPosition;

	//Check if the target is valid.
	bool bHasTarget = false; 
	if(m_pTargetEntityShootAt)
	{
		bHasTarget = true;
	}
	else if(!IsEqualAll(m_vTargetPositionShootAt, Vec3V(V_ZERO)))
	{
		bHasTarget = true;
	}

	//Change the flag.
	m_iFlags.ChangeFlag(JPF_HasShootAtTarget, bHasTarget);
}

void CTaskJetpack::SetTargetEntityShootAt(const CPhysical* pEntity)
{
	SetTargetShootAt(pEntity, Vec3V(V_ZERO));
}

void CTaskJetpack::SetTargetPositionShootAt(Vec3V_In vPosition)
{
	SetTargetShootAt(NULL, vPosition);
}

void CTaskJetpack::SetGoToParametersForAI(CPhysical *pTargetEntity, Vector3 vTargetPosition, float fMinHeightAboveSurface, bool bDisableThrustAtDestination, bool bUseRandomTimerWhenClose, float fProbeHeight)
{
	m_fMinHeightAboveSurface = fMinHeightAboveSurface;
	m_bDisableThrustAtDestination = bDisableThrustAtDestination;
	m_fProbeHeight = fProbeHeight;
	m_bUseRandomTimerWhenClose = bUseRandomTimerWhenClose;
	SetTargetGoTo(pTargetEntity, VECTOR3_TO_VEC3V(vTargetPosition));
}

void CTaskJetpack::SetShootAtParametersForAI(CPhysical *pTargetEntity, Vector3 vTargetPos, float fAbortRange, int iFrequencyPercentage, int iFiringPatternHash)
{
	SetTargetShootAt(pTargetEntity, VECTOR3_TO_VEC3V(vTargetPos));
	m_bIsAiming = true;
	m_fAbortRange = fAbortRange;
	m_iFrequencyPercentage = iFrequencyPercentage;
	m_iFiringPatternHash = iFiringPatternHash;
	m_bRestartShootingTask = true;
}

Vec3V_Out CTaskJetpack::CalculateTargetGoToPosition() 
{
	Vec3V vTargetPos(V_ZERO);
	//Check if the target entity is valid.
	if(m_pTargetEntityGoTo)
	{
		vTargetPos = m_pTargetEntityGoTo->GetTransform().GetPosition();
		//Transform the offset and add it to the target position (if we have one)
		if (!IsEqualAll(m_vTargetPositionGoTo, Vec3V(V_ZERO)))
		{
			vTargetPos += m_pTargetEntityGoTo->GetTransform().Transform3x3(m_vTargetPositionGoTo);
		}
	}
	else
	{
		//Use the target position.
		vTargetPos = m_vTargetPositionGoTo;
	}

#if __BANK
	if (ms_bDebugAI)
	{
		grcDebugDraw::Sphere(vTargetPos, 0.2f, Color_blue, true, -1);

		if( m_bIsAvoidingEntity)
		{
			grcDebugDraw::Sphere(m_vAvoidTargetPosition, 0.5f, Color_red, false, -1);
		}
	}
#endif

	if (m_bUseRandomTimerWhenClose)
	{
		// Use a small random delay if the AI ped is relatively close to the target entity and the position moves
		// before allowing the AI to move as well
		Vec3V vPedPos = GetPed()->GetTransform().GetPosition();
		Vec3V vPedToTarget = Subtract(vTargetPos, vPedPos);
		float fDistToTarget = VEC3V_TO_VECTOR3(vPedToTarget).Mag2();
		static dev_float fDistanceThreshold = 400.0f;
		bool bAllowMovement = true;
		if (fDistToTarget <= fDistanceThreshold)
		{
			bAllowMovement = false;
			if (m_fMovementDelayTime == -1.0f)
			{
				static dev_float fMinTime = 4.0f;
				static dev_float fMaxTime = 10.0f;
				m_fMovementDelayTime = fwRandom::GetRandomNumberInRange(fMinTime, fMaxTime);
				m_vCachedTargetPositionGoToDelay = vTargetPos;
				m_fMovementTimer = 0.0f;
			}

			if (m_fMovementDelayTime != -1.0f)
			{
				m_fMovementTimer += fwTimer::GetTimeStep();

				if (m_fMovementTimer >= m_fMovementDelayTime)
				{
					bAllowMovement = true;
					m_fMovementDelayTime = -1.0f;
					m_fMovementTimer = -1.0f;
				}
			}
		}
		else
		{
			m_vCachedTargetPositionGoToDelay = vTargetPos;
			m_fMovementDelayTime = -1.0f;
			m_fMovementTimer = -1.0f;
		}

		if (!bAllowMovement)
		{
			vTargetPos = m_vCachedTargetPositionGoToDelay;
		}

#if __BANK
		if (ms_bDebugAI)
		{
			Color32 sphereColor = Color_blue;
			if (fDistToTarget <= fDistanceThreshold)
			{
				sphereColor = Color_yellow;
			}
			grcDebugDraw::Sphere(vTargetPos, 0.75f, sphereColor, false, -1);
		}
#endif
	}

	return vTargetPos;
}

Vec3V_Out CTaskJetpack::CalculateTargetShootAtPosition() const
{
	//Check if the target entity is valid.
	if(m_pTargetEntityShootAt)
	{
		Vec3V vTargetPos = m_pTargetEntityShootAt->GetTransform().GetPosition();
		//Transform the offset and add it to the target position (if we have one)
		if (!IsEqualAll(m_vTargetPositionShootAt, Vec3V(V_ZERO)))
		{
			vTargetPos += m_pTargetEntityShootAt->GetTransform().Transform3x3(m_vTargetPositionShootAt);
		}
		return vTargetPos;
	}
	else
	{
		//Use the target position.
		return m_vTargetPositionShootAt;
	}
}

Vec3V_Out CTaskJetpack::CalculateTargetPositionFuture()
{
	//Calculate the target position.
	Vec3V vTargetPosition = CalculateTargetGoToPosition();

	//Ensure the target entity is valid.
	if(!m_pTargetEntityGoTo)
	{
		return vTargetPosition;
	}

	//Ensure we are hovering.
	if(GetState() != State_Hover)
	{
		return vTargetPosition;
	}

	//Ensure the jetpack is valid.
	if(!GetJetpack()->GetObject())
	{
		return vTargetPosition;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Generate a vector from the ped to the target.
	Vec3V vPedToTarget = Subtract(vTargetPosition, vPedPosition);

	//Ensure the ped is above the target.
	ScalarV scOffsetZ = vPedToTarget.GetZ();
	if(IsGreaterThanAll(scOffsetZ, ScalarV(V_ZERO)))
	{
		return vTargetPosition;
	}

	//Grab the target velocity.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(m_pTargetEntityGoTo->GetVelocity());

	//Grab the jetpack velocity.
	Vec3V vJetpackVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());

	//Ensure the ped will reach the target eventually.
	ScalarV scVelocityZ = Subtract(vJetpackVelocity.GetZ(), vTargetVelocity.GetZ());
	if(IsGreaterThanAll(scVelocityZ, ScalarV(V_ZERO)))
	{
		return vTargetPosition;
	}

	//Ensure the velocity difference is valid.
	scVelocityZ = Min(ScalarV(V_NEGONE), scVelocityZ);

	//Figure out how long it will take the ped to descend to the target height.
	ScalarV scTime = Scale(scOffsetZ, Invert(scVelocityZ));
	if(IsLessThanAll(scTime, ScalarV(V_ZERO)))
	{
		return vTargetPosition;
	}

	//Clamp the time.
	scTime = Clamp(scTime, ScalarV(V_ZERO), ScalarV(3.0f));//ScalarV(sm_Tunables.m_MaxTimeToLookAheadForFutureTargetPosition));

	//Calculate the future target position.
	Vec3V vFutureTargetPosition = AddScaled(vTargetPosition, vTargetVelocity, scTime);

	return vFutureTargetPosition;
}

bool CTaskJetpack::CalcDesiredVelocity(const Matrix34& UNUSED_PARAM(mUpdatedPed), float UNUSED_PARAM(fTimeStep), Vector3& UNUSED_PARAM(vDesiredVelocity)) const
{
	//! TO DO - override velocity if necessary.

	return false;
}

void CTaskJetpack::GetPitchConstraintLimits(float& UNUSED_PARAM(fMinOut), float& UNUSED_PARAM(fMaxOut)) const
{
	//! not used right now.
}

bool CTaskJetpack::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed();

	if(GetState() == State_Hover)
	{
		ProcessHoverPhysics(pPed, fTimeStep);
	}
	else if(GetState() == State_Flight)
	{
		ProcessFlightPhysics(pPed);
	}

	return true;
}

void CTaskJetpack::ProcessPreRender2()
{
	CPed* pPed = GetPed();
	CObject* pJetPackObject = static_cast<CObject*>(GetJetpack()->GetObject());
	if (pPed && pJetPackObject)
	{
		s32 nozzleBoneIndex = -1;
		if (pJetPackObject->GetSkeletonData().ConvertBoneIdToIndex((u16)Nozzle_Up_R, nozzleBoneIndex))
		{
			g_vfxGadget.UpdatePtFxJetPackNozzle(pPed, pJetPackObject, nozzleBoneIndex, m_fThrusterLeft, m_fThrusterRight, 0);
		}

		if (pJetPackObject->GetSkeletonData().ConvertBoneIdToIndex((u16)Nozzle_Up_L, nozzleBoneIndex))
		{
			g_vfxGadget.UpdatePtFxJetPackNozzle(pPed, pJetPackObject, nozzleBoneIndex, m_fThrusterLeft, m_fThrusterRight, 1);
		}

		if (pJetPackObject->GetSkeletonData().ConvertBoneIdToIndex((u16)Nozzle_Lo_R, nozzleBoneIndex))
		{
			g_vfxGadget.UpdatePtFxJetPackNozzle(pPed, pJetPackObject, nozzleBoneIndex, m_fThrusterLeft, m_fThrusterRight, 2);
		}

		if (pJetPackObject->GetSkeletonData().ConvertBoneIdToIndex((u16)Nozzle_Lo_L, nozzleBoneIndex))
		{
			g_vfxGadget.UpdatePtFxJetPackNozzle(pPed, pJetPackObject, nozzleBoneIndex, m_fThrusterLeft, m_fThrusterRight, 3);
		}
	}
}

float CalcAccel(float fCurrentVel, float fDesiredVel, float fControlMult, float fMaxAccel)
{
	// Velocity diff
	float fTemp = fDesiredVel - fCurrentVel;

	// Accel is proportional to this diff
	fTemp *= fControlMult;

	// Clamp Accel
	fTemp = rage::Clamp(fTemp, -fMaxAccel, fMaxAccel);

	return fTemp;
}


void CTaskJetpack::ProcessHoverPhysics(CPed *pPed, float fTimeStep)
{
	Vector3 vAccel = VEC3_ZERO;
	
	Vector3 vInput(m_fRoll, m_fPitch, 0.0f);
	vInput.z = m_fThrusterRight - m_fThrusterLeft;

	Vector3 vDesiredLocalSpeed = vInput;

	float fThrustSpeed = GetUpThrust();

	float fThrustMag = fThrustSpeed;
	if(sm_Tunables.m_HoverControlParams.m_bAntiGravityThrusters)
	{
		fThrustMag = Abs(m_fThrusterRight - m_fThrusterLeft);
	}

	Vector3 vMinStrafeSpeed = sm_Tunables.m_HoverControlParams.m_vMinStrafeSpeedMinThrust + ((sm_Tunables.m_HoverControlParams.m_vMinStrafeSpeedMaxThrust - sm_Tunables.m_HoverControlParams.m_vMinStrafeSpeedMinThrust) * fThrustMag);
	Vector3 vMaxStrafeSpeed = sm_Tunables.m_HoverControlParams.m_vMaxStrafeSpeedMinThrust + ((sm_Tunables.m_HoverControlParams.m_vMaxStrafeSpeedMaxThrust - sm_Tunables.m_HoverControlParams.m_vMaxStrafeSpeedMinThrust) * fThrustMag);

	if(m_fHoverModeBoost > 0.0f)
	{
		Vector3 vMinStrafeSpeedBoost = sm_Tunables.m_HoverControlParams.m_vMinStrafeSpeedMinThrustBoost + ((sm_Tunables.m_HoverControlParams.m_vMinStrafeSpeedMaxThrustBoost - sm_Tunables.m_HoverControlParams.m_vMinStrafeSpeedMinThrustBoost) * m_fHoverModeBoost);
		Vector3 vMaxStrafeSpeedBoost = sm_Tunables.m_HoverControlParams.m_vMaxStrafeSpeedMinThrustBoost + ((sm_Tunables.m_HoverControlParams.m_vMaxStrafeSpeedMaxThrustBoost - sm_Tunables.m_HoverControlParams.m_vMaxStrafeSpeedMinThrustBoost) * m_fHoverModeBoost);
	
		vMinStrafeSpeed.Lerp(m_fHoverModeBoost, vMinStrafeSpeed, vMinStrafeSpeedBoost);
		vMaxStrafeSpeed.Lerp(m_fHoverModeBoost, vMaxStrafeSpeed, vMaxStrafeSpeedBoost);
	}

	vDesiredLocalSpeed.x *= vDesiredLocalSpeed.x >= 0.0f ? vMaxStrafeSpeed.x : vMinStrafeSpeed.x;
	vDesiredLocalSpeed.y *= vDesiredLocalSpeed.y >= 0.0f ? vMaxStrafeSpeed.y : vMinStrafeSpeed.y;
	vDesiredLocalSpeed.z *= vDesiredLocalSpeed.z >= 0.0f ? vMaxStrafeSpeed.z : vMinStrafeSpeed.z;

	Vector3 vCurrentLocalSpeed = pPed->GetVelocity();
	vCurrentLocalSpeed = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vCurrentLocalSpeed)));

	Vector3 vAccelLimit;
	vAccelLimit.x = vDesiredLocalSpeed.x - vCurrentLocalSpeed.x >= 0.0f ? sm_Tunables.m_HoverControlParams.m_vMaxStrafeAccel.x : sm_Tunables.m_HoverControlParams.m_vMinStrafeAccel.x;
	vAccelLimit.y = vDesiredLocalSpeed.y - vCurrentLocalSpeed.y >= 0.0f ? sm_Tunables.m_HoverControlParams.m_vMaxStrafeAccel.y : sm_Tunables.m_HoverControlParams.m_vMinStrafeAccel.y;
	vAccelLimit.z = vDesiredLocalSpeed.z - vCurrentLocalSpeed.z >= 0.0f ? sm_Tunables.m_HoverControlParams.m_vMaxStrafeAccel.z : sm_Tunables.m_HoverControlParams.m_vMinStrafeAccel.z;

	vAccel.x = CalcAccel(vCurrentLocalSpeed.x,vDesiredLocalSpeed.x,sm_Tunables.m_HoverControlParams.m_fHorThrustModifier,vAccelLimit.x);
	vAccel.y = CalcAccel(vCurrentLocalSpeed.y,vDesiredLocalSpeed.y,sm_Tunables.m_HoverControlParams.m_fForThrustModifier,vAccelLimit.y);
	vAccel.z = CalcAccel(vCurrentLocalSpeed.z,vDesiredLocalSpeed.z,sm_Tunables.m_HoverControlParams.m_fVerThrustModifier,vAccelLimit.z);

	//! Vertical acceleration (assume z is always up).
	if(sm_Tunables.m_HoverControlParams.m_bAntiGravityThrusters)
	{
		float fGrav = -CPhysics::GetGravitationalAcceleration();
		vAccel.z -= (fGrav * sm_Tunables.m_HoverControlParams.m_fGravityDamping);
	}
	else
	{
		vAccel.z = fThrustSpeed;
		vAccel.z *= sm_Tunables.m_HoverControlParams.m_fVerThrustModifier;

		//! If standing, reset accel so that we don't cause ped to jitter about.
		if(pPed->GetIsStanding() && vAccel.z <= 0.0f)
		{
			vAccel.z = 0.0f;
		}
		else
		{
			float fThrustSpeedInput = m_fThrusterLeftInput + m_fThrusterRightInput;

			//! If accelerating upwards, but not providing thrust, attempt to more aggressively pull jetpack. This gives more responsive controls.
			float fDecelerationModifier = vCurrentLocalSpeed.z > 0.0f && fThrustSpeedInput <= 0.001f ? sm_Tunables.m_HoverControlParams.m_fGravityDecelerationModifier : 1.0f;

			//! Gravity damping is a constant acceleration provided by the boosters. So, if 0.5f, then we fall at half the speed of gravity.
			vAccel.z += (CPhysics::GetGravitationalAcceleration() * sm_Tunables.m_HoverControlParams.m_fGravityDamping * fDecelerationModifier);
		}
	}

	vAccel.Scale(pPed->GetMass());

	if(sm_Tunables.m_HoverControlParams.m_bDoRelativeToCam)
	{
		camInterface::GetPlayerControlCamAimFrame().GetWorldMatrix().Transform3x3(vAccel);
	}
	else
	{
		vAccel = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAccel)));
	}

	CFlightModelHelper::ApplyForceCgSafe(vAccel,pPed);

	bool bForceSyncToCamera = false;

	CTask* pSubTask = GetSubTask();
	if(pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN || pSubTask->GetTaskType() == CTaskTypes::TASK_MOUNT_THROW_PROJECTILE))
	{
		//! If doing drive by, sync to camera heading.
		bForceSyncToCamera = true;
	}
	else if(sm_Tunables.m_HoverControlParams.m_bSyncHeadingToDesired && pPed->IsPlayer())
	{
		// roll with camera. similar to on foot, we basically rotate around when we roll stick.
		float fHeadingModifier = sm_Tunables.m_HoverControlParams.m_fDesiredHeadingModifier + 
			(( sm_Tunables.m_HoverControlParams.m_fDesiredHeadingModifierBoost -  sm_Tunables.m_HoverControlParams.m_fDesiredHeadingModifier) * m_fHoverModeBoost);

		if(pPed->GetControlFromPlayer()->GetPedWalkUpDown().GetNorm() < sm_Tunables.m_HoverControlParams.m_fMinPitchForHeadingChange || 
			abs(pPed->GetControlFromPlayer()->GetPedWalkLeftRight().GetNorm()) < sm_Tunables.m_HoverControlParams.m_fMinYawForHeadingChange )
		{
			//Only heading sync to camera if we're using the ped follow cam or third person aim cam
			const camBaseCamera * pRenderCam = camInterface::GetDominantRenderedCamera();
			if(pRenderCam && (pRenderCam->GetClassId()==camFollowPedCamera::GetStaticClassId()))
			{
				//! Note: This is a hack, and we should remove it when we create a jetpack camera that doesn't lag as much.
				bForceSyncToCamera = true;
			}
		}
		else
		{
			//! updates desired heading and SetExtraHeadingChangeThisFrame.
			CTaskFall::UpdateHeading(pPed, pPed->GetControlFromPlayer(), fHeadingModifier);
		}
	}

	bool bRotateToGoToPosition = !m_bArrivedAtDestination && !m_bStopRotationToTarget;
	bool bRotateToShootAtPosition = !bRotateToGoToPosition && GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN || GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOUNT_THROW_PROJECTILE);

	if((sm_Tunables.m_HoverControlParams.m_bSyncHeadingToCamera || bForceSyncToCamera) && pPed->IsPlayer())
	{
		// sync up our heading with camera (e.g. if we rotate the camera, our ped heading changes too).
		float fCamHeading = camInterface::GetPlayerControlCamHeading(*pPed);
		pPed->SetDesiredHeading(fCamHeading);

		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		float fHeadingDelta = pTask ? pTask->CalcDesiredDirection(): fCamHeading;	
		float fHeadingChangeThisFrame = fHeadingDelta * sm_Tunables.m_HoverControlParams.m_fCameraHeadingModifier * fTimeStep;
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fHeadingChangeThisFrame);
	}

	//If we aren't a player and we have a target position, rotate towards it (unless we have already arrived at our destination)
	//If we have arrived at our GoTo destination but we are running the drive-by task, rotate towards the ShootAt target
	else if (!pPed->IsPlayer() && (bRotateToGoToPosition || bRotateToShootAtPosition))
	{
		Vector3 vTargetPos; vTargetPos.Zero();
		float fHeadingChangeRate = GetPed()->GetHeadingChangeRate();
		if( m_bIsAvoidingEntity && !m_bArrivedAtDestination)
		{
			//if we're not at our target, steer towards avoidance direction
			vTargetPos = m_vAvoidTargetPosition;
		}
		else
		{			
			if (bRotateToGoToPosition || m_rotateForAvoid)
			{
				vTargetPos = VEC3V_TO_VECTOR3(CalculateTargetGoToPosition());
			}
			else if (bRotateToShootAtPosition)
			{
				static dev_float fTunedHeadingChangeRate = 5.0f;
				fHeadingChangeRate = fTunedHeadingChangeRate;
				vTargetPos = VEC3V_TO_VECTOR3(CalculateTargetShootAtPosition());
			}
		}

		Vector3 vPedPos = pPed->GetBonePositionCached(BONETAG_L_FOOT);
		vPedPos.Average(pPed->GetBonePositionCached(BONETAG_R_FOOT));

		const float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPedPos.x, vPedPos.y);
		GetPed()->SetDesiredHeading(fDesiredHeading);

		const float fHeadingDiff = SubtractAngleShorter(fDesiredHeading, GetPed()->GetCurrentHeading());
		const float fHeadingDelta = Min(Abs(fHeadingDiff), fHeadingChangeRate * fwTimer::GetTimeStep());
	
		GetPed()->GetMotionData()->SetExtraHeadingChangeThisFrame(Sign(fHeadingDiff) * fHeadingDelta);	
	}

	//! Ang vel.
	Vector3 vAngularVelocity = GetPed()->GetAngVelocity();
	vAngularVelocity = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAngularVelocity)));
	m_vAngularVelocity.Lerp(sm_Tunables.m_HoverControlParams.m_fAngVelocityLerpRate, m_vAngularVelocity, vAngularVelocity);
}

void CTaskJetpack::ProcessFlightPhysics(CPed *pPed)
{
	float fThrustSpeed = GetUpThrust();

	Vector3 vAccel = VEC3_ZERO;

	//! Apply a forward thrust.
	vAccel.y = fThrustSpeed;
	vAccel.y *= sm_Tunables.m_FlightControlParams.m_fThrustModifier;

	vAccel.Scale(pPed->GetMass());
	vAccel = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAccel)));

	CFlightModelHelper::ApplyForceCgSafe(vAccel,pPed);

	// Adjust the air speed based on the left stick input
	Vector3 vAirSpeedIn = VEC3_ZERO;
	Vector3 vLinearV = VEC3_ZERO;
	vAirSpeedIn = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAirSpeedIn)));

	//Set the linear velocity.
	ms_FlightModeHelper.SetDragCoeff(vLinearV, CFlightModelHelper::FT_Linear_V);

	bool bFLattenVelocity = GetTimeInState() < sm_Tunables.m_FlightControlParams.m_fTimeToFlattenVelocity;

	//Process the flight model.
	ms_FlightModeHelper.ProcessFlightModel(pPed, 0.0f, m_fPitch, m_fRoll, m_fYaw, vAirSpeedIn, true, 1.0f, 1.0f, bFLattenVelocity);

	//! Ang vel.
	Vector3 vAngularVelocity = GetPed()->GetAngVelocity();
	vAngularVelocity = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAngularVelocity)));
	m_vAngularVelocity.Lerp(sm_Tunables.m_FlightControlParams.m_fAngVelocityLerpRate, m_vAngularVelocity, vAngularVelocity);
}

CTask::FSM_Return CTaskJetpack::ProcessPreFSM()
{
	CPed *pPed = GetPed();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);

	if(!DoesPedHaveJetpack(pPed))
	{
		return FSM_Quit;
	}

	if(pPed->GetIsSwimming())
	{
		return FSM_Quit;
	}

	if(pPed->GetCurrentMotionTask() && pPed->GetCurrentMotionTask()->IsUnderWater())
	{
		return FSM_Quit;
	}

	if(ShouldBlockWeaponSwitching())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
	}

	//! Don't allow ped to parachute when using the jetpack as models overlap.
	pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableParachuting);

	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack, true);

	switch(GetState())
	{
	case State_Flight:

		//! The flight physics require us to take control of the peds capsule so that we can roll, pitch, do loop the loops etc :)
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);
		
		//fall through.

	case State_Hover:
		{
			//! process physics.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);

			//! don't scale capsule.
			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);

			//! prevent jitter.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);

			//! Disable probes otherwise we end up sticking to the ground on take off.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableProcessProbes, true);

			//! Don't sync desired to current.
			//pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false);

			break;
		}
	default:
		{
			break;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskJetpack::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* pPed = GetPed();

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_StreamAssets)
			FSM_OnEnter
				StreamAssets_OnEnter(pPed);
			FSM_OnUpdate
				return StreamAssets_OnUpdate(pPed);

		FSM_State(State_Hover)
			FSM_OnEnter
				Hover_OnEnter(pPed);
			FSM_OnUpdate
				return Hover_OnUpdate(pPed);
			FSM_OnExit
				Hover_OnExit(pPed);

		FSM_State(State_Flight)
			FSM_OnEnter
				Flight_OnEnter(pPed);
			FSM_OnUpdate
				return Flight_OnUpdate(pPed);

		FSM_State(State_Fall)
			FSM_OnEnter
			Fall_OnEnter(pPed);
		FSM_OnUpdate
			return Fall_OnUpdate(pPed);

		FSM_State(State_Land)
			FSM_OnEnter
				Land_OnEnter(pPed);
			FSM_OnUpdate
				return Land_OnUpdate(pPed);

		FSM_State(State_Quit)
			FSM_OnUpdate
				return Quit_OnUpdate(pPed);
	FSM_End
}

CTaskJetpack::FSM_Return CTaskJetpack::ProcessPostFSM()
{
	UpdateMoVE();
	return FSM_Continue;
}

void CTaskJetpack::UpdateMoVE()
{
	if(m_networkHelper.IsNetworkActive())
	{
		float fPitchNormalised = (m_fPitch + 1.0f) * 0.5f;
		float fRollNormalised = (m_fRoll + 1.0f) * 0.5f;
		float fYawNormalised = (m_fYaw + 1.0f) * 0.5f;

		Vector3 vAngles;
		GetAnglesForMoVE(vAngles);
		Vector3 vAngVel = GetAngularVelocityForMoVE(m_vAngularVelocity);

		m_networkHelper.SetFloat(ms_PitchId, fPitchNormalised);
		m_networkHelper.SetFloat(ms_RollId, fRollNormalised);
		m_networkHelper.SetFloat(ms_YawId, fYawNormalised);
		m_networkHelper.SetFloat(ms_PitchAngleId, vAngles.x);
		m_networkHelper.SetFloat(ms_RollAngleId, vAngles.y);
		m_networkHelper.SetFloat(ms_YawAngleId, vAngles.z);
		m_networkHelper.SetFloat(ms_AngVelocityXId, vAngVel.x);
		m_networkHelper.SetFloat(ms_AngVelocityYId, vAngVel.y);
		m_networkHelper.SetFloat(ms_AngVelocityZId, vAngVel.z);
		m_networkHelper.SetFloat(ms_ThrusterLeftId, m_fThrusterLeft);
		m_networkHelper.SetFloat(ms_ThrusterRightId, m_fThrusterRight);
		m_networkHelper.SetFloat(ms_SpeedId, GetFlightSpeedForMoVE());
		m_networkHelper.SetFloat(ms_HoverModeBoostId, m_fHoverModeBoost);

		float fCombinedThrust = ((m_fThrusterRight - m_fThrusterLeft) + 1.0f) * 0.5f;
		fCombinedThrust = Clamp(fCombinedThrust, 0.0f, 1.0f);
		m_networkHelper.SetFloat(ms_CombinedThrustId, fCombinedThrust);
	}
}

void CTaskJetpack::Start_OnEnter(CPed* pPed)
{
	//add us to the list of flying peds
	CFlyingPedCollector::AddPed(RegdPed(pPed));
}

CTask::FSM_Return CTaskJetpack::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_StreamAssets);
	return FSM_Continue;
}

void CTaskJetpack::StreamAssets_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	// Get the motion task data set to get our clipset for this ped type
//	CPedModelInfo *pModelInfo = pPed->GetPedModelInfo();
//	const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pModelInfo->GetMotionTaskDataSetHash().GetHash());
}

CTask::FSM_Return CTaskJetpack::StreamAssets_OnUpdate(CPed* pPed)
{
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskJetpack);

	if (m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskJetpack))
	{
		m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskJetpack);
	
		pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION );

		TakeOff(pPed);

		SetState(State_Hover);
	}

	return FSM_Continue;
}

void CTaskJetpack::TakeOff(CPed* pPed)
{
	//Note that the ped is not standing.
	pPed->SetIsStanding(false);

	//Note that the ped is in the air.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, true);

	//Ensure gravity is turned on for the ped.
	pPed->SetUseExtractedZ(false);

	//Destroy the weapon in the ped's hand.
	weaponAssert(pPed->GetWeaponManager());
	pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
}

void CTaskJetpack::Hover_OnEnter(CPed* pPed)
{
	taskAssert(m_networkHelper.IsNetworkActive());

	m_networkHelper.SetClipSet(fwMvClipSetId(sm_Tunables.m_JetpackClipsets.m_HoverClipSetHash));

	//Set the move network state.
	SetMoveNetworkState(ms_Hover, ms_HoverOnEnter, ms_HoverState);

	//Check if we can aim.
	if(CanAim())
	{
		//Backup the equipped weapon.
		pPed->GetWeaponManager()->BackupEquippedWeapon();

		//Equip the best weapon.
		pPed->GetWeaponManager()->EquipBestWeapon();
	}

	m_fNearGroundTime = 0.0f;
}

CTask::FSM_Return CTaskJetpack::Hover_OnUpdate(CPed* pPed)
{
	//Ensure the state has been entered.
	if(!m_networkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(CanActivateKillSwitch(pPed))
	{
		SetState(State_Fall);
		return FSM_Continue;
	}

	if(CheckForLanding(pPed))
	{
		SetState(State_Land);
		return FSM_Continue;
	}

	if(GetTimeInState() > 0.1f && CanActivateFlightMode(pPed))
	{
		SetState(State_Flight);
		return FSM_Continue;
	}

	ProcessHoverControls(pPed);

	//! HACK. Restart jetpack if ambient clip killed it. If jetpack existed at motion task layer, this wouldn't be an issue, but it is :(
	if(!pPed->GetMovePed().GetTaskNetwork())
	{
		pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION );
		SetFlag( aiTaskFlags::RestartCurrentState );
	}

	return FSM_Continue;
}

void CTaskJetpack::Hover_OnExit(CPed* pPed)
{
	//Check if we can aim.
	if(CanAim())
	{
		//Restore the backup weapon.
		pPed->GetWeaponManager()->RestoreBackupWeapon();

		//Destroy the equipped weapon object.
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
	}
}

void CTaskJetpack::Flight_OnEnter(CPed* UNUSED_PARAM(pPed))
{	
	taskAssert(m_networkHelper.IsNetworkActive());

	m_networkHelper.SetClipSet(fwMvClipSetId(sm_Tunables.m_JetpackClipsets.m_FlightClipSetHash));

	//Set the move network state.
	SetMoveNetworkState(ms_Flight, ms_FlightOnEnter, ms_FlightState);
}

CTask::FSM_Return CTaskJetpack::Flight_OnUpdate(CPed* pPed)
{
	//Ensure the state has been entered.
	if(!m_networkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	ProcessFlightControls(pPed);

	if(ScanForCrashCollision())
	{
		//! Force NM.
		m_bCrashLanded = true;
		SetState(State_Fall);
	}

	if(GetTimeInState() > 0.1f && CanActivateFlightMode(pPed))
	{
		SetState(State_Hover);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskJetpack::Fall_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	fwFlags32 fallFlags = FF_DisableLandRoll;
	if(m_bCrashLanded)
	{
		fallFlags.SetFlag(FF_ForceHighFallNM);
	}

	//! Don't land roll with a jetpack :)
	CTaskFall* pNewTask = rage_new CTaskFall(fallFlags);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskJetpack::Fall_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//! Go back to hover if we are in mid-air still?
		SetState(State_Quit);
	}

	return FSM_Continue;
}

void CTaskJetpack::Land_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	//! Don't land roll with a jetpack :)
	fwFlags32 fallFlags = FF_DisableLandRoll | FF_SkipToLand;
	CTaskFall* pNewTask = rage_new CTaskFall(fallFlags);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskJetpack::Land_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Quit);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskJetpack::Quit_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Quit;
}

bool CTaskJetpack::DoesPedHaveJetpack(const CPed *pPed)
{
	//Ensure the ped's inventory is valid.
	const CPedInventory* pPedInventory = pPed->GetInventory();
	if(!pPedInventory)
	{
		return false;
	}

	//Ensure the ped has a jetpack in inventory.
	if(!pPedInventory->GetWeapon(GADGETTYPE_JETPACK))
	{
		return false;
	}

	//Ensure the ped can get a jetpack.
	if(pPed->GetJetpack() == NULL)
	{
		return false;
	}

	return true;
}

void CTaskJetpack::ProcessHoverControls(const CPed *pPed)
{
	// Make sure we're not a clone...
	Assert(GetPed() && !GetPed()->IsNetworkClone());

	//Check if we should use AI controls.
	if(ShouldUseAiControls())
	{
		//Process the parachuting AI controls.
		ProcessHoverControlsForAI(pPed);
	}
	else if(ShouldUsePlayerControls())
	{
		//Process the parachuting player controls.
		ProcessHoverControlsForPlayer(pPed);
	}
}

void CTaskJetpack::ProcessHoverControlsForPlayer(const CPed *pPed)
{
	const CControl *pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		// Use Abs on the norm values as mouse mappings can be inverted, this is due to the way the mappings work
		// (ideally we would alter this but its safer to fix the issue this way).
		m_fThrusterLeftInput = Abs(pControl->GetVehicleFlyYawLeft().GetNorm());
		m_fThrusterRightInput = Abs(pControl->GetVehicleFlyYawRight().GetNorm());

		if ((m_fThrusterLeftInput == 1.0f) || (m_fThrusterRightInput == 1.0f))
		{
			m_fThrusterLeft = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fThrustLerpRate, m_fThrusterLeft, m_fThrusterLeftInput);
			m_fThrusterRight = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fThrustLerpRate, m_fThrusterRight, m_fThrusterRightInput);
		}
		else
		{
			m_fThrusterLeft = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fThrustLerpOutRate, m_fThrusterLeft, m_fThrusterLeftInput);
			m_fThrusterRight = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fThrustLerpOutRate, m_fThrusterRight, m_fThrusterRightInput);
		}

		float fPitchInput = -pControl->GetPedWalkUpDown().GetNorm();
		float fRollInput = pControl->GetPedWalkLeftRight().GetNorm();

		if(pControl->GetPedSprintIsDown())
		{
			//If we have no pitch input from the player, set it as fully forwards
			if (fPitchInput == 0)
			{
				fPitchInput = 1.0f;
			}

			Approach(m_fHoverModeBoost, 1.0f, sm_Tunables.m_HoverControlParams.m_fBoostApproachRateIn, fwTimer::GetTimeStep());
		}
		else
		{
			Approach(m_fHoverModeBoost, 0.0f, sm_Tunables.m_HoverControlParams.m_fBoostApproachRateOut, fwTimer::GetTimeStep());
		}

		if (fPitchInput != 0)
		{
			m_fPitch = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fPitchLerpRate, m_fPitch, fPitchInput);
		}
		else
		{
			m_fPitch = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fPitchLerpOutRate, m_fPitch, fPitchInput);
		}

		if (fRollInput != 0)
		{
			m_fRoll = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fRollLerpRate, m_fRoll, fRollInput);
		}
		else
		{
			m_fRoll = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fRollLerpOutRate, m_fRoll, fRollInput);
		}
		
		bool bAiming = (pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsDown());
	
		if(pPed->IsLocalPlayer())
		{
			ProcessAimControls(bAiming);
		}
	}
	else
	{
		m_fThrusterLeftInput = 0.0f;
		m_fThrusterRightInput = 0.0f;
	}
}

void CTaskJetpack::ProcessHoverControlsForAI(const CPed *pPed)
{
	ProcessAimControls(m_bIsAiming);

	//Calculate the target position.
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(CalculateTargetPositionFuture());

	//Calculate the ped position.
	Vector3 vPedPosition = pPed->GetBonePositionCached(BONETAG_L_FOOT);
	vPedPosition.Average(pPed->GetBonePositionCached(BONETAG_R_FOOT));

	//Calculate if we need to apply some vertical avoidance
	vTargetPosition.z = ComputeTargetZToAvoidTerrain(pPed, vPedPosition, vTargetPosition);

	//Calculate a vector from the ped to the target.
	Vector3 vPedToTarget = vTargetPosition - vPedPosition;

	//give some leeway for AI to move around while still being at the destination
	float fArrivalThreshold = m_bIsAvoidingEntity ? sm_Tunables.m_AvoidanceSettings.m_fArrivalTolerance : 0.75f;
	m_bArrivedAtDestination = vPedToTarget.Mag() < fArrivalThreshold;
	m_bStopRotationToTarget = vPedToTarget.XYMag() < fArrivalThreshold;

	//Stop pitching towards target if we have a drastic change in target Z position (we likely have an obstacle to fly over).
	//or if our XY distance is v small
	static dev_float fThreshold = 10.0f;
	bool bDisablePitch = false;
	bool bShouldGoStraightUp = (m_fHitObstacleDistance <= 10.0f  && m_fHitObstacleDistance >= 0.0f) && (vTargetPosition.z > vPedPosition.z + fThreshold);
	if (bShouldGoStraightUp || (m_bStopRotationToTarget && m_bArrivedAtDestination))
	{
		bDisablePitch = true;
	}

	//Calculate and set jetpack control parameters
	CalculateStickForPitchForJetpackAI(vPedToTarget, bDisablePitch);
	CalculateThrusterValuesForJetpackAI(vPedToTarget.z);
	CalculateStickForRollForJetpackAI();

	static dev_bool s_bEnabledHeadingAvoidance = true;
	if( m_bFlyingPedAvoidance && s_bEnabledHeadingAvoidance )
	{
		CalculateDesiredHeadingForAvoidance(pPed, vPedPosition, vPedToTarget);
	}
}

void CTaskJetpack::CalculateDesiredHeadingForAvoidance(const CPed *pPed, Vector3 vPedPosition, Vector3 vPedToTarget)
{
	m_bIsAvoidingEntity = false;
	m_rotateForAvoid = false;

	Vector3 vVelocity = pPed->GetVelocity();
	float fspeed = vVelocity.Mag2();

	static dev_float fSpeedLimit = 9.0f;
	if( fspeed < fSpeedLimit )
	{
		float distToTarget = vPedToTarget.Mag2();
		if( distToTarget > 100.0f)
		{
			//going slowly and far from target
			for(int i = 0; i < CFlyingPedCollector::m_flyingPeds.GetCount(); i++)
			{
				const CPed* pOtherPed = CFlyingPedCollector::m_flyingPeds[i];
				if( pOtherPed && pPed != pOtherPed)
				{
					Vector3 vOtherPedPosition = pOtherPed->GetBonePositionCached(BONETAG_L_FOOT);
					vOtherPedPosition.Average(pOtherPed->GetBonePositionCached(BONETAG_R_FOOT));
					Vector3 vMeToOtherVehicle = vOtherPedPosition - vPedPosition;
					float fDistance2 = vMeToOtherVehicle.Mag2();
					if( fDistance2 < 9.0f)
					{
						float currentHeading = pPed->GetCurrentHeading();
						vPedToTarget.Normalize();
						float desiredHeading = rage::Atan2f(-vPedToTarget.x, vPedToTarget.y);
						float headingDiff = SubtractAngleShorter(desiredHeading, currentHeading);
						if(fabsf(headingDiff) > DtoR * sm_Tunables.m_AvoidanceSettings.m_fMinTurnAngle)
						{
							//we are very close to someone else, instead of moving, rotate to target first
							//prevents groups hitting each other when realigning to new target
							m_rotateForAvoid = true;
							m_fPitch = 0.0f;
							m_fThrusterRightInput = 0.0f;
							m_fThrusterLeftInput = 0.0f;
						}		
					}
				}
			}
		}
		else
		{
			//going slowly near our destination, steer away from nearby peds
			Vector3 desiredVelocity = Vector3(Vector3::ZeroType);
			float fAvoidDistance2 = sm_Tunables.m_AvoidanceSettings.m_fAvoidanceRadius*sm_Tunables.m_AvoidanceSettings.m_fAvoidanceRadius;

			int groupCount = 0;
			for(int i = 0; i < CFlyingPedCollector::m_flyingPeds.GetCount(); i++)
			{
				const CPed* pOtherPed = CFlyingPedCollector::m_flyingPeds[i];
				if( pOtherPed && pPed != pOtherPed)
				{
					Vector3 vOtherPedPosition = pOtherPed->GetBonePositionCached(BONETAG_L_FOOT);
					vOtherPedPosition.Average(pOtherPed->GetBonePositionCached(BONETAG_R_FOOT));
					//2dify
					vOtherPedPosition.z = vPedPosition.z;

					Vector3 vMeToOtherVehicle = vOtherPedPosition - vPedPosition;
					float fDistance2 = vMeToOtherVehicle.Mag2();
					if( fDistance2 < fAvoidDistance2)
					{
						vMeToOtherVehicle.Normalize();
						vMeToOtherVehicle.Scale(Min(1.0f,1.0f/fDistance2));
						desiredVelocity -= vMeToOtherVehicle;
						++groupCount;
#if __BANK
						if (ms_bDebugAI)
						{
							grcDebugDraw::Line(VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vOtherPedPosition), Color_red, -1);
							grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vPedPosition - vMeToOtherVehicle), 0.2f, Color_blue, -1);
						}
#endif
					}
				}
			}

			if( groupCount > 0)
			{
				desiredVelocity /= (float)groupCount;
				
				//average of avoidance and to target
				//vPedToTarget.Normalize();
				//desiredVelocity.Average(vPedToTarget);		

				desiredVelocity.Normalize();

				//convert with respect to heading
				float currentHeading = pPed->GetCurrentHeading();
				float worldHeading = rage::Atan2f(-desiredVelocity.x, desiredVelocity.y);
				float newHeading = worldHeading - currentHeading;
				newHeading = fwAngle::LimitRadianAngle( newHeading );

				//turn into pitch and roll
				float fDesiredPitch = Clamp(rage::Cosf( newHeading ), -1.0f, 1.0f);
				float fDesiredRoll = Clamp(-rage::Sinf( newHeading ), -1.0f, 1.0f);

				fDesiredPitch *= sm_Tunables.m_AvoidanceSettings.m_fAvoidanceMultiplier;
				fDesiredRoll *= sm_Tunables.m_AvoidanceSettings.m_fAvoidanceMultiplier;;

				m_fPitch = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fPitchLerpRate, m_fPitch, fDesiredPitch);
				m_fRoll = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fRollLerpRate, m_fRoll, fDesiredRoll);

#if __BANK
				if (ms_bDebugAI)
				{
					grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vPedPosition + desiredVelocity), 0.2f, Color_red, -1);
				}
#endif
			}
		}
	}
	else
	{
		//and avoid those that are near destination
		const CPed* closestPed = NULL;
		float avoidanceRadius = sm_Tunables.m_AvoidanceSettings.m_fAvoidanceRadius;
		float closestDistance = avoidanceRadius * avoidanceRadius * 2.0f;

		for(int i = 0; i < CFlyingPedCollector::m_flyingPeds.GetCount(); i++)
		{
			const CPed* pOtherPed = CFlyingPedCollector::m_flyingPeds[i];
			if( pOtherPed && pPed != pOtherPed)
			{
				Vector3 vOtherPedPosition = pOtherPed->GetBonePositionCached(BONETAG_L_FOOT);
				vOtherPedPosition.Average(pOtherPed->GetBonePositionCached(BONETAG_R_FOOT));

				Vector3 vMeToOtherVehicle = vOtherPedPosition - vPedPosition;
				float fDistance2 = vMeToOtherVehicle.Mag2();
				if( fDistance2 < closestDistance)
				{
					Vector3 direction = pPed->GetVelocity();
					direction.Normalize();
					vMeToOtherVehicle.Normalize();
					float dotDirection = direction.Dot(vMeToOtherVehicle);
					if( dotDirection > 0.0f)
					{
						closestDistance = fDistance2;
						closestPed = pOtherPed;
					}
				}
			}
		}

		if ( closestPed != NULL )
		{
			Vector3 direction = vPedToTarget;// pPed->GetVelocity();
			direction.z = 0.0f;
			direction.Normalize();

			Vector3 vFuturePosition = vPedPosition + direction * closestDistance;
			Vector3 vToFuturePosition = vFuturePosition - vPedPosition;

			Vector3 vOtherPedPosition = closestPed->GetBonePositionCached(BONETAG_L_FOOT);
			vOtherPedPosition.Average(closestPed->GetBonePositionCached(BONETAG_R_FOOT));
			//2dify
			vOtherPedPosition.z = vPedPosition.z;
			vFuturePosition.z = vPedPosition.z;

			const float vTValue = geomTValues::FindTValueOpenSegToPoint(vPedPosition, vToFuturePosition, vOtherPedPosition);
			if ( vTValue > 0.0f )
			{
				Vector3 vClosestPoint = vPedPosition + (vToFuturePosition * vTValue); 
				Vector3 vCenterToClosestDir = vClosestPoint - vOtherPedPosition;
				float distance2 = vCenterToClosestDir.Mag2();
				if ( distance2 > FLT_EPSILON && distance2 < avoidanceRadius*avoidanceRadius)
				{
					m_bIsAvoidingEntity = true;
					float distance = sqrt(distance2);
					vCenterToClosestDir *= 1/distance;

					//find point on circle around other ped tangential to us and closest to original direction
					Vector3 vTargetOffset = vOtherPedPosition + vCenterToClosestDir * avoidanceRadius; 
					Vector3 vDirToNewTarget = vTargetOffset - vPedPosition;
					vDirToNewTarget.Normalize();

					float distanceToTarget = vPedToTarget.Mag();
					//push new target away from us by the current distance to our target
					m_vAvoidTargetPosition = vPedPosition + ( vDirToNewTarget * distanceToTarget);
#if __BANK
					if (ms_bDebugAI)
					{
						grcDebugDraw::Sphere(vOtherPedPosition, 0.2f, Color_green, false, -1);
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vOtherPedPosition), VECTOR3_TO_VEC3V(vTargetOffset), 0.2f, Color_blue, -1);
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(m_vAvoidTargetPosition), 0.2f, Color_red, -1);
					}
#endif
				}
			}
		}
	}
}

void CTaskJetpack::CalculateThrusterValuesForJetpackAI(float fDistanceZ)
{
	bool bDisableThrust = m_bArrivedAtDestination && m_bDisableThrustAtDestination;
	if (bDisableThrust)
	{
		m_fThrusterRightInput = 0.0f;
		m_fThrusterLeftInput = 0.0f;
	}
	else
	{
		float fThrust = 0.0f;
		float fDesiredThrust = 1.0f;
		static dev_float fSlowDownDistance = 10.0f;
		static dev_float fSlowDownDistanceMin = 0.0f;
		//Check if the slow down distance is valid.
		if(fSlowDownDistance > 0.0f)
		{
			//Slow down as we approach the target.
			fDesiredThrust *= Min(1.0f, Max(Abs(fDistanceZ) - fSlowDownDistanceMin, 0.0f) / fSlowDownDistance);
		}

		//Ensure we apply the thrust in the right direction
		fThrust = fDistanceZ < 0 ? -fDesiredThrust : fDesiredThrust;

		m_fThrusterRightInput = fThrust;
		m_fThrusterLeftInput = 0.0f;
	}

	m_fThrusterLeft = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fThrustLerpRate, m_fThrusterLeft, m_fThrusterLeftInput);
	m_fThrusterRight = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fThrustLerpRate, m_fThrusterRight, m_fThrusterRightInput);
}

void CTaskJetpack::CalculateStickForPitchForJetpackAI(Vector3 vPedToTarget, bool bDisablePitch)
{
	if (bDisablePitch || m_bArrivedAtDestination)
	{
		static dev_float fStopPitchLerpRate = 0.15f;
		m_fPitch = rage::Lerp(fStopPitchLerpRate, m_fPitch, 0.0f);
	}
	else
	{
		float fPitch = 0.0f;

		//Calculate the flat distance from the ped to the target.
		float fTargetDist = vPedToTarget.XYMag2();
		float fDesiredPitch = 1.0f;

		static dev_float fSlowDownDistance = 225.0f;
		static dev_float fSlowDownDistanceMin = 0.0f;
		//Check if the slow down distance is valid.
		if(fSlowDownDistance > 0.0f)
		{
			//Slow down as we approach the target.
			fDesiredPitch *= Min(1.0f, Max( fTargetDist - fSlowDownDistanceMin, 0.0f ) / fSlowDownDistance);
		}

		fPitch = fDesiredPitch;	

		m_fPitch = rage::Lerp(sm_Tunables.m_HoverControlParams.m_fPitchLerpRate, m_fPitch, fPitch);
	}
}

void CTaskJetpack::CalculateStickForRollForJetpackAI() 
{
	m_fRoll = 0.0f;
}

float CTaskJetpack::ComputeTargetZToAvoidTerrain(const CPed* pPed, const Vector3& vJetpackPos, const Vector3& vTargetPosition)
{
	float targetZ = vTargetPosition.z;

	// check if we should use ground probes or height map
	static dev_bool bDisableGroundProbes = false;
	if (!bDisableGroundProbes)
	{
		// avoid terrain mins.  should avoid mountains and stuff.  also should project for long distances
		targetZ = VehicleNavigationHelper::ComputeTargetZToAvoidHeightMap(vJetpackPos, pPed->GetVelocity().XYMag(), vTargetPosition, 
												m_fMinHeightAboveSurface, CGameWorldHeightMap::GetMinHeightFromWorldHeightMap);

		// Clamp to min height above ground
		if (m_fMinHeightAboveSurface > 0)
		{
			bool bDetectedObstacle = false;
			Vector3 vHitPosition; vHitPosition.Zero();
			float fDistFromHitToEndOfCapsule = 0.0f;	//Used to scale vertical probe spread

			Vector3 vStartPos(vJetpackPos.x, vJetpackPos.y, vJetpackPos.z);
			Vector3 vEndPos = vTargetPosition;

			//If distance to target pos is greater than the defined max distance, then scale it down to that length
			float fDistanceToTarget = (vEndPos - vStartPos).Mag();
			static dev_float fMaxCapsuleDistance = 50.0f;
			if (fDistanceToTarget > fMaxCapsuleDistance)
			{
				vEndPos = vEndPos - vStartPos;
				vEndPos.Normalize();
				vEndPos.Scale(fMaxCapsuleDistance);
				vEndPos = vStartPos + vEndPos;
			}

			if (m_startLocationTestResults.GetResultsReady())
			{
				if (m_startLocationTestResults.GetNumHits() > 0)
				{	
					vHitPosition = VEC3V_TO_VECTOR3(m_startLocationTestResults[0].GetPosition());
					m_fHitObstacleDistance = (vHitPosition - vJetpackPos).Mag();
					fDistFromHitToEndOfCapsule = (vEndPos - vHitPosition).Mag();
					bDetectedObstacle = true;
				}
				else
				{
					m_fHitObstacleDistance = -1.0f;
					// No obstacles, reset target height
					m_fPreviousTargetHeight = vTargetPosition.z;
				}
				m_startLocationTestResults.Reset();
			}

			if (!m_startLocationTestResults.GetWaitingOnResults())
			{
				WorldProbe::CShapeTestCapsuleDesc capsuleDescForwards;
				capsuleDescForwards.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_RIVER_TYPE);
				capsuleDescForwards.SetExcludeInstance(GetPed()->GetCurrentPhysicsInst());
				capsuleDescForwards.SetResultsStructure(&m_startLocationTestResults);
				capsuleDescForwards.SetMaxNumResultsToUse(1);
				static dev_float fRadius = 1.0f;
				capsuleDescForwards.SetCapsule(vStartPos, vEndPos, fRadius);
				capsuleDescForwards.SetDoInitialSphereCheck(true);
				capsuleDescForwards.SetIsDirected(true);
				WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDescForwards, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
#if __BANK
				if (ms_bDebugAI)
				{
					ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStartPos), VECTOR3_TO_VEC3V(vEndPos), 1.0f, Color_red, 1, 0, false);
				}
#endif
			}

			// If we detected a hit on the forward-capsule, perform vertical probe tests to determine a target height
			if (bDetectedObstacle)
			{
				if (   m_targetLocationTestResults[0].GetResultsReady()
					&& m_targetLocationTestResults[1].GetResultsReady()
					&& m_targetLocationTestResults[2].GetResultsReady()
					&& m_targetLocationTestResults[3].GetResultsReady()
					&& m_targetLocationTestResults[4].GetResultsReady())
				{
					m_fPreviousTargetHeight = vTargetPosition.z;
					for (int i = 0; i < NUM_VERTICAL_CAPSULE_TESTS; i++)
					{
						if (m_targetLocationTestResults[i].GetNumHits() > 0)
						{
							float fDetectedHeight = m_targetLocationTestResults[i][0].GetPosition().GetZf() + static_cast<float>(m_fMinHeightAboveSurface);
							if (fDetectedHeight > m_fPreviousTargetHeight)
							{	
								m_fPreviousTargetHeight = fDetectedHeight;
							}
						}

						float fWaterHeight = -LARGE_FLOAT;
						if (Water::GetWaterLevelNoWaves(vTargetPosition, &fWaterHeight, POOL_DEPTH, 999999.9f, NULL))
						{
							m_fPreviousTargetHeight = rage::Max(m_fPreviousTargetHeight, fWaterHeight + m_fMinHeightAboveSurface);
						}

						m_targetLocationTestResults[i].Reset();
					}
				}

				if (   !m_targetLocationTestResults[0].GetWaitingOnResults()
					&& !m_targetLocationTestResults[1].GetWaitingOnResults()
					&& !m_targetLocationTestResults[2].GetWaitingOnResults()
					&& !m_targetLocationTestResults[3].GetWaitingOnResults()
					&& !m_targetLocationTestResults[4].GetWaitingOnResults())
				{	
					WorldProbe::CShapeTestHitPoint hitPoint;
					WorldProbe::CShapeTestCapsuleDesc capsuleDesc[NUM_VERTICAL_CAPSULE_TESTS];
					for (int i = 0; i < NUM_VERTICAL_CAPSULE_TESTS; i++)
					{	
						Vector3 vJetpackDirXY = vTargetPosition - vJetpackPos;
						vJetpackDirXY.Normalize();
					
						//Spread out tests from hit position to capsule end position
						float fDistanceMult = fDistFromHitToEndOfCapsule / (float)NUM_VERTICAL_CAPSULE_TESTS;
						fDistanceMult *= (float)i;
						vJetpackDirXY.x *= fDistanceMult; 
						vJetpackDirXY.y *= fDistanceMult;
						Vector3 vSamplePosition = /*vJetpackPos*/ vHitPosition + vJetpackDirXY;
							
						Vector3 vStartPos(vSamplePosition.x, vSamplePosition.y, vSamplePosition.z + m_fProbeHeight);
						Vector3 vEndPos(vSamplePosition.x, vSamplePosition.y, vSamplePosition.z - m_fProbeHeight);
#if __BANK
						if (ms_bDebugAI)
						{
							Color32 capsuleColor = Color_green;
							switch (i)
							{
							case 0:
								capsuleColor = Color_green4;
								break;
							case 1:
								capsuleColor = Color_green4;
								break;
							case 2:
								capsuleColor = Color_green3;
								break;
							case 3:
								capsuleColor = Color_green2;
								break;
							case 4:
								capsuleColor = Color_green;
								break;
							default:
								capsuleColor = Color_green;
								break;
							}
							ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStartPos), VECTOR3_TO_VEC3V(vEndPos), 0.5f, capsuleColor, 1, 0, false);
							ms_vTargetLocationTestStart = vStartPos;
							ms_vTargetLocationTestEnd = vEndPos;
						}
#endif	//__BANK

						capsuleDesc[i].SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_RIVER_TYPE);
						capsuleDesc[i].SetExcludeInstance(GetPed()->GetCurrentPhysicsInst());
						capsuleDesc[i].SetResultsStructure(&m_targetLocationTestResults[i]);
						capsuleDesc[i].SetMaxNumResultsToUse(1);
						static dev_float fRadius = 0.5f;
						capsuleDesc[i].SetCapsule(vStartPos, vEndPos, fRadius);
						capsuleDesc[i].SetDoInitialSphereCheck(false);
						capsuleDesc[i].SetIsDirected(true);
						WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc[i], WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
					}
				}
			}
			targetZ = Max(targetZ, m_fPreviousTargetHeight );
		}
	}
	else
	{
		targetZ = VehicleNavigationHelper::ComputeTargetZToAvoidHeightMap(vJetpackPos, pPed->GetVelocity().XYMag(), 
												vTargetPosition, m_fMinHeightAboveSurface, CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap);
	}

	targetZ = ComputeTargetZToAvoidFlyingPeds(pPed, vJetpackPos,vTargetPosition,targetZ);

	return targetZ;
}

float CTaskJetpack::ComputeTargetZToAvoidFlyingPeds(const CPed* pPed, const Vector3& vJetpackPos, const Vector3& vTargetPosition, float targetZ)
{
	float newTargetZ = targetZ;

	const CPed* pClosestAbove = NULL;
	const CPed* pClosestBelow = NULL;
	float fDistanceAbove = sm_Tunables.m_AvoidanceSettings.m_fVerticalBuffer;
	float fDistanceBelow = -sm_Tunables.m_AvoidanceSettings.m_fVerticalBuffer;
	float fAvoidance2 = sm_Tunables.m_AvoidanceSettings.m_fAvoidanceRadius*sm_Tunables.m_AvoidanceSettings.m_fAvoidanceRadius;

	for(int i = 0; i < CFlyingPedCollector::m_flyingPeds.GetCount(); i++)
	{
		const CPed* pOtherPed = CFlyingPedCollector::m_flyingPeds[i];
		if( pOtherPed && pPed != pOtherPed)
		{
			Vector3 vOtherPedPosition = pOtherPed->GetBonePositionCached(BONETAG_L_FOOT);
			vOtherPedPosition.Average(pOtherPed->GetBonePositionCached(BONETAG_R_FOOT));

			Vector3 vMeToOtherVOed = vOtherPedPosition - vJetpackPos;
			float fVerticalDistance = vMeToOtherVOed.GetZ();
			float fHorizontalDistance = vMeToOtherVOed.XYMag2();
			if(fHorizontalDistance < fAvoidance2)
			{
				if( fVerticalDistance < -0.5f && fVerticalDistance > fDistanceBelow)
				{
					fDistanceBelow = fVerticalDistance;
					pClosestBelow = pOtherPed;
				}
				else if( fVerticalDistance > 0.5f && fVerticalDistance < fDistanceAbove)
				{
					fDistanceAbove = fVerticalDistance;
					pClosestAbove = pOtherPed;
				}
			}
		}
	}

	if( pClosestAbove && pClosestBelow )
	{
		//do nothing
	}
	else if( pClosestBelow )
	{
		//go up
		Vector3 vOtherPedPosition = pClosestBelow->GetBonePositionCached(BONETAG_L_FOOT);
		vOtherPedPosition.Average(pClosestBelow->GetBonePositionCached(BONETAG_R_FOOT));

		newTargetZ = Max(vOtherPedPosition.GetZ() + sm_Tunables.m_AvoidanceSettings.m_fVerticalBuffer, targetZ);
	}
	else if( pClosestAbove )
	{
		Vector3 vOtherPedPosition = pClosestAbove->GetBonePositionCached(BONETAG_L_FOOT);
		vOtherPedPosition.Average(pClosestAbove->GetBonePositionCached(BONETAG_R_FOOT));

		if(vTargetPosition.GetZ() == targetZ) //not avoiding terrain
		{
			//go down
			newTargetZ = Min(vOtherPedPosition.GetZ() - sm_Tunables.m_AvoidanceSettings.m_fVerticalBuffer, targetZ);
		}
		else
		{
			//stay where we are?
			//newTargetZ = vJetpackPos.GetZ();
		}
	}

	return newTargetZ;
}

void CTaskJetpack::ProcessFlightControls(const CPed *pPed)
{
	const CControl *pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		if(sm_Tunables.m_FlightControlParams.m_bThrustersAlwaysOn)
		{
			m_fThrusterLeftInput = 1.0f;
			m_fThrusterRightInput = 1.0f;
		}
		else
		{
			// Use Abs on the norm values as mouse mappings can be inverted, this is due to the way the mappings work
			// (ideally we would alter this but its safer to fix the issue this way).
			m_fThrusterLeftInput = Abs(pControl->GetVehicleFlyYawLeft().GetNorm());
			m_fThrusterRightInput = Abs(pControl->GetVehicleFlyYawRight().GetNorm());
		}

		m_fThrusterLeft = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fThrustLerpRate, m_fThrusterLeft, m_fThrusterLeftInput);
		m_fThrusterRight = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fThrustLerpRate, m_fThrusterRight, m_fThrusterRightInput);

		float fPitchInput = -pControl->GetPedWalkUpDown().GetNorm();
		float fRollInput = pControl->GetPedWalkLeftRight().GetNorm();
		// Use Abs on the norm values as mouse mappings can be inverted, this is due to the way the mappings work
		// (ideally we would alter this but its safer to fix the issue this way).
		float fYawInput = Abs(pControl->GetVehicleFlyYawRight().GetNorm()) - Abs(pControl->GetVehicleFlyYawLeft().GetNorm());

		m_fPitch = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fPitchLerpRate, m_fPitch, fPitchInput);
		m_fRoll = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fRollLerpRate, m_fRoll, fRollInput);
		m_fYaw = rage::Lerp(sm_Tunables.m_FlightControlParams.m_fYawLerpRate, m_fYaw, fYawInput);
	}
	else
	{
		m_fThrusterLeftInput = 0.0f;
		m_fThrusterRightInput = 0.0f;
	}
}

void CTaskJetpack::ProcessAimControls(bool bAiming)
{
	CPed *pPed = GetPed();

	bool bShouldAim = (bAiming && CanAim());

	//Check if our weapon is valid.
	bool bIsWeaponValid = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo() &&
		pPed->GetWeaponManager()->GetEquippedWeaponInfo()->CanBeUsedAsDriveBy(pPed);
	if(!bIsWeaponValid)
	{
		bShouldAim = false;
	}
	else
	{
		//Check if the ped can drive by with the equipped weapon.
		if(!CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPed))
		{
			bShouldAim = false;
		}
	}

	CTask* pSubTask = GetSubTask();

	//Check if we should aim.
	if(bShouldAim)
	{
		//Reset holster timer if we aim again
		m_fHolsterTimer = 0.0f;

		//Check if the sub-task is valid.
		if(pSubTask)
		{
			//Check if the sub-task is not the right type.
			if(pSubTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
			{
				//Request termination.
				taskAssert(pSubTask->SupportsTerminationRequest());
				pSubTask->RequestTermination();
			}
			//Terminate driveby subtask if restart requested
			if((m_bRestartShootingTask || m_bTerminateDrivebyTask) && (pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN || pSubTask->GetTaskType() == CTaskTypes::TASK_MOUNT_THROW_PROJECTILE))
			{
				if (m_bRestartShootingTask)
					m_bRestartShootingTask = false;

				if (m_bTerminateDrivebyTask)
				{
					m_bIsAiming = false;
					m_bTerminateDrivebyTask = false;
				}

				//Request termination.
				taskAssert(pSubTask->SupportsTerminationRequest());
				pSubTask->RequestTermination();
			}
		}
		else
		{
			//Create the task.
			pSubTask = CreateTaskForAiming();

			//Start the task.
			SetNewTask(pSubTask);
		}
	}
	else
	{
		//Check if the sub-task is valid.
		if(pSubTask)
		{
			if(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN || pSubTask->GetTaskType() == CTaskTypes::TASK_MOUNT_THROW_PROJECTILE)
			{
				m_fHolsterTimer += fwTimer::GetTimeStep();
				bool bHolsterTimerFinished = m_fHolsterTimer >= sm_Tunables.m_HoverControlParams.m_fMaxHolsterTimer;
				if (bHolsterTimerFinished)
				{
					//Request termination.
					taskAssert(pSubTask->SupportsTerminationRequest());
					pSubTask->RequestTermination();
					m_fHolsterTimer = 0.0f;
				}
			}
		}
		else
		{
			//Create the task.
			pSubTask = CreateTaskForAmbientClips();

			//Start the task.
			SetNewTask(pSubTask);
		}
	}
}

CTask* CTaskJetpack::CreateTaskForAiming() const
{
	const CPed *pPed = GetPed();
	if (pPed->IsPlayer())
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon())
		{
			return rage_new CTaskMountThrowProjectile(CWeaponController::WCT_Player, 0, CWeaponTarget()); 
		}
		else
		{

			return rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Player);
		}
	}
	else
	{
		CAITarget target;
		// Use the entity as the target if we have been provided with one, else use the coordinates
		if (m_pTargetEntityShootAt)
		{
			// Set m_vTargetPositionShootAt as an offset if it is not zero
			if (!IsEqualAll(m_vTargetPositionShootAt, Vec3V(V_ZERO)))
			{
				target.SetEntityAndOffsetUnlimited(m_pTargetEntityShootAt, VEC3V_TO_VECTOR3(m_vTargetPositionShootAt));
			}
			else
			{
				target.SetEntity(m_pTargetEntityShootAt);
			}
		}
		else
		{
			target.SetPosition(VEC3V_TO_VECTOR3(m_vTargetPositionShootAt));
		}
		return rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Fire, m_iFiringPatternHash, &target, ((float)m_iFrequencyPercentage)/100.0f, m_fAbortRange);			
	}
}

CTask *CTaskJetpack::CreateTaskForAmbientClips() const
{
	return rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("PLAYER_IDLES"));
}

bool CTaskJetpack::CanActivateKillSwitch(CPed *pPed) const
{
	CControl *pControl = pPed->GetControlFromPlayer();
	if(sm_Tunables.m_HoverControlParams.m_bUseKillSwitch && pControl && pControl->GetMeleeAttackLight().IsPressed()) 
	{
		return true;
	}

	return false;
}

bool CTaskJetpack::CanActivateFlightMode(CPed *pPed) const
{
	if(sm_Tunables.m_FlightControlParams.m_bEnabled)
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		if(pControl && pControl->GetPedDuck().IsPressed()) 
		{
			return true;
		}
	}

	return false;
}

bool CTaskJetpack::CheckForLanding(CPed *pPed)
{
	if(sm_Tunables.m_HoverControlParams.m_bAutoLand && GetUpThrustInput() <= 0.0f) //don't land if we have upthrust.
	{
		if( CTaskJump::IsPedNearGround(pPed, sm_Tunables.m_HoverControlParams.m_fLandHeightThreshold) )
		{
			m_fNearGroundTime += fwTimer::GetTimeStep();

			if( (m_fNearGroundTime >= sm_Tunables.m_HoverControlParams.m_fTimeNearGroundToLand) || 
				(sm_Tunables.m_HoverControlParams.m_bAntiGravityThrusters && (m_fThrusterLeftInput > 0.0f)) )
			{
				return true;
			}
		}
		else
		{
			m_fNearGroundTime = 0.0f;
		}
	}

	return false;
}

float CTaskJetpack::GetUpThrust() const
{
	if(sm_Tunables.m_HoverControlParams.m_bAntiGravityThrusters)
	{
		return m_fThrusterRight;
	}
	else
	{
		return (m_fThrusterLeft + m_fThrusterRight) * 0.5f; //normalise.
	}
}


float CTaskJetpack::GetUpThrustInput() const
{
	if(sm_Tunables.m_HoverControlParams.m_bAntiGravityThrusters)
	{
		return m_fThrusterRightInput;
	}
	else
	{
		return (m_fThrusterLeftInput + m_fThrusterRightInput) * 0.5f; //normalise.
	}
}

bool CTaskJetpack::IsInMotion() const
{
	if(GetState() == State_Hover)
	{
		if(m_fThrusterLeft < 0.01f && m_fThrusterRight < 0.01f && 
			GetPed()->GetVelocity().Mag2() < rage::square(sm_Tunables.m_HoverControlParams.m_MaxIdleAnimVelocity) && 
			GetPed()->GetAngVelocity().Mag2() < rage::square(sm_Tunables.m_HoverControlParams.m_MaxIdleAnimVelocity))
		{
			return false;
		}
	}

	return true;
}

void CTaskJetpack::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	//Make sure that there is a valid camera at all times 
	settings.m_CameraHash = sm_Tunables.m_CameraSettings.m_HoverCamera.GetHash();
	settings.m_MaxOrientationDeltaToInterpolate	= PI;

	switch(GetState())
	{
	case State_Hover:
		settings.m_CameraHash = sm_Tunables.m_CameraSettings.m_HoverCamera.GetHash();
		break;
	case State_Flight:
		settings.m_CameraHash = sm_Tunables.m_CameraSettings.m_FlightCamera.GetHash();
		break;
	default:
		settings.m_CameraHash = 0;
		break;
	}
}

const CJetpack *CTaskJetpack::GetJetpack() const
{
	const CPed *pPed = GetPed();
	if(pPed)
	{
		return pPed->GetJetpack();
	}

	return NULL;
}

bool CTaskJetpack::ShouldBlockWeaponSwitching() const
{
	//Check if we are parachuting, and can aim.
	if((GetState() == State_Hover) && CanAim())
	{
		return false;
	}

	return true;
}

bool CTaskJetpack::CanAim() const
{
	//Ensure aiming is not disabled.
	if(!sm_Tunables.m_AimSettings.m_bEnabled)
	{
		return false;
	}
	
	//Ensure the weapon manager is valid.
	if(!GetPed()->GetWeaponManager())
	{
		return false;
	}

	return true;
}

bool CTaskJetpack::ScanForCrashCollision() const
{
	const CPed *pPed = GetPed();
	const CCollisionRecord* pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
	if(!pColRecord)
	{
		return false;
	}

	const CEntity* pEntity = pColRecord->m_pRegdCollisionEntity;
	if(pEntity) 
	{
		if (pEntity->GetIsTypeObject())
		{
			//Ensure the object is a parachute.
			if(static_cast<CObject*>(GetJetpack()->GetObject()) == pEntity)
			{
				return false;
			}
		}
	}

	return true;
}

float CTaskJetpack::GetFlightSpeedForMoVE() const
{
	float fSpeed = Clamp(GetPed()->GetVelocity().Mag2() / rage::square(sm_Tunables.m_FlightControlParams.m_fMaxSpeedForMoVE), 0.0f, 1.0f);
	return fSpeed;
}

float RemapTo360(float fAngle)
{
	if(fAngle < 0.0f)
	{
		fAngle += 2.0f * PI;
	}

	return fAngle;
}

void CTaskJetpack::GetAnglesForMoVE(Vector3 &vAngles) const
{
	Mat34V m = GetPed()->GetMatrix();
	MAT34V_TO_MATRIX34(m).ToEulersXYZ(vAngles);

	//! Bit hacky, but change order to get full roll.
	{
		Vector3 vTemp;
		Mat34V m = GetPed()->GetMatrix();
		MAT34V_TO_MATRIX34(m).ToEulersYXZ(vTemp);
		vAngles.y = vTemp.y;
	}

	vAngles.x = RemapTo360(vAngles.x);
	vAngles.y = RemapTo360(vAngles.y);
	vAngles.z = RemapTo360(vAngles.z);

	//! normalise to range 0.0f -> 1.0f
	vAngles.x /= (2.0f*PI);
	vAngles.y /= (2.0f*PI);
	vAngles.z /= (2.0f*PI);	
}

Vector3 CTaskJetpack::GetAngularVelocityForMoVE(const Vector3 &vAngVel) const
{
	Vector3 vAngVelLocal = vAngVel;

	//! Get ang vel in range -1.0f -> 1.0f
	if(GetState()==State_Hover)
	{
		vAngVelLocal.x = 0.0f;
		vAngVelLocal.y = 0.0f;
		vAngVelLocal.z = Clamp(vAngVel.z/sm_Tunables.m_HoverControlParams.m_fMaxAngularVelocityZ, -1.0f, 1.0f);
	}
	else if(GetState()==State_Flight)
	{
		vAngVelLocal.x = Clamp(vAngVel.x/sm_Tunables.m_FlightControlParams.m_fMaxAngularVelocityX, -1.0f, 1.0f);
		vAngVelLocal.y = Clamp(vAngVel.y/sm_Tunables.m_FlightControlParams.m_fMaxAngularVelocityY, -1.0f, 1.0f);
		vAngVelLocal.z = Clamp(vAngVel.z/sm_Tunables.m_FlightControlParams.m_fMaxAngularVelocityZ, -1.0f, 1.0f);
	}

	//! Normalise to range 0.0f -> 1.0f
	vAngVelLocal.x = (vAngVelLocal.x + 1.0f) * 0.5f;
	vAngVelLocal.y = (vAngVelLocal.y + 1.0f) * 0.5f;
	vAngVelLocal.z = (vAngVelLocal.z + 1.0f) * 0.5f;

	return vAngVelLocal;
}

bool CTaskJetpack::IsMoveTransitionAvailable(s32 UNUSED_PARAM(iNextState)) const
{
	//Note: At the point at which this function is called, we are always in the AI state of the new state.
	//		We need to check that there is a move transition from the previous state to the current.

	//Grab the previous state.
	int iPreviousState = GetPreviousState();

	//Grab the state.
	int iState = GetState();

	//Check the previous state.
	switch(iPreviousState)
	{
	case State_StreamAssets:
		{
			//Check the state.
			switch(iState)
			{
			case State_Hover:
			case State_Flight:
				{
					return true;
				}
			default:
				{
					return false;
				}
			}
		}
	case State_Hover:
		{
			//Check the state.
			switch(iState)
			{
			case State_Hover:
			case State_Flight:
				{
					return true;
				}
			default:
				{
					return false;
				}
			}
		}
	case State_Flight:
		{
			//Check the state.
			switch(iState)
			{
			case State_Hover:
				{
					return true;
				}
			default:
				{
					return false;
				}
			}
		}
	default:
		{
			return false;
		}
	}
}

void CTaskJetpack::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Quit);
}

CTaskInfo* CTaskJetpack::CreateQueriableState() const
{
	CClonedJetpackInfo::InitParams params(GetState(), (u8)m_iFlags);

	return rage_new CClonedJetpackInfo( params );
}

void CTaskJetpack::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	// Need to read the state first as we use it below to decide if we want to read something or not...
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_JETPACK );
	CClonedJetpackInfo *jetpackInfo = static_cast<CClonedJetpackInfo*>(pTaskInfo);

	m_iFlags	= jetpackInfo->GetFlags();
}

bool CTaskJetpack::OverridesNetworkAttachment(CPed* UNUSED_PARAM(pPed))
{
	//! TO DO.
	return false;
}

bool CTaskJetpack::OverridesNetworkBlender(CPed* UNUSED_PARAM(pPed))
{
	//! TO DO.
	return false;
}

bool CTaskJetpack::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	return true;
}

void CTaskJetpack::UpdateClonedSubTasks(CPed* UNUSED_PARAM(pPed), int const UNUSED_PARAM(iState))
{
	//! TO DO.
}

CTask::FSM_Return CTaskJetpack::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	//! TO DO.
	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone *CTaskJetpack::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	//! TO DO.
	CTaskJetpack *newTask = rage_new CTaskJetpack(0);
	newTask->SetState(GetState());

	return newTask;
}

CTaskFSMClone *CTaskJetpack::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	//! TO DO.
	CTaskJetpack *newTask = rage_new CTaskJetpack(0);
	newTask->SetState(GetState());
	return newTask;
}

CClonedJetpackInfo::CClonedJetpackInfo( CClonedJetpackInfo::InitParams const & _initParams )
: m_iFlags(_initParams.m_iFlags)
{
	SetStatusFromMainTaskState(_initParams.m_state);
}

CClonedJetpackInfo::CClonedJetpackInfo()
: m_iFlags(0)
{
}

CClonedJetpackInfo::~CClonedJetpackInfo()
{}

CTaskFSMClone* CClonedJetpackInfo::CreateCloneFSMTask()
{
	return rage_new CTaskJetpack( m_iFlags );
}

#if !__FINAL
const char * CTaskJetpack::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Quit);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_StreamAssets",			
		"State_Hover",		
		"State_Flight",		
		"State_Fall",
		"State_Land",			
		"State_Quit"		
	};

	return aStateNames[iState];
}
#endif	// !__FINAL

#if DEBUG_DRAW

float ConvertToDebugRange(float fValue)
{
	float fReturn = (fValue * -2.0f) - 1.0f;
	return fReturn;
}

void CTaskJetpack::Debug() const
{
	static dev_float fScale = 0.15f;
	static dev_float fHalfScale = fScale * 0.5f;
	static dev_float fEndWidth = 0.01f;

	if(ms_bDebugControls)
	{
		Vector2 vPitchDebugPos(0.25f, 0.15f);
		Vector2 vRollDebugPos(vPitchDebugPos.x - fHalfScale, vPitchDebugPos.y + fHalfScale);
		Vector2 vYawDebugPos(vRollDebugPos.x, vRollDebugPos.y + fHalfScale + fScale);

		Vector2 vLeftThrusterDebugPos(0.75f, 0.15f);
		Vector2 vRightThrusterDebugPos(vLeftThrusterDebugPos.x + fHalfScale, vLeftThrusterDebugPos.y);
		Vector2 vSpeedDebugPos(vLeftThrusterDebugPos.x + (fHalfScale * 2.0f), vLeftThrusterDebugPos.y);

		float fMeterLeftThrust = ConvertToDebugRange(m_fThrusterLeft);
		float fMeterRightThrust = ConvertToDebugRange(m_fThrusterRight);
		float fSpeed = ConvertToDebugRange(GetFlightSpeedForMoVE());

		grcDebugDraw::Meter(vPitchDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Pitch");
		grcDebugDraw::MeterValue(vPitchDebugPos, Vector2(0.0f,1.0f), fScale, m_fPitch, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vRollDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Roll");
		grcDebugDraw::MeterValue(vRollDebugPos, Vector2(1.0f,0.0f), fScale, m_fRoll, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vYawDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Yaw");
		grcDebugDraw::MeterValue(vYawDebugPos, Vector2(1.0f,0.0f), fScale, m_fYaw, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vLeftThrusterDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"L Thrust");
		Vector2 vLeftThrusterValueDebugPos = vLeftThrusterDebugPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vLeftThrusterValueDebugPos, Vector2(0.0f,1.0f), fScale, fMeterLeftThrust, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vRightThrusterDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"R Thrust");
		Vector2 vRightThrusterValueDebugPos = vRightThrusterDebugPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vRightThrusterValueDebugPos, Vector2(0.0f,1.0f), fScale, fMeterRightThrust, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vSpeedDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Speed");
		Vector2 vSpeedValueDebugPos = vSpeedDebugPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vSpeedValueDebugPos, Vector2(0.0f,1.0f), fScale, fSpeed, fEndWidth, Color32(255,0,0));
	}

	if(ms_bDebugAngles)
	{
		Vector2 vPitchAngleDebugPos(0.75f, 0.4f);
		Vector2 vRollAngleDebugPos(vPitchAngleDebugPos.x + fHalfScale, vPitchAngleDebugPos.y);
		Vector2 vYawAngleDebugPos(vPitchAngleDebugPos.x + (fHalfScale * 2.0f), vPitchAngleDebugPos.y);

		Vector2 vAngularVelocityXPos(0.75f, 0.65f);
		Vector2 vAngularVelocityYPos(vAngularVelocityXPos.x + fHalfScale, vAngularVelocityXPos.y);
		Vector2 vAngularVelocityZPos(vAngularVelocityXPos.x + (fHalfScale * 2.0f), vAngularVelocityXPos.y);

		//! Get angles.
		Vector3 vAngles;
		GetAnglesForMoVE(vAngles);
		vAngles.x = ConvertToDebugRange(vAngles.x);
		vAngles.y = ConvertToDebugRange(vAngles.y);
		vAngles.z = ConvertToDebugRange(vAngles.z);

		//! Get ang velocity.
		Vector3 vAngVel = GetAngularVelocityForMoVE(m_vAngularVelocity);
		vAngVel.x = ConvertToDebugRange(vAngVel.x);
		vAngVel.y = ConvertToDebugRange(vAngVel.y);
		vAngVel.z = ConvertToDebugRange(vAngVel.z);

		grcDebugDraw::Meter(vPitchAngleDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Pitch Angle");
		Vector2 vPitchAngleValueDebugPos = vPitchAngleDebugPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vPitchAngleValueDebugPos, Vector2(0.0f,1.0f), fScale, vAngles.x, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vRollAngleDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Roll Angle");
		Vector2 vRollAngleValueDebugPos = vRollAngleDebugPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vRollAngleValueDebugPos, Vector2(0.0f,1.0f), fScale, vAngles.y, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vYawAngleDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Yaw Angle");
		Vector2 vYawAngleValueDebugPos = vYawAngleDebugPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vYawAngleValueDebugPos, Vector2(0.0f,1.0f), fScale, vAngles.z, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vAngularVelocityXPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Ang Vel X");
		Vector2 vAngularVelocityXValueDebugPos = vAngularVelocityXPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vAngularVelocityXValueDebugPos, Vector2(0.0f,1.0f), fScale, vAngVel.x, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vAngularVelocityYPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Ang Vel Y");
		Vector2 vAngularVelocityYValueDebugPos = vAngularVelocityYPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vAngularVelocityYValueDebugPos, Vector2(0.0f,1.0f), fScale, vAngVel.y, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vAngularVelocityZPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Ang Vel Z");
		Vector2 vAngularVelocityZValueDebugPos = vAngularVelocityZPos + Vector2(0.0f,fScale);
		grcDebugDraw::MeterValue(vAngularVelocityZValueDebugPos, Vector2(0.0f,1.0f), fScale, vAngVel.z, fEndWidth, Color32(255,0,0));

		Vector2 v2DDebugPos(0.1f, 0.55f);

		static dev_float s_fTextSize = 0.025f;

		char buff[64];
		sprintf(buff, "Ang Vel X: %.2f", GetPed()->GetAngVelocity().x);
		grcDebugDraw::Text(v2DDebugPos, Color32(255,0,0), buff);
		v2DDebugPos.y += s_fTextSize;

		sprintf(buff, "Ang Vel Y: %.2f", GetPed()->GetAngVelocity().y);
		grcDebugDraw::Text(v2DDebugPos, Color32(255,0,0), buff);
		v2DDebugPos.y += s_fTextSize;

		sprintf(buff, "Ang Vel Z: %.2f", GetPed()->GetAngVelocity().z);
		grcDebugDraw::Text(v2DDebugPos, Color32(255,0,0), buff);
		v2DDebugPos.y += (s_fTextSize * 2.0f);

		Vector3 vRawAngles;
		Mat34V m = GetPed()->GetMatrix();
		MAT34V_TO_MATRIX34(m).ToEulersXYZ(vRawAngles);

		sprintf(buff, "Pitch: %.2f", vRawAngles.x * RtoD);
		grcDebugDraw::Text(v2DDebugPos, Color32(255,0,0), buff);
		v2DDebugPos.y += s_fTextSize;

		sprintf(buff, "Roll: %.2f", vRawAngles.y * RtoD);
		grcDebugDraw::Text(v2DDebugPos, Color32(255,0,0), buff);
		v2DDebugPos.y += s_fTextSize;

		sprintf(buff, "Yaw: %.2f", vRawAngles.z * RtoD);
		grcDebugDraw::Text(v2DDebugPos, Color32(255,0,0), buff);
		v2DDebugPos.y += (s_fTextSize * 2.0f);
	}

	if (ms_bDebugAI)
	{
	//	Vec3V vGoToPos = CalculateTargetGoToPosition();
		Vec3V vShootAtPos = CalculateTargetShootAtPosition();
		//grcDebugDraw::Sphere(vGoToPos, 0.5f, Color32(0,0,255));
		grcDebugDraw::Sphere(vShootAtPos, 0.5f, Color32(255,0,0));
		grcDebugDraw::Sphere(vShootAtPos, ms_fAbortRange, Color32(0,255,0), false);
		grcDebugDraw::Line(ms_vTargetLocationTestStart, ms_vTargetLocationTestEnd, Color32(255,0,0));

		grcDebugDraw::Sphere(ms_vHeightmapSamplePosition, 2.0f, Color_red, true, -1);
	}
}

#endif

#if __BANK

void CTaskJetpack::GiveJetpackTestCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if(pFocusPed)
	{
		CPedInventory* pPedInventory = pFocusPed->GetInventory();
		if(pPedInventory)
		{
			pPedInventory->AddWeapon(GADGETTYPE_JETPACK);
		}
	}
}

void CTaskJetpack::RemoveJetpackTestCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if(pFocusPed)
	{
		CPedInventory* pPedInventory = pFocusPed->GetInventory();
		if(pPedInventory)
		{
			pPedInventory->RemoveWeapon(GADGETTYPE_JETPACK);
		}
	}
}

void CTaskJetpack::ToggleEquipJetpack()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if(pFocusPed)
	{
		bool bOld = pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_EquipJetpack);
		pFocusPed->SetPedConfigFlag(CPED_CONFIG_FLAG_EquipJetpack, !bOld);
	}
}

void CTaskJetpack::AIShootAtTest()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if(pFocusPed)
	{
		//Warp into the air for tests
		static dev_bool bWarpIntoAir = false;
		if (pFocusPed->IsOnGround() && bWarpIntoAir)
		{
			Vector3 vWarpPos = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());
			vWarpPos.z += 10.0f;
			pFocusPed->Teleport(vWarpPos, pFocusPed->GetCurrentHeading());
		}
		CTaskJetpack *pTaskJetpack = NULL;
		if (pFocusPed->GetPedIntelligence() && pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK))
		{
			pTaskJetpack = static_cast<CTaskJetpack *>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK));
		}
		else
		{
			pFocusPed->GetPedIntelligence()->ClearTasks();
			pTaskJetpack = rage_new CTaskJetpack();
			pFocusPed->GetPedIntelligence()->AddTaskDefault(pTaskJetpack);
		}

		if (pTaskJetpack)
		{
			const u32 uFiringPattern =  ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0x0d31265f2);	
			int iFreqPercentage = 100;

			if (ms_bShootAtCoords)
			{
				pTaskJetpack->SetShootAtParametersForAI(NULL, ms_vTargetPositionShoot, ms_fAbortRange, iFreqPercentage, uFiringPattern);
			}
			else
			{
				CPed *pTargetPed = CGameWorld::FindLocalPlayer();
				if (pTargetPed)
				{
					pTaskJetpack->SetShootAtParametersForAI(pTargetPed, ms_vTargetPositionShoot, ms_fAbortRange, iFreqPercentage, uFiringPattern);
			
				}
			}
		}
	}
}

void CTaskJetpack::AIGoToTest()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if(pFocusPed)
	{
		CTaskJetpack *pTaskJetpack = NULL;
		if (pFocusPed->GetPedIntelligence() && pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK))
		{
			pTaskJetpack = static_cast<CTaskJetpack *>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK));
		}
		else
		{
			pFocusPed->GetPedIntelligence()->ClearTasks();
			pTaskJetpack = rage_new CTaskJetpack();
			pFocusPed->GetPedIntelligence()->AddTaskDefault(pTaskJetpack);
		}

		if (pTaskJetpack)
		{
			if (ms_bGoToCoords)
			{
				pTaskJetpack->SetGoToParametersForAI(NULL, ms_vTargetPositionGoTo, ms_fMinHeightAboveSurface, ms_bDisableThrustAtDestination, ms_bUseRandomTimerWhenClose, ms_fProbeHeight);
			}
			else
			{
				CPed *pTargetPed = CGameWorld::FindLocalPlayer();
				if (pTargetPed)
				{
					pTaskJetpack->SetGoToParametersForAI(pTargetPed, ms_vTargetPositionGoTo, ms_fMinHeightAboveSurface, ms_bDisableThrustAtDestination, ms_bUseRandomTimerWhenClose, ms_fProbeHeight);
				}
			}
		}
	}
}

void CTaskJetpack::SetDebugGoToFromCamera()
{
	ms_vTargetPositionGoTo = camInterface::GetPos();
}

void CTaskJetpack::SetGoToTestPoint1()
{
	ms_vTargetPositionGoTo = ms_vJetpackTestPoint1;
}

void CTaskJetpack::SetGoToTestPoint2()
{
	ms_vTargetPositionGoTo = ms_vJetpackTestPoint2;
}

void CTaskJetpack::SetGoToTestPoint3()
{
	ms_vTargetPositionGoTo = ms_vJetpackTestPoint3;
}

void CTaskJetpack::DisableDrivebyTest()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if(pFocusPed)
	{
		CTaskJetpack *pTaskJetpack = NULL;
		if (pFocusPed->GetPedIntelligence() && pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK))
		{
			pTaskJetpack = static_cast<CTaskJetpack *>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK));
		}

		if (pTaskJetpack)
		{
			pTaskJetpack->RequestDrivebyTermination(true);
		}
	}
}

void CTaskJetpack::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if(pBank)
	{
		pBank->PushGroup("Jetpack");
		pBank->AddButton("Give focus ped jetpack",GiveJetpackTestCB);
		pBank->AddButton("Remove focus ped jetpack",RemoveJetpackTestCB);
		pBank->AddButton("Toggle Equip jetpack",ToggleEquipJetpack);

		pBank->AddSlider("GoTo Target Coord X", &ms_vTargetPositionGoTo.x, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.1f);
		pBank->AddSlider("GoTo Target Coord Y", &ms_vTargetPositionGoTo.y, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX, 0.1f);
		pBank->AddSlider("GoTo Target Coord Z", &ms_vTargetPositionGoTo.z, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 0.1f);

		pBank->AddSlider("ShootAt Target Coord X", &ms_vTargetPositionShoot.x, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.1f);
		pBank->AddSlider("ShootAt Target Coord Y", &ms_vTargetPositionShoot.y, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX, 0.1f);
		pBank->AddSlider("ShootAt Target Coord Z", &ms_vTargetPositionShoot.z, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 0.1f);

		pBank->AddSlider("ShootAt: AbortRange", &ms_fAbortRange, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("GoTo: Probe height", &ms_fProbeHeight, 0.0f, 200.0f, 0.01f);
		pBank->AddSlider("GoTo: Min height above surface", &ms_fMinHeightAboveSurface, 0.0f, 100.0f, 0.01f);
	
		pBank->AddToggle("GoTo: Disable thrust at destination", &ms_bDisableThrustAtDestination);
		pBank->AddToggle("Debug AI", &ms_bDebugAI);
		pBank->AddToggle("Go To Coords (not entity)", &ms_bGoToCoords);
		pBank->AddToggle("Shoot At Coords (not entity)", &ms_bShootAtCoords);
		pBank->AddToggle("Go To: Use random timer for target pos when close", &ms_bUseRandomTimerWhenClose);

		pBank->AddButton("Shoot At Coord", AIShootAtTest);
		pBank->AddButton("Go To Coord", AIGoToTest);
		pBank->AddButton("Set Go To Coords from Camera", SetDebugGoToFromCamera);

		pBank->AddButton("Set Go To: TestPoint1", SetGoToTestPoint1);
		pBank->AddButton("Set Go To: TestPoint2", SetGoToTestPoint2);
		pBank->AddButton("Set Go To: TestPoint3", SetGoToTestPoint3);
		pBank->AddButton("Stop Driveby", DisableDrivebyTest);

		pBank->PushGroup("Controls");
		pBank->AddToggle("Debug controls",&ms_bDebugControls);
		pBank->AddToggle("Debug angles",&ms_bDebugAngles);
		pBank->PopGroup();

		pBank->PushGroup("Flight Mode handling");
			ms_FlightModeHelper.AddWidgetsToBank(pBank);
		pBank->PopGroup();

		pBank->PopGroup();
	}
}

#endif

#endif // ENABLE_JETPACK


atFixedArray<RegdPed, 20> CFlyingPedCollector::m_flyingPeds;
void CFlyingPedCollector::RemovePed(const RegdPed& ped)
{
	int index = m_flyingPeds.Find(ped);
	if(index != -1)
	{
		m_flyingPeds.DeleteFast(index);
	}
}

void CFlyingPedCollector::AddPed(const RegdPed& ped)
{
	if(m_flyingPeds.GetAvailable() && m_flyingPeds.Find(ped) == -1)
	{
		m_flyingPeds.Push(ped);
	}
}