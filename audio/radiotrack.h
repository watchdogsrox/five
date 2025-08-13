// 
// audio/radiotrack.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOTRACK_H
#define AUD_RADIOTRACK_H

#include "audio_channel.h"
#include "audiodefines.h"

#if NA_RADIO_ENABLED

#include "gameobjects.h"

#include "audiohardware/mediareader.h"
#include "audiohardware/waveref.h"
#include "audiosoundtypes/streamingsound.h"

const u8 g_NumRadioStationShadowedEmitters = 9;

class audRadioTrack
{
public:
	audRadioTrack();
	~audRadioTrack(){}

	void Init(const s32 category, const s32 trackIndex, const u32 trackSoundHash, const float startOffset, const bool isMixStationTrack, const bool isReverbStationTrack, const u32 stationNameHash);

#if __WIN32PC
	void SetGainAndPostRoll(const f32 makeUpGain, const u32 postRoll)	{ m_MakeUpGain = makeUpGain; m_PostRoll = postRoll; }
#endif

	void Shutdown(void);

	void Update(u32 timeInMs, bool isFrozen);
	audPrepareState Prepare(u32 startOffset);
#if __WIN32PC
	audPrepareState PrepareUserTrack(u32 startOffset);
#endif
	void Play(void);
	void PlayWhenReady();
	void SetPhysicalStreamingState(bool shouldStreamPhysically, audWaveSlot *waveSlot, u8 soundBucketId);

	void UpdateStereoEmitter(f32 volumeDb, f32 cutoff);
	void UpdatePositionedEmitter(const u32 emitterIndex, f32 emittedVolumeDb, f32 volumeOffsetDb, const u32 lpfCutoff, const u32 hpfCutoff, const Vector3 &position, const audEnvironmentGameMetric *occlusionMetric, const u32 emitterType, const f32 environmentalLoudness);
	void MuteEmitters();
	void SkipForward(u32 timeToSkip);

	bool IsInitialised() const
	{
		return m_IsInitialised;
	}

	audSound* GetStreamingSound() { return m_StreamingSound; }

	bool IsStreamingPhysically() const;
	bool IsStreamingVirtually() const;

	audWaveSlot* GetWaveSlot() 
	{ 
		return m_WaveSlot; 
	}

	s32 GetCategory() const
	{
		return m_Category;
	}

	s32 GetTrackIndex() const
	{
		return m_TrackIndex;
	}

	u32 GetRefForHistory() const
	{
		return m_RefForHistory;
	}

	void SetRefForHistory(const u32 trackId)
	{
		m_RefForHistory = trackId;
	}

	u32 GetSoundRef() const { return m_SoundRef; }

	bool IsFlyloPart1() const { return m_IsFlyloPart1; }

	s32 GetPlayTime() const
	{
		return m_PlayTime;
	}

	f32 GetPlayFraction() const
	{
		return m_Duration > 0 ? (m_PlayTime / (f32)m_Duration) : 0.0f;
	}

	s32 GetPlayTimeCalculationMixerFrame() const
	{
		return m_PlayTimeCalculationMixerFrame;
	}

	bool IsStreamingSoundValid() const
	{
		return m_StreamingSound != NULL;
	}

	void SetPlaytime(const s32 time)
	{
		naAssertf(IsStreamingVirtually(), "Cannot set playtime if we aren't streaming virtually");
		m_PlayTime = time;
	}

	u32 GetDuration() const
	{
#if RSG_PC
		return m_Duration - m_PostRoll;
#else
		return m_Duration;
#endif
	}
	const char *GetBankName() const;

	u32 GetTextId() const;
	u32 GetTextId(const u32 playtimeMs) const;
	u32 GetNumTextIds() const { return m_TextIds.GetCount(); }
	u32 StationNameHash() const { return m_StationNameHash; }

	bool GetDjMarkers(s32 &introStartMs, s32 &introEndMs, s32 &outroStartMs, s32 &outroEndMs) const;

	static u32 MakeTrackId(const s32 category, const u32 trackIndex, const u32 soundRef);
	static u32 GetBeatSoundName(u32 soundName);

	bool IsDormant() const;

	static u32 FindTextIdForSoundHash(const u32 soundNameHash);
	static bool IsScoreTrack(const u32 soundNameHash);
	static bool IsCommercial(const u32 soundHashHash);
	static bool IsSFX(const u32 soundHashHash);
	static bool IsAmbient(const u32 soundHash);


	enum {kFirstScoreTrackId = 2100};
	enum {kFirstCommercialTrackId = 3100};
	enum {kFirstSFXTrackId = 3300};
	enum {kFirstAmbinetTrackId = 3450};
	enum { kFirstUserTrackId = 5100 };
	static u32 GetUserTrackIndexFromTextId(const u32 textId)
	{
		return textId - kFirstUserTrackId;
	}

