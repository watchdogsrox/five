//========================================================================================================================
// name:		ObjectPacket.cpp
// description:	Object replay packet.
//========================================================================================================================
#include "ObjectPacket.h"

#if GTA_REPLAY

#include "vectormath/classfreefuncsf.h"

#include "animation/move.h"
#include "animation/MoveObject.h"
#include "debug/Debug.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayInternal.h"
#include "control/replay/ReplayExtensions.h"
#include "control/replay/entity/FragmentPacket.h"
#include "control/gamelogic.h"
#include "control/trafficlights.h"
#include "scene/AnimatedBuilding.h"
#include "scene/portals/portal.h"
#include "scene/world/GameWorld.h"
#include "shaders/CustomShaderEffectWeapon.h"
#include "objects/Door.h"
#include "fwscene/stores/maptypesstore.h"
#include "streaming/streaming.h"
#include "renderer/PostScan.h"
#include "renderer/Lights/LightEntity.h"
#include "script/commands_entity.h"

//////////////////////////////////////////////////////////////////////////
// Constructor needed for the array of these in the interface
CPacketObjectCreate::CPacketObjectCreate()
	: CPacketBase(PACKETID_PEDCREATE, sizeof(CPacketObjectCreate), CPacketObjectCreate_V3)
	, CPacketEntityInfo()
	, m_FrameCreated(FrameRef::INVALID_FRAME_REF)
{}

//////////////////////////////////////////////////////////////////////////
// Store data in a Packet for Creating an Object
const tPacketVersion CPacketObjectCreateExtensions_V1 = 1;
const tPacketVersion CPacketObjectCreateExtensions_V2 = 2;
const tPacketVersion CPacketObjectCreateExtensions_V3 = 3;
void CPacketObjectCreate::Store(const CObject* pObject, bool firstCreationPacket, u16 sessionBlockIndex, eReplayPacketId packetId, tPacketSize packetSize)
{
	(void)sessionBlockIndex;
	CPacketBase::Store(packetId, packetSize, CPacketObjectCreate_V3);
	CPacketEntityInfo::Store(pObject->GetReplayID());

	m_unused0 = 0;

	m_firstCreationPacket = firstCreationPacket;
	SetShouldPreload(firstCreationPacket);

	m_ModelNameHash = pObject->GetArchetype()->GetModelNameHash();
	fwArchetype* pArcheType = fwArchetypeManager::GetArchetypeFromHashKey(m_ModelNameHash, NULL);
	fwModelId modelID = fwArchetypeManager::LookupModelId(pArcheType);
	m_MapTypeDefHash = 0;
	if(modelID.GetMapTypeDefIndex() < strLocalIndex(fwModelId::TF_INVALID))
	{
		m_MapTypeDefHash = g_MapTypesStore.GetHash(modelID.GetMapTypeDefIndex());
	}

	m_TintIndex		 = pObject->GetTintIndex();

	//Save interior location in replay format
	m_interiorLocation.SetInterior(CGameWorld::OUTSIDE);
	if( pObject->GetIsInInterior() )
	{
		m_interiorLocation.SetInterior(pObject->GetInteriorLocation());
	}
	
	m_posAndRot.StoreMatrix(MAT34V_TO_MATRIX34(pObject->GetMatrix()));

	// All objects initially created from a DummyObject will have the correct hash.
	m_mapHash		= pObject->GetHash();

	m_fragParentID	 = pObject->GetFragParent() && pObject->GetFragParent()->GetIsPhysical() ? ((CPhysical*)pObject->GetFragParent())->GetReplayID().ToInt() : -1;
	m_bIsWeaponOrComponent = pObject->GetWeapon() || pObject->GetWeaponComponent();
	m_bIsADoor = pObject->IsADoor();
	m_isAWheel = pObject->m_nObjectFlags.bCarWheel;
	m_isAParachute = pObject->GetIsParachute();
	
	m_bIsPropOverride = pObject->m_nObjectFlags.bDetachedPedProp;
	if (m_bIsPropOverride)
	{
		m_uPropHash = ReplayObjectExtension::GetPropHash(pObject);
		m_uTexHash = ReplayObjectExtension::GetTexHash(pObject);
		m_uAnchorID = ReplayObjectExtension::GetAnchorID(pObject);
		m_preloadPropInfo = ReplayObjectExtension::IsStreamedPedProp(pObject);	// Preload if it's a streamed ped prop
		m_parentModelID = pObject->uPropData.m_parentModelIdx;
	}
	else
	{
		m_uPropHash = 0;
		m_uTexHash = 0;
		m_uAnchorID = 0;
		m_preloadPropInfo = false;
	}

	m_trafficLightData.m_initialTrafficLightsVerified = false;

	m_weaponHash = 0;
	m_weaponTint = 0;
	if(pObject->GetWeapon())
	{
		m_weaponHash = pObject->GetWeapon()->GetWeaponHash();
		m_weaponTint = pObject->GetWeapon()->GetTintIndex();
	}

	m_weaponParentID = -1;
	m_weaponComponentHash = 0;
	//Only support flash lights, currently no need to handle the other components in replay.
	if(pObject->GetWeaponComponent() && pObject->GetWeaponComponent()->GetInfo() &&
		(pObject->GetWeaponComponent()->GetInfo()->GetClassId() == WCT_FlashLight || pObject->GetWeaponComponent()->GetInfo()->GetClassId() == WCT_Scope))
	{
		m_weaponComponentHash = pObject->GetWeaponComponent()->GetInfo()->GetHash();

		if( pObject->GetWeaponComponent() &&
			pObject->GetWeaponComponent()->GetWeapon() && 
			pObject->GetWeaponComponent()->GetWeapon()->GetEntity() &&
			pObject->GetWeaponComponent()->GetWeapon()->GetEntity() != pObject)
		{
			m_weaponParentID = ((CPhysical*)pObject->GetWeaponComponent()->GetWeapon()->GetEntity())->GetReplayID().ToInt();
			if(m_weaponParentID == GetReplayID())
			{
				ASSERT_ONLY(const CWeaponComponentInfo* pComponentInfo = pObject->GetWeaponComponent()->GetInfo());
				ASSERT_ONLY(const CWeaponModelInfo* pWInfo = pObject->GetWeaponComponent()->GetWeapon()->GetWeaponModelInfo());
				replayAssertf(m_weaponParentID != GetReplayID(), "Confusion...weapon component entity is the same as weapon entity... 0x%08X.  Component is %s, Weapon is %s", 
					m_weaponParentID, 
					pComponentInfo ? pComponentInfo->GetModelName() : "Unknown", 
					pWInfo ? pWInfo->GetModelName() : "Unknown");
				m_weaponParentID = -1; // Set back to -1 so we don't get an object cyclic dependency on playback
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketObjectCreate::StoreExtensions(const CObject* pObject, bool firstCreationPacket, u16 sessionBlockIndex)
{
	(void)firstCreationPacket; (void)sessionBlockIndex; 
	PACKET_EXTENSION_RESET(CPacketObjectCreate);
	ObjectCreateExtension *pExtension = (ObjectCreateExtension*)BEGIN_PACKET_EXTENSION_WRITE(CPacketObjectCreateExtensions_V3, CPacketObjectCreate);

	pExtension->m_parentID = ReplayIDInvalid;
	//---------------------- CPacketObjectCreateExtensions_V1 ------------------------/
	if(pObject->GetDynamicComponent() && pObject->GetDynamicComponent()->GetAttachmentExtension() && pObject->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())
	{
		pExtension->m_parentID = ((CPhysical*)pObject->GetDynamicComponent()->GetAttachmentExtension()->GetAttachParent())->GetReplayID();
	}

	//---------------------- CPacketObjectCreateExtensions_V2 ------------------------/
	Quaternion doorStartRot(Quaternion::IdentityType);
	if(pObject->IsADoor())
	{
		doorStartRot = (static_cast<const CDoor*>(pObject))->GetDoorStartRot();
	}
	pExtension->m_qDoorStartRot.StoreQuaternion(doorStartRot);

	//---------------------- CPacketObjectCreateExtensions_V3 ------------------------/
	pExtension->m_weaponComponentTint = 0;
	if(pObject->GetWeaponComponent() && pObject->GetWeaponComponent()->GetInfo())
	{
		pExtension->m_weaponComponentTint = pObject->GetWeaponComponent()->GetTintIndex();
	}

	END_PACKET_EXTENSION_WRITE(pExtension+1, CPacketObjectCreate);
}


//////////////////////////////////////////////////////////////////////////
CReplayID CPacketObjectCreate::GetParentID() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketObjectCreate) >= CPacketObjectCreateExtensions_V1)
	{
		ObjectCreateExtension *pExtension = (ObjectCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketObjectCreate);
		return pExtension->m_parentID;
	}
	return ReplayIDInvalid;
}

//////////////////////////////////////////////////////////////////////////
Quaternion CPacketObjectCreate::GetDoorStartRot() const
{
	Quaternion quat(Quaternion::IdentityType);
	if(GET_PACKET_EXTENSION_VERSION(CPacketObjectCreate) >= CPacketObjectCreateExtensions_V2)
	{
		ObjectCreateExtension *pExtension = (ObjectCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketObjectCreate);
		pExtension->m_qDoorStartRot.LoadQuaternion(quat);
	}
	return quat;
}


u8 CPacketObjectCreate::GetWeaponComponentTint() const
{
	if(GET_PACKET_EXTENSION_VERSION(CPacketObjectCreate) >= CPacketObjectCreateExtensions_V3)
	{
		ObjectCreateExtension *pExtension = (ObjectCreateExtension*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketObjectCreate);
		return pExtension->m_weaponComponentTint;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectCreate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CPacketEntityInfo::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<CreationFrame>%u : %u</CreationFrame>\n", GetFrameCreated().GetBlockIndex(), GetFrameCreated().GetFrame());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<ModelNameHash>%u</ModelNameHash>\n", GetModelNameHash());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<MapTypeDefIndex>%u</MapTypeDefIndex>\n", GetMapTypeDefIndex().Get());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<MapHash>%u</MapHash>\n", GetMapHash());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsFirstCreationPacket>%s</IsFirstCreationPacket>\n", IsFirstCreationPacket() ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<m_parentModelID>%d</m_parentModelID>\n", m_parentModelID);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<m_fragParentID>0x%08X</m_fragParentID>\n", m_fragParentID);
	CFileMgr::Write(handle, str, istrlen(str));

	if(m_uPropHash != 0)
	{
		snprintf(str, 1024, "\t\t<m_uPropHash>%u</m_uPropHash>\n", m_uPropHash);
		CFileMgr::Write(handle, str, istrlen(str));
	}

	if(m_uTexHash != 0)
	{
		snprintf(str, 1024, "\t\t<m_uTexHash>%u</m_uTexHash>\n", m_uTexHash);
		CFileMgr::Write(handle, str, istrlen(str));
	}

	if(GetParentID() != ReplayIDInvalid)
	{
		snprintf(str, 1024, "\t\t<m_parentID>0x%08X</m_parentID>\n", GetParentID().ToInt());
		CFileMgr::Write(handle, str, istrlen(str));
	}
}


