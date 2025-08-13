// File header
#include "Event/EventWeapon.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwMaths/geomutil.h"

// Game headers
#include "Audio/PoliceScanner.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Event/EventResponseFactory.h"
#include "game/Wanted.h"
#include "PedGroup/PedGroup.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PlayerSpecialAbility.h"
#include "physics/WorldProbe/worldprobe.h"
#include "system/ControlMgr.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Combat/Subtasks/TaskDraggingToSafety.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskReactToDeadPed.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAI/VehicleIntelligence.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/train.h"
#include "Weapons/Info/WeaponInfoManager.h"

// Testing Tense Anims In New Cover Task
#if __BANK
#include "Task/Combat/Cover/TaskCover.h"
#endif

#include "event/EventWeapon_parser.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventGunAimedAt
//////////////////////////////////////////////////////////////////////////

// Constants
const float CEventGunAimedAt::INCREASED_PERCEPTION_DIST      = 12.0f;
const float CEventGunAimedAt::INCREASED_PERCEPTION_MAX_ANGLE = -1.0f;
const float CEventGunAimedAt::MAX_PERCEPTION_DIST = 25.0f;
const float CEventGunAimedAt::MAX_PERCEPTION_DIST_ANGLE = 0.707f;

IMPLEMENT_WEAPON_EVENT_TUNABLES(CEventGunAimedAt, 0x116a9571);

CEventGunAimedAt::Tunables CEventGunAimedAt::sm_Tunables;

CEventGunAimedAt::CEventGunAimedAt(CPed* pAggressorPed)
: m_pAggressorPed(pAggressorPed)
{
#if !__FINAL
	m_EventType = EVENT_GUN_AIMED_AT;
#endif // !__FINAL
}

CEventGunAimedAt::~CEventGunAimedAt()
{
}

bool CEventGunAimedAt::operator==(const fwEvent& otherEvent) const
{
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

	if(other.GetEventType() == EVENT_GUN_AIMED_AT)
	{
		if(static_cast<const CEventGunAimedAt*>(&other)->GetSourceEntity() == GetSourceEntity())
		{
			return true;
		}
	}

	return false;
}

bool CEventGunAimedAt::IsEventReady(CPed* pPed)
{
	//Note: We think it is better for task processing to be performed in IsEventReady
	//		instead of AffectsPedCore, since it is likely that AffectsPedCore will
	//		be called many times when sitting in an event queue, whereas IsEventReady
	//		will most likely be called only once.
	
	//Check if the ped is running the drag task.
	CTaskDraggingToSafety* pDraggingTask = static_cast<CTaskDraggingToSafety *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DRAGGING_TO_SAFETY));
	if(pDraggingTask)
	{
		//Notify the drag task that a gun has been aimed.
		pDraggingTask->OnGunAimedAt(m_pAggressorPed);
	}
	
	//Check if the event is ready.
	bool bIsEventReady = CEventEditableResponse::IsEventReady(pPed);
	if(bIsEventReady)
	{
		//Check if the ped is law-enforcement, and the aggressor is a player.
		if(pPed->IsLawEnforcementPed() && m_pAggressorPed && m_pAggressorPed->IsPlayer())
		{
			//Report the crime.
			CCrime::ReportCrime(CRIME_TARGET_COP, pPed, m_pAggressorPed);
		}
	}

	return bIsEventReady;
}

bool CEventGunAimedAt::AffectsPedCore(CPed* pPed) const
{
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

	if (pPed->IsPlayer() || !m_pAggressorPed || pPed->IsInjured())
	{ 
		return false;
	}

	CTask* pTaskActive = pPedIntelligence->GetTaskActive();
	if (pTaskActive && pTaskActive->GetTaskType() == CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT)
	{
		return false;
	}

	if (CPedGroups::AreInSameGroup(pPed, m_pAggressorPed.Get()))
	{
		return false;
	}  

	u32 iWeaponHash = m_pAggressorPed->GetWeaponManager() ? m_pAggressorPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;
	const CWeaponInfo* pTurretWeaponInfo = m_pAggressorPed->GetWeaponManager() ? m_pAggressorPed->GetWeaponManager()->GetEquippedTurretWeaponInfo() : NULL;
	
	if(iWeaponHash == 0 && !pTurretWeaponInfo)
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	if( camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() &&
		camInterface::GetGameplayDirector().IsFirstPersonModeAllowed() )
	{
		CPed* pPedThreat = GetTargetPed();
		if( pPedThreat && pPedThreat->GetPlayerInfo() && !pPedThreat->GetPlayerInfo()->IsAiming() )
		{
			// In first person mode, player is alway aiming, so ignore 
			// weapon unless player is really aiming or shooting.
			return false;
		}
	}
#endif // FPS_MODE_SUPPORTED

	const CWeaponInfo* pWeaponInfo = pTurretWeaponInfo ? pTurretWeaponInfo : CWeaponInfoManager::GetInfo<CWeaponInfo>(iWeaponHash);
	// Don't react to non-gun and non throwable weapons
	if(!pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() && !pWeaponInfo->GetIsProjectile() && !pWeaponInfo->GetIsDangerousLookingMeleeWeapon())
	{
		return false;
	}

	//Register the hostile action.
	CWanted::RegisterHostileAction(m_pAggressorPed);

	//Register this as a hostile event with CTaskAgitated and ignore if possible.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return false;
	}
	
	if(GetPlayerCriminal())
	{
		pPedIntelligence->SetTimeLastAimedAtByPlayer(fwTimer::GetTimeInMilliseconds());
	}

	//Grab the threat response task.
	CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
	if(pTaskThreatResponse)
	{
		pTaskThreatResponse->OnGunAimedAt(*this);
	}

	//Notify react and flee task.
	CTaskReactAndFlee* pTaskReactAndFlee = static_cast<CTaskReactAndFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_AND_FLEE));
	if(pTaskReactAndFlee)
	{
		pTaskReactAndFlee->OnGunAimedAt(*this);
	}

	//Grab the combat task.
	CTaskCombat* pCombatTask = pTaskThreatResponse ? 
							   static_cast<CTaskCombat *>(pTaskThreatResponse->FindSubTaskOfType(CTaskTypes::TASK_COMBAT)) :
							   static_cast<CTaskCombat *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pCombatTask)
	{
		pCombatTask->OnGunAimedAt(*this);
	}

	// Don't bother to react if already in combat, just make sure the target is registered
	if(!pPedIntelligence->IsFriendlyWith(*m_pAggressorPed))
	{
		if( pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT)
			&& pPedIntelligence->GetIsTargetingSystemActive() )
		{
			CPedTargetting* pPedTargeting = pPedIntelligence->GetTargetting();
			if(pPedTargeting)
			{
				pPedTargeting->RegisterThreat(m_pAggressorPed, true, false, true);
				return false;
			}
		}
	}

	CTaskSmartFlee* pFleeTask = static_cast<CTaskSmartFlee*>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if (pFleeTask)
	{
		if (pFleeTask->SuppressingHandsUp())
		{
			return false;
		}

		float fDistanceToTargetSq = DistSquared(m_pAggressorPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf();
		//float fFacing = pPed->GetB().Dot(m_pAggressorPed->GetB());	// Are we facing each other?
		//if ((fDistanceToTarget < 5.0f) && (fFacing > 0.0f))
		if (fDistanceToTargetSq < square(10.0f))
		{
			pFleeTask->SetPriorityEvent(EVENT_GUN_AIMED_AT);
		}
	}

	CTaskCowerScenario* pCowerTask = static_cast<CTaskCowerScenario*>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_COWER_SCENARIO));
	if (pCowerTask)
	{
		// Update the cowering status.
		if (pCowerTask->ShouldUpdateCowerStatus(*this))
		{
			pCowerTask->UpdateCowerStatus(*this);
		}
	}

	CVehicle * pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle && pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN && pPed->GetIsDrivingVehicle() && pPed->PopTypeIsRandom() && m_pAggressorPed->IsPlayer())
	{
		CTrain * pTrain = (CTrain*)pVehicle;
		Assert(pTrain->IsEngine());

		pTrain->SetThreatened();
	}

	return true;
}

