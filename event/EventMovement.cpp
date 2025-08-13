//
#include "fwmaths/geomutil.h"

// Game headers
#include "animation/AnimDefines.h"
#include "Event/EventMovement.h"
#include "Event/EventResponse.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "streaming/SituationalClipSetStreamer.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskFollowWaypointRecording.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskMoveWander.h"
#include "task/Response/TaskReactAndFlee.h"
#include "task/Response/TaskReactToImminentExplosion.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskRideTrain.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "vehicles/train.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicleDamage.h"
#include "modelinfo/PedModelInfo.h"
#include "weapons/projectiles/Projectile.h"
#include "Weapons/Weapon.h"

#include "event/EventMovement_parser.h"

AI_OPTIMISATIONS()
AI_EVENT_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventClimbLadderOnRoute
//////////////////////////////////////////////////////////////////////////

CEventClimbLadderOnRoute::CEventClimbLadderOnRoute(const Vector3& vLadderBottom, const Vector3& vLadderTop, const int iDirection, const float fLadderHeading, const float fMoveBlendRatio)
: m_vLadderBottom(vLadderBottom)
, m_vLadderTop(vLadderTop)
, m_iDirection(iDirection)
, m_fLadderHeading(fLadderHeading)
, m_fMoveBlendRatio(fMoveBlendRatio)
{
#if !__FINAL
	m_EventType = EVENT_CLIMB_LADDER_ON_ROUTE;
#endif

	Assertf(m_vLadderTop.z > m_vLadderBottom.z, "Ladder at (%.1f, %.1f, %.1f) is broken", vLadderTop.x, vLadderTop.y, vLadderTop.z);
	Assertf(m_iDirection == -1 || m_iDirection == 1, "Ladder at (%.1f, %.1f, %.1f) is broken", vLadderTop.x, vLadderTop.y, vLadderTop.z);
	Assertf(m_fLadderHeading >= -PI && m_fLadderHeading <= PI, "Ladder at (%.1f, %.1f, %.1f) is broken", vLadderTop.x, vLadderTop.y, vLadderTop.z);
}

bool CEventClimbLadderOnRoute::AffectsPedCore(CPed* pPed) const
{
	if(pPed->GetUsingRagdoll() || pPed->IsInjured() || pPed->GetIsInVehicle())
	{
		return false;
	}

	return true;
}

bool CEventClimbLadderOnRoute::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if(pPed->GetUsingRagdoll() || pPed->IsInjured() || pPed->GetIsInVehicle())
	{
		return false;
	}

	const Vector3& vTarget = m_iDirection == 1 ? m_vLadderBottom : m_vLadderTop;

	CTaskUseLadderOnRoute* pUseLadderTask = rage_new CTaskUseLadderOnRoute(m_fMoveBlendRatio, vTarget, m_fLadderHeading, NULL);

	response.SetEventResponse(CEventResponse::EventResponse, pUseLadderTask);
	return true;
}

//////////////////////////////////////////////////////////////////////////
// CEventClimbNavMeshOnRoute
//////////////////////////////////////////////////////////////////////////

CEventClimbNavMeshOnRoute::CEventClimbNavMeshOnRoute(const Vector3& vClimbPosition, const float fClimbHeading, const Vector3& vClimbTarget, const float fMoveBlendRatio, const u32 iWptFlags, CEntity* pEntityWeAreOn, const s32 iWarpTimer, const Vector3& vWarpTarget, bool bForceJump)
: m_vClimbPosition(vClimbPosition)
, m_vClimbTarget(vClimbTarget)
, m_fClimbHeading(fClimbHeading)
, m_fMoveBlendRatio(fMoveBlendRatio)
, m_iWptFlags(iWptFlags)
, m_pEntityWeAreOn(pEntityWeAreOn)
, m_iWarpTimer(iWarpTimer)
, m_vWarpTarget(vWarpTarget)
, m_bForceJump(bForceJump)
{
#if !__FINAL
	m_EventType = EVENT_CLIMB_NAVMESH_ON_ROUTE;
#endif
}

CEventClimbNavMeshOnRoute::~CEventClimbNavMeshOnRoute()
{
}

bool CEventClimbNavMeshOnRoute::AffectsPedCore(CPed* pPed) const
{
	if(pPed->GetUsingRagdoll() || pPed->IsInjured() || pPed->GetIsInVehicle())
	{
		return false;
	}

	return true;
}

