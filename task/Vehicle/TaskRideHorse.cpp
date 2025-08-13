#include "Task/Vehicle/TaskRideHorse.h"

#if ENABLE_HORSE

#include "Animation/AnimManager.h"
#include "animation/MovePed.h"
#include "camera/gameplay/GameplayDirector.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Horse/horseComponent.h"
#include "Peds/PedIntelligence.h"
#include "Task/Vehicle/TaskMountFollowNearestRoad.h"
#include "Task/Vehicle/TaskPlayerOnHorse.h"
#include "Task/default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "task\Motion\Locomotion\TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "task/Movement/Jumping/TaskJump.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"


AI_OPTIMISATIONS()

#define FRAMES_TO_DISABLE_BREAK (5) // To fix B* 146635, the desired speed is reset to 0 for a couple frames when ownership is transfered (due to this task and it's parents not being copied over the network).


u32 CTaskMotionRideHorse::ms_CameraHash						= ATSTRINGHASH("FOLLOW_PED_ON_HORSE_CAMERA", 0x0cdd88ad1);
s32 CTaskMotionRideHorse::ms_CameraInterpolationDuration	= 2500;

const fwMvClipSetVarId CTaskMotionRideHorse::ms_SpurClipSetVarId("Spurs",0x8a9783c);
const fwMvRequestId CTaskMotionRideHorse::ms_QuickStopId("QuickStop",0x951809DB);
const fwMvRequestId CTaskMotionRideHorse::ms_StartLocomotion("StartLocomotion",0x15F666A1);
const fwMvRequestId CTaskMotionRideHorse::ms_StartIdle("SitIdle",0x733CD73C);
const fwMvRequestId CTaskMotionRideHorse::ms_Spur("Spur",0xB5B49C6E);
const fwMvRequestId CTaskMotionRideHorse::ms_BrakingId("Braking",0x24D0F6F7);
const fwMvBooleanId CTaskMotionRideHorse::ms_QuickStopClipEndedId("QuickStopEnded",0x8ECB3922);
const fwMvBooleanId CTaskMotionRideHorse::ms_OnEnterIdleId("OnEnterIdle",0xF110481F);
const fwMvBooleanId CTaskMotionRideHorse::ms_OnEnterRideGaitsId("OnEnterRideGaits",0xCA228CB);
const fwMvBooleanId CTaskMotionRideHorse::ms_OnEnterQuickStopId("OnEnterQuickStop",0x26E1E521);
const fwMvBooleanId CTaskMotionRideHorse::ms_OnEnterBrakingId("OnEnterBraking",0xdca33a64);
const fwMvBooleanId CTaskMotionRideHorse::ms_OnExitRideGaitsId("OnExitRideGaits",0xA00CE9A1);
const fwMvFloatId CTaskMotionRideHorse::ms_MountSpeed("MountSpeed",0x383E38DD);
const fwMvFloatId CTaskMotionRideHorse::ms_WalkPhase("WalkPhase",0x347BDDC4);
const fwMvFloatId CTaskMotionRideHorse::ms_TrotPhase("TrotPhase",0x7E9788A4);
const fwMvFloatId CTaskMotionRideHorse::ms_CanterPhase("CanterPhase",0x65352AC3);
const fwMvFloatId CTaskMotionRideHorse::ms_GallopPhase("GallopPhase",0xE1B31487);
const fwMvFloatId CTaskMotionRideHorse::ms_TurnPhase("TurnPhase",0x9B1DE516);
const fwMvFloatId CTaskMotionRideHorse::ms_MoveSlopePhaseId("SlopePhase",0x1F6A5972);

CTaskMotionRideHorse::CTaskMotionRideHorse() :
m_bQuickStopRequested(false),
m_bMoVEInRideGaitsState(false),
m_fTurnPhase(0.5f),
m_WeaponClipSetId(CLIP_SET_ID_INVALID),
m_WeaponClipSet(NULL),
m_WeaponFilterId(FILTER_ID_INVALID),
m_pWeaponFilter(NULL),
m_Flags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_RIDE_HORSE);
}

CTaskMotionRideHorse::~CTaskMotionRideHorse()
{

}

CTask::FSM_Return CTaskMotionRideHorse::ProcessPreFSM()
{
	// If our mount no longer exists for no reason, end this task, so we don't
	// get stuck using riding clips. Hopefully the ped will get a more
	// appropriate motion task assigned.
	CPed* pPed = GetPed();
	if(!pPed->GetMyMount())
	{
		return FSM_Quit;
	}

	if (pPed->GetIsAttached())
	{	
		// On RDR3, we have removed these and replaced with a callback from the horse update, but it's safer to keep this approach
		// on DLC for now.
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true ); 
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );

		pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

		//pPed->SetPedResetFlag( CPED_RESET_FLAG_DontActivateRagdollFromHorseImpactReset, true );
	}

	return FSM_Continue;
}	

CTask::FSM_Return CTaskMotionRideHorse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_StreamClips)
			FSM_OnUpdate
				return StreamClips_OnUpdate();

		FSM_State(State_SitIdle)
			FSM_OnEnter
				SitIdle_OnEnter();
			FSM_OnUpdate
				return SitIdle_OnUpdate();

		FSM_State(State_Locomotion)
				FSM_OnEnter
				Locomotion_OnEnter();
			FSM_OnUpdate
				return Locomotion_OnUpdate();

		FSM_State(State_QuickStop)
			FSM_OnEnter
				QuickStop_OnEnter(); 
			FSM_OnUpdate
				return QuickStop_OnUpdate();

		FSM_State(State_Braking)
			FSM_OnEnter
				Brake_OnEnter(); 
			FSM_OnUpdate
				return Brake_OnUpdate();

		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
			
	FSM_End
}

CTaskMotionRideHorse::FSM_Return CTaskMotionRideHorse::ProcessPostFSM()
{
	// Send signals valid for all states
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		if(m_Flags.IsFlagSet(Flag_WeaponClipSetChanged))
		{
			if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
			{
				SendWeaponClipSet();
			}
			else
			{
				if(m_WeaponClipSet)
				{
					if(m_WeaponClipSet->Request_DEPRECATED())
					{
						SendWeaponClipSet();
					}
				}
			}
		}

		if(m_Flags.IsFlagSet(Flag_WeaponFilterChanged))
		{
			StoreWeaponFilter();
		}

		if(m_pWeaponFilter)
		{
			m_moveNetworkHelper.SetFilter(m_pWeaponFilter, CTaskHumanLocomotion::ms_WeaponHoldingFilterId);
		}
	}

	return FSM_Continue;
}

bool CTaskMotionRideHorse::ProcessPostPreRender() 
{
	//Stirrups
	CPed* rider = GetPed();
	if(rider)
	{
		CPed* horse = rider->GetMyMount();
		if (horse)
		{
			UpdateStirrups(rider, horse);
		}
	}

	return false;
}

void SetFloatClamped(CMoveNetworkHelper* rideNetwork, const fwMvFloatId &floatId, float fValue)
{
	taskAssert(rideNetwork);

	if(fValue >= 0.0f)
	{
		rideNetwork->SetFloat(floatId, fValue);
	}
}

void CTaskMotionRideHorse::UpdateStirrups(CPed* rider, CPed* horse)
{
	rmcDrawable* pStirrups = horse->GetComponent(PV_COMP_LOWR);
	if (pStirrups)
	{
		static dev_float theX = 0.05f;
		static dev_float theY = -0.01f;
		static dev_float theZ = 0.1f;
		Vector3 vOffset(theX, theY, theZ);

		static dev_float sfStirrupBlendRate = 3.0f;
		CHorseComponent* pHorseComponent = horse->GetHorseComponent();
		
		Approach(pHorseComponent->GetLeftStirrupBlendPhase(), pHorseComponent->GetLeftStirrupEnable() ? 1.0f : 0.0f, sfStirrupBlendRate, fwTimer::GetTimeStep());			
		UpdateStirrup(rider, horse, BONETAG_L_FOOT, BONETAG_L_STIRRUP, vOffset, pHorseComponent->GetLeftStirrupBlendPhase());			
		Approach(pHorseComponent->GetRightStirrupBlendPhase(), pHorseComponent->GetRightStirrupEnable() ? 1.0f : 0.0f, sfStirrupBlendRate, fwTimer::GetTimeStep());
		UpdateStirrup(rider, horse, BONETAG_R_FOOT, BONETAG_R_STIRRUP, vOffset, pHorseComponent->GetRightStirrupBlendPhase());
	}
}

