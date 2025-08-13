//
// renderer/PostProcessFX.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
// Post-processing effects
//

#ifndef _POST_FX_H_
#define _POST_FX_H_

#include "vector/vector4.h"

#include "camera/viewports/ViewportManager.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderThread.h"
#include "shaders/ShaderLib.h"
#include "renderer/PostFX_shared.h"
#include "renderer/MLAA.h"

#include "core/watermark.h"
#include "control/replay/ReplaySupportClasses.h"

#define USE_SCREEN_WATERMARK WATERMARKED_BUILD
#define DISPLAY_NETWORK_INFO (WATERMARKED_BUILD && 1)
#define USE_IMAGE_WATERMARKS (WATERMARKED_BUILD && 1) 


#define SSA_USES_EARLYSTENCIL		(__XENON || __PS3)
#if __PS3
	#define NUM_RESET_FRAMES	(3)
#else
	#define NUM_RESET_FRAMES	(3)
#endif

namespace PostFX
{
	enum eTonemapValues
	{
		TONEMAP_FILMIC_A_DARK = 0,
		TONEMAP_FILMIC_B_DARK,
		TONEMAP_FILMIC_C_DARK,
		TONEMAP_FILMIC_D_DARK,
		TONEMAP_FILMIC_E_DARK,
		TONEMAP_FILMIC_F_DARK,
		TONEMAP_FILMIC_W_DARK,
		TONEMAP_FILMIC_EXPOSURE_DARK,
		TONEMAP_FILMIC_A_BRIGHT,
		TONEMAP_FILMIC_B_BRIGHT,
		TONEMAP_FILMIC_C_BRIGHT,
		TONEMAP_FILMIC_D_BRIGHT,
		TONEMAP_FILMIC_E_BRIGHT,
		TONEMAP_FILMIC_F_BRIGHT,
		TONEMAP_FILMIC_W_BRIGHT,
		TONEMAP_FILMIC_EXPOSURE_BRIGHT,
		TONEMAP_VAR_COUNT
	};

	enum eResolvePause
	{
		RESOLVE_PAUSE = 0,	// need to resolve rendertargets for pause state
		UNRESOLVE_PAUSE,	// need to dealloc temp rendertargets of pause state
		IN_RESOLVE_PAUSE,	// we are in pause state
		NOT_RESOVE_PAUSE	// we are not in pause state
	};

	// Type ////////////////////////////////////////////////////////////////////////////////////////
	struct historyValues
	{
		float exposure;
		float luminance;
		float timestep;
	};

	void UpdateVisualDataSettings(); // Declared early for  PostFXParamBlock

	// Class to enforce thread access rules for paramBlock
	class PostFXParamBlock
	{
	public:

	struct paramBlock
	{
		// Feature control
		bool			m_nightVision;
		bool			m_lightRaysActive;
		bool			m_seeThrough;
		bool			m_sniperSightActive;

		// Fog ray
		float			m_fograyContrast;
		float			m_fograyIntensity;
		float			m_fograyDensity;
		bool			m_bShadowActive;
		bool			m_bFogRayMiniShadowMapGenerated;
		float			m_fograyFadeStart;
		float			m_fograyFadeEnd;

		// DOF
		float			m_AdaptiveDofTimeStep;
		float			m_AdaptiveDofCustomPlanesBlendLevel;
		float			m_AdaptiveDofBlurDiskRadiusPowerFactorNear;
		bool			m_HighDofScriptOverrideToggle;
		bool			m_HighDofScriptOverrideEnableDOF;
		bool			m_HighDof;
		bool			m_ShallowDof;
		bool			m_HighDofForceSmallBlur;
		bool			m_SimpleDof;
		bool			m_AdaptiveDof;
		Vector4			m_EnvironmentalBlurParams; // x: far blur in y: far blur out z: blur size
		Vector4			m_dofParameters;
		Vector4			m_hiDofParameters;
		Vector4			m_hiDofScriptParameters;

		// Color Correction
		Vector4		    m_colorCorrect;
		Vector4			m_colorShift;
		float			m_deSaturate;
		float			m_gamma;
		
		// HDR
		float			m_exposureCurveA;
		float			m_exposureCurveB;
		float			m_exposureCurveOffset;
		float			m_bloomThresholdMax;
		float			m_bloomThresholdWidth;
		float			m_bloomIntensity;
		float			m_exposureMin;
		float			m_exposureMax;
		float			m_exposureTweak;
		float			m_exposurePush;
		float			m_exposureFocus;
		Vector4			m_filmicParams[2];

		Vector2			m_tonemapParams;
		Vector4			m_brightTonemapParams[2];
		Vector4			m_darkTonemapParams[2];		

		// Adaptation
		float			m_adaptionMinStepSize;
		float			m_adaptionMaxStepSize;
		float			m_adaptionStepMult;
		s32				m_adaptionNumFramesAvg;
		float			m_adaptionThreshold;
		bool			m_adaptationResetRequired;
	
		float			m_averageExposure;
		float			m_averageTimestep;

		bool			m_isCameraCut;
		bool			m_justUseCalculated;
		float			m_adaptionTimeStep;
		float			m_nextHistoryIndex;
		float			m_historySize;
		mutable bool	m_isFirstTime;

		// Noise
		float			m_noise;
		float			m_noiseSize;
		
		// Motion Blur
		float			m_directionalBlurLength;
		float			m_directionalBlurGhostingFade;
		float			m_directionalBlurMaxVelocityMult;
		bool			m_isForcingMotionBlur;

		// Time
		float			m_timeStep;
		float			m_time;
		
		// Night Vision
		float			m_lowLum;
		float			m_highLum;
		float			m_topLum;

		float			m_scalerLum;

		float			m_offsetLum;
		float			m_offsetLowLum;
		float			m_offsetHighLum;

		float			m_noiseLum;
		float			m_noiseLowLum;
		float			m_noiseHighLum;

		float			m_bloomLum;

		// Screen blur
		float			m_screenBlurFade;

		// Scanline
		Vector4			m_scanlineParams;

		Vector4			m_colorLum;
		Vector4			m_colorLowLum;
		Vector4			m_colorHighLum;

		// Light Rays
		Vector4			m_lightrayParams;
		Vector4			m_sslrParams;
		Vector3			m_SunDirection;

