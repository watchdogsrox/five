#include "TaskDamageDeath.h"

// Rage Headers
#include "fragmentnm/manager.h"
#include "fragmentnm/messageparams.h"
#include "phbound/boundcomposite.h"
#include "physics/constraintfixedrotation.h"
#include "physics/constraintprismatic.h"
#include "pharticulated/articulatedcollider.h"
#include "vectormath/classfreefuncsv.h"

// Framework headers 
#include "fwanimation/animmanager.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/pointcloud.h"
#include "fwmaths/Angle.h"

// Game Headers
#include "AI/AITarget.h"
#include "Animation/FacialData.h"
#include "animation/MovePed.h"
#include "animation/AnimManager.h"
#include "audio/northaudioengine.h"
#include "audio/superconductor.h"
#include "camera/CamInterface.h"
#include "Debug/DebugScene.h"
#include "Debug/MarketingTools.h"
#include "Event/EventDamage.h"
#include "Event/EventGroup.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "game/Dispatch/IncidentManager.h"
#include "game/Dispatch/Incidents.h"
#include "game/Dispatch/Orders.h"
#include "Game/Localisation.h"
#include "ik/solvers/RootSlopeFixupSolver.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDefines.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "Physics/RagdollConstraints.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Pickups/PickupManager.h"
#include "Renderer/Water.h"
#include "Scene/Entity.h"
#include "scene/world/GameWorld.h"
#include "system/TamperActions.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/TaskAnimatedFallback.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Combat/TaskAnimatedHitByExplosion.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMElectrocute.h"
#include "Task/Physics/TaskNMInjuredOnGround.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMSimple.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Response/TaskFlee.h"
#include "Task/System/TaskTypes.h"
#include "vehicleAI/task/TaskVehicleDeadDriver.h"
#include "vehicleAI/VehicleIntelligence.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Systems/VfxBlood.h"
#include "weapons/Weapon.h"
#include "Network/Network.h"
#include "Network/General/NetworkStreamingRequestManager.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskDyingDead - combined dying and death task replaces individual dead and dying tasks
//////////////////////////////////////////////////////////////////////////

CTaskDyingDead::Tunables CTaskDyingDead::ms_Tunables;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskDyingDead, 0x57d30c0e);

//requests
fwMvRequestId CTaskDyingDead::ms_DyingAnimatedId("DyingAnimated",0x9482433);
fwMvRequestId CTaskDyingDead::ms_DyingRagdollId("DyingRagdoll",0xBFD8A870);
fwMvRequestId CTaskDyingDead::ms_DeadAnimatedId("DeadAnimated",0xA5B4DDAE);
fwMvRequestId CTaskDyingDead::ms_DeadRagdollId("DeadRagdoll",0x8B71C988);
fwMvRequestId CTaskDyingDead::ms_DeadRagdollFrameId("DeadRagdollFrame",0x840D1BB8);

// state enter events
fwMvBooleanId CTaskDyingDead::ms_DyingAnimatedEnterId("DyingAnimated_OnEnter",0x40326553);
fwMvBooleanId CTaskDyingDead::ms_DyingRagdollEnterId("DyingRagdoll_OnEnter",0xB3253F67);
fwMvBooleanId CTaskDyingDead::ms_DeadAnimatedEnterId("DeadAnimated_OnEnter",0xC8AAAD8A);
fwMvBooleanId CTaskDyingDead::ms_DeadRagdollEnterId("DeadRagdoll_OnEnter",0xCBE0DFBE);
fwMvBooleanId CTaskDyingDead::ms_DeadRagdollFrameEnterId("DeadRagdollFrame_OnEnter",0x4F44A959);

//other events
fwMvBooleanId CTaskDyingDead::ms_HornAudioOnId("HornAudioOn",0x83ECC144);
fwMvBooleanId CTaskDyingDead::ms_DyingClipEndedId("DyingClipEnded",0x53EDA87A);
fwMvBooleanId CTaskDyingDead::ms_DropWeaponRnd("DropWeaponRnd",0x6B9E7557);
fwMvClipId	  CTaskDyingDead::ms_DeathClipId("DeathClip",0xCDCBA16E);
fwMvClipId	  CTaskDyingDead::ms_UpperBodyReactionClipId("UpperBodyReactionClip",0x2F455599);
fwMvRequestId CTaskDyingDead::ms_UpperBodyReactionId("UpperBodyReaction",0xC01DD784);
fwMvRequestId CTaskDyingDead::ms_ReactToDamageId("ReactToDamage",0x455B1B57);
fwMvFloatId	  CTaskDyingDead::ms_DeathClipPhaseId("DeathClipPhase",0xC89034F0);
fwMvFloatId	  CTaskDyingDead::ms_DyingClipStartPhaseId("DyingClipStartPhase",0xC61C8A41);

float CTaskDyingDead::ms_fAbortAnimatedDyingFallWaterLevel = 0.5f;
u32 CTaskDyingDead::ms_iLastVehicleHornTime = 0;
bool CTaskDyingDead::ms_bForceFixMatrixOnDeadRagdollFrame = false;
static const unsigned int SIZEOF_SOURCE_WEAPON  = 32;
static const unsigned int SIZEOF_HIT_DIRECTION	= 8;
static const unsigned int SIZEOF_HIT_BONE		= 16;
static const unsigned int MIN_HORN_REPEAT_TIME  = 20000;

//////////////////////////////////////////////////////////////////////////
// CDeathSourceInfo
//////////////////////////////////////////////////////////////////////////
CDeathSourceInfo::CDeathSourceInfo() :
m_entity(NULL),
m_weapon(0),
m_fallDirection(DEFAULT_FALL_DIRECTION),
m_fallContext(DEFAULT_FALL_CONTEXT)
{
}

CDeathSourceInfo::CDeathSourceInfo(CEntity* pEntity, u32 weapon) :
m_entity(pEntity),
m_weapon(weapon),
m_fallDirection(DEFAULT_FALL_DIRECTION),
m_fallContext(DEFAULT_FALL_CONTEXT)
{
	if (weapon != 0)
	{
		m_fallContext = CTaskFallOver::ComputeFallContext(weapon, 0);
	}
}

CDeathSourceInfo::CDeathSourceInfo(CEntity* pEntity, u32 weapon, CTaskFallOver::eFallContext fallContext, CTaskFallOver::eFallDirection fallDirection) :
m_entity(pEntity),
m_weapon(weapon),
m_fallContext(fallContext),
m_fallDirection(fallDirection)
{
}

CDeathSourceInfo::CDeathSourceInfo(CEntity* pEntity, u32 weapon, float fDirection, int boneTag) :
m_entity(pEntity),
m_weapon(weapon),
m_fallContext(DEFAULT_FALL_CONTEXT),
m_fallDirection(DEFAULT_FALL_DIRECTION)
{
	fDirection += QUARTER_PI;
	if(fDirection < 0.0f) fDirection += TWO_PI;
	int nHitDirn = int(fDirection / HALF_PI);

	switch(nHitDirn)
	{
	case CEntityBoundAI::FRONT:		
		m_fallDirection = CTaskFallOver::kDirFront;
		break;
	case CEntityBoundAI::LEFT:
		m_fallDirection = CTaskFallOver::kDirLeft;
		break;
	case CEntityBoundAI::REAR:
		m_fallDirection = CTaskFallOver::kDirBack;
		break;
	case CEntityBoundAI::RIGHT:
		m_fallDirection = CTaskFallOver::kDirRight;
		break;
	}

	m_fallContext = CTaskFallOver::ComputeFallContext(weapon, boneTag);
}

CDeathSourceInfo::CDeathSourceInfo(const CDeathSourceInfo& info) :
m_entity(info.m_entity),
m_weapon(info.m_weapon),
m_fallContext(info.m_fallContext),
m_fallDirection(info.m_fallDirection)
{
}

void CDeathSourceInfo::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_FALL_DIRECTION = datBitsNeeded<CTaskFallOver::kNumFallDirections-1>::COUNT;
	static const unsigned SIZEOF_FALL_CONTEXT = datBitsNeeded<CTaskFallOver::kNumFallContexts-1>::COUNT;

	bool bHasSourceEntity	= m_entity.GetEntityID() != NETWORK_INVALID_OBJECT_ID;
	bool bHasSourceWeapon	= m_weapon != 0;
	bool bHasFallContext    = m_fallContext != DEFAULT_FALL_CONTEXT;
	bool bHasFallDirection  = m_fallDirection != DEFAULT_FALL_DIRECTION;

	SERIALISE_BOOL(serialiser, bHasSourceEntity);
	SERIALISE_BOOL(serialiser, bHasSourceWeapon);
	SERIALISE_BOOL(serialiser, bHasFallContext);
	SERIALISE_BOOL(serialiser, bHasFallDirection);

	if (bHasSourceEntity || serialiser.GetIsMaximumSizeSerialiser())
	{
		m_entity.Serialise(serialiser, "Source Entity");
	}

	if (bHasSourceWeapon || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_weapon, SIZEOF_SOURCE_WEAPON, "Source Weapon");
	}
	else
	{
		m_weapon = 0;
	}

	if (bHasFallContext || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, (u32&)m_fallContext, SIZEOF_FALL_CONTEXT, "Fall context");
	}
	else
	{
		m_fallContext = DEFAULT_FALL_CONTEXT;
	}

	if (bHasFallDirection || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, (u32&)m_fallDirection, SIZEOF_FALL_DIRECTION, "Fall Direction");
	}
	else
	{
		m_fallDirection = DEFAULT_FALL_DIRECTION;
	}
}

void CDeathSourceInfo::ComputeFallDirection(CPed& ped)
{
	if (GetEntity())
	{
		Vector3 vTempDir = VEC3V_TO_VECTOR3(GetEntity()->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	
		float fHitDir =  rage::Atan2f(-vTempDir.x, vTempDir.y);
		fHitDir -= ped.GetTransform().GetHeading();
		fHitDir =  fwAngle::LimitRadianAngle(fHitDir);

		fHitDir += QUARTER_PI;
		if(fHitDir < 0.0f) fHitDir += TWO_PI;
		int nHitDirn = int(fHitDir / HALF_PI);

		switch(nHitDirn)
		{
		case CEntityBoundAI::FRONT:		
			m_fallDirection = CTaskFallOver::kDirFront;
			break;
		case CEntityBoundAI::LEFT:
			m_fallDirection = CTaskFallOver::kDirLeft;
			break;
		case CEntityBoundAI::REAR:
			m_fallDirection = CTaskFallOver::kDirBack;
			break;
		case CEntityBoundAI::RIGHT:
			m_fallDirection = CTaskFallOver::kDirRight;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CTaskDyingDead
//////////////////////////////////////////////////////////////////////////

CTaskDyingDead::CTaskDyingDead( const CDeathSourceInfo* pSourceInfo, s32 iFlags, u32 timeOfDeath )
: m_flags(iFlags)
, m_deathSourceInfo(pSourceInfo ? *pSourceInfo : NULL)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_fStartPhase(0.0f)
, m_fBlendDuration(NORMAL_BLEND_DURATION)
, m_pForcedNaturalMotionTask(NULL)
, m_iTimeOfDeath(timeOfDeath)
, m_tProcessBloodPoolTimer(0)
, m_bloodPoolStartSize(0.0f)
, m_bloodPoolEndSize(0.0f)
, m_bloodPoolGrowRate(0.0f)
, m_fRagdollStartPhase(0.0f)
, m_QuitTask(false)
, m_bUseRagdollDuringClip(false)
, m_bCanAnimatedDeadFall(false)
, m_bPlayedHornAudio(false)
, m_fDropWeaponPhase(0.f)
, m_fRootSlopeFixupPhase(-1.0f)
, m_fFallTime(0.0f)
, m_iDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_SnapToGroundStage(kSnapNotBegun)
, m_iDynamicObjectLastUpdateTime(0)
, m_NetworkStartTime(fwTimer::GetSystemTimeInMilliseconds())
, m_iExitPointForVehicleFallOut(-1)
, m_fRagdollAbortVelMag(0.0f)
, m_bDontUseRagdollFrameDueToInVehicleDeath(false)
, m_clonePedLastSignificantBoneTag(-1)
, m_PhysicalGroundHandle(phInst::INVALID_INDEX, 0)
{
	taskAssertf(!m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash) || !m_flags.IsFlagSet(Flag_clipsOverridenByDictHash), "Cant have both clip flags set!" );

	taskDisplayf("[%d] CTaskDyingDead::CTaskDyingDead: [%p] \n", 
		CNetwork::GetNetworkTime(),
		this);

#if __ASSERT
	if (NetworkDebug::DebugPedMissingDamageEvents())
		sysStack::PrintStackTrace();
#endif

	SetInternalTaskType(CTaskTypes::TASK_DYING_DEAD);
}

//////////////////////////////////////////////////////////////////////////

CTaskDyingDead::~CTaskDyingDead()
{
	taskDisplayf("[%d] CTaskDyingDead::~CTaskDyingDead: [Ped=%s] [%p] [run as clone = %s]\n", 
		CNetwork::GetNetworkTime(), 
		(GetEntity() && GetPed()->GetNetworkObject()) ? GetPed()->GetNetworkObject()->GetLogName() : "No Name", 
		this,
		GetRunAsAClone() ? "true" : "false");

	// Stop live peds from sleeping
	if(GetEntity() && GetPed()->GetDeathState() != DeathState_Dead)
	{
		DisableSleep(GetPed());
	}

	if( m_pForcedNaturalMotionTask )
		delete m_pForcedNaturalMotionTask;

	if(m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_NONE && m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
	{
		CPathServerGta::RemoveBlockingObject(m_iDynamicObjectIndex);
		m_iDynamicObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::IsInScope(const CPed* pPed)
{
	// can't rag doll because geometry isn't streamed in so don't clone.....
	if(pPed->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION))
	{
		return false;
	}

	return true;
}

bool CTaskDyingDead::CanBeReplacedByCloneTask(u32 UNUSED_PARAM(taskType)) const
{
	// locally started dead dying tasks must remain
	return !WasLocallyStarted();
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::AddToStateHistory(DyingDeadState BANK_ONLY(state))
{
#if __BANK
	if (m_StateHistory.GetCount() > 0 && m_StateHistory.End().m_state == state)
	{
		return;
	}
	else if (m_StateHistory.IsFull())
	{
		m_StateHistory.Drop();
	}
	StateData newState;
	newState.m_state = state;
	newState.m_startTimeInStateMS = static_cast<u16>(fwTimer::GetTimeInMilliseconds());
	m_StateHistory.Push(newState);
#endif
}

//////////////////////////////////////////////////////////////////////////

aiTask* CTaskDyingDead::Copy() const
{
	CTaskDyingDead* pTask = rage_new CTaskDyingDead(&m_deathSourceInfo, m_flags);

	if (!m_flags.IsFlagSet(Flag_DeathAnimSetInternally))
	{
		if(m_flags.IsFlagSet( Flag_clipsOverridenByClipSetHash))
		{
			if( taskVerifyf(m_clipSetId!=CLIP_SET_ID_INVALID&&m_clipId!=CLIP_ID_INVALID, "Flag_clipsOverridenByDictIndex set but dictionary index and cliphash not set!") )
			{
				pTask->SetDeathAnimationBySet(fwMvClipSetId(m_clipSetId), fwMvClipId(m_clipId));
			}
		}
		else if(m_flags.IsFlagSet( Flag_clipsOverridenByDictHash))
		{
			if( taskVerifyf(m_clipSetId!=CLIP_SET_ID_INVALID&&m_clipId!=CLIP_ID_INVALID, "Flag_clipsOverridenByDictIndex set but dictionary index and cliphash not set!") )
			{
				pTask->SetDeathAnimationByDictionary(fwAnimManager::FindSlotFromHashKey(m_clipSetId).Get(), m_clipId);
			}
		}
	}

	if(m_flags.IsFlagSet(Flag_naturalMotionTaskSpecified))
	{
		if( taskVerifyf(m_pForcedNaturalMotionTask, "Natural motion task specified but task pointer NULL!") )
		{
			pTask->SetForcedNaturalMotionTask(m_pForcedNaturalMotionTask->Copy());
		}
	}
		
	pTask->m_iTimeOfDeath = m_iTimeOfDeath;

	return pTask;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::CleanUp()
{
	taskDisplayf("[%d] CTaskDyingDead::CleanUp: [Ped=%s] [%p] \n", 
		CNetwork::GetNetworkTime(), 
		(GetEntity() && GetPed()->GetNetworkObject()) ? GetPed()->GetNetworkObject()->GetLogName() : "No Name", 
		this);

	CPed* pPed = GetPed();
	
	pPed->SetUseExtractedZ(false);

	// death state is synced data so we need to wait for the next update
	if(!pPed->IsNetworkClone())
	{
		pPed->SetDeathState(DeathState_Alive);
	}

	if (m_flags.IsFlagSet(Flag_cloneLocalSwitch))
	{
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}
	else
	{
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, IN_VEHICLE_NETWORK_BLEND_OUT, NORMAL_BLEND_DURATION, 0.0f, 1.0f, 0.01f);
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, (pPed->GetIsInVehicle() && pPed->IsNetworkClone()) ? IN_VEHICLE_NETWORK_BLEND_OUT : INSTANT_BLEND_DURATION);
	}

	if (!m_flags.IsFlagSet(Flag_cloneLocalSwitch))
	{
		// SINGLE PLAYER CLEANUP. If Flag_cloneLocalSwitch is set then this CleanUp is getting called on a ped that is migrating in a network
		// game and will have the dead dying task restarted immediately after this.

		if (!pPed->GetUsingRagdoll())
		{
			// We're likely coming from the ragdoll frame state, in which case we need to fix our ped matrix.

			pPed->GetMovePed().SwitchToAnimated(0.0f);

			Matrix34 rootMatrix;
			pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);

			// get orientation from the root matrix too
			float fSwitchHeading = rage::Atan2f(-rootMatrix.b.x, rootMatrix.b.y);
			fSwitchHeading -= HALF_PI;
			rootMatrix.MakeRotateZ(fSwitchHeading);
			pPed->GetMotionData()->SetDesiredHeading(fSwitchHeading);
			pPed->GetMotionData()->SetCurrentHeading(fSwitchHeading);

			WorldProbe::CShapeTestFixedResults<> probeResults;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(rootMatrix.d, rootMatrix.d - Vector3(0.0f, 0.0f, 1.2f));
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
			probeDesc.SetExcludeEntity(pPed);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
				rootMatrix.d = probeResults[0].GetHitPosition() + Vector3(0.0f, 0.0f, pPed->GetCapsuleInfo()->GetGroundToRootOffset());

			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule, false);

			pPed->SetMatrix(rootMatrix, false, false, false);

			// Ensure that the slope fixup IK solver is cleaned up!
			CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
			if (pSolver != NULL)
			{
				pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
			}
		}

		phInst* pAnimatedInst = pPed->GetAnimatedInst();
		if (pAnimatedInst && pAnimatedInst->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE))
		{
			//This flag gets turned on by the 
			pAnimatedInst->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

			// If allowing the animated instance to activate ensure a bare minimum set of instance include flags is used to avoid falling through the map
			if (pAnimatedInst->IsInLevel() && CPhysics::GetLevel()->GetInstanceIncludeFlags(pAnimatedInst->GetLevelIndex()) == 0 &&
                 !pPed->GetIsAttached())
			{
				CPhysics::GetLevel()->SetInstanceIncludeFlags(pAnimatedInst->GetLevelIndex(), ArchetypeFlags::GTA_ALL_MAP_TYPES);
			}
		}
	}

	if (pPed->GetRagdollInst())
	{
		pPed->GetRagdollInst()->SetBlockNMActivation(false);

		// Ragdoll LOD switching is currently only supported on inactive insts.  Should we switch to animated here?
		if (PHLEVEL->IsInactive(pPed->GetRagdollInst()->GetLevelIndex()))
		{
			pPed->GetRagdollInst()->SetCurrentPhysicsLOD(fragInstNM::RAGDOLL_LOD_HIGH);
		}
	}

	if (pPed->GetFacialData())
		pPed->GetFacialData()->SetFacialIdleMode(pPed, CFacialDataComponent::kModeAnimated);

	// make sure we can't leave the ragdoll active if nothing is going to take control of it
	if (pPed->IsNetworkClone() || !m_flags.IsFlagSet(Flag_cloneLocalSwitch))
	{
		CTaskNMControl::CleanupUnhandledRagdoll(pPed);
	}

	if(m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_NONE && m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
	{
		CPathServerGta::RemoveBlockingObject(m_iDynamicObjectIndex);
		m_iDynamicObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
	}

	if (pPed->IsAPlayerPed() && pPed->GetNetworkObject())
	{
		static_cast<CNetObjPed*>(pPed->GetNetworkObject())->SetTaskOverridingPropsWeapons(false);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if( m_flags.IsFlagSet(Flag_disableRagdollActivationFromPlayerImpact))
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true );

	pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

	if (pPed->IsNetworkClone() && pPed->IsAPlayerPed() && !pPed->GetUsingRagdoll())
	{
		pPed->ResetDesiredMainMoverCapsuleData(true);
	}

	//pPed->SetIsOnGround( true );

	if ( GetState() > State_StreamAssets )
	{
		if (( GetState() != State_FallOutOfVehicle && (!pPed->GetIsInVehicle() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat))) &&
			!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && !( GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_CONTROL))
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Dead);
		}
	}

	if(IsInDyingState() || IsInDeadState())
	{
		// reset footstephelper flags 
		pPed->GetFootstepHelper().ResetFlags();
		pPed->GetFootstepHelper().ResetAirFlags();
		pPed->GetFootstepHelper().ResetTimeInTheAir();
	}

	// Play the "dead" idle animation when they are dying, so that the "die" one-shot can blend into it
	if (IsInDyingState() && pPed->IsLocalPlayer() && pPed->GetFacialData())
	{
		u32 nLastWeaponDamageHash = pPed->GetCauseOfDeath();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( nLastWeaponDamageHash );
		eDamageType damageType = pWeaponInfo ? pWeaponInfo->GetDamageType() : DAMAGE_TYPE_UNKNOWN;
		if( damageType != DAMAGE_TYPE_MELEE && damageType != DAMAGE_TYPE_ELECTRIC )
		{
			pPed->GetFacialData()->RequestFacialIdleClip(CFacialDataComponent::FICT_Dead);
		}
	}

	if (IsInDeadState())
	{
		// force disable leg ik
		pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
		pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_OFF);

		// Set a dead facial clip
		CFacialDataComponent* pFacialData = pPed->GetFacialData();
		if(pFacialData)
		{
			u32 nLastWeaponDamageHash = pPed->GetCauseOfDeath();
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( nLastWeaponDamageHash );
			eDamageType damageType = pWeaponInfo ? pWeaponInfo->GetDamageType() : DAMAGE_TYPE_UNKNOWN;
			if( damageType == DAMAGE_TYPE_MELEE || damageType == DAMAGE_TYPE_ELECTRIC )
				pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_KnockedOut);
			else
				pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Dead);
		}

		// Enable AI timeslicing, unconditionally - even if we are close to the dead
		// bodies, there probably is no reason to make them update on every frame.
		const int state = GetState();
		if(state == State_DeadAnimated || state == State_DeadRagdollFrame)
		{
			CPedAILod& lod = pPed->GetPedAiLod();
			lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
			lod.SetUnconditionalTimesliceIntelligenceUpdate(true);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::ProcessPostPreRender()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (GetState() == State_DyingAnimated)
	{
		const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pSolver != NULL && pSolver->GetBlendRate() >= 0.0f)
		{
			pPed->OrientBoundToSpine();
		}
		else
		{
			pPed->ClearBound();
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::ProcessMoveSignals()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// MN - moved this here (instead of ProcessPostPreRender) so that this doesn't get time-sliced
	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CreatedBloodPoolTimer ))
	{
		if ( !m_tProcessBloodPoolTimer.Tick() )
		{
			ProcessBloodPool(pPed);
		}
	}

	// Needs to be called each frame in order to maintain horn audio on if specified
	CheckForDeadInCarAudioEvents(GetPed());
	return true;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		// Stream the in vehicle death clip
		FSM_State(State_StreamAssets)
			FSM_OnEnter
				StreamAssets_OnEnter(pPed);
			FSM_OnUpdate
				return StreamAssets_OnUpdate(pPed);

		// Dying using the death clip
		FSM_State(State_DyingAnimated)
			FSM_OnEnter
				DyingAnimated_OnEnter(pPed);
			FSM_OnUpdate
				return DyingAnimated_OnUpdate(pPed);
			FSM_OnExit
				DyingAnimated_OnExit(pPed);

		// Dying / dead animated but falling through the air
		FSM_State(State_DyingAnimatedFall)
			FSM_OnEnter
				DyingAnimatedFall_OnEnter(pPed);
			FSM_OnUpdate
				return DyingAnimatedFall_OnUpdate(pPed);
			FSM_OnExit
				DyingAnimatedFall_OnExit(pPed);

		// Dying using ragdoll
		FSM_State(State_DyingRagdoll)
			FSM_OnEnter
				DyingRagdoll_OnEnter(pPed);
			FSM_OnUpdate
				return DyingRagdoll_OnUpdate(pPed);

		// Animated death - holds the last frame of the death clip
		FSM_State(State_DeadAnimated)
			FSM_OnEnter
				DeadAnimated_OnEnter(pPed);
			FSM_OnUpdate
				return DeadAnimated_OnUpdate(pPed);
			FSM_OnExit
				DeadAnimated_OnExit(pPed);

		// Used when the ragdoll is aborted without settling
		// Posematches to an appropriate dead pose / dying animation
		FSM_State(State_RagdollAborted)
				FSM_OnEnter
				RagdollAborted_OnEnter(pPed);
			FSM_OnUpdate
				return RagdollAborted_OnUpdate(pPed);
			FSM_OnExit
				RagdollAborted_OnExit(pPed);

		// Ragdoll death, ragdoll is active, will switch to a ragdoll frame once it has settled
		FSM_State(State_DeadRagdoll)
			FSM_OnEnter
				DeadRagdoll_OnEnter(pPed);
			FSM_OnUpdate
				return DeadRagdoll_OnUpdate(pPed);
			FSM_OnExit
				DeadRagdoll_OnExit(pPed);

		// A frame of clip is grabbed from the ragdoll and used to render the char
		FSM_State(State_DeadRagdollFrame)
			FSM_OnEnter
				DeadRagdollFrame_OnEnter(pPed);
			FSM_OnUpdate
				return DeadRagdollFrame_OnUpdate(pPed);
			FSM_OnExit
				DeadRagdollFrame_OnExit(pPed);

		FSM_State(State_FallOutOfVehicle)
			FSM_OnEnter
				FallOutOfVehicle_OnEnter(pPed);
			FSM_OnUpdate
				FallOutOfVehicle_OnUpdate(pPed);
			FSM_OnExit
				FallOutOfVehicle_OnExit(pPed);

		// Exit state
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskInfo *CTaskDyingDead::CreateQueriableState() const
{	
	s32 flags = m_flags;

	// if the dead task is running an NM task, set the Flag_naturalMotionTaskSpecified so the clone dead task knows to look for it
	if ((GetSubTask() && (GetSubTask()->IsNMBehaviourTask() || GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)) ||
		(GetState() == State_DyingRagdoll || GetState() == State_DeadRagdoll || GetState() == State_DeadRagdollFrame))
	{
		flags |= Flag_naturalMotionTaskSpecified;
	}

    s32 shotBoneTag = GetPed() ? GetPed()->GetLastSignificantShotBoneTag() : -1;

	if (!m_flags.IsFlagSet(Flag_DeathAnimSetInternally))
	{
		if (m_flags.IsFlagSet(Flag_clipsOverridenByDictHash))
		{
			Assert(m_clipSetId != CLIP_SET_ID_INVALID);
			Assert(m_clipId != CLIP_ID_INVALID);
			return rage_new CClonedDyingDeadInfo(GetState(), m_deathSourceInfo, shotBoneTag, flags, fwAnimManager::FindSlotFromHashKey(m_clipSetId).Get(), m_clipId, m_fBlendDuration);
		}
		else if (m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash))
		{
			Assert(m_clipSetId != CLIP_SET_ID_INVALID);
			Assert(m_clipId != CLIP_ID_INVALID);
			return rage_new CClonedDyingDeadInfo(GetState(), m_deathSourceInfo, shotBoneTag, flags, m_clipSetId, m_clipId, m_fBlendDuration);
		}
	}

	flags &= ~(Flag_clipsOverridenByDictHash | Flag_clipsOverridenByClipSetHash);

	return rage_new CClonedDyingDeadInfo(GetState(),m_deathSourceInfo, shotBoneTag, flags);
}

void CTaskDyingDead::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
    Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_DYINGDEAD);

    CClonedDyingDeadInfo *clonedDyingDead = (CClonedDyingDeadInfo *)pTaskInfo;

	m_clonePedLastSignificantBoneTag = clonedDyingDead->GetShotBoneTag();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskDyingDead::OnCloneTaskNoLongerRunningOnOwner()
{
	m_QuitTask = true;
}

