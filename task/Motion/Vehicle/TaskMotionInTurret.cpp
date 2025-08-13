// File Header
#include "Task\Motion\Vehicle\TaskMotionInTurret.h"

#if ENABLE_MOTION_IN_TURRET_TASK

// Game Headers
#include "animation\MoveVehicle.h"
#include "camera\CamInterface.h"
#include "camera\cinematic\CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera\gameplay\GameplayDirector.h"
#include "camera\gameplay\aim\ThirdPersonPedAimCamera.h"
#include "camera\helpers\ControlHelper.h"
#include "Peds\PedIntelligence.h"
#include "renderer\RenderPhases\RenderPhaseCascadeShadows.h"
#include "System\Control.h"
#include "Task\Vehicle\TaskInVehicle.h"
#include "Task\Vehicle\TaskVehicleWeapon.h"
#include "Vehicles\Metadata\VehicleSeatInfo.h"
#include "vehicles\Metadata\VehicleMetadataManager.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

CTaskMotionInTurret::Tunables CTaskMotionInTurret::sm_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskMotionInTurret, 0x4860cf0c);

CTaskMotionInTurret::CTaskMotionInTurret(CVehicle* pVehicle, CMoveNetworkHelper& rMoveNetworkHelper)
	: m_pVehicle(pVehicle)
	, m_MoveNetworkHelper(rMoveNetworkHelper)
	, m_fMvPitch(0.5f)
	, m_fMvHeading(-1.0f)
	, m_fMvTurnSpeed(-1.0f)
	, m_fInitialHeadingDelta(0.0f)
	, m_LastFramesTurretHeading(0.0f)
	, m_fAngVelocity(0.0f)
	, m_fAngAcceleration(0.0f)
	, m_fTimeDucked(0.0f)
	, m_bSeated(false)
	, m_bSweeps(false)
	, m_bTurningLeft(false)
	, m_bWasTurretMoving(false)
	, m_bFiring(false)
	, m_bInterruptDelayedMovement(false)
	, m_bCanBlendOutOfCurrentAnimState(false)
	, m_bIsLookingBehind(false)
	, m_bIsDucking(false)
	, m_vLastFramesVehicleVelocity(Vector3::ZeroType)
	, m_vCurrentVehicleAcceleration(Vector3::ZeroType)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_IN_TURRET);
}

CTaskMotionInTurret::~CTaskMotionInTurret()
{

}

void CTaskMotionInTurret::CleanUp()
{
	const CPed& rPed = *GetPed();
	if (ShouldSetAircraftMode(rPed))
	{
		CRenderPhaseCascadeShadowsInterface::Script_SetAircraftMode(false);
	}
}

bool CTaskMotionInTurret::ProcessMoveSignals()
{
	if (!m_pVehicle)
		return false;

	CPed& rPed = *GetPed();
	CTurret* pTurret = GetTurret();
	if (!rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsSeatShuffling))
	{
		// Update ik
		if (pTurret && !rPed.ShouldBeDead())
		{
			IkHandsToTurret(rPed, *m_pVehicle, *pTurret);
		}

		const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
		if (IsSeatWithHatchProtection(iSeatIndex, *m_pVehicle))
		{
			KeepTurretHatchOpen(iSeatIndex, *m_pVehicle);
		}
	}

	const float fTimeStep = fwTimer::GetTimeStep();
	UpdateAngVelocityAndAcceleration(pTurret, fTimeStep);
	UpdateMvTurnSpeed(fTimeStep);
	UpdateMvPitch(pTurret, fTimeStep);
	UpdateMvHeading(pTurret, rPed);
	SendMvParams();

	switch (GetState())
	{
		case State_InitialAdjust:
			{
				m_bCanBlendOutOfCurrentAnimState |= m_MoveNetworkHelper.GetBoolean(TaskMotionInTurretMoveNetworkMetadata::BlendOut_AdjustStepId);
			}
			break;
		default:
			break;
	}
	// we always need processing
	return true;
}

CTask::FSM_Return CTaskMotionInTurret::ProcessPreFSM()
{
	CPed& rPed = *GetPed();
	if (!m_pVehicle)
	{
		aiDebugf1("Frame %u, ped %s quitting CTaskMotionInTurret due to NULL m_pVehicle", fwTimer::GetFrameCount(), rPed.GetModelName());
		return FSM_Quit;
	}

	if (rPed.IsLocalPlayer())
	{
		if (!m_bSeated)
		{
			// Only do leg ik on local players see B*2209272
			rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
		}

		rPed.SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
		if (camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
		{
			// Will cause ProcessPostPreRenderAfterAttachments to get called in CTaskVehicleMountedWeapon (maybe motion tasks should have an update also?)
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRenderAfterAttachments, true);
		}
	}

	UpdateHeadIK();

	CTurret* pTurret = GetTurret();
	if (pTurret)
	{
		if (!rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			const float fHeadingDelta = ComputeHeadingDelta(pTurret, pTurret->GetCurrentHeading(m_pVehicle), rPed);
			bool bCanUseDesiredAngle = Abs(fHeadingDelta) < GetTuningValues().m_MaxAngleToUseDesiredAngle;

#if KEYBOARD_MOUSE_SUPPORT
			if (rPed.IsInFirstPersonVehicleCamera() || rPed.IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				bCanUseDesiredAngle = true;
			}
#endif // KEYBOARD_MOUSE_SUPPORT

			if (!rPed.IsNetworkClone() && bCanUseDesiredAngle)
			{
				pTurret->GetFlags().SetFlag(CTurret::TF_RenderDesiredAngleForPhysicalTurret);
				pTurret->GetFlags().SetFlag(CTurret::TF_UseDesiredAngleForPitch);
			}

			if (GetState() == State_InitialAdjust)
			{
				pTurret->SetPitchSpeedModifier(GetTuningValues().m_EnterPitchSpeedModifier);

				if (m_bSweeps)
					pTurret->SetHeadingSpeedModifier(GetTuningValues().m_EnterHeadingSpeedModifier);			
			}

			const CTaskVehicleMountedWeapon* pMountedWeaponTask = static_cast<const CTaskVehicleMountedWeapon*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
			if (pMountedWeaponTask && pMountedWeaponTask->IsAimingOrFiring())
			{
				m_bInterruptDelayedMovement = true;
			}

			if (GetTimeRunning() < GetTuningValues().m_DelayMovementDuration && !m_bInterruptDelayedMovement)
			{
				pTurret->GetFlags().SetFlag(CTurret::TF_DisableMovement);
				pTurret->SetTurretSpeedModifier(0.0f);
			}
		}
		else
		{
			m_pVehicle->ActivatePhysics();
			m_pVehicle->ForceNoSleep();	// Need vehicle to be active to process turret
			pTurret->SetPitchSpeedModifier(GetTuningValues().m_ExitPitchSpeedModifier);
		}
	}

	const fwMvClipSetId duckedClipSetId = GetDuckedClipSetId();
	if (duckedClipSetId != CLIP_SET_ID_INVALID)
	{
		CTaskVehicleFSM::RequestVehicleClipSet(duckedClipSetId);
	}

	s32 iPedsSeat = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
	if (m_pVehicle->IsSeatIndexValid(iPedsSeat))
	{
		CTaskVehicleFSM::ProcessAutoCloseDoor(rPed, *m_pVehicle, iPedsSeat);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionInTurret::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	FSM_State(State_Start)
	FSM_OnUpdate
	Start_OnUpdate();

	FSM_State(State_InitialAdjust)
	FSM_OnEnter
	InitialAdjust_OnEnter();
	FSM_OnUpdate
	InitialAdjust_OnUpdate();

	FSM_State(State_StreamAssets)
	FSM_OnEnter
	StreamAssets_OnEnter();
	FSM_OnUpdate
	StreamAssets_OnUpdate();

	FSM_State(State_Idle)
	FSM_OnEnter
	Idle_OnEnter();
	FSM_OnUpdate
	Idle_OnUpdate();

	FSM_State(State_Turning)
	FSM_OnEnter
	Turning_OnEnter();
	FSM_OnUpdate
	Turning_OnUpdate();

	FSM_State(State_Shunt)
	FSM_OnEnter
	Shunt_OnEnter();
	FSM_OnUpdate
	Shunt_OnUpdate();

	FSM_State(State_Finish)
	FSM_OnUpdate
	return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskMotionInTurret::ProcessPostFSM()
{
	CTurret* pTurret = GetTurret();
	if (!pTurret)
		return FSM_Continue;

	CPed& rPed = *GetPed();
	if (m_fTimeDucked < CTaskMotionInAutomobile::ms_Tunables.m_MaxDuckBreakLockOnTime && rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_BreakTargetLock, true);
	}

	m_LastFramesTurretHeading = pTurret->GetLocalHeading(*m_pVehicle);

	const Vector3 vCurrentVehicleVelocity = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(m_pVehicle->GetVelocityIncludingReferenceFrame())));
	Vector3 vAcceleration;
	if (m_pVehicle->IsNetworkClone())
	{
		vAcceleration = NetworkInterface::GetPredictedAccelerationVector(m_pVehicle);
	}
	else
	{
		vAcceleration = (vCurrentVehicleVelocity - m_vLastFramesVehicleVelocity) / GetTimeStep();
	}
	m_vLastFramesVehicleVelocity = vCurrentVehicleVelocity;
	m_vCurrentVehicleAcceleration = vAcceleration;
	return FSM_Continue;
}

