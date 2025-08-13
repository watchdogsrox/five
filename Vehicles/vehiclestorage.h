/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    vehiclestorage.h
// PURPOSE : Stuff having to do with taking vehicles off the map and storing them in a small structure.
//			 Later on the vehicle can be recreated from this structure.
// AUTHOR :  Obbe.
// CREATED : 30-9-05
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _VEHICLESTORAGE_H_
#define _VEHICLESTORAGE_H_

//#include "vehicles\vehicle.h"
#include "modelinfo/VehicleModelInfoVariation.h"

#include "physics/WorldProbe/worldprobe.h"

#include "shaders/CustomShaderEffectVehicle.h"



/////////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////////

class CStoredVehicleVariations
{
public:
	CStoredVehicleVariations();

	void StoreVariations(const CVehicle* pVeh);

	bool IsToggleModOn(eVehicleModType modSlot) const;

	void				SetKitIndex(u16 kitIndex)	{ m_kitIdx = kitIndex; }
	u16					GetKitIndex() const			{ return m_kitIdx; }

	void				SetModIndex(eVehicleModType modSlot, u8 modIndex) { m_mods[modSlot] = modIndex; }
	u8					GetModIndex(eVehicleModType modSlot) const			{ return m_mods[modSlot]; }

	void				SetColor1(u8 col)			{ m_color1 = col; }
	void				SetColor2(u8 col)			{ m_color2 = col; }
	void				SetColor3(u8 col)			{ m_color3 = col; }
	void				SetColor4(u8 col)			{ m_color4 = col; }
	void				SetColor5(u8 col)			{ m_color5 = col; }
	void				SetColor6(u8 col)			{ m_color6 = col; }
	u8					GetColor1() const			{ return m_color1; }
	u8					GetColor2() const			{ return m_color2; }
	u8					GetColor3() const			{ return m_color3; }
	u8					GetColor4() const			{ return m_color4; }
	u8					GetColor5() const			{ return m_color5; }
	u8					GetColor6() const			{ return m_color6; }

	void				SetSmokeColorR(u8 r)		{ m_smokeColR = r; }
	void				SetSmokeColorG(u8 g)		{ m_smokeColG = g; }
	void				SetSmokeColorB(u8 b)		{ m_smokeColB = b; }
	u8					GetSmokeColorR() const		{ return m_smokeColR; }
	u8					GetSmokeColorG() const		{ return m_smokeColG; }
	u8					GetSmokeColorB() const		{ return m_smokeColB; }

	void				SetNeonColorR(u8 r)			{ m_neonColR = r; }
	void				SetNeonColorG(u8 g)			{ m_neonColG = g; }
	void				SetNeonColorB(u8 b)			{ m_neonColB = b; }
	u8					GetNeonColorR() const		{ return m_neonColR; }
	u8					GetNeonColorG() const		{ return m_neonColG; }
	u8					GetNeonColorB() const		{ return m_neonColB; }
	void				SetNeonFlags(u8 flags)		{ m_neonFlags = flags; }
	u8					GetNeonFlags() const		{ return m_neonFlags; }

	void				SetWindowTint(u8 col)		{ m_windowTint = col; }
	u8					GetWindowTint() const		{ return m_windowTint; }

	void				SetWheelType(u8 type)		{ m_wheelType = type; }
	u8					GetWheelType() const		{ return m_wheelType; }

    bool                HasModVariation(u32 slot) const { return m_modVariation[slot]; }
	void				SetModVariation(u32 slot, bool val) { m_modVariation[slot] = val; }

	bool				IsEqual(const CStoredVehicleVariations& rhs) const
	{
		for(int i = 0; i < VMT_MAX; ++i)
		{
			if(m_mods[i] != rhs.m_mods[i])
			{
				return false;
			}
		}

		return m_kitIdx == rhs.m_kitIdx && 
				m_color1 == rhs.m_color1 && 
				m_color2 == rhs.m_color2 && 
				m_color3 == rhs.m_color3 && 
				m_color4 == rhs.m_color4 && 
				m_color5 == rhs.m_color5 && 
				m_color6 == rhs.m_color6 && 
				m_smokeColR == rhs.m_smokeColR &&
				m_smokeColG == rhs.m_smokeColG && 
				m_smokeColB == rhs.m_smokeColB && 
				m_neonColR == rhs.m_neonColR &&
				m_neonColG == rhs.m_neonColG && 
				m_neonColB == rhs.m_neonColB && 
				m_neonFlags == rhs.m_neonFlags && 
				m_windowTint == rhs.m_windowTint &&
				m_wheelType == rhs.m_wheelType &&
				m_modVariation[0] == rhs.m_modVariation[0] &&
				m_modVariation[1] == rhs.m_modVariation[1];
	}

private:
	//	u8 m_numMods;
	u16 m_kitIdx;
	u8 m_mods[VMT_MAX];
	u8 m_color1, m_color2, m_color3, m_color4, m_color5, m_color6;
	u8 m_smokeColR, m_smokeColG, m_smokeColB;
	u8 m_neonColR, m_neonColG, m_neonColB, m_neonFlags;
	u8 m_windowTint;
	u8 m_wheelType;
    bool m_modVariation[2]; // variations for the two types of wheels
};

/////////////////////////////////////////////////////////////////////////////////
// A class to store one vehicle. Only the essentials to restore a car are
// stored.
/////////////////////////////////////////////////////////////////////////////////

class CStoredVehicle
{
public:

	CStoredVehicle() { Clear(); }

	CStoredVehicleVariations m_variation;

	float	m_CoorX, m_CoorY, m_CoorZ;
	u32		m_FlagsLocal;
	u32		m_ModelHashKey;

	s16		m_HornSoundIndex;
	s16		m_AudioEngineHealth;
	s16		m_AudioBodyHealth;

	u16		m_bUsed:1;					// Is this vehicle filled at the moment
	u16		m_bInInterior:1;
	u16		m_bNotDamagedByBullets:1;
	u16		m_bNotDamagedByFlames:1;
	u16		m_bIgnoresExplosions:1;
	u16		m_bNotDamagedByCollisions:1;
	u16		m_bNotDamagedByMelee:1;
	u16		m_bInteriorRequired:1; // if we've requested this vehicle with STRFLAG_INTERIOR_REQUIRED, to ensure it streams in
	u16		m_bTyresDontBurst:1;
	u16		m_pad:7;

//	u8		m_RadioStation;
	u32		m_nDisableExtras;
	s32		m_LiveryId;
	s32		m_Livery2Id;
	WorldProbe::CShapeTestSingleResult m_collisionTestResult; // doesn't have to be saved
//	s8		m_BombOnBoard;	// Status of bombs we might be carrying
//	s8		m_PaintJob;
	s8		m_iFrontX, m_iFrontY, m_iFrontZ;
	s8		m_plateTexIndex;

	u8		m_LicencePlateText[CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX];

	void		StoreVehicle(class CVehicle *pVeh);
	CVehicle	*RestoreVehicle();
	void		Clear();
	bool		IsUsed();

	void	SetModelHashKey(u32 modelHashKey);
	u32		GetModelHashKey(void)					{ return(m_ModelHashKey); }

private:
	u32		m_hashedModelHashKey;
};

#endif
