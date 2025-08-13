// 
// audio/vehicleengineaudio.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLE_ENGINE_COMPONENT_H
#define AUD_VEHICLE_ENGINE_COMPONENT_H

#include "audio/gameobjects.h"
#include "audioengine/widgets.h"
#include "audio/vehicleaudioentity.h"

namespace rage
{
	class audSound;
}

class audCarAudioEntity;

// ----------------------------------------------------------
// Class for modelling engine/exhaust sounds
// ----------------------------------------------------------
class audVehicleEngineComponent
{
// Public types
public:
	enum EngineSoundType
	{
		ENGINE_SOUND_TYPE_ENGINE,
		ENGINE_SOUND_TYPE_EXHAUST,
		ENGINE_SOUND_TYPE_MAX,
	};

// Public functions
public:
	explicit audVehicleEngineComponent();
	virtual ~audVehicleEngineComponent() {};

	void Init(audCarAudioEntity* parent, EngineSoundType soundType);
	void Update(audVehicleVariables* state);
	void Stop(s32 releaseTime);
	void Start();
	bool Prepare();
	void Play();

	inline bool HasPrepared()		{ return m_HasPrepared; }
	inline bool HasPlayed()			{ return m_HasPlayed; }
	inline f32 GetLowLoopVolume()	{ return m_LowLoopVolume; }
	inline f32 GetHighLoopVolume()	{ return m_HighLoopVolume; }
	inline f32 GetIdleLoopVolume()	{ return m_IdleLoopVolume; }
	inline bool AreSoundsValid()	{ return m_LoopLow && m_LoopHigh && m_LoopIdle; }
	
	inline void SetVolumeScale(f32 scale)					{ m_VolumeScale = scale; }
	inline void SetFadeInRequired(u32 fadeInRequired) 		{ m_FadeInRequired = fadeInRequired;}

// Private attributes
private:
	audCarAudioEntity *				m_Parent;
	VehicleEngineAudioSettings *	m_VehicleEngineAudioSettings;
	EngineSoundType					m_EngineSoundType;

	audSimpleSmoother m_lowVolSmoother;
	audSound *m_LoopLow;
	audSound *m_LoopHigh;
	audSound *m_LoopIdle;

	f32 m_VolumeScale;
	f32 m_LowLoopVolume;
	f32 m_HighLoopVolume;
	f32 m_IdleLoopVolume;
	u32 m_FadeInRequired;

	bool m_HasPrepared : 1;
	bool m_HasPlayed : 1;
};


#endif // AUD_VEHICLE_ENGINE_COMPONENT_H

