//
// vfx/sky/SkySettings.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// INCLUDES
#include "Vfx/Sky/SkySettings.h"

// rage
#include "grcore/texture.h"
#include "fwsys/timer.h"
#include "parser/manager.h"
#include "pheffects/wind.h"
#include "system/nelem.h"
#include "system/threadtype.h"

#if __BANK
#include "bank/widget.h"
#include "bank/bkmgr.h"
#endif

// framework
#include "vfx/vfxwidget.h"

// game
#include "vfx/vfx_channel.h"
#include "vfx/VfxSettings.h"
#include "renderer/rendertargets.h"
#include "timecycle/TimeCycleConfig.h"
#include "timecycle/TimeCycle.h"
#include "game/Clock.h"

VFX_MISC_OPTIMISATIONS()

	BankFloat	g_NoisePhaseMult = 0.01f;
	BankBool	g_EnableWindEffectsForSkyClouds = true;
	BankBool	g_UseGustWindForSkyClouds = false;
	BankBool	g_UseGlobalVariationWindForSkyClouds = false;

#if __BANK
	bool		g_IsOverrideWindDirection = false;
	Vec2V		g_overrideWindDirection(V_ZERO);
	float		g_moonCycleDebug = 0.0f;
#endif

// ------------------------------------------------------------------------------------------------------------------//
//														Init														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::Init(grmShader* shader)
{
	m_shader = shader;

	// Sky variables
	m_azimuthEastColor.setNameAndValue(m_shader, "azimuthEastColor", Vec3V(1.000000f, 0.619608f, 0.011765f));
	m_azimuthEastColorIntensity = 1.0f;
	m_azimuthWestColor.setNameAndValue(m_shader, "azimuthWestColor", Vec3V(0.121569f, 0.509804f, 0.870588f));
	m_azimuthWestColorIntensity = 1.0f;
	m_azimuthTransitionColor.setNameAndValue(m_shader, "azimuthTransitionColor", Vec3V(0.211765f, 0.654902f, 0.890196f));
	m_azimuthTransitionColorIntensity = 1.0f;
	m_azimuthTransitionPosition.setNameAndValue(m_shader, "azimuthTransitionPosition", 0.5f);

	m_zenithColor.setNameAndValue(m_shader, "zenithColor", Vec3V(0.211765f, 0.349020f, 0.470588f));
	m_zenithColorIntensity = 1.0f;
	m_zenithTransitionColor.setNameAndValue(m_shader, "zenithTransitionColor", Vec3V(0.207843f, 0.584314f, 0.800000f));
	m_zenithTransitionColorIntensity = 1.0f;
	m_zenithConstants.setName(m_shader, "zenithConstants");
	m_zenithTransitionPosition = 0.8f;
	m_zenithTransitionEastBlend = 0.5f;
	m_zenithTransitionWestBlend = 0.5f;
	m_zenithBlendStart = 0.0f;

	m_skyPlaneColor.setNameAndValue(m_shader, "skyPlaneColor", Vec4V(V_ZERO));
	m_skyPlaneOffset = g_visualSettings.Get("skyplane.offset", 512.0f);
	m_skyPlaneFogFadeStart = g_visualSettings.Get("skyplane.fog.fade.start", 64.0f);
	m_skyPlaneFogFadeEnd = g_visualSettings.Get("skyplane.fog.fade.end", 128.0f);
	m_skyPlaneParams.setName(m_shader, "skyPlaneParams");

	m_hdrIntensity.setNameAndValue(m_shader, "hdrIntensity", 7.0f);

	// Sun variables
	m_sunAzimuth = 0.0f;
	m_sunZenith = 0.01f;
	m_sunPositionOverride = false;
	
	m_sunDirectionVar = m_shader->LookupVar("sunDirection");
	m_sunPositionVar = m_shader->LookupVar("sunPosition");

	m_sunColor.setNameAndValue(m_shader, "sunColor", Vec3V(V_ONE));
	m_sunColorHdr.setNameAndValue(m_shader, "sunColorHdr", Vec3V(V_TEN));
	m_sunDiscColor = Vec3V(V_ONE);
	m_sunDiscColorHdr.setNameAndValue(m_shader, "sunDiscColorHdr", Vec3V(V_TEN));
	m_sunDiscSize = 2.5f;
	m_sunHdrIntensity = 10.0f;
	m_miePhase = 1.0f;
	m_mieScatter = 1.0f;
	m_mieIntensityMult = 1.0f;
	m_sunConstants.setNameAndValue(m_shader, "sunConstants", Vec4V(V_ZERO));

	// Cloud variables
	m_cloudConstants1.setName(m_shader, "cloudConstants1");
	m_cloudBaseStrength = 0.9f;
	m_cloudDensityMultiplier = 2.24f;
	m_cloudDensityBias = 0.66f;
	m_cloudFadeOut = 0.3f;

	m_cloudConstants2.setName(m_shader, "cloudConstants2");
	m_cloudShadowStrength = 0.2f;
	m_cloudOffset = 0.1f;
	m_cloudOverallDetailColor = 0.5f;
	m_cloudHdrIntensity = 0.8f;

	m_cloudConstants3.setName(m_shader, "cloudConstants3");
	m_cloudDitherStrength = 50.0f;

	m_cloudDetailConstants.setName(m_shader, "cloudDetailConstants");
	m_cloudEdgeDetailStrength = 0.50f;
	m_cloudEdgeDetailScale = g_visualSettings.Get("cloudgen.edge.detail.scale", 16.0f);
	m_cloudOverallDetailStrength = 0.50f;
	m_cloudOverallDetailScale = g_visualSettings.Get("cloudgen.overlay.detail.scale", 8.0f);

	m_vCloudBaseColour =  Vec3V(0.75f, 0.75f, 0.75f);
	m_vCloudShadowColour =  Vec3V(0.8f, 0.8f, 0.8f);
	m_cloudMidColour.setNameAndValue(m_shader, "cloudMidColour", Vec3V(0.95f, 0.95f, 0.95f));
	m_cloudShadowMinusBaseColour.setNameAndValue(m_shader, "cloudShadowMinusBaseColourTimesShadowStrength", Scale(Subtract(m_vCloudShadowColour*m_vCloudShadowColour, m_vCloudBaseColour*m_vCloudBaseColour), ScalarVFromF32(m_cloudShadowStrength)));
	m_cloudBaseMinusMidColour.setNameAndValue(m_shader, "cloudBaseMinusMidColour", Subtract(m_vCloudBaseColour*m_vCloudBaseColour, m_cloudMidColour.m_value*m_cloudMidColour.m_value));

	m_smallCloudConstants.setName(m_shader, "smallCloudConstants");
	m_smallCloudDetailScale = 3.6f;
	m_smallCloudDetailStrength = 0.5f;
	m_smallCloudDensityMultiplier = 1.7f;
	m_smallCloudDensityBias = 0.558f;

	m_vSmallCloudColor = Vec3VFromF32(0.9f);
	m_smallCloudColorHdr.setNameAndValue(m_shader, "smallCloudColorHdr", Scale(m_vSmallCloudColor, ScalarVFromF32(m_cloudHdrIntensity)) );

	// Effects variables
	m_sunInfluenceRadius = 1.0;
	m_sunScatterIntensity = 2.0;
	m_moonInfluenceRadius = 1.0;
	m_moonScatterIntensity = 2.0;
	m_effectsConstants.setName(m_shader, "effectsConstants");

	// Noise
	m_noiseFrequency.setNameAndValue(m_shader, "noiseFrequency", g_visualSettings.Get("cloudgen.frequency", 2.0f));
	m_noiseScale.setNameAndValue(m_shader, "noiseScale", g_visualSettings.Get("cloudgen.scale", 48.0f));
	m_noiseThreshold.setNameAndValue(m_shader, "noiseThreshold", 0.65f);
	m_noiseSoftness.setNameAndValue(m_shader, "noiseSoftness", 0.2f);
	m_noiseDensityOffset.setNameAndValue(m_shader, "noiseDensityOffset", 0.15f);
	m_noisePhase.setNameAndValue(m_shader, "noisePhase", Vec2V(V_ONE));

	// Speed variables
	m_cloudSpeed = g_visualSettings.Get("cloud.speed.large", 5.0f);
	m_smallCloudSpeed = g_visualSettings.Get("cloud.speed.small", 1.0f);
	m_overallDetailSpeed = g_visualSettings.Get("cloud.speed.overall", 1.0f);
	m_edgeDetailSpeed = g_visualSettings.Get("cloud.speed.edge", 1.0f);
	m_cloudHatSpeed = g_visualSettings.Get("cloud.speed.hats", 1.0f);
	m_speedConstants.setName(m_shader, "speedConstants");

	// Misc variables
	m_horizonLevel.setNameAndValue(m_shader, "horizonLevel", g_vfxSettings.GetWaterLevel());
	m_lightningDirection = Vec3V(V_Z_AXIS_WZERO);

	// Night variables
	m_starFieldTex.setNameAndValue(m_shader, "starFieldTex", grcTextureFactory::GetInstance().Create("starfield"));
	m_starfieldIntensity.setNameAndValue(m_shader, "starfieldIntensity", 1.0f);
	m_moonTex.setNameAndValue(m_shader, "moonTex", grcTextureFactory::GetInstance().Create("moon-new"));
	
	m_moonDirectionVar = m_shader->LookupVar("moonDirection");
	m_moonPositionVar = m_shader->LookupVar("moonPosition"); 

	m_moonIntensity.setNameAndValue(m_shader, "moonIntensity", 1.0f);
	m_lunarCycle.setName(m_shader, "lunarCycle");
	m_moonColor.setNameAndValue(m_shader, "moonColor", Vec3V(V_ONE));
	m_moonDiscSize = 2.5f;

	// Perlin noise texture
	grcTextureFactory::CreateParams params = grcTextureFactory::CreateParams();
	params.Multisample = 0;
	params.Format = grctfA8R8G8B8;
	params.MipLevels = 1;

	#if __XENON
		grcRenderTarget *perlin = CRenderTargets::CreateRenderTarget(
			XENON_RTMEMPOOL_PERLIN_NOISE,  
			"PERLIN_NOISE_MAP",	
			grcrtPermanent,		
			512, 
			512, 
			32,
			&params,
			kPerlinNoiseHeap);
		perlin->AllocateMemoryFromPool();
	#else
		params.PoolID = kRTPoolIDAuto;
		params.AllocateFromPoolOnCreate = true;

		grcRenderTarget *perlin = grcTextureFactory::GetInstance().CreateRenderTarget("PERLIN_NOISE_MAP",	
			grcrtPermanent,		
			512, 
			512, 
			32, 
			&params);
	#endif

	m_perlinRT.setNameAndValue(m_shader, "perlinNoiseRT", perlin);

	m_noiseBaseTex.setNameAndValue(m_shader, "noiseTexture", grcTextureFactory::GetInstance().Create("basePerlinNoise3Channel"));
	
	// Load noise / dither textures
	m_highDetailNoiseTex.setNameAndValue(m_shader, "highDetailNoiseTex", grcTextureFactory::GetInstance().Create("NOISE16_p"));
	m_ditherTex.setNameAndValue(m_shader, "ditherTex", grcTextureFactory::GetInstance().Create("dither"));
}

