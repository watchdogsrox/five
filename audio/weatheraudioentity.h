// 
// audio/weatheraudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_WEATHERAUDIOENTITY_H
#define AUD_WEATHERAUDIOENTITY_H

#include "audio/environment/environment.h"

#include "audioengine/soundset.h"
#include "audioengine/curve.h"
#include "audioengine/widgets.h"
#include "vector/vector3.h"
#include "pathserver/PathServer.h"

#include "dynamicmixer.h"
#include "audioentity.h"

#define MAX_RUMBLE_SPIKE_EVENTS 40
#define MAX_RUMBLE_PAD_EVENTS 10

namespace rage
{
	class bkBank;
};

struct audRumbleSpikeEvent
{ 
	u32 timeToSpike;
	f32 intensity;
};

struct audRumblePadEvent
{
	u32 duration0,duration1;
	s32 freq0,freq1,timeToTrigger;
};

typedef enum
{
	AUD_WEATHER_SOUND_UNKNOWN,
	AUD_WEATHER_SOUND_THUNDER,
}audWeatherSounds;


class audWeatherAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audWeatherAudioEntity);

	static void InitClass();

	void Init(void);
	void Shutdown(void);
	virtual void PreUpdateService(u32 timeInMs);
	virtual void UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs);

	const Vector3 &GetGustMiddlePoint() {return m_GustMiddlePoint;};
	const Vector3 &GetGustDirection() {return m_GustDirection;};

	void AddCLoudBurstSpikeEvent(const Vector3 &pos, const f32 rumbleIntensity);
	void AddRumbleSpikeEvent(u32 delay,f32 intensity  BANK_ONLY(, f32 distanceToTravel));
	void AddRumblePadEvent(u32 duration0,s32 freq0,u32 duration1,s32 freq1,s32 timeToTrigger);


	void TriggerThunderStrike(const Vector3 &position);
	void TriggerThunderStrikeBurst(const Vector3 &position);
	void TriggerThunderBurst(const f32 intensity);
	void OverrideWindType(bool override,const char *oldWindTypeName = "",const char *newWindTypeName = "");

	void SetScriptWindElevation(const u32 windElevationHashName) {sm_ScriptElevationWindHash = windElevationHashName; } ;

	f32 GetRainVerticallAtt() const;
	f32 GetRainVolume(){return m_RainVolume;};
	f32 GetThunderRumbleIntensity(){return m_ThunderRumbleIntensity;};
	f32 GetWindStrength() const {return m_WindStrength;}

	bool IsGustInProgress(){return m_GustInProgress;};

	BANK_ONLY(static void AddWidgets(bkBank &bank));

private:
	typedef enum
	{
		STREAMED_SOUND_TYPE_ROLL,
		STREAMED_SOUND_TYPE_STRIKE,
		STREAMED_SOUND_TYPE_MAX,
	}audStreamedWeatherSoundType;

	typedef enum
	{
		STREAMED_SOUND_STATE_IDLE,
		STREAMED_SOUND_STATE_PREPARING,
		STREAMED_SOUND_STATE_PLAYING,
	}audStreamedWeatherSoundState;

	struct WeatherStreamedSoundSlot
	{
		WeatherStreamedSoundSlot()
		{
			sound = NULL;
			speechSlot = NULL;
			speechSlotID = 0;
			fallbackSoundRef = g_NullSoundRef;
			desiredTriggerTime = 0u;
			maxTriggerTime = 0u;
			rumbleSpikeIntensity = 0.0f;
			lpfCutoff = kVoiceFilterLPFMaxCutoff;
			streamedSoundState = STREAMED_SOUND_STATE_IDLE;
			usingFallbackSound = false;
			BANK_ONLY(distanceToTravel = 0.0f);
		}

		audSound*						sound;
		audWaveSlot*					speechSlot;
		audMetadataRef					fallbackSoundRef;
		s32								speechSlotID;
		u32								desiredTriggerTime;
		u32								maxTriggerTime;
		u32								lpfCutoff;
		f32								rumbleSpikeIntensity;
		audStreamedWeatherSoundState	streamedSoundState;
		bool							usingFallbackSound;
		BANK_ONLY(f32					distanceToTravel);
	};

private:
	void PlayWaterRain(const u8 soundIdx,const f32 volume,const u32 pan, const f32 LPFAttenuation);
	void PlayTarmacRain(const u8 soundIdx,const f32 volume,const u32 pan, const f32 LPFAttenuation);
	void PlayTreeRain(const u8 soundIdx,const f32 volume,const u32 pan, const f32 LPFAttenuation);
	void StopRainSound(const u8 rainSoundIdx,const u8 soundIdx);
	void UpdateThunder(const f32* speakerVolumes);
	void UpdateRain();
	void UpdateWind(u32 timeInMs, const f32* windSpeakerVolumes);
	void UpdatePrevWindSounds(WeatherAudioSettings* weatherType,const f32* volumes);
	void UpdateNextWindSounds(WeatherAudioSettings* weatherType,const f32* volumes);
	void UpdateWindElevationSounds(const f32* volumes);
	void UpdateWeatherDynamicMix(WeatherAudioSettings* oldWeatherType,WeatherAudioSettings* newWeatherType);
	bool RequestStreamedWeatherOneShot(const audMetadataRef soundRef, const audMetadataRef fallbackSoundRef, bool playEvenIfNoFreeSlots, const u32 desiredTriggerTime, const u32 maxTriggerTime, audStreamedWeatherSoundType soundType, const f32 rumbleSpikeIntensity, const u32 lpfCutoff BANK_ONLY(, f32 distanceToTravel));
	void UpdateStreamedWeatherSounds();

	void CalculateWindSpeakerVolumes(f32* volumes) const;
	void UpdateGustBand(u32 timeInMs,WeatherAudioSettings *windType,f32 volume);

	WeatherAudioSettings* GetWeatherAudioSettings(u32 weatherTypeHashName);

	BANK_ONLY(static void AddWeatherTypeWidgets(bool prev););
	BANK_ONLY(static void ToggleEnableWeatherInterpTool(););

