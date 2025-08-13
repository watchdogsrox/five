//
// audio/dynamicmixer.h
//
// Copyright (C) 1999-2010 Rockstar Games. All rights reserved.
//

#ifndef AUD_DYNAMICMIXMODULES_H
#define AUD_DYNAMICMIXMODULES_H

#include "atl/array.h"

class audScene;
class audDynMixModule;
class audVehicleCollisionAudio;

namespace rage
{
	class audEntity;
}

class audSceneVariableMixModule
{
public:
	static void Init(audDynMixModule * module);
	static void Update(audDynMixModule * module);
	static void Shutdown(audDynMixModule * module);
};

class audSceneTransitionMixModule
{
public:
	static void Init(audDynMixModule * module);
	static void Update(audDynMixModule * module);
	static void Shutdown(audDynMixModule * module);
};

class audVehicleCollisionMixModule
{
public:
	static void Init(audDynMixModule * module);
	static void Update(audDynMixModule * module);
	static void Shutdown(audDynMixModule * module);
	static void Process(audDynMixModule * module, audVehicleCollisionAudio * collisionAudio);
};


class audDynamicMixModuleInterface
{
public:

	audDynamicMixModuleInterface();

	static const int k_max_mods = 8;

	void AddVehicleCollisionModule(audDynMixModule * module);
	void RemoveVehicleCollisionModule(audDynMixModule * module);
	void ProcessVehiclePostCollision(audVehicleCollisionAudio * collisionAudio);

	void AddGeneralModule(audDynMixModule * module);
	void RemoveGeneralModule(audDynMixModule * module);
	void ProcessVehiclePostCollision(audEntity * audioEntity);

private:

	atRangeArray<audDynMixModule*, k_max_mods> m_VehiclePostCollisionMods;
	atRangeArray<audDynMixModule*, k_max_mods> m_GeneralCodeMods;


};


#endif