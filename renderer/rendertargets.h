//
// renderer/rendertargets.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef RENDERER_RENDERTARGETS_H
#define RENDERER_RENDERTARGETS_H

#include "math/amath.h"
#include "grcore/texture.h"
#include "vfx/vfx_shared.h"
#include "control/replay/ReplaySupportClasses.h"

#define DYNAMIC_BB (1 && DYNAMIC_ESRAM)

#if __XENON
#if __ASSERT
#include "system/system.h" //thread check for rendertarget resolve
#endif // __ASSERT

enum RTMemPoolID {
	XENON_RTMEMPOOL_GENERAL = 0,	// contains reflection, PostFX,
	XENON_RTMEMPOOL_GENERAL1,		// contains SSAO, water, tiled classification, Depth Quarter, ped player damage/decoration heaps
	XENON_RTMEMPOOL_GBUFFER0,		// GBuffer0(Albedo), sub-sample alpha, PostFX, Ped UI 0, Volume Offscreen, Volume Offscreen Hi-Res
	XENON_RTMEMPOOL_GBUFFER1,		// GBuffer1(Normal) on its own, SeeThrough Targets, mirror
	XENON_RTMEMPOOL_GBUFFER23,		// GBuffer2 and 3, Back Buffer, Ped UI 1, Ptfx downsample Color
	XENON_RTMEMPOOL_FRONTBUFFER0,	// Front Buffer 0, UI Depth Buffer, Scene Depth Buffer, Scene Depth Alias (Rolls across the current front buffer, not safe to cache)
	XENON_RTMEMPOOL_FRONTBUFFER1,	// Front Buffer 1, UI Depth Buffer, Scene Depth Buffer, Scene Depth Alias (Rolls across the current front buffer, not safe to cache)
	XENON_RTMEMPOOL_FRONTBUFFER2,	// Front Buffer 2, UI Depth Buffer, Scene Depth Buffer, Scene Depth Alias (Rolls across the current front buffer, not safe to cache)
	XENON_RTMEMPOOL_SHADOWS,		// contains Shadows and medium/low ped damage heaps
	XENON_RTMEMPOOL_SHADOW_CACHE,	// contains Shadow cache (don't overwrite or reuse)
	XENON_RTMEMPOOL_TEMP_BUFFER,	// DO NOT REUSE: contains a temporary fullscreen buffer
	XENON_RTMEMPOOL_PERLIN_NOISE,	// Perlin Noise
	XENON_RTMEMPOOL_MAXNUM,
	RTMEMPOOL_NONE
};

enum XenonGeneralPoolHeaps {	// the main render target pool on xenon uses several heaps to start from the beginning of the pool for each set.
	kReflectionHeap, //Mirror Reflection, Paraboloid Reflection,
	kWaterReflectionHeap, //Water Reflection
	kMirrorWaterReflectionHeap, // Mirror Water Reflection
	kWaterReflectionAliasHeap, //Water Reflection Alias
	kPostFxGeneralPoolHeap,
	kDeferredPedUIHeap3,
	kDeferredPedUIHeap5,
	kGeneralHeapCount,
};

enum XenonGeneral1PoolHeaps {	
	kSSAOWaterHeap,	//Water Lighting 1/4, Water Lighting, Water Refraction, Depth Quarter Linear, TOBEADDED: Depth Quarter
	kPuddleHeap,
	kMainWaterHeap,	//Water Lighting 1/16, Water Bump
	kTiledLightingHeap0,
	kTiledLightingHeap1,
	kTiledLightingHeap2,
	kTiledLightingHeap3,
	kDepthQuarter,		// Shadowed PTX Buffer
	kCubeReflectionHeap,
	kPedMedResDamageHeap,
	kPedLowResDamageHeap,
	kMinimapOffscreenBufferHeap,
	kMinimapOffscreenDepthBufferHeap,
	kMirrorPedDamageHeap,
	kGeneral1HeapCount,
};

enum XenonGBuffer0PoolHeaps {
	kGBuffersHeap,
	kPostFxHeap,
	kScreenShotHeap,
	kVolumeLightFxHeap,
	kOffscreenBuffer0,
	kOffscreenBuffer2,
	kVolumeLightFxHeapHiRes,
	kDeferredPedUIHeap0,
	kDeferredPedUIHeap1,
	kDeferredPedUIHeap6,
	kGBuffer0HeapCount
};

enum XenonGBuffer1PoolHeaps {
	kGBuffer1Heap,
	kFXHeap,
	kDummyHeap,
	kGBuffer1MirrorHeap,
	kOffscreenBuffer1,
	kGBuffer1HeapCount
};

