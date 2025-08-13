// 
// audio/fireaudioentity.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_FIREAUDIOENTITY_H
#define AUD_FIREAUDIOENTITY_H

#include "audio_channel.h"
#include "audioengine/widgets.h"
#include "audioengine/soundset.h"
#include "audiosoundtypes/soundcontrol.h"
#include "scene/RegdRefTypes.h"
#include "audioengine/engine.h"

#include "audioentity.h"
#include "system/criticalsection.h"

class CFire;
class CPed;

enum audFireState
{
	// timed fires
	FIRESTATE_STOPPED = 0,
	FIRESTATE_PLAYING,
	FIRESTATE_START,
	FIRESTATE_RESTART
};

class audFireAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audFireAudioEntity);

	static void InitClass();
	audFireAudioEntity();

	virtual void Init(){naAssertf(0, "audFireAudioEntity must be initialised with either a CFire or CVehicle");}
	void Init(const CFire *fire);
	void DeferredInit();
	bool EntityInitialized()					{ return m_EntityInitialized; } 
	virtual void Shutdown();

	void Update(const CFire *fire, const f32 timeStep);
	void PlayFireSound(const CFire *fire);
	u32 GetLastUpdateTime()						{ return m_LastUpdateTime; }
	Vec3V_Out GetPositionWorld() const			{ return m_Position; }
	f32 GetLifeRatio()							{ return m_LifeRatio; }

	void SetLifeSpan(const u32 lifeSpan)		{ m_FireLifeSpan = lifeSpan; }
	void SetStartTime(const u32 startTime)		{ m_FireStartTime = startTime; }
	u32 GetLifeSpan()							{ return m_FireLifeSpan; }
	u32 GetStartTime()							{ return m_FireStartTime; }
	
	void SetDistanceFromListener(f32 distance)	{	m_DistanceFromListener = distance;
													m_LastDistanceListenerUpdateTime = g_AudioEngine.GetTimeInMilliseconds(); 
												}
	f32 GetDistanceFromListener()				{ return m_DistanceFromListener; }
	u32 GetLastDistanceListenerUpdateTime()		{ return m_LastDistanceListenerUpdateTime; }

	void SetState(const audFireState state)		{ m_State = state; }
	audFireState GetState() const				{ return m_State; }

	virtual void UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs);
	virtual void PreUpdateService(u32 timeInMs);

	//PURPOSE
	//Supports audDynamicEntitySound queries
	u32 QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs);
	BANK_ONLY(static void AddWidgets(bkBank &bank););
private:

	void UpdateFire(const CFire *fire, const f32 velocityScalar, const Vector3 &pos);

	RegdPed m_Ped;
	static audSoundSet sm_FireSounds;
	audFluctuatorProbBased m_WindFluctuator;
	audSound *m_MainFireLoop, *m_CloseFireLoop, *m_WindFireLoop;
	static f32 sm_StartFireThreshold;
	u32 m_LastUpdateTime;
	f32 m_LifeRatio;
	audFireState m_State;

	bool m_EntityInitialized;

	u32 m_FireStartTime;
	u32 m_FireLifeSpan;

	Vec3V m_Position;

	f32 m_DistanceFromListener;
	u32 m_LastDistanceListenerUpdateTime;

	static u32 sm_LastFireStartTime;

};

class audFireSoundManager // simple class for managing fire sounds so we can limit the numbers
{
public:

	audFireSoundManager();
	void InitClass();
	void ShutdownClass();
	void Update();

	s32 GetCount()	{ return m_FireAudioEntity.GetCount(); }

	void AddFire(audFireAudioEntity *fireAudioEntity);
	void RemoveFire(s32 i);

	atArray<audFireAudioEntity*> m_FireAudioEntity;
	u32 m_LastSortTime;

private:
	static sysCriticalSectionToken sm_CritSec;
};


extern audFireSoundManager g_FireSoundManager;

#endif // AUD_FIREAUDIOENTITY_H