CTaskDyingDead::FSM_Return CTaskDyingDead::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (m_QuitTask && iEvent == OnUpdate)
	{
		return FSM_Quit;
	}

	if (iState == State_Start)
	{
		// we need to know if the owner fell out of the vehicle or not as it influences our decision on what to do....
		// Can't use synced FallsOutOfVehicleWhenKilled flag anymore as script host is not always in control of ped...
		if (GetStateFromNetwork() == State_Start && !WasLocallyStarted() && !m_flags.IsFlagSet(Flag_startDead))
		{
			// make sure we don't wait too long or the ped will be stuck T-posing
			if (fwTimer::GetSystemTimeInMilliseconds() - m_NetworkStartTime < 1000)
			{
				// so wait until we know which state the owner went to.
				return FSM_Continue;
			}
		}

		if (iEvent == OnEnter)
		{
			// if we are starting a clone task and already in animated ragdoll frame skip the dying part of this task
			if((pPed->GetDeathState() == DeathState_Dead) && !m_flags.IsFlagSet(Flag_startDead))
			{
				if(pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame)
				{
					m_flags.SetFlag(Flag_startDead);
				}
			}

			m_deathSourceInfo.ComputeFallDirection(*pPed);

			// See if there is the control or ragdoll task in the queriable interface, this will dictate the shot behaviour, etc
			CTaskInfo *pControlTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_NM_CONTROL, PED_TASK_PRIORITY_MAX, false);
			if (pControlTaskInfo)
			{
				CTaskNMControl* pControlTask = SafeCast(CTaskNMControl, pPed->GetPedIntelligence()->CreateCloneTaskFromInfo(pControlTaskInfo));

				if (pControlTask)
				{
					SetForcedNaturalMotionTask(pControlTask);

					pControlTask->SwitchClonePedToRagdoll(pPed);
				}
			}
		}
	}

	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone *CTaskDyingDead::CreateTaskForClonePed(CPed *pPed)
{
	taskDisplayf("[%d] CTaskDyingDead::CreateTaskForClonePed: [Ped=%s] [%p] \n", 
		CNetwork::GetNetworkTime(), 
		pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "No Name",
		this);

	fwFlags32 newFlags = m_flags;

	if (GetState() >= State_DeadAnimated || 
		(GetState() == State_DyingAnimated && GetTimeRunning() > 3.0f) ||
		(GetState() == State_DyingAnimated && pPed->GetIsInVehicle() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat)))
	{
		newFlags.SetFlag(Flag_startDead);
	}

	// force the dropped flag after creation of the task post migrate
	newFlags.SetFlag(Flag_PickupsDropped);
	newFlags.SetFlag(Flag_droppedWeapon);

	// prevent the replacement task from putting the ped into ragdoll again if the ped is in a ragdoll frame
	if (GetState() == State_DeadRagdollFrame)
	{
		newFlags.ClearFlag(Flag_ragdollWhenAble);
		newFlags.ClearFlag(Flag_ragdollDeath);
	}

	newFlags.ClearFlag(Flag_naturalMotionTaskSpecified);
	newFlags.ClearFlag(Flag_RunningAnimatedReactionSubtask);

	// prevent the control NM subtask from switching the ped to animated when it aborts
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
	{
		smart_cast<CTaskNMControl*>(GetSubTask())->SetDontSwitchToAnimatedOnAbort(true);
	}

	CTaskDyingDead *newTask = rage_new CTaskDyingDead(&m_deathSourceInfo, newFlags);

	if (m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash))
	{
		newTask->SetDeathAnimationBySet(fwMvClipSetId(m_clipSetId), fwMvClipId(m_clipId));
	}
	else if (m_flags.IsFlagSet(Flag_clipsOverridenByDictHash))
	{
		newTask->SetDeathAnimationByDictionary(fwAnimManager::FindSlotFromHashKey(m_clipSetId).Get(), m_clipId);
	}

	newTask->SetProcessBloodTimer(m_tProcessBloodPoolTimer.GetCurrentTime());
	newTask->SetHasPlayedHornAudio(m_bPlayedHornAudio);

	// leave the ragdoll frame / animation for the next task
	m_flags.SetFlag(Flag_cloneLocalSwitch);

	return newTask;
}

CTaskFSMClone *CTaskDyingDead::CreateTaskForLocalPed(CPed *pPed)
{
	return CreateTaskForClonePed(pPed);
}

void CTaskDyingDead::HandleLocalToCloneSwitch(CPed *pPed)
{
	taskDisplayf("[%d] CTaskDyingDead::HandleLocalToCloneSwitch: [Ped=%s] [%p] \n", 
		CNetwork::GetNetworkTime(), 
		GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "No Name",
		this);

	if(pPed)
	{
		if (GetState() >= State_DeadAnimated)
		{
			pPed->SetDeathState(DeathState_Dead);
		}
		else
		{
			pPed->SetDeathState(DeathState_Dying);
		}
	}
}

void CTaskDyingDead::HandleCloneToLocalSwitch(CPed *pPed)
{
	HandleLocalToCloneSwitch(pPed);
}

bool CTaskDyingDead::HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* pTaskInfo)
{
	HandleLocalToCloneSwitch(pPed);

	return CTaskFSMClone::HandleLocalToRemoteSwitch(pPed, pTaskInfo);
}

void CTaskDyingDead::SetDeathAnimationBySet( const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float blendDuration, float startPhase )
{
	m_clipSetId = clipSetId.GetHash();
	taskAssertf(m_clipSetId != CLIP_SET_ID_INVALID, "Invalid clip set ID %s:%s", clipSetId.TryGetCStr(), clipId.TryGetCStr());
	m_clipId = clipId.GetHash();
	taskAssertf(m_clipId != CLIP_ID_INVALID, "Invalid clip ID %s:%s", clipSetId.TryGetCStr(), clipId.TryGetCStr());
	m_fBlendDuration = blendDuration;
	taskAssertf(m_fBlendDuration >= -1.01 && m_fBlendDuration <= 1.01, "Invalid blend duration %f", m_fBlendDuration);
	m_fStartPhase = startPhase;

	if (m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID)
	{
		//taskAssertf(!m_flags.IsFlagSet(Flag_clipsOverridenByDictHash), "Already set Dict Hash Flag");
		m_flags.ClearFlag(Flag_clipsOverridenByDictHash);
		m_flags.SetFlag(Flag_clipsOverridenByClipSetHash);
	}
}

