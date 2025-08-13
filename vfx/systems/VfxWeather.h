///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWeather.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	16 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_WEATHER_H
#define	VFX_WEATHER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage

// game
#include "Task/System/TaskHelpers.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXWEATHER_NUM_WEATHER_PROBES		(5)


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxGroundProbe_s
{
	Vec3V	m_vStartPos;
	Vec3V	m_vEndPos;

	float	m_camZ;
	float	m_heightAboveGround;

	s8		m_vfxProbeId;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxWeather
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void				Init					(unsigned initMode);
		void				Shutdown				(unsigned shutdownMode);

		void				Update					(float deltaTime);
		void				Render					(Vec3V_ConstRef vCamVel);

		// debug functions
#if __BANK
		void				InitWidgets			();
#endif

		// access functions
		CPtFxGPUManager&	GetPtFxGPUManager		()						{return m_ptFxGPUManager;}

		float				GetCurrRain				()						{return m_currRain;}
		float				GetCurrSnow				()						{return m_currSnow;}
		float				GetCurrSnowMist			() const				{return m_currSnowMist;}
		float				GetCurrSandstorm		()						{return m_currSandstorm;}
		Vec3V_ConstRef		GetCamVel				()						{return m_vCamVel;}
		bool				GetWeatherProbeResult	(s32 i)					{return m_weatherProbeResults[i];}
		float				GetCamHeightAboveGround	()						{return m_heightAboveGroundDampened;}


	private: //////////////////////////

		// probe helpers
		void				InitProbes				();
		void				ShutdownProbes			();
		void				UpdateProbes			();

		// update helpers
		void				UpdateCameraLensFx		(Mat34V_In camMat, float fxLevel);
		void				UpdateWindDebrisFx		(Mat34V_In camMat, float fxAlpha, float windAirEvo, float windWaterEvo, float camSpeedEvo);

		// particle systems
//		void				UpdatePtFxRainMist		(Mat34V_In camMat, float fxAlpha, float rainEvo, float windEvo, float camSpeedEvo);
//		void				UpdatePtFxSnowMist		(Mat34V_In camMat, float fxAlpha, float snowEvo, float windEvo, float camSpeedEvo);
//		void				UpdatePtFxSandstorm		(Mat34V_In camMat, float fxAlpha, float sandEvo, float windEvo, float camSpeedEvo);
		void				UpdatePtFxWindDebris	(Mat34V_In camMat, float fxAlpha, float windEvo, float camSpeedEvo);
		void				RemovePtFxWindDebris	();
//		void				UpdatePtFxUnderWater	(Mat34V_In camMat, float fxAlpha, float windEvo, float camSpeedEvo, float depth);
		void				UpdatePtFxFog			(Mat34V_In camMat, float fxAlpha, float fogEvo, float windEvo, float camSpeedEvo);
		void				UpdatePtFxClouds		(Mat34V_In camMat, float fxAlpha, float cloudEvo, float speedEvo);

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		CPtFxGPUManager		m_ptFxGPUManager;

		float				m_currRain;
		float				m_currSnow;
		float				m_currSnowMist;
		float				m_currSandstorm;

		ATTR_UNUSED u32		m_pad0;
		Vec3V				m_vCamPos;
		Vec3V				m_vCamVel;

		CAsyncProbeHelper	m_weatherProbes			[VFXWEATHER_NUM_WEATHER_PROBES];
		ATTR_UNUSED u32		m_pad1[2];
		Vec3V				m_vWeatherProbeOffsets	[VFXWEATHER_NUM_WEATHER_PROBES];
		bool				m_weatherProbeResults	[VFXWEATHER_NUM_WEATHER_PROBES];
		s32					m_currWeatherProbeId;

		VfxGroundProbe_s	m_groundProbe;
		float				m_heightAboveGroundDampened;

		// debug variables
#if __BANK
//		bool				m_disableRainMist;	
//		bool				m_disableSnowMist;	
//		bool				m_disableSandstorm;				
		bool				m_disableFog;			
		bool				m_disableClouds;	
		bool				m_disableWindDebris;
//		bool				m_disableUnderWater;

		bool				m_rainOverrideActive;
		float				m_rainOverrideVal;

		bool				m_snowOverrideActive;
		float				m_snowOverrideVal;

		bool				m_snowMistOverrideActive;
		float				m_snowMistOverrideVal;

		bool				m_sandstormOverrideActive;
		float				m_sandstormOverrideVal;
#endif


}; // CVfxWeather



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxWeather			g_vfxWeather;


#endif // VFX_WEATHER_H


///////////////////////////////////////////////////////////////////////////////
