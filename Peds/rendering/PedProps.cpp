//rage headers
#if !__SPU

#include "Peds\rendering\pedprops.h"

#if __BANK
	#include "bank\bkmgr.h"
	#include "bank\bank.h"
	#include "bank\button.h"
	#include "bank\combo.h"
	#include "bank\slider.h"
	#include "bank\toggle.h"
	#include "bank\text.h"
#endif //__BANK
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "diag/art_channel.h"
#include "grcore/debugdraw.h"
#include "fwsys/fileExts.h"
#include "fwsys/metadatastore.h"
#include "rmcore/drawable.h"

//game headers
#include "animation/CreatureMetadata.h"
#include "Control/Replay/Replay.h"
#include "ModelInfo/PedModelInfo.h"
#include "Peds\ped.h"
#include "Peds\rendering\pedVariationPack.h"
#include "Peds\rendering\pedVariationStream.h"
#include "peds/rendering/PedVariationDebug.h"
#include "peds/rendering/pedvariationstream.h"
#include "scene\fileLoader.h"
#include "scene\world\gameworld.h"
#include "script/script.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/ScriptPreload.h"
#include "streaming\streaming.h"
#include "streaming\streamingmodule.h"
#include "system\filemgr.h"
#include "renderer\renderer.h"
#include "scene/ExtraMetadataMgr.h"
#if GTA_REPLAY
#include "control/replay/ReplayExtensions.h"
#endif // GTA_REPLAY


static	const char*	AnchorNames[NUM_ANCHORS] = {HEAD_NAME, HEAD_NAME, HEAD_NAME, HEAD_NAME,
													LHAND_NAME, RHAND_NAME, LWRIST_NAME, RWRIST_NAME, 
													HIP_NAME,LFOOT_NAME,RFOOT_NAME,
													PH_LHAND_NAME,PH_RHAND_NAME};
extern const char* propSlotNames[NUM_ANCHORS];


#if __BANK
	//debug vars
	char	attachName[24] = "undefined";
	char	propName[24] = "undefined";
	char dlcInformation[RAGE_MAX_PATH] = { 0 };
	s32	attachPointIdx = 0;
	s32	propIdx = -1;
	s32	propTexIdx = -1;
	s32	maxProps = 0;
	s32	maxPropsTex = 0;
	static bkSlider* pPropIdxSlider = NULL;
	static bkSlider* pPropTexIdxSlider = NULL;
	bool	showAnchorPoints = false;
	static Vector3 firstPersonEyewearOffset[CPedVariationStream::MAX_FP_PROPS];
#endif //__BANK

#define MAX_PROP_REQUEST_GFX (50)
FW_INSTANTIATE_CLASS_POOL(CPedPropRequestGfx, MAX_PROP_REQUEST_GFX, atHashString("PedProp req data",0x85fb462d));

#define MAX_PROP_RENDER_GFX (120)
FW_INSTANTIATE_CLASS_POOL(CPedPropRenderGfx, MAX_PROP_RENDER_GFX, atHashString("PedProp render data",0x539c8eb8));

CBaseModelInfo* CPedPropsMgr::ms_pGenericHat = NULL;
CBaseModelInfo* CPedPropsMgr::ms_pGenericHat2 = NULL;
CBaseModelInfo* CPedPropsMgr::ms_pGenericGlasses = NULL;

atArray<CPedPropRenderGfx*> CPedPropRenderGfx::ms_renderRefs[FENCE_COUNT];
s32 CPedPropRenderGfx::ms_fenceUpdateIdx = 0;

CPedPropsMgr::CPedPropsMgr(void)
{
}

CPedPropsMgr::~CPedPropsMgr(void)
{
}

static int s_alwaysZero = 0;

bool	CPedPropsMgr::Init(void)
{

	if (s_alwaysZero){
		Assertf(0, "Shouldn't get here");
		rage_new CPedPropExpressionData; // Make sure this class is referenced somewhere so an overeager linker doesn't strip it out
	}

	// want to ensure that physProps are always loaded 

	fwModelId capId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_hat",0xE60CBE8C), &capId);
	Assertf(pModelInfo, "Model Info for [physics_hat] is not valid");
	if (pModelInfo){
		Assertf(!pModelInfo->GetIsFixed(), "physics_hat _must_ be a dynamic object.");

		ms_pGenericHat = pModelInfo;
		CModelInfo::RequestAssets(capId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	}

	fwModelId capId2;
	pModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_hat2", 0x2E7E966C), &capId2);
	Assertf(pModelInfo, "Model Info for [physics_hat2] is not valid");
	if (pModelInfo){
		Assertf(!pModelInfo->GetIsFixed(), "physics_hat2 _must_ be a dynamic object.");

		ms_pGenericHat2 = pModelInfo;
		CModelInfo::RequestAssets(capId2, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	}

	fwModelId specsId;
	pModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_glasses",0x9F2D601A), &specsId);
	Assertf(pModelInfo, "Model Info for [physics_glasses] is not valid");
	if (pModelInfo){
		Assertf(!pModelInfo->GetIsFixed(), "physics_glasses _must_ be a dynamic object.");

		ms_pGenericGlasses = pModelInfo;
		CModelInfo::RequestAssets(specsId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	}

	CStreaming::LoadAllRequestedObjects();
	return(true);
}

void CPedPropsMgr::InitSystem(void)
{
	CPedPropRenderGfx::InitPool(MEMBUCKET_FX);
	CPedPropRequestGfx::InitPool(MEMBUCKET_FX);

	CPedPropRenderGfx::Init();
}

void CPedPropsMgr::Shutdown(void)
{
	if (ms_pGenericHat)
	{
		fwModelId modelId = CModelInfo::GetModelIdFromName("physics_hat");
		CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
		ms_pGenericHat = NULL;
	}

	if (ms_pGenericHat2)
	{
		fwModelId modelId = CModelInfo::GetModelIdFromName("physics_hat2");
		CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
		ms_pGenericHat2 = NULL;
	}

	if (ms_pGenericGlasses)
	{
		fwModelId modelId = CModelInfo::GetModelIdFromName("physics_glasses");
		CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_DONTDELETE);
		ms_pGenericGlasses = NULL;
	}

}

void CPedPropsMgr::ShutdownSystem(void)
{
	CPedStreamRenderGfx::ShutdownPool();
	CPedStreamRequestGfx::ShutdownPool();

	CPedPropRenderGfx::Shutdown();
}

u8 CPedPropInfo::GetMaxNumProps(eAnchorPoints anchorPointIdx) const
{
	u8 ret = 0;
	for (s32 i = 0; i < m_aAnchors.GetCount(); ++i)
	{
		if (m_aAnchors[i].anchor == anchorPointIdx)
		{
			ret = (u8)m_aAnchors[i].props.GetCount();
			break;
		}
	}

	return ret;
}

u8 CPedPropInfo::GetMaxNumPropsTex(eAnchorPoints anchorPointIdx, u32 propId)
{
	u8 ret = 0;
	for (s32 i = 0; i < m_aAnchors.GetCount(); ++i)
	{
		if (m_aAnchors[i].anchor == anchorPointIdx)
		{
			if (propId < m_aAnchors[i].props.GetCount())
			{
				ret = m_aAnchors[i].props[propId];
				break;
			}
		}
	}

	return ret;
}

u8 CPedPropInfo::GetMaxNumPropsTex(eAnchorPoints anchorPointIdx, u32 propId) const
{
	u8 ret = 0;
	for (s32 i = 0; i < m_aAnchors.GetCount(); ++i)
	{
		if (m_aAnchors[i].anchor == anchorPointIdx)
		{
			if (propId < m_aAnchors[i].props.GetCount())
			{
				ret = m_aAnchors[i].props[propId];
				break;
			}
		}
	}

	return ret;
}

#if __BANK
	void UpdateAttachIdxCB();
	void CPedPropsMgr::InitDebugData()
	{
		UpdateAttachIdxCB();
	}
#endif//__BANK

#else  //!__SPU
	#include "Peds\pedprops.h"
#endif //__SPU

// EJ: Memory Optimization
u32 CPedPropsMgr::GetPropBoneCount(CPedVariationInfoCollection* pVarInfo, const CPedPropData& propData)
{
	u32 numProps = 0;

	// EJ: Abort when (numProps == boneCount)
	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		const CPedPropData::SinglePropData& singlePropData = propData.GetPedPropData(i);
		eAnchorPoints anchorId = propData.GetPedPropActualAnchorPoint(i);

#if __DEV // KS - bug fix with badly formed prop data.
		if (anchorId >= ANCHOR_NONE && anchorId < NUM_ANCHORS)
#endif // __DEV
		{
			if (singlePropData.m_propModelID != PED_PROP_NONE)
			{
				const s32 boneIdx = pVarInfo->GetAnchorBoneIdx(anchorId);
				if (-1 != boneIdx)
				{			
					++numProps;
				}
			}			
		}
	}

	return numProps;
}

// EJ: Store the bone and prop data indices for rendering later.
u32 CPedPropsMgr::GetPropBoneIdxs(s32* pBoneIndices, size_t boneCount, CPedVariationInfoCollection* pVarInfo, CPedPropData& propData, s32* propDataIndices)
{
	Assert(boneCount > 0);

	u32 numProps = 0;

	// EJ: Abort when (numProps == boneCount)
	for (u32 i = 0; (i < MAX_PROPS_PER_PED) && (numProps < boneCount); ++i)
	{
		CPedPropData::SinglePropData& singlePropData = propData.GetPedPropData(i);
		eAnchorPoints anchorId = propData.GetPedPropActualAnchorPoint(i);

#if __DEV // KS - bug fix with badly formed prop data.
		if (anchorId >= ANCHOR_NONE && anchorId < NUM_ANCHORS)
#endif // __DEV
		{
			if (singlePropData.m_propModelID != PED_PROP_NONE)
			{
				s32 boneIdx = pVarInfo->GetAnchorBoneIdx(anchorId);
				if (boneIdx != -1)
				{
					// EJ: Only store valid bones
					pBoneIndices[numProps] = boneIdx;	
					propDataIndices[numProps] = i;

                    // we want to render eye props first to get correct alpha rendering when wearign glasses + helmet,
                    // so move it to the first slot if it's not there already
                    if (anchorId == ANCHOR_EYES && numProps != 0)
					{
                        pBoneIndices[numProps] = pBoneIndices[0];
                        pBoneIndices[0] = boneIdx;

                        propDataIndices[numProps] = propDataIndices[0];
                        propDataIndices[0] = i;
					}

					++numProps;
				}
			}
		}
	}

	return numProps;
}

// EJ: Memory Optimization
// EJ: The prop data indices will enable us to correctly reference the CPedPropData array offsets
void CPedPropsMgr::GetPropMatrices(Matrix34* pMatrices, const crSkeleton* pSkel, const s32* boneIdx, u32 boneCount, CPedPropData& propData, s32* propDataIndices, bool localMatrix)
{
	Assert(boneCount > 0);

	for (u32 i = 0; i < boneCount; ++i)
	{
		Matrix34 boneMatrix;
		if(localMatrix)
		{
			boneMatrix = RCC_MATRIX34(pSkel->GetObjectMtx(boneIdx[i]));
		}
		else
		{
			pSkel->GetGlobalMtx(boneIdx[i], RC_MAT34V(boneMatrix));
		}

		// EJ: Memory Optimization
		const int pos = propDataIndices[i];

		if (propData.m_aPropData[pos].m_anchorOverrideID == ANCHOR_NONE)
		{
			*pMatrices++ = boneMatrix;
		}
		else
		{
			Matrix34 offsetMatrix;
			offsetMatrix.Identity();
			offsetMatrix.FromQuaternion(propData.m_aPropData[pos].m_rotOffset);
			offsetMatrix.d = propData.m_aPropData[pos].m_posOffset;
			offsetMatrix.Dot(boneMatrix);

			*pMatrices++ = offsetMatrix;
		}
	}
}

#if !__SPU

inline u8 PedEffectLookupGlobalVar(const char *name, bool mustExist)
{
	grcEffectGlobalVar varID = grcEffect::LookupGlobalVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

//
//
//
//
static void CPedPropsMgr_ClearGlobalShaderVars(CPedModelInfo *ASSERT_ONLY(pModelInfo))
{
	Assert(pModelInfo);

	static const u8 idVarUmGlobalOverrideParams = PedEffectLookupGlobalVar("umPedGlobalOverrideParams0", true);
	Assertf(idVarUmGlobalOverrideParams, "Can't find umPedGlobalOverrideParams0 in %s.", pModelInfo->GetModelName());
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarUmGlobalOverrideParams, Vec4V(V_ONE));

#if 0 && (__WIN32PC || RSG_DURANGO || RSG_ORBIS)
	if(pClothParentMatrix)
	{
		grcEffect::SetGlobalVar((rage::grcEffectGlobalVar)idVarClothParentMatrix,(Mat44V*)pClothParentMatrix,1);
		grcEffect::SetGlobalVar((rage::grcEffectGlobalVar)idVarClothVertices,pClothVertices,numVertices);
	}
#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS

	static const u8 idVarDiffuseTexPal			= PedEffectLookupGlobalVar("DiffuseTexPal",				true);
	static const u8 idVarDiffuseTexPalSelector	= PedEffectLookupGlobalVar("DiffuseTexPaletteSelector",	true);
	Assertf(idVarDiffuseTexPal,			"Can't find DiffuseTexPal in %s.", pModelInfo->GetModelName());
	Assertf(idVarDiffuseTexPalSelector, "Can't find DiffuseTexPaletteSelector in %s.", pModelInfo->GetModelName());
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarDiffuseTexPal,			(grcTexture*)NULL);
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarDiffuseTexPalSelector,	0.0f);

	static const u8 idVarEnvEffFatSweatScale = PedEffectLookupGlobalVar("envEffFatSweatScale0", true);
	Assertf(idVarEnvEffFatSweatScale, "Can't find envEffFatSweatScale0 in %s.", pModelInfo->GetModelName());
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarEnvEffFatSweatScale, Vec4V(V_ZERO));

	static const u8 idVarEnvEffColorModCpvAdd	= PedEffectLookupGlobalVar("envEffColorModCpvAdd0",	true);
	Assertf(idVarEnvEffColorModCpvAdd, "Can't find envEffColorModCpvAdd0 in %s.", pModelInfo->GetModelName());
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarEnvEffColorModCpvAdd, Vec4V(V_ZERO));

	static const u8 idVarWrinkleMaskStrengths0 = PedEffectLookupGlobalVar("WrinkleMaskStrengths0", true);
	Assertf(idVarWrinkleMaskStrengths0, "Can't find WrinkleMaskStrengths0 in %s.", pModelInfo->GetModelName());
	const Vec4V values[6] = { Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO) };
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarWrinkleMaskStrengths0, values, 6);

	static const u8 idVarStubbleGrowthDetailScale	= PedEffectLookupGlobalVar("StubbleGrowth", true);
	Assertf(idVarStubbleGrowthDetailScale, "Can't find StubbleGrowth in %s.", pModelInfo->GetModelName());
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarStubbleGrowthDetailScale, Vec2V(V_ZERO));

	// set by ped props directly:
//	static const u8 idVarMaterialColorScale = PedEffectLookupGlobalVar("MaterialColorScale", true);
//	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarMaterialColorScale, Vec4V(V_ZERO));

	// set by ped props directly:
//	static const u8 idVarWetClothesData = PedEffectLookupGlobalVar("WetClothesData", true);
//	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarWetClothesData, vWetClothesData);

#if 1
	// if we are in the GBUF or Hud passes we should set the Data Data And samplers
	const bool bIsGbufPass	= DRAWLISTMGR->IsExecutingGBufDrawList();
	const bool bIsHUDPass	= DRAWLISTMGR->IsExecutingHudDrawList();
	const bool bIsMirrorPass= DRAWLISTMGR->IsExecutingMirrorReflectionDrawList();
	
	if (bIsHUDPass || bIsGbufPass || bIsMirrorPass)
	{
		// outside of GBUF pass, s15 is used for shadows, so don't mess with it
		if(bIsGbufPass)
		{
			static const u8 idVarPedBloodTarget	= PedEffectLookupGlobalVar("pedBloodTarget", true);
			Assert(idVarPedBloodTarget);
			grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarPedBloodTarget, grcTexture::NoneBlack);
		}

		static const u8 idVarPedTattooTarget = PedEffectLookupGlobalVar("pedTattooTarget", true);
		Assert(idVarPedTattooTarget);
		grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarPedTattooTarget, grcTexture::NoneBlack);
	}
#endif //#if 0...


	static const u8 idVarPedDamageData = PedEffectLookupGlobalVar("PedDamageData", true);
	Assertf(idVarPedDamageData, "Can't find PedDamageData in %s.", pModelInfo->GetModelName());
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarPedDamageData, Vec4V(V_ZERO));

	// set directly by ped props directly:
//	static const u8 idVarWetnessAdjust = PedEffectLookupGlobalVar("WetnessAdjust", true);
//	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarWetnessAdjust, Vec4V(V_ZERO));

	static const u8 idVarPedDamageColors = PedEffectLookupGlobalVar("pedDamageColors", false);		// depends on USE_VARIABLE_BLOOD_COLORS=1 in ped_common.fxh
	if(idVarPedDamageColors)
	{
		const Vec4V zeroDamageCols[3] = { Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO) };
		grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarPedDamageColors, zeroDamageCols, 3);
	}

}// end of CPedPropsMgr_ClearGlobalShaderVars()...

//
//
//
//
static void CPedPropsMgr_SetTextures(grmShaderGroup *pShaderGroup,
									 const grcTexture *pDiffuseTex,
									 const grcTexture *pNormalTex = grcTexture::NoneWhite,
									 const grcTexture *pSpecularTex =  grcTexture::NoneBlack)
{
	Assert(pShaderGroup);

	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
	{
		return;
	}

#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
	{
		return;
	}
#endif

	const grmShaderGroupVar idVarDiffuseTex = pShaderGroup->LookupVar("DiffuseTex");
	Assert(idVarDiffuseTex);

#if __BANK
	if (CPedVariationDebug::GetDisableDiffuse())
	{
		grcTexture *p = CPedVariationDebug::GetGreyDebugTexture();
		pDiffuseTex = p ? p : pDiffuseTex;
	}
#endif

#if CSE_PED_EDITABLEVALUES
	CCustomShaderEffectPed::SetEditableShaderValues(pShaderGroup, NULL);
#endif

	pShaderGroup->SetVar(idVarDiffuseTex, pDiffuseTex);

	static dev_bool bEnableBumpSpec = false; // temp disabled for BS#2220178
	if(bEnableBumpSpec)
	{
		const grmShaderGroupVar idVarNormalTex = pShaderGroup->LookupVar("BumpTex"); 
		if (idVarNormalTex)
		{
			pShaderGroup->SetVar(idVarNormalTex, pNormalTex);
		}

		const grmShaderGroupVar idVarSpecularTex = pShaderGroup->LookupVar("SpecularTex"); 
		if (idVarSpecularTex)
		{
			pShaderGroup->SetVar(idVarSpecularTex, pSpecularTex);
		}
	}
}

//
//
//
//
static void CPedPropsMgr_SetMaterialColorScale(const CCustomShaderEffectPed* pPedShaderEffect)
{
	static const u8 idVarMaterialColorScale = PedEffectLookupGlobalVar("MaterialColorScale", true);
	Assert(idVarMaterialColorScale);

	if(pPedShaderEffect)
	{
		const ScalarV vHeat((float)pPedShaderEffect->GetHeat()/255.0f);
		const ScalarV vEmissiveScale(pPedShaderEffect->GetEmissiveScale());
		const Vec4V pedNormal(ScalarV(1.0f), vEmissiveScale, ScalarV(1.0f), vHeat);
		const Vec4V pedVehBurnout(ScalarV(0.05f),Vec2V(V_ZERO), vHeat);
		const Vec4V pedDead(ScalarV(0.20f),Vec2V(V_ZERO), vHeat);
		const ScalarV burnLevel(pPedShaderEffect->GetBurnoutLevel());
		const Vec4V vColorScale = pPedShaderEffect->GetRenderDead()? Lerp(burnLevel,pedNormal,pedDead) : pPedShaderEffect->GetRenderVehBurnout()? Lerp(burnLevel,pedNormal,pedVehBurnout) : pedNormal;
		grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarMaterialColorScale, vColorScale);
	}
	else
	{
		grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarMaterialColorScale, Vec4V(V_ONE));
	}
}

