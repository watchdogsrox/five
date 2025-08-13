// FILE :    TaskShellShocked.cpp
// PURPOSE : Combat subtask in control of being shell-shocked
// CREATED : 3/12/2012

// File header
#include "Task/Combat/Subtasks/TaskShellShocked.h"

// Game headers
#include "animation/EventTags.h"
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "event/EventDamage.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "weapons/inventory/PedWeaponManager.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskShellShocked
//=========================================================================

//Static Initialization -- MoVE
fwMvBooleanId CTaskShellShocked::ms_StaggerClipEndedId("StaggerClipEnded",0xD7D657B9);

fwMvClipId CTaskShellShocked::ms_StaggerClipId("StaggerClip",0xFEBBD768);

fwMvFlagId CTaskShellShocked::ms_ShortId("Short",0x9D43FEE8);
fwMvFlagId CTaskShellShocked::ms_LongId("Long",0xDAA2D5EC);
fwMvFlagId CTaskShellShocked::ms_FrontId("Front",0x89F230A2);
fwMvFlagId CTaskShellShocked::ms_BackId("Back",0x37418674);
fwMvFlagId CTaskShellShocked::ms_LeftId("Left",0x6FA34840);
fwMvFlagId CTaskShellShocked::ms_RightId("Right",0xB8CCC339);

fwMvFloatId CTaskShellShocked::ms_RateId("Rate",0x7E68C088);
fwMvFloatId CTaskShellShocked::ms_PhaseId("Phase",0xA27F482B);

fwMvRequestId CTaskShellShocked::ms_StaggerId("Stagger",0xEE57898E);

CTaskShellShocked::CTaskShellShocked(Vec3V_In vPosition, Duration nDuration, fwFlags8 uConfigFlags)
: m_ClipSetRequestHelper()
, m_MoveNetworkHelper()
, m_vPosition(vPosition)
, m_nDuration(nDuration)
, m_nDirection(CDirectionHelper::D_Invalid)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_uConfigFlags(uConfigFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_SHELL_SHOCKED);
}

CTaskShellShocked::~CTaskShellShocked()
{
}

void CTaskShellShocked::CleanUp()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is valid.
	if(pPed)
	{
		//Check if the move network is active.
		if(m_MoveNetworkHelper.IsNetworkActive())
		{	
			//Clear the move network.
			pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
		}
	}
}

aiTask* CTaskShellShocked::Copy() const
{
	return rage_new CTaskShellShocked(m_vPosition, m_nDuration, m_uConfigFlags);
}

bool CTaskShellShocked::IsValid(const CPed& rPed, Vec3V_In vPosition)
{
	//Ensure we are not running normally.
	if(!rPed.GetMotionData()->GetIsLessThanRun() && !rPed.IsStrafing())
	{
		return false;
	}

	//Ensure we are not fleeing.
	if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true) ||
		rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_REACT_AND_FLEE, true))
	{
		return false;
	}

	//Ensure the clip set is valid.
	if(ChooseClipSet(rPed) == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	//Ensure the direction is valid.
	if(ChooseDirection(rPed, vPosition, false) == CDirectionHelper::D_Invalid)
	{
		return false;
	}
	
	return true;
}

#if !__FINAL
const char* CTaskShellShocked::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Stream",
		"State_Stagger",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif

CTask::FSM_Return CTaskShellShocked::ProcessPreFSM()
{
	//Process the streaming.
	ProcessStreaming();
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskShellShocked::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Stream)
			FSM_OnUpdate
				return Stream_OnUpdate();
				
		FSM_State(State_Stagger)
			FSM_OnEnter
				Stagger_OnEnter();
			FSM_OnUpdate
				return Stagger_OnUpdate();
		
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
		
	FSM_End
}

void CTaskShellShocked::Start_OnEnter()
{
	//Choose the clip set.
	m_ClipSetId = ChooseClipSet(*GetPed());

	//Choose the direction.
	m_nDirection = ChooseDirection(*GetPed(), m_vPosition, true);
}