bool CEventClimbNavMeshOnRoute::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if(pPed->GetUsingRagdoll() || pPed->IsInjured() || pPed->GetIsInVehicle())
	{
		return false;
	}

	static dev_bool bUseTaskDropDown = false;
	if(m_iWptFlags & WAYPOINT_FLAG_DROPDOWN)
	{
		if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_MAY_DROP))
		{
			CTaskUseDropDownOnRoute* pTaskUseDropDown = rage_new CTaskUseDropDownOnRoute(
				m_fMoveBlendRatio,
				m_vClimbPosition,
				m_fClimbHeading,
				m_vClimbTarget,
				m_pEntityWeAreOn,
				m_iWarpTimer,
				m_vWarpTarget,
				false,
				bUseTaskDropDown
				);

			response.SetEventResponse(CEventResponse::EventResponse, pTaskUseDropDown);
		}
	}
	else
	{
		if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_MAY_CLIMB))
		{
			CTaskUseClimbOnRoute* pTaskUseClimb = rage_new CTaskUseClimbOnRoute(
				m_fMoveBlendRatio,
				m_vClimbPosition,
				m_fClimbHeading,
				m_vClimbTarget,
				m_pEntityWeAreOn,
				m_iWarpTimer,
				m_vWarpTarget,
				m_bForceJump
				);

			response.SetEventResponse(CEventResponse::EventResponse, pTaskUseClimb);
		}

	}

	return response.HasEventResponse();
}

//////////////////////////////////////////////////////////////////////////
// CEventPotentialBeWalkedInto
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MOVEMENT_EVENT_TUNABLES(CEventPotentialBeWalkedInto, 0x1e8f6dd7);
CEventPotentialBeWalkedInto::Tunables CEventPotentialBeWalkedInto::sm_Tunables;

CEventPotentialBeWalkedInto::CEventPotentialBeWalkedInto(CPed* pOtherPed, const Vector3& vTarget, const float fMoveBlendRatio)
: m_pOtherPed(pOtherPed)
, m_vTarget(vTarget)
, m_fMoveBlendRatio(fMoveBlendRatio)
{
#if !__FINAL
	m_EventType = EVENT_POTENTIAL_BE_WALKED_INTO;
#endif

	m_fOtherPedSpeedSq = pOtherPed ? pOtherPed->GetVelocity().Mag2() : 0.0f;
}

CEventPotentialBeWalkedInto::~CEventPotentialBeWalkedInto()
{
}

CEvent* CEventPotentialBeWalkedInto::Clone() const
{
	CEventPotentialBeWalkedInto* pEvent = rage_new CEventPotentialBeWalkedInto((m_pOtherPed), m_vTarget, m_fMoveBlendRatio);
	pEvent->m_fOtherPedSpeedSq = m_fOtherPedSpeedSq;

	return pEvent;
}

bool CEventPotentialBeWalkedInto::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead() || 
		pPed->PopTypeIsMission() || 
		!m_pOtherPed || 
		pPed->GetIsInVehicle() ||
		pPed->GetMotionData()->GetIsRunning() ||
		m_pOtherPed->GetMotionData()->GetIsStill())
	{
		return false;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PermanentlyDisablePotentialToBeWalkedIntoResponse))
	{
		return false;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePotentialToBeWalkedIntoResponse))
	{
		return false;
	}

	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisablePotentialToBeWalkedIntoResponse))
	{
		return false;
	}

	//Ensure we have not recently evaded the other ped.
	static const u32 s_uMinTimeSinceLastEvaded = 3000;
	if((pPed->GetPedIntelligence()->GetLastEntityEvaded() == m_pOtherPed) &&
		((pPed->GetPedIntelligence()->GetLastTimeEvaded() + s_uMinTimeSinceLastEvaded) > fwTimer::GetTimeInMilliseconds()))
	{
		return false;
	}

	//Don't react for peds that are melee attacking us.
	CTaskMeleeActionResult *pMeleeResult = m_pOtherPed->GetPedIntelligence()->GetTaskMeleeActionResult();
	if(pMeleeResult && (pMeleeResult->GetTargetEntity() == pPed) && pMeleeResult->IsOffensiveMove())
	{
		return false;
	}

	//Ensure the ped is not swimming.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming))
	{
		return false;
	}

	//Ensure the ped is not in the water.
	if(pPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
	{
		return false;
	}

	// CNC: Disable evasive step response if other ped is a player doing a melee shoulder barge towards this ped.
	if (CEventPedCollisionWithPlayer::CNC_ShouldPreventEvasiveStepDueToPlayerShoulderBarging(pPed, m_pOtherPed))
	{	
		return false;
	}

	return true;
}

bool CEventPotentialBeWalkedInto::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if(!m_pOtherPed)
	{
		return false;
	}

	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableEvasiveStep))
	{
		return false;
	}

	bool bIsOtherPedInFirstPersonMode = false;
#if FPS_MODE_SUPPORTED
	bIsOtherPedInFirstPersonMode = m_pOtherPed->IsFirstPersonShooterModeEnabledForPlayer();
