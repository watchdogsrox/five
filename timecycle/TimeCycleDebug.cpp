///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	TimeCycleDebug.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#include "timecycle/tcdebug.h"

#if TC_DEBUG

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////	

#include "TimeCycleDebug.h"

// Rage headers
#include "bank/button.h"
#include "bank/msgbox.h"

#include "fwutil/xmacro.h"
#include "grcore/debugdraw.h"

// Game headers
#include "camera/CamInterface.h"
#include "Game/Clock.h"
#include "Game/Weather.h"
#include "Renderer/PostProcessFX.h"
#include "Scene/DataFileMgr.h"
#include "scene/portals/PortalTracker.h"
#include "timecycle/tcinstbox.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "Debug/Rendering/DebugDeferred.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

PARAM(disableReplayTwoPhaseSystem, "Enable the replay two phase recording");

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  STATIC VARIABLES
///////////////////////////////////////////////////////////////////////////////

// sun direction
float					CTimeCycleDebug::m_sunRollDegreeAngle;
float					CTimeCycleDebug::m_sunYawDegreeAngle;
float					CTimeCycleDebug::m_moonRollDegreeOffset;
float					CTimeCycleDebug::m_moonWobbleFreq;
float					CTimeCycleDebug::m_moonWobbleAmp;
float					CTimeCycleDebug::m_moonWobbleOffset;
float					CTimeCycleDebug::m_moonCycleOverride = -1.0f;
bool					CTimeCycleDebug::m_renderSunPosition;
bool					CTimeCycleDebug::m_dumpSunMoonPosition;
int						CTimeCycleDebug::m_hourDivider;

// modifier debug
bool					CTimeCycleDebug::m_disableCutsceneModifiers;
bool 					CTimeCycleDebug::m_disableBoxModifiers;
bool 					CTimeCycleDebug::m_disableInteriorModifiers;
bool 					CTimeCycleDebug::m_disableSecondaryInteriorModifiers;
bool 					CTimeCycleDebug::m_disableBaseUnderWaterModifier;
bool 					CTimeCycleDebug::m_disableUnderWaterModifiers;
bool 					CTimeCycleDebug::m_disableUnderWaterCycle;
bool 					CTimeCycleDebug::m_disableHighAltitudeModifer;
bool 					CTimeCycleDebug::m_disableGlobalModifiers;
bool 					CTimeCycleDebug::m_disableModifierFog;
bool 					CTimeCycleDebug::m_disableBoxModifierAmbientScale;
bool 					CTimeCycleDebug::m_disableIntModifierAmbientScale;
bool 					CTimeCycleDebug::m_disableExteriorVisibleModifier;
bool					CTimeCycleDebug::m_disableFogAndHaze = false;

// Ambient Scale Probes
bool					CTimeCycleDebug::m_probeEnabled;
bool					CTimeCycleDebug::m_probeShowNaturalAmb;
bool					CTimeCycleDebug::m_probeShowArtificialAmb;
bool					CTimeCycleDebug::m_probeAttachToCamera;
Vector3					CTimeCycleDebug::m_probeCenter;
float					CTimeCycleDebug::m_probeCamOffset;
int						CTimeCycleDebug::m_probeXCount;
int						CTimeCycleDebug::m_probeYCount;
int						CTimeCycleDebug::m_probeZCount;
float					CTimeCycleDebug::m_probeXOffset;
float					CTimeCycleDebug::m_probeYOffset;
float					CTimeCycleDebug::m_probeZOffset;
float					CTimeCycleDebug::m_probeRadius;

// widget data
bkBank*					CTimeCycleDebug::m_pBank;
bkButton*				CTimeCycleDebug::m_pCreateButton;

// debug
bool					CTimeCycleDebug::m_displayKeyframeData;
bool					CTimeCycleDebug::m_displayKeyframeDataFullPrecision;
s32						CTimeCycleDebug::m_displayKeyframeDataX;
s32						CTimeCycleDebug::m_displayKeyframeDataY;
bool					CTimeCycleDebug::m_displayVarMapCacheUse;
bool					CTimeCycleDebug::m_displayTCboxUse;
s32						CTimeCycleDebug::m_currentDisplayGroup;
bool					CTimeCycleDebug::m_displayModifierInfo;