void CTaskMotionRideHorse::UpdateStirrup(CPed* rider, CPed* horse, eAnimBoneTag nRiderBoneId, eAnimBoneTag nStirrupBoneId, const Vector3& vOffset, float fBlendPhase) 
{
	Matrix34 footBone;
	if(!rider->GetBoneMatrix(footBone, nRiderBoneId))
	{
		return;
	}	
	Matrix34 stirrupBone;
	if(!horse->GetBoneMatrix(stirrupBone, nStirrupBoneId))
	{
		return;
	}			
	Matrix34 mOldMtx = stirrupBone;
	Matrix34 mNewMtx = footBone;
	static dev_float YROT = HALF_PI;
	static dev_float ZROT = -HALF_PI;
	mNewMtx.RotateLocalY(YROT);
	mNewMtx.RotateLocalZ(ZROT);
	
	Vector3 vNewOffset;
	mNewMtx.Transform3x3(vOffset, vNewOffset);
	mNewMtx.Translate(vNewOffset);

	mNewMtx.Interpolate(mOldMtx, mNewMtx, fBlendPhase);

	s16 boneIndex = (s16)horse->GetBoneIndexFromBoneTag(nStirrupBoneId);	
	horse->GetSkeleton()->SetGlobalMtx(boneIndex, MATRIX34_TO_MAT34V(mNewMtx));
}

void	CTaskMotionRideHorse::CleanUp()
{
	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}

	// Clear any old filter
	if(m_pWeaponFilter)
	{
		m_pWeaponFilter->Release();
		m_pWeaponFilter = NULL;
	}
}

void CTaskMotionRideHorse::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CPed *pPed = GetPed(); //Get the ped ptr.
	if(!pPed)
	{
		return;
	}

	CPed* pMount = pPed->GetMyMount();
	if(!pMount)
	{
		return;
	}

	settings.m_CameraHash = ms_CameraHash;
	settings.m_InterpolationDuration = ms_CameraInterpolationDuration;
	settings.m_AttachEntity = pMount;
}

void CTaskMotionRideHorse::Start_OnEnter()
{
	//nothing to do here
}

CTask::FSM_Return CTaskMotionRideHorse::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	if (pPed->IsDead() || pPed->GetUsingRagdoll())
	{
		return FSM_Continue;
	}

	SetState(State_StreamClips);
	return FSM_Continue;
}


CTask::FSM_Return CTaskMotionRideHorse::StreamClips_OnUpdate()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();

	if (!pMount) 
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	int iSeatIndex = pMount->GetSeatManager()->GetPedsSeatIndex(pPed); 
	const CVehicleSeatAnimInfo* pSeatClipInfo = pMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(iSeatIndex);
	if (pSeatClipInfo)
	{
		if (m_ClipSetRequestHelper.Request(pSeatClipInfo->GetSeatClipSetId()))
		{	
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskRideHorse);

			taskAssertf(fwClipSetManager::GetClipSet(pSeatClipInfo->GetSeatClipSetId()), "Couldn't find clipset %s", pSeatClipInfo->GetSeatClipSetId().GetCStr());

			pPed->GetMovePed().SetMotionTaskNetwork(m_moveNetworkHelper, 0);
			m_moveNetworkHelper.SetClipSet(pSeatClipInfo->GetSeatClipSetId()); 

			SetState(State_SitIdle);
		}
	}
	
	return FSM_Continue;
}

void CTaskMotionRideHorse::SitIdle_OnEnter()
{	
	m_moveNetworkHelper.SendRequest( ms_StartIdle);
	m_moveNetworkHelper.WaitForTargetState(ms_OnEnterIdleId);
}

CTask::FSM_Return CTaskMotionRideHorse::SitIdle_OnUpdate()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();

	if (pPed->IsDead() || pPed->GetUsingRagdoll())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	ApplyWeaponClipSet();

	bool bExtractedHorseData = false;
	Vector4 horsePhase;
	float horseTurnPhase, horseSpeed = 0.0f;
	float horseSlope = 0.5f;
	if ( pMount )  
	{
		CTask* pActiveSimplestTask = pMount->GetPedIntelligence()->GetMotionTaskActiveSimplest();
		if( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE )
		{
			if( static_cast<CTaskHorseLocomotion*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlope) )
			{
				bExtractedHorseData = true;
			}
		}
		else if ( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING )
		{
			if( static_cast<CTaskMotionSwimming*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlope) )
			{
				bExtractedHorseData = true;			
			}
		}
	}
	if ( bExtractedHorseData && horseSpeed != 0.0f) 
	{
		SetState(State_Locomotion);
	}
	m_moveNetworkHelper.SetFloat(ms_MoveSlopePhaseId, horseSlope);

	CTask *task = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
	if (task) 
	{
		CMoveNetworkHelper* driveNetwork = static_cast<CTaskAimGunVehicleDriveBy*>(task)->GetMoveNetworkHelper();
		driveNetwork->SetFloat(ms_MountSpeed, 0);
	}

	return FSM_Continue;
}

void CTaskMotionRideHorse::Locomotion_OnEnter()
{	
	m_fTurnPhase = 0.5f;
	m_moveNetworkHelper.SendRequest( ms_StartLocomotion);
	m_moveNetworkHelper.WaitForTargetState(ms_OnEnterRideGaitsId);
	m_bMoVEInRideGaitsState = false;
}

