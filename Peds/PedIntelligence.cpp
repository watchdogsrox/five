// Rage headers
#include "Profile/timebars.h"
#include "crskeleton/Skeleton.h"
#include "crskeleton/SkeletonData.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "Animation/AnimBones.h"
#include "animation/FacialData.h"
#include "Camera/CamInterface.h"
#include "Camera/gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "Game/ModelIndices.h"
#include "Game/Dispatch/DispatchHelpers.h"
#include "Game/Dispatch/Orders.h"
#include "Event/Decision/EventDecisionMakerManager.h"
#include "event/Events.h"
#include "Event/ShockingEvents.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Objects/Door.h"
#include "Objects/Object.h"
#include "PedGroup/PedGroup.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedTaskRecord.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/Ped.h"
#include "Peds/QueriableInterface.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "script/script_channel.h"
#include "script/thread.h"
#include "scene/world/GameWorld.h"
#include "Stats/StatsInterface.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskCombatMounted.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskShockingEvents.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/System/TaskTreeClone.h"
#include "Task/System/TaskTreePed.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Heli.h"
#include "Weapons/AirDefence.h"

#include "Task/Default/TaskCuffed.h"

// grrr. This macro conflicts under visual studio.
#if defined (MSVC_COMPILER)
#ifdef max
#undef max
#endif //max
#endif //MSVC_COMPILER

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

/////////////////////
//PED INTELLIGENCE
/////////////////////


bool CPedIntelligence::ms_bPedsRespondToObjectCollisionEvents = true;

const u32 CPedIntelligence::ms_iMinFreqToAcceptBuildingCollisionEvents = 2000;
bool CPedIntelligence::ms_bAllowTeleportingWhenRoutesTimeOut = true;
const u32 CPedIntelligence::ms_iLastHeardGunFireTolerance = 800;
const u32 CPedIntelligence::ms_iLastPinnedDownTolerance = 175;
const u16 CPedIntelligence::ms_MinFramesPerRagdollBoundUpdate = 10;

#if !__FINAL
bool CPedIntelligence::ms_bWarnAboutRouteTaskBuildingCollisions = false;
#endif

PF_PAGE(GTA_PedAI, "GTA Ped AI");
PF_GROUP(GTA_AI);
PF_LINK(GTA_PedAI, GTA_AI);
PF_TIMER(AI_Process, GTA_AI);
PF_TIMER(AI_Process_NearbyEntityLists, GTA_AI);
PF_TIMER(AI_ProcessUpdateTargetting, GTA_AI);
PF_TIMER(AI_ProcessPhysics, GTA_AI);
PF_TIMER(AI_ProcessPostPhysics, GTA_AI);
PF_TIMER(AI_Process_Tasks, GTA_AI);
PF_TIMER(AI_ProcessPostMovement, GTA_AI);
PF_TIMER(AI_ProcessPostCamera, GTA_AI);
PF_TIMER(AI_ProcessPreRender2, GTA_AI);
PF_TIMER(AI_ProcessPostPreRender, GTA_AI);
PF_TIMER(AI_ProcessPostPreRenderAfterAttachments, GTA_AI);
PF_TIMER(AI_BuildQueriableState, GTA_AI);
PF_TIMER(AI_ScanForEvents, GTA_AI);

static const float s_fDefaultDriverRacingModifier = 0.25f;

CPedIntelligence::CPedIntelligence(CPed* pPed)
: m_pPed(pPed),
m_pedStealth(),
m_fTimeSinceLastAiUpdate(0.0f),
m_taskManager(pPed, PED_TASK_TREE_MAX),
m_eventGroup(pPed),
m_eventHandler(pPed),
m_pedPerception(pPed),
m_uMaxNumFriendsToInform(MAX_NUM_FRIENDS_TO_INFORM),
m_fMaxInformFriendDistance(MAX_INFORM_FRIEND_DISTANCE),
m_iHighestPriorityEventType(EVENT_NONE),
m_iHighestPriorityEventPriority(0),
m_vehicleScanner(CExpensiveProcess::PPT_VehicleScanner),
m_doorScanner(CExpensiveProcess::PPT_DoorScanner),
m_pedScanner(CExpensiveProcess::PPT_PedScanner),    
m_objectScanner(CExpensiveProcess::PPT_ObjectCollisionScanner),
m_audioDistributer(CExpensiveProcess::PPT_AudioDistributer),
m_climbDetector(*pPed),
m_dropDownDetector(*pPed),
m_pPedTargetting( NULL ),
m_pCoverFinder( NULL ),
m_pDefaultScoringFn( CPedTargetting::DefaultTargetScoring ),
m_uLastAimedAtByPlayerTime(0),
m_iLastClimbTime(fwTimer::GetTimeInMilliseconds()),
m_fClimbStrength(1.0f),
m_fClimbStrengthRate(0.0f),
m_vLastAttemptedClimbPosition(Vector3(9999.0f,9999.0f,9999.0f)),
m_iLastAttemptedClimbTime(0),
m_iEnclosedSearchVolumeCheckIndex(0),
m_vFailedClimb(Vector3(9999.0f,9999.0f,9999.0f)),
m_vLastMainNavMeshPoly(VEC3_ZERO),
m_iNumTimesClimbFailed(0),
m_vCrossRoadStartPos(VEC3_ZERO),
m_vCrossRoadEndPos(VEC3_ZERO),
m_vChargeGoalPosOverride(V_ZERO),
m_uLastFinishedCrossRoadTimeMS(0),
m_fTimeTilNextBulletReaction(0.0f),
m_fPinnedDown(0.0f),
m_bForcePinned(false),
m_bWillForceScanOnSwitchToHighPhysLod(true),
m_fForcedPinnedTimer(0.0f),
m_iIgnoreLowPriShockingEventsCount(0),
m_uNumPedsFiringAt(0),
m_uRelationshipGroupIndex(0),
m_iPedAlertness(0),
m_iLastReactedToDeadPedTime(0),
m_iLastHeardGunFireTime(0),
m_iLastSeenGunFireTime(0),
m_iLastPinnedDownTime(0),
m_iLastFiringVariationTime(0),
m_iLastCombatRollTime(0),
m_iLastGetUpTime(0),
m_iLastAimGrenadeThrowTime(0),
m_uLastTimeHonkedAt(0),
m_uLastTimeHonkAgitatedUs(0),
m_uLastTimeRevvedAt(0),
m_uLastTimeRevAgitatedUs(0),
m_uLastTimeWeWereKnockedToTheGround(0),
m_uLastTimeWeWereRammedInVehicle(0),
m_uLastTimeShot(0),
m_uLastTimeOurPositionWasShouted(0),
m_uLastTimeBumpedWhenStill(0),
m_uLastTimeEvaded(0),
m_uLastTimeCollidedWithPlayer(0),
m_uLastTimeTriedToSayAudioForDamage(0),
m_uLastTimeCalledPolice(0),
m_uLastTimeLeftVehicle(0),
m_uLastTimeStartedToEnterVehicle(0),
m_uTintIndexForParachute(-1),
m_uTintIndexForReserveParachute(-1),
m_uTintIndexForParachutePack(0),
m_alertState(AS_NotAlert),
m_pCombatDirector(NULL),
m_pRelationshipGroupDefault(CRelationshipManager::s_pCivmaleGroup),
m_fTimeUntilNextCopVariableUpdate(0.0f),
m_fDriverAbilityOverride(-1.0f),
m_fDriverAggressivenessOverride(-1.0f),
m_fDriverRacingModifier(s_fDefaultDriverRacingModifier),
m_fBattleAwareness(0.0f),
m_nLastBattleEventTime(0),
m_nLastBattleEventTimeLocalPlayer(0),
m_nLastTimeDeadPedSeen(0),
m_bBattleAwarenessForcingRun(false),
m_bHadAmbientFriend(false),
m_pLastEntityThatKnockedUsToTheGround(NULL),
m_pLastPedThatRammedUsInVehicle(NULL),
m_pLastUsedScenarioPoint(NULL),
m_iLastUsedScenarioPointType(-1),
m_uLastUsedScenarioFlags(0),
m_pLastEntityEvaded(NULL),
m_pFriendlyException(NULL),
m_pAchievedCombatVictoryOver(NULL),
m_pAmbientFriend(NULL),
m_pLastVehiclePedInside(NULL),
m_ControlPassingFailTime(0)
{
	m_audioDistributer.RegisterSlot();

#if __DEV
	for( s32 i = 0; i < MAX_DEBUG_SCRIPT_TASK_HISTORY; i++ )
	{
		m_aScriptHistoryName[i] = NULL;
		m_aScriptHistoryTaskName[i] = NULL;
		m_aScriptHistoryTime[i] = 0;
		m_szScriptName[i][0] = '\0';
		m_aProgramCounter[i] = 0;
	}

	m_iCurrentHistroyTop = 0;
	m_bNewTaskThisFrame = false;
	m_bAssertIfRouteNotFound = false;
#endif
}

CPedIntelligence::~CPedIntelligence()
{
	// If the targeting system is still active, delete it now
	ShutdownTargetting();

	if(m_pCoverFinder)
	{
		delete m_pCoverFinder;
		m_pCoverFinder = NULL;
	}

	if (m_pOrder)
	{
		m_pOrder->ClearAssignedToEntity();
	}
}

void CPedIntelligence::SetDecisionMakerId(u32 uDecisionMakerId)
{
	if(aiVerifyf(CEventDecisionMakerManager::GetIsDecisionMakerValid(uDecisionMakerId), "Decision maker with Id [%d] does not exist", uDecisionMakerId))
	{
		m_PedDecisionMaker.SetDecisionMakerId(uDecisionMakerId);
	}
}

bool CPedIntelligence::FindRespectedFriendInInformRange()
{	
	bool bFoundFriendInRange=false;

	u32 i=0;
	const CEntityScannerIterator pedList = GetNearbyPeds();
	const CEntity* pEntity = pedList.GetFirst();
	const ScalarV svMaxInformDistanceSquared = ScalarVFromF32(m_fMaxInformFriendDistance*m_fMaxInformFriendDistance);
	while(pEntity && i < m_uMaxNumFriendsToInform)
	{
		const CPed* pNextPed=static_cast<const CPed*>(pEntity);
		i++;

		const CRelationshipGroup* pRelGroup = m_pPed->GetPedIntelligence()->GetRelationshipGroup();
		//if(nRespected & CPedType::GetPedFlag(pNextPed->GetPedType()))
		if( pRelGroup && pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_RESPECT, pNextPed->GetPedIntelligence()->GetRelationshipGroupIndex() ) )
		{
			if(IsLessThanAll(DistSquared(m_pPed->GetTransform().GetPosition(), pNextPed->GetTransform().GetPosition()), svMaxInformDistanceSquared))
			{
				bFoundFriendInRange=true;
				break;
			}
		}
	}

	return bFoundFriendInRange;
}

CEntityScannerIterator CPedIntelligence::GetNearbyObjects()
{
	if(!m_pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
	{
		if(m_objectScanner.GetTimer().Tick())
		{
			m_objectScanner.GetTimer().SetCount(0);
			m_objectScanner.ScanForEntitiesInRange(*m_pPed);
		}
	}
	return m_objectScanner.GetIterator();
}

CObject* CPedIntelligence::GetClosestObjectInRange()
{
	if(!m_pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
	{
		if(m_objectScanner.GetTimer().Tick())
		{
			m_objectScanner.GetTimer().SetCount(0);
			m_objectScanner.ScanForEntitiesInRange(*m_pPed);
		}
		return m_objectScanner.GetClosestObjectInRange();
	}
	return NULL;
}

CEntityScannerIterator CPedIntelligence::GetNearbyVehicles()
{
	if(!m_pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
	{
		if(m_vehicleScanner.GetTimer().Tick())
		{
			m_vehicleScanner.GetTimer().SetCount(0);
			m_vehicleScanner.ScanForEntitiesInRange(*m_pPed);
		}
	}
	return m_vehicleScanner.GetIterator();
}

CVehicle* CPedIntelligence::GetClosestVehicleInRange(bool bForce /*= false*/)
{
	if(!m_pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning) || bForce)
	{
  	if (m_vehicleScanner.GetTimer().Tick() || bForce)
		{
			m_vehicleScanner.GetTimer().SetCount(0);
			m_vehicleScanner.ScanForEntitiesInRange(*m_pPed, bForce);
		}
		return m_vehicleScanner.GetClosestVehicleInRange();
	}
	return NULL;
}

void CPedIntelligence::ClearScanners()
{
	m_vehicleScanner.Clear();    
	m_pedScanner.Clear();
	m_objectScanner.Clear(); 
	m_doorScanner.Clear();
}

bool CPedIntelligence::IsRespondingToEvent(const int iEventType) const
{
	return GetEventHandler()->IsRespondingToEvent(iEventType);
}

// returns true if the ped's decision maker has a response to this event type (defined in EventData.h)
bool CPedIntelligence::HasResponseToEvent(int iEventType) const
{
	return GetPedDecisionMaker().HandlesEventOfType(static_cast<eEventType>(iEventType));
}

//-------------------------------------------------------------------------
// Determine if we can attack a specific ped
//-------------------------------------------------------------------------
bool CPedIntelligence::CanAttackPed(const CPed* pPed)
{
	if( !pPed )
	{
		return false;
	}

	// Don't attack any targets that have been marked as invalid targets
	if( pPed->GetPedResetFlag(CPED_RESET_FLAG_CannotBeTargeted) )
	{
		return false;
	}

	if(!pPed->IsAPlayerPed())
	{
		CVehicle* pTargetVehicle = pPed->GetVehiclePedInside();
		if( pTargetVehicle && pTargetVehicle->InheritsFromHeli() &&
			GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableAimAtAITargetsInHelis) )
		{
			return false;
		}
	}

	bool bOnlyAttackLawIfPlayerWanted = (pPed->IsLawEnforcementPed() && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyAttackLawIfPlayerIsWanted));
	if( bOnlyAttackLawIfPlayerWanted ||
		(m_pPed->IsLawEnforcementPed() && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_LawWillOnlyAttackIfPlayerIsWanted) || m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontAttackPlayerWithoutWantedLevel))) )
	{
		CWanted* pTargetWantedLevel = pPed->GetPlayerWanted();
		if(pTargetWantedLevel)
		{
			if( pTargetWantedLevel->GetWantedLevel() == WANTED_CLEAN ||
				(bOnlyAttackLawIfPlayerWanted && pTargetWantedLevel->CopsAreSearching()) )
			{
				return false;
			}
		}
	}

	return true;
}

//-------------------------------------------------------------------------
// Shuts down the peds targeting if it is active
//-------------------------------------------------------------------------
void CPedIntelligence::ShutdownTargetting( void )
{
	if( m_pPedTargetting )
	{
		delete m_pPedTargetting;
		m_pPedTargetting = NULL;
	}
}


//-------------------------------------------------------------------------
// Updates the peds targetting system if active
//-------------------------------------------------------------------------
bool CPedIntelligence::UpdateTargetting(bool bDoFullUpdate)
{
	PF_FUNC(AI_ProcessUpdateTargetting);

	if( m_pPedTargetting )
	{
		// Updates the peds targetting system
		// if it returns false, it is inactive and ready for 
		// destruction
		Assert(CPedTargetting::GetPool()->IsValidPtr(m_pPedTargetting));
		if( !m_pPedTargetting->UpdateTargetting(bDoFullUpdate) )
		{
			delete m_pPedTargetting;
			m_pPedTargetting = NULL;
		}
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
// returns the best target, activating the targetting system if it is disabled
//-------------------------------------------------------------------------
CPed* CPedIntelligence::GetBestTarget(void)
{
	// Is this peds targetting system active,
	// If not create now
	if( !m_pPedTargetting )
	{
		m_pPedTargetting = rage_checked_pool_new(CPedTargetting) (m_pPed);
	}

	// Pool must be full return null, this forces the ped to stick with 
	// their default target
	if( !m_pPedTargetting )
	{	
		return NULL;
	}

	return m_pPedTargetting->GetBestTarget();
}


//-------------------------------------------------------------------------
// Returns a pointer to the current target if targetting is active
//-------------------------------------------------------------------------
CPed* CPedIntelligence::GetCurrentTarget(void)
{
	// Is the targetting system active,
	if( !m_pPedTargetting )
	{
		return NULL;
	}

	return (CPed*)m_pPedTargetting->GetCurrentTarget();
}


//-------------------------------------------------------------------------
// Adds a specific threat into the targetting system
//-------------------------------------------------------------------------
void CPedIntelligence::RegisterThreat(const CPed* pThreat, bool bTargetSeen )
{
	if( m_pPed == pThreat )
	{
		return;
	}

	// Is this peds targetting system active,
	// If not create now
	if( !m_pPedTargetting )
	{
		m_pPedTargetting = rage_checked_pool_new(CPedTargetting) (m_pPed);
	}

	// Pool must be full, return null, this forces the ped to stick with 
	// their default target
	if( !m_pPedTargetting )
	{	
		return;
	}

	m_pPedTargetting->RegisterThreat( (const CEntity*)pThreat, bTargetSeen );

	//Assert( !IsFriendlyWith(*pThreat ) );
}

//-------------------------------------------------------------------------
// Sets this peds default target scoring function
//-------------------------------------------------------------------------
void CPedIntelligence::SetDefaultTargetScoringFunction( CPedTargetting::TargetScoringFunction* pScoringFn )
{
	// If the scoring function is currently set to the default, 
	// replace it with the new default
	if( m_pPedTargetting )
	{
		if( m_pPedTargetting->GetScoringFunction() == m_pDefaultScoringFn )
		{
			m_pDefaultScoringFn = pScoringFn;
			m_pPedTargetting->ResetScoringFunction();
		}
	}
	else
	{
		m_pDefaultScoringFn = pScoringFn;
	}
}

//-------------------------------------------------------------------------
// Calculates how long it's been since we have been aimed at by a player
//-------------------------------------------------------------------------
float CPedIntelligence::GetTimeSinceLastAimedAtByPlayer() const
{
	if(m_uLastAimedAtByPlayerTime > 0)
	{
		u32 timeSinceLastAimedAt = 0;
		u32 currTime = fwTimer::GetTimeInMilliseconds();

		// handle the time wrapping
		if (currTime < m_uLastAimedAtByPlayerTime)
		{
			timeSinceLastAimedAt = (MAX_UINT32 - m_uLastAimedAtByPlayerTime) + currTime;
		}
		else
		{
			timeSinceLastAimedAt = currTime - m_uLastAimedAtByPlayerTime;
		}

		return ((float) timeSinceLastAimedAt / 1000.0f);
	}

	return LARGE_FLOAT;
}

void CPedIntelligence::UpdateEnclosedSearchRegions(bool)
{
	bool inside = false;
	if ( m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UpdateEnclosedSearchRegion) )
	{
		static int s_NumPerFrame = 5;
		int iNumPerFrame = Min(s_NumPerFrame, CDispatchHelperVolumes::GetTunables().m_EnclosedSearchRegions.m_Areas.GetCount());
		int index = m_iEnclosedSearchVolumeCheckIndex;
		int end = ( m_iEnclosedSearchVolumeCheckIndex + iNumPerFrame ) % CDispatchHelperVolumes::GetTunables().m_EnclosedSearchRegions.m_Areas.GetCount();

		m_iEnclosedSearchVolumeCheckIndex = end;
		while( index != end )
		{
			if ( CDispatchHelperVolumes::GetTunables().m_EnclosedSearchRegions.m_Areas[index].m_Area.IsPointInArea(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition())) )
			{
				// start here next time
				m_iEnclosedSearchVolumeCheckIndex = index;
				inside = true;
				break;
			}
			index = ( index + 1 ) % CDispatchHelperVolumes::GetTunables().m_EnclosedSearchRegions.m_Areas.GetCount();
		}
	}
	m_pPed->SetPedResetFlag(CPED_RESET_FLAG_InsideEnclosedSearchRegion, inside);

}

