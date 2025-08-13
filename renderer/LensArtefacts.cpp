
#include "LensArtefacts.h"
#include "LensArtefacts_parser.h"

#include "grcore/device.h"
#include "grcore/texture.h"
#include "file/default_paths.h"
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "parser/manager.h"
#include "input/headtracking.h"

//////////////////////////////////////////////////////////////////////////
//
LensArtefacts* LensArtefacts::sm_pInstance = NULL;
#define LENSARTEFACTS_METADATA_FILE_LOAD_NAME	"platform:/data/effects/lensartefacts" 
#define LENSARTEFACTS_METADATA_FILE_SAVE_NAME	RS_ASSETS "/export/data/effects/lensartefacts" 

//////////////////////////////////////////////////////////////////////////
//
LensArtefacts::LensArtefacts()
{
	m_bIsActive = false;
	m_bUseInterleavedBlur = (RSG_DURANGO | RSG_ORBIS);
	m_bCombineLayerRendering = (RSG_DURANGO | RSG_ORBIS);
}

//////////////////////////////////////////////////////////////////////////
//
LensArtefacts::~LensArtefacts()
{

}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Init()
{
	m_layers.Reset();
	m_sortedLayers.Init(32);
	m_globalIntensity = 1.0f;
	m_minExposureForFade = 0.0f;
	m_maxExposureForFade = 1.0f; 
	m_exposureFadeMinMult = 1.0f;
	m_exposureFadeMaxMult = 1.0f;
	m_exposureFadeContribution = 1.0f;

	m_bIsActive = false;
	m_bUseInterleavedBlur = (RSG_DURANGO | RSG_ORBIS);
	m_bCombineLayerRendering = (RSG_DURANGO | RSG_ORBIS);
	m_pBlitCallback = NULL;

#if __BANK
	m_bUseSolidBorderSampler = true;
	m_bOverrideTimecycle = false;
#endif

	for (int i = 0; i < LENSARTEFACT_BUFFER_COUNT; i++)
	{
		m_pSrcBuffers[i] = NULL;
		m_pScratchBuffers[0][i] = NULL;
		m_pScratchBuffers[1][i] = NULL;
	}
	m_pFinalBuffer = NULL;
	m_pSrcBufferUnfiltered = NULL;
	m_pShader = NULL;

	m_blurWidth[0] = 4;
	m_blurWidth[1] = 32;
	m_blurWidth[2] = 64;
	m_blurFalloffExponent = 2.0f;

	LoadMetaData();

#if __BANK
	m_editor.Init();
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Shutdown()
{
	m_sortedLayers.Clear();
	m_layers.Reset();
	m_bIsActive = false;
	m_pShader = NULL;
	m_pBlitCallback = NULL;

	for (int i = 0; i < LENSARTEFACT_BUFFER_COUNT; i++)
	{
		m_pSrcBuffers[i] = NULL;
		m_pScratchBuffers[0][i] = NULL;
		m_pScratchBuffers[1][i] = NULL;
	}
	m_pFinalBuffer = NULL;
	m_pSrcBufferUnfiltered = NULL;
#if __BANK
	m_editor.Shutdown();
#endif 
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::ClassInit()
{
	sm_pInstance = rage_new LensArtefacts();
	Assert(sm_pInstance);

	if (sm_pInstance)
	{
		LENSARTEFACTSMGR.Init();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::ClassShutdown()
{
	if (Verifyf(sm_pInstance != NULL, "LensArtefacts is not instantiated"))
	{
		LENSARTEFACTSMGR.Shutdown();

		delete sm_pInstance;
		sm_pInstance = NULL;
	}
}

bool LensArtefacts::IsActive() const  
{ 
	return m_bIsActive && !rage::ioHeadTracking::IsMotionTrackingEnabled() 
#if RSG_PC
		 && (CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX >= CSettings::High)
#endif // RSG_PC
		 ;
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::SetRenderTargets(grcRenderTarget* pDstBuffer, grcRenderTarget* pSrcBufferUnfiltered, 
									 grcRenderTarget** pSrcBuffers, grcRenderTarget** pScratchBuffers0, grcRenderTarget** pScratchBuffers1)
{
	m_pFinalBuffer = pDstBuffer;
	m_pSrcBufferUnfiltered = pSrcBufferUnfiltered;
	for (int i = 0; i < LENSARTEFACT_BUFFER_COUNT; i++)
	{
		m_pSrcBuffers[i] = pSrcBuffers[i];
		m_pScratchBuffers[0][i] = pScratchBuffers0[i];
		m_pScratchBuffers[1][i] = pScratchBuffers1[i];
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::SetShaderData(grmShader* pShader, u32* pPasses)
{
	m_pShader = pShader;
	m_shaderTechnique = m_pShader->LookupTechnique("PostFx");
	
	for (int i = 0; i < LENSARTEFACT_SHADER_PASS_COUNT; i++)
		m_shaderPasses[i] = pPasses[i];

	m_shaderVarBlurDirectionID			= m_pShader->LookupVar("LightStreaksBlurDir");
	m_shaderVarDirBlurColWeightsID		= m_pShader->LookupVar("LightStreaksBlurColWeights");
	m_shaderVarColorShiftID				= m_pShader->LookupVar("LightStreaksColorShift0");
	m_shaderVarLensArtefactParams0ID	= m_pShader->LookupVar("LensArtefactsParams0");
	m_shaderVarLensArtefactParams1ID	= m_pShader->LookupVar("LensArtefactsParams1");
	m_shaderVarLensArtefactParams2ID	= m_pShader->LookupVar("LensArtefactsParams2");
	m_shaderVarLensArtefactParams3ID	= m_pShader->LookupVar("LensArtefactsParams3");
	m_shaderVarLensArtefactParams4ID	= m_pShader->LookupVar("LensArtefactsParams4");
	m_shaderVarLensArtefactParams5ID	= m_pShader->LookupVar("LensArtefactsParams5");
	m_shaderVarTexelSizeID				= m_pShader->LookupVar("TexelSize");
	m_shaderVarSamplerID				= m_pShader->LookupVar("PostFxTexture0");
	m_shaderVarSamplerAltID				= m_pShader->LookupVar("PostFxTexture1");
	m_shaderVarUpsampleParamsID			= m_pShader->LookupVar("BloomParams");

	grcSamplerStateDesc descBorder;
	descBorder.AddressU			= grcSSV::TADDRESS_BORDER;
	descBorder.AddressV			= grcSSV::TADDRESS_BORDER;
	descBorder.BorderColorAlpha	= 0.0f;
	descBorder.BorderColorRed	= 0.0f;
	descBorder.BorderColorGreen	= 0.0f;
	descBorder.BorderColorBlue	= 0.0f;
	m_samplerStateBorder = grcStateBlock::CreateSamplerState(descBorder);

}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::GetRenderTargets(grcRenderTarget*& pDstBuffer, grcRenderTarget*& pSrcBuffer, eLensArtefactBlurType blurType, eLensArtefactStreakDir layerType) const
{
	// Figure out what bloom buffer we should start with
	switch (blurType)
	{
	case LENSARTEFACT_BLUR_16X:
		{
			pSrcBuffer = m_pSrcBuffers[LENSARTEFACT_SIXTEENTH];
			pDstBuffer = m_pScratchBuffers[0][LENSARTEFACT_SIXTEENTH];
			break;
		}
	case LENSARTEFACT_BLUR_8X:
		{
			pSrcBuffer = m_pSrcBuffers[LENSARTEFACT_EIGHTH];
			pDstBuffer = m_pScratchBuffers[0][LENSARTEFACT_EIGHTH];
			break;
		}
	case LENSARTEFACT_BLUR_4X:
		{
			pSrcBuffer = m_pSrcBuffers[LENSARTEFACT_QUARTER];
			pDstBuffer = m_pScratchBuffers[0][LENSARTEFACT_QUARTER];
			break;
		}
	case LENSARTEFACT_BLUR_2X:
		{
			pSrcBuffer = m_pSrcBuffers[LENSARTEFACT_HALF];
			pDstBuffer = m_pScratchBuffers[0][LENSARTEFACT_HALF];
			break;
		}
	case LENSARTEFACT_BLUR_0X:
	default:
		{
			pSrcBuffer = m_pSrcBufferUnfiltered;
			pDstBuffer = m_pScratchBuffers[0][LENSARTEFACT_HALF];
			break;
		}
	}

	// If there's no directional blur for this layer, composite the artefact
	// directly into the final buffer
	if (layerType == LENSARTEFACT_STREAK_NONE)
	{
		pDstBuffer = m_pFinalBuffer;
	}

}

//////////////////////////////////////////////////////////////////////////
//
LensArtefact* LensArtefacts::Get(atHashString name)
{
	for (int i = 0; i < m_layers.GetCount(); i++)
	{
		if (m_layers[i].GetName() == name)
		{
			return &m_layers[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
const LensArtefact* LensArtefacts::Get(atHashString name) const
{
	for (int i = 0; i < m_layers.GetCount(); i++)
	{
		if (m_layers[i].GetName() == name)
		{
			return &m_layers[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Add(LensArtefact& layer)
{
	m_layers.PushAndGrow(layer);
	UpdateSortedList();
}

//////////////////////////////////////////////////////////////////////////
//
LensArtefactBufferSize LensArtefacts::GetBufferSize(const grcRenderTarget* pBuffer) const
{
	int bufferWidth = pBuffer->GetWidth();

	for (int i = 0; i < LENSARTEFACT_BUFFER_COUNT; i++)
	{
		if (bufferWidth == m_pScratchBuffers[0][i]->GetWidth())
			return (LensArtefactBufferSize)(i);
	}

	return LENSARTEFACT_BUFFER_COUNT;
}

//////////////////////////////////////////////////////////////////////////
//
bool LensArtefacts::LoadMetaData()
{
	bool bOk = fwPsoStoreLoader::LoadDataIntoObject(LENSARTEFACTS_METADATA_FILE_LOAD_NAME, META_FILE_EXT, *this);
	//PARSER.SaveObject(LENSARTEFACTS_METADATA_FILE_LOAD_NAME, "meta", this);
	if (bOk)
	{
		m_bIsActive = true;
		UpdateSortedList();
	}
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK && !__FINAL
bool LensArtefacts::SaveMetaData()
{
	bool bOk = PARSER.SaveObject(LENSARTEFACTS_METADATA_FILE_SAVE_NAME, "pso.meta", this);

	if (bOk == false)
	{
		Displayf("LensArtefacts::SaveMetaData: Failed to save data for \"%s\"", LENSARTEFACTS_METADATA_FILE_SAVE_NAME);
	}

	return bOk;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Update()
{
#if __BANK
	m_editor.Update();
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Remove(atHashString name)
{
	for (int i = 0; i < m_layers.GetCount(); i++)
	{
		if (m_layers[i].GetName() == name)
		{
			m_layers.Delete(i);
			UpdateSortedList();
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Copy(LensArtefact* pLayerDst, const LensArtefact* pLayerSrc)
{
	if (pLayerDst == NULL || pLayerSrc == NULL)
		return;

	bool bUpdateSortedList = (pLayerDst->GetSortIndex() != pLayerSrc->GetSortIndex());
	*pLayerDst = *pLayerSrc;

	if (bUpdateSortedList)
		UpdateSortedList();
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::UpdateSortedList()
{
	m_sortedLayers.Clear();
	for (int i = 0; i < m_layers.GetCount(); i++)
	{
		m_sortedLayers.InsertSorted(&m_layers[i]);
	}

#if __BANK
	m_editor.UpdateLayerNamesList(m_sortedLayers);
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::Render()
{
	// Clear final lens artefact buffer, there might not be any layer active
	PF_PUSH_MARKER("LensArtefactsClear");
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_pFinalBuffer, NULL);
	GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 0.0f, false);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	PF_POP_MARKER();

	// Early out if there's nothing!
	if (GetLayerCount() == 0 || m_bIsActive == false)
		return;

	// Go through all the layersz
	PF_PUSH_MARKER("PrimeLensArtefacts");
	SortedLayerkNode* pNode = m_sortedLayers.GetFirst()->GetNext();
	while(pNode != m_sortedLayers.GetLast())
	{
		const LensArtefact* pLayer0 = (pNode->item);

		pNode = pNode->GetNext();

		// Only render if enabled
		if (pLayer0->IsEnabled())
		{
			// We can only combine two layers if a) m_bCombineLayerRendering and b) the layer does *not* need blurring
			bool bNeedsBlurPass = ((pLayer0->GetStreakDirection() != LENSARTEFACT_STREAK_NONE) && (pLayer0->GetBlurType() == LENSARTEFACT_BLUR_2X));
			if (m_bCombineLayerRendering == false || bNeedsBlurPass)
			{
				RenderLayer(pLayer0);
			}
			else if (pNode != NULL && (pNode != m_sortedLayers.GetLast()))
			{
				const LensArtefact* pLayer1 = (pNode->item);

				// We can render both layers at once if a) layer1 is active and b) layer1 does not need blurring
				bool bCanRenderBothLayers = (pLayer1 && pLayer1->IsEnabled() && (pLayer1->GetStreakDirection() == LENSARTEFACT_STREAK_NONE));
				if (bCanRenderBothLayers)
				{
					RenderLayersCombined(pLayer0, pLayer1);
					// skip pLayer1
					pNode = pNode->GetNext();
				}
				// Otherwise render a single layer
				else
				{
					RenderLayer(pLayer0);
				}
			}

		} // layer enabled

	

	}
	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::RenderLayer(const LensArtefact* pLayer)
{
	// Extract common layer data
	Vector4 lensArtefactsParams0, lensArtefactsParams1, lensArtefactsParams2;
	GetLayerRenderData(pLayer, lensArtefactsParams0, lensArtefactsParams1, lensArtefactsParams2);
	
	// Figure out dst and src targets
	grcRenderTarget* pDstRenderTarget;
	grcRenderTarget* pSrcRenderTarget;
	GetRenderTargets(pDstRenderTarget, pSrcRenderTarget, pLayer->GetBlurType(), pLayer->GetStreakDirection());

	bool bNeedsBlurPass = ((pLayer->GetStreakDirection() != LENSARTEFACT_STREAK_NONE) && (pLayer->GetBlurType() == LENSARTEFACT_BLUR_2X));

	// Push shader constants and source texture
	m_pShader->SetVar(m_shaderVarLensArtefactParams0ID, lensArtefactsParams0);
	m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
	m_pShader->SetVar(m_shaderVarLensArtefactParams2ID, lensArtefactsParams2);
	m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pSrcRenderTarget->GetWidth()),1.0f/float(pSrcRenderTarget->GetHeight()),0.0, 0.0f));
	m_pShader->SetVar(m_shaderVarSamplerID, pSrcRenderTarget);
	
	if (bNeedsBlurPass)
	{
		// Clear temp. buffer
		PF_PUSH_MARKER("LensArtefactsClear");
			grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRenderTarget, NULL);
			GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 0.0f, false);
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		PF_POP_MARKER();

		BANK_ONLY(if (m_bUseSolidBorderSampler))
		{
			m_pShader->PushSamplerState(m_shaderVarSamplerID, m_samplerStateBorder);
		}

		// Directional blur + composite
		m_pBlitCallback(pDstRenderTarget, pSrcRenderTarget, m_shaderPasses[LENSARTEFACT_LENS_PASS]);
		ProcessDirectionalBlur(pLayer, pDstRenderTarget);

		BANK_ONLY(if (m_bUseSolidBorderSampler))
		{
			m_pShader->PopSamplerState(m_shaderVarSamplerID);
		}

	}
	else
	{
		BANK_ONLY(if (m_bUseSolidBorderSampler))
		{
			m_pShader->PushSamplerState(m_shaderVarSamplerID, m_samplerStateBorder);
		}

		// Composite
		m_pBlitCallback(pDstRenderTarget, pSrcRenderTarget, m_shaderPasses[LENSARTEFACT_LENS_PASS]);

		BANK_ONLY(if (m_bUseSolidBorderSampler))
		{
			m_pShader->PopSamplerState(m_shaderVarSamplerID);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::RenderLayersCombined(const LensArtefact* pLayer0, const LensArtefact* pLayer1)
{
	// Extract common layer data
	Vector4 lensArtefactsParams0_0, lensArtefactsParams1_0, lensArtefactsParams2_0;
	GetLayerRenderData(pLayer0, lensArtefactsParams0_0, lensArtefactsParams1_0, lensArtefactsParams2_0);
	Vector4 lensArtefactsParams0_1, lensArtefactsParams1_1, lensArtefactsParams2_1;
	GetLayerRenderData(pLayer1, lensArtefactsParams0_1, lensArtefactsParams1_1, lensArtefactsParams2_1);

	// Figure out dst and src targets
	grcRenderTarget* pDstRenderTarget;
	grcRenderTarget* pSrcRenderTarget0;
	grcRenderTarget* pSrcRenderTarget1;

	GetRenderTargets(pDstRenderTarget, pSrcRenderTarget0, pLayer0->GetBlurType(), pLayer0->GetStreakDirection());
	GetRenderTargets(pDstRenderTarget, pSrcRenderTarget1, pLayer1->GetBlurType(), pLayer1->GetStreakDirection());


	// Push shader constants and source texture
	m_pShader->SetVar(m_shaderVarLensArtefactParams0ID, lensArtefactsParams0_0);
	m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1_0);
	m_pShader->SetVar(m_shaderVarLensArtefactParams2ID, lensArtefactsParams2_0);
	m_pShader->SetVar(m_shaderVarLensArtefactParams3ID, lensArtefactsParams0_1);
	m_pShader->SetVar(m_shaderVarLensArtefactParams4ID, lensArtefactsParams1_1);
	m_pShader->SetVar(m_shaderVarLensArtefactParams5ID, lensArtefactsParams2_1);

	m_pShader->SetVar(m_shaderVarSamplerID, pSrcRenderTarget0);
	m_pShader->SetVar(m_shaderVarSamplerAltID, pSrcRenderTarget1);


	BANK_ONLY(if (m_bUseSolidBorderSampler))
	{
		m_pShader->PushSamplerState(m_shaderVarSamplerID, m_samplerStateBorder);
	}

	// Composite
	m_pBlitCallback(pDstRenderTarget, pSrcRenderTarget0, m_shaderPasses[LENSARTEFACT_LENS_COMBINED_PASS]);

	BANK_ONLY(if (m_bUseSolidBorderSampler))
	{
		m_pShader->PopSamplerState(m_shaderVarSamplerID);
	}

}
//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::ProcessDirectionalBlur(const LensArtefact* pLayer, grcRenderTarget* pSrcBuffer)
{
	PF_PUSH_MARKER("ProcessDirectionalBlur");


	eLensArtefactStreakDir blurDirectionType = pLayer->GetStreakDirection();
	Vector4 blurDirection0;
	Vector4 blurDirection1;
	blurDirection0.Zero();
	blurDirection1.Zero();

	switch (blurDirectionType)
	{
	case LENSARTEFACT_STREAK_HORIZONTAL:
		blurDirection0.Set(-1.0f, 0.0f, 0.0f, 0.0f);
		blurDirection1.Set( 1.0f, 0.0f, 0.0f, 0.0f);
		break;
	case LENSARTEFACT_STREAK_VERTICAL:
		blurDirection0.Set(0.0f, -1.0f, 0.0f, 0.0f);
		blurDirection1.Set(0.0f, 1.0f, 0.0f, 0.0f);
		break;
	case LENSARTEFACT_STREAK_DIAGONAL_1:
		blurDirection0.Set(-1.0f, 1.0f, 0.0f, 0.0f);
		blurDirection1.Set(1.0f, -1.0f, 0.0f, 0.0f);
		break;
	case LENSARTEFACT_STREAK_DIAGONAL_2:
		blurDirection0.Set(1.0f, 1.0f, 0.0f, 0.0f);
		blurDirection1.Set(-1.0f, -1.0f, 0.0f, 0.0f);
		break;
	default:
		break;
	}

	if (m_bUseInterleavedBlur)
	{
		RunDirectionalBlurPassesInterleaved(pLayer, pSrcBuffer, blurDirection0, blurDirection1);
	}
	else
	{
		RunDirectionalBlurPass(pLayer, pSrcBuffer, blurDirection0);
		RunDirectionalBlurPass(pLayer, pSrcBuffer, blurDirection1);
	}

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::RunDirectionalBlurPass(const LensArtefact* pLayer, grcRenderTarget* pSrcBuffer, Vector4& dir)
{
	LensArtefactBufferSize bufferSizeSrc =	GetBufferSize(pSrcBuffer);
	LensArtefactBufferSize bufferSizeDst = LENSARTEFACT_SIXTEENTH;

	if (bufferSizeSrc == LENSARTEFACT_BUFFER_COUNT)
	{
		Assertf(0, "Target size mismatch");
		return;
	}
	PF_PUSH_MARKER("RunDirectionalBlurPass");

	// Set common layer data
	Color32 colorShift = pLayer->GetColorShift();
	m_pShader->SetVar(m_shaderVarColorShiftID, colorShift);
	int currentSize = Min((int)(bufferSizeSrc+1), (int)bufferSizeDst);


	Vector4 lensArtefactsParams0;
	Vector4 lensArtefactsParams1;
	Vector4	lensArtefactsParams2;
	GetLayerRenderData(pLayer, lensArtefactsParams0, lensArtefactsParams1, lensArtefactsParams2);

	PF_PUSH_MARKER("RunDirectionalBlurPass - Downsample/Blur");
	// First downsample
	s32 curBlurWidth = 0;
	dir.z = (float)m_blurWidth[curBlurWidth++];
	dir.w = m_blurFalloffExponent;
	m_pShader->SetVar(m_shaderVarBlurDirectionID, dir);
	m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
	m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pSrcBuffer->GetWidth()),1.0f/float(pSrcBuffer->GetHeight()),0.0, 0.0f));
	m_pShader->SetVar(m_shaderVarSamplerID, pSrcBuffer);
	m_pBlitCallback(m_pScratchBuffers[1][currentSize], pSrcBuffer, m_shaderPasses[LENSARTEFACT_DIRECTIONAL_BLUR_LOW_PASS]);

	// Keep downsampling all the way to 16x
	int curPass = LENSARTEFACT_DIRECTIONAL_BLUR_MED_PASS;
	for (; currentSize < LENSARTEFACT_SIXTEENTH; currentSize++)
	{

		dir.z = (float)m_blurWidth[curBlurWidth++];
		curBlurWidth = Min(curBlurWidth, 2);
		m_pShader->SetVar(m_shaderVarBlurDirectionID, dir);

		grcRenderTarget* pCurDstTarget = m_pScratchBuffers[1][currentSize+1];
		grcRenderTarget* pCurSrcTarget = m_pScratchBuffers[1][currentSize];
		m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
		m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pCurSrcTarget->GetWidth()),1.0f/float(pCurSrcTarget->GetHeight()),0.0, 0.0f));
		m_pShader->SetVar(m_shaderVarSamplerID, pCurSrcTarget);
		m_pBlitCallback(pCurDstTarget, pCurSrcTarget, m_shaderPasses[curPass++]);

		curPass = curPass>LENSARTEFACT_DIRECTIONAL_BLUR_HIGH_PASS ? LENSARTEFACT_DIRECTIONAL_BLUR_LOW_PASS : curPass;
	}
	PF_POP_MARKER(); // end RunDirectionalBlurPass - Downsample/Blur

	PF_PUSH_MARKER("RunDirectionalBlurPass - Upsample");
	// And upsample
	Vector4 upsampleParams;
	upsampleParams.Zero();
	upsampleParams.w = 1.0f;

	for (currentSize = LENSARTEFACT_SIXTEENTH; currentSize > bufferSizeSrc; currentSize--)
	{
		grcRenderTarget* pCurDstTarget = m_pScratchBuffers[1][currentSize-1];

		// composite back into final target
		if ((currentSize-1) == bufferSizeSrc)
			pCurDstTarget = m_pFinalBuffer;

		grcRenderTarget* pCurSrcTarget = m_pScratchBuffers[1][currentSize];
		m_pShader->SetVar(m_shaderVarUpsampleParamsID, upsampleParams);
		m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pCurSrcTarget->GetWidth()),1.0f/float(pCurSrcTarget->GetHeight()),0.0, 0.0f));
		m_pShader->SetVar(m_shaderVarSamplerID, pCurSrcTarget);
		m_pBlitCallback(pCurDstTarget, pCurSrcTarget, m_shaderPasses[LENSARTEFACT_UPSAMPLE_PASS]);
	}
	PF_POP_MARKER(); // end RunDirectionalBlurPass - Upsample

	PF_POP_MARKER(); // end RunDirectionalBlurPass
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::RunDirectionalBlurPassesInterleaved(const LensArtefact* pLayer, grcRenderTarget* pSrcBuffer, Vector4& dir0, Vector4& dir1)
{
	LensArtefactBufferSize bufferSizeSrc =	GetBufferSize(pSrcBuffer);
	LensArtefactBufferSize bufferSizeDst = LENSARTEFACT_SIXTEENTH;

	if (bufferSizeSrc == LENSARTEFACT_BUFFER_COUNT)
	{
		Assertf(0, "Target size mismatch");
		return;
	}
	PF_PUSH_MARKER("RunDirectionalBlurPassesInterleaved");

	// Set common layer data
	Color32 colorShift = pLayer->GetColorShift();
	m_pShader->SetVar(m_shaderVarColorShiftID, colorShift);
	int currentSize = Min((int)(bufferSizeSrc+1), (int)bufferSizeDst);


	Vector4 lensArtefactsParams0;
	Vector4 lensArtefactsParams1;
	Vector4	lensArtefactsParams2;
	GetLayerRenderData(pLayer, lensArtefactsParams0, lensArtefactsParams1, lensArtefactsParams2);

	PF_PUSH_MARKER("RunDirectionalBlurPassesInterleaved - Downsample/Blur");
	// First downsample
	s32 curBlurWidth = 0;

	// Dir0
	dir0.z = (float)m_blurWidth[curBlurWidth];
	dir0.w = m_blurFalloffExponent;
	m_pShader->SetVar(m_shaderVarBlurDirectionID, dir0);
	m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
	m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pSrcBuffer->GetWidth()),1.0f/float(pSrcBuffer->GetHeight()),0.0, 0.0f));
	m_pShader->SetVar(m_shaderVarSamplerID, pSrcBuffer);
	m_pBlitCallback(m_pScratchBuffers[1][currentSize], pSrcBuffer, m_shaderPasses[LENSARTEFACT_DIRECTIONAL_BLUR_LOW_PASS]);

	// Dir1
	dir1.z = (float)m_blurWidth[curBlurWidth++];
	dir1.w = m_blurFalloffExponent;
	m_pShader->SetVar(m_shaderVarBlurDirectionID, dir1);
	m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
	m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pSrcBuffer->GetWidth()),1.0f/float(pSrcBuffer->GetHeight()),0.0, 0.0f));
	m_pShader->SetVar(m_shaderVarSamplerID, pSrcBuffer);
	m_pBlitCallback(m_pScratchBuffers[0][currentSize], pSrcBuffer, m_shaderPasses[LENSARTEFACT_DIRECTIONAL_BLUR_LOW_PASS]);


	// Keep downsampling all the way to 16x
	int curPass = LENSARTEFACT_DIRECTIONAL_BLUR_MED_PASS;
	for (; currentSize < LENSARTEFACT_SIXTEENTH; currentSize++)
	{

		// Dir0
		dir0.z = (float)m_blurWidth[curBlurWidth];
		curBlurWidth = Min(curBlurWidth, 2);
		m_pShader->SetVar(m_shaderVarBlurDirectionID, dir0);

		grcRenderTarget* pCurDstTarget0 = m_pScratchBuffers[1][currentSize+1];
		grcRenderTarget* pCurSrcTarget0 = m_pScratchBuffers[1][currentSize];
		m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
		m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pCurSrcTarget0->GetWidth()),1.0f/float(pCurSrcTarget0->GetHeight()),0.0, 0.0f));
		m_pShader->SetVar(m_shaderVarSamplerID, pCurSrcTarget0);
		m_pBlitCallback(pCurDstTarget0, pCurSrcTarget0, m_shaderPasses[curPass]);


		// Dir1
		dir1.z = (float)m_blurWidth[curBlurWidth++];
		curBlurWidth = Min(curBlurWidth, 2);
		m_pShader->SetVar(m_shaderVarBlurDirectionID, dir1);

		grcRenderTarget* pCurDstTarget1 = m_pScratchBuffers[0][currentSize+1];
		grcRenderTarget* pCurSrcTarget1 = m_pScratchBuffers[0][currentSize];
		m_pShader->SetVar(m_shaderVarLensArtefactParams1ID, lensArtefactsParams1);
		m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pCurSrcTarget1->GetWidth()),1.0f/float(pCurSrcTarget1->GetHeight()),0.0, 0.0f));
		m_pShader->SetVar(m_shaderVarSamplerID, pCurSrcTarget1);
		m_pBlitCallback(pCurDstTarget1, pCurSrcTarget1, m_shaderPasses[curPass++]);

		curPass = curPass>LENSARTEFACT_DIRECTIONAL_BLUR_HIGH_PASS ? LENSARTEFACT_DIRECTIONAL_BLUR_LOW_PASS : curPass;
	}
	PF_POP_MARKER(); // end RunDirectionalBlurPassesInterleaved - Downsample/Blur

	PF_PUSH_MARKER("RunDirectionalBlurPassesInterleaved - Upsample");
	// And upsample
	Vector4 upsampleParams;
	upsampleParams.Zero();
	upsampleParams.w = 1.0f;

	for (currentSize = LENSARTEFACT_SIXTEENTH; currentSize > bufferSizeSrc; currentSize--)
	{
		// Dir 0
		grcRenderTarget* pCurDstTarget0 = m_pScratchBuffers[1][currentSize-1];

		// composite back into final target
		if ((currentSize-1) == bufferSizeSrc)
			pCurDstTarget0 = m_pFinalBuffer;

		grcRenderTarget* pCurSrcTarget0 = m_pScratchBuffers[1][currentSize];
		m_pShader->SetVar(m_shaderVarUpsampleParamsID, upsampleParams);
		m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pCurSrcTarget0->GetWidth()),1.0f/float(pCurSrcTarget0->GetHeight()),0.0, 0.0f));
		m_pShader->SetVar(m_shaderVarSamplerID, pCurSrcTarget0);
		m_pBlitCallback(pCurDstTarget0, pCurSrcTarget0, m_shaderPasses[LENSARTEFACT_UPSAMPLE_PASS]);

		// Dir 1
		grcRenderTarget* pCurDstTarget1 = m_pScratchBuffers[0][currentSize-1];

		// composite back into final target
		if ((currentSize-1) == bufferSizeSrc)
			pCurDstTarget1 = m_pFinalBuffer;

		grcRenderTarget* pCurSrcTarget1 = m_pScratchBuffers[0][currentSize];
		m_pShader->SetVar(m_shaderVarUpsampleParamsID, upsampleParams);
		m_pShader->SetVar(m_shaderVarTexelSizeID, Vector4(1.0f/float(pCurSrcTarget1->GetWidth()),1.0f/float(pCurSrcTarget1->GetHeight()),0.0, 0.0f));
		m_pShader->SetVar(m_shaderVarSamplerID, pCurSrcTarget1);
		m_pBlitCallback(pCurDstTarget1, pCurSrcTarget1, m_shaderPasses[LENSARTEFACT_UPSAMPLE_PASS]);
	}
	PF_POP_MARKER(); // end RunDirectionalBlurPassesInterleaved - Upsample

	PF_POP_MARKER(); // end RunDirectionalBlurPassesInterleaved
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::GetLayerRenderData(const LensArtefact* pLayer, Vector4& scaleoffset, Vector4& misc, Vector4& color) const
{
	// Pack data for shader
	pLayer->GetScaleAndOffset(scaleoffset);

	misc.Zero();
	misc.x = pLayer->GetOpacity();

	Vector2 maskMinMax;
	pLayer->GetFadeMaskMinMax(maskMinMax);
	eLensArtefactMaskType maskType = pLayer->GetFadeMaskType();
	misc.z = maskMinMax.x;
	misc.w = maskMinMax.y;
	if (maskType == LENSARTEFACT_MASK_HORIZONTAL)
		misc.y = 0.0f;
	else if (maskType == LENSARTEFACT_MASK_VERTICAL)
		misc.y = 1.0f;
	else
	{
		misc.z = 1.0f;
		misc.w = 1.0f;
	}

	Color32 col = pLayer->GetColorShift();
	color = VEC4V_TO_VECTOR4(col.GetRGBA());
}

