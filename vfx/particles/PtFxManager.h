///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Fx.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	31 Oct 2005
//	WHAT:	Implementation of the Rage Particle effects
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLE_FX_H
#define	PARTICLE_FX_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "crskeleton/skeleton.h"
#include "grcore/effect.h"
#include "grcore/viewport.h"
#include "rmptfx/ptxeffectinst.h"
#include "rmptfx/ptxfxlist.h"
#include "rmptfx/ptxupdatetask.h"
#include "system/ipc.h"

// framework
#include "vfx/vfxlist.h"
#include "vfx/ptfx/ptfxCallbacks.h"
#include "vfx/ptfx/ptfxManager.h"

// game
#include "Streaming/StreamingRequest.h"
#include "Vfx/Particles/PtFxCollision.h"
#include "Vfx/Particles/PtFxDebug.h"
#include "Vfx/Particles/PtFxEntity.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{	
	class grcRenderTarget;
	class ptxDrawList;
}

class CEntity;
struct VfxWeaponInfo_s;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////	

#define PTFX_MAX_STORED_LIGHTS						(64)
#define PTFX_MAX_STORED_FIRES						(16)
#define PTFX_MAX_STORED_DECALS						(16)
#define PTFX_MAX_STORED_DECAL_POOLS					(16)
#define PTFX_MAX_STORED_DECAL_TRAILS				(16)
#define PTFX_MAX_STORED_LIQUIDS						(16)
#define PTFX_MAX_STORED_GLASS_IMPACTS				(16)
#define PTFX_MAX_STORED_GLASS_SMASHES				(16)
#define PTFX_MAX_STORED_GLASS_SHATTERS				(16)

#define USE_DOWNSAMPLED_PARTICLES					(__XENON || __PS3)
#define USE_DOWNSAMPLE_PTFX_WITH_SSAO				(USE_DOWNSAMPLED_PARTICLES && __PS3)


#define PTFX_SHADOWED_RANGE_DEFAULT					(ScalarVConstant<FLOAT_TO_INT(60.0f)>()) 
#define PTFX_SHADOWED_LOW_HEIGHT_DEFAULT			(ScalarVConstant<FLOAT_TO_INT(0.25f)>())
#define PTFX_SHADOWED_HIGH_HEIGHT_DEFAULT			(ScalarVConstant<FLOAT_TO_INT(2.0f)>())
#define PTFX_SHADOWED_BLUR_KERNEL_SIZE_DEFAULT 		(2.0f)

#define PTFX_DYNAMIC_QUALITY_UPDATE_TIME				(0.1f)
#define PTFX_DYNAMIC_QUALITY_TOTAL_TIME_THRESHOLD		(3.0f)
#define PTFX_DYNAMIC_QUALITY_LOWRES_TIME_THRESHOLD		(2.0f)
#define PTFX_DYNAMIC_QUALITY_END_LOWRES_TIME_THRESHOLD	(0.8f)

#if PTFX_APPLY_DOF_TO_PARTICLES
#define PTFX_PARTICLE_DOF_ALPHA_SCALE					(1.60f)
#endif //PTFX_APPLY_DOF_TO_PARTICLES

///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////	

enum PtFxDrawListId_e
{
	PTFX_DRAWLIST_QUALITY_LOW = 0,
	PTFX_DRAWLIST_QUALITY_MEDIUM,
	PTFX_DRAWLIST_QUALITY_HIGH,

	NUM_PTFX_DRAWLISTS
};

enum ptfxRenderFlags
{
	PTFX_RF_MEDRES_DOWNSAMPLE = 1 << 0,
	PTFX_RF_LORES_DOWNSAMPLE = 1 << 1
};


enum ptfxGameCustomFlags
{
	PTFX_CONTAINS_REFRACTION											= BIT(0),
	PTFX_WATERSURFACE_RENDER_ABOVE_WATER								= BIT(1),
	PTFX_WATERSURFACE_RENDER_BELOW_WATER								= BIT(2),
	PTFX_DRAW_SHADOW_CULLED												= BIT(3),
	PTFX_IS_VEHICLE_INTERIOR											= BIT(4),
	PTFX_WATERSURFACE_RENDER_BOTH										= (PTFX_WATERSURFACE_RENDER_ABOVE_WATER | PTFX_WATERSURFACE_RENDER_BELOW_WATER),
};


#define PTXPROGVAR_POST_PROCESS_FLIPDEPTH_NEARPLANEFADE_PARAM			(PTXPROGVAR_GAME_VAR_1)
#define PTXPROGVAR_WATER_FOG_PARAM										(PTXPROGVAR_GAME_VAR_2)
#define PTXPROGVAR_WATER_FOG_COLOR_BUFFER								(PTXPROGVAR_GAME_VAR_3)
#define PTXPROGVAR_SHADEDMAP											(PTXPROGVAR_GAME_VAR_4)
#define PTXPROGVAR_SHADEDPARAMS											(PTXPROGVAR_GAME_VAR_5)
#define PTXPROGVAR_SHADEDPARAMS2										(PTXPROGVAR_GAME_VAR_6)
#define PTXPROGVAR_FOG_MAP_PARAM										(PTXPROGVAR_GAME_VAR_7)
#define PTXPROGVAR_ACTIVE_SHADOW_CASCADES								(PTXPROGVAR_GAME_VAR_8)
#define PTXPROGVAR_NUM_ACTIVE_SHADOW_CASCADES							(PTXPROGVAR_GAME_VAR_9)


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct ptfxGlassImpactInfo
{
	VfxWeaponInfo_s* pFxWeaponInfo;
	RegdEnt RegdEnt;
	Vec3V vPos;
	Vec3V vNormal;
	bool isExitFx;
};

struct ptfxGlassSmashInfo
{
	RegdPhy regdPhy;
	Vec3V vPosHit;
	Vec3V vPosCenter;
	Vec3V vNormal;
	float force;
	bool isWindscreen;
};

struct ptfxGlassShatterInfo
{
	Vec3V vPos;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CSetLightingConstants  ////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CSetLightingConstants : public ptxRenderSetup
{

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CSetLightingConstants(const char* unlitShaderName)
			: m_unlitShader(atHashValue(unlitShaderName))
		{}

		bool PreDraw(const ptxEffectInst*, const ptxEmitterInst*, Vec3V_In vMin_In, Vec3V_In vMax_In, u32 shaderHashName, grmShader* pShader) const;

	private: ///////////////////////////

		static void CacheDeferredShadingGlobals(grmShader* pShader);
		static void SetDeferredShadingGlobals(grmShader* pShader, grcEffectTechnique techniqueId);

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: ///////////////////////////

		u32	m_unlitShader;

		static bool					ms_defShadingConstantsCached;
		static grcEffectTechnique	ms_projSpriteTechniqueId;

		static grcEffectVar ms_shaderVarIdDepthTexture;
		static grcEffectVar ms_shaderVarIdFogRayTexture;
		static grcEffectVar ms_shaderVarIdOOScreenSize;
		static grcEffectVar ms_shaderVarIdProjParams;
		static grcEffectVar ms_shaderVarIdShearParams0;
		static grcEffectVar ms_shaderVarIdShearParams1;
		static grcEffectVar ms_shaderVarIdShearParams2;

};


//  CPtFxManager  /////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxManager
{	
#if GTA_REPLAY
	friend class CReplayMgrInternal;
#endif // GTA_REPLAY

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		void					Init					(unsigned initMode);	
		void					InitShaderOverrides		();
		void					InitSessionSync			();
		void					Shutdown				(unsigned shutdownMode);	

		void					RemoveScript			(scrThreadId scriptThreadId);

#if RSG_PC
static	void					DeviceLost				();
static	void					DeviceReset				();
#endif
									
		void 					Update					(float deltaTime);	
		void					UpdateDynamicQuality	(void);
		void					UpdateDataCapsule		(ptxEffectInst* pFxInst);
		void					WaitForCallbackEffectsToFinish	();
		void					StartLocalEffectsUpdate	();

		void					BeginFrame				();	

#if __BANK
		void					ProcessExplosionPerformanceTest();
#endif
		
 static void					UpdateDepthBuffer		(grcRenderTarget* depthBuffer);
#if RSG_PC
 static void					SetDepthWriteEnabled	(bool depthWriteEnabled);
#endif
#if USE_SHADED_PTFX_MAP
 static	void					UpdateShadowedPtFxBuffer();
#endif
#if PTFX_APPLY_DOF_TO_PARTICLES
 static void					ClearPtfxDepthAlphaMap();
 static void					LockPtfxDepthAlphaMap(bool useReadOnlyDepth);
 static void					UnLockPtfxDepthAlphaMap();
 static void					FinalizePtfxDepthAlphaMap();
 static bool					UseParticleDOF()
								{return GRCDEVICE.GetDxFeatureLevel() > 1000 WIN32PC_ONLY( && CSettingsManager::GetInstance().GetSettings().m_graphics.m_PostFX > CSettings::Medium);}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

		void					Render					(grcRenderTarget* pFrameBuffer, grcRenderTarget* pFrameSampler, bool bRenderIntoRefraction = false);	
		void					RenderSimple			(grcRenderTarget* pBackBuffer, grcRenderTarget* pDepthBuffer);
		void					RenderManual			(ptxEffectInst* pFxInst BANK_ONLY(, bool debugRender));
#if RMPTFX_USE_PARTICLE_SHADOWS
		void					RenderShadows			(s32 cascadeIndex);
#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
		void					RenderShadowsAllCascades(s32 noOfCascades, grcViewport *pViewports, Vec4V *pWindows, bool bUseInstancedShadow = false);
#endif
		bool					HasAnyParticleShadowsRendered()						{ return m_bAnyParticleShadowsRendered; }
		void					SetParticleShadowsRenderedFlag(bool value)			{ m_bAnyParticleShadowsRendered = value; }
		static void				ResetParticleShadowsRenderedFlag();
#endif
		
		void					ProcessStoredData		();

#if USE_SHADED_PTFX_MAP
		Vec4V_Out				GetPtfxShadedParams();
		Vec4V_Out				GetPtfxShadedParams2()								{ return m_shadedPtfxParams2; }
		void					SetPtfxShadedParams2(Vec4V_In shadedPtfxParams2)	{ m_shadedPtfxParams2 = shadedPtfxParams2; }
		grcRenderTarget*		GetPtfxShadedMap()									{ return m_shadedParticleBuffer; }
#endif
#if PTFX_APPLY_DOF_TO_PARTICLES
		grcRenderTarget*		GetPtfxDepthMap()										{ return m_particleDepthMap;}
		grcRenderTarget*		GetPtfxAlphaMap()										{ return m_particleAlphaMap;}
		const grcTexture*		GetPtfxDepthTexture()									{ return MSAA_ONLY(1 ? m_particleDepthMapResolved :) m_particleDepthMap; }
		const grcTexture*		GetPtfxAlphaTexture()									{ return MSAA_ONLY(1 ? m_particleAlphaMapResolved :) m_particleAlphaMap; }
# if DEVICE_MSAA
		void					ResolvePtfxMaps();
# endif
#endif //PTFX_APPLY_DOF_TO_PARTICLES
#if __BANK
		void					InitWidgets				();
#endif //__BANK
	
		void					RelocateEntityPtFx		(CEntity* pEntity);
		void					StopPtFxOnEntity		(CEntity* pEntity);
		void					RemovePtFxFromEntity	(CEntity* pEntity, bool isRespawning=false);
		void					RemovePtFxInRange		(Vec3V_In vPos, float range);

		void					SetForceVehicleInteriorFlag(bool bSet, scrThreadId scriptThreadId);
		bool					GetForceVehicleInteriorFlag() { return m_forceVehicleInteriorFlag; }

#if __BANK
		// access to sub systems
		CPtFxDebug&				GetGameDebugInterface	()						{return m_gameDebug;}
#endif // _BANK

#if USE_DOWNSAMPLED_PARTICLES
		bool					ShouldDownsample		();
		void					SetDisableDownsamplingThisFrame() {m_disableDownsamplingThisFrame = true;}
#endif

#if PTFX_ALLOW_INSTANCE_SORTING
		CPtFxSortedManager&		GetSortedInterface		()						{return m_ptFxSorted;}
#endif 
		CPtFxCollision&			GetColnInterface		()						{return m_ptFxCollision;}

		// buffered requests
		void					StoreLight				(ptfxLightInfo& lightInfo);
		void					StoreFire				(ptfxFireInfo& fireInfo);
		void					StoreDecal				(ptfxDecalInfo& decalInfo);
		void					StoreDecalPool			(ptfxDecalPoolInfo& decalPoolInfo);
		void					StoreDecalTrail			(ptfxDecalTrailInfo& decalTrailInfo);
		void					StoreLiquid				(ptfxLiquidInfo& liquidInfo);
		void					StoreFogVolume			(ptfxFogVolumeInfo& fogVolumeInfo);
		void					DrawFogVolume			(ptfxFogVolumeInfo& fogVolumeInfo);

		void					StoreGlassImpact		(ptfxGlassImpactInfo& glassImpactInfo);
		void					StoreGlassSmash			(ptfxGlassSmashInfo& glassSmashInfo);
		void					StoreGlassShatter		(ptfxGlassShatterInfo& glassShatterInfo);

		void					ProcessLateStoredLights	();

		void					ResetStoredLights		()						{m_numLightInfos = 0;}

		void					UpdateVisualDataSettings();

		//accessors
		bool					IsRenderingIntoWaterRefraction()						{return m_isRenderingIntoWaterRefraction[g_RenderThreadIndex];}

#if __XENON
		bool					GetUseDownRezzedPtfx()									{return m_useDownrezzedPtfx;}
#endif

		ptxDrawInterface::ptxMultipleDrawInterface*		GetMultiDrawInterface() { return m_pMulitDrawInterface[g_RenderThreadIndex]; }

#if GTA_REPLAY
		void					SetReplayScriptPtfxAssetID(s32 slotId) {m_ReplayScriptPtfxAssetID = slotId;}
		//After a clip has been saved out we also need to reset this for mission and continuous modes
		void					ResetReplayScriptRequestAsset()	{ m_RecordReplayScriptRequestAsset = true; }
#endif

	private: //////////////////////////

		// init helper functions
		void		 			InitGlobalAssets		();
		void		 			InitThread				();
		void		 			InitRMPTFX				();
		void		 			InitRMPTFXTexture		();
#if USE_SHADED_PTFX_MAP
		void					InitShadowedPtFxBuffer  ();
#endif 
#if USE_DOWNSAMPLED_PARTICLES
		void		 			InitDownsampling		();
#endif

		// shutdown helper functions
		void		 			ShutdownGlobalAssets	();
		void		 			ShutdownThread			();
		void		 			ShutdownRMPTFX			();
#if USE_DOWNSAMPLED_PARTICLES
		void		 			ShutdownDownsampling	();
#endif

		// update helper functions
		void					ProcessStoredLights		();
		void					ProcessStoredFires		();
		void					ProcessStoredDecals		();
		void					ProcessStoredDecalPools	();
		void					ProcessStoredDecalTrails();
		void					ProcessStoredLiquids	();

		void					ProcessStoredGlassImpacts();
		void					ProcessStoredGlassSmashes();
		void					ProcessStoredGlassShatters();

		// misc helper functions
		void					TryToSoakPed			(CPed* pPed, ptxDataVolume& dataVolume);

		// update thread functions
		void					WaitForUpdateToFinish	();
		void					PreUpdateThread			();
static	void					UpdateThread			(void* _that);


		// render Helper functions
		void					RenderPtfxIntoWaterRefraction();
		void					RenderPtfxQualityLists	(bool bIsCameraUnderwater);
		void					RenderHighResolution();
		void					RenderLowResolution();
		void					RenderLowQualityList();
		void					RenderRefractionBucket();

#if USE_DOWNSAMPLED_PARTICLES
		// downsample render helper functions
		void					InitDownsampleRender	(grcRenderTarget* pColourSampler);
		void					ShutdownDownsampleRender();
#endif

#if PTFX_ALLOW_INSTANCE_SORTING
static	bool					ShouldSceneSort			(ptxEffectInst* pFxInst);
#endif

		// callbacks
static	void 					EffectInstStartFunctor	(ptxEffectInst* pFxInst);
static	void					EffectInstStopFunctor	(ptxEffectInst* pFxInst);
static	void					EffectInstDeactivateFunctor(ptxEffectInst* pFxInst);
//static	bool 					EffectInstSpawnFunctor	(ptxEffectInst* pFxInstParent, ptxEffectInst* pFxInstChild);
static	float					EffectInstDistToCamFunctor(ptxEffectInst* pFxInst);
static	bool					EffectInstOcclusionFunctor(Vec4V* vCullSphere);
static	void					EffectInstPostUpdateFunctor(ptxEffectInst* pFxInst);
static	void					ParticleRuleInitFunctor(ptxParticleRule* pPtxParticleRule);
static	bool					ParticleRuleShouldBeRenderedFunctor(ptxParticleRule* pPtxParticleRule);
static	bool					EffectInstShouldBeRenderedFunctor(ptxEffectInst* pFxInst, bool bSkipManualDraw);
static	void					EffectInstInitFunctor	(ptxEffectInst* pFxInst);
static	void					EffectInstUpdateSetupFunctor (ptxEffectInst* pFxInst);
static	void					EffectInstUpdateCullingFunctor (ptxEffectInst* pFxInst);
static	void					EffectInstUpdateFinalizeFunctor	(ptxEffectInst* pFxInst);

static	grcDepthStencilStateHandle	OverrideDepthStencilStateFunctor(bool bWriteDepth, bool bTestDepth, ptxProjectionMode projMode);
static	grcBlendStateHandle			OverrideBlendStateFunctor(int blendMode);

		void					SetGlobalConstants();

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		
					
	private: //////////////////////////
	
		// asset requests
#if __DEV
		strRequestArray<256>	m_streamingRequests;
#else
		strRequestArray<8>		m_streamingRequests;
#endif

		// sub systems
#if __BANK
		CPtFxDebug				m_gameDebug;
#endif

#if PTFX_ALLOW_INSTANCE_SORTING
		CPtFxSortedManager		m_ptFxSorted;
#endif
		CPtFxCollision			m_ptFxCollision;

		// script
		bool					m_forceVehicleInteriorFlag;
		scrThreadId				m_forceVehicleInteriorFlagScriptThreadId;

		// update thread specific
		sysIpcThreadId			m_updateThreadId;
		sysIpcSema				m_startUpdateSema;
		sysIpcSema				m_finishUpdateSema;
		volatile float			m_nextUpdateDelta;
		bool					m_startedUpdate;
		volatile bool			m_blockFxListRemoval;

		// viewport
		grcViewport				m_currViewport;
		atArray<grcViewport>	m_currShadowViewports;

		// the current time collectors for ptfx dynamic quality
		u32						m_numGPUFrames;
		float					m_gpuTimeMedRes[2];
		float					m_gpuTimeLowRes[2];
		float					m_gpuTimerSumMedRes;
		float					m_gpuTimerSumLowRes;
		float					m_updateDeltaSum;
		u8						m_ptfxRenderFlags[2];
		u8						m_ptfxCurrRenderFlags;

		// drawlists
		ptxDrawList*			m_pDrawList[NUM_PTFX_DRAWLISTS];

		//render states
		bool					m_isRenderingRefractionPtfxList[NUMBER_OF_RENDER_THREADS];
		bool					m_isRenderingIntoWaterRefraction[NUMBER_OF_RENDER_THREADS];
		bool					m_IsRefractPtfxPresent[2];
#if FPS_MODE_SUPPORTED
		bool					m_isFirstPersonCamActive[2];
#endif

		grcDepthStencilStateHandle	m_VehicleInteriorAlphaDepthStencilStates[2][2];	// [0,0]: write off, test off; [0, 1]: write off, test on;
		grcDepthStencilStateHandle	m_MirrorReflection_Simple_DepthStencilState[2][2];	// [0,0]: write off, test off; [0, 1]: write off, test on;
#if RMPTFX_USE_PARTICLE_SHADOWS
		grcBlendStateHandle	m_ShadowBlendState;	// [0,0]: write off, test off; [0, 1]: write off, test on;
#endif
#if PTFX_APPLY_DOF_TO_PARTICLES
		grcBlendStateHandle	m_ptfxDofBlendNormal;
		grcBlendStateHandle	m_ptfxDofBlendAdd;
		grcBlendStateHandle	m_ptfxDofBlendSubtract;
		grcBlendStateHandle	m_ptfxDofBlendCompositeAlpha;
		grcBlendStateHandle	m_ptfxDofBlendCompositeAlphaSubtract;
#endif


		//shaderOverrides
		ptxShaderTemplateOverride m_ptxSpriteWaterRefractionShaderOverride;
		ptxShaderTemplateOverride m_ptxTrailWaterRefractionShaderOverride;

#if USE_DOWNSAMPLED_PARTICLES
		// downsample buffer vars
		grcConditionalQuery		m_PtfxDownsampleQuery;
		
		grcRenderTarget* 		m_pPrevFrameBuffer;
		grcRenderTarget*		m_pPrevFrameBufferSampler;
		grcRenderTarget* 		m_pPrevDepthBuffer;
		grcRenderTarget*		m_pLowResPtfxBuffer;
#if __PPU
		grcRenderTarget* 		m_pPrevPatchedDepthBuffer;
#endif
#endif //USE_DOWNSAMPLED_PARTICLES

#if PTFX_APPLY_DOF_TO_PARTICLES
		grcRenderTarget*	    m_particleDepthMap;
		grcRenderTarget*	    m_particleAlphaMap;
# if DEVICE_MSAA
		grcRenderTarget*	    m_particleDepthMapResolved;
		grcRenderTarget*	    m_particleAlphaMapResolved;
# endif
#endif //PTFX_APPLY_DOF_TO_PARTICLES

#if __XENON
		bool 					m_useDownrezzedPtfx;
#endif

#if RMPTFX_USE_PARTICLE_SHADOWS
		grcEffectGlobalVar		m_GlobalShadowBiasVar;
		float					m_GlobalShadowIntensityValue;
		float					m_GlobalShadowBiasRange;
		float					m_GlobalShadowSlopeBias;
		float					m_GlobalshadowBiasRangeFalloff;
		bool					m_bAnyParticleShadowsRendered;
#endif
#if PTFX_APPLY_DOF_TO_PARTICLES
		grcEffectGlobalVar		m_GlobalParticleDofAlphaScaleVar;
		float					m_globalParticleDofAlphaScale;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

		// shaded particles
#if USE_SHADED_PTFX_MAP
		grcRenderTarget*		m_shadedParticleBuffer;
		grmShader*              m_shadedParticleBufferShader;
		grcEffectTechnique      m_shadedParticleBufferShaderTechnique;
		grcEffectVar			m_shadedParticleBufferShaderShadowMapId;
		grcEffectVar            m_shadedParticleBufferShaderParamsId;
		grcEffectVar            m_shadedParticleBufferShaderParams2Id;
		grcEffectVar            m_shadedParticleBufferShaderCloudSampler;
		grcEffectVar            m_shadedParticleBufferShaderSmoothStepSampler;
		Vec4V					m_shadedPtfxParams2;
		grcTexture*				m_CloudTexture;	
#endif			

		//grcTexture*				m_SmoothStempTexture;

		// buffer requests
		s32						m_numLightInfos;
		ptfxLightInfo			m_lightInfos[PTFX_MAX_STORED_LIGHTS];
		
		s32						m_numFireInfos;
		ptfxFireInfo			m_fireInfos[PTFX_MAX_STORED_FIRES];

		s32						m_numDecalInfos;
		ptfxDecalInfo			m_decalInfos[PTFX_MAX_STORED_DECALS];

		s32						m_numDecalPoolInfos;
		ptfxDecalPoolInfo		m_decalPoolInfos[PTFX_MAX_STORED_DECAL_POOLS];

		s32						m_numDecalTrailInfos;
		ptfxDecalTrailInfo		m_decalTrailInfos[PTFX_MAX_STORED_DECAL_TRAILS];

		s32						m_numLiquidInfos;
		ptfxLiquidInfo			m_liquidInfos[PTFX_MAX_STORED_LIQUIDS];

		s32						m_numGlassImpactInfos;
		ptfxGlassImpactInfo		m_glassImpactInfos[PTFX_MAX_STORED_GLASS_IMPACTS];

		s32						m_numGlassSmashInfos;
		ptfxGlassSmashInfo		m_glassSmashInfos[PTFX_MAX_STORED_GLASS_SMASHES];

		s32						m_numGlassShatterInfos;
		ptfxGlassShatterInfo	m_glassShatterInfos[PTFX_MAX_STORED_GLASS_SHATTERS];

		// settings
#if USE_DOWNSAMPLED_PARTICLES
		bool					m_disableDownsamplingThisFrame;
#endif

		ptxDrawInterface::ptxMultipleDrawInterface*		m_pMulitDrawInterface[NUMBER_OF_RENDER_THREADS];

		//Critical Sections for callbacks
		sysCriticalSectionToken m_storeFireCS;
		sysCriticalSectionToken m_storeDecalCS;
		sysCriticalSectionToken m_storeDecalPoolCS;
		sysCriticalSectionToken m_storeDecalTrailCS;
		sysCriticalSectionToken m_storeLiquidCS;
		sysCriticalSectionToken m_storeGlassImpactCS;
		sysCriticalSectionToken m_storeGlassSmashCS;
		sysCriticalSectionToken m_storeGlassShatterCS;
		sysCriticalSectionToken m_storeLightCS;
		sysCriticalSectionToken m_storeFogVolumeCS;

#if GTA_REPLAY
		bool m_RecordReplayScriptRequestAsset;
		s32 m_ReplayScriptPtfxAssetID;
#endif //GTA_REPLAY
}; // CPtFxManager


//  CPtFxCallbacks  ///////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxCallbacks : public ptfxCallbacks
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// general
		virtual	void GetMatrixFromBoneIndex(Mat34V_InOut boneMtx, const void* pEntity, int boneIndex,bool useAltSkeleton=false);

		// ptfxmanager 
		virtual void UpdateFxInst(ptxEffectInst* pFxInst);
		virtual bool RemoveFxInst(ptxEffectInst* pFxInst);
		virtual void ShutdownFxInst(ptxEffectInst* pFxInst);

		// ptfxreg
		virtual void RemoveScriptResource(int ptfxScriptId);

		// ptfxreg - entity referencing
		virtual void AddEntityRef(const void* pEntity, void** ppReference) {static_cast<const CEntity*>(pEntity)->AddKnownRef(ppReference);}
		virtual void RemoveEntityRef(const void* pEntity, void** ppReference) {static_cast<const CEntity*>(pEntity)->RemoveKnownRef(ppReference);}

		// ptfxattach - entity queries
		virtual void* GetEntityDrawable(const void* pEntity) {return static_cast<const CEntity*>(pEntity)->GetDrawable();}

		// behaviours
		virtual Vec3V_Out GetGameCamPos();
		virtual float GetGameCamWaterZ();
		virtual float GetWaterZ(Vec3V_In vPos, ptxEffectInst* pEffectInst);
		virtual void GetRiverVel(Vec3V_In vPos, Vec3V_InOut vVel);
		virtual float GetGravitationalAcceleration();
		virtual float GetAirResistance();

		virtual void AddLight(ptfxLightInfo& lightInfo);
		virtual void AddFire(ptfxFireInfo& fireInfo);
		virtual void AddDecal(ptfxDecalInfo& decalInfo);
		virtual void AddDecalPool(ptfxDecalPoolInfo& decalPoolInfo);
		virtual void AddDecalTrail(ptfxDecalTrailInfo& decalTrailInfo);
		virtual void AddLiquid(ptfxLiquidInfo& liquidInfo);
		virtual void AddFogVolume(ptfxFogVolumeInfo& fogVolumeInfo);

		// render behaviors
		virtual void DrawFogVolume(ptfxFogVolumeInfo& fogVolumeInfo);

		// replay
		virtual bool CanGetTriggeredInst();
		virtual bool CanGetRegisteredInst(bool bypassReplayPauseCheck);

};

// classes
class CPtFxShaderVarUtils
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

public: ///////////////////////////

	static const char* GetProgrammedVarName(ptxShaderProgVarType type);


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

private: //////////////////////////

	static const char* sm_progVarNameTable[PTXPROGVAR_GAME_COUNT];

};

//  CPtfxShaderCallbacks  ///////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////

class CPtfxShaderCallbacks
{

public:
	static void InitProgVars(ptxShaderTemplate* pPtxShaderTemplate);
	static void SetShaderVars(ptxShaderTemplate* pPtxShaderTemplate);

};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern 	CPtFxManager			g_ptFxManager;	

extern 	dev_float 				PTFX_SOAK_SLOW_INC_LEVEL_LOW;
extern 	dev_float 				PTFX_SOAK_SLOW_INC_LEVEL_HIGH;

extern 	dev_float 				PTFX_SOAK_MEDIUM_INC_LEVEL_LOW;
extern 	dev_float 				PTFX_SOAK_MEDIUM_INC_LEVEL_HIGH;

extern 	dev_float 				PTFX_SOAK_FAST_INC_LEVEL_LOW;
extern 	dev_float 				PTFX_SOAK_FAST_INC_LEVEL_HIGH;


#endif // PARTICLE_FX_H


///////////////////////////////////////////////////////////////////////////////
