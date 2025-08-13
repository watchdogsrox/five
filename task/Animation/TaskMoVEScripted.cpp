#include "Task/Animation/TaskMoVEScripted.h"

// Rage headers
#include "crclip/clip.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animhelpers.h"
#include "fwanimation/animmanager.h"
#include "fwscene/stores/networkdefstore.h"

// Game headers
#include "animation/AnimManager.h"
#include "animation/MovePed.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/System/Task.h"

AI_OPTIMISATIONS()

const fwMvBooleanId CTaskMoVEScripted::ms_ExitId("Exit",0x728E4B44);
const fwMvBooleanId CTaskMoVEScripted::ms_IntroFinishedId("IntroFinished",0xDB2EF7F1);
const fwMvClipId	CTaskMoVEScripted::ms_IntroOffsetClipId("IntroOffsetClip",0x330DC93);
const fwMvFloatId	CTaskMoVEScripted::ms_IntroOffsetPhaseId("IntroOffsetPhase",0xE07CC427);
const float CTaskMoVEScripted::DEFAULT_FLOAT_SIGNAL_LERP_RATE = 0.5f;
const float CTaskMoVEScripted::FLOAT_SIGNAL_LERP_EPSILON = 0.01f;  //stops lerping when current value reaches this difference from target

//#include "system/findsize.h"
//FindSize(CClonedTaskMoVEScriptedInfo); //144

CTaskMoVEScripted::CTaskMoVEScripted(const fwMvNetworkDefId &networkDefId, u32 iFlags, float fBlendInDuration, const Vector3 &vInitialPosition, const Quaternion &initialOrientation, ScriptInitialParameters* pInitParams)
: m_networkDefId(networkDefId)
, m_bStateRequested(false) 
, m_iFlags(iFlags)
, m_vInitialPosition(vInitialPosition)
, m_InitialOrientation(initialOrientation)
, m_fBlendInDuration(fBlendInDuration)
, m_CloneSyncDictHash(0)
, m_pInitialParameters(NULL)
, m_ExpectedCloneNextStatePartialHash(0)
, m_initClipSet0(CLIP_SET_ID_INVALID)
, m_initVarClipSet0(CLIP_SET_VAR_ID_INVALID)
{
	sprintf(m_szState, "Intro");
	sprintf(m_szPreviousState, "Intro");
	sprintf(m_szStateRequested, "Intro");
	m_StatePartialHash = atPartialStringHash(m_szStateRequested);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_SCRIPTED);

#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED
	SetInitParams(pInitParams);
}

CTaskMoVEScripted::~CTaskMoVEScripted()
{
	if(m_bCloneDictRefAdded)
	{
		strLocalIndex clipDictIndex = fwAnimManager::FindSlotFromHashKey(m_CloneSyncDictHash);
		if(clipDictIndex.Get() >-1)
		{
			fwAnimManager::RemoveRefWithoutDelete(clipDictIndex);
		}	
	}

	if (m_pInitialParameters)
	{
		delete m_pInitialParameters;
		m_pInitialParameters = NULL;
	}

#if FPS_MODE_SUPPORTED
	for (int helper = 0; helper < 2; ++helper)
	{
		if (m_apFirstPersonIkHelper[helper])
		{
			delete m_apFirstPersonIkHelper[helper];
			m_apFirstPersonIkHelper[helper] = NULL;
		}
	}
#endif // FPS_MODE_SUPPORTED
}

void CTaskMoVEScripted::RequestStateTransition( const char* szState )
{
	m_bStateRequested = true;
	Assert(strlen(szState) < STATE_NAME_LEN - 1);
	sprintf(m_szStateRequested, szState, STATE_NAME_LEN - 1);

	m_StatePartialHash = atPartialStringHash(m_szStateRequested);
	animTaskDebugf(this, " Script requested state transition: %s", szState);

	if(GetPed() && !GetPed()->IsNetworkClone())
	{
		m_ExpectedCloneNextStatePartialHash = 0; //Local peds clear this here pending script setting it after this command if required 
	}
}

void CTaskMoVEScripted::SetExpectedCloneNextStateTransition( const char* szState )
{
	if(taskVerifyf(!GetPed()->IsNetworkClone(),"%s only expected to be called on local peds",GetPed()->GetDebugName()))
	{
		Assert(strlen(szState) < STATE_NAME_LEN - 1);
		char nextStateName[STATE_NAME_LEN];
		sprintf(nextStateName, szState, STATE_NAME_LEN - 1);

		m_ExpectedCloneNextStatePartialHash = atPartialStringHash(nextStateName);
		animTaskDebugf(this, " Script requested clone next state transition: %s", szState);
	}
}

CTask::FSM_Return CTaskMoVEScripted::ProcessPreFSM()
{
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsHigherPriorityClipControllingPed, true);

	if (m_moveNetworkHelper.IsNetworkActive())
	{
		const crClip* pClip = m_moveNetworkHelper.GetClip(ms_IntroOffsetClipId);
		if ((m_iFlags & Flag_SetInitialPosition) && pClip)
		{

			Matrix34 startMatrix(M34_IDENTITY);
			Matrix34 deltaMatrix(M34_IDENTITY);

			startMatrix.d = m_vInitialPosition;
			startMatrix.FromQuaternion(m_InitialOrientation);

			// apply the initial offset from the clip
			fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, startMatrix);

			fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.0, m_moveNetworkHelper.GetFloat(ms_IntroOffsetPhaseId), deltaMatrix);

			startMatrix.DotFromLeft(deltaMatrix);

			GetPed()->SetMatrix(startMatrix);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoVEScripted::ProcessPostFSM()
{
	if(GetEntity() && GetPed()->IsNetworkClone())
	{
		UpdateCloneFloatSignals();
	}

	return FSM_Continue;
}



CTask::FSM_Return CTaskMoVEScripted::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				Start_OnUpdate();

		FSM_State(State_Transitioning)
			FSM_OnEnter
				Transitioning_OnEnter();
			FSM_OnUpdate
				Transitioning_OnUpdate();
			FSM_OnExit
				Transitioning_OnExit();

		FSM_State(State_InState)
			FSM_OnEnter
				InState_OnEnter();
			FSM_OnUpdate
				InState_OnUpdate();	
			
		FSM_State(State_Exit)
			FSM_OnEnter
			if (m_moveNetworkHelper.IsNetworkAttached())
			{
				GetPed()->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, 0.0f);
			}
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if FPS_MODE_SUPPORTED
void CTaskMoVEScripted::ProcessPreRender2()
{
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		if (GetState() != State_Exit)
		{
			ProcessFirstPersonIk();
		}
	}
}

