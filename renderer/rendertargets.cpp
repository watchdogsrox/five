//
// renderer/rendertargets.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
#include "rendertargets.h"

// Rage headers
#include "bank/bank.h"
#include "bank/toggle.h"
#include "bank/combo.h"
#include "grcore/allocscope.h"
#include "grcore/gfxcontext.h"
#include "grcore/texture.h"
#include "grcore/texturegcm.h"
#include "grcore/texturexenon.h"
#include "grcore/wrapper_gcm.h"
#include "grcore/quads.h"
#include "gpuptfx/ptxgpubase.h"
#include "system/param.h"
#include "system/memory.h"
#if __XENON
#include "system/xtl.h"
#include "system/xgraphics.h"
#elif __PPU
#include "diag/tracker.h"
#endif // __PPU

// Framework headers
#include "grcore/debugdraw.h"

#if __WIN32PC || RSG_DURANGO
#include "grcore/texturepc.h"
#endif // __WIN32PC || RSG_DURANGO

#if __D3D11
#include "grcore/texture_d3d11.h"
#include "grcore/texturefactory_d3d11.h"
#include "grcore/rendertarget_d3d11.h"
#endif

#if RSG_ORBIS
#include "grcore/rendertarget_gnm.h"
#include "grcore/texturefactory_gnm.h"
#endif

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "debug/debug.h"
#include "renderer/deferred/gbuffer.h"
#include "renderer/Mirrors.h"
#include "renderer/RenderPhases/RenderPhaseEntitySelect.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/sprite2d.h"
#include "renderer/shadows/shadows.h"
#include "renderer/SSAO_shared.h"
#include "renderer/Water.h"
#include "shaders/shaderlib.h"
#include "../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"
#include "control/replay/ReplaySupportClasses.h"

RENDER_OPTIMISATIONS()

#if DEVICE_EQAA
// Create full EQAA coverage for the back buffer and any other RT that is used with the depth buffer
// required to avoid DX runtime assertions on depth MSAA vs color MSAA mismatches
static bool UseFullBackbufferCoverage(const grcDevice::MSAAMode&)
{
	//return !RSG_ORBIS;	// doesn't work correctly somehow
	return false;
}
#endif //DEVICE_EQAA

#if !__FINAL && (__D3D11 || RSG_ORBIS)
XPARAM(DX11Use8BitTargets);
#endif // !__FINAL && (__D3D11 || RSG_ORBIS)

PARAM(gamewidth,"Set width of game's backbuffer/gbuffer)");
PARAM(gameheight,"Set height of game's backbuffer/gbuffer");
PARAM(highColorPrecision, "Switch RGB11F to RGBAF16 format for PC");

#define KB64 (64 * 1024)

u16 CRenderTargets::ms_RenderTargetMemPoolArray[RTMEMPOOL_SIZE];
DECLARE_MTR_THREAD bool CRenderTargets::ms_RenderTargetActive = false;
bool CRenderTargets::ms_DepthDownsampleReady = false;;
grcRenderTarget* CRenderTargets::ms_BackBuffer	= NULL;
#if DYNAMIC_BB
grcRenderTarget* CRenderTargets::ms_AltBackBuffer	= NULL;
#endif

#if __XENON
// Aliases the 7e3 float back buffer as a 7e3 packed integer buffer
grcRenderTarget* CRenderTargets::ms_BackBuffer7e3ToIntAlias = NULL;
grcRenderTarget* CRenderTargets::ms_DepthBuffers[3]	= {NULL, NULL, NULL};
static grcRenderTarget* ms_DepthBufferAliases[3]	= {NULL, NULL, NULL};
#else
grcRenderTarget* CRenderTargets::ms_DepthBuffer = NULL;
grcRenderTarget* CRenderTargets::ms_DepthBuffer_PreAlpha = NULL;
#endif //__XENON
#if __PS3
static grcRenderTarget* ms_UIDepthBuffer;
static grcRenderTarget* m_DepthTargetQuarterPadding;
#endif
static grcRenderTarget* m_DepthTargetQuarter;
static grcRenderTarget* m_DepthTargetQuarterLinear;
#if RSG_PC && __D3D11
static grcRenderTarget* m_DepthTargetQuarter_ReadOnly;
static grcRenderTarget* m_DepthTargetQuarter_Copy;
static grcRenderTarget* m_DepthBuffer_PreAlpha_ReadOnly;
#endif // RSG_PC && __D3D11
#if RSG_PC
static grcRenderTarget* ms_UIDepthBuffer = NULL;
grcRenderTarget* CRenderTargets::ms_UIDepthBuffer_PreAlpha = NULL;
grcRenderTarget* CRenderTargets::ms_UIDepthBuffer_Resolved = NULL;
grcRenderTarget* CRenderTargets::ms_UIDepthBuffer_Resolved_ReadOnly = NULL;
#endif

#if LIGHT_VOLUME_USE_LOW_RESOLUTION
grcRenderTarget* m_volumeOffscreenBuffer;
grcRenderTarget* m_volumeReconstructionBuffer;
#endif

#if __XENON
grcRenderTarget* m_miniMapOffscreenBuffer = NULL;
grcRenderTarget* m_miniMapOffscreenDepthBuffer = NULL;
grcRenderTarget* m_BackBufferCopy = NULL;
#endif

#if __PPU
grcRenderTarget* CRenderTargets::ms_PatchedDepthBuffer;

grcBlendStateHandle	CRenderTargets::ms_Refresh_BS;
grcDepthStencilStateHandle CRenderTargets::ms_RefreshStencil_DS;
grcDepthStencilStateHandle CRenderTargets::ms_RefreshStencilPreserveZFar_DS;
grcDepthStencilStateHandle CRenderTargets::ms_RefreshStencilZFar_DS;
grcDepthStencilStateHandle CRenderTargets::ms_RefreshZFar_G_DS;
grcDepthStencilStateHandle CRenderTargets::ms_RefreshZFar_L_DS;


bool CRenderTargets::ms_MSAARender = false;
bool CRenderTargets::ms_WithDepthRender = false;
#endif // __PPU

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
grcTexture *CRenderTargets::ms_ReplayThumbnailTex[REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE] = { NULL };
grcRenderTarget *CRenderTargets::ms_ReplayThumbnailRT[REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE] = { NULL };
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY

#if __BANK
grcBlendStateHandle CRenderTargets::ms_BlitTransparent_BS;
#endif // __BANK

grcDepthStencilStateHandle CRenderTargets::ms_CopyDepth_DS;

#if __PS3
grcDepthStencilStateHandle CRenderTargets::ms_CopyDepthClearStencil_DS;
#endif 

grcDepthStencilStateHandle  CRenderTargets::ms_CopyColorStencilMask_DS;
grcBlendStateHandle CRenderTargets::ms_AlphaMaskToStencilMask_BS = grcStateBlock::BS_Invalid;
grcDepthStencilStateHandle CRenderTargets::ms_AlphaMaskToStencilMask_DS = grcStateBlock::DSS_Invalid;


#if __WIN32PC || RSG_DURANGO
grcRenderTarget* CRenderTargets::ms_DepthBufferCopy = NULL;
grcRenderTarget* CRenderTargets::ms_BackBufferCopy = NULL;
#if __D3D11
grcRenderTarget* CRenderTargets::ms_DepthBuffer_Stencil = NULL;
grcRenderTarget* CRenderTargets::ms_DepthBufferCopy_Stencil = NULL;
grcRenderTarget* CRenderTargets::ms_DepthBuffer_ReadOnly = NULL;
#if RSG_PC
grcRenderTarget* CRenderTargets::ms_DepthBuffer_ReadOnlyDepth = NULL;
#endif

bool CRenderTargets::ms_Initialized = false;
#if RSG_PC
grcRenderTarget* CRenderTargets::ms_BackBufferRTForPause = NULL;
grcTexture* CRenderTargets::ms_BackBufferTexForPause = NULL;
#endif
#endif
#elif RSG_ORBIS
grcRenderTarget* CRenderTargets::ms_BackBufferCopy = NULL;
grcRenderTarget* CRenderTargets::ms_DepthBuffer_Stencil = NULL;
#endif // RSG_ORBIS

#if DEVICE_MSAA
grcRenderTarget* CRenderTargets::ms_BackBuffer_Resolved = NULL;
grcRenderTarget* CRenderTargets::ms_DepthBuffer_Resolved = NULL;
#if __D3D11
grcRenderTarget* CRenderTargets::ms_DepthBuffer_Resolved_ReadOnly = NULL;
#endif // __D3D11
#if ENABLE_PED_PASS_AA_SOURCE
grcRenderTarget* CRenderTargets::ms_BackBufferCopyAA = NULL;
#endif // ENABLE_PED_PASS_AA_SOURCE
#endif // DEVICE_MSAA

ASSERT_ONLY(static int g_CachedDownsampleDepthThreadID = -1;)

grcRenderTarget* CRenderTargets::GetBackBuffer(bool XENON_ONLY(realize))
{
#if __XENON

	if (realize)
	{
		Assertf(ms_RenderTargetActive, "Cannot resolve the scene buffer when it is not locked");
		grcResolveFlags flags;
		if(((grcRenderTargetXenon*)ms_BackBuffer)->GetSurface()->Format == D3DFMT_A2B10G10R10F_EDRAM)
			flags.ColorExpBias = -GRCDEVICE.GetColorExpBias();
		ms_BackBuffer->Realize(&flags);
	}

#elif !(__PPU || RSG_PC || RSG_DURANGO || RSG_ORBIS)
	ASSERT(0, "CRenderTargets::GetBackBuffer - Not Implemented");
#endif
#if DYNAMIC_BB
	return (ms_AltBackBuffer && sysThreadType::IsRenderThread() && static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->GetUseAltTarget()) ? ms_AltBackBuffer : ms_BackBuffer;
#else
#if RSG_PC
	if (PostFX::GetPauseResolve() == PostFX::IN_RESOLVE_PAUSE)
		return ms_BackBufferRTForPause;
	else
#endif
	return ms_BackBuffer;
#endif
}
#if DYNAMIC_BB
void CRenderTargets::InitAltBackBuffer()
{
	if (ms_AltBackBuffer)
	{
		ESRAMManager::ESRAMCopyIn(ms_AltBackBuffer, ms_BackBuffer);
		static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->SetUseAltTarget(true);
	}
}

void CRenderTargets::SetAltBackBuffer()
{
	if (ms_BackBuffer && ms_AltBackBuffer)
	{
		static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->SetUseAltTarget(true);
	}
}

void CRenderTargets::ResetAltBackBuffer()
{
	if (ms_BackBuffer)
	{
		static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->SetUseAltTarget(false);
	}
}

void CRenderTargets::FlushAltBackBuffer()
{
	if (ms_BackBuffer && ms_AltBackBuffer)
	{
		ESRAMManager::ESRAMCopyOut(ms_BackBuffer, ms_AltBackBuffer);
		static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->SetUseAltTarget(false);
	}
}

bool CRenderTargets::UseAltBackBufferTest()
{
#if !DYNAMIC_GB
	static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->GetUseAltTarget();
//	return (DRAWLISTMGR->IsExecutingDrawSceneDrawList());
#else
	return (DRAWLISTMGR->GetCurrExecDLInfo() && !DRAWLISTMGR->IsExecutingHudDrawList());
#endif
}

#endif

bool CRenderTargets::NeedsRescale()
{
#if __PS3
	return !((VideoResManager::GetUIHeight() == VideoResManager::GetNativeHeight()) && (VideoResManager::GetUIWidth() == VideoResManager::GetNativeWidth()));
#else
	return false;
#endif //__PS3
}

#if __PS3
void CRenderTargets::Rescale()
{
	if(NeedsRescale())//PS3 final downscale from 720p game res to 480p.
	{
		grcRenderTarget* UIBuffer = GetHudBuffer();

		grcTextureFactory::GetInstance().LockRenderTarget(0, grcTextureFactory::GetInstance().GetFrontBuffer(), NULL);
		PF_PUSH_TIMEBAR_DETAIL("SD Final Downsample");
		CSprite2d rescaleSprite;
		rescaleSprite.BeginCustomList(CSprite2d::CS_RESOLVE_SDDOWNSAMPLEFILTER, UIBuffer);
		CRenderTargets::DrawFullScreenTri(0.0f);
		rescaleSprite.EndCustomList();
		PF_POP_TIMEBAR_DETAIL();
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
}
#endif //__PS3

grcRenderTarget* CRenderTargets::GetUIBackBuffer(bool XENON_ONLY(realize))
{
#if __PS3
	if(NeedsRescale())
		return CRenderTargets::GetHudBuffer();
	else
		return grcTextureFactory::GetInstance().GetFrontBuffer();
#elif __XENON
	return grcTextureFactory::GetInstance().GetBackBuffer(realize);
#elif __D3D11 || RSG_ORBIS
	return grcTextureFactory::GetInstance().GetBackBuffer();
#else
	return grcTextureFactory::GetInstance().GetFrontBuffer();
#endif //__XENON
}

const grcRenderTarget* CRenderTargets::GetUIFrontBuffer()
{
	return grcTextureFactory::GetInstance().GetFrontBuffer();
}

grcRenderTarget* CRenderTargets::GetDepthBuffer(bool XENON_ONLY(realize))
{
#if __XENON
	int index = ((grcTextureFactoryXenon&)(grcTextureFactory::GetInstance())).GetCurrentFrontBufferIndex();
	if(realize)
		ms_DepthBuffers[index]->Realize();
	return ms_DepthBuffers[index];
#else
	return ms_DepthBuffer;
#endif //__XENON
}

#if __XENON || __PS3
grcRenderTarget* CRenderTargets::GetDepthBufferAsColor()
{
#if __XENON
	return ms_DepthBufferAliases[((grcTextureFactoryXenon&)(grcTextureFactory::GetInstance())).GetCurrentFrontBufferIndex()];
#else
	return ms_PatchedDepthBuffer;
#endif //__XENON
}
#endif //__XENON || __PS3

grcRenderTarget* CRenderTargets::GetUIDepthBuffer(bool MSAA_ONLY(realize))
{
#if __XENON
	return grcTextureFactory::GetInstance().GetBackBufferDepth(false);
#elif __PS3
	return ms_UIDepthBuffer;
#elif RSG_PC
	if (!VideoResManager::IsSceneScaled())
	{
		if (realize)
			return GetDepthResolved();
		else
			return ms_DepthBuffer;
	}
	else
	{
		if (realize)
			return GRCDEVICE.GetMSAA()>1 ? ms_UIDepthBuffer_Resolved : ms_UIDepthBuffer;
		else
			return ms_UIDepthBuffer;
	}
#else
# if DEVICE_MSAA
	if (realize)
		return GetDepthResolved();
# endif
	return GetDepthBuffer();
#endif
}
grcRenderTarget* CRenderTargets::GetDepthBufferQuarterLinear(){						return m_DepthTargetQuarterLinear; }
grcRenderTarget* CRenderTargets::GetDepthBufferQuarter(){							return m_DepthTargetQuarter; }

grcRenderTarget* CRenderTargets::GetDepthBufferQuarter_Read_Target(bool WIN32PC_ONLY(doCopy))
{
#if RSG_PC
	if (GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		return m_DepthTargetQuarter_ReadOnly;
	}else
	{
		if (doCopy)
		{
			m_DepthTargetQuarter_Copy->Copy( m_DepthTargetQuarter );
		}
		return m_DepthTargetQuarter_Copy;
	}
#else
	return m_DepthTargetQuarter;
#endif //RSG_PC
}

#if RSG_PC && __D3D11
grcRenderTarget* CRenderTargets::GetDepthBuffer_PreAlpha_ReadOnly(){				return m_DepthBuffer_PreAlpha_ReadOnly; }
#endif // RSG_PC && __D3D11

#if DEVICE_MSAA
void CRenderTargets::ResolveBackBuffer(bool depthReadOnly)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	if (!ms_BackBuffer->GetMSAA())
	{
		ms_BackBufferCopy->Copy( ms_BackBuffer );
		return;
	}
	
	UnlockSceneRenderTargets();

	static_cast<grcRenderTargetMSAA*>(ms_BackBuffer)->Resolve( ms_BackBufferCopy );

	if (depthReadOnly)
		CRenderTargets::LockSceneRenderTargets_DepthCopy();
	else
		CRenderTargets::LockSceneRenderTargets();
}
#endif // DEVICE_MSAA


//Hud Targets
grcRenderTarget* m_OffscreenBuffers[4];

#if DEVICE_MSAA
grcRenderTarget* m_OffscreenBuffersResolved[4] = { NULL, NULL, NULL, NULL };

void CRenderTargets::ResolveSourceDepth(MSDepthResolve depthResolveType, grcRenderTarget* sourceMSAADepthTarget, grcRenderTarget* depthResolveTarget)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();

	grcStateBlock::SetStates(grcStateBlock::RS_Default,
		SSAO_OUTPUT_DEPTH ? grcStateBlock::DSS_ForceDepth : grcStateBlock::DSS_IgnoreDepth,
		grcStateBlock::BS_Default);

	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, depthResolveTarget);

	CSprite2d::CustomShaderType type = CSprite2d::CS_COUNT;
	switch( depthResolveType )
	{
	case depthResolve_Sample0:
		type = CSprite2d::CS_RESOLVE_DEPTH_MS_SAMPLE0; break;
	case depthResolve_Closest:
		type = SUPPORT_INVERTED_PROJECTION ? CSprite2d::CS_RESOLVE_DEPTH_MS_MAX : CSprite2d::CS_RESOLVE_DEPTH_MS_MIN; break;
	case depthResolve_Farthest:
		type = SUPPORT_INVERTED_PROJECTION ? CSprite2d::CS_RESOLVE_DEPTH_MS_MIN : CSprite2d::CS_RESOLVE_DEPTH_MS_MAX; break;
	case depthResolve_Dominant:
		type = CSprite2d::CS_RESOLVE_DEPTH_DOMINANT; break;
	default:
		Errorf("invalid depth resolve type\n"); break;
	}

	CSprite2d resolveDepth;
	resolveDepth.BeginCustomListMS(type, sourceMSAADepthTarget);

	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255, 255, 255, 255));

	resolveDepth.EndCustomListMS();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0); //unlock and resolve

}

void CRenderTargets::ResolveDepth(MSDepthResolve depthResolveType, grcRenderTarget* alternateResolveTarget)
{
	grcRenderTarget* depthResolveTarget = alternateResolveTarget ? alternateResolveTarget : CRenderTargets::GetDepthResolved();
	ResolveSourceDepth(depthResolveType, CRenderTargets::GetDepthBuffer(), depthResolveTarget);
}

#endif // DEVICE_MSAA

void CRenderTargets::ResolveBuffer(unsigned index)
{
	Assert(index < 4);
	(void)index;

#	if DEVICE_MSAA
		grcRenderTarget *const pSource = m_OffscreenBuffers[index];
		grcRenderTarget *const pDest   = m_OffscreenBuffersResolved[index];
		if (pSource->GetMSAA())
		{
			Assert( pDest != NULL && !pDest->GetMSAA() );
			if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
			{
				grcRenderTargetMSAA *const aaSource = static_cast<grcRenderTargetMSAA*>(pSource);
#				if RSG_ORBIS
					// decompress Fmask / eliminate fast clear
					GRCDEVICE.FinishRendering( aaSource->GetColorTarget(), true );
#				endif
#				if NUMBER_OF_RENDER_THREADS == 1
					// Need to have single threaded rendering to be able to
					// correctly check this.  Hopefully people will develop in
					// single threaded mode often enough to pick up errors, as
					// unnecissary resolves will burn performance.
					Assert(static_cast<grcRenderTargetMSAA*>(pSource)->DebugGetLastResolved()
							!= static_cast<grcRenderTargetMSAA*>(pDest)->GetResolveTarget());
#				endif
				aaSource->Resolve( static_cast<grcRenderTargetMSAA*>(pDest) );
			}
		}
#	endif
}

grcRenderTarget* CRenderTargets::GetOffscreenBuffer(unsigned index)
{
	Assert(index < 4);
	return m_OffscreenBuffers[index];
}

grcRenderTarget* CRenderTargets::GetOffscreenBuffer_SingleSample(unsigned index)
{
	Assert(index < 4);
#if DEVICE_MSAA
	if (m_OffscreenBuffers[index]->GetMSAA())
		return m_OffscreenBuffersResolved[index];
	else
#endif
		return m_OffscreenBuffers[index];
}

grcRenderTarget* CRenderTargets::GetOffscreenBuffer_Resolved(unsigned index)
{
#if DEVICE_MSAA
#	if NUMBER_OF_RENDER_THREADS == 1
		// Need to have single threaded rendering to be able to correctly check
		// this.  Hopefully people will develop in single threaded mode often
		// enough to pick up errors (though chances are errors will be very
		// visible anyways).
		if (m_OffscreenBuffersResolved[index] != NULL)
		{
			Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)
				|| static_cast<grcRenderTargetMSAA*>(m_OffscreenBuffers[index])->DebugGetLastResolved()
					== static_cast<grcRenderTargetMSAA*>(m_OffscreenBuffersResolved[index])->GetResolveTarget());
		}
#	endif
#endif
	return GetOffscreenBuffer_SingleSample(index);
}

#if __PS3
// If USE_DEPTHBUFFER_ZCULL_SHARING then this shares zcull memory with the backbuffer
grcRenderTarget* m_OffscreenDepthBuffer = NULL;
grcRenderTarget* m_OffscreenDepthBufferAlias = NULL;
grcRenderTarget* CRenderTargets::GetOffscreenDepthBuffer() { return m_OffscreenDepthBuffer; }
grcRenderTarget* CRenderTargets::GetOffscreenDepthBufferAsColor() { return m_OffscreenDepthBufferAlias; }
grcRenderTarget* CRenderTargets::SetOffscreenDepthBufferAsColor( grcRenderTarget* tgt ) { return m_OffscreenDepthBufferAlias =  tgt; }

#endif

