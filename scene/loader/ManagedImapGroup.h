//
// scene/loader/ManagedImapGroup.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _SCENE_LOADER_MANAGEDIMAPGROUP_H_
#define _SCENE_LOADER_MANAGEDIMAPGROUP_H_

#include "atl/hashstring.h"
#include "scene/loader/PackFileMetaData.h"
#include "vectormath/vec3v.h"

#if __BANK
#include "atl/array.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#endif	//__BANK

// NOTES
// Usually IMAP groups are simply map data files which can be activated and
// deactivated by script, or sometimes by rayfire animation. However some
// IMAP groups are automatically activated and deactivated based on time
// of day or weather conditions. This system is responsible for managing that.

class CManagedImapGroup
{
public:
	CManagedImapGroup() {}
	~CManagedImapGroup() {}

	void Init(const CMapDataGroup& imapGroup);
	void Update(Vec3V_In vPos);

	void SetActive(bool bActive);
	bool IsActive() const { return m_bActive; }

	const atHashString& GetName() const { return m_name; }

private:
	bool IsRequired() const;
	void SwapStateIfSafe(Vec3V_In vPos);

	atHashString			m_name;
	s32						m_mapDataSlot;
	bool					m_bWeatherDependent;
	bool					m_bTimeDependent;
	u32						m_hoursOnOff;
	bool					m_bActive;
	bool					m_bOnlyRemoveWhenOutOfRange;
	atArray<atHashString>	m_aWeatherTypes;

#if __BANK
	friend class CManagedImapGroupMgr;
#endif	//__BANK
};

class CManagedImapGroupMgr
{
public:
	static void Reset();
	static void Update(Vec3V_In vPos);
	static void Add(const CMapDataGroup& imapGroup);
	static void MarkManagedImapFiles();

private:
	static atArray<CManagedImapGroup> ms_groups;

	//////////////////////////////////////////////////////////////////////////
#if __BANK
public:
	static void	InitWidgets(bkBank& bank);

private:
	static void UpdateDebug();

	static bool ms_bDisplayManagedImaps;
	static bool ms_bDisplayActiveOnly;
#endif	//__BANK
	//////////////////////////////////////////////////////////////////////////

};

#endif	//_SCENE_LOADER_MANAGEDIMAPGROUP_H_