#if GTA_REPLAY
bool					CTimeCycleDebug::m_disableTwoPhaseSystem;
#endif // GTA_REPLAY



///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CTimeCycleDebug
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycleDebug::Init   						()
{
	// sun direction
	m_sunRollDegreeAngle				= ( RtoD * g_timeCycle.GetSunRollAngle());
	m_sunYawDegreeAngle					= ( RtoD * g_timeCycle.GetSunYawAngle());
	m_moonRollDegreeOffset				= ( RtoD * g_timeCycle.GetMoonRollOffset());
	m_moonWobbleFreq					= g_timeCycle.GetMoonWobbleFrequency();
	m_moonWobbleAmp						= g_timeCycle.GetMoonWobbleAmplitude();
	m_moonWobbleOffset					= g_timeCycle.GetMoonWobbleOffset();
	m_renderSunPosition					= false;
	m_dumpSunMoonPosition				= false;
	m_hourDivider						= 8;
	
	// modifier debug
	m_disableCutsceneModifiers			= false;
	m_disableBoxModifiers				= false;
	m_disableInteriorModifiers			= false;
	m_disableSecondaryInteriorModifiers = false;
	m_disableBaseUnderWaterModifier		= false;
	m_disableUnderWaterModifiers		= false;
	m_disableUnderWaterCycle			= false;
	m_disableHighAltitudeModifer		= false;

	m_disableGlobalModifiers			= false;
	m_disableModifierFog				= false;
	m_disableBoxModifierAmbientScale	= false;
	m_disableIntModifierAmbientScale	= false;

	// Ambient Scale Probes
	m_probeEnabled = false;
	m_probeShowNaturalAmb = true;
	m_probeShowArtificialAmb = true;
	m_probeAttachToCamera = true;
	m_probeCenter = Vector3(0.0f,0.0f,0.0f);
	m_probeCamOffset = 10.0f;
	m_probeXCount = 5;
	m_probeYCount = 5;
	m_probeZCount = 5;
	m_probeXOffset = 1.0f;
	m_probeYOffset = 1.0f;
	m_probeZOffset = 1.0f;
	m_probeRadius = 0.20f;

	// widget data
	m_pBank								= NULL;
	m_pCreateButton						= NULL;

	// debug
	m_displayKeyframeData				= false;
	m_displayKeyframeDataFullPrecision	= false;
	m_displayKeyframeDataX				= 70;
	m_displayKeyframeDataY				= 10;
	
	m_displayVarMapCacheUse				= false;
	m_displayTCboxUse					= false;
	m_currentDisplayGroup				= 0;
	m_displayModifierInfo				= true;

#if GTA_REPLAY
	m_disableTwoPhaseSystem				= PARAM_disableReplayTwoPhaseSystem.Get();
#endif // GTA_REPLAY
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycleDebug::Update						()
{
	const Vec3V vCamPos = VECTOR3_TO_VEC3V( camInterface::GetPos() );
	g_tcInstBoxMgr.UpdateDebug( vCamPos );

	// display
	if (m_displayTCboxUse)
	{
		g_timeCycle.ShowMeBoxUse();
	}
	
	if (m_displayVarMapCacheUse)
	{
		tcModifier::ShowMeVarMapCacheUse();
	}
	
	if (m_displayKeyframeData)
	{
#if __ASSERT
		bool currentTCState = g_timeCycle.m_IsTimeCycleValid;
		g_timeCycle.m_IsTimeCycleValid = true;
#endif // __ASSERT		
		DisplayKeyframeGroup(g_timeCycle.GetCurrUpdateKeyframe(), m_currentDisplayGroup, m_displayKeyframeDataX, m_displayKeyframeDataY, m_displayKeyframeDataFullPrecision);
#if __ASSERT		
		g_timeCycle.m_IsTimeCycleValid = currentTCState;
#endif // __ASSERT		
	}
	
	g_timeCycle.SetSunRollAngle(( DtoR * m_sunRollDegreeAngle));
	g_timeCycle.SetSunYawAngle(( DtoR * m_sunYawDegreeAngle));
	g_timeCycle.SetMoonRollOffset(( DtoR * m_moonRollDegreeOffset));
	g_timeCycle.SetMoonWobbleAmplitude(m_moonWobbleAmp);
	g_timeCycle.SetMoonWobbleFrequency(m_moonWobbleFreq);
	g_timeCycle.SetMoonWobbleOffset(m_moonWobbleOffset);
	if (m_moonCycleOverride > -1.0f)
	{
		g_timeCycle.SetMoonCycleOverride(Saturate(m_moonCycleOverride));
	}
	
	
	// ambient scale grid.
	if( m_probeEnabled )
	{
		if( m_probeAttachToCamera )
		{
			m_probeCenter = camInterface::GetPos();
			m_probeCenter += camInterface::GetFront() * m_probeCamOffset;
		}
		
		fwInteriorLocation intLoc = CPortalVisTracker::GetInteriorLocation();
		Vector3 basePos = m_probeCenter;
		int xx = m_probeXCount;
		int yy = m_probeYCount;
		int zz = m_probeZCount;
		
		float startX = -((float)(xx) * m_probeXOffset)/ 2.0f;
		float startY = -((float)(yy) * m_probeYOffset)/ 2.0f;
		float startZ = -((float)(zz) * m_probeZOffset)/ 2.0f;
		float baseX;
		float baseY;
		float baseZ;
		int x;
		int y;
		int z;
		
		for(x=0,baseX = startX;x<xx;x++,baseX+=m_probeXOffset)
		{
			for(y=0,baseY = startY;y<yy;y++,baseY+=m_probeYOffset)
			{
				for(z=0,baseZ = startZ;z<zz;z++,baseZ+=m_probeZOffset)
				{
					Vector3 pos= basePos + Vector3(baseX,baseY,baseZ);
					Vec2V ambientScale = g_timeCycle.CalcAmbientScale(VECTOR3_TO_VEC3V(pos),intLoc);
					Color32 col;
					if( m_probeShowNaturalAmb == true && m_probeShowArtificialAmb == false)
					{
						if( ambientScale.GetXf() < 0.0f || ambientScale.GetXf() > 1.0f )
						{
							col = Color32(0xff,0x00,0xff);
						}
						else
						{
							col = Color32(ambientScale.GetXf(),ambientScale.GetXf(),ambientScale.GetXf());
						}
					}
					else if( m_probeShowNaturalAmb == false && m_probeShowArtificialAmb == true)
					{
						if( ambientScale.GetYf() < 0.0f || ambientScale.GetYf() > 1.0f ) 
						{
							col = Color32(0xff,0x00,0xff);
						}
						else
						{
							col = Color32(ambientScale.GetYf(),ambientScale.GetYf(),ambientScale.GetYf());
						}
					}
					else
					{
						if( ( ambientScale.GetXf() < 0.0f || ambientScale.GetXf() > 1.0f ) ||
							( ambientScale.GetYf() < 0.0f || ambientScale.GetYf() > 1.0f ) )
						{
							col = Color32(0xff,0x00,0xff);
						}
						else
						{
							col = Color32(ambientScale.GetXf(),ambientScale.GetYf(),0.0f);
						}
					}
					
					
					grcDebugDraw::Sphere(pos,m_probeRadius,col);
					
				}
			}
		}
	}
	
}

///////////////////////////////////////////////////////////////////////////////
//  ReloadVisualSettings
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycleDebug::ReloadVisualSettings		()
{
	g_visualSettings.LoadAll();
}


///////////////////////////////////////////////////////////////////////////////
//  DumpSunMoonSettings
///////////////////////////////////////////////////////////////////////////////													
void			CTimeCycleDebug::DumpSunMoonPosition		()
{
	g_timeCycle.GetGameDebug().SetDumpSunMoonPosition(true);
}

///////////////////////////////////////////////////////////////////////////////
//  DisplayKeyframeGroup
///////////////////////////////////////////////////////////////////////////////

void			CTimeCycleDebug::DisplayKeyframeGroup		(const tcKeyframeUncompressed& keyframe, const s32 debugGroupId, const s32 x, s32 y, bool fullPrecision) const
{

#define DisplayColour(name, r, g, b, a)\
sprintf(msg, "%s : %5.2f %5.2f %5.2f %5.2f", name, r, g, b, a);\
grcDebugDraw::PrintToScreenCoors(msg, x, y);\
y++;

#define DisplayFloat(name, value)\
sprintf(msg, "%s : %5.2f", name, value);\
grcDebugDraw::PrintToScreenCoors(msg, x, y);\
y++;

#define DisplayColourFP(name, r, g, b, a)\
sprintf(msg, "%s : %f %f %f %f", name, r, g, b, a);\
grcDebugDraw::PrintToScreenCoors(msg, x, y);\
y++;

#define DisplayFloatFP(name, value)\
sprintf(msg, "%s : %f", name, value);\
grcDebugDraw::PrintToScreenCoors(msg, x, y);\
y++;

	char msg[1024];

	const char* debugGroupName = g_timeCycle.m_debug.GetDebugGroupName(debugGroupId);

	for (int i=0; i<TCVAR_NUM; i++)
	{
		const tcVarInfo& varInfo = tcConfig::GetVarInfo(i);

		tcVarType_e varType = varInfo.varType;
		if (varType!=VARTYPE_NONE && strcmp(varInfo.debugGroupName, debugGroupName)==0)
		{
			float value = keyframe.GetVar(i);
			const char* varName = varInfo.debugName;

			if (varType==VARTYPE_FLOAT)
			{
				if( fullPrecision )
				{
					DisplayFloatFP(varName, value);
				}
				else
				{
					DisplayFloat(varName, value);
				}
			}
			else if (varType==VARTYPE_COL3 || varType==VARTYPE_COL3_LIN)
			{
				float g = keyframe.GetVar(i+1);
				float b = keyframe.GetVar(i+2);
				if( fullPrecision )
				{
					DisplayColourFP(varName, value, g, b, 1.0f);
				}
				else
				{
					DisplayColour(varName, value, g, b, 1.0f);
				}
			}
			else if (varType==VARTYPE_COL4 || varType==VARTYPE_COL4_LIN)
			{
				float g = keyframe.GetVar(i+1);
				float b = keyframe.GetVar(i+2);
				float a = keyframe.GetVar(i+3);

				if( fullPrecision )
				{
					DisplayColourFP(varName, value, g, b, a);
				}
				else
				{
					DisplayColour(varName, value, g, b, a);
				}
			} 
		}
	}
}  


///////////////////////////////////////////////////////////////////////////////
//  InitLevelWidgets
///////////////////////////////////////////////////////////////////////////////													
PARAM(rag_timecycle,"rag_timecycle");

void			CTimeCycleDebug::InitLevelWidgets			()
{
	Assert(!m_pBank);

	m_pBank = BANKMGR.FindBank("TimeCycle");
	if (m_pBank)
	{
		if (PARAM_rag_timecycle.Get())
		{
			CreateLevelWidgets();
		}
		else
		{
			m_pCreateButton = m_pBank->AddButton("Create Timecycle Widgets", CreateLevelWidgets);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CreateLevelWidgets
///////////////////////////////////////////////////////////////////////////////													

void			CTimeCycleDebug::CreateLevelWidgets			()
{
	if (m_pBank==NULL )
	{
		return;
	}

	if( m_pCreateButton )
	{
		m_pCreateButton->Destroy();
		m_pCreateButton = NULL;
	}

	tcDebug::CreateWidgets();

	bkGroup* pGroup = g_timeCycle.m_debug.GetGameGroup();

	m_pBank->SetCurrentGroup(*pGroup);
	{	
		const char* debugGroupNames[TIMECYCLE_MAX_VARIABLE_GROUPS];
		for (int i=0; i<g_timeCycle.m_debug.GetDebugGroupCount(); i++)
		{
			debugGroupNames[i] = g_timeCycle.m_debug.GetDebugGroupName(i);
		}
		
		g_tcInstBoxMgr.InitWidgets(m_pBank);

		// debug
		m_pBank->PushGroup("Debug", false);
		{	
			m_pBank->AddToggle("Display Keyframe Data", &m_displayKeyframeData);
			m_pBank->AddToggle("Display Keyframe Data higher Precision", &m_displayKeyframeDataFullPrecision);
			m_pBank->AddSlider("Display Keyframe Data X",&m_displayKeyframeDataX,-256,256,1);
			m_pBank->AddSlider("Display Keyframe Data Y",&m_displayKeyframeDataY,-256,256,1);
			m_pBank->AddCombo("Display Group", &m_currentDisplayGroup, g_timeCycle.m_debug.GetDebugGroupCount(), debugGroupNames, NullCB);
			m_pBank->AddToggle("Display Modifier Info", &m_displayModifierInfo);
			m_pBank->AddToggle("Use Default Keyframe", &g_timeCycle.m_useDefaultKeyframe);
			m_pBank->AddToggle("Display VarMap Cache Use", &m_displayVarMapCacheUse);
			m_pBank->AddToggle("Display Modifier Box Use", &m_displayTCboxUse);

			m_pBank->AddSeparator("");

			// debug toggles
			m_pBank->AddToggle("Disable Cutscene Modifiers", &m_disableCutsceneModifiers);	
			m_pBank->AddToggle("Disable Box Modifiers", &m_disableBoxModifiers);	
			m_pBank->AddToggle("Disable Interior Modifiers", &m_disableInteriorModifiers);	
			m_pBank->AddToggle("Disable Secondary Interior Modifiers", &m_disableSecondaryInteriorModifiers);
			m_pBank->AddToggle("Disable Under Water Base Modifier", &m_disableBaseUnderWaterModifier);	
			m_pBank->AddToggle("Disable Under Water Modifiers", &m_disableUnderWaterModifiers);	
			m_pBank->AddToggle("Disable Under Water Cycle", &m_disableUnderWaterCycle);	
			m_pBank->AddToggle("Disable High Altitude Cycle", &m_disableHighAltitudeModifer);	

			m_pBank->AddSeparator("");

			m_pBank->AddToggle("Disable Global Modifiers", &m_disableGlobalModifiers);	
			m_pBank->AddToggle("Disable Modifier Fog", &m_disableModifierFog);			
			m_pBank->AddToggle("Disable Modifier Box Ambient Scale", &m_disableBoxModifierAmbientScale);
			m_pBank->AddToggle("Disable Modifier Interior Ambient Scale", &m_disableIntModifierAmbientScale);
			m_pBank->AddToggle("Disable Exterior Visible Modifier", &m_disableExteriorVisibleModifier);

			m_pBank->AddSeparator("");
			
			m_pBank->AddToggle("Disable fog and haze", &m_disableFogAndHaze);

			m_pBank->AddSeparator("");

#if GTA_REPLAY
			m_pBank->AddToggle("Disable Replay Two Phase System", &m_disableTwoPhaseSystem);
#endif // GTA_REPLAY
		}
		m_pBank->PopGroup();

		// sun position
		m_pBank->PushGroup("Sun", false);
		{
			m_pBank->AddToggle("Display Sun Course",&m_renderSunPosition);
			m_pBank->AddButton("Dump sun/moon info", datCallback(CFA1(DumpSunMoonPosition)));
			m_pBank->AddSlider("Divide hours in X parts",&m_hourDivider, 0, 60, 1);

			m_pBank->AddSlider("Sun Roll Angle",&m_sunRollDegreeAngle,0,360.0f,0.1f);
			m_pBank->AddSlider("Sun Yaw Angle",&m_sunYawDegreeAngle,0,360.0f,0.1f);
			m_pBank->AddSlider("Moon Roll Offset",&m_moonRollDegreeOffset,-180.0f,360.0f,0.1f);
			m_pBank->AddSlider("Moon Wobble Frequency",&m_moonWobbleFreq,0,360.0f,0.1f);
			m_pBank->AddSlider("Moon Wobble Amplitude",&m_moonWobbleAmp,0,360.0f,0.1f);
			m_pBank->AddSlider("Moon Wobble Offset",&m_moonWobbleOffset,0,360.0f,0.1f);
			m_pBank->AddSlider("Moon Cycle Override",&m_moonCycleOverride,-1.0f,1.0f,0.1f);
		}
		m_pBank->PopGroup();

		// Ambient Probes
		m_pBank->PushGroup("Ambient Scale Probes");
		{
			m_pBank->AddToggle("Enable",&m_probeEnabled);
			m_pBank->AddToggle("Show Natural Ambient Scale",&m_probeShowNaturalAmb);
			m_pBank->AddToggle("Show Artificial Ambient Scale",&m_probeShowArtificialAmb);
			m_pBank->AddToggle("Attach To Camera",&m_probeAttachToCamera);
			m_pBank->AddSlider("Offset From Camera",&m_probeCamOffset,0.0f,50.0f,0.1f);
			m_pBank->AddSlider("X Axis Probe Count",&m_probeXCount,1,15,1);
			m_pBank->AddSlider("Y Axis Probe Count",&m_probeYCount,1,15,1);
			m_pBank->AddSlider("Z Axis Probe Count",&m_probeZCount,1,15,1);
			m_pBank->AddSlider("X Axis Probe Offset",&m_probeXOffset,0.0f,50.0,0.1f);
			m_pBank->AddSlider("Y Axis Probe Offset",&m_probeYOffset,0.0f,50.0,0.1f);
			m_pBank->AddSlider("Z Axis Probe Offset",&m_probeZOffset,0.0f,50.0,0.1f);
			m_pBank->AddSlider("Probe Radius",&m_probeRadius,0.0f,20.0,0.1f);
		}
		m_pBank->PopGroup();

		// visual settings
		m_pBank->PushGroup("Visual Settings", false);
		{
			m_pBank->AddButton("Reload", datCallback(CFA1(ReloadVisualSettings)));
		}
		m_pBank->PopGroup();

 		// weather and time overrides
		m_pBank->PushGroup("Weather and Time Overrides", false);
 		{
			m_pBank->AddCombo("Override Prev Type", (s32*)&g_weather.GetOverridePrevTypeIndexRef(), g_weather.GetNumTypes()+1, g_weather.GetTypeNames(0));
			m_pBank->AddCombo("Override Next Type", (s32*)&g_weather.GetOverrideNextTypeIndexRef(), g_weather.GetNumTypes()+1, g_weather.GetTypeNames(0));
			m_pBank->AddSlider("Override Interp", &g_weather.GetOverrideInterpRef(), -0.01f, 1.0f, 0.01f);

 			m_pBank->AddToggle("Override Time", &CClock::GetTimeOverrideFlagRef());
			m_pBank->AddSlider("Curr Time",&CClock::GetTimeOverrideValRef(), 0, (24*60)-1, 1);
			m_pBank->AddSlider("Time Speed Mult", &CClock::GetTimeSpeedMultRef(), 0.0f, 50.0f, 1.0f);
 		}
 		m_pBank->PopGroup();

		m_pBank->PushGroup("Ambient Lighting", false);
		{
			m_pBank->AddButton("Update ambient scale values", DebugDeferred::RefreshAmbientValues);
			
			extern bool g_drawAmbientLightsDebug;
			m_pBank->AddToggle("Ambient lights debug draw",&g_drawAmbientLightsDebug);
		}
		m_pBank->PopGroup();
	}

	m_pBank->UnSetCurrentGroup(*g_timeCycle.m_debug.GetGameGroup());
}

///////////////////////////////////////////////////////////////////////////////
//  ShutdownLevelWidgets
///////////////////////////////////////////////////////////////////////////////													

void CTimeCycleDebug::ShutdownLevelWidgets()
{
	if (AssertVerify(m_pBank))
	{
		BANKMGR.DestroyBank(*m_pBank);
		m_pBank = NULL;
	}
}

#endif // TC_DEBUG