grcRenderTarget* CRenderTargets::GetVolumeOffscreenBuffer()							{ return LOW_RES_VOLUME_LIGHT_SWITCH( m_volumeOffscreenBuffer, ms_BackBufferCopy); } // Use quarter res target or use full res non-msaa target

#if LIGHT_VOLUME_USE_LOW_RESOLUTION
grcRenderTarget* CRenderTargets::GetVolumeReconstructionBuffer()					{ return m_volumeReconstructionBuffer; }
#endif //LIGHT_VOLUME_USE_LOW_RESOLUTION

#if __XENON
grcRenderTarget* CRenderTargets::GetMinimapOffscreenBuffer()						{ return m_miniMapOffscreenBuffer; }
grcRenderTarget* CRenderTargets::GetMinimapOffscreenDepthBuffer()					{ return m_miniMapOffscreenDepthBuffer; }
grcRenderTarget* CRenderTargets::GetBackBufferCopy()								{ return m_BackBufferCopy; }
#endif

#if RSG_ORBIS

void DisplayRenderTargetDetailsHeader()
{
	Displayf("name\twidth\theight\tbpp\tmipCount\tpitch\talign\tphysSize");
}

#if !__NO_OUTPUT
uint64_t DisplayRenderTargetDetails(grcRenderTarget *target)
{
	const sce::Gnm::Texture* tex = target->GetTexturePtr();
	const sce::Gnm::DataFormat dataFormat = tex->getDataFormat();

	u16 width = tex->getWidth();
	u16 height = tex->getHeight();
	u32 mipCount = tex->getLastMipLevel();
	u32 bitsPerPixel = dataFormat.getBitsPerElement();
	//const bool msaaEnabled = tex->getNumFragments() != sce::Gnm::kNumFragments1;
	//const isDepth = dataFormat->getZFormat() != sce::Gnm::kZFormatInvalid;
	const char* name = target->GetName();
	u32 pitch = tex->getPitch();

	uint64_t outSize;
	sce::Gnm::AlignmentType outAlign;
	sce::GpuAddress::computeTotalTiledTextureSize(&outSize, &outAlign, tex);

	//        N   W   H   BBP MC  P   A   S 
	Displayf("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%lu",
		name, width, height, bitsPerPixel, mipCount,
		pitch, outAlign, outSize);

	return outSize;
}
#endif // !__NO_OUTPUT
#endif // RSG_ORBIS


#if __XENON
void CRenderTargets::PreAllocateRenderTargetMemPool()
{
	for (int i = 0; i< RTMEMPOOL_SIZE; i++)
		ms_RenderTargetMemPoolArray[i]=kRTPoolIDInvalid;


	CRenderTargets::AllocRenderTargetMemPool(1760256, XENON_RTMEMPOOL_GENERAL, kGeneralHeapCount); 
	CRenderTargets::AllocRenderTargetMemPool(3276800, XENON_RTMEMPOOL_GENERAL1, kGeneral1HeapCount);

	int poolSize;

	poolSize = CRenderTargets::GetRenderTargetMemPoolSize(1280, 720);

	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_GBUFFER0, kGBuffer0HeapCount);
	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_GBUFFER1, kGBuffer1HeapCount);
	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_TEMP_BUFFER, kTempBufferPoolHeapCount);

	CRenderTargets::AllocRenderTargetMemPool(
		CRenderTargets::GetRenderTargetMemPoolSize(512, 512), 
		XENON_RTMEMPOOL_PERLIN_NOISE, 
		kPerlinNoisePoolHeapCount);

	grcTextureLock lock;
	((grcTextureFactoryXenon&)(grcTextureFactory::GetInstance())).GetFrontBufferFromIndex(0)->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_FRONTBUFFER0, 2, lock.Base);
	((grcTextureFactoryXenon&)(grcTextureFactory::GetInstance())).GetFrontBufferFromIndex(1)->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_FRONTBUFFER1, 2, lock.Base);
	((grcTextureFactoryXenon&)(grcTextureFactory::GetInstance())).GetFrontBufferFromIndex(2)->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_FRONTBUFFER2, 2, lock.Base);

	poolSize = CRenderTargets::GetRenderTargetMemPoolSize(1280*2, 720);
	grcTextureFactory::GetInstance().GetBackBuffer(false)->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
	CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_GBUFFER23, kGBuffer23HeapCount, lock.Base);
}

int CRenderTargets::GetRenderTargetMemPoolSize(int width, int height)
{
	int tw=(width>>5);
	int th=(height>>5);
	if ((tw<<5)<width) tw++;
	if ((th<<5)<height) th++;

	return ((tw<<5)*(th<<5)*4);
}

void CRenderTargets::AllocRenderTargetMemPool(int size, RTMemPoolID memPoodID, u8 heapCount, void* preallocatedBuffer)
{
	char name[128];
	sprintf(name, "pool %d", memPoodID);
	Assert(memPoodID<RTMEMPOOL_SIZE);
	Assert(ms_RenderTargetMemPoolArray[memPoodID]==kRTPoolIDInvalid);

	//rounded up to 32x32 tiles
	grcRTPoolCreateParams params;
	params.m_Size = size;
	params.m_HeapCount = heapCount;

	if (memPoodID==XENON_RTMEMPOOL_SHADOWS || memPoodID==XENON_RTMEMPOOL_SHADOW_CACHE )
		params.m_MaxTargets = 256;		// more than the default for this pool

	ms_RenderTargetMemPoolArray[memPoodID] = grcRenderTargetPoolMgr::CreatePool(name, params, preallocatedBuffer);

	Assert(ms_RenderTargetMemPoolArray[memPoodID]!=kRTPoolIDInvalid);
}

#if !__NO_OUTPUT
void DisplayRenderTargetDetails(grcRenderTarget *target)
{
	const char *name = target->GetName();
	int width = target->GetWidth();
	int height = target->GetHeight();
	int poolId = target->GetPoolID();
	int heapId = target->GetPoolHeap();

	grcDeviceSurface *surface = ((rage::grcRenderTargetXenon*)target)->GetSurface();
	grcTextureObject *texture = ((rage::grcRenderTargetXenon*)target)->GetTexturePtr();
	
	bool rendereable = surface != NULL;
	bool resolveable = texture != NULL;

	XGTEXTURE_DESC texDesc;
	XGTEXTURE_DESC surfaceDesc;

	int bpp = 0;
	int physBlockSize = 0;
	int edramBlockSize = 0;
	int physSize = 0;
	int msaa = 0;
	if( rendereable )
	{
		XGGetSurfaceDesc(surface,&surfaceDesc);
		bpp = surfaceDesc.BitsPerPixel;
		edramBlockSize = surfaceDesc.BitsPerPixel * surfaceDesc.HeightInBlocks * surfaceDesc.BytesPerBlock;
		msaa = surfaceDesc.MultiSampleType > 0;
	}

	// For shared property, resolveable takes over.
	if( resolveable )
	{
		XGGetTextureDesc(texture,0,&texDesc);
		bpp = texDesc.BitsPerPixel;
		physBlockSize = texDesc.BitsPerPixel * texDesc.HeightInBlocks * texDesc.BytesPerBlock;

		physSize = XGSetTextureHeaderEx(texDesc.Width, texDesc.Height, 1, 0, texDesc.Format, 0, XGHEADEREX_NONPACKED,0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, NULL, NULL, NULL);
	}

	Displayf("%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d",poolId,heapId,name,width,height,bpp,msaa,physBlockSize,edramBlockSize,physSize);
}
#endif	// !__NO_OUTPUT

void DisplayRenderTargetDetailsHeader()
{
	Displayf("poolId\theapId\t\tname\twidth\theight\tbpp\tmsaa\tphysBlockSize\tedramBlockSize\tphysSize");
}
#endif // __XENON

#if __PPU

static struct RenderTargetMemPoolDesc
{
	const char* name;
	u32 size;
	u32 pitch;
	u8 heaps;
	bool tiled;
	u8 location;
	u8 compression;

} s_RenderTargetMemPoolDesc[RTMEMPOOL_SIZE] =
{
	// Color Buffer
	/* 00 */	{ "PSN_RTMEMPOOL_P10240_TILED_LOCAL_COMPMODE_DISABLED",	// 0: back buffer/Full Screen SSAO(Uses target directly), 1-3: ped blood and decoration, 4: Tiled Lighting 1/4 5: NONE 6: NONE 7: ped blood and decoration
	115 * KB64,	10240,	8, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_DISABLED					},
	/* 01 */	{ "PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_DISABLED",	// 0: POINT_SHADOW, 1: SPOT_SHADOW, 3: Ped Damage medRes, 4: ped Damage lowRes NOTE: ped damage skips over cached shadow textures using CParaboloidShadow::GetCacheTextureSize()
	172 * KB64,	 1024,	5, false, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_DISABLED			},
	/* 02 */	{ "PSN_RTMEMPOOL_P2048_TILED_LOCAL_COMPMODE_C32_2X1",	// 0: REFLECTION_MAP_COLOUR, MIRROR_RT RT 1: FX_SEETHROUGH_COLOR, SSLR Intensity 2: Cube-map target
	23 * KB64,	2048,	3, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_C32_2X1					},
	/* 03 */	{ "PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED",	// 0: Imposters + HEIGHT_MAP_VS, 1: Water Wet Map Temp / Cube map reflections 2: Water Foam RT 3: Ped Headshot GBuffers LoRes 4: Ped Headshot GBuffers HiRes  5: Headshot Composite Buffer
#if USE_TREE_IMPOSTERS
	10 * KB64,	  512,	6, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_DISABLED				},
#else // USE_TREE_IMPOSTERS
	7 * KB64,	  512,	6, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_DISABLED				},
#endif // USE_TREE_IMPOSTERS
	/* 04 */	{ "PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2",		// 0: GBuffer, 1: FX_SEETHROUGH_BLUR, MSAA reflection map, 2: NONE, 3: Volume Lights Offscreen buffer (low resolution), Volume Lights Reconstruction buffer, 4: NONE  5: Volume Lights Offscreen buffer(Full Resolution), Offscreen Depth, Particle Offscreen Downsampled Buffer 6: Offscreen buffers for HUD UI 7: Particle Offscreen Downsampled Buffer 8: PostFx Quarter Screen, SeeThrough Blur, Screenshot Buffer 9: Headshot Light Buffer 10: Meme Editor...
	225 * KB64,	5120,	11, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_DISABLED				},
	/* 05 */	{ "PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED",
	// 0: Water Bump, Refraction Mask 1: Various PostFX Targets 2: NONE 3: FX_SEETHROUGH_DEPTH 4: MIRROR_DEPTH && mirror ped damage 5: Water Targets, Quarter Buffer Padding, Depth Quarter Linear 6: Procedural Ripple targets 7: Ped Damage 8: some of the pedDamage med & low res targets
	// 9: SSAO Temp 0, SSAO Temp 1,  10: Water Lighting 1/4 + 1/16 temp RTs 11: Water Rain Bump 12: Temp classification textures 13: Shadowed PTX Buffer 14: pedheadshot colour target 15: pedheadshot colour target small 16: Water Reflection MSAA color and depth 17: ZCull Dummy 18: Water Bump 1/4
	75* KB64,	 2560,	19, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_DISABLED	},
	/* 06 */	{ "PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_C32_2X1",// 0: Photo_RT 1: Water Bump Temp RT, 2: Water Lighting 1/64, 3: PostFX Light Adaptation 4: Water Bump Temp 1/4 RT
	4 * KB64,	 1024,	5, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_C32_2X1					},
	/* 07 */	{ "PSN_RTMEMPOOL_P1280_TILED_LOCAL_COMPMODE_C32_2X1",	// 0 : sslr cutout 1 : WATER_REFLECTION_COLOUR  2: NONE 3: NONE 4 : NONE 5 : NONE 6: REFLECTION_MAP_COLOUR_MSAA(DISABLED)
	4 * KB64,	 1280,	2, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_C32_2X1				},
// TODO +8 should be able to be removed, if we can remove something from heap 5

	// Depth buffer
	/* 08 */	{ "PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR",	// 0: SHADOW_MAP_DIR, Depth Quarter 1: Light Rays Down Sampled Shadow
                                                                                        // If cascaded shadows stop being 640 wide, we need to move "Depth Quarter" back to P2560_TILED_LOCAL_COMPMODE_DISABLED and lose scull/zcull optimization in the up-sampler.
	#if CASCADE_SHADOWS_RES_X <= 512 // support 4x 512x512 cascades arranged vertically
		64 * KB64,	4*512,	2, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},
	#elif CASCADE_SHADOWS_RES_X <= 640 // support 4x 640x640 cascades arranged vertically
		115 * KB64,	4*640,	2, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},//NOTE: Compression is disabled on __DEV builds! See: CASCADE_SHADOWS_ALTERNATE_SHADOW_MAPS
	#elif CASCADE_SHADOWS_RES_X <= 800 // support 4x 800x800 cascades arranged vertically
		163 * KB64,	3328,	2, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},
	#elif CASCADE_SHADOWS_RES_X <= 1024 // support 4x 1024x1024 cascades arranged vertically
		256 * KB64,	4*1024,	2, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},
	#else // whatever, crash
		1 * KB64,	512,	2, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},
	#endif
	/* 09 */	{ "PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR",		// HEIGHT_MAP_DEPTH,
	1 * KB64,	512,	1, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},
	/* 10 */	{ "PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_DIAGONAL",	// 0: None, 1: Reflection Depth, 2: MSAA Cube Reflection Depth
	2 * KB64, 1024,	3, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR },
	// *THIS MUST BE LAST ENTRY* - see hack in PreAllocateRenderTargetMemPool():
	/* 11 */	{ "PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR_BBUFFER",		//0: UI Depth Buffer  1: NonMSAADepth, 2: REFLECTION_MAP_COLOUR_CUBE_DEPTH 3: tree imposters, 4,5,6,7: ped damage 8: Scene Depth Buffer, 9, 10: ped damage
	60 * KB64,	 5120,	11, true, CELL_GCM_LOCATION_LOCAL, CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR	},	// no compression so we can copy as colour data
};

const char* s_CompressionMode[] =
{
	"CELL_GCM_COMPMODE_C32_2X1",
	"CELL_GCM_COMPMODE_C32_2X2",
	"CELL_GCM_COMPMODE_Z32_SEPSTENCIL",
	"CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REG",
	"CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR",
	"CELL_GCM_COMPMODE_Z32_SEPSTENCIL_DIAGONAL",
	"CELL_GCM_COMPMODE_Z32_SEPSTENCIL_ROTATED",
};

#if !__NO_OUTPUT

void DisplayRenderTargetDetails(grcRenderTarget *target)
{
	grcRenderTargetGCM *gcmRenderTarget = (grcRenderTargetGCM *)target;

	int poolId = target->GetPoolID();
	int heapId = target->GetPoolHeap();
	u16 _width = gcmRenderTarget->GetWidth();
	u16 height = gcmRenderTarget->GetHeight();
	u32 bitsPerPixel = gcmRenderTarget->GetBitsPerPix();
	u32 mipCount = gcmRenderTarget->GetMipMapCount();
	bool msaa = gcmRenderTarget->GetMSAA() != grcDevice::MSAA_None;
	
	u8 format = gcm::StripTextureFormat(gcmRenderTarget->GetGcmTexture().format);
	bool isDepth =	(format == CELL_GCM_TEXTURE_DEPTH24_D8) || (format == CELL_GCM_TEXTURE_DEPTH24_D8_FLOAT) || 
					(format == CELL_GCM_TEXTURE_DEPTH16) ||	(format == CELL_GCM_TEXTURE_DEPTH16_FLOAT);

	const char *name = gcmRenderTarget->GetName();
	
	bool inTiledMemory = gcmRenderTarget->IsTiled();
	bool inLocalMemory = gcmRenderTarget->IsInLocalMemory();
	bool isSwizzled = gcmRenderTarget->IsSwizzled();
	bool isZCull = gcmRenderTarget->IsUsingZCull();
	bool isCubeMap = gcmRenderTarget->GetType() == /*grcRenderTargetType::*/grcrtCubeMap;
	
	// name,width,height,bitsPerPixel,mipCount
	u16 width = (msaa == true) ? _width * 2: _width;
	
	// HWwidth ,HW_Height,HW_mipmapCount
	u32 HW_width = gcm::GetSurfaceWidth(width);
	u32 HW_Height = gcm::GetSurfaceHeight(height,inLocalMemory);
	u32 HW_mipmapCount = gcm::GetSurfaceMipMapCount(HW_width,HW_Height,isSwizzled);

	// Pitch,Alignment,Size
	u32 Pitch = gcm::GetSurfacePitch(HW_width, bitsPerPixel, isSwizzled);
	u32 Alignment = gcm::GetSurfaceAlignment(inTiledMemory,isZCull);
	u32 Size = gcm::GetSurfaceSize(HW_width,HW_Height,bitsPerPixel,mipCount,1, inLocalMemory, inTiledMemory, isSwizzled, isCubeMap, isZCull,NULL);

	// zculledAlignment
	u32 zculledAlignment = 0;
	if( isDepth )
	{
		zculledAlignment = gcm::GetSurfaceAlignment(inTiledMemory,isZCull);
	}


	Displayf("%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",	poolId,heapId,name,width,height,bitsPerPixel,mipCount,
																			HW_width ,HW_Height,HW_mipmapCount,
																			Pitch,Alignment,Size, 
																			isZCull,zculledAlignment);
}

void DisplayRenderTargetDetailsHeader()
{
	Displayf("poolId\theapId\tname\twidth\theight\tbitsPerPixel\tmipCount\tHW_width\tHW_Height\tHW_mipmapCount\tPitch\tAlignment\tSize\tuseZcull\tzculledAlignment");
}
#endif // !__NO_OUTPUT

XPARAM(EnableShadowCache); // TEMP for 32 bit hires CMs

void CRenderTargets::PreAllocateRenderTargetMemPool()
{
	// Initialized to end of RSX local.  Afaict, there is no nice define
	// anywhere for this value, just needs to be hardcoded.
	//
	// The subtraction of 64KiB is annoyingly wasteful.  We need to leave
	// 16-bytes due to an RSX overfetch bug with S3TC or linear format textures
	// (https://ps3.scedev.net/technotes/view/1127).  But since the tiled
	// regions need to be 64KiB aligned, we end up wasting a lot more than
	// 16-bytes.
	//
	char *const vramEnd = (char*)0xcf900000;
	char *vramPtr = vramEnd - 0x10000;

	int i;
	for (i=0; i<RTMEMPOOL_SIZE - 1; ++i)
	{
		RenderTargetMemPoolDesc& desc = s_RenderTargetMemPoolDesc[i];

		const bool isPhysical = desc.location == CELL_GCM_LOCATION_LOCAL;

		grcRTPoolCreateParams poolParams;
		poolParams.m_Size = desc.size;
		poolParams.m_HeapCount = desc.heaps;
		poolParams.m_Alignment = gcm::GetSurfaceAlignment(desc.tiled, true),
		poolParams.m_PhysicalMem = isPhysical;
		poolParams.m_Pitch = desc.pitch;
		poolParams.m_Compression = desc.compression;
 		poolParams.m_Tiled = desc.tiled;

		void *buffer = NULL;
		if (isPhysical)
		{
			vramPtr -= desc.size;
			buffer = vramPtr;
		}

		poolParams.m_MaxTargets = (i==PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_DISABLED)?256:64; // there is only one pool has a more than the default max number of targets in them...

		// this sucks, but for now it is compatible with the old way where the first target specified the type, zcull, etc. when we add those to the desc, we can remove this.
		poolParams.m_InitOnCreate = (desc.compression<CELL_GCM_COMPMODE_Z32_SEPSTENCIL);

		ms_RenderTargetMemPoolArray[i] = grcRenderTargetPoolMgr::CreatePool(desc.name, poolParams, buffer);
		Assert(ms_RenderTargetMemPoolArray[i]!=kRTPoolIDInvalid);

		Displayf("PSN RT mempool%d size=%d (%dKB, %dMB)", i, desc.size, desc.size/1024, desc.size/(1024*1024));
	}

#if !__NO_OUTPUT
	const uptr vramSizeKB = (vramEnd - vramPtr + 0x3ff) >> 10;
	if (vramSizeKB != PSN_VRAM_RENDER_TARGET_TOTAL_KB)
	{
		Quitf("PSN_VRAM_RENDER_TARGET_TOTAL_KB (%u) is not the correct tally of the vram render targets.  Should be %u.", PSN_VRAM_RENDER_TARGET_TOTAL_KB, vramSizeKB);
	}
#endif
	
	// that one's a special case: it reuse the depth buffer already allocated by rage.	
	RenderTargetMemPoolDesc& desc = s_RenderTargetMemPoolDesc[i];

	Assertf(grcTextureFactory::GetInstance().GetBackBufferDepth(false) == NULL,"Depth buffer should be null at that point.");
	
	u32 depthWidth	= VideoResManager::GetUIWidth();
	u32 depthHeight	= VideoResManager::GetUIHeight();
	u32 depthSize = gcm::GetSurfaceSize(	depthWidth, /*u16 width, */
											depthHeight, /*u16 height, */
											32, /*u32 bitsPerPixel, */
											1, /*u32 mipCount, */
											1, /*u32 faceCount, */
											true, /*bool inLocalMemory, */
											true, /*bool inTiledMemory, */
											false, /*bool isSwizzled, */
											false, /*bool isCubeMap, */
											true, /*bool isZCull, */
											NULL /*u32* memoryOffsets*/);
	u32 depthAlignment	= gcm::GetSurfaceAlignment(true,true);
	u32 depthPitch		= gcm::GetSurfaceTiledPitch(depthWidth,32,false);

	// no compression so we can copy as colour data
	u8 compMode = CELL_GCM_COMPMODE_Z32_SEPSTENCIL_REGULAR;
	
	grcRTPoolCreateParams poolParams;
	poolParams.m_Size = depthSize;
	poolParams.m_HeapCount = desc.heaps;
	poolParams.m_Alignment = depthAlignment,
	poolParams.m_PhysicalMem = true;
	poolParams.m_Pitch = depthPitch;
	poolParams.m_Compression = compMode;
	poolParams.m_Type = grcrtDepthBuffer;

	ms_RenderTargetMemPoolArray[i] = grcRenderTargetPoolMgr::CreatePool(desc.name, poolParams);
	Assert(ms_RenderTargetMemPoolArray[i]!=kRTPoolIDInvalid);

	// Update description, for debug purpose
	desc.size = depthSize;
	desc.tiled = true;
	desc.location = CELL_GCM_LOCATION_LOCAL;
	desc.pitch = depthPitch;
	desc.compression = compMode;

	// Allocate and set a depth buffer. 
	// It's theoretically not needed but a) it's memory's shared with the other one and b) it helps laziness by not worrying about
	// the fact that some of the memorytarget pool specs are initialized based on the first target being allocated!	
	
	grcTextureFactory::CreateParams params;
	params.Multisample = grcDevice::MSAA_None;
	params.InTiledMemory = true;
	params.UseHierZ = true;
	params.IsSRGB = false;
	params.EnableCompression = ( compMode != CELL_GCM_COMPMODE_DISABLED );

	// create depth buffer texture
	grcRenderTargetGCM* depthBuffer = static_cast<grcRenderTargetGCM*>(CRenderTargets::CreateRenderTarget(
																					PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR_BBUFFER,
																					"UI DepthBuffer", 
																					grcrtDepthBuffer, 
																					depthWidth, 
																					depthHeight, 
																					32, 
																					&params,
																					0)
																				   );
	Assert(depthBuffer);
	depthBuffer->AllocateMemoryFromPool();

	ms_UIDepthBuffer = depthBuffer;

	// Rage, you lack depth.
	static_cast<grcTextureFactoryGCM&>(grcTextureFactory::GetInstance()).SetBufferDepth(NULL);
}
//
// Purpose: Refresh the stencil cull on PS3 only.
//
void CRenderTargets::RefreshStencilCull(bool useEqual, int material, int mask, bool preserveZFarCull)
{
	PF_PUSH_MARKER("Refresh Stencil Cull");

	// see libgcm-Overview_e.pdf section 11
	// NOTE: stencil mask must be set to 0xff or 0x00 for cellGcmSetClearZcullSurface() to work. to avoid setting a whole state block just for stencil mask we do it directly here.
	GCM_DEBUG(GCM::cellGcmSetStencilMask(GCM_CONTEXT, 0xff));
	GCM_DEBUG(GCM::cellGcmSetScullControl(GCM_CONTEXT, useEqual ? CELL_GCM_SCULL_SFUNC_EQUAL : CELL_GCM_SCULL_SFUNC_NOTEQUAL, material, mask));
	GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, false, true)); // clear stencil cull only

	// reload it (without modifying the values)
	grcStateBlock::SetBlendState(ms_Refresh_BS);
	if(preserveZFarCull)
		grcStateBlock::SetDepthStencilState(ms_RefreshStencilPreserveZFar_DS,0xFF);
	else
		grcStateBlock::SetDepthStencilState(ms_RefreshStencil_DS,0xFF);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	GRCDEVICE.SetZMinMaxControl(true, true, false);
	GRCDEVICE.SetDepthBoundsTestEnable(false);
	
	CSprite2d zCullReload;
	zCullReload.BeginCustomList(CSprite2d::CS_SCULL_RELOAD, NULL);

	DrawFullScreenTri(0.0f);

	zCullReload.EndCustomList();

	GRCDEVICE.SetZMinMaxControl(true, false, false); // restore

	PF_POP_MARKER();
}

