// =============================================================================================== //
// INCLUDES
// =============================================================================================== //


// rage
#include "system/param.h"
#include "grmodel/shader.h"
#include "grmodel/shaderfx.h"
#include "grcore/allocscope.h"
#include "grcore/quads.h"
#include "system/memory.h"
#include "system/bootmgr.h"
#include "grcore/AA_shared.h"

// game
#include "renderer/deferred/gbuffer.h"
#include "renderer/rendertargets.h"
#include "renderer/deferred/deferredlighting.h"
#include "renderer/renderphases/renderphaseentityselect.h"
#include "renderer/sprite2d.h"
#include "shaders/shaderlib.h"
#include "system/xtl.h"

#if __XENON
#include "grcore/texturexenon.h"
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#undef DBG // Make sure this is the last include or there may be compilation issues
#endif // __XENON

#if RSG_ORBIS
#include "grcore/texture_gnm.h"
#include "grcore/rendertarget_gnm.h"
#endif


// =============================================================================================== //
// DEFINES
// =============================================================================================== //

enum RestoreDepthPass_e
{
	RESTORE_DEPTH_NORMAL_PASS = 0,
	RESTORE_DEPTH_3_BIT_STENCIL_PASS,
	RESTORE_DEPTH_SKIP_PIXEL_SHADER,
	RESTORE_DEPTH_NUM_PASSES
};

extern grcRenderTarget* m_OffscreenBuffers[4];

s32 GBuffer::m_numTargets = -1;
s32 GBuffer::m_BitsPerPixel = -1;
int GBuffer::m_Width = 0;
int GBuffer::m_Height = 0;
u32 GBuffer::m_Attached;
bool DECLARE_MTR_THREAD GBuffer::m_IsRenderingTo = false;

grcRenderTarget* GBuffer::m_Target[MAX_MULTIRENDER_RENDER_TARGETS] = {NULL,NULL,NULL,NULL};
#if DYNAMIC_GB
grcRenderTarget* GBuffer::m_AltTarget[MAX_MULTIRENDER_RENDER_TARGETS] = {NULL,NULL,NULL,NULL};
#endif
#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
grcRenderTarget* GBuffer::m_CopyOfTarget[MAX_MULTIRENDER_RENDER_TARGETS];
#endif

#if GBUFFER_MSAA
grcDevice::MSAAMode GBuffer::m_multisample = grcDevice::MSAA_None;
#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
grcRenderTarget* GBuffer::m_CopyOfMSAATarget[MAX_MULTIRENDER_RENDER_TARGETS];
#endif
#endif // MSAA_ENABLE
#if RSG_ORBIS
static bool s_bFastClearSupport = true;
#endif // RSG_ORBIS

#if __XENON 
grcRenderTarget* GBuffer::m_Target_Tiled[MAX_MULTIRENDER_RENDER_TARGETS] = {NULL,NULL,NULL,NULL};
grcRenderTarget* GBuffer::m_DepthTarget_Tiled = NULL;
grcRenderTarget* GBuffer::m_DepthAliases[HACK_GTA4? 3:2];
grcRenderTarget* GBuffer::m_4xHiZRestore = NULL;
grcRenderTarget* GBuffer::m_4xHiZRestoreQuarter = NULL;

grcDepthStencilStateHandle GBuffer::m_copyDepth_DS;
grcRasterizerStateHandle GBuffer::m_copyDepth_R;
grcBlendStateHandle GBuffer::m_copyStencil_BS;

grcDepthStencilStateHandle GBuffer::m_restoreHiZ_DS;
grcRasterizerStateHandle GBuffer::m_restoreHiZ_R;
grcDepthStencilStateHandle GBuffer::m_MarkHiStencilState_DS;
grcDepthStencilStateHandle GBuffer::m_MarkHiStencilStateNE_DS;
#endif

#if __PPU
grcRenderTarget *GBuffer::m_DepthPatched = NULL;
#endif // __PPU

#if RSG_PC
bool GBuffer::ms_Initialized = false;
#endif

static void FreeTarget(grcRenderTarget *&target)
{
	if (target != NULL)
	{
		target->ReleaseMemoryToPool();
		target->Release();
		target = NULL;
	}
}

#if 0	//not used ATM
static void FreeTexture(grcTexture *&texture)
{
	if (texture != NULL)
	{
		texture->Release();
		texture = NULL;
	}
}
#endif

#if !__FINAL
PARAM(verticaltiling, "use vertical tiling split");
#endif

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

#if __WIN32PC
void GBuffer::DeviceLost()
{
}

void GBuffer::DeviceReset()
{
	if (ms_Initialized)
	{
		CreateTargets(MAX_MULTIRENDER_RENDER_TARGETS, VideoResManager::GetSceneWidth(), VideoResManager::GetSceneHeight(), 32);
	}
}
#endif

void GBuffer::Init()
{
	CreateTargets(MAX_MULTIRENDER_RENDER_TARGETS, VideoResManager::GetSceneWidth(), VideoResManager::GetSceneHeight(), 32);

	#if __XENON
	// State blocks
	grcDepthStencilStateDesc copyDepthStencilState;
	copyDepthStencilState.DepthEnable = FALSE;
	copyDepthStencilState.DepthWriteMask = FALSE;
	m_copyDepth_DS = grcStateBlock::CreateDepthStencilState(copyDepthStencilState);

	grcRasterizerStateDesc copyDepthRasterizerState;
	copyDepthRasterizerState.CullMode = grcRSV::CULL_NONE;
	m_copyDepth_R = grcStateBlock::CreateRasterizerState(copyDepthRasterizerState);
		
	grcBlendStateDesc copyStencilBlendState;
	copyStencilBlendState.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RED;
	m_copyStencil_BS = grcStateBlock::CreateBlendState(copyStencilBlendState);
	
	grcDepthStencilStateDesc restoreHiZDepthStencilState;
	restoreHiZDepthStencilState.DepthEnable = TRUE;
	restoreHiZDepthStencilState.DepthFunc = grcRSV::CMP_NEVER;
	restoreHiZDepthStencilState.DepthWriteMask = FALSE;
	m_restoreHiZ_DS = grcStateBlock::CreateDepthStencilState(restoreHiZDepthStencilState);

	grcRasterizerStateDesc restoreHiZRasterizerState;
	restoreHiZRasterizerState.CullMode = grcRSV::CULL_NONE;
	m_restoreHiZ_R = grcStateBlock::CreateRasterizerState(restoreHiZRasterizerState);

	grcDepthStencilStateDesc dssDesc;
	dssDesc.DepthEnable=false;
	dssDesc.StencilEnable = true;
	dssDesc.StencilReadMask = DEFERRED_MATERIAL_TYPE_MASK;	// only care about materials
	
	dssDesc.FrontFace.StencilFunc=grcRSV::CMP_EQUAL;   
	dssDesc.BackFace = dssDesc.FrontFace;
	
	m_MarkHiStencilState_DS = grcStateBlock::CreateDepthStencilState(dssDesc);
	
	dssDesc.FrontFace.StencilFunc=grcRSV::CMP_NOTEQUAL;   
	dssDesc.BackFace = dssDesc.FrontFace;
	m_MarkHiStencilStateNE_DS = grcStateBlock::CreateDepthStencilState(dssDesc);
	#endif
}

