#include "granularenginecomponent.h"
#include "audioengine/engine.h"
#include "audioengine/category.h"
#include "audiosoundtypes/sound.h"
#include "audiosoundtypes/granularsound.h"
#include "audiosoundtypes/modularsynthsound.h"
#include "audiosynth/synthcore.h"
#include "audiohardware/pcmsource.h"
#include "vehicles/vehicle.h"
#include "audio/caraudioentity.h"

AUDIO_VEHICLES_OPTIMISATIONS()

extern audCategory* g_PlayerVolumeCategories[5];
extern audCategory *g_EngineCategory;
extern audCategory *g_EngineLoudCategory;
extern f32 g_GranularAccelVolumeTrim;
extern f32 g_GranularDecelVolumeTrim;
extern f32 g_GranularIdleVolumeTrim;
extern f32 g_SelectedGrainPosition;
extern u32 g_audVehicleLoopReleaseTime;
extern bool g_GranularLimiterEnabled;
extern bool g_GranularWobblesEnabled;
extern bool g_GranularDebugRenderingEnabled;
extern bool g_GranularIdleDebugRenderingEnabled;
extern bool g_GranularLODEqualPowerCrossfade;
extern f32 g_GranularXValueSmoothRate;
extern f32 g_GranularEngineRevMultiplier;
extern u32 g_EngineSwitchFadeInTime;

f32 g_GranularAccelDecelSmoothRate = 0.025f;
f32 g_IdleActivationRevRatio = 0.125f;
f32 g_GranularQualityCrossfadeSpeed = 2.0f;

f32 g_IdleMainSmootherRateIncrease = 0.05f;
f32 g_IdleMainSmootherRateDecrease = 0.1f;

#if __BANK
extern CEntity * g_pFocusEntity;
extern bool g_VehicleAudioAllowEntityFocus;
extern s32 g_GrainPlaybackStyle;
extern s32 g_GranularRandomisationStyle;
extern s32 g_GranularIdleRandomisationStyle;
extern s32 g_GranularMixStyle;
extern f32 g_GranularSlidingWindowSizeHzFraction;
extern u32 g_GranularSlidingWindowMinGrains;
extern u32 g_GranularSlidingWindowMaxGrains;
extern u32 g_GranularMinRepeatRate;
extern bool g_GranularLoopEditorShiftLeftPressed;
extern bool g_GranularLoopEditorShiftRightPressed;
extern bool g_GranularLoopEditorShrinkLoopPressed;
extern bool g_GranularLoopEditorGrowLoopPressed;
extern bool g_GranularLoopEditorCreateLoopPressed;
extern bool g_GranularLoopEditorDeleteLoopPressed;
extern bool g_GranularLoopEditorExportDataPressed;
extern bool g_GranularEditorControlRevsThrottle;
extern bool g_GranularForceRevLimiter;
extern bool g_GranularSkipGrainTogglePressed;
extern bool g_GranularSkipEveryOtherGrain;
extern bool g_GranularToggleTestTonePressed;
extern bool g_GranularToneGeneratorEnabled;
extern bool g_MuteGranularDamage;
extern bool g_SetGranularEngineIdleNative;
extern bool g_SetGranularExhaustIdleNative;
extern f32 g_GranularEditorRevs;
extern f32 g_GranularEditorThrottle;
extern s32 g_GranularInterGrainCrossfadeStyle;
extern s32 g_GranularLoopGrainCrossfadeStyle;
extern s32 g_GranularLoopLoopCrossfadeStyle;
extern s32 g_GranularMixCrossfadeStyle;
extern bool g_TestWobble;
extern bool g_ForceMuteGranularSynths;
extern u32 g_TestWobbleLength;
extern f32 g_TestWobbleLengthVariance;
extern f32 g_TestWobbleSpeed;
extern f32 g_TestWobbleSpeedVariance;
extern f32 g_TestWobblePitch;
extern f32 g_TestWobblePitchVariance;
extern f32 g_TestWobbleVolume;
extern f32 g_TestWobbleVolumeVariance;
#endif

// -------------------------------------------------------------------------------
// audGranularEngineComponent constructor
// -------------------------------------------------------------------------------
audGranularEngineComponent::audGranularEngineComponent() :
  m_Parent(NULL)