//////////////////////////////////////////////////////////////////////////

void CPacketObjectUpdate::Store(CObject* pObject, bool firstUpdatePacket)
{
	PACKET_EXTENSION_RESET(CPacketObjectUpdate);
	Store(PACKETID_OBJECTUPDATE, sizeof(CPacketObjectUpdate), pObject, firstUpdatePacket);
}

const u32 g_CPacketObjectUpdate_Extensions_V1 = 1;
const u32 g_CPacketObjectUpdate_Extensions_V2 = 2;
const u32 g_CPacketObjectUpdate_Extensions_V3 = 3;

void CPacketObjectUpdate::StoreExtensions(CObject* pObject, bool firstUpdatePacket) 
{
	(void) pObject;
	(void) firstUpdatePacket;
	PACKET_EXTENSION_RESET(CPacketObjectUpdate); 

	PacketExtensions *pExtensions = (PacketExtensions *)BEGIN_PACKET_EXTENSION_WRITE(g_CPacketObjectUpdate_Extensions_V3, CPacketObjectUpdate);
	pExtensions->m_unused0 = 0;
	u8* pAfterExtensionHeader = (u8*)(pExtensions+1);

	//---------------------- Extensions V1 ------------------------/
	pExtensions->m_alphaOverrideValue = 0;
	m_hasAlphaOverride = CPostScan::GetAlphaOverride(pObject, (int&)pExtensions->m_alphaOverrideValue);

	//---------------------- Extensions V2 ------------------------/
	pExtensions->m_muzzlePosOffset = 0;

	CWeapon* pWeapon = pObject->GetWeapon();
	if(pWeapon)
	{
		pExtensions->m_muzzlePosOffset = sizeof(PacketExtensions);

		CPacketVector<3>* pMuzzlePos = (CPacketVector<3>*)pAfterExtensionHeader;

		const Matrix34& weaponMatrix = RCC_MATRIX34(pObject->GetMatrixRef());
		Vector3 vMuzzlePosition(Vector3::ZeroType);
		pWeapon->GetMuzzlePosition(weaponMatrix, vMuzzlePosition);
		pMuzzlePos->Store(vMuzzlePosition);

		pAfterExtensionHeader += sizeof(CPacketVector<3>);
	}

	//---------------------- Extensions V3 ------------------------/
	pExtensions->m_vehModSlot = pObject->GetVehicleModSlot();
	
	END_PACKET_EXTENSION_WRITE(pAfterExtensionHeader, CPacketObjectUpdate);
}