grcRenderTarget* GBuffer::GetDepthTarget()
{
	return CRenderTargets::GetDepthBuffer();
}

#if __XENON || __PS3
 grcRenderTarget* GBuffer::GetDepthTargetAsColor()
{
	return CRenderTargets::GetDepthBufferAsColor();
}
#endif

// ----------------------------------------------------------------------------------------------- //

#if __XENON
void GBuffer::CreateTargets(int number, int width, int height, int bpp)
{
	RAGE_TRACK(DeferredLighting);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// Early exit
	if ((m_numTargets == number && width == m_Width && height == m_Height && m_BitsPerPixel == bpp) || number == 0)
		return;
	
	m_numTargets = number;
	m_Width = width;
	m_Height = height;
	m_BitsPerPixel = bpp;

	FreeTarget(m_Target[GBUFFER_RT_0]);
	FreeTarget(m_Target_Tiled[GBUFFER_RT_0]);
	
	FreeTarget(m_Target[GBUFFER_RT_1]);
	FreeTarget(m_Target_Tiled[GBUFFER_RT_1]);

	FreeTarget(m_Target[GBUFFER_RT_2]);
	FreeTarget(m_Target_Tiled[GBUFFER_RT_2]);

	FreeTarget(m_Target[GBUFFER_RT_3]);
	FreeTarget(m_Target_Tiled[GBUFFER_RT_3]);

	FreeTarget(m_DepthTarget_Tiled);

	grcTextureFactory::CreateParams Params;
	Params.UseFloat = true;
	Params.Multisample = 0;
	Params.HasParent = true;
	Params.Parent = NULL;
	Params.IsResolvable=true;
	Params.IsRenderable=true;
	Params.UseHierZ=true;

	grcTextureFactory::CreateParams Params_Tiled;
	Params_Tiled.UseFloat = true;
	Params_Tiled.Multisample = 1;
	Params_Tiled.HasParent = true;
	Params_Tiled.Parent = NULL;
	Params_Tiled.IsResolvable=false;
	Params_Tiled.IsRenderable=true;
	Params_Tiled.UseHierZ=true;

	//needs to be a multiple of 32
	int width_Tiled=m_Width; 
	int height_Tiled=m_Height;

#if !__FINAL
	if(PARAM_verticaltiling.Get())
	{
		int tmp= m_Width / NUM_TILES;
		
		if (tmp % 80)
		{
			tmp=(tmp - tmp % 80) + 80;
		}

		width_Tiled = tmp;			
	}
	else
#endif
	{
		int tmp= m_Height / NUM_TILES;

		if (tmp & 0x1f)
		{
			tmp = (tmp & ~0x1f) + 32;
		}

		height_Tiled=tmp;
	}
	
	Params_Tiled.Format = grctfNone;
	Params_Tiled.Parent = NULL;
	m_DepthTarget_Tiled=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_Z_TILED", grcrtDepthBuffer, width_Tiled, height_Tiled, 32, &Params_Tiled);

	// Albedo
	Params.Parent = CRenderTargets::GetDepthBuffer();
	Params.Format = grctfA8R8G8B8;
	m_Target[GBUFFER_RT_0]=CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER0, "GBUFFER_0", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, kGBuffersHeap);
	m_Target[GBUFFER_RT_0]->AllocateMemoryFromPool();

	Params_Tiled.Format = grctfA8R8G8B8;
	Params_Tiled.Parent = m_DepthTarget_Tiled;
	m_Target_Tiled[GBUFFER_RT_0]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_0_TILED", grcrtPermanent, width_Tiled, height_Tiled, m_BitsPerPixel, &Params_Tiled);

	// Normals
	Params.Parent = CRenderTargets::GetDepthBuffer();
	Params.Format = grctfA8R8G8B8;
	m_Target[GBUFFER_RT_1]=CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER1, "GBUFFER_1", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, kGBuffer1Heap);
	m_Target[GBUFFER_RT_1]->AllocateMemoryFromPool();

	Params_Tiled.Format = grctfA8R8G8B8;
	Params_Tiled.Parent = m_Target_Tiled[GBUFFER_RT_0];
	m_Target_Tiled[GBUFFER_RT_1]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_1_TILED", grcrtPermanent, width_Tiled, height_Tiled, m_BitsPerPixel, &Params_Tiled);

	// spec intensity, specular exponent 
	Params.Parent = m_Target_Tiled[GBUFFER_RT_1]; // Start non-tiled Gbuffer2 *after* Gbuffer1_Tile. See notes below for justification of this layout.
	Params.Format = grctfA8R8G8B8;
	m_Target[GBUFFER_RT_2]=CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER23, "GBUFFER_2", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, kGBuffer23Heap);
	m_Target[GBUFFER_RT_2]->AllocateMemoryFromPool();

	// ambient and emissive bounce
	Params.Parent = CRenderTargets::GetDepthBuffer();
	Params.Format = grctfA8R8G8B8;
	m_Target[GBUFFER_RT_3]=CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER23, "GBUFFER_3", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, kGBuffer23Heap);
	m_Target[GBUFFER_RT_3]->AllocateMemoryFromPool();

    // We want gbuffer3's tile to come before gbuffer2's tile in EDRAM so that it isn't overlapped with the cascaded shadow's EDRAM region.
    // The re-ordering allows us to skip gbuffer2's second tile resolve at the end of gbuffer rendering and eliminates half of the "gbuffer2 reload"
    // that we currently do for cascaded shadow reveal. After reveal, it's all resolved and will be available from that point on. All of this 
    // saves 0.25ms every frame (0.126ms for skipping Gbuffer2's second tile resolve, 0.13ms for cutting the Gbuffer2 reload in half for shadow reveal).
	// Using all this along with high-stencil to skip sky pixels during reload typically pushes the total savings closer to 0.35ms.
    //
    // This puts the tiles in EDRAM in the following order:
    //
    //               .---------------. .---------------. .---------------. .------------------------------.                   
    // 0x000 - 0x17f | Depth tile    | | Cascaded      | | WaterRef D&C  | | RainCollision  0x000 - 0x00f |
    //               |---------------| | shadows depth | `---------------' |------------------------------|
    // 0x180 - 0x2ff | Gbuffer0 tile | |               |                   | PtFxRainUpdate 0x180 - 0x27f |
    //               |---------------| |               |				   `------------------------------'
    // 0x300 - 0x47f | Gbuffer1 tile | |               |				   
    //               |---------------| | 0x000 - 0x4ff | .---------------.
    // 0x480 - 0x5ff | Gbuffer3 tile | `---------------' | Non-tiled     |
    //               |---------------|                   | Gbuffer2      | 
    // 0x600 - 0x77f | Gbuffer2 tile |                   | 0x480 - 0x74f | 
    //               `---------------'                   `---------------' 
    // 0x780 - 0x8ff                                                       
	//																	   
	//                                                                     
	//
	// Note that there's a some extra padding at the end of the Gbuffer2 tile that isn't needed for the 2nd tile (compare with where
	// the non-tiled Gbuffer2 ends). So we could be more aggressive with overlapping if room starts getting tight.

	Params_Tiled.Format = grctfA8R8G8B8;
	Params_Tiled.Parent = m_Target_Tiled[GBUFFER_RT_1];
	m_Target_Tiled[GBUFFER_RT_3]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_3_TILED", grcrtPermanent, width_Tiled, height_Tiled, m_BitsPerPixel, &Params_Tiled);

	Params_Tiled.Format = grctfA8R8G8B8;
	Params_Tiled.Parent = m_Target_Tiled[GBUFFER_RT_3];
	m_Target_Tiled[GBUFFER_RT_2]=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_2_TILED", grcrtPermanent, width_Tiled, height_Tiled, m_BitsPerPixel, &Params_Tiled);

	// HiZRestore
	grcTextureFactory::CreateParams paramsHiZRestore;
	paramsHiZRestore.HasParent = true;
	paramsHiZRestore.Parent = NULL;
	paramsHiZRestore.Multisample=4;
	paramsHiZRestore.IsRenderable=true;
	paramsHiZRestore.IsResolvable=false;
	paramsHiZRestore.UseFloat=true;
	m_4xHiZRestore=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_DEPTH_HIZ_RESTORE", grcrtDepthBuffer, m_Width/2, m_Height/2, 32, &paramsHiZRestore);

	// HiZRestore for Quarter Depth
	// Any changes to this should be based on any changes made to the Quarter Depth buffer in rendertargets.cpp
	paramsHiZRestore.Parent = grcTextureFactory::GetInstance().GetBackBuffer(false);
	paramsHiZRestore.HiZBase = static_cast<grcRenderTargetXenon*>(grcTextureFactory::GetInstance().GetBackBufferDepth(false))->GetHiZSize();
	m_4xHiZRestoreQuarter=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_DEPTH_QUARTER_HIZ_RESTORE", grcrtDepthBuffer, m_Width/4, m_Height/4, 32, &paramsHiZRestore);
}

