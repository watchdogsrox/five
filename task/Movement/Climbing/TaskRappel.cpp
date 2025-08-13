// FILE :    TaskRappel.cpp
// PURPOSE : Controlling task for rappelling in different areas
// CREATED : 18-10-2011

// File header
#include "Task/Movement/Climbing/TaskRappel.h"

// Rage headers
#include "profile/profiler.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "animation/MovePed.h"
#include "control/replay/replay.h"
#include "control/replay/Misc/RopePacket.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "fwpheffects/ropemanager.h"
#include "game/dispatch/Orders.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/physics.h"
#include "system/controlmgr.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/Heli.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

RAGE_DEFINE_CHANNEL(rappel)

namespace AICombatStats
{
	EXT_PF_TIMER(CTaskHeliPassengerRappel);
}
using namespace AICombatStats;

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskHeliPassengerRappel, 0xaed01319)
CTaskHeliPassengerRappel::Tunables CTaskHeliPassengerRappel::sm_Tunables;

const fwMvBooleanId CTaskHeliPassengerRappel::ms_DetachClipFinishedId("DetachAnimFinished",0x622013D6);
const fwMvBooleanId CTaskHeliPassengerRappel::ms_JumpClipFinishedId("ExitSeatAnimFinished",0x7AA0BA73);
const fwMvBooleanId CTaskHeliPassengerRappel::ms_CriticalFrameId("CriticalFrame",0xFE8E637);
const fwMvBooleanId CTaskHeliPassengerRappel::ms_StartDescentId("StartDescent",0xEC0272F5);
const fwMvBooleanId CTaskHeliPassengerRappel::ms_CreateWeaponId("CreateWeapon",0x68C5D794);
const fwMvClipId CTaskHeliPassengerRappel::ms_ExitSeatClipId("ExitSeatClip",0x8A967A93);

const fwMvFlagId	CTaskHeliPassengerRappel::ms_LandId("Land",0xE355D038);
const fwMvFlagId	CTaskHeliPassengerRappel::ms_LandUnarmedId("LandUnarmed",0xA22117A2);


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskHeliPassengerRappel
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskHeliPassengerRappel::CTaskHeliPassengerRappel(float fMinRappelHeight, float fRopeLengthOverride)
: m_pRopeAttachment(NULL)
, m_iPedSeatIndex(-1)
, m_fMinRappelHeight(fMinRappelHeight)
, m_bPinRopeToHand(false)
, m_vHeliAttachOffset(VEC3_ZERO)
, m_fRopeLengthOverride(fRopeLengthOverride)
, m_startTimer(0)
, m_bMoveExitHeliCriticalFrame(false)
, m_bMoveJumpClipFinished(false)
, m_bMoveJumpCriticalFrame(false)
, m_bMoveJumpStartDescent(false)
, m_bMoveDeatchCreateWeapon(false)
, m_bMoveDetachClipFinished(false)
{
	SetInternalTaskType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL);
}

CTaskHeliPassengerRappel::~CTaskHeliPassengerRappel()
{}

void CTaskHeliPassengerRappel::CleanUp()
{
	PF_FUNC(CTaskHeliPassengerRappel);

	CPed* pPed = GetPed();
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);

	if (GetPed()->GetNetworkObject())
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Cleanup", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
	}

	// When we clean up we need to make sure we aren't attached to the rope anymore
	if(m_pRopeAttachment)
	{
		m_pRopeAttachment->markedForDetach = true;
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			ptxEffectRef rope = (ptxEffectRef)0;
			CVehicle* pMyVehicle = GetPed()->GetMyVehicle();
			if(pMyVehicle && pMyVehicle->InheritsFromHeli())
			{
				CHeli* pHeli = static_cast<CHeli*>(pMyVehicle);

				CHeli::eRopeID eSeatRopeID = pHeli->GetRopeIdFromSeat(IsSeatOnLeft(), m_iPedSeatIndex);

				if(pHeli->GetRope(eSeatRopeID))
				{
					rope = (ptxEffectRef)pHeli->GetRope(eSeatRopeID);
				}

				CReplayMgr::RecordPersistantFx<CPacketDetachRopeFromEntity>(
					CPacketDetachRopeFromEntity(true),
					CTrackedEventInfo<ptxEffectRef>(rope),
					NULL,
					false);
			}
		}
#endif
	}

	if(pPed->GetNoCollisionEntity() == pPed->GetMyVehicle())
	{
		pPed->SetNoCollision(NULL, 0);
	}

	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	if(pMyVehicle && pMyVehicle->InheritsFromHeli())
	{
		if(!pPed->GetIsInVehicle() && pPed->GetIsAttached() && pPed->GetAttachParent() == pMyVehicle)
		{
			DetachPedFromHeli(pPed);
		}

		CHeli::eRopeID eSeatRopeID = static_cast<CHeli*>(pMyVehicle)->GetRopeIdFromSeat(IsSeatOnLeft(), m_iPedSeatIndex);
		ropeInstance* pRope = static_cast<CHeli*>(pMyVehicle)->GetRope(eSeatRopeID);

		if(pRope)
		{
			pRope->UnpinAllVerts();
		}
	}
}

bool CTaskHeliPassengerRappel::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	// We need to make sure we're on the ground detached from the rope before allowing aborts
	const int iEventPriority = pEvent ? static_cast<const CEvent*>(pEvent)->GetEventPriority() : -1;
	if( GetState() <= State_DetachFromRope && iPriority != ABORT_PRIORITY_IMMEDIATE )
	{
		CPed* pPed = GetPed();
		if(iEventPriority < E_PRIORITY_DAMAGE || iEventPriority == E_PRIORITY_POTENTIAL_BLAST)
		{
			return false;
		}
		else if(!pPed->IsInjured() && iEventPriority == E_PRIORITY_DAMAGE && pPed->GetIsInVehicle())
		{
			return false;
		}
	}

	return true;
}

CTask::FSM_Return CTaskHeliPassengerRappel::ProcessPreFSM()
{
	PF_FUNC(CTaskHeliPassengerRappel);

	CPed* pPed = GetPed();
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();

	bool bShouldQuit = false;

	s32 iState = GetState();
	if(iState < State_Idle)
	{
		if (!(pPed->IsNetworkClone() && iState == State_Start)) // clones can wait in the start state to be placed in the vehicle
		{
			// We do require a vehicle while we are still supposed to be inside one
			if(!pVehicle || !pVehicle->InheritsFromHeli())
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, no heli", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				bShouldQuit = true;
			}
			else
			{
				m_iPedSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				if(!taskVerifyf(m_iPedSeatIndex > 1, "Ped must be in a rear passenger seat in order to rappel"))
				{
					rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, not passenger", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
					bShouldQuit = true;
				}
			}
		}
	}
	else
	{
		const CVehicle* pMyVehicle = pPed->GetMyVehicle();
		if( !pMyVehicle || !pMyVehicle->GetDriver() || pMyVehicle->GetDriver()->IsDead() || !pMyVehicle->IsInDriveableState() ||
			pMyVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_PRIMARY) )
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, driver dead / heli dead", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
			bShouldQuit = true;
		}

		if(iState == State_Idle)
		{
			fwEntity* pAttachEntity = pPed->GetAttachParent();
			if(pAttachEntity && pAttachEntity != pMyVehicle)
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : pAttachEntity != pMyVehicle", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				bShouldQuit = true;
			}
		}
	}

	// rappelling peds need to be synced at a high update level so that they exit the heli at roughly the same time on all machines
	if (pPed->GetNetworkObject() && !pPed->IsNetworkClone())
	{
		static_cast<CNetObjPed*>(pPed->GetNetworkObject())->ForceHighUpdateLevel();
	}

	// If we hit the water for whatever reason we should quit out.
	if(pPed->GetIsInWater())
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : ped in water", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		bShouldQuit = true;
	}
	else if(pVehicle)
	{
		// if we have a valid vehicle we are inside it needs to be one of the law enforcement helis
		CVehicleModelInfo* vmi = pVehicle->GetVehicleModelInfo();
		Assert(vmi);
		bool lawHeli = vmi->GetVehicleType() == VEHICLE_TYPE_HELI && (vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ALLOWS_RAPPEL) || (vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ALLOW_RAPPEL_AI_ONLY) && !pPed->IsAPlayerPed()));
		if (!taskVerifyf(lawHeli, "Ped must be in a helicopter that allows rappelling"))
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : !FLAG_ALLOWS_RAPPEL", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
			bShouldQuit = true;
		}		
	}


	if(bShouldQuit)
	{
		CEventSwitch2NM event(10000, rage_new CTaskNMHighFall(10000));
		if(pPed->GetPedIntelligence()->AddEvent(event) != NULL)
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : *DO HIGH FALL*", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
			return FSM_Quit;
		}
	}

	pPed->SetFallingHeight(pPed->GetTransform().GetPosition().GetZf());

	// Tell the ped to run the process physics task function
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksTimeSliced, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovementTimeSliced, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsRappelling, true );

	return FSM_Continue;
}

CTask::FSM_Return CTaskHeliPassengerRappel::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	PF_FUNC(CTaskHeliPassengerRappel);

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_CreateRope)
			FSM_OnEnter
				CreateRope_OnEnter();
			FSM_OnUpdate
				return CreateRope_OnUpdate();

		FSM_State(State_ExitHeli)
			FSM_OnEnter
				ExitHeli_OnEnter();
			FSM_OnUpdate
				return ExitHeli_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();
			FSM_OnExit
				Idle_OnExit();

		FSM_State(State_Jump)
			FSM_OnEnter
				Jump_OnEnter();
			FSM_OnUpdate
				return Jump_OnUpdate();
			FSM_OnExit
				Jump_OnExit();

		FSM_State(State_Descend)
			FSM_OnEnter
				Descend_OnEnter();
			FSM_OnUpdate
				return Descend_OnUpdate();

		FSM_State(State_DetachFromRope)
			FSM_OnEnter
				DetachFromRope_OnEnter();
			FSM_OnUpdate
				return DetachFromRope_OnUpdate();

		FSM_State(State_ExitOnGround)
			FSM_OnEnter
				ExitOnGround_OnEnter();
			FSM_OnUpdate
				return ExitOnGround_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskInfo *CTaskHeliPassengerRappel::CreateQueriableState() const
{
	return rage_new CClonedHeliPassengerRappelInfo(GetState());
}

void CTaskHeliPassengerRappel::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_HELI_PASSENGER_RAPPEL);

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskHeliPassengerRappel::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

