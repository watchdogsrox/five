// 
// audio/radiotrack.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#include "radiotrack.h"

#if NA_RADIO_ENABLED

#include "northaudioengine.h"
#include "radioaudioentity.h"
#include "radiostation.h"

#include "audio/scriptaudioentity.h"
#include "audioengine/engine.h"
#include "audioengine/soundfactory.h"
#include "audioengine/soundmanager.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverdefs.h"
#include "audiosoundtypes/simplesound.h"
#include "audiosoundtypes/streamingsound.h"
#include "audiosoundtypes/sounddefs.h"
#include "audiosoundtypes/externalstreamsound.h"
#include "control/replay/ReplaySettings.h"
#include "debugaudio.h"

AUDIO_MUSIC_OPTIMISATIONS()

s32 g_TrackReleaseTime = -1;
//leave this on for now since hopefully the root cause of continuous spew has been fixed
bool g_WarnOnPrepareProbs = true;

//
// PURPOSE
//	Radio track state enum, used for virtualization management.
//
enum
{
	AUD_TRACK_STATE_DORMANT,
	AUD_TRACK_STATE_VIRTUAL,
	AUD_TRACK_STATE_STARTING_PHYSICAL,
	AUD_TRACK_STATE_PHYSICAL,
	AUD_TRACK_STATE_STOPPING_PHYSICAL
};

const u32 g_RadioDjMarkerCategoryHash = ATSTRINGHASH("DJ", 0x008dba0f8);
const u32 g_RadioDjMarkerNameIntroStartHash = ATSTRINGHASH("INTRO_START", 0x0449687ba);
const u32 g_RadioDjMarkerNameIntroEndHash = ATSTRINGHASH("INTRO_END", 0x02cc6e05e);
const u32 g_RadioDjMarkerNameOutroStartHash = ATSTRINGHASH("OUTRO_START", 0x07dce9fa0);
const u32 g_RadioDjMarkerNameOutroEndHash = ATSTRINGHASH("OUTRO_END", 0x071d8dcad);
const u32 g_RadioTrackIdCategoryHash = ATSTRINGHASH("TRACKID", 0x0a6d93246);

float audRadioTrack::sm_LargeReverbMax = 0.f;
float audRadioTrack::sm_MediumReverbMax = 0.f;
float audRadioTrack::sm_SmallReverbMax = 0.f;
float audRadioTrack::sm_VolumeMax = -100.f;

float audRadioTrack::sm_LargeReverbMaxPrinted = 0.6f;
float audRadioTrack::sm_MediumReverbMaxPrinted = 0.6f;
float audRadioTrack::sm_SmallReverbMaxPrinted = 0.6f;
float audRadioTrack::sm_VolumeMaxPrinted = 19.f;

audRadioTrack::audRadioTrack()
{
	WIN32PC_ONLY(m_MediaReader = NULL);

	Reset();
	ShutdownUserMusic();
}

void audRadioTrack::Init(const s32 category, const s32 trackIndex, const u32 soundRef, const float startOffset, const bool isMixStationTrack, const bool isReverbStationTrack, const u32 stationNameHash)
{
	naAssertf(!m_IsInitialised, "Attempting to init audRadioTrack but it's already been initialised");
	for(u32 emitterIndex=0; emitterIndex<g_NumRadioStationShadowedEmitters; emitterIndex++)
	{
		naAssertf(!m_ShadowedEmitterSounds[emitterIndex], "Emmiter sound at index %d is not null during init", emitterIndex);
	}
	Reset();

	m_IsReverbStationTrack = isReverbStationTrack;
	m_IsMixStationTrack = isMixStationTrack;
	m_Category = category;
	m_TrackIndex = trackIndex;
	m_SoundRef = soundRef;
	m_TimePlayed = 0;
	m_StationNameHash = stationNameHash;
	m_IsFlyloPart1 = false;

#if __WIN32PC
	m_IsUserTrack = IsUserTrackSound(soundRef);
	m_StreamerStartOffset = 0;
#endif

	if(soundRef == ATSTRINGHASH("RADIO_14_DANCE_02_FLYLO_PART1", 0x146B859F) ||
		soundRef == ATSTRINGHASH("RADIO_14_DANCE_02_FLYLO_PART2", 0x45D4E879))
	{
		m_StereoVolumeOffset = 0.5f;
		m_IsFlyloPart1 = true;
	}
	else if(soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P1", 0xF8C7E995) ||
		soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P2", 0xF7B1703) ||
		soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P3", 0x7DBC7384) ||
		soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P3_Start", 0x708B73A2) ||
		soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P3_Alt", 0xD56A23A8) ||
		soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P4", 0x69FF4C0A))
	{
		m_StereoVolumeOffset = 1.f;
	}
	else
	{
		m_StereoVolumeOffset = 0.f;
	}
		
	m_IsInitialised = true;
	m_HasParsedMarkers = false;
	
	m_RefForHistory = MakeTrackId(category, (u32)trackIndex, soundRef);

	if(!m_IsUserTrack) 
	{
		const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(soundRef);
		if(naVerifyf(streamingSound, "Unable to get streaming metadata from track metadata during init: %s (%u) / Category: %s index %u", SOUNDFACTORY.GetMetadataManager().GetObjectName(soundRef), soundRef, TrackCats_ToString((TrackCats)category), trackIndex))
		{			
			m_Duration = streamingSound->Duration;
		}			

		if(soundRef == ATSTRINGHASH("RADIO_13_JAZZ_WWFM_P3_Alt", 0xD56A23A8))
		{
			enum {WWFMStartOffset = 278982};
			m_PlayTime = (s32)(WWFMStartOffset + (m_Duration - WWFMStartOffset) * startOffset);
		}
		else
		{
			m_PlayTime = (s32)(m_Duration * startOffset);
		}

		m_PlayTimeCalculationMixerFrame = audDriver::GetMixer()->GetMixerTimeFrames();
	}
	else
	{
		m_PlayTime = 0;
		m_PlayTimeCalculationMixerFrame = audDriver::GetMixer()->GetMixerTimeFrames();
#if RSG_PC
		m_Duration = audRadioStation::GetUserRadioTrackManager()->GetTrackDuration(trackIndex);
#else
		m_Duration = 0;
#endif
	}

	// Cache start offset for playtime adjustment later
	m_StartOffset = (s32)m_PlayTime;

	char textIdObjName[16];
	formatf(textIdObjName, "RTT_%08X", audRadioTrack::GetBeatSoundName(soundRef));
	const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);
	if(textIds)
	{
		m_FoundTrackTextIds = true;
		for(u32 i = 0; i < textIds->numTextIds; i++)
		{
			if(!m_TextIds.IsFull())
			{
				audTextIdMarker &m = m_TextIds.Append();
				m.playtimeMs = textIds->TextId[i].OffsetMs;
				m.textId = textIds->TextId[i].TextId;
			}
			else
			{
				audWarningf("%s has too many TextIDs: %d vs %d", GetBankName(), textIds->numTextIds, kMaxTrackTextIds);
				break;
			}
		}
	}
	formatf(textIdObjName, "RTB_%08X", audRadioTrack::GetBeatSoundName(soundRef));
	const RadioTrackTextIds *beatMarkers = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);
	if(beatMarkers)
	{
		m_FoundBeatMarkers = true;
		for(u32 i = 0; i < beatMarkers->numTextIds; i++)
		{
			if(!m_BeatMarkers.IsFull())
			{
				BeatMarker &m = m_BeatMarkers.Append();
				m.numBeats = beatMarkers->TextId[i].TextId;			
				m.playtimeMs = beatMarkers->TextId[i].OffsetMs;
			}
			else
			{
				audWarningf("%s has too many beat markers (%d vs %d)", GetBankName(), beatMarkers->numTextIds, kMaxBeatMarkers);
				break;
			}
		}
	}
}