//
//
//
//
static void CPedPropsMgr_SetWetClothesData(const Vector4& wetData, eAnchorPoints anchorId)
{
	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
	{
		return;
	}

#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
	{
		return;
	}
#endif

	// yuck, we really should cache this somewhere, maybe in CPedModelInfo?
	// at a minimum, it would be nice if we could pass in a hash for "WetClothesData"
	static const u8 idVarWetClothesData = PedEffectLookupGlobalVar("WetClothesData", false);
	Assert(idVarWetClothesData);
	if(idVarWetClothesData != grcegvNONE)
	{

		float height = 2.0f;	// ped props don't have height encoded in their cpv like the main ped. just estimate a basic height for the 
		switch (anchorId)
		{
			case ANCHOR_NONE:
				return;
			case ANCHOR_HEAD:
			case ANCHOR_EYES:
			case ANCHOR_EARS:
			case ANCHOR_MOUTH:
				height = 0.65f;
				break;
			case ANCHOR_LEFT_HAND:
			case ANCHOR_RIGHT_HAND:
			case ANCHOR_PH_L_HAND:
			case ANCHOR_PH_R_HAND:
			case ANCHOR_LEFT_WRIST:
			case ANCHOR_RIGHT_WRIST:
				height = -0.1f;
				break;
			case ANCHOR_HIP:
				height = 0.0f;
				break;
			case ANCHOR_LEFT_FOOT:
			case ANCHOR_RIGHT_FOOT:
				height = -0.75f;
			default:
				break;
		}

		grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarWetClothesData, Vec4V(wetData.x-height, wetData.y-height, -wetData.z, -wetData.w));  // pre subtract the height and wetdata and set the fade values to negative.
	}
}

//
//
//
//
static void CPedPropsMgr_SetWetnessAdjust()
{
	const Vec4V wetnessAdjustOverWater = Vec4V(0.35f, 8.0f, 0.35f, 0.1f);
	const Vec4V wetnessAdjustUnderWater = wetnessAdjustOverWater * Vec4V(V_X_AXIS_WONE);
	const Vec4V wetnessAdjust = (Water::IsCameraUnderwater()) ? wetnessAdjustUnderWater :  wetnessAdjustOverWater;
	static const u8 idVarWetnessAdjust = PedEffectLookupGlobalVar("WetnessAdjust", true);
	Assert(idVarWetnessAdjust);
	grcEffect::SetGlobalVar((grcEffectGlobalVar)idVarWetnessAdjust, wetnessAdjust);
}


void CPedPropsMgr::RenderPropAtPosn(CPedModelInfo *pModelInfo, const Matrix34& mat, s32 propIdxHash, s32 texIdxHash, u32 bucketMask, eRenderMode HAS_RENDER_MODE_SHADOWS_ONLY(renderMode), bool bAddToMotionBlurMask, const CCustomShaderEffectPed* pPedShaderEffect, eAnchorPoints anchorId)
{
	rmcDrawable*	pDrawable = NULL;
	Dwd*			pPedPropsDwd = NULL;

	Assert(pModelInfo);

	strLocalIndex	DwdIdx = strLocalIndex(pModelInfo->GetPropsFileIndex());
	if(DwdIdx != -1)
	{
		pPedPropsDwd = g_DwdStore.Get(DwdIdx);
		if (pPedPropsDwd != NULL)
		{
			//extract a ptr to the drawable from the Dwd (using this created name)
			pDrawable = pPedPropsDwd->Lookup(propIdxHash);
		}		
	}
	else
	{
		// find the Dwd containing the props for this ped	
		strLocalIndex DwdIdx = strLocalIndex(g_DwdStore.FindSlot(propIdxHash));
		if(DwdIdx == -1)
			return;

		if (!Verifyf(g_DwdStore.Get(DwdIdx), "Props dwd '%s' is not in memory! (%s)", g_DwdStore.GetName(DwdIdx), pModelInfo->GetModelName()))
			return;

		pDrawable = g_DwdStore.Get(DwdIdx)->GetEntry(0);
	}

	// if we can't find a drawable for this then skip over.
	if (!pDrawable){
		return;
	}
	
	grcTexture* pTex = NULL;
	if (DwdIdx != -1)
	{
		fwTxd* pTxd = g_TxdStore.Get(g_DwdStore.GetParentTxdForSlot(DwdIdx));

		if (pTxd)
		{
			pTex = pTxd->Lookup(texIdxHash);
		}
	}
	else
	{
		// remap the texture for this prop
		strLocalIndex txdIndex = strLocalIndex(g_TxdStore.FindSlot(texIdxHash));

		if (txdIndex != -1)
		{
			if (!Verifyf(g_TxdStore.Get(txdIndex), "Props txd '%s' is not in memory!", g_TxdStore.GetName(txdIndex)))
				return;

			pTex = g_TxdStore.Get(txdIndex)->GetEntry(0);
		}
	}

	CPedPropsMgr_ClearGlobalShaderVars(pModelInfo);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
	CPedPropsMgr_SetTextures(pShaderGroup, pTex);

	CPedPropsMgr_SetMaterialColorScale(pPedShaderEffect);

	if(pPedShaderEffect)
	{
		CPedPropsMgr_SetWetClothesData(pPedShaderEffect->GetWetClothesData(), anchorId);
	}

	CPedPropsMgr_SetWetnessAdjust();


	if (pDrawable)
	{
#if HAS_RENDER_MODE_SHADOWS
		if( IsRenderingShadows(renderMode) )
		{
			//only draw skinned props on the skinned shadow pass and non-skinned props on non-skinned shadow pass
			if( pDrawable->IsSkinned() == (renderMode == rmShadowsSkinned) )
			{
				pDrawable->DrawNoShaders(mat, bucketMask, 0);
			}
		}
		else
#endif // HAS_RENDER_MODE_SHADOWS
		{
			if( bAddToMotionBlurMask )
			{
				CShaderLib::BeginMotionBlurMask();
			}
			
			pDrawable->Draw(mat, bucketMask, 0, 6/*DCC_PROPS*/);
			
			if( bAddToMotionBlurMask )
			{
				CShaderLib::EndMotionBlurMask();
			}
		}
	}

	#if PGHANDLE_REF_COUNT
		// Only need to clear texture when debug is enabled, prop won't ever
		// render with a dangling grcTextureHandle.
		CPedPropsMgr_SetDiffuseTex(pShaderGroup, NULL);
	#endif
}

// local helpers to get cumulative transforms of a bone (used by RenderPropsInternal when the current matrix set has been overridden)
void PedPropsGetCumulativeDefaultTransform(Mat34V_InOut transformMtx, const crSkeletonData &skelData, const int startingBoneIndex)
{
	int curBoneIndex = startingBoneIndex;
	transformMtx = skelData.GetDefaultTransform(curBoneIndex);	

	while (curBoneIndex != 0)	
	{
		curBoneIndex = skelData.GetParentIndex(curBoneIndex);
		Transform(transformMtx, skelData.GetDefaultTransform(curBoneIndex), transformMtx );
	}
}

// render the props which are currently enabled - used by ped class, dumm ped class, & cutscene object class
void CPedPropsMgr::RenderPropsInternal(CPedModelInfo *pModelInfo, const Matrix34& rootMat, CPedPropRenderGfx* pGfx, const Matrix34 *pMatrices, const u32 matrixCount, CPedPropData& propData, s32* propDataIndices, u32 bucketMask, eRenderMode renderMode, bool bAddToMotionBlurMask, u8 lodIdx, const CCustomShaderEffectPed* pPedShaderEffect, bool bBlockFPSVisibleFlag)
{
	s32			boneIdx;
	s32			propIdx;

	Assert(pModelInfo);

	if (propData.m_bSkipRender)
		return;

	// if the matrix set has been overridden we need to work out a new bone transform
	// this is currently only used for ped mugshots
	const bool bMatrixSetOverridden = dlCmdAddCompositeSkeleton::IsCurrentMatrixSetOverriden();
	Matrix34 boneMatrixOverride;

	const bool bRootMatrixOverridden = dlCmdAddCompositeSkeleton::IsCurrentRootMatrixOverriden();
	Matrix34 rootMatOverride;
	Matrix34 finalRootMatOverride;

	if (bRootMatrixOverridden)
	{	
		Mat34V vRootMatOverride;
		dlCmdAddCompositeSkeleton::GetCurrentRootMatrixOverride(vRootMatOverride);
		rootMatOverride = MAT34V_TO_MATRIX34(vRootMatOverride);	
	}


	const bool mirrorPhase = DRAWLISTMGR->IsExecutingMirrorReflectionDrawList();
	const bool shadowPhase = DRAWLISTMGR->IsExecutingShadowDrawList();
	const bool globalReflPhase = DRAWLISTMGR->IsExecutingGlobalReflectionDrawList();
	const bool waterReflPhase = DRAWLISTMGR->IsExecutingWaterReflectionDrawList();
	bool allowStripHead = !mirrorPhase && !shadowPhase && !globalReflPhase && !waterReflPhase;

	// go through all the anchor slots and draw attached props for this ped
	for(u32 i = 0; i < matrixCount; ++i)
	{
		// EJ: Memory Optimization
		const int pos = propDataIndices[i];
		CPedPropData::SinglePropData& singlePropData = propData.GetPedPropData(pos);
		eAnchorPoints anchorId = propData.GetPedPropActualAnchorPoint(pos);
		propIdx = singlePropData.m_propModelID;

		// the anchorID (actual anchor) can be an overriden anchor point (e.g. a head helmet temporarily on the hand
		// bone while taken off). we need to use the original anchor for checking flags and similar things
		eAnchorPoints originalAnchor = singlePropData.m_anchorID;
		if (originalAnchor == ANCHOR_NONE)
			originalAnchor = anchorId;

		// skip empty anchor slots and props waiting for a head blend to finish
		if (propIdx == PED_PROP_NONE)
			continue;

		// if ped head is stripped don't render props on head bones
		if (allowStripHead)
		{
			if (dlCmdAddCompositeSkeleton::GetIsStrippedHead() || dlCmdAddSkeleton::GetIsStrippedHead())
			{
				if (anchorId == ANCHOR_HEAD || anchorId == ANCHOR_EYES || anchorId == ANCHOR_EARS || anchorId == ANCHOR_MOUTH)
					continue;
				if (((pModelInfo->GetVarInfo()->GetPropFlags((u8)originalAnchor, (u8)propIdx) & PV_FLAG_HIDE_IN_FIRST_PERSON) != 0) && !bBlockFPSVisibleFlag)
					continue;
			}
		}

        // we let props render lod 1 at the lod 2 level since these are large props, like helmets, that cause
        // too much of a visual artifact when they disappear. anything above lod 2 we skip though
        if (lodIdx > 2)
        {
            continue;
        }
        else if (lodIdx == 2)
        {
            if ((pModelInfo->GetVarInfo()->GetPropFlags((u8)originalAnchor, (u8)propIdx) & PV_FLAG_BULKY) != 0)
                lodIdx = 1;
            else
                continue;
        }

		// render the drawable at the anchor point
		if (pModelInfo->GetIsStreamedGfx())
		{
			boneIdx = pModelInfo->GetVarInfo()->GetAnchorBoneIdx(anchorId);
			if (pGfx && (boneIdx != -1))
			{
				const Matrix34* boneMat = &pMatrices[i];	

				// if the matrix set has been overridden we need to work out a new bone transform
				// this is currently only used for ped mugshots
				if (bMatrixSetOverridden)
				{
					rmcDrawable* pParentDrawable = pModelInfo->GetDrawable();
					boneMatrixOverride.Identity();
					if (pParentDrawable)
					{
						crSkeletonData *pSkelData = pParentDrawable->GetSkeletonData();

						if (pSkelData != NULL && pSkelData->GetDefaultTransforms() != NULL)
						{
							Mat34V vBoneMatOverride;
							PedPropsGetCumulativeDefaultTransform(vBoneMatOverride, *pSkelData, boneIdx);
							boneMatrixOverride = MAT34V_TO_MATRIX34(vBoneMatOverride);
						}
					}
					boneMat = &boneMatrixOverride;
				}
				else if (bRootMatrixOverridden)
				{
					// we need to undo the current root matrix transform
					finalRootMatOverride = *boneMat;
					finalRootMatOverride.DotTranspose(rootMat);
					// and apply the one set by the override command
					finalRootMatOverride.Dot(rootMatOverride);
					boneMat = &finalRootMatOverride;
				}

				grcTexture* pDiffMap = pGfx->GetTexture(pos);
				rmcDrawable* pDrawable = pGfx->GetDrawable(pos);

				if (pDrawable)
				{
					CPedPropsMgr_ClearGlobalShaderVars(pModelInfo);

					grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();
					CPedPropsMgr_SetTextures(pShaderGroup, pDiffMap);

					CPedPropsMgr_SetMaterialColorScale(pPedShaderEffect);

					if(pPedShaderEffect)
					{
						CPedPropsMgr_SetWetClothesData(pPedShaderEffect->GetWetClothesData(), anchorId);
					}

					CPedPropsMgr_SetWetnessAdjust();

	
#if HAS_RENDER_MODE_SHADOWS
					if( IsRenderingShadows(renderMode) )
					{
						//only draw skinned props on the skinned shadow pass and non-skinned props on non-skinned shadow pass
						if( pDrawable->IsSkinned() == (renderMode == rmShadowsSkinned) )
						{
							pDrawable->DrawNoShaders(*boneMat, bucketMask, 0);
						}
					}
					else
#endif // HAS_RENDER_MODE_SHADOWS
					{
						if( bAddToMotionBlurMask )
						{
							CShaderLib::BeginMotionBlurMask();
						}

						pDrawable->Draw(*boneMat, bucketMask, 0, 6/*DCC_PROPS*/);  

						if( bAddToMotionBlurMask )
						{
							CShaderLib::EndMotionBlurMask();
						}
					}

				#if PGHANDLE_REF_COUNT
						// Only need to clear texture when debug is enabled, prop
						// won't ever render with a dangling grcTextureHandle.
						CPedPropsMgr_SetDiffuseTex(pShaderGroup, NULL);
				#endif
				}
			}
		}
		else
		{
			s32 propIdxHash = singlePropData.m_propModelHash;
			s32 texIdxHash = singlePropData.m_propTexHash;

			Assert(propIdxHash != 0);
			Assert(texIdxHash != 0);

			// render the drawable at the anchor point
			boneIdx = pModelInfo->GetVarInfo()->GetAnchorBoneIdx(anchorId);
			if (boneIdx != -1)
			{
				const Matrix34* boneMat = &pMatrices[i];	

				// if the matrix set has been overridden we need to work out a new bone transform
				// this is currently only used for ped mugshots
				if (bMatrixSetOverridden)
				{
					rmcDrawable* pParentDrawable = pModelInfo->GetDrawable();
					boneMatrixOverride.Identity();
					if (pParentDrawable)
					{
						crSkeletonData *pSkelData = pParentDrawable->GetSkeletonData();

						if (pSkelData != NULL && pSkelData->GetDefaultTransforms() != NULL)
						{
							Mat34V vBoneMatOverride;
							PedPropsGetCumulativeDefaultTransform(vBoneMatOverride, *pSkelData, boneIdx);
							boneMatrixOverride = MAT34V_TO_MATRIX34(vBoneMatOverride);
						}
					}
					boneMat = &boneMatrixOverride;
				}
				else if (bRootMatrixOverridden)
				{
					// we need to undo the current root matrix transform
					finalRootMatOverride = *boneMat;
					finalRootMatOverride.DotTranspose(rootMat);
					// and apply the one set by the override command
					finalRootMatOverride.Dot(rootMatOverride);
					boneMat = &finalRootMatOverride;
				}


				RenderPropAtPosn(pModelInfo, *boneMat, propIdxHash, texIdxHash, bucketMask, renderMode, bAddToMotionBlurMask, pPedShaderEffect, anchorId);
			}
		}
	}
}


//--- prop debug stuff ---
#if __BANK
// render a crosshair at all anchor points for the given ped
void CPedPropsMgr::DrawDebugAnchorPoints(CPed* pPed, bool drawAnchors[NUM_ANCHORS])
{
	Assert(pPed);

	crSkeleton*	pSkel = pPed->GetSkeleton();
	s32			boneIdx;
	u32			j;
	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	Assert(pModelInfo);

	for(j=0;j<NUM_ANCHORS;j++)
	{
		boneIdx = pModelInfo->GetVarInfo()->GetAnchorBoneIdx((eAnchorPoints)j);
		if (drawAnchors[j] && (boneIdx != -1))
		{
			Matrix34 mBone;
			pSkel->GetGlobalMtx(boneIdx, RC_MAT34V(mBone));
			Vector3 vecPosition = mBone.GetVector(3);

			Color32 iColRed(0xff,0,0,0xff);
			Color32 iColGreen(0,0xff,0,0xff);
			Color32 iColBlue(0,0,0xff,0xff);

			grcDebugDraw::Line(vecPosition - Vector3(0.0f,0.0f,0.2f), vecPosition + Vector3(0.0f,0.0f,0.2f), iColRed);
			grcDebugDraw::Line(vecPosition - Vector3(0.0f,0.2f,0.0f), vecPosition + Vector3(0.0f,0.2f,0.0f), iColBlue);
			grcDebugDraw::Line(vecPosition - Vector3(0.2f,0.0f,0.0f), vecPosition + Vector3(0.2f,0.0f,0.0f), iColGreen);
		}
	}
}

void UpdateAttachIdxCB(void)
{
	CPed* pPed = CPedVariationDebug::GetDebugPed();
	if(pPed == NULL)
		return;
	ASSERT_ONLY(CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());)
	Assert(pModelInfo);

	if (pPed)
	{
		// get the damage value from the new bone idx
		propIdx = CPedPropsMgr::GetPedPropIdx(pPed, (eAnchorPoints)attachPointIdx);
	}

	maxProps = CPedPropsMgr::GetMaxNumProps(pPed, (eAnchorPoints)attachPointIdx)-1;
	if (pPropIdxSlider != NULL)
	{
		pPropIdxSlider->SetRange(-1.0f, (float)(maxProps));
	}


	//update the name of the bone attached to
	sprintf(attachName,"%s",AnchorNames[attachPointIdx]);

	//	//update the name of the prop
	if (propIdx == PED_PROP_NONE) {
		sprintf(propName, "No available prop");
		sprintf(dlcInformation, "No available prop");
	} else {
		sprintf(propName,"%s_%03d",propSlotNames[attachPointIdx],propIdx);

		u32 varInfoHash = 0;
		s32 localIndex = 0;

		CPedPropsMgr::GetLocalPropData(pPed, (eAnchorPoints)attachPointIdx, propIdx, varInfoHash, localIndex);
		sprintf(dlcInformation, "DLC Hash = %u || Local Index = %i || Global Index = %i", varInfoHash, localIndex, propIdx);
	}
}

static void UpdatePropTexIdxCB(void){
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed && propIdx>=0)
	{
		// set the prop and texture to the current settings
        Assertf(CPedPropsMgr::IsPropValid(pPed, (eAnchorPoints)attachPointIdx, propIdx, propTexIdx), "Invalid prop %d (tex %d) on '%s'. If the prop shows up just fine the meta data is wrong!", propIdx, propTexIdx, pPed->GetModelName());
		CPedPropsMgr::SetPedProp(pPed, (eAnchorPoints)attachPointIdx, propIdx, propTexIdx, ANCHOR_NONE, NULL, NULL);
	}
}