bool CTaskMotionInTurret::ProcessPostCamera()
{
	CPed& rPed = *GetPed();
	CTurret* pTurret = GetTurret();
	if (rPed.IsLocalPlayer() && pTurret)		
	{
		camThirdPersonPedAimCamera* thirdPersonPedAimCamera = static_cast<camThirdPersonPedAimCamera*>(camInterface::GetGameplayDirector().GetThirdPersonAimCamera());
		const camControlHelper* controlHelper = thirdPersonPedAimCamera ? thirdPersonPedAimCamera->GetControlHelper() : NULL;
		if(camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
		{
			camCinematicMountedCamera* pTurretCamera = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
			controlHelper = pTurretCamera->GetControlHelper();
		}

		const bool bWasLookingBehind = m_bIsLookingBehind;
		m_bIsLookingBehind = controlHelper && controlHelper->IsLookingBehind();
		if (m_bIsLookingBehind)
		{
			if (!bWasLookingBehind)
			{
				pTurret->SetTurretSpeedModifier(GetTuningValues().m_LookBehindSpeedModifier);
			}
			pTurret->GetFlags().ClearFlag(CTurret::TF_RenderDesiredAngleForPhysicalTurret);
		}

		if (rPed.IsInFirstPersonVehicleCamera(true))
		{
			UpdateMvPitch(pTurret, 0.0f);

			if (m_MoveNetworkHelper.IsNetworkActive())
				m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::PitchId, m_fMvPitch);

			rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		}
	}
	return true;
}

void CTaskMotionInTurret::Start_OnUpdate()
{
	RequestProcessMoveSignalCalls();

	CPed& rPed = *GetPed();
	if (ShouldSetAircraftMode(rPed))
	{
		CRenderPhaseCascadeShadowsInterface::Script_SetAircraftMode(true);
	}
	
	if (!StartMoveNetwork(m_MoveNetworkHelper, rPed))
		return;

	const CTurret* pTurret = GetTurret();
	const CVehicleSeatInfo* pSeatInfo = m_pVehicle->GetSeatInfo(&rPed);
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(&rPed);

	m_bSeated = pSeatClipInfo && pSeatClipInfo->GetInVehicleMoveNetwork() == CVehicleSeatAnimInfo::VEHICLE_TURRET_SEATED;
	bool bUseSweepLoops = m_bSeated && pSeatInfo && pSeatInfo->GetUseSweepLoopsForTurret();
	m_bSweeps = m_bSeated && pSeatInfo && (pSeatInfo->GetUseSweepsForTurret() || bUseSweepLoops);

	if (bUseSweepLoops)
	{
		m_MoveNetworkHelper.SetFlag(m_bSeated, TaskMotionInTurretMoveNetworkMetadata::Seated_SweepLoopsFlagId);
	}
	else
	{
		m_MoveNetworkHelper.SetFlag(m_bSeated, m_bSweeps ? TaskMotionInTurretMoveNetworkMetadata::Seated_SweepsFlagId : TaskMotionInTurretMoveNetworkMetadata::Seated_LoopsFlagId);
	}

	m_fInitialTurretHeading = pTurret ? pTurret->GetCurrentHeading(m_pVehicle) : 0.0f;
	m_LastFramesTurretHeading = m_fInitialTurretHeading;
	m_fInitialHeadingDelta = ComputeHeadingDelta(pTurret, m_fInitialTurretHeading, rPed);

	const float fTimeStep = fwTimer::GetTimeStep();
	UpdateAngVelocityAndAcceleration(pTurret, fTimeStep);
	UpdateMvTurnSpeed(fTimeStep);
	UpdateMvPitch(pTurret, fTimeStep);
	UpdateMvHeading(pTurret, rPed);
	SendMvParams();

	if (AssetsHaveBeenStreamed(&rPed, m_pVehicle))
	{
		SetInitialState();
		return;
	}
	else
	{
		SetState(State_StreamAssets);
		return;
	}
}