void CTaskMoVEScripted::ProcessFirstPersonIk()
{
	if (!(m_iFlags & Flag_UseFirstPersonArmIkLeft) && !(m_iFlags & Flag_UseFirstPersonArmIkRight))
	{
		return;
	}

	taskAssert(GetPhysical());
	taskAssert(GetPhysical()->GetIsTypePed());

	CPed* pPed = GetPed();

	if (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		for (int helper = 0; helper < 2; ++helper)
		{
			if (m_apFirstPersonIkHelper[helper])
			{
				m_apFirstPersonIkHelper[helper]->Reset();
			}
		}

		return;
	}

	for (int helper = 0; helper < 2; ++helper)
	{
		u32 controlFlag = (helper == 0) ? Flag_UseFirstPersonArmIkLeft : Flag_UseFirstPersonArmIkRight;

		if (m_iFlags & controlFlag)
		{
			if (m_apFirstPersonIkHelper[helper] == NULL)
			{
				m_apFirstPersonIkHelper[helper] = rage_new CFirstPersonIkHelper();
				taskAssert(m_apFirstPersonIkHelper[helper]);

				m_apFirstPersonIkHelper[helper]->SetArm((helper == 0) ? CFirstPersonIkHelper::FP_ArmLeft : CFirstPersonIkHelper::FP_ArmRight);
			}

			if (m_apFirstPersonIkHelper[helper])
			{
				m_apFirstPersonIkHelper[helper]->Process(pPed);
			}
		}
	}
}
#endif // FPS_MODE_SUPPORTED

void CTaskMoVEScripted::Start_OnEnter()
{
	CPed* pPed = GetPed();

	if (g_NetworkDefStore.FindSlotFromHashKey(m_networkDefId)==-1)
	{
		taskAssertf(0, "Move network '%s'(%u) does not exist in the network def store!", m_networkDefId.TryGetCStr()? m_networkDefId.TryGetCStr() : "UNKNOWN NETWORK", m_networkDefId.GetHash());
	}
	else if (!m_moveNetworkHelper.IsNetworkDefStreamedIn(m_networkDefId))
	{
		taskAssertf(0, "Move network '%s'(%u) is not streamed in!", m_networkDefId.TryGetCStr()? m_networkDefId.TryGetCStr() : "UNKNOWN NETWORK", m_networkDefId.GetHash());
	}
	else
	{
		animTaskDebugf(this, " Starting move network");

		m_moveNetworkHelper.CreateNetworkPlayer(pPed, m_networkDefId);
		
		//apply all the clip sets requested from script to the network player
		for (int i=0; i< m_clipSetsRequested.GetCount(); i++)
		{
			CScriptedClipSetRequest& req = m_clipSetsRequested[i];
			m_moveNetworkHelper.SetClipSet(req.setId, req.varId);

#if !__NO_OUTPUT
			const fwMoveNetworkPlayer* pNetworkPlayer = m_moveNetworkHelper.GetNetworkPlayer();
			mvMotionWeb* pNetwork = pNetworkPlayer ? pNetworkPlayer->GetNetwork() : nullptr;
#if CR_DEV
			const mvNetworkDef* pDefinition = pNetwork ? pNetwork->GetDefinition() : nullptr;
			const char* szName = pDefinition ? pDefinition->GetName() : "(null)";
#else
			const char* szName = "(null)";
#endif
#endif // !__NO_OUTPUT

			animTaskDebugf(this, "m_moveNetworkHelper.SetClipSet network:%p %s req.setId:%u %s req.varId:%u %s",
				pNetwork, szName,
				req.setId.GetHash(), req.setId.GetCStr(),
				req.varId.GetHash(), req.varId.GetCStr());
		}

		m_clipSetsRequested.clear();

		// Apply initial parameters (if set)
		if (m_pInitialParameters)
		{
			if (m_pInitialParameters->m_clipSet0 != CLIP_SET_ID_INVALID)
			{
				m_initClipSet0 = m_pInitialParameters->m_clipSet0;
				m_initVarClipSet0 = m_pInitialParameters->m_varClipSet0;					
				SetClipSet(m_pInitialParameters->m_varClipSet0, m_pInitialParameters->m_clipSet0);
			}

			if (m_pInitialParameters->m_clipSet1 != CLIP_SET_ID_INVALID)
			{
				SetClipSet(m_pInitialParameters->m_varClipSet1, m_pInitialParameters->m_clipSet1);
			}

			if (m_pInitialParameters->m_floatParamId0 != FLOAT_ID_INVALID)
			{
				SetSignalFloat(m_pInitialParameters->m_floatParamId0.GetHash(), m_pInitialParameters->m_floatParamValue0);

				if (m_pInitialParameters->m_floatParamLerpValue0 != -1.0f)
				{
					SetSignalFloatLerpRate(m_pInitialParameters->m_floatParamId0.GetHash(), m_pInitialParameters->m_floatParamLerpValue0);
				}
			}

			if (m_pInitialParameters->m_floatParamId1 != FLOAT_ID_INVALID)
			{
				SetSignalFloat(m_pInitialParameters->m_floatParamId1.GetHash(), m_pInitialParameters->m_floatParamValue1);

				if (m_pInitialParameters->m_floatParamLerpValue1 != -1.0f)
				{
					SetSignalFloatLerpRate(m_pInitialParameters->m_floatParamId1.GetHash(), m_pInitialParameters->m_floatParamLerpValue1);
				}
			}

			if (m_pInitialParameters->m_boolParamId0 != BOOLEAN_ID_INVALID)
			{
				SetSignalBool(m_pInitialParameters->m_boolParamId0.GetHash(), m_pInitialParameters->m_boolParamValue0);
			}

			if (m_pInitialParameters->m_boolParamId1 != BOOLEAN_ID_INVALID)
			{
				SetSignalBool(m_pInitialParameters->m_boolParamId1.GetHash(), m_pInitialParameters->m_boolParamValue1);
			}

			// Done with the initial parameters, free them
			delete m_pInitialParameters;
			m_pInitialParameters = NULL;
		}

		if (m_iFlags&Flag_Secondary)
		{
			pPed->GetMovePed().SetSecondaryTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), m_fBlendInDuration);
		}
		else
		{
		pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), m_fBlendInDuration);
		}
	}
}