static void UpdatePropIdxCB(void)
{
	if (propIdx > maxProps) {
		propIdx = maxProps;
	}

	propTexIdx = 0;			// reset texture to default when changing prop
	UpdatePropTexIdxCB();

	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed) {
		//update the name of the prop
		if (propIdx == PED_PROP_NONE) {
			sprintf(propName, "No available prop");
			sprintf(dlcInformation, "No available prop");
			CPedPropsMgr::ClearPedProp(pPed, (eAnchorPoints)attachPointIdx);
		} else {
			sprintf(propName,"%s_%03d",propSlotNames[attachPointIdx],propIdx);
			maxPropsTex = CPedPropsMgr::GetMaxNumPropsTex(pPed, (eAnchorPoints)attachPointIdx, propIdx)-1;
			if (pPropTexIdxSlider != NULL)
			{
				pPropTexIdxSlider->SetRange(0.0f, (float)(maxPropsTex));
			}

			u32 varInfoHash = 0;
			s32 localIndex = 0;

			CPedPropsMgr::GetLocalPropData(pPed, (eAnchorPoints)attachPointIdx, propIdx, varInfoHash, localIndex);
			sprintf(dlcInformation, "DLC Hash = %u || Local Index = %i || Global Index =%i", varInfoHash, localIndex, propIdx);
		}
	}

//	printf("Prop packed:0x%08x\n",CPedPropsMgr::GetPedPropsPacked(pPed));
}

static void ClearPropIdxsCB(void)
{
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	propIdx = PED_PROP_NONE;

	if (pPed)
	{
		// set the new damage value in the curr bone idx
		CPedPropsMgr::ClearAllPedProps(pPed);
	}

	UpdatePropIdxCB();
}

static void RandomizeCB(void)
{
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed)
	{
		// set the new damage value in the curr bone idx
		CPedPropsMgr::ClearAllPedProps(pPed);
	}

	UpdatePropIdxCB();

	//CPedPropsMgr::SetPedPropsRandomly(pPed, 0.4f, 0.25f);

	//static s32 bitMask = 0xffffffff;
	//CPedPropsMgr::SetRestrictProps(bitMask);

	CPedPropsMgr::SetPedPropsRandomly(pPed, 0.4f, 0.25f, PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL);
}

void KnockOffHatCB(void){
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed)
	{
		CPedPropsMgr::TryKnockOffProp(pPed, ANCHOR_HEAD);
	}
}

void KnockOffGlassesCB(void){
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed)
	{
		CPedPropsMgr::TryKnockOffProp(pPed, ANCHOR_EYES);
	}
}

void GiveRandomHelmetCB(void){
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed)
	{
		CPedPropsMgr::SetRandomHelmet(pPed, PV_FLAG_DEFAULT_HELMET|PV_FLAG_RANDOM_HELMET);
	}
}

void GiveRandomHatCB(void){
	CPed* pPed = CPedVariationDebug::GetDebugPed();

	if (pPed)
	{
		CPedPropsMgr::SetRandomHat(pPed);
	}
}

static void ReloadFirstPersonMetaCB() {
	CPedVariationStream::ReloadFirstPersonMeta();
}

Vector3& CPedPropsMgr::GetFirstPersonEyewearOffset(size_t idx){
	Assert(idx < CPedVariationStream::MAX_FP_PROPS);
	return firstPersonEyewearOffset[idx];
}

bool CPedPropsMgr::SetupWidgets(bkBank& bank)
{
	bank.PushGroup("Ped Props",false);

	bank.AddSlider("Max prop ID for slot", &maxProps, 0, 32, 0);
	bank.AddSlider("Max tex for prop", &maxPropsTex, 0, 32, 0);

	const char* propSlotList[128];
	for (u32 i=0; i<NUM_ANCHORS; i++)
	{
		propSlotList[i] = propSlotNames[i];
	}

	bank.AddCombo("Prop slot", &attachPointIdx, (NUM_ANCHORS-1),  propSlotList, UpdateAttachIdxCB);
	bank.AddText("Prop name",(char*)propName,24,true);
	bank.AddText("DLC Information:",(char*)dlcInformation,RAGE_MAX_PATH,true);
	pPropIdxSlider = bank.AddSlider("Prop id", &propIdx, -1, 8, 1, UpdatePropIdxCB);
	pPropTexIdxSlider = bank.AddSlider("Tex id", &propTexIdx, -1, 8, 1, UpdatePropTexIdxCB);
	bank.AddButton("Clear props", ClearPropIdxsCB);
	bank.AddButton("Randomize props (0.4,0.25)", RandomizeCB);
	bank.AddToggle("Show anchor points",&showAnchorPoints);

	bank.AddButton("Knock off hat", KnockOffHatCB);
	bank.AddButton("Knock off glasses", KnockOffGlassesCB);

	bank.AddButton("Give random helmet", GiveRandomHelmetCB);
	bank.AddButton("Give random hat", GiveRandomHatCB);

	bank.AddSeparator();
	for(size_t i=0; i<CPedVariationStream::MAX_FP_PROPS; ++i)
	{
		bank.AddVector("First person eyewear offset", &firstPersonEyewearOffset[i], -10.0f, 10.0f, 0.001f);
	}
	bank.AddButton("Reload first person meta file", ReloadFirstPersonMetaCB);

	bank.PopGroup();

	return true;
}

void CPedPropsMgr::WidgetUpdate()
{
	ClearPropIdxsCB();
	UpdateAttachIdxCB();
	UpdatePropIdxCB();
}
#endif //__BANK
//---

// Takes a global prop index and converts it to the variationInfo dlcNameHash and local index of the prop
void CPedPropsMgr::GetLocalPropData(const CPed* pPed, eAnchorPoints anchorID, s32 propIndx, u32& outVarInfoHash, s32& outLocalIndex)
{
	outVarInfoHash = 0;
	outLocalIndex = -1;

	if (pPed && pPed->GetBaseModelInfo())
	{
		if (CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo()))
		{
			if (CPedVariationInfoCollection* pVarInfoColl = pPedModelInfo->GetVarInfo())
			{
				if (CPedVariationInfo* varInfo = pVarInfoColl->GetVariationInfoFromPropIdx((eAnchorPoints)anchorID, propIndx))
				{
					outVarInfoHash = varInfo->GetDlcNameHash();
					outLocalIndex = pVarInfoColl->GetDlcPropIdx((eAnchorPoints)anchorID, propIndx);
				}
			}
		}
	}
}

// Takes local prop data and converts it to a global index
s32 CPedPropsMgr::GetGlobalPropData(const CPed* pPed, eAnchorPoints anchorId, u32 varInfoHash, s32 localIndex)
{
	s32 propIdx = -1;

	if (CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo()))
	{
		if (CPedVariationInfoCollection* pVarInfoColl = pPedModelInfo->GetVarInfo())
		{
			// If the variation exists globalise the index here, otherwise leave it as default.
			if (pVarInfoColl->GetVariationInfoFromDLCName(varInfoHash) != NULL)
				propIdx = pVarInfoColl->GetGlobalPropIndex(localIndex, anchorId, varInfoHash);
			else
			{
				Assertf(varInfoHash == 0, 
					"CPedPropsMgr::GetGlobalCompData - No CPedVariationInfo found for non-zero hash! [%u/%s] [%s]", varInfoHash, atHashString(varInfoHash).TryGetCStr(), pPedModelInfo->GetModelName());
			}
		}
	}

	return propIdx;
}

bool CPedPropsMgr::IsAnyPedPropDelayedDelete(const CPed* pPed)
{
	Assert(pPed);
	return pPed->GetPedDrawHandler().GetPropData().GetIsAnyPedPropDelayedDeleted();
}

bool CPedPropsMgr::IsPedPropDelayedDelete(const CPed* pPed, eAnchorPoints anchorID)
{
	Assert(pPed);
	return(pPed->GetPedDrawHandler().GetPropData().GetIsPedPropDelayedDelete(anchorID));
}

s32	CPedPropsMgr::GetPedPropIdx(const CPed *pPed, eAnchorPoints anchorID)
{
	Assert(pPed);
	return(pPed->GetPedDrawHandler().GetPropData().GetPedPropIdx(anchorID));
}

s32	CPedPropsMgr::GetPedPropActualAnchorPoint(CPed *pPed,eAnchorPoints anchorID)
{
	Assert(pPed);
	return(pPed->GetPedDrawHandler().GetPropData().GetPedPropActualAnchorPoint(anchorID));
}	

s32	CPedPropsMgr::GetPedPropTexIdx(const CPed *pPed, eAnchorPoints anchorID)
{
	Assert(pPed);
	return(pPed->GetPedDrawHandler().GetPropData().GetPedPropTexIdx(anchorID));
}

s32 CPedPropsMgr::GetPropIdxWithFlags(CPed* pPed, eAnchorPoints anchorId, u32 propFlags)
{
	Assert(pPed);
	Assert(pPed->GetPedModelInfo());
	Assert(pPed->GetPedModelInfo()->GetVarInfo());

	return pPed->GetPedModelInfo()->GetVarInfo()->GetPropIdxWithFlags((u8)anchorId, propFlags);
}

s32 CPedPropsMgr::GetPedCompAltForProp(CPed *pPed, eAnchorPoints anchorID, s32 propID, s32 localDrawIdx, u32 uDlcNameHash, u8 comp)
{
	// trigger any alternate drawables on the ped if required by this new prop
	CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
	const CAlternateVariations::sAlternateVariationPed* altPedVar = CPedVariationStream::GetAlternateVariations().GetAltPedVar(pPedModelInfo->GetModelNameHash());
	if (altPedVar)
	{
		CAlternateVariations::sAlternates* altStorage = Alloca(CAlternateVariations::sAlternates, MAX_NUM_ALTERNATES);
		atUserArray<CAlternateVariations::sAlternates> alts(altStorage, MAX_NUM_ALTERNATES);

		CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();

		u32 dlcNameHash = pVarInfo->GetDlcNameHashFromPropIdx(anchorID, propID);

		// check if the variation we set now needs to change a different slot
		s32 localPropIdx = pPedModelInfo->GetVarInfo()->GetDlcPropIdx((eAnchorPoints)anchorID, propID);
		if (altPedVar->GetAlternatesFromProp((u8)anchorID, (u8)localPropIdx, dlcNameHash, alts))
		{
			for (s32 i = 0; i < alts.GetCount(); ++i)
			{
				Assert(alts[i].altComp != 0xff && alts[i].altIndex != 0xff && alts[i].alt != 0xff);

				if (Verifyf(alts[i].alt != 0, "An alternate variation metadata entry specifies a 0 alternate! (%s)", pPedModelInfo->GetModelName()))
				{
					if (alts[i].altComp == comp && alts[i].altIndex == localDrawIdx && alts[i].m_dlcNameHash.GetHash() == uDlcNameHash)
					{
						return alts[i].alt;
					}
				}
			}
		}
	}
	return 0;
}

void 	CPedPropsMgr::SetPedProp(CPed *pEntity, eAnchorPoints anchorID, s32 propID, s32 texID, eAnchorPoints anchorOverrideID, Vector3 *pPosOffset, Quaternion *pRotOffset, u32 streamingFlags){
	Assert(pEntity);

	//check that this is a valid propId
	Assertf(propID < GetMaxNumProps(pEntity, anchorID), "Setting invalid prop: (%s,%d) on %s\n",propSlotNames[(u32)anchorID], propID, pEntity->GetBaseModelInfo()->GetModelName());

	pEntity->GetPedDrawHandler().GetPropData().SetPedProp(pEntity, anchorID, propID, texID, anchorOverrideID, pPosOffset, pRotOffset, streamingFlags);

	UpdatePedForAlphaProps(pEntity, pEntity->GetPedDrawHandler().GetPropData(), pEntity->GetPedDrawHandler().GetVarData());
}

// returns a packed value for the anchor point for syncing over the network
// only the anchor points used in the network game as included
static int GetPackedAnchorValue(eAnchorPoints anchorPoint)
{
    switch(anchorPoint)
    {
    case ANCHOR_NONE:
        return 0;
    case ANCHOR_HEAD:
        return 1;
    case ANCHOR_EYES:
        return 2;
    case ANCHOR_EARS:
        return 3;
    case ANCHOR_LEFT_WRIST:
        return 4;
    case ANCHOR_RIGHT_WRIST:
        return 5;
	case ANCHOR_PH_L_HAND:
		return 6;
    default:
        Assertf(0, "Ped is using a anchor type not supported by the network game!");
        return 0;
    }
}

// returns an unpacked value for an anchor point that was synced over
// the network; only the anchor points used in the network game as included
static eAnchorPoints GetUnPackedAnchorValue(int anchorPoint)
{
    static eAnchorPoints packedAnchorPoints[] =
    {
        ANCHOR_NONE,
        ANCHOR_HEAD,
        ANCHOR_EYES,
        ANCHOR_EARS,
        ANCHOR_LEFT_WRIST,
        ANCHOR_RIGHT_WRIST,
		ANCHOR_PH_L_HAND
    };

    static const int numPackedAnchorPoints = sizeof(packedAnchorPoints) / sizeof(eAnchorPoints);

    Assert(anchorPoint >= 0);
    Assert(anchorPoint <  numPackedAnchorPoints);

    if(anchorPoint < 0 || anchorPoint >= numPackedAnchorPoints)
    {
        anchorPoint = 0;
    }

    return packedAnchorPoints[anchorPoint];
}

// get a packed bit field representing the current props attached to this ped
CPackedPedProps CPedPropsMgr::GetPedPropsPacked(const CDynamicEntity *pPed, bool skipDelayedDeleteProps)
{
	CPackedPedProps packedBitfield;

    if(pPed)
    {
        const CPed* ped = pPed->GetIsTypePed() ? static_cast<const CPed*>(pPed) : 0;
		Assert(ped);

        if(ped)
        {
			u32 propIdx = 0;
            for(int index = 0; index < MAX_PROPS_PER_PED; index++)
            {
                const CPedPropData::SinglePropData &propData = ped->GetPedDrawHandler().GetPropData().GetPedPropData(index);
				if(!propData.m_delayedDelete || !skipDelayedDeleteProps)
				{
					packedBitfield.SetPropData(ped, propIdx, propData.m_anchorID, propData.m_anchorOverrideID, propData.m_propModelID, propData.m_propTexID);
					++propIdx;
				}
            }
        }
    }

	return packedBitfield;
}

// attach to this ped the props defined in the packed bit field
void CPedPropsMgr::SetPedPropsPacked(CDynamicEntity *pPed, CPackedPedProps packedProps)
{
    Assert(pPed);

    if(pPed)
    {
        CPed* ped = pPed->GetIsTypePed() ? static_cast<CPed*>(pPed) : 0;
        Assert(ped);

        if(ped)
        {
	        ped->GetPedDrawHandler().GetPropData().ClearAllPedProps(ped);

            for(int index = 0; index < MAX_PROPS_PER_PED; index++)
            {
				const eAnchorPoints propAnchorId = packedProps.GetAnchorID(index);
				if (propAnchorId != ANCHOR_NONE)
				{
					const s32 modelId = packedProps.GetModelIdx(index, ped);

					if (modelId == PED_PROP_NONE)
						ClearPedProp(ped, propAnchorId);
					else
					{
						ped->GetPedDrawHandler().GetPropData().SetPedProp(
							ped,
							propAnchorId,
							modelId,
							packedProps.GetPropTextureIdx(index),
							packedProps.GetAnchorOverrideID(index),
							NULL,
							NULL );
					}
				}
				else
				{
					CPedPropData::SinglePropData &propData = ped->GetPedDrawHandler().GetPropData().GetPedPropData(index);

					propData.m_anchorID = propAnchorId;
					propData.m_anchorOverrideID = packedProps.GetAnchorOverrideID(index);
					propData.m_propModelID = packedProps.GetModelIdx(index, ped);
					propData.m_propTexID = packedProps.GetPropTextureIdx(index);
				}
            }

	        UpdatePedForAlphaProps(ped, ped->GetPedDrawHandler().GetPropData(), ped->GetPedDrawHandler().GetVarData());
        }
    }
}

bool 
CPedPropsMgr::IsPropValid(CPed* pPed, eAnchorPoints anchorId, s32 propId, s32 textId)
{
	Assert(pPed);

	if (-1==propId && 0==textId) // Removes the prop
		return true;

	s32 iMaxPropId = CPedPropsMgr::GetMaxNumProps(pPed, anchorId);

	if (propId >= iMaxPropId || textId >= CPedPropsMgr::GetMaxNumPropsTex(pPed, anchorId, propId))
	{
		return false;
	}

	return true;
}

u32 CPedPropsMgr::SetPreloadProp(CPed* pPed, eAnchorPoints anchorID, s32 propID, s32 texID, u32 streamingFlags)
{
	Assert(pPed);
	return pPed->GetPedDrawHandler().GetPropData().SetPreloadProp(pPed, anchorID, propID, texID, streamingFlags);
}

bool CPedPropsMgr::HasPreloadPropsFinished(CPed* pPed)
{
	Assert(pPed);
	return pPed->GetPedDrawHandler().GetPropData().HasPreloadPropsFinished();
}

bool CPedPropsMgr::HasPreloadPropsFinished(CPed* pPed, u32 handle)
{
	Assert(pPed);
	return pPed->GetPedDrawHandler().GetPropData().HasPreloadPropsFinished(handle);
}

void CPedPropsMgr::CleanUpPreloadProps(CPed* pPed)
{
	Assert(pPed);
	pPed->GetPedDrawHandler().GetPropData().CleanUpPreloadProps();
}

void CPedPropsMgr::CleanUpPreloadPropsHandle(CPed* pPed, u32 handle)
{
	Assert(pPed);
	pPed->GetPedDrawHandler().GetPropData().CleanUpPreloadPropsHandle(handle);
}

void		CPedPropsMgr::ClearPedProp(CPed *pEntity, eAnchorPoints anchorID, bool bForceClear/*=false*/)
{
	Assert(pEntity);
	pEntity->GetPedDrawHandler().GetPropData().ClearPedProp(pEntity, anchorID,bForceClear);

	UpdatePropExpressions(pEntity);
}

void		CPedPropsMgr::ClearAllPedProps(CPed *pEntity)
{
	Assert(pEntity);
	pEntity->GetPedDrawHandler().GetPropData().ClearAllPedProps(pEntity);

	UpdatePropExpressions(pEntity);
}

bool		CPedPropsMgr::CheckPropFlags(CPed* ped, eAnchorPoints anchor, u32 flags)
{
	if (!Verifyf(ped, "Null ped passed to CheckPropFlags!"))
		return false;

	s32 propIndex = ped->GetPedDrawHandler().GetPropData().GetPedPropIdx(anchor);
	if (propIndex == -1)
		return false;

	u32 propFlags = ped->GetPedModelInfo()->GetVarInfo()->GetPropFlags((u8)anchor, (u8)propIndex);
	return (propFlags & flags) != 0;
}

bool CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(const CPed& ped, u32 pedCompFlags)
{
	/*const*/ CPedModelInfo* pModelInfo = ped.GetPedModelInfo();
	if(!pModelInfo)
	{
		return false;
	}
	/*const*/ CPedVariationInfoCollection* pVarInfoCollection = pModelInfo->GetVarInfo();
	if(!pVarInfoCollection)
	{
		return false;
	}

	const CPedDrawHandler& drawHandler = ped.GetPedDrawHandler();

	const CPedVariationData& varData = drawHandler.GetVarData();
	for(int i = 0; i < PV_MAX_COMP; i++)
	{
		const ePedVarComp comp = (ePedVarComp)i;
		const u32 drawblId = varData.GetPedCompIdx(comp);
		if(pVarInfoCollection->HasComponentFlagsSet(comp, drawblId, pedCompFlags))
		{
			return true;
		}
	}

	const CPedPropData& propData = drawHandler.GetPropData();
	for(int i = 0; i < NUM_ANCHORS; i++)
	{
		const eAnchorPoints anchor = (eAnchorPoints)i;

		// This should be equivalent to what this would do, except with less overhead
		// since some stuff is done outside of the loop now:
		//	if(CPedPropsMgr::CheckPropFlags(this, anchor, pedCompFlags))
		//	{
		//		return true;
		//	}

		const s32 propIndex = propData.GetPedPropIdx(anchor);
		Assert(propIndex <= 0xff);
		if(propIndex >= 0 && (pVarInfoCollection->GetPropFlags((u8)anchor, (u8)propIndex) & pedCompFlags))
		{
			return true;
		}

	}

	return false;
}