bool CEventGunAimedAt::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	if( CTaskTypes::IsCombatTask(GetResponseTaskType()) && 
		pPed && !pPed->GetPedIntelligence()->CanAttackPed(m_pAggressorPed) )
	{
		return false;
	}

	return true;
}

CPed* CEventGunAimedAt::GetPlayerCriminal() const
{
	if(m_pAggressorPed && m_pAggressorPed->IsPlayer())
	{	
		return m_pAggressorPed.Get();
	}
	else
	{
		return NULL;
	}
}

bool CEventGunAimedAt::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(m_pAggressorPed && m_pAggressorPed->IsPlayer())
	{
		// Player has aimed a gun
		// Respond to this event even though it has a low priority
		const CEntity* pOtherEventSource = otherEvent.GetSourceEntity();
		if(pOtherEventSource == m_pAggressorPed.Get())
		{
			// Already responding to player so let the priorities work out what to do
			return CEvent::TakesPriorityOver(otherEvent);
		}
		else
		{
			// Not already responding to player
			// If other event is damage or threat then return true
			const int iOtherEventType = otherEvent.GetEventType();
			if(EVENT_DAMAGE == iOtherEventType ||
				EVENT_ACQUAINTANCE_PED_HATE == iOtherEventType ||
				EVENT_ACQUAINTANCE_PED_RESPECT == iOtherEventType)
			{
				return true;
			}
			else
			{
				return CEvent::TakesPriorityOver(otherEvent);
			}
		}
	}
	else
	{
		return CEventEditableResponse::TakesPriorityOver(otherEvent);
	}
}

CEntity* CEventGunAimedAt::GetSourceEntity() const
{
	return m_pAggressorPed;
}

// PURPOSE: Checks if the target ped can see the ped aiming at him
bool CEventGunAimedAt::IsAimingInTargetSeeingRange(const CPed& rTargetPed, const CPed& rAimingPed, const CWeaponInfo& /*rWeaponInfo*/)
{
	// Calculate perception distance
	const float fMaxPerceptionDistance = rTargetPed.PopTypeIsMission() ? -1.0f : CEventGunAimedAt::MAX_PERCEPTION_DIST;

	// Calculate perception angle
	float fMaxPerceptionAngle;

	bool bWeaponHasActiveFlashlight = false; //NOTE[egarcia]: This condition is not longer initialized. Should it be retrieved from the weapon?
	if ( bWeaponHasActiveFlashlight )
	{
		fMaxPerceptionAngle = -1.0f;
	}
	else if ( rTargetPed.PopTypeIsMission() )
	{
		fMaxPerceptionAngle = 0.0f;
	} 
	else
	{
		fMaxPerceptionAngle = CEventGunAimedAt::MAX_PERCEPTION_DIST_ANGLE;
	}

	//Check if the player can be perceived by the target.  Use the head orientation of the target for perception purposes.
	const bool bWasUsingHeadOrientationForPerception = rTargetPed.GetPedResetFlag(CPED_RESET_FLAG_UseHeadOrientationForPerception);
	const bool bUseHeadOrientationForPerception = rTargetPed.PopTypeIsRandom();
	if(bUseHeadOrientationForPerception)
	{
		// We are restoring the flag just after the check
		const_cast<CPed&>(rTargetPed).SetPedResetFlag(CPED_RESET_FLAG_UseHeadOrientationForPerception, true);
	}

	const bool bUseIncreasedPerceptionDist = !rTargetPed.GetPedConfigFlag(CPED_CONFIG_FLAG_UseTargetPerceptionForCreatingAimedAtEvents);
	// Calculate SeeingInRange
	const bool bIsInSeeingRange = rTargetPed.GetPedIntelligence()->GetPedPerception().IsInSeeingRange(
		VEC3V_TO_VECTOR3(rAimingPed.GetTransform().GetPosition()),
		bUseIncreasedPerceptionDist ? CEventGunAimedAt::INCREASED_PERCEPTION_DIST : -1.0f, 
		CEventGunAimedAt::INCREASED_PERCEPTION_MAX_ANGLE, 
		fMaxPerceptionDistance, 
		fMaxPerceptionAngle);

	// Restore UseHeadOrientationForPerception
	const_cast<CPed&>(rTargetPed).SetPedResetFlag(CPED_RESET_FLAG_UseHeadOrientationForPerception, bWasUsingHeadOrientationForPerception);

	return bIsInSeeingRange;
}

//////////////////////////////////////////////////////////////////////////
// CEventGunShot
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_WEAPON_EVENT_TUNABLES(CEventGunShot, 0xf2d0bf13);

CEventGunShot::Tunables CEventGunShot::sm_Tunables;

float  CEventGunShot::ms_fGunShotSenseRangeForRiot2 = -1.0f;
u32 CEventGunShot::ms_iLastTimeShotFired         = 0;

CPed* CEventGunShot::GetDriverOfFiringVehicleIfLeaderIsLocalPlayer(const CVehicle& rFiringVeh, const CPed& rPed)
{
	if (rPed.IsPlayer())
		return NULL;

	if (!rPed.GetIsInVehicle())
		return NULL;

	// We probably don't need the group check for this, but to limit the scope of impact
	// this specifically targets a fix for B*1601611, i.e. we only change the firing ped to be the driver
	// of the vehicle for the friendly tests if that driver is our group leader.
	const CPedGroup* pPedGroup = rPed.GetPedsGroup();
	if (pPedGroup && pPedGroup->GetGroupMembership()->GetLeader() && pPedGroup->GetGroupMembership()->GetLeader()->IsLocalPlayer() && pPedGroup->GetGroupMembership()->GetLeader()->GetIsInVehicle())
	{
		return rFiringVeh.GetDriver();
	}
	return NULL;
}

CEventGunShot::CEventGunShot(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, bool bSilent, u32 weaponHash, float maxSoundRange)
: m_pFiringEntity(pFiringEntity)
, m_vShotOrigin(vShotOrigin)
, m_vShotTarget(vShotTarget)
, m_bSilent(bSilent)
, m_bFirstCheck(true)
, m_bIsReady(false)
, m_bIsJustReady(false)
, m_fMaxSoundRange(maxSoundRange)
, m_weaponHash(weaponHash)
{
	ms_iLastTimeShotFired = fwTimer::GetTimeInMilliseconds();

#if !__FINAL
	m_EventType = EVENT_SHOT_FIRED;
#endif // !__FINAL
}

CEventGunShot::~CEventGunShot()
{
}

bool CEventGunShot::IsValidForPed(CPed* pPed) const
{
	if(m_bIsReady && !m_bIsJustReady)
	{
		return CEvent::IsValidForPed(pPed);
	}
	else
	{
		return true;
	}
}

bool CEventGunShot::IsEventReady(CPed* pPed)
{
	// If the ped is already reacting to anything, don't delay the gunshot event
	if(pPed->GetPedIntelligence()->GetTaskEventResponse())
	{
		m_bIsJustReady = !m_bIsReady;
		m_bIsReady = true;
		SetDelayTimer(0.0f);
		return true;
	}

	// Some ped types never have a delay.
	// This might introduce some "twinning" effects (which was the whole reason to have the delays in the first place),
	// but there hopefully won't be too many peds with this flag on screen at once.  Currently its set on only the dogs.
	if(pPed->GetTaskData().GetIsFlagSet(CTaskFlags::IgnoreDelaysOnGunshotEvents))
	{
		m_bIsJustReady = !m_bIsReady;
		m_bIsReady = true;
		SetDelayTimer(0.0f);
		return true;
	}

	if(m_bIsReady)
	{
		m_bIsJustReady = false;
		return true;
	}

	m_bIsReady = CEvent::IsEventReady(pPed);

	if(m_bIsReady && !m_bIsJustReady)
	{
		ResetAccumulatedTime();
		SetDelayTimer(0.0f);
		m_bIsJustReady = true;
	}

	return m_bIsReady;
}

bool CEventGunShot::operator==(const fwEvent& otherEvent) const
{
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

	if(other.GetEventType() == EVENT_SHOT_FIRED)
	{
		if(static_cast<const CEventGunShot*>(&other)->GetSourceEntity() == m_pFiringEntity)
		{
			return true;
		}
	}

	return false;
}

