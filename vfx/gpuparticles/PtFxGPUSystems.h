///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxGPUSystems.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	23 November 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PTFX_GPU_SYSTEMS_H
#define	PTFX_GPU_SYSTEMS_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "gpuptfx/ptxgpubase.h"
#include "gpuptfx/ptxgpudrop.h"

// game
#include "Renderer/SpuPM/SpuPmMgr.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define PTFXGPU_FINAL_MAX_SETTINGS		(12)
#if __DEV
#define PTFXGPU_MAX_SETTINGS			(PTFXGPU_FINAL_MAX_SETTINGS * 2)			
#else
#define PTFXGPU_MAX_SETTINGS			PTFXGPU_FINAL_MAX_SETTINGS
#endif

#define PTFXGPU_MIN_DISTORTION_VISIBILITY_ENABLE	(0.01f)


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

class CWeatherGpuFxFromXmlFile;


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CPtFxGPULitShaderVars  ////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

// sets the specific constant variables for an effect

class CPtFxGPULitShaderVars
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

public: ///////////////////////////

	void						Set						(grmShader* pShader, float lightIntensityMult);
	void						SetupShader				(grmShader* pShader);

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

private: //////////////////////////

	grcEffectVar				m_lightIntensityMultId;
};


//  CPtFxGPUSettings  /////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxGPUSettings
{	
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CPtFxGPUManager;
	friend class CPtFxGPUOldSystem;
	friend class CPtFxGPUDropSystem;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	protected: ////////////////////////

		atHashValue					m_hashValue;

		u32							m_driveType;
		float						m_windInfluence;
		float						m_gravity;

#if !__FINAL
		char						m_name				[32];
#endif

};

//  CPtFxGPUDropSettings  /////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxGPUDropSettings : public CPtFxGPUSettings
{	
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CPtFxGPU;
	friend class CPtFxGPUDropSystem;

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	private: //////////////////////////

		ptxgpuDropEmitterSettings	m_dropEmitterSettings;
		ptxgpuDropRenderSettings	m_dropRenderSettings;

};



//  CPtFxGPUSystem  ///////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxGPUSystem
{	
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CPtFxGPUManager;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

									CPtFxGPUSystem				()				{m_pPtxGpu = NULL; m_overrideCurrLevel = 1.0f; m_useOverrideSettings = false;}

		void						Shutdown					();
		void						PreRender					(grcViewport& viewport, float deltaTime, bool resetParticles);
		
		void						ClearNumSettings			()				{m_numSettings = 0;}
		float						GetCurrLevel				()				{return m_currLevel;}
		float						GetCurrWindInfluence		()				{return m_currWindInfluence;}

		void						SeUseOverrideSettings		(bool bUseOverrideSettings)		{m_useOverrideSettings = bUseOverrideSettings;}
		void						SetOverrideCurrLevel		(float overrideCurrLevel)		{m_overrideCurrLevel = overrideCurrLevel;}

		static void					GlobalPreUpdate				();

	private:

		PTXGPU_USE_WIND_DISTURBANCE_ONLY(static void UpdateWindDisturbanceTextures();)


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	protected: ////////////////////////

		// old settings specific
 		u32							m_numSettings;

		// current state
		float						m_currLevel;
		float						m_overrideCurrLevel;

		float						m_currWindInfluence;
		float						m_currGravity;

		ptxgpuBase*					m_pPtxGpu;

		bool						m_useOverrideSettings;

};

//  CPtFxGPUDropSystem  ///////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxGPUDropSystem : public CPtFxGPUSystem
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void						Init						(unsigned int systemId, int numParticles, const char* updateShaderName, const char* updateShaderTechnique, const char* renderShaderName, const char* renderShaderTechnique, bool bUseSoft, grcRenderTarget* pParentTarget = NULL);
		void						Update						(s32 prevGpuFxIndex, s32 nextGpuFxIndex, float weatherInterp, float maxVfxRegionLevel, float currVfxRegionLevel);
		void						Interpolate					(s32 thirdGpuFxIndex, float weatherInterp);

		void						PreRender					(grcViewport& viewport, float deltaTime, bool resetParticles);
		void						Render						(grcViewport& viewport, ptxRenderSetup* pRenderSetup, Vec3V_In vCamVel, float camPosZ, float groundPosZ BANK_ONLY(, Vec4V_In vCamAngleLimits));
		void						PauseSimulation				();

		void						LoadFromXmlFile				(const CWeatherGpuFxFromXmlFile &WeatherGpuFxFromXmlFile);

		s32							GetSettingIndex				(atHashValue hashValue);

		bool						GetEmitterBounds			(Mat44V_ConstRef camMatrix, Vec3V_Ref emitterMin, Vec3V_Ref emitterMax);

		bool						HasDistortion				() {return (m_currDropRenderSettings.backgroundDistortionVisibilityPercentage > PTFXGPU_MIN_DISTORTION_VISIBILITY_ENABLE);}
		int							GetBackGroundBlurLevel		() {return (HasDistortion()?m_currDropRenderSettings.backgroundDistortionBlurLevel:-1);}

		//Override setters
		void						SetOverrideBoxSize			(Vec3V_In overrideBoxSize)		{m_overrideBoxSize = overrideBoxSize;}
		void						SetOverrideBoxOffset		(Vec3V_In overrideBoxOffset)	{m_overrideBoxOffset = overrideBoxOffset;}

		// debug functions
#if __BANK
		void						InitWidgets					();
		void						RenderDebug					(const grcViewport& viewport, const Color32& color);
		void						DebugUpdateRenderSettings		(s32 prevGpuFxIndex, s32 nextGpuFxIndex, float weatherInterp, float currVfxRegionLevel);
#endif

		const ptxgpuDropRenderSettings&	GetRenderSetings() const	{ return m_currDropRenderSettings; }
		void							SetRenderSettingsRT(const ptxgpuDropRenderSettings& settings)			{ m_currDropRenderSettingsRT = settings; }


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: //////////////////////////

		// drop settings specific
		CPtFxGPUDropSettings		m_gpuDropSettings			[PTFXGPU_MAX_SETTINGS];

		ptxgpuDropEmitterSettings	m_currDropEmitterSettings;
		ptxgpuDropRenderSettings	m_currDropRenderSettings;
		ptxgpuDropRenderSettings	m_currDropRenderSettingsRT;
		s32							m_currFxIndex;

		Vec3V						m_overrideBoxOffset;
		Vec3V						m_overrideBoxSize;
		bool						m_useSoft;

};



#endif // PTFX_GPU_SYSTEMS_H





