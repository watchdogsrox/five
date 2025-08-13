// 
// audio/vehicleturboaudio.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLE_TURBO_H
#define AUD_VEHICLE_TURBO_H

#include "audioengine/curve.h"
#include "audioengine/widgets.h"
#include "audio/vehicleaudioentity.h"

class audCarAudioEntity;

namespace rage
{
	class audSound;
	struct VehicleEngineAudioSettings;
}
// ----------------------------------------------------------
// Class for modelling turbo sounds
// ----------------------------------------------------------
class audVehicleTurbo
{
public:
	explicit audVehicleTurbo();
	virtual ~audVehicleTurbo() {};

	static void InitClass();

	void Init(audCarAudioEntity* parent);
	void Update(audVehicleVariables *state);
	void Stop();

	u32 GetLastDumpValveExhaustPopTime() const { return m_LastDumpValveExhaustPopTime; }

private:
	static audCurve sm_RevsToTurboBoostCurve;
	static audCurve sm_TurboBoostToVolCurve;
	static audCurve sm_TurboBoostToPitchCurve;

	audCarAudioEntity *				m_Parent;
	VehicleEngineAudioSettings *	m_VehicleEngineAudioSettings;
	audSound *						m_TurboWhineLoop;
	audSound *						m_DumpValveSound;
	audSimpleSmootherDiscrete		m_TurboSmoother;

	u32						m_LastDumpValveTime;
	u32						m_LastDumpValveExhaustPopTime;
	s32						m_PrevGear;
	bool					m_HasDumpValve;
};

#endif // AUD_VEHICLE_TURBO_H