void CTaskDyingDead::SetDeathAnimationByDictionary( s32 iClipDictIndex, u32 uClipHash, float blendDuration )
{
	taskAssertf(iClipDictIndex>CLIP_DICT_INDEX_INVALID, "Invalid clip dictionary index %d", iClipDictIndex);
	m_clipSetId = fwAnimManager::FindHashKeyFromSlot(strLocalIndex(iClipDictIndex));
	taskAssertf(m_clipSetId != CLIP_SET_ID_INVALID, "Invalid clip set ID %s:%s", fwMvClipSetId(m_clipSetId).TryGetCStr(), fwMvClipId(m_clipId).TryGetCStr());
	m_clipId = uClipHash;
	taskAssertf(m_clipId != CLIP_ID_INVALID, "Invalid clip ID %s:%s", fwMvClipSetId(m_clipSetId).TryGetCStr(), fwMvClipId(m_clipId).TryGetCStr());
	m_fBlendDuration = blendDuration;
	taskAssertf(m_fBlendDuration >= -1.01 && m_fBlendDuration <= 1.01, "Invalid blend duration %f", m_fBlendDuration);

	if (m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID)
	{
		//taskAssertf(!m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash), "Already set Clip Set Hash Flag");
		m_flags.ClearFlag(Flag_clipsOverridenByClipSetHash);
		m_flags.SetFlag(Flag_clipsOverridenByDictHash);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::SetForcedNaturalMotionTask( aiTask* pSubtask )
{
	if( m_pForcedNaturalMotionTask )
		delete m_pForcedNaturalMotionTask;

	m_pForcedNaturalMotionTask = pSubtask;

	if( m_pForcedNaturalMotionTask )
		m_flags.SetFlag(Flag_naturalMotionTaskSpecified);
	else
		m_flags.ClearFlag(Flag_naturalMotionTaskSpecified);
}

//////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
const char * CTaskDyingDead::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_StreamAssets",
		"State_DyingAnimated",
		"State_DyingAnimatedFall",
		"State_DyingRagdoll",
		"State_RagdollAborted",
		"State_DeadAnimated",
		"State_DeadRagdoll",
		"State_DeadRagdollFrame",
		"State_FallOutOfVehicle",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::RecordDamage( CDamageInfo &damageInfo )
{
	CPed* pPed = GetPed();

	if (m_damageInfo.GetCount() <= 16)
	{
		m_damageInfo.PushAndGrow(damageInfo, 4);
		m_flags.SetFlag(Flag_damageImpactRecorded);

		if (!CTaskRevive::IsRevivableDamageType(damageInfo.GetWeaponHash()))
		{
			pPed->SetCauseOfDeath(damageInfo.GetWeaponHash());
		}
	}

	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::TriggerBloodPool( CPed* pPed )
{
	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableBloodPoolCreation ) == false)
	{
		if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CreatedBloodPoolTimer ))
		{
			if (g_vfxBlood.InitBloodPool(pPed, m_tProcessBloodPoolTimer, m_bloodPoolStartSize, m_bloodPoolEndSize, m_bloodPoolGrowRate))
			{
				m_tProcessBloodPoolTimer.Reset();

				pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedBloodPoolTimer, true );

				// if the ped is off screen or too far away then set the start size to be the end size
				if (!pPed->GetIsVisibleInSomeViewportThisFrame() ||
					CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXBLOOD_POOL_LOD_DIST*VFXBLOOD_POOL_LOD_DIST)
				{
					m_bloodPoolStartSize = m_bloodPoolEndSize;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::ProcessBloodPool( CPed* pPed )
{
	// if the start and end size are the same then this ped was off screen or too far away when the blood pool was triggered
	// we just want to end the timer after the first (max size) blood pool has been processed
	if (m_bloodPoolStartSize == m_bloodPoolEndSize)
	{
		//@@: location CTASKDYINGDEAD_PROCESSBLOODPOOL_FORCE_FINISH
		m_tProcessBloodPoolTimer.ForceFinish();
	}
	else
	{
		// if the ped is off screen or too far away then set the growth rate as zero
		// this flags the decal code to instantly make this pool full size
		if (!pPed->GetIsVisibleInSomeViewportThisFrame() ||
			CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXBLOOD_POOL_LOD_DIST*VFXBLOOD_POOL_LOD_DIST)
		{
			m_bloodPoolGrowRate = 0.0f;
			m_tProcessBloodPoolTimer.ForceFinish();
		}
	}

	g_vfxBlood.ProcessBloodPool(pPed, m_bloodPoolStartSize, m_bloodPoolEndSize, m_bloodPoolGrowRate);
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DisablePhysicsWhilstDead( CPed* pPed )
{
	if(pPed->GetCurrentPhysicsInst() && pPed->GetCurrentPhysicsInst()->IsInLevel())
	{
		if(pPed->GetIsStanding() && pPed->GetGroundPhysical()==NULL && !pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ))
		{
			pPed->RemovePhysics();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::SetIKSlopeAngle( CPed* pPed )
{
	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && (pPed->GetIsStanding() || !pPed->IsCollisionEnabled()))
	{
		//float fGroundNormalFwd = DotProduct(pPed->GetGroundNormal(), VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
		//float fGroundNormalRight = DotProduct(pPed->GetGroundNormal(), VEC3V_TO_VECTOR3(pPed->GetTransform().GetA()));
		//pPed->GetIkManager().SetSlopeRollAngle(rage::Asinf(Clamp(fGroundNormalRight, -1.0f, 1.0f)));
		//pPed->GetIkManager().SetSlopeAngle(rage::Asinf(Clamp(fGroundNormalFwd, -1.0f, 1.0f)));
		//pPed->GetIkManager().SetFlag(PEDIK_SLOPE_PITCH);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::MakeAbortable(const AbortPriority iPriority, const aiEvent* pEvent)
{
	bool bMakeAbortable = CTaskFSMClone::MakeAbortable(iPriority, pEvent);

	taskDisplayf("[%d] DEADPED: CTaskDyingDead::MakeAbortable = %s [Ped=%s] [State=%s] [TPri=%d] [Type=%d] [EPri=%d] [Event=%s] [%p] \n", 
		CNetwork::GetNetworkTime(), 
		bMakeAbortable ? "true" : "false",
		GetEntity() && GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "No Name",
		GetStaticStateName(GetState()),
		(int)iPriority,
		pEvent ? pEvent->GetEventType() : -1, 
		pEvent ? pEvent->GetEventPriority() : -1, 
		(pEvent && pEvent->GetName()) ? pEvent->GetName().c_str() : "None",
		this);

	return bMakeAbortable;
}

bool CTaskDyingDead::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	bool bAbort = false;
	if(pEvent)
	{
		switch(((CEvent*)pEvent)->GetEventType())
		{
		// Always abort for death events
		case(EVENT_DEATH):
		case(EVENT_REVIVED):
			bAbort = true;
			break;
		// abort for damage events that are going to switch us to ragdoll
		case(EVENT_DAMAGE):
			{
				const CEventDamage* pEventDamage = static_cast<const CEventDamage*>(pEvent);
				if(pEventDamage->HasRagdollResponseTask())
				{
					bAbort = true;
				}
			}
			break;
		// Give ped task only work if flagged to accept dead or dying peds.
		case(EVENT_GIVE_PED_TASK):
			{
				const CEventGivePedTask* pEventGiveTask = static_cast<const CEventGivePedTask*>(pEvent);
				if(pEventGiveTask->GetAcceptWhenDead())
				{
					bAbort = true;
				}
			}
		}
	}
	else
	{
		bAbort = CTask::ABORT_PRIORITY_IMMEDIATE==iPriority;
	}

	if(bAbort)
	{
#if __BANK	
		if( GetEntity() && GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_PedBeingDeleted ) )
		{
			taskDisplayf("[%d] DEADPED: Aborting Dead Dying Task as ped is being deleted: [Ped=%s] [State=%s] [Death State=%d]\n", 
				CNetwork::GetNetworkTime(), 
				GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "No Name",
				GetStaticStateName(GetState()),
				GetPed()->GetDeathState());
		}
		else
		{
			taskDisplayf("[%d] DEADPED: Aborting Dead Dying Task: [Ped=%s] [State=%s] [Type=%d] [Pri=%d] [Event=%s] \n", 
				CNetwork::GetNetworkTime(), 
				GetEntity() && GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "No Name",
				GetStaticStateName(GetState()),
				pEvent ? pEvent->GetEventType() : -1, 
				pEvent ? pEvent->GetEventPriority() : -1, 
				pEvent ? pEvent->GetName().c_str() : "None");
		}

		//! print additional info for event types.
		if(pEvent)
		{
			switch(((CEvent*)pEvent)->GetEventType())
			{
			case(EVENT_GIVE_PED_TASK):
				{
					const CEventGivePedTask* pEventGiveTask = static_cast<const CEventGivePedTask*>(pEvent);
					taskDisplayf("DEADPED: [EVENT_GIVE_PED_TASK] Task replacing CTaskDyingDead is: %s [Task Priority=%d]\n", pEventGiveTask->GetTask() ? pEventGiveTask->GetTask()->GetName() : "None", pEventGiveTask->GetTaskPriority()); 
				}
				break;
			case(EVENT_DAMAGE):
				{
					const CEventDamage* pEventDamage = static_cast<const CEventDamage*>(pEvent);
					taskDisplayf("DEADPED: [EVENT_DAMAGE] Task replacing CTaskDyingDead is: %s \n", pEventDamage->GetPhysicalResponseTask() ? pEventDamage->GetPhysicalResponseTask()->GetName() : "None"); 
				}
				break;
			case(EVENT_SCRIPT_COMMAND):
				{
					const CEventScriptCommand* pScriptCommand = static_cast<const CEventScriptCommand*>(pEvent);
					taskDisplayf("DEADPED: [EVENT_SCRIPT_COMMAND] Task replacing CTaskDyingDead is: %s [Task Priority=%d]\n", pScriptCommand->GetTask() ? pScriptCommand->GetTask()->GetName() : "None", pScriptCommand->GetTaskPriority()); 

				}
				break;
			}
		}
#endif

		return CTask::ShouldAbort( iPriority, pEvent);
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::InitDeadState()
{
	CPed* pPed = GetPed();

	pPed->SetUseExtractedZ(false);

	// Set the peds death state to reflect this task
	if (!IsRunningLocally()) 
	{
		pPed->SetDeathState(DeathState_Dead);
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockDeadBodyShockingEventsWhenDead))
	{
		CEventShockingDeadBody ev(*pPed);

		// Make this event less noticeable based on the pedtype.
		float fKilledPerceptionModifier = pPed->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
		if (fKilledPerceptionModifier >= 0.0f)
		{
			ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
			ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
		}

		CShockingEventsManager::Add(ev);
	}

	if (!pPed->IsNetworkClone())
	{
		if (pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_USE_SEQUENCE) ||
			pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE))
		{
			pPed->GetPedIntelligence()->ClearPrimaryTask();
		}

		// Make sure the health is zero'd out
		if (pPed->GetHealth() > 0.0f)
		{
			// there is an assert in SetHealth() which will fire if the ped is migrating and his health is being altered. We need to prevent it in this case as the ped is already dead, and the new owner will therefore run the dyingdead task on him
			bool bPreventNetAssert = pPed->IsFatallyInjured();

			//Set last damage event because it is missing.
			if (pPed->GetNetworkObject())
			{
				CNetObjPed* netObjPed = static_cast< CNetObjPed* >(pPed->GetNetworkObject());
				if (netObjPed->GetLastDamageEventPending())
				{
					CEntity* inflictor = pPed->GetWeaponDamageEntity()?pPed->GetWeaponDamageEntity():m_deathSourceInfo.GetEntity();
					u32 weapon = pPed->GetWeaponDamageEntity()?pPed->GetWeaponDamageHash():m_deathSourceInfo.GetWeapon();
					CNetObjPhysical::NetworkDamageInfo damageInfo(inflictor, pPed->GetHealth(), pPed->GetArmour(), 0.0f, false, weapon, pPed->GetWeaponDamageComponent(), false, true, false);
					netObjPed->UpdateDamageTracker(damageInfo);
				}
			}

			//@@: location CTASKDYINGDEAD_SET_HEALTH_ZERO
			pPed->SetHealth(0.0f, bPreventNetAssert);
		}

		// The below code for dropping a weapon won't typically get called here, since other code in this file handles dropping the weapon,
		// but we include the drop code here as well as a back up in case the transition to the dead state happens very quickly for whatever reason.
		//
		// MP/SP - do the same thing.  This is where if you hit a prostitute in SP it will call CreateDeadPedPickups as they already have the startdead flag set.  Allow this to be called in the same way for MP as SP so that when you hit a prostitute you will get their money.
		if (!m_flags.IsFlagSet(Flag_droppedWeapon))
		{
			DropWeapon(pPed);
		}
		else if(pPed->GetWeaponManager())
			pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);

		if (!m_flags.IsFlagSet(Flag_PickupsDropped))
		{ 
	       	pPed->CreateDeadPedPickups(); 
        	m_flags.SetFlag(Flag_PickupsDropped); 
		} 

		// only add an injury incident for local peds
		// in a network game they must have a network ptr (dead players have a non-networked ped left behind. We can't generate an incident for these.)
		// Also, don't create an incident for a ped that has just migrated (Flag_startDead will be set)
		if (!m_flags.IsFlagSet(Flag_startDead) &&
			(!NetworkInterface::IsGameInProgress() || pPed->GetNetworkObject())
			&& GetState() != State_DeadRagdoll )
		{
			// set the start dead flag here so that if the ped is cloned on another machine after this point he won't play the animated death
			m_flags.SetFlag(Flag_startDead);

			if(CInjuryIncident::IsPedValidForInjuryIncident(pPed))
			{
				int incidentIndex = -1;

				CInjuryIncident incident(pPed, vPedPosition);
				CIncidentManager::GetInstance().AddIncident(incident, true, &incidentIndex);
			}
		}
	}
	else if (pPed->IsAPlayerPed())
	{
		// the player will still be holding his weapon, but it needs to be removed remotely
		CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		if(pWeaponMgr)
		{
			pWeaponMgr->DestroyEquippedWeaponObject(false OUTPUT_ONLY(,"CTaskDyingDead::InitDeadState()"));
		}

		if (pPed->GetNetworkObject())
		{
			static_cast<CNetObjPed*>(pPed->GetNetworkObject())->SetTaskOverridingPropsWeapons(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::BlendUpperBodyReactionClip( CPed* pPed )
{
	// It seems this can sometimes have no data in it in multiplayer games
	if (m_damageInfo.GetCount()>0)
	{
		Vector3 vDir(-VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
		if(m_damageInfo[0].GetStartPos().IsNonZero() )
		{
			vDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_damageInfo[0].GetStartPos();
			vDir.z = 0.0f;
			vDir.NormalizeSafe();
		}
		fwMvClipSetId nClipSetId = pPed->GetPedModelInfo()->GetAdditiveDamageClipSet();
		fwMvClipId nClipId = CLIP_DAM_FRONT;
		// transform float heading into an integer offset
		float fTemp = rage::Atan2f(-vDir.x, vDir.y) + QUARTER_PI;
		if(fTemp < 0.0f) fTemp += TWO_PI;
		int nHitDirn = int(fTemp / HALF_PI);

		switch(nHitDirn)
		{
		case CEntityBoundAI::FRONT:
			nClipId = CLIP_DAM_FRONT;
			break;
		case CEntityBoundAI::LEFT:
			nClipId = CLIP_DAM_LEFT;
			break;
		case CEntityBoundAI::REAR:
			nClipId = CLIP_DAM_BACK;
			break;
		case CEntityBoundAI::RIGHT:
			nClipId = CLIP_DAM_RIGHT;
			break;
		default:
			nClipId = CLIP_DAM_FRONT;
		}

		m_moveNetworkHelper.SendRequest(ms_UpperBodyReactionId);
		m_moveNetworkHelper.SetClip(fwAnimManager::GetClipIfExistsBySetId(nClipSetId, nClipId), ms_UpperBodyReactionClipId);
	}
	
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::CheckForDeadInCarAudioEvents( CPed* pPed )
{
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHornAudioWhenDead) && !m_bPlayedHornAudio && pPed->GetIsInVehicle() && 
		pPed->GetMyVehicle()->GetSeatManager()->GetDriver() == pPed && pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_CAR)
	{
		if (m_moveNetworkHelper.IsNetworkActive())
		{
			// If the driver is dead on the wheel, play the horn at the right time
			if (m_clipId == CVehicleSeatAnimInfo::ms_DeadClipId)
			{
				const bool bReachedEndOfAnim = m_moveNetworkHelper.GetBoolean(ms_DyingClipEndedId) || GetState() == State_DeadAnimated;
				if (m_moveNetworkHelper.GetBoolean(ms_HornAudioOnId) || bReachedEndOfAnim)
				{
#if __ASSERT
					const CVehicleSeatAnimInfo* pVehicleSeatInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);
					const crClip* pClip = fwClipSetManager::GetClip(pVehicleSeatInfo->GetSeatClipSetId(), CVehicleSeatAnimInfo::ms_DeadClipId);
					animAssertf(CClipEventTags::HasMoveEvent(pClip, ms_HornAudioOnId), "ADD A BUG TO DEFAULT ANIM INGAME - Clip : %s, Clipset : %s Doesn't Have A HornAudioOn Event Tag ", CVehicleSeatAnimInfo::ms_DeadClipId.GetCStr(), pVehicleSeatInfo->GetSeatClipSetId().GetCStr());
#endif // __ASSERT

					// The player purposefully killing a ped at the wheel of a car (eg. by shooting them) always triggers a horn. For peds dying indirectly, 
					// (eg. the player crashing into them), we require a cooldown period to elapse before retriggering to prevent loud horns constantly playing 
					// if you're just being a really crap driver and ricocheting between AI cars.
					u32 currentTime = fwTimer::GetTimeInMilliseconds();
					bool wasKilledbyPlayer = pPed->GetSourceOfDeath() == FindPlayerPed();

					if(currentTime - ms_iLastVehicleHornTime > MIN_HORN_REPEAT_TIME || wasKilledbyPlayer)
					{
						pPed->GetMyVehicle()->GetVehicleAudioEntity()->PlayHeldDownHorn(7.f);						
						ms_iLastVehicleHornTime = fwTimer::GetTimeInMilliseconds();
					}

					m_bPlayedHornAudio = true;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DropWeapon( CPed* pPed )
{
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead))
	{
		return;
	}

	CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if(pWeaponMgr)
	{
		if (CObject* pWeaponObject = pWeaponMgr->GetEquippedWeaponObject())
		{
			if (phCollider* collider = pWeaponObject->GetCollider())
			{
				collider->EnablePushCollisions();
			}
		}

		// Always try to drop the last equipped weapon if it's non-unarmed.
		// For cops or mission peds, they will try to drop the best weapon if they never equipped a non-unarmed weapon.
		bool bCanCreatePickupFromBestWeaponIfUnarmed = CPickupManager::CanCreatePickupIfUnarmed(pPed);

		const CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
		if(pWeapon)
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();

			const bool bHasPickup = CPickupManager::ShouldUseMPPickups(pPed) ? (pWeapon->GetWeaponInfo()->GetMPPickupHash() != 0) : (pWeapon->GetWeaponInfo()->GetPickupHash() != 0);
			CPickup* pPickup = bHasPickup ? CPickupManager::CreatePickUpFromCurrentWeapon(pPed) : NULL;

			if(pPickup)
			{
				if (pPed->IsLocalPlayer() && pPickup->GetNetworkObject())
				{
					CObject* pPlayerWeaponObject = NULL;

					if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
					{
						pPlayerWeaponObject = pWeaponMgr->DropWeapon(pWeapon->GetWeaponHash(), false, false);
					}
				
					// pass the weapon and pickup over to the player's network object. The pickup will remain hidden while the player is dead, and the weapon will be
					// deleted once he respawns
					NetworkInterface::SetLocalDeadPlayerWeaponPickupAndObject(pPickup, pPlayerWeaponObject);
				}
				else
				{
					pWeaponMgr->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);
					pWeaponMgr->DestroyEquippedWeaponObject(false);
				}
			}
			else
			{
				// Drop the object
				if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
				{
					pWeaponMgr->DropWeapon(pWeapon->GetWeaponHash(), false);
				}
				else if(bCanCreatePickupFromBestWeaponIfUnarmed)
				{
					CPickupManager::CreatePickUpFromCurrentWeapon(pPed, false, true);
				}
			}
		}
		else if(bCanCreatePickupFromBestWeaponIfUnarmed)
		{
			CPickupManager::CreatePickUpFromCurrentWeapon(pPed, false, true);
		}
	}

	if(!pPed->IsLocalPlayer() && pPed->GetInventory())
	{
		pPed->GetInventory()->RemoveAllWeaponsExcept(pPed->GetDefaultUnarmedWeaponHash());
	}

	m_flags.SetFlag(Flag_droppedWeapon);
}

//////////////////////////////////////////////////////////////////////////

const crClip* CTaskDyingDead::GetDeathClip(bool ASSERT_ONLY(bAssert))
{
	const crClip* pClip = NULL;

	if (m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash))
	{
		fwMvClipSetId clipSet;
		clipSet.SetHash(m_clipSetId);
#if __ASSERT
		if (bAssert)
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSet);
			if (aiVerifyf(pClipSet, "No clipset found for clipsetid %s (%u) (flags %d, previous state %s, clone ped %d)", clipSet.GetCStr(), m_clipSetId, m_flags.GetAllFlags(), GetStaticStateName(GetPreviousState()), GetPed() ? GetPed()->IsNetworkClone() : 0))
			{
				taskAssertf(pClipSet->IsStreamedIn_DEPRECATED(), "Clipset %s wasn't streamed in for %s ped %s, previous state %s", clipSet.GetCStr(), GetPed()->IsNetworkClone() ? "CLONE" : "LOCAL", GetPed()->GetDebugName(), GetStaticStateName(GetPreviousState()));
			}
		}
#endif // __ASSER
		fwMvClipId clip;
		clip.SetHash(m_clipId);
		pClip = fwAnimManager::GetClipIfExistsBySetId(clipSet, clip);
		taskAssertf(!bAssert || pClip, "Failed to find forced clip for dead task. Clip set: %s (%u), Clip: %s (%u)", 
			clipSet.TryGetCStr() ? clipSet.TryGetCStr() : "UNKNOWN", 
			m_clipSetId,
			clip.TryGetCStr() ? clip.TryGetCStr() : "UNKNOWN",
			m_clipId
			);
	}
	else if (m_flags.IsFlagSet(Flag_clipsOverridenByDictHash))
	{
		pClip = fwAnimManager::GetClipIfExistsByDictIndex(fwAnimManager::FindSlotFromHashKey(m_clipSetId).Get(), m_clipId);
#if __ASSERT
		atHashString clip(m_clipId);
		strLocalIndex slot = fwAnimManager::FindSlotFromHashKey(m_clipSetId);
		taskAssertf(!bAssert || pClip, "Failed to find forced clip for dead task. Clip dict %s, hash: %u, Clip %s, hash: %u", 
			slot.Get() >= 0 ? fwAnimManager::GetName(slot) : "dict not found", 
			m_clipSetId, 
			clip.TryGetCStr() ? clip.TryGetCStr() : "UNKNOWN", 
			m_clipId 
			);
#endif //__ASSERT
	}
	else
	{
		fwMvClipSetId clipSetId;
		fwMvClipId clipId;
		CTaskFallOver::PickFallAnimation(GetPed(), m_deathSourceInfo.GetFallContext(), m_deathSourceInfo.GetFallDirection(), clipSetId, clipId);
		pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
		taskAssertf(pClip, "Failed to find default clip for dead task. Clip set: %s, Clip: %s", 
			clipSetId.TryGetCStr() ? clipSetId.TryGetCStr() : "UNKNOWN" , 
			clipId.GetCStr() ? clipId.TryGetCStr() : "UNKNOWN");
	}

	return pClip;
}

void CTaskDyingDead::StartMoveNetwork(CPed* pPed, float blendDuration)
{
	if ((!m_moveNetworkHelper.IsNetworkActive() || m_moveNetworkHelper.HasNetworkExpired()) && fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskDyingDead))
	{
		bool blendingFromStaticPose =  false;
		if (pPed->GetAnimDirector())
		{
			if (pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame)
			{
				pPed->GetMovePed().SwitchToAnimated(blendDuration);
				blendingFromStaticPose = true;
			}
		}
		m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskDyingDead);
		pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, blendingFromStaticPose ? 0.0f : blendDuration);
	}
}

//////////////////////////////////////////////////////////////////////////
// FSM state methods

//////////////////////////////////////////////////////////////////////////
// Start
void CTaskDyingDead::Start_OnEnter( CPed* pPed )
{
	AddToStateHistory(State_Start);

	// We stream in and set the in vehicle death anim in the stream assets state, so no need to try to compute here
	if (!pPed->GetIsInVehicle())
	{
		ComputeDeathAnim(pPed);
	}

	if (!pPed->IsNetworkClone() && pPed->GetNetworkObject() && !pPed->GetNetworkObject()->GetSyncData())
	{
		pPed->GetNetworkObject()->StartSynchronising();
	}

	if (pPed->IsNetworkClone() && m_clonePedLastSignificantBoneTag != -1)
	{
		pPed->SetLastSignificantShotBoneTag(m_clonePedLastSignificantBoneTag);
	}

	const fwClipSetWithGetup* pSet = CTaskGetUp::FindClipSetWithGetup(pPed);
	if (pSet)
	{
		m_bCanAnimatedDeadFall = pSet->CanAnimatedDeadFall();
	}
	else
	{
		m_bCanAnimatedDeadFall = false;
	}

	if (pPed->IsNetworkClone() && !pPed->GetUsingRagdoll())
	{
		pPed->ResetDesiredMainMoverCapsuleData(true);
	}

	taskDisplayf("[%d] CTaskDyingDead::Start_OnEnter: [Ped=%s] [Clone:%s] [%p]\n", 
		CNetwork::GetNetworkTime(), 
		(GetPed()->GetNetworkObject()) ? GetPed()->GetNetworkObject()->GetLogName() : "No Name",
		GetPed()->IsNetworkClone() ? "true" : "false",
		this);

	if (pPed->GetIsInVehicle() && (pPed->GetAttachOffset().IsNonZero() || !pPed->GetAttachQuat().IsEqual(Quaternion(Quaternion::IdentityType))))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset, true);
	}

	if (!pPed->IsNetworkClone() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromBicycle())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat, true);
	}

	//Set the death info.
	SetDeathInfo();

	if (!IsRunningLocally())
	{
		pPed->SetDeathState(DeathState_Dying);
	}

	OnDeadEnter( *pPed );

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedBloodPoolTimer, false );

	if (pPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
	{
		CEventPlayerDeath playerDeath(pPed);
		GetEventGlobalGroup()->Add(playerDeath);
	}

	RequestProcessMoveSignalCalls();

	// Make sure any previous heading change request is removed
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());

	// force an immediate update of the ped's tasks to the damaging player so that he reacts to the shot a.s.a.p on that players machine
	if (NetworkInterface::IsGameInProgress() && !m_flags.IsFlagSet(Flag_startDead) && pPed->IsAPlayerPed())
	{
		if (pPed->GetNetworkObject() && !pPed->IsNetworkClone())
		{
			const netObject* pAttackerObj = NetworkUtils::GetNetworkObjectFromEntity(m_deathSourceInfo.GetEntity());

			if (pAttackerObj && pAttackerObj->GetObjectType() == NET_OBJ_TYPE_PLAYER && pAttackerObj->IsClone())
			{
				SafeCast(CNetObjPed, pPed->GetNetworkObject())->RequestForceSendOfTaskState();
			}
		}
	}
	
	// Passing the message to the conductors.
	ConductorMessageData messageData;
	messageData.conductorName = GunfightConductor;
	messageData.message = PedDying;
	messageData.entity = pPed;
	CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData));
	// For peds getting out of a vehicle we cannot activate the ragdoll too early because the vehicle is concave,
	// the interpenetration leads to peds being pinged about. Instead, we delay the abortion of TASK_VEHICLE_GET_OUT for the damage event until
	// a point in the clip in which it is safe to activate the ragdoll. Once the abortion occurs, this task is run, which forces the ped into an nm behaviour if possible
	// otherwise, the animated response happens after the vehicle get out is finished.
	if( m_flags.IsFlagSet(Flag_ragdollWhenAble) )
	{
		// Do a shot behaviour as it looks better than a relax, plus i think this situation will only happen whilst shooting a ped getting out
		const int MIN_TIME = 1000; 
		const int MAX_TIME = 10000;

		Assert(pPed->GetRagdollInst());
		//phBoundComposite *bounds = pPed->GetRagdollInst()->GetTypePhysics()->GetCompositeBounds();
		//Assert(bounds);
		static s32 iRagdollComponent = RAGDOLL_SPINE3;
		int nComponent = iRagdollComponent;//fwRandom::GetRandomNumberInRange(1, bounds->GetNumBounds()-1);

		Matrix34 matComponent = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		// get component position directly from the ragdoll bounds
		pPed->GetRagdollComponentMatrix(matComponent, RAGDOLL_SPINE0);

		dev_float TEST_SHOT_HIT_FORCE_MULTIPLIER = -30.0f;

		Vector3 vHitDir   = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());

		// Pretend the shot came from a direction that will push the ped away from the vehicle
		if (pPed->GetMyVehicle())
		{
			Vector3 vShotStart = VEC3V_TO_VECTOR3(pPed->GetMyVehicle()->GetTransform().GetPosition()) + ms_Tunables.m_VehicleForwardInitialScale * VEC3V_TO_VECTOR3(pPed->GetMyVehicle()->GetTransform().GetB());
			Vector3 vAwayFromVehicle = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vShotStart;
			vAwayFromVehicle.Normalize();
#if DEBUG_DRAW
			ms_debugDraw.AddLine(pPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition() + RCC_VEC3V(vAwayFromVehicle), Color_red, 2000);
#endif
			vHitDir = vAwayFromVehicle;
		}

		aiTask* pTaskNM = NULL;
		TUNE_GROUP_BOOL(DAMAGE_DEATH_TUNE, USE_RELAX_FOR_VEHICLE_DEATH, true);
		if (USE_RELAX_FOR_VEHICLE_DEATH && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the next line to route to TaskRageRagdoll
			if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
				CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);
			pTaskNM = rage_new CTaskNMRelax(MIN_TIME, MAX_TIME, -1, 1.0f);
		}
		else
		{
			Vector3 vHitForce = vHitDir * TEST_SHOT_HIT_FORCE_MULTIPLIER;
			pTaskNM = rage_new CTaskNMShot(pPed, MIN_TIME, MAX_TIME, NULL, WEAPONTYPE_PISTOL, nComponent, matComponent.d, vHitForce, vHitDir);
		}
		
		SetForcedNaturalMotionTask(rage_new CTaskNMControl(2000, 30000, pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM));

		m_flags.SetFlag(Flag_ragdollDeath);
	}
	else if (pPed->GetUsingRagdoll())
	{
		m_flags.SetFlag(Flag_ragdollDeath);
	}

	// If the ped has died in a vehicle make sure we set a deaddriver task on the vehicle
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && !pPed->IsExitingVehicle() && !pPed->GetMyVehicle()->InheritsFromBicycle())
	{
		//make sure the ped is the driver and no other players can take over the controls
		if(pPed->GetMyVehicle()->IsDriver(pPed) && 
		  !pPed->GetMyVehicle()->IsNetworkClone() &&
		   pPed->GetMyVehicle()->GetSeatManager()->GetNumPlayers() <= 1 )
		{
			pPed->GetMyVehicle()->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, rage_new CTaskVehicleDeadDriver(pPed->GetSourceOfDeath()), VEHICLE_TASK_PRIORITY_PRIMARY, false);
		}
	}

	if(NetworkInterface::IsGameInProgress())
	{
		const CVehicle* pVeh = pPed->GetMyVehicle();
		if (pVeh)
		{
			bool bIsTurretedVehicle = false;
			bool bShouldDestroyWeaponMgr = CTaskVehicleFSM::ShouldDestroyVehicleWeaponManager(*pVeh, bIsTurretedVehicle);

			// For vehicles with mountable turrets (these can be not in the driver seat), we need to keep the weapon manager around if there is a ped still alive in the vehicle
			if (!bIsTurretedVehicle)
			{
				// Otherwise cleanup vehicle weapons if we are the driver and are killed
				if (pPed->GetIsDrivingVehicle())
				{
					bShouldDestroyWeaponMgr = true;
				}
			}
			
			if (bShouldDestroyWeaponMgr)
			{
				pPed->GetMyVehicle()->DestroyVehicleWeaponWhenInactive();
			}
		}
	}

	//If on a mount just pop off for now
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) && pPed->GetMyMount())
	{
		pPed->SetPedOffMount();
	}

	if(!m_flags.IsFlagSet(Flag_DontSendPedKilledShockingEvent))
	{
		// Add a "seen ped killed" shocking event.
		CEntity* pEntityResponsible = m_deathSourceInfo.GetEntity();
		CPed* pKiller = NULL;
		if (pEntityResponsible && pEntityResponsible->GetIsTypePed())
		{
			pKiller = static_cast<CPed*>(pEntityResponsible);
		}
		else if (pEntityResponsible && pEntityResponsible->GetIsTypeVehicle())
		{
			CVehicle* pKillerVehicle = static_cast<CVehicle*>(pEntityResponsible);
			pKiller = pKillerVehicle->GetDriver();
		}

		if (NetworkInterface::IsGameInProgress())
		{
			if (pKiller && pKiller->IsPlayer())
			{
				bool bAllowCrimeOrThreatResponsePlayerJustResurrected = NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected(pKiller,pPed);
				wantedDebugf3("CTaskDyingDead::Start_OnEnter--pPed[%p] pKiller[%p] killer is player - AllowCrimeOrThreatResponsePlayerJustResurrected[%d] --> %s",pPed,pKiller,bAllowCrimeOrThreatResponsePlayerJustResurrected,bAllowCrimeOrThreatResponsePlayerJustResurrected ? "proceed -- establish CEventShockingSeenPedKilled" : "set pKill = NULL / prevents CEventShockingSeenPedKilled");			
				if (!bAllowCrimeOrThreatResponsePlayerJustResurrected)
					pKiller = NULL;
			}
		}

		if (pKiller)
		{
			CEventShockingSeenPedKilled ev(*pKiller, pPed);

			// Make this event less noticeable based on the pedtype.
			float fKilledPerceptionModifier = pPed->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
			if (fKilledPerceptionModifier >= 0.0f)
			{
				ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
				ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
			}

			CShockingEventsManager::Add(ev);

			// We only want to generate the event once
			m_flags.SetFlag(Flag_DontSendPedKilledShockingEvent);
		}
	}

	// local ped only stuff: 
	if (!pPed->IsNetworkClone())
	{
		//Set last damage event because it is missing.
		if (pPed->GetNetworkObject())
		{
			CNetObjPed* netObjPed = static_cast< CNetObjPed* >(pPed->GetNetworkObject());
			if (netObjPed->GetLastDamageEventPending())
			{
				CEntity* inflictor = pPed->GetWeaponDamageEntity()?pPed->GetWeaponDamageEntity():m_deathSourceInfo.GetEntity();
				u32 weapon = pPed->GetWeaponDamageEntity()?pPed->GetWeaponDamageHash():m_deathSourceInfo.GetWeapon();
				bool bMeleeHit = pPed->GetWeaponDamageEntity()?pPed->GetWeaponDamagedMeleeHit():false;
				CNetObjPhysical::NetworkDamageInfo damageInfo(inflictor, pPed->GetHealth(), pPed->GetArmour(), 0.0f, false, weapon, pPed->GetWeaponDamageComponent(), false, true, bMeleeHit);
				netObjPed->UpdateDamageTracker(damageInfo);
			}
		}

		// there is an assert in SetHealth() which will fire if the ped is migrating and his health is being altered. We need to prevent it in this case as the ped is already dead, and the new owner will therefore run the dyingdead task on him
		bool bPreventNetAssert = pPed->IsFatallyInjured();
		pPed->SetHealth(0.0f, bPreventNetAssert);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::Start_OnUpdate( CPed* pPed )
{
	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
	{
		bool bFallOutOfVehicle = false;

		// owner decides what to do based on flag...
		if(!pPed->IsNetworkClone())
		{
			const s32 iSeatIndex = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
			const CVehicleSeatAnimInfo* pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(iSeatIndex);

			bool bHasFallOutAnims = false;
			s32 iDirectEntryPointIndex = pPed->GetMyVehicle()->GetDirectEntryPointIndexForPed(pPed);
			if (pPed->GetMyVehicle()->IsEntryPointIndexValid(iDirectEntryPointIndex))
			{
				const CVehicleEntryPointAnimInfo* pEntryAnimInfo = pPed->GetMyVehicle()->GetEntryAnimInfo(iDirectEntryPointIndex);
				s32 iDictIndex = -1;
				u32	clipId = 0;
				u32 exitClipSet = pEntryAnimInfo->GetExitPointClipSetId().GetHash();
				bHasFallOutAnims = CVehicleSeatAnimInfo::FindAnimInClipSet(fwMvClipSetId(exitClipSet), CTaskExitVehicleSeat::ms_Tunables.m_DeadFallOutClipId, iDictIndex, clipId );
			}
			
			bool bOkToFallOut = (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled ) && bHasFallOutAnims) || 
				(pSeatAnimInfo && pSeatAnimInfo->GetFallsOutWhenDead());

			if (bOkToFallOut)
			{
				// Check if the seat anims are setup to make the ped fall out of the vehicle when dead
				VehicleEnterExitFlags vehicleFlags;
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DeadFallOut);
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);

				s32 iTargetEntryPoint = 
					CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, pPed->GetMyVehicle(), SR_Specific, iSeatIndex, false, vehicleFlags);
			
				if(iTargetEntryPoint != -1)
				{
					m_iExitPointForVehicleFallOut = iTargetEntryPoint;
					bFallOutOfVehicle = true;
				}
			}
		}
		else 
		{
			// network clones:
			if (GetStateFromNetwork() == State_FallOutOfVehicle)
			{
				bFallOutOfVehicle = true;
			}
			else if (pPed->GetIsInVehicle() && !m_flags.IsFlagSet(Flag_inVehicleDeath))
			{
				if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AttachedToVehicle))
				{
					bFallOutOfVehicle = true;
				}
				else if (CTaskNMBehaviour::CanUseRagdoll(pPed,RAGDOLL_TRIGGER_VEHICLE_FALLOUT))
				{
					pPed->SwitchToRagdoll(*this);
					SetState(State_DyingRagdoll);
					return FSM_Continue;
				}
			}
		}

		// however, if the collision isn't loaded under the ped, don't fall out (task should be out of scope for a clone)....
		if( pPed->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) && 
			CTaskNMBehaviour::CanUseRagdoll(pPed,RAGDOLL_TRIGGER_VEHICLE_FALLOUT) )
		{
			bFallOutOfVehicle = false;
		}

		// If the flag is set, tell the ped to fall out of the vehicle onto the floor....
		if (bFallOutOfVehicle)
		{
			if(!pPed->IsNetworkClone())
			{
				// Wipe any vehicle animation overrides that have been set up so they don't get synced over to a clone.....
				m_flags.ClearFlag(Flag_clipsOverridenByClipSetHash);
				m_flags.ClearFlag(Flag_clipsOverridenByDictHash);

				m_clipSetId = CLIP_SET_ID_INVALID;
				m_clipId	= CLIP_ID_INVALID;
			}

			SetState(State_FallOutOfVehicle);
			return FSM_Continue;
		}

		if (!pPed->IsNetworkClone())
		{
			m_flags.SetFlag(Flag_inVehicleDeath);
		}
	}

	// We've already switched the ped to the ragdoll frame
	// go straight to the correct state.
	if (m_flags.IsFlagSet(Flag_GoStraightToRagdollFrame))
	{
		SetState(State_DeadRagdollFrame);
		return FSM_Continue;
	}

	if (!m_flags.IsFlagSet(Flag_startDead))
	{
		//Deal with forced or existing natural motion tasks for dying

		// If a natural motion task is specified, use that
		if( m_flags.IsFlagSet(Flag_naturalMotionTaskSpecified) )
		{
			// If a subtask has been specified
			if(CTaskNMBehaviour::CanUseRagdoll(pPed,RAGDOLL_TRIGGER_DIE))
			{
				m_flags.SetFlag(Flag_ragdollDeath);

				SetState(State_DyingRagdoll);
				return FSM_Continue;
			}
			// Remove the forced NM task
			else
			{
				SetForcedNaturalMotionTask(NULL);
			}
		}
	}

	if(!pPed->GetUsingRagdoll() && !m_flags.IsFlagSet(Flag_startDead) && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceRagdollUponDeath) || m_flags.IsFlagSet(Flag_ragdollDeath)))
	{
		if(CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ) )
		{
			pPed->SwitchToRagdoll(*this);
		}
	}

	// If the ragdoll is already active, the ped must move to a dying ragdoll state
	if( pPed->GetUsingRagdoll() && CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ))
	{
		m_flags.SetFlag(Flag_ragdollDeath);

		if (m_flags.IsFlagSet(Flag_startDead))
		{
			SetState(State_DeadRagdoll);
		}
		else
		{
			SetState(State_DyingRagdoll);
		}
	}
	// Otherwise - no clip so move to a dying animated state.
	else
	{
		bool bPedInVehicle = pPed->GetIsInVehicle();

		if (m_flags.IsFlagSet(Flag_startDead))
		{
			if(pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame)
			{
				SetState(State_DeadRagdollFrame);
			}
			else
			{
				if (bPedInVehicle)
				{
					SetState(State_StreamAssets);
					return FSM_Continue;
				}
				else
				{
					SetState(State_DeadAnimated);
					return FSM_Continue;
				}
			}
		}
		else
		{
			if(pPed->GetMovePed().GetState() == CMovePed::kStateStaticFrame)
			{
				// This happens if the ragdoll was aborted (because another more important ped needed to use it)
				// and we were switched to a static frame.  Playing back our dying animation would probably not look so great as we would be blending
				// from a random static pose...
				// Switching to the RagdollAborted state allows us to pose match to a more suitable dying animation
				SetState(State_RagdollAborted);
				return FSM_Continue;
			}
			else if (m_clipSetId == CTaskWrithe::ms_ClipSetIdDie.GetHash())
			{
				// Maybe we should do this for all deathanims just for precaution
				m_ClipSetRequestHelper.Request(CTaskWrithe::ms_ClipSetIdDie);
				SetState(State_StreamAssets);
				return FSM_Continue;
			}
			else if (pPed->GetIsInVehicle())
			{
				SetState(State_StreamAssets);
				return FSM_Continue;
			}
			else
			{
				SetState(State_DyingAnimated);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;	
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::StreamAssets_OnEnter( CPed* pPed )
{
	AddToStateHistory(State_StreamAssets);

	if (m_ClipSetRequestHelper.GetClipSetId() == CTaskWrithe::ms_ClipSetIdDie)
	{
		//! Do nothing.
	}
	else if (taskVerifyf(pPed->GetIsInVehicle(), "CTaskDyingDead: StreamAssets_OnEnter - ped is not in a vehicle!"))
	{
		const CVehicleSeatAnimInfo* pVehicleSeatInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);

		if (taskVerifyf(pVehicleSeatInfo, "CTaskDyingDead: Couldn't get seat animation info for vehicle"))
		{
			u32 seatClipSet = pVehicleSeatInfo->GetSeatClipSetId().GetHash();
			
			// B*2500511: If in a lowrider and we're using the alternate clipset (arm on window variant), use that clipset for the death animations.
			if (pPed->GetIsInVehicle() && pPed->GetPedIntelligence())
			{
				const CTaskMotionInAutomobile *pTaskInAutomobile = static_cast<const CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
				if (pTaskInAutomobile && pTaskInAutomobile->IsUsingAlternateLowriderClipset())
				{
					u32 lowriderDeathClipset = pVehicleSeatInfo->GetAltLowriderLeanClipSet().GetHash();
					if (lowriderDeathClipset != CLIP_SET_ID_INVALID)
					{
						seatClipSet = lowriderDeathClipset;
					}
				}
			}

			if (taskVerifyf(seatClipSet != CLIP_SET_ID_INVALID, "Seat Anim Info (%s) has no seat clipset", pVehicleSeatInfo->GetName().GetCStr()))
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(fwMvClipSetId(seatClipSet));
				if (taskVerifyf(pClipSet, "Couldn't find clipset with hash (%u)", seatClipSet))
				{
					m_clipSetId = seatClipSet;
				}
			}
		}
	}
}

CTask::FSM_Return CTaskDyingDead::StreamAssets_OnUpdate( CPed* pPed )
{
	// Well sorry Tez but I also need to stream stuff in here
	if (m_ClipSetRequestHelper.GetClipSetId() == CTaskWrithe::ms_ClipSetIdDie)
	{
		if (m_ClipSetRequestHelper.Request())
		{
			if (m_flags.IsFlagSet(Flag_startDead))
			{
				SetState(State_DeadAnimated);
			}
			else
			{
				SetState(State_DyingAnimated);
			}
			return FSM_Continue;
		}
	}
	else if (taskVerifyf(m_clipSetId != CLIP_SET_ID_INVALID, "CTaskDyingDead: StreamAssets_OnUpdate - ped didn't set vehicle streaming clip set!"))
	{
		if(!pPed->GetIsInVehicle())
		{	
			//! Our vehicle has been deleted. Ragdoll if we can.
			if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
			{
				SetState(State_DyingRagdoll);
				return FSM_Continue;	
			}
		}
		
		if (m_ClipSetRequestHelper.Request(fwMvClipSetId(m_clipSetId)))
		{
			if (m_flags.IsFlagSet(Flag_clipsOverridenByDictHash))
			{
				s32 iDictIndex = -1;
				if(CVehicleSeatAnimInfo::FindAnimInClipSet(fwMvClipSetId(m_clipSetId), CVehicleSeatAnimInfo::ms_DeadClipId, iDictIndex, m_clipId))
				{
					m_clipSetId = fwAnimManager::FindHashKeyFromSlot(strLocalIndex(iDictIndex));
				}
				else
				{
					taskAssertf(0, "Couldn't find clip ID %s for vehicle %s", fwMvClipId(m_clipId).TryGetCStr(), pPed->GetMyVehicle()->GetModelName());
				}
			}
			else
			{
				m_flags.SetFlag(Flag_clipsOverridenByClipSetHash);
			}

			//! Force clip Id
			m_clipId = CVehicleSeatAnimInfo::ms_DeadClipId;

			m_flags.SetFlag(Flag_DeathAnimSetInternally);

			m_bDontUseRagdollFrameDueToInVehicleDeath = true;

			if (m_flags.IsFlagSet(Flag_startDead))
			{
				SetState(State_DeadAnimated);
			}
			else
			{
				SetState(State_DyingAnimated);
			}
		}
	}
	else
	{
		SetState(State_DeadAnimated);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// DyingAnimated

void CTaskDyingDead::DyingAnimated_OnEnter( CPed* pPed )
{
	AddToStateHistory(State_DyingAnimated);

	// If cause of death was an explosion, run CTaskAnimatedHitByExplosion as a subtask
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_deathSourceInfo.GetWeapon());
	if (!m_flags.IsFlagSet(Flag_EndingAnimatedDeathFall) && pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE && !pPed->IsAPlayerPed() && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )
		&& aiVerify(m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash)) && aiVerify(m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID))
	{
		// CTaskAnimatedHitByExplosion only supports humans
		if (pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetARTAssetID() >= 0)
		{
			SetNewTask(rage_new CTaskAnimatedHitByExplosion(static_cast<fwMvClipSetId>(m_clipSetId), static_cast<fwMvClipId>(m_clipId)));
			m_flags.SetFlag(Flag_RunningAnimatedReactionSubtask);
			return;
		}
	}

	// Play dying animation if local player
	if (pPed->IsLocalPlayer() && pPed->GetFacialData())
	{
		// Only play the die animation if we're not going to go to the knocked-out idle anim.
		u32 nLastWeaponDamageHash = pPed->GetCauseOfDeath();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( nLastWeaponDamageHash );
		eDamageType damageType = pWeaponInfo ? pWeaponInfo->GetDamageType() : DAMAGE_TYPE_UNKNOWN;
		if( damageType != DAMAGE_TYPE_MELEE && damageType != DAMAGE_TYPE_ELECTRIC )
		{
			pPed->GetFacialData()->PlayDyingFacialAnim(pPed);
		}
	}

	CEntity* pSourceEntity = m_deathSourceInfo.GetEntity();

	// If cause of death was a vehicle, run CTaskFallOver as a subtask
	if (!m_flags.IsFlagSet(Flag_EndingAnimatedDeathFall) && 
		!m_flags.IsFlagSet(Flag_clipsOverridenByDictHash) &&
		(m_deathSourceInfo.GetWeapon() == WEAPONTYPE_RAMMEDBYVEHICLE || m_deathSourceInfo.GetWeapon() == WEAPONTYPE_RUNOVERBYVEHICLE) && 
		m_deathSourceInfo.GetEntity() && m_deathSourceInfo.GetEntity()->GetIsTypeVehicle())
	{
		// the correct anim should have been set up in ComputeDeathAnim()
		if (aiVerify(m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash)) &&
			aiVerify(m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID))
		{
			fwMvClipSetId clipSetId = static_cast<fwMvClipSetId>(m_clipSetId);
			fwMvClipId clipId = static_cast<fwMvClipId>(m_clipId);

			CTaskFallOver* pTask = rage_new CTaskFallOver(clipSetId, clipId, 2.0f);
			pTask->SetCapsuleRestrictionsDisabled(true);
			SetNewTask(pTask);
			CTaskFallOver::ApplyInitialVehicleImpact(SafeCast(CVehicle, pSourceEntity), pPed);
			m_flags.SetFlag(Flag_RunningAnimatedReactionSubtask);
			return;
		}
	}

	if (m_fRootSlopeFixupPhase == 0.0f && !m_flags.IsFlagSet(Flag_EndingAnimatedDeathFall) && m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash) &&
		aiVerify(m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID))
	{
		fwMvClipSetId clipSetId = static_cast<fwMvClipSetId>(m_clipSetId);
		fwMvClipId clipId = static_cast<fwMvClipId>(m_clipId);

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, m_fRootSlopeFixupPhase));
		m_flags.SetFlag(Flag_RunningAnimatedReactionSubtask);
		return;
	}

	taskAssert( !pPed->GetUsingRagdoll() );

	bool bUseSlowBlend = m_deathSourceInfo.GetFallContext()==CTaskFallOver::kContextDrown || (m_flags.IsFlagSet(Flag_EndingAnimatedDeathFall) && pPed->GetWaterLevelOnPed()>ms_fAbortAnimatedDyingFallWaterLevel);
	StartMoveNetwork(pPed, bUseSlowBlend ? REALLY_SLOW_BLEND_DURATION : NORMAL_BLEND_DURATION);

	m_moveNetworkHelper.SendRequest(ms_DyingAnimatedId);
	m_moveNetworkHelper.WaitForTargetState(ms_DyingAnimatedEnterId);

	const crClip* pClip = GetDeathClip();

	// push our clip into the MoVE network
	m_moveNetworkHelper.SetClip(pClip, ms_DeathClipId);
	m_moveNetworkHelper.SetFloat(ms_DyingClipStartPhaseId, m_fStartPhase);

	// B*2500511: Set lowrider flag and window height so we can enter the "DyingAnimated_LowriderLowDoor" motion tree (as long as we can find the clip).
	if (pPed->GetIsInVehicle() && pPed->GetPedIntelligence())
	{
		const CTaskMotionInAutomobile *pTaskInAutomobile = static_cast<const CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
		if (pTaskInAutomobile && pTaskInAutomobile->IsUsingAlternateLowriderClipset())
		{
			const fwMvClipId dieLowDoorClipId("DIE_LOWDOOR", 0xc997914c);
			fwMvClipSetId clipSetId;
			clipSetId.SetHash(m_clipSetId);
			const crClip* pClipLowDoor = fwAnimManager::GetClipIfExistsBySetId(clipSetId, dieLowDoorClipId);
			if (pClipLowDoor)
			{
				fwMvClipId deathLowDoorClipId("DeathClipLowDoor", 0x721e1302);
				m_moveNetworkHelper.SetClip(pClipLowDoor, deathLowDoorClipId);
				m_moveNetworkHelper.SetFlag(true, CTaskMotionInVehicle::ms_UsingLowriderArmClipset);
				float fWindowHeight = pPed->GetVehiclePedInside()->GetVehicleModelInfo() ? pPed->GetVehiclePedInside()->GetVehicleModelInfo()->GetLowriderArmWindowHeight() : 1.0f;
				m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_WindowHeight, fWindowHeight);
			}
		}
	}

	if(taskVerifyf(pClip, "Failed to start death clip!"))
	{
		m_bUseRagdollDuringClip = false;

		if( CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ) )
			m_bUseRagdollDuringClip = CClipEventTags::FindEventPhase( pClip, CClipEventTags::SwitchToRagdoll, m_fRagdollStartPhase );

		if (m_fRootSlopeFixupPhase < 0.0f)
		{
			static const atHashString ms_BlendInRootIkSlopeFixup("BlendInRootIkSlopeFixup",0xDBFC38CA);
			CClipEventTags::FindEventPhase<crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_BlendInRootIkSlopeFixup, m_fRootSlopeFixupPhase);
		}
	}

	m_fFallTime = 0.0f;

	AddBlockingObject(pPed);

	m_bDyingClipEnded = false;

	//! Don't do gravity if we have no collision loaded under us.
	bool bWaitingForCollision = pPed->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION);
	if( (pPed->m_nDEflags.bFrozenByInterior || bWaitingForCollision) )
	{
		pPed->SetUseExtractedZ(true);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::DyingAnimated_OnUpdate( CPed* pPed )
{
	if (m_flags.IsFlagSet(Flag_DisableVehicleImpactsWhilstDying))
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableVehicleImpacts, true);

	if (pPed->GetIsInVehicle() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat))
	{
		return FSM_Continue;
	}

	// Somethings put us into ragdoll - change to the ragdoll dying state to handle it properly
	// If damage has been recorded - switch to ragdoll if possible
	if ((pPed->GetUsingRagdoll() || m_flags.IsFlagSet(Flag_damageImpactRecorded)) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
	{
		SetState(State_DyingRagdoll);
		return FSM_Continue;
	}

	// If CTaskAnimatedHitByExplosion has ended, switch to the ragdoll frame
	if (m_flags.IsFlagSet(Flag_RunningAnimatedReactionSubtask))
	{
		if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			// B* 1582472: Give peds killed by animated explosion and car hit reactions the 
			// chance to settle in ragdoll before switching to the ragdoll frame.
			m_flags.ClearFlag(Flag_RunningAnimatedReactionSubtask);
			SetState(State_DeadAnimated);
			return FSM_Continue;
		}
		else if (pPed->GetVelocity().z<-ms_Tunables.m_MinFallingSpeedForAnimatedDyingFall && m_bCanAnimatedDeadFall)
		{
			// switch to animated dying fall
			m_flags.ClearFlag(Flag_RunningAnimatedReactionSubtask);
			SetState(State_DyingAnimatedFall);
			return FSM_Continue;
		}
	}

	UpdateBlockingObject(pPed);

	if (m_flags.IsFlagSet(Flag_RagdollHasSettled))
	{
		if (pPed->GetIsInVehicle() || m_bDontUseRagdollFrameDueToInVehicleDeath)
		{
			SetState(State_DeadAnimated);
			return FSM_Continue;
		}
		else
		{
			SetState(State_DeadRagdollFrame);
			return FSM_Continue;
		}
	}

	if (!m_moveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// If dying in a vehicle, check if we should apply a force to the vehicle as the ped slumps over
	if (pPed->GetMyVehicle())
	{
		CTaskVehicleFSM::ProcessApplyForce(m_moveNetworkHelper, *pPed->GetMyVehicle(), *pPed);
	}

	// Needs to be before we check whether the dying clip has ended below so that leg IK is disabled on the frame that the animation ends!
	float fCurrentPhase = m_moveNetworkHelper.GetFloat( ms_DeathClipPhaseId );
	if (m_fRootSlopeFixupPhase >= 0.0f && fCurrentPhase >= m_fRootSlopeFixupPhase)
	{
		// If we haven't yet added IK fixup...
		const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pSolver == NULL || pSolver->GetBlendRate() <= 0.0f)
		{
			// Start aligning the ped to the slope...
			pPed->GetIkManager().AddRootSlopeFixup(SLOW_BLEND_IN_DELTA);
		}
	}

	// Blend out leg IK (need to set this each frame!)
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);

	//the dying clip is finished, progress to the dead animated state
	if (m_moveNetworkHelper.GetBoolean(ms_DyingClipEndedId))
	{
		m_bDyingClipEnded = true;
	}

	if (m_bDyingClipEnded && ((((pPed->GetVelocity().Mag2() - MagSquared(pPed->GetGroundVelocity()).Getf()) < 0.01f) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding)) || (pPed->GetIsInVehicle() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat))))
	{
		if (pPed->GetIsInVehicle() || m_bDontUseRagdollFrameDueToInVehicleDeath)
		{
			SetState(State_DeadAnimated);
			return FSM_Continue;
		}
		else
		{
			GoStraightToRagdollFrame();
			SetState(State_DeadRagdollFrame);
			return FSM_Continue;
		}
	}

	// Determine if and when we should switch to ragdoll
	if( m_bUseRagdollDuringClip && fCurrentPhase >= m_fRagdollStartPhase && CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ) )
	{
		SetState(State_DyingRagdoll);
		return FSM_Continue;
	}

	if (!m_flags.IsFlagSet(Flag_droppedWeapon) && m_moveNetworkHelper.GetBoolean(ms_DropWeaponRnd))
	{
		if (m_fDropWeaponPhase == 0.f)
			m_fDropWeaponPhase = fwRandom::GetRandomNumberInRange(fCurrentPhase, 0.9f);

		if (m_fDropWeaponPhase <= fCurrentPhase)
			DropWeapon(pPed);
	}

	// Set the entities height immediately, to stop prone peds sinking through the ground from high impacts
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && CTaskFall::GetIsHighFall(pPed, false, m_fFallTime, 0.0f, 0.0f, GetTimeStep()))
	{
		pPed->m_PedResetFlags.SetEntityZFromGround( 50 );
	}

	if (m_fFallTime>0.1f && m_clipSetId != CTaskWrithe::ms_ClipSetIdDie.GetHash() && !pPed->GetIsInVehicle() && m_bCanAnimatedDeadFall)	// We want to keep writhe death anim
	{
		// transition to the animated dying fall
		SetState(State_DyingAnimatedFall);
		return FSM_Continue;
	}

	// If damage has been recorded (and we can't switch to ragdoll - we already checked earlier in the function) and
	// we're in a vehicle then play an upper-body reaction
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded)) 
	{
		if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
		{
			BlendUpperBodyReactionClip(pPed);
		}
		m_damageInfo.Reset();
		m_flags.ClearFlag(Flag_damageImpactRecorded);
	}
	Assert(m_damageInfo.GetCount() == 0);

	//! If we are wanted to trigger a fall out, but couldn't (e.g. entry points blocked) then try and ragdoll now).
	if(pPed->GetIsInVehicle())
	{
		const s32 iSeatIndex = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
		const CVehicleSeatAnimInfo* pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(iSeatIndex);
		bool bOkToFallOut = false;
		if(pSeatAnimInfo)
		{
			bOkToFallOut = ( (pPed->GetMyVehicle()->InheritsFromBike() && pSeatAnimInfo->GetFallsOutWhenDead() ) || 
				(pSeatAnimInfo->GetCanDetachViaRagdoll() && (pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle() || pPed->GetMyVehicle()->GetIsJetSki()) ) );
		}
		if(bOkToFallOut && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			SetState(State_DyingRagdoll);
			return FSM_Continue;	
		}

		if (pPed->IsNetworkClone())
		{
			CNetObjPed* pPedObj = static_cast<CNetObjPed*>(pPed->GetNetworkObject());

			ObjectId vehicleId = pPedObj->GetPedsTargetVehicleId();

			if (vehicleId == NETWORK_INVALID_OBJECT_ID)
			{
				// the local ped is out of the vehicle, we need to force the clone out in this case
				pPedObj->SetClonePedOutOfVehicle(false, CPed::PVF_Warp);

				// clear anim data so that the ped does a standing death
				m_flags.ClearFlag(Flag_clipsOverridenByDictHash);
				m_flags.ClearFlag(Flag_clipsOverridenByClipSetHash);

				// restart the task
				SetState(State_Start);
				return FSM_Continue;	
			}
		}
	}
	else if(m_bDontUseRagdollFrameDueToInVehicleDeath)
	{
		//! Our vehicle has been deleted. Don't ragdoll frame until we have ragdolled.
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			SetState(State_DyingRagdoll);
			return FSM_Continue;	
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DyingAnimated_OnExit( CPed* pPed )
{
	pPed->ClearBound();
}


//////////////////////////////////////////////////////////////////////////
// DyingAnimated

void CTaskDyingDead::DyingAnimatedFall_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	AddToStateHistory(State_DyingAnimatedFall);

	//start a getup task using the dead getup set
	CTaskGetUp* pGetupTask = rage_new CTaskGetUp(NMBS_DEAD_FALL);
	SetNewTask(pGetupTask);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::DyingAnimatedFall_OnUpdate( CPed* pPed )
{
	if (m_flags.IsFlagSet(Flag_DisableVehicleImpactsWhilstDying))
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableVehicleImpacts, true);

	UpdateBlockingObject(pPed);

	if (m_flags.IsFlagSet(Flag_RagdollHasSettled))
	{
		taskAssert(!m_bDontUseRagdollFrameDueToInVehicleDeath);
		SetState(State_DeadRagdollFrame);
		return FSM_Continue;
	}

	// Somethings put us into ragdoll - change to the ragdoll dying state to handle it properly
	// If damage has been recorded - switch to ragdoll if possible
	if ((pPed->GetUsingRagdoll() || m_flags.IsFlagSet(Flag_damageImpactRecorded)) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
	{
		SetState(State_DyingRagdoll);
		return FSM_Continue;
	}

	// Blend out leg IK (need to set this each frame!)
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);

	// if we end up submerged in water, transition to a drown animation.
	if (pPed->GetWaterLevelOnPed()>ms_fAbortAnimatedDyingFallWaterLevel)
	{
		fwMvClipSetId clipSet(CLIP_SET_ID_INVALID);
		fwMvClipId clip(CLIP_ID_INVALID);

		// pick a default fall anim
		CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextDrown, CTaskFallOver::kDirFront, clipSet, clip);

		SetDeathAnimationBySet(clipSet, clip);
		SetState(State_DyingAnimated);
		m_flags.SetFlag(Flag_EndingAnimatedDeathFall);
		return FSM_Continue;
	}
	// Once the capsule hits ground, play the landing animation (in the dying animated state)
	else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding))
	{
		fwMvClipSetId clipSet(CLIP_SET_ID_INVALID);
		fwMvClipId clip(CLIP_ID_INVALID);

		// no longer falling. Switch to land anim and transition to dyinganimated to play it back

		// get the land animation from the next item of the getup set.
		if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_GET_UP)
		{			
			CTaskGetUp* pTask = static_cast<CTaskGetUp*>(GetSubTask());
			CNmBlendOutItem* pNextItem = pTask->GetNextBlendOutItem();
			if (pNextItem && pNextItem->IsPoseItem())
			{
				clipSet = pNextItem->GetClipSet();
				clip = pNextItem->GetClip();
			}
		}

		// if the getupset doesn't specify an anim, grab the default fall one.
		if (clipSet==CLIP_SET_ID_INVALID)
		{
			// pick a default fall anim
			CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextFallImpact, CTaskFallOver::kDirFront, clipSet, clip);
		}

		SetDeathAnimationBySet(clipSet, clip);
		SetState(State_DyingAnimated);
		m_flags.SetFlag(Flag_EndingAnimatedDeathFall);
		return FSM_Continue;
	}

	// If damage has been recorded (and we can't switch to ragdoll - we already checked earlier in the function) and
	// we're in a vehicle then play an upper-body reaction
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded)) 
	{
		m_damageInfo.Reset();
		m_flags.ClearFlag(Flag_damageImpactRecorded);
	}
	Assert(m_damageInfo.GetCount() == 0);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DyingAnimatedFall_OnExit( CPed* UNUSED_PARAM(pPed) )
{
	// move along, nothing to see here...
}

