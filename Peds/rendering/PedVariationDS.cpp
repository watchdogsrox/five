#include "Peds/rendering/PedVariationDS.h"

#include "Peds/rendering/PedVariationStream.h"
#include "Peds/rendering/pedvariations_parser.h"

#include "diag/art_channel.h"
#include "fwscene/stores/drawablestore.h"
#include "rmcore/drawable.h"
#include "parser/manager.h"
#include "system/typeinfo.h"


// game headers
#include "audio/pedaudioentity.h"
#include "shaders/CustomShaderEffectPed.h"
#include "scene/DynamicEntity.h"
#include "scene/EntityIterator.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "streaming/populationstreaming.h"
#include "streaming/ScriptPreload.h"
#include "streaming/streaming.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/Ped.h"
#include "Peds/Rendering/PedVariationPack.h"
#include "Physics/physics.h"
#include "control/replay/ReplayExtensions.h"
#include "control/replay/ped/PedPacket.h"

PARAM(noheadblend, "Disables ped head blend system");

AUTOID_IMPL(CPedHeadBlendData);

const char* varSlotNames[PV_MAX_COMP] = {"head","berd","hair","uppr","lowr","hand","feet","teef","accs","task","decl","jbib"};

ePedVarComp varSlotEnums[PV_MAX_COMP] = {	PV_COMP_HEAD, PV_COMP_BERD, PV_COMP_HAIR ,PV_COMP_UPPR, PV_COMP_LOWR, 
											PV_COMP_HAND, PV_COMP_FEET, PV_COMP_TEEF, PV_COMP_ACCS, 
											PV_COMP_TASK, PV_COMP_DECL, PV_COMP_JBIB};

const char* raceTypeNames[PV_RACE_MAX_TYPES] = {"uni","whi","bla","chi","lat","ara","bal","jam","kor","ita","pak"};

const char* propSlotNames[NUM_ANCHORS] = {"p_head","p_eyes","p_ears","p_mouth","p_lhand","p_rhand","p_lwrist","p_rwrist","p_lhip","p_lfoot","p_rfoot","ph_lhand","ph_rhand"};
const char* propSlotNamesClean[NUM_ANCHORS] = {"head","eyes","ears","mouth","lhand","rhand","lwrist","rwrist","lhip","lfoot","rfoot","ph_lhand","ph_rhand"};

const char* pedCompFlagNames[NUM_PED_COMP_FLAGS] = {"bulky", "job", "sunny", "wet", "cold", "not in car", "bike only", "not indoors", "fire retardant", "armored",
							  "light armored", "HD", "default helm", "rand helm", "script helm", "flight helm", "hide in first person", "use physics hat 2", "pilot helm", "wet: more wet", "wet: less wet"};

eAnchorPoints propSlotEnums[NUM_ANCHORS] = { ANCHOR_HEAD, ANCHOR_EYES, ANCHOR_EARS, ANCHOR_MOUTH, ANCHOR_LEFT_HAND,
											 ANCHOR_RIGHT_HAND, ANCHOR_LEFT_WRIST, ANCHOR_RIGHT_WRIST, ANCHOR_HIP,
											 ANCHOR_LEFT_FOOT, ANCHOR_RIGHT_FOOT, ANCHOR_PH_L_HAND, ANCHOR_PH_R_HAND };

CPVDrawblData::CPVDrawblData(void)
{
	m_propMask = 0;
	m_numAlternatives = 0;

#ifndef WIP_PEDVAR_XML
	m_aTexData.Resize(MAX_TEX_VARS);
	for(u32 k=0;k<MAX_TEX_VARS;k++){
		m_aTexData[k].texId = (u8)PV_RACE_INVALID;
	}
#endif

	m_clothData.Reset();
}

CPVDrawblData::~CPVDrawblData(void)
{
	if (m_clothData.HasCloth() && m_clothData.m_dictSlotId != -1)
		g_ClothStore.RemoveRef(strLocalIndex(m_clothData.m_dictSlotId), REF_OTHER);

	m_clothData.Reset();
}

void CPVDrawblData::IncrementUseCount(u32 index)
{
	if (index < m_aTexData.GetCount())
		m_aTexData[index].useCount++;
}

void CPVDrawblData::SetPropertyFlag(bool val, ePVDrawblProperty prop)
{
	if (val)
	{
		m_propMask |= prop;
	}
	else
	{
		m_propMask &= ~prop;
	}
}

CPVDrawblData* CPVComponentData::ObtainDrawblMapEntry(u32 i)
{
 	if (!Verifyf(i < MAX_PLAYER_DRAWBL_VARS, "Too large drawable index (%d) passed to ObtainDrawblMapEntry!", i))
		return NULL;

	if (i >= m_aDrawblData3.GetCount())
		m_aDrawblData3.ResizeGrow(i + 1);

	if (i < m_aDrawblData3.GetCount())
		return &m_aDrawblData3[i];

	Assert(false);
 	return NULL;
}

//---------------CPedVariationData----------------
CPedVariationData::CPedVariationData(void)
{
	u32	i;

	for(i=0;i<PV_MAX_COMP;i++) 
	{
 		m_clothVarData[i] = NULL;
#if CREATE_CLOTH_ONLY_IF_RENDERED
		m_aCreateCloth[i] = false;
#endif
		m_aTexId[i] = 0;
		m_aDrawblId[i] = 0;
		m_aDrawblAltId[i] = 0;
		m_aPaletteId[i] = 0;

		m_aDrawblHash[i] = 0;
	}

	m_LodFadeValue = 255;

	m_bIsCurrVarAlpha	= false;
	m_bIsCurrVarDecal	= false;
	m_bIsCurrVarDecalDecoration = false;
	m_bIsCurrVarCutout	= false;

	m_bIsShaderSetup = false;

	scaleFactor = 0.0f;

	m_bIsPropHighLOD = true;
#if CREATE_CLOTH_ONLY_IF_RENDERED
	m_pedSkeleton = NULL;
#endif

	m_preloadData = NULL;
	m_componentHiLod.Reset();
    m_bVariationSet = false;

	m_overrideCrewLogoTxd.Clear();
	m_overrideCrewLogoTex.Clear();
}

CPedVariationData::~CPedVariationData(void)
{
	delete m_preloadData;
}



void CPedVariationData::RemoveCloth(ePedVarComp slotId, bool restoreValue)
{
	clothVariationData* pClothData = m_clothVarData[slotId];
	if( pClothData )
	{		
		Assert( pClothData->GetCloth() );
		pgRef<characterCloth>& pCCloth = pClothData->GetCloth();
		Assert( pCCloth );

		characterClothController* cccontroler = pCCloth->GetClothController();
		Assert( cccontroler );

		clothManager* cManager = CPhysics::GetClothManager();
		Assert( cManager );
		cManager->RemoveCharacterCloth( cccontroler );

		gDrawListMgr->CharClothRemove( pClothData );

		m_clothVarData[slotId] = NULL;

#if CREATE_CLOTH_ONLY_IF_RENDERED	
		m_aCreateCloth[slotId] = restoreValue;
#else
		(void)restoreValue;
#endif
	}
#if CREATE_CLOTH_ONLY_IF_RENDERED
	else if( !restoreValue )
		m_aCreateCloth[slotId] = false;		// NOTE: m_aCreateCloth[slotId] can be up to here true or false(based on if cloth has been rendered already or not), so no need to assert
#endif
}

bool CPedVariationData::HasRacialTexture(CPed* ped, ePedVarComp slotId)
{
	if (!ped || !ped->GetPedModelInfo() || !ped->GetPedModelInfo()->GetVarInfo())
		return false;

	return ped->GetPedModelInfo()->GetVarInfo()->HasRaceTextures(slotId, m_aDrawblId[slotId]);
}

void CPedVariationData::UpdateAllHeadBlends(CPed* ped)
{
	if (!HasHeadBlendData(ped))
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (MESHBLENDMANAGER.IsHandleValid(blendData->m_blendHandle) && ped && ped->GetPedDrawHandler().GetPedRenderGfx())
	{
        u32 cloneCount = MESHBLENDMANAGER.GetPedCloneCount(blendData->m_blendHandle);
 		pedDebugf1("[HEAD BLEND] CPedVariationData::UpdateAllHeadBlends: Updating head blends for %u clones for ped %p, handle 0x%08x",
			cloneCount, ped, blendData->m_blendHandle);
         for (u32 i = 0; i < cloneCount; ++i)
		{
			CPed* clone = MESHBLENDMANAGER.GetPedClone(blendData->m_blendHandle, i);
            if (clone)
                UpdateHeadBlend(clone);
		}
	}
}

void CPedVariationData::UpdateHeadBlend(CPed* ped)
{
	if (!HasHeadBlendData(ped))
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (MESHBLENDMANAGER.IsHandleValid(blendData->m_blendHandle) && ped && ped->GetPedDrawHandler().GetPedRenderGfx())
	{
		pedDebugf1("[HEAD BLEND] CPedVariationData::UpdateHeadBlend: Replacing assets for ped %p, handle 0x%08x", ped, blendData->m_blendHandle);
		s32 drawable = MESHBLENDMANAGER.GetBlendDrawable(blendData->m_blendHandle, true, false);
		s32 texture = MESHBLENDMANAGER.GetBlendTexture(blendData->m_blendHandle, true, false);
		s32 feetDrawable = -1;
		s32 feetTexture = MESHBLENDMANAGER.GetBlendSkinTexture(blendData->m_blendHandle, SBC_FEET, true, false, feetDrawable);
		s32 upprDrawable = -1;
		s32 upprTexture = MESHBLENDMANAGER.GetBlendSkinTexture(blendData->m_blendHandle, SBC_UPPR, true, false, upprDrawable);
		s32 lowrDrawable = -1;
		s32 lowrTexture = MESHBLENDMANAGER.GetBlendSkinTexture(blendData->m_blendHandle, SBC_LOWR, true, false, lowrDrawable);

		bool newAssets = false;
		bool removedOldStreamData = false;

#define UPDATE_PED_COST() \
	if (!removedOldStreamData) \
	{ \
		gPopStreaming.RemoveStreamedPedVariation(ped->GetPedDrawHandler().GetPedRenderGfx()); \
		removedOldStreamData = true; \
	}

        // HEAD
		if (texture != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_txdIdx[PV_COMP_HEAD] != texture)
		{
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceTexture(PV_COMP_HEAD, texture);
			newAssets = true;
		}
        if (drawable != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_dwdIdx[PV_COMP_HEAD] != drawable)
        {
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceDrawable(PV_COMP_HEAD, drawable);
			newAssets = true;
        }


		// FEET
		if (feetTexture != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_txdIdx[PV_COMP_FEET] != feetTexture)
		{
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceTexture(PV_COMP_FEET, feetTexture);
			newAssets = true;
		}
        if (feetDrawable != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_dwdIdx[PV_COMP_FEET] != feetDrawable)
        {
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceDrawable(PV_COMP_FEET, feetDrawable);
			newAssets = true;
        }

		// UPPR
		if (upprTexture != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_txdIdx[PV_COMP_UPPR] != upprTexture)
		{
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceTexture(PV_COMP_UPPR, upprTexture);
			newAssets = true;
		}
        if (upprDrawable != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_dwdIdx[PV_COMP_UPPR] != upprDrawable)
        {
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceDrawable(PV_COMP_UPPR, upprDrawable);
			newAssets = true;
        }

		// LOWR
		if (lowrTexture != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_txdIdx[PV_COMP_LOWR] != lowrTexture)
		{
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceTexture(PV_COMP_LOWR, lowrTexture);
			newAssets = true;
		}
        if (lowrDrawable != -1 && ped->GetPedDrawHandler().GetPedRenderGfx()->m_dwdIdx[PV_COMP_LOWR] != lowrDrawable)
        {
			UPDATE_PED_COST();
			ped->GetPedDrawHandler().GetPedRenderGfx()->ReplaceDrawable(PV_COMP_LOWR, lowrDrawable);
			newAssets = true;
        }

		if (newAssets)
			ped->GetPedDrawHandler().GetPedRenderGfx()->ApplyNewAssets(ped);

		if (removedOldStreamData)
			gPopStreaming.AddStreamedPedVariation(ped->GetPedDrawHandler().GetPedRenderGfx());

#undef UPDATE_PED_COST
	}
}

