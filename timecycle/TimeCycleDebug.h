///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	TimeCycleDebug.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 Aug 2008
//	WHAT:	 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TIMECYCLEDEBUG_H
#define TIMECYCLEDEBUG_H

#include "vector\vector3.h"
#include "timecycle\tcdebug.h"
#include "control/replay/ReplaySettings.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													


#if TC_DEBUG
///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////													

namespace rage
{
	class bkBank;
	class bkButton;
	class tcKeyframe;
	class tcKeyframeUncompressed;
}


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CTimeCycleDebug
///////////////////////////////////////////////////////////////////////////////

class CTimeCycleDebug
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void					Init    							();
		void					Update								();

#if __BANK
		void					InitLevelWidgets					();
		static void				CreateLevelWidgets					();
		void					ShutdownLevelWidgets				();
#endif
		// access functions
		bool					GetDisableCutsceneModifiers			() const				{return m_disableCutsceneModifiers;}
		bool					GetDisableBoxModifiers				() const				{return m_disableBoxModifiers;}
		bool					GetDisableInteriorModifiers			() const				{return m_disableInteriorModifiers;}
		bool					GetDisableSecondaryInteriorModifiers() const				{return m_disableSecondaryInteriorModifiers;}
		bool					GetDisableBaseUnderWaterModifier	() const				{return m_disableBaseUnderWaterModifier;}
		bool					GetDisableUnderWaterModifiers		() const				{return m_disableUnderWaterModifiers;}
		bool					GetDisableUnderWaterCycle			() const				{return m_disableUnderWaterCycle;}
		bool					GetDisableHighAltitudeModifier		() const				{return m_disableHighAltitudeModifer;}
		bool					GetDisableGlobalModifiers			() const				{return m_disableGlobalModifiers;}
		bool					GetDisableModifierFog				() const				{return m_disableModifierFog;}
		bool					GetDisableIntModifierAmbientScale	() const				{return m_disableIntModifierAmbientScale;}
		bool					GetDisableBoxModifierAmbientScale	() const				{return m_disableBoxModifierAmbientScale;}
		bool					GetDisableExteriorVisibleModifier   () const				{return m_disableExteriorVisibleModifier;}

		bool					GetDisableFogAndHaze				() const				{return m_disableFogAndHaze;}
		void					SetDisableFogAndHaze				(bool bDisable)			{m_disableFogAndHaze = bDisable;}

		bool					GetRenderSunPosition				() const				{return m_renderSunPosition;}

		bool					GetDumpSunMoonPosition				() const				{return m_dumpSunMoonPosition;}
		void					SetDumpSunMoonPosition				(bool bDump)			{m_dumpSunMoonPosition = bDump;}

		int						GetHourDivider						() const				{return m_hourDivider;}

		bool					GetDisplayModifierInfo				() const				{return m_displayModifierInfo;}
		void					SetDisplayModifierInfo				(bool bDisplay)			{m_displayModifierInfo = bDisplay;}

#if GTA_REPLAY
		bool					GetDisableReplayTwoPhaseSystem		() const				{return m_disableTwoPhaseSystem; }
#endif

	private: //////////////////////////

		// update helpers
		void					DisplayKeyframeGroup				(const tcKeyframeUncompressed& keyframe, const s32 debugGroupId, const s32 x, s32 y, bool fullPrecision) const;

		// callbacks - visual settings
		static	void			ReloadVisualSettings				();				// should be moved
		static  void			DumpSunMoonPosition					();
		

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// sun direction
static	float					m_sunRollDegreeAngle;
static	float					m_sunYawDegreeAngle;
static	float					m_moonRollDegreeOffset;
static  float					m_moonWobbleFreq;
static  float					m_moonWobbleAmp;
static  float					m_moonWobbleOffset;
static  float					m_moonCycleOverride;
static	bool					m_renderSunPosition;
static  bool					m_dumpSunMoonPosition;
static  int						m_hourDivider;

		// modifier debug
static	bool					m_disableCutsceneModifiers;
static	bool 					m_disableBoxModifiers;
static	bool 					m_disableInteriorModifiers;
static	bool 					m_disableSecondaryInteriorModifiers;
static	bool 					m_disableBaseUnderWaterModifier;
static	bool 					m_disableUnderWaterModifiers;
static	bool					m_disableUnderWaterCycle;
static	bool					m_disableHighAltitudeModifer;
static	bool 					m_disableGlobalModifiers;
static	bool 					m_disableModifierFog;
static	bool 					m_disableBoxModifierAmbientScale;
static	bool 					m_disableIntModifierAmbientScale;
static	bool 					m_disableExteriorVisibleModifier;
static	bool					m_disableFogAndHaze;

		// Ambient Scale Probes
static	bool					m_probeEnabled;
static	bool					m_probeShowNaturalAmb;
static	bool					m_probeShowArtificialAmb;
static	bool					m_probeAttachToCamera;
static	Vector3					m_probeCenter;
static	float					m_probeCamOffset;
static	int						m_probeXCount;
static	int						m_probeYCount;
static	int						m_probeZCount;
static	float					m_probeXOffset;
static	float					m_probeYOffset;
static	float					m_probeZOffset;
static	float					m_probeRadius;
	
		
		// debug
static	bool					m_displayKeyframeData;	
static	bool					m_displayKeyframeDataFullPrecision;
static	s32						m_displayKeyframeDataX;
static	s32						m_displayKeyframeDataY;
static	bool					m_displayVarMapCacheUse;
static	bool					m_displayTCboxUse;
static	s32						m_currentDisplayGroup;
static	bool					m_displayModifierInfo;
#if GTA_REPLAY
static bool						m_disableTwoPhaseSystem;
#endif // GTA_REPLAY

		// widget data
static	bkBank*					m_pBank;
static	bkButton*				m_pCreateButton;

};

#endif // TC_DEBUG

#endif // TIMECYCLEDEBUG_H
