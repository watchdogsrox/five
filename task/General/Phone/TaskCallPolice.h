// FILE :    TaskCallPolice.h

#ifndef TASK_CALL_POLICE_H
#define TASK_CALL_POLICE_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Game forward declarations
struct CAmbientAudio;

//=========================================================================
// CTaskCallPolice
//=========================================================================

class CTaskCallPolice : public CTask
{

public:

	enum State
	{
		State_Start = 0,
		State_WaitForTalkingToFinish,
		State_MakeCall,
		State_Finish,
	};

private:

	enum RunningFlags
	{
		RF_SaidContextForCall			= BIT0,
		RF_PutDownPhone					= BIT1,
		RF_CalledPolice					= BIT2,
		RF_FailedToCallPolice			= BIT3,
		RF_IsPlayingAudioForCommit		= BIT4,
		RF_PlayerHeardAudioForCommit	= BIT5,
		RF_IsPlayingAudioForCall		= BIT6,
		RF_PlayerHeardAudioForCall		= BIT7,
	};
	
public:

	struct Tunables : CTuning
	{
		Tunables();

		float m_MinTimeMovingAwayToGiveToWitness;
		float m_MinTimeSinceTalkingEndedToMakeCall;
		float m_MinTimeSinceTargetTalkingEndedToMakeCall;
		float m_MinTimeTargetHasBeenTalkingToMakeCall;
		float m_MinTimeSinceTalkingEndedToSayContextForCall;
		float m_MinTimeSpentInEarLoopToSayContextForCall;
		float m_MinTimeToSpendInEarLoopToPutDownPhone;
		float m_MaxTimeToSpendInEarLoopToPutDownPhone;

		PAR_PARSABLE;
	};

public:

	CTaskCallPolice(const CPed* pTarget = NULL);
	virtual ~CTaskCallPolice();

public:

	static bool	CanGiveToWitness(const CPed& rPed, const CPed& rTarget);
	static bool CanMakeCall(const CPed& rPed, const CPed* pTarget = NULL);
	static bool	DoesPedHaveAudio(const CPed& rPed);
	static bool	DoesPedHavePhone(const CPed& rPed);
	static bool	GiveToWitness(CPed& rPed, const CPed& rTarget);

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	bool IsInEarLoop() const;
	bool IsTalking() const;
	bool IsTalking(const CPed& rPed) const;
	bool IsTargetTalking() const;
	void OnCalledPolice();
	void OnFailedToCallPolice();
	void ProcessFlags();
	void ProcessHeadIk();
	void ProcessTimers();
	void PutDownPhone();
	void SayAudio(const CAmbientAudio& rAudio, bool& bIsPlayingAudio, bool& bPlayerHeardAudio);
	bool ShouldPutDownPhone() const;
	bool ShouldSayContextForCall() const;

private:

	static const CAmbientAudio& GetAudioForCall();
	static const CAmbientAudio& GetAudioForCommit();

private:

	virtual s32	GetDefaultStateAfterAbort()	const
	{
		return State_Finish;
	}
	
	virtual s32 GetTaskTypeInternal() const
	{
		return CTaskTypes::TASK_CALL_POLICE;
	}

private:

	virtual void		CleanUp();
	virtual aiTask* 	Copy() const;
	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	WaitForTalkingToFinish_OnUpdate();
	void		MakeCall_OnEnter();
	FSM_Return	MakeCall_OnUpdate();

private:

	RegdConstPed	m_pTarget;
	float			m_fTimeSinceTalkingEnded;
	float			m_fTimeSinceTargetTalkingEnded;
	float			m_fTimeTargetHasBeenTalking;
	float			m_fTimeSpentInEarLoop;
	fwFlags8		m_uRunningFlags;

private:

	static Tunables sm_Tunables;

};

#endif // TASK_CALL_POLICE_H