enum XenonGBuffer23PoolHeaps {	// UI Backbuffer (declared in RAGE)
	kGBuffer23Heap,
	kBackBufferHeap,            // HDR scene back buffer (10bit float resolves to 16bit float)
	kBackBuffer7e3IntAliasHeap, // HDR back buffer alias (10bit float resolves to 10bit int)
	kFXAAHeap,					// Used only for: final composite output / FXAA input
	kPtfxDownsampleHeap,		// Used only for: Downsampled render target for ptfx
	kGBuffer23MirrorHeap,
	kGBuffer23HeapCount
};

enum XenonShadowPoolHeaps {
	kShadowMapHeap,
	kShadowMapHighResHeap, // used for CASCADE_SHADOWS_ALTERNATE_SHADOW_MAPS
	kShadowMapFixed24Heap, // used for CASCADE_SHADOWS_ALTERNATE_SHADOW_MAPS, CASCADE_SHADOWS_FLOAT_SHADOW_BUFFER
	kPedHighResDamageHeap,
	kPedMedRes2DamageHeap,
	kPedLowRes2DamageHeap,
	kPhotoHeap,
	kHeadshotHeap,
	kHeadshotSmallHeap,
	kShadowHeapCount,
};

enum XenonShadowCachePoolHeaps { // so cube maps and Paraboloid maps can co exist for now...
	kShadowMapCacheParaboloidHeap,
	kShadowMapCacheParaboloidHeap2,
	kShadowMapCacheCubeMapHeap,
	kShadowCacheHeapCount,
};

enum XenonTempBufferPoolHeaps {	// DO NOT REUSE: temporary new heap
	kTempBufferPoolHeap,
	kDeferredPedUIHeap2,
	kUIPedHeadshotHeap,
	kUIPedHeadshotHeap2,
	kTempBufferPoolHeapCount,
};

enum XenonPerlinNoisePoolHeaps {
	kPerlinNoiseHeap,
	kDeferredPedUIHeap4,
	kMemeEditorHeap,
	kPerlinNoisePoolHeapCount
};

#endif // __XENON

enum MSDepthResolve {
	depthResolve_Sample0,
	depthResolve_Closest,
	depthResolve_Farthest,
	depthResolve_Dominant,
};

#include "grcore/device.h"
#if __BANK
#include "vectormath/vec2v.h"
#endif // __BANK

#include "renderer/Deferred/DeferredConfig.h"
#include "debug/rendering/DebugDeferred.h"
#include "renderer/Lights/LightCommon.h"
#include "renderer/TreeImposters.h"

#include "grcore/rendertarget_common.h"

namespace rage
{
	class grcRenderTarget;
	class grcRenderTargetMemPool;
	class bkBank;
};

#define USE_DEPTHBUFFER_ZCULL_SHARING (1 &&  __PS3) // Force the backbuffer depth and the offscreen depth buffers to share their zcull memory

#if __PPU

#include "renderer/psnvramrendertargettotal.h"
#include "renderer/SpuPM/SpuPmMgr.h"

#define PSN_BUFFER_WIDTH						(1280)
#define PSN_BUFFER_HEIGHT						(720)
#define PSN_RESOLVE_TO_ALTBUFFER				(0)
#define PSN_CROP_RESOLVE						(0)
#define PSN_USE_FP16							(1)
enum RTMemPoolID {
	PSN_RTMEMPOOL_P10240_TILED_LOCAL_COMPMODE_DISABLED,							// 0
	PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_DISABLED,							// 1 
	PSN_RTMEMPOOL_P2048_TILED_LOCAL_COMPMODE_C32_2X1,							// 2
	PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED,							// 3
	PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2,						// 4
	PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED,							// 5
	PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_C32_2X1,							// 6
	PSN_RTMEMPOOL_P1280_TILED_LOCAL_COMPMODE_C32_2X1,							// 7
	
	// Depth buffer
	PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR,			// 8
	PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR,				// 9
	PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_DIAGONAL,			// 10
	PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR_BBUFFER,	// 11
	PSN_RTMEMPOOL_MAXNUM,
	RTMEMPOOL_NONE						
};

#define RTMEMPOOL_SIZE int(PSN_RTMEMPOOL_MAXNUM)
#else // __PPU

#define PSN_BUFFER_WIDTH						GRCDEVICE.GetWidth()
#define PSN_BUFFER_HEIGHT						GRCDEVICE.GetHeight()
#define PSN_RESOLVE_TO_ALTBUFFER				(0)
#define PSN_CROP_RESOLVE						(0)
#define PSN_USE_FP16							(0)