bool CTaskHeliPassengerRappel::ProcessPhysics(float /*fTimeStep*/, int UNUSED_PARAM(nTimeSlice))
{
	PF_FUNC(CTaskHeliPassengerRappel);

	CPed* pPed = GetPed();
	if((GetState() == State_Jump || GetState() == State_Descend) && !pPed->GetIsAttached())
	{
		Vector3 vPedVelocity = pPed->GetVelocity();
		if(vPedVelocity.z > SMALL_FLOAT)
		{
			vPedVelocity.z = -SMALL_FLOAT;
			pPed->SetVelocity(vPedVelocity);
			pPed->SetDesiredVelocity(vPedVelocity);
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );
		}
	}

	return true;
}

bool CTaskHeliPassengerRappel::ProcessPostMovement()
{
	PF_FUNC(CTaskHeliPassengerRappel);

	CPed* pPed = GetPed();
	CVehicle* pMyVehicle = pPed->GetMyVehicle();

	ropeInstance* pRope = NULL;
	if(pMyVehicle)
	{
		CHeli::eRopeID eSeatRopeID = static_cast<CHeli*>(pMyVehicle)->GetRopeIdFromSeat(IsSeatOnLeft(), m_iPedSeatIndex);
		pRope = static_cast<CHeli*>(pMyVehicle)->GetRope(eSeatRopeID);
	}

	if(pRope)
	{
		pRope->UnpinAllVerts();
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordPersistantFx<CPacketUnPinRope>(
				CPacketUnPinRope(-1, pRope->GetUniqueID()),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
				NULL, 
				false);
		}
#endif

		// If we should be pinning the rope to our hand
		if(m_bPinRopeToHand)
		{
			if(IsUsingAccurateRopeMode())
			{
				// Get our hand positions. Going to determine lead hand by which one is higher.
				Matrix34 boneMatLeft;
				s32 iBoneLeftIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_L_FINGER32);
				if (iBoneLeftIndex == -1)
				{
					iBoneLeftIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND);
				}
				aiAssertf(iBoneLeftIndex != -1, "Could not find BONETAG_L_FINGER32 or BONETAG_L_PH_HAND for this rappelling ped.");
				pPed->GetGlobalMtx(iBoneLeftIndex, boneMatLeft);

				Matrix34 boneMatRight;
				s32 iBoneRightIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_FINGER32);
				if (iBoneRightIndex == -1)
				{
					iBoneRightIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);
				}
				aiAssertf(iBoneRightIndex != -1, "Could not find BONETAG_R_FINGER32 or BONETAG_R_PH_HAND for this rappelling ped.");
				pPed->GetGlobalMtx(iBoneRightIndex, boneMatRight);

				Matrix34 *pLeadHand = NULL;
				Matrix34 *pOtherHand = NULL;
				if( (boneMatRight.d.GetZ() > boneMatLeft.d.GetZ()))
				{
					pLeadHand = &boneMatRight;
					pOtherHand = &boneMatLeft;
				}
				else
				{
					pLeadHand = &boneMatLeft;
					pOtherHand = &boneMatRight;
				}

				bool bPin = false;
				if(pRope->GetVertsCount() > 0)
				{
					s32 iRopeVertexLead = 0;
					s32 iRopeVertexOther = -1;

					Vec3V vVertex = pRope->GetWorldPosition(0);
					float fDistanceFromStart = vVertex.GetZf() - pLeadHand->d.GetZ();
					if(fDistanceFromStart > 0.0f)
					{
						float fRatio = Clamp(fDistanceFromStart / pRope->GetMaxLength(), 0.0f, 1.0f);
						float fSegment = fRatio * (pRope->GetVertsCount()-1);
						iRopeVertexLead = (s32)fSegment;
					}

					iRopeVertexLead = Max(1,iRopeVertexLead);
					iRopeVertexOther = iRopeVertexLead + 1;
					iRopeVertexOther = Min(iRopeVertexOther, pRope->GetVertsCount());

					bPin = !m_pRopeAttachment || (iRopeVertexLead < 2) || GetState() == State_Idle || GetState() == State_ExitHeli;
					if(bPin)
					{
						pRope->Pin(iRopeVertexLead, VECTOR3_TO_VEC3V(pLeadHand->d));

						if( (iRopeVertexOther != -1) && (iRopeVertexOther > iRopeVertexLead) )
						{
							pRope->Pin(iRopeVertexOther, VECTOR3_TO_VEC3V(pOtherHand->d));
						}
					}
					else
					{
						Assert(pRope->GetEnvCloth() && pRope->GetEnvCloth()->GetCloth());
						Vec3V v = VECTOR3_TO_VEC3V(pLeadHand->d);

						phClothData& clothData = pRope->GetEnvCloth()->GetCloth()->GetClothData();

						// Find the cloth vertex to pin more accurately. Using estimate if off resulting in finding points lower than our hands.
						TUNE_GROUP_BOOL(RAPPEL_ROPE, bFindClothVertex, true);
						if(bFindClothVertex)
						{
							for(int i = 0; i < clothData.GetVertexCount() - 1; ++i)
							{
								Vec3V vTestPos = clothData.GetVertexPosition(i);

								if(v.GetZf() < vTestPos.GetZf())
								{
									iRopeVertexLead = Max(2, i);
								}
								else
								{
									break;
								}
							}
						}

						Vec3V v0 = clothData.GetVertexPosition(iRopeVertexLead);
						Vec3V v1 = clothData.GetVertexPosition(iRopeVertexLead - 1);
						ScalarV v0Z = v0.GetZ();
						ScalarV v1Z = v1.GetZ();
						v0 = v;
						v1 = v;
						v0.SetZ(v0Z);
						v1.SetZ(v1Z);

						TUNE_GROUP_BOOL(RAPPEL_ROPE, bExactLeadHandPlacement, true);
						TUNE_GROUP_BOOL(RAPPEL_ROPE, bExactOtherHandPlacement, true);
						
						if(((iRopeVertexLead+1) < pRope->GetVertsCount()))
						{
							if(bExactOtherHandPlacement)
							{
								clothData.SetVertexPosition(iRopeVertexLead+1, VECTOR3_TO_VEC3V(pOtherHand->d));
							}
							else
							{
								Vec3V vOther = clothData.GetVertexPosition(iRopeVertexLead + 1);
								ScalarV vOtherZ = vOther.GetZ();
								vOther = VECTOR3_TO_VEC3V(pOtherHand->d);
								vOther.SetZ(vOtherZ);
								clothData.SetVertexPosition(iRopeVertexLead+1, vOther);
							}
						}

						if(bExactLeadHandPlacement)
						{
							clothData.SetVertexPosition(iRopeVertexLead, VECTOR3_TO_VEC3V(pLeadHand->d));
						}
						else
						{
							clothData.SetVertexPosition(iRopeVertexLead, v0);
						}

						clothData.SetVertexPosition(iRopeVertexLead - 1, v1);
						if (iRopeVertexLead > 3)
						{
							Vec3V v2 = clothData.GetVertexPosition(iRopeVertexLead - 2);
							ScalarV v2Z = v2.GetZ();
							v2 = v;
							v2.SetZ(v2Z);
							clothData.SetVertexPosition(iRopeVertexLead - 2, v2);
						}
					}
				}

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{			
					CReplayMgr::RecordPersistantFx<CPacketRappelPinRope>(
						CPacketRappelPinRope(m_pRopeAttachment ? m_pRopeAttachment->vertexIndex : 0, pLeadHand->d, bPin, pRope->GetUniqueID()),
						CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
						NULL, 
						false);
				}
#endif
			}
			else
			{
				// Get our hand position
				Matrix34 boneMat;
				s32 iBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_FINGER32);
				if (iBoneIndex == -1)
				{
					iBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);
				}

				aiAssertf(iBoneIndex != -1, "Could not find BONETAG_R_FINGER32 or BONETAG_R_PH_HAND for this rappelling ped.");
				pPed->GetGlobalMtx(iBoneIndex, boneMat);
				bool bPin = !m_pRopeAttachment || m_pRopeAttachment->vertexIndex < 2 || GetState() == State_Idle || GetState() == State_ExitHeli;
				if (bPin)
				{
					pRope->Pin(1, VECTOR3_TO_VEC3V(boneMat.d));
				}
				else
				{
					Assert(pRope->GetEnvCloth() && pRope->GetEnvCloth()->GetCloth());
					Vec3V v = VECTOR3_TO_VEC3V(boneMat.d);

					phClothData& clothData = pRope->GetEnvCloth()->GetCloth()->GetClothData();

					Vec3V v0 = clothData.GetVertexPosition(m_pRopeAttachment->vertexIndex);
					Vec3V v1 = clothData.GetVertexPosition(m_pRopeAttachment->vertexIndex - 1);
					ScalarV v0Z = v0.GetZ();
					ScalarV v1Z = v1.GetZ();
					v0 = v;
					v1 = v;
					v0.SetZ(v0Z);
					v1.SetZ(v1Z);
					clothData.SetVertexPosition(m_pRopeAttachment->vertexIndex, v0);
					clothData.SetVertexPosition(m_pRopeAttachment->vertexIndex - 1, v1);
					if (m_pRopeAttachment->vertexIndex > 3)
					{
						Vec3V v2 = clothData.GetVertexPosition(m_pRopeAttachment->vertexIndex - 2);
						ScalarV v2Z = v2.GetZ();
						v2 = v;
						v2.SetZ(v2Z);
						clothData.SetVertexPosition(m_pRopeAttachment->vertexIndex - 2, v2);
					}
				}

	#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{			
					CReplayMgr::RecordPersistantFx<CPacketRappelPinRope>(
						CPacketRappelPinRope(m_pRopeAttachment ? m_pRopeAttachment->vertexIndex : 0, boneMat.d, bPin, pRope->GetUniqueID()),
						CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pRope),
						NULL, 
						false);
				}
	#endif
			}
		}
	}

	return false;
}

bool CTaskHeliPassengerRappel::ProcessMoveSignals()
{
	//Check the state.
	const int iState = GetState();

	switch(iState)
	{
	case State_ExitHeli:
		ExitHeli_OnProcessMoveSignals();
		break;
	case State_Jump:
		Jump_OnProcessMoveSignals();
		break;
	case State_DetachFromRope:
		DetachFromRope_OnProcessMoveSignals();
		break;
	default:
		break;
	}

	return true;
}

