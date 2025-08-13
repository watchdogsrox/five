//
// audio/environment/occlusion/occlusioninteriorinfo.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef NAOCCLUSIONINTERIORINFO_H
#define NAOCCLUSIONINTERIORINFO_H

// Game
#include "audio/audiodefines.h"
#include "audio/environment/occlusion/occlusioninteriormetadata.h"
#include "audio/environment/occlusion/occlusionpathnode.h"
#include "audio/environment/occlusion/occlusionportalinfo.h"

// Rage
#include "atl/ptr.h"

class CInteriorProxy;

namespace rage
{
	class parTree;
	class parTreeNode;
}

class naOcclusionInteriorInfo
{

public:

	FW_REGISTER_CLASS_POOL(naOcclusionInteriorInfo);

	naOcclusionInteriorInfo();
	~naOcclusionInteriorInfo();

	static void InitClass();  

	static naOcclusionInteriorInfo* Allocate();
	
	void Update();

	bool InitFromMetadata(naOcclusionInteriorMetadata* interiorMetadata);

	void SetInteriorMetadata(naOcclusionInteriorMetadata* interiorMetadata) { naAssert(!m_InteriorMetadata); m_InteriorMetadata = interiorMetadata; }

	u32 GetKeyFromPathNode(naOcclusionPathNode* pathNode);
	naOcclusionPathNode* GetPathNodeFromKey(const u32 key); 
	naOcclusionPathNode* CreatePathNodeFromKey(const u32 key);

#if __BANK
	s32 GetNumPathNodes() const { return m_PathNodeMap.GetNumUsed(); }
	s32 GetNumPortalInfos() const { return m_PortalInfoList.GetCount(); }
#endif

#if __ASSERT
	u32 GetNumPortalInfosAbovePoolSize() const { return m_NumPortalInfosAbovePoolSize; }
	u32 GetNumPathNodesAbovePoolSize() const { return m_NumPathNodesAbovePoolSize; }
#endif

#if __BANK
	void PopulateMetadataFromInteriorInfo(naOcclusionInteriorMetadata* interiorMetadata, const bool isBuildingIntraInteriorPaths);

	naOcclusionPortalInfo* CreatePortalInfo(const CInteriorProxy* intProxy, const s32 roomIdx, const u32 portalIdx, const u32 destUniqueInteriorHashkey, const s32 destRoomIdx);

	void DeletePathTreeWithKey(const u32 key) { m_PathNodeMap.Delete(key); }

	naOcclusionInteriorMetadata* GetInteriorMetadata() { return m_InteriorMetadata; }
#endif

private:

#if __BANK
	void WritePortalInfoToInteriorMetadata(naOcclusionInteriorMetadata* interiorMetadata, const bool isBuildingIntraInteriorPaths);
	void WritePathTreesToInteriorMetadata(naOcclusionInteriorMetadata* interiorMetadata);

	u32 GetPathDepthForPathTree(naOcclusionPathNode* pathTree);

	s32 GetPortalInfoIdxForPortalInfo(naOcclusionPortalInfo* portalInfo);

	void GetAllRequiredPortalInfosForPathTree(naOcclusionPathNode* pathTree, atArray<naOcclusionPortalInfo*>& portalInfosUsedByPathTree);
	void GetAllRequiredKeysForPathTree(naOcclusionPathNode* pathTree, atArray<u32>& keysUsedByPathTree);
#endif


public:

#if __BANK
	static const s32 sm_OcclusionBuildInteriorInfoPoolSize = 200;
#endif

private:

	atMap<u32, naOcclusionPathNode*> m_PathNodeMap;
	atArray< atPtr<naOcclusionPortalInfo> > m_PortalInfoList;

	naOcclusionInteriorMetadata* m_InteriorMetadata;

#if __ASSERT
	u32 m_NumPortalInfosAbovePoolSize;
	u32 m_NumPathNodesAbovePoolSize;
#endif
};

#endif // NAOCCLUSIONPORTALINFO_H
