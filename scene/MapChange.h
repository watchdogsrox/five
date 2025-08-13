//
// scene/MapChange.h
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#ifndef _SCENE_MAPCHANGE_H_
#define _SCENE_MAPCHANGE_H_

#include "atl/array.h"
#include "scene/Entity.h"
#include "script/thread.h"
#include "spatialdata/sphere.h"

// represents a persistent change to map objects, automatically applied
// as map sections or interiors stream in
class CMapChange
{
public:

	enum eType
	{
		TYPE_SWAP,		// swap model of affected entities
		TYPE_HIDE,		// hide affected entities
		TYPE_FORCEOBJ	// force affected objects to be real objects rather than dummy objects
	};

	CMapChange() {}
	~CMapChange() {}

	void Init(const u32 oldHash, const u32 newHash, const spdSphere& searchVolume, bool bSurviveMapReload, eType changeType, bool bAffectsScriptObjects);
	void Shutdown();

	bool Matches(u32 oldHash, u32 newHash, const spdSphere& searchVolume, eType changeType) const;
	bool IncludesMapData(s32 mapDataSlotIndex) const;

	void SetOwner(scrThreadId threadId) { m_ownerThread = threadId; m_bScriptOwned = true; }
	bool IsScriptOwned() const { return m_bScriptOwned; }
	bool IsActive() const { return m_bActive; }
	const u32 GetOldHash() const { return m_oldHash; }
	const u32 GetNewHash() const { return m_newHash; }
	const scrThreadId GetOwnerThreadId() const { return m_ownerThread; }
	const spdSphere& GetSearchVolume() const { return m_searchVolume; }
	eType GetChangeType() const { return m_changeType; }
	bool GetSurviveMapReload() const { return m_surviveMapReload; }

	bool Execute(CEntity* pEntity, bool bReverse=false);
	
	// Added for replay
	void Execute(bool bReverse);
	bool HasBeenExecuted() const { return m_bExecuted; }

private:
	void FindEntitiesFromWorldSearch(atArray<CEntity*>& entityList, u32 searchModel);
	void SetActive(bool bActive) { m_bActive = bActive; }
	bool Intersects(const spdSphere& sphere) { return m_searchVolume.IntersectsSphere(sphere); }

	eType m_changeType;
	u32 m_oldHash;
	u32 m_newHash;
	spdSphere m_searchVolume;
	scrThreadId m_ownerThread;
	atArray<u32> m_mapDataSlots;
	bool m_bActive						: 1;
	bool m_bScriptOwned					: 1;
	bool m_bAffectsHighDetailOnly		: 1;
	bool m_bAffectsScriptObjects		: 1;
	bool m_surviveMapReload				: 1;
	bool m_bExecuted					: 1;

	friend class CMapChangeMgr;
};

// manages registration of active map changes
class CMapChangeMgr
{
public:
	void Reset();

	CMapChange* Add(u32 oldHash, u32 newHash, const spdSphere& searchVol, bool bSurviveMapReload, CMapChange::eType changeType, bool bLazy, bool bIncludeScriptObjects);
	void Remove(u32 oldHash, u32 newHash, const spdSphere& searchVol, CMapChange::eType changeType, bool bLazy);

	void ApplyChangesToMapData(s32 mapDataSlotIndex);
	void ApplyChangesToEntity(CEntity* pEntity);
	void RemoveAllOwnedByScript(scrThreadId threadId);

	bool IntersectsWithActiveChange(const spdSphere& sphere);

#if GTA_REPLAY
	u32 GetNoOfActiveMapChanges() const;
	void GetActiveMapChangeDetails(u32 i, u32 &oldHash, u32 &newHash, spdSphere& searchVol, bool &surviveMapReload, CMapChange::eType &changeType, bool &bAffectsScriptObjects, bool &bExecuted) const;
	CMapChange* GetActiveMapChange(u32 oldHash, u32 newHash, const spdSphere& searchVol, CMapChange::eType changeType);
#endif // GTA_REPLAY

private:
	bool AlreadyExists(u32 oldHash, u32 newHash, const spdSphere& searchVol,  CMapChange::eType changeType) const;

	atArray<CMapChange> m_activeChanges;

	//////////////////////////////////////////////////////////////////////////
	// DEBUG
#if __BANK
public:
	void DbgRemoveByIndex(s32 index, bool bLazy=false);
	void DbgDisplayActiveChanges();
#endif	//__BANK
	//////////////////////////////////////////////////////////////////////////
};

extern CMapChangeMgr g_MapChangeMgr;

#endif	//_SCENE_MAPCHANGE_H_
