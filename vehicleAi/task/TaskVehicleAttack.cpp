#include "TaskVehicleAttack.h"

// Framework headers
#include "fwmaths/geomutil.h"

// Game headers
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "VehicleAi/task/TaskVehicleGoTo.h"
#include "VehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "VehicleAi/task/TaskVehicleGoToPlane.h"
#include "VehicleAi/task/TaskVehicleGoToSubmarine.h"
#include "VehicleAi/task/TaskVehiclePlaneChase.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "VehicleAi/task/TaskVehicleCircle.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Vehicles/Heli.h"
#include "Vehicles/VehicleGadgets.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "game/modelindices.h"
#include "scene/world/gameWorld.h"
#include "weapons/info/WeaponInfo.h"
#include "Weapons/FiringPattern.h"
#include "weapons/Projectiles/Projectile.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
 
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
/////////////////////////////////////////////////////////////////////////////
static dev_float s_VehicleFiringConeDegrees = 25.0f;

CTaskVehicleAttack::CTaskVehicleAttack(const sVehicleMissionParams& params)
: CTaskVehicleMissionBase(params)
, m_fHeliRequestedOrientation(-1.0f)
, m_iFlightHeight(7)
, m_iMinHeightAboveTerrain(20)
, m_iHeliFlags(0)
, m_iSubFlags(0)
, m_bTargetWasAliveLastFrame(false)
, m_bHasLOS(true)
, m_uTimeWeaponSwap(0)
, m_uTimeLostLOS(0)
{
	// initialize this value to something
	FindTargetCoors(NULL, m_TargetLastAlivePosition);

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_ATTACK);
}


/////////////////////////////////////////////////////////////////////////////

CTaskVehicleAttack::~CTaskVehicleAttack()
{

}


/////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleAttack::ProcessPreFSM()
{
	if(!m_bMissionAchieved)
	{
		bool bTargetAlive = false;
		if (m_Params.GetTargetEntity().GetEntity())
		{
			if ( m_Params.GetTargetEntity().GetEntity()->GetIsTypeVehicle() )
			{
				const CVehicle& vehicle = static_cast<const CVehicle&>(*m_Params.GetTargetEntity().GetEntity());
				bTargetAlive = vehicle.GetStatus() != STATUS_WRECKED;
			}
			else if ( m_Params.GetTargetEntity().GetEntity()->GetIsTypePed() )
			{
				const CPed& ped = static_cast<const CPed&>(*m_Params.GetTargetEntity().GetEntity());
				bTargetAlive = !ped.IsInjured();
			}
		}

		if ( bTargetAlive || !m_Params.GetTargetEntity().GetEntity() )
		{
			FindTargetCoors(GetVehicle(), m_TargetLastAlivePosition);
		}
		else if ( m_bTargetWasAliveLastFrame )
		{
			SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, true);
			m_bMissionAchieved = true;
		}
		m_bTargetWasAliveLastFrame = bTargetAlive;
	}

	return FSM_Continue;
}


aiTask::FSM_Return CTaskVehicleAttack::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Attack state
		FSM_State(State_Attack)
			FSM_OnEnter
				Attack_OnEnter(pVehicle);
			FSM_OnUpdate
				return Attack_OnUpdate(pVehicle);

		// Circle state
		FSM_State(State_Circle)
			FSM_OnEnter
				Circle_OnEnter(pVehicle);
			FSM_OnUpdate
				return Circle_OnUpdate(pVehicle);

		// Stop state
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				Stop_OnUpdate(pVehicle);
	FSM_End
}

