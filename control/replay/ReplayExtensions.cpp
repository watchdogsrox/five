#include "ReplayExtensions.h"

#if GTA_REPLAY

#include "replay_channel.h"
#include "scene/Entity.h"
#include "Objects/object.h"
#include "pickups/Pickup.h"
#include "Peds/ped.h"
#include "fwscene/stores/maptypesstore.h"


atFixedArray<IReplayExtensionsController*, 16>	CReplayExtensionManager::m_extensionControllers;

bool ReplayEntityExtension::AllowCreationFrameResetting = false;

u8 QuantizeU8(float fValue, float fMin, float fMax, int iBits);

s16	ReplayEntityExtension::GetExtIndex(const CEntity* pEntity)
{
	const CPhysical* pPhysical = pEntity->GetIsPhysical() ? static_cast<const CPhysical*>(pEntity) : NULL;
	if(!pPhysical)
	{
		return -1;
	}

	const CReplayID& id = pPhysical->GetReplayID();
	s16 index = id.GetEntityID();
	if(index < 0)
	{
		return index;
	}

	if(pPhysical->GetIsTypeObject())
	{
		const CObject* pObject = static_cast<const CObject*>(pPhysical);

		if(pObject->IsPickup())
		{
			index += (s16)CObject::GetPool()->GetSize();
		}
		return index;
	}

	index += (s16)CObject::GetPool()->GetSize();
	index += (s16)CPickup::GetPool()->GetSize();

	if(pEntity->GetIsTypePed())
	{
		return index;
	}

	index += (s16)CPed::GetPool()->GetSize();

	Assert(pEntity->GetIsTypeVehicle());
	return index;
}


#define ENABLE_RECOUNT_PRINTING 0
//////////////////////////////////////////////////////////////////////////
ReplayObjectExtension::~ReplayObjectExtension()
{
#if ENABLE_RECOUNT_PRINTING
	if(m_strModelRequest.GetTypSlotIdx() != strLocalIndex::INVALID_INDEX)
	{
		char pBuff[128] = {0};
		g_MapTypesStore.GetRefCountString(strLocalIndex(m_strModelRequest.GetTypSlotIdx()), pBuff, 128);
		replayDebugf1("Removing Ref %u, string: %s", strLocalIndex(m_strModelRequest.GetTypSlotIdx()), pBuff);
	}
#endif // ENABLE_RECOUNT_PRINTING
}


//////////////////////////////////////////////////////////////////////////
void ReplayObjectExtension::SetStrModelRequest(const strModelRequest& req)
{
	m_strModelRequest = req;

#if ENABLE_RECOUNT_PRINTING
	if(m_strModelRequest.GetTypSlotIdx() != strLocalIndex::INVALID_INDEX)
	{
		char pBuff[128] = {0};
		g_MapTypesStore.GetRefCountString(strLocalIndex(m_strModelRequest.GetTypSlotIdx()), pBuff, 128);
		replayDebugf1("Adding Ref %u, string: %s", strLocalIndex(m_strModelRequest.GetTypSlotIdx()), pBuff);
	}
#endif // ENABLE_RECOUNT_PRINTING
}


