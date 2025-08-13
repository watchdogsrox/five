#ifndef SAVEGAME_DATA_GARAGES_H
#define SAVEGAME_DATA_GARAGES_H

// Rage headers
#include "parser/macros.h"

// Game headers
#include "control/garages.h"
#include "SaveLoad/savegame_defines.h"
#include "vehicles/vehiclestorage.h"


// Forward declarations
//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
class CSavedVehicleVariationInstance_Migration;
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
class CSavedVehicle_Migration;
class CSavedVehiclesInSafeHouse_Migration;
class CSaveGarage_Migration;
class CSaveGarages_Migration;
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


class CSaveGarages
{
public:
	class CSavedVehicleVariationInstance
	{
//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
		friend class CSavedVehicleVariationInstance_Migration;
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

	public:
		void CopyVehicleVariationBeforeSaving(const CStoredVehicleVariations *pVehicleVariationToSave);
		void CopyVehicleVariationAfterLoading(CStoredVehicleVariations *pVehicleVariationToLoad);

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		void Initialize(CSavedVehicleVariationInstance_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	private:
		//		u8 m_numMods;
		atBinaryMap<u8, atHashString> m_mods;
		bool m_modVariation[2];
		u8 m_kitIdx;				// this is misnamed - it's not a kit Index, it's a kit ID which is saved out. Can't rename it or legacy save games break though. The U16 version below replaces this completely.
		u8 m_color1, m_color2, m_color3, m_color4, m_color5, m_color6;
		u8 m_smokeColR, m_smokeColG, m_smokeColB;
		u8 m_neonColR, m_neonColG, m_neonColB, m_neonFlags;
		u8 m_windowTint;
		u8 m_wheelType;
		u16 m_kitID_U16;
		PAR_SIMPLE_PARSABLE
	};
	class CSavedVehicle
	{
	public:
		CSavedVehicleVariationInstance m_variation;
		float		m_CoorX, m_CoorY, m_CoorZ;
		u32		m_FlagsLocal;
		u32		m_ModelHashKey;
		u32		m_nDisableExtras;
		s32		m_LiveryId;
		s32		m_Livery2Id;
		s16		m_HornSoundIndex;
		s16		m_AudioEngineHealth;
		s16		m_AudioBodyHealth;
		s8		m_iFrontX, m_iFrontY, m_iFrontZ;
		bool		m_bUsed;					// Is this vehicle filled at the moment
		bool		m_bInInterior;
		bool		m_bNotDamagedByBullets;
		bool		m_bNotDamagedByFlames;
		bool		m_bIgnoresExplosions;
		bool		m_bNotDamagedByCollisions;
		bool		m_bNotDamagedByMelee;
		bool		m_bTyresDontBurst;

		s8			m_LicensePlateTexIndex;
		u8			m_LicencePlateText[CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX];

		void CopyVehicleIntoSavedVehicle(CStoredVehicle *pVehicle);
		void CopyVehicleOutOfSavedVehicle(CStoredVehicle *pVehicle);

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		void Initialize(CSavedVehicle_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

		PAR_SIMPLE_PARSABLE
	};

	class CSavedVehiclesInSafeHouse
	{
	public:
		atHashString m_NameHashOfGarage;
		atArray<CSavedVehicle> m_VehiclesSavedInThisSafeHouse;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		void Initialize(CSavedVehiclesInSafeHouse_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

		PAR_SIMPLE_PARSABLE
	};

	class CSaveGarage
	{
	public:
		atHashString m_NameHash;	//	If we ever get to a point where this is the only member that is actually being saved
		//	then we can just remove CSaveGarage completely

		u8	 m_Type;			//	There's a script command to change this so it probably does need to be saved
		bool m_bLeaveCameraAlone;	//	There's a script command to change this so it probably does need to be saved
		bool m_bSavingVehilclesEnabled;  // Saving enabled

		void CopyGarageIntoSaveGarage(CGarage *pGarage);
		void CopyGarageOutOfSaveGarage(CGarage *pGarage);

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
		void Initialize(CSaveGarage_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

		PAR_SIMPLE_PARSABLE
	};

