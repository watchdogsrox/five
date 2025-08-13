// 
// audio/explosionaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_EXPLOSIONAUDIOENTITY_H
#define AUD_EXPLOSIONAUDIOENTITY_H


#include "audioentity.h"
#include "gameobjects.h"
#include "Weapons/WeaponEnums.h"
#include "scene/RegdRefTypes.h"

class CEntity;
class phGtaExplosionInst;

#define AUD_EXPL_TAIL_INSTANCE_LIMIT (2)
#define AUD_EXPL_CAR_DEBRIS_INSTANCE_LIMIT (2)
#define AUD_MAX_NUM_EXPLOSIONS_PER_FRAME (10)
#define AUD_MAX_NUM_SMOKE_GRENADE_SOUNDS (8)
#define AUD_MAX_NUM_BULLET_EXPLOSION_SOUNDS (2)
#define AUD_MAX_DEBRIS_BITS (90)
#if __ASSERT
#define AUD_FRAMES_TO_TRACK_REQ_EXPLOSIONS (30)
#define AUD_MAX_RECENT_EXPLOSIONS (40)
#endif

struct audLimitedExplosionSoundStruct
{
	audSound* Sound;
	u32 TimeTriggered;
};

struct DeferredExplosionStruct
{
	DeferredExplosionStruct() :
		explosion(NULL),
		tag(EXP_TAG_DONTCARE),
		position(ORIGIN),
		entity(NULL),
		isUnderwater(false)
		{}

	phGtaExplosionInst *explosion;
	eExplosionTag tag; 
	Vector3 position;
	RegdEnt entity; 
	bool isUnderwater;
};

struct audSmokeGrenadeSoundStruct
{
	audSound* Sound;
	Vector3 Position;
};

class audExplosionAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audExplosionAudioEntity);

	static void InitClass();
	static void Update();

	void Init();
	void PlayExplosion(phGtaExplosionInst *explosion);
	BANK_ONLY(static void AddWidgets(bkBank &bank););
	void PlayExplosion(phGtaExplosionInst *explosion, eExplosionTag tag, const Vector3 &position, CEntity *entity, bool isUnderwater);
	void ReallyPlayExplosion(eExplosionTag tag, const Vector3 &position, CEntity *entity, bool isUnderwater, fwInteriorLocation intLoc, u32 weaponHash);
	BANK_ONLY(void PlayOldExplosion(eExplosionTag tag, const Vector3 &position, CEntity *entity, bool isUnderwater));
	void RemoveExplosion(phGtaExplosionInst *explosion);
	static void StopSmokeGrenadeExplosions();

	ExplosionAudioSettings* GetExplosionAudioSettings(eExplosionTag tag);
	rage::audMetadataRef GetSuperSlowMoExplosionSoundRef(eExplosionTag tag);
	static f32 sm_DistanceThresholdToPlayExplosionDebris;
	BANK_ONLY(const char* GetExplosionName(eExplosionTag tag));
	BANK_ONLY(static bool sm_PlayOldExplosion);
	
private:
	void HandleCodeTriggeredExplosionSound(u32 soundHash, f32 distanceSq, f32 outsideVisability, audSoundInitParams* initParams);
	void PlayTailLimitedSound(u32 soundHash, f32 outsideVisability, audSoundInitParams* initParams);
	void PlayCarDebrisSound(u32 soundHash, f32 outsideVisability, audSoundInitParams* initParams);
	void PlayUnderwaterExplosion(const Vector3 &pos, CEntity *entity, audEnvironmentGroupInterface*occlusionGroup);

	f32 GetExplosionResonanceImpulse(eExplosionTag tag);
	u8 GetIndexForLimitedSoundStructUsingTime(const audLimitedExplosionSoundStruct* structArray, u32 arraySize);
	u8 GetIndexForSmokeGrenadeSoundStruct(f32 &distanceSqOfFarthestSound);
	void PlayBulletExplosion(CEntity* entity, audEnvironmentGroupInterface* occlusionGroup, const Vector3& pos, ExplosionAudioSettings* explosionSettings, bool isUnderwater);
	void PlaySmokeGrenadeExplosion(CEntity* entity, audEnvironmentGroupInterface* occlusionGroup, const Vector3& pos, ExplosionAudioSettings* explosionSettings, bool isUnderwater);
	void PlayBirdCrapExplosion(audEnvironmentGroupInterface* occlusionGroup, const Vector3& pos);

	BANK_ONLY(void PlaySoundWithDebug(u32 soundHash, const audSoundInitParams& params);)
	BANK_ONLY(void PlaySoundWithDebug(audMetadataRef soundHash, const audSoundInitParams& params);)

	static u32 sm_NumExplosionsThisFrame;
	static u32 sm_DebrisSound;
	static u32 sm_NumDebrisBitsPlayed;
	static f32 sm_DebrisVol;

	static atRangeArray<audLimitedExplosionSoundStruct, AUD_EXPL_TAIL_INSTANCE_LIMIT> sm_TailSndStructs;
	static atRangeArray<audLimitedExplosionSoundStruct, AUD_EXPL_CAR_DEBRIS_INSTANCE_LIMIT> sm_CarDebrisSndStructs;
	static atRangeArray<audLimitedExplosionSoundStruct, AUD_MAX_NUM_BULLET_EXPLOSION_SOUNDS> sm_BulletExplosionSndStructs;
	static atRangeArray<DeferredExplosionStruct, AUD_MAX_NUM_EXPLOSIONS_PER_FRAME> sm_DeferredExplosionStructs;
	static atRangeArray<audSmokeGrenadeSoundStruct, AUD_MAX_NUM_SMOKE_GRENADE_SOUNDS> sm_SmokeGrenadeSoundStructs;
	static atFixedArray<u32, AUD_MAX_DEBRIS_BITS> sm_DebrisPlayTimes;

	static naAudioEntity sm_DebrisAudioEntity;

#if __ASSERT
	static atRangeArray<u32, AUD_FRAMES_TO_TRACK_REQ_EXPLOSIONS>sm_NumExplosionsReqInFrame;
	static u32 sm_NumRecentReqExplosions;
	static u32 sm_NumExpReqWriteIndex;
#endif
};

extern audExplosionAudioEntity g_audExplosionEntity;

#endif // AUD_EXPLOSIONAUDIOENTITY_H