//////////////////////////////////////////////////////////////////////////
u32 ReplayObjectExtension::GetPropHash(const CEntity* pObject)
{
	const ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	if(extension)
	{
		return extension->m_uPropHash;
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void ReplayObjectExtension::SetPropHash(CEntity* pObject, u32 id)
{
	ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	replayAssertf(extension != NULL, "Error: trying to set a PropID when it doesn't have the extension for it\n");
	extension->m_uPropHash = id;
}

//////////////////////////////////////////////////////////////////////////
u32 ReplayObjectExtension::GetTexHash(const CEntity* pObject)
{
	const ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	if(extension)
	{
		return extension->m_uTexHash;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void ReplayObjectExtension::SetTextHash(CEntity* pObject, u32 id)
{
	ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	replayAssertf(extension != NULL, "Error: trying to set a TextID when it doesn't have the extension for it\n");
	extension->m_uTexHash = id;
}

//////////////////////////////////////////////////////////////////////////
u8 ReplayObjectExtension::GetAnchorID(const CEntity* pObject)
{
	const ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	if(extension)
	{
		return extension->m_uAnchorID;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void ReplayObjectExtension::SetAnchorID(CEntity* pObject, u8 id)
{
	ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	replayAssertf(extension != NULL, "Error: trying to set a AnchorID when it doesn't have the extension for it\n");
	extension->m_uAnchorID = id;
}


//////////////////////////////////////////////////////////////////////////
bool ReplayObjectExtension::IsStreamedPedProp(const CEntity* pObject)
{
	const ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	if(extension)
	{
		return extension->m_streamedPedProp;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayObjectExtension::SetStreamedPedProp(CEntity* pObject, bool b)
{
	ReplayObjectExtension *extension = ReplayObjectExtension::GetExtension(pObject);
	replayAssertf(extension != NULL, "Error: trying to set a AnchorID when it doesn't have the extension for it\n");
	extension->m_streamedPedProp = b;
}


//////////////////////////////////////////////////////////////////////////
void ReplayPedExtension::Reset()
{	
	m_PedDecorationData.Reset();

	ResetHeadBlendData();

	m_PedShelteredInVehicle = false;
	m_PedSetForcePin = true;
	m_rewindVariationSet.Reset();
}

void ReplayPedExtension::ResetHeadBlendData()
{
	m_MultiplayerHeadFlags = 0;

	for( u32 i = 0; i < MAX_PALETTE_COLORS; i++ )
	{
		m_ReplayHeadBlendPaletteData.m_paletteRed[i] = 0;
		m_ReplayHeadBlendPaletteData.m_paletteGreen[i] = 0;
		m_ReplayHeadBlendPaletteData.m_paletteBlue[i] = 0;
		m_ReplayHeadBlendPaletteData.m_forcePalette[i] = false;
		m_ReplayHeadBlendPaletteData.m_paletteSet[i] = false;
	}

	for( u32 i = 0; i < HOS_MAX; i++ )
	{
		m_ReplayMultiplayerHeadData.m_overlayAlpha[i] = 255;
		m_ReplayMultiplayerHeadData.m_overlayNormAlpha[i] = 255;
		m_ReplayMultiplayerHeadData.m_overlayTex[i] = 255;

		m_ReplayHeadOverlayTintData.m_overlayTintIndex[i] = 0;
		m_ReplayHeadOverlayTintData.m_overlayTintIndex2[i] = 0;
		m_ReplayHeadOverlayTintData.m_overlayRampType[i] = RT_NONE;
	}
	for( u32 i = 0; i < MMT_MAX; i++ )
		m_ReplayMultiplayerHeadData.m_microMorphBlends[i] = 0;

	m_ReplayMultiplayerHeadData.m_pedHeadBlendData.SetData(255, 255, 255, 255, 255, 255, 0.0f, 0.0f, 0.0f, false);

	m_ReplayHairTintData.m_HairTintIndex = 0;
	m_ReplayHairTintData.m_HairTintIndex2 = 0;

	m_EyeColorIndex = -1;
}

void ReplayPedExtension::CloneHeadBlendData(CEntity* pPed, const CPedHeadBlendData* cloneData)
{
	sReplayHeadPedBlendData replayData;
	replayData.SetData(cloneData->m_head0, cloneData->m_head1, cloneData->m_head2, cloneData->m_tex0, cloneData->m_tex1, cloneData->m_tex2,
		cloneData->m_headBlend, cloneData->m_texBlend, cloneData->m_varBlend, cloneData->m_isParent);

	SetHeadBlendData(pPed, replayData);

	for (s32 i = 0; i < HOS_MAX; ++i)
	{
		SetHeadOverlayData(pPed, (eHeadOverlaySlots)i, cloneData->m_overlayTex[i], cloneData->m_overlayAlpha[i], cloneData->m_overlayNormAlpha[i]);
		SetHeadOverlayTintData(pPed, (eHeadOverlaySlots)i, (eRampType)cloneData->m_overlayRampType[i], cloneData->m_overlayTintIndex[i], cloneData->m_overlayTintIndex2[i]);
	}

	for (s32 i = 0; i < MMT_MAX; ++i)
	{
		SetHeadMicroMorphData(pPed, (eMicroMorphType)i, cloneData->m_microMorphBlends[i]);
	}

	for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
	{
		SetHeadBlendDataPaletteColor(pPed, cloneData->m_paletteRed[i], cloneData->m_paletteGreen[i], cloneData->m_paletteBlue[i], i, false);
	}

	SetEyeColor(pPed, cloneData->m_eyeColor);
	SetHairTintIndex(pPed, cloneData->m_hairTintIndex, cloneData->m_hairTintIndex2);	
}

//////////////////////////////////////////////////////////////////////////
void ReplayPedExtension::SetHeadBlendData(CEntity* pPed, const sReplayHeadPedBlendData& replayHeadBlendData)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetHeadBlendData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_ReplayMultiplayerHeadData.m_pedHeadBlendData = replayHeadBlendData;
	extension->SetMultiplayerHeadDataFlags(REPLAY_HEAD_BLEND);
}

//////////////////////////////////////////////////////////////////////////
void ReplayPedExtension::SetHeadOverlayData(CEntity* pPed, u32 slot, u8 overlayTex, float overlayAlpha, float overlayNormAlpha)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetHeadOverlayData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_ReplayMultiplayerHeadData.m_overlayTex[slot] = overlayTex;
	extension->m_ReplayMultiplayerHeadData.m_overlayAlpha[slot] = QuantizeU8(overlayAlpha, 0.0, 1.0, 8);
	extension->m_ReplayMultiplayerHeadData.m_overlayNormAlpha[slot] = QuantizeU8(overlayNormAlpha, 0.0, 1.0, 8);
	extension->SetMultiplayerHeadDataFlags(REPLAY_HEAD_OVERLAY);
}

//////////////////////////////////////////////////////////////////////////
void ReplayPedExtension::SetHeadMicroMorphData(CEntity* pPed, u32 type, float blend)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetHeadMicroMorphData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_ReplayMultiplayerHeadData.m_microMorphBlends[type] = blend;
	extension->SetMultiplayerHeadDataFlags(REPLAY_HEAD_MICROMORPH);
}

void ReplayPedExtension::SetHeadOverlayTintData(CEntity* pPed, eHeadOverlaySlots slot, eRampType rampType, u8 tint, u8 tint2)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetHeadOverlayTintData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_ReplayHeadOverlayTintData.m_overlayTintIndex[slot] = tint;
	extension->m_ReplayHeadOverlayTintData.m_overlayTintIndex2[slot] = tint2;
	extension->m_ReplayHeadOverlayTintData.m_overlayRampType[slot] = (u8)rampType;

	extension->SetMultiplayerHeadDataFlags(REPLAY_HEAD_OVERLAYTINTDATA);
}

void ReplayPedExtension::SetHairTintIndex(CEntity* pPed, u8 index, u8 index2)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetHeadOverlayTintData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_ReplayHairTintData.m_HairTintIndex = index;
	extension->m_ReplayHairTintData.m_HairTintIndex2 = index2;

	extension->SetMultiplayerHeadDataFlags(REPLAY_HEAD_HAIRTINTDATA);
}

void ReplayPedExtension::SetEyeColor(CEntity* pPed, s32 colorIndex)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetEyeColor when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_EyeColorIndex = colorIndex;
}

void ReplayPedExtension::SetPedDecorationData(CEntity* pPed, u32 collectionName, u32 presetName, int crewEmblemVariation, bool compressed, float alpha)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetPedDecorationData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	//Check to see if the decoration is already added
	for( u32 i = 0; i < extension->m_PedDecorationData.size(); i++ )
	{
		if( extension->m_PedDecorationData[i].collectionName == collectionName && extension->m_PedDecorationData[i].presetName == presetName && extension->m_PedDecorationData[i].crewEmblemVariation == crewEmblemVariation && extension->m_PedDecorationData[i].alpha == alpha)
			return;
	}

	if( extension->m_PedDecorationData.size() >= MAX_REPLAY_PED_DECORATION_DATA )
	{
		AssertMsg(false, "ReplayPedExtension::m_PedDecorationData run out of space");
		return;
	}

	sReplayPedDecorationData &decorationData = extension->m_PedDecorationData.Append();
	decorationData.collectionName = collectionName;
	decorationData.presetName = presetName;
	decorationData.crewEmblemVariation = crewEmblemVariation;		
	decorationData.compressed = compressed;
	decorationData.alpha = alpha;
}

void ReplayPedExtension::ClonePedDecorationData(const CPed* source, CPed* target)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(source);
	replayAssertf(extension != NULL, "Error: trying to set SetPedDecorationData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	ReplayPedExtension *targetExtension = ReplayPedExtension::GetExtension(target);
	replayAssertf(targetExtension != NULL, "Error: trying to set SetPedDecorationData when it doesn't have the extension for it\n");
	if( !targetExtension )
				return;

	targetExtension->m_PedDecorationData.clear();

	for(int i = 0; i < extension->m_PedDecorationData.size(); i++)
	{
		sReplayPedDecorationData const& decorationData = extension->m_PedDecorationData[i];
		SetPedDecorationData(target, decorationData.collectionName, decorationData.presetName, decorationData.crewEmblemVariation, decorationData.compressed, decorationData.alpha);
	}
}

void ReplayPedExtension::SetHeadBlendDataPaletteColor(CEntity* pPed, u8 r, u8 g, u8 b, u8 colorIndex, bool bforce)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set SetPedDecorationData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_ReplayHeadBlendPaletteData.m_paletteRed[colorIndex] = r;
	extension->m_ReplayHeadBlendPaletteData.m_paletteGreen[colorIndex] = g;
	extension->m_ReplayHeadBlendPaletteData.m_paletteBlue[colorIndex] = b;
	extension->m_ReplayHeadBlendPaletteData.m_forcePalette[colorIndex] = bforce;
	extension->m_ReplayHeadBlendPaletteData.m_paletteSet[colorIndex] = true;
}

