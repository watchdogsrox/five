// 
// vfx/clouds/CloudHatSettings.cpp 
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
// 

#include "CloudHatSettings.h"
#include "CloudHatSettings_parser.h"
#include "Clouds.h"

#include "timecycle/tckeyframe.h"
#include "timecycle/TimeCycle.h"
#include "game/Clock.h"

#include "bank/bank.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()

extern bool g_UseKeyFrameCloudLight;

namespace rage {

//------------------------------------------------------------------------------
CloudFrame::CloudFrame()
: m_PiercingLightPower_Strength_NormalStrength_Thickness(0.0f, 0.0f, 0.0f, 0.0f)
, m_ScaleDiffuseFillAmbient(0.0f, 0.0f, 0.0f)
, m_ScaleSkyBounceEastWest(0.0f,0.0f,0.0f,0.0f)
, m_CloudColor(0.0f, 0.0f, 0.0f)
, m_LightColor(0.0f, 0.0f, 0.0f)
, m_AmbientColor(0.0f, 0.0f, 0.0f)
, m_SkyColor(0.0f, 0.0f, 0.0f)
, m_BounceColor(0.0f, 0.0f, 0.0f)
, m_EastColor(0.0f, 0.0f, 0.0f)
, m_WestColor(0.0f, 0.0f, 0.0f)
, m_DensityShift_Scale(0.0f, 0.0f)
, m_ScatteringConst(0.0f)
, m_ScatteringScale(0.0f)
, m_WrapAmount(0.0f)
, m_MSAAErrorRef(1.0f)
{
}

//------------------------------------------------------------------------------
CloudFrame& CloudFrame::operator*=(const float mag)
{
	*this = *this * mag;
	return *this;
}

//------------------------------------------------------------------------------
CloudFrame& CloudFrame::operator+=(const CloudFrame& other)
{
	*this = *this + other;
	return *this;
}

//------------------------------------------------------------------------------
CloudFrame operator+(const CloudFrame& a, const CloudFrame& b)
{
	CloudFrame result;
	result.m_CloudColor = a.m_CloudColor + b.m_CloudColor;
	result.m_LightColor = a.m_LightColor + b.m_LightColor;
	result.m_AmbientColor = a.m_AmbientColor + b.m_AmbientColor;
	result.m_SkyColor = a.m_SkyColor + b.m_SkyColor;
	result.m_BounceColor = a.m_BounceColor + b.m_BounceColor;
	result.m_EastColor = a.m_EastColor + b.m_EastColor;
	result.m_WestColor = a.m_WestColor + b.m_WestColor;
	result.m_DensityShift_Scale = a.m_DensityShift_Scale + b.m_DensityShift_Scale;
	result.m_ScatteringConst = a.m_ScatteringConst + b.m_ScatteringConst;
	result.m_ScatteringScale = a.m_ScatteringScale + b.m_ScatteringScale;
	result.m_PiercingLightPower_Strength_NormalStrength_Thickness = a.m_PiercingLightPower_Strength_NormalStrength_Thickness + b.m_PiercingLightPower_Strength_NormalStrength_Thickness;
	result.m_ScaleDiffuseFillAmbient = a.m_ScaleDiffuseFillAmbient + b.m_ScaleDiffuseFillAmbient;
	result.m_ScaleSkyBounceEastWest = a.m_ScaleSkyBounceEastWest + b.m_ScaleSkyBounceEastWest;
	result.m_WrapAmount = a.m_WrapAmount + b.m_WrapAmount;
	return result;
}

//------------------------------------------------------------------------------
CloudFrame operator-(const CloudFrame& a, const CloudFrame& b)
{
	return (a + (b * -1.0f));
}

//------------------------------------------------------------------------------
CloudFrame operator*(const CloudFrame& a, const float mag)
{
	CloudFrame result;
	ScalarV magScalar(mag);
	result.m_CloudColor = a.m_CloudColor * magScalar;
	result.m_LightColor = a.m_LightColor * magScalar;
	result.m_AmbientColor = a.m_AmbientColor * magScalar;
	result.m_SkyColor = a.m_SkyColor * magScalar;
	result.m_BounceColor = a.m_BounceColor * magScalar;
	result.m_EastColor = a.m_EastColor * magScalar;
	result.m_WestColor = a.m_WestColor * magScalar;
	result.m_DensityShift_Scale = a.m_DensityShift_Scale * magScalar;
	result.m_ScatteringConst = a.m_ScatteringConst * mag;
	result.m_ScatteringScale = a.m_ScatteringScale * mag;
	result.m_PiercingLightPower_Strength_NormalStrength_Thickness = a.m_PiercingLightPower_Strength_NormalStrength_Thickness * magScalar;
	result.m_ScaleDiffuseFillAmbient = a.m_ScaleDiffuseFillAmbient * magScalar;
	result.m_ScaleSkyBounceEastWest = a.m_ScaleSkyBounceEastWest * magScalar;
	result.m_WrapAmount = a.m_WrapAmount * mag;
	return result;
}

//------------------------------------------------------------------------------
CloudFrame operator*(const float mag, const CloudFrame& b)
{
	return (b * mag);
}

#if __BANK
void CloudFrame::AddWidgets(bkBank &bank)
{
	static const float sMin = 0.0f;
	static const float sMax = 50.0f;
	static const float sDelta = 0.1f;

//	bank.AddVector("Piercing Light Power/Strength Normal Strength/Thickness:", &m_PiercingLightPower_Strength_NormalStrength_Thickness,	sMin, sMax, sDelta);
//	bank.AddVector("Scale [Diffuse/Fill/Ambient]:", &m_ScaleDiffuseFillAmbient, sMin, sMax, sDelta);
	bank.AddSlider("Piercing Light Power:", &(m_PiercingLightPower_Strength_NormalStrength_Thickness[0]),	sMin, sMax, sDelta);
	bank.AddSlider("Strength", &(m_PiercingLightPower_Strength_NormalStrength_Thickness[1]),	sMin, sMax, sDelta);
	bank.AddSlider("Normal Strength", &(m_PiercingLightPower_Strength_NormalStrength_Thickness[2]),	sMin, sMax, sDelta);
	bank.AddSlider("Thickness:", &(m_PiercingLightPower_Strength_NormalStrength_Thickness[3]),	sMin, sMax, sDelta);
	bank.AddSlider("Scale Diffuse:", &(m_ScaleDiffuseFillAmbient[0]), sMin, sMax, sDelta);
	bank.AddSlider("Scale Fill:", &(m_ScaleDiffuseFillAmbient[1]), sMin, sMax, sDelta);
	bank.AddSlider("Scale Ambient:", &(m_ScaleDiffuseFillAmbient[2]), sMin, sMax, sDelta);
	bank.AddSlider("Scale Bounce:", &(m_ScaleSkyBounceEastWest[1]), sMin, sMax, sDelta);
	bank.AddSlider("Scale Sky Color:", &(m_ScaleSkyBounceEastWest[0]), sMin, sMax, sDelta);
	bank.AddSlider("Scale East Color:", &(m_ScaleSkyBounceEastWest[2]), sMin, sMax, sDelta);
	bank.AddSlider("Scale West Color:", &(m_ScaleSkyBounceEastWest[3]), sMin, sMax, sDelta);
	bank.AddVector("Density Shift/Scale:", &m_DensityShift_Scale, -sMax, sMax, sDelta);
	bank.AddSlider("Scattering Constant:", &m_ScatteringConst, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("Scattering Scale:", &m_ScatteringScale, sMin, sMax, sDelta);
	bank.AddSlider("Wrap Amount:", &m_WrapAmount, 0.0f, 1.0f, 0.1f);
	bank.AddColor("Light Color:", &m_LightColor);
	bank.AddColor("Cloud Color:", &m_CloudColor);
	bank.AddColor("Ambient Color:", &m_AmbientColor);
	bank.AddColor("Bounce Color:", &m_BounceColor);

#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	bank.AddColor("Sky Color:", &m_SkyColor);	// we're just going to use the sky's colors now...
	bank.AddColor("East Color:", &m_EastColor);
	bank.AddColor("West Color:", &m_WestColor);
#endif // !LOCK_CLOUD_COLORS_TO_TIMECYCLE
//	bank.AddSlider("MSAA Error Ref:", &m_MSAAErrorRef, 0.0f, 1.0f, 0.1f);
}

void CloudFrame::CopyTimeCycleColors(float UNUSED_PARAM(time))
{
#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
	m_SkyColor	= Vec3V(currKeyframe.GetVar(TCVAR_SKY_ZENITH_COL_R),	  currKeyframe.GetVar(TCVAR_SKY_ZENITH_COL_G),		currKeyframe.GetVar(TCVAR_SKY_ZENITH_COL_B));
	m_EastColor = Vec3V(currKeyframe.GetVar(TCVAR_SKY_AZIMUTH_EAST_COL_R),currKeyframe.GetVar(TCVAR_SKY_AZIMUTH_EAST_COL_G),currKeyframe.GetVar(TCVAR_SKY_AZIMUTH_EAST_COL_B));
	m_WestColor = Vec3V(currKeyframe.GetVar(TCVAR_SKY_AZIMUTH_WEST_COL_R),currKeyframe.GetVar(TCVAR_SKY_AZIMUTH_WEST_COL_G),currKeyframe.GetVar(TCVAR_SKY_AZIMUTH_WEST_COL_B));
#endif // !LOCK_CLOUD_COLORS_TO_TIMECYCLE
}

#endif //__BANK


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
CloudHatSettings::CloudHatSettings()
{
}

void CloudHatSettings::GetCloudSettings(float time, CloudFrame &settings) const
{
	Vec4V vKeyData;
	ScalarV vTime = ScalarVFromF32(time);

	vKeyData = m_CloudColor.Query(vTime);
	settings.m_CloudColor = vKeyData.GetXYZ();
	
	vKeyData = m_CloudLightColor.Query(vTime);
	settings.m_LightColor = vKeyData.GetXYZ();

	vKeyData = m_CloudAmbientColor.Query(vTime);
	settings.m_AmbientColor = vKeyData.GetXYZ();

	vKeyData = m_CloudBounceColor.Query(vTime);
	settings.m_BounceColor = vKeyData.GetXYZ();

#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	vKeyData = m_CloudSkyColor.Query(vTime);
	settings.m_SkyColor = vKeyData.GetXYZ();

	vKeyData = m_CloudEastColor.Query(vTime);
	settings.m_EastColor = vKeyData.GetXYZ();

	vKeyData = m_CloudWestColor.Query(vTime);
	settings.m_WestColor = vKeyData.GetXYZ();
#endif
	settings.m_ScaleSkyBounceEastWest = m_CloudScaleFillColors.Query(vTime);

	vKeyData = m_CloudDensityShift_Scale_ScatteringConst_Scale.Query(vTime);
	settings.m_DensityShift_Scale = vKeyData.GetXY();

	//Art requested to rescale the scattering scale parameter for greater precision when using Rag's widgets.
	static const float sRescaleScatteringScale = 0.2f; //Rescale (0.0f, 1.0f) range in Rag to (0.0f, 0.2f) range for scatter scale.
	settings.m_ScatteringConst = vKeyData.GetZf();
	settings.m_ScatteringScale = vKeyData.GetWf() * sRescaleScatteringScale;

	settings.m_PiercingLightPower_Strength_NormalStrength_Thickness = m_CloudPiercingLightPower_Strength_NormalStrength_Thickness.Query(vTime);

	vKeyData = m_CloudScaleDiffuseFillAmbient_WrapAmount.Query(vTime);
	settings.m_ScaleDiffuseFillAmbient = vKeyData.GetXYZ();
	settings.m_WrapAmount = vKeyData.GetWf();

	//Until I add a new keyframe or whatever to get MSAA error ref, set it to 1 as a placeholder.
	settings.m_MSAAErrorRef = 1.0f;
}

void CloudHatSettings::SetCloudSettings(float time, const CloudFrame &settings)
{
	Vec4V vKeyData(V_ONE);
	ScalarV vTime = ScalarVFromF32(time);

	vKeyData.SetXYZ(settings.m_CloudColor);
	m_CloudColor.AddEntry(vTime, vKeyData);
	
	vKeyData.SetXYZ(settings.m_LightColor);
	m_CloudLightColor.AddEntry(vTime, vKeyData);

	vKeyData.SetXYZ(settings.m_AmbientColor);
	m_CloudAmbientColor.AddEntry(vTime, vKeyData);

#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	vKeyData.SetXYZ(settings.m_SkyColor);
	m_CloudSkyColor.AddEntry(vTime, vKeyData);

	vKeyData.SetXYZ(settings.m_EastColor);
	m_CloudEastColor.AddEntry(vTime, vKeyData);

	vKeyData.SetXYZ(settings.m_WestColor);
	m_CloudWestColor.AddEntry(vTime, vKeyData);
#endif

	vKeyData.SetXYZ(settings.m_BounceColor);
	m_CloudBounceColor.AddEntry(vTime, vKeyData);

	m_CloudScaleFillColors.AddEntry(vTime, settings.m_ScaleSkyBounceEastWest);

	//The next keyframe was split up by code and the scatter scale value was re-scaled. So we need to do the opposite here and compose
	//the full vec4 to set to the keyframe.
	static const float sRescaleScatteringScale = (1.0f / 0.2f); //When computing cloud frame, scatter scale is rescaled using 0.2f. We do the opposite here.
	vKeyData = Vec4V(settings.m_DensityShift_Scale, Vec2V(settings.m_ScatteringConst, settings.m_ScatteringScale * sRescaleScatteringScale));
	m_CloudDensityShift_Scale_ScatteringConst_Scale.AddEntry(vTime, vKeyData);

	//Does operator =() work??
	m_CloudPiercingLightPower_Strength_NormalStrength_Thickness.AddEntry(vTime, settings.m_PiercingLightPower_Strength_NormalStrength_Thickness);

	vKeyData.SetXYZ(settings.m_ScaleDiffuseFillAmbient);
	vKeyData.SetWf(settings.m_WrapAmount);
	m_CloudScaleDiffuseFillAmbient_WrapAmount.AddEntry(vTime, vKeyData);

	//MSAA error ref not in keyframes
	//settings.m_MSAAErrorRef;
}

bool CloudHatSettings::Load(const char *filename)
{
	bool success = PARSER.LoadObject(filename, "xml", *this);

#if __BANK
	if(success)
		UpdateUI();
#endif //__BANK

	return success;
}

float CloudHatSettings::GetTime24()
{
	//Get the current time
	u32 hours = CClock::GetHour();
	u32 minutes = CClock::GetMinute();
	u32 seconds = CClock::GetSecond();

	// adjust to funnky time strecthed time to match the timecycle interpolation (see CTimeCycle::CalcBaseKeyframe())
	u32 timeSampleIndex1;
	u32 timeSampleIndex2;
	float timeInterp;
	
#if TC_DEBUG
	if (g_timeCycle.GetTCDebug().GetKeyframeEditActive() && g_timeCycle.GetTCDebug().GetKeyframeEditOverrideTimeSample())
	{
		timeSampleIndex1 = g_timeCycle.GetTCDebug().GetKeyframeEditTimeSampleIndex();
		timeSampleIndex2 = timeSampleIndex1;
		timeInterp = 0.0f;
	}
	else
#endif
	{
		g_timeCycle.CalcTimeSampleInfo(hours, minutes, seconds, timeSampleIndex1, timeSampleIndex2, timeInterp);
	}

	float startHour = tcConfig::GetTimeSampleInfo(timeSampleIndex1).hour;
	float endHour = (timeSampleIndex1>0 && timeSampleIndex2==0) ? 24 : tcConfig::GetTimeSampleInfo(timeSampleIndex2).hour;
	float time = Lerp(timeInterp, startHour, endHour);
	return time;
}

void CloudHatSettings::PostLoad()
{
	SyncToTimeCycleTimes();
}
void CloudHatSettings::PreSave()
{
	SyncToTimeCycleTimes();
}

// clean up the keyframes so they match the timecycle times (we should then lock the times so they cannot change via widgets)
void CloudHatSettings::SyncTCHours(ptxKeyframe & keyFrame)
{
	// make sure there is at least one keyframe  (to make sure keyFrame.Query() works)
	if (keyFrame.GetNumEntries()==0 || (keyFrame.GetNumEntries()==1 && IsEqualAll(keyFrame.GetTimeV(0),ScalarV(V_ZERO)) && IsEqualAll(keyFrame.GetValue(0),Vec4V(V_ZERO))) )
		keyFrame.AddEntry(ScalarV(V_ZERO), Vec4V(V_ONE)); // make sure there is at least one entry

	// make a new key frame, extract the values at the time cycle times from the first, then reset the first with these new values
	ptxKeyframe temp;

	for (int i=0; i<CClouds::GetNumTimeSamples(); i++)
	{
		ScalarV time = ScalarV(CClouds::GetTimeSampleTime(i));
		temp.AddEntry(time, keyFrame.Query(time));
	}
	
	keyFrame.CopyEntriesFrom(temp);

	keyFrame.AddEntry(ScalarV(24.0f), keyFrame.Query(ScalarV(V_ZERO)));
}

void CloudHatSettings::SyncToTimeCycleTimes()
{
	// set key frame 24 to the same value as 0 so the are not midnight transition problems
	SyncTCHours(m_CloudColor);
	SyncTCHours(m_CloudLightColor);
	SyncTCHours(m_CloudAmbientColor);
	SyncTCHours(m_CloudBounceColor);

#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	SyncTCHours(m_CloudSkyColor);
	SyncTCHours(m_CloudEastColor);
	SyncTCHours(m_CloudWestColor);
#endif
	SyncTCHours(m_CloudScaleFillColors);
	SyncTCHours(m_CloudDensityShift_Scale_ScatteringConst_Scale);
	SyncTCHours(m_CloudPiercingLightPower_Strength_NormalStrength_Thickness);
	SyncTCHours(m_CloudScaleDiffuseFillAmbient_WrapAmount);
}


#if __BANK
void CloudHatSettings::SetLightColorFromTimeCycle()
{
	float time = GetTime24();

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

	// old color calc from CloudHatFragContainer::SetCloudSettings()
	Vec3V color = currKeyframe.GetVarV3(TCVAR_SKY_MOON_COL_R) * ScalarV(g_timeCycle.GetMoonFade()) + 
		currKeyframe.GetVarV3(TCVAR_SKY_SUN_COL_R) * ScalarV(g_timeCycle.GetSunFade());
	color *= color; 	

	m_CloudLightColor.AddEntry(ScalarV(time),Vec4V(color,ScalarV(V_ONE)));
	m_CloudLightColor.UpdateWidget();
}

bool CloudHatSettings::Save(const char *filename)
{

#if SYNC_TO_TIMECYCLE_TIMES
	SyncToTimeCycleTimes();  // make sure it's all clean when we save.
#endif
	
	bool success = PARSER.SaveObject(filename, "xml", this);

	return success;
}

void CloudHatSettings::AddWidgets(bkBank &bank)
{	
	static const float sMin = 0.0f;
	static const float sMax = 24.0f;
	static const float sDelta = 1.0f;

	Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
	Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);
	Vec4V vDefaultValue = Vec4V(V_ZERO);

	//Color widgets
	static ptxKeyframeDefn s_WidgetCloudColor("Cloud Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);
	static ptxKeyframeDefn s_WidgetLightColor("Light Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);
	static ptxKeyframeDefn s_WidgetCloudAmbientColor("Ambient Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);
	static ptxKeyframeDefn s_WidgetCloudBounceColor("Bounce Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);

#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE 
	static ptxKeyframeDefn s_WidgetCloudSkyColor("Sky Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);
	static ptxKeyframeDefn s_WidgetCloudEastColor("East Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);
	static ptxKeyframeDefn s_WidgetCloudWestColor("West Color", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Red", "Green", "Blue", NULL, true);
#endif

	//Curve Widgets
	static ptxKeyframeDefn s_WidgetCloudScaleFillColors("Fill Color Scales", 0, PTXKEYFRAMETYPE_FLOAT4, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Sky Color Scale", "Bounce Color Scale", "East Color Scale", "West Color Scale", false);
	static ptxKeyframeDefn s_WidgetCloudDensityShift_Scale_ScatteringConst_Scale("Density / Scattering", 0, PTXKEYFRAMETYPE_FLOAT4, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Density Shift", "Density Scale", "Scattering Constant", "Scattering Scale", false);
	static ptxKeyframeDefn s_WidgetCloudPiercingLightPower_Strength_NormalStrength_Thickness("Piercing", 0, PTXKEYFRAMETYPE_FLOAT4, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Piercing Light Power", "Piercing Light Strength", "Piercing Normal Strength", "Piercing Normal Thickness", false);
	static ptxKeyframeDefn s_WidgetCloudScaleDiffuseFillAmbient_WrapAmount("Lighting Scales", 0, PTXKEYFRAMETYPE_FLOAT4, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Diffuse Scale", "Fill Scale", "Ambient Scale", "Wrap Amount", false);

	//CloudHat Settings
	bank.PushGroup("CloudHat Settings", false,"Full keyframe graph editing of the CloudHat settings");
	bank.AddButton("Sync KeyFrames to TimeCycle Hours",datCallback(MFA(CloudHatSettings::SyncToTimeCycleTimes), (datBase*)this));
	bank.AddToggle("Use KeyFramed Cloud Light Color", &g_UseKeyFrameCloudLight );

	m_CloudList.AddWidgets(bank);

	bank.AddTitle("Cloud Color:");
	m_CloudColor.SetDefn(&s_WidgetCloudColor);
	m_CloudColor.AddWidgets(bank);

	bank.AddTitle("Light Color:");
	bank.AddButton("Set from TimeCycle",datCallback(MFA(CloudHatSettings::SetLightColorFromTimeCycle), (datBase*)this));
	m_CloudLightColor.SetDefn(&s_WidgetLightColor);
	m_CloudLightColor.AddWidgets(bank);

	bank.AddTitle("Ambient Color:");
	m_CloudAmbientColor.SetDefn(&s_WidgetCloudAmbientColor);
	m_CloudAmbientColor.AddWidgets(bank);

	bank.AddTitle("Bounce Fill Color:");
	m_CloudBounceColor.SetDefn(&s_WidgetCloudBounceColor);
	m_CloudBounceColor.AddWidgets(bank);

#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	bank.AddTitle("Sky Fill Color:");
	m_CloudSkyColor.SetDefn(&s_WidgetCloudSkyColor);
	m_CloudSkyColor.AddWidgets(bank);

	bank.AddTitle("East Fill Color:");
	m_CloudEastColor.SetDefn(&s_WidgetCloudEastColor);
	m_CloudEastColor.AddWidgets(bank);

	bank.AddTitle("West Fill Color:");
	m_CloudWestColor.SetDefn(&s_WidgetCloudWestColor);
	m_CloudWestColor.AddWidgets(bank);
#endif

	bank.AddTitle("Scales for Fill Top, Bounce, East and West Colors:");
	m_CloudScaleFillColors.SetDefn(&s_WidgetCloudScaleFillColors);
	m_CloudScaleFillColors.AddWidgets(bank);

	bank.AddTitle("Density Shift + Scale / Scattering Constant + Scale:");
	m_CloudDensityShift_Scale_ScatteringConst_Scale.SetDefn(&s_WidgetCloudDensityShift_Scale_ScatteringConst_Scale);
	m_CloudDensityShift_Scale_ScatteringConst_Scale.AddWidgets(bank);

	bank.AddTitle("Piercing Light Power + Strength / Normal Strength + Thickness:");
	m_CloudPiercingLightPower_Strength_NormalStrength_Thickness.SetDefn(&s_WidgetCloudPiercingLightPower_Strength_NormalStrength_Thickness);
	m_CloudPiercingLightPower_Strength_NormalStrength_Thickness.AddWidgets(bank);

	bank.AddTitle("Scales for Diffuse, Fill and Ambient / Wrap Amount:");
	m_CloudScaleDiffuseFillAmbient_WrapAmount.SetDefn(&s_WidgetCloudScaleDiffuseFillAmbient_WrapAmount);
	m_CloudScaleDiffuseFillAmbient_WrapAmount.AddWidgets(bank);

	bank.PopGroup(); //CloudHat Settings
}

void CloudHatSettings::UpdateUI()
{
	m_CloudList.UpdateUI();
#if RMPTFX_BANK
	m_CloudColor.UpdateWidget();
	m_CloudLightColor.UpdateWidget();
	m_CloudAmbientColor.UpdateWidget();
	m_CloudBounceColor.UpdateWidget();
#if !LOCK_CLOUD_COLORS_TO_TIMECYCLE
	m_CloudSkyColor.UpdateWidget();
	m_CloudEastColor.UpdateWidget();
	m_CloudWestColor.UpdateWidget();
#endif
	m_CloudScaleFillColors.UpdateWidget();
	m_CloudDensityShift_Scale_ScatteringConst_Scale.UpdateWidget();
	m_CloudPiercingLightPower_Strength_NormalStrength_Thickness.UpdateWidget();
	m_CloudScaleDiffuseFillAmbient_WrapAmount.UpdateWidget();
#endif
}
#endif

} // namespace rage