#if __SPU
typedef int RTMemPoolID;
#endif

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
enum RTMemPoolID {RTMEMPOOL_NONE};
#endif

#if __XENON
#define RTMEMPOOL_SIZE  int(XENON_RTMEMPOOL_MAXNUM)
#else
#define RTMEMPOOL_SIZE 4
#endif

#endif //__PPU...

class CRenderTargets
{
public:
	static void PreAllocateRenderTargetMemPool();
	static void AllocateRenderTargets();
#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif
	static void DeAllocateRenderTargets();

	static void LockSceneRenderTargets(bool lockDepth = true);
	static void LockSceneRenderTargets_DepthCopy();
	static void UnlockSceneRenderTargets();

	static void LockUIRenderTargets();
	static void UnlockUIRenderTargets(bool realize = false);
#if __D3D11
	static void LockUIRenderTargetsReadOnlyDepth();
#endif // __D3D11

	static bool NeedsRescale();
#if __PS3
	static void Rescale();
#endif

	static void ResetRenderThreadInfo();
	static void SetDownsampleDepthFlag(bool value) { ms_DepthDownsampleReady = value; };

	static void DownsampleDepth();
#if __XENON
	static int GetRenderTargetMemPoolSize(int width, int height);
	static void AllocRenderTargetMemPool(int size, RTMemPoolID id, u8 heapCount = 1, void* preallocatedBuffer = NULL);
#endif
	static void FreeRenderTargetMemPool();
	static rage::u16 GetRenderTargetPoolID(RTMemPoolID i)	{ FastAssert((int)i < RTMEMPOOL_SIZE); return ms_RenderTargetMemPoolArray[i]; }
	static grcRenderTarget* CreateRenderTarget(RTMemPoolID memPoolID, const char *name, grcRenderTargetType type, int width, int height, int bitsPerPixel, rage::grcTextureFactory::CreateParams *params=NULL, u8 poolHeap=0, grcRenderTarget* originalTexture = NULL);
	static grcRenderTarget* CreateRenderTarget(const char* name, grcTextureObject* sourceTexture, grcRenderTarget* originalTexture = NULL);

#if __BANK
	static void CheckPoolSizes();
	static void UpdateBank(const bool forceLoad);
	static void RenderBank(int mode);
	static bool IsShowingRenderTarget();
	static bool IsShowingRenderTarget(const char* name);
	static bool IsShowingRenderTargetWaterDraw();
	static Vec2V_Out ShowRenderTargetGetWindowMin();
	static Vec2V_Out ShowRenderTargetGetWindowMax();
	static void ShowRenderTarget(const char* name);
	static void ShowRenderTargetOpacity(float opacity);
	static void ShowRenderTargetBrightness(float brightness);
	static void ShowRenderTargetNativeRes(bool nativeRes, bool flipHoriz, bool waterDraw = false);
	static void ShowRenderTargetSetBounds(float x, float y, float w, float h);
	static void ShowRenderTargetFullscreen(bool fullscreen);
	static int& ShowRenderTargetGetMipRef();
	static void ShowRenderTargetInfo(bool show);
	static void ShowRenderTargetReset(); // reset back to defaults
	static void AddWidgets(bkBank &bank);
	static void DumpRenderTargets();
	static void DisplayRenderTargetOverlaps();
	static void DisplayRenderTargetMemPool(u16 index);
	static void DisplayRenderTarget(u32 index);
	static void BlitRenderTarget(grcRenderTarget* pRT, const Vector4 &posAndSize, float opacity = 1.0f, bool bUseDepth = false);
#endif // __BANK 


#if !(__WIN32PC || RSG_DURANGO || RSG_ORBIS)
	static void	RefreshStencilCull(bool useEqual, int material, int mask PS3_ONLY(, bool preserveZFarCull = true));
	static void ClearStencilCull(int material);
#endif

#if RSG_PS3 || RSG_XENON
	static void ConvertAlphaMaskToStencilMask(int alphaRef, grcTexture* pSourceAlpha XENON_ONLY(, bool bUseStencilAsColor = false));
#endif

	static void DrawFullScreenTri ( float depth );

	// PSN Flicker filters (for SD res only)
#if __PPU
	static void	RefreshStencilZFarCull(bool useEqual, int material, int mask);
	static void ResetStencilCull(bool useEqual, int material, int mask, bool clearZ = false, bool clearStencil = true);
	static void	RefreshZFarCull(bool useDepthGreater);
	static void	SetZCullControl(bool useDepthGreater);

