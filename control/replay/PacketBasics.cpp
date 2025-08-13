#include "PacketBasics.h"

#if GTA_REPLAY

#include "camera/CamInterface.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/Replay.h"
#include "control/replay/replayinteriormanager.h"
#include "control/replay/ReplayInternal.h"
#include "scene/world/GameWorld.h"
#include "scene/Physical.h"
#include "string/stringhash.h"
#include "Peds/Ped.h"
#include "fwdebug/picker.h"


converter<u8> u8_f32_0_to_3_0(		0,		255,	   0.0f,	  3.0f);
converter<u8> u8_f32_0_to_5(		0,		255,	   0.0f,	  5.0f);
converter<s8> s8_f32_n127_to_127(	-127,	127,	-127.0f,	127.0f);
converter<u8> u8_f32_0_to_65(		0,		255,	   0.0f,	 65.0f);
converter<s16> s16_f32_n650_to_650( -32767,	32767,	-650.0f,	650.0f);
converter<s8> s8_f32_n1_to_1(		-127,	127,	  -1.0f,	  1.0f);
converter<u8> u8_f32_0_to_2(		0,		255,	   0.0f,	  2.0f);
converter<s8> s8_f32_nPI_to_PI(		-127,	127,	    -PI,	    PI);
converter<s8> s8_f32_n20_to_20(		-127,	127,	 -20.0f,	 20.0f);
converter<s16> s16_f32_n4_to_4(		-32767,	32767,	  -4.0f,	  4.0f);
converter<s16> s16_f32_n35_to_35(	-32767,	32767,	 -35.0f,	 35.0f);

// float valCompTracked<s8, s8_float_n1_75_to_1_75>::m_maxValue = -FLT_MAX;	// Yes these intentionally look like they're the wrong way round.
// float valCompTracked<s8, s8_float_n1_75_to_1_75>::m_minValue = FLT_MAX;

CReplayID CEntityChecker::ResetVal				= ReplayIDInvalid;
CReplayID CEntityCheckerAllowNoEntity::ResetVal = NoEntityID;


//////////////////////////////////////////////////////////////////////////

void *CPacketExtension::BeginWrite(class CPacketBase *pBase, u8 version)
{
	SetVersion(version);

	// Align data to be 4 byte boundary from packet base.
	u32 alignedPacketSize = REPLAY_ALIGN(pBase->GetPacketSize(), PACKET_EXTENSION_DEFAULT_ALIGNMENT);
	u32 alignPadding = alignedPacketSize - pBase->GetPacketSize();
	pBase->AddToPacketSize((u32)alignPadding);

	// Guard data lives at start of extension data.
	PACKET_EXTENSION_GUARD *pGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pBase + pBase->GetPacketSize());
	// Set the offset to be where the guard data lives.
	SetOffset(pBase->GetPacketSize());

	// User data is directly after guard data.
	return (void *)(pGuard + 1);
}


void CPacketExtension::EndWrite(class CPacketBase *pBase, u8 *pPtr)
{
	PACKET_EXTENSION_GUARD *pGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pBase + GetOffset());
	// Calculate increase in packet size (inc. guard info).
	ptrdiff_t packetIncrease = (ptrdiff_t)pPtr - (ptrdiff_t)pGuard;
	pBase->AddToPacketSize((u32)REPLAY_ALIGN(packetIncrease, PACKET_EXTENSION_DEFAULT_ALIGNMENT));

	// Set the amount written.
	pGuard->m_DataSize = (u32)packetIncrease - (u32)sizeof(PACKET_EXTENSION_GUARD);
#if __ASSERT
	// Calculate a hash of written data.
	pGuard->m_CRCOfData = atDataHash((const char *)(pGuard+1), pGuard->m_DataSize, 0);
#else //__ASSERT
	pGuard->m_CRCOfData = 0;
#endif //__ASSERT
}


void *CPacketExtension::GetReadPtr(class CPacketBase const*pBase) const
{
	PACKET_EXTENSION_GUARD *pGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pBase + GetOffset());
	void *pRet = (void *)(pGuard + 1);
	replayAssertf((pGuard->m_CRCOfData == 0) || (atDataHash((const char *)pRet, pGuard->m_DataSize, 0) == pGuard->m_CRCOfData), "CPacketExtension::GetReadPtr()...Extensions data is not valid!");
	return pRet;
}


