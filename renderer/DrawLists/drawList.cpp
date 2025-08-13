/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    drawlist.cpp
// PURPOSE : intermediate data between game entitites and command buffers. Gives us
// independent data which can be handed off to the render thread
// AUTHOR :  john.
// CREATED : 30/8/06
//
/////////////////////////////////////////////////////////////////////////////////
#include "renderer/DrawLists/drawList.h"

#include "fragment/tune.h"
#include "grcore/allocscope.h"
#include "grcore/config.h"
#include "grcore/device.h"
#include "grcore/effect.h"
#include "grcore/viewport.h"
#include "grmodel/shaderfx.h"
#include "rmcore/instance.h"
#include "grmodel/geometry.h"
#include "grmodel/matrixset.h"
#include "system/xtl.h"
#include "system/cache.h"
#include "rmptfx/ptxdrawlist.h"
#include "rmptfx/ptxeffectinst.h"
#include "rmptfx/ptxmanager.h"
#include "rmptfx/ptxrenderstate.h"
#include "fragment/manager.h"

#include "vfx/ptfx/ptfxconfig.h"
#include "vfx/VisualEffects.h"

#if __D3D
#include "system/d3d9.h"
#endif
#include "renderer/color.h"
#include "Animation/AnimBones.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/MiniMap.h"
#include "frontend/MiniMapRenderThread.h"
#include "frontend/NewHud.h"
#if GTA_REPLAY
#include "frontend/VideoEditor/ui/Playback.h"
#include "frontend/VideoEditor/ui/Timeline.h"
#endif
#include "scene/entity.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/ped.h"
#include "peds/rendering/pedVariationPack.h"
#include "physics/breakable.h"
#include "pickups/PickupManager.h"
#include "vehicles/vehicle.h"
#include "renderer\Util\ShaderUtil.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LightSource.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/renderThread.h"
#include "renderer/water.h"
#include "scene/lod/LodDrawable.h"
#include "shaders/customshadereffectbase.h"
#include "Shaders/CustomShaderEffectPed.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "shaders/CustomShaderEffectAnimUV.h"
#include "shaders/CustomShaderEffectWeapon.h"
#include "shaders/CustomShaderEffectTint.h"
#include "shaders/shaderLib.h"
#include "streaming/streamingdebug.h"
#include "system/system.h"
#include "system/taskscheduler.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "TimeCycle/TimeCycle.h"
#include "Vehicles/vehicleLightSwitch.h"
#include "vehicles/wheel.h"		// CWheelInstanceRenderer
#include "vfx/misc/Coronas.h"
#include "vfx/misc/FogVolume.h"
#include "Vfx/misc/LODLightManager.h"
#include "vfx/misc/ScriptIM.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxEntity.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/vehicleglass/VehicleGlassManager.h"
#include "renderer/RenderPhases/renderPhaseDebugNY.h"
#include "renderer/RenderPhases/renderPhaseDefLight.h"
#include "renderer/RenderPhases/renderPhaseStd.h"
#include "renderer/RenderPhases/RenderPhaseTreeImposters.h"

#include "debug/Rendering/DebugRendering.h"

#include "profile/cputrace.h"
#include "grprofile/gcmtrace.h"

#if	__PPU
#include "grcore/wrapper_gcm.h"
using namespace cell::Gcm;
#include "system/gcmPerfmon.h"
#endif

#if __PS3 && SPU_GCM_FIFO
#include "grmodel/grmodelspu.h"
#endif

//#define MAX_COPIED_SKELETONS	(45)

RENDER_OPTIMISATIONS()


#if __DEV && __PPU
#include <sys/gpio.h>
bool bLightsToScreen;
#endif

//u32 CDrawCommandBuffer::m_execCommandIdx = 0;

DECLARE_MTR_THREAD CCustomShaderEffectBase* CCustomShaderEffectDC::ms_pLatestShader = NULL;
//grcInstanceData*		dlCmdInstData::ms_pLatestInstData = NULL;
ThreadVector3					CSetDrawableLODCalcPos ::ms_RenderLockLODCalcPos;

//#######################################
// --- CDrawCommandBuffer stuff ---

