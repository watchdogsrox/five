// File header
#include "Task/Movement/TaskParachuteObject.h"
#include "Task/Movement/TaskParachute.h"

// Game headers
#include "animation/MoveObject.h"
#include "Network/NetworkInterface.h"
#include "Objects/object.h"

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskParachuteObject
//////////////////////////////////////////////////////////////////////////

const fwMvFloatId CTaskParachuteObject::ms_BlendXAxisTurns("BlendXAxisTurns",0xB57F83B2);
const fwMvFloatId CTaskParachuteObject::ms_BlendYAxisTurns("BlendYAxisTurns",0xBD35F97F);
const fwMvFloatId CTaskParachuteObject::ms_BlendXAndYAxesTurns("BlendXAndYAxesTurns",0x7EC6CC8D);
const fwMvFloatId CTaskParachuteObject::ms_BlendIdleAndMotion("BlendIdleAndMotion",0xA2048626);
const fwMvFloatId CTaskParachuteObject::ms_Phase("Phase",0xA27F482B);

const fwMvRequestId CTaskParachuteObject::ms_Deploy("Deploy",0xC9200261);
const fwMvRequestId CTaskParachuteObject::ms_Idle("Idle",0x71C21326);
const fwMvRequestId CTaskParachuteObject::ms_Crumple("Crumple",0x8415F0E7);

const fwMvBooleanId CTaskParachuteObject::ms_Deploy_OnEnter("Deploy_OnEnter",0x1F9DC4D6);
const fwMvBooleanId CTaskParachuteObject::ms_DeployClipEnded("DeployClipEnded",0xB4DA4E24);
const fwMvBooleanId CTaskParachuteObject::ms_Idle_OnEnter("Idle_OnEnter",0xCA902721);
const fwMvBooleanId CTaskParachuteObject::ms_Crumple_OnEnter("Crumple_OnEnter",0x393E61E7);
const fwMvBooleanId CTaskParachuteObject::ms_CrumpleClipEnded("CrumpleClipEnded",0x19E59FCB);

CTaskParachuteObject::Tunables CTaskParachuteObject::ms_Tunables;
IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskParachuteObject, 0x5ded8b11);

CTaskParachuteObject::CTaskParachuteObject() 
: m_MoveNetworkHelper()
, m_ClipSetRequestHelper()
, m_uFlags(0)
, m_hasAlreadyDeployed(false)
{
	SetInternalTaskType(CTaskTypes::TASK_PARACHUTE_OBJECT);
}

CTaskParachuteObject::~CTaskParachuteObject()
{
}

void CTaskParachuteObject::CleanUp()
{
	//Grab the object.
	CObject* pObject = GetObject();
	
	//Check if the move network is active.
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		//This is apparently needed to avoid crashes.
		if(pObject->GetAnimDirector())
		{
			//Clear the network.
			pObject->GetMoveObject().ClearNetwork(m_MoveNetworkHelper, 0.0f);
		}
	}
}

aiTask*	CTaskParachuteObject::Copy() const
{
	return rage_new CTaskParachuteObject();
}

bool CTaskParachuteObject::CanDeploy() const
{
	return (m_uFlags.IsFlagSet(PORF_CanDeploy));
}

void CTaskParachuteObject::Crumple()
{
	//Set the crumple flag.
	m_uFlags.SetFlag(PORF_Crumple);
}

void CTaskParachuteObject::Deploy()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Set the deploy flag.
	m_uFlags.SetFlag(PORF_Deploy);
}

bool CTaskParachuteObject::IsOut() const
{
	return (m_uFlags.IsFlagSet(PORF_IsOut));
}

void CTaskParachuteObject::ProcessMoveParameters(float fBlendXAxisTurns, float fBlendYAxisTurns, float fBlendXAndYAxesTurns, float fBlendIdleAndMotion)
{
	if(!m_MoveNetworkHelper.IsNetworkAttached())
	{
		return;
	}

	//Set the move parameters.
	m_MoveNetworkHelper.SetFloat(ms_BlendXAxisTurns,		fBlendXAxisTurns);
	m_MoveNetworkHelper.SetFloat(ms_BlendYAxisTurns,		fBlendYAxisTurns);
	m_MoveNetworkHelper.SetFloat(ms_BlendXAndYAxesTurns,	fBlendXAndYAxesTurns);
	m_MoveNetworkHelper.SetFloat(ms_BlendIdleAndMotion,		fBlendIdleAndMotion);
}