bool CPacketExtension::Validate(class CPacketBase const*pBase) const
{
	if(GetVersion() != 0)
	{
		PACKET_EXTENSION_GUARD *pGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pBase + GetOffset());
		void *pRet = (void *)(pGuard + 1);
		return (pGuard->m_CRCOfData == 0) || (atDataHash((const char *)pRet, pGuard->m_DataSize, 0) == pGuard->m_CRCOfData);
	}
	return true;
}


u32 CPacketExtension::GetTotalDataSize(class CPacketBase const*pBase) const
{
	if(GetVersion() != 0)
	{
		PACKET_EXTENSION_GUARD *pGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pBase + GetOffset());
		return pGuard->m_DataSize + sizeof(PACKET_EXTENSION_GUARD);
	}
	return 0;
}

void CPacketExtension::CloneExt(class CPacketBase const* pSrc, class CPacketBase *pDst, CPacketExtension& pDstExt) const
{
	if(GetVersion() != 0)
	{
		void* dataDst = pDstExt.BeginWrite(pDst, GetVersion());
		void* dataSource = GetReadPtr(pSrc);

		s32 dataSize = GetTotalDataSize(pSrc) - (u32)sizeof(PACKET_EXTENSION_GUARD);
		replayAssertf(dataSize > 0, "Error cloning packet extension data, data size was less than zero");

		sysMemCpy(dataDst, dataSource, dataSize);

		pDstExt.EndWrite(pDst, (u8*)dataDst + dataSize);

		replayAssertf(pDstExt.Validate(pDst), "Error cloning packet extension data, validate failed");

#if __ASSERT
		PACKET_EXTENSION_GUARD *pSrcGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pSrc + GetOffset());
		PACKET_EXTENSION_GUARD *pDstGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pDst + pDstExt.GetOffset());

		replayAssertf(pSrcGuard->m_CRCOfData == pDstGuard->m_CRCOfData, "Error copying packet cloning data, CRC didn't match");
#endif
	}
}

//If we want to modify any extension data we need to recalculate the CRC
void CPacketExtension::RecomputeDataCRC(class CPacketBase *pBase)
{
	PACKET_EXTENSION_GUARD *pGuard = (PACKET_EXTENSION_GUARD *)((u8 *)pBase + GetOffset());
#if __ASSERT
	// Calculate a hash of written data.
	pGuard->m_CRCOfData = atDataHash((const char *)(pGuard+1), pGuard->m_DataSize, 0);
#else //__ASSERT
	pGuard->m_CRCOfData = 0;
#endif //__ASSERT
}

//////////////////////////////////////////////////////////////////////////
void CBasicEntityPacketData::Store(const CPhysical* pEntity, const CReplayID& replayID)
{
	CPacketEntityInfo::Store(replayID);

	// Portal
	m_inInterior	= pEntity->GetIsInInterior();
	m_isVisible		= (!pEntity->IsRetainedFlagSet() && pEntity->GetOwnerEntityContainer() != NULL) && (pEntity->GetIsVisible() || pEntity->GetIsVisibleIgnoringModule(SETISVISIBLE_MODULE_FIRST_PERSON));
	m_isVisibleInReplayCamera = pEntity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_GBUF|VIS_PHASE_MASK_SEETHROUGH_MAP);

	// Special case for peds, which might have the CPED_RESET_FLAG_DontRenderThisFrame on, if so, we're invisible.
	if( pEntity->GetIsTypePed() )
	{
		const CPed *pPed = reinterpret_cast<const CPed*>(pEntity);
		if( pPed->GetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame) )
		{
			m_isVisible = false;
		}
	}

	m_createUrgent	= false;

	m_fadeOutForward	= pEntity->GetLodData().GetFadeToZero();
	m_fadeOutBackward	= 0;
	
	m_interiorLocation.MakeInvalid();
	if (m_inInterior)
	{
		//Save the required info for the entity location.
		fwInteriorLocation interiorLocation = pEntity->GetInteriorLocation();
		m_interiorLocation.SetInterior(interiorLocation);
	}	

	m_lodDistance		= (u16)pEntity->GetLodDistance();

	m_posAndRot.StoreMatrix(MAT34V_TO_MATRIX34(pEntity->GetMatrix()));

	m_wasCutsceneOwned = false;
	if(pEntity && pEntity->GetPortalTracker())
	{
		m_wasCutsceneOwned = pEntity->GetPortalTracker()->GetIsCutsceneControlled();
	}

	m_WarpedThisFrame = pEntity->m_nDEflags.bReplayWarpedThisFrame;
	m_VersionUsesWarping = true;
}

