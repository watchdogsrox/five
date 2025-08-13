///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLightningSettings.h
//	BY	: 	Anoop Ravi Thomas
//	FOR	:	Rockstar Games
//	ON	:	29 November 2012
//	WHAT:	Lightning Effects
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFXLIGHTNINGSETTINGS_H
#define VFXLIGHTNINGSETTINGS_H

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// Rage headers

#include "data/base.h"
#include "parser/macros.h"
#include "parser/manager.h"
#include "rmptfx/ptxkeyframe.h"
#include "fwmaths/random.h"

// Framework headers


// Game headers


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	



///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////		

#define LIGHTNING_NUM_LEVEL_SUBDIVISIONS (5)
#define LIGHTNING_SETTINGS_NUM_VARIATIONS (3)
#define MAX_LIGHTNING_BLAST (10)
///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum eLightningType
{
	LIGHTNING_TYPE_NONE = 0,
	LIGHTNING_TYPE_DIRECTIONAL_BURST,
	LIGHTNING_TYPE_CLOUD_BURST,
	LIGHTNING_TYPE_STRIKE,
	LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE,
	LIGHTNING_TYPE_TOTAL
};

///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

namespace rage {

struct VfxLightningFrame
{
	Vec2V m_Burst;
	Vec2V m_MainIntensity;
	Vec2V m_BranchIntensity;

	VfxLightningFrame();

};

struct VfxLightningKeyFrames
{
public:
	VfxLightningKeyFrames(){}
	virtual ~VfxLightningKeyFrames() 
	{
	}
	ptxKeyframe m_LightningMainIntensity;
	ptxKeyframe m_LightningBranchIntensity;
	PAR_PARSABLE;
};

struct VfxLightningTimeCycleSettings : public datBase
{
public:
	VfxLightningTimeCycleSettings();
	virtual ~VfxLightningTimeCycleSettings()
	{
	}
	atHashString GetLightningTCMod(eLightningType lightningType) const;
#if __BANK
	void AddWidgets(bkBank &bank);
	void TCModifierChange(eLightningType lightningType);
#endif
protected:

	atHashString m_tcLightningDirectionalBurst;
	atHashString m_tcLightningCloudBurst;
	atHashString m_tcLightningStrikeOnly;
	atHashString m_tcLightningDirectionalBurstWithStrike;

	PAR_PARSABLE;

};

class VfxLightningDirectionalBurstSettings
{
public:
	VfxLightningDirectionalBurstSettings();
	virtual ~VfxLightningDirectionalBurstSettings(){}
	float GetBurstIntensityRandom() const		{ return fwRandom::GetRandomNumberInRange(m_BurstIntensityMin, m_BurstIntensityMax); }
	float GetBurstDurationRandom() const		{ return fwRandom::GetRandomNumberInRange(m_BurstDurationMin, m_BurstDurationMax); }
	int GetBurstSequenceRandomCount() const		{ return fwRandom::GetRandomNumber() % m_BurstSeqCount; }
	int GetBurstSequenceCount() const			{ return m_BurstSeqCount; }
	float GetRootOrbitXRandom() const			{ return fwRandom::GetRandomNumberInRange(-m_RootOrbitXVariance, m_RootOrbitXVariance); }
	float GetRootOrbitYRandom() const			{ return fwRandom::GetRandomNumberInRange(m_RootOrbitYVarianceMin, m_RootOrbitYVarianceMax); }
	float GetBurstSeqOrbitXRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_BurstSeqOrbitXVariance, m_BurstSeqOrbitXVariance); }
	float GetBurstSeqOrbitYRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_BurstSeqOrbitXVariance, m_BurstSeqOrbitXVariance); }

#if __BANK
	virtual void AddWidgets(bkBank &bank);
#endif

