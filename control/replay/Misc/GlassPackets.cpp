#include "GlassPackets.h"


#if GTA_REPLAY

#include "physics/physics.h"
#include "breakableglass/glassmanager.h"
#include "control/replay/ReplayExtensions.h"

ASSERT_ONLY(void CheckWillFitInU8(int val) { replayAssertf(val >= 0 && val <= 255, "Cannot be compressed into a U8"); })

CPacketCreateBreakableGlass::CPacketCreateBreakableGlass(int groupIndex, int boneIndex, Vec3V_In position, Vec3V_In impact, int glassCrackType)
	:  CPacketEvent(PACKETID_GLASS_CREATEBREAKABLEGLASS, sizeof(CPacketCreateBreakableGlass))
	, m_groupIndex(static_cast<u8>(groupIndex))
	, m_boneIndex(static_cast<u8>(boneIndex))
	, m_glassCrackType(static_cast<u8>(glassCrackType))
{
	ASSERT_ONLY(CheckWillFitInU8(groupIndex));
	ASSERT_ONLY(CheckWillFitInU8(boneIndex));
	ASSERT_ONLY(CheckWillFitInU8(glassCrackType));

	m_position.Store(position);
	m_impact.Store(impact);
}

void CPacketCreateBreakableGlass::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pGlassObject = eventInfo.GetEntity();
	
	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		//if we've not got a cached entry we'll need one
		if(pGlassObject->GetFragInst()->GetCached() == false)
		{
			pGlassObject->GetFragInst()->PutIntoCache();
		}

		Vec3V postion; m_position.Load(postion);
		Vec3V impact; m_impact.Load(impact);

		//make sure the glass panel is reset so we can break it
		replayGlassManager::ResetGlassOnBone(pGlassObject, m_groupIndex);

		//ensure matrix is valid, otherwise ShatterGlassOnBone won't function correctly
		pGlassObject->GetFragInst()->GetSkeleton()->PartialReset(m_boneIndex);
		
		//break the glass
		pGlassObject->GetFragInst()->GetCacheEntry()->ShatterGlassOnBone(m_groupIndex, m_boneIndex, postion, impact, m_glassCrackType);

		//Dont try to hide dead groups if its already dead, causes asserts trying to remove object with invalid level id.
		if( !pGlassObject->GetFragInst()->GetDead() )
			pGlassObject->GetFragInst()->HideDeadGroup(m_groupIndex, NULL);

		pGlassObject->GetFragInst()->PoseSkeletonFromArticulatedBody();
	}
	else
	{
		replayGlassManager::ResetGlassOnBone(pGlassObject, m_groupIndex);
	}
}

bool CPacketCreateBreakableGlass::HasExpired(const CEventInfo<CObject>& eventInfo) const
{
	return replayGlassManager::HasValidGlass(eventInfo.GetEntity(), m_groupIndex) == false;
}

CPacketHitGlass::CPacketHitGlass(int groupIndex, int crackType, Vec3V_In position, Vec3V_In impulse)
:  CPacketEvent(PACKETID_GLASS_HIT, sizeof(CPacketHitGlass))
, m_crackType(static_cast<u8>(crackType))
, m_groupIndex(static_cast<u8>(groupIndex))
{
	ASSERT_ONLY(CheckWillFitInU8(crackType));
	ASSERT_ONLY(CheckWillFitInU8(groupIndex));

	m_impulse.Store(impulse);
	m_position.Store(position);
}

void CPacketHitGlass::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pGlassObject = eventInfo.GetEntity();

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		Vec3V postion; m_position.Load(postion);
		Vec3V impulse; m_impulse.Load(impulse);

		replayAssertf(pGlassObject->GetFragInst()->GetCached(), "This glass should already has a cache entry");

		//make sure we have a valid cached in case the creation failed for some reason
		if(pGlassObject->GetFragInst() && pGlassObject->GetFragInst()->GetCached())
		{
			//find the glass associated with this group and add the hit
			bgGlassHandle handle = replayGlassManager::GetGlassHandleFromGroup(pGlassObject->GetFragInst()->GetCacheEntry()->GetHierInst(), m_groupIndex);
	
			//if the glass has been recycled then we can't do the hit
			if(handle != bgGlassHandle_Invalid)
			{
				bgGlassManager::HitGlass(handle, m_crackType, m_position, m_impulse);
			}
		}
	}
}