//////////////////////////////////////////////////////////////////////////
void CBasicEntityPacketData::PostStore(const CPhysical* pEntity, const FrameRef& currentFrameRef, const u16 previousBlockFrameCount)
{
	// This relies on the creation frame of the entity being correct.  However
	// we don't necessarily know this until the packet is actually stored in
	// the replay buffer itself (after we've figured out the block).
	FrameRef creationFrame;
	pEntity->GetCreationFrameRef(creationFrame);

	u16 age = currentFrameRef.GetFrame() - creationFrame.GetFrame();
	if((currentFrameRef.GetBlockIndex() - creationFrame.GetBlockIndex()) == 1)
	{
		// Entity was created in the previous block
		age = currentFrameRef.GetFrame() + (previousBlockFrameCount - creationFrame.GetFrame());
	}
	else if((currentFrameRef.GetBlockIndex() - creationFrame.GetBlockIndex()) > 1)
	{
		// Created more than 1 block ago....don't fade
		age = CEntityData::NUM_FRAMES_TO_FADE;
	}

	m_fadeOutBackward	= (age < CEntityData::NUM_FRAMES_TO_FADE) && (CReplayMgrInternal::GetCurrentSessionFrameRef().GetFrame() > CEntityData::NUM_FRAMES_TO_FADE);
}


//////////////////////////////////////////////////////////////////////////
void CBasicEntityPacketData::Extract(CPhysical* pEntity, CBasicEntityPacketData const* pNextPacketData, float interp, const CReplayState& flags, bool DoRemoveAndAdd, bool ignoreWarpingFlags ) const
{

	// Was intended to make entities who're about to be cleaned up by replay fade out, but in many cases this makes them vanish too early (hence B*1934159)
	// Left for future reference.
/*
	if(flags.IsSet(REPLAY_CURSOR_JUMP) == false)
	{
		if(flags.IsSet(REPLAY_DIRECTION_FWD))
		{
			pEntity->GetLodData().SetFadeToZero(GetFadeOutForward());
		}
		else if(flags.IsSet(REPLAY_DIRECTION_BACK))
		{
			pEntity->GetLodData().SetFadeToZero(GetFadeOutBackward());
		}
	}
*/
	bool isVisible = m_isVisible;

	// HACK
	// B*2217819 - When slowed down to 5%, sticky bombs spin around before exploding.
	CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
	if( pModelInfo && pModelInfo->GetModelNameHash() == ATSTRINGHASH("w_ex_pe",0xbdd3a2ff) )	// Check this is the sticky bomb model
	{
		if( pNextPacketData && pNextPacketData->m_isVisible == false )
		{
			// Make invisible, so we don't interp into an invisible frame.
			isVisible = false;
		}
	}
	// HACK

	pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY, isVisible);

	pEntity->ChangeVisibilityMask(VIS_PHASE_MASK_GBUF|VIS_PHASE_MASK_SEETHROUGH_MAP, m_isVisibleInReplayCamera || !CReplayMgr::IsUsingRecordedCamera(), false);

	Matrix34 matrix;
	bool nextPacketWarpedThisFrame = pNextPacketData ? pNextPacketData->m_WarpedThisFrame : false;
	// We ban warping inside luxor plane (hack fix for B*2341671).
	bool warpingInUse = (m_VersionUsesWarping && (m_WarpedThisFrame || nextPacketWarpedThisFrame)) && !ignoreWarpingFlags;
	if(pEntity->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || interp == 0.0f || pNextPacketData == NULL || warpingInUse)
	{
		m_posAndRot.LoadMatrix(matrix);
	}
	else if(pNextPacketData)
	{
		Quaternion rQuatNew;
		Vector3 rVecNew;
		pNextPacketData->m_posAndRot.FetchQuaternionAndPos(rQuatNew, rVecNew);
		Extract(matrix, rQuatNew, rVecNew, interp);
	}

	fwAttachmentEntityExtension* pExtension = pEntity->GetAttachmentExtension();
	bool assertOn = false;
	if (pExtension) 
	{
		assertOn = pExtension->GetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE);
		pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
	}
	pEntity->SetMatrix(matrix);
	if (pExtension) pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, assertOn);

	if(pEntity->GetPortalTracker())
	{
		pEntity->GetPortalTracker()->SetIsCutsceneControlled(m_wasCutsceneOwned);
	}

	if(DoRemoveAndAdd)
	{
		// Use the next packet interior location if possible
		// This fixes the interior location being incorrect in first person mode when going between interiors at 5% replay speed.
		if(pNextPacketData && !flags.IsSet(REPLAY_DIRECTION_BACK))
		{
			RemoveAndAdd(pEntity, pNextPacketData->GetInteriorLocation(), pNextPacketData->GetInInterior(), true);
		}
		else
		{
			RemoveAndAdd(pEntity, GetInteriorLocation(), GetInInterior(), true);
		}
	}

	if(pEntity->GetLodDistance() != m_lodDistance)
	{
		pEntity->SetLodDistance((u32)m_lodDistance);
		pEntity->RequestUpdateInWorld();
	}

	pEntity->UpdateWorldFromEntity();
	pEntity->UpdatePhysicsFromEntity(true);
}