#endif

	bool bIsOtherPedRunning = !bIsOtherPedInFirstPersonMode ?
		(m_pOtherPed->GetMotionData()->GetIsRunning() || m_pOtherPed->GetMotionData()->GetIsSprinting()) :
		m_pOtherPed->GetMotionData()->GetIsSprinting();

	if(!bIsOtherPedRunning &&
		!pPed->GetMotionData()->GetIsStill())
	{
		return false;
	}

	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableEvasiveDives ) ||
		pPed->GetIsInVehicle())
	{
		return false;
	}

	// If this ped is armed, and the other isn't, don't react
	const CWeapon* pPedsWeapon      = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	const CWeapon* pOtherPedsWeapon = m_pOtherPed->GetWeaponManager() ? m_pOtherPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if((pPedsWeapon && pPedsWeapon->GetWeaponInfo()->GetIsGun()) && (pOtherPedsWeapon && !pOtherPedsWeapon->GetWeaponInfo()->GetIsGun()))
	{
		return false;
	}

	// Work out if the peds can see each other
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
	const Vector3 vOtherPedPosition = VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition());
	const Vector3 vDiff = vOtherPedPosition - vPedPosition;
	Vector3 vDirection;
	vDirection.Normalize(vDiff);

	// don't dodge peds that are behind us
	if ( vDirection.Dot(vPedForward) < cos(DtoR * sm_Tunables.m_AngleThresholdDegrees) )
	{
		if(!bIsOtherPedRunning ||
			(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) >= sm_Tunables.m_ChancesToReactToRunningPedsBehindUs))
		{
			return false;
		}
	}
	
	pPed->GetPedIntelligence()->Dodged(*m_pOtherPed, m_fOtherPedSpeedSq);

	fwFlags8 uConfigFlags = 0;
	if(!bIsOtherPedRunning || (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sm_Tunables.m_ChanceToUseCasualAvoidAgainstRunningPed))
	{
		uConfigFlags.SetFlag(CTaskComplexEvasiveStep::CF_Casual);
	}

	//Create the task.
	CTask* pTask = rage_new CTaskComplexEvasiveStep(m_pOtherPed, uConfigFlags);

	response.SetEventResponse(CEventResponse::EventResponse, pTask);
	return true;
}

CEntity* CEventPotentialBeWalkedInto::GetSourceEntity() const
{
	return m_pOtherPed;
}

//////////////////////////////////////////////////////////////////////////
// CEventPotentialGetRunOver
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MOVEMENT_EVENT_TUNABLES(CEventPotentialGetRunOver, 0xba56f15a);

CEventPotentialGetRunOver::Tunables CEventPotentialGetRunOver::sm_Tunables;

CEventPotentialGetRunOver::CEventPotentialGetRunOver(CVehicle* pThreatVehicle)
: m_pThreatVehicle(pThreatVehicle)
{
#if !__FINAL
	m_EventType = EVENT_POTENTIAL_GET_RUN_OVER;
#endif

	m_fThreatVehicleSpeedSq = pThreatVehicle ? pThreatVehicle->GetVelocity().Mag2() : 0.0f;
}

CEventPotentialGetRunOver::~CEventPotentialGetRunOver()
{
}

CEventEditableResponse* CEventPotentialGetRunOver::CloneEditable() const
{
	CEventPotentialGetRunOver* pEvent = rage_new CEventPotentialGetRunOver(m_pThreatVehicle);
	pEvent->m_fThreatVehicleSpeedSq = m_fThreatVehicleSpeedSq;

	return pEvent;
}

bool CEventPotentialGetRunOver::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead() || 
		pPed->GetIsAttached() || 
		pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || 
		!m_pThreatVehicle ||
		m_pThreatVehicle->InheritsFromBoat())
	{
		return false;
	}

	//Ensure the ped is not swimming.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming))
	{
		return false;
	}

	//Ensure the ped is not in the water.
	if(pPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
	{
		return false;
	}

	//Ensure the ped is not ragdolling.
	if(pPed->GetUsingRagdoll())
	{
		return false;
	}

	return true;
}