u32 audRadioTrack::GetBeatSoundName(u32 soundHash)
{
	u32 beatSoundHash = soundHash;

	// url:bugstar:6812963 - Kult FM BPM's are slow in-game
	// Manually override the track IDs for Kult FM tracks (requireda s we updated the sound .xsl generator to cope with the 
	// unique bank layout, but not the track ID generator .dll)
	switch (soundHash)
	{
	case 0x3597022E /*HEI4_RADIO_KULT_AGE_OF_CONSENT*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_AGE_OF_CONSENT", 0x203DD604); break;
	case 0x7CA143C0 /*HEI4_RADIO_KULT_ALIEN_CRIME_LORD*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_ALIEN_CRIME_LORD", 0xD6EFB84F); break;
	case 0x69469669 /*HEI4_RADIO_KULT_BABY_I_LOVE_YOU_SO*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_BABY_I_LOVE_YOU_SO", 0xA7772305); break;
	case 0xAF7E2F3A /*HEI4_RADIO_KULT_CYCLE*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_CYCLE", 0x2247DFEC); break;
	case 0x5212E8BE /*HEI4_RADIO_KULT_DEEP*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_DEEP", 0x3303D8D0); break;
	case 0x45F1D4D4 /*HEI4_RADIO_KULT_DOWN_ON_THE_STREET*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_DOWN_ON_THE_STREET", 0xCDD7AD08); break;
	case 0xE4A2BC39 /*HEI4_RADIO_KULT_DRAB_MEASURE*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_DRAB_MEASURE", 0xDCC1D3B0); break;
	case 0x4CF2CE52 /*HEI4_RADIO_KULT_EISBAR*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_EISBAR", 0xB94A8B0E); break;
	case 0xB0927F93 /*HEI4_RADIO_KULT_FACELESS*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_FACELESS", 0x27727CB); break;
	case 0xD1A00144 /*HEI4_RADIO_KULT_FOUR_SHADOWS*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_FOUR_SHADOWS", 0x25A41D9B); break;
	case 0x49AB9DC6 /*HEI4_RADIO_KULT_GIRLS_AND_BOYS*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_GIRLS_AND_BOYS", 0x84AA491C); break;
	case 0xDFB0C335 /*HEI4_RADIO_KULT_GOING_BACK_TO_CALI*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_GOING_BACK_TO_CALI", 0xC86AC638); break;
	case 0x9C4BFDD0 /*HEI4_RADIO_KULT_HARD_TO_EXPLAIN*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_HARD_TO_EXPLAIN", 0xC66479AB); break;
	case 0xD968602A /*HEI4_RADIO_KULT_HUMAN_FLY*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_HUMAN_FLY", 0xBCAE2A58); break;
	case 0x8925D95E /*HEI4_RADIO_KULT_ITS_YOURS*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_ITS_YOURS", 0xBEEAD8BE); break;
	case 0xAF409C04 /*HEI4_RADIO_KULT_LFT_ME_LONELY*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_LFT_ME_LONELY", 0x535A265F); break;
	case 0xB98381AF /*HEI4_RADIO_KULT_LIEBE_AUF_DEN_ERSTEN_BLICK*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_LIEBE_AUF_DEN_ERSTEN_BLICK", 0xE9583693); break;
	case 0xA23BA519 /*HEI4_RADIO_KULT_MANY_TEARS_AGO*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_MANY_TEARS_AGO", 0xA62E5143); break;
	case 0x36CC3FE5 /*HEI4_RADIO_KULT_NIGHTCLUBBING*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_NIGHTCLUBBING", 0x95067767); break;
	case 0xD70C78A2 /*HEI4_RADIO_KULT_ON_THE_LEVEL*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_ON_THE_LEVEL", 0x68774DA0); break;
	case 0x256500C1 /*HEI4_RADIO_KULT_POOL_SONG*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_POOL_SONG", 0x507B8473); break;
	case 0xBB0D511E /*HEI4_RADIO_KULT_RAGA_MADHUVANTI*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_RAGA_MADHUVANTI", 0xE1AD1A43); break;
	case 0x5E34F34A /*HEI4_RADIO_KULT_ROCK_AND_ROLL*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_ROCK_AND_ROLL", 0xE8D5BEEF); break;
	case 0x669D5285 /*HEI4_RADIO_KULT_SHES_LOST_CONTROL*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_SHES_LOST_CONTROL", 0x148B6243); break;
	case 0xBF6EA304 /*HEI4_RADIO_KULT_SO_IT_GOES*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_SO_IT_GOES", 0x50E931A1); break;
	case 0x425996B3 /*HEI4_RADIO_KULT_TAINTED_LOVE*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_TAINTED_LOVE", 0x91CAD1CF); break;
	case 0xB838EB1B /*HEI4_RADIO_KULT_TAKE_DOWN_THE_HOUSE*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_TAKE_DOWN_THE_HOUSE", 0xDC0DCC52); break;
	case 0x9DA1CA5B /*HEI4_RADIO_KULT_THE_ADULTS_ARE_TALKING*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_THE_ADULTS_ARE_TALKING", 0xEBD4B927); break;
	case 0xA8AB4150 /*HEI4_RADIO_KULT_THE_ETERNAL_TAU*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_THE_ETERNAL_TAU", 0x6702A27D); break;
	case 0xC658DE05 /*HEI4_RADIO_KULT_THIS_IS_THE_DAY*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_THIS_IS_THE_DAY", 0xFFCF0A95); break;
	case 0x82A07F0  /*HEI4_RADIO_KULT_TIME_BOMB*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_TIME_BOMB", 0x1E297FDF); break;
	case 0xEE49D6D7 /*HEI4_RADIO_KULT_TOO_MUCH_MONEY*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_TOO_MUCH_MONEY", 0x6E30006B); break;
	case 0xD010E265 /*HEI4_RADIO_KULT_TV_CASUALTY*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_TV_CASUALTY", 0x6D73F8A4); break;
	case 0xE36F4E9B /*HEI4_RADIO_KULT_TYPICAL_GIRLS*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_TYPICAL_GIRLS", 0x6C1213CA); break;
	case 0xC271F39A /*HEI4_RADIO_KULT_WHERE_NO_EAGLES_FLY*/: beatSoundHash = ATSTRINGHASH("DLC_HEI4_MUSIC_WHERE_NO_EAGLES_FLY", 0xCB712A21); break;
	default: break;
	}

	return beatSoundHash;
}

u32 audRadioTrack::FindTextIdForSoundHash(const u32 soundHash)
{
	char textIdObjName[16];
	formatf(textIdObjName, "RTT_%08X", audRadioTrack::GetBeatSoundName(soundHash));
	const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);
	if(textIds)
	{
		return textIds->TextId[0].TextId;
	}

	// Score tracks have a different setup; we need to find the bank name
	class BankListBuilderFn : public audSoundFactory::audSoundProcessHierarchyFn
	{
	public:
		u32 bankId;
		BankListBuilderFn() 
		{
			bankId = AUD_INVALID_BANK_ID;
		}

		void operator()(u32 classID, const void* soundData)
		{		
			if(classID == SimpleSound::TYPE_ID)
			{
				const SimpleSound * sound = reinterpret_cast<const SimpleSound*>(soundData);
				bankId = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->WaveRef.BankName);
			}
		}
	};

	BankListBuilderFn bankListBuilder;
	SOUNDFACTORY.ProcessHierarchy(soundHash, bankListBuilder);
	if(bankListBuilder.bankId < AUD_INVALID_BANK_ID)
	{
		const char *bankName = g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankListBuilder.bankId);
		char modifiedBankName[64];
		safecpy(modifiedBankName, bankName);
		const size_t len = strlen(modifiedBankName);
		for(size_t i = 0; i < len; i++)
		{
			if(modifiedBankName[i] == '/' || modifiedBankName[i] == '\\')
			{
				modifiedBankName[i] = '_';
			}
		}
		formatf(textIdObjName, "RTT_%08X", atStringHash(modifiedBankName));
		const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(textIdObjName);
		if(textIds)
		{
			return textIds->TextId[0].TextId;
		}
	}

	BANK_ONLY(audWarningf("Failed to find TextId for %u (%s)", soundHash, SOUNDFACTORY.GetMetadataManager().GetObjectName(soundHash)));
	return 1;
}