bool CEventGunShot::AffectsPedCore(CPed* pPed) const
{
	if(!m_pFiringEntity)
	{
		return false;
	}

	if(pPed == m_pFiringEntity)
	{
		return false;
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	CPed* pFiringPed = NULL;
	CVehicle* pFiringVehicle = NULL;
	bool bGunshotFromFriendly = false;
	if(m_pFiringEntity->GetType() == ENTITY_TYPE_PED)
	{
		pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());
	}
	else if (m_pFiringEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		pFiringVehicle = static_cast<CVehicle*>(m_pFiringEntity.Get());
		pFiringPed = CEventGunShot::GetDriverOfFiringVehicleIfLeaderIsLocalPlayer(*pFiringVehicle, *pPed);
	}
	
	if (pFiringPed)
	{
		if(CPedGroups::AreInSameGroup(pPed, pFiringPed))
		{
			bGunshotFromFriendly = true;
		}
		else if(pFiringPed == pPed)
		{
			return false;
		}
		else if(pPed->GetPedIntelligence()->IsFriendlyWith(*pFiringPed))
		{
			bGunshotFromFriendly = true;
		}
	}

	if (bGunshotFromFriendly && pFiringVehicle)
	{
		return false;
	}

	bool bNonLocalPlayer = pPed->IsPlayer() && !pPed->IsLocalPlayer();

	if(pPed->IsDead() || bNonLocalPlayer )
	{
		return false;
	}
	else
	{
		// Ignore gunshots if they occur in a different interior state than the sensing ped.
		if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(*pPed, GetSourceEntity(), GetEntityOrSourcePosition()))
		{
			return false;
		}

		// Only accept gunshots that the ped can hear
		// Test if gun was fired in the ped's sense range
		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_pFiringEntity->GetTransform().GetPosition());
		float fDist2 = vDiff.Mag2();
		float fSenseRange = pPed->GetPedIntelligence()->GetPedPerception().GetSenseRange();

		// Might want to mod hearing range for different weapons?
		// Maybe use new hearing stuff.

		if(ms_fGunShotSenseRangeForRiot2 > 0.0f && pPed->PopTypeIsMission())
		{
			fSenseRange = ms_fGunShotSenseRangeForRiot2;
		}
		else if(pPed->PopTypeIsMission())
		{
			fSenseRange = pPed->GetPedIntelligence()->GetPedPerception().GetSenseRange();
		}
		else
		{
			fSenseRange = !m_bSilent && pPed->GetIsVisibleInSomeViewportThisFrame() ? 500.0f : 45.0f;
		}

		// Don't go above the max range the user passed in. This is mostly to support weapons
		// that shouldn't be considered silent, but also wouldn't be heard from very far,
		// such as a stun gun.
		fSenseRange = Min(fSenseRange, m_fMaxSoundRange);

		if(fDist2 > (fSenseRange * fSenseRange))
		{
			// Gun was fired out of range
			return false;
		}
		else
		{
			// Gun was fired in range
			if(m_bSilent)
			{
				// If we're further than 10m away from the shooter, ignore this silenced shot
				static dev_float fSilentRange = 10.0f;
				if (fDist2 > fSilentRange*fSilentRange )
				{
					return false;
				}

				// If the gun shot is silenced an the ped can't see it, he doesn't know
				Vector3	VecToGunShot = m_vShotOrigin - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				if (DotProduct(VecToGunShot, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())) <= 0)
				{
					// Ped is facing away from source of shot
					return false;
				}
				else
				{
					// Restrict this expensive probe test to when the shooter is a player.
					if (pFiringPed && pFiringPed->IsAPlayerPed())
					{
						const u32 includeFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
						const u32 typeFlags = 0xffffffff;
						// Possibility of having 2 full cars excluded from the test, so make the array big enough
						const int EXCLUSION_MAX = MAX_INDEPENDENT_VEHICLE_ENTITY_LIST * 2;
						const CEntity* exclusionList[EXCLUSION_MAX];
						int nExclusions = 0;

						// Exclude the reacting ped and the m_pFiringEntity from the collision check
						if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
						{
							// Exclude pPed's vehicle as well
							pPed->GetMyVehicle()->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, pPed->GetMyVehicle());
						}
						else
						{
							pPed->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, pPed);
						}

						if(pFiringPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pFiringPed->GetMyVehicle())
						{
							// Exclude m_pFiringEntity's vehicle as well
							pFiringPed->GetMyVehicle()->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, pFiringPed->GetMyVehicle());
						}
						else
						{
							m_pFiringEntity->GeneratePhysExclusionList(exclusionList, nExclusions, EXCLUSION_MAX,includeFlags,typeFlags, m_pFiringEntity);
						}

						//! Allow players to sense without LOS checks for battle awareness.
						if(!pPed->IsPlayer())
						{
							// If the peds skeleton has been updated then use their head bone as the end point for the LOS check
							// Otherwise just use the root bone
							Vector3 vLOSEndPos;
							pPed->GetBonePosition(vLOSEndPos, BONETAG_HEAD);

							// Fire a probe from m_vShotOrigin to vLOSEndPos - if its clear then
							WorldProbe::CShapeTestProbeDesc probeDesc;
							probeDesc.SetStartAndEnd(m_vShotOrigin, vLOSEndPos);
							probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
							probeDesc.SetExcludeEntities(exclusionList, nExclusions);
							probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
							if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
							{
								// Ped is facing source of shot but can't see it
								return false;
							}
						}
					}
				}
			}

			if(!pPed->IsPlayer())
			{
				//Grab the threat response task.
				CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
					pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
				if(pTaskThreatResponse)
				{
					pTaskThreatResponse->OnGunShot(*this);
				}
			}

			if (bGunshotFromFriendly)
			{
				// It can affect the ped, but we don't want to do any of the hostile threat code below if from a friendly.
				return !pPed->IsPlayer();
			}

			if(pPed->IsPlayer())
			{
				if(pPed->GetPedIntelligence()->CanEntityIncreaseBattleAwareness(m_pFiringEntity))
				{
					pPed->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_GUNSHOT);
				}

				return false;
			}
			else
			{
				//Register the hostile action.
				CWanted::RegisterHostileAction(pFiringPed);

				//Send the event on to the ped if cowering.
				CTaskCowerScenario::NotifyCoweringPedOfEvent(*pPed, *this);

				//Register this as a hostile event with CTaskAgitated and ignore if possible.
				if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
				{
					return false;
				}


				u32 iCurrentTime = fwTimer::GetTimeInMilliseconds();
				u32 iDiffGunshot = iCurrentTime - pPed->GetPedIntelligence()->GetLastHeardGunFireTime();
				// If we've heard gunfire within the tolerance, don't increase alertness state
				if (iDiffGunshot < CPedIntelligence::ms_iLastHeardGunFireTolerance)
				{
					return true;
				}

				pPed->GetPedIntelligence()->SetLastHeardGunFireTime(fwTimer::GetTimeInMilliseconds());	
				
	// 			CPedIntelligence::AlertnessState alert_state = pPed->GetPedIntelligence()->GetAlertnessState();
	// 			
 				pPed->GetPedIntelligence()->IncreaseAlertnessState();
	// 
	// 			if (alert_state == CPedIntelligence::AS_NotAlert)
	// 			{
	// 				// Do not react at a singular gunshot if we're not alert
	// 				return true;
	// 			}

				// Gunfire events are higher priority than investigating, but since it's part of threat response it won't abort for a new threat response
				// instead we affect the investigation task by informing it of this new priority event
				CTaskReactToDeadPed* pReactTask = static_cast<CTaskReactToDeadPed*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_TO_DEAD_PED));
				if (pReactTask)
				{
					aiFatalAssertf(GetSourceEntity(),"No Source Entity Exists for this event!");
					// The guy who is shooting most likely killed him
					if (GetSourceEntity()->GetIsTypePed())
					{
						pReactTask->SetThreatPed(static_cast<CPed*>(GetSourceEntity()));
					}
					return true;
				}

				CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE);
				if (pTask)
				{
					static_cast<CTaskSmartFlee*>(pTask)->SetPriorityEvent(EVENT_SHOT_FIRED);
				}

				// If we're investigating already, use the source entity location as the new investigation position
				CTaskInvestigate* pInvestigateTask = static_cast<CTaskInvestigate*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE));
				if (pInvestigateTask)
				{
					aiFatalAssertf(GetSourceEntity(),"No Source Entity Exists for this event!");
					if (GetSourceEntity() && pInvestigateTask->GetCanSetNewPos())
					{
						pInvestigateTask->SetInvestigationPos(VEC3V_TO_VECTOR3(GetSourceEntity()->GetTransform().GetPosition()));
					}
					return true;
				}
				
				//Grab the combat task.
				CTaskCombat* pTaskCombat = static_cast<CTaskCombat *>(
					pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
				if(pTaskCombat)
				{
					pTaskCombat->OnGunShot(*this);

					if( pFiringPed && pPed->GetPedIntelligence()->GetIsTargetingSystemActive() && pPed->GetPedIntelligence()->CanAttackPed(pFiringPed) )
					{
						pPed->GetPedIntelligence()->RegisterThreat(pFiringPed, true);
						return false;
					}
				}
		
				return true;
			}
		}
	}
}