//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketObjectUpdate_V1 = 1;
void CPacketObjectUpdate::Store(eReplayPacketId uPacketID, u32 packetSize, CObject* pObject, bool firstUpdatePacket)
{
	PACKET_EXTENSION_RESET(CPacketObjectUpdate);
	replayAssert(pObject);

	CPacketBase::Store(uPacketID, packetSize, CPacketObjectUpdate_V1);
	CPacketInterp::Store();
	CBasicEntityPacketData::Store(pObject, pObject->GetReplayID());
	
	m_unused0 = 0;
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

	SetPadBool(ContainsFragBit, false);
	SetPadU8(FragBoneCountU8, 0);
 	if (pObject->GetFragInst() && pObject->GetFragInst()->GetType())
 	{
 		SetPadBool(ContainsFragBit, true);
 		u32 sBoneCount = 0;
 		fragCacheEntry* pFragCacheEntry = pObject->GetFragInst()->GetCacheEntry();
 		if (pFragCacheEntry && pFragCacheEntry->GetHierInst()->skeleton)
 		{
 			sBoneCount += pFragCacheEntry->GetHierInst()->skeleton->GetBoneCount();
 			if (pFragCacheEntry->GetHierInst()->damagedSkeleton)
 			{
 				sBoneCount += pFragCacheEntry->GetHierInst()->damagedSkeleton->GetBoneCount();
 			}
 		}
		replayAssertf(sBoneCount < MAX_UINT8, "CPacketObjectUpdate::Store: Bonecount is greater than 255");
 		SetPadU8(FragBoneCountU8, (u8)sBoneCount);
 	}

	m_boneCount = 0;
	if( pObject->GetSkeleton() )
	{
		m_boneCount = (u8)pObject->GetSkeleton()->GetBoneCount();
	}

	SetPadBool(RenderDamagedBit, pObject->IsRenderDamaged());

	SetPadBool(ObjFadeOutBit, pObject->m_nObjectFlags.bFadeOut);

	m_weaponTint = 0;
	m_isPlayerWeapon = 0;
	if(pObject->GetWeapon())
	{
		m_weaponTint = pObject->GetWeapon()->GetTintIndex();

		if(pObject->GetAttachParent() && static_cast<CPhysical *>(pObject->GetAttachParent())->GetIsTypePed())
		{
			m_isPlayerWeapon = static_cast<CPed*>(pObject->GetAttachParent())->IsLocalPlayer();
		}
	}

	// ----------- g_CPacketObjectUpdate_Version_V1 -------------------//

	// Store existence of env cloth (as it can be set to NULL see fragInst::BreakEffects() in rage/suit/src/fragment/instance.cpp).
	// Env. cloth is transfered from a breaking entity to one of it's broken children. During Replay playback we will be making new cloth for the child,  
	// so we need to remove the cloth from the breaking (parent) instance.
	m_hasEnvCloth = false;

	if (pObject->GetFragInst() && pObject->GetFragInst()->GetType())
	{
		fragCacheEntry* pFragCacheEntry = pObject->GetFragInst()->GetCacheEntry();

		if (pFragCacheEntry && pFragCacheEntry->GetHierInst() && pFragCacheEntry->GetHierInst()->envCloth)
		{
			m_hasEnvCloth = true;
		}
	}

	m_AnimatedBuildingPhase = -1.0f;
	m_AnimatedBuildingRate = -1.0f;

	//Set the phase and rate for every lod level on the object dummy
	if(pObject->GetRelatedDummy() && pObject->GetRelatedDummy()->GetLodData().HasLod() )
	{
		CEntity* pLod = (CEntity*)pObject->GetRelatedDummy()->GetLod();

		while (pLod)
		{
			if(pLod->GetIsTypeAnimatedBuilding() && pLod->GetAnimDirector())
			{
				CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pLod);
				CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();

				m_AnimatedBuildingPhase = moveAnimatedBuilding.GetPhase();
				m_AnimatedBuildingRate = moveAnimatedBuilding.GetRate();
				break;
			}

			if(!pLod->GetLodData().HasLod())
			{
				break;
			}

			pLod = (CEntity*)pLod->GetLod(); 
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectUpdate::PreUpdate(const CInterpEventInfo<CPacketObjectUpdate, CObject>& eventInfo) const
{
	CObject* pObject = eventInfo.GetEntity();
	if (pObject)
	{
		const CPacketObjectUpdate* pNextPacket = eventInfo.GetNextPacket();
		float interp = eventInfo.GetInterp();
		CBasicEntityPacketData::Extract(pObject, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, eventInfo.GetPlaybackFlags(), true, ShouldIgnoreWarpingFlags(pObject));

		// Hack for B*2341690! Call this here to update the bounding box used for visibility (this call is mainly here so the debug rendering works, it's also 
		// a frame out because it's using skeleton data the the previous frame).
		UpdateBoundingBoxForLuxorPlane(pObject);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectUpdate::UpdateAnimatedBuilding(CObject* pReplayObject, CObject* pObject, float fPhase, float fRate) const
{
	//Set the phase and rate for every lod level on the object dummy
	if(pObject->GetRelatedDummy() && pObject->GetRelatedDummy()->GetLodData().HasLod() )
	{
		CEntity* pLod = (CEntity*)pObject->GetRelatedDummy()->GetLod();

		while (pLod)
		{
			if(pLod->GetIsTypeAnimatedBuilding() && pLod->GetAnimDirector())
			{
				CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pLod);
				CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();

				moveAnimatedBuilding.SetReplayPhase(fPhase);
				moveAnimatedBuilding.SetReplayRate(fRate);
			}

			if(!pLod->GetLodData().HasLod())
			{
				break;
			}

			pLod = (CEntity*)pLod->GetLod();
		} 
	}

	// Update replay object with phase/rate, bones are no longer extracted for this object.
	if(pReplayObject)
	{
		CGenericClipHelper& clipHelper = pReplayObject->GetMoveObject().GetClipHelper();
		clipHelper.SetPhase(fPhase);
		clipHelper.SetRate(fRate);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectUpdate::Extract(const CInterpEventInfo<CPacketObjectUpdate, CObject>& eventInfo) const
{
	CObject* pObject = eventInfo.GetEntity();
	if (pObject)
	{
		const CPacketObjectUpdate* pNextPacket = eventInfo.GetNextPacket();
		float interp = eventInfo.GetInterp();
		CBasicEntityPacketData::Extract(pObject, (HasNextOffset() && !IsNextDeleted() && pNextPacket) ? pNextPacket : NULL, interp, eventInfo.GetPlaybackFlags(), true, ShouldIgnoreWarpingFlags(pObject));

		// url:bugstar:5142285 - Hide the screen that's attached to the Battlebus as it has z-fighting
		if(pObject->GetModelNameHash() == 0xcacee7c6 /*"ba_prop_battle_pbus_screen"*/)
		{
			pObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_REPLAY,false,false);
		}

		if(pObject->m_nFlags.bLightObject)
		{
			LightEntityMgr::UpdateLightsLocationFromEntity(pObject);
		}

		pObject->m_nObjectFlags.bFadeOut = GetPadBool(ObjFadeOutBit);
		pObject->SetRenderDamaged(GetPadBool(RenderDamagedBit));

		if((eventInfo.GetPlaybackFlags().GetState() & REPLAY_CURSOR_JUMP) && (eventInfo.GetPlaybackFlags().GetState() & (1 << 16)))
  		{
 			pObject->UpdatePortalTracker();
  		}

		if(pObject->GetSkeleton() == NULL && m_boneCount > 0)
		{
			pObject->CreateSkeleton();
		}
		else if(m_boneCount == 0)
		{	// No bones so we won't have any frag packets...NULL them all out manually

			// AC: This doesn't appear to actually give us anything and we reckon this caused url:bugstar:2354084.
			// Has been tested in url:bugstar:2372191 with this commented out.
 			//SetObjectDestroyed(pObject);
		}
		
		if(pObject->GetWeapon())
		{
			if(m_isPlayerWeapon)
			{
				pObject->GetWeapon()->RequestHdAssets();
			}
			pObject->GetWeapon()->Update_HD_Models(NULL);

			if(pObject->GetWeapon()->GetTintIndex() != m_weaponTint)
			{
				pObject->GetWeapon()->UpdateShaderVariables(m_weaponTint);
			}
		}		

		fragInst *pFragInst = pObject->GetFragInst();

		if(pFragInst && pFragInst->GetType())
		{
			const fragType *pFragType = pObject->GetFragInst()->GetType();

			// We're only interested in environment cloth here.
			if(pFragType->GetNumEnvCloths())
			{
				fragCacheEntry* pFragCacheEntry = pObject->GetFragInst()->GetCacheEntry();

				if(pFragCacheEntry->GetHierInst() && pFragCacheEntry->GetHierInst()->envCloth)
				{
					if((m_hasEnvCloth == true) && (pFragCacheEntry->GetHierInst()->envCloth->GetFlag(environmentCloth::CLOTH_ForceInactiveForReplay) == true))
						pFragCacheEntry->GetHierInst()->envCloth->SetFlag(environmentCloth::CLOTH_ForceInactiveForReplay, false);
					else if((m_hasEnvCloth == false) && (pFragCacheEntry->GetHierInst()->envCloth->GetFlag(environmentCloth::CLOTH_ForceInactiveForReplay) == false))
						pFragCacheEntry->GetHierInst()->envCloth->SetFlag(environmentCloth::CLOTH_ForceInactiveForReplay, true);
				}
			}
		}

		CObject* pMapObject = pObject->ReplayGetMapObject();
		if(pMapObject)
		{
			//Interpolate the phase
			float fPhase = m_AnimatedBuildingPhase;
			float fRate = m_AnimatedBuildingRate;
			if(fPhase != -1.0f)
			{
				if(eventInfo.GetNextPacket())
				{
					float interpPhase = Lerp(eventInfo.GetInterp(), fPhase, eventInfo.GetNextPacket()->m_AnimatedBuildingPhase);

					if(abs(interpPhase - fPhase) < 0.2f)
						fPhase = interpPhase;
				}

				//Update all the lods with the current replay object phase and rate
				UpdateAnimatedBuilding(pObject, pMapObject, fPhase, fRate);
			}
		}

		ExtractExtensions(pObject, eventInfo);

		if(GetPacketVersion() >= CPacketObjectUpdate_V1)
		{
			pObject->SetTransformScale(m_scaleXY, m_scaleZ);
		}
	}
	else
	{
		replayDebugf1("CPacketObjectUpdate::Load: Invalid object");
	}
}

void CPacketObjectUpdate::ExtractExtensions(CObject* pObject, const CInterpEventInfo<CPacketObjectUpdate, CObject>& eventInfo) const
{
	PacketExtensions *pExtensions = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketObjectUpdate);
	if(GET_PACKET_EXTENSION_VERSION(CPacketObjectUpdate) >= g_CPacketObjectUpdate_Extensions_V1)
	{
		if( m_hasAlphaOverride )
		{
			int currentAlpha = 0;
			bool IsOnAlphaOverrideList = CPostScan::GetAlphaOverride(pObject, currentAlpha);
			if(!IsOnAlphaOverrideList || currentAlpha != pExtensions->m_alphaOverrideValue)
			{
				bool useSmoothAlpha = !pObject->GetShouldUseScreenDoor();
				entity_commands::CommandSetEntityAlphaEntity(pObject, (int)pExtensions->m_alphaOverrideValue, useSmoothAlpha BANK_PARAM(entity_commands::CMD_OVERRIDE_ALPHA_BY_NETCODE));
			}
		}
		else if(CPostScan::IsOnAlphaOverrideList(pObject))
		{
			entity_commands::CommandResetEntityAlphaEntity(pObject);
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketObjectUpdate) >= g_CPacketObjectUpdate_Extensions_V2)
	{
		if(pExtensions->m_muzzlePosOffset)
		{
			CPacketVector<3>* pMuzzlePos = (CPacketVector<3>*)((u8*)pExtensions + pExtensions->m_muzzlePosOffset);
			Vector3 muzzlePos;
			pMuzzlePos->Load(muzzlePos);

			const CPacketObjectUpdate* pNextPacket = eventInfo.GetNextPacket();
			if(pNextPacket)
			{
				PacketExtensions *pNextExtension = (PacketExtensions *)GET_PACKET_EXTENSION_READ_PTR_OTHER(pNextPacket, CPacketObjectUpdate);
				if(pNextExtension)
				{
					CPacketVector<3>* pNextMuzzlePos = (CPacketVector<3>*)((u8*)pNextExtension + pNextExtension->m_muzzlePosOffset);
					Vector3 nextMuzzlePos;
					pNextMuzzlePos->Load(nextMuzzlePos);

					muzzlePos = muzzlePos * (1.0f - eventInfo.GetInterp()) + nextMuzzlePos * eventInfo.GetInterp();
				}
			}

			CWeapon* pWeapon = pObject->GetWeapon();
			if(pWeapon)
				pWeapon->SetMuzzlePosition(muzzlePos);
		}
	}

	if(GET_PACKET_EXTENSION_VERSION(CPacketObjectUpdate) >= g_CPacketObjectUpdate_Extensions_V3)
	{
		pObject->SetVehicleModSlot((s8)pExtensions->m_vehModSlot);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectUpdate::SetObjectDestroyed(CObject* pObject) const
{
	rage::fragInst* pFragInst = pObject->GetFragInst();
	if (pFragInst)
	{
		pFragInst->SetBroken(true);
		bool bCached = pFragInst->GetCached();
		if (!bCached)
		{
			pFragInst->PutIntoCache();
		}

		const rage::fragPhysicsLOD * pFragType = pFragInst->GetTypePhysics();

		u8 rootGroupIndex = 0;
		rage::fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
		if(pCacheEntry)
		{
			rootGroupIndex = pFragType->GetChild(pCacheEntry->GetHierInst()->rootLinkChildIndex)->GetOwnerGroupPointerIndex();
			// We need to set this so the broken parts get rendered (see CDynamicEntityDrawHandler::AddFragmentToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pParams) in DynamicEntityDrawHandler.cpp).
			if(pFragInst->GetType()->GetDamagedDrawable())
				pCacheEntry->GetHierInst()->anyGroupDamaged = true;
		}

		if (pFragType)
		{
			for (u8 child = 0; child < pFragType->GetNumChildren(); ++child)
			{
				const rage::fragTypeChild* pChild = pFragType->GetChild(child);

				u8 groupIndex = pChild->GetOwnerGroupPointerIndex();
				if(!pCacheEntry || groupIndex == rootGroupIndex)
					continue;

				if(!pFragInst->GetChildBroken(child))
					pFragInst->DeleteAbove(child);
			}

			if(pFragInst->GetCacheEntry())
			{
				pFragInst->GetCacheEntry()->UpdateIsFurtherBreakable();
			}
		}

		fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();

		if (pFragCacheEntry)
		{
			rage::fragHierarchyInst* pHierInst = pFragCacheEntry->GetHierInst();

			if(pHierInst->damagedSkeleton)
			{
				for(u16 i = 0; i < pHierInst->damagedSkeleton->GetBoneCount(); ++i)
				{
					Matrix34 mat;
					mat.Zero();
					pHierInst->damagedSkeleton->SetGlobalMtx((u32)i, MATRIX34_TO_MAT34V(mat));
				}
			}

			if(pHierInst->skeleton)
			{
				for(u16 i = 0; i < pHierInst->skeleton->GetBoneCount(); ++i)
				{
					Matrix34 mat;
					mat.Zero();
					pHierInst->skeleton->SetGlobalMtx((u32)i, MATRIX34_TO_MAT34V(mat));
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectUpdate::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CBasicEntityPacketData::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<RenderDamaged>%u</RenderDamaged>\n", GetPadBool(RenderDamagedBit));
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<m_boneCount>%u</m_boneCount>\n", m_boneCount);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<ContainsFrag>%s</ContainsFrag>\n", GetPadBool(ContainsFragBit) ? "Yes" : "No");
	CFileMgr::Write(handle, str, istrlen(str));

	if(GetPadBool(ContainsFragBit))
	{
		snprintf(str, 1024, "\t\t<FragCount>%u</FragCount>\n", GetPadU8(FragBoneCountU8));
		CFileMgr::Write(handle, str, istrlen(str));
	}
}




//////////////////////////////////////////////////////////////////////////
// Hack for B*2341690!
atHashString CPacketObjectUpdate::ms_LuxorPlaneObjects[] = 
{
	ATSTRINGHASH("lux_prop_champ_01_luxe", 0xba823bb7),
};


bool CPacketObjectUpdate::IsLuxorPlaneObjectByModelNameHash(CEntity *pObject)
{
	int nElements = sizeof(ms_LuxorPlaneObjects) / sizeof(atHashString);
	for(int i=0; i<nElements; i++)
	{
		atHashString &tableEntry = ms_LuxorPlaneObjects[i];
		if( pObject->GetModelNameHash() == tableEntry.GetHash() )
		{
			return true;
		}
	}
	return false;
}

bool CPacketObjectUpdate::IsLuxorPlaneObjectByAttachment(CEntity *pObject)
{
	fwAttachmentEntityExtension* pExtension = pObject->GetAttachmentExtension();

	if (pExtension) 
	{
		CEntity *pParentEntity = reinterpret_cast < CEntity * > (pExtension->GetAttachParent());

		if(pParentEntity)
		{
			if(pParentEntity->GetModelNameHash() == ATSTRINGHASH("luxor2", 0xb79f589e))
				return true;
			else
				return IsLuxorPlaneObjectByAttachment(pParentEntity);
		}
	}
	return false;
}

bool CPacketObjectUpdate::ShouldIgnoreWarpingFlags(CEntity *pObject)
{
	atHashString ignoreWarpingModels[] = 
	{
		// These objects appear to have an initial frame with the warping flag set which avoids any interpolation
		// This results in strange popping issues where the missiles are left behind or they appear to jump forward.
		// e.g. url:bugstar:3892775 and url:bugstar:3892812
		ATSTRINGHASH("w_ex_vehiclemissile_2", 0xe9e8c70),
		ATSTRINGHASH("w_ex_vehiclemissile_3", 0x49efc7),
		ATSTRINGHASH("w_lr_rpg_rocket", 0x9a3207b7),
	};

	int nElements = sizeof(ignoreWarpingModels) / sizeof(atHashString);
	for(int i=0; i<nElements; i++)
	{
		if(pObject->GetModelNameHash() == ignoreWarpingModels[i])
			return true;
	}

	return IsLuxorPlaneObjectByModelNameHash(pObject) || IsLuxorPlaneObjectByAttachment(pObject);
}


// Moves the entities bounding box to where the skeleton root is. Inside the Luxor plane the skeleton root is modified to make objects move with the plane.
// This results in an entity matrix (and hence bounding box) which is not in the correct place. 
void CPacketObjectUpdate::UpdateBoundingBoxForLuxorPlane(CObject *pObject)
{
	if(!ShouldIgnoreWarpingFlags(pObject))
		return;

	crSkeleton *pSkeleton = pObject->GetSkeleton();

	if(pSkeleton)
	{

		bool assertOn = false;
		fwAttachmentEntityExtension* pExtension = pObject->GetAttachmentExtension();

		if (pExtension) 
		{
			assertOn = pExtension->GetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE);
			pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
		}

		Mat34V entityMatrix;
		Mat34V tempEntityMatrix;
		Mat34V rootMatrixInWorldSpace;

		entityMatrix = pObject->GetMatrix();	
		pSkeleton->GetGlobalMtx(0, rootMatrixInWorldSpace);

		// Set a new temp entity matrix to shift the bounding box to where the skeleton root is.
		tempEntityMatrix = entityMatrix;
		tempEntityMatrix.SetCol3(rootMatrixInWorldSpace.GetCol3());
		// Just update the game world (it just updates the bounding box).
		pObject->SetMatrix(MAT34V_TO_MATRIX34(tempEntityMatrix), true, false);

		// Reset the entity matrix without altering the bounding box for the object (i.e. just update the matrix and physics).
		pObject->SetMatrix(MAT34V_TO_MATRIX34(entityMatrix), false, true);

		if(pExtension) 
			pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, assertOn);

	}
}


//////////////////////////////////////////////////////////////////////////
// Store data in a Packet for Deleting a Ped
void CPacketObjectDelete::Store(const CObject* pObject, const CPacketObjectCreate* pLastCreatedPacket)
{
	//clone our last know good creation packet and setup the correct new ID and size
	CloneBase<CPacketObjectCreate>(pLastCreatedPacket);
	//We need to use the version from the create packet to version the delete packet as it inherits from it.
	CPacketBase::Store(PACKETID_OBJECTDELETE, sizeof(CPacketObjectDelete), pLastCreatedPacket->GetPacketVersion());
		
	FrameRef creationFrame;
	pObject->GetCreationFrameRef(creationFrame);
	SetFrameCreated(creationFrame);

	m_convertedToDummyObject = 0;
	m_unused = 0;
	m_unusedA[0] = 0;
	m_unusedA[1] = 0;
	m_unusedA[2] = 0;
	ReplayObjectExtension* pExtension = ReplayObjectExtension::GetExtension(pObject);
	if(pExtension)
	{
		m_convertedToDummyObject = pExtension->GetDeletedAsConvertedToDummy();
	}

	replayFatalAssertf(GetPacketSize() <= MAX_OBJECT_DELETION_SIZE, "Size of Object Deletion packet is too large! (%u)", GetPacketSize());
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectDelete::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);
	CPacketEntityInfo::PrintXML(handle);

	GetCreatePacket()->PrintXML(handle);
}



//========================================================================================================================
void CPacketDoorUpdate::Store(CDoor *pDoor, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketDoorUpdate);
	replayAssert(pDoor->IsADoor());

	CPacketObjectUpdate::Store(PACKETID_OBJECTDOORUPDATE, sizeof(CPacketDoorUpdate), pDoor, storeCreateInfo);

	m_ReplayDoorUpdating = false;
	if(pDoor->GetDoorAudioEntity())
	{
		m_ReplayDoorUpdating = pDoor->GetDoorAudioEntity()->IsDoorUpdating();
		m_CurrentVelocity =  pDoor->GetDoorAudioEntity()->GetCurrentVelocity();
		m_Heading = pDoor->GetDoorAudioEntity()->GetHeading();
		m_OriginalHeading = pDoor->GetDoorAudioEntity()->GetOriginalHeading();
	}
}

//========================================================================================================================
void CPacketDoorUpdate::Extract(const CInterpEventInfo<CPacketDoorUpdate, CDoor>& info) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketDoorUpdate) == 0);
	CDoor* pDoor = info.GetEntity();
	replayAssert(pDoor->IsADoor());
	CPacketObjectUpdate::Extract(info);

	pDoor->SetFixedPhysics(true);

	if(pDoor->GetDoorAudioEntity())
	{
		pDoor->GetDoorAudioEntity()->SetReplayDoorUpdating(m_ReplayDoorUpdating);
		pDoor->GetDoorAudioEntity()->ProcessAudio(m_CurrentVelocity ,m_Heading, m_OriginalHeading);
	}

	//Set the map object door to have the same open ratio as the replay door for occlusion purposes. 
	CObject* pMapObject = pDoor->ReplayGetMapObject();	
	if(pMapObject  && pMapObject->IsADoor())
	{
		CDoor* pMapDoor = static_cast<CDoor*>(pMapObject);
		pMapDoor->SetOpenDoorRatio(pDoor->GetDoorOpenRatio());
	}
}

//========================================================================================================================
void CPacketAnimatedObjBoneData::Store(const CObject* pObject)
{
	PACKET_EXTENSION_RESET(CPacketAnimatedObjBoneData);
	replayAssert(pObject && "CPacketAnimatedObjBoneData::Store: Invalid entity");

	CPacketBase::Store(PACKETID_ANIMATEDOBJDATA, sizeof(CPacketAnimatedObjBoneData));

	rage::crSkeleton* pSkeleton = pObject->GetSkeleton();
	if (pSkeleton)
	{
		bool forceObjMtx = false;
		if( pObject->GetBaseModelInfo() )
		{
			if( pObject->GetIsParachute() )
			{
				forceObjMtx = true;
			}
		}

		// Form an aligned pointer to the undamaged skeleton packet.
		CSkeletonBoneData<CPacketQuaternionL> *pSkeletonPacket = (CSkeletonBoneData<CPacketQuaternionL> *)REPLAY_ALIGN((ptrdiff_t)(&this[1]), SKELETON_BONE_DATA_ALIGNMENT);
		// Record the offset.
		m_OffsetToBoneData = (u16)((ptrdiff_t)pSkeletonPacket - (ptrdiff_t)this);

		float translationEpsilon = SKELETON_BONE_DATA_TRANSLATION_EPSILON;

		// If we're recording bones for the stungun then use an epsilon of zero to allow us to 
		// catch the recharge bone properly. Fix for url:bugstar:7410391
		if(pObject->GetModelNameHash() == ATSTRINGHASH("w_pi_stungun", 0x5FECD5DB))
		{
			translationEpsilon = 0.0001f;
		}

		// Store the data.
		pSkeletonPacket->Store(pSkeleton, translationEpsilon, SKELETON_BONE_DATA_QUATERNION_EPSILON, GetZeroScaleEpsilon(pObject), forceObjMtx);

		// Account for all the skeleton data written.
		pSkeletonPacket = (CSkeletonBoneData<CPacketQuaternionL> *)pSkeletonPacket->TraversePacketChain(1, false);
		u32 amountToAdd = (u32)((u8 *)pSkeletonPacket - (u8 *)this) - GetPacketSize();
		AddToPacketSize(amountToAdd);
	}

#if REPLAY_GUARD_ENABLE
	AddToPacketSize(sizeof(int));
	int* pEnd = (int*)((u8*)this + GetPacketSize() - sizeof(int));
	*pEnd = REPLAY_GUARD_END_DATA;
#endif // REPLAY_GUARD_ENABLE

}

//========================================================================================================================
void CPacketAnimatedObjBoneData::Extract(CObject* pObject, CPacketAnimatedObjBoneData const* pNextPacket, float fInterpValue, bool bUpdateWorld, const atArray<sInterpIgnoredBone>* noInterpBones) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketAnimatedObjBoneData) == 0);
	replayFatalAssertf(pObject, "pObject is NULL in CPacketAnimatedObjBoneData");

	if (pObject)
	{
		rage::crSkeleton* pSkeleton = pObject->GetSkeleton();

		if( pSkeleton )
		{
			if (pObject->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterpValue == 0.0f)
			{
				// Just use "previous" frame.
				GetBoneData()->Extract(pObject, pSkeleton, GetZeroScaleEpsilon(pObject), pObject->GetFragInst());
			}
			else if (pNextPacket)
			{
				// Interpolate between frames.
				GetBoneData()->Extract(pSkeleton, pNextPacket->GetBoneData(), fInterpValue, GetZeroScaleEpsilon(pObject), pObject->GetFragInst(), nullptr, nullptr, nullptr, noInterpBones);
			}
			else
			{
				replayAssert(0 && "CPacketAnimatedObjBoneData::Load - interpolation couldn't find a next packet");
			}

			if (bUpdateWorld)
			{
				CGameWorld::Remove((CEntity*)pObject);
				CGameWorld::Add((CEntity*)pObject, CGameWorld::OUTSIDE);
			}
		}
	}
}



