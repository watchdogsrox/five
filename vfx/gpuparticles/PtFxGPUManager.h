///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxGPUManager.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	23 November 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PTFX_GPU_H
#define	PTFX_GPU_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "rmptfx/ptxeffectinst.h"
#include "grcore/viewport.h"

// game
#include "Vfx/GPUParticles/PtFxGPUSystems.h"
#include "vfx/vfx_shared.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

class CWeatherGpuFxFromXmlFile;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CPtFxGPURenderSetup  //////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

// sets the lighting globals for rendering the effect

class CPtFxGPURenderSetup : public ptxRenderSetup
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		bool						PreDraw					(const ptxEffectInst* pEffectInst, const ptxEmitterInst* pEmitterInst, Vec3V_In vMin, Vec3V_In vMax, u32 shaderHashName, grmShader* pShader) const;
		bool						PostDraw				(const ptxEffectInst* pEffectInst, const ptxEmitterInst* pEmitterInst) const;
};




//  CPtFxGPUManager  //////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CPtFxGPUManager : public datBase
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		bool						Init						();
		void						Shutdown					();

		void						LoadSettingFromXmlFile		(const CWeatherGpuFxFromXmlFile &WeatherGpuFxFromXmlFile);

static	void						UpdateVisualDataSettings	();

		void						Update						(float deltaTime);
		void						Render						(Vec3V_ConstRef vCamVel);

static	void						PreRender					(bool bGamePaused, bool resetDropSystem, bool resetMistSystem, bool resetGroundSystem);

		// access functions
		Vec4V						GetDefaultLight				()						{return m_vDefaultLight;}

		float						GetCurrLevel				()						{return Max(m_dropSystem.GetCurrLevel(), m_mistSystem.GetCurrLevel(), m_groundSystem.GetCurrLevel());}

		float						GetCurrWindInfluence		()						{return m_dropSystem.GetCurrWindInfluence();}

		void						GetEmittersBounds			(Mat44V_ConstRef camMatrix, Vec3V_Ref emitterMin, Vec3V_Ref emitterMax);

		CPtFxGPUDropSystem&			GetDropSystem()				{ return m_dropSystem; }
		CPtFxGPUDropSystem&			GetMistSystem()				{ return m_mistSystem; }
		CPtFxGPUDropSystem&			GetGroundSystem()			{ return m_groundSystem; }
		s32							GetDropSettingIndex			(atHashValue hashValue) {return m_dropSystem.GetSettingIndex(hashValue);}
		s32							GetMistSettingIndex			(atHashValue hashValue) {return m_mistSystem.GetSettingIndex(hashValue);}
		s32							GetGroundSettingIndex		(atHashValue hashValue) {return m_groundSystem.GetSettingIndex(hashValue);}

		void						ClearNumSettings			()						{m_dropSystem.ClearNumSettings(); m_mistSystem.ClearNumSettings(); m_groundSystem.ClearNumSettings();}


		bool						GetResetDropSystem			()						{return m_resetDropSystem;}
		bool						GetResetMistSystem			()						{return m_resetMistSystem;}
		bool						GetResetGroundSystem		()						{return m_resetGroundSystem;}

		void						ClearResetSystemFlags		()						{m_resetDropSystem=false; m_resetMistSystem=false; m_resetGroundSystem=false;}

		//Script functions
		void						SetUseOverrideSettings		(bool bUseOverrideSettings);
		void						SetOverrideCurrLevel		(float overrideCurrLevel);
		void						SetOverrideBoxSize			(Vec3V_In overrideBoxSize);
		void						SetOverrideBoxOffset		(Vec3V_In overrideBoxOffset);

		// debug functions
#if __BANK
		void						InitWidgets					();
		static	void				DebugResetParticles			();
		void						DebugRenderUpdate			();

		void						UpdateOverrideSettings		();
#endif


	private: //////////////////////////

		// init helper functions
		void						LoadData					();


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	private: //////////////////////////

		CPtFxGPUDropSystem			m_dropSystem;
		CPtFxGPUDropSystem			m_mistSystem;
		CPtFxGPUDropSystem			m_groundSystem;

		// misc
		ptxRenderSetup				*m_ptxRenderSetup; 
		grcViewport					m_viewport;
		Vec4V						m_vDefaultLight;

		float						m_deltaTime;
		float						m_groundPosZ[2];
		float						m_camPosZ[2];

		bool						m_isPedShelteredInVehicle[2];
		bool						m_wasUnderwaterLastFrame;

		bool						m_wasWeatherDropSystemActiveLastFrame;
		bool						m_wasWeatherMistSystemActiveLastFrame;
		bool						m_wasWeatherGroundSystemActiveLastFrame;

		bool						m_wasRegionDropSystemActiveLastFrame;
		bool						m_wasRegionMistSystemActiveLastFrame;
		bool						m_wasRegionGroundSystemActiveLastFrame;

		bool						m_resetDropSystem;
		bool						m_resetMistSystem;
		bool						m_resetGroundSystem;

#if APPLY_DOF_TO_GPU_PARTICLES
		grcBlendStateHandle			m_BlendStateDof;
#endif //APPLY_DOF_TO_GPU_PARTICLES

		// debug variables
#if __BANK					
		bkGroup*					m_pBankGroup;
		bkCombo*					m_pRegionCombo;
		s32							m_currRegionComboId;
		s32							m_debugGpuFxPrevDropIndex;
		s32							m_debugGpuFxPrevMistIndex;
		s32							m_debugGpuFxPrevGroundIndex;
		s32							m_debugGpuFxNextDropIndex;
		s32							m_debugGpuFxNextMistIndex;
		s32							m_debugGpuFxNextGroundIndex;
		float						m_debugCurrVfxRegionDropLevel;
		float						m_debugCurrVfxRegionMistLevel;
		float						m_debugCurrVfxRegionGroundLevel;
		bool						m_forceRegionGpuPtFx;

		float						m_debugWindInfluence;
		float						m_debugGravity;
		bool						m_disableRendering;
		bool						m_debugRender;

		float						m_camAngleForwardMin;
		float						m_camAngleForwardMax;
		float						m_camAngleUpMin;
		float						m_camAngleUpMax;

		//override settings

		bool						m_useOverrideSettings;
		float						m_overrideCurrLevel;
		Vec3V						m_overrideBoxSize;
		Vec3V						m_overrideBoxOffset;
#endif

}; // CPtFxGPUManager


///////////////////////////////////////////////////////////////////////////////
//	EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern dev_float GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_WIDTH_SCALE;
extern dev_float GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_HEIGHT_SCALE;


#endif // PTFX_GPU_H





