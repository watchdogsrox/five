// Framework headers
#include "ai/task/taskchannel.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Random.h"
#include "fwmaths/Vector.h"

// Game headers
#include "ai\debug\system\AIDebugLogManager.h"
#include "animation/AnimDefines.h"
#include "fwanimation/animmanager.h"

#include "audio/scannermanager.h"
#include "camera/CamInterface.h"
#include "Control/Garages.h"
#include "Debug/DebugScene.h"
#include "Event/Event.h"
#include "Network/Live/NetworkTelemetry.h"
#include "debug/DebugScene.h"
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Event/EventGroup.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "Peds/PopCycle.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Script/Script.h"
#include "script/script_cars_and_peds.h"
#include "Streaming/Streaming.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskSecondary.h"
#include "Task/General/TaskBasic.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Bike.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#if __PPU
#include "Vehicles/Heli.h"
#endif

// network includes
#include "Network/NetworkInterface.h"

//Rage headers
#include "crSkeleton/skeleton.h"

AI_OPTIMISATIONS()

extern audScannerManager g_AudioScannerManager;

////////////////////////////
//SET PED IN AS PASSENGER
////////////////////////////

CTaskSetPedInVehicle::CTaskSetPedInVehicle(CVehicle* pTargetVehicle, const s32 iTargetSeat, const u32 iFlags)
: m_pTargetVehicle(pTargetVehicle)
, m_targetSeat(iTargetSeat)
, m_iFlags(iFlags)
{
	taskAssertf(m_pTargetVehicle, "Target vehicle was NULL in CTaskSetPedInVehicle");
	SetInternalTaskType(CTaskTypes::TASK_SET_PED_IN_VEHICLE);
}

CTaskSetPedInVehicle::~CTaskSetPedInVehicle()
{
}


CTask::FSM_Return CTaskSetPedInVehicle::Start_OnUpdate( CPed* pPed )
{
	if (!taskVerifyf(m_pTargetVehicle, "Target vehicle was NULL in CTaskSetPedInVehicle::Start_OnUpdate"))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Don't count as drowning when entering a vehicle
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsDrowning, false );

	SetState(State_SetPedIn);
	return FSM_Continue;
}

void CTaskSetPedInVehicle::SetPedIn_OnEnter( CPed* pPed )
{
	if (m_iFlags & CPed::PVF_KeepCurrentOffset)
	{
		// Cache the current position, we'll reapply the position after we're put into the vehicle
		const Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		const QuatV qPedRotation = pPed->GetTransform().GetOrientation();
		AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskSetPedInVehicle::SetPedIn_OnEnter] - Calling set ped into vehicle for ped %s, keep current offset\n", AILogging::GetDynamicEntityNameSafe(pPed));
		pPed->SetPedInVehicle(m_pTargetVehicle, m_targetSeat, m_iFlags);
		// Reapply
		CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, vPedPosition, qPedRotation);
	}
	else
	{
		AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskSetPedInVehicle::SetPedIn_OnEnter] - Calling set ped into vehicle for ped %s\n", AILogging::GetDynamicEntityNameSafe(pPed));
		pPed->SetPedInVehicle(m_pTargetVehicle, m_targetSeat, m_iFlags);
	}

	if(pPed->IsLocalPlayer())
	{
		g_AudioScannerManager.SetTimeToPlayCDIVehicleLine();
	}
	
	// need to set the move blender for network clones here to avoid glitches while
	// waiting for the move blender to be updated over the network
}

CTask::FSM_Return CTaskSetPedInVehicle::SetPedIn_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	SetState(State_Finish);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSetPedInVehicle::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
		// Start
	FSM_State(State_Start)
		FSM_OnUpdate
			return Start_OnUpdate(pPed);

	// Check for the condition to be true
	FSM_State(State_SetPedIn)
		FSM_OnEnter
			SetPedIn_OnEnter(pPed);
		FSM_OnUpdate
			return SetPedIn_OnUpdate(pPed);

	// exit
	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;
FSM_End
}