#elif __PS3

void GBuffer::CreateTargets(int number, int width, int height, int bpp)
{
	RAGE_TRACK(DeferredLighting);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// Early exit
	if ((m_numTargets == number && width == m_Width && height == m_Height && m_BitsPerPixel == bpp) || number == 0)
		return;
		
	m_numTargets = number;
	m_Width = width;
	m_Height = height;
	m_BitsPerPixel = bpp;

	FreeTarget(m_Target[GBUFFER_RT_0]);
	FreeTarget(m_Target[GBUFFER_RT_1]);
	FreeTarget(m_Target[GBUFFER_RT_2]);
	FreeTarget(m_Target[GBUFFER_RT_3]);

	grcTextureFactory::CreateParams Params;
	Params.UseFloat = true;
	Params.Multisample = 0;
	Params.HasParent = true;
	Params.Parent = NULL;
	Params.IsResolvable = true;
	Params.IsRenderable = true;
	Params.UseHierZ = true;

#if __PPU
	Params.InTiledMemory = true;
	Params.IsSwizzled = false;
	Params.EnableCompression = false;
	Params.Multisample = 0;
	Params.CreateAABuffer = false;
	Params.Format	= grctfA8R8G8B8;

	m_Target[GBUFFER_RT_0]=CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "GBUFFER_0", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0);
	m_Target[GBUFFER_RT_1]=CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "GBUFFER_1", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0);
	m_Target[GBUFFER_RT_2]=CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "GBUFFER_2", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0);
	m_Target[GBUFFER_RT_3]=CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "GBUFFER_3", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0);
	m_Target[GBUFFER_RT_0]->AllocateMemoryFromPool();
	m_Target[GBUFFER_RT_1]->AllocateMemoryFromPool();
	m_Target[GBUFFER_RT_2]->AllocateMemoryFromPool();
	m_Target[GBUFFER_RT_3]->AllocateMemoryFromPool();
#endif	// __PPU...
}

#else // PC, DURANGO, ORBIS

void GBuffer::DeleteTargets()
{
	FreeTarget(m_Target[GBUFFER_RT_0]);
	FreeTarget(m_Target[GBUFFER_RT_1]);
	FreeTarget(m_Target[GBUFFER_RT_2]);
	FreeTarget(m_Target[GBUFFER_RT_3]);
}

void GBuffer::CreateTargets(int number, int width, int height, int bpp)
{
#if RSG_PC
	ms_Initialized = true;
#endif

	RAGE_TRACK(DeferredLighting);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// Early exit
//	if ((m_numTargets == number && width == m_Width && height == m_Height && m_BitsPerPixel == bpp) || number == 0)
//		return;

	m_numTargets = number;
	m_Width = width;
	m_Height = height;
	m_BitsPerPixel = bpp;

	grcTextureFactory::CreateParams Params;
	Params.UseFloat = true;
	grcDevice::MSAAMode multisample = GRCDEVICE.GetMSAA();
	Params.Multisample = multisample;
	Params.HasParent = true;
	Params.Parent = NULL;
	Params.IsResolvable = true;
	Params.IsRenderable = true;
	Params.UseHierZ = true;
	Params.Format	= grctfA8R8G8B8;
	Params.Lockable = 0;
	ENTITYSELECT_ONLY(Params.Lockable = __BANK && __D3D11);

#if RSG_ORBIS
	Params.EnableFastClear = s_bFastClearSupport;
#endif // RSG_ORBIS
#if DEVICE_EQAA
	Params.ForceFragmentMask = GRCDEVICE.IsEQAA();
	Params.SupersampleFrequency = multisample.m_uFragments;
#else // DEVICE_EQAA
	u32 multisampleQuality = GRCDEVICE.GetMSAAQuality();
	Params.MultisampleQuality = (u8)multisampleQuality;
#endif // DEVICE_EQAA
#if RSG_DURANGO && !DYNAMIC_GB
	Params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_GBUF;
	Params.ESRAMMaxSize = 8 * 1024 * 1024;
#endif // RSG_DURANGO
	m_Target[GBUFFER_RT_0]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_0", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0 WIN32PC_ONLY(, m_Target[GBUFFER_RT_0]));
#if !RSG_DURANGO
	m_OffscreenBuffers[1] = m_Target[GBUFFER_RT_0];
#endif // !RSG_DURANGO
		
#if DYNAMIC_GB
	grcTextureFactory::CreateParams Params0 = Params;
#endif

#if DEVICE_EQAA
	if (GBUFFER_REDUCED_COVERAGE)
	{
		// let only GBUFFER_RT_0 to have full coverage information
		Params.Multisample.DisableCoverage();
	}
	//Params.ForceHardwareResolve = !RSG_ORBIS;
	Params.ForceFragmentMask &= EQAA_DECODE_GBUFFERS;
#endif // DEVICE_EQAA

	m_Target[GBUFFER_RT_1]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_1", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0 WIN32PC_ONLY(, m_Target[GBUFFER_RT_1]));
	m_Target[GBUFFER_RT_3]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_3", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0 WIN32PC_ONLY(, m_Target[GBUFFER_RT_3]));

#if DYNAMIC_GB
	grcTextureFactory::CreateParams Params13 = Params;
#endif
#if DEVICE_EQAA
	// Unable to use color compression because of ShadowReveal pass
	Params.Multisample.DisableCoverage();
	Params.ForceFragmentMask = false;
#endif
	m_Target[GBUFFER_RT_2]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "GBUFFER_2", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &Params, 0 WIN32PC_ONLY(, m_Target[GBUFFER_RT_2]));
