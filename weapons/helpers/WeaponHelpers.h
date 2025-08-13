//
// weapons/helpers/weaponhelpers.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_HELPERS_H
#define WEAPON_HELPERS_H

// Rage headers
#include "atl/hashstring.h"
#include "parser/macros.h"

// Forward declarations
namespace rage { class parTreeNode; }
class CBaseModelInfo;

////////////////////////////////////////////////////////////////////////////////

class CWeaponBoneId
{
public:

	CWeaponBoneId();

	// PURPOSE: Get the bone index
	s32 GetBoneIndex(const u32 uModelIndex) const;
	s32 GetBoneIndex(const CBaseModelInfo* pModelInfo) const;

private:

	// PURPOSE: Used to read the bone string without storing it as a member
	void OnPreLoad(parTreeNode* pNode);

	// PURPOSE: Used to save the string when its not a regular member
	void OnPostSave(parTreeNode* pNode);

	// PURPOSE: Adjust the RAG widgets
	void OnPostAddWidgets(bkBank& bank);

	//
	// Members
	//

	// PURPOSE: The bone id
	u16 m_BoneId;

	PAR_SIMPLE_PARSABLE;
};

#endif // WEAPON_HELPERS_H