u8		CPedPropsMgr::GetNumActiveProps(const CPed *pEntity)
{
	Assert(pEntity);
	return(pEntity->GetPedDrawHandler().GetPropData().GetNumActiveProps());
}

u8		CPedPropsMgr::GetMaxNumProps(CDynamicEntity *pEntity, eAnchorPoints anchorPointIdx)
{
	Assert(pEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	return((u8)pModelInfo->GetVarInfo()->GetMaxNumProps(anchorPointIdx));
}

u8		CPedPropsMgr::GetMaxNumPropsTex(CDynamicEntity *pEntity, eAnchorPoints anchorPointIdx, u32 propIdx)
{
	Assert(pEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	return((u8)pModelInfo->GetVarInfo()->GetMaxNumPropsTex(anchorPointIdx, propIdx));
}

//
// name:		void		CPedPropsMgr::SetPedPropRandomly
// description:	set 1 or 2 props randomly on this ped depending on given frequency.
//				freq range: (0.0f, 1.0f)
void		CPedPropsMgr::SetPedPropsRandomly(CPed* pEntity, float firstPropFreq, float secondPropFreq, u32 setRestrictionFlags, u32 clearRestrictionFlags, atFixedBitSet32* incs, atFixedBitSet32* excs,
		const CPedPropRestriction* pRestrictions, int numRestrictions, u32 wantHelmetType, u32 streamingFlags)
{

	Assert(pEntity);
	const CPedVariationData& pedVarData = pEntity->GetPedDrawHandler().GetVarData();
	CPedPropData& pedPropData = pEntity->GetPedDrawHandler().GetPropData();

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);
	CPedVariationInfoCollection* varInfo = pModelInfo->GetVarInfo();
	Assert(varInfo);

	if (!varInfo || !varInfo->HasAnyProps()){
		return;
	}

	const CPed* sameTypePeds[128];
	s32	numSameTypePeds = 0;
	numSameTypePeds = varInfo->GetExistingPeds(pEntity, numSameTypePeds, sameTypePeds);

	// get a sorted list of the prop textures in an ascending distribution rate order, i.e. top element occurs the least
	// and should therefore be prioritized for spawn.
	u8 anchorDistrib[NUM_ANCHORS] = { 0 };
	atArray<sPropSortedItem> offsets;
	SortPropTextures(varInfo, sameTypePeds, numSameTypePeds, offsets, anchorDistrib);

	u8 diceRoll = 255;
	if (numSameTypePeds == 0)
	{
		diceRoll = (u8)fwRandom::GetRandomNumberInRange(0, 256);
	}

	u32 propsLeft = MAX_PROPS_PER_PED;

	// keep track of what anchors we've already spawned a prop on so we don't try to spawn two props on the same place
	eAnchorPoints prevAnchors[MAX_PROPS_PER_PED - 1];
	for (u32 i = 0; i < MAX_PROPS_PER_PED - 1; ++i)
	{
		prevAnchors[i] = ANCHOR_NONE;
	}

	// Build an array to keep track of which anchors will need a prop in order
	// to fulfill the caller's requirements.
	atRangeArray<bool, NUM_ANCHORS> anchorRequiresProp;
	for(int i = 0; i < NUM_ANCHORS; i++)
	{
		anchorRequiresProp[i] = false;
	}
	for(int k = 0; k < numRestrictions; k++)
	{
		const CPedPropRestriction& restr = pRestrictions[k];

		// We need something on this anchor either if we must use a particular prop there,
		// or if we "can't use no prop".
		if(restr.GetRestriction() == CPedPropRestriction::MustUse
			|| (restr.GetRestriction() == CPedPropRestriction::CantUse && restr.GetPropIndex() < 0))
		{
			int slot = restr.GetPropSlot();
			if(slot >= 0 && slot < NUM_ANCHORS)
			{
				anchorRequiresProp[slot] = true;
			}
		}
	}

	if (wantHelmetType)
		anchorRequiresProp[ANCHOR_HEAD] = true;

	atFixedBitSet32 inclusions;
	if (incs)
		inclusions = *incs;
	atFixedBitSet32 exclusions;
	if (excs)
		exclusions = *excs;

	bool newRestrictions = false;

	// store the first 10 props (we don't tend to have much more than this) in case inclusion ids don't let anything spawn
	// but if we can't find a prop with any of the inclusion ids set, spawn a random one
	const u32 maxBackupProps = 10;
	u32 numBackupProps = 0;
	struct sTempPropData
	{
		u8 anchor;
		u8 prop;
		u8 tex;
	};
	sTempPropData backupProps[maxBackupProps];

    u32 tries = MAX_PROPS_PER_PED;
    do 
    {
		numBackupProps = 0;
		newRestrictions = false;
        bool triedToSpawnProp = false;
        for (u32 itemId = 0; itemId < offsets.GetCount(); ++itemId)
        {
            if (propsLeft == 0)
            {
                break;
            }

            u8 anchorId = offsets[itemId].anchorId;
            u8 propId = offsets[itemId].propId;
            u8 texId = offsets[itemId].texId;

            // early skip if distribution is set to 0, which means drawable is never meant to be randomized in
            if (offsets[itemId].distribution == 0)
            {
				// we might have a MustUse restriction on this prop, in which case we ignore the distribution of 0
				// and use it anyway
				bool skip = true;
				if (pRestrictions && anchorRequiresProp[anchorId])
				{
					for (s32 k = 0; k < numRestrictions; ++k)
					{
						const CPedPropRestriction& restr = pRestrictions[k];
						if (restr.GetRestriction() != CPedPropRestriction::MustUse)
							continue;
						if (restr.GetPropSlot() != anchorId)
							continue;
						if (restr.GetPropIndex() != propId)
							continue;

						skip = false;
						break;
					}
				}

				if (skip)
					continue;
            }

            // anchor distribution is set to 0 when there are more props than needed for the target distribution rate
            // for that specific anchor. we want to skip those props
            // Note: we probably should skip this if we the user requires a prop on this anchor (as is done now).
            if (anchorDistrib[anchorId] == 0 && !anchorRequiresProp[anchorId])
            {
                continue;
            }

            // Check to see if the user has requested this prop to not be used.
            if(IsPropRestricted((eAnchorPoints)anchorId, propId, pRestrictions, numRestrictions))
            {
                continue;
            }

            // make sure we don't try to spawn a second prop on same anchor
            bool skip = false;
            for (u32 i = 0; i < MAX_PROPS_PER_PED - 1; ++i)
            {
                if ((eAnchorPoints)anchorId == prevAnchors[i])
                {
                    skip = true;
                    break;
                }
            }

            if (skip)
            {
                continue;
            }

            // if no peds were found close to the spawn point, we have no distribution data for a correct pick
            // in this case we roll the dice and use the distribution as the spawn chance
            // - Note: but only if we are not required to pick a prop for this anchor.
            if (!anchorRequiresProp[anchorId] && numSameTypePeds == 0 && diceRoll > offsets[itemId].distribution)
            {
                continue;
            }

            const CPedPropMetaData* propData = NULL;
            const CPedPropTexData* texData = varInfo->GetPropTexData(anchorId, propId, texId, &propData);
			u32 propFlags = varInfo->GetPropFlags((u8)anchorId, (u8)propId);

            if (propData)
            {
                // skip helmets, we don't want to set them randomly on peds, unless explicitly requested
                u32 helmetFlags = PV_FLAG_DEFAULT_HELMET | PV_FLAG_RANDOM_HELMET | PV_FLAG_SCRIPT_HELMET;

				bool wantHelmet = wantHelmetType != 0;
				if (wantHelmet)
					helmetFlags = wantHelmetType;

				bool isHelmet = (propFlags & helmetFlags) != 0;
                if (wantHelmet != isHelmet)
                    continue;
            }

            // if no tex data we skip a bunch of restriction checks
            if (texData)
            {
                // check restriction flags
                if (propFlags != 0xFFFF)
                {
                    if ((propFlags & setRestrictionFlags) != setRestrictionFlags)
                    {
                        continue;
                    }

                    if ((propFlags & clearRestrictionFlags) != PV_FLAG_NONE)
                    {
                        continue;
                    }
                }

                // skip prop if any of the exclusion ids DO match
                if (exclusions.DoesOverlap(texData->exclusions))
                    continue;

                // skip prop if inclusion ids don't match
				if (inclusions.AreAnySet() && !inclusions.DoesOverlap(texData->inclusions))
				{
					if (numBackupProps < maxBackupProps)
					{
						sTempPropData& bp = backupProps[numBackupProps++];
						bp.anchor = anchorId;
						bp.prop = propId;
						bp.tex = texId;
					}
					continue;
				}
            }

            triedToSpawnProp = true;

			bool forceSpawn = (texData && inclusions.AreAnySet() && inclusions.DoesOverlap(texData->inclusions)) || anchorRequiresProp[anchorId];
			// TODO: FA: why the hell do we do all this work and at the end we check the dice roll? do this first and just skip
			// if we decide not to spawn any props (or just one instead of two)
            if (forceSpawn || (firstPropFreq >= fwRandom::GetRandomNumberInRange(0.0f, 1.0f)))
            {
                // Picked something for this anchor, so we no longer require anything else there:
                anchorRequiresProp[anchorId] = false;

                // we have a compatible prop and texture at this point so make it active
                pedPropData.SetPedProp(pEntity, static_cast<eAnchorPoints>(anchorId), propId, texId, ANCHOR_NONE, NULL, NULL, streamingFlags);

                // a new dice roll after a prop has passed to avoid situations where the first roll was high
                // and allows all props to pass
                if (numSameTypePeds == 0)
                {
                    diceRoll = (u8)fwRandom::GetRandomNumberInRange(0, 256);
                }

                prevAnchors[MAX_PROPS_PER_PED - propsLeft] = static_cast<eAnchorPoints>(anchorId);
                propsLeft--;

                if (texData)
				{
					// make sure this prop can apply its ids if required
					newRestrictions |= !texData->inclusions.IsEqual(inclusions);
					newRestrictions |= !texData->exclusions.IsEqual(exclusions);

					inclusions.Union(texData->inclusions);
					exclusions.Union(texData->exclusions);
                }
            }

            firstPropFreq = secondPropFreq;
            secondPropFreq = 0.f;
        }

        // if we didn't try to spawn any props they were all rejected. if they got rejected by inclusion ids
        // we should have a list of maxBackupProps props we can randomly select from
        // since this means we have no prop to satisfy the inclusion id
        if (!triedToSpawnProp && numBackupProps > 0 && (firstPropFreq >= fwRandom::GetRandomNumberInRange(0.0f, 1.0f)))
        {
            sTempPropData& bp = backupProps[0]; // select first one, they'll be sorted based on current distribution in the world
            pedPropData.SetPedProp(pEntity, static_cast<eAnchorPoints>(bp.anchor), bp.prop, bp.tex, ANCHOR_NONE, NULL, NULL, streamingFlags);

            prevAnchors[MAX_PROPS_PER_PED - propsLeft] = static_cast<eAnchorPoints>(bp.anchor);
            propsLeft--;

            const CPedPropMetaData* propData = NULL;
            const CPedPropTexData* texData = varInfo->GetPropTexData(bp.anchor, bp.prop, bp.tex, &propData);
            if (texData)
            {
				// make sure this prop can apply its ids if required
				newRestrictions |= !texData->inclusions.IsEqual(inclusions);
				newRestrictions |= !texData->exclusions.IsEqual(exclusions);

				inclusions.Union(texData->inclusions);
				exclusions.Union(texData->exclusions);

            }

            firstPropFreq = secondPropFreq;
            secondPropFreq = 0.f;
        }

    } while (--tries && propsLeft > 0 && newRestrictions);

    UpdatePedForAlphaProps(pEntity, pedPropData, pedVarData);
}

void CPedPropsMgr::SetRandomHelmet(CPed* ped, u32 helmetType)
{
	CPedPropsMgr::SetPedPropsRandomly(ped, 1.f, 0.f, PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, 0, helmetType);
}

void CPedPropsMgr::SetRandomHat(CPed* ped)
{
	CPedPropRestriction restrictions[1];
	restrictions[0].SetRestriction(ANCHOR_HEAD, -1, CPedPropRestriction::CantUse); // i.e., can't use an invalid model
	CPedPropsMgr::SetPedPropsRandomly(ped, 1.f, 0.f, PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, restrictions, 1, 0);
}

// if using alpha props then set draw last flag
// Complex signature is so it can work with dummy peds as well.
void		CPedPropsMgr::UpdatePedForAlphaProps(CEntity* pEntity, CPedPropData& pedPropData, const CPedVariationData& pedVarData)
{
	Assert(pEntity);

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pEntity->GetBaseModelInfo());
	Assert(pModelInfo);

	if (!Verifyf(pModelInfo->GetVarInfo(), "No pedVarInfo data on ped '%s'", pModelInfo->GetModelName()))
        return;

	CPedVariationInfoCollection* varInfo = pModelInfo->GetVarInfo();

	pedPropData.m_bIsCurrPropsAlpha = false;

	// test each prop point
	for (u32 i=0; i<NUM_ANCHORS; i++)
	{
		u32 propIdx = pedPropData.GetPedPropIdx((eAnchorPoints)i);

		if (propIdx == -1)
			continue;

		if (varInfo->HasPropAlpha((u8)i, (u8)propIdx))
		{
			pedPropData.m_bIsCurrPropsAlpha = true;
		}

		if (varInfo->HasPropDecal((u8)i, (u8)propIdx))
		{
			pedPropData.m_bIsCurrPropsDecal = true;
		}

		if (varInfo->HasPropCutout((u8)i, (u8)propIdx))
		{
			pedPropData.m_bIsCurrPropsCutout = true;
		}
	}

	pEntity->ClearBaseFlag(fwEntity::HAS_ALPHA | fwEntity::HAS_DECAL | fwEntity::HAS_CUTOUT);

	if ( pedPropData.m_bIsCurrPropsAlpha || pedVarData.IsCurrVarAlpha())
	{
		pEntity->SetBaseFlag(fwEntity::HAS_ALPHA);
	} 

	if ( pedPropData.m_bIsCurrPropsDecal || pedVarData.IsCurrVarDecal())
	{
		pEntity->SetBaseFlag(fwEntity::HAS_DECAL);
	} 

	if ( pedPropData.m_bIsCurrPropsCutout || pedVarData.IsCurrVarCutout())
	{
		pEntity->SetBaseFlag(fwEntity::HAS_CUTOUT);
	} 
}

// possibly trigger the knocking off of a prop
void CPedPropsMgr::TryKnockOffProp(CPed* pPed, eAnchorPoints propSlot){
	Assert(pPed);

	if (propSlot == ANCHOR_NONE)
	{
		return;
	}

	CPedPropData& propData = pPed->GetPedDrawHandler().GetPropData();
	s32 propIdx = propData.GetPedPropIdx(propSlot);

	if (propIdx == -1)
	{
		return;
	}

	CPedVariationInfoCollection* varInfo = pPed->GetPedModelInfo()->GetVarInfo();
	Assert(varInfo);
	float prob = varInfo->GetPropKnockOffProb(propSlot, propIdx);

	if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > prob)
	{
		return;
	}

	bool head = (propSlot == ANCHOR_HEAD);
	bool glasses = (propSlot == ANCHOR_EYES);
	KnockOffProps(pPed, true, head, glasses, head);
}

// convert the desired prop to a real object which is detached from the ped
void CPedPropsMgr::KnockOffProps(CPed* pPed, bool bDamaged, bool bHats, bool bGlasses, bool bHelmets, const Vector3* impulse, const Vector3* impulseOffset, const Vector3* angImpulse){

	Assert(pPed);

	Assertf(impulseOffset == NULL || angImpulse == NULL, "It doesn't make sense to supply an offset and an angular impulse since only one will be used");

	if (!pPed){
		return;
	}

	CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	Assert(pModelInfo);

	CPedPropData& pPropData = pPed->GetPedDrawHandler().GetPropData();

	s32 propIdx;

	propIdx = pPropData.GetPedPropIdx(ANCHOR_HEAD);

	bool headPropLoaded = false;
	if (propIdx != -1)
	{
		headPropLoaded = true;
		if (pPed->GetPedModelInfo()->GetIsStreamedGfx())
		{
			u32 propIdxHash;
			u32 texIdxHash;
			pPropData.GetPedPropHashes(ANCHOR_HEAD, propIdxHash, texIdxHash);
			strLocalIndex	dwdIdx = strLocalIndex(g_DwdStore.FindSlot(propIdxHash));
			headPropLoaded = dwdIdx != -1 && g_DwdStore.Get(dwdIdx);
		}
	}

	if (headPropLoaded && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanLosePropsOnDamage) || (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanLoseHelmetOnDamage) &&  bHelmets)|| !bDamaged) && ((bHats && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet )) || (bHelmets && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet )))){


		u32 propIdxHash;
		u32 texIdxHash;
		pPropData.GetPedPropHashes(ANCHOR_HEAD, propIdxHash, texIdxHash);

		Matrix34 genMat;

		// get the position of the hat prop
		eAnchorPoints anchorID = pPropData.GetPedPropActualAnchorPoint(ANCHOR_HEAD);
		if(anchorID != ANCHOR_NONE)
		{
			s32 boneIdx = pModelInfo->GetVarInfo()->GetAnchorBoneIdx(anchorID);
			if (boneIdx != -1) {
				crSkeleton* pSkel = pPed->GetSkeleton();
				Matrix34 boneMat;
				pSkel->GetGlobalMtx(boneIdx, RC_MAT34V(boneMat));

				if (anchorID == ANCHOR_HEAD){
					genMat = boneMat;
				}
				else{
					Matrix34 offsetMat;
					Vector3 posOffset;
					Quaternion rotOffset;

					pPropData.GetPedPropPosOffset(ANCHOR_HEAD, posOffset);
					pPropData.GetPedPropRotOffset(ANCHOR_HEAD, rotOffset);

					offsetMat.Identity();
					offsetMat.FromQuaternion(rotOffset);
					offsetMat.d = posOffset;
					offsetMat.Dot(boneMat);
					genMat = offsetMat;	
				}
			}
		}

		bool useSecondHat = CPedPropsMgr::CheckPropFlags(pPed, ANCHOR_HEAD, PV_FLAG_USE_PHYSICS_HAT_2);

		CBaseModelInfo* pModelInfo = useSecondHat
			? CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_hat2", 0x2E7E966C), NULL)
			: CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_hat", 0xE60CBE8C), NULL);

		if (Verifyf(pModelInfo, "Failed to find %s", useSecondHat ? "physics_hat2" : "physics_hat"))
		{
			fwModelId modelId = useSecondHat
				? CModelInfo::GetModelIdFromName(strStreamingObjectName("physics_hat2", 0x2E7E966C))
				: CModelInfo::GetModelIdFromName(strStreamingObjectName("physics_hat", 0xE60CBE8C));

			//Assert(pModelInfo->HasPhysics());
			CObject* pDisplayObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_TEMP, true);
            if (Verifyf(pDisplayObject, "Failed to create object for knocked off prop hat"))
            {
                pDisplayObject->SetMatrix(genMat);
                pDisplayObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() + 30000;
                pDisplayObject->m_nObjectFlags.bCanBePickedUp = false;
                pDisplayObject->m_nObjectFlags.bIsHelmet = bHelmets && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet ) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanLoseHelmetOnDamage );

				//Remove helmets after 1 min even if they are on screen.
				if(pDisplayObject->m_nObjectFlags.bIsHelmet)
				{
					pDisplayObject->m_nObjectFlags.bSkipTempObjectRemovalChecks = true;
				}

                pDisplayObject->SetNoCollision(pPed, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
                pDisplayObject->SetFragParent(pPed);
                pDisplayObject->GetLodData().SetResetDisabled(true); // don't want to reset alpha to 0 on drawable creation

                CGameWorld::Add(pDisplayObject, pPed->GetInteriorLocation() );

                phInst* pDisplayObjectPhysicsInst = pDisplayObject->GetCurrentPhysicsInst();
                if(pDisplayObjectPhysicsInst)
                {
                    Vector3 offset(0.2f, 0.0f, -0.01f);
                    if (impulseOffset)
                        offset = *impulseOffset;
                    if(bHelmets && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet ) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanLoseHelmetOnDamage ))
                    {
                        static dev_float impulseX = -30.0f;
                        static dev_float impulseY =   0.0f;
                        static dev_float impulseZ = -25.0f;
                        Vector3 localImpulse(impulseX, impulseY, impulseZ);
                        if (impulse)
                            localImpulse = *impulse;

						localImpulse = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(RCC_VEC3V(localImpulse)));
						if(angImpulse)
						{
							pDisplayObject->ApplyImpulseCg(localImpulse);
							pDisplayObject->ApplyAngImpulse(*angImpulse);
						}
						else
						{
							pDisplayObject->ApplyImpulse(localImpulse, offset);
						}
                    }
                    else
                    {
                        Vector3 localImpulse(5.0f, 0.0f, 5.0f);
                        if (impulse)
                            localImpulse = *impulse;

						if(angImpulse)
						{
							pDisplayObject->ApplyImpulseCg(localImpulse);
							pDisplayObject->ApplyAngImpulse(*angImpulse);
						}
						else
						{
							pDisplayObject->ApplyImpulse(localImpulse, offset);
						}
                    }

                    //Prevent the camera shapetests from detecting this prop, as we don't want the camera to pop if it passes near to the camera
                    //or collision root position.
                    u16 levelIndex = pDisplayObjectPhysicsInst->GetLevelIndex();
                    phLevelNew* pPhysicsLevel = CPhysics::GetLevel();
                    if(pPhysicsLevel && pPhysicsLevel->IsInLevel(levelIndex))
                    {
                        u32 includeFlags = pPhysicsLevel->GetInstanceIncludeFlags(levelIndex);
                        includeFlags &= ~ArchetypeFlags::GTA_CAMERA_TEST;
                        CPhysics::GetLevel()->SetInstanceIncludeFlags(levelIndex, includeFlags);
                    }
                    else if(pDisplayObjectPhysicsInst->GetArchetype())
                    {
                        //NOTE: This include flag change will apply to all new instances that use this archetype until it is streamed out.
                        //We can live with this for camera purposes, but it's worth noting in case it causes a problem.
                        pDisplayObjectPhysicsInst->GetArchetype()->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
                    }
                }

                pDisplayObject->m_nObjectFlags.bDetachedPedProp = true;
                pDisplayObject->uPropData.m_parentModelIdx = pPed->GetModelIndex();
                pDisplayObject->uPropData.m_propIdxHash = propIdxHash;
                pDisplayObject->uPropData.m_texIdxHash = texIdxHash;

                strLocalIndex	DwdIdx = strLocalIndex(pPed->GetPedModelInfo()->GetPropsFileIndex());
                if (DwdIdx != -1) 
                {
                    // pack ped
                    g_DwdStore.AddRef(DwdIdx, REF_OTHER);
                    g_TxdStore.AddRef(g_DwdStore.GetParentTxdForSlot(DwdIdx), REF_OTHER);
                }
                else 
                {
                    // streamed ped
                    strLocalIndex	dwdIdx = strLocalIndex(g_DwdStore.FindSlot(propIdxHash));
                    if (dwdIdx != -1)
                        g_DwdStore.AddRef(dwdIdx, REF_OTHER);

                    strLocalIndex	txdIdx = strLocalIndex(g_TxdStore.FindSlot(texIdxHash));
                    if (txdIdx != -1)
                        g_TxdStore.AddRef(txdIdx, REF_OTHER);
                }

