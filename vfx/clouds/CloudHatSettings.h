// 
// vfx/clouds/CloudHatSettings.h 
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
// 

#ifndef SHADERCONTROLLERS_CLOUDHATSETTINGS_H 
#define SHADERCONTROLLERS_CLOUDHATSETTINGS_H 

#include "data/base.h"
#include "parser/macros.h"
#include "rmptfx/ptxkeyframe.h"
#include "CloudListController.h"

#define LOCK_CLOUD_COLORS_TO_TIMECYCLE  0	// don't keep track of seperate d tuning values for the clouds for sky color, etc.

#define SYNC_TO_TIMECYCLE_TIMES			1	// force keyframe times to be equal to time cycle times.

namespace rage {

struct CloudFrame
{
	Vec4V m_PiercingLightPower_Strength_NormalStrength_Thickness;
	Vec3V m_ScaleDiffuseFillAmbient;
	Vec4V m_ScaleSkyBounceEastWest;
	Vec3V m_LightColor;	
	Vec3V m_CloudColor;
	Vec3V m_AmbientColor;
	Vec3V m_SkyColor;
	Vec3V m_BounceColor;
	Vec3V m_EastColor;
	Vec3V m_WestColor;
	Vec2V m_DensityShift_Scale;
	float m_ScatteringConst;
	float m_ScatteringScale;
	float m_WrapAmount;
	float m_MSAAErrorRef;
	
	Vec3V m_SunDirection; // just used to save the light direction for the render thread, not really blended, it's set directly in clouds.cpp ComputeFrameSettings()

	CloudFrame();
	CloudFrame& operator*=(float mag);
	CloudFrame& operator+=(const CloudFrame& other);

#if __BANK
	void AddWidgets(bkBank &bank);
	void CopyTimeCycleColors(float time);
#endif //__BANK
};
CloudFrame operator+(const CloudFrame& a, const CloudFrame& b);
CloudFrame operator-(const CloudFrame& a, const CloudFrame& b);
CloudFrame operator*(const CloudFrame& a, const float mag);
CloudFrame operator*(const float mag, const CloudFrame& b);

//////////////////////////////////////////////////////////////////////
// PURPOSE
//	Controls the weather dependent elements of the cloud hat with keyframes
//
class CloudHatSettings : public datBase
{
public:
	CloudHatSettings();

	void GetCloudSettings(float time, CloudFrame &settings) const;
	void SetCloudSettings(float time, const CloudFrame &settings);
	const CloudListController &GetCloudListController() const	{ return m_CloudList; }

	//Loading/Saving/Bank functionality
	bool Load(const char *filename);
	void SyncTCHours(ptxKeyframe & keyFrame);
	void SyncToTimeCycleTimes();
	static float GetTime24();
#if __BANK
	bool Save(const char *filename);
	void AddWidgets(bkBank &bank);
	void UpdateUI();
	void SetLightColorFromTimeCycle();
#endif

	PAR_SIMPLE_PARSABLE;

private:
	void PostLoad();
	void PreSave();

private:
	//Cloud Parameters
	CloudListController m_CloudList;
	ptxKeyframe m_CloudColor;
	ptxKeyframe m_CloudLightColor;
	ptxKeyframe m_CloudAmbientColor;
	ptxKeyframe m_CloudSkyColor;
	ptxKeyframe m_CloudBounceColor;
	ptxKeyframe m_CloudEastColor;
	ptxKeyframe m_CloudWestColor;
	ptxKeyframe m_CloudScaleFillColors;  // sky, bounce, east, west scales
	ptxKeyframe m_CloudDensityShift_Scale_ScatteringConst_Scale;
	ptxKeyframe m_CloudPiercingLightPower_Strength_NormalStrength_Thickness;
	ptxKeyframe m_CloudScaleDiffuseFillAmbient_WrapAmount;
};

} // namespace rage

#endif // SHADERCONTROLLERS_CLOUDHATSETTINGS_H 