bool CEventPotentialGetRunOver::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// If the vehicle has been deleted then don't respond to the event
	if(!m_pThreatVehicle)
	{
		return false;
	}

	// If the ped is in a car then don't respond to the event
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// RSNWE-JW we are already jumping aside, no need to request dupes
	if(pPed->GetPedIntelligence() &&
	   pPed->GetPedIntelligence()->GetQueriableInterface() &&
	   pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP))
	{
		return false;
	}

	// RSNWE-JW moved code below

	if(pPed->IsPlayer())
	{
		//////////////////////////////////////////////////////////////////////////
		// @RSNWE-JW
		/* Don't do all this work unless it will actually be used... */

		// Compute the corners and normals of the vehicle
		Vector3 corners[4];
		Vector3 normals[4];
		float ds[4];
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		CEntityBoundAI bound(*m_pThreatVehicle, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
		bound.GetCorners(corners);
		bound.GetPlanes(normals, ds);

		// Compute points at the centre of the front and rear of the car
		Vector3 vFront(corners[CEntityBoundAI::FRONT_LEFT] + corners[CEntityBoundAI::FRONT_RIGHT]);
		vFront *= 0.5f;
		Vector3 vRear(corners[CEntityBoundAI::REAR_LEFT] + corners[CEntityBoundAI::REAR_RIGHT]);
		vRear *= 0.5f; 

		// Compute the vehicle point (front or rear) which is going to hit the ped
		Vector3 vecThreatVehicleForward(VEC3V_TO_VECTOR3(m_pThreatVehicle->GetTransform().GetB()));
		const float fForwardSpeed = DotProduct(vecThreatVehicleForward, m_pThreatVehicle->GetVelocity());
		Vector3 vPoint = (fForwardSpeed > 0.0f) ? vFront : vRear;

		// Compute the distance along the car's forward/right dirs to the ped
		Vector3 vDiff(vPedPosition - vPoint);
		const float fLongDiff = DotProduct(vDiff, vecThreatVehicleForward);

		// Test if the car is near the player and the player is in no immediate danger
		if((rage::Abs(fLongDiff) < 3.0f) && (rage::Abs(fLongDiff) > rage::Abs(fForwardSpeed)))
		{
			// The car is near the player and the player is in no immediate danger
			// If the ped isn't looking at anything then get him to look at the vehicle and maybe shake his fist
			Vector2 vCurrMbr;
			pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMbr);
			if((vCurrMbr.Mag() < 1.0f) && (!pPed->GetIkManager().IsLooking()))
			{
				pPed->GetIkManager().LookAt(0, m_pThreatVehicle, 1500, BONETAG_INVALID, NULL);
			}
		}
	}
	else 
	{
		//Grab the car speed.
		float fCarSpeedSq = m_pThreatVehicle->GetVelocity().Mag2();
		
		//Grab the driver.
		CPed* pDriver = m_pThreatVehicle->GetSeatManager()->GetDriver();
		
		//Keep track of whether we are going to dive or not.
		bool bDive = false;
		
		//Check if the car is going fast enough to dive.
		//If the car is barely moving, diving looks pretty stupid.
		float fMinSpeedToDiveSq = square(sm_Tunables.m_MinSpeedToDive);
		float fMaxSpeedToDiveSq = square(sm_Tunables.m_MaxSpeedToDive);
		if((fCarSpeedSq >= fMinSpeedToDiveSq) && (fCarSpeedSq <= fMaxSpeedToDiveSq))
		{
			//Check if the ped is capable of diving.
			if(pPed->CheckAgilityFlags(AF_CAN_DIVE) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanDiveAwayFromApproachingVehicles))
			{
				//There are a few conditions where we should always dive:
				//	1) The car is moving quickly.
				//	2) The driver is a player who is playing the horn.
				if((fCarSpeedSq >= square(sm_Tunables.m_SpeedToAlwaysDive)) || (pDriver && pDriver->IsPlayer() && m_pThreatVehicle->IsHornOn()))
				{
					bDive = true;
				}
				//Check if we should randomly dive.
				else if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sm_Tunables.m_ChancesToDive)
				{
					bDive = true;
				}
			}
			
			//Always dive when in combat... even when agility flags are not set, apparently.
			if(!bDive && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{
				bDive = true;
			}
		}

		// @RSGNWE-JW if the Ped is fleeing, then always dive
		if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE ) ||
			pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE ) ||
			pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SCENARIO_FLEE ) )
		{
			bDive = true;
		}
		
		if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableEvasiveDives ) && !pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableEvasiveStep))
		{
			//////////////////////////////////////////////////////////////////////////
			// @RSNWE-JW
			/* Don't do this work unless it will be used... 
			   The ComputeMoveDirToAvoidEntity function is called, but is then 
			   overwritten in the CTaskComplexEvasiveStep::CreateFirstSubTask 
			   function. */

			// Find the direction the ped has to move in order to avoid the car  
			// If the ped is nearest the rhs(lhs) then move further right(left)

			pPed->GetPedIntelligence()->Dodged(*m_pThreatVehicle, m_fThreatVehicleSpeedSq);

			fwFlags8 uConfigFlags = 0;
			if(bDive)
			{
				uConfigFlags.SetFlag(CTaskComplexEvasiveStep::CF_Dive);
			}
			else if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sm_Tunables.m_ChancesToBeCasual)
			{
				uConfigFlags.SetFlag(CTaskComplexEvasiveStep::CF_Casual);
			}

			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskComplexEvasiveStep(m_pThreatVehicle.Get(), uConfigFlags));

			if(pDriver)
			{
				// If we already have a one star and are driving at a cop then we should bump the star to two
				if(pPed->IsLawEnforcementPed() && pDriver->IsLocalPlayer())
				{
					CWanted* pPlayerWanted = pDriver->GetPlayerWanted();
					if(pPlayerWanted && pPlayerWanted->GetWantedLevel() == WANTED_LEVEL1)
					{
						pPlayerWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pDriver->GetTransform().GetPosition()), WANTED_LEVEL2, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_LAW_RESPONSE_EVENT);
					}
				}
			}
		}
	}

	return response.HasEventResponse();
}