//////////////////////////////////////////////////////////////////////////
CPacketAnimatedObjBoneData::EPSILON_AND_HASH CPacketAnimatedObjBoneData::ms_ZeroScaleEpsilonAndHash[] = 
{
	{ 0.0001f,  ATSTRINGHASH("lux_p_champ_flute_s", 0xe61117bd) },
};


// Essentially as hack/fix for B*2330419 - Need specially low scale values for the liquid in the glasses.
float CPacketAnimatedObjBoneData::GetZeroScaleEpsilon(const CObject *pObject)
{
	int nElements = sizeof(ms_ZeroScaleEpsilonAndHash) / sizeof(EPSILON_AND_HASH);
	for(int i=0; i<nElements; i++)
	{
		EPSILON_AND_HASH &tableEntry = ms_ZeroScaleEpsilonAndHash[i];
		if( pObject->GetModelNameHash() == tableEntry.hashString.GetHash() )
		{
			return tableEntry.epsilon;
		}
	}
	// Use the default zero scale epsilon.
	return SKELETON_BONE_DATA_ZERO_SCALE_EPSILON_SQR;
}


//////////////////////////////////////////////////////////////////////////
s32 CPacketAnimatedObjBoneData::EstimatePacketSize(const CObject* pObject)
{
	replayAssert(pObject && "CPacketAnimatedObjBoneData::EstimatePacketSize: Invalid entity");

	u32 ret = 0;
	rage::crSkeleton* pSkeleton = pObject->GetSkeleton();

	if (pSkeleton && pSkeleton->GetBoneCount() > 0)
	{
		// Allow for alignment of skeleton packets.
		ret += SKELETON_BONE_DATA_ALIGNMENT - 1;
		ret += CSkeletonBoneData<CPacketQuaternionL>::EstimateSize(pSkeleton);
	}

#if REPLAY_GUARD_ENABLE
	// Adding an int on the end as a guard
	ret += sizeof(int);
#endif // REPLAY_GUARD_ENABLE

	return ret + sizeof(CPacketAnimatedObjBoneData);	
}