//////////////////////////////////////////////////////////////////////////
//
void  LensArtefacts::SetExposureMinMaxFadeMultipliers(float expMin, float expMax, float fadeMin, float fadeMax)
{
#if __BANK
	if (m_bOverrideTimecycle == false)
#endif
	{
		m_minExposureForFade = expMin;
		m_maxExposureForFade = expMax;

		m_exposureFadeMinMult = fadeMin;
		m_exposureFadeMaxMult = fadeMax;
	}
}


//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::GetExposureBasedOpacityMultipliers(Vector4& LensArtefactsParams0, Vector4& LensArtefactsParams1) const
{
	// From postfx.fx
	//#define LensArtefactsExposureMin				LensArtefactsParams0.x
	//#define LensArtefactsExposureOORange			LensArtefactsParams0.y
	//#define LensArtefactsRemappedMinMult			LensArtefactsParams0.z
	//#define LensArtefactsRemappedMaxMult			LensArtefactsParams0.w
	//#define LensArtefactsExposureDrivenFadeMult	LensArtefactsParams1.x

	const float ooRange = 1.0f/(Abs(m_maxExposureForFade-m_minExposureForFade)+SMALL_FLOAT);
	LensArtefactsParams0.x = m_minExposureForFade;
	LensArtefactsParams0.y = ooRange;
	LensArtefactsParams0.z = m_exposureFadeMinMult;
	LensArtefactsParams0.w = m_exposureFadeMaxMult;

	LensArtefactsParams1.Zero();
	LensArtefactsParams1.x = m_exposureFadeContribution;
}