void CTaskHeliPassengerRappel::ExitHeli_OnProcessMoveSignals()
{
	CPed *pPed = GetPed();
	if (pPed)
	{
		CTask* pSubTask = GetSubTask();
		CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
		// If we don't have a rope attachment yet and are running an exit vehicle task
		if(pMyVehicle && pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
		{
			// Check if we are actually exiting the vehicle yet
			CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(pSubTask);
			if (pExitVehicleTask && pExitVehicleTask->GetMoveNetworkHelper() && pExitVehicleTask->GetMoveNetworkHelper()->IsNetworkActive())
			{
				m_bMoveExitHeliCriticalFrame |= pExitVehicleTask->GetMoveNetworkHelper()->GetBoolean(ms_CriticalFrameId);
			}
		}
	}
}

void CTaskHeliPassengerRappel::Jump_OnProcessMoveSignals()
{
	m_bMoveJumpClipFinished  |= m_moveNetworkHelper.GetBoolean(ms_JumpClipFinishedId);
	m_bMoveJumpCriticalFrame |= m_moveNetworkHelper.GetBoolean(ms_CriticalFrameId);
	m_bMoveJumpStartDescent  |= m_moveNetworkHelper.GetBoolean(ms_StartDescentId);
}

void CTaskHeliPassengerRappel::DetachFromRope_OnProcessMoveSignals()
{
	m_bMoveDeatchCreateWeapon |= m_moveNetworkHelper.GetBoolean(ms_CreateWeaponId);
	m_bMoveDetachClipFinished |= m_moveNetworkHelper.GetBoolean(ms_DetachClipFinishedId);
}

void CTaskHeliPassengerRappel::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

void CTaskHeliPassengerRappel::Start_OnEnter()
{
	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : START", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	// start the ped network syncing so that the rappel task is started a.s.a.p on other machines
	if (GetPed() && !GetPed()->IsNetworkClone() && GetPed()->GetNetworkObject())
	{
		GetPed()->GetNetworkObject()->SetLocalFlag(CNetObjGame::LOCALFLAG_FORCE_SYNCHRONISE, true);

		static_cast<CNetObjPed*>(GetPed()->GetNetworkObject())->SetRappellingPed();

	}
}

CTask::FSM_Return CTaskHeliPassengerRappel::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	CHeli* pHeli = static_cast<CHeli*>(pPed->GetVehiclePedInside());

	if(pPed->IsNetworkClone())
	{
		// if the ped does not have a vehicle at this stage, the task has progressed too far ahead on the main ped, just hide the ped in that case
		if (!pPed->GetIsInVehicle() && GetStateFromNetwork() != State_Finish)
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : State_Start : ped not in vehicle, so hidden", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
			NetworkInterface::LocallyHideEntityForAFrame(*pPed, true, "CTaskHeliPassengerRappel::Start_OnUpdate()");
			return FSM_Continue;
		}

		if(GetStateFromNetwork() != State_CreateRope &&
           GetStateFromNetwork() != State_ExitHeli)
		{
			if(GetStateFromNetwork() == State_Finish)
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : GetStateFromNetwork() == State_Finish", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				SetState(State_Finish);
			}

			return FSM_Continue;
		}
	}
	else if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AdditionalRappellingPed))
	{
		// Check the distance to the ground and make sure we are far enough away from it
		float fGroundToPedDiff = 0.0f;
		if(GetPedToGroundDiff(fGroundToPedDiff, false) && fGroundToPedDiff < m_fMinRappelHeight)
		{
			const float s_fMinTimeLandedForGroundExit = 1.0f;

			if(pHeli->GetHeliIntelligence()->IsLanded() && pHeli->GetHeliIntelligence()->GetTimeSpentLanded() > s_fMinTimeLandedForGroundExit)
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_ExitOnGround)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				SetState(State_ExitOnGround);
			}

			return FSM_Continue;
		}
	}

	// in MP the task needs to wait for a couple of seconds in the start state, to give the network code time to create the ped on other machines (generally swat
	// peds are created in the heli, then immediately start rappelling)
	if (NetworkInterface::IsGameInProgress() && GetPed()->GetNetworkObject() && !pPed->IsNetworkClone())
	{
		m_startTimer += fwTimer::GetTimeStepInMilliseconds();
	
		if (m_startTimer < 2000)
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : State_Start : waiting for a couple of seconds", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
			return FSM_Continue;			
		}
		else
		{
			// we must also wait for the heli and peds to be created on other machines, otherwise the task will get screwed on the clone if the heli arrives late
			if (NetworkInterface::IsEntityPendingCloning(pPed))
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : State_Start : waiting for ped to clone", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				return FSM_Continue;
			}

			if (pHeli && !pPed->PopTypeIsMission() && !NetworkInterface::AreEntitiesClonedOnTheSameMachines(pPed, pHeli))
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : State_Start : waiting for heli (0x%p (%s)) and ped to clone on same machines", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-", pHeli, pHeli->GetNetworkObject() ? pHeli->GetNetworkObject()->GetLogName() : "-none-");
				return FSM_Continue;
			}
		}
	}

	// Load in the proper clip set based on which seat we are in
	m_iPedSeatIndex = pHeli ? pHeli->GetSeatManager()->GetPedsSeatIndex(pPed) : -1;
	if(m_iPedSeatIndex != -1)
	{
		fwMvClipSetId iClipSet = GetRappelClipSet();
		if (!m_moveNetworkHelper.IsNetworkActive() && m_clipSetRequestHelper.Request(iClipSet))
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_CreateRope)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
			// Once the clips are finished streaming start exiting the heli
			SetState(State_CreateRope);
		}
	}
	else
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, ped not in seat", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::CreateRope_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
	m_iPedSeatIndex = pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : CREATE ROPE", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");

	float fMaxRopeLength = sm_Tunables.m_fDefaultRopeLength;
	if(GetPedToGroundDiff(fMaxRopeLength, true))
	{
		fMaxRopeLength = rage::Max(fMaxRopeLength, m_fRopeLengthOverride) + sm_Tunables.m_fExtraRopeLength;
	}

	// Create our rope then start unwinding it
	CHeli* pHeli = static_cast<CHeli*>(pMyVehicle);
	int nNumSections = IsUsingAccurateRopeMode() ? 64 : 32;
	ropeInstance* pNewRope = pHeli->CreateAndAttachRope(IsSeatOnLeft(), m_iPedSeatIndex, 1.0f, .5f, fMaxRopeLength, sm_Tunables.m_fRopeUnwindRate, 0, nNumSections, true);

	// start unwinding if we aren't full unwound
	if(pNewRope && pNewRope->GetLength() < pNewRope->GetMaxLength())
	{
		if(pNewRope->GetIsWindingFront())
		{
			pNewRope->StopWindingFront();
		}

		pNewRope->StartUnwindingFront(sm_Tunables.m_fRopeUnwindRate);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			ropeInstance* r = pNewRope;
			CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pNewRope),
				NULL, 
				false);
		}
#endif
	}
}

CTask::FSM_Return CTaskHeliPassengerRappel::CreateRope_OnUpdate()
{
    CPed*  pPed  = GetPed();
	CHeli* pHeli = static_cast<CHeli*>(pPed->GetVehiclePedInside());

	if (!pHeli)
	{
		return FSM_Quit;
	}

	if(pHeli->CanEntitiesAttachToRope(IsSeatOnLeft(), m_iPedSeatIndex))
	{
		s32 iDesiredState = State_ExitHeli;

		float fGroundToPedDiff = 0.0f;
		if(pHeli->GetHeliIntelligence()->IsLanded() || (GetPedToGroundDiff(fGroundToPedDiff, false) && fGroundToPedDiff < sm_Tunables.m_fMinHeightToRappel) )
		{
			iDesiredState = State_ExitOnGround;
		}
		else if(pHeli->GetDriver() && pHeli->GetDriver()->IsDead())
		{
			return FSM_Continue;
		}

		if (iDesiredState == State_ExitHeli)
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_ExitHeli)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		}
		else
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_ExitOnGround)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		}
		SetState(iDesiredState);
	}

	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::ExitHeli_OnEnter()
{
	CPed* pPed = GetPed();

	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : EXIT HELI", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");

    CVehicle* pMyVehicle = pPed->GetVehiclePedInside();

    // Start the exit vehicle task, which will handle playing the initial animation
    VehicleEnterExitFlags vehicleFlags;
    vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
    vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
    CTaskExitVehicle* pNewTask = rage_new CTaskExitVehicle(pMyVehicle, vehicleFlags);
    pNewTask->SetIsRappel(true);
	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetNewTask(CTaskExitVehicle)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
    SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskHeliPassengerRappel::ExitHeli_OnUpdate()
{
	CTask* pSubTask = GetSubTask();

    CPed* pPed = GetPed();

    // ensure clone peds aren't lagging too far behind
    if(pPed->IsNetworkClone() && GetStateFromNetwork() == State_Finish)
    {
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, GetStateFromNetwork() == State_Finish", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
        SetState(State_Finish);
    }

	CVehicle* pMyVehicle = pPed->GetVehiclePedInside();

	// If we don't have a rope attachment yet and are running an exit vehicle task
	if(pMyVehicle && pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
	{
		// Check if we are actually exiting the vehicle yet
		CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(pSubTask);
		if(pExitVehicleTask && pExitVehicleTask->IsExiting())
		{
			m_iPedSeatIndex = pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			
			// Pin a vert to our hand
			if(pExitVehicleTask->GetMoveNetworkHelper())
			{
				if(m_vHeliAttachOffset.IsZero())
				{
					const crClip *pClip = pExitVehicleTask->GetMoveNetworkHelper()->GetClip(ms_ExitSeatClipId);
					if(pClip)
					{
						const CModelSeatInfo* pModelSeatInfo = pMyVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
						int iBoneIndex = pModelSeatInfo ? pModelSeatInfo->GetBoneIndexFromSeat(m_iPedSeatIndex) : -1;
						if(iBoneIndex != -1)
						{
							Vector3 vAnimOffset = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.0f, 1.0f);

							// Get the seat position and orientation in object space (transform by car matrix to get world space pos)
							Matrix34 mSeatMat = pMyVehicle->GetObjectMtx(iBoneIndex);

							// Rotate by the local seat matrix to get the offset relative to the local seat matrix
							mSeatMat.Transform3x3(vAnimOffset);

							// Store the attach position
							m_vHeliAttachOffset = (mSeatMat.d + vAnimOffset);
						}
					}
				}

				// Need the rope in our hands earlier in accurate mode.
				if(IsUsingAccurateRopeMode() && (GetTimeInState() > 0.5f))
				{
					m_bPinRopeToHand = true;
				}

				// If we hit the critical frame then we should start pinning the rope to our hand
				if(m_bMoveExitHeliCriticalFrame)
				{
					rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : pExitVehicleTask->SetRunningFlag(CVehicleEnterExitFlags::IgnoreExitDetachOnCleanup)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
					m_bPinRopeToHand = true;
					pExitVehicleTask->SetRunningFlag(CVehicleEnterExitFlags::IgnoreExitDetachOnCleanup);
				}
			}
		}
	}

	// When the task is done we should have exited the heli and no longer be attached to it
	if(!pSubTask || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_Idle)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Idle);
	}

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::Idle_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pMyVehicle = pPed->GetMyVehicle();

	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : IDLE", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");

	if(pMyVehicle && pMyVehicle->InheritsFromHeli())
	{
		// Calculate the heli heading.
		Vec3V vForward = pMyVehicle->GetTransform().GetForward();
		float fHeliHeading = rage::Atan2f(-vForward.GetXf(), vForward.GetYf());

		// Calculate the heading.
		float fHeading = fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() - fHeliHeading);

		Vector3 vOffset = m_vHeliAttachOffset;

		// This is a fail safe to make sure the ped attaches somewhere relatively logical in case the anim offset was not found
		if(vOffset.IsZero())
		{
			static bank_float s_fOffsetZ = -0.35f;
			static bank_float s_fOffsetX = 1.25f;
			static bank_float s_fOffsetY = 0.9f;
			Vector3 vOffset(0.0f, s_fOffsetY, s_fOffsetZ);

			if(IsSeatOnLeft())
			{
				vOffset.x -= s_fOffsetX;
			}
			else
			{
				vOffset.x += s_fOffsetX;
			}
		}

		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Attach to vehicle", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");

		pPed->AttachPedToPhysical(pMyVehicle, -1, ATTACH_STATE_PED|ATTACH_FLAG_INITIAL_WARP|ATTACH_FLAG_AUTODETACH_ON_DEATH|ATTACH_FLAG_AUTODETACH_ON_RAGDOLL, &vOffset, NULL, fHeading, 0.0f); 
		pPed->SetNoCollision(pMyVehicle, NO_COLLISION_PERMENANT);
	}

	// Create our network player and start it on the ped, also set the clip set we want
	m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkHelicopterRappel);
	pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), INSTANT_BLEND_DURATION);

	// For now we are just using the same anim set either way before the descend anim isn't any different
	m_moveNetworkHelper.SetClipSet(GetRappelClipSet());
}

