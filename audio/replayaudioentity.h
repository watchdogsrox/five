

#ifndef AUD_REPLAYAUDIOENTITY_H
#define AUD_REPLAYAUDIOENTITY_H

#include "northaudioengine.h"
#include "audiosoundtypes/sound.h"
#include "audioengine/soundset.h"
#include "audiosoundtypes/streamingsound.h"
#include "streamslot.h"
#include "atl/array.h"

#include "audioentity.h"
#include "control/replay/ReplaySettings.h" 

#if GTA_REPLAY

#define NUM_REPLAY_STREAM_SOUNDS		g_NumStreamSlots

#define REPLAY_AMBIENT_TRACK_MARKER_ID	10000
#define REPLAY_AMBIENT_RELEASE_TIME		2000
#define REPLAY_AMBIENT_ATTACK_TIME		1000

#define REPLAY_SFX_PREVIEW_MARKER_ID	9999
#define REPLAY_SFX_PREVIEW_WAIT_TIME	500
#define REPLAY_SFX_RELEASE_TIME			2000
#define REPLAY_SFX_PRELAOD_MS			2000

enum audReplayStreamStatus
{
	REPLAY_STREAM_STATUS_IDLE,
	REPLAY_STREAM_STATUS_PRELOADING,
	REPLAY_STREAM_STATUS_PRELOADED,
	REPLAY_STREAM_STATUS_PLAYING,
	REPLAY_STREAM_STATUS_FREEING
};

enum audReplayStreamType
{
	REPLAY_STREAM_SFX,
	REPLAY_STREAM_AMBIENT,
	REPLAY_STREAM_AMBIENT_STOP,
};

struct ReplayStreamSound {

	audStreamSlot*			m_StreamSlot;
	audSound*				m_Sound;
	u32						m_Duration;
	audReplayStreamStatus	m_Status;
	audReplayStreamType		m_Type;
	s32						m_MarkerIndex;
	u32						m_SoundHash;
	u32						m_StartOffset;
	bool					m_IsValid;
    bool                    m_IsPreview;
	bool					m_ShouldPlay;
};

class audReplayAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audReplayAudioEntity);

	audReplayAudioEntity();
	~audReplayAudioEntity();

	void Init();
	virtual void PreUpdateService(u32 timeInMs);
	virtual bool IsUnpausable() const { return true; }
	void Shutdown();

	s32 Preload(s32 markerIndex, u32 nameHash, audReplayStreamType type = REPLAY_STREAM_SFX, u32 startOffset = 0);
	void Stop(s32 markerIndex, u32 nameHash, bool release = false);
	void StopAllStreams();
	void StopAllSFX();								// stop and clears everything, even just preloaded sounds
	void StopAllPlayingSFX();						// only stops sounds that are playing, not preloaded ones
	void StopAllAmbient(bool release = false);
	void InvalidateSFX();							// sets all SFX to invalid so update code will re-validate the correct ones and stop the others
	void ClearInvalidSFX();							// stops and clears all SFX that shouldn't be active
	bool AreAllStreamingSoundsPrepared();

	// SFX
	void Play(s32 markerIndex, u32 nameHash, bool isPreview = false);
	bool IsReadyToPlay(s32 markerIndex, u32 nameHash);
	bool IsLoadedOrPlaying(s32 markerIndex, u32 nameHash);
	bool IsSFXPlaying();
	void SetPreviewMarkerSFX(bool preview, u32 sfxNameHash);
	void SetSfxVolumeAffectingMusic(f32 volume)		{ m_SFXVolumeAffectingMusic = volume; }
	f32 GetSFXVolumeAffectingMusic()				{ return m_SFXVolumeAffectingMusic; }

	// Ambient
	void PlayAmbient(s32 markerIndex, u32 nameHash, bool isPreview = false);
	bool IsAmbientReadyToPlay(s32 markerIndex, u32 nameHash);
	bool IsAmbientPlaying(s32 markerIndex, u32 nameHash);
	void StopExpiredAmbientTracks(s32 markerIndex);

	bool HasSoundStopped(u32 userData);

	void SetShouldDuckMusic(bool duck)		{ m_ShouldDuckMusic = duck; }
	bool ShouldDuckMusic()					{ return m_ShouldDuckMusic; }

	static bool HasStreamStoppedCallback(u32 userData);
	static bool OnStreamStopCallback(u32 userData);

	void SetMarkerSFXValid(s32 index, bool valid)		{ m_MarkerSFXValid[index] = valid; }
	bool IsMarkerSFXValid(s32 index)					{ return m_MarkerSFXValid[index]; }			

#if __BANK
	static void AddWidgets(bkBank &bank);
	void DebugDraw();
#endif

private:
	void ClearReplayStream(s32 index);
	void StopAllPreviewSFX(u32 preserveSoundHash);

	bool IsLoaded(s32 markerIndex, u32 nameHash);
	bool IsLoading(s32 markerIndex, u32 nameHash);
	bool IsPlaying(s32 markerIndex, u32 nameHash);

	void Stop(s32 slot, bool release = false);
	void UpdatePreload(s32 slot);
	void UpdatePlay(s32 slot);
	void UpdateVolume(s32 slot);
	void UpdateFree(s32 slot);
	void PlayInternal(s32 slot);

	atFixedArray<ReplayStreamSound, NUM_REPLAY_STREAM_SOUNDS> m_ReplayStream;

	bool m_MarkerSFXValid[CMontage::MAX_MARKERS_PER_CLIP];

	bool m_ShouldDuckMusic;
	f32 m_SFXVolumeAffectingMusic;
	bool m_PreviewMarkerSFX;
	u32 m_PreviewSFXStartTime;
	u32 m_PreviewSFXNameHash;
};

extern audReplayAudioEntity g_ReplayAudioEntity;


#endif
#endif

