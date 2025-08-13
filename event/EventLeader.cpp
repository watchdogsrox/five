// Game headers
#include "Event/EventLeader.h"
#include "Network/NetworkInterface.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/General/TaskBasic.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "vehicles/vehicle.h"
 
#include "event/EventLeader_parser.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventLeaderEnteredCarAsDriver
//////////////////////////////////////////////////////////////////////////

CEventLeaderEnteredCarAsDriver::CEventLeaderEnteredCarAsDriver()
{
#if !__FINAL
	m_EventType = EVENT_LEADER_ENTERED_CAR_AS_DRIVER;
#endif
}

CEventLeaderEnteredCarAsDriver::~CEventLeaderEnteredCarAsDriver()
{
}

bool CEventLeaderEnteredCarAsDriver::AffectsPedCore(CPed* pPed) const
{
	CPedGroup* pPedGroup = pPed->GetPedsGroup();
	if(pPedGroup)
	{
		CPed* pLeader = pPedGroup->GetGroupMembership()->GetLeader();
		if(pLeader && pLeader != pPed)
		{
			CVehicle* pLeadersVehicle = NULL;
			if(pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pLeader->GetMyVehicle())
			{
				pLeadersVehicle = pLeader->GetMyVehicle();
			}
			else if(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true))
			{
				pLeadersVehicle = static_cast<CVehicle*>(pLeader->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE));
			}

			if (pLeadersVehicle && pLeadersVehicle->GetTransform().GetC().GetZf() < 0.3f && !pLeadersVehicle->InheritsFromBike())
			{
				return false;
			}
		}
	}
	return true;
}

bool CEventLeaderEnteredCarAsDriver::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CPedGroup* pPedGroup = pPed->GetPedsGroup();
	if(pPedGroup)
	{
		CPed* pLeader = pPedGroup->GetGroupMembership()->GetLeader();
		if(pLeader && pLeader != pPed)
		{
			// If the ped isn't following a scripted task
			if(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY) == NULL)
			{
				CVehicle* pLeadersVehicle = NULL;
				if(pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pLeader->GetMyVehicle())
				{
					pLeadersVehicle = pLeader->GetMyVehicle();
				}
				else if(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true))
				{
					pLeadersVehicle = static_cast<CVehicle*>(pLeader->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE));
				}

				if(pLeadersVehicle && !(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() == pLeadersVehicle) && 
					!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE))
				{
					const bool bHeliOffTheGround = pLeadersVehicle->GetIsRotaryAircraft() && pLeadersVehicle->HasContactWheels() == false;

					if(!bHeliOffTheGround)
					{
						s32 iPriority = pPedGroup->GetGroupMembership()->GetNonLeaderMemberPriority(pPed);
						const int iNoOfSeats = pLeadersVehicle->GetSeatManager()->GetMaxSeats();
						//const int iNoGroupMembers = pPedGroup->GetGroupMembership()->CountMembersExcludingLeader();
						SeatRequestType iSeatRequestType = SR_Any;
						s32 iSeat = -1;

						if(pPed->m_PedConfigFlags.GetPassengerIndexToUseInAGroup() != -1)
						{
							iPriority = pPed->m_PedConfigFlags.GetPassengerIndexToUseInAGroup();

							if(iPriority < iNoOfSeats)
							{
								bool bUseSpecificSeat = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex);

								// If script have set PCF_OnlyUseForcedSeatWhenEnteringHeli and the leader isn't getting into a heli,
								// we do not use the forced seat index
								if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUseForcedSeatWhenEnteringHeliInGroup) && !pLeadersVehicle->InheritsFromHeli())
								{
									bUseSpecificSeat = false;
								}

								if (bUseSpecificSeat)
								{
									iSeatRequestType = SR_Specific;
								}
								else
								{
									iSeatRequestType = SR_Prefer;
								}
								iSeat = iPriority;
							}
							else
							{
								iSeatRequestType = SR_Any;
								iSeat = -1;
							}
						}

						if(iSeat != -1 || (iSeatRequestType == SR_Any && pLeadersVehicle->GetSeatManager()->GetMaxSeats() > 1) )
						{
							// We don't want to use the driver seat here, so we check to see if there may be free seats
							// that are not driver seats, before giving the task.
							// Note: theoretically, since this event is called CEventLeaderEnteredCarAsDriver,
							// the leader would be in the driver seat. However, the way things are right now,
							// this event actually gets sent even if the leader is not actually driving, as long
							// as he's in the vehicle.
							// We consider seats with peds we can jack in as free though -> B*952331
							if(pLeadersVehicle->GetSeatManager()->CountFreeSeats(false, true, pPed))
							{
								// Even if there are free non-driver seats, it's possible that if the driver seat
								// is free, we may pick it, which we don't want. To avoid this, we set RF_DontUseDriverSeat
								// here.
								VehicleEnterExitFlags vehicleFlags;
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WaitForLeader);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontUseDriverSeat);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontAbortForCombat);

								// So we must check if we can actually enter any seat before we abort and start this task since it will just quit
								// if there are no seats available and we get stuck in a limbo
								s32 iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, pLeadersVehicle, iSeatRequestType, iSeat, false, vehicleFlags);
								if (iTargetEntryPoint != -1)
								{
									float fTimeToWarp = 0.f;
									if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TeleportToLeaderVehicle))
									{
										fTimeToWarp = CTaskEnterVehicle::ms_Tunables.m_SecondsBeforeWarpToLeader;
										vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpAfterTime);
									}
									response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskEnterVehicle(pLeadersVehicle, iSeatRequestType, iSeat, vehicleFlags, fTimeToWarp));
								}
							}
						}
						else if(pLeadersVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
						{
							VehicleEnterExitFlags vehicleFlags;
							vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WaitForLeader);
							vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontAbortForCombat);
							float fTimeToWarp = 0.f;
							if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TeleportToLeaderVehicle))
							{
								fTimeToWarp = CTaskEnterVehicle::ms_Tunables.m_SecondsBeforeWarpToLeader;
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpAfterTime);
							}
							response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskEnterVehicle(pLeadersVehicle, SR_Specific, pLeadersVehicle->GetDriverSeat(), vehicleFlags, fTimeToWarp));
						}
					}
				}
			}
		}
	}

	return response.HasEventResponse();
}

