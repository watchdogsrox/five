//  
// audio/planeaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_PLANEAUDIOENTITY_H
#define AUD_PLANEAUDIOENTITY_H

#include "vehicleaudioentity.h"
#include "vehicleengine/vehicleengine.h"

enum PlaneLoops
{
	PLANE_LOOP_ENGINE,
	PLANE_LOOP_EXHAUST,
	PLANE_LOOP_IDLE,
	PLANE_LOOP_DISTANCE,
	PLANE_LOOP_PROPELLOR,
	PLANE_LOOP_BANKING,
	PLANE_LOOP_AFTERBURNER,
	PLANE_LOOP_DAMAGE,
	PLANE_LOOPS_MAX,
};

#define MAX_PLANE_WHEELS 10

// ----------------------------------------------------------------
// Helper class to store status variables
// ----------------------------------------------------------------
struct audPlaneValues
{
	audPlaneValues() :
		  propellorHealthFactor(1.0f)
		, bankingVolume(-100.0f)
		, bankingPitch(0)
		, haveBeenCalculated(false)
	{
		for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
		{
			loopVolumes[loop] = -100.0f;
			postSubmixVolumes[loop] = -100.0f;
			loopPitches[loop] = 0;
		}
	}

	f32 propellorHealthFactor;
	f32 bankingVolume;
	s32 bankingPitch;
	f32 loopVolumes[PLANE_LOOPS_MAX];
	s32 loopPitches[PLANE_LOOPS_MAX];
	f32 postSubmixVolumes[PLANE_LOOPS_MAX];
	bool haveBeenCalculated;
};

// ----------------------------------------------------------------
// audPlaneAudioEntity class
// ----------------------------------------------------------------
class audPlaneAudioEntity : public audVehicleAudioEntity
{
// Public methods
public:
	audPlaneAudioEntity();
	virtual ~audPlaneAudioEntity();

	AUDIO_ENTITY_NAME(audPlaneAudioEntity);

	virtual void Reset();
	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspSettings);
	virtual bool InitVehicleSpecific();
	virtual void PostUpdate();
	virtual void OnFocusVehicleChanged();

	virtual void TriggerDoorOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorCloseSound(eHierarchyId doorIndex, const bool isBroken);
	virtual void TriggerDoorFullyOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorStartOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorStartCloseSound(eHierarchyId doorIndex);

	virtual f32 GetDrowningSpeed() const { return 0.6f; }

	virtual u32 GetEngineSubmixSynth() const;
	virtual u32 GetEngineSubmixSynthPreset() const;
	virtual u32 GetExhaustSubmixSynth() const;
	virtual u32 GetExhaustSubmixSynthPreset() const;
	virtual u32 GetEngineSubmixVoice() const;
	virtual u32 GetExhaustSubmixVoice() const;
	virtual u32 GetCabinToneSound() const;
	virtual u32 GetIdleHullSlapSound() const;
	virtual u32 GetLeftWaterSound() const;
	virtual u32 GetWaveHitSound() const;
	virtual u32 GetWaveHitBigSound() const;
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;
	virtual void BreakOffSection(int nHitSection);
	virtual u32 GetNPCRoadNoiseSound() const;
	virtual u32 GetVehicleRainSound(bool interiorView) const;

	virtual u32 GetTurretSoundSet() const;
	virtual bool IsPlayingStartupSequence() const;
	virtual f32 GetIdleHullSlapVolumeLinear(f32 speed) const;
	virtual bool CalculateIsInWater() const;
	
	void TriggerDownwash(const u32 soundHash, const Vector3 &pos, const f32 distEvo, const f32 speedEvo);
	void DeployLandingGear();
	void RetractLandingGear();
	void SetCropDustingActive(bool active);
	void SetStallWarningEnabled(bool enabled);
	bool IsEngineOn() const;
	void OnVerticalFlightModeChanged(f32 newFlightModeRatio);

	audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);
	bool RequiresWaveSlot() const								{ return IsEngineOn(); }
	bool IsStartupSoundPlaying() const							{ return m_IgnitionSound != NULL; }
	bool GetIsStallWarningOn() const							{ return m_StallWarningOn; }
	void ResetEngineSpeedSmoother()								{ m_EngineSpeedSmoother.Reset(); }
	void SetDownwashHeightFactor(f32 downwashHeight)			{ m_DownwashHeight = downwashHeight; }

	void AdjustControlSurface(int partId, f32 newRotation);
	void TriggerPropBreak();
	void TriggerBackFire();
	void SetPropellorSpeedMult(s32 index, f32 mult);
	PlaneAudioSettings* GetPlaneAudioSettings();
	void UpdateEngineCooling();
	void UpdateFlyby();
	void TriggerFlyby();
	//void TriggerFlyby2();
	static void InitClass();

	virtual u32 GetSoundFromObjectData(u32 fieldHash) const;
	virtual f32 GetSeriousDamageThreshold()						{ return m_PlaneAudioSettings ? m_PlaneAudioSettings->AircraftWarningSeriousDamageThresh : 0.0f; }
	virtual f32 GetCriticalDamageThreshold()					{ return m_PlaneAudioSettings ? m_PlaneAudioSettings->AircraftWarningCriticalDamageThresh : 0.0f; }
	virtual f32 GetOverspeedValue()								{ return m_PlaneAudioSettings ? m_PlaneAudioSettings->AircraftWarningMaxSpeed : 0.0f; }
	virtual bool AircraftWarningVoiceIsMale();

	virtual AmbientRadioLeakage GetPositionalPlayerRadioLeakage() const;

	inline audVehicleKersSystem* GetVehicleKersSystem()			{ return &m_KersSystem; }