bool CPedVariationData::AssetLoadFinished(CPed* ped, CPedStreamRequestGfx* pStreamReq)
{
	if (!ped)
		return false;

	Assert(pStreamReq);

	CPedDrawHandler *drawHandler = &ped->GetPedDrawHandler();
	CPedVariationData *varData = &drawHandler->GetVarData();

	// check if this ped has a decal decoration
	strLocalIndex dwdIdx = pStreamReq->GetDwdSlot(PV_COMP_DECL);
	if (dwdIdx.IsValid())
	{
		rmcDrawable* pDrawable = g_DwdStore.GetDrawable(dwdIdx, 0);
		if (pDrawable && pDrawable->GetShaderGroup().LookupShader("ped_decal_decoration"))
			varData->m_bIsCurrVarDecalDecoration = true;
	}

	if (!HasHeadBlendData(ped))
	{
		pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - No head blend data", ped);
		return false;
	}

	// request new blend entry
	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();


	s32 head0 = pStreamReq->GetHeadBlendSlot(HBS_HEAD_0).Get();
	s32 head1 = pStreamReq->GetHeadBlendSlot(HBS_HEAD_1).Get();
	s32 head2 = pStreamReq->GetHeadBlendSlot(HBS_HEAD_2).Get();

	s32 tex0 = pStreamReq->GetHeadBlendSlot(HBS_TEX_0).Get();
	s32 tex1 = pStreamReq->GetHeadBlendSlot(HBS_TEX_1).Get();
	s32 tex2 = pStreamReq->GetHeadBlendSlot(HBS_TEX_2).Get();

	if (head0 == -1 || head1 == -1 || head2 == -1 || tex0 == -1 || tex1 == -1 || tex2 == -1)
	{
		pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - Missing stuff: %d, %d, %d, %d, %d, %d", ped, head0, head1, head2,
			tex0, tex1, tex2);
		return false;
	}

	s32 parentHead0 = pStreamReq->GetHeadBlendSlot(HBS_PARENT_HEAD_0).Get();
	s32 parentHead1 = pStreamReq->GetHeadBlendSlot(HBS_PARENT_HEAD_1).Get();

	if (blendData->HasParents())
	{
		if (parentHead0 == -1 || parentHead1 == -1)
		{
			pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - Missing parent: %d, %d", ped, parentHead0, parentHead1);
			return false;
		}
	}

	if (blendData->m_blendHandle == MESHBLEND_HANDLE_INVALID)
	{
		blendData->m_blendHandle = MESHBLENDMANAGER.RequestNewPedHeadBlend(ped, head0, tex0, head1, tex1, head2, tex2, blendData->m_headBlend, blendData->m_texBlend, blendData->m_varBlend, blendData->m_async, ped->IsMale(), blendData->m_isParent);
		pedDebugf1("[HEAD BLEND] Created blend handle 0x%08x for ped %p", blendData->m_blendHandle, ped);

		// we just received a valid palette index, make sure to set the palette texture and selector on the shader effect
		CCustomShaderEffectPed* pShaderEffect = smart_cast<CCustomShaderEffectPed*>(drawHandler->GetShaderEffect());
		if (pShaderEffect)
		{
			s32 palSelect = -1;
			grcTexture* palTex = MESHBLENDMANAGER.GetCrewPaletteTexture(blendData->m_blendHandle, palSelect);

			s32 palSelectHair = -1;
			s32 palSelectHair2 = -1;
			grcTexture* palTexHair = MESHBLENDMANAGER.GetTintTexture(blendData->m_blendHandle, RT_HAIR, palSelectHair, palSelectHair2);
			if (palTex)
			{
				for (u32 i = 0; i < PV_MAX_COMP; ++i)
				{
					if (i == PV_COMP_HAND) // in MP the parachute is on the hand component
						continue;

					if (i == PV_COMP_HAIR)
					{
						pShaderEffect->SetDiffuseTexturePal(palTexHair, i);
						pShaderEffect->SetDiffuseTexturePalSelector((u8)palSelectHair, i, true);
						pShaderEffect->SetSecondHairPalSelector((u8)palSelectHair2);
					}
					else
					{
						pShaderEffect->SetDiffuseTexturePal(palTex, i);
						pShaderEffect->SetDiffuseTexturePalSelector((u8)palSelect, i, true);
					}
				}
			}
		}
	}
	else
	{
		pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - Updating blend handle 0x%08x with data: 0={%d, %d}, 1={%d, %d}, 2={%d, %d}, blend={%f, %f, %f}", ped, blendData->m_blendHandle,
			head0, tex0, head1, tex1, head2, tex2, blendData->m_headBlend, blendData->m_texBlend, blendData->m_varBlend);
		MESHBLENDMANAGER.UpdatePedHeadBlend(blendData->m_blendHandle, head0, tex0, head1, tex1, head2, tex2, blendData->m_headBlend, blendData->m_texBlend, blendData->m_varBlend);
	}

	if (blendData->HasParents())
	{
		pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - Updating parent blend data 0x%08x with head0=%d, head1=%d, blend=%f, blend2=%f", ped, blendData->m_blendHandle,
			parentHead0, parentHead1, blendData->m_extraParentData.parentBlend, blendData->m_parentBlend2);
		MESHBLENDMANAGER.SetParentBlendData(blendData->m_blendHandle, parentHead0, parentHead1, blendData->m_extraParentData.parentBlend, blendData->m_parentBlend2);
	}

	if (Verifyf(MESHBLENDMANAGER.IsHandleValid(blendData->m_blendHandle), "Couldn't create new head blend slot! Too many heads created at once!"))
	{
		// set overlays
		for (s32 i = 0; i < HOS_MAX; ++i)
		{
			strLocalIndex overlayTex = pStreamReq->GetHeadBlendSlot(HBS_OVERLAY_TEX_BLEMISHES + i);
			if (overlayTex == -1)
			{
				MESHBLENDMANAGER.ResetOverlay(blendData->m_blendHandle, i);
				continue;
			}

			eOverlayBlendType blendType = OBT_ALPHA_BLEND;
			if (i == HOS_AGING || i == HOS_BLEMISHES || i == HOS_DAMAGE || i == HOS_BASE_DETAIL || i == HOS_SKIN_DETAIL_2 || i == HOS_BODY_2 || i == HOS_BODY_3)
				blendType = OBT_OVERLAY;

			MESHBLENDMANAGER.RequestNewOverlay(blendData->m_blendHandle, i, overlayTex, blendType, blendData->m_overlayAlpha[i], blendData->m_overlayNormAlpha[i]);
			MESHBLENDMANAGER.SetOverlayTintIndex(blendData->m_blendHandle, i, (eRampType)blendData->m_overlayRampType[i], blendData->m_overlayTintIndex[i], blendData->m_overlayTintIndex2[i]);
		}

		// set spec and normal textures
		const grcTexture* spec0 = NULL;
		const grcTexture* norm0 = NULL;
		const grcTexture* spec1 = NULL;
		const grcTexture* norm1 = NULL;

		s32 slotHead0 = HBS_HEAD_0;
		s32 slotHead1 = HBS_HEAD_1;
		if (blendData->HasParents())
		{
#if 1
            // if we have parents we choose the norm/spec maps from either the grandma or grandpa, based on child gender
            if (!ped->IsMale())
			{
				slotHead0 = HBS_PARENT_HEAD_0;
				slotHead1 = HBS_PARENT_HEAD_1;
			}
#else
			// if we have parents we choose the norm/spec maps from the each dominant grandparent
			if (blendData->m_extraParentData.parentBlend <= 0.5f)
				slotHead0 = HBS_PARENT_HEAD_0;
			if (blendData->m_parentBlend2 <= 0.5f)
				slotHead1 = HBS_PARENT_HEAD_1;
#endif
		}
		strLocalIndex indexHead0 = pStreamReq->GetHeadBlendSlot(slotHead0);
		strLocalIndex indexHead1 = pStreamReq->GetHeadBlendSlot(slotHead1);
		const rmcDrawable *drawHead0 = g_DwdStore.GetDrawable(indexHead0, 0);
		const rmcDrawable *drawHead1 = g_DwdStore.GetDrawable(indexHead1, 0);
		Assert(drawHead0 && drawHead1);

		const fwTxd* txdHead0 = drawHead0->GetShaderGroup().GetTexDict();
		if (txdHead0)
		{
			for (int i = 0; i < txdHead0->GetCount(); ++i)
			{
				const grcTexture* texture = txdHead0->GetEntry(i);
				if (!strncmp(texture->GetName(), "head_spec", 9))
					spec0 = texture;
				else
					norm0 = texture;
			}
		}
		const fwTxd* txdHead1 = drawHead1->GetShaderGroup().GetTexDict();
		if (txdHead1)
		{
			for (int i = 0; i < txdHead1->GetCount(); ++i)
			{
				const grcTexture* texture = txdHead1->GetEntry(i);
				if (!strncmp(texture->GetName(), "head_spec", 9))
					spec1 = texture;
				else
					norm1 = texture;
			}
		}

		if (spec0 && norm0 && spec1 && norm1)
			MESHBLENDMANAGER.SetSpecAndNormalTextures(blendData->m_blendHandle, indexHead0, indexHead1);

		// blend data for feet
		s32 skinFeet0 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_FEET_0).Get();
		s32 skinFeet1 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_FEET_1).Get();
		s32 skinFeet2 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_FEET_2).Get();

		s32 clothFeet = pStreamReq->GetTxdSlot(PV_COMP_FEET);
		strLocalIndex feetMaskParentDwd = pStreamReq->GetDwdSlot(PV_COMP_FEET);
		rmcDrawable* drawableFeet = g_DwdStore.GetDrawable(feetMaskParentDwd, 0);

		const char* dbgName = "n/a";
		NOTFINAL_ONLY(dbgName = g_DwdStore.GetName(feetMaskParentDwd));
		const grcTexture* clothFeetMask = MESHBLENDMANAGER.GetSpecTextureFromDrawable(drawableFeet, dbgName);

		if (varData->HasRacialTexture(ped, PV_COMP_FEET))
		{
			if (skinFeet0 != -1 && skinFeet1 != -1 && skinFeet2 != -1 && clothFeet != -1 && clothFeetMask != NULL && feetMaskParentDwd != -1)
			{
				u8 palId = varData->m_aPaletteId[PV_COMP_FEET];
				MESHBLENDMANAGER.SetSkinBlendComponentData(blendData->m_blendHandle, SBC_FEET, clothFeet, skinFeet0, skinFeet1, skinFeet2, feetMaskParentDwd.Get(), palId);
			}
		}
		else
			MESHBLENDMANAGER.RemoveSkinBlendComponentData(blendData->m_blendHandle, SBC_FEET);

		// blend data for uppr
		s32 skinUppr0 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_UPPR_0).Get();
		s32 skinUppr1 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_UPPR_1).Get();
		s32 skinUppr2 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_UPPR_2).Get();

		s32 clothUppr = pStreamReq->GetTxdSlot(PV_COMP_UPPR);
		strLocalIndex upprMaskParentDwd = pStreamReq->GetDwdSlot(PV_COMP_UPPR);
		rmcDrawable* drawableUppr = g_DwdStore.GetDrawable(upprMaskParentDwd, 0);

		NOTFINAL_ONLY(dbgName = g_DwdStore.GetName(upprMaskParentDwd));
		const grcTexture* clothUpprMask = MESHBLENDMANAGER.GetSpecTextureFromDrawable(drawableUppr, dbgName);

		if (varData->HasRacialTexture(ped, PV_COMP_UPPR))
		{
			if (skinUppr0 != -1 && skinUppr1 != -1 && skinUppr2 != -1 && clothUppr != -1 && clothUpprMask != NULL && upprMaskParentDwd != -1)
			{
				u8 palId = varData->m_aPaletteId[PV_COMP_UPPR];
				MESHBLENDMANAGER.SetSkinBlendComponentData(blendData->m_blendHandle, SBC_UPPR, clothUppr, skinUppr0, skinUppr1, skinUppr2, upprMaskParentDwd.Get(), palId);
			}
		}
		else
			MESHBLENDMANAGER.RemoveSkinBlendComponentData(blendData->m_blendHandle, SBC_UPPR);

		// blend data for lowr
		s32 skinLowr0 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_LOWR_0).Get();
		s32 skinLowr1 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_LOWR_1).Get();
		s32 skinLowr2 = pStreamReq->GetHeadBlendSlot(HBS_SKIN_LOWR_2).Get();

		s32 clothLowr = pStreamReq->GetTxdSlot(PV_COMP_LOWR);
		strLocalIndex lowrMaskParentDwd = pStreamReq->GetDwdSlot(PV_COMP_LOWR);
		rmcDrawable* drawableLowr = g_DwdStore.GetDrawable(lowrMaskParentDwd, 0);

		NOTFINAL_ONLY(dbgName = g_DwdStore.GetName(lowrMaskParentDwd));
		const grcTexture* clothLowrMask = MESHBLENDMANAGER.GetSpecTextureFromDrawable(drawableLowr, dbgName);

		if (varData->HasRacialTexture(ped, PV_COMP_LOWR))
		{
			if (skinLowr0 != -1 && skinLowr1 != -1 && skinLowr2 != -1 && clothLowr != -1 && clothLowrMask != NULL && lowrMaskParentDwd != -1)
			{
				u8 palId = varData->m_aPaletteId[PV_COMP_LOWR];
				MESHBLENDMANAGER.SetSkinBlendComponentData(blendData->m_blendHandle, SBC_LOWR, clothLowr, skinLowr0, skinLowr1, skinLowr2, lowrMaskParentDwd.Get(), palId);
			}
		}
		else
			MESHBLENDMANAGER.RemoveSkinBlendComponentData(blendData->m_blendHandle, SBC_LOWR);

		// set custom palette color
		if (blendData->m_usePaletteRgb)
		{
			for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
			{
				MESHBLENDMANAGER.SetPaletteColor(blendData->m_blendHandle, true, blendData->m_paletteRed[i], blendData->m_paletteGreen[i], blendData->m_paletteBlue[i], i);
			}
		}

		// set micro morph data
		for (s32 i = 0; i < MMT_MAX; ++i)
		{
			pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - Blend 0x%08x set micromorph %d = %f", ped, blendData->m_blendHandle,
				i, blendData->m_microMorphBlends[i]);

			if (rage::Abs(blendData->m_microMorphBlends[i]) < 0.01f)
			{
				MESHBLENDMANAGER.MicroMorph(blendData->m_blendHandle, (eMicroMorphType)i, -1, 0.f);
				continue;
			}

			float assetBlend = 0.f;

			// deal with micro morph pairs. i.e. these blend [-1, 1] and use two different assets:
			// one for the [-1, 0] range and another for the [0, 1] range.
			if (i < MMT_NUM_PAIRS)
			{
				if (blendData->m_microMorphBlends[i] < 0.f)
				{
					assetBlend = rage::Abs(blendData->m_microMorphBlends[i]);
				}
				else
				{
					assetBlend = blendData->m_microMorphBlends[i];
				}
			}
			else
			{
				assetBlend = blendData->m_microMorphBlends[i];
			}

			MESHBLENDMANAGER.MicroMorph(blendData->m_blendHandle, (eMicroMorphType)i, pStreamReq->GetHeadBlendSlot(HBS_MICRO_MORPH_SLOTS + i).Get(), assetBlend);
		}

		// update eye color and hair tints
		pedDebugf1("[HEAD BLEND] AssetLoadFinished(%p) - Blend 0x%08x set eye colour %d, hair {%d, %d}", ped, blendData->m_blendHandle,
			blendData->m_eyeColor, blendData->m_hairTintIndex, blendData->m_hairTintIndex2);
		MESHBLENDMANAGER.SetEyeColor(blendData->m_blendHandle, blendData->m_eyeColor);
		MESHBLENDMANAGER.SetHairBlendRampIndex(blendData->m_blendHandle, blendData->m_hairTintIndex, blendData->m_hairTintIndex2);
		CPedVariationStream::SetPaletteTexture(ped, PV_COMP_HAIR);
	}

#if GTA_REPLAY
	if ( CReplayMgr::IsEditModeActive() && blendData->m_blendHandle != MESHBLEND_HANDLE_INVALID)
	{
		ReplayPedExtension *extension = ReplayPedExtension::GetExtension(ped);
		if( extension )
		{
			extension->ApplyHeadBlendDataPostBlendHandleCreate(ped);
		}
	}
#endif //GTA_REPLAY

	return true;
}

void CPedVariationData::SetHeadBlendData(CPed* ped, const CPedHeadBlendData& data, bool bForce)
{
	static const float blendEpsilon = 0.000001f;

	if (!PARAM_noheadblend.Get())
	{
		// early out if we're setting same blend, since it's quite expensive going through with it. and scripts tend to spam set settings
		CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
		if (blendData)
		{
			if (!bForce)
			{
				if (blendData->m_head0 == data.m_head0 && blendData->m_head1 == data.m_head1 && blendData->m_head2 == data.m_head2 &&
					blendData->m_tex0 == data.m_tex0 && blendData->m_tex1 == data.m_tex1 && blendData->m_tex2 == data.m_tex2 &&
					fabs(blendData->m_headBlend - data.m_headBlend) < blendEpsilon && fabs(blendData->m_texBlend - data.m_texBlend) < blendEpsilon && fabs(blendData->m_varBlend - data.m_varBlend) < blendEpsilon)
				{
					pedDebugf1("[HEAD BLEND] CPedVariationData::SetHeadBlendData: Ignoring redundant blend");
					return;
				}
			}
		}
		else
        {
			blendData = rage_new CPedHeadBlendData();
            ped->GetExtensionList().Add(*blendData);
        }

#if GTA_REPLAY
		sReplayHeadPedBlendData previousReplayData;
		previousReplayData.SetData(blendData->m_head0, blendData->m_head1, blendData->m_head2, blendData->m_tex0, blendData->m_tex1, blendData->m_tex2,
			blendData->m_headBlend, blendData->m_texBlend, blendData->m_varBlend, blendData->m_isParent);
#endif

		blendData->m_head0 = data.m_head0;
		blendData->m_head1 = data.m_head1;
		blendData->m_head2 = data.m_head2;
		blendData->m_tex0 = data.m_tex0;
		blendData->m_tex1 = data.m_tex1;
		blendData->m_tex2 = data.m_tex2;
		blendData->m_headBlend = data.m_headBlend;
		blendData->m_texBlend = data.m_texBlend;
		blendData->m_varBlend = data.m_varBlend;
		blendData->m_async = data.m_async;
		blendData->m_isParent = data.m_isParent;

		blendData->m_hasParents = data.m_hasParents;
		if (blendData->m_hasParents)
		{
			blendData->m_extraParentData.head0Parent0 = data.m_extraParentData.head0Parent0;
			blendData->m_extraParentData.head0Parent1 = data.m_extraParentData.head0Parent1;
			blendData->m_extraParentData.parentBlend  = data.m_extraParentData.parentBlend;
			blendData->m_parentBlend2				  = data.m_parentBlend2;
		}

		pedDebugf1("[HEAD BLEND] CPedVariationData::SetHeadBlendData: Setting data on %p: {%f, %d, %d, %d} {%f, %d, %d, %d}", ped,
			data.m_headBlend, data.m_head0, data.m_head1, data.m_head2, data.m_texBlend, data.m_tex0, data.m_tex1, data.m_tex2);
		CPedVariationStream::RequestStreamPedFiles(ped, this);

#if GTA_REPLAY
		sReplayHeadPedBlendData replayData;
		replayData.SetData(blendData->m_head0, blendData->m_head1, blendData->m_head2, blendData->m_tex0, blendData->m_tex1, blendData->m_tex2,
			blendData->m_headBlend, blendData->m_texBlend, blendData->m_varBlend, blendData->m_isParent);
		if((CReplayMgr::ShouldRecord() || CReplayMgr::IsRecording()) && ped)
		{
			//if we don't have the extension then we need to get one
			if(!ReplayPedExtension::HasExtension(ped))
			{
				if(!ReplayPedExtension::Add(ped))
					replayAssertf(false, "Failed to add a ped extension...ran out?");
			}

			ReplayPedExtension::SetHeadBlendData(ped, replayData);

			if( CReplayMgr::IsRecording() && ped)
			{
				//Record headblendchange
				CReplayMgr::RecordFx<CPacketPedHeadBlendChange>(CPacketPedHeadBlendChange(replayData, previousReplayData), ped);
			}
		}
#endif //GTA_REPLAY
	}
}

