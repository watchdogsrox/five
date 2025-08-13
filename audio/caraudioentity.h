//  
// audio/caraudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_CARAUDIOENTITY_H
#define AUD_CARAUDIOENTITY_H

#include "audioengine/curve.h"
#include "audioengine/soundset.h"
#include "audioengine/widgets.h"
#include "audiohardware/waveslot.h"
#include "audioengine/environment.h"
#include "bank/bkmgr.h"
#include "renderer/HierarchyIds.h"
#include "vehicles/VehicleDefines.h"

#include "audio/vehicleengine/transmission.h"
#include "audio/vehicleengine/turbo.h"
#include "audio/vehicleengine/vehicleenginecomponent.h"
#include "audio/vehicleengine/vehicleengine.h"

#include "vehicleaudioentity.h"
#include "collisionaudioentity.h"
#include "audio_channel.h"
#include "gameobjects.h"
#include "audio/environment/environmentgroup.h"
#include "radioemitter.h"
#include "vehiclecollisionaudio.h"
#include "waveslotmanager.h"

#include "audioentity.h"

struct audDynamicWaveSlot;

struct tDamageLayerDescription
{
	u32 soundHash;
	u32 volCurveName;
};

const u32 g_NumDamageLayersPerCar = 3;

class CVehicle;

class audCarAudioEntity : public audVehicleAudioEntity 
{
	friend class audBoatAudioEntity;
	friend class audVehicleCollisionAudio;

public:
	enum SportCarRevsTriggerType
	{
		REVS_TYPE_JUNCTION,
		REVS_TYPE_PASSBY,
		REVS_TYPE_DEBUG,
		REVS_TYPE_MAX,
	};

public:
	AUDIO_ENTITY_NAME(audCarAudioEntity);

	audCarAudioEntity();
	virtual ~audCarAudioEntity();

	static void InitClass();
	static void UpdateClass();

