//
// filename:	RenderPhaseDefLight.h
// description:	renderphase classes
//

#ifndef INC_RENDERPHASEDEFLIGHT_H_
#define INC_RENDERPHASEDEFLIGHT_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "renderer/DrawLists/drawListMgr_config.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "renderer/SpuPm/SpuPmMgr.h"

#include "shader_source/Terrain/terrain_cb_switches.fxh"

#if TERRAIN_TESSELLATION_SUPPORT

class TerrainTessellationParameters
{
public:
	TerrainTessellationParameters() :
	m_PixelsPerVertex(40),
	m_WorldTexelUnit(1.0f),
	m_NearNoise(0.0f, 0.02f),
	m_FarNoise(-0.1f, 0.1f),
	m_Envelope(4.0, 5.0f, 50.0f, 70.0f),
	m_UseNearWEnd(20.0f)
	{
	}
	~TerrainTessellationParameters()
	{
	}
public:
	int m_PixelsPerVertex;
	float m_WorldTexelUnit;
	Vector2 m_NearNoise;
	Vector2 m_FarNoise;
	Vector4 m_Envelope;
	float m_UseNearWEnd;
	PAR_SIMPLE_PARSABLE;
};

#endif //TERRAIN_TESSELLATION_SUPPORT


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// name:		CRenderPhaseDeferredLighting_SceneToGBuffer
// description:	Under deferred lighting render the scene into the GBuffer.
class CRenderPhaseDeferredLighting_SceneToGBuffer : public CRenderPhaseScanned
{
public:
	CRenderPhaseDeferredLighting_SceneToGBuffer(CViewport *pGameViewport);
	virtual ~CRenderPhaseDeferredLighting_SceneToGBuffer();

	virtual void BuildRenderList();

	virtual void UpdateViewport();

	virtual void BuildDrawList();
#if DRAW_LIST_MGR_SPLIT_GBUFFER
	void StartDrawListOfType(int drawListType);
	void EndDrawListOfType(int drawListType);
#endif // DRAW_LIST_MGR_SPLIT_GBUFFER

	virtual void ConstructRenderList();

	virtual u32 GetCullFlags() const;

#if __BANK
	virtual void AddWidgets(bkGroup& bank);
#endif // __BANK

	float ComputeLodDistanceScale() const;

#if RSG_PC
	void StorePortalVisTrackerForReset();
#endif

public:

#if TERRAIN_TESSELLATION_SUPPORT

	// Return linear mapping from (a, b)->(c, d) as a linear function of the form Ax + B.
	void ComputeMapping(float a, float b, float c, float d, float &A, float &B)
	{
		A = (d - c)/(b - a);
		B = -a*(d - c)/(b - a) + c;
	}

	void InitTerrainGlobals(u32 width, u32 height, u32 depth);
	void CleanUpTerrainGlobals();
	void SetTerrainGlobals(const grcViewport &viewport);
	static void LoadTerrainTessellationData();
#if __BANK
	static void AddTerrainWidgets(bkBank &bank);
	static void SaveTerrainTessellationData();
#endif //__BANK

	// Tess factor, world units per texels (frequency essentially), envelope near, envelope far. 
	grcEffectGlobalVar m_TerrainTessellation0Var; 
	// Mapping from [envelope[0], envelope[1]]->[0,1] and [envelope[2], envelope[3]]->[1,0],
	// (meaning the "near" ramp up then the "far" ramp down).
	grcEffectGlobalVar m_TerrainNearAndFarRampVar;
	// Near: mapping from [-1, 1] to [noise min, noise max].
	grcEffectGlobalVar m_TerrainNearNoiseAmplitudeVar;
	// Far: mapping from [-1, 1] to [noise min, noise max].
	grcEffectGlobalVar m_TerrainFarNoiseAmplitudeVar;
	// Global blend (envelope[0], envelope[3])->[0,1] plus vertex densities (vertices per metre - used along edges).
	grcEffectGlobalVar m_TerrainGlobalBlendAndProjectionInfoVar;
	// World space equation of camera w=0 plane (to calculate scene depth in normal + shadow pass).
	grcEffectGlobalVar m_TerrainCameraWEqualsZeroPlaneVar;
	// W used for tessellation factors remain fixed (at envelope[0]) until UseWNearEnd, then ramps up to the true value over the envelope.
	// W modification:- mapping from [UseNearWEnd, envelope[3]]->[envelope[0], envelope[3]].
	grcEffectGlobalVar m_TerrainWModificationVar;

	// Texture used as a look up in the noise function.
	grcTexture *m_pTerrainPerlinNoiseTexture;
	grcEffectGlobalVar m_TerrainPerlinNoiseTextureVar;

	/// Width, Height and Depth of Perlin Noise Texture.
	grcEffectGlobalVar m_TerrainPerlinNoiseTextureParamVar;

	static bool ms_TerrainTessellationOn;
	static TerrainTessellationParameters ms_TerrainTessellationParams;
#endif // TERRAIN_TESSELLATION_SUPPORT

#if RSG_PC
	static CPortalVisTracker* ms_pPortalVisTrackerSavedDuringReset;
#endif
};

class CRenderPhaseDeferredLighting_LightsToScreen : public CRenderPhase
{
public:
	CRenderPhaseDeferredLighting_LightsToScreen(CViewport* pGameViewport);

	virtual void BuildRenderList();
	virtual void BuildDrawList();

	void SetGBufferRenderPhase(CRenderPhaseScanned* pPhase){m_pGBufferPhase = pPhase;}

private:
	CRenderPhaseScanned*		m_pGBufferPhase;
};

#if SPUPMMGR_PS3
class CRenderPhaseDeferredLighting_SSAOSPU : public CRenderPhase
{
public:
	CRenderPhaseDeferredLighting_SSAOSPU(CViewport* pGameViewport);
	virtual void BuildDrawList();
};
#endif

#endif // !INC_RENDERPHASEDEFLIGHT_H_