void CRenderTargets::ResetStencilCull(bool useEqual, int material, int mask, bool clearZ, bool clearStencil)
{
	PF_PUSH_MARKER("Reset Stencil Cull");

	GCM_DEBUG(GCM::cellGcmSetStencilMask(GCM_CONTEXT, 0xff));
	GCM_DEBUG(GCM::cellGcmSetScullControl(GCM_CONTEXT, useEqual ? CELL_GCM_SCULL_SFUNC_EQUAL : CELL_GCM_SCULL_SFUNC_NOTEQUAL, material, mask));
	GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, clearZ, clearStencil));
	
	PF_POP_MARKER();
}

void CRenderTargets::RefreshStencilZFarCull(bool useEqual, int material, int mask)
{
	PF_PUSH_MARKER("Refresh Stencil ZFar Cull");

	// see libgcm-Overview_e.pdf section 11
	// NOTE: stencil mask must be set to 0xff or 0x00 for cellGcmSetClearZcullSurface() to work. to avoid setting a whole state block just for stencil mask we do it directly here.
	GCM_DEBUG(GCM::cellGcmSetStencilMask(GCM_CONTEXT, 0xff));
	GCM_DEBUG(GCM::cellGcmSetScullControl(GCM_CONTEXT, useEqual ? CELL_GCM_SCULL_SFUNC_EQUAL : CELL_GCM_SCULL_SFUNC_NOTEQUAL, material, mask));
	GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, true, true));
 
	// reload it (without modifying the values)
	GRCDEVICE.SetZMinMaxControl(true, true, false);
	GRCDEVICE.SetDepthBoundsTestEnable(false);

	grcStateBlock::SetRasterizerState( grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetBlendState(ms_Refresh_BS);
	grcStateBlock::SetDepthStencilState(ms_RefreshStencilZFar_DS,0x00);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	
	CSprite2d zCullReload;
	zCullReload.BeginCustomList(CSprite2d::CS_ZCULL_RELOAD, NULL);

	grcDrawSingleQuadf(0,0,1,1,0,0,0,1,1,Color32(255,255,255,255));
	
	zCullReload.EndCustomList();

	GRCDEVICE.SetZMinMaxControl(true, false, false); // restore

	PF_POP_MARKER();
}

void CRenderTargets::SetZCullControl(bool useDepthGreater)   
{
	GRCDEVICE.InsertFence();  // without the WaitForIdle this inserts, changing the zcull control direction causes some places on screen occasionally that fail the zcull test (and drop out lights, etc) 

	GCM_DEBUG(GCM::cellGcmSetZcullControl(GCM_CONTEXT, (useDepthGreater) ? CELL_GCM_ZCULL_GREATER : CELL_GCM_ZCULL_LESS, (useDepthGreater) ? CELL_GCM_ZCULL_MSB:CELL_GCM_ZCULL_LONES));
	GCM_STATE(SetClearDepthStencil, (static_cast<u32>(((useDepthGreater)?0.0f:1.0f) * 0xffffff) << 8));
}

void CRenderTargets::RefreshZFarCull(bool useDepthGreater)
{
	PF_PUSH_MARKER("Refresh ZFar Cull");

	// Section 7.3.2.2 RSX-Users_Manual_e.pdf
	SetZCullControl( useDepthGreater );	

	grcStateBlock::SetRasterizerState( grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetBlendState(ms_Refresh_BS);
	grcStateBlock::SetDepthStencilState(useDepthGreater ? ms_RefreshZFar_G_DS : ms_RefreshZFar_L_DS);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	
	GRCDEVICE.SetZMinMaxControl(true, true, false);
	GRCDEVICE.SetDepthBoundsTestEnable(false);

	GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, true, false));

	CSprite2d zCullReload;

	zCullReload.BeginCustomList((useDepthGreater) ? CSprite2d::CS_ZCULL_RELOAD_GT : CSprite2d::CS_ZCULL_RELOAD, NULL);  
	
	static float depth = 0.0f;

	DrawFullScreenTri(depth);

	zCullReload.EndCustomList();

	GRCDEVICE.SetZMinMaxControl(true, false, false); // restore

	PF_POP_MARKER();
}


static void ResolveMSAA(const grcWindow *win)
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_CompositeAlpha);	

	Color32 color(1.0f,1.0f,1.0f,1.0f);

	CSprite2d msaaResolve;
	
	msaaResolve.BeginCustomList(CSprite2d::CS_RESOLVE_MSAA2X, GBuffer::GetTarget(GBUFFER_RT_2));
	
	if( win )
	{
		const float x1 = win->GetNormX() * 2.0f - 1.0f;
		const float y1 = -(win->GetNormY() * 2.0f - 1.0f);
		const float x2 = (win->GetNormX() + win->GetNormWidth()) * 2.0f - 1.0f;
		const float y2 = -((win->GetNormY() + win->GetNormHeight()) * 2.0f - 1.0f);

		grcDrawSingleQuadf(	x1,y1,
						x2,y2,
						0.1f,
						win->GetNormX(),win->GetNormY(),
						win->GetNormX() + win->GetNormWidth(),win->GetNormY() + win->GetNormHeight(),
						color);
	}
	else
	{
		const float x1 = -1.0f;
		const float y1 =  1.0f;
		const float x2 =  1.0f;
		const float y2 = -1.0f;

		grcDrawSingleQuadf(	x1,y1,
						x2,y2,
						0.1f,
						0.0f,0.0f,
						1.0f,1.0f,
						color);
	}
	
	msaaResolve.EndCustomList();
}



void CRenderTargets::BeginMSAARender()
{
	if( false == ms_MSAARender )
	{
		grcViewport* viewPort = grcViewport::GetCurrent();
		
		const grcWindow& win = viewPort->GetWindow();
		
		// Lock MSAA buffer
		// We just use the GBuffer 3 as an msaa target
		grcRenderTarget *fbRT = GBuffer::GetTarget(GBUFFER_RT_2);
		grcRenderTarget *depthRT = GBuffer::GetDepthTarget();
		grcTextureFactory::GetInstance().LockRenderTarget(0, fbRT, depthRT);

		// Adding an evil 1 pixel border: we might be doing scissoring, and we are doing some quincunx filtering so we need that to be clean...
		GRCDEVICE.Clear(true, Color32(0x00),true,1.0f,false,0);
		ms_MSAARender = true;
	}
}


void CRenderTargets::EndMSAARender()
{
	if( ms_MSAARender )
	{
		// Unlock RT
		grcViewport* viewPort = grcViewport::GetCurrent();
		grcWindow win = viewPort->GetWindow();
		
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);	
		ResolveMSAA(&win);
		ms_MSAARender = false;
	}
}


void CRenderTargets::ClearStencilCull( int /*material*/)
{}
#endif // __PPU


#if __XENON
void CRenderTargets::RefreshStencilCull(bool useEqual, int material, int /*mask*/)
{
	{
		GBuffer::BeginUseHiStencil( material, useEqual);
	}
}
void CRenderTargets::ClearStencilCull(int material)
{
	GBuffer::EndUseHiStencil(material);
}


//
// Alias the 10bit float HDR back buffer with a 10bit integer format so that we don't have to expand to 16bit float on resolve.
// This cuts the resolve time in half and speeds up sampling but it requires you to decode color in the shader and prevents you
// from using texture filtering.
grcRenderTarget* CRenderTargets::GetBackBuffer7e3toInt( bool bRealize /*= false*/ )
{
	Assertf(static_cast<grcRenderTargetXenon*>(grcTextureFactory::GetInstance().GetBackBuffer(false))->GetSurface()->Format == D3DFMT_A2B10G10R10F_EDRAM, "Can't resolve non-7e3 backbuffer format to 10bit int");

	if ( bRealize )
	{
		// Change the render target to the same format as the destination texture
		bool targetsActive = ms_RenderTargetActive;
		if(targetsActive)
			UnlockSceneRenderTargets();
#if RSG_ORBIS || __D3D11
		grcTextureFactory::GetInstance().LockRenderTarget(0, ms_BackBuffer7e3ToIntAlias, NULL, 0, false, 0);
#else
		grcTextureFactory::GetInstance().LockRenderTarget(0, ms_BackBuffer7e3ToIntAlias, NULL, 0, false);
#endif

		// Restore the render target format to the correct format for future use
		grcResolveFlags resolveFlags;
		resolveFlags.NeedResolve = true;
		resolveFlags.ClearDepthStencil = false;
		resolveFlags.ClearColor = false;
		resolveFlags.Color = Color32(0,0,0,0);

		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
		if(targetsActive)
			LockSceneRenderTargets();
	}

	static_cast<grcRenderTargetXenon*>(ms_BackBuffer7e3ToIntAlias)->SetExpAdjust(3); // Use this to avoid division by 8
	return ms_BackBuffer7e3ToIntAlias;
}

#endif // __XENON

#if RSG_PS3 || RSG_XENON
//
// Purpose: Convert an alpha mask to a stencil mask, on the consoles this will also setup early-stencil culling (HiStencil/SCull) so
//          no need to do a HiZ/SCull refresh pass. This pass is slightly more expensive than a regular (non-alpha) HiZ/SCull refresh:
//          1280x720 & RGBA8 Source Texture: PS3 = 0.22ms, 360 = 0.26ms.
//
//       ** Because of differences in the early-stencil hardware, the PS3 will write the stencil bit where the alpha mask passes, and
//          the 360 will write the stencil bit where the alpha mask fails. If this becomes a major issue, then the 360 can be made to 
//          match the PS3's behavior but we will need to switch to FlushHiZStencil(D3DFHZS_SYNCHRONOUS) and reverse some alpha/stencil tests.
//
void CRenderTargets::ConvertAlphaMaskToStencilMask(int alphaRef, grcTexture* pSourceAlpha XENON_ONLY(, bool bUseStencilAsColor))
{
	// Alpha Mask to Stencil Mask conversion stateblocks
	if ( grcStateBlock::BS_Invalid == ms_AlphaMaskToStencilMask_BS )
	{
		grcBlendStateDesc BSDesc;
		BSDesc.BlendRTDesc[0].BlendEnable = false;
		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;

		ms_AlphaMaskToStencilMask_BS = grcStateBlock::CreateBlendState(BSDesc);
	}

	if ( grcStateBlock::DSS_Invalid == ms_AlphaMaskToStencilMask_DS )
	{
		grcDepthStencilStateDesc SDDesc;
		SDDesc.DepthEnable = false;
		SDDesc.DepthWriteMask = false;
		SDDesc.DepthFunc = grcRSV::CMP_LESS;//DEFAULT
		SDDesc.StencilEnable = true;
		SDDesc.StencilReadMask = DEFERRED_MATERIAL_SPECIALBIT;
		SDDesc.StencilWriteMask = DEFERRED_MATERIAL_SPECIALBIT;
		SDDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		SDDesc.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
		SDDesc.BackFace = SDDesc.FrontFace;

		ms_AlphaMaskToStencilMask_DS = grcStateBlock::CreateDepthStencilState(SDDesc);
	}


	PF_PUSH_MARKER("Alpha Mask to Stencil Mask");
	{
		#if __PS3

			// see libgcm-Overview_e.pdf section 11
			// NOTE: stencil mask must be set to 0xff or 0x00 for cellGcmSetClearZcullSurface() to work. to avoid setting a whole state block just for stencil mask we do it directly here.
			GCM_DEBUG(GCM::cellGcmSetClearDepthStencil(GCM_CONTEXT, 0x0));  // Need to set this so that SCull is cleared to 'fail' state
			GCM_DEBUG(GCM::cellGcmSetStencilMask(GCM_CONTEXT, 0xff));
			GCM_DEBUG(GCM::cellGcmSetScullControl(GCM_CONTEXT, CELL_GCM_SCULL_SFUNC_EQUAL, DEFERRED_MATERIAL_SPECIALBIT, DEFERRED_MATERIAL_SPECIALBIT));
			GCM_DEBUG(GCM::cellGcmSetClearZcullSurface(GCM_CONTEXT, false, true)); // clear stencil cull only

		#elif __XENON

			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, true ); // Must be true since we are using an alpha clip below
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, true ); 
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, DEFERRED_MATERIAL_SPECIALBIT );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_EQUAL );
			GRCDEVICE.GetCurrent()->Clear(0, NULL, D3DCLEAR_HISTENCIL_PASS|D3DCLEAR_STENCIL, 0x0, 1.0f, 0x0 ); // There appears to be a bug if you only set D3DCLEAR_HISTENCIL_PASS (results in depth and stencil getting stomped, and hi-stencil not reliably being cleared)
			GRCDEVICE.GetCurrent()->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

		#endif

		// Use an alpha mask to set a stencil bit
		grcStateBlock::SetBlendState(ms_AlphaMaskToStencilMask_BS, ~0U, ~0ULL);
		grcStateBlock::SetDepthStencilState(ms_AlphaMaskToStencilMask_DS, DEFERRED_MATERIAL_SPECIALBIT);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

		PS3_ONLY( GRCDEVICE.SetZMinMaxControl(true, true, false) );
#if DEPTH_BOUNDS_SUPPORT
		GRCDEVICE.SetDepthBoundsTestEnable(false);
#endif //DEPTH_BOUNDS_SUPPORT

		// Draw full screen triangle
		CSprite2d zCullReload;
		zCullReload.BeginCustomList( 
			XENON_ONLY(bUseStencilAsColor? CSprite2d::CS_ALPHA_MASK_TO_STENCIL_MASK_STENCIL_AS_COLOR : )CSprite2d::CS_ALPHA_MASK_TO_STENCIL_MASK, 
			pSourceAlpha
			);
		DrawFullScreenTri(0.0f);
		zCullReload.EndCustomList();

		// Enable HiStencil culling for 360
		XENON_ONLY( GRCDEVICE.GetCurrent()->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS ) );
		XENON_ONLY( GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, false ) );
		XENON_ONLY( GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, true ) );

		PS3_ONLY( GRCDEVICE.SetZMinMaxControl(true, false, false) ); // restore
	}
	PF_POP_MARKER(); //"Alpha Mask to Stencil Mask"
}
#endif // RSG_PS3 || RSG_XENON

//
// Purpose: Draws a full screen triangle, effectively the same as full screen quad but achieves better pixel quad 
//          utilization since there are fewer edges on screen (and thus fewer partial pixel quads)
//
void CRenderTargets::DrawFullScreenTri ( float depth )
{
	// 360 requires an offset to align pixels and texels, PS3 and DX10+ do not.
	const bool bUseOffset = __XENON ? true : false;
	float fHalfTexelOffsetX = 0.f;
	float fHalfTexelOffsetY = 0.f;

	if ( bUseOffset )
	{
		fHalfTexelOffsetX = 0.5f/GRCDEVICE.GetWidth();
		fHalfTexelOffsetY = 0.5f/GRCDEVICE.GetHeight();
	}

	// use a tri as per docs to avoid edges on screen
	grcBegin(drawTris,3);
	grcVertex(0.0f, 0.0f, depth, 0.0f,0.0f,0.0f, Color32(255,255,255,255), 0.0f+fHalfTexelOffsetX, 1.0f+fHalfTexelOffsetY);
	grcVertex(2.0f, 0.0f, depth, 0.0f,0.0f,0.0f, Color32(255,255,255,255), 2.0f+fHalfTexelOffsetX, 1.0f+fHalfTexelOffsetY);
	grcVertex(0.0f, 2.0f, depth, 0.0f,0.0f,0.0f, Color32(255,255,255,255), 0.0f+fHalfTexelOffsetX,-1.0f+fHalfTexelOffsetY);
	grcEnd();
}