void CDrawCommandBuffer::Initialise(){

	dlDrawCommandBuffer::Initialise();

	DLC_REGISTER(CDrawEntityDC);
	DLC_REGISTER(CDrawEntityFmDC);
	DLC_REGISTER(CDrawEntityInstancedDC);
	DLC_REGISTER(CDrawGrassBatchDC);
	DLC_REGISTER(CDrawSkinnedEntityDC);
	DLC_REGISTER(CDrawPedBIGDC);
	DLC_REGISTER(CDrawStreamPedDC);
	DLC_REGISTER(CDrawDetachedPedPropDC);
	DLC_REGISTER(CDrawVehicleVariationDC);

	DLC_REGISTER(CDrawFragDC);
	DLC_REGISTER(CDrawFragTypeDC);
#if ENABLE_FRAG_OPTIMIZATION
	DLC_REGISTER(CDrawFragTypeVehicleDC);
#endif // ENABLE_FRAG_OPTIMIZATION

	DLC_REGISTER(CDrawPrototypeBatchDC);

	DLC_REGISTER(CCustomShaderEffectDC);

	DLC_REGISTER(dlCmdBeginOcclusionQuery);
	DLC_REGISTER(dlCmdEndOcclusionQuery);
	
	DLC_REGISTER(dlCmdAddSkeleton);
	DLC_REGISTER(dlCmdAddCompositeSkeleton);
	DLC_REGISTER(dlCmdDrawTwoSidedDrawable);

	DLC_REGISTER(CMiniMap_UpdateBlips);
	DLC_REGISTER(CMiniMap_ResetBlipConeFlags);
	DLC_REGISTER(CMiniMap_RemoveUnusedBlipConesFromStage);
	DLC_REGISTER(CMiniMap_RenderBlipCone);
	DLC_REGISTER(CMiniMap_AddSonarBlipToStage);
	DLC_REGISTER(CMiniMap_RenderState_Setup);
	DLC_REGISTER(CDrawSpriteDC);
	DLC_REGISTER(CDrawSpriteUVDC);
	DLC_REGISTER(CDrawUIWorldIcon);
	DLC_REGISTER(CDrawRectDC);
	DLC_REGISTER(CDrawRadioHudTextDC);
	DLC_REGISTER(CRenderTextDC);

	DLC_REGISTER(CDrawPtxEffectInst);
	DLC_REGISTER(CDrawVehicleGlassComponent);

#if __PPU
////////	DLC_REGISTER(CShadowToDepthPatch);
////////	DLC_REGISTER(CDepthToShadowPatch);
#endif // __PPU

	DLC_REGISTER(CSetDrawableLODCalcPos);
	DLC_REGISTER(CDrawBreakableGlassDC);

	DLC_REGISTER(dlCmdOverrideSkeleton);

#if DRAWABLE_STATS
	DLC_REGISTER(CSetDrawableStatContext);
#endif
	
#if USE_TREE_IMPOSTERS
	CRenderPhaseTreeImposters::InitDLCCommands();
#endif // USE_TREE_IMPOSTERS
	
	CRenderListBuilder::InitDLCCommands();
	CViewportManager::InitDLCCommands();
	Water::InitDLCCommands();
	CRenderPhaseDrawSceneInterface::InitDLCCommands();
	CTimeCycle::InitDLCCommands();
	Lights::InitDLCCommands();
	CVisualEffects::InitDLCCommands();
	CPickupManager::InitDLCCommands();
	CPedDamageManager::InitDLCCommands();
	
#if DEBUG_DISPLAY_BAR
	CRenderPhaseDebug::InitDLCCommands();
#endif // DEBUG_DISPLAY_BAR
}

void CDrawCommandBuffer::Shutdown(){

	dlDrawCommandBuffer::Shutdown();
}

#if !__FINAL
void CDrawCommandBuffer::DumpMemoryInfo()
{
	CStreamingDebug::FullMemoryDump();
}
#endif // !__FINAL

//atFixedArray<CCustomShaderEffectBase*, 150> s_CopiedShaders;	// list of shader data which has been copied off this frame	

// for reset just start adding at start & overwrite stuff
void CDrawCommandBuffer::Reset(){

	dlDrawCommandBuffer::Reset();

/*	int count = s_CopiedShaders.GetCount();
	for (int x=0; x<count; x++)
		s_CopiedShaders[x]->RemoveKnownRefs();
	s_CopiedShaders.Reset();*/
}