CTask::FSM_Return CTaskMoVEScripted::Start_OnUpdate()
{

	if (taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "CTaskMoveScripted: Network '%s'(%u) hasn't been started!", m_networkDefId.TryGetCStr()? m_networkDefId.TryGetCStr() : "UNKNOWN NETWORK", m_networkDefId.GetHash() ) )
	{
		if ( m_moveNetworkHelper.GetBoolean(ms_ExitId) )
		{
			animTaskDebugf(this, " Ending task - exit event hit");
			SetState(State_Exit);
			return FSM_Continue;
		}
		else if ( m_moveNetworkHelper.GetBoolean(ms_IntroFinishedId) )
		{
			animTaskDebugf(this, "Moving to state - Intro event hit");
			SetState(State_InState);
			return FSM_Continue;
		}
		else if (GetTimeInState() > 30.0f)
		{
			taskAssertf(0, "CTaskMoVEScripted:: IntroFinished event must be generated once the intro state has completed!");
			SetState(State_Exit);
			return FSM_Continue;
		}
	}
	else
	{
		animTaskDebugf(this, " Ending task - network not active");
		return FSM_Quit;
	}

#if FPS_MODE_SUPPORTED
	if (GetState() != State_Exit)
	{
		if ((m_iFlags & Flag_UseFirstPersonArmIkLeft) || (m_iFlags & Flag_UseFirstPersonArmIkRight))
		{
			if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	if (m_iFlags&Flag_UseKinematicPhysics)
	{
		GetPed()->SetShouldUseKinematicPhysics(TRUE);
	}
	else if (!(m_iFlags&Flag_Secondary))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
	}

	return FSM_Continue;
}

void CTaskMoVEScripted::Transitioning_OnEnter()
{
	taskAssert(m_bStateRequested);
	char RequestName[STATE_NAME_LEN+8];
	sprintf(RequestName, "%sRequest", m_szStateRequested);
	
	fwMvRequestId requestID(RequestName);

	if(GetPed()->IsNetworkClone())
	{
		Assert(m_StatePartialHash);
		requestID.SetHash(atStringHash("Request", m_StatePartialHash));
	}

	m_moveNetworkHelper.SendRequest( requestID);
}

CTask::FSM_Return CTaskMoVEScripted::Transitioning_OnUpdate()
{
	if (taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "CTaskMoveScripted: Network hasn't been started!"))
	{
		if ( m_moveNetworkHelper.GetBoolean(ms_ExitId) )
		{
			animTaskDebugf(this, " Ending task - exit event hit");
			SetState(State_Exit);
			return FSM_Continue;
		}
		else if ( m_moveNetworkHelper.GetBoolean( fwMvBooleanId(atFinalizeHash(m_StatePartialHash)) ) )
		{
			strcpy(m_szPreviousState, m_szState);
			strcpy(m_szState, m_szStateRequested);
			animTaskDebugf(this, " Moving to InState - State enter event recieved");
			SetState(State_InState);
			return FSM_Continue;
		}
		else if (GetTimeInState() > 30.0f)
		{
#if __ASSERT
			if( GetPed() && GetPed()->IsNetworkClone() )
			{
				taskAssertf(0, "%s CTaskMoVEScripted:: Clone Transition request to state [%u] %s, failed (in network %s)! Check there is a transition to this state, and that it has an Enter event named the same as the state. Current state %s.", GetPed()->GetDebugName(), m_StatePartialHash, fwMvBooleanId(atFinalizeHash(m_StatePartialHash)).GetCStr(), m_networkDefId.GetCStr(), m_szState);
			}
			else
			{
				taskAssertf(0, "%s CTaskMoVEScripted:: Transition request between states %s, %s failed (in network %s)!  Check there is a transition between these states, and that the destination state (%s) has an Enter event named the same as the state.", GetPed()->GetDebugName(), m_szState, m_szStateRequested, m_networkDefId.GetCStr(), m_szStateRequested);
			}
#endif
			animTaskDebugf(this, " Ending task - transition timed out");
			SetState(State_Exit);
			return FSM_Continue;
		}
	}

#if FPS_MODE_SUPPORTED
	if (GetState() != State_Exit)
	{
		if ((m_iFlags & Flag_UseFirstPersonArmIkLeft) || (m_iFlags & Flag_UseFirstPersonArmIkRight))
		{
			if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	if (m_iFlags&Flag_UseKinematicPhysics)
	{
		GetPed()->SetShouldUseKinematicPhysics(TRUE);
	}
	else if (!(m_iFlags&Flag_Secondary))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
	}

	return FSM_Continue;
}

void CTaskMoVEScripted::Transitioning_OnExit()
{
	m_bStateRequested = false;
}

void CTaskMoVEScripted::InState_OnEnter()
{

}