protected:
	float m_BurstIntensityMin;
	float m_BurstIntensityMax;
	float m_BurstDurationMin;
	float m_BurstDurationMax;
	int m_BurstSeqCount;
	float m_RootOrbitXVariance;
	float m_RootOrbitYVarianceMin;
	float m_RootOrbitYVarianceMax;
	float m_BurstSeqOrbitXVariance;
	float m_BurstSeqOrbitYVariance;
	PAR_PARSABLE;
};


class VfxLightningCloudBurstCommonSettings
{
public:
	VfxLightningCloudBurstCommonSettings();
	virtual ~VfxLightningCloudBurstCommonSettings(){}
	float GetBurstDurationRandom() const		{ return fwRandom::GetRandomNumberInRange(m_BurstDurationMin, m_BurstDurationMax); }
	float GetDelayRandom() const		{ return fwRandom::GetRandomNumberInRange(m_DelayMin, m_DelayMax); }
	float GetSizeRandom() const		{ return fwRandom::GetRandomNumberInRange(m_SizeMin, m_SizeMax); }
	int GetBurstSequenceRandomCount() const	{ return fwRandom::GetRandomNumberInRange(1, Max(1, m_BurstSeqCount));}
	int GetBurstSequenceCount() const			{ return m_BurstSeqCount; }
	float GetRootOrbitXRandom() const			{ return fwRandom::GetRandomNumberInRange(-m_RootOrbitXVariance, m_RootOrbitXVariance); }
	float GetRootOrbitYRandom() const			{ return fwRandom::GetRandomNumberInRange(m_RootOrbitYVarianceMin, m_RootOrbitYVarianceMax); }
	float GetLocalOrbitXRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_LocalOrbitXVariance, m_LocalOrbitXVariance); }
	float GetLocalOrbitYRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_LocalOrbitYVariance, m_LocalOrbitYVariance); }
	float GetBurstSeqOrbitXRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_BurstSeqOrbitXVariance, m_BurstSeqOrbitXVariance); }
	float GetBurstSeqOrbitYRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_BurstSeqOrbitXVariance, m_BurstSeqOrbitXVariance); }

#if __BANK
	void AddWidgets(bkBank &bank);
#endif

private:
	float m_BurstDurationMin;
	float m_BurstDurationMax;
	int m_BurstSeqCount;
	float m_RootOrbitXVariance;
	float m_RootOrbitYVarianceMin;
	float m_RootOrbitYVarianceMax;
	float m_LocalOrbitXVariance;
	float m_LocalOrbitYVariance;
	float m_BurstSeqOrbitXVariance;
	float m_BurstSeqOrbitYVariance;
	float m_DelayMin;
	float m_DelayMax;
	float m_SizeMin;
	float m_SizeMax;
	PAR_PARSABLE;
};


class VfxLightningCloudBurstSettings
{
public:
	VfxLightningCloudBurstSettings();
	virtual ~VfxLightningCloudBurstSettings(){}
	float GetLightExponentFallOff() const		{ return m_LightSourceExponentFalloff; }
	float GetLightDeltaPos() const			{ return m_LightDeltaPos; }
	float GetLightDistance() const			{ return m_LightDistance; }
	const Color32& GetLightColor() const	{ return m_LightColor; }
	float GetBurstIntensityRandom() const	{ return fwRandom::GetRandomNumberInRange(m_BurstIntensityMin, m_BurstIntensityMax); }
	float GetBurstMinIntensity() const		{ return m_BurstIntensityMin; }
	float GetBurstMaxIntensity() const		{ return m_BurstIntensityMax; }
	const VfxLightningCloudBurstCommonSettings& GetCloudBurstCommonSettings() const	{ return m_CloudBurstCommonSettings; }

#if __BANK
	void AddWidgets(bkBank &bank);
#endif
private:
	float m_BurstIntensityMin;
	float m_BurstIntensityMax;
	float m_LightSourceExponentFalloff;
	float m_LightDeltaPos;
	float m_LightDistance;
	Color32 m_LightColor;
	VfxLightningCloudBurstCommonSettings m_CloudBurstCommonSettings;
	PAR_PARSABLE;

};

