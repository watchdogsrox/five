// C++ headers
#include <limits> 

// rage headers
#include "Profile/timebars.h"
#include "phbound/BoundSphere.h"
#include "phcore/Segment.h"
#include "fragmentnm/nm_channel.h"

// Framework headers
#include "fwmaths/Angle.h"
#include "fwscene/search/SearchVolumes.h"
#include "grcore/debugdraw.h"
#include "fwmaths/geomutil.h"

// Game headers
#include "animation/FacialData.h"
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "Control/Record.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/vectormap.h"
#include "Event/EventDamage.h"
#include "Event/EventLeader.h"
#include "Event/EventMovement.h"
#include "Event/EventScript.h"
#include "Event/EventShocking.h"
#include "event/EventSound.h"
#include "Event/Events.h"
#include "Event/Decision/EventDecisionMaker.h"
#include "Event/ShockingEvents.h"
#include "Event/system/EventData.h"
#include "Event/system/EventDataManager.h"
#include "Game/Cheat.h"
#include "Game/localisation.h"
#include "Game/ModelIndices.h"
#include "game/Riots.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/NetworkInterface.h"
#include "Objects/Door.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped_channel.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedEventScanner.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedTaskRecord.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnSkis.h"
#include "Physics/Floater.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/Tunable.h"
#include "physics/WorldProbe/worldprobe.h"
//#include "Renderer/Font.h"
#include "renderer/River.h"
#include "renderer/Water.h"
#include "scene/world/GameWorld.h"
#include "Stats/StatsInterface.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/General/TaskBasic.h"
#include "Task/Default/TaskIncapacitated.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "task/Movement/TaskFall.h"
#include "task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Physics/BlendFromNmData.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/TaskNMRollUpAndRelax.h"
#include "Task/Physics/TaskNMSimple.h"
#include "Task/Physics/TaskNMSit.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/Info/AgitatedManager.h"
#include "Task/Response/Info/AgitatedTrigger.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/MotionTaskData.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Boat.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Submarine.h"
#include "vehicleAi/vehicleintelligence.h"
#include "weapons/Weapon.h"
#include "Weapons/WeaponDamage.h"
#include "stats/StatsMgr.h"
#include "peds/HealthConfig.h"

AI_OPTIMISATIONS()
AI_EVENT_OPTIMISATIONS()
PHYSICS_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

// RAGDOLL DAMAGE PARAMETERS //////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define the mass categories for different damage bands (minimum mass for each category).
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_MASS_HEAVY = 5000.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_MASS_MEDIUM = 500.0f;
// Define the base damage inflicted in each of the different velocity bands for vehicle damage.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_CRAZY = 100.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_FAST = 50.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_MEDIUM = 17.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_SLOW = 12.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_CRAWLING = 0.1f;
bank_float CCollisionEventScanner::RAGDOLL_SPEED_VEHICLE_CRAZY = 30.0f;
bank_float CCollisionEventScanner::RAGDOLL_SPEED_VEHICLE_FAST = 20.0f;
bank_float CCollisionEventScanner::RAGDOLL_SPEED_VEHICLE_MEDIUM = 10.0f;
bank_float CCollisionEventScanner::RAGDOLL_SPEED_VEHICLE_SLOW = 5.0f;
// Angle impact normal makes with horizontal to decide when to apply vehicle damage.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_VEHICLE_IMPACT_ANGLE_PARAM = 0.2f;
// Dot product value to decide when a ped is under a vehicle.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_UNDER_VEHICLE_IMPACT_ANGLE_PARAM = -0.9f;
// Minimum velocity of train before damage is applied.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_TRAIN_VEL_THRESHOLD = 4.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_TRAIN_BB_ADJUST_Y = 1.0f;
// Modifiers for the various damage inflictor types. This is related to the mass and is multiplied by the magnitude of the relative
// velocity to compute a damage value.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_TRAIN = 5.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_PLANE = 2.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_BIKE = 5.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_LIGHT_VEHICLE = 3.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_MEDIUM_VEHICLE = 7.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_HEAVY_VEHICLE = 10.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_UNDER_VEHICLE = 10.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_FALL = 7.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_FALL_LAND_FOOT = 3.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_GENERAL_COLLISION = 5.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MIN_SPEED_SLIDING_DAMAGE = 2.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_SLIDING_COLLISION = 0.05f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_PED = 3.0f;
// Minimum amount to fall before ped will take any fall damage.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_PED_FALL_HEIGHT_THRESHOLD = 3.0f;
// Minimum relative speed at which a non-zero damage event will be created for each inflictor type.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MIN_SPEED_FALL = 3.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MIN_SPEED_PED = 3.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE = 2.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MIN_SPEED_WRECKED_BIKE = 6.0f;

// Minimum mass of object which causes damage to ped.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MIN_OBJECT_MASS = 100.0f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_LIGHT = 0.5f;
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_HEAVY = 10.0f;
// Scaling factors to modify various damage types when they happen to a player ped.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_PLAYER_VEHICLE_SCALE = 0.8f;
// Scaling factors to modify various damage types when they happen to an animal.
bank_float CCollisionEventScanner::RAGDOLL_DAMAGE_MULTIPLIER_ANIMAL_SCALE = 10.0f;
// The amount of time before an impact with the same vehicle will cause more damage.
bank_u32 CCollisionEventScanner::RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT = 1500;
// Per-component weightings for those that want them (see enumerated list on pedDefines.h).
bank_float CCollisionEventScanner::RAGDOLL_COMPONENT_SCALES[RAGDOLL_NUM_COMPONENTS] =
{
	1.0f, // RAGDOLL_BUTTOCKS
	0.9f, // RAGDOLL_THIGH_LEFT
	0.7f, // RAGDOLL_SHIN_LEFT
	0.1f, // RAGDOLL_FOOT_LEFT
	0.9f, // RAGDOLL_THIGH_RIGHT
	0.7f, // RAGDOLL_SHIN_RIGHT
	0.1f, // RAGDOLL_FOOT_RIGHT
	1.0f, // RAGDOLL_SPINE0
	1.0f, // RAGDOLL_SPINE1
	1.0f, // RAGDOLL_SPINE2
	1.0f, // RAGDOLL_SPINE3
	1.0f, // RAGDOLL_CLAVICLE_LEFT
	0.9f, // RAGDOLL_UPPER_ARM_LEFT
	0.7f, // RAGDOLL_LOWER_ARM_LEFT
	0.1f, // RAGDOLL_HAND_LEFT
	1.0f, // RAGDOLL_CLAVICLE_RIGHT
	0.9f, // RAGDOLL_UPPER_ARM_RIGHT
	0.7f, // RAGDOLL_LOWER_ARM_RIGHT
	0.1f, // RAGDOLL_HAND_RIGHT
	1.0f, // RAGDOLL_NECK
	1.0f  // RAGDOLL_HEAD
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// KNOCK OFF VEHICLE PARAMETERS ///////////////////////////////////////////////////////////////////////////////////////////////////////
bank_float CCollisionEventScanner::sfBikeKO_PlayerMult = 0.5f;
bank_float CCollisionEventScanner::sfBikeKO_PlayerMultMP = 0.5f;
bank_float CCollisionEventScanner::sfBikeKO_VehicleMultPlayer = 2.0f;
bank_float CCollisionEventScanner::sfBikeKO_VehicleMultAI = 4.0f;
bank_float CCollisionEventScanner::sfBikeKO_TrainMult = 0.50f;
bank_float CCollisionEventScanner::sfBikeKO_BikeMult = 2.0f;

bank_float CCollisionEventScanner::sfBikeKO_Mag = 7.0f;
bank_float CCollisionEventScanner::sfBikeKO_EasyMag = 3.0f;
bank_float CCollisionEventScanner::sfBikeKO_HardMag = 10.0f;
bank_float CCollisionEventScanner::sfPushBikeKO_Mag = 6.0f;
bank_float CCollisionEventScanner::sfPushBikeKO_EasyMag = 1.25f;
bank_float CCollisionEventScanner::sfPushBikeKO_HardMag = 8.0f;


bank_float CCollisionEventScanner::sfBikeKO_HeadOnDot = 0.85f;

bank_float CCollisionEventScanner::sfBikeKO_FrontMult = 0.80f; 
bank_float CCollisionEventScanner::sfBikeKO_FrontZMult = 0.60f;

bank_float CCollisionEventScanner::sfBikeKO_TopMult = 1.5f;
bank_float CCollisionEventScanner::sfBikeKO_TopUpsideDownMult = 10.0f;
bank_float CCollisionEventScanner::sfBikeKO_TopUpsideDownMultAI = 50.0f;
bank_float CCollisionEventScanner::sfBikeKO_BottomMult = 0.05f;
bank_float CCollisionEventScanner::sfBikeKO_SideMult = 0.40f;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CPedStuckChecker::CPedStuckChecker()
{
	m_fStuckCounter = 0.0f;
	m_nStuck = PED_NOT_STUCK;
	m_nSkipStuckCheck = 0;
}


dev_float sfStuckTimeForReset = 5.0f;
dev_float sfStuckTimeForDist = 1.0f;
dev_float sfStuckSpeedForDist = 0.2f;
dev_float sfStuckTimeForZ = 2.0f;
dev_float sfStuckSpeedForZ = 0.1f;
//
bool CPedStuckChecker::TestPedStuck(CPed *pPed)
{
	if(!pPed->IsCollisionEnabled() || pPed->GetIsAttached()
		|| pPed->GetIsDeadOrDying()
		|| pPed->GetUsingRagdoll())
	{
		m_fStuckCounter = 0.0f;
		m_nStuck = PED_NOT_STUCK;
		return false;
	}

	// certain tasks will want to skip the stuck checker (swimming, climbing, ladder)
	if(m_nSkipStuckCheck > 0)
	{
		//	&& !pPed->GetPedIntelligence()->GetTaskSwim()
		//	&& !pPed->GetPedIntelligence()->IsPedClimbing()
		//	&& !pPed->GetPedIntelligence()->GetTaskClimbLadder())

		m_nSkipStuckCheck--;
		return false;
	}

	//	Displayf("Stuck Counter %d/n", m_nStuckCounter);

	if(!pPed->GetIsStanding() && !pPed->GetWasStanding() && pPed->GetFrameCollisionHistory()->GetNumCollidedEntities() > 0
		&& !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding))
	{
		const Vector3 vecPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vecMoveDelta;
		vecMoveDelta.Subtract(vecPedPos, m_vecPreviousPos);

		if(m_fStuckCounter > sfStuckTimeForReset || m_fStuckCounter <= 0.0f)
		{
			m_fStuckCounter = fwTimer::GetTimeStep();
			m_nStuck = PED_NOT_STUCK;
			m_vecPreviousPos = vecPedPos;
			vecMoveDelta.Zero();
		}
		else
		{
			m_fStuckCounter += fwTimer::GetTimeStep();
		}

		// first check if we're not moving, so we think we're stuck
		if((m_fStuckCounter > sfStuckTimeForDist && vecMoveDelta.Mag2() < square(sfStuckSpeedForDist*m_fStuckCounter))
			|| (m_fStuckCounter > sfStuckTimeForZ    && rage::Abs(vecMoveDelta.z) < sfStuckSpeedForZ*m_fStuckCounter) )
		{
			m_nStuck = PED_STUCK_STAND_FOR_JUMP;
		}
	}
	else
	{
		m_fStuckCounter = 0.0f;
		m_nStuck = PED_NOT_STUCK;
	}

	if(m_nStuck != PED_NOT_STUCK)
	{
		// stop creating stuck events if scanningPed is now underwater, so that InWater event can get processed
		bool bUnderWater = false;
		if(pPed->GetPedIntelligence()->GetEventOfType(EVENT_IN_WATER)
			&& ((CEventInWater *)pPed->GetPedIntelligence()->GetEventOfType(EVENT_IN_WATER))->GetBuoyancyFraction() > 1.0f)
			bUnderWater = true;

		if(bUnderWater)
		{
			pPed->SetIsStanding(false);
		}
		else
		{
			pPed->SetIsStanding(true);
			pPed->SetWasStanding(true);
			pPed->SetGroundPos(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - pPed->GetCapsuleInfo()->GetGroundToRootOffset() * ZAXIS);
			pPed->SetGroundNormal(ZAXIS);

			CEventStuckInAir event(pPed);
			pPed->GetPedIntelligence()->AddEvent(event);
			return true;
		}
	}
	return false;
}


//////////////////////////////
//PED POTENTIAL COLLISION SCANNER
//////////////////////////////

EXT_PF_TIMER(PotentialCollisionScanner);

const float CPlayerToPedWalkIntoScanner::ms_fPedAvoidDistance=3.5f;	// was 2.5f;	

void CPlayerToPedWalkIntoScanner::Scan(CPed & ped, CPed* pClosestPed)
{
	PF_FUNC(PotentialCollisionScanner);

	if( !ped.IsAPlayerPed() )
	{
		return;
	}

	if(0==pClosestPed)
	{
		//No ped to avoid.
		return;
	}

	if(!ped.IsCollisionEnabled())
	{
		//Ped doesn't use collision.
		return;
	}

	bool bShouldScan = true;

	if( IsRegistered() )
	{
		bShouldScan = ShouldBeProcessedThisFrame();
	}

	if( bShouldScan )
	{
		StartProcess();

		Vec3V vPedPos = ped.GetTransform().GetPosition();
		Vec3V vTarget;

		bool bThisPedIsPlayer = ped.IsAPlayerPed();

		if(!bThisPedIsPlayer)// && !ped.IsNetworkClone())
		{
			//Don't want AI pushing people out the way
			StopProcess();
			return;
		}
		else
		{
			// If this ped is the player, then extrapolate current direction
			TUNE_GROUP_FLOAT(STATIONARYPEDAVOIDANCE, fPlayerOrNetworkCloneLookAheadDist, 0.75f, 0.0f, 5.0f, 0.1f);
			vTarget = vPedPos + VECTOR3_TO_VEC3V(ped.GetVelocity()) * ScalarV(fPlayerOrNetworkCloneLookAheadDist);
		}

		// If movement vector is nearly zero in XY, then don't bother with this scan
		if( (DistXY(vPedPos, vTarget) <= ScalarV(0.1f) ).Getb() )
		{
			StopProcess();
			return;
		}

		// Display our look-ahead vector
#if __DEV
		if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eMovementDebugging)
		{
			grcDebugDraw::Line(vPedPos, vTarget, Color_CadetBlue, Color_BlueViolet);
		}
#endif

		const CEntityScannerIterator entityList = ped.GetPedIntelligence()->GetNearbyPeds();
		const CEntity* pEntity = entityList.GetFirst();
		while(pEntity)
		{
			Assert(pEntity->GetType()==ENTITY_TYPE_PED);
			CPed* pClosePed=(CPed*)pEntity;

			if ( !pClosePed->IsPlayer() )
			{
				Vec3V vClosePedPos = pClosePed->GetTransform().GetPosition();

				//Check that close ped is collidable.
				if(!pClosePed->IsDead() && pClosePed->IsCollisionEnabled())
				{
					TUNE_GROUP_FLOAT(STATIONARYPEDAVOIDANCE, s_CollisionPadding, 0.25f, 0.0f, 5.0f, 0.1f);

					ScalarV vPlayerHalfWidth = ScalarV(ped.GetCapsuleInfo()->GetHalfWidth());
					ScalarV vOtherHalfWidth = ScalarV(pClosePed->GetCapsuleInfo()->GetHalfWidth());
					ScalarV vCollisionPadding = ScalarV(s_CollisionPadding);
					ScalarV vRadius = vPlayerHalfWidth + vOtherHalfWidth + vCollisionPadding;	

#if __DEV
					if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eMovementDebugging)
					{
						grcDebugDraw::Line(vPedPos, vTarget, Color_CadetBlue, Color_BlueViolet);

						Vector3 drawTestLocation(vClosePedPos.GetXf(), vClosePedPos.GetYf(), vClosePedPos.GetZf());
						grcDebugDraw::Circle(drawTestLocation, vRadius.Getf(), Color_red, XAXIS, YAXIS );

					}
#endif

#if __BANK
					//! Hard to test ped shoving, when peds dive out of the way all the time.
					if(!CTaskShovePed::IsTesting())
#endif
					{
						if(pClosePed->GetMotionData()->GetIsStill() || pClosePed->GetMotionData()->GetIsWalking())
						{
							if ( rage::geomSpheres::TestSphereToSeg(vClosePedPos, vRadius, vPedPos, vTarget) )
							{
								CEventPotentialBeWalkedInto beWalkedIntoEvent(
									&ped,
									VEC3V_TO_VECTOR3(vTarget),
									pClosePed->GetMotionData()->GetCurrentMbrY()
								);

								pClosePed->GetPedIntelligence()->AddEvent(beWalkedIntoEvent);
							}
						}
					}

					//! Can Ped shove?
					if(CTaskShovePed::ScanForShoves() && 
						!CPedMotionData::GetIsStill(ped.GetMotionData()->GetCurrentMbrY()) && 
						CTaskShovePed::ShouldAllowTask( &ped, pClosePed))
					{
						CEventShovePed event( pClosePed );
						ped.GetPedIntelligence()->AddEvent(event);
					}
				}
			}

			pEntity = entityList.GetNext();
		}

		StopProcess();
	}
}


//////////////////////////////
//VEHICLE POTENTIAL COLLISION SCANNER
//////////////////////////////

EXT_PF_TIMER(VehicleCollisionScanner);

const float CVehiclePotentialCollisionScanner::ms_fVehiclePotentialRunOverDistance=4.0f;//16.0f;
const float CVehiclePotentialCollisionScanner::ms_fVehicleThreatMultiplier=2.0f;
const float CVehiclePotentialCollisionScanner::ms_fMinAvoidSpeed = 0.05f;//0.01f;	//0.1f;
const float CVehiclePotentialCollisionScanner::ms_fMinAvoidScaredSpeed=0.3f;
const float CVehiclePotentialCollisionScanner::ms_fSlowDiveDist=-0.5f;
const float CVehiclePotentialCollisionScanner::ms_fFastDiveDist=0.1f;
const float CVehiclePotentialCollisionScanner::ms_fVehicleAvoidDistance=7.5f;
dev_float CVehiclePotentialCollisionScanner::ms_fMinIntersectionLength=0.01f; //0.5f;
const int CVehiclePotentialCollisionScanner::ms_iPeriod=500;	//1;

const float g_HalfPedHeight = 0.5f;

float CVehiclePotentialCollisionScanner::GetVehicleAvoidDistance(CVehicle * pVehicle)
{
	return (pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN || pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE || pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI) ? ms_fVehicleAvoidDistance*2.0f : ms_fVehicleAvoidDistance;
}

void CVehiclePotentialCollisionScanner::Scan(const CPed& ped, bool bForce /* = false */)
{
	PF_FUNC(VehicleCollisionScanner);

	bool bShouldScan = false;

	if( IsRegistered() )
	{
		bShouldScan = ShouldBeProcessedThisFrame();
	}
	else
	{
		if(!m_timer.IsSet())
		{
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),ms_iPeriod);
		}

		if(m_timer.IsOutOfTime())
		{	
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),ms_iPeriod);
			bShouldScan = true;
		}
	}


	if( bShouldScan || bForce)
	{
		StartProcess();

		//------------------------------------------------------------------------------------------------------------------
		// Test if ped is wandering
		// This scanner is now only used for wandering peds in order to navigate them past cars parked up on the sidewalk,
		// by creation of the event 'CEventPotentialWalkIntoVehicle'.

		CTaskMoveWander * pMoveWander = (CTaskMoveWander*)ped.GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER);
		if(pMoveWander && pMoveWander->GetState() == CTaskNavBase::NavBaseState_FollowingPath)
		{
			//Test if the ped is walking to a target.
			const CTask* pTaskActiveSimplest=ped.GetPedIntelligence()->GetActiveSimplestMovementTask();
			if( pTaskActiveSimplest )
			{
				const CTaskMoveInterface* pMoveInterface = pTaskActiveSimplest->GetMoveInterface();

				if( !CPedMotionData::GetIsStill(pMoveInterface->GetMoveBlendRatio()) && pMoveInterface->HasTarget() )
				{
					//Get the closest vehicle.
					CVehicle* pVehicle = ped.GetPedIntelligence()->GetClosestVehicleInRange(bForce);
					
					// If we just forced an update, clear the scanners. [11/5/2012 mdawe]
					if (bForce && ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning)) 
					{
						ped.GetPedIntelligence()->ClearScanners();
					}

					if(pVehicle)
					{
						//Test if the ped is near the vehicle or if the ped has just 
						//left a vehicle and should be forced to test the closest vehicle.
						const Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
						Vector3 vDiff = vPedPos - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
						const phBound* pBound=pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
						const Matrix34 mat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());

						Vector3 wldmin = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMin()) + mat.d;
						Vector3 wldmax = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMax()) + mat.d;

						Vector3 up_vec = mat.c;
						float up_planedist = - DotProduct(wldmax, up_vec);
						Vector3 down_vec = mat.c * -1.0f;
						float down_planedist = - DotProduct(wldmin, down_vec);

						// DPs to determine if this vehicle is a threat to ped in Z
						bool isZThreat = ((DotProduct(up_vec, vPedPos) + up_planedist) < g_HalfPedHeight) &&
							((DotProduct(down_vec, vPedPos) + down_planedist) < g_HalfPedHeight);

						// For some larger vehicles, comparing distance between origins against ms_fVehicleAvoidDistance is no good.
						// The vehicles are large enough to still be potential obstacles when outside this range.
						const float fVehicleAvoidDistance = GetVehicleAvoidDistance(pVehicle);

						if(isZThreat && (ped.GetPedConfigFlag( CPED_CONFIG_FLAG_HasJustLeftCar ) || vDiff.Mag2() < (fVehicleAvoidDistance*fVehicleAvoidDistance)))
						{
							CEntityBoundAI bound(*pVehicle,vPedPos.z,ped.GetCapsuleInfo()->GetHalfWidth());

							// Try and catch bad shape test data before we pass it through to ComputeIntersectionLength.
							Assert(ped.GetCurrentPhysicsInst()->IsInLevel());
							Assert(pVehicle->GetCurrentPhysicsInst()->IsInLevel());

							Assertf(vPedPos.Mag2()<1e8*1e8,"CVehiclePotentialCollisionScanner::Scan() - impractical segment starting point: (%f, %f, %f) movetask: %s", vPedPos.x, vPedPos.y, vPedPos.z, pTaskActiveSimplest->GetTaskName());

							static dev_float fExtend = 3.5f;

							Vector3 v1,v2;
							ScalarV distToIsectV;
							float fTotalRouteDist = 0.f;
							Vector3 vPrevRoutePt = vPedPos;
							int nProgress = pMoveWander->GetCurrentRouteProgress();
							int nRouteLength = pMoveWander->GetNumRoutePoints();

							for (int i = nProgress; i < nRouteLength; ++i)
							{
								Vector3 vRouteTarget = pMoveWander->GetRoutePoint(i);
								Assertf(vRouteTarget.Mag2()<1e8*1e8,"CVehiclePotentialCollisionScanner::Scan() - impractical segment ending point: (%f, %f, %f) movetask: %s", vRouteTarget.x, vRouteTarget.y, vRouteTarget.z, pTaskActiveSimplest->GetTaskName());
								
								Vector3 vToRouteTarget = vRouteTarget - vPrevRoutePt;
								vToRouteTarget.Normalize();

								// Target point must be somewhat in front of us, we might not be following the path precicely
								if ((i > nProgress || vToRouteTarget.Dot(VEC3V_TO_VECTOR3(ped.GetTransform().GetForward())) > 0.5f) &&
									!bound.TestLineOfSightReturnDistance(VECTOR3_TO_VEC3V(vPrevRoutePt), VECTOR3_TO_VEC3V(vRouteTarget), distToIsectV))
								{
									//Vector3 vToRouteTarget = vRouteTarget - vPrevRoutePt;
									//vToRouteTarget.Normalize();
									vRouteTarget = vPrevRoutePt + (vToRouteTarget * fVehicleAvoidDistance);

									if ((bound.ComputeHitSideByPosition(vPrevRoutePt) != bound.ComputeHitSideByPosition(vRouteTarget)) &&
										bound.ComputeCrossingPoints(vPrevRoutePt, vToRouteTarget, v2, v1))
									{
										// This might happen if we are inside the vehicle, low lod peds can be that
										if ((v1-vRouteTarget).Mag2() >= (v2-vRouteTarget).Mag2())
										{
											vRouteTarget = v2 + (vToRouteTarget * (fExtend+ped.GetCapsuleInfo()->GetHalfWidth()));
											if(IsGreaterThanAll(distToIsectV, LoadScalar32IntoScalarV(ms_fMinIntersectionLength)))
											{
												Vector3 vDirToTarget = vRouteTarget - vPedPos;
												vDirToTarget.Normalize();
												if(!CPedGeometryAnalyser::IsBrushingContact(ped, *pVehicle, vDirToTarget))
												{
													CEventPotentialWalkIntoVehicle event(pVehicle, ped.GetPedIntelligence()->GetMoveBlendRatioFromGoToTask(), vRouteTarget);
													ped.GetPedIntelligence()->AddEvent(event);
												}
											}
										}
									}

									break;
								}

								if (fTotalRouteDist > fVehicleAvoidDistance)
								{
									break;
								}

								fTotalRouteDist += (vRouteTarget - vPrevRoutePt).XYMag();
								vPrevRoutePt = vRouteTarget;
							}
						}
					}
				}
			}
		}
		StopProcess();
	}
}

//////////////////////////////
//BUILDING COLLISION SCANNER
//////////////////////////////

const float CBuildingPotentialCollisionScanner::ms_fLookAheadDistanceWalking=4.0f;
const float CBuildingPotentialCollisionScanner::ms_fLookAheadDistanceRunning=8.0f;
const float CBuildingPotentialCollisionScanner::ms_fNormalZThreshold=0.707f;
const int CBuildingPotentialCollisionScanner::ms_iPeriod=500;

void CBuildingPotentialCollisionScanner::ScanForBuildingPotentialCollisionEvents(const CPed& UNUSED_PARAM(ped))
{

}

////////////////////////////
//PED ACQUAINTANCE SCANNER
////////////////////////////

float CPedAcquaintanceScanner::ms_fThresholdDotProduct=0.0f;
bool CPedAcquaintanceScanner::ms_bDoPedAcquaintanceScannerLosAsync = true;

// PURPOSE: true => Apply fix for LOSForSoundEvents flag (B*2105791) (Just for testing purposes)
bank_bool CPedAcquaintanceScanner::ms_bFixCheckLoSForSoundEvents = false;

EXT_PF_TIMER(AcquaintanceScanner);

//PURPOSE: Check to see that in SP player wanted level is less than 'wanted', in MP all players wanted level less than 'wanted'
bool CheckPlayerWantedLevelLessThan(eWantedLevel wanted)
{
	if (NetworkInterface::IsGameInProgress())
	{
		//MP - the first player found with a wanted level greater than or equal to the passed in wanted level will return false
		for(ActivePlayerIndex i=0; i<MAX_NUM_ACTIVE_PLAYERS; ++i)
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetActivePlayerFromIndex(i);
			CPed* pPed = pPlayer ? pPlayer->GetPlayerPed() : NULL;
			if(pPed && pPed->GetPlayerWanted()->GetWantedLevel() >= wanted)
			{
				return false;
			}
		}
	}
	else
	{
		//SP - evaluate the local player only
		return (FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() < wanted);
	}

	return true;
}

void CPedAcquaintanceScanner::Scan(CPed& ped, CEntityScannerIterator& entityList)
{
	PF_FUNC(AcquaintanceScanner);

	Assert(IsRegistered());

	// Should this event be processed this frame
	bool bShouldScan = ShouldBeProcessedThisFrame();

	if( bShouldScan )
	{
		StartProcess();
		// Is the ped in a good state for scanning, not dead, injured etc...
		if(PedShouldBeScanning(ped))
		{
			// Scan nearby acquaintances
			ScanNearbyAcquaintances(ped, entityList);

			// clear check time
			m_uNextShouldBeScanningCheckTimeMS = 0;
		}
		else
		{
			// If check time has not been set
			// and this is not a mission ped
			// and this is not a law ped with wanted player
			if( m_uNextShouldBeScanningCheckTimeMS == 0 && !ped.PopTypeIsMission() && (!ped.IsLawEnforcementPed() || CheckPlayerWantedLevelLessThan(WANTED_LEVEL1)) )
			{
				// store a time that we may scan again
				// and in the meantime keep returning false for performance
				static dev_u32 delayDurationMS = 500;
				m_uNextShouldBeScanningCheckTimeMS = fwTimer::GetTimeInMilliseconds() + delayDurationMS;
			}
		}
		StopProcess();
	}
}

// PURPOSE: Check our acquaintance type between ped and pOtherPed and add them to the array of peds if our acquaintance types have a response
// PARAM NOTES: ped - the local ped doing the scans, pOtherPed - the ped we are checking against
//				aPedsToScan - an array of ped pointers which we've decided we are allowed to scan, we end up picking one of these peds
//				aAcquaintanceTypes - the acquaintance types of the peds we decide to scan, numPedsToScan - the number of peds we've decided we can scan
bool CPedAcquaintanceScanner::AddPedToScan(CPed& ped, CPed* pOtherPed, CPed** aPedsToScan, s32* aAcquaintanceTypes, int& numPedsToScan)
{
	bool bPedAdded = false;

	// Work out the highest priority acquaintance type this ped has with the scanned ped.
	s32 iAcquaintanceType = GetHighestAcquintanceTypeBetweenPeds(ped, *pOtherPed);	

	// Some checks to see if we should force add this ped
	const bool bAllowAddForDeadPed = (iAcquaintanceType == ACQUAINTANCE_TYPE_PED_DEAD) && ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents);
	bool bForceAddIfAlive = iAcquaintanceType ==  ACQUAINTANCE_TYPE_PED_DEAD || (iAcquaintanceType == ACQUAINTANCE_TYPE_PED_HATE && ped.GetPedResetFlag(CPED_RESET_FLAG_CanPedSeeHatedPedBeingUsed));
	if(!bAllowAddForDeadPed && !bForceAddIfAlive && ped.GetBlockingOfNonTemporaryEvents())
	{
		// When blocking events we ONLY force add hate or wanted peds, otherwise just early out
		bForceAddIfAlive = (iAcquaintanceType == ACQUAINTANCE_TYPE_PED_HATE || iAcquaintanceType == ACQUAINTANCE_TYPE_PED_WANTED);
		if(!bForceAddIfAlive)
		{
			return bPedAdded;
		}
	}

	// Check to see if this ped has a response for the given acquaintance type
	// This fn can handle being passed ACQUAINTANCE_TYPE_INVALID
	bool isLawEnforcementPed = ped.IsLawEnforcementPed();
	
	// url:bugstar:2557284 - eTEAM_COP isn't used any more, script use teams 0-3 freely with assuming certain team types. Removing below code.
	// || (ped.IsPlayer() && ped.GetNetworkObject() && ped.GetNetworkObject()->GetPlayerOwner() && ped.GetNetworkObject()->GetPlayerOwner()->GetTeam() == eTEAM_COP);

	if(NetworkInterface::IsGameInProgress() && isLawEnforcementPed && !pOtherPed->IsDead() &&
		pOtherPed->GetPlayerWanted() && pOtherPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
	{
		NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::AddPedToScan ACQUAINTANCE_TYPE_PED_WANTED [%s]",pOtherPed->GetNetworkObject() ? pOtherPed->GetNetworkObject()->GetLogName() : pOtherPed->GetModelName());)
			aAcquaintanceTypes[numPedsToScan] = ACQUAINTANCE_TYPE_PED_WANTED;
		aPedsToScan[numPedsToScan++] = pOtherPed;
		bPedAdded = true;
	}
	else if( !pOtherPed->IsDead() && 
		(bForceAddIfAlive || PedHasResponseForAcquaintanceType(ped, *pOtherPed, iAcquaintanceType)) ) 
	{
		// In MP the wanted relationship is only valid if the player has a wanted level
		if(  NetworkInterface::IsGameInProgress() && iAcquaintanceType == ACQUAINTANCE_TYPE_PED_WANTED && pOtherPed->GetPlayerWanted() && pOtherPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN )
		{
			// Reject, the target doesn't have a wanted level
			NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::AddPedToScan Reject, the target doesn't have a wanted level [%s]",pOtherPed->GetNetworkObject() ? pOtherPed->GetNetworkObject()->GetLogName() : pOtherPed->GetModelName());)
		}
		else
		{
			aAcquaintanceTypes[numPedsToScan] = iAcquaintanceType;
			aPedsToScan[numPedsToScan++] = pOtherPed;
			bPedAdded = true;
		}
	}
	// Code to get dead ped investigation stuff working
	else if (pOtherPed->IsDead() && 
		(ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillScanForDeadPeds) 
		|| ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents)))
	{
		aAcquaintanceTypes[numPedsToScan] = ACQUAINTANCE_TYPE_PED_DEAD;
		aPedsToScan[numPedsToScan++] = pOtherPed;
		bPedAdded = true;
	}

	return bPedAdded;
}

void CPedAcquaintanceScanner::ScanNearbyAcquaintances(CPed& ped, CEntityScannerIterator& entityList)
{
	CPed * aPedsToScan[CEntityScanner::MAX_NUM_ENTITIES];
	s32  aAcquaintanceTypes[CEntityScanner::MAX_NUM_ENTITIES];
	int numPedsToScan = 0;

	bool bIsPedLocalPlayer = ped.IsLocalPlayer();
	CPlayerInfo* pLocalPlayerInfo = ped.GetPlayerInfo();

	// If we are a local player then scan through our player peds
	if(pLocalPlayerInfo && bIsPedLocalPlayer && NetworkInterface::IsGameInProgress())
	{
		float fRange = MAX(ped.GetPedIntelligence()->GetPedPerception().GetSeeingRange(), CPedPerception::ms_fSenseRange);
		float fRangeSq = square(fRange);

		// Verify the indices of network players and if they don't exist then mark that index as not seen
		for(u8 playerIndex = 0; playerIndex < MAX_NUM_PHYSICAL_PLAYERS; playerIndex++)
		{
			CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(playerIndex));
			if (!pPlayer || !pPlayer->IsPhysical() || !pPlayer->GetPlayerPed())
			{
				pLocalPlayerInfo->UpdatePlayerLOS(playerIndex, false);
				pLocalPlayerInfo->UpdatePlayerTimeLastScanned(playerIndex, 0);
			}
		}

        // iterate through remote network players
        CNetGamePlayer *localPlayer = NetworkInterface::GetLocalPlayer();

        if(localPlayer)
		{
			CVehicle* pMyVehicle = ped.GetVehiclePedInside();
			bool bInHeli = pMyVehicle && pMyVehicle->InheritsFromHeli();

            unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
            const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
            {
		        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

			    if(localPlayer->GetTeam() != pPlayer->GetTeam())
			    {
				    CPed* pOtherPed = pPlayer->GetPlayerPed();
				    if(pOtherPed)
				    {
					    // If we should scan this ped and they are within our range then we add them to the list, otherwise we say they aren't visible
						if( ShouldScanThisPed(pOtherPed, ped) )
					    {
							// if the other ped is in a heli then we should increase our range
							CVehicle* pOtherPedVeh = pOtherPed->GetVehiclePedInside();
							TUNE_GROUP_FLOAT(PED_ACQUAINTANCE_SCANNER, fPlayerInHeliRangeMultiplier, 2.5f, 0.0f, 50.0f, 0.1f);
							float fMaxDistSq = (bInHeli || (pOtherPedVeh && pOtherPedVeh->InheritsFromHeli())) ? square(fRange * fPlayerInHeliRangeMultiplier) : fRangeSq;
							if( DistSquared(ped.GetTransform().GetPosition(), pOtherPed->GetTransform().GetPosition()).Getf() < fMaxDistSq )
							{
								AddPedToScan(ped, pOtherPed, &aPedsToScan[0], &aAcquaintanceTypes[0], numPedsToScan);
							}
							else
							{
								pLocalPlayerInfo->UpdatePlayerLOS(pPlayer->GetPhysicalPlayerIndex(), false);
							}
					    }
						else
						{
							pLocalPlayerInfo->UpdatePlayerLOS(pPlayer->GetPhysicalPlayerIndex(), false);
						}
				    }
			    }
		    }
        }
	}


	if( !bIsPedLocalPlayer )
	{
		// If have pending requests, we process just those this tick
		for (s32 iRequestIdx = 0; !IsScanRequestQueueEmpty() && iRequestIdx < uMAX_SCAN_REQUESTS; ++iRequestIdx)
		{
			CScanRequest& rRequest = m_aScanRequests[iRequestIdx];
			if (rRequest.IsValid())
			{
				bool bPedAdded = false;

				if (ShouldScanThisPed(rRequest.GetPed(), ped))
				{
					bPedAdded = AddPedToScan(ped, rRequest.GetPed(), &aPedsToScan[0], &aAcquaintanceTypes[0], numPedsToScan);
				}

				if (!bPedAdded)
				{
					ClearScanRequest(iRequestIdx);
				}
			}
		}

		// If we didn't have any request (or the request peds could not be added to the scan), check the entity list
		if (!numPedsToScan)
		{
			// Scan through all the nearby peds and make a list of those with whom this ped has a valid relationship
			CEntity* pEntity = entityList.GetFirst();
			while( pEntity )
			{
				//Get the ith nearby scanningEntity from the nearby ped list.
				Assert(!pEntity || pEntity->GetType()==ENTITY_TYPE_PED);
				CPed* pOtherPed=static_cast<CPed*>(pEntity);

				//Test if we should scan this ped, also checks the pointer is valid.
				if(ShouldScanThisPed(pOtherPed, ped))
				{
					AddPedToScan(ped, pOtherPed, &aPedsToScan[0], &aAcquaintanceTypes[0], numPedsToScan);
				}

				pEntity = entityList.GetNext();
			}
		}
	}


	// If some valid peds to scan, pick a random one from the list
	if( numPedsToScan > 0 )
	{
		CPed* pOtherPed = NULL;
		if(!m_bWaitingOnAsyncTest)
		{
			// Not waiting on a previous async targeting query so we can pick a new target ped.
			s32 iPedToScan = fwRandom::GetRandomNumberInRange(0, numPedsToScan);

			// If our ped is a local player and has a valid info
			if( pLocalPlayerInfo && (bIsPedLocalPlayer || NetworkInterface::IsGameInProgress()) )
			{
				// Look through our peds to scan
				u32 oldestScannedTime = MAX_UINT32;
				for(int i = 0; i < numPedsToScan; i++)
				{
					// Verify the other ped has a player info
					CPlayerInfo* pOtherPlayerInfo = aPedsToScan[i]->GetPlayerInfo();
					if(pOtherPlayerInfo)
					{
						// Get the last scan timed and if its older than our stored value store this as the ped to scan
						// unless of course another ped in the array has an older scanned time
						u32 uTimeLastScanned = pLocalPlayerInfo->GetTimeLastScannedPlayer(pOtherPlayerInfo->GetPhysicalPlayerIndex());
						if(uTimeLastScanned < oldestScannedTime)
						{
							oldestScannedTime = uTimeLastScanned;
							iPedToScan = i;
						}
					}
				}
			}

			pOtherPed = aPedsToScan[iPedToScan];
			m_pPedBeingScanned = pOtherPed;
		}
		else
		{
			pOtherPed = m_pPedBeingScanned;
		}

		// If the ped we have been scanning gets deleted, the Regd ptr will be NULL. In that case, we just reset in order to pick a
		// new ped next time and quit. The cache entry will get cleaned up in CPedGeometryAnalyser::Process() when we call ClearExpiredEntries()
		// on the CanTargetCache.
		ECanTargetResult eLosResult = ENotSureYet;
		if(pOtherPed)
		{
			// If we have a local player info stored and the other ped has a player info
			if(pLocalPlayerInfo)
			{
				CPlayerInfo* pOtherPedInfo = pOtherPed->GetPlayerInfo();
				if(pOtherPedInfo)
				{
					// Get their physical player index, reset the LOS (to false) and update the time we last scanned them to now
					NOTFINAL_ONLY(wantedDebugf3("ResetPlayerLOS() and UpdatePlayerTimeLastScanned by ped[%s] for player[%s]",ped.GetNetworkObject() ? ped.GetNetworkObject()->GetLogName() : ped.GetModelName(),pOtherPed->GetNetworkObject() ? pOtherPed->GetNetworkObject()->GetLogName() : pOtherPed->GetModelName());)
					PhysicalPlayerIndex otherPedIndex = pOtherPedInfo->GetPhysicalPlayerIndex();
					pLocalPlayerInfo->ResetPlayerLOS(otherPedIndex);
					pLocalPlayerInfo->UpdatePlayerTimeLastScanned(otherPedIndex, fwTimer::GetTimeInMilliseconds());
				}
			}

			//Set flag to ignore vehicles
			const bool ignoreVehiclesFlag = (bIsPedLocalPlayer 
				&& ped.GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) 
				&& pOtherPed 
				&& pOtherPed->IsAPlayerPed() 
				&& pOtherPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle));
			// Check visibility to the other ped from this ped.
			m_bWaitingOnAsyncTest = true;
			eLosResult = CanPedSeePed(ped, *pOtherPed);
			if(eLosResult==ECanTarget || eLosResult==ECanNotTarget)
			{
				bool bMustPedBeAbleToSeeOtherPed = MustPedBeAbleToSeeOtherPed(ped, *pOtherPed);
				bool bPedCanSeePed = (eLosResult == ECanTarget);

				NOTFINAL_ONLY(wantedDebugf3("(eLosResult==ECanTarget[%d] || eLosResult==ECanNotTarget[%d]) bMustPedBeAbleToSeeOtherPed[%d] bPedCanSeePed[%d]",eLosResult==ECanTarget,eLosResult==ECanNotTarget,bMustPedBeAbleToSeeOtherPed,bPedCanSeePed);)

				if( (!bIsPedLocalPlayer && (!bMustPedBeAbleToSeeOtherPed || bPedCanSeePed)) ||
					(bIsPedLocalPlayer && pOtherPed->GetIsVisibleInSomeViewportThisFrame() && !CPedTargetEvaluator::GetIsLineOfSightBlockedBetweenPeds(&ped, pOtherPed, false, false, false, ignoreVehiclesFlag, false)) )
				{
					// Because relationship groups can change while waiting on the test results, we need to regrab the acquaintance type to make sure it's up to date
					s32 iAcquaintanceType = GetHighestAcquintanceTypeBetweenPeds(ped, *pOtherPed);

					NOTFINAL_ONLY(wantedDebugf3("next call ShouldAddAcquaintanceEvent");)

					if(ShouldAddAcquaintanceEvent(ped, iAcquaintanceType, *pOtherPed))
					{
						// This isn't in AddAcquaintance event due to ped being passed in as const
						if (ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents) 
							&& iAcquaintanceType == ACQUAINTANCE_TYPE_PED_DEAD)
						{
							if (!ped.GetPedIntelligence()->HasPedSeenDeadPedRecently())
							{
								ped.GetPedIntelligence()->RecordPedSeenDeadPed();
								if(NetworkInterface::IsGameInProgress())
								{
									CNetworkPedSeenDeadPedEvent::Trigger(pOtherPed, &ped);
								}
								
								aiDebugf2("Frame %i: Adding CEventPedSeenDeadPed to script ai group ped %p has found dead ped %p", fwTimer::GetFrameCount(), &ped, pOtherPed);
								GetEventScriptAIGroup()->Add(CEventPedSeenDeadPed(*pOtherPed, ped));
							}
						}
						else
						{
							// This ped can see the other ped, generate an acquaintance event
							AddAcquaintanceEvent( ped, iAcquaintanceType, pOtherPed );
						}
					}
				}

				eLosResult = EReadyForNewPedPair;
			}
		}

		if(eLosResult==EReadyForNewPedPair || !pOtherPed)
		{
			// Either our async probe has returned or timed out and we've done a synchronous probe or the ped we were scanning
			// has been deleted. Either way, we can now select a new random ped.
			m_bWaitingOnAsyncTest = false;

			if (m_pPedBeingScanned)
			{
				ClearPedScanRequest(*m_pPedBeingScanned);
				m_pPedBeingScanned = NULL;
			}
		}
	}

	// NOTE[egarcia]: We clear the scan request queue just to be sure we are not leaving anyone hanging around
	if (!IsScanRequestQueueEmpty())
	{
		//aiWarningf("CPedAcquaintanceScanner::ScanNearbyAcquaintances: Scan Requests pending...");
		ClearAllScanRequests();
	}
}


bool CPedAcquaintanceScanner::ArePedsInSameVehicle(const CPed& rPed, const CPed& rOtherPed) const
{
	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the other ped is in a vehicle.
	const CVehicle* pOtherVehicle = rOtherPed.GetVehiclePedInside();
	if(!pOtherVehicle)
	{
		return false;
	}
	
	//Ensure the vehicles match.
	if(pVehicle != pOtherVehicle)
	{
		return false;
	}
	
	return true;
}


bool CPedAcquaintanceScanner::IsPedInFastMovingVehicle(const CPed& rPed) const
{
	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle is moving fast.
	TUNE_GROUP_FLOAT(PED_ACQUAINTANCE_SCANNER, fMinSpeedForVehicleToBeConsideredMovingFast, 5.0f, 0.0f, 50.0f, 1.0f);
	float fMinSpeedForVehicleToBeConsideredMovingFastSq = square(fMinSpeedForVehicleToBeConsideredMovingFast);
	float fSmoothedSpeedSq = pVehicle->GetIntelligence()->GetSmoothedSpeedSq();
	if(fSmoothedSpeedSq < fMinSpeedForVehicleToBeConsideredMovingFastSq)
	{
		return false;
	}
	
	return true;
}


bool CPedAcquaintanceScanner::MustPedBeAbleToSeeOtherPed(const CPed& rPed, const CPed& rOtherPed) const
{
	//There are some situations where we want acquaintance events to fire, even if the ped can't see the other ped.
	
	//Check if we are in a vehicle.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if the other ped is standing on our vehicle.
		if(rOtherPed.GetGroundPhysical() == pVehicle)
		{
			return false;
		}

		//Check if the other ped is driving on our vehicle.
		const CVehicle* pOtherVehicle = rOtherPed.GetVehiclePedInside();
		if(pOtherVehicle && (pOtherVehicle->GetVehicleDrivingOn() == pVehicle))
		{
			return false;
		}
	}

	//Grab the current time.
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	
	//Check if we have been knocked to the ground recently by another ped.
	u32 uLastTimeWeWereKnockedToTheGround = rPed.GetPedIntelligence()->GetLastTimeWeWereKnockedToTheGround();
	static u32 s_uMaxTimeSinceWeWereLastKnockedToTheGround = 10000;
	if((uLastTimeWeWereKnockedToTheGround + s_uMaxTimeSinceWeWereLastKnockedToTheGround) > uCurrentTime)
	{
		//Check if the entity that knocked us to the ground was the other ped.
		const CEntity* pLastEntityThatKnockedUsToTheGround = rPed.GetPedIntelligence()->GetLastEntityThatKnockedUsToTheGround();
		if(pLastEntityThatKnockedUsToTheGround == &rOtherPed)
		{
			return false;
		}
	}

	//Check if we have been rammed in a vehicle recently by another ped.
	u32 uLastTimeWeWereRammedInVehicle = rPed.GetPedIntelligence()->GetLastTimeWeWereRammedInVehicle();
	static u32 s_uMaxTimeSinceWeWereLastRammedInVehicle = 10000;
	if((uLastTimeWeWereRammedInVehicle + s_uMaxTimeSinceWeWereLastRammedInVehicle) > uCurrentTime)
	{
		//Check if the ped that rammed us in vehicle was the other ped.
		const CPed* pLastPedThatRammedUsInVehicle = rPed.GetPedIntelligence()->GetLastPedThatRammedUsInVehicle();
		if(pLastPedThatRammedUsInVehicle == &rOtherPed)
		{
			return false;
		}
	}
	
	return true;
}


bool CPedAcquaintanceScanner::ShouldAddAcquaintanceEvent(const CPed& rPed, const int iAcquaintanceType, const CPed& rOtherPed) const
{
	// Don't add acquaintance type if it's invalid
	if(iAcquaintanceType == ACQUAINTANCE_TYPE_INVALID)
	{
		NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::ShouldAddAcquaintanceEvent--ACQUAINTANCE_TYPE_INVALID-->return false");)
		return false;
	}

	//Check if the ped should ignore hated peds in fast moving vehicles.
	if((iAcquaintanceType == ACQUAINTANCE_TYPE_PED_HATE) &&
		rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_IgnoreHatedPedsInFastMovingVehicles))
	{
		//Check if the other ped is in a fast moving vehicle, and the peds are not in the same vehicle.
		if(IsPedInFastMovingVehicle(rOtherPed) && !ArePedsInSameVehicle(rPed, rOtherPed))
		{
			NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::ShouldAddAcquaintanceEvent--(IsPedInFastMovingVehicle(rOtherPed) && !ArePedsInSameVehicle(rPed, rOtherPed))-->return false");)
			return false;
		}
	}

	//Check if the ped should ignore the other ped, when they are wanted.
	if((iAcquaintanceType == ACQUAINTANCE_TYPE_PED_WANTED) &&
		rOtherPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_IgnoredByOtherPedsWhenWanted))
	{
		NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::ShouldAddAcquaintanceEvent--ACQUAINTANCE_TYPE_PED_WANTED && BF_IgnoredByOtherPedsWhenWanted-->return false");)
		return false;
	}
	
	return true;
}


bool CPedAcquaintanceScanner::AddAcquaintanceEvent(const CPed& ped, const int iAcquaintanceType, CPed* pAcquaintancePed)
{	
	bool bAdded=false;

	switch(iAcquaintanceType)
	{
		//Create the appropriate event.
		case ACQUAINTANCE_TYPE_PED_RESPECT:
			{
				// Chat stuff
				CTask* pTask = CChatHelper::GetPedsTask(const_cast<CPed*>(&ped));
				if (pTask)
				{
					CChatHelper* pChatHelper = pTask->GetChatHelper();
					if (pChatHelper)
					{
						pChatHelper->SetBestChatPed(pAcquaintancePed);
					}
				}
				
				//CEventAcquaintancePedRespect event(pAcquaintancePed);
				//bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false); // Gets the chat stuff working
			}
			break;
		case ACQUAINTANCE_TYPE_PED_LIKE:
			{
				CEventAcquaintancePedLike event(pAcquaintancePed);
				bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false);
			}
			break;
		case ACQUAINTANCE_TYPE_PED_IGNORE:		
			break;
		case ACQUAINTANCE_TYPE_PED_DISLIKE:
			{
				CEventAcquaintancePedDislike event(pAcquaintancePed);
				bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false);
			}
			break;
		case ACQUAINTANCE_TYPE_PED_WANTED:
			{
				// If we have a net game in progress, the other ped is a player we need
				// to do some additional stuff (as we forced our way through previous checks to accomplish this)
				if(NetworkInterface::IsGameInProgress() && pAcquaintancePed->IsPlayer())
				{
					NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::AddAcquaintanceEvent--ACQUAINTANCE_TYPE_PED_WANTED");)

					// Get and verify both player infos
					CPed* localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
					if (localPlayer)
					{
						CPlayerInfo* pLocalPlayerInfo = localPlayer->GetPlayerInfo();
						CPlayerInfo* pNetPlayerInfo = pAcquaintancePed->GetPlayerInfo();

						//Only valid if this isn't targetting the local player as this is for cop peds targetting other players and to fix that situation.
						if(pLocalPlayerInfo && pNetPlayerInfo && pLocalPlayerInfo != pNetPlayerInfo)
						{
							// Here we are updating the LOS to this net player to be true, meaning our local player sees this player
							NOTFINAL_ONLY(wantedDebugf3("CPedAcquaintanceScanner::AddAcquaintanceEvent--ACQUAINTANCE_TYPE_PED_WANTED--UpdatePlayerLOS(true)--UpdatePlayerTimeLastScanned");)
							pLocalPlayerInfo->UpdatePlayerLOS(pNetPlayerInfo->GetPhysicalPlayerIndex(), true);
							pLocalPlayerInfo->UpdatePlayerTimeLastScanned(pNetPlayerInfo->GetPhysicalPlayerIndex(), fwTimer::GetTimeInMilliseconds());
						}
					}
				}

				if(!ped.GetBlockingOfNonTemporaryEvents())
				{
					CEventAcquaintancePedWanted event(pAcquaintancePed);
					bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false);
				}
				else if(ped.GetPedIntelligence()->IsThreatenedBy(*pAcquaintancePed) && ped.GetPedIntelligence()->CanAttackPed(pAcquaintancePed))
				{
#if __BANK
					CAILogManager::GetLog().Log("CPedAcquaintanceScanner::AddAcquaintanceEvent, ACQUAINTANCE_TYPE_PED_WANTED, NonTemporaryEvents blocked. Registering threat without adding the event! Ped %s, AcquaintancePed %s.\n", AILogging::GetEntityDescription(&ped).c_str(), AILogging::GetEntityDescription(pAcquaintancePed).c_str());
#endif // __BANK
					CPedTargetting* pPedTargetting = ped.GetPedIntelligence()->GetTargetting(true);
					if (pPedTargetting)
					{
						pPedTargetting->RegisterThreat(pAcquaintancePed);
					}
				}
			}
			break;
		case ACQUAINTANCE_TYPE_PED_HATE:
			{	
				bool bAddEvent = (!ped.GetBlockingOfNonTemporaryEvents() || ped.GetPedResetFlag(CPED_RESET_FLAG_CanPedSeeHatedPedBeingUsed));
				ped.GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_Alert);
#if __BANK
				if (!bAddEvent)
				{
					CAILogManager::GetLog().Log("CPedAcquaintanceScanner::AddAcquaintanceEvent, ACQUAINTANCE_TYPE_PED_HATE, NonTemporaryEvents blocked. Increasing alertness state without adding the event! Ped %s, AcquaintancePed %s.\n", AILogging::GetEntityDescription(&ped).c_str(), AILogging::GetEntityDescription(pAcquaintancePed).c_str());
				}
#endif // __BANK


				// Check to see if the target ped is actually at a distance where they aren't properly identified
				if( DistSquared(ped.GetTransform().GetPosition(), pAcquaintancePed->GetTransform().GetPosition()).Getf() > rage::square(ped.GetPedIntelligence()->GetPedPerception().GetIdentificationRange()) &&
					ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate) &&
					pAcquaintancePed->GetPedIntelligence()->GetPedStealth().GetFlags().IsFlagSet(CPedStealth::SF_TreatedAsActingSuspiciously))
				{
					if(bAddEvent)
					{
						CEventUnidentifiedPed event(pAcquaintancePed);
						bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false);
					}
				}
				else
				{
					CPedTargetting* pPedTargetting = ped.GetPedIntelligence()->GetTargetting(true);
					if (pPedTargetting)
					{
						pPedTargetting->RegisterThreat(pAcquaintancePed);
					}

#if __BANK
					if(!bAddEvent)
					{
						CAILogManager::GetLog().Log("CPedAcquaintanceScanner::AddAcquaintanceEvent, ACQUAINTANCE_TYPE_PED_HATE, NonTemporaryEvents blocked. Registering threat without adding the event! Ped %s, AcquaintancePed %s.\n", AILogging::GetEntityDescription(&ped).c_str(), AILogging::GetEntityDescription(pAcquaintancePed).c_str());
					}
#endif // __BANK
					
					if(bAddEvent)
					{
						CEventAcquaintancePedHate event(pAcquaintancePed);
						bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false);
					}
				}
			}
			break;
		case ACQUAINTANCE_TYPE_PED_DEAD:
			{
				// If the dead ped has not already been reported
				if (taskVerifyf(pAcquaintancePed->IsDead(),"Ped Isn't Dead, something has gone wrong with the aquaintance scanning") && !pAcquaintancePed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasDeadPedBeenReported ))
				{
					CPedIntelligence* pPedIntell = ped.GetPedIntelligence();
					CTask* pTask = pPedIntell->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
				
					if (pTask)
					{
						static_cast<CTaskInvestigate*>(pTask)->QuitForDeadPedInvestigation(pAcquaintancePed);
					}	
					else
					{
						CEventAcquaintancePedDead event(pAcquaintancePed, true);
						bAdded=(ped.GetPedIntelligence()->AddEvent(event) ? true : false);
					}
				}
			}
			break;
		default:
			Assert(false);
			bAdded=false;
			break; 
	}

	return bAdded;
}	


bool CPedAcquaintanceScanner::PedShouldBeScanning(const CPed& ped) const
{
	if(ped.IsDead())
	{
		return false;
	}

	// If we returned false before, continue to do so for a short time
	if( m_uNextShouldBeScanningCheckTimeMS > 0 && fwTimer::GetTimeInMilliseconds() < m_uNextShouldBeScanningCheckTimeMS )
	{
		return false;
	}

	// If this is a mission ped
	if(ped.PopTypeIsMission())
	{
		//Test if the scanner is activated everywhere.
		if(!m_bActivatedEverywhere)
		{
			//Scanner isn't activated everywhere.  
			//Test if the scanner is activated under specific conditions.

			//Test if the ped is in a car.
			if(!ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || !m_bActivatedInVehicle)
			{
				//Ped is in a car and the scanner is activated for cars
				return false;
			}

			//Test if the ped is executing a script command.
			if(!CPedScriptedTaskRecord::GetStatus(&ped) || !m_bActivatedDuringScriptCommands)
			{
				//Ped is on script command and scanner is activated for script commands.
				return false;
			}
		}		
	}

	//Check if the ped is already responding to an acquaintance event.
	CEvent* pEvent=ped.GetPedIntelligence()->GetCurrentEvent();
	if(pEvent && pEvent->GetEventType()==EVENT_ACQUAINTANCE_PED_HATE)
	{
		bool bForceScan = (ped.ShouldBehaveLikeLaw() && ped.PopTypeIsMission());
		if(!bForceScan)
		{
			return false;
		}
	}

	if (ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillScanForDeadPeds) ||
		ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents))
	{
		return true;
	}

	// Only bother scanning at all if the ped has any hate relationships
	if( !PedHasRelationshipsToScan(ped) )
	{
		return false;
	}

	return true;
}


bool CPedAcquaintanceScanner::PedHasRelationshipsToScan( const CPed &ped ) const
{
	// If the ped is in a group then always scan
	if( ped.GetPedGroupIndex() != PEDGROUP_INDEX_NONE )
	{
		return true;
	}

	// Is the ped a random type?
	bool bIsPopTypeRandom = ped.PopTypeIsRandom();
	
	// If the ped is a random type and there is a riot, always scan
	if( bIsPopTypeRandom && CRiots::GetInstance().GetEnabled() )
	{
		return true;
	}

	// By default the ped does not need to scan
	bool bHasRelationshipsToScan = false;

	// Get the ped's relationship group
	const CRelationshipGroup* pRelGroup = ped.GetPedIntelligence()->GetRelationshipGroup();
	if( pRelGroup )
	{
		// If the ped is a random type
		if( bIsPopTypeRandom )
		{
			// Random peds scan if they have any relationships they hate
			// OR if they are law enforcement and have any that are wanted
			if( pRelGroup->HasAnyRelationShipOfType( ACQUAINTANCE_TYPE_PED_HATE ) ||
				(ped.ShouldBehaveLikeLaw() && pRelGroup->HasAnyRelationShipOfType( ACQUAINTANCE_TYPE_PED_WANTED ) ) )
			{
				bHasRelationshipsToScan = true;
			}
		}
		else // ped is not random
		{
			// Mission peds scan if they have any relationships of certain types
			if( pRelGroup->HasAnyRelationShipOfType( ACQUAINTANCE_TYPE_PED_HATE ) || 
				pRelGroup->HasAnyRelationShipOfType( ACQUAINTANCE_TYPE_PED_DISLIKE) || 
				pRelGroup->HasAnyRelationShipOfType( ACQUAINTANCE_TYPE_PED_WANTED ) )
			{
				bHasRelationshipsToScan = true;
			}
		}
	}

	return bHasRelationshipsToScan;
}

bool CPedAcquaintanceScanner::ShouldScanThisPed( const CPed* pOtherPed, CPed& ped )
{
	// Invalid ped, quit
	if( pOtherPed == NULL )
	{
		return false;
	}

#if !__FINAL
	// Debug test for ignoring an invisible player ped
	if( pOtherPed->IsPlayer() && CPlayerInfo::ms_bDebugPlayerInvisible )
	{
		return false;
	}
#endif // !__FINAL

	// SP only - Don't scan peds who are in the targeting list already
	// MP need to rescan these as they need to get wanted times updated - including local player
	if ( !NetworkInterface::IsGameInProgress() )
	{
		CPedTargetting* pPedTargetting = ped.GetPedIntelligence()->GetTargetting( false );
		if( pPedTargetting && pPedTargetting->FindTarget(pOtherPed) )
		{
			if( !ped.GetPedResetFlag(CPED_RESET_FLAG_CanPedSeeHatedPedBeingUsed) ||
				!ped.GetBlockingOfNonTemporaryEvents() )
			{
				return false;
			}
		}
	}

	// If we can't scan for dead peds and the ped is dead, do not scan this ped
	if (pOtherPed->IsDead() && !ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillScanForDeadPeds))
	{
		if (ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents))
		{
			return true;
		}
		else if (pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasDeadPedBeenReported ) == true)
		{	
			return false;
		}	
	}

	return true;
}

int CPedAcquaintanceScanner::GetHighestAcquintanceTypeBetweenPeds( const CPed& ped, const CPed& otherPed )
{
	if (ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_WillGenerateDeadPedSeenScriptEvents) && otherPed.IsDead())
	{
		return ACQUAINTANCE_TYPE_PED_DEAD;
	}

	const CRelationshipGroup* pRelGroup = ped.GetPedIntelligence()->GetRelationshipGroup();
	if( pRelGroup )
	{
		const CPedIntelligence* pOtherIntelligence = otherPed.GetPedIntelligence();
		pedAssert(pOtherIntelligence->GetRelationshipGroup());
		eRelationType relationType = pRelGroup->GetRelationship(pOtherIntelligence->GetRelationshipGroupIndex());
		
		if(relationType == ACQUAINTANCE_TYPE_PED_DISLIKE && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat) &&
			ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) )
		{
			relationType = ACQUAINTANCE_TYPE_PED_HATE;
		}

		if( relationType < ACQUAINTANCE_TYPE_PED_WANTED || relationType == ACQUAINTANCE_TYPE_INVALID )
		{
			// Check the players effective wanted level directly as wanted level acquaintances aren't synced
			// during network games
			if(otherPed.IsPlayer())
			{
				if( otherPed.GetPlayerWanted() && otherPed.GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN )
				{
#if __ASSERT
					if (ped.GetPedIntelligence()->IsFriendlyWith(otherPed))
					{
						aiDisplayf("Setting acquaintance type to ACQUAINTANCE_TYPE_PED_WANTED Ped %s in relationship group %s (ped group %p, ped type %i) is friendly with ped %s in relationship group %s (ped group %p, ped type %i)",
							ped.GetModelName(), ped.GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr(), ped.GetPedsGroup(), ped.GetPedType(),
							otherPed.GetModelName(), otherPed.GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr(), otherPed.GetPedsGroup(), otherPed.GetPedType());
					}
#endif // __ASSERT
					relationType = ACQUAINTANCE_TYPE_PED_WANTED;
				}
			}

			// If the relationship to the enemy ped is neutral but they are aggressive to this ped, 
			// or a ped in this peds group, treat them as a threat.
			// Only do this if in a group and a mission ped, swat/cops should automatically do this
			if( ped.PopTypeIsMission() && ped.GetPedsGroup() && !ped.GetPedIntelligence()->IsFriendlyWith(otherPed) &&
				( ( otherPed.GetWeaponManager() && otherPed.GetWeaponManager()->GetIsArmed() ) || !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) ) )
			{
				CEntity* pEntity = otherPed.GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
				CPed* pPedsTarget = NULL;
				if( pEntity && pEntity->GetIsTypePed() )
				{
					pPedsTarget = static_cast<CPed*>(pEntity);
				}
				else if( pEntity && pEntity->GetIsTypeVehicle() )
				{
					pPedsTarget = static_cast<CVehicle*>(pEntity)->GetDriver();
				}
				if( pPedsTarget )
				{
					if( ped.GetPedsGroup() &&
						(ped.GetPedIntelligence()->IsFriendlyWith( *pPedsTarget) ||
						ped.GetPedsGroup() == pPedsTarget->GetPedsGroup() ))
					{
#if __ASSERT
						if (ped.GetPedIntelligence()->IsFriendlyWith(otherPed))
						{
							aiDisplayf("Setting acquaintance type to ACQUAINTANCE_TYPE_PED_HATE Ped %s in relationship group %s (ped group %p, ped type %i) is friendly with ped %s in relationship group %s (ped group %p, ped type %i)",
								ped.GetModelName(), ped.GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr(), ped.GetPedsGroup(), ped.GetPedType(),
								otherPed.GetModelName(), otherPed.GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr(), otherPed.GetPedsGroup(), otherPed.GetPedType());
							aiAssertf(0,"Please add a bug and attach logs");
						}
#endif // __ASSERT
						relationType = ACQUAINTANCE_TYPE_PED_HATE;
					}
				}
			}
		}

		if(ped.PopTypeIsRandom() && CRiots::GetInstance().GetEnabled() && !ped.GetPedIntelligence()->IsFriendlyWith(otherPed))
		{
			relationType = ACQUAINTANCE_TYPE_PED_HATE;
		} 

		return relationType;
	}
	return ACQUAINTANCE_TYPE_INVALID;
}

bool CPedAcquaintanceScanner::PedHasResponseForAcquaintanceType( CPed& ped, CPed& otherPed, s32 iAcquaintanceScanType )
{
	//Check if ped has a response set up to any acquaintance event.
	switch(iAcquaintanceScanType)
	{
	case ACQUAINTANCE_TYPE_PED_HATE:	
		{
			CEventAcquaintancePedHate tempEvent(&otherPed);
			return HasEventResponse(ped, &tempEvent);
		}
	case ACQUAINTANCE_TYPE_PED_WANTED:
		{
			// url:bugstar:2557284 - eTEAM_COP isn't used any more, script use teams 0-3 freely with assuming certain team types. Removing below code.

			// If this is a network game, our ped is a local player and we are checking against another player we want
			// to assume we have a response (we don't, but we use this assumption for visibility storage for other players)
			//if(NetworkInterface::IsGameInProgress() && ped.IsLocalPlayer() && otherPed.IsPlayer() &&
			//   ped.GetPlayerInfo() && ped.GetPlayerInfo()->Team == eTEAM_COP)
			//{
			//	return true;
			//}
			//else

			CEventAcquaintancePedWanted tempEvent(&otherPed);
			return HasEventResponse(ped, &tempEvent);
			
		}
	case ACQUAINTANCE_TYPE_PED_DISLIKE:
		{
			CEventAcquaintancePedDislike tempEvent(&otherPed);
			return HasEventResponse(ped, &tempEvent);
		}
	case ACQUAINTANCE_TYPE_PED_IGNORE:
		return true;
	default:
		return false;
	}
}

bool CPedAcquaintanceScanner::HasEventResponse(CPed& ped, CEventEditableResponse* pEvent)
{
	CPedEventDecisionMaker& rPedDecisionMaker = ped.GetPedIntelligence()->GetPedDecisionMaker();

	CEventResponseTheDecision decision;
	rPedDecisionMaker.MakeDecision(&ped, pEvent, decision);
	if(decision.HasDecision())
	{
		return true;
	}

	return false;
}

ECanTargetResult CPedAcquaintanceScanner::CanPedSeePed( CPed& ped, CPed& otherPed )
{
	ECanTargetResult returnValue = ENotSureYet;
	s32 iTargetFlags = !(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || otherPed.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) ? TargetOption_doDirectionalTest : 0;
	u32 uCacheTimeOut = CAN_TARGET_CACHE_TIMEOUT;

	CPedTargetting* pPedTargetting = ped.GetPedIntelligence()->GetTargetting(false);
	bool bHasTargetInfo = false;
	if( pPedTargetting == NULL || (!pPedTargetting->AreTargetsWhereaboutsKnown(NULL, &otherPed, NULL, &bHasTargetInfo) || !bHasTargetInfo ) )
	{
		iTargetFlags |= TargetOption_IgnoreTargetsCover;
		iTargetFlags |= TargetOption_UseFOVPerception;
		iTargetFlags &= ~TargetOption_doDirectionalTest;
	}

	if(ped.GetPedResetFlag(CPED_RESET_FLAG_IgnoreTargetsCoverForLOS))
	{
		iTargetFlags |= TargetOption_IgnoreTargetsCover;
	}

	if (ms_bFixCheckLoSForSoundEvents)
	{
		// If we have a pending scan request for this ped, check the flags
		const CScanRequest* pScanRequest = GetPedScanRequest(otherPed);
		if (pScanRequest)
		{
			iTargetFlags |=  pScanRequest->GetAddTargetFlags();
			iTargetFlags &= ~pScanRequest->GetRemoveTargetFlags();
			uCacheTimeOut = 0; // Force the test
		}
	}
	else
	{
		// If this ped can hear sound events, check to see if we're within sound range of the other ped.
		// Unless CPED_CONFIG_FLAG_CheckLoSForSoundEvents is set, we can skip doing FOV perception if we're within sound range. [5/14/2013 mdawe]
		if ( (iTargetFlags & TargetOption_UseFOVPerception) && 
			ped.GetPedConfigFlag(CPED_CONFIG_FLAG_ListensToSoundEvents) && 
			!ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate) && 
			!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CheckLoSForSoundEvents) )
		{
			CPlayerInfo *pPlayerInfo = otherPed.GetPlayerInfo();
			if (pPlayerInfo)
			{
				ScalarV vPlayerSoundRangeSq = ScalarVFromF32(rage::square(pPlayerInfo->GetStealthNoise()));
				ScalarV vDistToTargetSq     = DistSquared(ped.GetTransform().GetPosition(), otherPed.GetTransform().GetPosition());
				if (IsLessThanAll(vDistToTargetSq, vPlayerSoundRangeSq))
				{
					// Script can set a distance beyond which sounds should not be heard.
					if (ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEvents) >= 0)
					{
						ScalarV vMaxDistanceSq = ScalarVFromF32(rage::square(ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEvents)));
						if (IsLessThanAll(vDistToTargetSq, vMaxDistanceSq))
						{
							iTargetFlags &= ~TargetOption_UseFOVPerception;
						}
					}
					else
					{
						iTargetFlags &= ~TargetOption_UseFOVPerception;
					}
				}
			}
		}
	}


	// IF the peds targeting system is active and scanning this target, use its LOS
	bool bScanTarget = true;
	if( pPedTargetting )
	{
		CSingleTargetInfo* pTargetInfo = pPedTargetting->FindTarget(&otherPed);
		if( pTargetInfo )
		{
			LosStatus losStatus = pTargetInfo->GetLosStatus();
			if( losStatus == Los_clear ||  losStatus == Los_blockedByFriendly)
			{
				//SP - Allow as before - keep the same.
				//MP - Might have target but might no longer be valid - check to see if the last seen timer is valid before using
				if (!NetworkInterface::IsGameInProgress() || !pTargetInfo->HasLastSeenTimerExpired())
				{
					BANK_ONLY(wantedDebugf3("CPedAcquaintanceScanner::CanPedSeePed--losStatus[%d] ( losStatus == Los_clear ||  losStatus == Los_blockedByFriendly) note: GetTimeUntilLastSeenTimerExpiresMs[%u]",losStatus,pTargetInfo->GetTimeUntilLastSeenTimerExpiresMs());)
					returnValue = ECanTarget;
					bScanTarget = false;
				}
			}
			else if( losStatus == Los_blocked )
			{
				returnValue = ECanNotTarget;
				bScanTarget = false;
			}
		}
	}

	if( bScanTarget )
	{
		if(ms_bDoPedAcquaintanceScannerLosAsync)
		{
			// The following call will check if there is a valid entry for these two peds in the targeting cache and
			// either return that result or, if there is no valid cache entry, try and do an asynchronous probe if there
			// is free space to store the result within the cache. If there is no space in the cache, a synchronous probe
			// will be done instead.
			ECanTargetResult eLosRet = CPedGeometryAnalyser::CanPedTargetPedAsync(ped,otherPed,iTargetFlags,false,CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr, uCacheTimeOut);

			bool bAsyncTimedOut = false;

			// If we still have a valid result for the target query in the cache, just use this.
			if(eLosRet==ECanTarget || eLosRet==ECanNotTarget)
			{
				BANK_ONLY(wantedDebugf3("CPedAcquaintanceScanner::CanPedSeePed--(eLosRet==ECanTarget[%d] || eLosRet==ECanNotTarget[%d])",eLosRet==ECanTarget,eLosRet==ECanNotTarget);)
				returnValue = eLosRet;
			}
			else if(eLosRet==ENotSureYet)
			{
				// There is an asynchronous probe in flight for this query. Check if we can wait for it.

				static const u32 knTimeToWaitOnAsyncTargetProbes = 500; // Time in milliseconds.
				if(fwTimer::GetTimeInMilliseconds() - CPedGeometryAnalyser::m_nAsyncProbeTimeout > knTimeToWaitOnAsyncTargetProbes)
				{
					// We have waited too long for the async probe between these two peds to complete. We are now going
					// to cancel it and do a synchronous probe later in CanPedTargetPed().
					CPedGeometryAnalyser::CancelAsyncProbe(&ped, &otherPed);
					CPedGeometryAnalyser::ClearCachedResult(&ped, &otherPed);

					bAsyncTimedOut = true;

					// Signal the scanner that we can move on to another random ped.
					returnValue = EReadyForNewPedPair;
				}
				else
				{
					// We are ok to continue waiting on this async request.
					returnValue = ENotSureYet;
				}
			}

			if(bAsyncTimedOut)
			{
				// If we get here, we have to do a synchronous test.
#if !__FINAL
				CPedGeometryAnalyser::ms_iNumLineTestsNotProcessedInTime++;
				CPedGeometryAnalyser::ms_iNumLineTestsNotAsync++;
#endif
				// If we still don't have a result at this point, we're going to have to perform a synchronous probe.
				// (This call is forced into synchronous mode by the default NULL value for pEmptyCacheEntry.)
				BANK_ONLY(wantedDebugf3("CPedAcquaintanceScanner::CanPedSeePed--bAsyncTimedOut--invoke CPedGeometryAnalyser::CanPedTargetPed");)
				returnValue = CPedGeometryAnalyser::CanPedTargetPed(ped,otherPed,iTargetFlags) ? ECanTarget : ECanNotTarget;
			}
		}
		else
		{
			BANK_ONLY(wantedDebugf3("CPedAcquaintanceScanner::CanPedSeePed--else--invoke CPedGeometryAnalyser::CanPedTargetPed");)
			returnValue = CPedGeometryAnalyser::CanPedTargetPed(ped,otherPed,iTargetFlags) ? ECanTarget : ECanNotTarget;
		}
	}


	// Field of view perception, adjust the distances to take into account the peds alertness this time
	// If this fails, mark the target as spotted
// 	if( bCanPedTargetPed && iTargetFlags & TargetOption_UseFOVPerception )
// 	{
// 		if( !ped.GetPedIntelligence()->GetPedPerception().ComputeFOVVisibility(&otherPed, &otherPed.GetPosition(), CPedPerception::FOV_AdjustPerceptionForAlertness|CPedPerception::FOV_AdjustPerceptionForLighting) )
// 		{
// 			otherPed.GetPedIntelligence()->GetPedStealth().SpottedThisFrame();
// 			return false;
// 		}
// 	}

#if DEBUG_DRAW
	if( CPedTargetting::DebugAcquaintanceScanner )
	{
		Color32 colour = (returnValue==ECanTarget) ? Color_green : Color_red;
		grcDebugDraw::Line( VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 1.0f), VEC3V_TO_VECTOR3(otherPed.GetTransform().GetPosition()), colour);
	}
#endif	

	if(returnValue == ECanTarget)
	{
		// If cops are hunting player in an unknown vehicle, the range is much tighter
		if( ped.PopTypeIsRandom() && ped.IsLawEnforcementPed() &&
			CPedGeometryAnalyser::IsPedInUnknownCar(otherPed) && !CPedGeometryAnalyser::CanPedSeePedInUnknownCar( ped, otherPed ))
		{
			returnValue = ECanNotTarget;
		}

		// If peds are hunting player in a bush, the range is much tighter
		if( !ped.PopTypeIsMission() && (iTargetFlags&TargetOption_UseFOVPerception) &&
			CPedGeometryAnalyser::IsPedInBush(otherPed) && !CPedGeometryAnalyser::CanPedSeePedInBush(ped, otherPed) )
		{
			returnValue = ECanNotTarget;
		}

		if( !ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanSeeUnderwaterPeds) &&
			CPedGeometryAnalyser::IsPedInWaterAtDepth(otherPed, CPedGeometryAnalyser::ms_MaxPedWaterDepthForVisibility) )
		{			
			returnValue = ECanNotTarget;
		}
	}

#if __BANK
	if (returnValue == ECanTarget)
	{
		wantedDebugf3("CPedAcquaintanceScanner::CanPedSeePed ped[%s] otherPed[%s]",ped.GetDebugName(),otherPed.GetDebugName());
	}
#endif

	return returnValue;
}

////////////////////////////////////////////////////////////////////////////////
// Scan Requests
////////////////////////////////////////////////////////////////////////////////

void CPedAcquaintanceScanner::RegisterScanRequest(CPed& rPed, int iAddTargetFlags, int iRemoveTargetFlags)
{
	const CScanRequest* pPedScanRequest = GetPedScanRequest(rPed);
	if (pPedScanRequest)
	{
		if (iAddTargetFlags != pPedScanRequest->GetAddTargetFlags() || iRemoveTargetFlags != pPedScanRequest->GetRemoveTargetFlags())
		{
			aiWarningf("PedAcquaintanceScanner already had a scan request for this ped and the new one was discarded. The new request had different flags though.");
		}

		return;
	}

	if (aiVerifyf(!IsScanRequestQueueFull(), "Not enough scan requests!"))
	{
		s32 iFreeRequestIdx = FindFreeScanRequestIdx(); 
		if (aiVerify(iFreeRequestIdx >= 0)) 
		{ 
			m_aScanRequests[iFreeRequestIdx].Set(rPed, iAddTargetFlags, iRemoveTargetFlags);
			++m_uNumScanRequests;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CPedAcquaintanceScanner::ClearAllScanRequests()
{
	if (m_uNumScanRequests == 0)
	{
		return;
	}

	for (int iRequestIdx = 0; iRequestIdx < uMAX_SCAN_REQUESTS; ++iRequestIdx)
	{
		m_aScanRequests[iRequestIdx].Clear();
	}

	m_uNumScanRequests = 0;
}

////////////////////////////////////////////////////////////////////////////////

void CPedAcquaintanceScanner::ClearScanRequest(int iIdx)
{
	CScanRequest& rRequest = m_aScanRequests[iIdx]; 
	if (aiVerify(rRequest.IsValid())) 
	{ 
		rRequest.Clear(); 

		aiAssert(m_uNumScanRequests > 0); 
		--m_uNumScanRequests;
	}
}

////////////////////////////////////////////////////////////////////////////////

int CPedAcquaintanceScanner::FindFreeScanRequestIdx() const
{
	for (int iRequestIdx = 0; iRequestIdx < uMAX_SCAN_REQUESTS; ++iRequestIdx)
	{
		if (!m_aScanRequests[iRequestIdx].IsValid())
		{
			return iRequestIdx;
		}
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////

int CPedAcquaintanceScanner::FindPedScanRequestIdx(const CPed& rPed) const
{
	for (int iRequestIdx = 0; iRequestIdx < uMAX_SCAN_REQUESTS; ++iRequestIdx)
	{
		if (m_aScanRequests[iRequestIdx].GetPed() == &rPed)
		{
			return iRequestIdx;
		}
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////


#if __BANK
void CPedAcquaintanceScanner::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Acquaintance Scanner");
	bank.AddToggle("Fix CheckLosForSoundEvents (ScanRequests) (B*2105791)", &CPedAcquaintanceScanner::ms_bFixCheckLoSForSoundEvents);
	bank.PopGroup(); // "Acquaintance Scanner"
}
#endif // __BANK


////////////////////////////
//PED AGITATION SCANNER
////////////////////////////

CPedAgitationScanner::CPedAgitationScanner()
: CExpensiveProcess(PPT_AgitationScanner)
, m_aTriggers()
, m_bBumped(false)
, m_bBumpedByVehicle(false)
, m_bDodged(false)
, m_bDodgedVehicle(false)
, m_bAmbientFriendBumped(false)
, m_bAmbientFriendBumpedByVehicle(false)
{
	//Register a slot for the expensive process.
	RegisterSlot();
}

CPedAgitationScanner::~CPedAgitationScanner()
{
	//Unregister the slot for the expensive process.
	UnregisterSlot();
	
	//Deactivate the triggers.
	DeactivateTriggers();
}

void CPedAgitationScanner::Scan(CPed& rPed, CEntityScannerIterator& UNUSED_PARAM(entityList))
{
	//No need to scan on the local player.
	if(rPed.IsLocalPlayer())
	{
		return;
	}
	
	//Grab the player.
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
	{
		return;
	}
	
	//Update the flags.
	UpdateFlags(rPed, *pPlayer);
	
	//Update the triggers.
	UpdateTriggers(rPed, *pPlayer);

#if __BANK
	RenderDebug(rPed);
#endif
}

void CPedAgitationScanner::ActivateTrigger(const CPed& rPed, const CPed& UNUSED_PARAM(rPlayer), const CAgitatedTrigger& rTrigger)
{
	//Find an inactive trigger.
	int iIndex = FindFirstInactiveTrigger();
	if(!aiVerify(iIndex >= 0))
	{
		return;
	}

	//Ensure the reaction is valid.
	atHashWithStringNotFinal hReaction = rTrigger.GetReaction();
	CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(hReaction);
	if(!aiVerifyf(pReaction, "The reaction: %s is invalid.", hReaction.GetCStr()))
	{
		return;
	}

	//Add a reaction.
	pReaction->AddReaction();

	//Grab the trigger.
	Trigger& rNewTrigger = m_aTriggers[iIndex];

	//Set the reaction.
	rNewTrigger.m_hReaction = hReaction;

	//Set the state.
	rNewTrigger.m_nState = Trigger::Start;

	//Set the chances.
	rNewTrigger.m_fChances = rTrigger.GetChances();

	//Set the distance.
	rNewTrigger.m_fDistance = GetDistance(rPed, rTrigger, *pReaction);

	//Set the original position.
	rNewTrigger.SetOriginalPosition(rPed.GetTransform().GetPosition());

	//Set the time in state.
	rNewTrigger.m_fTimeInState = 0.0f;

	//Clear the timers.
	rNewTrigger.m_fTimeBeforeReaction	= 0.0f;
	rNewTrigger.m_fTimeBeforeFinish		= 0.0f;

	//Clear the flags.
	rNewTrigger.m_uFlags.ClearAllFlags();
}

void CPedAgitationScanner::ActivateTriggers(CPed& rPed, CPed& rPlayer)
{
	//Ensure we can check the triggers.
	if(!CanActivateTriggers(rPed, rPlayer))
	{
		return;
	}

	//Load the reactions.
	static const int s_iMaxReactions = 8;
	atHashWithStringNotFinal aReactions[s_iMaxReactions];
	int iNumReactions = CAgitatedManager::GetInstance().GetTriggers().LoadReactions(aReactions, s_iMaxReactions);

	//Iterate over the reactions.
	for(int i = 0; i < iNumReactions; ++i)
	{
		//Ensure the trigger is valid.
		const CAgitatedTrigger* pTrigger = CAgitatedManager::GetInstance().GetTriggers().GetTrigger(rPed, aReactions[i]);
		if(!pTrigger)
		{
			continue;
		}

		//Ensure the trigger can be activated.
		if(!CanActivateTrigger(rPed, rPlayer, *pTrigger))
		{
			continue;
		}

		//Activate the trigger.
		ActivateTrigger(rPed, rPlayer, *pTrigger);
	}
}

bool CPedAgitationScanner::AreExpensiveFlagsValid(const CPed& rPed, const CPed& rPlayer, const CAgitatedTriggerReaction& rReaction, bool bIsActive) const
{
	//Check if a line of sight is needed.
	bool bNoLineOfSightNeeded = rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::NoLineOfSightNeeded);
	bool bLineOfSightNeeded = !bNoLineOfSightNeeded;
	if(bLineOfSightNeeded)
	{
		//Ensure the ped can see the player.
		s32 iTargetFlags = TargetOption_UseFOVPerception;
		if(!bIsActive)
		{
			if(!CPedGeometryAnalyser::CanPedTargetPed(rPed, rPlayer, iTargetFlags))
			{
				return false;
			}
		}
		else
		{
			if(CPedGeometryAnalyser::CanPedTargetPedAsync(const_cast<CPed &>(rPed), const_cast<CPed &>(rPlayer), iTargetFlags) == ECanNotTarget)
			{
				return false;
			}
		}
	}

	return true;
}

bool CPedAgitationScanner::AreFlagsValid(const CPed& rPed, const CPed& rPlayer, const CAgitatedTriggerReaction& rReaction, bool UNUSED_PARAM(bIsActive)) const
{
	//Check if we should be disabled when agitated.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::DisableWhenAgitated))
	{
		//Ensure the ped is not agitated,
		if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
		{
			return false;
		}
	}

	//Check if the trigger reaction should be disabled if a friendly is agitated.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::DisableWhenFriendlyIsAgitated))
	{
		//Ensure a friendly is not agitated.
		if(IsFriendlyAgitated(rPed, rPlayer))
		{
			return false;
		}
	}

	//Check if we must be using a scenario.
	bool bMustBeUsingScenario						= rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeUsingScenario);
	bool bMustBeUsingScenarioBeforeInitialReaction	= rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeUsingScenarioBeforeInitialReaction);
	bool bMustBeUsingATerritorialScenario			= rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeUsingATerritorialScenario);
	bMustBeUsingScenario = (bMustBeUsingScenario || (bMustBeUsingScenarioBeforeInitialReaction && IsBeforeInitialReaction(rReaction)));
	if(bMustBeUsingScenario)
	{
		//Ensure we are using a scenario.
		if(!(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO, true) ||
			rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDERING_SCENARIO, true) ||
			rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_VEHICLE_SCENARIO, true)))
		{
			return false;
		}
	}

	//Check if the ped must be using a scenario point marked as "Territorial" to be a valid trigger.
	if (bMustBeUsingATerritorialScenario)
	{
		//Grab the point off the ped.
		CTask* pActiveTask = rPed.GetPedIntelligence()->GetTaskActive();

		if (!pActiveTask)
		{
			return false;
		}

		CScenarioPoint* pPoint = pActiveTask->GetScenarioPoint();
		
		//Check the flag.
		if (!pPoint || !pPoint->IsFlagSet(CScenarioPointFlags::TerritorialScenario))
		{
			return false;
		}
	}

	//Check if we must be wandering.
	bool bMustBeWandering						= rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeWandering);
	bool bMustBeWanderingBeforeInitialReaction	= rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeWanderingBeforeInitialReaction);
	bMustBeWandering = (bMustBeWandering || (bMustBeWanderingBeforeInitialReaction && IsBeforeInitialReaction(rReaction)));
	if(bMustBeWandering)
	{
		//Ensure we are wandering.
		if(!rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDER, true))
		{
			return false;
		}
	}

	//Check if we should be disabled when wandering.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::DisableWhenWandering))
	{
		//Ensure we are not wandering.
		if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDER, true))
		{
			return false;
		}
	}

	//Check if we should be disabled when still.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::DisableWhenStill))
	{
		//Ensure the ped is not still.
		if(rPed.GetMotionData()->GetIsStill())
		{
			return false;
		}
	}

	//Check if we should be disabled if neither ped is on foot.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::DisableIfNeitherPedIsOnFoot))
	{
		//Ensure one of the peds are on foot.
		if(!rPed.GetIsOnFoot() && !rPlayer.GetIsOnFoot())
		{
			return false;
		}
	}

	//Check if we must be on foot.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeOnFoot))
	{
		//Ensure we are on foot.
		if(!rPed.GetIsOnFoot())
		{
			return false;
		}
	}

	//Check if the target must be on foot.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::TargetMustBeOnFoot))
	{
		//Ensure the target is on foot.
		if(!rPlayer.GetIsOnFoot())
		{
			return false;
		}
	}

	//Check if we must be in a vehicle.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeInVehicle))
	{
		//Ensure we are in a vehicle.
		if(!rPed.GetIsInVehicle())
		{
			return false;
		}
	}

	//Check if we must be stationary.
	if(rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::MustBeStationary))
	{
		//Ensure we are stationary.
		float fSpeedSq = rPed.GetVelocity().XYMag2();
		static dev_float s_fMaxSpeed = 0.15f;
		float fMaxSpeedSq = square(s_fMaxSpeed);
		if(fSpeedSq > fMaxSpeedSq)
		{
			return false;
		}
	}

	return true;
}

float CPedAgitationScanner::CalculateDistanceSqBetweenPedAndVehicle(const CPed& rPed, const CVehicle& rVehicle) const
{
	//Transform the ped's position into the local space of the vehicle.
	Vec3V vLocalPedPosition = rVehicle.GetTransform().UnTransform(rPed.GetTransform().GetPosition());

	return rVehicle.GetBaseModelInfo()->GetBoundingBox().DistanceToPointSquared(vLocalPedPosition).Getf();
}

float CPedAgitationScanner::CalculateDistanceSqBetweenPeds(const CPed& rPed, const CPed& rPlayer) const
{
	//Transform the player's position into the local space of the ped.
	Vec3V vLocalPlayerPosition = rPed.GetTransform().UnTransform(rPlayer.GetTransform().GetPosition());

	return (rPed.GetBaseModelInfo()->GetBoundingBox().DistanceToPointSquared(vLocalPlayerPosition).Getf());
}

bool CPedAgitationScanner::CanActivateTrigger(const CPed& rPed, const CPed& rPlayer, const CAgitatedTrigger& rTrigger) const
{
	//Ensure we have an inactive trigger.
	if(!HasInactiveTrigger())
	{
		return false;
	}

	//Ensure the trigger is not active.
	if(IsTriggerActive(rTrigger))
	{
		return false;
	}

	//Ensure the ped type is valid.
	if(!IsPedTypeValid(rPlayer, rTrigger))
	{
		return false;
	}

	//Ensure the reaction is valid.
	CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(rTrigger.GetReaction());
	if(!pReaction)
	{
		return false;
	}

	//Ensure we have not reached the max reactions.
	if(pReaction->HasReachedMaxReactions())
	{
		return false;
	}

	//Ensure the flags are valid.
	if(!AreFlagsValid(rPed, rPlayer, *pReaction, false))
	{
		return false;
	}

	//Ensure the distance is valid.
	if(!IsDistanceValid(rPed, rPlayer, rTrigger, *pReaction))
	{
		return false;
	}

	//Ensure the expensive flags are valid.
	if(!AreExpensiveFlagsValid(rPed, rPlayer, *pReaction, false))
	{
		return false;
	}

	//Ensure the dot is valid.
	if(!IsDotValid(rPed, rPlayer, pReaction->GetMinDotToTarget(), pReaction->GetMaxDotToTarget()))
	{
		return false;
	}

	//Ensure the target speed is valid.
	if(!IsSpeedValid(rPlayer, pReaction->GetMinTargetSpeed(), pReaction->GetMaxTargetSpeed()))
	{
		return false;
	}

	//Ensure the type is valid.
	if(!IsTypeValid(rPed, pReaction->GetType()))
	{
		return false;
	}

	return true;
}

bool CPedAgitationScanner::CanActivateTriggers(const CPed& rPed, const CPed& rPlayer)
{
	//Check if we should not ignore time slicing.
	if(!ShouldIgnoreTimeSlicing(rPed, rPlayer))
	{
		//Ensure the processing should happen this frame.
		if(!ShouldBeProcessedThisFrame())
		{
			return false;
		}
	}
	
	//Ensure the ped is ambient.
 	if(!rPed.PopTypeIsRandom())
	{
 		return false;
 	}

	//Ensure that either the player is not wanted or the scanning ped is an animal.
	if(rPlayer.GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN && rPed.GetPedType() != PEDTYPE_ANIMAL)
	{
		return false;
	}

	//Ensure the ped is not friendly with the player.
	if(rPed.GetPedIntelligence()->IsFriendlyWith(rPlayer))
	{
		return false;
	}

	//Ensure the ped does not ignore the player.
	if(rPed.GetPedIntelligence()->Ignores(rPlayer))
	{
		return false;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pPlayerInfo = rPlayer.GetPlayerInfo();
	if(!pPlayerInfo)
	{
		return false;
	}

	//Ensure the player's controls are enabled.
	if(pPlayerInfo->AreControlsDisabled())
	{
		return false;
	}

	//Ensure a cutscene is not running.
	if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
	{
		return false;
	}

	//Ensure we have an inactive trigger.
	if(!HasInactiveTrigger())
	{
		return false;
	}
	
	return true;
}

bool CPedAgitationScanner::CanUpdateTriggers(const CPed& rPed, const CPed& rPlayer) const
{
	//Ensure the ped is not dead.
	if(rPed.IsInjured())
	{
		return false;
	}

	//Ensure the player is not dead.
	if(rPlayer.IsInjured())
	{
		return false;
	}

	//Ensure the ped has not disabled agitation.
	if(rPed.GetPedResetFlag(CPED_RESET_FLAG_DisableAgitation))
	{
		return false;
	}

	//Ensure the ped has not disabled agitation triggers.
	if(rPed.GetPedResetFlag(CPED_RESET_FLAG_DisableAgitationTriggers))
	{
		return false;
	}

	//Ensure the player has not disabled agitation.
	if(rPlayer.GetPedResetFlag(CPED_RESET_FLAG_DisableAgitation))
	{
		return false;
	}

	//Ensure the player has not disabled agitation triggers.
	if(rPlayer.GetPedResetFlag(CPED_RESET_FLAG_DisableAgitationTriggers))
	{
		return false;
	}

	//Ensure the player has not disabled agitation.
	if(rPlayer.GetPlayerInfo() && rPlayer.GetPlayerInfo()->GetDisableAgitation())
	{
		return false;
	}

	//Ensure the ped should not flee the player.
	if(ShouldPedFleePlayer(rPed, rPlayer))
	{
		return false;
	}
	
	return true;
}

void CPedAgitationScanner::ClearAgitation(CPed& rPed, ePedConfigFlags nFlag, bool& bStorage)
{
	//Clear the flag.
	rPed.SetPedConfigFlag(nFlag, false);

	//Clear the storage.
	bStorage = false;
}

void CPedAgitationScanner::DeactivateTrigger(Trigger& rTrigger)
{
	//Ensure the trigger is active.
	if(!aiVerify(rTrigger.IsActive()))
	{
		return;
	}

	//Ensure the reaction is valid.
	CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(rTrigger.m_hReaction);
	if(!pReaction)
	{
		return;
	}

	//Remove a reaction.
	pReaction->RemoveReaction();

	//Clear the reaction.
	rTrigger.m_hReaction.Clear();

	//Clear the state.
	rTrigger.m_nState = Trigger::None;

	//Clear the chances.
	rTrigger.m_fChances = 0.0f;

	//Clear the distance.
	rTrigger.m_fDistance = 0.0f;

	//Clear the original position.
	rTrigger.SetOriginalPosition(Vec3V(V_ZERO));

	//Clear the time in state.
	rTrigger.m_fTimeInState = 0.0f;

	//Clear the timers.
	rTrigger.m_fTimeBeforeReaction	= 0.0f;
	rTrigger.m_fTimeBeforeFinish	= 0.0f;

	//Clear the flags.
	rTrigger.m_uFlags.ClearAllFlags();
}

void CPedAgitationScanner::DeactivateTriggers()
{
	//Iterate over the triggers.
	int iCount = m_aTriggers.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the trigger is active.
		Trigger& rTrigger = m_aTriggers[i];
		if(!rTrigger.IsActive())
		{
			continue;
		}

		//Deactivate the trigger.
		DeactivateTrigger(rTrigger);
	}
}

int CPedAgitationScanner::FindFirstActiveTriggerWithType(AgitatedType nType) const
{
	//Iterate over the triggers.
	int iCount = m_aTriggers.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the trigger is active.
		const Trigger& rTrigger = m_aTriggers[i];
		if(!rTrigger.IsActive())
		{
			continue;
		}

		//Ensure the reaction is valid.
		const CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(rTrigger.m_hReaction);
		if(!pReaction)
		{
			continue;
		}

		//Ensure the type matches.
		if(nType != pReaction->GetType())
		{
			continue;
		}

		return i;
	}

	return -1;
}

int CPedAgitationScanner::FindFirstInactiveTrigger() const
{
	//Iterate over the triggers.
	int iCount = m_aTriggers.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the trigger is inactive.
		if(m_aTriggers[i].IsActive())
		{
			continue;
		}

		return i;
	}

	return -1;
}

float CPedAgitationScanner::GetDistance(const CPed& rPed, const CAgitatedTrigger& rTrigger, const CAgitatedTriggerReaction& rReaction) const
{
	//Grab the distance.
	float fDistance = rTrigger.GetDistance();

	//Check if we should use the distance from the scenario.
	bool bUseDistanceFromScenario = rReaction.GetFlags().IsFlagSet(CAgitatedTriggerReaction::UseDistanceFromScenario);
	if(bUseDistanceFromScenario)
	{
		//Check if the scenario point is valid.
		const CScenarioPoint* pScenarioPoint = rPed.GetScenarioPoint(rPed, true);
		if(pScenarioPoint)
		{
			//Set the distance.
			fDistance = pScenarioPoint->GetRadius();
		}
	}

	return fDistance;
}

bool CPedAgitationScanner::HasInactiveTrigger() const
{
	//Ensure we have an inactive trigger.
	int iIndex = FindFirstInactiveTrigger();
	if(iIndex < 0)
	{
		return false;
	}

	return true;
}

bool CPedAgitationScanner::IsBeforeInitialReaction(const CAgitatedTriggerReaction& rReaction) const
{
	//Check if there is no trigger active for the type.
	int iIndex = FindFirstActiveTriggerWithType(rReaction.GetType());
	if(iIndex < 0)
	{
		return true;
	}

	//Check if the trigger has not reacted.
	if(!m_aTriggers[iIndex].m_uFlags.IsFlagSet(Trigger::HasReacted))
	{
		return true;
	}

	return false;
}

bool CPedAgitationScanner::IsDistanceValid(const CPed& rPed, const CPed& rPlayer, const CAgitatedTrigger& rTrigger, const CAgitatedTriggerReaction& rReaction) const
{
	//Get the distance.
	float fDistance = GetDistance(rPed, rTrigger, rReaction);

	return IsDistanceValid(rPed, rPlayer, fDistance);
}

bool CPedAgitationScanner::IsDistanceValid(const CPed& rPed, const CPed& rPlayer, float fDistance) const
{
	//Ensure the distance is valid.
	if(fDistance <= 0.0f)
	{
		return false;
	}

	//Comparing against distance squared.
	fDistance *= fDistance;

	//Calculate the distance between the peds.
	float fDistanceBetweenPeds = FLT_MAX;
	const CVehicle* pPedVehicle		= rPed.GetVehiclePedInside();
	const CVehicle* pPlayerVehicle	= rPlayer.GetVehiclePedInside();
	if(pPedVehicle && !pPlayerVehicle)
	{
		fDistanceBetweenPeds = CalculateDistanceSqBetweenPedAndVehicle(rPlayer, *pPedVehicle);
	}
	else if(!pPedVehicle && pPlayerVehicle)
	{
		fDistanceBetweenPeds = CalculateDistanceSqBetweenPedAndVehicle(rPed, *pPlayerVehicle);
	}
	else
	{
		fDistanceBetweenPeds = CalculateDistanceSqBetweenPeds(rPed, rPlayer);
	}

	return (fDistanceBetweenPeds <= fDistance);
}

bool CPedAgitationScanner::IsDotValid(const CPed& rPed, const CPed& rPlayer, float fMin, float fMax) const
{
	//Check if we care about the dot to the target.
	float fMinDotToTarget = fMin;
	float fMaxDotToTarget = fMax;
	bool bHasMinDotToTarget = (fMinDotToTarget > -1.0f);
	bool bHasMaxDotToTarget = (fMaxDotToTarget < 1.0f);
	if(bHasMinDotToTarget || bHasMaxDotToTarget)
	{
		//Grab the ped forward.
		Vec3V vPedForward = rPed.GetTransform().GetForward();

		//Grab the direction from the ped to the target.
		Vec3V vPedToTarget = Subtract(rPlayer.GetTransform().GetPosition(), rPed.GetTransform().GetPosition());
		// Vec3V vPedToTargetDirection = NormalizeFastSafe(vPedToTarget, vPedForward);

		//Calculate the dot to the target.
		ScalarV scDotToTarget = Dot(vPedForward, vPedToTarget);
		scDotToTarget = Clamp(scDotToTarget, ScalarV(V_NEGONE), ScalarV(V_ONE));

		//Check if we have a min dot to the target.
		if(bHasMinDotToTarget)
		{
			//Ensure the dot is valid.
			if(IsLessThanAll(scDotToTarget, ScalarVFromF32(fMinDotToTarget)))
			{
				return false;
			}
		}

		//Check if we have a max dot to the target.
		if(bHasMaxDotToTarget)
		{
			//Ensure the dot is valid.
			if(IsGreaterThanAll(scDotToTarget, ScalarVFromF32(fMaxDotToTarget)))
			{
				return false;
			}
		}
	}

	return true;
}

bool CPedAgitationScanner::IsFriendlyAgitated(const CPed& rPed, const CPed& UNUSED_PARAM(rPlayer)) const
{
	//Scan the nearby peds.
	const CEntityScannerIterator entityList = rPed.GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pOtherEntity = entityList.GetFirst(); pOtherEntity; pOtherEntity = entityList.GetNext())
	{
		//Grab the other ped.
		const CPed* pOtherPed = static_cast<const CPed *>(pOtherEntity);

		//Ensure the other ped is not the ped.
		if(pOtherPed == &rPed)
		{
			continue;
		}

		//Ensure the peds are friendly.
		if(!rPed.GetPedIntelligence()->IsFriendlyWith(*pOtherPed))
		{
			continue;
		}

		//Ensure the other ped is agitated.
		const CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pOtherPed);
		if(!pTask)
		{
			continue;
		}

		//Ensure the ped is not just reacting to our own agitation.
		if(&rPed == pTask->GetLeader())
		{
			continue;
		}

		return true;
	}

	return false;
}

bool CPedAgitationScanner::IsPedTypeValid(const CPed& rPlayer, const CAgitatedTrigger& rTrigger) const
{
	//Ensure the ped type matches.
	if(!rTrigger.HasPedType(rPlayer.GetPedType()))
	{
		return false;
	}

	return true;
}

bool CPedAgitationScanner::IsSpeedValid(const CPed& rPed, float fMin, float fMax) const
{
	//Check if we care about the speed.
	bool bHasMin = (fMin > 0.0f);
	bool bHasMax = (fMax > 0.0f);
	if(bHasMin || bHasMax)
	{
		//Grab the speed.
		float fSpeedSq = rPed.GetVelocity().XYMag2();

		//Check if we have a min speed.
		if(bHasMin)
		{
			//Ensure the speed is valid.
			if(fSpeedSq < square(fMin))
			{
				return false;
			}
		}

		//Check if we have a max speed.
		if(bHasMax)
		{
			//Ensure the speed is valid.
			if(fSpeedSq > square(fMax))
			{
				return false;
			}
		}
	}

	return true;
}

bool CPedAgitationScanner::IsTriggerActive(const CAgitatedTrigger& rTrigger) const
{
	//Ensure the reaction is valid.
	const CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(rTrigger.GetReaction());
	if(!pReaction)
	{
		return false;
	}

	//Ensure the trigger is valid.
	int iIndex = FindFirstActiveTriggerWithType(pReaction->GetType());
	if(iIndex < 0)
	{
		return false;
	}

	return true;
}

bool CPedAgitationScanner::IsTriggerStillValid(const CPed& rPed, const CPed& rPlayer, const Trigger& rTrigger) const
{
	//Assert that the trigger is active.
	aiAssert(rTrigger.IsActive());

	//Ensure the reaction is valid.
	const CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(rTrigger.m_hReaction);
	if(!pReaction)
	{
		return false;
	}

	//Ensure the flags are valid.
	if(!AreFlagsValid(rPed, rPlayer, *pReaction, true))
	{
		return false;
	}

	//Ensure the distance is valid.
	if(!IsDistanceValid(rPed, rPlayer, rTrigger.m_fDistance))
	{
		return false;
	}

	//Ensure the expensive flags are valid.
	if(!AreExpensiveFlagsValid(rPed, rPlayer, *pReaction, true))
	{
		return false;
	}

	//Ensure the dot is valid.
	if(!IsDotValid(rPed, rPlayer, pReaction->GetMinDotToTarget(), pReaction->GetMaxDotToTarget()))
	{
		return false;
	}

	//Ensure the target speed is valid.
	if(!IsSpeedValid(rPlayer, pReaction->GetMinTargetSpeed(), pReaction->GetMaxTargetSpeed()))
	{
		return false;
	}

	return true;
}

bool CPedAgitationScanner::IsTypeValid(const CPed& rPed, AgitatedType nType) const
{
	//Check the type.
	switch(nType)
	{
		case AT_Loitering:
		{
			//Ensure the active task is valid.
			CTask* pTask = rPed.GetPedIntelligence()->GetTaskActive();
			if(!pTask)
			{
				break;
			}

			//Ensure the point is valid.
			const CScenarioPoint* pPoint = pTask->GetScenarioPoint();
			if(!pPoint)
			{
				break;
			}

			//Check if the point ignores loitering.
			if(pPoint->IsFlagSet(CScenarioPointFlags::IgnoreLoitering))
			{
				return false;
			}
		}
		default:
		{
			break;
		}
	}

	return true;
}

bool CPedAgitationScanner::ShouldIgnoreTimeSlicing(const CPed& rPed, const CPed& rPlayer) const
{
	//Check if the distance is within the threshold.
	ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), rPlayer.GetTransform().GetPosition());
	static dev_float s_fMaxDistance = 3.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsLessThanAll(scDistSq, scMaxDistSq))
	{
		return true;
	}

	return false;
}

bool CPedAgitationScanner::ShouldPedFleePlayer(const CPed& rPed, const CPed& rPlayer) const
{
	//Check if the ped is random.
	if(rPed.PopTypeIsRandom())
	{
		//Check if the player has a wanted structure.
		const CWanted* pWanted = rPlayer.GetPlayerWanted();
		if(pWanted)
		{
			//Check if all randoms should flee the player.
			if(pWanted->m_AllRandomsFlee)
			{
				return true;
			}
			
			//Check if all neutral randoms should flee the player.
			if(pWanted->m_AllNeutralRandomsFlee)
			{
				//Check if the ped and the player have a neutral (or better) relationship.
				if(!rPed.GetPedIntelligence()->IsThreatenedBy(rPlayer))
				{
					return true;
				}
			}

			if(pWanted->m_AllRandomsOnlyAttackWithGuns)
			{
				const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
				const CWeaponInfo* pBestWeaponInfo = pWeaponManager ? pWeaponManager->GetBestWeaponInfo() : NULL;
				if(!pBestWeaponInfo || !pBestWeaponInfo->GetIsGun())
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

void CPedAgitationScanner::UpdateAgitation(CPed& ped, CPed& player, ePedConfigFlags nFlag, AgitatedType nType, bool& bStorage) const
{
	//Check if the flag is set.
	if(ped.GetPedConfigFlag(nFlag))
	{
		//Clear the flag.
		ped.SetPedConfigFlag(nFlag, false);

		//Set the storage.
		bStorage = true;

		//Delay a frame (to allow ragdoll, etc to kick in).
		return;
	}
	else if(!bStorage)
	{
		return;
	}

	//Ensure the ped is not using a ragdoll.
	if(ped.GetUsingRagdoll())
	{
		return;
	}

	//Ensure we are not falling over.
	if(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FALL_OVER))
	{
		return;
	}

	//Ensure the current event type is valid.
	const s32 iCurrentEventType = ped.GetPedIntelligence()->GetCurrentEventType();
	switch(iCurrentEventType)
	{
		case EVENT_CLIMB_LADDER_ON_ROUTE:
		case EVENT_CLIMB_NAVMESH_ON_ROUTE:
		case EVENT_GET_OUT_OF_WATER:
		case EVENT_SWITCH_2_NM_TASK:
		{
			return;
		}
		case EVENT_PED_COLLISION_WITH_PED:
		case EVENT_PED_COLLISION_WITH_PLAYER:
		case EVENT_POTENTIAL_BE_WALKED_INTO:
		case EVENT_POTENTIAL_GET_RUN_OVER:
		case EVENT_VEHICLE_COLLISION:
		{
			//Allow the event through when we start to get up (if ever).
			//Make sure get up has been running for a bit of time, since
			//it's sometimes used when we don't actually need to get up.
			CTask* pTask = ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP);
			if(pTask && (pTask->GetTimeRunning() > 0.25f))
			{
				break;
			}

			return;
		}
		default:
		{
			break;
		}
	}

	//Ensure we are not in the water, in the air, or climbing.
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming) ||
		ped.GetIsInWater() || ped.GetWasInWater() || 
		ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) ||
		ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER, true) ||
		ped.GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning))
	{
		return;
	}

	//Clear the storage.
	bStorage = false;

	//Add the agitation event.
	CEventAgitated event(&player, nType);
	ped.GetPedIntelligence()->AddEvent(event);
}

void CPedAgitationScanner::UpdateFlags(CPed& rPed, CPed& rPlayer)
{
	//Ensure the ped is not dead.
	if(rPed.IsInjured())
	{
		return;
	}

	//Ensure the player is not dead.
	if(rPlayer.IsInjured())
	{
		return;
	}

	//If the player has been bumped by a ped, disregard the fact that they tried to dodge it.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_BumpedByPlayer) || m_bBumped)
	{
		ClearAgitation(rPed, CPED_CONFIG_FLAG_DodgedPlayer, m_bDodged);
	}

	//If the player has been bumped by a vehicle, disregard the fact that they tried to dodge it.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_BumpedByPlayerVehicle) || m_bBumpedByVehicle)
	{
		ClearAgitation(rPed, CPED_CONFIG_FLAG_DodgedPlayerVehicle, m_bDodgedVehicle);
	}

	//If the player is entering a vehicle, ignore bumped & dodged.
	if(rPlayer.GetIsEnteringVehicle())
	{
		ClearAgitation(rPed, CPED_CONFIG_FLAG_BumpedByPlayer,	m_bBumped);
		ClearAgitation(rPed, CPED_CONFIG_FLAG_DodgedPlayer,		m_bDodged);
		ClearAgitation(rPed, CPED_CONFIG_FLAG_AmbientFriendBumpedByPlayer, m_bAmbientFriendBumped);
	}

	//Update the agitation.
	UpdateAgitation(rPed, rPlayer, CPED_CONFIG_FLAG_BumpedByPlayer,			AT_Bumped,			m_bBumped);
	UpdateAgitation(rPed, rPlayer, CPED_CONFIG_FLAG_BumpedByPlayerVehicle,	AT_BumpedByVehicle,	m_bBumpedByVehicle);
	UpdateAgitation(rPed, rPlayer, CPED_CONFIG_FLAG_DodgedPlayer,			AT_Dodged,			m_bDodged);
	UpdateAgitation(rPed, rPlayer, CPED_CONFIG_FLAG_DodgedPlayerVehicle,	AT_DodgedVehicle,	m_bDodgedVehicle);
	UpdateAgitation(rPed, rPlayer, CPED_CONFIG_FLAG_AmbientFriendBumpedByPlayer,		AT_Bumped,			m_bAmbientFriendBumped);
	UpdateAgitation(rPed, rPlayer, CPED_CONFIG_FLAG_AmbientFriendBumpedByPlayerVehicle,	AT_BumpedByVehicle,	m_bAmbientFriendBumpedByVehicle);
}

void CPedAgitationScanner::UpdateTrigger(CPed& rPed, CPed& rPlayer, Trigger& rTrigger)
{
	//Assert that the trigger is active.
	aiAssert(rTrigger.IsActive());

	//Ensure the reaction is valid.
	const CAgitatedTriggerReaction* pReaction = CAgitatedManager::GetInstance().GetTriggers().GetReaction(rTrigger.m_hReaction);
	if(!pReaction)
	{
		//Deactivate the trigger.
		DeactivateTrigger(rTrigger);
		return;
	}

	//Keep track of the states.
	Trigger::State nCurrentState = rTrigger.m_nState;
	Trigger::State nNextState = nCurrentState;

	//Check if the trigger is still valid.
	bool bDoWeCareIfTriggerIsStillValid = (nCurrentState != Trigger::WaitBeforeFinish);
	bool bIsTriggerStillValid = bDoWeCareIfTriggerIsStillValid && IsTriggerStillValid(rPed, rPlayer, rTrigger);

	//Check the current state.
	switch(nCurrentState)
	{
		case Trigger::Start:
		{
			//Check the chances to react initially.
			float fChancesToReactInitially = rTrigger.m_fChances;
			if(fChancesToReactInitially > 0.0f)
			{
				//Check if we should react.
				float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
				if(fRandom <= fChancesToReactInitially)
				{
					//Calculate the time before the initial reaction.
					float fMinTimeBeforeInitialReaction = pReaction->GetTimeBeforeInitialReaction().m_Min;
					float fMaxTimeBeforeInitialReaction = pReaction->GetTimeBeforeInitialReaction().m_Max;
					float fTimeBeforeInitialReaction = (fMaxTimeBeforeInitialReaction > 0.0f) ? fwRandom::GetRandomNumberInRange(fMinTimeBeforeInitialReaction, fMaxTimeBeforeInitialReaction) : 0.0f;

					//Check if the time before initial reaction is valid.
					if(fTimeBeforeInitialReaction > 0.0f)
					{
						//Set the time before reaction.
						rTrigger.m_fTimeBeforeReaction = fTimeBeforeInitialReaction;

						//Wait before the reaction.
						nNextState = Trigger::WaitBeforeReaction;
					}
					else
					{
						//React.
						nNextState = Trigger::React;
					}
				}
				else
				{
					//Calculate the time after an initial reaction failure.
					float fMinTimeAfterInitialReactionFailure = pReaction->GetTimeAfterInitialReactionFailure().m_Min;
					float fMaxTimeAfterInitialReactionFailure = pReaction->GetTimeAfterInitialReactionFailure().m_Max;
					float fTimeAfterInitialReactionFailure = (fMaxTimeAfterInitialReactionFailure > 0.0f) ? fwRandom::GetRandomNumberInRange(fMinTimeAfterInitialReactionFailure, fMaxTimeAfterInitialReactionFailure) : 0.0f;

					//Check if the time after an initial reaction failure is valid.
					if(fTimeAfterInitialReactionFailure > 0.0f)
					{
						//Set the time to wait before finish.
						rTrigger.m_fTimeBeforeFinish = fTimeAfterInitialReactionFailure;

						//Wait before we finish.
						nNextState = Trigger::WaitBeforeFinish;
					}
					else
					{
						//Move to the finish state.
						nNextState = Trigger::Finish;
					}
				}
			}

			break;
		}
		case Trigger::WaitBeforeReaction:
		{
			//Check if the trigger is still valid.
			if(bIsTriggerStillValid)
			{
				//Check if we are agitated.
				const CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(rPed);
				if(pTask)
				{
					//Sync the time in state to the time since the audio ended.
					//This allows us to have more consistent timing in between reactions.
					rTrigger.m_fTimeInState = pTask->GetTimeSinceAudioEndedForState();
				}

				//Check if the time in state has exceeded the threshold.
				float fTimeBeforeReaction = rTrigger.m_fTimeBeforeReaction;
				if(rTrigger.m_fTimeInState > fTimeBeforeReaction)
				{
					//React.
					nNextState = Trigger::React;
				}
			}
			else
			{
				//Check if we have reacted.
				if(rTrigger.m_uFlags.IsFlagSet(Trigger::HasReacted))
				{
					//Calculate the time after the last successful reaction.
					float fMinTimeAfterLastSuccessfulReaction = pReaction->GetTimeAfterLastSuccessfulReaction().m_Min;
					float fMaxTimeAfterLastSuccessfulReaction = pReaction->GetTimeAfterLastSuccessfulReaction().m_Max;
					float fTimeAfterLastSuccessfulReaction = (fMaxTimeAfterLastSuccessfulReaction > 0.0f) ? fwRandom::GetRandomNumberInRange(fMinTimeAfterLastSuccessfulReaction, fMaxTimeAfterLastSuccessfulReaction) : 0.0f;

					//Check if the time after the last successful reaction is valid.
					if(fTimeAfterLastSuccessfulReaction > 0.0f)
					{
						//Set the time before finish.
						rTrigger.m_fTimeBeforeFinish = fTimeAfterLastSuccessfulReaction;

						//Wait before finish.
						nNextState = Trigger::WaitBeforeFinish;
					}
					else
					{
						//Move to the finish state.
						nNextState = Trigger::Finish;
					}
				}
				else
				{
					//Move to the finish state.
					nNextState = Trigger::Finish;
				}
			}

			break;
		}
		case Trigger::React:
		{
			//Check if the trigger is still valid.
			if(bIsTriggerStillValid)
			{
				//Note that we have reacted.
				rTrigger.m_uFlags.SetFlag(Trigger::HasReacted);

				//Add the agitation event.
				CEventAgitated event(&rPlayer, pReaction->GetType());
				rPed.GetPedIntelligence()->AddEvent(event);

				//Calculate the time between escalating reactions.
				float fMinTimeBetweenEscalatingReactions = pReaction->GetTimeBetweenEscalatingReactions().m_Min;
				float fMaxTimeBetweenEscalatingReactions = pReaction->GetTimeBetweenEscalatingReactions().m_Max;
				float fTimeBetweenEscalatingReactions = (fMaxTimeBetweenEscalatingReactions > 0.0f) ? fwRandom::GetRandomNumberInRange(fMinTimeBetweenEscalatingReactions, fMaxTimeBetweenEscalatingReactions) : 0.0f;

				//Check if the time between escalating reactions is valid.
				if(fTimeBetweenEscalatingReactions > 0.0f)
				{
					//Set the time before reaction.
					rTrigger.m_fTimeBeforeReaction = fTimeBetweenEscalatingReactions;

					//Wait before the reaction.
					nNextState = Trigger::WaitBeforeReaction;
				}
				else
				{
					//Calculate the time after the last successful reaction.
					float fMinTimeAfterLastSuccessfulReaction = pReaction->GetTimeAfterLastSuccessfulReaction().m_Min;
					float fMaxTimeAfterLastSuccessfulReaction = pReaction->GetTimeAfterLastSuccessfulReaction().m_Max;
					float fTimeAfterLastSuccessfulReaction = (fMaxTimeAfterLastSuccessfulReaction > 0.0f) ? fwRandom::GetRandomNumberInRange(fMinTimeAfterLastSuccessfulReaction, fMaxTimeAfterLastSuccessfulReaction) : 0.0f;

					//Check if the time after the last successful reaction is valid.
					if(fTimeAfterLastSuccessfulReaction > 0.0f)
					{
						//Set the time before finish.
						rTrigger.m_fTimeBeforeFinish = fTimeAfterLastSuccessfulReaction;

						//Wait before finish.
						nNextState = Trigger::WaitBeforeFinish;
					}
					else
					{
						//Move to the finish state.
						nNextState = Trigger::Finish;
					}
				}
			}
			else
			{
				//Move to the finish state.
				nNextState = Trigger::Finish;
			}

			break;
		}
		case Trigger::WaitBeforeFinish:
		{
			//Check if the time in state has exceeded the threshold.
			float fTimeBeforeFinish = rTrigger.m_fTimeBeforeFinish;
			if(rTrigger.m_fTimeInState > fTimeBeforeFinish)
			{
				//Move to the finish state.
				nNextState = Trigger::Finish;
			}

			break;
		}
		case Trigger::Finish:
		{
			//Deactivate the trigger.
			DeactivateTrigger(rTrigger);
			return;
		}
		case Trigger::None:
		default:
		{
			aiAssertf(false, "The state is invalid: %d.", rTrigger.m_nState);
			return;
		}
	}

	//Check if we are in the same state.
	if(nCurrentState == nNextState)
	{
		//Increase the time in state.
		rTrigger.m_fTimeInState += fwTimer::GetTimeStep();
	}
	else
	{
		//Set the state.
		rTrigger.m_nState = nNextState;

		//Clear the time in state.
		rTrigger.m_fTimeInState = 0.0f;
	}
}

void CPedAgitationScanner::UpdateTriggers(CPed& rPed, CPed& rPlayer)
{
	//Ensure we can update triggers.
	if(!CanUpdateTriggers(rPed, rPlayer))
	{
		//Deactivate the triggers.
		DeactivateTriggers();
		return;
	}

	//Activate the triggers.
	ActivateTriggers(rPed, rPlayer);

	//Iterate over the triggers.
	int iCount = m_aTriggers.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the trigger is active.
		Trigger& rTrigger = m_aTriggers[i];
		if(!rTrigger.IsActive())
		{
			continue;
		}

		//Update the trigger.
		UpdateTrigger(rPed, rPlayer, rTrigger);
	}
}

#if __BANK
void CPedAgitationScanner::RenderDebug(const CPed& rPed) const
{
	//Ensure rendering is enabled.
	TUNE_GROUP_BOOL(AGITATION_SCANNER, bRenderingEnabled, false);
	if(!bRenderingEnabled)
	{
		return;
	}

	//Render the active triggers.
	TUNE_GROUP_BOOL(AGITATION_SCANNER, bRenderActiveTriggers, true);
	if(bRenderActiveTriggers)
	{
		char debugText[128];
		int iLine = 0;

		for(int i = 0; i < m_aTriggers.GetMaxCount(); ++i)
		{
			const Trigger& rTrigger = m_aTriggers[i];
			if(!rTrigger.IsActive())
			{
				continue;
			}

			sprintf(debugText, "Reaction: %s, State: %d, Time in state: %.2f", rTrigger.m_hReaction.GetCStr(), rTrigger.m_nState, rTrigger.m_fTimeInState);
			grcDebugDraw::Text(VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition()) + ZAXIS, Color_red, 0, iLine++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}
}
#endif

//////////////////////////////
//Ped encroachment scanner
//////////////////////////////

EXT_PF_TIMER(EncroachmentScanner);

const float CPedEncroachmentScanner::ms_fPedMinimumSpeedSq = (0.5f * 0.5f);
const float CPedEncroachmentScanner::ms_fSneakyPedMinimumSpeedSq = (3.0f * 3.0f);

void CPedEncroachmentScanner::Scan(CPed & ped, CEntityScannerIterator& entityList)
{
	PF_FUNC(EncroachmentScanner);

	//shortcut to only check our decisionmaker once rather than every scan
	if (!m_bCheckedForRelevance) 
	{
		if ( ped.GetPedIntelligence()->HasResponseToEvent(EVENT_ENCROACHING_PED) )
		{
			RegisterSlot();
		}
		m_bCheckedForRelevance = true;
	}

	if( IsRegistered() && (ShouldBeProcessedThisFrame() || ShouldForceEncroachmentScanThisFrame()))
	{
		StartProcess();

		const Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
		

		// Note: the following code ASSUMES that this entity list is sorted by order of distance from the focus ped (nearest first).
		// If this ceases to be the case, then this scanner is going to become pretty useless!
		const CEntity* pEntity = entityList.GetFirst();
		while (pEntity != NULL)
		{
			Assert(pEntity->GetType()==ENTITY_TYPE_PED);
			CPed* pClosePed=(CPed*)pEntity;
			Vector3 vPedVelocity = !pClosePed->GetIsInVehicle() ? pClosePed->GetVelocity() : pClosePed->GetMyVehicle()->GetVelocity();
			float fPedVelocityMagSq = vPedVelocity.Mag2();
			const CPedPerception& perception = ped.GetPedIntelligence()->GetPedPerception();

			bool bNoisyPlayer = pClosePed->GetPlayerInfo() ? pClosePed->GetPlayerInfo()->GetStealthNoise() > SOUNDRANGE_MOSTLY_AUDIBLE : false;
			bool bAutoSense = pClosePed->GetPlayerInfo() && perception.GetCanAlwaysSenseEncroachingPlayers();
			bool bDo3DCheck = perception.GetPerformsEncroachmentChecksIn3D();

			//only have encroachment from alive things that are moving fast or making noise.
			if(!pClosePed->IsDead() && (pClosePed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_WALK || fPedVelocityMagSq >= ms_fPedMinimumSpeedSq || bNoisyPlayer || bAutoSense))
			{
				const Vector3 vClosePedPos = VEC3V_TO_VECTOR3(pClosePed->GetTransform().GetPosition());
				if ( ped.GetPedIntelligence()->IsThreatenedBy(*pClosePed, true ) )
				{
					//Check that close ped is in front of ped.
					Vector3 vDiff = vClosePedPos - vPedPos;

					//Calculate the personal space.
					float fPedPersonalSpaceDistanceSq = rage::square(perception.GetEncroachmentRange());

					//Check if the ped is using a scenario.
					const CTaskUseScenario* pScenarioTask = static_cast<CTaskUseScenario *>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
					if(pScenarioTask)
					{
						//Check if the scenario point is valid.
						const CScenarioPoint* pScenarioPoint = pScenarioTask->GetScenarioPoint();
						if(pScenarioPoint && pScenarioPoint->GetRadius() > 0.0f)
						{
							//Set the encroachment radius to be the point's radius.
							fPedPersonalSpaceDistanceSq = square(pScenarioPoint->GetRadius());
						}
					}
					
					float fDiff = bDo3DCheck ? vDiff.Mag2() : vDiff.XYMag2();
					if (fDiff <= fPedPersonalSpaceDistanceSq)
					{
						// If the ped is sneaking, then don't startle unless super close.
						bool bAddEncroachmentEvent = false;
						if (!bAutoSense && !bNoisyPlayer && pClosePed->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_WALK && fPedVelocityMagSq <= ms_fSneakyPedMinimumSpeedSq)
						{
							if (fDiff <= rage::square(perception.GetEncroachmentCloseRange()))
							{
								// Even though the ped was sneaking he got too close.
								bAddEncroachmentEvent = true;
							}
						}
						else
						{
							// The ped isn't sneaking so just add the event.
							bAddEncroachmentEvent = true;
						}
						if (bAddEncroachmentEvent)
						{
							CEventEncroachingPed event(pClosePed);
							ped.GetPedIntelligence()->AddEvent(event);
						}
					}
					else
					{
						//we can safely bail out because the nearest ped was not near enough to startle
						break;
					}
				}
			}
			pEntity = entityList.GetNext();
		}

		StopProcess();
	}
}

bool CPedEncroachmentScanner::ShouldForceEncroachmentScanThisFrame() const
{
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer)
	{
		const CVehicle* pVehicle = pPlayer->GetVehiclePedInside();
		if (pVehicle)
		{
			if (IsGreaterThanAll(MagSquared(pVehicle->GetAiVelocityConstRef()), ScalarV(CPedIntelligence::ms_fFrequentScanVehicleVelocitySquaredThreshold)))
			{
				return true;
			}
		}
	}

	return false;
}


EXT_PF_TIMER(FireScanner);

const int CNearbyFireScanner::ms_iLatencyPeriod=100;
const float CNearbyFireScanner::ms_fNearbyFireRange=20.0f;
const float CNearbyFireScanner::ms_fPotentialWalkIntoFireRange=4.0f;

void CNearbyFireScanner::Scan(CPed& ped, const bool bForce)
{
	PF_FUNC(FireScanner);

	//Test if its time to do another check.
	bool bShouldScan = false;
	if( IsRegistered() )
	{
		bShouldScan = ShouldBeProcessedThisFrame();
	}
	else
	{
		//Set the timer if it isn't already set.
		if(!m_timer.IsSet())
		{
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),0);
		}

		//Test if the scanner is ready to do another threat scan.
		if(m_timer.IsOutOfTime())
		{
			//Ready to to a threat scan.
			//Reset the timer.
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),ms_iLatencyPeriod);
			bShouldScan = true;
		}
	}

	if( bShouldScan || bForce)
	{
		StartProcess();
		//Time do do another check.
		//Test if the ped isn't on fire.

		//Reset the timer.
		m_timer.Set(fwTimer::GetTimeInMilliseconds(),ms_iLatencyPeriod);

		//const float fMoveBlendRatio = ped.GetPedIntelligence()->GetMoveBlendRatioFromGoToTask();

		//Ped isn't on fire so he still might want to avoid fire.
		//Test that the ped isn't already walking around a fire.
		//CTask* pTask = ped.GetPedIntelligence()->GetTaskActive();

		CFire* pNearestFire = NULL;

		// Perform a full scan once every ten scan periods;
		// Since scan interval is 100ms, this equals once per second.
		m_iFullScanCounter++;

		if(m_iFullScanCounter >= 5 || bForce)
		{
			m_iFullScanCounter = 0;

			struct TNearbyFire
			{
				Vec3V vPositionAndRadius;
				ScalarV fDistance;
			};
			TNearbyFire fires[NUM_NEARBY_FIRES+1];
			s32 iNumFires = 0;
			
			//-----------------------------------------------------------
			// Obtain a list of the nearest fires to this ped
			// Find the closest fire in the XY plane as 'pNearestFire'

			const ScalarV fScanRange( ms_fNearbyFireRange*ms_fNearbyFireRange );
			ScalarV fNearestFireXY = fScanRange;

			s32 i,f;

			for (i=0; i<g_fireMan.GetNumActiveFires(); i++)
			{
				CFire * pFire = g_fireMan.GetActiveFire(i);
				Vec3V vVec = pFire->GetPositionWorld() - ped.GetTransform().GetPosition();
				ScalarV fDistSqr = MagSquared(vVec);
				if( (fDistSqr < fScanRange).Getb())
				{
					for(f=0; f<iNumFires; f++)
					{
						if( (fDistSqr < fires[f].fDistance).Getb() )
						{
							for(s32 g=iNumFires-1; g>f; g--)
							{
								fires[g+1] = fires[g];
							}
							fires[f].vPositionAndRadius = pFire->GetPositionWorld();
							fires[f].vPositionAndRadius.SetW(pFire->GetMaxRadius());
							fires[f].fDistance = fDistSqr;
							break;
						}
					}
					if(f == iNumFires && f < NUM_NEARBY_FIRES)
					{
						fires[f].vPositionAndRadius = pFire->GetPositionWorld();
						fires[f].vPositionAndRadius.SetW(pFire->GetMaxRadius());
						fires[f].fDistance = fDistSqr;
						iNumFires++;
					}

					// Maintain the previous behaviour which determines the nearst fire
					// ranking nearby fires only on their XY distance from the ped.

					Vec2V vVecXY( vVec.GetIntrin128ConstRef() );
					ScalarV fDistSqrXY = MagSquared(vVecXY);
					if( (fDistSqrXY < fNearestFireXY).Getb() )
					{
						fNearestFireXY = fDistSqrXY;
						pNearestFire = pFire;
					}
				}
			}

			for(i=0; i<iNumFires; i++)
			{
				m_vNearbyFires[i] = RCC_VECTOR3(fires[i].vPositionAndRadius);
			}
			if(i < NUM_NEARBY_FIRES)
			{
				m_vNearbyFires[i].Zero();
			}
		}
		else
		{
			//Get the nearest fire

			float nearestFireDistSqr = ms_fNearbyFireRange*ms_fNearbyFireRange;

			for (int i=0; i<g_fireMan.GetNumActiveFires(); i++)
			{
				CFire * pFire = g_fireMan.GetActiveFire(i);

				Vec3V vVec = pFire->GetPositionWorld() - ped.GetTransform().GetPosition();
				vVec.SetZ(ScalarV(V_ZERO));

				float distSqr = MagSquared(vVec).Getf();
				if (distSqr<nearestFireDistSqr)
				{
					nearestFireDistSqr = distSqr;
					pNearestFire = pFire;
				}
			}
		}

		//Trigger a CEventFireNearby if required.
		if(pNearestFire)
		{
			Vec3V vFirePos = pNearestFire->GetPositionWorld();

			m_vNearbyFires[0] = RCC_VECTOR3(vFirePos);
			m_vNearbyFires[0].SetW(pNearestFire->GetMaxRadius());

			float zDiff = Abs((vFirePos - ped.GetTransform().GetPosition()).GetZf());
			if(zDiff < 2.0f)
			{			
				CEventFireNearby event(RCC_VECTOR3(vFirePos));
				ped.GetPedIntelligence()->AddEvent(event);
			}

			////Trigger a CEventPotentialWalkIntoFire if required.
		
			//CTaskMove * pMoveTask = ped.GetPedIntelligence()->GetActiveSimplestMovementTask();
			//if(pMoveTask && pMoveTask->IsMoveTask() && pMoveTask->IsTaskMoving() )
			//{
			//	//There is a nearest fire.
			//	//Test if the nearest fire is in the danger range.
			//	if(nearestFireDistSqr < (ms_fPotentialWalkIntoFireRange*ms_fPotentialWalkIntoFireRange))
			//	{
			//		if(zDiff<2.0f && pMoveTask->HasTarget())
			//		{
			//			Vector3 vMoveTarget2d = pMoveTask->GetTarget();
			//			Vector3 vPedPos2d = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
			//			vMoveTarget2d.z = vFirePos.GetZf();
			//			vPedPos2d.z = vFirePos.GetZf();

			//			const spdSphere sphere( vFirePos, ScalarV(pNearestFire->GetMaxRadius() + ped.GetCapsuleInfo()->GetHalfWidth()) );
			//			Vector3 vIsect1, vIsect2;

			//			const bool bIntersectsFire = fwGeom::IntersectSphereEdge(sphere, vPedPos2d, vMoveTarget2d, vIsect1, vIsect2);

			//			//Nearest fire is in the danger range.
			//			//Give the ped an event
			//			if(bIntersectsFire)
			//			{
			//				CEventPotentialWalkIntoFire event(RCC_VECTOR3(vFirePos), pNearestFire->GetMaxRadius(), fMoveBlendRatio);
			//				ped.GetPedIntelligence()->AddEvent(event);
			//			}
			//		}
			//	}
			//}
			
			if( ped.GetPedConfigFlag( CPED_CONFIG_FLAG_RunFromFiresAndExplosions ) )
			{
				//Find the nearest fire that can cause a blast.
				float nearestFireDistSqr = ms_fNearbyFireRange*ms_fNearbyFireRange;
				CFire* pNearestBlastFire = NULL;
				for (int i=0; i<g_fireMan.GetNumActiveFires(); i++)
				{
					CFire* pFire = g_fireMan.GetActiveFire(i);
					if (pFire->GetFireType()==FIRETYPE_REGD_VEH_PETROL_TANK || pFire->GetFireType()==FIRETYPE_TIMED_PETROL_POOL || pFire->GetFireType()==FIRETYPE_TIMED_PETROL_TRAIL)
					{
						Vec3V vVec = pFire->GetPositionWorld()-ped.GetTransform().GetPosition();
						vVec.SetZ(ScalarV(V_ZERO));

						float distSqr = MagSquared(vVec).Getf();
						if (distSqr<nearestFireDistSqr)
						{
							nearestFireDistSqr = distSqr;
							pNearestBlastFire = pFire;
						}
					}
				}

				//Trigger a CEventPotentialBlast if required.
				if(pNearestBlastFire)
				{
					CEntity* pEntity = pNearestBlastFire->GetEntity();

					float blastRadius = 0.0f;
					if(pNearestBlastFire->GetFireType() == FIRETYPE_REGD_VEH_PETROL_TANK)
					{
						if(pEntity)
						{
							blastRadius = pEntity->GetBoundRadius();
						}
					}
					else if(pNearestBlastFire->GetFireType() == FIRETYPE_TIMED_PETROL_TRAIL || pNearestBlastFire->GetFireType() == FIRETYPE_TIMED_PETROL_POOL)
					{
						// Consider petrol trails and pools explosive to make peds react, as fires
						// there tend to spread and may connect up to vehicles or other explosive objects.
						// We may want to actually add some more sophisticated checks for that stuff later,
						// if needed. The request to escape from petrol trails came from
						// B* 191538: "Shocking event: if there is a fire trail near them peds should see it and react and run away - see video".
						blastRadius = 3.0f;	// MAGIC!
					}
					else
					{
						Assertf(0, "found unsupported fire type");
					}

					//Ensure the entity is valid.
					if(pEntity)
					{
						//Calculate the time of explosion.
						float fTimeUntilExplosion;
						u32 uTimeOfExplosion = CExplosionHelper::CalculateTimeUntilExplosion(*pEntity, fTimeUntilExplosion) ?
							(fwTimer::GetTimeInMilliseconds() + (u32)(fTimeUntilExplosion * 1000.0f)) : 0;

						//Add the event.
						CEventPotentialBlast event(CAITarget(pEntity), blastRadius, uTimeOfExplosion);
						ped.GetPedIntelligence()->AddEvent(event);

						//Add the shocking event.
						CEventShockingPotentialBlast shockingEvent(*pEntity);
						CShockingEventsManager::Add(shockingEvent);
					}
					else
					{
						//Add the event.
						CEventPotentialBlast event(CAITarget(VEC3V_TO_VECTOR3(pNearestBlastFire->GetPositionWorld())), blastRadius, 0);
						ped.GetPedIntelligence()->AddEvent(event);

						//Add the shocking event.
						CEventShockingPotentialBlast shockingEvent(pNearestBlastFire->GetPositionWorld());
						CShockingEventsManager::Add(shockingEvent);
					}
				}
			}
		}
		StopProcess();
	}

	//Process the IsEntityOnFire section every time this method is called, regardless of bShouldScan.
	//If this is a MP game then the ApplyDamangeAndComputeResponse needs to be called every time otherwise the application of damage from fire will differ from SP.
	//When this is a SP game the throttle of bShouldScan is still applied to peds and players in SP, so the addevent calls occur at the same rate as prior.
	//lavalley

	//Check if ped is on fire.
	if(g_fireMan.IsEntityOnFire(&ped))
	{
		bool bPlayerInNetworkGame = false;
		if( ped.IsAPlayerPed() && NetworkInterface::IsGameInProgress() )
			bPlayerInNetworkGame = true;
		if(!bPlayerInNetworkGame)
		{
			if (bShouldScan) //only call addevent at the same rate as if this was in the bShouldScan section (throttled) <works at the same rate as before this change for peds and non-MP players> lavalley.
			{
				u32 uWeaponHash = WEAPONTYPE_FIRE;
				CFire* pFire = g_fireMan.GetEntityFire(&ped);
				if (pFire)
				{
					uWeaponHash = pFire->GetWeaponHash();
				}

				CEventOnFire event(uWeaponHash);
				ped.GetPedIntelligence()->AddEvent(event);
			}
		}
		else
		{
			CFire* pFire = g_fireMan.GetEntityFire(&ped);

			//Allow the player to take damage when they pause the game (even though controls are disabled).
			//If this was not here, players could pause the game inside of a blazing inferno and take no damage while waiting for the fire to dissipate.
			//Allow the player to take damage if SPC_AllowPlayerToBeDamaged is set to true
			bool bInflictDamage = (CGameWorld::GetMainPlayerInfo() && (!CGameWorld::GetMainPlayerInfo()->AreAnyControlsOtherThanFrontendDisabled() || CGameWorld::GetMainPlayerInfo()->GetPlayerDataCanBeDamaged()));

			if( bInflictDamage )
			{
				if(pFire)
				{
					float ftimestep = fwTimer::GetTimeStep();
					u32 uWeaponHash = pFire->GetWeaponHash();
					if(uWeaponHash == 0)
					{
						uWeaponHash = WEAPONTYPE_FIRE;
					}

					CEventDamage event(NULL, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_FIRE);
					CPedDamageCalculator damageCalculator(pFire->GetCulprit(), CTaskComplexOnFire::ms_fHealthRate*ftimestep, uWeaponHash, false);
					damageCalculator.ApplyDamageAndComputeResponse(&ped, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);
					if (CLocalisation::PedsOnFire() && ped.GetSpeechAudioEntity())
					{
						audDamageStats damageStats;
						damageStats.Fatal = false;
						damageStats.RawDamage = 20.0f;
						damageStats.DamageReason = AUD_DAMAGE_REASON_ON_FIRE;
						damageStats.PedWasAlreadyDead = ped.ShouldBeDead();
						ped.GetSpeechAudioEntity()->InflictPain(damageStats);
					}

					if( (event.GetDamageResponseData().m_bKilled || event.GetDamageResponseData().m_bInjured) )
					{
						Vec3V vFirePos = pFire->GetPositionWorld();
						CWeaponDamage::GeneratePedDamageEvent(pFire->GetCulprit(), &ped, uWeaponHash, CTaskComplexOnFire::ms_fHealthRate, RCC_VECTOR3(vFirePos), NULL, CPedDamageCalculator::DF_None);
					}
				}
			}
		}
	}
}

//////////////////////////
// PASSENGER EVENT SCANNER
//////////////////////////

EXT_PF_TIMER(PassengerEventScanner);

const int CPassengerEventScanner::ms_iLatencyPeriod=300;

void CPassengerEventScanner::Scan(CPed& ped)
{
	PF_FUNC(PassengerEventScanner);

	//Test if its time to do another check.
	bool bShouldScan = false;
	if( IsRegistered() )
	{
		bShouldScan = ShouldBeProcessedThisFrame();
	}
	else
	{
		//Test if the scanner is ready to do another scan.
		if(!m_timer.IsSet() || m_timer.IsOutOfTime())
		{
			bShouldScan = true;
		}
	}

	if( bShouldScan )
	{
		StartProcess();

		//Reset the timer.
		m_timer.Set(fwTimer::GetTimeInMilliseconds(),ms_iLatencyPeriod);

		//Scan for player standing on my boat
		if(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped.GetMyVehicle()
			&& ped.GetMyVehicle()->InheritsFromBoat()
			&& !ped.IsPlayer() && FindPlayerPed()->GetGroundPhysical()==ped.GetMyVehicle())
		{
			//Player is standing on ped's boat.
			CEventPedEnteredMyVehicle event(FindPlayerPed(),ped.GetMyVehicle());
			event.SetResponseTaskType(CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE);
			ped.GetPedIntelligence()->AddEvent(event); 
		}

		// If we're in a car thats on fire, generate the event
		if( ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped.GetMyVehicle() && ped.GetPedConfigFlag( CPED_CONFIG_FLAG_GetOutBurningVehicle ) )
		{
			if( ped.GetMyVehicle()->IsOnFire() )
			{
				CEventVehicleOnFire fireEvent(ped.GetMyVehicle());
				ped.GetPedIntelligence()->AddEvent(fireEvent);
			}
		}

		// If we are allowed to react to a ped on the roof
		if(!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_NeverReactToPedOnRoof))
		{
			//Scan for player on my car roof and I am the driver
			CPed * pPlayerPed = FindPlayerPed();
			if(pPlayerPed && pPlayerPed->GetGroundPhysical() && ped.GetMyVehicle() && 
				 pPlayerPed->GetGroundPhysical()==ped.GetMyVehicle() && 
				!ped.PopTypeIsMission() && !ped.IsAPlayerPed() )
			{
				if( !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_AlreadyReactedToPedOnRoof ) || !ped.GetMyVehicle()->IsHornOn() )
				{
					CEventPedOnCarRoof event(pPlayerPed,(CVehicle*)pPlayerPed->GetGroundPhysical());
					ped.GetPedIntelligence()->AddEvent(event);
					ped.SetPedConfigFlag( CPED_CONFIG_FLAG_AlreadyReactedToPedOnRoof, true );
				}
			}
			else
			{
				ped.SetPedConfigFlag( CPED_CONFIG_FLAG_AlreadyReactedToPedOnRoof, false );
			}
		}

		StopProcess();
	}
}

//////////////////////////
//COLLISION EVENT SCANNER
//////////////////////////

const float CCollisionEventScanner::KILL_FALL_HEIGHT = 10.0f;
const float CCollisionEventScanner::PLAYER_KILL_FALL_HEIGHT = 15.0f;

void CCollisionEventScanner::ScanForCollisionEvents(CPed* pPed)
{
	if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
		return;

	const CCollisionHistory* pCollisionHistory = pPed->GetFrameCollisionHistory();
	if(pCollisionHistory->GetNumCollidedEntities() == 0)
		return;

	const float fMoveBlendRatio = pPed->GetPedIntelligence()->GetMoveBlendRatioFromGoToTask();
	const CCollisionRecord* pColRecord;

	//Vehicle records
	for(pColRecord = pCollisionHistory->GetFirstVehicleCollisionRecord();
		pColRecord != NULL ; pColRecord = pColRecord->GetNext())
	{
		CVehicle * pVehicle = static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get());

		if( pVehicle == NULL ) continue;
		if( m_bAlreadyHitByCar ) continue;
		if( pVehicle->GetIsAttached() && pVehicle->GetAttachParent() == pPed ) continue;

		// enforce a minimum absolute vehicle speed and minimum speed relative to (And in the direction of) the ped
		const float fVehicleSpeedSqr = pVehicle->GetVelocity().XYMag2();
		const float fMinVelSqr = rage::square(CEventVehicleCollision::ms_fDamageThresholdSpeed);
		bool bOverSpeedThreshold = fVehicleSpeedSqr > fMinVelSqr;
		bool bOverRelSpeedInDirectionThreshold = false;

		if (bOverSpeedThreshold)
		{
			const float fVehicleRelSpeedAlongContact = Dot(VECTOR3_TO_VEC3V(pVehicle->GetVelocity() - pPed->GetVelocity()), RCC_VEC3V(pColRecord->m_MyCollisionNormal)).Getf();
			const float fMinRelSpeedAlongContact = CEventVehicleCollision::ms_fDamageThresholdSpeedAlongContact;

			bOverRelSpeedInDirectionThreshold = fVehicleRelSpeedAlongContact > fMinRelSpeedAlongContact;
			
			//animEntityDebugf(pPed, "VehicleCollisionRecord: speed check: %s(AbsSpeed: %.3f, min: %.3f) relSpeedAlongContact: %s (dot: %.3f, min:%.3f)(relSpeed: %.3f, %.3f, %.3f)(normal:%.3f, %.3f, %.3f)",
			//	bOverSpeedThreshold ? "PASSED" : "FAILED", fVehicleSpeedSqr, fMinVelSqr,
			//	bOverRelSpeedInDirectionThreshold ? "PASSED" : "FAILED", fVehicleRelSpeedAlongContact, fMinRelSpeedAlongContact,
			//	(pVehicle->GetVelocity() - pPed->GetVelocity()).x, (pVehicle->GetVelocity() - pPed->GetVelocity()).y, (pVehicle->GetVelocity() - pPed->GetVelocity()).z,
			//	pColRecord->m_MyCollisionNormal.x, pColRecord->m_MyCollisionNormal.y, pColRecord->m_MyCollisionNormal.z);
		}

		if(bOverSpeedThreshold && bOverRelSpeedInDirectionThreshold)
		{
			float fImpMag = pColRecord->m_fCollisionImpulseMag;
			if(pPed->GetIsStanding())
			{
				float fPedVelAlongCol = DotProduct(pPed->GetVelocity(), pColRecord->m_MyCollisionNormal);
				if(fPedVelAlongCol < 0.0f)
					fImpMag = MAX(0.0f, fImpMag + fPedVelAlongCol*pPed->GetMass());
			}

			//bool bLimitedCollisionImpact = pPed->IsPlayer();
			// don't limit the collision impact for cars and players
			// if we're playing a network game
			//bLimitedCollisionImpact &= !NetworkInterface::IsGameInProgress();
			if(pPed->IsPlayer() && !NetworkInterface::IsGameInProgress())
			{
				if(fImpMag > 1000.0f)	// limited to this anyway
					fImpMag = 1000.0f;

				const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				const float fVehicleHeading = pVehicle->GetTransform().GetHeading();
				const Vector3 bBoxVMin = pVehicle->GetBaseModelInfo()->GetBoundingBoxMin();
				const Vector3 bBoxVMax = pVehicle->GetBaseModelInfo()->GetBoundingBoxMax();
				Vector3 bBoxCentre = (bBoxVMin + bBoxVMax) / 2.0f;
				bBoxCentre = pVehicle->TransformIntoWorldSpace(bBoxCentre);
				bBoxCentre = bBoxCentre - vPedPos;

				const float theta = rage::Atan2f(-bBoxCentre.x, bBoxCentre.y);
				const float theta2 = fwAngle::LimitRadianAngle(fVehicleHeading - theta);
				const float cornerAngle = rage::Atan2f(bBoxVMax.x - bBoxVMin.x, bBoxVMax.y - bBoxVMin.y);

				Vector3 vecHitOffset = vPedPos - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
				vecHitOffset.Normalize();
				float fHitSpeed;

				static const float fHitSpdThreshMax = 5.0f;

				// hit by front or back of car (just do normal hit)
				if(rage::Abs(theta2) < cornerAngle || rage::Abs(theta2) > (PI - cornerAngle))
				{
					fHitSpeed = DotProduct(vecHitOffset, pVehicle->GetVelocity());
				}
				// hit by left side of car
				else if(theta2 > 0.0f)
				{
					fHitSpeed = DotProduct(-1.0f*VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()), pVehicle->GetVelocity());
				}
				// hit by right side of car
				else
				{
					fHitSpeed = DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()), pVehicle->GetVelocity());
				}

				if(fHitSpeed > fHitSpdThreshMax)
				{
					pPed->KillPedWithCar(pVehicle, fImpMag, pColRecord);
				}
				else
				{
					//Player shouldn't do anything if he isn't going to take any damage.
				}
			}
			else
			{
				if(!pPed->GetAttachParent() || pPed->GetAttachParent()->GetType()!=ENTITY_TYPE_VEHICLE)
				{
					// Ped not attached to a car.
					// Depending on the impulse magnitude either make him fall over or kill him
					pPed->KillPedWithCar(pVehicle, fImpMag, pColRecord);
				}
				else
				{
					// Ped attached to a car, just decrement the health
					CPedDamageCalculator damageCalculator(pVehicle, fImpMag, WEAPONTYPE_RAMMEDBYVEHICLE, 0, false);
					CEventDamage event(pVehicle, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_RAMMEDBYVEHICLE);
					damageCalculator.ApplyDamageAndComputeResponse(pPed, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);
				}						
			}				
		}
		else
		{
			if(0==pPed->GetAttachParent() || pPed->GetAttachParent()->GetType()!=ENTITY_TYPE_VEHICLE)
			{
				bool bShouldBeEnteringVehicleAsPartOfGroup = false;
				if (NetworkInterface::IsGameInProgress() && pPed->GetPedsGroup() 
					&& pPed->GetPedsGroup()->GetGroupMembership()->GetLeader() && pPed->GetPedsGroup()->GetGroupMembership()->GetLeader()->GetIsInVehicle()
					&& pPed->GetPedsGroup()->GetGroupMembership()->GetLeader()->GetMyVehicle() == pVehicle)
				{
					bShouldBeEnteringVehicleAsPartOfGroup = true;
				}

				// We want the player to react to running into car doors so he can navigate round them
				CTaskMove * pSimpleMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
				const bool bPlayerInControl = pSimpleMove && pSimpleMove->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER;
				if(!bPlayerInControl && !bShouldBeEnteringVehicleAsPartOfGroup)
				{
					// Ped not attached to a car.
					// Just make him react to the collision.
					const int iVehicleComponent = pColRecord->m_OtherCollisionComponent;
					Assert((unsigned)iVehicleComponent < 65536);

					CEventVehicleCollision event(
						pVehicle,
						pColRecord->m_MyCollisionNormal,
						pColRecord->m_MyCollisionPos,
						pColRecord->m_fCollisionImpulseMag,
						(u16)iVehicleComponent,
						fMoveBlendRatio);
					event.SetVehicleStationary(true);
					pPed->GetPedIntelligence()->AddEvent(event);
				}
			}
			else
			{
				if(pVehicle!=pPed->GetAttachParent())
				{
					bool bApplyDamage = true;
					if(pVehicle->GetVelocity().Mag2() < CTaskVehicleFSM::ms_Tunables.m_MinPhysSpeedToActivateRagdoll)
					{
						bApplyDamage = false;
					}

					if(bApplyDamage)
					{
						// Ped attached to a car, compute reduction in health from impact with other car.

						// Work out the relative speed difference between this ped and pEntity from the impulse.
						float fRelSpeed = 0.0f;
						float fPedEntRelVelMag = 0.0f;
						float fPedEntTangVelMagSq = 0.0f;
						ComputeRelativeSpeedFromImpulse(pPed, pVehicle, pColRecord->m_fCollisionImpulseMag, pColRecord->m_MyCollisionNormal, fRelSpeed, fPedEntRelVelMag, fPedEntTangVelMagSq);

						float fDamage = ComputeDamageFromVehicle(pPed, pVehicle, pColRecord->m_MyCollisionComponent, fPedEntRelVelMag, pColRecord->m_MyCollisionNormal, true, fRelSpeed);

						static float DAMAGE_MULTIPLIER_VEHICLE_IN_WATER = 0.1f;
						if(pVehicle->GetIsInWater())
						{
							fDamage *= DAMAGE_MULTIPLIER_VEHICLE_IN_WATER;
						}

						CPedDamageCalculator damageCalculator(pVehicle, fDamage, WEAPONTYPE_RAMMEDBYVEHICLE, PED_SPHERE_CHEST, false);
						CEventDamage event(pVehicle, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_RAMMEDBYVEHICLE);
						damageCalculator.ApplyDamageAndComputeResponse(pPed, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);
					}
				}
			}
		}
	}
	
	//Ped records
	for(pColRecord = pCollisionHistory->GetFirstPedCollisionRecord();
		pColRecord != NULL ; pColRecord = pColRecord->GetNext())
	{
		CPed* pOtherPed=(CPed*)pColRecord->m_pRegdCollisionEntity.Get();

		if(pOtherPed == NULL) continue;

		// Collision events not generated for swimming peds until NM behaviours can be switched off
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ) || pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming )) continue;
			
		const float fOtherPedMoveBlendRatio = pOtherPed->GetMotionData()->GetCurrentMbrY();

		if(pPed->IsPlayer())
		{
			CEventPlayerCollisionWithPed event(
				(u16)pColRecord->m_MyCollisionComponent,
				pColRecord->m_fCollisionImpulseMag,
				pOtherPed,
				pColRecord->m_MyCollisionNormal,
				pColRecord->m_MyCollisionPos,
				fMoveBlendRatio,
				fOtherPedMoveBlendRatio);
			pPed->GetPedIntelligence()->AddEvent(event); 
		}
		else
		{
			if(pOtherPed->IsPlayer())
			{
				CEventPedCollisionWithPlayer event(
					(u16)pColRecord->m_MyCollisionComponent,
					pColRecord->m_fCollisionImpulseMag,
					pOtherPed,
					pColRecord->m_MyCollisionNormal,
					pColRecord->m_MyCollisionPos,
					fMoveBlendRatio,
					fOtherPedMoveBlendRatio);
				pPed->GetPedIntelligence()->AddEvent(event); 
				const CRelationshipGroup* pRelGroup = pPed->GetPedIntelligence()->GetRelationshipGroup();

				// consider bumping into the player as the same as seeing them
				if( pRelGroup )
				{
					if(pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_HATE, pOtherPed->GetPedIntelligence()->GetRelationshipGroupIndex() ))
					{
						CEventAcquaintancePedHate eventHate(pOtherPed);
						pPed->GetPedIntelligence()->AddEvent(eventHate);
					}
					else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat) &&
							pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) &&
							pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_DISLIKE, pOtherPed->GetPedIntelligence()->GetRelationshipGroupIndex() ))
					{
						CEventAcquaintancePedHate eventHate(pOtherPed);
						pPed->GetPedIntelligence()->AddEvent(eventHate);
					}
				}
			}
			else
			{
				CEventPedCollisionWithPed event(
					(u16)pColRecord->m_MyCollisionComponent,
					pColRecord->m_fCollisionImpulseMag,
					pOtherPed,
					pColRecord->m_MyCollisionNormal,
					pColRecord->m_MyCollisionPos,
					fMoveBlendRatio,
					fOtherPedMoveBlendRatio);  
				pPed->GetPedIntelligence()->AddEvent(event);
			}
		}
	}

	//Object records (this is a mess)
	for(pColRecord = pCollisionHistory->GetFirstObjectCollisionRecord();
		pColRecord != NULL ; pColRecord = pColRecord->GetNext())
	{
		CObject *pDamageObj = (CObject*)pColRecord->m_pRegdCollisionEntity.Get();

		if(pDamageObj == NULL) continue;

		// subtract damage caused by the ped running into the object
		// so we know how hard the object hit the ped
		float fTempDamageImpulse = pColRecord->m_fCollisionImpulseMag;
		fTempDamageImpulse *= Clamp(1.0f - pColRecord->m_MyCollisionNormal.z, 0.0f, 1.0f);

		if(pPed->GetIsStanding() && !pDamageObj->GetIsStatic())
		{
			float fPedVelAlongCol = DotProduct(pPed->GetVelocity(), pColRecord->m_MyCollisionNormal);
			if(fPedVelAlongCol < 0.0f)
				fTempDamageImpulse = rage::Max(0.0f, fTempDamageImpulse + fPedVelAlongCol*pPed->GetMass());
		}

		static float PED_OBJECT_COLLISION_DAMAGE_THRESHOLD = 50.0f;
#if 0 // CS
		static float PED_HIT_BY_WEAPON_OBJECT_COLLISION_DAMAGE_THRESHOLD = 10.0f;
#endif // 0
		static float PLAYER_OBJECT_COLLISION_DAMAGE_THRESHOLD = 100.0f;
		float fCollisionDamageThreshold = PED_OBJECT_COLLISION_DAMAGE_THRESHOLD;
#if 0 // CS
		if(pDamageObj->GetWeapon() && pDamageObj->GetWeapon()->GetWeaponType() == WEAPONTYPE_OBJECT)
			fCollisionDamageThreshold = PED_HIT_BY_WEAPON_OBJECT_COLLISION_DAMAGE_THRESHOLD;
		else
#endif // 0
		if(pPed->GetPlayerInfo())
				fCollisionDamageThreshold = PLAYER_OBJECT_COLLISION_DAMAGE_THRESHOLD;

		if(fTempDamageImpulse > fCollisionDamageThreshold && !pDamageObj->GetIsStatic() && pPed->GetIsStanding()
			&& !pDamageObj->IsADoor()
			&& pDamageObj!=pPed->GetGroundPhysical() && !(pDamageObj->GetAttachParent() && pDamageObj==pDamageObj->GetAttachParent()))
		{

//If ever re-enabled this will need fixing to use the latest type of collision records
#if 0 // CS 
			static float OBJECT_COLLISION_DAMAGE_MULTIPLIER = 10.0f;
			fTempDamageImpulse *= OBJECT_COLLISION_DAMAGE_MULTIPLIER/fCollisionDamageThreshold;

			Vector3 vecTempStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			phIntersection tempIntersection;
			tempIntersection.Set(pDamageObj->GetCurrentPhysicsInst(), RCC_VEC3V(pColRecord->m_MyCollisionPos), 
				RCC_VEC3V(pColRecord->m_MyCollisionNormal), 0.0f, 1.0f, 0, 0, (u16)pColRecord->m_MyCollisionComponent());

			if(pDamageObj->GetWeapon() && pDamageObj->GetWeapon()->GetWeaponType() == WEAPONTYPE_OBJECT)
			{
				CEntity* pParent = pPed->m_pCollisionEntity;

				// look up the original owner of the projectile in the projectile manager
				CProjectileData* pProj = CProjectileInfo::GetProjectile(pDamageObj);
				if(pProj && pProj->m_pEntProjectileOwner)
					pParent = pProj->m_pEntProjectileOwner;

				float fObjectDamage;
				CWeaponInfo* pWeaponInfo = pDamageObj->GetWeapon()->GetWeaponInfo();
				if(pWeaponInfo)
				{
					// Use the constant weapon damage from the CWeaponInfo
					fObjectDamage = static_cast<float>(pWeaponInfo->GetWeaponDamage(pPed));
				}
				else
				{
					fObjectDamage = fTempDamageImpulse;
				}

				CWeaponDamage::GeneratePedDamageEvent(pParent, pPed, WEAPONTYPE_OBJECT, fObjectDamage, &vecTempStart, &tempIntersection);
			}
			else
			{
				CWeaponDamage::GeneratePedDamageEvent(pPed->m_pCollisionEntity, pPed, WEAPONTYPE_FALL, fTempDamageImpulse, &vecTempStart, &tempIntersection);
			}
#endif // 0
		}
		else
		{
			u32 frameCount = fwTimer::GetSystemFrameCount();

			// Do not allow object collision event to trigger consecutively
			bool bGenerateEvent = ( m_CollisionEventFrame + 1 ) < frameCount;
			if( bGenerateEvent )
			{
				m_CollisionEventFrame = frameCount;

				CEventObjectCollision event(
					(u16)pColRecord->m_MyCollisionComponent,
					pDamageObj,
					pColRecord->m_MyCollisionNormal,
					pColRecord->m_MyCollisionPos,
					fMoveBlendRatio);
				pPed->GetPedIntelligence()->AddEvent(event);
			}
		}
	}

	//Audio
	if( pPed->GetPlayerInfo() && pCollisionHistory->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_OBJECT) )
	{
		static float PLAYER_BUMP_SOUND_THRESHOLD_MED = 3.0f;
		static float PLAYER_BUMP_SOUND_THRESHOLD_LOW = 1.0f;
		static u32 nLastBumpSoundEvent = 0;

		float fRange = 0.0f;  // sound event range
		float fMaxImpulse = pCollisionHistory->GetMaxCollisionImpulseMagLastFrameOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_OBJECT);

		if(fMaxImpulse > PLAYER_BUMP_SOUND_THRESHOLD_LOW && fwTimer::GetTimeInMilliseconds() > nLastBumpSoundEvent + 1000)
		{
			if(fMaxImpulse > PLAYER_BUMP_SOUND_THRESHOLD_MED)
				fRange = SOUNDRANGE_CLEARLY_AUDIBLE;
			else
				fRange = SOUNDRANGE_BARELY_AUDIBLE;

			nLastBumpSoundEvent = fwTimer::GetTimeInMilliseconds();
		}
	}

	// reset flag
	m_bAlreadyHitByCar = false;
}

dev_float BLEEDER_DAMAGE_INTERVAL = 5.0f;
dev_float BLEEDER_DAMAGE_AMOUNT = 6.0f;
//dev_u32 BLEEDER_MIN_TIME			= 5000;

CPedHealthScanner::CPedHealthScanner()
	: m_DamageEntity(NETWORK_INVALID_OBJECT_ID)
	, m_fApplyDamageTimer(BLEEDER_DAMAGE_INTERVAL)
	, m_fTimeInValidHunchedTransition(0.f)
	, m_nFramesSinceLastUpdate(1)
	, m_bIsFallingOver(false)
{

}

void CPedHealthScanner::Scan(CPed& ped)
{
	if (ped.IsPlayer())
	{
		if (ped.GetHealth() < ped.GetHurtHealthThreshold())
		{
			if (ped.GetHurtEndTime() == 0 && !ped.GetIsSwimming())
			{
				static const int iBleedDuration = 15000;
				ped.SetHurtEndTime(fwTimer::GetTimeInMilliseconds() + iBleedDuration);
			}
		}
		else
		{
			ped.SetHurtEndTime(0);	// Allow more bleed once we reach threshold again
		}

		return;
	}

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || !ped.CanBeInHurt() || ped.IsDead())	// 
		return;

	// We don't need to check this every frame really, but beware of the timeslice also
	if (m_nFramesSinceLastUpdate++ < 1)	// ugly prevent LHS
		return;

	// Keep caching the last damager as this bugger might not always be valid
	CEntity* pEntity = ped.GetWeaponDamageEntityPedSafe();
	if (pEntity && pEntity->GetIsTypePed())
	{
		if (((CPed*)pEntity)->GetNetworkObject())
		{
			m_DamageEntity = ((CPed*)pEntity)->GetNetworkObject()->GetObjectID();
		}
	}

	m_nFramesSinceLastUpdate = 0;
	const float fFrameTimeConsideringFrameSkip = fwTimer::GetTimeStep() * 2.f;

	// Well... These tunes are "expensive" in non BANK and we probably don't need them anymore
	static const int MinTimeInHunched = 5000;
	static const int MaxTimeInHunched = 10000;
	static const float fBleedDamageAmount = 15.f;
	static const float fBleedValidTransitionTimeThreshold = 2.f;
	static const float fLungsValidTransitionTimeThreshold = 0.75f;
	static const float fBleedMaxVelocityThreshold = 0.1f;
	static const float fLungsMaxVelocityThreshold = 0.3f;
	//TUNE_GROUP_INT(WRITHE_TWEAK, MinTimeInHunched, 5000, 0, 10000, 100);
	//TUNE_GROUP_INT(WRITHE_TWEAK, MaxTimeInHunched, 10000, 0, 25000, 100);
	//TUNE_GROUP_FLOAT(BLEEDING_TWEAK, BleedDamageAmount, 15.f, 0.f, 500.f, 100.f);
	//TUNE_GROUP_FLOAT(BLEEDING_TWEAK, BleedValidTransitionThreshold, 2.f, 0.f, 10.f, 100.f);

	float fTimeInValidHunchedTransition = m_fTimeInValidHunchedTransition; // Local variable to prevent LHS when comparing since we write to member variable
	bool bIsInValidHunchTransitionState = (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) && ped.CanDoHurtTransition());
	if (bIsInValidHunchTransitionState)
		fTimeInValidHunchedTransition += fFrameTimeConsideringFrameSkip;
	else
		fTimeInValidHunchedTransition = 0.f;

	m_fTimeInValidHunchedTransition = fTimeInValidHunchedTransition;
	Assertf(ped.GetPedIntelligence(), "Invalid PedIntelligence in CPedHealthScanner::Scan()");

	if (!ped.IsHurtHealthThreshold())
	{
		// Bleeding when damaged
		if (ped.GetHurtEndTime() == 0)
		{
			if (*(int*)&m_fApplyDamageTimer > 0) // Yes super neat but >= cannot be tested just like this, as, -0.0f != 0.0f
			{
				if (bIsInValidHunchTransitionState)
					m_fApplyDamageTimer -= fFrameTimeConsideringFrameSkip;
			}
			else
			{
				m_fApplyDamageTimer += BLEEDER_DAMAGE_INTERVAL;

				// IsHurtHealthThreshold is dependent on not having scripts fiddling around with the health
				// Therefor we might actually bleed to death unless we check for that here, maybe death is fine
				// but it was never the intention according to the design
				if (ped.GetHealth() - fBleedDamageAmount > ped.GetInjuredHealthThreshold())
				{
					CEntity* pCurrentTarget = ped.GetWeaponDamageEntityPedSafe();
					if (!pCurrentTarget || !pCurrentTarget->GetIsTypePed())
					{
						if (NetworkInterface::IsGameInProgress() && m_DamageEntity != NETWORK_INVALID_OBJECT_ID)
							pCurrentTarget = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetNetworkObject(m_DamageEntity));
					}

					CPedDamageCalculator damageCalculator(pCurrentTarget, fBleedDamageAmount, WEAPONTYPE_BLEEDING, 0, false);
					CEventDamage event(pCurrentTarget, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_BLEEDING);

					// Don't apply the bleeding if we already have a damage event, as it will throw out valid response tasks
					if (!ped.GetPedIntelligence()->HasEventOfType(&event))
					{	
						damageCalculator.ApplyDamageAndComputeResponse(&ped, event.GetDamageResponseData(), false);
						if ((event.GetDamageResponseData().m_bKilled || event.GetDamageResponseData().m_bInjured))
						{
							Assertf(0, "Bleeder damage killed the ped, this shouldn't happend in this code path");
						}
					}
				}
			}
		}

		if (!ped.IsBodyPartDamaged(CPed::DAMAGED_GOTOWRITHE) && !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured ) && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested))	// To end up here we are already shot somewhere so this is safe
		{
			bool bAllowGoToWrithe = false;

			//! If OnlyWritheFromWeaponDamage is set & we have been damaged by a weapon in the last 5 secs, allow writhe.
			if( ped.GetPedConfigFlag( CPED_CONFIG_FLAG_OnlyWritheFromWeaponDamage ) )
			{
				float fWritheThreshold = ped.GetPedHealthInfo() ? ped.GetPedHealthInfo()->GetWritheFromBulletDamageTheshold() : 200.0f;
				if(ped.GetHealth() < fWritheThreshold )
				{
					for(int i = 0; i < ped.GetWeaponDamageInfoCount() && !bAllowGoToWrithe; ++i)
					{
						const CPhysical::WeaponDamageInfo &weaponDamageInfo = ped.GetWeaponDamageInfo(i);
						if(fwTimer::GetTimeInMilliseconds() < (weaponDamageInfo.weaponDamageTime + 5000))
						{
							const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo< CWeaponInfo >(weaponDamageInfo.weaponDamageHash);
							if(pWeaponInfo)
							{
								switch(pWeaponInfo->GetDamageType())
								{
									case DAMAGE_TYPE_BULLET:
									case DAMAGE_TYPE_BULLET_RUBBER:
									case DAMAGE_TYPE_EXPLOSIVE:
									case DAMAGE_TYPE_FIRE:
										bAllowGoToWrithe = true;
										break;
									default:
										break;
								}
							}
						}
					}
				}
			}
			else
			{
				bAllowGoToWrithe = true;
			}


			if(bAllowGoToWrithe)
			{
				// Make sure we roll the dice if peds are falling over but not hurt enough
				CTaskNMControl* pNMTask = smart_cast<CTaskNMControl*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
				if (pNMTask)
				{
					bool bWasFallingOver = m_bIsFallingOver;
					if (pNMTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
					{
						static dev_float WRITHE_CHANCE = 0.5f;
						if (!bWasFallingOver && fwRandom::GetRandomNumberInRange(0.f, 1.f) < WRITHE_CHANCE)
							ped.ReportBodyPartDamage(CPed::DAMAGED_GOTOWRITHE);

						m_bIsFallingOver = true;
					}
					else
					{
						m_bIsFallingOver = false;
					}
				}
			}
		}
	}
	else // ped health is hurt and they can go to hurt mode
	{
		// Set default check values
		float fMinTime = fBleedValidTransitionTimeThreshold;
		float fMaxVelocity = fBleedMaxVelocityThreshold;

		// If lungs are damaged
		if( ped.IsBodyPartDamaged(CPed::DAMAGED_LUNGS) )
		{
			// ped can no longer use cover effectively
			// NOTE: without this the peds may choose to go right back to cover they bail from
			ped.GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanUseCover);

			// Make the ped cough due to lung damage
			const audSpeechAudioEntity* pAudioEnt = ped.GetSpeechAudioEntity();
			if( pAudioEnt && !pAudioEnt->IsPainPlaying() && pAudioEnt->ShouldPlayCoughForDamagedLungs() )
			{
				audDamageStats damageStats;
				damageStats.DamageReason = AUD_DAMAGE_REASON_COUGH;
				ped.GetSpeechAudioEntity()->InflictPain(damageStats);
			}

			// adjust check settings
			fMinTime = fLungsValidTransitionTimeThreshold;
			fMaxVelocity = fLungsMaxVelocityThreshold;
		}

		// Alright see if we should trigger the transition to hunched
		if (!ped.HasHurtStarted() && fTimeInValidHunchedTransition > fMinTime)
		{
			if (ped.GetVelocity().Mag2() > fMaxVelocity && fBleedValidTransitionTimeThreshold < fMinTime * 4.f)
			{
				// This stuff does not seem to help, either add a flag or something else
			//	ped.GetMotionData()->SetDesiredMoveBlendRatio(0.f);
			//	ped.GetMotionData()->StopAllMotion();
			}
			else // Possibly check this one also IsBodyPartDamage(CPed::DAMAGED_GOTOWRITHE), we might be in getup task mode
			{
				CTask* pTaskActive = ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL);
				if (!pTaskActive)
				{
					// We might have waited for this transition for quite a while so we set the timer again
					ped.SetHurtEndTime(fwRandom::GetRandomNumberInRange(0, MaxTimeInHunched - MinTimeInHunched) + MinTimeInHunched + fwTimer::GetTimeInMilliseconds());

					CEntity* pTarget = ped.GetWeaponDamageEntityPedSafe();
					if (!pTarget || !pTarget->GetIsTypePed())
					{
						if (NetworkInterface::IsGameInProgress() && m_DamageEntity != NETWORK_INVALID_OBJECT_ID)
							pTarget = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetNetworkObject(m_DamageEntity));
					}

					CEventHurtTransition hurtEvent(pTarget);
					ped.GetPedIntelligence()->AddEvent(hurtEvent);
				}
			}
		}

		// See if we should go into writhe or trigger the signal for NM to start the transition next time damaged
		if (!ped.IsInjured()) // Can't let stunned enemies to bleed out and stuff
		{
			if (ped.GetHurtEndTime() == 0 &&
				!ped.GetPedIntelligence()->GetTaskMelee() &&
				!ped.GetPedIntelligence()->GetTaskWrithe())
			{
				CTask* pTask = FindControllingTask(&ped);
				if ((pTask && pTask->IsConsideredInCombat()) || ped.GetPedIntelligence()->GetTaskCombat() || ped.GetPedIntelligence()->GetTaskGun())
				{
					ped.SetHurtEndTime(fwRandom::GetRandomNumberInRange(0, MaxTimeInHunched - MinTimeInHunched) + MinTimeInHunched + fwTimer::GetTimeInMilliseconds());
					ped.SetPedResetFlag(CPED_RESET_FLAG_HurtThisFrame, true);
				}
			}

			if (ped.HasHurtStarted())
			{
				// Ped can no longer use cover while in hurt state
				ped.GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanUseCover);
				ped.GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(true);
				ped.GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(true);

				if (ped.GetHurtEndTime() > 0 && ped.GetHurtEndTime() <= fwTimer::GetTimeInMilliseconds() &&
					!ped.GetPedIntelligence()->GetTaskMelee() &&
					!ped.GetPedIntelligence()->GetTaskWrithe())
				{
					CTask* pTaskActive = ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL);
					if (!pTaskActive && CTaskWrithe::StreamReqResourcesInOldAndBad(&ped, CTaskWrithe::NEXT_STATE_COLLAPSE))
					{
						CEntity* pTarget = ped.GetWeaponDamageEntityPedSafe();
						if (!pTarget || !pTarget->GetIsTypePed())
						{
							if (NetworkInterface::IsGameInProgress() && m_DamageEntity != NETWORK_INVALID_OBJECT_ID)
								pTarget = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetNetworkObject(m_DamageEntity));
						}

						//! Don't set target unless we have one as it'll assert otherwise.
						CWeaponTarget Target;
						if(pTarget)
						{
							Target.SetEntity(pTarget);
						}

						CEventWrithe event(Target);
						if (!ped.GetPedIntelligence()->HasEventOfType(&event))
							ped.GetPedIntelligence()->AddEvent(event);
					}
				}
			}
		}
	}
}

CTask* CPedHealthScanner::FindControllingTask(CPed* pPed)
{
	CTask* pTask = NULL;
	if (pPed)
	{
		pTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP);

		if (!pTask)
			pTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		if (!pTask)
			pTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_DEFAULT);
	}

	return pTask;
}

bool IsPlayerOrPlayersVehicle(const CEntity* const pEntity)
{
	bool bIsPlayerOrPlayersVehicle = false;

	if(pEntity)
	{
		if(pEntity->GetIsTypeVehicle())
		{
			const CPed* const pDriver = static_cast<const CVehicle*>(pEntity)->GetDriver();
			bIsPlayerOrPlayersVehicle = (pDriver && pDriver->IsPlayer());
		}
		else if(pEntity->GetIsTypePed())
		{
			bIsPlayerOrPlayersVehicle = static_cast<const CPed*>(pEntity)->IsPlayer();
		}
	}

	return bIsPlayerOrPlayersVehicle;
}

#if __BANK
void CCollisionEventScanner::AddRagdollDamageWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Ragdoll Damage");
		// Helps to just see the health of a ped without any other debug info sometimes.
		bank.AddToggle("Display ped health", &CPhysics::ms_bDisplayPedHealth);
		// Useful to figure out if the fall height is being updated correctly.
		bank.AddToggle("Display fall height", &CPhysics::ms_bDisplayPedFallHeight);
		// Define the mass categories for different damage bands (minimum mass for each category).
		bank.AddSlider("Vehicle mass heavy", &RAGDOLL_DAMAGE_VEHICLE_MASS_HEAVY, 0.0f, 1000000.0f, 1.0f);
		bank.AddSlider("Vehicle mass medium", &RAGDOLL_DAMAGE_VEHICLE_MASS_MEDIUM, 0.0f, 1000000.0f, 1.0f);
		// Define the base damage inflicted in each of the different velocity bands for vehicle damage.
		bank.AddSlider("Vehicle damage crazy fast", &RAGDOLL_DAMAGE_VEHICLE_CRAZY, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle damage fast", &RAGDOLL_DAMAGE_VEHICLE_FAST, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle damage medium", &RAGDOLL_DAMAGE_VEHICLE_MEDIUM, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle damage slow", &RAGDOLL_DAMAGE_VEHICLE_SLOW, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle damage crawling", &RAGDOLL_DAMAGE_VEHICLE_CRAWLING, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle speed crazy fast", &RAGDOLL_SPEED_VEHICLE_CRAZY, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle speed fast", &RAGDOLL_SPEED_VEHICLE_FAST, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle speed medium", &RAGDOLL_SPEED_VEHICLE_MEDIUM, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Vehicle speed slow", &RAGDOLL_SPEED_VEHICLE_SLOW, 0.0f, 1000.0f, 1.0f);
		// Angle impact normal makes with horizontal to decide when to apply vehicle damage.
		bank.AddSlider("Vehicle impact angle param", &RAGDOLL_DAMAGE_VEHICLE_IMPACT_ANGLE_PARAM, 0.0f, 1.0f, 0.01f);
		// Dot product value to decide when a ped is under a vehicle.
		bank.AddSlider("Under vehicle impact angle param", &RAGDOLL_DAMAGE_UNDER_VEHICLE_IMPACT_ANGLE_PARAM, -1.0f, 1.0f, 0.01f);
		// Minimum velocity of train before damage is applied.
		bank.AddSlider("Train vel threshold", &RAGDOLL_DAMAGE_TRAIN_VEL_THRESHOLD, 0.0f, 100.0f, 0.1f);
		// If player is within this range at end of bounding box during impact then we count it as a front on impact.
		bank.AddSlider("Train bounding box 'y' adjust", &RAGDOLL_DAMAGE_TRAIN_BB_ADJUST_Y, 0.0f, 10.0f, 0.01f);
		// Modifiers for the various damage inflictor types. This is related to the mass and is multiplied by the magnitude of the relative
		// velocity to compute a damage value.
		bank.AddSlider("Damage mult train", &RAGDOLL_DAMAGE_MULTIPLIER_TRAIN, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult plane", &RAGDOLL_DAMAGE_MULTIPLIER_PLANE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult bike", &RAGDOLL_DAMAGE_MULTIPLIER_BIKE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult light vehicle", &RAGDOLL_DAMAGE_MULTIPLIER_LIGHT_VEHICLE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult medium vehicle", &RAGDOLL_DAMAGE_MULTIPLIER_MEDIUM_VEHICLE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage multiplier heavy vehicle", &RAGDOLL_DAMAGE_MULTIPLIER_HEAVY_VEHICLE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult under vehicle", &RAGDOLL_DAMAGE_MULTIPLIER_UNDER_VEHICLE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult fall", &RAGDOLL_DAMAGE_MULTIPLIER_FALL, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult fall land foot", &RAGDOLL_DAMAGE_MULTIPLIER_FALL_LAND_FOOT, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult general collision", &RAGDOLL_DAMAGE_MULTIPLIER_GENERAL_COLLISION, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult sliding damage", &RAGDOLL_DAMAGE_MULTIPLIER_SLIDING_COLLISION, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult ped", &RAGDOLL_DAMAGE_MULTIPLIER_PED, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Damage mult light object", &RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_LIGHT, 0.0, 100.0f, 0.1f);
		bank.AddSlider("Damage mult heavy object", &RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_HEAVY, 0.0, 100.0f, 0.1f);
		// Minimum amount to fall before ped will take any fall damage.
		bank.AddSlider("Ped fall height threshold", &RAGDOLL_DAMAGE_PED_FALL_HEIGHT_THRESHOLD, 0.0f, 1000.0f, 0.01f);
		// Minimum tangential velocity before inflicting sliding damage.
		bank.AddSlider("Min speed for sliding damage", &RAGDOLL_DAMAGE_MIN_SPEED_SLIDING_DAMAGE, 0.0f, 100.0f, 0.1f);
		// Minimum relative speed at which a non-zero damage event will be created for each inflictor type.
		bank.AddSlider("Min speed fall", &RAGDOLL_DAMAGE_MIN_SPEED_FALL, 0.0f, 100.0f, 0.01f);
		bank.AddSlider("Min speed ped", &RAGDOLL_DAMAGE_MIN_SPEED_PED, 0.0f, 100.0f, 0.01f);
		bank.AddSlider("Min speed vehicle", &RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE, 0.0f, 100.0f, 0.01f);
		// Minimum mass of object which causes damage to ped.
		bank.AddSlider("Min object mass", &RAGDOLL_DAMAGE_MIN_OBJECT_MASS, 0.0f, 10000.0f, 0.1f);
		// Scaling factors to modify various damage types when they happen to a player ped.
		bank.AddSlider("Player ped scale vehicle damage", &RAGDOLL_DAMAGE_MULTIPLIER_PLAYER_VEHICLE_SCALE, 0.0f, 100.0f, 0.1f);
		// Scaling factors to modify various damage types when they happen to an animal.
		bank.AddSlider("Scale animal damage", &RAGDOLL_DAMAGE_MULTIPLIER_ANIMAL_SCALE, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Vehicle impact damage time limit", &RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT, 0, 100000, 1);
	bank.PopGroup(); // "Ragdoll Damage"
}
#endif // __BANK

void CCollisionEventScanner::ComputeRelativeSpeedFromImpulse(const CPed* pPed, const CEntity* pEntity, const float fImpulseMag, const Vector3& vPedNormal,
															 float& fRelSpeed, float& fPedEntRelVelMag, float& fPedEntTangVelMagSq)
{
	fRelSpeed = fImpulseMag * pPed->GetRagdollInst()->GetArchetype()->GetInvMass();
	// Also work out the magnitude of the velocity difference between this ped and the entity doing the damage which will
	// likely be different (NB: this is the difference in the linear velocities of the ped and the inflictor entity, not just
	// the velocity along the collision normal which is given by fRelSpeed).
	fPedEntRelVelMag = 0.0f;
	fPedEntTangVelMagSq = 0.0f; // Tangential / sliding component of relative velocity.
	Vector3 vPedEntRelVel(VEC3_ZERO);
	if(pEntity && pEntity->GetIsPhysical())
	{
		const CPhysical* pPhysical = static_cast<const CPhysical*>(pEntity);
		vPedEntRelVel = pPed->GetVelocity() - pPhysical->GetVelocity();
	}
	else
	{
		vPedEntRelVel = pPed->GetVelocity();
	}
	fPedEntRelVelMag = vPedEntRelVel.Dot(-vPedNormal);
	fPedEntTangVelMagSq = vPedEntRelVel.Mag2() - fPedEntRelVelMag*fPedEntRelVelMag;

	// We can get valid contacts with zero impulse applied due to the velocity solve happening at the start of each physics update. If
	// this happens, just estimate the impulse from the velocity difference.
	if(fImpulseMag < SMALL_FLOAT)
	{
		fRelSpeed = fPedEntRelVelMag;
	}
}

float CCollisionEventScanner::ComputeDamageFromVehicle(const CPed* pPed, const CVehicle* pVehicle, const int nComponent, const float fPedEntRelVelMag,
													   const Vector3& vPedNormal, const bool bPedStanding, const float fRelSpeed)
{
	float fDamage = 0.0f;

	// Base damage is calculated from the relative velocity of the inflictor vehicle. The vehicle's velocity
	// range is divided into 5 equal regions.
	if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_CRAZY)
	{
		fDamage = RAGDOLL_DAMAGE_VEHICLE_CRAZY;
	}
	else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_FAST)
	{
		const float maxSpeedDiff = RAGDOLL_SPEED_VEHICLE_CRAZY - RAGDOLL_SPEED_VEHICLE_FAST;
		const float maxDamageDiff = RAGDOLL_DAMAGE_VEHICLE_CRAZY - RAGDOLL_DAMAGE_VEHICLE_FAST;

		float speedFactor = ( fPedEntRelVelMag - RAGDOLL_SPEED_VEHICLE_FAST ) / maxSpeedDiff;
		fDamage = RAGDOLL_DAMAGE_VEHICLE_FAST + ( speedFactor * maxDamageDiff );
	}
	else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_MEDIUM)
	{
		const float maxSpeedDiff = RAGDOLL_SPEED_VEHICLE_FAST - RAGDOLL_SPEED_VEHICLE_MEDIUM;
		const float maxDamageDiff = RAGDOLL_DAMAGE_VEHICLE_FAST - RAGDOLL_DAMAGE_VEHICLE_MEDIUM;

		float speedFactor = ( fPedEntRelVelMag - RAGDOLL_DAMAGE_VEHICLE_MEDIUM ) / maxSpeedDiff;
		fDamage = RAGDOLL_DAMAGE_VEHICLE_MEDIUM + ( speedFactor * maxDamageDiff );
	}
	else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_SLOW)
	{
		const float maxSpeedDiff = RAGDOLL_SPEED_VEHICLE_MEDIUM - RAGDOLL_SPEED_VEHICLE_SLOW;
		const float maxDamageDiff = RAGDOLL_DAMAGE_VEHICLE_MEDIUM - RAGDOLL_DAMAGE_VEHICLE_SLOW;

		float speedFactor = ( fPedEntRelVelMag - RAGDOLL_DAMAGE_VEHICLE_SLOW ) / maxSpeedDiff;
		fDamage = RAGDOLL_DAMAGE_VEHICLE_SLOW + ( speedFactor * maxDamageDiff );
	}
	else
	{
		fDamage = RAGDOLL_DAMAGE_VEHICLE_CRAWLING;
	}

	// Scale the damage based on which of three different weight classes the inflictor vehicle is in if the
	// vehicle isn't moving very slowly.
	float fVehicleMassDamageMult = RAGDOLL_DAMAGE_MULTIPLIER_LIGHT_VEHICLE;
	if(pVehicle->GetVelocity().Mag2() >= RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE*RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE)
	{
		float fVehMass = pVehicle->GetMass();
		if(fVehMass > RAGDOLL_DAMAGE_VEHICLE_MASS_HEAVY)
		{
			fVehicleMassDamageMult = RAGDOLL_DAMAGE_MULTIPLIER_HEAVY_VEHICLE;
		}
		else if(fVehMass > RAGDOLL_DAMAGE_VEHICLE_MASS_MEDIUM)
		{
			fVehicleMassDamageMult = RAGDOLL_DAMAGE_MULTIPLIER_MEDIUM_VEHICLE;
		}
	}
	fDamage *= fVehicleMassDamageMult;

	// Don't apply damage to peds which are standing when the closing speed between the vehicle and ped
	// is less than some threshold or when the vehicle is hardly moving unless the ped is under the vehicle.
	float fScalarProd = vPedNormal.Dot(-ZAXIS);
	bool bConsiderThisComponentUnderVeh = nComponent == RAGDOLL_BUTTOCKS
		|| (nComponent >= RAGDOLL_SPINE0 && nComponent <= RAGDOLL_SPINE3)
		|| nComponent == RAGDOLL_CLAVICLE_LEFT
		|| nComponent == RAGDOLL_CLAVICLE_RIGHT
		|| nComponent == RAGDOLL_NECK
		|| nComponent == RAGDOLL_HEAD;
	if(fScalarProd>RAGDOLL_DAMAGE_UNDER_VEHICLE_IMPACT_ANGLE_PARAM || !bConsiderThisComponentUnderVeh)
	{
		if( bPedStanding && 
			( fRelSpeed < RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE || 
			( pVehicle->GetVelocity().Mag2() < RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE * RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE &&
			fRelSpeed < RAGDOLL_DAMAGE_VEHICLE_SLOW ) ) )
		{
			// Always take some damage when hit while ragdolling to create the AI response
			if( pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE && pPed->GetPedType() == PEDTYPE_ANIMAL)
				fDamage = 0.000001f;
			else
				fDamage = 0.0;
		}
	}
	else
	{
		fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_UNDER_VEHICLE;
	}

	// Reduce damage if this ped is a player.
	if(pPed->IsPlayer())
	{
		fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_PLAYER_VEHICLE_SCALE;
	}

	return fDamage;
}


extern f32 g_HealthLostForLowPain;

void CCollisionEventScanner::ProcessRagdollImpact(CPed* pPed, float fMag, CEntity* pEntity, const Vector3& vPedNormal, int nComponent,
												  phMaterialMgr::Id nMaterialId)
{
	// TODO: Move these declarations back to their original place within the code when removing the old damage code.
	u32 weaponHash;
	float fDamage;
	float fKillFallHeight = KILL_FALL_HEIGHT;
	float fOriginalHealth;

	pPed->ProcessFallCollision();

	CPhysical* pPhysical = (pEntity && pEntity->GetIsPhysical()) ? static_cast<CPhysical*>(pEntity) : nullptr;

	// Don't want to take any damage from objects that we are attached to.
	if(pPhysical)
	{
		if(pPhysical->GetAttachmentExtension() && pPhysical->GetAttachmentExtension()->GetAttachParent() == pPed)
		{
			weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->ped is attached to %s[%p], ignoring damage", pEntity->GetModelName(), pEntity);
			return;
		}
	}

	// Work out the relative speed difference between this ped and pEntity from the impulse.
	float fRelSpeed = 0.0f;
	float fPedEntRelVelMag = 0.0f;
	float fPedEntTangVelMagSq = 0.0f;
	ComputeRelativeSpeedFromImpulse(pPed, pEntity, fMag, vPedNormal, fRelSpeed, fPedEntRelVelMag, fPedEntTangVelMagSq);

	// we can only process ragdoll impacts on clones when they are already running an NM task, otherwise the ped must wait for an update to be sure it
	// should be running an NM task. We also need to keep the task running, hence the HandleRagdollImpact method.
	if(pPed->IsNetworkClone() && pPhysical)
	{
		CTask* pTaskActive = pPed->GetPedIntelligence()->GetTaskActive();
		// if the ped is already running an NM control task, inform the task about the collision
		if(pTaskActive && pTaskActive->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
		{
			// No need to poke the task here. Local reactions should keep the ped in ragdoll
		}
		else
		{
			// pass damage events through to the dying task so that the dead ped reacts
			if (pTaskActive && pTaskActive->GetTaskType() == CTaskTypes::TASK_DYING_DEAD)
			{
				CDamageInfo damageInfo(WEAPONTYPE_FALL, NULL, NULL, NULL, 0.0f, NULL);
				pTaskActive->RecordDamage(damageInfo);
			}
		}
		return;
	}

	// Detect if our ped has fallen and if so whether the fall should be fatal.
	fKillFallHeight = KILL_FALL_HEIGHT;
	if(pPed->IsAPlayerPed())
		fKillFallHeight = PLAYER_KILL_FALL_HEIGHT;
	float fActualFallHeight = pPed->GetFallingHeight() - pPed->GetTransform().GetPosition().GetZf();
	// NB - If ped isn't attached (important as falling height is only useful if not attached) and has fallen more than
	// some threshold, set the damage to be fatal.
	bool bHasFallen = !pPed->GetAttachParent() && fActualFallHeight>RAGDOLL_DAMAGE_PED_FALL_HEIGHT_THRESHOLD;

	// As soon as the AI system turns off the "in air" flag, reset the fall height so we don't continue to take fall damage
	// while rolling around on the ground.
	if(bHasFallen && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ))
	{
		pPed->SetFallingHeight(pPed->GetTransform().GetPosition().GetZf());
	}

	// We want to be alter damage magnitude based on context like whether the ped is standing up.
	bool bPedStanding = true;
	Assert(pPed->GetPedIntelligence());
	if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL))
	{
		CTaskNMControl* pTask = smart_cast<CTaskNMControl*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
		bPedStanding = !pTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE);
	}

#if __BANK
	weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact--> %s has impacted with %s[%p]: bPedStanding - %s, bHasFallen - %s, fActualFallHeight - %.2f, fMag - %.2f, fRelSpeed - %.2f, fPedEntRelVelMag - %.2f, fPedEntTangVelMagSq - %.2f,",
		AILogging::GetDynamicEntityNameSafe(pPed), 
		pEntity ? (pEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pEntity)) : pEntity->GetModelName()) : "NULL",
		pEntity, AILogging::GetBooleanAsString(bPedStanding), AILogging::GetBooleanAsString(bHasFallen), fActualFallHeight, fMag, fRelSpeed, fPedEntRelVelMag, fPedEntTangVelMagSq);
#endif

	// Scale the damage appropriately for the entity type inflicting the damage:
	fDamage = 0.0f;
	fOriginalHealth = pPed->GetHealth();
	weaponHash = WEAPONTYPE_FALL;
	if(pEntity)
	{
		switch(pEntity->GetType())
		{
			//////////////////////
		case ENTITY_TYPE_VEHICLE:
			//////////////////////
			{
				weaponHash = WEAPONTYPE_RUNOVERBYVEHICLE;
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);

				// Don't want to take too much damage from stationary cars when a ragdoll is thrown into one.
				if(pVehicle->GetVelocity().Mag2() < 1.0f)
				{
					fPedEntRelVelMag = Min(fPedEntRelVelMag, RAGDOLL_SPEED_VEHICLE_SLOW);
				}

				switch(pVehicle->GetVehicleType())
				{
				case VEHICLE_TYPE_TRAIN:
					{
						// If ped hits train and train is moving over threshold 
						// ... then we work out a force proportional to the relative speed of the train to the ped
						if(pVehicle->GetVelocity().Mag2()>RAGDOLL_DAMAGE_TRAIN_VEL_THRESHOLD*RAGDOLL_DAMAGE_TRAIN_VEL_THRESHOLD
							&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain)
							&& pVehicle->GetPhysArch()&&pVehicle->GetPhysArch()->GetBound())
						{
							// Compare peds position with bounding box of train
							Vector3 vBoundSize = VEC3V_TO_VECTOR3(pVehicle->GetPhysArch()->GetBound()->GetBoundingBoxSize());
							Vector3 vLocalImpact = VEC3V_TO_VECTOR3(pVehicle->GetTransform().UnTransform(pPed->GetTransform().GetPosition()));
							// Apply much more damage when roughly at the front of the train.
							if(vLocalImpact.y+RAGDOLL_DAMAGE_TRAIN_BB_ADJUST_Y > (vBoundSize.y*0.5f) && rage::Abs(vLocalImpact.x)<vBoundSize.x*0.5f)
							{
								fDamage = fRelSpeed*DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
								fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_TRAIN;
							}
							else
							{
								// Base damage is calculated from the relative velocity of the inflictor vehicle. The vehicle's velocity
								// range is divided into 5 equal regions.
								if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_CRAZY)
								{
									fDamage = RAGDOLL_DAMAGE_VEHICLE_CRAZY;
								}
								else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_FAST)
								{
									fDamage = RAGDOLL_DAMAGE_VEHICLE_FAST;
								}
								else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_MEDIUM)
								{
									fDamage = RAGDOLL_DAMAGE_VEHICLE_MEDIUM;
								}
								else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_SLOW)
								{
									fDamage = RAGDOLL_DAMAGE_VEHICLE_SLOW;
								}
								else
								{
									fDamage = RAGDOLL_DAMAGE_VEHICLE_CRAWLING;
								}

								fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_TRAIN;
							}
						}
					}
					break;
				case VEHICLE_TYPE_PLANE:
					{
						// Base damage is calculated from the relative velocity of the inflictor vehicle. The vehicle's velocity
						// range is divided into 5 equal regions.
						if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_CRAZY)
						{
							fDamage = RAGDOLL_DAMAGE_VEHICLE_CRAZY;
						}
						else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_FAST)
						{
							fDamage = RAGDOLL_DAMAGE_VEHICLE_FAST;
						}
						else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_MEDIUM)
						{
							fDamage = RAGDOLL_DAMAGE_VEHICLE_MEDIUM;
						}
						else if(fPedEntRelVelMag > RAGDOLL_SPEED_VEHICLE_SLOW)
						{
							fDamage = RAGDOLL_DAMAGE_VEHICLE_SLOW;
						}
						else
						{
							fDamage = RAGDOLL_DAMAGE_VEHICLE_CRAWLING;
						}

						// Temporary fix to prevent ped from dying when colliding with wing.
						CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT);
						if(pTask && pTask->GetState() == CTaskExitVehicleSeat::State_JumpOutOfSeat)
						{
							return;
						}
						else
						{
							fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_PLANE;
						}

						// The "no ragdoll" poly flag is set on interiors of vehicles and can be used so that ragdolls don't take any
						// more damage than just falling on the ground normally.
						if(PGTAMATERIALMGR->GetPolyFlagNoRagdoll(nMaterialId))
						{
							fDamage = 0.0f;
							if(fPedEntRelVelMag > RAGDOLL_DAMAGE_MIN_SPEED_FALL
								&& !(nComponent==RAGDOLL_FOOT_LEFT || nComponent==RAGDOLL_FOOT_RIGHT || nComponent==RAGDOLL_SHIN_LEFT || nComponent==RAGDOLL_SHIN_RIGHT))
							{
								const CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
								Assert(pPedIntelligence);
								// Put a threshold on relative velocity too so that we don't accumulate damage from sliding along a stationary
								// object. Threshold is roughly defined by the vertical velocity an object can attain due to gravity in the
								// worst case frame rate.
								const float sfRelSpeedThreshold = 1.1f*CPhysics::GetGravitationalAcceleration()*(1.0f/15.0f);
								if(fRelSpeed > sfRelSpeedThreshold
									&& !pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_HIGH_FALL))
								{
									// Ped is sliding along a stationary object but with a total relative velocity which is high enough for damage.
									fDamage = fRelSpeed * RAGDOLL_DAMAGE_MULTIPLIER_GENERAL_COLLISION;
								}
							}
						}
					}
					break;
				case VEHICLE_TYPE_BIKE:
				case VEHICLE_TYPE_BICYCLE:
				case VEHICLE_TYPE_QUADBIKE:
				case VEHICLE_TYPE_BOAT:
				case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
					{
						/***** Damage for vehicles you can be knocked off *****/

						// B*2796249: Like the above comment suggests, we only want this to apply to jetski vehicles and not *all* boat types...
						if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && !pVehicle->GetIsJetSki())
						{
							break;
						}

						static float RAGDOLL_DAMAGE_MULTIPLIER_BIKE_IN_WATER = 0.1f;
						// Only generate damage for impacts with sufficiently horizontal impact normal. Peds which fall onto
						// a car's bonnet, roof, etc. will take damage from the fall code at the end.
						if(!bHasFallen)
						{
							float fScalarProd = vPedNormal.Dot(ZAXIS);
							if(fScalarProd<RAGDOLL_DAMAGE_VEHICLE_IMPACT_ANGLE_PARAM)
							{
								// Don't apply damage to peds which are standing when the closing speed between the vehicle and ped
								// is less than some threshold or when the vehicle is hardly moving unless the ped is under the vehicle.
								if(fScalarProd>RAGDOLL_DAMAGE_UNDER_VEHICLE_IMPACT_ANGLE_PARAM)
								{
									fDamage = fPedEntRelVelMag * (pVehicle->GetIsInWater() ? RAGDOLL_DAMAGE_MULTIPLIER_BIKE_IN_WATER : RAGDOLL_DAMAGE_MULTIPLIER_BIKE);
									if((bPedStanding && fRelSpeed<RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE)
										|| pVehicle->GetVelocity().Mag2()<RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE*RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE)
									{
										fDamage = 0.0f;
									}
								}
								else if(pVehicle->GetVehicleType()!=VEHICLE_TYPE_BIKE && !pPed->IsLocalPlayer())
								{
									// Only take damage for being under a quad, not an ordinary bike.
									fDamage = fRelSpeed*RAGDOLL_DAMAGE_MULTIPLIER_BIKE;
									fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_UNDER_VEHICLE;
								}
							}
							else
							{
								float ragdollDamageMinSpeedVehicle = RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE;

								if( pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE &&
									pVehicle->GetStatus() == STATUS_WRECKED )
								{
									ragdollDamageMinSpeedVehicle = RAGDOLL_DAMAGE_MIN_SPEED_WRECKED_BIKE;
								}
								if( fPedEntRelVelMag > ragdollDamageMinSpeedVehicle )
								{
									if(!pVehicle->GetIsInWater())
									{
										fDamage = fRelSpeed*RAGDOLL_DAMAGE_MULTIPLIER_BIKE;
									}
									else
									{
										fDamage = fRelSpeed*RAGDOLL_DAMAGE_MULTIPLIER_BIKE_IN_WATER;
									}
								}
							}
						}

						// We don't want fall damage if we are falling on a bike in water.
						if(pVehicle->GetIsInWater())
						{
							bHasFallen = false;
						}
					}
					break;
				default: // All other vehicle types handled here.
					{
						fDamage = ComputeDamageFromVehicle(pPed, pVehicle, nComponent, fPedEntRelVelMag, vPedNormal, bPedStanding, fRelSpeed);
					}
					break;
				} // End of vehicle-type switch statement.
				
				// Apply the weapon damage modifier
				const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(weaponHash);
				if(pItemInfo && pItemInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
				{
					const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pItemInfo);
					{
						fDamage *= pWeaponInfo->GetWeaponDamageModifier();
					}
				}

				// First time through here, we want the collision with the vehicle to perhaps knock any held props from our ped.
				if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE)
				{
					float fKnockOffProp = fwRandom::GetRandomNumberInRange(0.0, 1.0f);
					// More likely to drop the prop if hit harder.
					if(fMag > 30.0f)
						fKnockOffProp *= 2.0f;

					if(fKnockOffProp > 0.9f && !NetworkInterface::IsGameInProgress())
						CPedPropsMgr::KnockOffProps(pPed, true, true, true);
				}
				break;
			}

			//////////////////
		case ENTITY_TYPE_PED:
			//////////////////
			{
				// Set the weapon type to be unarmed - this is necessary to trigger appropriate facial animations.
				weaponHash = WEAPONTYPE_UNARMED;
				CPed* pInflictorPed = static_cast<CPed*>(pEntity);
				//if(pInflictorPed->IsPlayer() && (!pPed->GetAttacker() || pPed->GetAttacker() != pInflictorPed))
				//{
				//	if(!pPed->GetIsDeadOrDying() && pPed->GetSpeechAudioEntity())
				//	{
				//		// play some low pain
				//		audDamageStats damageStats;
				//		damageStats.Fatal = false;
				//		damageStats.RawDamage = g_HealthLostForLowPain + 1.0f;
				//		damageStats.DamageReason = AUD_DAMAGE_REASON_SHOVE;
				//		pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
				//	}
				//}
				// Only take damage from ragdolling peds flying through the air, not from animated peds running into other peds.
				fDamage = 0.0f;
				if(pInflictorPed->GetUsingRagdoll() && fRelSpeed>RAGDOLL_DAMAGE_MIN_SPEED_PED)
				{
					fDamage = fRelSpeed * RAGDOLL_DAMAGE_MULTIPLIER_PED;
				}
			}
			break;

			/////////////////////
		case ENTITY_TYPE_OBJECT:
			/////////////////////
			{
				// Only apply damage from objects (not static ones!) deemed heavy enough -- we don't want ped props to do damage.
				Assert(dynamic_cast<CObject*>(pEntity));
				// Don't take damage from certain props.
				if(pEntity->GetModelIndex()==MI_PED_PARACHUTE || static_cast<CObject*>(pEntity)->GetIsParachute())
				{
					fDamage = 0.0f;
					bHasFallen = false;
					break;
				}
				if(pEntity->GetCurrentPhysicsInst() && pEntity->GetCurrentPhysicsInst()->IsInLevel())
				{
					phInst* pPhysInst = pEntity->GetCurrentPhysicsInst();
					u32 nLevelIndex = pPhysInst->GetLevelIndex();
					if(CPhysics::GetLevel()->IsActive(nLevelIndex)
						&& pEntity->GetCollider()
						&& MagSquared(pEntity->GetCollider()->GetVelocity()).Getf() >= RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE*RAGDOLL_DAMAGE_MIN_SPEED_VEHICLE)
					{
						float fMass = static_cast<CObject*>(pEntity)->GetMass();
						if(fMass > 10000.0f)
						{
							fDamage = fRelSpeed * RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_HEAVY;
							break;
						}
						else if(fMass > RAGDOLL_DAMAGE_MIN_OBJECT_MASS)
						{
							fDamage = fRelSpeed * RAGDOLL_DAMAGE_MULTIPLIER_OBJECT_LIGHT;
							break;
						}
					}
					else
					{
						// INTENTIONAL FALL-THROUGH TO BUILDING DAMAGE BECAUSE WE HAVE HIT A STATIC OBJECT.
					}
				}
			}
			////////////////////////////////
		case ENTITY_TYPE_BUILDING:
		case ENTITY_TYPE_ANIMATED_BUILDING:
		case ENTITY_TYPE_MLO:
			////////////////////////////////
			{
				// We haven't fallen if we are in here. Only inflict damage if we hit something really hard (not with feet).
				fDamage = 0.0f;
				if(fPedEntRelVelMag > RAGDOLL_DAMAGE_MIN_SPEED_FALL
					&& !(nComponent==RAGDOLL_FOOT_LEFT || nComponent==RAGDOLL_FOOT_RIGHT || nComponent==RAGDOLL_SHIN_LEFT || nComponent==RAGDOLL_SHIN_RIGHT))
				{
					const CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
					Assert(pPedIntelligence);
					// Put a threshold on relative velocity too so that we don't accumulate damage from sliding along a stationary
					// object. Threshold is roughly defined by the vertical velocity an object can attain due to gravity in the
					// worst case frame rate.
					const float sfRelSpeedThreshold = 1.1f*CPhysics::GetGravitationalAcceleration()*(1.0f/15.0f);
					if(fRelSpeed > sfRelSpeedThreshold
						&& (!pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_HIGH_FALL)
						|| pPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting) ) )
					{
						// Ped is sliding along a stationary object but with a total relative velocity which is high enough for damage.
						fDamage = fRelSpeed * RAGDOLL_DAMAGE_MULTIPLIER_GENERAL_COLLISION;
					}
				}

				if(!bPedStanding && !(nComponent==RAGDOLL_FOOT_LEFT || nComponent==RAGDOLL_FOOT_RIGHT)
					&& fPedEntTangVelMagSq > RAGDOLL_DAMAGE_MIN_SPEED_SLIDING_DAMAGE*RAGDOLL_DAMAGE_MIN_SPEED_SLIDING_DAMAGE)
				{
					// Ped is sliding along a stationary object with most of its velocity along the collision entity.
					fDamage += fPedEntTangVelMagSq * RAGDOLL_DAMAGE_MULTIPLIER_SLIDING_COLLISION;

					// We still want to generate a small amount of rumble on the control pad even when not taking damage from sliding.
					if(pPed->IsLocalPlayer())
					{
						const u32 knRumbleDuration = 10;
						const u32 knRumbleDelay = 0;
						const float kfRumbleIntensityScale = 0.1f;
						CControlMgr::StartPlayerPadShakeByIntensity(knRumbleDuration, kfRumbleIntensityScale, knRumbleDelay);
					}
				}
			}
			break;

		default:
			{
				Assertf(false, "Unhandled damage inflictor type: %d", pEntity->GetType());
				fDamage = fRelSpeed * RAGDOLL_DAMAGE_MULTIPLIER_PED;
			}
			break;
		}
	}

#if __BANK
	weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->hit entity %s[%p] processed. Initial damage - %.2f",
		pEntity ? (pEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pEntity)) : pEntity->GetModelName()) : "NULL", pEntity, fDamage);
#endif

	// Do fall processing here as the ped could fall onto any entity type.
	if(bHasFallen)
	{
		if(fActualFallHeight>fKillFallHeight &&
			!pPed->GetIsInWater() && // GTAV - B*1853590 - Don't just kill the ped if they've fallen into water.
			!pPed->GetHighFallInstantDeathDisabled() ) // GTAV - B*2410881 - Script want to be able to disable this
		{
			fDamage = 100.0f*pPed->GetHealth();
			fKillFallHeight = -1.0f;
		}
		else if(fPedEntRelVelMag > RAGDOLL_DAMAGE_MIN_SPEED_FALL)
		{
			fDamage = fActualFallHeight;
			// Ped has landed on their feet after a fall. Want to give less damage in this case. General fall damage will be done in
			// the "else" block.
			if((pEntity==NULL || pEntity->GetIsTypeBuilding())
				&& (nComponent==RAGDOLL_FOOT_LEFT || nComponent==RAGDOLL_FOOT_RIGHT || nComponent==RAGDOLL_SHIN_LEFT || nComponent==RAGDOLL_SHIN_RIGHT))
			{
				fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_FALL_LAND_FOOT;
				weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->applying 'land on foot' modifier to damage");
			}
			else
			{
				fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_FALL;
				weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->applying 'fall' modifier to damage");
			}

		}

		weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->ped has fallen resulting in %.2f damage. fPedEntRelVelMag - %.2f, fActualFallHeight - %.2f, fKillFallHeight - %.2f, inWater - %s, FallInstantDeathDisabled - %s", 
			fDamage, fPedEntRelVelMag, fActualFallHeight, fKillFallHeight,
			pPed->GetIsInWater() ? "TRUE" : "FALSE",
			pPed->GetHighFallInstantDeathDisabled() ? "TRUE" : "FALSE");

		// Reset the fall height so we don't continue to take fall damage while rolling around on the ground.
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir))
		{
			pPed->SetFallingHeight(pPed->GetTransform().GetPosition().GetZf());
		}
	}
	else if(fDamage < 0.0f)
	{
		fDamage = 0.0f;
	}

	// Perform any scaling for special case ped types.
	if(pPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		if (!pPed->IsAPlayerPed())
		{
			fDamage *= RAGDOLL_DAMAGE_MULTIPLIER_ANIMAL_SCALE;
			weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->applying 'animal' modifier to damage");
		}
	}
	// Random peds hit by cars are always fatigued
	else 
	{
		// Scale the computed damage value per component.
		Assertf(nComponent >= 0 && nComponent < RAGDOLL_NUM_COMPONENTS, "Invalid ragdoll component: %d.", nComponent);
		fDamage *= RAGDOLL_COMPONENT_SCALES[nComponent];
		weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->applying 'component scale' modifier to damage, nComponent - %i", nComponent);

		if( pEntity && pEntity->GetType() == ENTITY_TYPE_VEHICLE && !pPed->PopTypeIsMission() )
		{
			if( pPed->GetHealth() > pPed->GetFatiguedHealthThreshold() )
			{
				fDamage = MAX(fDamage, (pPed->GetHealth() - pPed->GetFatiguedHealthThreshold()) + 1.0f );
				weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->updating damage when hit a vehicle to %.2f", fDamage);
			}
		}
	}
	
	// We will scale the damage for players based on their strength stat. Maximum strength => 90% of total computed damage.
	if(pPed->IsLocalPlayer())
	{
		float fStrengthStatDamageScale = 1.0f;
		const float fStrengthStatDamageMaxReductionFactor = 0.1f;

		float fStrengthValue = 1.0f - Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
		fStrengthStatDamageScale = 1.0f - fStrengthStatDamageMaxReductionFactor*fStrengthValue;
		fDamage *= fStrengthStatDamageScale;
		weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->applying 'player strenght scale' modifier to damage. fStrengthStatDamageScale - %.2f", fStrengthStatDamageScale);
	}

	// Scale the damage if we have been thrown from a bike due to exhaustion.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrownFromVehicleDueToExhaustion))
	{
		// Go easy on this ped for some number of milliseconds after falling off.
		const u32 nReducedDamageWhileExhaustedTimePeriod = 2000;

		u32 nTimeNow = fwTimer::GetTimeInMilliseconds();
		if(nTimeNow - pPed->GetExhaustionDamageTimer() > nReducedDamageWhileExhaustedTimePeriod)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ThrownFromVehicleDueToExhaustion, false);
		}
		else
		{
			// x -> x^3 but scaled so that damage of 100.0f is still 100.0f.
			fDamage = fDamage*fDamage*fDamage / 10000.0f;
		}
	}

	// Check if this ped is set to be immune to collision damage.
	if(pPed->m_nPhysicalFlags.bNotDamagedByCollisions || pPed->m_nPhysicalFlags.bNotDamagedByAnything ||
		(pEntity && pEntity->GetIsTypePed() && pPed->GetPedResetFlag(CPED_RESET_FLAG_NoCollisionDamageFromOtherPeds)))
	{
		fDamage = 0.0f;
		weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->reducing this damage to 0.f due to being immune to collion damage: bNotDamagedByCollisions - %s, bNotDamagedByAnything - %s, _NoCollisionDamageFromOtherPeds - %s",
			pPed->m_nPhysicalFlags.bNotDamagedByCollisions ? "TRUE":"FALSE",
			pPed->m_nPhysicalFlags.bNotDamagedByAnything ? "TRUE" : "FALSE",
			pPed->GetPedResetFlag(CPED_RESET_FLAG_NoCollisionDamageFromOtherPeds) ? "TRUE" : "FALSE");
	}

	//Process bumps.
	ProcessBumps(*pPed);
	
	//Helper flags to detect the collision type.
	bool bHitByGround = pEntity && (pEntity == pPed->GetGroundPhysical());
	bool bHitByVehicle = !bHitByGround && pEntity && pEntity->GetIsTypeVehicle();
	bool bHitByObject = !bHitByGround && pEntity && pEntity->GetIsTypeObject();

	// Ragdolls don't currently register a ground physical.  So instead, try to figure out via other means if the ragdoll is on a train or boat.
	if (bHitByVehicle)
	{
		CVehicle *pVehicle = static_cast<CVehicle *>(pEntity);
		if (pVehicle && (pVehicle->InheritsFromTrain() || pVehicle->InheritsFromBoat()) && pPed->GetCollider())  
		{
			static float fSmallVelDif = 1.0f;
			Vector3 vUp(0.0f,0.0f,1.0f);
			Vector3 vTrainPedVelDif = pVehicle->GetVelocity() - pPed->GetVelocity();
			Vector3 vTrainPedRefVelDif = pVehicle->GetVelocity() - RCC_VECTOR3(pPed->GetCollider()->GetReferenceFrameVelocity());
			if (vTrainPedVelDif.Mag2() < fSmallVelDif || vTrainPedRefVelDif.Mag2() < fSmallVelDif || vPedNormal.Dot(vUp) > 0.9f)
			{
				bHitByVehicle = false;
				bHitByGround = true;
			}
		}
	}

	// Determine which entity instigated this damage, if any
	CEntity* pImpactDamageInstigator = CalculateRagdollImpactDamageInstigator(pPed, pPhysical, weaponHash, bHasFallen);

	// Don't allow similar damage to get applied repeatedly over a short amount of time from the same source.
	// This is an attempt at determining the most important impact from a particular source over a short amount of time that
	// isn't frame-rate dependent.
	if (pEntity && pPed->GetRagdollInst())
	{
		if (pPed->GetRagdollInst()->GetLastImpactDamageEntity() == pEntity && pPed->GetRagdollInst()->GetTimeSinceImpactDamage() < RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT)
		{
			float newDamage = fDamage - pPed->GetRagdollInst()->GetAccumulatedImpactDamageFromLastEntity();
			if (newDamage > 0.0f)
			{
				pPed->GetRagdollInst()->SetAccumulatedImpactDamageFromLastEntity(fDamage + newDamage);
				fDamage = newDamage;
				weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->already applied damage from %s recently. reducing this damage to %.2f", pEntity->GetModelName(), fDamage);
			}
			else
			{
				// We've already applied more than the new damage from this entity over the past <RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT> milliseconds, so ignore this new damage
				weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->already applied damage from %s recently. reducing this damage to 0.f", pEntity->GetModelName());
				fDamage = 0.0f;
			}
		}
		else
		{
			// Note the new entity and damage taken from it
			pPed->GetRagdollInst()->SetImpactDamageEntity(pEntity);
			pPed->GetRagdollInst()->SetAccumulatedImpactDamageFromLastEntity(fDamage);
		}
	}
	
	// don't need to do anything further if damage is zero and ped isn't currently activating
	if(fDamage <= 0.0f && pPed->GetRagdollState()!=RAGDOLL_STATE_PHYS_ACTIVATE)
	{
		weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->ignore negative damage (%.2f) as ped is not active", fDamage);
		return;
	}

	//Create the damage event.
	CEventDamage tempDamageEvent(pImpactDamageInstigator, fwTimer::GetTimeInMilliseconds(), weaponHash);
	
	//Apply the damage.
	weaponDebugf3("CCollisionEventScanner::ProcessRagdollImpact-->invoke ApplyDamageAndComputeResponse: damage - %.2f, hitComponent[%d]", fDamage, nComponent);
	CPedDamageCalculator damageCalculator(pImpactDamageInstigator, fDamage, weaponHash, nComponent, false);
	damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);

    if (bHitByVehicle)
    {
        CVehicle* veh = (CVehicle*)pEntity;
        CPlayerSpecialAbilityManager::ChargeEvent(ACET_RAN_OVER_PED, veh->GetDriver(), fDamage);
    }

#if __DEV
	static bool PRINT_RESULTS = false;
	if(PRINT_RESULTS)
		Displayf("ped (%d) health = %3.2f\n", CPed::GetPool()->GetIndex(pPed), pPed->GetHealth());
#endif

	bool bWasKilledOrInjured = (tempDamageEvent.GetDamageResponseData().m_bKilled || tempDamageEvent.GetDamageResponseData().m_bInjured);

	if(fKillFallHeight < 0.0f && tempDamageEvent.GetDamageResponseData().m_bKilled && fOriginalHealth > 0.0f)
	{
		if (fDamage>0.0f)
		{
			g_vfxBlood.TriggerPtFxFallToDeath(pPed);
			pPed->GetPedAudioEntity()->TriggerFallToDeath();
		}
	}
	else if(fDamage >= pPed->GetHealth())
	{
		pPed->GetPedAudioEntity()->PedFellToDeath();
	}


	if(!tempDamageEvent.AffectsPed(pPed))
	{
		// ped doesn't want to react to damage - if ragdoll has already activated though, we will need to deal with it.
		if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE)
			;
		else
			return;
	}

	if (bHitByVehicle)
	{
		//Setting up Custom Idle for after recovery (B* 810427)
		//Possibly data drive this in the future if these become more prevalent
		static fwMvClipSetId s_HitByCarSet("reaction@shake_it_off",0xC6AE9A3E);	
		static fwMvClipId s_HitByCarClip("dustoff",0xC9A0759E);
		pPed->GetMotionData()->RequestCustomIdle(s_HitByCarSet, s_HitByCarClip);
	}

	// Determine if we need a new ragdoll task to handle this impact
	int iRagdollTaskPriority = 0;

	if (pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
	{
		// We're already in ragdoll. Ask the existing nm behaviour if it wants to handle this impact itself
		CTask* pTaskSimplest = pPed->GetPedIntelligence()->GetTaskActiveSimplest();

		if (pTaskSimplest)
		{
			if (pTaskSimplest->IsNMBehaviourTask())
			{
				CTaskNMBehaviour* pRunningNmTask = smart_cast<CTaskNMBehaviour*>(pTaskSimplest);
				if (pRunningNmTask->HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId))
				{
					iRagdollTaskPriority = E_PRIORITY_MAX;
				}
			}

			if (iRagdollTaskPriority == 0)
			{
				// If we're being 'hit' by a slow moving vehicle and are already running some kind of task that handles ragdoll then
				// no need to generate a new task
				static const float s_fMinSpeedNeedsRagdollTask2 = square(2.0f);
				if (bHitByVehicle && static_cast<const CVehicle*>(pEntity)->GetVelocity().Mag2() < s_fMinSpeedNeedsRagdollTask2 && pTaskSimplest->HandlesRagdoll(pPed))
				{
					iRagdollTaskPriority = E_PRIORITY_MAX;
				}
			}
		}
		else
		{
			// The ragdoll must have just activated in the last update
			// We'll need to add a new event, unless we've already added one
			for (int i = 0; i < pPed->GetPedIntelligence()->GetNumEventsInQueue(); i++)
			{
				const CEvent* pEvent = pPed->GetPedIntelligence()->GetEventByIndex(i);
				if (pEvent->RequiresAbortForRagdoll() && !pEvent->HasExpired() && pEvent->GetEventPriority() > iRagdollTaskPriority)
				{
					iRagdollTaskPriority = pEvent->GetEventPriority();
				}
			}
		}
	}
	else if (pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE)
	{
		// The ragdoll must have just activated in the last update
		// We'll need to add a new event, unless we've already added one
		for (int i = 0; i < pPed->GetPedIntelligence()->GetNumEventsInQueue(); i++)
		{
			const CEvent* pEvent = pPed->GetPedIntelligence()->GetEventByIndex(i);
			if (pEvent->RequiresAbortForRagdoll() && !pEvent->HasExpired() && pEvent->GetEventPriority() > iRagdollTaskPriority)
			{
				iRagdollTaskPriority = pEvent->GetEventPriority();
			}
		}
	}

	// If we already have a ragdoll task, we don't need to do any of the rest of this function.
	if(iRagdollTaskPriority >= tempDamageEvent.GetEventPriority())
		return;

	CTask* pTaskResponse = NULL;
	if(bWasKilledOrInjured && (fDamage > 0.0f || pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE))
	{
		CTask* pTaskActive = pPed->GetPedIntelligence()->GetTaskActive();
		CDamageInfo damageInfo(weaponHash, NULL, NULL, NULL, 0.0f, NULL);
		CTask* pNMTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL);
		// If an NM task isn't active, check if there's an event in the queue that is about to start an NM task
		if (!pNMTask)
		{
			CEventDamage *damageEvent = static_cast<CEventDamage*>(pPed->GetPedIntelligence()->GetEventOfType(EVENT_DAMAGE));
			if (damageEvent && !damageEvent->HasExpired() && damageEvent->GetPhysicalResponseTask() && damageEvent->GetPhysicalResponseTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
			{
				pNMTask = (CTask*) damageEvent->GetPhysicalResponseTask();
			}
		}
		// sconde (4/19/2013) This logic seems broken to me...
		// We are essentially checking to see if there is already a damage event with a physical response task that has the ability to handle a ragdoll reaction (pNMTask->HandlesRagdoll(pPed))
		// If there we find a damage event that handles ragdoll reactions we don't create a task response for the 'death' damage event even though it has higher priority than non-'death' ragdoll damage events
		// This means that when we go to handle the events we'll be handling a 'death' event that doesn't have a ragdoll response - essentially leaving a ragdoll without a ragdoll task!
		if( (pNMTask && pNMTask->HandlesRagdoll(pPed)) || (pTaskActive && (pTaskActive->RecordDamage(damageInfo))))
		{
			// Do nothing more, the task has handled the damage response
		}
		else
		{
			// If the ped is on fire relax for only a short period of time,
			// this will give CTaskComplexDie the opportunity to switch back
			// to onFire quickly.
			if(g_fireMan.IsEntityOnFire(pPed))
			{
				//Displayf(__FILE__ " Ln%d CTaskNMRelax: was killed or injured (damage %.3f, ragdoll state %d) and is on fire/n", __LINE__, fDamage, (int)pPed->GetRagdollState());
				nmEntityDebugf(pPed, "CCollisionEventScanner::ProcessRagdollImpact - Adding CTaskNMRelax for fatal collision on flaming ped");
				pTaskResponse = rage_new CTaskNMRelax(250, 500, 25.0f, 0.75f);
			}
			else
			{
				if(IsPlayerOrPlayersVehicle(pEntity) || pPed->IsPlayer())
				{
					//pTaskResponse = rage_new CTaskNMRollUpAndRelax(2000, 60000, fwRandom::GetRandomNumberInRange(1000, 3000), -1, 1.0f);
					nmEntityDebugf(pPed, "CCollisionEventScanner::ProcessRagdollImpact - Adding CTaskNMBrace for fatal collision with player vehicle or player");
					Vector3 vPedVelocity(pPed->GetVelocity());
					if (MagSquared(pPed->GetGroundVelocityIntegrated()).Getf() > vPedVelocity.Mag2())
					{
						vPedVelocity = VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());
					}
					pTaskResponse = rage_new CTaskNMBrace(1000, 60000, pEntity, CTaskNMBrace::BRACE_DEFAULT, vPedVelocity);
				}
				else
				{
					//Displayf(__FILE__ " Ln%d CTaskNMRelax: was killed or injured (damage %.3f, ragdoll state %d)/n", __LINE__, fDamage, (int)pPed->GetRagdollState());
					nmEntityDebugf(pPed, "CCollisionEventScanner::ProcessRagdollImpact - Adding CTaskNMRelax for fatal collision with non player vehicle and non player");
					pTaskResponse = rage_new CTaskNMRelax(2000, 60000, -1, 1.0f);
				}
			}
		}
	}
	else
	{
		CEvent* pCurrentEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS_ACTIVATE || ( bHitByVehicle && (!pCurrentEvent || pCurrentEvent->GetEventType()!=EVENT_DAMAGE)))
		{
			bool bHitByVehicleOrObject = bHitByVehicle || bHitByObject;
			CEntity* pHitByEntity = NULL;

			CPed* pLocalPlayer = NULL;

			// If we do have a collision scanningEntity that is a player or vehicle we
			// are happy, otherwise we do a search for better options.
			if (pEntity && ((pEntity->GetIsTypePed() && ((CPed*)pEntity)->IsPlayer()) || bHitByVehicleOrObject))
			{
				pHitByEntity = pEntity;
			}
			else
			{
				// Search a collision record involving a player or player car.
				const CCollisionRecord* pColRecord = NULL;
					
				for(pColRecord = pPed->GetFrameCollisionHistory()->GetFirstPedCollisionRecord();
					pColRecord != NULL ; pColRecord = pColRecord->GetNext())
				{
					CEntity* pCollisionEntity = pColRecord->m_pRegdCollisionEntity.Get();

					if(pCollisionEntity != NULL && static_cast<const CPed*>(pCollisionEntity)->IsPlayer())
					{
						//- look at the player and balance
						pHitByEntity = pCollisionEntity;
						bHitByVehicleOrObject = false;
						break;
					}
				}
		
				if( pHitByEntity == NULL )
				{
					for(pColRecord = pPed->GetFrameCollisionHistory()->GetFirstVehicleCollisionRecord();
						pColRecord != NULL ; pColRecord = pColRecord->GetNext())
					{
						CEntity* pCollisionEntity = pColRecord->m_pRegdCollisionEntity.Get();

						const CPed* pDriver = static_cast<CVehicle*>(pCollisionEntity)->GetDriver();

						if(pDriver && pDriver->IsPlayer())
						{
							//- brace against the vehicle
							pHitByEntity = pCollisionEntity;
							bHitByVehicleOrObject = true;
							break;
						}
					}
				}

				// Not successful yet?
				if(!pHitByEntity)
				{
					static float DIST_THRESHOLD_CAR_2 = 5.0f * 5.0f;
					static float DIST_THRESHOLD_PLAYER_2 = 1.28f * 1.28f;

					const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					// Search for a player car close by
					pLocalPlayer = FindPlayerPed();
					const Vector3 vLocalPlayerPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());

					if(pPed == pLocalPlayer || (pLocalPlayer && pLocalPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )
						&& (vLocalPlayerPos.Dist2(vPedPos) < DIST_THRESHOLD_CAR_2)))
					{
						static float TEST_FOR_CAR_RADIUS = 0.6f;
						static u32 INCLUDE_FLAGS = ArchetypeFlags::GTA_VEHICLE_TYPE;

						phIntersection intersection;

						if(CPhysics::GetLevel()->TestSphere(vPedPos, TEST_FOR_CAR_RADIUS, &intersection, pPed->GetRagdollInst(), INCLUDE_FLAGS))
						{
							//- brace against the vehicle
							pHitByEntity = CPhysics::GetEntityFromInst(intersection.GetInstance());
							bHitByVehicleOrObject = true;
						}
					}

					// Search for a player close by
					if (!pHitByEntity && pLocalPlayer && pPed!=pLocalPlayer
						&& (vLocalPlayerPos.Dist2(vPedPos) < DIST_THRESHOLD_PLAYER_2))
					{
						pHitByEntity = pLocalPlayer;
						bHitByVehicleOrObject = false;
					}
				}
			}

			if(!pHitByEntity)
				pHitByEntity = pEntity;

			bool bRollUp = false;

			// If we are blending from NM we check if we should balance/brace
			// or rollUp.
			const CTaskBlendFromNM* const pTask = static_cast<const CTaskBlendFromNM*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BLEND_FROM_NM));
			if(pTask != NULL && pTask->ShouldRollupOnRagdollImpact())
			{
				bRollUp = true;
			}

			// rollUp or balance ?
			if(bRollUp)
			{
				nmEntityDebugf(pPed, "CCollisionEventScanner::ProcessRagdollImpact - Adding CTaskNMRollUpAndRelax for non-fatal hit");
				pTaskResponse = rage_new CTaskNMRollUpAndRelax(2000, 60000, fwRandom::GetRandomNumberInRange(1000, 3000), -1, 1.0f);

				CFacialDataComponent* pFacialData = pPed->GetFacialData();
				if(pFacialData)
				{
					pFacialData->PlayPainFacialAnim(pPed);
				}
			}
			else
			{
				//CTaskSitIdle* pTaskSit = (CTaskSitIdle*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SIT_IDLE);
				//if(pTaskSit && pHitByEntity && pHitByEntity->GetIsTypePed())
				//{
				//	pTaskResponse = rage_new CTaskNMSit(2000, 60000, pTaskSit->GetClipSet(), pTaskSit->GetClipId(), VEC3_ZERO, pHitByEntity, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
				//}
				//else 
				if(bHitByVehicleOrObject)
				{
					Assert(pHitByEntity);

					nmEntityDebugf(pPed, "CCollisionEventScanner::ProcessRagdollImpact - Adding CTaskNMBrace for non-fatal hit with vehicle or object");
					Vector3 vPedVelocity(pPed->GetVelocity());
					if (MagSquared(pPed->GetGroundVelocityIntegrated()).Getf() > vPedVelocity.Mag2())
					{
						vPedVelocity = VEC3V_TO_VECTOR3(pPed->GetGroundVelocityIntegrated());
					}
					pTaskResponse = rage_new CTaskNMBrace(1000, 60000, pHitByEntity, CTaskNMBrace::BRACE_DEFAULT, vPedVelocity);

					/// set weak option based on agility and health
					static float fHealthThresholdMissionCharPlayer = 110.0f, fHealthThresholdOther = 140.0f;
					const float fHealthThreshold = (pPed->IsPlayer() || (pPed->PopTypeIsMission()) ? fHealthThresholdMissionCharPlayer : fHealthThresholdOther);

					if (CTaskNMBehaviour::ms_bUseParameterSets && (!pPed->CheckAgilityFlags(AF_RAGDOLL_BRACE_STRONG) || (pPed->GetHealth() < fHealthThreshold))) {
						((CTaskNMBrace*)pTaskResponse)->SetType(CTaskNMBrace::BRACE_WEAK);
					}
				}
				else
				{
					Assert(!bHitByVehicleOrObject);

					u32 grabFlags = 0;
					if (!CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer || !pPed->IsPlayer())
					{
						grabFlags|=CGrabHelper::TARGET_AUTO_WHEN_FALLING;
					}

					nmEntityDebugf(pPed, "CCollisionEventScanner::ProcessRagdollImpact - Adding CTaskNMBalance for non-fatal hit");
					pTaskResponse = rage_new CTaskNMBalance(250, 60000, pHitByEntity, grabFlags );

					if(!pHitByEntity)
					{
						Vector3 vColNormal = vPedNormal;
						vColNormal.z = 0.0f;
						vColNormal *= 3.0f;

						((CTaskNMBalance*)pTaskResponse)->SetLookAtPosition(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vColNormal);
					}

					/// do facials (100%/50% angry, 25% pain, 25% shocked) on the player or if bumped into by a player
					CFacialDataComponent* pFacialData = pPed->GetFacialData();
					if(pFacialData)
					{
						if (pPed->IsPlayer())
						{
							pFacialData->PlayFacialAnim(pPed, FACIAL_CLIP_ANGRY);
						}
						else if (pHitByEntity && (pHitByEntity->GetIsTypePed() && ((CPed*)pHitByEntity)->IsPlayer()))
						{
							if(fwRandom::GetRandomTrueFalse())
							{
								if(fwRandom::GetRandomTrueFalse())
									pFacialData->PlayPainFacialAnim(pPed);
								else
									pFacialData->PlayFacialAnim(pPed, FACIAL_CLIP_SHOCKED);
							}
							else
							{
								pFacialData->PlayFacialAnim(pPed, FACIAL_CLIP_ANGRY);
							}
						}
					}
				}
			}
		}
	}

	// if there's no response, then there's no need to add the event
	if(pTaskResponse)
	{
		if(pTaskResponse->GetTaskType() == CTaskTypes::TASK_NM_BALANCE) {
			static_cast<CTaskNMBalance*>(pTaskResponse)->EvaluateAndSetType(pPed);
		}

		// if ped is going to accept the damage response - fine, send that
		if(tempDamageEvent.AffectsPed(pPed))
		{
			{
				// And the event and physical response (if one was found).
				CEvent* pEventAdded = pPed->GetPedIntelligence()->AddEvent(tempDamageEvent);
				if(pEventAdded && pEventAdded != &tempDamageEvent 
					&& pTaskResponse
					&& pEventAdded->GetEventType()==EVENT_DAMAGE)
				{
					// We need to wrap the response in a complexNM task.
					CTask* pTempResponse = pTaskResponse;
					pTaskResponse = rage_new CTaskNMControl(1000, 10000, pTempResponse, CTaskNMControl::DO_BLEND_FROM_NM, fDamage);
					
					nmDebugf2("[%u] ProcessRagdollImpact:%s(%p) Added event: %s, %s", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, pEventAdded->GetName().c_str(), pTempResponse->GetTaskName());

					((CEventDamage*)pEventAdded)->SetPhysicalResponseTask(pTaskResponse, true);
				}
				else if(pTaskResponse)
				{
					delete pTaskResponse;
					pTaskResponse = nullptr;
				}
			}
		}
		// if not then force it through with a switch event
		else
		{
			{
				CEventSwitch2NM  event(60000, pTaskResponse);
				pPed->GetPedIntelligence()->AddEvent(event);
				nmDebugf2("[%u] ProcessRagdollImpact:%s(%p) Added event: %s, %s", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, event.GetName().c_str(), pTaskResponse->GetTaskName());
				// think we might be in trouble if we get in here with a dead ped, because it won't trigger the complex die task
				Assert(!bWasKilledOrInjured);
			}
		}
	}
	else
	{
		CTaskNMControl* pTaskNM = smart_cast<CTaskNMControl*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
		if(pTaskNM)
			pTaskNM->SetDamageDone(fDamage);
	}

#if CNC_MODE_ENABLED
	if (pTaskResponse && pPed->GetPedIntelligence()->GetTaskActive()->GetTaskType() == CTaskTypes::TASK_INCAPACITATED)
	{
		if (tempDamageEvent.GetEventPriority() > E_PRIORITY_INCAPACITATED && pTaskResponse->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
		{
			CTaskIncapacitated* pIncapacitationTask = (CTaskIncapacitated*)pPed->GetPedIntelligence()->GetTaskActive();
			pIncapacitationTask->SetForcedNaturalMotionTask(pTaskResponse);
		}
	}
#endif //CNC_MODE_ENABLED
}

CEntity* CCollisionEventScanner::CalculateRagdollImpactDamageInstigator(const CPed* pPed, CPhysical* pImpactInflictor, const u32 uWeaponHash, bool bHasFallen) const
{
	CEntity* pInstigator = pImpactInflictor;

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_UseExtendedRagdollCollisionCalculator))
	{
		// Calculate relative velocity to determine if oncoming vehicle collided with falling ped
		if (pImpactInflictor && pImpactInflictor->GetIsTypeVehicle())
		{
			// Assume vehicle is not instigator by default
			pInstigator = nullptr;

			const Vector3& vDamagerVelocity = pImpactInflictor->GetVelocity();

			// Only consider vehicle as instigator if it is moving
			if (vDamagerVelocity.IsNonZero() && vDamagerVelocity.Mag2() >= square(RAGDOLL_SPEED_VEHICLE_SLOW))
			{
				// Evaluate damager velocity and impact normal; vehicle is only the instigator if moving towards impact
				const CCollisionRecord* pCollisionRecord = PhysicsHelpers::GetCollisionRecord(pImpactInflictor, pPed);
				if (pCollisionRecord && pCollisionRecord->m_MyCollisionNormal.IsNonZero())
				{
					const float fAngleVectorsPointSameDirectionRadians = PI * 0.25f; // 45 degrees

					Vector3 vDamagerVelocityNorm = vDamagerVelocity;
					vDamagerVelocityNorm.NormalizeFast();

					if (DotProduct(pCollisionRecord->m_MyCollisionNormal, vDamagerVelocityNorm) <= fAngleVectorsPointSameDirectionRadians)
					{
						// Vehicle collided with ragdoll at sufficient speed in direction of travel
						return pImpactInflictor;
					}
				}
			}
		}

		if (bHasFallen)
		{
			// If present, assign blame to last entity this ragdoll collided with
			if (pPed->GetRagdollInst())
			{
				CEntity* pLastImpactEntity = pPed->GetRagdollInst()->GetLastImpactDamageEntity();
				// ProcessRagdollImpact may be called multiple times during a collision.
				// Assume that we can't be colliding with the same entity that caused us to ragdoll
				if (pLastImpactEntity && pLastImpactEntity != pImpactInflictor)
				{
					return pLastImpactEntity;
				}
			}

			// Ped has fallen with no known instigator, treat this as self-inflicted
			pInstigator = nullptr;
		}
	}

	// If we are hit by a ped reacting to an impact from another entity, we consider that entity to be our instigator
	if (pImpactInflictor && pImpactInflictor->GetIsTypePed() && uWeaponHash == WEAPONTYPE_UNARMED)
	{
		CPed* pInflictorPed = SafeCast(CPed, pImpactInflictor);
		const CPedIntelligence* pInflictorPedIntelligence = pInflictorPed->GetPedIntelligence();
		if (pInflictorPedIntelligence->GetLowestLevelNMTask(pInflictorPed))
		{
			const CEvent* pInflictorCurrentEvent = pInflictorPedIntelligence->GetCurrentEvent();
			if (pInflictorCurrentEvent && pInflictorCurrentEvent->GetEventType() == EVENT_DAMAGE)
			{
				const CEventDamage* pInflictorCurrentDamageEvent = SafeCast(const CEventDamage, pInflictorCurrentEvent);
				if (pInflictorCurrentDamageEvent->GetInflictor() && pInflictorCurrentDamageEvent->GetInflictor() != pPed)
				{
					pInstigator = pInflictorCurrentDamageEvent->GetInflictor();
				}
			}
		}
	}

	// If we are hit by a recently wrecked vehicle, then set the entity that wrecked the vehicle as our instigator
	if (pImpactInflictor && pImpactInflictor->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = SafeCast(CVehicle, pImpactInflictor);
		if (pVehicle->GetStatus() == STATUS_WRECKED && fwTimer::GetCamTimeInMilliseconds() < pVehicle->GetTimeOfDestruction() + m_TimeToConsiderWreckedVehicleSource)
		{
			pInstigator = pVehicle->GetSourceOfDestruction();
		}
	}

	if (pImpactInflictor && pImpactInflictor->GetIsClassId(CProjectile::GetStaticClassId()))
	{
		CProjectile* pProjectile = (CProjectile*)pImpactInflictor;
		if (pProjectile->GetWeaponFiredFromHash() == ATSTRINGHASH("WEAPON_RAYPISTOL", 0xAF3696A1))
		{
			if (CEntity* pOwner = pProjectile->GetOwner())
			{
				pInstigator = pOwner;
			}
		}
	}

	return pInstigator;
}

bool CCollisionEventScanner::ProcessVehicleImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int nComponent, bool bForceBecauseDriverFell, s32 iSeatIndex)
{
	// If the car's seats have collided then knock off the peds
	
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!physicsVerifyf(pVehicle,"Unexpected NULL vehicle"))
	{
		return false;
	}

	physicsFatalAssertf(pVehicle->GetVehicleModelInfo(),"Unexpected NULL vehicle model info");

	if (pPed->GetSpecialAbility() && pImpactEntity->GetIsTypeVehicle())
	{
        CVehicle* impactVeh = (CVehicle*)pImpactEntity;
        if (impactVeh->GetDriver())
        {
            float impactVehVel = impactVeh->GetVelocity().Mag2();
            float vehVel = pVehicle->GetVelocity().Mag2();
            const float minAiThresholdSqr = 4.f * 4.f;
            const float minThresholdSqr = 6.f * 6.f;

            // ai vehicle velocity can't be too low for any of the events to count
            if (impactVehVel > minAiThresholdSqr)
            {
                Vector3 vel0 = pVehicle->GetVelocity();
                vel0.Normalize();
                Vector3 dir = VEC3V_TO_VECTOR3(impactVeh->GetVehiclePosition()) - VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
                dir.Normalize();

                float dot = dir.Dot(vel0);
                if (dot > 0.2f)
                {
                    // we crashed into ai, most likely
                    if (vehVel > minThresholdSqr)
                        CPlayerSpecialAbilityManager::ChargeEvent(ACET_RAMMED_INTO_CAR, pPed, fImpactMag);
                }
                else if (impactVehVel > vehVel)
                {
                    // ai crashed into us, this will only happen when our veicle faces away from the ai one
                    CPlayerSpecialAbilityManager::ChargeEvent(ACET_RAMMED_BY_CAR, pPed, fImpactMag);
                }
            }
        }
	}
	CPlayerSpecialAbilityManager::ChargeEvent(ACET_CRASHED_VEHICLE, pPed, fImpactMag);

	bool bFallOff = false;

	const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
	int iSeatFragChild = pVehicle->GetVehicleModelInfo()->GetFragChildForSeat(iSeatIndex);
	bool bRagdollWhenVehicleUpsideDown = (pSeatAnimInfo && pSeatAnimInfo->GetRagdollWhenVehicleUpsideDown());
	if((iSeatFragChild > -1 && iSeatFragChild == nComponent) || bRagdollWhenVehicleUpsideDown)
	{
		if(pVehicle->GetTransform().GetC().GetZf() < 0.0f)
		{
			bFallOff = true;
		}
	}

	if(!bFallOff)
	{
		// Scale the impulse if the impacted object requests this through tunableobjects.meta.
		if(pImpactEntity->GetIsTypeObject())
		{
			u32 nModelNameHash = pImpactEntity->GetBaseModelInfo()->GetModelNameHash();
			const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(nModelNameHash);
			if(pTuning)
			{
				fImpactMag *= pTuning->GetKnockOffBikeImpulseScalar();
			}
		}
		if (pVehicle->InheritsFromBike())
		{
			bFallOff = ProcessBikeImpact(pPed,pImpactEntity,fImpactMag,vecImpactNormal,nComponent,bForceBecauseDriverFell);
		}
		else if (pVehicle->InheritsFromQuadBike() ||
				 pVehicle->InheritsFromAmphibiousQuadBike())
		{
			bFallOff = ProcessQuadBikeImpact(pPed,pImpactEntity,fImpactMag,vecImpactNormal,nComponent,bForceBecauseDriverFell);
		}
		else if(pVehicle->GetIsJetSki())
		{
			bFallOff = ProcessJetSkiImpact(pPed,pImpactEntity,fImpactMag,vecImpactNormal,nComponent,bForceBecauseDriverFell);
		}
	}

	// We allow peds on bikes to ragdoll off as we don't have an animated upside down exit
	if (iSeatFragChild <= -1 && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown))
	{
		return false;
	}

	if(bFallOff)
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT, pImpactEntity, fImpactMag))
		{
			static float sfDamageMultiplier = 15.0f;
			float fDamage = 0.0f;
			if(pVehicle->InheritsFromBike()||pVehicle->InheritsFromQuadBike()||pVehicle->GetIsJetSki()||pVehicle->InheritsFromAmphibiousQuadBike()||bRagdollWhenVehicleUpsideDown)
			{
				Vector3 vVehicleVelocity = pVehicle->GetVelocity();
				float fVelocityIntoGroundPoly = vecImpactNormal.Dot(-vVehicleVelocity);
				float fVerticalVelocityToKnockOffThisBike = pVehicle->GetVelocityThresholdBeforeKnockOffVehicle();

				fDamage = sfDamageMultiplier * fVelocityIntoGroundPoly/fVerticalVelocityToKnockOffThisBike;
				if(fDamage < 0.0f)
					fDamage = 0.0f;

				 // Check if this ped is set to be immune to collision damage.
				 if(pPed->m_nPhysicalFlags.bNotDamagedByCollisions || pPed->m_nPhysicalFlags.bNotDamagedByAnything)
					 fDamage = 0.0f;
			}

			CEventDamage tempDamageEvent(pImpactEntity, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_RAMMEDBYVEHICLE);
			CPedDamageCalculator damageCalculator(pImpactEntity, fDamage, WEAPONTYPE_RAMMEDBYVEHICLE, 0, false);
			damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);

			if(tempDamageEvent.AffectsPed(pPed))
			{
				CEvent* pEventAdded = pPed->GetPedIntelligence()->AddEvent(tempDamageEvent);
				if(pEventAdded && pEventAdded != &tempDamageEvent && pEventAdded->GetEventType()==EVENT_DAMAGE)
				{
					// AUDIO, i've just been thrown off my bike audio
					pPed->NewSay("OVER_HANDLEBARS");

					nmDebugf2("[%u] ProcessVehicleImpact:%s(%p) Added High fall task (ped knocked from vehicle)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);

					// Determine velocity prior to processing all attachments as processing all attachments will update our current position
					Mat34V lastMatrix = PHSIM->GetLastInstanceMatrix(pPed->GetRagdollInst());
					Vector3 vVelocityFromLastMatrix = VEC3V_TO_VECTOR3(pPed->GetRagdollInst()->GetMatrix().GetCol3() - lastMatrix.GetCol3());
					vVelocityFromLastMatrix *= fwTimer::GetInvTimeStep();

					// do an explicit attachment update here to
					// make sure the ped is in the correct position
					if(pPed->GetIsAttached())
					{
						CPhysical* pAttachParent = static_cast<CPhysical*>(pPed->GetAttachParent());
						if(pAttachParent && pAttachParent->GetCurrentPhysicsInst())
						{
							pAttachParent->UpdateEntityFromPhysics(pAttachParent->GetCurrentPhysicsInst(), 1);
						}
						pPed->ProcessAllAttachments();
					}

					CTask* pTaskRoll = rage_new CTaskNMThroughWindscreen(2000, 30000, false, pVehicle);
					CTask* pTaskComplexNM = rage_new CTaskNMControl(2000, 30000, pTaskRoll, CTaskNMControl::DO_BLEND_FROM_NM);

					((CEventDamage*)pEventAdded)->SetPhysicalResponseTask(pTaskComplexNM, true);

					pPed->SetRagdollState(RAGDOLL_STATE_PHYS_ACTIVATE);

					if (pPed->GetRagdollInst() != NULL)
					{
						pPed->GetRagdollInst()->SetRagdollEjectedFromVehicle(pVehicle);
						// don't reduce the velocity of the ragdoll on activation (by default we seem to half it)
						pPed->GetRagdollInst()->SetIncomingAnimVelocityScaleReset(1.0f);
					}

					float fSpeedFromLastMatrix = 0.0f;
					// If we don't have a meaningful velocity then just use the impact normal
					if (vVelocityFromLastMatrix.Mag2() > square(1.0f))
					{
						fSpeedFromLastMatrix = NormalizeAndMag(vVelocityFromLastMatrix);
					}
					else
					{
						vVelocityFromLastMatrix = vecImpactNormal;
					}

					// Calculate the amount of forward impulse
					
					Vector3 vImpulse(vVelocityFromLastMatrix);
					
					// Determine the thrown force (adds some upwards force and scales the amount of forward force) and then scale the overall thrown force (including
					// the upwards component) by the speed of the ped

					float fSpeedUpwardComponent = 0.0f;
					// 0.0f -> s_fMinSpeed = 0.0f -> s_fUpMinComponent
					// s_fMinSpeed -> s_fMaxSpeed = s_fUpMinComponent -> s_fUpMaxComponent
					// s_fMaxComponent -> +INF = s_fUpMaxComponent
					if (fSpeedFromLastMatrix < CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMinSpeed)
					{
						fSpeedUpwardComponent = RampValue(fSpeedFromLastMatrix, 0.0f, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMinSpeed, 0.0f, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeUpMinComponent);
					}
					else
					{
						fSpeedUpwardComponent = RampValue(fSpeedFromLastMatrix, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMinSpeed, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMaxSpeed, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeUpMinComponent, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeUpMaxComponent);
					}

					float fSpeedForwardComponent = RampValue(fSpeedFromLastMatrix, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMinSpeed, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMaxSpeed, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeForwardMinComponent, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeForwardMaxComponent);
			
					// Scale the impact depending on how upright the ped is
					Vector3 vPedUp = VEC3V_TO_VECTOR3(pPed->GetTransform().GetUp());
					float fMassForApplyVehicleForce = RampValue(vPedUp.z, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMinUpright, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMaxUpright, 0.0f, pPed->GetMassForApplyVehicleForce());
					vImpulse = fMassForApplyVehicleForce * ((vImpulse * fSpeedForwardComponent) + (vPedUp * fSpeedUpwardComponent));
					
					// GTAV - B*2189036 - If the ped is on a bike and is hit head on by a large vehicle, e.g. a truck, the impulse can be in the wrong direction
					// which pushes the last matrix into the vehicle and results in the ped not colliding with it.
					if( Dot( vImpulse, VEC3V_TO_VECTOR3( lastMatrix.GetCol1().GetY() ) ) > 0.0f )
					{
						Vector3 vDistance = vImpulse * fwTimer::GetTimeStep();
						lastMatrix.SetCol3(pPed->GetRagdollInst()->GetMatrix().GetCol3() - RCC_VEC3V(vDistance));
					}

					// pitch the last matrix back a little so we get pitched forward when we activate
					float fSpeedPitchComponent = RampValue(fSpeedFromLastMatrix, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMinSpeed, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikeMaxSpeed, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikePitchMinComponent, CTaskNMThroughWindscreen::sm_Tunables.m_KnockOffBikePitchMaxComponent);
					Mat34VRotateLocalX(lastMatrix, ScalarV(fSpeedPitchComponent*fwTimer::GetTimeStep()));

					Assertf(IsFiniteStable(lastMatrix.GetCol3()),  "lastMatrix.GetCol3() is invalid. timestep is %f. vImpulse.Mag2() is %f.", 
						fwTimer::GetTimeStep(), vImpulse.Mag2());
					PHSIM->SetLastInstanceMatrix(pPed->GetRagdollInst(), lastMatrix);
					
					// returning true will trigger the SwitchToRagdoll
					return true;
				}
			}
		}
	}

	return false;
}




#if __BANK
void CCollisionEventScanner::AddKnockOffVehicleWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Knock off vehicle");
		bank.AddSlider("Player mult", &sfBikeKO_PlayerMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Player mult MP", &sfBikeKO_PlayerMultMP, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Vehicle mult (player)", &sfBikeKO_VehicleMultPlayer, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Vehicle mult (AI)", &sfBikeKO_VehicleMultAI, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Train mult", &sfBikeKO_TrainMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Bike mult", &sfBikeKO_BikeMult, 0.0f, 100.0f, 0.1f);

		bank.AddSlider("Bike KO mag default", &sfBikeKO_Mag, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Bike KO mag easy", &sfBikeKO_EasyMag, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Bike KO mag hard", &sfBikeKO_HardMag, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Push bike KO mag default", &sfPushBikeKO_Mag, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Push bike KO mag easy", &sfPushBikeKO_EasyMag, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Push bike KO mag hard", &sfPushBikeKO_HardMag, 0.0f, 100.0f, 0.1f);


		bank.AddSlider("Head on dot", &sfBikeKO_HeadOnDot, 0.0f, 1.0f, 0.01f);

		bank.AddSlider("Front mult", &sfBikeKO_FrontMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Front Z mult", &sfBikeKO_FrontZMult, 0.0f, 100.0f, 0.1f);

		bank.AddSlider("Top mult", &sfBikeKO_TopMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Upside down mult", &sfBikeKO_TopUpsideDownMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Top upside down multi AI", &sfBikeKO_TopUpsideDownMultAI, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Bottom mult", &sfBikeKO_BottomMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Side mult", &sfBikeKO_SideMult, 0.0f, 100.0f, 0.1f);
	bank.PopGroup(); // "Knock off vehicle"
}
#endif // __BANK

bool CCollisionEventScanner::ProcessBikeImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int UNUSED_PARAM(nComponent), bool bForceBecauseDriverFell)
{
	// Note: there's an initial threshold in CPhysics::PostSimUpdate that can stop us getting in here in the first place
#if AI_OPTIMISATIONS_OFF && 0
	aiLogf("KO Bike original impact %f\n", fImpactMag);
#endif
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetMyVehicle()==NULL)
		return false;

	if(!pPed->GetMyVehicle()->InheritsFromBike())
	{
		Assertf(false, "ProcessBikeImpact called on a non-bike vehicle");
		return false;
	}

	// don't knock off peds when we're doing a car recording
	if(pPed->GetMyVehicle()->GetIntelligence()->GetRecordingNumber() >= 0
		&& !CVehicleRecordingMgr::GetUseCarAI(pPed->GetMyVehicle()->GetIntelligence()->GetRecordingNumber()))
		return false;

	if(pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_NEVER && !pPed->IsInjured())
		return false;

	CBike* pBike = (CBike*)pPed->GetMyVehicle();
	bool bFallOff = bForceBecauseDriverFell;

    // if we are the passenger on a bike driven by a network clone we only fall off when the driver does.
    // This avoid passengers being knocked off bikes due to collisions that occur because of network lag
    if(pBike->GetDriver() && pBike->GetDriver() != pPed && pBike->GetDriver()->IsNetworkClone())
    {
        return bFallOff;
    }

	if(!bFallOff)
	{
		fImpactMag *= pBike->GetInvMass();
		
		// For consistency all peds use the same settings on a bike as the rider unless they are the player.
		CPed* pPedToCheckForces = pPed;
		if( pBike->GetDriver() && pBike->GetDriver()->IsAPlayerPed() && !pPed->IsAPlayerPed() )
		{
			pPedToCheckForces = pBike->GetDriver();
		}

		if(pPedToCheckForces->IsPlayer())
		{
			if(NetworkInterface::IsGameInProgress())
				fImpactMag *= sfBikeKO_PlayerMultMP;
			else
				fImpactMag *= sfBikeKO_PlayerMult;

			// Scale for impacts with various vehicle types.
			if(pImpactEntity->GetIsTypeVehicle())
			{
				CVehicle* pImpactVehicle = static_cast<CVehicle*>(pImpactEntity);
				if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					fImpactMag *= sfBikeKO_TrainMult;
				}
				else
				{
					fImpactMag *= sfBikeKO_VehicleMultPlayer;
				}
			}
		}
		else // A.I. peds.
		{
			// Scale for impacts with various vehicle types.
			if(pImpactEntity->GetIsTypeVehicle())
			{
				CVehicle* pImpactVehicle = static_cast<CVehicle*>(pImpactEntity);
				if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					fImpactMag *= sfBikeKO_TrainMult;
				}
				else if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
				{
					//reduce impact for bikes that're chasing the same target (and thus aren't avoiding each other)
					if(pImpactVehicle->GetIntelligence()->GetAvoidanceCache().m_pTarget &&
					   pImpactVehicle->GetIntelligence()->GetAvoidanceCache().m_pTarget == pBike->GetIntelligence()->GetAvoidanceCache().m_pTarget)
					{
						fImpactMag *= sfBikeKO_BikeMult;
					}
					else
					{
						fImpactMag *= sfBikeKO_VehicleMultAI;
					}				
				}
			}
		}

		float fScaledFront = sfBikeKO_FrontMult;
		float fFrontDotProd = rage::Abs(vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pBike->GetTransform().GetB())));
		if(fFrontDotProd > sfBikeKO_HeadOnDot)
		{
			// check incidence of impact to the ground
			float fScaledFrontVertical = rage::Max(0.0f, vecImpactNormal.Dot(ZAXIS));
			// if bike is hitting the ground head first
			if( fScaledFrontVertical > sfBikeKO_HeadOnDot)
				fScaledFront = sfBikeKO_FrontMult + sfBikeKO_FrontZMult*fScaledFrontVertical*fScaledFrontVertical;
		}

		// different multipliers for collisions from above if bike is upside down
		float fScaledTop = sfBikeKO_TopMult;
		if(pBike->GetTransform().GetC().GetZf() < -0.5f)
		{
			fScaledTop = pBike->GetStatus()==STATUS_PLAYER ? sfBikeKO_TopUpsideDownMult : sfBikeKO_TopUpsideDownMultAI;
		}

		// calculate combined scaled impact mag
		Vector3 vecBikeUp(VEC3V_TO_VECTOR3(pBike->GetTransform().GetC()));
		fImpactMag *= fScaledFront			* rage::Abs( vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pBike->GetTransform().GetB())) )
			+ sfBikeKO_SideMult		* rage::Abs( vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pBike->GetTransform().GetA())) )
			+ sfBikeKO_BottomMult	* rage::Max(0.0f, vecImpactNormal.Dot(vecBikeUp) )
			+ fScaledTop			* rage::Max(0.0f, -vecImpactNormal.Dot(vecBikeUp) );

		bool bPushBike = pBike->GetVehicleType()==VEHICLE_TYPE_BICYCLE;
		float fKoMag = bPushBike ? sfPushBikeKO_Mag : sfBikeKO_Mag;
		if(pPedToCheckForces->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_EASY)
			fKoMag = bPushBike ? sfPushBikeKO_EasyMag : sfBikeKO_EasyMag;
		else if(pPedToCheckForces->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_HARD)
			fKoMag = bPushBike ? sfPushBikeKO_HardMag : sfBikeKO_HardMag;

		if( pImpactEntity->GetIsTypeVehicle() )
		{
			if( static_cast<CVehicle*>( pImpactEntity )->GetHitByWeaponBlade() )
			{
				static dev_float sfHitByWeaponBladeKOScale = 2.0f;
				fKoMag *= sfHitByWeaponBladeKOScale;
			}
		}
#if AI_OPTIMISATIONS_OFF && 0
		aiLogf(DIAG_SEVERITY_DISPLAY,"KO Bike scaled impact %f\n", fImpactMag);
#endif
		if(fImpactMag > fKoMag)
		{
			if(pImpactEntity->GetIsPhysical() && !pImpactEntity->GetIsAnyFixedFlagSet())
			{
				if(pImpactEntity->GetCurrentPhysicsInst()->GetArchetype()->GetMass() > 60.0f)
					bFallOff = true;
			}
			else
			{
				bFallOff = true;
			}
		}

		// For new handling technique we need to KO if we are scraping along on the side of the bike
		if(CBike::ms_bNewLeanMethod && !bFallOff)
		{
			bool bIsWheelCollision = false;
			bool bWheelsTouching = false;
			for(int i = 0; i < pBike->GetNumWheels(); i++)
			{
				if(pBike->GetWheel(i)->GetIsTouching())
				{
					bWheelsTouching = true;
					break;
				}
			}

			if(!bIsWheelCollision && !bWheelsTouching)
			{
				// Check for scraping along ground
				// Max ground slope at which we knock people off
				static dev_float fScrapeAngleCos = 0.5f;
				static dev_float fSideTolerance = 0.707f;	// what do we count as a side collision?

				float fSideCollisionFactor = rage::Abs(vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pBike->GetTransform().GetA())));
				if(vecImpactNormal.z > fScrapeAngleCos && fSideCollisionFactor > fSideTolerance)
				{
					bFallOff = true;
				}
			}
		}
	}

	

	return bFallOff;
}

bool CCollisionEventScanner::ProcessQuadBikeImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int UNUSED_PARAM(nComponent), bool bForceBecauseDriverFell)
{
	// Note: there's an initial threshold in CPhysics::PostSimUpdate that can stop us getting in here in the first place
#if AI_OPTIMISATIONS_OFF && 0
	aiLogf("KO quad bike original impact %f\n", fImpactMag);
#endif
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetMyVehicle()==NULL)
		return false;

	if(!pPed->GetMyVehicle()->InheritsFromQuadBike() &&
		!pPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike() )
	{
		Assertf(false, "ProcessQuadBikeImpact called on a non-quad vehicle");
		return false;
	}

	// don't knock off peds when we're doing a car recording
	if(pPed->GetMyVehicle()->GetIntelligence()->GetRecordingNumber() >= 0
		&& !CVehicleRecordingMgr::GetUseCarAI(pPed->GetMyVehicle()->GetIntelligence()->GetRecordingNumber()))
		return false;

	if(pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_NEVER && !pPed->IsInjured())
		return false;

	CVehicle* pQuad = (CVehicle*)pPed->GetMyVehicle();
	bool bFallOff = bForceBecauseDriverFell;

	// if we are the passenger on a quad driven by a network clone we only fall off when the driver does.
	// This avoid passengers being knocked off bikes due to collisions that occur because of network lag
	if(pQuad->GetDriver() && pQuad->GetDriver() != pPed && pQuad->GetDriver()->IsNetworkClone())
	{
		return bFallOff;
	}

	// If the quad bike is on its side or upside down, we throw the rider.
	dev_float sfQuadKnockOffZ = 0.1f;
	if(pQuad->GetTransform().GetC().GetZf() < sfQuadKnockOffZ)
	{
		return true;
	}

	if(!bFallOff)
	{
		fImpactMag *= pQuad->GetInvMass();

		// For consistency all peds use the same settings on a quad as the rider unless they are the player.
		CPed* pPedToCheckForces = pPed;
		if( pQuad->GetDriver() && pQuad->GetDriver()->IsAPlayerPed() && !pPed->IsAPlayerPed() )
		{
			pPedToCheckForces = pQuad->GetDriver();
		}

		if(pPedToCheckForces->IsPlayer())
		{
			if(NetworkInterface::IsGameInProgress())
				fImpactMag *= sfBikeKO_PlayerMultMP;
			else
				fImpactMag *= sfBikeKO_PlayerMult;

			// Scale for impacts with various vehicle types.
			if(pImpactEntity->GetIsTypeVehicle())
			{
				CVehicle* pImpactVehicle = static_cast<CVehicle*>(pImpactEntity);
				if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					fImpactMag *= sfBikeKO_TrainMult;
				}
				else
				{
					fImpactMag *= sfBikeKO_VehicleMultPlayer;
				}
			}
		}
		else // A.I. peds.
		{
			// Scale for impacts with various vehicle types.
			if(pImpactEntity->GetIsTypeVehicle())
			{
				CVehicle* pImpactVehicle = static_cast<CVehicle*>(pImpactEntity);
				if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					fImpactMag *= sfBikeKO_TrainMult;
				}
				else
				{
					fImpactMag *= sfBikeKO_VehicleMultAI;
				}
			}
		}

		float fScaledFront = sfBikeKO_FrontMult;
		float fFrontDotProd = rage::Abs(vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pQuad->GetTransform().GetB())));
		if(fFrontDotProd > sfBikeKO_HeadOnDot)
		{
			// check incidence of impact to the ground
			float fScaledFrontVertical = rage::Max(0.0f, vecImpactNormal.Dot(ZAXIS));
			// if quad is hitting the ground head first
			if( fScaledFront > sfBikeKO_HeadOnDot)
				fScaledFront = sfBikeKO_FrontMult + sfBikeKO_FrontZMult*fScaledFrontVertical*fScaledFrontVertical;
		}

		// different multipliers for collisions from above if quad is upside down
		float fScaledTop = sfBikeKO_TopMult;
		if(pQuad->GetTransform().GetC().GetZf() < -0.5f)
		{
			fScaledTop = pQuad->GetStatus()==STATUS_PLAYER ? sfBikeKO_TopUpsideDownMult : sfBikeKO_TopUpsideDownMultAI;
		}

		// calculate combined scaled impact mag
		Vector3 vecBikeUp(VEC3V_TO_VECTOR3(pQuad->GetTransform().GetC()));
		fImpactMag *= fScaledFront			* rage::Abs( vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pQuad->GetTransform().GetB())) )
			+ sfBikeKO_SideMult		* rage::Abs( vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pQuad->GetTransform().GetA())) )
			+ sfBikeKO_BottomMult	* rage::Max(0.0f, vecImpactNormal.Dot(vecBikeUp) )
			+ fScaledTop			* rage::Max(0.0f, -vecImpactNormal.Dot(vecBikeUp) );


		float fKoMag = sfBikeKO_Mag;
		if(pPedToCheckForces->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_EASY)
			fKoMag = sfBikeKO_EasyMag;
		else if(pPedToCheckForces->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_HARD)
			fKoMag = sfBikeKO_HardMag;

#if AI_OPTIMISATIONS_OFF && 0
		aiLogf(DIAG_SEVERITY_DISPLAY,"KO Bike scaled impact %f\n", fImpactMag);
#endif
		if(fImpactMag > fKoMag)
		{
			if(pImpactEntity->GetIsPhysical() && !pImpactEntity->GetIsAnyFixedFlagSet())
			{
				if(pImpactEntity->GetCurrentPhysicsInst()->GetArchetype()->GetMass() > 60.0f)
					bFallOff = true;
			}
			else
			{
				bFallOff = true;
			}
		}
	}

	return bFallOff;
}

bool CCollisionEventScanner::ProcessJetSkiImpact(CPed* pPed, CEntity* pImpactEntity, float fImpactMag, const Vector3& vecImpactNormal, int UNUSED_PARAM(nComponent), bool bForceBecauseDriverFell)
{
	// Note: there's an initial threshold in CPhysics::PostSimUpdate that can stop us getting in here in the first place
#if AI_OPTIMISATIONS_OFF && 0
	aiLogf("KO jetski original impact %f\n", fImpactMag);
#endif
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetMyVehicle()==NULL)
		return false;

	if(!pPed->GetMyVehicle()->GetIsJetSki())
	{
		Assertf(false, "ProcessJetSkiImpact called on a non-jetski vehicle");
		return false;
	}

	// don't knock off peds when we're doing a car recording
	if(pPed->GetMyVehicle()->GetIntelligence()->GetRecordingNumber() >= 0
		&& !CVehicleRecordingMgr::GetUseCarAI(pPed->GetMyVehicle()->GetIntelligence()->GetRecordingNumber()))
		return false;

	if(pPed->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_NEVER && !pPed->IsInjured())
		return false;

	CBoat* pJetSki = (CBoat*)pPed->GetMyVehicle();
	bool bFallOff = bForceBecauseDriverFell;

	// if we are the passenger on a jetski driven by a network clone we only fall off when the driver does.
	// This avoid passengers being knocked off bikes due to collisions that occur because of network lag
	if(pJetSki->GetDriver() && pJetSki->GetDriver() != pPed && pJetSki->GetDriver()->IsNetworkClone())
	{
		return bFallOff;
	}

	if(!bFallOff)
	{
		fImpactMag *= pJetSki->GetInvMass();

		// For consistency all peds use the same settings on a jetski as the rider unless they are the player.
		CPed* pPedToCheckForces = pPed;
		if( pJetSki->GetDriver() && pJetSki->GetDriver()->IsAPlayerPed() && !pPed->IsAPlayerPed() )
		{
			pPedToCheckForces = pJetSki->GetDriver();
		}

		if(pPedToCheckForces->IsPlayer())
		{
			if(NetworkInterface::IsGameInProgress())
				fImpactMag *= sfBikeKO_PlayerMultMP;
			else
				fImpactMag *= sfBikeKO_PlayerMult;

			// Scale for impacts with various vehicle types.
			if(pImpactEntity->GetIsTypeVehicle())
			{
				CVehicle* pImpactVehicle = static_cast<CVehicle*>(pImpactEntity);
				if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					fImpactMag *= sfBikeKO_TrainMult;
				}
				else
				{
					fImpactMag *= sfBikeKO_VehicleMultPlayer;
				}
			}
		}
		else // A.I. peds.
		{
			// Scale for impacts with various vehicle types.
			if(pImpactEntity->GetIsTypeVehicle())
			{
				CVehicle* pImpactVehicle = static_cast<CVehicle*>(pImpactEntity);
				if(pImpactVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					fImpactMag *= sfBikeKO_TrainMult;
				}
				else
				{
					fImpactMag *= sfBikeKO_VehicleMultAI;
				}
			}
		}

		float fScaledFront = sfBikeKO_FrontMult;
		float fFrontDotProd = rage::Abs(vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pJetSki->GetTransform().GetB())));
		if(fFrontDotProd > sfBikeKO_HeadOnDot)
		{
			// check incidence of impact to the ground
			float fScaledFrontVertical = rage::Max(0.0f, vecImpactNormal.Dot(ZAXIS));
			// if jetski is hitting the ground head first
			if( fScaledFront > sfBikeKO_HeadOnDot)
				fScaledFront = sfBikeKO_FrontMult + sfBikeKO_FrontZMult*fScaledFrontVertical*fScaledFrontVertical;
		}

		// different multipliers for collisions from above if jetski is upside down
		float fScaledTop = sfBikeKO_TopMult;
		if(pJetSki->GetTransform().GetC().GetZf() < -0.5f)
		{
			fScaledTop = pJetSki->GetStatus()==STATUS_PLAYER ? sfBikeKO_TopUpsideDownMult : sfBikeKO_TopUpsideDownMultAI;
		}

		// calculate combined scaled impact mag
		Vector3 vecBikeUp(VEC3V_TO_VECTOR3(pJetSki->GetTransform().GetC()));
		fImpactMag *= fScaledFront			* rage::Abs( vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pJetSki->GetTransform().GetB())) )
			+ sfBikeKO_SideMult		* rage::Abs( vecImpactNormal.Dot(VEC3V_TO_VECTOR3(pJetSki->GetTransform().GetA())) )
			+ sfBikeKO_BottomMult	* rage::Max(0.0f, vecImpactNormal.Dot(vecBikeUp) )
			+ fScaledTop			* rage::Max(0.0f, -vecImpactNormal.Dot(vecBikeUp) );


		float fKoMag = sfBikeKO_Mag;
		if(pPedToCheckForces->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_EASY)
			fKoMag = sfBikeKO_EasyMag;
		else if(pPedToCheckForces->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_HARD)
			fKoMag = sfBikeKO_HardMag;

#if AI_OPTIMISATIONS_OFF && 0
		aiLogf(DIAG_SEVERITY_DISPLAY,"KO Bike scaled impact %f\n", fImpactMag);
#endif
		if(fImpactMag > fKoMag)
		{
			if(pImpactEntity->GetIsPhysical() && !pImpactEntity->GetIsAnyFixedFlagSet())
			{
				if(pImpactEntity->GetCurrentPhysicsInst()->GetArchetype()->GetMass() > 60.0f)
					bFallOff = true;
			}
			else
			{
				bFallOff = true;
			}
		}
	}

	return bFallOff;
}

void CCollisionEventScanner::ProcessBumps(CPed& rPed)
{
	//Ensure the collision history is valid.
	const CCollisionHistory* pCollisionHistory = rPed.GetFrameCollisionHistory();
	if(!pCollisionHistory)
	{
		return;
	}
	
	//Check if the most significant collision entity is valid.
	const CCollisionRecord* pMostSignificantCollisionRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
	const CEntity* pMostSignificantCollisionEntity = pMostSignificantCollisionRecord ? pMostSignificantCollisionRecord->m_pRegdCollisionEntity.Get() : NULL;
	if(pMostSignificantCollisionEntity)
	{
		rPed.BumpedByEntity(*pMostSignificantCollisionEntity);
	}
	
	//Sometimes, checking the most significant collision entity isn't quite enough to detect all player bumps.
	//For this reason, check at least the first ped/vehicle collision records as well.
	
	//Check if the first ped collision entity is valid.
	const CCollisionRecord* pFirstPedCollisionRecord = pCollisionHistory->GetFirstPedCollisionRecord();
	const CEntity* pFirstPedCollisionEntity = pFirstPedCollisionRecord ? pFirstPedCollisionRecord->m_pRegdCollisionEntity.Get() : NULL;
	if(pFirstPedCollisionEntity && (pFirstPedCollisionEntity != pMostSignificantCollisionEntity))
	{
		rPed.BumpedByEntity(*pFirstPedCollisionEntity);
	}
	
	//Check if the first ped collision entity is valid.
	const CCollisionRecord* pFirstVehicleCollisionRecord = pCollisionHistory->GetFirstVehicleCollisionRecord();
	const CEntity* pFirstVehicleCollisionEntity = pFirstVehicleCollisionRecord ? pFirstVehicleCollisionRecord->m_pRegdCollisionEntity.Get() : NULL;
	if(pFirstVehicleCollisionEntity && (pFirstVehicleCollisionEntity != pMostSignificantCollisionEntity))
	{
		rPed.BumpedByEntity(*pFirstVehicleCollisionEntity);
	}
}


/////////////////
//EVENT SCANNER
/////////////////
u32 CEventScanner::m_sDeadPedWalkingTimer = 0;
//
CEventScanner::CEventScanner()
	: m_TimeTranqDamageStarted(0)
{
}

CEventScanner::~CEventScanner()
{
}


void CEventScanner::ScanForEventsNow(const CPed& ped, const int iScanType)
{
	const bool bForce = true;
	switch(iScanType)
	{
	case VEHICLE_POTENTIAL_COLLISION_SCAN:
		m_vehicleCollisionScanner.ResetTimer();
		m_vehicleCollisionScanner.Scan(ped, bForce);
		break;

	default:
		break;
	}
}

EXT_PF_TIMER(GroupScanner);

const int CGroupEventScanner::ms_iLatencyPeriod=100;

//-------------------------------------------------------------------------
// Scan for group events, e.g. geting in and out of cars, attacking...
//-------------------------------------------------------------------------
void CGroupEventScanner::Scan(CPed& ped)
{
	PF_FUNC(GroupScanner);

	CPedGroup* pPedGroup=ped.GetPedsGroup();
	if( !pPedGroup )
	{
		return;
	}

	if(!pPedGroup->GetNeedsGroupEventScan())
	{
		return;
	}

	CPed* pLeader = pPedGroup->GetGroupMembership()->GetLeader();
	if( !pLeader )
	{
		return;
	}

	// If this ped currently has a primary task, don't scan (it has been scripted to do something specific
	if( ped.GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY) != NULL )
	{
		return;
	}

	bool bShouldScan = false;
	if( IsRegistered() )
	{
		bShouldScan = ShouldBeProcessedThisFrame();
	}
	else
	{
		//Set the timer if it isn't already set.
		if(!m_timer.IsSet())
		{
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),0);
		}

		//Test if its time to do another check.
		if(m_timer.IsOutOfTime())
		{
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),ms_iLatencyPeriod);
			bShouldScan = true;
		}
	}

	if( bShouldScan || ped.GetPedResetFlag(CPED_RESET_FLAG_ForceScanForEventsThisFrame) )
	{
		StartProcess();
		if( pLeader && pLeader != &ped )
		{

			const bool bLeaderOnBoat = (pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pLeader->GetMyVehicle() && pLeader->GetMyVehicle()->GetVehicleType()==VEHICLE_TYPE_BOAT) ||
				(pLeader->GetGroundPhysical() && pLeader->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pLeader->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT);

			const bool bPedOnBoat = (ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped.GetMyVehicle() && ped.GetMyVehicle()->GetVehicleType()==VEHICLE_TYPE_BOAT) ||
				(ped.GetGroundPhysical() && ped.GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)ped.GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT);
			const bool bPedEnteringVehicle = ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE) != NULL;
			const bool bLeaderEnteringVehicle = pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true) || pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true);

			bool bPedInVehicle = ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || ped.GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_ENTER_VEHICLE );

			const bool bLeaderInVehicle = bLeaderOnBoat || (pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && 
				!pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_EXIT_VEHICLE )) || 
				(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE, true ) &&
				pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType( CTaskTypes::TASK_ENTER_VEHICLE ) < CTaskEnterVehicle::State_Finish);

			// Double check we're in the same vehicle
			if( ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped.GetMyVehicle() && pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pLeader->GetMyVehicle() && ped.GetMyVehicle() != pLeader->GetMyVehicle()  )
			{
				bPedInVehicle = false;
			}

			// VEHICLE EVENTS
			{
				if( bLeaderInVehicle && !bPedInVehicle )
				{
					bool bIgnore = false;

					// Ignore leader entering vehicle events if this ped cant use vehicles.
					if( ped.GetTaskData().GetIsFlagSet(CTaskFlags::DisableVehicleUsage) )
					{
						bIgnore = true;
					}
					else if( ped.GetPedType() == PEDTYPE_MEDIC 
						&& ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_FOLLOW_LEADER_IN_FORMATION))
					{
						bIgnore = true;
					}
					else if(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_WillFollowLeaderAnyMeans ) || ped.GetPedConfigFlag( CPED_CONFIG_FLAG_DontEnterVehiclesInPlayersGroup ) ||
						ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontEnterLeadersVehicle))
					{
						// Only issue this event if this ped is not set to follow any means, for in that case the
						// CTaskComplexFollowLeaderAnyMeans (see SeekEntity.cpp) will handle the car entering/exiting
						bIgnore = true;
					}
					else
					{
						const CVehicle* pLeadersVehicle = pLeader->GetMyVehicle();
						if(pLeadersVehicle)
						{
							const CPed* pDriver = pLeadersVehicle->GetDriver();
							if(pDriver)
							{
								CTaskVehiclePersuit* pDriverTask = static_cast<CTaskVehiclePersuit*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_PERSUIT));
								if(pDriverTask)
								{
									// This shouldn't affect us if the driver of our leader's vehicle is still stopping or exiting
									s32 iDriverState = pDriverTask->GetState();
									if( iDriverState == CTaskVehiclePersuit::State_emergencyStop ||
										iDriverState == CTaskVehiclePersuit::State_exitVehicle ||
										iDriverState == CTaskVehiclePersuit::State_exitVehicleImmediately )
									{
										bIgnore = true;
									}
								}
							}
						}
					}

					if( !bIgnore && !ped.GetPedIntelligence()->GetEventOfType(EVENT_LEADER_ENTERED_CAR_AS_DRIVER) )
					{
						CEventLeaderEnteredCarAsDriver EventLeaderEnteredCarAsDriver;
						ped.GetPedIntelligence()->AddEvent(EventLeaderEnteredCarAsDriver);
					}
				}
				// Only issue this event if this ped is not set to follow any means, for in that case the
				// CTaskComplexFollowLeaderAnyMeans (see SeekEntity.cpp) will handle the car entering/exiting
				else if( !bLeaderInVehicle && bPedInVehicle && !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_WillFollowLeaderAnyMeans ))
				{
					if( !ped.GetPedIntelligence()->GetEventOfType(EVENT_LEADER_EXITED_CAR_AS_DRIVER) )
					{
						CEventLeaderExitedCarAsDriver EventLeaderExitedCarAsDriver;
						ped.GetPedIntelligence()->AddEvent(EventLeaderExitedCarAsDriver);
					}
				}
			}

			// SETUP DEFENSIVE AREA, not for cops/swat they still use normal decision makers in groups
			if( !ped.IsLawEnforcementPed() || ped.PopTypeIsMission() )
			{
				static float PLAYER_GROUP_SIZE = 15.0f;
				static float AI_GROUP_SIZE = 20.0f;
				float fAreaSize = PLAYER_GROUP_SIZE;
				if( pPedGroup->GetGroupMembership()->GetLeader() &&
					!pPedGroup->GetGroupMembership()->GetLeader()->IsAPlayerPed() )
				{
					fAreaSize = AI_GROUP_SIZE;
				}
				Vector3 v1(0.0f, -fAreaSize*0.5f, -7.0f);
				Vector3 v2(0.0f, fAreaSize*0.5f, 7.0f);
				ped.GetPedIntelligence()->GetPrimaryDefensiveArea()->Set( v1, v2, fAreaSize, pLeader, false );
			}

			// The following group events are only applicable if neither ped is in a vehicle (as they are problematic if so)
			if(!bPedInVehicle && !bLeaderInVehicle)
			{
				// HOLSTER/UNHOLSTER weapon
				// Only when not ourselves in combat
				const bool bInCombat = ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true);
				const bool bLeaderSwappingWeapon = pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SWAP_WEAPON, true);
				const bool bPedSwappingWeapon = pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SWAP_WEAPON, true);
				const bool bInCover = ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER);
				const bool bIsUnarmed = !ped.GetWeaponManager() || (ped.GetWeaponManager()->GetBestWeaponSlotHash() == atHashString("SLOT_UNARMED"));

				const CWeapon* pPedWeapon = ped.GetWeaponManager() ? ped.GetWeaponManager()->GetEquippedWeapon() : NULL;

				if(!bIsUnarmed && !bInCover && !bInCombat && !bLeaderSwappingWeapon && !bPedSwappingWeapon && !ped.GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) && !ped.GetPedResetFlag( CPED_RESET_FLAG_IsClimbing ) && !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_BlockWeaponSwitching ))
				{
					const CWeapon* pLeaderWeapon = pLeader->GetWeaponManager() ? pLeader->GetWeaponManager()->GetEquippedWeapon() : NULL;
					const bool bLeaderWeaponIsFists = !pLeaderWeapon || pLeaderWeapon->GetWeaponInfo()->GetIsUnarmed()
#if 0 // CS
						|| pLeaderWeapon->GetWeaponType()==WEAPONTYPE_OBJECT
#endif // 0
						;
					const bool bPedWeaponIsFists = !pPedWeapon || pPedWeapon->GetWeaponInfo()->GetIsUnarmed() 
#if 0 // CS
						|| pPedWeapon->GetWeaponType()==WEAPONTYPE_OBJECT
#endif // 0
						;

					if(!bLeaderWeaponIsFists && bPedWeaponIsFists)
					{
						CEventLeaderUnholsteredWeapon EventLeaderUnholsteredWeapon;
						ped.GetPedIntelligence()->AddEvent(EventLeaderUnholsteredWeapon);
					}
					if(bLeaderWeaponIsFists && !bPedWeaponIsFists)
					{
						CEventLeaderHolsteredWeapon EventLeaderHolsteredWeapon;
						ped.GetPedIntelligence()->AddEvent(EventLeaderHolsteredWeapon);
					}
				}

				// LEADER ENTERING/EXITING COVER
				if(!bInCombat)
				{
					const bool bLeaderInCover = pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true);
					if( !bLeaderInCover )
					{
						ped.SetPedConfigFlag( CPED_CONFIG_FLAG_GroupPedFailedToEnterCover, false );
					}
					else if( !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_GroupPedFailedToEnterCover ) )
					{
						if( bInCover && ped.GetCoverPoint() == NULL )
						{
							ped.SetPedConfigFlag( CPED_CONFIG_FLAG_GroupPedFailedToEnterCover, true );
						}
					}
					if(bLeaderInCover && !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_GroupPedFailedToEnterCover ) && !bInCover)
					{
						// Disabling to try getting a position or cover point to move to earlier
						//CEventLeaderEnteredCover EventLeaderEnteredCover;
						//ped.GetPedIntelligence()->AddEvent(EventLeaderEnteredCover);
					}
					else if( ( !bLeaderInCover || ped.GetPedConfigFlag( CPED_CONFIG_FLAG_GroupPedFailedToEnterCover ) ) && bInCover && 
						!ped.GetPedResetFlag(CPED_RESET_FLAG_IfLeaderStopsSeekCover))
					{
						CEventLeaderLeftCover EventLeaderLeftCover;
						ped.GetPedIntelligence()->AddEvent(EventLeaderLeftCover);
					}
				}
				else
				{
					ped.SetPedConfigFlag( CPED_CONFIG_FLAG_GroupPedFailedToEnterCover, false );
				}

				if (CTaskFollowLeaderInFormation::IsLeaderInMeleeCombat(pLeader))
				{
					ped.NewSay("FIGHT_ENCOURAGE");
				}
			}

			// If the leader is not on any boat, but this ped is - then get them to leave the boat.
			// OR if the leader is on a different boat to this ped!
			// Don't do this if either ped is currently entering a vehicle as one might be about to get into
			// the boat AND their ground physical pointer might not be pointing to anything.

			if(!bPedEnteringVehicle && !bLeaderEnteringVehicle)
			{
				if( (bPedOnBoat && !bLeaderOnBoat) || (bLeaderOnBoat && bPedOnBoat && pLeader->GetGroundPhysical() && ped.GetGroundPhysical() && (pLeader->GetGroundPhysical() != ped.GetGroundPhysical())) )
				{
					CBoat * pBoatToLeave = NULL;
					if(ped.GetGroundPhysical() && ped.GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)ped.GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT)
					{
						pBoatToLeave = (CBoat*)ped.GetGroundPhysical();
					}
					else if(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped.GetMyVehicle() && ped.GetMyVehicle()->GetVehicleType()==VEHICLE_TYPE_BOAT)
					{
						pBoatToLeave = (CBoat*)ped.GetMyVehicle();
					}
					else
					{
						Assertf(pBoatToLeave, "No boat to leave!");
					}
					if(pBoatToLeave)
					{
						CEventMustLeaveBoat leaveBoatEvent(pBoatToLeave);
						ped.GetPedIntelligence()->AddEvent(leaveBoatEvent);
					}
				}
			}

		}
		StopProcess();
	}

	// OFFENSIVE EVENT - CHECK MADE EACH FRAME
	if(pLeader != &ped)
	{
		CEntity* pLeaderTarget = pLeader->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
		if( pLeaderTarget &&
			pLeaderTarget->GetIsTypePed() &&
			!ped.GetPedIntelligence()->GetEventOfType(EVENT_ACQUAINTANCE_PED_HATE ) &&
			!ped.GetPedIntelligence()->IsFriendlyWith(*((CPed*)pLeaderTarget)) )
		{
			CEventAcquaintancePedHate event((CPed*)pLeaderTarget);
			ped.GetPedIntelligence()->AddEvent(event);
		}
	}
}

bool CShockingEventScanner::ms_bLocalPlayerVehicleHornOn = false;
Vec3V CShockingEventScanner::ms_PlayerVehiclePosition = Vec3V(V_ZERO);

void CShockingEventScanner::UpdateClass()
{
	// Reset the static values for local player vehicle
	ms_bLocalPlayerVehicleHornOn = false;
	ms_PlayerVehiclePosition.ZeroComponents();

	// If the local player is in a vehicle
	const CVehicle* pPlayerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if( pPlayerVehicle )
	{
		// cache the position this frame
		ms_PlayerVehiclePosition = pPlayerVehicle->GetTransform().GetPosition();

		// cache whether the horn is on or not this frame
		if( pPlayerVehicle->IsHornOn() )
		{
			ms_bLocalPlayerVehicleHornOn = true;
		}
	}
}

EXT_PF_TIMER(ShockingEventScanner);

dev_float DISTANCE_TO_SCAN_IMMEDIATE = 10.0f;
void CShockingEventScanner::ScanForShockingEvents(CPed* pPed)
{
	PF_FUNC(ShockingEventScanner);

	if(!pPed->GetPedIntelligence()->GetCheckShockFlag())
	{
		return;
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableShockingEvents))
	{
		return;
	}

	if(!pPed->PopTypeIsRandom())
	{
		return;
	}

	CEventGroupGlobal& global = *GetEventGlobalGroup();
	const int num = global.GetNumEvents();
	if( num == 0 )
	{
		// no events to process
		return;
	}

	// If the local player is in a vehicle with the horn on, check if this ped is nearby
	const bool bNearPlayersCarUsingHorn = ms_bLocalPlayerVehicleHornOn && DistSquared(ms_PlayerVehiclePosition, pPed->GetTransform().GetPosition()).Getf() < rage::square(DISTANCE_TO_SCAN_IMMEDIATE);

	// Check if this ped should be processed this frame
	Assert(IsRegistered());
	if(!bNearPlayersCarUsingHorn && !ShouldBeProcessedThisFrame())
	{
		return;
	}

	// Note: more or less all of the checks on the ped here could potentially
	// be moved to CEventShocking::AffectsPedCore(). Doing that would make the code
	// cleaner, but there is a trade-off as for performance it's nice to be able to
	// skip the whole iteration and dispatch of the events to the individual.

	if(pPed->IsPlayer() || !pPed->GetShockingEvent())	// TODO: maybe remove GetShockingEvent()?
	{
		return;
	}
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle())
	{
		CVehicle* pPedVehicle = pPed->GetMyVehicle();
		if(pPedVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT
				|| pPedVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN
				|| pPedVehicle->GetVehicleType() == VEHICLE_TYPE_HELI
				|| pPedVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE
				|| pPedVehicle->HasMissionCharsInIt()
				|| pPedVehicle->PopTypeIsMission())
		{
			return;
		}
	}
	if(pPed->IsLawEnforcementPed())
	{
		// if this cop is chasing someone (it has an order) don't do shocking events
		if(pPed->GetPedIntelligence()->GetOrder())
		{
			return;
		}
	}

	StartProcess();

	CPedShockingEvent& shockingEventMem = *pPed->GetShockingEvent();
	for(int j = 0; j < num; j++)
	{
		fwEvent* ev = global.GetEventByIndex(j);
		if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
		{
			continue;
		}
		CEventShocking* pThisEvent = static_cast<CEventShocking*>(ev);

		// Note: we could move the two checks below to CEventShocking::AffectsPedCore(),
		// but again, it's nice to stop the processing early.
		if(pThisEvent->GetSourceEntity() == pPed)
		{
			continue;
		}

		if (pThisEvent->GetOtherEntity() == pPed && pThisEvent->GetTunables().m_IgnoreIfSensingPedIsOtherEntity)
		{
			continue;
		}

		// Check if we've already reacted to this event.
		if(shockingEventMem.HasReactedToShockingEvent(pThisEvent->GetId()) && !pThisEvent->GetTunables().m_AllowScanningEvenIfPreviouslyReacted)
		{
			continue;
		}

		pPed->GetPedIntelligence()->AddEvent(*pThisEvent);

		// Note: we don't call shockingEventMem.SetShockingEventReactedTo() here
		// to remember that we've handled the event, because we don't know yet if
		// it can be sensed and pass other tests. We could perhaps call
		// CEventShocking::AffectsPedCore() to find out, but then we may be doing
		// duplicate work, and may be in trouble (miss an event) if the conditions
		// change between now and the time the ped gets to process the event.
	}

	StopProcess();
}

//-------------------------------------------------------------------------
// Scan for in water (swimming) events, e.g. drowning.
//-------------------------------------------------------------------------
CInWaterEventScanner::CInWaterEventScanner() 
: m_fTimeInWater(0.0f),
m_fTimeSubmerged(0.0f),
m_fTimeOnSurface(0.0f),
m_fApplyDamageTimer(0.0f)
{

}

CInWaterEventScanner::~CInWaterEventScanner()
{
}

dev_float SUBMERGE_RESET_TIME		= 1.0f; // How long do we have to spend on surface before we reset the submerge timer?
dev_float DROWNING_DAMAGE_INTERVAL	= 0.5f;
dev_float DROWNING_DAMAGE_AMOUNT	= 8.0f;
dev_float DROWNING_DAMAGE_AMOUNT_FLEE = 2.0f;
dev_float UNDERWATER_UPSIDE_DOWN_TIME = 1.0f;

//Not sure this definitely belongs in here, but it seems like the best home for now.
void CInWaterEventScanner::Scan(CPed& ped)
{
	bool bInVehicle = ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle );
	bool bAddedDetachFlag = false;

	if( bInVehicle && !ped.IsNetworkClone() && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown))
	{
		CVehicle* pVehicle = ped.GetMyVehicle();
		if (pVehicle && pVehicle->InheritsFromBoat())
		{
			if (static_cast<CBoat*>(pVehicle)->m_BoatHandling.GetCapsizedTimer() >= UNDERWATER_UPSIDE_DOWN_TIME)
			{
				s32 iSeatIndex = ped.GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(&ped);
				if (pVehicle->IsSeatIndexValid(iSeatIndex))
				{
					bool bFallOff = false;
					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
					if(pSeatAnimInfo && pSeatAnimInfo->GetRagdollWhenVehicleUpsideDown())
					{
						if(pVehicle->GetTransform().GetC().GetZf() < 0.0f)
						{
							bFallOff = true;
							
							if(!ped.GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL))
							{
								ped.SetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL, true);
								bAddedDetachFlag = true;
							}
						}
					}

					if(bFallOff)
					{
						if(CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_VEHICLE_FALLOUT))
						{
							CEventDamage tempDamageEvent(NULL, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_DROWNINGINVEHICLE);
							CPedDamageCalculator damageCalculator(NULL, 0.0f, WEAPONTYPE_DROWNINGINVEHICLE, 0, false);
							damageCalculator.ApplyDamageAndComputeResponse(&ped, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);

							if(tempDamageEvent.AffectsPed(&ped))
							{
								CTask* pTaskRoll = rage_new CTaskNMJumpRollFromRoadVehicle(2000, 30000, false, false, atHashString::Null(), pVehicle);

								ped.SetPedOutOfVehicle(CPed::PVF_Warp|CPed::PVF_IgnoreSafetyPositionCheck);

								//! Kick into ragdoll immediately.
								CEventSwitch2NM event(6000, pTaskRoll);
								ped.SwitchToRagdoll(event);

								//! Apply instant -z velocity to push ped away from boat.
								Vector3 vel(Vector3::ZeroType);
								static dev_float s_fVel = -20.0f;
								vel.z += s_fVel;
								ped.SetVelocity(vel);
								ped.SetDesiredVelocity(vel);
							}				
						}
					}

					//! If we had to add detach flag, but couldn't ragdoll, remove it.
					if(bAddedDetachFlag)
					{
						ped.SetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL, false);
					}
				}
			}
		}
	}

	// We want to increment TIME_UNDERWATER whenever local player is underwater, even if SP or MP, in or out of a vehicle, and capable or not of drowning (ie in submarine or in scuba gear)
	if (ped.IsLocalPlayer() && CStatsMgr::IsPlayerActive())
	{
		const bool bSwimmingUnderWater = ped.GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && ped.m_Buoyancy.GetStatus()==FULLY_IN_WATER;
		const bool bInAVehicleUnderWater = bInVehicle && IsOccupantsHeadUnderWater(ped);
		if(bSwimmingUnderWater || bInAVehicleUnderWater)
		{
			//If the ped doesn't die under water do not count as time underwater.
			if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DrownsInWater))
			{
				StatId stat = StatsInterface::GetStatsModelHashId("TOTAL_TIME_UNDERWATER");
				StatsInterface::IncrementStat(stat, fwTimer::GetTimeStep()*1000.0f);

				if (bSwimmingUnderWater)
				{
					StatId stat = StatsInterface::GetStatsModelHashId("TIME_UNDERWATER");
					StatsInterface::IncrementStat(stat, fwTimer::GetTimeStep()*1000.0f);
				}
			}
			else
			{
				StatId stat = StatsInterface::GetStatsModelHashId("TIME_NOTDROWNINWATER");
				StatsInterface::IncrementStat(stat, fwTimer::GetTimeStep()*1000.0f);
			}
		}
	}

	if(ped.IsNetworkClone() || (!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && !IsDrowningInVehicle(ped) && !IsDrowningOnLadder(ped) && ped.m_Buoyancy.GetStatus()!=FULLY_IN_WATER && !ped.GetPedResetFlag(CPED_RESET_FLAG_IsDrowning)) )
	{
		//Reset in water timers.
		m_fTimeInWater = 0.0f;
		m_fTimeSubmerged = 0.0f;
		m_fTimeOnSurface = 0.0f;
		//m_fApplyDamageTimer = 0.0f;

		return;
	}

	if( (ped.GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater ) && !bInVehicle) || 
		(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInSinkingVehicle ) && bInVehicle) )
	{
		const CQueriableInterface* pQueriableInterface = ped.GetPedIntelligence()->GetQueriableInterface();
		bool bScenarioPed = pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO, true) ||
			pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDERING_SCENARIO, true);

		// Update water timers
		if (CanDrown(ped))
		{
			float fTimeSubmerged = fwTimer::GetTimeStep();

			m_fTimeInWater += fTimeSubmerged;
			m_fTimeSubmerged += fTimeSubmerged;

			m_fTimeOnSurface = 0.0f;
		}
		else if (ped.m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER)
		{
			float fTimeInWater = fwTimer::GetTimeStep();
			
			//! scenario peds don't do damage whilst in water, so don't update timer either. This gives them longer to swim away as otherwise they
			//! start taking damage immediately.
			if(!bScenarioPed)
			{
				m_fTimeInWater += fTimeInWater;
			}
			
			m_fTimeOnSurface += fTimeInWater;

			if(m_fTimeOnSurface > SUBMERGE_RESET_TIME)
			{
				// If we're the player & we have just surfaced after being submerged for some
				// time - then play a gasp-for-air sound.
				//Now handled in audSpeechAudioEntity::Update() so we can preload it	-R Katz 2/28/13

				// Reset submerged timer when on surface, if we have been on surface for long enough
				// (avoid resetting if we are flicking between two states)
				m_fTimeSubmerged = 0.0;
			}

			// Update time in water
			if (ped.IsPlayer() && !bInVehicle)
			{
				StatId stat = StatsInterface::GetStatsModelHashId("TIME_IN_WATER");
				StatsInterface::IncrementStat(stat, fTimeInWater*1000.0f);
			}
		}

		// Has ped been in water long enough to drown?
		float fDrownDamage = 0.0f;
		float fMaxTimeInWater = ped.GetMaxTimeInWater();
		static dev_float sf_MaximumWaterPressureDepth = -170.0f;
		bool bGettingCrushedByWater =  !bInVehicle && ped.GetTransform().GetPosition().GetZf() < sf_MaximumWaterPressureDepth;		
		bool bPlayerAnimalWithNoWaterData = ped.IsLocalPlayer() && CPedType::IsAnimalType(ped.GetPedType()) && ped.GetMotionTaskDataSet() && ped.GetMotionTaskDataSet()->GetInWaterData() == nullptr;
		if(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming ) || bPlayerAnimalWithNoWaterData)
		{
			// Ped cannot swim - Kill it instantly
			fDrownDamage = ped.GetHealth();
		}
		else if(bGettingCrushedByWater || (fMaxTimeInWater >= 0.0f && m_fTimeInWater > fMaxTimeInWater) || 
			(m_fTimeSubmerged > ped.GetMaxTimeUnderwater() && CanDrown(ped)))
		{
			// Ped has been in water for too long... it should start to take damage

			if( m_fApplyDamageTimer > 0.0f )
				m_fApplyDamageTimer -= fwTimer::GetTimeStep();
			else
			{
				fDrownDamage = (ped.GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning) ? DROWNING_DAMAGE_AMOUNT_FLEE : DROWNING_DAMAGE_AMOUNT);
				m_fApplyDamageTimer = DROWNING_DAMAGE_INTERVAL;
			}	
		}
		else
		{
			// Ped hasn't been in water for long enough to start taking any damage
			m_fApplyDamageTimer = DROWNING_DAMAGE_INTERVAL;
		}

		if ( fDrownDamage > 0.0f )
		{
			if ( !(pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT,true) ||
						 bScenarioPed) )
			{ 
				u32 nWeaponUsedHash = bInVehicle ? WEAPONTYPE_DROWNINGINVEHICLE : WEAPONTYPE_DROWNING;
				CPedDamageCalculator damageCalculator(NULL,fDrownDamage,nWeaponUsedHash, 0, false);
				CEventDamage event(NULL, fwTimer::GetTimeInMilliseconds(), nWeaponUsedHash);
				damageCalculator.ApplyDamageAndComputeResponse(&ped, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);

				// Apply damage will vibrate the pad based on the amount of damage done. We want the pad to vibrate based on the health left
				// so that it gets more intense as your health goes down.
				if(ped.IsLocalPlayer())
				{
					const float MAX_RUMBLE_DURATION = 1000.0f; // 1 second

					float intensity = 0.0f;

					// if the player is not dead.
					if(ped.GetHealth() > ped.GetDyingHealthThreshold())
					{
						if(event.GetDamageApplied() > 0.0f)
						{
							float maxHealth = ped.GetMaxHealth() - ped.GetDyingHealthThreshold();
							intensity = (maxHealth - (ped.GetHealth() - ped.GetDyingHealthThreshold())) / maxHealth;
						}
					}
					else
					{
						intensity = 1.0f;
					}

					if(intensity > 0.0f)
					{
						// We are using the intensity as the duration so as the rumble gets stronger it lasts longer. This also gives more
						// time for the motor to speed up.
						CControlMgr::StartPlayerPadShakeByIntensity(static_cast<u32>(intensity * MAX_RUMBLE_DURATION), intensity);
					}
				}

				bool bWasKilledOrInjured = (event.GetDamageResponseData().m_bKilled || event.GetDamageResponseData().m_bInjured);
				if( bWasKilledOrInjured )
				{
					ped.GetPedIntelligence()->AddEvent(event);
				}
			}
		}
	}
}

void CInWaterEventScanner::StartDrowningNow(CPed& ped)
{
	if( ped.m_Buoyancy.GetStatus() != NOT_IN_WATER )
	{
		m_fTimeSubmerged = ped.GetMaxTimeUnderwater();
		m_fApplyDamageTimer = 0.0f;
	}
}

bool CInWaterEventScanner::CanDrown(CPed& ped)
{
	//Disable drowning while wearing scuba gear.
	if(CTaskMotionSwimming::IsScubaGearVariationActiveForPed(ped))
	{
		return false;
	}

	// Disable drowning while in a functioning submarine.
	CVehicle* vehicle = ped.GetVehiclePedInside();

	if( vehicle )
	{
		CSubmarineHandling* subHandling = vehicle->GetSubHandling();

		if( subHandling &&
			!subHandling->PassengersShouldDrown( vehicle ))
		{
			return false;
		}
	}

	return ped.m_Buoyancy.GetStatus() == FULLY_IN_WATER || IsDrowningInVehicle(ped) || IsDrowningOnLadder(ped); 
}

bool CInWaterEventScanner::IsDrowningInVehicle(CPed& Ped) 
{
	// If we are drowning start damaging drivers and passengers
	CVehicle* pVehicle = Ped.GetVehiclePedInside(); 

	if(pVehicle)
	{
		// Don't need to check if the vehicle is drowning unless it's a submarine / boat.
		CBoatHandling* boatHandling = pVehicle->GetBoatHandling();

		if(boatHandling)
		{
			if(pVehicle->GetStatus()==STATUS_WRECKED && IsOccupantsHeadUnderWater(Ped))
			{
				return true;
			}
		}
		else
		{
			CSubmarineHandling* subHandling = pVehicle->GetSubHandling();

			if( subHandling )
			{
				return subHandling->PassengersShouldDrown( pVehicle ) && IsOccupantsHeadUnderWater(Ped);
			}
			else
			{
				if(IsOccupantsHeadUnderWater(Ped))
				{
					return true;
				}
			}
		}
	}

	return false; 
}

bool CInWaterEventScanner::IsDrowningOnLadder(CPed& Ped) 
{
	const CTaskClimbLadder* pClimbLadderTask  = (const CTaskClimbLadder*)Ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER);
	if (pClimbLadderTask)
	{
		// Get the head bone position in global space
		Vector3 headPosition;
		Ped.GetBonePosition(headPosition, BONETAG_HEAD);

		//Get the water level in global space
		float waterLevel;
		if (Ped.m_Buoyancy.GetWaterLevelIncludingRivers(headPosition, &waterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL) != WATERTEST_TYPE_NONE)
		{
			return headPosition.z < waterLevel;
		}
	}
	return false; 
}

bool CInWaterEventScanner::IsTakingDamageDueToFatigue(CPed& ped)
{
	if( ped.GetIsInWater() && ped.GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater ) && ped.GetMaxTimeInWater() >= 0.0f && m_fTimeInWater > ped.GetMaxTimeInWater() )
	{
		return true;
	}
	return false;
}

bool CInWaterEventScanner::IsOccupantsHeadUnderWater(CPed& ped)
{
	Vector3 headPosition;

	//get the head bone position in global space
	ped.GetBonePosition(headPosition, BONETAG_HEAD);
	//get the water level in global space
	float waterLevel;

	CVehicle* pVehicle = ped.GetMyVehicle();

	if(pVehicle && pVehicle->m_Buoyancy.GetWaterLevelIncludingRivers(headPosition, &waterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL)
		!= WATERTEST_TYPE_NONE)
	{
#if __DEV
		static dev_bool bDebugDrowningCheck = false;

		if (bDebugDrowningCheck)
		{
			headPosition.x+=1.5f; //offset the debug draw so we can see what's happening
			//head position
			grcDebugDraw::Sphere(headPosition,0.05f,Color_red);

			//water level
			grcDebugDraw::Sphere(Vector3(headPosition.x, headPosition.y, waterLevel),0.05f,Color_blue);
		}

#endif //__DEV

		return (headPosition.z<waterLevel);
	}
	else
	{
		return false;
	}
}




//-------------------------------------------------------------------------
// Scan for Core events (common to all projects)
//-------------------------------------------------------------------------
EXT_PF_TIMER(AI_ScanForEvents);

static const u32 NUM_UNINTERRUPTIBLE_TASKS = 8;
static s32 UNINTERRUPTIBLE_TASKS[NUM_UNINTERRUPTIBLE_TASKS] =
{
	CTaskTypes::TASK_ENTER_VEHICLE,
	CTaskTypes::TASK_EXIT_VEHICLE,
	CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE,
	CTaskTypes::TASK_RELOAD_GUN,
	CTaskTypes::TASK_ENTER_COVER,
	CTaskTypes::TASK_EXIT_COVER,
	CTaskTypes::TASK_AIM_GUN_SCRIPTED, 
	CTaskTypes::TASK_CUTSCENE
};

void CEventScanner::ScanForEvents(CPed& ped, bool bFullUpdate)
{
	PF_FUNC(AI_ScanForEvents);

	CPedIntelligence& pedIntelligence=*(ped.GetPedIntelligence());

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WaitingForPlayerControlInterrupt))
	{
		if (ped.IsLocalPlayer() && ped.GetPlayerInfo())
		{
			// If we're already running a player on foot task, just clear the flags,
			// we've already got control, this can happen if script switch, but don't clear the peds tasks
			// then give the ped a put ped directly into cover task which already handles creating a parent 
			// on foot task. This prevents the cover task from restarting again unnecessarily
			if (ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT))
			{
				ped.GetPedIntelligence()->ClearTaskEventResponse();
				ped.SetPedConfigFlag(CPED_CONFIG_FLAG_WaitingForPlayerControlInterrupt, false);
				ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToStayInCoverDueToPlayerSwitch, false);
			}
			else
			{
				CControl* pControl = ped.GetControlFromPlayer();
				if (pControl)
				{
					if (ShouldInterruptCurrentTask(ped, *pControl))
					{
						bool bUsingCover = false;

						// Check if we're using cover
						CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
						if (pInCoverTask && ped.GetCoverPoint() && pInCoverTask->GetState() != CTaskInCover::State_Aim)
						{
							ped.SetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft, pInCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft));
							ped.SetPlayerResetFlag(CPlayerResetFlags::PRF_SPECIFY_INITIAL_COVER_HEADING);
							ped.SetPlayerResetFlag(CPlayerResetFlags::PRF_SKIP_COVER_ENTRY_ANIM);
							CTaskPlayerOnFoot* pPlayerTask = rage_new CTaskPlayerOnFoot();
							pPlayerTask->SetScriptedToGoIntoCover(true);
							// In cover task pointer will get nuked the next line, don't use it again!
							ped.GetPedIntelligence()->AddTaskDefault(pPlayerTask);
							ped.SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);
							bUsingCover = true;
						}

						ped.GetPedIntelligence()->ClearPrimaryTask();
						ped.GetPedIntelligence()->ClearTaskEventResponse();
						ped.SetPedConfigFlag(CPED_CONFIG_FLAG_WaitingForPlayerControlInterrupt, false);
						ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToStayInCoverDueToPlayerSwitch, false);

						if (!bUsingCover)
						{
							ped.GetPedIntelligence()->AddTaskDefault(ped.ComputeDefaultTask(ped));
						}
					}	
				}	
			}
		}
	}
	else
	{
		ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToStayInCoverDueToPlayerSwitch, false);
	}

	// A check to make sure this ped is either not dead or correctly playing the dead tasks
	ScanForDeadPedNotRunningCorrectTasks(ped);

	if( !ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning) )
	{
		//Scan for potential collisions with peds.
		m_playerToPedWalkIntoScanner.Scan(ped,pedIntelligence.GetClosestPedInRange());

		//Scan for ped acquaintances
		CEntityScannerIterator entityList = pedIntelligence.GetNearbyPeds();
		m_acquaintanceScanner.Scan(ped,entityList);
		
		//Scan for agitation
		m_agitationScanner.Scan(ped, entityList);

		//Scan for personal space encroachment
		m_encroachmentScanner.Scan(ped, entityList);

		//Scan for group events (e.g. getting in and out of cars).   
		m_groupScanner.Scan(ped);

		//Scan for nearby fires
		m_fireScanner.Scan(ped);

		// Scan for AI responses related to vehicles
		m_passengerEventScanner.Scan(ped);

		// Scan the nearby doors
		ScanForDoorResponses(ped);

		// Scan for the ped being in water and drowning
		m_inWaterScanner.Scan(ped);
	}

	// Scan for health stuff
	m_pedHealthScanner.Scan(ped);

	// Only run physics event scanning when the ped is active in the physics world (or standing on an active physical)
	if( !ped.IsAsleep() || (ped.GetGroundPhysical() != NULL && !ped.GetGroundPhysical()->IsAsleep()) )
	{
		//Scan for potential collisions with vehicles.
		m_vehicleCollisionScanner.Scan(ped);

		// Scan for physics related events, e.g. standing on cars
		m_physicsEventScanner.Scan(ped);

		// Collision events
		// needs to be done before data gets cleared in Physical::ProcessControl()
		if(m_stuckChecker.TestPedStuck(&ped))
		{
			// do nothing, event will have been added if it was required
		}
		else
		{
			m_collisionEventScanner.ScanForCollisionEvents(&ped);	
		}

		// Scans for the ped movement being stopped
		m_staticMovementScanner.Scan(ped);
	}
	else
	{
		// A ped's physics can go to sleep even whilst they are trying to move but stuck against something.
		// We need to run the static movement test for all peds (high or low physics) who are trying to move somewhere
		if(ped.GetMotionData()->GetCurrentMoveBlendRatio().Mag2() > SMALL_FLOAT)	
		{
			// Scans for the ped movement being stopped
			m_staticMovementScanner.Scan(ped);
		}
	}

	CheckForTranquilizerDamage(ped);

	//Check that this ped's tasks, down to the first non-temporary task, are compatible with its active move blender.
	CheckTasksAreCompatibleWithMotion(ped);

	// Check our ambient friend and see if we need to respond to them.
	CheckAmbientFriend(ped);

	// Check if our ambient friend is gone.
	CheckAmbientFriendDeletion(ped);

	// Scan for shocking events
	m_shockingEventsScanner.ScanForShockingEvents(&ped);

	if(bFullUpdate)
	{
		// Now when we've had the chance to scan for shocking events, we can reset
		// the check shock flag. If the ped is in a state where it should respond
		// to shocking events, it should be set again this frame by a task.
		ped.GetPedIntelligence()->SetCheckShockFlag(false);
	}
}

bool CEventScanner::ShouldInterruptCurrentTask(const CPed& rPed, const CControl& rControl)
{
	const CPedIntelligence& rIntelligence = *rPed.GetPedIntelligence();
	
	bool bCheckForInterrupt = true;

	for (s32 i=0; i<NUM_UNINTERRUPTIBLE_TASKS; i++)
	{
		if (rIntelligence.FindTaskActiveByType(UNINTERRUPTIBLE_TASKS[i]))
		{
			bCheckForInterrupt = false;
		}
	}

	if (bCheckForInterrupt)
	{
		if (rPed.GetPlayerInfo() && fwTimer::GetTimeInMilliseconds() > rPed.GetPlayerInfo()->TimeSinceSwitch + CPlayerInfo::ms_Tunables.m_TimeBetweenSwitchToClearTasks)
		{
			return true;
		}

#if FPS_MODE_SUPPORTED
		// always interrupt in the first person
		if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_SwitchingCharactersInFirstPerson))
		{
			return true;
		}
#endif // FPS_MODE_SUPPORTED

		Vector2 vecStick(rControl.GetPedWalkLeftRight().GetNorm(), - rControl.GetPedWalkUpDown().GetNorm());
		float fInputMag = vecStick.Mag();
		TUNE_GROUP_FLOAT(SWITCH_DEBUG, MOVEMENT_BREAKOUT, 0.1f, 0.0f, 1.0f, 0.01f);
		if (fInputMag > MOVEMENT_BREAKOUT 
			|| CPlayerInfo::IsAiming() 
			|| rControl.GetPedJump().IsPressed() 
			|| rControl.GetPedEnter().IsPressed() 
			|| rControl.GetPedCover().IsPressed()
			|| rControl.GetVehicleAccelerate().IsDown())
		{
			return true;
		}
	}
	return false;
}

void CEventScanner::Clear()
{

}
 
void CEventScanner::ScanForDeadPedNotRunningCorrectTasks( CPed &ped )
{
	// If the ped isn't dead or dying but 
	// Is injured and not playing an injured task, or is fatally injured and not dying
	
	// MK: assuming here that the threshold in pedhealth.meta is the same for injured and dying
	// combining the two comparisons into one. Adding an assert that this is a correct assumption
	// Old Check : if(ped.IsInjured() || ped.IsFatallyInjured())
	Assertf(ped.GetDyingHealthThreshold() >= ped.GetInjuredHealthThreshold(), "Assumption that DyingHealthThreshold (%.4f) is >= InjuredHealthThreshold (%.4f) is wrong.", 
		ped.GetDyingHealthThreshold(), ped.GetInjuredHealthThreshold() );
	if(ped.IsFatallyInjured() && !ped.GetIsDeadOrDying())
	{
		if (m_sDeadPedWalkingTimer > 0)
			m_sDeadPedWalkingTimer--;
		else
		{
			bool bGenerateEvent = true;

			// Don't bother trying to generate the damage event if the ped is already running the dying task
			if (ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD))
			{
				bGenerateEvent = false;
			}
			else
			{
				// If the active task or any of its subtasks handles running on a dead ped, don't generate the event, otherwise generate it
				CTask* pActiveTask = ped.GetPedIntelligence()->GetTaskActive();
				bGenerateEvent = pActiveTask ? !pActiveTask->HandlesDeadPed() : true;
			}

			if(bGenerateEvent)
			{
				if(!(ped.GetPedIntelligence()->HasEventOfType(EVENT_DEATH) || ped.GetPedIntelligence()->HasEventOfType(EVENT_DAMAGE)))
				{
					bool bUseRagdoll = ped.GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_DIE);
					CEventDeath deathEvent(false, bUseRagdoll);

					if (!bUseRagdoll)
					{
						// look for an appropriate existing animation to use
						CTaskFallAndGetUp* pTask = static_cast<CTaskFallAndGetUp*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FALL_AND_GET_UP));
						if (pTask && pTask->GetClipSetId()!=CLIP_SET_ID_INVALID)
						{
							deathEvent.SetClipSet(pTask->GetClipSetId(), pTask->GetClipId(), pTask->GetClipPhase());
						}
						ped.GetPedIntelligence()->AddEvent(deathEvent);
					}
					else
					{
						if (!ped.GetUsingRagdoll())
							ped.SwitchToRagdoll(deathEvent);
						else
							ped.GetPedIntelligence()->AddEvent(deathEvent);
					}
					
					m_sDeadPedWalkingTimer = 5;
				}
			}
		}
	}
}

void CEventScanner::ScanForDoorResponses( const CPed &ped )
{
	//Scan for closest door
	if( ped.GetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK ) || ped.GetPedResetFlag( CPED_RESET_FLAG_OpenDoorArmIK ) )
	{
		if( ( !ped.GetPedIntelligence()->GetTaskSecondaryPartialAnim() || 
			   ped.GetPedIntelligence()->GetTaskSecondaryPartialAnim()->GetTaskType() != CTaskTypes::TASK_OPEN_DOOR) && 
			  !ped.GetPedResetFlag( CPED_RESET_FLAG_ScriptDisableSecondaryAnimationTasks ) && 
			  !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_IsHandCuffed ) )
		{
			CDoor* pClosestDoor = ped.GetPedIntelligence()->GetClosestDoorInRange();
			if( pClosestDoor && CTaskOpenDoor::ShouldAllowTask( &ped, pClosestDoor ) )
			{
				CEventOpenDoor event( pClosestDoor );
				ped.GetPedIntelligence()->AddEvent(event);
			}
		}
	}
}

static const fwMvClipSetId ms_TranqDict("anim@fidgets@hit", 0x8CC743AD);
void CEventScanner::CheckForTranquilizerDamage(CPed& ped)
{
	TUNE_GROUP_INT(TRANQUILIZER_TUNE, uTranqDamageTimeBeforeDie, 1000, 0, 100000, 100);
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_HitByTranqWeapon) && m_TranqClipRequestHelper.Request(ms_TranqDict) && m_TimeTranqDamageStarted == 0)
	{
		const atHashString clipName("HIT_A", 0x5FF35585);

		eScriptedAnimFlagsBitSet flags;
		flags.BitSet().Set(AF_UPPERBODY);
		flags.BitSet().Set(AF_SECONDARY);
		flags.BitSet().Set(AF_ADDITIVE);

		eIkControlFlagsBitSet IKflags;

		fwClipSet* clipSet = fwClipSetManager::GetClipSet(ms_TranqDict);
		if (clipSet)
		{
			CTaskScriptedAnimation* task = rage_new CTaskScriptedAnimation(clipSet->GetClipDictionaryName(), clipName, CTaskScriptedAnimation::kPriorityMid, (u32)BONEMASK_SPINEONLY, SLOW_BLEND_DURATION, NORMAL_BLEND_DURATION, uTranqDamageTimeBeforeDie, flags, 0.0f, false, false, IKflags);

			ped.GetPedIntelligence()->AddTaskSecondary(task, PED_TASK_SECONDARY_PARTIAL_ANIM);
		}

		m_TimeTranqDamageStarted = fwTimer::GetTimeInMilliseconds();
	}

	if (!ped.IsNetworkClone() && m_TimeTranqDamageStarted > 0 && !ped.GetIsDeadOrDying())
	{
		ped.GetMotionData()->SetScriptedMaxMoveBlendRatio(MOVEBLENDRATIO_STILL);

		s32 sTimeSince = fwTimer::GetTimeInMilliseconds() - m_TimeTranqDamageStarted;
		if (sTimeSince > uTranqDamageTimeBeforeDie)
		{
			m_TranqClipRequestHelper.Release();
			m_TimeTranqDamageStarted = 0;

			ped.SetHealth(0);

			CTask* pNMTask = NULL;
			const CTaskNMSimple::Tunables::Tuning* pTuning = CTaskNMSimple::sm_Tunables.GetTuning("Death_Tranquilizer");
			if (aiVerifyf(pTuning,"Failed to find 'Death_Tranquilizer' NM tuning data"))
			{
				pNMTask = rage_new CTaskNMSimple(*pTuning);
			}
			else
			{
				pNMTask = rage_new CTaskNMRelax(1000, 3000);
			}

			CEventSwitch2NM eventNM(3000, pNMTask);
			ped.GetPedIntelligence()->AddEvent(eventNM);
		}
	}
}

void CEventScanner::CheckTasksAreCompatibleWithMotion( CPed &ped )
{
	if (ped.GetAllowTasksIncompatibleWithMotion()) 
	{
		return;
	}

	//Check that this ped's tasks, down to the first non-temporary task, are compatible with its active move blender.
	// - don't do this for network clones or the player.
	if(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ) && !ped.IsNetworkClone() && !ped.IsPlayer() )
	{
		CTaskMotionBase* pMotionTask = ped.GetCurrentMotionTask();
		if (pMotionTask && pMotionTask->IsInWater())
		{
			bool isTaskCompatible = false;
			for(int i=0; i<PED_TASK_PRIORITY_MAX; i++)
			{
				aiTask *pTask = ped.GetPedIntelligence()->GetTaskManager()->GetTask(PED_TASK_TREE_PRIMARY, i);

				CTask * task = static_cast<CTask*>(pTask);
				if(task)
				{
					if (pMotionTask && task->IsValidForMotionTask(*pMotionTask))
					{
						isTaskCompatible = true;
						break;
					}
					else if(i >= PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP)
					{
						//This is the first non-temporary task, so stop checking.
						break;
					}
				}
			}

			if(!isTaskCompatible)
			{
				//This ped's task is not compatible with its active move blender, so fire an event to get the ped back onto a compatible move blender.
				aiAssert(pMotionTask && pMotionTask->IsInWater());
				//Add new motion types here.
				//We need to get the ped out of the water before it can continue with the active task.
				CEventGetOutOfWater event;
				ped.GetPedIntelligence()->AddEvent(event);
				
			}
		}
	}
}

void CEventScanner::CheckAmbientFriend(CPed& ped)
{
	CPed* pAmbientFriend = ped.GetPedIntelligence()->GetAmbientFriend();

	// No friends.
	if (!pAmbientFriend)
	{
		return;
	}
	
	// Don't do this on the player.
	if (pAmbientFriend->IsAPlayerPed() || ped.IsAPlayerPed())
	{
		return;
	}

	// Don't do this on mission entities.
	if (pAmbientFriend->PopTypeIsMission() || ped.PopTypeIsMission())
	{
		return;
	}

	// See if they are doing something worth responding to.
	CEvent* pFriendEvent = pAmbientFriend->GetPedIntelligence()->GetCurrentEvent();
	bool bFriendIsWanderingAway = false;

	// Check for the owner wandering away (without being sent an event)
	if (pAmbientFriend->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDER, true)
		&& ped.GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		bFriendIsWanderingAway = true;
	}

	// At this point, we know that our friend isn't doing anything interesting.
	if (!pFriendEvent && !bFriendIsWanderingAway)
	{
		return;
	}

	// Don't respond to temporary events.
	if (pFriendEvent && pFriendEvent->IsTemporaryEvent())
	{
		return;
	}

	// Check if our friend is responding to us already.
	if (pFriendEvent && pFriendEvent->GetEventType() == EVENT_HELP_AMBIENT_FRIEND)
	{
		// The "independent" friend (the owner of the dog) does not respond to the dog's help event.
		// The dependent friend (the dog) will - this is so it will follow if the situation escalates.
		if (!ped.GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
		{
			return;
		}
	}

	// Determine how we want to help.
	CEventHelpAmbientFriend::eHelpAmbientReaction iNewResponse = CEventHelpAmbientFriend::DetermineNewHelpResponse(ped, *pAmbientFriend, pFriendEvent);

	// Check if we are responding to our friend already.
	CEvent* pCurrentEvent = ped.GetPedIntelligence()->GetCurrentEvent();
	if (pCurrentEvent && pCurrentEvent->GetEventType() == EVENT_HELP_AMBIENT_FRIEND)
	{
		// Unless you're the dependent, you don't need to add another event in this case.
		if (!ped.GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
		{
			return;
		}

		// Check how we were helping.
		CEventHelpAmbientFriend::eHelpAmbientReaction iOldResponse = static_cast<CEventHelpAmbientFriend*>(pCurrentEvent)->GetHelpResponse();

		// Only allow a new response if things got worse.
		if (iNewResponse <= iOldResponse)
		{
			return;
		}
	}

	// Add the event.
	CEventHelpAmbientFriend helpEvent(pAmbientFriend, pFriendEvent ? pFriendEvent->GetSourcePed() : NULL, iNewResponse);
	ped.GetPedIntelligence()->AddEvent(helpEvent, false, true);
}

void CEventScanner::CheckAmbientFriendDeletion(CPed& ped)
{
	// We had a friend but they were destroyed and we were dependent on them.
	if (ped.GetPedIntelligence()->HadAmbientFriend() && !ped.GetPedIntelligence()->GetAmbientFriend() && ped.GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		// Do short range removal on this ped.
		ped.SetRemoveAsSoonAsPossible(true);
	}
}

CInterestingEvents g_InterestingEvents;

const u32 CInterestingEvents::ms_iScanFrequency = 500;


CInterestingEvents::CInterestingEvents(void)
{
	m_bIsActive = false;//true;	// NB : this must be removed, and only set when camera goes idle and then reset again when it exits idle-mode
	m_bIgnoreEventsBehindPlayer = true;
	m_bWaitForEventDurationToComplete = true;
	m_bUseTimeDelayBeforeAddingSimilarEvent = true;
	m_iLookingAtEvent = -1;
	m_iLastScanTime = 0;	
	m_fEventRadius = 30.0f;
	m_iCurrentFrameCounter = fwTimer::GetSystemFrameCount() - 1;

	for(int e=0; e<MAX_NUM_INTERESTING_EVENTS; e++)
	{
		m_Events[e].m_eType = ENone;
		m_Events[e].m_iStartTime = 0;
		m_Events[e].m_pEntity = NULL;
		m_Events[e].m_bVisibleToCamera = false;
	}

	for(int i=0; i<ENumCategories; i++)
	{
		m_NextTimeToAcceptEvents[i] = 0;
		// set defaults in case I forgot any below..
		m_EventDurations[i] = 2000;
		m_EventPriorities[i] = 5;
	}

	m_EventPriorities[EPedGotKilled]				= 10;
	m_EventPriorities[EExplosion]					= 10;
	m_EventPriorities[ESwatTeamAbseiling]			= 9;
	m_EventPriorities[ECopKillingCriminal]			= 9;
	m_EventPriorities[EGangFight]					= 9;
	m_EventPriorities[EGangAttackingPed]			= 9;
	m_EventPriorities[EMeleeAction]					= 9;
	m_EventPriorities[ESeenMeleeAction]				= 6;
	m_EventPriorities[EGunshotFired]				= 9;
	m_EventPriorities[EHelicopterOverhead]			= 8;
	m_EventPriorities[ECarJacking]					= 7;
	m_EventPriorities[ERoadRage]					= 7;
	m_EventPriorities[EFistFight]					= 6;
	m_EventPriorities[ECarCrash]					= 8;
	m_EventPriorities[EPedKnockedOffBike]			= 9;
	m_EventPriorities[EPedRunOver]					= 9;
	m_EventPriorities[EMadDriver]					= 5;
	m_EventPriorities[EEmergencyServicesArrived]	= 6;
	m_EventPriorities[EPanickedPed]					= 6;
	m_EventPriorities[EPedRevived]					= 6;
	m_EventPriorities[EPlaneFlyby]					= 5;					// not done
	m_EventPriorities[ESexyPed]						= 4;
	m_EventPriorities[ESexyCar]						= 4;
	m_EventPriorities[EGangMemberNearby]			= 2;
	m_EventPriorities[ECriminalNearby]				= 2;
	m_EventPriorities[ECopNearby]					= 2;
	m_EventPriorities[EProzzyNearby]				= 2;
	m_EventPriorities[EPedUsingAttractor]			= 1;
	m_EventPriorities[EPedSunbathing]				= 1;
	m_EventPriorities[EPedsChatting]				= 1;

	m_EventDurations[EPedGotKilled]					= 4000;
	m_EventDurations[EExplosion]					= 4000;
	m_EventDurations[ESwatTeamAbseiling]			= 8000;
	m_EventDurations[ECopKillingCriminal]			= 6000;
	m_EventDurations[EGangFight]					= 6000;
	m_EventDurations[EGangAttackingPed]				= 6000;
	m_EventDurations[EMeleeAction]					= 5000;
	m_EventDurations[ESeenMeleeAction]				= 5000;
	m_EventDurations[EGunshotFired]					= 5000;
	m_EventDurations[EHelicopterOverhead]			= 8000;
	m_EventDurations[ECarJacking]					= 6000;
	m_EventDurations[ERoadRage]						= 6000;
	m_EventDurations[EFistFight]					= 5000;
	m_EventDurations[ECarCrash]						= 6000;
	m_EventDurations[EPedKnockedOffBike]			= 6000;
	m_EventDurations[EPedRunOver]					= 6000;
	m_EventDurations[EMadDriver]					= 6000;
	m_EventDurations[EEmergencyServicesArrived]		= 8000;
	m_EventDurations[EPanickedPed]					= 5000;
	m_EventDurations[EPedRevived]					= 6000;
	m_EventDurations[EPlaneFlyby]					= 6000;					// not done
	m_EventDurations[ESexyPed]						= 3000;
	m_EventDurations[ESexyCar]						= 3000;
	m_EventDurations[EGangMemberNearby]				= 3000;
	m_EventDurations[ECriminalNearby]				= 3000;
	m_EventDurations[ECopNearby]					= 3000;
	m_EventDurations[EProzzyNearby]					= 3000;
	m_EventDurations[EPedUsingAttractor]			= 5000;
	m_EventDurations[EPedSunbathing]				= 5000;
	m_EventDurations[EPedsChatting]					= 5000;
}

CInterestingEvents::~CInterestingEvents(void)
{
	for(int e=0; e<MAX_NUM_INTERESTING_EVENTS; e++)
	{
		TInterestingEvent & pEvent = m_Events[e];
		if(pEvent.m_pEntity)
		{
			pEvent.m_pEntity = NULL;
		}
	}
}


void
CInterestingEvents::Add(EType eType, CEntity * pEntity)
{
	if(!m_bIsActive || !pEntity)
		return;

	bool bVisibleToCamera = true;

	Vector3 vCameraOrigin = camInterface::GetPos();

	if(m_iCurrentFrameCounter != fwTimer::GetSystemFrameCount())
	{
		m_iCurrentFrameCounter = fwTimer::GetSystemFrameCount();
		CPed * pPlayerPed = FindPlayerPed();
		const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
		m_ViewVec = vPlayerPos - vCameraOrigin;
		m_ViewVec.z = 0;
		//		if(m_ViewVec.NormalizeAndMag() == 0.0f)
		if(!m_ViewVec.NormalizeSafeRet())
		{
			// NB: just in case camera is directly above player
			m_ViewVec = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB());
		}
		m_ScanOrigin = vPlayerPos + (m_ViewVec * m_fEventRadius);
	}

	// get position 'fRadius' units in front of player, in view direction
	const float fRadiusSqr = m_fEventRadius * m_fEventRadius;

	const Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	// sqr'd 2d distance from scanningEntity to center
	Vector3 vDiff = m_ScanOrigin - vEntityPos;
	float fRadialDistSqr = (vDiff.x * vDiff.x) + (vDiff.y * vDiff.y);

	// scanningEntity must be within circle of interest, centered 'fRadius' units in front of player
	if(fRadialDistSqr > fRadiusSqr)
	{
		bVisibleToCamera = false;
	}

	if(m_bIgnoreEventsBehindPlayer)
	{
		// planar distance in front of player
		float fPlaneDist = - DotProduct(m_ViewVec, vCameraOrigin);
		float fPlanarDist = DotProduct(vEntityPos, m_ViewVec) + fPlaneDist;
		// scanningEntity must be in front of player
		if(fPlanarDist < 0.0f)
		{
			bVisibleToCamera = false;
		}
	}

	// do a line-test to see if this scanningEntity is initially visible
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vCameraOrigin, vEntityPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	bool bLOS = bVisibleToCamera && !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	if(!bLOS)
	{
		bVisibleToCamera = false;
	}

	u32 iTimeMs = fwTimer::GetTimeInMilliseconds();
	int iPriority = m_EventPriorities[eType];
	bool bCanAddNewEventOfType = iTimeMs > m_NextTimeToAcceptEvents[eType];


	for(int e=0; e<MAX_NUM_INTERESTING_EVENTS; e++)
	{
		TInterestingEvent & pEvent = m_Events[e];

		bool bLookingAtThisEvent = (m_iLookingAtEvent == e);

		// old event's duration has expired ?
		bool bDurationExpired;
		if(pEvent.m_eType > ENone && pEvent.m_eType < ENumCategories)
		{
			bDurationExpired = iTimeMs > pEvent.m_iStartTime + m_EventDurations[pEvent.m_eType];
		}
		else
		{
			bDurationExpired = true;
		}

		// old event's scanningEntity disappeared ?
		if(!pEvent.m_pEntity)
		{
			pEvent.m_eType = ENone;
			pEvent.m_pEntity = NULL;
			bLookingAtThisEvent = false;
		}

		EType eExistingType = (EType)pEvent.m_eType;
		int iExistingPriority = m_EventPriorities[pEvent.m_eType];

		// to add the new event, the existing one must've had it's scanningEntity disappeared - or else :
		// it must a greater priority, the existing event's duration must have expired & the delay
		// before making a new similar event must have expired
		if(eExistingType == ENone || ((iPriority >= iExistingPriority || bDurationExpired) && bCanAddNewEventOfType && !bLookingAtThisEvent))
		{
			pEvent.m_eType = eType;
			pEvent.m_pEntity = pEntity;
			pEvent.m_iStartTime = iTimeMs;
			pEvent.m_bVisibleToCamera = bVisibleToCamera;

			// Optionally wait an amount of time before accepting any events of the same type
			if(m_bUseTimeDelayBeforeAddingSimilarEvent)
			{
				//				m_NextTimeToAcceptEvents[eType] = iTimeMs + m_EventDurations[eType];
				m_NextTimeToAcceptEvents[eType] = iTimeMs + (m_EventDurations[eType] / 2);
			}
			else
			{
				m_NextTimeToAcceptEvents[eType] = iTimeMs;
			}

			break;
		}
	}
}


const TInterestingEvent *
CInterestingEvents::GetInterestingEvent(bool bForCamera)
{
	u32 iTimeMs = fwTimer::GetTimeInMilliseconds();

	// Optionally aways return the same event until its duration expires
	if(bForCamera && m_bWaitForEventDurationToComplete && m_iLookingAtEvent != -1)
	{
		// if current event not expired, return it again - to stop camera focus from flicking constantly
		TInterestingEvent * pLookingAtEvent = &m_Events[m_iLookingAtEvent];
		if(pLookingAtEvent->m_pEntity && iTimeMs < pLookingAtEvent->m_iStartTime + m_EventDurations[pLookingAtEvent->m_eType])
		{
			return pLookingAtEvent;
		}
	}

	s8 iBestEventPriority = ENone;
	s8 iBestEventIndex = -1;

	for(s8 e=0; e<MAX_NUM_INTERESTING_EVENTS; e++)
	{
		TInterestingEvent * pEvent = &m_Events[e];

		if(pEvent->m_pEntity && 
			(pEvent->m_bVisibleToCamera || !bForCamera) &&
			iTimeMs < pEvent->m_iStartTime + m_EventDurations[pEvent->m_eType])
		{
			if(m_EventPriorities[pEvent->m_eType] > iBestEventPriority || (fwRandom::GetRandomNumber() & 0xFFFF) < 128)
			{
				iBestEventPriority = m_EventPriorities[pEvent->m_eType];
				iBestEventIndex = e;
			}
		}
	}

	if( bForCamera )
	{
		m_iLookingAtEvent = iBestEventIndex;
	}

	if(iBestEventIndex != -1)
	{
		return &m_Events[iBestEventIndex];
	}
	else
	{
		return NULL;
	}
}


// If an event is no good, or no longer applicable - this function can be called
// to ensure that it gets deleted or superseded by a new event
void
CInterestingEvents::InvalidateEvent(const TInterestingEvent * pInvalidEvent)
{
	for(int i=0; i<MAX_NUM_INTERESTING_EVENTS; i++)
	{
		TInterestingEvent * pEvent = &m_Events[i];
		if(pEvent == pInvalidEvent)
		{
			pEvent->m_iStartTime = 0;

			if(pEvent->m_pEntity)
			{
				pEvent->m_pEntity = NULL;
			}

			if(m_iLookingAtEvent == i)
			{
				m_iLookingAtEvent = -1;
			}

			return;
		}
	}

}

// Tests all events to see if they're still visible, and invalidates those which are not
void
CInterestingEvents::InvalidateNonVisibleEvents(void)
{
	Vector3 vCameraOrigin = camInterface::GetPos();

	for(int i=0; i<MAX_NUM_INTERESTING_EVENTS; i++)
	{
		TInterestingEvent * pEvent = &m_Events[i];
		if(pEvent->m_pEntity)
		{
			// do a line-test to see if this scanningEntity is still visible
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vCameraOrigin, VEC3V_TO_VECTOR3(pEvent->m_pEntity->GetTransform().GetPosition()));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
			probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
			bool bLOS = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

			// no line of sight, so invalidate this event
			if(!bLOS)
			{
				pEvent->m_bVisibleToCamera = false;
			}
		}
	}
}

void CPhysicsEventScanner::Scan( CPed &ped )
{
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMinFallBeforeInAir, 2.0f, 0.0f, 10.0f, 0.001f);

	// Check if ped is in air and not being handled by the in air task
	// Disable this if the physics lod is active, until it disables the capsule collision correctly.
	if ( (ped.GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ) && 
		!ped.GetPedResetFlag(CPED_RESET_FLAG_IsClimbing) &&
		!ped.GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) &&
		!ped.IsDead() &&
		!ShouldIgnoreInAirEventDueToFallTask(ped) &&
		!(ped.GetAttachParent() && ((CPhysical*)ped.GetAttachParentForced())->GetIsTypeVehicle() && !ped.GetPedResetFlag(CPED_RESET_FLAG_CanAbortExitForInAirEvent))) ||
		(!ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) && !ped.GetPedResetFlag(CPED_RESET_FLAG_OverridePhysics) && !ped.GetIsStanding() && !ped.GetIsInWater() && !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) && 
		!ped.GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ) && CPedGeometryAnalyser::IsInAir(ped) ))
	{
		Vector3 vecStart = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
		Vector3 vecEnd = vecStart - (ZAXIS * fMinFallBeforeInAir);

		//grcDebugDraw::Line(vecStart, vecEnd, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 60 );

		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vecStart, vecEnd);
		probeDesc.SetExcludeEntity(&ped);				// Exclude ourselves! (IMPORTANT!)
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			// We didn't hit anything, generate the in Air Event
			CEventInAir event(&ped);
			ped.GetPedIntelligence()->AddEvent(event);
			return;
		}
	}

	if (!ped.GetPedResetFlag(CPED_RESET_FLAG_DisableNMForRiverRapids) &&
		ped.GetIsInWater() && 
		!ped.GetUsingRagdoll() &&
		!ped.IsDead() && 
		!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) && 
		!(ped.GetAttachParent() && ((CPhysical*)ped.GetAttachParentForced())->GetIsTypeVehicle()) && 
		!ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) && 
		!ped.GetPedResetFlag(CPED_RESET_FLAG_OverridePhysics) &&
		!CPedType::IsAnimalType(ped.GetPedType()))
	{
		if (CTaskMotionSwimming::CheckForRapids(&ped))
		{
			if (CPhysics::CanUseRagdolls() && ped.GetRagdollState() != RAGDOLL_STATE_ANIM_LOCKED && CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_FALL))
			{
				nmDebugf2("[%u] Adding nm river rapids task:%s(%p) Ped moving in rapid water.", fwTimer::GetTimeInMilliseconds(), ped.GetModelName(), &ped);
				CTaskNMRiverRapids* pTaskNM = rage_new CTaskNMRiverRapids(2000, 30000);
				CEventSwitch2NM eventRagdoll(30000, pTaskNM);
				ped.SwitchToRagdoll(eventRagdoll);
			}
		}
	}

	// Search for peds standing on moving objects such as vehicles, generate a natural
	// motion response that will throw them off.
	CPhysical* pGround = ped.GetGroundPhysical();
	if(pGround && !ped.GetUsingRagdoll())
	{
		bool canStandOnPhysical = false;
		if(pGround->GetIsTypePed())
		{
			canStandOnPhysical = true;
		}
		else
		{
			//Check if the ground is a vehicle, and the ped can stand on it.
			if(pGround->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pGround);

				bool bIsClimbingLadder = (ped.GetPedResetFlag(CPED_RESET_FLAG_IsClimbing) && ped.GetPedIntelligence()->GetTaskClimbLadder());
				if(!bIsClimbingLadder)
				{
					canStandOnPhysical =  pVehicle->CanPedsStandOnTop();
				}

				CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));	
				if (pEnterVehicleTask)
				{
					if (pVehicle->IsTurretSeat(pEnterVehicleTask->GetTargetSeat()))
					{
						canStandOnPhysical = true;
					}
				}
			}
		}
		
		// Don't allow similar event to get applied repeatedly over a short amount of time from the same source.
		// This is an attempt at determining the most important impact from a particular source over a short amount of time that
		// isn't frame-rate dependent.
		if (ped.GetRagdollInst())
		{
			if (ped.GetRagdollInst()->GetLastImpactDamageEntity() == pGround && ped.GetRagdollInst()->GetTimeSinceImpactDamage() < CCollisionEventScanner::RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT)
			{
				canStandOnPhysical = true;
			}
			else
			{
				// Note the new entity and damage taken from it
				ped.GetRagdollInst()->SetImpactDamageEntity(pGround);
				ped.GetRagdollInst()->SetAccumulatedImpactDamageFromLastEntity(0.0f);
			}
		}
		
		// Under certain circumstances we don't wish to do this - eg. when swimming above another model
		if(!canStandOnPhysical && !ped.GetIsSwimming() && !ped.GetCapsuleInfo()->IsQuadruped() && !ped.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollFromVehicleFallOff) && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir)) // horses are ok too B* 476793
		{
			if(CPhysics::CanUseRagdolls() && ped.GetRagdollState()!=RAGDOLL_STATE_ANIM_LOCKED)
			{
				if(CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_PHYSICAL_FALLOFF, ped.GetGroundPhysical()))
				{
					static float SPEED_TO_SAY_SCREAM_AUDIO = 7.5f;
					if(ped.GetVelocity().Mag2() > rage::square(SPEED_TO_SAY_SCREAM_AUDIO) && ped.GetSpeechAudioEntity())
					{
						audSpeechInitParams speechParams;
						speechParams.forcePlay = true;
						speechParams.allowRecentRepeat = true;
						ped.GetSpeechAudioEntity()->Say("HIGH_FALL", speechParams, ATSTRINGHASH("PAIN_VOICE", 0x048571d8d));
					}

					u32 grabFlags = 0;
					if (!CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer || !ped.IsPlayer())
					{
						grabFlags|=CGrabHelper::TARGET_AUTO_WHEN_FALLING;
					}

					nmDebugf2("[%u] Adding nm balance task:%s(%p) Ped balancing on moving vehicle.", fwTimer::GetTimeInMilliseconds(), ped.GetModelName(), &ped);
					CTaskNMBalance* pTaskNM = rage_new CTaskNMBalance(2000, 30000, ped.GetGroundPhysical(), grabFlags,NULL,0.0f,NULL,CTaskNMBalance::BALANCE_FORCE_FALL_FROM_MOVING_CAR);

					CEventSwitch2NM eventRagdoll(30000, pTaskNM);
					ped.SwitchToRagdoll(eventRagdoll);
				}
			}
		}
	}

	if(ped.GetPedResetFlag(CPED_RESET_FLAG_StandingOnForkliftForks) && !ped.GetUsingRagdoll() && !ped.GetIsSwimming())
	{
		if(CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_FORKLIFT_FORKS_FALLOFF, ped.GetGroundPhysical()))
		{
			static float SPEED_TO_SAY_SCREAM_AUDIO = 7.5f;
			if(ped.GetVelocity().Mag2() > rage::square(SPEED_TO_SAY_SCREAM_AUDIO) && ped.GetSpeechAudioEntity())
			{
				audSpeechInitParams speechParams;
				speechParams.forcePlay = true;
				speechParams.allowRecentRepeat = true;
				ped.GetSpeechAudioEntity()->Say("HIGH_FALL", speechParams, ATSTRINGHASH("PAIN_VOICE", 0x048571d8d));
			}

			u32 grabFlags = 0;
			if(!CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer || !ped.IsPlayer())
			{
				grabFlags|=CGrabHelper::TARGET_AUTO_WHEN_FALLING;
			}

			nmDebugf2("[%u] Adding nm balance task:%s(%p) Ped balancing on forklift forks.", fwTimer::GetTimeInMilliseconds(), ped.GetModelName(), &ped);
			CTaskNMHighFall* pTaskNM = rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_TEETER_EDGE);
			CEventSwitch2NM eventRagdoll(30000, pTaskNM);
			ped.SwitchToRagdoll(eventRagdoll);
		}
	}

	const CCollisionHistory* pCollisionHistory = ped.GetFrameCollisionHistory();
	// if ped is walking into something flag it for later so we can do stuff in CPedIntelligence()->Process()
	CCollisionHistoryIterator obstacleIterator(pCollisionHistory, /*bool bIterateBuildings=*/ true, /*bool bIterateVehicles=*/ true, 
														      /*bool bIteratePeds=*/ false, /*bool bIterateObjects=*/ true, /*bool bIterateOthers=*/ false);

	while(const CCollisionRecord* pObstacleColRecord = obstacleIterator.GetNext())
	{
		if(DotProduct(pObstacleColRecord->m_MyCollisionNormal, VEC3V_TO_VECTOR3(ped.GetTransform().GetB())) < -0.5f)
		{
			ped.SetPedResetFlag( CPED_RESET_FLAG_PedHitWallLastFrame, true );
			
			CEntity *pHitEntity = pObstacleColRecord->m_pRegdCollisionEntity;
			
			//B*1748361: If we're standing on a vehicle and hit an object at a reasonable velocity then trigger the ped to ragdoll 
			//same ragdoll effect as above: if(!canStandOnVehicle && !ped.GetIsSwimming() && !ped.GetCapsuleInfo()->IsQuadruped() && !ped.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollFromVehicleFallOff) && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) condition
			if(pGround && pGround->GetIsTypeVehicle() && (pHitEntity != pGround) )
			{
				static dev_float fMinRagdollVelocity = 5.0f;
				if (pGround->GetVelocity().Mag() > fMinRagdollVelocity)
				{
					if(CPhysics::CanUseRagdolls() && ped.GetRagdollState()!=RAGDOLL_STATE_ANIM_LOCKED)
					{
						if(CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_PHYSICAL_FALLOFF, ped.GetGroundPhysical()))
						{
							static float SPEED_TO_SAY_SCREAM_AUDIO = 7.5f;
							if(ped.GetVelocity().Mag2() > rage::square(SPEED_TO_SAY_SCREAM_AUDIO) && ped.GetSpeechAudioEntity())
							{
								audSpeechInitParams speechParams;
								speechParams.forcePlay = true;
								speechParams.allowRecentRepeat = true;
								ped.GetSpeechAudioEntity()->Say("HIGH_FALL", speechParams, ATSTRINGHASH("PAIN_VOICE", 0x048571d8d));
							}

							u32 grabFlags = 0;
							if (!CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer || !ped.IsPlayer())
							{
								grabFlags|=CGrabHelper::TARGET_AUTO_WHEN_FALLING;
							}

							nmDebugf2("[%u] Adding nm balance task:%s(%p) Ped balancing on moving vehicle.", fwTimer::GetTimeInMilliseconds(), ped.GetModelName(), &ped);
							CTaskNMBalance* pTaskNM = rage_new CTaskNMBalance(2000, 30000, ped.GetGroundPhysical(), grabFlags,NULL,0.0f,NULL,CTaskNMBalance::BALANCE_FORCE_FALL_FROM_MOVING_CAR);

							CEventSwitch2NM eventRagdoll(30000, pTaskNM);
							ped.SwitchToRagdoll(eventRagdoll);
						}
					}
				}
			}
			break;
		}
	}
}

bool CPhysicsEventScanner::ShouldIgnoreInAirEventDueToFallTask(const CPed& rPed) const
{
	//Check if the fall task is valid.
	const CTaskFall* pTaskFall = static_cast<CTaskFall *>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FALL));
	if(pTaskFall)
	{
		//Check if the parachute task is invalid.
		const CTaskParachute* pTaskParachute = static_cast<CTaskParachute *>(pTaskFall->FindSubTaskOfType(CTaskTypes::TASK_PARACHUTE));
		if(!pTaskParachute)
		{
			return true;
		}
		//Check if the ped is not landing.
		else if(!pTaskParachute->IsLanding())
		{
			return true;
		}
		//Check if the ped is crash landing, and can't parachute.
		else if(pTaskParachute->IsCrashLanding() && !CTaskParachute::CanPedParachute(rPed))
		{
			return true;
		}
	}

	return false;
}

const float CStaticMovementScanner::ms_fMillisecsWithoutCollisionTolerance = 4.0f * (1000.0f / 30.0f);
const float CStaticMovementScanner::ms_iStaticCountColPosToleranceSqr = (0.25f * 0.25f);
const int CStaticMovementScanner::ms_iStaticCounterStuckLimit = 30;
const int CStaticMovementScanner::ms_iStaticCounterStuckLimitPlayerMP = 90;
const int CStaticMovementScanner::ms_iStaticActivateObjectLimit = 10;
const float CStaticMovementScanner::ms_fMillisecsPerCount = 1000.0f / 30.0f;

void CStaticMovementScanner::Scan( CPed& ped )
{
	if(ped.GetIsInVehicle() || ped.GetPedResetFlag(CPED_RESET_FLAG_ResetMovementStaticCounter))
	{
		m_iStaticCounter = 0;
		m_fMillisecsCounter = 0.0f;
		return;
	}

	// Static counter check only makes sense for peds with physics enabled, since only these may collide with things

	if( ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics)==false )
	{
		const float fTimestepMS = fwTimer::GetTimeStep() * 1000.0f;

		// We used to do this here:
		//	const bool bMovingInCover = ped.GetPedIntelligence()->GetPedCoverStatus(coverState,bCanFire) && (coverState == CTaskInCover::State_AimIntro || coverState == CTaskInCover::State_AimOutro);
		// but now CTaskInCover sets CPED_RESET_FLAG_ForceMovementScannerCheck instead, for performance reasons.
		const bool forceCheck = ped.GetPedResetFlag(CPED_RESET_FLAG_ForceMovementScannerCheck);

		CTask * pTaskMotion = ped.GetPedIntelligence()->GetMotionTaskActiveSimplest();
		const bool bTurn180 =
			pTaskMotion && pTaskMotion->GetTaskType()==CTaskTypes::TASK_HUMAN_LOCOMOTION && pTaskMotion->GetState()==CTaskHumanLocomotion::State_Turn180;

		if(forceCheck || (ped.GetMotionData()->GetIsStill()==false && bTurn180==false) )
		{
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

			if(ped.GetFrameCollisionHistory()->GetNumCollidedEntities() > 0)
			{
				if(m_fMillisecsWithoutCollision > ms_fMillisecsWithoutCollisionTolerance)
				{
					m_vPedPositionAtFirstCollision = vPedPosition;
				}
				m_fMillisecsWithoutCollision = 0.0f;
			}
			else
			{
				m_fMillisecsWithoutCollision += fTimestepMS;
				m_fMillisecsWithoutCollision = Min(m_fMillisecsWithoutCollision, 9999.0f);
			}

			if(m_fMillisecsWithoutCollision > ms_fMillisecsWithoutCollisionTolerance)
			{
				m_iStaticCounter = 0;
				m_fMillisecsCounter = 0.0f;
			}
			else
			{
				if(m_vPedPositionAtFirstCollision.IsNonZero())
				{
					pedAssertf(m_vPedPositionAtFirstCollision.IsNonZero(), "m_vPedPositionAtFirstCollision has not been initialised!");

#if __BANK		// Ensure that debug timescale does not cause static counter to activate
					const Vector3 vDiff = (vPedPosition - m_vPedPositionAtFirstCollision) / fwTimer::GetDebugTimeScale();
#else
					const Vector3 vDiff = vPedPosition - m_vPedPositionAtFirstCollision;
#endif
					const float fPosTolerance = ped.GetCurrentMotionTask()->IsInWater() ? ms_iStaticCountColPosToleranceSqr * 2.0f : ms_iStaticCountColPosToleranceSqr;
					const float fFwdMovementSqr = (ped.IsStrafing() ? vDiff.XYMag2() : square(vDiff.Dot(VEC3V_TO_VECTOR3(ped.GetTransform().GetForward()))));

					if(fFwdMovementSqr < fPosTolerance)
					{
						m_fMillisecsCounter += fTimestepMS;
						while(m_fMillisecsCounter >= ms_fMillisecsPerCount)
						{
							m_fMillisecsCounter -= ms_fMillisecsPerCount;
							m_iStaticCounter++;
						}
					}
					else
					{
						m_fMillisecsCounter = 0.0f;
						m_iStaticCounter = 0;
						m_vPedPositionAtFirstCollision = vPedPosition;
					}
				}
			}
		}
		else
		{
			m_fMillisecsWithoutCollision += fTimestepMS;
			m_fMillisecsWithoutCollision = Min(m_fMillisecsWithoutCollision, 9999.0f);

			m_iStaticCounter = 0;
			m_fMillisecsCounter = 0.0f;
		}
	}
}
