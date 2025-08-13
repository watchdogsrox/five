//
// vfx/sky/SkySettings.h
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SKY_SETTINGS
#define INC_SKY_SETTINGS

// INCLUDES
// rage
#include "grcore/effect_mrt_config.h"
#if __BANK
// For doing the XML saving / loading
#include "parser/tree.h"
#endif

// game
#include "vfx/sky/ShaderVariable.h"
#include "game/Clock.h"
#include "vfx/VisualEffects.h"

// Forward declaration
namespace rage
{
	class grmShader;
}

#define DOME_SCALE 0x469C4000 //u32 for 20000.0f

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//													CShaderVariable													 //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct CSkySettings
{
	// FUNCTIONS
	void Init(grmShader* shader);
	void Shutdown();

	void SendToShader(CVisualEffects::eRenderMode mode);
	void SendSunMoonDirectionAndPosition(Vec3V_In domeOffset);
	void SetPerlinParameters();
	void UpdateRenderThread();

	#if __BANK
	void InitWidgets();
	#endif

	grmShader* m_shader;

	// SHADER VARLIABLES
	// Sky variables
	CShaderVariable<Vec3V> m_azimuthEastColor;
	float m_azimuthEastColorIntensity;
	CShaderVariable<Vec3V> m_azimuthWestColor;
	float m_azimuthWestColorIntensity;
	CShaderVariable<Vec3V> m_azimuthTransitionColor;
	float m_azimuthTransitionColorIntensity;
	CShaderVariable<float> m_azimuthTransitionPosition;

	CShaderVariable<Vec3V> m_zenithColor;
	float m_zenithColorIntensity;
	CShaderVariable<Vec3V> m_zenithTransitionColor;
	float m_zenithTransitionColorIntensity;

	float m_zenithTransitionPosition;
	float m_zenithTransitionEastBlend;
	float m_zenithTransitionWestBlend;
	float m_zenithBlendStart;
	CShaderVariable<Vec4V> m_zenithConstants;

	CShaderVariable<Vec4V> m_skyPlaneColor;
	float m_skyPlaneOffset;
	float m_skyPlaneFogFadeStart;
	float m_skyPlaneFogFadeEnd;
	CShaderVariable<Vec4V> m_skyPlaneParams;

	CShaderVariable<float> m_hdrIntensity;
	float m_reflectionHdrIntensityMult;

	// Sun variables
	bool m_sunPositionOverride; 
	float m_sunAzimuth;
	float m_sunZenith;

	grcEffectVar m_sunDirectionVar;
	grcEffectVar m_sunPositionVar;
	Vec3V m_sunDirection;

	CShaderVariable<Vec3V> m_sunColor;
	CShaderVariable<Vec3V> m_sunColorHdr;
	Vec3V m_sunDiscColor;
	CShaderVariable<Vec3V> m_sunDiscColorHdr;
	float m_sunDiscSize;
	float m_sunHdrIntensity;

	float m_miePhase;
	float m_mieScatter;
	float m_mieIntensityMult;
	CShaderVariable<Vec4V> m_sunConstants;

	// Cloud variables
	CShaderVariable<grcTextureHandle> m_highDetailNoiseTex;
	
	float m_cloudBaseStrength;
	float m_cloudDensityMultiplier;
	float m_cloudDensityBias;
	float m_cloudFadeOut;
	CShaderVariable<Vec4V> m_cloudConstants1;

	float m_cloudShadowStrength;
	float m_cloudOffset;
	float m_cloudOverallDetailColor;
	float m_cloudHdrIntensity;
	CShaderVariable<Vec4V> m_cloudConstants2;

	float m_cloudDitherStrength;
	CShaderVariable<Vec4V> m_cloudConstants3;

	float m_cloudEdgeDetailStrength; 
	float m_cloudEdgeDetailScale;
	float m_cloudOverallDetailStrength;
	float m_cloudOverallDetailScale;
	CShaderVariable<Vec4V> m_cloudDetailConstants;

	Vec3V m_vCloudBaseColour;
	Vec3V m_vCloudShadowColour;
	CShaderVariable<Vec3V> m_cloudBaseMinusMidColour;
	CShaderVariable<Vec3V> m_cloudMidColour;
	CShaderVariable<Vec3V> m_cloudShadowMinusBaseColour;

	float m_smallCloudDetailScale;
	float m_smallCloudDetailStrength;
	float m_smallCloudDensityMultiplier;
	float m_smallCloudDensityBias;
	CShaderVariable<Vec4V> m_smallCloudConstants;

	Vec3V m_vSmallCloudColor;
	CShaderVariable<Vec3V> m_smallCloudColorHdr;

	float m_sunInfluenceRadius;
	float m_sunScatterIntensity;
	float m_moonInfluenceRadius;
	float m_moonScatterIntensity;
	CShaderVariable<Vec4V> m_effectsConstants;

	CShaderVariable<Vec2V> m_noisePhase;
	float m_cloudSpeed;

	float m_smallCloudSpeed;
	float m_overallDetailSpeed;
	float m_edgeDetailSpeed;
	float m_cloudHatSpeed;
	CShaderVariable<Vec3V> m_speedConstants;

	// Misc
	CShaderVariable<float> m_horizonLevel;
	CShaderVariable<grcRenderTarget *> m_perlinRT;
	CShaderVariable<grcTextureHandle> m_ditherTex;
	float m_time;
	Vec3V m_lightningDirection;

	// Night sky
	CShaderVariable<grcTextureHandle> m_starFieldTex;
	CShaderVariable<float> m_starfieldIntensity;
	
	CShaderVariable<grcTextureHandle> m_moonTex;
	CShaderVariable<Vec3V> m_moonPosition;
	CShaderVariable<float> m_moonIntensity;
	CShaderVariable<Vec3V> m_moonColor;
	float m_moonDiscSize;
	CShaderVariable<Vec3V> m_lunarCycle;

	// Moon direction is a special case like sun direction.
	grcEffectVar m_moonDirectionVar;
	grcEffectVar m_moonPositionVar;
	Vec3V m_moonDirection;
	
	// Noise
	CShaderVariable<float> m_noiseFrequency;
	CShaderVariable<float> m_noiseScale;
	CShaderVariable<float> m_noiseThreshold;
	CShaderVariable<float> m_noiseSoftness;
	CShaderVariable<float> m_noiseDensityOffset;
	CShaderVariable<grcTextureHandle> m_noiseBaseTex;
};

#endif