CTask::FSM_Return CTaskMoVEScripted::InState_OnUpdate()
{
	if(GetPed()->IsNetworkClone() && m_ExpectedCloneNextStatePartialHash && m_StatePartialHash != m_ExpectedCloneNextStatePartialHash)
	{
		if ( m_moveNetworkHelper.GetBoolean( fwMvBooleanId(atFinalizeHash(m_ExpectedCloneNextStatePartialHash)) ) )
		{
			m_StatePartialHash = m_ExpectedCloneNextStatePartialHash;  //Will remain in State on update 
		}
	}

	if ( m_moveNetworkHelper.GetBoolean(ms_ExitId) )
	{
		animTaskDebugf(this, " Ending task - exit event recieved");
		SetState(State_Exit);
		return FSM_Continue;
	}
	if( m_bStateRequested )
	{
		if(GetPed()->IsNetworkClone() && m_moveNetworkHelper.GetBoolean(ms_IntroFinishedId) )
		{
			//If clone has set the state request in same frame as creating task then
			//wait a frame for move network intro finish before setting next state
			return FSM_Continue;			
		}

		animTaskDebugf(this, " Moving to transitioning state for new state request");
		SetState(State_Transitioning);
		return FSM_Continue;
	}

#if FPS_MODE_SUPPORTED
	if (GetState() != State_Exit)
	{
		if ((m_iFlags & Flag_UseFirstPersonArmIkLeft) || (m_iFlags & Flag_UseFirstPersonArmIkRight))
		{
			if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	if (m_iFlags&Flag_UseKinematicPhysics)
	{
		GetPed()->SetShouldUseKinematicPhysics(TRUE);
	}
	else if (!(m_iFlags&Flag_Secondary))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
	}

	return FSM_Continue;
}

void CTaskMoVEScripted::SetInitParams(ScriptInitialParameters* pInitParams)
{
	if (m_pInitialParameters)
	{
		delete m_pInitialParameters;
		m_pInitialParameters = NULL;
	}

	if (pInitParams)
	{
		m_pInitialParameters = rage_new InitialParameters(pInitParams);

#if !__NO_OUTPUT
		const fwMoveNetworkPlayer* pNetworkPlayer = m_moveNetworkHelper.GetNetworkPlayer();
		mvMotionWeb* pNetwork = pNetworkPlayer ? pNetworkPlayer->GetNetwork() : nullptr; 
#if CR_DEV
		const mvNetworkDef* pDefinition = pNetwork ? pNetwork->GetDefinition() : nullptr;
		const char* szName = pDefinition ? pDefinition->GetName() : "(null)";
#else
		const char* szName = "(null)";
#endif
#endif // !__NO_OUTPUT

		animTaskDebugf(this, "SetInitParams network:%p %s varClipSet0:%u %s clipSet0:%u %s varClipSet1:%u %s clipSet1:%u %s",
			pNetwork, szName,
			m_pInitialParameters->m_varClipSet0.GetHash(), m_pInitialParameters->m_varClipSet0.GetCStr(),
			m_pInitialParameters->m_clipSet0.GetHash(), m_pInitialParameters->m_clipSet0.GetCStr(),
			m_pInitialParameters->m_varClipSet1.GetHash(), m_pInitialParameters->m_varClipSet1.GetCStr(),
			m_pInitialParameters->m_clipSet1.GetHash(), m_pInitialParameters->m_clipSet1.GetCStr());
	}
}

void CTaskMoVEScripted::CopyInitialParameters(InitialParameters* pInitParams)
{
	if (m_pInitialParameters)
	{
		delete m_pInitialParameters;
		m_pInitialParameters = NULL;
	}

	if (pInitParams)
	{
		m_pInitialParameters = rage_new InitialParameters(pInitParams);

#if !__NO_OUTPUT
		const fwMoveNetworkPlayer* pNetworkPlayer = m_moveNetworkHelper.GetNetworkPlayer();
		mvMotionWeb* pNetwork = pNetworkPlayer ? pNetworkPlayer->GetNetwork() : nullptr;
#if CR_DEV
		const mvNetworkDef* pDefinition = pNetwork ? pNetwork->GetDefinition() : nullptr;
		const char* szName = pDefinition ? pDefinition->GetName() : "(null)";
#else
		const char* szName = "(null)";
#endif
#endif // !__NO_OUTPUT

		animTaskDebugf(this, "CopyInitialParameters network:%p %s varClipSet0:%u %s clipSet0:%u %s varClipSet1:%u %s clipSet1:%u %s",
			pNetwork, szName,
			m_pInitialParameters->m_varClipSet0.GetHash(), m_pInitialParameters->m_varClipSet0.GetCStr(),
			m_pInitialParameters->m_clipSet0.GetHash(), m_pInitialParameters->m_clipSet0.GetCStr(),
			m_pInitialParameters->m_varClipSet1.GetHash(), m_pInitialParameters->m_varClipSet1.GetCStr(),
			m_pInitialParameters->m_clipSet1.GetHash(), m_pInitialParameters->m_clipSet1.GetCStr());
	}
}

void CTaskMoVEScripted::ResetStateTransitionRequest()
{
	m_StatePartialHash = 0;
	m_ExpectedCloneNextStatePartialHash = 0; 
	m_bStateRequested = false;
}

void CTaskMoVEScripted::CleanUp()
{
	CPed* pPed = GetPed();
	if (m_moveNetworkHelper.IsNetworkAttached())
	{
		if (m_iFlags&Flag_Secondary)
		{
			pPed->GetMovePed().ClearSecondaryTaskNetwork(m_moveNetworkHelper, m_fBlendInDuration);
		}
		else
		{
			pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, 0.0f);
		}		
	}
}

void CTaskMoVEScripted::SetSignalFloat( const char* szSignal, float fSignal, bool bSend )
{
	fwMvBooleanId floatSignalId(szSignal);
	SetSignalFloat( floatSignalId.GetHash(),  fSignal, bSend);
}

void CTaskMoVEScripted::SetSignalFloat( const u32 signalHash, float fSignal, bool bSend )
{
	if(!bSend && !m_moveNetworkHelper.IsNetworkActive())
	{//wait for valid network if receiving from remote
		return;
	}

	if( GetPed()->IsNetworkClone() )
	{
		if(bSend)
		{
			if(m_iFlags & Flag_AllowOverrideCloneUpdate)
			{
				bSend = false;  //we are overriding a clones network value locally so make sure it doesn't change clone mapped value via SetCloneSignalFloat
			}
#if __ASSERT
			else
			{
				Assertf(0," Don't expect float signals being applied to clone without Flag_AllowOverrideCloneUpdate: signalHash %u = %f",signalHash, fSignal );
			}
#endif
		}
	}

	if(taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "CTaskMoveScripted::SendSignalFloat Network hasn't been started!"))
	{
		if(bSend)
		{
			SetCloneSignalFloat(  signalHash,  fSignal );
		}

		m_moveNetworkHelper.SetFloat( fwMvFloatId(signalHash), fSignal);
	}
}

void CTaskMoVEScripted::SetCloneSignalFloat( const u32 signalHash, float fSignal, float fLerpRate )
{
	if(NetworkInterface::IsGameInProgress())
	{
		if (m_mapSignalFloatValues.Access(signalHash) == NULL)
		{
			LerpFloat newLerpFloat;
			newLerpFloat.fTarget	= fSignal;
			newLerpFloat.fCurrent	= fSignal;
			newLerpFloat.fLerpRate	= fLerpRate;

			m_mapSignalFloatValues.Insert(signalHash, newLerpFloat);
		}
		else
		{
			m_mapSignalFloatValues[signalHash].fTarget = fSignal;
		}
	}
}