#if __BANK
	static void AddWidgets(bkBank &bank);
	void ShowDebugInfo(audPlaneValues& planeValues);
#endif

protected:
	virtual f32 GetRoadNoiseVolumeScale() const;

// Private types
private:
	enum audPlaneStatus
	{
		AUD_PLANE_OFF,
		AUD_PLANE_STARTING,
		AUD_PLANE_ON,
		AUD_PLANE_STOPPING
	};

// Private methods
private:
	void StartEngine();
	void StopEngine(bool fadeOut = false);
	void ShutdownJetEngine();
	void SetUpVolumeCurves();

#if __BANK
	void UpdateDebug();
#endif

	void UpdateGliding();
	void UpdatePitchRoll(audPlaneValues& planeValues);
	void UpdateTricks();
	void UpdateStallWarning(audPlaneValues& planeValues);
	void UpdateSounds(audPlaneValues& planeValues);
	void UpdateEngineOn(audPlaneValues& planeValues);
	void UpdateVTOL();
	void UpdateTyreSqueals(); 
	void UpdateSuspension();
	void UpdateSeaPlane();
	void UpdateWindNoise(audPlaneValues& planeValues);
	bool AcquireWaveSlot();
	void ConvertToDisabled();
	void UpdateControlSurfaces();
	void UpdateDiveSound(audPlaneValues& planeValues);
	void SetTracker(audSoundInitParams &initParams);
	void FreeSpeechSlot();
	f32 GetPropellorHealthFactor() const;