void CTaskParachuteObject::TaskSetState(const s32 iState)
{
	aiTask::SetState(iState);
}

void CTaskParachuteObject::MakeVisible(CObject& rObject, bool bVisible)
{
	if(taskVerifyf(!rObject.IsNetworkClone(), "The object is a clone."))
	{
		rObject.SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, bVisible);

#if !__NO_OUTPUT
		if(!bVisible)
		{
			if(rObject.GetIsVisible())
			{
				NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : Parachute was set to not be visible, but is still visible.", fwTimer::GetFrameCount(), &rObject, BANK_ONLY(true ? rObject.GetDebugName() : ) "NO OBJECT");)

				for(int i = 0; i < NUM_VISIBLE_MODULES; ++i)
				{
					eIsVisibleModule nModule = (eIsVisibleModule)i;
					if(rObject.GetIsVisibleForModule(nModule))
					{
						NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : Module %d is still visible.", fwTimer::GetFrameCount(), &rObject, BANK_ONLY(true ? rObject.GetDebugName() : ) "NO OBJECT", i);)
					}
				}
			}
		}
#endif
	}
}

#if !__FINAL
const char* CTaskParachuteObject::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Stream",
		"State_WaitForDeploy",
		"State_Deploy",
		"State_Idle",
		"State_Crumple",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

CTask::FSM_Return CTaskParachuteObject::ProcessPreFSM()
{
	//Process the streaming.
	ProcessStreaming();
	
	// Process the clone parachute visibility.
	ProcessVisibility();

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachuteObject::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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
				
		FSM_State(State_WaitForDeploy)
			FSM_OnEnter
				WaitForDeploy_OnEnter();
			FSM_OnUpdate
				return WaitForDeploy_OnUpdate();
				
		FSM_State(State_Deploy)
			FSM_OnEnter
				Deploy_OnEnter();
			FSM_OnUpdate
				return Deploy_OnUpdate();
				
		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();
			
		FSM_State(State_Skydiving)
				FSM_OnEnter
				SkyDiving_OnEnter();
			FSM_OnUpdate
				return SkyDiving_OnUpdate();
				
		FSM_State(State_Crumple)
			FSM_OnEnter
				Crumple_OnEnter();
			FSM_OnUpdate
				return Crumple_OnUpdate();
				
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskParachuteObject::Start_OnEnter()
{
	//Grab the object.
	CObject* pObject = GetObject();
	
	//Force animations to always play for the parachute, even if it is off-screen.
	pObject->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
}

CTask::FSM_Return CTaskParachuteObject::Start_OnUpdate()
{
	//Move to the stream state.
	TaskSetState(State_Stream);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskParachuteObject::Stream_OnUpdate()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Check if the move network has streamed in.
	bool bMoveNetworkStreamedIn = m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskParachuteObject);

	//Check if the clip set has streamed in.
	bool bClipSetStreamedIn = m_ClipSetRequestHelper.IsLoaded();

	//Ensure everything required to start the skydive has streamed in.
	if(bMoveNetworkStreamedIn && bClipSetStreamedIn)
	{
		//Grab the object.
		CObject* pObject = GetObject();
		
		//Create the network player.
		m_MoveNetworkHelper.CreateNetworkPlayer(pObject, CClipNetworkMoveInfo::ms_NetworkTaskParachuteObject);

		//Set the clip set.
		m_MoveNetworkHelper.SetClipSet(m_ClipSetRequestHelper.GetClipSetId());

		//Set the network.
		pObject->GetMoveObject().SetNetwork(m_MoveNetworkHelper, 0.0f);
		
		//Move to the wait for deploy state.
		TaskSetState(State_WaitForDeploy);
	}
	
	return FSM_Continue;
}

void CTaskParachuteObject::WaitForDeploy_OnEnter()
{
	//Note that the parachute can be deployed.
	m_uFlags.SetFlag(PORF_CanDeploy);
}