void	CDrawCommandBuffer::FlipBuffers(void) { 

/*	int count = s_CopiedShaders.GetCount();
	for (int x=0; x<count; x++)
		s_CopiedShaders[x]->RemoveKnownRefs();
	s_CopiedShaders.Reset();*/
	ScriptIM::Clear();
	dlDrawCommandBuffer::FlipBuffers();
}

void CopyOffShader(const CCustomShaderEffectBase* pShader, u32 dataSize, DrawListAddress &drawListOffset, bool bDoubleBuffered, CCustomShaderEffectBaseType *pType)
{
	DL_PF_FUNC( CopyOffShader );

	Assert(pShader);

	const CCustomShaderEffectBase*	pData = pShader;

	drawListOffset = gDCBuffer->LookupSharedData(DL_MEMTYPE_SHADER, pData->GetSharedDataOffset());	// lookup to see if this shader has already been copied

	if (!drawListOffset.IsNULL())
	{
		// don't add a data block in this case - we are passing back the drawListOffset instead to use the existing copy
	}
	else
	{
		CCustomShaderEffectBase* pDest;
		if(bDoubleBuffered)
		{
			// PS3: add shader data to double pages that have long enough lifetime that drawablespu/EDGE have chance to fetch something valid;
			const eDrawListPageType pageType = RSG_PS3 ? DPT_LIFETIME_GPU : DPT_LIFETIME_RENDER_THREAD;
			// align dataSize to 16 bytes:
			dataSize += 0xf;
			dataSize &= 0xfffffff0;
			pDest = reinterpret_cast<CCustomShaderEffectBase *>(gDCBuffer->AllocatePagedMemory(pageType, dataSize, false, 16, drawListOffset));
		}
		else
		{
			pDest = (CCustomShaderEffectBase*)gDCBuffer->AddDataBlock(NULL, dataSize, drawListOffset);		// add an empty data block big enough for mtxs
		}
		gDCBuffer->AllocateSharedData(DL_MEMTYPE_SHADER, pData->GetSharedDataOffset(), dataSize, drawListOffset);

		sysMemCpy((void*)pDest, (void*)pData, dataSize);

		// this is a little scary -- we're calling a virtual on a binary copy of an object.
		// minor optimization - don't add the shader to the list if we didn't do any work
//		if (pDest->AddKnownRefs())
//			s_CopiedShaders.Append() = pDest;

		pType->AddRef();
		gDrawListMgr->AddRefCountedCustomShaderEffectType(pType);
	}
}

DrawListAddress::Parameter	CDrawCommandBuffer::LookupWriteShaderCopy(const CCustomShaderEffectBase* originalAddr){ 

	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));		// this command is forbidden outwith the main thread
	Assert(originalAddr);

	return gDCBuffer->LookupSharedData(DL_MEMTYPE_SHADER, originalAddr->GetSharedDataOffset());
}

// ----------------------------------------------------------------------------------------------- //


void CMiniMap_ResetBlipConeFlags::Execute()
{
	CMiniMap_RenderThread::ResetBlipConeFlags();
}


void CMiniMap_RemoveUnusedBlipConesFromStage::Execute()
{
	CMiniMap_RenderThread::RemoveUnusedBlipConesFromStage();
}


void CMiniMap_RenderBlipCone::Execute()
{
	CMiniMap_RenderThread::AddOrUpdateConeToBlipOnStage(m_BlipCone);
}


// update blips
CMiniMap_AddSonarBlipToStage::CMiniMap_AddSonarBlipToStage(const sSonarBlipStruct &sonarBlip)
{
	m_SonarBlip.Update(sonarBlip);
}

void CMiniMap_AddSonarBlipToStage::Execute()
{
	CMiniMap_RenderThread::AddSonarBlipToStage(&m_SonarBlip);
}

CMiniMap_RenderState_Setup::CMiniMap_RenderState_Setup(sMiniMapRenderStateStruct *pMiniMapRenderStruct)
{
	sysMemCpy(&m_StoredMiniMapRenderStruct, pMiniMapRenderStruct, sizeof(sMiniMapRenderStateStruct));	  // make a copy of the player struct
}

void CMiniMap_RenderState_Setup::Execute()
{
	// copy into the global
	CMiniMap_RenderThread::SetMiniMapRenderState(m_StoredMiniMapRenderStruct);
	CMiniMap_RenderThread::PrepareForRendering();
}