void CTaskMoVEScripted::SetSignalFloatLerpRate(const char* szSignal, float fLerpRate )
{
	Assert(szSignal);
	Assertf(fLerpRate >0.0f && fLerpRate <=1.0f, "signal %s lerp rate: %f out of range",szSignal?szSignal:"Null name string", fLerpRate);
	fwMvBooleanId floatSignalId(szSignal);
	const u32 signalHash = floatSignalId.GetHash();

#if __ASSERT
	if(szSignal)
	{
		if (m_mapSignalFloatValues.Access(signalHash) == NULL)
		{
			Assertf(0, "Float signal %s has not been defined!", szSignal);
			return;
		}
	}
#endif

	SetSignalFloatLerpRate( signalHash,  fLerpRate );
}

void CTaskMoVEScripted::SetSignalFloatLerpRate(const u32 signalHash, float fLerpRate )
{
	Assertf(fLerpRate<=1.0f, "Signal hash %u lerp rate: %f exceeds maximum range 1.0f",signalHash, fLerpRate);
	if(NetworkInterface::IsGameInProgress())
	{
		if (m_mapSignalFloatValues.Access(signalHash) == NULL)
		{
			Assertf(0, "Float signal hash %u has not been defined!", signalHash);
			return;
		}
		else
		{
			m_mapSignalFloatValues[signalHash].fLerpRate = fLerpRate;
		}
	}
}


bool CTaskMoVEScripted::GetCloneMappedFloatSignal(u32 &rsignalHash, float &rfValue, u32 findIndex)
{
	Assertf(NetworkInterface::IsGameInProgress(), "Only expected to be used in network games!");

	u32 index =0;
	for ( CTaskMoVEScripted::FloatSignalMap::Iterator iter = m_mapSignalFloatValues.CreateIterator(); !iter.AtEnd(); iter.Next())
	{
		if(findIndex == index)
		{
			Assertf(iter.GetData().fLerpRate >0.0f && iter.GetData().fLerpRate <=1.0f, "Signal %u lerp rate: %f out of range",iter.GetKey(), iter.GetData().fLerpRate);
			rfValue = iter.GetData().fTarget;
			rsignalHash = iter.GetKey();

			return true;
		}

		index++;
	}

	rsignalHash = 0;
	rfValue = 0.0f;

	return false;
}

bool CTaskMoVEScripted::GetCloneMappedBoolSignal(u32 uFindHash, bool &rbValue)
{
	Assertf(NetworkInterface::IsGameInProgress(), "Only expected to be used in network games!");

	for (CTaskMoVEScripted::BoolSignalMap::Iterator iter = m_mapSignalBoolValues.CreateIterator() ; !iter.AtEnd(); iter.Next())
	{
		if(uFindHash == iter.GetKey())
		{
			rbValue = iter.GetData();
			return true;
		}
	}

	rbValue = false;
	return false;
}

void CTaskMoVEScripted::SetSignalBool( const char* szSignal, bool bSignal, bool bSend )
{
	fwMvBooleanId boolSignalId(szSignal);

	Assertf(boolSignalId.GetHash() !=0,"Trying setting %s on Bool named with %s string which gives Hash = 0", bSignal?"TRUE":"FALSE",szSignal?(szSignal[0]==0?"Empty":szSignal):"Null");

	SetSignalBool( boolSignalId.GetHash(), bSignal, bSend);
}

void CTaskMoVEScripted::SetSignalBool( const u32 signalHash, bool bSignal, bool bSend )
{
	if(!bSend && !m_moveNetworkHelper.IsNetworkActive())
	{//wait for valid network if receiving from remote
		return;
	}

	if( GetPed()->IsNetworkClone() )
	{
		if(bSend)
		{
			if(m_iFlags & Flag_AllowOverrideCloneUpdate)
			{
				bSend = false;  //we are overriding a clones network value locally so make sure it doesn't change clone mapped value in m_mapSignalBoolValues
			}
#if __ASSERT
			else
			{
				Assertf(0," Don't expect bool signals being applied to clone without Flag_AllowOverrideCloneUpdate: signalHash %u = %s",signalHash, bSignal?"True":"False" );
			}
#endif
		}
	}

	if(taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "CTaskMoveScripted::SendSignalBool Network hasn't been started!"))
	{
		if(NetworkInterface::IsGameInProgress() && bSend && taskVerifyf(signalHash!=0," Can't network sync bool signal with 0 Hash name"))
		{
			if (m_mapSignalBoolValues.Access(signalHash) == NULL)
			{
				m_mapSignalBoolValues.Insert(signalHash, bSignal);
			}
			else
			{
				m_mapSignalBoolValues[signalHash] = bSignal;
			}
		}

		m_moveNetworkHelper.SetBoolean( fwMvBooleanId(signalHash), bSignal);
	}
}

float CTaskMoVEScripted::GetSignalFloat( const char* szSignal)
{
	if(taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "CTaskMoveScripted::GetSignalFloat Network hasn't been started!"))
	{
		return m_moveNetworkHelper.GetFloat( fwMvFloatId(szSignal));
	}
	return 0.0f;
}

bool CTaskMoVEScripted::GetSignalBool( const char* szSignal)
{
	if(taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "CTaskMoveScripted::GetSignalBool Network hasn't been started!"))
	{
		return m_moveNetworkHelper.GetBoolean( fwMvBooleanId(szSignal));
	}
	return false;
}

bool CTaskMoVEScripted::GetMoveEvent(const char * eventName) 
{ 
	if (!GetIsNetworkActive())
	{
		animTaskDebugf(this, " GetMoveEvent '%s' returned FALSE. Network not active.", eventName);
		return false;
	}
	else
	{
		bool ret = m_moveNetworkHelper.GetBoolean( fwMvBooleanId(eventName));
		animTaskDebugf(this, " GetMoveEvent '%s' returned %s.", eventName, ret ? "TRUE" : "FALSE");
		return ret;
	}
}