// ------------------------------------------------------------------------------------------------------------------//
//													Shutdown														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::Shutdown()
{
}

// ------------------------------------------------------------------------------------------------------------------//
//											 SendSunMoonDirection													 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::SendSunMoonDirectionAndPosition(Vec3V_In domeOffset)
{
	// Adjust direction / position to be correct
	Vec3V sunDir, moonDir;

	const ScalarV scale = ScalarVConstant<DOME_SCALE>(); //u32 for 20000.0f

	moonDir = AddScaled(domeOffset, m_moonDirection, scale);
	sunDir  = AddScaled(domeOffset, m_sunDirection,  scale);
	
	m_shader->SetVar(m_sunPositionVar, sunDir);
	m_shader->SetVar(m_moonPositionVar, moonDir);

	if(g_timeCycle.GetLightningOverrideRender())
	{
		moonDir = sunDir = m_lightningDirection;
	}
	else
	{
		moonDir = Normalize(moonDir);
		sunDir = Normalize(sunDir);
	}

	m_shader->SetVar(m_sunDirectionVar, sunDir);
	m_shader->SetVar(m_moonDirectionVar, moonDir);
}

// ------------------------------------------------------------------------------------------------------------------//
//												SendToShader														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::SendToShader(CVisualEffects::eRenderMode mode)
{
	const float hdrMult = (mode == CVisualEffects::RM_CUBE_REFLECTION) ? m_reflectionHdrIntensityMult : 1.0f;

	m_sunColorHdr.m_value = m_sunColor.m_value * m_sunColor.m_value * ScalarVFromF32(m_sunHdrIntensity * hdrMult);
	m_sunDiscColorHdr.m_value = m_sunDiscColor * m_sunDiscColor * ScalarVFromF32(m_sunHdrIntensity * hdrMult);

	// Sky related
	m_azimuthEastColor.sendToShaderLinearMult(m_shader, m_azimuthEastColorIntensity);
	m_azimuthWestColor.sendToShaderLinearMult(m_shader, m_azimuthWestColorIntensity);
	m_azimuthTransitionColor.sendToShaderLinearMult(m_shader, m_azimuthTransitionColorIntensity);
	m_azimuthTransitionPosition.sendToShader(m_shader);

	m_zenithColor.sendToShaderLinearMult(m_shader, m_zenithColorIntensity);
	m_zenithTransitionColor.sendToShaderLinearMult(m_shader, m_zenithTransitionColorIntensity);
	m_zenithConstants.sendToShader(m_shader);

	m_skyPlaneColor.sendToShader(m_shader);
	m_skyPlaneParams.sendToShader(m_shader);

	m_hdrIntensity.sendToShaderMult(m_shader, hdrMult);
	
	// Sun related
	m_sunColor.sendToShaderLinear(m_shader);
	
	m_sunColorHdr.sendToShader(m_shader);
	m_sunDiscColorHdr.sendToShader(m_shader);
	m_sunConstants.sendToShader(m_shader);

	// Cloud related
	m_highDetailNoiseTex.sendToShader(m_shader);

	m_cloudConstants1.sendToShader(m_shader);
	Vec4V cloudConstants2Mult(1.0f, 1.0f, 1.0f, hdrMult);
	m_cloudConstants2.sendToShaderMultV(m_shader, cloudConstants2Mult);
	m_cloudConstants3.sendToShader(m_shader);

	m_cloudDetailConstants.sendToShader(m_shader);

	m_smallCloudConstants.sendToShader(m_shader);
	m_smallCloudColorHdr.sendToShaderMult(m_shader, hdrMult);

	m_cloudBaseMinusMidColour.sendToShader(m_shader);
	m_cloudMidColour.sendToShaderLinear(m_shader);
	m_cloudShadowMinusBaseColour.sendToShader(m_shader);

	// Effects variables
	Vec4V effectConstantsMult(1.0f, hdrMult, 1.0f, 1.0f);
	m_effectsConstants.sendToShaderMultV(m_shader, effectConstantsMult);

	// Night
	m_starFieldTex.sendToShader(m_shader);
	m_starfieldIntensity.sendToShader(m_shader);
	m_moonTex.sendToShader(m_shader);
	m_moonIntensity.sendToShader(m_shader);
	m_lunarCycle.sendToShader(m_shader);
	m_moonColor.sendToShaderLinear(m_shader);

	// Speed
	m_speedConstants.sendToShader(m_shader);

	// Misc
	m_horizonLevel.sendToShader(m_shader);
	m_ditherTex.sendToShader(m_shader);

	// these are needed for both perlin render target generation and sky rendering. set them again here, in case the perlin RT was generated on a different thread than the sky
	m_perlinRT.sendToShader(m_shader);
	m_noiseSoftness.sendToShader(m_shader);
	m_noiseThreshold.sendToShader(m_shader);
}

// ------------------------------------------------------------------------------------------------------------------//
//											SetPerlinParameters														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::SetPerlinParameters()
{
	m_perlinRT.sendToShader(m_shader);

	if(!fwTimer::IsGamePaused())
	{
		if(g_EnableWindEffectsForSkyClouds)
		{
			//Change the noise phase based on the wind 
			Vec3V windVelocity3V;
			WIND.GetGlobalVelocity(WIND_TYPE_AIR, windVelocity3V, g_UseGustWindForSkyClouds, g_UseGlobalVariationWindForSkyClouds);
			Vec2V vCurrAirVel = windVelocity3V.GetXY(); 
			// get the time step
			float deltaTime = fwTimer::GetTimeStep();
#if __BANK
			if(g_IsOverrideWindDirection)
			{
				vCurrAirVel = g_overrideWindDirection;
			}
#endif


			vCurrAirVel = NormalizeSafe(vCurrAirVel, Vec2V(V_X_AXIS_WZERO));
			Vec2V vNewValue = m_noisePhase.getValue() + vCurrAirVel * ScalarVFromF32(m_cloudSpeed * g_NoisePhaseMult * g_NoisePhaseMult * deltaTime);
			Vector2 newValue = RCC_VECTOR2(vNewValue);

			newValue.x = fmod(newValue.x, 1.0f);
			newValue.y = fmod(newValue.y, 1.0f);

			m_noisePhase.setValue(Vec2V(newValue.x, newValue.y));

			skyDebugf3("CSkySettings::Update() -- noisePhase = %f, %f\n", VEC2V_ARGS(m_noisePhase.getValue()));
		}
		else
		{
			m_noisePhase.setValue(Vec2V(ScalarVFromF32(m_cloudSpeed * m_time * 0.24f))); //multiplying 0.01f from shader
		}

	}

	m_noiseFrequency.sendToShader(m_shader);
	m_noiseScale.sendToShader(m_shader);
	m_noiseThreshold.sendToShader(m_shader);
	m_noiseSoftness.sendToShader(m_shader);
	m_noiseDensityOffset.sendToShader(m_shader);

	m_noiseBaseTex.sendToShader(m_shader);
	m_noisePhase.sendToShader(m_shader);
}

// ------------------------------------------------------------------------------------------------------------------//
//											UpdateRenderThread														 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::UpdateRenderThread()
{
	Assert(sysThreadType::IsRenderThread());

	// If sun position is over ridden then calculate position else
	// grab it from time-cycle
	if (m_sunPositionOverride)
	{
		float zenithAngle = (m_sunZenith * 0.5f + 0.5f) * PI;
		float azimuthAngle = m_sunAzimuth * 2.0f * PI - (PI * 0.5f);

		m_sunDirection = -Vec3V(sinf(zenithAngle) * cosf(azimuthAngle),
		                        sinf(zenithAngle) * sinf(azimuthAngle),
		                        cosf(zenithAngle));

		m_moonDirection = -m_sunDirection;
	}

	// Store zenith constants
	m_zenithConstants.setValue(Vec4V(m_zenithTransitionPosition,
									   m_zenithTransitionEastBlend,
									   m_zenithTransitionWestBlend,
									   1.0f - m_zenithBlendStart));

	// Store sun constants
	const float g = -0.49f - (m_miePhase / 2.0f), 
				g2 = g * g,
				scatter = m_mieScatter / 1000.0f;

	m_sunConstants.setValue(Vec4V(g * 2.0f, // phase * 2
									g2 + 1.0f, // phase squared plus one
									scatter * (1.5f * ((1.0f - g2) / (2.0f + g2))), // constant multiplier times scatter
									m_mieIntensityMult)); 

	// Cloud detail constants
	m_cloudDetailConstants.setValue(Vec4V(m_cloudEdgeDetailStrength,
											m_cloudEdgeDetailScale,
											m_cloudOverallDetailStrength,
											m_cloudOverallDetailScale));

	m_cloudConstants1.setValue(Vec4V(m_cloudBaseStrength,
									   m_cloudDensityMultiplier,
									   m_cloudDensityBias,
									   m_cloudFadeOut));

	m_cloudConstants2.setValue(Vec4V(m_cloudShadowStrength,
									   m_cloudOffset,
									   m_cloudOverallDetailColor,
									   m_cloudHdrIntensity));

	m_cloudConstants3.setValue(Vec4V(m_cloudDitherStrength / 1024.0f,
									   0.0f,
									   0.0f,
									   0.0f));

	m_smallCloudConstants.setValue(Vec4V(m_smallCloudDetailScale,
										   m_smallCloudDetailStrength,
									  	   m_smallCloudDensityMultiplier,
										   m_smallCloudDensityBias));

	m_cloudShadowMinusBaseColour.setValue(Scale(Subtract(m_vCloudShadowColour*m_vCloudShadowColour, m_vCloudBaseColour*m_vCloudBaseColour), ScalarVFromF32(m_cloudShadowStrength)));
	m_cloudBaseMinusMidColour.setValue(Subtract(m_vCloudBaseColour*m_vCloudBaseColour, m_cloudMidColour.m_value*m_cloudMidColour.m_value));
	m_smallCloudColorHdr.setValue(Scale(m_vSmallCloudColor, ScalarVFromF32(m_cloudHdrIntensity)));

	// Effects variables
	m_effectsConstants.setValue(Vec4V(m_sunInfluenceRadius,
										m_sunScatterIntensity,
										m_moonInfluenceRadius,
										m_moonScatterIntensity));

	// Cloud speed variables
	float smallCloudSpeed =    (0.5f + (m_smallCloudSpeed    * m_time / 11.0f));
	float overallDetailSpeed = (0.5f + (m_overallDetailSpeed * m_time / 11.0f));
	float edgeDetailSpeed =    (0.5f + (m_edgeDetailSpeed    * m_time / 11.0f));

	smallCloudSpeed =    fmod(smallCloudSpeed, 1.0f);
	overallDetailSpeed = fmod(overallDetailSpeed, 1.0f);
	edgeDetailSpeed =    fmod(edgeDetailSpeed, 1.0f);

	m_speedConstants.setValue(Vec3V(smallCloudSpeed, overallDetailSpeed, edgeDetailSpeed));

	const float fogFadeDiff = m_skyPlaneFogFadeEnd - m_skyPlaneFogFadeStart;
	const float fogFadeDiv = (fogFadeDiff != 0.0f) ? fogFadeDiff : 1.0f; 

	m_skyPlaneParams.setValue(Vec4V(
		m_skyPlaneFogFadeStart,
		m_skyPlaneFogFadeEnd,
		1.0f / fogFadeDiv,
		0.0f));

	// Calculate day of lunar cycle and adjust lighting angle accordingly
	float lunarCycle = fmod(m_time, 27.0f) / 26.0f;
	float cycleOverride = g_timeCycle.GetCurrMoonCycleOverride();
	if (cycleOverride >= 0.0f) { lunarCycle = cycleOverride; }
	BANK_ONLY(lunarCycle += g_moonCycleDebug;)

	static dev_float moonCycleAdjust = -0.2f;
	static dev_float startAngle = 2.90f;
	
	Vec3V lightDirection(V_Y_AXIS_WZERO);
	ScalarV angle = ScalarVFromF32(Lerp(lunarCycle, -startAngle, startAngle + moonCycleAdjust));
	lightDirection = RotateAboutZAxis(lightDirection, angle);
	m_lunarCycle.setValue(lightDirection);
}

#if __BANK
// ------------------------------------------------------------------------------------------------------------------//
//												InitWidgets															 //
// ------------------------------------------------------------------------------------------------------------------//
void CSkySettings::InitWidgets()
{
	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->AddSeparator();
	pVfxBank->PushGroup("Sky", false);
	{
		pVfxBank->AddSlider("Zenith Transition Position (Vertical)", &m_zenithTransitionPosition, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Zenith Transition Blend (East)", &m_zenithTransitionEastBlend, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Zenith Transition Blend (West)", &m_zenithTransitionWestBlend, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Zenith Blend Start", &m_zenithBlendStart, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddColor("Zenith Color", &m_zenithColor.m_value);
		pVfxBank->AddSlider("Zenith Intensity", &m_zenithColorIntensity, 0.0f, 64.0f, 0.1f);
		pVfxBank->AddColor("Zenith Transition Color", &m_zenithTransitionColor.m_value);
		pVfxBank->AddSlider("Zenith Transition Intensity", &m_zenithTransitionColorIntensity, 0.0f, 64.0f, 0.1f);
		pVfxBank->AddSeparator();
		pVfxBank->AddSlider("Azimuth Transition Position (Horizontal)", &m_azimuthTransitionPosition.m_value, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddColor("Azimuth East Color", &m_azimuthEastColor.m_value);
		pVfxBank->AddSlider("Azimuth East Intensity", &m_azimuthEastColorIntensity, 0.0f, 64.0f, 0.1f);
		pVfxBank->AddColor("Azimuth Transition Color", &m_azimuthTransitionColor.m_value);
		pVfxBank->AddSlider("Azimuth Transition Intensity", &m_azimuthTransitionColorIntensity, 0.0f, 64.0f, 0.1f);
		pVfxBank->AddColor("Azimuth West Color", &m_azimuthWestColor.m_value);
		pVfxBank->AddSlider("Azimuth West Intensity", &m_azimuthWestColorIntensity, 0.0f, 64.0f, 0.1f);
		pVfxBank->AddSeparator();
		pVfxBank->AddSlider("HDR Intensity", &m_hdrIntensity.m_value, 0.0f, 64.0f, 1.0f);
	}
	pVfxBank->PopGroup();

	pVfxBank->AddSeparator();
	pVfxBank->PushGroup("Sun", false);
	{
		pVfxBank->AddColor("Sun Color", &m_sunColor.m_value);
		pVfxBank->AddColor("Sun Disc Color", &m_sunDiscColor);
		pVfxBank->AddSlider("Sun Disc Size", &m_sunDiscSize, 0.0f, 16.0f, 0.1f);
		pVfxBank->AddSlider("Sun HDR Intensity", &m_sunHdrIntensity, 0.0f, 256.0f, 0.1f);
		pVfxBank->AddSlider("Mie Phase", &m_miePhase, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Mie Scatter", &m_mieScatter, 1.0f, 1024.0f, 1.0f);
		pVfxBank->AddSlider("Sun Influence (Radius)", &m_sunInfluenceRadius, 0.0f, 128.0f, 0.01f);
		pVfxBank->AddSlider("Sun Scatter Intensity", &m_sunScatterIntensity, 0.0, 128.0, 0.01f);
		pVfxBank->AddToggle("Override Sun Position", &m_sunPositionOverride);
		pVfxBank->AddSlider("Sun Azimuth", &m_sunAzimuth, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Sun Zenith", &m_sunZenith, -1.0f, 1.0f, 0.01f);
	}
	pVfxBank->PopGroup();

	pVfxBank->AddSeparator();
	pVfxBank->PushGroup("Moon and Stars", false);
	{
		pVfxBank->AddColor("Moon Color", &m_moonColor.m_value);
		pVfxBank->AddSlider("Moon Disc Size", &m_moonDiscSize, 0.0f, 16.0f, 0.1f);
		pVfxBank->AddSlider("Moon Intensity", &m_moonIntensity.m_value, 0.0f, 16.0f, 0.1f);
		pVfxBank->AddSlider("Starfield Intensity", &m_starfieldIntensity.m_value, 0.0f, 32.0f, 0.1f);
		pVfxBank->AddSlider("Moon Influence (Radius)", &m_moonInfluenceRadius, 0.0f, 128.0f, 0.01f);
		pVfxBank->AddSlider("Moon Scatter Intensity", &m_moonScatterIntensity, 0.0, 16.0, 0.01f);
		pVfxBank->AddSlider("Moon Cycle debug", &g_moonCycleDebug, 0.0f, PI * 2.0f, 0.01f);
	}
	pVfxBank->PopGroup();

	pVfxBank->AddSeparator();
	pVfxBank->PushGroup("Clouds", false);
	{
		pVfxBank->AddSlider("Dither Strength", &m_cloudDitherStrength, 0.0f, 2048.0f, 0.1f);

		pVfxBank->PushGroup("Cloud Generation", false);
		{
			pVfxBank->AddSlider("Frequency", &m_noiseFrequency.m_value, 0.0f, 32.0f, 0.01f);
			pVfxBank->AddSlider("Scale", &m_noiseScale.m_value, 0.0f, 256.0f, 0.01f);
			pVfxBank->AddSlider("Threshold", &m_noiseThreshold.m_value, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Softness", &m_noiseSoftness.m_value, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Density Multiplier", &m_cloudDensityMultiplier, 0.0f, 6.0f, 0.01f);
			pVfxBank->AddSlider("Density Bias", &m_cloudDensityBias, 0.0f, 6.0f, 0.01f);
			pVfxBank->AddSlider("Noise Phase Multiplier", &g_NoisePhaseMult, 0.0f, 1.0f, 0.001f);
			pVfxBank->AddToggle("Enable Wind Effects", &g_EnableWindEffectsForSkyClouds);
			pVfxBank->AddToggle("Use Gust Wind Velocity", &g_UseGustWindForSkyClouds);
			pVfxBank->AddToggle("Use Global Variation Wind Velocity", &g_UseGlobalVariationWindForSkyClouds);
			
			pVfxBank->AddToggle("Override Global Wind Direction", &g_IsOverrideWindDirection);
			pVfxBank->AddVector("Overriden Global Wind Direction", &g_overrideWindDirection, -1.0f, 1.0f, 0.0001f);
		}
		pVfxBank->PopGroup();

		pVfxBank->PushGroup("Large Clouds", false);
		{
			pVfxBank->AddColor("Mid Color", &m_cloudMidColour.m_value);

			pVfxBank->AddColor("Base Color", &m_vCloudBaseColour);
			pVfxBank->AddSlider("Base Strength", &m_cloudBaseStrength, 0.0f, 1.0f, 0.01f);

			pVfxBank->AddColor("Shadow Color", &m_vCloudShadowColour);
			pVfxBank->AddSlider("Shadow Strength", &m_cloudShadowStrength, -1.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Shadow Threshold", &m_noiseDensityOffset.m_value, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Colour / Shadow Offset", &m_cloudOffset, 0.0f, 1.0f, 0.01f);

			pVfxBank->AddSeparator();
			pVfxBank->AddSlider("Detail Overlay", &m_cloudOverallDetailStrength, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Detail Overlay Color", &m_cloudOverallDetailColor, 0.0f, 1.0f, 0.01f);

			pVfxBank->AddSlider("Edge Detail", &m_cloudEdgeDetailStrength, 0.0f, 1.0f, 0.01f);

			pVfxBank->AddSeparator();
			pVfxBank->AddSlider("Fade Out", &m_cloudFadeOut, 0.0f, 1.0f, 0.01f);

			pVfxBank->AddSlider("HDR Intensity", &m_cloudHdrIntensity, 0.0f, 256.0f, 0.01f);
		}
		pVfxBank->PopGroup();

		pVfxBank->PushGroup("Small / Filler Clouds", false);
		{
			pVfxBank->AddColor("Color", &m_vSmallCloudColor);
			pVfxBank->AddSlider("Strength", &m_smallCloudDetailStrength, 0.0, 1.0, 0.01f);
			pVfxBank->AddSlider("Scale", &m_smallCloudDetailScale, 0.0f, 32.0f, 0.01f);
			pVfxBank->AddSlider("Density Multiplier", &m_smallCloudDensityMultiplier, 0.0, 6.0, 0.01f);
			pVfxBank->AddSlider("Density Bias", &m_smallCloudDensityBias, 0.0, 6.0, 0.01f);
		}
		pVfxBank->PopGroup();
	}
	pVfxBank->PopGroup();
	
	pVfxBank->AddSeparator();
	pVfxBank->PushGroup("Sky Plane", false);
	{
		pVfxBank->AddColor("Plane Color", &m_skyPlaneColor.m_value);
		pVfxBank->AddSlider("Plane Z-offset", &m_skyPlaneOffset, 0.0f, 4096.0f, 0.01f);
	}
	pVfxBank->PopGroup();
}

#endif