void CMiniMap_UpdateBlips::Execute()
{ 
	CMiniMap_Common::RenderUpdate(); 
}

CDrawRadioHudTextDC::CDrawRadioHudTextDC(Vector2 pos[4], const grcTexture* pTex, Color32 col)
{
	m_pos[0] = pos[0];
	m_pos[1] = pos[1];
	m_pos[2] = pos[2];
	m_pos[3] = pos[3];
	m_pTex = pTex;
	m_col = col;
}

void CDrawRadioHudTextDC::Execute()
{
	Vector2 texCoord[4];
	texCoord[0] = Vector2(0.0f, 1.0f);
	texCoord[1] = Vector2(0.0f, 0.0f);
	texCoord[2] = Vector2(1.0f, 1.0f);
	texCoord[3] = Vector2(1.0f, 0.0f);

	grcBindTexture(m_pTex);
	grcBegin(drawTriStrip, 4);
	for (s32 i = 3; i >= 0 ; i--)
	{
		grcVertex(m_pos[i].x, m_pos[i].y, 0.0f, 0, 0, -1, m_col, texCoord[i].x, texCoord[i].y);
	}
	grcEnd();
}

CRenderTextDC::CRenderTextDC(CTextLayout Layout, Vector2 vPosition, char *pString)
{
	m_TextLayout = Layout;
	m_vPos = vPosition;
	safecpy(m_cTextString, &pString[0]);
}

void CRenderTextDC::Execute()
{
	m_TextLayout.Render(m_vPos, &m_cTextString[0]);
	CText::Flush();
}

CDrawSpriteDC::CDrawSpriteDC(Vector2& v1, Vector2& v2, Vector2& v3, Vector2& v4, Color32 col, const grcTexture *pTex)
{
	m_pos[0] = v1;
	m_pos[1] = v2;
	m_pos[2] = v3;
	m_pos[3] = v4;
	m_pTex = pTex;
	m_col = col;
}

void CDrawSpriteDC::Execute()
{

	grcBindTexture(m_pTex);
	CSprite2d::Draw(m_pos[0], m_pos[1], m_pos[2], m_pos[3], m_col);
}

CDrawSpriteUVDC::CDrawSpriteUVDC(Vector2& v1, Vector2& v2, Vector2& v3, Vector2& v4, Vector2& vU1, Vector2& vU2, Vector2& vU3, Vector2& vU4, Color32 col, const grcTexture *pTex, s32 txdId, grcBlendStateHandle blendState)
{
	m_pos[0] = v1;
	m_pos[1] = v2;
	m_pos[2] = v3;
	m_pos[3] = v4;
	m_pos[4] = vU1;
	m_pos[5] = vU2;
	m_pos[6] = vU3;
	m_pos[7] = vU4;
	m_pTex = pTex;
	m_col = col;
	m_blendState = blendState;

	if (txdId != -1) {
		gDrawListMgr->AddTxdReference(txdId);
	}

}