void CBasicEntityPacketData::Extract(Matrix34& rMatrix, const Quaternion& rQuatNew, const Vector3& rVecNew, float fInterp) const
{
	Quaternion rQuatPrev;
	m_posAndRot.GetQuaternion(rQuatPrev);

	Quaternion rQuatInterp;
	rQuatPrev.PrepareSlerp(rQuatNew);
	rQuatInterp.Slerp(fInterp, rQuatPrev, rQuatNew);

	rMatrix.FromQuaternion(rQuatInterp);
	replayAssert(rMatrix.IsOrthonormal());

	m_posAndRot.LoadPosition(rMatrix);

	rMatrix.d = rMatrix.d * (1.0f - fInterp) + rVecNew * fInterp;
}

void CBasicEntityPacketData::RemoveAndAdd(CPhysical* pEntity, const fwInteriorLocation& interiorLoc, bool inInterior, bool bIgnorePhysics) const
{
	//todo4five temp as these are causing issues in the final builds
	(void) bIgnorePhysics;

	if(!CReplayMgrInternal::IsCursorJumpingForward())
	{
		pEntity->UpdatePortalTracker();
	}

	CPortalTracker* pTracker = pEntity->GetPortalTracker();
	bool wasInInterior = pEntity->GetIsInInterior();
	fwInteriorLocation currentLoc = pEntity->GetInteriorLocation();
	CViewport *pVp = gVpMan.GetCurrentViewport();

	static const CEntity* pCachedAttachedEntity = nullptr;
	if(!camInterface::GetReplayDirector().IsFreeCamActive())
	{
		pCachedAttachedEntity = pVp ? pVp->GetAttachEntity() : pCachedAttachedEntity;
	}
	const CEntity* pAttachedEntity = pCachedAttachedEntity;

#if __BANK
	if(g_PickerManager.GetSelectedEntity() == pEntity)
	{
		replayDebugf3("Entity picked, used to break inside CBasicEntityPacketData::RemoveAndAdd");	
	}
#endif 

	//////////////////////////////////////////////////////////////////////////
	// Hack for url:bugstar:5394550 to get the Arena to appear.
	// It feels like we might be missing the interior location comparison anyway but
	// without further bugs I'd rather not add it in.  This is all keyed off the 
	// interior used in the Arena so as to not affect anything else.
	bool forceUpdateInteriorFor5394550 = false;
	bool isArena = false;
	CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation( interiorLoc );
	if(pInteriorProxy && pInteriorProxy->GetNameHash() == 0x6acc1a25)
	{
		isArena = true;
		forceUpdateInteriorFor5394550 = !currentLoc.IsSameLocation(interiorLoc);
	}
	//////////////////////////////////////////////////////////////////////////

	if(pAttachedEntity == pEntity && ((CReplayMgr::IsJumping() || CReplayMgr::WasJumping()) && !CReplayMgr::IsFineScrubbing()) && !isArena)
	{
		pTracker->RequestRescanNextUpdate();
	}
	// Don't update the interior location of the tracked entity as it will not be accurate. 
	// The player going in and out of interiors can cause the camera to track incorrect.
	else if(	pAttachedEntity != pEntity || 
		forceUpdateInteriorFor5394550 ||
		pEntity->GetPortalTracker()->WasFlushedFromRetainList() || 
		((CReplayMgr::IsJumping() || CReplayMgr::WasJumping()) && !CReplayMgr::IsFineScrubbing()) ) 
	{	
		if(  interiorLoc.IsAttachedToPortal()==false && inInterior && (wasInInterior==false || interiorLoc.GetAsUint32() != currentLoc.GetAsUint32()))
		{
			ASSERT_ONLY(u32 ret = )pTracker->MoveToNewLocation(interiorLoc);
			replayFatalAssertf(ret & eNetIntUpdateOk || ret & eNetIntUpdateSameLocation, "Failed to move entity to new location (ret:%u)", ret);
		}
		else if(inInterior && interiorLoc.IsAttachedToPortal() && interiorLoc.GetAsUint32() != currentLoc.GetAsUint32())
		{
			//If the object didn't get added to the correct location on load then update it.
			//Only do this for objects attached to portals such as doors and make sure the interior can receive the object
			bool bAdd = true;
			if( interiorLoc.GetInteriorProxyIndex() != INTLOC_INVALID_INDEX )
			{
				CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation( interiorLoc );
				if (AssertVerify(pInteriorProxy))
				{
					CInteriorInst* pIntInst = pInteriorProxy->GetInteriorInst();
					if( pIntInst == NULL || (pIntInst && !pIntInst->CanReceiveObjects()) )
					{
						bAdd = false;
					}
				}
			}

			// If the interior is loaded, add it into the correct interior location
			if(bAdd)
			{
				CGameWorld::RemoveAndAdd(pEntity, interiorLoc);
			}
		}
		else if(wasInInterior && !inInterior)
		{
			CGameWorld::RemoveAndAdd(pEntity,  CGameWorld::OUTSIDE, true);
		}
	}
	pTracker->Update(VEC3V_TO_VECTOR3(pEntity->GetMatrix().d()));
}