//////////////////////////////////////////////////////////////////////////
// DyingRagdoll

//dev_bool sbForceImpulsesWhenDead = true;

void CTaskDyingDead::DyingRagdoll_OnEnter( CPed* pPed )
{
	//! Can reset this now - it's safe to ragdoll frame if we ragdoll.
	m_bDontUseRagdollFrameDueToInVehicleDeath = false;

	AddToStateHistory(State_DyingRagdoll);

	m_moveNetworkHelper.ReleaseNetworkPlayer();

	// Switch to ragdoll if not switched already
	if(!pPed->GetUsingRagdoll() && CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ))
		 pPed->SwitchToRagdoll(*this);

	// Play dying animation if local player
	if (pPed->IsLocalPlayer() && pPed->GetFacialData())
	{
		// Only play the die animation if we're not going to go to the knocked-out idle anim.
		u32 nLastWeaponDamageHash = pPed->GetCauseOfDeath();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( nLastWeaponDamageHash );
		eDamageType damageType = pWeaponInfo ? pWeaponInfo->GetDamageType() : DAMAGE_TYPE_UNKNOWN;
		if( damageType != DAMAGE_TYPE_MELEE && damageType != DAMAGE_TYPE_ELECTRIC )
		{
			pPed->GetFacialData()->PlayDyingFacialAnim(pPed);
		}
	}

	// Dying ragdolls shouldn't sleep.  Sleep should only be allowed once any NM Performance has ended
	DisableSleep(pPed);

	if (pPed->GetUsingRagdoll() && pPed->GetRagdollInst()->GetCached() &&
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body)
	{
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->SetBodyMinimumStiffness(0.0f);
		pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body->SetStiffness(0.5f);
	}

	// Disable head look
	pPed->GetIkManager().AbortLookAt(0);

	// A damage impact has been recorded - react to it here
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded) )
	{
		if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
		{
			for(s32 iDamage = 0; iDamage < m_damageInfo.GetCount(); iDamage++)
			{
				float fImpulseMag = m_damageInfo[iDamage].GetImpulseMag();
				Vector3 vImpulseDir = m_damageInfo[iDamage].GetImpulseDir();
				u32 iWeaponHash = m_damageInfo[iDamage].GetWeaponHash();

				// apply force to the ragdoll if its active
				if( m_damageInfo[iDamage].GetIntersectionInfo() && 
					fImpulseMag > 0.0f && vImpulseDir.IsNonZero() )
				{
					if (taskVerifyf(pPed->GetRagdollInst(), "NULL ragdoll inst pointer in CTaskDying::DyingRagdoll_OnEnter"))
					{
						int numbounds = pPed->GetRagdollInst()->GetCacheEntry()->GetBound()->GetNumActiveBounds();
						if (m_damageInfo[iDamage].GetIntersectionComponent() < numbounds)
						{
							pPed->GetRagdollInst()->ApplyBulletForce(pPed, fImpulseMag, vImpulseDir, m_damageInfo[iDamage].GetWorldIntersectionPos(pPed), m_damageInfo[iDamage].GetIntersectionComponent(), iWeaponHash);
						}
					}
				}
			}
		}
		m_damageInfo.Reset();
		m_flags.ClearFlag(Flag_damageImpactRecorded);
	}
	Assert(m_damageInfo.GetCount() == 0);

	if (pPed->IsNetworkClone() && m_flags.IsFlagSet(Flag_naturalMotionTaskSpecified) && !m_pForcedNaturalMotionTask)
	{
		taskWarningf("CTaskDyingDead::DyingRagdoll_OnEnter: %s had the Flag_naturalMotionTaskSpecified set, but no m_pForcedNaturalMotionTask", pPed->GetNetworkObject()->GetLogName());

		m_flags.ClearFlag(Flag_naturalMotionTaskSpecified);
	}

	if( m_flags.IsFlagSet(Flag_naturalMotionTaskSpecified))
	{
#if __ASSERT
		taskAssert(m_pForcedNaturalMotionTask);
		if (m_pForcedNaturalMotionTask)	
		{
			taskAssertf(dynamic_cast<CTaskNMControl*>(m_pForcedNaturalMotionTask.Get()), "Task %s isn't an nm control task", m_pForcedNaturalMotionTask->GetName().c_str());
		}
#endif
		if (IsRunningLocally() && m_pForcedNaturalMotionTask && static_cast<CTask*>(m_pForcedNaturalMotionTask.Get())->IsClonedFSMTask())
		{
			static_cast<CTaskFSMClone*>(m_pForcedNaturalMotionTask.Get())->SetRunningLocally(true);
		}

		SetNewTask(m_pForcedNaturalMotionTask);
		m_pForcedNaturalMotionTask = NULL;
		m_flags.ClearFlag(Flag_naturalMotionTaskSpecified);
	}
	// No natural motion task has been specified, use a simple relax
	else if( !GetIsFlagSet(aiTaskFlags::RestartCurrentState) )
	{
		pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the next line to route to TaskRageRagdoll
		if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
			CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);

		CTaskNMControl* pNMControlTask = rage_new CTaskNMControl(1000, 10000, rage_new CTaskNMRelax(1000, 10000), CTaskNMControl::ALL_FLAGS_CLEAR);
		SetNewTask(pNMControlTask);

		if (pPed->IsNetworkClone())
		{
			pNMControlTask->SetRunningLocally(true);
		}
	}

	// Request_DEPRECATED that the natural motion control task doesn't blend back to the idle clip
	// when it's finished with the main behaviour task. Expects that the NM control task is either
	// already running as the immediate subtask or, if it's just been instantiated above, as a new
	// task ready to be stored as the subtask.
	CTaskNMControl* pNMControlTask = NULL;
	if(GetNewTask() && taskVerifyf(GetNewTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL, "new subtask (%s) isn't an nm control task", GetNewTask()->GetName().c_str()))
	{
		pNMControlTask = smart_cast<CTaskNMControl*>(GetNewTask());
	}
	else if (GetSubTask() && taskVerifyf(GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL, "subtask (%s) isn't an nm control task", GetSubTask()->GetName().c_str()))
	{
		pNMControlTask = smart_cast<CTaskNMControl*>(GetSubTask());
	}

	if (taskVerifyf(pNMControlTask, "Sub-task was null"))
	{
		pNMControlTask->ClearFlag(CTaskNMControl::DO_BLEND_FROM_NM);
	}

	AddBlockingObject(pPed);

	// Non-player peds who get killed by explosions should drop their weapons right away. This helps prevents
	//  situations where ragdolls are unable to fall asleep on top of weapons. 
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_deathSourceInfo.GetWeapon());
	if (pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE && !pPed->IsAPlayerPed())
	{
		if(!m_flags.IsFlagSet(Flag_droppedWeapon))
		{
			DropWeapon(pPed);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::DyingRagdoll_OnUpdate( CPed* pPed )
{
	// Peds should not be allowed to sleep wile they're dying, since they might happen to sleep in a strange pose being driven to by an NM behavior.
	taskAssert(!m_flags.IsFlagSet(Flag_SleepingIsAllowed));

	UpdateBlockingObject(pPed);

	// If ped exiting a vehicle, force them ped away from the vehicle they were in
	ProcessPushFromVehicle();

	// Mark that this task is still in control of the peds ragdoll
	if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
	{
		pPed->TickRagdollStateFromTask(*this);
	}

	// Throw a held weapon away after some time
	CTaskNMShot* pShotTask = static_cast<CTaskNMShot*>(FindSubTaskOfType(CTaskTypes::TASK_NM_SHOT));
	u32 uTimeToThrowWeaponMS = (pShotTask != NULL && pShotTask->IsSprinting()) ? 0 : pPed->IsPlayer() ? ms_Tunables.m_TimeToThrowWeaponPlayerMS : ms_Tunables.m_TimeToThrowWeaponMS;
	if (!NetworkInterface::IsGameInProgress() && !m_flags.IsFlagSet(Flag_droppedWeapon) && fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfDeath() > uTimeToThrowWeaponMS)
	{
		DropWeapon(pPed);
		pPed->CreateDeadPedPickups();
		m_flags.SetFlag(Flag_PickupsDropped);
	}

	if (m_flags.IsFlagSet(Flag_RagdollHasSettled))
	{
		SetState(State_DeadRagdollFrame);
		return FSM_Continue;
	}

	CTaskNMInjuredOnGround* pInjuredOnGroundTask = static_cast<CTaskNMInjuredOnGround*>(FindSubTaskOfType(CTaskTypes::TASK_NM_INJURED_ON_GROUND));
	if (pInjuredOnGroundTask != NULL)
	{
		// If running the injuredOnGround and the flag to disable it has been set, switch to the dead state
		if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableInjuredOnGround) && CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ))
		{
			SetState(State_DeadRagdoll);
			return FSM_Continue;
		}
	}
	else
	{
		// If we've been dying for too long (and we're not playing out an injured performance) and our velocity is small, switch to the dead ragdoll state
		TUNE_GROUP_INT(FORCE_QUIT_DYING_RAGDOLL_MAX_TIME, iForceQuitDyingRagdollMaxTime, 5000, 0, 30000, 1);
		TUNE_GROUP_FLOAT(FORCE_QUIT_DYING_RAGDOLL_MIN_VELOCITY, fForceQuitDyingRagdollMinVelocity, 1.0f, 0.0f, 10.0f, 0.1f);
		if ((fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfDeath()) > static_cast<u32>(iForceQuitDyingRagdollMaxTime) && pPed->GetVelocity().Mag2() < square(fForceQuitDyingRagdollMinVelocity))
		{
			nmWarningf("We've been in the DyingRagdoll state for too long with a low velocity - switch to the dead ragdoll state");
			SetState(State_DeadRagdoll);
			return FSM_Continue;
		}
	}

	// If offscreen, snap the ped to the ground
	if (IsOffscreenAndCanSnapToGround(pPed))
	{
		nmTaskDebugf(this, "IsOffscreenAndCanSnapToGround returned TRUE - switching to static frame and moving to RagdollAborted");

		// Switch to a static animated frame and set the corpse pose (pose to take effect the next frame at the latest)
		if(pPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE)
		{
			m_fRagdollAbortVelMag = pPed->GetVelocity().Mag();

			pPed->SwitchToAnimated(true, true, true, false, false, true, true);
			// We've now switched to the static frame - make sure our child nm control task 
			// doesn't switch us to animated when it blends out, or we'll get a bind pose
			CTask* pTask = GetSubTask();
			if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_NM_CONTROL)
			{
				static_cast<CTaskNMControl*>(pTask)->SetDontSwitchToAnimatedOnAbort(true);
			}
		}

		// Need to go to the ragdoll aborted state as it can properly deal with going from static frame -> animated and then
		// switching to the DeadAnimated state
		SetState(State_RagdollAborted);
		return FSM_Continue;
	}

	// If the subtask finished, work out what to do next
	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		// the ped isn't ragdolling anymore, force an animated frame
		if (pPed->GetRagdollState()!=RAGDOLL_STATE_PHYS || !CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ))
		{
			nmTaskDebugf(this, "Dying ragdoll task ended unexpectedly - switching to dead animated!");
			SetState(State_RagdollAborted);
			return FSM_Continue;
		}
		else
		{
			bool headShot = (pPed->GetPrimaryWoundData()->valid && pPed->GetPrimaryWoundData()->component == RAGDOLL_HEAD) ||
				(pPed->GetSecondaryWoundData()->valid && pPed->GetSecondaryWoundData()->component == RAGDOLL_HEAD);

			bool cuffed = pPed->GetRagdollConstraintData() && 
				(pPed->GetRagdollConstraintData()->HandCuffsAreEnabled() || pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled());

			// Avoid running the injuredOnGround reaction for deaths other than those cause by bullets/explosions, or if cuffed
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_deathSourceInfo.GetWeapon());
			const CEntity* pSourceEntity = m_deathSourceInfo.GetEntity();

			// Don't do this in a network game as it prevents peds from migrating and we want dead peds to be removed quickly
			if (!m_flags.IsFlagSet(Flag_doInjuredOnGround) && pPed->GetNetworkObject() == NULL &&
				pPed->GetVelocity().z > CTaskNMInjuredOnGround::sm_Tunables.m_fFallingSpeedThreshold && 
				!headShot && !cuffed && !pPed->GetIsInWater() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
			{
				// Randomly choose whether to use the injured on ground behaviour (or force it if requested by script)
				if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ForceInjuredOnGround) && CTaskNMInjuredOnGround::GetNumInjuredOnGroundAgents() == 0 &&
					!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll))
				{
					// Force the duration if specified by script
					int iInjuredTime = Clamp(pPed->GetPedIntelligence()->GetCombatBehaviour().GetForcedInjuredOnGroundDuration(), 0, 65535);
					SetNewTask(rage_new CTaskNMControl(iInjuredTime, iInjuredTime, rage_new CTaskNMInjuredOnGround(iInjuredTime, iInjuredTime), CTaskNMControl::ALL_FLAGS_CLEAR));
				}
				else if (pSourceEntity != NULL && pSourceEntity->GetType() == ENTITY_TYPE_PED && !pPed->IsPlayer() && 
					pWeaponInfo != NULL && (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE))
				{
					float fResult = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
					if (!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableInjuredOnGround) && CTaskNMInjuredOnGround::GetNumInjuredOnGroundAgents() == 0 &&
						!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll) && fResult <= CTaskNMInjuredOnGround::sm_Tunables.m_fDoInjuredOnGroundChance)
					{
						// Otherwise pick a random duration
						TUNE_GROUP_INT(INJURED_ON_GROUND, fInjuredOnGroundRandomDurationMin, 10000, 0, 65535, 1);
						TUNE_GROUP_INT(INJURED_ON_GROUND, fInjuredOnGroundRandomDurationMax, 35000, 0, 65535, 1);
						int iInjuredTime = fwRandom::GetRandomNumberInRange(CTaskNMInjuredOnGround::sm_Tunables.m_iRandomDurationMin, CTaskNMInjuredOnGround::sm_Tunables.m_iRandomDurationMax);
						SetNewTask(rage_new CTaskNMControl(iInjuredTime, iInjuredTime, rage_new CTaskNMInjuredOnGround(iInjuredTime, iInjuredTime), CTaskNMControl::ALL_FLAGS_CLEAR));
					}
					else
					{
						const CTaskNMSimple::Tunables::Tuning* pTuning = CTaskNMSimple::sm_Tunables.GetTuning("DeathWrithe");
						if (pTuning != NULL)
						{
							SetNewTask(rage_new CTaskNMControl(500, 5000, rage_new CTaskNMSimple(*pTuning), CTaskNMControl::ALL_FLAGS_CLEAR));
						}
					}
				}
			}

			// If no task was generated then we move to the next state
			if (GetNewTask() == NULL)
			{
				SetState(State_DeadRagdoll);
			}

			m_flags.SetFlag(Flag_doInjuredOnGround);

			return FSM_Continue;
		}
	}
	else
	{
		// The dying state should always have an nm control task as a sub task (even if running a rage ragdoll)
		taskAssert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_CONTROL);
	}

	bool bRunningNMRelax = GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL && 
		smart_cast<CTaskNMControl*>(GetSubTask())->GetSubTask() && smart_cast<CTaskNMControl*>(GetSubTask())->GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_RELAX;
		 
	// Send animals, lower LOD humans, and NM agents running NMRelax directly to State_DeadRagdoll
	if (bRunningNMRelax || pPed->GetRagdollInst()->GetARTAssetID() < 0 || pPed->GetRagdollInst()->GetCurrentPhysicsLOD() != fragInst::RAGDOLL_LOD_HIGH)
	{
		if (CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ))
		{
			// If it was a shot that we were trying to apply to an NM agent, apply it here
			if (!bRunningNMRelax && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL && 
				smart_cast<CTaskNMControl*>(GetSubTask())->GetForcedSubTask() && smart_cast<CTaskNMControl*>(GetSubTask())->GetForcedSubTask()->GetTaskType() == CTaskTypes::TASK_NM_SHOT)
			{
				const CTaskNMShot *pShotTask = smart_cast<const CTaskNMShot *>(smart_cast<CTaskNMControl*>(GetSubTask())->GetForcedSubTask());
				int numbounds = pPed->GetRagdollInst()->GetCacheEntry()->GetBound()->GetNumActiveBounds();
				if (pShotTask && pShotTask->GetComponent() < numbounds)
				{
					float impulseMag = pShotTask->GetWorldImpulse().Mag();
					Vector3 worldHitPos;
					pShotTask->GetWorldHitPosition(pPed, worldHitPos);
					pPed->GetRagdollInst()->ApplyBulletForce(pPed, impulseMag, pShotTask->GetWorldImpulse() / impulseMag, 
						worldHitPos, pShotTask->GetComponent(), pShotTask->GetWeaponHash());
				}
			}

			SetState(State_DeadRagdoll);
		}
		else
		{
			SetState(State_DeadAnimated);
		}

		return FSM_Continue;
	}

	// A damage impact has been recorded - restart the state to correctly react to the event
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded) )
	{
		// If writhing in pain on the ground when a shot comes in, let it reset you to a relax behavior
		if ((pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_INJURED_ON_GROUND) || !GetSubTask())
			&& CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the next line to route to TaskRageRagdoll
			if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
				CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);
			SetNewTask(rage_new CTaskNMControl(1000, 10000, rage_new CTaskNMRelax(1000, 10000), CTaskNMControl::ALL_FLAGS_CLEAR) );
		}

		SetFlag(aiTaskFlags::RestartCurrentState);
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

		if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
		{
			for(s32 iDamage = 0; iDamage < m_damageInfo.GetCount(); iDamage++)
			{
				float fImpulseMag = m_damageInfo[iDamage].GetImpulseMag();
				Vector3 vImpulseDir = m_damageInfo[iDamage].GetImpulseDir();
				u32 iWeaponHash = m_damageInfo[iDamage].GetWeaponHash();

				// apply force to the ragdoll if its active
				if( m_damageInfo[iDamage].GetIntersectionInfo() && 
					fImpulseMag > 0.0f && vImpulseDir.IsNonZero() )
				{
					int numbounds = pPed->GetRagdollInst()->GetCacheEntry()->GetBound()->GetNumActiveBounds();
					if (m_damageInfo[iDamage].GetIntersectionComponent() < numbounds)
					{
						pPed->GetRagdollInst()->ApplyBulletForce(pPed, fImpulseMag, vImpulseDir, m_damageInfo[iDamage].GetWorldIntersectionPos(pPed), m_damageInfo[iDamage].GetIntersectionComponent(), iWeaponHash);
					}
				}
			}
		}
		m_damageInfo.Reset();
		m_flags.ClearFlag(Flag_damageImpactRecorded);

		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// Dead Animated

void CTaskDyingDead::DeadAnimated_OnEnter( CPed* pPed )
{
	AddToStateHistory(State_DeadAnimated);

	InitDeadState();

	if (IsOffscreenAndCanSnapToGround(pPed))
	{
		// only choose a random death clip from the default dead clip set
		//  if there isn't an override blend out set specified.
		const fwClipSetWithGetup* pSet = CTaskGetUp::FindClipSetWithGetup(pPed);
		if (!pSet || pSet->GetDeadBlendOutSet()==NMBS_INVALID)
		{
			fwMvClipId newClipId;
			if (fwClipSetManager::PickRandomClip(CLIP_SET_DEAD, newClipId))
			{
				const crClip* pRandClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_DEAD, newClipId);
				if (pRandClip)
				{
					SetDeathAnimationBySet(CLIP_SET_DEAD, newClipId, m_fBlendDuration, m_fStartPhase);
				}
			}
		}

		// Probe for the ground position and normal
		Assert(pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetCacheEntry() && pPed->GetRagdollInst()->GetCacheEntry()->GetBound());
		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 pelvisMat;
		pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
		WorldProbe::CShapeTestProbeDesc losDesc;
		WorldProbe::CShapeTestResults probeResults(1);
		losDesc.SetResultsStructure(&probeResults);
		static float depth = 10.0f;
		Vector3 vecStart = pelvisMat.d;
		Vector3 vecEnd = pelvisMat.d;
		vecEnd.z -= depth;
		losDesc.SetStartAndEnd(vecStart, vecEnd);
		u32 nIncludeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE;
		losDesc.SetIncludeFlags(nIncludeFlags);
		losDesc.SetExcludeInstance(pPed->GetCurrentPhysicsInst());
		if (WorldProbe::GetShapeTestManager()->SubmitTest(losDesc))
		{
			Matrix34 startMat;
			startMat.d = probeResults[0].GetHitPosition();
			startMat.d.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();
			startMat.c = probeResults[0].GetHitNormal();
			startMat.a = Vector3(1.0f,0.0f,0.0f);
			startMat.b.Cross(startMat.c, startMat.a);
			startMat.b.Normalize();
			startMat.a.Cross(startMat.b, startMat.c);
			startMat.RotateLocalZ(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
			pPed->SetMatrix(startMat);

			// Disable the constraint, since we're angling the pose based on the ground normal
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);

			// Stop any velocity
			pPed->SetDesiredVelocity(VEC3_ZERO);
			pPed->SetDesiredAngularVelocity(VEC3_ZERO);

			m_SnapToGroundStage = kSnapPoseRequested;
		}
		else
		{
			m_SnapToGroundStage = kSnapFailed;
		}
	}
	else
	{
		m_SnapToGroundStage = kSnapPoseRequested;
	}

	if (!m_flags.IsFlagSet(Flag_startDead) && m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash) && aiVerify(m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID))
	{
		fwMvClipSetId clipSetId = static_cast<fwMvClipSetId>(m_clipSetId);
		fwMvClipId clipId = static_cast<fwMvClipId>(m_clipId);

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 1.0f, 0.0f, true));
	}
	else
	{
		StartMoveNetwork(pPed, NORMAL_BLEND_DURATION);

		m_moveNetworkHelper.SendRequest(ms_DeadAnimatedId);
		m_moveNetworkHelper.WaitForTargetState(ms_DeadAnimatedEnterId);

		// push our clip into the MoVE network
		const crClip* pClip = GetDeathClip();
		m_moveNetworkHelper.SetClip(pClip, ms_DeathClipId);
	}

	TriggerBloodPool( pPed );

	// Add navmesh blocking bound to prevent wandering peds from walking over dead bodies
	AddBlockingObject(pPed);

	m_SnapToGroundStage = kSnapPoseRequested;

	// If we have a clone ped starting dead then they shouldn't need aligning to the slope
	// (I believe this is already accounted for in the ped matrix orientation)
	if (!pPed->IsNetworkClone() || !m_flags.IsFlagSet(Flag_startDead))
	{
		pPed->GetIkManager().AddRootSlopeFixup(INSTANT_BLEND_IN_DELTA);
	}

	m_fFallTime = 0.0f;
}	