bool audRadioTrack::IsScoreTrack(const u32 soundHash)
{
	const u32 textTrackId = FindTextIdForSoundHash(soundHash);
	return textTrackId >= kFirstScoreTrackId && textTrackId < kFirstCommercialTrackId;
}

bool audRadioTrack::IsCommercial(const u32 soundHash)
{
	const u32 textTrackId = FindTextIdForSoundHash(soundHash);
	return textTrackId >= kFirstCommercialTrackId && textTrackId < kFirstSFXTrackId;
}

bool audRadioTrack::IsSFX(const u32 soundHash)
{
	const u32 textTrackId = FindTextIdForSoundHash(soundHash);
	return textTrackId >= kFirstSFXTrackId;
}

bool audRadioTrack::IsAmbient(const u32 soundHash)
{
	const u32 textTrackId = FindTextIdForSoundHash(soundHash);
	return textTrackId >= kFirstAmbinetTrackId;
}

#if __WIN32PC
bool audRadioTrack::IsUserTrackSound(const u32 soundRef)
{
	return (soundRef == ATSTRINGHASH("DLC_RADIO_19_USER_NULLTRACK", 0xC96DC433));
}
#endif // __WIN32PC

void audRadioTrack::Shutdown(void)
{
#if __WIN32PC
	if(m_IsUserTrack && m_State == AUD_TRACK_STATE_PHYSICAL && m_StreamingSound && m_StreamingSound->GetPlayState() == AUD_SOUND_PLAYING)
	{
		m_StreamingSound->SetReleaseTime(500);
	}
#endif /// __WIN32PC

	if(m_StreamingSound)
	{
		m_StreamingSound->StopAndForget();
	}
	
	//Ensure we have cleaned up any active shadow sound positioned emitters.
	for(u8 emitterIndex=0; emitterIndex<g_NumRadioStationShadowedEmitters; emitterIndex++)
	{
		if(m_ShadowedEmitterSounds[emitterIndex] != NULL)
		{
			m_ShadowedEmitterSounds[emitterIndex]->StopAndForget();
		}
	}

	ShutdownUserMusic();
	Reset();
}

void audRadioTrack::Reset(void)
{
	m_Category = NUM_RADIO_TRACK_CATS;
	m_SoundRef = 0;
	m_SoundBucketId = 0xff;
	m_StreamingSound = NULL;
	m_WaveSlot = NULL;
	m_PlayTime = 0;
	m_PlayTimeCalculationMixerFrame = 0;
	m_Duration = 0;
	m_LastUpdateTime = 0;
	m_State = AUD_TRACK_STATE_DORMANT;
	m_IsInitialised = false;
	m_IsUserTrack = false;

	memset(m_ShadowedEmitterSounds.GetElements(), 0, g_NumRadioStationShadowedEmitters * sizeof(audSound *));
	m_DjRegions.Reset();
	m_TextIds.Reset();
	m_BeatMarkers.Reset();

	m_Bpm = 0.f;
	m_BeatNumber = 0;
	m_BeatTimeS = 0.f;

	m_FoundTrackTextIds = false;
	m_FoundBeatMarkers = false;
	m_StereoVolumeOffset = 0.f;

#if __WIN32PC
	m_MakeUpGain = 0.f;
	m_PostRoll = 0;	
#endif
}

void audRadioTrack::ShutdownUserMusic()
{
#if __WIN32PC
	if(m_IsUserTrack)
	{
		if(m_MediaReader)
		{
			audMediaReader::DestroyMediaStreamer(m_MediaReader);
		}
		m_MediaReader = NULL;
	}
#endif
	//m_IsUserTrack = false;
}

void audRadioTrack::Update(u32 timeInMs, bool isFrozen)
{
	audPrepareState prepareState;
	switch(m_State)
	{
		case AUD_TRACK_STATE_VIRTUAL:
			if(!isFrozen)
			{
				if(m_LastUpdateTime == 0)
				{
					m_LastUpdateTime = timeInMs;
				}
				// compute virtual playtime
				m_PlayTime += timeInMs - m_LastUpdateTime; 
				m_PlayTimeCalculationMixerFrame = audDriver::GetMixer()->GetMixerTimeFrames();
				naAssertf(GetDuration() > 0, "Radio track with 0 duration. sound name: %s hash: %u", m_StreamingSound ? m_StreamingSound->GetName() : "(null)", m_SoundRef);

				if(m_PlayTime >= (s32)GetDuration())
				{
					//We are past the end of the track, so shutdown.
					Shutdown();
				}
			}

			//Ensure we have cleaned up any active shadow sound positioned emitters.
			for(u8 emitterIndex=0; emitterIndex<g_NumRadioStationShadowedEmitters; emitterIndex++)
			{
				if(m_ShadowedEmitterSounds[emitterIndex] != NULL)
				{
					m_ShadowedEmitterSounds[emitterIndex]->StopAndForget();
				}
			}
			break;

		case AUD_TRACK_STATE_STARTING_PHYSICAL:
			// we lose time when waiting for the track to physically prepare; compensate for some of that
			enum { PrepareTimeEstimate = 500 }; 
			prepareState = Prepare((u32)m_PlayTime + (isFrozen? 0u : PrepareTimeEstimate));

			switch(prepareState)
			{
				case AUD_PREPARED:
					//Physically play our track sound.
					if(!isFrozen)
					{
						if(m_IsUserTrack)
						{
#if __WIN32PC
							Assert(m_StreamingSound->GetSoundTypeID() == ExternalStreamSound::TYPE_ID);
							audExternalStreamSound *str = reinterpret_cast<audExternalStreamSound*>(m_StreamingSound);	
							u32 usamplerate = m_MediaReader->GetSampleRate();
							str->InitStreamPlayer(m_MediaReader->m_RingBuffer, 2, usamplerate); // for now for stereo
							m_StreamingSound->PrepareAndPlay();
#endif
						}
						else
							m_StreamingSound->Play();

						m_State = AUD_TRACK_STATE_PHYSICAL;
					}
					break;

				case AUD_PREPARE_FAILED:
					Shutdown();
					break;

				//Intentional fall-through.
				case AUD_PREPARING:
				default:
					break;
			}			
			break;

		case AUD_TRACK_STATE_PHYSICAL:
			if(m_StreamingSound)
			{
	
				if(m_IsUserTrack) 
				{
#if __WIN32PC					
					m_PlayTime = m_StreamerStartOffset + ((audExternalStreamSound*)m_StreamingSound)->GetCurrentPlayTimeOfWave();
					m_PlayTimeCalculationMixerFrame = audDriver::GetMixer()->GetMixerTimeFrames();
					
					if(m_ShadowedEmitterSounds[0] == NULL && m_StreamingSound->GetPlayState() == AUD_SOUND_PLAYING)
					{	
						const s32 pcmSourceId = ((audExternalStreamSound*)m_StreamingSound)->GetStreamPlayerId();
						CreateShadowEmitterSounds(pcmSourceId, pcmSourceId, pcmSourceId);
					}

					// Check if we're beyond duration due to post-roll.
					if(m_PlayTime >= GetDuration())
					{
						Shutdown();
					}
#endif
				}
				else
				{
					m_PlayTime = ((audStreamingSound*)m_StreamingSound)->GetCurrentPlayTimeOfWave();
					m_PlayTimeCalculationMixerFrame = ((audStreamingSound*)m_StreamingSound)->GetPlayTimeCalculationMixerFrame();

					//Ensure we have active shadow sound positioned emitters.
					// Wait until the streaming sound has been played to prevent the shadow sounds starting before the streaming sound
					if(m_ShadowedEmitterSounds[0] == NULL && m_StreamingSound->GetPlayState() == AUD_SOUND_PLAYING)
					{
						const s32 pcmSourceIdL = ((audStreamingSound*)m_StreamingSound)->GetFirstWavePlayerId();
						const s32 pcmSourceIdR = ((audStreamingSound*)m_StreamingSound)->GetWavePlayerIdCount() > 1 ? ((audStreamingSound*)m_StreamingSound)->GetWavePlayerId(1) : pcmSourceIdL;
						const s32 pcmSourceIdReverb = ((audStreamingSound*)m_StreamingSound)->GetWavePlayerIdCount() > 2 ? ((audStreamingSound*)m_StreamingSound)->GetWavePlayerId(2) : pcmSourceIdL;
						CreateShadowEmitterSounds(pcmSourceIdL, pcmSourceIdR, pcmSourceIdReverb);
					}

					if(!m_HasParsedMarkers)
					{
						ParseMarkers();											
					} 
					UpdateBeatInfo();
				}	
			}
			else
			{
				//Our sounds have finished playing, so shutdown.
				Shutdown();
			}
			break;

		case AUD_TRACK_STATE_STOPPING_PHYSICAL:
			//Stop our track sound and clear up our sound references.
			if(m_StreamingSound)
			{
				// grab the latest play time
#if __WIN32PC
				if(m_IsUserTrack && m_StreamingSound)
				{
					m_PlayTime = m_StreamerStartOffset + ((audExternalStreamSound*)m_StreamingSound)->GetCurrentPlayTimeOfWave();
					m_PlayTimeCalculationMixerFrame = audDriver::GetMixer()->GetMixerTimeFrames();
				}
				else
#endif
				{
					m_PlayTime = ((audStreamingSound*)m_StreamingSound)->GetCurrentPlayTimeOfWave();		
					m_PlayTimeCalculationMixerFrame = ((audStreamingSound*)m_StreamingSound)->GetPlayTimeCalculationMixerFrame();		
				}
				m_StreamingSound->StopAndForget();
			}
			ShutdownUserMusic();

			m_State = AUD_TRACK_STATE_VIRTUAL;
			break;

		case AUD_TRACK_STATE_DORMANT:	//Intentional fall-through.
		default:
			break;
	}

	m_LastUpdateTime = timeInMs;
}