struct VfxLightningStrikeSplitPointSettings
{
	VfxLightningStrikeSplitPointSettings();
	virtual ~VfxLightningStrikeSplitPointSettings(){}
	float	m_FractionMin;
	float	m_FractionMax;		
	float	m_DeviationDecay;
	float	m_DeviationRightVariance;
	PAR_PARSABLE;
};

struct VfxLightningStrikeForkPointSettings
{

	VfxLightningStrikeForkPointSettings();
	virtual ~VfxLightningStrikeForkPointSettings(){}
	float	m_DeviationRightVariance;
	float	m_DeviationForwardMin;
	float	m_DeviationForwardMax;
	float	m_LengthMin;
	float	m_LengthMax;
	float	m_LengthDecay;
	PAR_PARSABLE;
};

class VfxLightningStrikeVariationSettings
{
public:

	VfxLightningStrikeVariationSettings();
	virtual ~VfxLightningStrikeVariationSettings(){}
	void GetKeyFrameSettings(float time, VfxLightningFrame &settings) const;
	void SetKeyFrameSettings(float time, const VfxLightningFrame &settings);
	float GetDurationRandom() const					{ return fwRandom::GetRandomNumberInRange(m_DurationMin, m_DurationMax); }

	float GetEndPointOffsetRandom() const			{ return fwRandom::GetRandomNumberInRange(-m_EndPointOffsetXVariance, m_EndPointOffsetXVariance); }
	float GetAnimationTimeRandom() const			{ return fwRandom::GetRandomNumberInRange(m_AnimationTimeMin, m_AnimationTimeMax); }

	// for Zig Zag split Point
	float GetZigZagFractionRandom() const			{ return fwRandom::GetRandomNumberInRange(m_ZigZagSplitPoint.m_FractionMin, m_ZigZagSplitPoint.m_FractionMax); }
	float GetZigZagDeviationRightRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_ZigZagSplitPoint.m_DeviationRightVariance, m_ZigZagSplitPoint.m_DeviationRightVariance); }	
	float GetZigZagDeviationDecay()	 const			{ return m_ZigZagSplitPoint.m_DeviationDecay; }

	// for Fork + Zig Zag split Point
	float GetForkFractionRandom() const				{ return fwRandom::GetRandomNumberInRange(m_ForkZigZagSplitPoint.m_FractionMin, m_ForkZigZagSplitPoint.m_FractionMax); }
	float GetForkZigZagDeviationRightRandom() const	{ return fwRandom::GetRandomNumberInRange(-m_ForkZigZagSplitPoint.m_DeviationRightVariance, m_ForkZigZagSplitPoint.m_DeviationRightVariance); }	
	float GetForkZigZagDeviationDecay() const		{ return m_ForkZigZagSplitPoint.m_DeviationDecay; }

	// for Fork new point
	float GetForkDeviationRightRandom() const		{ return fwRandom::GetRandomNumberInRange(-m_ForkPoint.m_DeviationRightVariance, m_ForkPoint.m_DeviationRightVariance); }
	float GetForkDeviationForwardRandom() const		{ return fwRandom::GetRandomNumberInRange(m_ForkPoint.m_DeviationForwardMin, m_ForkPoint.m_DeviationForwardMax); }	
	float GetForkLengthRandom() const				{ return fwRandom::GetRandomNumberInRange(m_ForkPoint.m_LengthMin, m_ForkPoint.m_LengthMax); }
	float GetForkLengthDecay() const				{ return m_ForkPoint.m_LengthDecay;	}

	//Lightning Subdivision
	u8		GetSubdivisionPatternMask() const		{ return m_SubdivisionPatternMask; }
	
#if __BANK
	void AddWidgets(bkBank &bank);
	void UpdateUI();
#endif

