#include "TaskConversationHelper.h"

#include "Event/EventShocking.h"
#include "Peds/PedIntelligence.h"
#include "Task/Scenario/Types/TaskUseScenario.h"

AI_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

////////////////////////////////////////////////////////////////////////////////
//CTaskConversationHelper
////////////////////////////////////////////////////////////////////////////////
FW_INSTANTIATE_CLASS_POOL(CTaskConversationHelper, CONFIGURED_FROM_FILE, atHashString("CTaskConversationHelper",0xcc0af84f));

CTaskConversationHelper::Tunables CTaskConversationHelper::sm_Tunables;
IMPLEMENT_GENERAL_TASK_TUNABLES(CTaskConversationHelper, 0xcc0af84f);

const char * CTaskConversationHelper::m_caPhoneConversationLines[m_num_phone_conversations_per_voice][m_num_phone_lines_per_conversation] = 
{
	{ "PHONE_CONV1_INTRO", "PHONE_CONV1_CHAT1", "PHONE_CONV1_CHAT2", "PHONE_CONV1_CHAT3", "PHONE_CONV1_OUTRO" },
	{ "PHONE_CONV2_INTRO", "PHONE_CONV2_CHAT1", "PHONE_CONV2_CHAT2", "PHONE_CONV2_CHAT3", "PHONE_CONV2_OUTRO" },
	{ "PHONE_CONV3_INTRO", "PHONE_CONV3_CHAT1", "PHONE_CONV3_CHAT2", "PHONE_CONV3_CHAT3", "PHONE_CONV3_OUTRO" },
	{ "PHONE_CONV4_INTRO", "PHONE_CONV4_CHAT1", "PHONE_CONV4_CHAT2", "PHONE_CONV4_CHAT3", "PHONE_CONV4_OUTRO" },
	{ "PHONE_CONV5_INTRO", "PHONE_CONV5_CHAT1", "PHONE_CONV5_CHAT2", "PHONE_CONV5_CHAT3", "PHONE_CONV5_OUTRO" },
	{ "PHONE_CONV6_INTRO", "PHONE_CONV6_CHAT1", "PHONE_CONV6_CHAT2", "PHONE_CONV6_CHAT3", "PHONE_CONV6_OUTRO" },
};
const char* CTaskConversationHelper::m_caChatLines[m_numChatLines] = { "CHAT_STATE", "GENERIC_HOWS_IT_GOING" };
const char* CTaskConversationHelper::m_caChatResponses[m_numChatLines] = { "CHAT_RESP", "PED_RANT" } ;

const u8	CTaskConversationHelper::m_ProvokeBumpedLineIndex  = 0;
const u8  CTaskConversationHelper::m_ProvokeStaringLineIndex = 1;
const u8  CTaskConversationHelper::m_ProvokeGenericLineIndex = 2;
const atHashString	CTaskConversationHelper::m_uaArgumentProvokeLines[m_numProvokeLines] =
{
	atHashString("PROVOKE_BUMPED_INTO"),
	atHashString("PROVOKE_STARING"),
	atHashString("PROVOKE_GENERIC")
};

const atHashString	CTaskConversationHelper::m_uChallengeLine = atHashString("CHALLENGE_THREATEN");

const atHashString	CTaskConversationHelper::m_uaChallengeResponseLines[m_numAcceptChallengeLines] =
{
	atHashString("CHALLENGE_ACCEPTED_BUMPED_INTO"),
	atHashString("CHALLENGE_ACCEPTED_STARING"),
	atHashString("CHALLENGE_ACCEPTED_GENERIC")
};

const atHashString  CTaskConversationHelper::m_uApologyLine = atHashString("APOLOGY_NO_TROUBLE");
const atHashString	CTaskConversationHelper::m_uArgumentWonLine = atHashString("WON_DISPUTE");