void CPedVariationData::SetHeadOverlay(CPed* ped, eHeadOverlaySlots slot, u8 tex, float blend, float normBlend)
{
	if (!PARAM_noheadblend.Get())
	{
		if (!Verifyf(HasHeadBlendData(ped), "SetHeadOverlay used before SetHeadBlendData!"))
		{
			return;
		}

		// using low blend values won't have any visual effect so we cull these here
		if (blend <= 0.1f && normBlend <= 0.1f)
		{
			blend = 0.f;
			normBlend = 0.f;
			tex = 0xff;
		}

		CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
		
		// early out if we're setting same overlay, since it's quite expensive going through with it. 
		if (tex == blendData->m_overlayTex[slot] && rage::IsClose(blend, blendData->m_overlayAlpha[slot], 0.01f) && rage::IsClose(normBlend, blendData->m_overlayNormAlpha[slot], 0.01f))
		{
			return;
		}

		blendData->m_overlayTex[slot] = tex;
		blendData->m_overlayAlpha[slot] = blend;
		blendData->m_overlayNormAlpha[slot] = normBlend;

		CPedVariationStream::RequestStreamPedFiles(ped, this);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && ped)
		{
			//if we don't have the extension then we need to get one
			if(!ReplayPedExtension::HasExtension(ped))
			{
				if(!ReplayPedExtension::Add(ped))
					replayAssertf(false, "Failed to add a ped extension...ran out?");
			}
			ReplayPedExtension::SetHeadOverlayData(ped, slot, blendData->m_overlayTex[slot], blendData->m_overlayAlpha[slot], blendData->m_overlayNormAlpha[slot]);
		}
#endif //GTA_REPLAY
	}
}

void CPedVariationData::UpdateHeadOverlayBlend(CPed* ped, eHeadOverlaySlots slot, float blend, float normBlend)
{
	if (!PARAM_noheadblend.Get())
	{
		if (!Verifyf(HasHeadBlendData(ped), "UpdateHeadOverlayBlend used before SetHeadBlendData!"))
			return;

		// using low blend values won't have any visual effect so we cull these here
		if (blend <= 0.1f && normBlend <= 0.1f)
		{
			blend = 0.f;
			normBlend = 0.f;
		}

		CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
		
		blendData->m_overlayAlpha[slot] = blend;
		blendData->m_overlayNormAlpha[slot] = normBlend;
		MESHBLENDMANAGER.UpdateOverlayBlend(blendData->m_blendHandle, slot, blendData->m_overlayAlpha[slot], blendData->m_overlayNormAlpha[slot]);
	}
}

void CPedVariationData::SetHeadOverlayTintIndex(CPed* ped, eHeadOverlaySlots slot, eRampType rampType, u8 tint, u8 tint2)
{
	if (PARAM_noheadblend.Get())
		return;

	if (!Verifyf(HasHeadBlendData(ped), "SetHeadOverlayTintIndex used before SetHeadBlendData!"))
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	blendData->m_overlayTintIndex[slot] = tint;
	blendData->m_overlayTintIndex2[slot] = tint2;
	blendData->m_overlayRampType[slot] = (u8)rampType;
	MESHBLENDMANAGER.SetOverlayTintIndex(blendData->m_blendHandle, slot, rampType, tint, tint2);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && ped)
	{
		//if we don't have the extension then we need to get one
		if(!ReplayPedExtension::HasExtension(ped))
		{
			if(!ReplayPedExtension::Add(ped))
				replayAssertf(false, "Failed to add a ped extension...ran out?");
		}
		ReplayPedExtension::SetHeadOverlayTintData(ped, slot, rampType, tint, tint2);
	}
#endif //GTA_REPLAY
}

void CPedVariationData::SetNewHeadBlendValues(CPed* ped, float headBlend, float texBlend, float varBlend)
{
	if (!PARAM_noheadblend.Get())
	{
		if (!HasHeadBlendData(ped))
			return;

		CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
		blendData->m_headBlend = headBlend;
		blendData->m_texBlend = texBlend;
		blendData->m_varBlend = varBlend;

		if (MESHBLENDMANAGER.IsHandleValid(blendData->m_blendHandle))
		{
			MESHBLENDMANAGER.SetNewBlendValues(blendData->m_blendHandle, blendData->m_headBlend, blendData->m_texBlend, blendData->m_varBlend);
		}
	}
}

bool CPedVariationData::HasHeadBlendData(CPed* ped) const
{
	if (!ped || PARAM_noheadblend.Get())
		return false;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return false;

	return true;
}

bool CPedVariationData::HasHeadBlendFinished(CPed* ped) const
{
	bool headBlendFinished = true;
	if (!ped || PARAM_noheadblend.Get())
		return headBlendFinished;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return headBlendFinished;

    // if we have blend data but an invalid handle the blend slot has probably
    // not been requested yet, but it will as soon as the source assets are loaded
    // so we need to return false here or script will think the blend is done for
    // a frame or two before it even starts
    if (!MESHBLENDMANAGER.IsHandleValid(blendData->m_blendHandle))
	{
		pedDebugf1("[HEAD BLEND] HasHeadBlendFinished(%p): Invalid blend handle: 0x%08x", ped, blendData->m_blendHandle);
        return false;
	}

	headBlendFinished = MESHBLENDMANAGER.HasBlendFinished(blendData->m_blendHandle);

	CPedStreamRequestGfx* req = ped->GetExtensionList().GetExtension<CPedStreamRequestGfx>();
	if (!req)
		return headBlendFinished;

	headBlendFinished &= req->IsReady();

	return headBlendFinished;
}

void CPedVariationData::FinalizeHeadBlend(CPed* ped) const
{
	if (!ped || PARAM_noheadblend.Get())
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
	{
		pedDebugf1("[HEAD BLEND] CPedVariationData::FinalizeHeadBlend(%p): No head blend data", ped);
		return;
	}

	MESHBLENDMANAGER.FinalizeHeadBlend(blendData->m_blendHandle);

	if (ped->GetPedDrawHandler().GetPedRenderGfx())
		ped->GetPedDrawHandler().GetPedRenderGfx()->ReleaseHeadBlendAssets();
}

bool CPedVariationData::SetHeadBlendRefreshPaletteColor(CPed* ped)
{
	if (!ped || PARAM_noheadblend.Get())
		return false;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return false;

	return MESHBLENDMANAGER.SetRefreshPaletteColor(blendData->m_blendHandle);
}

bool CPedVariationData::SetHeadBlendPaletteColor(CPed* ped, u8 r, u8 g, u8 b, u8 colorIndex, bool bforce)
{
	pedDebugf3("CPedVariationData::SetHeadBlendPaletteColor i[%d] rgb[%d %d %d] bforce[%d]",colorIndex,r,g,b,bforce);

	if (!ped || PARAM_noheadblend.Get())
	{
		pedDebugf3("(!ped || PARAM_noheadblend.Get())-->return false");
		return false;
	}

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
	{
		pedDebugf3("!blendData-->return false");
		return false;
	}

	blendData->m_paletteRed[colorIndex] = r;
	blendData->m_paletteGreen[colorIndex] = g;
	blendData->m_paletteBlue[colorIndex] = b;
	blendData->m_usePaletteRgb = true;

	return MESHBLENDMANAGER.SetPaletteColor(blendData->m_blendHandle, true, r, g, b, colorIndex, bforce);
}

void CPedVariationData::GetHeadBlendPaletteColor(CPed* ped, u8& r, u8& g, u8& b, u8 colorIndex)
{
	if (!ped)
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
	{
		pedDebugf3("CPedVariationData::GetHeadBlendPaletteColor--!blendData-->return");
		return;
	}

	MESHBLENDMANAGER.GetPaletteColor(blendData->m_blendHandle, r, g, b, colorIndex);
}

void CPedVariationData::SetHeadBlendPaletteColorDisabled(CPed* ped)
{
	if (!ped || PARAM_noheadblend.Get())
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return;

	// Early out if the head blend palette is already disabled.
	if(blendData->m_usePaletteRgb == false)
		return;

	blendData->m_usePaletteRgb = false;

	MESHBLENDMANAGER.SetPaletteColor(blendData->m_blendHandle, false);
}

void CPedVariationData::SetHeadBlendEyeColor(CPed* ped, s32 colorIndex)
{
	if (!ped || PARAM_noheadblend.Get())
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return;

    blendData->m_eyeColor = (s8)colorIndex;

	MESHBLENDMANAGER.SetEyeColor(blendData->m_blendHandle, colorIndex);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && ped)
	{
		//if we don't have the extension then we need to get one
		if(!ReplayPedExtension::HasExtension(ped))
		{
			if(!ReplayPedExtension::Add(ped))
				replayAssertf(false, "Failed to add a ped extension...ran out?");
		}
		ReplayPedExtension::SetEyeColor(ped, colorIndex);
	}
#endif //GTA_REPLAY
}

s32 CPedVariationData::GetHeadBlendEyeColor(CPed* ped)
{
	if (!ped || PARAM_noheadblend.Get())
		return -1;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return -1;

	return(blendData->m_eyeColor);
}

void CPedVariationData::MicroMorph(CPed* ped, eMicroMorphType type, float blend)
{
	if (!ped || PARAM_noheadblend.Get())
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return;

	// Early out if we're setting the same blend on this micro morph type. 
	// This is especially important here, so that we don't make the call to 
	// RequestStreamPedFiles() below unnecessarily (which triggers many 
	// things that are very expensive).
	if(fabs(blendData->m_microMorphBlends[type] - blend) < 0.0001f)
		return;

#if GTA_REPLAY
	float previousMicroMorphBlend = blendData->m_microMorphBlends[type];
#endif

	blendData->m_microMorphBlends[type] = blend;
	CPedVariationStream::RequestStreamPedFiles(ped, this);

#if GTA_REPLAY
	if((CReplayMgr::ShouldRecord() || CReplayMgr::IsRecording()) && ped)
	{
		//if we don't have the extension then we need to get one
		if(!ReplayPedExtension::HasExtension(ped))
		{
			if(!ReplayPedExtension::Add(ped))
				replayAssertf(false, "Failed to add a ped extension...ran out?");
		}
		ReplayPedExtension::SetHeadMicroMorphData(ped, static_cast<u32>(type), blend);

		if( CReplayMgr::IsRecording() && ped)
		{
			//Record mircomorphchange
			CReplayMgr::RecordFx<CPacketPedMicroMorphChange>(CPacketPedMicroMorphChange(static_cast<u32>(type), blend, previousMicroMorphBlend), ped);
		}
	}
#endif //GTA_REPLAY
}

void CPedVariationData::SetHairTintIndex(CPed* ped, u32 index, u32 index2)
{
	if (!ped || PARAM_noheadblend.Get())
		return;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return;

	blendData->m_hairTintIndex = (u8)index;
	blendData->m_hairTintIndex2 = (u8)index2;

	MESHBLENDMANAGER.SetHairBlendRampIndex(blendData->m_blendHandle, (u8)index, (u8)index2);
	CPedVariationStream::SetPaletteTexture(ped, PV_COMP_HAIR);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && ped)
	{
		//if we don't have the extension then we need to get one
		if(!ReplayPedExtension::HasExtension(ped))
		{
			if(!ReplayPedExtension::Add(ped))
				replayAssertf(false, "Failed to add a ped extension...ran out?");
		}
		ReplayPedExtension::SetHairTintIndex(ped, (u8)index, (u8)index2);
	}
#endif //GTA_REPLAY
}

bool CPedVariationData::GetHairTintIndex(CPed* ped, u32& outIndex, u32& outIndex2)
{
	if (!ped || PARAM_noheadblend.Get())
		return false;

	CPedHeadBlendData* blendData = ped->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData)
		return false;

	outIndex	= blendData->m_hairTintIndex;
	outIndex2	= blendData->m_hairTintIndex2;

	return true;
}

void CPedVariationData::RemoveAllCloth(bool restoreValue)
{
	for( int i = 0; i < PV_MAX_COMP; ++i)
	{
		RemoveCloth( (ePedVarComp) i, restoreValue );
	}
}

bool CPedVariationData::IsThisSettingSameAs(ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId, void* clothPtr, s32 dictionarySlotId) 
{
	bool retVal = true;

	retVal &= (!m_clothVarData[slotId] || m_clothVarData[slotId]->GetClothDictionarySlotId() == dictionarySlotId);
	retVal &= (!m_clothVarData[slotId] || m_clothVarData[slotId]->GetCloth() == clothPtr);
	retVal &= (m_aDrawblId[slotId] == drawblId);
	retVal &= (m_aDrawblAltId[slotId] == (u8)drawblAltId);
	retVal &= (m_aTexId[slotId] == (u8)texId);
	retVal &= (m_aPaletteId[slotId] == (u8)paletteId);

	return(retVal);
}

void CPedVariationData::SetPedVariation(ePedVarComp slotId, u32 drawblId, u32 drawblAltId, u32 texId, u32 paletteId, clothVariationData* pClothVarData/*=NULL*/, bool requireShaderSetup) 
{
	SetClothData( slotId, pClothVarData );

	m_aDrawblId[slotId] = drawblId;
	m_aDrawblAltId[slotId] = (u8)drawblAltId;
	m_aTexId[slotId] = (u8)texId;
	m_aPaletteId[slotId] = (u8)paletteId;

	m_aDrawblHash[slotId] = 0;		// invalidate the hash so it gets recalculated for the rage_new value

	m_bIsShaderSetup = !requireShaderSetup;		// shader needs setup

    m_bVariationSet = true;
}

ePedVarComp CPedVariationData::GetComponentSlotFromBoneTag(eAnimBoneTag tag)
{
	switch (tag)
	{
		case BONETAG_ROOT:
		case BONETAG_PELVIS:
		case BONETAG_SPINE_ROOT:
		case BONETAG_SPINE0:
		case BONETAG_SPINE1:
		case BONETAG_SPINE2:
		case BONETAG_SPINE3:
		case BONETAG_NECK:
		case BONETAG_R_CLAVICLE:
		case BONETAG_R_UPPERARM:
		case BONETAG_R_FOREARM:
		case BONETAG_L_CLAVICLE:
		case BONETAG_L_UPPERARM:
		case BONETAG_L_FOREARM:
		case BONETAG_NECKROLL:
		case BONETAG_L_ARMROLL:
		case BONETAG_R_ARMROLL:
		case BONETAG_L_FOREARMROLL:
		case BONETAG_R_FOREARMROLL:
			return PV_COMP_UPPR;
		case BONETAG_R_HAND:
		case BONETAG_R_FINGER0:
		case BONETAG_R_FINGER01:
		case BONETAG_R_FINGER02:
		case BONETAG_R_FINGER1:
		case BONETAG_R_FINGER11:
		case BONETAG_R_FINGER12:
		case BONETAG_R_FINGER2:
		case BONETAG_R_FINGER21:
		case BONETAG_R_FINGER22:
		case BONETAG_R_FINGER3:
		case BONETAG_R_FINGER31:
		case BONETAG_R_FINGER32:
		case BONETAG_R_FINGER4:
		case BONETAG_R_FINGER41:
		case BONETAG_R_FINGER42:
		case BONETAG_L_FINGER0:
		case BONETAG_L_FINGER01:
		case BONETAG_L_FINGER02:
		case BONETAG_L_FINGER1:
		case BONETAG_L_FINGER11:
		case BONETAG_L_FINGER12:
		case BONETAG_L_FINGER2:
		case BONETAG_L_FINGER21:
		case BONETAG_L_FINGER22:
		case BONETAG_L_FINGER3:
		case BONETAG_L_FINGER31:
		case BONETAG_L_FINGER32:
		case BONETAG_L_FINGER4:
		case BONETAG_L_FINGER41:
		case BONETAG_L_FINGER42:
		case BONETAG_L_PH_HAND:
		case BONETAG_R_PH_HAND:
		case BONETAG_WEAPON_GRIP:
		case BONETAG_WEAPON_GRIP2:
			return PV_COMP_HAND;
		case BONETAG_HEAD:
			return PV_COMP_HEAD;
		case BONETAG_L_THIGH:
		case BONETAG_L_CALF:
		case BONETAG_R_THIGH:
		case BONETAG_R_CALF:
		case BONETAG_L_THIGHROLL:
		case BONETAG_R_THIGHROLL:
			return PV_COMP_LOWR;
		case BONETAG_L_FOOT:
		case BONETAG_L_TOE:
		case BONETAG_R_FOOT:
		case BONETAG_R_TOE:
			return PV_COMP_FEET;
		default:
			return PV_COMP_INVALID;
	}
}

