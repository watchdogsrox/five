//
// vfx/sky/Sky.h
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SKY
#define INC_SKY

// INCLUDES
// rage
#include "fwscene/stores/dwdstore.h" // For loading model

// game
#include "vfx/sky/ShaderVariable.h"
#include "vfx/sky/SkySettings.h"
#include "vfx/VisualEffects.h"
#include "vectormath/legacyconvert.h"

// Forward declaration
namespace rage
{
	class SkyDome;
	class grmShader;
	class grcTexture;
}
class gtaDrawable;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//															CSky													 //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSky
{
public:
	enum RenderType {
		RT_SKY = 0,
		RT_MOON,
		RT_SUN,
		RT_NUM
	};

	enum PassFlags {
		PF_STARS		= (1 << 0),
		PF_CLOUDS		= (1 << 1),
		PF_MOON			= (1 << 2),
		PF_SUN			= (1 << 3),
#if __BANK
		PF_DEBUG_CLOUDS	= (1 << 4),
#endif // __BANK
	};

	// CONSTRUCTORS / DESTRUCTORS
	CSky();
	~CSky();

	// FUNCTIONS
	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	void Render(CVisualEffects::eRenderMode mode, bool bUseStencilMask, bool bUseRestrictedProjection, int scissorX, int scissorY, int scissorW, int scissorH, bool bFirstCallThisDrawList=true);
	void Render(CVisualEffects::eRenderMode mode, bool bUseStencilMask, bool bUseRestrictedProjection, bool bFirstCallThisDrawList=true) { Render(mode, bUseStencilMask, bUseRestrictedProjection, -1, -1, -1, -1, bFirstCallThisDrawList); }
	static void PreRender();
	void UpdateSettings();
	static void Update();
	void UpdateVisualSettings();

	void GeneratePerlin();
	grcTexture* GetPerlinTexture();
	grcTexture* GetHighDetailNoiseTexture(); // cloud shadows 2
	float GetCloudDensityMultiplier();
	float GetCloudDensityBias();
	const Vec4V* GetCloudShaderConstants(Vec4V v[6], int n, float largeCloudAmount, float smallCloudAmount); // cloud shadows 2

	bool ShouldRenderLargeClouds();
	bool ShouldRenderSmallClouds();

	#if __BANK
	void InitWidgets();
	static void ReloadSkyDome(CSky *instance);
	#endif

	// Get the current sun position
	Vec3V_Out GetSunPosition() 
	{ 
		return AddScaled(grcViewport::GetCurrent()->GetCameraPosition(), m_skySettings.m_sunDirection, ScalarVConstant<DOME_SCALE>());
	}

	Vec3V_Out GetSunDirection() { return m_skySettings.m_sunDirection; }

	float GetSunRadius() { return m_skySettings.m_sunDiscSize * m_fSunDiscSizeMul; }
	float GetMoonPhaseIntensity() { return m_skySettings.m_lunarCycle.getValue().GetYf()*0.5f+0.5f; }

	// Get the current sun color
	void GetSunColor(Color32& sunColor) { sunColor = m_skySettings.m_sunColor.getValue(); }

	grcTextureHandle GetDitherTexture() {return m_skySettings.m_ditherTex.getValue();}

	float GetCloudHatSpeed() { return m_skySettings.m_cloudHatSpeed; } 

private:

	void LoadSkyDomeModel();
	void ReloadSkyDomeModel();
	void skyPlane(const u32 numTris);

	// VARIABLES
	grcEffectTechnique m_techniques[CVisualEffects::RM_NUM][RT_NUM];
	grcEffectTechnique m_perlinNoiseTechniqueIdx;
	grcEffectTechnique m_skyPlaneTechniqueIdx;
	grmShader *m_skyShader;
	Dwd *m_skyDomeDictionary;
	CSkySettings m_skySettings;
	gtaDrawable *m_pDrawable;
	int m_overwriteTimeCycle;

	grcBlendStateHandle m_skyMoonSun_B;
	grcDepthStencilStateHandle m_skyReflectionUseStencilMask_DS; // same as m_skyStandard_DS for PS3
	grcDepthStencilStateHandle m_skyStandard_DS;
	grcDepthStencilStateHandle m_skyPlane_DS;
	grcDepthStencilStateHandle m_skyCubeReflection_DS;

	/**
	 * Additional size multiplier for the suns radius.
	 */
	float m_fSunDiscSizeMul;

#if __BANK
	bool m_overridePassSelection;
	bool m_RenderSky;
	
	/**
	 * Turn on/off the code that forces the domes depth to the far planes value in the default mode.
	 */
	bool m_bEnableForcedDepth;

	bool m_bMirrorForceInfiniteProjectionON;
	bool m_bMirrorForceInfiniteProjectionOFF;
	float m_mirrorInfiniteProjectionEpsilonExponent;
	bool m_bMirrorForceSetClipON;
	bool m_bMirrorForceSetClipOFF;
	bool m_bMirrorEnableSkyPlane;

	enum
	{
		CLOUD_DEBUGGING_MODE_DENSITY,
		CLOUD_DEBUGGING_MODE_WIREFRAME,
		CLOUD_DEBUGGING_MODE_PERLIN_X,
		CLOUD_DEBUGGING_MODE_PERLIN_Z,
		CLOUD_DEBUGGING_MODE_PERLIN_TEXCOORD,
		CLOUD_DEBUGGING_MODE_PERLIN_OFFSET_X,
		CLOUD_DEBUGGING_MODE_PERLIN_OFFSET_Y,
		CLOUD_DEBUGGING_MODE_PERLIN_OFFSET_TEXCOORD,
		CLOUD_DEBUGGING_MODE_DETAIL_EDGE_X,
		CLOUD_DEBUGGING_MODE_DETAIL_EDGE_X_TEXCOORD,
		CLOUD_DEBUGGING_MODE_DETAIL_EDGE_Y,
		CLOUD_DEBUGGING_MODE_DETAIL_EDGE_Y_TEXCOORD,
		CLOUD_DEBUGGING_MODE_DETAIL_OVERALL,
		CLOUD_DEBUGGING_MODE_DETAIL_OVERALL_TEXCOORD,
		CLOUD_DEBUGGING_MODE_DITHER,
		CLOUD_DEBUGGING_MODE_DITHER_TEXCOORD,
		CLOUD_DEBUGGING_MODE_COUNT
	};
	bool m_enableCloudDebugging;
	int m_cloudDebuggingMode;
	float m_cloudDebuggingTexcoordScale;
	grcEffectVar m_cloudDebuggingParamsId;
#endif
};

// EXTERNS
extern CSky g_sky;

#endif // INC_SKY
