// 
// audio/boataudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_BOATAUDIOENTITY_H
#define AUD_BOATAUDIOENTITY_H

#include "audio_channel.h"
#include "gameobjects.h"

#include "audioentity.h"

#include "audioengine/curve.h"
#include "audioengine/widgets.h"

#include "atl/bitset.h"
#include "scene/RegdRefTypes.h"
#include "vehicles/Boat.h"

#include "audio/vehicleaudioentity.h"
#include "audio/vehicleengine/vehicleengine.h"

// Disabled to reduce memory footprint
#define NON_GRANULAR_BOATS_ENABLED 0

class CBoat;

namespace rage 
{
	class audSound;
}

struct audDynamicWaveSlot;

class audBoatAudioEntity : public audVehicleAudioEntity
{
// Public methods
public:
	AUDIO_ENTITY_NAME(audBoatAudioEntity);

	audBoatAudioEntity();
	virtual ~audBoatAudioEntity();

	static void InitClass();
	virtual void Reset();
	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspVariables);
	virtual bool InitVehicleSpecific();
	virtual void Shutdown();
	virtual f32 GetMinOpenness() const;


	inline void SetKeelForce(const f32 keelForce)		{ m_KeelForce = keelForce; }
	inline void SetSpeed(const f32 speed)				{ m_SpeedEvo = speed; }
	inline u32 GetScannerManufacturerHash() const		{ return m_ManufacturerHash; }
	inline u32 GetScannerModelHash() const				{ return m_ModelHash; }
	inline u32 GetScannerCategoryHash() const			{ return m_CategoryHash; }
	inline u32 GetScannerVehicleSettingsHash() const	{ return m_ScannerVehicleSettingsHash; }
	audVehicleEngine* GetVehicleEngine()				{ return &m_VehicleEngine; }
	audVehicleKersSystem* GetVehicleKersSystem()		{ return m_VehicleEngine.GetKersSystem(); }
	audSimpleSmootherDiscrete* GetRevsSmoother()		{ return &m_RevsSmoother; }

	virtual bool CalculateIsInWater() const;
	void EngineStarted();
	GranularEngineAudioSettings* GetGranularEngineAudioSettings() const;	
	virtual bool IsSubmersible() const;

	virtual bool IsWaterDiveSoundPlaying() const { return m_WaterDiveSound != NULL; }
	virtual f32 GetDrowningSpeed() const { return 20.0f; }

	virtual f32 ComputeIgnitionHoldTime(bool isStartingThisTime);
	virtual u32 GetEngineIgnitionLoopSound() const;
	virtual u32 GetEngineShutdownSound() const;
	virtual u32 GetEngineStartupSound() const;
	virtual u32 GetIdleHullSlapSound() const;
	virtual u32 GetLeftWaterSound() const;
	virtual u32 GetWaveHitSound() const;
	virtual u32 GetWaveHitBigSound() const;
	virtual u32 GetVehicleRainSound(bool interiorView) const;
	virtual f32 GetEngineSwitchFadeTimeScalar() const;
	virtual f32 GetWaveHitBigAirTime() const;
	virtual u32 GetTurretSoundSet() const;

	virtual void TriggerDoorOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorCloseSound(eHierarchyId doorIndex, const bool isBroken);
	virtual void TriggerDoorFullyOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorStartCloseSound(eHierarchyId doorIndex);

	virtual const u32 GetWindClothSound() const { return (m_BoatAudioSettings ? m_BoatAudioSettings->WindClothSound : 0);};
	BoatAudioSettings *GetBoatAudioSettings();

	virtual u32 GetSoundFromObjectData(u32 fieldHash) const;
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

// Private types
private:
#if NON_GRANULAR_BOATS_ENABLED
	enum BoatEngineSoundType
	{
		BOAT_ENGINE_SOUND_TYPE_ENGINE_1,
		BOAT_ENGINE_SOUND_TYPE_ENGINE_2,
		BOAT_ENGINE_SOUND_TYPE_LOW_RESO,
		BOAT_ENGINE_SOUND_TYPE_RESO,
		BOAT_ENGINE_SOUND_TYPE_MAX,
	};