CTask::FSM_Return CTaskMotionRideHorse::Locomotion_OnUpdate()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();

	if (!m_bMoVEInRideGaitsState && m_moveNetworkHelper.IsInTargetState())
	{
		m_bMoVEInRideGaitsState = true;
		m_moveNetworkHelper.WaitForTargetState(ms_OnExitRideGaitsId);
	}
	else if (m_bMoVEInRideGaitsState && m_moveNetworkHelper.IsInTargetState())
	{
		m_bMoVEInRideGaitsState = false;
		m_moveNetworkHelper.WaitForTargetState(ms_OnEnterRideGaitsId);
	}

	ApplyWeaponClipSet();

	if (pPed->IsDead() || pPed->GetUsingRagdoll())
	{
		SetState(State_Start);
		return FSM_Continue;
	}	

	bool bExtractedHorseData = false;
	Vector4 horsePhase;
	float horseTurnPhase = 0.0f;
	float horseSpeed = 0.0f;
	float horseSlopePhase = 0.0f;
	if ( pMount )  
	{
		CTask* pActiveSimplestTask = pMount->GetPedIntelligence()->GetMotionTaskActiveSimplest();
		if( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE )
		{
			if( static_cast<CTaskHorseLocomotion*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase) )
			{
				bExtractedHorseData = true;
			}
		}
		else if ( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING )
		{
			if( static_cast<CTaskMotionSwimming*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase) )
			{
				bExtractedHorseData = true;			
			}
		}
	}

	if ( bExtractedHorseData )
	{
		static fwMvFilterId s_LegsOnlyFilterId("LegsOnlyFilter",0x8BC32D40);
		static fwMvFilterId s_LegsOnlyFilterHashId("LegsOnly_filter",0xCF7C3B18);
		m_moveNetworkHelper.SetFilter(g_FrameFilterDictionaryStore.FindFrameFilter(s_LegsOnlyFilterHashId), s_LegsOnlyFilterId);

		static dev_float WalkTurnApproachRate = 0.75f;	 
		Approach(m_fTurnPhase, horseTurnPhase, WalkTurnApproachRate, fwTimer::GetTimeStep());		

		//Displayf("Turn phase: %f", horseTurnPhase);
		m_moveNetworkHelper.SetFloat(ms_TurnPhase, m_fTurnPhase);
		SetFloatClamped(&m_moveNetworkHelper, ms_WalkPhase, horsePhase.x);
		SetFloatClamped(&m_moveNetworkHelper, ms_TrotPhase, horsePhase.y);
		SetFloatClamped(&m_moveNetworkHelper, ms_CanterPhase, horsePhase.z);
		SetFloatClamped(&m_moveNetworkHelper, ms_GallopPhase, horsePhase.w);
		m_moveNetworkHelper.SetFloat(ms_MountSpeed, horseSpeed);
		m_moveNetworkHelper.SetFloat(ms_MoveSlopePhaseId, horseSlopePhase);

		CTask *task = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
		if (task) 
		{
			CMoveNetworkHelper* driveNetwork = static_cast<CTaskAimGunVehicleDriveBy*>(task)->GetMoveNetworkHelper();
			SetFloatClamped(driveNetwork, ms_WalkPhase, horsePhase.x);
			SetFloatClamped(driveNetwork, ms_TrotPhase, horsePhase.y);
			SetFloatClamped(driveNetwork, ms_CanterPhase, horsePhase.z);
			SetFloatClamped(driveNetwork, ms_GallopPhase, horsePhase.w);
			driveNetwork->SetFloat(ms_MountSpeed, horseSpeed);
		}

		if ( m_bMoVEInRideGaitsState && (horseSpeed == 0.f))
		{
			SetState(State_SitIdle);
			return FSM_Continue;
		}
	}

	//Spur processing
	if (pMount) 
	{
		CHorseComponent* pHorseComponent = pMount->GetHorseComponent();
		if (pHorseComponent->GetSpurredThisFrame() && !pHorseComponent->GetSuppressSpurAnim())
		{
			//Send spur to driveby network if ped is in gunplay.
			CTask *task = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
			if (task) 
			{
				CMoveNetworkHelper* driveByNetwork = static_cast<CTaskAimGunVehicleDriveBy*>(task)->GetMoveNetworkHelper();
				if (driveByNetwork->IsNetworkAttached())
				{
					static fwMvFilterId s_LowerBodyFilterId("LowerBodyFilter",0xcd12aa22);
					static fwMvFilterId s_LowerBodyFilterHashId("LegsOnly_filter",0xcf7c3b18);
					driveByNetwork->SetFilter(g_FrameFilterDictionaryStore.FindFrameFilter(s_LowerBodyFilterHashId), s_LowerBodyFilterId);

					driveByNetwork->SetClipSet(m_moveNetworkHelper.GetClipSetId(), ms_SpurClipSetVarId);
					driveByNetwork->SendRequest( ms_Spur);					
				}
			} 
			else
				m_moveNetworkHelper.SendRequest( ms_Spur);
		}

		if (pHorseComponent->GetBrakeCtrl().GetSoftBrake() || (pHorseComponent->GetBrakeCtrl().GetHardBrake() && !m_bQuickStopRequested))
		{
			SetState(State_Braking);
			return FSM_Continue;
		}
	}

	if (m_bQuickStopRequested)
	{
		m_bQuickStopRequested = false;
		SetState(State_QuickStop);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void  CTaskMotionRideHorse::Brake_OnEnter()
{
	m_moveNetworkHelper.SendRequest( ms_BrakingId );	
	m_moveNetworkHelper.WaitForTargetState(ms_OnEnterBrakingId);
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskMotionRideHorse::Brake_OnUpdate()
{
	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	CPed *pPed = GetPed();
	
	CPed* pMount = pPed->GetMyMount();
	bool bBraking = false;
	if (pMount) 
	{
		CHorseComponent* pHorseComponent = pMount->GetHorseComponent();
		bBraking = pHorseComponent->GetBrakeCtrl().GetSoftBrake() || (pHorseComponent->GetBrakeCtrl().GetHardBrake() && !m_bQuickStopRequested);
	}

	bool bExtractedHorseData = false;
	Vector4 horsePhase;
	float horseTurnPhase = 0.0f;
	float horseSpeed = 0.0f;
	float horseSlopePhase = 0.0f;
	if ( pMount )  
	{
		CTask* pActiveSimplestTask = pMount->GetPedIntelligence()->GetMotionTaskActiveSimplest();
		if( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE )
		{
			if( static_cast<CTaskHorseLocomotion*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase) )
			{
				bExtractedHorseData = true;
			}
		}
		else if ( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING )
		{
			if( static_cast<CTaskMotionSwimming*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase) )
			{
				bExtractedHorseData = true;			
			}
		}

		if ( horseSpeed == 0.f /*&& finished intro*/)
		{
			SetState(State_SitIdle);
			return FSM_Continue;
		}
		else if (!bBraking)
		{
			SetState(State_Locomotion);
			return FSM_Continue;
		}
	}

	//! go to quick stop.
	if (m_bQuickStopRequested)
	{
		m_bQuickStopRequested = false;
		SetState(State_QuickStop);
	}

	//! go to locomotion.
	
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void  CTaskMotionRideHorse::QuickStop_OnEnter()
{
	m_moveNetworkHelper.SendRequest( ms_QuickStopId );	
	m_moveNetworkHelper.WaitForTargetState(ms_OnEnterQuickStopId);
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return	CTaskMotionRideHorse::QuickStop_OnUpdate()
{
	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	if ( m_moveNetworkHelper.GetBoolean(ms_QuickStopClipEndedId)) 
	{		
		SetState(State_SitIdle);
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskMotionRideHorse::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Start",		
		"State_StreamAnims",
		"State_SitIdle",    
		"State_Locomotion",
		"State_QuickStop",
		"State_Braking",
		"State_Exit"
	};
	return aStateNames[iState];
}

void CTaskMotionRideHorse::Debug() const
{
#if DEBUG_DRAW
	TUNE_GROUP_BOOL (HORSE_MOVEMENT, bShowRiderHorseInfo, false);
	if ( bShowRiderHorseInfo )
	{
		const CPed* pPed = GetPed();
		if( pPed )
		{
			const CPed* pMount = pPed->GetMyMount();
			if ( pMount )  
			{
				bool bExtractedHorseData = false;
				Vector4 horsePhase;
				float horseTurnPhase = 0.0f;
				float horseSpeed = 0.0f;
				float horseSlopePhase = 0.0f;
				CTask* pActiveSimplestTask = pMount->GetPedIntelligence()->GetMotionTaskActiveSimplest();
				if( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE )
				{
					if( static_cast<CTaskHorseLocomotion*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase) )
					{
						bExtractedHorseData = true;
					}
				}
				else if ( pActiveSimplestTask->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING )
				{
					if( static_cast<CTaskMotionSwimming*>(pActiveSimplestTask)->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase) )
					{
						bExtractedHorseData = true;			
					}
				}
				if ( bExtractedHorseData )
				{
					char debugText[100];
					int iNumLines = 0;
					const s32 iTextOffsetY = grcDebugDraw::GetScreenSpaceTextHeight();
					formatf(debugText, "HorsePhase: %.2f/%.2f/%.2f/%.2f", horsePhase.x, horsePhase.y, horsePhase.z, horsePhase.w);
					grcDebugDraw::Text( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_blue, 0, iTextOffsetY*iNumLines, debugText);
					iNumLines++;
					formatf(debugText, "TurnPhase:  %.2f", horseTurnPhase);
					grcDebugDraw::Text( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_blue, 0, iTextOffsetY*iNumLines, debugText);
					iNumLines++;
					formatf(debugText, "SlopePhase: %.2f", horseSlopePhase);
					grcDebugDraw::Text( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_blue, 0, iTextOffsetY*iNumLines, debugText);
					iNumLines++;
					formatf(debugText, "Speed:      %.2f", horseSpeed);
					grcDebugDraw::Text( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), Color_blue, 0, iTextOffsetY*iNumLines, debugText);
					iNumLines++;
				}
			}
		}
	}