//========================================================================================================================
void CObjInterp::Init(const ReplayController& controller, CPacketObjectUpdate const* pPrevPacket)
{
	// Setup the previous packet
	SetPrevPacket(pPrevPacket);

	CPacketObjectUpdate const* pObjectPrevPacket = GetPrevPacket();
	m_prevData.m_fullPacketSize += pObjectPrevPacket->GetPacketSize();

	HookUpAnimBoneAndFrags(pObjectPrevPacket, m_prevData);

	// Look for/Setup the next packet
	if (pPrevPacket->IsNextDeleted())
	{
		m_nextData.Reset();
	}
	else if (pPrevPacket->HasNextOffset())
	{
		CPacketObjectUpdate const* pNextPacket = NULL;
		controller.GetNextPacket(pPrevPacket, pNextPacket);
		SetNextPacket(pNextPacket);

		CPacketObjectUpdate const* pObjectNextPacket = GetNextPacket();
#if __ASSERT
		CPacketObjectUpdate const* pObjectPrevPacket = GetPrevPacket();
		replayAssert(pObjectPrevPacket->GetReplayID() == pObjectNextPacket->GetReplayID());
#endif // __ASSERT

		m_nextData.m_fullPacketSize += pObjectNextPacket->GetPacketSize();

		HookUpAnimBoneAndFrags(pObjectNextPacket, m_nextData); 		
	}
	else
	{
		replayAssertf(false, "CObjInterp::Init");
	}
}