#if DYNAMIC_GB
	grcTextureFactory::CreateParams Params2 = Params;
	{
		grcTextureFactory::CreateParams AltParams = Params13;
		AltParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_GBUF | grcTextureFactory::TextureCreateParams::ESRPHASE_SHADOWS | grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING;
		AltParams.ESRAMMaxSize = 8 * 1024 * 1024;
		m_AltTarget[GBUFFER_RT_1]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "ALT_GBUFFER_1", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &AltParams, 0 );
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_1])->SetAltTarget(m_AltTarget[GBUFFER_RT_1]);
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_1])->SetUseAltTestFunc(GBuffer::UseAltGBufferTest1);
	}
	{
		grcTextureFactory::CreateParams AltParams = Params0;
		AltParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_GBUF;
		AltParams.ESRAMMaxSize = 127 * 64 * 1024;
		m_AltTarget[GBUFFER_RT_0]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "ALT_GBUFFER_0", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &AltParams, 0 );
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_0])->SetAltTarget(m_AltTarget[GBUFFER_RT_0]);
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_0])->SetUseAltTestFunc(GBuffer::UseAltGBufferTest0);
	}
	{
		grcTextureFactory::CreateParams AltParams = Params2;
		AltParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_GBUF | grcTextureFactory::TextureCreateParams::ESRPHASE_SHADOWS;
		AltParams.ESRAMMaxSize = 8 * 1024 * 1024;
		m_AltTarget[GBUFFER_RT_2]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "ALT_GBUFFER_2", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &AltParams, 0 );
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_2])->SetAltTarget(m_AltTarget[GBUFFER_RT_2]);
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_2])->SetUseAltTestFunc(GBuffer::UseAltGBufferTest2);
	}
	{
		grcTextureFactory::CreateParams AltParams = Params13;
		AltParams.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_GBUF | grcTextureFactory::TextureCreateParams::ESRPHASE_SHADOWS | grcTextureFactory::TextureCreateParams::ESRPHASE_LIGHTING;
		AltParams.ESRAMMaxSize = 4 * 1024 * 1024;
		m_AltTarget[GBUFFER_RT_3]	=	CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "ALT_GBUFFER_3", grcrtPermanent, m_Width, m_Height, m_BitsPerPixel, &AltParams, 0 );
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_3])->SetAltTarget(m_AltTarget[GBUFFER_RT_3]);
		static_cast<grcRenderTargetDurango*>(m_Target[GBUFFER_RT_3])->SetUseAltTestFunc(GBuffer::UseAltGBufferTest3);
	}
#endif

#if DEVICE_MSAA
	Params.SupersampleFrequency = 0;
#endif

#if DEVICE_EQAA
	Params.ForceFragmentMask = false;
	if (GRCDEVICE.IsEQAA())
	{
#if DYNAMIC_GB && 1
		grcRenderTargetMSAA *const donor = static_cast<grcRenderTargetMSAA*>( m_AltTarget[GBUFFER_RT_0] );
#else
		grcRenderTargetMSAA *const donor = static_cast<grcRenderTargetMSAA*>( m_Target[GBUFFER_RT_0] );
#endif
		static_cast<grcRenderTargetMSAA*>( CRenderTargets::GetBackBuffer() )->SetFragmentMaskDonor( donor, ResolveSW_NoAnchors );	//option: high-quality
		//static_cast<grcRenderTargetMSAA*>( grcTextureFactory::GetInstance().GetBackBuffer() )->SetFragmentMaskDonor( donor, false, true ); // Non-MSAA back buffer now
		static_cast<grcRenderTargetMSAA*>( CRenderTargets::GetOffscreenBuffer1() )->SetFragmentMaskDonor( donor, ResolveSW_HighQuality );
	}
#endif // DEVICE_EQAA
#if RSG_DURANGO
	Params.ESRAMPhase = 0;
#endif

	Assertf(CRenderTargets::GetOffscreenBuffer2(), "Haven't created offscreen buffer 2 yet but re-using it");

#	if CREATE_TARGETS_FOR_DYNAMIC_DECALS
	// We only need a copy of the normal channel.
	m_CopyOfTarget[GBUFFER_RT_0]	=	NULL;
	m_CopyOfTarget[GBUFFER_RT_1]	=	WIN32PC_ONLY(VideoResManager::IsSceneScaled() ? CRenderTargets::GetOffscreenBuffer3() :) CRenderTargets::GetOffscreenBuffer2();
	m_CopyOfTarget[GBUFFER_RT_2]	=	NULL;
	m_CopyOfTarget[GBUFFER_RT_3]	=	NULL;
#	endif

#	if GBUFFER_MSAA
	m_multisample = GRCDEVICE.GetMSAA();

	if (m_multisample)
	{
		// These are simply textures that can be set (via grmShader::SetVar()) that share the same texture as the normal targets but will have the msaa version in the srv
#		if CREATE_TARGETS_FOR_DYNAMIC_DECALS
		// We only need a copy of the normal channel.
		m_CopyOfMSAATarget[GBUFFER_RT_0] = NULL;
		m_CopyOfMSAATarget[GBUFFER_RT_1] = CRenderTargets::CreateRenderTarget("Copy of GBUFFER_1_MS", m_CopyOfTarget[GBUFFER_RT_1]->GetTexturePtr() WIN32PC_ONLY(, m_CopyOfMSAATarget[GBUFFER_RT_1]));
		m_CopyOfMSAATarget[GBUFFER_RT_2] = NULL;
		m_CopyOfMSAATarget[GBUFFER_RT_3] = NULL;
#		endif
	}