//-------------------------------------------------------------------------
// Updates the cover finder (if there is one)
//-------------------------------------------------------------------------
void CPedIntelligence::UpdateCoverFinder(bool bDoFullUpdate)
{
	if(m_pCoverFinder)
	{
		// If the cover finder is no longer being used then we need to remove it
		if(!m_pCoverFinder->IsReferenced() || m_pPed->IsInjured())
		{
			delete m_pCoverFinder;
			m_pCoverFinder = NULL;
		}
		else if(m_pCoverFinder->IsActive())
		{
			// If we don't do a full update we still need to validate our cover points
			if(!bDoFullUpdate)
			{
				m_pCoverFinder->ValidateCoverPoints();
				return;
			}

			// If the player has aimed at us recently we should force an update
			static float fMaxTimeForceCoverFinderUpdate = 1.0f;
			if(GetTimeSinceLastAimedAtByPlayer() < fMaxTimeForceCoverFinderUpdate)
			{
				m_pCoverFinder->SetForceUpdateThisFrame();
			}

			// Is our ped visible in some view port?
			if(m_pPed->GetIsVisibleInSomeViewportThisFrame())
			{
				// Grab our local player
				CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
				if(pLocalPlayer)
				{
					// Go through the player's nearby peds
					CEntityScannerIterator entityList = pLocalPlayer->GetPedIntelligence()->GetNearbyPeds();
					for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
					{
						CPed* pNearbyPed = static_cast<CPed*>(pEntity);
						if(pNearbyPed)
						{
							// If we hit our ped then it means we are the closest visible ped so force an update
							if(pNearbyPed == m_pPed)
							{
								m_pCoverFinder->SetForceUpdateThisFrame();
								break;
							}
							else if(pNearbyPed->GetIsVisibleInSomeViewportThisFrame())
							{
								// If we hit a visible ped before we hit ourselves it means we aren't closest so don't force an update
								break;
							}
						}
					}
				}
			}

			// Update our cover finder (internally figures out if an update is needed)
			m_pCoverFinder->Update();
		}
		else if(m_pCoverFinder->GetLastSearchResult(false) != CCoverFinderFSM::SEARCH_INACTIVE)
		{
			UpdateDefensiveAreaFromCoverFinderResult(m_pCoverFinder->GetLastSearchResult(true));
		}
	}
}

//-------------------------------------------------------------------------
// Gets the cover finder, wakes it up if needed 
//-------------------------------------------------------------------------
CCoverFinder* CPedIntelligence::GetCoverFinder(void)
{
	if(!m_pCoverFinder)
	{
		// Create our cover finder (this is pooled which is why it's a pointer)
		if(CCoverFinder::GetPool()->GetNoOfFreeSpaces() > 0)
		{
			m_pCoverFinder = rage_new CCoverFinder();
		}

		// Assert on our cover finder, this should tell us we need to bump up the pool size
		Assertf(m_pCoverFinder, "Cover finder pool ran out of spaces. Increase the value of CCoverFinder in gameconfig.xml");
	}

	return m_pCoverFinder;
}

//-------------------------------------------------------------------------
// Given a cover finding result it will update the defensive areas we are using currently and want to use for cover
//-------------------------------------------------------------------------
void CPedIntelligence::UpdateDefensiveAreaFromCoverFinderResult( s32 iCoverSearchResult )
{
	if(iCoverSearchResult == CCoverFinderFSM::SEARCH_SUCCEEDED)
	{
		if(m_defensiveAreaManager.GetCurrentDefensiveArea() != m_defensiveAreaManager.GetDefensiveAreaForCoverSearch())
		{
			m_defensiveAreaManager.SetCurrentDefensiveArea(m_defensiveAreaManager.GetDefensiveAreaForCoverSearch());
		}
	}
	else if(iCoverSearchResult == CCoverFinderFSM::SEARCH_FAILED)
	{
		// If we have a valid cover point then this would be an alternate cover search so don't swap our defensive areas
		if(m_pPed->GetCoverPoint())
		{
			// If we're standing at our cover point but not using it then it means it's not safe and we should try looking in the other defensive area
			if( GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) )
			{
				return;
			}
		}

		m_defensiveAreaManager.SwapDefensiveAreaForCoverSearch();
	}
}

CTask* CPedIntelligence::FindTaskByType(const int iType) const
{
	CTask* p=0;
	p=FindTaskDefaultByType(iType);
	if(p)
	{
		return p;
	}

	p=FindTaskPrimaryByType(iType);
	if(p)
	{
		return p;
	}

	p=FindTaskEventResponseByType(iType);
	if(p)
	{
		return p;
	}

	p=FindTaskPhysicalResponseByType(iType);
	if(p)
	{
		return p;
	}

	p=FindMovementTaskByType(iType);
	if(p)
	{
		return p;
	}

	return 0;
}

//-------------------------------------------------------------------------
// Returns a pointer to any movement task present of the given type
//-------------------------------------------------------------------------
CTask* CPedIntelligence::FindMovementTaskByType(const int iType) const
{
	aiTask* p=0;
	p=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, iType, PED_TASK_MOVEMENT_EVENT_RESPONSE);
	if(p)
	{
		return static_cast<CTask*>(p);
	}

	p=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, iType, PED_TASK_MOVEMENT_GENERAL);
	if(p)
	{
		return static_cast<CTask*>(p);
	}

	p=m_taskManager.FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, iType, PED_TASK_MOVEMENT_DEFAULT);
	if(p)
	{
		return static_cast<CTask*>(p);
	}

	return 0;
}

#if __ASSERT
bool CPedIntelligence::IsRunningRagdollTask()
{
	bool bHandlesRagdoll = false;
	bHandlesRagdoll = m_taskManager.HandlesRagdoll(PED_TASK_TREE_PRIMARY, m_pPed);

	if (!bHandlesRagdoll)
		bHandlesRagdoll = m_taskManager.HandlesRagdoll(PED_TASK_TREE_MOTION, m_pPed);

	return bHandlesRagdoll;
}
#endif //__ASSERT

CTaskCombat* CPedIntelligence::GetTaskCombat() const
{
	return static_cast<CTaskCombat*>(m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_COMBAT));
}

CTaskGun* CPedIntelligence::GetTaskGun() const
{
	return static_cast<CTaskGun*>(m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_GUN));
}

CTaskCombat* CPedIntelligence::FindTaskCombat() const
{
	return static_cast<CTaskCombat*>(FindTaskByType(CTaskTypes::TASK_COMBAT));
}

CTaskGun* CPedIntelligence::FindTaskGun() const
{
	return static_cast<CTaskGun*>(FindTaskByType(CTaskTypes::TASK_GUN));
}

CTaskWrithe* CPedIntelligence::GetTaskWrithe() const
{
	return static_cast<CTaskWrithe*>(m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_WRITHE));
}

CTaskMelee* CPedIntelligence::GetTaskMelee() const
{
	return static_cast<CTaskMelee*>(m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_MELEE));
}

CTaskMeleeActionResult* CPedIntelligence::GetTaskMeleeActionResult() const
{
	return static_cast<CTaskMeleeActionResult*>(m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_MELEE_ACTION_RESULT));
}

CTaskSmartFlee* CPedIntelligence::GetTaskSmartFlee() const
{
	return static_cast<CTaskSmartFlee*>(m_taskManager.FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_SMART_FLEE));
}

CTaskSmartFlee* CPedIntelligence::FindTaskSmartFlee() const
{
	return static_cast<CTaskSmartFlee*>(FindTaskByType(CTaskTypes::TASK_SMART_FLEE));
}

CTaskSimpleMoveSwim *CPedIntelligence::GetTaskSwim() const
{
	//CDE - Swimming is now handled by a custom move blender, not a task.
	return NULL;
}

bool CPedIntelligence::IsPedSwimming() const
{
	//CDE - we are no longer using a specific task for swimming, just a different move blender.
	// - TODO: Ensure that this still works remotely by virtue of the isDrowning flag being synced.
	return (m_pPed->GetIsSwimming());
}

bool CPedIntelligence::IsPedInAir() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping );
}

bool CPedIntelligence::IsPedClimbing() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsClimbing );
}

bool CPedIntelligence::IsPedVaulting() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsVaulting );
}

bool CPedIntelligence::IsPedFalling() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsFalling );
}

bool CPedIntelligence::IsPedJumping() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping );
}

bool CPedIntelligence::IsPedLanding() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsLanding);
}

bool CPedIntelligence::IsPedDiving() const
{
	return m_pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDiving );
}

bool CPedIntelligence::IsPedGettingUp() const
{	
	if( GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_BLEND_FROM_NM) || 
		GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_OVER) || 
		GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP) || 
		GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_AND_GET_UP) || 
		GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP_AND_STAND_STILL))
	{
		return true;
	}
	else if (FindTaskActiveMotionByType(CTaskTypes::TASK_GET_UP))
	{
		return true;
	}
	
	return false;
}

//dev_bool TREAT_SLIDING_INTO_COVER_AS_IN_COVER = true;
float CPedIntelligence::DISTANCE_TO_START_COVER_CAM_WHEN_SLIDING = 1.0f;
bool CPedIntelligence::GetPedCoverStatus( s32& iCoverState, bool& bCanFire ) const
{
	iCoverState = -1;
	bCanFire = false;
	if( GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER) )
	{
		CTaskInCover* pCoverTask = (CTaskInCover*)FindTaskActiveByType(CTaskTypes::TASK_IN_COVER);
		if( pCoverTask )
		{
			iCoverState = pCoverTask->GetState();
			bCanFire = m_pPed->GetCoverPoint() != NULL;
		}
		return true;
	}
// 	else if( TREAT_SLIDING_INTO_COVER_AS_IN_COVER )
// 	{
// 		// Only count as in cover near to the point for a smooth camera transition.
// 		if( m_pPed->GetCoverPoint() && GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SLIDE_INTO_COVER) )
// 		{
// 			if(  m_pPed->GetCoverPoint()->GetType() != CCoverPoint::COVTYPE_OBJECT )
// 			{
// 				Vector3 vCoverPos;
// 				if( m_pPed->GetCoverPoint()->GetCoverPointPosition(vCoverPos) )
// 				{
// 					if( (VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition())-vCoverPos).XYMag2() < rage::square(DISTANCE_TO_START_COVER_CAM_WHEN_SLIDING) )
// 					{
// 						return true;
// 					}
// 				}
// 			}
// 		}
// 	}
	return false;
}

bool CPedIntelligence::IsPedClimbingLadder() const
{
	if(GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER))
		return true;

	if(GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER))
		return true;

	return false;
}

bool CPedIntelligence::IsPedInMelee() const
{
	if(GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE))
		return true;

	return false;
}

bool CPedIntelligence::IsPedBlindFiring() const
{
	CTaskInCover* pCoverTask = static_cast<CTaskInCover*>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if (pCoverTask && ( pCoverTask->GetState() == CTaskInCover::State_BlindFiring ))
	{
		return true;
	}

	return false;
}

CTaskClimbLadder* CPedIntelligence::GetTaskClimbLadder() const
{
	aiTask* pTask = m_taskManager.GetActiveLeafTask(PED_TASK_TREE_PRIMARY);
	if(pTask
		&& pTask->GetTaskType()==CTaskTypes::TASK_CLIMB_LADDER)
		return (CTaskClimbLadder *)pTask;

	return NULL;
}

void CPedIntelligence::ClearPrimaryTask()
{
	m_taskManager.SetTask(PED_TASK_TREE_PRIMARY, NULL, PED_TASK_PRIORITY_PRIMARY);
}

void CPedIntelligence::ClearSecondaryTask(s32 iType)
{
	m_taskManager.SetTask(PED_TASK_TREE_SECONDARY, NULL, iType);
}

void CPedIntelligence::ClearPrimaryTaskAtPriority(s32 iPriority)
{
	m_taskManager.ClearTask(PED_TASK_TREE_PRIMARY, iPriority);
}

void CPedIntelligence::ClearTasks(const bool bClearMainTask, const bool bClearSecondaryTask)
{
	Assertf(!m_pPed->IsInjured(), "Ped was injured when ClearTasks was called!");
	if(bClearMainTask)
	{
		bool bFailedToAbort = false;

		// Shut down the targeting if active
		ShutdownTargetting();

		// First try and clear the main task tree by aborting all tasks, if this succeeds NULL all trees immediately.
		const bool bInVehicle = m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && (m_pPed->GetMyVehicle());
		CEventGivePedTask makeAbortEvent(PED_TASK_PRIORITY_PRIMARY,  rage_new CTaskDoNothing(0, bInVehicle?1:0));

		int i;
		for(i=0;i<PED_TASK_PRIORITY_MAX;i++)
		{
			aiTask* pTask=m_taskManager.GetTask(PED_TASK_TREE_PRIMARY, i);
			if(pTask && pTask->MakeAbortable( aiTask::ABORT_PRIORITY_URGENT,&makeAbortEvent))
			{
				if( i != PED_TASK_PRIORITY_DEFAULT )
				{
					m_taskManager.ClearTask(PED_TASK_TREE_PRIMARY,i);
				}
			}
			else if( pTask )
			{
				bFailedToAbort = true;
			}
		}

		// The make abortable above failed as some tasks failed to quit, add an event so this can be processed next frame.
		if( bFailedToAbort )
		{
			//	Ends any tasks being performed on the ped and gives him the most basic task to do (stand still or sit in vehicle)
			if ( (m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (m_pPed->GetMyVehicle()) )
			{
				if(!m_eventGroup.HasScriptCommandOfTaskType(CTaskTypes::TASK_IN_VEHICLE_BASIC))
				{
					//Already got a default drive task so no need for a primary drive task.  Just add a task that will
					//finish immediately to ensure all tasks are removed in event handler.
					CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDoNothing(0, 1));
					AddEvent(event);
				}
			}
			else
			{
				if(!m_eventGroup.HasScriptCommandOfTaskType(CTaskTypes::TASK_DO_NOTHING))
				{
					//Clear everything by getting the ped to stand still.
					CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDoNothing(0));
					AddEvent(event);
				}
			}
		}

		//Update the record straight away in case the flush was called during a script process.
		CPedScriptedTaskRecord::Process();
	}

	if(bClearSecondaryTask)
	{
		int i;
		for(i=0;i<PED_TASK_SECONDARY_MAX;i++)
		{
			aiTask* pTask=m_taskManager.GetTask(PED_TASK_TREE_SECONDARY, i);
			if(pTask && pTask->MakeAbortable( aiTask::ABORT_PRIORITY_URGENT,0))
			{
				m_taskManager.SetTask(PED_TASK_TREE_SECONDARY,0,i);
			}
		}
	}
	Assertf(!m_pPed->IsInjured(), "Ped is injured after ClearTasks was called!");
}