		// Heat Haze
		Vector4			m_HeatHazeParams;
		Vector4			m_HeatHazeTex1Params;
		Vector4			m_HeatHazeTex2Params;
		Vector4			m_HeatHazeOffsetParams;
		Vector4			m_HeatHazeTex1Anim;
		Vector4			m_HeatHazeTex2Anim;

		//  See Through
		Vector4			m_SeeThroughDepthParams;
		Vector4			m_SeeThroughParams;
		Vector4			m_SeeThroughColorNear;
		Vector4			m_SeeThroughColorFar;
		Vector4			m_SeeThroughColorVisibleBase;
		Vector4			m_SeeThroughColorVisibleWarm;
		Vector4			m_SeeThroughColorVisibleHot;
		
		// Vignetting
		Vector4			m_vignettingParams;
		Vector3			m_vignettingColour;

		// Damage Overlay
		Vector4			m_damageOverlayMisc;

		Vector4			m_damageFiringPos[4];
		Vector4			m_damagePlayerPos;

		// Lens Gradient
		Vector4			m_lensGradientColTop;
		Vector4			m_lensGradientColMiddle;
		Vector4			m_lensGradientColBottom;

		// Distortion/Colour Aberration
		Vector4			m_distortionParams;

		// Blur Vignetting
		Vector4			m_blurVignettingParams;

		// Sniper Sight
		Vector4			m_sniperSightDOFParams;

		// Lens Artefacts
		float			m_lensArtefactsGlobalMultiplier;
		float			m_lensArtefactsMinExposureMultiplier;
		float			m_lensArtefactsMaxExposureMultiplier;

		bool			m_damageEndurance[4];
		bool			m_damageEnabled[4];
		bool			m_damageFrozen[4];
		bool			m_drawDamageOverlayAfterHud;

		// Cutscene DOF override
		float			m_DOFOverride;
		bool			m_DOFOverrideEnabled; // [-1, 1] => [1, maxKernelSize], 0 maps to default DOF kernel size.

		float			m_BokehBrightnessThresholdMax;
		float			m_BokehBrightnessThresholdMin;
		float			m_BokehBrightnessFadeThresholdMax;
		float			m_BokehBrightnessFadeThresholdMin;

		// FPV Motion Blur
		Vector4			m_fpvMotionBlurWeights;
		bool			m_fpvMotionBlurActive;
		Vec2V			m_fpvMotionBlurVelocity;
		float			m_fpvMotionBlurSize;

		paramBlock()
		{
			// Fog ray
			m_fograyContrast = 1.0f;
			m_fograyIntensity = 0.0f;
			m_fograyDensity = 1.0f;
			m_bShadowActive = false;
			m_bFogRayMiniShadowMapGenerated = false;

			// DOF
			m_dofParameters.Set(1000.0f, 1.0f/2000.0f, 0.5f, 0.0f);
			m_hiDofParameters.Set(0.0f, 0.0f, 5000.0f, 5000.0f);
			m_hiDofScriptParameters.Zero();
			m_AdaptiveDofTimeStep = 0.0f;
			m_HighDof = false;
			m_ShallowDof = false;
			m_HighDofForceSmallBlur = false;
			m_SimpleDof = false;
			m_HighDofScriptOverrideToggle = false;
			m_HighDofScriptOverrideEnableDOF = false;
			m_AdaptiveDof = false;
			m_EnvironmentalBlurParams.Set(5000.0f, 5000.0f, 16.0f, 0.0f);
			m_DOFOverrideEnabled = false;
			m_DOFOverride = 1.0f;
			m_AdaptiveDofCustomPlanesBlendLevel = 0.0f;
			m_AdaptiveDofBlurDiskRadiusPowerFactorNear = 1.0f;

			// Color Correction
			m_colorCorrect.Set(0.5f, 0.5f, 0.5f, 1.0f);
			m_colorShift.Set(0.0f,0.0f,0.0f,0.0f);
			m_deSaturate = 1.0f;
			m_gamma = 1.0f / 2.2f;

			// HDR
			m_bloomThresholdMax = 0.3f;
			m_bloomThresholdWidth = 0.0f;
			m_bloomIntensity = 1.5f;

			m_filmicParams[0] = Vector4(0.22f, 0.15f, 0.10f, 0.40f);
			m_filmicParams[1] = Vector4(0.01f, 0.30f, 3.0f, 0.0f);

			m_brightTonemapParams[0] = m_filmicParams[0];
			m_brightTonemapParams[1] = m_filmicParams[1];

			m_darkTonemapParams[0] = m_filmicParams[0];
			m_darkTonemapParams[1] = m_filmicParams[1];

			m_tonemapParams = Vector2(0.0f, 0.0f);

			m_exposureMin = -2.3f;
			m_exposureMax = 2.0f;
			m_exposureTweak = 0.0f;
			m_exposurePush = 0.0f;
			m_exposureFocus = 0.0f;

			m_exposureCurveA = -1.0668907449987120E+02f;
			m_exposureCurveB = 4.1897015173052825E-03f;
			m_exposureCurveOffset = 1.0460364172443734E+02f;

			// Adaption
			m_adaptionMinStepSize = 0.15f;
			m_adaptionMaxStepSize = 15.0f;
			m_adaptionStepMult = 9.0f;
			m_adaptionNumFramesAvg = 30;
			m_adaptionThreshold = 0.0f;
			m_adaptationResetRequired = true;
			m_averageExposure = 0.0f;
			m_averageTimestep = 1.0f;

			m_isCameraCut = true;
			m_justUseCalculated = true;
			m_adaptionTimeStep = 0.0f;
			m_nextHistoryIndex = 0;
			m_historySize = 0;
			m_isFirstTime = true;


			// Noise
			m_noise = 1.0f;
			m_noiseSize = 1.0f;
						
			// Motion Blur
			m_directionalBlurLength = 0.0f;
			m_isForcingMotionBlur = false;
		
			// Time
			m_timeStep = 0.0f;
			m_time = 0.0f;

			// NightVision
			m_lowLum = 0.05f;
			m_highLum = 0.600f;
			m_topLum = 0.75f;

			m_scalerLum = 10.0f;

			m_offsetLum = 0.2f;
			m_offsetLowLum = 0.180f;
			m_offsetHighLum = 0.2f;

			m_noiseLum = 0.050f;
			m_noiseLowLum = 0.080f;
			m_noiseHighLum = 0.300f;

			m_bloomLum = 0.010f;
			
			m_colorLum.Set(0.0f, 1.0f, 0.22f, 1.0f);
			m_colorLowLum.Set(0.0f, 0.5f, 1.0f, 1.0f);
			m_colorHighLum.Set(0.67f, 1.0f, 0.64f, 1.0f);

			// Scanline
			m_scanlineParams.x = 0.0f;
			m_scanlineParams.y = 800.0f;
			m_scanlineParams.z = 800.0f;
			m_scanlineParams.w = 0.0f;

			// Screen blur fade
			m_screenBlurFade = 0.0f;

			// Light rays
			m_lightrayParams.Set(1.0f,1.0f,1.0f,100.0f);
			
			// Heat Haze
			m_HeatHazeParams.Set(90.0f,187.5f,0.0f,1.0f);
			m_HeatHazeTex1Params.Set(1.0f,1.0f,0.0f,0.0f);
			m_HeatHazeTex2Params.Set(0.5f,0.5f,0.0f,0.0f);
			m_HeatHazeOffsetParams.Set(8.0f,2.0f,1.0f,1.0f);
			m_HeatHazeTex1Anim.Set(PI/2.0f,PI/2.0f,3.0f,20.0f);
			m_HeatHazeTex2Anim.Set(0.0f,PI/5,0.5f,3.0f);

			// See Through
			m_SeeThroughDepthParams.Set(0.0f,500.0f,10.0f,0.0f);
			m_SeeThroughParams.Set(0.25f,1.0f,0.5f,0.5f);
			m_SeeThroughColorNear.Set(0.1f,0.0f,0.25f,1.0f);
			m_SeeThroughColorFar.Set(0.25f,0.0f,0.75f,1.0f);
			m_SeeThroughColorVisibleBase.Set(0.5f,0.0f,0.0f,1.0f);
			m_SeeThroughColorVisibleWarm.Set(1.0f,0.0f,0.0f,1.0f);
			m_SeeThroughColorVisibleHot.Set(1.0f,0.0f,0.0f,1.0f);

			// Vignetting
			m_vignettingParams.Zero();
			m_vignettingColour.Zero();

			// Damage Overlay
			m_damageFiringPos[0].Zero();
			m_damageFiringPos[1].Zero();
			m_damageFiringPos[2].Zero();
			m_damageFiringPos[3].Zero();
			m_damagePlayerPos.Zero();
			m_damageEnabled[0] = false;
			m_damageEnabled[1] = false;
			m_damageEnabled[2] = false;
			m_damageEnabled[3] = false;
			m_damageFrozen[0] = false;
			m_damageFrozen[1] = false;
			m_damageFrozen[2] = false;
			m_damageFrozen[3] = false;
			m_drawDamageOverlayAfterHud = false;

			// Lens gradient
			m_lensGradientColTop.Set(1.0f,1.0f,1.0f,0.5f);
			m_lensGradientColBottom.Set(1.0f,1.0f,1.0f,0.5f);
			m_lensGradientColMiddle.Set(1.0,1.0f,1.0f,0.5f);

			// Distortion
			m_distortionParams.Zero();

			// Blur Vignetting
			m_blurVignettingParams.Zero();

			// Sniper Sight
			m_sniperSightActive = false;
			m_sniperSightDOFParams.x = 0.0f;
			m_sniperSightDOFParams.y = 0.0f;
			m_sniperSightDOFParams.z = 0.0f;
			m_sniperSightDOFParams.w = 2180.0f;

			// Lens Artefacts
			m_lensArtefactsGlobalMultiplier = 1.0f;
			m_lensArtefactsMinExposureMultiplier = 1.0f;
			m_lensArtefactsMaxExposureMultiplier = 1.0f;

			m_BokehBrightnessThresholdMax = 0.0f;
			m_BokehBrightnessThresholdMin = 0.0f;
			m_BokehBrightnessFadeThresholdMax = 0.0f;
			m_BokehBrightnessFadeThresholdMin = 0.0f;

			m_fpvMotionBlurWeights = Vector3(1.0f, 0.0f, 0.8f, 1.0f);
			m_fpvMotionBlurActive = false;
			m_fpvMotionBlurVelocity = Vec2V(0.0f, 0.0f);
			m_fpvMotionBlurSize = 1.0f;
		}
	};

	typedef struct paramBlock paramBlock;

		// *** DO NOT MAKE A NON-CONST ACCESSOR for anything but the update thread! ***
		// The param block may only be read by the sub-render threads (if sub-render threads write to this struct, we'll get race conditions!)
		static const paramBlock& GetRenderThreadParams() {Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return ms_params[gRenderThreadInterface.GetRenderBuffer()]; }
				
		// The param block may read or written by the update thread
		static paramBlock& GetUpdateThreadParams() {Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); return ms_params[gRenderThreadInterface.GetUpdateBuffer()]; }

		// This can be called from either main or render threads, but it must be read-only (if you need write access, use GetUpdateThreadParams(). Don't write this from the render thread!
		static const paramBlock& GetParams() { return ms_params[gRenderThreadInterface.GetCurrentBuffer()]; } 
		static const paramBlock& GetParams(int nBufferID) { return ms_params[nBufferID]; } 


	private:

		friend void PostFX::UpdateVisualDataSettings();

		// Update visual setting after reload. This should only be called from PostFX::UpdateVisualDataSettings()
		static void UpdateVisualSettings();

		// Do not make a non-const accessors to ms_params for anything but the update thread!
		static paramBlock ms_params[2]; 
	};

	//////////////////////////////////////////////////////////////////////////
	//
	class PedKillOverlay
	{
	public:
		PedKillOverlay();
		void	Trigger();
		void	Toggle(bool bEnabled) { m_disable = !bEnabled; }
		atHashString m_effectName;

		bool	m_disable;
	};

	//////////////////////////////////////////////////////////////////////////
	//
	class ScreenBlurFade
	{
	public:
		
		enum ScreenBlurFadeState 
		{
			SBF_IDLE = 0,
			SBF_FADING_IN = 1,
			SBF_FADING_OUT = 2,
		};

		void	Init();
		void	UpdateOnLoadingScreen();
		void	Update();
		void	Trigger(bool bFadeOut = false);
		void	Reset() { Init(); };
		void	SetDuration(float duration) { m_fDuration = rage::Max<float>(duration, 0.1f); }

		bool	HasFadedIn() const { return (((ScreenBlurFadeState)m_state == SBF_FADING_IN) && m_fFadeLevel == 1.0f); }
		bool	IsPlaying() const { return (ScreenBlurFadeState)m_state != SBF_IDLE; }
		bool	IsFading() const { return ( (((ScreenBlurFadeState)m_state == SBF_FADING_IN) && m_fFadeLevel < 1.0f) || ((ScreenBlurFadeState)m_state == SBF_FADING_OUT) ); }

		float	GetCurrentTime() const { return m_fDuration*m_fFadeLevel; } 

		float	GetFadeLevel() const { return ( ((ScreenBlurFadeState)m_state == SBF_FADING_IN) ? m_fFadeLevel : 1.0f-m_fFadeLevel); }
		float*	GetDurationPtr() { return &m_fDuration; }
		s32*	GetStatePtr() { return &m_state; }

	private:
	
		s32					m_state;
		float				m_fFadeLevel;
		float				m_fDuration;
	};

	//////////////////////////////////////////////////////////////////////////
	//
	class BulletImpactOverlay
	{
	public:

		void	Init();
		void	Reset() { Init(); };
		void	Update();

		void	RegisterBulletImpact(const Vector3& vWeaponPos, f32 damageIntensity, bool bIsEnduranceDamage = false);
				
		void	Toggle(bool bEnable) {  if (bEnable == false) { Reset(); } m_disabled = !bEnable; }

		static const int GetNumEntries() { return NUM_ENTRIES; }

	#if __BANK
		bool	m_bEnableDebugRender; 
	#endif

		static const int	NUM_ENTRIES = 4;

		// R/W by render thread only
		static Vector4	ms_frozenDamagePosDir[NUM_ENTRIES];
		static float	ms_frozenDamageOffsetMult[NUM_ENTRIES];

		enum Type
		{
			DOT_DEFAULT = 0,
			DOT_FIRST_PERSON,
			DOT_COUNT
		};

		struct Settings
		{
			Vector3	colourTop;
			Vector3	colourBottom;
			Vector3	colourTopEndurance;
			Vector3	colourBottomEndurance;
			u32 	rampUpDuration;
			u32 	rampDownDuration;
			u32 	holdDuration;
			float 	spriteBaseWidth;
			float 	spriteTipWidth;
			float	spriteLength;
			float	globalAlphaBottom;
			float	globalAlphaTop;	
			float	angleScalingMult;
		};

		// default values exposed to visual settings
		static Settings	ms_settings[DOT_COUNT];
		static float	ms_screenSafeZoneLength;

		bool			m_drawAfterHud;
		bool			m_disabled;

	private:

		struct Entry
		{
			Entry() { Reset(); }
			void Reset();

			Vector3		m_firingWeaponPos;
			float		m_fFadeLevel;
			float		m_fIntensity;
			u32			m_startTime;
			bool		m_bFrozen;
			bool		m_bIsEnduranceDamage;
		};

		void	UpdateTiming(Entry* pEntry, const Settings& pSettings);
		Entry*	GetFreeEntry();
		bool	IsAnyUpdating() const { return (m_entries[0].m_startTime != 0 || m_entries[1].m_startTime != 0 || m_entries[2].m_startTime != 0 || m_entries[3].m_startTime != 0); }

		Entry				m_entries[NUM_ENTRIES];
	};

#if USE_SCREEN_WATERMARK
	struct WatermarkParams
	{
		WatermarkParams() {};
		void Init();

		char    text[RL_MAX_NAME_BUF_SIZE];
		Vector2	textPos;
		float	textSize;
		Color32 textColor;
#if	DISPLAY_NETWORK_INFO
		Vector2	netTextPos;
		float	netTextSize;
		Color32 netTextColor;
#endif
		bool	useOutline;
		bool	bEnabled;
		float	alphaNight;
		float	alphaDay;
#if __BANK
		bool	bForceWatermark;
		bool	bUseDebugText;
		char    debugText[RL_MAX_NAME_BUF_SIZE];
#endif
	};

	void UpdateScreenWatermark();
	void RenderScreenWatermark();
	bool IsWatermarkEnabled();
#endif

	// Data ////////////////////////////////////////////////////////////////////////////////////////

	extern PedKillOverlay g_pedKillOverlay;
	extern ScreenBlurFade g_screenBlurFade;
	extern BulletImpactOverlay g_bulletImpactOverlay;
	extern DofTODOverrideHelper g_cutsceneDofTODOverride;

	extern int g_resetAdaptedLumCount;
	extern bool g_enableLightRays;
	extern bool g_lightRaysAboveWaterLine;
	extern bool g_enableNightVision;
#if __BANK
	extern bool g_overrideNightVision;
#endif // __BANK
	extern bool g_enableSeeThrough;

	extern bool g_enableExposureTweak;
	extern bool g_forceExposureReadback;

	#define VALID_UPDATE() \
	{\
		Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));\
	}

	#define VALID_RENDER() \
	{\
		Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));\
	}

	class Adaptation
	{
	public:

		Adaptation() : ms_bufferIndex(0)
		{
			for (int i = 0; i < 2; i++)
			{
				ms_luminance[i] = 0.0f;
				ms_exposure[i] = 0.0f;
				ms_targetExposure[i] = 0.0f;
				ms_powTwoExposure[i] = 1.0f;
				ms_packScalar[i] = 1.0f;
			}
		}

		void Synchronise();

		float GetUpdateLum()			{ VALID_UPDATE(); return ms_luminance[GetUpdateBufferIndex()]; }
		float GetUpdateExposure()		{ VALID_UPDATE(); return ms_exposure[GetUpdateBufferIndex()]; }
		float GetUpdatePowTwoExposure() { VALID_UPDATE(); return ms_powTwoExposure[GetUpdateBufferIndex()]; }
		float GetUpdateTargetExposure() { VALID_UPDATE(); return ms_targetExposure[GetUpdateBufferIndex()]; }

		float GetRenderLum()			{ VALID_RENDER(); return ms_luminance[GetRenderBufferIndex()]; }
		float GetRenderExposure()		{ VALID_RENDER(); return ms_exposure[GetRenderBufferIndex()]; }
		float GetRenderPowTwoExposure() { VALID_RENDER(); return ms_powTwoExposure[GetRenderBufferIndex()]; }
		float GetRenderTargetExposure() { VALID_RENDER(); return ms_targetExposure[GetRenderBufferIndex()]; }

		void SetRenderLum(float lum)					{ VALID_RENDER(); ms_luminance[GetRenderBufferIndex()] = lum; }
		void SetRenderExposure(float exposure)			{ VALID_RENDER(); ms_exposure[GetRenderBufferIndex()] = exposure; }
		void SetRenderPowTwoExposure(float exposure)    { VALID_RENDER(); ms_powTwoExposure[GetRenderBufferIndex()] = exposure; }
		void SetRenderTargetExposure(float exposure)    { VALID_RENDER(); ms_targetExposure[GetRenderBufferIndex()] = exposure; }

		void SetUpdateLum(float lum)					{ VALID_UPDATE(); ms_luminance[GetUpdateBufferIndex()] = lum; }
		void SetUpdateExposure(float exposure)			{ VALID_UPDATE(); ms_exposure[GetUpdateBufferIndex()] = exposure; }
		void SetUpdatePowTwoExposure(float exposure)    { VALID_UPDATE(); ms_powTwoExposure[GetUpdateBufferIndex()] = exposure; }
		void SetUpdateTargetExposure(float exposure)    { VALID_UPDATE(); ms_targetExposure[GetUpdateBufferIndex()] = exposure; }

		void SetUpdatePackScalar(float scalar)			{ VALID_UPDATE(); ms_packScalar[GetUpdateBufferIndex()] = scalar; }
		float GetUpdatePackScalar()						{ VALID_UPDATE(); return ms_packScalar[GetUpdateBufferIndex()]; }
		float GetRenderPackScalar()						{ VALID_RENDER(); return ms_packScalar[GetRenderBufferIndex()]; }

		#if __BANK
			float* GetUpdateLumPtr() { return &ms_luminance[GetUpdateBufferIndex()]; }
			float* GetUpdateExposurePtr() { return &ms_exposure[GetUpdateBufferIndex()]; }
			float* GetUpdatePowTwoExposurePtr() { return &ms_powTwoExposure[GetUpdateBufferIndex()]; }
			float* GetUpdateTargetExposurePtr() { return &ms_targetExposure[GetUpdateBufferIndex()]; }
			float* GetUpdatePackScalarPtr() { return &ms_packScalar[GetUpdateBufferIndex()]; }
		#endif

	private:

		int  GetRenderBufferIndex() { VALID_RENDER(); return ms_bufferIndex; }
		int  GetUpdateBufferIndex() { VALID_UPDATE(); return ms_bufferIndex ^ 0x1; }

		float ms_luminance[2];
		float ms_exposure[2];
		float ms_powTwoExposure[2];
		float ms_targetExposure[2];
		float ms_packScalar[2];

		int ms_bufferIndex;
	};

	extern Adaptation g_adaptation;
	extern grcRenderTarget*		g_prevExposureRT;
	extern grcRenderTarget*		g_currentExposureRT;
#if __PS3
	extern u16 g_RTPoolIdPostFX;
#endif
	extern grcRenderTarget* LumDownsampleRT[];
	extern int				LumDownsampleRTCount;

#if AVG_LUMINANCE_COMPUTE
	extern grcRenderTarget* LumCSDownsampleUAV[];
	extern int				LumCSDownsampleUAVCount;
#endif

#if (RSG_PC)
	extern grcRenderTarget* ImLumDownsampleRT[];
	extern int				ImLumDownsampleRTCount;
#endif

	extern grcRenderTarget* FogOfWarRT0;
	extern grcRenderTarget* FogOfWarRT1;
	extern grcRenderTarget* FogOfWarRT2;

	extern grcRenderTarget*	FogRayRT;
	extern grcRenderTarget* FogRayRT2;

#if RSG_PC
// for nvstereo
	extern grcRenderTarget*	CenterReticuleDist;
#endif

	inline grcRenderTarget* GetPreviousExposureRT() { return g_prevExposureRT; }

	extern bool  g_overrideDistanceBlur;
	extern float g_distanceBlurOverride;
	
	extern bool		g_scriptHighDOFOverrideToggle;
	extern bool		g_scriptHighDOFOverrideEnableDOF;
	extern Vector4	g_scriptHighDOFOverrideParams;

#if __ASSERT
	extern sysIpcCurrentThreadId g_FogRayCascadeShadowMapDownsampleThreadId;
#endif

#if __BANK
	extern bool g_Override;
	extern bool g_UseTiledTechniques;
	extern bool g_DrawCOCOverlay;
	extern float g_overwrittenExposure;
#endif// __BANK

#if __BANK || __WIN32PC || RSG_DURANGO || RSG_ORBIS
	// We'd like to turn this on and off in-game for PC
	extern bool g_FXAAEnable;
	MLAA_ONLY(extern MLAA g_MLAA;)
	TXAA_ONLY(extern bool g_TXAAEnable;);
#endif // __BANK || __WIN32PC

	extern bool g_UseSubSampledAlpha;
	extern bool g_UseSinglePassSSA;
	extern bool g_UseSSAOnFoliage;
	DECLARE_MTR_THREAD extern bool g_MarkingSubSamples;

	extern float g_gammaFrontEnd;

	extern float g_linearExposure;
	extern float g_averageLinearExposure;

	extern bool g_noiseOverride;
	extern float g_noisinessOverride;
	
	extern bool g_useAutoColourCompression;

#if RSG_PC
	extern bool g_allowDOFInReplay;
#endif

	extern grcTexture* g_pNoiseTexture;

	extern Matrix34 g_motionBlurPrevMatOverride;
	extern float	g_motionBlurMaxVelocityMult;
	extern bool		g_motionBlurCutTestDisabled;
	extern bool		g_bMotionBlurOverridePrevMat;

	extern float	g_filmicTonemapParams[TONEMAP_VAR_COUNT];

	extern Vector4	g_sniperSightDefaultDOFParams;
	extern bool		g_sniperSightDefaultEnabled;
	extern bool		g_sniperSightOverrideEnabled;
	extern bool		g_sniperSightOverrideDisableDOF;
	extern Vector4	g_sniperSightOverrideDOFParams;

	extern bool		g_DefaultMotionBlurEnabled;
	extern float	g_DefaultMotionBlurStrength;
	void UpdateDefaultMotionBlur();

	// Functions ///////////////////////////////////////////////////////////////////////////////////

#if RSG_PC
	void DeviceLost();
	void DeviceReset();

	void ResetDOFRenderTargets();
	void SetRequireResetDOFRenderTargets(bool request);
	bool GetRequireResetDOFRenderTargets();
#endif

	// Management
	void ResetMSAAShaders();
	void InitMSAAShaders();

	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);
	void SetGaussianBlur();
#if __BANK
	void AddWidgets(bkBank &bk);
	void AddWidgetsOnDemand();
	bool& GetSimplePfxRef();
#endif// __BANK

	void SimpleBlitWrapper(grcRenderTarget* pDestTarget, const grcTexture* pSrcTarget, u32 pass);

	void Update();

	void GetFPVPos();

	// Post FX
	inline void	TriggerPedKillOverlay();
	inline void TogglePedKillOverlay(bool bEnable);

	void SetFograyPostFxParams(float fograyIntensity, float fograyContrast, float fograyFadeStart, float fogRayFadeEnd);
	//void ProcessDepthRender()

	void DownSampleBackBuffer(int numDownSamples);
	grcTexture* GetDownSampledBackBuffer(int numDownSamples);

	// Apply depth based only effects
	void ProcessDepthFX();
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	void ApplyFilmEffect(int normalPass, grcRenderTarget* destination, const PostFXParamBlock::paramBlock& settings);
#endif 
#if USE_IMAGE_WATERMARKS
	void RenderWatermarkImage(const grcTexture* watermark, const Vec2f& pos, bool scale, float alpha);
#endif

	void ProcessSubSampleAlpha(grcRenderTarget* pColorTarget, grcRenderTarget* pDepthStencilTarget); // applied after transparencies.
	void ProcessUISubSampleAlpha(grcRenderTarget* pDstColorTarget, grcRenderTarget* pSrcColorTarget, grcRenderTarget* pSrcAlphaTarget, grcRenderTarget* pDepthStencilTarget, const Vector4 &scissorRect); // applied after transparencies.

	//////////////////////////////////////////////////////////////////////////
	// DOF time-of-day override for cutscenes API
	//
	// The override remains active in between the BeginCutsceneDofTODOverride/EndCutsceneDofTODOverride window;
	// the user needs to call these.
	//
	// Once active, time ranges can be added with AddCutsceneDofTODOverride (hours in [0, 23] range). These persist in between the Begin/End brackets.
	// A default override value for any time outside the active time ranges can be set with SetCutsceneDofTODDefaultOutOfRangeOverride.

	bool IsCutsceneDofTODOverrideActive();

	bool BeginCutsceneDofTODOverride();
	bool EndCutsceneDofTODOverride();

	void AddCutsceneDofTODOverride(float hourFrom, float hourTo, float overrideValue);
	void SetCutsceneDofTODDefaultOutOfRangeOverride(float defOverride);

#if __BANK
	void RenderDofDebug(grcRenderTarget* pDepthBlurTarget, const PostFXParamBlock::paramBlock &settings);
#endif //__BANK
	//////////////////////////////////////////////////////////////////////////

	// Apply non depth based effects
	void ProcessNonDepthFX();

#if RSG_PC
	void DoPauseResolve(eResolvePause bPause);
	eResolvePause GetPauseResolve();
#endif

	// Override motion blur previous matrix
	void SetMotionBlurPrevMatrixOverride(const Matrix34& mat);

	inline void SetMotionBlurMaxVelocityScale(float maxVelScalar);
	inline void SetForceMotionBlur(bool force);
	inline bool GetForceMotionBlur();
	inline float GetMotionBlurMaxVelocityScale();
	bool  IsMotionBlurEnabled();

	// Post FX Miscs
	void GetFilmicParams(Vector4& filmic0, Vector4& filmic1);
	void GetDefaultFilmicParams(Vector4& filmic0, Vector4& filmic1);

	void SetFilmicParams();
	void SetBloomParams();
	void SetExposureParams();
	BANK_ONLY(void SetDebugParams();)

	// HDR Reset adapted luminance
	inline void ResetAdaptedLuminance(int count = NUM_RESET_FRAMES);

	// Color Compression
	inline float GetPackScalar10bit(void);
	inline float GetPackScalar8bit(void);

	// Post FX system settings

	// Nightvision on/off
	inline void SetUseNightVision(bool state);
	inline bool GetGlobalUseNightVision();
#if __BANK
	inline bool GetGlobalOverrideNightVision();
#endif // __BANK
	inline bool GetUseNightVision();

	// SeeThrough on/off
	inline void SetUseSeeThrough(bool val);
	inline bool GetUseSeeThrough();
	
	// Exposure Tweak on/off
	inline void SetUseExposureTweak(bool val);
	inline bool GetUseExposureTweak();

	inline void ForceExposureReadback(bool val);

	// Distance Blur overrides
	inline void SetDistanceBlurStrengthOverride(float val);

	// Script hidof override
	inline void SetScriptHiDOFOverride(bool bOverride, bool bHiDofEnabled, Vector4& params);

	// Post FX Settings

	// Motion Blur Strength
	void SetMotionBlur();

	inline float GetTonemapParam(eTonemapValues value) { return g_filmicTonemapParams[value]; }

	inline void SetSniperSightDOFOverride(bool bEnableOverride, bool bEnableDof, Vector4& dofParams);

#if __BANK || __WIN32PC || RSG_DURANGO || RSG_ORBIS
	inline void SetFXAA(bool enable) { g_FXAAEnable = enable; }
#endif // __BANK || __WIN32PC || RSG_DURANGO || RSG_ORBIS
#if USE_NV_TXAA
	inline void SetTXAA(bool enable) { g_TXAAEnable = enable; }
	bool IsTXAAAvailable();
#endif // USE_NV_TXAA

#if __ASSERT
	inline sysIpcCurrentThreadId GetFogRayMiniMapGenerationThreadId() { return g_FogRayCascadeShadowMapDownsampleThreadId; }
#endif

	// Apply tone-mapping
	float FilmicTweakedTonemap(const float inputValue);

	// Sub Sample Alpha Enabled
	inline bool UseSubSampledAlpha()  { return g_UseSubSampledAlpha; }
	inline void SetUseSubSampledAlpha(bool SSA)  { g_UseSubSampledAlpha = SSA; }

	inline bool UseSinglePassSSA()  { return g_UseSinglePassSSA; }
	inline bool UseSSAOnFoliage()  { return g_UseSSAOnFoliage; }

	inline void SetMarkingSubSampleAlpha( bool v) { g_MarkingSubSamples=v; }
	inline bool GetMarkingSubSampledAlphaSamples()  { return g_MarkingSubSamples; }

	// Brightness/Contrast/Saturation from user settings.
	inline void SetGammaFrontEnd(float g);

#if RSG_PC
	// DOF Replay control
	inline void SetDOFInReplay(bool flag)	{ g_allowDOFInReplay = flag; }
	inline bool UseDOFInReplay()			{ return g_allowDOFInReplay; }
#endif

	// Noise override
	inline void SetNoiseOverride(bool override);
	inline void SetNoisinessOverride(float value); 

	// Exposure
	inline float GetUpdateExposure();
	inline float GetUpdateLuminance();
	
	inline float GetRenderExposure();
	inline float GetRenderLuminance();
	
	inline float GetExposureValue(float luminance);
	inline float GetLinearExposure();
	inline float GetAverageLinearExposure();

	// Tone-mapping
	inline float GetToneMapWhitePoint();
	
	// Screen blur fade
	inline bool TriggerScreenBlurFadeIn(float duration);
	inline bool TriggerScreenBlurFadeOut(float duration);
	inline void DisableScreenBlurFade();
	inline float GetScreenBlurFadeCurrentTime();
	inline bool IsScreenBlurFadeRunning();

	inline void UpdateOnLoadingScreen();

	// Sniper override
	inline float GetSniperScopeOverrideStrength();

	// Bullet impacts
	inline void RegisterBulletImpact(const Vector3& vWeaponPos, f32 damageIntensity, bool bIsEnduranceDamage = false);
	inline void ToggleBulletImpact(bool bEnable);
	inline void ForceBulletImpactOverlaysAfterHud(bool bEnable);
	void DrawBulletImpactOverlaysAfterHud();

	// Fogray Parameters
	void SetFograyParams(bool bShadowActive);
	inline float GetFogRayIntensity();
	inline const grcTexture* GetFogRayRT();
	inline bool GetFogRayMiniShadowMapGenerated();

	// DOF Parameters
	void SetDofParams();
	void LockAdaptiveDofDistance(bool lock);

	// LDR/HDR settings
	void SetLDR8bitHDR8bit();
	XENON_ONLY(void SetLDR10bitHDR10bit();)
	XENON_ONLY(void SetLDR8bitHDR10bit();)
	void SetLDR8bitHDR16bit();
	void SetLDR16bitHDR16bit();

	// Flash Effect:
		void SetFlashParameters(float fMinIntensity, float fMaxIntensity, u32 rampUpDuration, u32 holdDuration, u32 rampDownDuration);
		inline float GetFlashEffectFadeLevel();
		inline atHashString GetFlashEffectModName();
		extern AnimatedPostFX g_defaultFlashEffect;
	// END Flash Effect

	// Pulse Effect:
		void SetPulseParameters(const Vector4& pulseParams, u32 rampUpDuration, u32 holdDuration, u32 rampDownDuration);
		void UpdatePulseEffect();

		extern Vector4 g_vInitialPulseParams;
		extern Vector4 g_vTargetPulseParams;
		extern u32   g_uPulseStartTime;
		extern u32   g_uPulseRampUpDuration;
		extern u32   g_uPulseRampDownDuration;
		extern u32   g_uPulseHoldDuration;
	// END Pulse Effect

	extern float	g_fSniperScopeOverrideStrength;

	inline bool AreExtraEffectsActive();
	inline bool ExtraEffectsRequireBlur();

	void ApplyFXAA(grcRenderTarget *pDstRT, grcRenderTarget *pSrcRT, const Vector4 &scissorRect);

#if PTFX_APPLY_DOF_TO_PARTICLES || APPLY_DOF_TO_ALPHA_DECALS
	void SetupBlendForDOF(grcBlendStateDesc &blendStateDesc, bool forAlphaPass);
#endif //PTFX_APPLY_DOF_TO_PARTICLES || APPLY_DOF_TO_ALPHA_DECALS

#if GTA_REPLAY
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	void CreateReplayThumbnail();
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

	void SetUpdateReplayEffectsWhilePaused(bool val);
	extern bool g_UpdateReplayEffectsWhilePaused;
#endif // GTA_REPLAY

} // namespace PostFx

inline void PostFX::SetScriptHiDOFOverride(bool bOverride, bool bHiDofEnabled, Vector4& params)
{
	g_scriptHighDOFOverrideToggle = bOverride;
	g_scriptHighDOFOverrideEnableDOF = bHiDofEnabled;
	g_scriptHighDOFOverrideParams = params;

}

#if GTA_REPLAY
inline void PostFX::SetUpdateReplayEffectsWhilePaused(bool val) { g_UpdateReplayEffectsWhilePaused = val; }
#endif //GTA_REPLAY

inline void PostFX::SetDistanceBlurStrengthOverride(float val) { g_overrideDistanceBlur = true; g_distanceBlurOverride = val; }

inline void PostFX::SetUseExposureTweak(bool val) { g_enableExposureTweak = val; }
inline bool PostFX::GetUseExposureTweak() { return g_enableExposureTweak; }

inline void PostFX::ForceExposureReadback(bool val) { g_forceExposureReadback = val; }

inline void PostFX::SetUseNightVision(bool val) { g_enableNightVision = val; }
inline bool PostFX::GetGlobalUseNightVision() { return g_enableNightVision; }
#if __BANK
inline bool PostFX::GetGlobalOverrideNightVision() { return g_overrideNightVision; }
#endif // __BANK
inline bool PostFX::GetUseNightVision() { return PostFXParamBlock::GetParams().m_nightVision; }

inline void PostFX::SetUseSeeThrough(bool val) { g_enableSeeThrough = val; }
inline bool PostFX::GetUseSeeThrough() { return PostFXParamBlock::GetParams().m_seeThrough; }

inline float PostFX::GetUpdateExposure() { return g_adaptation.GetUpdateExposure(); }
inline float PostFX::GetUpdateLuminance() { return g_adaptation.GetUpdateLum(); }

inline float PostFX::GetRenderExposure() { return g_adaptation.GetRenderExposure(); }
inline float PostFX::GetRenderLuminance() { return g_adaptation.GetRenderLum(); }

inline float PostFX::GetExposureValue(float luminance) 
{
	const PostFXParamBlock::paramBlock& params = PostFXParamBlock::GetParams();
	return params.m_exposureCurveA * 
		pow(luminance, params.m_exposureCurveB) + 
		params.m_exposureCurveOffset;
}
inline float PostFX::GetLinearExposure() { return g_linearExposure; }

inline float PostFX::GetAverageLinearExposure() { return g_averageLinearExposure; }

inline float PostFX::GetToneMapWhitePoint() { return PostFXParamBlock::GetParams().m_filmicParams[1].z; }

inline float PostFX::GetFogRayIntensity() { return PostFXParamBlock::GetParams().m_fograyIntensity; }

inline const grcTexture* PostFX::GetFogRayRT() {
#if RSG_PC
	if(grcEffect::GetShaderQuality() == 0)
		return grcTexture::NoneBlack;
	else
#endif
		return static_cast<grcTexture*>(PostFX::FogRayRT);
}

inline bool PostFX::GetFogRayMiniShadowMapGenerated()  { return PostFXParamBlock::GetParams().m_bFogRayMiniShadowMapGenerated; }

inline void PostFX::ResetAdaptedLuminance(int count) {g_resetAdaptedLumCount=count;}

inline float PostFX::GetPackScalar10bit(void)
{
	float scalar = (sysThreadType::IsRenderThread()) ? g_adaptation.GetRenderPackScalar() : g_adaptation.GetUpdatePackScalar();
	// Divide by 8 as FP10 range is 0 to 32 so this gives a 0 to 4 range == / 32.0 * 4.0
	scalar /= 8.0f;
	return scalar;
}

inline float PostFX::GetPackScalar8bit(void)
{
	float scalar = (sysThreadType::IsRenderThread()) ? g_adaptation.GetRenderPackScalar() : g_adaptation.GetUpdatePackScalar();
	return scalar;
}


inline void PostFX::SetGammaFrontEnd(float g)
{
	g_gammaFrontEnd = g;
}

// Noise override
inline void PostFX::SetNoiseOverride(bool state)
{
	g_noiseOverride = state;
}

inline void PostFX::SetNoisinessOverride(float value)
{
	g_noisinessOverride = value;
}

inline void PostFX::SetSniperSightDOFOverride(bool bEnableOverride, bool bEnableDof, Vector4& dofParams)
{ 
	g_sniperSightOverrideEnabled = bEnableOverride; 
	g_sniperSightOverrideDisableDOF = !bEnableDof; 
	g_sniperSightOverrideDOFParams = dofParams; 
}

inline void PostFX::SetMotionBlurMaxVelocityScale(float maxVelScalar)
{
	g_motionBlurMaxVelocityMult = maxVelScalar;
}

inline void PostFX::SetForceMotionBlur(bool force)
{
	g_motionBlurCutTestDisabled = force;
}

inline bool PostFX::GetForceMotionBlur()
{
	return g_motionBlurCutTestDisabled;
}

inline float PostFX::GetMotionBlurMaxVelocityScale()
{
	return g_motionBlurMaxVelocityMult;
}

inline void	PostFX::TriggerPedKillOverlay()
{
	g_pedKillOverlay.Trigger();
}

inline void PostFX::TogglePedKillOverlay(bool bEnable)
{
	g_pedKillOverlay.Toggle(bEnable);
}

inline bool PostFX::TriggerScreenBlurFadeIn(float duration)
{
	if (g_screenBlurFade.IsPlaying())
	{
		return false;
	}

	g_screenBlurFade.SetDuration(duration);
	g_screenBlurFade.Trigger(false);
	return true;
}

inline bool PostFX::TriggerScreenBlurFadeOut(float duration)
{
	if (g_screenBlurFade.HasFadedIn() == false)
	{
		return false;
	}

	g_screenBlurFade.SetDuration(duration);
	g_screenBlurFade.Trigger(true);
	return true;
}

inline void PostFX::DisableScreenBlurFade()
{
	g_screenBlurFade.Reset();
}

inline float PostFX::GetScreenBlurFadeCurrentTime()
{
	return g_screenBlurFade.GetCurrentTime();
}

inline bool PostFX::IsScreenBlurFadeRunning()
{
	return g_screenBlurFade.IsFading();
}

inline float PostFX::GetFlashEffectFadeLevel()
{
	return g_defaultFlashEffect.GetFadeLevel();
}

inline atHashString PostFX::GetFlashEffectModName()
{
	return g_defaultFlashEffect.GetModifierName();
}

inline void PostFX::UpdateOnLoadingScreen()
{
	g_screenBlurFade.UpdateOnLoadingScreen();
}

inline float PostFX::GetSniperScopeOverrideStrength()
{
	return g_fSniperScopeOverrideStrength;
}

inline bool PostFX::AreExtraEffectsActive()
{
	const PostFXParamBlock::paramBlock& params = PostFXParamBlock::GetParams();
	return (params.m_distortionParams.IsNonZero()	||	// distortion/chromatic aberration enabled
			params.m_blurVignettingParams.x > 0.0f	||	// blur vignetting
			ExtraEffectsRequireBlur());
}

inline bool PostFX::ExtraEffectsRequireBlur()
{
	return (g_screenBlurFade.IsPlaying()					||	// screen blur triggered by script
		PostFXParamBlock::GetParams().m_screenBlurFade > 0.0f);		// screen blur enabled by time cycle
}

inline void PostFX::RegisterBulletImpact(const Vector3& vWeaponPos, f32 damageIntensity, bool bIsEnduranceDamage)
{
	g_bulletImpactOverlay.RegisterBulletImpact(vWeaponPos, damageIntensity, bIsEnduranceDamage);
}

inline void PostFX::ForceBulletImpactOverlaysAfterHud(bool bEnable)
{
	g_bulletImpactOverlay.m_drawAfterHud = bEnable;
}

inline void PostFX::ToggleBulletImpact(bool bEnable)
{
	g_bulletImpactOverlay.Toggle(bEnable);
}

inline bool PostFX::IsCutsceneDofTODOverrideActive()
{
	return g_cutsceneDofTODOverride.IsActive();
}

inline bool PostFX::BeginCutsceneDofTODOverride()
{
	return g_cutsceneDofTODOverride.Begin();
}

inline bool PostFX::EndCutsceneDofTODOverride()
{
	return g_cutsceneDofTODOverride.End();
}

inline void PostFX::AddCutsceneDofTODOverride(float hourFrom, float hourTo, float overrideValue)
{
	g_cutsceneDofTODOverride.AddSample(hourFrom, hourTo, overrideValue);
}

inline void PostFX::SetCutsceneDofTODDefaultOutOfRangeOverride(float defOverride)
{
	g_cutsceneDofTODOverride.SetDefaultOutOfRangeOverride(defOverride);
};

#endif //_POST_PROCESS_FX_H_