void CDrawSpriteUVDC::Execute()
{
	CSprite2d::SetRenderState(m_pTex);
	grcStateBlock::SetBlendState(m_blendState);
	CSprite2d::SetGeneralParams(Vector4(1.0f,1.0f,1.0f,1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
	CSprite2d::DrawRect(m_pos[0], m_pos[1], m_pos[2], m_pos[3], 0.0f, m_pos[4], m_pos[5], m_pos[6], m_pos[7], m_col);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	CSprite2d::ClearRenderState();
}

CDrawUIWorldIcon::CDrawUIWorldIcon(Vector3& v1, Vector3& v2, Vector3& v3, Vector3& v4, Vector2& vU1, Vector2& vU2, Vector2& vU3, Vector2& vU4, Color32 col, const grcTexture *pTex, s32 txdId, grcBlendStateHandle blendState)
{
	m_pos[0] = v1;
	m_pos[1] = v2;
	m_pos[2] = v3;
	m_pos[3] = v4;
	m_uv[0] = vU1;
	m_uv[1] = vU2;
	m_uv[2] = vU3;
	m_uv[3] = vU4;
	m_pTex = pTex;
	m_col = col;
	m_blendState = blendState;

	if (txdId != -1) {
		gDrawListMgr->AddTxdReference(txdId);
	}

}

void CDrawUIWorldIcon::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CSprite2d::SetRenderState(m_pTex);
	grcStateBlock::SetBlendState(m_blendState);
	CSprite2d::SetGeneralParams(Vector4(1.0f,1.0f,1.0f,1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
	CSprite2d::DrawUIWorldIcon(m_pos[0], m_pos[1], m_pos[2], m_pos[3], 0.0f, m_uv[0], m_uv[1], m_uv[2], m_uv[3], m_col);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	CSprite2d::ClearRenderState();
}


CDrawRectDC::CDrawRectDC(const fwRect& rect, Color32 col)
{
	m_rect = rect;
	m_col = col;
}


void CDrawRectDC::Execute(){

	CSprite2d::DrawRect(m_rect, m_col);
}

CDrawPrototypeBatchDC::CDrawPrototypeBatchDC(IDrawListPrototype * prototype, u32 batchSize, DrawListAddress data)
: m_Prototype(prototype)
, m_BatchSize(batchSize)
, m_Data(data)
{
}

void CDrawPrototypeBatchDC::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	void *data = gDCBuffer->ResolveDrawListAddress(m_Data);

	Assert(m_Prototype);
	Assert(data);

	m_Prototype->Draw(m_BatchSize, data);
}

//#################################################################################
CCustomShaderEffectDC::CCustomShaderEffectDC(const CCustomShaderEffectBase& shaderEffect, u32 modelIndex, bool bExecuteShader, CCustomShaderEffectBaseType *pType)
{
	Assertf(pType, "parameter CCustomShaderEffectBaseType *pType is NULL");
	m_bResetLatestShader	= FALSE;

	m_shaderOffset			= 0;
	m_bExecute				= bExecuteShader;
	Assert(shaderEffect.Size() < 65535);
	m_dataSize				= (u16)shaderEffect.Size();
#if __PS3
	m_bDoubleBuffered		= (shaderEffect.GetType()==CSE_TINT);	// ps3: only double buffering of tints allowed
#else
	m_bDoubleBuffered		= false;
#endif
	CopyOffShader(&shaderEffect, m_dataSize, m_shaderOffset, m_bDoubleBuffered, pType);

	m_modelIndex = modelIndex;

#if __DEV
	m_shaderOffsetDup = m_shaderOffset;
#endif // __DEV

	// alignment check
	if(!m_shaderOffset.IsNULL())
	{
		Assert((m_shaderOffset & 0x03) == 0);
	}

	m_bBrokenOffPart = false;
	if (shaderEffect.GetType() == CSE_VEHICLE)
	{
		const CCustomShaderEffectVehicle* pSEV = static_cast<const CCustomShaderEffectVehicle*>(&shaderEffect);
		m_bBrokenOffPart = pSEV->GetIsBrokenOffPart();

		// we are using the texture on this frag - make sure it sticks around until all the rendering is flushed through
		strLocalIndex liveryFragSlotIdx = pSEV->GetLiveryFragSlotIdx();
		if (liveryFragSlotIdx != strLocalIndex::INVALID_INDEX)
		{
			g_FragmentStore.AddRef(liveryFragSlotIdx, REF_RENDER);								
			gDrawListMgr->AddRefCountedModuleIndex(liveryFragSlotIdx.Get(), &g_FragmentStore);
		}
	}
}

// "Reset latest shader" command constructor:
CCustomShaderEffectDC::CCustomShaderEffectDC(void* ASSERT_ONLY(ptr))
{
	Assert(ptr==NULL);
	m_bResetLatestShader	= TRUE;
	m_shaderOffset			= 0;
	m_bExecute				= FALSE;
	m_dataSize				= 0;
	m_modelIndex			= 0;
#if __DEV
	m_shaderOffsetDup		= m_shaderOffset;
#endif // __DEV
	m_bBrokenOffPart		= false;
}

static rage::fragType* pFragType_DEBUG = NULL;
static CVehicleModelInfo* pVehicleModelInfo_DEBUG = NULL;
static CCustomShaderEffectVehicle* pCSE_DEBUG = NULL;

void CCustomShaderEffectDC::Execute()
{
	if(m_bResetLatestShader)
	{	// reset latest shader effect ptr only:
		ms_pLatestShader = NULL;
	}
	else
	{	// full execute of shader:
	#if __DEV
/*		if (m_shaderOffset != m_shaderOffsetDup){
			printf("m_shaderOffset = %x\n",m_shaderOffset);
			printf("m_shaderOffsetDup = %x\n\n",m_shaderOffsetDup);
		}

		Assert(m_shaderOffset == m_shaderOffsetDup);*/
	#endif //__DEV

		// alignment check
		if (!m_shaderOffset.IsNULL())
		{
			Assert((m_shaderOffset & 0x03) == 0);
		}

		//ExecuteShader(m_dataSize, m_shaderOffset, m_bExecute);
		void* pData;
		if(m_bDoubleBuffered)
		{
			pData = gDCBuffer->ResolveDrawListAddress(m_shaderOffset);
		}
		else
		{
			pData = gDCBuffer->GetDataBlock(m_dataSize, m_shaderOffset);
		}
		Assert(pData);

		CCustomShaderEffectBase* pShader = static_cast<CCustomShaderEffectBase*>(pData);

		if(pShader)
		{
			if (m_bExecute)
			{
				rmcDrawable* pDrawable = NULL;

				CBaseModelInfo* pMI = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_modelIndex)));
				Assert(pMI);
				if (pMI)
				{
					pDrawable = pMI->GetDrawable();
					
					if(pMI->GetModelType()== MI_TYPE_VEHICLE)
					{
						CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pMI);
						Assert(pShader->GetType()==CSE_VEHICLE);
						CCustomShaderEffectVehicle* pSEV = static_cast<CCustomShaderEffectVehicle*>(pShader);
						if (pSEV->GetCseType()->GetIsHighDetail())
						{
							Assert(pVMI);
							Assert(pSEV);
							Assert(pSEV->GetCseType());
							gtaFragType* pFragType = pVMI->GetHDFragType();
							if (pFragType != NULL)
							{
								pFragType_DEBUG = pFragType;
								pVehicleModelInfo_DEBUG = pVMI;
								pCSE_DEBUG = pSEV;
								pDrawable = pFragType->GetCommonDrawable();
							} 
							else 
							{
								pDrawable = NULL;		// if shader effect is HD but matching model not available...
							}
						}
					}
					else if(pMI->GetModelType()== MI_TYPE_WEAPON)
					{
						CWeaponModelInfo* pWMI = static_cast<CWeaponModelInfo*>(pMI);

						bool IsDrawableHD=false;
						// CSEWeapon is cascaded - may be a CSEWeapon or CSETint:
						if(pShader->GetType()==CSE_WEAPON)
						{
							CCustomShaderEffectWeapon* pCSEWeapon = static_cast<CCustomShaderEffectWeapon*>(pShader);
							IsDrawableHD =  pCSEWeapon->GetCseType()->GetIsHighDetail();
						}
						else if(pShader->GetType()==CSE_TINT)
						{
							CCustomShaderEffectTint* pCSETint = static_cast<CCustomShaderEffectTint*>(pShader);
							IsDrawableHD = pCSETint->GetCseType()->GetIsHighDetail();
						}

						if(IsDrawableHD)
						{
							if(pWMI->GetAreHDFilesLoaded())
							{
								pDrawable = pWMI->GetHDDrawable();
							} 
							else 
							{
								pDrawable = NULL;		// if shader effect is HD but matching model not available...
							}
						}
					}
				}
				
				if (pDrawable)
				{
					if(pShader->GetType()==CSE_VEHICLE)
					{
						CCustomShaderEffectVehicle* pSEV = static_cast<CCustomShaderEffectVehicle*>(pShader);
						Assert(pSEV);
						pSEV->SetShaderVariables(pDrawable, Vector3(0,0,0), m_bBrokenOffPart);
					}
					else
					{
						pShader->SetShaderVariables(pDrawable);
					}
				}
			}
		}
		ms_pLatestShader = pShader;

	}// m_bResetLatestShader...

}// end of CCustomShaderEffectDC::Execute()...