#	endif // GBUFFER_MSAA

}
#endif	//platforms

// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //

void GBuffer::LockTargets()
{
#if __XENON 
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Target_Tiled[GBUFFER_RT_0], m_DepthTarget_Tiled);
	grcTextureFactory::GetInstance().LockRenderTarget(1, m_Target_Tiled[GBUFFER_RT_1], m_DepthTarget_Tiled);
	grcTextureFactory::GetInstance().LockRenderTarget(2, m_Target_Tiled[GBUFFER_RT_2], m_DepthTarget_Tiled);
	grcTextureFactory::GetInstance().LockRenderTarget(3, m_Target_Tiled[GBUFFER_RT_3], m_DepthTarget_Tiled);
#endif //__XENON
	
#if __PPU || __D3D11 || RSG_ORBIS
	const grcRenderTarget* rendertargets[grcmrtColorCount] = {
		m_Target[GBUFFER_RT_0],
		m_Target[GBUFFER_RT_1],
		m_Target[GBUFFER_RT_2],
		NULL
	};

	// HACK - Talk to Klaas before changing this
#if 1
	rendertargets[GBUFFER_RT_3] = (grcRenderTarget*)
		(
			!!m_Attached * (u64)(GBuffer::GetDepthTarget()) + 
			!m_Attached * (u64)(m_Target[GBUFFER_RT_3])
		);
#else
	rendertargets[GBUFFER_RT_3] = m_Target[GBUFFER_RT_3];
#endif // __MASTER
	// HACK - Talk to Klaas before changing this

	grcTextureFactory::GetInstance().LockMRT(rendertargets, GBuffer::GetDepthTarget());

# if DEVICE_EQAA && RSG_DURANGO
	// Init the sample pattern
	const grcDevice::MSAAMode mode = m_Target[GBUFFER_RT_0]->GetMSAA();
	GRCDEVICE.SetAACount(mode.m_uSamples, mode.m_uFragments, mode.m_uFragments);
# endif //DEVICE_EQAA && RSG_DURANGO

#elif __WIN32PC
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Target[GBUFFER_RT_0], GBuffer::GetDepthTarget());
	grcTextureFactory::GetInstance().LockRenderTarget(1, m_Target[GBUFFER_RT_1], GBuffer::GetDepthTarget());
	grcTextureFactory::GetInstance().LockRenderTarget(2, m_Target[GBUFFER_RT_2], GBuffer::GetDepthTarget());
	grcTextureFactory::GetInstance().LockRenderTarget(3, m_Target[GBUFFER_RT_3], GBuffer::GetDepthTarget());
#endif //__PPU || __WIN32
}

// ----------------------------------------------------------------------------------------------- //

void GBuffer::LockTarget_AmbScaleMod()
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Target[GBUFFER_RT_3], NULL);
}

// ----------------------------------------------------------------------------------------------- //

void GBuffer::LockSingleTarget(GBufferRT index, bool bLockDepth)
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Target[index], bLockDepth? GBuffer::GetDepthTarget() : NULL);
}

// ----------------------------------------------------------------------------------------------- //

void GBuffer::UnlockTargets()
{
	#if __XENON 
	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve=false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(3,&resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(2,&resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1,&resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0,&resolveFlags);
	#elif __PPU || __D3D11 || RSG_ORBIS
	grcTextureFactory::GetInstance().UnlockMRT();
	#else
	grcTextureFactory::GetInstance().UnlockRenderTarget(3);
	if (m_Target[GBUFFER_RT_2])
	{
		grcTextureFactory::GetInstance().UnlockRenderTarget(2);
	}
	grcTextureFactory::GetInstance().UnlockRenderTarget(1);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	#endif
}

// ----------------------------------------------------------------------------------------------- //

void GBuffer::UnlockTarget_AmbScaleMod(const bool XENON_ONLY(resolve))
{
	grcResolveFlags resolveFlags;

	#if __XENON
	resolveFlags.NeedResolve=resolve;
	resolveFlags.ClearDepthStencil=false;
	resolveFlags.ClearColor=true;
	resolveFlags.Color=Color32(0,0,0,0);
	#elif (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	resolveFlags.NeedResolve=false;
	#endif	// __XENON,(RSG_PC || RSG_DURANGO || RSG_ORBIS)

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
}

// ----------------------------------------------------------------------------------------------- //

void GBuffer::UnlockSingleTarget(GBufferRT)
{
	grcResolveFlags resolveFlags;

	#if __XENON || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	resolveFlags.NeedResolve=(RSG_PC || RSG_DURANGO || RSG_ORBIS) ? false : true;
	resolveFlags.ClearDepthStencil=false;
	resolveFlags.ClearColor=true;
	resolveFlags.Color=Color32(0,0,0,0);
	#endif // __XENON

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
}

// ----------------------------------------------------------------------------------------------- //
#if __XENON
// ----------------------------------------------------------------------------------------------- //
ASSERT_ONLY( static int g_MarkingStencilRef=-1;)

void GBuffer::RestoreHiZ(bool bUseDepthQuarter)
{
	PF_PUSH_TIMEBAR_DETAIL("Restore HiZ");
	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, bUseDepthQuarter?GBuffer::Get4xHiZRestoreQuarter():GBuffer::Get4xHiZRestore());

	D3DVIEWPORT9 Viewport;
	GRCDEVICE.GetCurrent()->GetViewport( &Viewport );
	Viewport.MinZ = 1.0f;
	Viewport.MaxZ = 0.0f;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_restoreHiZ_DS);
	grcStateBlock::SetRasterizerState(m_restoreHiZ_R);

	GRCDEVICE.GetCurrent()->SetViewport( &Viewport );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, TRUE );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC,D3DHSCMP_EQUAL );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, DEFERRED_MATERIAL_CLEAR);
	CSprite2d restoreHiZ;

	restoreHiZ.BeginCustomList(CSprite2d::CS_RESTORE_HIZ, NULL);
	grcDrawSingleQuadf(0,0,1,1,0,0,0,1,1,Color32(255,255,255,255));
	restoreHiZ.EndCustomList();

	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
	GRCDEVICE.GetCurrent()->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	PF_POP_TIMEBAR_DETAIL();
}