void audRadioTrack::CreateShadowEmitterSounds(const s32 pcmSourceIdL, const s32 pcmSourceIdR, const s32 pcmSourceIDReverb)
{
	audSoundInitParams initParams;
	Assert(-1 != pcmSourceIdL && -1 != pcmSourceIdR);
	initParams.TimerId = 2;

	for(u32 emitterIndex=0; emitterIndex<g_NumRadioStationShadowedEmitters; emitterIndex++)
	{
		if(m_ShadowedEmitterSounds[emitterIndex] == NULL)
		{			
			const u32 shadowSoundNameHash = (g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::EnableGameplayCriticalMusicEmitters) && m_IsMixStationTrack) ? ATSTRINGHASH("GamePlayCriticalShadowEmitter", 0xF6CA611F) : ATSTRINGHASH("ShadowEmitter", 0x83B2D715);

			if (pcmSourceIDReverb == pcmSourceIdL)
			{
				initParams.ShadowPcmSourceId = (emitterIndex & 1) == 1 ? pcmSourceIdR : pcmSourceIdL;
			}
			else
			{
				u32 soundIndex = emitterIndex % 3;

				if (soundIndex == 0)
				{
					initParams.ShadowPcmSourceId = pcmSourceIdL;
				}
				else if (soundIndex == 1)
				{
					initParams.ShadowPcmSourceId = pcmSourceIdR;
				}
				else
				{
					initParams.ShadowPcmSourceId = pcmSourceIDReverb;
				}				
			}

			g_RadioAudioEntity.CreateAndPlaySound_Persistent(g_RadioAudioEntity.GetRadioSounds().Find(shadowSoundNameHash), &(m_ShadowedEmitterSounds[emitterIndex]), &initParams);

			if (m_ShadowedEmitterSounds[emitterIndex] && m_IsReverbStationTrack)
			{
				m_ShadowedEmitterSounds[emitterIndex]->SetRequestedShouldDisableRearFiltering(AUD_TRISTATE_TRUE);
			}
		}
	}
}

u32 audRadioTrack::GetTextId() const
{
#if RSG_PC
	if(m_IsUserTrack)
	{		
		return GetUserTrackTextIdFromIndex(m_TrackIndex);
	}
#endif
	return GetTextId(static_cast<u32>(m_PlayTime));
}

u32 audRadioTrack::GetTextId(const u32 playTimeMs) const
{
#if RSG_PC
	if(m_IsUserTrack)
	{
		return GetUserTrackTextIdFromIndex(m_TrackIndex);
	}
#endif
	u32 textId = 1;
	for(u32 i = 0; i < m_TextIds.GetCount(); i++)
	{
		if(playTimeMs >= m_TextIds[i].playtimeMs)
		{
			textId = m_TextIds[i].textId;
		}
		else if(m_TextIds[i].playtimeMs > playTimeMs)
		{
			// may aswell stop the search; markers will be ordered
			return textId;
		}
	}
	return textId;
}

audPrepareState audRadioTrack::Prepare(u32 startOffset)
{
#if __WIN32PC
	if(m_IsUserTrack)
		return PrepareUserTrack(startOffset);
#endif

	if(m_StreamingSound == NULL)
	{
		// Trying to play physically with no waveslot? Parent hasn't managed to allocate us a streaming slot, so mark as preparing
		// so that we try again
		if(!m_WaveSlot)
		{
			audWarningf("Track %u trying to start physically with no waveslot", m_SoundRef);
			return AUD_PREPARING;
		}

		// if the radio timer is paused then we wont be clearing out old sounds, so we also don't want to be creating new ones
		if(!g_AudioEngine.GetSoundManager().IsPaused(2))
		{
			//Create a new streaming sound.
			audSoundInitParams initParams;
			initParams.BucketId = m_SoundBucketId;
			initParams.TimerId = 2;		
			initParams.StartOffset = startOffset;
			// For now we can't virtualise streams
			initParams.ShouldPlayPhysically = true;
			
			// Start muted
			initParams.Volume = -100.f;
			
			const u32 trackSoundHash = m_SoundRef;
			g_RadioAudioEntity.CreateSound_PersistentReference(trackSoundHash, (audSound**)&m_StreamingSound, &initParams);
			if(m_StreamingSound == NULL || m_StreamingSound->GetSoundTypeID() != StreamingSound::TYPE_ID)
			{
				if(!m_StreamingSound)
				{
					naWarningf("Failed to create radio track sound %u", trackSoundHash);
				}
				else
				{
					naWarningf("radio track sound not of type streamingsound: %u", trackSoundHash);
				}
				return AUD_PREPARE_FAILED;
			}
			
			if (m_IsReverbStationTrack)
			{
				m_StreamingSound->SetRequestedShouldDisableRearFiltering(AUD_TRISTATE_TRUE);
			}
			
			m_Duration = m_StreamingSound->ComputeDurationMsExcludingStartOffsetAndPredelay(NULL);
		}
		else
		{
			// if we've not created our sound and the radio timer is paused then we're still preparing
			return AUD_PREPARING;
		}
	}

	audPrepareState state = m_StreamingSound->Prepare(m_WaveSlot, true);

	if(state != AUD_PREPARED && g_WarnOnPrepareProbs)
	{
		if(m_WaveSlot && m_WaveSlot->GetReferenceCount() > 0)
		{
			naDebugf1("Radio is trying to stream into a wave slot with %u references: %s (currently loaded bank: %s). Prepare state %d.", m_WaveSlot->GetReferenceCount(), m_WaveSlot->GetSlotName(), m_WaveSlot->GetLoadedBankName(), state);
		}
		else if(!m_WaveSlot)
		{
			naWarningf("Radio preparing into NULL slot");	
			return AUD_PREPARE_FAILED;
		}
	}

	return state;
}

