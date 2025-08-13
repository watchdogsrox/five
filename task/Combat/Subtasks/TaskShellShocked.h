// FILE :    TaskShellShocked.h
// PURPOSE : Combat subtask in control of being shell-shocked
// CREATED : 3/12/2012

#ifndef TASK_SHELL_SHOCKED_H
#define TASK_SHELL_SHOCKED_H

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"

//=========================================================================
// CTaskShellShocked
//=========================================================================

class CTaskShellShocked : public CTask
{

public:

	enum ConfigFlags
	{
		CF_Interrupt			= BIT0,
		CF_DisableMakeFatigued	= BIT1,
	};
	
	enum Duration
	{
		Duration_Invalid = -1,
		Duration_Short,
		Duration_Long,
		Duration_Max,
	};

	enum ShellShockedState
	{
		State_Start = 0,
		State_Stream,
		State_Stagger,
		State_Finish
	};

	CTaskShellShocked(Vec3V_In vPosition, Duration nDuration, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskShellShocked();

	virtual void CleanUp();
	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const {return CTaskTypes::TASK_SHELL_SHOCKED;}
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }
	
public:

	static bool IsValid(const CPed& rPed, Vec3V_In vPosition);
	
public:

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif

private:

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	FSM_Return	Stream_OnUpdate();
	void		Stagger_OnEnter();
	FSM_Return	Stagger_OnUpdate();
	
private:

	void MakeFatigued();
	void ProcessStreaming();
	bool ShouldInterrupt() const;
	bool ShouldMakeFatigued() const;
	
private:

	static fwMvClipSetId				ChooseClipSet(const CPed& rPed);
	static CDirectionHelper::Direction	ChooseDirection(const CPed& rPed, Vec3V_In vPosition, bool bUpdateTime);
	
private:

	fwClipSetRequestHelper		m_ClipSetRequestHelper;
	CMoveNetworkHelper			m_MoveNetworkHelper;
	Vec3V						m_vPosition;
	Duration					m_nDuration;
	CDirectionHelper::Direction	m_nDirection;
	fwMvClipSetId				m_ClipSetId;
	fwFlags8					m_uConfigFlags;
	
private:

	static fwMvBooleanId ms_StaggerClipEndedId;
	
	static fwMvClipId ms_StaggerClipId;

	static fwMvFlagId ms_ShortId;
	static fwMvFlagId ms_LongId;
	static fwMvFlagId ms_FrontId;
	static fwMvFlagId ms_BackId;
	static fwMvFlagId ms_LeftId;
	static fwMvFlagId ms_RightId;
	
	static fwMvFloatId ms_RateId;
	static fwMvFloatId ms_PhaseId;

	static fwMvRequestId ms_StaggerId;
	
};

#endif //TASK_SHELL_SHOCKED_H