void GBuffer::RestoreHiStencil( int stencilRef, bool useEqual, bool bUseDepthQuarter )
{
	PF_PUSH_TIMEBAR_DETAIL("Mark HiStencil");
	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, bUseDepthQuarter?GBuffer::Get4xHiZRestoreQuarter():GBuffer::Get4xHiZRestore());

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(useEqual?m_MarkHiStencilState_DS:m_MarkHiStencilStateNE_DS, stencilRef );
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);


	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, TRUE );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, useEqual? D3DHSCMP_NOTEQUAL:D3DHSCMP_EQUAL );
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, stencilRef);

	CSprite2d markHiZ;

	CSprite2d restoreHiZ;

	restoreHiZ.BeginCustomList(CSprite2d::CS_RESTORE_HIZ, NULL);
	grcDrawSingleQuadf(0,0,1,1,0,0,0,1,1,Color32(255,255,255,255));
	restoreHiZ.EndCustomList();

	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
	GRCDEVICE.GetCurrent()->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );
 
	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	PF_POP_TIMEBAR_DETAIL();

}
void GBuffer::BeginUseHiStencil( int stencilRef, bool useEqual )
{
	Assert( g_MarkingStencilRef == -1 );
	ASSERT_ONLY( g_MarkingStencilRef = stencilRef);

	CRenderTargets::UnlockSceneRenderTargets();

	RestoreHiStencil(stencilRef, useEqual, false);

	CRenderTargets::LockSceneRenderTargets();

	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, TRUE);
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, useEqual? D3DHSCMP_EQUAL:D3DHSCMP_NOTEQUAL );
	
}
void GBuffer::EndUseHiStencil( int ASSERT_ONLY(stencilRef)  )
{
	Assert( g_MarkingStencilRef == stencilRef );
	ASSERT_ONLY( g_MarkingStencilRef = -1);
	GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE);
}
void GBuffer::CalculateTiles(D3DRECT *pTilingRects, const u32 numTiles)
{
	#if !__FINAL
	if(PARAM_verticaltiling.Get())
	{
		int w = m_Width / numTiles;
		int h = m_Height;

		if (w % 80) { w = (w - w % 80) + 80; }

		for (u32 i = 0; i < numTiles; i++)
		{
			pTilingRects[i].x1=w*i;				pTilingRects[i].y1=0; 
			pTilingRects[i].x2=w*(i+1);			pTilingRects[i].y2=h;

			if (i+1 == numTiles)
			{
				pTilingRects[i].x2=m_Width;
			}
		}
	}
	else
	#endif
	{
		int w = m_Width;
		int h = m_Height / numTiles;

		if (h & 0x1f) { h = (h&~0x1f) + 32; }

		for (u32 i = 0; i < numTiles; i++)
		{
			pTilingRects[i].x1=0;				pTilingRects[i].y1=h*i; 
			pTilingRects[i].x2=w;				pTilingRects[i].y2=h*(i+1);

			if (i+1 == numTiles)
			{
				pTilingRects[i].y2=m_Height;
			}
		}
	}
}
// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //
#if __XENON
// ----------------------------------------------------------------------------------------------- //
void GBuffer::CopyDepth()
{
	PF_PUSH_TIMEBAR_DETAIL("Copy GBuffer Depth");
	CSprite2d copyDepth;
	grcTextureFactory::GetInstance().LockRenderTarget(0, GBuffer::GetDepthTargetAsColor(), NULL);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_copyDepth_DS);
	grcStateBlock::SetRasterizerState(m_copyDepth_R);

	copyDepth.BeginCustomList(CSprite2d::CS_COPY_GBUFFER_DEPTH, NULL);
	grcDrawSingleQuadf(0,0,1,1,0,0,0,1,1,Color32(255,255,255,255));
	copyDepth.EndCustomList();

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	PF_POP_TIMEBAR_DETAIL();
}

// Copies only the stencil bits (doesn't shave off top 5 bits like CopyDepth() does)
void GBuffer::CopyStencil()
{
	PF_PUSH_TIMEBAR_DETAIL("Copy GBuffer Stencil");
	CSprite2d copyDepth;
	grcTextureFactory::GetInstance().LockRenderTarget(0, GBuffer::GetDepthTargetAsColor(), NULL);

	grcStateBlock::SetBlendState(m_copyStencil_BS); // Only let the R channel through, this prevents us from stomping depth
	grcStateBlock::SetDepthStencilState(m_copyDepth_DS);
	grcStateBlock::SetRasterizerState(m_copyDepth_R);

	copyDepth.BeginCustomList(CSprite2d::CS_COPY_GBUFFER_DEPTH, NULL, 1);
	grcDrawSingleQuadf(0,0,1,1,0,0,0,1,1,Color32(255,255,255,255));
	copyDepth.EndCustomList();

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	PF_POP_TIMEBAR_DETAIL();
}