//////////////////////////////////////////////////////////////////////////
void CObjInterp::HookUpAnimBoneAndFrags(CPacketObjectUpdate const* pObjectPacket, CObjInterpData& data)
{
	// Frag Data
	data.m_hasFragData = pObjectPacket->HasFragInst();
	data.m_pFragDataPacket = NULL;
	if (data.m_hasFragData)
	{
		data.m_pFragDataPacket = (CPacketBase *)((u8*)pObjectPacket + data.m_fullPacketSize);
		replayAssertf((data.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA_NO_DAMAGE_BITS) || (data.m_pFragDataPacket->GetPacketID() == PACKETID_FRAGDATA) , "CObjInterp::HookUpAnimBoneAndFrags()....Expecting frag data packet.");
		data.m_fullPacketSize += data.m_pFragDataPacket->GetPacketSize();
	}

	// Frag Bone
	data.m_fragBoneCount = (pObjectPacket->HasFragInst() && pObjectPacket->GetFragBoneCount() > 0) ? pObjectPacket->GetFragBoneCount() : 0;
	data.m_pFragBonePacket = NULL;
	if (data.m_fragBoneCount > 0)
	{
		replayAssert(data.m_hasFragData && data.m_fragBoneCount > 0);
		data.m_pFragBonePacket = (CPacketFragBoneData const*)((u8*)pObjectPacket + data.m_fullPacketSize);
		replayFatalAssertf(data.m_pFragBonePacket->GetPacketID() == PACKETID_FRAGBONEDATA, "Invalid packet");
		data.m_fullPacketSize += data.m_pFragBonePacket->GetPacketSize();
	}

	// AnimObjBones
	data.m_animObjBoneCount = 0;
	data.m_pAnimObjBonePacket = NULL;
	CPacketAnimatedObjBoneData *pAnimObjBoneDataPkt = (CPacketAnimatedObjBoneData*)((u8*)pObjectPacket + data.m_fullPacketSize);

	// Is the next packet anim obj bone data ?
	if(pObjectPacket->GetBoneCount() && pAnimObjBoneDataPkt->GetPacketID() == PACKETID_ANIMATEDOBJDATA)
	{
		data.m_animObjBoneCount = pObjectPacket->GetBoneCount();
		data.m_pAnimObjBonePacket = pAnimObjBoneDataPkt;
		replayFatalAssertf(data.m_pAnimObjBonePacket->GetPacketID() == PACKETID_ANIMATEDOBJDATA, "Invalid packet");
		data.m_fullPacketSize += data.m_pAnimObjBonePacket->GetPacketSize();
	}
}


//////////////////////////////////////////////////////////////////////////
void CObjInterp::Reset()
{
	CInterpolator<CPacketObjectUpdate>::Reset();

	m_prevData.Reset();
	m_nextData.Reset();
}

//========================================================================================================================
CPacketFragBoneData const* CObjInterp::GetPrevFragBonePacket()
{
	if (GetPrevPacket() == NULL)
		return NULL;

	replayAssert(m_prevData.m_pFragBonePacket != NULL);

	CPacketFragBoneData const* pPacketFragBoneUpdate = (CPacketFragBoneData const*)((u8*)m_prevData.m_pFragBonePacket);
	replayFatalAssertf(pPacketFragBoneUpdate->GetPacketID() == PACKETID_FRAGBONEDATA, "Invalid packet");
	return pPacketFragBoneUpdate;
}

//========================================================================================================================
CPacketFragBoneData const* CObjInterp::GetNextFragBonePacket()
{
	if (GetNextPacket() == NULL)
		return NULL;

	replayAssert(m_nextData.m_pFragBonePacket != NULL);

	CPacketFragBoneData const* pPacketFragBoneUpdate = (CPacketFragBoneData const*)((u8*)m_nextData.m_pFragBonePacket);
	replayFatalAssertf(pPacketFragBoneUpdate->GetPacketID() == PACKETID_FRAGBONEDATA, "Invalid packet");
	return pPacketFragBoneUpdate;
}

//========================================================================================================================
CPacketAnimatedObjBoneData const* CObjInterp::GetPrevAnimObjBonePacket()
{
	if (GetPrevPacket() == NULL)
		return NULL;

	replayAssert(m_prevData.m_pAnimObjBonePacket != NULL);

	CPacketAnimatedObjBoneData const* pPacketAnimObjBoneUpdate = (CPacketAnimatedObjBoneData const*)((u8*)m_prevData.m_pAnimObjBonePacket);
	replayFatalAssertf(pPacketAnimObjBoneUpdate->GetPacketID() == PACKETID_ANIMATEDOBJDATA, "Invalid packet");
	return pPacketAnimObjBoneUpdate;
}

//========================================================================================================================
CPacketAnimatedObjBoneData const* CObjInterp::GetNextAnimObjBonePacket()
{
	if (GetNextPacket() == NULL)
		return NULL;

	replayAssert(m_nextData.m_pAnimObjBonePacket != NULL);

	CPacketAnimatedObjBoneData const* pPacketAnimObjBoneUpdate = (CPacketAnimatedObjBoneData const*)((u8*)m_nextData.m_pAnimObjBonePacket);
	replayFatalAssertf(pPacketAnimObjBoneUpdate->GetPacketID() == PACKETID_ANIMATEDOBJDATA, "Invalid packet");
	return pPacketAnimObjBoneUpdate;
}