private:
	//Base Settings
	float	m_DurationMin;
	float	m_DurationMax;
	float	m_AnimationTimeMin;
	float	m_AnimationTimeMax;
	float	m_EndPointOffsetXVariance;

	u8		m_SubdivisionPatternMask;

	// for Zig Zag split Point
	VfxLightningStrikeSplitPointSettings m_ZigZagSplitPoint;
	// for Fork + Zig Zag split Point
	VfxLightningStrikeSplitPointSettings m_ForkZigZagSplitPoint;
	// for Fork new point
	VfxLightningStrikeForkPointSettings m_ForkPoint;

	VfxLightningKeyFrames m_KeyFrameData;
	PAR_PARSABLE;

};
class VfxLightningStrikeSettings
{
public:

	VfxLightningStrikeSettings();
	virtual ~VfxLightningStrikeSettings() 
	{
	}
	const VfxLightningStrikeVariationSettings &GetVariation(int index) const{ return m_variations[index]; }

	//Base Settings

	float GetBaseIntensityRandom() const				{ return fwRandom::GetRandomNumberInRange(m_IntensityMin, m_IntensityMax); }
	float GetBaseIntensityFlickerRandom() const			{ return fwRandom::GetRandomNumberInRange(m_IntensityFlickerMin, m_IntensityFlickerMax); }
	float GetBaseIntensiyFlickerFrequency() const		{ return m_IntensityFlickerFrequency; }
	float GetBaseWidthRandom() const					{ return fwRandom::GetRandomNumberInRange(m_WidthMin, m_WidthMax); }
	float GetWidthMinClamp() const						{ return m_WidthMinClamp; }
	float GetIntensityMinClamp() const					{ return m_IntensityMinClamp; }
	u8	GetSubdivisionCount() const						{ return m_SubdivisionCount; }
	//Level Decay Settings
	float GetIntensityLevelDecayRandom() const			{ return fwRandom::GetRandomNumberInRange(m_IntensityLevelDecayMin, m_IntensityLevelDecayMax); }
	float GetWidthLevelDecayRandom() const				{ return fwRandom::GetRandomNumberInRange(m_WidthLevelDecayMin, m_WidthLevelDecayMax); }

	//Noise Settings

	float GetNoiseTexScale() const						{ return m_NoiseTexScale; }
	float GetNoiseAmplitude() const						{ return m_NoiseAmplitude; }
	float GetNoiseAmpMinDistLerp() const				{ return m_NoiseAmpMinDistLerp; }
	float GetNoiseAmpMaxDistLerp() const				{ return m_NoiseAmpMaxDistLerp; }

	float GetMaxDistanceForExposureReset() const		{ return m_MaxDistanceForExposureReset; }
	float GetAngularWidthFactor() const					{ return m_AngularWidthFactor; }
	float GetMaxHeightForStrikes() const				{ return m_MaxHeightForStrike; }
	float GetCoronaIntensityMult() const				{ return m_CoronaIntensityMult; }
	float GetBaseCoronaSize() const						{ return m_BaseCoronaSize; }
	const Color32 &GetBaseColor() const					{ return m_BaseColor; }
	const Color32 &GetFogColor() const					{ return m_FogColor; }
	float GetOrbitDirXRandom() const					{ return fwRandom::GetRandomNumberInRange(-m_OrbitDirXVariance, m_OrbitDirXVariance);  }
	float GetOrbitDirYRandom() const					{ return fwRandom::GetRandomNumberInRange(m_OrbitDirYVarianceMin, m_OrbitDirYVarianceMax);  }
	float GetCloudLightIntensityMult() const			{ return m_CloudLightIntensityMult; }
	float GetCloudLightDeltaPos() const					{ return m_CloudLightDeltaPos; }
	float GetCloudLightRadius() const					{ return m_CloudLightRadius; }
	float GetCloudLightFallOffExponent() const			{ return m_CloudLightFallOffExponent; }
	float GetMaxDistanceForBurst() const				{ return m_MaxDistanceForBurst; }
	float GetBurstThresholdIntensity() const			{ return m_BurstThresholdIntensity; }
	const VfxLightningCloudBurstCommonSettings& GetCloudBurstCommonSettings() const { return m_CloudBurstCommonSettings; }

