// 
// vfx/clouds/Clouds.cpp
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#include "Clouds.h"

// framework
#include "vfx/vfxwidget.h"

// game
#include "Core/Game.h"
#include "timecycle/TimeCycle.h"
#include "Vfx/clouds/CloudHat.h"
#include "Vfx/clouds/CloudHatSettings.h"
#include "Vfx/clouds/CloudSettingsMap.h"
#include "Vfx/sky/Sky.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "renderer/rendertargets.h"

#include "game/weather.h"
#if !__PS3
	#include "renderer/Water.h"
#endif
#include "fwsys/timer.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//	Statics
///////////////////////////////////////////////////////////////////////////////
CloudFrame	CClouds::m_CurrentFrame[2];
int			CClouds::m_UpdateBufferIndex=0;
float		CClouds::m_CloudAlpha=1.0f;

extern bool g_UseKeyFrameCloudLight;

//
// Code
namespace
{
	//Global Keyframed Cloud Settings Map
	static CloudSettingsMap sCloudSettingsMap;
	static bool needCloudTransition = true;
	static bool needInstantTransition = true;
	static bool assumedMaxLengthTransition = false;
	static const char sCloudManagerSettingsFile[]	= "commoncrc:/data/clouds";
	static const char sCloudKeyframeSettingsFile[]	= "common:/data/CloudKeyframes";
	static rage::atHashString sForceOverrideSettingsName; // for script override. 
	
	bool IsSimulationPaused()
	{
		return fwTimer::IsGamePaused();
	}

	bool ShouldCloudsTransition()
	{
		bool shouldTransition = needCloudTransition;

		needCloudTransition = false;
			
		return shouldTransition;
	}

	bool ShouldCloudsTransitionInstantly()
	{
		// TODO:	If there's any time the clouds should transition instantly, add the cases here. (ie. If you are loading into game,
		//			don't want the clouds to transition in when the loading screen fades.)
	
		bool shouldInstantTransition = needInstantTransition;

		needCloudTransition = false;
				
		return shouldInstantTransition || sForceOverrideSettingsName.IsNotNull();
	}

	bool HasAssumedMaxLengthTransition()
	{
		bool hasAssumedMaxLengthTransition = assumedMaxLengthTransition;

		assumedMaxLengthTransition = false;

		return hasAssumedMaxLengthTransition;
	}

	CloudHatManager::size_type SelectNextSkyhat()
	{
		CloudHatManager::size_type nextCloud = 0;
		if(Verifyf(CLOUDHATMGR, "CloudHatManager appears to be uninitialized... how is this getting called?"))
		{
			atHashString prevSettingsName = g_weather.GetPrevType().m_CloudSettingsName; //TODO: this will need to change if we start transitions early.
			
			if (sForceOverrideSettingsName.IsNotNull())
				prevSettingsName = sForceOverrideSettingsName;

			CloudSettingsMap::DataType *data = sCloudSettingsMap.Access(prevSettingsName);
			if(data)
				nextCloud = data->GetCloudListController().SelectNewActiveCloudHat(); //Select new cloudhat from cloud list controller associated with the current weather type.
			else
				nextCloud = CLOUDHATMGR->GetNumCloudHats();	//No cloud settings for selected weather type? Then select no cloudhat as the transition target.
		}
		return nextCloud;
	}

	s32 FindDrawableIndexFromHash(const atHashString &name)
	{
		s32 drawableIndex = -1;
		CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromName(name,NULL);
		
		if(Verifyf(pModelInfo,"Can't find the CModelInfo for \"%s\". Does the level's .meta file include a .ityp entry for the clouds (usually 'v_clouds.ityp')?",name.TryGetCStr()))
		{
			if(Verifyf(pModelInfo->GetDrawableType() == fwArchetype::DT_DRAWABLE,"Cloud archetype \"%s\" drawable type is %d, should be %d (DT_DRAWABLE)", pModelInfo->GetModelName(), pModelInfo->GetDrawableType(), fwArchetype::DT_DRAWABLE))
				drawableIndex = pModelInfo->GetDrawableIndex();
		}

		return drawableIndex;
	}


