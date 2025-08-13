// 
// audio/frontendaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "ambience/ambientaudioentity.h"
#include "ambience/audambientzone.h"
#include "weatheraudioentity.h" 
#include "northaudioengine.h"
#include "frontendaudioentity.h"
#include "speechmanager.h"
#include "scriptaudioentity.h"
#include "cutsceneaudioentity.h" 
#include "pheffects/wind.h"
#include "system/controlMgr.h"
#if __BANK
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#endif

#include "audioengine/curverepository.h"
#include "audioengine/engine.h"
#include "audioengine/environment.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "audioengine/engineutil.h"
#include "audioeffecttypes/biquadfiltereffect.h"
#include "audiosoundtypes/soundcontrol.h"
#include "camera/CamInterface.h"
#include "audiosoundtypes/envelopesound.h"
#include "entity/archetype.h"
#include "grcore/debugdraw.h"
#include "game/clock.h"
#include "game/weather.h"
#include "game/wind.h"
#include "modelinfo/MloModelInfo.h"
#include "peds/ped.h"
#include "vector/geometry.h"
#include "radioaudioentity.h"
#include "renderer/zonecull.h"
#include "renderer/water.h"
#include "scriptaudioentity.h"
#include "debugaudio.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/portals/Portal.h"
#include "task/motion/Locomotion/taskmotionped.h" 
#include "renderer/Water.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

audWeatherAudioEntity g_WeatherAudioEntity;

audSoundSet audWeatherAudioEntity::sm_ThunderSounds;

audEnvironmentMetricsInternal audWeatherAudioEntity::sm_MetricsInternal;
audEnvironmentSoundMetrics audWeatherAudioEntity::sm_Metrics;

audCurve audWeatherAudioEntity::sm_DistanceToRumbleIntensityScale;
audCurve audWeatherAudioEntity::sm_BurstIntensityToDelayFactor;
audCurve audWeatherAudioEntity::sm_BurstIntensityToLPFCutOff;

#if __BANK
f32 audWeatherAudioEntity::sm_OverridenRumbleIntensity = -1.f;
f32 audWeatherAudioEntity::sm_OverridenWindSpeakerVolumes[4];
#endif

f32 audWeatherAudioEntity::sm_RumbleIntensityDecay = 0.07f;
f32 audWeatherAudioEntity::sm_CloudBurstMinDistance = 1000.f;
f32 audWeatherAudioEntity::sm_CloudBurstMaxDistance = 1500.f;
f32 audWeatherAudioEntity::sm_ThunderStrikeMinDistance = 1300.f;
f32 audWeatherAudioEntity::sm_ThunderStrikeMaxDistance = 3000.f;
f32 audWeatherAudioEntity::sm_ThunderStrikeDistanceCorrection = 0.15f;
f32 audWeatherAudioEntity::sm_CloudBurstDistanceCorrection = 0.333f;

u32 audWeatherAudioEntity::sm_MinTimeToTriggerGust = 15000;
u32 audWeatherAudioEntity::sm_MaxTimeToTriggerGust = 30000;
u32 audWeatherAudioEntity::sm_MaxBurstDelay = 3000;
u32 audWeatherAudioEntity::sm_PredelayTestForRollingThunder = 3000;
u32 audWeatherAudioEntity::sm_LastThunderStrikeTriggeredTime = 0;
u32 audWeatherAudioEntity::sm_TimeToRetriggerThunderStrike = 2000;
u32 audWeatherAudioEntity::sm_ScriptElevationWindHash = 0;
u32 audWeatherAudioEntity::sm_NumRainSoundPerChannel = 2;

#if __BANK
f32 audWeatherAudioEntity::sm_WeatherInterp = 1.f;
f32 audWeatherAudioEntity::sm_GustSpeed = 7.f;

u8	audWeatherAudioEntity::sm_OverridenPrevWeatherType = 0;
u8	audWeatherAudioEntity::sm_OverridenNextWeatherType = 0;

bool audWeatherAudioEntity::sm_ShowRainVolumes = false;
bool audWeatherAudioEntity::sm_DrawWindSpeakerVolumes = false;

bool audWeatherAudioEntity::sm_ShowThunderInfo = false;

bool audWeatherAudioEntity::sm_EnableWindTestTool = false;
bool audWeatherAudioEntity::sm_HasToStopDebugSounds = false;
bool audWeatherAudioEntity::sm_ShowWindSoundsInfo = false;
bool audWeatherAudioEntity::sm_DebugGust = false;
bool audWeatherAudioEntity::sm_ManualGust = false;
bool audWeatherAudioEntity::sm_TriggerGust = false;
bool audWeatherAudioEntity::sm_OverrideGustFactors = false;

extern Vector3 g_Directions[4];
#endif

