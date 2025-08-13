#ifndef SAVEGAME_DATA_PLAYER_PED_H
#define SAVEGAME_DATA_PLAYER_PED_H

// Rage headers
#include "atl/array.h"
#include "atl/binmap.h"
#include "grcore/config.h"
#include "parser/macros.h"
#include "vector/vector2.h"
#include "vector/vector4.h"


// Game headers
#include "Peds/rendering/PedProps.h"
#include "SaveLoad/savegame_defines.h"

// Forward declarations
#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
class CPlayerPedSaveStructure_Migration;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

class CPlayerPedSaveStructure
{	
public:	 

	class CPlayerDecorationStruct
	{
	public:
		CPlayerDecorationStruct()  { m_Alpha=255; m_FixedFrame=0; m_SourceNameHash=0; m_Age=0.0f; m_FlipUVFlags=0x0;}
	public:
		Vector4 m_UVCoords;
		Vector2 m_Scale;
		u32		m_TxdHash;
		u32		m_TxtHash;
		u8		m_Type;
		u8		m_Zone;
		u8		m_Alpha;
		u8		m_FixedFrame;
		float   m_Age;
		u8		m_FlipUVFlags;
		u32		m_SourceNameHash;
		PAR_SIMPLE_PARSABLE
	};

	struct SPedCompPropConvData
	{
		SPedCompPropConvData() : m_hash(0), m_data(0) { }

		u32 m_hash;
		u32 m_data;

		PAR_SIMPLE_PARSABLE
	};

	u32		m_GamerIdHigh;	//	The gamer id was only used in GTA4 to check that the player hadn't changed on returning from a network game.
	u32		m_GamerIdLow;	//	If the single player restore is handled differently in GTA5 then we can remove these two u32's
	u16 	m_CarDensityForCurrentZone;
	s32	m_CollectablesPickedUp;	
	s32	m_TotalNumCollectables;
	bool 	m_DoesNotGetTired;
	bool	m_FastReload;
	bool	m_FireProof;
	u16	m_MaxHealth;
	u16	m_MaxArmour;
	bool	m_bCanDoDriveBy;
	bool	m_bCanBeHassledByGangs;
	u16	m_nLastBustMessageNumber;

	u32	m_ModelHashKey;
	Vector3	m_Position;
	float	m_fHealth;
	float	m_nArmour;
	u32	m_nCurrentWeaponSlot;
	//	u32	m_nWeaponHashes[CWeaponInfoManager::MAX_SLOTS];
	//	u32	m_nAmmoTotals[MAX_AMMOS];

	bool	m_bPlayerHasHelmet;
	int		m_nStoredHatPropIdx;
	int		m_nStoredHatTexIdx;

	atFixedArray<SPedCompPropConvData, MAX_PROPS_PER_PED> m_CurrentProps;
	atBinaryMap<SPedCompPropConvData, atHashString> m_ComponentVariations;
	atBinaryMap<u8, atHashString> m_TextureVariations;

	atArray<CPlayerDecorationStruct> m_DecorationList;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void Initialize(CPlayerPedSaveStructure_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
// The only difference between CPlayerPedSaveStructure_Migration and CPlayerPedSaveStructure is in the type of the key used for the two atBinaryMap's.
// The maps in CPlayerPedSaveStructure use atHashString for the key. The string for an atHashString gets stripped from the Final exe.
// The maps in CPlayerPedSaveStructure_Migration use u32 for the key. This allows the data to be written to an XML file.
class CPlayerPedSaveStructure_Migration
{
public:	 
	u32		m_GamerIdHigh;
	u32		m_GamerIdLow;
	u16 	m_CarDensityForCurrentZone;
	s32	m_CollectablesPickedUp;	
	s32	m_TotalNumCollectables;
	bool 	m_DoesNotGetTired;
	bool	m_FastReload;
	bool	m_FireProof;
	u16	m_MaxHealth;
	u16	m_MaxArmour;
	bool	m_bCanDoDriveBy;
	bool	m_bCanBeHassledByGangs;
	u16	m_nLastBustMessageNumber;

	u32	m_ModelHashKey;
	Vector3	m_Position;
	float	m_fHealth;
	float	m_nArmour;
	u32	m_nCurrentWeaponSlot;
	//	u32	m_nWeaponHashes[CWeaponInfoManager::MAX_SLOTS];
	//	u32	m_nAmmoTotals[MAX_AMMOS];

	bool	m_bPlayerHasHelmet;
	int		m_nStoredHatPropIdx;
	int		m_nStoredHatTexIdx;

	atFixedArray<CPlayerPedSaveStructure::SPedCompPropConvData, MAX_PROPS_PER_PED> m_CurrentProps;

	atBinaryMap<CPlayerPedSaveStructure::SPedCompPropConvData, u32> m_ComponentVariations;	//	Use u32 instead of atHashString as the key
	atBinaryMap<u8, u32> m_TextureVariations;	//	Use u32 instead of atHashString as the key

	atArray<CPlayerPedSaveStructure::CPlayerDecorationStruct> m_DecorationList;

	void Initialize(CPlayerPedSaveStructure &copyFrom);

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#endif // SAVEGAME_DATA_PLAYER_PED_H