#if !__FINAL
const char * CTaskSetPedInVehicle::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_SetPedOut",
		"State_Finish"
	};
	return aStateNames[iState];
}

#endif // !__FINAL

////////////////////
//SET PED OUT
////////////////////

CTaskSetPedOutOfVehicle::CTaskSetPedOutOfVehicle(const u32 iFlags)
: m_iFlags(iFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_SET_PED_OUT_OF_VEHICLE);
}

CTaskSetPedOutOfVehicle::~CTaskSetPedOutOfVehicle()
{
}


CTask::FSM_Return CTaskSetPedOutOfVehicle::Start_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	SetState(State_SetPedOut);
	return FSM_Continue;
}

void CTaskSetPedOutOfVehicle::SetPedOut_OnEnter( CPed* pPed )
{
	pPed->SetPedOutOfVehicle(m_iFlags);
}

CTask::FSM_Return CTaskSetPedOutOfVehicle::SetPedOut_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	SetState(State_Finish);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSetPedOutOfVehicle::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	// Start
	FSM_State(State_Start)
		FSM_OnUpdate
			return Start_OnUpdate(pPed);

	// Check for the condition to be true
	FSM_State(State_SetPedOut)
		FSM_OnEnter
		SetPedOut_OnEnter(pPed);
	FSM_OnUpdate
		return SetPedOut_OnUpdate(pPed);

	// exit
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}


#if !__FINAL
const char * CTaskSetPedOutOfVehicle::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_SetPedIn",
		"State_Finish"
	};

	return aStateNames[iState];
}

#endif // !__FINAL

CTaskSmashCarWindow::CTaskSmashCarWindow( bool bSmashWindscreen )
: m_bSmashWindscreen(bSmashWindscreen),
m_windowToSmash(VEH_INVALID_ID),
m_iClipHash(0),
m_iClipDictIndex(-1),
m_fRandomAIDelayTimer(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_SMASH_CAR_WINDOW);
}


bool CTaskSmashCarWindow::GetWindowToSmash( const CPed* pPed, bool bWindScreen, u32& iClipHash, s32& iDictIndex, eHierarchyId& window, bool bIgnoreCracks )
{
	// JP:
	// Vehicle materials are not set up correctly so need to disable all window smashing
	static bool bDisableWindowSmashing = false;
	if(bDisableWindowSmashing)
		return false;

	iClipHash = 0;
	iDictIndex = -1;
	
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if( pVehicle == NULL )
	{
		return false;
	}

	// Get the window and clip for this window
	s32 seat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

	// Want an clip to play and a window to smash
	// If we find neither then return false

	// Find window
	// Lookup door id
	int iEntryPointIndex = -1;
	for(int i = 0; i < pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumberEntryExitPoints(); i++)
	{
		const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(i);
		Assert(pEntryPoint);
		
		// Don't check entrypoints with multiple seats as windows are linked to a single entry point
		if (pEntryPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle)
		{
			if(seat == pEntryPoint->GetSeat(SA_directAccessSeat))
			{
				iEntryPointIndex = i;
				break;
			}
		}
	}

	// Did we find a door?
	if(iEntryPointIndex == -1)
	{
		return false;
	}

	const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex);
	Assertf(pEntryPointInfo, "Vehicle is missing entry point");

	window = pEntryPointInfo->GetWindowId();

	if(window == VEH_INVALID_ID)
	{
		return false;
	}

	const int windowBoneIndex = pVehicle->GetBoneIndex(window);
	if(windowBoneIndex < 0)
	{
		return false;
	}
	
	// Found a valid window

	// find clip
	const CVehicleSeatAnimInfo* pSeatInfo = pVehicle->GetSeatAnimationInfo(seat);
	Assert(pSeatInfo);

	// Disable window smashing for vehicles using old clips
	if (pSeatInfo->GetSeatClipSetId().GetHash() == 0)
	{
		return false;
	}

	bool bFirstPerson = pPed->IsInFirstPersonVehicleCamera();

	pSeatInfo->FindAnim(bWindScreen ? CVehicleSeatAnimInfo::SMASH_WINDSCREEN : CVehicleSeatAnimInfo::SMASH_WINDOW, iDictIndex, iClipHash, bFirstPerson);

	// make sure there's a window to smash
	if(!pVehicle->HasWindowToSmash(window, bIgnoreCracks))
	{
		return false;
	}

	if (bIgnoreCracks)
	{
		return iDictIndex != -1;
	}

	bool bWindowToSmash = false;
	const int windowBoneId = pVehicle->GetBoneIndex(window);
	if(pVehicle->GetFragInst() && windowBoneId!=-1)
	{
		s32 iWindowComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex( windowBoneId );
		if(iWindowComponent != -1)
		{
			// Hack because the glass doesn't seem to null the window component
			if (!pVehicle->IsBrokenFlagSet(iWindowComponent))
			{
				bWindowToSmash = true;
			}
		}
	}

	return iDictIndex != -1 && bWindowToSmash;
}