void CTaskMotionInTurret::InitialAdjust_OnEnter()
{
	if (!m_bSeated)
	{
		m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::AdjustStepRequestId);
		atArray<const crClip*> aClips;
		if (GetAdjustStepClips(aClips))
		{
			m_MoveNetworkHelper.SetClip(aClips[0], TaskMotionInTurretMoveNetworkMetadata::Clip0Id);
			m_MoveNetworkHelper.SetClip(aClips[1], TaskMotionInTurretMoveNetworkMetadata::Clip1Id);
			m_MoveNetworkHelper.SetClip(aClips[2], TaskMotionInTurretMoveNetworkMetadata::Clip2Id);
		}
		m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::AdjustStep_OnEnterId);
		m_bCanBlendOutOfCurrentAnimState = false;
	}
	else
	{
		m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::IdleRequestId);
		m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::Idle_OnEnterId);
		m_bCanBlendOutOfCurrentAnimState = true;
	}
}

void CTaskMotionInTurret::InitialAdjust_OnUpdate()
{
	const float fInitialHeadingDelta = Abs(m_fInitialHeadingDelta / PI);
	m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::InitialHeadingDeltaId, fInitialHeadingDelta);

	if (!m_MoveNetworkHelper.IsInTargetState())
		return;

	bool bCanTransitionToIdle = false;
	CTurret* pTurret = GetTurret();
	if (m_bSeated && pTurret)
	{
		pTurret->GetFlags().SetFlag(CTurret::TF_InitialAdjust);
		if (!pTurret->GetFlags().IsFlagSet(CTurret::TF_DisableMovement))
		{
			CPed& rPed = *GetPed();
			const float fCurrentPitch = pTurret->GetCurrentPitch();
			const float fDesiredPitch = pTurret->GetDesiredPitch();
			const float fDeltaPitch = fwAngle::LimitRadianAngle(fCurrentPitch - fDesiredPitch);
			const float fCurrentHeading = pTurret->GetCurrentHeading(m_pVehicle);
			const float fDeltaHeading = ComputeHeadingDelta(pTurret, fCurrentHeading, rPed);
			if (Abs(fDeltaHeading) < GetTuningValues().m_InitialAdjustFinishedHeadingTolerance && Abs(fDeltaPitch) < GetTuningValues().m_InitialAdjustFinishedPitchTolerance)
			{
				bCanTransitionToIdle = true;
			}
		}
	}
	else
	{
		bCanTransitionToIdle = m_bCanBlendOutOfCurrentAnimState;
	}

	if (bCanTransitionToIdle)
	{
		SetState(State_Idle);
		return;
	}
}

void CTaskMotionInTurret::StreamAssets_OnEnter()
{
	// Sync move
	m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::StreamAssetsRequestId);

	const CPed* pPed = GetPed();
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	// Set the move network clipset based on the seat info and make sure we set
	// the low lod animation while assets are being streamed
	if(pSeatClipInfo)
	{
		// Get the low lod clip for this vehicle from the low lod clipset
		static fwMvClipSetId lowLodIdleClipSetId("clipset@heists@veh@lo_res_idles", 0xfdb8e5d3);
		const fwMvClipId lowLodClipId = pSeatClipInfo->GetLowLODIdleAnim();
		const crClip* pLowLodClip = fwClipSetManager::GetClip( lowLodIdleClipSetId, lowLodClipId );
		// Set the input clip in move to play this clip
		// while animations are being streamed in
		m_MoveNetworkHelper.SetClip(pLowLodClip, TaskMotionInTurretMoveNetworkMetadata::LowLodTurretClipId);
	}

	// Wait for move to be in sync
	m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::StreamAssets_OnEnterId);
}

void CTaskMotionInTurret::StreamAssets_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
		return;

	const CPed* pPed = GetPed();
	if (AssetsHaveBeenStreamed(pPed, m_pVehicle))
	{
		SetInitialState();
		return;
	}
}

void CTaskMotionInTurret::Idle_OnEnter()
{
	bool bBlockIdleRequest = false;
	if (GetPreviousState() == State_InitialAdjust && m_bSeated)
	{
		bBlockIdleRequest = true;
	}

	if (!bBlockIdleRequest && (HasTurnAnims() || (GetPreviousState() != State_Turning)))
	{
		m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::IdleRequestId);
		m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::Idle_OnEnterId);
	}

	const bool bTurningLeft = !m_bTurningLeft;	// B*3530895, BlairT: invert the wobble flag for the MoVE network as wobble anims are the wrong way around. Simpler than updating the MoVE network!
	m_MoveNetworkHelper.SetFlag(bTurningLeft, TaskMotionInTurretMoveNetworkMetadata::WasTurningLeftFlagId);
	m_MoveNetworkHelper.SetClipSet(GetAppropriateClipSetId());

	if (GetPreviousState() == State_Idle)
	{
		m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::RestartIdleBlendDurationId, GetTuningValues().m_RestartIdleBlendDuration);
	}
}

void CTaskMotionInTurret::Idle_OnUpdate()
{
	ProcessFiring();

	if (GetTimeInState() > 0.1f && !IsDucking())
	{
		m_MoveNetworkHelper.SetFlag(false, TaskMotionInTurretMoveNetworkMetadata::WobbleFlagId);
	}

	if (!m_MoveNetworkHelper.IsInTargetState())
		return;

	if (ShouldRestartStateBecauseDuckingStateChange())
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return;
	}

	const float fMinTimeInState = GetPed()->IsLocalPlayer() ? GetTuningValues().m_MinTimeInIdleState : GetTuningValues().m_MinTimeInIdleStateAiOrRemote;
	if (GetTimeInState() > fMinTimeInState)
	{
		bool bRestart = false;

		CPed& rPed = *GetPed();
		if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, rPed, m_vCurrentVehicleAcceleration.z))
		{
			SetState(State_Shunt);
			return;
		}
		else if (CheckForTurning(bRestart) && !rPed.IsInFirstPersonVehicleCamera(true))
		{
			SetState(State_Turning);
			return;
		}
	}
}

void CTaskMotionInTurret::Turning_OnEnter()
{
	if (HasTurnAnims())
	{
		m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::TurningRequestId);
		m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::Turning_OnEnterId);
		atArray<const crClip*> aClips;
		if (GetTurningClips(aClips))
		{
			m_MoveNetworkHelper.SetClip(aClips[0], TaskMotionInTurretMoveNetworkMetadata::Clip0Id);
			m_MoveNetworkHelper.SetClip(aClips[1], TaskMotionInTurretMoveNetworkMetadata::Clip1Id);
			m_MoveNetworkHelper.SetClip(aClips[2], TaskMotionInTurretMoveNetworkMetadata::Clip2Id);
			m_MoveNetworkHelper.SetClip(aClips[3], TaskMotionInTurretMoveNetworkMetadata::Clip3Id);
			m_MoveNetworkHelper.SetClip(aClips[4], TaskMotionInTurretMoveNetworkMetadata::Clip4Id);
			m_MoveNetworkHelper.SetClip(aClips[5], TaskMotionInTurretMoveNetworkMetadata::Clip5Id);
		}
	}
	else if (GetPreviousState() == State_Turning)
	{
		m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::IdleRequestId);
		m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::Idle_OnEnterId);
		m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::RestartIdleBlendDurationId, GetTuningValues().m_RestartIdleBlendDuration);
		m_MoveNetworkHelper.SetClipSet(GetAppropriateClipSetId());
	}
	m_MoveNetworkHelper.SetFlag(false, TaskMotionInTurretMoveNetworkMetadata::WobbleFlagId);
}