void CPedIntelligence::FlushImmediately(const bool bRestartDefaultTasks, const bool bAbortMotionTasks, const bool bAbortRagdoll, const bool bProcessTaskRecord)
{
	// Ensure intelligence update next frame
	m_pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	// Shut down the targetting if active
	ShutdownTargetting();

	// cache the current anim sets here
	CTaskMotionPed::CPersistentData data;

	if (m_pPed->GetPrimaryMotionTask() && m_pPed->GetPrimaryMotionTask()->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
	{
		CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(m_pPed->GetPrimaryMotionTask());
		data.Init(*pTask);
	}

	//Store the facial task if there is one.
	m_eventGroup.FlushAll();
	GetEventHandler()->FlushImmediately();

    if(bAbortMotionTasks)
    {
	    m_taskManager.AbortTasks();
    }
    else
    {
        s32 numTrees = m_taskManager.GetTreeCount();

        for(s32 treeIndex = 0; treeIndex < numTrees; treeIndex++)
        {
            aiTaskTree *taskTree = m_taskManager.GetTree(treeIndex);
            if(taskTree && treeIndex != PED_TASK_TREE_MOTION)
            {
                taskTree->AbortTasks();
            }
        }
    }

	// Ragdoll will normally be cleared up by this point, but sometimes script can clear the ped
	// tasks between physics activating the ragdoll and the task to handle it being run.
	if (bAbortRagdoll && m_pPed->GetUsingRagdoll())
	{
		nmEntityDebugf(m_pPed, "CPedIntelligence::FlushImmediately switching to animated");
		m_pPed->SwitchToAnimated();
	}

	// Make sure the ped is out of the car after a flush immediately!
	u32 iFlags = CPed::PVF_IgnoreSettingJustLeftVehicle;
	if(bRestartDefaultTasks == false)
		iFlags |= CPed::PVF_DontResetDefaultTasks;

	if (m_pPed->IsNetworkClone())
	{
		// if the ped is being removed from the vehicle via a task flush, he needs to be warped out
		iFlags |= CPed::PVF_Warp;

		// a bit of a hacky fix to get clone players to exit planes properly when being warped out (B* 1816633)
		CVehicle* pPedVehicle = m_pPed->GetIsInVehicle() ? m_pPed->GetMyVehicle() : NULL;

		if (pPedVehicle && pPedVehicle->GetIsAircraft() && pPedVehicle->IsInAir())
		{
			iFlags |= CPed::PVF_InheritVehicleVelocity;
			iFlags |= CPed::PVF_NoCollisionUntilClear;
		}
	}

	m_pPed->SetPedOutOfVehicle(iFlags);
	m_pPed->SetPedOffMount();
	//if I'm a ridable ped, clear my rider
	if (m_pPed->GetSeatManager()) {
		for (s32 seatId=0; seatId< m_pPed->GetSeatManager()->GetMaxSeats(); seatId++)
		{
			if (m_pPed->GetSeatManager()->GetPedInSeat(seatId)) 
			{
				m_pPed->GetSeatManager()->GetPedInSeat(seatId)->SetPedOffMount();
			}
		}
	}

	//Update the record straight away in case the flush was called during a script process.
	if(bProcessTaskRecord)
	{
		CPedScriptedTaskRecord::Process();
	}

    if(bAbortMotionTasks)
    {
	    //we always want to restart the primary motion task. If there isn't one we can't process physics on the ped.
	    m_pPed->StartPrimaryMotionTask();

	    // Set the default anim sets back on the ped
	    if ( m_pPed->GetPrimaryMotionTask() && m_pPed->GetPrimaryMotionTask()->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
	    {
		    CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(m_pPed->GetPrimaryMotionTask());
		    data.Apply(*pTask);
	    }

		m_pPed->SetBoundPitch(0);

#if __ASSERT
		fwAttachmentEntityExtension *pPedAttachmentExtension = m_pPed->GetAttachmentExtension();
		if(pPedAttachmentExtension)
		{
			pPedAttachmentExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true); // Necessary to stop spurious assert in fwEntity::SetMatrix().
		}
#endif	// __ASSERT
		m_pPed->SetPitch(0);
#if __ASSERT
		if(pPedAttachmentExtension)
		{
			pPedAttachmentExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
		}
#endif	// __ASSERT
		m_pPed->GetMotionData()->Reset();
    }

	if(bRestartDefaultTasks)
	{

		AddTaskDefault(m_pPed->ComputeDefaultTask(*m_pPed));

        // this can happen in a network game if CNetObjPed::CanSetTask returns false
        Assertf(GetTaskDefault() != 0, "Unable to restart default task for a ped!");
		//		if(m_pPed->IsPlayer())
		//		{
		//			// might be that they should return to ski, so now they return to default player control task for their moveblend class
		//			AddTaskDefault(m_pPed->GetMoveBlender()->CreatePlayerControlTask() );
		//		}
		//		else if(m_pPed->PopTypeIsMission())
		//		{
		//			AddTaskDefault(new CTaskSimpleStandStill(0,true));
		//		}
		//		else
		//		{
		//			AddTaskDefault(m_pPed->ComputeWanderTask(*m_pPed));
		//		}
	}

	if (!m_pPed->IsNetworkClone()) // the queriable state is dictated by the machine which owns the ped
	{
		GetTaskManager()->SetTask(PED_TASK_TREE_MOVEMENT, rage_new CTaskMoveStandStill(), PED_TASK_MOVEMENT_DEFAULT);

		BuildQueriableState();
	}

	m_fDriverAbilityOverride = -1.0f;
	m_fDriverAggressivenessOverride = -1.0f;
	m_fDriverRacingModifier = s_fDefaultDriverRacingModifier;
}

void CPedIntelligence::ClearTasksAbovePriority(const s32 iPriority)
{
	aiTaskTree *taskTree = m_taskManager.GetTree(PED_TASK_TREE_PRIMARY);
	taskTree->AbortTasksAbovePriority(iPriority);
}

void
CPedIntelligence::FlushEvents(void)
{
	m_eventGroup.FlushAll();
	GetEventHandler()->FlushImmediately();
}


void CPedIntelligence::BuildQueriableState()
{
	PF_FUNC(AI_BuildQueriableState);

	aiTaskTree *taskTree = m_taskManager.GetTree(PED_TASK_TREE_PRIMARY);
	if(taskTree->IsCloneTree())
	{
		gnetDebug1("%s skipping BuildQueriableState because of clone tree", m_pPed->GetLogName());
		return;
	}

	Assert(dynamic_cast<CTaskTreePed*>(taskTree));
	CTaskTreePed *taskTreePed = (static_cast<CTaskTreePed*>(taskTree));

	// Write out the current main tasks to the queriable interface
	CQueriableInterface* pQueriableInterface = GetQueriableInterface();
	taskTreePed->WriteTasksToQueriableInterface( pQueriableInterface );

	pQueriableInterface->UpdateCachedInfo();

    NetworkInterface::OnQueriableStateBuilt(*m_pPed);
}

// The default climb strength regeneration rate
dev_float CPedIntelligence::DEFAULT_CLIMB_STRENGTH_RATE = 0.2f;

// How fast the local player must be traveling in a vehicle for peds with a response to encroachment
// to scan every frame.
dev_float CPedIntelligence::ms_fFrequentScanVehicleVelocitySquaredThreshold = 64.0; 

// Alertness threshold
const u8 CPedIntelligence::ALERTNESS_RESPONSE_THRESHOLD = 10;

// Distance to inform params
const u8    CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM = 3u;
const float CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE = 50.0f;

const u8 	CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE = 255;
const int	CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_NUM_BITS = 8; 

const float CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE_MAX_VALUE = 1000.0f;
const s32   CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE_NUM_BITS = 16;

void CPedIntelligence::Process(bool fullUpdate, float fOverrideTimeStep)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

	PF_FUNC(AI_Process);

	// Debug update
	DEV_ONLY(Process_Debug();)

	const float dt = fOverrideTimeStep >= 0.0f ? fOverrideTimeStep : fwTimer::GetTimeStep();
	Assert(dt >= 0.0f);
	const float fTimeslicedTimeStep = m_fTimeSinceLastAiUpdate + dt;
	m_fTimeSinceLastAiUpdate = fTimeslicedTimeStep;

	if(fullUpdate)
	{
		m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_LowPhysicsLodMayPlaceOnNavMesh, false);

#if __BANK
		if(CPedAILodManager::ms_bDisplayFullUpdatesLines || CPedAILodManager::ms_bDisplayFullUpdatesSpheres)
		{
			CPedAILodManager::DrawFullUpdate(*m_pPed);
		}
#endif

		// Update climb scanner
		GetClimbDetector().Process();
	}

	// Maintain lists of nearby entities
	Process_NearbyEntityLists();

	if(fullUpdate)
	{
		// Update any CPedIntelligence variables pre task update
		Process_UpdateVariables(fTimeslicedTimeStep);
	}

	//Compute all events.
	GetEventScanner()->ScanForEvents(*m_pPed, fullUpdate);

	//Update decision maker
	UpdatePedDecisionMaker(fullUpdate);

	// Update the targeting system
	UpdateTargetting(fullUpdate);

	// Update the cover finder
	UpdateCoverFinder(fullUpdate);

	// update enclosed search regions
	UpdateEnclosedSearchRegions(fullUpdate);

	if(fullUpdate)
	{

		//Handle the events by computing event response tasks.
		aiEvent::BeginEventUpdates(fTimeslicedTimeStep);
		GetEventHandler()->HandleEvents();
		aiEvent::EndEventUpdates();

		// Update our defensive area information before task update
		GetDefensiveAreaManager()->UpdateDefensiveAreas();

		// reset our single frame shoot rate modifier
		GetCombatBehaviour().ResetSingleFrameShootRateModifier();
	}

	// Update firing pattern
	bool bUpdateFiringPattern = GetFiringPattern().ShouldProcess(*m_pPed);

	if(bUpdateFiringPattern)
	{
		// Update the firing pattern before the task update
		GetFiringPattern().PreProcess(*m_pPed);
	}

	// Update the AI tasks
	Process_Tasks(fullUpdate, fTimeslicedTimeStep);

	if(fullUpdate)
	{
		// Update any CPedIntelligence variables after task update
		Process_UpdateVariablesAfterTaskProcess(fTimeslicedTimeStep);

		// Build this peds queriable state after the task update
		BuildQueriableState();
	}

	if(bUpdateFiringPattern)
	{
		// Update the firing pattern after the task update
		GetFiringPattern().PostProcess(fwTimer::GetTimeStep());
	}

	if(fullUpdate)
	{
		// Update the peds perception
		m_pedPerception.Update();

		// Update the peds stealth status
		m_pedStealth.Update(fTimeslicedTimeStep);
	}

	// Update the peds motivation
	m_pedMotivation.Update(); 

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Check for some more conditions that should trigger a bounds update
	Process_BoundUpdateRequest();
#endif

	if(fullUpdate)
	{
		m_fTimeSinceLastAiUpdate = 0.0f;
	}
}

bool CPedIntelligence::ProcessPhysics(float fTimeStep, int nTimeSlice, bool nmTasksOnly)
{
	PF_FUNC(AI_ProcessPhysics);

	Assert(m_taskManager.GetTreeCount() == PED_TASK_TREE_MAX);
	CompileTimeAssert(PED_TASK_TREE_MAX == 4);

	bool ret = false;

	// Only call Ped Intelligence Process Physics when necessary.
	CPed* pPed = m_pPed;
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks) || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksTimeSliced))
	{
		ret |= static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_PRIMARY))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);

		// Probably not needed, since we don't run a whole lot of tasks on the secondary tree, and none of them
		// appear to currently be using ProcessPhysics(). If needed, we could either use the same flag, or add a new one.
		//	ret |= static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_SECONDARY))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
	}
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMovement))
	{
		ret |= static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_MOVEMENT))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
	}
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion))
	{
		ret |= static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_MOTION))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
	}

	// The code below may help to track down possible problems with the ProcessPhysics reset flags.
	// For example, if a motion task has a ProcessPhysics() function that potentially did something
	// because some other task set CPED_RESET_FLAG_ProcessPhysicsTasks.
#if 0
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksTimeSliced))
			|| pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMovement)
			|| pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion))
	{
		bool ret1 = static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_PRIMARY))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
		Assert(!ret1 || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks) || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksTimeSliced));
		bool ret1b = static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_SECONDARY))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
		Assert(!ret1b);
		bool ret2 = static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_MOVEMENT))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
		Assert(!ret2 || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMovement));
		bool ret3 = static_cast<CTaskTree*>(m_taskManager.GetTree(PED_TASK_TREE_MOTION))->ProcessPhysics(fTimeStep, nTimeSlice, nmTasksOnly);
		Assert(!ret3 || pPed->GetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasksMotion));

		ret = ret1 | ret1b | ret2 | ret3;
	}
#endif

	return ret;
}

#if __BANK
EXTERN_PARSER_ENUM(ePedResetFlags);
#endif // __BANK

void CPedIntelligence::ProcessPostPhysics()
{
	PF_FUNC(AI_ProcessPostPhysics);

	m_pPed->m_PedResetFlags.ResetPostPhysics(m_pPed);

#if __BANK
	if (CPedDebugVisualiserMenu::ms_bDebugPostPhysicsResetFlagActive)
	{
		CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

		if (!pEntity || !pEntity->GetIsTypePed())
		{
			pEntity = FindPlayerPed();
		}

		if (pEntity == m_pPed)
		{
			for(int i = 0; i < PARSER_ENUM(ePedResetFlags).m_NumEnums; i++)
			{
				if (CPedDebugVisualiserMenu::ms_DebugPedPostPhysicsResetFlagsBitSet.BitSet().IsSet(i))
				{
					m_pPed->SetPedResetFlag((ePedResetFlags)i, true);
				}
			}
		}
	}
#endif // __BANK

	if (m_pPed->GetPlayerInfo())
	{
		m_pPed->GetPlayerInfo()->GetPlayerResetFlags().ResetPostPhysics();
		m_pPed->GetPlayerInfo()->SetVehiclePlayerPreferRearSeat(NULL);
		m_pPed->GetPlayerInfo()->SetVehiclePlayerPreferFrontPassengerSeat(NULL);
	}
}

// needs processed AFTER ProcessCollision loop
void CPedIntelligence::ProcessPostMovement()
{
	PF_FUNC(AI_ProcessPostMovement);

	if (m_pPed->IsLocalPlayer())
	{
		m_pPed->GetPlayerInfo()->ProcessPostMovement();
	}

	// do positioning of peds relative to vehicles here
	if (!m_pPed->GetUsingRagdoll())
	{
		m_taskManager.ProcessPostMovement(PED_TASK_TREE_PRIMARY);
		m_taskManager.ProcessPostMovement(PED_TASK_TREE_MOTION);
	}	
}

// needs processed AFTER camManager::Update
void CPedIntelligence::ProcessPostCamera()
{
	PF_FUNC(AI_ProcessPostCamera);

	m_taskManager.ProcessPostCamera(PED_TASK_TREE_PRIMARY);
	m_taskManager.ProcessPostCamera(PED_TASK_TREE_SECONDARY);
	m_taskManager.ProcessPostCamera(PED_TASK_TREE_MOTION);
}

void CPedIntelligence::ProcessPreRender2()
{
	PF_FUNC(AI_ProcessPreRender2);

	m_taskManager.ProcessPreRender2(PED_TASK_TREE_PRIMARY);
	m_taskManager.ProcessPreRender2(PED_TASK_TREE_SECONDARY);
	m_taskManager.ProcessPreRender2(PED_TASK_TREE_MOTION);
}

// needs processed AFTER PreRender (so ped's matrices have been calculated)
void CPedIntelligence::ProcessPostPreRender()
{
	PF_FUNC(AI_ProcessPostPreRender);

	m_taskManager.ProcessPostPreRender(PED_TASK_TREE_PRIMARY);
	m_taskManager.ProcessPostPreRender(PED_TASK_TREE_MOTION);
}

// needs to be processed AFTER attachments 
void CPedIntelligence::ProcessPostPreRenderAfterAttachments()
{
	PF_FUNC(AI_ProcessPostPreRenderAfterAttachments);

	m_taskManager.ProcessPostPreRenderAfterAttachments(PED_TASK_TREE_PRIMARY);
}

//-------------------------------------------------------------------------
// Sets the ped's default relationship group
//-------------------------------------------------------------------------
void CPedIntelligence::SetRelationshipGroupDefault(CRelationshipGroup* pRelationshipGroup)
{
	aiAssertf(pRelationshipGroup, "The relationship group is invalid.");
	m_pRelationshipGroupDefault = pRelationshipGroup;
}

//-------------------------------------------------------------------------
// Sets the ped's relationship group
//-------------------------------------------------------------------------
void CPedIntelligence::SetRelationshipGroup(CRelationshipGroup* pRelationshipGroup)
{
	if(aiVerifyf(pRelationshipGroup, "The relationship group is invalid."))
	{
		const int index = pRelationshipGroup->GetIndex();
		Assertf(index >= 0 && index < MAX_RELATIONSHIP_GROUPS, "Rel group %s has an index of %d", pRelationshipGroup->GetName().TryGetCStr(), index);
		m_uRelationshipGroupIndex = (u16)index;
	}
	m_pRelationshipGroup = pRelationshipGroup;
}

CRelationshipGroup* CPedIntelligence::GetRelationshipGroup() const
{
	//Check if the relationship group is valid.
	if(aiVerifyf(m_pRelationshipGroup, "The relationship group is invalid."))
	{
		return m_pRelationshipGroup;
	}
	
	//Check if the default relationship group is valid.
	if(aiVerifyf(m_pRelationshipGroupDefault, "The default relationship group is invalid."))
	{
		return m_pRelationshipGroupDefault;
	}
	
	return NULL;
}


int	CPedIntelligence::GetRelationshipGroupIndex() const
{
	Assertf(m_uRelationshipGroupIndex == m_pRelationshipGroup->GetIndex(), "\nCached relationship group index (%d) doesn't match current index (%d) of relationship group %s\n",
			m_uRelationshipGroupIndex, m_pRelationshipGroup->GetIndex(), m_pRelationshipGroup->GetName().TryGetCStr());

	return m_uRelationshipGroupIndex;
}

//-------------------------------------------------------------------------
// Sets the peds default relationship group depending on the ped type
//-------------------------------------------------------------------------
void CPedIntelligence::SetDefaultRelationshipGroup()
{
	// Force player controlled peds groups
	if(m_pPed->IsControlledByLocalOrNetworkPlayer() || CPedType::IsSinglePlayerType(m_pPed->GetPedType()))
	{
		SetRelationshipGroupDefault(CRelationshipManager::s_pPlayerGroup);
		SetRelationshipGroup(CRelationshipManager::s_pPlayerGroup);
	}
	else if(m_pPed->GetPedType() == PEDTYPE_COP || m_pPed->GetPedType() == PEDTYPE_SWAT)
	{
		SetRelationshipGroupDefault(CRelationshipManager::s_pCopGroup);
		SetRelationshipGroup(CRelationshipManager::s_pCopGroup);
	}
	else if(m_pPed->GetPedType() == PEDTYPE_ARMY)
	{
		SetRelationshipGroupDefault(CRelationshipManager::s_pArmyGroup);
		SetRelationshipGroup(CRelationshipManager::s_pArmyGroup);
	}
	else if(m_pPed->PopTypeIsMission())
	{
		SetRelationshipGroupDefault(CRelationshipManager::s_pNoRelationshipGroup);
		SetRelationshipGroup(CRelationshipManager::s_pNoRelationshipGroup);
	}
	else
	{
		bool isRelGroupValid = false;
		if(Verifyf(m_pPed->IsArchetypeSet(), "Peds model index is not valid, falling back to default %s", m_pRelationshipGroupDefault->GetName().GetCStr()))
		{
			CPedModelInfo* pModelInfo = m_pPed->GetPedModelInfo();
			if(Verifyf(pModelInfo, "Peds model info not found, falling back to default relationship %s", m_pRelationshipGroupDefault->GetName().GetCStr()))
			{
				CRelationshipGroup* pRelationshipGroup = CRelationshipManager::FindRelationshipGroup(pModelInfo->GetRelationshipGroupHash());
				if(Verifyf(pRelationshipGroup, "%s Relationship group not found, falling back to default %s", pModelInfo->GetRelationshipGroupHash().GetCStr(), m_pRelationshipGroupDefault->GetName().GetCStr()))
				{
					isRelGroupValid = true;
					SetRelationshipGroupDefault(pRelationshipGroup);
					SetRelationshipGroup(pRelationshipGroup);
				}
			}
		}

		// We'll fall back to the old way of setting the relationship group if the one in the model info is invalid
		if(!isRelGroupValid)
		{
			SetRelationshipGroup(m_pRelationshipGroupDefault);
		}
	}
}

void CPedIntelligence::FixRelationshipGroup()
{
	//Ensure the relationship group is invalid.
	if(m_pRelationshipGroup)
	{
		return;
	}
	
	//Set the default relationship group.
	SetDefaultRelationshipGroup();
}

bool CPedIntelligence::IsFriendlyWithEntity(const CEntity* pEntity, bool bIncludeFriendsDueToScenarios, bool bDebug /*= false*/) const
{
	if (!pEntity)
	{
		return false;
	}

	if (pEntity->GetIsTypePed())
	{
		const CPed* pPed = SafeCast(const CPed, pEntity);
		return IsFriendlyWith(*pPed, bDebug) || (bIncludeFriendsDueToScenarios && IsFriendlyWithDueToScenarios(*pPed));
	}
	else if (pEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = SafeCast(const CVehicle, pEntity);
		return IsFriendlyWithVehicleOccupants(*pVehicle, bIncludeFriendsDueToScenarios, bDebug);
	}

	return false;
}

bool CPedIntelligence::IsFriendlyWithVehicleOccupants(const CVehicle& rVehicle, bool bIncludeFriendsDueToScenarios, bool bDebug /*= false*/) const
{
	const CSeatManager* pSeatManager = rVehicle.GetSeatManager();
	const int iNumSeats  = pSeatManager ? pSeatManager->GetMaxSeats() : 0;

	bool bHasOccupants = false;
	bool bAllOcuppantsFriendly = true;
	for(int iSeat = 0; bAllOcuppantsFriendly && iSeat < iNumSeats; iSeat++)
	{
		CPed *pOccupantPed = CTaskVehicleFSM::GetPedInOrUsingSeat(rVehicle, iSeat);
		if (pOccupantPed)
		{	
			bHasOccupants = true;
			bAllOcuppantsFriendly = IsFriendlyWith(*pOccupantPed, bDebug) || (bIncludeFriendsDueToScenarios && IsFriendlyWithDueToScenarios(*pOccupantPed));
		}
	}

	return bHasOccupants && bAllOcuppantsFriendly;
}

