// FILE :    TaskChat.h
// PURPOSE : Makes two peds go to each other and do a chat scenario.
//			 Used during patrols.
// AUTHOR :  Chi-Wai Chiu
// CREATED : 17-02-2009

#ifndef INC_TASK_CHAT_H
#define INC_TASK_CHAT_H

// Rage headers
#include "streaming/streamingmodule.h"

// Game headers
#include "AI/AITarget.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

class CClipRequestHelper;

// Declaration
class CTaskChat : public CTask
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Max allowed angle away from the desired heading for going to State_Chatting, in degrees.
		float	m_HeadingToleranceDegrees;

		// PURPOSE:	Max time to wait for the other ped (or proper heading) in State_WaitingToChat, in seconds.
		float	m_MaxWaitTime;

		PAR_PARSABLE;
	};

	// FSM states
	enum 
	{ 
		State_GoToPosition = 0, 
		State_QuickChat,
		State_WaitingToChat,
		State_Chatting,
		State_PlayingClip,
		State_Idle,
		State_TurnToFacePed,
		State_Finish
	};

	enum ChatFlags
	{
		// Options
		CF_IsInitiator					= BIT0,
		CF_DoQuickChat					= BIT1,
		CF_GoToSpecificPosition			= BIT2,
		CF_UseCustomHeading				= BIT3,
		CF_AutoChat						= BIT4,
		CF_PlayGreetingGestures			= BIT5,
		CF_PlayGoodByeGestures			= BIT6,
	
		// Running Flags
		CF_IsInPosition					= BIT7,
		CF_IsPlayingClip				= BIT8,
		CF_ShouldPlayClip				= BIT9,
		CF_ShouldCheckPed				= BIT10,
		CF_ShouldLeave					= BIT11
	};

	CTaskChat(CPed* pPedToChatTo, fwFlags32 iFlags, const Vector3 &vGoToPosition = Vector3(Vector3::ZeroType), float fHeading = 0.0f, float fIdleTime = -1.0f);
	virtual ~CTaskChat() {};

	// Task required implementations
	virtual aiTask*				Copy()		  const { return rage_new CTaskChat(m_pPedToChatTo, m_iFlags, m_vGoToPosition, m_fHeading, m_fIdleTime); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_CHAT; }

	// FSM required implementations
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32				GetDefaultStateAfterAbort()	const { return State_GoToPosition; }

	virtual FSM_Return			ProcessPreFSM();

	// Interface functions
	bool						IsInPosition() { return m_iFlags.IsFlagSet(CF_IsInPosition); }
	bool						IsPlayingClip() { return m_iFlags.IsFlagSet(CF_IsPlayingClip); }
	void						PlayClip(const char* pClipName, const strStreamingObjectName pClipDictName, const u32 iPriority, const float fBlendDelta, const s32 flags, const s32 boneMask, const int iDuration = -1, const s32 iTaskFlags = 0);
	void						LeaveChat(bool bPlayGoodByeGestures) 
	{ 
		if (bPlayGoodByeGestures) 
		{
			m_iFlags.SetFlag(CF_PlayGoodByeGestures);
		}
		m_iFlags.SetFlag(CF_ShouldLeave); 
	}

// debug:
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

protected:
	// FSM State functions
	void						GoToPosition_OnEnter			(CPed* pPed);
	FSM_Return					GoToPosition_OnUpdate		(CPed* pPed);

	void						WaitingToChat_OnEnter	(CPed* pPed);
	FSM_Return					WaitingToChat_OnUpdate	(CPed* pPed);

	void						Chatting_OnEnter		(CPed* pPed);
	FSM_Return					Chatting_OnUpdate		(CPed* pPed);

	void						PlayingClip_OnEnter		(CPed* pPed);
	FSM_Return					PlayingClip_OnUpdate	(CPed* pPed);

	FSM_Return					Idle_OnUpdate			(CPed* pPed);

	FSM_Return					CheckPed_OnUpdate		(CPed* pPed);

	void						TurnToFacePed_OnEnter	(CPed* pPed);
	FSM_Return					TurnToFacePed_OnUpdate	(CPed* pPed);


private:

	// Helper functions
	void RecalculateParameters();

private:

	enum {MAX_NAME_LENGTH = 32}; 

	// Task member variables
	RegdPed				m_pPedToChatTo;
	fwMvClipSetId		m_clipSetId;
	fwMvClipId			m_clipId;
	CTaskGameTimer		m_timer;

	s32					m_iTimeDelay;
	Vector3				m_vPreviousGoToPos;

	float				m_fHeading;
	Vector3				m_vGoToPosition;
	fwFlags32			m_iFlags;
	char				m_szContext[MAX_NAME_LENGTH];

	int					m_clipDictIndex;
	u32					m_clipHashKey;
	s32					m_iTaskFlags;
	s32					m_iClipFlags;
	float				m_fBlendDelta;
	s32					m_iBoneMask;
	s32					m_iPriority;
	s32					m_iDuration;
	float				m_fIdleTime;
	CTaskTimer			m_IdleTimer;
	CTaskTimer			m_WaitTimer;
	bool				m_bStartedIdleTimer;

	static Tunables		sm_Tunables;
};

#endif // INC_TASK_CHAT_H