// Taken for the Xbox FastDepthRestore sample
void GBuffer::RestoreDepthOptimized(grcRenderTarget* depthTargetAlias, bool bRestore3Bits)
{
	if(AssertVerify(depthTargetAlias))
	{
		PF_PUSH_TIMEBAR("Restore Depth/Stencil"); 
		const UInt32 baseAddress = ((grcRenderTargetXenon*)depthTargetAlias)->GetSurface()->ColorInfo.ColorBase;

		grcTextureFactory::GetInstance().LockRenderTarget(0, depthTargetAlias, NULL);

		int iRowWidth = depthTargetAlias->GetWidth();
		int iRowHeight = 2 * GPU_EDRAM_TILE_HEIGHT_1X; // Smallest size usable for consistent Hi-Z mapping
		int iEDRAMTilesPerRow = XGSurfaceSize( iRowWidth, iRowHeight, D3DFMT_D24FS8, D3DMULTISAMPLE_NONE );
		int iHiZTilesPerRow = XGHierarchicalZSize( iRowWidth, iRowHeight, D3DMULTISAMPLE_NONE );

		// Special case for last row when depth buffer height is not a multiple of row height (for instance 720!)
		int iLastRowHeight = ( ( depthTargetAlias->GetWidth() - 1 ) % iRowHeight ) + 1;
		bool bPartialLastRow = iLastRowHeight != iRowHeight;
		float fFullRowBottom = -1.0f;
		float fFullRowTop = 1.0f;
		float fPartialRowBottom = -( 2.0f * ( iLastRowHeight / (float) iRowHeight ) - 1.0f );
		float fPartialRowTop = fPartialRowBottom;

		// Temporary fake surfaces aliasing rows of the original depth buffer
		IDirect3DSurface9 DepthSurface;
		grcDeviceSurfaceParameters DepthSurfaceParams;
		DepthSurfaceParams.Base = baseAddress;
		DepthSurfaceParams.HierarchicalZBase = 0;
		DepthSurfaceParams.ColorExpBias = 0;
		DepthSurfaceParams.HiZFunc = D3DHIZFUNC_DEFAULT;
		IDirect3DSurface9 ColorSurface;
		grcDeviceSurfaceParameters ColorSurfaceParams = DepthSurfaceParams;

		int iDepthBufferSizeInEDRAMTiles = XGSurfaceSize(depthTargetAlias->GetWidth(), 
			depthTargetAlias->GetHeight(), 
			D3DFMT_D24FS8, 
			D3DMULTISAMPLE_NONE );

		CSprite2d copyDepth;

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(m_copyDepth_DS);
		grcStateBlock::SetRasterizerState(m_copyDepth_R);

		float arrRowOffset[] = { 0.0f, 0.0f };

		// Restore depth row-by-row, using the color buffer to write depth, and the depth buffer to refresh Hi-Z
		RestoreDepthPass_e passType;
		const RestoreDepthPass_e passTypeDepth = bRestore3Bits? RESTORE_DEPTH_3_BIT_STENCIL_PASS : RESTORE_DEPTH_NORMAL_PASS;
		while( DepthSurfaceParams.Base < baseAddress + iDepthBufferSizeInEDRAMTiles )
		{
			float fRowBottom = fFullRowBottom;
			float fRowTop = fFullRowTop;

			copyDepth.SetGeneralParams(Vector4((float)iRowWidth, (float)iRowHeight, arrRowOffset[0], arrRowOffset[1]), Vector4(0.0f, 0.0f, 0.0f, 0.0f));

			if( ColorSurfaceParams.Base == baseAddress )
			{
				// First row, restore depth using color-write only.
				// Temp color buffer aliases first row of real depth buffer.
				XGSetSurfaceHeader( iRowWidth, 
					iRowHeight, 
					D3DFMT_A8R8G8B8, 
					D3DMULTISAMPLE_NONE, 
					&ColorSurfaceParams, 
					&ColorSurface, 
					0 );
				GRCDEVICE.SetRenderTarget(0, &ColorSurface);
				GRCDEVICE.SetDepthStencilSurface(NULL);

				// Advance color row
				ColorSurfaceParams.Base += iEDRAMTilesPerRow;
				arrRowOffset[1] += iRowHeight;
				passType = passTypeDepth;				
			}
			else if ( ColorSurfaceParams.Base < baseAddress + iDepthBufferSizeInEDRAMTiles ) // Assume depth EDRAM base is 0 
			{
				// Middle row - restore Hi-Z for row n-1 while restoring depth for row n.
				// Temp depth buffer aliases previous row of real depth buffer.
				// Temp color buffer aliases current row of real depth buffer.
				XGSetSurfaceHeader( iRowWidth, 
					iRowHeight, 
					D3DFMT_A8R8G8B8, 
					D3DMULTISAMPLE_NONE, 
					&ColorSurfaceParams, 
					&ColorSurface, 
					0 );
				XGSetSurfaceHeader( iRowWidth, 
					iRowHeight, 
					D3DFMT_D24FS8, 
					D3DMULTISAMPLE_NONE, 
					&DepthSurfaceParams, 
					&DepthSurface, 
					0 );
				GRCDEVICE.SetRenderTarget( 0, &ColorSurface );
				GRCDEVICE.SetDepthStencilSurface( &DepthSurface );

				// Special case, last row may be a partial row
				BOOL bThisIsLastRow = ColorSurfaceParams.Base + iEDRAMTilesPerRow > baseAddress + iDepthBufferSizeInEDRAMTiles; // Assume depth EDRAM base is 0

				// Advance depth row (unless this was a partial row)
				if( bPartialLastRow && bThisIsLastRow )
				{
					fRowBottom = fPartialRowBottom;
				}
				else
				{
					DepthSurfaceParams.Base += iEDRAMTilesPerRow;
					DepthSurfaceParams.HierarchicalZBase += iHiZTilesPerRow;
				}

				// Advance color row
				ColorSurfaceParams.Base += iEDRAMTilesPerRow;
				arrRowOffset[1] += iRowHeight;
				passType = passTypeDepth;				
			}
			else
			{
				// Last row (or two) - restore Hi-Z only, using double-depth & 4xMSAA.
				// Temp depth buffer aliases last row (or two) of real depth buffer.
				XGSetSurfaceHeader( iRowWidth / 2, 
					iRowHeight / 2, 
					D3DFMT_D24FS8, 
					D3DMULTISAMPLE_4_SAMPLES, 
					&DepthSurfaceParams, 
					&DepthSurface, 
					0 );
				GRCDEVICE.SetRenderTarget( 0, NULL );
				GRCDEVICE.SetDepthStencilSurface( &DepthSurface );

				// Special case, last row may be a partial row
				BOOL bThisIsLastRow = DepthSurfaceParams.Base + iEDRAMTilesPerRow > baseAddress + iDepthBufferSizeInEDRAMTiles; // Assume depth EDRAM base is 0
				if( bPartialLastRow && !bThisIsLastRow )
				{
					fRowTop = fPartialRowTop;
				}
				else if( bPartialLastRow && bThisIsLastRow )
				{
					fRowBottom = fPartialRowBottom;
				}

				DepthSurfaceParams.Base += iEDRAMTilesPerRow;
				DepthSurfaceParams.HierarchicalZBase += iHiZTilesPerRow;

				passType = RESTORE_DEPTH_SKIP_PIXEL_SHADER;
			}

			// DF: Sample says we need to change this every time
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_ZENABLE, TRUE );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, FALSE );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, TRUE );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC,D3DHSCMP_EQUAL );
			GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, DEFERRED_MATERIAL_CLEAR);

			copyDepth.BeginCustomList(CSprite2d::CS_RSTORE_GBUFFER_DEPTH, depthTargetAlias, (int)passType);
			grcDrawSingleQuadf(-1.0f, fRowBottom, 1.0f, fRowTop, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Color32(255,255,255,255));
			copyDepth.EndCustomList();

			GRCDEVICE.SetRenderTarget( 0, NULL );
			GRCDEVICE.SetDepthStencilSurface( NULL );
		}

		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC );
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE );
		GRCDEVICE.GetCurrent()->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

		grcResolveFlags resolveFlags;
		resolveFlags.NeedResolve = false;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
		PF_POP_TIMEBAR();
	}
}

// ----------------------------------------------------------------------------------------------- //
#endif //__XENON
// ----------------------------------------------------------------------------------------------- //

void GBuffer::CopyBuffer(GBufferRT index, bool XENON_ONLY(bTopTileOnly) /*=false*/)
{
	CSprite2d copyBuffer;
	copyBuffer.BeginCustomList(CSprite2d::CS_COPY_GBUFFER, GetTarget(index));

#if __XENON
	if (bTopTileOnly)
	{
		D3DRECT pTilingRects[NUM_TILES];
		GBuffer::CalculateTiles(pTilingRects, NUM_TILES);
		float fY1 = 1.0f - (pTilingRects[0].y2 / static_cast<float>(m_Height));
		float fV1 = pTilingRects[0].y2 / static_cast<float>(m_Height);
		float fV2 = pTilingRects[0].y1 / static_cast<float>(m_Height);
		grcDrawSingleQuadf(0,fY1,1,1, 0, 0.0f,fV1,1.0f,fV2, Color32(255,255,255,255));
	}
	else
#endif // __XENON
	{
		grcDrawSingleQuadf(0,0,1,1, 0, 0.0f,1.0f,1.0f,0.0f, Color32(255,255,255,255));
	}

	copyBuffer.EndCustomList();

#if __PPU
	grcTextureFactory::GetInstance().UnlockRenderTarget( 0 );
#endif
}