#endif


// Private methods
private:
	virtual u32 GetEngineSubmixSynth() const;
	virtual u32 GetEngineSubmixSynthPreset() const;
	virtual u32 GetExhaustSubmixSynth() const;
	virtual u32 GetExhaustSubmixSynthPreset() const;
	virtual u32 GetEngineSubmixVoice() const;
	virtual u32 GetExhaustSubmixVoice() const;

	virtual void OnFocusVehicleChanged();
	virtual f32 GetIdleHullSlapVolumeLinear(f32 speed) const;
	virtual u32 GetVehicleHornSoundHash(bool ignoreMods = false);
	virtual u32 GetNormalPattern() const	 { return ATSTRINGHASH("BOATS_HP_NORMAL", 0xAF6FB5BB); }
	virtual u32 GetAggressivePattern() const { return GetNormalPattern(); }
	virtual u32 GetHeldDownPattern() const	 { return GetNormalPattern(); }

	audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);

	bool AcquireWaveSlot();
	void SetupVolumeCones();
	bool IsUsingGranularEngine();	
	void ConvertFromDummy();
	void ConvertToSuperDummy();
	void ConvertToDisabled();
	void PopulateBoatAudioVariables(audVehicleVariables* state);
	void ShutdownPrivate();
	void StopEngineSounds(bool convertedToDummy = false);

#if NON_GRANULAR_BOATS_ENABLED
	void StartEngineSounds();
	void UpdateEngine(audVehicleVariables& vehicleVariables, const u32 timeInMs);
#endif

	void UpdateEngineGranular(audVehicleVariables& vehicleVariables, const u32 timeInMs);
	void UpdateNonEngine(audVehicleVariables& vehicleVariables, const u32 timeInMs);

public:
	static audCurve sm_BoatAirTimeToRevsMultiplier;	
	static audCurve sm_BoatAirTimeToPitch;

// Private attributes
private:
	audVolumeConeLite m_EngineVolumeCone, m_ExhaustVolumeCone;
	audVehicleEngine m_VehicleEngine;
	audSimpleSmootherDiscrete m_RevsSmoother;
	audSmoother m_ThrottleSmoother;
	audSimpleSmootherDiscrete m_BankSpraySmoother;
	audSimpleSmootherDiscrete m_RollOffSmoother;	
	audCurve m_VehicleSpeedToHullSlapVol;	
	audCurve m_WaterTurbulenceVolumeCurve;
	audCurve m_WaterTurbulencePitchCurve;

#if NON_GRANULAR_BOATS_ENABLED
	audCurve m_EngineCurves[BOAT_ENGINE_SOUND_TYPE_MAX];
	audCurve m_EnginePitchCurves[BOAT_ENGINE_SOUND_TYPE_MAX];
	audSound *m_EngineSounds[BOAT_ENGINE_SOUND_TYPE_MAX];
#endif 
	
	audSound *m_BankSpray;	
	BoatAudioSettings *m_BoatAudioSettings;
	GranularEngineAudioSettings* m_GranularEngineSettings;

	audSmoother m_FakeRevsSmoother;
	f32 m_FakeRevsHoldTime;
	f32 m_FakeOutOfWaterTimer;
	
	f32 m_SpeedEvo, m_KeelForce;	
	u32 m_ManufacturerHash, m_ModelHash, m_CategoryHash;
	u32	m_ScannerVehicleSettingsHash;
	f32 m_OutOfWaterRevsMultiplier;
	f32 m_WaterTurbulenceOutOfWaterTimer;
	f32 m_WaterTurbulenceInWaterTimer;			
	u32 m_EngineStartTime;
	bool m_WasInWaterLastFrame : 1;
	bool m_EngineOn : 1;
	bool m_WasHighFakeRevs : 1;
	bool m_FakeOutOfWater : 1;
	
	static audCurve sm_BoatAirTimeToVolume;	
	static audCurve sm_BoatAirTimeToResonance;
};

#endif // AUD_BOATAUDIOENTITY_H