	virtual void Reset();
	void Init() { naAssertf(0, "audCarAudioEntity must be initialized with a valid CVehicle ptr"); }

	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspVariables);
	virtual bool InitVehicleSpecific();

	virtual void UpdateParkingBeep(float distance);
	virtual void StopParkingBeepSound();
	virtual void Fix();
	virtual void Shutdown();

	inline s16 GetEnginePitchOffset() const { return m_EnginePitchOffset; }
	virtual f32 GetDrowningSpeed() const { return 2.5f; }
	virtual VehicleVolumeCategory GetVehicleVolumeCategory();
	virtual void TriggerDoorOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorCloseSound(eHierarchyId doorIndex, const bool isBroken);
	virtual void TriggerDoorFullyOpenSound(eHierarchyId doorIndex);
	
	virtual void OnFocusVehicleChanged();
	virtual u32 GetLeftWaterSound() const;
	virtual u32 GetWaveHitSound() const;
	virtual u32 GetWaveHitBigSound() const;
	virtual u32 GetJumpLandingSound(bool interiorView) const;
	virtual u32 GetVehicleRainSound(bool interiorView) const;
	virtual u32 GetTowTruckSoundSet() const;
	virtual u32 GetDamagedJumpLandingSound(bool interiorView) const;
	virtual u32 GetNPCRoadNoiseSound() const;
	virtual u32 GetReverseWarningSound() const;
	virtual u32 GetConvertibleRoofSoundSet() const;
	virtual u32 GetConvertibleRoofInteriorSoundSet() const;
	virtual u32 GetTurretSoundSet() const;
	virtual u32 GetForkliftSoundSet() const;
	virtual u32 GetDiggerSoundSet() const;
	virtual u32 GetWindNoiseSound() const;
	virtual u32 GetCabinToneSound() const;
	virtual u32 GetEngineShutdownSound() const;
	virtual u32 GetVehicleShutdownSound() const;
	virtual u32 GetEngineStartupSound() const;
	virtual u32 GetEngineIgnitionLoopSound() const;
	virtual f32 GetVehicleModelRadioVolume() const;
	virtual f32 GetControllerRumbleIntensity() const;
	virtual f32 GetMinOpenness() const;
	virtual f32 GetEngineSwitchFadeTimeScalar() const;
	virtual f32 GetInteriorViewEngineOpenness() const;
	virtual bool IsKickstarted() const;
	virtual bool HasCBRadio() const;
	virtual bool IsDamageModel() const;
	virtual bool IsToyCar() const;
	virtual bool IsGolfKart() const;
	virtual bool IsGoKart() const;
	virtual bool AreTyreChirpsEnabled() const;
	virtual bool HasReverseWarning() const;
	virtual bool HasDoorOpenWarning() const;
	virtual bool HasHeavyRoadNoise() const;
	virtual bool CanCauseControllerRumble() const;
	virtual bool HasAlarm(); 
	virtual void SetIsInCarModShop(bool inCarMod);
	virtual f32 GetExhaustProximityVolumeBoost() const;
	virtual RandomDamageClass GetRandomDamageClass() const;
	virtual f32 ComputeIgnitionHoldTime(bool isStartingThisTime);
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;
	virtual bool IsPlayingStartupSequence() const;
	virtual bool IsSubmersible() const;
	virtual const u32 GetWindClothSound() const { return (m_CarAudioSettings ? m_CarAudioSettings->WindClothSound : 0);};

	virtual void OnPlayerOpenDoor();

	void TriggerHandbrakeSound();
	GranularEngineAudioSettings * GetGranularEngineAudioSettings() const;

	inline VehicleEngineAudioSettings * GetVehicleEngineAudioSettings() const { return m_VehicleEngineSettings; }
	inline CollisionMaterialSettings *GetCachedMaterialSettings() const { return m_CachedMaterialSettings; }
	inline audVehicleEngine* GetVehicleEngine() { return &m_vehicleEngine; }
	inline const audVehicleEngine* GetVehicleEngine() const { return &m_vehicleEngine; }
	inline audVehicleKersSystem* GetVehicleKersSystem()	{ return m_vehicleEngine.GetKersSystem(); }
	inline bool HasAutoShutOffEngine() const { return m_HasAutoShutOffEngine; }
	inline f32 GetAutoShutOffTimer() const { return m_AutoShutOffTimer; }
	inline u32 GetLastTimeStationary() const { return m_LastTimeStationary; }
	inline bool IsStartupSoundPlaying() const { return m_vehicleEngine.GetStartupSound() != NULL; }
	inline bool IsSportsCarRevsSoundPlayingOrAboutToPlay() { return m_SportsCarRevsSound != NULL || m_SportsCarRevsRequested; }
	inline void SetScriptStartupRevsSoundRef(audMetadataRef soundRef) { m_ScriptStartupRevsSoundRef = soundRef; }
	inline void CancelScriptStartupRevsSound() { m_ScriptStartupRevsSoundRef = g_NullSoundRef; }

	void TriggerSportsCarRevs(SportCarRevsTriggerType triggerType);
	void BlipHorn();
	audMetadataRef GetClatterSound() const;
	audMetadataRef GetChassisStressSound() const;

	f32 GetChassisStressSensitivityScalar() const;
	f32 GetClatterSensitivityScalar() const;
	f32 GetChassisStressVolumeBoost() const;
	f32 GetClatterVolumeBoost() const;

	inline void SetExhaustPopsEnabled(bool enabled) { m_AreExhaustPopsEnabled = enabled; }
	bool AreExhaustPopsEnabled() const;

#if __BANK
	static void AddWidgets(bkBank &bank);
	static void UpdateVolumeCaptureUpdateThread();
#endif

	void TriggerTrailerBump(f32 volume);
	bool CanBeDescribedAsACar();
	void BlipThrottle();
	void EngineStarted();
	void SetBoostActive(bool active);

	virtual bool IsInAmphibiousBoatMode() const { return m_IsInAmphibiousBoatMode; }
	void SetInAmphibiousBoatMode(bool boatMode) { m_IsInAmphibiousBoatMode = boatMode; }
	BoatAudioSettings* GetAmphibiousBoatSettings() const { return m_AmphibiousBoatSettings; }

	bool IsEngineUpgraded() const;
	bool IsExhaustUpgraded() const;
	bool IsTransmissionUpgraded() const;
	bool IsTurboUpgraded() const;
	bool IsRadioUpgraded() const;
	bool IsElectricOrHybrid() const;
	Vec3V_Out GetRadioEmitterOffset() const;	

	static audSoundSet* GetDamageSoundSet() { return &sm_DamageSoundSet; }

	void OnModChanged(eVehicleModType modSlot, u8 modSlotnewModIndex);

	void TriggerRoofStuckSound();

	virtual u32 GetSoundFromObjectData(u32 fieldHash) const
	{ 
		if(m_CarAudioSettings) 
		{ 
			u32 *ret = (u32 *)m_CarAudioSettings->GetFieldPtr(fieldHash);
			return (ret) ? *ret : 0; 
		} 
		return 0;
	}

	audSimpleSmootherDiscrete* GetRevsSmoother() { return &m_RevsSmoother; }

	CarAudioSettings *GetCarAudioSettings();

	virtual u32 GetEngineSubmixSynth() const;
	virtual u32 GetEngineSubmixSynthPreset() const;
	virtual u32 GetExhaustSubmixSynth() const;
	virtual u32 GetExhaustSubmixSynthPreset() const;
	virtual u32 GetEngineSubmixVoice() const;
	virtual u32 GetExhaustSubmixVoice() const;

	virtual f32 GetSkidVolumeScale() const;
	virtual AmbientRadioLeakage GetPositionalPlayerRadioLeakage() const;
	virtual bool CalculateIsInWater() const;