CTask::FSM_Return CTaskHeliPassengerRappel::Idle_OnUpdate()
{
	// Get our ped's order and verify that it's a swat passenger rappel order
	CPed* pPed = GetPed();
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetType() == COrder::OT_Swat)
	{
		const CSwatOrder* pSwatOrder = static_cast<const CSwatOrder*>(pOrder);

		// Check the distance between our ped and the desired location for the order, if close enough start the rappel state
		Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vPedPosition.z = pSwatOrder->GetStagingLocation().z;
		if(pSwatOrder->GetStagingLocation().Dist2(vPedPosition) < sm_Tunables.m_fStartDescendingDistToTargetSq)
		{
			CVehicle* pMyVehicle = pPed->GetMyVehicle();
			if(!pMyVehicle)
			{
				rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_Jump). No vehicle", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				SetState(State_Jump);
			}
			else
			{
				ScalarV scMaxHeliSpeedSq = LoadScalar32IntoScalarV(square(sm_Tunables.m_fMaxHeliSpeedForRappel));
				ScalarV scHeliSpeedSq = MagSquared(VECTOR3_TO_VEC3V(pMyVehicle->GetVelocity()));
				if(IsLessThanAll(scHeliSpeedSq, scMaxHeliSpeedSq))
				{
					rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_Jump). Close to target", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
					SetState(State_Jump);
				}
			}
		}
	}
	else
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_Jump)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Jump);
	}

	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::Idle_OnExit()
{
	if(GetState() != State_Jump)
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Idle_OnExit - detach ped from heli", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		DetachPedFromHeli(GetPed());
	}
}

void CTaskHeliPassengerRappel::Jump_OnEnter()
{
	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : JUMP", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	// Send the detach request to the move network and also detach from the rope right now
	static const fwMvRequestId detachId("JumpFromHeli",0x2668322);
	m_moveNetworkHelper.SendRequest(detachId);
}

CTask::FSM_Return CTaskHeliPassengerRappel::Jump_OnUpdate()
{
	// If the clip is finished then we are finished
	if (m_bMoveJumpClipFinished)
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_Descend)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Descend); 
	}

	CPed* pPed = GetPed();
	if (m_bMoveJumpCriticalFrame && pPed->GetIsAttached())
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : JumpOnUpdate - detach ped from heli", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		DetachPedFromHeli(pPed);

		CVehicle* pMyVehicle = pPed->GetMyVehicle();
		if(pMyVehicle && pMyVehicle->InheritsFromHeli())
		{
			// attach the ped to the rope
			m_pRopeAttachment = ((CHeli*)pMyVehicle)->AttachObjectToRope(IsSeatOnLeft(), m_iPedSeatIndex, pPed, 0.0f);
			if(m_pRopeAttachment)
			{
				m_pRopeAttachment->enableMovement = true;
			}
		}
	}

	if (m_bMoveJumpStartDescent && m_pRopeAttachment)
	{
		m_pRopeAttachment->constraintRate = sm_Tunables.m_fExitDescendRate;
	}

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::Jump_OnExit()
{
	CPed* pPed = GetPed();
	if(pPed->GetIsAttached())
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : JumpOnExit - detach ped from heli", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		DetachPedFromHeli(pPed);
	}

	if(pPed->GetNoCollisionEntity() == pPed->GetMyVehicle())
	{
		pPed->SetNoCollision(NULL, 0);
	}
}

void CTaskHeliPassengerRappel::Descend_OnEnter()
{
	if(m_pRopeAttachment)
	{
		m_pRopeAttachment->constraintRate = sm_Tunables.m_fDefaultDescendRate;
	}
}

CTask::FSM_Return CTaskHeliPassengerRappel::Descend_OnUpdate()
{
	CPed *pPed = GetPed();

	// Check if we are close enough to the ground so we can detach from the rope
	float fGroundToPedDiff = 0.0f;
	if( pPed->GetIsStanding() ||
		HasValidCollision() ||
		(GetPedToGroundDiff(fGroundToPedDiff, false) && fGroundToPedDiff < sm_Tunables.m_fMinHeightToRappel) )
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_DetachFromRope)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_DetachFromRope);
	}
	else if(!m_pRopeAttachment)
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, no rope attachment", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Finish);
	}
	else if(!m_pRopeAttachment->enableMovement || m_pRopeAttachment->distRatio >= 1.0f)
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, !m_pRopeAttachment->enableMovement || m_pRopeAttachment->distRatio >= 1.0f", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
		m_pRopeAttachment->markedForDetach = true;
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			ptxEffectRef rope = (ptxEffectRef)0;
			
			CVehicle* pMyVehicle = GetPed()->GetMyVehicle();
			if(pMyVehicle && pMyVehicle->InheritsFromHeli())
			{
				CHeli* pHeli = static_cast<CHeli*>(pMyVehicle);

				CHeli::eRopeID eSeatRopeID = pHeli->GetRopeIdFromSeat(IsSeatOnLeft(), m_iPedSeatIndex);

				if(pHeli->GetRope(eSeatRopeID))
				{
					rope = (ptxEffectRef)pHeli->GetRope(eSeatRopeID);
				}
	
				CReplayMgr::RecordPersistantFx<CPacketDetachRopeFromEntity>(
					CPacketDetachRopeFromEntity(true),
					CTrackedEventInfo<ptxEffectRef>(rope),
					NULL,
					false);
				}
		}
#endif
		SetState(State_Finish);
	}

    if(pPed->IsNetworkClone())
    {
        if(GetStateFromNetwork() == State_DetachFromRope ||
           GetStateFromNetwork() == State_Finish)
        {
            static const float WAIT_FOR_GROUND_TIME = 1.0f;

            if(GetTimeInState() >= WAIT_FOR_GROUND_TIME)
            {
				if (GetStateFromNetwork() == State_DetachFromRope)
				{
					rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetState(State_DetachFromRope)", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				}
				else
				{
					rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, GetStateFromNetwork() == State_Finish", pPed, pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "-none-");
				}
               SetState(GetStateFromNetwork());
            }
        }
    }

	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::DetachFromRope_OnEnter()
{
	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : DETACH FROM ROPE", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	// Send the detach request to the move network and also detach from the rope right now
	static const fwMvRequestId detachId("DetachFromRope",0x493E9918);
	m_moveNetworkHelper.SendRequest(detachId);
	
	//Set the landing flags
	m_moveNetworkHelper.SetFlag(true, ms_LandId);
	m_moveNetworkHelper.SetFlag(GetPed()->IsAPlayerPed(), ms_LandUnarmedId);

	audPedAudioEntity* pedAudioEntity = GetPed()->GetPedAudioEntity();

	if (pedAudioEntity)
	{
		pedAudioEntity->TriggerRappelLand();
	}

	if(m_pRopeAttachment)
	{
		m_pRopeAttachment->markedForDetach = true;
		m_pRopeAttachment = NULL;

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			ptxEffectRef rope = (ptxEffectRef)0;
			CVehicle* pMyVehicle = GetPed()->GetMyVehicle();
			if(pMyVehicle && pMyVehicle->InheritsFromHeli())
			{
				CHeli* pHeli = static_cast<CHeli*>(pMyVehicle);

				CHeli::eRopeID eSeatRopeID = pHeli->GetRopeIdFromSeat(IsSeatOnLeft(), m_iPedSeatIndex);

				if(pHeli->GetRope(eSeatRopeID))
				{
					rope = (ptxEffectRef)pHeli->GetRope(eSeatRopeID);
				}
					
				CReplayMgr::RecordPersistantFx<CPacketDetachRopeFromEntity>(
					CPacketDetachRopeFromEntity(true),
					CTrackedEventInfo<ptxEffectRef>(rope),
					NULL,
					false);
			}
		}
#endif
	}

	m_bPinRopeToHand = false;
}

CTask::FSM_Return CTaskHeliPassengerRappel::DetachFromRope_OnUpdate()
{
	CPed *pPed = GetPed(); 

	if(pPed->IsControlledByLocalOrNetworkAi() && m_bMoveDeatchCreateWeapon)
	{
		pPed->GetWeaponManager()->EquipBestWeapon(true);
	}

	if(IsUsingAccurateRopeMode())
	{
		// Enable Aggressive IK.
		pPed->SetPedResetFlag( CPED_RESET_FLAG_IsLanding, true );
		pPed->GetIkManager().SetOverrideLegIKBlendTime(0.1f);

		TUNE_GROUP_FLOAT(RAPPEL_ROPE, fDetachBlendOut, 0.1f, 0.0f, 5.0f, 0.01f);
		if(GetTimeInState() > fDetachBlendOut)
		{
			rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, detach interrupt", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
			SetState(State_Finish); 
		}
	}

	// If the clip is finished then we are finished, or we have been too long in this state and we need to quit
	if (m_bMoveDetachClipFinished || (GetStateFromNetwork() == State_Finish && GetTimeInState() > 2.0f))
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, detach clip finished", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Finish); 
	}

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::ExitOnGround_OnEnter()
{
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_DontEnterLeadersVehicle, true);

	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return;
	}

	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	SetNewTask(rage_new CTaskExitVehicle(pVehicle, vehicleFlags));

	rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : SetNewTask(CTaskExitVehicle)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
}