#if __WIN32PC
audPrepareState audRadioTrack::PrepareUserTrack(u32 startOffset)
{
	if(m_StreamingSound == NULL)
	{
		m_IsUserTrack = true;
		// Trying to play physically with no waveslot? Parent hasn't managed to allocate us a streaming slot, so mark as preparin
		// so that we try again
		//if(!m_WaveSlot)
		//{
		//	return AUD_PREPARING;
		//}

		// if the radio timer is paused then we wont be clearing out old sounds, so we also don't want to be creating new ones
		if(!g_AudioEngine.GetSoundManager().IsPaused(2))
		{
			//Create a new streaming sound.
			audSoundInitParams initParams;
			//initParams.BucketId = m_SoundBucketId;
			initParams.TimerId = 2;		
			initParams.StartOffset = 0;// startOffset;
			// For now we can't virtualise streams
			initParams.ShouldPlayPhysically = true;
			
			// Start muted
			initParams.Volume = -100.f;

			const audMetadataRef trackSoundHash = g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("UserMusicSound", 0xE9DE2CD9));
			g_RadioAudioEntity.CreateSound_PersistentReference(trackSoundHash, (audSound**)&m_StreamingSound, &initParams);
			if(m_StreamingSound == NULL || m_StreamingSound->GetSoundTypeID() != ExternalStreamSound::TYPE_ID)
			{
				if(!m_StreamingSound)
				{
					naWarningf("Failed to create radio track sound %u", trackSoundHash);
				}
				else
				{
					naWarningf("radio track sound not of type ExternalStreamSound: %u", trackSoundHash);
				}
				return AUD_PREPARE_FAILED;
			}
			
			audPrepareState state = CreateStreamer(startOffset);
			m_StreamerStartOffset = startOffset;
			if(state == AUD_PREPARE_FAILED)
			{
				return AUD_PREPARE_FAILED;
			}
		}
		else
		{
			// if we've not created our sound and the radio timer is paused then we're still preparing
			return AUD_PREPARING;
		}
	}

	if(m_MediaReader)
	{
		s32 user_state = m_MediaReader->GetState();
		if(user_state == TRACK_STATE_PREPARED)
		{
			m_Duration = m_MediaReader->GetStreamLengthMs();
			return AUD_PREPARED;
		}
		if(user_state == TRACK_STATE_FAILED) // we have a media reader, but the actual file is bad
		{
			///OMG better deal with this, shut down and try next track
			if(m_StreamingSound)
			{
				m_StreamingSound->StopAndForget();
			}
			if(m_MediaReader)
			{
				audMediaReader::DestroyMediaStreamer(m_MediaReader);
			}
			m_MediaReader = NULL;
			return AUD_PREPARE_FAILED;
		}
	}
	return AUD_PREPARING;
}

audPrepareState audRadioTrack::CreateStreamer(u32 startOffset)
{
	s32 track = m_TrackIndex;
	if(track != -1)
	{
		u32 trim = startOffset;
		Displayf("Preparing User Track : %s at %d", audRadioStation::GetUserRadioTrackManager()->GetFileName(track), startOffset);
		if(startOffset == 0)
		{
			trim = audRadioStation::GetUserRadioTrackManager()->GetStartOffsetMs(track);
		}
		SetGainAndPostRoll(audRadioStation::GetUserRadioTrackManager()->ComputeTrackMakeUpGain(track), audRadioStation::GetUserRadioTrackManager()->GetTrackPostRoll(track));
		m_MediaReader = audMediaReader::CreateMediaStreamer(audRadioStation::GetUserRadioTrackManager()->GetFileName(track), &m_StreamingSound, trim);
		if(!m_MediaReader)
		{
			naWarningf("Failed to create radio track user music audMediaReader: %s", 
				audRadioStation::GetUserRadioTrackManager()->GetFileName(track));
			return AUD_PREPARE_FAILED;
		}
	}
	else
	{
		naWarningf("Failed to create radio track user music audMediaReader, no valid track");
		return AUD_PREPARE_FAILED;
	}

	return AUD_PREPARING;
}
#endif

void audRadioTrack::PlayWhenReady()
{
	if(naVerifyf(m_State == AUD_TRACK_STATE_DORMANT, "Attempted to play a radio track with state %d when it should be in state %d (AUD_TRACK_STATE_DORMANT)", m_State, AUD_TRACK_STATE_DORMANT))
	{
#if __WIN32PC
		if(m_IsUserTrack)
		{
			m_State = AUD_TRACK_STATE_STARTING_PHYSICAL;
			return;
		}
#endif

		if(m_WaveSlot)
		{
			m_State = AUD_TRACK_STATE_STARTING_PHYSICAL;
		}
		else
		{
			m_State = AUD_TRACK_STATE_VIRTUAL;
		}
	}
}

void audRadioTrack::Play(void)
{
	if(naVerifyf(m_State == AUD_TRACK_STATE_DORMANT, "Attempted to play a radio track with state %d when it should be in state %d (AUD_TRACK_STATE_DORMANT)", m_State, AUD_TRACK_STATE_DORMANT))
	{
		if(m_StreamingSound)
		{
			//Physically play our track sound.
			if(m_IsUserTrack)
			{
#if __WIN32PC
				Assert(m_StreamingSound->GetSoundTypeID() == ExternalStreamSound::TYPE_ID);
				audExternalStreamSound *str = reinterpret_cast<audExternalStreamSound*>(m_StreamingSound);	
				u32 usamplerate = m_MediaReader->GetSampleRate();
				str->InitStreamPlayer(m_MediaReader->m_RingBuffer, 2, usamplerate); // for now for stereo
				m_StreamingSound->PrepareAndPlay();
#endif
			}
			else
				m_StreamingSound->Play();

			m_State = AUD_TRACK_STATE_PHYSICAL;
		}
		else
		{
			m_State = AUD_TRACK_STATE_VIRTUAL;
		}

		m_TimePlayed = audDriver::GetMixer()->GetMixerTimeMs();
	}
}

void audRadioTrack::SetPhysicalStreamingState(bool shouldStreamPhysically, audWaveSlot *waveSlot, u8 soundBucketId)
{
	if(shouldStreamPhysically)
	{
		m_WaveSlot = waveSlot;
		m_SoundBucketId = soundBucketId;

		//Assert(m_State != AUD_TRACK_STATE_STOPPING_PHYSICAL);
		if(m_State == AUD_TRACK_STATE_VIRTUAL)
		{
			m_State = AUD_TRACK_STATE_STARTING_PHYSICAL;
		}
		//else if(m_State == AUD_TRACK_STATE_DORMANT) - If this track is dormant, preparation should be handled explicitly by the radio station.
	}
	else
	{
		if(m_State == AUD_TRACK_STATE_PHYSICAL || m_State == AUD_TRACK_STATE_STARTING_PHYSICAL)
		{
			m_State = AUD_TRACK_STATE_STOPPING_PHYSICAL;
		}
		else if(m_State == AUD_TRACK_STATE_DORMANT)
		{
			//Ensure we clean up our sounds if we have created but not played them yet.
			if(m_StreamingSound)
			{
				m_StreamingSound->StopAndForget();
			}			
			ShutdownUserMusic();
		}
	}
}

void audRadioTrack::UpdateStereoEmitter(f32 volumeDb, f32 cutoff)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		volumeDb = g_SilenceVolume;
#if __WIN32PC
		m_MakeUpGain = 0.f;
#endif
	}
#endif

	if(m_StreamingSound)
	{
#if __WIN32PC
		m_StreamingSound->SetRequestedVolume(volumeDb + m_MakeUpGain);
#else
		m_StreamingSound->SetRequestedVolume(volumeDb);
#endif
		m_StreamingSound->SetRequestedLPFCutoff(static_cast<u32>(cutoff));
		m_StreamingSound->SetRequestedPostSubmixVolumeAttenuation(m_StereoVolumeOffset);
	}
}