//////////////////////////////////////////////////////////////////////////


CTask::FSM_Return CTaskDyingDead::DeadAnimated_OnUpdate( CPed* pPed)
{
	if (pPed->GetUsingRagdoll())
	{
		SetState(State_DeadRagdoll);
		return FSM_Continue;
	}

	if (m_moveNetworkHelper.IsNetworkActive() && !m_moveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// If damage has been recorded - switch to ragdoll if possible
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded) && pPed->GetMovePed().IsTaskNetworkFullyBlended() )
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			// Switch to a high ragdoll LOD if being fired upon
			if (m_damageInfo.GetCount())
			{	
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_damageInfo[0].GetWeaponHash());
				if (pWeaponInfo && (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
				{
					pPed->GetRagdollInst()->SetSuppressHighRagdollLODOnActivation(false);
				}
			}

			pPed->SwitchToRagdoll(*this);

			SetState(State_DeadRagdoll);
			return FSM_Continue;
		}
	}

	// if the ped is done moving, disable the capsule so things don't push it around
	if (pPed->GetIsStanding() &&  pPed->GetWasStanding() && pPed->GetVelocity().Mag2()<0.001f)
	{
		pPed->StopAllMotion(true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule, true);
	}

	if(pPed->GetIsInVehicle())
	{
		//! If we are wanted to trigger a fall out, but couldn't (e.g. entry points blocked) then try and ragdoll now).
		const s32 iSeatIndex = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
		const CVehicleSeatAnimInfo* pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(iSeatIndex);
		bool bOkToFallOut = false;
		if(pSeatAnimInfo)
		{
			bOkToFallOut = ( (pPed->GetMyVehicle()->InheritsFromBike() && pSeatAnimInfo->GetFallsOutWhenDead() ) || 
				(pSeatAnimInfo->GetCanDetachViaRagdoll() && (pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle() || pPed->GetMyVehicle()->GetIsJetSki() || pPed->GetMyVehicle()->InheritsFromBicycle()) ) );
		}
		if(bOkToFallOut && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			SetState(State_DeadRagdoll);
			return FSM_Continue;	
		}
	}
	else if(m_bDontUseRagdollFrameDueToInVehicleDeath)
	{
		//! Our vehicle has been deleted. Don't ragdoll frame until we have ragdolled.
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			SetState(State_DeadRagdoll);
			return FSM_Continue;	
		}
	}
	else
	{
		if (m_SnapToGroundStage == kSnapFailed)
		{
			if (pPed->GetMovePed().IsTaskNetworkFullyBlended())
			{
				// switch to the ragdoll frame state after the character is fully posed in the dead animation.
				GoStraightToRagdollFrame();
				SetState(State_DeadRagdollFrame);
				return FSM_Continue;
			}
		}
		else
		{
			// Disable the constraint, since we're angling the pose based on the ground normal
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);

			// Stop any velocity
			pPed->SetDesiredVelocity(Vector3(0.0f,0.0f,0.0f));
			pPed->SetDesiredAngularVelocity(Vector3(0.0f,0.0f,0.0f));

			if (m_SnapToGroundStage == kSnapPoseRequested)
			{
				// TODO - make a better check than just assuming that waiting a frame is enough, or even needed.
				// I found that this frame wait wasn't needed if the ped was previously ragdolling, but if animated prior then without this wait
				// the ped would not get teleported and posed on the ground.
				m_SnapToGroundStage = kSnapPoseReceived; 
			}
			else if (m_SnapToGroundStage == kSnapPoseReceived)
			{
				// If we're starting dead then we need to ragdoll the ped to settle them to the ground properly - otherwise they can be intersecting geometry
				// We want to do this even if the ped is on-screen (in fact - we especially want to do this if the ped is on-screen)
				// This should allow the ped to ragdoll if there's a ragdoll available or if they are on-screen and there is another ragdoll with lower priority (i.e. one that is off-screen)
				if (pPed->GetMovePed().IsTaskNetworkFullyBlended()
					&& CTaskNMBehaviour::RagdollPoolHasSpaceForPed(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed, pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen), 0.5f /*SCORE_AMB_DEAD*/)
					&& CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
				{
					// synch the last and current matrices, then switch to ragdoll
					PHSIM->SetLastInstanceMatrix(pPed->GetRagdollInst(), pPed->GetRagdollInst()->GetMatrix());
					phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
					if (bound)
					{
						bound->UpdateLastMatricesFromCurrent();
					}

					pPed->SwitchToRagdoll(*this);

					if (pPed->GetRagdollInst()->GetArticulatedBody())
						pPed->GetRagdollInst()->GetArticulatedBody()->Freeze();

					SetState(State_DeadRagdoll);
					return FSM_Continue;
				}
			}
		}
	}

	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding) && !pPed->GetIsInVehicle() && pPed->GetCollider()) // Not falling inside a vehicle tbh
	{
		m_fFallTime += fwTimer::GetTimeStep();
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}
	else
	{
		m_fFallTime = 0.0f;
	}

	static float m_fFallTimeForDyingFall = 0.35f;
	if (m_fFallTime>m_fFallTimeForDyingFall && m_bCanAnimatedDeadFall)
	{
		SetState(State_DyingAnimatedFall);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::OnDeadExit( CPed &ped )
{
	// In non-network games, do this at the end.
	if ( !NetworkInterface::IsGameInProgress() && !m_flags.IsFlagSet(Flag_PickupsDropped) && !m_flags.IsFlagSet(Flag_startDead) && !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_PedBeingDeleted ) && !ped.IsNetworkClone())
	{
		// The ped zeros it's money member so this has adequate protection 
		// if it gets called/restarted more than once.
		DropWeapon(&ped);
		ped.CreateDeadPedPickups();
		m_flags.SetFlag(Flag_PickupsDropped);
	}
}