//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void LensArtefacts::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Lens Artefacts");
		bank.AddToggle("Enabled", &m_bIsActive);
		bank.AddToggle("Use Interleaved Blur", &m_bUseInterleavedBlur);
		bank.AddToggle("Combine Layer Rendering", &m_bCombineLayerRendering);
		bank.AddSlider("Global Intensity Multiplier", &m_globalIntensity, 0.0f, 1.0f, 0.001f);
		bank.PushGroup("Exposure Based Intensity");
			bank.AddToggle("Override TC Exposure Min/Max", &m_bOverrideTimecycle);
			bank.AddSlider("Min. Exposure", &m_minExposureForFade, -10.0f, 10.0f, 0.001f);
			bank.AddSlider("Max. Exposure", &m_maxExposureForFade, -10.0f, 10.0f, 0.001f);
			bank.AddSlider("Multiplier for Min. Exposure", &m_exposureFadeMinMult, 0.0f, 10.0f, 0.001f);
			bank.AddSlider("Multiplier for Max. Exposure", &m_exposureFadeMaxMult, 0.0f, 10.0f, 0.001f);
			bank.AddSlider("Exposure Multiplier Blend", &m_exposureFadeContribution, 0.0f, 1.0f, 0.001f);
		bank.PopGroup();
		bank.AddSeparator();
		bank.AddToggle("Solid Border Sampling", &m_bUseSolidBorderSampler);
		bank.AddSeparator();
		m_editor.AddWidgets(bank);
	bank.PopGroup();
}
#endif