	// Render Targets
	static void BeginMSAARender();
	static void EndMSAARender();
#endif // __PPU

	static grcRenderTarget*			GetBackBuffer(bool realize = false);
#if DYNAMIC_BB
	static void						InitAltBackBuffer();
	static void						SetAltBackBuffer();
	static void						ResetAltBackBuffer();
	static void						FlushAltBackBuffer();
	static bool						UseAltBackBufferTest();
#endif
	static grcRenderTarget*			GetUIBackBuffer(bool realize = false);
	static const grcRenderTarget*	GetUIFrontBuffer();

	static grcRenderTarget*			GetDepthBuffer(bool realize = false);	//Main scene depth buffer
	static grcRenderTarget*			GetDepthBuffer_PreAlpha() {return ms_DepthBuffer_PreAlpha;}
#if RSG_PC
	static grcRenderTarget*			GetUIDepthBuffer_PreAlpha() {return ms_UIDepthBuffer_PreAlpha;}
#endif
#if __PPU || __XENON
	static grcRenderTarget*			GetDepthBufferAsColor();
	static grcRenderTarget*			GetDepthBufferQuarterAsColor();
	static void						SetDepthBufferQuarterAsColor(grcRenderTarget* rt);
#endif // __PPU || __XENON
	static grcRenderTarget*			GetUIDepthBuffer(bool realize = false);	//UI/Frontend depth buffer
	static grcRenderTarget*			GetDepthBufferQuarter();

	static grcRenderTarget*			GetDepthBufferQuarter_Read_Target(bool WIN32PC_ONLY(doCopy));
#if RSG_PC && __D3D11
	static grcRenderTarget*			GetDepthBuffer_PreAlpha_ReadOnly();
#endif // RSG_PC && __D3D11

	static grcRenderTarget*			GetVolumeOffscreenBuffer();
	
#if LIGHT_VOLUME_USE_LOW_RESOLUTION
	static grcRenderTarget*			GetVolumeReconstructionBuffer();
#endif
	
	static grcRenderTarget*			GetDepthBufferQuarterLinear();

#if DEVICE_MSAA
	// Get a custom resolved version of the depth (min, max single sample or average).
	static inline grcRenderTarget* GetDepthResolved();
#if RSG_PC
	static inline grcRenderTarget* GetUIDepthResolved();
	static inline grcRenderTarget* GetUIDepthBufferResolved_ReadOnly();
#endif
#if __D3D11
	static inline grcRenderTarget* GetDepthBufferResolved_ReadOnly();
#endif // __D3D11
	// Update the resolved light buffer
	static void ResolveBackBuffer(bool depthReadOnly);

#if ENABLE_PED_PASS_AA_SOURCE
	// Get the MSAA copy of the back buffer
	static grcRenderTarget* GetBackBufferCopyAA( bool bDoCopy=false );
#endif // ENABLE_PED_PASS_AA_SOURCE

	static void ResolveSourceDepth(MSDepthResolve depthResolveType, grcRenderTarget* sourceMSAADepthTarget, grcRenderTarget* depthResolveTarget);
	static void ResolveDepth(MSDepthResolve depthResolveType, grcRenderTarget* alternateResolveTarget = NULL);
#endif // DEVICE_MSAA

