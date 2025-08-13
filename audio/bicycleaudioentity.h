//  
// audio/bicycleaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_BICYCLEAUDIOENTITY_H
#define AUD_BICYCLEAUDIOENTITY_H

#include "vehicleaudioentity.h"

class CWheel;

// ----------------------------------------------------------------
// audBicycleAudioEntity class
// ----------------------------------------------------------------
class audBicycleAudioEntity : public audVehicleAudioEntity
{
// Public methods
public:
	audBicycleAudioEntity();
	virtual ~audBicycleAudioEntity() {};

	AUDIO_ENTITY_NAME(audBicycleAudioEntity);

	virtual void Reset();
	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspSettings);
	virtual void StopHorn(bool checkForCarModShop = false, bool onlyStopIfPlayer = false);
	virtual bool InitVehicleSpecific();
	virtual u32 GetSoundFromObjectData(u32 fieldHash) const;

	// Overridden virtual methods from VehicleAudioEntity
	virtual f32 GetMinOpenness() const {return 1.f;};
	virtual u32 GetJumpLandingSound(bool interiorView) const;
	virtual u32 GetDamagedJumpLandingSound(bool interiorView) const;
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;
	virtual audMetadataRef GetShallowWaterWheelSound() const;

	static void InitClass();

#if __BANK
	static void AddWidgets(bkBank &bank);
	void UpdateDebug();
#endif

	BicycleAudioSettings* GetBicycleAudioSettings();

// Protected methods
protected:
	virtual f32 GetRoadNoiseVolumeScale() const;
	virtual f32 GetSkidVolumeScale() const;

// Private methods
private:
	void UpdateDriveTrainSounds();
	void UpdateSuspension();
	void UpdateBrakeSounds();
	void UpdateWheelSounds();

	CWheel* GetDriveWheel() const;
	bool AcquireWaveSlot();
	audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);
	void ConvertToDisabled();

	virtual u32 GetVehicleHornSoundHash(bool ignoreMods = false);
	virtual u32 GetNormalPattern() const	 { return ATSTRINGHASH("BICYCLE_HP_NORMAL", 0xFAD9E424); }
	virtual u32 GetAggressivePattern() const { return GetNormalPattern(); }
	virtual u32 GetHeldDownPattern() const	 { return GetNormalPattern(); }

private:
	BicycleAudioSettings* m_BicycleAudioSettings;

	audSound* m_ChainSound;
	audSound* m_SprocketSound;
	audSound* m_TyreGritSound;

	f32	m_LastCompressionChange[NUM_VEH_CWHEELS_MAX];
	f32 m_InvPedalRadius;
	audSound *m_SuspensionSounds[2];

	u32 m_PrevTyreGritSound;
	bool m_WasSprinting;
	bool m_BrakeBlockPlayed;

	static audCurve sm_SuspensionVolCurve;
	static audCurve sm_GritLeanAngleToLinVolCurve;
	static audCurve sm_GritSpeedToLinVolCurve;
};

#endif // AUD_BICYCLEAUDIOENTITY_H





