	CloudFrame ComputeFrameSettings()
	{
		CloudFrame totalFrame;
		float tod = CloudHatSettings::GetTime24();

        //Get prev/next weather types and the current interpolation value
        const CWeatherType &prevWeatherType = g_weather.GetPrevType();
        const CWeatherType &nextWeatherType = g_weather.GetNextType();
        float weatherInterp = g_weather.GetInterp();

        //Get prev/next cloud frame values
        CloudFrame prevWeatherFrame, nextWeatherFrame;
        atHashString prevSettingsName = prevWeatherType.m_CloudSettingsName;
        atHashString nextSettingsName = nextWeatherType.m_CloudSettingsName;

        if (sForceOverrideSettingsName.IsNotNull())
        {
            prevSettingsName = sForceOverrideSettingsName;
            nextSettingsName = sForceOverrideSettingsName;	
        }

        CloudSettingsMap::DataType *prevData = sCloudSettingsMap.Access(prevSettingsName);
        CloudSettingsMap::DataType *nextData = sCloudSettingsMap.Access(nextSettingsName);

        if(prevData)
            prevData->GetCloudSettings(tod, prevWeatherFrame);

        if(nextData)
            nextData->GetCloudSettings(tod, nextWeatherFrame);

        //Accumulate cloud frame values
        totalFrame = Lerp(weatherInterp, prevWeatherFrame, nextWeatherFrame);

		// deal with override, lerp from where we were to the override 
		if (g_weather.GetOverTimeType() != 0xffffffff)
		{
			if (CloudSettingsMap::DataType *overrideData = sCloudSettingsMap.Access( g_weather.GetTypeInfo(g_weather.GetOverTimeType()).m_CloudSettingsName))
			{
				overrideData->GetCloudSettings(tod, nextWeatherFrame);
				totalFrame = Lerp(g_weather.GetOverTimeInterp(), totalFrame, nextWeatherFrame);
			}
		}

        //Net transition - we need to move from pre-transition snapshot to current (moving) target above
        if(g_weather.IsInNetTransition())
        {
            prevData = sCloudSettingsMap.Access(g_weather.GetNetTransitionPrevType().m_CloudSettingsName);
            nextData = sCloudSettingsMap.Access(g_weather.GetNetTransitionNextType().m_CloudSettingsName);

            if(prevData)
                prevData->GetCloudSettings(tod, prevWeatherFrame);

            if(nextData)
                nextData->GetCloudSettings(tod, nextWeatherFrame);

            CloudFrame prevFrame = Lerp(g_weather.GetNetTransitionPrevInterp(), prevWeatherFrame, nextWeatherFrame);
            totalFrame = Lerp(g_weather.GetNetTransitionInterp(), prevFrame, totalFrame);
        }

#if __BANK   // we may want to replace the frame with the override frame
		totalFrame = CloudHatManager::OverrideFrameSettings(totalFrame); 
#endif

		// adjust the lights by the moon phase, etc so we don't need to in the render thread.	
		// calc Moon Phase Adjustment (reduce scatter and diffuse light when the moon is small.
		// NOTE: we may want to use a gamma on this values to it does not reduce the light prematurely 
		float moonPhaseAdjust = (g_timeCycle.GetMoonFade()==0.0f) ? 1.0f : moonPhaseAdjust = g_sky.GetMoonPhaseIntensity();

		if (!g_UseKeyFrameCloudLight)
		{
			const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

			if(g_timeCycle.GetLightningOverrideUpdate())
			{
				totalFrame.m_LightColor = g_timeCycle.GetLightningColorUpdate().GetRGB();
			}
			else
			{
				totalFrame.m_LightColor = currKeyframe.GetVarV3(TCVAR_SKY_MOON_COL_R) * ScalarV(g_timeCycle.GetMoonFade()) + 
										  currKeyframe.GetVarV3(TCVAR_SKY_SUN_COL_R) * ScalarV(g_timeCycle.GetSunFade());

				totalFrame.m_LightColor *= totalFrame.m_LightColor; // square it so it matches the old sky hat colors	
//				if (g_timeCycle.GetMoonFade()> 0.0f && g_timeCycle.GetSunFade()> 0.0f)
//					Displayf("[CHD] LightColor seting using Moon fade = %f, sun fade = %f",g_timeCycle.GetMoonFade(),g_timeCycle.GetSunFade());
			}
		}

		totalFrame.m_LightColor *= ScalarV(moonPhaseAdjust);
		totalFrame.m_ScatteringScale *= moonPhaseAdjust;

		// get sun Direction too
		if(g_timeCycle.GetLightningOverrideUpdate())
		{
			totalFrame.m_SunDirection =  g_timeCycle.GetLightningDirectionUpdate();
		}
		else
		{
			if (g_timeCycle.GetSunFade()<=0.0f)
				totalFrame.m_SunDirection = g_timeCycle.GetMoonSpriteDirection();
			else
				totalFrame.m_SunDirection = g_timeCycle.GetSunSpriteDirection();
		}

		//Return the accumulated frame values
		return totalFrame;
	}

#if __BANK
	float sTODFromWidget = 0.0f;
	int	 sTODFromWidgetCombo = 0;
	bool sCopySettingsOnTODChange = true;
	bool sShadowTimeCycleTime = true;
	void Bank_ExecCopyOp()	{ CClouds::CopyDebugSettingsFromKeyframes(sTODFromWidget, sCloudSettingsMap.BANKONLY_GetCurrentlySelectedSet()); }
	void Bank_ExecPasteOp()	{ CClouds::PasteDebugSettingsInToKeyframes(sTODFromWidget, sCloudSettingsMap.BANKONLY_GetCurrentlySelectedSet()); }
	void Bank_OnTodChange()	{ if(sCopySettingsOnTODChange) Bank_ExecCopyOp();	}