#if GTA_REPLAY
	void SetIsInWater(bool val) { m_ReplayIsInWater = val; }
	void SetVehiclePhysicsInWater(bool val) { m_ReplayVehiclePhysicsInWater = val; }
#endif

private:
	bool IsUsingGranularEngine();
	void ShutdownPrivate();
	void ConvertFromDummy();
	void ConvertToDummy();
	void ConvertToSuperDummy();
	void ConvertToDisabled();	
	void PopulateCarAudioVariables(audVehicleVariables* state);
	void StartEngineSounds(const bool fadeIn);
	void StopEngineSounds(const bool playEngineStop);
    void UpdateHydraulicSuspension();
	bool UpdateHydraulicSuspensionInputLoop();
	bool UpdateHydraulicSuspensionAngleLoop(bool useDonkSuspension);
	void UpdateOffRoad(audVehicleVariables* state);
	void UpdateCarPassSounds(audVehicleVariables* state);
	void UpdatePlayerCarSpinSounds(audVehicleVariables* state);
	void UpdateSuspension(audVehicleVariables* state);
	void UpdateJump(audVehicleVariables *state);
	void UpdateSpecialFlightMode(audVehicleVariables *state);
	void UpdateToyCar(audVehicleVariables *state);
	void UpdateBrakes(audVehicleVariables *state);
	void UpdateWind(audVehicleVariables *state);
	void UpdateDamage(audVehicleVariables *state);
	void UpdateBlips(audVehicleVariables* state);
	void UpdateSportsCarRevs(audVehicleVariables* state);
	void UpdateRevsBoostEffect(audVehicleVariables* state);
	void UpdateAmphibiousBoatMode(audVehicleVariables* state);
	void UpdateWooWooSound(audVehicleVariables* state);
	void UpdateGliding(audVehicleVariables* state);
	void TriggerBrakeRelease(f32 brakeHoldTime, bool isHandbrake);
	void SetupVolumeCones();
	void UpdateIndicator();
	void SetHydraulicSuspensionActive(bool hydraulicsActive);
	void TriggerHydraulicBounce(bool smallBounce);
	audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);	
	bool AcquireWaveSlot();

#if __BANK
	void UpdateDebug(audVehicleVariables* state);
	static void UpdateVolumeCapture();
#endif

	virtual u32 GetIdleHullSlapSound() const;
	virtual f32 GetIdleHullSlapVolumeLinear(f32 speed) const;
	virtual bool DoesHornHaveTail() const;
	virtual u32 GetVehicleHornSoundHash(bool ignoreMods = false);	
	virtual u32 GetNormalPattern() const	 { return ATSTRINGHASH("CARS_HP_NORMAL", 0xDD12E104); }
	virtual u32 GetAggressivePattern() const { return ATSTRINGHASH("CARS_HP_AGGRESSIVE", 0x9AD2FED8); }
	virtual u32 GetHeldDownPattern() const	 { return ATSTRINGHASH("CARS_HP_HELD_DOWN", 0xB13ACB40); }