///////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAttack::Attack_OnEnter(CVehicle* pVehicle)
{
	pVehicle->SwitchEngineOn();
	SelectBestVehicleWeapon(*pVehicle);

	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_PLANE:
		{
			//Reset the burst firing timer as we've only just started attacking
			CPed* pFiringPed = pVehicle->GetDriver();
			if ( pFiringPed )
			{
				CFiringPattern& firingPattern = pFiringPed->GetPedIntelligence()->GetFiringPattern();
				firingPattern.ResetTimeUntilNextBurst();
			}

			SetTargetPosition(&m_TargetLastAlivePosition);
			sVehicleMissionParams params = m_Params;
			params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);

			CAITarget target;
			if ( !GetTargetEntity() )
			{
				Assertf(m_Params.IsTargetValid(), "CTaskVehicleAttack::Attack_OnEnter : Target is invalid. %s", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : pVehicle->GetDebugName());
				target.SetPosition( m_Params.GetTargetPosition() );
			}
			else
			{
				const CEntity* pTargetEntity = GetTargetEntity();
				const CVehicle* pVehicle = NULL;
				if ( pTargetEntity->GetIsTypePed() )
				{
					const CPed* pPed = static_cast<const CPed*>(pTargetEntity);
					pVehicle = pPed->GetVehiclePedInside();			
				}
				else if ( pTargetEntity->GetIsTypeVehicle() )
				{
					pVehicle = static_cast<const CVehicle*>(pTargetEntity);
				}

				Vector3 vTargetOffset = Vector3::ZeroType;
				if ( pVehicle )
				{
					if (const_cast<CVehicle*>(pVehicle)->IsInAir() && pVehicle->InheritsFromPlane() )
					{
						static Vector3 s_vPlaneAttackOffset(0.0f, -20.0f, 2.0f);
						vTargetOffset = s_vPlaneAttackOffset;
					}
					else
					{
						static Vector3 s_vVehicleAttackOffset(0.0f, 0.0f, 15.0f);
						vTargetOffset = s_vVehicleAttackOffset;
					}
				}
				
				target.SetEntityAndOffsetUnlimited(GetTargetEntity(), vTargetOffset);
			}
			
			SetNewTask(rage_new CTaskVehiclePlaneChase(params, target ) );
		}
		break;
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		{
			SetTargetPosition(&m_TargetLastAlivePosition);
			sVehicleMissionParams params = m_Params;
			params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);

			SetNewTask(rage_new CTaskVehicleGoToHelicopter(params, m_iHeliFlags, m_fHeliRequestedOrientation, m_iMinHeightAboveTerrain));
		}
		break;
	case VEHICLE_TYPE_SUBMARINE:
	case VEHICLE_TYPE_SUBMARINECAR:
		{
			SetTargetPosition(&m_TargetLastAlivePosition);
			sVehicleMissionParams params = m_Params;
			params.m_iDrivingFlags.SetFlag(DF_DontTerminateTaskWhenAchieved);
			SetNewTask(rage_new CTaskVehicleGoToSubmarine(m_Params, m_iSubFlags, m_iMinHeightAboveTerrain));
		}
		break;
	default:
		{
			//any vehicle with mounted weapon should use tank attack rather than just raming
			if(pVehicle->IsTank() || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
			{
				SetNewTask(rage_new CTaskVehicleAttackTank(m_Params));
			}
			else
			{
				sVehicleMissionParams params = m_Params;
				if(pVehicle->IsTank())
				{
					params.m_iDrivingFlags = DMode_PloughThrough|DF_DriveIntoOncomingTraffic;
				}	
				params.m_fTargetArriveDist = 0.0f;

				SetNewTask(CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST));
			}
		}
		break;
	}
}

bool IsTargetInTheAir(const CEntity* in_Entity)
{
	if ( in_Entity != NULL )
	{
		if ( in_Entity->GetIsTypeVehicle() )
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(in_Entity);
			return pVehicle->IsInAir();
		}
	}
	return false;
}

bool IsTargetInVehicle(const CEntity* in_Entity)
{
	if ( in_Entity != NULL )
	{
		if ( in_Entity->GetIsTypeVehicle() )
		{
			return true;
		}
		else if ( in_Entity->GetIsTypePed() )
		{
			const CPed* pPed = static_cast<const CPed*>(in_Entity);
			return pPed->GetIsInVehicle();
		}
	}
	return false;
}