void CTaskMotionInTurret::Turning_OnUpdate()
{
	ProcessFiring();

	if (!m_MoveNetworkHelper.IsInTargetState())
		return;

	if (ShouldRestartStateBecauseDuckingStateChange())
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return;
	}

	CPed& rPed = *GetPed();
	const float fMinTimeInState = rPed.IsLocalPlayer() ? GetTuningValues().m_MinTimeInTurnState : GetTuningValues().m_MinTimeInTurnStateAiOrRemote;
	if (GetTimeInState() > fMinTimeInState)
	{
		bool bRestart = false;
		if (CheckForTurning(bRestart))
		{
			if (bRestart)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return;
			}
		}
		else
		{
			if (GetTimeInState() > GetTuningValues().m_MinTimeTurningForWobble && !IsDucking())
			{
				m_MoveNetworkHelper.SetFlag(true, TaskMotionInTurretMoveNetworkMetadata::WobbleFlagId);
			}
			SetState(State_Idle);
			return;
		}
	}
}

void CTaskMotionInTurret::Shunt_OnEnter()
{
	m_MoveNetworkHelper.SendRequest(TaskMotionInTurretMoveNetworkMetadata::ShuntRequestId);
	m_MoveNetworkHelper.WaitForTargetState(TaskMotionInTurretMoveNetworkMetadata::Shunt_OnEnterId);

	const fwMvClipId clipId = GetShuntClip();
	const fwMvClipSetId clipsetId = m_MoveNetworkHelper.GetClipSetId();
	const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
	m_MoveNetworkHelper.SetClip(pClip, TaskMotionInTurretMoveNetworkMetadata::Clip0Id);
}

void CTaskMotionInTurret::Shunt_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
		return;

	if (m_MoveNetworkHelper.GetBoolean(TaskMotionInTurretMoveNetworkMetadata::BlendOutShuntAnimId))
	{
		SetState(State_Idle);
		return;
	}
}

bool CTaskMotionInTurret::StartMoveNetwork(CMoveNetworkHelper& rMoveNetworkHelper, const CPed& rPed)
{
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(&rPed);
	const fwMvClipSetId clipSetId = pSeatClipInfo->GetSeatClipSetId();
	if (!CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(clipSetId))
		return false;

	const fwMvNetworkDefId networkDefId = TaskMotionInTurretMoveNetworkMetadata::NetworkId;
	rMoveNetworkHelper.RequestNetworkDef(networkDefId);

	if (rMoveNetworkHelper.IsNetworkActive())
		rMoveNetworkHelper.ReleaseNetworkPlayer();

	rMoveNetworkHelper.CreateNetworkPlayer(&rPed, networkDefId);

	if (pSeatClipInfo)
		rMoveNetworkHelper.SetClipSet(clipSetId);

	rMoveNetworkHelper.DeferredInsert();
	return true;
}

void CTaskMotionInTurret::ProcessFiring()
{
	CPed& rPed = *GetPed();

	const CTaskVehicleMountedWeapon* pMountedWeaponTask = static_cast<const CTaskVehicleMountedWeapon*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
	if (pMountedWeaponTask)
	{
		m_bFiring = rPed.IsAPlayerPed() ? pMountedWeaponTask->IsFiring() : pMountedWeaponTask->FiredThisFrame();

		// Don't class as firing if the weapon is spinning up
		const CVehicleWeaponMgr* pVehWeaponMgr = m_pVehicle->GetVehicleWeaponMgr();
		const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());

		if (pVehWeaponMgr && m_pVehicle->IsSeatIndexValid(iSeatIndex))
		{
			const CFixedVehicleWeapon* pFixedVehicleWeapon = pVehWeaponMgr->GetFirstFixedWeaponForSeat(iSeatIndex);
			if (pFixedVehicleWeapon)
			{
				CWeapon* pWeapon = pFixedVehicleWeapon->GetWeapon();
				if (pWeapon && ((pWeapon->GetState() == CWeapon::STATE_SPINNING_UP) || (pWeapon->GetState() == CWeapon::STATE_SPINNING_DOWN) 
					|| (!pWeapon->GetNeedsToSpinDown() && pWeapon->GetState() == CWeapon::STATE_READY)))
				{
					m_bFiring = false;
				}
			}
			
			// Also don't send the signal to play the fire animation for projectile turrets
			const CVehicleWeapon* pVehicleWeapon = rPed.GetWeaponManager()->GetEquippedVehicleWeapon();
			if (pVehicleWeapon && pVehicleWeapon->GetWeaponInfo() && pVehicleWeapon->GetWeaponInfo()->GetIsProjectile())
			{
				m_bFiring = false;
			}

			// Also prevent on the MOGUL, as the handles and guns aren't connected so it looks kinda crappy
			if (MI_PLANE_MOGUL.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_MOGUL)
			{
				m_bFiring = false;
			}

			// Same with the BARRAGE, but only in first person (third person looks okay)
			if (IsInFirstPersonCamera() && MI_CAR_BARRAGE.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_BARRAGE)
			{
				m_bFiring = false;
			}
			
		}

		if (rPed.IsLocalPlayer() && pMountedWeaponTask->ShouldDimReticule())
		{
			m_bFiring = false;
		}
	}

	m_MoveNetworkHelper.SetFlag(m_bFiring, TaskMotionInTurretMoveNetworkMetadata::FireFlagId);
}

// Consider introducing NULL object
CTurret* CTaskMotionInTurret::GetTurret()
{
	if (!m_pVehicle)
		return NULL;

	if (!m_pVehicle->GetVehicleWeaponMgr())
		return NULL;

	const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
	if (!m_pVehicle->IsSeatIndexValid(iSeatIndex))
		return NULL;

	// Maybe cache this?
	return m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(iSeatIndex);
}