void CRenderTargets::AllocateRenderTargets()
{	
	RAGE_TRACK(CRenderTargets);
	RAGE_TRACK(AllocateRenderTargets);
	USE_MEMBUCKET(MEMBUCKET_RENDER);
#if RSG_PC
	ms_Initialized = true;
#endif
	int iWidth		= VideoResManager::GetSceneWidth();
	int iHeight		= VideoResManager::GetSceneHeight();

	int UIWidth		= VideoResManager::GetUIWidth();
	int UIHeight	= VideoResManager::GetUIHeight();

#if __XENON
	{
		// LDR temp buffer so that we have 32bit tone mapped target to sample from for anti-aliasing
		grcTextureFactory::CreateParams osbparams;	
		osbparams.Format		= grctfA8R8G8B8;
		osbparams.HasParent		= true;
		osbparams.Parent		= grcTextureFactory::GetInstance().GetBackBufferDepth(false);
		osbparams.ColorExpBias	= 0;

		m_OffscreenBuffers[0]	= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0,	"UI Buffer",		grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams, kOffscreenBuffer0);
		m_OffscreenBuffers[0]->AllocateMemoryFromPool();

		m_OffscreenBuffers[1]	= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER1,	"OffscreenBuffer1", grcrtPermanent, UIWidth, UIHeight, 32, &osbparams, kOffscreenBuffer1);
		m_OffscreenBuffers[1]->AllocateMemoryFromPool();
		
		m_OffscreenBuffers[2]	= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0,	"OffscreenBuffer2", grcrtPermanent, UIWidth, UIHeight, 32, &osbparams, kOffscreenBuffer2);
		m_OffscreenBuffers[2]->AllocateMemoryFromPool();

		m_OffscreenBuffers[3]	= m_OffscreenBuffers[1];

		osbparams.Parent		= m_OffscreenBuffers[1];
		m_miniMapOffscreenBuffer= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1,	"MiniMapOffscreenBuffer", grcrtPermanent, 512, 512, 32, &osbparams, kMinimapOffscreenBufferHeap);
		m_miniMapOffscreenBuffer->AllocateMemoryFromPool();

		grcTextureFactory::CreateParams paramsMinimapDepthBuffer;
		paramsMinimapDepthBuffer.HasParent	= true;
		paramsMinimapDepthBuffer.Parent		= m_miniMapOffscreenBuffer;
		paramsMinimapDepthBuffer.Format		= grctfA2B10G10R10;
		paramsMinimapDepthBuffer.IsResolvable	= false;
		m_miniMapOffscreenDepthBuffer = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1, "MiniMapOffscreenDepthBuffer", grcrtDepthBuffer,  512, 512, 32, &paramsMinimapDepthBuffer, kMinimapOffscreenDepthBufferHeap);

		grcTextureFactory::CreateParams paramsDepthBuffer;
		paramsDepthBuffer.HasParent		= true;
		paramsDepthBuffer.Parent		= NULL;
		paramsDepthBuffer.Format		= grctfA2B10G10R10;
		ms_DepthBuffers[0]				= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_FRONTBUFFER0, "DepthBuffer0", grcrtDepthBuffer, iWidth, iHeight, 32, &paramsDepthBuffer, 0);
		ms_DepthBuffers[0]->AllocateMemoryFromPool();
		ms_DepthBuffers[1]				= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_FRONTBUFFER1, "DepthBuffer1", grcrtDepthBuffer, iWidth, iHeight, 32, &paramsDepthBuffer, 0);
		ms_DepthBuffers[1]->AllocateMemoryFromPool();
		ms_DepthBuffers[2]				= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_FRONTBUFFER2, "DepthBuffer2", grcrtDepthBuffer, iWidth, iHeight, 32, &paramsDepthBuffer, 0);
		ms_DepthBuffers[2]->AllocateMemoryFromPool();

		paramsDepthBuffer.Format		= grctfA8R8G8B8;
		ms_DepthBufferAliases[0]		= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_FRONTBUFFER0, "DepthBufferAlias0", grcrtPermanent, iWidth, iHeight, 32, &paramsDepthBuffer, 1);
		ms_DepthBufferAliases[0]->AllocateMemoryFromPool();
		ms_DepthBufferAliases[1]		= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_FRONTBUFFER1, "DepthBufferAlias1", grcrtPermanent, iWidth, iHeight, 32, &paramsDepthBuffer, 1);
		ms_DepthBufferAliases[1]->AllocateMemoryFromPool();
		ms_DepthBufferAliases[2]		= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_FRONTBUFFER2, "DepthBufferAlias2", grcrtPermanent, iWidth, iHeight, 32, &paramsDepthBuffer, 1);
		ms_DepthBufferAliases[2]->AllocateMemoryFromPool();

		grcTextureFactory::CreateParams paramsBackBuffer;
		paramsBackBuffer.HasParent		= true;
		paramsBackBuffer.Parent			= grcTextureFactory::GetInstance().GetBackBufferDepth(false);
		paramsBackBuffer.ColorExpBias	= -GRCDEVICE.GetColorExpBias();
		paramsBackBuffer.Format			= grctfA2B10G10R10;
		ms_BackBuffer					= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER23, "BackBuffer", grcrtPermanent, iWidth, iHeight, 32, &paramsBackBuffer, kBackBufferHeap);
		ms_BackBuffer->AllocateMemoryFromPool();

		//////////////////////////////////////////////////////////////////////////
		// part of temporary fix for edram overflow
		paramsBackBuffer.Format			= grctfA8R8G8B8;
		m_BackBufferCopy				= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "BackbufferCopy", grcrtPermanent, iWidth, iHeight, 32, &paramsBackBuffer, kTempBufferPoolHeap);
		m_BackBufferCopy->AllocateMemoryFromPool();
		//////////////////////////////////////////////////////////////////////////

		grcTextureFactory::CreateParams params7e3toInt;
		params7e3toInt.Format		= grctfA2B10G10R10ATI;
		params7e3toInt.HasParent	= true;
		params7e3toInt.Parent		= grcTextureFactory::GetInstance().GetBackBufferDepth(false);
		params7e3toInt.ColorExpBias	= 0;

		ms_BackBuffer7e3ToIntAlias = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER23, "BackBuffer 7e3/int Alias", grcrtPermanent, iWidth, iHeight, 32, &params7e3toInt, kBackBuffer7e3IntAliasHeap);
		ms_BackBuffer7e3ToIntAlias->AllocateMemoryFromPool();

		//640x360 Depth Buffer
		// If any changes are made here for this target, please make the same changes to GBuffer::m_4xHiZRestoreQuarter 
		// as we use that for doing Hi-Stencil/Hi-Z refresh with the quarter depth buffer
		grcTextureFactory::CreateParams params;
		params.HasParent				= true;
		params.Parent					= grcTextureFactory::GetInstance().GetBackBuffer(false);
		params.Format					= grctfA8R8G8B8;
		params.AllocateFromPoolOnCreate	= false;
		params.PoolID					= GetRenderTargetPoolID(XENON_RTMEMPOOL_GENERAL1);
		params.PoolHeap					= kTiledLightingHeap2;
		params.UseHierZ					= true;
		//Setting the Hi-Z base to after the full res depth buffer so that it does not destroy the contents of Hi-Z after downsampling depth
		params.HiZBase					= static_cast<grcRenderTargetXenon*>(grcTextureFactory::GetInstance().GetBackBufferDepth(false))->GetHiZSize();
		m_DepthTargetQuarter			= grcTextureFactory::GetInstance().CreateRenderTarget("Depth Quarter", grcrtDepthBuffer, iWidth/2, iHeight/2, 32, &params);
		//m_DepthTargetQuarter->AllocateMemoryFromPool(); // This happens in water.cpp

		//R32F Version
		//This is suppose to be the tiled lighting target 0 but since that allocation comes later I can't use it,the backbuffer depth should provide a sufficent EDRAM offset
		params.Parent					= grcTextureFactory::GetInstance().GetBackBufferDepth(false);
		params.Format					= grctfR32F;
		params.PoolID					= GetRenderTargetPoolID(XENON_RTMEMPOOL_GENERAL1);
		params.PoolHeap					= kSSAOWaterHeap;
		m_DepthTargetQuarterLinear		= grcTextureFactory::GetInstance().CreateRenderTarget("Depth Quarter Linear", grcrtPermanent, iWidth/2, iHeight/2, 32, &params);
		//m_DepthTargetQuarterLinear->AllocateMemoryFromPool(); // This happens in water.cpp
	}
#endif // __XENON

	#if LIGHT_VOLUME_USE_LOW_RESOLUTION && RSG_DURANGO
		grcTextureFactory::CreateParams volParams;
		volParams.Multisample = 0;
		volParams.UseFloat = true;
		ORBIS_ONLY( volParams.EnableFastClear = false );
		volParams.Format = grctfA16B16G16R16F; // up-sampling & reconstruction optimizations require that we have an alpha channel  
		const int nVolumeBPP = 64;
		const int nVolWidth = iWidth/2;
		const int nVolHeight =iHeight/2;
		
		m_volumeOffscreenBuffer = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Volume Lights Offscreen buffer", grcrtPermanent, nVolWidth, nVolHeight, nVolumeBPP, &volParams, 1 WIN32PC_ONLY(, m_volumeOffscreenBuffer));
		m_volumeReconstructionBuffer = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Volume Lights Reconstruction buffer", grcrtPermanent, nVolWidth, nVolHeight, nVolumeBPP, &volParams, 1 WIN32PC_ONLY(, m_volumeReconstructionBuffer));
	#endif

	// create offscreen Buffer
	grcTextureFactory::CreateParams params;

	// color
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = true;
	params.UseHierZ = false;

	params.InTiledMemory	= true;
	params.IsSwizzled		= false;
	params.EnableCompression = false;

	u32 bpp = 64;
	params.IsSRGB = true;
	params.Format = grctfA16B16G16R16F;

#if !__FINAL && (__D3D11 || RSG_ORBIS)
	if( PARAM_DX11Use8BitTargets.Get() )
	{
		params.Format = grctfA8R8G8B8;
		params.UseFloat = false;
		params.IsSRGB = true;
		bpp = 32;
	}
	else
#endif // !__FINAL
	{
		params.Format = grctfA16B16G16R16F;
		bpp = 64;
		// Back Buffer needs alpha but BackBUfferCopy only needs alpha for fxaa
		/*
#if __D3D11 || RSG_ORBIS
		if (!PARAM_highColorPrecision.Get())
		{
			params.Format = grctfR11G11B10F;
			bpp = 32;
		}
#endif // __D3D11 || RSG_ORBIS
		*/
	}

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	grcDevice::MSAAMode multisample = GRCDEVICE.GetMSAA();
	params.Multisample = multisample;
# if RSG_ORBIS
	params.EnableFastClear = false;
# endif	//RSG_ORBIS
# if DEVICE_EQAA
	if (!UseFullBackbufferCoverage(multisample))
	{
		params.Multisample.DisableCoverage();
	}
# else
	const u32 multisampleQuality = GRCDEVICE.GetMSAAQuality();
	params.MultisampleQuality = (u8)multisampleQuality;
# endif

	ms_BackBuffer = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BackBuffer", grcrtPermanent, iWidth, iHeight, bpp, &params, 0 WIN32PC_ONLY(, ms_BackBuffer) DURANGO_ONLY(, ms_BackBuffer));

#if DYNAMIC_BB
	grcTextureFactory::CreateParams altParams = params;
#endif
	if (!ms_BackBuffer)
	{
		Quitf(ERR_GFX_RENDERTARGET,"failed to create RenderTarget::ms_BackBuffer");
	}
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS

#if DEVICE_MSAA
	if(multisample)
	{
#if ENABLE_PED_PASS_AA_SOURCE
		ms_BackBufferCopyAA = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BackBufferCopyAA", grcrtPermanent, iWidth, iHeight, bpp, &params, 0 WIN32PC_ONLY(, ms_BackBufferCopyAA) DURANGO_ONLY(, ms_BackBufferCopyAA));
#endif // ENABLE_PED_PASS_AA_SOURCE
		grcTextureFactory::CreateParams ResolvedParams = params;
		ResolvedParams.Multisample = grcDevice::MSAA_None;
		ms_BackBuffer_Resolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BackBuffer_Resolved", grcrtPermanent, iWidth, iHeight, bpp, &ResolvedParams, 0 WIN32PC_ONLY(, ms_BackBuffer_Resolved));
	}
#endif // DEVICE_MSAA

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	ms_BackBufferCopy =
#if DEVICE_MSAA
		multisample ? ms_BackBuffer_Resolved :
#endif //DEVICE_MSAA
		CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "BackBufferCopy", grcrtPermanent, iWidth, iHeight, bpp, &params, 0 WIN32PC_ONLY(, ms_BackBufferCopy));
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	params.UseHierZ = true;
	params.Format = grctfD32FS8;
	params.Lockable = 0;
	int depthBpp	= 40;

	// DX11: TODO: Could set this up so the lockable textures are created on first use.
	// Current method creates extra lockable render targets with associated extra memory usage
	ENTITYSELECT_ONLY(params.Lockable = __BANK);	// In __BANK these are lockable as they're used for entity select.
	params.Multisample = grcDevice::MSAA_None;

#if __D3D11
	grcTextureFactoryDX11 &textureFactory11 = static_cast<grcTextureFactoryDX11&>( grcTextureFactory::GetInstance() );
#endif

	//We need a copy of the depth buffer pre alpha for the depth of field through windows, etc...
	ms_DepthBuffer_PreAlpha = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DepthBuffer Pre Alpha",	grcrtDepthBuffer, iWidth, iHeight, depthBpp, &params, 0 WIN32PC_ONLY(, ms_DepthBuffer_PreAlpha) DURANGO_ONLY(, ms_DepthBuffer_PreAlpha));
#if RSG_PC
	if (VideoResManager::IsSceneScaled() && multisample > 1)
		ms_UIDepthBuffer_Resolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UIDepthBuffer Resolved",	grcrtDepthBuffer, UIWidth, UIHeight, depthBpp, &params, 0 WIN32PC_ONLY(, ms_UIDepthBuffer_Resolved));
	params.Multisample			= multisample;
	ms_UIDepthBuffer = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "UIDepthBuffer",	grcrtDepthBuffer, UIWidth, UIHeight, depthBpp, &params, 0 WIN32PC_ONLY(, ms_UIDepthBuffer));
	if (VideoResManager::IsSceneScaled())
	{
		ms_UIDepthBuffer_PreAlpha = multisample > 1 ? ms_UIDepthBuffer_Resolved : ms_UIDepthBuffer;

		if (GRCDEVICE.IsReadOnlyDepthAllowed())
		{
			ms_UIDepthBuffer_Resolved_ReadOnly = textureFactory11.CreateRenderTarget("UIDepthBuffer (read-only)"
				,multisample > 1 ? ms_UIDepthBuffer_Resolved->GetTexturePtr() : ms_UIDepthBuffer->GetTexturePtr() , params.Format, DepthReadOnly
				WIN32PC_ONLY(, ms_UIDepthBuffer_Resolved_ReadOnly));
		}
	}	
#endif
#if RSG_PC && __D3D11
	if (GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		m_DepthBuffer_PreAlpha_ReadOnly = textureFactory11.CreateRenderTarget("DepthBuffer Pre Alpha (read-only)"
			,VideoResManager::IsSceneScaled() ? ms_UIDepthBuffer_PreAlpha->GetTexturePtr() : ms_DepthBuffer_PreAlpha->GetTexturePtr() , params.Format, DepthReadOnly
			WIN32PC_ONLY(, m_DepthBuffer_PreAlpha_ReadOnly) DURANGO_ONLY(, m_DepthBuffer_PreAlpha_ReadOnly));
	}
#endif

	params.Multisample			= multisample;
#if !DEVICE_EQAA
	params.MultisampleQuality	= (u8)multisampleQuality;
#endif
#if RSG_ORBIS
	params.EnableFastClear = true;
#endif //RSG_ORBIS
#if RSG_DURANGO
	params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_GBUF
		| grcTextureFactory::TextureCreateParams::ESRPHASE_SHADOWS
		| grcTextureFactory::TextureCreateParams::ESRPHASE_RAIN_COLLSION_MAP
		| grcTextureFactory::TextureCreateParams::ESRPHASE_WATER_REFLECTION
		| grcTextureFactory::TextureCreateParams::ESRPHASE_RAIN_UPDATE
		| grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING
		| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE
		| grcTextureFactory::TextureCreateParams::ESRPHASE_HUD;
	params.ESRAMMaxSize = 4 * 1024 * 1024;
#endif
	ms_DepthBuffer = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DepthBuffer",	grcrtDepthBuffer, iWidth, iHeight, depthBpp, &params, 0 WIN32PC_ONLY(, ms_DepthBuffer) DURANGO_ONLY(, ms_DepthBuffer));
#if RSG_DURANGO
	params.ESRAMPhase = 0;
	params.ESRAMMaxSize = 0;
#endif
#if RSG_ORBIS
	params.EnableFastClear = false;
#endif //RSG_ORBIS
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS	

#if RSG_PC || RSG_DURANGO
#if __D3D11
	grcTextureObject *const depthObjectMSAA = ms_DepthBuffer->GetTexturePtr();
	if( GRCDEVICE.IsReadOnlyDepthAllowed() )
	{
		// no need for the copy, we use read-only depth instead
		ms_DepthBufferCopy = textureFactory11.CreateRenderTarget("DepthBufferCopy (read-only)", depthObjectMSAA, params.Format, DepthReadOnly WIN32PC_ONLY(, ms_DepthBufferCopy) DURANGO_ONLY(, ms_DepthBufferCopy));
	}else
#endif	//__D3D11
	{
		ms_DepthBufferCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DepthBufferCopy",	grcrtDepthBuffer, iWidth, iHeight, depthBpp, &params, 0 WIN32PC_ONLY(, ms_DepthBufferCopy));
	}
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS

#if DEVICE_MSAA
	if (multisample)
	{
		grcTextureFactory::CreateParams ResolvedParams = params;
		ResolvedParams.Format = SSAO_OUTPUT_DEPTH ? grctfD32FS8 : grctfNone;
		ResolvedParams.UseFloat = true;	//Warning: floating point buffer will not store enough precision for far objects (close to 1)
		ResolvedParams.Multisample = grcDevice::MSAA_None;
		ms_DepthBuffer_Resolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "DepthBuffer_Resolved",
			SSAO_OUTPUT_DEPTH ? grcrtDepthBuffer : grcrtPermanent, iWidth, iHeight, depthBpp, &ResolvedParams, 0 WIN32PC_ONLY(, ms_DepthBuffer_Resolved));
#if __D3D11
		grcTextureObject *const depthObject = ms_DepthBuffer_Resolved->GetTexturePtr();
		Assert(depthObject != NULL);
		ms_DepthBuffer_Resolved_ReadOnly = textureFactory11.CreateRenderTarget("DepthBuffer_Resolved (read-only)", depthObject, ResolvedParams.Format, DepthReadOnly WIN32PC_ONLY(, ms_DepthBuffer_Resolved_ReadOnly) DURANGO_ONLY(, ms_DepthBuffer_Resolved_ReadOnly));
#endif // __D3D11
	}
#endif // DEVICE_MSAA

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	grcTextureFactory::CreateParams depthParams;
	depthParams.Format			= grctfD32FS8;
	m_DepthTargetQuarter		= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Depth Quarter", grcrtDepthBuffer, iWidth/2, iHeight/2, 32, &depthParams, 0 WIN32PC_ONLY(, m_DepthTargetQuarter));
	#if __D3D11 && RSG_PC
		if( GRCDEVICE.IsReadOnlyDepthAllowed() )
		{
			grcTextureObject* const pDepthQuarterObject = m_DepthTargetQuarter->GetTexturePtr();
			Assert(pDepthQuarterObject != NULL);
			m_DepthTargetQuarter_ReadOnly = textureFactory11.CreateRenderTarget("Depth Quarter(read-only)", pDepthQuarterObject, depthParams.Format, DepthReadOnly WIN32PC_ONLY(, m_DepthTargetQuarter_ReadOnly) DURANGO_ONLY(, m_DepthTargetQuarter_ReadOnly));
		}else
		{
			m_DepthTargetQuarter_Copy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Depth Quarter (copy)", grcrtDepthBuffer, iWidth/2, iHeight/2, 32, &depthParams, 0 WIN32PC_ONLY(, m_DepthTargetQuarter_Copy));
		}
	#endif // __D3D11

	//R32F Version
	depthParams.StereoRTMode	= grcDevice::STEREO;
	depthParams.Format			= grctfR32F;
	m_DepthTargetQuarterLinear	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Depth Quarter Linear", grcrtPermanent, iWidth/2, iHeight/2, 32, &depthParams, 0 WIN32PC_ONLY(, m_DepthTargetQuarterLinear));
#endif	//RSG_PC || RSG_DURANGO || RSG_ORBIS

#if	DEVICE_MSAA
	grcTextureFactory::CreateParams osbparams;
	osbparams.Format				= grctfA8R8G8B8;
	osbparams.Multisample			= multisample;
# if RSG_ORBIS
	osbparams.EnableFastClear = false;
# endif	//RSG_ORBIS
# if DEVICE_EQAA
	//osbparams.ForceHardwareResolve = !RSG_ORBIS;
	osbparams.Multisample.DisableCoverage();
	osbparams.ForceFragmentMask = false;
# else
	osbparams.MultisampleQuality	= (u8)multisampleQuality;
# endif // DEVICE_EQAA

#if RSG_DURANGO
	osbparams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_9 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_10;
	m_OffscreenBuffers[1]	= CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer1",	grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams, 0 WIN32PC_ONLY(, m_OffscreenBuffers[1]));
#else
	m_OffscreenBuffers[1] = NULL;
#endif

#if DYNAMIC_BB
	{
		altParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING_1
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_0
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_1
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_2
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_3
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_4
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_5
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_6
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_7
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_8
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_9;
		altParams.ESRAMMaxSize = 16 * 1024 * 1024;
		ms_AltBackBuffer = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "AltBackBuffer", grcrtPermanent, iWidth, iHeight, bpp, &altParams, 0 , ms_AltBackBuffer);
		if (ms_AltBackBuffer && ms_BackBuffer)
		{
			static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->SetAltTarget(ms_AltBackBuffer);
			static_cast<grcRenderTargetDurango*>(ms_BackBuffer)->SetUseAltTestFunc(CRenderTargets::UseAltBackBufferTest);
		}
	}
#endif
#if RSG_DURANGO
	osbparams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_HUD;
#endif
	osbparams.Multisample	= multisample;
#if DEVICE_EQAA
	osbparams.ForceFragmentMask = multisample.NeedFmask();
	osbparams.EnableHighQualityResolve = true;
#endif
	m_OffscreenBuffers[2]	= CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer2",	grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams, 0 WIN32PC_ONLY(, m_OffscreenBuffers[2]));
#if RSG_DURANGO
	osbparams.ESRAMPhase = 0;
#endif
	osbparams.Multisample	= 0;
#if __DEV
	m_OffscreenBuffers[0]	= CreateRenderTarget(RTMEMPOOL_NONE, "UI Buffer",			grcrtPermanent,	UIWidth, UIHeight, 32, &osbparams, 0 WIN32PC_ONLY(, m_OffscreenBuffers[0]));
#else
	m_OffscreenBuffers[0]	= NULL;
#endif

	osbparams.Multisample = multisample;
	m_OffscreenBuffers[3]	= NULL;
#if RSG_PC
	if (VideoResManager::IsSceneScaled())
		m_OffscreenBuffers[3]	= CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer3",	grcrtPermanent,	iWidth , iHeight, 32, &osbparams, 0 WIN32PC_ONLY(, m_OffscreenBuffers[3]));
#endif

	grcTextureFactory::CreateParams resolvedParams = osbparams;
	resolvedParams.Multisample			= grcDevice::MSAA_None;
#if !DEVICE_EQAA
	resolvedParams.MultisampleQuality	= 0;
#endif

	if (!multisample)
	{
		if (m_OffscreenBuffersResolved[1]) {delete m_OffscreenBuffersResolved[1]; m_OffscreenBuffersResolved[1] = NULL;}
		if (m_OffscreenBuffersResolved[2]) {delete m_OffscreenBuffersResolved[2]; m_OffscreenBuffersResolved[2] = NULL;}
		if (m_OffscreenBuffersResolved[3]) {delete m_OffscreenBuffersResolved[3]; m_OffscreenBuffersResolved[3] = NULL;}
	}

	m_OffscreenBuffersResolved[0] = NULL;
#if RSG_DURANGO
	resolvedParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_10 | grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_11;
#endif
	m_OffscreenBuffersResolved[1] = multisample ? // m_OffscreenBuffers[1]->GetMSAA() ? 
		CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer1_Resolved",	grcrtPermanent,	iWidth, iHeight, 32, &resolvedParams, 0 WIN32PC_ONLY(, m_OffscreenBuffersResolved[1]))
		: NULL;
#if RSG_DURANGO
	resolvedParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_HUD;
