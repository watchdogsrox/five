// 
// audio/radioemitter.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOEMITTER_H
#define AUD_RADIOEMITTER_H

#include "audiodefines.h"

#if NA_RADIO_ENABLED
#include "radiostation.h"
#include "gameobjects.h"
#include "streamslot.h"
#include "audiohardware/driverdefs.h"
#include "vector/vector3.h"

class naEnvironmentGroup;
class CEntity;

class audRadioEmitter
{
public:
	enum RadioEmitterType
	{
		ENTITY_RADIO_EMITTER,
		STATIC_RADIO_EMITTER,
		AMBIENT_RADIO_EMITTER,
		RADIO_EMITTER_TYPE_MAX,
	};

public:
	virtual ~audRadioEmitter(){}
	virtual void GetPosition(Vector3 &position) const = 0;
	virtual f32 GetEmittedVolume(void) const = 0;

	virtual naEnvironmentGroup *GetOcclusionGroup() = 0;
	virtual void SetOcclusionGroup(naEnvironmentGroup *occlusionGroup) = 0;
	virtual bool Equals(void *emitter) = 0;
	virtual bool IsReplaceable() = 0;
	virtual bool ShouldPlayFullRadio() = 0;
	virtual u32 GetPriority() const = 0;
	virtual void SetRadioStation(const audRadioStation *station) = 0;
	BANK_ONLY(virtual const char* GetName() const = 0;)
	virtual RadioEmitterType GetEmitterType() = 0;
	
	virtual bool IsPlayerVehicle()
	{
		return false;
	}

	virtual bool IsClubEmitter() const 
	{ 
		return false;  
	}

	virtual bool IsLastPlayerVehicle()
	{
		return false;
	}

	virtual bool ShouldMuteForFrontendRadio() const { return true; }

	virtual u32 GetLPFCutoff() const
	{
		return kVoiceFilterLPFMaxCutoff;
	}
	virtual u32 GetHPFCutoff() const
	{
		return 0;
	}

	virtual f32 GetRolloffFactor() const
	{
		return 1.f;
	}

	virtual f32 GetEnvironmentalLoudness() const
	{
		return 0.f;
	}

	virtual void NotifyTrackChanged() const { }
};

class audEntityRadioEmitter : public audRadioEmitter
{
public:
	audEntityRadioEmitter() : m_Entity(NULL) {}
	audEntityRadioEmitter(CEntity *entity);

	void GetPosition(Vector3 &position) const;
	f32 GetEmittedVolume(void) const;
	naEnvironmentGroup *GetOcclusionGroup();
	void SetOcclusionGroup(naEnvironmentGroup *occlusionGroup);
	bool Equals(void *emitter)
	{
		return ((CEntity *)emitter == m_Entity);
	}
	bool IsReplaceable();
	bool ShouldPlayFullRadio()
	{
		return true;
	}

	void SetEmitter(CEntity *emitter)
	{
		m_Entity = emitter;
	}
	void SetRadioStation(const audRadioStation *station);

	virtual RadioEmitterType GetEmitterType() { return ENTITY_RADIO_EMITTER; }

	BANK_ONLY(virtual const char* GetName() const;)

	u32 GetPriority() const;	

	virtual u32 GetLPFCutoff() const;
	virtual u32 GetHPFCutoff() const;
	virtual float GetRolloffFactor() const;
	virtual float GetEnvironmentalLoudness() const;

	virtual bool IsPlayerVehicle();
	virtual bool IsLastPlayerVehicle();

	virtual void NotifyTrackChanged() const;

protected:
	CEntity *m_Entity;
};

class audStaticRadioEmitter : public audRadioEmitter
{
public:
	audStaticRadioEmitter() { m_LPFCutoff = kVoiceFilterLPFMaxCutoff; m_HPFCutoff = 0; m_RolloffFactor = 1.f; m_IsClubEmitter = false; m_IsHighPriorityEmitter = false;}
	