CTask::FSM_Return CTaskSmashCarWindow::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Init)
		FSM_OnUpdate
			return Init_OnUpdate(pPed);

	FSM_State(State_StreamingClips)
		FSM_OnUpdate
			return StreamingClips_OnUpdate(pPed);

	FSM_State(State_PlaySmashClip)
		FSM_OnEnter
			PlaySmashClip_OnEnter(pPed);
		FSM_OnUpdate
			return PlaySmashClip_OnUpdate(pPed);

	FSM_State(State_Finish)
		FSM_OnUpdate
			return Finish_OnUpdate(pPed);
FSM_End
}

CTask::FSM_Return CTaskSmashCarWindow::Init_OnUpdate(CPed* pPed)
{
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!taskVerifyf(pVehicle,"NULL vehicle in CTaskSmashCarWindow"))
	{
		return FSM_Quit;
	}

	if( !GetWindowToSmash( pPed, m_bSmashWindscreen, m_iClipHash, m_iClipDictIndex, m_windowToSmash ) )
	{
		return FSM_Quit;
	}

	static dev_float s_fAITimeDelayRange = 2.0f;
	if (!pPed->IsPlayer())
		m_fRandomAIDelayTimer = fwRandom::GetRandomNumberInRange(0.0f, s_fAITimeDelayRange);

	SetState(State_StreamingClips);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSmashCarWindow::StreamingClips_OnUpdate(CPed* pPed)
{
	if(taskVerifyf(m_iClipDictIndex != -1 && m_iClipHash!= 0,"Invalid clip data"))
	{
		if (!pPed->IsPlayer() && GetTimeRunning() < m_fRandomAIDelayTimer)
		{
			return FSM_Continue;
		}
		if(m_clipStreamer.RequestClipsByIndex(m_iClipDictIndex))
		{
			SetState(State_PlaySmashClip);			
		}
		return FSM_Continue;
	}
	else
	{
		// Invalid clips
		return FSM_Quit;
	}
}

void CTaskSmashCarWindow::PlaySmashClip_OnEnter(CPed* pPed)
{
	if(taskVerifyf(m_iClipDictIndex != -1 && m_iClipHash!= 0,"Invalid clip data"))
	{
		StartClipByDictAndHash(pPed, m_iClipDictIndex, m_iClipHash, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, AP_HIGH, 4.0f, CClipHelper::TerminationType_OnFinish);
	}
}

