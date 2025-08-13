//
// weapons/weaponcomponentdata.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_DATA_H
#define WEAPON_COMPONENT_DATA_H

// Rage headers
#include "fwutil/rtti.h"
#include "parser/macros.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentData
{
	DECLARE_RTTI_BASE_CLASS(CWeaponComponentData);
public:

	virtual ~CWeaponComponentData();

	// PURPOSE: Get the name hash
	u32 GetHash() const;

#if !__FINAL
	// PURPOSE: Get the name
	const char* GetName() const;
#endif // !__FINAL

private:

	//
	// Members
	//

	// PURPOSE: The hash of the name
	atHashWithStringNotFinal m_Name;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline CWeaponComponentData::~CWeaponComponentData()
{
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentData::GetHash() const
{
	return m_Name.GetHash();
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
inline const char* CWeaponComponentData::GetName() const
{
	return m_Name.GetCStr();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_DATA_H