void audRadioTrack::UpdatePositionedEmitter(const u32 emitterIndex, f32 emittedVolumeDb, f32 volumeOffsetDb, const u32 lpfCutoff, const u32 hpfCutoff, const Vector3 &position, const audEnvironmentGameMetric *occlusionMetric, const u32 emitterType, const f32 environmentalLoudness)
{
#if !__ASSERT
	(void)emitterType;
#endif

	f32 volumeDb = emittedVolumeDb + volumeOffsetDb;
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		volumeDb = g_SilenceVolume;
#if __WIN32PC
		m_MakeUpGain = 0.f;
#endif
	}
#endif

	naCErrorf(emitterIndex < g_NumRadioStationShadowedEmitters, "Emitter index passed in to UpdatePositionedEmitter (%d) is out of bounds", emitterIndex);
	if((emitterIndex < g_NumRadioStationShadowedEmitters) && m_ShadowedEmitterSounds[emitterIndex])
	{			
		audRequestedSettings *requestedSettings = m_ShadowedEmitterSounds[emitterIndex]->GetRequestedSettings();
	
		audEnvironmentGameMetric &reqSetMetric = requestedSettings->GetEnvironmentGameMetric();
		reqSetMetric = *occlusionMetric;
				
		reqSetMetric.SetReverbLarge(Clamp(reqSetMetric.GetReverbLarge(), 0.f, 1.f));
		reqSetMetric.SetReverbMedium(Clamp(reqSetMetric.GetReverbMedium(), 0.f, 1.f));
		reqSetMetric.SetReverbSmall(Clamp(reqSetMetric.GetReverbSmall(), 0.f, 1.f));

		sm_LargeReverbMax = Max(sm_LargeReverbMax, reqSetMetric.GetReverbLarge());
		sm_MediumReverbMax = Max(sm_MediumReverbMax, reqSetMetric.GetReverbMedium());
		sm_SmallReverbMax = Max(sm_SmallReverbMax, reqSetMetric.GetReverbSmall());
		sm_VolumeMax = Max(sm_VolumeMax, volumeDb);

#if RSG_BANK
		if(sm_LargeReverbMax >= sm_LargeReverbMaxPrinted + 1.f)
		{
			audWarningf("Large reverb max send level increased: %f (%s emitter %d type %d pos [%f,%f,%f]) "
				, sm_LargeReverbMax
				, m_StreamingSound ? m_StreamingSound->GetName() : "(NULL sound)"
				, emitterIndex
				, emitterType
				, position.x
				, position.y
				, position.z
				);

			sm_LargeReverbMaxPrinted = sm_LargeReverbMax;
		}
		if(sm_MediumReverbMax >= sm_MediumReverbMaxPrinted + 1.f)
		{
			audWarningf("Medium reverb max send level increased: %f (%s emitter %d type %d pos [%f,%f,%f]) "
				, sm_MediumReverbMax
				, m_StreamingSound ? m_StreamingSound->GetName() : "(NULL sound)"
				, emitterIndex
				, emitterType
				, position.x
				, position.y
				, position.z
				);

			sm_MediumReverbMaxPrinted = sm_MediumReverbMax;
		}

		if(sm_SmallReverbMax >= sm_SmallReverbMaxPrinted + 1.f)
		{
			audWarningf("Small reverb max send level increased: %f (%s emitter %d type %d pos [%f,%f,%f]) "
				, sm_SmallReverbMax
				, m_StreamingSound ? m_StreamingSound->GetName() : "(NULL sound)"
				, emitterIndex
				, emitterType
				, position.x
				, position.y
				, position.z
				);

			sm_SmallReverbMaxPrinted = sm_SmallReverbMax;
		}

		if(sm_VolumeMax >= sm_VolumeMaxPrinted + 1.f)
		{
			audWarningf("Emitter volume max level increased: %f (%s emitter %d type %d pos [%f,%f,%f]) "
				, sm_VolumeMax
				, m_StreamingSound ? m_StreamingSound->GetName() : "(NULL sound)"
				, emitterIndex
				, emitterType
				, position.x
				, position.y
				, position.z
				);

			sm_VolumeMaxPrinted = sm_VolumeMax;
		}
#endif

		if (volumeDb >= 36.f)
		{
			audWarningf("Radio emitter track volume: %f (Emitter Vol: %f Offset Vol: %f, %s emitter %d type %d pos [%f,%f,%f]) - will clamp"
				, volumeDb
				, emittedVolumeDb
				, volumeOffsetDb
				, m_StreamingSound ? m_StreamingSound->GetName() : "(NULL sound)"
				, emitterIndex
				, emitterType
				, position.x
				, position.y
				, position.z
			);
		}		

		// Don't allow volume to go beyond category headroom (which is approx 32dB)
		volumeDb = Clamp(volumeDb, g_SilenceVolume, 26.f);

		//Use RequestedPostSubmixVolumeAttenuation rather than RequestedVolume here, so we have the flexibility to specify a volume above 0dB
		//in order to cancel-out a proportion of the attenuation specified via categories.
		requestedSettings->SetVolume(0.0f);
#if __WIN32PC
		requestedSettings->SetPostSubmixVolumeAttenuation(volumeDb + m_MakeUpGain);
#else
		requestedSettings->SetPostSubmixVolumeAttenuation(volumeDb);
#endif
		requestedSettings->SetPosition(position);
		requestedSettings->SetLowPassFilterCutoff(lpfCutoff);
		requestedSettings->SetHighPassFilterCutoff(hpfCutoff);		
		requestedSettings->SetEnvironmentalLoudnessFloat(environmentalLoudness);
	}
}

void audRadioTrack::MuteEmitters(void)
{
	//Mute the stereo emitter.
	UpdateStereoEmitter(g_SilenceVolume, kVoiceFilterLPFMaxCutoff);

	//Mute the shadowed emitters.
	for(u8 emitterIndex=0; emitterIndex<g_NumRadioStationShadowedEmitters; emitterIndex++)
	{
		if(m_ShadowedEmitterSounds[emitterIndex])
		{
			//Use RequestedPostSubmixVolumeAttenuation rather than RequestedVolume here, so we have the flexibility to specify a volume above 0dB
			//in order to cancel-out a proportion of the attenuation specified via categories.
			m_ShadowedEmitterSounds[emitterIndex]->SetRequestedPostSubmixVolumeAttenuation(g_SilenceVolume);
		}
	}
}


void audRadioTrack::SkipForward(u32 timeToSkip)
{
	if(naVerifyf(m_State == AUD_TRACK_STATE_VIRTUAL || m_State == AUD_TRACK_STATE_DORMANT, "Attempting to skip track forward but state (%d) is neither virtual (%d) or dormant (%d)", m_State, AUD_TRACK_STATE_VIRTUAL, AUD_TRACK_STATE_DORMANT))
	{
		//Skip forward by advancing the virtual play time.
		m_PlayTime += timeToSkip;
	}
}

bool audRadioTrack::IsStreamingPhysically() const
{
	//Report that we are physically streaming if we have already created sounds, but not played them yet.
	return ((m_State == AUD_TRACK_STATE_PHYSICAL) || ((m_State == AUD_TRACK_STATE_DORMANT) && m_StreamingSound));
}

bool audRadioTrack::IsStreamingVirtually() const
{
	//Report that we are virtually streaming if we have not created any sounds and are yet to play virtually.
	return ((m_State == AUD_TRACK_STATE_VIRTUAL) || ((m_State == AUD_TRACK_STATE_DORMANT) && (m_StreamingSound == NULL)));
}

bool audRadioTrack::IsPlayingPhysicallyOrVirtually() const
{
	return (m_State == AUD_TRACK_STATE_VIRTUAL || m_State == AUD_TRACK_STATE_PHYSICAL);
}