CTaskConversationHelper::CTaskConversationHelper() : 
	CTaskHelperFSM()
		, m_stateTimer()
		, m_pPed(NULL)
		, m_pNearbyPed(NULL)
		, m_eConversationMode(Mode_Uninitialized)
		, m_uConversationIndex(0)
		, m_uLineIndex(0)
		, m_bFirstPedRetreatsAfterArgument(false) 
		, m_bConversationFailed(false)
	{
	}

	CTaskConversationHelper::~CTaskConversationHelper() 
	{ 
		StopAllSounds();

		if (m_pPed && IsPhoneConversation()) 
		{
			m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedHadPhoneConversation, true);
		}
		CleanupAmbientTaskFlags();
	}


	void CTaskConversationHelper::StartPhoneConversation(CPed* ped)
	{
		if (aiVerify(m_pPed.Get() != ped) && aiVerify(GetState() == State_Start))
		{
			m_pPed = ped;
			SetPhoneConversation();

			if (m_pPed)
			{
				SetInConversationOnAmbientTask(m_pPed, true);

				audSpeechAudioEntity* pAudioEntity = m_pPed->GetSpeechAudioEntity();
				if (aiVerify(pAudioEntity))
				{
					m_uConversationIndex = pAudioEntity->SetAmbientVoiceNameAndGetVariationForPhoneConversation();
					if (m_uConversationIndex > 0) 
					{
						--m_uConversationIndex; // Audio variations are 1-indexed. [8/3/2012 mdawe]
						if (!pAudioEntity->SetPedVoiceForContext(CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][0])) // Check the first line in this conversation [11/20/2012 mdawe]
						{
							// If the ped is missing this line, he's missing all of the lines, so set the failure mode. [11/20/2012 mdawe]
							SetFailureMode();
						}
					}
					else
					{
						//Failure case. This Ped was assigned a voice without recorded phone conversations after
						// the animation checked against all possible voices. (Not very likely to happen, but just in case.)
						SetFailureMode();
					}
				}
			}
		}
	}

	void CTaskConversationHelper::StartHangoutConversation(CPed* ped, CPed* nearbyPed)
	{
		if (aiVerify(m_pPed.Get() != ped) && aiVerify(m_pNearbyPed.Get() != nearbyPed))
		{
			if (ped && nearbyPed) 
			{
				if (aiVerify(GetState() == State_Start))
				{
					m_pPed = ped;
					m_pNearbyPed = nearbyPed;
					SetHangoutConversation();
					SetInConversationOnAmbientTask(m_pPed, true);
					SetInConversationOnAmbientTask(m_pNearbyPed, true);
				}
			}
		}
	}

	void CTaskConversationHelper::StartHangoutArgumentConversation(CPed* ped, CPed* nearbyPed)
	{
		if (aiVerify(m_pPed.Get() != ped) && aiVerify(m_pNearbyPed.Get() != nearbyPed))
		{
			if (ped && nearbyPed) 
			{
				if (aiVerify(GetState() == State_Start))
				{
					m_pPed = ped;
					m_pNearbyPed = nearbyPed;
					ChooseArgumentVoicetypes();
					SetArgumentConversation();
					SetInConversationOnAmbientTask(m_pPed, true);
					SetInConversationOnAmbientTask(m_pNearbyPed, true);
				}
			}
		}
	}


	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ProcessPreFSM()
	{
		if (m_pPed)
		{
			m_pPed->SetGestureAnimsAllowed(true);
		}

		// If we're supposed to be on the phone, ensure the prop is still loaded and held [1/23/2013 mdawe]
		if (GetState() >= FIRST_PHONE_CONVERSATION_LINE_STATE && GetState() <= LAST_PHONE_CONVERSATION_LINE_STATE)
		{
			if (!IsHoldingProp())
			{
				CleanupAmbientTaskFlags();
				return FSM_Quit;
			}
		}

		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::UpdateFSM(const s32 iState, const FSM_Event iEvent)
	{
		FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
			return StartState_OnUpdate();

		// ----------------- MOBILE PHONE STATES -----------------
		FSM_State(State_WaitingForModel)
			FSM_OnUpdate
			return WaitingForModel_OnUpdate();

		FSM_State(State_PhoneWaitForRingTone)
			FSM_OnUpdate
				return PhoneWaitForRingTone_OnUpdate();

		FSM_State(State_PhoneRinging)
			FSM_OnEnter
				return PhoneRinging_OnEnter();
			FSM_OnUpdate
				return PhoneRinging_OnUpdate();
			FSM_OnExit
				return PhoneRinging_OnExit();

		FSM_State(State_PhonePlayFirstLine)
			FSM_OnUpdate
			return PhonePlayFirstLine_OnUpdate();

		FSM_State(State_PhonePlayLine)
			FSM_OnUpdate
			return PhonePlayLine_OnUpdate();

		FSM_State(State_PhoneWaitForAudio)
			FSM_OnUpdate
			return PhoneWaitForAudio_OnUpdate();

		FSM_State(State_PhoneDelayBetweenLines)
			FSM_OnUpdate
			return PhoneDelayBetweenLines_OnUpdate();

		FSM_State(State_PhoneStartHangup)
			FSM_OnUpdate
			return PhoneStartHangup_OnUpdate();

		// --------------- HANGOUT STATES --------------------
		FSM_State(State_HangoutChat)
			FSM_OnUpdate
			return HangoutChat_OnUpdate();

		FSM_State(State_HangoutWaitForAudio)
			FSM_OnUpdate
			return HangoutWaitForAudio_OnUpdate();

		FSM_State(State_HangoutDelayAfterLines)
			FSM_OnUpdate
			return HangoutDelayAfterLines_OnUpdate();

		// --------------- ARGUMENT STATES --------------------
		FSM_State(State_ArgumentProvoke)
			FSM_OnUpdate
			return ArgumentProvoke_OnUpdate();

		FSM_State(State_ArgumentChallenge)
			FSM_OnUpdate
			return ArgumentChallenge_OnUpdate();

		FSM_State(State_ArgumentApology)
			FSM_OnUpdate
			return ArgumentApology_OnUpdate();

		FSM_State(State_ArgumentWon)
			FSM_OnUpdate
			return ArgumentWon_OnUpdate();

		FSM_State(State_ArgumentFinished)
			FSM_OnUpdate
			return ArgumentFinished_OnUpdate();

		//-------------- CONVERSATION OVER --------------------
		FSM_State(State_ConversationOver)
			FSM_OnUpdate
			return ConversationOver_OnUpdate();

		FSM_End
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::StartState_OnUpdate()
	{
		if (IsFailure())
		{
			SetState(State_ConversationOver);
		}
		else if (IsPhoneConversation()) 
		{
			if (IsHoldingProp())
			{
				// If this ped was generated holding the phone, go straight to conversation [1/23/2013 mdawe]
				SetState(State_PhonePlayFirstLine);
			}
			else
			{
				// ...otherwise wait for the model to load
				SetState(State_WaitingForModel);
			}
		}
		else if (IsHangoutConversation())
		{
			SetState(State_HangoutChat);
		}
		else if (IsArgumentConversation())
		{
			SetState(State_ArgumentProvoke);
		}
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::WaitingForModel_OnUpdate()
	{
		aiAssert(IsPhoneConversation());
		if (IsPropModelLoaded())
		{
			SetState(State_PhoneWaitForRingTone);
		}
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneWaitForRingTone_OnUpdate()
	{	
		// Once the clip signals to start playing the ringtone, go ahead and play it [1/22/2013 mdawe]
		if (m_conversationHelperFlags.IsFlagSet(Flag_PlayRingTone))
		{
			SetState(State_PhoneRinging);
		}
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneRinging_OnEnter()
	{
		// Set a timer to make sure we don't play the ringtone forever if data doesn't tell us when to stop. [1/22/2013 mdawe]
		m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(), CTaskConversationHelper::sm_Tunables.m_uMaxTimeInMSToPlayRingTone);

		// Start playing the ringtone [1/22/2013 mdawe]
		if (m_pPed)
		{
			audPedAudioEntity* pPedAudioEntity = m_pPed->GetPedAudioEntity();
			if (!pPedAudioEntity->IsRingTonePlaying())
			{
				pPedAudioEntity->PlayRingTone();
			}
		}

		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneRinging_OnUpdate()
	{
		// If we've been signalled to stop playing the ringtone or have hit the max time to play it, stop playing the ringtone. [1/22/2013 mdawe]
		if ( !m_conversationHelperFlags.IsFlagSet(Flag_PlayRingTone) || m_stateTimer.IsOutOfTime())
		{
			SetState(State_PhonePlayFirstLine);
		}
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneRinging_OnExit()
	{
		StopRingTone();
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhonePlayFirstLine_OnUpdate()
	{
		if (m_pPed && aiVerify(m_uLineIndex == 0))
		{
			if (aiVerify(m_uConversationIndex < m_num_phone_conversations_per_voice && 
				m_uLineIndex < m_num_phone_lines_per_conversation))
			{
				audSpeechAudioEntity* pAudioEntity = m_pPed->GetSpeechAudioEntity();
				if (aiVerifyf(pAudioEntity && pAudioEntity->DoesContextExistForThisPed(CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][m_uLineIndex]), "Ped type %s missing audio line %s", m_pPed->GetDebugName(), CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][m_uLineIndex])) 
				{
					if (IsPlayerWithinPhoneAudioDistance()) 
					{
						// Say the first line in the phone conversation, which is a force load by design. [7/30/2012 mdawe]
						m_pPed->NewSayWithParams(CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][m_uLineIndex],  "SPEECH_PARAMS_FORCE_NORMAL");
					}
					// Move on to the next line regardless of whether we actually played the audio. We could wait until the player is in range to start, but that seems artificial. [8/13/2012 mdawe]
					++m_uLineIndex;
					SetState(State_PhoneWaitForAudio);
					return FSM_Continue;
				}
				else 
				{
					//If we somehow no longer have the audio context, fail out
					SetState(State_ConversationOver);
					return FSM_Continue;
				}
			}
		}
		//End the conversation if our Ped no longer exists.
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhonePlayLine_OnUpdate()
	{	
		if (m_pPed && aiVerify(m_uLineIndex > 0) && aiVerify(m_uLineIndex <= m_num_phone_lines_per_conversation))
		{
			if (aiVerify(m_uConversationIndex < m_num_phone_conversations_per_voice))
			{
				audSpeechAudioEntity* pAudioEntity = m_pPed->GetSpeechAudioEntity();
				if (aiVerifyf(pAudioEntity && pAudioEntity->DoesContextExistForThisPed(CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][m_uLineIndex]), "Ped type %s missing audio line %s", m_pPed->GetDebugName(), CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][m_uLineIndex])) 
				{
					if (IsPlayerWithinPhoneAudioDistance()) 
					{
						// Say lines 2-[end] of the conversation. This can fail if the disk is busy, so we may not move on from this state. [7/30/2012 mdawe]		
						if (m_pPed->NewSay(CTaskConversationHelper::m_caPhoneConversationLines[m_uConversationIndex][m_uLineIndex]))
						{
							++m_uLineIndex;
							SetState(State_PhoneWaitForAudio);
						}
						else
						{
							// If NewSay fails, wait for a second before trying this line again [8/13/2012 mdawe]
							m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(), CTaskConversationHelper::sm_Tunables.m_uTimeToWaitAfterNewSayFailureInSeconds * 1000);
							SetState(State_PhoneDelayBetweenLines);
						}
					}
					return FSM_Continue;
				}
				else 
				{
					//If we somehow no longer have the audio context, fail out
					SetState(State_ConversationOver);
					return FSM_Continue;
				}
			}
		}
		//End the conversation if our Ped no longer exists.
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneWaitForAudio_OnUpdate()
	{
		if (m_pPed)
		{
			audSpeechAudioEntity* pAudioEntity = m_pPed->GetSpeechAudioEntity();
			if (aiVerify(pAudioEntity))
			{
				// If we're still playing audio, spin. 
				if (!pAudioEntity->IsAmbientSpeechPlaying())
				{
					// Audio is finished, delay before playing the next line...
					if (m_uLineIndex < m_num_phone_lines_per_conversation)
					{
						float seconds_to_delay = fwRandom::GetRandomNumberInRange(CTaskConversationHelper::sm_Tunables.m_fMinSecondsDelayBetweenPhoneLines, CTaskConversationHelper::sm_Tunables.m_fMaxSecondsDelayBetweenPhoneLines);
						m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(), static_cast<int>(seconds_to_delay * 1000));
						SetState(State_PhoneDelayBetweenLines);
					}
					//...or hang up the phone if we've played every line. [7/30/2012 mdawe]
					else
					{
						SetState(State_PhoneStartHangup);
					}
				}
				return FSM_Continue;
			}
		}
		//End the conversation if our Ped no longer exists.
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneDelayBetweenLines_OnUpdate()
	{
		CheckForAndMakeWeirdPedComment();

		// Wait until the timer is done before playing the next line [7/30/2012 mdawe]
		if (m_stateTimer.IsOutOfTime())
		{
			SetState(State_PhonePlayLine);
		}
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::PhoneStartHangup_OnUpdate()
	{
		if (m_pPed)
		{
			CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pTask)
			{
				// End this phone conversation. [8/21/2012 mdawe]
				pTask->SetInConversation(false);
				// Force CTaskAmbientClips to start our Exit Anim
				pTask->SetTimeTillNextClip(-1.0f);

				// When the exit anim is finished, CTaskAmbientClips should quit. [8/21/2012 mdawe]
				pTask->QuitAfterIdle();
			}    
		}
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::HangoutChat_OnUpdate()
	{
		if (m_pPed && m_pNearbyPed)
		{
			m_stateTimer.Unset();
			audSpeechAudioEntity* pAudioEntity = m_pPed->GetSpeechAudioEntity();

			u32 uChatLineIndex = GetChatLineIndexToUse();

			if (pAudioEntity && pAudioEntity->DoesContextExistForThisPed(CTaskConversationHelper::m_caChatLines[uChatLineIndex]))
			{
				// Play the lines. This can fail if the disc is busy. [7/30/2012 mdawe]
				if ( m_pPed->NewSay(CTaskConversationHelper::m_caChatLines[uChatLineIndex], 0, false, false, -1, m_pNearbyPed, atStringHash(CTaskConversationHelper::m_caChatResponses[uChatLineIndex])) )
				{
					SetState(State_HangoutWaitForAudio);
				}
				else 
				{
					SetState(State_ConversationOver);
				}
				return FSM_Continue;
			}
		}
		//End the conversation if our Ped no longer exists.
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::HangoutWaitForAudio_OnUpdate()
	{
		if (m_pPed && m_pNearbyPed)
		{
			audSpeechAudioEntity* pPedAudioEntity = m_pPed->GetSpeechAudioEntity();
			if (!pPedAudioEntity->IsAmbientSpeechPlaying()) 
			{
				audSpeechAudioEntity* pNearbyPedAudioEntity = m_pNearbyPed->GetSpeechAudioEntity();
				if (!pNearbyPedAudioEntity->IsAmbientSpeechPlaying())
				{
					float seconds_to_delay = fwRandom::GetRandomNumberInRange(CTaskConversationHelper::sm_Tunables.m_fMinSecondsDelayBetweenChatLines, CTaskConversationHelper::sm_Tunables.m_fMaxSecondsDelayBetweenChatLines);
					m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(), static_cast<int>(seconds_to_delay * 1000));
					SetState(State_HangoutDelayAfterLines);
				}
			}
		}

		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::HangoutDelayAfterLines_OnUpdate()
	{
		if (m_stateTimer.IsOutOfTime())
		{
			SetState(State_ConversationOver);
		}
		return FSM_Continue;
	}


	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ArgumentProvoke_OnUpdate()
	{
		if (m_pPed && m_pNearbyPed)
		{
			audSpeechAudioEntity* pPedAudioEntity = m_pPed->GetSpeechAudioEntity();
			if (pPedAudioEntity && !pPedAudioEntity->IsAmbientSpeechPlaying()) 
			{
				u32 uProvokeLineIndex = fwRandom::GetRandomNumberInRange(0, m_numProvokeLines);
				aiAssert(uProvokeLineIndex < m_numProvokeLines);
				if ( m_pPed->NewSay(CTaskConversationHelper::m_uaArgumentProvokeLines[uProvokeLineIndex]) )
				{
					// We played the line successfully. First, note if we played a "bumped into" or "staring" line:
					if (uProvokeLineIndex == CTaskConversationHelper::m_ProvokeBumpedLineIndex)
					{
						m_conversationHelperFlags.SetFlag(Flag_PlayedBumpedIntoProvoke);
					}
					else if (uProvokeLineIndex == CTaskConversationHelper::m_ProvokeStaringLineIndex)
					{
						m_conversationHelperFlags.SetFlag(Flag_PlayedStaringProvoke);
					}

					//See if we should move to the challenge state
					if (fwRandom::GetRandomNumberInRange(0.f, 1.f) < CTaskConversationHelper::sm_Tunables.m_fChanceOfArgumentChallenge)
					{
						SetState(State_ArgumentChallenge);
					}
					else
					{
						SetState(State_ArgumentApology);
						// The second ped is apologizing and will retreat. [6/13/2013 mdawe]
						m_bFirstPedRetreatsAfterArgument = false;
					}
					return FSM_Continue;
				}
				else
				{
					//Failed to NewSay the Provoke, so just end the conversation.
					SetState(State_ConversationOver);
					return FSM_Continue;
				}
			}

			// Peds exist but were saying some other line.
			SetState(State_ConversationOver);
			return FSM_Continue;
		}

		//End the conversation if our Ped no longer exists.
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ArgumentChallenge_OnUpdate()
	{
		// Challenges always start with the second ped.
		if (m_pPed && m_pNearbyPed)
		{
			audSpeechAudioEntity* pPedAudioEntity				= m_pPed->GetSpeechAudioEntity();
			audSpeechAudioEntity* pNearbyPedAudioEntity = m_pNearbyPed->GetSpeechAudioEntity();
			if (pPedAudioEntity && pNearbyPedAudioEntity)	
			{
				if (pPedAudioEntity->IsAmbientSpeechPlaying() || pNearbyPedAudioEntity->IsAmbientSpeechPlaying())
				{
					return FSM_Continue;
				}
				else
				{
					// Roll to see if the first ped will respond to the second ped's challenge.
					if (fwRandom::GetRandomNumberInRange(0.f, 1.f) < sm_Tunables.m_fChanceOfArgumentChallengeBeingAccepted)
					{
						// m_pPed will respond. m_pNearbyPed will retreat.
						m_bFirstPedRetreatsAfterArgument = false;

						// Need to roll for the challenge response line.
						u32 uChallengeResponseLine = fwRandom::GetRandomNumberInRange(0, CTaskConversationHelper::m_numAcceptChallengeLines);

						// We can't mix the BumpedInto provoke line with the Staring response line and vice versa
						if (uChallengeResponseLine == CTaskConversationHelper::m_ProvokeBumpedLineIndex && m_conversationHelperFlags.IsFlagSet(Flag_PlayedStaringProvoke))
						{
							uChallengeResponseLine = CTaskConversationHelper::m_ProvokeGenericLineIndex;
						}
						else if (uChallengeResponseLine == CTaskConversationHelper::m_ProvokeStaringLineIndex && m_conversationHelperFlags.IsFlagSet(Flag_PlayedBumpedIntoProvoke))
						{
							uChallengeResponseLine = CTaskConversationHelper::m_ProvokeGenericLineIndex;
						}
						if (aiVerify(uChallengeResponseLine < CTaskConversationHelper::m_numAcceptChallengeLines))
						{
							// Say the challenge with a response.
							m_pNearbyPed->NewSay(CTaskConversationHelper::m_uChallengeLine, 0, false, false, -1, m_pPed, CTaskConversationHelper::m_uaChallengeResponseLines[uChallengeResponseLine]);
						}
					}
					else
					{
						// m_pPed retreats from the challenge
						m_bFirstPedRetreatsAfterArgument = true;

						// m_pNearbyPed speaks the challenge unanswered
						m_pNearbyPed->NewSay(CTaskConversationHelper::m_uChallengeLine);
					}

					SetState(State_ArgumentApology);
					return FSM_Continue;
				}
			}
		}

		// End the conversation if the peds no longer exist
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ArgumentApology_OnUpdate()
	{
		if (m_pPed && m_pNearbyPed)
		{
			audSpeechAudioEntity* pPedAudioEntity				= m_pPed->GetSpeechAudioEntity();
			audSpeechAudioEntity* pNearbyPedAudioEntity = m_pNearbyPed->GetSpeechAudioEntity();
			if (pPedAudioEntity && pNearbyPedAudioEntity)
			{
				if (pPedAudioEntity->IsAmbientSpeechPlaying() || pNearbyPedAudioEntity->IsAmbientSpeechPlaying())
				{
					return FSM_Continue;
				}
				else
				{
					if (m_bFirstPedRetreatsAfterArgument)
					{
						m_pPed->NewSay(CTaskConversationHelper::m_uApologyLine);
						TriggerScenarioQuit(m_pPed);
					}
					else
					{
						m_pNearbyPed->NewSay(CTaskConversationHelper::m_uApologyLine);
						TriggerScenarioQuit(m_pNearbyPed);
					}
					SetState(State_ArgumentWon);
					return FSM_Continue;
				}
			}
		}

		// End the conversation if the peds no longer exist
		SetState(State_ConversationOver);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ArgumentWon_OnUpdate()
	{
		if (m_pPed && m_pNearbyPed)
		{
			audSpeechAudioEntity* pPedAudioEntity				= m_pPed->GetSpeechAudioEntity();
			audSpeechAudioEntity* pNearbyPedAudioEntity = m_pNearbyPed->GetSpeechAudioEntity();
			if (pPedAudioEntity && pNearbyPedAudioEntity)
			{
				if (pPedAudioEntity->IsAmbientSpeechPlaying() || pNearbyPedAudioEntity->IsAmbientSpeechPlaying())
				{
					return FSM_Continue;
				}
				else
				{
					if (m_bFirstPedRetreatsAfterArgument)
					{
						m_pNearbyPed->NewSay(CTaskConversationHelper::m_uArgumentWonLine);
					}
					else
					{
						m_pPed->NewSay(CTaskConversationHelper::m_uArgumentWonLine);
					}
				}
			}
		}
	
		SetState(State_ArgumentFinished);
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ArgumentFinished_OnUpdate()
	{
		// A holding state so that the peds that had the argument will remember it and not start a new one.
		return FSM_Continue;
	}

	CTaskHelperFSM::FSM_Return CTaskConversationHelper::ConversationOver_OnUpdate()
	{
		CleanupAmbientTaskFlags();
		return FSM_Quit;
	}

	void CTaskConversationHelper::SetPhoneConversation()
	{
		if (aiVerify(IsModeUninitialized()))
		{
			m_eConversationMode = Mode_Phone;
		}
	}

	void CTaskConversationHelper::SetHangoutConversation()
	{ 
		if (aiVerify(IsModeUninitialized()))
		{
			m_eConversationMode = Mode_Hangout;
		}
	}

	void CTaskConversationHelper::SetArgumentConversation()
	{
		if (aiVerify(IsModeUninitialized()))
		{
			m_eConversationMode = Mode_Argument;
		}
	}

	void CTaskConversationHelper::SetFailureMode()
	{
		m_bConversationFailed = true;
		if (m_pPed)
		{
			SetInConversationOnAmbientTask(m_pPed, false);
		}
		if (m_pNearbyPed)
		{
			SetInConversationOnAmbientTask(m_pNearbyPed, false);
		}
	}

	void CTaskConversationHelper::ChooseArgumentVoicetypes()
	{
		if (aiVerify(m_pPed && m_pNearbyPed))
		{
			audSpeechAudioEntity* pFirstAudioEntity  = m_pPed->GetSpeechAudioEntity();
			audSpeechAudioEntity* pSecondAudioEntity = m_pNearbyPed->GetSpeechAudioEntity();
			if (pFirstAudioEntity && pSecondAudioEntity)
			{
				bool bFirstPedHasProvokeLine = false;
				for (s32 iProvokeIndex = 0; iProvokeIndex < CTaskConversationHelper::m_numProvokeLines && !bFirstPedHasProvokeLine; ++iProvokeIndex)
				{
					bFirstPedHasProvokeLine = pFirstAudioEntity->SetPedVoiceForContext(CTaskConversationHelper::m_uaArgumentProvokeLines[iProvokeIndex]);
				}

				pSecondAudioEntity->SetPedVoiceForContext(CTaskConversationHelper::m_uChallengeLine);
			}
		}
	}
	
	void CTaskConversationHelper::SetInConversationOnAmbientTask(CPed* pPed, bool bOnOff)
	{
		if (aiVerify(pPed))
		{
			CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			taskAssert(pTask || !bOnOff);
			if (pTask)
			{
				pTask->SetInConversation(bOnOff);
			}    
		}
	}

	void CTaskConversationHelper::TriggerScenarioQuit(CPed* pQuittingPed)
	{
		if (aiVerify(pQuittingPed))
		{
			CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario *>(pQuittingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (pTaskUseScenario)
			{
				pTaskUseScenario->SetTimeToLeave(0.0f);
			}
		}
	}

	bool CTaskConversationHelper::IsPlayerWithinPhoneAudioDistance()
	{
		CPed* pPlayerPed = FindPlayerPed();
		if (m_pPed && pPlayerPed)
		{
			// Calculate if the player is within our tunable-defined distance. [8/13/2012 mdawe]
			const float fDistSqr = DistSquared(pPlayerPed->GetTransform().GetPosition(), m_pPed->GetTransform().GetPosition()).Getf();
			return (fDistSqr < CTaskConversationHelper::sm_Tunables.m_fMinDistanceSquaredToPlayerForAudio);
		}
		return false;
	}

	void CTaskConversationHelper::CheckForAndMakeWeirdPedComment()
	{
		// If we've already made a "Weird Ped" comment, don't make another one. [8/14/2012 mdawe]
		if (m_pPed && !HasMadeWeirdPedComment())
		{
			// Make sure we can actually make a comment. [8/14/2012 mdawe]
			audSpeechAudioEntity* pAudioEntity = m_pPed->GetSpeechAudioEntity();
			if (pAudioEntity && pAudioEntity->DoesContextExistForThisPed("PHONE_SURPRISE_PLAYER_APPEARANCE"))
			{
				CPed* pPlayerPed = FindPlayerPed();
				if (pPlayerPed)
				{
					// Don't make a "Weird Ped" comment if another ped made one recently [8/14/2012 mdawe]
					CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
					if (pPlayerInfo && (fwTimer::GetTimeInMilliseconds() - pPlayerInfo->GetLastTimePedCommentedOnWeirdPlayer() > CTaskConversationHelper::sm_Tunables.m_uTimeInMSUntilNewWeirdPedComment))
					{

						// Search for the ShockingWeirdPed event [8/14/2012 mdawe]
						const CEventGroupGlobal* list = GetEventGlobalGroup();
						const int num = list->GetNumEvents();
						for(int i = 0; i < num; i++)
						{
							const fwEvent* ev = list->GetEventByIndex(i);
							if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
							{
								continue;
							}
							const CEventShocking* pEvent = static_cast<const CEventShocking*>(ev);

							if (pEvent->GetEventType() == EVENT_SHOCKING_SEEN_WEIRD_PED)
							{
								// Check to see that we're within visual range of the event
								if (DistSquared(m_pPed->GetTransform().GetPosition(), pEvent->GetEntityOrSourcePosition()).Getf() < square(pEvent->GetTunables().m_VisualReactionRange))
								{
									// Make sure we can see the player that started this event
									if (m_pPed->GetPedIntelligence()->GetPedPerception().IsInSeeingRange(pPlayerPed))
									{
										m_pPed->NewSay("PHONE_SURPRISE_PLAYER_APPEARANCE");

										// Mark the last time any ped has made a comment about this player [8/15/2012 mdawe]
										pPlayerInfo->SetLastTimePedCommentedOnWeirdPlayer();

										// Mark that we've made a weird ped comment so we don't do it again [8/15/2012 mdawe]
										SetMadeWeirdPedComment();
										return;
									}
								}
							}
						}
					}
				}
			} 
		}
	}

	bool CTaskConversationHelper::IsHoldingProp() const
	{
		if (aiVerify(m_pPed))
		{
			const CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (aiVerify(pTask))
			{
				if (pTask->GetPropHelper() && pTask->GetPropHelper()->IsHoldingProp())
				{
					return true;
				}
			}
		}	
		return false;
	}

	bool CTaskConversationHelper::IsPropModelLoaded() const
	{
		if (aiVerify(m_pPed))
		{
			CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (aiVerify(pTask))
			{
				if (pTask->GetPropHelper() && pTask->GetPropHelper()->IsPropLoaded())
				{
					return true;
				}
			}
		}
		return false;
	}


	void CTaskConversationHelper::CleanupAmbientTaskFlags()
	{
		if (m_pPed)
		{
			SetInConversationOnAmbientTask(m_pPed, false);
		}
		if (m_pNearbyPed)
		{
			SetInConversationOnAmbientTask(m_pNearbyPed, false);
		}
	}

	void CTaskConversationHelper::StopRingTone()
	{
		if (m_pPed)
		{
			audPedAudioEntity* pPedAudioEntity = m_pPed->GetPedAudioEntity();
			if (pPedAudioEntity && pPedAudioEntity->IsRingTonePlaying())
			{
				pPedAudioEntity->StopRingTone();
			}
		}
	}

	void CTaskConversationHelper::StopAllSounds()
	{
		if (m_pPed)
		{
			audPedAudioEntity* pPedAudioEntity = m_pPed->GetPedAudioEntity();
			if (pPedAudioEntity)
			{
				// This also sets an internal pPedAudioEntity::m_shouldPlayRingTone = false, so best to call it as well as pPedAudioEntity->StopAllSounds()
				pPedAudioEntity->StopRingTone();
				pPedAudioEntity->StopAllSounds();
			}
			audSpeechAudioEntity* pSpeechAudioEntity = m_pPed->GetSpeechAudioEntity();
			if (pSpeechAudioEntity)
			{
				pSpeechAudioEntity->StopSpeech();
			}
    }
	}

  u32 CTaskConversationHelper::GetChatLineIndexToUse() const
	{
		CompileTimeAssert(CTaskConversationHelper::m_numChatLines == 2); //The logic here will need to change if this fails.
		
		u32 uChatLineIndex = 0;
		DEV_ONLY(static bool bForceRantLines = false;)
		if (fwRandom::GetRandomNumberInRange(0.f, 1.f) < CTaskConversationHelper::sm_Tunables.m_fChanceOfConversationRant DEV_ONLY(|| bForceRantLines))
		{
			if( m_pNearbyPed && m_pNearbyPed->GetSpeechAudioEntity() && m_pNearbyPed->GetSpeechAudioEntity()->DoesContextExistForThisPed(CTaskConversationHelper::m_caChatResponses[uChatLineIndex]) )
			{
				uChatLineIndex = 1;
			}
		}
		return uChatLineIndex;
	}