	bool m_RespraysAreFree;
	bool m_NoResprays;
	//	aCarsInSafeHouse
	atArray<CSavedVehiclesInSafeHouse> m_SafeHouses;

	atArray<CSaveGarage> m_SavedGarages;

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void Initialize(CSaveGarages_Migration &copyFrom);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
// The only difference between CSaveGarages_Migration and CSaveGarages is in the type of the key used for the m_mods Binary Map.
// The one in CSaveGarages uses atHashString for the key. The string for an atHashString gets stripped from the Final exe.
// The one in CSaveGarages_Migration uses u32 for the key. This allows the data to be written to an XML file.
class CSavedVehicleVariationInstance_Migration
{
public:
	void Initialize(CSaveGarages::CSavedVehicleVariationInstance &copyFrom);

public:
	//		u8 m_numMods;
	atBinaryMap<u8, u32> m_mods;	//	Use u32 instead of atHashString as the key
	bool m_modVariation[2];
	u8 m_kitIdx;				// this is misnamed - it's not a kit Index, it's a kit ID which is saved out. Can't rename it or legacy save games break though. The U16 version below replaces this completely.
	u8 m_color1, m_color2, m_color3, m_color4, m_color5, m_color6;
	u8 m_smokeColR, m_smokeColG, m_smokeColB;
	u8 m_neonColR, m_neonColG, m_neonColB, m_neonFlags;
	u8 m_windowTint;
	u8 m_wheelType;
	u16 m_kitID_U16;
	PAR_SIMPLE_PARSABLE
};

class CSavedVehicle_Migration
{
public:
	void Initialize(CSaveGarages::CSavedVehicle &copyFrom);

	CSavedVehicleVariationInstance_Migration m_variation;
	float		m_CoorX, m_CoorY, m_CoorZ;
	u32		m_FlagsLocal;
	u32		m_ModelHashKey;
	u32		m_nDisableExtras;
	s32		m_LiveryId;
	s32		m_Livery2Id;
	s16		m_HornSoundIndex;
	s16		m_AudioEngineHealth;
	s16		m_AudioBodyHealth;
	s8		m_iFrontX, m_iFrontY, m_iFrontZ;
	bool		m_bUsed;					// Is this vehicle filled at the moment
	bool		m_bInInterior;
	bool		m_bNotDamagedByBullets;
	bool		m_bNotDamagedByFlames;
	bool		m_bIgnoresExplosions;
	bool		m_bNotDamagedByCollisions;
	bool		m_bNotDamagedByMelee;
	bool		m_bTyresDontBurst;

	s8			m_LicensePlateTexIndex;
	u8			m_LicencePlateText[CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX];

	PAR_SIMPLE_PARSABLE
};

class CSavedVehiclesInSafeHouse_Migration
{
public:
	void Initialize(CSaveGarages::CSavedVehiclesInSafeHouse &copyFrom);

	atHashString m_NameHashOfGarage;
	atArray<CSavedVehicle_Migration> m_VehiclesSavedInThisSafeHouse;

	PAR_SIMPLE_PARSABLE
};

class CSaveGarage_Migration
{
public:
	void Initialize(CSaveGarages::CSaveGarage &copyFrom);

	atHashString m_NameHash;	//	If we ever get to a point where this is the only member that is actually being saved
	//	then we can just remove CSaveGarage completely

	u8	 m_Type;			//	There's a script command to change this so it probably does need to be saved
	bool m_bLeaveCameraAlone;	//	There's a script command to change this so it probably does need to be saved
	bool m_bSavingVehilclesEnabled;  // Saving enabled

	PAR_SIMPLE_PARSABLE
};

class CSaveGarages_Migration
{
public:
	bool m_RespraysAreFree;
	bool m_NoResprays;
	//	aCarsInSafeHouse
	atArray<CSavedVehiclesInSafeHouse_Migration> m_SafeHouses;

	atArray<CSaveGarage_Migration> m_SavedGarages;

	void Initialize(CSaveGarages &copyFrom);

	void PreSave();
	void PostLoad();

	PAR_SIMPLE_PARSABLE
};
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#endif // SAVEGAME_DATA_GARAGES_H