//////////////////////////////////////////////////////////////////////////
void CPacketHiddenMapObjects::Store(const atArray<mapObjectID>& ids)
{
	PACKET_EXTENSION_RESET(CPacketHiddenMapObjects);
	CPacketBase::Store(PACKETID_OBJECTHIDDENMAP, sizeof(CPacketHiddenMapObjects));

	m_sizeOfBasePacket = sizeof(CPacketHiddenMapObjects);

	u8* p = GetIDsPtr();
	sysMemCpy(p, ids.GetElements(), sizeof(mapObjectID) * ids.GetCount());

	m_hiddenMapObjectCount = ids.GetCount();

	AddToPacketSize(sizeof(mapObjectID) * ids.GetCount());
}


//////////////////////////////////////////////////////////////////////////
void CPacketHiddenMapObjects::Extract(atArray<mapObjectID>& ids) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketHiddenMapObjects) == 0);
	ids.CopyFrom((const mapObjectID*)GetIDsPtr(), (u16)m_hiddenMapObjectCount);
}


//////////////////////////////////////////////////////////////////////////
void CPacketHiddenMapObjects::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<m_hiddenMapObjectCount>%u</m_hiddenMapObjectCount>\n", m_hiddenMapObjectCount);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<m_hiddenMapObjectCount>");
	CFileMgr::Write(handle, str, istrlen(str));

	for(u32 i = 0; i < m_hiddenMapObjectCount; ++i)
	{
		snprintf(str, 1024, "%u ", ((const mapObjectID*)GetIDsPtr())[i].objectHash);
		CFileMgr::Write(handle, str, istrlen(str));
	}
	snprintf(str, 1024, "</m_hiddenMapObjectCount>\n");
	CFileMgr::Write(handle, str, istrlen(str));
}

//////////////////////////////////////////////////////////////////////////
CPacketObjectSniped::CPacketObjectSniped()
	:	CPacketEvent(PACKETID_OBJECT_SNIPED, sizeof(CPacketObjectSniped))
	,	CPacketEntityInfo()
{
	//Nothing to store, just need the object
}