#if GTA_REPLAY
				CReplayMgr::RecordObject(pDisplayObject);

				if( CReplayMgr::ShouldRecord() )
				{
					//if we don't have the extension then we need to get one
					if(ReplayObjectExtension::HasExtension(pDisplayObject) == false)
					{
						ReplayObjectExtension::Add(pDisplayObject);
					}

					if(ReplayObjectExtension::HasExtension(pDisplayObject) == true)
					{
						ReplayObjectExtension::SetPropHash(pDisplayObject, propIdxHash);
						ReplayObjectExtension::SetTextHash(pDisplayObject, texIdxHash);
						ReplayObjectExtension::SetAnchorID(pDisplayObject, ANCHOR_HEAD);
						ReplayObjectExtension::SetStreamedPedProp(pDisplayObject, DwdIdx == -1);
					}
					else
					{
						replayAssertf(false, "Failed to set prop information as there was no extension added...there should be related asserts previous to this one.");
					}
				}
#endif // GTA_REPLAY
            }
        }

		// turn off the hat ped prop
		pPropData.ClearPedProp(pPed, ANCHOR_HEAD, false);		
	}

	bool eyesPropLoaded = false;
	if (pPropData.GetPedPropIdx(ANCHOR_EYES) != -1)
	{
		eyesPropLoaded = true;
		if (pPed->GetPedModelInfo()->GetIsStreamedGfx())
		{
			u32 propIdxHash;
			u32 texIdxHash;
			pPropData.GetPedPropHashes(ANCHOR_EYES, propIdxHash, texIdxHash);
			strLocalIndex	dwdIdx = strLocalIndex(g_DwdStore.FindSlot(propIdxHash));
			eyesPropLoaded = dwdIdx != -1 && g_DwdStore.Get(dwdIdx);
		}
	}

	if (eyesPropLoaded && bGlasses && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanLosePropsOnDamage) || !bDamaged)){

		u32 propIdxHash;
		u32 texIdxHash;
		pPropData.GetPedPropHashes(ANCHOR_EYES, propIdxHash,texIdxHash);

		Matrix34 genMat;

		// get the position of the hat prop
		s32 boneIdx = pModelInfo->GetVarInfo()->GetAnchorBoneIdx(ANCHOR_EYES);
		if (boneIdx != -1) {
			crSkeleton* pSkel = pPed->GetSkeleton();
			pSkel->GetGlobalMtx(boneIdx, RC_MAT34V(genMat));
		}

		CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_glasses",0x9F2D601A), NULL);
		if (Verifyf(pModelInfo, "Failed to find physics_glasses"))
		{
			//Assert(CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("physics_glasses",0x9F2D601A), NULL)->HasPhysics());
			fwModelId specsId = CModelInfo::GetModelIdFromName(strStreamingObjectName("physics_glasses",0x9F2D601A));
			CObject* pDisplayObject = CObjectPopulation::CreateObject(specsId, ENTITY_OWNEDBY_TEMP, true);
            if (Verifyf(pDisplayObject, "Failed to create object for knocked off prop glasses"))
            {
                pDisplayObject->SetMatrix(genMat);
                pDisplayObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() + 30000;
                pDisplayObject->m_nObjectFlags.bCanBePickedUp = false;

                pDisplayObject->SetNoCollision(pPed, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
                pDisplayObject->SetFragParent(pPed);

                CGameWorld::Add(pDisplayObject, pPed->GetInteriorLocation());

                phInst* pDisplayObjectPhysicsInst = pDisplayObject->GetCurrentPhysicsInst();
                if(pDisplayObjectPhysicsInst)
                {
                    Vector3 localImpulse(5.0f, 0.0f, 3.0f);
                    if (impulse)
                        localImpulse = (*impulse) * 0.3f;
                    Vector3 offset(0.1f, 0.0f, 0.0f);
                    if (impulseOffset)
                        offset = *impulseOffset;

					if(angImpulse)
					{
						pDisplayObject->ApplyImpulseCg(localImpulse);
						pDisplayObject->ApplyAngImpulse(*angImpulse);
					}
					else
					{
						pDisplayObject->ApplyImpulse(localImpulse, offset);
					}

                    //Prevent the camera shapetests from detecting this prop, as we don't want the camera to pop if it passes near to the camera
                    //or collision root position.
                    u16 levelIndex = pDisplayObjectPhysicsInst->GetLevelIndex();
                    phLevelNew* pPhysicsLevel = CPhysics::GetLevel();
                    if(pPhysicsLevel && pPhysicsLevel->IsInLevel(levelIndex))
                    {
                        u32 includeFlags = pPhysicsLevel->GetInstanceIncludeFlags(levelIndex);
                        includeFlags &= ~ArchetypeFlags::GTA_CAMERA_TEST;
                        CPhysics::GetLevel()->SetInstanceIncludeFlags(levelIndex, includeFlags);
                    }
                    else if(pDisplayObjectPhysicsInst->GetArchetype())
                    {
                        //NOTE: This include flag change will apply to all new instances that use this archetype until it is streamed out.
                        //We can live with this for camera purposes, but it's worth noting in case it causes a problem.
                        pDisplayObjectPhysicsInst->GetArchetype()->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
                    }
                }

                pDisplayObject->m_nObjectFlags.bDetachedPedProp = true;
                pDisplayObject->uPropData.m_parentModelIdx = pPed->GetModelIndex();
                pDisplayObject->uPropData.m_propIdxHash = propIdxHash;
                pDisplayObject->uPropData.m_texIdxHash = texIdxHash;

                strLocalIndex	DwdIdx = strLocalIndex(pPed->GetPedModelInfo()->GetPropsFileIndex());
                if (DwdIdx != -1) 
                {
                    // pack ped
                    g_DwdStore.AddRef(DwdIdx, REF_OTHER);
                    g_TxdStore.AddRef(g_DwdStore.GetParentTxdForSlot(DwdIdx), REF_OTHER);
                }
                else 
                {
                    // streamed ped
                    strLocalIndex	dwdIdx = strLocalIndex(g_DwdStore.FindSlot(propIdxHash));
                    if (dwdIdx != -1)
                        g_DwdStore.AddRef(dwdIdx, REF_OTHER);

                    strLocalIndex	txdIdx = strLocalIndex(g_TxdStore.FindSlot(texIdxHash));
                    if (txdIdx != -1)
                        g_TxdStore.AddRef(txdIdx, REF_OTHER);
                }

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					//if we don't have the extension then we need to get one
					if(ReplayObjectExtension::HasExtension(pDisplayObject) == false)
					{
						ReplayObjectExtension::Add(pDisplayObject);
					}

					ReplayObjectExtension::SetPropHash(pDisplayObject, propIdxHash);
					ReplayObjectExtension::SetTextHash(pDisplayObject, texIdxHash);
					ReplayObjectExtension::SetAnchorID(pDisplayObject, ANCHOR_EYES);
					ReplayObjectExtension::SetStreamedPedProp(pDisplayObject, DwdIdx == -1);
				}
#endif // GTA_REPLAY
            }
        }

		// turn off the glasses ped prop
		pPropData.ClearPedProp(pPed, ANCHOR_EYES, false);
	}
}

int ComparePropTextures(const sPropSortedItem* a, const sPropSortedItem* b)
{
	return (int)b->occurrence - (int)a->occurrence;
}

void CPedPropsMgr::SortPropTextures(CPedVariationInfoCollection* varInfo, const CPed** peds, u32 numPeds, atArray<sPropSortedItem>& offsets, u8* anchorDistrib)
{
	Assert(varInfo && peds);

	// create and randomize initial array
	RandomizeTextures(varInfo, offsets, anchorDistrib);

	if (numPeds == 0)
	{
		// sort based on target distribution
		offsets.QSort(0, -1, ComparePropTextures);
		return;
	}

	u8 anchorCount[NUM_ANCHORS] = { 0 };

	// gather occurrence count for every texture
	for (u32 i = 0; i < numPeds; ++i)
	{
		const CPedPropData* propData = &peds[i]->GetPedDrawHandler().GetPropData();

		for (u32 f = 0; f < MAX_PROPS_PER_PED; ++f)
		{
			const CPedPropData::SinglePropData& sd = propData->GetPedPropData(f);

			// find texture and increment count
			for (u32 g = 0; g < offsets.GetCount(); ++g)
			{
				if (offsets[g].anchorId == (u8)sd.m_anchorID && offsets[g].propId == sd.m_propModelID && offsets[g].texId == sd.m_propTexID)
				{
					offsets[g].count++;
					anchorCount[offsets[g].anchorId]++;
					break;
				}
			}
		}
	}

	// calculate occurrence per anchor and overwrite the anchor distribution with the deviation
	// set negative deviation to 0
	for (u32 i = 0; i < NUM_ANCHORS; ++i)
	{
		u32 occ = (anchorCount[i] * 255) / numPeds;
		s32 deviation = anchorDistrib[i] - occ;
		deviation = deviation < 0 ? 0 : deviation;
		anchorDistrib[i] = deviation > 255 ? 255 : u8(deviation);
	}

	// calculate occurrence ratio and store deviation from target distribution
	for (u32 i = 0; i < offsets.GetCount(); ++i)
	{
		u32 occ = (offsets[i].count * offsets[i].distribution) / numPeds;
		offsets[i].occurrence = u8( offsets[i].distribution - occ );
	}

	// sort based on distribution deviation
	offsets.QSort(0, -1, ComparePropTextures);
}

void CPedPropsMgr::RandomizeTextures(CPedVariationInfoCollection* varInfo, atArray<sPropSortedItem>& offsets, u8* anchorDistrib)
{
	if (!Verifyf(varInfo, "NULL variation info collection passed to RandomizeTextures!"))
		return;

	u32 totalDistrib[NUM_ANCHORS] = { 0 };
	u32 totalCount[NUM_ANCHORS] = { 0 };

	// init array with default values
	for (u32 i = 0; i < NUM_ANCHORS; ++i)
	{
		for (u32 f = 0; f < varInfo->GetMaxNumProps(static_cast<eAnchorPoints>(i)); ++f)
		{
			for (u32 g = 0; g < varInfo->GetMaxNumPropsTex(static_cast<eAnchorPoints>(i), f); ++g)
			{
				u8 distrib = 100;
				const CPedPropTexData* texData = varInfo->GetPropTexData(u8(i), u8(f), u8(g), NULL);
				if (texData)
				{
					distrib = texData->distribution;
				}

				sPropSortedItem item;
				item.anchorId = u8(i);
				item.propId = u8(f); 
				item.texId = u8(g);
				item.count = 0;
				item.distribution = distrib;
				item.occurrence = item.distribution;
				offsets.PushAndGrow(item);

				totalDistrib[i] += item.distribution;

				if (item.distribution != 0)
				{
					totalCount[i]++;
				}
			}			
		}
	}

	// randomize item array
	for (u32 i = 0; i < offsets.GetCount(); ++i)
	{
		u32 newIdx = fwRandom::GetRandomNumberInRange(0, offsets.GetCount());
		sPropSortedItem item = offsets[i];
		offsets[i] = offsets[newIdx];
		offsets[newIdx] = item;
	}

	// calculate anchor distributions
	for (u32 i = 0; i < NUM_ANCHORS; ++i)
	{
		if (totalCount[i] == 0)
		{
			anchorDistrib[i] = 0;
			continue;
		}

		anchorDistrib[i] = u8( totalDistrib[i] / totalCount[i] );
	}
}


void CPedPropsMgr::ApplyStreamPropFiles(CPed* pPed, CPedPropRequestGfx* pPSGfx)
{
	Assert(pPSGfx);
	pPSGfx->RequestsComplete();						// add refs to loaded gfx & free up the requests

	CPedPropRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetPedPropRenderGfx();
	Assert(pGfxData);

	USE_MEMBUCKET(MEMBUCKET_SYSTEM);

	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		pPed->GetPedDrawHandler().GetPropData().DelayedDelete(i);

		rmcDrawable* pDrawable = pGfxData->GetDrawable(i);
		if (!pDrawable)
			continue;

		// initialize shader bucket bits in PedPropInfo:
		CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
		Assert(pModelInfo);

		CPedVariationInfoCollection* pVarInfo = pModelInfo->GetVarInfo();
		Assert(pVarInfo);
		if (pVarInfo)
		{
			CPedPropData::SinglePropData& singlePropData = pPed->GetPedDrawHandler().GetPropData().GetPedPropData(i);
			const s32 propIdx = singlePropData.m_propModelID;
			const eAnchorPoints anchorId = singlePropData.m_anchorID;

			if (pDrawable && propIdx != PED_PROP_NONE && anchorId != ANCHOR_NONE) 
			{
				pDrawable->GetLodGroup().ComputeBucketMask(pDrawable->GetShaderGroup());

				pVarInfo->SetPropFlag((u8)anchorId, (u8)propIdx, PRF_ALPHA, false);
				if (pDrawable->GetBucketMask(0) & (0x01 << CRenderer::RB_ALPHA))
				{
					pVarInfo->SetPropFlag((u8)anchorId, (u8)propIdx, PRF_ALPHA, true);
				}

				pVarInfo->SetPropFlag((u8)anchorId, (u8)propIdx, PRF_DECAL, false);
				if (pDrawable->GetBucketMask(0) & (0x01 << CRenderer::RB_DECAL))
				{
					pVarInfo->SetPropFlag((u8)anchorId, (u8)propIdx, PRF_DECAL, true);
				}

				pVarInfo->SetPropFlag((u8)anchorId, (u8)propIdx, PRF_CUTOUT, false);
				if (pDrawable->GetBucketMask(0) & (0x01 << CRenderer::RB_CUTOUT))
				{
					pVarInfo->SetPropFlag((u8)anchorId, (u8)propIdx, PRF_CUTOUT, true);
				}
			}
		}
	}

	UpdatePedForAlphaProps(pPed, pPed->GetPedDrawHandler().GetPropData(), pPed->GetPedDrawHandler().GetVarData());
}

void CPedPropsMgr::ProcessStreamRequests()
{
	CPedPropRequestGfx::Pool* pPropReqPool = CPedPropRequestGfx::GetPool();

	if (pPropReqPool->GetNoOfUsedSpaces() == 0)
		return;

	CPedPropRequestGfx* pPropReq = NULL;
	CPedPropRequestGfx* pPropReqNext = NULL;
	s32 poolSize = pPropReqPool->GetSize();

	pPropReqNext = pPropReqPool->GetSlot(0);

	s32 i = 0;
	while (i < poolSize)
	{
		pPropReq = pPropReqNext;

		// spin to next valid entry
		while((++i < poolSize) && (pPropReqPool->GetSlot(i) == NULL));

		if (i != poolSize)
			pPropReqNext = pPropReqPool->GetSlot(i);

		ProcessSingleStreamRequest(pPropReq);
	}
}

bool CPedPropsMgr::HasProcessedAllRequests()
{
	CPedPropRequestGfx::Pool* pPropReqPool = CPedPropRequestGfx::GetPool();

	return (pPropReqPool->GetNoOfUsedSpaces() == 0);		
}

void CPedPropsMgr::ProcessSingleStreamRequest(CPedPropRequestGfx* pPropReq)
{
	if (pPropReq && pPropReq->HaveAllLoaded() && pPropReq->IsSyncGfxLoaded())
	{
		CPedStreamRequestGfx* streamGfx = pPropReq->GetWaitForGfx();

		// apply the files we requested and which have now all loaded to the target ped
		CPed* pPlayer =  pPropReq->GetTargetEntity();
		ApplyStreamPropFiles(pPlayer, pPropReq);
		pPlayer->SetPropRequestGfx(NULL);

		CPedVariationStream::ProcessSingleStreamRequest(streamGfx);
	}
}

// check if expressions need modified because of the prop state of this ped
void CPedPropsMgr::UpdatePropExpressions(CPed* pPed)
{
	if (pPed)
	{
		if (CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo())
		{
			atFixedArray<SPedDLCMetaFileQueryData, STREAMING_MAX_DEPENDENCIES> creatureIndicesToLoad;
			EXTRAMETADATAMGR.GetCreatureMetaDataIndices(pPedModelInfo, creatureIndicesToLoad);

		#if __ASSERT
			for (u32 i = 0; i < creatureIndicesToLoad.GetCount(); i++)
			{
				Assertf(g_fwMetaDataStore.HasObjectLoaded(pPedModelInfo->GetCreatureMetadataFileIndex()), 
					"About to InvalidateExternallyDrivenDOFs but meta file is not loaded! %i", creatureIndicesToLoad[i].m_storeIndex);
			}
		#endif

			if (creatureIndicesToLoad.GetCount() > 0)
				pPed->InvalidateExternallyDrivenDOFs();
		}
		else
			Assertf(false, "pPedModelInfo is NULL!");
	}
}