#endif

	CTaskMotionBase::Debug();
}

#endif // !__FINAL

Vec3V_Out CTaskMotionRideHorse::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix,float fTimestep)
{
	return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep);	
}


CTask * CTaskMotionRideHorse::CreatePlayerControlTask()
{
	return rage_new CTaskPlayerOnHorse();
}


//////////////////////////////////////////////////////////////////////////

void CTaskMotionRideHorse::ApplyWeaponClipSet()
{
	const CPed* pPed = GetPed();
	const CWeapon * pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;

	if(pWeapon && pWeapon->GetWeaponInfo())
	{
		SetWeaponClipSet(pWeapon->GetWeaponInfo()->GetPedMotionClipSetId(*pPed), pWeapon->GetWeaponInfo()->GetPedMotionFilterId(*pPed));		
	}
	else
	{
		SetWeaponClipSet(CLIP_SET_ID_INVALID, FILTER_ID_INVALID);
	}
}

void CTaskMotionRideHorse::SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	if(m_WeaponClipSetId != clipSetId)
	{

		// Store the new clip set
		m_WeaponClipSetId = clipSetId;

		bool bClipSetStreamed = false;
		if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
		{
			// Clip set doesn't need to be streamed
			bClipSetStreamed = true;
			m_WeaponClipSet = NULL;
		}
		else
		{
			m_WeaponClipSet = fwClipSetManager::GetClipSet(m_WeaponClipSetId);
			if(taskVerifyf(m_WeaponClipSet, "Weapon clip set [%s] doesn't exist", m_WeaponClipSetId.GetCStr()))
			{
				if(m_WeaponClipSet->Request_DEPRECATED())
				{
					// Clip set streamed
					bClipSetStreamed = true;
				}
			}
		}

		if(m_moveNetworkHelper.IsNetworkActive() && bClipSetStreamed)
		{
			// Send immediately
			SendWeaponClipSet();
		}
		else
		{
			// Flag the change
			m_Flags.SetFlag(Flag_WeaponClipSetChanged);
		}
	}

	if(m_WeaponFilterId != filterId)
	{
		// Clear any old filter
		if(m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}

		// Store the new filter
		m_WeaponFilterId = filterId;

		if(m_WeaponFilterId != FILTER_ID_INVALID)
		{
			if(GetEntity())
			{
				// Store now
				StoreWeaponFilter();
			}
			else
			{
				// Flag the change
				m_Flags.SetFlag(Flag_WeaponFilterChanged);
			}
		}
		else
		{
			// Clear any pending flags, as the filter is now set to invalid
			m_Flags.ClearFlag(Flag_WeaponFilterChanged);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionRideHorse::SendWeaponClipSet()
{
	m_moveNetworkHelper.SetClipSet(m_WeaponClipSetId, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);

	bool bUseWeaponHolding = m_WeaponClipSetId != CLIP_SET_ID_INVALID;

	m_moveNetworkHelper.SetFlag(bUseWeaponHolding, CTaskHumanLocomotion::ms_UseWeaponHoldingId);
	if (bUseWeaponHolding)
		m_moveNetworkHelper.SendRequest(CTaskHumanLocomotion::ms_RestartWeaponHoldingId);

	// Don't do this again
	m_Flags.ClearFlag(Flag_WeaponClipSetChanged);
}


void CTaskMotionRideHorse::StoreWeaponFilter()
{
	if(taskVerifyf(m_WeaponFilterId != FILTER_ID_INVALID, "Weapon filter [%s] is invalid", m_WeaponFilterId.GetCStr()))
	{
		m_pWeaponFilter = CGtaAnimManager::FindFrameFilter(m_WeaponFilterId.GetHash(), GetPed());
		if(taskVerifyf(m_pWeaponFilter, "Failed to get filter [%s]", m_WeaponFilterId.GetCStr()))	
		{
			m_pWeaponFilter->AddRef();
			m_Flags.ClearFlag(Flag_WeaponFilterChanged);
		}
	}
}


//////////////////////////////////////////////////////////////////////////

#if AI_RIDE_DBG
#define AI_RIDE_DBG_MSG(x) DebugMsgf x
#else
#define AI_RIDE_DBG_MSG(x)
#endif

float CTaskMoveMountedSpurControl::sm_ClipFilterSpurThreshold = 0.2f; //rdr2 was 0.1f, but lowering spur frequency at least until follow tasks are better, B* 1732590
float CTaskMoveMountedSpurControl::sm_ClipFilterSpurThresholdRandomness = 0.5f;
//float CTaskMoveMountedSpurControl::sm_ClipFilterBrakeThreshold = 0.5f;
bool CTaskMoveMountedSpurControl::sm_ClipFilterEnabled = true;


CTask::FSM_Return CTaskMoveMountedSpurControl::StateSpurring_OnUpdate()
{
	if(!ShouldUseHorseSim())
	{
		SetState(State_NoSim);
		return FSM_Continue;
	}

	const Tuning &tuning = m_Tuning;

	CHorseComponent& hrs = *GetPed()->GetHorseComponent();

	const float currentNormSpeed = Max(hrs.GetSpeedCtrl().GetCurrSpeed(), 0.0f);

	const float desiredNormSpeed = CalcTargetNormSpeed();

	bool bFollowingLeader = (GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_BE_IN_FORMATION) != NULL);
	if (!bFollowingLeader)
	{
		if(m_FramesToDisableBreak)
		{
			--m_FramesToDisableBreak;
		}
		else if((currentNormSpeed > desiredNormSpeed + tuning.Get(Tuning::kAiRideNormSpeedStartBrakeThreshold)) || desiredNormSpeed <= SMALL_FLOAT)
		{
			AI_RIDE_DBG_MSG((1, "Entering braking state."));
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetState(State_Braking);
			return FSM_Continue;
		}
	}

	const float dt = TIME.GetSeconds();

	const float err = desiredNormSpeed - currentNormSpeed;
	const float stickYP = tuning.Get(Tuning::kAiRideStickYProportionalConstant);
	const float newStick = Clamp(m_CurrentStickY + stickYP*err*dt, 0.0f, 1.0f);		// Should -1.0f or 0.0f be the minimum here? /FF
	m_CurrentStickY = newStick;

	bool spur = false;

	if((currentNormSpeed > desiredNormSpeed - tuning.Get(Tuning::kAiRideNormSpeedSpurThreshold)) && (desiredNormSpeed < SMALL_FLOAT || currentNormSpeed >= SMALL_FLOAT))
	{
		AI_RIDE_DBG_MSG((3, "No spur - speed high enough."));
	}
	else if(TIME.GetElapsedTime() < m_LastSpurTime + tuning.Get(Tuning::kAiRideMinTimeBetweenSpurs))
	{
		AI_RIDE_DBG_MSG((3, "No spur - not enough time has elapsed."));
	}
	else if(hrs.GetFreshnessCtrl().GetCurrFreshnessRelativeToMax() < tuning.Get(Tuning::kAiRideMinFreshnessForSpur))
	{
		AI_RIDE_DBG_MSG((3, "No spur - freshness too low."));
	}
 	else if (!bFollowingLeader && (hrs.GetAgitationCtrl().GetCurrAgitation() > tuning.Get(Tuning::kAiRideMaxAgitationForSpur)))
 	{
 		AI_RIDE_DBG_MSG((3, "No spur - agitation too high."));
 	}
	else
	{
		spur = true;

		m_LastSpurTime = TIME.GetElapsedTime();

		// See if the speed increase we desire is large enough to exceed a threshold, which
		// includes some random variation. If large enough, we let clips play. If this
		// is really just a smaller speed increase we are spurring for, it tends to look better
		// to just not play the spur clips, even if we would have done so if the player
		// were riding. /FF
		const float rand = fwRandom::GetRandomNumberInRange(0.0f, sm_ClipFilterSpurThresholdRandomness);
		if(sm_ClipFilterEnabled && err <= (sm_ClipFilterSpurThreshold + rand))
		{		
			hrs.SetSuppressSpurAnim();
		}

		AI_RIDE_DBG_MSG((1, "Spurring (%d)", (int)spur));
	}


	ApplyInput(spur, 0.0f, newStick);
	return FSM_Continue;
}


CTask::FSM_Return CTaskMoveMountedSpurControl::StateBraking_OnUpdate()
{
	if(!ShouldUseHorseSim())
	{
		SetState(State_NoSim);
		return FSM_Continue;
	}

	const Tuning &tuning = m_Tuning;

	const float desiredNormSpeed = CalcTargetNormSpeed();

	const CHorseComponent& hrs = *GetPed()->GetHorseComponent();

	const float currentNormSpeed = Max(hrs.GetSpeedCtrl().GetCurrSpeed(), 0.0f);

	const bool still = (currentNormSpeed <= SMALL_FLOAT);
	if((currentNormSpeed < desiredNormSpeed + tuning.Get(Tuning::kAiRideNormSpeedStopBrakeThreshold)) || (desiredNormSpeed > SMALL_FLOAT && still))
	{
		AI_RIDE_DBG_MSG((1, "Entering spurring state"));
		m_CurrentStickY = 0.0f;		// Start over at 0, I guess. /FF
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_Spurring);
		return FSM_Continue;
	}

	static dev_float sf_MinSpeedForBrakingAnims = 0.25f;
	float brake = (!still && (desiredNormSpeed <= sf_MinSpeedForBrakingAnims) ? 1.0f : 0.0f);

// TODO: Perhaps re-enable this, if needed.
#if 0
	// Decide if we should allow brake clips to play or not. This is done
	// if the desired slowdown exceeds a threshold, or if we are trying to
	// slow down to a complete stop and are not already moving very slowly, or
	// if we allowed the clip to play last frame. /FF
	if(!sm_ClipFilterEnabled
			|| currentNormSpeed > desiredNormSpeed + sm_ClipFilterBrakeThreshold
			|| (desiredNormSpeed <= 0.01f && currentNormSpeed >= 0.1f)	// MAGIC!
			|| (m_FlagClipatingBrake))
	{
		msgOut.m_bAllowBrakeAndSpurAnimation = true;
	}
	else
	{
		msgOut.m_bAllowBrakeAndSpurAnimation = false;
	}
#endif

#if AI_RIDE_DBG
	if(brake > 0.0f)
	{
		AI_RIDE_DBG_MSG((1, "Braking"));
	}
#endif

	ApplyInput(false, brake, 0.0f);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveMountedSpurControl::StateNoSim_OnUpdate()
{
	if(ShouldUseHorseSim())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	GetPed()->GetMotionData()->SetDesiredMoveBlendRatio(m_InputTargetMoveBlendRatio);

	return FSM_Continue;
}


CTask::FSM_Return CTaskMoveMountedSpurControl::StateFinish_OnUpdate()
{
	return FSM_Quit;
}

#if AI_RIDE_DBG

void CTaskMoveMountedSpurControl::DebugMsgf(int level, const char *fmt, ...) const
{
	if(m_DebugLevel < level)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);

	char buff[256];
	vformatf(buff, sizeof(buff), fmt, args);

	Displayf("%d: %p %s", TIME.GetFrameCount(), (const void*)GetPed(), buff);
}