, m_GranularEngineAudioSettings(NULL)
, m_EngineSoundType(ENGINE_SOUND_TYPE_MAX)
, m_HasPrepared(false)
, m_HasPlayed(false)
, m_DisableLoopsOnDecels(false)
, m_FadeInRequired(0u)
, m_LastGearForWobble(0)
, m_LastWobbleTimeMs(0)
, m_EngineQuality(audGranularMix::GrainPlayerQualityMax)
, m_OverridenPlaybackOrder(audGranularSubmix::PlaybackOrderMax)
{
	for(u32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
	{
		m_GranularSound[loop] = NULL;
	}

	for(u32 loop = 0; loop < SYNTH_TYPE_NUM; loop++)
	{
		m_SynthSounds[loop] = NULL;
		m_AudioFramesSinceSynthStart[loop] = 0;
	}
	
	m_QualityCrossfade = -1.0f;
	m_ReverseWobbleActive = false;
	m_IdleMainSmoother.Init(g_IdleMainSmootherRateIncrease, g_IdleMainSmootherRateDecrease, true);
	m_RevsOffEffectSmoother.Init(0.08f, true);
	m_GasPedalLastFrame = 0.0f;
	m_LastGearForWobble = 0;
	m_LastWobbleTimeMs = 0;
	m_PrevAudioFrame = 0;
	m_VolumeScale = 1.0f;
	m_DamageSoundHash = g_NullSoundHash;
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent Init
// -------------------------------------------------------------------------------
void audGranularEngineComponent::Init(audVehicleAudioEntity* parent, EngineSoundType soundType, audGranularMix::audGrainPlayerQuality quality)
{
	naAssertf(parent, "Initialising vehicle transmission without a valid vehicle");
	m_Parent = parent;
	m_EngineSoundType = soundType;
	m_QualitySettingChanged = m_EngineQuality != audGranularMix::GrainPlayerQualityMax && m_GranularSound[m_EngineQuality] && m_EngineQuality != quality;

	if(m_QualitySettingChanged)
	{
		m_QualityCrossfade = m_EngineQuality == audGranularMix::GrainPlayerQualityHigh? 0.0f : 1.0f;
	}
	else
	{
		m_QualityCrossfade = quality == audGranularMix::GrainPlayerQualityHigh? 0.0f : 1.0f;
	}

	m_EngineQuality = quality;

	if( m_Parent )
	{
		m_GranularEngineAudioSettings = m_Parent->GetGranularEngineAudioSettings();
	
		if(m_DamageSoundHash == g_NullSoundHash)
		{
			// Pick a random damage synth sound from the available list
			SoundHashList* damageHashList = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObject<SoundHashList>(m_GranularEngineAudioSettings->DamageSynthHashList);

			if(damageHashList)
			{
				m_DamageSoundHash = damageHashList->SoundHashes[audEngineUtil::GetRandomNumberInRange(0, damageHashList->numSoundHashes - 1)].SoundHash;
			}
		}

		// GTA V fix - no metadata space to define this in RAVE, so having to hard code it
		const u32 modelNameHash = m_Parent->GetVehicleModelNameHash();

		if(modelNameHash == ATSTRINGHASH("BRAWLER", 0xA7CE1BC5) ||
			modelNameHash == ATSTRINGHASH("BALLER7", 0x1573422D) ||	
			modelNameHash == ATSTRINGHASH("BANSHEE2", 0x25C5AF13) ||
			modelNameHash == ATSTRINGHASH("BLAZER4", 0xE5BA6858) ||
			modelNameHash == ATSTRINGHASH("COMET4", 0x5D1903F9) ||
			modelNameHash == ATSTRINGHASH("CINQUEMILA", 0xA4F52C13) ||
			modelNameHash == ATSTRINGHASH("COQUETTE3", 0x2EC385FE) ||
			modelNameHash == ATSTRINGHASH("DRAFTER", 0x28EAB80F) ||
			modelNameHash == ATSTRINGHASH("EUROS", 0x7980BDD5) ||
			modelNameHash == ATSTRINGHASH("FELTZER3", 0xA29D6D10) ||
			modelNameHash == ATSTRINGHASH("FACTION", 0x81A9CDDF) ||
			modelNameHash == ATSTRINGHASH("FACTION2", 0x95466BDB) ||
			modelNameHash == ATSTRINGHASH("FACTION3", 0x866BCE26) ||
			modelNameHash == ATSTRINGHASH("FAGGIO3", 0xB328B188) ||
			modelNameHash == ATSTRINGHASH("FUTO2", 0xA6297CC8) ||
			modelNameHash == ATSTRINGHASH("GAUNTLET4", 0x734C5E50) ||
			modelNameHash == ATSTRINGHASH("HELLION", 0xEA6A047F) ||
			modelNameHash == ATSTRINGHASH("HUSTLER", 0x23CA25F2) ||
			modelNameHash == ATSTRINGHASH("LYNX", 0x1CBDC10B) ||
			modelNameHash == ATSTRINGHASH("ITALIGTB", 0x85E8E76B) ||
			modelNameHash == ATSTRINGHASH("ISSI7", 0x6E8DA4F7) ||
			modelNameHash == ATSTRINGHASH("JESTER4", 0xA1B3A871) ||
			modelNameHash == ATSTRINGHASH("JUGULAR", 0xF38C4245) ||
			modelNameHash == ATSTRINGHASH("KOMODA", 0xCE44C4B9) ||
			modelNameHash == ATSTRINGHASH("SHEAVA", 0x30D3F6D8) ||
			modelNameHash == ATSTRINGHASH("NIGHTSHADE", 0x8C2BD0DC) ||
			modelNameHash == ATSTRINGHASH("NIGHTSHARK", 0x19DD9ED1) ||
			modelNameHash == ATSTRINGHASH("POLGAUNTLET", 0xB67633E6) ||
			modelNameHash == ATSTRINGHASH("REMUS", 0x5216AD5E) ||
			modelNameHash == ATSTRINGHASH("VERLIERER2", 0x41B77FA4) ||
			modelNameHash == ATSTRINGHASH("SHOTARO", 0xE7D2A16E) ||
			modelNameHash == ATSTRINGHASH("SCHAFTER2", 0xB52B5113) ||
			modelNameHash == ATSTRINGHASH("SCHAFTER3", 0xA774B5A6) ||
			modelNameHash == ATSTRINGHASH("SCHAFTER4", 0x58CF185C) ||
			modelNameHash == ATSTRINGHASH("SCHAFTER5", 0xCB0E7CD9) ||
			modelNameHash == ATSTRINGHASH("SCHAFTER6", 0x72934BE4) ||
			modelNameHash == ATSTRINGHASH("SCHLAGEN", 0xE1C03AB0) ||
			modelNameHash == ATSTRINGHASH("SM722", 0x2E3967B0) ||
			modelNameHash == ATSTRINGHASH("TOROS", 0xBA5334AC) ||
			modelNameHash == ATSTRINGHASH("VSTR", 0x56CDEE7D) ||
			modelNameHash == ATSTRINGHASH("XA21", 0x36B4A8A9) ||
			modelNameHash == ATSTRINGHASH("YOSEMITE", 0x6F946279) ||
			modelNameHash == ATSTRINGHASH("YOSEMITE2", 0x64F49967) ||
			modelNameHash == ATSTRINGHASH("YOSEMITE3", 0x409D787) ||
			modelNameHash == ATSTRINGHASH("ZR380", 0x20314B42) ||
			modelNameHash == ATSTRINGHASH("ZR3802", 0xBE11EFC6) ||
			modelNameHash == ATSTRINGHASH("ZR3803", 0xA7DCC35C))
		{
			m_DisableLoopsOnDecels = true;
		}
		
		if(modelNameHash == ATSTRINGHASH("FAGGIO3", 0xB328B188))
		{
			m_OverridenPlaybackOrder = audGranularSubmix::PlaybackOrderWalk;
		}
	}
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent Prepare
// -------------------------------------------------------------------------------
bool audGranularEngineComponent::Prepare()
{
	if(!m_HasPrepared)
	{
		if(m_GranularSound[m_EngineQuality])
		{
			audWaveSlot* waveSlot = m_Parent->GetWaveSlot();

			if(waveSlot && !waveSlot->GetIsLoading())
			{	
				if(m_GranularSound[m_EngineQuality]->Prepare(waveSlot) == AUD_PREPARED)
				{
					m_HasPrepared = true;
				}
			}
		}
	}

	return m_HasPrepared;
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent Play
// -------------------------------------------------------------------------------
void audGranularEngineComponent::Play()
{
	if(m_GranularSound[m_EngineQuality]->GetPlayState() != AUD_SOUND_PLAYING)
	{
		m_GranularSound[m_EngineQuality]->SetClientVariable(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? (u32)AUD_VEHICLE_SOUND_ENGINE : (u32)AUD_VEHICLE_SOUND_EXHAUST);
		m_GranularSound[m_EngineQuality]->SetReleaseTime(g_audVehicleLoopReleaseTime);
		m_GranularSound[m_EngineQuality]->Play();

		// GTA V fix - no metadata space to define this in RAVE, so having to hard code it
		const u32 modelNameHash = m_Parent->GetVehicleModelNameHash();

		if(modelNameHash == ATSTRINGHASH("SCHAFTER3", 0xA774B5A6) || 
		   modelNameHash == ATSTRINGHASH("SCHAFTER4", 0x58CF185C) ||
		   modelNameHash == ATSTRINGHASH("SCHAFTER5", 0xCB0E7CD9) ||
		   modelNameHash == ATSTRINGHASH("SCHAFTER6", 0x72934BE4))
		{
			((audGranularSound*)m_GranularSound[m_EngineQuality])->SetGranularChangeRateForLoops(0.5f);
		}
	}

	m_HasPlayed = true;
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent Stop
// -------------------------------------------------------------------------------
void audGranularEngineComponent::Stop(s32 releaseTime)
{
	for(u32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
	{
		if(m_GranularSound[loop])
		{
			// Turn off the rev limiter when stopping
			if(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE)
			{
				if(m_GranularSound[loop]->GetPlayState() == AUD_SOUND_PLAYING)
				{
					((audGranularSound*)m_GranularSound[loop])->SetGrainSkippingEnabled(0, false);
					((audGranularSound*)m_GranularSound[loop])->SetGrainSkippingEnabled(1, false);
				}
			}

			m_GranularSound[loop]->SetReleaseTime(releaseTime);
			m_Parent->StopAndForgetSounds(m_GranularSound[loop]);
		}

#if __BANK
		if(m_Parent && ((!g_VehicleAudioAllowEntityFocus && m_Parent->IsPlayerVehicle()) || (g_VehicleAudioAllowEntityFocus && m_Parent->GetVehicle() == g_pFocusEntity)))
		{
			g_GranularDebugRenderingEnabled = false;
			audGrainPlayer::s_DebugDrawGrainPlayer = NULL;
		}
#endif
	}

	for(u32 loop = 0; loop < SYNTH_TYPE_NUM; loop++)
	{
		if(m_SynthSounds[loop])
		{
			m_SynthSounds[loop]->SetReleaseTime(releaseTime);
			m_Parent->StopAndForgetSounds(m_SynthSounds[loop]);
		}
	}
	
	m_FadeInRequired = 0u;
	m_HasPlayed = false;
	m_HasPrepared = false;
	m_QualitySettingChanged = false;
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent QualitySettingChanged
// -------------------------------------------------------------------------------
void audGranularEngineComponent::QualitySettingChanged()
{
	m_HasPlayed = false;
	m_HasPrepared = false;
}

// -------------------------------------------------------------------------------
// Check if this component is actively playing any sounds
// -------------------------------------------------------------------------------
bool audGranularEngineComponent::AreAnySoundsPlaying() const
{
	for(u32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
	{
		if(m_GranularSound[loop] && m_GranularSound[loop]->GetPlayState() == AUD_SOUND_PLAYING)
		{
			return true;
		}
	}
	
	return false;
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent Start
// -------------------------------------------------------------------------------
void audGranularEngineComponent::Start()
{
	naAssertf(m_GranularEngineAudioSettings, "Granular audio settings must be valid, about to access a null ptr...");
	audAssert(m_Parent->GetEngineEffectRoute() >= 0 && m_Parent->GetExhaustEffectRoute() >= 0);

	if(!m_Parent->GetDynamicWaveSlot() || !m_Parent->GetDynamicWaveSlot()->IsLoaded())
	{
		return;
	}

	audSoundInitParams initParams;

	if(!m_QualitySettingChanged)
	{
		initParams.AttackTime = m_FadeInRequired;
	}
	else
	{
		initParams.AttackTime = 0u;
	}
	
	initParams.UpdateEntity = true;
	initParams.u32ClientVar = AUD_VEHICLE_SOUND_UNKNOWN;
	initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();

	if( m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE )
	{
		Assign(initParams.EffectRoute, m_Parent->GetEngineEffectRoute());
		
		if(!m_GranularSound[m_EngineQuality])
		{
			if(m_EngineQuality == audGranularMix::GrainPlayerQualityHigh)
			{
				m_Parent->CreateSound_PersistentReference(m_GranularEngineAudioSettings->EngineAccel, &m_GranularSound[m_EngineQuality], &initParams);
			}
			else
			{
				m_Parent->CreateSound_PersistentReference(m_GranularEngineAudioSettings->NPCEngineAccel, &m_GranularSound[m_EngineQuality], &initParams);
			}

			if(m_GranularSound[m_EngineQuality])
			{
				ConfigureInitialGrainPlayerState();
			}

			m_HasPrepared = false;
		}
	}
	else
	{
		Assign(initParams.EffectRoute, m_Parent->GetExhaustEffectRoute());

		audGranularSound* granularSound = (audGranularSound*)m_Parent->GetVehicleEngine()->GetGranularEngine()->GetGranularSound(m_EngineQuality);

		if(granularSound && granularSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			initParams.ShadowPcmSourceId = granularSound->GetGrainPlayerID();

			if(!m_GranularSound[m_EngineQuality])
			{
				if(m_EngineQuality == audGranularMix::GrainPlayerQualityHigh)
				{
					m_Parent->CreateSound_PersistentReference(m_GranularEngineAudioSettings->ExhaustAccel, &m_GranularSound[m_EngineQuality], &initParams);
				}
				else
				{
					m_Parent->CreateSound_PersistentReference(m_GranularEngineAudioSettings->NPCExhaustAccel, &m_GranularSound[m_EngineQuality], &initParams);
				}
			}
		}
		else if(m_HasPlayed)
		{
			Stop(0);
			return;
		}
	}
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent ConfigureInitialGrainPlayerState
// -------------------------------------------------------------------------------
void audGranularEngineComponent::ConfigureInitialGrainPlayerState()
{
	audGranularSound* granularSound = (audGranularSound*)m_GranularSound[m_EngineQuality];
	f32 startingRevs = m_Parent->GetVehicle()->m_Transmission.GetRevRatio();
	f32 revRatio = Clamp((startingRevs - 0.2f) * 1.25f, 0.0f, 1.0f);
	f32 throttleRatio = Abs(Min(m_Parent->GetVehicle()->GetThrottle(), m_Parent->GetVehicle()->m_Transmission.GetThrottle()));
	f32 accelVolScale = throttleRatio > 0.1f? 1.0f : 0.0f;
	f32 decelVolScale = 1.0f - accelVolScale;
	f32 idleVolScale = (revRatio <= g_IdleActivationRevRatio && throttleRatio <= 0.1f) ? 1.0f : 0.0f;

	// If we've changed quality then start off muted in preparation for the crossfade
	if(m_QualitySettingChanged)
	{
		accelVolScale = 0.0f;
		decelVolScale = 0.0f;
		idleVolScale = 0.0f; 
	}

	granularSound->SetInitialParameters(revRatio);
	granularSound->SetChannelVolume(0, m_VolumeScale * accelVolScale * (1.0f - idleVolScale), 1.0f);
	granularSound->SetChannelVolume(1, m_VolumeScale * accelVolScale * (1.0f - idleVolScale), 1.0f);
	granularSound->SetChannelVolume(2, m_VolumeScale * decelVolScale * (1.0f - idleVolScale), 1.0f);
	granularSound->SetChannelVolume(3, m_VolumeScale * decelVolScale * (1.0f - idleVolScale), 1.0f);
	granularSound->SetChannelVolume(4, m_VolumeScale * idleVolScale, 1.0f);
	granularSound->SetChannelVolume(5, m_VolumeScale * idleVolScale, 1.0f);	
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent CalculateDialRPMRatio
// -------------------------------------------------------------------------------
f32 audGranularEngineComponent::CalculateDialRPMRatio() const
{
	// Get the engine sound if we're querying this on the exhaust for some reason
	audGranularSound* granularSound = (audGranularSound*)m_Parent->GetVehicleEngine()->GetGranularEngine()->GetGranularSound(m_EngineQuality);

	if(granularSound)
	{
		for(u32 loop = 0; loop < g_MaxPcmSourceVariableBlocks; loop++)
		{
			if(audPcmSource::GetVariableBlock(loop)->GetPcmSource() == granularSound->GetGrainPlayerID())
			{
				audPcmVariableBlock* variableBlock = audPcmSource::GetVariableBlock(loop);

				if(variableBlock)
				{
					f32* frequencyRatioPtr = variableBlock->FindVariableAddress(ATSTRINGHASH("granularfrequencyratio", 0x6AF66575));
					f32* grainSkippingPtr = variableBlock->FindVariableAddress(ATSTRINGHASH("grainskippingactive", 0xFB63030F));
					
					if(frequencyRatioPtr)
					{
						// Granular playback frequency ratio is 0-1, but lets scale that so idle is 10% of total RPM
						f32 frequencyRatio = (*frequencyRatioPtr * 0.9f) + 0.1f;

						if(frequencyRatio >= 0.0f && frequencyRatio <= 1.0f)
						{
							if(grainSkippingPtr)
							{
								frequencyRatio -= (0.2f * (*grainSkippingPtr));
							}

							return frequencyRatio;
						}					
					}
				}								
			}
		}
	}
	
	return m_Parent->GetVehicleEngine()->GetEngineLastRevs();
}

// -------------------------------------------------------------------------------
// audGranularEngineComponent Update
// -------------------------------------------------------------------------------
void audGranularEngineComponent::Update(audVehicleVariables* state, bool updateChannelVolumes)
{
	f32 preSubmixVol = 0.0f;
	f32 preSubmixVolTrimRevsThrottle = 0.0f;

	f32 revRatio = ((state->revs - 0.2f) * 1.25f);
	f32 throttleRatio = state->throttle;

#if __BANK
	if(g_GranularEditorControlRevsThrottle)
	{
		revRatio = g_GranularEditorRevs;
		throttleRatio = g_GranularEditorThrottle;
	}

	m_IdleMainSmoother.SetRates(g_IdleMainSmootherRateIncrease, g_IdleMainSmootherRateDecrease);
#endif

	if( m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE )
	{
		preSubmixVol = m_GranularEngineAudioSettings->EngineVolume_PreSubmix/100.0f;
		preSubmixVolTrimRevsThrottle += revRatio * m_GranularEngineAudioSettings->EngineRevsVolume_PreSubmix/100.0f;
		preSubmixVolTrimRevsThrottle += throttleRatio * m_GranularEngineAudioSettings->EngineThrottleVolume_PreSubmix/100.0f;
	}
	else
	{
		preSubmixVol = m_GranularEngineAudioSettings->ExhaustVolume_PreSubmix/100.0f;
		preSubmixVolTrimRevsThrottle += revRatio * m_GranularEngineAudioSettings->ExhaustRevsVolume_PreSubmix/100.0f;
		preSubmixVolTrimRevsThrottle += throttleRatio * m_GranularEngineAudioSettings->ExhaustThrottleVolume_PreSubmix/100.0f;
	}

	f32 desiredIdleVolScale = (revRatio <= g_IdleActivationRevRatio && (throttleRatio <= 0.1f || revRatio == 0.0f)) ? 1.0f : 0.0f;
	f32 idleVolScale = m_IdleMainSmoother.CalculateValue(desiredIdleVolScale);

#if __BANK
	if(g_GranularEditorControlRevsThrottle)
	{
		idleVolScale = 0.0f;
	}
#endif

	f32 accelVolScale = throttleRatio > 0.1f? 1.0f : 0.0f;

#if __BANK
	if(g_GranularMixStyle == 1)
	{
		accelVolScale = 1;
	}
	else if(g_GranularMixStyle == 2)
	{
		accelVolScale = 0;
	}
#endif

	f32 decelVolScale = 1.0f - accelVolScale;
	f32 revLimiterVolumeTrim = 0.0f;
	bool accelMuted = accelVolScale == 0;
	bool decelMuted = accelVolScale == 1;

	for(s32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
	{
		audGranularSound* granularSound = ((audGranularSound*)m_Parent->GetVehicleEngine()->GetGranularEngine()->GetGranularSound((audGranularMix::audGrainPlayerQuality)loop));

		if(granularSound && granularSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			if(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE)
			{
				granularSound->AllocateVariableBlock();

				if(m_EngineQuality == audGranularMix::GrainPlayerQualityHigh)
				{
					UpdateWobbles(state, (audGranularMix::audGrainPlayerQuality)loop);
				}

				if(loop == m_EngineQuality)
				{
					UpdateSynthSounds(state, revRatio, granularSound);
				}

				f32 desiredGranularXValue = revRatio * g_GranularEngineRevMultiplier;

				if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR && (state->onRevLimiter BANK_ONLY(|| g_GranularForceRevLimiter)))
				{
					if(m_Parent->GetVehicle()->GetDriver() && !m_Parent->IsReversing())
					{
						if(AUD_GET_TRISTATE_VALUE(m_GranularEngineAudioSettings->Flags, FLAG_ID_GRANULARENGINEAUDIOSETTINGS_HASREVLIMITER) == AUD_TRISTATE_TRUE || ((audCarAudioEntity*)m_Parent)->IsEngineUpgraded())
						{
							if(g_GranularLimiterEnabled BANK_ONLY(|| g_GranularForceRevLimiter))
							{
								if(desiredGranularXValue >= 1.0f)
								{
									revLimiterVolumeTrim = -2.0f;
									granularSound->SetX(1.0f, fwTimer::GetSystemTimeStepInMilliseconds());
									granularSound->LockPitch(true);

									switch(m_GranularEngineAudioSettings->RevLimiterApplyType)
									{
									case REV_LIMITER_ENGINE_ONLY:
										granularSound->SetGrainSkippingEnabled(0, true, m_GranularEngineAudioSettings->RevLimiterGrainsToPlay, m_GranularEngineAudioSettings->RevLimiterGrainsToSkip, m_GranularEngineAudioSettings->RevLimiterVolumeCut);
										break;
									case REV_LIMITER_EXHAUST_ONLY:
										granularSound->SetGrainSkippingEnabled(1, true, m_GranularEngineAudioSettings->RevLimiterGrainsToPlay, m_GranularEngineAudioSettings->RevLimiterGrainsToSkip, m_GranularEngineAudioSettings->RevLimiterVolumeCut);
										break;									
									case REV_LIMITER_BOTH_CHANNELS:
										granularSound->SetGrainSkippingEnabled(0, true, m_GranularEngineAudioSettings->RevLimiterGrainsToPlay, m_GranularEngineAudioSettings->RevLimiterGrainsToSkip, m_GranularEngineAudioSettings->RevLimiterVolumeCut);
										granularSound->SetGrainSkippingEnabled(1, true, m_GranularEngineAudioSettings->RevLimiterGrainsToPlay, m_GranularEngineAudioSettings->RevLimiterGrainsToSkip, m_GranularEngineAudioSettings->RevLimiterVolumeCut);
										break;
									default:
										break;
									}									
								}
							}
						}
					}
				}
				else
				{
					granularSound->SetGrainSkippingEnabled(0, false);
					granularSound->SetGrainSkippingEnabled(1, false);
					granularSound->LockPitch(false);
				}

				granularSound->SetX(desiredGranularXValue, fwTimer::GetSystemTimeStepInMilliseconds());
				granularSound->SetXSmoothRate(g_GranularXValueSmoothRate);

#if __BANK
				if(g_GranularIdleRandomisationStyle != audGranularSubmix::PlaybackOrderMax)
				{
					granularSound->SetAsPureRandomWithStyle(4, (audGranularSubmix::audGrainPlaybackOrder)g_GranularIdleRandomisationStyle);
					granularSound->SetAsPureRandomWithStyle(5, (audGranularSubmix::audGrainPlaybackOrder)g_GranularIdleRandomisationStyle);
				}
				else
#endif
				{
					if(m_OverridenPlaybackOrder != audGranularSubmix::PlaybackOrderMax)
					{
						granularSound->SetAsPureRandomWithStyle(4, m_OverridenPlaybackOrder);
						granularSound->SetAsPureRandomWithStyle(5, m_OverridenPlaybackOrder);
					}
					else
					{
						granularSound->SetAsPureRandom(4, true);
						granularSound->SetAsPureRandom(5, true);
					}
				}
			}

#if __BANK
			if((audGranularMix::audGrainPlaybackStyle)g_GrainPlaybackStyle >= audGranularMix::PlaybackStyleMax)
#endif
			{				
				bool disableLoopsOnDecels = m_DisableLoopsOnDecels;

				if(!disableLoopsOnDecels)
				{
					const u32 modelNameHash = m_Parent->GetVehicleModelNameHash();

					if(modelNameHash == ATSTRINGHASH("SLAMVAN3", 0x42BC5E19))
					{
						if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR && ((audCarAudioEntity*)m_Parent)->IsExhaustUpgraded())
						{
							disableLoopsOnDecels = true;
						}
					}
				}				
				
				// Here we manually tweak the playback style so as to limit the peak number of decoders used. Our worst case is when we're blending 
				// between loops and grains on the accel, and then suddenly need to crossfade into the decel (or vice versa)
				if(m_EngineQuality == audGranularMix::GrainPlayerQualityLow)
				{
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 0 : 1, audGranularMix::PlaybackStyleLoopsOnly);
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 2 : 3, audGranularMix::PlaybackStyleLoopsOnly);
				}
				else if(!accelMuted && !decelMuted)
				{
					// Neither muted - we're transitioning so clamp the playback type to prevent either from using expensive loops & grains
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 0 : 1, audGranularMix::PlaybackStyleGrainsOnly);
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 2 : 3, audGranularMix::PlaybackStyleGrainsOnly);
				}
				else if(accelMuted)
				{
					// Accel fully muted, so set it to low quality and decel to full quality
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 0 : 1, audGranularMix::PlaybackStyleGrainsOnly);
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 2 : 3, disableLoopsOnDecels? audGranularMix::PlaybackStyleGrainsOnly : audGranularMix::PlaybackStyleLoopsAndGrains);
				}
				else if(decelMuted)
				{
					// Decel fully muted, so set it to low quality and accel to full quality
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 0 : 1, audGranularMix::PlaybackStyleLoopsAndGrains);
					granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 2 : 3, audGranularMix::PlaybackStyleGrainsOnly);
				}
			}
#if __BANK
			else
			{
				granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 0 : 1, (audGranularMix::audGrainPlaybackStyle)g_GrainPlaybackStyle);
				granularSound->SetGrainPlaybackStyle(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 2 : 3, (audGranularMix::audGrainPlaybackStyle)g_GrainPlaybackStyle);
			}
#endif
		}
	}

	if(updateChannelVolumes)
	{
		f32 accelVolume = g_GranularAccelVolumeTrim + (m_GranularEngineAudioSettings->AccelVolume_PreSubmix/100.0f);
		f32 decelVolume = g_GranularDecelVolumeTrim + (m_GranularEngineAudioSettings->DecelVolume_PreSubmix/100.0f);
		f32 idleVolume = g_GranularIdleVolumeTrim + (m_GranularEngineAudioSettings->IdleVolume_PreSubmix/100.0f);
		accelVolume += preSubmixVolTrimRevsThrottle + preSubmixVol + revLimiterVolumeTrim;
		decelVolume += decelVolume + preSubmixVolTrimRevsThrottle + preSubmixVol;

		for(s32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
		{
			audGranularSound* granularSound = ((audGranularSound*)m_Parent->GetVehicleEngine()->GetGranularEngine()->GetGranularSound((audGranularMix::audGrainPlayerQuality)loop));

			if(granularSound)
			{
				f32 crossfadeVolume;
				f32 perQualityAccelVolScale = accelVolScale;
				f32 perQualityDecelVolume = decelVolScale;

				if(loop == audGranularMix::GrainPlayerQualityLow)
				{
					crossfadeVolume = m_QualityCrossfade;
					perQualityAccelVolScale = 1.0f;
					perQualityDecelVolume = 0.0f;
				}
				else
				{
					crossfadeVolume = 1.0f - m_QualityCrossfade;
				}

				if(g_GranularLODEqualPowerCrossfade)
				{
					const f32 PI_over_2 = (PI / 2.0f);
					crossfadeVolume = Sinf(crossfadeVolume * PI_over_2);
				}

				f32 channelVolumeAccel = perQualityAccelVolScale * audDriverUtil::ComputeLinearVolumeFromDb(accelVolume) * (1.0f - idleVolScale) * crossfadeVolume;
				f32 channelVolumeDecel = perQualityDecelVolume * audDriverUtil::ComputeLinearVolumeFromDb(decelVolume) * (1.0f - idleVolScale) * crossfadeVolume;
				f32 channelVolumeIdle = idleVolScale * audDriverUtil::ComputeLinearVolumeFromDb(idleVolume) * crossfadeVolume;

				if(granularSound->GetPlayState() == AUD_SOUND_PLAYING)
				{
					granularSound->SetChannelVolume(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 0 : 1, m_VolumeScale * channelVolumeAccel, g_GranularAccelDecelSmoothRate);
					granularSound->SetChannelVolume(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 2 : 3, m_VolumeScale * channelVolumeDecel, g_GranularAccelDecelSmoothRate);
					granularSound->SetChannelVolume(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE? 4 : 5, m_VolumeScale * channelVolumeIdle, g_GranularAccelDecelSmoothRate);
				}
			}
		}
	}

	UpdateQualityCrossfade();

#if __BANK
	UpdateDebug();
#endif

	m_GasPedalLastFrame = state->gasPedal;
}

// -------------------------------------------------------------------------------
// UpdateSynthSounds
// -------------------------------------------------------------------------------
void audGranularEngineComponent::UpdateSynthSounds(audVehicleVariables* state, f32 revRatio, audGranularSound* primaryGranularSound)
{
	u32 audioFrameIndex = audRequestedSettings::GetReadIndex();

	for(u32 loop = 0; loop < SYNTH_TYPE_NUM; loop++)
	{
#if __BANK
		if(g_ForceMuteGranularSynths)
		{
			if(m_SynthSounds[loop])
			{
				m_SynthSounds[loop]->StopAndForget();
			}

			continue;
		}
#endif

		u32 synthSound = g_NullSoundHash;

		switch(loop)
		{
		case SYNTH_TYPE_GENERAL:
			synthSound = m_GranularEngineAudioSettings->SynchronisedSynth;
			break;
		case SYNTH_TYPE_DAMAGE:
			synthSound = m_DamageSoundHash;
			break;
		default:
			break;
		}

		f32 volume = 0.0f;

		if(loop == SYNTH_TYPE_DAMAGE)
		{
			if(state->engineDamageFactor > 0.5f BANK_ONLY(&& !g_MuteGranularDamage))
			{
				volume = audDriverUtil::ComputeDbVolumeFromLinear(Clamp((state->engineDamageFactor - 0.5f)/0.3f, 0.0f, 1.0f) * (1.0f - Clamp(revRatio/0.75f, 0.0f, 1.0f)));
			}
			else
			{
				volume = g_SilenceVolume;
			}
		}

		if(!m_SynthSounds[loop])
		{
			if(synthSound != g_NullSoundHash)
			{
				if(volume > g_SilenceVolume)
				{
					audSoundInitParams initParams;
					initParams.UpdateEntity = true;
					initParams.u32ClientVar = AUD_VEHICLE_SOUND_UNKNOWN;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
					initParams.Volume = volume;
					initParams.AttackTime = g_EngineSwitchFadeInTime;
					
					m_Parent->CreateAndPlaySound_Persistent(synthSound, &m_SynthSounds[loop], &initParams);
					m_AudioFramesSinceSynthStart[loop] = 0;
				}
			}
		}

		if(m_SynthSounds[loop])
		{
			if(volume <= g_SilenceVolume)
			{
				m_Parent->StopAndForgetSounds(m_SynthSounds[loop]);
			}
			else
			{
				// Wait a few frames to ensure that the synth sound PCM source is initialized, otherwise it'll assert when we start adjusting parameters
				if(m_AudioFramesSinceSynthStart[loop] < 3)
				{
					if(m_SynthSounds[loop]->GetPlayState() == AUD_SOUND_PLAYING && m_PrevAudioFrame != audioFrameIndex)
					{
						m_AudioFramesSinceSynthStart[loop]++;
					}

					m_SynthSounds[loop]->SetRequestedVolume(g_SilenceVolume);
				}
				else
				{
					s32 synthPcmSource = ((audModularSynthSound*)m_SynthSounds[loop])->GetPcmSourceId();

					if(synthPcmSource >= 0)
					{
						audPcmSourceInterface::SetParam(synthPcmSource, synthCorePcmSource::Params::ExternalPcmSourceVariableBlock, (u32)primaryGranularSound->GetGrainPlayerID());

						if(loop == SYNTH_TYPE_GENERAL && m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
						{
							// For future projects, this should be switched over to use the actual upgrade value, rather than just being a binary on/off
							audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("Mod_Upgrade_Level", 0x893FF500), ((audCarAudioEntity*)m_Parent)->IsExhaustUpgraded() ? 1.0f : 0.0f);
							audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("Mod_Eng_Upgrade_Level", 0xA5417B0C), ((audCarAudioEntity*)m_Parent)->GetEngineUpgradeLevel());
							audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("RevsOff", 0x2ACC025D), m_RevsOffEffectSmoother.CalculateValue(((audCarAudioEntity*)m_Parent)->GetVehicleEngine()->IsRevsOffActive() ? 1.0f : 0.0f));
						}

						audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("RPM", 0x5B924509), state->revs);
						audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("Throttle", 0xEA0151DC), state->throttle);

						if(m_Parent->IsPlayerDrivingVehicle())
						{
							audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("ThrottleInput", 0x918028C4), state->throttleInput);
						}
						else
						{
							// AI throttle control can behave weirdly, best to just use the smoothed input
							audPcmSourceInterface::SetParam(synthPcmSource, ATSTRINGHASH("ThrottleInput", 0x918028C4), state->throttle);
						}
						
						m_SynthSounds[loop]->SetRequestedVolume(volume);
					}
				}
			}
		}
	}

	m_PrevAudioFrame = audioFrameIndex;
}