//////////////////////////////////////////////////////////////////////////
void CPacketObjectSniped::Extract(CEventInfo<CObject>& info) const
{
	CObject* pObject = info.GetEntity(0);
	if(pObject )
	{
		//we need to tell the map version that it's been sniped, otherwise
		//it will get converted into a dummy object which we don't wont
		u32 mapHash = ReplayObjectExtension::GetExtension(pObject)->GetMapHash();

		CObject* pRelatedMapObject = NULL;

		//search the object pool first
		for(int i = 0; i < CObject::GetPool()->GetSize(); ++i)
		{
			CObject* pMapObject = CObject::GetPool()->GetSlot(i);
			if(pMapObject && pMapObject->GetHash() == mapHash)
			{
				pRelatedMapObject = pMapObject;
				break;
			}
		}

		//if we don't find it in the object pool, search the object dummy pool and then
		//promote it to be an actual object so we can apply the correct flags
		if(pRelatedMapObject == NULL)
		{
			for(int i = 0; i < CDummyObject::GetPool()->GetSize(); ++i)
			{
				CDummyObject* pDummyMapObject = CDummyObject::GetPool()->GetSlot(i);
				if(pDummyMapObject && pDummyMapObject->GetHash() == mapHash)
				{
					pRelatedMapObject = CObjectPopulation::ConvertToRealObject(pDummyMapObject);
					break;
				}
			}
		}

		//it's possible that it's been completely unloaded so we still need to check here
		if(pRelatedMapObject)
		{
			pRelatedMapObject->ProcessSniperDamage();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPacketObjectSniped::HasExpired(const CEventInfo<CObject>& info) const
{
	const CObject* pObject = info.GetEntity();
	if( pObject )
	{
		return pObject->RecentlySniped() == false;
	}

	return true;
}


atMap<CDummyObject*, int>	CPacketSetDummyObjectTint::sm_dummyObjects;
CPacketSetDummyObjectTint::CPacketSetDummyObjectTint(Vec3V pos, u32 hash, int tint, CDummyObject* pDummy)
	: CPacketEvent(PACKETID_SETDUMMYOBJECTTINT, sizeof(CPacketSetDummyObjectTint))
	, m_objectModelHash(hash)
	, m_tint(tint)
	, m_pDummy(pDummy)
{
	m_position.Store(VEC3V_TO_VECTOR3(pos));

	if(sm_dummyObjects.Access(m_pDummy) == nullptr)
	{
		sm_dummyObjects.Insert(m_pDummy, 1);
	}
	else
	{
		sm_dummyObjects[m_pDummy]++;
	}
}


void CPacketSetDummyObjectTint::SetTintOnEntity(CEntity* pEntity) const
{
	pEntity->SetTintIndex(m_tint);
	CCustomShaderEffectTint *pTintCSE = nullptr;
	if( pEntity->GetDrawHandlerPtr() )
	{
		fwCustomShaderEffect *fwCSE = pEntity->GetDrawHandler().GetShaderEffect();
		if(fwCSE && fwCSE->GetType()==CSE_TINT)
		{
			pTintCSE = static_cast<CCustomShaderEffectTint*>(fwCSE);
		}
	}

	if(pTintCSE)
	{
		pTintCSE->SelectTintPalette((u8)pEntity->GetTintIndex(), pEntity);
	}
}

void CPacketSetDummyObjectTint::Extract(CEventInfo<void>& /*pEventInfo*/) const
{
	Vector3 pos;
	m_position.Load(pos);
	CEntity* pEntity = CObject::GetPointerToClosestMapObjectForScript(pos.GetX(), pos.GetY(), pos.GetZ(), 
																		100.0f, m_objectModelHash, ENTITY_TYPE_MASK_OBJECT, ENTITY_OWNEDBY_MASK_REPLAY);
	CDummyObject* pDummy = nullptr;

	if(pEntity && pEntity->GetIsTypeDummyObject())
	{
		CDummyObject* pDummy = static_cast<CDummyObject*>(pEntity);
		SetTintOnEntity(pDummy);
	}
	else if(pEntity && pEntity->GetIsTypeObject())
	{
		SetTintOnEntity(pEntity);

		CObject* pObject = static_cast<CObject*>(pEntity);
		pDummy = pObject->GetRelatedDummy();
		if(pDummy)
			SetTintOnEntity(pDummy);
	}
}


bool CPacketSetDummyObjectTint::HasExpired(const CEventInfo<void>& /*info*/) const
{
	int* p = sm_dummyObjects.Access(m_pDummy);
	if(!p)	// Map doesn't contain this dummy...expire
		return true;

	if(*p > 1)
	{
		--(*p); // Map says we have more than one packet pointed to this dummy...expire
		return true;
	}

	Vector3 pos;
	m_position.Load(pos);
	CEntity* pEntity = CObject::GetPointerToClosestMapObjectForScript(pos.GetX(), pos.GetY(), pos.GetZ(), 
		100.0f, m_objectModelHash, ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT, 0xFFFFFFFF);
	if(!pEntity)
	{
		// Entity no longer exists...expire
		if(p)
		{
			replayAssert(*p == 1);
			sm_dummyObjects.Delete(m_pDummy);
		}
		return true;
	}
	
	return false;
}


atMap<CReplayID, int>	CPacketSetObjectTint::sm_objects;
tPacketVersion CPacketSetObjectTint_V1 = 1;
CPacketSetObjectTint::CPacketSetObjectTint(int tint, CObject* pObject)
	: CPacketEvent(PACKETID_SETOBJECTTINT, sizeof(CPacketSetObjectTint), CPacketSetObjectTint_V1)
	, m_tint(tint)
{
	if(sm_objects.Access(pObject->GetReplayID()) == nullptr)
	{
		sm_objects.Insert(pObject->GetReplayID(), 1);
	}
	else
	{
		sm_objects[pObject->GetReplayID()]++;
	}
}


void CPacketSetObjectTint::SetTintOnEntity(CEntity* pEntity, bool forwards) const
{
	u32 tint = m_tint;

	if(forwards && (pEntity->GetTintIndex() == tint))
	{
		m_prevTint = pEntity->GetTintIndex();
		return;
	}

	if(GetPacketVersion() >= CPacketSetObjectTint_V1)
	{
		if(forwards)
		{
			m_prevTint = pEntity->GetTintIndex();
		}
		else
		{
			tint = m_prevTint;
		}
	}
		
	pEntity->SetTintIndex(tint);
	CCustomShaderEffectTint *pTintCSE = nullptr;
	if( pEntity->GetDrawHandlerPtr() )
	{
		fwCustomShaderEffect *fwCSE = pEntity->GetDrawHandler().GetShaderEffect();
		if(fwCSE && fwCSE->GetType()==CSE_TINT)
		{
			pTintCSE = static_cast<CCustomShaderEffectTint*>(fwCSE);
		}
	}

	if(pTintCSE)
	{
		pTintCSE->SelectTintPalette((u8)pEntity->GetTintIndex(), pEntity);
	}
}

void CPacketSetObjectTint::Extract(CEventInfo<CObject>& info) const
{
	CObject* pObject = info.GetEntity();
	if(pObject)
	{
		SetTintOnEntity(pObject, info.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD));
	}
}


bool CPacketSetObjectTint::HasExpired(const CEventInfo<CObject>& info) const
{
	int* p = sm_objects.Access(GetReplayID());
	if(!p)	// Map doesn't contain this object...expire
	{
		return true;
	}

	if(*p > 1)
	{
		--(*p); // Map says we have more than one packet pointed to this object...expire
		return true;
	}

	const CObject* pObject = info.GetEntity();
	if(!pObject)
	{
		// object no longer exists...expire
		if(p)
		{
			replayAssert(*p == 1);
			sm_objects.Delete(GetReplayID());
		}
		return true;
	}

	return false;
}

atMap<int, Color32> CPacketOverrideObjectLightColour::sm_colourOverrides;

tPacketVersion CPacketOverridObjectLightColour_V1 = 1;
CPacketOverrideObjectLightColour::CPacketOverrideObjectLightColour(Color32 previousCol, Color32 newCol)
	: CPacketEvent(PACKETID_OVERRIDEOBJECTLIGHTCOLOUR, sizeof(CPacketOverrideObjectLightColour), CPacketOverridObjectLightColour_V1)
	, m_colour(newCol)
	, m_prevColour(previousCol)
{

}

void CPacketOverrideObjectLightColour::Extract(CEventInfo<CObject>& pEventInfo) const
{
	CObject* pObject = pEventInfo.GetEntity();
	if(pObject)
	{
		const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

		Color32 colour;
		if(pEventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
		{
			colour = m_colour;
		}
		else
		{
			colour = m_prevColour;
		}
		
		for(s32 i=0; i<maxNumLightEntities; i++)
		{
			if(CLightEntity::GetPool()->GetIsUsed(i))
			{
				CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
				if(pLightEntity && (pLightEntity->GetParent() == pObject))
				{
					pLightEntity->OverrideLightColor(colour.GetAlpha() != 0, colour.GetRed(), colour.GetGreen(), colour.GetBlue());
				}
			}
		}
	}
}

bool CPacketOverrideObjectLightColour::HasExpired(const CEventInfo<CObject>& info) const
{
	// If the object no longer exists then expire
 	CObject* pObject = info.GetEntity();
 	if(!pObject)
 	{
 		// Also remove the cached override as it's no longer needed
 		RemoveColourOverride(GetReplayID());
 		return true;
 	}

	// If an override for this object doesn't exist or the colour is different then the packet is no longer needed
	Color32* pOverriddenColour = sm_colourOverrides.Access(pObject->GetReplayID().ToInt());
	if(pOverriddenColour == nullptr || m_colour != *pOverriddenColour)
	{
		return true;
	}

#if __BANK && 0
	bool found = false;
	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();
	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if(pLightEntity && (pLightEntity->GetParent() == pObject))
			{
				replayAssert(pLightEntity->GetOverridenLightColor() == m_colour);
				found = true;
				break;
			}
		}
	}
#endif // __BANK && 0 

	return false;
}


void CPacketOverrideObjectLightColour::StoreColourOverride(CReplayID objectID, Color32 colour)
{
	Color32 *pCol = sm_colourOverrides.Access(objectID.ToInt());
	if(pCol)
	{
		*pCol = colour;
	}
	else
	{
		sm_colourOverrides.Insert(objectID.ToInt(), colour);
	}

#if __BANK && 0
	static bool printOutMap = false;
	if(printOutMap)
	{
		atArray<int> o;
		atMap<int, Color32>::Iterator it = sm_colourOverrides.CreateIterator();
		while(!it.AtEnd())
		{
			replayDebugf1("0x%08X, %X", it.GetKey(), it.GetData().GetColor());

			if(replayVerify(o.Find(it.GetKey()) == -1))
			{
				o.PushAndGrow(it.GetKey());
			}

			it.Next();
		}

		printOutMap = false;
	}
#endif // __BANK && 0
}
bool CPacketOverrideObjectLightColour::GetColourOverride(CReplayID objectID, Color32*& colour)
{
	colour = sm_colourOverrides.Access(objectID.ToInt());
	return colour != nullptr;
}
void CPacketOverrideObjectLightColour::RemoveColourOverride(CReplayID objectID)
{
	sm_colourOverrides.Delete(objectID.ToInt());
}
int CPacketOverrideObjectLightColour::GetColourOverrideCount()
{
	return sm_colourOverrides.GetNumUsed();
}

atMap<CBuilding*, int>	CPacketSetBuildingTint::sm_buildings;
bool CPacketSetBuildingTint::sm_tintSuccess = true;
atArray<CPacketSetBuildingTint const*> CPacketSetBuildingTint::sm_deferredPackets;
CPacketSetBuildingTint::CPacketSetBuildingTint(Vec3V pos, float radius, u32 hash, int tint, CBuilding* pBuilding)
	: CPacketEvent(PACKETID_SETBUILDINGTINT, sizeof(CPacketSetBuildingTint))
	, m_radius(radius)
	, m_objectModelHash(hash)
	, m_tint(tint)
	, m_pBuilding(pBuilding)
{
	m_position.Store(VEC3V_TO_VECTOR3(pos));

	if(sm_buildings.Access(m_pBuilding) == nullptr)
	{
		sm_buildings.Insert(m_pBuilding, 1);
	}
	else
	{
		sm_buildings[m_pBuilding]++;
	}
}


void CPacketSetBuildingTint::SetTintOnEntity(CEntity* pEntity) const
{
	pEntity->SetTintIndex(m_tint);
	CCustomShaderEffectTint *pTintCSE = nullptr;
	if( pEntity->GetDrawHandlerPtr() )
	{
		fwCustomShaderEffect *fwCSE = pEntity->GetDrawHandler().GetShaderEffect();
		if(fwCSE && fwCSE->GetType()==CSE_TINT)
		{
			pTintCSE = static_cast<CCustomShaderEffectTint*>(fwCSE);
		}
	}

	if(pTintCSE)
	{
		pTintCSE->SelectTintPalette((u8)pEntity->GetTintIndex(), pEntity);
	}
}

void CPacketSetBuildingTint::Extract(CEventInfo<void>& /*pEventInfo*/) const
{
	Vector3 pos;
	m_position.Load(pos);
	CEntity* pEntity = CObject::GetPointerToClosestMapObjectForScript(pos.GetX(), pos.GetY(), pos.GetZ(), 
		m_radius, m_objectModelHash, ENTITY_TYPE_MASK_BUILDING, 0xFFFFFFFF);

	if(pEntity && pEntity->GetIsTypeBuilding())
	{
		SetTintOnEntity(pEntity);

		sm_tintSuccess = true;
	}
	else
	{
		if(sm_deferredPackets.Find(this) == -1)
			sm_deferredPackets.PushAndGrow(this);
	}
}


bool CPacketSetBuildingTint::HasExpired(const CEventInfo<void>& /*info*/) const
{
	int* p = sm_buildings.Access(m_pBuilding);
	if(!p)	// Map doesn't contain this building...expire
		return true;

	if(*p > 1)
	{
		--(*p); // Map says we have more than one packet pointed to this building...expire
		return true;
	}

	Vector3 pos;
	m_position.Load(pos);
	CEntity* pEntity = CObject::GetPointerToClosestMapObjectForScript(pos.GetX(), pos.GetY(), pos.GetZ(), 
		m_radius, m_objectModelHash, ENTITY_TYPE_MASK_BUILDING, 0xFFFFFFFF);
	if(!pEntity)
	{
		// Entity no longer exists...expire
		if(p)
		{
			replayAssert(*p == 1);
			sm_buildings.Delete(m_pBuilding);
		}
		return true;
	}

	return false;
}


#endif // GTA_REPLAY
