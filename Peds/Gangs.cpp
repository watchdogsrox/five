// Title	:	Gangs.cpp
// Author	:	Graeme Williamson
// Started	:	10/11/00

#include "Peds\gangs.h"

// Rage header
#include "audioengine/widgets.h"

// Framework headers
#include "fwmaths\random.h"

// Game headers
#include "Game\modelindices.h"
#include "Streaming\streaming.h"
#include "Streaming\populationstreaming.h"
#include "Task\System\Task.h"
#include "Weapons\Weapon.h"
#include "Weapons\WeaponTypes.h"

AI_OPTIMISATIONS()

CGangInfo CGangs::Gang[NUM_OF_GANGS];


u8 gaGangColoursR[NUM_OF_GANGS] = { 200,  70, 255,   0, 255, 200, 240,   0, 255,   0,   0,   0 };
u8 gaGangColoursG[NUM_OF_GANGS] = {   0, 200, 200,   0, 220, 200, 140, 200,   0,   0, 255, 255 };
u8 gaGangColoursB[NUM_OF_GANGS] = { 200,   0,   0, 200, 190, 200, 240, 255,   0, 255, 255,   0 };


// Name			:	CGangInfo::CGangInfo
// Purpose		:	constructor
// Parameters	:	None
// Returns		:	Nothing
CGangInfo::CGangInfo()
{
	PedModelOverride = -1;	//	Use both gang ped models

	for( s32 i = 0; i < MAX_WEAPONS; i++ )
	{
		m_aWeaponTypes[i] = 0;
		m_aWeaponPercs[i] = 0;
	}
}


// Name			:	CGangInfo::~CGangInfo
// Purpose		:	destructor
// Parameters	:	None
// Returns		:	Nothing
CGangInfo::~CGangInfo()
{

}

u32 CGangInfo::PickRandomWeapon( const bool UNUSED_PARAM(bForceDrivebyWeapon) ) const
{
	// CS - Removed as using a hard coded WeaponType
	u8 iRandom = (u8) fwRandom::GetRandomNumberInRange(0, 300); //Temporarily put it down to 1/3
// 	if( bForceDrivebyWeapon && iRandom < 100 )
// 	{
// 		return WEAPONTYPE_PISTOL;
// 	}

	//@@: location CGANGINFO_PICKRANDOMWEAPON
	s32 iMaxWeapon = MAX_WEAPONS;
	if( CWanted::MaximumWantedLevel <= WANTED_LEVEL2 )
	{
		iMaxWeapon = MELEE_WEAPON;
	}
	else if( CWanted::MaximumWantedLevel <= WANTED_LEVEL4 )
	{
		iMaxWeapon = SIDEARM_WEAPON;
	}
	else  if( CWanted::MaximumWantedLevel <= WANTED_LEVEL5 )
	{
		iMaxWeapon = BIG_WEAPON;
	}

	u8 iRunningTotal = 0;
	for( s32 i = 0; i < MAX_WEAPONS; i++ )
	{
		iRunningTotal = iRunningTotal + m_aWeaponPercs[i];
		if( iRandom < iRunningTotal && i <= iMaxWeapon )
			return m_aWeaponTypes[i];
	}

	return m_nDefaultWeaponType;
}

void CGangs::SetGangWeapons(s32 GangIndex, u32 FirstWeap, u8 firstWeaponPerc, u32 SecondWeap, u8 secondWeaponPerc, u32 ThirdWeap, u8 thirdWeaponPerc)
{
	CGangInfo *pGang;
	
	Assertf((GangIndex >= 0) && (GangIndex < NUM_OF_GANGS), "SetGangWeapons - GangIndex must be between 0 and 11");
	
	pGang = &(Gang[GangIndex]);

	pGang->m_aWeaponTypes[0] = FirstWeap;
	pGang->m_aWeaponPercs[0] = firstWeaponPerc;
	pGang->m_aWeaponTypes[1] = SecondWeap;
	pGang->m_aWeaponPercs[1] = secondWeaponPerc;
	pGang->m_aWeaponTypes[2] = ThirdWeap;
	pGang->m_aWeaponPercs[2] = thirdWeaponPerc;
}

void CGangs::SetGangPedModelOverride(s32 GangIndex, s8 PedModOverride)
{
	CGangInfo *pGang;
	
	Assertf((GangIndex >= 0) && (GangIndex < NUM_OF_GANGS), "SetGangPedModelOverride - GangIndex must be between 0 and 11");
	Assertf((PedModOverride >= -1) && (PedModOverride <= 1), "SetGangPedModelOverride - Ped model override must be -1, 0 or 1");
	
	pGang = &(Gang[GangIndex]);

	pGang->PedModelOverride = PedModOverride;
}

u32 CGangs::ChooseGangPedModel(s32 UNUSED_PARAM(GangIndex))
{
	return fwModelId::MI_INVALID;
//	return gPopStreaming.GetFallbackPedModelIndex();
/* rage
	CGangInfo *pGang;
	s32 override;
	
	Assertf((GangIndex >= 0) && (GangIndex < 10), "GetGangPedModelOverride - GangIndex must be between 0 and 9");
	
	pGang = &(Gang[GangIndex]);
	override = pGang->PedModelOverride;
	
	Assertf((override > -2) && (override < 2), " CGangs::ChooseGangPedModel - Unexpected override value");
	
	if (override == -1)
	{
		// The idea is to go through all of the possible models in this ped until we find one that is loaded in.
		CCarCtrl::InitSequence(CPopulation::GetNumPedsInGroup(POPCYCLE_TYPEGROUPS+GangIndex, 0));

		for (s32 i = 0; i < CPopulation::GetNumPedsInGroup(POPCYCLE_TYPEGROUPS+GangIndex, 0); i++)
		{
			s32	ModelInArray = CCarCtrl::FindSequenceElement(i);
			s32	ModelIndex = CPopulation::GetPedGroupPed(POPCYCLE_TYPEGROUPS+GangIndex, ModelInArray, CPopulation::CurrentWorldZone);
			if (CStreaming::HasModelLoaded(ModelIndex)) // && !((CVehicleModelInfo *)CModelInfo::GetModelInfo (ModelIndex))->GetFlag(VEHICLE_MODINFO_FLAGS_TRY_TO_STREAM_OUT))
			{
				return (CPopulation::GetPedGroupPed(POPCYCLE_TYPEGROUPS+GangIndex, ModelInArray, CPopulation::CurrentWorldZone));
			}
		}
        return -1;		// If nothing is streamed in we return -1.
    }
    else
    {
        return CPopulation::GetPedGroupPed(POPCYCLE_TYPEGROUPS+GangIndex, 0, CPopulation::CurrentWorldZone);
    }
*/
}


CGangInfo& CGangs::GetGangInfo(ePedType pedType)
{
	Assert(pedType >= PEDTYPE_GANG_ALBANIAN);
	Assert(pedType <= PEDTYPE_GANG_PUERTO_RICAN);
	
	return CGangs::Gang[pedType - PEDTYPE_GANG_ALBANIAN];
}