// -------------------------------------------------------------------------------
// Update quality crossfade
// -------------------------------------------------------------------------------
void audGranularEngineComponent::UpdateQualityCrossfade()
{
	if(m_QualitySettingChanged)
	{
		const audGranularSound* granularEngineSound = (audGranularSound*)m_Parent->GetVehicleEngine()->GetGranularEngine()->GetGranularSound(m_EngineQuality);
		const audSound* granularExhaustSound = m_Parent->GetVehicleEngine()->GetGranularExhaust()->GetGranularSound(m_EngineQuality);

		// Only start the crossfade once both the engine and exhaust sounds for the new quality level are playing
		if(granularEngineSound && 
		   granularEngineSound->GetPlayState() == AUD_SOUND_PLAYING &&
		   granularExhaustSound &&
		   granularExhaustSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			if(m_EngineQuality == audGranularMix::GrainPlayerQualityHigh && m_QualityCrossfade > 0.0f)
			{
				m_QualityCrossfade -= g_GranularQualityCrossfadeSpeed * fwTimer::GetTimeStep();
			}
			else if(m_EngineQuality == audGranularMix::GrainPlayerQualityLow && m_QualityCrossfade < 1.0f)
			{
				m_QualityCrossfade += g_GranularQualityCrossfadeSpeed * fwTimer::GetTimeStep();
			}

			m_QualityCrossfade = Clamp(m_QualityCrossfade, 0.0f, 1.0f);

			if(m_QualityCrossfade <= 0.0f && m_GranularSound[audGranularMix::GrainPlayerQualityLow])
			{
				m_GranularSound[audGranularMix::GrainPlayerQualityLow]->SetReleaseTime(0);
				m_Parent->StopAndForgetSounds(m_GranularSound[audGranularMix::GrainPlayerQualityLow]);
				m_QualitySettingChanged = false;
			}
			else if(m_QualityCrossfade >= 1.0f && m_GranularSound[audGranularMix::GrainPlayerQualityHigh])
			{
				m_GranularSound[audGranularMix::GrainPlayerQualityHigh]->SetReleaseTime(0);
				m_Parent->StopAndForgetSounds(m_GranularSound[audGranularMix::GrainPlayerQualityHigh]);
				m_QualitySettingChanged = false;
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Update wobbles
// -------------------------------------------------------------------------------
void audGranularEngineComponent::UpdateWobbles(audVehicleVariables* state, audGranularMix::audGrainPlayerQuality quality)
{
	audGranularSound* granularSound = (audGranularSound*)m_GranularSound[quality];
	u32 timeInMs = fwTimer::GetTimeInMilliseconds();
	u32 timeSinceGearChange = timeInMs - m_Parent->GetVehicleEngine()->GetTransmission()->GetLastGearChangeTimeMs();
	u32 timeSinceWobble = timeInMs - m_LastWobbleTimeMs;
	bool gearchangeWobblesEnabled = AUD_GET_TRISTATE_VALUE(m_GranularEngineAudioSettings->Flags, FLAG_ID_GRANULARENGINEAUDIOSETTINGS_GEARCHANGEWOBBLESENABLED) == AUD_TRISTATE_TRUE;

	if(gearchangeWobblesEnabled && 
		timeSinceGearChange < 1000 && 
		state->throttle == 1.0f && 
		state->gear != m_LastGearForWobble)
	{
		if(state->gear > 1)
		{
			granularSound->SetWobbleEnabled((u32)(m_GranularEngineAudioSettings->GearChangeWobbleLength * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobbleLengthVariance, 1.0f)), 
				m_GranularEngineAudioSettings->GearChangeWobbleSpeed * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobbleSpeedVariance, 1.0f),
				m_GranularEngineAudioSettings->GearChangeWobblePitch * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobblePitchVariance, 1.0f),
				m_GranularEngineAudioSettings->GearChangeWobbleVolume * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobbleVolumeVariance, 1.0f));

			m_LastWobbleTimeMs = timeInMs;
		}

		m_LastGearForWobble = state->gear;
	}
	else if(gearchangeWobblesEnabled && 
		timeSinceWobble >= 500 && 
		timeSinceGearChange >= 1000 && 
		state->gasPedal > 0.8f && 
		m_GasPedalLastFrame < 0.2f && 
		state->fwdSpeedRatio > 0.2f && 
		!state->onRevLimiter)
	{
		// If the player suddenly slams on the throttle after a brief period off, do a slightly slower, less intense wobble
		granularSound->SetWobbleEnabled((u32)(m_GranularEngineAudioSettings->GearChangeWobbleLength * 2.0f * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobbleLengthVariance, 1.0f)), 
			m_GranularEngineAudioSettings->GearChangeWobbleSpeed * 0.75f * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobbleSpeedVariance, 1.0f),
			m_GranularEngineAudioSettings->GearChangeWobblePitch * 0.5f * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobblePitchVariance, 1.0f),
			m_GranularEngineAudioSettings->GearChangeWobbleVolume * audEngineUtil::GetRandomNumberInRange(1.0f - m_GranularEngineAudioSettings->GearChangeWobbleVolumeVariance, 1.0f));

		m_LastWobbleTimeMs = timeInMs;
	}
	// Reversing wobble
	else if(state->gear == 0 && m_Parent->IsReversing())
	{
		granularSound->SetWobbleEnabled(1000, 
			0.15f * Clamp(state->fwdSpeedAbs * 0.1f, 0.0f, 1.0f),
			0.02f,
			0.1f);

		m_ReverseWobbleActive = true;
	}
	else if(state->throttle == 0.0f || m_ReverseWobbleActive)
	{
		granularSound->SetWobbleEnabled(0, 0.0f, 0.0f, 0.0f);
		m_ReverseWobbleActive = false;
	}
}