	float GetLightningFadeWidthFactor() const			{ return m_LightningFadeWidth; }
	float GetLightningFadeWidthFalloffExp() const		{ return m_LightningFadeWidthFalloffExp; }
#if __BANK
	void AddWidgets(bkBank &bank);
	void UpdateUI();
#endif

private:
	//Common strike settings
	//Base Settings
	float	m_IntensityMin;
	float	m_IntensityMax;
	float	m_IntensityMinClamp;
	float	m_WidthMin;
	float	m_WidthMax;
	float	m_WidthMinClamp;
	float	m_IntensityFlickerMin;
	float	m_IntensityFlickerMax;
	float	m_IntensityFlickerFrequency;

	//Level Decay Settings
	float	m_IntensityLevelDecayMin;
	float	m_IntensityLevelDecayMax;
	float	m_WidthLevelDecayMin;
	float	m_WidthLevelDecayMax;

	//Noise Settings
	float	m_NoiseTexScale;
	float	m_NoiseAmplitude;
	float	m_NoiseAmpMinDistLerp;
	float	m_NoiseAmpMaxDistLerp;

	//Lightning Subdivision
	u8		m_SubdivisionCount;

	float	m_OrbitDirXVariance;
	float	m_OrbitDirYVarianceMin;
	float	m_OrbitDirYVarianceMax;
	float	m_MaxDistanceForExposureReset;
	float	m_AngularWidthFactor;
	float	m_MaxHeightForStrike;
	float	m_CoronaIntensityMult;
	float	m_BaseCoronaSize;
	Color32 m_BaseColor;
	Color32 m_FogColor;
	float	m_CloudLightIntensityMult;
	float	m_CloudLightDeltaPos;
	float	m_CloudLightRadius;
	float	m_CloudLightFallOffExponent;
	float	m_MaxDistanceForBurst;
	float	m_BurstThresholdIntensity;
	float	m_LightningFadeWidth;
	float	m_LightningFadeWidthFalloffExp;
	VfxLightningStrikeVariationSettings m_variations[LIGHTNING_SETTINGS_NUM_VARIATIONS];
	VfxLightningCloudBurstCommonSettings m_CloudBurstCommonSettings;
	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class VfxLightningSettings : public datBase
{
public:
	VfxLightningSettings();
	virtual ~VfxLightningSettings() 
	{
	}

	int GetLightningOccurranceChance() const										{ return m_lightningOccurranceChance; }
	float GetLightningShakeIntensity() const										{ return m_lightningShakeIntensity; }
	
	//Lightning Directional Burst Settings
	const VfxLightningDirectionalBurstSettings& GetDirectionalBurstSettings() const	{ return m_DirectionalBurstSettings; }

	//Lightning Cloud Burst Settings
	const VfxLightningCloudBurstSettings& GetCloudBurstSettings()const				{ return m_CloudBurstSettings; }
	//Lightning strike settings
	const VfxLightningStrikeSettings& GetStrikeSettings() const						{ return m_StrikeSettings; }
	const VfxLightningTimeCycleSettings& GetTimeCycleSettings() const				{ return m_lightningTimeCycleMods; }

	//Loading/Saving/Bank functionality
	bool Load(const char *filename);

#if __BANK
	bool Save(const char *filename);
	void AddWidgets(bkBank &bank);
	void UpdateUI();
#endif
	
private:
	void PostLoad();

private:

	int m_lightningOccurranceChance;
	float m_lightningShakeIntensity;
	VfxLightningTimeCycleSettings m_lightningTimeCycleMods;
	VfxLightningDirectionalBurstSettings m_DirectionalBurstSettings;
	VfxLightningCloudBurstSettings m_CloudBurstSettings;
	VfxLightningStrikeSettings m_StrikeSettings;

	PAR_PARSABLE;
};


} // namespace rage

#endif