	static void CopyDepthManual(grcRenderTarget *srcDepthbuffer, grcRenderTarget *dstDepthBuffer);
	static void CopyDepthBuffer( grcRenderTarget* srcDepthbuffer, grcRenderTarget* dstDepthBuffer);

#if DEBUG_TRACK_MSAA_RESOLVES
	static bool IsBackBufferResolved();
#endif
#if RSG_PC || RSG_DURANGO
	// NOTE These can`t be be inlined as they are used in draw lists.
	static void CopyBackBuffer();
	static void CopyDepthBufferCmd() { CopyDepthBuffer(ms_DepthBuffer, ms_DepthBufferCopy); }	
	static void CopyTarget(grcRenderTarget *src, grcRenderTarget *dst, grcRenderTarget *dstStencil = NULL, u8 stencilRef = 0xFF );
#if RSG_PC
	static grcRenderTarget *GetBackBufferCopy( bool bDoCopy=false );
#else
	static inline grcRenderTarget *GetBackBufferCopy( bool bDoCopy=false );
#endif
	static inline grcRenderTarget *GetDepthBufferCopy( bool bDoCopy=false );
#if RSG_PC
	static inline grcRenderTarget* SwapBackBuffer();
	static inline grcRenderTarget* GetBackBufferCopyForPause();
	static void ResolveForMultiGPU(int bResolve);
#endif
#if __D3D11
	static inline grcRenderTarget *GetDepthBuffer_Stencil();
	static inline grcRenderTarget *GetDepthBufferCopy_Stencil();
	static inline grcRenderTarget *GetDepthBuffer_ReadOnly();
#if RSG_PC
	static inline grcRenderTarget *GetDepthBuffer_ReadOnlyDepth();
#endif
#endif
#elif __PS3
	static void CopyDepthBuffer( grcRenderTarget* srcDepthbuffer, grcRenderTarget* dstDepthBuffer, u8 stencilRef );
#elif RSG_ORBIS
	static void CopyBackBuffer();
	// these should return const grcTexture* ideally, but the client code expects grcRenderTarget*
	static inline grcRenderTarget	*GetBackBufferCopy( bool bDoCopy=false );
	static inline grcRenderTarget	*GetDepthBuffer_Stencil();	//actually returns AA texture
#endif // platforms

#if __D3D11
	static grcRenderTarget* LockReadOnlyDepth(bool needsCopy = true, bool bLockBothDepthStencilRO = true, grcRenderTarget* depthBuffer = ms_DepthBuffer, grcRenderTarget* depthBufferCopy = ms_DepthBufferCopy, grcRenderTarget* depthBufferReadOnly = ms_DepthBuffer_ReadOnly);
	static void UnlockReadOnlyDepth();
#endif	//__D3D11

	// In MSAA or EQAA enabled builds, perform a resolve of the specified
	// offscreen buffer to the corresponding single sample buffer.  Nop in
	// non-MSAA/EQAA builds.
	static void ResolveBuffer(unsigned index);

	static void ResolveHudBuffer()                              { ResolveBuffer(0); }
	static void ResolveOffscreenBuffer1()                       { ResolveBuffer(1); }
	static void ResolveOffscreenBuffer2()                       { ResolveBuffer(2); }
	static void ResolveOffscreenBuffer3()                       { ResolveBuffer(3); }

	// Get a poitner the specified multi-sample offscreen buffer (or
	// single-sample in non-MSAA/EQAA builds).
	static grcRenderTarget* GetOffscreenBuffer(unsigned index);

	// Get a pointer to the specified single-sample buffer.
	static grcRenderTarget* GetOffscreenBuffer_SingleSample(unsigned index);

	// Get a pointer to the specified single-sample buffer.  In single threaded
	// rendering (NUMBER_OF_RENDER_THREADS==1) beta builds, debug code will
	// track whether the offscreen buffer has been resolved.  Unlike
	// GetOffscreenBuffer_SingleSample, this function will assert if it hasn't
	// been resolved.  Multithreaded rendering builds cannot correctly track the
	// resolve state, so they cannot assert.
	static grcRenderTarget* GetOffscreenBuffer_Resolved(unsigned index);

	static grcRenderTarget* GetHudBuffer()                      { return GetOffscreenBuffer(0); }
	static grcRenderTarget* GetOffscreenBuffer1()               { return GetOffscreenBuffer(1); }
	static grcRenderTarget* GetOffscreenBuffer2()               { return GetOffscreenBuffer(2); }
	static grcRenderTarget* GetOffscreenBuffer3()               { return GetOffscreenBuffer(3); }

	static grcRenderTarget* GetHudBuffer_Resolved()             { return GetOffscreenBuffer_Resolved(0); }
	static grcRenderTarget* GetOffscreenBuffer1_Resolved()      { return GetOffscreenBuffer_Resolved(1); }
	static grcRenderTarget* GetOffscreenBuffer2_Resolved()      { return GetOffscreenBuffer_Resolved(2); }
	static grcRenderTarget* GetOffscreenBuffer3_Resolved()      { return GetOffscreenBuffer_Resolved(3); }

#if __PS3
	// If USE_DEPTHBUFFER_ZCULL_SHARING then this shares zcull memory with the backbuffer
	static grcRenderTarget*	GetOffscreenDepthBuffer();
	static grcRenderTarget*	GetOffscreenDepthBufferAsColor();
	static grcRenderTarget* SetOffscreenDepthBufferAsColor( grcRenderTarget* );
#endif

#if __XENON
	static grcRenderTarget* GetBackBuffer7e3toInt(bool bRealize = false);
#endif // __XENON

#if __XENON
	static grcRenderTarget* GetMinimapOffscreenBuffer();
	static grcRenderTarget* GetMinimapOffscreenDepthBuffer();
	static grcRenderTarget* GetBackBufferCopy();
#endif

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	static grcTexture* GetReplayThumbnailTexture(u32 idx);
	static grcRenderTarget* GetReplayThumbnailRT(u32 idx);
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY

private:

	static rage::u16 ms_RenderTargetMemPoolArray[RTMEMPOOL_SIZE];

	static DECLARE_MTR_THREAD bool ms_RenderTargetActive;

	static bool ms_DepthDownsampleReady;

	static grcRenderTarget* ms_BackBuffer;
#if __XENON
	static grcRenderTarget* ms_BackBuffer7e3ToIntAlias;
#endif
#if DYNAMIC_BB
	static grcRenderTarget* ms_AltBackBuffer;
#endif

#if RSG_PC || RSG_DURANGO
	static grcRenderTarget* ms_BackBufferCopy;
	static grcRenderTarget* ms_DepthBufferCopy;
#if RSG_PC
	// used for multi GPU
	static grcRenderTarget* ms_BackBufferRTForPause;
	static grcTexture*	ms_BackBufferTexForPause;
#endif
#if __D3D11
	static grcRenderTarget* ms_DepthBuffer_Stencil;
	static grcRenderTarget* ms_DepthBufferCopy_Stencil;
	static grcRenderTarget* ms_DepthBuffer_ReadOnly;
#if RSG_PC
	static grcRenderTarget* ms_DepthBuffer_ReadOnlyDepth;
#endif

	static bool ms_Initialized;
#endif   //__D3D11
#elif RSG_ORBIS
	static grcRenderTarget* ms_BackBufferCopy;
	static grcRenderTarget* ms_DepthBuffer_Stencil;
#endif	// platforms

#if __XENON
	static grcRenderTarget* ms_DepthBuffers[3];
#else
	static grcRenderTarget* ms_DepthBuffer;
	static grcRenderTarget* ms_DepthBuffer_PreAlpha;
#endif //__XENON

#if DEVICE_MSAA
	static grcRenderTarget* ms_BackBuffer_Resolved;
	static grcRenderTarget* ms_DepthBuffer_Resolved;
#if RSG_PC
	static grcRenderTarget* ms_UIDepthBuffer_PreAlpha;
	static grcRenderTarget* ms_UIDepthBuffer_Resolved;
	static grcRenderTarget* ms_UIDepthBuffer_Resolved_ReadOnly;
#endif
#if __D3D11
	static grcRenderTarget* ms_DepthBuffer_Resolved_ReadOnly;
#endif // __D3D11
#if ENABLE_PED_PASS_AA_SOURCE
	static grcRenderTarget* ms_BackBufferCopyAA;
#endif // ENABLE_PED_PASS_AA_SOURCE
#endif //DEVICE_MSAA

#if __PPU
	static grcRenderTarget* ms_PatchedDepthBuffer;
	static grcRenderTarget* ms_VSDepthBuffer;
	static grcRenderTarget* ms_PatchedNativeDepthBuffer;

	static grcBlendStateHandle ms_Refresh_BS;

	static grcDepthStencilStateHandle ms_RefreshStencil_DS;
	static grcDepthStencilStateHandle ms_RefreshStencilPreserveZFar_DS;
	static grcDepthStencilStateHandle ms_RefreshStencilZFar_DS;
	static grcDepthStencilStateHandle ms_RefreshZFar_G_DS;
	static grcDepthStencilStateHandle ms_RefreshZFar_L_DS;

	static bool ms_MSAARender;
	static bool ms_WithDepthRender;
#endif // __PPU

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	static grcTexture *ms_ReplayThumbnailTex[REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE];
	static grcRenderTarget *ms_ReplayThumbnailRT[REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE];
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#endif // GTA_REPLAY

	static grcDepthStencilStateHandle ms_CopyDepth_DS;

#if __PS3
	static grcDepthStencilStateHandle ms_CopyDepthClearStencil_DS;
#endif
#if __BANK
	static grcBlendStateHandle ms_BlitTransparent_BS;
#endif
	static grcDepthStencilStateHandle ms_CopyColorStencilMask_DS;

	static grcBlendStateHandle ms_AlphaMaskToStencilMask_BS;
	static grcDepthStencilStateHandle ms_AlphaMaskToStencilMask_DS;
};