bool CPedIntelligence::IsFriendlyWith(const CPed& otherPed, bool OUTPUT_ONLY(bDebug)) const
{
	Assert(m_pPed->GetPedIntelligence() != NULL);
	Assert(otherPed.GetPedIntelligence() != NULL);

	//Ensure the ped is not the friendly exception.
	if(m_pFriendlyException == &otherPed)
	{
#if !__NO_OUTPUT
		if ((Channel_weapon.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weapon.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			if (bDebug)
			{
				weaponDebugf3("CPedIntelligence::IsFriendlyWith--(m_pFriendlyException == &otherPed)-->return false");
			}
		}
#endif
	
		return false;
	}

	const CRelationshipGroup* pRelGroup = m_pPed->GetPedIntelligence()->GetRelationshipGroup();
	const int otherRelationshipGroupIndex = otherPed.GetPedIntelligence()->GetRelationshipGroupIndex();
	Assert(pRelGroup);
	Assert(otherPed.GetPedIntelligence()->GetRelationshipGroup());
	bool bFriendly = pRelGroup && (
		pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_RESPECT, otherRelationshipGroupIndex ) ||
		pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_LIKE, otherRelationshipGroupIndex )||
		(m_pPed->GetPedsGroup() && (m_pPed->GetPedsGroup() == otherPed.GetPedsGroup())));

	// Now check against ped types assuming neither ped has been set to ignore ped types for is friendly with checks
	if(!bFriendly && !m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnorePedTypeForIsFriendlyWith) && !otherPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IgnorePedTypeForIsFriendlyWith))
	{
		bFriendly = (m_pPed->GetPedType() == PEDTYPE_MEDIC && otherPed.GetPedType() == PEDTYPE_MEDIC) ||
					(m_pPed->GetPedType() == PEDTYPE_FIRE && otherPed.GetPedType() == PEDTYPE_FIRE) ||
					(m_pPed->GetPedType() == PEDTYPE_COP && otherPed.GetPedType() == PEDTYPE_COP);
	}

#if !__NO_OUTPUT
	if ((Channel_weapon.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weapon.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{	
		if (bDebug)
		{
			weaponDebugf3("CPedIntelligence::IsFriendlyWith--m_pPed.GroupIndex[%d] otherPed.GetGroundIndex[%d] RESPECT[%d] LIKE[%d] SAMEPEDGROUP[%d] MEDIC[%d] FIRE[%d] COP[%d]-->return bFriendly[%d]",
				m_pPed->GetPedsGroup() ? m_pPed->GetPedsGroup()->GetGroupIndex() : -9,
				otherPed.GetPedsGroup() ? otherPed.GetPedsGroup()->GetGroupIndex() : -9,
				pRelGroup ? pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_RESPECT, otherRelationshipGroupIndex ) : 0,
				pRelGroup ? pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_LIKE, otherRelationshipGroupIndex ) : 0,
				(m_pPed->GetPedsGroup() && (m_pPed->GetPedsGroup() == otherPed.GetPedsGroup())),
				(m_pPed->GetPedType() == PEDTYPE_MEDIC && otherPed.GetPedType() == PEDTYPE_MEDIC),
				(m_pPed->GetPedType() == PEDTYPE_FIRE && otherPed.GetPedType() == PEDTYPE_FIRE),
				(m_pPed->GetPedType() == PEDTYPE_COP && otherPed.GetPedType() == PEDTYPE_COP),		
				bFriendly);
		}
	}
#endif

	return bFriendly;

}

bool CPedIntelligence::IsFriendlyWithByAnyMeans(const CPed& rOtherPed) const
{
	return (IsFriendlyWith(rOtherPed) || IsFriendlyWithDueToScenarios(rOtherPed) || IsFriendlyWithDueToSameVehicle(rOtherPed));
}

bool CPedIntelligence::IsFriendlyWithDueToSameVehicle(const CPed& rOtherPed) const
{
	//Ensure the peds are random.
	if(!m_pPed->PopTypeIsRandom() || !rOtherPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the vehicle is valid.
	if(!m_pPed->GetMyVehicle() || (m_pPed->GetMyVehicle() != rOtherPed.GetMyVehicle()))
	{
		return false;
	}

	return true;
}

bool CPedIntelligence::IsFriendlyWithDueToScenarios(const CPed& rOtherPed) const
{
	//Ensure the peds are random.
	if(!m_pPed->PopTypeIsRandom() || !rOtherPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the last used scenario point is valid.
	const CScenarioPoint* pLastUsedScenarioPoint = m_pLastUsedScenarioPoint;
	if(!pLastUsedScenarioPoint)
	{
		return false;
	}

	//Ensure the other last used scenario point is valid.
	const CScenarioPoint* pOtherLastUsedScenarioPoint = rOtherPed.GetPedIntelligence()->GetLastUsedScenarioPoint();
	if(!pOtherLastUsedScenarioPoint)
	{
		return false;
	}

	//Grab the max distance.
	TUNE_GROUP_FLOAT(FRIENDLY_WITH, fMaxDistanceBetweenScenarios, 3.5f, 0.0f, 50.0f, 0.1f);

	//Ensure the distance is within the threshold.
	ScalarV scDistSq = DistSquared(pLastUsedScenarioPoint->GetPosition(), pOtherLastUsedScenarioPoint->GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistanceBetweenScenarios));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CPedIntelligence::IsThreatenedBy(const CPed& otherPed, bool bIncludeDislike, bool bIncludeNonFriendly, bool bForceInCombatCheck) const
{
	bool bThreatened = false;

	bool bInCombat = bForceInCombatCheck || m_pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT);

	// If we aren't told to include non-friendly peds but we have TreatNonFriendlyAsHateWhenInCombat flag set then we should include non-friendly ones
	if( !bIncludeNonFriendly && bInCombat && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatNonFriendlyAsHateWhenInCombat))
	{
		bIncludeNonFriendly = true;
	}

	if (!bThreatened && bIncludeNonFriendly)
	{
		bThreatened = !CPedGroups::AreInSameGroup(m_pPed, &otherPed) && !IsFriendlyWith(otherPed);
	}

	if (!bThreatened)
	{
		// If we aren't told to include dislike but we are in combat with the treat dislike as hate when in combat flag set then we should include dislike
		if( !bIncludeDislike && bInCombat && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat) )
		{
			bIncludeDislike = true;
		}

		const CRelationshipGroup* pRelGroup = m_pPed->GetPedIntelligence()->GetRelationshipGroup();
		const int otherPedRelationshipGroupIndex = otherPed.GetPedIntelligence()->GetRelationshipGroupIndex();
		bThreatened = pRelGroup && 
			((bIncludeDislike && pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_DISLIKE, otherPedRelationshipGroupIndex ) ) ||
			pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_HATE, otherPedRelationshipGroupIndex ) ||
			pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_WANTED, otherPedRelationshipGroupIndex ));
	}

	// The relationship groups aren't correctly set up in MP games
	// Gam it here instead.
	if( NetworkInterface::IsGameInProgress() ) 
	{
		bool isLawEnforcementPed = m_pPed->IsLawEnforcementPed();

		if( !bThreatened && isLawEnforcementPed)
		{
			if(otherPed.IsAPlayerPed() && otherPed.GetPlayerWanted() && otherPed.GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN )
			{
				bThreatened=true;
			}
		}
	}

	if( bThreatened && ((m_pPed->GetPedsGroup() && m_pPed->GetPedsGroup() == otherPed.GetPedsGroup()) || IsFriendlyWith(otherPed)) )
	{
		bThreatened = false;
	}

	return bThreatened;
}

bool CPedIntelligence::IsPedThreatening(const CPed& rOtherPed, bool bIncludeNonViolentWeapons) const
{
	//Check if the ped is threatened by the other ped.
	if(IsThreatenedBy(rOtherPed))
	{
		return true;
	}

	//Check if the other ped is firing a weapon.
	if(rOtherPed.GetPedResetFlag(CPED_RESET_FLAG_FiringWeapon))
	{
		return true;
	}

	//Check if the other ped is armed.
	const CPedWeaponManager* pWeaponManager = rOtherPed.GetWeaponManager();
	if(pWeaponManager && pWeaponManager->GetIsArmed() && (bIncludeNonViolentWeapons || !pWeaponManager->GetEquippedWeaponInfo() || !pWeaponManager->GetEquippedWeaponInfo()->GetIsNonViolent()))
	{
		return true;
	}

	//Check if the other ped is a player.
	if(rOtherPed.IsPlayer())
	{
		//Check if the player is wanted.
		const CWanted* pWanted = rOtherPed.GetPlayerWanted();
		if(pWanted && (pWanted->GetWantedLevel() > WANTED_CLEAN))
		{
			return true;
		}
	}

	return false;
}


bool CPedIntelligence::Respects(const CPed& otherPed) const
{
	const CRelationshipGroup* pRelGroup = m_pPed->GetPedIntelligence()->GetRelationshipGroup();
	return pRelGroup && pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_RESPECT, otherPed.GetPedIntelligence()->GetRelationshipGroupIndex() );
}

bool CPedIntelligence::Ignores(const CPed& otherPed) const
{
	const CRelationshipGroup* pRelGroup = m_pPed->GetPedIntelligence()->GetRelationshipGroup();
	return pRelGroup && pRelGroup->CheckRelationship( ACQUAINTANCE_TYPE_PED_IGNORE, otherPed.GetPedIntelligence()->GetRelationshipGroupIndex() );
}

u8 CPedIntelligence::CountFriends(float fMaxDistance, bool bDueToScenarios, bool bDueToSameVehicle) const
{
	//Keep track of the count.
	u8 uCount = 0;

	//Grab the position.
	Vec3V vPosition = m_pPed->GetTransform().GetPosition();

	//Calculate the max distance.
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistance));

	//Iterate over the nearby peds.
	const CEntityScannerIterator it = GetNearbyPeds();
	for(const CEntity* pEntity = it.GetFirst(); pEntity != NULL; pEntity = it.GetNext())
	{
		//Ensure the distance is within the threshold.
		ScalarV scDistSq = DistSquared(vPosition, pEntity->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		//Ensure the other ped is alive.
		const CPed* pOtherPed = static_cast<const CPed *>(pEntity);
		if(pOtherPed->IsInjured())
		{
			continue;
		}

		//Check if the other ped is friendly.
		if(IsFriendlyWith(*pOtherPed))
		{
			++uCount;
		}
		else if(bDueToScenarios && IsFriendlyWithDueToScenarios(*pOtherPed))
		{
			++uCount;
		}
		else if(bDueToSameVehicle && IsFriendlyWithDueToSameVehicle(*pOtherPed))
		{
			uCount++;
		}
	}

	return uCount;
}

///////////////////////////////////////////////////////////
// FUNCTION: IsInACarOrEnteringOne
// PURPOSE:  Returns true if this ped happens to be in a car or is
//			 is in the process of entering one.
///////////////////////////////////////////////////////////

CVehicle *CPedIntelligence::IsInACarOrEnteringOne()
{
	if( GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
	{
		return (CVehicle*) GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
	}
	if( GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_BASIC) )
	{
		return (CVehicle*) m_pPed->GetMyVehicle();
	}
	return NULL;
}

CPropManagementHelper* CPedIntelligence::GetActivePropManagementHelper()
{
	// get our active task
	CTask* pBaseTask = GetTaskActive();
	//taskDisplayf("TaskActive: %s", pBaseTask->GetTaskName());

	// get the first running prop manangement helper in the task hierarchy
	return GetPropManagementHelper(pBaseTask);
}

CPropManagementHelper* CPedIntelligence::GetPropManagementHelper(CTask* pTask)
{
	// if we have a task
	if (pTask)
	{
		// check if the task has a prop helper
		if (pTask->GetPropHelper())
		{
			//taskDisplayf("CurrentTask: %s has a prop helper", pTask->GetTaskName());
			return pTask->GetPropHelper();
		}
		// otherwise, try again with our task's subtask
		else
		{
			//taskDisplayf("CurrentTask: %s doesn't have a prop helper", pTask->GetTaskName());
			return GetPropManagementHelper(pTask->GetSubTask());
		}
	}
	return NULL;
}

bool CPedIntelligence::AreFriends(const CPed& ped1, const CPed& ped2)
{
	return (ped1.GetPedIntelligence()->IsFriendlyWith(ped2) ||
		ped2.GetPedIntelligence()->IsFriendlyWith(ped1));
}

void
CPedIntelligence::RecordAttemptedClimbOnRoute(const Vector3 & vClimbPosition)
{
	m_vLastAttemptedClimbPosition = vClimbPosition;
	m_iLastAttemptedClimbTime = fwTimer::GetTimeInMilliseconds();
}

void CPedIntelligence::RecordFailedClimbOnRoute(const Vector3 & vClimbPosition)
{
	static const float fSameClimbEps = 0.1f;
	static const float fSameClimbEpsZ = 2.0f;
	static const s32 iMaxNumAttempts = 3;

	if(rage::IsNearZero(vClimbPosition.x - m_vFailedClimb.x, fSameClimbEps) &&
		rage::IsNearZero(vClimbPosition.y - m_vFailedClimb.y, fSameClimbEps) &&
		rage::IsNearZero(vClimbPosition.z - m_vFailedClimb.z, fSameClimbEpsZ))
	{
		m_iNumTimesClimbFailed++;
		if(m_iNumTimesClimbFailed >= iMaxNumAttempts)
		{
			// Disable any regular adjacency climb here
			CPathServer::DisableClimbAtPosition(vClimbPosition, false);
			// Disable any climbable object link here
			CPathServer::DisableClimbAtPosition(vClimbPosition, true);

			m_vFailedClimb = Vector3(0.0f,0.0f,0.0f);
			m_iNumTimesClimbFailed = 0;
		}
	}
	else
	{
		m_vFailedClimb = vClimbPosition;
		m_iNumTimesClimbFailed = 1;
	}
}

s32 CPedIntelligence::RecordStaticCountReachedMax(const Vector3& vPosition)
{
	static dev_u32 iMaxDelta = 15000;
	static dev_float fMaxDistSqr = 5.0f*5.0f;

	u32 iDeltaMs = fwTimer::GetTimeInMilliseconds() - m_iFirstStuckTime;
	if(iDeltaMs < iMaxDelta)
	{
		const float fDistSqr = (m_vStuckPosition - vPosition).Mag2();
		if(fDistSqr < fMaxDistSqr)
		{
			m_iNumTimesStuck++;
			return m_iNumTimesStuck;
		}
	}

	m_vStuckPosition = vPosition;
	m_iNumTimesStuck = 1;
	m_iFirstStuckTime = fwTimer::GetTimeInMilliseconds();

	return m_iFirstStuckTime;
}

void CPedIntelligence::RecordBeganCrossingRoad(const Vector3& vStartPos, const Vector3& vEndPos) 
{
	m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad, true);
	m_vCrossRoadStartPos = vStartPos;
	m_vCrossRoadEndPos = vEndPos;
}

void CPedIntelligence::RecordFinishedCrossingRoad()
{
	m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad, false);
	m_uLastFinishedCrossRoadTimeMS = fwTimer::GetTimeInMilliseconds();
}

bool CPedIntelligence::ShouldResumeCrossingRoad() const
{
	return m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BeganCrossingRoad);
}

void CPedIntelligence::RestartNavigationThroughRegion(const Vector3 & vMin, const Vector3 & vMax)
{
	CTaskMoveWander * pTaskWander = (CTaskMoveWander*) FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER);
	if(pTaskWander)
	{
		if(pTaskWander->GetPathIntersectsRegion(vMin, vMax))
		{
			pTaskWander->Restart();
		}
	}
	CTaskMoveFollowNavMesh * pTaskNavMesh = (CTaskMoveFollowNavMesh*) FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
	if(pTaskNavMesh)
	{
		if(pTaskNavMesh->GetPathIntersectsRegion(vMin, vMax))
		{
			pTaskNavMesh->Restart();
		}
	}
}

float CPedIntelligence::GetMoveBlendRatioFromGoToTask(void) const
{
	// If this is a player ped, and they're doing  TASK_MOVE_PLAYER - then get the move state
	// from the ped's moveblend vector within the CPedMoveBlendData class.
	CTask * pSimplestMoveTask = GetActiveSimplestMovementTask();
	if( pSimplestMoveTask && pSimplestMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_PLAYER )
	{
		Vector2 vMoveBlend;
		m_pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMoveBlend);
		float fMoveBlendRatio = vMoveBlend.Mag();
		return fMoveBlendRatio;
	}

	if(!pSimplestMoveTask)
	{
		return 0.0f;
	}

	Assert( pSimplestMoveTask->IsMoveTask() );

	if (!pSimplestMoveTask->GetMoveInterface())
	{
		return 0.0f;
	}

	return pSimplestMoveTask->GetMoveInterface()->GetMoveBlendRatio();
}

void CPedIntelligence::RefreshCombatDirector()
{
	//Release the combat director.
	CCombatDirector::RefreshCombatDirector(*m_pPed);
}

void CPedIntelligence::ReleaseCombatDirector()
{
	//Release the combat director.
	CCombatDirector::ReleaseCombatDirector(*m_pPed);
}

void CPedIntelligence::RequestCombatDirector()
{
	//Request a combat director.
	CCombatDirector::RequestCombatDirector(*m_pPed);
}