u32 CPedVariationData::SetPreloadData(CPed* ped, ePedVarComp slotId, u32 drawableId, u32 textureId, u32 streamingFlags)
{
	Assert(ped);

	CPedModelInfo* pmi = ped->GetPedModelInfo();
	Assert(pmi);
	Assert(pmi->GetIsStreamedGfx());

	CPedVariationInfoCollection* varInfo = pmi->GetVarInfo();
	Assert(varInfo);
	if (!varInfo)
		return 0;

	const CPedStreamRenderGfx* pGfxData = ped->GetPedDrawHandler().GetConstPedRenderGfx();
	if (pGfxData)
	{
		rmcDrawable* pDrawable = pGfxData->GetDrawable(PV_COMP_HEAD);
		if (pDrawable && CPedVariationStream::IsBlendshapesSetup(pDrawable))
		{
			Assertf(false, "Can't modify player variation when blendshapes are enabled");
			return 0;
		}
	}

	CPVDrawblData* pDrawblData = NULL;
	if (drawableId != PV_NULL_DRAWBL)
	{
		pDrawblData = varInfo->GetCollectionDrawable(slotId, drawableId);
		if (!pDrawblData)
			return 0;

		if (!pDrawblData->HasActiveDrawbl())
			return 0;
	}

	if (!Verifyf(pDrawblData, "Trying to preload invalid assets (%d - %d - %d) on ped '%s'", slotId, drawableId, textureId, pmi->GetModelName()))
		return 0;

	Assertf(drawableId != PV_NULL_DRAWBL || slotId != PV_COMP_HEAD, "Empty drawable cannot be set on head component! (%s)", ped->GetModelName());
	Assertf(drawableId == PV_NULL_DRAWBL || drawableId < varInfo->GetMaxNumDrawbls(slotId), " DrawableId(%d) > (%d)MaxNumDrawbls || Invalid variation: %s : %s_%3d_%c", 
																						drawableId, 
																						varInfo->GetMaxNumDrawbls(slotId), 
																						pmi->GetModelName(), 
																						varSlotNames[slotId],
																						drawableId,
																						'a' + textureId);

	Assertf(drawableId == PV_NULL_DRAWBL || textureId < varInfo->GetMaxNumTex(slotId, drawableId), " texId(%d) > (%d)MaxNumTex || Invalid variation: %s : %s_%3d_%c", 
																						textureId, 
																						varInfo->GetMaxNumTex(slotId, drawableId), 
																						pmi->GetModelName(), 
																						varSlotNames[slotId],
																						drawableId,
																						'a' + textureId);

	// generate the required slot names and store them to be requested in a moment
	char dlcPostFix[STORE_NAME_LENGTH] = { 0 };
	char drawablName[STORE_NAME_LENGTH];
	char diffMap[STORE_NAME_LENGTH];
	const char*	pedFolder = pmi->GetStreamFolder();

	if (const char* tempDLCName = varInfo->GetDlcNameFromDrawableIdx(slotId, drawableId))
		formatf(dlcPostFix, sizeof(dlcPostFix), "_%s", tempDLCName);
	else
		safecpy(dlcPostFix, "");

	drawableId = varInfo->GetDlcDrawableIdx((ePedVarComp)slotId, drawableId);

	if (pDrawblData->IsUsingRaceTex())
		formatf(drawablName, "%s%s/%s_%03d_r", pedFolder, dlcPostFix, varSlotNames[slotId], drawableId);
	else
		formatf(drawablName, "%s%s/%s_%03d_u", pedFolder, dlcPostFix, varSlotNames[slotId], drawableId);

    if (!Verifyf(textureId < pDrawblData->GetNumTex(), "Invalid texture index %d passed to SetPreloadData on ped '%s' for component %s_%03d", textureId, pmi->GetModelName(), varSlotNames[slotId], drawableId))
        return 0;

	u8 raceIdx = pDrawblData->GetTexData(textureId);
	if (!Verifyf(raceIdx != (u8)PV_RACE_INVALID, "Bad race index for drawable %s with textureId %d on ped %s", drawablName, textureId, ped->GetModelName()))
		return 0;

	formatf(diffMap, "%s%s/%s_diff_%03d_%c_%s", pedFolder, dlcPostFix, varSlotNames[slotId], drawableId, (textureId+'a'), raceTypeNames[raceIdx]);

	bool addToScriptMem = !pmi->GetIsPlayerModel();

	if (!m_preloadData)
		m_preloadData = rage_new CScriptPreloadData(PV_MAX_COMP * SCRIPT_PRELOAD_MULTIPLIER, PV_MAX_COMP * SCRIPT_PRELOAD_MULTIPLIER, PV_MAX_COMP * SCRIPT_PRELOAD_MULTIPLIER, 0, 0, addToScriptMem);

    s32 txdSlot = -1;
    s32 dwdSlot = -1;
    s32 cldSlot = -1;
    bool slotFound = true;

    Assertf(g_TxdStore.FindSlot(diffMap) != -1, "Missing ped texture %s", diffMap);
    Assertf(g_DwdStore.FindSlot(drawablName) != -1, "Missing ped drawable %s", drawablName);

    slotFound &= m_preloadData->AddTxd(g_TxdStore.FindSlot(diffMap), txdSlot, streamingFlags);
    slotFound &= m_preloadData->AddDwd(g_DwdStore.FindSlot(drawablName), CPedModelInfo::GetCommonTxd(), dwdSlot, streamingFlags);
	if (pDrawblData->m_clothData.HasCloth())
	{
		Assertf(g_ClothStore.FindSlot(drawablName) != -1, "Missing ped cloth %s", drawablName);
        slotFound &= m_preloadData->AddCld(g_ClothStore.FindSlot(drawablName), cldSlot, streamingFlags);
	}

    Assertf(slotFound, "No more preload slots available for '%s', please clean up old ones first!", pmi->GetModelName());
    return m_preloadData->GetHandle(dwdSlot, txdSlot, cldSlot);
}

void CPedVariationData::CleanUpPreloadData()
{
	delete m_preloadData;
	m_preloadData = NULL;
}

void CPedVariationData::CleanUpPreloadData(u32 handle)
{
	if (m_preloadData)
		m_preloadData->ReleaseHandle(handle);
}

bool CPedVariationData::HasPreloadDataFinished()
{
    if (!m_preloadData)
        return true;
    return m_preloadData->HasPreloadFinished();
}

bool CPedVariationData::HasPreloadDataFinished(u32 handle)
{
	if (!m_preloadData)
		return true;
	return m_preloadData->HasPreloadFinished(handle);
}

#if !__NO_OUTPUT
const char * CPedVariationData::GetVarOrPropSlotName(int component)
{
	if (component>-1 && component<PV_MAX_COMP)
	{
		return varSlotNames[component];
	}
	else if ((component-PV_MAX_COMP) < NUM_ANCHORS)
	{
		return propSlotNames[component-PV_MAX_COMP];
	}

	return "UNKNOWN";
}
#endif // !__NO_OUTPUT


void CSyncedPedVarData::Reset()
{
	m_usedComponents = 0;
	m_playerData = false;

	for (u32 i=0; i<PV_MAX_COMP; i++)
	{
		m_componentData[i] = 0;
		m_textureData[i] = 0;
		m_paletteData[i] = 0;
		m_varInfoHash[i] = 0;
	}
}

void CSyncedPedVarData::ExtractFromPed(CPed& ped)
{
	CPedVariationData&	varData = ped.GetPedDrawHandler().GetVarData();

	m_usedComponents = 0;
	m_playerData = ped.IsAPlayerPed();

	for (int i=0; i<PV_MAX_COMP; i++)
	{
		m_textureData[i]	= (u32)varData.GetPedTexIdx(static_cast<ePedVarComp>(i));
		m_paletteData[i]	= (u32)varData.GetPedPaletteIdx(static_cast<ePedVarComp>(i));

		CPedVariationPack::GetLocalCompData(&ped, (ePedVarComp)i, varData.GetPedCompIdx(static_cast<ePedVarComp>(i)), m_varInfoHash[i], m_componentData[i]);

		bool hasComponent = m_componentData[i] != 0 && m_componentData[i] != 255;
		bool hasTexture = m_textureData[i] != 0 && m_textureData[i] != 255;
		bool hasPalette = m_paletteData[i] != 0 && m_paletteData[i] != 255;
		bool hasVarInfoHash = m_varInfoHash[i] != 0;

		//The ped might have previously been a playerped - in which case the components might have a number index greater than the max allowable for a basic ped.
		if (m_componentData[i] >= MAX_DRAWBL_VARS)
			m_playerData = true;

		if (hasComponent || hasTexture || hasPalette || hasVarInfoHash)
		{
			m_usedComponents |= (1<<i);
		}
	}
}

void CSyncedPedVarData::ApplyToPed(CPed& ped, bool forceApply) const
{
	bool bVariationChanged = forceApply;

	if (!bVariationChanged)
	{
		// apply variation if different
		CSyncedPedVarData currentData;

		currentData.ExtractFromPed(ped);

		if (currentData.m_usedComponents != m_usedComponents)
		{
			bVariationChanged = true;
		}
		else
		{
			for (int i = 0; i < PV_MAX_COMP; i++)
			{
				if (currentData.m_componentData[i] != m_componentData[i] ||
					currentData.m_textureData[i] != m_textureData[i] ||
					currentData.m_paletteData[i] != m_paletteData[i] ||
					currentData.m_varInfoHash[i] != m_varInfoHash[i])
				{
					pedDebugf3("CSyncedPedVarData::ApplyToPed bVariationChanged[1] i[%d] currentData.m_componentData[%d] m_componentData[%d] currentData.m_textureData[%d] m_textureData[%d] currentData.m_paletteData[%d] m_paletteData[%d] currentdata.m_varInfoHash[%d] m_varInfoHash[%d]"
						, i
						, currentData.m_componentData[i], m_componentData[i]
						, currentData.m_textureData[i], m_textureData[i]
						, currentData.m_paletteData[i], m_paletteData[i]
						, currentData.m_varInfoHash[i], m_varInfoHash[i]
					);

					bVariationChanged = true;
					break;
				}
			}
		}
	}

	ePedVarComp variationOrder[] = {
		PV_COMP_HEAD,
		PV_COMP_HAIR,
		PV_COMP_UPPR,
		PV_COMP_LOWR,
		PV_COMP_HAND,
		PV_COMP_FEET,
		PV_COMP_TEEF,
		PV_COMP_ACCS,
		PV_COMP_TASK,
		PV_COMP_DECL,
		PV_COMP_JBIB,
		PV_COMP_BERD,	// Do the BERD last to ensure that hair is hidden
	};

	if(bVariationChanged)
	{
		for(int i = 0; i < PV_MAX_COMP; i++)
		{
			ePedVarComp comp = variationOrder[i];
			u32 compIdx = CPedVariationPack::GetGlobalCompData(&ped, comp, m_varInfoHash[comp], m_componentData[comp]);

			ped.SetVariation(comp, compIdx, 0, m_textureData[comp], m_paletteData[comp], 0, false);
		}
	}
}

void CSyncedPedVarData::Serialise(class CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_USED_COMPONENTS = PV_MAX_COMP;
	static const unsigned SIZEOF_PED_COMPONENT = datBitsNeeded<MAX_DRAWBL_VARS-1>::COUNT;
	static const unsigned SIZEOF_PLAYER_COMPONENT = datBitsNeeded<MAX_PLAYER_DRAWBL_VARS-1>::COUNT;
	static const unsigned SIZEOF_TEXTURE = datBitsNeeded<MAX_TEX_VARS-1>::COUNT;
	static const unsigned SIZEOF_PALETTE = datBitsNeeded<MAX_PALETTES-1>::COUNT;
	static const unsigned SIZEOF_VAR_INFO_HASH = 32;

	bool hasData = m_usedComponents != 0;

	SERIALISE_BOOL(serialiser, hasData);

	if (hasData || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_usedComponents, SIZEOF_USED_COMPONENTS);

		SERIALISE_BOOL(serialiser, m_playerData);

		for (int i=0; i<PV_MAX_COMP; i++)
		{
			if ((m_usedComponents & (1<<i)) || serialiser.GetIsMaximumSizeSerialiser())
			{
				if (m_playerData || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_UNSIGNED(serialiser, m_componentData[i], SIZEOF_PLAYER_COMPONENT, "Player var component");
				}
				else
				{
					SERIALISE_UNSIGNED(serialiser, m_componentData[i], SIZEOF_PED_COMPONENT, "Ped var component");
				}

				bool hasTexture     = m_textureData[i] != 0 && m_textureData[i] != 255;
				bool hasPalette     = m_paletteData[i] != 0 && m_paletteData[i] != 255;
                bool hasVarInfoHash = m_varInfoHash[i] != 0;

				SERIALISE_BOOL(serialiser, hasTexture);

				if (hasTexture || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_UNSIGNED(serialiser, m_textureData[i], SIZEOF_TEXTURE, "Ped var texture");
				}
				else
				{
					m_textureData[i] = 0;
				}

				SERIALISE_BOOL(serialiser, hasPalette);

				if (hasPalette || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_UNSIGNED(serialiser, m_paletteData[i], SIZEOF_PALETTE, "Ped var palette");
				}
				else
				{
					m_paletteData[i] = 0;
				}

                SERIALISE_BOOL(serialiser, hasVarInfoHash);

				if (hasVarInfoHash || serialiser.GetIsMaximumSizeSerialiser())
                {
					SERIALISE_UNSIGNED(serialiser, m_varInfoHash[i], SIZEOF_VAR_INFO_HASH, "Ped var info hash");
                }
				else
                {
					m_varInfoHash[i] = 0;
                }
			}
			else
			{
				m_componentData[i]	= 0;
				m_textureData[i]	= 0;
				m_paletteData[i]	= 0;
				m_varInfoHash[i] = 0;
			}
		}
	}
	else
	{
		m_usedComponents = 0;

		for (int i=0; i<PV_MAX_COMP; i++)
		{
			m_componentData[i]	= 0;
			m_textureData[i]	= 0;
			m_paletteData[i]	= 0;
			m_varInfoHash[i]    = 0;
		}
	}

	if (serialiser.GetLog())
	{
		Log(*serialiser.GetLog());
	}
}

void CSyncedPedVarData::Log(class netLoggingInterface& log)
{
	if (m_usedComponents == 0)
	{
		log.WriteDataValue("Ped Variation Data", "None");
	}
	else
	{
		for (int i=0; i<PV_MAX_COMP; i++)
		{
			if (m_usedComponents & (1<<i))
			{
				switch (i)
				{
				case PV_COMP_HEAD:
					log.WriteDataValue("Ped Variation Data", "HEAD");
					break;
				case PV_COMP_BERD:
					log.WriteDataValue("Ped Variation Data", "BEARD");
					break;
				case PV_COMP_HAIR:
					log.WriteDataValue("Ped Variation Data", "HAIR");
					break;
				case PV_COMP_UPPR:
					log.WriteDataValue("Ped Variation Data", "UPPER");
					break;
				case PV_COMP_LOWR:
					log.WriteDataValue("Ped Variation Data", "LOWER");
					break;
				case PV_COMP_HAND:
					log.WriteDataValue("Ped Variation Data", "HAND");
					break;
				case PV_COMP_FEET:
					log.WriteDataValue("Ped Variation Data", "FEET");
					break;
				case PV_COMP_TEEF:
					log.WriteDataValue("Ped Variation Data", "TEETH");
					break;
				case PV_COMP_ACCS:
					log.WriteDataValue("Ped Variation Data", "ACCESSORIES");
					break;
				case PV_COMP_TASK:
					log.WriteDataValue("Ped Variation Data", "TASK");
					break;
				case PV_COMP_DECL:
					log.WriteDataValue("Ped Variation Data", "DECL");
					break;
				case PV_COMP_JBIB:
					log.WriteDataValue("Ped Variation Data", "JBIB");
					break;
				default:
					Assertf(0, "Unrecognised ped variation data component %u", i);
				}

				if (m_componentData[i] != 0)
				{
					log.WriteDataValue("Drawable", "%u", m_componentData[i]);
				}

				if (m_textureData[i] != 0)
				{
					log.WriteDataValue("Texture", "%u", m_textureData[i]);
				}

				if (m_paletteData[i] != 0)
				{
					log.WriteDataValue("Palette", "%u", m_paletteData[i]);
				}
			}
		}
	}
}