#if !__FINAL
const char * CTaskMoVEScripted::GetStateName( s32 iState  ) const
{
	if(GetEntity() && GetPed() && GetPed()->IsNetworkClone())
	{
		return CTask::GetStateName( iState );
	}

	static char szStateName[128];
	if ( iState == State_Transitioning )
	{
		sprintf( szStateName, "%s: %s to %s", CTask::GetStateName(iState), m_szState, m_szStateRequested );
		return szStateName;
	}
	else
	{
		if ( iState == State_InState )
		{
			sprintf( szStateName, "%s: %s", CTask::GetStateName(iState), m_szState );
			return szStateName;
		}
	}
	return CTask::GetStateName( iState );
}

const char * CTaskMoVEScripted::GetStaticStateName( s32 iState  )
{
	static const char *stateNames[] = 
	{
		"State_Start",
		"State_Transitioning",
		"State_InState",
		"State_Exit",
		"Unknown"
	};
	return stateNames[iState];
}

#endif // __FINAL

// Clone task implementation for CTaskMoVEScripted
CTaskInfo* CTaskMoVEScripted::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	pInfo =  rage_new CClonedTaskMoVEScriptedInfo(	GetState(),
													m_StatePartialHash,
													m_ExpectedCloneNextStatePartialHash,
													m_networkDefId.GetHash(),
													static_cast<u8>(m_iFlags),
													m_fBlendInDuration,
													m_CloneSyncDictHash,
													m_mapSignalFloatValues,
													m_mapSignalBoolValues,
													m_vInitialPosition,
													m_InitialOrientation,
													m_initClipSet0.GetHash(),
													m_initVarClipSet0.GetHash());
	
	//Now we have sent all the bools this frame reset the map to empty
	m_mapSignalBoolValues.Reset();

	return pInfo;
}

void CTaskMoVEScripted::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_MOVE_SCRIPTED);
	CClonedTaskMoVEScriptedInfo *moVEScriptedInfo = static_cast<CClonedTaskMoVEScriptedInfo*>(pTaskInfo);

	if(	moVEScriptedInfo->GetStatePartialHash()!= 0 && 
		moVEScriptedInfo->GetStatePartialHash()!= m_StatePartialHash	)
	{
		//new transition state requested 
		m_bStateRequested = true;
		m_StatePartialHash = moVEScriptedInfo->GetStatePartialHash();
		m_ExpectedCloneNextStatePartialHash = 0;
	}

	if(	moVEScriptedInfo->GetExpectedNextStatePartialHash()!= 0 && 
		moVEScriptedInfo->GetExpectedNextStatePartialHash()!= m_StatePartialHash	)
	{
		m_ExpectedCloneNextStatePartialHash = moVEScriptedInfo->GetExpectedNextStatePartialHash();

		Assertf(m_ExpectedCloneNextStatePartialHash!=m_StatePartialHash,"%s dont expect m_StatePartialHash and m_CloneNextStatePartialHash to be the same %u",GetEntity()?GetPed()->GetDebugName():"Null entity",m_StatePartialHash);
	}

	m_CloneSyncDictHash = moVEScriptedInfo->GetNetworkSyncDictHash();
	m_iFlags = moVEScriptedInfo->GetFlags();

	if( !( m_iFlags & Flag_AllowOverrideCloneUpdate) )
	{
		for (int i=0; i < moVEScriptedInfo->GetNumFloatValues(); i++)
		{
            // we need to round the float signal value so network quantisation does not affect
            // the results - some move networks use 0.5f as a neutral point between two anims for
            // them both to be blended out so this needs to be exact
            float fSignalValue = moVEScriptedInfo->GetFloatSignalValue(i);
            fSignalValue = floorf((fSignalValue * 100.0f) + 0.5f) / 100.0f;

			SetCloneSignalFloat(moVEScriptedInfo->GetFloatSignalHash(i), fSignalValue);
			
			if(moVEScriptedInfo->GetFloatHasLerpValue(i))
			{
				SetSignalFloatLerpRate(moVEScriptedInfo->GetFloatSignalHash(i), moVEScriptedInfo->GetFloatSignalLerpValue(i) );
			}
			else
			{
				SetSignalFloatLerpRate(moVEScriptedInfo->GetFloatSignalHash(i), DEFAULT_FLOAT_SIGNAL_LERP_RATE );
			}
		}
		for (int i=0; i < moVEScriptedInfo->GetNumBoolValues(); i++)
		{
			SetSignalBool(moVEScriptedInfo->GetBoolSignalHash(i), moVEScriptedInfo->GetBoolSignalValue(i), false);
		}
	}

#if __ASSERT
	if(m_bStateRequested)
	{
		Assertf( m_StatePartialHash, "Expect a non-zero m_StatePartialHash when m_bStateRequested is true" );
	}
#endif

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskMoVEScripted::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(GetEnableCollisionOnNetworkCloneWhenFixed())
	{
		CPed *pPed = GetPed();
		if(pPed)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_EnableCollisionOnNetworkCloneWhenFixed, true);
		}
	}

	if(GetStateFromNetwork() == State_Exit)
	{
		return FSM_Quit;
	}

	return UpdateFSM(iState, iEvent);
}

void CTaskMoVEScripted::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Exit);
}
bool CTaskMoVEScripted::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	bool inScope = true;

    const unsigned MAX_CLONE_SYNC_DICT_HASHES = 2;
    u32 cloneSyncDictHashes[MAX_CLONE_SYNC_DICT_HASHES];

    unsigned numDictHashes = 0;
    cloneSyncDictHashes[numDictHashes] = m_CloneSyncDictHash;
    numDictHashes++;

    if (m_pInitialParameters)
    {
        if(m_pInitialParameters->m_clipSet0 != CLIP_SET_ID_INVALID)
        {
            fwClipSet *clipset = fwClipSetManager::GetClipSet(m_pInitialParameters->m_clipSet0);

            if(taskVerifyf(clipset, "Failed to find clipset with hash %d!", m_pInitialParameters->m_clipSet0.GetHash()))
            {
                strLocalIndex index = clipset->GetClipDictionaryIndex();
                cloneSyncDictHashes[numDictHashes] = fwAnimManager::FindHashKeyFromSlot(index);
                numDictHashes++;
            }
        }
    }

	inScope &= CheckAndRequestSyncDictionary(cloneSyncDictHashes, numDictHashes);

	return inScope;
}