//////////////////////////////////////////////////////////////////////////
// CEventLeaderExitedCarAsDriver
//////////////////////////////////////////////////////////////////////////

bool CEventLeaderExitedCarAsDriver::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CPedGroup* pPedGroup = pPed->GetPedsGroup();
	if(pPedGroup)
	{
		CPed* pLeader = pPedGroup->GetGroupMembership()->GetLeader();
		if(pLeader && pLeader != pPed)
		{
			// If the ped isn't following a scripted task
			if(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY) == NULL)
			{
				if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) &&
					!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_LEAVE_ANY_CAR) && 
					!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) &&
					!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_PERSUIT) &&
					!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontLeaveVehicleIfLeaderNotInVehicle))
				{
					VehicleEnterExitFlags iVehicleFlags;
					if(!pLeader->IsAPlayerPed() && pPed->GetPedType() == PEDTYPE_COP)
					{
						iVehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
					}

					s32 iDelayTime = (s32)(pPed->GetRandomSeed()/65.5350f);
					CTaskExitVehicle* pLeadersExitTask = static_cast<CTaskExitVehicle*>(pLeader->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
					if(pLeadersExitTask && (pLeadersExitTask->IsFlagSet(CVehicleEnterExitFlags::JumpOut) || pLeadersExitTask->IsFlagSet(CVehicleEnterExitFlags::InAirExit)) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableJumpingFromVehiclesAfterLeader) )
					{
						iVehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
						iDelayTime = 0;
					}
					
					// If this ped is a mission ped or allowed to teleport into the vehicle, also allow it to teleport out. [4/12/2013 mdawe]
					if (pPed->PopTypeIsMission() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TeleportToLeaderVehicle))
					{
						iVehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
					}
					
					iVehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WaitForLeader);

					if ( !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableJumpingFromVehiclesAfterLeader) )
					{
						iVehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
					}
					
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskLeaveAnyCar(iDelayTime, iVehicleFlags));
				}
				else if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE))
				{
					// Stop entering car task by starting a do nothing task
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskDoNothing(1));
				}
			}
		}
	}

	return response.HasEventResponse();
}

//////////////////////////////////////////////////////////////////////////
// CEventLeaderHolsteredWeapon
//////////////////////////////////////////////////////////////////////////

bool CEventLeaderHolsteredWeapon::CreateResponseTask(CPed* UNUSED_PARAM(pPed), CEventResponse& response) const
{
	response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskSwapWeapon(SWAP_HOLSTER));
	return true;
}

//////////////////////////////////////////////////////////////////////////
// CEventLeaderUnholsteredWeapon
//////////////////////////////////////////////////////////////////////////

bool CEventLeaderUnholsteredWeapon::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->GetWeaponManager()->EquipBestWeapon())
	{
		response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskSwapWeapon(SWAP_DRAW));
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CEventLeaderEnteredCover
//////////////////////////////////////////////////////////////////////////

bool CEventLeaderEnteredCover::AffectsPedCore(CPed* pPed) const
{
	const CTaskCover* pCoverTask = static_cast<const CTaskCover *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
	if(pCoverTask && pCoverTask->IsSeekingCover())
	{
		return false;
	}

	CPedGroup* pGroup = pPed->GetPedsGroup();
	if(pGroup)
	{
		CPed* pLeader = pGroup->GetGroupMembership()->GetLeader();
		if(pLeader)
		{
			if(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true))
			{
				s32 iCoverState = pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER);

				if(iCoverState == CTaskInCover::State_Idle)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CEventLeaderEnteredCover::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CPedGroup* pGroup = pPed->GetPedsGroup();
	if(pGroup && !pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableCover))
	{
		CPed* pLeader = pGroup->GetGroupMembership()->GetLeader();
		if(pLeader)
		{
			if(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true) &&
				!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) )
			{
				s32 iCoverState = pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER);

				if(iCoverState == CTaskInCover::State_Idle)
				{
					Vector3 vCoverDir;
					Vector3 vPedHeading(VEC3V_TO_VECTOR3(pLeader->GetTransform().GetB()));
					vPedHeading.z = 0.0f;
					vPedHeading.Normalize();
					//vPedHeading.RotateZ(bLeft ? -HALF_PI : HALF_PI);

					static const float fCoverDistAhead = 10.0f;

					const Vector3 vLeaderPosition = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
					Vector3 vFrom = vLeaderPosition + (vPedHeading * fCoverDistAhead);

					CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(vFrom));
					pCoverTask->SetSearchPosition(vLeaderPosition);
					pCoverTask->SetSearchFlags(CTaskCover::CF_SeekCover|CTaskCover::CF_DontMoveIfNoCoverFound|CTaskCover::CF_CoverSearchByPos);
					pCoverTask->SetMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
					pCoverTask->SetSearchType(CCover::CS_MUST_PROVIDE_COVER);
					response.SetEventResponse(CEventResponse::EventResponse, pCoverTask);
					return true;
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// CEventLeaderLeftCover
//////////////////////////////////////////////////////////////////////////

bool CEventLeaderLeftCover::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	if(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY) == NULL)
	{
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COVER))
		{
			return true;
		}
	}

	return false;
}