bool CTaskMotionInTurret::CheckForTurning(bool& bRestart)
{
	CTurret* pTurret = GetTurret();
	if (!pTurret)
		return false;

	const CPed& rPed = *GetPed();
	float fLeftRight;
	float fTurnThreshold;
	bool bIsTurningLeft;

	if (m_pVehicle->GetVelocity().Mag2() < SMALL_FLOAT)
	{
		if (rPed.IsLocalPlayer() && !IsPlayerTryingToTurn(rPed))
		{
			return false;
		}
	}

	if (Abs(m_fAngVelocity) < GetTuningValues().m_TurnVelocityThreshold)
	{
		const float fTurnDecelerationThreshold = GetTuningValues().m_TurnDecelerationThreshold;
		if ((m_bTurningLeft && m_fAngAcceleration > fTurnDecelerationThreshold) || (!m_bTurningLeft && m_fAngAcceleration < -fTurnDecelerationThreshold))
		{
			return false;
		}
	}

	fLeftRight = m_fMvTurnSpeed;
	fTurnThreshold = rPed.IsLocalPlayer() ? GetTuningValues().m_PlayerTurnControlThreshold : GetTuningValues().m_RemoteOrAiTurnControlThreshold;
	bIsTurningLeft = m_fAngVelocity < 0.0f ? true : false;

	if (Abs(m_fAngVelocity) > fTurnThreshold)
	{
		if (m_bTurningLeft != bIsTurningLeft)
		{
			m_bTurningLeft = bIsTurningLeft;
			bRestart = true;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskMotionInTurret::HasTurnAnims() const
{
	return !m_bSeated && !m_bSweeps;
}

bool CTaskMotionInTurret::ShouldRestartStateBecauseDuckingStateChange()
{
	if (CanBeDucked())
	{
		const bool bWantsToDuck = WantsToDuck();

		if (bWantsToDuck)
		{
			m_fTimeDucked += GetTimeStep();
		}
		else
		{
			m_fTimeDucked = 0.0f;
		}

		if (bWantsToDuck != m_bIsDucking)
		{
			CPed& rPed = *GetPed();
			rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle, bWantsToDuck);
			m_bIsDucking = bWantsToDuck;
			return true;
		}
	}
	return false;
}

void CTaskMotionInTurret::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	if (taskVerifyf(m_pVehicle, "NULL vehicle in CTaskMotionInAutomobile::GetNMBlendOutClip"))
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
		if (pSeatClipInfo)
		{
			outClipSet = pSeatClipInfo->GetSeatClipSetId();
			outClip = CLIP_IDLE;
		}
	}
}

Vec3V_Out CTaskMotionInTurret::CalcDesiredVelocity(Mat34V_ConstRef UNUSED_PARAM(updatedPedMatrix),float UNUSED_PARAM(fTimestep))
{
	return VECTOR3_TO_VEC3V(GetPed()->GetVelocity());
}

CTask * CTaskMotionInTurret::CreatePlayerControlTask()
{
	taskFatalAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_VEHICLE, "Invalid parent task");
	return static_cast<CTaskMotionInVehicle*>(GetParent())->CreatePlayerControlTask();
}

void CTaskMotionInTurret::IkHandsToTurret( CPed &rPed, const CVehicle& rVeh, const CTurret& rTurret )
{
	const crSkeletonData& rSkelData = rVeh.GetSkeletonData();
	TUNE_GROUP_FLOAT(TURRET_TUNE, BLEND_DURATION, 0.25f, 0.0f, 1.0f, 0.01f);
	CTaskVehicleFSM::IkHandToTurret(rPed, rVeh, rSkelData, true, BLEND_DURATION, rTurret);
	CTaskVehicleFSM::IkHandToTurret(rPed, rVeh, rSkelData, false, BLEND_DURATION, rTurret);
}

bool CTaskMotionInTurret::GetCanFire() const
{
	if (GetState() == State_InitialAdjust)
	{
		TUNE_GROUP_FLOAT(TURRET_TUNE, CAN_FIRE_PHASE, 0.5f, 0.0f, 1.0f, 0.01f);

		fwMvFloatId phaseToUseId = GetPhaseIdForAdjustStep();
		const float fAdjustStepPhase = Clamp(m_MoveNetworkHelper.GetFloat(phaseToUseId), 0.0f, 1.0f);
		return fAdjustStepPhase > CAN_FIRE_PHASE;
	}
	return true;
}

float CTaskMotionInTurret::GetAdjustStepMoveRatio() const
{
	TUNE_GROUP_FLOAT(TURRET_TUNE, PHASE_START_MOVING, 0.211f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, PHASE_STOP_MOVING, 0.579f, 0.0f, 1.0f, 0.01f);

	float fStartMovingPhase = PHASE_START_MOVING;
	float fStopMovingPhase = PHASE_STOP_MOVING;

	//const float fInitialHeadingDelta = Abs(m_fInitialHeadingDelta / PI);
	//aiDisplayf("fInitialHeadingDelta = %.2f", fInitialHeadingDelta);
	const u32 uClipToUse = GetDominantClipForAdjustStep();
	const fwMvFloatId phaseToUseId = GetPhaseIdForAdjustStep();

	atArray<const crClip*> aClips;
	if (GetAdjustStepClips(aClips))
	{
		CClipEventTags::FindEventPhase<crPropertyAttributeBool>(aClips[uClipToUse], CClipEventTags::Door, CClipEventTags::Start, true, fStartMovingPhase);
		CClipEventTags::FindEventPhase<crPropertyAttributeBool>(aClips[uClipToUse], CClipEventTags::Door, CClipEventTags::Start, false, fStopMovingPhase);
	}

	const float fAdjustStepPhase = Clamp(m_MoveNetworkHelper.GetFloat(phaseToUseId), 0.0f, 1.0f);
	return Clamp((fAdjustStepPhase - fStartMovingPhase) / (fStopMovingPhase - fStartMovingPhase), 0.0f, 1.0f);
}

bool CTaskMotionInTurret::ShouldHaveControlOfTurret() const
{
	if (GetState() != State_InitialAdjust)
	{
		return true;
	}

	TUNE_GROUP_FLOAT(TURRET_TUNE, SHOULD_HAVE_CONTROL_MOVE_RATIO, 0.0f, 0.0f, 1.0f, 0.01f);
	const float fAdjustStepMoveRatio = GetAdjustStepMoveRatio();
	if (fAdjustStepMoveRatio > SHOULD_HAVE_CONTROL_MOVE_RATIO)
	{
		return true;
	}
	return false;
}

void CTaskMotionInTurret::KeepTurretHatchOpen(const s32 iSeatIndex, CVehicle& rVeh)
{
	const s32 iEntryPointIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, &rVeh);
	if (!rVeh.IsEntryIndexValid(iEntryPointIndex))
		return;

	const s32 iDoorBoneIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);
	CCarDoor* pDoor = rVeh.GetDoorFromBoneIndex(iDoorBoneIndex);
	if (!pDoor)
		return;

	pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SPECIAL|CCarDoor::DRIVEN_USE_STIFFNESS_MULT);
}

