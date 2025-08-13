//  
// audio/heliaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_HELIAUDIOENTITY_H
#define AUD_HELIAUDIOENTITY_H

#include "vehicleaudioentity.h"
#include "vehicleengine/vehicleengine.h"
#include "renderer/HierarchyIds.h"
#include "vehicles/LandingGear.h"
#include "audiosoundtypes/retriggeredoverlappedsound.h"

#define NUM_NPC_PICH_SHIFTS 3

// ----------------------------------------------------------------
// Helper class to store status variables
// ----------------------------------------------------------------
struct audHeliValues
{
	audHeliValues()	:
	  bladeVolume(g_SilenceVolume)
	, rotorPitch(0)
	, rotorVolume(g_SilenceVolume)
	, exhaustVolume(g_SilenceVolume)
	, exhaustPitch(0)
	, bankingVolume(g_SilenceVolume)
	, bankingPitch(0)
	, rearRotorVolume(g_SilenceVolume)
	, throttle(0.0f)
	, startup(0.0f)
	, banking(0.0f)
	, health(0.0f)
	, damageVolume(0.0f)
	, throttleInput(0.0f)
	, damageVolumeBelow600(0.0f)
	, triggerRate(0.0f)
	, interiorView(false)
	{
	}

	f32 bladeVolume;
	s32 rotorPitch;
	f32 rotorVolume;
	f32 exhaustVolume;
	s32 exhaustPitch;
	f32 bankingVolume;
	s32 bankingPitch;
	f32 rearRotorVolume;
	f32 throttle;
	f32 startup;
	f32 banking;
	f32 health;
	f32 damageVolume;
	f32 damageVolumeBelow600;
	f32 triggerRate;
	f32 throttleInput;
	bool interiorView;
};

// ----------------------------------------------------------------
// audHeliAudioEntity class
// ----------------------------------------------------------------
class audHeliAudioEntity : public audVehicleAudioEntity
{
// Public methods
public:
	audHeliAudioEntity();
	virtual ~audHeliAudioEntity();

	AUDIO_ENTITY_NAME(audHeliAudioEntity);

	void SetHeliShouldSkipStartup(bool skipStartup);
	void TriggerDownwash(const u32 soundHash, const Vector3 &pos, const f32 distEvo, const f32 speedEvo, const f32 angleEvo, const f32 rotorEvo);
	void TriggerEngineFailedToStart();
	void PlayTailBreakSound();
	void PlayRearRotorBreakSound();
	void PlayRotorBreakSound();
	void PlayHookDeploySound();
	void TriggerGoingDownSound();
	void StopGoingDownSound();
	void DeployLandingGear();
	void RetractLandingGear();
	void EnableCrazyHeliSounds(bool enable)		{ m_EnableCrazyHeliSounds = enable; }

	virtual void PostUpdate();
	virtual void Reset();
	virtual void OnFocusVehicleChanged();
	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspSettings);
	virtual bool InitVehicleSpecific();
	virtual void TriggerDoorOpenSound(eHierarchyId doorId);
	virtual void TriggerDoorCloseSound(eHierarchyId doorId, const bool isBroken);
	virtual void TriggerDoorFullyOpenSound(eHierarchyId doorId);
	virtual void TriggerDoorStartOpenSound(eHierarchyId doorId);
	virtual void TriggerDoorStartCloseSound(eHierarchyId doorId);
	virtual u32 GetSoundFromObjectData(u32 fieldHash) const;
	virtual u32 GetVehicleRainSound(bool interiorView) const;
	virtual VehicleVolumeCategory GetVehicleVolumeCategory() { return VEHICLE_VOLUME_VERY_LOUD; }
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;
	virtual void VehicleExploded();
		
	void TriggerRotorImpactSounds();
	s32 GetShockwaveID() const						{ return m_ShockwaveId; }
	void SetShockwaveID(s32 shockwave)				{ m_ShockwaveId = shockwave; }
	audVehicleKersSystem* GetVehicleKersSystem()	{ return &m_KersSystem; }
	bool IsEngineOn() const;

	HeliAudioSettings *GetHeliAudioSettings();

	static void InitClass();

	virtual f32 GetSeriousDamageThreshold()		{return m_HeliAudioSettings ? m_HeliAudioSettings->AircraftWarningSeriousDamageThresh : 0.0f;}
	virtual f32 GetCriticalDamageThreshold()	{return m_HeliAudioSettings ? m_HeliAudioSettings->AircraftWarningCriticalDamageThresh : 0.0f;}
	virtual f32 GetOverspeedValue()				{return m_HeliAudioSettings ? m_HeliAudioSettings->AircraftWarningMaxSpeed : 0.0f;}	
	virtual bool AircraftWarningVoiceIsMale();
	virtual AmbientRadioLeakage GetPositionalPlayerRadioLeakage() const;
	bool IsRadioUpgraded() const;
	
