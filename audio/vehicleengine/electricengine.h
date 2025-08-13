// 
// audio/electricengine.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLE_ELECTRIC_ENGINE_H
#define AUD_VEHICLE_ELECTRIC_ENGINE_H

#include "audioengine/curve.h"
#include "audioengine/soundset.h"
#include "audioengine/widgets.h"
#include "audio/vehicleaudioentity.h"

class audCarAudioEntity;

namespace rage
{
	class audSound;
	struct ElectricEngineAudioSettings;
}
// ----------------------------------------------------------
// Class for modelling electric engine sounds
// ----------------------------------------------------------
class audVehicleElectricEngine
{
public:
	explicit audVehicleElectricEngine();
	virtual ~audVehicleElectricEngine() {};

	static void InitClass();
	void Init(audCarAudioEntity* parent);
	void Update(audVehicleVariables* state);
	void Stop();

	inline void SetVolumeScale(f32 volumeScale) { m_VolumeScale = volumeScale; }

private:
	void UpdateSpeedLoop(audVehicleVariables* state);
	void UpdateBoostLoop(audVehicleVariables* state);
	void UpdateRevsOffLoop(audVehicleVariables* state);

private:
	audSimpleSmootherDiscrete m_BoostLoopApplySmoother;
	audSimpleSmootherDiscrete m_RevsOffLoopVolSmoother;
	audSimpleSmootherDiscrete m_SpeedLoopSpeedVolSmoother;
	audSimpleSmootherDiscrete m_SpeedLoopThrottleVolSmoother;
	audCurve m_SpeedLoopPitchCurve;

	audCarAudioEntity* m_Parent;
	ElectricEngineAudioSettings* m_ElectricEngineAudioSettings;
	audSound* m_SpeedLoop;
	audSound* m_BoostLoop;
	audSound* m_RevsOffLoop;

	f32 m_VolumeScale;

	static audCurve sm_RevsToBoostCurve;
	static audCurve sm_BoostToVolCurve;
};

#endif // AUD_VEHICLE_ELECTRIC_ENGINE_H