CTask::FSM_Return CTaskHeliPassengerRappel::ExitOnGround_OnUpdate()
{
	// ensure clone peds aren't lagging too far behind
	if(GetPed()->IsNetworkClone() && GetStateFromNetwork() == State_Finish)
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, GetStateFromNetwork() == State_Finish", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Finish);
	}

	if(GetIsSubtaskFinished(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		rappelDebugf3("[RAPPEL PASSENGER] : 0x%p (%s) : Quit, GetIsSubtaskFinished(CTaskTypes::TASK_EXIT_VEHICLE)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskHeliPassengerRappel::ActivateTimeslicing()
{
	CPed* pPed = GetPed();
	if(pPed)
	{
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}
}

// PURPOSE: To find the difference in height between our local ped and the ground at our x/y position
bool CTaskHeliPassengerRappel::GetPedToGroundDiff(float &fPedToGroundDiff, bool bUseStagingLocation)
{
	CPed *pPed = GetPed();
	Vector3 vTestPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(bUseStagingLocation)
	{
		const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if(pOrder && pOrder->GetType() == COrder::OT_Swat && pOrder->GetIsValid())
		{
			const CSwatOrder* pSwatOrder = static_cast<const CSwatOrder*>(pOrder);
			vTestPosition = pSwatOrder->GetStagingLocation();
			vTestPosition.z += sm_Tunables.m_fDefaultRopeLength;
		}
	}

	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vTestPosition, Vector3(vTestPosition.x, vTestPosition.y, -1000.0f));
	// not using GTA_PED_INCLUDE_TYPES as it was still colliding with our ped despite the exclusion, possibly due to multiple physInst?
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);

	// Since we are including vehicles and peds in our test make sure to exclude ourself and the vehicle we came out of
	const s32 nExclusions = 2;
	const CEntity* excludeEntities[nExclusions];
	excludeEntities[0] = pPed;
	excludeEntities[1] = pPed->GetMyVehicle();
	probeDesc.SetExcludeEntities(excludeEntities, nExclusions);

	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		fPedToGroundDiff = vTestPosition.z - probeResult[0].GetHitPosition().z;
		return true;
	}

	return false;
}

void CTaskHeliPassengerRappel::DetachPedFromHeli(CPed* pPed)
{
	pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS | DETACH_FLAG_EXCLUDE_VEHICLE | DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR | DETACH_FLAG_DONT_SNAP_MATRIX_UPRIGHT);
	pPed->SetHasJustLeftVehicle(50);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_PedExitedVehicleThisFrame, true);
}

bool CTaskHeliPassengerRappel::HasValidCollision()
{
	CPed* pPed = GetPed();

	CCollisionHistory* pCollisionHistory = pPed->GetFrameCollisionHistory();
	if(!pCollisionHistory)
	{
		return false;
	}

	if(!pCollisionHistory->IsHistoryComplete())
	{
		return false;
	}

	if( pCollisionHistory->GetFirstBuildingCollisionRecord() ||
		pCollisionHistory->GetFirstObjectCollisionRecord() ||
		pCollisionHistory->GetFirstOtherCollisionRecord() )
	{
		return true;
	}

	CVehicle* pMyHeli = pPed->GetMyVehicle();
	for(const CCollisionRecord* pCollisionRecord = pCollisionHistory->GetFirstVehicleCollisionRecord(); pCollisionRecord != NULL; pCollisionRecord = pCollisionRecord->GetNext())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pCollisionRecord->m_pRegdCollisionEntity.Get());
		if(!pVehicle)
		{
			continue;
		}

		if(pVehicle == pMyHeli)
		{
			continue;
		}

		return true;
	}

	return false;
}

fwMvClipSetId CTaskHeliPassengerRappel::GetRappelClipSet()
{
	// We may patch in some new animations later for frogger-style seats
	CPed* pPed = GetPed();
	CVehicle* pMyHeli = pPed->GetMyVehicle();

	return CTaskExitVehicle::SelectRappelClipsetForHeliSeat(pMyHeli, m_iPedSeatIndex);
}

bool CTaskHeliPassengerRappel::IsUsingAccurateRopeMode() const
{
	TUNE_GROUP_BOOL(RAPPEL_ROPE, bEnableAccurateMode, true);
	const CPed* pPed = GetPed();
	return pPed && pPed->IsPlayer() && bEnableAccurateMode;
}


#if !__FINAL
const char * CTaskHeliPassengerRappel::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_CreateRope:						return "State_CreateRope";
	case State_ExitHeli:						return "State_ExitHeli";
	case State_Idle:							return "State_Idle";
	case State_Jump:							return "State_Jump";
	case State_Descend:							return "State_Descend";
	case State_DetachFromRope:					return "State_DetachFromRope";
	case State_ExitOnGround:					return "State_ExitOnGround";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


//////////////////////////////////////////////////////////////////////////
// CClonedHeliPassengerRappelInfo
//////////////////////////////////////////////////////////////////////////

CClonedHeliPassengerRappelInfo::CClonedHeliPassengerRappelInfo(s32 state)
{
	SetStatusFromMainTaskState(state);
}

CTaskFSMClone *CClonedHeliPassengerRappelInfo::CreateCloneFSMTask()
{
	return rage_new CTaskHeliPassengerRappel(10.0f);
}

//////////////////////////////////////////////////////////////////////////
// CTaskRappel
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskRappel, 0x12937edf)

CTaskRappel::Tunables CTaskRappel::sm_Tunables;
const fwMvBooleanId CTaskRappel::sm_ClimbWallClipFinishedId("ClimbOverWallAnimFinished",0xD402FF06);
const fwMvBooleanId CTaskRappel::sm_AttachRopeId("Attach_Rope",0x8F69E085);
const fwMvBooleanId CTaskRappel::sm_JumpClipFinishedId("JumpAnimFinished",0x5F4B35B7);
const fwMvBooleanId CTaskRappel::sm_SmashWindowClipFinishedId("SmashWindowAnimFinished",0xE1618DDD);
const fwMvBooleanId CTaskRappel::sm_OnEnterIdleId("OnEnterIdle",0xF110481F);
const fwMvBooleanId CTaskRappel::sm_OnEnterDescendId("OnEnterDescend",0xDE0E0759);
const fwMvBooleanId CTaskRappel::sm_TurnOffExtractedZId("TurnOffExtractedZ",0x62DC9DBE);
const fwMvFloatId CTaskRappel::sm_JumpPhaseId("JumpPhase",0xE46C8DE3);
const fwMvFloatId CTaskRappel::sm_SmashWindowPhaseId("SmashWindowPhase",0x71C7DE3);
const fwMvClipSetId CTaskRappel::sm_BaseClipSetId("building_rappel",0xBEF9101);
const fwMvClipSetId CTaskRappel::sm_AdditionalClipSetId("building_rappel_mp",0x27DEF598);

const float CTaskRappel::sm_fDesiredHeadingForCasinoHeist = 0.0f;
const Vector3 CTaskRappel::sm_vCasinoHeistRappelPos(2500.f, -255.f, -65.f);

CTaskRappel::CTaskRappel(float fDestinationHeight, const Vector3& vStartPos, const Vector3& vAnchorPos, int ropeID, fwMvClipSetId clipSetID, bool bSkipClimbOverWallState, u16 uOwnerJumpID)
	: m_fDestinationHeight(fDestinationHeight)
	, m_fStartHeading(0.0f)
	, m_vStartPos(vStartPos)
	, m_vAnchorPos(vAnchorPos)
	, m_vEndAttachPos(V_ZERO)
	, m_vWindowPosition(V_ZERO)
	, m_bJumpedFromDescend(false)
	, m_bIsLongJumping(false)
	, m_bIsDescending(false)
	, m_iRopeID(ropeID)
	, m_iNextRopeVertex(1)
	, m_iEndAttachVertex(-1)
	, m_bHasBrokenGlass(false)
	, m_bShouldPinToEndVertex(false)
	, m_bShouldUpdateRope(false)
	, m_bCanBreakGlass(false)
	, m_OverrideBaseClipSetId(clipSetID)
	, m_bSetWallClimbToFinished(false)
	, m_bSkipClimbOverWallState(bSkipClimbOverWallState)
    , m_bRotatedForClimbSkip(false)
    , m_bAlreadyWaitingForTargetStateIdle(false)
	, m_uOwnerJumpID(uOwnerJumpID)
	, m_fDesiredRotation(0.0f)
	, m_uCloneJumpID(0)
{
	SetInternalTaskType(CTaskTypes::TASK_RAPPEL);
}

CTaskRappel::~CTaskRappel()
{}

void CTaskRappel::CleanUp()
{
	if (GetPed()->GetNetworkObject())
	{
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : Cleanup", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
	}

	// We need to go back to using gravity, clear our task network and turn collision back on
	CPed* pPed = GetPed();
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
	pPed->SetUseExtractedZ(false);
	pPed->EnableCollision();

	if(!m_bShouldPinToEndVertex)
	{
		// This will unpin the non-anchor verts from the rope if it still exists
		UnpinRopeVerts();
	}
}

CTask::FSM_Return CTaskRappel::ProcessPreFSM()
{
    CPed* pPed = GetPed();

	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	ropeInstance* pRopeInstance = (pRopeManager && m_iRopeID != -1) ? pRopeManager->FindRope(m_iRopeID) : NULL;
	if(!pRopeInstance)
	{
        if(!pPed->IsNetworkClone() || GetState() > State_WaitForNetworkRope)
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : Quit, no rope", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		    return FSM_Quit;
        }
	}

	// Tell the ped to run the process physics task function
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovement, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsRappelling, true );

	// Had some problems with the feet IKing to buildings/objects/ground
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_OFF);

	// Reset the variable controlling our descent
	m_bIsDescending = false;

	return FSM_Continue;
}

CTask::FSM_Return CTaskRappel::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnEnter
		Start_OnEnter();
	FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_ClimbOverWall)
		FSM_OnEnter
		ClimbOverWall_OnEnter();
		FSM_OnUpdate
		return ClimbOverWall_OnUpdate();

	FSM_State(State_Idle)
		FSM_OnEnter
		Idle_OnEnter();
	FSM_OnUpdate
		return Idle_OnUpdate();

	FSM_State(State_IdleAtDestHeight)
		FSM_OnEnter
		Idle_OnEnter();
	FSM_OnUpdate
		return Idle_OnUpdate();

	FSM_State(State_Descend)
		FSM_OnEnter
		Descend_OnEnter();
	FSM_OnUpdate
		return Descend_OnUpdate();

	FSM_State(State_Jump)
		FSM_OnEnter
		Jump_OnEnter();
	FSM_OnUpdate
		return Jump_OnUpdate();

	FSM_State(State_SmashWindow)
		FSM_OnEnter
		SmashWindow_OnEnter();
	FSM_OnUpdate
		return SmashWindow_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return Finish_OnUpdate();

    // unhandled states
    FSM_State(State_WaitForNetworkRope)

	FSM_End
}

