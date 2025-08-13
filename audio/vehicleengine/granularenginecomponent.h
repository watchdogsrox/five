// 
// audio/granularengineaudio.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GRANULAR_ENGINE_COMPONENT_H
#define AUD_GRANULAR_ENGINE_COMPONENT_H

#include "audio/gameobjects.h"
#include "audioengine/widgets.h"
#include "audio/vehicleaudioentity.h"
#include "audiohardware/granularmix.h"

namespace rage
{
	class audSound;
	class audGranularSound;
}

class audCarAudioEntity;

// ----------------------------------------------------------
// Class for modelling engine/exhaust sounds
// ----------------------------------------------------------
class audGranularEngineComponent
{
// Public types
public:
	enum EngineSoundType
	{
		ENGINE_SOUND_TYPE_ENGINE,
		ENGINE_SOUND_TYPE_EXHAUST,
		ENGINE_SOUND_TYPE_MAX,
	};

	enum EngineChannelType
	{
		ENGINE_CHANNEL_TYPE_ACCEL,
		ENGINE_CHANNEL_TYPE_DECEL,
		ENGINE_CHANNEL_TYPE_IDLE,
		ENGINE_CHANNEL_TYPE_MAX,
	};

// Public functions
public:
	explicit audGranularEngineComponent();
	virtual ~audGranularEngineComponent() {};

	void Init(audVehicleAudioEntity* parent, EngineSoundType soundType, audGranularMix::audGrainPlayerQuality quality);
	void Update(audVehicleVariables* state, bool updateChannelVolumes);
	void UpdateSynthSounds(audVehicleVariables* state, f32 revRatio, audGranularSound* primaryGranularSound);
	void Stop(s32 releaseTime);
	void QualitySettingChanged();
	bool AreAnySoundsPlaying() const;
	void Start();
	bool Prepare();
	void Play();
	void TriggerGearChangeWobble() const;
	f32 CalculateDialRPMRatio() const;

	inline bool HasPrepared() const																	{ return m_HasPrepared; }
	inline bool HasPlayed()	const																	{ return m_HasPlayed; }
	inline bool IsSoundValid(audGranularMix::audGrainPlayerQuality quality) const					{ return m_GranularSound[quality] != NULL; }
	inline bool HasQualitySettingChanged() const													{ return m_QualitySettingChanged; }
	inline audSound* GetGranularSound(audGranularMix::audGrainPlayerQuality quality) const			{ return m_GranularSound[quality]; }
	inline void SetFadeInRequired(u32 fadeInRequired) 												{ m_FadeInRequired = fadeInRequired; }
	inline void SetVolumeScale(f32 scale)															{ m_VolumeScale = scale; }
	inline f32 GetIdleVolumeScale() const															{ return m_IdleMainSmoother.GetLastValue(); }

// Private methods
private:
	void ConfigureInitialGrainPlayerState();
	void UpdateWobbles(audVehicleVariables* state, audGranularMix::audGrainPlayerQuality quality);
	void UpdateQualityCrossfade();

#if __BANK
	void UpdateDebug();
#endif

// Private types
private:
	enum SynchronisedSynthType
	{
		SYNTH_TYPE_GENERAL,
		SYNTH_TYPE_DAMAGE,
		SYNTH_TYPE_NUM,
	};
	
// Private attributes
private:
	audSimpleSmoother												m_RevsOffEffectSmoother;
	audSimpleSmootherDiscrete										m_IdleMainSmoother;
	atRangeArray<audSound*, audGranularMix::GrainPlayerQualityMax>	m_GranularSound;
	atRangeArray<audSound*, SYNTH_TYPE_NUM>							m_SynthSounds;
	atRangeArray<u32, SYNTH_TYPE_NUM>								m_AudioFramesSinceSynthStart;
	audVehicleAudioEntity*											m_Parent;
	GranularEngineAudioSettings*									m_GranularEngineAudioSettings;
	EngineSoundType													m_EngineSoundType;
	audGranularMix::audGrainPlayerQuality							m_EngineQuality;
	audGranularSubmix::audGrainPlaybackOrder						m_OverridenPlaybackOrder;
	f32 m_VolumeScale;
	f32 m_QualityCrossfade;
	u32 m_FadeInRequired;
	u32 m_LastGearForWobble;
	f32 m_GasPedalLastFrame;
	u32 m_LastWobbleTimeMs;
	u32 m_DamageSoundHash;
	u32 m_PrevAudioFrame;

	bool m_HasPrepared : 1;
	bool m_HasPlayed : 1;
	bool m_ReverseWobbleActive : 1;
	bool m_QualitySettingChanged : 1;
	bool m_DisableLoopsOnDecels : 1;
};

#endif // AUD_GRANULAR_ENGINE_COMPONENT_H