//
//
// returns direct ptr to CSE instance stored in gDCBuffer:
//
CCustomShaderEffectBase* CCustomShaderEffectDC::GetCsePtr()
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));	// allowed only during DL creation

	// alignment check
	if (!m_shaderOffset.IsNULL())
	{
		Assert((m_shaderOffset & 0x03) == 0);
	}

void* pData = NULL;
	if(m_bDoubleBuffered)
	{
		pData = gDCBuffer->ResolveDrawListAddress(m_shaderOffset);
	}
	else
	{
		pData = gDCBuffer->GetDataBlockEx(this, m_shaderOffset);
	}

	Assert(pData);
	CCustomShaderEffectBase* pShader = static_cast<CCustomShaderEffectBase*>(pData);

	return(pShader);
}




CDrawPtxEffectInst::CDrawPtxEffectInst(CPtFxSortedEntity *prtEntity BANK_ONLY(, bool debugRender))
{
#if PTFX_ALLOW_INSTANCE_SORTING	
	m_effect = prtEntity->GetFxInst();
#endif // PTFX_ALLOW_INSTANCE_SORTING	
#if __BANK
	m_debugRender = debugRender;
#endif // __BANK
}

void CDrawPtxEffectInst::Execute()
{
#if PTFX_ALLOW_INSTANCE_SORTING
	if( m_effect->GetIsFinished() )
	{
		// skip that one, it's dead...
		// Due to the way we do our rendering, an effect could die while still
		// being in the render pipe, so we just silently ignore those...
		return;
	}
	Assert(m_effect->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW));
	Assert(m_effect->GetFlag(PTXEFFECTFLAG_KEEP_RESERVED));
	Assert(m_effect->GetFlag(PTFX_RESERVED_SORTED));

	CShaderLib::SetGlobalEmissiveScale(g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_PTFX_EMISSIVE_INTENSITY_MULT), true);

	// pfff.... sorted particle fx are specialz, so we need to store and restore those.
	grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
	grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
	grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;
	u8 ActiveStencilRef_Backup = grcStateBlock::ActiveStencilRef;
	u32 ActiveBlendFactors_Backup = grcStateBlock::ActiveBlendFactors;
	u64 ActiveSampleMask_Backup = grcStateBlock::ActiveSampleMask;

	g_ptFxManager.RenderManual(m_effect BANK_ONLY(, m_debugRender));

	grcStateBlock::SetDepthStencilState(DSS_Backup,ActiveStencilRef_Backup);
	grcStateBlock::SetRasterizerState(RS_Backup);
	grcStateBlock::SetBlendState(BS_Backup,ActiveBlendFactors_Backup,ActiveSampleMask_Backup);
	
	CShaderLib::SetGlobalEmissiveScale(1.0f);