	void Bank_OnTodComboChange()	{ sTODFromWidget = CClouds::GetTimeSampleTime(sTODFromWidgetCombo); if(sCopySettingsOnTODChange) Bank_ExecCopyOp();	}
#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	void Bank_CopyColors()			{ CClouds::CopyTimeCycleColorsToDebugSettings(sTODFromWidget);	}
#endif

	void Bank_UpdateTime(bool updateWidgets)
	{
		if (sShadowTimeCycleTime )
		{
			float newTime;
			int newCombo=0;

#if TC_DEBUG
			if (g_timeCycle.GetTCDebug().GetKeyframeEditActive() && g_timeCycle.GetTCDebug().GetKeyframeEditOverrideTimeSample())
				newTime = tcConfig::GetTimeSampleInfo(g_timeCycle.GetTCDebug().GetKeyframeEditTimeSampleIndex()).hour;
			else
#endif //  TC_DEBUG
			{
				newTime = CClock::GetHour() + CClock::GetMinute()/60.0f;
			}
			
			for (int i=0; i<CClouds::GetNumTimeSamples(); i++)
			{
				if (CClouds::GetTimeSampleTime(i) <= newTime)
					newCombo = i;
			}

			if (newTime != sTODFromWidget || newCombo != sTODFromWidgetCombo)
			{
				sTODFromWidgetCombo = newCombo;
				sTODFromWidget = newTime;
				if (updateWidgets)
					Bank_OnTodChange();
			}
		}
	}

	void OnWidgetsChanged(bkBank & /*bank*/)
	{
		//Add some useful widgets
		if(bkGroup *grp = CLOUDHATMGR->GetDebugGroup())
		{
#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
			grp->AddButton("Copy sky colors from TimeCycle data", &Bank_CopyColors);
#endif

//			grp->AddTitle("");
			grp->AddSeparator();

//			grp->AddTitle("Copy/Paste Settings from Selected Keyframe Set.");
			grp->AddToggle("Sync to TimeCycle Time", &sShadowTimeCycleTime);
			grp->AddToggle("Auto Copy Settings when TOD (below) changes", &sCopySettingsOnTODChange);
//			grp->AddSlider("TOD to copy/paste keyframe settings:", &sTODFromWidget, 0.0f, 24.0f, 0.1f, &Bank_OnTodChange);
				
			int timeSampleCount = CClouds::GetNumTimeSamples();

			if (timeSampleCount==0)
				Errorf("CClouds::GetNumTimeSamples() returned 0, did you create the Cloud Widgets before the game finished loading?");

			static char * * timeNames = NULL;
			if (timeNames==NULL)
			{
				timeNames = rage_new char*[timeSampleCount];

				for (int i=0;i<timeSampleCount; i++)
				{
					timeNames[i] = rage_new char[6];
					float time = CClouds::GetTimeSampleTime(i);
					formatf(timeNames[i], 6, "%02d:%02d", int(time), int((time-int(time))*60));
				}
			}

			Bank_UpdateTime(false);
			
			if (timeSampleCount>0)
				grp->AddCombo("TOD to copy/paste keyframe settings:", &sTODFromWidgetCombo, timeSampleCount, (const char **)timeNames, 0, &Bank_OnTodComboChange);
			
			grp->AddButton("Copy Settings from Current Keyframes", &Bank_ExecCopyOp);
			grp->AddButton("Paste Settings into Current Keyframes", &Bank_ExecPasteOp);
			Bank_OnTodChange();
		}
	}
#endif
}