bool CEventGunShot::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	if( m_pFiringEntity && m_pFiringEntity->GetIsTypePed() &&
		CTaskTypes::IsCombatTask(GetResponseTaskType()) && 
		pPed && !pPed->GetPedIntelligence()->CanAttackPed(static_cast<const CPed*>(m_pFiringEntity.Get())) )
	{
		return false;
	}

	return true;
}

CPed* CEventGunShot::GetSourcePed() const
{
	if(m_pFiringEntity)
	{
		if(m_pFiringEntity->GetIsTypeVehicle())
		{
			return static_cast<CVehicle *>(m_pFiringEntity.Get())->GetDriver();
		}
	}

	return CEventEditableResponse::GetSourcePed();
}

CPed* CEventGunShot::GetPlayerCriminal() const
{
	CPed* pSourcePed = GetSourcePed();
	if(pSourcePed && pSourcePed->IsPlayer())
	{
		return pSourcePed;
	}
	else
	{
		return NULL;
	}
}

CTask* CEventGunShot::CreateVehicleReponseTaskWithParams(sVehicleMissionParams& paramsOut) const
{
	paramsOut.Clear();
	paramsOut.m_iDrivingFlags =DMode_AvoidCars|DF_AvoidTargetCoors|DF_SteerAroundPeds;
	paramsOut.m_fCruiseSpeed = 40.0f;
	if (GetSourceEntity())
	{
		paramsOut.SetTargetEntity(GetSourceEntity());
	}
	else
	{
		paramsOut.SetTargetPosition(GetSourcePos());
	}

	return rage_new CTaskVehicleFlee(paramsOut);
}

bool CEventGunShot::AffectsVehicleCore(CVehicle* pVehicle) const
{
	if (!pVehicle->GetIntelligence()->ShouldHandleEvents())
	{
		return false;
	}

	if (!m_pFiringEntity)
	{
		return false;
	}

	// Only accept gunshots that the ped can hear
	// Test if gun was fired in the ped's sense range
	Vector3 vDiff = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition() - m_pFiringEntity->GetTransform().GetPosition());
	float fDist2 = vDiff.Mag2();
	float fSenseRange = !m_bSilent && pVehicle->GetIsVisibleInSomeViewportThisFrame() ? 500.0f : 45.0f;

	// Don't go above the max range the user passed in. This is mostly to support weapons
	// that shouldn't be considered silent, but also wouldn't be heard from very far,
	// such as a stun gun.
	fSenseRange = Min(fSenseRange, m_fMaxSoundRange);

	if(fDist2 > (fSenseRange * fSenseRange))
	{
		// Gun was fired out of range
		return false;
	}

	return true;
}

bool CEventGunShot::WasShotFiredWithinTime(u32 timeMs)
{
	// Note: we check for 0 here, because that's the initial value and it would be wrong
	// to say that a shot was just fired right after the game starts up (could interfere with
	// the initial population of the world at scenarios and stuff).
	if(ms_iLastTimeShotFired)
	{
		if(ms_iLastTimeShotFired + timeMs > fwTimer::GetTimeInMilliseconds())
		{
			return true;
		}
	}
	return false;
}

bool CEventGunShot::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType() == GetEventType())
	{
		const CEventGunShot& otherGunShotEvent = static_cast<const CEventGunShot&>(otherEvent);
		const bool bIsPlayer      = (m_pFiringEntity && m_pFiringEntity->GetType() == ENTITY_TYPE_PED) ? static_cast<CPed*>(m_pFiringEntity.Get())->IsPlayer() : false;		
		const bool bIsOtherPlayer = (otherGunShotEvent.m_pFiringEntity && otherGunShotEvent.m_pFiringEntity->GetType() == ENTITY_TYPE_PED) ? static_cast<CPed*>(otherGunShotEvent.m_pFiringEntity.Get())->IsPlayer() : false;

		if(bIsPlayer && bIsOtherPlayer)
		{
			// Both involve player so otherEvent will do
			return false;
		}
		else if(bIsPlayer)
		{
			// Only this event involves the player. Always respond to the player
			return CEvent::TakesPriorityOver(otherEvent);
		}
		else if(bIsOtherPlayer)
		{
			// Only other event is the player.  Just carry on with other event
			return false;
		}
		else
		{
			// Neither is the player so just carry on with other event
			return false;
		}
	}
	else if(otherEvent.GetEventType() == EVENT_GUN_AIMED_AT)
	{
		return true;
	}
	else
	{
		// Events of different type so use the priorities
		return CEvent::TakesPriorityOver(otherEvent);
	}
}

const Vector3& CEventGunShot::GetSourcePos() const
{
	return m_vShotOrigin;
}

//////////////////////////////////////////////////////////////////////////
// CEventGunShotWhizzedBy
//////////////////////////////////////////////////////////////////////////

CEventGunShotWhizzedBy::CEventGunShotWhizzedBy(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, const bool bIsSilent, u32 weaponHash, bool bFromNetwork)
: CEventGunShot(pFiringEntity, vShotOrigin, vShotTarget, bIsSilent, weaponHash, LARGE_FLOAT)
, m_bFromNetwork(bFromNetwork)
{
#if !__FINAL
	m_EventType = EVENT_SHOT_FIRED_WHIZZED_BY;
#endif // !__FINAL
}

bool CEventGunShotWhizzedBy::operator==(const fwEvent& otherEvent) const
{
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

	if(/*other.GetEventType() == EVENT_SHOT_FIRED_BULLET_IMPACT ||*/
		other.GetEventType() == EVENT_SHOT_FIRED_WHIZZED_BY )
	{
		if(static_cast<const CEventGunShot*>(&other)->GetSourceEntity() == m_pFiringEntity)
		{
			return true;
		}
	}

	return false;
}