void CPedHeadBlendData::Serialise(class CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_BLEND_RATIO	= 8;
	static unsigned const MAX_HEAD_BLEND_DATA	= 3;
	static unsigned const MAX_TEX_BLEND_DATA	= 3;
	static const unsigned SIZEOF_HEAD_DATA      = 8;
	static const unsigned SIZEOF_TEX_DATA       = 8;
	static const unsigned SIZEOF_OVERLAY_TEX    = 8;
	static const unsigned SIZEOF_OVERLAY_ALPHA	= 8;
	static const unsigned SIZEOF_OVERLAY_NORMAL	= 8;
	static const unsigned SIZEOF_PALETTE_COLOUR	= 8;
	static const unsigned SIZEOF_MICROMORPHS	= 8;
	static const unsigned SIZEOF_EYECOLOR		= 9;
	static const unsigned SIZEOF_HAIRTINTINDEX  = 8;
	static const unsigned SIZEOF_OVERLAY_TINTINDEX = 8;
	static const unsigned SIZEOF_RAMPTYPE		= datBitsNeeded<RT_MAX>::COUNT;
	static float DEFAULT_MICROMORPH_VALUE		= 0.f;

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_headBlend, 1.05f, SIZEOF_BLEND_RATIO, "Head Blend");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_texBlend,  1.05f, SIZEOF_BLEND_RATIO, "Tex Blend");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_varBlend,  1.05f, SIZEOF_BLEND_RATIO, "Var Blend");

	SERIALISE_INTEGER(serialiser, m_eyeColor, SIZEOF_EYECOLOR, "Eye Color");
	SERIALISE_UNSIGNED(serialiser, m_hairTintIndex, SIZEOF_HAIRTINTINDEX, "Hair Tint Index");
	SERIALISE_UNSIGNED(serialiser, m_hairTintIndex2, SIZEOF_HAIRTINTINDEX, "Hair Tint Index2");

	u32 headData[MAX_HEAD_BLEND_DATA];
	u32 texData[MAX_HEAD_BLEND_DATA];

	headData[0] = (u32)m_head0;
	headData[1] = (u32)m_head1;
	headData[2] = (u32)m_head2;

	texData[0] = (u32)m_tex0;
	texData[1] = (u32)m_tex1;
	texData[2] = (u32)m_tex2;

	//------------------------------------------
	u32 headDataFlags = 0;

	for (u32 i=0; i<MAX_HEAD_BLEND_DATA; i++)
	{
		if (headData[i] != 0 && headData[i] != 255)
		{
			headDataFlags |= BIT(i);
		}
	}

	bool hasHeadData = headDataFlags != 0;

	SERIALISE_BOOL(serialiser, hasHeadData,"hasHeadData");

	if (hasHeadData || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, headDataFlags, MAX_HEAD_BLEND_DATA, "headDataFlags");

		for (u32 i=0; i<MAX_HEAD_BLEND_DATA; i++)
		{
			if ((headDataFlags & BIT(i)) || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, headData[i], SIZEOF_HEAD_DATA, "Head Geometry");
			}
			else
			{
				headData[i] = 0;
			}
		}
	}
	else
	{
		for (u32 i=0; i<MAX_HEAD_BLEND_DATA; i++)
		{
			headData[i] = 0;
		}
	}

	//------------------------------------------
	//separate out the tex data from the head data - as the head data can be set to all zeroed - but we still want the texture data sent - happens when using masks and heads are shrunk/zeroed - textures are retained. lavalley
	u32 texDataFlags = 0;

	for (u32 i=0; i<MAX_TEX_BLEND_DATA; i++)
	{
		if (texData[i] != 0 && texData[i] != 255)
		{
			texDataFlags |= BIT(i);
		}
	}

	bool hasTexData = texDataFlags != 0;

	SERIALISE_BOOL(serialiser, hasTexData,"hasTexData");

	if (hasTexData || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, texDataFlags, MAX_TEX_BLEND_DATA, "texDataFlags");

		for (u32 i=0; i<MAX_TEX_BLEND_DATA; i++)
		{
			if ((texDataFlags & BIT(i)) || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, texData[i], SIZEOF_TEX_DATA, "Head Texture");
			}
			else
			{
				texData[i] = 0;
			}
		}
	}
	else
	{
		for (u32 i=0; i<MAX_TEX_BLEND_DATA; i++)
		{
			texData[i] = 0;
		}
	}

	//------------------------------------------

	m_head0 = (u8)headData[0];
	m_head1 = (u8)headData[1];
	m_head2 = (u8)headData[2];

	m_tex0 = (u8)texData[0];
	m_tex1 = (u8)texData[1];
	m_tex2 = (u8)texData[2];

	u32 overlayTexFlags = 0;

	for (u32 i=0; i<HOS_MAX; i++)
	{
		if (m_overlayTex[i] != 255)
		{
			overlayTexFlags |= BIT(i);
		}
	}

	bool hasOverlayTextures = overlayTexFlags != 0;

	SERIALISE_BOOL(serialiser, hasOverlayTextures,"hasOverlayTextures");

	if (hasOverlayTextures || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, overlayTexFlags, HOS_MAX, "overlayTexFlags");

		for (u32 i=0; i<HOS_MAX; i++)
		{
			if ((overlayTexFlags & BIT(i)) || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_UNSIGNED(serialiser, m_overlayTex[i], SIZEOF_OVERLAY_TEX, "Overlay Texture");
				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_overlayAlpha[i],1.0f,SIZEOF_OVERLAY_ALPHA, "Overlay Alpha");
				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_overlayNormAlpha[i],1.0f,SIZEOF_OVERLAY_NORMAL, "Overlay Normal");

				bool bHasTintIndexes = ((m_overlayTintIndex[i] != 0) || (m_overlayTintIndex2[i] != 0) || (m_overlayRampType != 0));
				SERIALISE_BOOL(serialiser, bHasTintIndexes,"bHasTintIndexes");
				if (bHasTintIndexes || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_UNSIGNED(serialiser, m_overlayTintIndex[i], SIZEOF_OVERLAY_TINTINDEX, "Overlay Tint Index");
					SERIALISE_UNSIGNED(serialiser, m_overlayTintIndex2[i], SIZEOF_OVERLAY_TINTINDEX, "Overlay Tint Index 2");
					SERIALISE_UNSIGNED(serialiser, m_overlayRampType[i], SIZEOF_RAMPTYPE, "Ramp Type");
				}
				else
				{
					m_overlayTintIndex[i] = 0;
					m_overlayTintIndex2[i] = 0;
					m_overlayRampType[i] = 0;
				}
			}
			else
			{
				m_overlayTex[i] = 255;
				m_overlayAlpha[i] = 0.0f;
				m_overlayNormAlpha[i] = 0.0f;
				m_overlayTintIndex[i] = 0;
				m_overlayTintIndex2[i] = 0;
				m_overlayRampType[i] = 0;
			}
		}
	}
	else
	{
		for (u32 i=0; i<HOS_MAX; i++)
		{
			m_overlayTex[i] = 255;
			m_overlayAlpha[i] = 0.0f;
			m_overlayNormAlpha[i] = 0.0f;
			m_overlayTintIndex[i] = 0;
			m_overlayTintIndex2[i] = 0;
			m_overlayRampType[i] = 0;
		}
	}

	//-----

	u32 micromorphFlags = 0;

	for (u32 i=0; i<MMT_MAX; i++)
	{
		if (m_microMorphBlends[i] != DEFAULT_MICROMORPH_VALUE)
		{
			micromorphFlags |= BIT(i);
		}
	}

	bool hasMicroMorphs = micromorphFlags != 0;

	SERIALISE_BOOL(serialiser, hasMicroMorphs,"hasMicroMorphs");

	if (hasMicroMorphs || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, micromorphFlags, MMT_MAX, "micromorphFlags");

		for (u32 i=0; i<MMT_MAX; i++)
		{
			if ((micromorphFlags & BIT(i)) || serialiser.GetIsMaximumSizeSerialiser())
			{
				if (i < MMT_NUM_PAIRS)
				{
					SERIALISE_PACKED_FLOAT(serialiser, m_microMorphBlends[i],1.0f,SIZEOF_MICROMORPHS,"MicroMorph Blend i<MMT_NUM_PAIRS"); //serialise values [-1,1] -- see PedVariationStream.cpp -- CPedVariationStream::RequestStreamPedFilesInternal -- reference: "deal with micro morph pairs"... 
				}
				else
				{
					SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_microMorphBlends[i],1.0f,SIZEOF_MICROMORPHS,"MicroMorph Blend i>=MMT_NUM_PAIRS"); //serialise values [0,1]  -- see PedVariationStream.cpp -- CPedVariationStream::RequestStreamPedFilesInternal -- reference: "deal with micro morph pairs"... 
				}
			}
			else
			{
				m_microMorphBlends[i] = DEFAULT_MICROMORPH_VALUE;
			}
		}
	}
	else
	{
		for (u32 i=0; i<MMT_MAX; i++)
		{
			m_microMorphBlends[i] = DEFAULT_MICROMORPH_VALUE;
		}
	}

	//-----

	SERIALISE_BOOL(serialiser, m_usePaletteRgb,"m_usePaletteRgb");

	if (m_usePaletteRgb || serialiser.GetIsMaximumSizeSerialiser())
	{
		for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
		{
			u32 red = (u32)m_paletteRed[i];
			u32 green = (u32)m_paletteGreen[i];
			u32 blue = (u32)m_paletteBlue[i];

			SERIALISE_UNSIGNED(serialiser, red, SIZEOF_PALETTE_COLOUR, "Palette Red");
			SERIALISE_UNSIGNED(serialiser, green, SIZEOF_PALETTE_COLOUR, "Palette Green");
			SERIALISE_UNSIGNED(serialiser, blue, SIZEOF_PALETTE_COLOUR, "Palette Blue");

			m_paletteRed[i]		= (u8)red;
			m_paletteGreen[i]	= (u8)green;
			m_paletteBlue[i]	= (u8)blue;
		}
	}
	else
	{
		for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
		{
			m_paletteRed[i] = 0;
			m_paletteGreen[i] = 0;
			m_paletteBlue[i] = 0;
		}
	}

	SERIALISE_BOOL(serialiser, m_hasParents, "m_hasParents");
	if (m_hasParents || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_extraParentData.head0Parent0, SIZEOF_HEAD_DATA, "Head0 Parent0");
		SERIALISE_UNSIGNED(serialiser, m_extraParentData.head0Parent1, SIZEOF_HEAD_DATA, "Head0 Parent1");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_extraParentData.parentBlend, 1.0f, SIZEOF_BLEND_RATIO, "Parent Blend");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_parentBlend2, 1.0f, SIZEOF_BLEND_RATIO, "Parent Blend2");
	}
	else
	{
		m_extraParentData.head0Parent0 = 0;
		m_extraParentData.head0Parent1 = 1;
		m_extraParentData.parentBlend  = 0.f;
		m_parentBlend2 = 0.f;
	}
}

CPedHeadBlendData::CPedHeadBlendData() :
m_blendHandle(MESHBLEND_HANDLE_INVALID)
{
	Reset();
}

CPedHeadBlendData::CPedHeadBlendData(const CPedHeadBlendData& other) :
m_blendHandle(MESHBLEND_HANDLE_INVALID)
{
	*this = other;
}

CPedHeadBlendData::~CPedHeadBlendData()
{
	pedDebugf1("CPedHeadBlendData::~CPedHeadBlendData(): Releasing handle 0x%08x", m_blendHandle);
	MESHBLENDMANAGER.ReleaseBlend(m_blendHandle);
    m_parentPed0 = NULL;
    m_parentPed1 = NULL;
}

CPedHeadBlendData& CPedHeadBlendData::operator=(const CPedHeadBlendData& other)
{
	Reset();

	m_headBlend = other.m_headBlend;
	m_texBlend  = other.m_texBlend;
	m_varBlend  = other.m_varBlend;
	m_head0     = other.m_head0;
	m_head1     = other.m_head1;
	m_head2     = other.m_head2;
	m_tex0      = other.m_tex0;
	m_tex1      = other.m_tex1;
	m_tex2      = other.m_tex2;

	m_eyeColor  = other.m_eyeColor;
	m_hairTintIndex = other.m_hairTintIndex;
	m_hairTintIndex2 = other.m_hairTintIndex2;

	for (s32 i = 0; i < HOS_MAX; ++i)
	{
		m_overlayTex[i] = other.m_overlayTex[i];
		m_overlayAlpha[i] = other.m_overlayAlpha[i];
		m_overlayNormAlpha[i] = other.m_overlayNormAlpha[i];
		m_overlayTintIndex[i] = other.m_overlayTintIndex[i];
		m_overlayTintIndex2[i] = other.m_overlayTintIndex2[i];
		m_overlayRampType[i] = other.m_overlayRampType[i];
	}

	for (s32 i = 0; i < MMT_MAX; ++i)
	{
		m_microMorphBlends[i] = other.m_microMorphBlends[i];
	}

	m_usePaletteRgb = other.m_usePaletteRgb;

	if (m_usePaletteRgb)
	{
		for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
		{
			m_paletteRed[i]      = other.m_paletteRed[i];
			m_paletteGreen[i]    = other.m_paletteGreen[i];
			m_paletteBlue[i]     = other.m_paletteBlue[i];
		}
	}

	m_hasParents = other.m_hasParents;

	if (m_hasParents)
	{
		m_extraParentData.head0Parent0 = other.m_extraParentData.head0Parent0;
		m_extraParentData.head0Parent1 = other.m_extraParentData.head0Parent1;
		m_extraParentData.parentBlend  = other.m_extraParentData.parentBlend;
		m_parentBlend2 = other.m_parentBlend2;
	}

	return *this;
}

bool CPedHeadBlendData::HasDuplicateHeadData(const CPedHeadBlendData& other) const
{
	if (m_headBlend != other.m_headBlend || 
		m_texBlend  != other.m_texBlend || 
		m_varBlend  != other.m_varBlend || 
		m_head0     != other.m_head0 || 
		m_head1     != other.m_head1 || 
		m_head2     != other.m_head2 || 
		m_tex0      != other.m_tex0 || 
		m_tex1      != other.m_tex1 || 
		m_tex2      != other.m_tex2 || 
		m_eyeColor  != other.m_eyeColor ||
		m_hairTintIndex != other.m_hairTintIndex ||
		m_hairTintIndex2 != other.m_hairTintIndex2)
	{
		return false;
	}

	if (memcmp(m_overlayAlpha, other.m_overlayAlpha, sizeof(m_overlayAlpha)) != 0 ||
		memcmp(m_overlayNormAlpha, other.m_overlayNormAlpha, sizeof(m_overlayNormAlpha)) != 0 ||
		memcmp(m_microMorphBlends, other.m_microMorphBlends, sizeof(m_microMorphBlends)) != 0 ||
		memcmp(m_overlayTintIndex, other.m_overlayTintIndex, sizeof(m_overlayTintIndex)) != 0 ||
		memcmp(m_overlayTintIndex2, other.m_overlayTintIndex2, sizeof(m_overlayTintIndex2)) != 0 ||
		memcmp(m_overlayRampType, other.m_overlayRampType, sizeof(m_overlayRampType)) != 0 ||
		memcmp(m_overlayTex, other.m_overlayTex, sizeof(m_overlayTex)) != 0)
	{
		return false;
	}

	if (m_usePaletteRgb != other.m_usePaletteRgb)
	{
		return false;
	}

	if (m_usePaletteRgb)
	{
		if (memcmp(m_paletteRed, other.m_paletteRed, sizeof(m_paletteRed)) != 0 ||
			memcmp(m_paletteGreen, other.m_paletteGreen, sizeof(m_paletteGreen)) != 0 ||
			memcmp(m_paletteBlue, other.m_paletteBlue, sizeof(m_paletteBlue)) != 0)
		{
			return false;
		}
	}

	if (m_hasParents != other.m_hasParents)
	{
		return false;
	}

	if (m_hasParents)
	{
		if (m_extraParentData.head0Parent0 != other.m_extraParentData.head0Parent0 || 
			m_extraParentData.head0Parent1 != other.m_extraParentData.head0Parent1 || 
			m_extraParentData.parentBlend != other.m_extraParentData.parentBlend || 
			m_parentBlend2 != other.m_parentBlend2)
		{
			return false;
		}
	}

	return true;
}