void CTaskVehicleAttack::SelectBestVehicleWeapon(CVehicle& in_Vehicle)
{
	static u32 s_WeaponSwapDealy = 10 * 1000;
	if ( fwTimer::GetTimeInMilliseconds() - m_uTimeWeaponSwap > s_WeaponSwapDealy )
	{
		m_uTimeWeaponSwap = fwTimer::GetTimeInMilliseconds();
		if ( !in_Vehicle.GetIntelligence()->IsWeaponSelectionBlocked() )
		{
			CPed* pFiringPed = in_Vehicle.GetDriver();
			if ( pFiringPed )
			{
				int seatIndex = in_Vehicle.GetSeatManager()->GetPedsSeatIndex(pFiringPed);			
		
				CVehicleWeaponMgr* pVehicleWeaponMgr = in_Vehicle.GetVehicleWeaponMgr();
				if( pVehicleWeaponMgr )
				{
					const CVehicleWeapon* pEquippedVehicleWeapon = pFiringPed->GetWeaponManager()->GetEquippedVehicleWeapon();

					atArray<const CVehicleWeapon*> vehicleWeapons;
					pVehicleWeaponMgr->GetWeaponsForSeat(seatIndex, vehicleWeapons);
				
					Vector3 vHeliPosition = VEC3V_TO_VECTOR3(in_Vehicle.GetVehiclePosition());
					Vector3 vTargetPosition = m_TargetLastAlivePosition;
					bool bIsLongRange = vHeliPosition.Dist2(vTargetPosition) > 25.0f; 

					// iterate weapons and score the best one to use
					float fBestScore = -1000.0f;
					int iBestIndex = -1;
					for (int i = 0; i < vehicleWeapons.GetCount(); i++ )
					{
						float fScore = 0.0f;

						const CVehicleWeapon* pVehicleWeapon = vehicleWeapons[i];
						if (pVehicleWeapon)
						{
							const CWeaponInfo* pWeaponInfo = pVehicleWeapon->GetWeaponInfo();
							if (pWeaponInfo)
							{
								if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
								{
									fScore += bIsLongRange ? 10.0f : -10.0f;

									if (!IsTargetInTheAir(m_Params.GetTargetEntity().GetEntity()))
									{
										if (GetVehicle()->InheritsFromPlane())
										{
											// always pick rockets for planes
											// against ground targets
											// could just set the index but this 
											fScore += 1000.0f;
										}
									}
								}
								else if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET)
								{
									if (!IsTargetInTheAir(m_Params.GetTargetEntity().GetEntity()))
									{
										if (!IsTargetInVehicle(m_Params.GetTargetEntity().GetEntity()))
										{
											if (GetVehicle()->InheritsFromHeli())
											{
												if (NetworkInterface::IsGameInProgress())
												{
													// always pick 
													fScore += 1000.0f;
												}
											}
										}
									}
								}


								// prefer different weapon
								if (pEquippedVehicleWeapon == pVehicleWeapon)
								{
									fScore -= 50.0f;
								}

								if (fScore > fBestScore)
								{
									fBestScore = fScore;
									iBestIndex = i;
								}
							}
						}
					}

					if ( iBestIndex >= 0 )
					{
						// only equip if we have not already equipted a weapon
						if ( pEquippedVehicleWeapon != vehicleWeapons[iBestIndex] )
						{
							s32 nVehicleWeaponIndex = pVehicleWeaponMgr->GetVehicleWeaponIndexByHash( vehicleWeapons[iBestIndex]->GetHash() );
							pFiringPed->GetWeaponManager()->EquipWeapon( 0, nVehicleWeaponIndex, false );
						}
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleAttack::IsAbleToFireWeapons(CVehicle& vehicle, const CPed* pFiringPed)
{
	if (pFiringPed && !pFiringPed->IsPlayer())
	{
		const CPed* pDriver = vehicle.GetDriver();
		if(pDriver && (pDriver == pFiringPed) && (pDriver->IsPlayer() || ( vehicle.GetStatus() != STATUS_WRECKED && vehicle.GetStatus() != STATUS_OUT_OF_CONTROL)))
		{
			return true;
		}
		else if (pFiringPed)
		{
			const s32 iSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(pFiringPed);
			if (vehicle.IsTurretSeat(iSeatIndex) || (vehicle.GetVehicleWeaponMgr() && vehicle.GetVehicleWeaponMgr()->GetNumWeaponsForSeat(iSeatIndex) > 0))
			{
				return true;
			}
		}

		return false;
	}

	return true;
}

aiTask::FSM_Return	CTaskVehicleAttack::Attack_OnUpdate(CVehicle* pVehicle)
{
	// Sometimes the target may have been removed for a frame or so.
	// In that case don't do anything.
// 	if(!GetTargetEntity())
// 	{
// 		SetState(State_Stop);
// 		return FSM_Continue;
// 	}	

	//don't attack when wrecked. This task'll be deleted soon enough
	CPed* pFiringPed = pVehicle ? pVehicle->GetDriver() : NULL;
	if(!IsAbleToFireWeapons(*pVehicle, pFiringPed))
	{
		return FSM_Continue;
	}

	bool bAttemptFire = false;
	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_BOAT:
		{
			if (pVehicle->GetIsJetSki() && NetworkInterface::IsGameInProgress())
				bAttemptFire = true;

			break;
		}
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		{
			if( GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER) )
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}

			Vector3 vHeliPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
			Vector3 vTargetPosition = m_TargetLastAlivePosition;		
			Vector3 vStrafePosition = vTargetPosition - vHeliPosition;

			float offset = m_Params.m_fTargetArriveDist;

			// This is multiplayer only just so we don't mess up singleplayer missions
			if ( NetworkInterface::IsGameInProgress() )
			{
				CVehicleWeapon* pEquippedVehicleWeapon = pFiringPed ? pFiringPed->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
				if ( pEquippedVehicleWeapon )
				{
					static float s_fAngleFraction = 0.6f;
					static float s_fMaxOffset = 75.0f;
					float maxoffset = Min(pEquippedVehicleWeapon->GetWeaponInfo()->GetRange(), s_fMaxOffset);
					float minoffset = fabsf(vStrafePosition.z) / sin(pEquippedVehicleWeapon->GetWeaponInfo()->GetVehicleAttackAngle() * DtoR * s_fAngleFraction);
					offset = Max(offset, minoffset);
					offset = Min(offset, maxoffset);
				}
			}

			vStrafePosition.z = 0.0f;
			vStrafePosition.Normalize();
			float fHeadingToTarget = fwAngle::GetATanOfXY(vStrafePosition.x, vStrafePosition.y);
			vStrafePosition.Scale(offset);
			vStrafePosition = vTargetPosition - vStrafePosition;

			aiFatalAssertf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER, "Unexpected subtask!");
			CTaskVehicleGoToHelicopter* pSubtask = static_cast<CTaskVehicleGoToHelicopter*>(GetSubTask());
			pSubtask->SetTargetPosition(&vStrafePosition);
			pSubtask->SetTargetEntity(NULL);

			// This is multiplayer only just so we don't mess up singleplayer missions
			if ( NetworkInterface::IsGameInProgress() )
			{
				pSubtask->SetDrivingFlag(DF_TargetPositionOverridesEntity, true);  
			}

			if( (vHeliPosition-vTargetPosition).XYMag2() < rage::square(offset + 20.0f) )
			{
				if (m_iHeliFlags & CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation)
				{
					fHeadingToTarget += m_fHeliRequestedOrientation;
				}

				pSubtask->SetOrientation(fHeadingToTarget);
				pSubtask->SetOrientationRequested(true);
			}
			else
			{
				pSubtask->SetOrientationRequested(false);
			}

			//check we have line of sight for rockets
			CVehicleWeapon* pEquippedVehicleWeapon =  pFiringPed ? pFiringPed->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
			if ( pEquippedVehicleWeapon && pEquippedVehicleWeapon->GetWeaponInfo()->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
			{		
				if( m_LOSResult.GetResultsReady())
				{
					bool bHadLOS = m_bHasLOS;
					m_bHasLOS = true;

					if(m_LOSResult.GetNumHits() > 0)
					{
						Vector3 vHitPos = VEC3V_TO_VECTOR3(m_LOSResult[0].GetPosition());
						float fDistToTargetSqr = vHitPos.Dist2(vTargetPosition);
						//allow anything closer than 10 meters to target to be ignored - missiles could still do damage
						if(fDistToTargetSqr >= 100.0f)
						{
							if(bHadLOS)
							{
								m_uTimeLostLOS = fwTimer::GetTimeInMilliseconds();
							}
							m_bHasLOS = false;
						}
#if DEBUG_DRAW
						bLosHit = true;
						vHitLocation = vHitPos;
#endif
					}
#if DEBUG_DRAW
					else
					{
						bLosHit = false;
					}
#endif
					m_LOSResult.Reset();
				}

				if(!m_LOSResult.GetWaitingOnResults())
				{
#if !DEBUG_DRAW
					WorldProbe::CShapeTestProbeDesc m_LosProbe;
#endif					
					m_LosProbe.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
					m_LosProbe.SetExcludeInstance(pVehicle->GetCurrentPhysicsInst());
					m_LosProbe.SetResultsStructure(&m_LOSResult);
					m_LosProbe.SetMaxNumResultsToUse(1);
					m_LosProbe.SetIsDirected(true);
					m_LosProbe.SetStartAndEnd(vHeliPosition, vTargetPosition);
					WorldProbe::GetShapeTestManager()->SubmitTest(m_LosProbe, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
				}
			}
			else
			{
				m_bHasLOS = true;
			}

			bAttemptFire = true;
			break;
		}
	case VEHICLE_TYPE_PLANE:
		{
			if( GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_PLANE_CHASE) )
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}

			bAttemptFire = true;
			break;
		}
	case VEHICLE_TYPE_SUBMARINE:
	case VEHICLE_TYPE_SUBMARINECAR:
		{
			if( GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE) )
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}

			Vector3 vTargetPosition = m_TargetLastAlivePosition;

			aiFatalAssertf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE, "Unexpected subtask!");
			CTaskVehicleGoToSubmarine* pSubtask = static_cast<CTaskVehicleGoToSubmarine*>(GetSubTask());
			pSubtask->SetTargetPosition(&vTargetPosition);
			pSubtask->SetTargetEntity(NULL);
			break;
		}
	default:
		break;
	}

	//allowed to fire if had line of sight in last half second
	bool bHasLOSRecently = m_bHasLOS || fwTimer::GetTimeInMilliseconds() - m_uTimeLostLOS < 500;
	if(bAttemptFire && !m_bMissionAchieved && pFiringPed && bHasLOSRecently)
	{
		CVehicleWeapon* pEquippedVehicleWeapon = pFiringPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		const CFiringPattern& firingPattern = pFiringPed->GetPedIntelligence()->GetFiringPattern();
		const bool readyToFire = firingPattern.ReadyToFire();
		const bool wantsToFire = firingPattern.WantsToFire();
		if( pEquippedVehicleWeapon && pEquippedVehicleWeapon->GetIsEnabled() && readyToFire && wantsToFire )
		{
			Vector3 vTargetPos = m_TargetLastAlivePosition;
			FireAtTarget( pFiringPed, pVehicle, pEquippedVehicleWeapon, GetTargetEntity(), vTargetPos );
		}
		else
		{
			SelectBestVehicleWeapon(*pVehicle);
		}
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////

void		CTaskVehicleAttack::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

//////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleAttack::Stop_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAttack::Circle_OnEnter(CVehicle* pVehicle)
{
	pVehicle->SwitchEngineOn();
	CTaskVehicleCircle* pTask = rage_new CTaskVehicleCircle(m_Params);
	pTask->SetHelicopterSpecificParams(m_fHeliRequestedOrientation, m_iFlightHeight, m_iMinHeightAboveTerrain, m_iHeliFlags);
	SetNewTask(pTask);
}

//////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleAttack::Circle_OnUpdate(CVehicle* pVehicle)
{
	// Sometimes the target may have been removed for a frame or so.
	// In that case don't do anything.
// 	if(!GetTargetEntity())
// 	{
// 		SetState(State_Stop);
// 		return FSM_Continue;
// 	}			

	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_BOAT:
		break;
	case VEHICLE_TYPE_PLANE:
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		{
			CPed* pDriver = pVehicle->GetDriver();
			CVehicleWeapon* pEquippedVehicleWeapon = pDriver ? pDriver->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
			if( pEquippedVehicleWeapon )
			{
				Vector3 vTargetPos = m_TargetLastAlivePosition;
				FireAtTarget( pDriver, pVehicle, pEquippedVehicleWeapon, GetTargetEntity(), vTargetPos );
			}
			break;
		}
	default:
		break;
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FireAtTarget
// PURPOSE :  If we are in a good position to fire a vehicle weapon we go for it.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleAttack::FireAtTarget(CPed* pFiringPed, CVehicle* pVehicle, CVehicleWeapon* pVehicleWeapon, const CEntity* pTarget, const Vector3& vTargetPos)
{
	Assert(pVehicleWeapon);
	const CWeaponInfo* pWeaponInfo = pVehicleWeapon->GetWeaponInfo();

	const Vector3 vHeliPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	const Vector3 vTargetPosition = vTargetPos;
	float distToTarget = (vHeliPosition - vTargetPosition).Mag();
	if(distToTarget >= pWeaponInfo->GetRange() || distToTarget <= 10.0f)
	{
		return;
	}

	const Vector3 vecForward(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()));
	Vector3 vToTargetDirection = vTargetPosition - vHeliPosition;
	vToTargetDirection.Normalize();

	// Make sure the target is in firing cone

	float fAttackAngle = pWeaponInfo ? pWeaponInfo->GetVehicleAttackAngle() : s_VehicleFiringConeDegrees;

	if(DotProduct(vecForward, vToTargetDirection ) <= cos(fAttackAngle*DtoR))
	{
		return;
	}

	Vector3 Origin = vHeliPosition + (vecForward * 4.0f);

	// Make sure we're not too close to the ground or otherwise placing missile in a collision state
	if(IsTooCloseToGeometryToFire(pVehicle, Origin, 8.0f))
	{
		return;
	}

	// Register the event with the firing pattern
	if( pFiringPed )
	{
		pFiringPed->GetPedIntelligence()->GetFiringPattern().ProcessFired();
	}

	pVehicleWeapon->Fire( pFiringPed, pVehicle, vTargetPosition, pTarget );
}


aiTask* CTaskVehicleAttack::Copy() const
{
	CTaskVehicleAttack* pTask = rage_new CTaskVehicleAttack(m_Params);
	pTask->SetHelicopterSpecificParams(m_fHeliRequestedOrientation, m_iFlightHeight, m_iMinHeightAboveTerrain, m_iHeliFlags);
	pTask->SetSubmarineSpecificParams(m_iMinHeightAboveTerrain, m_iSubFlags);
	return pTask;
}

void CTaskVehicleAttack::SetHelicopterSpecificParams( float fHeliRequestedOrientation, int iFlightHeight, int iMinHeightAboveTerrain, s32 iHeliFlags )
{
	m_fHeliRequestedOrientation = fHeliRequestedOrientation;
	m_iFlightHeight				= iFlightHeight; 
	m_iMinHeightAboveTerrain	= iMinHeightAboveTerrain;
	m_iHeliFlags				= iHeliFlags;
}

void CTaskVehicleAttack::SetSubmarineSpecificParams(int iMinHeightAboveTerrain, s32 iSubFlags )
{
	m_iSubFlags					= iSubFlags;
	m_iMinHeightAboveTerrain	= iMinHeightAboveTerrain;
}

bool CTaskVehicleAttack::IsTooCloseToGeometryToFire(CVehicle* pVehicle, const Vector3& vPos, const float fRadius)
{
	//Create the type flags.
	s32 iTypeFlags = ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_OBJECT;
	
	//Create the control flags.
	s32 iControlFlags = SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS;
	
	//Create the intersection sphere.
	fwIsSphereInside intersection(spdSphere(RCC_VEC3V(vPos), ScalarV(fRadius)));
	
	//Scan the intersecting entities.
	GeometryTestResults results(pVehicle);
	CGameWorld::ForAllEntitiesIntersecting(&intersection, CTaskVehicleAttack::IsTooCloseToGeometryToFireCB, &results, iTypeFlags, iControlFlags);
	
	return results.m_bObstructed;
}

bool CTaskVehicleAttack::IsTooCloseToGeometryToFireCB(fwEntity* pEntity, void* pData)
{
	//Grab the CEntity.
	CEntity* pGameEntity = static_cast<CEntity *>(pEntity);
	
	//Grab the results.
	GeometryTestResults* pResults = (GeometryTestResults *)pData;
		
	//Exclude the vehicle.
	if(pGameEntity == pResults->m_pVehicle)
	{
		//Keep looking.
		return true;
	}
	
	//Check the physics instance.
	phInst* pInst = pGameEntity->GetCurrentPhysicsInst();
	if(pInst && pInst->IsInLevel())
	{
		//Exclude weapons, weapon components, and projectiles.
		if(PHLEVEL->GetInstanceTypeFlag(pInst->GetLevelIndex(), ArchetypeFlags::GTA_PROJECTILE_TYPE))
		{
			//Keep looking.
			return true;
		}
	}
	
	//We are obstructed.
	pResults->m_bObstructed = true;
	
	//No need to continue looking.
	return false;
}

/////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskVehicleAttack::Debug() const
{
#if DEBUG_DRAW
	static bool s_RenderFiringCone = false;
	if ( s_RenderFiringCone )
	{
		const CVehicle* pVehicle = GetVehicle();

		const Vector3 vHeliPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
		const Vector3 vTargetPosition = m_TargetLastAlivePosition;
		const Vector3 vecForward(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()));

		Color32 color = Color32(0, 0, 255, 128);

		// Make sure the target is in front of us and not behind.
		Vector3 vToTargetDirection = vTargetPosition - vHeliPosition;
		vToTargetDirection.Normalize();

		if(DotProduct(vecForward, vToTargetDirection ) <= cos(s_VehicleFiringConeDegrees*DtoR))
		{
			color = Color32(255, 0, 0, 128);
		}

		Vector3 vDelta = vTargetPosition - vHeliPosition;
		float radius = sin(s_VehicleFiringConeDegrees*DtoR) * vDelta.Mag();
		grcDebugDraw::Cone(VECTOR3_TO_VEC3V(vHeliPosition), VECTOR3_TO_VEC3V(vHeliPosition + (vDelta.Mag() * vecForward)),  radius, color);
		//grcDebugDraw::Line(vHeliPosition, vHeliPosition + (vDelta.Mag() * vecForward), color, color);
		grcDebugDraw::Sphere(vTargetPosition, 1.0f, Color_green);
	}

	static bool s_RenderLOSTest = true;
	if(s_RenderLOSTest)
	{
		CPed* pFiringPed = GetVehicle() ? GetVehicle()->GetDriver() : NULL;
		CVehicleWeapon* pEquippedVehicleWeapon = pFiringPed ? pFiringPed->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
		if ( pEquippedVehicleWeapon &&  pEquippedVehicleWeapon->GetWeaponInfo()->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{
			Color32 drawColor = m_bHasLOS ? Color_green : Color_red;
			grcDebugDraw::Line(VECTOR3_TO_VEC3V(m_LosProbe.GetStart()), VECTOR3_TO_VEC3V(m_LosProbe.GetEnd()), drawColor);
			if(bLosHit)
			{
				grcDebugDraw::Sphere(vHitLocation, 0.25f, drawColor);
			}
		}
	}

	static bool s_RenderFiringPattern = true;
	if ( s_RenderFiringPattern )
	{
		const CVehicle* pVehicle = GetVehicle();
		if ( pVehicle )
		{
			CPed* pFiringPed = pVehicle ? pVehicle->GetDriver() : NULL;
			if ( pFiringPed )
			{
				if ( pFiringPed->GetWeaponManager() )
				{
					CVehicleWeapon* pEquippedVehicleWeapon = pFiringPed->GetWeaponManager()->GetEquippedVehicleWeapon();
					if ( pEquippedVehicleWeapon )
					{
						const CFiringPattern& firingPattern = pFiringPed->GetPedIntelligence()->GetFiringPattern();
						const bool readyToFire = firingPattern.ReadyToFire();
						const bool wantsToFire = firingPattern.WantsToFire();

						char debugText[128];
						sprintf(debugText, "%s (readyToFire: %s, wantsToFire: %s)", pEquippedVehicleWeapon->GetName(), readyToFire ? "True" : "False", wantsToFire ? "True" : "False");
						grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), Color_black, 0, 0, debugText);
					}
				}	
			}		
		}
	}