bool CEventGunShotWhizzedBy::IsBulletInRange(const Vector3& vTestPosition, Vec3V_InOut vClosestPosition) const
{
	fwGeom::fwLine pathLine;
	pathLine.m_start = m_vShotOrigin;
	pathLine.m_end = m_vShotTarget;

	// Find the closest point on a line to the bullet's impact.
	pathLine.FindClosestPointOnLineV(RCC_VEC3V(vTestPosition), vClosestPosition);

	// Check if that point is less than a certain threshold.
	if (IsLessThanAll(DistSquared(vClosestPosition, RCC_VEC3V(vTestPosition)), ScalarV(CEventGunShot::sm_Tunables.m_GunShotThresholdDistance * CEventGunShot::sm_Tunables.m_GunShotThresholdDistance)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CEventGunShotWhizzedBy::AffectsVehicleCore(CVehicle* pVehicle) const
{
	if (!pVehicle->GetIntelligence()->ShouldHandleEvents())
	{
		return false;
	}

	//assumption: pretend occupants are not in any groups or have any
	//special properties. so we treat them as if they're not affiliated
	//with anything that can shoot guns, and should be scared of all of them


	// Test if the gunshot whizzes by
	Vec3V vClosestPosition;
	const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	bool bAffectsVeh = IsBulletInRange(vVehPosition, vClosestPosition);

	if (!bAffectsVeh)
	{
		return false;
	}

	// If it's a silenced weapon then ignore
	if (m_bSilent)
	{
		return false;
	}

	// Veh is unaffected
	return bAffectsVeh;
}

bool CEventGunShotWhizzedBy::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}

	if(pPed == m_pFiringEntity)
	{
		return false;
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	// Ignore gunshots if they occur in a different interior state than the sensing ped.
	if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(*pPed, GetSourceEntity(), GetEntityOrSourcePosition()))
	{
		return false;
	}

	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

	CPed* pFiringPed = NULL;
	CVehicle* pFiringVehicle = NULL;
	bool bGunshotFromFriendlyPed = false;
	if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed())
	{
		pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());
	}
	else if (m_pFiringEntity && m_pFiringEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		pFiringVehicle = static_cast<CVehicle*>(m_pFiringEntity.Get());
		pFiringPed = CEventGunShot::GetDriverOfFiringVehicleIfLeaderIsLocalPlayer(*pFiringVehicle, *pPed);
	}

	if (pFiringPed)
	{
		if(CPedGroups::AreInSameGroup(pPed,pFiringPed))
		{
			bGunshotFromFriendlyPed = true;
		}
		else if(pFiringPed == pPed)
		{
			return false;
		}
		else if(pPedIntelligence->IsFriendlyWith(*pFiringPed))
		{
			bGunshotFromFriendlyPed = true;
		}

        if(!m_bFromNetwork)
        {
            if(m_bSilent && pFiringPed->IsNetworkClone() && pFiringPed->IsPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventReactingToSilencedCloneBullets))
            {
                return false;
            }
        }
	}
	
	if (bGunshotFromFriendlyPed && pFiringVehicle)
	{
		return false;
	}

	// Test if the gunshot whizzes by
	Vec3V vClosestPosition;
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	bool bAffectsPed = IsBulletInRange(vPedPosition, vClosestPosition);

	if (!bAffectsPed)
	{
		return false;
	}

	CPlayerSpecialAbilityManager::ChargeEvent(ACET_NEAR_BULLET_MISS, pPed);

	bool bNonLocalPlayer = pPed->IsPlayer() && !pPed->IsLocalPlayer();

	// skip for non local players after the charge event is called
	if(bNonLocalPlayer)
	{
		return false;
	}

	if(pPed->IsPlayer())
	{
		if(!bGunshotFromFriendlyPed && pPed->GetPedIntelligence()->CanEntityIncreaseBattleAwareness(m_pFiringEntity))
		{
			pPed->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_BULLET_WHIZBY);
		}
		return false;
	}
	else
	{
		u32 iCurrentTime = fwTimer::GetTimeInMilliseconds();

		bool bIncreasesAlertness = GetWitnessedFirstHand() && bAffectsPed && pFiringPed;
		if (bIncreasesAlertness && (iCurrentTime - pPedIntelligence->GetLastPinnedDownTime()) > CPedIntelligence::ms_iLastPinnedDownTolerance)
		{
			if(!bGunshotFromFriendlyPed || pPed->GetPedResetFlag(CPED_RESET_FLAG_CanBePinnedByFriendlyBullets))
			{
				// Grab the cover task so we can tell it that this event happened
				CTaskInCover* pCoverTask = static_cast<CTaskInCover* >(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
				if(pCoverTask)
				{
					pCoverTask->OnBulletEvent();
				}
				
				if(pFiringPed->GetPedIntelligence()->CanPinDownOthers())
				{
					pPedIntelligence->IncreaseAmountPinnedDown(CTaskInCover::ms_Tunables.m_AmountPinnedDownByBullet);
				}
			}
		}

		if (bGunshotFromFriendlyPed)
		{
			// It can affect the ped, but we don't want to do any of the hostile threat code below if from a friendly.
			return true;
		}

		u32 iDiffSeenGunshot = iCurrentTime - pPedIntelligence->GetLastSeenGunFireTime();
		u32 iDiffHeardGunshot = iCurrentTime - pPedIntelligence->GetLastHeardGunFireTime();

		CTask* pTask = pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE);
		if (pTask)
		{
			static_cast<CTaskSmartFlee*>(pTask)->SetPriorityEvent(EVENT_SHOT_FIRED_WHIZZED_BY);
		}

		// If we've seen or heard gunfire within the tolerance, don't increase alertness state
		if (iDiffSeenGunshot < CPedIntelligence::ms_iLastHeardGunFireTolerance || iDiffHeardGunshot < CPedIntelligence::ms_iLastHeardGunFireTolerance)
		{
			bIncreasesAlertness = false;
		}

		pPedIntelligence->SetLastSeenGunFireTime(iCurrentTime);

		if(bIncreasesAlertness)
		{
			pPedIntelligence->IncreaseAlertnessState();
		}

		//Register the hostile action.
		CWanted::RegisterHostileAction(pFiringPed);

		//Send the event on to the ped if cowering.
		CTaskCowerScenario::NotifyCoweringPedOfEvent(*pPed, *this);

		//Register this as a hostile event with CTaskAgitated and ignore if possible.
		if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
		{
			bAffectsPed = false;
		}
		else
		{
			//Grab the threat response task.
			CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
				pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
			if(pTaskThreatResponse)
			{
				pTaskThreatResponse->OnGunShotWhizzedBy(*this);
			}

			//Grab the combat task.
			CTaskCombat* pTaskCombat = pTaskThreatResponse ? static_cast<CTaskCombat *>(
				pTaskThreatResponse->FindSubTaskOfType(CTaskTypes::TASK_COMBAT)) : NULL;
			if(!pTaskCombat)
			{
				pTaskCombat = static_cast<CTaskCombat *>(
					pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
			}
			if(pTaskCombat)
			{
				pTaskCombat->OnGunShotWhizzedBy(*this);
				if( pFiringPed && pPedIntelligence->GetIsTargetingSystemActive() && pPed->GetPedIntelligence()->CanAttackPed(pFiringPed) )
				{
					CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting();
					if(pPedTargeting)
					{
						pPedTargeting->RegisterThreat(pFiringPed, true, false, true);
						bAffectsPed = false;
					}
				}
			}
			else
			{
				CTaskGun* pGunTask = static_cast<CTaskGun *>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_GUN));
				if(pGunTask)
				{
					pGunTask->OnGunShotWhizzedBy(*this);
				}

				CTaskCombatAdditionalTask* pAdditionalCombatTask = static_cast<CTaskCombatAdditionalTask *>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK));
				if(pAdditionalCombatTask)
				{
					pAdditionalCombatTask->OnGunShotWhizzedBy(*this);
				}
			}
		}

		if (pFiringPed && pPed->IsLawEnforcementPed())
		{
			CCrime::ReportCrime(CRIME_SHOOT_AT_COP, pPed, pFiringPed);
		}
	}

	// Ped is unaffected
	return bAffectsPed;
}

//////////////////////////////////////////////////////////////////////////
// CEventGunShotBulletImpact
//////////////////////////////////////////////////////////////////////////

CEventGunShotBulletImpact::CEventGunShotBulletImpact(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, const bool bIsSilent, u32 weaponHash, bool bMustBeSeenWhenInVehicle, bool bFromNetwork)
: CEventGunShotWhizzedBy(pFiringEntity, vShotOrigin, vShotTarget, bIsSilent, weaponHash)
, m_pHitPed(NULL)
, m_bMustBeSeenWhenInVehicle(bMustBeSeenWhenInVehicle)
, m_bFromNetwork(bFromNetwork)
{
#if !__FINAL
	m_EventType = EVENT_SHOT_FIRED_BULLET_IMPACT;
#endif // !__FINAL
}

bool CEventGunShotBulletImpact::operator==(const fwEvent& otherEvent) const
{
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

	if(other.GetEventType() == EVENT_SHOT_FIRED_BULLET_IMPACT /*||*/
		/*other.GetEventType() == EVENT_SHOT_FIRED_WHIZZED_BY*/)
	{
		if(static_cast<const CEventGunShot*>(&other)->GetSourceEntity() == m_pFiringEntity)
		{
			return true;
		}
	}

	return false;
}