#endif // PTFX_ALLOW_INSTANCE_SORTING	
}


CDrawVehicleGlassComponent::CDrawVehicleGlassComponent(CVehicleGlassComponentEntity *vehGlassEntity BANK_ONLY(, bool debugRender))
{
	m_vehGlassComponent = vehGlassEntity->GetVehicleGlassComponent();
#if __BANK
	m_debugRender = debugRender;
#endif // __BANK
}

void CDrawVehicleGlassComponent::Execute()
{
	if(m_vehGlassComponent == NULL)
	{
		return;
	}
	g_vehicleGlassMan.RenderManual(m_vehGlassComponent);

}

CSetDrawableLODCalcPos::CSetDrawableLODCalcPos(const Vector3& pos){
	m_LODCalcPos = pos;
}

extern bool g_render_lock;
void CSetDrawableLODCalcPos::Execute(){

	if (g_render_lock){
		gDrawListMgr->SetCalcPosDrawableLOD((Vector3&)ms_RenderLockLODCalcPos);
	} else
		gDrawListMgr->SetCalcPosDrawableLOD(m_LODCalcPos);
}

#if DRAWABLE_STATS
CSetDrawableStatContext::CSetDrawableStatContext(u16 stat){
	m_DrawableStatContext = stat;
}

void CSetDrawableStatContext::Execute(){
		gDrawListMgr->SetDrawableStatContext(m_DrawableStatContext);
}
#endif // DRAWABLESPU_STATS || DRAWABLE_STATS

dlCmdBeginOcclusionQuery::dlCmdBeginOcclusionQuery(grcOcclusionQuery q)
{
	query = q;
}


void dlCmdBeginOcclusionQuery::Execute()
{
	GRCDEVICE.BeginOcclusionQuery(query);
}

dlCmdEndOcclusionQuery::dlCmdEndOcclusionQuery(grcOcclusionQuery q)
{
	query = q;
}

void dlCmdEndOcclusionQuery::Execute()
{
	GRCDEVICE.EndOcclusionQuery(query);
}