void CTaskMoVEScripted::UpdateCloneFloatSignals()
{
	if(m_iFlags & Flag_AllowOverrideCloneUpdate)
	{
		return;  //we are setting a clones network values locally so don't apply any float signal lerping
	}

	for ( CTaskMoVEScripted::FloatSignalMap::Iterator iter = m_mapSignalFloatValues.CreateIterator() ; !iter.AtEnd(); iter.Next())
	{
		float fSignal = iter.GetData().fTarget;
		
		Assertf(iter.GetData().fLerpRate >0.0f && iter.GetData().fLerpRate <=1.0f, "Signal %u lerp rate: %f out of range",iter.GetKey(), iter.GetData().fLerpRate);
		
		if( IsClose(iter.GetData().fCurrent, iter.GetData().fTarget, FLOAT_SIGNAL_LERP_EPSILON) )
		{
			iter.GetData().fCurrent = iter.GetData().fTarget;
		}
		else
		if(iter.GetData().fLerpRate >0.0f && iter.GetData().fLerpRate <1.0f)
		{
			iter.GetData().fCurrent = rage::Lerp(iter.GetData().fLerpRate, iter.GetData().fCurrent, iter.GetData().fTarget);

			fSignal = iter.GetData().fCurrent;
		}

		SetSignalFloat(iter.GetKey(), fSignal, false);
	}
}

CClonedTaskMoVEScriptedInfo::CClonedTaskMoVEScriptedInfo()
:
m_StatePartialHash(0),
m_ExpectedNextStatePartialHash(0),
m_NetworkDefHash(0),
m_Flags(0),
m_fBlendInDuration(INSTANT_BLEND_DURATION),
m_vInitialPosition(VEC3_ZERO),
m_InitialOrientation(Quaternion::sm_I),
m_NumFloatValues(0),
m_NumBoolValues(0),
m_SyncDictHash(0),
m_clipSet0Hash(0),
m_varClipSet0Hash(0)
{
	for (int i=0; i < CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE; i++)
	{
		m_FloatSignalHash[i] = 0;
		m_FloatSignalValues[i] = 0.0f;
		m_FloatSignalLerpValues[i] = 0.0f;
	}

	for (int i=0; i < CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE; i++)
	{
		m_BoolSignalHash[i] =0;
		SetBoolSignalValue(i, false);
	}
}

CClonedTaskMoVEScriptedInfo::CClonedTaskMoVEScriptedInfo(s32 state,
														 u32 statePartialHash,
														 u32 nextStatePartialHash,
														 u32 networkDefHash,
														 u8 flags,
														 float fBlendInDuration,
														 u32   cloneSyncDictHash,
														 const CTaskMoVEScripted::FloatSignalMap &mapSignalFloatValues,
														 const CTaskMoVEScripted::BoolSignalMap	&mapSignalBoolValues,
														 const Vector3& vInitialPosition,
														 const Quaternion & initialOrientation,
														 u32		initClipIdHash,
														 u32		initVarClipIdHash)
:
m_StatePartialHash(statePartialHash),
m_ExpectedNextStatePartialHash(nextStatePartialHash),
m_NetworkDefHash(networkDefHash),
m_Flags(flags),
m_vInitialPosition(vInitialPosition),
m_InitialOrientation(initialOrientation),
m_fBlendInDuration(fBlendInDuration),
m_NumFloatValues(0),
m_NumBoolValues(0),
m_SyncDictHash(cloneSyncDictHash),
m_clipSet0Hash(initClipIdHash),
m_varClipSet0Hash(initVarClipIdHash)
{


	Assertf(mapSignalFloatValues.GetNumUsed() <=CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE, "Float Index %d out of range", mapSignalFloatValues.GetNumUsed());
	Assertf(mapSignalBoolValues.GetNumUsed() <=CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE, "Bool Index %d out of range", mapSignalBoolValues.GetNumUsed());
	
	int i=0;
	for ( CTaskMoVEScripted::FloatSignalMap::Iterator iter = const_cast<CTaskMoVEScripted::FloatSignalMap&>(mapSignalFloatValues).CreateIterator() ; !iter.AtEnd(); iter.Next())
	{
		m_FloatSignalHash[i] = iter.GetKey();
		m_FloatSignalValues[i] = iter.GetData().fTarget;
		m_FloatSignalLerpValues[i] = iter.GetData().fLerpRate;
		i++;
	}

	m_NumFloatValues = static_cast<u8>(mapSignalFloatValues.GetNumUsed());

	i=0;
	for (CTaskMoVEScripted::BoolSignalMap::Iterator iter = const_cast<CTaskMoVEScripted::BoolSignalMap&>(mapSignalBoolValues).CreateIterator() ; !iter.AtEnd(); iter.Next())
	{
		m_BoolSignalHash[i] = iter.GetKey();
		SetBoolSignalValue(i, iter.GetData());
		i++;
	}

	m_NumBoolValues = static_cast<u8>(mapSignalBoolValues.GetNumUsed());

	SetStatusFromMainTaskState(state);
}

CTaskFSMClone * CClonedTaskMoVEScriptedInfo::CreateCloneFSMTask()
{
	CTaskMoVEScripted* pTask = NULL;

	m_InitialOrientation.Normalize();

	CTaskMoVEScripted::ScriptInitialParameters params;
	params.clipSetHash0.Int			= m_clipSet0Hash;
	params.variableClipSetHash0.Int = m_varClipSet0Hash;

	pTask=rage_new CTaskMoVEScripted(fwMvNetworkDefId(m_NetworkDefHash), m_Flags, m_fBlendInDuration, m_vInitialPosition, m_InitialOrientation, &params);

	return pTask;
}

void	CClonedTaskMoVEScriptedInfo::SetBoolSignalValue(unsigned index, bool bValue )
{
	switch(index)
	{
	case 0:
		m_BoolSignalValue1 = bValue;
		break;
	case 1:
		m_BoolSignalValue2 = bValue;
		break;
	default:
		Assertf(index<CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE, "Bool index %d out of max range %d",index,CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE);
		break;
	}
}