#endif
	m_OffscreenBuffersResolved[2] = m_OffscreenBuffers[2]->GetMSAA() ? 
		CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer2_Resolved",	grcrtPermanent,	UIWidth, UIHeight, 32, &resolvedParams, 0 WIN32PC_ONLY(, m_OffscreenBuffersResolved[2]))
		: NULL;
#if RSG_DURANGO
	resolvedParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_11;
	m_OffscreenBuffersResolved[3] = multisample ? 
		CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer1X_Resolved",	grcrtPermanent,	UIWidth, UIHeight, 32, &resolvedParams, 0 WIN32PC_ONLY(, m_OffscreenBuffersResolved[3]))
		: NULL;
	m_OffscreenBuffers[3] = m_OffscreenBuffers[1];
#else
	m_OffscreenBuffersResolved[3] = NULL;

#if RSG_PC
	if (VideoResManager::IsSceneScaled() && multisample)
		m_OffscreenBuffersResolved[3] = CreateRenderTarget(RTMEMPOOL_NONE, "OffscreenBuffer1X_Resolved",	grcrtPermanent,	iWidth , iHeight, 32, &resolvedParams, 0 WIN32PC_ONLY(, m_OffscreenBuffersResolved[3]));
#endif
#endif


#endif // DEVICE_MSAA


#if __D3D11
	grcRenderTargetD3D11 *const depthCopy = static_cast<grcRenderTargetD3D11*>( GRCDEVICE.IsReadOnlyDepthAllowed() ? ms_DepthBuffer : ms_DepthBufferCopy );
	
	grcTextureFormat eStencilFormat = grctfX32S8;

	ms_DepthBuffer_Stencil = textureFactory11.CreateRenderTarget( "StencilBuffer", depthObjectMSAA, eStencilFormat, DepthStencilRW WIN32PC_ONLY(, ms_DepthBuffer_Stencil) );

# if RSG_PC
	static_cast<grcRenderTargetDX11*>(ms_DepthBuffer)->SetSecondaryTextureView(ms_DepthBuffer_Stencil->GetTextureView());
# endif

	ms_DepthBufferCopy_Stencil = textureFactory11.CreateRenderTarget( "StencilBufferCopy",
		depthCopy->GetTexturePtr(), eStencilFormat, DepthStencilRW WIN32PC_ONLY(, ms_DepthBufferCopy_Stencil) );

	ms_DepthBuffer_ReadOnly = textureFactory11.CreateRenderTarget( "DepthBufferReadOnly", depthObjectMSAA, params.Format, DepthStencilReadOnly WIN32PC_ONLY(, ms_DepthBuffer_ReadOnly) );

#if RSG_PC
	ms_DepthBuffer_ReadOnlyDepth = textureFactory11.CreateRenderTarget( "DepthBufferReadOnlyDepth", depthObjectMSAA, params.Format, DepthReadOnly WIN32PC_ONLY(, ms_DepthBuffer_ReadOnlyDepth) );
#endif

# if DEVICE_EQAA
	if (multisample && multisample.m_uFragments==1)
	{
		textureFactory11.EnableMultisample(ms_DepthBuffer_Stencil);
		textureFactory11.EnableMultisample(ms_DepthBufferCopy_Stencil);
		textureFactory11.EnableMultisample(ms_DepthBuffer_ReadOnly);
	}
# endif // DEVICE_EQAA
#endif // __D3D11

#if RSG_ORBIS
	grcTextureFactoryGNM& textureFactoryGNM = static_cast<grcTextureFactoryGNM&>(grcTextureFactoryGNM::GetInstance()); 
	grcTextureFormat eStencilFormat = grctfX24G8;
	ms_DepthBuffer_Stencil = textureFactoryGNM.CreateRenderTarget( "StencilBuffer", ms_DepthBuffer, eStencilFormat );
#endif

#if __PPU

#if PSN_USE_FP16
	grcTextureFactory::CreateParams params0;

	params0.Multisample			= 0;
	params0.HasParent			= true;
	params0.Parent				= NULL; 
	params0.UseFloat			= true;
	params0.InTiledMemory		= true;
	params0.IsSwizzled			= false;
	params0.EnableCompression	= false;
	params0.InLocalMemory		= true;
	params0.Format				= grctfA16B16G16R16F;
	params0.IsSRGB				= false;

	ms_BackBuffer = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P10240_TILED_LOCAL_COMPMODE_DISABLED, "BackBuffer FP16", grcrtPermanent, iWidth, iHeight, 64, &params0,0);
	ms_BackBuffer->AllocateMemoryFromPool();
#else
	ms_BackBuffer = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P10240_TILED_LOCAL_COMPMODE_DISABLED, "BackBuffer", grcrtPermanent, iWidth, iHeight, 32, &params,0);
	ms_BackBuffer->AllocateMemoryFromPool();
#endif // PSN_USE_FP16
	
	grcTextureFactory::CreateParams osbparams;
	osbparams.Format		= grctfA8R8G8B8;

	m_OffscreenBuffers[3] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "OffscreenBuffer3",	grcrtPermanent, UIWidth, UIHeight, 32, &osbparams, 6);
	m_OffscreenBuffers[2] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "OffscreenBuffer2",	grcrtPermanent, UIWidth, UIHeight, 32, &osbparams, 6);
	m_OffscreenBuffers[1] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "OffscreenBuffer1",	grcrtPermanent, UIWidth, UIHeight, 32, &osbparams, 6);
	m_OffscreenBuffers[0] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "UI Buffer",		grcrtPermanent, UIWidth, UIHeight, 32, &osbparams, 6);
	m_OffscreenBuffers[3]->AllocateMemoryFromPool();
	m_OffscreenBuffers[2]->AllocateMemoryFromPool();
	m_OffscreenBuffers[1]->AllocateMemoryFromPool();
	m_OffscreenBuffers[0]->AllocateMemoryFromPool();

	params.Format				= grctfA8R8G8B8;
	params.UseFloat				= false;

	params.EnableCompression	= true;
	params.UseHierZ				= true;
	ms_DepthBuffer = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR_BBUFFER, "DepthBuffer",	grcrtDepthBuffer, iWidth, iHeight, 32, NULL, 8);
	ms_DepthBuffer->AllocateMemoryFromPool();

	params.InTiledMemory		= true;

	ms_PatchedDepthBuffer		= ((grcRenderTargetGCM*)ms_DepthBuffer)->CreatePrePatchedTarget(false);

	//640x360 Depth Buffer
	params.Format				= grctfD24S8;
	params.EnableCompression	= true;
	params.UseHierZ				= true;
	params.Multisample = 0;
	params.CreateAABuffer = false;
	m_DepthTargetQuarter		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR, "Depth Quarter", grcrtDepthBuffer, iWidth/2, iHeight/2, 32, &params, 0);
	//m_DepthTargetQuarter->AllocateMemoryFromPool(); // This happens in CRenderer::Init()

	params.Format				= grctfA8R8G8B8;
	params.EnableCompression	= false;
	params.UseHierZ				= true;
	params.Multisample = 0;
	params.CreateAABuffer = false;
	params.UseCustomZCullAllocation = USE_DEPTHBUFFER_ZCULL_SHARING ? true : false; // We force this target to share zcull with the back buffer once it's allocated
	m_OffscreenDepthBuffer		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "Offscreen Depth", grcrtDepthBuffer, iWidth, iHeight, 32, &params, 5);
	//m_OffscreenDepthBuffer->AllocateMemoryFromPool();  // This happens in CRenderer::Init()
	//m_OffscreenDepthBufferAlias = ((grcRenderTargetGCM*)m_OffscreenDepthBuffer)->CreatePrePatchedTarget(false); // This happens in CRenderer::Init()
	params.UseCustomZCullAllocation = false; // Turn this off now so it doesn't create havoc down below

	//R32F Version
	params.Format				= grctfR32F;
	params.UseHierZ				= false;
	params.EnableCompression	= true;
	m_DepthTargetQuarterPadding	= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "Depth Quarter Padding", grcrtPermanent, 640, 360, 32, &params, 5);
	m_DepthTargetQuarterLinear	= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "Depth Quarter Linear", grcrtPermanent, iWidth/2, iHeight/2, 32, &params, 5);
	//m_DepthTargetQuarterPadding->AllocateMemoryFromPool(); // This happens in Water.cpp
	//m_DepthTargetQuarterLinear->AllocateMemoryFromPool(); // This happens in Water.cpp

	// Stencil/Zfar cull stateblocks
	{
		grcBlendStateDesc BSDesc;
		BSDesc.AlphaTestEnable = false;
		BSDesc.BlendRTDesc[0].BlendEnable = false;
		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;

		ms_Refresh_BS = grcStateBlock::CreateBlendState(BSDesc);
	}

	// Stencil cull refresh stateblocks
	{
		grcDepthStencilStateDesc SDDesc;

		SDDesc.DepthEnable		= false;
		SDDesc.DepthWriteMask	= false;
		SDDesc.DepthFunc		= grcRSV::CMP_LESS;//DEFAULT
		SDDesc.StencilEnable	= true;
		SDDesc.StencilReadMask	= 0xFF; //DEFAULT
		SDDesc.StencilWriteMask	= 0xFF; //DEFAULT

		SDDesc.FrontFace.StencilFailOp		= grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_INVERT;
		SDDesc.FrontFace.StencilFunc		= grcRSV::CMP_LESS;
		SDDesc.BackFace						= SDDesc.FrontFace;

		ms_RefreshStencil_DS				= grcStateBlock::CreateDepthStencilState(SDDesc);
		SDDesc.DepthEnable					= true;
		ms_RefreshStencilPreserveZFar_DS	= grcStateBlock::CreateDepthStencilState(SDDesc);
	}
	
	// Stencil cull + ZFAR refresh stateblocks
	{
		grcDepthStencilStateDesc SDDesc;

		SDDesc.DepthEnable = true;//DEFAULT
		SDDesc.DepthWriteMask = true;//DEFAULT
		SDDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);//DEFAULT
		SDDesc.StencilEnable = true;
		SDDesc.StencilReadMask = 0x00;
		SDDesc.StencilWriteMask = 0xFF; //DEFAULT

		SDDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP; //DEFAULT
		SDDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_INVERT;
		SDDesc.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;//DEFAULT
		SDDesc.BackFace = SDDesc.FrontFace;

		ms_RefreshStencilZFar_DS = grcStateBlock::CreateDepthStencilState(SDDesc);
	}
	
	// ZFAR refresh stateblocks
	{
		grcDepthStencilStateDesc SDDesc;

		SDDesc.DepthEnable = true;
		SDDesc.DepthWriteMask = true;
		SDDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		SDDesc.StencilEnable = false;
		ms_RefreshZFar_L_DS = grcStateBlock::CreateDepthStencilState(SDDesc);

		SDDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATEREQUAL);
		ms_RefreshZFar_G_DS = grcStateBlock::CreateDepthStencilState(SDDesc);
	}
	
#endif // __PPU

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	{
		grcTextureFactory::TextureCreateParams replayThumbnailParams(grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead|grcsWrite, NULL, grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);

		for(u32 i=0; i<REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE; i++)
		{
			char name[256];
			sprintf(name, "Replay thumbnail %d", i);
			BANK_ONLY(grcTexture::SetCustomLoadName(name);)
			ms_ReplayThumbnailTex[i] = grcTextureFactory::GetInstance().Create( CReplayThumbnail::GetThumbnailWidth(), CReplayThumbnail::GetThumbnailHeight(), grctfA8R8G8B8, NULL, 1U, &replayThumbnailParams);
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			ms_ReplayThumbnailRT[i] = grcTextureFactory::GetInstance().CreateRenderTarget(name, ms_ReplayThumbnailTex[i]->GetTexturePtr());
		}
	}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY

	//Copy depth state
	{
		grcDepthStencilStateDesc dssDesc;
		dssDesc.DepthFunc = grcRSV::CMP_ALWAYS;
		dssDesc.DepthWriteMask = TRUE;
		dssDesc.DepthEnable = TRUE;
		ms_CopyDepth_DS = grcStateBlock::CreateDepthStencilState(dssDesc, "copyDepthDSS");
	}
#if __PS3
	{
		grcDepthStencilStateDesc dssDesc;
		dssDesc.DepthFunc = grcRSV::CMP_ALWAYS;
		dssDesc.DepthWriteMask = TRUE;
		dssDesc.DepthEnable = TRUE;
		dssDesc.StencilEnable = TRUE;
		dssDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_REPLACE;
		dssDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_REPLACE;
		dssDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_REPLACE;
		dssDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		dssDesc.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
		dssDesc.BackFace = dssDesc.FrontFace;
		ms_CopyDepthClearStencil_DS = grcStateBlock::CreateDepthStencilState(dssDesc, "copyDepthClearStencilDSS");
	}
#endif

	//Copy with Stencil Mask
	{
		grcDepthStencilStateDesc dssDesc;
		dssDesc.DepthFunc = grcRSV::CMP_NEVER;
		dssDesc.DepthWriteMask = FALSE;
		dssDesc.DepthEnable = FALSE;
		dssDesc.StencilEnable = TRUE;
		dssDesc.StencilReadMask = DEFERRED_MATERIAL_CLEAR;
		dssDesc.StencilWriteMask = 0;
		dssDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		dssDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
		dssDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		dssDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		dssDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		dssDesc.BackFace = dssDesc.FrontFace;
		ms_CopyColorStencilMask_DS = grcStateBlock::CreateDepthStencilState(dssDesc, "copyColorStencilMaskDSS");
	}

#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		grcTextureFactory::TextureCreateParams TextureParams(
			grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead|grcsWrite, NULL, 
			grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);
		
		ms_BackBufferTexForPause = grcTextureFactory::GetInstance().Create(iWidth, iHeight, grctfA16B16G16R16F, NULL, 1, &TextureParams);
		ms_BackBufferRTForPause  = grcTextureFactory::GetInstance().CreateRenderTarget("BackBufferRT for Pause", ms_BackBufferTexForPause->GetTexturePtr());	
	}
#endif //RSG_PC
}

#if RSG_PC
void CRenderTargets::DeviceLost()
{
	if (ms_Initialized)
	{
		DeAllocateRenderTargets();
	}
}

void CRenderTargets::DeviceReset()
{
	if (ms_Initialized)
	{
		AllocateRenderTargets();
	}
}
#endif	//RSG_PC

void CRenderTargets::DeAllocateRenderTargets()
{
	RAGE_TRACK(CRenderTargets);
	RAGE_TRACK(DeAllocateRenderTargets);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

#if RSG_PC || __PPU || RSG_DURANGO || RSG_ORBIS
#if RSG_PC || RSG_DURANGO
	if (ms_BackBuffer) {delete ms_BackBuffer; ms_BackBuffer = NULL;}

	if (ms_BackBufferCopy) {
		delete ms_BackBufferCopy; ms_BackBufferCopy = NULL;
#if DEVICE_MSAA
 		ms_BackBuffer_Resolved = NULL;
#endif
	}

	if (ms_DepthBuffer) {delete ms_DepthBuffer; ms_DepthBuffer = NULL;}

	if (ms_DepthBufferCopy) {delete ms_DepthBufferCopy; ms_DepthBufferCopy = NULL;}

	if (m_DepthTargetQuarter) {delete m_DepthTargetQuarter; m_DepthTargetQuarter = NULL;}

	if (m_DepthTargetQuarterLinear) {delete m_DepthTargetQuarterLinear; m_DepthTargetQuarterLinear = NULL;}

	if (ms_DepthBuffer_PreAlpha) {delete ms_DepthBuffer_PreAlpha; ms_DepthBuffer_PreAlpha = NULL;}

#if RSG_PC && __D3D11
	if (m_DepthTargetQuarter_ReadOnly) { delete m_DepthTargetQuarter_ReadOnly; m_DepthTargetQuarter_ReadOnly = NULL;}
	if (m_DepthTargetQuarter_Copy) { delete m_DepthTargetQuarter_Copy; m_DepthTargetQuarter_Copy = NULL;}
	if (m_DepthBuffer_PreAlpha_ReadOnly) { delete m_DepthBuffer_PreAlpha_ReadOnly; m_DepthBuffer_PreAlpha_ReadOnly = NULL;}
	if (ms_UIDepthBuffer) {delete ms_UIDepthBuffer; ms_UIDepthBuffer = NULL;}
	if (ms_UIDepthBuffer_Resolved_ReadOnly) {delete ms_UIDepthBuffer_Resolved_ReadOnly; ms_UIDepthBuffer_Resolved_ReadOnly = NULL;}
#endif // RSG_PC && __D3D11

	if (m_OffscreenBuffers[0]) {delete m_OffscreenBuffers[0]; m_OffscreenBuffers[0] = NULL;}
	if (m_OffscreenBuffers[1]) { DURANGO_ONLY(delete m_OffscreenBuffers[1];) m_OffscreenBuffers[1] = NULL;}
	if (m_OffscreenBuffers[2]) {delete m_OffscreenBuffers[2]; m_OffscreenBuffers[2] = NULL;}
	if (m_OffscreenBuffers[3]) {delete m_OffscreenBuffers[3]; m_OffscreenBuffers[3] = NULL;}

	// Shared Render Targets
	LOW_RES_VOLUME_LIGHT_ONLY( if (m_volumeOffscreenBuffer) { DURANGO_ONLY(delete m_volumeOffscreenBuffer;) m_volumeOffscreenBuffer = NULL;} )
	LOW_RES_VOLUME_LIGHT_ONLY( if (m_volumeReconstructionBuffer) { DURANGO_ONLY(delete m_volumeReconstructionBuffer;) m_volumeReconstructionBuffer = NULL;} )

#endif	//RSG_PC,RSG_DURANGO

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	{
		for(u32 i=0; i<REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE; i++)
		{
			if(ms_ReplayThumbnailTex[i]) { ms_ReplayThumbnailTex[i]->Release(); ms_ReplayThumbnailTex[i] = NULL; }
			if(ms_ReplayThumbnailRT[i]) { delete ms_ReplayThumbnailRT[i]; ms_ReplayThumbnailRT[i] = NULL; }		
		}
	}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif

#if __D3D11 || RSG_ORBIS
	if (ms_BackBufferCopy != ms_BackBuffer)	
		delete ms_BackBufferCopy;
	ms_BackBufferCopy = NULL;
	if (ms_DepthBuffer_Stencil)	{delete ms_DepthBuffer_Stencil; ms_DepthBuffer_Stencil = NULL;}

	if (ms_DepthBuffer_Resolved) {delete ms_DepthBuffer_Resolved; ms_DepthBuffer_Resolved = NULL;}
#endif

#if __D3D11
	if (ms_DepthBufferCopy_Stencil) {delete ms_DepthBufferCopy_Stencil; ms_DepthBufferCopy_Stencil = NULL;}

	if (ms_DepthBuffer_ReadOnly) {delete ms_DepthBuffer_ReadOnly; ms_DepthBuffer_ReadOnly = NULL;}
#if RSG_PC
	if (ms_DepthBuffer_ReadOnlyDepth) {delete ms_DepthBuffer_ReadOnlyDepth; ms_DepthBuffer_ReadOnlyDepth = NULL;}
#endif

//	if (ms_CopyDepth_DS) {delete ms_CopyDepth_DS; ms_CopyDepth_DS = NULL;}
#endif	//__D3D11
#endif //RSG_PC || __PPU || RSG_DURANGO || RSG_ORBIS

#if RSG_PC
	if (ms_BackBufferTexForPause) {ms_BackBufferTexForPause->Release(); ms_BackBufferTexForPause = NULL;}
	if (ms_BackBufferRTForPause) {delete ms_BackBufferRTForPause; ms_BackBufferRTForPause = NULL;}
#endif //RSG_PC
}

struct RestoreHiStencil
{
	XENON_ONLY(DWORD hasHiStencilEnabled; )
	RestoreHiStencil(){
		XENON_ONLY( GRCDEVICE.GetCurrent()->GetRenderState( D3DRS_HISTENCILENABLE, &hasHiStencilEnabled) );
	}
	~RestoreHiStencil(){
		XENON_ONLY( GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, hasHiStencilEnabled ) );
	}
};
void CRenderTargets::LockSceneRenderTargets(bool lockDepth)
{
	RestoreHiStencil restore;

	// on PSN, we will render to a rendertarget so we have easy access to the screen contents
	Assertf(!ms_RenderTargetActive, "Attemping to lock scene rt when already active!");
	grcRenderTarget *fbRT	 = CRenderTargets::GetBackBuffer();
	grcRenderTarget *depthRT = lockDepth? CRenderTargets::GetDepthBuffer() : NULL;
	grcTextureFactory::GetInstance().LockRenderTarget(0, fbRT, depthRT);
	ms_RenderTargetActive = true;
}
//
// Purpose:  As BindCustomRenderTarget, but works for cases when we need to bind the same resource in a shader
// Under DX11 the copy is actually a read-only view of the original depth buffer
//
void CRenderTargets::LockSceneRenderTargets_DepthCopy()
{
#if RSG_PC || RSG_DURANGO
	grcRenderTarget *fbRT	 = CRenderTargets::GetBackBuffer();
	#if __D3D11
		grcRenderTarget *depthRT = (GRCDEVICE.IsReadOnlyDepthAllowed()) ? CRenderTargets::GetDepthBuffer_ReadOnly() : CRenderTargets::GetDepthBufferCopy();
	#else
		grcRenderTarget *depthRT = CRenderTargets::GetDepthBufferCopy();
	#endif
	grcTextureFactory::GetInstance().LockRenderTarget(0, fbRT, depthRT);
	ms_RenderTargetActive = true;
#else
	LockSceneRenderTargets();
#endif
}

void CRenderTargets::UnlockSceneRenderTargets()
{
	RestoreHiStencil restore;

	Assertf(ms_RenderTargetActive, "Render target release request when not active");

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

	ms_RenderTargetActive = false;
}

void CRenderTargets::LockUIRenderTargets()
{
	//not sure if required on any other platforms
	#if RSG_DURANGO || RSG_ORBIS || RSG_PC || RSG_PS3
		grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetUIBackBuffer(),	CRenderTargets::GetUIDepthBuffer(true));
	#endif
}