CTaskInfo *CTaskRappel::CreateQueriableState() const
{
	return rage_new CClonedRappelDownWallInfo(GetState(), m_fDestinationHeight, m_fStartHeading, m_vStartPos, m_vAnchorPos, m_iRopeID, m_OverrideBaseClipSetId.GetHash(), m_bSkipClimbOverWallState, m_uOwnerJumpID);
}

void CTaskRappel::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
    taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_RAPPEL_DOWN_WALL);

    CClonedRappelDownWallInfo *pRappelInfo = SafeCast(CClonedRappelDownWallInfo, pTaskInfo);

    m_fDestinationHeight = pRappelInfo->GetDestinationHeight();
    m_fStartHeading      = pRappelInfo->GetStartHeading();
    m_vStartPos          = pRappelInfo->GetStartPos();
    m_vAnchorPos         = pRappelInfo->GetAnchorPos();
    m_OverrideBaseClipSetId.SetHash(pRappelInfo->GetOverrideBaseClipSetIdHash());
	m_bSkipClimbOverWallState = pRappelInfo->GetSkipClimbOverWallState();
	m_uOwnerJumpID = pRappelInfo->GetOwnerJumpID();

    int localRopeIndex   = NetworkInterface::GetLocalRopeIDFromNetworkID(pRappelInfo->GetNetworkRopeID());

    m_iRopeID            = localRopeIndex;

    CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskRappel::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
    FSM_Begin

		FSM_State(State_Start)
		FSM_OnEnter
		Start_OnEnter();
	FSM_OnUpdate
		return Start_OnUpdate();

    FSM_State(State_ClimbOverWall)
		FSM_OnEnter
		ClimbOverWall_OnEnter();
		FSM_OnUpdate
		return ClimbOverWall_OnUpdate();

	FSM_State(State_WaitForNetworkRope)
        FSM_OnEnter
        WaitForNetworkRope_OnEnter();
		FSM_OnUpdate
		return WaitForNetworkRope_OnUpdate();

	FSM_State(State_Idle)
		FSM_OnEnter
		Idle_OnEnter();
	FSM_OnUpdate
		return Idle_OnUpdateClone();

	FSM_State(State_IdleAtDestHeight)
		FSM_OnEnter
		Idle_OnEnter();
	FSM_OnUpdate
		return Idle_OnUpdateClone();

	FSM_State(State_Descend)
		FSM_OnEnter
		Descend_OnEnter();
	FSM_OnUpdate
		return Descend_OnUpdate();

	FSM_State(State_Jump)
		FSM_OnEnter
		Jump_OnEnter();
	FSM_OnUpdate
		return Jump_OnUpdate();

	FSM_State(State_SmashWindow)
		FSM_OnEnter
		SmashWindow_OnEnter();
	FSM_OnUpdate
		return SmashWindow_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return Finish_OnUpdate();

	FSM_End
}

void CTaskRappel::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Finish);
}

bool CTaskRappel::ProcessPhysics(float /*fTimeStep*/, int UNUSED_PARAM(nTimeSlice))
{
	if(GetState() == State_Jump)
	{
		Vector3 vDesiredVel(VEC3_ZERO);
		if(m_bIsDescending)
		{
			vDesiredVel.z = m_bIsLongJumping ? -sm_Tunables.m_fLongJumpDescendRate : -sm_Tunables.m_fJumpDescendRate;
		}

		GetPed()->SetDesiredVelocity(vDesiredVel);
	}

	return true;
}

bool CTaskRappel::ProcessPostMovement()
{
	if(m_bShouldUpdateRope)
	{
		UpdateRope();
	}

	return false;
}

void CTaskRappel::Start_OnEnter()
{
	rappelDebugf3("[RAPPEL] : 0x%p (%s) : START", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	// disable gravity and collision
	m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskRappel);

    CPed *pPed = GetPed();
	pPed->SetUseExtractedZ(true);
	pPed->DisableCollision();

    if(!pPed->IsNetworkClone())
    {
        m_vStartPos     = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
        m_fStartHeading = pPed->GetCurrentHeading();
    }

	// Request our clip sets that we will be using
	m_ClipSetRequestHelper.AddClipSetRequest(GetBaseClipSet());
	m_ClipSetRequestHelper.AddClipSetRequest(sm_AdditionalClipSetId);
}

fwMvClipSetId CTaskRappel::GetBaseClipSet() const
{
	if (m_OverrideBaseClipSetId != CLIP_SET_ID_INVALID)
	{
		return m_OverrideBaseClipSetId;
	}

	return sm_BaseClipSetId;
}

CTask::FSM_Return CTaskRappel::Start_OnUpdate()
{
	// We can't proceed until the clip sets are loaded and the network def is loaded
	if (m_ClipSetRequestHelper.RequestAllClipSets() && m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskRappel))
	{
		CPed* pPed = GetPed();

        // ensure the network clone player is at the correct start point, otherwise the anims won't look correct
        if(pPed->IsNetworkClone())
        {
			Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

            if((pedPos - m_vStartPos).Mag2() > rage::square(0.05f))
            {
                if(pPed->GetUsingRagdoll())
                {
                    pPed->SwitchToAnimated();
                }

                pPed->SetPosition(m_vStartPos, true, true, true);
            }

            pPed->SetHeading(m_fStartHeading);
        }

		// Create our network player and start it on the ped, also set the clip set we want
		m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskRappel);
		pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), INSTANT_BLEND_DURATION);

		// We have to set our clip set and also the variable clip set as all our anims are not in the same clip set
		m_moveNetworkHelper.SetClipSet(GetBaseClipSet());

		const fwMvClipSetVarId additionalAnimsClipSet("AdditionalAnims",0x4411658A);
		m_moveNetworkHelper.SetClipSet(sm_AdditionalClipSetId, additionalAnimsClipSet);

		rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_ClimbOverWall)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		SetState(State_ClimbOverWall);
	}

	return FSM_Continue;
}

void CTaskRappel::ClimbOverWall_OnEnter()
{
	if (m_bSkipClimbOverWallState)
	{
		m_moveNetworkHelper.SetBoolean(sm_ClimbWallClipFinishedId, true);
		m_bSetWallClimbToFinished = true;
	}
}

CTask::FSM_Return CTaskRappel::ClimbOverWall_OnUpdate()
{
	rappelDebugf3("[RAPPEL] : 0x%p (%s) : CLIMB OVER WALL", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	// If the clip is finished then we should move on to the idle state
	if (m_moveNetworkHelper.GetBoolean(sm_ClimbWallClipFinishedId) || m_bSetWallClimbToFinished)
	{
		// Just set this to true in case we didn't find any attach rope event
		m_bShouldUpdateRope = true;

        CPed* pPed = GetPed();
        if(pPed->IsNetworkClone())
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_WaitForNetworkRope)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
            SetState(State_WaitForNetworkRope);
        }
        else
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Idle)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		    SetState(State_Idle); 
        }
	}

	// Once we find the attach_rope event we can start updating the rope
	if (m_moveNetworkHelper.GetBoolean(sm_AttachRopeId))
	{
		m_bShouldUpdateRope = true;
	}

	return FSM_Continue;
}

void CTaskRappel::WaitForNetworkRope_OnEnter()
{
    m_moveNetworkHelper.WaitForTargetState(sm_OnEnterIdleId); // transitioning to idle already - we don't want to miss the event
    m_bAlreadyWaitingForTargetStateIdle = true;
}

CTask::FSM_Return CTaskRappel::WaitForNetworkRope_OnUpdate()
{
    CPed* pPed = GetPed();
    taskAssertf(pPed->IsNetworkClone(), "A local ped is in the waiting for network rope state!");

	rappelDebugf3("[RAPPEL] : 0x%p (%s) : WAIT FOR NETWORK ROPE", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	if(GetStateFromNetwork() == State_Finish)
    {
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : Quit, GetStateFromNetwork() == State_Finish", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
       SetState(State_Finish);
    }
    else
    {
        if(m_iRopeID == -1)
        {
            if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_RAPPEL))
            {
                CTaskInfo *pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_RAPPEL, PED_TASK_PRIORITY_MAX);

                if(pTaskInfo)
                {
                    ReadQueriableState(SafeCast(CClonedFSMTaskInfo, pTaskInfo));
                }
            }
        }

        if(m_iRopeID != -1)
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Idle)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
            SetState(State_Idle);
        }
    }

    return FSM_Continue;
}