bool CEventGunShotBulletImpact::AffectsVehicleCore(CVehicle* pVehicle) const
{
	if (!pVehicle->GetIntelligence()->ShouldHandleEvents())
	{
		return false;
	}

	float fTestDistance = 15.0f;

	CPed* pFiringPed = NULL;
	if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed())
	{
		pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());

		// CC: Need to think about this more maybe
		// E.g. the case where a guard is outside the door and the player shoots a dead body this is ok, 
		// but if the guard doesn't notice a shot down a corridor against a wall, seems a bit crappy,
		// though this might not be that apparently considering they probably see or hear the player before then.
		if (m_bSilent)
		{	
			// If I am, or the guy firing is indoors, ignore
			if (pVehicle->GetIsInInterior() || pFiringPed->GetIsInInterior())
			{
				return false;
			}

			fTestDistance = rage::Min(fTestDistance, CMiniMap::sm_Tunables.Sonar.fSoundRange_SilencedGunshot);
		}
	}

	// Test to see how close the bullet impacts
	Vector3 w = m_vShotTarget - VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	float fPedToImpactSq = w.Mag2();
	if(fPedToImpactSq < rage::square(fTestDistance))
	{
		if(m_bMustBeSeenWhenInVehicle)
		{
			float fDot = w.Dot(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()));
			if(fDot < 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool CEventGunShotBulletImpact::AffectsPedCore(CPed* pPed) const
{
	bool bNonLocalPlayer = pPed->IsPlayer() && !pPed->IsLocalPlayer();
	if(bNonLocalPlayer)
	{
		return false;
	}

	if(pPed->IsDead())
	{
		return false;
	}

	if(pPed == m_pFiringEntity)
	{
		return false;
	}

	if(pPed == GetSourcePed())
	{
		return false;
	}

	// Ignore gunshots if they occur in a different interior state than the sensing ped.
	if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(*pPed, GetSourceEntity(), GetEntityOrSourcePosition()))
	{
		return false;
	}

	float fTestDistance = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatBulletImpactDetectionRange);

	CPed* pFiringPed = NULL;
	CVehicle* pFiringVehicle = NULL;
	bool bGunshotFromFriendlyPed = false;
	if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed())
	{
		pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());
	}
	else if (m_pFiringEntity && m_pFiringEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		pFiringVehicle = static_cast<CVehicle*>(m_pFiringEntity.Get());
		pFiringPed = CEventGunShot::GetDriverOfFiringVehicleIfLeaderIsLocalPlayer(*pFiringVehicle, *pPed);
	}

	if (pFiringPed)
	{
		// CC: Need to think about this more maybe
		// E.g. the case where a guard is outside the door and the player shoots a dead body this is ok, 
		// but if the guard doesn't notice a shot down a corridor against a wall, seems a bit crappy,
		// though this might not be that apparently considering they probably see or hear the player before then.
		if (m_bSilent)
		{	
			// If I am, or the guy firing is indoors, ignore
			if (pPed->GetIsInInterior() || pFiringPed->GetIsInInterior())
			{
				return false;
			}

			fTestDistance = rage::Min(fTestDistance, CMiniMap::sm_Tunables.Sonar.fSoundRange_SilencedGunshot);
		}

		if(CPedGroups::AreInSameGroup(pPed,pFiringPed))
		{
			bGunshotFromFriendlyPed = true;
		}
		else if(pFiringPed == pPed)
		{
			return false;
		}
		else if(pPed->GetPedIntelligence()->IsFriendlyWith(*pFiringPed))
		{
			bGunshotFromFriendlyPed = true;
		}

        if(!m_bFromNetwork)
        {
            if(m_bSilent && pFiringPed->IsNetworkClone() && pFiringPed->IsPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventReactingToSilencedCloneBullets))
            {
                return false;
            }
        }
	}

	if (bGunshotFromFriendlyPed && pFiringVehicle)
	{
		return false;
	}

	// Test to see how close the bullet impacts
	Vector3 w = m_vShotTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float fPedToImpactSq = w.Mag2();
	if(fPedToImpactSq < rage::square(fTestDistance))
	{
		if(m_bMustBeSeenWhenInVehicle && pPed->GetIsInVehicle())
		{
			float fDot = w.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()));
			if(fDot < 0.0f)
			{
				return false;
			}
		}

		if(GetWitnessedFirstHand() && m_pFiringEntity && m_pFiringEntity->GetIsTypePed())
		{
			if(pPed->IsPlayer())
			{
				//! Add to battle awareness.
				if(!bGunshotFromFriendlyPed && pPed->GetPedIntelligence()->CanEntityIncreaseBattleAwareness(m_pFiringEntity))
				{
					pPed->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_BULLET_IMPACT);
				}
				return false;
			}
			else
			{
				//Register the hostile action.
				CWanted::RegisterHostileAction(pFiringPed);

				CPed* pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());
				u32 iCurrentTime = fwTimer::GetTimeInMilliseconds();

				if((iCurrentTime - pPed->GetPedIntelligence()->GetLastPinnedDownTime()) > CPedIntelligence::ms_iLastPinnedDownTolerance &&
					fPedToImpactSq < square(CTaskInCover::ms_Tunables.m_PinnedDownByBulletRange) && !pPed->GetPedIntelligence()->IsFriendlyWith(*pFiringPed))
				{
					if(!bGunshotFromFriendlyPed || pPed->GetPedResetFlag(CPED_RESET_FLAG_CanBePinnedByFriendlyBullets))
					{
						// Grab the cover task so we can tell it that this event happened
						CTaskInCover* pCoverTask = static_cast<CTaskInCover* >(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
						if(pCoverTask)
						{
							pCoverTask->OnBulletEvent();
						}
					
						if(pFiringPed->GetPedIntelligence()->CanPinDownOthers())
						{
							pPed->GetPedIntelligence()->IncreaseAmountPinnedDown(CTaskInCover::ms_Tunables.m_AmountPinnedDownByBullet);
						}
					}
				}

				if (bGunshotFromFriendlyPed)
				{
					// It can affect the ped, but we don't want to do any of the hostile threat code below if from a friendly.
					return true;
				}

				//Register the hostile action.
				CWanted::RegisterHostileAction(pFiringPed);

				//Register this as a hostile event with CTaskAgitated and ignore if possible.
				if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
				{
					return false;
				}

				u32 iDiffSeenGunshot = iCurrentTime - pPed->GetPedIntelligence()->GetLastSeenGunFireTime();
				u32 iDiffHeardGunshot = iCurrentTime - pPed->GetPedIntelligence()->GetLastHeardGunFireTime();

				// If we've seen or heard gunfire within the tolerance, don't increase alertness state
				if (iDiffSeenGunshot > CPedIntelligence::ms_iLastHeardGunFireTolerance && iDiffHeardGunshot > CPedIntelligence::ms_iLastHeardGunFireTolerance)
				{
					pPed->GetPedIntelligence()->SetLastSeenGunFireTime(iCurrentTime);
					pPed->GetPedIntelligence()->IncreaseAlertnessState();
				}
			}
		}

		if(!pPed->IsPlayer())
		{
			if (bGunshotFromFriendlyPed)
			{
				// It can affect the ped, but we don't want to do any of the hostile threat code below if from a friendly.
				return true;
			}

			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE);
			if (pTask)
			{
				static_cast<CTaskSmartFlee*>(pTask)->SetPriorityEvent(EVENT_SHOT_FIRED_BULLET_IMPACT);
			}

			//Grab the threat response task.
			CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
				pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
			if(pTaskThreatResponse)
			{
				pTaskThreatResponse->OnGunShotBulletImpact(*this);
			}

			//Send the event on to the ped if cowering.
			CTaskCowerScenario::NotifyCoweringPedOfEvent(*pPed, *this);

			//Grab the combat task.
			CTaskCombat* pTaskCombat = pTaskThreatResponse ? static_cast<CTaskCombat *>(
				pTaskThreatResponse->FindSubTaskOfType(CTaskTypes::TASK_COMBAT)) : NULL;
			if(!pTaskCombat)
			{
				pTaskCombat = static_cast<CTaskCombat *>(
					pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
			}
			if(pTaskCombat)
			{
				pTaskCombat->OnGunShotBulletImpact(*this);
				if( pFiringPed && pPed->GetPedIntelligence()->GetIsTargetingSystemActive() && pPed->GetPedIntelligence()->CanAttackPed(pFiringPed) )
				{
					CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting();
					if(pPedTargeting)
					{
						pPedTargeting->RegisterThreat(pFiringPed, true, false, true);
						return false;
					}
				}
			}

			return true;
		}
	}

	return false;
}