CTask::FSM_Return CTaskShellShocked::Start_OnUpdate()
{
	//Ensure the clip set is valid.
	if(m_ClipSetId == CLIP_SET_ID_INVALID)
	{
		return FSM_Quit;
	}

	//Ensure the direction is valid.
	if(m_nDirection == CDirectionHelper::D_Invalid)
	{
		return FSM_Quit;
	}
	
	//Move to the stream state.
	SetState(State_Stream);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskShellShocked::Stream_OnUpdate()
{
	//Stream the MoVE network.
	bool bNetworkIsStreamedIn = fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskShellShocked);

	//Stream the clip set.
	bool bClipSetIsStreamedIn = m_ClipSetRequestHelper.IsLoaded();
	
	//Check if everything is streamed in.
	if(bNetworkIsStreamedIn && bClipSetIsStreamedIn)
	{
		//Grab the ped.
		CPed* pPed = GetPed();

		//Set up the move network on the ped.
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskShellShocked);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), NORMAL_BLEND_DURATION);
		
		//Set the clip set.
		m_MoveNetworkHelper.SetClipSet(m_ClipSetId);
		
		//Move to the stagger state.
		SetState(State_Stagger);
	}

	return FSM_Continue;
}

void CTaskShellShocked::Stagger_OnEnter()
{
	//Check if we should make the ped fatigued.
	if(ShouldMakeFatigued())
	{
		//Make the ped fatigued.
		MakeFatigued();
	}

	//Set the duration.
	switch(m_nDuration)
	{
		case Duration_Short:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_ShortId);
			break;
		}
		case Duration_Long:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_LongId);
			break;
		}
		default:
		{
			taskAssertf(false, "Unknown duration: %d.", m_nDuration);
			break;
		}
	}
	
	//Set the rate.
	static dev_float s_fMinRate = 0.8f;
	static dev_float s_fMaxRate = 1.2f;
	fwRandom::SetRandomSeed(GetPed()->GetRandomSeed());
	m_MoveNetworkHelper.SetFloat(ms_RateId, fwRandom::GetRandomNumberInRange(s_fMinRate, s_fMaxRate));
	
	//Check the direction.
	switch(m_nDirection)
	{
		case CDirectionHelper::D_FrontLeft:
		case CDirectionHelper::D_FrontRight:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_FrontId);
			break;
		}
		case CDirectionHelper::D_BackLeft:
		case CDirectionHelper::D_BackRight:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_BackId);
			break;
		}
		case CDirectionHelper::D_Left:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_LeftId);
			break;
		}
		case CDirectionHelper::D_Right:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_RightId);
			break;
		}
		default:
		{
			taskAssertf(false, "Unknown direction: %d.", m_nDirection);
			break;
		}
	}
	
	//Send the stagger request.
	m_MoveNetworkHelper.SendRequest(ms_StaggerId);
	
	//Wait for the stagger clip to end.
	m_MoveNetworkHelper.WaitForTargetState(ms_StaggerClipEndedId);
}

