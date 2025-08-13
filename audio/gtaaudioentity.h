#if 0 //Removing file

// 
// audio/gtaaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GTAAUDIOENTITY_H
#define AUD_GTAAUDIOENTITY_H

#include "audioengine/entity.h"
#include "audioengine/widgets.h"
class CEntity;

class audGtaAudioEntity : public audEntity
{
public:
	AUDIO_ENTITY_NAME(audGtaAudioEntity);

	audGtaAudioEntity(){};
	virtual ~audGtaAudioEntity(){};
	virtual CEntity* GetOwningEntity(){return NULL;};

private:
};

extern audStringHash g_UtilEnvelopeHash;
extern audStringHash g_BaseCategoryHash;

namespace northAudio
{
	extern audStringHash g_Boat;
	extern audStringHash g_Vehicle;
	extern audStringHash g_Driver;
	extern audStringHash g_UpperClothing;
	extern audStringHash g_LowerClothing;
	extern audStringHash g_Weapon;
	extern audStringHash g_Car;
	extern audStringHash g_Heli;
	extern audStringHash g_Motorbike;
	extern audStringHash g_Ped;
	extern audStringHash g_EntityBulletImpactMaterial;
	extern audStringHash g_GroundBulletImpactMaterial;
	extern audStringHash g_EntityBehindBulletImpactMaterial;
}
#endif // AUD_GTAAUDIOENTITY_H

#endif