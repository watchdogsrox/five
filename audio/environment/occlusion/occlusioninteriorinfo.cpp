// 
// audio/environment/occlusion/occlusioninteriorinfo.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

// GAME
#include "audio/audio_channel.h"
#include "audio/environment/occlusion/occlusioninteriorinfo.h"
#include "audio/northaudioengine.h"

// RAGE
#include "parser/treenode.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()


FW_INSTANTIATE_CLASS_POOL(naOcclusionInteriorInfo, CONFIGURED_FROM_FILE, atHashString("OcclusionInteriorInfo",0xd21f4731));	

naOcclusionInteriorInfo::naOcclusionInteriorInfo() : m_InteriorMetadata(NULL)
{
#if __ASSERT
	m_NumPortalInfosAbovePoolSize = 0;
	m_NumPathNodesAbovePoolSize = 0;
#endif
}

naOcclusionInteriorInfo::~naOcclusionInteriorInfo()
{
	const s32 numPortalInfos = m_PortalInfoList.GetCount();
	for(s32 i = 0; i < numPortalInfos; i++)
	{
		if(m_PortalInfoList[i])
		{
			// This is a smart pointer, so if there are no other interiors referencing this portalInfo, then it will also be deleted here
			m_PortalInfoList[i] = NULL;
		}		
	}

	atMap<u32, naOcclusionPathNode*>::Iterator entry = m_PathNodeMap.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); )
	{
		naOcclusionPathNode** pathNodePtr = m_PathNodeMap.Access(entry.GetKey());
		entry.Next();
		if(pathNodePtr && *pathNodePtr)
		{
			delete (*pathNodePtr);
		}
	}
}

void naOcclusionInteriorInfo::InitClass()
{
}

naOcclusionInteriorInfo* naOcclusionInteriorInfo::Allocate()
{
	if(naVerifyf(naOcclusionInteriorInfo::GetPool()->GetNoOfFreeSpaces() > 0, "naOcclusionInteriorInfo pool is full, need to increase size"))
	{
		return rage_new naOcclusionInteriorInfo();
	}
	return NULL;
}

bool naOcclusionInteriorInfo::InitFromMetadata(naOcclusionInteriorMetadata* interiorMetadata)
{
	bool initSuccessful = true;

	if(naVerifyf(interiorMetadata, "Passed NULL naOcclusionInteriorMetadata* when initializing a naOcclusionInteriorInfo object"))
	{
		SetInteriorMetadata(interiorMetadata);

		// Load in the portal information first as we'll need pointers to them when creating the path node trees
		s32 numPortalInfos = interiorMetadata->m_PortalInfoList.GetCount();
		m_PortalInfoList.Resize(numPortalInfos);

		for(s32 i = 0; i < numPortalInfos; i++)
		{
			// First try and find the portal if we already have one allocated
			m_PortalInfoList[i] = audNorthAudioEngine::GetOcclusionManager()->GetPortalInfoFromParameters(interiorMetadata->m_PortalInfoList[i].m_InteriorProxyHash,
																											interiorMetadata->m_PortalInfoList[i].m_RoomIdx,
																											interiorMetadata->m_PortalInfoList[i].m_PortalIdx);
			if(!m_PortalInfoList[i])
			{
				m_PortalInfoList[i] = naOcclusionPortalInfo::Allocate();
				if(m_PortalInfoList[i])
				{
					m_PortalInfoList[i]->InitFromMetadata(&interiorMetadata->m_PortalInfoList[i]);
				}
				else
				{
					initSuccessful = false;
#if __ASSERT
					m_NumPortalInfosAbovePoolSize++;
#endif
				}
			}
		}

		for(s32 i = 0; i < interiorMetadata->m_PathNodeList.GetCount(); i++)
		{
			naOcclusionPathNode* pathNode = naOcclusionPathNode::Allocate();
			if(pathNode)
			{
				naOcclusionPathNodeMetadata* pathNodeMetadata = &interiorMetadata->m_PathNodeList[i];
				s32 numChildren = pathNodeMetadata->m_PathNodeChildList.GetCount();
				pathNode->ReserveChildArray(numChildren);
				for(s32 childIdx = 0; childIdx < numChildren; childIdx++)
				{
					if(naVerifyf(pathNodeMetadata->m_PathNodeChildList[childIdx].m_PortalInfoIdx != INTLOC_INVALID_INDEX,
						"Metadata portal index value is INTLOC_INVALID_INDEX, need to investigate what's wrong in build process (most likely a pool is too small)") 
						&&
						naVerifyf(pathNodeMetadata->m_PathNodeChildList[childIdx].m_PortalInfoIdx < m_PortalInfoList.GetCount(), 
						"Bad m_PortalInfoIdx: %d in audio occlusion metadata.  Out of bounds of array [0 - %d]", 
						pathNodeMetadata->m_PathNodeChildList[childIdx].m_PortalInfoIdx, m_PortalInfoList.GetCount()))
					{
						naOcclusionPortalInfo* portalInfo = m_PortalInfoList[pathNodeMetadata->m_PathNodeChildList[childIdx].m_PortalInfoIdx];
						if(portalInfo)
						{
							// Since we load in paths from least to greatest depths, we should already have any pathnodes a child is referencing in the map
							naOcclusionPathNode* childPathNode = NULL;
							if(pathNodeMetadata->m_PathNodeChildList[childIdx].m_PathNodeKey != 0)
							{
								naOcclusionPathNode** childPathNodePtr = m_PathNodeMap.Access(pathNodeMetadata->m_PathNodeChildList[childIdx].m_PathNodeKey);
								if(childPathNodePtr && *childPathNodePtr)
								{
									childPathNode = *childPathNodePtr;
								}
							}
							pathNode->CreateChild(childPathNode, portalInfo);
						}
					}
				}

				// Store a ptr to the naOcclusionPathNode* in a map so we can quickly look them up later and also
				// so we can find them when connecting children to pathnodes as we start loading paths with depths greater than 1
				naOcclusionPathNode** pathNodeMapPtr = &m_PathNodeMap[pathNodeMetadata->m_Key];
				*pathNodeMapPtr = pathNode;
			}
			else
			{
				initSuccessful = false;
#if __ASSERT
				m_NumPathNodesAbovePoolSize++;
#endif
			}
		}
	}

	return initSuccessful;
}

naOcclusionPathNode* naOcclusionInteriorInfo::GetPathNodeFromKey(const u32 key)
{
	naOcclusionPathNode** pathNodePtr = m_PathNodeMap.Access(key);
	if(pathNodePtr)
	{
		return *pathNodePtr;
	}
	return NULL;
}

#if __BANK
s32 QSortPortalInfoCompareFunc(const atPtr<naOcclusionPortalInfo>* infoA, const atPtr<naOcclusionPortalInfo>* infoB)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if((*infoA)->GetUniqueInteriorProxyHashkey() == (*infoB)->GetUniqueInteriorProxyHashkey())
	{
		if((*infoA)->GetRoomIdx() == (*infoB)->GetRoomIdx())
		{
			if(naVerifyf((*infoA)->GetPortalIdx() != (*infoB)->GetPortalIdx(), "Shouldn't have duplicate portal infos in here"))
			{
				return (*infoA)->GetPortalIdx() < (*infoB)->GetPortalIdx() ? -1 : 1;
			}
		}
		else
		{
			return (*infoA)->GetRoomIdx() < (*infoB)->GetRoomIdx() ? -1 : 1;
		}
	}
	else
	{
		return (*infoA)->GetUniqueInteriorProxyHashkey() < (*infoB)->GetUniqueInteriorProxyHashkey() ? -1 : 1;
	}

	return 0;
}

s32 QSortPathNodeCompareFunc(const u32* a, const u32* b)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(*a < *b)
	{
		return -1;
	}
	else if(*a > *b)
	{
		return 1;
	}
	else
	{
		naAssertf(0, "We shouldn't have duplicate keys in the required keys array");
		return 0;
	}
}

void naOcclusionInteriorInfo::PopulateMetadataFromInteriorInfo(naOcclusionInteriorMetadata* interiorMetadata, const bool isBuildingIntraInteriorPaths)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	WritePortalInfoToInteriorMetadata(interiorMetadata, isBuildingIntraInteriorPaths);
	WritePathTreesToInteriorMetadata(interiorMetadata);
}

void naOcclusionInteriorInfo::WritePortalInfoToInteriorMetadata(naOcclusionInteriorMetadata* interiorMetadata, const bool isBuildingIntraInteriorPaths)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	// Use an array of smart pointers, this way we'll add refs to the ones we want to keep and when
	// we reset the m_PortalInfoList, we won't delete any needed naOcclusionPortalInfo objects
	atArray< atPtr<naOcclusionPortalInfo> > requiredPortalInfos;

	// We may need portal infos in inter-interior paths that we don't need in intra-interior paths.  
	// So when building intra-interior paths, just write out every portal info we have.
	if(isBuildingIntraInteriorPaths)
	{
		for(s32 i = 0; i < naOcclusionPortalInfo::GetPool()->GetSize(); i++)
		{
			naOcclusionPortalInfo* portalInfo = naOcclusionPortalInfo::GetPool()->GetSlot(i);
			if(portalInfo)
			{
				requiredPortalInfos.PushAndGrow(portalInfo);
			}
		}
	}
	// Otherwise, for inter-interior paths, cycle through all the paths that we built and determine which portal infos we require for those paths.
	else
	{
		atMap<u32, naOcclusionPathNode*>::Iterator entry = m_PathNodeMap.CreateIterator();
		for(entry.Start(); !entry.AtEnd(); entry.Next())
		{
			naOcclusionPathNode** pathNodePtr = m_PathNodeMap.Access(entry.GetKey());
			if(pathNodePtr && *pathNodePtr)
			{
				atArray<naOcclusionPortalInfo*> portalInfosUsedByPathTree;
				GetAllRequiredPortalInfosForPathTree(*pathNodePtr, portalInfosUsedByPathTree);

				for(s32 i = 0; i < portalInfosUsedByPathTree.GetCount(); i++)
				{
					bool isPortalInfoAlreadyInArray = false;
					for(s32 j = 0; j < requiredPortalInfos.GetCount(); j++)
					{
						if(portalInfosUsedByPathTree[i]->GetUniqueInteriorProxyHashkey() == requiredPortalInfos[j]->GetUniqueInteriorProxyHashkey() &&
							portalInfosUsedByPathTree[i]->GetRoomIdx() == requiredPortalInfos[j]->GetRoomIdx() &&
							portalInfosUsedByPathTree[i]->GetPortalIdx() == requiredPortalInfos[j]->GetPortalIdx())
						{
							isPortalInfoAlreadyInArray = true;
						}
					}

					if(!isPortalInfoAlreadyInArray)
					{
						requiredPortalInfos.PushAndGrow(portalInfosUsedByPathTree[i]);
					}
				}
			}
		}
	}

	USE_DEBUG_MEMORY();

	interiorMetadata->m_PortalInfoList.Resize(requiredPortalInfos.GetCount());

	requiredPortalInfos.QSort(0, -1, QSortPortalInfoCompareFunc);

	for(s32 i = 0; i < requiredPortalInfos.GetCount(); i++)
	{
		interiorMetadata->m_PortalInfoList[i].m_InteriorProxyHash = requiredPortalInfos[i]->GetUniqueInteriorProxyHashkey();
		interiorMetadata->m_PortalInfoList[i].m_PortalIdx = requiredPortalInfos[i]->GetPortalIdx();
		interiorMetadata->m_PortalInfoList[i].m_RoomIdx = requiredPortalInfos[i]->GetRoomIdx();
		interiorMetadata->m_PortalInfoList[i].m_DestInteriorHash = requiredPortalInfos[i]->GetDestInteriorHash();
		interiorMetadata->m_PortalInfoList[i].m_DestRoomIdx = requiredPortalInfos[i]->GetDestRoomIdx();
		
		interiorMetadata->m_PortalInfoList[i].m_PortalEntityList.Resize(requiredPortalInfos[i]->GetNumLinkedEntities());
		
		for(s32 entityIdx = 0; entityIdx < requiredPortalInfos[i]->GetNumLinkedEntities(); entityIdx++)
		{
			interiorMetadata->m_PortalInfoList[i].m_PortalEntityList[entityIdx].m_LinkType = requiredPortalInfos[i]->GetPortalEntity(entityIdx)->GetLinkType();
			interiorMetadata->m_PortalInfoList[i].m_PortalEntityList[entityIdx].m_MaxOcclusion = requiredPortalInfos[i]->GetPortalEntity(entityIdx)->GetMaxOcclusion();
			interiorMetadata->m_PortalInfoList[i].m_PortalEntityList[entityIdx].m_EntityModelHashkey = requiredPortalInfos[i]->GetPortalEntity(entityIdx)->GetEntityModelHashkey();
			interiorMetadata->m_PortalInfoList[i].m_PortalEntityList[entityIdx].m_IsDoor = requiredPortalInfos[i]->GetPortalEntity(entityIdx)->GetIsDoor();
			interiorMetadata->m_PortalInfoList[i].m_PortalEntityList[entityIdx].m_IsGlass = requiredPortalInfos[i]->GetPortalEntity(entityIdx)->GetIsGlass();
		}
	}

	m_PortalInfoList.Reset();
	m_PortalInfoList = requiredPortalInfos;
}

void naOcclusionInteriorInfo::WritePathTreesToInteriorMetadata(naOcclusionInteriorMetadata* interiorMetadata)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	// Build the keys for all paths that could be needed
	atArray<u32> requiredKeys;

	atMap<u32, naOcclusionPathNode*>::Iterator entry = m_PathNodeMap.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); entry.Next())
	{
		naOcclusionPathNode** pathNodePtr = m_PathNodeMap.Access(entry.GetKey());
		if(pathNodePtr && *pathNodePtr)
		{
			// Add the key for the pathTree root, and then recursively go through all the children and find all path keys needed
			atArray<u32> keysUsedByPathTree;
			keysUsedByPathTree.PushAndGrow(entry.GetKey());
			GetAllRequiredKeysForPathTree(*pathNodePtr, keysUsedByPathTree);

			for(s32 i = 0; i < keysUsedByPathTree.GetCount(); i++)
			{
				bool isInRequiredKeyArray = false;
				for(s32 j = 0; j < requiredKeys.GetCount(); j++)
				{
					if(keysUsedByPathTree[i] == requiredKeys[j])
					{
						isInRequiredKeyArray = true;
					}
				}

				if(!isInRequiredKeyArray)
				{
					requiredKeys.PushAndGrow(keysUsedByPathTree[i]);
				}
			}
		}
	}


	USE_DEBUG_MEMORY();

	interiorMetadata->m_PathNodeList.Resize(requiredKeys.GetCount());

	requiredKeys.QSort(0, -1, QSortPathNodeCompareFunc);

	// Need to keep a separate idx from i because we want the lower path depths to be written out first so they load in first.
	u32 intMetaIdx = 0; 

	for(u32 pathDepth = 1; pathDepth <= naOcclusionManager::sm_MaxOcclusionPathDepth; pathDepth++)
	{
		for(s32 i = 0; i < requiredKeys.GetCount(); i++)
		{
			naOcclusionPathNode* pathTree = audNorthAudioEngine::GetOcclusionManager()->GetPathNodeFromKey(requiredKeys[i]);
			if(naVerifyf(pathTree, "We couldn't find the pathtree for a required path combination: %u", requiredKeys[i]))
			{
				u32 pathTreeDepth = GetPathDepthForPathTree(pathTree);
				if(pathTreeDepth == pathDepth)
				{
					interiorMetadata->m_PathNodeList[intMetaIdx].m_Key = requiredKeys[i];

					interiorMetadata->m_PathNodeList[intMetaIdx].m_PathNodeChildList.Resize(pathTree->GetNumChildren());

					for(s32 childIdx = 0; childIdx < pathTree->GetNumChildren(); childIdx++)
					{
						naOcclusionPathNodeChild* child = pathTree->GetChild(childIdx);			

						u32 childPathNodeKey = 0;
						if(child->pathNode)
						{
							childPathNodeKey = audNorthAudioEngine::GetOcclusionManager()->GetKeyForPathNode(child->pathNode);
						}

						naAssertf(child->portalInfo, "pathNodeChild doesn't have a portal info entry");
						s32 portalInfoIdx = GetPortalInfoIdxForPortalInfo(child->portalInfo);
						naAssertf(portalInfoIdx != INTLOC_INVALID_INDEX, "Couldn't find portalInfoIdx from the portal info");

						interiorMetadata->m_PathNodeList[intMetaIdx].m_PathNodeChildList[childIdx].m_PathNodeKey = (s32)childPathNodeKey;
						interiorMetadata->m_PathNodeList[intMetaIdx].m_PathNodeChildList[childIdx].m_PortalInfoIdx = portalInfoIdx;
					}

					intMetaIdx++;
				}
			}
		}
	}
}

u32 naOcclusionInteriorInfo::GetPathDepthForPathTree(naOcclusionPathNode* pathTree)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	u32 pathDepth = 1;
	u32 greatestChildPathDepth = 0;
	if(naVerifyf(pathTree, "Passed NULL naOcclusionPathNode to GetPathDepthForPathTree"))
	{
		for(s32 i = 0; i < pathTree->GetNumChildren(); i++)
		{
			naOcclusionPathNodeChild* pathNodeChild = pathTree->GetChild(i);
			if(pathNodeChild && pathNodeChild->pathNode)
			{
				u32 childPathDepth = GetPathDepthForPathTree(pathNodeChild->pathNode);
				if(childPathDepth > greatestChildPathDepth)
				{
					greatestChildPathDepth = childPathDepth;
				}
			}
		}
	}

	return pathDepth + greatestChildPathDepth;
}

s32 naOcclusionInteriorInfo::GetPortalInfoIdxForPortalInfo(naOcclusionPortalInfo* portalInfo)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < m_PortalInfoList.GetCount(); i++)
	{
		// Compare them because there's a good chance we could have gotten a portalinfo for the same portal but in a different naOcclusionInteriorInfo
		if(m_PortalInfoList[i]->GetUniqueInteriorProxyHashkey() == portalInfo->GetUniqueInteriorProxyHashkey() &&
			m_PortalInfoList[i]->GetRoomIdx() == portalInfo->GetRoomIdx() &&
			m_PortalInfoList[i]->GetPortalIdx() == portalInfo->GetPortalIdx())
		{
			return i;
		}
	}
	return INTLOC_INVALID_INDEX;
}

void naOcclusionInteriorInfo::GetAllRequiredPortalInfosForPathTree(naOcclusionPathNode* pathTree, atArray<naOcclusionPortalInfo*>& portalInfosUsedByPathTree)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < pathTree->GetNumChildren(); i++)
	{
		naOcclusionPathNodeChild* child = pathTree->GetChild(i);

		if(child->portalInfo)
		{
			portalInfosUsedByPathTree.PushAndGrow(child->portalInfo);
			if(child->pathNode)
			{
				GetAllRequiredPortalInfosForPathTree(child->pathNode, portalInfosUsedByPathTree);
			}
		}
	}
}

void naOcclusionInteriorInfo::GetAllRequiredKeysForPathTree(naOcclusionPathNode* pathTree, atArray<u32>& keysUsedByPathTree)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < pathTree->GetNumChildren(); i++)
	{
		naOcclusionPathNodeChild* child = pathTree->GetChild(i);

		if(child->pathNode)
		{
			u32 childPathNodeKey = audNorthAudioEngine::GetOcclusionManager()->GetKeyForPathNode(child->pathNode);
			naAssertf(childPathNodeKey != 0, "Couldn't find key for pathnode");
			keysUsedByPathTree.PushAndGrow(childPathNodeKey);
			
			GetAllRequiredKeysForPathTree(child->pathNode, keysUsedByPathTree);
		}
	}
}

naOcclusionPortalInfo* naOcclusionInteriorInfo::CreatePortalInfo(const CInteriorProxy* intProxy, const s32 roomIdx, const u32 portalIdx, const u32 destUniqueInteriorHashkey, const s32 destRoomIdx)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	naOcclusionPortalInfo* portalInfo = naOcclusionPortalInfo::Allocate();
	if(portalInfo)
	{
		portalInfo->SetInteriorProxy(intProxy);
		portalInfo->SetInteriorProxyHash(audNorthAudioEngine::GetOcclusionManager()->GetUniqueProxyHashkey(intProxy));
		portalInfo->SetRoomIdx(roomIdx);
		portalInfo->SetPortalIdx(portalIdx);
		portalInfo->SetDestInteriorHash(destUniqueInteriorHashkey);
		portalInfo->SetDestInteriorRoomIdx(destRoomIdx);

		const CInteriorInst* intInst = intProxy->GetInteriorInst();
		if(naVerifyf(intInst, "CInteriorInst is not loaded for a portalinfo that we're trying to build an entity array for"))
		{
			s32 globalPortalIdx = intInst->GetPortalIdxInRoom(roomIdx, portalIdx);
			portalInfo->SetPortalCorners(&intInst->GetPortal(globalPortalIdx));
			portalInfo->BuildPortalEntityArray();
		}
	}

	return portalInfo;
}

u32 naOcclusionInteriorInfo::GetKeyFromPathNode(naOcclusionPathNode* pathNode)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	atMap<u32, naOcclusionPathNode*>::Iterator entry = m_PathNodeMap.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); entry.Next())
	{
		naOcclusionPathNode** pathNodePtr = m_PathNodeMap.Access(entry.GetKey());
		if(pathNode && pathNodePtr && *pathNodePtr && *pathNodePtr == pathNode)
		{
			return entry.GetKey();
		}
	}
	return 0;
}

naOcclusionPathNode* naOcclusionInteriorInfo::CreatePathNodeFromKey(const u32 key)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	// First check to see if we already have this key
	naOcclusionPathNode* pathNode = GetPathNodeFromKey(key);
	if(!pathNode)
	{
		// Allocate a new root, and then store a pointer to that root in the map for easy look up
		pathNode = naOcclusionPathNode::Allocate();
		naAssertf(pathNode, "Ran out of space in the pool when building paths increase the pool limit");

		naOcclusionPathNode** pathNodePtr = &m_PathNodeMap[key];
		*pathNodePtr = pathNode;
	}

	return pathNode;
}

#endif