fwMvClipId CTaskMotionInTurret::GetShuntClip()
{
	fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();
	if (m_fMvPitch < GetTuningValues().m_MaxMvPitchToPlayAimDownShunt && fwClipSetManager::GetClip(clipSetId, GetTuningValues().m_ShuntDownClipId))
	{
		return GetTuningValues().m_ShuntDownClipId;
	}
	
	if (m_fMvPitch > GetTuningValues().m_MinMvPitchToPlayAimUpShunt && fwClipSetManager::GetClip(clipSetId, GetTuningValues().m_ShuntUpClipId))
	{
		return GetTuningValues().m_ShuntUpClipId;
	}
	
	Vec3V vPedFwd = GetPed()->GetTransform().GetB();
	Vec3V vCollNormal = -vPedFwd;

	const CCollisionHistory* pCollisionHistory = m_pVehicle->GetFrameCollisionHistory();
	const CCollisionRecord* pRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
	if (pRecord)
	{
		vCollNormal = RCC_VEC3V(pRecord->m_MyCollisionNormal);
#if DEBUG_DRAW
		Vec3V vVehPos = m_pVehicle->GetTransform().GetPosition() + Vec3V(V_Z_AXIS_WZERO);
		CTask::ms_debugDraw.AddArrow(vVehPos, vVehPos + vCollNormal, 0.025f, Color_red, 2000);
		CTask::ms_debugDraw.AddArrow(vVehPos, vVehPos + vPedFwd, 0.025f, Color_blue, 2000);
#endif // DEBUG_DRAW
	}

	const float fCollisionDot = Dot(vCollNormal, vPedFwd).Getf();

	if (fCollisionDot > 0.707f) // < 45o
	{
		return GetTuningValues().m_ShuntBackClipId;
	}
	else if (fCollisionDot > -0.707f) // < 135o
	{
		const float fCrossZ = Cross(vCollNormal, vPedFwd).GetZf();
		if (fCrossZ < 0.0f)
		{
			return GetTuningValues().m_ShuntRightClipId;
		}
		else
		{
			return GetTuningValues().m_ShuntLeftClipId;
		}
	}
	else // >= 135o
	{
		return GetTuningValues().m_ShuntFrontClipId;
	}
}

bool CTaskMotionInTurret::GetAdjustStepClips(atArray<const crClip*>& rClipArray) const
{
	const bool bAdjustLeft = Sign(m_fInitialHeadingDelta) > 0.0f ? true : false;
	const fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);
	const fwMvClipId clip0Id = bAdjustLeft ? GetTuningValues().m_AdjustStep0LClipId : GetTuningValues().m_AdjustStep0RClipId;
	const fwMvClipId clip1Id = bAdjustLeft ? GetTuningValues().m_AdjustStep90LClipId : GetTuningValues().m_AdjustStep90RClipId;
	const fwMvClipId clip2Id = bAdjustLeft ? GetTuningValues().m_AdjustStep180LClipId : GetTuningValues().m_AdjustStep180RClipId;
	taskAssert(clip0Id != CLIP_ID_INVALID);
	taskAssert(clip1Id != CLIP_ID_INVALID);
	taskAssert(clip2Id != CLIP_ID_INVALID);
	const crClip* pClip0 = fwClipSetManager::GetClip(clipSetId, clip0Id);
	const crClip* pClip1 = fwClipSetManager::GetClip(clipSetId, clip1Id);
	const crClip* pClip2 = fwClipSetManager::GetClip(clipSetId, clip2Id);
	rClipArray.PushAndGrow(pClip0);
	rClipArray.PushAndGrow(pClip1);
	rClipArray.PushAndGrow(pClip2);
	return rClipArray.GetCount() == 3 ? true : false;
}

bool CTaskMotionInTurret::GetTurningClips(atArray<const crClip*>& rClipArray) const
{
	const fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);
	const fwMvClipId clip0Id = m_bTurningLeft ? GetTuningValues().m_TurnLeftSlowDownClipId : GetTuningValues().m_TurnRightSlowDownClipId;
	const fwMvClipId clip1Id = m_bTurningLeft ? GetTuningValues().m_TurnLeftFastDownClipId : GetTuningValues().m_TurnRightFastDownClipId;
	const fwMvClipId clip2Id = m_bTurningLeft ? GetTuningValues().m_TurnLeftSlowClipId : GetTuningValues().m_TurnRightSlowClipId;
	const fwMvClipId clip3Id = m_bTurningLeft ? GetTuningValues().m_TurnLeftFastClipId : GetTuningValues().m_TurnRightFastClipId;
	const fwMvClipId clip4Id = m_bTurningLeft ? GetTuningValues().m_TurnLeftSlowUpClipId : GetTuningValues().m_TurnRightSlowUpClipId;
	const fwMvClipId clip5Id = m_bTurningLeft ? GetTuningValues().m_TurnLeftFastUpClipId : GetTuningValues().m_TurnRightFastUpClipId;
	taskAssert(clip0Id != CLIP_ID_INVALID);
	taskAssert(clip1Id != CLIP_ID_INVALID);
	taskAssert(clip2Id != CLIP_ID_INVALID);
	taskAssert(clip3Id != CLIP_ID_INVALID);
	taskAssert(clip4Id != CLIP_ID_INVALID);
	taskAssert(clip5Id != CLIP_ID_INVALID);
	const crClip* pClip0 = fwClipSetManager::GetClip(clipSetId, clip0Id);
	const crClip* pClip1 = fwClipSetManager::GetClip(clipSetId, clip1Id);
	const crClip* pClip2 = fwClipSetManager::GetClip(clipSetId, clip2Id);
	const crClip* pClip3 = fwClipSetManager::GetClip(clipSetId, clip3Id);
	const crClip* pClip4 = fwClipSetManager::GetClip(clipSetId, clip4Id);
	const crClip* pClip5 = fwClipSetManager::GetClip(clipSetId, clip5Id);
	rClipArray.PushAndGrow(pClip0);
	rClipArray.PushAndGrow(pClip1);
	rClipArray.PushAndGrow(pClip2);
	rClipArray.PushAndGrow(pClip3);
	rClipArray.PushAndGrow(pClip4);
	rClipArray.PushAndGrow(pClip5);
	return rClipArray.GetCount() == 6 ? true : false;
}

fwMvFloatId CTaskMotionInTurret::GetPhaseIdForAdjustStep() const
{
	fwMvFloatId phaseToUseId = TaskMotionInTurretMoveNetworkMetadata::AdjustStep0PhaseId;
	const u32 uDominantClip = GetDominantClipForAdjustStep();
	if (uDominantClip == 1)
	{
		phaseToUseId = TaskMotionInTurretMoveNetworkMetadata::AdjustStep90PhaseId;
	}
	else if (uDominantClip == 2)
	{
		phaseToUseId = TaskMotionInTurretMoveNetworkMetadata::AdjustStep180PhaseId;
	}
	return phaseToUseId;
}

u32 CTaskMotionInTurret::GetDominantClipForAdjustStep() const
{
	const float fInitialHeadingDelta = Abs(m_fInitialHeadingDelta / PI);
	u32 uClipToUse = 0;
	if (fInitialHeadingDelta > 0.25 && fInitialHeadingDelta < 0.75f)
	{
		uClipToUse = 1;
	}
	else if (fInitialHeadingDelta > 0.75f)
	{
		uClipToUse = 2;
	}
	return uClipToUse;
}