void CTaskDyingDead::ComputeDeathAnim(CPed* pPed)
{
	// has an anim already been externally specified?
	bool bComputeAnim = !m_flags.IsFlagSet(Flag_clipsOverridenByDictHash) && !m_flags.IsFlagSet(Flag_clipsOverridenByClipSetHash);

	// if we have a clone ped starting dead and no anim streamed in for him, just default to a resident one
	if (pPed->IsNetworkClone() && !bComputeAnim && m_flags.IsFlagSet(Flag_startDead) && !GetDeathClip(false))
	{
		bComputeAnim = true;
	}

	// only compute death anim if there is no anim already specified
	if (bComputeAnim)
	{
		float blendDuration = NORMAL_BLEND_DURATION;

		CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
		const bool bExitingVehicle = pExitVehicleTask && pExitVehicleTask->GetState() > CTaskExitVehicle::State_PickDoor;

		if (bExitingVehicle)
		{
			m_flags.SetFlag(Flag_ragdollWhenAble);
		}

		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !bExitingVehicle)
		{
			Assert(pPed->GetMyVehicle());

			if(pPed->GetMyVehicle())
			{
				const CVehicleSeatAnimInfo* pVehicleSeatInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed);
				Assert(pVehicleSeatInfo);

				s32 iDictIndex = -1;
				u32 iAnimHash = 0;
				if (pVehicleSeatInfo->FindAnim(CVehicleSeatAnimInfo::DEAD, iDictIndex, iAnimHash, false))
				{
					SetDeathAnimationByDictionary(iDictIndex, iAnimHash, blendDuration);
					m_flags.SetFlag(Flag_DeathAnimSetInternally);
				}
				else
				{
					aiAssertf(0, "Failed to find default vehicle death anim for ped %s - ped's seat has no anim, vehicle %s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", pPed->GetMyVehicle()->GetModelName());
				}
			}
			else
			{
				aiAssertf(0, "Failed to find default vehicle death anim for ped %s - ped has no vehicle", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
			}
		}
		else
		{
			//if this is an animal, use the dying anim in it's default clipset 
			fwMvClipSetId clipSetId = pPed->GetPedModelInfo()->GetMovementClipSet();
			fwMvClipId clipId = CLIP_NON_HUMAN_DYING;

			fwClipSet* pSet = fwClipSetManager::GetClipSet(clipSetId);	

			aiAssertf(pSet, "Failed to find default death anim for ped %s - invalid movement clip set %d", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", clipSetId.GetHash());

			if (!pSet || !pSet->GetClip(clipId))
			{
				clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();

				if (m_deathSourceInfo.GetWeapon() == WEAPONTYPE_DROWNING || (pPed->GetIsInWater() && !pPed->GetIsStanding()))
				{
					CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextDrown, CTaskFallOver::kDirFront, clipSetId, clipId);
					aiAssertf(clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID, "Failed to find default drowning anim for ped %s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
				}
				else if (m_deathSourceInfo.GetWeapon() == WEAPONTYPE_BLEEDING && CTaskWrithe::GetDieClipSetAndRandClip(clipSetId, clipId))
				{
					blendDuration = REALLY_SLOW_BLEND_DURATION;
					aiAssertf(clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID, "Failed to find default bleeding anim for ped %s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
				}
				else if ((m_deathSourceInfo.GetWeapon() == WEAPONTYPE_RAMMEDBYVEHICLE || m_deathSourceInfo.GetWeapon() == WEAPONTYPE_RUNOVERBYVEHICLE) && m_deathSourceInfo.GetEntity() && m_deathSourceInfo.GetEntity()->GetIsTypeVehicle())
				{
					CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextVehicleHit, CTaskFallOver::kDirBack, clipSetId, clipId);
					aiAssertf(clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID, "Failed to find default rammed by vehicle anim for ped %s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
				}
				else
				{
					Vector3 vecHeadPos(0.0f,0.0f,0.0f);
					pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);

					// No need to use an on-ground damage animation if starting off dead since in that case we start the animation at phase 1.0 
					// which should force the ped to be on the ground anyway
					if(!m_flags.IsFlagSet(Flag_startDead) && vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf() - 0.2f)
					{
						const EstimatedPose pose = pPed->EstimatePose();
						if( pose == POSE_ON_BACK )
							clipId = CLIP_DAM_FLOOR_BACK;
						else
							clipId = CLIP_DAM_FLOOR_FRONT;

						Matrix34 rootMatrix;
						if (pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
						{
							float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
							if (fAngle < -QUARTER_PI)
							{
								clipId = CLIP_DAM_FLOOR_LEFT;
							}
							else if (fAngle > QUARTER_PI)
							{
								clipId = CLIP_DAM_FLOOR_RIGHT;
							}
						}

						m_fRootSlopeFixupPhase = 0.0f;
					}
					else
					{
						CTaskFallOver::PickFallAnimation(pPed, m_deathSourceInfo.GetFallContext(), m_deathSourceInfo.GetFallDirection(), clipSetId, clipId);
						aiAssertf(clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID, "Failed to find default death anim for ped %s (fall context: %d, fall direction: %d", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "", m_deathSourceInfo.GetFallContext(), m_deathSourceInfo.GetFallDirection());
					}
				}
			}
			else
			{
				aiAssertf(pSet, "Failed to find default death anim for ped %s - invalid clip CLIP_NON_HUMAN_DYING", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
			}

			if (clipSetId != CLIP_SET_ID_INVALID && clipId != CLIP_ID_INVALID)
			{
				SetDeathAnimationBySet(clipSetId, clipId, blendDuration);
				m_flags.SetFlag(Flag_DeathAnimSetInternally);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DeadAnimated_OnExit( CPed* pPed )
{
	OnDeadExit( *pPed );
}

//////////////////////////////////////////////////////////////////////////
// Ragdoll aborted

void CTaskDyingDead::RagdollAborted_OnEnter( CPed* pPed )
{
	AddToStateHistory(State_RagdollAborted);

	eNmBlendOutSet deadBlendOutSet(NMBS_INVALID);

	const fwClipSetWithGetup* pSet = CTaskGetUp::FindClipSetWithGetup(pPed);
	if (pSet)
	{
		// use the dead blend out set specified by the movement clip set
		deadBlendOutSet = pSet->GetDeadBlendOutSet();
	}
	
	if (deadBlendOutSet==NMBS_INVALID)
	{
		// default - use the human death blend out sets.
		deadBlendOutSet = pPed && pPed->GetIsVisibleInSomeViewportThisFrame() ?  NMBS_DEATH_HUMAN : NMBS_DEATH_HUMAN_OFFSCREEN;
	}

	bool bUseCurrentPose = false;

	CNmBlendOutSetList blendOutSetList;
	blendOutSetList.Add(deadBlendOutSet);
	if (blendOutSetList.GetFilter().GetKeyCount() > 0
		&& m_fRagdollAbortVelMag<ms_Tunables.m_RagdollAbortPoseMaxVelocity
		&& pPed->GetFrameCollisionHistory()->GetNumCollidedEntities()>0)
	{
		fwMvClipSetId outClipSetId;
		fwMvClipId outClipId;
		Matrix34 outTransform;
		if (pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().FindBestMatch(*pPed->GetSkeleton(), outClipSetId, outClipId, outTransform, &blendOutSetList.GetFilter()))
		{
			if (blendOutSetList.GetFilter().GetBestDistance() < ms_Tunables.m_RagdollAbortPoseDistanceThreshold)
			{
				bUseCurrentPose = true;
			}
		}
	}

	if (bUseCurrentPose)
	{
		nmTaskDebugf(this, "RagdollAborted - no getup task required. Using ragdoll frame instead (bestDistance=%.3f, threshold=%.3f, poseSet=%s, velMag=%.3f, collidedEnts=%d)",blendOutSetList.GetFilter().GetBestDistance() ,ms_Tunables.m_RagdollAbortPoseDistanceThreshold, deadBlendOutSet.TryGetCStr(), m_fRagdollAbortVelMag, pPed->GetFrameCollisionHistory()->GetNumCollidedEntities() );
	}
	else
	{
		nmTaskDebugf(this, "RagdollAborted - starting getup task (bestDistance=%.3f, threshold=%.3f, poseSet=%s, velMag=%.3f, collidedEnts=%d)",blendOutSetList.GetFilter().GetBestDistance() ,ms_Tunables.m_RagdollAbortPoseDistanceThreshold, deadBlendOutSet.TryGetCStr(), m_fRagdollAbortVelMag, pPed->GetFrameCollisionHistory()->GetNumCollidedEntities() );
		//start a getup task using the dead getup set
		CTaskGetUp* pGetupTask = rage_new CTaskGetUp( deadBlendOutSet );
		SetNewTask(pGetupTask);
	}
}

//////////////////////////////////////////////////////////////////////////


CTask::FSM_Return CTaskDyingDead::RagdollAborted_OnUpdate( CPed* pPed )
{
	// If damage has been recorded - switch to ragdoll if possible
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded) )
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			// Switch to a high ragdoll LOD if being fired upon
			if (m_damageInfo.GetCount())
			{	
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_damageInfo[0].GetWeaponHash());
				if (pWeaponInfo && (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
				{
					pPed->GetRagdollInst()->SetSuppressHighRagdollLODOnActivation(false);
				}
			}

			SetState(State_DeadRagdoll);
			return FSM_Continue;
		}
	}

	if (pPed->GetUsingRagdoll())
	{
		SetState(State_DeadRagdoll);
		return FSM_Continue;
	}

	// if the ped is done moving, disable the capsule so things don't push it around
	if (pPed->GetIsStanding() &&  pPed->GetWasStanding() && pPed->GetVelocity().Mag2()<0.001f)
	{
		pPed->StopAllMotion(true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule, true);
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Switch to the dead animated pose (to wait for an opportunity to settle using the ragdoll)
		SetState(State_DeadAnimated);
	}
	else if (!GetSubTask())
	{
		// If we're starting dead then we need to ragdoll the ped to settle them to the ground properly - otherwise they can be intersecting geometry
		// We want to do this even if the ped is on-screen (in fact - we especially want to do this if the ped is on-screen)
		// This should allow the ped to ragdoll if there's a ragdoll available or if they are on-screen and there is another ragdoll with lower priority (i.e. one that is off-screen)
		if (CTaskNMBehaviour::RagdollPoolHasSpaceForPed(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed, pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen), 0.5f /*SCORE_AMB_DEAD*/) && 
			CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			// Sync the last and current matrices, then switch to ragdoll
			PHSIM->SetLastInstanceMatrix(pPed->GetRagdollInst(), pPed->GetRagdollInst()->GetMatrix());
			phBoundComposite* pBound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
			if (pBound)
			{
				pBound->UpdateLastMatricesFromCurrent();
			}

			pPed->SwitchToRagdoll(*this);

			if (pPed->GetRagdollInst()->GetArticulatedBody())
			{
				pPed->GetRagdollInst()->GetArticulatedBody()->Freeze();
			}

			SetState(State_DeadRagdoll);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////


void CTaskDyingDead::RagdollAborted_OnExit( CPed* UNUSED_PARAM(pPed))
{

}


//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::OnDeadEnter( CPed & )
{

}

//////////////////////////////////////////////////////////////////////////
// State_DeadRagdoll

void CTaskDyingDead::DeadRagdoll_OnEnter( CPed* pPed )
{
	//! Can reset this now - it's safe to ragdoll frame if we ragdoll.
	m_bDontUseRagdollFrameDueToInVehicleDeath = false;

	AddToStateHistory(State_DeadRagdoll);

	InitDeadState();

	// If coming from the dead animated or dead ragdoll states, make sure we don't allow any more timeslicing.
	// Dead peds can be timesliced for long periods, in some cases long enough to cause the ragdoll to abort.
	if (GetPreviousState()==State_DeadAnimated || GetPreviousState()==State_DeadRagdollFrame)
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	m_moveNetworkHelper.ReleaseNetworkPlayer();

	if( !pPed->GetUsingRagdoll() && CTaskNMBehaviour::CanUseRagdoll( pPed, RAGDOLL_TRIGGER_DIE ) )
		pPed->SwitchToRagdoll(*this);

	if (pPed->GetUsingRagdoll())
	{
		static float stiffness = 0.0f;
		static float damping = 0.1f;
		pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the next line to route to TaskRageRagdoll
		if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
			CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);

		CTaskNMControl* pNMControlTask = rage_new CTaskNMControl(1000, 10000, rage_new CTaskNMRelax(1000, 10000, stiffness, damping), CTaskNMControl::ALL_FLAGS_CLEAR);
		SetNewTask(pNMControlTask);

		if (pPed->IsNetworkClone())
		{
			pNMControlTask->SetRunningLocally(true);
		}

		// Re-init the sleep params (since the phSleep gets removed when switching to ragdoll frame)
		InitSleep(pPed);

		// Increase gravity to push a ped that started dead or a re-activated shallow water ped to conform to the ground
		if ((m_flags.IsFlagSet(Flag_startDead) || m_flags.IsFlagSet(Flag_HasReactivatedShallowWaterPed)) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RagdollFloatsIndefinitely))
		{
			phCollider* pCollider = pPed->GetCollider();
			if (pCollider && pCollider->GetSleep())
			{
				pCollider->SetGravityFactor(1.1f);
				pCollider->GetSleep()->SetMaxMotionlessTicks(30);
			}
		}
	}

	// Add navmesh blocking bound to prevent wandering peds from walking over dead bodies
	AddBlockingObject(pPed);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::DeadRagdoll_OnUpdate( CPed* pPed )
{
	// Sleep should be allowed by this point
	taskAssert(!pPed->GetUsingRagdoll() || m_flags.IsFlagSet(Flag_SleepingIsAllowed));

	taskAssert(pPed);
	// If ped exiting a vehicle, force them ped away from the vehicle they were in
	ProcessPushFromVehicle();

	UpdateBlockingObject(pPed);

	// Mark that this task is still in control of the peds ragdoll
	if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
	{
		pPed->TickRagdollStateFromTask(*this);
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RagdollFloatsIndefinitely))
	{
		const phCollider* pCollider = pPed->GetCollider();
		if (pCollider && pCollider->GetSleep())
		{
			// pPed->GetIsInWater() requires the owning player to be near, otherwise it defaults to a water height of 0.0,
			// which is fine for when the ped is in the ocean but not when they're in one of the elevated lakes.
			float fWaterZ;
			if (pPed->GetIsInWater() || Water::GetWaterLevel(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), &fWaterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
			{
				pCollider->GetSleep()->BlockSleepFinishing(CPhysics::GetNumTimeSlices());
			}
		}
	}

	// Ensure that we don't timeslice the ai update whilst the ragdoll is actively
	// simulating. Active ragdolls need to be carefully managed by controlling ai.
	if (pPed->GetUsingRagdoll())
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}

	if (m_flags.IsFlagSet(Flag_RagdollHasSettled))
	{
		// The ragdoll has settled appropriately
		// switch to the ragdoll frame state
		SetState(State_DeadRagdollFrame);
		return FSM_Continue;
	}

	if (!pPed->GetUsingRagdoll())
	{
		// If interrupting special handling for shallow water peds, give them another chace to sink to conform to the bottom if they come back on screen
		m_flags.ClearFlag(Flag_HasReactivatedShallowWaterPed);

		// ped is not ragdolling, either it's been fixed, the ragdoll timed out, or it was forced to abort by a more important ragdoll activation.
		// fall back to an appropriate animated response
		SetState(State_RagdollAborted);
		return FSM_Continue;
	}

	taskAssertf(pPed->GetRagdollState()>=RAGDOLL_STATE_PHYS_ACTIVATE, "Ragdoll state has become animated - %u", pPed->GetRagdollState());

	// A damage impact has been recorded - react to it here
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded) )
	{
		if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
		{
			for(s32 iDamage = 0; iDamage < m_damageInfo.GetCount(); iDamage++)
			{
				float fImpulseMag = m_damageInfo[iDamage].GetImpulseMag();
				Vector3 vImpulseDir = m_damageInfo[iDamage].GetImpulseDir();
				u32 iWeaponHash = m_damageInfo[iDamage].GetWeaponHash();

				// apply force to the ragdoll if its active
				if( m_damageInfo[iDamage].GetIntersectionInfo() && 
					fImpulseMag > 0.0f && vImpulseDir.IsNonZero() )
				{
					if (Verifyf(FPIsFinite(fImpulseMag), "Non-finite impulse in CTaskDyingDead::DeadRagdoll_OnUpdate"))
					{
						if (Verifyf(vImpulseDir.FiniteElements(), "Non-finite impulse direction in CTaskDyingDead::DeadRagdoll_OnUpdate"))
						{
							taskAssert(pPed->GetRagdollInst());
							int numbounds = pPed->GetRagdollInst()->GetCacheEntry()->GetBound()->GetNumActiveBounds();
							if (m_damageInfo[iDamage].GetIntersectionComponent() < numbounds)
							{
								pPed->GetRagdollInst()->ApplyBulletForce(pPed, fImpulseMag, vImpulseDir, m_damageInfo[iDamage].GetWorldIntersectionPos(pPed), m_damageInfo[iDamage].GetIntersectionComponent(), iWeaponHash);
							}
						}
					}
				}
			}
		}
		m_damageInfo.Reset();
		m_flags.ClearFlag(Flag_damageImpactRecorded);
	}
	Assert(m_damageInfo.GetCount() == 0);

	// Make sure that muscle driving is turned off after a five seconds or so have passed as a failsafe
	static u32 switchOffDrivingTime = 5000;
	if (pPed->GetUsingRagdoll() && fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfDeath() >= switchOffDrivingTime && 
		RAGE_VERIFY(ai_task, pPed->GetCollider()))
	{
		taskAssert(((phArticulatedCollider*)pPed->GetCollider())->GetBody());
		((phArticulatedCollider*)pPed->GetCollider())->GetBody()->SetDriveState(phJoint::DRIVE_STATE_FREE);
	}

	if(ShouldFallOutOfVehicle())
	{
		SetState(State_FallOutOfVehicle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::MakeAnonymousDamageByEntity(const CEntity* pEntity)
{
	if (pEntity && (m_deathSourceInfo.GetEntity() == pEntity))
	{
		m_deathSourceInfo.ClearEntity();
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::InitSleep(CPed *pPed)
{
	if (pPed && pPed->GetUsingRagdoll() && pPed->GetCollider())
	{
		nmTaskDebugf(this, "CTaskDyingDead::InitSleep - initializing sleep object");

		pPed->GetCollider()->GetSleep()->Init(pPed->GetCollider());

		static float sm_DefaultVelTolerance2 = 0.2f; //0.005f;
		static float sm_DefaultAngVelTolerance2 = 0.2f; //0.01f;
		static float sm_DefaultInternalVelTolerance2 = 0.5f; //0.2f;

#if RSG_ORBIS || RSG_DURANGO
		// Since the sleep system uses a frame counter we need to account for the more solid 30 fps that can be achieved on next-gen consoles
		static int sm_DefaultMotionlessTicks = 20; //15;
		static int sm_DefaultAsleepTicks = 25; //20;
#elif RSG_PC
		// PC runs at 60 fps so need to double the already increased NG values from above
		static int sm_DefaultMotionlessTicks = 40; //15;
		static int sm_DefaultAsleepTicks = 50; //20;
#else
		static int sm_DefaultMotionlessTicks = 10; //15;
		static int sm_DefaultAsleepTicks = 10; //20;
#endif

		pPed->GetCollider()->GetSleep()->SetVelTolerance2(sm_DefaultVelTolerance2);
		pPed->GetCollider()->GetSleep()->SetAngVelTolerance2(sm_DefaultAngVelTolerance2);
		pPed->GetCollider()->GetSleep()->SetInternalMotionTolerance2Sum(sm_DefaultInternalVelTolerance2);
		pPed->GetCollider()->GetSleep()->SetMaxMotionlessTicks(sm_DefaultMotionlessTicks);
		pPed->GetCollider()->GetSleep()->SetMaxAsleepTicks(sm_DefaultAsleepTicks);

		m_flags.SetFlag(Flag_SleepingIsAllowed);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DisableSleep(CPed *pPed)
{
	if (pPed && pPed->GetUsingRagdoll() && pPed->GetCollider())
	{
		nmTaskDebugf(this, "CTaskDyingDead::DisableSleep - disabling sleep object");

		pPed->GetCollider()->GetSleep()->Init(pPed->GetCollider());
		pPed->GetCollider()->GetSleep()->SetActiveSleeper(true);
		pPed->GetCollider()->GetSleep()->SetVelTolerance2(0.0f);
		pPed->GetCollider()->GetSleep()->SetAngVelTolerance2(0.0f);
		pPed->GetCollider()->GetSleep()->SetInternalMotionTolerance2Sum(0.0f);
	}

	m_flags.ClearFlag(Flag_SleepingIsAllowed);
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DeadRagdoll_OnExit(CPed* pPed)
{
	// Restore gravity in case it was modified in the on-enter function
	if (pPed->GetCollider())
	{
		pPed->GetCollider()->SetGravityFactor(1.0f);
	}

	OnDeadExit(*pPed);
}


//////////////////////////////////////////////////////////////////////////
// State DeadRagdollFrame

dev_float SIMPLE_DEAD_DEEP_WATER_THRESHOLD		= 1.3f;		// Peds must be in deep water to deactivate their ragdoll, unless they are in VERY shallow water (defined by PED_MIN_WATER_DEPTH)
dev_float SIMPLE_DEAD_MIN_WATER_DEPTH			= 0.15f;		// Peds in water shallower than this won't be treated differently to dead peds on ground. This allows peds lying dead in very shallow water to go to sleep.

void CTaskDyingDead::DeadRagdollFrame_OnEnter( CPed* pPed )
{
	AddToStateHistory(State_DeadRagdollFrame);

	// B*1944436: Don't allow peds that are attached to switch to ragdoll frame.
	// The switch to ragdoll frame fiddles with the entity matrix in the case where we're in ragdoll, which shouldn't be done if the entity is attached to something.
	// The ped should have always been detached prior to getting into this state during regular gameplay if the attachment wasn't forced on by a script or something.
	if(pPed->GetIsAttached() && pPed->GetUsingRagdoll())
	{
		m_flags.ClearFlag(Flag_RagdollHasSettled);
		return;
	}	

	// Get the money down early.
	OnDeadExit( *pPed );

	m_moveNetworkHelper.ReleaseNetworkPlayer();

	if (pPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE || m_flags.IsFlagSet(Flag_GoStraightToRagdollFrame)) // this can happen in a network game when a dead ped migrates that has already run the dead task
	{
		SwitchToRagdollFrame(pPed);
	}

	//Requires the ragdoll to be attach in SwitchToRagdollFrame before the incident is checked
	InitDeadState();

	TriggerBloodPool( pPed );

	AddBlockingObject(pPed);

	if (m_flags.IsFlagSet(Flag_RagdollHasSettled) && pPed->GetCapsuleInfo()->IsQuadruped())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableQuadrupedSpring, true);
	}

	// We're now in the ragdoll frame state, as requested
	m_flags.ClearFlag(Flag_RagdollHasSettled);
}

void CTaskDyingDead::AddBlockingObject( CPed* pPed )
{
	// Add navmesh blocking bound to prevent wandering peds from walking over dead bodies
	if(m_iDynamicObjectIndex == DYNAMIC_OBJECT_INDEX_NONE || m_iDynamicObjectIndex == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
	{
		const u32 iFlags = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlocksPathingWhenDead) ? TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES : TDynamicObject::BLOCKINGOBJECT_WANDERPATH;

		Vector3 vPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		Vector3 vSize = (pPed->GetBoundingBoxMax() - pPed->GetBoundingBoxMin())/2;
		float heading = pPed->GetTransform().GetHeading();
		m_iDynamicObjectIndex = CPathServerGta::AddBlockingObject(vPos, vSize, heading, iFlags);
		m_iDynamicObjectLastUpdateTime = fwTimer::GetTimeInMilliseconds();
	}
}

void CTaskDyingDead::UpdateBlockingObject( CPed* pPed )
{
	static dev_s32 iUpdateInterval = 4000;
	if(m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_NONE && m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD &&
		fwTimer::GetTimeInMilliseconds()-m_iDynamicObjectLastUpdateTime >= iUpdateInterval)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vSize = (pPed->GetBoundingBoxMax() - pPed->GetBoundingBoxMin())/2;
		float heading = pPed->GetTransform().GetHeading();
		CPathServerGta::UpdateBlockingObject(m_iDynamicObjectIndex, vPos, vSize, heading, false);
		m_iDynamicObjectLastUpdateTime = fwTimer::GetTimeInMilliseconds();
	}
}

bool CTaskDyingDead::ShouldFallOutOfVehicle() const
{
	//Ensure the ped is ambient.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the vehicle is an automobile.
	if(!pVehicle->InheritsFromAutomobile())
	{
		return false;
	}

	//Ensure the driver is valid.
	const CPed* pDriver = pVehicle->GetDriver();
	if(!pDriver)
	{
		return false;
	}

	//Ensure the driver is not the ped.
	if(pDriver == GetPed())
	{
		return false;
	}

	//Ensure the driver is running 'motion in automobile'.
	const CTaskMotionInAutomobile* pTask = static_cast<CTaskMotionInAutomobile *>(
		pDriver->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
	if(!pTask)
	{
		return false;
	}

	//Grab the acceleration.
	float fAcceleration = pTask->GetAcceleration().GetXf();

	//Check if we are on the left side of the vehicle.
	Vec3V vLocalPosition = pVehicle->GetTransform().UnTransform(GetPed()->GetTransform().GetPosition());
	if(IsLessThanAll(vLocalPosition.GetX(), ScalarV(V_ZERO)))
	{
		//Ensure the acceleration exceeds the threshold.
		static float s_fMinAcceleration = 0.7f;
		if(fAcceleration < s_fMinAcceleration)
		{
			return false;
		}
	}
	else
	{
		//Ensure the acceleration does not exceed the threshold.
		static float s_fMaxAcceleration = 0.3f;
		if(fAcceleration > s_fMaxAcceleration)
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskDyingDead::ShouldUseReducedRagdollLOD(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	if (!pPed->IsPlayer() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SuppressLowLODRagdollSwitchWhenCorpseSettles) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		// don't use reduced ragdoll lod on peds close to the player
		CPed *pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			if (IsLessThanAll(DistSquared(pPlayer->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()), square(ScalarV(V_FIFTEEN))))
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::DeadRagdollFrame_OnUpdate( CPed* pPed )
{

	bool bShouldUseReducedLod = ShouldUseReducedRagdollLOD(pPed);
	TUNE_GROUP_INT(RAGDOLL_LOD_WHEN_CORPSE_SETTLES, iRagdollLODWhenCorpseSettles, 1, 0, 2, 1);

	// Can only set the ragdoll lod if the ragdoll is inactive
	if (PHLEVEL->IsInactive(pPed->GetRagdollInst()->GetLevelIndex()))
	{
		// Set the corpse to use the appropriate ragdoll lod if it's not already in it
		if (bShouldUseReducedLod && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInstNM::RAGDOLL_LOD_HIGH)
		{		
			pPed->GetRagdollInst()->SetCurrentPhysicsLOD(static_cast<fragInst::ePhysicsLOD>(iRagdollLODWhenCorpseSettles));
		}
		else if (!bShouldUseReducedLod && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() != fragInstNM::RAGDOLL_LOD_HIGH)
		{
			pPed->GetRagdollInst()->SetCurrentPhysicsLOD(fragInstNM::RAGDOLL_LOD_HIGH);
		}
	}

	// Keep setting this flag as it gets reset each time the ragdoll activates
	pPed->GetRagdollInst()->SetSuppressHighRagdollLODOnActivation(bShouldUseReducedLod);

	UpdateBlockingObject(pPed);

	if (CPhysics::GetLevel()->LegitLevelIndex(m_PhysicalGroundHandle.GetLevelIndex()))
	{
		// If the ground physical has been deleted out from underneath us then switch to ragdoll
		if (CPhysics::GetLevel()->GetInstance(m_PhysicalGroundHandle) == NULL)
		{
			m_flags.SetFlag(Flag_damageImpactRecorded);
		}
	}

	// If damage has been recorded - switch to ragdoll if possible
	if( m_flags.IsFlagSet(Flag_damageImpactRecorded) )
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			// Switch to a high ragdoll LOD if being fired upon
			if (m_damageInfo.GetCount())
			{	
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_damageInfo[0].GetWeaponHash());
				if (pWeaponInfo && (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
				{
					pPed->GetRagdollInst()->SetSuppressHighRagdollLODOnActivation(false);
				}
			}

			SetState(State_DeadRagdoll);
			return FSM_Continue;
		}
	}
	else if (pPed->GetUsingRagdoll())
	{
		// ped has been put into ragdoll (probably by a nearby explostion, etc
		// change to the ragdoll state to deal with it properly
		SetState(State_DeadRagdoll);
		return FSM_Continue;
	}

	// Constrain non-attached animated corpses in water so that they can only translate straight down without rotation
	if (!m_flags.IsFlagSet(Flag_UnderwaterCorpseConstraintsApplied) && pPed->GetIsInWater())
	{
		const fwAttachmentEntityExtension *pedAttachExt = pPed->GetAttachmentExtension();
		if (pPed->GetCollider() && !(pedAttachExt && pedAttachExt->GetAttachParent()))
		{
			phConstraintPrismatic::Params constraintPrism;
			constraintPrism.instanceA = pPed->GetAnimatedInst();
			constraintPrism.slideAxisLocalB = Vec3V(V_Z_AXIS_WZERO);
			constraintPrism.minDisplacement = -100.0f;
			constraintPrism.maxDisplacement = 0.0f;
			PHCONSTRAINT->Insert(constraintPrism);

			m_flags.SetFlag(Flag_UnderwaterCorpseConstraintsApplied);
		}
	}

	// Stop nudges from getting applied to sleeping corpses in water
	if (pPed->GetIsInWater())
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableAsleepImpulse, true );
	}

	// Activate the ragdoll if the capsule is resting on the ground and the ped is on screen and possibly exposed to the surface, otherwise it'll float in the air
	if( pPed->GetRagdollState()==RAGDOLL_STATE_ANIM && m_flags.IsFlagSet(Flag_ragdollFramePosedInWater) && pPed->GetIsVisibleInSomeViewportThisFrame() && !pPed->m_nDEflags.bFrozen)
	{
		if((pPed->IsOnGround() || pPed->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypesThisFrame(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING))&& CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))// && pPed->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER)
		{
			SetState(State_DeadRagdoll);
			return FSM_Continue;
		}
	}

	if (m_flags.IsFlagSet(Flag_disableCapsuleInRagdollFrameState))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule, true);
	}
	// For non-biped peds we don't orient the main mover bound to their spine but instead orient the entire instance so we need to disable ped constraints
	// otherwise the ped will get snapped when roll/pitch is constrained
	else if (pPed->GetCapsuleInfo() != NULL && !pPed->GetCapsuleInfo()->IsBiped())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);
	}

	if(ShouldFallOutOfVehicle())
	{
		SetState(State_FallOutOfVehicle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DeadRagdollFrame_OnExit( CPed* pPed )
{
	if (pPed->GetCapsuleInfo()->IsQuadruped())
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableQuadrupedSpring, false);

	// Ensure the physical ground handle is always reset when leaving this state
	// This also happens inside of SwitchToRagdollFrame so it shouldn't really be possible to get back into
	// this state without this having been reset but just in case there's a way to get to DeadRagdollFrame
	// without calling SwitchToRagdollFrame
	m_PhysicalGroundHandle.SetLevelIndex(phInst::INVALID_INDEX);
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::FallOutOfVehicle_OnEnter(CPed* pPed)
{
	Assert(pPed);
	Assert(pPed->GetMyVehicle());

	AddToStateHistory(State_FallOutOfVehicle);

	pPed->GetMovePed().SwitchToAnimated(NORMAL_BLEND_DURATION);

	int seatIdx = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
	Assert((seatIdx >= 0) && (seatIdx < pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats()));

	CVehicle* pedVehicle = pPed->GetMyVehicle();
	Assert(pedVehicle);

	if(pedVehicle)
	{
		// if the vehicle is on fire, we need to propogate that fire to the ped in the same state
		if(pedVehicle->IsOnFire())
		{
			CFire* fire = g_fireMan.GetEntityFire(pedVehicle);
			if(fire)
			{
#if __BANK
				g_fireMan.StartPedFire(pPed, NULL, fire->GetNumGenerations(), Vec3V(V_ZERO), Vec3V(V_ZERO), false, false, 0, fire->GetCurrBurnTime(), true);
#else
				g_fireMan.StartPedFire(pPed, NULL, fire->GetNumGenerations(), Vec3V(V_ZERO), false, false, 0, fire->GetCurrBurnTime(), true);
#endif
			}
		}

		// Set everything down here for task
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DeadFallOut);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);

		CEntity* pSourceEntity = m_deathSourceInfo.GetEntity();

        CPed *pPedJacker = 0;

        if (pSourceEntity && pSourceEntity->GetIsTypePed())
        {
            pPedJacker = static_cast<CPed *>(pSourceEntity);
        }

		//! Note: On clones, m_iExitPointForVehicleFallOut will be -1, so just generate one here. It doesn't necessarily have to match 
		//! the owner as it doesn't really matter when you are dead.
		if(pPed->IsNetworkClone())
		{
			m_iExitPointForVehicleFallOut = 
				CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, pPed->GetMyVehicle(), SR_Specific, seatIdx, false, vehicleFlags);
		}

		CTaskExitVehicle* pExitVehicleTask = rage_new CTaskExitVehicle( pPed->GetMyVehicle(), vehicleFlags, 0.0f, pPedJacker, m_iExitPointForVehicleFallOut );

		SetNewTask(pExitVehicleTask);

		if (pPed->IsNetworkClone())
		{
			pExitVehicleTask->SetRunningLocally(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::FallOutOfVehicle_OnUpdate(CPed* pPed )
{
	if(!GetIsSubtaskFinished(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		// if a transition to ragdoll happens whilst the
		// task is running, change to the dead ragdoll state
		// to handle it.
		if (pPed->GetUsingRagdoll())
		{
			SetState(State_DeadRagdoll);
		}

		return FSM_Continue;
	}

	if (pPed->GetIsInVehicle())
	{
		if (pPed->IsNetworkClone())
		{
			// force the clone out of the vehicle if the exit vehicle task has failed to do so (this can happen when it is out of scope)
			((CNetObjPed*)pPed->GetNetworkObject())->SetClonePedOutOfVehicle(false, CPed::PVF_Warp);

			// clear anim data so that the ped does a standing death
			m_flags.ClearFlag(Flag_clipsOverridenByDictHash);
			m_flags.ClearFlag(Flag_clipsOverridenByClipSetHash);

			// restart the task
			SetState(State_Start);
			return FSM_Continue;	
		}
		else
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled, false);
			SetState(State_StreamAssets);
		}
		return FSM_Continue;
	}

	// Set this so clones and owners are matched up....
	if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT)) 
	{
		pPed->SwitchToRagdoll(*this);
		SetState(State_DeadRagdoll);
	}
	else
	{
		CVehicle* pPedsVehicle = pPed->GetMyVehicle();

		// if we're in the air, transition to the dying animated fall
		if (pPedsVehicle && pPedsVehicle->IsInAir() && m_bCanAnimatedDeadFall )
		{
			// Disable the ped constraint for a frame. The getup task should take care of
			// snapping us to an upright capsule whilst blending the pose correctly.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);
			SetState(State_DyingAnimatedFall);
		}
		else
		{
			SetState(State_RagdollAborted);
		}

		if (pPedsVehicle)
		{
			if (pPedsVehicle->GetCollider())
			{
				Vector3 localVel = VEC3V_TO_VECTOR3(pPedsVehicle->GetCollider()->GetLocalVelocity(pPed->GetMatrix().d().GetIntrin128()));
				pPed->SetVelocity(localVel);
			}
			else
			{
				pPed->SetVelocity(pPedsVehicle->GetVelocity());
			}
		}
	}
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskDyingDead::FallOutOfVehicle_OnExit(CPed* UNUSED_PARAM(pPed) )
{

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::DealWithDeactivatedPhysics( CPed* pPed, bool ragdollIsAsleep )
{
	if (ragdollIsAsleep /*|| GetState() == State_DeadRagdoll*/)
	{
		// The ragdoll has settled, it's safe to switch to the ragdoll frame
		nmTaskDebugf(this, "DealWithDeactivatedPhysics: Ragdoll has settled");
		SwitchToRagdollFrame(pPed);
		if(GetState() != State_DeadRagdollFrame)
		{
			m_flags.SetFlag(Flag_RagdollHasSettled);
		}
	}
	else if (pPed->GetUsingRagdoll())
	{
		// The ragdoll hasn't settled, but we need to abort the ragdoll anyway (e.g. due to
		// the map streaming out). Pose the ragdoll frame and switch to it. The AbortRagdoll
		// state should deal with the fact that we've deactivated.
		nmTaskDebugf(this, "DealWithDeactivatedPhysics: Ragdoll needs to be aborted");
		SwitchToRagdollFrame(pPed);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::IsOffscreenAndCanSnapToGround( CPed* pPed )
{
	bool bCanSnap = false;

	if (pPed->GetIsInVehicle())
		return false;

#if GTA_REPLAY
	if(CReplayMgr::ShouldIncreaseFidelity())
	{
		CEntity* pPlayer = CGameWorld::FindLocalPlayer();
		if(DistSquared(pPlayer->GetTransform().GetPosition(),pPed->GetTransform().GetPosition()).Getf() < rage::square(30.0f))
		{
			return false;
		}
	}
#endif

	// we can't do this in MP as it can happen in view of other players
	if (!NetworkInterface::IsGameInProgress() && !pPed->GetIsVisibleInSomeViewportThisFrame() && !pPed->IsLocalPlayer())
	{
		bCanSnap = true;

		// This is a cheap approximation to detect if a ped is falling above the camera viewport
		float velMag = pPed->GetVelocity().Mag();
		static float sfMinVel = 1.0f;
		if (velMag > sfMinVel)
		{
			Vector3 vCamFacing = camInterface::GetFront(), vTemp;
			Vector3 vCamToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - camInterface::GetPos();
			vCamToPed.NormalizeSafe();

			// If the ped if with the side extends of the camera
			static float fSideDotLim = 0.7f;
			float fDot = vCamToPed.Dot(camInterface::GetUp());
			Vector3 vCamToPedFlattened = vCamToPed - (fDot*camInterface::GetUp());
			vCamToPedFlattened.NormalizeSafe();
			fDot = vCamToPedFlattened.Dot(vCamFacing);
			if (fDot > fSideDotLim)
			{
				// If the ped if above the camera FOV
				vTemp.Cross(vCamFacing, vCamToPed);
				if (camInterface::GetRight().Dot(vTemp) > 0.0f)
				{
					bCanSnap = false;
				}
			}
		}
	}

	return bCanSnap;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::SwitchToRagdoll( CPed* pPed )
{
	Assert(pPed);
	pPed->SwitchToRagdoll(*this);
	// nothing more to do here. The state will change the next
	// time the task runs.
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::IsRagdollSettledOnMapCollision(CPed* pPed)
{
	// cheap test - look for collision with the map
	if (pPed->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypesThisFrame(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING))
	{
		return true;
	}

	// Sometimes the test above will fail when in water because bouyancy is keeping the ped up.
	// do a sphere test around the spine
	if (pPed->GetRagdollInst())
	{
		const fragPhysicsLOD* physicsLOD = pPed->GetRagdollInst()->GetTypePhysics();
		if (physicsLOD)
		{
			fragTypeChild** child = physicsLOD->GetAllChildren();
			if (child && RAGDOLL_SPINE0<physicsLOD->GetNumChildren() && child[RAGDOLL_SPINE0] && (child[RAGDOLL_SPINE0])->GetUndamagedEntity())
			{			
				Vector3 rootPos = VEC3V_TO_VECTOR3(Transform(pPed->GetRagdollInst()->GetMatrix(), VECTOR3_TO_VEC3V((child[RAGDOLL_SPINE0])->GetUndamagedEntity()->GetBoundMatrix().d)));

				//Now perform a sphere test at the bound position.
				WorldProbe::CShapeTestSphereDesc sphereTest;
				sphereTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				sphereTest.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
				sphereTest.SetSphere(rootPos, ms_Tunables.m_SphereTestRadiusForDeadWaterSettle);
				return WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskDyingDead::SwitchToRagdollFrame( CPed* pPed )
{
	bool bIsInRagdoll = pPed->GetRagdollState()>=RAGDOLL_STATE_PHYS_ACTIVATE;

	if (pPed->GetAnimDirector())
	{
		pPed->GetAnimDirector()->WaitForPrePhysicsUpdateToComplete();
	}

	// Don't zero last matrices on the next activation - can create awkward collisions when near/under cars
	pPed->GetRagdollInst()->SetDontZeroMatricesOnActivation();

	// Corpses should not consume an NM agent.  Just be a rage ragdoll instead.
	pPed->GetRagdollInst()->SetBlockNMActivation(true);

	Matrix34 rootMatrix;
	pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);

	bool bIsBiped = pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsBiped();

	Vector3 vecSwitchPos = rootMatrix.d;

	bool bAddPhysics = false;
	m_flags.ClearFlag(Flag_ragdollFramePosedInWater);
	CPhysical* pAttachPhysical = NULL;		// If this gets set to something then ped will attach after ragdoll is turned off
	m_PhysicalGroundHandle.SetLevelIndex(phInst::INVALID_INDEX);

	// don't leave capsule physics active when in water and touching the ground 
	if(pPed->GetIsInWater() && !IsRagdollSettledOnMapCollision(pPed))
	{
		bAddPhysics = true;
		m_flags.SetFlag(Flag_ragdollFramePosedInWater);
	}

	m_flags.ClearFlag(Flag_UnderwaterCorpseConstraintsApplied);

	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();
	// Still scan for boat collision if the peds physics haven't been activated to allow it to float in water.
	WorldProbe::CShapeTestHitPoint capsuleIsect;
	WorldProbe::CShapeTestResults capsuleResult(capsuleIsect);
	if( !bAddPhysics )
	{
		int nIncludeFlags = ArchetypeFlags::GTA_PED_INCLUDE_TYPES;
		nIncludeFlags &= ~(ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_RAGDOLL_TYPE );
		int nTypeFlags = ArchetypeFlags::GTA_PED_TYPE;

		float fPedProbeBottom = -pCapsuleInfo->GetGroundToRootOffset(); 
		if(pCapsuleInfo->GetBipedCapsuleInfo())
		{
			fPedProbeBottom -= pCapsuleInfo->GetBipedCapsuleInfo()->GetGroundOffsetExtend();
		}
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure(&capsuleResult);
		capsuleDesc.SetCapsule(vecSwitchPos, vecSwitchPos+Vector3(0.0f,0.0f,fPedProbeBottom), pCapsuleInfo->GetProbeRadius());
		capsuleDesc.SetIncludeFlags(nIncludeFlags);
		capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		capsuleDesc.SetTypeFlags(nTypeFlags);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(false);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			vecSwitchPos.z = capsuleResult[0].GetHitPosition().z + pCapsuleInfo->GetGroundToRootOffset();

			//Turn physics on if we are standing on something which could move
			//If its a boat then we will attach to it as well
			CEntity* pEntityStandingOn = CPhysics::GetEntityFromInst(capsuleResult[0].GetHitInst());
			if(pEntityStandingOn != NULL && pEntityStandingOn->GetIsPhysical())
			{
				bAddPhysics = true;
				pAttachPhysical = static_cast<CPhysical*>(pEntityStandingOn);
			}
		}
	}

	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning))
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
	
	// Recreate the animated capsule
	bool bFixMatrix = ((bAddPhysics && bIsBiped) || ms_bForceFixMatrixOnDeadRagdollFrame) ? true : false;
	if (bIsInRagdoll)
	{
		m_fRagdollAbortVelMag = pPed->GetVelocity().Mag();

		// for non bipeds if we need to reactivate physics, try to position the capsule over the spine bounds
		if (bAddPhysics && !bIsBiped)
		{	
			s32 boneIdx = -1;
			pPed->GetSkeleton()->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_ROOT, boneIdx);
			if (boneIdx>-1)
			{
				Vector3 zWorld = rootMatrix.d;
				pPed->GetRagdollInst()->SetPosition(RC_VEC3V(zWorld));
				pPed->SetPosition(zWorld, false, false);
				
				pPed->GetObjectMtxNonConst(boneIdx).d.Zero();
				pPed->GetLocalMtxNonConst(boneIdx).d.Zero();
			}
		}

		pPed->SwitchToAnimated(true, true, bFixMatrix, true, false, true, true);

		// Ensure the root slope fixup is disabled - this would occasionally get left on from previous animations, leading to orientation issues 
		CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pSolver != NULL)
		{
			pSolver->SetBlendRate(INSTANT_BLEND_OUT_DELTA);
		}
	}
	else
	{
		pPed->SwitchToStaticFrame(0.0f);
	}

	if (pPed->GetFacialData())
		pPed->GetFacialData()->SetFacialIdleMode(pPed, CFacialDataComponent::kModeRagdoll);

	if (m_flags.IsFlagSet(Flag_ragdollFramePosedInWater))
	{
		pPed->SetIsStanding(false);
		pPed->SetWasStanding(false);
	}
	else
	{
		pPed->SetIsStanding(true);
		pPed->SetWasStanding(true);
	}

	pPed->SetGroundOffsetForPhysics(-pCapsuleInfo->GetGroundToRootOffset());

	if (pAttachPhysical)
	{
		// Check for specific vehicle types
		if(pAttachPhysical->GetIsTypeVehicle() && (static_cast<CVehicle*>(pAttachPhysical)->GetVehicleType() == VEHICLE_TYPE_BOAT || 
			static_cast<CVehicle*>(pAttachPhysical)->GetVehicleType() == VEHICLE_TYPE_PLANE || 
			static_cast<CVehicle*>(pAttachPhysical)->GetVehicleType() == VEHICLE_TYPE_TRAIN))
		{
			// Heading and position need to be relative to the attach entity
			Vector3 vOffset = VEC3V_TO_VECTOR3(pAttachPhysical->GetTransform().UnTransform(pPed->GetTransform().GetPosition()));
			Quaternion qOffset = QUATV_TO_QUATERNION(UnTransform(pAttachPhysical->GetTransform().GetOrientation(), pPed->GetTransform().GetOrientation()));
			pPed->AttachPedToPhysical(pAttachPhysical, -1, ATTACH_STATE_PED_WITH_ROT | ATTACH_FLAG_AUTODETACH_ON_RAGDOLL | ATTACH_FLAG_INITIAL_WARP | ATTACH_FLAG_AUTODETACH_ON_TELEPORT | ATTACH_FLAG_COL_ON, &vOffset, &qOffset, 0.0f, 0.0f);
		}
		// Otherwise if the physical is considered valid ground we just remember it in case it gets changed/deleted out from underneath us
		else if(AssertVerify(capsuleIsect.GetHitDetected()) && pPed->ValidateGroundPhysical(*capsuleIsect.GetHitInst(), capsuleIsect.GetHitPositionV(), capsuleIsect.GetHitComponent()))
		{
			m_PhysicalGroundHandle = CPhysics::GetLevel()->GetHandle(capsuleIsect.GetHitInst());
		}
	}

	phInst* pAnimatedInst = pPed->GetAnimatedInst();
	
	if (physicsVerifyf(pAnimatedInst != NULL, "CTaskDyingDead::SwitchToRagdollFrame: Invalid animated instance") && 
		physicsVerifyf(pAnimatedInst->IsInLevel(), "CTaskDyingDead::SwitchToRagdollFrame: Animated instance should always be in the level"))
	{
		if(!bAddPhysics)
		{
			// We no longer remove the animated physics instance from the world here. It never really made
			// sense to have our current physics inst point to a phInst which is not in the level and now
			// that we need to refer to the physics level to query per-instance type/include flags this can
			// cause problems.
			// Instead, since we have the ability to set per-instance type/include flags, lets just disable
			// collision on the animated inst and leave it in the level.
			if (!pPed->GetAttachParent() && pPed->GetAttachState() != ATTACH_STATE_BASIC && pPed->GetAttachState() != ATTACH_STATE_WORLD)
			{
				pPed->UpdateEntityFromPhysics(pAnimatedInst, 0);
			}			
			m_flags.SetFlag(Flag_disableCapsuleInRagdollFrameState);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule, true);

			// be sure to deactivate the inst too
			if (CPhysics::GetLevel()->IsActive(pAnimatedInst->GetLevelIndex()))
			{
				CPhysics::GetSimulator()->DeactivateObject(pAnimatedInst->GetLevelIndex());
			}

			pAnimatedInst->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);

			// NB - Because the collision include flags will be zero, the animated bounds will not even show
			// up in RAGE profile draw!
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pAnimatedInst->GetLevelIndex(), 0);
		}
		else
		{
			// Ensure the mover bound roughly matches the ped's spine as the bound is used to keep the ped from clipping through world collision when floating around in water
			// or when positioned on a physical object. This logic is deliberately disabled on non bipeds.
			if (bIsBiped)
			{
				pPed->OrientBoundToSpine(INSTANT_BLEND_IN_DELTA);
				// Call process bounds directly because it is not normally called when dead
				pPed->ProcessBounds(0.0f);
			}

			m_flags.ClearFlag(Flag_disableCapsuleInRagdollFrameState);

			// If we are attached (e.g. to a vehicle) and we are messing with the collision include flags, we
			// need to inform the attachment system that we are expecting to have collision on.
			if(pPed->GetIsAttached() && !pPed->GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_COL_ON))
			{
				pPed->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_COL_ON, true);
			}
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pAnimatedInst->GetLevelIndex(), ArchetypeFlags::GTA_ALL_MAP_TYPES);
		}
	}
	
	// We've taken care of switching the ped to his animated state here. Signal 
	// any child states not to do the switch to animated when aborting ( this 
	// probably shouldn't be needed, but for some reason this casues peds to 
	// pop back to thier upright position and orientation at small timescales).
	if (GetSubTask() && GetSubTask()->GetTaskType()== CTaskTypes::TASK_NM_CONTROL)
	{
		smart_cast<CTaskNMControl*>(GetSubTask())->SetDontSwitchToAnimatedOnAbort(true);
	}

	m_flags.ClearFlag(Flag_GoStraightToRagdollFrame);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskDyingDead::CheckRagdollFrameDepthConditions( CPed* pPed )
{
	if(pPed->GetIsInWater() )
	{
		Matrix34 rootMatrix;
		// Use the spine as the switch position
		pPed->GetBoneMatrix(rootMatrix, BONETAG_SPINE2);
		Vector3 vecSwitchPos = rootMatrix.d;
		Vector3 vecGroundPos(vecSwitchPos);

		const float fSecondSurfaceInterp=0.0f;

		// Figure out the depth of the water at switch position
		CPedPlacement::FindZCoorForPed(fSecondSurfaceInterp,&vecGroundPos, NULL);
		const CBaseCapsuleInfo* pPedCapsule = CPedCapsuleInfoManager::GetInfo(pPed->GetPedModelInfo()->GetPedCapsuleHash().GetHash());
		if (pPedCapsule)
		{
			vecGroundPos.z -= pPedCapsule->GetGroundToRootOffset();
		}

		float fWaterZ = 0.0f;
		if(Water::GetWaterLevel(vecSwitchPos,&fWaterZ,true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
		{
			// Just bail out since deactivating the ragdoll in shallower water can look bad
			if (fWaterZ - vecGroundPos.z >= SIMPLE_DEAD_MIN_WATER_DEPTH && fWaterZ - vecGroundPos.z <= SIMPLE_DEAD_DEEP_WATER_THRESHOLD)
			{
				return false;
			}
		}
	}
	return true;
}

void CTaskDyingDead::SetDeathInfo()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	u32 uWeaponHash = m_deathSourceInfo.GetWeapon();
	CEntity* pEntity = m_deathSourceInfo.GetEntity();

	// So use the prev damager and weapon if bleeding as bleeding damage will not register in the history there
	if (m_deathSourceInfo.GetWeapon() == WEAPONTYPE_BLEEDING)
	{
		uWeaponHash = pPed->GetWeaponDamageHash();
		pEntity = pPed->GetWeaponDamageEntity();
	}

	//Set the source of death.
	pPed->SetSourceOfDeath(pEntity);

	//only set cause of death if we don't already have one, or the new one is one of the types
	//that is unrevivable.  the rationale being we don't want someone taking fall damage after being shot
	//to become able to be revived afterward.  really this would be best implemented as some sort of
	//ordered list of damagetypes, but the list will do for now
	if (pPed->GetCauseOfDeath() == 0 || !CTaskRevive::IsRevivableDamageType(uWeaponHash))
	{
		pPed->SetCauseOfDeath(uWeaponHash);
	}

	//Set the time of death.
	if(pPed->GetTimeOfDeath() == 0)
	{
		pPed->SetTimeOfDeath(fwTimer::GetTimeInMilliseconds());
	}
}

void CTaskDyingDead::SetProcessBloodTimer(float fBloodTimer)
{
	m_tProcessBloodPoolTimer.SetTime(fBloodTimer);
}

void CTaskDyingDead::ProcessPushFromVehicle()
{
	if (m_flags.IsFlagSet(Flag_ragdollWhenAble))
	{
		CPed& ped = *GetPed();
		if (ped.GetMyVehicle())
		{
			const bool bInheritsFromPlane = ped.GetMyVehicle()->InheritsFromPlane();
			if (!bInheritsFromPlane && GetTimeInState() < ms_Tunables.m_TimeToApplyPushFromVehicleForce && (ped.GetAttachState() == ATTACH_STATE_DETACHING))
			{
				Vector3 vShotStart = VEC3V_TO_VECTOR3(ped.GetMyVehicle()->GetTransform().GetPosition()) + ms_Tunables.m_VehicleForwardScale * VEC3V_TO_VECTOR3(ped.GetMyVehicle()->GetTransform().GetB());
				Vector3 vAwayFromVehicle = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) - vShotStart;
				vAwayFromVehicle.Normalize();
#if DEBUG_DRAW
				ms_debugDraw.AddLine(ped.GetTransform().GetPosition(), ped.GetTransform().GetPosition() + RCC_VEC3V(vAwayFromVehicle), Color_red, 2000);
#endif
				ped.ApplyImpulseCg(ms_Tunables.m_ForceToApply * vAwayFromVehicle);
			}
			else
			{
				if(ped.GetAttachState() == ATTACH_STATE_DETACHING)
				{
					u16 uFlags = bInheritsFromPlane ? DETACH_FLAG_EXCLUDE_VEHICLE : DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR|DETACH_FLAG_EXCLUDE_VEHICLE;
					ped.DetachFromParent(uFlags);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CClonedDyingDeadInfo
//////////////////////////////////////////////////////////////////////////

CClonedDyingDeadInfo::CClonedDyingDeadInfo(u32 const state, const CDeathSourceInfo& deathSourceInfo, s32 shotBoneTag, s32 flags,
										   const u32 &clipSetId, const u32 &clipId, float blendDuration)
										   : m_deathSourceInfo(deathSourceInfo)
                                           , m_lastSignificantShotBoneTag(shotBoneTag)
										   , m_flags(flags)
										   , m_clipSetId(clipSetId)
										   , m_clipId(clipId)
										   , m_blendDuration(blendDuration)
{
	SetStatusFromMainTaskState(state);

	// clear the flags we are not interested in
	m_flags &= ((1<<SIZEOF_FLAGS)-1);

	taskAssertf(!(flags & CTaskDyingDead::Flag_clipsOverridenByClipSetHash) || !(flags & CTaskDyingDead::Flag_clipsOverridenByDictHash), "Cant have both clip flags set!" );
 
	if (m_clipSetId != CLIP_SET_ID_INVALID)
	{
		Assert((flags & CTaskDyingDead::Flag_clipsOverridenByClipSetHash) || (flags & CTaskDyingDead::Flag_clipsOverridenByDictHash));
	}
	else
	{
		Assert(!(flags & CTaskDyingDead::Flag_clipsOverridenByClipSetHash) && !(flags & CTaskDyingDead::Flag_clipsOverridenByDictHash));
	}

	if (blendDuration < -1.0f || blendDuration > 1.0f)
	{
		Assertf(0, "Invalid blend duration %f passed into CClonedDyingDeadInfo", blendDuration);
		m_blendDuration = NORMAL_BLEND_DURATION;
	}
}

CClonedDyingDeadInfo::CClonedDyingDeadInfo()
: m_flags(0)
, m_lastSignificantShotBoneTag(-1)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_blendDuration(NORMAL_BLEND_DURATION)
{
}

CTaskFSMClone *CClonedDyingDeadInfo::CreateCloneFSMTask()
{
	CTaskDyingDead* pTask = rage_new CTaskDyingDead(&m_deathSourceInfo, m_flags);

	if (HasClip())
	{
		if (m_flags & CTaskDyingDead::Flag_clipsOverridenByClipSetHash)
		{
			pTask->SetDeathAnimationBySet(fwMvClipSetId(m_clipSetId), fwMvClipId(m_clipId), m_blendDuration);
		}
		else if (AssertVerify(m_flags & CTaskDyingDead::Flag_clipsOverridenByDictHash))
		{
			//! Clip Set ID is actually storing a dictionary ID here. So don't convert.
			s32 iDictIndex = m_clipSetId; 
			pTask->SetDeathAnimationByDictionary(iDictIndex, m_clipId, m_blendDuration);
		}
	}

	return pTask;
}

//////////////////////
//HIT RESPONSE
//////////////////////

CTaskHitResponse::CTaskHitResponse(const int iHitSide)
: m_iHitSide(iHitSide)
{
	SetInternalTaskType(CTaskTypes::TASK_HIT_RESPONSE);
}

CTaskHitResponse::~CTaskHitResponse()
{
}
CTask::FSM_Return CTaskHitResponse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
		FSM_OnEnter
		return StateInitial_OnEnter(pPed);
	FSM_OnUpdate
		return StateInitial_OnUpdate(pPed);
	FSM_State(State_PlayingClip)
		FSM_OnEnter
		return StatePlayingClip_OnEnter(pPed);
	FSM_OnUpdate
		return StatePlayingClip_OnUpdate(pPed);
	FSM_End
}
CTask::FSM_Return CTaskHitResponse::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskHitResponse::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	SetState(State_PlayingClip);
	return FSM_Continue;
}
CTask::FSM_Return CTaskHitResponse::StatePlayingClip_OnEnter(CPed *pPed)
{
	CMovePed &movePed = pPed->GetMovePed();
	CGenericClipHelper &additiveHelper = movePed.GetAdditiveHelper();

	switch(m_iHitSide)
	{
	case CEntityBoundAI::LEFT:
		additiveHelper.BlendInClipBySetAndClip(pPed, pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_LEFT, SLOW_BLEND_IN_DELTA);
		return FSM_Continue;
	case CEntityBoundAI::RIGHT:
		additiveHelper.BlendInClipBySetAndClip(pPed, pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_RIGHT, SLOW_BLEND_IN_DELTA);
		return FSM_Continue;
	case CEntityBoundAI::FRONT:
		additiveHelper.BlendInClipBySetAndClip(pPed, pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_FRONT, SLOW_BLEND_IN_DELTA);
		return FSM_Continue;
	case CEntityBoundAI::REAR:
		additiveHelper.BlendInClipBySetAndClip(pPed, pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_BACK, SLOW_BLEND_IN_DELTA);
		return FSM_Continue;
	default:
		Assert(0);
		return FSM_Quit;
	}
}
CTask::FSM_Return CTaskHitResponse::StatePlayingClip_OnUpdate(CPed *pPed)
{
	CMovePed &movePed = pPed->GetMovePed();
	CGenericClipHelper &additiveHelper = movePed.GetAdditiveHelper();

	switch(m_iHitSide)
	{
	case CEntityBoundAI::LEFT:
		if(!additiveHelper.IsClipPlaying(pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_LEFT))
		{
			return FSM_Quit;
		}
		break;
	case CEntityBoundAI::RIGHT:
		if(!additiveHelper.IsClipPlaying(pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_RIGHT))
		{
			return FSM_Quit;
		}
		break;
	case CEntityBoundAI::FRONT:
		if(!additiveHelper.IsClipPlaying(pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_FRONT))
		{
			return FSM_Quit;
		}
		break;
	case CEntityBoundAI::REAR:
		if(!additiveHelper.IsClipPlaying(pPed->GetPedModelInfo()->GetAdditiveDamageClipSet(), CLIP_DAM_BACK))
		{
			return FSM_Quit;
		}
		break;
	default:
		Assert(0);
	}

	return FSM_Continue;
}


/////////////
//ON FIRE
/////////////

const float CTaskComplexOnFire::ms_fSafeDistance = 1000.0f;
const float CTaskComplexOnFire::ms_fHealthRate = 18.75f;
const u32 CTaskComplexOnFire::ms_nMaxFleeTime = 100000;
//const u32 CTaskComplexOnFire::ms_nMaxPlayerOnFireTime = 3000;

CTaskComplexOnFire::CTaskComplexOnFire(u32 weaponHash)
: CTaskComplex()
{
	m_nWeaponHash = weaponHash;
	//	m_nStartTime = 0;

	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_ON_FIRE);
}

CTaskComplexOnFire::~CTaskComplexOnFire()
{
}

aiTask* CTaskComplexOnFire::CreateNextSubTask(CPed* pPed)
{
	aiTask* pNextSubTask = 0;
	CFire* pFire = NULL;
	if (g_fireMan.IsEntityOnFire(pPed))
	{
		pFire = g_fireMan.GetEntityFire(pPed);
	}
	
	switch(GetSubTask()->GetTaskType())
	{
	case CTaskTypes::TASK_NM_CONTROL:
	case CTaskTypes::TASK_NM_ONFIRE:
	case CTaskTypes::TASK_NM_RELAX:
	case CTaskTypes::TASK_FALL_OVER:
		{
			if(pPed->IsDead() || (pPed->IsInjured() && pFire==NULL))
			{
				pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_DYING_DEAD);
				if(pPed->GetUsingRagdoll())
				{
					aiTask* pTaskNM = CreateSubTask(pPed, CTaskTypes::TASK_NM_RELAX);
					((CTaskDyingDead*)pNextSubTask)->SetForcedNaturalMotionTask(pTaskNM);
				}
			}
			else if(pFire)
			{
				if(pPed->GetPlayerInfo() && !pPed->GetUsingRagdoll())
				{
					// Give player some time to escape the fire.
					static dev_u32 uProofTime = 1500;
					pPed->GetPlayerInfo()->SetFireProofTimer(uProofTime);
					if(GetSubTask()->GetTaskType()!=CTaskTypes::TASK_NM_CONTROL
						|| (static_cast<CTaskNMControl*>(GetSubTask())->GetFlags() & CTaskNMControl::DO_BLEND_FROM_NM) == 0)
					{
						pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_GET_UP);
					}
				}
				else if(pPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FIRE))
				{
						// switch between relax and onfire if still on fire
						if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_RELAX)
							pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_NM_ONFIRE);
						else
							pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_NM_RELAX);
				}
				else if(GetSubTask()->GetTaskType()!=CTaskTypes::TASK_NM_CONTROL
					|| (static_cast<CTaskNMControl*>(GetSubTask())->GetFlags() & CTaskNMControl::DO_BLEND_FROM_NM) == 0)
				{
					pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_GET_UP);
				}
			}
			else
			{
				if(pPed->GetUsingRagdoll())
				{
					if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_ONFIRE
						&& ((CTaskNMOnFire*)GetSubTask())->GetSuggestedNextTask() == CTaskTypes::TASK_BLEND_FROM_NM)
						pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_BLEND_FROM_NM);
					else
						pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_NM_RELAX);
				}
				else if(GetSubTask()->GetTaskType()!=CTaskTypes::TASK_NM_CONTROL
					|| (static_cast<CTaskNMControl*>(GetSubTask())->GetFlags() & CTaskNMControl::DO_BLEND_FROM_NM) == 0)
				{
					pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_GET_UP);
				}
			}
		}
		break;
	case CTaskTypes::TASK_GET_UP:
	case CTaskTypes::TASK_BLEND_FROM_NM:
		{
			if(pFire && (!pPed->GetPlayerInfo() || !pPed->GetPlayerInfo()->bFireProof))
			{
				if(pPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FIRE))
				{
					pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_NM_ONFIRE);
				}
				else if(!pPed->GetBlockingOfNonTemporaryEvents())
				{
					pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_SMART_FLEE);
				}	
			}
		}
		break;
	case CTaskTypes::TASK_SMART_FLEE:
		{
			if(pPed->IsDead() || (pPed->IsInjured() && pFire==NULL))
			{
				pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_DYING_DEAD);
			}
			else if(pFire)
			{
				if(pPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FIRE))
				{
					pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_NM_ONFIRE);
				}
				else
				{
					pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_FALL_OVER);
				}
			}
			else
				pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_FINISHED);
		}
		break;
	case CTaskTypes::TASK_DYING_DEAD:
		{
			pNextSubTask = CreateSubTask(pPed, CTaskTypes::TASK_FINISHED);
		}
		break;

	default:
		Assertf(false, "CTaskComplexOnFire::CreateNextSubTask: Unhandled task type %d", GetSubTask()->GetTaskType());
		break;
	}
	return pNextSubTask;
}
#define CHANCE_OF_SWITCHING_TO_WRYTHING 0.02f
#define INITIAL_CHANCE_OF_SWITCHING_TO_WRYTHING 0.2f