bool	CClonedTaskMoVEScriptedInfo::GetBoolSignalValue(unsigned index )
{
	switch(index)
	{
	case 0:
		return m_BoolSignalValue1;
		break;
	case 1:
		return m_BoolSignalValue2;
		break;
	default:
		Assertf(index<CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE, "Bool index %d out of max range %d",index,CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE);
		break;
	}

	return false;
}

void	CClonedTaskMoVEScriptedInfo::SetFloatHasLerpValue(unsigned index, bool bValue )
{
	switch(index)
	{
	case 0:
		m_FloatHasLerpValue1 = bValue;
		break;
	case 1:
		m_FloatHasLerpValue2 = bValue;
		break;
	default:
		Assertf(index<CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE, "Float index %d out of max range %d",index,CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE);
		break;
	}
}

bool	CClonedTaskMoVEScriptedInfo::GetFloatHasLerpValue(unsigned index )
{
	switch(index)
	{
	case 0:
		return m_FloatHasLerpValue1;
		break;
	case 1:
		return m_FloatHasLerpValue2;
		break;
	default:
		Assertf(index<CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE, "Float index %d out of max range %d",index,CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE);
		break;
	}

	return false;
}

void CTaskMoVEScripted::SetEnableCollisionOnNetworkCloneWhenFixed(bool bEnableCollisionOnNetworkCloneWhenFixed)
{
	if (bEnableCollisionOnNetworkCloneWhenFixed)
	{
		m_iFlags |= Flag_EnableCollisionOnNetworkCloneWhenFixed;
	}
	else
	{
		m_iFlags &= ~Flag_EnableCollisionOnNetworkCloneWhenFixed;
	}
}

#if __BANK

char	CTaskMoVEScripted::ms_NameOfTransistion[MAX_TRANSISTION_NAME_LEN];
char	CTaskMoVEScripted::ms_NameOfSignal[MAX_SIGNAL_NAME_LEN];
bool	CTaskMoVEScripted::ms_bankOverrideCloneUpdate = false;
bool	CTaskMoVEScripted::ms_bankBoolSignal = false;
float	CTaskMoVEScripted::ms_bankFloatSignal = 0.0f;
float	CTaskMoVEScripted::ms_bankFloatLerpRate = CTaskMoVEScripted::DEFAULT_FLOAT_SIGNAL_LERP_RATE;

void CTaskMoVEScripted::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		pBank->PushGroup("MoVE Scripted", false);
		pBank->AddSeparator();

		pBank->AddButton("Start state transition request", SetStartStateTransitionRequest);
		pBank->AddText("State transition request from current MoVE network", ms_NameOfTransistion, MAX_TRANSISTION_NAME_LEN, false);
		pBank->AddSeparator();

		pBank->AddToggle("Override Clone Update", &ms_bankOverrideCloneUpdate, SetAllowOverrideCloneUpdate);
		pBank->AddSeparator();
		pBank->AddText("Signal name for bool or float signal", ms_NameOfSignal,	MAX_SIGNAL_NAME_LEN, false);
		pBank->AddToggle("Signal bool", &ms_bankBoolSignal);
		pBank->AddSlider("Signal float", &ms_bankFloatSignal,  0.0f, 1.0f, 0.001f, NullCB);
		pBank->AddSlider("Signal float lerp rate", &ms_bankFloatLerpRate,  0.0f, 1.0f, 0.001f, NullCB);
		pBank->AddSeparator();

		pBank->AddButton("Send bool signal", SendBoolSignal);
		pBank->AddButton("Send float signal", SendFloatSignal);
		pBank->AddButton("Set float lerp value", SetFloatLerpRate);
		pBank->AddSeparator();
		
		pBank->PopGroup();
	}
}

CTaskMoVEScripted* CTaskMoVEScripted::GetActiveMoVENetworkTaskOnFocusPed(bool bOnlyGetLocal)
{
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		const CPed* pPed = (CPed*)pFocusEntity1;
		if (pPed && pPed->GetPedIntelligence())
		{
			if(bOnlyGetLocal && pPed->IsNetworkClone())
			{
				return NULL;
			}

			CTaskMoVEScripted* pTaskMoVE = static_cast<CTaskMoVEScripted*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_MOVE_SCRIPTED));
			if( pTaskMoVE && pTaskMoVE->GetIsNetworkActive() )
			{
				return pTaskMoVE;
			}
		}
	}
	return NULL;
}
void CTaskMoVEScripted::SetStartStateTransitionRequest()
{
	CTaskMoVEScripted* pTaskMoVE = CTaskMoVEScripted::GetActiveMoVENetworkTaskOnFocusPed();

	if(pTaskMoVE)
	{
		pTaskMoVE->RequestStateTransition(ms_NameOfTransistion);
	}
}

void CTaskMoVEScripted::SendBoolSignal()
{
	CTaskMoVEScripted* pTaskMoVE = CTaskMoVEScripted::GetActiveMoVENetworkTaskOnFocusPed();

	if(pTaskMoVE)
	{
		pTaskMoVE->SetSignalBool(ms_NameOfSignal, ms_bankBoolSignal);
	}
}

void CTaskMoVEScripted::SendFloatSignal()
{
	CTaskMoVEScripted* pTaskMoVE = CTaskMoVEScripted::GetActiveMoVENetworkTaskOnFocusPed();

	if(pTaskMoVE)
	{
		pTaskMoVE->SetSignalFloat(ms_NameOfSignal, ms_bankFloatSignal);
	}
}

void CTaskMoVEScripted::SetFloatLerpRate()
{
	CTaskMoVEScripted* pTaskMoVE = CTaskMoVEScripted::GetActiveMoVENetworkTaskOnFocusPed();

	if(pTaskMoVE)
	{
		pTaskMoVE->SetSignalFloatLerpRate(ms_NameOfSignal, ms_bankFloatLerpRate);
	}
}

void CTaskMoVEScripted::SetAllowOverrideCloneUpdate()
{
	CTaskMoVEScripted* pTaskMoVE = CTaskMoVEScripted::GetActiveMoVENetworkTaskOnFocusPed(true);

	if(pTaskMoVE)
	{
		if(ms_bankOverrideCloneUpdate)
		{
			pTaskMoVE->SetAllowOverrideCloneUpdateFlag();
		}
		else
		{
			pTaskMoVE->ClearAllowOverrideCloneUpdateFlag();
		}
	}
}
#endif