#if __BANK
atHashString LensArtefacts::EditTool::ms_newLayerName = "[New Layer]";

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::Update()
{
	// Allow live edit of current layer if it's not a new one that hasn't been added 
	// to the manager
	if (m_layerNamesComboIdx <= 0)
		return;

	atHashString currentLayerName = m_layerNames[m_layerNamesComboIdx];

	LensArtefact* pCurrentLayer = LENSARTEFACTSMGR.Get(currentLayerName);

	if (pCurrentLayer)
	{
		LENSARTEFACTSMGR.Copy(pCurrentLayer, &m_editableLayer);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::Init()
{
	m_layerNames.Reset();
	m_layerNames.Reserve(4);

	m_layerNamesComboIdx = -1;
	m_pLayerNamesCombo = NULL;

	m_editableLayer.Reset();

	UpdateLayerNamesList(LENSARTEFACTSMGR.m_sortedLayers);
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::Shutdown()
{
	m_layerNames.Reset();
	m_layerNamesComboIdx = -1;
	m_pLayerNamesCombo = NULL;
	m_editableLayer.Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::AddWidgets(rage::bkBank& bank )
{
	UpdateLayerNamesList(LENSARTEFACTSMGR.m_sortedLayers);

	bank.PushGroup("Edit Layers");

		m_pLayerNamesCombo = bank.AddCombo("Select Layer To Edit", &m_layerNamesComboIdx, m_layerNames.GetCount(), &m_layerNames[0], datCallback(MFA(LensArtefacts::EditTool::OnLayerNameSelected), (datBase*)this));
		bank.AddSeparator();
		bank.PushGroup("Current Layer Selected For Edit", false);
			PARSER.AddWidgets(bank, &m_editableLayer);
			bank.AddSeparator();
			bank.AddButton("Save Current Layer", datCallback(MFA(LensArtefacts::EditTool::OnSaveCurrentLayer), (datBase*)this));
			bank.AddButton("Delete Current Layer", datCallback(MFA(LensArtefacts::EditTool::OnDeleteCurrentLayer), (datBase*)this));
			bank.AddSeparator();
			bank.AddButton("SAVE DATA", datCallback(MFA(LensArtefacts::EditTool::OnSaveData), (datBase*)this));
			bank.AddButton("RELOAD DATA", datCallback(MFA(LensArtefacts::EditTool::OnLoadData), (datBase*)this));
			bank.AddSeparator();
		bank.PopGroup();

	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::UpdateLayerNamesList(const fwLinkList<LensArtefact*>&  layers)
{

	m_layerNames.Reset();
	m_layerNames.PushAndGrow(ms_newLayerName.TryGetCStr());

	SortedLayerkNode* pNode = layers.GetFirst()->GetNext();
	while(pNode != layers.GetLast())
	{
		LensArtefact* pCurEntry = (pNode->item);
		pNode = pNode->GetNext();
		m_layerNames.PushAndGrow(pCurEntry->GetName().TryGetCStr());
	}

	m_layerNamesComboIdx = 0;
	if (m_pLayerNamesCombo != NULL)
	{
		m_pLayerNamesCombo->UpdateCombo("Select Layer To Edit", &m_layerNamesComboIdx, m_layerNames.GetCount(), &m_layerNames[0], datCallback(MFA(LensArtefacts::EditTool::OnLayerNameSelected), (datBase*)this));
	}

}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::OnSaveCurrentLayer()
{
	int idx = m_layerNamesComboIdx;
	if (idx < 0 || idx > m_layerNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected, add the entry to the collection
	atHashString selectionName = atHashString(m_layerNames[idx]);

	if (m_editableLayer.IsValid())
	{
		if (selectionName == ms_newLayerName)
		{
			// only add to the collection is another layer with the same
			// name doesn't exist already
			if (LENSARTEFACTSMGR.Get(m_editableLayer.GetName()) == NULL)
			{
				LENSARTEFACTSMGR.Add(m_editableLayer);
			}
		}
		else
		{
			// otherwise, try finding the layer in the collection
			// and update it
			LensArtefact* pLayer = LENSARTEFACTSMGR.Get(selectionName);
			if (pLayer != NULL) 
			{
				*pLayer = m_editableLayer;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::OnDeleteCurrentLayer()
{
	int idx = m_layerNamesComboIdx;
	if (idx < 0 || idx > m_layerNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected, add the entry to the collection
	atHashString selectionName = atHashString(m_layerNames[idx]);

	if (selectionName == ms_newLayerName)
	{
		m_editableLayer.Reset();
	}
	else
	{
		// otherwise, try finding the preset in the collection
		// and remove it
		const LensArtefact* pLayer = LENSARTEFACTSMGR.Get(selectionName);
		if (pLayer != NULL) 
		{
			LENSARTEFACTSMGR.Remove(pLayer->GetName());
			m_editableLayer.Reset();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::OnSaveData()
{

#if !__FINAL
	LENSARTEFACTSMGR.SaveMetaData();
#endif

	UpdateLayerNamesList(LENSARTEFACTSMGR.m_sortedLayers);
	m_editableLayer.Reset();
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::OnLoadData()
{
	if (LENSARTEFACTSMGR.LoadMetaData())
	{
		UpdateLayerNamesList(LENSARTEFACTSMGR.m_sortedLayers);
		m_editableLayer.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LensArtefacts::EditTool::OnLayerNameSelected()
{
	int idx = m_layerNamesComboIdx;
	if (idx < 0 || idx > m_layerNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected, add the entry to the collection
	atHashString selectionName = atHashString(m_layerNames[idx]);
	if (selectionName == ms_newLayerName)
	{
		m_editableLayer.Reset();
	}
	else
	{
		// otherwise, try finding the effect stack
		const LensArtefact* pArtefact = LENSARTEFACTSMGR.Get(selectionName);
		if (pArtefact != NULL) 
		{
			m_editableLayer = *pArtefact;
		}
	}
}
#endif // __BANK
