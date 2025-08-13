#ifndef _TASK_CONVERSATION_HELPER_H
#define _TASK_CONVERSATION_HELPER_H

#include "task/System/TaskHelperFSM.h"
#include "Peds/ped.h"

////////////////////////////////////////////////////////////////////////////////

class CTaskConversationHelper : public CTaskHelperFSM
{
public:
#define CONVERSATION_HELPER_STATES(X) \
	  X(State_Start),										\
		X(State_WaitingForModel),					\
		X(State_PhoneWaitForRingTone),		\
		X(State_PhoneRinging),						\
		X(State_PhonePlayFirstLine),			\
		X(State_PhonePlayLine),						\
		X(State_PhoneWaitForAudio),				\
		X(State_PhoneDelayBetweenLines),	\
		X(State_PhoneStartHangup),				\
		X(State_HangoutChat),							\
		X(State_HangoutReply),						\
		X(State_HangoutWaitForAudio),			\
		X(State_HangoutDelayAfterLines),	\
		X(State_ArgumentProvoke),					\
		X(State_ArgumentChallenge),				\
		X(State_ArgumentApology),					\
		X(State_ArgumentWon),							\
		X(State_ArgumentFinished),				\
		X(State_ConversationOver) 


enum SearchState
	{
		CONVERSATION_HELPER_STATES(DEFINE_TASK_ENUM),

		FIRST_PHONE_CONVERSATION_LINE_STATE = State_PhonePlayFirstLine,
		LAST_PHONE_CONVERSATION_LINE_STATE  = State_PhoneStartHangup
	};

	FW_REGISTER_CLASS_POOL(CTaskConversationHelper);

	CTaskConversationHelper();
	virtual ~CTaskConversationHelper();

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();
		float	m_fMinSecondsDelayBetweenPhoneLines;
		float	m_fMaxSecondsDelayBetweenPhoneLines;
		float	m_fMinSecondsDelayBetweenChatLines;
		float	m_fMaxSecondsDelayBetweenChatLines;
		float m_fMinDistanceSquaredToPlayerForAudio;
		float m_fChanceOfConversationRant;
		float m_fChanceOfArgumentChallenge;
		float m_fChanceOfArgumentChallengeBeingAccepted;
		u32		m_uTimeInMSUntilNewWeirdPedComment;
		u32   m_uMaxTimeInMSToPlayRingTone;
		u8		m_uTimeToWaitAfterNewSayFailureInSeconds;
		u8		m_uTicksUntilHangoutConversationCheck;
		PAR_PARSABLE;
	};

	void StartPhoneConversation(CPed* ped);
	void StartHangoutConversation(CPed* ped, CPed* nearbyPed);
	void StartHangoutArgumentConversation(CPed* ped, CPed* nearbyPed);

	inline bool IsPhoneConversation() const		 { return m_eConversationMode == Mode_Phone; }
	inline bool IsHangoutConversation() const	 { return m_eConversationMode == Mode_Hangout ; }
	inline bool IsArgumentConversation() const { return m_eConversationMode == Mode_Argument; }
	inline bool IsModeUninitialized() const			{ return m_eConversationMode == Mode_Uninitialized; }
	inline bool IsFailure() const								{ return m_bConversationFailed; }

	void SetRingToneFlag() { m_conversationHelperFlags.SetFlag(Flag_PlayRingTone); }
	void ClearRingToneFlag()  { m_conversationHelperFlags.ClearFlag(Flag_PlayRingTone); }

	void CleanupAmbientTaskFlags();

#if !__NO_OUTPUT
	virtual const char* GetStateName() const { static const char* StateNames[] = {	CONVERSATION_HELPER_STATES(DEFINE_TASK_STRING) };	return StateNames[GetState()];				}
#endif // !__NO_OUTPUT

#if !__FINAL
	// PURPOSE: Display debug information specific to this task helper
	virtual const char* GetName() const { return "TASK_CONVERSATION_HELPER"; }

	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_ConversationOver, CONVERSATION_HELPER_STATES);
#if __BANK
	static bool			ms_bRenderStateDebug;
#endif // __BANK
#endif // !__FINAL