//--------------CPedPropData class------------

CPedPropData::CPedPropData(void)
{
	// init prop data
	ClearAllPedProps(NULL);
	m_preloadData = NULL;
}
CPedPropData::~CPedPropData(void)
{
	CleanUpPreloadProps();
}

CPedPropData::SinglePropData::SinglePropData() :
m_anchorID(ANCHOR_NONE),
m_propModelID(PED_PROP_NONE),
m_propTexID(-1),
m_propModelHash(0),
m_propTexHash(0),
m_anchorOverrideID(ANCHOR_NONE),
m_delayedDelete(false)
{
	m_posOffset.Set(0.0f, 0.0f, 0.0f);
	m_rotOffset.Identity();
}
#endif // !__SPU

void CPedPropData::DelayedDelete(u32 idx)
{
	if (idx < MAX_PROPS_PER_PED && m_aPropData[idx].m_delayedDelete)
	{
		m_aPropData[idx].m_anchorID = ANCHOR_NONE;
		m_aPropData[idx].m_propModelID = -1;
		m_aPropData[idx].m_propTexID = -1;
		m_aPropData[idx].m_propModelHash = 0;
		m_aPropData[idx].m_propTexHash = 0;
		m_aPropData[idx].m_anchorOverrideID = ANCHOR_NONE;
		m_aPropData[idx].m_posOffset.Set(0.0f, 0.0f, 0.0f);
		m_aPropData[idx].m_rotOffset.Identity();
		m_aPropData[idx].m_delayedDelete = false;
	}
}

eAnchorPoints	CPedPropData::GetPedPropActualAnchorPoint(u32 idx) const
{
	const SinglePropData& singlePropData = GetPedPropData(idx);
	return(singlePropData.m_anchorOverrideID == ANCHOR_NONE ? singlePropData.m_anchorID : singlePropData.m_anchorOverrideID);
}

eAnchorPoints	CPedPropData::GetPedPropActualAnchorPoint(eAnchorPoints anchorID) const
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID){
			return(GetPedPropActualAnchorPoint(i));
		}
	}

	return(ANCHOR_NONE);
}

bool	CPedPropData::GetPedPropPosOffset(eAnchorPoints anchorID, Vector3& vec)
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID){
			vec = m_aPropData[i].m_posOffset;
			return(true);
		}
	}

	return(false);
}

bool	CPedPropData::GetPedPropRotOffset(eAnchorPoints anchorID, Quaternion& quat)
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID){
			quat = m_aPropData[i].m_rotOffset;
			return(true);
		}
	}

	return(false);
}

u8	CPedPropData::GetNumActiveProps() const
{
	u8 numProps = 0;

	for(s32 i=0;i<MAX_PROPS_PER_PED;i++){
		if (m_aPropData[i].m_anchorID != ANCHOR_NONE){
			numProps++;
		}
	}

	return numProps;
}

bool	CPedPropData::GetPedPropHashes(eAnchorPoints anchorID, u32& modelHash, u32& texHash)
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID){
			modelHash = m_aPropData[i].m_propModelHash;
			texHash = m_aPropData[i].m_propTexHash;
			return(true);
		}
	}

	return(false);
}

s32	CPedPropData::GetPedPropIdx(eAnchorPoints anchorID) const
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID && !m_aPropData[i].m_delayedDelete){
			return(m_aPropData[i].m_propModelID);
		}
	}

	return(-1);
}

s32	CPedPropData::GetPedPropTexIdx(eAnchorPoints anchorID) const
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID && !m_aPropData[i].m_delayedDelete){
			return(m_aPropData[i].m_propTexID);
		}
	}

	return(-1);
}

bool CPedPropData::GetIsAnyPedPropDelayedDeleted(void) const
{
	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_delayedDelete){
			return true;
		}
	}	

	return false;
}

bool CPedPropData::GetIsPedPropDelayedDelete(eAnchorPoints anchorID) const
{
	Assert(anchorID < NUM_ANCHORS);

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (m_aPropData[i].m_anchorID == anchorID){
			return(m_aPropData[i].m_delayedDelete);
		}
	}

	return false;
}

void CPedPropData::SetPedProp(CPed* pPed, eAnchorPoints anchorID, s32 propID, s32 texID, eAnchorPoints anchorOverrideID, Vector3 *pPosOffset, Quaternion *pRotOffset, u32 streamingFlags)
{
	if (!pPed)
		return;

	if (!Verifyf(anchorID < NUM_ANCHORS, "Invalid anchor id %d used to set prop on ped '%s'", anchorID, pPed->GetModelName()))
		return;
	if (!Verifyf(anchorID != ANCHOR_NONE, "CPedPropData::SetPedProp - shouldn't call this with ANCHOR_NONE. Maybe try CPedPropData::ClearPedProp"))
		return;

	CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
	const u32 maxNumProps = pPedModelInfo->GetVarInfo()->GetMaxNumProps(anchorID);

	if (!Verifyf(propID >= 0 && propID < maxNumProps, "Invalid prop id %d (anchor %d) when setting prop on ped '%s'", propID, anchorID, pPed->GetModelName()))
		return;

	const u32 maxNumTex = pPedModelInfo->GetVarInfo()->GetMaxNumPropsTex(anchorID, propID);
	if (!Verifyf(texID < maxNumTex, "Tried to set texture index %d on %s_%03d. Maximum texture allowed is %d on ped '%s'", texID, propSlotNames[anchorID], propID, maxNumTex, pPed->GetModelName()))
		return;

	s32 freeSlot = -1;
	s32 oldProp = -1;

	// look for a prop already in the desired slot and modify it
	// don't reuse the slot if the override anchor isn't none and it's currently being deleted.
	for(s32 i=0;i<MAX_PROPS_PER_PED;i++){
		if (m_aPropData[i].m_anchorID == anchorID && (m_aPropData[i].m_anchorOverrideID == ANCHOR_NONE || !m_aPropData[i].m_delayedDelete)){
			freeSlot = i;
			oldProp = m_aPropData[i].m_propModelID;
			break;
		}
	}

	// else look for a free slot to put the prop in
	if (freeSlot == -1){
		for(s32 i=0;i<MAX_PROPS_PER_PED;i++){
			if (m_aPropData[i].m_anchorID == ANCHOR_NONE || (m_aPropData[i].m_anchorOverrideID == ANCHOR_NONE && m_aPropData[i].m_delayedDelete)){
				freeSlot = i;
				break;
			}
		}
	}

	// else couldn't find a free slot - bail
	if (freeSlot == -1){
		// Dump props
		Warningf("Dumping props:");
		for(s32 i=0;i<MAX_PROPS_PER_PED;i++)
		{
			Displayf("%d: Prop model %d (tex %d, hash 0x%08x, texhash 0x%08x), anchor=%d, anchorOverride=%d, delayedDelete=%s",
				i, m_aPropData[i].m_propModelID, m_aPropData[i].m_propTexID, m_aPropData[i].m_propModelHash, m_aPropData[i].m_propTexHash,
				m_aPropData[i].m_anchorID, m_aPropData[i].m_anchorOverrideID, m_aPropData[i].m_delayedDelete ? "true" : "false");
		}
		Assertf((freeSlot != -1), "No free slot for ped prop (model %d, tex %d, anchor %d, override %d), trying to set more than %d props on one ped",
			propID, texID, anchorID, anchorOverrideID, MAX_PROPS_PER_PED);
		return;
	}

	bool isStreamed = pPedModelInfo->GetIsStreamedGfx() && strcmp(pPed->GetPedModelInfo()->GetPropStreamFolder(), "null");

	m_aPropData[freeSlot].m_anchorID = anchorID;
	m_aPropData[freeSlot].m_propModelID = propID;
	m_aPropData[freeSlot].m_propTexID = texID;
	m_aPropData[freeSlot].m_delayedDelete = false;

	m_bSkipRender = false;

#if __ASSERT
    char assertDwdName[80] = {0};
    char assertTxdName[80] = {0};
#endif // __ASSERT

	char dlcPostFix[STORE_NAME_LENGTH] = {0};
    if (isStreamed)
    {
        if (const char* dlcName = pPedModelInfo->GetVarInfo()->GetDlcNameFromPropIdx((eAnchorPoints)anchorID, propID))
            formatf(dlcPostFix, "_%s", dlcName);
        else
            safecpy(dlcPostFix, "");
    }

	s32 localPropIdx = pPedModelInfo->GetVarInfo()->GetDlcPropIdx((eAnchorPoints)anchorID, propID);

	//need to generate the name of the desired prop from slot and prop index
	char	sourceName[80];
	if (isStreamed)
		sprintf(sourceName,"%s%s/%s_%03d",pPedModelInfo->GetPropStreamFolder(), dlcPostFix, propSlotNames[m_aPropData[freeSlot].m_anchorID], localPropIdx);
	else
		sprintf(sourceName,"%s_%03d",propSlotNames[m_aPropData[freeSlot].m_anchorID], localPropIdx);
	m_aPropData[freeSlot].m_propModelHash = atStringHash(sourceName);

    ASSERT_ONLY(safecpy(assertDwdName, sourceName));

	s32 texIdx = m_aPropData[freeSlot].m_propTexID;
    if (texIdx == -1)
        texIdx = 0;

	if (isStreamed)
		sprintf(sourceName, "%s%s/%s_diff_%03d_%c", pPedModelInfo->GetPropStreamFolder(), dlcPostFix, propSlotNames[m_aPropData[freeSlot].m_anchorID], localPropIdx, (texIdx+'a'));
	else
		sprintf(sourceName, "%s_diff_%03d_%c", propSlotNames[m_aPropData[freeSlot].m_anchorID], localPropIdx, (texIdx+'a'));
	m_aPropData[freeSlot].m_propTexHash = atStringHash(sourceName);

    ASSERT_ONLY(safecpy(assertTxdName, sourceName));

	m_aPropData[freeSlot].m_anchorOverrideID = anchorOverrideID;
	if (m_aPropData[freeSlot].m_anchorOverrideID != ANCHOR_NONE){

		if (pPosOffset)
			m_aPropData[freeSlot].m_posOffset = *pPosOffset;
		else
			m_aPropData[freeSlot].m_posOffset.Set(0.0f, 0.0f, 0.0f);

		if (pRotOffset)
			m_aPropData[freeSlot].m_rotOffset = *pRotOffset;
		else
			m_aPropData[freeSlot].m_rotOffset.Identity();
	}

	// trigger any alternate drawables on the ped if required by this new prop
	const CAlternateVariations::sAlternateVariationPed* altPedVar = CPedVariationStream::GetAlternateVariations().GetAltPedVar(pPedModelInfo->GetModelNameHash());
	if (altPedVar)
	{
		CPedVariationData& pedVarData = pPed->GetPedDrawHandler().GetVarData();
		CPedPropData& pedPropData = pPed->GetPedDrawHandler().GetPropData();

		CAlternateVariations::sAlternates* altStorage = Alloca(CAlternateVariations::sAlternates, MAX_NUM_ALTERNATES);
		atUserArray<CAlternateVariations::sAlternates> alts(altStorage, MAX_NUM_ALTERNATES);

		UndoAlternateDrawable(pPed, anchorID, oldProp);

		CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();

		u32 dlcNameHash = pVarInfo->GetDlcNameHashFromPropIdx(anchorID, propID);

		// check if the variation we set now needs to change a different slot
		if (altPedVar->GetAlternatesFromProp((u8)anchorID, (u8)localPropIdx, dlcNameHash, alts))
		{
			for (s32 i = 0; i < alts.GetCount(); ++i)
			{
				Assert(alts[i].altComp != 0xff && alts[i].altIndex != 0xff && alts[i].alt != 0xff);

				if (Verifyf(alts[i].alt != 0, "An alternate variation metadata entry specifies a 0 alternate! (%s)", pPedModelInfo->GetModelName()))
				{
					u32 compIdx = pedVarData.GetPedCompIdx((ePedVarComp)alts[i].altComp);
					u32 globalAltDrawIdx = pVarInfo->GetGlobalDrawableIndex(alts[i].altIndex, (ePedVarComp)alts[i].altComp, alts[i].m_dlcNameHash);

					if (compIdx == globalAltDrawIdx)
					{
						u8 texIdx = pedVarData.GetPedTexIdx((ePedVarComp)alts[i].altComp);
						u8 paletteIdx = pedVarData.GetPedPaletteIdx((ePedVarComp)alts[i].altComp);
						CPedVariationStream::SetVariation(pPed, pedPropData, pedVarData, (ePedVarComp)alts[i].altComp, globalAltDrawIdx, alts[i].alt, texIdx, paletteIdx, 0, false);
					}
				}
			}
		}
	}


	// streaming requests
	if (isStreamed)
	{
		CPedPropRequestGfx* pNewStreamGfx = rage_new CPedPropRequestGfx;
		Assert(pNewStreamGfx);
		pPed->SetPropRequestGfx(pNewStreamGfx);

		for (s32 i = 0; i < MAX_PROPS_PER_PED; ++i)
		{
			if (m_aPropData[i].m_anchorID != ANCHOR_NONE)
			{
                bool failed = false;
				if (g_DwdStore.FindSlot(m_aPropData[i].m_propModelHash) == -1)
                    failed = true;
				if (g_TxdStore.FindSlot(m_aPropData[i].m_propTexHash) == -1)
                    failed = true;

                // if any asset is missing we clear this prop
                if (failed)
                {
                    m_aPropData[i].m_anchorID = ANCHOR_NONE;
                    m_aPropData[i].m_propModelID = -1;
                    m_aPropData[i].m_propTexID = -1;
                    m_aPropData[i].m_propModelHash = 0;
                    m_aPropData[i].m_propTexHash = 0;
                    m_aPropData[i].m_anchorOverrideID = ANCHOR_NONE;
                    m_aPropData[i].m_posOffset.Set(0.0f, 0.0f, 0.0f);
                    m_aPropData[i].m_rotOffset.Identity();
					m_aPropData[i].m_delayedDelete = false;
                    continue;
                }

				pNewStreamGfx->AddDwd(i, m_aPropData[i].m_propModelHash, streamingFlags, pPedModelInfo);
				pNewStreamGfx->AddTxd(i, m_aPropData[i].m_propTexHash, streamingFlags, pPedModelInfo);
			}
		}

		pNewStreamGfx->PushRequest();
	}

	CPedPropsMgr::UpdatePropExpressions(pPed);
}

void CPedPropData::UndoAlternateDrawable(CPed* pPed, s32 anchor, s32 oldProp)
{
	const CAlternateVariations::sAlternateVariationPed* altPedVar = CPedVariationStream::GetAlternateVariations().GetAltPedVar(pPed->GetPedModelInfo()->GetModelNameHash());
	if (!altPedVar)
		return;

	CAlternateVariations::sAlternates* altStorage = Alloca(CAlternateVariations::sAlternates, MAX_NUM_ALTERNATES);
	atUserArray<CAlternateVariations::sAlternates> alts(altStorage, MAX_NUM_ALTERNATES);

	CPedVariationInfoCollection* pVarInfo = pPed->GetPedModelInfo()->GetVarInfo();

	s32 oldLocalPropIdx = pVarInfo->GetDlcPropIdx((eAnchorPoints)anchor, oldProp);
	u32 oldDlcNameHash = pVarInfo->GetDlcNameHashFromPropIdx((eAnchorPoints)anchor, oldProp);

	CPedVariationData& pedVarData = pPed->GetPedDrawHandler().GetVarData();

	bool requestAssets = false;

	// check if the variation we replace forced an alternate drawable when it was set. that means we need to undo that now
	if (oldLocalPropIdx != -1 && altPedVar->GetAlternatesFromProp((u8)anchor, (u8)oldLocalPropIdx, oldDlcNameHash, alts))
	{
		for (s32 i = 0; i < alts.GetCount(); ++i)
		{
			Assert(alts[i].altComp != 0xff && alts[i].altIndex != 0xff && alts[i].alt != 0xff);

			u32 globalAltDrawIdx = pVarInfo->GetGlobalDrawableIndex(alts[i].altIndex, (ePedVarComp)alts[i].altComp, alts[i].m_dlcNameHash);

			// the current asset had an alternate, check if we currently have this alternate and if we do, replace it with the original
			if (pedVarData.GetPedCompIdx((ePedVarComp)alts[i].altComp) == globalAltDrawIdx && pedVarData.GetPedCompAltIdx((ePedVarComp)alts[i].altComp) == alts[i].alt)
			{
				CAlternateVariations::sAlternates *ccAltStorage = Alloca(CAlternateVariations::sAlternates, MAX_NUM_ALTERNATES);

				bool bStillNeeded = false;

				// Before we revert this alt, first check no other components require it
				for (u8 checkComponent = 0; checkComponent < PV_MAX_COMP; ++checkComponent) {
					// If this check component has a drawable, so get its DLC hash and local index
					if (pedVarData.m_aDrawblId[checkComponent]) {
						// Convert index to DLC hash/local index pair
						u32 ccLocalDLCHash		=		pVarInfo->GetDlcNameHashFromDrawableIdx	((ePedVarComp)checkComponent, pedVarData.m_aDrawblId[checkComponent]);
						u8	ccLocalDrawableIdx	= (u8)	pVarInfo->GetDlcDrawableIdx				((ePedVarComp)checkComponent, pedVarData.m_aDrawblId[checkComponent]);

						// Get a list of alternates the checked component's drawable wants
						atUserArray<CAlternateVariations::sAlternates> ccAlternates(ccAltStorage, MAX_NUM_ALTERNATES);
						altPedVar->GetAlternates(checkComponent, ccLocalDrawableIdx, ccLocalDLCHash, ccAlternates);

						// For each alternate, check it's not the one we're trying to revert!
						for (u8 j = 0; j < ccAlternates.GetCount(); ++j) {
							if (ccAlternates[j].alt == alts[i].alt && ccAlternates[j].m_dlcNameHash == alts[i].m_dlcNameHash) {
								bStillNeeded = true; // jog on, this alternate is still in use!
								break;
							}
						}
					}	

					if (bStillNeeded) {
						break;
					}
				}
				
				if (!bStillNeeded) {
					u8 texIdx = pedVarData.GetPedTexIdx((ePedVarComp)alts[i].altComp);
					u8 paletteIdx = pedVarData.GetPedPaletteIdx((ePedVarComp)alts[i].altComp);
					pedVarData.SetPedVariation((ePedVarComp)alts[i].altComp, globalAltDrawIdx, 0, texIdx, paletteIdx, NULL);
					requestAssets = true;
				}
			}
		}
	}

	if (requestAssets)
		CPedVariationStream::RequestStreamPedFiles(pPed, &pedVarData, 0);
}