//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::InitClass()
{
	sm_DistanceToRumbleIntensityScale.Init(ATSTRINGHASH("DISTANCE_TO_RUMBLE_INTENSITY_SCALE", 0x1A91562A));
	sm_BurstIntensityToDelayFactor.Init(ATSTRINGHASH("BURST_INTENSITY_TO_DELAY_FACTOR", 0x96AE104));
	sm_BurstIntensityToLPFCutOff.Init(ATSTRINGHASH("BURST_INTENSITY_TO_LPF", 0x39BA7CB9));

	sm_ThunderSounds.Init(ATSTRINGHASH("THUNDER_SOUNDS", 0x6B835098));

#if __BANK
	for (u32 i = 0; i < 4; i++)
	{
		sm_OverridenWindSpeakerVolumes[i] = -1;
	}
#endif
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audWeatherAudioEntity");

	// Wind
	m_RainSoundSet.Init(ATSTRINGHASH("RAIN_SOUNDSET", 0x115CA96C));
	m_OldWeatherTypeScene = NULL;
	m_NewWeatherTypeScene = NULL;
	m_GustSound = NULL;
	m_OldWeatherTypeLastIndex = 0;
	m_NewWeatherTypeLastIndex = 0;
	m_OldWeatherTypeSoundsIndex = 0;

	m_NextGustTime = 0;
	m_GustSpeed = 7.f;

	m_GustMiddlePoint = VEC3_ZERO;
	for (u32 i = 0; i < 2; i++)
	{
		for(u32 j = 0; j<6; j++)
		{
			m_WindSounds[i][j] = NULL; 
		}
	}
	for (u32 i = 0; i < 6; i++)
	{
		m_HighElevationWindSounds[i] = NULL;
	}
	m_InitialGustLineStart.Zero();
	m_GustLineStart.Zero();
	m_GustLineStartToEnd.Zero();
	m_GustDirection.Zero();
	m_GustAheadWidth = 50;
	m_GustBehindWidth = 50;
	m_GustStartTime = 0;

	m_OldScriptWind = 0;
	m_NewScriptWind = 0;

	m_ScriptOverridingWind = false;
	m_ScriptWasOverridingWind = false;
	m_GustInProgress = false;
	// Rain
	for(u32 i = 0; i < 4; i++)
	{
		for(u32 j = 0; j < 4; j++)
		{
			m_RainSounds[i][j] = NULL;
			m_RainSmoothers[i][j].Init(0.001f, true);
		}
	}

	m_RainDistanceToNonCoverCurve.Init(ATSTRINGHASH("RAIN_DISTANCE_TO_UNCOVER_VOL", 0x5487186E));
	//m_RainDistanceSmoother.Init(0.01f, 0.01f, 0.0f, 25.0f);
	m_RainToVolumeCurve.Init(ATSTRINGHASH("RAIN_TO_RAIN_VOLUME", 0x58BD5234));
	m_RainBlockedVolCurve.Init(ATSTRINGHASH("RAIN_BLOCKED_ATTEN", 0x68ED073D));
	m_RainExteriorOcclusionVolCurve.Init(ATSTRINGHASH("RAIN_EXTERIOR_OCCLUSION_TO_VOL_LIN", 0xD1C98525));
	m_WorlHeightToRainVolume.Init(ATSTRINGHASH("LISTENER_HEIGHT_TO_RAIN_LIN_VOL", 0x42FBCD1D));
	// Thunder
	m_ThunderRumbleIntensity = 0.f;
	m_RumbleLoop = NULL;
	m_RollingThunder = NULL;
	for(u8 i = 0; i < MAX_RUMBLE_SPIKE_EVENTS; i ++)
	{
		m_RumbleSpikeEvents[i].intensity = 0.f;
		m_RumbleSpikeEvents[i].timeToSpike = 0;
	}
	for(u8 i = 0; i < MAX_RUMBLE_PAD_EVENTS; i ++)
	{
		m_RumblePadEvents[i].duration0 = 0;
		m_RumblePadEvents[i].duration1 = 0;
		m_RumblePadEvents[i].freq0 = 0;
		m_RumblePadEvents[i].freq1 = 0;
		m_RumblePadEvents[i].timeToTrigger = -1;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::Shutdown()
{
	if(m_OldWeatherTypeScene)
	{
		m_OldWeatherTypeScene->Stop();
	}
	if (m_NewWeatherTypeScene)
	{
		m_NewWeatherTypeScene->Stop();
	}

	naAudioEntity::Shutdown();
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::OverrideWindType(bool useScriptWind,const char *oldWindTypeName,const char *newWindTypeName)
{
	m_ScriptOverridingWind = useScriptWind;
	m_OldScriptWind = atHashString (oldWindTypeName);
	m_NewScriptWind = atHashString (newWindTypeName);
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::PreUpdateService(u32 timeInMs)
{
	ALIGNAS(16) f32 windSpeakerVolumes[4] ;
	sysMemSet(&windSpeakerVolumes[0], 0, sizeof(windSpeakerVolumes));
	CalculateWindSpeakerVolumes(windSpeakerVolumes);

#if __BANK
	for (u32 i = 0; i < 4; i++)
	{
		if (sm_OverridenWindSpeakerVolumes[i] >= 0)
		{
			windSpeakerVolumes[i] = sm_OverridenWindSpeakerVolumes[i];
		}
	}
#endif

	UpdateWind(timeInMs, windSpeakerVolumes);
	UpdateRain();
	// Thunder.
	// TODO : Only do stuff is the weather type is thunder. 
	UpdateThunder(windSpeakerVolumes);
	UpdateStreamedWeatherSounds();
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 /*timeInMs*/)
{
	f32 playerInteriorRatio = 0.0f;
	f32 interiorOffsetVolume = 1.0f;
	u32 clientVariable;
	reqSets->GetClientVariable(clientVariable);
	audWeatherSounds soundId = (audWeatherSounds)clientVariable;
	switch(soundId)
	{
	case AUD_WEATHER_SOUND_THUNDER:
		audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
		playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);
		interiorOffsetVolume = audDriverUtil::ComputeDbVolumeFromLinear(1.f - playerInteriorRatio);
		sound->SetRequestedVolume(interiorOffsetVolume);
		if( interiorOffsetVolume <= g_SilenceVolume)
		{
			sound->StopAndForget();
		}
		break;
	default:
		break;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateWind(u32 timeInMs, const f32* windSpeakerVolumes)
{
	u32 prevTypeIndex = g_weather.GetPrevTypeIndex();
	u32 nextTypeIndex = g_weather.GetNextTypeIndex();
	// Wind - also varies city noise in interiors
#if __BANK
	if(sm_HasToStopDebugSounds)
	{
		for (u32 i = 0; i < 2; ++i)
		{
			for(u32 j = 0; j<6; ++j)
			{
				if(m_WindSounds[i][j])
				{
					m_WindSounds[i][j]->StopAndForget(); 
					m_WindSounds[i][j] = NULL; 
				}
			}
		}
		sm_HasToStopDebugSounds = false;
	}
	if(sm_EnableWindTestTool)
	{
		prevTypeIndex = (u32)sm_OverridenPrevWeatherType;
		nextTypeIndex = (u32)sm_OverridenNextWeatherType;
	}
#endif

	if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor)
	{
		for (u32 i = 0; i < 2; ++i)
		{
			for(u32 j = 0; j<6; ++j)
			{
				if(m_WindSounds[i][j])
				{
					m_WindSounds[i][j]->StopAndForget(); 
					m_WindSounds[i][j] = NULL; 
				}
			}
		}
		for (u32 windSoundIndex = 0; windSoundIndex <6 ; windSoundIndex++)
		{
			if(	m_HighElevationWindSounds[windSoundIndex] )
			{
				m_HighElevationWindSounds[windSoundIndex]->SetReleaseTime(250);
				m_HighElevationWindSounds[windSoundIndex]->StopAndForget();
				m_HighElevationWindSounds[windSoundIndex] = NULL;
			}
		}
		return;
	}
	//References to the last and new weather types and it interpolation
	WeatherAudioSettings* oldWindType = GetWeatherAudioSettings(g_weather.GetTypeHashName( prevTypeIndex));
	WeatherAudioSettings* newWindType = GetWeatherAudioSettings(g_weather.GetTypeHashName( nextTypeIndex)); 
#if __BANK
	if(sm_EnableWindTestTool)
	{
		WeatherTypeAudioReference *ref = audNorthAudioEngine::GetObject<WeatherTypeAudioReference>("WeatherTypeList");
		if(Verifyf(ref, "Couldn't find weather type references"))
		{
			oldWindType =  audNorthAudioEngine::GetObject<WeatherAudioSettings>(ref->Reference[sm_OverridenPrevWeatherType].WeatherType);
			newWindType =  audNorthAudioEngine::GetObject<WeatherAudioSettings>(ref->Reference[sm_OverridenNextWeatherType].WeatherType);
		}
	}
#endif
	if(m_ScriptOverridingWind)
	{
		oldWindType = audNorthAudioEngine::GetObject<WeatherAudioSettings>(m_OldScriptWind);
		newWindType = audNorthAudioEngine::GetObject<WeatherAudioSettings>(m_NewScriptWind);
	}
	if ( !oldWindType || !newWindType )
	{
		return;
	}
	//First of all update the dynamic mixing.
	UpdateWeatherDynamicMix(oldWindType,newWindType);
	f32 interp = 1.0f - (BANK_ONLY(sm_EnableWindTestTool ? sm_WeatherInterp :) g_weather.GetInterp());
	m_WindStrength =  g_weather.GetWindSpeed() / g_weather.GetWindSpeedMax();
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Weather.Wind.Strength", 0xCD89BAFA),m_WindStrength);
	
#if __BANK
	if (sm_OverrideGustFactors)
	{
		m_GustSpeed = sm_GustSpeed;
	}
#endif

	f32 playerInteriorRatio = 0.0f;
	audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
	playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);
	f32 windInteriorOffsetVolume = audDriverUtil::ComputeDbVolumeFromLinear(1.f - playerInteriorRatio);
		
	UpdatePrevWindSounds(oldWindType,windSpeakerVolumes);
	UpdateNextWindSounds(newWindType,windSpeakerVolumes);
	UpdateWindElevationSounds(windSpeakerVolumes);

	UpdateGustBand(timeInMs,((interp < 0.5f) ? oldWindType : newWindType),windInteriorOffsetVolume);

#if __BANK
	if(sm_EnableWindTestTool)
	{
		if(sm_WeatherInterp == 1.f)
		{
			prevTypeIndex = nextTypeIndex;
		}
	}

	if(sm_DrawWindSpeakerVolumes)
	{		
		f32 actualDegrees = 45.0f;
		static audMeterList meter[4];
		static f32 meterBasePan[4] = {0, 90, 180, 270};
		static const char* directionName[4] = {"FL", "FR", "RR", "RL"};
		static f32 directionValue[4];

		for(u32 i = 0; i < 4; i++)
		{ 
			switch(i)
			{
			case 0:
				directionValue[i] = windSpeakerVolumes[RAGE_SPEAKER_ID_FRONT_LEFT];
				break;
			case 1:
				directionValue[i] = windSpeakerVolumes[RAGE_SPEAKER_ID_FRONT_RIGHT];
				break;
			case 2:
				directionValue[i] = windSpeakerVolumes[RAGE_SPEAKER_ID_BACK_RIGHT];
				break;
			case 3:
				directionValue[i] = windSpeakerVolumes[RAGE_SPEAKER_ID_BACK_LEFT];
				break;
			}			

			f32 cirleDegrees = meterBasePan[i] + (360.0f - actualDegrees) + 270.0f;
			while(cirleDegrees > 360.0f)
			{
				cirleDegrees -= 360.0f;
			}
			const f32 angle = cirleDegrees * (PI/180);

			meter[i].left = 150.f + (75.f * rage::Cosf(angle));
			meter[i].width = 50.f;
			meter[i].bottom = 400.f + (75.f * rage::Sinf(angle));
			meter[i].height = 50.f;
			meter[i].names = &directionName[i];
			meter[i].values = &directionValue[i];
			meter[i].numValues = 1;
			audNorthAudioEngine::DrawLevelMeters(&meter[i]);
		}
	}
#endif
	if(!m_ScriptOverridingWind)
	{
		// First of all check if we need to swap the old and new indeces:
		if(	m_OldWeatherTypeLastIndex != prevTypeIndex)
		{
			//We just move to the next weather type
			if(prevTypeIndex == m_NewWeatherTypeLastIndex)
			{
				// Old become new and viceversa
				m_OldWeatherTypeSoundsIndex = (m_OldWeatherTypeSoundsIndex + 1)%2;
				m_OldWeatherTypeLastIndex = prevTypeIndex;
#if __BANK
				if(sm_EnableWindTestTool)
				{
					sm_OverridenPrevWeatherType = sm_OverridenNextWeatherType;
					sm_WeatherInterp = 0.f;
				}
#endif
			}
		}
	}
	m_ScriptWasOverridingWind = m_ScriptOverridingWind;
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::CalculateWindSpeakerVolumes(f32* volumes) const
{
	//Update wind sounds.
	const Vec3V north = Vec3V(0.0f, 1.0f, 0.0f);
	const Vec3V east = Vec3V(1.0f,0.0f,0.0f);
	const Vec3V right = NormalizeFast(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().a());
	const ScalarV cosangle = Dot(right, north);

	const ScalarV angle = Arccos(Clamp(cosangle, ScalarV(V_NEGONE), ScalarV(V_ONE)));
	const ScalarV degrees = angle * ScalarV(V_TO_DEGREES);
	ScalarV actualDegrees;

	if(IsTrue(IsLessThanOrEqual(Dot(right, east), ScalarV(V_ZERO))))
	{
		actualDegrees = ScalarV(360.0f) - degrees;
	}
	else
	{
		actualDegrees = degrees;
	}

	s32 basePan = 360 - ((s32)actualDegrees.Getf() - 90);
	const s32 directionalPans[4] = {0,90,180,270};

	// Pre-calculate all the per-direction speaker volumes, as these will be the same for each directional ambience
	atRangeArray<f32, 16> perDirectionSpeakerVolumes;

	for(u32 direction = 0; direction < 4; direction++)
	{
		const s16 pan = (basePan + directionalPans[direction]) % 360;
		sm_MetricsInternal.environmentSoundMetrics = &sm_Metrics;

		// Just pretend we're using the positioned route so that ComputeSpeakerVolumesFromPosition gives us correct ear attenuation values
		sm_Metrics.routingMetric.effectRoute = EFFECT_ROUTE_POSITIONED;
		sm_Metrics.pan = pan;

		g_AudioEngine.GetEnvironment().ComputeSpeakerVolumesFromPan(&sm_MetricsInternal, audDriver::GetNumOutputChannels());

		for(u32 speakerIndex = 0; speakerIndex < 4; speakerIndex++)
		{
			perDirectionSpeakerVolumes[(direction * 4) + speakerIndex] = sm_MetricsInternal.relativeChannelVolumes[speakerIndex].GetLinear();
		}
	}
	f32 playerInteriorRatio = 0.0f;
	audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
	playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);

	for(u32 direction = 0; direction < 4; direction++)
	{
		const f32 blockedVolAttenLin = (f32)m_RainBlockedVolCurve.CalculateValue(1.0f - audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)direction));
		const f32 exteriorOcclusionVolAttenLin = m_RainExteriorOcclusionVolCurve.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection((audAmbienceDirection)direction));
		const f32 occlusionVolAttenLin = Lerp(playerInteriorRatio, blockedVolAttenLin,Min(blockedVolAttenLin,exteriorOcclusionVolAttenLin));

		f32 channelVolume[4];

		for(u32 speakerIndex=0; speakerIndex<4; speakerIndex++)
		{
			channelVolume[speakerIndex] = perDirectionSpeakerVolumes[(direction * 4) + speakerIndex];
		}

		// We're using a single panned sound, so we want to ensure that the overall volume emitted by all speakers is equal to our 
		// desired level, and just alter what proportion is coming from each speaker. Therefore work out our channel contribution as
		// fraction of the summed channel contribution and use this as a per-speaker scalar.
		f32 totalChannelVolume = 0.0f;
		for(u32 loop = 0; loop < 4; loop++)
		{
			totalChannelVolume += channelVolume[loop];
		}

		if(totalChannelVolume > 0.0f)
		{
			f32 invChannelVolume = 1.0f/totalChannelVolume;
			volumes[RAGE_SPEAKER_ID_FRONT_LEFT] += occlusionVolAttenLin * (channelVolume[RAGE_SPEAKER_ID_FRONT_LEFT] * invChannelVolume);
			volumes[RAGE_SPEAKER_ID_FRONT_RIGHT] += occlusionVolAttenLin * (channelVolume[RAGE_SPEAKER_ID_FRONT_RIGHT] * invChannelVolume);
			volumes[RAGE_SPEAKER_ID_BACK_LEFT] += occlusionVolAttenLin * (channelVolume[RAGE_SPEAKER_ID_BACK_LEFT] * invChannelVolume);
			volumes[RAGE_SPEAKER_ID_BACK_RIGHT] += occlusionVolAttenLin * (channelVolume[RAGE_SPEAKER_ID_BACK_RIGHT] * invChannelVolume);
		}
	}
}
//----------------------------------------------------------------------------------------------------------------------
bool audWeatherAudioEntity::RequestStreamedWeatherOneShot(const audMetadataRef soundRef, const audMetadataRef fallbackSoundRef, bool playEvenIfNoFreeSlots, const u32 desiredTriggerTime, const u32 maxTriggerTime, audStreamedWeatherSoundType soundType, const f32 rumbleSpikeIntensity, const u32 lpfCutoff BANK_ONLY(, f32 distanceToTravel))
{
	WeatherStreamedSoundSlot* streamedSoundSlot = &m_StreamedWeatherSoundSlots[soundType];

	// Check if the slot is free
	if(streamedSoundSlot->streamedSoundState == STREAMED_SOUND_STATE_IDLE && !streamedSoundSlot->sound && !streamedSoundSlot->speechSlot)
	{
		streamedSoundSlot->usingFallbackSound = false;
		streamedSoundSlot->lpfCutoff = lpfCutoff;
		streamedSoundSlot->rumbleSpikeIntensity = rumbleSpikeIntensity;
		streamedSoundSlot->fallbackSoundRef = fallbackSoundRef;
		streamedSoundSlot->desiredTriggerTime = desiredTriggerTime;
		streamedSoundSlot->maxTriggerTime = maxTriggerTime;
		BANK_ONLY(streamedSoundSlot->distanceToTravel = distanceToTravel;)

		// Try to grab a free ambient speech slot at low priority
		s32 speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL, NULL, -1.0f);

		if(speechSlotId >= 0)
		{
			audWaveSlot* speechSlot = g_SpeechManager.GetAmbientWaveSlotFromId(speechSlotId);

			// Success! Use this slot to load and play our asset
			if(speechSlot)
			{
				streamedSoundSlot->speechSlotID = speechSlotId;
				streamedSoundSlot->speechSlot = speechSlot;

				audSoundInitParams initParams;
				initParams.WaveSlot = speechSlot;
				initParams.AllowLoad = true;
				initParams.PrepareTimeLimit = 5000;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = (u32)AUD_WEATHER_SOUND_THUNDER;

				g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
					"WEATHER_AUDIO_ENTITY_TRIGGERED",
#endif
					0);

				CreateSound_PersistentReference(soundRef, &streamedSoundSlot->sound, &initParams);

				if(!streamedSoundSlot->sound)
				{
					// Free the speech slot - this will trigger our fallback sound
					g_SpeechManager.FreeAmbientSpeechSlot(speechSlotId, true);
					streamedSoundSlot->speechSlot = NULL;
				}

				if(streamedSoundSlot->sound)
				{
					streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_PREPARING;
					streamedSoundSlot->sound->Prepare(speechSlot, true);
					return true;
				}
			}
			else
			{
				// Free the speech slot - this will trigger our fallback sound
				g_SpeechManager.FreeAmbientSpeechSlot(speechSlotId, true);
				streamedSoundSlot->speechSlot = NULL;
			}
		}

		if(!streamedSoundSlot->speechSlot)
		{
			streamedSoundSlot->usingFallbackSound = true;
			CreateSound_PersistentReference(fallbackSoundRef, &streamedSoundSlot->sound);
			streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_PREPARING;
			return true;
		}
	}
	else if(playEvenIfNoFreeSlots)
	{
		// If we have no free slots, just play a fire and forget version of the fallback sound
		audSoundInitParams initParams;
		initParams.Predelay = desiredTriggerTime - g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		initParams.LPFCutoff = lpfCutoff;
		initParams.UpdateEntity = true;
		initParams.u32ClientVar = (u32)AUD_WEATHER_SOUND_THUNDER;
		CreateAndPlaySound(fallbackSoundRef, &initParams);

		if(streamedSoundSlot->rumbleSpikeIntensity > 0.0f)
		{
			AddRumbleSpikeEvent(initParams.Predelay, rumbleSpikeIntensity BANK_ONLY(, distanceToTravel));
		}

		return true;
	}

	return false;
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateStreamedWeatherSounds()
{
	for(u32 slot = 0; slot < m_StreamedWeatherSoundSlots.GetMaxCount(); slot++)
	{
		WeatherStreamedSoundSlot* streamedSoundSlot = &m_StreamedWeatherSoundSlots[slot];

		switch (streamedSoundSlot->streamedSoundState)
		{
		case STREAMED_SOUND_STATE_PLAYING:
			{
				if(!streamedSoundSlot->sound)
				{
					if(streamedSoundSlot->speechSlotID >= 0)
					{
						g_SpeechManager.FreeAmbientSpeechSlot(streamedSoundSlot->speechSlotID, true);
						streamedSoundSlot->speechSlotID = -1;
						streamedSoundSlot->speechSlot = NULL;
					}

					streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_IDLE;
				}
			}
			break;
		case STREAMED_SOUND_STATE_PREPARING:
			{
				if(streamedSoundSlot->sound)
				{
					audWaveSlot* waveSlot = streamedSoundSlot->speechSlot;
					audPrepareState prepareState = streamedSoundSlot->sound->Prepare(waveSlot, true);

					if(prepareState == AUD_PREPARE_FAILED)
					{
						// Ditch the speech slot, revert to resident sound
						if(!streamedSoundSlot->usingFallbackSound)
						{
							StopAndForgetSounds(streamedSoundSlot->sound);
							CreateSound_PersistentReference(streamedSoundSlot->fallbackSoundRef, &streamedSoundSlot->sound);
							streamedSoundSlot->usingFallbackSound = true;
							streamedSoundSlot->speechSlot = NULL;
						}
						else
						{
							// Buh? The fallback sound failed too - just mark this as playing so the slot get cleaned up next update
							if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) > streamedSoundSlot->desiredTriggerTime)
							{
								streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_PLAYING;
							}
						}
					}
					else
					{
						if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) > streamedSoundSlot->maxTriggerTime)
						{
							// If we exceeded the trigger time but haven't finished preparing, fall back to the resident sound
							if(prepareState == AUD_PREPARING && !streamedSoundSlot->usingFallbackSound)
							{
								StopAndForgetSounds(streamedSoundSlot->sound);
								CreateSound_PersistentReference(streamedSoundSlot->fallbackSoundRef, &streamedSoundSlot->sound);
								streamedSoundSlot->usingFallbackSound = true;
								streamedSoundSlot->speechSlot = NULL;
							}

							if(streamedSoundSlot->sound)
							{
								streamedSoundSlot->sound->PrepareAndPlay();
							}

							streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_PLAYING;
						}
						else if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) > streamedSoundSlot->desiredTriggerTime)
						{
							// If we've prepared in time, happy days - play the sound
							if(prepareState == AUD_PREPARED)
							{
								streamedSoundSlot->sound->PrepareAndPlay();
								streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_PLAYING;
							}
						}
					}

					// If we transitioned to the playing state this frame, immediately spike the rumble and set the LPF if required
					if(streamedSoundSlot->streamedSoundState == STREAMED_SOUND_STATE_PLAYING)
					{
						if(streamedSoundSlot->sound)
						{
							streamedSoundSlot->sound->SetRequestedLPFCutoff(streamedSoundSlot->lpfCutoff);
						}

						if(streamedSoundSlot->rumbleSpikeIntensity > 0.0f)
						{
							AddRumbleSpikeEvent(0, streamedSoundSlot->rumbleSpikeIntensity BANK_ONLY(, streamedSoundSlot->distanceToTravel));
						}
					}
				}
				else
				{
					if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) > streamedSoundSlot->desiredTriggerTime)
					{
						// Shouldn't get in this situation, but if so just mark the slot as playing so it gets cleaned up next update
						streamedSoundSlot->streamedSoundState = STREAMED_SOUND_STATE_PLAYING;

						if(streamedSoundSlot->rumbleSpikeIntensity > 0.0f)
						{
							AddRumbleSpikeEvent(0, streamedSoundSlot->rumbleSpikeIntensity BANK_ONLY(, streamedSoundSlot->distanceToTravel));
						}
					}
				}
			}
			break;
		default:
			break;
		}
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateWeatherDynamicMix(WeatherAudioSettings* oldWeatherType,WeatherAudioSettings* newWeatherType)
{
	naAssertf(oldWeatherType,"Null previous weather type");
	naAssertf(newWeatherType,"Null next weather type");
	u32 prevTypeIndex = g_weather.GetPrevTypeIndex();
	u32 nextTypeIndex = g_weather.GetNextTypeIndex();
#if __BANK
	if(sm_EnableWindTestTool)
	{
		prevTypeIndex = (u32)sm_OverridenPrevWeatherType;
		nextTypeIndex = (u32)sm_OverridenNextWeatherType;
	}
#endif
	if(m_OldWeatherTypeLastIndex != prevTypeIndex || (m_ScriptOverridingWind && !m_ScriptWasOverridingWind )
		|| (!m_ScriptOverridingWind && m_ScriptWasOverridingWind))
	{
		if(m_OldWeatherTypeScene)
		{
			m_OldWeatherTypeScene->Stop();
			m_OldWeatherTypeScene = NULL;
			//The old weather type last index will be updated later on the update wind. 
		}
	}
	if(m_NewWeatherTypeLastIndex != nextTypeIndex || (m_ScriptOverridingWind && !m_ScriptWasOverridingWind )
		|| (!m_ScriptOverridingWind && m_ScriptWasOverridingWind))
	{
		if(m_NewWeatherTypeScene)
		{
			m_NewWeatherTypeScene->Stop();
			m_NewWeatherTypeScene = NULL;
			//The new weather type last index will be updated later on the update wind. 
		}
	}
	if(!m_OldWeatherTypeScene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(oldWeatherType->AudioScene); 
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_OldWeatherTypeScene, NULL);
		}
	}
	if(!m_NewWeatherTypeScene)
	{
		// Start the scene if we haven't do it already.
		MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(newWeatherType->AudioScene); 
		if(sceneSettings)
		{
			DYNAMICMIXER.StartScene(sceneSettings, &m_NewWeatherTypeScene, NULL);
		}
	}
	f32 interp = 1 - (BANK_ONLY(sm_EnableWindTestTool ? sm_WeatherInterp :) g_weather.GetInterp());
	if(m_OldWeatherTypeScene)
	{
		m_OldWeatherTypeScene->SetVariableValue(ATSTRINGHASH("apply",0xE865CDE8),interp);
	}
	if(m_NewWeatherTypeScene)
	{
		m_NewWeatherTypeScene->SetVariableValue(ATSTRINGHASH("apply",0xE865CDE8),BANK_ONLY(sm_EnableWindTestTool ? sm_WeatherInterp :) g_weather.GetInterp());
	}
}