void CPedHeadBlendData::Reset()
{
	m_hasParents = false;
	m_isParent = true;
	m_needsFinalizing = false;

	m_headBlend = 0.f;
	m_texBlend = 0.f;
	m_varBlend = 0.f;
	m_parentBlend2 = 0.f;
	m_head0 = m_head1 = m_head2 = 255;
	m_tex0 = m_tex1 = m_tex2 = 255;

    m_eyeColor = -1;
	m_hairTintIndex = 0;
	m_hairTintIndex2 = 0;

	m_async = false;

	for (s32 i = 0; i < HOS_MAX; ++i)
	{
		m_overlayTex[i] = 255;
		m_overlayAlpha[i] = 1.f;
		m_overlayNormAlpha[i] = 1.f;
		m_overlayTintIndex[i] = 0;
		m_overlayTintIndex2[i] = 0;
		m_overlayRampType[i] = RT_NONE;
	}

	for (s32 i = 0; i < MMT_MAX; ++i)
	{
		m_microMorphBlends[i] = 0.f;
	}

	//setup initial values for palette here
	m_usePaletteRgb = false;
	for (u8 i = 0; i < MAX_PALETTE_COLORS; ++i)
	{
		m_paletteRed[i] = 0;
		m_paletteGreen[i] = 0;
		m_paletteBlue[i] = 0;
	}
}

void CPedHeadBlendData::UpdateFromParents()
{
    if (!m_parentPed0 || !m_parentPed1)
        return;

	CPedHeadBlendData* blendData0 = m_parentPed0->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData0)
        return;

	CPedHeadBlendData* blendData1 = m_parentPed1->GetExtensionList().GetExtension<CPedHeadBlendData>();
	if (!blendData1)
        return;

    // this function should be called whenever a parent's blend has changed in order for this child to copy the right values
    m_extraParentData.parentBlend = blendData0->m_headBlend;
    m_extraParentData.head0Parent0 = blendData0->m_head0;
    m_extraParentData.head0Parent1 = blendData1->m_head0;

    m_parentBlend2 = blendData1->m_headBlend;
    m_head0 = blendData0->m_head1;
    m_head1 = blendData1->m_head1;

	// select child textures based on parent blends, we assume the grandparents are same race so we pick the
	// texture each parent blend favors
	if (blendData0->m_texBlend < 0.5f)
		m_tex0 = blendData0->m_tex0;
	else
		m_tex0 = blendData0->m_tex1;

	if (blendData1->m_texBlend < 0.5f)
		m_tex1 = blendData1->m_tex0;
	else
		m_tex1 = blendData1->m_tex1;

	// make sure the 3rd genetic variation blend isn't used here
	m_head2 = 0;
	m_tex2 = 0;
	m_varBlend = 0.f;
}

//---------------PedVariationInfo-----------------

CPedVariationInfo::CPedVariationInfo(void)
{
	m_seqHeadTexIdx = 0;
	m_seqHeadDrawblIdx = 0;

	memset(m_availComp, 0xFF, PV_MAX_COMP);
}

#if !__FINAL
void CPedVariationInfo::UpdateAndSave(const char* modelName)
{
	char path[256] = {0};
	formatf(path, "X:/gta5/assets/metadata/characters/%s.pso", modelName);
	Displayf("Saving meta data to %s\n", path);
	AssertVerify(PARSER.SaveObject(path, "meta", this, parManager::XML));
}
#endif

CPedVariationInfo::~CPedVariationInfo(void)
{
}

void CPedVariationInfoCollection::EnumCloth( CPedModelInfo* pModelInfo )
{
	Assert( pModelInfo );

	strLocalIndex dictSlotId = strLocalIndex(g_ClothStore.FindSlotFromHashKey(pModelInfo->GetModelNameHash()));
	if( dictSlotId == -1 )
		return;					// there is no cloth for this model

	fwCloth* clothdict = g_ClothStore.Get( dictSlotId );
	AssertMsg(clothdict,"Streaming in of the desired character cloth into the dictionary failed!");

	if( !clothdict )
		return;

	// need to generate and test model names & determine if cloth for those are in the Cloth dictionary
	for (s32 infoIdx = 0; infoIdx < m_infos.GetCount(); ++infoIdx)
	{
		CPedVariationInfo* info = m_infos[infoIdx];
		if (!Verifyf(info, "NULL ped variation info on ped '%s' %d/%d", pModelInfo->GetModelName(), infoIdx, m_infos.GetCount()))
			continue;

		for( u32 i=0; i < PV_MAX_COMP; ++i ) 
		{
			for( u32 j = 0; j < MAX_DRAWBL_VARS; ++j) 
			{
				CPVComponentData* pCompData = info->GetCompData(i);
				if( !pCompData )
				{				
					j = MAX_DRAWBL_VARS;		// if component not valid, then skip to the next one.
					continue;
				}

				CPVDrawblData* drawabl = pCompData->GetDrawbl(j);
				if (!drawabl)
				{				
					j = MAX_DRAWBL_VARS;		// if we have no drawable, skip to next one
					continue;
				}

				characterCloth* compCloth = NULL;

				char variationName[32];
				sprintf(variationName,"%s_%03d_U",varSlotNames[i],j); // if it doesn't (assuming it's named properly in max)
				compCloth = static_cast<characterCloth*>( clothdict->Lookup( variationName ));

				if( !compCloth )
				{
					sprintf(variationName,"%s_%03d_R",varSlotNames[i],j); // if the drawable uses race variation textures
					compCloth = static_cast<characterCloth*>( clothdict->Lookup( variationName ));

					if( !compCloth )
					{
						sprintf(variationName,"%s_%03d_M",varSlotNames[i],j); // if the drawable requires texture matching
						compCloth = static_cast<characterCloth*>( clothdict->Lookup( variationName ));
					}
				}

				if( compCloth )
				{
					if( !drawabl->m_clothData.m_charCloth )
					{
						g_ClothStore.AddRef( dictSlotId, REF_OTHER );

						drawabl->m_clothData.m_charCloth = compCloth;
						drawabl->m_clothData.m_dictSlotId = dictSlotId.Get();
						drawabl->m_clothData.m_ownsCloth = true;
					}
					else
					{
						Assert( drawabl->m_clothData.HasCloth() );
						Assert( drawabl->m_clothData.m_dictSlotId != -1);
					}
				}
			}
		}
	}
}

// return race type info for a specific texture
ePVRaceType		CPedVariationInfo::GetRaceType(u32 compId, u32 drawblId, u32 texId, CPedModelInfo* ASSERT_ONLY(pModelInfo)) {

	CPVComponentData* pCompData = GetCompData(compId);
	if (Verifyf(pCompData, "Invalid component data for ped '%s' (%d, %d, %d). Continuing but peds might not look correctly.", pModelInfo->GetModelName(), compId, drawblId, texId)) {
		CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblId);
		Assert(pDrawblData);

		// temp fix for bug 3270
		// if any of the values are out of bounds then they are reset
		if (compId >= PV_MAX_COMP) { compId = 0; }
		if (drawblId >= pCompData->GetNumDrawbls()) { drawblId = 0; }
		if (texId >=  pDrawblData->GetNumTex()) { texId = 0;}

        if (!Verifyf(texId < pDrawblData->GetNumTex(), "Missing texture %s_%03d_%c for ped '%s'", varSlotNames[compId], drawblId, 'a' + texId, pModelInfo->GetModelName()))
            return PV_RACE_UNIVERSAL;

		Assertf(pDrawblData->GetTexData(texId) != (u8)PV_RACE_INVALID, "Invalid race for %s_%03d_%c (%s)", varSlotNames[compId], drawblId, 'a' + texId, pModelInfo->GetModelName());
		return((ePVRaceType)pDrawblData->GetTexData(texId));
	}
	else
		return PV_RACE_UNIVERSAL;	// better than crashing!
}

// return race type info for a specific texture
bool		CPedVariationInfo::IsMatchComponent(u32 compId, u32 drawblId, CPedModelInfo* ASSERT_ONLY(pModelInfo)) {

	CPVComponentData* pCompData = GetCompData(compId);
	if (Verifyf(pCompData, "Invalid component data for ped '%s' (%d, %d). Continuing but peds might not look correctly.", pModelInfo->GetModelName(), compId, drawblId)) {
		CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblId);
		Assertf(pDrawblData, "Invalid component data for ped '%s' (%d, %d). Continuing but peds might not look correctly.", pModelInfo->GetModelName(), compId, drawblId);

		// temp fix for bug 3270
		// if any of the values are out of bounds then they are reset
		if (compId >= PV_MAX_COMP) { compId = 0; }
		if (drawblId >= pCompData->GetNumDrawbls()) { drawblId = 0; }

		if (pDrawblData)
			return(pDrawblData->IsMatchingPrev());
		else
			return false;
	}
	else
		return false;
}

u8	CPedVariationInfo::GetMaxNumTex(ePedVarComp compType, u32 drawblIdx) const {
	const CPVComponentData* pCompData = GetCompData(compType);

	if (pCompData){
		const CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblIdx);
		if(pDrawblData){
			return((u8) pDrawblData->GetNumTex());
		}
	}

	return 0;
}

u8	CPedVariationInfo::GetMaxNumDrawbls(ePedVarComp compType) const {
	const CPVComponentData* pCompData = GetCompData(compType);

	if (pCompData){
		return((u8)GetCompData(compType)->GetNumDrawbls());
	} else {
		return 0;
	}
}

u8	CPedVariationInfo::GetMaxNumDrawblAlts(ePedVarComp compType, u32 drawblIdx) const {
	const CPVComponentData* pCompData = GetCompData(compType);

	if (pCompData){
		const CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblIdx);
		if(pDrawblData){
			return(pDrawblData->GetNumAlternatives());
		}
	}
	
	return 0;
}

bool	CPedVariationInfo::HasRaceTextures(u32 compId, u32 drawblId) {
	CPVComponentData* pCompData = GetCompData(compId);

	if (pCompData){
		CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblId);
		Assert(pDrawblData);
		return(pDrawblData->IsUsingRaceTex());
	} else {
		Assert(false);
		return (false);
	}
}

bool	CPedVariationInfo::HasPaletteTextures(u32 compId, u32 drawblId) {
	CPVComponentData* pCompData = GetCompData(compId);

	if (pCompData){
		CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblId);
		if (Verifyf(pDrawblData, "No drawable data for component %d and drawable %d", compId, drawblId))
			return(pDrawblData->HasPalette());
		else
			return false;
	} else {
		Assert(false);
		return (false);
	}
}

bool	CPedVariationInfo::IsValidDrawbl(u32 compId, u32 drawblId) {
	CPVComponentData* pCompData = GetCompData(compId);

	if (pCompData){
		CPVDrawblData* pDrawblData = pCompData->GetDrawbl(drawblId);
		if (!pDrawblData){
			return(false);
		}
		return(pDrawblData->HasActiveDrawbl());
	} else {
		return(false);
	}
}

CCustomShaderEffectBaseType* CPedVariationInfoCollection::GetShaderEffectType(Dwd* pDwd, CPedModelInfo* pModelInfo)
{
#if __DEV
	char modelName[24];
#endif //__DEV
	char R_ModelName[24];
	char U_ModelName[24];
	char M_ModelName[24];

	rmcDrawable* pDrawable = NULL;
	CCustomShaderEffectBaseType* pShaderEffectType = NULL;
	bool bIsSuperLod = (pModelInfo && CPed::IsSuperLodFromHash(pModelInfo->GetHashKey()));

	// if no component file was loaded for this ped...
	if (!pDwd)
	{
		return(pShaderEffectType);
	}

	// reset
	m_bHasLowLODs = true;				// assume it has low LODs until we find out otherwise

	for (s32 infoIdx = 0; infoIdx < m_infos.GetCount(); ++infoIdx)
	{
		CPedVariationInfo* info = m_infos[infoIdx];
		if (!Verifyf(info, "NULL ped variation info on ped '%s' %d/%d", pModelInfo->GetModelName(), infoIdx, m_infos.GetCount()))
			continue;

		for (s32 i = 0; i < PV_MAX_COMP; ++i)
		{
			CPVComponentData* pCompData = info->GetCompData(i);
			if (!pCompData)
			{
				continue;
			}

			for (s32 f = 0; f < pCompData->m_aDrawblData3.GetCount(); ++f)
			{
				// if the model name is in the dictionary then store this index for the drawable
				sprintf(U_ModelName,"%s_%03d_U",varSlotNames[i],f); // if it doesn't (assuming it's named properly in max)
				if ((pDrawable = pDwd->Lookup(U_ModelName)) != NULL)
				{
					artAssertf(pCompData->m_aDrawblData3[f].IsUsingRaceTex() == false, "Most likely bad metadata for asset '%s' for ped '%s'", U_ModelName, pModelInfo->GetModelName());
				}
				else {
					sprintf(R_ModelName,"%s_%03d_R",varSlotNames[i],f); // if the drawable uses race variation textures
					if ((pDrawable = pDwd->Lookup(R_ModelName)) != NULL){
						artAssertf(pCompData->m_aDrawblData3[f].IsUsingRaceTex() == true, "Most likely bad metadata for asset '%s' for ped '%s'", R_ModelName, pModelInfo->GetModelName());
					}
					else {
						sprintf(M_ModelName,"%s_%03d_M",varSlotNames[i],f); // if the drawable requires texture matching
						if ((pDrawable = pDwd->Lookup(M_ModelName)) != NULL){
							artAssertf(pCompData->m_aDrawblData3[f].IsMatchingPrev() == true, "Most likely bad metadata for asset '%s' for ped '%s'", M_ModelName, pModelInfo->GetModelName());
						}  else {

#if __DEV
							sprintf(modelName,"%s_%02d",varSlotNames[i],f);		//old stylee
							if ((pDrawable = pDwd->Lookup(modelName)) != NULL){
								Assertf(false, "%s in ped %s is no longer a supported component name\n",  modelName, pModelInfo->GetModelName());
							}
#endif //__DEV

							// else assume no more models for this slot & so bail and go on to the next name
							break;
						}
					}
				}


				// if drawable does not contain an LOD for the base components then invalidate the low LOD flag
				if ( i == PV_COMP_HEAD ){
					if (!pDrawable->GetLodGroup().ContainsLod(1)){
						m_bHasLowLODs = false;
					}
				}

				// if drawable has alpha (uses odd shader buckets) in it set it to drawlast
				pDrawable->GetLodGroup().ComputeBucketMask(pDrawable->GetShaderGroup());

				// superlod only contains 1 element & doesn't have shaders, so bail out now.
				if (bIsSuperLod)
				{
					i = PV_MAX_COMP;
					f = pCompData->m_aDrawblData3.GetCount();
					break;
				}

				pShaderEffectType = CCustomShaderEffectPed::SetupForDrawableComponent(pDrawable, i, pShaderEffectType);

#if __DEV
				if (!pShaderEffectType || !pShaderEffectType->AreShadersValid(pDrawable))
				{
					extern const char* varSlotNames[];
					Assertf(false, "[Art] Invalid shaders in ped: %s (%s) (component: %s%d).", pModelInfo ? pModelInfo->GetModelName() : "", modelName, varSlotNames[i], f);
				}
#endif //__DEV
				Assert(pShaderEffectType);
			}
		}

		// enough data has been setup for the superlod that we can bail now.
		if (bIsSuperLod)
		{
			m_bIsSuperLOD = true;
			m_bHasLowLODs = false;
			return(NULL);
		}
    }

	return(pShaderEffectType);
}