#if DEVICE_MSAA
inline grcRenderTarget* CRenderTargets::GetDepthResolved()
{
	return GRCDEVICE.GetMSAA()>1 ? ms_DepthBuffer_Resolved	: GetDepthBuffer();
}
#if RSG_PC
inline grcRenderTarget* CRenderTargets::GetUIDepthResolved()
{
	return GRCDEVICE.GetMSAA()>1 ? ms_UIDepthBuffer_Resolved	: GetUIDepthBuffer();
}
inline grcRenderTarget* CRenderTargets::GetUIDepthBufferResolved_ReadOnly()
{
	return ms_UIDepthBuffer_Resolved_ReadOnly;
}

#endif
#if __D3D11
inline grcRenderTarget* CRenderTargets::GetDepthBufferResolved_ReadOnly()
{
	return GRCDEVICE.GetMSAA()>1 ? ms_DepthBuffer_Resolved_ReadOnly : GetDepthBuffer_ReadOnly();
}
#endif // __D3D11

#if ENABLE_PED_PASS_AA_SOURCE
inline grcRenderTarget* CRenderTargets::GetBackBufferCopyAA(bool bDoCopy)
{
	if(bDoCopy && ms_BackBufferCopyAA)
		ms_BackBufferCopyAA->Copy(ms_BackBuffer);
	return ms_BackBufferCopyAA;
}
#endif // ENABLE_PED_PASS_AA_SOURCE
#endif // DEVICE_MSAA

#if RSG_PC || RSG_DURANGO
#if !RSG_PC
inline grcRenderTarget *CRenderTargets::GetBackBufferCopy(bool bDoCopy/*=false*/)	{ if(bDoCopy) CopyBackBuffer(); return ms_BackBufferCopy; }
#endif
inline grcRenderTarget *CRenderTargets::GetDepthBufferCopy(bool bDoCopy/*=false*/)	{ if(bDoCopy) CopyDepthBuffer(ms_DepthBuffer, ms_DepthBufferCopy); return ms_DepthBufferCopy; }
#if RSG_PC
inline grcRenderTarget* CRenderTargets::SwapBackBuffer() { grcRenderTarget* copy = ms_BackBufferCopy; ms_BackBufferCopy = ms_BackBuffer; ms_BackBuffer = copy; return ms_BackBufferCopy; }
inline grcRenderTarget* CRenderTargets::GetBackBufferCopyForPause()		{ return ms_BackBufferRTForPause; }
#endif
#if __D3D11
inline grcRenderTarget *CRenderTargets::GetDepthBuffer_Stencil()	 {	return ms_DepthBuffer_Stencil; }
inline grcRenderTarget *CRenderTargets::GetDepthBufferCopy_Stencil() {	return ms_DepthBufferCopy_Stencil; }
inline grcRenderTarget *CRenderTargets::GetDepthBuffer_ReadOnly()	 {	/*Assert(GRCDEVICE.IsReadOnlyDepthAllowed());*/ return ms_DepthBuffer_ReadOnly; }
#if RSG_PC
inline grcRenderTarget *CRenderTargets::GetDepthBuffer_ReadOnlyDepth()	 {	/*Assert(GRCDEVICE.IsReadOnlyDepthAllowed());*/ return ms_DepthBuffer_ReadOnlyDepth; }
#endif
#endif
#elif RSG_ORBIS
inline grcRenderTarget	*CRenderTargets::GetBackBufferCopy(bool bDoCopy/*=false*/)	{ if(bDoCopy) CopyBackBuffer(); return ms_BackBufferCopy; }
inline grcRenderTarget	*CRenderTargets::GetDepthBuffer_Stencil() {	return ms_DepthBuffer_Stencil; }
#endif	//RSG_ORBIS


//////////////////////////////////////////////////////////////////////////
// VideoResManager
class VideoResManager {

public:

	enum eDownscaleFactor
	{
		DOWNSCALE_FIRST = 1,

		DOWNSCALE_ONE = DOWNSCALE_FIRST,
		DOWNSCALE_HALF = 2,
		DOWNSCALE_QUARTER = 4,
		DOWNSCALE_EIGHTH = 8,
		DOWNSCALE_SIXTEENTH = 16,
		
		DOWNSCALE_MAX = DOWNSCALE_SIXTEENTH
	};

	static eDownscaleFactor GetDownscaleFactor( u32 const rawDownscale );

	static void Init();
	static void Shutdown();

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif

	//Gets the system native depth
	static inline int GetNativeWidth();
	static inline int GetNativeHeight();

	//Gets the main scene width
	static inline int GetSceneWidth();
	static inline int GetSceneHeight();