int  CClouds::GetNumTimeSamples()
{
	return sCloudSettingsMap.GetKeyFrameTimesArray().GetCount();
}

float CClouds::GetTimeSampleTime(int index)
{
	return sCloudSettingsMap.GetKeyFrameTimesArray()[index];
}

void CClouds::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		CloudHatManager::Create();
		if(Verifyf(CLOUDHATMGR, "Failed to create CloudHat Manager!"))
		{
			//Load Settings
			CLOUDHATMGR->Load(sCloudManagerSettingsFile);

			//Setup Cloud Functors
			CloudHatManager::NullaryPredicate_type				IsSimulationPausedFunc;
			CloudHatManager::NullaryPredicate_type				TransitionFunc;
			CloudHatManager::NullaryPredicate_type				InstantTransitionFunc;
			CloudHatManager::NullaryPredicate_type				AssumedMaxLengthTransitionFunc;
			CloudHatManager::NullaryFunctor_type				SelectCloudFunc;
			CloudHatManager::StreamingNameToSlotFunctor_type	Name2SlotFunc;
			IsSimulationPausedFunc.Reset<&IsSimulationPaused>();
			TransitionFunc.Reset<&ShouldCloudsTransition>();
			AssumedMaxLengthTransitionFunc.Reset<&HasAssumedMaxLengthTransition>();
			InstantTransitionFunc.Reset<&ShouldCloudsTransitionInstantly>();
			SelectCloudFunc.Reset<&SelectNextSkyhat>();
			Name2SlotFunc.Reset<&FindDrawableIndexFromHash>();

			CLOUDHATMGR->SetSimulationPausedPredicate(IsSimulationPausedFunc).SetTransitionPredicate(TransitionFunc).SetInstantTransitionPredicate(InstantTransitionFunc);
			CLOUDHATMGR->SetNextCloudHatFunctor(SelectCloudFunc).SetStreamingNameToSlotFunctor(Name2SlotFunc);
			CLOUDHATMGR->SetAssumedMaxLengthTransitionPredicate(AssumedMaxLengthTransitionFunc);
		}

		//Next, load the cloud TOD keyframed settings.
		sCloudSettingsMap.Load(sCloudKeyframeSettingsFile);
	}
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		sCloudSettingsMap.PostMapLoad();
	}

	m_CloudAlpha = 1.0f;

}

void CClouds::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		CloudHatManager::Destroy();
	}
}

void CClouds::OverrideCloudSettingName(const char * settingName)
{
	sForceOverrideSettingsName.SetFromString(settingName);
	needCloudTransition = true;			// force a transition
}

void CClouds::RequestCloudTransition(bool instant, bool assumedMaxLength)
{
	needCloudTransition = true;
	needInstantTransition = instant;
	assumedMaxLengthTransition = assumedMaxLength;
}

void CClouds::Update()
{
	//Note: Ideally, this would run when bkManager::Update runs, but I was told that it should be safe to add/remove widgets during the update thread.
	//If this causes any issues, it's perfectly safe to comment out this line. This just helps make it easier to tune/debug cloud settings. -AJG
#if __BANK
	sCloudSettingsMap.CheckForWeatherChange();
	Bank_UpdateTime(true);
	UpdateWidgets();
#endif

	if(CLOUDHATMGR)
		CLOUDHATMGR->Update();
	
	if (!CPauseMenu::GetPauseRenderPhasesStatus())
	{
		m_CurrentFrame[m_UpdateBufferIndex]=ComputeFrameSettings();
	}
	else
	{
		m_CurrentFrame[m_UpdateBufferIndex] = m_CurrentFrame[1 - m_UpdateBufferIndex];
	}
	
}