u32 CPedPropData::SetPreloadProp(CPed* pPed, eAnchorPoints anchorID, s32 propID, s32 texID, u32 streamingFlags)
{
	if (!pPed)
		return 0;

	if (!Verifyf(anchorID < NUM_ANCHORS, "Invalid anchor id %d used to preload prop on ped '%s'", anchorID, pPed->GetModelName()))
		return 0;
	if (!Verifyf(anchorID != ANCHOR_NONE, "CPedPropData::SetPreloadProp - shouldn't call this with ANCHOR_NONE."))
		return 0;
	CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
	bool isStreamed = pPedModelInfo->GetIsStreamedGfx() && strcmp(pPedModelInfo->GetPropStreamFolder(), "null");
	if (!isStreamed)
		return 0;

	const u32 maxNumProps = pPedModelInfo->GetVarInfo()->GetMaxNumProps(anchorID);

	if (!Verifyf(propID >= 0 && propID < maxNumProps, "Invalid prop id %d (anchor %d) when preloading prop on ped '%s'", propID, anchorID, pPed->GetModelName()))
		return 0;

	const u32 maxNumTex = pPedModelInfo->GetVarInfo()->GetMaxNumPropsTex(anchorID, propID);
	if (!Verifyf(texID < maxNumTex, "Too large tex id %i (max - %i) used to preload ped prop on ped '%s'",texID, maxNumTex, pPed->GetModelName()))
		return 0;

	bool addToScriptMem = !pPedModelInfo->GetIsPlayerModel();

	if (!m_preloadData)
		m_preloadData = rage_new CScriptPreloadData(MAX_PROPS_PER_PED * SCRIPT_PRELOAD_MULTIPLIER, MAX_PROPS_PER_PED * SCRIPT_PRELOAD_MULTIPLIER, 0, 0, 0, addToScriptMem);

	char sourceName[80];
	char dlcPostFix[STORE_NAME_LENGTH] = {0};
	if (isStreamed)
	{
		if (const char* dlcName = pPedModelInfo->GetVarInfo()->GetDlcNameFromPropIdx(anchorID, propID))
			formatf(dlcPostFix, "_%s", dlcName);
		else
			safecpy(dlcPostFix, "");
	}

	propID = pPedModelInfo->GetVarInfo()->GetDlcPropIdx((eAnchorPoints)anchorID, propID);

	sprintf(sourceName,"%s%s/%s_%03d",pPedModelInfo->GetPropStreamFolder(), dlcPostFix, propSlotNames[anchorID], propID);
	u32 dwdHash = atStringHash(sourceName);
    Assertf(g_DwdStore.FindSlot(dwdHash) != -1, "Missing ped prop drawable %s", sourceName);

	if (texID == -1)
		texID = 0;

	sprintf(sourceName, "%s%s/%s_diff_%03d_%c", pPedModelInfo->GetPropStreamFolder(), dlcPostFix, propSlotNames[anchorID], propID, (texID + 'a'));
	u32 txdHash = atStringHash(sourceName);
    Assertf(g_TxdStore.FindSlot(txdHash) != -1, "Missing ped prop texture %s", sourceName);

	s32 txdSlot = -1;
	s32 dwdSlot = -1;
	bool slotFound = true;

	slotFound &= m_preloadData->AddTxd(g_TxdStore.FindSlot(txdHash), txdSlot, streamingFlags);
	slotFound &= m_preloadData->AddDwd(g_DwdStore.FindSlot(dwdHash), CPedModelInfo::GetCommonTxd(), dwdSlot, streamingFlags);

	Assertf(slotFound, "No more prop preload slots available for '%s', please clean up old ones first!", pPedModelInfo->GetModelName());
	return m_preloadData->GetHandle(dwdSlot, txdSlot, -1);
}

void CPedPropData::CleanUpPreloadProps()
{
	delete m_preloadData;
	m_preloadData = NULL;
}

void CPedPropData::CleanUpPreloadPropsHandle(u32 handle)
{
    if (m_preloadData)
        m_preloadData->ReleaseHandle(handle);
}

bool CPedPropData::HasPreloadPropsFinished()
{
    if (!m_preloadData)
        return true;
    return m_preloadData->HasPreloadFinished();
}

bool CPedPropData::HasPreloadPropsFinished(u32 handle)
{
    if (!m_preloadData)
        return true;
    return m_preloadData->HasPreloadFinished(handle);
}

// clear this slot for the ped.
void	CPedPropData::ClearPedProp(CPed* ped, eAnchorPoints anchorID, bool UNUSED_PARAM(bForceClear))
{
	Assert(anchorID < NUM_ANCHORS);

	if (!ped)
		return;

	bool isStreamed = ped->GetPedModelInfo()->GetIsStreamedGfx() && strcmp(ped->GetPedModelInfo()->GetPropStreamFolder(), "null");

	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		if (m_aPropData[i].m_anchorID == anchorID)
		{
			if (isStreamed)
			{
				m_aPropData[i].m_delayedDelete = true;

				if (m_aPropData[i].m_anchorID != ANCHOR_NONE)
					UndoAlternateDrawable(ped, anchorID, m_aPropData[i].m_propModelID);
			}
			else
			{
				m_aPropData[i].m_anchorID = ANCHOR_NONE;
				m_aPropData[i].m_propModelID = -1;
				m_aPropData[i].m_propTexID = -1;
				m_aPropData[i].m_propModelHash = 0;
				m_aPropData[i].m_propTexHash = 0;
				m_aPropData[i].m_anchorOverrideID = ANCHOR_NONE;
				m_aPropData[i].m_posOffset.Set(0.0f, 0.0f, 0.0f);
				m_aPropData[i].m_rotOffset.Identity();
				m_aPropData[i].m_delayedDelete = false;
			}
		}
	}

	if (isStreamed)
	{
		// if we have any props left on the ped re-request them to clear refs for the removed props
		// in the case where there is no props left, still need to request an empty gfx to handle detached props
		CPedPropRequestGfx* pNewStreamGfx = rage_new CPedPropRequestGfx;
		Assert(pNewStreamGfx);
		ped->SetPropRequestGfx(pNewStreamGfx);

		for (s32 i = 0; i < MAX_PROPS_PER_PED; ++i)
		{
			if (m_aPropData[i].m_anchorID != ANCHOR_NONE && !m_aPropData[i].m_delayedDelete)
			{
				pNewStreamGfx->AddDwd(i, m_aPropData[i].m_propModelHash, 0, ped->GetPedModelInfo());
				pNewStreamGfx->AddTxd(i, m_aPropData[i].m_propTexHash, 0, ped->GetPedModelInfo());
			}
		}

		pNewStreamGfx->PushRequest();
	}
}

void	CPedPropData::ClearAllPedProps(CPed* ped)
{
	if (!ped)
		return;

	bool isStreamed = ped->GetPedModelInfo()->GetIsStreamedGfx() && strcmp(ped->GetPedModelInfo()->GetPropStreamFolder(), "null");

	for(u32 i=0;i<MAX_PROPS_PER_PED;i++)
	{
		if (isStreamed)
		{
			m_aPropData[i].m_delayedDelete = true;

			if (m_aPropData[i].m_anchorID != ANCHOR_NONE)
				UndoAlternateDrawable(ped, m_aPropData[i].m_anchorID, m_aPropData[i].m_propModelID);
		}
		else
		{
            m_aPropData[i].m_anchorID = ANCHOR_NONE;
            m_aPropData[i].m_propModelID = -1;
            m_aPropData[i].m_propTexID = -1;
            m_aPropData[i].m_propModelHash = 0;
            m_aPropData[i].m_propTexHash = 0;
            m_aPropData[i].m_anchorOverrideID = ANCHOR_NONE;
            m_aPropData[i].m_posOffset.Set(0.0f, 0.0f, 0.0f);
            m_aPropData[i].m_rotOffset.Identity();
			m_aPropData[i].m_delayedDelete = false;
		}
	}

	m_bIsCurrPropsAlpha = false;
	m_bIsCurrPropsDecal = false;
	m_bIsCurrPropsCutout= false;
	m_bSkipRender = false;

	if (isStreamed)
	{
		CPedDrawHandler* pdh = &ped->GetPedDrawHandler();
		if (pdh)
		{
			CPedPropRequestGfx* pNewStreamGfx = rage_new CPedPropRequestGfx;
			Assert(pNewStreamGfx);
			ped->SetPropRequestGfx(pNewStreamGfx);
			pNewStreamGfx->PushRequest();
			//pdh->SetPedPropRenderGfx(NULL);
		}
	}
}

//----------------CPedPropInfo class----------------


CPedPropInfo::CPedPropInfo(void) {
	m_numAvailProps = 0;
}

CPedPropInfo::~CPedPropInfo(void)
{
}

bool CPedVariationInfoCollection::InitAnchorPoints(crSkeletonData *pSkelData)
{
	u32	i,j;
	u32	numBones = pSkelData->GetNumBones();
	const char*	pBoneName = NULL;

	for(j=0;j<NUM_ANCHORS;j++)
	{
		for(i=0;i<numBones;i++)
		{
			const crBoneData* pBoneData = pSkelData->GetBoneData(i);
			pBoneName = pBoneData->GetName();

			if (strcmp(pBoneName,AnchorNames[j])==0)
			{
				m_aAnchorBoneIdxs[j] = i;		//store the index for this bone
				break;
			}
		}
	}

	return(true);
}


#if __ASSERT
bool CPedVariationInfoCollection::ValidateProps(s32 DwdId, u8 varInfoIdx)
{
	char modelName[80];

	if (m_infos.GetCount() == 0)
		return false;

	Dwd* pDwd = g_DwdStore.Get(strLocalIndex(DwdId));
	if (pDwd == NULL)
		return false;

	u32 numTotalProps = 0;

	// need to generate and test model names & determine if they are in the dict
	for (u32 i=0; i < NUM_ANCHORS; ++i)
	{
		for (u32 j = 0; j < m_infos[varInfoIdx]->m_propInfo.GetMaxNumProps((eAnchorPoints)i); ++j)
		{
			// generate the model name
			sprintf(modelName,"%s_%03d",propSlotNames[i],j);
			rmcDrawable* pDrawable = pDwd->Lookup(modelName);

			// if the model name is in the dictionary then store this index for the drawable
			if (Verifyf(pDrawable, "No drawable found in dwd %s for prop %d on anchor %s found in the meta data!", g_DwdStore.GetName(strLocalIndex(DwdId)), j, propSlotNames[i]))
			{
				pDrawable->GetLodGroup().ComputeBucketMask(pDrawable->GetShaderGroup());

				if (pDrawable->GetBucketMask(0)&(0x01<<CRenderer::RB_ALPHA))
				{
					artAssertf(HasPropAlpha((u8)i, (u8)j), "Metadata for ped prop %d, anchor %s, dwd %s is not tagged correctly with alpha flag!", j, propSlotNames[i], g_DwdStore.GetName(strLocalIndex(DwdId)));
				}

				if (pDrawable->GetBucketMask(0)&(0x01<<CRenderer::RB_DECAL))
				{
					artAssertf(HasPropDecal((u8)i, (u8)j), "Metadata for ped prop %d, anchor %s, dwd %s is not tagged correctly with decal flag!", j, propSlotNames[i], g_DwdStore.GetName(strLocalIndex(DwdId)));
				}

				if (pDrawable->GetBucketMask(0)&(0x01<<CRenderer::RB_CUTOUT))
				{
					artAssertf(HasPropCutout((u8)i, (u8)j), "Metadata for ped prop %d, anchor %s, dwd %s is not tagged correctly with cutout flag!", j, propSlotNames[i], g_DwdStore.GetName(strLocalIndex(DwdId)));
				}

				numTotalProps++;
			}
		}
	}

	Assertf(numTotalProps == m_infos[varInfoIdx]->m_propInfo.m_numAvailProps, "Total props found in dwd %s doesn't match meta data!", g_DwdStore.GetName(strLocalIndex(DwdId)));

	return true;
}
#endif // __ASSERT

// scan through the folder in the stream ped prop file to determine what variation models and textures are available for this model
void CPedVariationInfoCollection::EnumTargetPropFolder(const char* pFolderName, CPedModelInfo* ASSERT_ONLY(pModelInfo))
{
	if ((pFolderName == NULL) || (pFolderName[0] == '\0'))
	{
		Assertf(false, "Attempting to enum a ped prop stream folder with no name for ped %s", pModelInfo->GetModelName());
		return;
	}

	// make sure we have the variation info so we can append to the first one
	if (m_infos.GetCount() == 0)
		return;

#if LIVE_STREAMING
	// char modelName[80];

	int passCount = 0;

	if (strStreamingEngine::GetLive().GetEnabled())
	{
		passCount += strStreamingEngine::GetLive().GetFolderCount();
	}

	for (int pass = 0; pass < passCount; pass++)
	{
		fiFindData find;
		fiHandle handle = fiHandleInvalid;
		// count files at root
		char	targetPath[200];
		const char *prefix = strStreamingEngine::GetLive().GetFolderPath(pass);

		formatf(targetPath,"%s%s/", prefix, pFolderName);

		const fiDevice* pDevice = NULL;

		if ((pDevice = fiDevice::GetDevice(targetPath)) != NULL)
		{
			handle = pDevice->FindFileBegin(targetPath, find);
		}

		if (fiIsValidHandle(handle))
		{
			do
			{
				// only interested in the textures
				const char* pExt = ASSET.FindExtensionInPath(find.m_Name);
				if (pExt && !strcmp(&pExt[1], TXD_FILE_EXT))
				{
					char anchorName[80];
					char texType[80];
					char texRace[16];
					s32 propIdx;
					char texIdx;
					s32 ret = sscanf(find.m_Name, "p_%[^_]_%[^_]_%d_%c_%[^.]", anchorName, texType, &propIdx, &texIdx, texRace);
					Assertf(ret != 5, "Bad ped prop texture name. Remove '_%s' from the texture '%s'.", texRace, find.m_Name);

					// only using the diffuse textures to enumerate the models and textures
					if ((ret == 4) && !strncmp(texType, "diff", 4))
					{
						texIdx -= 'a';		// convert char to int index

						// establish which component this texture is to be mapped onto
						eAnchorPoints anchorId = ANCHOR_NONE;
						for (s32 i = 0; i < NUM_ANCHORS; ++i)
						{
							if (!strcmp(anchorName, propSlotNamesClean[i]))
							{
								anchorId = propSlotEnums[i];
								break;
							}
						}

						if (anchorId == ANCHOR_NONE)
						{
							Printf("Invalid ped prop: Unknown anchor name: %s\n", anchorName);
							Assertf(anchorId != ANCHOR_NONE, "ped prop diff texture doesn't specify anchor name" );
							continue;
						}

						// TODO: this will need some patching up once we have some preview data
#if 0
						Assert(propIdx < MAX_PED_PROPS);

						// generate the tex name from our data
						sprintf(modelName, "%s/%s_%s_%03d_%c", pFolderName, propSlotNames[anchorId], texType, propIdx, texIdx + 'a');
						if (g_TxdStore.FindSlot(modelName) != -1 && texIdx >= m_infos[0]->m_propInfo.m_aMaxPropTexId[anchorId][propIdx]) // increment only if it's an addition
						{
							m_infos[0]->m_propInfo.m_aMaxPropTexId[anchorId][propIdx]++;
						}
#endif
					}
				}
				else if (pExt && !strcmp(&pExt[1], DWD_FILE_EXT))
				{
					char anchorName[80];
					s32 propIdx;
					s32 ret = sscanf(find.m_Name, "p_%[^_]_%d_%*[^.]", anchorName, &propIdx);

					if (ret == 2)
					{
						// establish which component this texture is to be mapped onto
						eAnchorPoints anchorId = ANCHOR_NONE;
						for (s32 i = 0; i < NUM_ANCHORS; ++i)
						{
							if (!strcmp(anchorName, propSlotNamesClean[i]))
							{
								anchorId = propSlotEnums[i];
								break;
							}
						}

						if (anchorId == ANCHOR_NONE)
						{
							Printf("Invalid ped prop: Unknown anchor name: %s\n", anchorName);
							Assertf(anchorId != ANCHOR_NONE, "ped prop drawable doesn't specify anchor name" );
							continue;
						}

						// TODO: this will need some patching up once we have some preview data
#if 0
						// generate the model name
						sprintf(modelName, "%s/%s_%03d", pFolderName, propSlotNames[anchorId], propIdx);
						if (g_DwdStore.FindSlot(modelName) != -1 && propIdx >= m_infos[0]->m_propInfo.m_aMaxPropId[anchorId])
						{
							m_infos[0]->m_propInfo.m_aMaxPropId[anchorId]++;		// inc no. of props found for this slot only if it's an addition to current assets
							m_infos[0]->m_propInfo.m_numAvailProps++;
						}
#endif
					}
				}
			} while (pDevice->FindFileNext(handle, find));

			pDevice->FindFileEnd(handle);
		}
	}
#endif // LIVE_STREAMING
}

// Name			:	CountTextures
// Purpose		:	generate texture names to determine how many textures are available for the
//					given prop in the given slot for the given drawable
// Parameters	:	ptr to drawable, prop slot to check, prop id for this slot to check, dictionary id
// Returns		:	no of available texture
u32 CPedVariationInfoCollection::CountTextures(u32 slotId, u32 propId, s32 DwdId) {

	char	texName[32];

	fwTxd* pTxd = g_TxdStore.Get(g_DwdStore.GetParentTxdForSlot(strLocalIndex(DwdId)));

	s32 i;
	for(i=0; i<MAX_PED_PROP_TEX; i++){
		sprintf(texName, "%s_diff_%03d_%c", propSlotNames[slotId], propId, (i+'a'));

		grcTexture* pRet = pTxd->Lookup(texName);

		if (!pRet){
			break;
		}
	}

	return(i);
}

const CPedPropMetaData* CPedVariationInfoCollection::GetPropCollectionMetaData(u32 anchorId, u32 propId) const
{
	const CPedVariationInfo* varInfo = GetVariationInfoFromPropIdx((eAnchorPoints)anchorId, propId);
	propId = GetDlcPropIdx((eAnchorPoints)anchorId, propId);

	if (Verifyf(varInfo, "NULL CPedvariationInfo on ped! anchorId=%i, propId=%i", anchorId, propId))
	{
		for (s32 f = 0; f < varInfo->m_propInfo.m_aPropMetaData.GetCount(); ++f)
		{
			if (varInfo->m_propInfo.m_aPropMetaData[f].anchorId == anchorId && varInfo->m_propInfo.m_aPropMetaData[f].propId == propId)
			{
				return &varInfo->m_propInfo.m_aPropMetaData[f];
			}
		}
		return NULL;
	}

	return NULL;
}

float CPedVariationInfoCollection::GetPropKnockOffProb(u32 anchorId, u32 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return 1.f - (meta->stickyness / 255.f);
	return 1.f;
}

const float* CPedVariationInfoCollection::GetExpressionMods(u32 anchorId, u32 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return meta->expressionMods;
	return NULL;
}

s32 CPedVariationInfoCollection::GetPropIdxWithFlags(u8 anchorId, u32 propFlags) const
{
	s32 index = 0;
	for (s32 i = 0; i < m_infos.GetCount(); ++i)
	{
		if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
		{
			s32 idx = m_infos[i]->m_propInfo.GetPropIdxWithFlags(anchorId, propFlags);
			if (idx == -1)
			{
				// if this variation info has no props with these flags, increment the index
				// to handle dlc indices correctly
				index += m_infos[i]->m_propInfo.GetMaxNumProps((eAnchorPoints)anchorId);
			}
			else
			{
				// if we found a prop, this might not be the first variation info so add the found index to our counter
				return index + idx;
			}
		}
	}
	return -1;
}

u32 CPedVariationInfoCollection::GetMaxNumProps(eAnchorPoints anchorPointIdx) const
{
	u32 numProps = 0;

	for (int i = 0; i < m_infos.GetCount(); i++)
		numProps += m_infos[i]->m_propInfo.GetMaxNumProps(anchorPointIdx);

	return numProps;
}

u32 CPedVariationInfoCollection::GetMaxNumPropsTex(eAnchorPoints anchorPointIdx, u32 propId)
{
	if (CPedVariationInfo* varInfo = GetVariationInfoFromPropIdx(anchorPointIdx, propId))
	{
		u32 localPropIdx = GetDlcPropIdx(anchorPointIdx, propId);
		return varInfo->m_propInfo.GetMaxNumPropsTex(anchorPointIdx, localPropIdx);
	}

	return 0;
}

u32 CPedVariationInfoCollection::GetPropFlags(u8 anchorId, u8 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
	{
		if (meta->propFlags != 0)
			return meta->propFlags;
		else
			return meta->flags;
	}
	return 0;
}

const CPedPropTexData* CPedVariationInfoCollection::GetPropTexData(u8 anchorId, u8 propId, u8 texId, const CPedPropMetaData** propData) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
	{
		for (u32 i = 0; i < meta->texData.GetCount(); ++i)
		{
			if (meta->texData[i].texId == texId)
			{
				if (propData)
				{
					*propData = meta;
				}

				return &meta->texData[i];
			}
		}
	}
	return NULL;
}

bool CPedVariationInfoCollection::HasAnyProps() const
{
	for (s32 i = 0; i < m_infos.GetCount(); ++i)
		if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
			if (m_infos[i]->m_propInfo.m_numAvailProps > 0)
				return true;
	return false;
}

bool CPedVariationInfoCollection::HasPropAlpha(u8 anchorId, u8 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return meta->renderFlags.IsSet(PRF_ALPHA);
	return false;
}

bool CPedVariationInfoCollection::HasPropDecal(u8 anchorId, u8 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return meta->renderFlags.IsSet(PRF_DECAL);
	return false;
}