CTask::FSM_Return CTaskParachuteObject::WaitForDeploy_OnUpdate()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Wait for the deploy flag to be set.
	if(m_uFlags.IsFlagSet(PORF_Deploy) || (UsesStateFromNetwork() && GetStateFromNetwork() == State_Deploy))
	{
		//Move to the deploy state.
		TaskSetState(State_Deploy);
	}
	
	return FSM_Continue;
}

void CTaskParachuteObject::Deploy_OnEnter()
{	
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Note that the parachute can no longer be deployed.
	m_uFlags.ClearFlag(PORF_CanDeploy);
	
	//Note that the parachute should no longer be deployed.
	m_uFlags.ClearFlag(PORF_Deploy);
	
	//Send the request.
	m_MoveNetworkHelper.SendRequest(ms_Deploy);

	//Wait for the state to be entered.
	m_MoveNetworkHelper.WaitForTargetState(ms_Deploy_OnEnter);
}

CTask::FSM_Return CTaskParachuteObject::Deploy_OnUpdate()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Ensure the state has been entered.
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}
		
	//Check if the parachute is out.
	if(!m_uFlags.IsFlagSet(PORF_IsOut))
	{
		//Check if the phase has exceeded the threshold.
		float fPhase = m_MoveNetworkHelper.GetFloat(ms_Phase);
		if(fPhase >= ms_Tunables.m_PhaseDuringDeployToConsiderOut)
		{
			//Set the flag.
			m_uFlags.SetFlag(PORF_IsOut);
		}
	}
	
	//Wait for the animation to end.
	if(m_MoveNetworkHelper.GetBoolean(ms_DeployClipEnded))
	{
		//Move to the idle state.
		TaskSetState(State_Skydiving);
	}
	
	return FSM_Continue;
}

void CTaskParachuteObject::Idle_OnEnter()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Send the request.
	m_MoveNetworkHelper.SendRequest(ms_Idle);

	//Wait for the state to be entered.
	m_MoveNetworkHelper.WaitForTargetState(ms_Idle_OnEnter);
}

CTask::FSM_Return CTaskParachuteObject::Idle_OnUpdate()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Ensure the state has been entered.
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}
	
	//Wait for the crumple.
	if(m_uFlags.IsFlagSet(PORF_Crumple) || (UsesStateFromNetwork() && GetStateFromNetwork() == State_Crumple))
	{
		//Move to the crumple state.
		TaskSetState(State_Crumple);
	}
	
	return FSM_Continue;
}

void CTaskParachuteObject::SkyDiving_OnEnter()
{
	Idle_OnEnter();
}

CTask::FSM_Return CTaskParachuteObject::SkyDiving_OnUpdate()
{
	return Idle_OnUpdate();
}

void CTaskParachuteObject::Crumple_OnEnter()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Clear the crumple flag.
	m_uFlags.ClearFlag(PORF_Crumple);
	
	//Send the request.
	m_MoveNetworkHelper.SendRequest(ms_Crumple);

	//Wait for the state to be entered.
	m_MoveNetworkHelper.WaitForTargetState(ms_Crumple_OnEnter);
}