void ReplayPedExtension::GetHeadBlendPaletteColor(u8 colorIndex, u8 &r, u8 &g, u8 &b, bool &force, bool &hasBeenSet)
{
	Assert(colorIndex < MAX_PALETTE_COLORS);

	r = m_ReplayHeadBlendPaletteData.m_paletteRed[colorIndex];
	g = m_ReplayHeadBlendPaletteData.m_paletteGreen[colorIndex];
	b = m_ReplayHeadBlendPaletteData.m_paletteBlue[colorIndex];
	force = m_ReplayHeadBlendPaletteData.m_forcePalette[colorIndex];
	hasBeenSet = m_ReplayHeadBlendPaletteData.m_paletteSet[colorIndex];
}

void ReplayPedExtension::ClearPedDecorationData(CEntity* pPed)
{
	ReplayPedExtension *extension = ReplayPedExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set ClearPedDecorationData when it doesn't have the extension for it\n");
	if( !extension )
		return;

	extension->m_PedDecorationData.Reset();
}

//Some head blend data can only be applied when we have a valid head blend handle
void ReplayPedExtension::ApplyHeadBlendDataPostBlendHandleCreate(CPed* pPed)
{
	for( u8 i = 0; i < MAX_PALETTE_COLORS; i++ )
	{
		if( m_ReplayHeadBlendPaletteData.m_paletteSet[i] )
		{
			pPed->SetHeadBlendPaletteColor(m_ReplayHeadBlendPaletteData.m_paletteRed[i], m_ReplayHeadBlendPaletteData.m_paletteGreen[i], m_ReplayHeadBlendPaletteData.m_paletteBlue[i], i, m_ReplayHeadBlendPaletteData.m_forcePalette[i]);
		}
	}	

	if( m_MultiplayerHeadFlags & REPLAY_HEAD_OVERLAYTINTDATA )
	{
		for( u32 i = 0; i < HOS_MAX; i++ )
		{				
			pPed->SetHeadOverlayTintIndex((eHeadOverlaySlots)i, (eRampType)m_ReplayHeadOverlayTintData.m_overlayRampType[i], m_ReplayHeadOverlayTintData.m_overlayTintIndex[i], m_ReplayHeadOverlayTintData.m_overlayTintIndex2[i]);
		}
	}

	if( m_MultiplayerHeadFlags & REPLAY_HEAD_HAIRTINTDATA )
	{									
		pPed->SetHairTintIndex(m_ReplayHairTintData.m_HairTintIndex, m_ReplayHairTintData.m_HairTintIndex2);
	}	

	if( m_EyeColorIndex != -1 )
	{
		pPed->SetHeadBlendEyeColor(m_EyeColorIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
bool ReplayBicycleExtension::GetIsPedalling(const CEntity* pVehicle)
{
	const ReplayBicycleExtension *extension = ReplayBicycleExtension::GetExtension(pVehicle);
	if(extension)
	{
		return extension->m_bIsPedalling;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayBicycleExtension::SetIsPedalling(CEntity* pVehicle, bool val)
{
	ReplayBicycleExtension *extension = ReplayBicycleExtension::GetExtension(pVehicle);
	replayAssertf(extension != NULL, "Error: trying to set a m_bIsPedalling when it doesn't have the extension for it\n");
	extension->m_bIsPedalling = val;
}

//////////////////////////////////////////////////////////////////////////
f32 ReplayBicycleExtension::GetPedallingRate(const CEntity* pVehicle)
{
	const ReplayBicycleExtension *extension = ReplayBicycleExtension::GetExtension(pVehicle);
	if(extension)
	{
		return extension->m_fPedallingRate;
	}

	return 0.f;
}

//////////////////////////////////////////////////////////////////////////
void ReplayBicycleExtension::SetPedallingRate(CEntity* pVehicle, f32 rate)
{
	ReplayBicycleExtension *extension = ReplayBicycleExtension::GetExtension(pVehicle);
	replayAssertf(extension != NULL, "Error: trying to set a m_fPedallingRate when it doesn't have the extension for it\n");
	extension->m_fPedallingRate = rate;
}


//////////////////////////////////////////////////////////////////////////
bgGlassHandle ReplayGlassExtension::GetGlassHandle(const CEntity* pEntity)
{
	const ReplayGlassExtension *extension = ReplayGlassExtension::GetExtension(pEntity);
	if(extension)
	{
		return extension->m_glassHandle;
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
void ReplayGlassExtension::SetGlassHandle(CEntity* pEntity, bgGlassHandle handle)
{
	ReplayGlassExtension *extension = ReplayGlassExtension::GetExtension(pEntity);
	replayAssertf(extension != NULL, "Error: trying to set a glass handle when it doesn't have the extension for it\n");
	extension->m_glassHandle = handle;
}

//////////////////////////////////////////////////////////////////////////
bool ReplayParachuteExtension::GetIsParachuting(const CEntity* pPed)
{
	const ReplayParachuteExtension *extension = ReplayParachuteExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_bIsParachuting;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayParachuteExtension::SetIsParachuting(CEntity* pPed, bool val)
{
	ReplayParachuteExtension *extension = ReplayParachuteExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_bIsParachuting when it doesn't have the extension for it\n");
	extension->m_bIsParachuting = val;
}

//////////////////////////////////////////////////////////////////////////
u8 ReplayParachuteExtension::GetParachutingState(const CEntity* pPed)
{
	const ReplayParachuteExtension *extension = ReplayParachuteExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_iState;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayParachuteExtension::SetParachutingState(CEntity* pPed, u8 val)
{
	ReplayParachuteExtension *extension = ReplayParachuteExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_iState = val;
}


//////////////////////////////////////////////////////////////////////////
u32 ReplayReticuleExtension::GetWeaponHash(const CEntity* pPed)
{
	const ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_ReticulePacketInfo.m_iWeaponHash;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayReticuleExtension::SetWeaponHash(CEntity* pPed, u32 val)
{
	ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_ReticulePacketInfo.m_iWeaponHash = val;
}

//////////////////////////////////////////////////////////////////////////
bool ReplayReticuleExtension::GetReadyToFire(const CEntity* pPed)
{
	const ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_ReticulePacketInfo.m_bReadyToFire;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayReticuleExtension::SetReadyToFire(CEntity* pPed, bool val)
{
	ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_ReticulePacketInfo.m_bReadyToFire = val;
}

//////////////////////////////////////////////////////////////////////////
bool ReplayReticuleExtension::GetHit(const CEntity* pPed)
{
	const ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_ReticulePacketInfo.m_bHit;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayReticuleExtension::SetHit(CEntity* pPed, bool val)
{
	ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_ReticulePacketInfo.m_bHit = val;
}

//////////////////////////////////////////////////////////////////////////
bool ReplayReticuleExtension::GetIsFirstPerson(const CEntity* pPed)
{
	const ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_ReticulePacketInfo.m_bIsFirstPerson;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayReticuleExtension::SetIsFirstPerson(CEntity* pPed, bool val)
{
	ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_ReticulePacketInfo.m_bIsFirstPerson = val;
}

//////////////////////////////////////////////////////////////////////////
u8 ReplayReticuleExtension::GetZoomLevel(const CEntity* pPed)
{
	const ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	if(extension)
	{
		return static_cast<s32>(extension->m_ReticulePacketInfo.m_iZoomLevel);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayReticuleExtension::SetZoomLevel(CEntity* pPed, u8 val)
{
	ReplayReticuleExtension *extension = ReplayReticuleExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	if (val < 0)
		val = 0;
	// just goes up to 100 so store as u8
	extension->m_ReticulePacketInfo.m_iZoomLevel = static_cast<u8>(val);
}

//////////////////////////////////////////////////////////////////////////
bool ReplayHUDOverlayExtension::GetTelescope(const CEntity* pPed)
{
	const ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_hudOverlayPacketInfo.m_overlay.m_telescopeOn;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayHUDOverlayExtension::SetTelescope(CEntity* pPed, bool val)
{
	ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_hudOverlayPacketInfo.m_overlay.m_telescopeOn = val;
}

//////////////////////////////////////////////////////////////////////////
bool ReplayHUDOverlayExtension::GetBinoculars(const CEntity* pPed)
{
	const ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_hudOverlayPacketInfo.m_overlay.m_binocularsOn;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayHUDOverlayExtension::SetBinoculars(CEntity* pPed, bool val)
{
	ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_hudOverlayPacketInfo.m_overlay.m_binocularsOn = val;
}

//////////////////////////////////////////////////////////////////////////
u8 ReplayHUDOverlayExtension::GetCellphone(const CEntity* pPed)
{
	const ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	if(extension)
	{
		if (extension->m_hudOverlayPacketInfo.m_overlay.m_iFruitOn)
			return 1;
		else if (extension->m_hudOverlayPacketInfo.m_overlay.m_badgerOn)
			return 2;
		else if (extension->m_hudOverlayPacketInfo.m_overlay.m_facadeOn)
			return 3;

		return 0;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayHUDOverlayExtension::SetCellphone(CEntity* pPed, u8 val)
{
	ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");

	extension->m_hudOverlayPacketInfo.m_overlay.m_iFruitOn = false;
	extension->m_hudOverlayPacketInfo.m_overlay.m_badgerOn = false;
	extension->m_hudOverlayPacketInfo.m_overlay.m_facadeOn = false;

	switch (val)
	{
	case 1:
		{
			extension->m_hudOverlayPacketInfo.m_overlay.m_iFruitOn = true;
		}
		break;
	case 2:
		{
			extension->m_hudOverlayPacketInfo.m_overlay.m_badgerOn = true;
		}
		break;
	case 3:
		{
			extension->m_hudOverlayPacketInfo.m_overlay.m_facadeOn = true;
		}
		break;
	default: break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool ReplayHUDOverlayExtension::GetTurretCam(const CEntity* pPed)
{
	const ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_hudOverlayPacketInfo.m_overlay.m_turretCamOn;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayHUDOverlayExtension::SetTurretCam(CEntity* pPed, bool val)
{
	ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_hudOverlayPacketInfo.m_overlay.m_turretCamOn = val;
}


//////////////////////////////////////////////////////////////////////////
bool ReplayHUDOverlayExtension::GetDroneCam(const CEntity* pPed)
{
	const ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_hudOverlayPacketInfo.m_overlay.m_droneCamOn;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void ReplayHUDOverlayExtension::SetDroneCam(CEntity* pPed, bool val)
{
	ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_hudOverlayPacketInfo.m_overlay.m_droneCamOn = val;
}

//////////////////////////////////////////////////////////////////////////
u8 ReplayHUDOverlayExtension::GetFlagsValue(const CEntity* pPed)
{
	const ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	if(extension)
	{
		return extension->m_hudOverlayPacketInfo.m_allOverlays;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void ReplayHUDOverlayExtension::SetFlagsValue(CEntity* pPed, u8 val)
{
	ReplayHUDOverlayExtension *extension = ReplayHUDOverlayExtension::GetExtension(pPed);
	replayAssertf(extension != NULL, "Error: trying to set a m_iState when it doesn't have the extension for it\n");
	extension->m_hudOverlayPacketInfo.m_allOverlays = val;
}

//////////////////////////////////////////////////////////////////////////
void ReplayVehicleExtension::Reset()
{
	ReplayVehicleWithWheelsExtension<ReplayVehicleExtension>::Reset();

	RemoveAllDeformations();

	m_incrementScorch = true;
	m_ForceHD = false;

	Matrix34 mat;
	mat.Zero();
	m_dialBoneMtx.StoreMatrix(mat);
}

void ReplayVehicleExtension::ResetOnRewind()
{
	for( u32 i = 0; i < m_Impacts.size(); i++ )
	{
		m_Impacts[i].hasBeenApplied = false;
	}
	m_ImpactsAppliedOffset = 0;
}

void ReplayVehicleExtension::RemoveDeformation()
{
	if( m_Impacts.size() <= 0 )
		return;

	m_Impacts.Delete(m_Impacts.size()-1);

	ResetOnRewind();
}

void ReplayVehicleExtension::RemoveAllDeformations()
{
	m_Impacts.Reset();
	m_ImpactsAppliedOffset = 0;
}

void ReplayVehicleExtension::StoreVehicleDeformationData(const Vector4& damage, const Vector3& offset)
{
	if( m_Impacts.size() >= MAX_REPLAY_VEHICLE_IMPACTS )
	{
		Warningf("ReplayVehicleExtension::StoreVehicleDeformationData run out of space");
		return;
	}

	vehicleImpactData &impactData = m_Impacts.Append();

	impactData.damage = damage;
	impactData.offset = offset;
	impactData.hasBeenApplied = false;
}

void ReplayVehicleExtension::SetVehicleAsDamaged(CVehicle* pVehicle)
{
	if( !CReplayMgr::ShouldRegisterElement(pVehicle) )
		return;

	//When we apply new damage make sure the fixed flag in the extension is set to false
	ReplayVehicleExtension *extension = ReplayVehicleExtension::GetExtension(pVehicle);
	if(!extension)
	{
		extension = ReplayVehicleExtension::Add(pVehicle);
		if(!extension)
		{
			replayAssertf(false, "Failed to add a vehicle extension...ran out?");
			return;
		}
	}
	extension->SetFixed(false);
}

bool ReplayVehicleExtension::IsVehicleUndamaged(const CVehicle* pVehicle)
{
	//check to see if the damage has been repaired
	if(pVehicle)
	{
		ReplayVehicleExtension* extension = ReplayVehicleExtension::GetExtension(pVehicle);

		if(extension)
		{
			return extension->HasBeenFixed();
		}
	}

	return true;
}

void ReplayVehicleExtension::SetForceHD(bool force)
{
	m_ForceHD = force;
}

bool ReplayVehicleExtension::GetForceHD()
{
	return m_ForceHD;
}

void ReplayVehicleExtension::SetVehicleDialBoneMtx(const Matrix34& mtx)
{
	m_dialBoneMtx.StoreMatrix(mtx);
}

void ReplayVehicleExtension::GetVehicleDialBoneMtx(Matrix34& mtx)
{
	m_dialBoneMtx.LoadMatrix(mtx);
}

CPacketPositionAndQuaternion20& ReplayVehicleExtension::GetVehicleDialBoneMtx()
{
	return m_dialBoneMtx;
}

bool ReplayVehicleExtension::HasFinishedApplyingDamage()
{
	u32 impactSize = m_Impacts.size();
	if(impactSize == 0 || m_ImpactsAppliedOffset >= impactSize)
		return true;

	return false;
}

void ReplayVehicleExtension::ApplyVehicleDeformation(CVehicle* pVehicle)
{
	u32 i = 0;

	u32 impactSize = m_Impacts.size();
	if( impactSize == 0 || m_ImpactsAppliedOffset >= impactSize || pVehicle->GetNumAppliedVehicleDamage() >=  MAX_IMPACTS_PER_VEHICLE_PER_FRAME)
		return;


	CVehicleDeformation* pVehDeformation = pVehicle->GetVehicleDamage()->GetDeformation();

	if(!pVehDeformation->HasDamageTexture())
	{
		pVehDeformation->DamageTextureAllocate(true);
		//dont allow any other impacts to be added this frame until the damage texture has been reset
		pVehicle->SetNumAppliedVehicleDamage(MAX_IMPACTS_PER_VEHICLE_PER_FRAME);
		return;
	}

	for( i = m_ImpactsAppliedOffset; i < m_Impacts.size(); i++ )
	{
		if( m_Impacts[i].hasBeenApplied == false )
		{
			m_Impacts[i].hasBeenApplied = true;

			pVehDeformation->SetNewImpactToStore(m_Impacts[i].damage, Vector4(m_Impacts[i].offset));			

			pVehicle->IncreaseNumAppliedVehicleDamage();
			//Only allowed so many impacts per frame so stop when we hit that limit and continue the next frame
			if( pVehicle->GetNumAppliedVehicleDamage() >=  MAX_IMPACTS_PER_VEHICLE_PER_FRAME)
				break;
		}
	}

	pVehDeformation->ApplyDeformations();

	m_ImpactsAppliedOffset = i+1;
}


#endif //GTA_REPLAY