CEntity* CEventPotentialGetRunOver::GetSourceEntity() const
{
	CVehicle* pSourceVehicle = m_pThreatVehicle.Get();

	if (pSourceVehicle)
	{
		CPed* pDriver = pSourceVehicle->GetSeatManager()->GetDriver();
		if (pDriver)
		{
			return pDriver;
		}
	}
	return pSourceVehicle;
}

bool CEventPotentialGetRunOver::GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const
{
	fMin = sm_Tunables.m_MinDelay;
	fMax = sm_Tunables.m_MaxDelay;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// CEventPotentialBlast
//////////////////////////////////////////////////////////////////////////

CEventPotentialBlast::Tunables CEventPotentialBlast::sm_Tunables;
IMPLEMENT_MOVEMENT_EVENT_TUNABLES(CEventPotentialBlast, 0xcf658faa);

CEventPotentialBlast::CEventPotentialBlast(const CAITarget& rTarget, float fRadius, u32 uTimeOfExplosion)
: m_Target(rTarget)
, m_fRadius(fRadius)
, m_uTimeOfExplosion(uTimeOfExplosion)
, m_bIsSmoke(false)
{
#if !__FINAL
	m_EventType = EVENT_POTENTIAL_BLAST;
#endif
}

CEvent* CEventPotentialBlast::Clone() const
{
	CEventPotentialBlast* pEvent = rage_new CEventPotentialBlast(m_Target, m_fRadius, m_uTimeOfExplosion);
	pEvent->SetIsSmoke(GetIsSmoke());

	return pEvent;
}

bool CEventPotentialBlast::AffectsPedCore(CPed* pPed) const
{
	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return false;
	}

	//Ensure the ped is not a fireman.
	if(pPed->GetPedType() == PEDTYPE_FIRE)
	{
		return false;
	}

	//Ensure reactions are not disabled.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePotentialBlastReactions))
	{
		return false;
	}

	//Check if responses are disabled by task flags.
	if(pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisablePotentialBlastResponse))
	{
		return false;
	}

	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return false;
	}

	//Keep track of the projectile owner.
	const CEntity* pProjectileOwner = NULL;

	//Check if the target is valid.
	if(m_Target.GetEntity())
	{
		//Ensure the target is not the ped.
		if(pPed == m_Target.GetEntity())
		{
			return false;
		}

		//Ensure the target is not a projectile that the ped owns.
		if(m_Target.GetEntity()->GetIsTypeObject())
		{
			//Check if the projectile is valid.
			const CProjectile* pProjectile = static_cast<const CObject *>(m_Target.GetEntity())->GetAsProjectile();
			if(pProjectile)
			{
				//Set the projectile owner.
				pProjectileOwner = pProjectile->GetOwner();
				if(pPed == pProjectileOwner)
				{
					return false;
				}
			}
		}
	}
	
	//Check if the ped is in combat.
	CTaskCombat* pTaskCombat = static_cast<CTaskCombat *>(
		pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pTaskCombat)
	{
		pTaskCombat->OnPotentialBlast(*this);
		return false;
	}

	//Ensure the ped is not already running threat response.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return false;
	}

	//Grab the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));

	//Ensure we are within the radius to react.
	ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), vTargetPosition);
	static float s_fExtraDistanceToReact = 10.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(m_fRadius + s_fExtraDistanceToReact));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	//Check if the target position is not in our FOV.
	const CPedPerception& rPerception = pPed->GetPedIntelligence()->GetPedPerception();
	if(!rPerception.ComputeFOVVisibility(VEC3V_TO_VECTOR3(vTargetPosition)))
	{
		//Check if the projectile owner is not in our FOV.
		if(!pProjectileOwner || !rPerception.ComputeFOVVisibility(VEC3V_TO_VECTOR3(pProjectileOwner->GetTransform().GetPosition())))
		{
			return false;
		}
	}

	//Ensure the ped is not random (they use the shocking version).
	if(pPed->PopTypeIsRandom())
	{
		return false;
	}
	
	//Ensure the explosion is imminent.
	if(fwTimer::GetTimeInMilliseconds() + 1500 < m_uTimeOfExplosion)
	{
		return false;
	}
	
	//Ensure we can react.
	if(!CTaskReactToImminentExplosion::IsValid(*pPed, m_Target, m_fRadius))
	{
		return false;
	}

	return true;
}

