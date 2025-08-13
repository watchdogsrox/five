// Title	:	Gangs.h
// Author	:	Graeme Williamson
// Started	:	10/11/00

#ifndef _GANGS_H_
#define _GANGS_H_

#include "basetypes.h"
#include "peds/PlayerInfo.h"
#include "Peds\PedType.h"
//#include "zones.h"


enum {	GANG_ALBANIAN = 0,
		GANG_BIKER_1,
		GANG_BIKER_2,
		GANG_ITALIAN,
		GANG_RUSSIAN,
		GANG_RUSSIAN_2,
		GANG_IRISH,
		GANG_JAMAICAN,
		GANG_AFRICAN_AMERICAN,
		GANG_KOREAN,
		GANG_CHINESE_JAPANESE,
		GANG_PUERTO_RICAN,
		NUM_OF_GANGS
};


extern u8 gaGangColoursR[NUM_OF_GANGS];
extern u8 gaGangColoursG[NUM_OF_GANGS];
extern u8 gaGangColoursB[NUM_OF_GANGS];

class CGangInfo
{
public:

	u32 PickRandomWeapon( const bool bForceDrivebyWeapon = false ) const;

	s8 PedModelOverride;	//	-1 = use both models, 0 = use first gang ped model, 1 = use second gang ped model

	enum { MELEE_WEAPON = 0, SIDEARM_WEAPON, BIG_WEAPON, MAX_WEAPONS };
	u32		m_aWeaponTypes[MAX_WEAPONS];
	u8		m_aWeaponPercs[MAX_WEAPONS];
	u32		m_nDefaultWeaponType;

	CGangInfo();
	~CGangInfo();
};

class CGangs
{
public:
	static CGangInfo Gang[NUM_OF_GANGS];
	
//	static void SetGangVehicleModel(s16 GangIndex, s32 VehModIndx);
//	static void SetGangPedModels(s16 GangIndex, s32 ped1, s32 ped2);
	static void SetGangWeapons(s32 GangIndex, u32 FirstWeap, u8 firstWeaponPerc, u32 SecondWeap, u8 secondWeaponPerc, u32 ThirdWeap, u8 thirdWeaponPerc);
	
	static void SetGangPedModelOverride(s32 GangIndex, s8 PedModOverride);
	static u32 ChooseGangPedModel(s32 GangIndex);
	static CGangInfo& GetGangInfo(ePedType pedType);
};

#endif