bool CPacketHitGlass::HasExpired(const CEventInfo<CObject>& eventInfo) const
{
	return replayGlassManager::HasValidGlass(eventInfo.GetEntity(), m_groupIndex) == false;
}

CPacketTransferGlass::CPacketTransferGlass(int groupIndex)
	:  CPacketEvent(PACKETID_GLASS_TRANSFER, sizeof(CPacketHitGlass))
	, m_groupIndex(static_cast<u8>(groupIndex))
{
	ASSERT_ONLY(CheckWillFitInU8(groupIndex));
}

void CPacketTransferGlass::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pGlassObjectSource = eventInfo.GetEntity(0);
	CObject* pGlassObjectDest = eventInfo.GetEntity(1);

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) &&
		pGlassObjectSource != NULL && pGlassObjectDest != NULL)
	{
		replayAssertf(pGlassObjectSource->GetFragInst()->GetCached(), "The source object should already has a cache entry");

		fragHierarchyInst* pSourceHier = pGlassObjectSource->GetFragInst()->GetCacheEntry()->GetHierInst();

		//make sure the dest object is in the cache
		if(pGlassObjectDest->GetFragInst()->GetCached() == false)
		{
			pGlassObjectDest->GetFragInst()->PutIntoCache();
		}

		fragHierarchyInst::GlassInfo* glassInfo = replayGlassManager::GetGlassInfoFromGroup(pSourceHier, m_groupIndex);

		//if the handle is valid transfer the glass across
		if(glassInfo && glassInfo->handle != bgGlassHandle_Invalid)
		{
			fragCacheEntry* dstFragCache = pGlassObjectDest->GetFragInst()->GetCacheEntry();

			dstFragCache->AllocateGlassInfo();

			fragHierarchyInst::GlassInfo& newGlassInfo = dstFragCache->GetHierInst()->glassInfos[dstFragCache->GetHierInst()->numGlassInfos++];

			newGlassInfo = *glassInfo;
			bgGlassManager::TransferBreakableGlassOwner(&glassInfo->handle, &newGlassInfo.handle, dstFragCache->GetInst());
		}
	}
}

bool CPacketTransferGlass::HasExpired(const CEventInfo<CObject>& eventInfo) const
{
	return eventInfo.GetEntity(0) && replayGlassManager::HasValidGlass(eventInfo.GetEntity(1), m_groupIndex) == false;
}

void replayGlassManager::OnCreateBreakableGlass(const phInst* pInst, int groupIndex, int boneIndex, Vec3V_In position, Vec3V_In impact, int glassCrackType) 
{
	if(CReplayMgr::ShouldRecord() && pInst)
	{
		const CObject* pHitObject = GetObjectFromInst(pInst);

		CReplayMgr::RecordFx<CPacketCreateBreakableGlass>(
			CPacketCreateBreakableGlass(groupIndex, boneIndex, position, impact, glassCrackType),
			pHitObject);
	}
}

void replayGlassManager::OnHitGlass(bgGlassHandle handle, int crackType, Vec3V_In position, Vec3V_In impulse)
{
	if(CReplayMgr::ShouldRecord() && handle != bgGlassHandle_Invalid)
	{
		const CObject* pHitObject = GetObjectFromInst(bgGlassManager::GetGlassPhysicsInst(handle));
		int groupIndex = GetGroupFromGlassHandle(pHitObject->GetFragInst()->GetCacheEntry()->GetHierInst(), handle);

			CReplayMgr::RecordFx<CPacketHitGlass>(
				CPacketHitGlass(groupIndex, crackType,  position, impulse),
				pHitObject);
	}
}

