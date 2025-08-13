//
// filename:	VisualEffects.h
// description:	
//
#ifndef INC_VISUALEFFECTS_H_
#define INC_VISUALEFFECTS_H_

#include "grcore/config.h"

// --- Structure/Class Definitions ----------------------------------------------

class CVisualEffects
{
public:
	enum eRenderMode
	{
		RM_DEFAULT = 0,
		RM_MIRROR_REFLECTION,
		RM_WATER_REFLECTION,
		RM_CUBE_REFLECTION,
		RM_SEETHROUGH,
		RM_NUM
	};

	enum eMarkerMask
	{
		MM_NONE = 0,
		MM_MARKERS = 1,
		MM_GLOWS = 2,
		MM_SCRIPT = 4,
		MM_SCALEFORM = 8,
		MM_ALL = 0xFF,
	};

	static void Init(unsigned initMode);
	static void InitDLCCommands();
	static void InitDefinitionsPreRpfs();
	static void InitDefinitionsPostRpfs();
	static void InitTunables();
	static void Shutdown(unsigned shutdownMode);

	static void PreUpdate();
	static void Update();
	static void UpdateAfterPreRender();
	static void UpdateSafe();

	static void SetupRenderThreadFrameInfo();
	static void ClearRenderThreadFrameInfo();

	static void RenderDistantLights(eRenderMode mode, u32 renderFlags, float intensityScale);
	static void RenderBeforeWater(u32 renderFlags);
	static void RenderAfter(u32 renderFlags);
	static void RenderLateInterior(u32 renderFlags);
	static void RenderLateVehicleInterior(u32 renderFlags);
	static void ResetDecalsForwardContext();
	static void RenderIntoWaterRefraction(u32 renderFlags);
	static void RenderSky(eRenderMode mode, bool bUseStencilMask, bool bUseRestrictedProjection, int scissorX, int scissorY, int scissorW, int scissorH, bool bFirstCallThisDrawList, float globalCloudAlpha);
#if __D3D11
	static void SetTargetsForDX11ReadOnlyDepth(bool needsCopy, grcRenderTarget* depthAndStencilBuffer, grcRenderTarget* depthBufferCopy, grcRenderTarget* depthAndStencilBufferReadOnly);
#endif // __D3D11
	static void RenderMarkers(const eMarkerMask mask);
	static void RenderBrightLights();
	static void RenderCoronas(eRenderMode mode, float sizeScale, float intensityScale);
	static float GetSunVisibility();

	static void RenderLensFlare(bool cameraCut);
	static void IssueLensFlareOcclusionQuerries();
	static void RegisterLensFlares();

	static void RenderForwardEmblems();

	static void	AddNightVisionLight();
	static void OverrideNightVisionLightRange(float r);

	static void UpdateVisualDataSettings();
	
#if __BANK
	static void InitWidgets();
	static void CreateWidgets();
	static void RenderDebug();

	static void DisableAllVisualEffects() { sm_disableVisualEffects = true; }
	static void EnableAllVisualEffects() { sm_disableVisualEffects = false; }

	static bool sm_disableVisualEffects;
#endif

private:
#if __D3D11
	class DX11_READ_ONLY_DEPTH_TARGETS
	{
	public:
		DX11_READ_ONLY_DEPTH_TARGETS()
		{
			needsCopy = false;
			depthAndStencilBuffer = NULL;
			depthBufferCopy = NULL;
			depthAndStencilBufferReadOnly = NULL;
		}
		~DX11_READ_ONLY_DEPTH_TARGETS() 
		{
		}
	public:
		bool needsCopy;
		grcRenderTarget* depthAndStencilBuffer;
		grcRenderTarget* depthBufferCopy;
		grcRenderTarget* depthAndStencilBufferReadOnly;
	};
	static DX11_READ_ONLY_DEPTH_TARGETS ms_DX11ReadOnlyDepths[NUMBER_OF_RENDER_THREADS];
#endif //__D3D11
};

// --- Globals ------------------------------------------------------------------


#endif // !INC_VISUALEFFECTS_H_