CCombatOrders* CPedIntelligence::GetCombatOrders() const
{
	//Find the combat task.
	CTaskCombat* pCombatTask = static_cast<CTaskCombat *>(FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(!pCombatTask)
	{
		return NULL;
	}

	return &(pCombatTask->GetCombatOrders());
}

void CPedIntelligence::RecordEventForScript(const int iEventType, const int iEventPriority)
{
	if(EVENT_SCRIPT_COMMAND==iEventType)
	{
		return;
	}

	if((EVENT_NONE==iEventType) || (iEventPriority>m_iHighestPriorityEventPriority))
	{
		m_iHighestPriorityEventType=(u8)iEventType;
		m_iHighestPriorityEventPriority=(u8)iEventPriority;
	}
}

bool CPedIntelligence::NetworkMigrationAllowed(const netPlayer& player, eMigrationType migrationType) const
{
    // only check top level (active) tasks, as soon as we find a task at one of the priorities
    // we don't want to allow lower priority tasks to prevent migration
    bool bPrimaryTaskFound = false;

	for(int i=0; i<PED_TASK_PRIORITY_MAX && !bPrimaryTaskFound; i++)
	{
		CTask * pTask = static_cast<CTask*>(m_taskManager.GetTask(PED_TASK_TREE_PRIMARY, i));

        bPrimaryTaskFound = pTask != 0;

		while(pTask)
		{
#if ENABLE_NETWORK_LOGGING
			if(pTask->IsClonedFSMTask())
			{
				static_cast<CTaskFSMClone*>(pTask)->ResetLastTaskMigrateFailReason();
			}
#endif //ENABLE_NETWORK_LOGGING

			if (pTask->IsClonedFSMTask() && !static_cast<CTaskFSMClone*>(pTask)->ControlPassingAllowed(m_pPed, player, migrationType) && !CanForcefullyAllowControlPass())
			{
#if ENABLE_NETWORK_LOGGING
				formatf(m_lastMigrationFailTaskReason, "%s / %s", pTask->GetName().c_str(), static_cast<CTaskFSMClone*>(pTask)->GetLastTaskMigrateFailReason());
#endif // ENABLE_NETWORK_LOGGING
				return false;
			}

			pTask = pTask->GetSubTask();
		}
	}

    // only check top level (active) tasks, as soon as we find a task at one of the priorities
    // we don't want to allow lower priority tasks to prevent migration
    bool bSecondaryTaskFound = false;

	for(int i=0;i<PED_TASK_SECONDARY_MAX && !bSecondaryTaskFound;i++)
	{
		CTask *pTask = static_cast<CTask*>(m_taskManager.GetTask(PED_TASK_TREE_SECONDARY, i));

        bSecondaryTaskFound = pTask != 0;

		while(pTask)
		{
#if ENABLE_NETWORK_LOGGING
			if(pTask->IsClonedFSMTask())
			{
				static_cast<CTaskFSMClone*>(pTask)->ResetLastTaskMigrateFailReason();
			}
#endif //ENABLE_NETWORK_LOGGING

			if (pTask->IsClonedFSMTask() && !static_cast<CTaskFSMClone*>(pTask)->ControlPassingAllowed(m_pPed, player, migrationType) && !CanForcefullyAllowControlPass())
			{
#if ENABLE_NETWORK_LOGGING
				formatf(m_lastMigrationFailTaskReason , "%s / %s", pTask->GetName().c_str(), static_cast<CTaskFSMClone*>(pTask)->GetLastTaskMigrateFailReason());
#endif // ENABLE_NETWORK_LOGGING
				return false;
			}

			pTask = pTask->GetSubTask();
		}
	}

	return true;
}

CTaskFSMClone *CPedIntelligence::CreateCloneTaskForTaskType(u32 taskType) const
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Trying to create a clone task on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            return cloneTaskTree->CreateCloneTaskForTaskType(taskType);
        }
    }

    return 0;
}

CTask* CPedIntelligence::CreateCloneTaskFromInfo(CTaskInfo *taskInfo) const
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Trying to create a clone task on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            return cloneTaskTree->CreateCloneTaskFromInfo(taskInfo);
        }
    }

    return 0;
}

void CPedIntelligence::RecalculateCloneTasks()
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Trying to recalculate clone tasks on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            cloneTaskTree->RecalculateTasks();
        }
    }
}

void CPedIntelligence::UpdateTaskSpecificData()
{
	if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Trying to recalculate clone tasks on a local ped!"))
	{
		CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
		if(cloneTaskTree)
		{
			cloneTaskTree->UpdateTaskSpecificData();
		}
	}
}

bool CPedIntelligence::CanForcefullyAllowControlPass() const
{
	bool useTaskFailTimer = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CONTROL_PASSING_TASK_FAIL_TIMER_ENABLED", 0x768BFC4D), true);
	if(useTaskFailTimer)
	{
		u32 lastFailTime = m_ControlPassingFailTime;
		gnetDebug3("[ControlPassing] %s passing not allowed. lastFailTime: %u", m_pPed->GetLogName(), lastFailTime);

		// If the last fail time happened a long time ago:
		if(lastFailTime  < fwTimer::GetSystemTimeInMilliseconds() - RESET_CONTROL_PASS_FAIL_TIME)
		{
			gnetDebug3("[ControlPassing] %s setting fail timer: %u", m_pPed->GetLogName(), fwTimer::GetSystemTimeInMilliseconds());
			m_ControlPassingFailTime = fwTimer::GetSystemTimeInMilliseconds();
		}
		else
		{
			gnetDebug3("[ControlPassing] %s checking fail timer. Current: %u", m_pPed->GetLogName(), fwTimer::GetSystemTimeInMilliseconds());
			// If the last fail time happened more than MAX_CONTROL_PASS_FAIL_TIME seconds ago, then we can allow control passing
			// Otherwise we continue blocking control passing to allow the task some time fix itself up
			if(fwTimer::GetSystemTimeInMilliseconds() > (lastFailTime  + MAX_CONTROL_PASS_FAIL_TIME))
			{
				gnetDebug3("[ControlPassing] %s passing forcefully allowed.", m_pPed->GetLogName());
				return true;
			}
		}
	}
	return false;
}

bool CPedIntelligence::ControlPassingAllowed(const netPlayer& player, eMigrationType migrationType)
{
    if(m_pPed && taskVerifyf(m_pPed->IsNetworkClone(), "Calling ControlPassingAllowed on a local ped!"))
    {
        CTaskTreeClone *cloneTaskTree = SafeCast(CTaskTreeClone, m_taskManager.GetTree(PED_TASK_TREE_PRIMARY));
        if(cloneTaskTree)
        {
            bool passingAllowed = cloneTaskTree->ControlPassingAllowed(player, migrationType);
			
			if(!passingAllowed && netObject::IsCriticalMigration(migrationType))
			{
				passingAllowed = CanForcefullyAllowControlPass();
			}

			return passingAllowed;
        }
    }

    return true;
}

void CPedIntelligence::InstantTaskUpdate(bool bForceFullUpdate)
{
	bool fullUpdate = bForceFullUpdate || CPedAILodManager::ShouldDoFullUpdate(*m_pPed);

	const float fTimeslicedTimeStep = m_fTimeSinceLastAiUpdate;
	m_fTimeSinceLastAiUpdate = fTimeslicedTimeStep;

	Process_Tasks(fullUpdate, fTimeslicedTimeStep);

	if(fullUpdate)
	{
		m_fTimeSinceLastAiUpdate = 0.0f;
	}
}

void CAi::Init()
{
	CTaskSmartFlee::InitClass();
	CTaskMoveCombatMounted::InitTransitionTables();

	CTaskCombat::InitClass();
	//@@: location CAI_INIT_CCOMBBATMANAGER_INITCLASS
	CCombatManager::InitClass();

	// Disabled, didn't have any effect these days.
	//	CScriptedPriorities::Init();
}

void CAi::Shutdown()
{
	CCombatManager::ShutdownClass();
	CTaskSmartFlee::ShutdownClass();

	GetEventGlobalGroup()->Shutdown();

	// Disabled, didn't have any effect these days.
	//	CScriptedPriorities::Shutdown();
}

#if __DEV
void CPedIntelligence::PrintTasks()
{	
	// Print the task hierarchy for this ped
	Printf("Task hierarchy\n");
	CTask* pActiveTask = GetTaskActive();
	if(pActiveTask)
	{
		CTask* pTaskToPrint = pActiveTask;
		while(pTaskToPrint)
		{
			Printf("name: %s\n", (const char*) pTaskToPrint->GetName());
			pTaskToPrint=pTaskToPrint->GetSubTask();
		}
	}

	// Print the movement task hierarchy for this ped
	Printf("Movement task hierarchy\n");
	pActiveTask = GetActiveMovementTask();
	if(pActiveTask)
	{
		CTask* pTaskToPrint = pActiveTask;
		while(pTaskToPrint)
		{
			Printf("name: %s\n", (const char*) pTaskToPrint->GetName());
			pTaskToPrint=pTaskToPrint->GetSubTask();
		}
	}
}
//*******************************************************************************
//	PrintTasksAndEvents
//	This function should be as above, but output all task trees & events.
//*******************************************************************************

void CPedIntelligence::PrintTasksAndEvents()
{
	for(int iTaskPriority=0; iTaskPriority<PED_TASK_PRIORITY_MAX; iTaskPriority++)
	{
		aiTask * pTask = m_taskManager.GetTask(PED_TASK_TREE_PRIMARY, iTaskPriority);
		while(pTask)
		{

			pTask = pTask->GetSubTask();
		}
	}
}

void CPedIntelligence::PrintEvents() const
{
	m_eventHandler.Print();
	m_eventGroup.Print();
}

void CPedIntelligence::PrintScriptTaskHistory() const
{
	const s32 iStart = m_iCurrentHistroyTop - 1;
	for (s32 i=0; i > -MAX_DEBUG_SCRIPT_TASK_HISTORY; i--)
	{
		s32 iIndex = iStart + i;
		if (iIndex < 0)
		{
			iIndex += MAX_DEBUG_SCRIPT_TASK_HISTORY;
		}

		if(m_aScriptHistoryName[iIndex] != NULL )
		{
			float fHowLongAgo = ((float)(fwTimer::GetTimeInMilliseconds() - m_aScriptHistoryTime[iIndex])) / 1000.0f;
			printf("%s %s script(%s, %d) - %.2f\n", 
				m_aScriptHistoryName[iIndex], 
				m_aScriptHistoryTaskName[iIndex], 
				m_szScriptName[iIndex], 
				m_aProgramCounter[iIndex], 
				fHowLongAgo);
		}
	}
}

#endif // __DEV

bank_float CPedIntelligence::BATTLE_AWARENESS_BULLET_WHIZBY = 20.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_BULLET_IMPACT = 15.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_GUNSHOT = 10.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER = 50.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_DAMAGE = 25.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_MELEE_FIGHT = 75.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_SHOVED = 50.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_MIN = 0.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_MAX = 100.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_DEPLETION_TIME_FROM_MAX = 10.0f;
bank_float CPedIntelligence::BATTLE_AWARENESS_MIN_TIME = 5.0f;
bank_u32 CPedIntelligence::BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER_COOLDOWN = 1000;

void CPedIntelligence::IncreaseBattleAwareness(float fIncrement, bool bLocalPedTriggered, bool bForceRun)
{	
	bool bIncrease = true;

	if(bLocalPedTriggered)
	{
		//! Only add awareness if fired outwith small tolerance.
		if( (m_nLastBattleEventTimeLocalPlayer > 0) && 
			fwTimer::GetTimeInMilliseconds() < (m_nLastBattleEventTimeLocalPlayer + BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER_COOLDOWN) )
		{
			bIncrease = false;
		}
		else
		{
			m_nLastBattleEventTimeLocalPlayer = fwTimer::GetTimeInMilliseconds();
		}
	}
	else
	{
		m_nLastBattleEventTime = fwTimer::GetTimeInMilliseconds();
	}

	if(bIncrease)
	{
		m_fBattleAwareness = Min(m_fBattleAwareness + fIncrement, BATTLE_AWARENESS_MAX);
	}

	if(bForceRun)
	{
		m_bBattleAwarenessForcingRun = true;
	}
}

void CPedIntelligence::DecreaseBattleAwareness(float fDecrement)
{
	m_fBattleAwareness = Max(m_fBattleAwareness - fDecrement, 0.0f);
	if(m_fBattleAwareness <= 0.0f)
	{	
		m_bBattleAwarenessForcingRun = false;
	}
}

void CPedIntelligence::ResetBattleAwareness()
{
	m_fBattleAwareness = 0.0f;
	m_nLastBattleEventTime = 0;
	m_nLastBattleEventTimeLocalPlayer = 0;
	m_bBattleAwarenessForcingRun = false;
}

bool CPedIntelligence::IsBattleAwareForcingRun() const
{
	return m_bBattleAwarenessForcingRun;
}

bool CPedIntelligence::IsBattleAware() const
{
	return m_fBattleAwareness > BATTLE_AWARENESS_MIN;
}

float CPedIntelligence::GetBattleAwareness() const
{
	return (m_fBattleAwareness-BATTLE_AWARENESS_MIN)/(BATTLE_AWARENESS_MAX-BATTLE_AWARENESS_MIN);
}

float CPedIntelligence::GetBattleAwarenessSecs() const
{
	return (m_fBattleAwareness-BATTLE_AWARENESS_MIN)/(BATTLE_AWARENESS_MAX-BATTLE_AWARENESS_MIN)*BATTLE_AWARENESS_DEPLETION_TIME_FROM_MAX;
}

u32 CPedIntelligence::GetLastBattleEventTime(bool bIncludeLocalEvents) const
{
	if(bIncludeLocalEvents)
	{
		return m_nLastBattleEventTimeLocalPlayer > m_nLastBattleEventTime ? m_nLastBattleEventTimeLocalPlayer : m_nLastBattleEventTime;
	}
	return m_nLastBattleEventTime;
}

void CPedIntelligence::RecordPedSeenDeadPed()
{
	m_nLastTimeDeadPedSeen = fwTimer::GetTimeInMilliseconds();
}

bool CPedIntelligence::HasPedSeenDeadPedRecently(u32 uDiffMS) const
{
	const u32 uDelta = fwTimer::GetTimeInMilliseconds() - m_nLastTimeDeadPedSeen;
	if (uDelta < uDiffMS)
	{
		return true;
	}
	return false;
}

bool CPedIntelligence::CanEntityIncreaseBattleAwareness(CEntity *pEntity) const
{
	bool bIncrease = false;
	if(pEntity && pEntity->GetType() == ENTITY_TYPE_PED)
	{
		CPed *pOtherPed = static_cast<CPed*>(pEntity);

		CPedTargetting *pOtherTargeting = pOtherPed->GetPedIntelligence()->GetTargetting();

		//! If either ped is a threat to each other, then increase.
		if(pOtherPed->GetPedIntelligence()->IsThreatenedBy(*m_pPed) || m_pPed->GetPedIntelligence()->IsThreatenedBy(*pOtherPed))
		{
			bIncrease = true;
		}
		else if(pOtherTargeting)
		{
			//! If this ped is in other peds targeting list, or if other ped is targeting a friendly, increase.
			const int numActiveTargets = pOtherTargeting->GetNumActiveTargets();
			for(int i = 0; i < numActiveTargets; ++i)
			{
				const CEntity *pTargetEntity = pOtherTargeting->GetTargetAtIndex(i);
				if(pTargetEntity && pTargetEntity->GetIsTypePed())
				{
					const CPed *pTargetPed = static_cast<const CPed*>(pTargetEntity);
					if( (pTargetPed == m_pPed) || IsFriendlyWith(*pTargetPed))
					{
						bIncrease = true;
						break;
					}
				}
			}
		}
	}
	else
	{
		bIncrease = true;
	}

	return bIncrease;
}

bool CPedIntelligence::CanBePinnedDown() const
{
	//Check if we can be pinned down.
	if(!m_combatBehaviour.IsFlagSet(CCombatData::BF_DisablePinnedDown))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CPedIntelligence::CanPinDownOthers() const
{
	//Check if we can pin down others.
	if(!m_combatBehaviour.IsFlagSet(CCombatData::BF_DisablePinDownOthers))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//-------------------------------------------------------------------------
// Increase the amount by which the ped is pinned down,
// The increase is altered by the peds pinned down affector, to make less 
// cautious peds less affected by shots etc...
//-------------------------------------------------------------------------
void  CPedIntelligence::IncreaseAmountPinnedDown	( float fIncrease )		
{
	// Record the last time we increased our pinned down time
	SetLastPinnedDownTime(fwTimer::GetTimeInMilliseconds());

	// The amount a ped caution is increased is dependent upon how cautious they are, 
	// the more cautions the more they are bothered, am i bothered tho?
	float fPedCaution;
	if(GetCombatBehaviour().GetCombatAbility() == CCombatData::CA_Poor )
	{
		fPedCaution = 0.75f;
	}
	else // CA_Average and CA_Professional
	{
		fPedCaution = 0.55f;
	}

	SetAmountPinnedDown(m_fPinnedDown + fIncrease * fPedCaution);
}

void CPedIntelligence::IncreaseAlertnessState()
{
	AlertnessState current_state = GetAlertnessState();	
	if (current_state < AS_MustGoToCombat)
	{
		SetAlertnessState(static_cast<AlertnessState>(current_state + 1));
	}
}


void CPedIntelligence::DecreaseAlertnessState()
{
	AlertnessState current_state = GetAlertnessState();	
	if (current_state > AS_NotAlert)
	{
		SetAlertnessState(static_cast<AlertnessState>(current_state - 1));
	}
}

bool CPedIntelligence::GetCanCombatRoll() const
{
#if __DEV
	TUNE_GROUP_BOOL(PED_MOVEMENT, INSTANT_COMBAT_ROLL, false);
	if(INSTANT_COMBAT_ROLL)
	{
		return true;
	}
#endif // __DEV

	// Don't combat roll if disabled by script
	if (m_pPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePlayerCombatRoll))
	{
		return false;
	}

	if(m_pPed->GetIsInCover() || m_pPed->GetIsSwimming())
	{
		return false;
	}

	if(m_pPed->GetHasJetpackEquipped())
	{
		return false;
	}

	// Don't combat roll if super jump is enabled.
	if (m_pPed->GetPlayerInfo() && m_pPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON))
	{
		return false;
	}

	if(m_iLastCombatRollTime == 0)
	{
		return true;
	}
	else
	{
		TUNE_GROUP_FLOAT(PED_MOVEMENT, COMBAT_ROLL_WORST_TIME, 3.0f, 0.f, 100.f, 0.01f);
		TUNE_GROUP_FLOAT(PED_MOVEMENT, COMBAT_ROLL_BEST_TIME,  1.0f, 0.f, 100.f, 0.01f);

		static const float ONE_OVER_100 = 1.f / 100.f;
		float fShootingAbility = StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY")) * ONE_OVER_100;
		float fCombatRollTime = COMBAT_ROLL_WORST_TIME + ((COMBAT_ROLL_BEST_TIME - COMBAT_ROLL_WORST_TIME) * fShootingAbility);
		u32 iCombatRollMinTime = (u32)(fCombatRollTime * 1000.f);

		if((m_iLastCombatRollTime + iCombatRollMinTime) < fwTimer::GetTimeInMilliseconds())
			return true;
	}

	return false;
}

bool CPedIntelligence::GetCanDoAimGrenadeThrow() const
{
#if __DEV
	TUNE_GROUP_BOOL(AIMING_TUNE, INSTANT_AIM_GRENADE_THROW, false);
	if(INSTANT_AIM_GRENADE_THROW)
	{
		return true;
	}
#endif // __DEV

	if(m_pPed->GetIsSwimming())
	{
		return false;
	}

	if(m_iLastAimGrenadeThrowTime == 0)
	{
		return true;
	}
	else
	{
		TUNE_GROUP_FLOAT(AIMING_TUNE, AIM_GRENADE_THROW_TIME, 1.0f, 0.f, 100.f, 0.01f);
		float fAimGrenadeThrowTime = AIM_GRENADE_THROW_TIME;
		u32 iAimGrenadeThrowMinTime = (u32)(fAimGrenadeThrowTime * 1000.f);

		if((m_iLastAimGrenadeThrowTime + iAimGrenadeThrowMinTime) < fwTimer::GetTimeInMilliseconds())
			return true;
	}

	return false;
}

//-------------------------------------------------------------------------
// Decrease the amount by which the ped is pinned down
//-------------------------------------------------------------------------
void  CPedIntelligence::DecreaseAmountPinnedDown	( float fDecrease )		
{ 
	SetAmountPinnedDown(m_fPinnedDown - fDecrease);
}