#endif

CTaskMoveMountedSpurControl::Tuning::Tuning()
{
	Reset();
}


void CTaskMoveMountedSpurControl::Tuning::Reset()
{
	m_Params[kAiRideMaxAgitationForSpur] = 0.1f;
	m_Params[kAiRideMinFreshnessForSpur] = 0.8f;
	m_Params[kAiRideMinTimeBetweenSpurs] = 0.5f;
	m_Params[kAiRideNormSpeedSpurThreshold] = 0.1f;
	m_Params[kAiRideNormSpeedStartBrakeThreshold] = 0.2f;
	m_Params[kAiRideNormSpeedStopBrakeThreshold] = 0.0f;
	m_Params[kAiRideStickYProportionalConstant] = 5.0f;

	// If you add new parameters, make sure to initialize them to
	// reasonable values above, then update the count here. /FF
	CompileTimeAssert(kNumAiRideParams == 7);
}

//////////////////////////////////////////////////////////////////////////
static int si_Counter=0;
CTaskMoveMountedSpurControl::CTaskMoveMountedSpurControl()
		: CTaskMove(MOVEBLENDRATIO_STILL)
		, m_InputTargetMoveBlendRatio(0.0f)
		, m_CurrentStickY(0.0f)
		, m_LastSpurTime(-LARGE_FLOAT)
		, m_FramesToDisableBreak(FRAMES_TO_DISABLE_BREAK)
#if AI_RIDE_DBG
		, m_DebugLevel(0)
#endif
// TODO: Perhaps re-enable this, if needed.
#if 0
		, m_FlagClipatingBrake(false)
#endif
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_MOUNTED_SPUR_CONTROL);
	si_Counter++;
}


CTaskMoveMountedSpurControl::~CTaskMoveMountedSpurControl()
{
}


aiTask* CTaskMoveMountedSpurControl::Copy() const
{
	CTaskMoveMountedSpurControl* pTask = rage_new CTaskMoveMountedSpurControl();
	pTask->m_InputTargetMoveBlendRatio = m_InputTargetMoveBlendRatio;
	return pTask;
}


CTask::FSM_Return CTaskMoveMountedSpurControl::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				return StateStart_OnEnter();
			FSM_OnUpdate
				return StateStart_OnUpdate();
		FSM_State(State_Spurring)
			FSM_OnUpdate
				return StateSpurring_OnUpdate();
		FSM_State(State_Braking)
			FSM_OnUpdate
				return StateBraking_OnUpdate();
		FSM_State(State_NoSim)
			FSM_OnUpdate
				return StateNoSim_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return StateFinish_OnUpdate();
	FSM_End
}


float CTaskMoveMountedSpurControl::CalcTargetNormSpeed() const
{
	float desiredNormSpeed = m_InputTargetMoveBlendRatio/MOVEBLENDRATIO_SPRINT;

	// If the desired speed is very small, make it 0. This is to prevent a problem where
	// the state machine could potentially oscillate between the spurring and braking states,
	// as both switch conditions would be fulfilled if
	//	desiredNormSpeed <= SMALL_FLOAT && currentNormSpeed < desiredNormSpeed
	// This was rarely happening in practice, but I did see the safety mechanism trip once,
	// probably because of this. /FF
	desiredNormSpeed = Selectf(desiredNormSpeed - (1.5f*SMALL_FLOAT), desiredNormSpeed, 0.0f);

	return desiredNormSpeed;
}

#if !__FINAL

const char * CTaskMoveMountedSpurControl::GetStaticStateName( s32 iState )
{
	static const char *s_StateNames[] =
	{
		"State_Start",
		"State_Spurring",
		"State_Braking",
		"State_NoSim",
		"State_Finish"
	};

	if(iState >= 0 && iState < kNumStates)
	{
		CompileTimeAssert(NELEM(s_StateNames) == kNumStates);
		return s_StateNames[iState];
	}
	else
	{
		return "Invalid";
	}
}