CTask::FSM_Return CTaskSmashCarWindow::PlaySmashClip_OnUpdate(CPed* pPed)
{
	if(!GetClipHelper() || GetIsFlagSet(aiTaskFlags::AnimFinished))
	{
		// Smash clip finished
		SetState(State_Finish);
		return FSM_Continue;
	}

	CVehicle* pVehicle = pPed->GetMyVehicle();

	// Smash the windscreen if you hit a particular event
	if( GetClipHelper()->IsEvent(CClipEventTags::Smash) && pVehicle )
	{
		pVehicle->SmashWindow(m_windowToSmash, VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW);

		// If there's any mods attached to the window (i.e. rally netting), hide them
		s8 dummyValue = -1;
		const int windowBoneId = pVehicle->GetBoneIndex(m_windowToSmash);
		pVehicle->GetVehicleDrawHandler().GetVariationInstance().HideModOnBone(pVehicle, windowBoneId, dummyValue);
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskSmashCarWindow::GetStaticStateName( s32 iState )
{
	static const char* sStateNames[] = 
	{
		"Init",
		"StreamingAnims",
		"PlaySmashAnim",
		"Finish"
	};
	taskFatalAssertf(iState < 4,"invalid state for state name array");

	return sStateNames[iState];
}
#endif


CTask::FSM_Return CTaskWaitForCondition::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	// Start
	FSM_State(State_start)
		FSM_OnUpdate
			return Start_OnUpdate(pPed);

	// Check for the condition to be true
	FSM_State(State_checkCondition)
		FSM_OnEnter
			CheckCondition_OnEnter(pPed);
		FSM_OnUpdate
			return CheckCondition_OnUpdate(pPed);

	// exit
	FSM_State(State_exit)
		FSM_OnUpdate
			return FSM_Quit;
	FSM_End

}

CTask::FSM_Return CTaskWaitForCondition::Start_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	SetState(State_checkCondition);
	return FSM_Continue;
}

void CTaskWaitForCondition::CheckCondition_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
// 	static const u32 iContextHash = ATSTRINGHASH("Scenario", 0xE5191663);
// 	SetNewTask(rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_playIdleClips, iContextHash));
}

CTask::FSM_Return CTaskWaitForCondition::CheckCondition_OnUpdate( CPed* pPed )
{
	if( CheckCondition(pPed) )
	{
		SetState(State_exit);
		return FSM_Continue;
	}
	return FSM_Continue;
}
#if !__FINAL
const char * CTaskWaitForCondition::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_start&&iState<=State_exit);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_checkCondition",
		"State_exit"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

extern const u32 g_PainVoice;

bool CTaskWaitForSteppingOut::CheckCondition( CPed* pPed )
{
	if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || !pPed->GetMyVehicle() )
		return true;

	bool bPanic = false;
	if( pPed->GetPedType() != PEDTYPE_COP && pPed->PopTypeIsRandom() )
	{
		// this seems to trigger for buddies in helis, so let's just not allow helis (or trains)
		if( pPed->GetMyVehicle()->GetDriver() && pPed->GetMyVehicle()->GetDriver()->IsPlayer() )
		{
			bPanic = true;
		}
		else if( CGameWorld::FindLocalPlayer() )
		{
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if( pPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
			{
				CVehicle* pVehicle	= (CVehicle*)(pPlayer->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType( CTaskTypes::TASK_ENTER_VEHICLE));
				if( pVehicle == pPed->GetMyVehicle())
				{
					bPanic = true;
				}
			}
		}
	}

	// this seems to trigger for buddies in helis, so let's just not allow helis (or trains)
	if (bPanic && pPed->GetMyVehicle()->GetVehicleType()!=VEHICLE_TYPE_HELI && pPed->GetMyVehicle()->GetVehicleType()!=VEHICLE_TYPE_TRAIN)
	{
		if(!pPed->NewSay("TRAPPED"))
		{
			pPed->NewSay("PANIC", g_PainVoice, false, true);
		}
	}

	if( pPed->GetMyVehicle()->CanPedStepOutCar(pPed, false, !pPed->IsPlayer()) )
	{
		// Stay in for an extra amount of time
		if( m_fWaitTime > 0.0f )
		{
			m_fWaitTime -= GetTimeStep();
			if( m_fWaitTime > 0.0f )
			{
				return false;
			}
		}
		return true;
	}


	return false;
}


bool CTaskWaitForMyCarToStop::CheckCondition (CPed* pPed)
{
	if( !pPed->GetMyVehicle() ||
		!pPed->GetMyVehicle()->GetDriver() ||
		pPed->GetMyVehicle()->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) )
	{
		return true;
	}
	return pPed->GetMyVehicle()->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_WAIT_FOR_SEAT_TO_BE_FREE);
}