CTask::FSM_Return CTaskShellShocked::Stagger_OnUpdate()
{
	//Check if the target state has been reached, or we should interrupt.
	if(m_MoveNetworkHelper.IsInTargetState() || ShouldInterrupt())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskShellShocked::MakeFatigued()
{
	taskAssert(ShouldMakeFatigued());

	//Ensure the ped is not already fatigued.
	float fHealth = GetPed()->GetHealth();
	float fThreshold = GetPed()->GetFatiguedHealthThreshold();
	float fDifference = fHealth - fThreshold;
	if(fDifference <= 0.0f)
	{
		return;
	}

	//Create the damage event.
	CEventDamage tempDamageEvent(NULL, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_FALL);

	//Apply the damage.
	float fDamage = fDifference + 1.0f;
	CPedDamageCalculator damageCalculator(NULL, fDamage, WEAPONTYPE_FALL, RAGDOLL_HEAD, false);
	damageCalculator.ApplyDamageAndComputeResponse(GetPed(), tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);
}

void CTaskShellShocked::ProcessStreaming()
{
	//Stream the clip set.
	m_ClipSetRequestHelper.Request(m_ClipSetId);
}

bool CTaskShellShocked::ShouldInterrupt() const
{
	//Ensure the flag is set.
	if(!m_uConfigFlags.IsFlagSet(CF_Interrupt))
	{
		return false;
	}
	
	//Ensure the clip is valid.
	const crClip* pClip = m_MoveNetworkHelper.GetClip(ms_StaggerClipId);
	if(!pClip)
	{
		return false;
	}
	
	//Find the event phase.
	float fInterruptiblePhase;
	if(!CClipEventTags::FindEventPhase(pClip, CClipEventTags::Interruptible, fInterruptiblePhase))
	{
		return false;
	}
	
	//Ensure the phase has exceeded the interruptible phase.
	float fPhase = m_MoveNetworkHelper.GetFloat(ms_PhaseId);
	if(fPhase < fInterruptiblePhase)
	{
		return false;
	}
	
	return true;
}

bool CTaskShellShocked::ShouldMakeFatigued() const
{
	//Ensure the config flag is not set.
	if(m_uConfigFlags.IsFlagSet(CF_DisableMakeFatigued))
	{
		return false;
	}

	//Ensure the ped is random.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	return true;
}

fwMvClipSetId CTaskShellShocked::ChooseClipSet(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	//Ensure the equipped weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	return pWeaponInfo->GetShellShockedClipSetId(rPed);
}

CDirectionHelper::Direction CTaskShellShocked::ChooseDirection(const CPed& rPed, Vec3V_In vPosition, bool bUpdateTime)
{
	//Calculate the closest direction.
	CDirectionHelper::Direction nDirection = CDirectionHelper::CalculateClosestDirection(
		rPed.GetTransform().GetPosition(), rPed.GetCurrentHeading(), vPosition);
	taskAssert(nDirection != CDirectionHelper::D_Invalid);

	//Ensure the ped is on-screen.
	if(!rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		return nDirection;
	}

	//Get the left/right directions.
	CDirectionHelper::Direction nDirectionToLeft	= CDirectionHelper::MoveDirectionToLeft(nDirection);
	CDirectionHelper::Direction nDirectionToRight	= CDirectionHelper::MoveDirectionToRight(nDirection);
	taskAssert(nDirectionToLeft		!= CDirectionHelper::D_Invalid);
	taskAssert(nDirectionToRight	!= CDirectionHelper::D_Invalid);

	//Choose the direction that was used furthest in the past.
	static u32 s_uTimes[CDirectionHelper::D_Max];
	static bool s_bInitialized = false;
	if(!s_bInitialized)
	{
		s_bInitialized = true;

		for(int i = 0; i < CDirectionHelper::D_Max; ++i)
		{
			s_uTimes[i] = 0;
		}
	}

	//Choose the best time.
	u32 uTime			= s_uTimes[nDirection];
	u32 uTimeToLeft		= s_uTimes[nDirectionToLeft];
	u32 uTimeToRight	= s_uTimes[nDirectionToRight];
	u32 uTimeBest		= Min(Min(uTime, uTimeToLeft), uTimeToRight);

	//Choose a direction.
	static const int s_iMaxDirections = 3;
	CDirectionHelper::Direction aDirections[s_iMaxDirections];
	int iNumDirections = 0;

	//Check the directions.
	if(uTimeBest == uTime)
	{
		aDirections[iNumDirections++] = nDirection;
	}
	else if(uTimeBest == uTimeToLeft)
	{
		aDirections[iNumDirections++] = nDirectionToLeft;
	}
	else if(uTimeBest == uTimeToRight)
	{
		aDirections[iNumDirections++] = nDirectionToRight;
	}

	//Ensure the directions are valid.
	if(iNumDirections <= 0)
	{
		return nDirection;
	}

	//Choose a direction.
	fwRandom::SetRandomSeed(rPed.GetRandomSeed());
	int iIndex = fwRandom::GetRandomNumberInRange(0, iNumDirections);
	CDirectionHelper::Direction nDirectionToUse = aDirections[iIndex];

	//Set the time.
	if(bUpdateTime && (nDirectionToUse != CDirectionHelper::D_Invalid))
	{
		s_uTimes[nDirectionToUse] = fwTimer::GetTimeInMilliseconds();
	}

	return nDirectionToUse;
}