bool CEventPotentialBlast::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{
	//Create the task.
	CTask* pTaskImminentReact = rage_new CTaskReactToImminentExplosion(m_Target, m_fRadius, m_uTimeOfExplosion);

	//Set the response.
	response.SetEventResponse(CEventResponse::EventResponse, pTaskImminentReact);

	return response.HasEventResponse();
}

//////////////////////////////////////////////////////////////////////////
// CEventPotentialWalkIntoFire
//////////////////////////////////////////////////////////////////////////

CEventPotentialWalkIntoFire::CEventPotentialWalkIntoFire(const Vector3& vFirePos, const float fFireRadius, const float fMoveBlendRatio)
: m_vFirePos(vFirePos)
, m_fFireRadius(fFireRadius)
, m_fMoveBlendRatio(fMoveBlendRatio)
{
#if !__FINAL
	m_EventType = EVENT_POTENTIAL_WALK_INTO_FIRE;
#endif
}

bool CEventPotentialWalkIntoFire::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	if(m_fMoveBlendRatio == MOVEBLENDRATIO_STILL)
	{
		return false;
	}

	CTask* pTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(pTask == NULL || !pTask->IsTaskMoving())	
	{
		return false;
	}

	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if( (m_vFirePos - vPedPos).Mag2() < CNearbyFireScanner::ms_fPotentialWalkIntoFireRange*CNearbyFireScanner::ms_fPotentialWalkIntoFireRange)
	{
		if(IsClose(m_vFirePos.z, vPedPos.z, 2.0f))
		{
			spdSphere sphere;
			sphere.Set(RCC_VEC3V(m_vFirePos), ScalarV( m_fFireRadius + pPed->GetCapsuleInfo()->GetHalfWidth() ) );

			Vector3 vTarget = pTask->GetMoveInterface()->GetTarget();
			Vector3 v1,v2;

			vTarget.z = m_vFirePos.z;
			vPedPos.z = m_vFirePos.z;

			if(!fwGeom::IntersectSphereEdge(sphere, vPedPos, vTarget, v1, v2))
			{
				return false;
			}
		}
	}

	return true;
}

bool CEventPotentialWalkIntoFire::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CTask* pTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(pTask == NULL || !pTask->IsTaskMoving())
	{
		return false;
	}

	switch(GetResponseTaskType())
	{
		case CTaskTypes::TASK_WALK_ROUND_FIRE:
		{
			CTaskMoveInterface* pTaskMoveInterface = pTask->GetMoveInterface();
			response.SetEventResponse(
				CEventResponse::EventResponse,
				rage_new CTaskWalkRoundFire(
					m_fMoveBlendRatio,
					m_vFirePos,
					m_fFireRadius + pPed->GetCapsuleInfo()->GetHalfWidth(),
					pTaskMoveInterface->GetTarget()
				)
			);
			return true;
		}
		default:
			return CEventEditableResponse::CreateResponseTask(pPed, response);
	}
}

//////////////////////////////////////////////////////////////////////////
// CEventPotentialWalkIntoObject
//////////////////////////////////////////////////////////////////////////

CEventPotentialWalkIntoObject::CEventPotentialWalkIntoObject(CObject* pObject, const float fMoveBlendRatio)
: m_pObject(pObject)
, m_fMoveBlendRatio(fMoveBlendRatio)
{
#if !__FINAL
	m_EventType = EVENT_POTENTIAL_WALK_INTO_OBJECT;
#endif
}

CEventPotentialWalkIntoObject::~CEventPotentialWalkIntoObject()
{
}

bool CEventPotentialWalkIntoObject::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsPlayer() || 
		pPed->IsDead() || 
		pPed->GetIsInVehicle() ||
		!m_pObject || 
		pPed->GetIsAttached() || 
		m_pObject->GetAttachParent() == pPed || 
		m_fMoveBlendRatio == MOVEBLENDRATIO_STILL)
	{
		return false;
	}

	if(m_pObject == pPed->GetGroundPhysical())
		return false;

	// Ignore very thin objects
	const Vector3& vMin = m_pObject->GetBaseModelInfo()->GetBoundingBoxMin();
	const Vector3& vMax = m_pObject->GetBaseModelInfo()->GetBoundingBoxMax();
	if((vMax.x - vMin.x < 0.01f) || (vMax.y - vMin.y < 0.01f) || (vMax.z - vMin.z < 0.01f))
	{
		return false;
	}

	// Waypoint following tasks may wish to prevent this event from occurring at all
	CTaskFollowWaypointRecording * pWaypointTask = (CTaskFollowWaypointRecording*)pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
	if(pWaypointTask && !pWaypointTask->GetRespondsToCollisionEvents())
		return false;

	const CTask* pTask = pPed->GetPedIntelligence()->GetTaskActive();
	if(pTask)
	{
		switch(pTask->GetTaskType())
		{
		case CTaskTypes::TASK_WALK_ROUND_ENTITY:
			{
				const CTaskWalkRoundEntity* pTaskWalkRound = static_cast<const CTaskWalkRoundEntity*>(pTask);
				if(pTaskWalkRound->GetEntity() == static_cast<CEntity*>(m_pObject))
				{
					return false;
				}
			}
			break;

		default:
			break;
		}
	}

	return true;
}

