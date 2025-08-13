//========================================================================================================================
// name:		PickupPacket.cpp
// description:	Pickup replay packet.
//========================================================================================================================
#include "PickupPacket.h"

#if GTA_REPLAY

#include "debug/Debug.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayInternal.h"
#include "control/gamelogic.h"
#include "control/trafficlights.h"
#include "pickups/Pickup.h"
#include "scene/portals/portal.h"
#include "scene/world/GameWorld.h"
#include "shaders/CustomShaderEffectWeapon.h"


//////////////////////////////////////////////////////////////////////////
// Constructor needed for the array of these in the interface
CPacketPickupCreate::CPacketPickupCreate()
	: CPacketBase(PACKETID_PICKUP_CREATE, sizeof(CPacketPickupCreate), CPacketPickupCreate_V1)
	, CPacketEntityInfo()
	, m_FrameCreated(FrameRef::INVALID_FRAME_REF)
{}

//////////////////////////////////////////////////////////////////////////
// Store data in a packet for creating an pickup
const tPacketVersion CPacketPickupCreateExtensions_V1 = 1;
const tPacketVersion CPacketPickupCreateExtensions_V2 = 2;
const tPacketVersion CPacketPickupCreateExtensions_V3 = 3;
void CPacketPickupCreate::Store(const CPickup* pObject, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId, tPacketSize packetSize)
{
	(void)sessionBlockIndex;
	PACKET_EXTENSION_RESET(CPacketPickupCreate);
	CPacketBase::Store(packetId, packetSize, CPacketPickupCreate_V1);
	CPacketEntityInfo::Store(pObject->GetReplayID());

	m_firstCreationPacket = firstCreationPacket;
	SetShouldPreload(firstCreationPacket);

	m_ModelNameHash = pObject->GetArchetype()->GetModelNameHash();
	fwArchetype* archetype = fwArchetypeManager::GetArchetypeFromHashKey(m_ModelNameHash, NULL);

	fwModelId modelID = fwArchetypeManager::LookupModelId(archetype);
	m_MapTypeDefHash = 0;
	if(modelID.GetMapTypeDefIndex() < strLocalIndex(fwModelId::TF_INVALID))
	{
		m_MapTypeDefHash = g_MapTypesStore.GetHash(modelID.GetMapTypeDefIndex());
	}

	m_PickupHash = pObject->GetPickupHash();
}


//////////////////////////////////////////////////////////////////////////
void CPacketPickupCreate::StoreExtensions(const CPickup* pObject, bool firstCreationPacket, u16 sessionBlockIndex)
{
	(void)firstCreationPacket; (void)sessionBlockIndex;
	PACKET_EXTENSION_RESET(CPacketPickupCreate);
	PickupCreateExtension *pExtension = (PickupCreateExtension*)BEGIN_PACKET_EXTENSION_WRITE(CPacketPickupCreateExtensions_V3, CPacketPickupCreate);

	pExtension->m_parentID = ReplayIDInvalid;
	//---------------------- CPacketPickupCreateExtensions_V1 ------------------------/
	if(pObject->GetDynamicComponent() && pObject->GetDynamicComponent()->GetAttachmentExtension() && pObject->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())
	{
		pExtension->m_parentID = ((CPhysical*)pObject->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())->GetReplayID();
	}

	//---------------------- CPacketPickupCreateExtensions_V2/3 ------------------------/
	pExtension->m_weaponHash = 0;
	pExtension->m_weaponTint = 0;
	if(pObject->GetWeapon())
	{
		pExtension->m_weaponHash = pObject->GetWeapon()->GetWeaponHash();
		pExtension->m_weaponTint = pObject->GetWeapon()->GetTintIndex();
	}

	pExtension->m_unused[0] = pExtension->m_unused[1] = pExtension->m_unused[2] = 0;

	END_PACKET_EXTENSION_WRITE(pExtension+1, CPacketPickupCreate);
}


//////////////////////////////////////////////////////////////////////////
CReplayID CPacketPickupCreate::GetParentID() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPickupCreate) >= CPacketPickupCreateExtensions_V1)
	{
		PickupCreateExtension *pExtension = (PickupCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPickupCreate);
		return pExtension->m_parentID;
	}
	return ReplayIDInvalid;
}