void CTaskMoveMountedSpurControl::Debug() const
{
#if DEBUG_DRAW
	TUNE_GROUP_BOOL(HORSE_MOVEMENT, bDebug_TASK_MOVE_MOUNTED_SPUR_CONTROL, false);
	if (bDebug_TASK_MOVE_MOUNTED_SPUR_CONTROL)
	{
		const CPed* pPed = GetPed();
		char str[100];

#define TMMSC_DEBUG_FLOAT(f, yy)	formatf(str, #f": %.2f", f); grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_white, 0, 10 * yy, str);

		TMMSC_DEBUG_FLOAT(m_fMoveBlendRatio, 1)
		TMMSC_DEBUG_FLOAT(m_InputTargetMoveBlendRatio, 2)
		TMMSC_DEBUG_FLOAT(m_CurrentStickY, 3)
		TMMSC_DEBUG_FLOAT(m_LastSpurTime, 4)

		float dt = TIME.GetSeconds();
		TMMSC_DEBUG_FLOAT(dt, 5)

#undef TMMSC_DEBUG_FLOAT
	}
#endif	// #if DEBUG_DRAW

	const CTask* pSubTask = GetSubTask();
	if (pSubTask)
	{
		pSubTask->Debug();
	}
}

#endif	// !__FINAL

CTask::FSM_Return CTaskMoveMountedSpurControl::StateStart_OnEnter()
{
#if AI_RIDE_DBG
	m_DebugLevel = 0;
#endif

	m_LastSpurTime = -LARGE_FLOAT;
	m_CurrentStickY = 0.0f;

	m_Tuning.Reset();

// TODO: Perhaps re-enable this, if needed.
#if 0
	m_FlagClipatingBrake = false;
#endif


	return FSM_Continue;
}


CTask::FSM_Return CTaskMoveMountedSpurControl::StateStart_OnUpdate()
{
	if(ShouldUseHorseSim())
	{
		CTaskMoveMounted* pTask = rage_new CTaskMoveMounted;
		SetNewTask(pTask);

		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_Spurring);
	}
	else
	{
		SetState(State_NoSim);
	}

	return FSM_Continue;
}

void CTaskMoveMounted::SetDesiredHeading(float fDesiredHeading) 
{
#if DR_ENABLED
	if ( GetEntity() )
	{
		CPed* pPed = GetPed();
		if (pPed)
		{
			debugPlayback::RecordTaggedFloatValue(*pPed->GetCurrentPhysicsInst(), fDesiredHeading, "CTaskMoveMounted::SetDesiredHeading");
		}
	}
#endif
	m_fDesiredHeading = fDesiredHeading;
}

#if !__FINAL
void CTaskMoveMounted::Debug() const
{
#if DEBUG_DRAW
	TUNE_GROUP_BOOL(HORSE_MOVEMENT, bDebug_TASK_MOVE_MOUNTED, false);
	if (bDebug_TASK_MOVE_MOUNTED)
	{
		const CPed* pPed = GetPed();
		char str[100];
		s32 y = 10;

#define TMM_DEBUG_FLOAT(f)		formatf(str, #f": %.2f", f); grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_white, 0, y, str); y += 10
#define TMM_DEBUG_VECTOR2(v)	formatf(str, #v": (%.2f, %.2f)", v.x, v.y); grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_white, 0, y, str); y += 10
#define TMM_DEBUG_BOOL(b)		formatf(str, #b": %s", b ? "true" : "false"); grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_white, 0, y, str); y += 10
#define TMM_DEBUG_INT(i)		formatf(str, #i": %d", i); grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_white, 0, y, str); y += 10
	
		TMM_DEBUG_FLOAT(m_fMoveBlendRatio);
		
		formatf(str, "(Actual speed: %.2f)", Mag(VECTOR3_TO_VEC3V(pPed->GetVelocity())).Getf());
		grcDebugDraw::Text(pPed->GetTransform().GetPosition(), Color_white, 0, y, str);
		y += 10;
		
		TMM_DEBUG_FLOAT(m_fDesiredHeading);
		TMM_DEBUG_FLOAT(m_fTimeSinceSpur);
		TMM_DEBUG_FLOAT(m_fMatchSpeed);
		TMM_DEBUG_FLOAT(m_fBrake);
		TMM_DEBUG_FLOAT(m_fReverseDelay);

		TMM_DEBUG_VECTOR2(m_vStick);

		TMM_DEBUG_BOOL(m_bHardBrake);
		TMM_DEBUG_BOOL(m_bSpurRequested);
		TMM_DEBUG_BOOL(m_bMaintainSpeed);
		TMM_DEBUG_BOOL(m_bIsAiming);
		TMM_DEBUG_BOOL(m_bReverseRequested);

		TMM_DEBUG_INT(m_eJumpRequestType);

		float dt = TIME.GetSeconds();
		TMM_DEBUG_FLOAT(dt);

#undef TMM_DEBUG_FLOAT
#undef TMM_DEBUG_VECTOR2
#undef TMM_DEBUG_BOOL
#undef TMM_DEBUG_INT
	}
	// Call our base class
	CTask::Debug();

#endif	// #if DEBUG_DRAW
}
#endif // !__FINAL	

void CTaskMoveMountedSpurControl::ApplyInput(bool spur, float brake, float stickY)
{
	CTask* pSubTask = GetSubTask();
	if(!Verifyf(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_MOUNTED, "Expected task not running"))
	{
		return;
	}

	CPed* pPed = GetPed();

	CTaskMoveMounted* pMoveMountedTask = static_cast<CTaskMoveMounted*>(pSubTask);
	float outputMatchSpeed = -1.0f;

	const CHorseComponent& hrs = *GetPed()->GetHorseComponent();
	const CPed* pRider = hrs.GetRider();
	if (pRider)
	{
		CPlayerInfo* pPlayerInfo = pRider->GetPlayerInfo();
		if (pPlayerInfo)
		{
			CPlayerHorseSpeedMatch* playerInputHorse = pPlayerInfo->GetInputHorse();
			if (playerInputHorse)
			{
				outputMatchSpeed = playerInputHorse->GetCurrentSpeedMatchSpeed();
			}
		}
	}

	Vector2 stick(0.0f, stickY);
	pMoveMountedTask->ApplyInput(pPed->GetDesiredHeading(), spur, false, brake, (outputMatchSpeed >= 0.f), false, stick, outputMatchSpeed);

#if AI_RIDE_DBG
	if(m_DebugLevel >= 2)
	{
		const CHorseComponent& hrs = *GetPed()->GetHorseComponent();

		const int state = GetState();
		Assert(state >= 0 && state < kNumStates);
		DebugMsgf(2, "st: %-9s cur: %.2f des: %.2f ag: %.2f fr: %.2f y: %.2f %s%s", GetStateName(state),
				hrs.GetSpeedCtrl().GetCurrSpeed(), CalcTargetNormSpeed(),
				hrs.GetAgitationCtrl().GetCurrAgitation(),
				hrs.GetFreshnessCtrl().GetCurrFreshnessRelativeToMax(),
				m_CurrentStickY,
				spur ? "Spur " : "",
				(brake > 0.0f) ? "Brake" : "");
	}
#endif

// TODO: Perhaps re-enable this, if needed.
#if 0
	// Remember if we are braking and letting the brake clip play, so that we can
	// make sure to allow this to happen on the next frame, assuming that we still want
	// to slow down. /FF
	m_FlagClipatingBrake = msg.m_bAllowBrakeAndSpurAnimation && (brake > 0.0f);
#endif
}


bool CTaskMoveMountedSpurControl::ShouldUseHorseSim() const
{
	//	TUNE_BOOL(USE_HORSE_SIM, true);
	//	return USE_HORSE_SIM;
	return true;
}

#undef AI_RIDE_DBG_MSG