void CRenderTargets::UnlockUIRenderTargets(bool realize)
{	
	#if	RSG_XENON
	(void)realize;
	#endif

	#if RSG_DURANGO || RSG_ORBIS || RSG_PC || RSG_PS3
		grcResolveFlags resolveFlags;
		resolveFlags.NeedResolve = realize;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	#endif
}

#if __D3D11
void CRenderTargets::LockUIRenderTargetsReadOnlyDepth()
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, CRenderTargets::GetUIBackBuffer(),	CRenderTargets::GetDepthBufferResolved_ReadOnly());
}
#endif // __D3D11

grcRenderTarget* CRenderTargets::CreateRenderTarget(RTMemPoolID memPoolID, const char *name, grcRenderTargetType type, int width, int height, int bitsPerPixel, rage::grcTextureFactory::CreateParams *_params, u8 poolHeap, grcRenderTarget* originalTarget)
{
	grcTextureFactory::CreateParams params;
	if (_params)
		params = *_params;

	u16 poolId = (memPoolID == RTMEMPOOL_NONE ? kRTPoolIDInvalid : GetRenderTargetPoolID(memPoolID));

	// don't want unpooled render targets, force kRTPoolIDAuto if needed	
	if (poolId == kRTPoolIDInvalid
		XENON_ONLY(&& params.IsResolvable == true)) // if it's not resolvable we don't care
	{
		poolId = kRTPoolIDAuto;
	}


	params.PoolID = poolId;
	params.PoolHeap = poolHeap;

	 // do not allocate from pool straight away unless no pool is specified
	params.AllocateFromPoolOnCreate = (params.PoolID==kRTPoolIDAuto ? true : false);

	grcRenderTarget *rt = grcTextureFactory::GetInstance().CreateRenderTarget(name, type, width, height, bitsPerPixel, &params, originalTarget);
	Assert(rt);
	return(rt);
}

grcRenderTarget* CRenderTargets::CreateRenderTarget(const char* name, grcTextureObject* sourceTexture, grcRenderTarget* originalTexture)
{
	// Actually just create another render target that points at another one...
	grcRenderTarget *rt = grcTextureFactory::GetInstance().CreateRenderTarget(name, sourceTexture, originalTexture);
	Assert(rt);
	return(rt);
}

void CRenderTargets::FreeRenderTargetMemPool()
{
	for (s32 i=0; i<RTMEMPOOL_SIZE; i++)
	{
		if(ms_RenderTargetMemPoolArray[i])
			grcRenderTargetPoolMgr::DestroyPool( ms_RenderTargetMemPoolArray[i] );

		ms_RenderTargetMemPoolArray[i] = kRTPoolIDInvalid;
	}
}


void CRenderTargets::DownsampleDepth()
{
#if __ASSERT
	Assertf(g_CachedDownsampleDepthThreadID == -1 || ((int)g_RenderThreadIndex) == g_CachedDownsampleDepthThreadID, "Different Sub render threads are trying to Downsample Depth. Caching state logic should be updated. Cached Thread ID = %d, Current Thread ID = %u", g_CachedDownsampleDepthThreadID, g_RenderThreadIndex);
	g_CachedDownsampleDepthThreadID = (int)g_RenderThreadIndex;
#endif //__ASSERT
	if(!ms_DepthDownsampleReady)
	{
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
		grcCompositeRenderTargetHelper::DownsampleParams downsampleParams;
		downsampleParams.srcDepth = MSAA_ONLY(GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() :) CRenderTargets::GetDepthBuffer();
		downsampleParams.dstDepth = m_DepthTargetQuarter;
		PS3_ONLY(downsampleParams.srcPatchedDepth = CRenderTargets::GetDepthBufferAsColor();)

		grcCompositeRenderTargetHelper::DownsampleDepth(downsampleParams);
		ms_DepthDownsampleReady = true;
	}
}


void CRenderTargets::ResetRenderThreadInfo()
{
	SetDownsampleDepthFlag(false);
	ASSERT_ONLY(g_CachedDownsampleDepthThreadID = -1;)
}
//////////////////////////////////////////////////////////////////////////
#if __BANK
//////////////////////////////////////////////////////////////////////////
static atArray<const char*> s_RenderTargetNames;
static atArray<const char*> s_RenderTargetMemPoolNames;
static bkCombo* s_RenderTargetNamesCombo = NULL;
static int      s_RenderTargetCount = 0;
static int      s_RenderTargetIndex = 0;
static int      s_RenderTargetPoolIndex = 0;
static int      s_RenderTargetPoolCount = 0;
static Vector4  s_RenderTargetPos(200.0f, 200.0f, 1.0f, 200.0f); // xy = pos, z = aspect, w = size
static Vector4  s_RenderTargetPosAndSize; // xy=pos, zw=size (final coords)
static Vector4  s_RenderTargetUv(0.0f, 0.0f, 1.0f, 0.0f);
static int      s_RenderTargetMip = 0;
static float    s_RenderTargetBrightness = 1.0f;
static float    s_RenderTargetOpacity = 1.0f;
static float    s_RenderTargetDepthNearClip = 0.5f;
static float    s_RenderTargetDepthFarClip = 4000.0f;
static float    s_RenderTargetDepthNearClipGBuffer = 0.5f; // display-only
static float    s_RenderTargetDepthFarClipGBuffer = 4000.0f; // display-only
static float    s_RenderTargetDepthMin = 0.0f;
static float    s_RenderTargetDepthMax = 4000.0f;
static bool     s_bRenderTargetFullScreen = false;
static bool     s_bRenderTargetShowInfo = true;
static bool     s_bRenderTargetVisualise = false;
static bool     s_bRenderTargetRelativeAspect = true;
static bool     s_bRenderTargetNativeRes = false;
static bool     s_bRenderTargetFlipHoriz = false; // for MIRROR_RT
static bool     s_bRenderTargetWaterDraw = false; // water draw overlay
static Vector4  s_RenderTargetBounds = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
static bool     s_bRenderTargetCentre = true;
static bool     s_bRenderTargetRed = true;
static bool     s_bRenderTargetGreen = true;
static bool     s_bRenderTargetBlue = true;
static bool     s_bRenderTargetAlpha = false;
# if DEVICE_EQAA
static int		s_bRenderTargetAAMode = 0;
static int		s_bRenderTargetSampleIndex = 0;
static const char *s_RenderTargetAAModeNames[] =
{
	"Default",
	"AA Sample Id",
	"AA Fragment Id",
	"AA FMASK",
	"Resolve EQAA",
};
# endif //DEVICE_EQAA

bool CRenderTargets::IsShowingRenderTarget()
{
	return s_RenderTargetIndex > 0;
}

bool CRenderTargets::IsShowingRenderTarget(const char* name)
{
	if (s_RenderTargetIndex < 0 || s_RenderTargetIndex >= s_RenderTargetNames.GetCount())
	{
		return false;
	}

	if (s_RenderTargetNames[s_RenderTargetIndex] == NULL)
	{
		return false;
	}

	return stricmp(s_RenderTargetNames[s_RenderTargetIndex], name) == 0;
}

bool CRenderTargets::IsShowingRenderTargetWaterDraw()
{
	return s_bRenderTargetWaterDraw;
}

Vec2V_Out CRenderTargets::ShowRenderTargetGetWindowMin()
{
	return Vec2V(s_RenderTargetPosAndSize.x, s_RenderTargetPosAndSize.y);
}

Vec2V_Out CRenderTargets::ShowRenderTargetGetWindowMax()
{
	return Vec2V(s_RenderTargetPosAndSize.x + s_RenderTargetPosAndSize.z, s_RenderTargetPosAndSize.y + s_RenderTargetPosAndSize.w);
}

void CRenderTargets::ShowRenderTarget(const char* name)
{
	s_RenderTargetIndex = 0;

	for (int i = 0; i < s_RenderTargetNames.size(); i++)
	{
		if (stricmp(s_RenderTargetNames[i], name) == 0)
		{
			s_RenderTargetIndex = i;
			break;
		}
	}
}

void CRenderTargets::ShowRenderTargetOpacity(float opacity)
{
	s_RenderTargetOpacity = opacity;
}

void CRenderTargets::ShowRenderTargetBrightness(float brightness)
{
	s_RenderTargetBrightness = brightness;
}

void CRenderTargets::ShowRenderTargetNativeRes(bool nativeRes, bool flipHoriz, bool waterDraw)
{
	s_bRenderTargetNativeRes = nativeRes;
	s_bRenderTargetFlipHoriz = flipHoriz;
	s_bRenderTargetWaterDraw = waterDraw;
}

void CRenderTargets::ShowRenderTargetSetBounds(float x, float y, float w, float h)
{
	s_RenderTargetBounds = Vector4(x, y, w, h);
}

void CRenderTargets::ShowRenderTargetFullscreen(bool fullscreen)
{
	s_bRenderTargetFullScreen = fullscreen;
}

int& CRenderTargets::ShowRenderTargetGetMipRef()
{
	return s_RenderTargetMip;
}

void CRenderTargets::ShowRenderTargetInfo(bool show)
{
	s_bRenderTargetShowInfo = show;
}

void CRenderTargets::ShowRenderTargetReset()
{
	s_RenderTargetIndex       = 0;
	s_RenderTargetOpacity     = 1.0f;
	s_RenderTargetBrightness  = 1.0f;
	s_bRenderTargetNativeRes  = false;
	s_bRenderTargetFlipHoriz  = false;
	s_bRenderTargetWaterDraw  = false;
	s_RenderTargetBounds      = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	s_bRenderTargetFullScreen = false;
	s_bRenderTargetShowInfo   = true;
	s_bRenderTargetVisualise  = false;
}

void CRenderTargets::DisplayRenderTargetOverlaps()
{
#if __PS3 || __XENON
	Displayf("\nRender target overlaps (not counting MIP maps): [PoolID, HeapID]\n");

	for (int nIndexA = 0; nIndexA < grcTextureFactory::GetRenderTargetCount(); ++nIndexA)
	{
		grcRenderTarget* pTargetA = grcTextureFactory::GetRenderTarget(nIndexA);

		// If TargetA isn't resolvable then we don't care about overlaps
		XENON_ONLY( if (static_cast<rage::grcRenderTargetXenon*>(pTargetA)->GetTexturePtr() == NULL) continue );

		bool bNoOverlap = true;
		u32 nStartAddressA = PS3_SWITCH( static_cast<grcRenderTargetGCM*>(pTargetA)->GetMemoryOffset(), 0 );
		u32 nSizeA =  PS3_SWITCH( pTargetA->GetPhysicalSize(), 0 );
		XENON_ONLY( XGGetTextureLayout( static_cast<grcRenderTargetXenon*>(pTargetA)->GetTexturePtr(), &nStartAddressA, &nSizeA, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0 ) );
		u32 nEndAddressA = nStartAddressA + nSizeA;


		Displayf("[%d,%d] \"%s\" (0x%x - 0x%x) overlaps with:", pTargetA->GetPoolID(), pTargetA->GetPoolHeap(), pTargetA->GetName(), nStartAddressA, nEndAddressA);
		for (int nIndexB = 0; nIndexB < grcTextureFactory::GetRenderTargetCount(); ++nIndexB)
		{
			if ( nIndexA == nIndexB )
				continue;

			grcRenderTarget* pTargetB = grcTextureFactory::GetRenderTarget(nIndexB);

			// If TargetB isn't resolvable then we don't care about overlaps
			XENON_ONLY( if (static_cast<rage::grcRenderTargetXenon*>(pTargetB)->GetTexturePtr() == NULL) continue );

			u32 nStartAddressB = PS3_SWITCH( static_cast<grcRenderTargetGCM*>(pTargetB)->GetMemoryOffset(), 0 );
			u32 nSizeB = PS3_SWITCH( pTargetB->GetPhysicalSize(), 0 );
			XENON_ONLY( XGGetTextureLayout( static_cast<grcRenderTargetXenon*>(pTargetB)->GetTexturePtr(), &nStartAddressB, &nSizeB, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0 ) );
			u32 nEndAddressB = nStartAddressB + nSizeB;
			
			// If there's an overlap
			if ( ( (nStartAddressA >= nStartAddressB) && (nStartAddressA < nEndAddressB) ) ||
			     ( (nEndAddressA > nStartAddressB)    && (nEndAddressA <= nEndAddressB)  ) ||
			     ( (nStartAddressA <= nStartAddressB) && (nEndAddressA > nStartAddressB) ) )
			{
				bNoOverlap = false;
				Displayf("\t[%d,%d] \"%s\" (0x%x - 0x%x)", pTargetB->GetPoolID(), pTargetB->GetPoolHeap(), pTargetB->GetName(), nStartAddressB, nEndAddressB);
			}
		}

		Displayf( bNoOverlap ? "\tNONE!\n" : "");
	}
#endif // __PS3 || __XENON
}

void CRenderTargets::DumpRenderTargets()
{
#if !__WIN32PC && !__NO_OUTPUT && !RSG_DURANGO
	DisplayRenderTargetDetailsHeader();
	for (int rt = 0; rt != grcTextureFactory::GetRenderTargetCount(); ++rt)
	{
		DisplayRenderTargetDetails(grcTextureFactory::GetRenderTarget(rt));
	}
#endif
}

void CRenderTargets::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Render Targets");

	VideoResManager::AddWidgets(bank);

	grcRenderTarget::AddLoggingWidgets(bank);

	bank.AddButton("Dump RenderTargets",DumpRenderTargets);
	bank.AddButton("Dump RenderTarget Overlaps",DisplayRenderTargetOverlaps);

	s_RenderTargetMemPoolNames.Grow() = "";
	for (u16 pool = 0; pool != grcRenderTargetPoolMgr::GetMemPoolCount(); ++pool)
	{
		s_RenderTargetMemPoolNames.Grow() = grcRenderTargetPoolMgr::GetName(pool);
	}

	bank.AddCombo("Render Target Pool", &s_RenderTargetPoolIndex, s_RenderTargetMemPoolNames.GetCount(), &s_RenderTargetMemPoolNames[0], NullCB, "Lists the render target pool");

	s_RenderTargetNames.Reset();
	s_RenderTargetNames.Reserve(grcTextureFactory::GetRenderTargetCount());
	s_RenderTargetNames.Grow() = "";
	for (int rt = 0; rt != grcTextureFactory::GetRenderTargetCount(); ++rt)
	{
		s_RenderTargetNames.Grow() = grcTextureFactory::GetRenderTarget(rt)->GetName();
	}
	s_RenderTargetCount = grcTextureFactory::GetRenderTargetCount()+1;
	s_RenderTargetPoolCount = grcRenderTargetPoolMgr::GetMemPoolCount()+1;

	const float maxZoom = 32.0f;
	const float screenSize = maxZoom * float( Max<int>(VideoResManager::GetNativeWidth(), VideoResManager::GetNativeHeight()) );

	s_RenderTargetNamesCombo = bank.AddCombo("Render Target", &s_RenderTargetIndex, s_RenderTargetNames.GetCount(), &s_RenderTargetNames[0], NullCB, "Lists the render targets");
#if DEVICE_EQAA
	bank.AddCombo("MSAA Mode", &s_bRenderTargetAAMode, NELEM(s_RenderTargetAAModeNames), s_RenderTargetAAModeNames, NullCB, "Switch MSAA surface access mode");
	bank.AddSlider("Sample/Fragment Index", &s_bRenderTargetSampleIndex, 0, 15, 1 );
#endif
	bank.AddToggle("Show Info", &s_bRenderTargetShowInfo);
	bank.AddToggle("Visualise", &s_bRenderTargetVisualise);
	bank.AddSeparator();
	bank.AddToggle("Fullscreen", &s_bRenderTargetFullScreen);
	bank.AddToggle("Native Resolution", &s_bRenderTargetNativeRes);
	bank.AddToggle("Flip Horizontal", &s_bRenderTargetFlipHoriz);
	bank.AddToggle("Water Draw Overlay", &s_bRenderTargetWaterDraw);
	bank.AddToggle("Relative Aspect", &s_bRenderTargetRelativeAspect);
	bank.AddToggle("Centre", &s_bRenderTargetCentre);
	bank.AddSlider("Mip Level", &s_RenderTargetMip, 0, 12, 1);
	bank.AddSeparator();
	bank.AddSlider("Pos X", &s_RenderTargetPos.x, -screenSize, screenSize, 1.f);
	bank.AddSlider("Pos Y", &s_RenderTargetPos.y, -screenSize, screenSize, 1.f);
	bank.AddSlider("Size", &s_RenderTargetPos.w, 0.0f, screenSize, 1.f);
	bank.AddSlider("Aspect", &s_RenderTargetPos.z, 0.0f, 8.0f, 0.01f);
	bank.AddSeparator();
	bank.AddSlider("U", &s_RenderTargetUv.x, 0.f, 100.0f, 0.01f);
	bank.AddSlider("V", &s_RenderTargetUv.y, 0.f, 100.0f, 0.01f);
	bank.AddSlider("UV Size", &s_RenderTargetUv.z, 0.0f, 100.0f, 0.01f);
	bank.AddSeparator();
	bank.AddSlider("Brightness", &s_RenderTargetBrightness, 0.0f, 16.0f, 0.01f);
	bank.AddSlider("Opacity", &s_RenderTargetOpacity, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("Depth Near Clip", &s_RenderTargetDepthNearClip, 0.0f, 16000.0f, 0.001f);
	bank.AddSlider("Depth Far Clip", &s_RenderTargetDepthFarClip, 0.0f, 16000.0f, 1.0f);
	bank.AddText("Depth Near Clip GBuffer", &s_RenderTargetDepthNearClipGBuffer);
	bank.AddText("Depth Far Clip GBuffer", &s_RenderTargetDepthFarClipGBuffer);
	bank.AddSlider("Depth Min", &s_RenderTargetDepthMin, 0.0f, 16000.0f, 0.001f);
	bank.AddSlider("Depth Max", &s_RenderTargetDepthMax, 0.0f, 16000.0f, 0.001f);
	bank.AddSeparator();
	bank.AddToggle("Show Red", &s_bRenderTargetRed);
	bank.AddToggle("Show Green", &s_bRenderTargetGreen);
	bank.AddToggle("Show Blue", &s_bRenderTargetBlue);
	bank.AddToggle("Show Alpha", &s_bRenderTargetAlpha);

	bank.PopGroup();

	// Also create debug rendering state blocks.
	grcBlendStateDesc BSDesc;
	BSDesc.BlendRTDesc[0].BlendEnable = true;
	BSDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
#if !__WIN32 && !RSG_ORBIS
	BSDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_CONSTANTALPHA;
	BSDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVCONSTANTALPHA;
	BSDesc.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
	BSDesc.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_CONSTANTALPHA;
	BSDesc.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVCONSTANTALPHA;
#endif // !__WIN32
	ms_BlitTransparent_BS = grcStateBlock::CreateBlendState(BSDesc);
}

void CRenderTargets::UpdateBank(const bool forceLoad)
{
	// For B*1923551 - Rare crash in CRenderTargets::UpdateBank(). List size count becomes stale during function execution.
	// It`s an array over-run. Been caught in debugger once, loop count is one higher than the rendertarget list size. Can only conclude
	// a target was unregistered after grcTextureFactory::GetRenderTargetCount() is called.
	sysCriticalSection cs(grcTextureFactory::GetRenderTargetListCritSectionToken());

#if !__WIN32PC && !RSG_DURANGO && !RSG_ORBIS
	static bool bCheckedPoolSizes = true;

	if (bCheckedPoolSizes)
	{
		bCheckedPoolSizes = false;
		CheckPoolSizes();
	}
#endif // !__WIN32PC && !RSG_DURANGO && !RSG_ORBIS

	// repopulate rendertargets
	if (s_RenderTargetCount != grcTextureFactory::GetRenderTargetCount()+1 || forceLoad)
	{
		s_RenderTargetCount = grcTextureFactory::GetRenderTargetCount()+1;

		s_RenderTargetNames.Reset();
		s_RenderTargetNames.Reserve(s_RenderTargetCount);
		for (int i=0; i!=s_RenderTargetCount; ++i)
		{
			s_RenderTargetNames.Grow() = NULL;
		}

		if (s_RenderTargetNamesCombo)
		{
			s_RenderTargetNamesCombo->UpdateCombo("Render target", &s_RenderTargetIndex, s_RenderTargetNames.GetCount(), &s_RenderTargetNames[0], NullCB, "Lists the render targets");
		}

		s_RenderTargetNames.Reset();
		s_RenderTargetNames.Reserve(s_RenderTargetCount);
		s_RenderTargetNames.Grow() = "";
		for (int rt = 0; rt != s_RenderTargetCount-1; ++rt)
		{
			s_RenderTargetNames.Grow() = grcTextureFactory::GetRenderTarget(rt)->GetName();
		}

		if (s_RenderTargetNamesCombo)
		{
			for (int i=0; i!=s_RenderTargetCount; ++i)
				s_RenderTargetNamesCombo->SetString(i, s_RenderTargetNames[i]);
		}
	}

	if (s_RenderTargetIndex && s_RenderTargetIndex < s_RenderTargetCount)
	{
		DisplayRenderTarget(s_RenderTargetIndex - 1);
	}

	if (s_RenderTargetPoolIndex && s_RenderTargetPoolIndex  < s_RenderTargetPoolCount)
	{
		DisplayRenderTargetMemPool(u16(s_RenderTargetPoolIndex-1));
	}

	const grcViewport* vp = gVpMan.GetUpdateGameGrcViewport();

	if (vp)
	{
		s_RenderTargetDepthNearClipGBuffer = vp->GetNearClip();
		s_RenderTargetDepthFarClipGBuffer = vp->GetFarClip();
	}
}

