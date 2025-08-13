// 
// audio/gunfireaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GUNFIREAUDIOENTITY_H
#define AUD_GUNFIREAUDIOENTITY_H

#include "atl/array.h"
#include "audio_channel.h"
#include "audio/environment/environment.h"
#include "audioengine\tracker.h"
#include "scene/RegdRefTypes.h"
#include "audioengine/soundset.h"

#include "audioentity.h"

#define USE_GUN_TAILS 0
#define AUD_MAX_NUM_ENERGY_DELAY_EVENTS 4

class CEntity;
class CObject;
namespace rage
{
	class bkBank;
	class audSound;
}
enum audGunFightGridSectors
{
	AUD_SECTOR_TL = 0,
	AUD_SECTOR_TM,
	AUD_SECTOR_TR, 
	AUD_SECTOR_L,
	AUD_SECTOR_M,
	AUD_SECTOR_R,
	AUD_SECTOR_BL,
	AUD_SECTOR_BM,
	AUD_SECTOR_BR,
};
struct gunFireSectorsCoord{
	Vector3 xCoord,yCoord;
};
struct energyDelayEvent{
	bool hasToTriggerEvent;
	f32 tailEnergy,evenTime;
};
class audGunFireAudioEntity;
class audGunFireAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audGunFireAudioEntity);

	// Init 
	virtual void Init();
	// Update 
	virtual void PreUpdateService(u32 timeInMs);
	void 	     Update(u32 timeInMs);
	void 		 PostGameUpdate(u32 timeInMs);
	// ShutDown 
	virtual void ShutDown();
	// Gun Tails functions.
	void		 AddGunTailsDelayEvents(u32 timeInMs);
	void		 TriggerGunTailsDelayEvents(u32 timeInMs);
	void		 ComputeGunTailsVolume();
	void 		 AddGunFireEvent(const Vector3 &shooterPos, f32 weaponTailEnergy);
	void 		 SpikeEnergy(s32 sector, u32 gunFightsector,f32 weaponTailEnergy, f32 distanceToPlayer);
	void 		 SpikeNearByZones(s32 sector,f32 energy,u32 timeInMs);

#if __BANK
	void 		 InitGunFireGrid();
	gunFireSectorsCoord GetGunFireSectorsCoords(int sector) const { return m_GunFireSectorsCoords[sector]; }
	static void  AddWidgets(bkBank &bank);
#endif // __BANK
private:
	// Gun tails curves
	atRangeArray<audSimpleSmoother,4>													m_EnergyFlowsSmoother;
	audCurve 																			m_AmountOfTailEnergyAffects;
	audCurve 																			m_DistanceDumpFactor;
	audCurve 																			m_EnergyDecay;
	audCurve 																			m_EnergyToEnergyDecay;
	audCurve 																			m_BuiltUpFactorToEnergy;
	audCurve 																			m_BuiltUpFactorToSourceEnergy;
	audCurve 																			m_EnergyToVolume;

	static audSoundSet																	sm_TailSounds; // Gun tails sound set
	atRangeArray<audSound*,4>															m_TailSounds;  // Reference to the sound 
	atRangeArray<f32,4>																	m_TailVolume;  // Volume of each sector.
	atRangeArray<f32,4>																	m_OverallTailEnergy;  // Energy of each speaker
	atRangeArray<f32,AUD_NUM_SECTORS_ACROSS>											m_TimeToSpikeNearbyZones;
	atRangeArray<f32,AUD_NUM_SECTORS>													m_OldTailEnergy; // To control the delay spike when a gun shot is triggered.
	atRangeArray<f32,AUD_NUM_SECTORS>													m_TailEnergy; // To control the delay spike when a gun shot is triggered.
	atMultiRangeArray<energyDelayEvent,AUD_NUM_SECTORS,AUD_MAX_NUM_ENERGY_DELAY_EVENTS> m_EnergyDelayEvents; // To control the delay spike when a gun shot is triggered.
	u32					 																m_NumberOfSquaresInOverallGridSquare;
#if __BANK
	atRangeArray<gunFireSectorsCoord,AUD_NUM_SECTORS_ACROSS+1>	  						m_GunFireSectorsCoords;// To store the origin of the grid.
	atRangeArray<Vector3,AUD_NUM_SECTORS>	  											m_OverallGridCenters; // To control the delay spike when a gun shot is triggered.
public:
	static bool																			g_DrawOverallGrid ;
	static bool																			g_DrawGunFightGrid;
	static u32 																			g_Definition;
#endif 
};
#endif // AUD_GUNFIREAUDIOENTITY_H