	void GetPosition(Vector3 &position) const
	{
		position = m_Position;
	}
	virtual f32 GetEmittedVolume(void) const;
	
	naEnvironmentGroup *GetOcclusionGroup()
	{
		return m_OcclusionGroup;
	}
	void SetOcclusionGroup(naEnvironmentGroup *occlusionGroup)
	{
		m_OcclusionGroup = occlusionGroup;
	}
	bool Equals(void *emitter)
	{
		return ((StaticEmitter *)emitter == m_StaticEmitter);
	}
	bool IsReplaceable()
	{
		return false;
	}
	virtual bool ShouldPlayFullRadio()
	{
		return (AUD_GET_TRISTATE_VALUE(m_StaticEmitter->Flags, FLAG_ID_STATICEMITTER_PLAYFULLRADIO)==AUD_TRISTATE_TRUE);
	}

	void SetEmitter(StaticEmitter *emitter)
	{
		m_StaticEmitter = emitter;
	}

	void SetIsClubEmitter(bool isClubEmitter)
	{
		m_IsClubEmitter = isClubEmitter;
	}

	void SetIsHighPriorityEmitter(bool isHighPriority)
	{
		m_IsHighPriorityEmitter = isHighPriority;
	}

	bool IsClubEmitter() const 
	{ 
		return m_IsClubEmitter;
	}

	bool IsHighPriorityEmitter() const 
	{ 
		return m_IsHighPriorityEmitter;
	}


	BANK_ONLY(virtual const char* GetName() const;)

	void SetRadioStation(const audRadioStation *station);

	u32 GetPriority() const;

	void SetEmittedVolume(const f32 vol)
	{
		Assertf(vol < 24.f, "Radio emitter set to high volume (%f dB) - will clamp", vol);
		m_EmittedVolume = Clamp(vol, g_SilenceVolume, 24.f);
	}

	void SetPosition(const Vector3 &pos)
	{
		m_Position = pos;
	}

	void SetLPFCutoff(const u32 cutoff)
	{
		Assign(m_LPFCutoff, cutoff);
	}

	u32 GetLPFCutoff() const
	{
		return m_LPFCutoff;
	}

	void SetHPFCutoff(const u32 hpfCutoff)
	{
		Assign(m_HPFCutoff, hpfCutoff);
	}

	virtual u32 GetHPFCutoff() const
	{
		return m_HPFCutoff;
	}

	void SetRolloffFactor(const f32 rolloff)
	{
		m_RolloffFactor = rolloff;
	}

	virtual f32 GetRolloffFactor() const
	{
		return m_RolloffFactor;
	}

	virtual bool ShouldMuteForFrontendRadio() const
	{
		return (AUD_GET_TRISTATE_VALUE(m_StaticEmitter->Flags, FLAG_ID_STATICEMITTER_MUTEFORFRONTENDRADIO) != AUD_TRISTATE_FALSE);
	}

	virtual RadioEmitterType GetEmitterType() { return STATIC_RADIO_EMITTER; }

protected:	

	Vector3 m_Position;
	StaticEmitter *m_StaticEmitter;	
	naEnvironmentGroup *m_OcclusionGroup;
	f32 m_EmittedVolume;
	bool m_IsClubEmitter;
	bool m_IsHighPriorityEmitter;
	
	u16 m_LPFCutoff;
	u16 m_HPFCutoff;
	f32 m_RolloffFactor;
	
	
};

class audAmbientRadioEmitter : public audStaticRadioEmitter
{
public:
	virtual f32 GetEmittedVolume(void) const;
	virtual bool ShouldPlayFullRadio() { return true; }
	virtual bool ShouldMuteForFrontendRadio() const {return true;}
	virtual RadioEmitterType GetEmitterType() { return AMBIENT_RADIO_EMITTER; }
	BANK_ONLY(virtual const char* GetName() const { return "Ambient Radio Emitter"; })
};
#endif // NA_RADIO_ENABLED
#endif // AUD_RADIOEMITTER_H