	//Gets the UI/frontend width
	static inline int GetUIWidth();
	static inline int GetUIHeight();

#if RSG_PC
	enum ScalingMode {
		MODE_1o1,	// Default
		MODE_1o2,	// x 0.500
		MODE_2o3,	// x 0.667
		MODE_3o4,	// x 0.750
		MODE_5o6,	// x 0.834
		MODE_5o4,	// x 1.250
		MODE_3o2,	// x 1.500
		MODE_7o4,	// x 1.750
		MODE_2o1,	// x 2.000
		MODE_5o2,	// x 2.500
		MODE_COUNT
	};

	static void SetScalingMode(int mode) { m_scalingMode = mode; Assert(mode>=0 && mode<10); }
	static int GetScalingMode() { return m_scalingMode; }
	static void GetSceneScale(int scalingMode, int& scaleNumerator, int& scaleDenominator);
	static bool IsSceneScaled(){return(m_gameWidth != m_UIWidth || m_gameHeight != m_UIHeight);}
#endif

	//Gets the Screenshot width/height with some constraints
	static inline void GetScreenshotDimensions( u32& out_width, u32& out_height, eDownscaleFactor const scale = DOWNSCALE_ONE );
#if RSG_PC
	static inline void GetScreenshotDimensionsOfConsoles( u32& out_width, u32& out_height, eDownscaleFactor const scale = DOWNSCALE_ONE );
#endif
	static inline void ConstrainScreenshotDimensions( u32& inout_width, u32& inout_height, eDownscaleFactor const maxScale = DOWNSCALE_MAX );

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif 

private:
#if RSG_PC
	static int m_scalingMode;
#endif
	static int m_nativeWidth;
	static int m_nativeHeight;
	static int m_gameWidth;
	static int m_gameHeight;
	static int m_UIWidth;
	static int m_UIHeight;

};


inline int VideoResManager::GetNativeWidth() 
{
	return m_nativeWidth;
}
inline int VideoResManager::GetNativeHeight()
{
	return m_nativeHeight;
}
inline int VideoResManager::GetSceneWidth()
{
	return m_gameWidth;
}
inline int VideoResManager::GetSceneHeight()
{
	return m_gameHeight;
}
inline int VideoResManager::GetUIWidth()
{
	return m_UIWidth;
}
inline int VideoResManager::GetUIHeight()
{
	return m_UIHeight;
}

inline void VideoResManager::GetScreenshotDimensions( u32& out_width, u32& out_height, eDownscaleFactor const scale )
{
#if SUPPORT_MULTI_MONITOR
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
	out_width = mon.getWidth();
	out_height = mon.getHeight();
#else
	out_width = (u32)m_UIWidth;
	out_height = (u32)m_UIHeight;
#endif //SUPPORT_MULTI_MONITOR

	ConstrainScreenshotDimensions( out_width, out_height );

	u32 downscale = Clamp<u32>( scale, DOWNSCALE_FIRST, DOWNSCALE_MAX );
	out_width = out_width / downscale;
	out_height = out_height / downscale;
}

#if RSG_PC
inline void VideoResManager::GetScreenshotDimensionsOfConsoles( u32& out_width, u32& out_height, eDownscaleFactor const scale )
{
#define CONSOLE_X 1920
#define CONSOLE_Y 1080

	out_width = (u32)CONSOLE_X;
	out_height = (u32)CONSOLE_Y;

	ConstrainScreenshotDimensions( out_width, out_height );

	u32 downscale = Clamp<u32>( scale, DOWNSCALE_FIRST, DOWNSCALE_MAX );
	out_width = out_width / downscale;
	out_height = out_height / downscale;
}
#endif

inline void VideoResManager::ConstrainScreenshotDimensions( u32& inout_width, u32& inout_height, eDownscaleFactor const maxScale )
{
	//! First clamp the input sizes
	u32 const c_maxWidth = 1920;
	if( inout_width > c_maxWidth )
	{
		float const downscaleRatio = c_maxWidth / (float)inout_width;
		inout_height = (u32)(inout_height * downscaleRatio);
		inout_width = c_maxWidth;
	}

	u32 const c_maxHeight = 1080;
	if( inout_height > c_maxHeight )
	{
		float const downscaleRatio = c_maxHeight / (float)inout_height;
		inout_width = (u32)(inout_width * downscaleRatio);
		inout_height = c_maxHeight;
	}

	// Now ensure that the width and height are able to uniformly scale
	u32 const c_widthModulo = inout_width % maxScale;
	inout_width = inout_width - c_widthModulo;

	u32 const c_heightModulo = inout_height % maxScale;
	inout_height = inout_height - c_heightModulo;
}

#endif // RENDERER_RENDERTARGETS_H

// eof