#endif

}

const char * CTaskVehicleAttack::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Attack&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Attack",   
		"State_Circle",
		"State_Stop"
	};

	return aStateNames[iState];
}
#endif

/////////////////////////////////////////////////////////////////////////////

CTaskVehicleAttackTank::CTaskVehicleAttackTank(const sVehicleMissionParams& params)
: CTaskVehicleMissionBase(params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_ATTACK_TANK);
}


/////////////////////////////////////////////////////////////////////////////

CTaskVehicleAttackTank::~CTaskVehicleAttackTank()
{

}


/////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleAttackTank::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Approach)
			FSM_OnEnter
				Approach_OnEnter();
			FSM_OnUpdate
				return Approach_OnUpdate();

		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter();
			FSM_OnUpdate
				return Stop_OnUpdate();

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

///////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAttackTank::Start_OnUpdate()
{
	const CVehicle* pVehicle = GetVehicle();
	if ( pVehicle && pVehicle->GetIntelligence()->m_bStationaryInTank )
	{
		SetState(State_Stop);
	}
	else
	{
		SetState(State_Approach);
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAttackTank::Approach_OnEnter()
{
	sVehicleMissionParams params(m_Params);
	params.m_iDrivingFlags = DMode_AvoidCars_Reckless | DF_DriveIntoOncomingTraffic;
	params.m_fTargetArriveDist = 20.0f;

	SetNewTask(CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST));
}

///////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAttackTank::Approach_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Stop);
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAttackTank::Stop_OnEnter()
{
	SetNewTask(rage_new CTaskVehicleStop(CTaskVehicleStop::SF_UseFullBrake));
}

//////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAttackTank::Stop_OnUpdate()
{
	//B*2421581, infinite loop detected here switching between approach and stop
	//probably because vehicle in unresponsive state causing goto subtask to exit
	if(GetPreviousState() == State_Approach && m_TimeInState < 0.5f)
	{
		return FSM_Continue;
	}

	const CVehicle* pVehicle = GetVehicle();
	if ( pVehicle && pVehicle->GetIntelligence()->m_bStationaryInTank )
	{
		return FSM_Continue;
	}

	Vec3V vTargetPosition;
	FindTargetCoors(GetVehicle(), RC_VECTOR3(vTargetPosition));
	ScalarV scDistSq = DistSquared(GetVehicle()->GetTransform().GetPosition(), vTargetPosition);
	static dev_float s_fMinDistance = 10.0f;
	ScalarV scMinDistSq = ScalarVFromF32(square(s_fMinDistance));
	static dev_float s_fMaxDistance = 30.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		SetState(State_Approach);
	}
	else if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		SetState(State_Flee);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAttackTank::Flee_OnEnter()
{
	sVehicleMissionParams params(m_Params);
	params.m_iDrivingFlags.SetFlag(DF_DriveIntoOncomingTraffic);
	params.m_iDrivingFlags.SetFlag(DF_PreventJoinInRoadDirectionWhenMoving);

	SetNewTask(rage_new CTaskVehicleFlee(params));
}

//////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAttackTank::Flee_OnUpdate()
{
	Vec3V vTargetPosition;
	FindTargetCoors(GetVehicle(), RC_VECTOR3(vTargetPosition));
	ScalarV scDistSq = DistSquared(GetVehicle()->GetTransform().GetPosition(), vTargetPosition);
	static dev_float s_fMaxDistance = 20.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		SetState(State_Stop);
	}

	return FSM_Continue;
}

aiTask* CTaskVehicleAttackTank::Copy() const
{
	CTaskVehicleAttackTank* pTask = rage_new CTaskVehicleAttackTank(m_Params);
	return pTask;
}

/////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskVehicleAttackTank::Debug() const
{
#if DEBUG_DRAW
#endif
}

const char * CTaskVehicleAttackTank::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",   
		"State_Approach",
		"State_Stop",
		"State_Flee",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif
