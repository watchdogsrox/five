

// Rage headers
#include "audioengine/engine.h"
#include "audioengine/environment.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "rline/socialclub/rlsocialclub.h"
#include "script/wrapper.h"

// Game headers
#include "audio/cutsceneaudioentity.h"
#include "audio/dynamicmixer.h"
#include "audio/emitteraudioentity.h"
#include "audio/vehicleengine/vehicleengine.h"
#include "audio/vehiclereflectionsaudioentity.h"
#include "audio/environment/environment.h"
#include "audio/frontendaudioentity.h"
#include "audio/glassaudioentity.h"
#include "audio/caraudioentity.h"
#include "audio/heliaudioentity.h"
#include "audio/ambience/ambientaudioentity.h"
#include "audio/music/musicplayer.h"
#include "audio/northaudioengine.h"
#include "audio/trainaudioentity.h"
#include "audio/planeaudioentity.h"
#include "audio/policescanner.h"
#include "audio/radioaudioentity.h"
#include "audio/radiostation.h"
#include "audio/radioslot.h"
#include "audio/scannermanager.h"
#include "audio/scriptaudioentity.h"
#include "audio/speechaudioentity.h"
#include "audio/speechmanager.h"
#include "audio/weatheraudioentity.h"
#include "Network/Live/NetworkTelemetry.h"
#include "vfx/misc/TVPlaylistManager.h"
#include "objects/Door.h"
#include "peds/ped.h"
#include "frontend/MiniMap.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "script/thread.h"
#include "Network/NetworkInterface.h"
#include "network/events/NetworkEventTypes.h"
#include "control/gps.h"
#include "peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "frontend/ProfileSettings.h"
#include "text/TextConversion.h"
#include "Control/Replay/Audio/ScriptAudioPacket.h"

bool g_DisableEndCreditsFade = false;
int g_RomansMood = AUD_ROMANS_MOOD_NORMAL;
int g_BriansMood = AUD_BRIANS_MOOD_CLEAN;
extern audAmbientAudioEntity g_AmbientAudioEntity;
#if !__FINAL
extern bool g_EnablePlaceholderDialogue;
#endif
namespace audio_commands
{
	void CommandPlayPedRingtone(const char* soundName, s32 pedIndex, bool triggerAsHudSound)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(SCRIPT_VERIFY(ped, "Invalid ped"))
			{
				if(SCRIPT_VERIFY(ped->GetAudioEntity(), "Invalid ped audio entity"))
				{
					audSoundSet phoneSoundset;
					bool isMainCharacterPed = true;
					bool isInPrologue = CMiniMap::GetInPrologue();

					switch(ped->GetPedAudioEntity()->GetPedType())
					{
					case PEDTYPE_PLAYER_0: //MICHAEL
						phoneSoundset.Init(isInPrologue? ATSTRINGHASH("Phone_SoundSet_Prologue", 0x596B8EBB) : ATSTRINGHASH("Phone_SoundSet_Michael", 0x578FE4D7));
						break;
					case PEDTYPE_PLAYER_1: //FRANKLIN
						phoneSoundset.Init(isInPrologue? ATSTRINGHASH("Phone_SoundSet_Prologue", 0x596B8EBB) : ATSTRINGHASH("Phone_SoundSet_Franklin", 0x8A73028A));
						break;
					case PEDTYPE_PLAYER_2: //TREVOR
						phoneSoundset.Init(isInPrologue? ATSTRINGHASH("Phone_SoundSet_Prologue", 0x596B8EBB) : ATSTRINGHASH("Phone_SoundSet_Trevor", 0xE52306DE));
						break;
					default: // MULTIPLAYER (presumably)
						if(ped->IsLocalPlayer())
						{
							phoneSoundset.Init(isInPrologue? ATSTRINGHASH("Phone_SoundSet_Prologue", 0x596B8EBB) : ATSTRINGHASH("Phone_SoundSet_Default", 0x28C8633));
						}
						else
						{
							isMainCharacterPed = false;
						}
						break;
					}

					if(isMainCharacterPed)
					{
						if(SCRIPT_VERIFY(phoneSoundset.IsInitialised(), "Failed to initialise soundset"))
						{
							audMetadataRef ringtoneSoundRef = phoneSoundset.Find(soundName);

							if(SCRIPT_VERIFY(ringtoneSoundRef.IsValid(), "Failed to find sound in soundset"))
							{
#if GTA_REPLAY
								ped->GetPedAudioEntity()->ReplaySetRingTone(phoneSoundset.GetNameHash(), atStringHash(soundName));
#endif
								ped->GetPedAudioEntity()->SetRingtone(ringtoneSoundRef);
								ped->GetPedAudioEntity()->PlayRingTone(-1, triggerAsHudSound);
							}
						}
					}
					else
					{
						ped->GetPedAudioEntity()->PlayRingTone(-1, triggerAsHudSound);
					}
				}
			}
		}
	}

	bool CommandIsPedRingtonePlaying(s32 pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(SCRIPT_VERIFY(ped, "Invalid ped"))
			{
				if(SCRIPT_VERIFY(ped->GetPedAudioEntity(), "Invalid ped audio entity"))
				{
					return ped->GetPedAudioEntity()->IsRingTonePlaying();
				}
			}
		}

		return false;
	}

	void CommandStopPedRingtone(s32 pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(SCRIPT_VERIFY(ped, "Invalid ped"))
			{
				if(SCRIPT_VERIFY(ped->GetPedAudioEntity(), "Invalid ped audio entity"))
				{
					ped->GetPedAudioEntity()->StopRingTone();
				}
			}
		}
	}

	s32 CommandGetLaughterTrackMarkerTime(int index)
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(currentVideo->m_Name);

			if(textIds)
			{
				if(audVerifyf(index >= 0 && index < textIds->numTextIds, "Invalid laughter track index"))
				{
					return textIds->TextId[index].OffsetMs;
				}
			}
		}

		return -1;
	}

	s32 CommandGetLaughterTrackMarkerContext(int index)
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(currentVideo->m_Name);

			if(textIds)
			{
				if(audVerifyf(index >= 0 && index < textIds->numTextIds, "Invalid laughter track index"))
				{
					return textIds->TextId[index].TextId;
				}
			}
		}

		return 0;
	}

	s32 CommandGetNextLaughterTrackMarkerIndex()
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(currentVideo->m_Name);

			if(textIds)
			{
				const f32 currentPlayTime = CTVPlaylistManager::GetCurrentTVShowTime();
				const f32 playTimeSeconds = CTVPlaylistManager::ScaleToRealTime(currentPlayTime);
				const u32 playTimeMs = (u32)(playTimeSeconds * 1000.0f);
				const s32 markerIndex = audSpeechAudioEntity::GetNextLaughterTrackMarkerIndex(textIds, playTimeMs);
				return markerIndex;
			}
		}

		return -1;
	}

	s32 CommandGetTimeUntilLaughterTrackMarker(int index)
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(currentVideo->m_Name);

			if(textIds)
			{
				if(audVerifyf(index >= 0 && index < textIds->numTextIds, "Invalid laughter track index"))
				{
					const f32 currentPlayTime = CTVPlaylistManager::GetCurrentTVShowTime();
					const f32 playTimeSeconds = CTVPlaylistManager::ScaleToRealTime(currentPlayTime);
					const u32 playTimeMs = (u32)(playTimeSeconds * 1000.0f);
					return textIds->TextId[index].OffsetMs - playTimeMs;
				}
			}
		}

		return -1;
	}

	u32 CommandGetCurrentTVShowPlayTime()
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			const f32 currentPlayTime = CTVPlaylistManager::GetCurrentTVShowTime();
			const f32 playTimeSeconds = CTVPlaylistManager::ScaleToRealTime(currentPlayTime);
			const u32 playTimeMs = (u32)(playTimeSeconds * 1000.0f);
			return playTimeMs;
		}

		return 0;
	}

	u32 CommandGetCurrentTVShowDuration()
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			const f32 currentDuration = currentVideo->GetDuration();
			const f32 durationSeconds = CTVPlaylistManager::ScaleToRealTime(currentDuration);
			const u32 durationMs = (u32)(durationSeconds * 1000.0f);
			return durationMs;
		}

		return 0;
	}


//	static tScriptedConversationEntryPointers ScriptedConversationData[AUD_MAX_SCRIPTED_CONVERSATION_LINES];
	static int NumberOfLinesInScriptedConversation;
//	static char EmptyString[2] = "";

	void CommandNewScriptedConversation()
	{
		NumberOfLinesInScriptedConversation = 0;

		g_ScriptAudioEntity.StartNewScriptedConversationBuffer();
/*
		for (int loop = 0; loop < AUD_MAX_SCRIPTED_CONVERSATION_LINES; loop++)
		{
			ScriptedConversationData[loop].Speaker=-1;
			ScriptedConversationData[loop].Context = EmptyString;
			ScriptedConversationData[loop].Subtitle = EmptyString;
		}
*/
	}


	void CommandAddLineToConversation(int SpeakerNumber, const char *pContext, const char *pSubtitle, int listenerNumber, int volume, bool isRandom, 
		bool interruptible = true, bool ducksRadio = true, bool ducksScore = false, int audibility = 0, bool headset = false, 
		bool dontInterruptForSpecialAbility = false, bool isPadSpeakerRoute = false)
	{
		if(SCRIPT_VERIFY(NumberOfLinesInScriptedConversation < AUD_MAX_SCRIPTED_CONVERSATION_LINES, "ADD_LINE_TO_SCRIPTED_CONVERSATION - too many lines in this conversation"))
		{
			if(SCRIPT_VERIFY(SpeakerNumber < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS, "ADD_LINE_TO_SCRIPTED_CONVERSATION - SpeakerNumber must be 0 or 1"))
			{
				if(audibility >= (int) AUD_NUM_AUDIBILITIES)
					audibility = 0;

				g_ScriptAudioEntity.AddLineToScriptedConversation(NumberOfLinesInScriptedConversation, SpeakerNumber, pContext, pSubtitle, listenerNumber, (u8)volume, isRandom,
					interruptible, ducksRadio, ducksScore, (audConversationAudibility)audibility, headset, dontInterruptForSpecialAbility, isPadSpeakerRoute);
				NumberOfLinesInScriptedConversation++;
			}
		}
	}

	void CommandAddNewConversationSpeaker(int speakerConversationIndex, int speakerPedIndex, const char* voiceName)
	{
		CPed *speaker = NULL;
		if(SCRIPT_VERIFY(speakerConversationIndex<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS, "Too many speakers in a conversation - must be < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS (36)")
			&& SCRIPT_VERIFY(speakerConversationIndex!=9, "Speaker ID 9 is reserved"))
		{
			if (NULL_IN_SCRIPTING_LANGUAGE != speakerPedIndex)
			{
				speaker = CTheScripts::GetEntityToModifyFromGUID<CPed>(speakerPedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
				if(!speaker)
					return;
			}
			g_ScriptAudioEntity.AddConversationSpeaker(speakerConversationIndex, speaker, (char *) voiceName);
		}
	}

	void CommandSetPositionForNullConvPed(int speakerConversationIndex, const scrVector &nullPedPos)
	{
		// Vector3 newPosition = Vector3(nullPedPos);
		g_ScriptAudioEntity.SetPositionForNullConvPed(speakerConversationIndex, nullPedPos);
	}

	void CommandSetMicrophonePosition(bool override,const scrVector &panningPos,const scrVector &vol1Pos,const scrVector &vol2Pos)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->SetAudioFlag(audScriptAudioFlags::ScriptForceMicPosition, override);
			audNorthAudioEngine::GetMicrophones().SetScriptMicPosition(panningPos,vol1Pos,vol2Pos);
		}
	}

	void CommandOverrideGunfightConductorIntensity(bool override,const int intensity)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->SetAudioFlag(audScriptAudioFlags::ScriptForceGunfightConductorIntensity, override);
			SUPERCONDUCTOR.SendOrderToConductor(GunfightConductor,(ConductorsOrders)intensity);
		}
	}

	void CommandSetEntityForNullConvPed(int speakerConversationIndex, int entityIndex)
	{
		CEntity* entity = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != entityIndex)
		{
			entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex);
			if(SCRIPT_VERIFY(entity, "Invalid entity passed in when setting NULL conversation speaker."))
			{
				g_ScriptAudioEntity.SetEntityForNullConvPed(speakerConversationIndex, entity);
			}
		}
	}

	void CommandSetConversationControlledByAnimTriggers(bool enable)
	{
		g_ScriptAudioEntity.SetConversationControlledByAnimTriggers(enable);
	}

	void CommandSetConversationPlaceholder(bool isPlaceholder)
	{
		g_ScriptAudioEntity.SetConversationPlaceholder(isPlaceholder);
	}

	void CommandStartScriptConversation(bool displaySubtitles, bool addToBriefScreen, bool cloneConversation, bool interruptible)
	{
		g_ScriptAudioEntity.StartConversation(false, displaySubtitles, addToBriefScreen, cloneConversation, interruptible);
	}

	void CommandPreloadScriptConversation(bool displaySubtitles, bool addToBriefScreen, bool cloneConversation, bool interruptible)
	{
		g_ScriptAudioEntity.StartConversation(false, displaySubtitles, addToBriefScreen, cloneConversation, interruptible, true);
	}

	void CommandStartPreloadedConversation()
	{
		g_ScriptAudioEntity.RestartScriptedConversation(false, true);
	}

	bool CommandIsPreloadedConversationReady()
	{
		return g_ScriptAudioEntity.IsPreloadedScriptedSpeechReady();
	}

	void CommandStartScriptedConversation(int /*FirstSpeakerIndex*/, const char* /*pFirstVoiceName*/, int /*SecondSpeakerIndex*/, const char* /*pSecondVoiceName*/, bool /*displaySubtitles*/, bool /*addToBriefing*/)
	{
		Assertf(0, "%s:START_SCRIPTED_CONVERSATION - this command has been removed. Try START_SCRIPT_CONVERSATION instead", CTheScripts::GetCurrentScriptNameAndProgramCounter());
/*
		CommandAddNewConversationSpeaker(0, FirstSpeakerIndex, pFirstVoiceName);
		CommandAddNewConversationSpeaker(1, SecondSpeakerIndex, pSecondVoiceName);

		g_ScriptAudioEntity.StartConversation(false, displaySubtitles, addToBriefing);
*/
	}

	bool CommandIsScriptedConversationOngoing()
	{
		return (g_ScriptAudioEntity.IsScriptedConversationOngoing() && !g_ScriptAudioEntity.IsScriptedConversationAMobileCall());
	}

	bool CommandIsUnpausedScriptedConversationOngoing()
	{
		return (g_ScriptAudioEntity.IsScriptedConversationOngoing() && !g_ScriptAudioEntity.IsScriptedConversationAMobileCall() && 
			!g_ScriptAudioEntity.IsScriptedConversationPaused());
	}

	bool CommandIsScriptedConversationLoaded()
	{
		return g_ScriptAudioEntity.IsScriptedConversationLoaded();
	}

	s32 CommandGetCurrentScriptedConversationLine()
	{
		return g_ScriptAudioEntity.GetCurrentScriptedConversationLine();
	}

	s32 CommandGetCurrentUnresolvedScriptedConversationLine()
	{
		return g_ScriptAudioEntity.GetCurrentUnresolvedScriptedConversationLine();
	}

	void CommandPauseScriptedConversation(bool finishCurrentLine)
	{
		g_ScriptAudioEntity.PauseScriptedConversation(finishCurrentLine, true, true);
	}

	void CommandRestartScriptedConversation()
	{
		g_ScriptAudioEntity.RestartScriptedConversation(false, true);
	}

	s32 CommandAbortScriptedConversation(bool finishCurrentLine)
	{
//		Assertf(0, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		naConvDisplayf("STOP_SCRIPTED_CONVERSATION called from %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return g_ScriptAudioEntity.AbortScriptedConversation(finishCurrentLine BANK_ONLY(,"From script"));
	}

	void CommandSkipToNextScriptedConversationLine()
	{
		g_ScriptAudioEntity.SkipToNextScriptedConversationLine();
	}

	void CommandInterruptConversation(s32 pedIndex, const char *context, const char* voiceName)
	{
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			if (voiceName == NULL)
			{
				voiceName = "";
			}
			CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			g_ScriptAudioEntity.RegisterScriptRequestedInterrupt(ped, context, voiceName,false);
		}
	}
	void CommandInterruptConversationAndPause(s32 pedIndex, const char *context, const char* voiceName)
	{
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			if (voiceName == NULL)
			{
				voiceName = "";
			}
			CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			g_ScriptAudioEntity.RegisterScriptRequestedInterrupt(ped, context, voiceName,true);
		}
	}


	s32 CommandGetVariationChosenForScriptedLine(const char* pTextLabel)
	{
		if (TheText.DoesTextLabelExist(pTextLabel))
		{
			return g_ScriptAudioEntity.GetVariationChosenForScriptedLine(atStringHash(TheText.Get(pTextLabel)));
		}
		else
		{
			return -1;
		}
	}

	void CommandSetConversationCodeName(int codeletter, int codenumber, const char* userName)
	{
		if (SCRIPT_VERIFY(codeletter < 26, "SET_CONVERSATION_CODE_NAME - codeletter is out of range"))
		{
			if (SCRIPT_VERIFY(codenumber < 26, "SET_CONVERSATION_CODE_NAME - codenumber is out of range"))
			{
				g_ScriptAudioEntity.SetConversationCodename((char)(65+codeletter), (u8)codenumber, userName);
			}
		}
	}

	void CommandSetConversationLocation(const scrVector &srcPosition)
	{
		Vector3 newPosition = Vector3(srcPosition);
		g_ScriptAudioEntity.SetConversationLocation(newPosition);
	}

	void CommandSetNoDuckingForConversation(bool enable)
	{
		g_ScriptAudioEntity.SetNoDuckingForConversationFromScript(enable);
	}

//	Copied from Brenda's PLAY_AMBIENT_DIALOGUE_LINE function in Ambient_Speech.sch
	const char *CommandGetTextKeyForLineOfAmbientSpeech(s32 PedIndex, const char *pSubtitleGroupID, const char *pLineRoot)
	{
		const u32 MaxLengthOfString = 64;
		char const * MissionPrefix = NULL;
		char const * ActualPrefix = NULL;
		char PrefixLabel[MaxLengthOfString];
		char TextLabel[MaxLengthOfString];
		static char LineToPlay[MaxLengthOfString];

		safecpy(LineToPlay, "", MaxLengthOfString);

		const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
		if (SCRIPT_VERIFY(pPed, "GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH - ped doesn't exist"))
		{
			CBaseModelInfo* pPedModel = pPed->GetBaseModelInfo();
			if (SCRIPT_VERIFY(pPedModel, "GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH - couldn't find model for the ped's model index"))
			{
				s32 ModelHashKey = (s32) pPedModel->GetHashKey();

				formatf(TextLabel, MaxLengthOfString, "PRE_%s", pSubtitleGroupID);

				MissionPrefix = TheText.Get(TextLabel);
				scriptDebugf3("GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH - MissionPrefix = %s\n", MissionPrefix);

				formatf(PrefixLabel, MaxLengthOfString, "%s_%d", MissionPrefix, ModelHashKey);
				scriptDebugf3("GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH - PrefixLabel = %s\n", PrefixLabel);

				//Get DialogueStar Chosen Prefix For this Ped
				ActualPrefix = TheText.Get(PrefixLabel);
				scriptDebugf3("GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH - ActualPrefix = %s\n", ActualPrefix);

				formatf(LineToPlay, MaxLengthOfString, "%s%s", pLineRoot, ActualPrefix);
				scriptDebugf3("GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH - LineToPlay = %s\n", LineToPlay);
			}
		}

		return LineToPlay;
	}



	const char *CommandGetSpeechForEmergencyServiceCall()
	{
		static char RetString[16];
		g_ScriptAudioEntity.GetSpeechForEmergencyServiceCall(RetString);
		return (const char *)&RetString;
	}

	//////////// Deprecated Mobile commands
	void CommandNewMobilePhoneCall()
	{
		CommandNewScriptedConversation();
	}

	void CommandAddLineToMobilePhoneCall(int SpeakerNumber, const char *pContext, const char *pSubtitle)
	{
		// only ever have two speakers, so make them listen to each other
		int volume = AUD_SPEECH_NORMAL;	//	This was changed to AUD_SPEECH_FRONTEND in E1
		int listenerNumber = 9;
		if (SpeakerNumber == 0)
		{
			listenerNumber = 2; // ped 2 is the real ped, if we have one. Ped 1 is the disembodied voice.
		}
		else if (SpeakerNumber == 1) // and we have nico only respond to the phone voice, not the real one.
		{
			listenerNumber = 0;
			volume = AUD_SPEECH_FRONTEND;
		}
		else if (SpeakerNumber == 2)
		{
			volume = AUD_SPEECH_NORMAL;
		}
		CommandAddLineToConversation(SpeakerNumber, pContext, pSubtitle, listenerNumber, volume, false);
	}

	void CommandStartMobilePhoneCall(int FirstSpeakerIndex, const char *pFirstVoiceName, int SecondSpeakerIndex, const char *pSecondVoiceName, bool displaySubtitles, bool addToBriefing)
	{
		CommandAddNewConversationSpeaker(0, FirstSpeakerIndex, pFirstVoiceName);
		CommandAddNewConversationSpeaker(1, NULL_IN_SCRIPTING_LANGUAGE, pSecondVoiceName);
		// If we have a ped passed in, set that up as the third speaker, as it's the real ped on the other end of the phonecall.
		// The phone lines themselves always play frontend, hence never have a ped index.
		if (SecondSpeakerIndex!=NULL_IN_SCRIPTING_LANGUAGE)
		{
			CommandAddNewConversationSpeaker(2, SecondSpeakerIndex, pSecondVoiceName);
		}
		
		g_ScriptAudioEntity.StartConversation(true, displaySubtitles, addToBriefing);
	}

	void CommandPreloadMobilePhoneCall(int FirstSpeakerIndex, const char *pFirstVoiceName, int SecondSpeakerIndex, const char *pSecondVoiceName, bool displaySubtitles, bool addToBriefing)
	{
		CommandAddNewConversationSpeaker(0, FirstSpeakerIndex, pFirstVoiceName);
		CommandAddNewConversationSpeaker(1, NULL_IN_SCRIPTING_LANGUAGE, pSecondVoiceName);
		// If we have a ped passed in, set that up as the third speaker, as it's the real ped on the other end of the phonecall.
		// The phone lines themselves always play frontend, hence never have a ped index.
		if (SecondSpeakerIndex!=NULL_IN_SCRIPTING_LANGUAGE)
		{
			CommandAddNewConversationSpeaker(2, SecondSpeakerIndex, pSecondVoiceName);
		}

		g_ScriptAudioEntity.StartConversation(true, displaySubtitles, addToBriefing, false, true, true);
	}

	void CommandStartScriptPhoneConversation(bool displaySubtitles, bool addToBriefScreen)
	{
		g_ScriptAudioEntity.StartConversation(true, displaySubtitles, addToBriefScreen);
	}

	void CommandPreloadScriptPhoneConversation(bool displaySubtitles, bool addToBriefScreen)
	{
		g_ScriptAudioEntity.StartConversation(true, displaySubtitles, addToBriefScreen, false, true, true);
	}

	bool CommandIsMobilePhoneCallOngoing()
	{
		return (g_ScriptAudioEntity.IsScriptedConversationOngoing() && g_ScriptAudioEntity.IsScriptedConversationAMobileCall());
	}

	bool CommandIsMobilePhoneInterferenceActive()
	{
		return g_ScriptAudioEntity.IsMobileInterferenceSoundValid();
	}
	//////////// End of Deprecated Mobile commands

	void CommandPlayAudioEvent(const char *eventName)
	{
		g_ScriptAudioEntity.HandleScriptAudioEvent(atStringHash(eventName), NULL);
	}

	void CommandPlayAudioEventFromEntity(const char *eventName, int entityIndex)
	{
		CEntity* entity = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != entityIndex)
		{
			entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex);
			if(!entity)
				return;
		}
		g_ScriptAudioEntity.HandleScriptAudioEvent(atStringHash(eventName), entity);
	}

	// Generic mission and ambient bank loading and sound playback commands.
	// Scripts register themselves with the script audio entity, request a bank, and then either fire-and-forget sounds, or 
	// request persistent soundIds, and play, stop, check, and set variables on that sound.

	void CommandRegisterScriptWithAudio(bool /*inChargeOfAudio*/ = false)
	{
		//(void)inChargeOfAudio;
		//g_ScriptAudioEntity.AddScript(inChargeOfAudio);
	}

	void CommandUnregisterScriptWithAudio()
	{
		//g_ScriptAudioEntity.RemoveScript(CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}

	void CommandSetScriptCleanupTime(int delay)
	{
		g_ScriptAudioEntity.SetScriptCleanupTime((u32)delay); 
	}

	bool CommandRequestWeaponAudio(const char * weaponName)
	{
		return g_ScriptAudioEntity.RequestScriptWeaponAudio(weaponName);
	}

	void CommandReleaseWeaponAudio()
	{
		g_ScriptAudioEntity.ReleaseScriptWeaponAudio();
	}

	void CommandSetSkipMinigunSpinUp(bool skipSpinUp)
	{
		g_WeaponAudioEntity.SetSkipMinigunSpinUp(skipSpinUp);
	}

	bool CommandHasLoadedMPDataSet()
	{
		return audNorthAudioEngine::HasLoadedMPDataSet();
	}

	bool CommandHasLoadedSPDataSet()
	{
		return audNorthAudioEngine::HasLoadedSPDataSet();
	}

	//Mission and ambient banks are legacy and now both point at RequestScriptBank which
	//handles all script bank loading now
	bool CommandRequestMissionAudioBank(const char* bankName, bool bOverNetwork, int nPlayerBits)
	{
		return g_ScriptAudioEntity.RequestScriptBank(bankName, bOverNetwork, static_cast<unsigned>(nPlayerBits));//"SCRIPT_BANKS/QUB3D");//
	}

	bool CommandRequestAmbientAudioBank(const char* bankName, bool bOverNetwork, int nPlayerBits)
	{
		return g_ScriptAudioEntity.RequestScriptBank(bankName, bOverNetwork, static_cast<unsigned>(nPlayerBits));
	}

	bool CommandRequestScriptAudioBank(const char* bankName, bool bOverNetwork, int nPlayerBits)
	{
		networkAudioDebugf3("CommandRequestScriptAudioBank bankName[%s] bOverNetwork[%d]",bankName,bOverNetwork);
		return g_ScriptAudioEntity.RequestScriptBank(bankName, bOverNetwork, static_cast<unsigned>(nPlayerBits));
	}

	void CommandHintMissionAudioBank(const char* bankName, bool bOverNetwork, int nPlayerBits)
	{
		g_ScriptAudioEntity.HintScriptBank(bankName, bOverNetwork, static_cast<unsigned>(nPlayerBits));//"SCRIPT_BANKS/QUB3D");//
	}

	void CommandHintAmbientAudioBank(const char* bankName, bool bOverNetwork, int nPlayerBits)
	{
		g_ScriptAudioEntity.HintScriptBank(bankName, bOverNetwork, static_cast<unsigned>(nPlayerBits));
	}

	void CommandHintScriptAudioBank(const char* bankName, bool bOverNetwork, int nPlayerBits)
	{
		g_ScriptAudioEntity.HintScriptBank(bankName, bOverNetwork, static_cast<unsigned>(nPlayerBits));
	}

	void CommandMissionAudioBankNoLongerNeeded()
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded();
	}

	void CommandAmbientAudioBankNoLongerNeeded()
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded();
	}

	void CommandScriptAudioBankNoLongerNeeded()
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded();
	}
	void CommandNamedScriptAudioBankNoLongerNeeded(const char * bankName)
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded(bankName);
	}

	void CommandUnHintMissionAudioBank()
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded();
	}

	void CommandUnHintAmbientAudioBank()
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded();
	}

	void CommandUnHintScriptAudioBank()
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded();
	}

	void CommandUnHintNamedScript(const char * bankName)
	{
		g_ScriptAudioEntity.ScriptBankNoLongerNeeded(bankName);
	}
	s32	CommandGetSoundId()
	{
		return g_ScriptAudioEntity.GetSoundId();
	}

	void CommandRequestTennisBanks(s32 pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if (ped)
			{
				g_SpeechManager.RequestTennisBanks(ped);
			}
		}
	}

	void CommandUnrequestTennisBanks()
	{
		g_SpeechManager.UnrequestTennisBanks();
	}

	void CommandReleaseSoundId(s32 soundId)
	{
		g_ScriptAudioEntity.ReleaseSoundId(soundId);
	}

	void CommandPlaySoundFromEntity(s32 soundId, const char* soundName, int entityIndex, const char * setName, bool bOverNetwork, int nNetworkRange)
	{
		networkAudioDisplayf("CommandPlaySoundFromEntity soundId[%d] soundName[%s] entityIndex[%d] setName[%s] bOverNetwork[%d] nNetworkRange[%d]",soundId,soundName,entityIndex,setName,bOverNetwork,nNetworkRange);

		const CEntity* entity = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != entityIndex)
		{
			entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
			if(entity)
			{
				g_ScriptAudioEntity.PlaySoundFromEntity(soundId, soundName, entity, setName, bOverNetwork, nNetworkRange);
			}
		}
	}


	void CommandPlaySoundFromEntityHash(s32 soundId, const int soundNameHash, int entityIndex, const int setNameHash, bool bOverNetwork, int nNetworkRange)
	{
		networkAudioDisplayf("CommandPlaySoundFromEntityHash soundId[%d] soundName[%d] entityIndex[%d] setName[%d] bOverNetwork[%d] nNetworkRange[%d]",soundId,soundNameHash,entityIndex,setNameHash,bOverNetwork,nNetworkRange);

		const CEntity* entity = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != entityIndex)
		{
			entity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
			if(entity)
			{
				g_ScriptAudioEntity.PlaySoundFromEntity(soundId, soundNameHash, entity, setNameHash, bOverNetwork, nNetworkRange OUTPUT_ONLY(, NULL, NULL));
			}
		}
	}

	void CommandRefreshClosestOceanShoreline()
	{
		g_AmbientAudioEntity.ForceOceanShorelineUpdateFromNearestRoad();
	}	

	void CommandPlaySoundFromPosition(s32 soundId, const char* soundName, const scrVector &srcPosition, const char * setName, bool bOverNetwork, int nNetworkRange, bool isExteriorLoc)
	{
		networkAudioDisplayf("CommandPlaySoundFromPosition soundId[%d] soundName[%s] srcPosition[%f %f %f] setName[%s] bOverNetwork[%d] nNetworkRange[%d] isExteriorLoc[%d]",soundId,soundName,srcPosition.x,srcPosition.y,srcPosition.z,setName,bOverNetwork,nNetworkRange,isExteriorLoc);

		Vector3 newPosition = Vector3(srcPosition);
		g_ScriptAudioEntity.PlaySoundFromPosition(soundId, soundName, &newPosition, setName, bOverNetwork, nNetworkRange, isExteriorLoc);
	}

	void CommandUpdateSoundPosition(s32 soundId, const scrVector &scrPosition)
	{
		Vector3 newPosition = Vector3(scrPosition);
		g_ScriptAudioEntity.UpdateSoundPosition(soundId, &newPosition);
	}

	void CommandPlayFireSoundFromPosition(s32 soundId, const scrVector &srcPosition)
	{
		Vector3 newPosition = Vector3(srcPosition);
		g_ScriptAudioEntity.PlayFireSoundFromPosition(soundId, &newPosition);
	}

	void CommandPlaySound(s32 soundId, const char* soundName, const char * setName, bool bOverNetwork, int nNetworkRange, bool REPLAY_ONLY(enableOnReplay))
	{
		networkAudioDisplayf("CommandPlaySound soundId[%d] soundName[%s] setName[%s] bOverNetwork[%d] nNetworkRange[%d]",soundId,soundName,setName,bOverNetwork,nNetworkRange);

		bool param = false;
		REPLAY_ONLY(param = enableOnReplay;)
		g_ScriptAudioEntity.PlaySound(soundId, soundName, setName, bOverNetwork, nNetworkRange, param);
	}

	void CommandPlaySoundFrontend(s32 soundId, const char* soundName, const char * setName, bool REPLAY_ONLY(enableOnReplay))
	{
		bool param = false;
		REPLAY_ONLY(param = enableOnReplay;)
		g_ScriptAudioEntity.PlaySoundFrontend(soundId, soundName, setName , param);
	}
	void CommandPlayDeferredSoundFrontend(const char* soundName, const char * setName)
	{
		g_FrontendAudioEntity.PlaySound(soundName, setName);
	}
	void CommandStopSound(s32 soundId)
	{
		if (soundId >= 0)
		{
			networkAudioDisplayf("CommandStopSound soundId[%d]",soundId);
		}

		g_ScriptAudioEntity.StopSound(soundId);
	}

	s32 CommandGetNetworkIdFromSoundId(s32 soundId)
	{
		return static_cast<s32>(g_ScriptAudioEntity.GetNetworkIdFromSoundId(soundId));
	}

	s32 CommandGetSoundIdFromNetworkId(s32 networkId)
	{
		return g_ScriptAudioEntity.GetSoundIdFromNetworkId(static_cast<u32>(networkId));
	}

	void CommandSetVariableOnSound(s32 soundId, const char* variableName, f32 variableValue)
	{
		networkAudioDebugf3("CommandSetVariableOnSound soundId[%d] variableName[%s] variableValue[%f]",soundId,variableName,variableValue);

		g_ScriptAudioEntity.SetVariableOnSound(soundId, variableName, variableValue);
	}

	void CommandSetVariableOnStream(const char* variableName, f32 variableValue)
	{
		g_ScriptAudioEntity.SetVariableOnStream(variableName, variableValue);
	}

	void CommandOverrideUnderWaterStream(const char* streamName,bool override)
	{
		g_AmbientAudioEntity.OverrideUnderWaterStream(streamName, override);
	}

	void CommandSetVariableOnUnderWaterStream(const char* variableName, f32 variableValue)
	{
		g_AmbientAudioEntity.SetVariableOnUnderWaterStream(variableName, variableValue);
	}

	bool CommandHasSoundFinished(s32 soundId)
	{
		return g_ScriptAudioEntity.HasSoundFinished(soundId);
	}

	void CommandSayAmbientSpeech(int pedIndex, const char* context, bool forcePlay, bool allowRepeat, int requestedVolume, bool preloadOnly, int preloadTimeoutMS)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if (ped)
			{
				bool played = ped->NewSay(const_cast<char*>(context), 0, forcePlay, allowRepeat, -1, 0, 0, -1, 1.0f, (u32)requestedVolume, preloadOnly, (u32)preloadTimeoutMS, true);

				if (played && ped->IsLocalPlayer() && NetworkInterface::IsGameInProgress())
				{
					// See what variation number we just played, and send that across the network too.
					u32 tauntVariationNumber = ped->GetSpeechAudioEntity()->GetCurrentlyPlayingAmbientSpeechVariationNumber();
					CPlayerTauntEvent::Trigger(context, tauntVariationNumber);
				}
			}
		}
	}

	void CommandSayAmbientSpeechWithVoice(int pedIndex, const char* context, const char* voiceName, bool forcePlay, bool allowRepeat, int requestedVolume, bool preloadOnly, int preloadTimeoutMS)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped)
			{
				ped->NewSay(const_cast<char*>(context), atStringHash(voiceName), forcePlay, allowRepeat, -1, 0, 0, -1, 1.0f, (u32)requestedVolume, preloadOnly, (u32)preloadTimeoutMS, true);
			}		
		}
	}

	void CommandSayAmbientSpeechAndCloneNative(int pedIndex, const char* context, const char* speechParams, bool syncOverNetwork)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if(ped)
			{
                if(NetworkUtils::IsNetworkCloneOrMigrating(ped) && syncOverNetwork)
                {
                    scriptAssertf(false, "%s:PLAY_PED_AMBIENT_SPEECH_AND_CLONE_NATIVE - Can't call this function on a remotely owned ped with the \"sync over network\" option set!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
                }
                else if(ped->GetSpeechAudioEntity())
                {
				    s32 variationNumOverride = 0;

				    ped->GetSpeechAudioEntity()->Say(const_cast<char*>(context), speechParams, 0, -1, NULL, 0, -1, 1.0f, true, &variationNumOverride);

				    if(NetworkInterface::IsGameInProgress() && syncOverNetwork && !ped->GetSpeechAudioEntity()->IsOutgoingNetworkSpeechBlocked())
				    {
					    CPedConversationLineEvent::Trigger(ped, ped->GetSpeechAudioEntity()->GetCurrentlyPlayingVoiceNameHash(), context, variationNumOverride, speechParams);
				    }
                }
			}
		}
	}

	void CommandSayAmbientSpeechNative(int pedIndex, const char* context, const char* speechParams, bool syncOverNetwork)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if (ped)
			{
                if(NetworkUtils::IsNetworkCloneOrMigrating(ped) && syncOverNetwork)
                {
                    scriptAssertf(false, "%s:PLAY_PED_AMBIENT_SPEECH_NATIVE - Can't call this function on a remotely owned ped with the \"sync over network\" option set!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
                }
                else if(ped->GetSpeechAudioEntity())
                {
				    bool played = ped->GetSpeechAudioEntity()->Say(const_cast<char*>(context), speechParams, 0, -1, NULL, 0, -1, 1.0f, true) != AUD_SPEECH_SAY_FAILED;

					if (NetworkInterface::IsGameInProgress())
					{
						u32 variationNumber = 0u;
						u32 voiceNameHash = 0u;

						if (played)
						{
							voiceNameHash = ped->GetSpeechAudioEntity()->GetCurrentlyPlayingVoiceNameHash();
							variationNumber = ped->GetSpeechAudioEntity()->GetCurrentlyPlayingAmbientSpeechVariationNumber();
						}
						else if (!ped->IsLocalPlayer() && syncOverNetwork)
						{
							// HL - Script can configure peds to block speech locally but still trigger on remote machine using a flag on the the BLOCK_ALL_SPEECH_FROM_PED command.
							// This was required for the casino table games, where we have a limited number of speech slots and so each client needs to restrict speech to only that coming
							// from the game that the local player is sat at. However, all the speech triggering logic for the whole casino is handled by a single machine, so we end up 
							// with the unusual situation we need to block speech on the machine that is doing the initial speech triggering logic, but still send out the speech packet 
							// (as though the speech had been successfully triggered) so that client machines can make their own decision as to whether to play it or not.

							// This script command in particular is awkward as the network packet is only sent *if* the speech played successfully (the other script commands, which take 
							// an explicit context/voice, just assume it worked)
							if (ped->GetSpeechAudioEntity()->IsAllSpeechBlocked() && !ped->GetSpeechAudioEntity()->IsOutgoingNetworkSpeechBlocked())
							{
								// If the speech was blocked locally but we still want to send it over the network, treat the speech as having played. 
								// Nb. *Technically* it may not have been the blocked flag that prevented the speech from playing (eg. the ped may have 
								// been dead, or some other valid failure case) but this is very much a special case, and modifying the speech code to 
								// add an additional audSaySuccessValue to detect blockages vs. failures is much riskier than just assuming that script 
								// know what they're doing
								played = true;

								u32 timeContextLastPlayedForThisVoice = 0u;
								bool hasBeenPlayedRecently = false;

								// Need to rustle up a voice and variation to keep the speech in sync for all clients that choose to play it, and add this to the variation history as though it had been triggered locally
								voiceNameHash = ped->GetSpeechAudioEntity()->GetAmbientVoiceName();
								variationNumber = ped->GetSpeechAudioEntity()->GetRandomVariation(voiceNameHash, atPartialStringHash(context), &timeContextLastPlayedForThisVoice, &hasBeenPlayedRecently);
								g_SpeechManager.AddVariationToHistory(voiceNameHash, atPartialStringHash(context), variationNumber, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
							}
						}

						if (played)
						{
							if (ped->IsLocalPlayer())
							{
								// See what variation number we just played, and send that across the network too.
								CPlayerTauntEvent::Trigger(context, variationNumber);
							}
							else if (ped->GetNetworkObject() && !ped->IsNetworkClone() && syncOverNetwork && !ped->GetSpeechAudioEntity()->IsOutgoingNetworkSpeechBlocked())
							{
								CPedConversationLineEvent::Trigger(ped, voiceNameHash, context, variationNumber, speechParams);
							}
						}
					}
                }
			}
		}
	}

	void CommandSayAmbientSpeechWithVoiceNative(int pedIndex, const char* context, const char* voiceName, const char* speechParams, bool syncOverNetwork)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if(ped)
			{
                if(NetworkUtils::IsNetworkCloneOrMigrating(ped) && syncOverNetwork)
                {
                    scriptAssertf(false, "%s:PLAY_PED_AMBIENT_SPEECH_WITH_VOICE_NATIVE - Can't call this function on a remotely owned ped with the \"sync over network\" option set!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
                }
                else if(ped->GetSpeechAudioEntity())
                {
				    s32 variationNumOverride = 0;

#if !__FINAL
				    audSaySuccessValue success = AUD_SPEECH_SAY_FAILED;
				    // check we have the line at all - if not, try again with _PLACEHOLDER voice, in case this is scripted dialogue
				    if (g_EnablePlaceholderDialogue && voiceName &&
					    ((s32)audSpeechSound::FindNumVariationsForVoiceAndContext(atStringHash(voiceName), const_cast<char*>(context)) < 1))
				    {
					    char placeholderVoiceName[128] = "";
					    formatf(placeholderVoiceName, sizeof(placeholderVoiceName), "%s_PLACEHOLDER", voiceName);
					    success = ped->GetSpeechAudioEntity()->Say(const_cast<char*>(context), speechParams, atStringHash(placeholderVoiceName), -1,
														    NULL, 0, -1, 1.0f, true, &variationNumOverride);
				    }
				    if(success == AUD_SPEECH_SAY_FAILED)
				    {

					    ped->GetSpeechAudioEntity()->Say(const_cast<char*>(context), speechParams, atStringHash(voiceName), -1,
														    NULL, 0, -1, 1.0f, true, &variationNumOverride);
				    }
#else
				    ped->GetSpeechAudioEntity()->Say(const_cast<char*>(context), speechParams, atStringHash(voiceName), -1,
													    NULL, 0, -1, 1.0f, true, &variationNumOverride);
#endif // !__FINAL
                    if(NetworkInterface::IsGameInProgress() && syncOverNetwork && !ped->GetSpeechAudioEntity()->IsOutgoingNetworkSpeechBlocked())
                    {
                        CPedConversationLineEvent::Trigger(ped, atStringHash(voiceName), context, variationNumOverride, speechParams);
                    }
                }
			}		
		}
	}

	void CommandSayAmbientSpeechFromPositionNative(const char* context, const char* voiceName, const scrVector &reqPosition, const char* speechParams)
	{
		const Vector3 pos = Vector3(reqPosition);
		g_ScriptAudioEntity.SayAmbientSpeechFromPosition(context, atStringHash(voiceName), pos, speechParams);
	}

	void CommandMakeTrevorRage()
	{
		audSpeechAudioEntity::MakeTrevorRage();
	}

	void CommandOverrideTrevorRage(const char* context)
	{
		audSpeechAudioEntity::OverrideTrevorRageContext(context, true);
	}

	void CommandResetTrevorRage()
	{
		audSpeechAudioEntity::ResetTrevorRageContext(true);
	}

	void CommandSetPlayerAngry(int pedIndex, bool isAngry)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				g_ScriptAudioEntity.SetPlayerAngry(ped->GetSpeechAudioEntity(), isAngry);
			}
		}
	}

	void CommandSetPlayerAngryShortTime(int pedIndex, int timeInMs)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				g_ScriptAudioEntity.SetPlayerAngryShortTime(ped->GetSpeechAudioEntity(), (u32)timeInMs);
			}
		}
	}
	
	void CommandPlayPain(int pedIndex, int damageReason, float rawDamage, bool syncOverNetwork)
	{
		if(!naVerifyf(damageReason >= 0 && damageReason <  AUD_NUM_DAMAGE_REASONS, "Invalid damage reason passed to CommandPlayPain."))
			return;

		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

            if(ped)
            {
                if(NetworkUtils::IsNetworkCloneOrMigrating(ped) && syncOverNetwork)
                {
                    scriptAssertf(false, "%s:PLAY_PAIN - Can't call this function on a remotely owned ped with the \"sync over network\" option set!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
                }
                else if(ped->GetSpeechAudioEntity())
			    {
				    audDamageStats stats;
				    stats.DamageReason = (audDamageReason)damageReason;
				    stats.RawDamage = rawDamage;
				    //don't run velocity safety check for script-triggered falling, as sometimes they have crazy falls where the ped's initial
				    //	velocity is actually upwards (a la Trevor2 guy falling off rising airplane)
				    if(stats.DamageReason == AUD_DAMAGE_REASON_FALLING || stats.DamageReason == AUD_DAMAGE_REASON_SUPER_FALLING)
					    ped->GetSpeechAudioEntity()->SetBlockFallingVelocityCheck();
				    ped->GetSpeechAudioEntity()->InflictPain(stats);

                    if(syncOverNetwork)
                    {
                        CPedPlayPainEvent::Trigger(ped, damageReason, rawDamage);
                    }
			    }
            }
		}
	}

	void CommandReserveAmbientSpeechSlot()
	{
		g_SpeechManager.ReserveAmbientSpeechSlotForScript();
	}

	void CommandReleaseAmbientSpeechSlot()
	{
		g_SpeechManager.ReleaseAmbientSpeechSlotForScript();
	}

	void CommandPlayPreloadedSpeech(int pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayPreloadedSpeech(0);
				//ped->GetSpeechAudioEntity()->PlayPreloadedSpeech_FromScript();
			}		
		}
	}		

	void CommandSetAmbientVoiceName(int pedIndex, const char* voiceName)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{	
				ped->GetSpeechAudioEntity()->SetAmbientVoiceName(atStringHash(voiceName), true);
			}
		}
	}

    void CommandSetAmbientVoiceNameHash(int pedIndex, const int voiceNameHash)
    {
        CPed* ped = NULL;
        if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
        {
            ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
            if(ped && ped->GetSpeechAudioEntity())
            {	
                ped->GetSpeechAudioEntity()->SetAmbientVoiceName((u32)voiceNameHash, true);
            }
        }
    }

    int CommandGetAmbientVoiceNameHash(int pedIndex)
    {
        CPed* ped = NULL;
        if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
        {
            ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
            if(ped && ped->GetSpeechAudioEntity())
            {	
                return ped->GetSpeechAudioEntity()->GetAmbientVoiceName();
            }
        }

        return 0u;
    }

	void CommandForceFullVoice(int pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->ForceFullVoice();
			}
		}
	}

	void CommandSetPedRaceAndVoiceGroup(int pedIndex,int pedRace, int PVGHash = 0)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->ForceFullVoice(PVGHash,pedRace);
			}
		}
	}

	void CommandSetPedGroupVoice(int pedIndex, s32 PVGHash)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->ForceFullVoice(PVGHash);
			}
		}
	}

	void CommandSetPedRaceToPedVoiceGroup(int pedIndex, s32 PedRaceToPVGHash)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				if(const PedRaceToPVG* pedRaceToPVG = audNorthAudioEngine::GetObject<PedRaceToPVG>(PedRaceToPVGHash))
				{
					u32 pvgHash = ped->GetSpeechAudioEntity()->GetPVGHashFromRace(pedRaceToPVG);
					audDisplayf("SET_PED_VOICE_GROUP_FROM_RACE_TO_PVG is setting voice %u (PedRaceToPVG: %d, PedRace: %d)", pvgHash, PedRaceToPVGHash, (s32)ped->GetPedRace());
					ped->GetSpeechAudioEntity()->ForceFullVoice(pvgHash);
				}
			}
		}
	}

	void CommandSetPedGender(int pedIndex, bool isMale)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->SetPedGender(isMale);
			}
		}
	}
	void CommandSetVoiceIdFromHeadComponent(int iPedIndex, int UNUSED_PARAM(iPedComponent), bool UNUSED_PARAM(bMale))
	{
		CPed* pPed = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != iPedIndex)
		{
			pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedIndex);
			if(pPed && pPed->GetSpeechAudioEntity())
			{
				{
					pPed->GetSpeechAudioEntity()->ForceFullVoice();
				}
			}
		}
	}

	void CommandStopCurrentlyPlayingSpeech(int pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->StopSpeech();
			}
		}
	}

	void CommandCancelCurrentlyPlayingAmbientSpeech(int pedIndex)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->StopAmbientSpeech();
			}
		}
	}

	bool CommandIsAmbientSpeechPlaying(int pedIndex)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				return ped->GetSpeechAudioEntity()->IsAmbientSpeechPlaying();
			}	
		}
		return false;
	}
	
	bool CommandIsScriptedSpeechPlaying(int pedIndex)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				return ped->GetSpeechAudioEntity()->IsScriptedSpeechPlaying();
			}	
		}
		return false;
	}

	bool CommandIsAnySpeechPlaying(int pedIndex)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				return (ped->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() || ped->GetSpeechAudioEntity()->IsScriptedSpeechPlaying() ||
					ped->GetSpeechAudioEntity()->IsPainPlaying());
			}
		}
		return false;
	}

	bool CommandIsAnyPositionalSpeechPlaying()
	{
		return g_ScriptAudioEntity.GetAmbientSpeechAudioEntity().IsAmbientSpeechPlaying() || 
			   g_ScriptAudioEntity.GetAmbientSpeechAudioEntity().IsScriptedSpeechPlaying() ||
			   g_ScriptAudioEntity.GetAmbientSpeechAudioEntity().IsPainPlaying();
	}

	bool CommandDoesContextExistForThisPed(int pedIndex, const char* context, bool allowBackupPVG)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				bool isPlayerCharacterVoice = (ped->GetPedType() == PEDTYPE_PLAYER_0 ||
																ped->GetPedType() == PEDTYPE_PLAYER_1 ||
																ped->GetPedType() == PEDTYPE_PLAYER_2);

				if(CTheScripts::GetIsInDirectorMode() &&  ped->GetSpeechAudioEntity()->GetAmbientVoiceName() == 0 && !isPlayerCharacterVoice)
				{
					ped->GetSpeechAudioEntity()->SetAmbientVoiceName(context, allowBackupPVG);
				}
				return ped->GetSpeechAudioEntity()->DoesContextExistForThisPed(context, allowBackupPVG);
			}
		}
		return false;
	}

	bool CommandSetPedVoiceForContext(int pedIndex, const char* context, bool allowBackupPVG)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				return ped->GetSpeechAudioEntity()->SetPedVoiceForContext(context, allowBackupPVG);
			}
		}
		return false;
	}

	bool CommandIsPedInCurrentConversation(int pedIndex)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped)
			{
				return g_ScriptAudioEntity.IsPedInCurrentConversation(ped);
			}
		}
		return false;
	}

	void CommandSetPedIsDrunk(int pedIndex, bool isDrunk)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->SetPedIsDrunk(isDrunk);
			}
		}
	}

	void CommandSetPedIsBlindRaging(int pedIndex, bool isBlindRaging)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->SetPedIsBlindRaging(isBlindRaging);
			}
		}
	}

	void CommandSetPedIsCrying(int pedIndex, bool isCrying)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				if(isCrying)
					ped->GetSpeechAudioEntity()->StartCrying();
				else
					ped->GetSpeechAudioEntity()->StopCrying();
			}
		}
	}

	void CommandPlayAnimalVocalization(int pedIndex, int animalType, const char* context)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);

			audDisplayf("Triggering animal vocal %s on ped %d, animal type %d (from script %s)", context, animalType, pedIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if (ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalType)animalType, context);
				if (NetworkInterface::IsGameInProgress() && ped->GetNetworkObject())
				{
					CAudioBarkingEvent::Trigger(ped->GetNetworkObject()->GetObjectID(), atPartialStringHash(context), VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()));
				}
			}
		}
	}

	bool CommandIsAnimalVocalizationPlaying(int pedIndex)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				return ped->GetSpeechAudioEntity()->IsAnimalVocalizationPlaying();
			}	
		}
		return false;
	}

	void CommandSetAnimalMood(int pedIndex, int mood)
	{
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex && mood < AUD_ANIMAL_MOOD_NUM_MOODS)
		{
			CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->SetAnimalMood((audAnimalMood)mood);
			}	
		}
	}


	void CommandHandleAudioAnimEvent(int pedIndex, const char* audioEvent)
	{
		CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped)
			{
				ped->GetPedAudioEntity()->HandleAnimEventFlag(audioEvent);
			}
		}

	}

#if NA_POLICESCANNER_ENABLED
	void CommandTriggerPoliceReportSpottingNPC(int pedIndex, f32 accuracy, bool force, bool forceVehicle)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped)
			{
				g_PoliceScanner.ReportPoliceSpottingNonPlayer(ped,accuracy,force,forceVehicle);
			}
		}
	}

	s32 CommandTriggerPoliceReport(const char *reportName, f32 applyValue)
	{
		return g_AudioScannerManager.TriggerScriptedReport(reportName,applyValue);
	}

	void CommandCancelPoliceReport(s32 scriptID)
	{
		g_AudioScannerManager.CancelScriptedReport(scriptID);
	}

	void CommandCancelAllPoliceReports()
	{
		g_AudioScannerManager.CancelAllReports();
	}

	bool CommandIsPoliceReportPlaying(s32 scriptID)
	{
		return g_AudioScannerManager.IsScriptedReportPlaying(scriptID);
	}
	
	bool CommandIsPoliceReportValid(s32 scriptID)
	{
		return g_AudioScannerManager.IsScriptedReportValid(scriptID);
	}

	int CommandGetPlayingPoliceReport()
	{
		return g_AudioScannerManager.GetPlayingScriptedReport();
	}

	void CommandTriggerVigilanteCrime(int crimeId, const scrVector &pos)
	{
		Vector3 position = Vector3(pos);
		g_PoliceScanner.TriggerVigilanteCrime((u32)crimeId, position);
	}

	void CommandSetPolicScannerCarCodeInfo(int UnitType, int BeatNum)
	{
		g_AudioScannerManager.SetCarUnitValues(UnitType, BeatNum);
	}

	void CommandSetPoliceScannerPositionInfo(const scrVector &pos)
	{
		Vector3 position = Vector3(pos);
		g_AudioScannerManager.SetPositionValue(position);
	}

	void CommandSetPoliceScannerCrimeInfo(s32 crime)
	{
		g_AudioScannerManager.SetCrimeValue(crime);
	}
#endif // NA_POLICESCANNER_ENABLED

#if NA_RADIO_ENABLED
	bool CommandIsMobilePhoneRadioActive(void)
	{
		return g_RadioAudioEntity.IsMobilePhoneRadioActive();
	}

	void CommandSetMobilePhoneRadioState(bool isActive)
	{
		g_RadioAudioEntity.SetMobilePhoneRadioState(isActive);
	}

	s32 CommandGetPlayerRadioStationIndex(void)
	{
		const audRadioStation *station = g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
		if(station)
			return static_cast<s32>(station->GetStationIndex());
		return g_OffRadioStation;
	}

	const char *CommandGetPlayerRadioStationName(void)
	{
		return g_RadioAudioEntity.GetPlayerRadioStationNamePendingRetune();
	}

	const char *CommandGetRadioStationName(s32 stationIndex)
	{
		audRadioStation *station = audRadioStation::GetStation(stationIndex);
		if(station)
		{
			return station->GetName();
		}
		return "OFF";
	}

	s32 CommandFindRadioStationIndex(s32 stationNameHash)
	{
		audRadioStation *station = audRadioStation::FindStation(stationNameHash);
		if(station)
		{
			return station->GetStationIndex();
		}
		return static_cast<s32>(g_NullRadioStation);
	}

	void CommandSetRadioStationMusicOnly(const char *stationName, const bool musicOnly)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if(audVerifyf(station, "Failed to find radio station %s", stationName))
		{
			station->SetMusicOnly(musicOnly);
		}
	}

	void CommandSetRadioFrontendFadeTime(const float timeSeconds)
	{
		g_RadioAudioEntity.SetFrontendFadeTime(timeSeconds);
	}

	s32 CommandGetPlayerRadioStationGenre()
	{
		const audRadioStation *station = g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
		if(station)
		{
			return station->GetGenre();
		}
		return RADIO_GENRE_OFF;
	}

	bool CommandIsRadioRetuning(void)
	{
		return g_RadioAudioEntity.IsRetuning();
	}

	bool CommandIsRadioFadedOut(void)
	{
		return g_RadioAudioEntity.IsRadioFadedOut();
	}

	void CommandRetuneRadioUp(void)
	{
		audDisplayf("%s tuning radio up", CTheScripts::GetCurrentScriptName());
		if(g_AudioEngine.IsGameInControlOfMusicPlayback())
			audRadioAudioEntity::RetuneRadioUp();
	}

	void CommandRetuneRadioDown(void)
	{
		audDisplayf("%s tuning radio down", CTheScripts::GetCurrentScriptName());
		if(g_AudioEngine.IsGameInControlOfMusicPlayback())
			audRadioAudioEntity::RetuneRadioDown();
	}

	void CommandRetuneRadioToStationName(const char *stationName)
	{
		audDisplayf("%s tuning radio to %s", CTheScripts::GetCurrentScriptName(), stationName);
		g_RadioAudioEntity.RetuneToStation(stationName);
	}

	void CommandRetuneRadioToStationIndex(int radioStationIndex)
	{
		audDisplayf("%s tuning radio to index %d", CTheScripts::GetCurrentScriptName(), radioStationIndex);
		g_RadioAudioEntity.RetuneToStation(audRadioStation::GetStation((u32)radioStationIndex));
	}
	
	void CommandSetFrontendRadio (bool Active)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->SetAudioFlag(audScriptAudioFlags::FrontendRadioDisabled, !Active);
		}
	}

	void CommandSetUserRadioControlEnabled(const bool enabled)
	{
		// Since this command has the ability to create hard to debug situations, log every time it is used
		// so if the radio is broken we can easily see why retrospectively.
		
		Warningf("Script %s %sabled user radio control", CTheScripts::GetCurrentScriptName(), enabled ? "en" : "dis");
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->SetAudioFlag(audScriptAudioFlags::UserRadioControlDisabled, !enabled);
		}
	}

	void CommandUnlockMissionNewsStory(const int story)
	{
		audRadioStation::UnlockMissionNewsStory((u32)story);
	}

	bool CommandIsMissionNewsStoryUnlocked(const int story)
	{
		return audRadioStation::IsMissionNewsStoryUnlocked((u32)story);
	}

	void UpdateDLCBattleUnlockableTracks(bool UNUSED_PARAM(allowReprioritization))
	{
		//audDisplayf("Script %s is calling UPDATE_UNLOCKABLE_DJ_RADIO_TRACKS", CTheScripts::GetCurrentScriptName());
		//g_RadioAudioEntity.UpdateDLCBattleUnlockableTracks(allowReprioritization);
	}	

	void CommandUnlockRadioStationTrackList(const char *radioStationName, const char *trackListName)
	{
		audRadioStation *radioStation = audRadioStation::FindStation(radioStationName);
		if(audVerifyf(radioStation, "Invalid radio station name - %s", radioStationName))
		{
			audDisplayf("UNLOCK_RADIO_STATION_TRACK_LIST called on %s, track list %s", radioStationName, trackListName);

			// USB mix station locking is handled internally via ForceMusicTrackList
			if(!radioStation->IsUSBMixStation())
			{
				if(!radioStation->UnlockTrackList(trackListName))
				{
					audWarningf("Failed to unlock tracklist %s on %s", trackListName, radioStationName);
				}
			}
		}
	}

	void CommandLockRadioStationTrackList(const char *radioStationName, const char *trackListName)
	{
		audRadioStation *radioStation = audRadioStation::FindStation(radioStationName);
		if(audVerifyf(radioStation, "Invalid radio station name - %s", radioStationName))
		{
			audDisplayf("LOCK_RADIO_STATION_TRACK_LIST called on %s, track list %s", radioStationName, trackListName);

			// USB mix station locking is handled internally via ForceMusicTrackList
			if(!radioStation->IsUSBMixStation())
			{
				if(!radioStation->LockTrackList(trackListName))
				{
					audWarningf("Failed to lock tracklist %s on %s", trackListName, radioStationName);
				}
			}
			else
			{
				u32 trackListNameHash = atStringHash(trackListName);

				if(trackListNameHash != ATSTRINGHASH("TUNER_AP_SILENCE_MUSIC", 0x68865F9) && trackListNameHash == radioStation->GetLastForcedMusicTrackList())
				{
					audDisplayf("LOCK_RADIO_STATION_TRACK_LIST is locking the most recently forced music track list (%u), switching playback to TUNER_AP_SILENCE_MUSIC", trackListNameHash);
					radioStation->ForceMusicTrackList(ATSTRINGHASH("TUNER_AP_SILENCE_MUSIC", 0x68865F9), 0u);
				}
			}
		}
	}

	bool CommandIsRadioStationTrackListUnlocked(const char *radioStationName, const char *trackListName)
	{
		audRadioStation *radioStation = audRadioStation::FindStation(radioStationName);
		audAssertf(radioStation, "Invalid radio station name - %s", radioStationName);
		return radioStation ? radioStation->IsTrackListUnlocked(trackListName) : false;		
	}

	void CommandLockRadioStation(const char* radioStationName, bool shouldLock)
	{
		if (atStringHash(radioStationName) == ATSTRINGHASH("RADIO_22_DLC_BATTLE_MIX1_RADIO", 0xF8BEAA16))
		{
			audDisplayf("Ignoring LOCK_RADIO_STATION request on station RADIO_22_DLC_BATTLE_MIX1_RADIO, now permanently unlocked");
			return;
		}
		
		audDisplayf("LOCK_RADIO_STATION is %s station %s (from script %s)", shouldLock ? "locking" : "unlocking", radioStationName, CTheScripts::GetCurrentScriptName());
		audRadioStation *radioStation = audRadioStation::FindStation(radioStationName);
		if (audVerifyf(radioStation, "Invalid radio station name - %s", radioStationName))
		{
			radioStation->SetLocked(shouldLock);
		}
	}

	void CommandSetRadioStationAsFavourite(const char* radioStationName, bool isFavourite)
	{
		audDisplayf("SET_RADIO_STATION_AS_FAVOURITE is %s station %s (from script %s)", isFavourite ? "favouriting" : "un-favouriting", radioStationName, CTheScripts::GetCurrentScriptName());
		audRadioStation *radioStation = audRadioStation::FindStation(radioStationName);
		if (audVerifyf(radioStation, "Invalid radio station name - %s", radioStationName))
		{
			radioStation->SetFavourited(isFavourite);
		}
	}

	bool CommandIsRadioStationFavourited(const char* radioStationName)
	{
		audRadioStation *radioStation = audRadioStation::FindStation(radioStationName);
		if (audVerifyf(radioStation, "Invalid radio station name - %s", radioStationName))
		{
			return radioStation->IsFavourited();
		}

		return false;
	}

	int CommandGetAudibleTrackTextId()
	{
		return (int)g_RadioAudioEntity.GetAudibleTrackTextId();
	}

	void CommandReportTaggedRadioTrack(int TrackId)
	{
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
		{
			char trackIdString[32];
			formatf(trackIdString, "%d", TrackId);
			rlSocialClub::PostUserFeedActivity(localGamerIndex, "SHARE_SPOTIFY_SONG", 0, trackIdString, NULL);
		}

		CNetworkTelemetry::AudTrackTagged((u32)TrackId);
	}

	void CommandFreezeRadioStation(const char *stationName)
	{
		audRadioStation* radioStation = audRadioStation::FindStation(stationName);

		if (audVerifyf(radioStation, "FREEZE_RADIO_STATION - Failed to find station %s", stationName))
		{
			radioStation->Freeze();
		}
		
		// For safety always default to auto-unfreeze
		g_RadioAudioEntity.SetAutoUnfreezeForPlayer(true);
	}

	void CommandUnfreezeRadioStation(const char *stationName)
	{
		audRadioStation* radioStation = audRadioStation::FindStation(stationName);		

		if(audVerifyf(radioStation, "Failed to find radio station %s", stationName))
		{
			audDisplayf("UNFREEZE_RADIO_STATION called on station %s, station is currently %s playing track %u, next track %u", stationName, radioStation->IsFrozen()? "frozen" : "unfrozen", radioStation->GetCurrentTrack().GetSoundRef(), radioStation->GetNextTrack().GetSoundRef());
			radioStation->Unfreeze();
		}		
	}

	void CommandSetAutoUnfreeze(const bool enable)
	{
		g_RadioAudioEntity.SetAutoUnfreezeForPlayer(enable);
	}

	void CommandForceInitialPlayerRadioStation(const char *stationName)
	{
		audWarningf("Script %s is forcing player radio to %s", CTheScripts::GetCurrentScriptName(), stationName);
		g_RadioAudioEntity.ForcePlayerRadioStation(audRadioStation::FindStation(stationName)->GetStationIndex());
	}

	void CommandForceRadioTrack(const char *stationName, const char *trackSettingsName)
	{
		audWarningf("Script %s is forcing player radio to %s/%s", CTheScripts::GetCurrentScriptName(), stationName, trackSettingsName);
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if(audVerifyf(station, "Failed to find station: %s", stationName))
		{
			station->ForceTrack(trackSettingsName);
		}
	}

	void CommandForceRadioTrackWithStartOffset(const char *stationName, const char *trackSettingsName, s32 startOffsetMs)
	{
		audWarningf("Script %s is forcing player radio to %s/%s with start offset %d", CTheScripts::GetCurrentScriptName(), stationName, trackSettingsName, startOffsetMs);
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station: %s", stationName))
		{
			station->ForceTrack(trackSettingsName, startOffsetMs);
		}
	}

	void CommandForceNextRadioTrack(const char* stationName, const char* categoryName, const char* contextName, const char* trackName)
	{
		audWarningf("Script %s is forcing station %s next track to category %s track %s", CTheScripts::GetCurrentScriptName(), stationName, categoryName, trackName);
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station: %s", stationName))
		{
			station->ForceNextTrack(categoryName, atStringHash(contextName), atStringHash(trackName), true);
		}
	}

	u32 CommandGetCurrentTrackSoundName(const char *stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station: %s", stationName))
		{
			return station->GetCurrentTrack().GetSoundRef();
		}

		return 0u;
	}

	void CommandSkipRadioForward()
	{
		g_RadioAudioEntity.SkipStationsForward();
	}

	void CommandSetEndCreditsMusic(bool Active)
	{
		if (Active)
		{
			g_RadioAudioEntity.StartEndCredits();
		}
		else
		{
			g_RadioAudioEntity.StopEndCredits();
		}
	}

	void CommandSetLoudVehicleRadio(int VehicleIndex, bool loud)
	{
		CVehicle *pVehicle;
		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetLoudRadio(loud);
		}
	}

	bool CommandCanVehicleReceiveCBRadio(int VehicleIndex)
	{
		CVehicle *pVehicle;
		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			return pVehicle && pVehicle->GetVehicleAudioEntity()->HasCBRadio() && audRadioStation::GetCountryTalkRadioSignal() == 1.0f;
		}
		else
		{
			return false;
		}
	}

	int CommandGetVehicleHornSoundIndex(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			return (s32)pVehicle->GetVehicleAudioEntity()->GetHornSoundIdx();
		}

		return -1;
	}

	void CommandSetVehicleHornSoundIndex(int VehicleIndex, int hornSoundIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if (pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetHornSoundIdx((s16)hornSoundIndex);
		}
	}
	
	void CommandSetVehicleRadioEnabled(int VehicleIndex, bool enabled)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetRadioEnabled(enabled);
			audWarningf("Script is %s the radio in %svehicle %s (%f,%f,%f)", enabled ? "enabling" : "disabling", 
				pVehicle->ContainsLocalPlayer() ? "player " : "",
				pVehicle->GetModelName(), pVehicle->GetTransform().GetPosition().GetX().Getf(), pVehicle->GetTransform().GetPosition().GetY().Getf(), pVehicle->GetTransform().GetPosition().GetZ().Getf());
		}
	}

	void CommandSetPositionedPlayerVehicleRadioEmitterEnabled(bool enabled)
	{
		audDisplayf("Script %s is %s positioned player vehicle radio emitter", CTheScripts::GetCurrentScriptName(), enabled? "enabling" : "disabling");
		audRadioSlot::SetPositionedPlayerVehicleRadioEmitterEnabled(enabled);
	}

	void CommandSetVehicleForcedRadio(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetScriptForcedRadio();
		}
	}

	bool CommandIsVehicleRadioOn(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			return pVehicle->GetVehicleAudioEntity()->IsRadioSwitchedOn();			
		}

		return false;
	}

	void CommandSetVehicleRadioStation(int VehicleIndex, const char *stationName)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			if(stationName == NULL || !strcmp(stationName, "OFF"))
			{
				audDisplayf("SET_VEH_RADIO_STATION - Script is requesting %s retune to OFF (currently %s) (from %s)", pVehicle->GetModelName(), pVehicle->GetVehicleAudioEntity()->GetRadioStation()? pVehicle->GetVehicleAudioEntity()->GetRadioStation()->GetName() : "nullPtr", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				pVehicle->GetVehicleAudioEntity()->SetRadioOffState(true);
			}
			else
			{
				audDisplayf("SET_VEH_RADIO_STATION - Script is requesting %s retune to station %s (from %s)", pVehicle->GetModelName(), stationName, CTheScripts::GetCurrentScriptNameAndProgramCounter());
				pVehicle->GetVehicleAudioEntity()->SetScriptRequestedRadioStation();
				pVehicle->GetVehicleAudioEntity()->SetRadioStation(audRadioStation::FindStation(stationName));				
				audDisplayf("SET_VEH_RADIO_STATION - %s now tuned to station %s", pVehicle->GetModelName(), pVehicle->GetVehicleAudioEntity()->GetRadioStation()? pVehicle->GetVehicleAudioEntity()->GetRadioStation()->GetName() : "nullPtr");
			}
		}
	}

	void CommandSetVehicleHasNormalRadio(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetRadioType(RADIO_TYPE_NORMAL);
		}
	}

	void CommandSetEmitterRadioStation(const char *emitterName, const char *stationName)
	{
		g_EmitterAudioEntity.SetEmitterRadioStation(emitterName, stationName);
	}

	void CommandSetEmitterEnabled(const char *emitterName, const bool enabled)
	{
		if (naVerifyf(emitterName, "SET_STATIC_EMITTER_ENABLED - Script %s passed through an empty emitter name string", CTheScripts::GetCurrentScriptName()))
		{
			if (g_EmitterAudioEntity.SetEmitterEnabled(emitterName, enabled))
			{
				audDisplayf("SET_STATIC_EMITTER_ENABLED is %s emitter %s (from script %s)", enabled ? "enabling" : "disabling", emitterName, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}		
	}

	void CommandLinkStaticEmitterToEntity(const char* emitterName, const int entityIndex)
	{
		CEntity* entity = NULL;
		if(SCRIPT_VERIFY(NULL_IN_SCRIPTING_LANGUAGE != entityIndex, "LINK_STATIC_EMITTER_TO_ENTITY - trying to link emitter to a null entity"))
		{
			entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
			if(entity)
			{
				g_EmitterAudioEntity.SetEmitterLinkedObject(emitterName, entity, true);
				return;
			}			
		}
	}

	void CommandSetMobileRadioEnabledDuringGameplay(const bool enabled)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "MobileRadioInGame", CTheScripts::GetCurrentScriptName()))
		{
			script->SetAudioFlag(audScriptAudioFlags::MobileRadioInGame, enabled);
		}		
	}

	bool CommandDoesPlayerVehicleHaveRadio()
	{
		CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
		if(playerVeh && playerVeh->GetVehicleAudioEntity())
		{
			return playerVeh->GetVehicleAudioEntity()->GetHasNormalRadio();
		}
		return false;
	}

	bool CommandIsPlayerVehRadioEnable()
	{
		CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
		
		if (playerVeh && playerVeh->GetVehicleAudioEntity())
		{
			return playerVeh->GetVehicleAudioEntity()->IsRadioEnabled();
		}
		return false;
	}

	void CommandSetAudioFlag(const char *flagName, const bool enabled)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", flagName, CTheScripts::GetCurrentScriptName()))
		{
			script->SetAudioFlag(flagName, enabled);
		}
	}

	void CommandSetCustomRadioTrackList(const char *stationName, const char *trackListName, bool forceNow)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if(audVerifyf(station, "Failed to find station %s for track list %s", stationName, trackListName))
		{
			const RadioStationTrackList *trackList = audNorthAudioEngine::GetObject<RadioStationTrackList>(trackListName);
			if(audVerifyf(trackList, "Failed to find track list %s", trackListName))
			{
				station->QueueTrackList(trackList, forceNow);
			}
		}
	}

	void CommandClearCustomRadioTrackList(const char *stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if(audVerifyf(station, "Failed to find station %s", stationName))
		{
			station->ClearQueuedTracks();
		}
	}

	s32 CommandGetNumUnlockedRadioStations()
	{
		return audRadioStation::GetNumUnlockedStations();
	}

	s32 CommandGetNumUnlockedFavouritedRadioStations()
	{
		return audRadioStation::GetNumUnlockedFavouritedStations();
	}

	void CommandShutDownActiveTracks(const char* stationName)
	{
		audDisplayf("SHUT_DOWN_ACTIVE_TRACKS called on %s", stationName);

		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			station->GetCurrentTrack().Shutdown();
			station->GetNextTrack().Shutdown();
		}
	}

	void CommandForceMusicTrackList(const char* stationName, const char* trackListName, int startOffsetMs)
	{
		audDisplayf("FORCE_MUSIC_TRACK_LIST called on %s, track list %s", stationName, trackListName);

		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			station->ForceMusicTrackList(atStringHash(trackListName), startOffsetMs);
		}
	}

	u32 CommandGetLastForcedTrackList(const char* stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetLastForcedMusicTrackList();
		}

		return 0u;
	}

	int CommandGetMusicTrackListPlayTimeOffset(const char* stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			station->GetMusicTrackListCurrentPlayTimeMs();
		}

		return -1;
	}

	int CommandGetMusicTrackListDurationMs(const char* stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListDurationMs();
		}

		return -1;
	}

	int CommandGetMusicTrackListTrackDurationMs(const char* stationName, int trackIndex)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListTrackDurationMs(trackIndex);
		}

		return -1;
	}

	int CommandGetMusicTrackListActiveTrackIndex(const char* stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListActiveTrackIndex();
		}

		return -1;
	}

	int CommandGetMusicTrackListNumTracks(const char* stationName)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListNumTracks();
		}

		return -1;
	}

	int CommandGetMusicTrackListTrackIDForTimeOffset(const char* stationName, int timeOffset)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListTrackIDForTimeOffset(timeOffset);
		}

		return -1;
	}

	int CommandGetMusicTrackListTrackIDForIndex(const char* stationName, int index)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListTrackIDForIndex(index);
		}

		return -1;
	}

	int CommandGetMusicTrackListTrackStartTimeMs(const char* stationName, int trackIndex)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->GetMusicTrackListTrackStartTimeMs(trackIndex);
		}

		return -1;
	}

	void CommandSkipToOffsetInMusicTrackList(const char* stationName, int offsetMs)
	{
		audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find station %s", stationName))
		{
			return station->SkipToOffsetInMusicTrackList(offsetMs);
		}
	}
#endif // NA_RADIO_ENABLED

	void CommandForceVehicleGameObject(int VehicleIndex, const char *vehicleName)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			audAssertf(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR && audNorthAudioEngine::GetObject<CarAudioSettings>(atStringHash(vehicleName)) ||
				pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BICYCLE && audNorthAudioEngine::GetObject<BicycleAudioSettings>(atStringHash(vehicleName)) ||
				pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE && audNorthAudioEngine::GetObject<PlaneAudioSettings>(atStringHash(vehicleName)) ||
				pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN && audNorthAudioEngine::GetObject<TrainAudioSettings>(atStringHash(vehicleName)) ||
				pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BOAT && audNorthAudioEngine::GetObject<BoatAudioSettings>(atStringHash(vehicleName)),
				"Vehicle game object %s does not exist, or is not a valid type for vehicle %s!", vehicleName, pVehicle->GetDebugName());

			pVehicle->GetVehicleAudioEntity()->ForceUseGameObject(atStringHash(vehicleName));
		}
	}

	void CommandSetVehicleStartupRevSound(int VehicleIndex, const char* soundName, const char * setName)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(audVerifyf(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR, "Startup revs can only be set on cars"))
			{
				audMetadataRef soundRef = g_NullSoundRef;

				if(setName)
				{
					audSoundSet soundSet;
					soundSet.Init(setName);
					soundRef = soundSet.Find(soundName);
				}
				else
				{
					soundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(atStringHash(soundName));
				}

				((audCarAudioEntity*)pVehicle->GetVehicleAudioEntity())->SetScriptStartupRevsSoundRef(soundRef);
			}
		}
	}

	void CommandResetVehicleStartupRevSound(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(audVerifyf(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR, "Startup revs can only be set on cars"))
			{				
				((audCarAudioEntity*)pVehicle->GetVehicleAudioEntity())->CancelScriptStartupRevsSound();
			}
		}
	}

	void CommandSetVehicleForceReverseWarning(int VehicleIndex, bool ForceReverseWarning)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetForceReverseWarning(ForceReverseWarning);
		}
	}

	bool CommandIsVehicleAudiblyDamaged(int VehicleIndex)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			return pVehicle->GetVehicleAudioEntity()->GetFakeBodyHealth() < 500 || pVehicle->GetVehicleAudioEntity()->GetFakeEngineHealth() < 500;
		}

		return false;
	}

	void CommandSetEngineDamageFactor(int VehicleIndex, float damageFactor)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetScriptEngineDamageFactor(damageFactor);
		}
	}

	void CommandSetBodyDamageFactor(int VehicleIndex, float damageFactor)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetScriptBodyDamageFactor(damageFactor);
		}
	}

	void CommandEnableFanbeltDamage(int VehicleIndex, bool enableFanbeltDamage)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "ENABLE_VEHICLE_FANBELT_DAMAGE - Vehicle is not a car"))
			{
				((audCarAudioEntity*)pVehicle->GetVehicleAudioEntity())->GetVehicleEngine()->SetHasBreakableFanbelt(enableFanbeltDamage);
			}
		}
	}

	void CommandPreLoadVehicleBank( int ModelNameHash )
	{
		g_ScriptAudioEntity.RequestScriptVehicleAudio(ModelNameHash);			
	}

	void CommandSetAmbientZoneState(const char *zoneName, const bool enabled, const bool forceUpdate)
	{
		audAssertf(audNorthAudioEngine::GetObject<AmbientZone>(atStringHash(zoneName)), "Ambient zone %s is not valid!", zoneName);
		g_ScriptAudioEntity.SetAmbientZoneNonPersistentState(atStringHash(zoneName), enabled, forceUpdate);
	}

	void CommandClearAmbientZoneState(const char *zoneName, const bool forceUpdate)
	{
		audAssertf(audNorthAudioEngine::GetObject<AmbientZone>(atStringHash(zoneName)), "Ambient zone %s is not valid!", zoneName);
		g_ScriptAudioEntity.ClearAmbientZoneNonPersistentState(atStringHash(zoneName), forceUpdate);
	}

	void CommandSetAmbientZoneStatePersistent(const char *zoneName, const bool enabled, const bool forceUpdate)
	{
		audAssertf(audNorthAudioEngine::GetObject<AmbientZone>(atStringHash(zoneName)), "Ambient zone %s is not valid!", zoneName);
		g_AmbientAudioEntity.SetZonePersistentStatus(atStringHash(zoneName), enabled, forceUpdate);
	}

	void CommandSetAmbientZoneListState(const char *zoneListName, const bool enabled, const bool forceUpdate)
	{
		AmbientZoneList * zoneList = audNorthAudioEngine::GetObject<AmbientZoneList>(atStringHash(zoneListName));
		audAssertf(zoneList, "Ambient zone list %s is not valid!", zoneListName);

		if(zoneList)
		{
			for(u32 i = 0; i < zoneList->numZones; i++)
			{
				g_ScriptAudioEntity.SetAmbientZoneNonPersistentState(zoneList->Zone[i].Ref, enabled, forceUpdate);
			}
		}
	}

	void CommandClearAmbientZoneListState(const char *zoneListName, const bool forceUpdate)
	{
		AmbientZoneList * zoneList = audNorthAudioEngine::GetObject<AmbientZoneList>(atStringHash(zoneListName));
		audAssertf(zoneList, "Ambient zone list %s is not valid!", zoneListName);

		if(zoneList)
		{
			for(u32 i = 0; i < zoneList->numZones; i++)
			{
				g_ScriptAudioEntity.ClearAmbientZoneNonPersistentState(zoneList->Zone[i].Ref, forceUpdate);
			}
		}
	}

	void CommandSetAmbientZoneListStatePersistent(const char *zoneListName, const bool enabled, const bool forceUpdate)
	{
		AmbientZoneList * zoneList = audNorthAudioEngine::GetObject<AmbientZoneList>(atStringHash(zoneListName));
		audAssertf(zoneList, "Ambient zone list %s is not valid!", zoneListName);

		if(zoneList)
		{
			for(u32 i = 0; i < zoneList->numZones; i++)
			{
				g_AmbientAudioEntity.SetZonePersistentStatus(zoneList->Zone[i].Ref, enabled, forceUpdate);
			}
		}
	}

	bool CommandIsAmbientZoneEnabled(const char *zoneName)
	{
		AmbientZone * zone = audNorthAudioEngine::GetObject<AmbientZone>(atStringHash(zoneName));
		audAssertf(zone, "Ambient zone %s is not valid!", zoneName);

		if(zone)
		{
			bool hasTemporaryState = AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT)!=AUD_TRISTATE_UNSPECIFIED;
			bool zoneEnabled = AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT)==AUD_TRISTATE_TRUE;

			// If this zone has a temporary modifier assigned, then defer to this, otherwise use the persistent state
			if(hasTemporaryState)
			{
				zoneEnabled = AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT) == AUD_TRISTATE_TRUE;
			}

			return zoneEnabled;
		}
		
		return false;
	}

	void CommandSetVehicleMissileWarningEnabled(int VehicleIndex, bool missileWarningEnabled)
	{
		CVehicle *pVehicle;
		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetHasMissileWarningSystem(missileWarningEnabled);
		}
	}

	void CommandSetCutsceneAudioOverride(const char *override)
	{
		g_CutsceneAudioEntity.SetScriptCutsceneOverride(override);
	}

	s32 CommandGetCutsceneAudioTimeMs()
	{
		return g_CutsceneAudioEntity.GetPlayTimeMs();
	}

	void CommandSetVariableOnSynchedScene(const char * name, float value)
	{
		g_CutsceneAudioEntity.SetVariableOnSound(atStringHash(name), value);
	}

	void CommandSetProximityWhooshesEnabled(bool enabled)
	{
		g_ReflectionsAudioEntity.SetProximityWhooshesEnabled(enabled);
	}

	void CommandEnableChaseAudio(bool enable)
	{
		g_ScriptAudioEntity.EnableChaseAudio(enable);
	}

	void CommandBlipSiren(int vehIndex)
	{
		CVehicle *veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);
		if(veh)
		{
			veh->GetVehicleAudioEntity()->BlipSiren();
		}
	}

	void CommandOverrideHorn(int vehIndex,bool override, const int overridenHornHash)
	{
		CVehicle *veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);
		if(veh)
		{
			veh->GetVehicleAudioEntity()->OverrideHorn(override, (u32)overridenHornHash);
		}
	}

	bool CommandIsHornActive(int vehIndex)
	{
		CVehicle* veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(veh)
		{
			return veh->GetVehicleAudioEntity()->IsHornActive();
		}
		return false;
	}

	void CommandEnableParkingBeep(int vehIndex,bool enable)
	{
		CVehicle *veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);
		if(veh)
		{
			veh->GetVehicleAudioEntity()->EnableParkingBeep(enable);
		}
	}

	void CommandEnableAggressiveHorns(bool enable)
	{
		g_ScriptAudioEntity.EnableAggressiveHorns(enable);
	}

	void CommandMuteGameWorldAudio(bool UNUSED_PARAM(mute))
	{
	}

	void CommandMutePositionedRadio(bool UNUSED_PARAM(mute))
	{
	}

	void CommandMuteGameWorldAndPositionedRadioForTV(bool mute)
	{
		g_ScriptAudioEntity.MuteGameWorldAndPositionedRadioForTV(mute);
	}

	void CommandDontAbortVehicleConversations(bool allowUpsideDown, bool allowOnFire)
	{
		g_ScriptAudioEntity.DontAbortCarConversations(allowUpsideDown, allowOnFire, false);
	}

	void CommandDontAbortVehicleConversationsNew(bool allowUpsideDown, bool allowOnFire, bool allowDrowning)
	{
		g_ScriptAudioEntity.DontAbortCarConversations(allowUpsideDown, allowOnFire, allowDrowning);
	}

	void CommandSetTrainAudioRolloff(int VehicleIndex, f32 rolloff)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		if(pVehicle)
		{
			if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN)
			{
				static_cast<audTrainAudioEntity*>(pVehicle->GetVehicleAudioEntity())->SetTrainRolloff(rolloff);
			}
		}
	}

	bool CommandIsStreamPlaying()
	{
		return g_ScriptAudioEntity.IsPlayingStream();
	}

	u32 CommandGetStreamPlayTime()
	{
		return g_ScriptAudioEntity.GetStreamPlayTime();
	}

	bool CommandPreloadStream(const char *streamName, const char *setName)
	{
		audDisplayf("LOAD_STREAM (stream %s, set %s) called from script %s", streamName, setName, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return g_ScriptAudioEntity.PreloadStream(streamName, 0, setName);
	}

	bool CommandPreloadStreamWithStartOffset(const char *streamName, int startoffset, const char *setName)
	{
		audDisplayf("LOAD_STREAM_WITH_START_OFFSET (stream %s, set %s, startoffset %d) called from script %s", streamName, setName, startoffset, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return g_ScriptAudioEntity.PreloadStream(streamName, (u32)startoffset, setName);
	}

	void CommandPlayStreamFrontend()
	{
		audDisplayf("PLAY_STREAM_FRONTEND called from script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		g_ScriptAudioEntity.PlayStreamFrontend();
	}
	void CommandPlayStreamFromPosition(const scrVector &pos)
	{
		Vector3 position = Vector3(pos);
		audDisplayf("PLAY_STREAM_FROM_PED called from script %s @ position (%.02f %02f %02f)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), position.GetX(), position.GetY(), position.GetZ());
		g_ScriptAudioEntity.PlayStreamFromPosition(position);
	}

	void CommandPlayStreamFromPed(int pedIndex)
	{
		CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		audDisplayf("PLAY_STREAM_FROM_PED called from script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(ped)
		{
			g_ScriptAudioEntity.PlayStreamFromEntity(ped);
		}
	}

	void CommandPlayStreamFromVehicle(int vehIndex)
	{
		CVehicle *veh = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		audDisplayf("PLAY_STREAM_FROM_VEHICLE called from script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(veh)
		{
			g_ScriptAudioEntity.PlayStreamFromEntity(veh);
		}
	}

	void CommandPlayStreamFromObject(int objectIndex)
	{
		audDisplayf("PLAY_STREAM_FROM_OBJECT called from script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NULL_IN_SCRIPTING_LANGUAGE != objectIndex)
		{
			CObject* object = CTheScripts::GetEntityToModifyFromGUID<CObject>(objectIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if(object)
			{
				g_ScriptAudioEntity.PlayStreamFromEntity(object);
			}	
		}
	}

	void CommandStopStream()
	{
		audDisplayf("STOP_STREAM called from script %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		g_ScriptAudioEntity.StopStream();
	}

	void CommandSetEndCreditsFade (bool Active )
	{
		if(Active)
		{
			g_DisableEndCreditsFade = false;
		}
		else
		{
			g_DisableEndCreditsFade = true;
		}
	}

	bool CommandIsGameInControlOfMusic()
	{
		return g_AudioEngine.IsGameInControlOfMusicPlayback();
	}

	void CommandStopPedSpeaking(int pedIndex, bool shouldDisable )
	{
		CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if(ped)
			{
				g_ScriptAudioEntity.StopPedSpeaking(ped, shouldDisable );
			}	
	}

	void CommandBlockAllSpeechFromPed(int pedIndex, bool shouldBlock, bool suppressOutgoingNetworkSpeech)
	{
		CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if (ped)
		{
			g_ScriptAudioEntity.BlockAllSpeechFromPed(ped, shouldBlock, suppressOutgoingNetworkSpeech);
		}
	}

	void CommandStopPedSpeakingSynced(int pedIndex, bool shouldDisable)
	{
		CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if (ped)
		{
			g_ScriptAudioEntity.StopPedSpeakingSynced(ped, shouldDisable );
		}
	}

	void CommandDisablePedPainAudio(int pedIndex, bool shouldDisable )
	{
		CPed *ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
		if(ped)
		{
			g_ScriptAudioEntity.StopPedPainAudio(ped, shouldDisable );
		}	
	}

	bool CommandIsAmbientSpeechDisabled(int pedIndex)
	{
		const CPed* ped = NULL;
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			ped = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
			if(ped)
			{
				return !ped->GetSpeechAudioEntity() || ped->GetSpeechAudioEntity()->IsSpeakingDisabled() || ped->GetSpeechAudioEntity()->IsSpeakingDisabledSynced();
			}
		}
		return false;
	}


	void CommandBlockSpeechContextGroup(const char* groupName, int target)
	{
		scrThreadId scriptThreadId =  CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		if(naVerifyf(scriptThreadId, "Failed to get thread id"))
			audSpeechAudioEntity::AddContextBlock(groupName, (audContextBlockTarget) target, scriptThreadId);
	}
	
	void CommandUnblockSpeechContextGroup(const char* groupName)
	{
		audSpeechAudioEntity::RemoveContextBlock(groupName);
	}

	void CommandSetIsBuddy(int pedIndex, bool isBuddy)
	{
		if (NULL_IN_SCRIPTING_LANGUAGE != pedIndex)
		{
			CPed* ped = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->SetPedIsBuddy(isBuddy);
			}
		}
	}

	void CommandSetSirenWithNoDriver(int VehicleIndex, bool SirenFlag)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
		
		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetForceSiren(SirenFlag);	
		}	
	}

	void CommandSetSirenBypassMPDriverCheck(int VehicleIndex, bool ShouldBypass)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "SET_SIREN_BYPASS_MP_DRIVER_CHECK - Vehicle is not a car"))
			{
				pVehicle->GetVehicleAudioEntity()->SetBypassMPDriverCheck(ShouldBypass);	
			}
		}	
	}

	void CommandTriggerSirenAudio(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetSirenOn();
		}
	}	

	void CommandSetAudioVehiclePriority(int VehicleIndex, int priority)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			g_ScriptAudioEntity.SetVehicleScriptPriorityForCurrentScript(pVehicle->GetVehicleAudioEntity(), (audVehicleScriptPriority)priority);
		}
	}

    void CommandSetHornEnabled(int VehicleIndex, bool enabled)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			pVehicle->GetVehicleAudioEntity()->SetHornEnabled(enabled);	
		}
	}

	void CommandSetHornPermanentlyOn(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			audDisplayf("SET_HORN_PERMANENTLY_ON - Script setting vehicle %s horn permanently on (%s)", pVehicle->GetModelName(), CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBoat() || pVehicle->GetType() == VEHICLE_TYPE_BIKE , "SET_HORN_PERMANENTLY_ON - Vehicle is not a car, boat or bicycle"))
			{
				pVehicle->GetVehicleAudioEntity()->SetHornPermanentlyOn(true);	
			}
		}
	}

	void CommandSetHornPermanentlyOnTime(int VehicleIndex, f32 hornTime)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			audDisplayf("SET_HORN_PERMANENTLY_ON_TIME - Script setting vehicle %s horn permanently on time to %.02fms (%s)", pVehicle->GetModelName(), hornTime, CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile() || pVehicle->InheritsFromBoat() || pVehicle->GetType() == VEHICLE_TYPE_BIKE , "SET_HORN_PERMANENTLY_ON_TIME - Vehicle is not a car, boat or bicylce"))
			{
				pVehicle->GetVehicleAudioEntity()->PlayHeldDownHorn(hornTime/1000.f, true);	
			}
		}
	}
	void CommandUseSireAsHorn(int VehicleIndex, bool sirenAsHorn)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "USE_SIREN_AS_HORN - Vehicle is not a car"))
			{
				pVehicle->GetVehicleAudioEntity()->UseSireAsHorn(sirenAsHorn);	
			}
		}
	}

	void CommandEnableVehicleExhaustPops(int VehicleIndex, bool enable)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromAutomobile(), "ENABLE_VEHICLE_EXHAUST_POPS - Vehicle is not a car"))
			{
				if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
				{
					audCarAudioEntity* carEntity = static_cast<audCarAudioEntity*>(pVehicle->GetVehicleAudioEntity());
					carEntity->SetExhaustPopsEnabled(enable);
				}
			}
		}
	}

	void CommandSetVehicleBoostActive(int VehicleIndex, bool active)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR || pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI, "SET_VEHICLE_BOOST_ACTIVE - Vehicle is not a car or a heli"))
			{
				if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
				{
					audCarAudioEntity* carEntity = static_cast<audCarAudioEntity*>(pVehicle->GetVehicleAudioEntity());
					carEntity->SetBoostActive(active);
				}
				else if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
				{
					audHeliAudioEntity* heliEntity = static_cast<audHeliAudioEntity*>(pVehicle->GetVehicleAudioEntity());
					heliEntity->EnableCrazyHeliSounds(active);
				}
			}
		}
	}

	void CommandSetPlayerVehicleAlarmActive(int VehicleIndex, bool active)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetPlayerAlarmBehaviour(active);
		}
	}

	void CommandScriptUpdateDoorAudio(int doorEnumHash, bool update)
	{
		CDoorSystemData *pDoorData = CDoorSystem::GetDoorData(doorEnumHash);

		SCRIPT_ASSERT(pDoorData, "Door hash supplied failed find a door");

		if (pDoorData)
		{
			CDoor *pDoor = pDoorData->GetDoor();
			if (pDoor && pDoor->GetDoorAudioEntity())
			{
				pDoor->GetDoorAudioEntity()->SetScriptUpdate(update);
			}
		}
	}

	void CommandPlayVehicleDoorCloseSound(int VehicleIndex, int doorIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(doorIndex < pVehicle->GetNumDoors(), "PLAY_DOOR_CLOSE_SOUND - Invalid door specified"))
			{
				CCarDoor *door = pVehicle->GetDoor(doorIndex);

				if(door && (door->GetFlag(CCarDoor::IS_ARTICULATED) || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)) 
				{
					pVehicle->GetVehicleAudioEntity()->TriggerDoorCloseSound(door->GetHierarchyId(), false);
				}
			}
		}
	}

	void CommandPlayVehicleDoorOpenSound(int VehicleIndex, int doorIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(doorIndex < pVehicle->GetNumDoors(), "PLAY_DOOR_OPEN_SOUND - Invalid door specified"))
			{
				CCarDoor *door = pVehicle->GetDoor(doorIndex);

				if(door && (door->GetFlag(CCarDoor::IS_ARTICULATED) || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)) 
				{
					pVehicle->GetVehicleAudioEntity()->TriggerDoorOpenSound(door->GetHierarchyId());
				}
			}
		}
	}

	void CommandEnableVehicleCropDustSounds(int VehicleIndex, bool enable)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "ENABLE_CROP_DUSTING_SOUNDS - Vehicle is not a plane"))
			{
				if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
				{
					audPlaneAudioEntity* planeEntity = static_cast<audPlaneAudioEntity*>(pVehicle->GetVehicleAudioEntity());
					planeEntity->SetCropDustingActive(enable);
				}
			}
		}
	}

	void CommandEnableVehicleStallWarningSounds(int VehicleIndex, bool enable)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			if(SCRIPT_VERIFY(pVehicle->InheritsFromPlane(), "ENABLE_STALL_WARNING_SOUNDS - Vehicle is not a plane"))
			{
				if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
				{
					audPlaneAudioEntity* planeEntity = static_cast<audPlaneAudioEntity*>(pVehicle->GetVehicleAudioEntity());
					planeEntity->SetStallWarningEnabled(enable);
				}
			}
		}
	}

	void CommandDisableGps(bool bEnable)
	{
		bEnable = !bEnable;
		CGps::TemporarilyDisableAudio(bEnable);
	}

	void CommandTriggerMissionComplete(const char *  MissionCompleteId)
	{
		g_ScriptAudioEntity.TriggerMissionComplete(MissionCompleteId);
	}

	bool CommandIsMissionCompletePlaying()
	{
		return g_ScriptAudioEntity.IsPlayingMissionComplete();
	}

	bool CommandMissionCompleteReadyForUI()
	{
		return g_ScriptAudioEntity.IsMissionCompleteReadyForUI();
	}

	void CommandBlockDeathJingle(bool blocked)
	{
		g_ScriptAudioEntity.BlockDeathJingle(blocked);
	}

	bool CommandIsDeathJingleBlocked()
	{
		return !g_ScriptAudioEntity.CanPlayDeathJingle();
	}

	void CommandSetRomansMood(int Mood)
	{
		g_RomansMood = Mood;
	}

	void CommandSetBriansMood(int Mood)
	{
		g_BriansMood = Mood;
	}

	void CommandSetInNetworkLobby(const bool isInNetworkLobby)
	{
		audNorthAudioEngine::SetInNetworkLobby(isInNetworkLobby);
	}

	void CommandSetLobbyMuteOverride(const bool overrideMute)
	{
		audNorthAudioEngine::SetLobbyMuteOverride(overrideMute);
	}

	void CommandAddBrokenGlassArea(const scrVector &centrePosition, f32 areaRadius)
	{
		Vec3V pos = Vec3V(centrePosition);
		g_GlassAudioEntity.ScriptHandleBreakableGlassEvent(pos,areaRadius);
	}
	void CommandClearAllGlassArea()
	{
		g_GlassAudioEntity.ClearAll(true);
	}

	bool CommandIsMusicPlaying() { return g_InteractiveMusicManager.IsMusicPlaying(); }
	
	// GTA V Music Interface
	bool CommandPrepareMusicEvent(const char *eventName)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(naVerifyf(script, "Failed to register script with audio"))
		{
			audMusicAction::PrepareState prepareState = g_InteractiveMusicManager.GetEventManager().PrepareEvent(eventName, static_cast<u32>(script->ScriptThreadId));
			naAssertf(prepareState != audMusicAction::PREPARE_FAILED, "Failed to prepare music event %s for script %s", eventName, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return (prepareState == audMusicAction::PREPARED);
		}
		return false;
	}

	bool CommandCancelMusicEvent(const char *eventName)
	{
		audDisplayf("%f - CANCEL_MUSIC_EVENT: %s", fwTimer::GetTimeInMilliseconds() * 0.001f, eventName);
		return g_InteractiveMusicManager.GetEventManager().CancelEvent(eventName);
	}

	bool CommandTriggerMusicEvent(const char *eventName)
	{
		audDisplayf("%f - TRIGGER_MUSIC_EVENT: %s", fwTimer::GetTimeInMilliseconds() * 0.001f, eventName);
		return g_InteractiveMusicManager.GetEventManager().TriggerEvent(eventName);
	}

	bool CommandIsMusicOneshotPlaying()
	{
		return g_InteractiveMusicManager.IsOneshotActive();
	}

	bool CommandIsScoreMutedForRadio()
	{
		return g_InteractiveMusicManager.IsScoreMutedForRadio();
	}

	s32 CommandGetMusicPlayTime()
	{
		return g_InteractiveMusicManager.GetCurPlayTimeMs();
	}

	void CommandSetGlobalRadioSignalLevel(f32 signalLevel)
	{
		audRadioStation::SetScriptGlobalRadioSignalLevel(signalLevel);
	}

	//Dynamic mixing commands 

	bool CommandStartAudioScene(const char * sceneName)
	{
#if __ASSERT
		MixerScene * scene = DYNAMICMIXMGR.GetObject<MixerScene>(atStringHash(sceneName));
		naAssertf(scene, "SCRIPT: Attempting to start audio scene %s but there's no object with this name", sceneName);
#endif
		return g_ScriptAudioEntity.StartScene(atStringHash(sceneName)); 
	}

	void CommandStopAudioScene(const char * sceneName)
	{
		g_ScriptAudioEntity.StopScene(atStringHash(sceneName));
	}


	void CommandStopAudioScenes()
	{
		g_ScriptAudioEntity.StopScenes();
	}

	bool CommandIsSceneActive(const char * sceneName)
	{
		return sceneName && g_ScriptAudioEntity.IsSceneActive(atStringHash(sceneName));
	}

	void CommandSetAudioSceneVariable(const char * sceneName, const char * variableName, float value)
	{
		g_ScriptAudioEntity.SetSceneVariable(sceneName, variableName, value);
	}

	bool CommandHasSceneFinishedFade(const char * sceneName)
	{
		return g_ScriptAudioEntity.SceneHasFinishedFading(atStringHash(sceneName));
	}

#if NA_POLICESCANNER_ENABLED
	void CommandSetPoliceScannerAudioScene(const char * sceneName)
	{
		g_ScriptAudioEntity.SetPoliceScannerAudioScene(sceneName);
	}

	void CommandStopAndRemovePoliceScannerAudioScene()
	{
		g_ScriptAudioEntity.StopAndRemovePoliceScannerAudioScene();
	}
#endif

	void CommandSetAudioScriptVariable(const char * variableName, float value) 
	{
		g_ScriptAudioEntity.SetScriptVariable(variableName, value);
	}

	void CommandAddEntityToMixGroup(int entityIndex, const char *groupName, float fadeIn)
	{  
		CEntity* entity = NULL;
		if(SCRIPT_VERIFY(NULL_IN_SCRIPTING_LANGUAGE != entityIndex, "ADD_ENTITY_TO_AUDIO_MIX_GROUP - trying to add a null entity"))
		{
			entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
			if(!entity)
			{
				return;
			}
			if(fadeIn > 0.f)
			{
				DYNAMICMIXER.AddEntityToMixGroup(entity, atStringHash(groupName), fadeIn);
			}
			else
			{
				DYNAMICMIXER.AddEntityToMixGroup(entity, atStringHash(groupName));
			}
		}
	}

	void CommandRemoveEntityFromMixGroup(int entityIndex, float fadeOut)
	{
		CEntity* entity = NULL;
		if(SCRIPT_VERIFY(NULL_IN_SCRIPTING_LANGUAGE != entityIndex, "REMOVE_ENTITY_FROM_AUDIO_MIX_GROUP - trying to remove a null entity"))
		{
			entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
			if(!entity)
			{
				return;
			}

			if(fadeOut > 0.f)
			{
				DYNAMICMIXER.RemoveEntityFromMixGroup(entity, fadeOut);
			}
			else
			{
				DYNAMICMIXER.RemoveEntityFromMixGroup(entity);
			}
		}
	}

	void CommandEnableStuntJump()
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			ConductorMessageData messageData;
			messageData.conductorName = VehicleConductor;
			messageData.message = StuntJump;
			messageData.bExtraInfo = true;
			SUPERCONDUCTOR.SendConductorMessage(messageData);
		}
	}
	

	void CommandPlayFakeDistantSiren(bool play)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->SetAudioFlag(audScriptAudioFlags::ScriptPlayingDistantSiren, play);
			ConductorMessageData messageData;
			messageData.conductorName = VehicleConductor;
			messageData.message = ScriptFakeSirens;
			messageData.bExtraInfo = play;
			SUPERCONDUCTOR.SendConductorMessage(messageData);
		}
		
	}
	void CommandSirenControlledByAudio(int vehIndex,bool controlled)
	{
		CVehicle *pVehicle;
		pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);
		if (pVehicle && pVehicle->GetAudioEntity())
		{
			pVehicle->GetVehicleAudioEntity()->SetSirenControlledByConductors(controlled);
			pVehicle->GetVehicleAudioEntity()->SetMustPlaySiren(true);
			pVehicle->GetVehicleAudioEntity()->SmoothSirenVolume(1/2000.f,1.f);
		}

	}

#if __BANK
	void CommandDrawGunFightGrid()
	{
		Matrix34 g_ListenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
		const u32 sectorIndex = static_cast<u32>(audNorthAudioEngine::GetGtaEnvironment()->ComputeWorldSectorIndex(g_ListenerMatrix.d));
		const u32 sectorX = sectorIndex % g_audNumWorldSectorsX, sectorY = (sectorIndex-sectorX)/g_audNumWorldSectorsY;
		// Work out where the centre of the main sector is, so we know how much of the others to factor in
		f32 mainSectorX = g_audWidthOfSector * (sectorX - (g_fAudNumWorldSectorsX/2.f) + 0.5f);
		f32 mainSectorY = g_audDepthOfSector * (sectorY - (g_fAudNumWorldSectorsY/2.f) + 0.5f);
		Vector3 origin,gridControl; 
		origin.SetX(mainSectorX- g_audWidthOfSector * ((AUD_NUM_SECTORS_ACROSS/2.f)));
		origin.SetY(mainSectorY- g_audWidthOfSector * ((AUD_NUM_SECTORS_ACROSS/2.f)));
		origin.SetZ(15.f);
		gridControl = origin;
		if(audNorthAudioEngine::GetGunFireAudioEntity()->g_DrawOverallGrid){
			for(u32 i = 0 ; i <= AUD_NUM_SECTORS_ACROSS ; ++i)
			{
				grcDebugDraw::Line(gridControl,gridControl+ Vector3(AUD_NUM_SECTORS_ACROSS * g_audWidthOfSector,0.f,0.f), Color_red,1);
				gridControl.SetY(gridControl.GetY() + g_audWidthOfSector);
			}
			gridControl = origin;
			for(u32 i = 0 ; i <= AUD_NUM_SECTORS_ACROSS ; ++i)
			{
				grcDebugDraw::Line(gridControl,gridControl+ Vector3(0.f,AUD_NUM_SECTORS_ACROSS * g_audWidthOfSector,0.f), Color_red,1);
				gridControl.SetX(gridControl.GetX() + g_audWidthOfSector);
			}
		}
		audNorthAudioEngine::GetGunFireAudioEntity()->InitGunFireGrid();
		if(audNorthAudioEngine::GetGunFireAudioEntity()->g_DrawGunFightGrid){
			for(u32 i = 0 ; i <= AUD_NUM_SECTORS_ACROSS ; ++i)
			{
				gridControl = audNorthAudioEngine::GetGunFireAudioEntity()->GetGunFireSectorsCoords(i).xCoord;
				gridControl.SetZ(15.f);
				grcDebugDraw::Line(gridControl,gridControl + Vector3(AUD_NUM_SECTORS_ACROSS * g_audWidthOfSector/audNorthAudioEngine::GetGunFireAudioEntity()->g_Definition,0.f,0.f), Color_blue,1);

				gridControl = audNorthAudioEngine::GetGunFireAudioEntity()->GetGunFireSectorsCoords(i).yCoord;
				gridControl.SetZ(15.f);
				grcDebugDraw::Line(gridControl,gridControl - Vector3(0.f,AUD_NUM_SECTORS_ACROSS * g_audWidthOfSector/audNorthAudioEngine::GetGunFireAudioEntity()->g_Definition,0.f), Color_blue,1);
			}
		}

	} 
	scrVector CommandShooterPosInSector(int x,int y)
	{ 
		float xx,yy; 
		xx = CRandomScripts::GetRandomNumberInRange( audNorthAudioEngine::GetGunFireAudioEntity()->GetGunFireSectorsCoords(y).yCoord.x
													,audNorthAudioEngine::GetGunFireAudioEntity()->GetGunFireSectorsCoords(y+1).yCoord.x);
		yy = CRandomScripts::GetRandomNumberInRange( audNorthAudioEngine::GetGunFireAudioEntity()->GetGunFireSectorsCoords(x+1).xCoord.y
													,audNorthAudioEngine::GetGunFireAudioEntity()->GetGunFireSectorsCoords(x).xCoord.y);
		Vector3 result;
		result.x = xx;
		result.y = yy;
		result.z = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition()).z;
		return result;
	}
#endif
	void CommandSetWindTemperature(f32 /*temperature*/,f32 /*applyValue*/)
	{ 
		//g_WeatherAudioEntity.SetWindTemperature(temperature,applyValue);
	}
	void CommandSetWindStrength(f32 /*strength*/,f32 /*applyValue*/)
	{ 
		//g_WeatherAudioEntity.SetWindStrength(strength,applyValue); 
	}	
	void CommandSetWindBlustery(f32 /*blustery*/,f32 /*applyValue*/)
	{ 
		//g_WeatherAudioEntity.SetWindBlustery(blustery,applyValue);
	}
	void CommandScriptOverrideWindTypes(bool override,const char *oldWindTypeName,const char *newWindTypeName)
	{ 
		g_WeatherAudioEntity.OverrideWindType(override,oldWindTypeName,newWindTypeName);
	}

	void CommandScriptOverrideWindElevation(bool override,const int windElevationHashName)
	{ 
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->SetAudioFlag(audScriptAudioFlags::OverrideElevationWind, override);
			g_WeatherAudioEntity.SetScriptWindElevation(windElevationHashName);
		}
	}

	void CommandSetPedWallaDensity(f32 density,f32 applyValue)
	{ 
		g_ScriptAudioEntity.SetDesiredPedWallaDensity(density, applyValue, false);
	}

	void CommandSetInteriorPedWallaDensity(f32 density,f32 applyValue)
	{
		g_ScriptAudioEntity.SetDesiredPedWallaDensity(density, applyValue, true);
	}

	void CommandSetPedWallaSoundSet(const char* soundSet, bool isInterior)
	{
		g_ScriptAudioEntity.SetDesiredPedWallaSoundSet(soundSet, isInterior);
	}

	void CommandForcePedPanic()
	{ 
		g_AmbientAudioEntity.ForcePedPanic();
	}

	bool CommandPrepareAlarm(const char* alarmName)
	{
		return g_AmbientAudioEntity.PrepareAlarm(alarmName);
	}

	void CommandStartAlarm(const char* alarmName, bool skipStartup)
	{
		g_AmbientAudioEntity.StartAlarm(alarmName, skipStartup);
	}

	void CommandStopAlarm(const char* alarmName, bool stopInstantly)
	{
		g_AmbientAudioEntity.StopAlarm(alarmName, stopInstantly);
	}

	void CommandStopAllAlarms(bool stopInstantly)
	{
		g_AmbientAudioEntity.StopAllAlarms(stopInstantly);
	}

	bool CommandIsAlarmPlaying(const char* alarmName)
	{
		AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmName);
		Assertf(alarmSettings, "Could not find alarm named %s", alarmName);

		if(alarmSettings)
		{
			return g_AmbientAudioEntity.IsAlarmActive(alarmSettings);
		}

		return false;
	}

	int CommandGetVehicleDefaultHorn(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			return (int)pVehicle->GetVehicleAudioEntity()->GetVehicleHornSoundHash();
		}
		return 0;
	}

	int CommandGetVehicleDefaultHornIgnoreMods(int VehicleIndex)
	{
		CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pVehicle && pVehicle->GetVehicleAudioEntity())
		{
			return (int)pVehicle->GetVehicleAudioEntity()->GetVehicleHornSoundHash(true);
		}
		return 0;
	}

	void CommandUpdatePedVariations(int /*PedIndex*/)
	{
// 		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
// 		if(pPed && pPed->GetPedAudioEntity())
// 		{
// 			pPed->GetPedAudioEntity()->GetFootStepAudio().UpdateVariationData();
// 		}
	}

	void CommandResetPedAudioFlags(int PedIndex)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed && pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().ResetWetFeet(true);
		}
	}

	void CommandSetPedFootstepMode(int PedIndex, bool scriptOverrides, int modeHash)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if(pPed && pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().SetFootstepTuningMode(scriptOverrides,modeHash);
		}
	}

	void CommandSetForcePedWearingScubaMask(int PedIndex, bool force)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPedAudioEntity())
		{
			audDisplayf("%s is forcing scuba mask on ped %d (from script %s)", force ? "Enabling" : "Disabling", PedIndex, CTheScripts::GetCurrentScriptNameAndProgramCounter());
			pPed->GetPedAudioEntity()->GetFootStepAudio().SetForceWearingScubaMask(force);
		}
	}

	void CommandSetPedFootstepEventsEnabled(int PedIndex, bool enabled)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().SetFootstepEventsEnabled(enabled);
		}
	}

	void CommandSetPedClothEventsEnabled(int PedIndex, bool enabled)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed && pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().SetClothEventsEnabled(enabled);
		}
	}

	void CommandOverridePlayerMaterial(int overriddenMaterialHash, bool scriptOverrides)
	{
		CPed *pPed = CGameWorld::FindLocalPlayer();
		if(pPed && pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().OverridePlayerMaterial(overriddenMaterialHash);
			audScript *script = g_ScriptAudioEntity.GetScript();
			if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "OverridePlayerGroundMaterial", CTheScripts::GetCurrentScriptName()))
			{
				script->SetAudioFlag("OverridePlayerGroundMaterial", scriptOverrides);
			}
		}
	}

	void CommandUseFoostepScriptSweeteners(int PedIndex, bool use,const int soundSetHash)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
		if(pPed && pPed->GetPedAudioEntity())
		{
			if(use && !pPed->GetPedAudioEntity()->GetFootStepAudio().ShouldScriptAddSweetenersEvents())
			{
				pPed->GetPedAudioEntity()->GetFootStepAudio().SetScriptSweetenerSoundSet(soundSetHash);
			}
			else if (!use)
			{
				pPed->GetPedAudioEntity()->GetFootStepAudio().ResetScriptSweetenerSoundSet();
			}
			else
			{
				scriptAssertf(false, "Script %s is calling USE_FOOTSTEP_SCRIPT_SWEETENERS more than once, if you want to use a different soundset, first call the command with false.", CTheScripts::GetCurrentScriptName());
			}
		}
	}
	void CommandFreezeMicrophone()
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "OverrideMicrophoneSettings", CTheScripts::GetCurrentScriptName()))
		{
			audNorthAudioEngine::GetMicrophones().FreezeMicrophone();
		}
	}
	

	void CommandOverrideMicrophoneSettings(int overriddenMicSettingsHash, bool scriptOverrides)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "OverrideMicrophoneSettings", CTheScripts::GetCurrentScriptName()))
		{
			audNorthAudioEngine::GetMicrophones().SetScriptMicSettings(overriddenMicSettingsHash);
			script->SetAudioFlag("OverrideMicrophoneSettings", scriptOverrides);
		}
	}
	void CommandSetSniperAudioDisable(bool enable)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "DisableSniperAudio", CTheScripts::GetCurrentScriptName()))
		{
			script->SetAudioFlag("DisableSniperAudio", enable);
		}
	}

	class CSyncedSceneAudioInterface : public fwSyncedSceneAudioInterface
	{
	public:

		// Constructor

		CSyncedSceneAudioInterface();

		// Operations

		virtual bool Prepare(const char *audioEvent);
		virtual u32 Play(u32 audioHash);
		virtual void Stop(u32 audioHash);
		virtual void Skip(u32 audioHash, u32 skipTimeMs);

		// Properties

		virtual u32 GetElapsedTimeMs(u32 audioHash) const;
		virtual u32 GetDurationMs(u32 audioHash) const;
	};

	CSyncedSceneAudioInterface g_SyncedSceneAudioInterface;

	// Constructor

	CSyncedSceneAudioInterface::CSyncedSceneAudioInterface()
	{
	}

	// Operations

	bool CSyncedSceneAudioInterface::Prepare(const char *audioEvent)
	{
		if(Verifyf(audioEvent, ""))
		{
			if(g_CutsceneAudioEntity.IsSyncSceneTrackPrepared(audioEvent))
			{
				return true;
			}
		}

		return false;
	}

	u32 CSyncedSceneAudioInterface::Play(u32 audioHash)
	{
		if(audioHash != 0)
		{
			g_CutsceneAudioEntity.TriggerSyncScene(audioHash);

			return audioHash;
		}
		else
		{
			return g_CutsceneAudioEntity.TriggerSyncScene();
		}
	}

	void CSyncedSceneAudioInterface::Stop(u32 audioHash)
	{
		if(Verifyf(audioHash != 0, ""))
		{
			g_CutsceneAudioEntity.StopSyncScene(audioHash);
		}
	}

	void CSyncedSceneAudioInterface::Skip(u32 audioHash, u32 skipTimeMs)
	{
		if(Verifyf(audioHash != 0, ""))
		{
			g_CutsceneAudioEntity.SkipSynchedSceneToTime(audioHash, skipTimeMs);
		}
	}

	u32 CSyncedSceneAudioInterface::GetElapsedTimeMs(u32 audioHash) const 
	{
		if(Verifyf(audioHash != 0, ""))
		{
			return g_CutsceneAudioEntity.GetSyncSceneTime(audioHash);
		}

		return (u32)-1;
	}

	u32 CSyncedSceneAudioInterface::GetDurationMs(u32 audioHash) const
	{
		if(Verifyf(audioHash != 0, ""))
		{
			return g_CutsceneAudioEntity.GetSyncSceneDuration(audioHash);
		}

		return (u32)-1;
	}

	bool CommandPrepareSynchronizedAudioEvent(const char *audioEvent, int UNUSED_PARAM(startTimeMs))
	{
		return fwAnimDirectorComponentSyncedScene::PrepareSynchronizedSceneAudioEvent(audioEvent);
	}

	bool CommandPrepareSynchronizedAudioEventForScene(int iSceneId, const char *audioEvent)
	{
		fwSyncedSceneId sceneId = static_cast< fwSyncedSceneId >(iSceneId);
		if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneId))
		{
			return fwAnimDirectorComponentSyncedScene::PrepareSynchronizedSceneAudioEvent(sceneId, audioEvent);
		}
		else
		{
			scriptAssertf(false, "%s: PREPARE_SYNCHRONIZED_AUDIO_EVENT_FOR_SCENE - Invalid scene ID! %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iSceneId);
		}

		return false;
	}

	bool CommandPlaySynchronizedAudioEvent(int iSceneId)
	{
		fwSyncedSceneId sceneId = static_cast< fwSyncedSceneId >(iSceneId);
		if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneId))
		{
			return fwAnimDirectorComponentSyncedScene::PlaySynchronizedSceneAudioEvent(sceneId);
		}
		else
		{
			scriptAssertf(false, "%s: PLAY_SYNCHRONIZED_AUDIO_EVENT - Invalid scene ID! %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iSceneId);
		}

		return false;
	}

	bool CommandStopSynchronizedAudioEvent(int iSceneId)
	{
		fwSyncedSceneId sceneId = static_cast< fwSyncedSceneId >(iSceneId);
		if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneId))
		{
			return fwAnimDirectorComponentSyncedScene::StopSynchronizedSceneAudioEvent(sceneId);
		}
		else
		{
			scriptAssertf(false, "%s: STOP_SYNCHRONIZED_AUDIO_EVENT - Invalid scene ID! %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iSceneId);
		}

		return false;
	}

	void CommandInitSynchronizedSceneAudioWithPosition(const char * audioEvent, const scrVector &pos)
	{
		g_CutsceneAudioEntity.InitSynchedScene(audioEvent);
		g_CutsceneAudioEntity.SetScenePlayPosition(atStringHash(audioEvent), pos);
	}

	void CommandInitSynchronizedSceneAudioWithEntity(const char * audioEvent, int entityIndex)
	{
		CEntity * entity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex);
		if(entity)
		{
			g_CutsceneAudioEntity.InitSynchedScene(audioEvent);
			g_CutsceneAudioEntity.SetScenePlayEntity(atStringHash(audioEvent), entity);
		}
	}

	void CommandSetSynchSceneAudioPosition(const scrVector &UNUSED_PARAM(pos))
	{
	}

	void CommandSetSynchSceneEntity(int UNUSED_PARAM(entityIndex))
	{
	
	}

	void CommandActivateSlowMoMode(const char * mode)
	{
		audDisplayf("Script %s activating slow mo mode %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), mode);
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->ActivateSlowMoMode(mode);
		}
	}

	void CommandDeactivateSlowMoMode(const char * mode)
	{
		audDisplayf("Script %s deactivating slow mo mode %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), mode);
		audScript *script = g_ScriptAudioEntity.GetScript();
		if(audVerifyf(script, "Failed to register script with audio"))
		{
			script->DeactivateSlowMoMode(mode);
		}		
	}

	void CommandSetAudioSpecialEffectMode(int mode)
	{
		audNorthAudioEngine::GetGtaEnvironment()->SetScriptedSpecialEffectMode((audSpecialEffectMode)mode);		
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && mode != kSpecialEffectModeNormal)
		{
			CReplayMgr::RecordFx<CPacketScriptAudioSpecialEffectMode>(CPacketScriptAudioSpecialEffectMode((audSpecialEffectMode)mode));
		}
#endif
		audWarningf("Script %s setting special effect mode %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), mode);
	}

	void CommandSetPortalSettingsOverride(const char* oldPortalSettingsName, const char* newPortalSettingsName)
	{
		naOcclusionPortalInfo::SetPortalSettingsScriptOverride(oldPortalSettingsName, newPortalSettingsName);
	}

	void CommandSetPerPortalSettingsOverride(int interiorNameHash, int roomIndex, int doorIndex, const char* newPortalSettingsName)
	{
		naOcclusionPortalInfo::SetPortalSettingsScriptOverride(interiorNameHash, roomIndex, doorIndex, newPortalSettingsName);
	}

	void CommandRemovePortalSettingsOverride(const char* portalSettingsName)
	{
		naOcclusionPortalInfo::RemovePortalSettingsScriptOverride(portalSettingsName);
	}

	void CommandRemovePerPortalSettingsOverride(int interiorNameHash, int roomIndex, int doorIndex)
	{
		naOcclusionPortalInfo::RemovePortalSettingsScriptOverride(interiorNameHash, roomIndex, doorIndex);
	}

	void CommandStopSmokeGrenadeExplosionSounds()
	{
		audExplosionAudioEntity::StopSmokeGrenadeExplosions();
	}

	void CommandTriggerAircraftWarningSpeech(int warningType)
	{
		if(SCRIPT_VERIFY(warningType >= 0 && warningType < AUD_AW_NUM_WARNINGS, "Bad aircraft warning type passed in to TRIGGER_AIRCRAFT_WARNING_SPEECH"))
		{
			g_ScriptAudioEntity.TriggerAircraftWarningSpeech((audAircraftWarningTypes) warningType);
		}
	}

	int CommandGetSfxVolSlider()
	{
		return CPauseMenu::GetMenuPreference(PREF_SFX_VOLUME);
	}

	int CommandGetMusicVolSlider()
	{
		const eMenuPref c_mpMusicVolumePref = RSG_GEN9 ? PREF_MUSIC_VOLUME : PREF_MUSIC_VOLUME_IN_MP;
		return CNetwork::IsGameInProgress() ? CPauseMenu::GetMenuPreference(c_mpMusicVolumePref)
											: CPauseMenu::GetMenuPreference(PREF_MUSIC_VOLUME);
	}

	bool CommandGetNextAudibleBeat(float &timeS, float &bpm, s32 &beatNumber)
	{
		return g_RadioAudioEntity.GetCachedNextAudibleBeat(timeS, bpm, beatNumber);
	}

	bool CommandGetNextBeatForStation(const char *stationName, float &timeS, float &bpm, s32 &beatNumber)
	{
		const audRadioStation *station = audRadioStation::FindStation(stationName);
		if(audVerifyf(station, "Failed to find radio station %s", stationName))
		{
			return station->GetCurrentTrack().GetNextBeat(timeS, bpm, beatNumber);
		}
		return false;
	}

	s32 CommandGetCurrentTrackPlayTime(const char* stationName)
	{
		const audRadioStation *station = audRadioStation::FindStation(stationName);
		if (audVerifyf(station, "Failed to find radio station %s", stationName))
		{
			return station->GetCurrentTrack().GetPlayTime();
		}

		return 0u;
	}

	void CommandStopCutsceneAudio()
	{
		g_CutsceneAudioEntity.StopOverunAudio();
	}

#if AUD_MUSIC_STUDIO_PLAYER
	bool CommandMusicStudioLoadSession(const char *sessionName)
	{
		return g_FrontendAudioEntity.GetMusicStudioSessionPlayer().LoadSession(sessionName);
	}

	void CommandMusicStudioRequestSectionTransition(const s32 sectionIndex)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().RequestSectionTransition(sectionIndex);
	}

	void CommandMusicStudioStartPlayback()
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().StartPlayback();
	}

	void CommandMusicStudioStopPlayback()
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().StopPlayback();
	}

	void CommandMusicStudioSetPartVariationIndex(const s32 sectionIndex, const s32 partIndex, const s32 variationIndex)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartVariationIndex(sectionIndex, partIndex, variationIndex);
	}

	void CommandMusicStudioSetPartVolume(const s32 sectionIndex, const s32 partIndex, const float volumeDb)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartVolume(sectionIndex, partIndex, volumeDb);
	}

	void CommandMusicStudioUnloadSession()
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().UnloadSession();
	}

	void CommandMusicStudioSetPartHPF(const s32 sectionIndex, const s32 partIndex, const bool enableFilter)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartHPF(sectionIndex, partIndex, enableFilter);
	}

	void CommandMusicStudioSetPartLPF(const s32 sectionIndex, const s32 partIndex, const bool enableFilter)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetPartLPF(sectionIndex, partIndex, enableFilter);
	}

	void CommandMusicStudioGetTimingInfo(float &nextBeatTimeS, float &bpm, s32 &beatNum, float &timeUntilNextSectionTransitionS)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().GetTimingInfo(nextBeatTimeS, bpm, beatNum, timeUntilNextSectionTransitionS);
	}

	void CommandMusicStudioRequestDelayedSettings()
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().RequestDelayedSettings();
	}

	bool CommandMusicStudioIsSessionPlaying()
	{
		return g_FrontendAudioEntity.GetMusicStudioSessionPlayer().IsPlaying();
	}

	s32 CommandMusicStudioGetActiveSection()
	{
		return g_FrontendAudioEntity.GetMusicStudioSessionPlayer().GetActiveSection();
	}

	s32 CommandMusicStudioGetLoadedSessionHash()
	{
		return (s32)g_FrontendAudioEntity.GetMusicStudioSessionPlayer().GetLoadedSessionHash();
	}

	void CommandMusicStudioSetSoloState(const bool solo1, const bool solo2, const bool solo3, const bool solo4)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetSoloState(solo1, solo2, solo3, solo4);
	}

	void CommandMusicStudioSetMuteState(const bool mute)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetMuteState(mute);
	}

	void CommandMusicStudioSetOcclusionEnabled(const bool enabled)
	{
		g_FrontendAudioEntity.GetMusicStudioSessionPlayer().SetOcclusionEnabled(enabled);
	}
#endif // AUD_MUSIC_STUDIO_PLAYER

	// Register commands
	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(PLAY_PED_RINGTONE,0x1e3bdc4ee6f896d7, CommandPlayPedRingtone);
		SCR_REGISTER_SECURE(IS_PED_RINGTONE_PLAYING,0xa626788ab8ad0c3b, CommandIsPedRingtonePlaying);
		SCR_REGISTER_SECURE(STOP_PED_RINGTONE,0x1697786d4c3afedd, CommandStopPedRingtone);
		SCR_REGISTER_UNUSED(CREATE_NEW_MOBILE_PHONE_CALL,0xc0628219e2ceecca, CommandNewMobilePhoneCall);
		SCR_REGISTER_UNUSED(ADD_LINE_TO_MOBILE_PHONE_CALL,0x415a44b6b08ba3bd, CommandAddLineToMobilePhoneCall);
		SCR_REGISTER_UNUSED(START_MOBILE_PHONE_CALL,0xbf4ee5de6a77b5c0, CommandStartMobilePhoneCall);
		SCR_REGISTER_UNUSED(PRELOAD_MOBILE_PHONE_CALL,0xdc24accf6822b4f4, CommandPreloadMobilePhoneCall);
		SCR_REGISTER_SECURE(IS_MOBILE_PHONE_CALL_ONGOING,0xa3329b7a7520670e, CommandIsMobilePhoneCallOngoing);
		SCR_REGISTER_SECURE(IS_MOBILE_INTERFERENCE_ACTIVE,0x1b34b424d95b4726, CommandIsMobilePhoneInterferenceActive);
		SCR_REGISTER_UNUSED(GET_LAUGHTER_TRACK_MARKER_TIME,0xf2103de4d7d70cb6, CommandGetLaughterTrackMarkerTime);
		SCR_REGISTER_UNUSED(GET_NEXT_LAUGHTER_TRACK_MARKER_INDEX,0x2f4411c87dde1a26, CommandGetNextLaughterTrackMarkerIndex);
		SCR_REGISTER_UNUSED(GET_TIME_UNTIL_LAUGHTER_TRACK_MARKER,0x9c5b3a5cc7f42414, CommandGetTimeUntilLaughterTrackMarker);
		SCR_REGISTER_UNUSED(GET_LAUGHTER_TRACK_MARKER_CONTEXT,0xe14d445f73159e5a, CommandGetLaughterTrackMarkerContext);
		SCR_REGISTER_UNUSED(GET_CURRENT_TV_SHOW_PLAY_TIME,0x0afc20d9778b66d8, CommandGetCurrentTVShowPlayTime);
		SCR_REGISTER_UNUSED(GET_CURRENT_TV_SHOW_DURATION,0x35435ad4c9024be3, CommandGetCurrentTVShowDuration);
		SCR_REGISTER_SECURE(CREATE_NEW_SCRIPTED_CONVERSATION,0x59b217a592638f61, CommandNewScriptedConversation);
		SCR_REGISTER_SECURE(ADD_LINE_TO_CONVERSATION,0x532c5782ea6bc972, CommandAddLineToConversation);
		SCR_REGISTER_SECURE(ADD_PED_TO_CONVERSATION,0x5ec2d166433a9aa4, CommandAddNewConversationSpeaker);
		SCR_REGISTER_SECURE(SET_POSITION_FOR_NULL_CONV_PED,0x71995cc4a2efffbf, CommandSetPositionForNullConvPed);
		SCR_REGISTER_SECURE(SET_ENTITY_FOR_NULL_CONV_PED,0x1522d4231a74d0b1, CommandSetEntityForNullConvPed);
		SCR_REGISTER_SECURE(SET_MICROPHONE_POSITION,0x3eb9c60f0a12c060, CommandSetMicrophonePosition);
		SCR_REGISTER_UNUSED(OVERRIDE_GUNFIGHT_INTENSITY,0x415522de5dd2b29e, CommandOverrideGunfightConductorIntensity);
		SCR_REGISTER_SECURE(SET_CONVERSATION_AUDIO_CONTROLLED_BY_ANIM,0x524b5fc84f413ca8, CommandSetConversationControlledByAnimTriggers);
		SCR_REGISTER_SECURE(SET_CONVERSATION_AUDIO_PLACEHOLDER,0x91700cad9f55ac58, CommandSetConversationPlaceholder);
		SCR_REGISTER_UNUSED(START_SCRIPTED_CONVERSATION, 0x7e4cb1e, CommandStartScriptedConversation);
		SCR_REGISTER_SECURE(START_SCRIPT_PHONE_CONVERSATION,0x0c42fe4000b56a73, CommandStartScriptPhoneConversation);
		SCR_REGISTER_SECURE(PRELOAD_SCRIPT_PHONE_CONVERSATION,0xb4caa90c9dd83932, CommandPreloadScriptPhoneConversation);
		SCR_REGISTER_SECURE(START_SCRIPT_CONVERSATION,0x71c95eba363bdeae, CommandStartScriptConversation);
		SCR_REGISTER_SECURE(PRELOAD_SCRIPT_CONVERSATION,0x3a8253262437ba02, CommandPreloadScriptConversation);
		SCR_REGISTER_SECURE(START_PRELOADED_CONVERSATION,0xfed65f7c525b4505, CommandStartPreloadedConversation);
		SCR_REGISTER_SECURE(GET_IS_PRELOADED_CONVERSATION_READY,0xd973aa641a1cf78a, CommandIsPreloadedConversationReady);
		SCR_REGISTER_SECURE(IS_SCRIPTED_CONVERSATION_ONGOING,0x2e5f8a288a954523, CommandIsScriptedConversationOngoing);
		SCR_REGISTER_UNUSED(IS_UNPAUSED_SCRIPTED_CONVERSATION_ONGOING,0x39f0e915b5405d5e, CommandIsUnpausedScriptedConversationOngoing);
		SCR_REGISTER_SECURE(IS_SCRIPTED_CONVERSATION_LOADED,0x6502d3750e36039c, CommandIsScriptedConversationLoaded);
		SCR_REGISTER_SECURE(GET_CURRENT_SCRIPTED_CONVERSATION_LINE,0xbee29398902b0435, CommandGetCurrentScriptedConversationLine);
		SCR_REGISTER_UNUSED(GET_CURRENT_UNRESOLVED_SCRIPTED_CONVERSATION_LINE,0xc5546e304079dfc4, CommandGetCurrentUnresolvedScriptedConversationLine);
		SCR_REGISTER_SECURE(PAUSE_SCRIPTED_CONVERSATION,0x1195a02d3676e6af, CommandPauseScriptedConversation);
		SCR_REGISTER_SECURE(RESTART_SCRIPTED_CONVERSATION,0x82b0661a78cc39cf, CommandRestartScriptedConversation);
		SCR_REGISTER_SECURE(STOP_SCRIPTED_CONVERSATION,0xd681cc2bc1084db6, CommandAbortScriptedConversation);
		SCR_REGISTER_SECURE(SKIP_TO_NEXT_SCRIPTED_CONVERSATION_LINE,0x26a09f012cc86e43, CommandSkipToNextScriptedConversationLine);
		SCR_REGISTER_SECURE(INTERRUPT_CONVERSATION,0x372ebe3d1a45b19c, CommandInterruptConversation);
		SCR_REGISTER_SECURE(INTERRUPT_CONVERSATION_AND_PAUSE,0xc40ae949f4dd3bb4, CommandInterruptConversationAndPause);
		SCR_REGISTER_SECURE(GET_VARIATION_CHOSEN_FOR_SCRIPTED_LINE,0xf278a9e1bf1bf431, CommandGetVariationChosenForScriptedLine);
		SCR_REGISTER_UNUSED(SET_CONVERSATION_CODE_NAME,0x0ee26c178743f498, CommandSetConversationCodeName);
		SCR_REGISTER_UNUSED(SET_CONVERSATION_POSITION,0x7fedb237be9ea7c3, CommandSetConversationLocation);
		SCR_REGISTER_SECURE(SET_NO_DUCKING_FOR_CONVERSATION,0xd5e0b6866334ad85, CommandSetNoDuckingForConversation);
		SCR_REGISTER_UNUSED(GET_TEXT_KEY_FOR_LINE_OF_AMBIENT_SPEECH,0x2cc69c69faa8ad87, CommandGetTextKeyForLineOfAmbientSpeech);
		SCR_REGISTER_UNUSED(GET_SPEECH_FOR_EMERGENCY_SERVICE_CALL,0x026b1f1220ef56de, CommandGetSpeechForEmergencyServiceCall);
		SCR_REGISTER_UNUSED(PLAY_AUDIO_EVENT,0xf1f7acf1098ac6c3, CommandPlayAudioEvent);
		SCR_REGISTER_UNUSED(PLAY_AUDIO_EVENT_FROM_ENTITY,0x351fc09f9654684d, CommandPlayAudioEventFromEntity);
		SCR_REGISTER_SECURE(REGISTER_SCRIPT_WITH_AUDIO,0xf30cf8286fba13d7, CommandRegisterScriptWithAudio);
		SCR_REGISTER_SECURE(UNREGISTER_SCRIPT_WITH_AUDIO,0xab06b90f4b4b3043, CommandUnregisterScriptWithAudio);
		SCR_REGISTER_SECURE(REQUEST_MISSION_AUDIO_BANK,0x0fa01d16fcd94a52, CommandRequestMissionAudioBank);
		SCR_REGISTER_SECURE(REQUEST_AMBIENT_AUDIO_BANK,0x8133a5f807883bc6, CommandRequestAmbientAudioBank);
		SCR_REGISTER_SECURE(REQUEST_SCRIPT_AUDIO_BANK,0x70f555a7ccf10659, CommandRequestScriptAudioBank);
		SCR_REGISTER_SECURE(HINT_MISSION_AUDIO_BANK,0xf66ae48dbf243a5a, CommandHintMissionAudioBank);
		SCR_REGISTER_SECURE(HINT_AMBIENT_AUDIO_BANK,0x334012e4f8cc6bb6, CommandHintAmbientAudioBank);
		SCR_REGISTER_SECURE(HINT_SCRIPT_AUDIO_BANK,0x925a4a10597c775b, CommandHintScriptAudioBank);
		SCR_REGISTER_SECURE(RELEASE_MISSION_AUDIO_BANK,0x4edac32c9daec24a, CommandMissionAudioBankNoLongerNeeded);
		SCR_REGISTER_SECURE(RELEASE_AMBIENT_AUDIO_BANK,0x257d98d0e399f50e, CommandAmbientAudioBankNoLongerNeeded);
		SCR_REGISTER_SECURE(RELEASE_NAMED_SCRIPT_AUDIO_BANK,0xe24865b69d79521b, CommandNamedScriptAudioBankNoLongerNeeded);
		SCR_REGISTER_SECURE(RELEASE_SCRIPT_AUDIO_BANK,0xeccdae4e977bda96, CommandScriptAudioBankNoLongerNeeded);
		SCR_REGISTER_UNUSED(UNHINT_MISSION_AUDIO_BANK,0xea96d35f9f4b6a76, CommandUnHintMissionAudioBank);
		SCR_REGISTER_SECURE(UNHINT_AMBIENT_AUDIO_BANK,0x1469f0f8128b7961, CommandUnHintAmbientAudioBank);
		SCR_REGISTER_SECURE(UNHINT_SCRIPT_AUDIO_BANK,0x2e8bce38b05b7486, CommandUnHintScriptAudioBank);
		SCR_REGISTER_SECURE(UNHINT_NAMED_SCRIPT_AUDIO_BANK,0x010e091f36dfd98f, CommandUnHintNamedScript);
		SCR_REGISTER_SECURE(GET_SOUND_ID,0xa9adcc8d104aa552, CommandGetSoundId);
		SCR_REGISTER_SECURE(RELEASE_SOUND_ID,0xcdbcc1bc1184b002, CommandReleaseSoundId);
		SCR_REGISTER_SECURE(PLAY_SOUND,0x5e32098b63d0daa5, CommandPlaySound);
		SCR_REGISTER_SECURE(PLAY_SOUND_FRONTEND,0x91dfc8f68f6d9b05, CommandPlaySoundFrontend);
		SCR_REGISTER_SECURE(PLAY_DEFERRED_SOUND_FRONTEND,0xb17f82e32ffb8e31, CommandPlayDeferredSoundFrontend);
		SCR_REGISTER_SECURE(PLAY_SOUND_FROM_ENTITY,0x979fc7400a5d0cd2, CommandPlaySoundFromEntity);
		SCR_REGISTER_SECURE(PLAY_SOUND_FROM_ENTITY_HASH,0xfd9b3f34e9fae19d, CommandPlaySoundFromEntityHash);
		SCR_REGISTER_SECURE(PLAY_SOUND_FROM_COORD,0xd336f8d9637b963f, CommandPlaySoundFromPosition);
		SCR_REGISTER_SECURE(UPDATE_SOUND_COORD,0xff8f59260b13a7c7, CommandUpdateSoundPosition);
		SCR_REGISTER_UNUSED(PLAY_FIRE_SOUND_FROM_COORD,0xd5176c9737691935, CommandPlayFireSoundFromPosition);
		SCR_REGISTER_SECURE(STOP_SOUND,0xf889bdfcc181ba9f, CommandStopSound);
		SCR_REGISTER_SECURE(GET_NETWORK_ID_FROM_SOUND_ID,0x829eedc841317071, CommandGetNetworkIdFromSoundId);
		SCR_REGISTER_SECURE(GET_SOUND_ID_FROM_NETWORK_ID,0xeb33eead608b9d11, CommandGetSoundIdFromNetworkId);
		SCR_REGISTER_SECURE(SET_VARIABLE_ON_SOUND,0x57973addf2bfaa90, CommandSetVariableOnSound);
		SCR_REGISTER_SECURE(SET_VARIABLE_ON_STREAM,0xaba36033671c7e58, CommandSetVariableOnStream);
		SCR_REGISTER_SECURE(OVERRIDE_UNDERWATER_STREAM,0x7926611c8aac1853, CommandOverrideUnderWaterStream);
		SCR_REGISTER_SECURE(SET_VARIABLE_ON_UNDER_WATER_STREAM,0x9e115306acf84ae1, CommandSetVariableOnUnderWaterStream);
		SCR_REGISTER_SECURE(HAS_SOUND_FINISHED,0x78d9add511fead8b, CommandHasSoundFinished);
		SCR_REGISTER_SECURE(PLAY_PED_AMBIENT_SPEECH_NATIVE,0xb0c559bd7d5d270d, CommandSayAmbientSpeechNative);
		SCR_REGISTER_SECURE(PLAY_PED_AMBIENT_SPEECH_AND_CLONE_NATIVE,0x9368ec1f042bf3f9, CommandSayAmbientSpeechAndCloneNative);
		SCR_REGISTER_SECURE(PLAY_PED_AMBIENT_SPEECH_WITH_VOICE_NATIVE,0x49c085d876a9986d, CommandSayAmbientSpeechWithVoiceNative);
		SCR_REGISTER_SECURE(PLAY_AMBIENT_SPEECH_FROM_POSITION_NATIVE,0x1f2f249740361ec0, CommandSayAmbientSpeechFromPositionNative);
		SCR_REGISTER_UNUSED(PLAY_PED_AMBIENT_SPEECH, 0xb4eb114e, CommandSayAmbientSpeech);
		SCR_REGISTER_UNUSED(PLAY_PED_AMBIENT_SPEECH_WITH_VOICE, 0xa3e3390c, CommandSayAmbientSpeechWithVoice);

		SCR_REGISTER_UNUSED(MAKE_TREVOR_RAGE,0x754469aa9ce16092, CommandMakeTrevorRage);
		SCR_REGISTER_SECURE(OVERRIDE_TREVOR_RAGE,0x7213963684b1782c, CommandOverrideTrevorRage);
		SCR_REGISTER_SECURE(RESET_TREVOR_RAGE,0x3007781d739cd0e2, CommandResetTrevorRage);
		SCR_REGISTER_SECURE(SET_PLAYER_ANGRY,0x748643a0d9d0e927, CommandSetPlayerAngry);
		SCR_REGISTER_UNUSED(SET_PLAYER_ANGRY_SHORT_TIME,0x9fec63369ba1e0ea, CommandSetPlayerAngryShortTime);

		SCR_REGISTER_SECURE(PLAY_PAIN,0x35083f820751423c, CommandPlayPain);

		SCR_REGISTER_UNUSED(RESERVE_AMBIENT_SPEECH_SLOT, 0x6A56EAE1, CommandReserveAmbientSpeechSlot);
		SCR_REGISTER_UNUSED(RELEASE_AMBIENT_SPEECH_SLOT, 0xBB148F19, CommandReleaseAmbientSpeechSlot);

		SCR_REGISTER_UNUSED(REQUEST_WEAPON_AUDIO,0x32b002f89adf91cb, CommandRequestWeaponAudio);
		SCR_REGISTER_SECURE(RELEASE_WEAPON_AUDIO,0xa6c6ff0199ed0e97, CommandReleaseWeaponAudio);

		SCR_REGISTER_SECURE(ACTIVATE_AUDIO_SLOWMO_MODE,0x6da0cd62263eca47, CommandActivateSlowMoMode);
		SCR_REGISTER_SECURE(DEACTIVATE_AUDIO_SLOWMO_MODE,0x691fb3202b3fdffe, CommandDeactivateSlowMoMode);

		SCR_REGISTER_UNUSED(PLAY_PRELOADED_SPEECH,0x05bdbb07a1062ef1, CommandPlayPreloadedSpeech);
		SCR_REGISTER_SECURE(SET_AMBIENT_VOICE_NAME,0xeebc95a38cbded42, CommandSetAmbientVoiceName);
        SCR_REGISTER_SECURE(SET_AMBIENT_VOICE_NAME_HASH,0x1e8c1135d675a212, CommandSetAmbientVoiceNameHash);
        SCR_REGISTER_SECURE(GET_AMBIENT_VOICE_NAME_HASH,0xf6734e5188cc2835, CommandGetAmbientVoiceNameHash);        
		SCR_REGISTER_SECURE(SET_PED_VOICE_FULL,0xcc903889dfa2b498, CommandForceFullVoice);
		SCR_REGISTER_SECURE(SET_PED_RACE_AND_VOICE_GROUP,0xa1d1db7d9797b00f, CommandSetPedRaceAndVoiceGroup);
 		SCR_REGISTER_SECURE(SET_PED_VOICE_GROUP,0xdb0d726ee9705c73, CommandSetPedGroupVoice);
		SCR_REGISTER_SECURE(SET_PED_VOICE_GROUP_FROM_RACE_TO_PVG,0x0babc1345abbfb16, CommandSetPedRaceToPedVoiceGroup);		
 		SCR_REGISTER_SECURE(SET_PED_GENDER,0xaf59712dc40dac2c, CommandSetPedGender);
		SCR_REGISTER_UNUSED(SET_VOICE_ID_FROM_HEAD_COMPONENT,0x7b0b0cd553be436d, CommandSetVoiceIdFromHeadComponent);
		SCR_REGISTER_SECURE(STOP_CURRENT_PLAYING_SPEECH,0x603a206371fcf97b, CommandStopCurrentlyPlayingSpeech);
		SCR_REGISTER_SECURE(STOP_CURRENT_PLAYING_AMBIENT_SPEECH,0x642eeab0cfc6a636, CommandCancelCurrentlyPlayingAmbientSpeech);
		SCR_REGISTER_SECURE(IS_AMBIENT_SPEECH_PLAYING,0xb6f46abd140050f1, CommandIsAmbientSpeechPlaying);
		SCR_REGISTER_SECURE(IS_SCRIPTED_SPEECH_PLAYING,0x110b92aad0fbee95, CommandIsScriptedSpeechPlaying);
		SCR_REGISTER_SECURE(IS_ANY_SPEECH_PLAYING,0x4239f34c6ee6a3e9, CommandIsAnySpeechPlaying);
		SCR_REGISTER_SECURE(IS_ANY_POSITIONAL_SPEECH_PLAYING,0x56b992eded7c4010, CommandIsAnyPositionalSpeechPlaying);
		SCR_REGISTER_SECURE(DOES_CONTEXT_EXIST_FOR_THIS_PED,0xacfde386dbaed938, CommandDoesContextExistForThisPed);
		SCR_REGISTER_UNUSED(SET_PED_VOICE_FOR_CONTEXT,0xab603956e0ce8be9, CommandSetPedVoiceForContext);
		SCR_REGISTER_SECURE(IS_PED_IN_CURRENT_CONVERSATION,0x769cb0e209f11ad8, CommandIsPedInCurrentConversation);
		SCR_REGISTER_SECURE(SET_PED_IS_DRUNK,0x0cf765282e225f04, CommandSetPedIsDrunk);
		SCR_REGISTER_UNUSED(SET_PED_IS_BLIND_RAGING,0x3056b4c713926e8f, CommandSetPedIsBlindRaging);
		SCR_REGISTER_UNUSED(SET_PED_IS_CRYING,0x0933142654e9bddc, CommandSetPedIsCrying);
		SCR_REGISTER_SECURE(PLAY_ANIMAL_VOCALIZATION,0x396d9bc8f22862ea, CommandPlayAnimalVocalization);
		SCR_REGISTER_SECURE(IS_ANIMAL_VOCALIZATION_PLAYING,0x1756a9731cba54ba, CommandIsAnimalVocalizationPlaying);
		SCR_REGISTER_SECURE(SET_ANIMAL_MOOD,0xfbd62b4feb3546a4, CommandSetAnimalMood);
		SCR_REGISTER_UNUSED(PLAY_PED_AUDIO_EVENT_ANIM,0xb296df92a31fea0c, CommandHandleAudioAnimEvent);
		
#if NA_RADIO_ENABLED
		SCR_REGISTER_SECURE(IS_MOBILE_PHONE_RADIO_ACTIVE,0xa678e3511119a139, CommandIsMobilePhoneRadioActive);
		SCR_REGISTER_SECURE(SET_MOBILE_PHONE_RADIO_STATE,0x55a6c59c22fe1c30, CommandSetMobilePhoneRadioState);
		SCR_REGISTER_SECURE(GET_PLAYER_RADIO_STATION_INDEX,0x763a39ac8944f8ba, CommandGetPlayerRadioStationIndex);
		SCR_REGISTER_SECURE(GET_PLAYER_RADIO_STATION_NAME,0x3efc185839d591db, CommandGetPlayerRadioStationName);
		SCR_REGISTER_SECURE(GET_RADIO_STATION_NAME,0x8ecf4849e085e658, CommandGetRadioStationName);
		SCR_REGISTER_SECURE(GET_PLAYER_RADIO_STATION_GENRE,0x3af345aa5885df2b, CommandGetPlayerRadioStationGenre);
		SCR_REGISTER_SECURE(IS_RADIO_RETUNING,0xf5a00c1eb6e872b8, CommandIsRadioRetuning);
		SCR_REGISTER_SECURE(IS_RADIO_FADED_OUT,0xab0bb8aa7ab39616, CommandIsRadioFadedOut);		
		SCR_REGISTER_SECURE(SET_RADIO_RETUNE_UP,0x337db56aafc7c552, CommandRetuneRadioUp);
		SCR_REGISTER_SECURE(SET_RADIO_RETUNE_DOWN,0x0fc0db6fc527861c, CommandRetuneRadioDown);
		SCR_REGISTER_SECURE(SET_RADIO_TO_STATION_NAME,0x65c31d05dd70b2cc, CommandRetuneRadioToStationName);
		SCR_REGISTER_SECURE(SET_VEH_RADIO_STATION,0xd501428969a52c0f, CommandSetVehicleRadioStation);
		SCR_REGISTER_SECURE(SET_VEH_HAS_NORMAL_RADIO,0xbadc62ccfab06a0a, CommandSetVehicleHasNormalRadio);
		SCR_REGISTER_SECURE(IS_VEHICLE_RADIO_ON,0x4c5bb58aaac629c5, CommandIsVehicleRadioOn);
		SCR_REGISTER_SECURE(SET_VEH_FORCED_RADIO_THIS_FRAME,0xc36e594dcaeb0f23, CommandSetVehicleForcedRadio);
		SCR_REGISTER_SECURE(SET_EMITTER_RADIO_STATION,0xece7b7efd96cebf4, CommandSetEmitterRadioStation);
		SCR_REGISTER_SECURE(SET_STATIC_EMITTER_ENABLED,0x380313ff4cef9b49, CommandSetEmitterEnabled);
		SCR_REGISTER_SECURE(LINK_STATIC_EMITTER_TO_ENTITY,0x4637ae8ffd474a33, CommandLinkStaticEmitterToEntity);		
		SCR_REGISTER_SECURE(SET_RADIO_TO_STATION_INDEX,0xd0b0f5e50ce08c52, CommandRetuneRadioToStationIndex);
		SCR_REGISTER_SECURE(SET_FRONTEND_RADIO_ACTIVE,0x6ef26a94575e15c4, CommandSetFrontendRadio);
		SCR_REGISTER_SECURE(UNLOCK_MISSION_NEWS_STORY,0x9c7f69ddeb6dbb06, CommandUnlockMissionNewsStory);
		SCR_REGISTER_SECURE(IS_MISSION_NEWS_STORY_UNLOCKED,0x10b1feb12ad97ddd, CommandIsMissionNewsStoryUnlocked);
		SCR_REGISTER_SECURE(GET_AUDIBLE_MUSIC_TRACK_TEXT_ID,0x4fc939cff1d6386d, CommandGetAudibleTrackTextId);
		SCR_REGISTER_UNUSED(REGISTER_RADIO_TRACK_TAGGED,0x49f6bad81a7e6fbd, CommandReportTaggedRadioTrack);
		SCR_REGISTER_SECURE(PLAY_END_CREDITS_MUSIC,0x9981786616f43e7d, CommandSetEndCreditsMusic);
		SCR_REGISTER_SECURE(SKIP_RADIO_FORWARD,0xaf08933cf373bf40, CommandSkipRadioForward);
		SCR_REGISTER_SECURE(FREEZE_RADIO_STATION,0x8b7cd2568e24fe82, CommandFreezeRadioStation);
		SCR_REGISTER_SECURE(UNFREEZE_RADIO_STATION,0xde6c620b87b6536c, CommandUnfreezeRadioStation);
		SCR_REGISTER_SECURE(SET_RADIO_AUTO_UNFREEZE,0xa64f9d860f7389c9, CommandSetAutoUnfreeze);
		SCR_REGISTER_SECURE(SET_INITIAL_PLAYER_STATION,0x211c581b8aacd11c, CommandForceInitialPlayerRadioStation);
		SCR_REGISTER_SECURE(SET_USER_RADIO_CONTROL_ENABLED,0xb68bd12b3a8c52fd, CommandSetUserRadioControlEnabled);
		SCR_REGISTER_SECURE(SET_RADIO_TRACK,0x93cb338c8902831c, CommandForceRadioTrack);
		SCR_REGISTER_SECURE(SET_RADIO_TRACK_WITH_START_OFFSET,0x5b1db18d92a04434, CommandForceRadioTrackWithStartOffset);
		SCR_REGISTER_SECURE(SET_NEXT_RADIO_TRACK,0x3d330982888f05e5, CommandForceNextRadioTrack);	
		SCR_REGISTER_SECURE(SET_VEHICLE_RADIO_LOUD,0x32cac81ec8c5ba86, CommandSetLoudVehicleRadio);
		SCR_REGISTER_SECURE(CAN_VEHICLE_RECEIVE_CB_RADIO,0xb833846de5552bc6, CommandCanVehicleReceiveCBRadio);
		SCR_REGISTER_UNUSED(SET_IN_NETWORK_LOBBY,0x0bb65b3bbd80075c, CommandSetInNetworkLobby);
		SCR_REGISTER_UNUSED(SET_LOBBY_MUTE_OVERRIDE,0x22894a272b6966fe, CommandSetLobbyMuteOverride);
		SCR_REGISTER_SECURE(SET_MOBILE_RADIO_ENABLED_DURING_GAMEPLAY,0x9367fadb11a30d38, CommandSetMobileRadioEnabledDuringGameplay);
		SCR_REGISTER_SECURE(DOES_PLAYER_VEH_HAVE_RADIO,0x678069837eae492b, CommandDoesPlayerVehicleHaveRadio);
		SCR_REGISTER_SECURE(IS_PLAYER_VEH_RADIO_ENABLE,0xb9fd3472c37271a4, CommandIsPlayerVehRadioEnable);
		SCR_REGISTER_SECURE(SET_VEHICLE_RADIO_ENABLED,0xbd10a469f50e1a2a, CommandSetVehicleRadioEnabled);
		SCR_REGISTER_SECURE(SET_POSITIONED_PLAYER_VEHICLE_RADIO_EMITTER_ENABLED,0xe9669168376aa182, CommandSetPositionedPlayerVehicleRadioEmitterEnabled);
		SCR_REGISTER_SECURE(SET_CUSTOM_RADIO_TRACK_LIST,0xba4e8f889faa1728, CommandSetCustomRadioTrackList);
		SCR_REGISTER_SECURE(CLEAR_CUSTOM_RADIO_TRACK_LIST,0x234263703caeb763, CommandClearCustomRadioTrackList);
		SCR_REGISTER_SECURE(GET_NUM_UNLOCKED_RADIO_STATIONS,0x6519b5145bb024dc, CommandGetNumUnlockedRadioStations);
		SCR_REGISTER_UNUSED(GET_NUM_UNLOCKED_FAVOURITED_RADIO_STATIONS,0x497fc61659bcafaa, CommandGetNumUnlockedFavouritedRadioStations);
		SCR_REGISTER_SECURE(FIND_RADIO_STATION_INDEX,0xaff4500a326aaf91, CommandFindRadioStationIndex);
		SCR_REGISTER_SECURE(SET_RADIO_STATION_MUSIC_ONLY,0x216991f905439747, CommandSetRadioStationMusicOnly);
		SCR_REGISTER_SECURE(SET_RADIO_FRONTEND_FADE_TIME,0x1436b64d165d89e4, CommandSetRadioFrontendFadeTime);
		SCR_REGISTER_SECURE(UNLOCK_RADIO_STATION_TRACK_LIST,0x5b1ab4d2fe023b66, CommandUnlockRadioStationTrackList);
		SCR_REGISTER_SECURE(LOCK_RADIO_STATION_TRACK_LIST,0xd1f899744fecbcea, CommandLockRadioStationTrackList);
		SCR_REGISTER_UNUSED(IS_RADIO_STATION_TRACK_LIST_UNLOCKED,0xceacf83594d21c2e, CommandIsRadioStationTrackListUnlocked);		
		SCR_REGISTER_SECURE(UPDATE_UNLOCKABLE_DJ_RADIO_TRACKS,0x389438916b0cb443, UpdateDLCBattleUnlockableTracks);
		SCR_REGISTER_SECURE(LOCK_RADIO_STATION,0xaabab6267db5e5cd, CommandLockRadioStation);
		SCR_REGISTER_SECURE(SET_RADIO_STATION_AS_FAVOURITE,0xe479fc5ebd431c84, CommandSetRadioStationAsFavourite);
		SCR_REGISTER_SECURE(IS_RADIO_STATION_FAVOURITED,0x2b1784db08afea79, CommandIsRadioStationFavourited);		
		SCR_REGISTER_SECURE(GET_NEXT_AUDIBLE_BEAT,0xcd5b8ecec71faa00, CommandGetNextAudibleBeat);
		SCR_REGISTER_UNUSED(GET_NEXT_BEAT_FOR_STATION,0xd15e24de615a024b, CommandGetNextBeatForStation);
		SCR_REGISTER_UNUSED(SHUT_DOWN_ACTIVE_TRACKS,0x0e91f757c49179cc, CommandShutDownActiveTracks);

		// USB Station functionality
		SCR_REGISTER_SECURE(FORCE_MUSIC_TRACK_LIST,0x3da2b2e4f297cafa, CommandForceMusicTrackList);
		SCR_REGISTER_UNUSED(GET_LAST_FORCED_TRACK_LIST,0xa74e35a2434bdf42, CommandGetLastForcedTrackList);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_PLAY_TIME_OFFSET_MS,0xc9a6f1669345b14e, CommandGetMusicTrackListPlayTimeOffset);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_DURATION_MS,0x62ec19ec897e956f, CommandGetMusicTrackListDurationMs);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_TRACK_DURATION_MS,0x2d07c69a4729fe95, CommandGetMusicTrackListTrackDurationMs);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_ACTIVE_TRACK_INDEX,0x1ad8c31a93bbc5a3, CommandGetMusicTrackListActiveTrackIndex);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_NUM_TRACKS,0x93d096bc3ba70c02, CommandGetMusicTrackListNumTracks);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_TRACK_ID_AT_TIME_MS,0x02396a09f2dd0070, CommandGetMusicTrackListTrackIDForTimeOffset);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_TRACK_ID_AT_INDEX,0x667b6b936d29a222, CommandGetMusicTrackListTrackIDForIndex);
		SCR_REGISTER_UNUSED(GET_MUSIC_TRACK_LIST_TRACK_START_TIME_MS,0xf63d184b4dab8d00, CommandGetMusicTrackListTrackStartTimeMs);
		SCR_REGISTER_UNUSED(SKIP_TO_OFFSET_IN_MUSIC_TRACK_LIST,0x920243ab6c7dd0ab, CommandSkipToOffsetInMusicTrackList);

		SCR_REGISTER_SECURE(GET_CURRENT_TRACK_PLAY_TIME,0x48c9930f53211812, CommandGetCurrentTrackPlayTime);
		SCR_REGISTER_SECURE(GET_CURRENT_TRACK_SOUND_NAME,0x07e9d9bdd31f68bd, CommandGetCurrentTrackSoundName);
#endif // NA_RADIO_ENABLED

		SCR_REGISTER_SECURE(SET_VEHICLE_MISSILE_WARNING_ENABLED,0x36b3f1be54404061, CommandSetVehicleMissileWarningEnabled);

		SCR_REGISTER_SECURE(SET_AMBIENT_ZONE_STATE,0x5fb1ee6965515598, CommandSetAmbientZoneState);
		SCR_REGISTER_SECURE(CLEAR_AMBIENT_ZONE_STATE,0x2e81e6da894efe9c, CommandClearAmbientZoneState);
		SCR_REGISTER_SECURE(SET_AMBIENT_ZONE_LIST_STATE,0x2d6eda8e0bc56548, CommandSetAmbientZoneListState);
		SCR_REGISTER_SECURE(CLEAR_AMBIENT_ZONE_LIST_STATE,0x9718190decc57d4b, CommandClearAmbientZoneListState);
		SCR_REGISTER_SECURE(SET_AMBIENT_ZONE_STATE_PERSISTENT,0x1f71b978fc3cf279, CommandSetAmbientZoneStatePersistent);
		SCR_REGISTER_SECURE(SET_AMBIENT_ZONE_LIST_STATE_PERSISTENT,0x38275e3ea5d16c78, CommandSetAmbientZoneListStatePersistent);
		SCR_REGISTER_SECURE(IS_AMBIENT_ZONE_ENABLED,0xdf307e8b8dd36e6e, CommandIsAmbientZoneEnabled);

		SCR_REGISTER_SECURE(REFRESH_CLOSEST_OCEAN_SHORELINE,0x582b5cd0b4425222, CommandRefreshClosestOceanShoreline);

		SCR_REGISTER_UNUSED(GET_CUTSCENE_AUDIO_TIME_MS,0x125775f5c74c6fb2, CommandGetCutsceneAudioTimeMs);
		SCR_REGISTER_SECURE(SET_CUTSCENE_AUDIO_OVERRIDE,0x52763eee2db5ee35, CommandSetCutsceneAudioOverride);

		SCR_REGISTER_SECURE(SET_VARIABLE_ON_SYNCH_SCENE_AUDIO,0xbb4da79ca6c5e9ed, CommandSetVariableOnSynchedScene);
		SCR_REGISTER_UNUSED(SET_PROXIMITY_WHOOSHES_ENABLED,0x80351b8b4bbc7250, CommandSetProximityWhooshesEnabled);
		
#if NA_POLICESCANNER_ENABLED
		SCR_REGISTER_UNUSED(PLAY_POLICE_REPORT_SPOTTING_NPC,0xeb1167105e048768, CommandTriggerPoliceReportSpottingNPC);
		SCR_REGISTER_SECURE(PLAY_POLICE_REPORT,0x8bae3b6695282dc0, CommandTriggerPoliceReport);
		SCR_REGISTER_UNUSED(CANCEL_POLICE_REPORT,0xbf7f84a49adc9c29, CommandCancelPoliceReport);
		SCR_REGISTER_SECURE(CANCEL_ALL_POLICE_REPORTS,0xa43700313f126344, CommandCancelAllPoliceReports);
		SCR_REGISTER_UNUSED(IS_POLICE_REPORT_PLAYING,0xc7c07ae99b12f1c1, CommandIsPoliceReportPlaying);
		SCR_REGISTER_UNUSED(IS_POLICE_REPORT_ID_VALID,0xd6d709e15722c84e, CommandIsPoliceReportValid);
		SCR_REGISTER_UNUSED(GET_PLAYING_POLICE_REPORT,0x49a190be049f104a, CommandGetPlayingPoliceReport);
		SCR_REGISTER_UNUSED(PLAY_VIGILANTE_CRIME,0x336bb9b1a77040f9, CommandTriggerVigilanteCrime);
		SCR_REGISTER_UNUSED(SET_POLICE_SCANNER_CAR_CODE_INFO,0xd1d13577fc39bfaa, CommandSetPolicScannerCarCodeInfo);
		SCR_REGISTER_UNUSED(SET_POLICE_SCANNER_POSITION_INFO,0x63824a641c4828c3, CommandSetPoliceScannerPositionInfo);
		SCR_REGISTER_UNUSED(SET_POLICE_SCANNER_CRIME_INFO,0xfe5aeb24424a78f4, CommandSetPoliceScannerCrimeInfo);
#endif // NA_POLICESCANNER_ENABLED

		SCR_REGISTER_UNUSED(ENABLE_PARKING_BEEP,0x702be60b9f5d3a2a, CommandEnableParkingBeep);
		SCR_REGISTER_SECURE(BLIP_SIREN,0x22348609bc8e8831, CommandBlipSiren);
		SCR_REGISTER_SECURE(OVERRIDE_VEH_HORN,0x785a594ef1969952, CommandOverrideHorn);
		SCR_REGISTER_SECURE(IS_HORN_ACTIVE,0x5e1f820e490c4eef, CommandIsHornActive);
		SCR_REGISTER_UNUSED(SET_CHASE_AUDIO,0x25e54a9c8f37f101, CommandEnableChaseAudio);
		SCR_REGISTER_SECURE(SET_AGGRESSIVE_HORNS,0x8a422f5ce0368c78, CommandEnableAggressiveHorns);
		SCR_REGISTER_UNUSED(SET_GAMEWORLD_AUDIO_MUTE,0x013f178a77a43095, CommandMuteGameWorldAudio);
		SCR_REGISTER_SECURE(SET_RADIO_POSITION_AUDIO_MUTE,0x2bbae83c1aa1c306, CommandMutePositionedRadio);
		SCR_REGISTER_UNUSED(SET_GAMEWORLD_AND_POSITIONED_RADIO_FOR_TV_MUTE,0xc67f090d7168ebec, CommandMuteGameWorldAndPositionedRadioForTV);
		SCR_REGISTER_SECURE(SET_VEHICLE_CONVERSATIONS_PERSIST,0x9ef092cf52f06ef6, CommandDontAbortVehicleConversations);
		SCR_REGISTER_SECURE(SET_VEHICLE_CONVERSATIONS_PERSIST_NEW,0xf580e58474fe0bee, CommandDontAbortVehicleConversationsNew);
		SCR_REGISTER_UNUSED(SET_TRAIN_AUDIO_ROLLOFF,0x9e4626dfc6ed763d, CommandSetTrainAudioRolloff);
		SCR_REGISTER_SECURE(IS_STREAM_PLAYING,0xa74cb1a66ee16d43, CommandIsStreamPlaying);
		SCR_REGISTER_SECURE(GET_STREAM_PLAY_TIME,0x5414f3b44b148780, CommandGetStreamPlayTime);
		SCR_REGISTER_SECURE(LOAD_STREAM,0x01fe3d0e34407698, CommandPreloadStream);
		SCR_REGISTER_SECURE(LOAD_STREAM_WITH_START_OFFSET,0x991d256c78de67f5, CommandPreloadStreamWithStartOffset);
		SCR_REGISTER_SECURE(PLAY_STREAM_FROM_PED,0x61379e8ce8e4bcee, CommandPlayStreamFromPed);
		SCR_REGISTER_SECURE(PLAY_STREAM_FROM_VEHICLE,0xc06ef1b97dd5bbe8, CommandPlayStreamFromVehicle);
		SCR_REGISTER_SECURE(PLAY_STREAM_FROM_OBJECT,0xa0d1364489a44449, CommandPlayStreamFromObject);
		SCR_REGISTER_SECURE(PLAY_STREAM_FRONTEND,0xb414ad851359ed65, CommandPlayStreamFrontend);
		SCR_REGISTER_SECURE(PLAY_STREAM_FROM_POSITION,0xc2512e3805388b5b, CommandPlayStreamFromPosition);
		SCR_REGISTER_SECURE(STOP_STREAM,0xacc66366e248a4ca, CommandStopStream);
		SCR_REGISTER_SECURE(STOP_PED_SPEAKING,0x059473086913178c, CommandStopPedSpeaking);
		SCR_REGISTER_SECURE(BLOCK_ALL_SPEECH_FROM_PED,0x94a6586aee4bb68e, CommandBlockAllSpeechFromPed);
		SCR_REGISTER_SECURE(STOP_PED_SPEAKING_SYNCED,0x3124fcfd969b6839, CommandStopPedSpeakingSynced);		
		SCR_REGISTER_SECURE(DISABLE_PED_PAIN_AUDIO,0x6b27e5dc03fa026e, CommandDisablePedPainAudio);
		SCR_REGISTER_SECURE(IS_AMBIENT_SPEECH_DISABLED,0x86a123372f1a821a, CommandIsAmbientSpeechDisabled);
		SCR_REGISTER_SECURE(BLOCK_SPEECH_CONTEXT_GROUP,0x98357e1b4e44f841, CommandBlockSpeechContextGroup);
		SCR_REGISTER_SECURE(UNBLOCK_SPEECH_CONTEXT_GROUP,0x509e14b39e123d75, CommandUnblockSpeechContextGroup);
		SCR_REGISTER_UNUSED(SET_IS_BUDDY,0xc7ad2f9591529ba4, CommandSetIsBuddy);
		SCR_REGISTER_SECURE(SET_SIREN_WITH_NO_DRIVER,0xfadd5f130031faf6, CommandSetSirenWithNoDriver);
		SCR_REGISTER_SECURE(SET_SIREN_BYPASS_MP_DRIVER_CHECK,0x7e84e95e84952cfe, CommandSetSirenBypassMPDriverCheck);		
		SCR_REGISTER_SECURE(TRIGGER_SIREN_AUDIO,0xff98a9b54847121f, CommandTriggerSirenAudio);
		SCR_REGISTER_SECURE(SET_HORN_PERMANENTLY_ON,0xcfc69c95792129ea, CommandSetHornPermanentlyOn);
		SCR_REGISTER_SECURE(SET_HORN_ENABLED,0xe01b728a49fd80c4, CommandSetHornEnabled);
		SCR_REGISTER_SECURE(SET_AUDIO_VEHICLE_PRIORITY,0xc5ead0c81781889b, CommandSetAudioVehiclePriority);
		SCR_REGISTER_SECURE(SET_HORN_PERMANENTLY_ON_TIME,0x90cc747689895bca, CommandSetHornPermanentlyOnTime);
		SCR_REGISTER_SECURE(USE_SIREN_AS_HORN,0xe57583873c4b123e, CommandUseSireAsHorn);
		SCR_REGISTER_SECURE(FORCE_USE_AUDIO_GAME_OBJECT,0x1be725508ba268ef, CommandForceVehicleGameObject);
		SCR_REGISTER_SECURE(PRELOAD_VEHICLE_AUDIO_BANK,0x1b848f64eff9350e, CommandPreLoadVehicleBank);
		SCR_REGISTER_SECURE(SET_VEHICLE_STARTUP_REV_SOUND,0x528a587b1b02de46, CommandSetVehicleStartupRevSound);
		SCR_REGISTER_SECURE(RESET_VEHICLE_STARTUP_REV_SOUND,0x8674818c1d545dc3, CommandResetVehicleStartupRevSound);		
		SCR_REGISTER_SECURE(SET_VEHICLE_FORCE_REVERSE_WARNING,0x29a5c8a8ea30da95, CommandSetVehicleForceReverseWarning);		
		SCR_REGISTER_SECURE(IS_VEHICLE_AUDIBLY_DAMAGED,0x588c47cedf788e4c, CommandIsVehicleAudiblyDamaged);
		SCR_REGISTER_SECURE(SET_VEHICLE_AUDIO_ENGINE_DAMAGE_FACTOR,0xde74af71c08a81ea, CommandSetEngineDamageFactor);
		SCR_REGISTER_SECURE(SET_VEHICLE_AUDIO_BODY_DAMAGE_FACTOR,0xf4801e32b95aa145, CommandSetBodyDamageFactor);
		SCR_REGISTER_SECURE(ENABLE_VEHICLE_FANBELT_DAMAGE,0x140cd5c2b2916f0e, CommandEnableFanbeltDamage);
		SCR_REGISTER_SECURE(ENABLE_VEHICLE_EXHAUST_POPS,0x0690e7850ceca08a, CommandEnableVehicleExhaustPops);
		SCR_REGISTER_SECURE(SET_VEHICLE_BOOST_ACTIVE,0x888ac6fc97c7ebc7, CommandSetVehicleBoostActive);
		SCR_REGISTER_SECURE(SET_PLAYER_VEHICLE_ALARM_AUDIO_ACTIVE,0x805e5195b36dae42, CommandSetPlayerVehicleAlarmActive);
		SCR_REGISTER_SECURE(SET_SCRIPT_UPDATE_DOOR_AUDIO,0x99c46337f53eecad,  CommandScriptUpdateDoorAudio);
		SCR_REGISTER_SECURE(PLAY_VEHICLE_DOOR_OPEN_SOUND,0x01c2ab1a8ed72f72,  CommandPlayVehicleDoorOpenSound);
		SCR_REGISTER_SECURE(PLAY_VEHICLE_DOOR_CLOSE_SOUND,0x6767e4193fd5b5f1, CommandPlayVehicleDoorCloseSound);
		SCR_REGISTER_UNUSED(ENABLE_CROP_DUSTING_SOUNDS,0xd8a730c14c1a1a2f, CommandEnableVehicleCropDustSounds);
		SCR_REGISTER_SECURE(ENABLE_STALL_WARNING_SOUNDS,0x7b9a3d69e4563a91, CommandEnableVehicleStallWarningSounds);
		SCR_REGISTER_SECURE(IS_GAME_IN_CONTROL_OF_MUSIC,0xd4cce8d6ca771ac8, CommandIsGameInControlOfMusic);
		SCR_REGISTER_UNUSED(SET_END_CREDITS_FADE_ACTIVE,0x6d92b611847e4c1a, CommandSetEndCreditsFade);
		SCR_REGISTER_SECURE(SET_GPS_ACTIVE,0x62ab99f33efdf125, CommandDisableGps);
		SCR_REGISTER_SECURE(PLAY_MISSION_COMPLETE_AUDIO,0xb6e6f1fbdde27cf3, CommandTriggerMissionComplete);
		SCR_REGISTER_SECURE(IS_MISSION_COMPLETE_PLAYING,0xf936e26eb07c7a06, CommandIsMissionCompletePlaying);
		SCR_REGISTER_SECURE(IS_MISSION_COMPLETE_READY_FOR_UI,0x51b424149bc1c8f0, CommandMissionCompleteReadyForUI);
		SCR_REGISTER_SECURE(BLOCK_DEATH_JINGLE,0x6430be9007e43c50, CommandBlockDeathJingle);
		SCR_REGISTER_UNUSED(IS_DEATH_JINGLE_BLOCKED,0x0ea671f79d4ffa14, CommandIsDeathJingleBlocked);
		SCR_REGISTER_UNUSED(SET_ROMANS_MOOD,0xfe8cabe22d9a393b, CommandSetRomansMood);
		SCR_REGISTER_UNUSED(SET_BRIANS_MOOD,0x0acaf833bef3835f, CommandSetBriansMood);
		SCR_REGISTER_SECURE(START_AUDIO_SCENE,0x66b27a59829491d3, CommandStartAudioScene);
		SCR_REGISTER_SECURE(STOP_AUDIO_SCENE,0x0af8d3a06bb92903, CommandStopAudioScene);
		SCR_REGISTER_SECURE(STOP_AUDIO_SCENES,0xc33ba49210b9fbbf, CommandStopAudioScenes);
		SCR_REGISTER_SECURE(IS_AUDIO_SCENE_ACTIVE,0xa37204c64473b324, CommandIsSceneActive);
		SCR_REGISTER_SECURE(SET_AUDIO_SCENE_VARIABLE,0x9364fa34f6e72a61, CommandSetAudioSceneVariable);
		SCR_REGISTER_UNUSED(HAS_AUDIO_SCENE_FINISHED_FADE,0x6d9b4046e9dd1392, CommandHasSceneFinishedFade);
		SCR_REGISTER_SECURE(SET_AUDIO_SCRIPT_CLEANUP_TIME,0xbc770cb8938a190b, CommandSetScriptCleanupTime);
#if NA_POLICESCANNER_ENABLED
		SCR_REGISTER_UNUSED(SET_POLICE_SCANNER_AUDIO_SCENE,0x4c4ac3aa47666f87, CommandSetPoliceScannerAudioScene);
		SCR_REGISTER_UNUSED(STOP_AND_REMOVE_POLICE_SCANNER_AUDIO_SCENE,0xa1daf3140c603ed2, CommandStopAndRemovePoliceScannerAudioScene);
#endif
		SCR_REGISTER_UNUSED(SET_AUDIO_SCRIPT_VARIABLE,0x0c88a748e981ab40, CommandSetAudioScriptVariable);
		SCR_REGISTER_SECURE(ADD_ENTITY_TO_AUDIO_MIX_GROUP,0x2a375326652f1b50, CommandAddEntityToMixGroup);
		SCR_REGISTER_SECURE(REMOVE_ENTITY_FROM_AUDIO_MIX_GROUP,0x5545b7d117bb40b4, CommandRemoveEntityFromMixGroup);
		
		SCR_REGISTER_SECURE(AUDIO_IS_MUSIC_PLAYING,0x08255a6ed9b71dc9, CommandIsMusicPlaying);
		SCR_REGISTER_SECURE(AUDIO_IS_SCRIPTED_MUSIC_PLAYING,0xf1557162766fcdeb, CommandIsMusicPlaying);
				
		SCR_REGISTER_SECURE(PREPARE_MUSIC_EVENT,0x63b063064dc08617, CommandPrepareMusicEvent);
		SCR_REGISTER_SECURE(CANCEL_MUSIC_EVENT,0x0709e99f1ddfff26, CommandCancelMusicEvent);
		SCR_REGISTER_SECURE(TRIGGER_MUSIC_EVENT,0x1cf3f44fc2eb9f99, CommandTriggerMusicEvent);
		SCR_REGISTER_SECURE(IS_MUSIC_ONESHOT_PLAYING,0x41820d2cc10e7244, CommandIsMusicOneshotPlaying);
		SCR_REGISTER_UNUSED(IS_SCORE_MUTED_FOR_RADIO,0x52111705d9406c70, CommandIsScoreMutedForRadio);
		SCR_REGISTER_SECURE(GET_MUSIC_PLAYTIME,0xa4a087efa0fa28f5, CommandGetMusicPlayTime);
		SCR_REGISTER_SECURE(SET_GLOBAL_RADIO_SIGNAL_LEVEL,0x69a65bc53c5d08f7, CommandSetGlobalRadioSignalLevel);		
				
		//Glass
		SCR_REGISTER_SECURE(RECORD_BROKEN_GLASS,0x7a7c20063da9a0a4, CommandAddBrokenGlassArea);
		SCR_REGISTER_SECURE(CLEAR_ALL_BROKEN_GLASS,0x95ccae5d9e04405c, CommandClearAllGlassArea);
		//Weather 
		SCR_REGISTER_UNUSED(SET_WIND_TEMPERATURE,0x086aa060d2349956, CommandSetWindTemperature);
		SCR_REGISTER_UNUSED(SET_WIND_STRENGTH,0xfd595d4c52b54b28, CommandSetWindStrength);
		SCR_REGISTER_UNUSED(SET_WIND_BLUSTERY,0x88e5803bfdef631e, CommandSetWindBlustery);
		SCR_REGISTER_UNUSED(SCRIPT_OVERRIDES_WIND_TYPES,0x9531b2f767d64655, CommandScriptOverrideWindTypes);
		SCR_REGISTER_SECURE(SCRIPT_OVERRIDES_WIND_ELEVATION,0x31950ebe600e22b4, CommandScriptOverrideWindElevation);
		// Walla
		SCR_REGISTER_SECURE(SET_PED_WALLA_DENSITY,0xcb19e968c4d646d8,  CommandSetPedWallaDensity);
		SCR_REGISTER_SECURE(SET_PED_INTERIOR_WALLA_DENSITY,0xa39ca20de8266571,  CommandSetInteriorPedWallaDensity);
		SCR_REGISTER_UNUSED(SET_PED_WALLA_SOUND_SET,0x452c2e17f66b6478,  CommandSetPedWallaSoundSet);
		SCR_REGISTER_SECURE(FORCE_PED_PANIC_WALLA,0x941e500b05a5e04a, CommandForcePedPanic);
		// Alarms
		SCR_REGISTER_SECURE(PREPARE_ALARM,0x3e3727cdacee36ac, CommandPrepareAlarm);
		SCR_REGISTER_SECURE(START_ALARM,0x0eb18a2edd6957a4, CommandStartAlarm);
		SCR_REGISTER_SECURE(STOP_ALARM,0x837739686f969991, CommandStopAlarm);
		SCR_REGISTER_SECURE(STOP_ALL_ALARMS,0xc28bdd13a962277f, CommandStopAllAlarms);
		SCR_REGISTER_SECURE(IS_ALARM_PLAYING,0x83565bf2f64aa92e, CommandIsAlarmPlaying);

		SCR_REGISTER_SECURE(GET_VEHICLE_DEFAULT_HORN,0x125e8813edfab26f, CommandGetVehicleDefaultHorn);
		SCR_REGISTER_SECURE(GET_VEHICLE_DEFAULT_HORN_IGNORE_MODS,0x1ab7ba13996a24ab, CommandGetVehicleDefaultHornIgnoreMods);
		

		//Peds variations
		SCR_REGISTER_UNUSED(UPDATE_PED_CLOTHING_AND_SHOES,0x218b29a001d13495, CommandUpdatePedVariations);
		SCR_REGISTER_SECURE(RESET_PED_AUDIO_FLAGS,0x8a30e6dea2ac25bb, CommandResetPedAudioFlags);
		SCR_REGISTER_UNUSED(SET_PED_FOOTSTEP_MODE,0xdc92ebb7bbd43a08, CommandSetPedFootstepMode);
		SCR_REGISTER_UNUSED(SET_PED_WEARING_SCUBA_MASK,0xf148e0dcb2c2e775, CommandSetForcePedWearingScubaMask);
		SCR_REGISTER_SECURE(SET_PED_FOOTSTEPS_EVENTS_ENABLED,0xa4b6ad042af25671, CommandSetPedFootstepEventsEnabled);
		SCR_REGISTER_SECURE(SET_PED_CLOTH_EVENTS_ENABLED,0x3c012596f5866c99, CommandSetPedClothEventsEnabled);

		SCR_REGISTER_SECURE(OVERRIDE_PLAYER_GROUND_MATERIAL,0x383ad2d91b55328a, CommandOverridePlayerMaterial);
		SCR_REGISTER_SECURE(USE_FOOTSTEP_SCRIPT_SWEETENERS,0x610c6ede2d4627d9, CommandUseFoostepScriptSweeteners);

		SCR_REGISTER_UNUSED(SET_SNIPER_AUDIO_DISABLE,0xf9cd2627f62e7281, CommandSetSniperAudioDisable);
		SCR_REGISTER_SECURE(OVERRIDE_MICROPHONE_SETTINGS,0xf46237c6eb61354b, CommandOverrideMicrophoneSettings);
		SCR_REGISTER_SECURE(FREEZE_MICROPHONE,0xb579093da045578f, CommandFreezeMicrophone);
		//GUN FIGHTS
#if __BANK
		SCR_REGISTER_UNUSED(DRAW_GUN_FIGHT_GRID,0x92369b81bc791209, CommandDrawGunFightGrid);
		SCR_REGISTER_UNUSED(SHOOTER_POS_IN_SECTOR,0x6cdb2152a6472ef9, CommandShooterPosInSector);
#endif
		//CONDUCTORS
		SCR_REGISTER_SECURE(DISTANT_COP_CAR_SIRENS,0xcfde67f69da0b3e8, CommandPlayFakeDistantSiren);
		SCR_REGISTER_SECURE(SET_SIREN_CAN_BE_CONTROLLED_BY_AUDIO,0x1e081b80e395bb07, CommandSirenControlledByAudio);
		SCR_REGISTER_SECURE(ENABLE_STUNT_JUMP_AUDIO,0x27d7b460da73d437, CommandEnableStuntJump);
		

		SCR_REGISTER_SECURE(SET_AUDIO_FLAG,0x6ce6c3a402c3e20c, CommandSetAudioFlag);

		//SYNCHRONIZED SCENES
		SCR_REGISTER_SECURE(PREPARE_SYNCHRONIZED_AUDIO_EVENT,0xef91bc25caa31753, CommandPrepareSynchronizedAudioEvent);
		SCR_REGISTER_SECURE(PREPARE_SYNCHRONIZED_AUDIO_EVENT_FOR_SCENE,0x662a04c7bf4ceb42, CommandPrepareSynchronizedAudioEventForScene);
		SCR_REGISTER_SECURE(PLAY_SYNCHRONIZED_AUDIO_EVENT,0x0726c6430f767980, CommandPlaySynchronizedAudioEvent);
		SCR_REGISTER_SECURE(STOP_SYNCHRONIZED_AUDIO_EVENT,0xa6ada1dc68c9eaed, CommandStopSynchronizedAudioEvent);
		SCR_REGISTER_UNUSED(SET_SYNCH_SCENE_AUDIO_POSITION,0x51e330928c9467bd, CommandSetSynchSceneAudioPosition);
		SCR_REGISTER_UNUSED(SET_SYNCH_SCENE_AUDIO_ENTITY,0x26338e80420d8556, CommandSetSynchSceneEntity);

		SCR_REGISTER_SECURE(INIT_SYNCH_SCENE_AUDIO_WITH_POSITION,0xe39797060f976995, CommandInitSynchronizedSceneAudioWithPosition);
		SCR_REGISTER_SECURE(INIT_SYNCH_SCENE_AUDIO_WITH_ENTITY,0x0472288e3e06730d, CommandInitSynchronizedSceneAudioWithEntity);

		SCR_REGISTER_SECURE(SET_AUDIO_SPECIAL_EFFECT_MODE,0x085dc39d279ee40f, CommandSetAudioSpecialEffectMode);

		SCR_REGISTER_SECURE(SET_PORTAL_SETTINGS_OVERRIDE,0x67bc5545974932c4, CommandSetPortalSettingsOverride);
		SCR_REGISTER_UNUSED(SET_INDIVIDUAL_PORTAL_SETTINGS_OVERRIDE, 0x9f1b1b3c7678d953, CommandSetPerPortalSettingsOverride);
		SCR_REGISTER_SECURE(REMOVE_PORTAL_SETTINGS_OVERRIDE,0x3ca223e853957584, CommandRemovePortalSettingsOverride);
		SCR_REGISTER_UNUSED(REMOVE_INDIVIDUAL_PORTAL_SETTINGS_OVERRIDE, 0x21073d88283831f8, CommandRemovePerPortalSettingsOverride);

		SCR_REGISTER_SECURE(STOP_SMOKE_GRENADE_EXPLOSION_SOUNDS,0x3b6fc86679cadb21, CommandStopSmokeGrenadeExplosionSounds);
		SCR_REGISTER_UNUSED(TRIGGER_AIRCRAFT_WARNING_SPEECH,0x0d3ab7a8a025c96c, CommandTriggerAircraftWarningSpeech);

		SCR_REGISTER_UNUSED(GET_SFX_VOL_SLIDER,0x43feefa12a44859a, CommandGetSfxVolSlider);
		SCR_REGISTER_SECURE(GET_MUSIC_VOL_SLIDER,0x3f42e2c72fcd39ff, CommandGetMusicVolSlider);

		SCR_REGISTER_SECURE(REQUEST_TENNIS_BANKS,0xbf56337b6fca39c7, CommandRequestTennisBanks);
		SCR_REGISTER_SECURE(UNREQUEST_TENNIS_BANKS,0x440197ac4891d351, CommandUnrequestTennisBanks);


		SCR_REGISTER_SECURE(SET_SKIP_MINIGUN_SPIN_UP_AUDIO,0x4c4b1dd88e61c8ce, CommandSetSkipMinigunSpinUp);
		SCR_REGISTER_SECURE(STOP_CUTSCENE_AUDIO,0xca9941f698b36f0a, CommandStopCutsceneAudio);

		SCR_REGISTER_SECURE(HAS_LOADED_MP_DATA_SET,0x1f0893f56c568d39, CommandHasLoadedMPDataSet);
		SCR_REGISTER_SECURE(HAS_LOADED_SP_DATA_SET,0xc4e3c124e0d2ff08, CommandHasLoadedSPDataSet);

		SCR_REGISTER_SECURE(GET_VEHICLE_HORN_SOUND_INDEX,0x4841afe1a236e4e9, CommandGetVehicleHornSoundIndex);
		SCR_REGISTER_SECURE(SET_VEHICLE_HORN_SOUND_INDEX,0x2daf1526ef0058ee, CommandSetVehicleHornSoundIndex);

#if AUD_MUSIC_STUDIO_PLAYER
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_LOAD_SESSION,0xae2b613b2200c4be, CommandMusicStudioLoadSession);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_REQUEST_SECTION_TRANSITION,0x4c186c29148d3f70, CommandMusicStudioRequestSectionTransition);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_PART_VARIATION_INDEX,0x3d2741167fd60c57, CommandMusicStudioSetPartVariationIndex);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_PART_VOLUME,0x13f24d6072f69481, CommandMusicStudioSetPartVolume);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_START_PLAYBACK,0x9440720141a3cccc, CommandMusicStudioStartPlayback);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_STOP_PLAYBACK,0x1077459c2b854375, CommandMusicStudioStopPlayback);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_UNLOAD_SESSION,0x3e244cb92e1a00d0, CommandMusicStudioUnloadSession);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_PART_HPF,0xe014570f63ec2091, CommandMusicStudioSetPartHPF);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_PART_LPF,0xb0b6a09783a662e8, CommandMusicStudioSetPartLPF);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_GET_TIMING_INFO,0x7eff15c2c4e36703, CommandMusicStudioGetTimingInfo);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_IS_SESSION_PLAYING,0xc18bd9a2fc5bc4ee, CommandMusicStudioIsSessionPlaying);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_REQUEST_DELAYED_SETTINGS,0xb7123dd20edcf378, CommandMusicStudioRequestDelayedSettings);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_GET_ACTIVE_SECTION, 0x06ffb0826108fb93, CommandMusicStudioGetActiveSection);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_GET_LOADED_SESSION_HASH, 0x71ddcd5b080847d1, CommandMusicStudioGetLoadedSessionHash);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_SOLO_STATE, 0x7cab158440703153, CommandMusicStudioSetSoloState);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_MUTE_STATE, 0x263d3b2cdbb7ee12, CommandMusicStudioSetMuteState);
		SCR_REGISTER_UNUSED(MUSIC_STUDIO_SET_OCCLUSION_ENABLED, 0xb7f7ff34e54daf22, CommandMusicStudioSetOcclusionEnabled);
#endif // AUD_MUSIC_STUDIO_PLAYER

	}
}	//	end of namespace audio_commands
