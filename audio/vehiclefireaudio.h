// 
// audio/vehiclefireaudioentity.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLEFIREAUDIO_H
#define AUD_VEHICLEFIREAUDIO_H

#include "vehicleaudioentity.h"

#include "audio_channel.h"
#include "audioengine/widgets.h"
#include "audioengine/soundset.h"
#include "audiosoundtypes/soundcontrol.h"
#include "scene/RegdRefTypes.h"

class CFire;
class CVehicle;

enum audVehicleFires
{
	audInvalidFire = -1,
	audEngineFire = 0,
	audTankFire, // 2 of them
	audTyreFire, // 4 of them
	audGenericFire,
	audVehWreckedFire,
	audMaxVehFires = 9,
};
struct audFireInfo
{
	CFire* fire;
	audSound* fireSound;
	audVehicleFires fireType;
	bool soundStarted;
};
class audVehicleFireAudio
{
public:

	static void InitClass();
	audVehicleFireAudio();

	void Init(audVehicleAudioEntity *pParent);
	void Update();
	void UpdateEngineFire(f32 fireEvo, u32 lpfCutoff);
	void StopAllSounds();

	void UnregisterVehFire(CFire* fire);
	void RegisterVehTankFire(CFire* fire);
	void RegisterVehTyreFire(CFire* fire);
	void RegisterVehWreckedFire(CFire* fire);
	void RegisterVehGenericFire(CFire* fire);

	BANK_ONLY(static void AddWidgets(bkBank &bank););
private:

	void RegisterVehFire(CFire* fire,audVehicleFires firetype);
	void StartVehFire(s32 index);

	atRangeArray<audFireInfo,audMaxVehFires> m_VehicleFires;
	audVehicleAudioEntity * m_Parent;
	audSoundSet m_VehicleFireSounds;

	u32 m_NumberOfCurrentFires;

	static audCurve sm_VehBurnStrengthToMainVol;
	static f32 sm_StartVehFireThreshold;
};


#endif // AUD_VEHICLEFIREAUDIO_H