aiTask* CTaskComplexOnFire::CreateFirstSubTask(CPed* pPed)
{
	//	m_nStartTime = fwTimer::GetTimeInMilliseconds();

	if(pPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FIRE))
	{
		return CreateSubTask(pPed, CTaskTypes::TASK_NM_ONFIRE);
	}
	else
	{
		if(pPed->IsPlayer())
		{
			return CreateSubTask(pPed, CTaskTypes::TASK_FALL_OVER);
		}
		else
		{
			return CreateSubTask(pPed, CTaskTypes::TASK_SMART_FLEE);
		}
	}
}


aiTask* CTaskComplexOnFire::ControlSubTask(CPed* pPed)
{
	// Make petrol can explode if the ped is on fire.
	CWeapon* pEquippedWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pEquippedWeapon && pEquippedWeapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_PETROLCAN", 0x34A67B97))
	{
		CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		static dev_float fPetrolCanExplodeDelayTime = 1.0f;	// Add some delay
		if(pWeaponObject && GetTimeRunning() > fPetrolCanExplodeDelayTime)
		{
			Vector3 vFireDirection(0.0f, 0.0f, 1.0f);

			CExplosionManager::CExplosionArgs explosionArgs(EXP_TAG_GAS_CANISTER, VEC3V_TO_VECTOR3(pWeaponObject->GetTransform().GetPosition()));

			explosionArgs.m_pEntExplosionOwner = pPed;
			explosionArgs.m_bInAir = false;
			explosionArgs.m_vDirection = vFireDirection;
			explosionArgs.m_originalExplosionTag = EXP_TAG_GAS_CANISTER;

			CExplosionManager::AddExplosion(explosionArgs);

			// Destroy the weapon object and remove it from inventory
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false);
			pPed->GetInventory()->RemoveWeapon(pEquippedWeapon->GetWeaponHash());
		}
	}

	aiTask* pReturnTask = GetSubTask();	

	//	if(pPed->GetIsInWater()
	//	|| (pPed->IsPlayer() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + ms_nMaxPlayerOnFireTime))
	//	{
	//		g_fireMan.ExtinguishEntityFires(pPed);
	//	}

	const bool bPedKilled = ApplyPerFrameFiredamage(pPed, m_nWeaponHash);

	// send a death event as soon as the ped is fatally injured
	if (pPed->IsFatallyInjured())
	{
		if (!pPed->GetUsingRagdoll() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			CEventDeath deathEvent(false, true);
			pPed->SwitchToRagdoll(deathEvent);
		}
		else
		{
			CEventDeath deathEvent(false, pPed->GetUsingRagdoll());

			if(!pPed->GetUsingRagdoll())
			{
				fwMvClipSetId clipSetId;
				fwMvClipId clipId;

				Vector3 vecHeadPos(0.0f,0.0f,0.0f);
				pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);

				if(vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf() - 0.2f)
				{
					const EstimatedPose pose = pPed->EstimatePose();
					if(pose == POSE_ON_BACK)
					{
						clipId = CLIP_DAM_FLOOR_BACK;
					}
					else
					{
						clipId = CLIP_DAM_FLOOR_FRONT;
					}

					Matrix34 rootMatrix;
					if(pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
					{
						float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
						if(fAngle < -QUARTER_PI)
						{
							clipId = CLIP_DAM_FLOOR_LEFT;
						}
						else if(fAngle > QUARTER_PI)
						{
							clipId = CLIP_DAM_FLOOR_RIGHT;
						}
					}

					clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();
				}
				else
				{
					CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::ComputeFallContext(m_nWeaponHash, BONETAG_ROOT), CTaskFallOver::kDirFront, clipSetId, clipId);
				}
				deathEvent.SetClipSet(clipSetId, clipId);
			}

			pPed->GetPedIntelligence()->AddEvent(deathEvent);
		}		

		return pReturnTask;
	}

	ProcessFacialIdle();

	// other subtasks should deal with dying or the fire going out and finish themselves
	if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_SMART_FLEE)
	{
		if(!g_fireMan.IsEntityOnFire(pPed) || bPedKilled)
		{
			if(GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0))
			{
				pReturnTask = CreateNextSubTask(pPed);
			}
		}
	}

	return pReturnTask;
}



aiTask* CTaskComplexOnFire::CreateSubTask(CPed* pPed, const int iSubTaskType) const
{
	aiTask* pSubTask=0;
	switch(iSubTaskType)
	{
	case CTaskTypes::TASK_SMART_FLEE:
		{
			CEntity* pFleeEntity = CGameWorld::FindLocalPlayer();
			CFire* pFire = NULL;
			if (g_fireMan.IsEntityOnFire(pPed))
			{
				pFire = g_fireMan.GetEntityFire(pPed);
			}
			if(pFire && pFire->GetCulprit())
				pFleeEntity = pFire->GetCulprit();

			pSubTask=rage_new CTaskSmartFlee(CAITarget(pFleeEntity), ms_fSafeDistance, ms_nMaxFleeTime);
		}
		break;
	case CTaskTypes::TASK_DYING_DEAD:
		pSubTask = rage_new CTaskDyingDead(NULL, 0, 0);
		break;
	case CTaskTypes::TASK_NM_ONFIRE:
		pSubTask = rage_new CTaskNMOnFire(1000, 30000);

		//- set weak option ?
		{
			if (CTaskNMBehaviour::ms_bUseParameterSets && !pPed->CheckAgilityFlags(AF_RAGDOLL_ON_FIRE_STRONG)) {
				((CTaskNMOnFire*)pSubTask)->SetType(CTaskNMOnFire::ONFIRE_WEAK);
			}
		}

		{
			// NM tasks must be passed to the NM control task.
			CTaskNMBehaviour* pTempTaskNM = smart_cast<CTaskNMBehaviour*>(pSubTask);
			pSubTask = rage_new CTaskNMControl(pTempTaskNM->GetMinTime(), pTempTaskNM->GetMaxTime(), pTempTaskNM, CTaskNMControl::DO_BLEND_FROM_NM | CTaskNMControl::FORCE_FALL_OVER);
		}

		break;
	case CTaskTypes::TASK_NM_RELAX:
		pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the next line to route to TaskRageRagdoll
		if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
			CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);
		pSubTask = rage_new CTaskNMControl(300, 10000, rage_new CTaskNMRelax(300, 10000), CTaskNMControl::DO_BLEND_FROM_NM | CTaskNMControl::FORCE_FALL_OVER);
		break;
	case CTaskTypes::TASK_BLEND_FROM_NM:
		{
			eNmBlendOutSet getupSet = pPed->IsMale() ? NMBS_SLOW_GETUPS : NMBS_SLOW_GETUPS_FEMALE;
			if(pPed->IsPlayer())
				getupSet = pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE;

			pSubTask = rage_new CTaskBlendFromNM(getupSet);
			// NM tasks must be passed to the NM control task.
			{
				CTaskBlendFromNM *pTempTaskNM = static_cast<CTaskBlendFromNM*>(pSubTask);
				pSubTask = rage_new CTaskNMControl(1000, 100000, pTempTaskNM, CTaskNMControl::DO_BLEND_FROM_NM);
			}
		}
		break;
	case CTaskTypes::TASK_FALL_OVER:
		{
			fwMvClipSetId clipSetId;
			fwMvClipId clipId;
			CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextOnFire, CTaskFallOver::kDirFront, clipSetId, clipId);
			pSubTask = rage_new CTaskFallOver(clipSetId, clipId, 5.0f);
		}
		break;

	case CTaskTypes::TASK_GET_UP:
		pSubTask = rage_new CTaskGetUp();
		break;
	case CTaskTypes::TASK_FINISHED:
		pSubTask=0;
		break;
	default:
		Assertf(false, "CTaskComplexOnFire::CreateSubTask: Unhandled task type %d", iSubTaskType);
		break;			
	}

	// Check that there were no NM tasks generated in this function without CTaskNMControl as parent
	// for easier debugging.
	ASSERT_ONLY(CTask *pSubCTask = (CTask *) pSubTask;)
		taskAssertf(!pSubCTask || !pSubCTask->IsNMBehaviourTask() || !pSubCTask->IsBlendFromNMTask(), "NM behaviour task found without control task.");

	return pSubTask;
}

void CTaskComplexOnFire::ProcessFacialIdle()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		if (pPed && !pPed->ShouldBeDead() && !pPed->IsFatallyInjured())
		{
			if (!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP))
			{
				CFacialDataComponent* pFacialData = pPed->GetFacialData();
				if (pFacialData)
				{
					pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_OnFire);
				}
			}
		}
	}
}

bool CTaskComplexOnFire::ApplyPerFrameFiredamage( CPed* pPed, u32 weaponHash )
{
	CFire* pFire = NULL;
	if (g_fireMan.IsEntityOnFire(pPed))
	{
		pFire = g_fireMan.GetEntityFire(pPed);
	}

	if( pFire == NULL )
		return false;

	bool bInflictDamage = true;

	// Do not inflict damage on a player if the controls are disabled
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if( pPlayerInfo && (pPlayerInfo->AreControlsDisabled() || pPlayerInfo->bFireProof))
	{
		bInflictDamage = false;
	}
	else if(pPed->m_nPhysicalFlags.bNotDamagedByFlames || pPed->m_nPhysicalFlags.bNotDamagedByAnything)
	{
		bInflictDamage = false;
	}

	//@@: range CTASKCOMPLEXONFIRE_APPLYPERFRAMEFIREDAMAGE {
	#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_INVINCIBLECOP
		else if(TamperActions::IsInvincibleCop(pPed))
		{
			bInflictDamage = false;
		}
	#endif
	//@@: } CTASKCOMPLEXONFIRE_APPLYPERFRAMEFIREDAMAGE

	bool bApplyKillDamage = false;
	if(GetSubTask())
	{
		switch(GetSubTask()->GetTaskType())
		{
		case CTaskTypes::TASK_NM_CONTROL:
		case CTaskTypes::TASK_NM_ONFIRE:
		case CTaskTypes::TASK_NM_RELAX:
			{
				// no kill damage.
			}
			break;
		default:
			// if we are playing anim fallback (i.e. no ragdoll, then kill immediately).
			bApplyKillDamage = !pPed->GetUsingRagdoll() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured);
			break;
		}
	}

	float fDamageToTake;
	if(!pPlayerInfo && bApplyKillDamage)
	{
		fDamageToTake = pPed->GetHealth();
	}
	else
	{
		fDamageToTake = ms_fHealthRate*fwTimer::GetTimeStep();
	}

	bool bPedKilled = false;
	if( bInflictDamage )
	{
		CEventDamage event(NULL, fwTimer::GetTimeInMilliseconds(), weaponHash);
		CPedDamageCalculator damageCalculator(pFire->GetCulprit(), fDamageToTake, weaponHash, 0, false);
		damageCalculator.ApplyDamageAndComputeResponse(pPed, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);
		if (CLocalisation::PedsOnFire() && pPed->GetSpeechAudioEntity())
		{
			audDamageStats damageStats;
			damageStats.Fatal = false;
			damageStats.RawDamage = 20.0f;
			damageStats.DamageReason = AUD_DAMAGE_REASON_ON_FIRE;
			damageStats.PedWasAlreadyDead = pPed->ShouldBeDead();
			pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
		}
		bPedKilled = (event.GetDamageResponseData().m_bKilled || event.GetDamageResponseData().m_bInjured);
	}
	return bPedKilled;
}

void CTaskComplexOnFire::GetUpComplete(eNmBlendOutSet UNUSED_PARAM(setMatched), CNmBlendOutItem* UNUSED_PARAM(lastItem), CTaskMove* UNUSED_PARAM(pMoveTask), CPed* pPed)
{
	static const fwMvClipSetId s_OnFireSet("reaction@shake_it_off", 0xC6AE9A3E);	
	static const fwMvClipId s_OnFireClip("dustoff", 0xC9A0759E);
	pPed->GetMotionData()->RequestCustomIdle(s_OnFireSet, s_OnFireClip);
}

bool CTaskComplexOnFire::AddGetUpSets(CNmBlendOutSetList& UNUSED_PARAM(sets), CPed* pPed)
{
	// Give player some time to escape the fire.
	if (pPed->GetPlayerInfo())
	{
		static dev_u32 uProofTime = 1500;
		pPed->GetPlayerInfo()->SetFireProofTimer(uProofTime);
	}

	return false;
}

bool CTaskComplexOnFire::ShouldAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_ON_FIRE)
	{
		if(!((CEvent*)pEvent)->RequiresAbortForRagdoll())
		{
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CClonedDamageElectricInfo::CClonedDamageElectricInfo( u32 _damageTotalTime, fwMvClipSetId _clipSetId, eRagdollTriggerTypes _ragdollType, bool _animated )
:
m_uDamageTotalTime( _damageTotalTime ),
m_ClipSetId( _clipSetId ),
m_RagdollType( _ragdollType ),
m_Animated( _animated )
{}

CClonedDamageElectricInfo::CClonedDamageElectricInfo()
{}

CClonedDamageElectricInfo::~CClonedDamageElectricInfo()
{}

CTaskFSMClone *CClonedDamageElectricInfo::CreateCloneFSMTask()
{
	return rage_new CTaskDamageElectric( m_ClipSetId, m_uDamageTotalTime, m_RagdollType, NULL, m_Animated );
}

////////////////////////////////////////////////////////////////////////////////

const fwMvClipId CTaskDamageElectric::ms_damageClipId("damage",0x431375E1);
const fwMvClipId CTaskDamageElectric::ms_damageVehicleClipId("damage_vehicle",0xE2D45A9D);

CTaskDamageElectric::Tunables CTaskDamageElectric::ms_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskDamageElectric, 0x8f5bf7f2);

CTaskDamageElectric::CTaskDamageElectric(fwMvClipSetId clipSetId, u32 uTime, eRagdollTriggerTypes ragdollType, CTaskNMControl* forcedNMTask, bool forceAnimated)
: m_ClipSetId(clipSetId)
, m_uDamageStartTime(fwTimer::GetTimeInMilliseconds())
, m_uDamageTotalTime(uTime)
, m_RagdollType(ragdollType)
, m_forcedNMTask(forcedNMTask)
, m_fSteeringBias(0.0f)
, m_bRestartNMTask(false)
, m_vHitPosition(VEC3_ZERO)
, m_vHitDirection(VEC3_ZERO)
, m_vHitPolyNormal(VEC3_ZERO)
, m_pFiringEntity(NULL)
, m_uWeaponHash(0)
, m_hitComponent(0)
, m_bUseShotReaction(false)
, m_bForceAnimated(forceAnimated)
{
	SetInternalTaskType(CTaskTypes::TASK_DAMAGE_ELECTRIC);
}

CTaskDamageElectric::CTaskDamageElectric(fwMvClipSetId clipSetId, u32 uTime, eRagdollTriggerTypes ragdollType,
										 CEntity *pFiringEntity, const u32 uWeaponHash, u16 hitComponent, const Vector3 &vHitPosition, 
										 const Vector3 &vHitDirection, const Vector3 &vHitPolyNormal)
										 : m_ClipSetId(clipSetId)
										 , m_uDamageStartTime(fwTimer::GetTimeInMilliseconds())
										 , m_uDamageTotalTime(uTime)
										 , m_RagdollType(ragdollType)
										 , m_forcedNMTask(NULL)
										 , m_fSteeringBias(0.0f)
										 , m_bRestartNMTask(false)
										 , m_vHitPosition(vHitPosition)
										 , m_vHitDirection(vHitDirection)
										 , m_vHitPolyNormal(vHitPolyNormal)
										 , m_pFiringEntity(pFiringEntity)
										 , m_uWeaponHash(uWeaponHash)
										 , m_hitComponent(hitComponent)
										 , m_bUseShotReaction(true)
										 , m_bForceAnimated(false)
{
	SetInternalTaskType(CTaskTypes::TASK_DAMAGE_ELECTRIC);
}

CTaskDamageElectric::~CTaskDamageElectric()
{
	if(m_forcedNMTask)
	{
		delete m_forcedNMTask;
		m_forcedNMTask = NULL;
	}
}

aiTask* CTaskDamageElectric::Copy() const
{ 
	if (m_bUseShotReaction)
		return rage_new CTaskDamageElectric(m_ClipSetId, m_uDamageTotalTime, m_RagdollType, m_pFiringEntity, m_uWeaponHash, 
		m_hitComponent, m_vHitPosition, m_vHitDirection, m_vHitPolyNormal); 
	else
	{
		CTaskNMControl* forcedNMTask = NULL;
		if (m_forcedNMTask && nmVerifyf(m_forcedNMTask->GetTaskType() == CTaskTypes::TASK_NM_CONTROL, "CTaskDamageElectric::Copy: Invalid forced NM task type %s", m_forcedNMTask->GetTaskName()))
		{
			forcedNMTask = SafeCast(CTaskNMControl, m_forcedNMTask->Copy());
		}

		CTaskDamageElectric* pTask = rage_new CTaskDamageElectric(m_ClipSetId, m_uDamageTotalTime, m_RagdollType, forcedNMTask, m_bForceAnimated);
		if (pTask)
			pTask->SetContactPosition(m_vHitPosition);
		return pTask;
	}
}

#if !__FINAL
const char * CTaskDamageElectric::GetStaticStateName( s32 iState )
{
	static const char* stateNames[] = 
	{
		"Init",
		"DamageAnimated",
		"DamageNM",
		"Quit",
	};

	return stateNames[iState];
}
#endif // !__FINAL

void CTaskDamageElectric::Update(u32 uTime, CEntity *pFiringEntity, const u32 uWeaponHash, u16 hitComponent, const Vector3 &vHitPosition, const Vector3 &vHitDirection, const Vector3 &vHitPolyNormal)
{
	m_vHitPosition = vHitPosition;
	m_vHitDirection = vHitDirection;
	m_vHitPolyNormal = vHitPolyNormal;
	m_pFiringEntity = pFiringEntity;
	m_uWeaponHash = uWeaponHash;
	m_hitComponent = hitComponent;

	m_uDamageStartTime = fwTimer::GetTimeInMilliseconds();
	m_uDamageTotalTime = uTime;

	if (NetworkInterface::IsGameInProgress())
	{
		// Ensure when the task restarts that it creates the correct type of task (NM shot task instead of an NM electrocute task)
		m_bUseShotReaction = true;
	}

	if(GetState() == State_DamageNM)
	{
		m_bRestartNMTask = true;
	}
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit_OnUpdate();
		FSM_State(State_DamageAnimated)
			FSM_OnEnter
				return StateDamageAnimated_OnEnter();
			FSM_OnUpdate
				return StateDamageAnimated_OnUpdate();
		FSM_State(State_DamageNM)
			FSM_OnEnter
				return StateDamageNM_OnEnter();
			FSM_OnUpdate
				return StateDamageNM_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::ProcessPreFSM()
{
	if (GetState() == State_DamageAnimated)
	{
		// Continually disable collisions between ped and vehicle so that once the ped has exited the vehicle the two won't collide until there are no impacts
		// This is needed because the in-air event can't be processed if there are collisions between the ped and vehicle
		// Need to do this continually because once the ped has been detached from the vehicle we no longer have a pointer to it...
		CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
		if(pVehicle)
		{
			GetPed()->SetNoCollision(pVehicle, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
		}
	}

	ProcessFacialIdle();

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_BeingElectrocuted, true);

	return FSM_Continue;
}

void CTaskDamageElectric::ProcessFacialIdle()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		if (pPed && !pPed->ShouldBeDead() && !pPed->IsFatallyInjured())
		{
			if (!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP))
			{
				CFacialDataComponent* pFacialData = pPed->GetFacialData();
				if (pFacialData)
				{
					pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Electrocution);
				}
			}
		}
	}
}

