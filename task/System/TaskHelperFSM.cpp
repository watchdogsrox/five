#include "Task/System/TaskHelperFSM.h"

// Rage Includes
#include "ai/task/task.h"
#include "grcore/debugdraw.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

// Disabled, was unused.
//	FW_INSTANTIATE_BASECLASS_POOL(CTaskHelperFSM, 19, atHashString("CTaskHelperFSM",0xaac0aa6e), SIZE_LARGEST_TASK_HELPER_CLASS);

#if __BANK
s32 CTaskHelperFunctions::ms_iIndentSize = 10;

////////////////////////////////////////////////////////////////////////////////

void CTaskHelperFunctions::AddDebugStateNameWithIndent(Color32 color, s32 iIndentLevel, const char* szHelperName, const char* szHelperState)
{
	atString strIndent("");

	for (s32 i=0; i<iIndentLevel; i++)
	{
		for (s32 j=0; j<ms_iIndentSize; j++)
		{
			strIndent += " ";
		}
	}

	grcDebugDraw::AddDebugOutput(color, "%s%s : %s", strIndent.c_str(), szHelperName, szHelperState);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHelperFunctions::AddDebugTextWithIndent(Color32 color, s32 iIndentLevel, const char* szDebugText)
{
	atString strIndent("");

	for (s32 i=0; i<iIndentLevel; i++)
	{
		for (s32 j=0; j<ms_iIndentSize; j++)
		{
			strIndent += " ";
		}
	}

	grcDebugDraw::AddDebugOutput(color, "%s%s", strIndent.c_str(), szDebugText);
}
#endif 

////////////////////////////////////////////////////////////////////////////////

#if __BANK
bool CTaskHelperFSM::ms_bRenderStateDebug = true;
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::CTaskHelperFSM()
: m_State(0)
, m_Flags(aiTaskHelperFlags::CurrentStateNotYetEntered, 0)
#if __ASSERT
, m_CanChangeState(true)
#endif // __ASSERT
{
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::~CTaskHelperFSM()
{
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CTaskHelperFSM::Update()
{
	if (ProcessPreFSM() == FSM_Quit)
	{
		return FSM_Quit;
	}

	bool bContinueFsmUpdate			= true;
	bool bQuit						= false;
	s32 iLoopCount = 0;

	// Call the main update function
	while(bContinueFsmUpdate)
	{
		// By default, don't repeat the FSM update, this will be set below if the state changes
		bContinueFsmUpdate = false;

		// Keep track of any state changes
		s32 iOldState = GetState();

		// If the state hasn't yet started, enter the state then continue as normal
		if(m_Flags.IsFlagSet(aiTaskHelperFlags::CurrentStateNotYetEntered))
		{
			if(UpdateFSM(GetState(), OnEnter) == FSM_Quit)
			{
				// Exit the state
				UpdateFSM(GetState(), OnExit);
				bQuit  = true;
			}
			else
			{
				// Update the new state, continue updating
				bContinueFsmUpdate = true;
			}

			m_Flags.ClearFlag(aiTaskHelperFlags::CurrentStateNotYetEntered);
		}
		else
		{
			Assertf(!m_Flags.IsFlagSet(aiTaskHelperFlags::RestartCurrentState), "If encountered, please assign a bug to Dustin Russell.  Thanks!");
			m_Flags.ClearFlag(aiTaskHelperFlags::RestartCurrentState);

			// Validate state changes while performing the OnUpdate of the current state
			ASSERT_ONLY(m_CanChangeState = true);
			FSM_Return updateReturn = UpdateFSM(GetState(), OnUpdate);
			ASSERT_ONLY(m_CanChangeState = false);

			// Quit immediately if returned from the update
			switch(updateReturn)
			{
			case FSM_Continue:
				if(iOldState != GetState() || m_Flags.IsFlagSet(aiTaskHelperFlags::RestartCurrentState))
				{
					// Exit the old state
					UpdateFSM(iOldState, OnExit);

					if(UpdateFSM(GetState(), OnEnter) == FSM_Quit)
					{
						// Exit the state
						UpdateFSM(GetState(), OnExit);
						bQuit  = true;
					}
					// Update the new state, continue updating 
					bContinueFsmUpdate = true;

					m_Flags.ClearFlag(aiTaskHelperFlags::CurrentStateNotYetEntered);
				}
				break;

			case FSM_Quit:
				// Exit the old state
				ASSERT_ONLY(m_CanChangeState = true);
				UpdateFSM(GetState(), OnExit);
				bQuit = true;
				break;
			}

			// Clear any used request to restart the current state
			m_Flags.ClearFlag(aiTaskHelperFlags::RestartCurrentState);
		}

		const s32 MAX_STATE_LOOPS = 30;
		++iLoopCount;
		aiAssertf(iLoopCount < MAX_STATE_LOOPS, "Possible state machine infinite loop detected in task helper!");
		if(iLoopCount >= MAX_STATE_LOOPS)
		{
			break;
		}
	}

	// If set to quit, quit immediately
	if(bQuit)
	{
		CleanUp();
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskHelperFSM::SetState(const s32 iState)
{
	taskAssertf(m_CanChangeState, "Unable to change state, current state: %d", m_State);

	if(iState != m_State)
	{
		m_Flags.SetFlag(aiTaskHelperFlags::CurrentStateNotYetEntered);
	}

	m_State = iState;
}