//-------------------------------------------------------------------------
// Directly set the amount by which this is ped is pinned down
//-------------------------------------------------------------------------
void  CPedIntelligence::SetAmountPinnedDown		( float fPinnedDown, bool bForce )	
{
	float fMaxPinnedDown = (bForce || CanBePinnedDown()) ? 100.0f : 0.0f;
	m_fPinnedDown = Clamp(fPinnedDown, 0.0f, fMaxPinnedDown);
}

//-------------------------------------------------------------------------
// Directly set the amount by which this is ped is pinned down
//-------------------------------------------------------------------------
void  CPedIntelligence::ForcePedPinnedDown		(const bool bPinned, float fTime )	
{ 
	m_fForcedPinnedTimer = fTime;
	m_bForcePinned = bPinned;
	SetAmountPinnedDown( 100.0f );
}

void CPedIntelligence::SetCheckShockFlag(bool bCheck)
{ 
	if (bCheck)
	{
		m_Flags.SetFlag(PIFlag_CheckShockingEvents);
	}
	else
	{
		m_Flags.ClearFlag(PIFlag_CheckShockingEvents); 
	}
}

void CPedIntelligence::SetStartNewHangoutScenario(bool bOnOff)
{ 
	if (bOnOff)
	{
		m_Flags.SetFlag(PIFlag_StartNewHangoutScenario);
	}
	else
	{
		m_Flags.ClearFlag(PIFlag_StartNewHangoutScenario);
	}
}

void CPedIntelligence::SetTakenNiceCarPicture(bool bOnOff)
{
	if (bOnOff)
	{
		m_Flags.SetFlag(PIFlag_TakenNiceCarPicture);
	}
	else
	{
		m_Flags.ClearFlag(PIFlag_TakenNiceCarPicture);
	}
}

void CPedIntelligence::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	//Find the lowest/simplest task in the hierarchy that overrides the camera and use its settings.
	    
	CTask* activeTask = GetTaskActiveSimplest();
	while(activeTask)
	{
		tGameplayCameraSettings tempSettings(settings);

		activeTask->GetOverriddenGameplayCameraSettings(tempSettings);

		//NOTE: A task must override the camera in order to override the other settings.
		if(tempSettings.m_CameraHash)
		{
			settings = tempSettings;
			return; 
		}

		activeTask = activeTask->GetParent();
	}	 

	// taskmobile phone is a secondary
	activeTask = GetTaskSecondaryActive(); 
	while(activeTask)
	{
		tGameplayCameraSettings tempSettings(settings);

		activeTask->GetOverriddenGameplayCameraSettings(tempSettings);

		//NOTE: A task must override the camera in order to override the other settings.
		if(tempSettings.m_CameraHash)
		{
			settings = tempSettings;
			return; 
		}

		activeTask = activeTask->GetParent();
	}

	activeTask = GetMotionTaskActiveSimplest(); 
	while(activeTask)
	{
		tGameplayCameraSettings tempSettings(settings);

		activeTask->GetOverriddenGameplayCameraSettings(tempSettings);

		//NOTE: A task must override the camera in order to override the other settings.
		if(tempSettings.m_CameraHash)
		{
			settings = tempSettings;
			return; 
		}

		activeTask = activeTask->GetParent();
	}
}

//set the ped 
void CPedIntelligence::SetPedMotivation(const CPed* pPed)
{
	if (pPed)
	{
		if (pPed->GetBaseModelInfo() != NULL)
		{
			u32 MotivationGroup = pPed->GetMotivation(); 
			m_pedMotivation.SetMotivationProfile(MotivationGroup); 
		}
	}
}

//set the ped's nav capabilities 
void CPedIntelligence::SetNavCapabilities(const CPed* pPed)
{
	if (pPed)
	{
		if(pPed->GetBaseModelInfo() != NULL)
		{
			const CPedNavCapabilityInfo* pNavCapabilities = CPedNavCapabilityInfoManager::GetInfo(pPed->GetPedModelInfo()->GetNavCapabilitiesHash().GetHash());
			Assert(pNavCapabilities);
			if (pNavCapabilities)
			{
				m_navCapabilities = *pNavCapabilities;
			}
		}
	}
}

//set the ped's perception based on its hash value
void CPedIntelligence::SetPedPerception(const CPed* pPed)
{
	if (pPed)
	{
		if  (pPed->GetBaseModelInfo() != NULL)
		{
			const SPedPerceptionInfo* pPerception = CPedPerceptionInfoManager::GetPerception(pPed->GetPedModelInfo()->GetPerceptionHash().GetHash());
			Assertf(pPerception, "Could not find a valid perception for ped!");
			if (pPerception)
			{
				m_pedPerception.SetEncroachmentRange(pPerception->m_EncroachmentRange);
				m_pedPerception.SetEncroachmentCloseRange(pPerception->m_EncroachmentCloseRange);
				m_pedPerception.SetSeeingRange(pPerception->m_SeeingRange);
				m_pedPerception.SetHearingRange(pPerception->m_HearingRange);
				m_pedPerception.SetSeeingRangePeripheral(pPerception->m_SeeingRangePeripheral);
				m_pedPerception.SetVisualFieldMinAzimuthAngle(pPerception->m_VisualFieldMinAzimuthAngle);
				m_pedPerception.SetVisualFieldMaxAzimuthAngle(pPerception->m_VisualFieldMaxAzimuthAngle);
				m_pedPerception.SetVisualFieldMinElevationAngle(pPerception->m_VisualFieldMinElevationAngle);
				m_pedPerception.SetVisualFieldMaxElevationAngle(pPerception->m_VisualFieldMaxElevationAngle);
				m_pedPerception.SetCentreOfGazeMaxAngle(pPerception->m_CentreFieldOfGazeMaxAngle);
				m_pedPerception.SetCanAlwaysSenseEncroachingPlayers(pPerception->m_CanAlwaysSenseEncroachingPlayers);
				m_pedPerception.SetPerformsEncroachmentChecksIn3D(pPerception->m_PerformEncroachmentChecksIn3D);
			}
		}
	}
}

// B*2343564: Set cop ped perception values based on script-specified overrides (from SET_COP_PERCEPTION_OVERRIDES).
void CPedIntelligence::SetCopPedOverridenPerception(const CPed *pPed)
{
	if (pPed && CPedPerception::ms_bCopOverridesSet)
	{
		m_pedPerception.SetSeeingRange(CPedPerception::ms_fCopSeeingRangeOverride);
		m_pedPerception.SetSeeingRangePeripheral(CPedPerception::ms_fCopSeeingRangePeripheralOverride);
		m_pedPerception.SetHearingRange(CPedPerception::ms_fCopHearingRangeOverride);
		m_pedPerception.SetVisualFieldMinAzimuthAngle(CPedPerception::ms_fCopVisualFieldMinAzimuthAngleOverride);
		m_pedPerception.SetVisualFieldMaxAzimuthAngle(CPedPerception::ms_fCopVisualFieldMaxAzimuthAngleOverride);
		m_pedPerception.SetCentreOfGazeMaxAngle(CPedPerception::ms_fCopCentreOfGazeMaxAngleOverride);
		m_pedPerception.SetRearViewRangeOverride(CPedPerception::ms_fCopRearViewRangeOverride);
	}
}

void CPedIntelligence::Process_NearbyEntityLists()
{
	PF_FUNC(AI_Process_NearbyEntityLists);

	//reset near-door flag
	m_pPed->SetPedResetFlag(CPED_RESET_FLAG_IsNearDoor, false);

	if(!m_pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
	{
		// Update all peds and vehicles and objects in range.
		m_vehicleScanner.ScanForEntitiesInRange(*m_pPed);
		m_pedScanner.ScanForEntitiesInRange(*m_pPed, ShouldForcePedScanThisFrame());

		if( m_pPed->GetPedResetFlag( CPED_RESET_FLAG_SearchingForDoors ) )
		{
			m_pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDoors, false);
			m_doorScanner.ScanForEntitiesInRange(*m_pPed);

			//update near-Door flag
			CDoor* pClosestDoor = GetClosestDoorInRange();
			if (pClosestDoor && !pClosestDoor->IsBaseFlagSet( fwEntity::IS_FIXED ))
			{				
				static dev_float sfMinDoorDistance = rage::square(1.5f);			
				const float fDoorDistance = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()).Dist2(VEC3V_TO_VECTOR3(pClosestDoor->GetTransform().GetPosition()));	
				if (fDoorDistance <= sfMinDoorDistance) 
					m_pPed->SetPedResetFlag(CPED_RESET_FLAG_IsNearDoor, true);
			}

			// Note: in other code paths, m_doorScanner should always be empty, so there is no need to
			// run the code above as it could never have any effect.
		}
		else
		{
			m_doorScanner.Clear();
		}

		// Don't update the object scanners, just tick them on so any recent forallentities
		// are invalidated after time, set the count to 0 if it finishes so that any subsequent call will immediately update
		// the list again
		if( m_objectScanner.GetTimer().Tick() )
		{
			m_objectScanner.GetTimer().SetCount(0);
		}
	}
	else
	{
		//m_vehicleScanner.Clear();
		//m_doorScanner.Clear();
		m_pedScanner.Clear();
		//m_objectScanner.Clear();
		//Assert(m_vehicleScanner.IsEmpty());
		Assert(m_doorScanner.IsEmpty());
		//Assert(m_pedScanner.IsEmpty());
		//Assert(m_objectScanner.IsEmpty());
	}
}

bool CPedIntelligence::ShouldForcePedScanThisFrame() const
{
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (pPlayer)
	{
		CVehicle* pVehicle = pPlayer->GetVehiclePedInside();
		if (pVehicle)
		{
			if (HasResponseToEvent(EVENT_ENCROACHING_PED))
			{
				if (IsGreaterThanAll(MagSquared(pVehicle->GetAiVelocityConstRef()), ScalarV(ms_fFrequentScanVehicleVelocitySquaredThreshold)))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void CPedIntelligence::Process_FacialDataPreTasks()
{
	//Ensure the facial data is valid.
	CFacialDataComponent* pFacialData = m_pPed->GetFacialData();
	if(!pFacialData)
	{
		return;
	}

	//Process the facial data.
	pFacialData->ProcessPreTasks(*m_pPed);

	if(m_pPed->GetPedResetFlag(CPED_RESET_FLAG_DoDamageCoughFacial))
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Coughing);
	}
}

void CPedIntelligence::Process_FacialDataPostTasks(float fTimeStep)
{
	//Ensure the facial data is valid.
	CFacialDataComponent* pFacialData = m_pPed->GetFacialData();
	if(!pFacialData)
	{
		return;
	}

	//Process the facial data.
	pFacialData->ProcessPostTasks(*m_pPed);

	//Update the facial data.
	pFacialData->Update(*m_pPed, fTimeStep);
}

#if LAZY_RAGDOLL_BOUNDS_UPDATE
//-------------------------------------------------------------------------
// Check for some more conditions that should trigger a bounds update
//-------------------------------------------------------------------------
void CPedIntelligence::Process_BoundUpdateRequest()
{
	if (!m_pPed->IsRagdollBoundsUpdateRequested())
	{
		// players and peds with ActivateRagdollOnCollision enabled should update
		if (m_pPed->IsPlayer() || m_pPed->GetActivateRagdollOnCollision())
		{
			m_pPed->RequestRagdollBoundsUpdate();
			return;
		}	

		// Update if a player is using a special ability
		CPed *pLocalPlayer = CGameWorld::FindLocalPlayer();
		if (pLocalPlayer && pLocalPlayer->GetSpecialAbility() && pLocalPlayer->GetSpecialAbility()->IsActive())
		{
			m_pPed->RequestRagdollBoundsUpdate();
			return;
		}

		// Should update if on screen and...
		if (m_pPed->m_nDEflags.bIsVisibleInSomeViewportThisFrame)
		{
			// ...a player is aiming a weapon (conservative method to handle stray bullets hitting peds)
			if (pLocalPlayer && pLocalPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun))
			{
				m_pPed->RequestRagdollBoundsUpdate();
				return;
			}
		}

		// Should update at least once every X frames at a minimum
		size_t staggerUpdateFrame = ((size_t)m_pPed) + fwTimer::GetFrameCount();
		if (staggerUpdateFrame % ms_MinFramesPerRagdollBoundUpdate == 0)
		{
			m_pPed->RequestRagdollBoundsUpdate(1);
			return;
		}
	}
}
#endif

void CPedIntelligence::Process_Tasks(bool fullUpdate, float fTimeStep)
{
	PF_FUNC(AI_Process_Tasks);

	// If this reset flag is set, call ProcessMoveSignals() regardless of whether this is a full update or
	// not, so we don't miss out on signals from Move.
	if(m_pPed->GetPedResetFlag(CPED_RESET_FLAG_TasksNeedProcessMoveSignalCalls))
	{
		// Call ProcessMoveSignals() on each tree.
		// Note: currently we don't do this for PED_TASK_TREE_MOVEMENT,
		// which is similar to how it's done for ProcessPostMovement(), etc. We could do it if needed, though.
		bool stillDoingSomething = m_taskManager.ProcessMoveSignals(PED_TASK_TREE_PRIMARY);
		stillDoingSomething |= m_taskManager.ProcessMoveSignals(PED_TASK_TREE_MOTION);
		stillDoingSomething |= m_taskManager.ProcessMoveSignals(PED_TASK_TREE_SECONDARY);

		// If no task returned true, we let this reset flag reset, stopping the updates.
		if(!stillDoingSomething)
		{
			m_pPed->SetPedResetFlag(CPED_RESET_FLAG_TasksNeedProcessMoveSignalCalls, false);
		}
	}

	if(fullUpdate)
	{
		// Reset any appropriate reset flags
		m_pPed->m_PedResetFlags.ResetPreTask();

		if(!m_pPed->IsNetworkClone())
		{
			// Reset the ped's strafing status also
			// If script are forcing the ped to strafe set the ped to strafe
			m_pPed->GetMotionData()->SetIsStrafing(m_pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePedToStrafe));
		}

		// Tell the IK manager that we are about to update tasks, for resetting things similar
		// to the reset flags above.
		m_pPed->GetIkManager().PreTaskUpdate();

		//Process the facial data (pre-tasks).
		Process_FacialDataPreTasks();

		// Perform the designated task.
		m_taskManager.Process(fTimeStep);

		// Clear the tasks set needing update since we have just updated this frame
		m_pPed->GetPedAiLod().SetTaskSetNeedIntelligenceUpdate(false);

		//Process the facial data (post-tasks).
		Process_FacialDataPostTasks(fTimeStep);

#if ENABLE_TASKS_ARREST_CUFFED
		// Update cuffed secondary task.
		CTaskPlayCuffedSecondaryAnims::ProcessHandCuffs(*m_pPed);
#endif // ENABLE_TASKS_ARREST_CUFFED

		// Block Anim timeslicing for this ped if tasks have been creating lots of network players without an anim update.
		// B* 1909275.
		m_pPed->DoPostTaskUpdateAnimChecks();

		m_pPed->ClearRenderDelayFlag(CPed::PRDF_DontRenderUntilNextTaskUpdate);
	}
}


void CPedIntelligence::Process_UpdateVariables(float fTimeStep)
{
	// If we are a law enforcement ped we will need to make sure certain variables are set
	if(m_pPed->IsLawEnforcementPed())
	{
		// only do this a couple times a second, no exact science so just use magic numbers
		m_fTimeUntilNextCopVariableUpdate -= fTimeStep;
		if(m_fTimeUntilNextCopVariableUpdate <= 0.0f)
		{
			UpdateCopPedVariables();
			m_fTimeUntilNextCopVariableUpdate = fwRandom::GetRandomNumberInRange(0.2f, 0.35f);
		}
	}

	// Reset the climb strength rate to the default
	SetClimbStrengthRate(DEFAULT_CLIMB_STRENGTH_RATE);

	// If the ped currently has a coverpoint claimed but is not actually using it,
	// Discard it if it is outside a reasonable range
	if( m_pPed->GetCoverPoint() )
	{
		// Coverpoints of type 'COVTYPE_NONE' should never be available for use.
		// However according to url:bugstar:426681 this has happened, so ensure that
		// we release any COVTYPE_NONE coverpoint immediately if it is found
		if( m_pPed->GetCoverPoint()->GetType() == CCoverPoint::COVTYPE_NONE )
		{
			Assertf(false, "Ped '%s' at (%.1f, %.1f, %.1f) has cover point of type COVTYPE_NONE.", m_pPed->GetModelName(), m_pPed->GetTransform().GetPosition().GetXf(), m_pPed->GetTransform().GetPosition().GetYf(), m_pPed->GetTransform().GetPosition().GetZf() );

			m_pPed->ReleaseCoverPoint();
		}
		else if( !m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint ) && !m_pPed->GetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint ) )
		{
			Vector3 vCoverPos;
			m_pPed->GetCoverPoint()->GetCoverPointPosition(vCoverPos);
			if( vCoverPos.Dist2( VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition())) > rage::square(3.0f) )
			{
				m_pPed->ReleaseCoverPoint();
			}
		}
	}

	DecreaseBattleAwareness(fTimeStep * (BATTLE_AWARENESS_MAX/BATTLE_AWARENESS_DEPLETION_TIME_FROM_MAX));

	// The amount by which the ped is pinned down degrades all the time
	DecreaseAmountPinnedDown( fTimeStep * CTaskInCover::ms_Tunables.m_PinnedDownDecreaseAmountPerSecond );

	// If this ped is being forced pinned, update the timer and set as pinned
	if( m_fForcedPinnedTimer > 0.0f )
	{
		m_fForcedPinnedTimer = MAX( m_fForcedPinnedTimer - fTimeStep, 0.0f );
		if( m_bForcePinned )
		{
			SetAmountPinnedDown( 100.0f );
		}
		else
		{
			SetAmountPinnedDown( 0.0f );
		}
	}

	// Reset the movement vector which is tested against the navmesh, to detect peds steering into walls
	m_pPed->GetNavMeshTracker().ResetAvoidanceLineTest();
	m_pPed->GetNavMeshTracker().ResetLookAheadLineTest();

	// Rest path-following data
	m_bHasClosestPosOnRoute = false;

	// Update the time for bullet reactions
	m_fTimeTilNextBulletReaction -= fTimeStep;

	// Flag if any local peds are within an air defence zone.
	if (!m_pPed->IsNetworkClone() && CAirDefenceManager::AreAirDefencesActive())
	{
		u8 uIntersectingZoneIdx = 0;
		if (CAirDefenceManager::IsPositionInDefenceZone(m_pPed->GetTransform().GetPosition(), uIntersectingZoneIdx))
		{
			m_pPed->SetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere, true);
		}
	}
}

void CPedIntelligence::Process_UpdateVariablesAfterTaskProcess(float fTimeStep)
{
	// Update the climb strength
	m_fClimbStrength += m_fClimbStrengthRate * fTimeStep;
	m_fClimbStrength  = Clamp(m_fClimbStrength, 0.0f, 1.0f);

	//if we have a valid order, we shouldn't be culled as readily
	if(GetOrder() && GetOrder()->GetIsValid())
	{
		m_pPed->SetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway, true );
		m_pPed->SetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway, true );
	}

	m_pPed->SetPedResetFlag( CPED_RESET_FLAG_TriggerRoadRageAnim, false );
}