bool CEventGunShotBulletImpact::IsEventReady(CPed* pPed)
{
	//Disable the delay if the ped was 'hit'.
	if(pPed == m_pHitPed)
	{
		m_bIsReady = true;
		m_bIsJustReady = false;
		SetDelayTimer(0.0f);
	}

	return CEventGunShotWhizzedBy::IsEventReady(pPed);
}

//////////////////////////////////////////////////////////////////////////
// CEventMeleeAction
//////////////////////////////////////////////////////////////////////////

// Constants
const float CEventMeleeAction::SENSE_RANGE = 8.0f;

CEventMeleeAction::Tunables CEventMeleeAction::sm_Tunables;
IMPLEMENT_WEAPON_EVENT_TUNABLES(CEventMeleeAction, 0x9f22c6c8);

CEventMeleeAction::CEventMeleeAction(CEntity* pActingEntity, CEntity* pHitEntity, bool isOffensive)
: m_pActingEntity(pActingEntity)
, m_pHitEntity(pHitEntity)
, m_isOffensive(isOffensive)
{
#if !__FINAL
	m_EventType = EVENT_MELEE_ACTION;
#endif // !__FINAL
}

CEventMeleeAction::~CEventMeleeAction()
{
}

bool CEventMeleeAction::AffectsPedCore(CPed* pPed) const
{
	if(!m_pActingEntity)
	{
		return false;
	}

	CPedGroup* pThisPedsGroup = pPed->GetPedsGroup();
	const bool bRespondToMeleeAttackOnLeader =
		pThisPedsGroup && pThisPedsGroup->GetGroupMembership()->GetLeader() &&
		pThisPedsGroup->GetGroupMembership()->GetLeader() == m_pHitEntity &&
		pThisPedsGroup->GetGroupMembership()->GetLeader()->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

	if(!bRespondToMeleeAttackOnLeader && m_pActingEntity->GetType() == ENTITY_TYPE_PED)
	{
		if(CPedGroups::AreInSameGroup(pPed, static_cast<CPed*>(m_pActingEntity.Get())))
		{
			return false;
		}
	}

	// If there is a valid target and it isn't us, ignore it
	if(m_pHitEntity && m_pHitEntity != pPed && !bRespondToMeleeAttackOnLeader)
	{
		return false;
	}

	// If ped is passenger of vehicle and vehicle has a driver then let the driver work out what to do
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->ContainsPed(pPed) && !pPed->GetMyVehicle()->IsDriver(pPed) && !pPed->GetPedsGroup())
	{
		return false;
	}

	if(pPed->IsDead() || pPed == m_pActingEntity || pPed->IsPlayer())
	{
		return false;
	}
	else if(bRespondToMeleeAttackOnLeader)
	{
		return true;
	}
	else
	{
		//Grab the acting ped.
		const CPed* pActingPed = m_pActingEntity->GetIsTypePed() ? static_cast<const CPed*>(m_pActingEntity.Get()) : NULL;

		//Register the hostile action.
		CWanted::RegisterHostileAction(pActingPed);

		//Ignore melee lockon if the ped is already running the agitated task.
		CTaskAgitated* pTaskAgitated = CTaskAgitated::FindAgitatedTaskForPed(*pPed);
		if (pTaskAgitated)
		{
			return false;
		}

		return true;
	}
}

bool CEventMeleeAction::ShouldCreateResponseTaskForPed(const CPed* pPed) const
{
	CPed* pPuncher = static_cast<CPed*>(m_pActingEntity.Get()); 
	CPed* pTarget  = static_cast<CPed*>(m_pHitEntity.Get());

	if(!pPuncher || !pTarget || !pPed || (pPuncher != pPed && pTarget != pPed && !GetDoesResponseToAttackOnLeader(pPed)))
	{
		return false;
	}

	return true;
}

bool CEventMeleeAction::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed->GetPedType() == PEDTYPE_SWAT)
	{
		return CEvent::CreateResponseTask(pPed, response);
	}

	CPed* pPuncher = static_cast<CPed*>(m_pActingEntity.Get());

	if(GetDoesResponseToAttackOnLeader(pPed))
	{
		// Specific response to leader attack
		aiTask* pCombatTask = rage_new CTaskThreatResponse(pPuncher);
		response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
		pPed->GetPedIntelligence()->RegisterThreat(pPuncher, true);
		return response.HasEventResponse();
	}

	//
	// Perform specific event wanted level response here - will be moved to data driven system
	//

	switch(GetResponseTaskType())
	{
	case CTaskTypes::TASK_COMBAT:
		if(CPed* pSourcePed = GetSourcePed())
		{
			if(pSourcePed->IsPlayer())
			{
				// Up the wanted level of the player
				if(pPed->GetPedType() == PEDTYPE_COP && pSourcePed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN && pSourcePed->GetPlayerWanted()->m_fMultiplier > 0.0f)
				{
					const Vector3 vSourcePed = VEC3V_TO_VECTOR3(pSourcePed->GetTransform().GetPosition());
					pSourcePed->GetPlayerWanted()->SetWantedLevelNoDrop(vSourcePed, WANTED_LEVEL1, WANTED_CHANGE_DELAY_QUICK, WL_FROM_LAW_RESPONSE_EVENT);
					NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPlayerCrime(vSourcePed, CRIME_HIT_COP, WANTED_CHANGE_DELAY_QUICK));
				}
			}
		}
		break;

	default:
		break;
	}
	
	// Base class
	return CEventEditableResponse::CreateResponseTask(pPed, response);
}

CPed* CEventMeleeAction::GetPlayerCriminal() const
{
	if(m_pActingEntity && m_pActingEntity->GetIsTypePed() && static_cast<CPed*>(m_pActingEntity.Get())->IsPlayer())
	{
		return static_cast<CPed*>(m_pActingEntity.Get());
	}
	else
	{
		return NULL;
	}
}

bool CEventMeleeAction::operator==(const fwEvent& rOtherEvent) const
{
	if (rOtherEvent.GetEventType() == GetEventType())
	{
		const CEventMeleeAction& rOtherMeleeActionEvent = static_cast<const CEventMeleeAction&>(rOtherEvent);
		return rOtherMeleeActionEvent.GetSourcePed() == GetSourcePed();
	}
	return false;
}

bool CEventMeleeAction::TakesPriorityOver(const CEvent& otherEvent) const
{
	if(otherEvent.GetEventType() == GetEventType())
	{
		const CEventMeleeAction& otherMeleeActionEvent = static_cast<const CEventMeleeAction&>(otherEvent);
		const bool bIsPlayer = (m_pActingEntity && m_pActingEntity->GetType() == ENTITY_TYPE_PED) ? static_cast<CPed*>(m_pActingEntity.Get())->IsPlayer() : false;		
		const bool bIsOtherPlayer =(otherMeleeActionEvent.m_pActingEntity && otherMeleeActionEvent.m_pActingEntity->GetType() == ENTITY_TYPE_PED) ? static_cast<CPed*>(otherMeleeActionEvent.m_pActingEntity.Get())->IsPlayer() : false;

		if(bIsPlayer && bIsOtherPlayer)
		{
			// Both involve player so otherEvent will do
			return false;
		}
		else if(bIsPlayer)
		{
			// Only this event involves the player. Always respond to the player
			return CEvent::TakesPriorityOver(otherEvent);
		}
		else if(bIsOtherPlayer)
		{
			// Only other event is the player.  Just carry on with other event
			return false;
		}
		else
		{
			// Neither is the player so just carry on with other event
			return false;
		}
	}
	else
	{
		// Events of different type so use the priorities
		return CEvent::TakesPriorityOver(otherEvent);
	}
}