private:
	atRangeArray<WeatherStreamedSoundSlot, STREAMED_SOUND_TYPE_MAX> m_StreamedWeatherSoundSlots;

	Vector3			m_GustMiddlePoint;
	Vector3			m_InitialGustLineStart, m_GustLineStart, m_GustLineStartToEnd, m_GustDirection;

	atRangeArray<audRumbleSpikeEvent, MAX_RUMBLE_SPIKE_EVENTS>		m_RumbleSpikeEvents;
	atRangeArray<audRumblePadEvent, MAX_RUMBLE_PAD_EVENTS>			m_RumblePadEvents;
	audSimpleSmoother	m_RainSmoothers[4][4];

	audSound		*m_HighElevationWindSounds[6];
	audSound		*m_WindSounds[2][6];
	audSound		*m_RainSounds[4][4];

	audSoundSet		m_RainSoundSet;

	audCurve		m_WorlHeightToRainVolume;
	audCurve		m_RainHeightVolumeCurve;
	audCurve		m_RainToVolumeCurve,m_RainBlockedVolCurve,m_RainExteriorOcclusionVolCurve;
	audCurve		m_RainDistanceToNonCoverCurve;

	audScene*		m_OldWeatherTypeScene;
	audScene*		m_NewWeatherTypeScene;

	audSound*		m_GustSound;
	audSound*		m_RumbleLoop;
	audSound*		m_RollingThunder;

	f32  			m_WindStrength;
	f32				m_GustSpeed;
	f32				m_GustAheadWidth;
	f32				m_GustBehindWidth;
	f32				m_ThunderRumbleIntensity;

	u32				m_OldWeatherTypeLastIndex;
	u32				m_NewWeatherTypeLastIndex;
	u32				m_OldScriptWind;
	u32				m_NewScriptWind;
	u32				m_GustStartTime;

	f32				m_RainVolume;
	u32				m_NextGustTime;


	bool			m_ScriptOverridingWind;
	bool			m_ScriptWasOverridingWind;
	bool			m_GustInProgress;

	u8 				m_OldWeatherTypeSoundsIndex;

	static audSoundSet sm_ThunderSounds;

	static audCurve sm_DistanceToRumbleIntensityScale;
	static audCurve sm_BurstIntensityToDelayFactor;
	static audCurve sm_BurstIntensityToLPFCutOff;

	static audEnvironmentMetricsInternal sm_MetricsInternal;
	static audEnvironmentSoundMetrics sm_Metrics;

#if __BANK
	static f32 sm_OverridenRumbleIntensity;
	static f32 sm_OverridenWindSpeakerVolumes[4];
#endif

	static f32 sm_RumbleIntensityDecay;
	static f32 sm_CloudBurstMinDistance;
	static f32 sm_CloudBurstMaxDistance;
	static f32 sm_ThunderStrikeMinDistance;
	static f32 sm_ThunderStrikeMaxDistance;
	static f32 sm_ThunderStrikeDistanceCorrection;
	static f32 sm_CloudBurstDistanceCorrection;

	static u32 sm_MinTimeToTriggerGust;
	static u32 sm_MaxTimeToTriggerGust;
	static u32 sm_MaxBurstDelay;
	static u32 sm_PredelayTestForRollingThunder;
	static u32 sm_TimeToRetriggerThunderStrike;
	static u32 sm_LastThunderStrikeTriggeredTime;

	static u32 sm_ScriptElevationWindHash;
	static u32 sm_NumRainSoundPerChannel;

#if __BANK 
	static f32		sm_WeatherInterp;
	static f32		sm_GustSpeed;

	static u8 		sm_OverridenPrevWeatherType;
	static u8 		sm_OverridenNextWeatherType;

	static bool		sm_ShowRainVolumes;
	static bool		sm_DrawWindSpeakerVolumes;
	static bool		sm_ShowThunderInfo;

	static bool		sm_EnableWindTestTool;
	static bool		sm_HasToStopDebugSounds;
	static bool		sm_ShowWindSoundsInfo;
	static bool		sm_DebugGust;
	static bool		sm_ManualGust;
	static bool		sm_TriggerGust;
	static bool		sm_OverrideGustFactors;

#endif
};	   			

extern audWeatherAudioEntity g_WeatherAudioEntity;

#endif // AUD_WEATHERAUDIOENTITY_H