atHashString CPedVariationInfo::LookupCompInfoAudioID(s32 slotId, s32 drawblId, bool second) const {
	for (u32 i = 0; i < m_compInfos.GetCount(); ++i)
	{
		if ((m_compInfos[i].pedXml_compIdx == slotId) && (m_compInfos[i].pedXml_drawblIdx == drawblId))
		{
			return second ? m_compInfos[i].pedXml_audioID2 : m_compInfos[i].pedXml_audioID;
		}
	}

	return atHashString::Null();
}

u32	CPedVariationInfo::LookupCompInfoFlags(s32 slotId, s32 drawblId) const {
	for (u32 i = 0; i < m_compInfos.GetCount(); ++i)
	{
		if ((m_compInfos[i].pedXml_compIdx == slotId) && (m_compInfos[i].pedXml_drawblIdx == drawblId))
		{
			if (m_compInfos[i].flags != 0)
				return m_compInfos[i].flags;
			else
				return m_compInfos[i].pedXml_flags;
		}
	}

	return 0;
}

u32	CPedVariationInfo::LookupSecondaryFlags(s32 slotId, s32 drawblId, s32 targetSlot) const {
    for (u32 i = 0; i < m_compInfos.GetCount(); ++i)
    {
        if ((m_compInfos[i].pedXml_compIdx == slotId) && (m_compInfos[i].pedXml_drawblIdx == drawblId))
        {
            if (m_compInfos[i].pedXml_vfxComps.IsSet(targetSlot))
            {
				if (m_compInfos[i].flags != 0)
					return m_compInfos[i].flags;
				else
					return m_compInfos[i].pedXml_flags;
            }
        }
    }

    return 0;
}

const CPVComponentData* CPedVariationInfo::GetAvailCompData(u32 compId) const {
	if (compId >= PV_MAX_COMP)
	{
		return NULL;
	}

	if (m_availComp[compId] == 0xFF)
	{
		return NULL;
	}

	if (Verifyf(m_availComp[compId] < m_aComponentData3.GetCount(), "Invalid ped meta data, requested component %d of %d for slot %d!", m_availComp[compId]+1, m_aComponentData3.GetCount(), compId))
	{
		return &m_aComponentData3[m_availComp[compId]];
	}

	return NULL;
}

const atFixedBitSet<32, u32>* CPedVariationInfo::GetInclusions(u32 compId, u32 drawblId) const {
	if (compId >= PV_MAX_COMP)
	{
		return NULL;
	}

	for (u32 i = 0; i < m_compInfos.GetCount(); ++i)
	{
		if (m_compInfos[i].pedXml_compIdx == compId && m_compInfos[i].pedXml_drawblIdx == drawblId)
		{
			return &m_compInfos[i].inclusions;
		}
	}

	return NULL;
}

const atFixedBitSet<32, u32>* CPedVariationInfo::GetExclusions(u32 compId, u32 drawblId) const {
	if (compId >= PV_MAX_COMP)
	{
		return NULL;
	}

	for (u32 i = 0; i < m_compInfos.GetCount(); ++i)
	{
		if (m_compInfos[i].pedXml_compIdx == compId && m_compInfos[i].pedXml_drawblIdx == drawblId)
		{
			return &m_compInfos[i].exclusions;
		}
	}

	return NULL;
}

// PURPOSE: Helper function for checking if any of the restrictions in the given array
//			restrict a specific drawable being used for a certain component.
bool CPedVariationInfo::IsComponentRestricted(ePedVarComp compId, int drawableId, const CPedCompRestriction* pRestrictions, int numRestrictions)
{
	for(int j = 0; j < numRestrictions; j++)
	{
		if(pRestrictions[j].IsRestricted(compId, drawableId))
		{
			return true;
		}
	}
	return false;
}

// PURPOSE: Helper function to retrieve a valid palette id for a component
u32 CPedVariationInfoCollection::GetPaletteId(u32 compId, s32 drawable, s32& texture, CPedModelInfo* pModelInfo, s32& forceTexMatchId, s32& forcePaletteId)
{
	// initialize
	u32 paletteId = 0;

	// if we are drawable
	if (drawable != PV_NULL_DRAWBL)
	{
		// randomize palette id if available
		if(HasPaletteTextures(compId, (u32)drawable))
		{
			paletteId = fwRandom::GetRandomNumberInRange(0, MAX_PALETTES);
		}

		// match components have to use the same texture id as the previous component (so LOWR texture matches UPPR texture for suits etc.)
		if (IsMatchComponent(static_cast<ePedVarComp>(compId), (u32)drawable, pModelInfo))
		{
			if (forceTexMatchId == -1)
			{
				forceTexMatchId = texture;
				forcePaletteId = paletteId;
			}
			else
			{
				texture = forceTexMatchId;

				//only force a match if the lower drawable has a palette. 
				if (HasPaletteTextures(compId, (u32)drawable))
				{	
					paletteId = forcePaletteId;
				}

				const s32 maxTextures = GetMaxNumTex(static_cast<ePedVarComp>(compId), (u32)drawable);
				if (texture >= maxTextures)
				{
					Assertf(texture < maxTextures, "Tried to force an invalid match for %s\n", pModelInfo->GetModelName());
					texture = 0;
				}
			}
		} 
	}

	return paletteId;
}

// PURPOSE: Helper function to update the component inclusion/exclusion sets
void CPedVariationInfoCollection::UpdateInclusionAndExclusionSets(const atFixedBitSet32* compInclusions, const atFixedBitSet32* compExclusions, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, atFixedBitSet32& inclusionsSet, atFixedBitSet32& exclusionsSet)
{
	if (compInclusions)
	{
		inclusionsSet.Union(*compInclusions);
	}
	if (outInclusions)
	{
		outInclusions->Union(inclusionsSet);
	}

	if (compExclusions && compExclusions->AreAnySet())
	{
		exclusionsSet.Union(*compExclusions);
		if (outExclusions)
		{
			outExclusions->Union(exclusionsSet);
		}
	}
}

// PURPOSE: Helper function to set MustUse component variations
void CPedVariationInfoCollection::SetRequiredComponentVariations(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData, CPedModelInfo* pModelInfo, const CPed** sameTypePeds, u32 numSameTypePeds, s32& forceTexMatchId, s32& forcePaletteId, atFixedBitSet32& inclusionsSet, atFixedBitSet32& exclusionsSet, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, ePVRaceType& race, const CPedCompRestriction* pRestrictions, int numRestrictions, u32 streamingFlags)
{
	// set required components
	for (u32 restrictionIndex = 0; restrictionIndex < numRestrictions; ++restrictionIndex)
	{
		// check if this is a MustUse component
		if (pRestrictions[restrictionIndex].GetRestriction() != CPedCompRestriction::MustUse)
		{
			// go to the next component, this is not a required component
			continue;
		}

		// get the component slot id
		u32 compId = pRestrictions[restrictionIndex].GetComponentSlot();

		// check the number of available textures
		const u32 numAvailTex = GetNumAvailTex(compId);
		if (numAvailTex == 0)
		{
			// go to the next component if there are no textures
			continue;
		}
		
		// get a sorted list of the textures in an ascending distribution rate order, i.e. top element occurs the least
		// and should therefore be prioritized for spawn.
		atArray<sVarSortedItem> offsets;
		offsets.Reserve(numAvailTex);
		SortTextures(pDynamicEntity, static_cast<ePedVarComp>(compId), sameTypePeds, numSameTypePeds, offsets);

		// get the drawable id
		u32 drawableId = pRestrictions[restrictionIndex].GetDrawableIndex();
		s32 overrideTextureId = pRestrictions[restrictionIndex].GetTextureIndex();

		bool bTriedLookingForOverrideTexture = false;
		for (u32 itemId = 0; itemId < offsets.GetCount(); ++itemId)
		{
			// if we have the right drawable index
			if (drawableId == offsets[itemId].drawblId)
			{
				// get our texture id
				s32 textureId = (s32)offsets[itemId].texId;

				// If override texture is defined
				if (overrideTextureId > -1 && !bTriedLookingForOverrideTexture)
				{
					bTriedLookingForOverrideTexture = true;
					if (overrideTextureId != textureId)
					{
						// Look through the rest of the items and see if a valid one with matching texture id exists
						for (u32 i = itemId + 1; i < offsets.GetCount(); i++)
						{
							if (drawableId == offsets[i].drawblId)
							{
								s32 nextTextureId = (s32)offsets[i].texId;
								if (nextTextureId == overrideTextureId)
								{
									textureId = overrideTextureId;
									break;
								}
							}
						}
					}
				}

				// If we can't find the override texture the override setup may be invalid, assert so this is easier to track
				if (overrideTextureId > -1 && textureId != overrideTextureId)
				{
					Assertf(false, "Specified overrideTextureId %d for drawableId %d on compId %d can't be found for ped '%s'", overrideTextureId, drawableId, compId, pModelInfo->GetModelName());
				}

				// check texture compatibility
				ePVRaceType overrideRace = GetRaceType(static_cast<ePedVarComp>(compId), drawableId, textureId, pModelInfo);
				if (!CPedVariationInfo::IsRaceCompatible(overrideRace, race))
				{
					// if we haven't found a matching head a bad race type was passed in which doesn't exist on the current ped.
					// assert but set to unviersal so we don't get a ped with a missing head
					if (compId == PV_COMP_HEAD && (s32)itemId == offsets.GetCount() - 1)
					{
						Assertf(false, "Specified race %d can't be found for ped '%s', drawableId - %i, textureId - %i", race, pModelInfo->GetModelName(), drawableId, textureId);
						race = PV_RACE_UNIVERSAL;
					}
					else
					{
						// Don't worry about incompatible races if the overrides are explicit (assume design is happy with the textures chosen)
						// Only continue if we don't have a texture override
						if (overrideTextureId == -1)
						{
							continue;
						}
					}
				}

				// update the sets
				const atFixedBitSet32* compInclusions = GetInclusions(compId, drawableId);
				const atFixedBitSet32* compExclusions = GetExclusions(compId, drawableId);
				UpdateInclusionAndExclusionSets(compInclusions, compExclusions, outInclusions, outExclusions, inclusionsSet, exclusionsSet);

				// get our palette id
				u32 paletteId = GetPaletteId(compId, drawableId, textureId, pModelInfo, forceTexMatchId, forcePaletteId); 

				// if we are streamed
				if (m_bIsStreamed)
				{
					// set our component variation
					CPedVariationStream::SetVariation(pDynamicEntity, pedPropData, pedVarData, static_cast<ePedVarComp>(compId), drawableId, 0, textureId, paletteId, streamingFlags, false);
				}
				else
				{
					// set our component variation
					CPedVariationPack::SetVariation(pDynamicEntity, pedPropData, pedVarData, static_cast<ePedVarComp>(compId), drawableId, 0, textureId, paletteId);
				}

				// go to the next component
				break;
			}
		}
	}
}