void replayGlassManager::OnTransferGlass(const phInst* pSourceInst, const phInst* pDestInst, u16 groupIndex)
{
	if(CReplayMgr::ShouldRecord())
	{
		const CObject* pSourceObject = GetObjectFromInst(pSourceInst);
		const CObject* pDestObject = GetObjectFromInst(pDestInst);

		CReplayMgr::RecordFx<CPacketTransferGlass>(
			CPacketTransferGlass(groupIndex),
			pSourceObject, pDestObject);

	}
}

int replayGlassManager::GetGroupFromGlassHandle(const fragHierarchyInst* pGlassObjectHierInst, bgGlassHandle glassHandle)
{
	int groupIndex = -1;

	//work out the group index by searching through the glass handles allocated to this object
	for(u32 i = 0; i < pGlassObjectHierInst->numGlassInfos; ++i)
	{
		const fragHierarchyInst::GlassInfo& glassInfo = pGlassObjectHierInst->glassInfos[i];
		if(glassInfo.handle == glassHandle)
		{
			groupIndex = glassInfo.groupIndex;
		}
	}
	replayAssertf(groupIndex != -1, "Couldn't find a matching group for this glass handle");

	return groupIndex;
}

bgGlassHandle replayGlassManager::GetGlassHandleFromGroup(const fragHierarchyInst* pGlassObjectHierInst, int groupIndex)
{
	fragHierarchyInst::GlassInfo* glassGroup = GetGlassInfoFromGroup(pGlassObjectHierInst, groupIndex);
	return glassGroup != NULL ? glassGroup->handle :  bgGlassHandle_Invalid;
}

fragHierarchyInst::GlassInfo* replayGlassManager::GetGlassInfoFromGroup(const fragHierarchyInst* pGlassObjectHierInst, int groupIndex)
{
	//work out the glass info by searching through the glass manager allocated to this object
	for(u32 i = 0; i < pGlassObjectHierInst->numGlassInfos; ++i)
	{
		fragHierarchyInst::GlassInfo& glassInfo = pGlassObjectHierInst->glassInfos[i];
		if(glassInfo.groupIndex == groupIndex)
		{
			return &glassInfo;
		}
	}

	return NULL;
}

const CObject* replayGlassManager::GetObjectFromInst(const phInst* pInst)
{
	const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
	replayFatalAssertf(pEntity && pEntity->GetIsTypeObject(), "Couldn't get object for glass hit recording");
	return static_cast<const CObject*>(pEntity);
}

void replayGlassManager::ResetGlassOnBone(const CObject* pGlassObject, int groupIndex)
{
	//clear up any breakable glass that might be present
	if(pGlassObject->GetFragInst()->GetCached())
	{
		fragHierarchyInst* pHierInst = pGlassObject->GetFragInst()->GetCacheEntry()->GetHierInst();

		for(u32 i = 0; i < pHierInst->numGlassInfos; ++i)
		{
			fragHierarchyInst::GlassInfo& glassInfo = pHierInst->glassInfos[i];
			if(glassInfo.handle != bgGlassHandle_Invalid && glassInfo.groupIndex == groupIndex)
			{
				bgGlassManager::DestroyBreakableGlass(glassInfo.handle);
				glassInfo.handle = bgGlassHandle_Invalid;
			}
		}
	}
}

bool replayGlassManager::HasValidGlass(const CObject* pGlassObject, int groupIndex)
{
	bool hasValidGlass = false;

	if(pGlassObject && pGlassObject->GetFragInst() && pGlassObject->GetFragInst()->GetCached())
	{
		bgGlassHandle handle = replayGlassManager::GetGlassHandleFromGroup(pGlassObject->GetFragInst()->GetCacheEntry()->GetHierInst(), groupIndex);
		hasValidGlass = handle != bgGlassHandle_Invalid;
	}

	return hasValidGlass;
}

#endif // GTA_REPLAY