float CTaskMotionInTurret::ComputeHeadingDelta(const CTurret* pTurret, float fTurretHeading, CPed &rPed) const
{
	if (!pTurret)
		return 0.0f;

	float fCamHeading = 0.0f;
	if (rPed.IsNetworkClone() || !rPed.IsPlayer())
	{
		const CEntity* pTarget = NULL;
		Vector3 vTargetPos;
		const CTaskVehicleMountedWeapon* pMountedTask = static_cast<const CTaskVehicleMountedWeapon*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
		if (pMountedTask && pMountedTask->GetTargetInfo(&rPed, vTargetPos, pTarget))
		{
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
			fCamHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPedPos.x, vPedPos.y);
		}
		else
		{
			return 0.0f;
		}
	}
	else
	{
		fCamHeading = camInterface::GetGameplayDirector().GetFrame().ComputeHeading();
		if(camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
		{
			camCinematicMountedCamera* pTurretCamera = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
			if(pTurretCamera->IsVehicleTurretCamera())
			{
				fCamHeading = pTurretCamera->GetFrame().ComputeHeading();
			}
		}
	}
	return fwAngle::LimitRadianAngle(fTurretHeading - fCamHeading);
}

bool CTaskMotionInTurret::HasValidWeapon(const CPed& BANK_ONLY(rPed), const CVehicle& rVeh)
{
	const CVehicleWeaponMgr* pWeaponMgr = rVeh.GetVehicleWeaponMgr();
	if (!pWeaponMgr)
	{
		BANK_ONLY(aiDebugf1("Frame %u, ped %s HasValidWeapon false : NULL pWeaponMgr", fwTimer::GetFrameCount(), rPed.GetModelName());)
		return false;
	}

	if (pWeaponMgr->GetNumTurrets() == 0)
	{
		BANK_ONLY(aiDebugf1("Frame %u, ped %s HasValidWeapon false : no turrets", fwTimer::GetFrameCount(), rPed.GetModelName());)
		return false;
	}

	return true;
}

bool CTaskMotionInTurret::IsSeatWithHatchProtection(s32 iSeatIndex, const CVehicle& rVeh)
{
	if (rVeh.GetIsAircraft())
		return false;

	if (!rVeh.IsSeatIndexValid(iSeatIndex))
		return false;

	if (!rVeh.IsTurretSeat(iSeatIndex))
		return false;

	if (rVeh.GetSeatAnimationInfo(iSeatIndex)->IsStandingTurretSeat())
		return false;

	return true;
}

bool CTaskMotionInTurret::ShouldUseAlternateJackDueToTurretOrientation(CVehicle& rVeh, bool bLeftEntry)
{
	const CVehicleWeaponMgr* pVehWeaponMgr = rVeh.GetVehicleWeaponMgr();
	if (!pVehWeaponMgr)
		return false;

	const CTurret* pTurret = pVehWeaponMgr->GetTurret(0);
	if (!pTurret)
		return false;

	const float fLocalHeading = pTurret->GetLocalHeading(rVeh);
	TUNE_GROUP_FLOAT(TURRET_TUNE, USE_ALTERNATE_JACK_LEFT_LOCAL_ANGLE_MIN, -0.4f, -PI, PI, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, USE_ALTERNATE_JACK_LEFT_LOCAL_ANGLE_MAX, 0.6f, -PI, PI, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, USE_ALTERNATE_JACK_RIGHT_LOCAL_ANGLE_MIN, -0.6f, -PI, PI, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, USE_ALTERNATE_JACK_RIGHT_LOCAL_ANGLE_MAX, 0.4f, -PI, PI, 0.01f);

	const float fLocalHeadingForRear = fwAngle::LimitRadianAngle(fLocalHeading + PI);
	const float fMinAngle = bLeftEntry ? USE_ALTERNATE_JACK_LEFT_LOCAL_ANGLE_MIN : USE_ALTERNATE_JACK_RIGHT_LOCAL_ANGLE_MIN;
	const float fMaxAngle = bLeftEntry ? USE_ALTERNATE_JACK_LEFT_LOCAL_ANGLE_MAX : USE_ALTERNATE_JACK_RIGHT_LOCAL_ANGLE_MAX;
	if (fLocalHeadingForRear > fMinAngle && fLocalHeadingForRear < fMaxAngle)
	{
		return true;
	}
	return false;
}

void CTaskMotionInTurret::UpdateMvPitch(const CTurret* pTurret, const float fTimeStep)
{
	const float fMinPitch = GetTuningValues().m_MinAnimPitch;
	const float fMaxPitch = GetTuningValues().m_MaxAnimPitch;
	const float fCurrentPitch = pTurret ? pTurret->GetCurrentPitch() : 0.0f;
	const float fPitch = rage::Clamp((fCurrentPitch - fMinPitch) / (fMaxPitch - fMinPitch), 0.0f, 1.0f);
	if (camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera() && pTurret)
	{
		const float fDesiredPitch = pTurret->GetDesiredPitch();
		const float fDesiredPitchClamped = rage::Clamp((fDesiredPitch - fMinPitch) / (fMaxPitch - fMinPitch), 0.0f, 1.0f);
		m_fMvPitch = fDesiredPitchClamped;
	}
	else
	{
		Approach(m_fMvPitch, fPitch, GetTuningValues().m_PitchApproachRate, fTimeStep);
	}
}

void CTaskMotionInTurret::UpdateMvHeading(const CTurret* pTurret, CPed& rPed)
{
	if (!m_bSweeps)
		return;

	if (!pTurret)
		return;

	const CVehicleDriveByInfo* pDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&rPed);
	if (!pDriveByInfo)
		return;

	const float fMinHeadingAngle = pDriveByInfo->GetMinAimSweepHeadingAngleDegs(&rPed) * DtoR;
	const float fMaxHeadingAngle = pDriveByInfo->GetMaxAimSweepHeadingAngleDegs(&rPed) * DtoR;

	// Compute current yaw angle value
	float fDesiredHeading = pTurret->GetAimHeading();

	// Map the desired target heading to 0-1, depending on the range of the sweep
	fDesiredHeading = 1.0f - CTaskAimGun::ConvertRadianToSignal(fDesiredHeading, fMinHeadingAngle, fMaxHeadingAngle, false);

	// The desired heading is smoothed so it doesn't jump too much in a timestep
	if (m_fMvHeading < 0.0f)
	{
		m_fMvHeading = fDesiredHeading;
	}
	else
	{
		m_fMvHeading = CTaskAimGun::SmoothInput(m_fMvHeading, fDesiredHeading, GetTuningValues().m_SweepHeadingChangeRate);
	}
}