void GBuffer::ClearFirstTarget()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcTextureFactory::GetInstance().LockRenderTarget(0,GetTarget(GBUFFER_RT_0),NULL);
	GRCDEVICE.Clear( true,Color32(0x00000000),false,false,false,0);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

void GBuffer::ClearAllTargets(bool onlyMeta)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if RSG_ORBIS	//|| RSG_MANTLE
	if (onlyMeta)
	{

		for (int i=GBUFFER_RT_0; s_bFastClearSupport && i<=GBUFFER_RT_3; ++i)
		{
			grcRenderTargetMSAA *const target = static_cast<grcRenderTargetMSAA*>(m_Target[i]);
			// Cmask clear has to be finalized by either EliminateFastClear, or FmaskDecompress
			// Sony issue 24629 is created to investigate why pure-MSAA configurations fail with EliminateFastClear
			// Also see B#1745138, B#1664618
			if (/*!GRCDEVICE.IsEQAA() ||*/ GRCDEVICE.ClearCmask(i-GBUFFER_RT_0, target->GetColorTarget(), Color32(0x00000000)))
			{
				target->PushCmaskDirty();
			}
		}
		// this is done inside ClearCmask, but we may want to do it here once instead
		//if (s_ClearWithGPU)
		//GRCDEVICE.SynchronizeComputeToGraphics();
	}else
#elif DEVICE_MSAA
	if (!onlyMeta || m_Target[GBUFFER_RT_0]->GetMSAA())
#else
	if (!onlyMeta)
#endif //RSG_ORBIS, DEVICE_MSAA
	{
		const grcRenderTarget* rendertargets[grcmrtColorCount] = {
			m_Target[GBUFFER_RT_0],
			m_Target[GBUFFER_RT_1],
			m_Target[GBUFFER_RT_2],
			m_Target[GBUFFER_RT_3]
		};

		grcTextureFactory::GetInstance().LockMRT(rendertargets,CRenderTargets::GetDepthBuffer());
		GRCDEVICE.Clear(true,Color32(0x00000000),false,0.f,false,0);
		grcTextureFactory::GetInstance().UnlockMRT();
	}
}
// ----------------------------------------------------------------------------------------------- //
void GBuffer::BeginPrimeZBuffer()
{
	GBuffer::UnlockTargets();
	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, CRenderTargets::GetDepthBuffer());
}
void GBuffer::EndPrimeZBuffer()
{
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	GBuffer::LockTargets();
}


// ----------------------------------------------------------------------------------------------- //
#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
grcRenderTarget *GBuffer::CopyGBuffer(GBufferRT UNUSED_PARAM(index), bool 
#if GBUFFER_MSAA
isGBufferResolved
#endif // GBUFFER_MSAA
)
{
	grcRenderTarget *pRet = NULL;
#if GBUFFER_MSAA
	bool useMSAAVersion = !isGBufferResolved;
#else
	bool useMSAAVersion = false;
#endif

	CRenderTargets::CopyTarget(GetTarget(GBUFFER_RT_1), GetCopyOfTarget(GBUFFER_RT_1));
	// DX11 TODO:- Make sure copy works if g-buffer has already been resolved (the DX11 render target copy just copis the MSAA texture).
	pRet = GetCopyOfTarget(GBUFFER_RT_1, useMSAAVersion);
	return pRet;
}
#endif // CREATE_TARGETS_FOR_DYNAMIC_DECALS

// ----------------------------------------------------------------------------------------------- //
#if DEVICE_EQAA && GBUFFER_MSAA
const grcTexture *GBuffer::GetFragmentMaskTexture(GBufferRT index)
{
	Assert(index >= GBUFFER_FMASK_0 && index <= GBUFFER_FMASK_3);
#if DYNAMIC_GB
	grcRenderTarget *const buffer = m_AltTarget[ GBUFFER_RT_0 + index-GBUFFER_FMASK_0 ] ? m_AltTarget[ GBUFFER_RT_0 + index-GBUFFER_FMASK_0 ] : m_Target[ GBUFFER_RT_0 + index-GBUFFER_FMASK_0 ];
#else
	grcRenderTarget *const buffer = m_Target[ GBUFFER_RT_0 + index-GBUFFER_FMASK_0 ];
#endif
	return static_cast<grcRenderTargetMSAA*>(buffer)->GetCoverageData().texture;
}
#endif // DEVICE_EQAA && GBUFFER_MSAA

#if DYNAMIC_GB
void GBuffer::FlushAltGBuffer(GBufferRT i)
{
	if (m_AltTarget[i])
	{
		ESRAMManager::ESRAMCopyOut(m_Target[i], m_AltTarget[i]);
		static_cast<grcRenderTargetDurango*>(m_Target[i])->SetUseAltTarget(false);
	}
}

void GBuffer::InitAltGBuffer(GBufferRT i)
{
	if (m_AltTarget[i])
	{
		static_cast<grcRenderTargetDurango*>(m_Target[i])->SetUseAltTarget(true);
	}
}

bool GBuffer::UseAltGBufferTest0()
{
	return (DRAWLISTMGR->IsExecutingGBufDrawList());	
}

bool GBuffer::UseAltGBufferTest1()
{
	const bool bLateEviction = BANK_SWITCH( DebugDeferred::m_useLateESRAMGBufferEviction, true );
	return (DRAWLISTMGR->IsExecutingGBufDrawList() || (bLateEviction && (DRAWLISTMGR->IsExecutingLightingDrawList() || DRAWLISTMGR->IsExecutingCascadeShadowDrawList())));
}

bool GBuffer::UseAltGBufferTest2()
{
	return (DRAWLISTMGR->IsExecutingGBufDrawList() || DRAWLISTMGR->IsExecutingCascadeShadowDrawList());	
}

bool GBuffer::UseAltGBufferTest3()
{
	const bool bLateEviction = BANK_SWITCH( DebugDeferred::m_useLateESRAMGBufferEviction, true );
	return (DRAWLISTMGR->IsExecutingGBufDrawList() || (bLateEviction && (DRAWLISTMGR->IsExecutingLightingDrawList() || DRAWLISTMGR->IsExecutingCascadeShadowDrawList())));
}

#endif