CTask::FSM_Return CTaskParachuteObject::Crumple_OnUpdate()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : %s : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", GetStaticStateName(GetState()), __FUNCTION__);)

	//Ensure the state has been entered.
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}
	
	//Wait for the animation to end.
	if(m_MoveNetworkHelper.GetBoolean(ms_CrumpleClipEnded))
	{
		//Finish the task.
		TaskSetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskParachuteObject::ProcessVisibility()
{
	//Grab the object.
	CObject* pObject = GetObject();
	if(pObject)
	{
		bool shouldBeVisible = false;
		switch(GetState())
		{
			case State_Start:			
			case State_Stream:			
			case State_WaitForDeploy:	
				shouldBeVisible = false;
				Assert(!m_uFlags.IsFlagSet(PORF_IsOut));
				break;
			case State_Deploy:
				shouldBeVisible = true;
				break;
			case State_Idle:
			case State_Crumple:	
			case State_Finish:
			case State_Skydiving:
				shouldBeVisible = true;
				Assert(m_uFlags.IsFlagSet(PORF_IsOut));
				break;
			default:
				Assertf(false, "%s : missing state?!", __FUNCTION__);
				break;
		}

		if(!pObject->IsNetworkClone())
		{
			NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : Make Visible : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", shouldBeVisible ? "yes" : "no");)

			MakeVisible(*pObject, shouldBeVisible);
		}
		else
		{
			if(pObject->GetNetworkObject())
			{
				CNetObjPhysical* netobjPhysical = SafeCast(CNetObjPhysical, pObject->GetNetworkObject());

				if (gnetVerify(netobjPhysical) && !netobjPhysical->IsLocalFlagSet(CNetObjGame::LOCALFLAG_SHOWINCUTSCENE) && GetState() != State_WaitForDeploy)
				{
					shouldBeVisible = !NetworkInterface::IsInMPCutscene();
				}
			}

			NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Object %p %s : Overriding Local Visiblity : %s", fwTimer::GetFrameCount(), GetObject(), BANK_ONLY(GetObject() ? GetObject()->GetDebugName() : ) "NO OBJECT", shouldBeVisible ? "yes" : "no");)

			// we tell the network we're taking charge of the parachutes visibility from now on.....
			((CNetObjEntity*)pObject->GetNetworkObject())->SetOverridingLocalVisibility(shouldBeVisible, __FUNCTION__, true);			
		}
	}
}

void CTaskParachuteObject::MakeVisible(bool bVisible)
{
	MakeVisible(*GetObject(), bVisible);
}

void CTaskParachuteObject::ProcessStreaming()
{
	//Stream the move network.
	m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskParachuteObject);
	
	bool bAttachedToCar = false;

	//Stream the clip set.
	if (GetObject())
	{
		CObject* pObject = GetObject();

		CPhysical* attachParent = (CPhysical *)pObject->GetAttachParent();

		if (attachParent && attachParent->GetIsTypeVehicle())
		{
			m_ClipSetRequestHelper.Request(CLIP_SET_VEH_SKYDIVE_PARACHUTE_CHUTE);
			bAttachedToCar = true;
		}
	}

	if(!bAttachedToCar)
	{
		m_ClipSetRequestHelper.Request(CLIP_SET_SKYDIVE_PARACHUTE_CHUTE);
	}
}

void CTaskParachuteObject::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskInfo* CTaskParachuteObject::CreateQueriableState() const
{
	return rage_new CClonedParachuteObjectInfo(GetState());
}

void CTaskParachuteObject::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskParachuteObject::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == aiTask::OnUpdate && GetStateFromNetwork() == State_Finish)
	{
		TaskSetState(State_Finish);
		return FSM_Continue;
	}

	// the clone hasn't deployed the parachute yet
	if(iEvent == aiTask::OnUpdate && !m_hasAlreadyDeployed && GetStateFromNetwork() == State_Skydiving)
	{
		m_hasAlreadyDeployed = true;
		TaskSetState(State_Deploy);
		return FSM_Continue;
	}


	return UpdateFSM(iState, iEvent);
}

bool CTaskParachuteObject::OverridesNetworkBlender(CPed*)
{
	// we do not want to position sync until the ped is attached to the parachute after deploying has completed.
	return (GetObject() && GetObject()->GetChildAttachment() == NULL);
}

bool CTaskParachuteObject::OverridesNetworkAttachment(CPed* )
{
	// The TaskParachute running on the ped handles attachment and detachment at all times...
	return true;
}

bool CTaskParachuteObject::IsInScope(const CPed* )
{
	return true;
}

// Use state from network if the parachute object is attached to a vehicle
bool CTaskParachuteObject::UsesStateFromNetwork()
{
	if (GetObject())
	{
		CObject* pObject = GetObject();

		CPhysical* attachParent = (CPhysical *)pObject->GetAttachParent();

		if (attachParent && attachParent->GetIsTypeVehicle())
		{
			return true;
		}
	}
	
	return false;
}

CTaskFSMClone* CClonedParachuteObjectInfo::CreateCloneFSMTask(void)
{
	return rage_new CTaskParachuteObject();
}
