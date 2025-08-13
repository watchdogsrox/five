//
// debug/editing/LiveEditing.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _DEBUG_LIVEEDITING_H_
#define _DEBUG_LIVEEDITING_H_

#if __BANK

#include "atl/array.h"
#include "atl/map.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "objects/object.h"
#include "scene/Entity.h"

//NOTES:
// Interface to allow REST driven live-editing of map objects etc.
// Starting to live edit generates an atMap<guid, entityptr> for speedy
// lookup of entity from guids, which can then have changes applied
// such as transform changes, lod distance changes etc.
class CMapLiveEdit
{
public:
	CMapLiveEdit();
	~CMapLiveEdit() {}

	void		Start(const char* pszMapSection);
	void		Stop();
	void		Update();
	CEntity*	FindEntity(const u32 guid, bool bPreferObjects=false);

	void		DeleteMapEntity(const u32 guid, const char* pszMapSection);
	void		SetSelectedInPicker(const char* pszMapSection, const u32 guid);

private:
	void		Init();
	bool		SetEditing(const char* pszImapName);
	void		SetEnabled(bool bEnabled)			{ m_bEnabled = bEnabled; }
	bool		GetEnabled() const					{ return m_bEnabled; }
	void		ClearGuidMap()						{ m_guidMap.Reset(); }
	void		AddGuidsForImap(s32 imapSlot);
	void		AddGuidsForLiveEditAdds();
	void		RebuildGuidMap();
	
	atString				m_mapSectionBeingEdited;
	bool					m_bEnabled;
	atArray<s32>			m_imapFiles;
	atMap<u32, CEntity*>	m_guidMap;

	//////////////////////////////////////////////////////////////////////////
	// TEST WIDGETS
public:
	void		InitWidgets(bkBank& bank);

private:
	void		DebugUpdate();
	static void DebugStartEditCb();
	static void DebugStopEditCb();
	static void GenerateTestEditCb();
	static void GenerateTestDeleteCb();
	static void GenerateTestAddCb();
	static void GenerateCamChangeCb();
	static void GenerateFlagChangeCb();

	bool m_bDbgDisplay;
	bool m_bDbgQuery;
	u32 m_dbgGuid;
	bool m_bDbgDontRenderInShadows;
	bool m_bDbgOnlyRenderInShadows;
	bool m_bDbgOnlyRenderInReflections;
	bool m_bDbgDontRenderInReflections;
	bool m_bDbgOnlyRenderInWaterReflections;
	bool m_bDbgDontRenderInWaterReflections;
	//////////////////////////////////////////////////////////////////////////
};

class CMapLiveEditUtils
{
public:
	static void	FindRelatedObjects(CEntity* pDummy, atArray<CObject*>& objects);
	static void FindRelatedImapFiles(const char* pszMapSection, atArray<s32>& relatedImaps);

private:
	static void RecursivelyAddChildren(s32 imapSlot, atArray<s32>& relatedImaps);
};

extern CMapLiveEdit g_mapLiveEditing;

#endif	//__BANK

#endif	//_DEBUG_LIVEEDITING_H_