void CTaskDamageElectric::CleanUp()
{
	// Cache the ped
	CPed* pPed = GetPed();

	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle)
	{
		pVehicle->m_fSteerInputBias = 0.0f;;
	}

	//Disable blood pools if this was caused by an electric weapon.
	if(pPed->IsDead() || pPed->IsFatallyInjured())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableBloodPoolCreation, true);
	}

	pPed->GetMovePed().ClearAdditiveNetwork(m_networkHelper, SLOW_BLEND_DURATION);
	pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, SLOW_BLEND_DURATION);

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHealthRegenerationWhenStunned))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableHealthRegenerationWhenStunned, false);
	}
}

CTaskDamageElectric::FSM_Return	CTaskDamageElectric::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
    if(iEvent == OnUpdate)
    {
        if(GetStateFromNetwork() == State_Quit)
        {
            SetState(State_Quit);
        }
    }

	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit_OnUpdate();
		FSM_State(State_DamageAnimated)
			FSM_OnEnter
				return StateDamageAnimated_OnEnter();
			FSM_OnUpdate
				return StateDamageAnimated_OnUpdate();
		FSM_State(State_DamageNM)
			FSM_OnEnter
				return StateDamageNM_OnEnterClone();
			FSM_OnUpdate
				return StateDamageNM_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskInfo* CTaskDamageElectric::CreateQueriableState() const
{
	return rage_new CClonedDamageElectricInfo(m_uDamageTotalTime, m_ClipSetId, m_RagdollType, GetState() == State_DamageAnimated);
}

void CTaskDamageElectric::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_DAMAGE_ELECTRIC );
	CClonedDamageElectricInfo *damageElectricInfo = static_cast<CClonedDamageElectricInfo*>(pTaskInfo);

	m_uDamageTotalTime	= damageElectricInfo->GetDamageTotalTime();
	m_ClipSetId			= damageElectricInfo->GetClipSetId();
	m_RagdollType		= damageElectricInfo->GetRagdollType();
	m_bForceAnimated	= damageElectricInfo->GetAnimated();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

void CTaskDamageElectric::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Quit);
}

CTaskFSMClone*	CTaskDamageElectric::CreateTaskForClonePed(CPed *pPed)
{
	CTaskNMControl* forcedNMTask = NULL;

	// prevent the control NM subtask from switching the ped to animated when it aborts
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
	{
		forcedNMTask = SafeCast(CTaskNMControl, smart_cast<CTaskNMControl*>(GetSubTask())->CreateTaskForClonePed(pPed));
	}

	u32 timeRunning = fwTimer::GetTimeInMilliseconds() - m_uDamageStartTime;
	u32 damageTotalTime = m_uDamageTotalTime - timeRunning;

	return rage_new CTaskDamageElectric(m_ClipSetId, damageTotalTime, m_RagdollType, forcedNMTask);
}

CTaskFSMClone*	CTaskDamageElectric::CreateTaskForLocalPed(CPed *pPed)
{
	CTaskNMControl* forcedNMTask = NULL;

	// prevent the control NM subtask from switching the ped to animated when it aborts
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
	{
		forcedNMTask = SafeCast(CTaskNMControl, smart_cast<CTaskNMControl*>(GetSubTask())->CreateTaskForLocalPed(pPed));
	}

	u32 timeRunning = fwTimer::GetTimeInMilliseconds() - m_uDamageStartTime;
	u32 damageTotalTime = m_uDamageTotalTime - timeRunning;

	return rage_new CTaskDamageElectric(m_ClipSetId, damageTotalTime, m_RagdollType, forcedNMTask);
}

bool CTaskDamageElectric::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pEvent)
	{
		const int eventType = ((CEvent*)pEvent)->GetEventType();

		if((eventType == EVENT_SWITCH_2_NM_TASK) && (GetState() == State_DamageAnimated))
		{
			// Need to stay in CTaskDamageElectric, so ignore Switch2NM events spawned by CTaskExitVehicleSeat
			return false;
		}
	}

	if(CTask::ABORT_PRIORITY_IMMEDIATE == iPriority)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}
	else if(CTask::ABORT_PRIORITY_URGENT == iPriority)
	{
        CEvent *pThisEvent = ((CEvent*)pEvent);

		bool bGivingExitVehicleTask = false;
		bool bGivingCuffedTask = false;

        if(pThisEvent && pThisEvent->GetEventType() == EVENT_GIVE_PED_TASK)
        {
            CEventGivePedTask *pGivePedTask = static_cast<CEventGivePedTask *>(pThisEvent);

            if(pGivePedTask->GetTask())
			{
				if (pGivePedTask->GetTask()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
				{
					bGivingExitVehicleTask = true;
				}
				else if (pGivePedTask->GetTask()->GetTaskType() == CTaskTypes::TASK_CUFFED)
				{
					bGivingCuffedTask = true;
				}
			}
        }

		if(pEvent == NULL ||
			bGivingExitVehicleTask ||
			bGivingCuffedTask || 
			((CEvent*)pEvent)->RequiresAbortForRagdoll() || 
			(((CEvent*)pEvent)->GetEventType() == EVENT_DAMAGE && pPed->GetHealth() < 1.0f) ||
			((CEvent*)pEvent)->GetEventType() == EVENT_IN_WATER || 
			((CEvent*)pEvent)->GetEventPriority() == E_PRIORITY_SCRIPT_COMMAND_SP ||
			((CEvent*)pEvent)->GetEventType() == EVENT_STUCK_IN_AIR ||
			((CEvent*)pEvent)->GetEventType() == EVENT_ON_FIRE)
		{
			return CTask::ShouldAbort(iPriority, pEvent);
		}
	}

	return false;
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::StateInit_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

    // ensure the vehicle state is correct for the ped (this task can kickoff while a ped is still
    // entering a vehicle on the local machine but is already inside the vehicle on the owner machine)
    if(pPed->IsNetworkClone())
    {
        NetworkInterface::ForceVehicleForPedUpdate(pPed);
 
		if (!m_bForceAnimated && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL))
		{
			return FSM_Continue;
		}
    }

	if (!m_bForceAnimated && (pPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pPed, m_RagdollType)))
	{
		SetState(State_DamageNM);
	}
	else
	{
		SetState(State_DamageAnimated);
	}
	return FSM_Continue;
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::StateDamageAnimated_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(pPed->GetVehiclePedInside())
	{
		if (fwAnimManager::GetClipIfExistsBySetId(m_ClipSetId, ms_damageVehicleClipId))
		{
			if(!m_networkHelper.BlendInClipBySetAndClip(pPed, m_ClipSetId, ms_damageVehicleClipId, NORMAL_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, false, true, false))
			{
				return FSM_Quit;
			}

			pPed->GetMovePed().SetAdditiveNetwork(m_networkHelper, NORMAL_BLEND_DURATION);
		}

		//Check if abnormal exits are disabled.
		const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
		if(pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits())
		{
			return FSM_Continue;
		}

		static dev_float MIN_ANGLE = DtoR * 20.0f;
		static dev_float MAX_ANGLE = DtoR * 135.0f;
		m_fSteeringBias  = fwRandom::GetRandomNumberInRange(MIN_ANGLE, MAX_ANGLE);
		m_fSteeringBias *= fwRandom::GetRandomTrueFalse() ? -1.f : 1.f;

        if(pPed->IsNetworkClone())
        {
            CTask *exitVehicleTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_EXIT_VEHICLE);

            if(exitVehicleTask)
            {
                SetNewTask(exitVehicleTask);
            }
        }
        else
        {
		    // DMKH. Kick out of vehicle. 
		    Vector3 vVelocity = pPed->GetVehiclePedInside()->GetVelocity();
		    float fSpeedSqr = vVelocity.Mag2();

		    if(fSpeedSqr < (ms_Tunables.m_FallsOutofVehicleVelocity * ms_Tunables.m_FallsOutofVehicleVelocity) )
		    {
				// PDB. Unless the vehicle seat doesn't have dead_fall_out animations...
				s32 iDirectEntryPointIndex = pPed->GetMyVehicle()->GetDirectEntryPointIndexForPed(pPed);
				if (pPed->GetMyVehicle()->IsEntryPointIndexValid(iDirectEntryPointIndex))
				{
					const CVehicleEntryPointAnimInfo* pEntryAnimInfo = pPed->GetMyVehicle()->GetEntryAnimInfo(iDirectEntryPointIndex);
					s32 iDictIndex = -1;
					u32	clipId = 0;
					u32 exitClipSet = pEntryAnimInfo->GetExitPointClipSetId().GetHash();
					bool bHasFallOutAnims = CVehicleSeatAnimInfo::FindAnimInClipSet(fwMvClipSetId(exitClipSet), CTaskExitVehicleSeat::ms_Tunables.m_DeadFallOutClipId, iDictIndex, clipId);

					if (bHasFallOutAnims)
					{
						VehicleEnterExitFlags vehicleFlags;
						vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
						vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
						vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DeadFallOut);
						SetNewTask( rage_new CTaskExitVehicle( pPed->GetMyVehicle(), vehicleFlags ) );
					}
				}
		    }
        }
	}

	return FSM_Continue;
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::StateDamageAnimated_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	CVehicle* pVehicle = pPed->GetVehiclePedInside();

	if(GetIsOutOfTime() && !GetSubTask())
	{
		return FSM_Quit;
	}

	if(pVehicle)
	{
		if (!m_networkHelper.IsClipPlaying(m_ClipSetId, ms_damageVehicleClipId) || m_networkHelper.IsHeldAtEnd())
		{
			if (pPed && pPed->ShouldBeDead())
			{
				return FSM_Quit;
			}
		}
		pVehicle->m_fSteerInputBias = m_fSteeringBias;

		if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			return FSM_Quit;
		}
	}
	else
	{
		// If not in a vehicle and the ped is ragdolling or the owner is running an NM task or we're the owner in a network game then switch to the 
		// damage NM state even if we might not be allowed to ragdoll as we should just end up running the animated fall-back logic
		// Need to always go to the NM damage state 
		if (pPed->GetUsingRagdoll() || 
			(pPed->IsNetworkClone() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL)) ||
			(NetworkInterface::IsGameInProgress() && !pPed->IsNetworkClone() && CTaskNMBehaviour::WantsToRagdoll(pPed, m_RagdollType, NULL, 0.0f) > 0.0f))
		{
			// Check for event spawned by CTaskExitVehicleSeat
			CEvent* pEvent = pPed->GetPedIntelligence()->GetEventOfType(EVENT_SWITCH_2_NM_TASK);

			if (pEvent)
			{
				pPed->GetPedIntelligence()->RemoveEvent(pEvent);

				u32 uTimeInState = static_cast<u32>(GetTimeInState() * 1000.0f);
				// If there's time left then handle switching to NM... otherwise, let things finish off normally...
				if (uTimeInState < m_uDamageTotalTime)
				{
					// Subtract off the time we've spent exiting the vehicle...
					m_uDamageTotalTime -= uTimeInState;
				}
			}

			SetState(State_DamageNM);
			return FSM_Continue;
		}

		if (!m_networkHelper.IsAnimating() && fwAnimManager::GetClipIfExistsBySetId(m_ClipSetId, ms_damageClipId))
		{
			if(!m_networkHelper.BlendInClipBySetAndClip(pPed, m_ClipSetId, ms_damageClipId, SLOW_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, false, true, false))
			{
				return FSM_Quit;
			}
			pPed->GetMovePed().SetTaskNetwork(m_networkHelper, SLOW_BLEND_DURATION);
		}

		if (!m_networkHelper.IsAnimating() || m_networkHelper.IsHeldAtEnd())
		{
			if (pPed && pPed->ShouldBeDead())
			{
				return FSM_Quit;
			}
		}
	}

	return FSM_Continue;
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::StateDamageNM_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	taskAssertf(pPed->GetUsingRagdoll() || (NetworkInterface::IsGameInProgress() && CTaskNMBehaviour::WantsToRagdoll(pPed, m_RagdollType, NULL, 0.0f) > 0.0f) || 
		(!NetworkInterface::IsGameInProgress() && CTaskNMBehaviour::CanUseRagdoll(pPed, m_RagdollType, NULL, 0.0f)), "Ragdoll can't be activated on ped");

	aiTask* pNewTask = m_forcedNMTask;

	if (!pNewTask)
	{
		CTask* pNMTask = NULL;

		switch(m_RagdollType)
		{
		case RAGDOLL_TRIGGER_RUBBERBULLET:
			{
				static dev_float STIFFNESS = 1.0f;
				static dev_float DAMPING = 1.0f;
				pPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the next line to route to TaskRageRagdoll
				if (CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
					CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);
				pNMTask = rage_new CTaskNMRelax(m_uDamageTotalTime, m_uDamageTotalTime, STIFFNESS, DAMPING);
			}
			break;
		case RAGDOLL_TRIGGER_ELECTRIC:
			if (m_bUseShotReaction)
			{
				pNMTask = rage_new CTaskNMShot(pPed, m_uDamageTotalTime, m_uDamageTotalTime, m_pFiringEntity, m_uWeaponHash, m_hitComponent, 
					m_vHitPosition, m_vHitDirection, m_vHitPolyNormal);
			}
			else
			{
				pNMTask = rage_new CTaskNMElectrocute(m_uDamageTotalTime, 10000, m_vHitPosition);
			}
			break;
		default:
			taskAssertf(0, "Unhandled ragdoll type [%d]", m_RagdollType);
			break;
		}

		pNewTask = rage_new CTaskNMControl(m_uDamageTotalTime, m_uDamageTotalTime, pNMTask, pPed->ShouldBeDead() ? 0 : CTaskNMControl::DO_BLEND_FROM_NM);
	}

	SetNewTask(pNewTask);

	m_forcedNMTask = NULL;

	return FSM_Continue;
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::StateDamageNM_OnEnterClone()
{
	aiTask* pNewTask = m_forcedNMTask;

	if (!pNewTask)
	{
		CTaskInfo* pTaskInfo = GetPed()->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_NM_CONTROL, PED_TASK_PRIORITY_MAX);
		
		if (pTaskInfo)
		{	
			pNewTask = pTaskInfo->CreateCloneFSMTask();
			Assert(pNewTask);
		}
	}

	if (pNewTask)
	{
		SetNewTask(pNewTask);
	}

	m_forcedNMTask = NULL;

	return FSM_Continue;
}

CTaskDamageElectric::FSM_Return CTaskDamageElectric::StateDamageNM_OnUpdate()
{
	if(m_bRestartNMTask && CTaskNMBehaviour::CanUseRagdoll(GetPed(), m_RagdollType))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		m_bRestartNMTask = false;
		return FSM_Continue;
	}

	bool bDontGetUp = false;
	CPed* pPed = GetPed();

	if (pPed)
	{
		bDontGetUp = pPed->ShouldBeDead() || pPed->IsFatallyInjured();
	}	

	// Throw a held weapon away after some time
	static u32 timeToThrowWeaponMS = 500;
	if (bDontGetUp && !NetworkInterface::IsGameInProgress() && fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfDeath() > timeToThrowWeaponMS)
	{
		DropWeapon(pPed);
	}

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (bDontGetUp && !pPed->IsNetworkClone())
		{
			// there is an assert in SetHealth() which will fire if the ped is migrating and his health is being altered. We need to prevent it in this case as the ped is already dead, and the new owner will therefore run the dyingdead task on him
			bool bPreventNetAssert = bDontGetUp; 
			pPed->SetHealth(0.0f, bPreventNetAssert);
			CEventDeath event(false, true);
			pPed->GetPedIntelligence()->AddEvent(event);
		}
		return FSM_Quit;
	}
	else
	{
		// stop the nm blend out from happening if the ped dies whilst running the task
		if (bDontGetUp && GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_CONTROL)
		{
			CTaskNMControl* pNmControlTask = smart_cast<CTaskNMControl*>(GetSubTask());
			pNmControlTask->ClearFlag(CTaskNMControl::DO_BLEND_FROM_NM);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskDamageElectric::DropWeapon( CPed* pPed )
{
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead))
	{
		return;
	}

	CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if(pWeaponMgr)
	{
		const CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
		if(pWeapon)
		{
			const bool bHasPickup = CPickupManager::ShouldUseMPPickups(pPed) ? (pWeapon->GetWeaponInfo()->GetMPPickupHash() != 0) : (pWeapon->GetWeaponInfo()->GetPickupHash() != 0);
			if(bHasPickup && CPickupManager::CreatePickUpFromCurrentWeapon(pPed))
			{
				pWeaponMgr->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);
				pWeaponMgr->DestroyEquippedWeaponObject(false);
			}
			else
			{
				// Drop the object
				const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
				if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
				{
					pWeaponMgr->DropWeapon(pWeaponInfo->GetHash(), false);
				}
			}
		}
	}

	if(!pPed->IsLocalPlayer() && pPed->GetInventory())
	{
		pPed->GetInventory()->RemoveAllWeaponsExcept(pPed->GetDefaultUnarmedWeaponHash());
	}

	// Stop using the NM point gun behavior
	if (pPed && pPed->GetUsingRagdoll() && pPed->GetRagdollInst()->GetNMAgentID() >= 0)
	{
		ART::MessageParams msgPointGun;
		msgPointGun.addBool(NMSTR_PARAM(NM_START), false);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_POINTGUN_MSG), &msgPointGun);

		ART::MessageParams msgWeaponMode;
		msgWeaponMode.addInt(NMSTR_PARAM(NM_SETWEAPONMODE_MODE), -1);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SETWEAPONMODE_MSG), &msgWeaponMode);

		ART::MessageParams msgRegisterWeapon;
		msgRegisterWeapon.addInt(NMSTR_PARAM(NM_REGISTER_WEAPON_LEVEL_INDEX), -1);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_REGISTER_WEAPON_MSG), &msgRegisterWeapon);
	}
}

bool CTaskDamageElectric::GetIsOutOfTime() const
{
	return fwTimer::GetTimeInMilliseconds() > (m_uDamageStartTime + m_uDamageTotalTime);
}

//-------------------------------------------------------------------------
//		REVIVE TASK INFO - For peds being revived by medics
//-------------------------------------------------------------------------

CClonedReviveInfo::CClonedReviveInfo
(
 s32 const		_state, 
 CEntity*		_pMedic, 
 CEntity*		_pAmbulance
 ) 
 :
m_pMedic(_pMedic),
m_pAmbulance(_pAmbulance)
{
	SetStatusFromMainTaskState(_state);	
}

CClonedReviveInfo::CClonedReviveInfo()
{}

CClonedReviveInfo::~CClonedReviveInfo()
{}

CTaskFSMClone *CClonedReviveInfo::CreateCloneFSMTask()
{
	return rage_new CTaskRevive( static_cast<CPed*>(m_pMedic.GetEntity()), static_cast<CVehicle*>(m_pAmbulance.GetEntity()) );
}

CTask*	CClonedReviveInfo::CreateLocalTask(fwEntity* UNUSED_PARAM( pEntity ) )
{
	return rage_new CTaskRevive( static_cast<CPed*>(m_pMedic.GetEntity()), static_cast<CVehicle*>(m_pAmbulance.GetEntity()) );
}

//-------------------------------------------------------------------------
//		REVIVE TASK - For peds being revived by medics
//-------------------------------------------------------------------------
CTaskRevive::CTaskRevive(CPed* pMedic, CVehicle* pAmbulance)
:
m_pNewTaskToSet(NULL),
m_pMedic(pMedic),
m_pAmbulance(pAmbulance)
{
	SetInternalTaskType(CTaskTypes::TASK_REVIVE);
}

CTaskRevive::~CTaskRevive()
{
	m_pMedic = NULL;
	m_pAmbulance = NULL;

	// Gah! Can't use GetPed() as it asserts if it's NULL....
	if (GetEntity())
	{
		CPed* ped = (CPed*)GetEntity();

		CTaskMotionBase* pTask = ped->GetPrimaryMotionTask();
		if (pTask)
		{
			pTask->ResetOnFootClipSet();
		}
	}

	m_moveGroupRequestHelper.ReleaseClips();

	Assert(m_pNewTaskToSet == NULL);
}

bool CTaskRevive::IsValidForMotionTask(CTaskMotionBase & task) const
{
	bool isValid = CTask::IsValidForMotionTask(task);
	if(!isValid && GetSubTask())
	{
		//This task is not valid, but an active subtask might be.
		isValid = GetSubTask()->IsValidForMotionTask(task);
	}

	return isValid;
}

void CTaskRevive::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Quit);
}

CTaskFSMClone* CTaskRevive::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	// no need to set data as it's all read in from the queriable state after this is called...
	return rage_new CTaskRevive( m_pMedic, m_pAmbulance );
}

CTaskInfo* CTaskRevive::CreateQueriableState() const
{
	return rage_new CClonedReviveInfo( GetState(), m_pMedic, m_pAmbulance );
}

void CTaskRevive::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_REVIVE);
	CClonedReviveInfo *reviveInfo = static_cast<CClonedReviveInfo*>(pTaskInfo);

	m_pMedic					= static_cast<CPed*>(reviveInfo->GetMedic());
	m_pAmbulance				= static_cast<CVehicle*>(reviveInfo->GetAmbulance());

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

CTaskRevive::FSM_Return CTaskRevive::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Quit)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_State(State_CreateFirstSubTask)
		FSM_State(State_ControlSubTask)
		FSM_State(State_CreateNextSubTask)

		FSM_End	
}

//
//
// Basic FSM implementation, for the inherited class
CTask::FSM_Return CTaskRevive::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_CreateFirstSubTask)
		FSM_OnEnter
		return StateCreateFirstSubTask_OnEnter(pPed);
	FSM_OnUpdate
		return StateCreateFirstSubTask_OnUpdate(pPed);

	FSM_State(State_ControlSubTask)
		FSM_OnEnter
		return StateControlSubTask_OnEnter(pPed);
	FSM_OnUpdate
		return StateControlSubTask_OnUpdate(pPed);

	FSM_State(State_CreateNextSubTask)
		FSM_OnEnter
		return StateCreateNextSubTask_OnEnter(pPed);
	FSM_OnUpdate
		return StateCreateNextSubTask_OnUpdate(pPed);

	FSM_State(State_Quit)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

//
//
//
CTask::FSM_Return CTaskRevive::StateCreateFirstSubTask_OnEnter(CPed* pPed)
{
	m_pNewTaskToSet = CreateFirstSubTask(pPed);
	if(m_pNewTaskToSet)
	{
		return FSM_Continue;
	}
	return FSM_Quit;
}

//
//
//
CTask::FSM_Return CTaskRevive::StateCreateFirstSubTask_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_ControlSubTask);
	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskRevive::StateControlSubTask_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	if(m_pNewTaskToSet)//is a new task ready to be set, if so set it
	{
		SetNewTask(m_pNewTaskToSet);
		m_pNewTaskToSet = NULL;
	}

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskRevive::StateControlSubTask_OnUpdate(CPed* pPed)
{
	if(taskVerifyf(GetSubTask(), "GetSubTask is NULL"))
	{
		//If the subtask has ended or doesn't exit go to the CreateNextSubTask state.
		if( GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			SetState(State_CreateNextSubTask);
			return FSM_Continue;
		}

		aiTask* pTask = ControlSubTask(pPed);

		//is there a new task?
		if(pTask && pTask != GetSubTask())
		{
			m_pNewTaskToSet = pTask;
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else if( pTask == NULL )//no task, so just quit
		{		
			return FSM_Quit;
		}

		return FSM_Continue;
	}
	else
	{
#if !__FINAL
		pPed->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_ASSERT);
#endif // !__FINAL
	}

	return FSM_Quit;
}

//
//
//
CTask::FSM_Return CTaskRevive::StateCreateNextSubTask_OnEnter(CPed* pPed)
{
	m_pNewTaskToSet = CreateNextSubTask(pPed);

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskRevive::StateCreateNextSubTask_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(m_pNewTaskToSet)//if there is a new task go to the control sub task state.
	{
		SetState(State_ControlSubTask);
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;//no new task so quit
	}

}


#if !__FINAL
const char * CTaskRevive::GetStaticStateName( s32 iState )
{
	static const char* sStateNames[] = 
	{
		"State_CreateFirstSubTask",
		"State_ControlSubTask",
		"State_CreateNextSubTask",
		"Quit",
	};

	return sStateNames[iState];
}
#endif

bool CTaskRevive::IsRevivableDamageType(u32 weaponHash)
{
	const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	if (weaponInfo && weaponInfo->GetDoesRevivableDamage())
	{
		return true;
	}

	return false;
}

void CTaskRevive::HelperRequestInjuredClipset(CPed* UNUSED_PARAM(pPed))
{
// 	if (pPed)
// 	{
// 		fwMvClipSetId clipSetId = MOVE_INJURED_GENERIC;
// 		if (!m_moveGroupRequestHelper.HaveClipsBeenLoaded() 
// 			&& m_moveGroupRequestHelper.RequestClipsById(clipSetId))
// 		{
// 			CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
// 			if (pTask)
// 			{
// 				pTask->SetOnFootClipSet(clipSetId);
// 			}
// 		}
// 
// 	}
}

//-------------------------------------------------------------------------
// Creates the first revive ped subtask, which sets the ped to being healthy
// and alive, then makes them stand up
//-------------------------------------------------------------------------
aiTask* CTaskRevive::CreateFirstSubTask( CPed* pPed )
{
	// PREVIOUSLY IN EVENTHANDLER : PH 22/05/2006
	aiTask* pSubTask = NULL;

	//Bring the ped back to life.
	pPed->SetHealth(200.0f);
	pPed->EnableCollision();
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KnockedUpIntoAir, false );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth, false );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown, false );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout, false );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStandardMelee, false );
	pPed->SetDeathState(DeathState_Alive);
	pPed->SetArrestState(ArrestState_None);
	pPed->m_nPhysicalFlags.bRenderScorched = false;	
	pPed->ClearDeathInfo();

	if((0==pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_TREE_PRIMARY)) &&
		(0==pPed->GetPedIntelligence()->GetTaskDefault()))
	{
		pPed->GetPedIntelligence()->AddTaskDefault(pPed->ComputeWanderTask(*pPed));
	}

	HelperRequestInjuredClipset(pPed);

	//int nRandomRecoveryTime = CRandom::GetRandomNumberInRange(5000, 20000);
	//pSubTask = new CTaskSimpleDoNothing(nRandomRecoveryTime);

	if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
	{
		pSubTask = rage_new CTaskBlendFromNM(NMBS_INJURED_GETUPS);
	}
	else
	{
		pPed->RemovePhysics();
		pPed->AddPhysics();
		pSubTask = rage_new CTaskGetUpAndStandStill();
	}
	return pSubTask;
}

aiTask* CTaskRevive::CreateNextSubTask(CPed* pPed)
{
	aiTask* pSubTask = NULL;

	HelperRequestInjuredClipset(pPed);

	switch( GetSubTask()->GetTaskType() )
	{
	case CTaskTypes::TASK_DO_NOTHING:
		{
			if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				pSubTask = rage_new CTaskBlendFromNM(NMBS_INJURED_GETUPS);
			}
			else
			{
				pPed->RemovePhysics();
				pPed->AddPhysics();
				pSubTask = rage_new CTaskGetUpAndStandStill();
			}
		}
		break;

	case CTaskTypes::TASK_GET_UP_AND_STAND_STILL:
	case CTaskTypes::TASK_BLEND_FROM_NM:
		{
			//pPed->NewSay("SAVED", 0, true);
			return 0;
		}
		break;

	default:
		return 0;

	}

	return pSubTask;
}

aiTask* CTaskRevive::ControlSubTask(CPed* pPed )
{
	if(pPed->GetIsInVehicle())
	{
		//finish
		return 0;
	}

	HelperRequestInjuredClipset(pPed);

	aiTask* pSubTask = NULL;

	if(pPed->GetIsStanding() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_DO_NOTHING)
	{
		if(pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
		{
			pSubTask = rage_new CTaskBlendFromNM(pPed->IsMale() ? NMBS_SLOW_GETUPS : NMBS_SLOW_GETUPS_FEMALE);
		}
		else
		{
			pPed->RemovePhysics();
			pPed->AddPhysics();
			pSubTask = rage_new CTaskGetUpAndStandStill();
		}
	}

	if(pSubTask)
	{
		if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
		{

			return pSubTask->Copy();
		}
	}

	return GetSubTask();
}

CDamageInfo::CDamageInfo(u32 weaponHash, const Vector3* pvStartPos, WorldProbe::CShapeTestHitPoint* pResult, Vector3* pvImpulseDir, float fImpulseMag, CPed *pPed)
: m_weaponHash(weaponHash)
, m_vStartPos(VEC3_ZERO)
, m_vImpulseDir(VEC3_ZERO)
, m_vLocalIntersectionPos(VEC3_ZERO)
, m_fImpulseMag(fImpulseMag)
, m_bValidIntersectionInfo(pResult != NULL)
{
	Assert(FPIsFinite(fImpulseMag));
	if( pResult )
	{
		m_iIntersectionComponent = pResult->GetHitComponent();

		// Convert the hit location into local space
		Matrix34 ragdollComponentMatrix;
		if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, m_iIntersectionComponent))
		{
			ragdollComponentMatrix.UnTransform(pResult->GetHitPosition(), m_vLocalIntersectionPos);
			Assert(m_vLocalIntersectionPos.Mag2() < 5.0f);
		}
	}
	if( pvStartPos )
		m_vStartPos = *pvStartPos;

	if( pvImpulseDir )
	{
		Assert(pvImpulseDir->FiniteElements());
		m_vImpulseDir = *pvImpulseDir;
	}
}

Vector3 CDamageInfo::GetWorldIntersectionPos(CPed *pPed) 
{
	// Convert the local hit location into world space
	Vector3 vWorldSpacePos(VEC3_ZERO);
	if (Verifyf(m_bValidIntersectionInfo, "CDamageInfo::GetWorldIntersectionPos - Function called without valid intersection data"))
	{
		Matrix34 ragdollComponentMatrix;
		if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, m_iIntersectionComponent))
		{
			ragdollComponentMatrix.Transform(m_vLocalIntersectionPos, vWorldSpacePos);
		}
	}
	return vWorldSpacePos;
}