#if __BANK
	static void AddWidgets(bkBank &bank);
	void UpdateDebug();
#endif

#if GTA_REPLAY
	u8 GetHeliAudioState() { return m_HeliState; }
	void ProcessReplayAudioState(u8 PacketState);
	void SetSpeechSlotID(s32 speechSlotId);
	s32 GetSpeechSlotID() { return m_SpeechSlotId; }
#endif //GTA_REPLAY

// Private methods
private:
	virtual u32 GetEngineSubmixSynth() const;
	virtual u32 GetEngineSubmixSynthPreset() const;
	virtual u32 GetExhaustSubmixSynth() const;
	virtual u32 GetExhaustSubmixSynthPreset() const;
	virtual u32 GetEngineSubmixVoice() const;
	virtual u32 GetExhaustSubmixVoice() const;
	virtual u32 GetCabinToneSound() const;
	virtual u32 GetWaveHitSound() const;
	virtual u32 GetWaveHitBigSound() const;
	virtual u32 GetIdleHullSlapSound() const;

	f32 GetIdleHullSlapVolumeLinear(f32 speed) const;
	bool CalculateIsInWater() const;
	audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);
	bool RequiresWaveSlot() const;
	void InitHeliSettings();
	void StopHeliEngineSounds();
	void SetupHeliVolumeCones();
	void StartHeliEngineSounds(audHeliValues& heliValues, bool instantStart = false);
	void UpdateHeliEngineSounds(audHeliValues& heliValues);
	void UpdateHeliEngineSoundsOn(audHeliValues& heliValues);
	void UpdateHeliEngineSoundStartingStopping(audHeliValues& heliValues);
	void UpdateDamageSounds(audHeliValues& heliValues);
	bool AcquireWaveSlot();
	void ConvertToDisabled();
	void ConvertFromDummy();
	void FreeSpeechSlot();	
	void SetTracker(audSoundInitParams &initParams);
	void UpdateSuspension();
	void UpdateWheelSounds();
	void PlayGoingDownSound();
	void UpdateEngineCooling();
	void UpdateRotorImpactSounds(audHeliValues& heliValues);
	f32 UpdateSlowmoRotorSounds();
	void TriggerBackFire();
	void UpdateShockwave();
	void UpdateCrazyFlying(audHeliValues& heliValues);
	void UpdateCustomLandingGearSounds();
	void UpdateJetPackSounds();
	void UpdateSeaPlane();

#if __BANK
	void ShowHeliValues(audHeliValues& heliValues);
#endif