void CTaskMotionInTurret::UpdateMvTurnSpeed(const float fTimeStep)
{
	TUNE_GROUP_FLOAT(TURRET_TUNE, MAX_ANG_VELOCITY, 2.0f, 0.0f, TWO_PI, 0.01f);
	const float fAngVelocityNorm = Clamp(Abs(m_fAngVelocity) / MAX_ANG_VELOCITY, 0.0f, 1.0f);
	Approach(m_fMvTurnSpeed, fAngVelocityNorm, GetTuningValues().m_TurnSpeedApproachRate, fTimeStep);
}

void CTaskMotionInTurret::UpdateAngVelocityAndAcceleration(const CTurret* pTurret, const float fTimeStep)
{
	const float fPrevAngVelocity = m_fAngVelocity;
	TUNE_GROUP_FLOAT(TURRET_TUNE, MIN_HEADING_DELTA_STILL, 0.005f, 0.0f, TWO_PI, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, MIN_HEADING_DELTA_MOVING, 0.025f, 0.0f, TWO_PI, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, CONSIDERED_MOVING_VELOCITY, 0.5f, 0.0f, 10.0f, 0.01f);
	const float fThisFramesLocalHeading = pTurret ? pTurret->GetLocalHeading(*m_pVehicle) : 0.0f;
	const float fMinDelta = m_pVehicle->GetVelocity().Mag2() > square(CONSIDERED_MOVING_VELOCITY) ? MIN_HEADING_DELTA_MOVING : MIN_HEADING_DELTA_STILL;
	const float fDelta = fwAngle::LimitRadianAngle(fThisFramesLocalHeading - m_LastFramesTurretHeading);
	m_fAngVelocity = fTimeStep > SMALL_FLOAT && Abs(fDelta) > fMinDelta ? (fDelta / fTimeStep) : 0.0f;
	m_fAngAcceleration = fTimeStep > SMALL_FLOAT ? (m_fAngVelocity - fPrevAngVelocity) / fTimeStep : 0.0f;
}

void CTaskMotionInTurret::SendMvParams()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::PitchId, m_fMvPitch);
		m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::TurnSpeedId, m_fMvTurnSpeed);
		m_MoveNetworkHelper.SetFloat(TaskMotionInTurretMoveNetworkMetadata::HeadingId, m_fMvHeading);
	}
}

bool CTaskMotionInTurret::SetSeatClipSet(const CVehicleSeatAnimInfo* pSeatClipInfo)
{
	if (!taskVerifyf(pSeatClipInfo, "NULL Seat Clip Info"))
		return false;

	fwMvClipSetId seatClipSetId = pSeatClipInfo->GetSeatClipSetId();
	if (m_MoveNetworkHelper.GetClipSetId() == CLIP_SET_ID_INVALID)
	{
		m_MoveNetworkHelper.SetClipSet(seatClipSetId);
	}
	return true;
}

bool CTaskMotionInTurret::AssetsHaveBeenStreamed(const CPed* pPed, const CVehicle* pVehicle)
{
	const s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	taskAssertf( pVehicle->IsSeatIndexValid(iSeatIndex), "The specified seat index is not valid" );

	const fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(*pPed, pVehicle, iSeatIndex);
	const CVehicleSeatAnimInfo* pSeatClipInfo = pVehicle->GetSeatAnimationInfo(pPed);

	return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(inVehicleClipsetId, pPed->IsLocalPlayer() ? SP_High : SP_Invalid) &&
		   !CTaskVehicleFSM::ShouldClipSetBeStreamedOut(inVehicleClipsetId) &&
		   SetSeatClipSet(pSeatClipInfo);
}

bool CTaskMotionInTurret::IsPlayerTryingToTurn(const CPed& rPed)
{
	taskAssert(rPed.IsLocalPlayer());
	if (!camInterface::GetCinematicDirector().IsAnyCinematicContextActive())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			const float fLookLeftRight = pControl->GetPedLookLeftRight().GetNorm();
			if (fLookLeftRight > GetTuningValues().m_NoTurnControlThreshold)
			{
				return true;
			}
		}
	}
	return false;
}

void CTaskMotionInTurret::SetInitialState()
{
	if (Abs(m_fInitialHeadingDelta) > GetTuningValues().m_MinHeadingDeltaToAdjust || m_bSeated)
	{
		SetState(State_InitialAdjust);
		return;
	}
	else
	{
		SetState(State_Idle);
		return;
	}
}

bool CTaskMotionInTurret::ShouldSetAircraftMode(const CPed &rPed)
{
	return rPed.IsLocalPlayer() && m_pVehicle && m_pVehicle->InheritsFromHeli();
}

bool CTaskMotionInTurret::WantsToDuck() const
{
	const CPed& rPed = *GetPed();
	if (rPed.IsLocalPlayer())
	{
		return CTaskMotionInAutomobile::WantsToDuck(rPed, true);
	}
	else
	{
		return rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle);
	}
}

bool CTaskMotionInTurret::CanBeDucked() const
{
	fwMvClipSetId clipSetId = GetDuckedClipSetId();
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	return true;
}

fwMvClipSetId CTaskMotionInTurret::GetDefaultClipSetId() const
{
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if (!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	return pSeatAnimInfo->GetSeatClipSetId();
}

fwMvClipSetId CTaskMotionInTurret::GetDuckedClipSetId() const
{
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if (!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	return pSeatAnimInfo->GetDuckedClipSet();
}

fwMvClipSetId CTaskMotionInTurret::GetAppropriateClipSetId() const
{
	if (IsInFirstPersonCamera())
	{
		return GetDefaultClipSetId();
	}
	return m_bIsDucking ? GetDuckedClipSetId() : GetDefaultClipSetId();
}

bool CTaskMotionInTurret::IsInFirstPersonCamera() const
{
	if (camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
	{
		return true;
	}
	return false;
}

#if !__FINAL
void CTaskMotionInTurret::Debug() const
{
	if (GetPed()->IsLocalPlayer() || GetPed() == CDebugScene::FocusEntities_Get(0))
	{
#if DEBUG_DRAW
		grcDebugDraw::AddDebugOutput(Color_green, "m_vCurrentVehicleAcceleration.x = %.2f", m_vCurrentVehicleAcceleration.x);
		grcDebugDraw::AddDebugOutput(Color_green, "m_bTurningLeft = %s", m_bTurningLeft ? "TURNING LEFT" : "TURNING RIGHT");
		grcDebugDraw::AddDebugOutput(Color_green, "m_fAngVelocity = %.2f", m_fAngVelocity);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fAngAcceleration = %.2f", m_fAngAcceleration);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fMvTurnSpeed = %.2f", m_fMvTurnSpeed);
#endif // DEBUG_DRAW
	}
}
#endif // !__FINAL

#endif // ENABLE_MOTION_IN_TURRET_TASK