//////////////////////////////////////////////////////////////////////////
//
CTaskMoveMounted::CTaskMoveMounted()
:	CTaskMove(MOVEBLENDRATIO_STILL)
, m_fDesiredHeading(0.f)
, m_fTimeSinceSpur(LARGE_FLOAT)
, m_pAssistedMovementControl(NULL)
, m_fReverseDelay(0.f)
, m_fBuckDelay(0.0f)
, m_fSlowdownToStopTime(0.0f)
, m_fForcedStopTime(0.0f)
{
#if __PLAYER_HORSE_ASSISTED_MOVEMENT
		m_pAssistedMovementControl = rage_new CPlayerAssistedMovementControl;
		m_pAssistedMovementControl->SetScanForRoutesFrequency(0.1f); //faster scans for horses
		m_pAssistedMovementControl->SetUseWideLanes(true);
#endif
	ResetInput();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_MOUNTED);
}

CTaskMoveMounted::~CTaskMoveMounted()
{
#if __PLAYER_ASSISTED_MOVEMENT
	delete m_pAssistedMovementControl;
#endif
}

CTask::FSM_Return CTaskMoveMounted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* pPed = GetPed();

	FSM_Begin
		FSM_State(State_AcceptingInput)
			FSM_OnUpdate
				return StateAcceptingInput_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveMounted::StateAcceptingInput_OnUpdate(CPed* pPed)
{
    // Check if we are forcibly stopped from external code
    float fTimeRemainingToStayHalted = m_fForcedStopTime;
    if (fTimeRemainingToStayHalted > 0.0f)
    {
        m_bSpurRequested = false;
        m_fTimeSinceSpur = 0.f;
        ResetInput();
        pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
        pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f, 0.0f);
        pPed->SetDesiredHeading(pPed->GetCurrentHeading());
        float fNewTimeRemainingStayHalted = Max(fTimeRemainingToStayHalted - GetTimeStep(), 0.0f);
        m_fForcedStopTime = fNewTimeRemainingStayHalted;
        return FSM_Continue;
    }

    // Check if we are forcibly slowing down to a stop
    float fTimeRemainingToSlowdownAndStop = m_fSlowdownToStopTime;
    if (fTimeRemainingToSlowdownAndStop > 0.0f)
    {
        m_bSpurRequested = false;
        m_fTimeSinceSpur = 0.f;
        ResetInput();
        pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
        pPed->SetDesiredHeading(pPed->GetCurrentHeading());
        float fNewTimeRemainingToSlowdownAndStop = Max(fTimeRemainingToSlowdownAndStop - GetTimeStep(), 0.0f);
        m_fSlowdownToStopTime = fNewTimeRemainingToSlowdownAndStop;
        return FSM_Continue;
    }

	// If this feature is enabled
	TUNE_GROUP_BOOL(FOLLOW_NEAREST_PATH_HELPER, bFollowNearestPathOnHorse, true);
	if (bFollowNearestPathOnHorse)
	{
		// Should mount auto follow?
		const bool bMountShouldBeMoving = pPed->GetHorseComponent() && pPed->GetHorseComponent()->GetSpeedCtrl().GetTgtSpeed() > 0.0f;
		const bool bShouldMountFollowNearbyRoad = bMountShouldBeMoving && m_bIsAiming && m_vStick.IsZero() && m_fBrake < SMALL_FLOAT && !m_bHardBrake;
		
		// Is mount already auto-following?
		const bool bIsMountFollowingNearestRoad = GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOUNT_FOLLOW_NEAREST_ROAD;
		
		// If we need to start auto-following
		if (bShouldMountFollowNearbyRoad && !bIsMountFollowingNearestRoad)
		{
			// NOTE: Normally this task does NOT have a subtask.
			SetNewTask(rage_new CTaskMountFollowNearestRoad());
		}
		// If we should stop auto-following
		else if (!bShouldMountFollowNearbyRoad && bIsMountFollowingNearestRoad)
		{
			// NOTE: Normally this task does NOT have a subtask.
			SetFlag(aiTaskFlags::RemoveSubtask);
		}
	}

	float dt = TIME.GetSeconds();		
	bool bSpur = m_bSpurRequested;

	if (bSpur) 
	{
		m_bSpurRequested = false;
		m_fTimeSinceSpur = 0.f;		
	} 
	else
	{
		m_fTimeSinceSpur += dt;
	}

	// Let the movement system know that the player is in direct control of this ped
	Assert(pPed->GetHorseComponent());
	pPed->GetHorseComponent()->UpdateInput(bSpur, m_fTimeSinceSpur, m_bHardBrake, m_fBrake, m_bMaintainSpeed, m_bIsAiming, m_vStick, m_fMatchSpeed);

	float fDesiredNormalizedSpeed = pPed->GetHorseComponent()->GetSpeedCtrl().GetTgtSpeed();
	//Displayf("Speed: %f", speed);
	if (pPed->GetHorseComponent()->HasHeadingOverride())
	{
		//TMS: This is used for player control while in formation
		float fDesiredStick = pPed->GetHorseComponent()->GetHeadingOverride();
		const float TWO_PI = 2.f * PI;
		while (m_fDesiredHeading < -PI)
		{
			m_fDesiredHeading += TWO_PI;
		}
		while (m_fDesiredHeading > PI)
		{
			m_fDesiredHeading -= TWO_PI;
		}
		while (fDesiredStick < m_fDesiredHeading - PI)
		{
			fDesiredStick += TWO_PI;
		}
		while (fDesiredStick > m_fDesiredHeading + PI)
		{
			fDesiredStick -= TWO_PI;
		}

		//Need to lerp in the m_DesiredHeading...?
		m_fDesiredHeading = Lerp(pPed->GetHorseComponent()->GetHeadingOverrideStrength(), m_fDesiredHeading, fDesiredStick);
		while (m_fDesiredHeading < -PI)
		{
			m_fDesiredHeading += TWO_PI;
		}
		while (m_fDesiredHeading > PI)
		{
			m_fDesiredHeading -= TWO_PI;
		}
		pPed->GetHorseComponent()->ClearHeadingOverride();
	}

	pPed->GetMotionData()->SetDesiredMoveBlendRatio(fDesiredNormalizedSpeed * MOVEBLENDRATIO_SPRINT, 0.f);	
	if (fDesiredNormalizedSpeed != 0.f) //don't adjust heading when not moving (turn in place)
	{
		pPed->GetHorseComponent()->AdjustHeading(m_fDesiredHeading);
	}
	pPed->SetDesiredHeading(m_fDesiredHeading);
	
	float fCurrentMBR = pPed->GetMotionData()->GetCurrentMbrY();
	if(!pPed->GetPedResetFlag( CPED_RESET_FLAG_DontChangeHorseMbr ))
	{
		//reverse logic
		static const float REVERSE_TIMER = 0.5f;	
		if (m_bReverseRequested)
		{
			if (fCurrentMBR <= 0.f)
			{
				m_fReverseDelay -= dt;
			}
			else
			{
				m_fReverseDelay = REVERSE_TIMER;
			}

			if (m_fReverseDelay <= 0.f)
			{
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_REVERSE, 0.f);
			}
		}
		else
		{
			if (fCurrentMBR <= 0.f)
			{
				m_fReverseDelay = 0.f;
			}
		}
	}

	//Displayf("Agitation: %f", pPed->GetHorseComponent()->GetAgitationCtrl().GetCurrAgitation());
	static dev_float BUCKOFF_AGITATION_LIMIT = 1.0f;
	static dev_float BUCKOFF_AGITATION_RESET = 0.7f;
	CPed* pRider = pPed->GetHorseComponent()->GetRider();
	float fCurrentAgitation = pPed->GetHorseComponent()->GetAgitationCtrl().GetCurrAgitation();
	if (pRider && !pPed->GetHorseComponent()->GetAgitationCtrl().IsBuckingDisabled() && fCurrentAgitation >= BUCKOFF_AGITATION_LIMIT)
	{
		static dev_float BUCKOFF_TIMER = 1.0f;
		m_fBuckDelay+=dt;
		//kick my rider
		if (m_fBuckDelay >= BUCKOFF_TIMER && pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE) 
		{
			static_cast<CTaskHorseLocomotion*>(pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest())->RequestRearUp(true);							
		}		
	}
	else if (m_eJumpRequestType != eJR_None) 
	{
		if (fCurrentMBR <= 0 && pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE) 
		{
			static_cast<CTaskHorseLocomotion*>(pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest())->RequestRearUp();							
		} 
		else
		{
			fwFlags32 iflags = JF_DisableVault;
			if(m_eJumpRequestType == eJR_Auto)
			{
				iflags.SetFlag(JF_AutoJump);
			}

			aiTask* pNewTask = rage_new CTaskJumpVault(iflags); 
			pPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, pNewTask, PED_TASK_PRIORITY_PRIMARY);
			if (pRider)
			{
				aiTask* pRiderJumpTask = rage_new CTaskJumpMounted;
				pRider->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, pRiderJumpTask, PED_TASK_PRIORITY_PRIMARY);
			}

		}
	}
	else if (fCurrentAgitation <= BUCKOFF_AGITATION_RESET)
	{
		m_fBuckDelay=0.0f;
	}