private:
	audVehicleEngine m_vehicleEngine;
	audSimpleSmootherDiscrete m_RevsSmoother;
	audSmoother m_ThrottleSmoother;
	audSimpleSmootherDiscrete m_BankSpraySmoother;
	audSimpleSmootherDiscrete m_BrakeLinearVolumeSmoother;
	audSimpleSmootherDiscrete m_RollOffSmoother;
	audSimpleSmoother m_ReversingExhaustVolSmoother;
	audSimpleSmootherDiscrete m_TyreDamageGrindRatioSmoother;
	audSimpleSmootherDiscrete m_StartupRevsVolBoostSmoother;
	audFluctuatorProbBased m_EngineDamageFrequencyRatioFluctuator;
	SportCarRevsTriggerType m_SportsCarRevsTriggerType;

	u32 m_GearChangeTime;
	u32 m_EngineStartTime;
	f32 m_BrakeHoldTime;

	audVolumeConeLite m_EngineVolumeCone, m_ExhaustVolumeCone;

	audSound *m_DiscBrakeSqueel, *m_BrakeReleaseSound;
	audSound *m_WheelGrindLoops[NUM_VEH_CWHEELS_MAX];
	audSound *m_SuspensionSounds[NUM_VEH_CWHEELS_MAX];
	audSound *m_TyreDamageLoops[NUM_VEH_CWHEELS_MAX];
	audSound *m_WheelOffRoadSounds[NUM_VEH_CWHEELS_MAX];
	audSound *m_OffRoadRumbleSound;
	audSound *m_ParkingBeep;
	audSound *m_SpecialFlightModeSound;
	audSound* m_NOSBoostLoop;
	audSound* m_StartUpRevsSound;
	audSound* m_RevsBlipSound;
	audSound* m_WindFastSound;
	audSound* m_InAirRotationSound;
    audSound* m_HydraulicSuspensionInputLoop;
	audSound* m_HydraulicSuspensionAngleLoop;
	audSound *m_EngineDamageLayers[g_NumDamageLayersPerCar];
	audSound *m_DamageTyreScrapeSound;
	audSound *m_DamageTyreScrapeTurnSound;
	audSound *m_SportsCarRevsSound;
	audSound *m_OffRoadRumblePulse;
	audSound *m_BankSpray;
	audSound *m_WooWooSound;
	audSound *m_GliderSound;
	audSound *m_ToyCarEngineLoop;
	audSound *m_SubmarineTransformSound;

	s32 m_EngineDamageLayerIndex[g_NumDamageLayersPerCar];
	u32 m_WheelOffRoadSoundHashes[NUM_VEH_CWHEELS_MAX];
	f32	m_LastCompressionChange[NUM_VEH_CWHEELS_MAX];
	u32 m_LastBottomOutTime[NUM_VEH_CWHEELS_MAX];
	f32 m_DistanceToPlayerLastFrame;
	f32 m_DistanceToPlayerDeltaLastFrame;
	f32 m_DamageThrottleCutTimer;
	f32 m_WaterTurbulenceOutOfWaterTimer;
	f32 m_WaterTurbulenceInWaterTimer;			
	u32 m_NextDamageThrottleCutTime;
	u32 m_ConsecutiveShortThrottleCuts;
	u32 m_LastCarByTime;
	s32 m_ThrottleBlipTimer;
	f32 m_MaxSpeedPitchFactor;
	f32 m_MaxSpeedRevsAddition;
	f32 m_RevsLastFrame;
	f32 m_LeanRatio;
    f32 m_HydraulicBounceTimer;
	u32 m_LastSportsCarRevsTime;
	u32 m_LastHydraulicBounceTime;
	u32 m_TimeAlarmLastArmed;

	CarAudioSettings *m_CarAudioSettings;
	BoatAudioSettings* m_AmphibiousBoatSettings;
	VehicleEngineAudioSettings * m_VehicleEngineSettings;
	GranularEngineAudioSettings * m_GranularEngineSettings;
	ElectricEngineAudioSettings * m_ElectricEngineSettings;

	f32 m_JumpThrottleCutTimer;

	audCurve m_WaterTurbulenceVolumeCurve;
	audCurve m_VehicleSpeedToHullSlapVol;
	audCurve m_WaterTurbulencePitchCurve;
	audSmoother m_FakeRevsSmoother;
	audMetadataRef m_ScriptStartupRevsSoundRef;
	u32 m_PrevOffRoadRumbleHash;
	u32 m_NextDoorTime;
	u32 m_LastTimeStationary;
	f32 m_AutoShutOffTimer;
	f32 m_SportsCarRevsApplyFactor;
	f32 m_AdditionalRevsSmoothing;
	f32 m_FakeOutOfWaterTimer;
	f32 m_FakeRevsHoldTime;
	f32 m_OutOfWaterRevsMultiplier;

	bool m_FakeOutOfWater : 1;
	bool m_WasHighFakeRevs : 1;
	bool m_WasEngineOnLastFrame : 1;
	bool m_WasBrakeHeldLastFrame : 1;
	bool m_WasStationaryLastFrame : 1;	
	bool m_ForceThrottleBlip : 1;
	bool m_WasInCarByRange : 1;
	bool m_IsIndicatorOn : 1;
	
	atFixedBitSet<NUM_VEH_CWHEELS_MAX> m_IsTyreCompletelyDead;
	bool m_AreExhaustPopsEnabled : 1;
	bool m_HasPlayedStartupSequence : 1;
	bool m_WasPlayingStartupSound : 1;	
	bool m_HasAutoShutOffEngine : 1;
	bool m_IsGeneratingReflections : 1;
	bool m_HasRandomWheelDamage : 1;
	bool m_HasInitialisedVolumeCones : 1;
	bool m_HasBlippedSinceStopping : 1;
	bool m_WasUpsideDownInAir : 1;
	bool m_HasPlayedInitialJumpStress : 1;
	bool m_HasPlayedInAirJumpStress : 1;
	bool m_WasWaitingAtLights : 1;
	bool m_WasDoingBurnout : 1;
	bool m_SportsCarRevsEnabled : 1;
	bool m_SportsCarRevsRequested : 1;
	bool m_WasSportsCarRevsCancelled : 1;
	bool m_FakeEngineStartupRevs : 1;
	bool m_HydraulicAngleLoopRetriggerValid : 1;
	bool m_IsInAmphibiousBoatMode : 1;