void CTaskRappel::Idle_OnEnter()
{
	rappelDebugf3("[RAPPEL] : 0x%p (%s) : IDLE", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

    if (!m_bAlreadyWaitingForTargetStateIdle)
    {
        // Request the idle state and make sure we wait for it to be entered
        static const fwMvRequestId idleId("ReturnToIdle", 0x185D3234);
        m_moveNetworkHelper.SendRequest(idleId);
        m_moveNetworkHelper.WaitForTargetState(sm_OnEnterIdleId);
    }
	m_bAlreadyWaitingForTargetStateIdle = false;

	//Set heading if we skip the climb over wall
	if (m_bSkipClimbOverWallState && !m_bRotatedForClimbSkip)
	{
		CPed* pPed = GetPed();
		if(pPed)
		{
			float fDist = DistSquared(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(sm_vCasinoHeistRappelPos)).Getf();
			TUNE_GROUP_FLOAT(TASKRAPPEL, fAllowedRangForCasinoRappel, 10.f, 0.0f, 1000.f, 0.1f);
			if(fDist < fAllowedRangForCasinoRappel)
			{
				m_fDesiredRotation = sm_fDesiredHeadingForCasinoHeist;
			}

			ChangeRappelHeading(pPed);
			m_bRotatedForClimbSkip = true;
		}
	}
}

CTask::FSM_Return CTaskRappel::Idle_OnUpdate()
{
	if(!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	CPed* pPed = GetPed();

	if(pPed && !IsPedRappelHeadingCorrect(pPed))
	{
		ChangeRappelHeading(pPed);
	}

	const CControl* pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		// If we are above our destination z then we can either descend or jump
		bool bIsAtDesiredHeight = pPed->GetTransform().GetPosition().GetZf() < m_fDestinationHeight;
		if(!bIsAtDesiredHeight)
		{
			// if our stick is pressed enough we can descend
			if(-pControl->GetPedWalkUpDown().GetNorm() <= sm_Tunables.m_fMinStickValueAllowDescend)
			{
				rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Descend)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
				SetState(State_Descend);
				return FSM_Continue;
			}
		}
		
		// If either jump button is pressed then we check if we are smashing a window or just jumping
		if(pControl->GetPedRappelJump().IsDown() || pControl->GetPedRappelLongJump().IsDown())
		{
			if(bIsAtDesiredHeight && pControl->GetPedRappelSmashWindow().IsDown())
			{
				rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_SmashWindow)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
				SetState(State_SmashWindow);
			}
			else
			{
				rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Jump)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
				// Make sure to track if it's a long jump
				m_bIsLongJumping = pControl->GetPedRappelLongJump().IsDown();
				SetState(State_Jump);
			}
		}
	}
	else if(!pPed->IsAPlayerPed())
	{
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Descend)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		// AI go straight to descend
		SetState(State_Descend);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskRappel::Idle_OnUpdateClone()
{
	if(!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

    if(GetStateFromNetwork() == State_Finish)
    {
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : Quit, SetState(State_Finish)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
        SetState(State_Finish);
    }
    else
    {
        CPed* pPed = GetPed();

		if(pPed && !IsPedRappelHeadingCorrect(pPed))
		{
			ChangeRappelHeading(pPed);
		}

	    // If we are above our destination z then we can either descend or jump
	    const Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	    if(vPedPosition.GetZf() > m_fDestinationHeight)
	    {
			if (GetStateFromNetwork() == State_Jump && (m_uCloneJumpID != m_uOwnerJumpID))
			{
				rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Jump)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
				SetState(State_Jump);
			}
		    else
		    {
                const Vector3 vLastPosFromNetwork = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
                const Vector3 vPedPosition        = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

                const float fZDifference = vPedPosition.z - vLastPosFromNetwork.z;

                if(fZDifference > 0.0f)
                {
					rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Descend)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
			        SetState(State_Descend);
                }
		    }
	    }
        else if(GetStateFromNetwork() == State_SmashWindow)
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_SmashWindow)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
           SetState(State_SmashWindow);
        }
    }

	return FSM_Continue;
}

void CTaskRappel::Descend_OnEnter()
{
	rappelDebugf3("[RAPPEL] : 0x%p (%s) : DESCEND", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	// Request our walk state from the move network and make sure to wait til we are in it
	static const fwMvRequestId walkId("Walk",0x83504C9C);
	m_moveNetworkHelper.SendRequest(walkId);
	m_moveNetworkHelper.WaitForTargetState(sm_OnEnterDescendId);
}

CTask::FSM_Return CTaskRappel::Descend_OnUpdate()
{
	if(!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	// If we are descending and reach our z limit players should go to idle and AI go to smash window
	CPed* pPed = GetPed();
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	if(vPedPosition.GetZf() <= m_fDestinationHeight)
	{
        if(pPed->IsAPlayerPed() || pPed->IsNetworkClone())
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_IdleAtDestHeight)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
            SetState(State_IdleAtDestHeight);
        }
        else
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_SmashWindow)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		    SetState(State_SmashWindow);
        }

		return FSM_Continue;
	}

    bool bStopDescend = false;
    bool bShouldJump  = false;

	const CControl* pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		// Player peds have to keep pressing the stick to descend
		if(-pControl->GetPedWalkUpDown().GetNorm() > sm_Tunables.m_fMinStickValueAllowDescend)
		{
			bStopDescend = true;
		}
		else if(pControl->GetPedRappelJump().IsDown() || pControl->GetPedRappelLongJump().IsDown())
		{
			m_bIsLongJumping = pControl->GetPedRappelLongJump().IsDown();
			bShouldJump = true;
		}
	}
    else if(pPed->IsNetworkClone())
    {
        if(GetStateFromNetwork() == State_Finish)
        {
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : Quit, GetStateFromNetwork() == State_Finish", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
            SetState(State_Finish);
        }
        else if(GetStateFromNetwork() == State_Jump)
        {
          bShouldJump = true;
        }
        else
        {
            const Vector3 vLastPosFromNetwork = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
            const Vector3 vPedPosition        = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

            const float fZDifference = vPedPosition.z - vLastPosFromNetwork.z;

            if(fZDifference <= 0.0f)
            {
                bStopDescend = true;
            }
        }
    }

    if(bStopDescend)
    {
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Idle)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
        SetState(State_Idle);
        return FSM_Continue;
    }
    else if(bShouldJump)
    {
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Jump)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		// If the player ped jumps from descend we need to make sure jump knows this
		m_bJumpedFromDescend = true;
		SetState(State_Jump);
		return FSM_Continue;
    }

	return FSM_Continue;
}

void CTaskRappel::Jump_OnEnter()
{
	rappelDebugf3("[RAPPEL] : 0x%p (%s) : JUMP", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	static const fwMvRequestId jumpId("Jump",0x2E90C5D5);
	m_moveNetworkHelper.SendRequest(jumpId);

	//! Inc owner jump id. 
	m_uOwnerJumpID++;

	//! Clones set id to be the same as owner when they jump. And we can now prevent the ped from re-jumping until we see the owner id change.
	if (GetPed()->IsNetworkClone())
	{
		m_uCloneJumpID = m_uOwnerJumpID;
	}
}

CTask::FSM_Return CTaskRappel::Jump_OnUpdate()
{
	CPed* pPed = GetPed();
	const Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	// If the clip is finished then we are finished
	if (m_moveNetworkHelper.GetBoolean(sm_JumpClipFinishedId))
	{
		m_moveNetworkHelper.SetBoolean(sm_JumpClipFinishedId, false);
		m_bJumpedFromDescend = false;
		m_bIsLongJumping = false;

		if(vPedPosition.GetZf() > m_fDestinationHeight)
		{
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_Idle)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
			SetState(State_Idle);
		}
		else
		{
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_IdleAtDestHeight)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
			SetState(State_IdleAtDestHeight);
		}

		return FSM_Continue;
	}

	const CControl* pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		// Get the jump phase (needed for checking if we can descend or smash windows)
		const float fJumpPhase = m_moveNetworkHelper.GetFloat(sm_JumpPhaseId);

		// if the player ped is still above the z position
		if(vPedPosition.GetZf() > m_fDestinationHeight)
		{
			// to descend we either have to be pressing down or have jumped from descend plus be within the proper anim phase
			if((m_bJumpedFromDescend || -pControl->GetPedWalkUpDown().GetNorm() <= sm_Tunables.m_fMinStickValueAllowDescend) && 
				fJumpPhase < sm_Tunables.m_fMaxJumpPhaseAllowDescend && fJumpPhase > sm_Tunables.m_fMinJumpPhaseAllowDescend)
			{
				// mark us as descending so process physics can handle the movement
				m_bIsDescending = true;
			}
		}
		else if(sm_Tunables.m_bAllowSmashDuringJump && pControl->GetPedRappelSmashWindow().IsDown() && fJumpPhase >= sm_Tunables.m_fMinJumpPhaseAllowSmashWindow && 
				fJumpPhase <= sm_Tunables.m_fMaxJumpPhaseAllowSmashWindow)
		{
			m_moveNetworkHelper.SetFloat(sm_SmashWindowPhaseId, fJumpPhase * sm_Tunables.m_fJumpToSmashWindowPhaseChange);
			rappelDebugf3("[RAPPEL] : 0x%p (%s) : SetState(State_SmashWindow)", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
			SetState(State_SmashWindow);
		}
	}
    else if(pPed->IsNetworkClone())
    {
        const Vector3 vLastPosFromNetwork = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);

        const float fZDifference = vPedPosition.GetZf() - vLastPosFromNetwork.z;

        if(fZDifference > 0.0f)
        {
            // mark us as descending so process physics can handle the movement
            m_bIsDescending = true;
        }
    }

	return FSM_Continue;
}

void CTaskRappel::SmashWindow_OnEnter()
{
	rappelDebugf3("[RAPPEL] : 0x%p (%s) : SMASH WINDOW", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");

	static const fwMvRequestId smashWindowId("SmashWindow",0xAF37B3B4);
	m_moveNetworkHelper.SendRequest(smashWindowId);

	// Get the rope vertex and position that we'll attach to when the task ends
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = (m_iRopeID != -1) ? pRopeManager->FindRope(m_iRopeID) : 0;
		if(pRope && pRope->GetVertsCount()) 
		{
			m_iEndAttachVertex = rage::Max(m_iNextRopeVertex - 2, 0);
			m_vEndAttachPos = pRope->GetWorldPosition(m_iEndAttachVertex);
		}
	}
}

CTask::FSM_Return CTaskRappel::SmashWindow_OnUpdate()
{
	// If the clip is finished then we are finished
	if (m_moveNetworkHelper.GetBoolean(sm_SmashWindowClipFinishedId))
	{
		rappelDebugf3("[RAPPEL] : 0x%p (%s) : Quit, clip finished", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-");
		m_bShouldPinToEndVertex = true;
		SetState(State_Finish); 
	}

	CPed* pPed = GetPed();

	// If the anim has an event for turning off the use of extracted z, then do so.
	if (m_moveNetworkHelper.GetBoolean(sm_TurnOffExtractedZId))
	{
		pPed->SetUseExtractedZ(false);
	}

	if(!m_bHasBrokenGlass)
	{
		if(IsZeroAll(m_vWindowPosition) == 0)
		{
			Matrix34 boneMat;
			pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_L_FOOT), boneMat);
			boneMat.d.z = 0.0f;

			ScalarV scDistSq = DistSquared(VECTOR3_TO_VEC3V(boneMat.d), m_vWindowPosition);
			ScalarV scMinDistSq = ScalarVFromF32(square(sm_Tunables.m_fMinDistanceToBreakWindow));

			// If we can't yet break the glass then check if we moved far enough away to allow breaking
			if(!m_bCanBreakGlass)
			{
				m_bCanBreakGlass = (IsGreaterThanOrEqualAll(scDistSq, scMinDistSq) != 0);
			}
			// Once we can break the glass we make sure we are not close enough again to trigger the effect
			else if(IsLessThanAll(scDistSq, scMinDistSq))
			{
				FindAndBreakGlass(true);
				if(m_bHasBrokenGlass)
				{
					pPed->EnableCollision();
				}
			}
		}
		else
		{
			FindAndBreakGlass(false);
		}
	}

	if(!pPed->IsNetworkClone())
	{
		pPed->m_PedResetFlags.SetEntityZFromGround(40);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskRappel::Finish_OnUpdate()
{
	if(m_bShouldPinToEndVertex)
	{
		// If we ended the task properly then we should make sure the rope verts are properly pinned
		UpdateRope();
	}

	return FSM_Quit;
}

void CTaskRappel::FindAndBreakGlass(bool bBreakGlass)
{
	// Get our ped position, ped normal and create an end position
	CPed *pPed = GetPed();
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vector3 vPedNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
	Vector3 vEndPosition = (vPedNormal * 3.0f) + VEC3V_TO_VECTOR3(vPedPosition);

	// Set the start and end position, restrict to glass intersections
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(vPedPosition), vEndPosition);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_GLASS_TYPE);

	// Submit our probe
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// Get the entity from the phInst of the intersection and make sure it's dynamic and breakable glass
		CEntity* pEntity = CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());
		if(pEntity && pEntity->GetIsDynamic())
		{
			CDynamicEntity *pGlassObj = (CDynamicEntity*)pEntity;
			if(pGlassObj->m_nDEflags.bIsBreakableGlass)
			{
				if(!bBreakGlass)
				{
					m_vWindowPosition = probeResult[0].GetHitPositionV();
					m_vWindowPosition.SetZ(ScalarV(V_ZERO));
				}
				else
				{
					// Attempt to break the glass that we found
					m_bHasBrokenGlass = CPhysics::BreakGlass(pGlassObj, probeResult[0].GetHitPositionV(), sm_Tunables.m_fGlassBreakRadius, VECTOR3_TO_VEC3V(vPedNormal), sm_Tunables.m_fGlassDamage, 0);

					// Send a shocking event.
					if (m_bHasBrokenGlass && NetworkInterface::IsGameInProgress())
					{
						CEventShockingBrokenGlass event(*pPed, probeResult[0].GetHitPositionV());
						CShockingEventsManager::Add(event);
					}
				}
			}
		}
	}
}

