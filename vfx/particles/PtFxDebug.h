///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxDebug.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	17 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PTFX_DEBUG_H
#define	PTFX_DEBUG_H

#if __BANK

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// game
#include "Vfx/Particles/PtFxDefines.h"
#include "grcore/config.h"
#include "vectormath/classes.h"
#include "Vfx/vfx_shared.h"

///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class ptxFxList;
}


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CPtFxDebug ////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxDebug
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		void				Init					();	
		void				InitWidgets				();	
		void				Update					();	

		// access functions
		bool				GetDisablePtFxRender	()								{return m_disablePtFxRender;}

		bool				GetExplosionPerformanceTest		()						{return m_ExplosionPerformanceTest;}
		void				ResetExplosionPerformanceTest		()					{m_ExplosionPerformanceTest = false;}

#if PTFX_ALLOW_INSTANCE_SORTING	
		bool				GetDisableSortedRender	()								{return m_disableSortedRender;}
		bool				GetDisableSortedEffects	()								{return m_disableSortedEffects;}
		bool				GetForceSortedEffects	()								{return m_forceSortedEffects;}
		bool				GetOnlyFlaggedSortedEffects()							{return m_onlyFlaggedSortedEffects;}
		bool				GetDisableSortedVehicleOffsets()						{return m_disableSortedVehicleOffsets;}
		bool				GetRenderSortedEffectsBoundingBox()						{return m_renderSortedEffectsBoundingBox;}
		bool				GetRenderSortedVehicleEffectsBoundingBox()				{return m_renderSortedVehicleEffectsBoundingBox;}
		bool				GetRenderSortedVehicleEffectsCamDebug()					{return m_renderSortedVehicleEffectsCamDebug;}
		ScalarV_Out			GetSortedEffectsVehicleCamDirOffset()					{return m_sortedEffectsVehicleCamDirOffset;}
		ScalarV_Out			GetSortedEffectsVehicleCamPosOffset()					{return m_sortedEffectsVehicleCamPosOffset;}
		ScalarV_Out			GetSortedEffectsVehicleMinDistFromCenter()				{return m_sortedEffectsVehicleMinDistFromCenter;}

#endif

#if PTFX_RENDER_IN_MIRROR_REFLECTIONS
		bool				GetDisableRenderMirrorReflections()						{return m_disableRenderMirrorReflections;}
#endif //PTFX_RENDER_IN_MIRROR_REFLECTIONS

		bool				GetDisableHighBucket	()								{return m_disableHighBucket;}
		bool				GetDisableMediumBucket  ()								{return m_disableMediumBucket;}
		bool				GetDisableLowBucket		()								{return m_disableLowBucket;}
		bool				GetDisableWaterRefractionBucket()						{return m_disableWaterRefractionBucket;}
		bool				GetDisableRefractionBucket()							{return m_disableRefractionBucket;}

		bool				GetDrawWireframe		()								{return m_drawWireframe;}

#if __D3D11 || RSG_ORBIS
		float				GetTessellationFactor	()								{return m_tessellationFactor;}
#endif

#if RMPTFX_USE_PARTICLE_SHADOWS
		float				GetGlobalParticleShadowIntensity	()					{return m_globalParticleShadowIntensity;}
		bool				IsParticleShadowIntensityOverridden()					{return m_globalParticleShadowIntensityOveride;}		
		float				GetGlobalParticleShadowSlopeBias()						{return m_globalParticleShadowSlopeBias;}
		float				GetGlobalParticleShadowBiasRange()						{return m_globalParticleShadowBiasRange;}
		float				GetGlobalParticleShadowBiasRangeFalloff()				{return m_globalParticleShadowBiasRangeFalloff;}

#endif //RMPTFX_USE_PARTICLE_SHADOWS

#if PTFX_APPLY_DOF_TO_PARTICLES
		float				GetGlobalParticleDofAlphaScale()							{return m_globalParticleDofAlphaScale;}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

#if USE_SHADED_PTFX_MAP
		bool				GetDisableShadowedPtFx()								{return m_disableShadowedPtFx;}
		bool				GetRenderShadowedPtFxBox()								{return m_renderShadowedPtFxBox;}
		ScalarV_Out			GetShadowedPtFxRange()									{return m_shadowedPtFxRange;}
		ScalarV_Out			GetShadowedPtFxLoHeight()								{return m_shadowedPtFxLoHeight;}
		ScalarV_Out			GetShadowedPtFxHiHeight()								{return m_shadowedPtFxHiHeight;}
		float				GetShadowedPtFxBlurKernelSize()							{return m_shadowedPtFxBlurKernelSize;}
#endif //USE_SHADED_PTFX_MAP

		bool				GetDisableDownsampling	()								{return m_disableDownsampling;}
		bool				GetRenderAllDownsampled	()								{return m_renderAllDownsampled;}
		void				SetRenderAllDownsampled (bool makeAllLowRes)			{m_renderAllDownsampled = makeAllLowRes;}

		bool				GetDynamicQualitySystemEnabled ()						{return m_dynamicQualitySystemEnabled;}
		void				SetDynamicQualitySystemEnabled (bool enabled)			{m_dynamicQualitySystemEnabled = enabled;}
		float				GetDQUpdateFrequency()									{return m_qualityUpdateTime;}
		float				GetDQTotalRenderTimeThreshold()							{return m_totalRenderTimeThreshold;}
		float				GetDQLowResRenderTimeThreshold()						{return m_lowResRenderTimeThreshold;}
		float				GetDQEndDownsampleThreshold()							{return m_endDownsamplingThreshold;}

		bool				GetRenderDebugCollisionProbes()							{return m_renderDebugCollisionProbes;}
		s32					GetRenderDebugCollisionProbesFrames()					{return m_renderDebugCollisionProbesFrames;}
		bool				GetRenderDebugCollisionPolys()							{return m_renderDebugCollisionPolys;}

		bool				GetOverrideDeltaTime()									{return m_overrideDeltaTime;}
		float				GetOverrideDeltaTimeValue()								{return m_overrideDeltaTimeValue;}

		bool				GetDisableWaterRefractionPostProcess()					{return m_disableWaterRefractionPostProcess;}

		bool				GetAlwaysPassShouldBeRenderedRuleChecks()				{return m_alwaysPassShouldBeRenderedRuleChecks;}

		// misc functions
static	void				RemoveFxFromPlayer		();
static	void				RemoveFxFromPlayerVeh	();
static	void				RemoveFxAroundPlayer	();
static	void				MovePlayerPedForward	();
static	void				MovePlayerPedBack		();
static	void				MovePlayerVehForward	();
static	void				MovePlayerVehBack		();
		void				UpdatePerformanceTest	();


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		
					
	private: //////////////////////////

		// debug variables
		bool				m_disablePtFxRender;

		bool				m_ExplosionPerformanceTest;

#if PTFX_ALLOW_INSTANCE_SORTING				
		bool				m_disableSortedRender;
		bool				m_disableSortedEffects;
		bool				m_forceSortedEffects;
		bool				m_onlyFlaggedSortedEffects;
		bool				m_disableSortedVehicleOffsets;
		bool				m_renderSortedEffectsBoundingBox;
		bool				m_renderSortedVehicleEffectsBoundingBox;
		bool				m_renderSortedVehicleEffectsCamDebug;
		ScalarV				m_sortedEffectsVehicleCamDirOffset;
		ScalarV				m_sortedEffectsVehicleCamPosOffset;
		ScalarV				m_sortedEffectsVehicleMinDistFromCenter;
#endif 

		bool				m_disableHighBucket;
		bool				m_disableMediumBucket;
		bool				m_disableLowBucket;
		bool				m_disableWaterRefractionBucket;
		bool				m_disableRefractionBucket;
		bool				m_drawWireframe;


#if PTFX_RENDER_IN_MIRROR_REFLECTIONS
		bool				m_disableRenderMirrorReflections;
#endif //PTFX_RENDER_IN_MIRROR_REFLECTIONS

#if __D3D11 || RSG_ORBIS
		float				m_tessellationFactor;
#endif
#if RMPTFX_USE_PARTICLE_SHADOWS
		float				m_globalParticleShadowIntensity;
		bool				m_globalParticleShadowIntensityOveride;
		float				m_globalParticleShadowSlopeBias;
		float				m_globalParticleShadowBiasRange;
		float				m_globalParticleShadowBiasRangeFalloff;
#endif //#if RMPTFX_USE_PARTICLE_SHADOWS

#if PTFX_APPLY_DOF_TO_PARTICLES
		float				m_globalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

#if USE_SHADED_PTFX_MAP
		bool				m_disableShadowedPtFx;
		bool				m_renderShadowedPtFxBox;
		ScalarV				m_shadowedPtFxRange;
		ScalarV				m_shadowedPtFxLoHeight;
		ScalarV				m_shadowedPtFxHiHeight;
		float				m_shadowedPtFxBlurKernelSize;
#endif //USE_SHADED_PTFX_MAP

		bool				m_disableDownsampling;
		bool				m_renderAllDownsampled;	

		bool				m_dynamicQualitySystemEnabled;
		float				m_qualityUpdateTime;
		float				m_totalRenderTimeThreshold;
		float				m_lowResRenderTimeThreshold;
		float				m_endDownsamplingThreshold;

		bool				m_renderDebugCollisionProbes;
		s32					m_renderDebugCollisionProbesFrames;
		bool				m_renderDebugCollisionPolys;

		bool				m_disableWaterRefractionPostProcess;

		float				m_interiorFxLevel;
		float				m_currAirResistance;

		bool				m_overrideDeltaTime;
		float				m_overrideDeltaTimeValue;

		bool				m_alwaysPassShouldBeRenderedRuleChecks;

		static bool			m_moveUsingTeleport;
		static float		m_movePlayerPedDist;
		static float		m_movePlayerVehDist;


}; // CPtFxDebug


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////



#endif // __BANK
#endif // PTFX_DEBUG_H