//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdatePrevWindSounds(WeatherAudioSettings* weatherType,const f32* volumes)
{	
	naAssertf(weatherType,"Null previous weather type");
	u32 prevTypeIndex = g_weather.GetPrevTypeIndex();
#if __BANK
	if(sm_EnableWindTestTool)
	{
		prevTypeIndex = (u32)sm_OverridenPrevWeatherType;
	}
#endif
	if(	m_OldWeatherTypeLastIndex != prevTypeIndex || (m_ScriptOverridingWind && !m_ScriptWasOverridingWind )
		|| (!m_ScriptOverridingWind && m_ScriptWasOverridingWind))
	{
		for(u32 j = 0; j<6; ++j)
		{
			if(m_WindSounds[m_OldWeatherTypeSoundsIndex][j])
			{
				m_WindSounds[m_OldWeatherTypeSoundsIndex][j]->StopAndForget(); 
				m_WindSounds[m_OldWeatherTypeSoundsIndex][j] = NULL; 
			}
		}
		m_OldWeatherTypeLastIndex = prevTypeIndex; 
	}
	f32 prevInterpThreshold = 1.f/(f32)weatherType->numWindSounds;
	f32 interp = BANK_ONLY(sm_EnableWindTestTool ? sm_WeatherInterp :) g_weather.GetInterp();
	interp = 1.f - interp;
	for (u32 windSoundIndex = 0; windSoundIndex <weatherType->numWindSounds ; windSoundIndex++)
	{
		if(interp > windSoundIndex * prevInterpThreshold)
		{
			if(!m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex])
			{
				audSoundInitParams initParams; 
				CreateSound_PersistentReference(weatherType->WindSound[windSoundIndex].WindSoundComponent,&m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex],&initParams);
				if(m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex])
				{
#if __BANK
					if(sm_ShowWindSoundsInfo)
					{
						const char* weatherName = (sm_EnableWindTestTool ? ""  : g_weather.GetTypeName(prevTypeIndex));
						naDisplayf("Playing %s component %s",weatherName,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(weatherType->WindSound[windSoundIndex].WindSoundComponent));
					}
#endif
					m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex]->SetRequestedQuadSpeakerLevels(volumes);
					m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex]->PrepareAndPlay();
				}
			}
		}
		else 
		{
			if(	m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex] )
			{
#if __BANK
				if(sm_ShowWindSoundsInfo)
				{
					const char* weatherName = (sm_EnableWindTestTool ? "" :g_weather.GetTypeName(prevTypeIndex));
					naDisplayf("Stopping %s component %s",weatherName,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(weatherType->WindSound[windSoundIndex].WindSoundComponent));
				}
#endif
				m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex]->StopAndForget();
				m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex] = NULL;
			}
		}
		if(m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex])
		{
			//f32 newVolume = m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex]->GetRequestedVolume() + volume ;
			//newVolume = Max(newVolume, g_SilenceVolume);
			m_WindSounds[m_OldWeatherTypeSoundsIndex][windSoundIndex]->SetRequestedQuadSpeakerLevels(volumes);
		}
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateNextWindSounds(WeatherAudioSettings* weatherType,const f32* volumes)
{	
	naAssertf(weatherType,"Null next weather type");
	u32 nextTypeIndex = g_weather.GetNextTypeIndex();
#if __BANK
	if(sm_EnableWindTestTool)
	{
		nextTypeIndex = (u32)sm_OverridenNextWeatherType;
	}
#endif
	if(	m_NewWeatherTypeLastIndex != nextTypeIndex || (m_ScriptOverridingWind && !m_ScriptWasOverridingWind )
		|| (!m_ScriptOverridingWind && m_ScriptWasOverridingWind))
	{
		for(u32 j = 0; j<6; ++j)
		{
			if(m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][j])
			{
				m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][j]->StopAndForget(); 
				m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][j] = NULL; 
			}
		}
		m_NewWeatherTypeLastIndex =  nextTypeIndex;
	}

	f32 nextInterpThreshold = 1.f/(f32)weatherType->numWindSounds;
	f32 interp = BANK_ONLY(sm_EnableWindTestTool ? sm_WeatherInterp :) g_weather.GetInterp();
	for (u32 windSoundIndex = 0; windSoundIndex <weatherType->numWindSounds ; windSoundIndex++)
	{
		if(interp > windSoundIndex * nextInterpThreshold)
		{
			if(!m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex])
			{
				audSoundInitParams initParams; 
				CreateSound_PersistentReference(weatherType->WindSound[windSoundIndex].WindSoundComponent,&m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex],&initParams);
				if(m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex])
				{
#if __BANK
					if(sm_ShowWindSoundsInfo)
					{
						const char* weatherName = (sm_EnableWindTestTool ? "" :g_weather.GetTypeName(nextTypeIndex));
						naDisplayf("Playing %s component %s",weatherName,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(weatherType->WindSound[windSoundIndex].WindSoundComponent));
					}
#endif
					m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex]->SetRequestedQuadSpeakerLevels(volumes );
					m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex]->PrepareAndPlay();
				}
			}
		}
		else 
		{
			if(	m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex] )
			{
#if __BANK
				if(sm_ShowWindSoundsInfo)
				{
					const char* weatherName = (sm_EnableWindTestTool ? "" :g_weather.GetTypeName(nextTypeIndex));
					naDisplayf("Stopping %s component %s",weatherName,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(weatherType->WindSound[windSoundIndex].WindSoundComponent));
				}
#endif
				m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex]->StopAndForget();
				m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex] = NULL;
			}
		}
		if(m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex])
		{
			/*f32 newVolume = m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex]->GetRequestedVolume() + volume ;
			newVolume = Max(newVolume, g_SilenceVolume);*/
			m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex]->SetRequestedQuadSpeakerLevels(volumes);
		}
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateWindElevationSounds(const f32* volumes)
{
	WeatherAudioSettings* highElevationWind = NULL;
	if( g_AmbientAudioEntity.GetCurrentWindElevationZone() && g_AmbientAudioEntity.GetCurrentWindElevationZone()->GetZoneData())
	{
		highElevationWind = audNorthAudioEngine::GetObject<WeatherAudioSettings>(g_AmbientAudioEntity.GetCurrentWindElevationZone()->GetZoneData()->WindElevationSounds);
	}
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OverrideElevationWind))
	{
		highElevationWind = audNorthAudioEngine::GetObject<WeatherAudioSettings>(sm_ScriptElevationWindHash);
	}
	if(highElevationWind)
	{
		f32 normalizedHeight = g_AmbientAudioEntity.GetWorldHeightFactorAtListenerPosition();

		f32 highElevationThreshold = 1.f/(f32)highElevationWind->numWindSounds;
		for (u32 windSoundIndex = 0; windSoundIndex <highElevationWind->numWindSounds ; windSoundIndex++)
		{
			if(normalizedHeight > windSoundIndex * highElevationThreshold)
			{
				if(!m_HighElevationWindSounds[windSoundIndex])
				{
					audSoundInitParams initParams; 
					CreateSound_PersistentReference(highElevationWind->WindSound[windSoundIndex].WindSoundComponent,&m_HighElevationWindSounds[windSoundIndex],&initParams);
					if(m_HighElevationWindSounds[windSoundIndex])
					{
#if __BANK
						if(sm_ShowWindSoundsInfo)
						{
							naDisplayf("Playing WEATHER_TYPES_HIGH_ELEVATION component %s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(highElevationWind->WindSound[windSoundIndex].WindSoundComponent));
						}
#endif
						m_HighElevationWindSounds[windSoundIndex]->SetRequestedQuadSpeakerLevels(volumes);
						m_HighElevationWindSounds[windSoundIndex]->PrepareAndPlay();
					}
				}
			}
			else 
			{
				if(	m_HighElevationWindSounds[windSoundIndex] )
				{
#if __BANK
					if(sm_ShowWindSoundsInfo)
					{
						naDisplayf("Stopping WEATHER_TYPES_HIGH_ELEVATION component %s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(highElevationWind->WindSound[windSoundIndex].WindSoundComponent));
					}
#endif
					m_HighElevationWindSounds[windSoundIndex]->StopAndForget();
					m_HighElevationWindSounds[windSoundIndex] = NULL;
				}
			}
			if(m_HighElevationWindSounds[windSoundIndex])
			{
				/*f32 newVolume = m_WindSounds[1 - m_OldWeatherTypeSoundsIndex][windSoundIndex]->GetRequestedVolume() + volume ;
				newVolume = Max(newVolume, g_SilenceVolume);*/
				m_HighElevationWindSounds[windSoundIndex]->SetRequestedQuadSpeakerLevels(volumes);
			}
		}
	}
	else
	{
		for (u32 windSoundIndex = 0; windSoundIndex <6 ; windSoundIndex++)
		{
			if(	m_HighElevationWindSounds[windSoundIndex] )
			{
#if __BANK
				if(sm_ShowWindSoundsInfo)
				{
					naDisplayf("Stopping WEATHER_TYPES_HIGH_ELEVATION component %s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(highElevationWind->WindSound[windSoundIndex].WindSoundComponent));
				}
#endif
				m_HighElevationWindSounds[windSoundIndex]->SetReleaseTime(250);
				m_HighElevationWindSounds[windSoundIndex]->StopAndForget();
				m_HighElevationWindSounds[windSoundIndex] = NULL;
			}
		}
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateGustBand(u32 timeInMs,WeatherAudioSettings *windType,f32 volume)
{
	bool newGust = (!m_GustInProgress && g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) >= m_NextGustTime);
#if __BANK
	// Manually triggered gusts
	if (sm_ManualGust)
	{
		newGust = false;
		if (sm_TriggerGust && !m_GustInProgress)
		{
			sm_TriggerGust = false;
			newGust = true;
		}
	}
#endif
	if (newGust)
	{
		// Grab the current wind direction at the listener, and the current speed
		m_GustSpeed = m_GustSpeed * audEngineUtil::GetRandomNumberInRange(0.85f, 1.15f);
		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		Vector3 windSpeed;
		WIND.GetLocalVelocity(RCC_VEC3V(listenerPosition), RC_VEC3V(windSpeed));
		windSpeed.z = 0.0f;
		windSpeed.NormalizeSafe();
		m_GustDirection = windSpeed;
		listenerPosition.z = 0.0f;
		Vector3 normalToGust;
		normalToGust.Cross(windSpeed, Vector3(0.0f, 0.0f, 1.0f));
		m_InitialGustLineStart = listenerPosition + 100.0f*normalToGust; // these 100 and 200 are width related, not length
		m_GustLineStartToEnd = -200.0f*normalToGust;
		// Now pull it back 50m, and start it moving down
		m_GustStartTime = timeInMs;
		m_GustInProgress = true;
		// Play gust sound from the weater type with more importance
		audSoundInitParams initParams;
		initParams.Position = m_GustMiddlePoint;
		initParams.Volume = volume;
		CreateAndPlaySound_Persistent(windType->WindGust,&m_GustSound,&initParams);
	}																													  
	if (!m_GustInProgress)
	{
		return;
	}

	// Update our gust
	f32 lengthFromStart = (-1.0f*m_GustAheadWidth) + m_GustSpeed * ((f32)(timeInMs-m_GustStartTime)) / 1000.0f;
	m_GustLineStart = m_InitialGustLineStart;
	m_GustLineStart.AddScaled(m_GustDirection, lengthFromStart);
	m_GustMiddlePoint = 0.5f * m_GustLineStartToEnd + m_GustLineStart;

	// Has it finished yet
	if (lengthFromStart>(m_GustBehindWidth))
	{
		if(m_GustSound)
		{
			m_GustSound->StopAndForget();
			m_GustSound = NULL;
		}
		m_GustInProgress = false;
		m_NextGustTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + (u32)audEngineUtil::GetRandomNumberInRange((f32)sm_MinTimeToTriggerGust,(f32)sm_MaxTimeToTriggerGust);
		// Play gust end sounds if we have treesin the current secto.
		if(audNorthAudioEngine::GetGtaEnvironment()->ShouldTriggerWindGustEnd())
		{
			if(windType->WindGustEnd != g_NullSoundHash)
			{
				audSoundInitParams initParams;
				initParams.Position = m_GustMiddlePoint;
				g_AmbientAudioEntity.TriggerStreamedAmbientOneShot(windType->WindGustEnd, &initParams);
			}
		}
	}
	
	if(m_GustSound)
	{
		m_GustSound->SetRequestedPosition(m_GustMiddlePoint);
		f32 gustRatio = lengthFromStart/m_GustAheadWidth;
		m_GustSound->SetVariableValueDownHierarchyFromName("gustRatio",gustRatio);
		m_GustSound->SetRequestedVolume(volume);
	}
#if __BANK
	if (sm_DebugGust)
	{
		Vector3 camera = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		f32 height = camera.z + 5.0f;
		Vector3 gustDir = m_GustMiddlePoint + 1000.0f * m_GustDirection;
		grcDebugDraw::Line(m_GustMiddlePoint+Vector3(0.0f, 0.0f, height), gustDir+Vector3(0.0f, 0.0f, height), Color32(255, 0, 0, 255));
		grcDebugDraw::Line(m_GustLineStart+Vector3(0.0f, 0.0f, height), m_GustLineStart+m_GustLineStartToEnd+Vector3(0.0f, 0.0f, height), Color32(255, 0, 0, 255));
		}
#endif
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateRain()
{
	f32 rainVolLin = m_RainToVolumeCurve.CalculateValue(g_weather.GetTimeCycleAdjustedRain());
	m_RainVolume = audDriverUtil::ComputeDbVolumeFromLinear(rainVolLin);
	//Set global variables (RainLevel, RailOccluddedLevel, RoadWetness)
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Weather.Rain.Level", 0x93C9A4A0), g_weather.GetTimeCycleAdjustedRain());
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Weather.Rain.LinearVolume", 0x2930166A), rainVolLin);
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Weather.RoadWetness", 0xB6749CC4), g_weather.GetWetness());

	if(m_RainVolume <= g_SilenceVolume)
	{
		for(u32 i = 0; i < 4; i++)
		{
			for(u32 j = 0; j < 4; j++)
			{
				if(m_RainSounds[i][j])
				{
					m_RainSounds[i][j]->SetReleaseTime(1000);
					m_RainSounds[i][j]->StopAndForget();
				}
				m_RainSmoothers[i][j].Reset();
			}
		}
	}
	else
	{
		// Compute rain mix
		static audRainXfade mixDirections[4];
		const Vector3 listenerPos = audNorthAudioEngine::GetMicrophones().GetAmbienceMic().d;
		const f32 buildingHeight = Max(0.f, audNorthAudioEngine::GetGtaEnvironment()->GetAveragedBuildingHeightForIndex(AUD_CENTER_SECTOR) - 32.f);
		const f32 camHeightFromBuilding = Max(listenerPos.z - buildingHeight, 0.f);
        bool ignoreCameraHeight = false;

        // GTA V Hack
        CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

        // V director mode trailer is actually miles up in the air - if this interior is active then ignore any 
        // camera height values otherwise the rain will be silent
        if (pIntInst)
        {
            CMloModelInfo *pModelInfo = reinterpret_cast<CMloModelInfo*>(pIntInst->GetBaseModelInfo());            
            ignoreCameraHeight = (pModelInfo && pModelInfo->GetModelNameHash() == ATSTRINGHASH("milo_replay", 0x3e9967cc));            
        }
        // End GTA V Hack

		const f32 heightAboveWorldBlanket = ignoreCameraHeight? 0.f : MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.z - CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.x, MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.y);
		const f32 rainHeightVolOffset = m_WorlHeightToRainVolume.CalculateValue(heightAboveWorldBlanket);
		const f32 smoothedDist = 0;// m_RainDistanceSmoother.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetListenerDistanceToNearestUncoveredNavmeshPoly(),timeInMs);
		const f32 distVol = m_RainDistanceToNonCoverCurve.CalculateValue(smoothedDist);
		// default roll off with a 2 distance scale factor
		const u32 listenerSector = static_cast<u32>(audNorthAudioEngine::GetGtaEnvironment()->ComputeWorldSectorIndex(listenerPos));
		audNorthAudioEngine::GetGtaEnvironment()->ComputeRainMixForSector(listenerSector, distVol * rainVolLin * Max(0.33f, audDriverUtil::ComputeLinearVolumeFromDb((g_weather.GetLightning()*9.f) + m_RainHeightVolumeCurve.CalculateValue(camHeightFromBuilding * 0.1f))), &mixDirections[0]);

		f32 blockedFactors[] = {1.f - audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection(AUD_AMB_DIR_NORTH),
								1.f - audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection(AUD_AMB_DIR_EAST),
								1.f - audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection(AUD_AMB_DIR_SOUTH),
								1.f - audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection(AUD_AMB_DIR_WEST)};

		f32 exteriorOcclusion[]  = {audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection(AUD_AMB_DIR_NORTH),
			audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection(AUD_AMB_DIR_EAST),
			audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection(AUD_AMB_DIR_SOUTH),
			audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection(AUD_AMB_DIR_WEST)};

		bool alreadyOccluding = (exteriorOcclusion[0] == 0.f) && (exteriorOcclusion[1] == 0.f) && (exteriorOcclusion[2] == 0.f) && (exteriorOcclusion[3] == 0.f);
		if( g_AmbientAudioEntity.HasToOccludeRain() && !alreadyOccluding)
		{
			exteriorOcclusion[0] = 0.5f;
			exteriorOcclusion[1] = 0.5f;
			exteriorOcclusion[2] = 0.5f;
			exteriorOcclusion[3] = 0.5f;
		}
		f32 playerInteriorRatio = 0.0f;
		audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
		playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);
		
		f32 LPFAttenuation[4] = {23900.f,23900.f,23900.f,23900.f};

		f32 underCoverFactor = audNorthAudioEngine::GetGtaEnvironment()->GetUnderCoverFactor();
		f32 underCoverAttenLin = 1.f;
		f32 fLinearRatio;
		u32 i;

		const u32 numPoints = 3;			//input,vol,LPF
		Vector3 points[numPoints] = {Vector3(0.f,1.f,23900.f),Vector3(55.f,0.9f,9900.f),Vector3(100.f,0.85f,4000.f)};
		underCoverFactor = Selectf(underCoverFactor - points[numPoints -1].x, points[numPoints -1].x, underCoverFactor);
		underCoverFactor = Selectf(points[0].x - underCoverFactor, points[0].x, underCoverFactor);

		// see which straight-line section we're in
		for (i=1; i<numPoints; i++)
		{
			if (points[i].x >= underCoverFactor)
				break;
		}

		fLinearRatio = ( (underCoverFactor-points[i-1].x) / (points[i].x-points[i-1].x) );

		underCoverAttenLin = points[i-1].y + fLinearRatio * ( points[i].y - points[i-1].y );
		f32 underCoverLPF = points[i-1].z + fLinearRatio * ( points[i].z - points[i-1].z );
		for(u32 i = 0; i < 4; i++)
		{
			f32 blockedFactor = (1.f - blockedFactors[i]) * 100.f;
			blockedFactor = Selectf(blockedFactor - points[numPoints -1].x, points[numPoints -1].x, blockedFactor);
			blockedFactor = Selectf(points[0].x - blockedFactor, points[0].x, blockedFactor);

			// see which straight-line section we're in
			u32 pointId;
			for (pointId=1; pointId<numPoints; pointId++)
			{
				if (points[pointId].x >= blockedFactor)
					break;
			}

			fLinearRatio = ( (blockedFactor-points[pointId-1].x) / (points[pointId].x-points[pointId-1].x) );
			const f32 blockedVolAttenLin =  Min(underCoverAttenLin,points[pointId-1].y + fLinearRatio * ( points[pointId].y - points[pointId-1].y) );
			LPFAttenuation[i] =  Min(underCoverLPF,points[pointId-1].z + fLinearRatio * ( points[pointId].z - points[pointId-1].z ));

			const f32 exteriorOcclusionVolAttenLin = m_RainExteriorOcclusionVolCurve.CalculateValue(exteriorOcclusion[i]);
			const f32 occlusionVolAttenLin = Lerp(playerInteriorRatio, blockedVolAttenLin,Min(blockedVolAttenLin,exteriorOcclusionVolAttenLin));

			f32 waterRain = ClampRange(mixDirections[i].waterRain,0.f,1.f) * occlusionVolAttenLin * rainHeightVolOffset;
			f32 buildingRain = ClampRange(mixDirections[i].buildingRain,0.f,1.f) * occlusionVolAttenLin * rainHeightVolOffset;
			f32 treeRain = ClampRange(mixDirections[i].treeRain,0.f,1.f) * occlusionVolAttenLin * rainHeightVolOffset;
			f32 heavyRain = distVol * occlusionVolAttenLin * rainHeightVolOffset;

			mixDirections[i].waterRain =  m_RainSmoothers[i][0].CalculateValue(waterRain,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			mixDirections[i].buildingRain =  m_RainSmoothers[i][1].CalculateValue(buildingRain,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			mixDirections[i].treeRain = m_RainSmoothers[i][2].CalculateValue(treeRain,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			mixDirections[i].heavyRain = m_RainSmoothers[i][3].CalculateValue(heavyRain,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));

		}
		audSoundInitParams initParams;
		initParams.AttackTime = 1000;
		for(u8 i = 0; i < 4; i++)
		{
			bool playWater = (sm_NumRainSoundPerChannel == 0 ? false : true);
			bool playTarmac = (sm_NumRainSoundPerChannel == 0 ? false : true);
			bool playTree = (sm_NumRainSoundPerChannel == 0 ? false : true);

			if(sm_NumRainSoundPerChannel == 2)
			{
				f32 minVol = Min(mixDirections[i].waterRain,mixDirections[i].buildingRain,mixDirections[i].treeRain);
				if(minVol == mixDirections[i].waterRain)
				{
					playWater = false;
					mixDirections[i].waterRain = 0.f;
				}
				else if (minVol == mixDirections[i].buildingRain)
				{
					playTarmac = false;
					mixDirections[i].buildingRain = 0.f;
				}
				else
				{
					playTree = false;
					mixDirections[i].treeRain = 0.f;
				}
			}
			else if(sm_NumRainSoundPerChannel == 1)
			{
				playWater = false;
				playTarmac = false;
				playTree = false;
				f32 maxVol = Max(mixDirections[i].waterRain,mixDirections[i].buildingRain,mixDirections[i].treeRain);
				if(maxVol == mixDirections[i].waterRain)
				{
					playWater = true;
					mixDirections[i].buildingRain = 0.f;
					mixDirections[i].treeRain = 0.f;
				}
				else if (maxVol == mixDirections[i].buildingRain)
				{
					playTarmac = true;
					mixDirections[i].waterRain = 0.f;
					mixDirections[i].treeRain = 0.f;
				}
				else
				{
					playTree = true;
					mixDirections[i].buildingRain = 0.f;
					mixDirections[i].waterRain = 0.f;
				}
			}
			else if(sm_NumRainSoundPerChannel == 0)
			{
				mixDirections[i].waterRain = 0.f;
				mixDirections[i].buildingRain = 0.f;
				mixDirections[i].treeRain = 0.f;
			}
			
			// Water Rain 
			if(playWater)
			{
				PlayWaterRain(i,mixDirections[i].waterRain,mixDirections[i].pan,LPFAttenuation[i]);
			}
			else
			{
				StopRainSound(0,i);
			}
			//Tarmac rain
			if(playTarmac)
			{
				PlayTarmacRain(i,mixDirections[i].buildingRain,mixDirections[i].pan,LPFAttenuation[i]);
			}
			else
			{
				StopRainSound(1,i);
			}
			//Plants rain
			if(playTree)
			{
				PlayTreeRain(i,mixDirections[i].treeRain,mixDirections[i].pan,LPFAttenuation[i]);
			}
			else
			{
				StopRainSound(2,i);
			}
			//Heavy rain
			if(!m_RainSounds[i][3])
			{
				char soundName[64];
				formatf(soundName,"HEAVY_RAIN_%u",i+1);
				initParams.LPFCutoff = (u32)LPFAttenuation[i];
				CreateAndPlaySound_Persistent(m_RainSoundSet.Find(soundName), &m_RainSounds[i][3], &initParams);
			}
			if(m_RainSounds[i][3])
			{
				m_RainSounds[i][3]->SetRequestedLPFCutoff(LPFAttenuation[i]); 
				m_RainSounds[i][3]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(mixDirections[i].heavyRain)); 
				m_RainSounds[i][3]->SetRequestedPan(mixDirections[i].pan);
			}
		}
#if __BANK
	if(sm_ShowRainVolumes)
	{
		static audMeterList meterLists[4];
		static const char *namesList[4][4]={{"N:W","C","T","H"},
		{"E:W","C","T","H"},
		{"S:W","C","T","H"},
		{"W:W","C","T","H"}};

		for(u32 i = 0; i < 4; i++)
		{
			// 0 degrees on the unit circle is 90 degrees for our panning, so subtract 90 degrees (add 270)
			const f32 angle = (270.f + mixDirections[i].pan) * (PI/180);

			meterLists[i].left = 150.f + (75.f * rage::Cosf(angle));
			meterLists[i].width = 100.f;
			meterLists[i].bottom = 200.f + (75.f * rage::Sinf(angle));
			meterLists[i].height = 50.f;
			meterLists[i].names = namesList[i];
			meterLists[i].values = (f32*)&mixDirections[i].waterRain;
			meterLists[i].numValues = 4;
			audNorthAudioEngine::DrawLevelMeters(&meterLists[i]);
		}
	}
#endif
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::PlayWaterRain(const u8 soundIdx,const f32 volume,const u32 pan, const f32 LPFAttenuation)
{
	f32 waterRainVolume = audDriverUtil::ComputeDbVolumeFromLinear(volume); 
	if(waterRainVolume > g_SilenceVolume)
	{
		char soundName[64];
		formatf(soundName,"WATER_RAIN_%u",soundIdx+1);
		if(!m_RainSounds[soundIdx][0])
		{
			audSoundInitParams initParams;
			initParams.AttackTime = 1000;
			initParams.LPFCutoff = (u32)LPFAttenuation;
			CreateAndPlaySound_Persistent(m_RainSoundSet.Find(soundName), &m_RainSounds[soundIdx][0], &initParams);
		}
		if(m_RainSounds[soundIdx][0])
		{
			m_RainSounds[soundIdx][0]->SetRequestedLPFCutoff(LPFAttenuation);
			m_RainSounds[soundIdx][0]->SetRequestedVolume(waterRainVolume);
			m_RainSounds[soundIdx][0]->SetRequestedPan(pan);
		}
	}
	else if(m_RainSounds[soundIdx][0])
	{
		m_RainSounds[soundIdx][0]->StopAndForget();
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::PlayTarmacRain(const u8 soundIdx,const f32 volume,const u32 pan, const f32 LPFAttenuation)
{
	f32 tarmacRainVolume = audDriverUtil::ComputeDbVolumeFromLinear(volume); 
	if(tarmacRainVolume > g_SilenceVolume)
	{
		char soundName[64];
		formatf(soundName,"TARMAC_RAIN_%u",soundIdx+1);
		if(!m_RainSounds[soundIdx][1])
		{
			audSoundInitParams initParams;
			initParams.AttackTime = 1000;
			initParams.LPFCutoff = (u32)LPFAttenuation;
			CreateAndPlaySound_Persistent(m_RainSoundSet.Find(soundName), &m_RainSounds[soundIdx][1], &initParams);
		}
		if(m_RainSounds[soundIdx][1])
		{
			m_RainSounds[soundIdx][1]->SetRequestedLPFCutoff(LPFAttenuation);
			m_RainSounds[soundIdx][1]->SetRequestedVolume(tarmacRainVolume);
			m_RainSounds[soundIdx][1]->SetRequestedPan(pan);
		}
	}
	else if(m_RainSounds[soundIdx][1])
	{
		m_RainSounds[soundIdx][1]->StopAndForget();
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::PlayTreeRain(const u8 soundIdx,const f32 volume,const u32 pan, const f32 LPFAttenuation)
{
	f32 plantsRainVolume = audDriverUtil::ComputeDbVolumeFromLinear(volume); 
	if(plantsRainVolume > g_SilenceVolume)
	{
		char soundName[64];
		formatf(soundName,"PLANTS_RAIN_%u",soundIdx+1);
		if(!m_RainSounds[soundIdx][2])
		{
			audSoundInitParams initParams;
			initParams.AttackTime = 1000;
			initParams.LPFCutoff = (u32)LPFAttenuation;
			CreateAndPlaySound_Persistent(m_RainSoundSet.Find(soundName), &m_RainSounds[soundIdx][2], &initParams);
		}
		if(m_RainSounds[soundIdx][2])
		{
			audSoundInitParams initParams;
			initParams.AttackTime = 1000;
			m_RainSounds[soundIdx][2]->SetRequestedLPFCutoff(LPFAttenuation);
			m_RainSounds[soundIdx][2]->SetRequestedVolume(plantsRainVolume);
			m_RainSounds[soundIdx][2]->SetRequestedPan(pan);
		}
	}
	else if(m_RainSounds[soundIdx][2])
	{
		m_RainSounds[soundIdx][2]->StopAndForget();
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::StopRainSound(const u8 rainSoundIdx,const u8 soundIdx)
{
	if(m_RainSounds[soundIdx][rainSoundIdx])
	{
		m_RainSounds[soundIdx][rainSoundIdx]->StopAndForget();
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::AddCLoudBurstSpikeEvent(const Vector3 &pos, const f32 rumbleIntensity)
{
	if(rumbleIntensity == 0.f)
		return;
	// Get the distance from the listener to the sound source.
	f32 distanceToTravel = fabs((pos - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag()) * sm_CloudBurstDistanceCorrection;
	u32 delay = (u32)(distanceToTravel/343.2f) * 1000;

	//f32 intensity = rumbleIntensity * sm_DistanceToRumbleIntensityScale.CalculateValue(ClampRange(distanceToTravel,sm_CloudBurstMinDistance,sm_CloudBurstMaxDistance));
	AddRumbleSpikeEvent(delay,rumbleIntensity BANK_ONLY(,distanceToTravel));

}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::TriggerThunderBurst(const f32 intensity)
{
	if(intensity == 0.f)
		return;
	// Get the distance from the listener to the sound source.
	u32 delay = (u32)(sm_MaxBurstDelay * sm_BurstIntensityToDelayFactor.CalculateValue(intensity));

	if(sm_LastThunderStrikeTriggeredTime + sm_TimeToRetriggerThunderStrike <  g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
	{
		u32 triggerTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + delay;

		// This value represents the max amount of time we're willing to wait for assets to stream in - if we pass this then we'll fallback to the 
		// resident sound instead
		const u32 maxTriggerTimeVariance = 0;

		RequestStreamedWeatherOneShot(sm_ThunderSounds.Find(ATSTRINGHASH("STRIKE", 0x7E20602)), 
									  sm_ThunderSounds.Find(ATSTRINGHASH("FALLBACK_STRIKE", 0x74D67DED)),
									  true,
									  triggerTime, 
									  triggerTime + maxTriggerTimeVariance, 
									  STREAMED_SOUND_TYPE_STRIKE, 
									  intensity,
									  (u32)sm_BurstIntensityToLPFCutOff.CalculateValue(intensity) BANK_ONLY(, 0.0f));

		RequestStreamedWeatherOneShot(sm_ThunderSounds.Find(ATSTRINGHASH("ROLL", 0xCB2448C6)), 
									sm_ThunderSounds.Find(ATSTRINGHASH("FALLBACK_ROLL", 0xF34CE6F3)),
									false,
									triggerTime + sm_PredelayTestForRollingThunder, 
									triggerTime + sm_PredelayTestForRollingThunder + maxTriggerTimeVariance, 
									STREAMED_SOUND_TYPE_ROLL,
									0.0f,
									kVoiceFilterLPFMaxCutoff BANK_ONLY(, 0.0f));


		sm_LastThunderStrikeTriggeredTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		AddRumblePadEvent(1500 + audEngineUtil::GetRandomNumberInRange(-500,500)
			,127 + audEngineUtil::GetRandomNumberInRange(0,128)
			,500 + audEngineUtil::GetRandomNumberInRange(-250,100)
			,50 + audEngineUtil::GetRandomNumberInRange(0,50)
			,triggerTime);
	}
	else
	{
		// We're not playing the thunder because it has been played recently, but add a spike anyway
		AddRumbleSpikeEvent(delay, intensity BANK_ONLY(,0.f));
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::TriggerThunderStrikeBurst(const Vector3 &position)
{
	// Get the distance from the listener to the sound source.
	f32 distanceToTravel = fabs((position - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag()) * sm_ThunderStrikeDistanceCorrection;
	u32 delay = (u32)(distanceToTravel/343.2f) * 1000;

	// lightning strikes always spike the maximum intensity
	f32 intensity = sm_DistanceToRumbleIntensityScale.CalculateValue(ClampRange(distanceToTravel,sm_ThunderStrikeMinDistance,sm_ThunderStrikeMaxDistance));
	
	if(sm_LastThunderStrikeTriggeredTime + sm_TimeToRetriggerThunderStrike <  g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
	{
		u32 triggerTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + delay;

		// This value represents the max amount of time we're willing to wait for assets to stream in - if we pass this then we'll fallback to the 
		// resident sound instead
		const u32 maxTriggerTimeVariance = 0;

		RequestStreamedWeatherOneShot(sm_ThunderSounds.Find(ATSTRINGHASH("STRIKE", 0x7E20602)), 
									sm_ThunderSounds.Find(ATSTRINGHASH("FALLBACK_STRIKE", 0x74D67DED)),
									true,
									triggerTime, 
									triggerTime + maxTriggerTimeVariance, 
									STREAMED_SOUND_TYPE_STRIKE, 
									intensity,
									kVoiceFilterLPFMaxCutoff BANK_ONLY(,distanceToTravel));

		RequestStreamedWeatherOneShot(sm_ThunderSounds.Find(ATSTRINGHASH("ROLL", 0xCB2448C6)), 
									sm_ThunderSounds.Find(ATSTRINGHASH("FALLBACK_ROLL", 0xF34CE6F3)),
									false,
									triggerTime + sm_PredelayTestForRollingThunder, 
									triggerTime + sm_PredelayTestForRollingThunder + maxTriggerTimeVariance, 
									STREAMED_SOUND_TYPE_ROLL,
									0.0f,
									kVoiceFilterLPFMaxCutoff BANK_ONLY(, 0.0f));

		sm_LastThunderStrikeTriggeredTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		AddRumblePadEvent(2000 + audEngineUtil::GetRandomNumberInRange(-500,500)
			,127 + audEngineUtil::GetRandomNumberInRange(-100,128)
			,1000 + audEngineUtil::GetRandomNumberInRange(-500,500)
			,155 + audEngineUtil::GetRandomNumberInRange(-100,100)
			,triggerTime);
	}
	else
	{
		// We're not playing the thunder because it has been played recently, but add a spike anyway
		AddRumbleSpikeEvent(delay, intensity BANK_ONLY(,distanceToTravel));
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::TriggerThunderStrike(const Vector3 &position)
{
	// Get the distance from the listener to the sound source.
	f32 distanceToTravel = fabs((position - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag()) * sm_ThunderStrikeDistanceCorrection;
	u32 delay = (u32)(distanceToTravel/343.2f) * 1000;

	// lightning strikes always spike the maximum intensity
	f32 intensity = sm_DistanceToRumbleIntensityScale.CalculateValue(ClampRange(distanceToTravel,sm_ThunderStrikeMinDistance,sm_ThunderStrikeMaxDistance));

	if(sm_LastThunderStrikeTriggeredTime + sm_TimeToRetriggerThunderStrike <  g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
	{
		u32 triggerTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + delay;

		// This value represents the max amount of time we're willing to wait for assets to stream in - if we pass this then we'll fallback to the 
		// resident sound instead
		const u32 maxTriggerTimeVariance = 0;

		RequestStreamedWeatherOneShot(sm_ThunderSounds.Find(ATSTRINGHASH("STRIKE", 0x7E20602)), 
									  sm_ThunderSounds.Find(ATSTRINGHASH("FALLBACK_STRIKE", 0x74D67DED)),
									  true,
									  triggerTime, 
									  triggerTime + maxTriggerTimeVariance, 
								      STREAMED_SOUND_TYPE_STRIKE, 
									  intensity,
									  kVoiceFilterLPFMaxCutoff BANK_ONLY(, 0.0f));
		RequestStreamedWeatherOneShot(sm_ThunderSounds.Find(ATSTRINGHASH("ROLL", 0xCB2448C6)), 
										sm_ThunderSounds.Find(ATSTRINGHASH("FALLBACK_ROLL", 0xF34CE6F3)),
										false,
										triggerTime + sm_PredelayTestForRollingThunder, 
										triggerTime + sm_PredelayTestForRollingThunder + maxTriggerTimeVariance, 
										STREAMED_SOUND_TYPE_ROLL,
										0.0f,
										kVoiceFilterLPFMaxCutoff BANK_ONLY(, 0.0f));


		sm_LastThunderStrikeTriggeredTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		AddRumblePadEvent(1500 + audEngineUtil::GetRandomNumberInRange(-500,500)
			,127 + audEngineUtil::GetRandomNumberInRange(0,128)
			,500 + audEngineUtil::GetRandomNumberInRange(-250,100)
			,50 + audEngineUtil::GetRandomNumberInRange(0,50)
			,triggerTime);
	}
	else
	{
		// We're not playing the thunder because it has been played recently, but add a spike anyway
		AddRumbleSpikeEvent(delay, intensity BANK_ONLY(,distanceToTravel));
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::AddRumbleSpikeEvent(u32 delay,f32 intensity BANK_ONLY(, f32 distanceToTravel))
{
	audRumbleSpikeEvent event; 
	event.timeToSpike = delay + g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	event.intensity = intensity;

	//BANK_ONLY(bool added = false); 
	for (u32 i = 0; i < MAX_RUMBLE_SPIKE_EVENTS; i++)
	{
		if( m_RumbleSpikeEvents[i].intensity == 0.f)
		{
			m_RumbleSpikeEvents[i] = event;
			
#if __BANK
			//added = true; 
			if(sm_ShowThunderInfo)
			{
				naDisplayf("Rumble spike [Distance %f] [Delay %u] [Burst Intensity %f] [Intensity %f]",distanceToTravel,delay,intensity,m_RumbleSpikeEvents[i].intensity );
			}
#endif
			break;
		}
	}
	//naAssertf(added, "Run out of rumble spike events, please increase the max number, currently %u",MAX_RUMBLE_SPIKE_EVENTS);
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::AddRumblePadEvent(u32 duration0,s32 freq0,u32 duration1,s32 freq1,s32 timeToTrigger)
{
	audRumblePadEvent event; 
	event.duration0 = duration0;
	event.freq0 = freq0;
	event.duration1 = duration1;
	event.freq1 = freq1;
	event.timeToTrigger = timeToTrigger;

	for (u32 i = 0; i < MAX_RUMBLE_PAD_EVENTS; i++)
	{
		if( m_RumblePadEvents[i].timeToTrigger == -1)
		{
			m_RumblePadEvents[i] = event;
			break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::UpdateThunder(const f32* windSpeakerVolumes)
{
	//intensity decay over time. 
	m_ThunderRumbleIntensity -= sm_RumbleIntensityDecay * audNorthAudioEngine::GetTimeStep();
	m_ThunderRumbleIntensity = Max(0.f,m_ThunderRumbleIntensity);

	// go through all the spike events and see if we have to spike the intensity.
	for (u32 i = 0; i < MAX_RUMBLE_SPIKE_EVENTS; i++)
	{
		if( m_RumbleSpikeEvents[i].intensity != 0.f && m_RumbleSpikeEvents[i].timeToSpike <= g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
		{
			m_ThunderRumbleIntensity += m_RumbleSpikeEvents[i].intensity;
			m_ThunderRumbleIntensity = Min(1.f,m_ThunderRumbleIntensity);
			m_RumbleSpikeEvents[i].intensity = 0.f;
		}
	}
	for (u32 i = 0; i < MAX_RUMBLE_PAD_EVENTS; i++)
	{
		if( m_RumblePadEvents[i].timeToTrigger && m_RumblePadEvents[i].timeToTrigger <= g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
		{
			if(!CPortal::IsInteriorScene())
			{
				CControlMgr::GetPlayerMappingControl().StartPlayerPadShake(m_RumblePadEvents[i].duration0,m_RumblePadEvents[i].freq0/*,m_RumblePadEvents[i].duration1*/,m_RumblePadEvents[i].freq1,0,-1);
			}

			m_RumblePadEvents[i].timeToTrigger = -1;
		}
	}


#if __BANK
	if (sm_OverridenRumbleIntensity >= 0.f)
	{
		m_ThunderRumbleIntensity = sm_OverridenRumbleIntensity;
	}
#endif

	ALIGNAS(16) f32 thunderSpeakerVolumes[4];

	for (u32 i = 0; i < 4; i++)
	{
		thunderSpeakerVolumes[i] = windSpeakerVolumes[i] * m_ThunderRumbleIntensity;
	}
	
	if(m_ThunderRumbleIntensity > g_SilenceVolumeLin)
	{
		if(!m_RumbleLoop)
		{
			audSoundInitParams initParams;
			CreateAndPlaySound_Persistent(sm_ThunderSounds.Find(ATSTRINGHASH("RUMBLE", 0x34D7C0E3)),&m_RumbleLoop,&initParams);
		}

		if(m_RumbleLoop)
		{
			m_RumbleLoop->SetRequestedQuadSpeakerLevels(thunderSpeakerVolumes);
		}
	}
	else if(m_RumbleLoop)
	{
		m_RumbleLoop->StopAndForget(true);
	}
	if(m_StreamedWeatherSoundSlots[STREAMED_SOUND_TYPE_ROLL].sound)
	{
		m_StreamedWeatherSoundSlots[STREAMED_SOUND_TYPE_ROLL].sound->SetRequestedQuadSpeakerLevels(thunderSpeakerVolumes);
	}

#if __BANK 
	if(sm_ShowThunderInfo)
	{
		char txt [128];
		formatf(txt,"Rumble intensity %f",m_ThunderRumbleIntensity);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
	}
#endif
}


//----------------------------------------------------------------------------------------------------------------------
WeatherAudioSettings* audWeatherAudioEntity::GetWeatherAudioSettings(u32 weatherTypeHashName)
{
	if(!g_AudioEngine.IsAudioEnabled())
	{
		return NULL;
	}

	WeatherAudioSettings *settings = NULL;
	// hacking the new XMAS weather type.
	if( weatherTypeHashName == 0xAAC9C895)// ATSTRINGHASH("XMAS", 0xAAC9C895))
	{
		settings =  audNorthAudioEngine::GetObject<WeatherAudioSettings>(ATSTRINGHASH("WEATHER_TYPES_SNOW", 0xE4F982DB));
		return settings;
	}
	WeatherTypeAudioReference *ref = audNorthAudioEngine::GetObject<WeatherTypeAudioReference>(ATSTRINGHASH("WeatherTypeList", 0x0106604c4));

	if(Verifyf(ref, "Couldn't find weather type references"))
	{
		for (s32 i = 0; i < ref->numReferences; ++i)
		{
			if( weatherTypeHashName == ref->Reference[i].WeatherName)
			{
				settings =  audNorthAudioEngine::GetObject<WeatherAudioSettings>(ref->Reference[i].WeatherType);
				break;
			}
		}
		if(!settings)
		{
			//naDebugf1("Couldn't find audio settings for %d, defaulting to NEUTRAL type", weatherTypeHashName);

			settings = audNorthAudioEngine::GetObject<WeatherAudioSettings>(ATSTRINGHASH("WEATHER_TYPES_NEUTRAL", 0x07fde384c));
		}
	}
	return settings;
}
//----------------------------------------------------------------------------------------------------------------------
f32 audWeatherAudioEntity::GetRainVerticallAtt() const
{
	const f32 heightAboveWorldBlanket = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.z - CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.x, MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.y);
	return m_WorlHeightToRainVolume.CalculateValue(heightAboveWorldBlanket);
}
//----------------------------------------------------------------------------------------------------------------------
#if __BANK
void audWeatherAudioEntity::AddWeatherTypeWidgets(bool prev)
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bkWidget* weatherTypeWidget =  BANKMGR.FindWidget("Audio/Weather/Wind/Weather type interpolation" ) ;
	if(weatherTypeWidget)
	{
		WeatherTypeAudioReference *weatherList = audNorthAudioEngine::GetObject<WeatherTypeAudioReference>(ATSTRINGHASH("WeatherTypeList",0x106604C4));
		if(naVerifyf(weatherList, "Couldn't find autogenerated weather type list"))
		{
			rage::bkCombo* pWeatherTypeCombo = bank->AddCombo((prev ? "Prev weather type": "Next weather type"), (prev ? &sm_OverridenPrevWeatherType : &sm_OverridenNextWeatherType), weatherList->numReferences, NULL);
			if (pWeatherTypeCombo != NULL)
			{
				for (int i = 0; i < weatherList->numReferences; i++)
				{
					pWeatherTypeCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(weatherList->Reference[i].WeatherType));
				}
			}
		}
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::ToggleEnableWeatherInterpTool()
{
	if(!sm_EnableWindTestTool)
	{
		sm_HasToStopDebugSounds = true;
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audWeatherAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Weather", false);
	bank.PushGroup("Rain");
		bank.AddToggle("DrawRainVolumes", &sm_ShowRainVolumes);
		bank.AddSlider("sm_NumRainSoundPerChannel", &sm_NumRainSoundPerChannel, 0, 3, 1);
	bank.PopGroup();
		bank.PushGroup("Thunder",false);
			bank.PushGroup("Common",false);
				bank.AddSlider("RumbleIntensityDecay", &sm_RumbleIntensityDecay, 0.0f, 1.0f, 0.001f);
				bank.AddSlider("Force Rumble Intensity", &sm_OverridenRumbleIntensity, -1.f, 1.f, 0.01f);
				bank.AddToggle("Show Thunder Info", &sm_ShowThunderInfo);
			bank.PopGroup();
			bank.PushGroup("CloudBurst",false);
				bank.AddSlider("sm_ThunderMinDistance", &sm_CloudBurstMinDistance, 0.0f, 10000.0f, 0.001f);
				bank.AddSlider("sm_ThunderMaxDistance", &sm_CloudBurstMaxDistance, 0.0f, 10000.0f, 0.001f);
				bank.AddSlider("sm_CloudBurstDistanceCorrection", &sm_CloudBurstDistanceCorrection, 0.0f, 1.0f, 0.001f);
				bank.PopGroup();
			bank.PushGroup("Strike",false);
				bank.AddSlider("sm_ThunderMinDistance", &sm_ThunderStrikeMinDistance, 0.0f, 10000.0f, 0.001f);
				bank.AddSlider("sm_ThunderMaxDistance", &sm_ThunderStrikeMaxDistance, 0.0f, 10000.0f, 0.001f);
				bank.AddSlider("sm_ThunderStrikeDistanceCorrection", &sm_ThunderStrikeDistanceCorrection, 0.0f, 1.0f, 0.001f);
				bank.AddSlider("sm_TimeToRetriggerThunderStrike", &sm_TimeToRetriggerThunderStrike, 0, 5000, 100);
			bank.PopGroup();
			bank.PushGroup("Strike + burst",false);
				bank.AddSlider("sm_PredelayTestForRollingThunder", &sm_PredelayTestForRollingThunder, 0, 5000, 100);
			bank.PopGroup();
			bank.PushGroup("burst",false);
				bank.AddSlider("sm_MaxBurstDelay", &sm_MaxBurstDelay, 0, 5000, 100);
			bank.PopGroup();
		bank.PopGroup();
		bank.PushGroup("Wind", false);
			bank.AddToggle("Show Wind Speaker Volumes", &sm_DrawWindSpeakerVolumes);
			bank.AddSlider("Force Wind Speaker Volume FL", &sm_OverridenWindSpeakerVolumes[0], -1.f, 1.f, 0.01f);
			bank.AddSlider("Force Wind Speaker Volume FR", &sm_OverridenWindSpeakerVolumes[1], -1.f, 1.f, 0.01f);
			bank.AddSlider("Force Wind Speaker Volume RR", &sm_OverridenWindSpeakerVolumes[2], -1.f, 1.f, 0.01f);
			bank.AddSlider("Force Wind Speaker Volume RL", &sm_OverridenWindSpeakerVolumes[3], -1.f, 1.f, 0.01f);
			
		if(g_AudioEngine.GetRemoteControl().IsPresent())
		{
			bank.AddToggle("g_ShowWindSoundsInfo", &sm_ShowWindSoundsInfo);
			bank.PushGroup("Weather type interpolation",false);
				bank.AddToggle("Enable weather type interpolation tool",&sm_EnableWindTestTool,datCallback(ToggleEnableWeatherInterpTool), "StopWindDebugSounds");
				bank.AddSlider("g_WeatherInterp", &sm_WeatherInterp, 0.0f, 1.0f, 0.01f);
				audWeatherAudioEntity::AddWeatherTypeWidgets(true);
				audWeatherAudioEntity::AddWeatherTypeWidgets(false);
			bank.PopGroup();
			bank.PushGroup("Wind gusts", false);
				bank.AddToggle("Debug gust", &sm_DebugGust);
				bank.AddToggle("Manual gust", &sm_ManualGust);
				bank.AddToggle("Trigger", &sm_TriggerGust);
				bank.AddSlider("sm_MinTimeToTriggerGust", &sm_MinTimeToTriggerGust, 0, 100000, 100);
				bank.AddSlider("sm_MaxTimeToTriggerGust", &sm_MaxTimeToTriggerGust, 0, 100000, 100);
				bank.AddToggle("Override gust factors", &sm_OverrideGustFactors);
				bank.AddSlider("Gust speed", &sm_GustSpeed, 0.0f, 100.0f, 0.1f);
			bank.PopGroup();
		}
		bank.PopGroup();
	bank.PopGroup();
}
#endif


