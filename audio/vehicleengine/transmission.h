// 
// audio/vehicletransmissionaudio.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLE_TRANSMISSION_H
#define AUD_VEHICLE_TRANSMISSION_H

#include "audioengine/curve.h"
#include "audioengine/soundset.h"
#include "audioengine/widgets.h"
#include "audio/vehicleaudioentity.h"

class audCarAudioEntity;

namespace rage
{
	class audSound;
	struct VehicleEngineAudioSettings;
}
// ----------------------------------------------------------
// Class for modelling gear transmission sounds
// ----------------------------------------------------------
class audVehicleTransmission
{
public:
	explicit audVehicleTransmission();
	virtual ~audVehicleTransmission() {};

	static void InitClass();

	void Init(audCarAudioEntity* parent);
	void Update(audVehicleVariables* state);
	void Stop();

	inline u32 GetLastGearChangeTimeMs() const { return m_LastGearChangeTimeMs; }

private:
	void UpdateGearChanges(audVehicleVariables *state);

private:
	static audCurve sm_GearTransPitchCurve;

	static audSoundSet sm_ExtrasSoundSet;
	static audSoundSet sm_DamageSoundSet;

	audCarAudioEntity *				m_Parent;
	VehicleEngineAudioSettings *	m_VehicleEngineAudioSettings;
	audSound *						m_GearTransLoop;
	u32								m_LastGearChangeTimeMs;
	u32								m_EngineLastGear;
	u32								m_LastGearChangeGear;
	u32								m_LastGearForWobble;
	f32								m_ThrottleLastFrame;
	bool							m_GearChangePending;
};

#endif // AUD_VEHICLE_TRANSMISSION_H