	static u32 GetUserTrackTextIdFromIndex(const u32 index)
	{
		return index + kFirstUserTrackId;
	}
#if __BANK
	void DrawDebug(audDebugDrawManager &drawMgr) const;
#endif

	bool HasBeatInfo() const { return m_BeatMarkers.GetCount() > 0; }
	// PURPOSE
	//	Returns info about the timing of the next beat, if available
	// RETURNS
	//	true if info was available, false otherwise
	// PARAMS
	//	timeS: time until next beat (seconds)
	//	bpm: current beats per minute value
	//	beatNumber: All tracks are assumed to be 4/4; this value is therefore in range [1,4]
	bool GetNextBeat(float &timeS, float &bpm, s32 &beatNumber) const;

	bool IsUserTrack()	{ return m_IsUserTrack; }
	bool IsPlaying()	{ return (m_StreamingSound == NULL?false:true); }

	u32 GetTimePlayed() const { return m_TimePlayed; }

	bool IsPlayingPhysicallyOrVirtually() const;

	struct audDjSpeechRegion
	{
		enum Type
		{
			INTRO = 0,
			OUTRO,
			ROCKOUT,
		};
		u32 startMs;
		u32 endMs;
		Type type;
	};

	const audDjSpeechRegion *FindNextRegion(const audDjSpeechRegion::Type type) const;

	static float GetLargeReverbMax() { return sm_LargeReverbMax; }
	static float GetMediumReverbMax() { return sm_MediumReverbMax; }
	static float GetSmallReverbMax() { return sm_SmallReverbMax; }
	static float GetVolumeMax() { return sm_VolumeMax; }

#if RSG_PC
	bool IsUserTrack() const
	{
		return m_IsUserTrack;
	}
#endif // RSG_PC

private:

	WIN32PC_ONLY(static bool IsUserTrackSound(const u32 soundRef));

	void ParseMarkers();
	void Reset();
	void UpdateBeatInfo();
	void ShutdownUserMusic();

	void CreateShadowEmitterSounds(const s32 pcmSourceIdL, const s32 pcmSourceIdR, const s32 pcmSourceIDReverb);

	struct audTextIdMarker
	{
		u32 playtimeMs;
		u32 textId;
	};

	enum { kMaxTrackTextIds = 48 };
	atFixedArray<audTextIdMarker, kMaxTrackTextIds> m_TextIds;

	enum { kRadioMarkerHistorySize = kMaxTrackTextIds * 2 };
	atFixedArray<audDjSpeechRegion, kRadioMarkerHistorySize> m_DjRegions;
		
		
	audSound *m_StreamingSound;
	atRangeArray<audSound *, g_NumRadioStationShadowedEmitters> m_ShadowedEmitterSounds;
	audWaveSlot *m_WaveSlot;
	s32 m_PlayTime;
	u32 m_PlayTimeCalculationMixerFrame;
	u32 m_Duration;
	u32 m_LastUpdateTime;
	u32 m_TimePlayed;
	u32 m_StartOffset;
	u32 m_SoundRef;
	u32 m_RefForHistory;
	u32 m_StationNameHash;
	
	struct BeatMarker
	{
		u32 playtimeMs;
		u32 numBeats;		
	};
	enum {kMaxBeatMarkers = 64};
	atFixedArray<BeatMarker, kMaxBeatMarkers> m_BeatMarkers;

	float m_Bpm;
	u32 m_BeatNumber;
	float m_BeatTimeS;
		
	s32 m_Category;
	s32 m_TrackIndex;

	float m_StereoVolumeOffset;

	u8 m_State;
	u8 m_SoundBucketId;
	bool m_IsFlyloPart1;
	bool m_IsMixStationTrack;
	bool m_IsReverbStationTrack;
	bool m_IsInitialised;
	bool m_HasParsedMarkers;
	bool m_FoundTrackTextIds;
	bool m_FoundBeatMarkers;
	bool m_IsUserTrack; // Keeping this out of the #if makes some of the code easier to read
#if __WIN32PC
	audPrepareState audRadioTrack::CreateStreamer(u32 startOffset);

	audMediaReader* m_MediaReader;
	f32 m_MakeUpGain;
	u32 m_PostRoll;
	u32 m_StreamerStartOffset;
#endif

	static f32 sm_LargeReverbMax;
	static f32 sm_MediumReverbMax;
	static f32 sm_SmallReverbMax;
	static f32 sm_VolumeMax;

	static f32 sm_LargeReverbMaxPrinted;
	static f32 sm_MediumReverbMaxPrinted;
	static f32 sm_SmallReverbMaxPrinted;
	static f32 sm_VolumeMaxPrinted;

};
#endif // NA_RADIO_ENABLED
#endif // AUD_RADIOTRACK_H