void CClouds::Synchronise()
{
	m_UpdateBufferIndex ^= 1;
	CLOUDHATMGR->ResetCloudLights();
#if __BANK
	CLOUDHATMGR->UpdateTextures(); // in case they reloaded with widgets
#endif
}

void CClouds::Render(CVisualEffects::eRenderMode mode, int scissorX, int scissorY, int scissorW, int scissorH, float cloudAlpha)
{
	bool bIsUnderWater =  Water::IsCameraUnderwater();
	grcViewport *viewPort = grcViewport::GetCurrent();
	const bool reflectionPhaseMode = (mode != CVisualEffects::RM_DEFAULT);

	if(CLOUDHATMGR && viewPort && !(bIsUnderWater && reflectionPhaseMode)) // skip drawing clouds in underwater reflections
	{
		Mat34V oldCamMtx = viewPort->GetCameraMtx();
		CLOUDHATMGR->SetCameraPos(oldCamMtx.GetCol3());
		
		// adjust the camera position to origin (we'll move cloud there to) to improve resolution
		Mat34V newCamMtx = oldCamMtx;
		newCamMtx.SetCol3(Vec3V(V_ZERO));
		viewPort->SetCameraMtx(newCamMtx);

		// make sure there are no leftover world matrix transforms in the viewport, we need to save a clean copy for the shaders
		viewPort->SetWorldIdentity();

#if !__PS3 
		bool needWaterClipPlane = bIsUnderWater;
		if (needWaterClipPlane)
		{
			// add a clipping plan at the water level, so we don't see clouds underwater.
			Assertf(GRCDEVICE.GetClipPlaneEnable()==0x00, "ClipPlaneEnable already set (0x%02x)", GRCDEVICE.GetClipPlaneEnable()); 
			GRCDEVICE.SetClipPlane(0, Vec4V(Vec3V(V_Z_AXIS_WZERO),ScalarV(-Water::GetWaterLevel())));
			GRCDEVICE.SetClipPlaneEnable(0x01);
		}
#endif

		if (scissorX > -1)
		{
			GRCDEVICE.SetScissor(scissorX, scissorY, scissorW, scissorH);
		}

		CLOUDHATMGR->Draw(m_CurrentFrame[1^m_UpdateBufferIndex], reflectionPhaseMode, cloudAlpha);

		// restore the camera matrix
		viewPort->SetCameraMtx(oldCamMtx);

#if !__PS3 
		if (needWaterClipPlane)
			GRCDEVICE.SetClipPlaneEnable(0x00);
#endif
	}
}

#if __BANK
void CClouds::InitWidgets()
{
	//Setup Cloud Bank Functors
// 	CLOUDHATMGR->GetWidgetsChangedCbFunctor().Reset<&OnWidgetsChanged>();

	bkBank *pVfxBank = vfxWidget::GetBank();

	if(CLOUDHATMGR)
		CLOUDHATMGR->AddWidgets(*pVfxBank);

	//Add cloud keyframe settings map
	pVfxBank->PushGroup("Cloud Keyframed Settings");
	{
		sCloudSettingsMap.AddWidgets(*pVfxBank);
		OnWidgetsChanged(*pVfxBank);
	}
	pVfxBank->PopGroup();
}

void CClouds::UpdateWidgets()
{
	if(CLOUDHATMGR)
		CLOUDHATMGR->UpdateWidgets();
}

void CClouds::CopyDebugSettingsFromKeyframes(float time, const atHashString &name)
{
	if(CloudSettingsMap::DataType *keyframes = sCloudSettingsMap.Access(name))
	{
		CloudFrame fromKeyframes;
		keyframes->GetCloudSettings(time, fromKeyframes);
		CloudHatManager::SetCloudOverrideSettings(fromKeyframes);
	}
}

void CClouds::PasteDebugSettingsInToKeyframes(float time, const atHashString &name)
{
	if(CloudSettingsMap::DataType *keyframes = sCloudSettingsMap.Access(name))
	{
		keyframes->SetCloudSettings(time, CloudHatManager::GetCloudOverrideSettings());
		
		if (time==0.0f)
			keyframes->SetCloudSettings(24.0, CloudHatManager::GetCloudOverrideSettings());

		keyframes->UpdateUI();
	}
}

void CClouds::CopyTimeCycleColorsToDebugSettings(float time)
{
	CloudHatManager::GetCloudOverrideSettings().CopyTimeCycleColors(time);
}
#endif