#if __PLAYER_HORSE_ASSISTED_MOVEMENT
	if (pRider && pRider->IsLocalPlayer() && CPlayerAssistedMovementControl::ms_bAssistedMovementEnabled)
	{
		Vector2 vecStickInput; //not used		
		const float fPrevMbrSqr = pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2();
		const bool bForceScan = (fDesiredNormalizedSpeed > 0.f) && (fPrevMbrSqr == 0.f);
		m_pAssistedMovementControl->ScanForSnapToRoute(pPed, bForceScan);
		m_pAssistedMovementControl->Process(pPed, vecStickInput);
	}
#endif

	//Reset input states
	ResetInput();

	return FSM_Continue;
}



//////////////////////////////////////////////////////////////////////////
void CTaskMoveMounted::ApplyInput(float fDesiredHeading, bool bSpur, bool bHardBrake, float fBrake, bool bMaintainSpeed, bool bIsAiming, Vector2& vStick, float fMatchSpeed)
{
	m_bSpurRequested = m_bSpurRequested || bSpur;	
	m_fMatchSpeed = fMatchSpeed;
	m_vStick.Set(vStick);	
	m_bMaintainSpeed = bMaintainSpeed;
	m_bIsAiming = bIsAiming;
	m_bHardBrake = bHardBrake;
	m_fBrake = fBrake;
	m_fDesiredHeading = fDesiredHeading;
}

void CTaskMoveMounted::ResetInput()
{
	m_bSpurRequested = false;	
	m_fMatchSpeed = -1.f;
	m_vStick.Zero();	
	m_bMaintainSpeed = false;
	m_bIsAiming = false;
	m_bHardBrake = false;
	m_fBrake = 0;
	m_eJumpRequestType = eJR_None;
	m_bReverseRequested = false;
}

void CTaskMoveMounted::RequestJump(eJumpRequestType requestType)
{
	m_eJumpRequestType = requestType;
}

void CTaskMoveMounted::RequestReverse()
{
	m_bReverseRequested = true;
}

void CTaskMoveMounted::RequestSlowdownToStop( float fTimeToHalt )
{
    m_fSlowdownToStopTime = fTimeToHalt;
}

void CTaskMoveMounted::RequestForcedStop( float fTimeToStayHalted )
{
    m_fForcedStopTime = fTimeToStayHalted;
}

//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CTaskJumpMounted
//////////////////////////////////////////////////////////////////////////
const fwMvFloatId	CTaskJumpMounted::ms_MoveSpeedId("Speed",0xF997622B);
const fwMvBooleanId CTaskJumpMounted::ms_ExitLandId("ExitLand",0x57C11B06);					// Set true when we exit Land
const fwMvFlagId	CTaskJumpMounted::ms_FallingFlagId("bFalling",0x35840204);

CTaskJumpMounted::CTaskJumpMounted()
{
	SetInternalTaskType(CTaskTypes::TASK_JUMP_MOUNTED);
}

CTask::FSM_Return CTaskJumpMounted::ProcessPreFSM()
{
	if (GetPed()->GetMyMount() == NULL)
	{
		//STOP
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskJumpMounted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnEnter
			return StateInit_OnEnter();
		FSM_OnUpdate
			return StateInit_OnUpdate();
		FSM_State(State_Launch)
			FSM_OnEnter
			return StateLaunch_OnEnter();
		FSM_OnUpdate
			return StateLaunch_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;
	FSM_End

}

CTask::FSM_Return CTaskJumpMounted::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	return UpdateFSM(iState, iEvent); 
}

CTaskInfo* CTaskJumpMounted::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

CTaskJumpMounted::FSM_Return CTaskJumpMounted::StateInit_OnEnter()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();

	if (!pMount) 
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	int iSeatIndex = pMount->GetSeatManager()->GetPedsSeatIndex(pPed); 
	const CVehicleSeatAnimInfo* pSeatClipInfo = pMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(iSeatIndex);
	if (!pSeatClipInfo)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}
	m_clipSetId	= pSeatClipInfo->GetSeatClipSetId();		
	m_networkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskJumpMounted );

	return FSM_Continue;
}

CTaskJumpMounted::FSM_Return CTaskJumpMounted::StateInit_OnUpdate()
{
	CPed* pPed = GetPed();
	if(!m_networkHelper.IsNetworkActive())
	{
		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
		taskAssertf(pSet, "Can't find clip set! Clip ID: %s", m_clipSetId.GetCStr());
		
		if (pSet->Request_DEPRECATED() && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskJumpMounted))
		{
			m_networkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskJumpMounted);
			m_networkHelper.SetClipSet(m_clipSetId);

			// Attach it to an empty precedence slot in fwMove			
			pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), NORMAL_BLEND_DURATION);			
		}
	}

	if(m_networkHelper.IsNetworkActive() && m_networkHelper.IsNetworkAttached())
	{
		SetState(State_Launch);
	}

	return FSM_Continue;
}

CTaskJumpMounted::FSM_Return CTaskJumpMounted::StateLaunch_OnEnter()
{
	return FSM_Continue;
}

CTaskJumpMounted::FSM_Return CTaskJumpMounted::StateLaunch_OnUpdate()
{
	SendMoveParams();
	if( m_networkHelper.GetBoolean(ms_ExitLandId) == true )
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

void CTaskJumpMounted::CleanUp()
{
	CPed* pPed = GetPed();

	// Blend out the move network
	if (m_networkHelper.IsNetworkAttached())
	{		
		pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, REALLY_SLOW_BLEND_DURATION);
	}	

	// Base class
	CTask::CleanUp();
}

void CTaskJumpMounted::SendMoveParams() 
{
	CPed* pMount = GetPed()->GetMyMount();
	taskAssert(pMount != NULL);
	float fSpeed = pMount->GetMotionData()->GetCurrentMbrY() / MOVEBLENDRATIO_SPRINT;
	m_networkHelper.SetFloat( ms_MoveSpeedId, fSpeed);
	CTask* pJumpTask = pMount->GetPedIntelligence()->GetTaskActive();
	if (pJumpTask && pJumpTask->GetTaskType() == CTaskTypes::TASK_JUMPVAULT)
		m_networkHelper.SetFlag(static_cast<CTaskJumpVault*>(pJumpTask)->GetIsFalling(), ms_FallingFlagId);		
	else
		m_networkHelper.SetFlag(false, ms_FallingFlagId);	
}

#if !__FINAL
const char * CTaskJumpMounted::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Init && iState <= State_Finish);

	switch (iState)
	{
	case State_Init:							return "State_Init";
	case State_Launch:							return "State_Launch";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}
	return "State_Invalid";
}
#endif //!__FINAL
//////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE