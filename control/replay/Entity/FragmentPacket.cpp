//========================================================================================================================
// name:		FragmentPacket.cpp
// description:	Fragment replay packets.
//========================================================================================================================

#include "FragmentPacket.h"

#if GTA_REPLAY

#include "control/replay/replay.h"
#include "control/replay/ReplayInterfaceVeh.h"
#include "scene/world/GameWorld.h"
#include "scene/Entity.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketFragData.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPacketFragData::Store(CEntity* pEntity)
{
	PACKET_EXTENSION_RESET(CPacketFragData);
	replayAssert(pEntity && "CPacketFragData::Store: Invalid entity");
	replayAssert(pEntity->GetFragInst() && "CPacketFragData::Store: Invalid frag inst");
	replayAssert(pEntity->GetFragInst()->GetType() && "CPacketFragData::Store: Invalid frag type");

	CPacketBase::Store(PACKETID_FRAGDATA, sizeof(CPacketFragData));

	rage::fragInst* pFragInst = pEntity->GetFragInst();
	rage::fragHierarchyInst* pHierInst = NULL;

	SetPadBool(IsCachedBit, (u8)pFragInst->GetCached());
	SetPadBool(IsBrokenBit, (u8)pFragInst->GetBroken());

	if (pFragInst->GetCached())
	{
		pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
	}

	const rage::fragPhysicsLOD * pFragType = pFragInst->GetTypePhysics();

	replayFatalAssertf(pFragType->GetNumChildren() < NUMFRAGBITS, "Number of frag children is greater than %u (%u)", NUMFRAGBITS, pFragType->GetNumChildren());

	bool bDamaged	= false;
	bool bDestroyed	= false;

	m_damagedBits.Reset();
	m_destroyedBits.Reset();

	if(!pHierInst || pHierInst->anyGroupDamaged || pHierInst->groupBroken->AreAnySet())
	{
		for(u8 child = 0; child < pFragType->GetNumChildren(); ++child)
		{
			const rage::fragTypeChild* pChild = pFragType->GetChild(child);

			int groupIndex = pChild->GetOwnerGroupPointerIndex();

			if (!pHierInst || pHierInst->groupBroken->IsClear(groupIndex))
			{
				bDestroyed = false;
				bDamaged   = false;

				if (pHierInst && (pHierInst->groupInsts[groupIndex].IsDamaged()))
				{
					bDamaged = true;
				}
				else if (!pHierInst && (pFragInst->GetDamageHealth() < 0.0f))
				{
					bDamaged = true;
				}
			}
			else 
			{
				bDestroyed = true;
				bDamaged   = false;
			}

			if (bDestroyed)
			{
				m_destroyedBits.Set(child, true);
			}

			// We don't actually need the damaged bits!
			if (bDamaged)
			{
				m_damagedBits.Set(child, true);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragData::Extract(CEntity* pEntity) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketFragData) == 0);
	replayAssert(pEntity && "CPacketFragData::Load: Invalid entity");

	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if (pFragInst)
	{
		pFragInst->SetBroken(IsBroken());

		bool bCached = pFragInst->GetCached();

		if (IsCached() && !bCached)
		{
			pFragInst->PutIntoCache();
		}
		else if (!IsCached() && bCached)
		{
			rage::FRAGCACHEMGR->ReleaseCacheEntry(pFragInst->GetCacheEntry());
		}

		// We need to call this per frame as cloth can be made in a thread, so it's not necessarily present after initializing the physics (i.e. puts the frag into the cache).
		CReplayMgr::SetClothAsReplayControlled(reinterpret_cast < CPhysical * > (pEntity), pFragInst, IsCached());

		const rage::fragPhysicsLOD * pFragType = pFragInst->GetTypePhysics();

		u8 rootGroupIndex = 0;
		rage::fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
		if(pCacheEntry)
		{
			rootGroupIndex = pFragType->GetChild(pCacheEntry->GetHierInst()->rootLinkChildIndex)->GetOwnerGroupPointerIndex();
			// We need to set this so the broken parts get rendered (see CDynamicEntityDrawHandler::AddFragmentToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pParams) in DynamicEntityDrawHandler.cpp).
			if(pFragInst->GetType()->GetDamagedDrawable())
				pCacheEntry->GetHierInst()->anyGroupDamaged = IsBroken();
		}

		if (pFragType)
		{
			// When restoring a break we need to do it in reverse order (fixes url:bugstar:2038276).
			atUserArray<u8> toRestore(Alloca(u8, pFragType->GetNumChildren()), pFragType->GetNumChildren());

			for (u8 child = 0; child < pFragType->GetNumChildren(); ++child)
			{
				const rage::fragTypeChild* pChild = pFragType->GetChild(child);

				u8 groupIndex = pChild->GetOwnerGroupPointerIndex();
				if(!pCacheEntry || groupIndex == rootGroupIndex)
					continue;

				if (m_destroyedBits.IsSet(child))
				{
					if(!pFragInst->GetChildBroken(child))
						pFragInst->DeleteAbove(child);
				}
				else
				{
					if(pFragInst->GetChildBroken(child))
					{
						// Store this child ID to do this in reverse order.
						toRestore.Push(child);
					}
				}
			}

			for(s32 i = toRestore.size() -1; i >= 0; --i)
			{
				pFragInst->RestoreAbove(toRestore[i]);
			}

			if(pFragInst->GetCacheEntry())
			{
				pFragInst->GetCacheEntry()->UpdateIsFurtherBreakable();
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragData::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<IsCached>%s</IsCached>\n", IsCached() ? "Yes" : "No");
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsBroken>%s</IsBroken>\n", IsBroken() ? "Yes" : "No");
	CFileMgr::Write(handle, str, istrlen(str));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketFragData_NoDamageBits.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPacketFragData_NoDamageBits::Store(CEntity* pEntity)
{
	PACKET_EXTENSION_RESET(CPacketFragData_NoDamageBits);
	replayAssert(pEntity && "CPacketFragData_NoDamageBits::Store: Invalid entity");
	replayAssert(pEntity->GetFragInst() && "CPacketFragData_NoDamageBits::Store: Invalid frag inst");
	replayAssert(pEntity->GetFragInst()->GetType() && "CPacketFragData_NoDamageBits::Store: Invalid frag type");

	CPacketBase::Store(PACKETID_FRAGDATA_NO_DAMAGE_BITS, sizeof(CPacketFragData_NoDamageBits));

	rage::fragInst* pFragInst = pEntity->GetFragInst();
	rage::fragHierarchyInst* pHierInst = NULL;

	SetPadBool(IsCachedBit, (u8)pFragInst->GetCached());
	SetPadBool(IsBrokenBit, (u8)pFragInst->GetBroken());

	if (pFragInst->GetCached())
		pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

	const rage::fragPhysicsLOD * pFragType = pFragInst->GetTypePhysics();

	replayFatalAssertf(pFragType->GetNumChildren() <= NUMFRAGBITS_NO_DAMAGE_BITS, "Number of frag children is greater than %u (%u)", NUMFRAGBITS_NO_DAMAGE_BITS, pFragType->GetNumChildren());

	bool bDestroyed	= false;
	m_destroyedBits.Reset();
	m_ExtraBits = 0;

	if(!pHierInst || pHierInst->anyGroupDamaged || pHierInst->groupBroken->AreAnySet())
	{
		for(u8 child = 0; child < pFragType->GetNumChildren() && child < NUMFRAGBITS_NO_DAMAGE_BITS; ++child)
		{
			const rage::fragTypeChild* pChild = pFragType->GetChild(child);

			int groupIndex = pChild->GetOwnerGroupPointerIndex();

			if (!pHierInst || pHierInst->groupBroken->IsClear(groupIndex))
			{
				bDestroyed = false;
			}
			else 
			{
				bDestroyed = true;
			}

			if (bDestroyed)
			{
				m_destroyedBits.Set(child, true);
			}
		}
	}
	m_ExtraBits = 0;

	if(pHierInst)
		m_ExtraBits |= (pHierInst->anyGroupDamaged ? 1 : 0) << 0;

	// Do some validation here as this packet might be misbehaving
	ValidatePacket();
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragData_NoDamageBits::Extract(CEntity* pEntity) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketFragData_NoDamageBits) == 0);
	replayAssert(pEntity && "CPacketFragData_NoDamageBits::Load: Invalid entity");

	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if (pFragInst)
	{
		pFragInst->SetBroken(IsBroken());

		bool bCached = pFragInst->GetCached();

		if (IsCached() && !bCached)
		{
			pFragInst->PutIntoCache();
		}
		else if (!IsCached() && bCached)
		{
			rage::FRAGCACHEMGR->ReleaseCacheEntry(pFragInst->GetCacheEntry());
		}

		// We need to call this per frame as cloth can be made in a thread, so it's not necessarily present after initializing the physics (i.e. puts the frag into the cache).
		CReplayMgr::SetClothAsReplayControlled(reinterpret_cast < CPhysical * > (pEntity), pFragInst, IsCached());

		const rage::fragPhysicsLOD * pFragType = pFragInst->GetTypePhysics();

		u8 rootGroupIndex = 0;
		rage::fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
		if(pCacheEntry)
		{
			rootGroupIndex = pFragType->GetChild(pCacheEntry->GetHierInst()->rootLinkChildIndex)->GetOwnerGroupPointerIndex();

			if(pCacheEntry->GetHierInst())
				pCacheEntry->GetHierInst()->anyGroupDamaged = ((m_ExtraBits & (1 << 0)) == 1) ? true : false;
		}

		if (pFragType)
		{
			// When restoring a break we need to do it in reverse order (fixes url:bugstar:2038276).
			atUserArray<u8> toRestore(Alloca(u8, pFragType->GetNumChildren()), pFragType->GetNumChildren());

			for (u8 child = 0; child < pFragType->GetNumChildren(); ++child)
			{
				const rage::fragTypeChild* pChild = pFragType->GetChild(child);

				u8 groupIndex = pChild->GetOwnerGroupPointerIndex();
				if(!pCacheEntry || groupIndex == rootGroupIndex)
					continue;

				if (m_destroyedBits.IsSet(child))
				{
					if(!pFragInst->GetChildBroken(child))
						pFragInst->DeleteAbove(child);
				}
				else
				{
					if(pFragInst->GetChildBroken(child))
					{
						// Store this child ID to do this in reverse order.
						toRestore.Push(child);
					}
				}
			}

			for(s32 i = toRestore.size() -1; i >= 0; --i)
			{
				pFragInst->RestoreAbove(toRestore[i]);
			}

			if(pFragInst->GetCacheEntry())
			{
				pFragInst->GetCacheEntry()->UpdateIsFurtherBreakable();
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragData_NoDamageBits::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<IsCached>%s</IsCached>\n", IsCached() ? "Yes" : "No");
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsBroken>%s</IsBroken>\n", IsBroken() ? "Yes" : "No");
	CFileMgr::Write(handle, str, istrlen(str));
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPacketFragBoneData.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CPacketFragBoneData::Store(CEntity* pEntity)
{
	PACKET_EXTENSION_RESET(CPacketFragBoneData);
	replayAssert(pEntity && "CPacketFragBoneData::Store: Invalid entity");
	replayAssert(pEntity->GetFragInst() && "CPacketFragBoneData::Store: Invalid frag inst");

	CPacketBase::Store(PACKETID_FRAGBONEDATA,	sizeof(CPacketFragBoneData));

	m_numUndamagedBones	= 0;
	m_numDamagedBones	= 0;

	if(ShouldStoreHighResBones(pEntity) == false)
	{
		tBoneData* pBones = NULL;
		tBoneData* pDamagedBones = NULL;
		rage::fragInst* pFragInst = pEntity->GetFragInst();
		if (pFragInst)
		{
			fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

			if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
			{
				pBones		= GetBones(0);
				const crSkeleton* pSkeleton = pFragCacheEntry->GetHierInst()->skeleton;
				m_numUndamagedBones = (u16)pSkeleton->GetBoneCount();
				for (s32 boneIndex = 0; boneIndex < m_numUndamagedBones; boneIndex++)
				{
					StoreBone(pBones, pFragCacheEntry->GetHierInst()->skeleton, boneIndex);
				}

				AddToPacketSize(m_numUndamagedBones * sizeof(tBoneData));

				if (pFragCacheEntry->GetHierInst()->damagedSkeleton)
				{
					pDamagedBones = GetDamagedBones(0);
					m_numDamagedBones = (u16)pFragCacheEntry->GetHierInst()->damagedSkeleton->GetBoneCount();
					for (s32 boneIndex = 0; boneIndex < m_numDamagedBones; boneIndex++)
					{
 						StoreBone(pDamagedBones, pFragCacheEntry->GetHierInst()->damagedSkeleton, boneIndex);
					}

					AddToPacketSize(m_numDamagedBones * sizeof(tBoneData));
				}
			}
		}
	}

#if REPLAY_GUARD_ENABLE
	AddToPacketSize(sizeof(int));

	int* pEnd = (int*)((u8*)this + GetPacketSize() - sizeof(int));
	*pEnd = REPLAY_GUARD_END_DATA;
#endif // REPLAY_GUARD_ENABLE
}


void CPacketFragBoneData::StoreExtensions(CEntity* pEntity)
{
	FRAG_BONE_DATA_EXTENSIONS *pExtensions = NULL;
	void *pAfterExtensions = NULL;

	if(ShouldStoreHighResBones(pEntity) == true)
	{
		rage::fragInst* pFragInst = pEntity->GetFragInst();

		if (pFragInst)
		{
			fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

			if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
			{
				const crSkeleton* pSkeleton = pFragCacheEntry->GetHierInst()->skeleton;
				pExtensions = (FRAG_BONE_DATA_EXTENSIONS *)BeginExtensionWrite();

				m_numUndamagedBones = (u16)pSkeleton->GetBoneCount();
				tHighResBoneData *pUndamaged = (tHighResBoneData *)(pExtensions + 1);
				pAfterExtensions = (void *)&pUndamaged[m_numUndamagedBones];
				pExtensions->m_HighResUndamagedBones_Offset = (u32)((ptrdiff_t)pUndamaged - (ptrdiff_t)pExtensions);

				for (s32 boneIndex = 0; boneIndex < m_numUndamagedBones; boneIndex++)
				{
					StoreHighResBone(pUndamaged, pFragCacheEntry->GetHierInst()->skeleton, boneIndex);
				}

				if (pFragCacheEntry->GetHierInst()->damagedSkeleton)
				{
					m_numDamagedBones = (u16)pFragCacheEntry->GetHierInst()->damagedSkeleton->GetBoneCount();
					tHighResBoneData *pDamaged = (tHighResBoneData *)pAfterExtensions;
					pAfterExtensions = (void *)&pDamaged[m_numDamagedBones];
					pExtensions->m_HighResDamagedBones_Offset = (u32)((ptrdiff_t)pDamaged - (ptrdiff_t)pExtensions);

					for (s32 boneIndex = 0; boneIndex < m_numDamagedBones; boneIndex++)
					{
						StoreHighResBone(pDamaged, pFragCacheEntry->GetHierInst()->damagedSkeleton, boneIndex);
					}
				}
			}
		}
	}

	// Use this to open new extensions. DON'T USE BEGIN_PACKET_EXTENSION_WRITE() etc.
	//pExtensions = (FRAG_BONE_DATA_EXTENSIONS *)BeginExtensionWrite();

	if(pExtensions)
	{
		// Can write new extension data here...But maintain pAfterExtensions pointer - see below.
	}

	if(pAfterExtensions)
	{
		replayAssertf(pExtensions, "CPacketFragBoneData::StoreExtensions()...No extensions open");
		EndExtensionWrite(pAfterExtensions);
	}
}


CPacketFragBoneData::FRAG_BONE_DATA_EXTENSIONS *CPacketFragBoneData::BeginExtensionWrite()
{
	FRAG_BONE_DATA_EXTENSIONS *pRet = (FRAG_BONE_DATA_EXTENSIONS *)BEGIN_PACKET_EXTENSION_WRITE(1, CPacketFragBoneData);
	pRet->m_HighResDamagedBones_Offset = 0;
	pRet->m_HighResUndamagedBones_Offset = 0;
	return pRet;
}


void CPacketFragBoneData::EndExtensionWrite(void *pAddr)
{
	END_PACKET_EXTENSION_WRITE(pAddr, CPacketFragBoneData);
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragBoneData::StoreBone(tBoneData* pBones, rage::crSkeleton* pSkeleton, s32 boneID)
{
	Mat34V mat;
	replayAssert(pSkeleton && "CPacketFragBoneData::Store: Invalid skeleton");
	pSkeleton->GetGlobalMtx(boneID, mat);
	pBones[boneID].StoreMatrix(MAT34V_TO_MATRIX34(mat));
}


void CPacketFragBoneData::StoreHighResBone(tHighResBoneData* pBones, rage::crSkeleton* pSkeleton, s32 boneID)
{
	Mat34V mat;
	replayAssert(pSkeleton && "CPacketFragBoneData::Store: Invalid skeleton");
	pSkeleton->GetGlobalMtx(boneID, mat);
	pBones[boneID].StoreMatrix(MAT34V_TO_MATRIX34(mat));
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragBoneData::Extract(CEntity* pEntity, CPacketFragBoneData const* pNextPacket, float fInterpValue, bool bUpdateWorld) const
{
	replayAssert(pEntity);

	tBoneData* pBones = NULL;
	tBoneData* pDamagedBones = NULL;
	tBoneData* pNextBones = NULL;
	tBoneData* pNextDamagedBones = NULL;

	// Set up for low-res extraction.
	pBones = GetBones();
	pDamagedBones = GetDamagedBones();

	if(pNextPacket)
	{
		pNextBones = pNextPacket->GetBones();
		pNextDamagedBones = pNextPacket->GetDamagedBones();
	}

	tHighResBoneData* pBonesHR = NULL;
	tHighResBoneData* pDamagedBonesHR = NULL;
	tHighResBoneData* pNextBonesHR = NULL;
	tHighResBoneData* pNextDamagedBonesHR = NULL;

	// Detect presence of high res. bone info.
	if(GET_PACKET_EXTENSION_VERSION(CPacketFragBoneData))
	{
		FRAG_BONE_DATA_EXTENSIONS *pExtensions = (FRAG_BONE_DATA_EXTENSIONS *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketFragBoneData);

		pBonesHR = pExtensions->GetUndamagedBones();
		pDamagedBonesHR = pExtensions->GetDamagedBones();

		if(pNextPacket)
		{
			FRAG_BONE_DATA_EXTENSIONS *pExtensionsNext = (FRAG_BONE_DATA_EXTENSIONS *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketFragBoneData);
			pNextBonesHR = pExtensionsNext->GetUndamagedBones();
			pNextDamagedBonesHR = pExtensionsNext->GetDamagedBones();
		}

		// Cancel low res. extraction if we find high res. bones.
		if(pBonesHR || pDamagedBonesHR)
		{
			pBones = NULL;
			pNextBones = NULL;
			pDamagedBones = NULL;
			pNextDamagedBones = NULL;
		}
	}

	if (pEntity)
	{
		rage::fragInst* pFragInst = pEntity->GetFragInst();

		if (pFragInst)
		{
			fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

			if (pFragCacheEntry)
			{
				rage::fragHierarchyInst* pHierInst = pFragCacheEntry->GetHierInst();

				Matrix34 mat;
	
				
				const rage::crSkeletonData *pSkelData = &pEntity->GetSkeleton()->GetSkeletonData();
				u16 boneCount = Min(m_numUndamagedBones, (u16)pSkelData->GetNumBones());
				for(u16 i = 0; i < boneCount; ++i)
				{
					if (pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterpValue == 0.0f)
					{
						if(pBones)
							pBones[i].LoadMatrix(mat);
						else
							pBonesHR[i].LoadMatrix(mat);

						pHierInst->skeleton->SetGlobalMtx((u32)i, MATRIX34_TO_MAT34V(mat));
					}
					else if (pNextPacket)
					{
						if(pBones)
							pBones[i].LoadMatrix(mat, pNextBones[i], fInterpValue);
						else
							pBonesHR[i].LoadMatrix(mat, pNextBonesHR[i], fInterpValue);

						pHierInst->skeleton->SetGlobalMtx((u32)i, MATRIX34_TO_MAT34V(mat));
					}
					else
					{
						replayAssert(0 && "CPacketFragBoneData::Load - interpolation couldn't find a next packet");
					}
				}

				boneCount = Min(m_numDamagedBones, (u16)pSkelData->GetNumBones());
				for(u16 i = 0; i < boneCount; ++i)
				{
					if (pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterpValue == 0.0f)
					{
						if(pDamagedBones)
							pDamagedBones[i].LoadMatrix(mat);
						else
							pDamagedBonesHR[i].LoadMatrix(mat);

						pHierInst->damagedSkeleton->SetGlobalMtx((u32)i, MATRIX34_TO_MAT34V(mat));
					}
					else if (pNextPacket)
					{
						if(pDamagedBones)
							pDamagedBones[i].LoadMatrix(mat, pNextDamagedBones[i], fInterpValue);
						else
							pDamagedBonesHR[i].LoadMatrix(mat, pNextDamagedBonesHR[i], fInterpValue);

						pHierInst->damagedSkeleton->SetGlobalMtx((u32)i, MATRIX34_TO_MAT34V(mat));
					}
					else
					{
						replayAssert(0 && "CPacketFragBoneData::Load - interpolation couldn't find a next packet");
					}
				}

				if (bUpdateWorld)
				{
					CGameWorld::Remove(pEntity);
					CGameWorld::Add(pEntity, CGameWorld::OUTSIDE);
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketFragBoneData::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<m_numUndamagedBones,m_numDamagedBones>%u, %u</m_numUndamagedBones,m_numDamagedBones>\n", m_numUndamagedBones, m_numDamagedBones);
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////

u32 CPacketFragBoneData::ms_HighResWhiteList[] = 
{
	ATSTRINGHASH("prop_ld_dstplanter_01", 0xdf8c1fd4),
	ATSTRINGHASH("prop_fruitstand_b",0x333332ed),
	0xffffffff
};


bool CPacketFragBoneData::ShouldStoreHighResBones(const CEntity *pEntity)
{
	if(pEntity && pEntity->GetArchetype())
	{
		u32 idx = 0;
		u32 modelHash = pEntity->GetArchetype()->GetModelNameHash();

		while(ms_HighResWhiteList[idx] != 0xffffffff)
		{
			if(ms_HighResWhiteList[idx++] == modelHash)
				return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
/*
s32 CPacketFragBoneData::EstimatePacketSize(const CEntity* pEntity)
{
	replayAssert(pEntity && "CPacketFragBoneData::EstimatePacketSize: Invalid entity");

	rage::fragInst* pFragInst = pEntity->GetFragInst();

	if (pFragInst)
	{
		fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

		if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
		{
			s32 numberOfBones	 = pFragCacheEntry->GetHierInst()->skeleton->GetBoneCount();
			s32 fragBoneDataSize = numberOfBones * sizeof(tBoneData);
			if (pFragCacheEntry->GetHierInst()->damagedSkeleton)
			{
				numberOfBones = pFragCacheEntry->GetHierInst()->damagedSkeleton->GetBoneCount();
				fragBoneDataSize += numberOfBones * sizeof(tBoneData);
			}

#if REPLAY_GUARD_ENABLE
			// Adding an int on the end as a guard
			fragBoneDataSize += sizeof(int);
#endif // REPLAY_GUARD_ENABLE
			return fragBoneDataSize + sizeof(CPacketFragBoneData);	
		}
	}
	return 0;
}
*/

//////////////////////////////////////////////////////////////////////////
// CSkeletonBoneData.
//////////////////////////////////////////////////////////////////////////

template<> eReplayPacketId CSkeletonBoneData<CPacketQuaternionL>::PacketID = PACKETID_SKELETONBONEDATA;
template<> eReplayPacketId CSkeletonBoneData<CPacketQuaternionH>::PacketID = PACKETID_SKELETONBONEDATA_HIGHQUALITY;

template<> tFragmentDeferredQuaternionBuffer CSkeletonBoneData<CPacketQuaternionL>::sm_DeferredQuaternionBuffer2 = tFragmentDeferredQuaternionBuffer();
template<> tFragmentDeferredQuaternionBuffer CSkeletonBoneData<CPacketQuaternionH>::sm_DeferredQuaternionBuffer2 = tFragmentDeferredQuaternionBuffer();

template<> eReplayPacketId CPacketFragBoneDataUsingSkeletonPackets<CPacketQuaternionL>::PacketID = PACKETID_FRAGBONEDATA_USINGSKELETONPACKETS;
template<> eReplayPacketId CPacketFragBoneDataUsingSkeletonPackets<CPacketQuaternionH>::PacketID = PACKETID_FRAGBONEDATA_USINGSKELETONPACKETS_HIGHQUALITY;





#endif // GTA_REPLAY