#if __DEV
void CPedIntelligence::Process_Debug()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessIntelligenceOfFocusEntity(), m_pPed );

	m_bNewTaskThisFrame = false;

	TUNE_GROUP_BOOL(PED_NAVMESH_TRACKING, s_bDisplay, false);
	if(s_bDisplay)
	{
		if(m_pPed->GetNavMeshTracker().GetIsValid())
		{
#if DEBUG_DRAW
			const Vector3 vPos = m_pPed->GetNavMeshTracker().GetLastNavMeshIntersection();
			grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vPos), 0.25f, Color_white);
			grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vPos), VECTOR3_TO_VEC3V(vPos+m_pPed->GetNavMeshTracker().GetPolyNormal()), 0.25f, Color_yellow);
#endif // DEBUG_DRAW
		}
	}
}
#endif //__DEV

dev_s32 MAX_COPS_ON_FOOT_PLAYER_IN_INTERIOR = 10;

void CPedIntelligence::UpdateCopPedVariables()
{
	if(m_pPed->PopTypeIsMission())
	{
		return;
	}

	CVehicle* pVehicleInside = m_pPed->GetVehiclePedInside();
	if( m_pPed->GetPedType() == PEDTYPE_COP && !pVehicleInside && !m_pPed->IsInjured() ) 
	{
		CWanted::sm_iNumCopsOnFoot++;
	}

	//Grab the combat behavior.
	CCombatBehaviour &combatBehaviour = GetCombatBehaviour();

	//Check if the cop can be aggressive.
	bool bDontBeAggressive = m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NoCopWantedAggro);
	bool bCanBeAggressive = !bDontBeAggressive;
	if(bCanBeAggressive)
	{
		//Give the cop a radio.
		combatBehaviour.SetFlag(CCombatData::BF_HasRadio);

		//The cop can chase the target on foot.
		combatBehaviour.SetFlag(CCombatData::BF_CanChaseTargetOnFoot);

		//The cop should be aggressive.
		combatBehaviour.SetFlag(CCombatData::BF_Aggressive);
	}

	//Cops should drag their friends to safety.
	combatBehaviour.SetFlag(CCombatData::BF_WillDragInjuredPedsToSafety);

	if( m_pPed->GetPedType() != PEDTYPE_SWAT )
	{
		combatBehaviour.SetFlag(CCombatData::BF_CanBust);
	}

	eWantedLevel playerWantedLevel = WANTED_CLEAN;
	CWanted* pLocalPlayerWanted = CGameWorld::FindLocalPlayerWanted();
	if(pLocalPlayerWanted)
	{
		playerWantedLevel = pLocalPlayerWanted->GetWantedLevel();
	}

	CPedPerception &pPedPerception = GetPedPerception();

	//Law enforcement peds should not fly through the windscreen.
#if __DEV
	if (m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillFlyThroughWindscreen))
	{
		scriptDisplayf("Ped %s set to not fly through windscreen at frame %i, via code CPedIntelligence::UpdateCopPedVariables", m_pPed->GetDebugName() ? m_pPed->GetDebugName() : "NULL", fwTimer::GetFrameCount());
		scrThread::PrePrintStackTrace();
	}
#endif 
	m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillFlyThroughWindscreen, false);
	weaponAssert(m_pPed->GetWeaponManager());
	
	//Keep track of the ped attributes.
	CWanted::PedAttributes attributes;

	//Check if the wanted is valid.
	if(pLocalPlayerWanted)
	{
		//Calculate the ped attributes.
		pLocalPlayerWanted->CalculatePedAttributes(*m_pPed, attributes);
	}
	
	//Set the shoot rate.
	combatBehaviour.SetShootRateModifier(attributes.m_fShootRateModifier);

	//Set the weapon accuracy.
	combatBehaviour.SetCombatFloat(kAttribFloatWeaponAccuracy, attributes.m_fWeaponAccuracy);

	//Set the automobile speed modifier.
	combatBehaviour.SetCombatFloat(kAttribFloatAutomobileSpeedModifier, attributes.m_fAutomobileSpeedModifier);

	//Set the heli speed modifier.
	combatBehaviour.SetCombatFloat(kAttribFloatHeliSpeedModifier, attributes.m_fHeliSpeedModifier);

	//Set the senses range.
	if (!m_pPed->IsNetworkClone() && !CPedPerception::ms_bCopOverridesSet)
	{
		pPedPerception.SetSeeingRange(attributes.m_fSensesRange);
		pPedPerception.SetHearingRange(attributes.m_fSensesRange);
	}
	
	//Set the identification range.
	pPedPerception.SetIdentificationRange(attributes.m_fIdentificationRange);
	
	//Set the drivebys flag.
	combatBehaviour.ChangeFlag(CCombatData::BF_CanDoDrivebys, attributes.m_bCanDoDrivebys || (pVehicleInside && pVehicleInside->InheritsFromBike()));
	
	//Check if the ped is in a vehicle.
	if(pVehicleInside)
	{
		//Keep track of whether we can leave our vehicle.
		bool bCanLeaveVehicle = false;
		
		//Check if the ped is in a heli.
		if(pVehicleInside->InheritsFromHeli())
		{
			//Check if the heli is landed, or in water.
			CHeli* pHeli = static_cast<CHeli *>(pVehicleInside);
			if(pHeli->GetHeliIntelligence()->IsLanded() || (pHeli->m_Buoyancy.GetStatus() != NOT_IN_WATER && pHeli->m_Buoyancy.GetSubmergedLevel() >= 0.5f))
			{
				bCanLeaveVehicle = true;
			}
		}
		//Check if the vehicle is a boat.
		else if(pVehicleInside->InheritsFromBoat())
		{
			bCanLeaveVehicle = true;
		}
		//Combat handles if we can exit the plane in combat so set it to always be true. Cars and bikes should allow exits
		if( pVehicleInside->InheritsFromPlane() ||
			pVehicleInside->GetVehicleType() == VEHICLE_TYPE_CAR ||
			pVehicleInside->InheritsFromBike() )
		{
			bCanLeaveVehicle = true;
		}
		
		//Check if we can leave the vehicle.
		if(bCanLeaveVehicle)
		{
			bool bOptimiseVehicleExit = true;
			if( NetworkInterface::IsGameInProgress() )
				bOptimiseVehicleExit = false;

			if( bOptimiseVehicleExit && playerWantedLevel >= WANTED_LEVEL1)
			{
				bool bIsInWater = (pVehicleInside->m_Buoyancy.GetStatus() != NOT_IN_WATER);
				if(!bIsInWater && !g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(pVehicleInside->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
				{
					bCanLeaveVehicle = false;
				}
				else if( CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetIsInInterior() && CWanted::sm_iNumCopsOnFoot > MAX_COPS_ON_FOOT_PLAYER_IN_INTERIOR )
				{
					bCanLeaveVehicle = false;
				}
			}
		}
		
		//Change the flag.
		combatBehaviour.ChangeFlag(CCombatData::BF_CanLeaveVehicle, bCanLeaveVehicle);
	}
}

CTaskNMBehaviour* CPedIntelligence::GetLowestLevelNMTask(CPed* UNUSED_PARAM(pThisPed)) const
{
	// Scan through the task tree from the bottom up until a Natural Motion task is
	// found and return a pointer to this task. Null pointer returned if there is no
	// valid NM task.

	CTaskNMBehaviour* pTaskNM = NULL;

    CTask* pTask = GetTaskActiveSimplest();
	// Iterate up from the bottom task in the task chain while we have a valid task
	// pointer. Break out early if we find a Natural Motion behaviour sub-task.
	while(pTask && !pTaskNM)
	{
		pTaskNM = pTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pTask : NULL;
		pTask = pTask->GetParent();
	}

	if (!pTaskNM)
	{
		//check the motion task tree too
		CTask* pTask = GetMotionTaskActiveSimplest();
		// Iterate up from the bottom task in the task chain while we have a valid task
		// pointer. Break out early if we find a Natural Motion behaviour sub-task.
		while(pTask && !pTaskNM)
		{
			pTaskNM = pTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pTask : NULL;
			pTask = pTask->GetParent();
		}
	}

	return pTaskNM;
}

CTask* CPedIntelligence::GetLowestLevelRagdollTask(CPed* pThisPed) const
{
	// Scan through the task tree from the bottom up until a Natural Motion task is
	// found and return a pointer to this task. Null pointer returned if there is no
	// valid NM task.

	CTask* pTaskRagdoll = NULL;

	CTask* pTask = GetTaskActiveSimplest();
	// Iterate up from the bottom task in the task chain while we have a valid task
	// pointer. Break out early if we find a Natural Motion behaviour sub-task.
	while(pTask && !pTaskRagdoll)
	{
		pTaskRagdoll = pTask->HandlesRagdoll(pThisPed) ? pTask : NULL;
		pTask = pTask->GetParent();
	}

	if (!pTaskRagdoll)
	{
		//check the motion task tree too
		CTask* pTask = GetMotionTaskActiveSimplest();
		// Iterate up from the bottom task in the task chain while we have a valid task
		// pointer. Break out early if we find a Natural Motion behaviour sub-task.
		while(pTask && !pTaskRagdoll)
		{
			pTaskRagdoll = pTask->HandlesRagdoll(pThisPed) ? pTask : NULL;
			pTask = pTask->GetParent();
		}
	}

	return pTaskRagdoll;
}

void CPedIntelligence::HonkedAt(const CVehicle& rVehicle)
{
	//Get the current time.
	u32 uTime = fwTimer::GetTimeInMilliseconds();
	
	//Update the last honked at time.
	u32 uLastTimeHonkedAt = m_uLastTimeHonkedAt;
	m_uLastTimeHonkedAt = uTime;
	
	//Ensure we have been honked at quickly and successively.
	TUNE_GROUP_FLOAT(HONK, fMaxTimeBetweenHonksToBecomeAgitated, 5.0f, 0.0f, 60.0f, 1.0f);
	if(uLastTimeHonkedAt + (u32)(fMaxTimeBetweenHonksToBecomeAgitated * 1000.0f) < uTime)
	{
		return;
	}
	
	//Ensure we haven't been agitated in the last few seconds.
	TUNE_GROUP_FLOAT(HONK, fMinTimeBetweenAgitationEvents, 5.0f, 0.0f, 60.0f, 1.0f);
	if(m_uLastTimeHonkAgitatedUs + (u32)(fMinTimeBetweenAgitationEvents * 1000.0f) > uTime)
	{
		return;
	}
	
	//Become agitated at the driver.
	CEventAgitated event(rVehicle.GetDriver(), AT_HonkedAt);
	AddEvent(event);
	
	//Update the last time a honk agitated us.
	m_uLastTimeHonkAgitatedUs = uTime;
}

void CPedIntelligence::RevvedAt(const CVehicle& rVehicle)
{
	//Get the current time.
	u32 uTime = fwTimer::GetTimeInMilliseconds();

	//Update the last revved at time.
	u32 uLastTimeRevvedAt = m_uLastTimeRevvedAt;
	m_uLastTimeRevvedAt = uTime;

	//Ensure we have been revved at quickly and successively.
	TUNE_GROUP_FLOAT(REV, fMaxTimeBetweenRevsToBecomeAgitated, 5.0f, 0.0f, 60.0f, 1.0f);
	if(uLastTimeRevvedAt + (u32)(fMaxTimeBetweenRevsToBecomeAgitated * 1000.0f) < uTime)
	{
		return;
	}

	//Ensure we haven't been agitated in the last few seconds.
	TUNE_GROUP_FLOAT(REV, fMinTimeBetweenAgitationEvents, 5.0f, 0.0f, 60.0f, 1.0f);
	if(m_uLastTimeRevAgitatedUs + (u32)(fMinTimeBetweenAgitationEvents * 1000.0f) > uTime)
	{
		return;
	}

	//Become agitated at the driver.
	CEventAgitated event(rVehicle.GetDriver(), AT_RevvedAt);
	AddEvent(event);

	//Update the last time a rev agitated us.
	m_uLastTimeRevAgitatedUs = uTime;
}

void CPedIntelligence::KnockedToGround(const CEntity* pEntity)
{
	//Set the last entity that knocked us to the ground.
	m_pLastEntityThatKnockedUsToTheGround = pEntity;
	
	//Set the time that we were last knocked to the ground.
	m_uLastTimeWeWereKnockedToTheGround = fwTimer::GetTimeInMilliseconds();
}

void CPedIntelligence::RammedInVehicle(const CPed* pPed)
{
	//Set the last ped that rammed us in vehicle.
	m_pLastPedThatRammedUsInVehicle = pPed;

	//Set the time that we were last rammed in vehicle.
	m_uLastTimeWeWereRammedInVehicle = fwTimer::GetTimeInMilliseconds();
}

const CAmbientAudio* CPedIntelligence::GetLastAudioSaidToPed(const CPed& rPed) const
{
	//Ensure we last talked to the ped.
	if(m_TalkedToPedInfo.m_pPed != &rPed)
	{
		return NULL;
	}

	return &m_TalkedToPedInfo.m_Audio;
}

bool CPedIntelligence::HasRecentlyTalkedToPed(const CPed& rPed, float fMaxTime) const
{
	//Ensure we last talked to the ped.
	if(m_TalkedToPedInfo.m_pPed != &rPed)
	{
		return false;
	}

	//Ensure we talked to the ped within the max time.
	if(m_TalkedToPedInfo.m_uTime + (u32)(fMaxTime * 1000.0f) < fwTimer::GetTimeInMilliseconds())
	{
		return false;
	}

	return true;
}

bool CPedIntelligence::HasRecentlyTalkedToPed(const CPed& rPed, float fMaxTime, const CAmbientAudio& rAudio) const
{
	//Ensure we have recently talked to the ped.
	if(!HasRecentlyTalkedToPed(rPed, fMaxTime))
	{
		return false;
	}

	//Ensure we last said the audio.
	if(rAudio.GetPartialHash() != m_TalkedToPedInfo.m_Audio.GetPartialHash())
	{
		return false;
	}

	return true;
}

void CPedIntelligence::UpdatePedDecisionMaker(bool bFullUpdate)
{
	aiAssert(m_pPed);
	m_PedDecisionMaker.Update(*m_pPed, bFullUpdate);
}

void CPedIntelligence::Bumped(const CEntity& rEntity, float fEntitySpeedSq)
{
	//Check if we are still.
	static dev_float s_fMaxSpeed = 0.5f;
	bool bIsStill = (m_pPed->GetVelocity().Mag2() < square(s_fMaxSpeed));
	if(bIsStill)
	{
		m_uLastTimeBumpedWhenStill = fwTimer::GetTimeInMilliseconds();
	}

	//Check if the other entity is a ped.
	if(rEntity.GetIsTypePed())
	{
		//Check if the ped is a player.
		const CPed& rPed = static_cast<const CPed &>(rEntity);
		if(rPed.IsPlayer())
		{
			//Get the speed.
			if(fEntitySpeedSq < 0.0f)
			{
				fEntitySpeedSq = rPed.GetVelocity().Mag2();
			}

			//Check if the ped is not still.
			static dev_float s_fMinSpeed = 0.1f;
			float fMinSpeedSq = square(s_fMinSpeed);
			if(fEntitySpeedSq >= fMinSpeedSq)
			{
				//Note that this ped was bumped by a player.
				m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BumpedByPlayer, true);

				//Let the ambient friend know that their buddy was bumped by a player.
				CPed* pAmbientFriend = m_pPed->GetPedIntelligence()->GetAmbientFriend();
				if (pAmbientFriend)
				{
					pAmbientFriend->SetPedConfigFlag(CPED_CONFIG_FLAG_AmbientFriendBumpedByPlayer, true);
				}

				if(!m_pPed->IsDead())
				{
					CPlayerSpecialAbilityManager::ChargeEvent(ACET_BUMPED_INTO_PED, const_cast<CPed*>(&rPed), 0.f, 0, this);
				}
			}
		}
	}
	//Check if the other entity is a vehicle.
	else if(rEntity.GetIsTypeVehicle())
	{
		//Check if the driver is a player.
		const CVehicle& rVehicle = static_cast<const CVehicle &>(rEntity);
		const CPed* pDriver = rVehicle.GetDriver();
		if(pDriver && pDriver->IsPlayer())
		{
			//Get the speed.
			if(fEntitySpeedSq < 0.0f)
			{
				fEntitySpeedSq = rVehicle.GetVelocity().Mag2();
			}

			//Check if the vehicle is not still.
			static dev_float s_fMinSpeed = 0.2f;
			float fMinSpeedSq = square(s_fMinSpeed);
			if(fEntitySpeedSq >= fMinSpeedSq)
			{
				//Note that the player bumped the ped.
				pDriver->GetPlayerInfo()->BumpedInVehicle(*m_pPed);

				//Note that this ped was bumped by a player vehicle.
				m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BumpedByPlayerVehicle, true);

				//Let the ambient friend know that their buddy was bumped by a player.
				CPed* pAmbientFriend = m_pPed->GetPedIntelligence()->GetAmbientFriend();
				if (pAmbientFriend)
				{
					pAmbientFriend->SetPedConfigFlag(CPED_CONFIG_FLAG_AmbientFriendBumpedByPlayerVehicle, true);
				}
			}
		}
	}
}

void CPedIntelligence::Dodged(const CEntity& rEntity, float fEntitySpeedSq)
{
	//Check if the other entity is a ped.
	if(rEntity.GetIsTypePed())
	{
		//Check if the ped is a player.
		const CPed& rPed = static_cast<const CPed &>(rEntity);
		if(rPed.IsPlayer())
		{
			//Get the speed.
			if(fEntitySpeedSq < 0.0f)
			{
				fEntitySpeedSq = rPed.GetVelocity().Mag2();
			}

			//Check if the ped is not still.
			static dev_float s_fMinSpeed = 0.1f;
			float fMinSpeedSq = square(s_fMinSpeed);
			if(fEntitySpeedSq >= fMinSpeedSq)
			{
				//Note that this ped dodged a player.
				m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DodgedPlayer, true);
			}
		}
	}
	//Check if the other entity is a vehicle.
	else if(rEntity.GetIsTypeVehicle())
	{
		//Check if the driver is a player.
		const CVehicle& rVehicle = static_cast<const CVehicle &>(rEntity);
		const CPed* pDriver = rVehicle.GetDriver();
		if(pDriver && pDriver->IsPlayer())
		{
			//Get the speed.
			if(fEntitySpeedSq < 0.0f)
			{
				fEntitySpeedSq = rVehicle.GetVelocity().Mag2();
			}

			//Check if the vehicle is not still.
			static dev_float s_fMinSpeed = 0.2f;
			float fMinSpeedSq = square(s_fMinSpeed);
			if(fEntitySpeedSq >= fMinSpeedSq)
			{
				//Note that this ped dodged a player vehicle.
				m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DodgedPlayerVehicle, true);
			}
		}
	}
}