bool CEventMeleeAction::GetDoesResponseToAttackOnLeader(const CPed* pPed) const
{
	CPedGroup* pThisPedsGroup = pPed->GetPedsGroup();
	return 
		pThisPedsGroup && pThisPedsGroup->GetGroupMembership()->GetLeader() &&
		pThisPedsGroup->GetGroupMembership()->GetLeader() == m_pHitEntity &&
		pThisPedsGroup->GetGroupMembership()->GetLeader()->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
}

//////////////////////////////////////////////////////////////////////////
// CEventFriendlyAimedAt
//////////////////////////////////////////////////////////////////////////
CEventFriendlyAimedAt::CEventFriendlyAimedAt(CPed* pAggressorPed)
: CEventGunAimedAt(pAggressorPed)
{
#if !__FINAL
	m_EventType = EVENT_FRIENDLY_AIMED_AT;
#endif // !__FINAL
}

// Used by certain events to make sure duplicate events aren't added
bool CEventFriendlyAimedAt::operator==(const fwEvent& otherEvent) const
{	
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

	if ( other.GetEventType() == EVENT_FRIENDLY_AIMED_AT )
	{
		if ( static_cast<const CEventFriendlyAimedAt*>(&other)->GetSourceEntity() == m_pAggressorPed )
		{
			return true;
		}
	}

	return false;
}

// Core implementation of affects ped
bool CEventFriendlyAimedAt::AffectsPedCore(CPed* pPed) const
{
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableFriendlyGunReactAudio))
	{
		return false;
	}

	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

	if (pPed->IsPlayer() || !m_pAggressorPed || pPed->IsInjured())
	{
		return false;
	}

	//Reject for thrown weapons
	const CPed* pSourcePed = GetSourcePed();
	if (pSourcePed)
	{
		const CPedWeaponManager* pWeaponManager = pSourcePed->GetWeaponManager();
		if (pWeaponManager)
		{
			const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
			if (pWeaponInfo && pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE)
			{
				return false;
			}
		}
	}

	// Ensure that we're friendly with the firing ped
	if ( !(CPedGroups::AreInSameGroup(pPed, m_pAggressorPed.Get()) || pPedIntelligence->IsFriendlyWith(*m_pAggressorPed)) )
	{
		return false;
	}

	u32 iWeaponHash = m_pAggressorPed->GetWeaponManager() ? m_pAggressorPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;
	const CWeaponInfo* pTurretWeaponInfo = m_pAggressorPed->GetWeaponManager() ? m_pAggressorPed->GetWeaponManager()->GetEquippedTurretWeaponInfo() : NULL;
	if(iWeaponHash == 0 && !pTurretWeaponInfo)
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = pTurretWeaponInfo ? pTurretWeaponInfo : CWeaponInfoManager::GetInfo<CWeaponInfo>(iWeaponHash);
	// Don't react to non-gun and non throwable weapons
	if(!pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() && !pWeaponInfo->GetIsProjectile())
	{
		return false;
	}

	CTaskCombat* pCombatTask = static_cast<CTaskCombat *>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pCombatTask)
	{
		return false;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
// CEventFriendlyFireNearMiss
//////////////////////////////////////////////////////////////////////////
CEventFriendlyFireNearMiss::CEventFriendlyFireNearMiss(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, bool bSilent, u32 weaponHash, ENearMissType type)
: CEventGunShot(pFiringEntity, vShotOrigin, vShotTarget, bSilent, weaponHash)
, m_type(type)
{
#if !__FINAL
	m_EventType = EVENT_FRIENDLY_FIRE_NEAR_MISS;
#endif // !__FINAL
}

// Used by certain events to make sure duplicate events aren't added
bool CEventFriendlyFireNearMiss::operator==(const fwEvent& otherEvent) const
{	
	const CEvent& other = static_cast<const CEvent&>(otherEvent);

	if ( other.GetEventType() == EVENT_FRIENDLY_FIRE_NEAR_MISS )
	{
		if ( static_cast<const CEventGunShot*>(&other)->GetSourceEntity() == m_pFiringEntity )
		{
			return true;
		}
	}

	return false;
}

// Core implementation of affects ped
bool CEventFriendlyFireNearMiss::AffectsPedCore(CPed* pPed) const
{
	if(pPed->IsDead())
	{
		return false;
	}
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableFriendlyGunReactAudio))
	{
		return false;
	}
	
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

	CPed* pFiringPed = NULL;
	if ( !(m_pFiringEntity && m_pFiringEntity->GetIsTypePed()) )
	{
		return false;
	}

	pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());
	if(pFiringPed == pPed)
	{
		return false;
	}
			
	// Ensure that we're friendly with the firing ped
	if ( !( CPedGroups::AreInSameGroup(pPed,pFiringPed) || pPedIntelligence->IsFriendlyWith(*pFiringPed) ) )
	{
		return false;
	}

	// Ignore gunshots if they occur in a different interior state than the sensing ped.
	if (CEvent::CanEventBeIgnoredBecauseOfInteriorStatus(*pPed, GetSourceEntity(), GetEntityOrSourcePosition()))
	{
		return false;
	}

	//Note: This check was added so that buddies would react with audio if we shot around randomly nearby them.
	//		We should probably remove this check, and solve this problem differently when there is time.
	{
		//Check if the ped is not in combat.
		if(pFiringPed && !pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
		{
			//Check if the ped is close.
			ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pFiringPed->GetTransform().GetPosition());
			static dev_float s_fMaxDistance = 7.0f;
			ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
			if(IsLessThanAll(scDistSq, scMaxDistSq))
			{
				return true;
			}
		}
	}

	if(!IsBulletInRange(*pPed))
	{
		return false;
	}

	//Grab the threat response task.
	CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
		pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
	if(pTaskThreatResponse)
	{
		pTaskThreatResponse->OnFriendlyFireNearMiss(*this);
	}

	return true;
}

// Checks to see if the bullet was close enough.
bool CEventFriendlyFireNearMiss::IsBulletInRange(const CPed& rPed) const
{
	Vector3 vTestPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());

	if(m_type == FNM_BULLET_WHIZZED_BY)
	{
		Vec3V vClosestPosition;
		fwGeom::fwLine pathLine;
		pathLine.m_start = m_vShotOrigin;
		pathLine.m_end = m_vShotTarget;

		// Find the closest point on a line to the bullet's impact.
		pathLine.FindClosestPointOnLineV(RCC_VEC3V(vTestPosition), vClosestPosition);

		// Check if that is within a certain distance.
		if (IsLessThanAll(DistSquared(vClosestPosition, RCC_VEC3V(vTestPosition)), ScalarV(CEventGunShot::sm_Tunables.m_GunShotThresholdDistance * CEventGunShot::sm_Tunables.m_GunShotThresholdDistance)))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else if(m_type == FNM_BULLET_IMPACT)
	{
		float fTestDistance = rPed.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatBulletImpactDetectionRange);

		CPed* pFiringPed = NULL;
		if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed())
		{
			pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());

			// CC: Need to think about this more maybe
			// E.g. the case where a guard is outside the door and the player shoots a dead body this is ok, 
			// but if the guard doesn't notice a shot down a corridor against a wall, seems a bit crappy,
			// though this might not be that apparently considering they probably see or hear the player before then.
			if (m_bSilent)
			{	
				// If I am, or the guy firing is indoors, ignore
				if (rPed.GetIsInInterior() || pFiringPed->GetIsInInterior())
				{
					return false;
				}

				fTestDistance = rage::Min(fTestDistance, CMiniMap::sm_Tunables.Sonar.fSoundRange_SilencedGunshot);
			}
		}

		// Test to see how close the bullet impacts
		Vector3 w = m_vShotTarget - vTestPosition;
		float fPedToImpactSq = w.Mag2();
		return (fPedToImpactSq < rage::square(fTestDistance));
	}

	return false;
}
