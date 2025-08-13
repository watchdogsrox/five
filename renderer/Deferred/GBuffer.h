#ifndef GBUFFER_H_
#define GBUFFER_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// game
#include "renderer/Deferred/DeferredConfig.h"
#include "vector/vector4.h"
#include "Renderer/DrawLists/drawListMgr.h"
#include "Renderer/SpuPM/SpuPmMgr.h"

// framework
#include "fwdrawlist/drawlistmgr.h"

// =============================================================================================== //
// DEFINITIONS
// =============================================================================================== //

#if DYNAMIC_GB
#include "grcore/rendertarget_durango.h"
#endif

#define GBUFFER_MSAA				( 1 && DEVICE_MSAA )
#define GBUFFER_REDUCED_COVERAGE	( RSG_ORBIS || 1 )
// We can`t read & write to/from a target simultaneously on PC DX11.
// We copy off the g-buffer normals into a separate target which is passed into the dynamic decals shaders to avoid this.
#define CREATE_TARGETS_FOR_DYNAMIC_DECALS ((1 && (RSG_PC && __D3D11)))

#if __XENON
struct _D3DRECT;

namespace rage {
	class grcTextureXenon;
}
#endif

namespace rage
{
	class grcRenderTarget;
}

class GBuffer
{
public:

	static void Init();
	static void CreateTargets(int number, int width, int height, int bpp);
#if !__PS3 && !__XENON
	static void DeleteTargets();
#endif
#if __WIN32PC
	static void DeviceLost();
	static void DeviceReset();
#endif

	static void LockSingleTarget(GBufferRT index, bool bLockDepth);
	static void LockTargets();
	static void LockTarget_AmbScaleMod();

	static bool	IsRenderingTo() { return m_IsRenderingTo; }

	static void SetRenderingTo(bool value) { m_IsRenderingTo = value;}

	static void CopyBuffer(GBufferRT index, bool bTopTileOnly=false);

	static void UnlockSingleTarget(GBufferRT index);
	static void UnlockTargets();
	static void UnlockTarget_AmbScaleMod(const bool resolve);

	// Accessors 
#if DYNAMIC_GB
	static inline grcRenderTarget* GetTarget(GBufferRT index) { Assert(index<GBUFFER_DEPTH); return ( sysThreadType::IsRenderThread() && reinterpret_cast<grcRenderTargetDurango*>(m_Target[index])->GetUseAltTarget() ? m_AltTarget[index] : m_Target[index] ); }
#else
	static inline grcRenderTarget* GetTarget(GBufferRT index) { Assert(index<GBUFFER_DEPTH); return m_Target[index]; }
#endif
	static grcRenderTarget* GetDepthTarget();
#if __XENON || __PS3
	static grcRenderTarget* GetDepthTargetAsColor();
#endif

	static inline u32 GetHeight() { return m_Height; }
	static inline u32 GetWidth() { return m_Width; }

#if __XENON
	static inline grcRenderTarget** GetTargets() { return m_Target; }
	static void	CopyDepth();
	static void CopyStencil();
	static void RestoreDepthOptimized(grcRenderTarget* depthTargetAlias, bool bRestore3Bits);
	static void	CalculateTiles(_D3DRECT *pTilingRects, const u32 numTiles);

	static inline grcRenderTarget* Get4xHiZRestore() { return m_4xHiZRestore; }
	static inline grcRenderTarget* Get4xHiZRestoreQuarter() { return m_4xHiZRestoreQuarter; }

	static inline grcRenderTarget* GetTiledTarget(GBufferRT index) { return m_Target_Tiled[index]; }

	static void	RestoreHiZ(bool bUseDepthQuarter = false);
	static void RestoreHiStencil( int stencilRef, bool useEqual=true, bool bUseDepthQuarter = false );

	static void BeginUseHiStencil( int stencilRef, bool useEqual=true );  
	static void EndUseHiStencil( int stencilRef );
#endif	//__XENON

	static void ClearFirstTarget();
	static void ClearAllTargets(bool onlyMeta);

	static void BeginPrimeZBuffer();
	static void EndPrimeZBuffer();

#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
	static grcRenderTarget *CopyGBuffer(GBufferRT index, bool isGBufferResolved);
#if GBUFFER_MSAA
	static inline grcRenderTarget *GetCopyOfTarget(GBufferRT index, bool msaaTarget=false) { return (m_multisample && msaaTarget) ? m_CopyOfMSAATarget[index] : m_CopyOfTarget[index]; }
#else
	static inline grcRenderTarget *GetTarget(GBufferRT index, bool msaaTarget=false) { UNUSED_VAR(msaaTarget); return m_CopyOfTarget[index]; }
#endif
#endif
	static void IncrementAttached(u32 attached) {m_Attached += attached;}

#if GBUFFER_MSAA && DEVICE_EQAA
	static const grcTexture* GetFragmentMaskTexture(GBufferRT index);
#endif

#if DYNAMIC_GB
	static void FlushAltGBuffer(GBufferRT i);
	static void InitAltGBuffer(GBufferRT i);
	static bool UseAltGBufferTest0();
	static bool UseAltGBufferTest1();
	static bool UseAltGBufferTest2();
	static bool UseAltGBufferTest3();
#endif

private:

	static s32 m_numTargets;
	static s32 m_BitsPerPixel;
	static int m_Width;
	static int m_Height;
	static u32 m_Attached;
	static grcRenderTarget* m_Target[MAX_MULTIRENDER_RENDER_TARGETS];
#if DYNAMIC_GB
	static grcRenderTarget* m_AltTarget[MAX_MULTIRENDER_RENDER_TARGETS];
#endif
	static DECLARE_MTR_THREAD bool m_IsRenderingTo;
#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
	static grcRenderTarget* m_CopyOfTarget[MAX_MULTIRENDER_RENDER_TARGETS];
#endif
	
#if GBUFFER_MSAA
	static grcDevice::MSAAMode m_multisample;
#if CREATE_TARGETS_FOR_DYNAMIC_DECALS
	static grcRenderTarget* m_CopyOfMSAATarget[MAX_MULTIRENDER_RENDER_TARGETS];
#endif
#endif // GBUFFER_MSAA

#if __XENON  
	static grcRenderTarget* m_Target_Tiled[MAX_MULTIRENDER_RENDER_TARGETS];
	static grcRenderTarget* m_DepthTarget_Tiled;
	static grcRenderTarget* m_DepthAliases[HACK_GTA4? 3:2];
	static grcRenderTarget* m_4xHiZRestore;
	static grcRenderTarget* m_4xHiZRestoreQuarter;

	static grcDepthStencilStateHandle m_copyDepth_DS;
	static grcRasterizerStateHandle m_copyDepth_R;
	static grcBlendStateHandle m_copyStencil_BS;

	static grcDepthStencilStateHandle m_restoreHiZ_DS;
	static grcRasterizerStateHandle m_restoreHiZ_R;
	static grcDepthStencilStateHandle m_MarkHiStencilState_DS;
	static grcDepthStencilStateHandle m_MarkHiStencilStateNE_DS;
#endif	//__XENON


#if __PPU
	static grcRenderTarget*		m_DepthPatched;
#endif // __PPU

#if RSG_PC
	static bool ms_Initialized;
#endif
};

#endif