const char *audRadioTrack::GetBankName() const
{
#if __WIN32PC
	if(m_IsUserTrack && m_MediaReader)
	{
		static char trackInfo[255];
		formatf(trackInfo, "%s - %s", 
			audRadioStation::GetUserRadioTrackManager()->GetTrackArtist(m_TrackIndex), 
			audRadioStation::GetUserRadioTrackManager()->GetTrackTitle(m_TrackIndex));
		return trackInfo;
	}
#endif
	if(m_StreamingSound)
	{
		u32 bankIndex = ((audStreamingSound*)m_StreamingSound)->GetStreamingWaveBankIndex();
		return SOUNDFACTORY.GetBankNameFromIndex(bankIndex);
	}
	else
	{
		return NULL;
	}
}

u32 audRadioTrack::MakeTrackId(const s32 category, const u32 trackIndex, const u32 soundRef)
{
	if(audRadioStation::IsNetworkModeHistoryActive())
	{
		// We need to encode category and trackIndex as the TrackId for network sync
		return (u32(category)&0xffff) | (trackIndex << 16);
	}
	else
	{
		// in SP, using the sound ref as the history identifier allows us to check against custom track lists
		return soundRef;
	}
}

void audRadioTrack::UpdateBeatInfo()
{
	const u32 currentPlaytimeInMs = m_PlayTime;
	const f32 currentPlaytimeInSeconds = currentPlaytimeInMs*0.001f;	

	u32 nearestEarlierMarkerTimeMs = 0;
	u32 nearestLaterMarkerTimeMs = m_IsMixStationTrack ? GetDuration() : ~0u; // B*4886712 - Always ensure that we have a valid beat marker to cover the last section of a mix track
	u32 numBeats = 0;
	// search for the two bar markers around the current play head
	for(u32 i=0; i<m_BeatMarkers.GetCount(); i++)
	{
		if(m_BeatMarkers[i].playtimeMs >= nearestEarlierMarkerTimeMs && m_BeatMarkers[i].playtimeMs <= currentPlaytimeInMs)
		{
			nearestEarlierMarkerTimeMs = m_BeatMarkers[i].playtimeMs;
			numBeats = m_BeatMarkers[i].numBeats;
		}
		else if(m_BeatMarkers[i].playtimeMs > currentPlaytimeInMs && m_BeatMarkers[i].playtimeMs < nearestLaterMarkerTimeMs)
		{
			nearestLaterMarkerTimeMs = m_BeatMarkers[i].playtimeMs;
		}	
	}

	if(nearestEarlierMarkerTimeMs != nearestLaterMarkerTimeMs && nearestLaterMarkerTimeMs <= GetDuration())
	{
		const f32 nearestBarTimeInSeconds = nearestEarlierMarkerTimeMs*0.001f;
		const f32 timeSinceBar = currentPlaytimeInSeconds - nearestBarTimeInSeconds;
		
		if(numBeats == 0)
		{
			// default to 2 bar markup
			numBeats = 8;
		}

		// markers occur n beats
		const f32 beatDuration = (nearestLaterMarkerTimeMs - nearestEarlierMarkerTimeMs) / (numBeats * 1000.f);
		m_Bpm = 60.f / beatDuration;
		const f32 beatsPastLastBar = timeSinceBar / beatDuration;
		const f32 currentBeat = Floorf(beatsPastLastBar);
		m_BeatNumber = 1 + ((u32)currentBeat) % 4;
		const f32 timeSinceLastBeat = (beatsPastLastBar - currentBeat) * beatDuration;
		m_BeatTimeS = beatDuration - timeSinceLastBeat;
	}
}

bool audRadioTrack::GetNextBeat(float &timeS, float &bpm, s32 &beatNumber) const
{
	if(HasBeatInfo())
	{
		if (m_IsMixStationTrack)
		{
			audRadioStation* radioStation = audRadioStation::FindStation(m_StationNameHash);

			if (radioStation)
			{
				return radioStation->GetNextMixStationBeat(timeS, bpm, beatNumber);
			}
		}

		if(m_Bpm > 220.f)
		{
			// Filter out very high BPMs (they typically occur due to incorrect markup on start/end of tracks)
			return false;
		}

		timeS = m_BeatTimeS;
		bpm = m_Bpm;
		beatNumber = static_cast<s32>(m_BeatNumber);
		return true;
	}
	return false;
}

bool audRadioTrack::GetDjMarkers(s32 &introStartMs, s32 &introEndMs, s32 &outroStartMs, s32 &outroEndMs) const
{
	if(m_DjRegions.GetCount() > 0)
	{
		const audDjSpeechRegion *region = FindNextRegion(audDjSpeechRegion::INTRO);
		if(region)
		{
			introStartMs = region->startMs;
			introEndMs = region->endMs;
		}
		else
		{
			introStartMs = introEndMs = -1;
		}

		region = FindNextRegion(audDjSpeechRegion::OUTRO);
		if(region)
		{
			outroStartMs = region->startMs;
			outroEndMs = region->endMs;

			// For motomami we want the hosts to talk right up until the end of the track.  Since WFH is slowing down marker edits significantly this is an expedient way to achieve that.
			if(m_StationNameHash == ATSTRINGHASH("RADIO_37_MOTOMAMI", 0x97074FCC))
			{
				outroEndMs = GetDuration();
			}
		}
		else
		{
			outroEndMs = outroStartMs = -1;
		}
		return true;
	}
	else
	{
		introStartMs = -1;
		introEndMs = -1;
		outroStartMs = -1;
		outroEndMs = -1;
	}
	return false;
}

const audRadioTrack::audDjSpeechRegion *audRadioTrack::FindNextRegion(const audRadioTrack::audDjSpeechRegion::Type type) const
{
	const audDjSpeechRegion *region = NULL;
	for(s32 i = 0; i < m_DjRegions.GetCount(); i++)
	{
		if(m_DjRegions[i].type == type && (s32)m_DjRegions[i].endMs > m_PlayTime)
		{
			region = &m_DjRegions[i];
			// Stop on the first one we find that we've not yet passed
			break;
		}
	}

	return region;
}

