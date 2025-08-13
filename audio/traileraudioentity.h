//  
// audio/traileraudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_TRAILERAUDIOENTITY_H
#define AUD_TRAILERAUDIOENTITY_H

#include "vehicleaudioentity.h"

// ----------------------------------------------------------------
// audTrailerAudioEntity class
// ----------------------------------------------------------------
class audTrailerAudioEntity : public audVehicleAudioEntity
{
// Public methods
public:
	audTrailerAudioEntity();
	virtual ~audTrailerAudioEntity() {};

	AUDIO_ENTITY_NAME(audTrailerAudioEntity);

	static void InitClass();
	virtual void Reset();
	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& dspSettings);
	virtual bool InitVehicleSpecific();
	virtual u32 GetSoundFromObjectData(u32 fieldHash) const;
	TrailerAudioSettings* GetTrailerAudioSettings();
	audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);
	void TriggerTrailerBump(f32 volume);

	audMetadataRef GetClatterSound() const;
	audMetadataRef GetChassisStressSound() const;
	ModelAudioCollisionSettings* GetModelCollisionSettings();
	
	virtual u32 GetTurretSoundSet() const;

	f32 GetChassisStressSensitivityScalar() const;
	f32 GetClatterSensitivityScalar() const;
	f32 GetChassisStressVolumeBoost() const; 
	f32 GetClatterVolumeBoost() const;

#if __BANK
	void UpdateDebug();
	static void AddWidgets(bkBank &bank);
#endif

private:
	TrailerAudioSettings* m_TrailerAudioSettings;
	audSimpleSmoother m_LinkAngleChangeRateSmoother;
	audSimpleSmootherDiscrete m_LinkStressVolSmoother;
	audSound* m_LinkStressSound;
	f32 m_LinkAngle;
	f32 m_LinkAngleLastFrame;
	f32 m_LinkAngleChangeRate;
	f32 m_LinkAngleChangeRateChangeRate;
	f32 m_LinkAngleChangeRateLastFrame;
	bool m_IsParentVehiclePlayer;
	bool m_IsAttached;
	bool m_FirstUpdate;

	static audCurve sm_TrailerAngleToVolumeCurve;
};

#endif // AUD_TRAILERAUDIOENTITY_H





