bool CEventPotentialWalkIntoObject::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// If the object has been deleted then don't respond to the event
	if(!m_pObject)
	{
		return false;
	}

	// Don't walk around things attached to us
	if(m_pObject->GetIsAttached() && m_pObject->GetAttachParent() == pPed)
	{
		return false;
	}

	// If the ped is in a car then don't respond to the event
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	if(m_fMoveBlendRatio < MBR_WALK_BOUNDARY)
	{
		return false;
	}

	CTask* pSimplestMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(CPedIntelligence::ms_bPedsRespondToObjectCollisionEvents && pSimplestMoveTask && pSimplestMoveTask->IsTaskMoving())
	{
		CTaskMoveInterface* pTaskMove = pSimplestMoveTask->GetMoveInterface();
		if(pTaskMove->HasTarget() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DontWalkRoundObjects))
		{
			response.SetEventResponse(CEventResponse::MovementResponse, rage_new CTaskWalkRoundEntity(m_fMoveBlendRatio, pTaskMove->GetTarget(), static_cast<CEntity*>(m_pObject)));
			return true;
		}
	}

	return false;
}

CEntity* CEventPotentialWalkIntoObject::GetSourceEntity() const
{
	return m_pObject;
}
//////////////////////////////////////////////////////////////////////////
// CEventPotentialWalkIntoVehicle
//////////////////////////////////////////////////////////////////////////

CEventPotentialWalkIntoVehicle::CEventPotentialWalkIntoVehicle(CVehicle* pThreatVehicle, const float fMoveBlendRatio, const Vector3 & vTarget)
: m_pThreatVehicle(pThreatVehicle)
, m_fMoveBlendRatio(fMoveBlendRatio)
, m_vTarget(vTarget)
{
#if !__FINAL
	m_EventType = EVENT_POTENTIAL_WALK_INTO_VEHICLE;
#endif
}

CEventPotentialWalkIntoVehicle::~CEventPotentialWalkIntoVehicle()
{
}

bool CEventPotentialWalkIntoVehicle::AffectsPedCore(CPed* pPed) const
{
	CTask * pWander = pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER);
	if(!pWander)
		return false;

	// If the ped is in a car then don't respond to the event
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	if(pPed->IsDead() || 
		pPed->GetIsInVehicle() || 
		pPed->GetIsAttached() ||
		m_fMoveBlendRatio == MOVEBLENDRATIO_STILL || 
		!m_pThreatVehicle)
	{
		return false;
	}

	if( m_pThreatVehicle == pPed->GetGroundPhysical() )
		return false;