bool CTaskRappel::IsPedRappelHeadingCorrect(CPed* pPed)
{
	const float fHeadingDelta = SubtractAngleShorter(m_fDesiredRotation , pPed->GetCurrentHeading()); 
	if(m_bSkipClimbOverWallState && fHeadingDelta != 0.0f)
	{
		return false;
	}

	return true;
}

void CTaskRappel::ChangeRappelHeading(CPed* pPed)
{
#if !__FINAL
	float oldHeading = pPed->GetCurrentHeading();
#endif

	pPed->SetHeading(fwAngle::LimitRadianAngle(m_fDesiredRotation));

#if __ASSERT
	const float fHeadingDelta = SubtractAngleShorter(pPed->GetCurrentHeading() , sm_fDesiredHeadingForCasinoHeist); 
	taskAssertf((fHeadingDelta < 0.2f) && (fHeadingDelta > -0.2f), "New desiered heading is not facing the correct direction for casino heist. Heading delta = %.2f, for ped: %s", fHeadingDelta, pPed->GetDebugName());
#endif

#if !__FINAL
	rappelDebugf3("[RAPPEL] : Heading changed for ped, 0x%p (%s), from: %f to: %f", GetPed(), GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "-none-", oldHeading, m_fDesiredRotation);
#endif
}

// Unpin all the rope verts except the anchor point
void CTaskRappel::UnpinRopeVerts()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
		ropeInstance* pRope = (m_iRopeID != -1) ? pRopeManager->FindRope(m_iRopeID) : 0;
		if(pRope) 
		{
			for(int i = 1; i < pRope->GetVertsCount(); i++)
			{
				pRope->UnPin(i);
			}
		}
	}
}

bool CTaskRappel::UpdateRope()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if(pRopeManager)
	{
        ropeInstance* pRope = (m_iRopeID != -1) ? pRopeManager->FindRope(m_iRopeID) : 0;
		if(pRope) 
		{
			CPed* pPed = GetPed();
			float fRopeSegmentLength = (pRope->GetLength() / (pRope->GetVertsCount() - 1));

			// TODO: base the "interpolation" position on distance from the base rather than an arbitrary value used w/ the time step
			float fRopeVertexInterpSpeed = fwTimer::GetTimeStep() * ((fRopeSegmentLength * -0.741f) + 3.982f);
			if(fRopeVertexInterpSpeed < 0)
			{
				fRopeVertexInterpSpeed *= -1;
			}

			// Get our hand position
			Matrix34 boneMat;
			pPed->GetGlobalMtx(GetPed()->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND), boneMat);
			Vector3 vHand = boneMat.d;
			Vector3 vEndPoint = vHand;

			// Once we break the glass we should try to keep the rope pinned outside
			if(m_bShouldPinToEndVertex)
			{
				vEndPoint = VEC3V_TO_VECTOR3(m_vEndAttachPos);
			}
			else if(m_iNextRopeVertex < pRope->GetVertsCount() - 1)
			{
				// update current last vertex passed
				float fAnchorToHandDist = m_vAnchorPos.Dist(vEndPoint);

				if(m_iNextRopeVertex > 1)
				{
					float fDistanceToCurrent = fRopeSegmentLength * (m_iNextRopeVertex - 1);
					if(fAnchorToHandDist < fDistanceToCurrent)
					{
						m_iNextRopeVertex--;
					}
				}

				float fDistanceToNext = fRopeSegmentLength * m_iNextRopeVertex;
				if(fAnchorToHandDist > fDistanceToNext)
				{
					m_iNextRopeVertex++;
				}
			}

			Vector3 vUnitVector = (vEndPoint - m_vAnchorPos);
			vUnitVector.Normalize();

			pRope->Pin(0, VECTOR3_TO_VEC3V(m_vAnchorPos));
#if __BANK
			TUNE_GROUP_BOOL(RAPPEL_ROPE, bRenderRopeVertex, false);
			TUNE_GROUP_FLOAT(RAPPEL_ROPE, fSphereRenderSize, 0.1f, 0.0f, 1.f, 0.1f);
#endif
			TUNE_GROUP_INT(RAPPEL_ROPE, iPlusVertexToPin, 1, 0, 100, 1);
			int iLastVertexToPin = m_bShouldPinToEndVertex ? m_iEndAttachVertex : m_iNextRopeVertex + iPlusVertexToPin;
			iLastVertexToPin = Clamp(iLastVertexToPin, 1, (pRope->GetVertsCount() - 1));
			for(int i = 1; i < pRope->GetVertsCount(); i++)
			{
				if(i <= iLastVertexToPin)
				{
					float fRopeLengthToVertex = fRopeSegmentLength * i;
					Vector3 vPinVertex = m_vAnchorPos + (fRopeLengthToVertex * vUnitVector);

					// All verts prior to the next vertex get pinned directly to their logical point
					if(i != iLastVertexToPin || i == m_iEndAttachVertex)
					{
						pRope->Pin(i, VECTOR3_TO_VEC3V(vPinVertex));
					}
					else
					{
						// otherwise interpolate nicely to avoid pop
						Vector3 vNextVertexPos = VEC3V_TO_VECTOR3(pRope->GetWorldPosition(i));

						// If the position is close enough to the logical point then go ahead and pin it directly
						TUNE_GROUP_FLOAT(RAPPEL_ROPE, fPinVertexDistance, 0.02f, 0.0f, 1000.f, 0.1f);
						if(vPinVertex.Dist(vNextVertexPos) <= fPinVertexDistance)
						{
							pRope->Pin(i, VECTOR3_TO_VEC3V(vPinVertex));
						}
						else
						{
							// TODO: look into replacing this with a distance based interpolation, possibly just changing
							// how fRopeVertexInterpSpeed is calculated?
							Vector3 vInterpolatedVertexPos;
							TUNE_GROUP_FLOAT(RAPPEL_ROPE, fLerpBlend, 0.3f, 0.0f, 1.0f, 0.1f);
							vInterpolatedVertexPos = rage::Lerp(fLerpBlend, vNextVertexPos, vPinVertex);
							
							pRope->Pin(i, VECTOR3_TO_VEC3V(vInterpolatedVertexPos));
						}
					}
				}
				else
				{
					pRope->UnPin(i);
				}
#if __BANK
				if (bRenderRopeVertex)
				{
					//Render all vertex
					if (i != m_iNextRopeVertex)
					{
						grcDebugDraw::Sphere(pRope->GetWorldPosition(i), fSphereRenderSize, Color_blue);
					}

					int iPreviousVertex = i == 0 ? 0 : i - 1;
					grcDebugDraw::Line(pRope->GetWorldPosition(iPreviousVertex), pRope->GetWorldPosition(i), Color_red);
				}
#endif
			}
#if __BANK
			if (bRenderRopeVertex)
			{
				grcDebugDraw::Sphere(m_vAnchorPos, fSphereRenderSize, Color_red);
				grcDebugDraw::Sphere(pRope->GetWorldPosition(m_iNextRopeVertex), fSphereRenderSize, Color_green);
			}
#endif
			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}

#if !__FINAL
const char * CTaskRappel::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_ClimbOverWall:					return "State_ClimbOverWall";
	case State_WaitForNetworkRope:				return "State_WaitForNetworkRope";
	case State_Idle:							return "State_Idle";
	case State_IdleAtDestHeight:				return "State_IdleAtDestHeight";
	case State_Descend:							return "State_Descend";
	case State_Jump:							return "State_Jump";
	case State_SmashWindow:						return "State_SmashWindow";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

CClonedRappelDownWallInfo::CClonedRappelDownWallInfo(s32 iState, float fDestinationHeight, float fStartHeading, const Vector3& vStartPos, const Vector3& vAnchorPos, int ropeID, u32 overrideBaseClipSetIdHash, bool skipClimbOverWall, u16 uOwnerJumpID) :
	m_fDestinationHeight(fDestinationHeight)
	, m_fStartHeading(fStartHeading)
	, m_vStartPos(vStartPos)
	, m_vAnchorPos(vAnchorPos)
	, m_iNetworkRopeIndex(-1)
	, m_OverrideBaseClipSetIdHash(overrideBaseClipSetIdHash)
	, m_bSkipClimbOverWall(skipClimbOverWall)
	, m_uOwnerJumpID(uOwnerJumpID)
{
    SetStatusFromMainTaskState(iState);

    m_iNetworkRopeIndex = NetworkInterface::GetNetworkRopeIDFromLocalID(ropeID);
}

CTaskFSMClone *CClonedRappelDownWallInfo::CreateCloneFSMTask()
{
    int localRopeIndex = NetworkInterface::GetLocalRopeIDFromNetworkID(m_iNetworkRopeIndex);

	return rage_new CTaskRappel(m_fDestinationHeight, m_vStartPos, m_vAnchorPos, localRopeIndex, fwMvClipSetId(m_OverrideBaseClipSetIdHash), m_bSkipClimbOverWall, m_uOwnerJumpID);
}
CTask *CClonedRappelDownWallInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
    return CreateCloneFSMTask();
}