// Private attributes
private:
	PlaneAudioSettings* m_PlaneAudioSettings;
	audSound* m_StallWarning;
	audSound* m_LandingGearDeploy;
	audSound* m_LandingGearRetract;
	audSound* m_CropDustingSound;
	audSound* m_WindSound;
	audSound* m_EngineCoolingSound;
	audSound* m_DownwashSound;
	audSound* m_DiveSound;
	audSound* m_FlybySound;
	audSound* m_IgnitionSound;
	audSound* m_VTOLModeSwitchSound;
	audSound* m_GliderSound;
	audSound* m_SuspensionSounds[NUM_VEH_CWHEELS_MAX];
	f32	m_LastCompressionChange[NUM_VEH_CWHEELS_MAX];
	u32 m_DownwashHash;
	u32 m_LastDownwashTime;
	f32 m_DownwashVolume;
	f32 m_DownwashHeight;
	s32 m_DiveSoundPitch;
	f32 m_DiveSoundVolumeLin;
	audPlaneStatus m_PlaneStatus;
	audVehicleKersSystem m_KersSystem;
	audSound* m_PlaneLoops[PLANE_LOOPS_MAX];
	audVehicleSounds m_PlaneLoopTypes[PLANE_LOOPS_MAX];
	u32 m_PlaneLoopHashes[PLANE_LOOPS_MAX];
	audCurve m_PlaneLoopVolumeCurves[PLANE_LOOPS_MAX];
	audCurve m_PlaneLoopPitchCurves[PLANE_LOOPS_MAX];
	audCurve m_VehicleSpeedToHullSlapVol;
	audCurve m_DistanceEffectVolCurve;
	audCurve m_BankingAngleVolCurve;
	audCurve m_PeelingPitchCurve;
	audCurve m_DamageVolumeCurve;
	audVolumeConeLite m_PlaneLoopVolumeCones[PLANE_LOOPS_MAX];
	audSimpleSmoother m_angularVelocitySmoother;
	audSmoother m_angleBasedPitchLoopSmoother;
	audSmoother m_angleBasedVolumeSmoother;
	audSmoother m_angleBasedPitchSmoother;
	audSimpleSmoother m_peelingPitchSmoother;
	audSimpleSmoother m_EngineSpeedSmoother;
	audSimpleSmoother m_DopplerSmoother;
	audSmoother m_DistantSoundVolumeSmoother;
	audSoundSet m_VTOLThrusterSoundset;
	bool m_WheelInAir[MAX_PLANE_WHEELS];
	f32 m_TimeInAir;
	f32 m_PropellorHealthFactorLastFrame;
	f32 m_PeelingFactor;
	f32 m_BankingAngle;
	f32 m_EngineSpeed;
	f32 m_FakeEngineSpeed;
	
	bool m_IsUsingStealthMicrolightGameobject : 1;
	bool m_HasPlayedShutdownSound : 1;
	bool m_StallWarningOn : 1;
	bool m_StallWarningEnabled : 1;
	bool m_IsPeeling : 1;
	bool m_IsJet : 1;
	bool m_OnGround : 1;
	bool m_FlybyComingAtYou : 1;
	bool m_InARecording : 1;
	bool m_EngineInstantOnOff : 1;
	
	f32 m_PropellorSpeedMult[PLANE_NUM_PROPELLERS];
	u32 m_NextMissFireSoundTime;
	u32 m_BackFireTime;
	f32 m_EngineOffTemperature;
	f32 m_ClosingRate;
	f32 m_ClosingRateAccum;
	s32 m_NumClosingSamples;
	u32 m_TimeLastClosingRateUpdated;
	u32 m_FlybyStartTime;
	f32 m_LastDistanceFromListener;
	f32 m_DistanceFromListener;
	s32 m_SpeechSlotId;
	f32 m_AirSpeed;
				
	u32 m_TimeStallStarted;

	s32 m_IgnitionTime;
		
#if __BANK
	audPlaneValues m_DebugPlaneValues;
	audDebugTracker m_DebugTracker;

	s32 m_DebugDivePitchLoops;
	f32 m_DebugVertAirSpeed;
	f32 m_DebugDivingRate;
	f32 m_DebugOcclusion;
#endif

	static audCurve sm_BankingAngleToWindNoiseVolumeCurve;
	static audCurve sm_BankingAngleToWindNoisePitchCurve;
	static audCurve sm_SuspensionVolCurve;
	static audCurve sm_JetClosingToFlybyDistance;
	static audCurve sm_PropClosingToFlybyDistance;

	atRangeArray<f32, 6> m_LastControlSurfaceRotation; //PLANE_AILERON_R - PLANE_RUDDER
	audSound* m_ControlSurfaceMovementSound[3];

};

#endif // AUD_PLANEAUDIOENTITY_H





