// set the variation by cycling through all heads and assigning other components randomly
void CPedVariationInfoCollection::SetVarRandom(CDynamicEntity* pDynamicEntity, CPedPropData& pedPropData, CPedVariationData& pedVarData, u32 UNUSED_PARAM(setRestrictionFlags), u32 clearRestrictionFlags, atFixedBitSet32* outInclusions, atFixedBitSet32* outExclusions, const CPedCompRestriction* pRestrictions, int numRestrictions, ePVRaceType race, u32 streamingFlags)
{
	Assert(pDynamicEntity);
	Assert(pDynamicEntity->GetIsTypePed());

	CPedModelInfo*	pModelInfo = static_cast<CPedModelInfo*>(pDynamicEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	if (m_bIsSuperLOD)
	{
		return;
	}

	// get an array of the same type peds and dummy peds in a range of 10 meters from this ped
	// so we can check for variation frequency locally
	const CPed* sameTypePeds[128];
	s32	numSameTypePeds = 0;
	if (!m_bIsStreamed)
	{
		numSameTypePeds = GetExistingPeds(pDynamicEntity, numSameTypePeds, sameTypePeds);
	}

	atFixedBitSet32 inclusionsSet;
	atFixedBitSet32 exclusionsSet;

	s32	forceTexMatchId = -1;
	s32 forcePaletteId = -1;

	CPed* pPed = static_cast<CPed*>(pDynamicEntity);
	for(u32 lod = 0; lod < LOD_COUNT; lod++)
	{
		pPed->SetLodBucketInfoExists(lod, false);
		pPed->SetLodHasAlpha(lod, false);
		pPed->SetLodHasDecal(lod, false);
		pPed->SetLodHasCutout(lod, false);
	}

	// set random component variations
	for (u32 compId = PV_COMP_HEAD; compId < PV_MAX_COMP; ++compId)
	{
		// check the number of available textures
		u32 numAvailTex = GetNumAvailTex(compId);
		if (numAvailTex == 0)
		{
			// go to the next component if there are no textures
			continue;
		}

		s32 drawable = -1;
		s32 potentialDrawable = -1;
		s32 potentialTexture = -1;
		s32 texture = -1;

		// get a sorted list of the textures in an ascending distribution rate order, i.e. top element occurs the least
		// and should therefore be prioritized for spawn.
		atArray<sVarSortedItem> offsets;
		offsets.Reserve(numAvailTex);
		SortTextures(pDynamicEntity, static_cast<ePedVarComp>(compId), sameTypePeds, numSameTypePeds, offsets);

		// try to avoid spawning identical assets within 10 meters of each other
		const float minClosestDistSqr = 10.f * 10.f; 

        const atFixedBitSet32* compInclusions = NULL;
        const atFixedBitSet32* compExclusions = NULL;
		
		// check if we have a drawable and texture override for the head component as this will affect the ped race type (don't want to generate incompatible randomised components)
		bool bOverrideRaceFromRequiredHeadComponent = false;
		if (compId == PV_COMP_HEAD)
		{
			for (u32 restrictionIndex = 0; restrictionIndex < numRestrictions; ++restrictionIndex)
			{
				// check if this is a MustUse component
				if (pRestrictions[restrictionIndex].GetRestriction() != CPedCompRestriction::MustUse)
				{
					// go to the next component, this is not a required component
					continue;
				}

				// get the component slot id
				u32 overrideCompId = pRestrictions[restrictionIndex].GetComponentSlot();

				if (overrideCompId == PV_COMP_HEAD)
				{
					u32 overrideDrawableId = pRestrictions[restrictionIndex].GetDrawableIndex();
					s32 overrideTextureId = pRestrictions[restrictionIndex].GetTextureIndex();

					s32 foundTextureId = -1;

					bool bDrawableFound = false;

					// currently we only check if there's also an override texture id set
					// this is to ensure this fix only applies to post CL#23578040 ped setup
					if (overrideTextureId > -1)
					{
						for (u32 itemId = 0; itemId < offsets.GetCount(); ++itemId)
						{
							// if we have the right drawable index
							if (overrideDrawableId == offsets[itemId].drawblId)
							{
								bDrawableFound = true;

								// get our texture id
								foundTextureId = (s32)offsets[itemId].texId;

								if (overrideTextureId != foundTextureId)
								{
									// Look through the rest of the items and see if a valid one with matching texture id exists
									for (u32 i = itemId + 1; i < offsets.GetCount(); i++)
									{
										if (overrideDrawableId == offsets[i].drawblId)
										{
											s32 nextTextureId = (s32)offsets[i].texId;
											if (nextTextureId == overrideTextureId)
											{
												foundTextureId = overrideTextureId;
												break;
											}
										}
									}
								}
								// go to the next component
								break;
							}
						}

						if (bDrawableFound)
						{
							if (foundTextureId > -1)
							{
								race = GetRaceType(static_cast<ePedVarComp>(overrideCompId), overrideDrawableId, foundTextureId, pModelInfo);
								bOverrideRaceFromRequiredHeadComponent = true;
							}
						}
					}

					// we only check head components so break
					break;
				}
			}
		}

		for (u32 itemId = 0; itemId < offsets.GetCount(); ++itemId)
		{
            compInclusions = NULL;
			compExclusions = NULL;

			// early skip if distribution is set to 0, which means drawable is never meant to be randomized in
			if (offsets[itemId].distribution == 0)
			{
				continue;
			}

			u32 drawableId = offsets[itemId].drawblId;
			u32 textureId = offsets[itemId].texId;
			Assert(textureId != (u8)PV_RACE_INVALID);

			if (!IsValidDrawbl(compId, drawableId))
			{
				continue;
			}

			// Check if this drawable isn't allowed for this component by the user.
			// Note: we may want to move this inside SortTextures()/RandomizeTextures(),
			// so we don't have to think about texture variations of drawables we are not
			// allowed to use anyway.
			if(CPedVariationInfo::IsComponentRestricted((ePedVarComp)compId, drawableId, pRestrictions, numRestrictions))
			{
				continue;
			}

            compInclusions = GetInclusions(compId, drawableId);
			compExclusions = GetExclusions(compId, drawableId);

			// check restriction flags
			u32 drawableFlags = LookupCompInfoFlags(compId, drawableId);
			if (drawableFlags != (u32)-1)
			{
				//if ((drawableFlags & setRestrictionFlags) != setRestrictionFlags)
				//{
				//	continue;
				//}

				if ((drawableFlags & clearRestrictionFlags) != PV_FLAG_NONE)
				{
					continue;
				}
			}

			// skip drawable if any of the exclusion ids DO match
			if (exclusionsSet.DoesOverlap(*compExclusions))
				continue;

			// check texture compatibility
			if (!CPedVariationInfo::IsRaceCompatible(GetRaceType(static_cast<ePedVarComp>(compId), drawableId, textureId, pModelInfo), race))
			{
				// if we haven't found a matching head a bad race type was passed in which doesn't exist on the current ped.
				// assert but set to unviersal so we don't get a ped with a missing head
				if (compId == PV_COMP_HEAD && (s32)itemId == offsets.GetCount() - 1)
				{
					Assertf(false, "Specified race %d can't be found for ped '%s', bOverrideRaceFromRequiredHeadComponent - %s, drawableId - %i, textureId - %i", race, pModelInfo->GetModelName(), bOverrideRaceFromRequiredHeadComponent ? "TRUE":"FALSE", drawableId, textureId);
					race = PV_RACE_UNIVERSAL;
				}
				else
					continue;
			}

			// skip drawable if inclusions don't match or we have the same assets too close already
			if ((inclusionsSet.AreAnySet() && !inclusionsSet.DoesOverlap(*compInclusions)) || (!inclusionsSet.AreAnySet() && offsets[itemId].shortestDistanceSqr < minClosestDistSqr))
			{
				// keep track of first drawable with mismatch in inclusion ids.
				// this should be used in case there are no matching drawables
				if (potentialDrawable == -1)
				{
					potentialDrawable = (s32)drawableId;
					potentialTexture = (s32)textureId;
				}

				continue;
			}

			// we have a compatible drawable and texture at this point
			drawable = (s32)drawableId;
			texture = (s32)textureId;

			// grow inclusion set if needed
			UpdateInclusionAndExclusionSets(compInclusions, compExclusions, outInclusions, outExclusions, inclusionsSet, exclusionsSet);

			break;
		}

		// if no drawable found, fall back to the potential one
		// if no potential drawables/textures are found, assert that this isn't happening for head, uppr or lowr
		if (drawable == -1)
		{
			drawable = potentialDrawable;
			texture = potentialTexture;

			if (drawable == -1 && compId != PV_COMP_HEAD)
			{
				drawable = PV_NULL_DRAWBL;
				texture = 0;
			}

			// grow inc/exc sets with the fallback drawable data
            compInclusions = GetInclusions(compId, drawable);
			compExclusions = GetExclusions(compId, drawable);
			UpdateInclusionAndExclusionSets(compInclusions, compExclusions, outInclusions, outExclusions, inclusionsSet, exclusionsSet);
		}

		u32 paletteId = GetPaletteId(compId, drawable, texture, pModelInfo, forceTexMatchId, forcePaletteId); 

		if (m_bIsStreamed)
		{
			CPedVariationStream::SetVariation(pDynamicEntity, pedPropData, pedVarData, static_cast<ePedVarComp>(compId), (u32)drawable, 0, (u32)texture, paletteId, streamingFlags, false);
		}
		else
		{
			CPedVariationPack::SetVariation(pDynamicEntity, pedPropData, pedVarData, static_cast<ePedVarComp>(compId), (u32)drawable, 0, (u32)texture, paletteId);
		}

		if (compId == PV_COMP_HEAD && !((CPed*)pDynamicEntity)->HasHeadBlend() && !bOverrideRaceFromRequiredHeadComponent)
		{
			race = GetRaceType(PV_COMP_HEAD, drawable, texture, pModelInfo);
		}
	}

	// set any required component variations
	SetRequiredComponentVariations(pDynamicEntity, pedPropData, pedVarData, pModelInfo, sameTypePeds, numSameTypePeds, forceTexMatchId, forcePaletteId, inclusionsSet, exclusionsSet, outInclusions, outExclusions, race, pRestrictions, numRestrictions, streamingFlags);

	CPedVariationPack::UpdatePedForAlphaComponents(pDynamicEntity, pedPropData, pedVarData);
}

s32 CPedVariationInfoCollection::GetExistingPeds( CDynamicEntity* pDynamicEntity, s32& numSameTypePeds, const CPed** sameTypePeds ) const
{
#if 1
	if (pDynamicEntity->GetIsTypePed())
	{
		CPed* ped = (CPed*)pDynamicEntity;
		CPedModelInfo* pmi = ped->GetPedModelInfo();
		Assert(pmi);

		for (s32 i = 0; i < pmi->GetNumPedInstances(); ++i)
		{
			CPed* pedInst = pmi->GetPedInstance(i);
			if (!pedInst || pedInst == ped || pedInst->GetIsInPopulationCache())
				continue;

			if (numSameTypePeds < 128)
				sameTypePeds[numSameTypePeds++] = pedInst;
		}
	}
#else
	// get an array of the same type peds and dummy peds in a range of 10 metres from this ped
	// so we can check for variation frequency locally
	Vector3 position = VEC3V_TO_VECTOR3(pDynamicEntity->GetTransform().GetPosition());
	CEntityIterator localEntityIterator(IteratePeds, pDynamicEntity, &position, 10.0f);
	const CPed* pPed = static_cast<CPed*>(localEntityIterator.GetNext());
	while(pPed)
	{
		if( pPed->GetModelIndex() == pDynamicEntity->GetModelIndex() )
		{
			if (numSameTypePeds < 128)
			{
				Assert(pPed->GetIsTypePed());
				sameTypePeds[numSameTypePeds] = pPed;
				numSameTypePeds++;
			}
		}
		pPed = static_cast<CPed*>(localEntityIterator.GetNext());
	}
	// If no hits are found locally, balance variations globally
	if( numSameTypePeds == 0 )
	{
		CEntityIterator globalEntityIterator(IteratePeds, pDynamicEntity);
		pPed = static_cast<CPed*>(globalEntityIterator.GetNext());
		while(pPed)
		{
			if( pPed->GetModelIndex() == pDynamicEntity->GetModelIndex() )
			{
				if (numSameTypePeds < 128)
				{
					Assert(pPed->GetIsTypePed());
					sameTypePeds[numSameTypePeds] = pPed;
					numSameTypePeds++;
				}
			}
			pPed = static_cast<CPed*>(globalEntityIterator.GetNext());
		}
	}
#endif

	return numSameTypePeds;
}

int CompareVarTextures(const sVarSortedItem* a, const sVarSortedItem* b)
{
#if 1
	int res = (int)b->occurrence - (int)a->occurrence;

	if (res == 0)
	{
		if (a->count == 0 && b->count == 0)
			return (b->useCount < a->useCount) ? 1 : -1;
		else
			return (b->shortestDistanceSqr > a->shortestDistanceSqr) ? 1 : -1;
	}

	return res;
#else
	return (b->shortestDistanceSqr > a->shortestDistanceSqr) ? 1 : -1;
#endif
}

int CompareVarTexturesUseCount(const sVarSortedItem* a, const sVarSortedItem* b)
{
	return (b->useCount < a->useCount) ? 1 : -1;
}

void CPedVariationInfoCollection::SortTextures(CDynamicEntity* pDynamicEntity, ePedVarComp comp, const CPed** peds, u32 numPeds, atArray<sVarSortedItem>& offsets)
{
	Assert(peds);
	Assert(pDynamicEntity->GetIsTypePed());
	CPed* ped = (CPed*)pDynamicEntity;

	// create and randomize initial array
	RandomizeTextures(comp, offsets, ped);

	if (numPeds == 0)
	{
		// sort based on texture use count, we want to spawn the least used ones first
		offsets.QSort(0, -1, CompareVarTexturesUseCount);
		return;
	}

	// gather occurrence count for every texture
	for (u32 i = 0; i < numPeds; ++i)
	{
		const CPedVariationData* varData = &peds[i]->GetPedDrawHandler().GetVarData();
		Assert( varData );
		u32 tex = varData->GetPedTexIdx(comp);
		u32 drawbl = varData->GetPedCompIdx(comp);

		const float distSqr = DistSquared(peds[i]->GetTransform().GetPosition(), ped->GetTransform().GetPosition()).Getf();

		// find texture and increment count
		for (u32 f = 0; f < offsets.GetCount(); ++f)
		{
			if (offsets[f].drawblId == drawbl && offsets[f].texId == tex)
			{
				offsets[f].count++;

				if (distSqr < offsets[f].shortestDistanceSqr)
					offsets[f].shortestDistanceSqr = distSqr;

				break;
			}
		}
	}

	// calculate occurrence ratio and store deviation from target distribution
	for (u32 i = 0; i < offsets.GetCount(); ++i)
	{
		float occ = offsets[i].count / (float)numPeds;
		float dev = (float)offsets[i].distribution - (occ * 255.f);
		offsets[i].occurrence = dev < 0.f ? 0 : (u8)dev;
	}

	// sort based on distribution deviation
	offsets.QSort(0, -1, CompareVarTextures);
}

void CPedVariationInfoCollection::RandomizeTextures(ePedVarComp comp, atArray<sVarSortedItem>& offsets, CPed* ASSERT_ONLY(ped))
{
	// init array with default values
	for (u32 i = 0; i < GetMaxNumDrawbls(comp); ++i)
	{
		for (u32 f = 0; f < GetMaxNumTex(comp, i); ++f)
		{
			sVarSortedItem item;
			item.texId = static_cast<u8>(f);
			item.drawblId = static_cast<u8>(i); 
			item.count = 0;
			item.distribution = GetTextureDistribution(comp, i, f);
			item.useCount = GetTextureUseCount(comp, i, f);
			item.occurrence = item.distribution;
			item.shortestDistanceSqr = 10000.f;
			if (Verifyf(offsets.GetCapacity() > offsets.GetCount(), "Num textures for %s for %s not correct in metadata?", varSlotNames[comp], ped->GetModelName()))
				offsets.Push(item);
		}
	}
	Assertf(offsets.GetCapacity() == offsets.GetCount(), "Num textures for %s for %s not correct in metadata?", varSlotNames[comp], ped->GetModelName());

	for (u32 i = 0; i < offsets.GetCount(); ++i)
	{
		u32 newIdx = fwRandom::GetRandomNumberInRange(0, offsets.GetCount());
		sVarSortedItem item = offsets[i];
		offsets[i] = offsets[newIdx];
		offsets[newIdx] = item;
	}
}

bool CPedVariationInfo::IsRaceCompatible(ePVRaceType typeA, ePVRaceType typeB)
{
	// races are compatible if they are the same, or if either one is universal
	if ((typeA != typeB) &&
		(typeA != PV_RACE_UNIVERSAL) &&
		(typeB != PV_RACE_UNIVERSAL))
	{
		return(false);			// non matching race types found
	}

	return(true);
}

u8 CPedVariationInfo::GetTextureDistribution(ePedVarComp compId, u32 drawableId, u32 texture) const
{
	if (compId < PV_COMP_HEAD || compId >= PV_MAX_COMP)
		return 255;

	const CPVComponentData* comp = GetCompData(compId);
	if (!comp || drawableId >= comp->GetNumDrawbls())
		return 255;

	const CPVDrawblData* drawable = comp->GetDrawbl(drawableId);
	if (!drawable || texture >= drawable->GetNumTex())
		return 255;

	return drawable->GetTexDistrib(texture);
}

u8 CPedVariationInfo::GetTextureUseCount(ePedVarComp compId, u32 drawableId, u32 texture) const
{
	if (compId < PV_COMP_HEAD || compId >= PV_MAX_COMP)
		return 255;

	const CPVComponentData* comp = GetCompData(compId);
	if (!comp || drawableId >= comp->GetNumDrawbls())
		return 255;

	const CPVDrawblData* drawable = comp->GetDrawbl(drawableId);
	if (!drawable || texture >= drawable->GetNumTex())
		return 255;

	return drawable->GetUseCount(texture);
}

u32 CPedVariationInfo::GetDlcNameHash() const
{
	return m_dlcName.GetHash();
}

const char* CPedVariationInfo::GetDlcName() const
{
	if (m_dlcName.GetHash() == 0)
		return NULL;

	return m_dlcName.TryGetCStr();
}

void CPedVariationInfo::RemoveFromStore()
{
	for (s32 i = 0; i < m_aComponentData3.GetCount(); ++i)
	{
		for (s32 f = 0; f < m_aComponentData3[i].m_aDrawblData3.GetCount(); ++f)
		{
			CPVDrawblData::CPVClothComponentData& clothData = m_aComponentData3[i].m_aDrawblData3[f].m_clothData;
			if (clothData.HasCloth() && clothData.m_dictSlotId != -1)
				g_ClothStore.RemoveRef(strLocalIndex(clothData.m_dictSlotId), REF_OTHER);

			clothData.Reset();
		}
	}
}

const float* CPedVariationInfo::GetExpressionMods(ePedVarComp comp, u32 drawblId) const
{
	for (u32 i = 0; i < m_compInfos.GetCount(); ++i)
	{
		if (m_compInfos[i].pedXml_compIdx == comp && m_compInfos[i].pedXml_drawblIdx == drawblId)
		{
			return m_compInfos[i].pedXml_expressionMods;
		}
	}

	return NULL;
}

const CPedSelectionSet* CPedVariationInfo::GetSelectionSetByHash(u32 hash) const
{
	for (s32 i = 0; i < m_aSelectionSets.GetCount(); ++i)
	{
		if (m_aSelectionSets[i].m_name.GetHash() == hash)
			return &m_aSelectionSets[i];
	}
	return NULL;
}

const CPedSelectionSet* CPedVariationInfo::GetSelectionSet(u32 index) const
{
	if (index < m_aSelectionSets.GetCount())
		return &m_aSelectionSets[index];
	return NULL;
}

bool CPedVariationInfo::HasComponentFlagsSet(ePedVarComp compId, u32 drawableId, u32 flags)
{
	if (compId == PV_COMP_INVALID)
		return false;

	u32 compFlags = LookupCompInfoFlags(compId, drawableId);
	if (compFlags == (u32)-1)
		return false;

	return ((compFlags & flags)	== flags);
}

u32	CPedVariationInfoCollection::LookupSecondaryFlags(const CPed* ped, s32 slotId)
{
    // check all drawables this ped has on him and if any act as the specified component (except that component of course)
    // return those flags as well
    u32 flags = 0;

    if (!ped)
        return flags;

    for (s32 i = 0; i < PV_MAX_COMP; ++i)
    {
        if (i == slotId)
            continue;

        u32 drawableId = ped->GetPedDrawHandler().GetVarData().GetPedCompIdx((ePedVarComp)i);
        if (drawableId == 255)
            continue;

        CPedVariationInfo* info = GetCollectionInfo(i, drawableId);
        if (!info)
            continue;

        flags |= info->LookupSecondaryFlags(i, drawableId, slotId);
    }

    return flags;
}

//------------------------------------------------------------------------------------------