#if __BANK
// -------------------------------------------------------------------------------
// Update any debug features
// -------------------------------------------------------------------------------
void audGranularEngineComponent::UpdateDebug()
{
	for(s32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
	{
		audGranularSound* granularSound = (audGranularSound*)m_GranularSound[loop];

		if(m_EngineSoundType == ENGINE_SOUND_TYPE_ENGINE && 
		   granularSound && granularSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			if((!g_VehicleAudioAllowEntityFocus && m_Parent->IsPlayerVehicle()) || (g_VehicleAudioAllowEntityFocus && m_Parent->GetVehicle() == g_pFocusEntity))
			{
				if(g_TestWobble)
				{
					granularSound->SetWobbleEnabled((u32)(g_TestWobbleLength * audEngineUtil::GetRandomNumberInRange(1.0f - g_TestWobbleLengthVariance, 1.0f)), 
						g_TestWobbleSpeed * audEngineUtil::GetRandomNumberInRange(1.0f - g_TestWobbleSpeedVariance, 1.0f),
						g_TestWobblePitch * audEngineUtil::GetRandomNumberInRange(1.0f - g_TestWobblePitchVariance, 1.0f),
						g_TestWobbleVolume * audEngineUtil::GetRandomNumberInRange(1.0f - g_TestWobbleVolumeVariance, 1.0f));

					g_TestWobble = false;
				}

				granularSound->SetDebugRenderingEnabled(g_GranularDebugRenderingEnabled);
				granularSound->SetGranularRandomisationStyle((audGranularSubmix::audGrainPlaybackOrder)g_GranularRandomisationStyle);
				granularSound->SetGranularSlidingWindowSize(g_GranularSlidingWindowSizeHzFraction, g_GranularSlidingWindowMinGrains, g_GranularSlidingWindowMaxGrains, g_GranularMinRepeatRate);		

				if(g_GranularLoopEditorShiftLeftPressed)
				{
					granularSound->ShiftLoopLeft();
					g_GranularLoopEditorShiftLeftPressed = false;
				}

				if(g_GranularLoopEditorShiftRightPressed)
				{
					granularSound->ShiftLoopRight();
					g_GranularLoopEditorShiftRightPressed = false;
				}

				if(g_GranularLoopEditorShrinkLoopPressed)
				{
					granularSound->ShrinkLoop();
					g_GranularLoopEditorShrinkLoopPressed = false;
				}

				if(g_GranularLoopEditorGrowLoopPressed)
				{
					granularSound->GrowLoop();
					g_GranularLoopEditorGrowLoopPressed = false;
				}

				if(g_GranularLoopEditorCreateLoopPressed)
				{
					granularSound->CreateLoop();
					g_GranularLoopEditorCreateLoopPressed = false;
				}

				if(g_GranularLoopEditorDeleteLoopPressed)
				{
					granularSound->DeleteLoop();
					g_GranularLoopEditorDeleteLoopPressed = false;
				}

				if(g_GranularLoopEditorExportDataPressed)
				{
					granularSound->ExportData();
					g_GranularLoopEditorExportDataPressed = false;
				}

				if(g_GranularSkipGrainTogglePressed)
				{
					granularSound->SetGrainSkippingEnabled(0, g_GranularSkipEveryOtherGrain, 1, 1, 0.0f);
					granularSound->SetGrainSkippingEnabled(1, g_GranularSkipEveryOtherGrain, 1, 1, 0.0f);
					granularSound->SetGrainSkippingEnabled(2, g_GranularSkipEveryOtherGrain, 1, 1, 0.0f);
					granularSound->SetGrainSkippingEnabled(3, g_GranularSkipEveryOtherGrain, 1, 1, 0.0f);
					granularSound->SetGrainSkippingEnabled(4, g_GranularSkipEveryOtherGrain, 1, 1, 0.0f);
					granularSound->SetGrainSkippingEnabled(5, g_GranularSkipEveryOtherGrain, 1, 1, 0.0f);
					g_GranularSkipGrainTogglePressed = false;
				}

				if(g_GranularToggleTestTonePressed)
				{
					granularSound->SetTestToneEnabled(g_GranularToneGeneratorEnabled);
					g_GranularToggleTestTonePressed = false;
				}

				if(g_SetGranularEngineIdleNative)
				{
					granularSound->SetMixNative(4, true);
					granularSound->SetMixNative(5, false);
				}
				else if(g_SetGranularExhaustIdleNative)
				{
					granularSound->SetMixNative(4, false);
					granularSound->SetMixNative(5, true);					
				}
				else
				{
					granularSound->SetMixNative(4, false);
					granularSound->SetMixNative(5, false);
				}
			}
			else
			{
				granularSound->SetDebugRenderingEnabled(false);
			}
		}

		if(granularSound)
		{
			if(g_VehicleAudioAllowEntityFocus && m_Parent->GetVehicle() != g_pFocusEntity)
			{
				granularSound->SetRequestedVolume(-100);
			}
			else
			{
				granularSound->SetRequestedVolume(0);
			}
		}
	}

	audGranularMix::s_InterGrainCrossfadeStyle = g_GranularInterGrainCrossfadeStyle;
	audGranularMix::s_LoopGrainCrossfadeStyle = g_GranularLoopGrainCrossfadeStyle;
	audGranularMix::s_LoopLoopCrossfadeStyle = g_GranularLoopLoopCrossfadeStyle;
	audGranularMix::s_GranularMixCrossfadeStyle = g_GranularMixCrossfadeStyle;
}
#endif