// Private attributes
private:
	HeliAudioSettings* m_HeliAudioSettings;

	audFluctuatorProbBased m_HeliDamageFluctuator;
	audSmoother m_HeliThrottleSmoother;
	audSoundSet m_LandingGearSoundSet;
	audSimpleSmootherDiscrete m_HeliStartupSmoother;
	audSimpleSmootherDiscrete m_HeliShutdownWaterSmoother;
	audSimpleSmootherDiscrete m_HeliSlowmoSmoother;

	audCurve m_BankAngleVolCurve, m_RotorThrottleVolCurve, m_RearRotorThrottleVolCurve, m_ExhaustThrottleVolCurve, m_RotorThrottlePitchCurve;
	audCurve m_BankThrottlePitchCurve, m_ExhaustThrottlePitchCurve, m_BankThrottleVolumeCurve;
	audCurve m_HeliThrottleResonanceCurve1, m_HeliThrottleResonanceCurve2, m_HeliBankingResonanceCurve;
	audCurve m_RotorVolStartupCurve;
	audCurve m_BladeVolStartupCurve;
	audCurve m_RotorPitchStartupCurve;
	audCurve m_RearRotorVolStartupCurve;
	audCurve m_ExhaustVolStartupCurve;
	audCurve m_ExhaustPitchStartupCurve;
	audCurve m_DamageVolumeCurve;
	audCurve m_DamageVolumeBelow600Curve;
	audCurve m_RotorSpeedToTriggerSpeedCurve;
	audCurve m_VehicleSpeedToHullSlapVol;

	static audCurve sm_SuspensionVolCurve;
	static audCurve sm_GritSpeedToLinVolCurve;

	s32 m_ShockwaveId;
	u32 m_HeliDownwashHash;

	audVolumeConeLite m_EngineVolumeCone, m_ExhaustVolumeCone;
	audVolumeConeLite m_RotorBladeVolumeCone, m_RearRotorVolumeCone, m_RotorThumpVolumeCone;

	// Heli sounds
	audSound *m_HeliBank, *m_HeliExhaust, *m_HeliRotor, *m_HeliRearRotor, *m_HeliRotorLowFreq;
	audSound *m_HeliDamage, *m_HeliDamageOneShots, *m_HeliDamageBelow600;
	audSound *m_HeliDownwash, *m_HeliGoingDown;
	audSound *m_AltitudeWarning, *m_DistantRotor;
	audSound *m_HeliDamageWarning;
	audSound *m_SuspensionSounds[NUM_VEH_CWHEELS_MAX];
	audSound *m_ScrapeSound;
	audSound *m_HardScrapeSound;
	audSound *m_StartupSound;
	audSound *m_EngineCoolingSound;
	audSound *m_HeliRotorSlowmo;
	audSound *m_HeliRotorLowFreqSlowmo;
	audSound *m_CustomLandingGearLoop;
	audRetriggeredOverlappedSound* m_RotorImpactSound;
	audVehicleKersSystem m_KersSystem;
	CLandingGear::eLandingGearPublicStates m_PrevLandingGearState;
	
	u32 m_LastRotorImpactTime;
	u32 m_LastRotorImpactSoundTime;
	u32 m_PrevScrapeSound;

	f32	m_LastCompressionChange[NUM_VEH_CWHEELS_MAX];
	f32 m_Throttle;
	f32 m_MainRotorSpeed;
	u32 m_LastHeliDownwashTime;
	f32	m_HeliStartupRatio;	
	f32 m_MainRotorVolume;
	f32 m_PrevJetpackThrusterLeftForce;	
	f32 m_PrevJetpackThrusterRightForce;
	s32 m_SpeechSlotId;
	REPLAY_ONLY(u32 m_LastReplaySpeechSlotAllocTime;)
	u32 m_BackFireTime;
	u32 m_LastTimeInStrafeMode;
	u8	m_HeliState;
	bool m_HasCustomLandingGear : 1;
	bool m_HeliShouldSkipStartup : 1;
	bool m_Shotdown : 1;
	bool m_PlayGoingDown : 1;
	bool m_Exploded : 1;
	bool m_EnableCrazyHeliSounds : 1;
	bool m_HasBeenScannerDecremented : 1;
	bool m_RequiresLowLatencyWaveSlot : 1;
	u32 m_LastJetpackStrafeTime;
	s16 m_PitchShiftIndex;
	f32 m_EngineOffTemperature;

	s32 m_FailedToStartCount;
	u32 m_LastFailToStartTime;
	audSound* m_FailedToStartSound;

	audSound *m_CrazyWarning;
	audSmoother m_CrazyFlyingRotorPitchSmoother;
	audSmoother m_CrazyFlyingDamageSmoother;
	
#if __BANK
	audDebugTracker m_DebugTracker;
	f32 m_DebugOcclusion;
#endif

	static audCurve sm_HeliStartupToNormalCurve;
	static s16 sm_PitchShiftIndex;
	static s16 sm_PitchShift[NUM_NPC_PICH_SHIFTS];
};

#endif // AUD_HELIAUDIOENTITY_H





