bool CPedVariationInfoCollection::HasPropCutout(u8 anchorId, u8 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return meta->renderFlags.IsSet(PRF_CUTOUT);
	return false;
}

void CPedVariationInfoCollection::SetPropFlag(u8 anchorId, u8 propId, ePropRenderFlags flag, bool value)
{
	CPedPropMetaData* meta = (CPedPropMetaData*)GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		meta->renderFlags.Set(flag, value);
}

bool CPedVariationInfoCollection::HasHelmetProp() const
{
	for (s32 i = 0; i < m_infos.GetCount(); ++i)
		if (Verifyf(m_infos[i], "NULL CPedvariationInfo on ped! %d/%d", i, m_infos.GetCount()))
			if (m_infos[i]->m_propInfo.CheckFlagsOnAnyProp(ANCHOR_HEAD, PV_FLAG_DEFAULT_HELMET/*|PV_FLAG_RANDOM_HELMET|PV_FLAG_SCRIPT_HELMET*/))
				return true;
	return false;
}

atHashString CPedVariationInfoCollection::GetAudioId(u32 anchorId, u32 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return meta->audioId;
	return atHashString::Null();
}

u8 CPedVariationInfoCollection::GetPropStickyness(u8 anchorId, u8 propId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta)
		return meta->stickyness;
	return (u8)-1;
}

u8 CPedVariationInfoCollection::GetPropTexDistribution(u8 anchorId, u8 propId, u8 texId) const
{
	const CPedPropMetaData* meta = GetPropCollectionMetaData(anchorId, propId);
	if (meta && texId < meta->texData.GetCount())
		return meta->texData[texId].distribution;
	return (u8)-1;
}

// returns true if any of the flags specified are found on any of the props on the given anchor
bool CPedPropInfo::CheckFlagsOnAnyProp(u8 anchorId, u32 flags) const
{
	for (u32 i = 0; i < m_aPropMetaData.GetCount(); ++i)
	{
		if (m_aPropMetaData[i].anchorId == anchorId)
		{
			if (m_aPropMetaData[i].propFlags != 0)
			{
				if ((m_aPropMetaData[i].propFlags & flags) != 0)
					return true;
			}
			else
			{
				if ((m_aPropMetaData[i].flags & flags) != 0)
					return true;
			}
		}
	}

	return false;
}

const atFixedBitSet<32, u32>* CPedPropInfo::GetInclusions(u8 anchorId, u8 propId, u8 texId) const
{
	for (s32 i = 0; i < m_aPropMetaData.GetCount(); ++i)
	{
		if (m_aPropMetaData[i].anchorId == anchorId && m_aPropMetaData[i].propId == propId)
		{
            if (texId < m_aPropMetaData[i].texData.GetCount())
            {
                return &m_aPropMetaData[i].texData[texId].inclusions;
            }
		}
	}

	return NULL;
}

const atFixedBitSet<32, u32>* CPedPropInfo::GetExclusions(u8 anchorId, u8 propId, u8 texId) const
{
	for (s32 i = 0; i < m_aPropMetaData.GetCount(); ++i)
	{
		if (m_aPropMetaData[i].anchorId == anchorId && m_aPropMetaData[i].propId == propId)
		{
            if (texId < m_aPropMetaData[i].texData.GetCount())
            {
                return &m_aPropMetaData[i].texData[texId].exclusions;
            }
		}
	}

	return NULL;
}

s32 CPedPropInfo::GetPropIdxWithFlags(u8 anchorId, u32 propFlags) const
{
	for (int i = 0; i < m_aPropMetaData.GetCount(); ++i)
	{
		if (m_aPropMetaData[i].anchorId == anchorId)
		{
			if (m_aPropMetaData[i].propFlags != 0)
			{
				if ((m_aPropMetaData[i].propFlags & propFlags) == propFlags)
					return m_aPropMetaData[i].propId;
			}
			else
			{
				if ((m_aPropMetaData[i].flags & propFlags) == propFlags)
					return m_aPropMetaData[i].propId;
			}
		}
	}
 
	return -1;
}

CPedPropRequestGfx::CPedPropRequestGfx() 
{ 
	m_pTargetEntity = NULL; 
	m_state = PS_INIT; 
	m_bDelayCompletion = false;

	m_waitForGfx = NULL;

	m_reqTxds.UseAsArray();
	m_reqDwds.UseAsArray();
}

CPedPropRequestGfx::~CPedPropRequestGfx()
{
	Release();
	Assert(m_state == PS_FREED);
}

void CPedPropRequestGfx::AddTxd(u32 componentSlot, const strStreamingObjectName txdName, s32 streamFlags, CPedModelInfo* ASSERT_ONLY(pPedModelInfo))
{
	s32 txdSlotId = g_TxdStore.FindSlot(txdName).Get();

	if (txdSlotId != -1){
		m_reqTxds.SetRequest(componentSlot, txdSlotId, g_TxdStore.GetStreamingModuleId(), streamFlags);
	} else {
		Assertf(false,"Streamed .txd request for ped '%s' is not found : %s", pPedModelInfo->GetModelName(), txdName.TryGetCStr());
	}
}

void CPedPropRequestGfx::AddDwd(u32 componentSlot, const strStreamingObjectName dwdName, s32 streamFlags, CPedModelInfo* ASSERT_ONLY(pPedModelInfo))
{
	strLocalIndex dwdSlotId = strLocalIndex(g_DwdStore.FindSlot(dwdName));

	if (dwdSlotId != -1){
		g_DwdStore.SetParentTxdForSlot(dwdSlotId, strLocalIndex(CPedModelInfo::GetCommonTxd()));	
		m_reqDwds.SetRequest(componentSlot,dwdSlotId.Get(), g_DwdStore.GetStreamingModuleId(), streamFlags);
	}else {
		Assertf(false,"Streamed .dwd request for ped '%s' is not found : %s", pPedModelInfo->GetModelName(), dwdName.TryGetCStr());
	}
}

void CPedPropRequestGfx::Release(void)
{
	m_reqTxds.ReleaseAll();
	m_reqDwds.ReleaseAll();

	m_state = PS_FREED;
}

// requests have completed - add refs to the slots we wanted and free up the initial requests
void CPedPropRequestGfx::RequestsComplete()
{ 
	Assert(m_state == PS_REQ);

	CPed* pTarget = GetTargetEntity();

	Assert(pTarget->GetDrawHandlerPtr()); // Do we ever not have a draw handler when we set this up?

	// if we already have graphics data then dump it

	CPedDrawHandler* pDrawHandler = &pTarget->GetPedDrawHandler();

#if !__NO_OUTPUT
	if (CPedPropRenderGfx::GetPool()->GetNoOfFreeSpaces() <= 0)
	{
		Displayf("Ran out of pool entries for CPedPropRenderGfx frame %d", fwTimer::GetFrameCount());
		for (s32 i = 0; i < CPedPropRenderGfx::GetPool()->GetSize(); ++i)
		{
			CPedPropRenderGfx* gfx = CPedPropRenderGfx::GetPool()->GetSlot(i);
			if (gfx)
			{
				Displayf("CPedPropRenderGfx %d / %d: %s (%p) (%s) (%d refs) (fc: %d)\n", i, CPedPropRenderGfx::GetPool()->GetSize(), gfx->m_pTargetEntity ? gfx->m_pTargetEntity->GetModelName() : "NULL", gfx->m_pTargetEntity.Get(), gfx->m_used ? "used" : "not used", gfx->m_refCount, gfx->m_frameCountCreated);
			}
			else
			{
				Displayf("CPedPropRenderGfx %d / %d: null slot\n", i, CPedPropRenderGfx::GetPool()->GetSize());
			}
		}
		Quitf("Ran out of pool entries for CPedPropRenderGfx!");
	}
	else
#endif // __NO_OUTPUT //On final we want to hit the pool limit so it's a bit more obvious what went wrong
	{
		pDrawHandler->SetPedPropRenderGfx(rage_new CPedPropRenderGfx(this));
	}

	m_state = PS_USE;
}

void CPedPropRequestGfx::SetTargetEntity(CPed *pEntity)
{ 
	Assert(pEntity);
	m_pTargetEntity = pEntity; 
}

void CPedPropRequestGfx::ClearWaitForGfx()
{
    if (m_waitForGfx)
        m_waitForGfx->SetWaitForGfx(NULL);
	m_waitForGfx = NULL;
}

bool CPedPropRequestGfx::IsSyncGfxLoaded() const
{
	return !m_waitForGfx || m_waitForGfx->IsReady();
}

CPedPropRenderGfx::CPedPropRenderGfx() 
{
	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		m_dwdIdx[i] = -1;
		m_txdIdx[i] = -1;
	}

#if !__NO_OUTPUT
	m_pTargetEntity = NULL;
	m_frameCountCreated = fwTimer::GetFrameCount();
#endif

	m_blendIndex = (u16)-1;
	m_refCount = 0;
	m_used = true;
}

CPedPropRenderGfx::CPedPropRenderGfx(CPedPropRequestGfx *pReq)  
{
	Assert(pReq);

	if (pReq)
	{
		for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
		{
			strLocalIndex dwdSlot = strLocalIndex(pReq->GetDwdSlot(i));
			if (dwdSlot.IsValid()){
				g_DwdStore.AddRef(dwdSlot, REF_OTHER);
				m_dwdIdx[i] = dwdSlot.Get();
			}
			else
			{
				m_dwdIdx[i] = -1;
			}

			strLocalIndex txdSlot = strLocalIndex(pReq->GetTxdSlot(i));
			if (txdSlot.IsValid()){
				g_TxdStore.AddRef(txdSlot, REF_OTHER);
				m_txdIdx[i] = txdSlot.Get();
			}
			else
			{
				m_txdIdx[i] = -1;
			}
		}
	}

#if !__NO_OUTPUT
	m_pTargetEntity = pReq->GetTargetEntity();
	m_frameCountCreated = fwTimer::GetFrameCount();
#endif

	m_blendIndex = (u16)-1;
	m_refCount = 0;
	m_used = true;
}

CPedPropRenderGfx::~CPedPropRenderGfx()
{
	ReleaseGfx();
}

void CPedPropRenderGfx::Init()
{
	for (int i = 0; i < FENCE_COUNT; ++i)
	{
		ms_renderRefs[i].Reserve(MAX_PROP_RENDER_GFX);
	}
}

void CPedPropRenderGfx::Shutdown()
{
	for (int i = 0; i < FENCE_COUNT; ++i)
	{
		ms_renderRefs[i].Reset();
	}
}

void CPedPropRenderGfx::ReleaseGfx()
{
	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		// release the ref to the drawlb dict - should cause it to delete
		if (m_dwdIdx[i] != -1) {
			g_DwdStore.RemoveRef(strLocalIndex(m_dwdIdx[i]), REF_OTHER);
			m_dwdIdx[i] = -1;
		}

		// release the ref to the diff tex - should cause it to delete
		if (m_txdIdx[i] != -1) {
			g_TxdStore.RemoveRef(strLocalIndex(m_txdIdx[i]), REF_OTHER);
			m_txdIdx[i] = -1;
		}
	}
}

grcTexture* CPedPropRenderGfx::GetTexture(u32 propSlot)		
{ 
	Assert(propSlot < MAX_PROPS_PER_PED); 
	s32 txdIdx = m_txdIdx[propSlot];

	if (txdIdx > -1){
		return(g_TxdStore.Get(strLocalIndex(m_txdIdx[propSlot]))->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

s32 CPedPropRenderGfx::GetTxdIndex(u32 propSlot) const
{ 
	Assert(propSlot < MAX_PROPS_PER_PED); 
	return m_txdIdx[propSlot];
}

rmcDrawable* CPedPropRenderGfx::GetDrawable(u32 propSlot) const 
{ 
	Assert(propSlot < MAX_PROPS_PER_PED); 
	s32 dwdIdx = m_dwdIdx[propSlot];

	if (dwdIdx > -1){
		return(g_DwdStore.Get(strLocalIndex(m_dwdIdx[propSlot]))->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

const rmcDrawable* CPedPropRenderGfx::GetDrawableConst(u32 propSlot) const
{ 
	Assert(propSlot < MAX_PROPS_PER_PED); 
	s32 dwdIdx = m_dwdIdx[propSlot];

	if (dwdIdx > -1){
		return(g_DwdStore.Get(strLocalIndex(m_dwdIdx[propSlot]))->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

void CPedPropRenderGfx::AddRefsToGfx()
{
	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{
		// Drawable
		strLocalIndex dwdIdx = strLocalIndex(m_dwdIdx[i]);
		if (dwdIdx.IsValid())
		{
			g_DwdStore.AddRef(dwdIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(dwdIdx.Get(), &g_DwdStore);
		}
	}

	for (u32 i = 0; i < MAX_PROPS_PER_PED; ++i)
	{	
		// Texture
		strLocalIndex txdIdx = strLocalIndex(m_txdIdx[i]);
		if (txdIdx.IsValid())
		{
			g_TxdStore.AddRef(txdIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(txdIdx.Get(), &g_TxdStore);
		}
	}

	AddRef();
	ms_renderRefs[ms_fenceUpdateIdx].Push(this);
}

#if __DEV
bool CPedPropRenderGfx::ValidateStreamingIndexHasBeenCached() 
{
	bool indexInCache = true;

	for (int i = 0; indexInCache && i < MAX_PROPS_PER_PED; ++i)
	{
		// Drawable
		s32 dwdIdx = m_dwdIdx[i];
		if (dwdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(dwdIdx, &g_DwdStore);
		}
	}

	for (int i = 0; indexInCache && i < MAX_PROPS_PER_PED; ++i)
	{	// Texture
		s32 txdIdx = m_txdIdx[i];
		if (txdIdx > -1)
		{
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(txdIdx, &g_TxdStore);
		}
	}

	return indexInCache;
}
#endif

void CPedPropRenderGfx::SetUsed(bool val)
{
	m_used = val;

	if (!m_used && !m_refCount)
		delete this;
}

void CPedPropRenderGfx::RemoveRefs()
{
	s32 newUpdateIdx = (ms_fenceUpdateIdx + 1) % FENCE_COUNT;

	for (s32 i = 0; i < ms_renderRefs[newUpdateIdx].GetCount(); ++i)
	{
		CPedPropRenderGfx* gfx = ms_renderRefs[newUpdateIdx][i];
		if (!gfx)
			continue;

		gfx->m_refCount--;
	}

	ms_renderRefs[newUpdateIdx].Resize(0);
	ms_fenceUpdateIdx = newUpdateIdx;

	// free unused entries
	CPedPropRenderGfx::Pool* pool = CPedPropRenderGfx::GetPool();
    if (pool)
    {
        for (s32 i = 0; i < pool->GetSize(); ++i)
        {
            CPedPropRenderGfx* gfx = pool->GetSlot(i);
            if (!gfx)
                continue;

            if (!gfx->m_used && !gfx->m_refCount)
                delete gfx;
        }
    }
}

// PURPOSE: Helper function for checking if any of the restrictions in the given array
//			restrict a specific prop being used on a certain anchor.
bool CPedPropsMgr::IsPropRestricted(eAnchorPoints propSlot, int propIndex, const CPedPropRestriction* pRestrictions, int numRestrictions)
{
	for(int j = 0; j < numRestrictions; j++)
	{
		if(pRestrictions[j].IsRestricted(propSlot, propIndex))
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CPackedPedProps
//////////////////////////////////////////////////////////////////////////
void CPackedPedProps::Reset()
{
	for (int i = 0; i < MAX_PROPS_PER_PED; i++)
	{
		m_packedPedProps[i] = 0;
		m_propVarInfoHashes[i] = 0;
	}
}

void CPackedPedProps::Serialise(CSyncDataBase& serialiser)
{
	LOGGING_ONLY(char buffer[32] = "\0";)

	for (int i = 0; i < MAX_PROPS_PER_PED; i++)
	{
		LOGGING_ONLY(formatf(buffer, 32, "PropVarInfoHash %d", i);)
		SERIALISE_UNSIGNED(serialiser, m_propVarInfoHashes[i], SIZEOF_HASH, buffer);

		LOGGING_ONLY(formatf(buffer, 32, "PackedPedProp %d", i);)
		SERIALISE_UNSIGNED(serialiser, m_packedPedProps[i], SIZEOF_COMBINED, buffer);
	}
}

// Sync the variationinfo hash as well as the indicies -1's are converted to a positive range for syncing
void CPackedPedProps::SetPropData(const CPed* pPed, u32 index, s32 anchorID, s32 anchorOverrideID, s32 propIdx, s32 propTexIdx)
{
	if (index < MAX_PROPS_PER_PED)
	{
		u32 varInfoHash = 0;
		s32 localIdx = -1;

		CPedPropsMgr::GetLocalPropData(pPed, (eAnchorPoints)anchorID, propIdx, varInfoHash, localIdx);

		m_propVarInfoHashes[index] = varInfoHash;

		u32 packableAnchorValue			= GetPackedAnchorValue((eAnchorPoints)anchorID);
		u32 packableAnchorOverrideValue = GetPackedAnchorValue((eAnchorPoints)anchorOverrideID);
		u32 packableModelIdx = localIdx + 1;
		u32 packableTexIdx = propTexIdx + 1;

		m_packedPedProps[index] |= packableModelIdx				<< (SIZEOF_ANCHOR_ID_BITS + SIZEOF_ANCHOR_OVERRIDE_ID_BITS + SIZEOF_TEXTURE_IDX_BITS);
		m_packedPedProps[index] |= packableAnchorValue			<< (SIZEOF_ANCHOR_OVERRIDE_ID_BITS + SIZEOF_TEXTURE_IDX_BITS);
		m_packedPedProps[index] |= packableAnchorOverrideValue	<< (SIZEOF_TEXTURE_IDX_BITS);
		m_packedPedProps[index] |= packableTexIdx;
	}
}

void CPackedPedProps::SetPackedPedPropData(u32 index, u32 varInfoHash, u32 data)
{
	if (index < MAX_PROPS_PER_PED)
	{
		m_propVarInfoHashes[index] = varInfoHash;
		m_packedPedProps[index] = data;
	}
}

eAnchorPoints CPackedPedProps::GetAnchorID(u32 index) const
{
	if (index < MAX_PROPS_PER_PED)
		return GetUnPackedAnchorValue(Extract(m_packedPedProps[index], SIZEOF_ANCHOR_ID_BITS, SIZEOF_ANCHOR_OVERRIDE_ID_BITS + SIZEOF_TEXTURE_IDX_BITS));

	return ANCHOR_NONE;
}

eAnchorPoints CPackedPedProps::GetAnchorOverrideID(u32 index) const
{
	if (index < MAX_PROPS_PER_PED)
		return GetUnPackedAnchorValue(Extract(m_packedPedProps[index], SIZEOF_ANCHOR_OVERRIDE_ID_BITS, SIZEOF_TEXTURE_IDX_BITS));

	return ANCHOR_NONE;
}


s32 CPackedPedProps::GetModelIdx(u32 index, const CPed* pPed) const
{
	if (index < MAX_PROPS_PER_PED)
	{
		s32 localIndex = (s32)(Extract(m_packedPedProps[index], SIZEOF_PROP_IDX_BITS, SIZEOF_ANCHOR_ID_BITS + SIZEOF_ANCHOR_OVERRIDE_ID_BITS + SIZEOF_TEXTURE_IDX_BITS)) - 1;

		return CPedPropsMgr::GetGlobalPropData(pPed, GetAnchorID(index), m_propVarInfoHashes[index], localIndex);
	}

	return 0;
}

s32 CPackedPedProps::GetPropTextureIdx(u32 index) const
{
	if (index < MAX_PROPS_PER_PED)
		return (s32)(Extract(m_packedPedProps[index], SIZEOF_TEXTURE_IDX_BITS, 0) - 1);

	return 0;
}

void CPackedPedProps::GetPackedPedPropData(u32 index, u32& varInfoHash, u32& data) const
{
	if (index < MAX_PROPS_PER_PED)
	{
		data = m_packedPedProps[index];
		varInfoHash = m_propVarInfoHashes[index];
	}
}