protected:
	// PURPOSE: FSM implementation
	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:
	enum ConversationMode
	{
		Mode_Uninitialized,
		Mode_Phone,
		Mode_Hangout,
		Mode_Argument
	};
	enum ConversationFlags
	{
		Flag_MadeWeirdPedCommment			= BIT0,
		Flag_PlayRingTone							= BIT1,
		Flag_PlayedBumpedIntoProvoke	= BIT2,
		Flag_PlayedStaringProvoke			= BIT3,
	};


	void SetPhoneConversation();
	void SetHangoutConversation();
	void SetArgumentConversation();
	void SetFailureMode();

	void ChooseArgumentVoicetypes();

	inline void SetMadeWeirdPedComment()				{ m_conversationHelperFlags.SetFlag(Flag_MadeWeirdPedCommment); }
	inline bool HasMadeWeirdPedComment() const	{ return m_conversationHelperFlags.IsFlagSet(Flag_MadeWeirdPedCommment); }

	void SetInConversationOnAmbientTask(CPed* ped, bool onOff);
	void TriggerScenarioQuit(CPed* pQuittingPed);

	// Returns true if the player is within the Tunable distance to play audio phone conversations. [8/13/2012 mdawe]
	bool IsPlayerWithinPhoneAudioDistance(); 

	// Check for the existence of the WeirdPed shocking event, and if we meet the conditions for it, make an audio comment
	void CheckForAndMakeWeirdPedComment();

	bool IsHoldingProp() const;
	bool IsPropModelLoaded() const;

	void StopRingTone();
	void StopAllSounds();

	u32 GetChatLineIndexToUse() const;

public:
	// PURPOSE: Instance of tunable task params
	static Tunables	sm_Tunables;	

private:
	FSM_Return StartState_OnUpdate();
	
	// ----------- Phone Mode States ----------------
	FSM_Return WaitingForModel_OnUpdate();
	FSM_Return PhoneWaitForRingTone_OnUpdate();
	FSM_Return PhoneRinging_OnEnter();
	FSM_Return PhoneRinging_OnUpdate();
	FSM_Return PhoneRinging_OnExit();
	FSM_Return PhonePlayFirstLine_OnUpdate();
	FSM_Return PhonePlayLine_OnUpdate();
	FSM_Return PhoneWaitForAudio_OnUpdate();
	FSM_Return PhoneDelayBetweenLines_OnUpdate();
	FSM_Return PhoneStartHangup_OnUpdate();
	
	// ------------ Hangout Mode States --------------
	FSM_Return HangoutChat_OnUpdate();
	FSM_Return HangoutWaitForAudio_OnUpdate();
	FSM_Return HangoutDelayAfterLines_OnUpdate();

	// ------------ Argument Mode States -------------
	FSM_Return ArgumentProvoke_OnUpdate();
	FSM_Return ArgumentChallenge_OnUpdate();
	FSM_Return ArgumentApology_OnUpdate();
	FSM_Return ArgumentWon_OnUpdate();
	FSM_Return ArgumentFinished_OnUpdate();

	FSM_Return ConversationOver_OnUpdate();

	CTaskGameTimer m_stateTimer;
	RegdPed m_pPed;
	RegdPed m_pNearbyPed;
	ConversationMode m_eConversationMode;
	u8 m_uConversationIndex;
	u8 m_uLineIndex;
	fwFlags8 m_conversationHelperFlags;
	u8 m_bFirstPedRetreatsAfterArgument : 1; // One ped always retreats at the end of the argument. This keeps track of which one.
	u8 m_bConversationFailed : 1;

	static const u8			m_num_phone_conversations_per_voice = 6;
	static const u8			m_num_phone_lines_per_conversation = 5;
	static const char*	m_caPhoneConversationLines[m_num_phone_conversations_per_voice][m_num_phone_lines_per_conversation];

	static const u8			m_numChatLines = 2;
	static const char*	m_caChatLines[m_numChatLines];
	static const char*	m_caChatResponses[m_numChatLines];

	static const u8						m_numProvokeLines = 3;
	static const u8						m_ProvokeBumpedLineIndex;
	static const u8						m_ProvokeStaringLineIndex;
	static const u8						m_ProvokeGenericLineIndex;
	static const atHashString m_uaArgumentProvokeLines[m_numProvokeLines];

	static const atHashString m_uChallengeLine;

	static const u8						m_numAcceptChallengeLines = 3;
	static const atHashString	m_uaChallengeResponseLines[m_numAcceptChallengeLines];

	static const atHashString m_uApologyLine;
	static const atHashString	m_uArgumentWonLine;
};

#endif //_TASK_CONVERSATION_HELPER_H