//=====================================================================================================
//Saves the fwInteriorLocation into replay CReplayInteriorProxy
//Stores the interior proxy model hash instead of the index of the proxy due to the array changing each build.
void CReplayInteriorProxy::SetInterior(fwInteriorLocation interiorLocation)
{
	//Set data to default which is also outside
	MakeInvalid();
	
	if(interiorLocation.IsValid())
	{
		//Get the current proxy to be able to grab the required data
		CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation(interiorLocation);
		if(pInteriorProxy)
		{
			//Set interior model hash
			SetInteriorProxyHash(pInteriorProxy->GetNameHash());
			
			//Set position as there can be more than one proxy with the same hash
			Vec3V position;
			pInteriorProxy->GetPosition(position);
			SetInteriorPosition(VEC3V_TO_VECTOR3(position));

			//Set portal and room data
			//Note this may change but it's not going to change that often and isn't worth sorting the hash which is currently not in final builds.
			SetIsAttachedToPortal(interiorLocation.IsAttachedToPortal());
			if(interiorLocation.IsAttachedToRoom())
			{
				SetRoomOrPortalIndex(interiorLocation.GetRoomIndex());
			}
			else if(interiorLocation.IsAttachedToPortal())
			{
				SetRoomOrPortalIndex(interiorLocation.GetPortalIndex());
				SetIsPortalToExterior(interiorLocation.IsPortalToExterior());
			}
		}
	}
}

void CReplayInteriorProxy::MakeInvalid()
{ 
	m_Position.Store(Vector3(0.0f, 0.0f, 0.0f));
	m_ProxyHash = 0;
	
	m_roomOrPortalIndex = INTLOC_INVALID_INDEX;
	m_isAttachedToPortal = false;
	m_isPortalToExterior = false;
}

fwInteriorLocation CReplayInteriorProxy::GetInteriorLocation() const
{
	//If there isn't a valid proxy hash then it must be outside (exterior)
	if(GetInteriorProxyHash() == 0)
	{
		return CGameWorld::OUTSIDE;
	}
	
	fwInteriorLocation	intLoc;
	intLoc.MakeInvalid();

	//Get the correct proxy index using the interior proxy model hash and the position
	Vector3 pos;
	GetInteriorPosition(pos);
	const CInteriorProxy *pInteriorProxy = CInteriorProxy::FindProxy(GetInteriorProxyHash(), pos);
	
	if (replayVerify(pInteriorProxy))
	{
		//Extact the room/portal data from the saved data
		intLoc.SetInteriorProxyIndex( CInteriorProxy::GetPool()->GetJustIndex( pInteriorProxy ) );

		if(!m_isAttachedToPortal && m_roomOrPortalIndex != INTLOC_INVALID_INDEX && m_roomOrPortalIndex < INTLOC_MAX_INTERIOR_ROOMS_COUNT )
		{
			intLoc.SetRoomIndex(m_roomOrPortalIndex);
		}

		if(m_isAttachedToPortal && m_roomOrPortalIndex != INTLOC_INVALID_INDEX && m_roomOrPortalIndex < INTLOC_MAX_INTERIOR_PORTALS_COUNT)
		{
			intLoc.SetPortalIndex(m_roomOrPortalIndex);
			intLoc.SetPortalToExterior(m_isPortalToExterior);
		}
	}

	return intLoc;
}


#endif // GTA_REPLAY