// currently mode 0 = standard, and mode 1 = water
// if we end up needing more modes, i'll change this to an enum
void CRenderTargets::RenderBank(int mode)
{
	if (s_RenderTargetIndex)
	{
#		if __XENON
			grcRenderTarget *rt = grcTextureFactory::GetRenderTarget(s_RenderTargetIndex - 1);
			if (IsShowingRenderTargetWaterDraw() && strcmp(s_RenderTargetNames[s_RenderTargetIndex], "WATER_REFLECTION_COLOUR") == 0)
			{
				const grcRenderTarget* waterTarget = Water::GetReflectionRT();

				if (CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterReflectionTechnique_Update())
				{
					waterTarget = CMirrors::GetMirrorWaterRenderTarget();
				}

				for (int i = 1; i < s_RenderTargetNames.GetCount(); i++)
				{
					grcRenderTarget* rt2 = grcTextureFactory::GetRenderTarget(i - 1);

					if (rt2 == waterTarget)
					{
						rt = rt2;
						break;
					}
				}
			}
#		else
			grcRenderTarget *const rt = grcTextureFactory::GetRenderTarget(s_RenderTargetIndex - 1);
#		endif

		float multiplierWidth = (rt->GetType() == grcrtCubeMap) ? 4.0f : 1.0f;
		float multiplierHeight = (rt->GetType() == grcrtCubeMap) ? 2.0f : 1.0f;

		if (s_bRenderTargetFullScreen)
		{
			s_RenderTargetPosAndSize.x = 0.0f;
			s_RenderTargetPosAndSize.y = 0.0f;
			s_RenderTargetPosAndSize.z = (float)VideoResManager::GetNativeWidth();
			s_RenderTargetPosAndSize.w = (float)VideoResManager::GetNativeHeight();
		}
		else
		{
			if (s_bRenderTargetNativeRes)
			{
				s_RenderTargetPosAndSize.z = (float)rt->GetWidth() * multiplierWidth;
				s_RenderTargetPosAndSize.w = (float)rt->GetHeight() * multiplierHeight;
			}
			else
			{
				s_RenderTargetPosAndSize.z = s_RenderTargetPos.w*s_RenderTargetPos.z;
				s_RenderTargetPosAndSize.w = s_RenderTargetPos.w;

				if (s_bRenderTargetRelativeAspect)
				{
					s_RenderTargetPosAndSize.z *= (float)rt->GetWidth()/(float)rt->GetHeight();
				}
			}

			if (s_bRenderTargetCentre)
			{
				s_RenderTargetPosAndSize.x = 0.5f*((float)VideoResManager::GetNativeWidth()  - s_RenderTargetPosAndSize.z);
				s_RenderTargetPosAndSize.y = 0.5f*((float)VideoResManager::GetNativeHeight() - s_RenderTargetPosAndSize.w);
			}else
			{
				s_RenderTargetPosAndSize.x = s_RenderTargetPos.x;
				s_RenderTargetPosAndSize.y = s_RenderTargetPos.y;
			}
		}

		if (s_RenderTargetBounds.z != 0.0f &&
			s_RenderTargetBounds.w != 0.0f)
		{
			s_RenderTargetPosAndSize = s_RenderTargetBounds;
		}

		if (s_bRenderTargetFlipHoriz)
		{
			s_RenderTargetPosAndSize.x += s_RenderTargetPosAndSize.z;
			s_RenderTargetPosAndSize.z *= -1.0f;
		}

		BlitRenderTarget(
			rt, 
			s_RenderTargetPosAndSize,
			s_RenderTargetOpacity,
			mode == 1);
	}
}

#if __PPU
namespace rage {
	extern u32 g_AllowTexturePatch;
};
#endif // __PPU

void CRenderTargets::BlitRenderTarget(grcRenderTarget* pRT, const Vector4 &posAndSize, float opacity, bool bUseDepth)
{
	if(!pRT || opacity <= 0.0f)
		return;
	
#if __PPU
	int src_format = -1;
	g_AllowTexturePatch = 0xFFFFFFFF;
#endif // __PPU

	CSprite2d::CustomShaderType type = CSprite2d::CS_BLIT;

	switch(pRT->GetType())
	{
		case grcrtDepthBuffer:
#if __PPU
		case grcrtShadowMap:
		{
			u8 format = gcm::StripTextureFormat(((grcRenderTargetGCM *)pRT)->GetGcmTexture().format);
			bool isDepth =	(format == CELL_GCM_TEXTURE_DEPTH24_D8) || (format == CELL_GCM_TEXTURE_DEPTH24_D8_FLOAT);
			if( isDepth )
			{	
				src_format = GRCDEVICE.PatchShadowToDepthBuffer(pRT, false);
				type = CSprite2d::CS_BLIT_DEPTH;
			}
			else
			{
				type = (s_bRenderTargetAlpha) ? CSprite2d::CS_BLIT_SHOW_ALPHA : CSprite2d::CS_BLIT_NOFILTERING;
			}
		}
#else
# if DEVICE_MSAA
			if (pRT->GetMSAA())
				type = strstr(pRT->GetName(),"Stencil") ? CSprite2d::CS_BLIT_MSAA_STENCIL : CSprite2d::CS_BLIT_MSAA_DEPTH;
			else
# endif // DEVICE_MSAA
				type = strstr(pRT->GetName(),"Stencil") ? CSprite2d::CS_BLIT_STENCIL : CSprite2d::CS_BLIT_DEPTH;
#endif // __PPU
		break;

		case grcrtCubeMap:
		{
			type = CSprite2d::CS_BLIT_CUBE_MAP;
		}
		break;

		default:
		{
#if DEVICE_MSAA
			if (pRT->GetMSAA())
				type = CSprite2d::CS_BLIT_MSAA;
			else
#endif // DEVICE_MSAA
			if (pRT->GetArraySize() > 1)
				type = CSprite2d::CS_BLIT_TEX_ARRAY;
			else
				type = (s_bRenderTargetAlpha) ? CSprite2d::CS_BLIT_SHOW_ALPHA : CSprite2d::CS_BLIT_NOFILTERING;
		}
		break;
	}

	CSprite2d spriteBlit;
	spriteBlit.SetTexture(pRT);

	Vector4 params0(s_RenderTargetBrightness * s_bRenderTargetRed,
				s_RenderTargetBrightness * s_bRenderTargetGreen,
				s_RenderTargetBrightness * s_bRenderTargetBlue,
				s_bRenderTargetAlpha * 1.0f);

	Vector4 params1((float)s_RenderTargetMip, 0.0f, 0.0f, 0.0f);

#if DEVICE_EQAA
	const grcRenderTargetMSAA *const rtAA = static_cast<grcRenderTargetMSAA*>(pRT);
	if (s_bRenderTargetAAMode && rtAA->GetResolveFragmentMask())
	{
		type = CSprite2d::CS_BLIT_FMASK;
		const CoverageData &coverage = rtAA->GetCoverageData();
		const grcDevice::MSAAMode mode = (coverage.donor ? coverage.donor : rtAA)->GetMSAA();
		params1.x = (float)( mode.m_uSamples );
		params1.y = (float)( mode.m_uFragments );
		params1.z = mode.GetFmaskShift();
		params1.w = (float)((s_bRenderTargetAAMode<<8) + s_bRenderTargetSampleIndex);
	}
#endif	//DEVICE_EQAA

	if (type == CSprite2d::CS_BLIT_DEPTH)
	{
		params1.x = s_RenderTargetDepthMin;
		params1.y = s_RenderTargetDepthMax;

		// see CalculateProjectionParams
		const float n = s_RenderTargetDepthNearClip;
		const float f = s_RenderTargetDepthFarClip;
		const float Q = f/(f - n);
		params1.z = XENON_SWITCH(n*Q, -n*Q);
		params1.w = XENON_SWITCH(Q - 1.0f, -Q);
	}

	spriteBlit.SetRenderState();
	spriteBlit.SetGeneralParams(params0, params1);

	// adjustment for displaying multiple render targets 
	static float g_x_start = -0.125f;
	static float g_y_start = 0.4f;
	static float g_x_off = 0.325f;
	static float g_y_off = -0.6f;
	static const int NUM_RT_DISPLAY_ROW = 4;

	

	const float deviceWidth = float(  VideoResManager::GetNativeWidth() );
	const float deviceHeight = float( VideoResManager::GetNativeHeight() );

	const float normX = posAndSize.x / deviceWidth;
	const float normWidth = posAndSize.z / deviceWidth;
	const float normY = posAndSize.y / deviceHeight;
	const float normHeight = posAndSize.w / deviceHeight;
	
	const int rtArraySize = type == CSprite2d::CS_BLIT_TEX_ARRAY ? pRT->GetArraySize() : 1;
	const float x1 = normX * 2.0f - 1.0f + g_x_start * (int)(Min(rtArraySize,NUM_RT_DISPLAY_ROW) - 1);
	const float y1 = -(normY * 2.0f - 1.0f) + g_y_start * (int)(rtArraySize / (NUM_RT_DISPLAY_ROW+1));
	const float x2 = (normX + normWidth) * 2.0f - 1.0f + g_x_start * (int)(Min(rtArraySize,NUM_RT_DISPLAY_ROW) - 1);
	const float y2 = -((normY + normHeight) * 2.0f - 1.0f) + g_y_start * (int)(rtArraySize / (NUM_RT_DISPLAY_ROW+1));;

	const bool bTransparent = (opacity < 1.0f);

	if (bTransparent)
	{
		Color32 col(0,0,0,(int)(opacity*255.0f + 0.5f));

		grcStateBlock::SetBlendState(ms_BlitTransparent_BS,col.GetColor(),~0ULL);
	}
	else
	{
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	}
	grcStateBlock::SetDepthStencilState(bUseDepth ? grcStateBlock::DSS_LessEqual : grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	for (int i = 0; i < rtArraySize; i++)
	{
		if (type == CSprite2d::CS_BLIT_TEX_ARRAY)
		{
			params1.w = (float)i;
			spriteBlit.SetGeneralParams(params0, params1);
		}
		spriteBlit.BeginCustomList(type, pRT);
		grcDrawSingleQuadf(
			x1+g_x_off*(int)(i%NUM_RT_DISPLAY_ROW), y1+g_y_off*(int)(i/NUM_RT_DISPLAY_ROW),
			x2+g_x_off*(int)(i%NUM_RT_DISPLAY_ROW), y2+g_y_off*(int)(i/NUM_RT_DISPLAY_ROW),
			(__XENON && !bUseDepth) ? 1.0f : 0.0f,
			s_RenderTargetUv.x,
			s_RenderTargetUv.y,
			s_RenderTargetUv.x + s_RenderTargetUv.z,
			s_RenderTargetUv.y + s_RenderTargetUv.z,
			Color32(255, 255, 255, 255));
		spriteBlit.EndCustomList();
	}

	spriteBlit.SetTexture( static_cast<grcTexture*>(NULL) );

#if __PPU
	if (src_format != -1)
	{
		GRCDEVICE.PatchDepthToShadowBuffer(pRT, src_format, false);
	}
	g_AllowTexturePatch = 0;
#endif // __PPU
}

int GetMaxUsedSize(u16 poolID)
{
	u32 max = 0;
	u8 heapCount = grcRenderTargetPoolMgr::GetHeapCount(poolID);
	for (u8 i = 0; i < heapCount; ++i) {
		u32 val = grcRenderTargetPoolMgr::GetUsedMem(poolID,i);
		if (val > max)
			max = val;
	}
	return max;
}

int GetMinFreeSize(u16 poolID)
{
	u32 min = grcRenderTargetPoolMgr::GetAllocated(poolID);
	u8 heapCount = grcRenderTargetPoolMgr::GetHeapCount(poolID);

	for (u8 i = 0; i < heapCount; ++i) {
		u32 val = grcRenderTargetPoolMgr::GetFreeMem(poolID,i);
		if (val < min)
			min = val;
	}
	return min;
}

int GetMaxFreeSize(u16 poolID)
{
	u32 max = 0;
	u8 heapCount = grcRenderTargetPoolMgr::GetHeapCount(poolID);

	for (u8 i = 0; i < heapCount; ++i) {
		u32 val = grcRenderTargetPoolMgr::GetFreeMem(poolID,i);
		if (val > max)
			max = val;
	}
	return max;
}

Vec2V pixelToScreen(u32 x, u32 y) { return (Vec2V((float)x, (float)y) / Vec2V(1280.0f, 720.0f)); }

void CRenderTargets::DisplayRenderTargetMemPool(u16 index)
{
	if (s_RenderTargetPoolIndex > 0 && s_bRenderTargetVisualise)
	{
		u32 startX = 50, startY = 50;
		u32 sizeX = 1180, sizeY = 620;

		u32 heapOffset = (u32)ceilf(sizeX / float(grcRenderTargetPoolMgr::GetHeapCount(index)));

		grcDebugDraw::RectAxisAligned(
			pixelToScreen(startX, startY),
			pixelToScreen(startX + sizeX, startY + sizeY),
			Color32(0, 0, 0, 128),
			true);

		u32 poolStartAddress = UINT_MAX;
		u32 poolEndAddress = 0;

		for (int rt = 0; rt != grcTextureFactory::GetRenderTargetCount(); ++rt)
		{
			#if __PS3
				grcRenderTarget* pTarget = grcTextureFactory::GetRenderTarget(rt);
				((rage::grcRenderTargetGCM*)pTarget)->LockSurface(0, 0);
			#elif __XENON
				grcRenderTarget* pTarget = grcTextureFactory::GetRenderTarget(rt);
				((rage::grcRenderTargetXenon*)pTarget)->LockSurface(0, 0);
			#endif
		}

		for (int h = 0; h < grcRenderTargetPoolMgr::GetHeapCount(index); ++h)
		{
			for (int rt = 0; rt != grcTextureFactory::GetRenderTargetCount(); ++rt)
			{
				grcRenderTarget* pTarget = grcTextureFactory::GetRenderTarget(rt);

				if (pTarget && pTarget->GetPoolID() == index && pTarget->GetPoolHeap() == h)
				{
					u32 startAddress = PS3_SWITCH( static_cast<grcRenderTargetGCM*>(pTarget)->GetMemoryOffset(), 0 );
					u32 size =  PS3_SWITCH( pTarget->GetPhysicalSize(), 0 );
#if __XENON					
					if( static_cast<grcRenderTargetXenon*>(pTarget)->GetTexturePtr() )
					{
						XGGetTextureLayout( static_cast<grcRenderTargetXenon*>(pTarget)->GetTexturePtr(), &startAddress, &size, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0 );
					}
#endif
					u32 endAddress = startAddress + size;

					poolStartAddress = Min<u32>(poolStartAddress, startAddress);
					poolEndAddress = Max<u32>(poolEndAddress, endAddress);
				}
			}
		}

		for (int h = 0; h < grcRenderTargetPoolMgr::GetHeapCount(index); ++h)
		{
			if (h < grcRenderTargetPoolMgr::GetHeapCount(index) - 1)
			{
				grcDebugDraw::Line(
					pixelToScreen(startX + (h + 1) * heapOffset, startY + 0),
					pixelToScreen(startX + (h + 1) * heapOffset, startY + sizeY),
					Color32(255, 0, 0, 128),
					true);
			}

			for (int rt = 0; rt != grcTextureFactory::GetRenderTargetCount(); ++rt)
			{
				grcRenderTarget* pTarget = grcTextureFactory::GetRenderTarget(rt);
				
				if (pTarget->GetPoolHeap() == h && pTarget->GetPoolID() == index)
				{
					u32 startAddress = PS3_SWITCH( static_cast<grcRenderTargetGCM*>(pTarget)->GetMemoryOffset(), 0 );
					u32 size =  PS3_SWITCH( pTarget->GetPhysicalSize(), 0 );
#if __XENON					
					if( static_cast<grcRenderTargetXenon*>(pTarget)->GetTexturePtr() )
					{
						XGGetTextureLayout( static_cast<grcRenderTargetXenon*>(pTarget)->GetTexturePtr(), &startAddress, &size, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0 );
					}
#endif

					u32 endAddress = startAddress + size;

					int heapId = pTarget->GetPoolHeap();

					u32 startOffset = u32(((startAddress - poolStartAddress) / float(poolEndAddress - poolStartAddress)) * sizeY);
					u32 endOffset   = u32(((endAddress   - poolStartAddress) / float(poolEndAddress - poolStartAddress)) * sizeY);

					grcDebugDraw::RectAxisAligned(
						pixelToScreen(startX + (heapId + 0) * heapOffset, startY + startOffset),
						pixelToScreen(startX + (heapId + 1) * heapOffset, startY + endOffset),
						Color32(255, 0, 0, 32),
						true);

					grcDebugDraw::Line(
						pixelToScreen(startX + (heapId + 0) * heapOffset, startY + startOffset),
						pixelToScreen(startX + (heapId + 1) * heapOffset, startY + startOffset),
						Color32(255, 0, 0, 128),
						true);

					grcDebugDraw::Line(
						pixelToScreen(startX + (heapId + 0) * heapOffset, startY + endOffset),
						pixelToScreen(startX + (heapId + 1) * heapOffset, startY + endOffset),
						Color32(255, 0, 0, 128),
						true);

					if (endOffset - startOffset >= 20)
					{
						grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());
							grcDebugDraw::Text(
								pixelToScreen(startX + (heapId + 0) * heapOffset + 5, startY + startOffset + 5),
								Color32(255, 255, 255, 255),
								pTarget->GetName());
						grcDebugDraw::TextFontPop();
					}
				}
			}
		}
	}

	if (!s_bRenderTargetShowInfo)
		return;

	grcDebugDraw::AddDebugOutput("%s: allocated %d used %d free %d worst %d waste %d", 
			grcRenderTargetPoolMgr::GetName(index),
			grcRenderTargetPoolMgr::GetAllocated(index), 
			GetMaxUsedSize(index), 
			GetMinFreeSize(index), 
			grcRenderTargetPoolMgr::GetLowestFreeMem(index),
			GetMaxFreeSize(index)
		);

#if __PPU
	grcDebugDraw::AddDebugOutput("pitch %d heaps %d location %s compression %s", 
			grcRenderTargetPoolMgr::GetMemoryPoolPitch(index), 
			grcRenderTargetPoolMgr::GetHeapCount(index), 
			grcRenderTargetPoolMgr::GetIsPhysical(index)?"LOCAL":"MAIN", 
			grcRenderTargetPoolMgr::GetCompression(index)?s_CompressionMode[grcRenderTargetPoolMgr::GetCompression(index)-7]:"CELL_GCM_COMPMODE_DISABLED"
			);
	int unused = grcRenderTargetGCM::GetBitFieldTiledMemory().CountBits(false);
	if (unused)
		grcDebugDraw::AddDebugOutput(Color32(255, 0,0,255), "%d unused tiling regions", unused);
#endif

	int heapCount = grcRenderTargetPoolMgr::GetHeapCount(index);

	for (int h = 0; h != heapCount; ++h)
	{
		for (int rt = 0; rt != grcTextureFactory::GetRenderTargetCount(); ++rt)
		{
			grcRenderTarget * renderTarget = grcTextureFactory::GetRenderTarget(rt);
			if (renderTarget && renderTarget->GetPoolID() == index && renderTarget->GetPoolHeap() == h)
			{
#if __PPU
				grcRenderTargetGCM* rt = (grcRenderTargetGCM*)renderTarget;
				u32 pitch = rt->GetPitch();
				size_t size = gcm::GetSharedSurfaceSize(
					rt->GetWidth(), rt->GetHeight(), 
					rt->GetBitsPerPix(), 
					rt->GetMipMapCount(), 
					rt->GetLayerCount(), 
					rt->IsInLocalMemory(), rt->IsTiled(), rt->IsSwizzled(), rt->GetType() == grcrtCubeMap, rt->IsUsingZCull(), 1, NULL);
#endif // __PPU

				grcDebugDraw::AddDebugOutput("heap %d - %s  width %d height %d mips %d " PPU_ONLY("pitch %d/%d bipp %d%s %dKb"),
					renderTarget->GetPoolHeap(),
					renderTarget->GetName(), 
					renderTarget->GetWidth(),
					renderTarget->GetHeight(),
					renderTarget->GetMipMapCount()
#if __PPU
					,rt->GetPitch()
					,rt->IsTiled() ? (	gcm::GetSurfaceTiledPitch(rt->GetWidth(), rt->GetBitsPerPix(), rt->IsSwizzled())) : 
					(	gcm::GetSurfacePitch	 (rt->GetWidth(), rt->GetBitsPerPix(), rt->IsSwizzled()))
					,rt->GetBitsPerPix()
					,rt->IsTiled()?" TILED":""
					,size >> 10
#endif // __PPU
					); 
			}
		}
	}
}

void CRenderTargets::DisplayRenderTarget(u32 index)
{
	grcRenderTarget * renderTarget = grcTextureFactory::GetRenderTarget(index);

	if (renderTarget && s_bRenderTargetShowInfo)
	{
#if __PPU
		grcRenderTargetGCM* rt = (grcRenderTargetGCM*)renderTarget;
		u32 pitch = rt->GetPitch();
		size_t size = gcm::GetSharedSurfaceSize(
			rt->GetWidth(), rt->GetHeight(), 
			rt->GetBitsPerPix(), 
			rt->GetMipMapCount(),
			rt->GetLayerCount(), 
			rt->IsInLocalMemory(), rt->IsTiled(), rt->IsSwizzled(), rt->GetType() == grcrtCubeMap, rt->IsUsingZCull(), 1, NULL);
#endif // _PSN


		grcDebugDraw::AddDebugOutput(" %s  width %d height %d mips %d " PPU_ONLY("pitch %d/%d bipp %d%s %dKb"),
			renderTarget->GetName(), 
			renderTarget->GetWidth(),
			renderTarget->GetHeight(),
			renderTarget->GetMipMapCount()
#if __PPU
			,rt->GetPitch()
			,rt->IsTiled() ? (	gcm::GetSurfaceTiledPitch(rt->GetWidth(), rt->GetBitsPerPix(), rt->IsSwizzled())) : 
							 (	gcm::GetSurfacePitch	 (rt->GetWidth(), rt->GetBitsPerPix(), rt->IsSwizzled()))
			,rt->GetBitsPerPix()
			,rt->IsTiled()?" TILED":""
			,size >> 10
#endif // __PPU
			); 

#if __PPU
		if (rt->IsInMemoryPool())
		{		
			grcDebugDraw::AddDebugOutput(" -> %s (%d)", grcRenderTargetPoolMgr::GetName(renderTarget->GetPoolID()), renderTarget->GetPoolHeap());
		}
		else
			grcDebugDraw::AddDebugOutput(" -> NOT POOLED");
#endif // __PPU
	}
}