void audRadioTrack::ParseMarkers()
{
	if(audVerifyf(m_WaveSlot, "Radio track playing physically with no wave slot"))
	{
		// find the wavename hash
		const u32 metadataSoundHash = m_SoundRef;
		u32 wavehash[2] = {~0U,~0U};

		const StreamingSound *streamingSound = SOUNDFACTORY.DecompressMetadata<StreamingSound>(metadataSoundHash);
		if(streamingSound)
		{
			for(u32 waveIndex = 0; waveIndex < Min<u32>(2, streamingSound->numSoundRefs); waveIndex++)
			{
				const SimpleSound *simpleSound = SOUNDFACTORY.DecompressMetadata<SimpleSound>(streamingSound->SoundRef[waveIndex].SoundId);
				if(simpleSound)
				{								
					wavehash[waveIndex] = simpleSound->WaveRef.WaveName;
				}
			}
		}

		if(wavehash[0] != ~0U || wavehash[1] != ~0U)
		{
			audWaveRef waveRef;
			audWaveMarkerList markers;

			u32 sampleRate = 0;
			if(waveRef.Init(m_WaveSlot, wavehash[0]))
			{
				waveRef.FindMarkerData(markers);
				const audWaveFormat *format = waveRef.FindFormat();
				if(format)
				{
					sampleRate = format->SampleRate;
				}
			}
			if(markers.GetCount() == 0)
			{
				if(waveRef.Init(m_WaveSlot, wavehash[1]))
				{
					waveRef.FindMarkerData(markers);
					const audWaveFormat *format = waveRef.FindFormat();
					if(format)
					{
						sampleRate = format->SampleRate;
					}
				}
			}

			audDjSpeechRegion currentRegion;
			currentRegion.type = audDjSpeechRegion::OUTRO;

			audDjSpeechRegion currentRockOutRegion;
			currentRockOutRegion.type = audDjSpeechRegion::ROCKOUT;

			for(u32 i=0; i<markers.GetCount(); i++)
			{
				if(markers[i].categoryNameHash == g_RadioTrackIdCategoryHash)
				{
					if(!m_FoundTrackTextIds)
					{
						if(!m_TextIds.IsFull())
						{
							audTextIdMarker &textIdMarker = m_TextIds.Append();
							textIdMarker.textId = static_cast<u32>(markers[i].value);
							textIdMarker.playtimeMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						}
						else
						{
							naWarningf("Wave with too many track text markers: %s", m_WaveSlot->GetLoadedBankName());
						}
					}
				}
				else if(markers[i].categoryNameHash == g_RadioDjMarkerCategoryHash)
				{
					if(markers[i].nameHash == g_RadioDjMarkerNameIntroStartHash)
					{
						currentRegion.type = audDjSpeechRegion::INTRO;
						currentRegion.startMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
					}
					else if(markers[i].nameHash == g_RadioDjMarkerNameIntroEndHash)
					{
						audAssertf(currentRegion.type == audDjSpeechRegion::INTRO, "%s - invalid intro end marker at %u ms", GetBankName(), audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate));
						
						currentRegion.endMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						m_DjRegions.Append() = currentRegion;
					}
					else if(markers[i].nameHash == g_RadioDjMarkerNameOutroStartHash)
					{
						currentRegion.type = audDjSpeechRegion::OUTRO;
						currentRegion.startMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
					}
					else if(markers[i].nameHash == g_RadioDjMarkerNameOutroEndHash)
					{
						audAssertf(currentRegion.type == audDjSpeechRegion::OUTRO, "%s - invalid intro end marker at %u ms", GetBankName(), audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate));
						currentRegion.endMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						m_DjRegions.Append() = currentRegion;
					}
					else
					{
						naWarningf("Unknown wave marker name in %s", m_WaveSlot->GetLoadedBankName());
					}								
				}
				else if(markers[i].categoryNameHash == ATSTRINGHASH("ROCKOUT", 0xF31B4F6A))
				{
					if(markers[i].nameHash == ATSTRINGHASH("START", 0x84DC271F))
					{
						currentRockOutRegion.startMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
					}
					else if(markers[i].nameHash == ATSTRINGHASH("END", 0xB0C485D7))
					{
						currentRockOutRegion.endMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						m_DjRegions.Append() = currentRockOutRegion;
					}
				}
				else if(markers[i].categoryNameHash == ATSTRINGHASH("dance", 0x44AAC91A))
				{
					if(!m_FoundBeatMarkers)
					{
						// E2 format beat markers
						if(!m_BeatMarkers.IsFull())
						{										
							audAssertf(markers[i].nameHash == ATSTRINGHASH("beat", 0xE89AE78C), "Invalid radio dance marker name hash: %u", markers[i].nameHash);
							BeatMarker &bm = m_BeatMarkers.Append();
							bm.numBeats = markers[i].data;
							bm.playtimeMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						}
						else
						{
							naWarningf("Too many beat markers for track: %s", GetBankName());
						}
					}
				}
				else if(markers[i].categoryNameHash == ATSTRINGHASH("TEMPO", 0x7a495db3))
				{
					// ignore interactive music format tempo markers
				}
				else if(markers[i].categoryNameHash == ATSTRINGHASH("beat", 0xE89AE78C))
				{
					if(!m_FoundBeatMarkers)
					{
						// E2 format beat markers
						if(!m_BeatMarkers.IsFull())
						{
							BeatMarker &bm = m_BeatMarkers.Append();
							bm.numBeats = markers[i].data;
							bm.playtimeMs = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
						}
						else
						{
							naWarningf("Too many beat markers for track: %s", GetBankName());
						}
					}
				}
				else
				{
					naWarningf("Unknown wave marker category %08X in %s", markers[i].categoryNameHash, m_WaveSlot->GetLoadedBankName());
				}
			}
		}
		m_HasParsedMarkers = true;
	}
}

bool audRadioTrack::IsDormant() const
{
	return m_State == AUD_TRACK_STATE_DORMANT;
}
#if __BANK

void audRadioTrack::DrawDebug(audDebugDrawManager &drawMgr) const
{
	s32 introS, introE, outroS, outroE;
	GetDjMarkers(introS, introE, outroS, outroE);

	char playtimeString[32];
	char durationString[32];
	char timeLeftString[32];

	audRadioStation::FormatTimeString(GetPlayTime(), playtimeString);
	audRadioStation::FormatTimeString(GetDuration(), durationString);
	audRadioStation::FormatTimeString(GetDuration() - GetPlayTime(), timeLeftString);

	char introRegionString[32];
	char outroRegionString[32];

	if(introS != -1 && introE != -1)
	{
		char startString[32];
		char endString[32];
		audRadioStation::FormatTimeString(introS, startString);
		audRadioStation::FormatTimeString(introE, endString);
		formatf(introRegionString, "Intro [%s,%s] ", startString, endString);
	}
	else
	{
		introRegionString[0] = 0;
	}
	if(outroS != -1 && outroE != -1)
	{
		char startString[32];
		char endString[32];
		audRadioStation::FormatTimeString(outroS, startString);
		audRadioStation::FormatTimeString(outroE, endString);
		formatf(outroRegionString, "Outro [%s,%s] ", startString, endString);
	}
	else
	{
		outroRegionString[0] = 0;
	}

	const char *trackName = GetBankName();
	if(trackName == NULL)
	{
		trackName = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_SoundRef);
	}

	drawMgr.DrawLinef("%s %s(%s:%u) playtime: %s/%s timeLeft: %s TextID: %u", 
		trackName,
		m_StreamingSound == NULL ? "(NoSound) " : "",
		audRadioStation::GetTrackCategoryName(GetCategory()), 
		GetTrackIndex(), 
		playtimeString, 
		durationString, 
		timeLeftString, 
		GetTextId());

	if(m_StreamingSound)
	{
		drawMgr.DrawLinef("Track Volume: %.02f", audDriverUtil::ComputeLinearVolumeFromDb(m_StreamingSound->GetRequestedVolume()));
	}

	if(strlen(introRegionString))
		drawMgr.DrawLine(introRegionString);
	if(strlen(outroRegionString))
		drawMgr.DrawLine(outroRegionString);

	const audDjSpeechRegion *rockOutRegion = FindNextRegion(audDjSpeechRegion::ROCKOUT);
	if(rockOutRegion)
	{
		if(GetPlayTime() >= rockOutRegion->startMs)
		{
			drawMgr.DrawLinef("ROCK-OUT! (%.2fS remaining)", 0.001f * (rockOutRegion->endMs - GetPlayTime()));
		}
		else
		{
			drawMgr.DrawLinef("Rock out in  %.2fS", 0.001f * (rockOutRegion->startMs - GetPlayTime()));
		}
	}
	
	float bpm;
	s32 beatNumber;
	float beatTimeS;
	if(GetNextBeat(beatTimeS, bpm, beatNumber))
	{		
		drawMgr.DrawLinef("%u beat markers.  Beat: %.2f (%u/4 %.1f BPM)", m_BeatMarkers.GetCount(), beatTimeS, beatNumber, bpm);
	}

#if __WIN32PC
	if(m_IsUserTrack && audRadioStation::GetUserRadioTrackManager()->GetRadioMode() == USERRADIO_PLAY_MODE_RADIO)
	{
		char startString[32];
		char endString[32];
		audRadioStation::FormatTimeString(audRadioStation::GetUserRadioTrackManager()->GetStartOffsetMs(m_TrackIndex), startString);
		audRadioStation::FormatTimeString(m_PostRoll, endString);
		drawMgr.DrawLinef("Make-up gain: %f, start trim: %s end trim: %s]", m_MakeUpGain, startString, endString);
	}
#endif // __WIN32PC
}

#endif // __BANK
#endif // NA_RADIO_ENABLED