void CPedIntelligence::Evaded(CEntity* pEntity)
{
	//Set the last entity evaded.
	m_pLastEntityEvaded = pEntity;

	//Set the time that we last evaded.
	m_uLastTimeEvaded = fwTimer::GetTimeInMilliseconds();
}

void CPedIntelligence::CollidedWithPlayer()
{
	//Set the time that we last collided with the player.
	m_uLastTimeCollidedWithPlayer = fwTimer::GetTimeInMilliseconds();
}

void CPedIntelligence::TriedToSayAudioForDamage()
{
	//Set the time.
	m_uLastTimeTriedToSayAudioForDamage = fwTimer::GetTimeInMilliseconds();
}

void CPedIntelligence::CalledPolice()
{
	//Set the time.
	//@@: location CPEDINTELLIGENCE_CALLEDPOLICE
	m_uLastTimeCalledPolice = fwTimer::GetTimeInMilliseconds();
}

u32 CPedIntelligence::GetCurrentTintIndexForParachute() const
{
	s32 sIndex = 0;

	#if __BANK || __ASSERT
	static const u32 nMaxTints = 16;
	#endif

	//Use network value if we don't own the ped
	if(m_pPed->IsNetworkClone())
	{
		sIndex = GetTintIndexForParachute();
	}
	else
	{
		s32 sOverrideTintIndex = 0;

		//! Clones will sync the correct tint index depending on this flag being set on a local ped.
		if(m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute))
		{
			sOverrideTintIndex = GetTintIndexForReserveParachute();
		}
		else
		{
			sOverrideTintIndex = GetTintIndexForParachute();
		}

#if __BANK
		TUNE_GROUP_BOOL(TASK_PARACHUTE, bOverrideTintIndexForParachute, false);
		if (bOverrideTintIndexForParachute)
		{
			TUNE_GROUP_INT(TASK_PARACHUTE, uOverrideTintIndexForParachute, 0, -1, nMaxTints, 1);
			sOverrideTintIndex = (u32)uOverrideTintIndexForParachute;
		}
#endif

		if(sOverrideTintIndex >= 0)
		{
			sIndex = sOverrideTintIndex;
		}
		else
		{
			//Randomise in single player only
			bool bRandomize = (!NetworkInterface::IsGameInProgress() && !CTheScripts::GetPlayerIsOnAMission());
			if(bRandomize)
			{
				sIndex = (u32)fwRandom::GetRandomNumberInRange(0, 8);
			}
		}
	}

	//Ensure index is >= 0. I.e. -1 defaults to 0.
	u32 uIndex = (u32)MAX(0, sIndex);

	pedAssertf(uIndex < nMaxTints, "Tint index out of range. %d", uIndex);

	return uIndex;
}

///////////////////////////////////////////////////
// CLASS: CPedEventDecisionMaker
// PURPOSE: Ped decision making instance data
///////////////////////////////////////////////////

//
bool CPedEventDecisionMaker::IsEventResponseDisabled(const eEventType eventType) const
{
	const SResponse* pPedResponse = FindResponse(eventType);
	if (pPedResponse)
	{
		return !pPedResponse->HasCooldownExpired();
	}

	return false;
}

//
void CPedEventDecisionMaker::OnDecisionMade(const CEvent& rEvent, const CEventResponseTheDecision& rDecision)
{
	const CEventDecisionMakerResponse* const pResponse = rDecision.GetResponse();
	if (!aiVerify(pResponse))
	{
		return;
	}

	// Calculate cooldown
	const CEventDecisionMakerResponse::sCooldown& rResponseCooldown = pResponse->GetCooldown();
	if (rResponseCooldown.Time > 0.0f)
	{
		SResponse* pPedResponse = FindResponse(pResponse->GetEventType());
		if (pPedResponse)
		{
			pPedResponse->vEventPosition = rEvent.GetEntityOrSourcePosition();
			pPedResponse->uEventTS = fwTimer::GetTimeInMilliseconds();
			pPedResponse->Cooldown = rResponseCooldown;
		}
		else
		{
			AddResponse(pResponse->GetEventType(), rResponseCooldown, rEvent.GetEntityOrSourcePosition(), fwTimer::GetTimeInMilliseconds());
		}
	}
}

//
void CPedEventDecisionMaker::Update(const CPed& rPed, bool bFullUpdate)
{
	if (bFullUpdate)
	{
		UpdateResponses(rPed);
	}
}

//
void CPedEventDecisionMaker::UpdateResponses(const CPed& rPed)
{
	const Vec3V vPedPosition = rPed.GetTransform().GetPosition();
	const eEventType ePedCurrentEventType = static_cast<eEventType>(rPed.GetPedIntelligence()->GetCurrentEventType());

	u32 uNumResponses = m_aResponses.GetCount();
	for (u32 uResponseIdx = 0; uResponseIdx < uNumResponses; )
	{
		SResponse& rPedResponse = m_aResponses[uResponseIdx];

		const bool bRemoveResponse = rPedResponse.HasCooldownExpired() || IsGreaterThanAll(DistSquared(vPedPosition, rPedResponse.vEventPosition), ScalarV(rage::square(rPedResponse.Cooldown.MaxDistance)));
		if (!bRemoveResponse)
		{
			if (ePedCurrentEventType == rPedResponse.eventType)
			{
				rPedResponse.uEventTS = fwTimer::GetTimeInMilliseconds();
			}

			++uResponseIdx;
		}
		else
		{
			m_aResponses.DeleteFast(uResponseIdx);
			aiAssert(uNumResponses > 0);
			--uNumResponses;
		}
	}
}

//
CPedEventDecisionMaker::SResponse* CPedEventDecisionMaker::AddResponse(const eEventType eventType, const CEventDecisionMakerResponse::sCooldown& rCooldown, Vec3V_In vPosition, u32 uTS)
{
	aiAssert(!FindResponse(eventType));

	SResponse& rResponse = m_aResponses.Grow();

	rResponse.eventType = eventType;
	rResponse.vEventPosition = vPosition;
	rResponse.uEventTS = uTS;
	rResponse.Cooldown = rCooldown;

	return &rResponse;
}

//
CPedEventDecisionMaker::SResponse* CPedEventDecisionMaker::FindResponse(const eEventType eventType)
{
	const u32 uNumResponses = m_aResponses.GetCount(); 
	for (u32 uResponseIdx = 0; uResponseIdx < uNumResponses; ++uResponseIdx)
	{
		SResponse& rResponse = m_aResponses[uResponseIdx];
		if (rResponse.eventType == eventType)
		{
			return &rResponse;
		}
	}

	return NULL;
}

//
const CPedEventDecisionMaker::SResponse* CPedEventDecisionMaker::FindResponse(const eEventType eventType) const
{
	const u32 uNumResponses = m_aResponses.GetCount(); 
	for (u32 uResponseIdx = 0; uResponseIdx < uNumResponses; ++uResponseIdx)
	{
		const SResponse& rResponse = m_aResponses[uResponseIdx];
		if (rResponse.eventType == eventType)
		{
			return &rResponse;
		}
	}

	return NULL;
}

//
const CEventDecisionMaker* CPedEventDecisionMaker::GetDecisionMaker() const
{
	return CEventDecisionMakerManager::GetDecisionMaker(m_uDecisionMakerId);
}

//
CEventDecisionMaker* CPedEventDecisionMaker::GetDecisionMaker()
{
	return CEventDecisionMakerManager::GetDecisionMaker(m_uDecisionMakerId);
}

#if !__FINAL
//
const char* CPedEventDecisionMaker::GetDecisionMakerName() const
{
	const CEventDecisionMaker* pDecisionMaker = GetDecisionMaker();
	return pDecisionMaker ? pDecisionMaker->GetName() : "null";
}
#endif // __FINAL

//
bool CPedEventDecisionMaker::HandlesEventOfType(const eEventType eventType) const
{
	const CEventDecisionMaker* pDecisionMaker = GetDecisionMaker();
	return pDecisionMaker && pDecisionMaker->HandlesEventOfType(eventType);
}

//
void CPedEventDecisionMaker::MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, CEventResponseTheDecision &theDecision)
{
	if (!aiVerify(pPed && pEvent))
	{
		return;
	}

	const eEventType eventType = static_cast<eEventType>(pEvent->GetEventType());

	if (!IsEventResponseDisabled(eventType))
	{
		const CEventDecisionMaker* pDecisionMaker = GetDecisionMaker();
		if(aiVerifyf(pDecisionMaker, "Ped does not have a valid decision maker"))
		{
			pDecisionMaker->MakeDecision(pPed, pEvent, theDecision);
			const CTaskTypes::eTaskType taskType = (CTaskTypes::eTaskType)theDecision.GetTaskType();
			if(taskType != CTaskTypes::TASK_INVALID_ID)
			{
				OnDecisionMade(*pEvent, theDecision);
			}
		}
	}
}


#if __DEV
void CPedIntelligence::AddScriptHistoryString(const char* pStaticString, const char* pStaticTaskString, const char* scriptName, const s32 iProgramCounter)
{
	// Record the task in the debug list
	m_aScriptHistoryName[m_iCurrentHistroyTop] = pStaticString;
	m_aScriptHistoryTaskName[m_iCurrentHistroyTop] = pStaticTaskString;
	m_aScriptHistoryTime[m_iCurrentHistroyTop] = fwTimer::GetTimeInMilliseconds();
	strncpy(m_szScriptName[m_iCurrentHistroyTop], scriptName, 32);
	m_aProgramCounter[m_iCurrentHistroyTop] = iProgramCounter;
	++m_iCurrentHistroyTop;
	if(m_iCurrentHistroyTop >= MAX_DEBUG_SCRIPT_TASK_HISTORY)
	{
		m_iCurrentHistroyTop = 0;
	}

	m_bNewTaskThisFrame = true;
}
#endif

#if __BANK
void CPedIntelligence::AddWidgets(bkBank& bank)
{
	bank.AddSlider("Battle Awareness Whizby",					&BATTLE_AWARENESS_BULLET_WHIZBY, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Bullet Impact",			&BATTLE_AWARENESS_BULLET_IMPACT, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Gun Shot",					&BATTLE_AWARENESS_GUNSHOT, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Gun Shot Local Player",	&BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Damage",					&BATTLE_AWARENESS_DAMAGE, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Min",						&BATTLE_AWARENESS_MIN, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Max",						&BATTLE_AWARENESS_MAX, 100.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Depletion Time from max",	&BATTLE_AWARENESS_DEPLETION_TIME_FROM_MAX, 1.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Min Time",					&BATTLE_AWARENESS_MIN_TIME, 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Battle Awareness Gun Shot Local Player Cooldown",	&BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER_COOLDOWN, 0, 5000, 100);
}
#endif

// -----------------------------------------------------------------------------
// EVENT HISTORY

#if DEBUG_EVENT_HISTORY

//
const char* CPedIntelligence::SEventHistoryEntry::SState::GetName(Enum state)
{
	static const char* apszStateNames[] =
	{
		"CREATED",
		"DISCARDED_PRIORITY",
		"DISCARDED_CANNOT_INTERRUPT_SAME_EVENT",
		"TRYING_TO_ABORT_TASK",
		"ACCEPTED",
		"REMOVED",
		"DELETED",
		"EXPIRED"
	};

	CompileTimeAssert(sizeof(apszStateNames) / sizeof(const char*) == COUNT);

	if (aiVerify(state < COUNT))
	{
		return apszStateNames[state];
	}

	return "INVALID";
}


CPedIntelligence::SEventHistoryEntry::SEventHistoryEntry()
	: eType(EVENT_INVALID)
	, sSourceDescription("")
	, sEventDescription("")
	, vPosition(V_ZERO)
	, uCreatedTS(0u)
	, uModifiedTS(0u)
	, uProcessedTS(0u)
	//, bProcessedReady(false)
	, bProcesedValidForPed(false)
	//, bProcessedAffectsPed(false)
	, bProcessedCalculatedResponse(false)
	, bProcessedWillRespond(false)
	, uSelectedAsHighestTS(0u)
	, eState(SState::INVALID)
	, fDelayTimerMax(0.0f)
	, fDelayTimerLast(0.0f)
	, pEvent(NULL)
{
}

CPedIntelligence::SEventHistoryEntry::SEventHistoryEntry(const CEvent& rEvent)
	: eType(static_cast<eEventType>(rEvent.GetEventType()))
	, sSourceDescription(AILogging::GetEntityDescription(rEvent.GetSourceEntity()))
	, sEventDescription(rEvent.GetDescription())
	, vPosition(rEvent.GetEntityOrSourcePosition())
	, uCreatedTS(fwTimer::GetFrameCount())
	, uModifiedTS(fwTimer::GetFrameCount())
	, uProcessedTS(0u)
	//, bProcessedReady(false)
	, bProcesedValidForPed(false)
	//, bProcessedAffectsPed(false)
	, bProcessedCalculatedResponse(false)
	, bProcessedWillRespond(false)
	, uSelectedAsHighestTS(0u)
	, eState(SState::CREATED)
	, fDelayTimerMax(rEvent.GetDelayTimer())
	, fDelayTimerLast(rEvent.GetDelayTimer())
	, pEvent(&rEvent)
{
}

void CPedIntelligence::AddToEventHistory(const CEvent& rEvent)
{
	SEventHistoryEntry CurrentEventEntry;
	if (static_cast<size_t>(m_EventHistory.GetCount()) == m_EventHistory.Size())
	{
		SEventHistoryEntry& rFirstEntry = m_EventHistory.Get(0);
		if (rFirstEntry.pEvent && rFirstEntry.pEvent == GetCurrentEvent())
		{
			CurrentEventEntry = rFirstEntry;
		}
	}

	m_EventHistory.Append(SEventHistoryEntry(rEvent));

	if (CurrentEventEntry.pEvent)
	{
		m_EventHistory[0] = CurrentEventEntry;
	}
}

void CPedIntelligence::SetEventHistoryEntryState(const CEvent& rEvent, SEventHistoryEntry::SState::Enum eState)
{
	SEventHistoryEntry* pEntry = FindEventHistoryEntry(rEvent);
	if (pEntry)
	{
		pEntry->eState = eState;
		pEntry->uModifiedTS = fwTimer::GetFrameCount();
		pEntry->fDelayTimerLast = rEvent.GetDelayTimer();
		pEntry->fDelayTimerMax = Max(pEntry->fDelayTimerMax, pEntry->fDelayTimerLast);
	}
}

void CPedIntelligence::SetEventHistoryEntryProcessed(const CEvent& rEvent)
{
	SEventHistoryEntry* pEntry = FindEventHistoryEntry(rEvent);
	if (pEntry)
	{
		pEntry->uProcessedTS = fwTimer::GetFrameCount();
		pEntry->uProcessedTS = fwTimer::GetFrameCount();
		//pEntry->bProcessedReady = rEvent.IsEventReady(m_pPed);
		pEntry->bProcesedValidForPed = rEvent.IsValidForPed(m_pPed);
		//pEntry->bProcessedAffectsPed = pEntry->bProcesedValidForPed ? rEvent.AffectsPed(m_pPed) : false;
		if (rEvent.HasEditableResponse())
		{
			const CEventEditableResponse& rEventEditable = static_cast<const CEventEditableResponse&>(rEvent);
			pEntry->bProcessedCalculatedResponse = rEventEditable.HasCalculatedResponse();
			pEntry->bProcessedWillRespond = rEventEditable.WillRespond();
		}
	}
}

void CPedIntelligence::SetEventHistoryEntrySelectedAsHighest(const CEvent& rEvent)
{
	SEventHistoryEntry* pEntry = FindEventHistoryEntry(rEvent);
	if (pEntry)
	{
		pEntry->uSelectedAsHighestTS = fwTimer::GetFrameCount();
	}
}


void CPedIntelligence::EventHistoryOnEventSetTaskBegin(const CEvent& rEvent)
{
	SEventHistoryEntry* pEntry = FindEventHistoryEntry(rEvent);
	if (pEntry)
	{
		for (u32 uTaskTreeIdx = 0; uTaskTreeIdx < PED_TASK_TREE_MAX; ++uTaskTreeIdx)
		{
			aiTask* pTask = m_taskManager.GetActiveTask(uTaskTreeIdx);
			pEntry->eBeforeAcceptedActiveTaskType[uTaskTreeIdx] = pTask ? static_cast<CTaskTypes::eTaskType>(pTask->GetTaskType()) : CTaskTypes::TASK_NONE;
		}
	}
}

void CPedIntelligence::EventHistoryOnEventSetTaskEnd(const CEvent& rEvent)
{
	SEventHistoryEntry* pEntry = FindEventHistoryEntry(rEvent);
	if (pEntry)
	{
		for (u32 uTaskTreeIdx = 0; uTaskTreeIdx < PED_TASK_TREE_MAX; ++uTaskTreeIdx)
		{
			aiTask* pTask = m_taskManager.GetActiveTask(uTaskTreeIdx);
			pEntry->eAfterAcceptedActiveTaskType[uTaskTreeIdx] = pTask ? static_cast<CTaskTypes::eTaskType>(pTask->GetTaskType()) : CTaskTypes::TASK_NONE;
		}

		CEvent* pCurrentEvent = GetCurrentEvent();
		if (pCurrentEvent && *pCurrentEvent == rEvent)
		{
			pEntry->pEvent = pCurrentEvent;
		}
	}

	SetEventHistoryEntryState(rEvent, SEventHistoryEntry::SState::ACCEPTED);
}

void CPedIntelligence::EventHistoryOnEventRemoved(const CEvent& rEvent)
{
	SetEventHistoryEntryState(rEvent, SEventHistoryEntry::SState::REMOVED);
}

CPedIntelligence::SEventHistoryEntry* CPedIntelligence::FindEventHistoryEntry(const CEvent& rEvent)
{
	const u32 uEventCount = m_EventHistory.GetCount();
	for (u32 uEventIdx = 0; uEventIdx < uEventCount; ++uEventIdx)
	{
		SEventHistoryEntry& rEntry = m_EventHistory[uEventIdx];
		if (rEntry.pEvent == &rEvent)
		{
			return &rEntry;
		}
	}

	return NULL;
}

void CPedIntelligence::EventHistoryMarkExpiredEvents()
{
	const u32 uNumEvents = GetNumEventsInQueue();
	for (u32 uEventIdx = 0; uEventIdx < uNumEvents; ++uEventIdx)
	{
		const CEvent* pEvent = GetEventByIndex(uEventIdx);
		if (AssertVerify(pEvent) && !pEvent->GetBeingProcessed() && pEvent->HasExpired())
		{
			SetEventHistoryEntryState(*pEvent, SEventHistoryEntry::SState::EXPIRED);
		}
	}
}

void CPedIntelligence::EventHistoryMarkProcessedEvents()
{
	const u32 uNumEvents = GetNumEventsInQueue();
	for (u32 uEventIdx = 0; uEventIdx < uNumEvents; ++uEventIdx)
	{
		CEvent* pEvent = GetEventByIndex(uEventIdx);
		if (AssertVerify(pEvent))
		{
			SetEventHistoryEntryProcessed(*pEvent);
		}
	}
}

#endif //DEBUG_EVENT_HISTORY