#if GTA_REPLAY
	bool m_ReplayIsInWater : 1;
	bool m_ReplayVehiclePhysicsInWater : 1;
#endif

	static SportsCarRevsSettings* sm_SportsCarRevsSettings;
	static audCurve sm_EnvironmentalLoudnessThrottleCurve, sm_EnvironmentalLoudnessRevsCurve;
	static audCurve sm_SpeedToBrakeDiscVolumeCurve, sm_BrakeToBrakeDiscVolumeCurve;
	static audCurve sm_BrakeHoldTimeToStartOffsetCurve;
	static audCurve sm_SuspensionVolCurve;
	static audCurve sm_SkidFwdSlipToScrapeVol;
	static audCurve sm_LateralSlipToSideVol;
	static audCurve sm_PhysicsRevsToGranularRevs;
	static audSmoother sm_PlayerWindNoiseSpeedSmoother;
	static audSoundSet sm_DamageSoundSet;

	static f32 sm_InnerUpdateFrames, sm_OuterUpdateFrames, sm_InnerCheapUpdateFrames, sm_OuterCheapUpdateFrames;
	static f32 sm_MaxSuspensionStartOffset;
	static f32 sm_TimeToApplyHandbrake;
	static f32 sm_CarFwdSpeedSum;
	static f32 sm_NumActiveCars;
	static u32 sm_LastSportsRevsTime;

	static bool sm_ShouldDisplayEngineDebug, sm_ShouldFlipPlayerCones;

	struct carHLODInfo
	{
		RegdVeh veh;
		f32		fitness;
	};

	static atRangeArray<carHLODInfo,NumQuadrants> sm_HLODCars; 

	enum { kSportsCarRevsHistorySize = 2 };
	struct SportsCarRevHistoryEntry
	{
		u32 soundName;
		u32 time;
	};

	static atRangeArray<SportsCarRevHistoryEntry, kSportsCarRevsHistorySize> sm_SportsCarRevsSoundHistory;
	static u32 sm_SportsCarRevsHistoryIndex;

#if __BANK
	struct carPassHistory
	{
		const char* vehicleName;
		f32 relativeSpeed;
		f32 boundingVolume;
		f32 triggerDistance;
	};

	static atArray<carPassHistory> sm_CarPassHistory;

public:
	enum VolumeCaptureState
	{
		State_NextVehicle,
		State_CapturingAccel,
		State_CapturingAccelModded,
	};

	static Vector3 sm_VolumeCaptureStartPos;
	static VolumeCaptureState sm_VolumeCaptureState;
	static s32 sm_VolumeCaptureVehicleIndex;
	static s32 sm_VolumeCaptureVehicleStartIndex;
	static f32 sm_VolumeCaptureTimer;
	static f32 sm_VolumeCaptureIntervalTimer;
	static u32 sm_VolumeCaptureFramesAtState;
	static f32 sm_VolumeCaptureRevs;
	static f32 sm_VolumeCaptureThrottle;
	static f32 sm_VolumeCapturePeakLoudness;
	static f32 sm_VolumeCapturePeakRMS;
	static f32 sm_VolumeCapturePeakAmplitude;
	static FileHandle sm_VolumeCaptureFileID;
	static atArray<f32> sm_VolumeCaptureLoudnessHistory;
	static atArray<f32> sm_VolumeCaptureRMSHistory;
	static atArray<f32> sm_VolumeCaptureAmplitudeHistory;
	static fwModelId sm_VolumeCaptureVehicleRequest;
#endif
};


#endif // AUD_CARAUDIOENTITY_H
