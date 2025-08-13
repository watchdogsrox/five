#ifndef INC_TASK_HELPER_FSM_H
#define INC_TASK_HELPER_FSM_H

// Rage headers
#include "system/bit.h"
#include "vector/color32.h"

// Framework headers
#include "fwutil/flags.h"
#include "fwtl/pool.h"

// Defines
#define SIZE_LARGEST_TASK_HELPER_CLASS	(192)	// sizeof(CPathNodeRouteSearchHelper)

////////////////////////////////////////////////////////////////////////////////

#if __BANK
struct CTaskHelperFunctions
{
	static s32 ms_iIndentSize;
	static void AddDebugStateNameWithIndent(Color32 color, s32 iIndentLevel, const char* szHelperName, const char* szHelperState);
	static void AddDebugTextWithIndent(Color32 color, s32 iIndentLevel, const char* szDebugText);
};
#endif

////////////////////////////////////////////////////////////////////////////////

// PURPOSE: Wrapper class for aiTaskHelper flag settings
class aiTaskHelperFlags
{
public:

	// Flags that can be queried to check the state of the task
	enum InformationFlags
	{
		// Has the task helper ever ran?
		HasBegun							= BIT0,

		// Has the task helper terminated?
		HasFinished							= BIT1,

		// Has helper termination been requested?
		TerminationRequested				= BIT2,

		// Has the current state not been started yet?
		CurrentStateNotYetEntered			= BIT3, 
	};

	// Flags that can be set to alter the behaviour of the task helper
	enum AdjustableFlags
	{
		// Restart the current state
		RestartCurrentState					= BIT0,
	};

	aiTaskHelperFlags();
	aiTaskHelperFlags(const s16 iInformationFlags, const s16 iAdjustableFlags);

	bool IsFlagSet(InformationFlags flag) const;
	bool IsFlagSet(AdjustableFlags flag) const;

	void SetFlag(InformationFlags flag);
	void SetFlag(AdjustableFlags flag);

	void ClearFlag(InformationFlags flag);
	void ClearFlag(AdjustableFlags flag);

private:

	fwFlags16 m_iInformationFlags;
	fwFlags16 m_iAdjustableFlags;
};

////////////////////////////////////////////////////////////////////////////////

inline aiTaskHelperFlags::aiTaskHelperFlags()
{
}

////////////////////////////////////////////////////////////////////////////////

inline aiTaskHelperFlags::aiTaskHelperFlags(const s16 iInformationFlags, const s16 iAdjustableFlags)
: m_iInformationFlags(iInformationFlags)
, m_iAdjustableFlags(iAdjustableFlags)
{
}

////////////////////////////////////////////////////////////////////////////////

inline bool aiTaskHelperFlags::IsFlagSet(InformationFlags flag) const
{
	return m_iInformationFlags.IsFlagSet((s16)flag);
}

////////////////////////////////////////////////////////////////////////////////

inline bool aiTaskHelperFlags::IsFlagSet(AdjustableFlags flag) const 
{
	return m_iAdjustableFlags.IsFlagSet((s16)flag);
}

////////////////////////////////////////////////////////////////////////////////

inline void aiTaskHelperFlags::SetFlag(InformationFlags flag)
{
	m_iInformationFlags.SetFlag((s16)flag);
}

////////////////////////////////////////////////////////////////////////////////

inline void aiTaskHelperFlags::SetFlag(AdjustableFlags flag)
{
	m_iAdjustableFlags.SetFlag((s16)flag);
}

////////////////////////////////////////////////////////////////////////////////

inline void aiTaskHelperFlags::ClearFlag(InformationFlags flag)
{
	m_iInformationFlags.ClearFlag((s16)flag);
}

////////////////////////////////////////////////////////////////////////////////

inline void aiTaskHelperFlags::ClearFlag(AdjustableFlags flag)
{
	m_iAdjustableFlags.ClearFlag((s16)flag);
}

////////////////////////////////////////////////////////////////////////////////

class CTaskHelperFSM
{
public:
	// Disabled, was unused:
	//	FW_REGISTER_CLASS_POOL(CTaskHelperFSM);

	CTaskHelperFSM();
	virtual ~CTaskHelperFSM();

	// RETURNS: The current FSM state
	s32 GetState() const;

	// PURPOSE: Sets the FSM state
	void SetState(const s32 iState);

	// PURPOSE: State return values, continue or quit
	enum FSM_Return
	{
		FSM_Continue = 0,
		FSM_Quit,
	};

	// PURPOSE: Task helper update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: Base update function (acts as a task manager and calls UpdateFSM, should NOT be overriden)
	//			It is the user's responsibility to call this update function and check if the task helper should quit
	FSM_Return Update();

#if !__FINAL
	// PURPOSE: Display debug information specific to this task helper
	virtual const char* GetName() const { return "TASK_HELPER_NONE"; }
	virtual const char* GetStateName() const { return "State_Invalid"; }
	virtual void		Debug(s32 UNUSED_PARAM(iIndentLevel)) const { }
#if __BANK
	static bool			ms_bRenderStateDebug;
#endif // __BANK
#endif // !__FINAL

protected:

	// PURPOSE: Basic FSM operation events
	enum FSM_Event
	{
		OnEnter = 0,
		OnUpdate,
		OnExit,
	};

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent) = 0;

	// PURPOSE: Generic cleanup function, called when the task helper quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Set an adjustable flag
	void SetFlag(const aiTaskHelperFlags::AdjustableFlags flag);

	// PURPOSE: Clear an adjustable flag
	void ClearFlag(const aiTaskHelperFlags::AdjustableFlags flag);

private:
	// Unintentionally private and unimplemented, to show that there is no
	// need for a pool since we don't allocate these objects.
	void* operator new(size_t RAGE_NEW_EXTRA_ARGS_UNUSED);
	void* operator new(size_t, s32);

	// PURPOSE: Current state
	s32 m_State;

	// PURPOSE: Flags
	aiTaskHelperFlags	m_Flags;

#if __ASSERT
	// Alert when calling SetState in invalid conditions
	bool m_CanChangeState;
#endif // __ASSERT
};

////////////////////////////////////////////////////////////////////////////////

inline void CTaskHelperFSM::CleanUp()
{
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CTaskHelperFSM::GetState() const
{
	return m_State;
}

////////////////////////////////////////////////////////////////////////////////

inline void CTaskHelperFSM::SetFlag(aiTaskHelperFlags::AdjustableFlags flag)
{
	m_Flags.SetFlag(flag);
}

////////////////////////////////////////////////////////////////////////////////

inline void CTaskHelperFSM::ClearFlag(aiTaskHelperFlags::AdjustableFlags flag)
{
	m_Flags.ClearFlag(flag);
}

////////////////////////////////////////////////////////////////////////////////

inline CTaskHelperFSM::FSM_Return CTaskHelperFSM::ProcessPreFSM()
{
	return FSM_Continue;
}

#endif // INC_TASK_HELPER_FSM_H