/*
	// Player only accepts event if he is getting into a car,
	// or if they have a GoTo task applied to them
	CTaskEnterVehicle* pTaskNewGetIn = static_cast<CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));

	CTask* pTaskSimplestMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();

	if(pTaskSimplestMove && pPed->IsPlayer() && !pTaskNewGetIn && !pTaskSimplestMove->IsTaskMoving())
	{
		return false;
	}



	if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects))
		return false;

	// Waypoint following tasks may wish to prevent this event from occurring at all
	CTaskFollowWaypointRecording * pWaypointTask = (CTaskFollowWaypointRecording*)pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
	if(pWaypointTask && !pWaypointTask->GetRespondsToCollisionEvents())
		return false;

	CTaskWalkRoundCarWhileWandering * pWalkRndVehicle = static_cast<CTaskWalkRoundCarWhileWandering*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER_AROUND_VEHICLE));
	if(pWalkRndVehicle && pWalkRndVehicle->GetVehicle()==m_pThreatVehicle)
		return false;

	// If we're following a navmesh route and are avoiding objects (and are not crossing the road), then don't accept
	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, PED_TASK_MOVEMENT_GENERAL));
	if(pNavMeshTask && pNavMeshTask->GetAvoidObjects())
	{
		if(!pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS, PED_TASK_MOVEMENT_GENERAL))
		{
			return false;
		}
	}

	const bool bUsingNavMeshButNotAvoidingVehicles = pNavMeshTask && !pNavMeshTask->GetAvoidObjects();

	CTaskManager* pTaskManager = pPed->GetPedIntelligence()->GetTaskManager();
	Assert(pTaskManager);

	// Don't accept if we're already walking around this entity
	CTaskWalkRoundEntity* pWlkRndEntity = static_cast<CTaskWalkRoundEntity*>(pTaskManager->FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, CTaskTypes::TASK_WALK_ROUND_ENTITY, PED_TASK_MOVEMENT_EVENT_RESPONSE));
	if(pWlkRndEntity && pWlkRndEntity->GetEntity() == m_pThreatVehicle)
	{
		return false;
	}

	if(pTaskNewGetIn)
	{
		// If we're entering this car, then don't accept this event (except if the car is
		// about to run us over?)
		if(pTaskNewGetIn->GetVehicle() == m_pThreatVehicle && !bUsingNavMeshButNotAvoidingVehicles)
		{
			return false;
		}
	}		
	
	//Check if the threat vehicle is a train.
	if(m_pThreatVehicle->InheritsFromTrain())
	{
		//Check if we are trying to ride a train.
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_RIDE_TRAIN, true))
		{
			//Check if we are trying to ride this train.
			const CTaskRideTrain* pRideTrainTask = static_cast<CTaskRideTrain *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RIDE_TRAIN));
			if(pRideTrainTask && (m_pThreatVehicle == pRideTrainTask->GetTrain()))
			{
				return false;
			}
		}
	}
*/
	// Don't generate event if target-pos is on same side of car as we are
	CTask* pTaskSimplestMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(((CTaskMove*)pTaskSimplestMove)->HasTarget())
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		CEntityBoundAI bound(*m_pThreatVehicle, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth() );
		Vector3 vClosestSurfacePoint;
		bound.ComputeClosestSurfacePoint(vPedPosition, vClosestSurfacePoint);
		const int iCurrentSide = bound.ComputeHitSideByPosition(vClosestSurfacePoint);
		const int iTargetSide = bound.ComputeHitSideByPosition(m_vTarget);
		if(iCurrentSide == iTargetSide)
		{
			return false;
		}
	}
/*
	// If we're already entering into this car then don't accept the event
	CVehicle* pCarEntering = pPed->GetPedIntelligence()->IsInACarOrEnteringOne();
	if(pCarEntering && pCarEntering == m_pThreatVehicle && !bUsingNavMeshButNotAvoidingVehicles)
	{
		return false;
	}
*/
	return true;
}

bool CEventPotentialWalkIntoVehicle::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	// If the vehicle has been deleted then don't respond to the event
	if(!m_pThreatVehicle)
	{
		return false;
	}

	// If the ped is in a car then don't respond to the event
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return false;
	}

	CTask* pTaskActiveSimplest = pPed->GetPedIntelligence()->GetTaskActiveSimplest();
	CTask* pSimplestMoveTask   = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();

	if(pTaskActiveSimplest && pSimplestMoveTask && pSimplestMoveTask->IsTaskMoving())
	{
		// Don't bother trying to walk around this if we're standing still
		if(m_fMoveBlendRatio == MOVEBLENDRATIO_STILL)
		{
			return false;
		}

		// If the ped is wandering, then do a special walk-round-car response which navigates past the car(s).  This is because
		// the wander path finding ignores cars on the pavement so peds have a better chance of passing them - otherwise the
		// wandering ped would likely turn around 180, since he is not allowed to leave the pavement during wander.
		CTask * pWander = pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER);
		if(pWander)
		{ 
			response.SetEventResponse(CEventResponse::MovementResponse, rage_new CTaskWalkRoundCarWhileWandering(m_fMoveBlendRatio, m_vTarget, m_pThreatVehicle));
		}
		else
		{
			return false;
		}

		/*
		else if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
		{
			CTaskWalkRoundEntity* pTaskWalkRound = rage_new CTaskWalkRoundEntity(m_fMoveBlendRatio, m_vTarget, m_pThreatVehicle);

			// Its too expensive to use capsule tests all the time for walking round vehicles.  So instead we can switch them
			// on occasionally, which should help rescue peds who are trapped between vehicles for several seconds.
			static u32 iLosCheckCounter = 0;
			iLosCheckCounter++;

			// Only do line tests every 16 collisions
			if(iLosCheckCounter >= 16 && fwRandom::GetRandomTrueFalse())
			{
				pTaskWalkRound->SetLineTestAgainstBuildings(true);
				pTaskWalkRound->SetLineTestAgainstVehicles(true);
				iLosCheckCounter = 0;
			}

			response.SetEventResponse(CEventResponse::MovementResponse, pTaskWalkRound);
		}
		*/
	}

	return response.HasEventResponse();
}

CEntity* CEventPotentialWalkIntoVehicle::GetSourceEntity() const
{
	return m_pThreatVehicle;
}