void CRenderTargets::CheckPoolSizes()
{
	int RtCount = grcTextureFactory::GetRenderTargetCount();

	for (int i = 0; i != RTMEMPOOL_SIZE; ++i)
	{
		u16 poolID = ms_RenderTargetMemPoolArray[i];

		if (poolID == 65535) // necessary for running with USE_D3D_DEBUG_LIBS
		{
			continue;
		}

		bool bNotWasting = FALSE;
		for(int j=0; j<grcRenderTargetPoolMgr::GetHeapCount(poolID); j++)
		{
			int used = grcRenderTargetPoolMgr::GetUsedMem(poolID,(u8)j);

			u32 memUsed = used;
#if __PS3
			u32 pitch = grcRenderTargetPoolMgr::GetMemoryPoolPitch(poolID);
			Assertf( ( used % pitch ) == 0, "used (%d) %% pitch (%d) = %d", used, pitch, used % pitch );

			// now align this height according to the memory region
			u32 linesUsed = used/pitch;
			u32 linesNeeded = rage::gcm::GetSurfaceHeight( linesUsed, grcRenderTargetPoolMgr::GetIsPhysical(poolID) );
			memUsed = linesNeeded*pitch;
#endif // __PS3			

			Assertf(memUsed <= ( u32 )grcRenderTargetPoolMgr::GetAllocated(poolID), "%s uses %d/%d",grcRenderTargetPoolMgr::GetName(poolID),memUsed / (1024*64),( u32 )grcRenderTargetPoolMgr::GetAllocated(poolID)/ (1024*64));			
			u32 memFree = grcRenderTargetPoolMgr::GetAllocated(poolID) - memUsed;

			// we're allowed to waste up to 64k in order to align the tile region size
			const u32 maxWastage = KB64;
			
			bNotWasting = bNotWasting || (memFree < maxWastage);
			if(!bNotWasting)
			{
				Displayf("grcRenderTargetMemPool '%s': heap %d, memFree: %d KB (%d pages)", grcRenderTargetPoolMgr::GetName(poolID), j, memFree/1024, memFree / (64*1024));
				for(int k=0;k<RtCount;k++)
				{
					grcRenderTarget * renderTarget = grcTextureFactory::GetRenderTarget(k);
					
					if(renderTarget && renderTarget->GetPoolID() == poolID && renderTarget->GetPoolHeap() == j)
					{
						Displayf("                              %s",renderTarget->GetName());
					}
				}
			}
		}

#if __PS3
		u32 maxPitch = 0;
		for(int j=0;j<RtCount;j++)
		{
			grcRenderTarget * renderTarget = grcTextureFactory::GetRenderTarget(j);
			
			if(renderTarget && renderTarget->GetPoolID() == poolID )
			{
				const u32 width = renderTarget->GetWidth();
				const u32 bpp = ((grcRenderTargetGCM*)renderTarget)->GetBitsPerPixel();
				const bool swizzled = renderTarget->IsSwizzled();
				const bool isTiled = ((grcRenderTargetGCM*)renderTarget)->IsTiled();
				u32 rtPitch =  isTiled ? gcm::GetSurfaceTiledPitch(width, bpp, swizzled) : gcm::GetSurfacePitch(width, bpp, swizzled);
				maxPitch = Max(maxPitch,rtPitch);
			}
		}

		Assertf(maxPitch == grcRenderTargetPoolMgr::GetMemoryPoolPitch(poolID),"grcRenderTargetMemPool '%s' has a pitch of %d while the biggest required pitch is %d",grcRenderTargetPoolMgr::GetName(poolID),grcRenderTargetPoolMgr::GetMemoryPoolPitch(poolID),maxPitch);
#endif // __PS3

		Assertf(bNotWasting==true, "grcRenderTargetMemPool '%s' is wasting memory on all heaps",grcRenderTargetPoolMgr::GetName(poolID));
		if(!bNotWasting)
		{
			Displayf("grcRenderTargetMemPool '%s' is wasting memory on all heaps",grcRenderTargetPoolMgr::GetName(poolID));
#if __PS3
			Displayf("grcRenderTargetMemPool '%s' Pitch %d",grcRenderTargetPoolMgr::GetName(poolID), grcRenderTargetPoolMgr::GetMemoryPoolPitch(poolID));
#endif			
			for(int j=0;j<RtCount;j++)
			{
				grcRenderTarget * renderTarget = grcTextureFactory::GetRenderTarget(j);
				
				if(renderTarget && renderTarget->GetPoolID() == poolID )
				{
#if __PPU
					const u32 width = renderTarget->GetWidth();
					const u32 bpp = renderTarget->GetBitsPerPixel();
					const bool swizzled = renderTarget->IsSwizzled();
					const bool isTiled = ((grcRenderTargetGCM*)renderTarget)->IsTiled();
					u32 rtPitch =  isTiled ? gcm::GetSurfaceTiledPitch(width, bpp, swizzled) : gcm::GetSurfacePitch(width, bpp, swizzled);
					Displayf("RT %s Pitch %d",renderTarget->GetName(),rtPitch);
#else
					Displayf("RT %s",renderTarget->GetName());
#endif					
					
				}
		}

			
		}
	}
		
}
//////////////////////////////////////////////////////////////////////////
#endif // __BANK 
//////////////////////////////////////////////////////////////////////////

#if  __PS3
void CRenderTargets::CopyDepthBuffer( grcRenderTarget* srcDepthbuffer, grcRenderTarget* dstDepthBuffer, u8 stencilRef )
{

	PF_PUSH_TIMEBAR_DETAIL("Copy Depth");

	CSprite2d copyDepth;
	copyDepth.SetDepthTexture( srcDepthbuffer );
	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, dstDepthBuffer);

	grcBlendStateHandle BS_Backup = grcStateBlock::BS_Default_WriteMaskNone;
	grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
	grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default_WriteMaskNone);
	grcStateBlock::SetDepthStencilState(ms_CopyDepthClearStencil_DS, stencilRef);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	copyDepth.BeginCustomList(CSprite2d::CS_COPYDEPTH, NULL);
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255,255,255,255));
	copyDepth.EndCustomList();

	grcStateBlock::SetStates(RS_Backup, DSS_Backup, BS_Backup);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	PF_POP_TIMEBAR_DETAIL();
}
#endif // __PS3

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS

void CRenderTargets::PreAllocateRenderTargetMemPool(void)
{
}

#endif

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
#if DEBUG_TRACK_MSAA_RESOLVES
bool CRenderTargets::IsBackBufferResolved()
{
#if DEVICE_MSAA
	if (ms_BackBuffer->GetMSAA()>1)
	{
		return static_cast<grcRenderTargetMSAA*>(ms_BackBuffer)->HasBeenResolvedTo( ms_BackBufferCopy );
	}else
#endif
	{
		return true;
	}
}
#endif

void CRenderTargets::CopyBackBuffer()
{
	PIX_TAGGING_ONLY( PIXBegin( 0x1, "Copying BackBuffer."); )
#if DEVICE_MSAA
	if (ms_BackBuffer->GetMSAA()>1)
	{
#if RSG_PC
		if(PostFX::GetPauseResolve() == PostFX::IN_RESOLVE_PAUSE)		//	resolved earlier...
			ms_BackBufferCopy->Copy( ms_BackBuffer );
		else
#endif
			static_cast<grcRenderTargetMSAA*>(ms_BackBuffer)->Resolve( ms_BackBufferCopy );
	}
	else
#endif
	{
		ms_BackBufferCopy->Copy( ms_BackBuffer );
	}
	PIX_TAGGING_ONLY( PIXEnd(); )
}
#endif	//RSG_PC || RSG_DURANGO || RSG_ORBIS

void CRenderTargets::CopyDepthBuffer( grcRenderTarget* srcDepthBuffer, grcRenderTarget* dstDepthBuffer )
{
#if RSG_PC
	// no copy is done if the depth copy is just and RO view into the depth
	if (srcDepthBuffer->GetTexturePtr() == dstDepthBuffer->GetTexturePtr())
		return;
#endif // RSG_PC

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __D3D11

#if D3D11_RENDER_TARGET_COPY_OPTIMISATION_TEST
	grcRenderTargetDX11 *srcD3D11 = static_cast<grcRenderTargetDX11*>(srcDepthBuffer);
	bool needsCopying = srcD3D11->HasBeenWrittenTo();
	Assert(needsCopying);
	if( !needsCopying )
		return;

	srcD3D11->SetHasBeenWrittenTo(false);
#endif

	PIX_TAGGING_ONLY( PIXBegin( 0x1, "Copying DepthBuffer."); )
	if( GRCDEVICE.GetDxFeatureLevel() > 1000 )
	{
#if RSG_DURANGO
		//On Durango it actually appears to be faster to use a pixel shader to copy the target
		CopyDepthManual(srcDepthBuffer, dstDepthBuffer);
#elif RSG_PC
		if (srcDepthBuffer->GetWidth() != dstDepthBuffer->GetWidth() || srcDepthBuffer->GetHeight() != dstDepthBuffer->GetHeight())
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, dstDepthBuffer);

			PUSH_DEFAULT_SCREEN();

			grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_ForceDepth, grcStateBlock::BS_Default);

			CSprite2d sprite;
			sprite.SetDepthTexture(srcDepthBuffer);
			sprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			sprite.BeginCustomList(CSprite2d::CS_COPYDEPTH, NULL);
			grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color_black);
			sprite.EndCustomList();

			grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

			POP_DEFAULT_SCREEN();

			grcTextureFactory::GetInstance().UnlockRenderTarget(0, NULL);
		}
		else
			dstDepthBuffer->Copy( srcDepthBuffer );
#else
		dstDepthBuffer->Copy( srcDepthBuffer );
#endif
	}
	else
	{
		//Need to make sure we call this in the correct place to set everything up correctly.
		//On DX10 we have to do a full screen pass to manually copy the depth
		CopyDepthManual(srcDepthBuffer, dstDepthBuffer);
	}
	PIX_TAGGING_ONLY( PIXEnd(); )

#elif RSG_ORBIS
	PIX_TAGGING_ONLY( PIXBegin( 0x1, "Copying DepthBuffer."); )
	dstDepthBuffer->Copy( srcDepthBuffer );
	PIX_TAGGING_ONLY( PIXEnd(); )
#endif
}

#if RSG_PC || RSG_DURANGO
#if RSG_PC && __D3D11
grcRenderTarget* CRenderTargets::GetBackBufferCopy(bool bDoCopy/*=false*/)	
{ 
	if(bDoCopy) CopyBackBuffer(); 
#if RSG_PC
	if (PostFX::GetPauseResolve() == PostFX::IN_RESOLVE_PAUSE)
		return ms_BackBufferRTForPause;
	else
#endif
	return ms_BackBufferCopy; 
}

void CRenderTargets::ResolveForMultiGPU(int bResolve)
{
	if (!AssertVerify(GRCDEVICE.UsingMultipleGPUs() && ms_BackBufferTexForPause))
	{
		return;
	}
	
	static int g_GPUCounter = 0;

	if (bResolve == (int)PostFX::RESOLVE_PAUSE)
	{
		/*
		 * Copy the current backbuffer to staging texture and create rendertarget to be used by postfx (necessary for multi GPU)
		 */
		grcRenderTarget *pSrc = (CRenderTargets::GetBackBufferCopy(true));

		ms_BackBufferTexForPause->Copy((grcTexture*)pSrc);
		((grcTextureDX11*)ms_BackBufferTexForPause)->UpdateCPUCopy();

		g_GPUCounter = GRCDEVICE.GetGPUCount(true);
	}
	else if (bResolve == (int)PostFX::IN_RESOLVE_PAUSE)
	{
		if (g_GPUCounter > 0)
		{
			// Update GPU copy of backbuffer tex for the current GPU index
			((grcTextureDX11*)ms_BackBufferTexForPause)->UpdateGPUCopyFromBackingStore();

			g_GPUCounter--;
		}
	}
	else if (bResolve == (int)PostFX::UNRESOLVE_PAUSE)
	{
		// we used to de-allocate targets here
	}
}
#endif

void CRenderTargets::CopyTarget(grcRenderTarget *src, grcRenderTarget *dst, grcRenderTarget *dstStencil, u8 stencilRef)
{
	if (dstStencil == NULL || stencilRef >= 0xFF)
	{
		PF_PUSH_TIMEBAR_DETAIL("CopyTarget");

	#if RSG_PC || RSG_DURANGO // Not sure if Orbis supports grcRenderTarget->Copy()
		dst->Copy(src);
	#else
		CSprite2d copyTarget;

		grcTextureFactory::GetInstance().LockRenderTarget(0, dst, NULL);

		grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
		grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
		grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

		copyTarget.BeginCustomList(CSprite2d::CS_BLIT_NOFILTERING, src);
		grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255,255,255,255));
		copyTarget.EndCustomList();

		grcStateBlock::SetStates(RS_Backup, DSS_Backup, BS_Backup);

		grcResolveFlags resolveFlags;
		resolveFlags.NeedResolve = false;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	#endif
		PF_POP_TIMEBAR_DETAIL();
	}
	else
	{
		PF_PUSH_TIMEBAR_DETAIL("CopyTarget StencilRef");

		grcTextureFactory::GetInstance().LockRenderTarget(0, dst, dstStencil);

		grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
		grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
		grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
		grcStateBlock::SetDepthStencilState(ms_CopyColorStencilMask_DS, stencilRef);

		CSprite2d copyTarget;
		copyTarget.BeginCustomList(CSprite2d::CS_BLIT_NOFILTERING, src);
		grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255,255,255,255));
		copyTarget.EndCustomList();

		grcStateBlock::SetStates(RS_Backup, DSS_Backup, BS_Backup);

		grcResolveFlags resolveFlags;
		resolveFlags.NeedResolve = false;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

		PF_POP_TIMEBAR_DETAIL();
	}
}

#endif //RSG_PC || RSG_DURANGO

void CRenderTargets::CopyDepthManual(grcRenderTarget *srcDepthbuffer, grcRenderTarget *dstDepthBuffer)
{
	PF_PUSH_TIMEBAR_DETAIL("Copy Depth");
	CSprite2d copyDepth;
	copyDepth.SetDepthTexture( srcDepthbuffer );
	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, dstDepthBuffer);

	grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
	grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
	grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(ms_CopyDepth_DS);

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	copyDepth.BeginCustomList(CSprite2d::CS_COPYDEPTH, NULL);

	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255,255,255,255));
	copyDepth.EndCustomList();

	grcStateBlock::SetStates(RS_Backup, DSS_Backup, BS_Backup);

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	PF_POP_TIMEBAR_DETAIL();
}

#if __D3D11

//returns depth buffer to use as shader resource.
grcRenderTarget* CRenderTargets::LockReadOnlyDepth( bool needsCopy, bool bLockBothDepthStencilRO, grcRenderTarget* depthAndStencilBuffer, grcRenderTarget* depthBufferCopy, grcRenderTarget* depthAndStencilBufferReadOnly)
{
	grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance());  
	grcRenderTarget* depthBufferReturn = NULL;
	//On Dx11 we can use a read only depth buffer when depth writes are turned off
	//and use the main depth buffer as a shader resource
	if( GRCDEVICE.IsReadOnlyDepthAllowed() )
	{ 
		if ( bLockBothDepthStencilRO)
			textureFactory11.LockDepthAndCurrentColorTarget( depthAndStencilBufferReadOnly );
		else
			textureFactory11.LockDepthAndCurrentColorTarget( depthBufferCopy );
		depthBufferReturn = GetDepthBuffer();
	}
	//On DX10/10.1 we need to copy the depth buffer and use the copy.
	else
	{
		if( needsCopy )
		{
			CopyDepthBuffer( depthAndStencilBuffer, depthBufferCopy);
		}
		depthBufferReturn = depthBufferCopy;
	}
	return depthBufferReturn;
}

void CRenderTargets::UnlockReadOnlyDepth()
{
	grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance());  
	if( GRCDEVICE.IsReadOnlyDepthAllowed() )
	{
		grcResolveFlags resolveFlags;
		resolveFlags.MipMap = false;
		textureFactory11.UnlockDepthAndCurrentColorTarget(&resolveFlags);
	}
}

#endif //__D3D11

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS

grcTexture* CRenderTargets::GetReplayThumbnailTexture(u32 idx)
{
	grcAssert(idx < REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE);
	return ms_ReplayThumbnailTex[idx];
}

grcRenderTarget* CRenderTargets::GetReplayThumbnailRT(u32 idx)
{
	grcAssert(idx < REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE);
	return ms_ReplayThumbnailRT[idx];
}

#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY

//////////////////////////////////////////////////////////////////////////
// VideoResManager
#if RSG_PC
int VideoResManager::m_scalingMode;
#endif

int VideoResManager::m_nativeWidth;
int VideoResManager::m_nativeHeight;
int VideoResManager::m_gameWidth;
int VideoResManager::m_gameHeight;
int VideoResManager::m_UIWidth;
int VideoResManager::m_UIHeight;

//////////////////////////////////////////////////////////////////////////
//
VideoResManager::eDownscaleFactor VideoResManager::GetDownscaleFactor( u32 const rawDownscale )
{
	VideoResManager::eDownscaleFactor result = DOWNSCALE_SIXTEENTH;

	switch( rawDownscale )
	{
	case 1:
		{
			result = DOWNSCALE_ONE;
			break;
		}
	case 2:
		{
			result = DOWNSCALE_HALF;
			break;
		}
	case 4:
		{
			result = DOWNSCALE_QUARTER;
			break;
		}
	case 8:
		{
			result = DOWNSCALE_EIGHTH;
			break;
		}
	default:
		{
			// default to 16th, as set earlier.
			break;
		}
	}

	return result;
}


//////////////////////////////////////////////////////////////////////////
//
#if RSG_PC

void VideoResManager::GetSceneScale(int scalingMode, int& scaleNumerator, int& scaleDenominator)
{
	switch (scalingMode)
	{
	default:
	case 0: // x 1.000
		scaleNumerator = 1;
		scaleDenominator = 1;
		break;
	case 1: // x 0.500
		scaleNumerator = 1;
		scaleDenominator = 2;
		break;
	case 2: // x 0.6667
		scaleNumerator = 2;
		scaleDenominator = 3;
		break;
	case 3: // x 0.750
		scaleNumerator = 3;
		scaleDenominator = 4;
		break;
	case 4: // x 0.833
		scaleNumerator = 5;
		scaleDenominator = 6;
		break;
	case 5: // x 1.250
		scaleNumerator = 5;
		scaleDenominator = 4;
		break;
	case 6: // x 1.500
		scaleNumerator = 3;
		scaleDenominator = 2;
		break;
	case 7: // x 1.750
		scaleNumerator = 7;
		scaleDenominator = 4;
		break;
	case 8: // x 2.0
		scaleNumerator = 2;
		scaleDenominator = 1;
		break;
	case 9: // x 2.5
		scaleNumerator = 5;
		scaleDenominator = 2;
		break;
	}
}

#endif

void VideoResManager::Init() 
{
	m_nativeWidth	= GRCDEVICE.GetWidth();
	m_nativeHeight	= GRCDEVICE.GetHeight();
#if RSG_PC
	int scaleNumerator = 1;
	int scaleDenominator = 1;
	GetSceneScale(m_scalingMode,scaleNumerator, scaleDenominator);

	m_gameWidth		= PSN_BUFFER_WIDTH*scaleNumerator/scaleDenominator;
	m_gameHeight	= PSN_BUFFER_HEIGHT*scaleNumerator/scaleDenominator;
#else
	m_gameWidth		= PSN_BUFFER_WIDTH;
	m_gameHeight	= PSN_BUFFER_HEIGHT;
#endif
	m_UIWidth		= PSN_BUFFER_WIDTH;
	m_UIHeight		= PSN_BUFFER_HEIGHT;

	// override if defined
	PARAM_gamewidth.Get(m_gameWidth);
	PARAM_gameheight.Get(m_gameHeight);

#if RSG_PC
	DeviceReset();
#endif // RSG_PC
}

//////////////////////////////////////////////////////////////////////////
//
void VideoResManager::Shutdown()
{

}

#if RSG_PC
//////////////////////////////////////////////////////////////////////////
//
void VideoResManager::DeviceLost()
{

}

void VideoResManager::DeviceReset()
{
	int scaleNumerator = 1;
	int scaleDenominator = 1;
	GetSceneScale(m_scalingMode,scaleNumerator, scaleDenominator);

	m_gameWidth = GRCDEVICE.GetWidth()*scaleNumerator/scaleDenominator;
	m_gameHeight = GRCDEVICE.GetHeight()*scaleNumerator/scaleDenominator;

	m_nativeWidth = GRCDEVICE.GetWidth();
	m_nativeHeight = GRCDEVICE.GetHeight();
	m_UIWidth = PSN_BUFFER_WIDTH;
	m_UIHeight = PSN_BUFFER_HEIGHT;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void VideoResManager::AddWidgets(bkBank &bank) 
{
	bank.PushGroup("Video Resolution Manager", false);
		bank.AddSlider("Native Width", &m_nativeWidth, m_nativeWidth, m_nativeWidth, 0);
		bank.AddSlider("Native Height", &m_nativeHeight, m_nativeHeight, m_nativeHeight, 0);
		bank.AddSeparator();
		bank.AddSlider("Game Width", &m_gameWidth, m_gameWidth, m_gameWidth, 0);
		bank.AddSlider("Game Height", &m_gameHeight, m_gameHeight, m_gameHeight, 0);
	bank.PopGroup();
}
#endif // __BANK 