//////////////////////////////////////////////////////////////////////////
u32 CPacketPickupCreate::GetWeaponHash() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPickupCreate) >= CPacketPickupCreateExtensions_V2)
	{
		PickupCreateExtension *pExtension = (PickupCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPickupCreate);
		return pExtension->m_weaponHash;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
u8 CPacketPickupCreate::GetWeaponTint() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketPickupCreate) >= CPacketPickupCreateExtensions_V3)
	{
		PickupCreateExtension *pExtension = (PickupCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketPickupCreate);
		return pExtension->m_weaponTint;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketPickupCreate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CPacketEntityInfo::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<CreationFrame>%u : %u</CreationFrame>\n", GetFrameCreated().GetBlockIndex(), GetFrameCreated().GetFrame());
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketPickupUpdate_V1 = 1;
void CPacketPickupUpdate::Store(CPickup* pObject, bool firstUpdatePacket)
{
	PACKET_EXTENSION_RESET(CPacketPickupUpdate);
	Store(PACKETID_PICKUP_UPDATE, sizeof(CPacketPickupUpdate), pObject, firstUpdatePacket);
}

//////////////////////////////////////////////////////////////////////////
void CPacketPickupUpdate::Store(eReplayPacketId uPacketID, u32 packetSize, CPickup* pObject, bool firstUpdatePacket)
{
	PACKET_EXTENSION_RESET(CPacketPickupUpdate);
	replayAssert(pObject);

	CPacketBase::Store(uPacketID, packetSize, CPacketPickupUpdate_V1);
	CPacketInterp::Store();
	CBasicEntityPacketData::Store(pObject, pObject->GetReplayID());
	
	SetIsFirstUpdatePacket(firstUpdatePacket);

	m_scaleXY = 1.0f;
	m_scaleZ = 1.0f;
	const fwTransform* pTransform = pObject->GetTransformPtr();
	if (pTransform->IsSimpleScaledTransform())
	{
		m_scaleXY = ((fwSimpleScaledTransform*)pTransform)->GetScaleXY();
		m_scaleZ = ((fwSimpleScaledTransform*)pTransform)->GetScaleZ();
	}
	else if (pTransform->IsMatrixScaledTransform())
	{
		m_scaleXY = ((fwMatrixScaledTransform*)pTransform)->GetScaleXY();
		m_scaleZ = ((fwMatrixScaledTransform*)pTransform)->GetScaleZ();
	}
	else if (pTransform->IsQuaternionScaledTransform())
	{
		m_scaleXY = ((fwQuaternionScaledTransform*)pTransform)->GetScaleXY();
		m_scaleZ = ((fwQuaternionScaledTransform*)pTransform)->GetScaleZ();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPickupUpdate::PreUpdate(const CInterpEventInfo<CPacketPickupUpdate, CPickup>& eventInfo) const
{
	CPickup* pObject = eventInfo.GetEntity();
	if (pObject)
	{
		const CPacketPickupUpdate* pNextPacket = eventInfo.GetNextPacket();
		float interp = eventInfo.GetInterp();
		CBasicEntityPacketData::Extract(pObject, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, eventInfo.GetPlaybackFlags());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPickupUpdate::Extract(const CInterpEventInfo<CPacketPickupUpdate, CPickup>& eventInfo) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketPickupUpdate) == 0);
	CPickup* pObject = eventInfo.GetEntity();
	if (pObject)
	{
		const CPacketPickupUpdate* pNextPacket = eventInfo.GetNextPacket();
		float interp = eventInfo.GetInterp();
		CBasicEntityPacketData::Extract(pObject, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, eventInfo.GetPlaybackFlags());


		if((eventInfo.GetPlaybackFlags().GetState() & REPLAY_CURSOR_JUMP) && (eventInfo.GetPlaybackFlags().GetState() & (1 << 16)))
  		{
 			pObject->UpdatePortalTracker();
  		}

		if(GetPacketVersion() >= CPacketPickupUpdate_V1)
		{
			pObject->SetTransformScale(m_scaleXY, m_scaleZ);
		}
	}
	else
	{
		replayDebugf1("CPacketObjectUpdate::Load: Invalid object");
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketPickupUpdate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CBasicEntityPacketData::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<ReplayID>0x%X, %d</ReplayID>\n", GetReplayID().ToInt(), GetReplayID().ToInt());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsFirstUpdate>%s</IsFirstUpdate>\n", GetIsFirstUpdatePacket() == true ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));
}

//////////////////////////////////////////////////////////////////////////
// Store data in a Packet for deleting a pickup
void CPacketPickupDelete::Store(const CPickup* pObject, const CPacketPickupCreate* pLastCreatedPacket)
{
	//clone our last know good creation packet and setup the correct new ID and size
	CloneBase<CPacketPickupDelete>(pLastCreatedPacket);
	//We need to use the version from the create packet to version the delete packet as it inherits from it.
	CPacketBase::Store(PACKETID_PICKUP_DELETE, sizeof(CPacketPickupDelete), pLastCreatedPacket->GetPacketVersion());

	FrameRef creationFrame;
	pObject->GetCreationFrameRef(creationFrame);
	SetFrameCreated(creationFrame);

	replayFatalAssertf(GetPacketSize() <= MAX_PICKUP_DELETION_SIZE, "Size of Pickup Deletion packet is too large! (%u)", GetPacketSize());
}

//////////////////////////////////////////////////////////////////////////
void CPacketPickupDelete::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<ReplayID>0x%8X</ReplayID>\n", GetReplayID().ToInt());
	CFileMgr::Write(handle, str, istrlen(str));
}


//========================================================================================================================
void CPickupInterp::Init(const ReplayController& controller, CPacketPickupUpdate const* pPrevPacket)
{
	// Setup the previous packet
	SetPrevPacket(pPrevPacket);


	CPacketPickupUpdate const* pObjectPrevPacket = GetPrevPacket();
	m_prevData.m_fullPacketSize += pObjectPrevPacket->GetPacketSize();

	// Look for/Setup the next packet
	if (pPrevPacket->IsNextDeleted())
	{
		m_nextData.Reset();
	}
	else if (pPrevPacket->HasNextOffset())
	{
		CPacketPickupUpdate const* pNextPacket = NULL;
		controller.GetNextPacket(pPrevPacket, pNextPacket);
		SetNextPacket(pNextPacket);

		CPacketPickupUpdate const* pObjectNextPacket = GetNextPacket();
#if __ASSERT
		CPacketPickupUpdate const* pObjectPrevPacket = GetPrevPacket();
		replayAssert(pObjectPrevPacket->GetReplayID() == pObjectNextPacket->GetReplayID());
#endif // __ASSERT

		m_nextData.m_fullPacketSize += pObjectNextPacket->GetPacketSize();
	}
	else
	{
		replayAssertf(false, "CPickupInterp::Init");
	}
}

//////////////////////////////////////////////////////////////////////////
void CPickupInterp::Reset()
{
	CInterpolator<CPacketPickupUpdate>::Reset();

	m_prevData.Reset();
	m_nextData.Reset();
}

#endif // GTA_REPLAY
