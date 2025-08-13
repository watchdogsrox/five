///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLightningSettings.cpp
//	BY	: 	Anoop Ravi Thomas
//	FOR	:	Rockstar Games
//	ON	:	17 December 2012
//	WHAT:	Lightning Effects Settings
//
///////////////////////////////////////////////////////////////////////////////

#include "VfxLightningSettings.h"
#include "VfxLightningSettings_parser.h"

#include "timecycle/tckeyframe.h"
#include "timecycle/TimeCycle.h"
#include "fwmaths/random.h"

#include "bank/bank.h"
#include "bank/combo.h"

#include "vfx/vfxwidget.h"

#include "game/weather.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()	

namespace rage {

#if __BANK

	int g_tcModDirectionalBurst = 0; 
	int g_tcModDirectionalBurstWithStrike = 0; 
	int g_tcModCloudBurst = 0; 
	int g_tcModStrikeOnly = 0; 
	bkGroup* g_BankTCModLightningGroup = NULL;
	bkCombo* g_BankTCModDirectionalBurst = NULL;
	bkCombo* g_BankTCModDirectionalBurstWithStrike = NULL;
	bkCombo* g_BankTCModCloudBurst = NULL;
	bkCombo* g_BankTCModStrikeOnly = NULL;
	VfxLightningTimeCycleSettings* g_lightningTimeCycleSettings = NULL;
#endif

	//------------------------------------------------------------------------------
	VfxLightningFrame::VfxLightningFrame()
		: m_Burst(0.0f, 0.0f)
		, m_MainIntensity(0.0f, 0.0f)
		, m_BranchIntensity(0.0f, 0.0f)
	{
	}


	VfxLightningDirectionalBurstSettings::VfxLightningDirectionalBurstSettings()
		: m_BurstIntensityMin(0.0f)
		, m_BurstIntensityMax(0.0f)
		, m_BurstDurationMin(0.0f)
		, m_BurstDurationMax(0.0f)
		, m_BurstSeqCount(0)
		, m_RootOrbitXVariance(0.0f)
		, m_RootOrbitYVarianceMin(0.0f)
		, m_RootOrbitYVarianceMax(0.0f)
		, m_BurstSeqOrbitXVariance(0.0f)
		, m_BurstSeqOrbitYVariance(0.0f)
	{

	}

	VfxLightningTimeCycleSettings::VfxLightningTimeCycleSettings()
		: m_tcLightningDirectionalBurst(u32(0))
		, m_tcLightningCloudBurst(u32(0))
		, m_tcLightningStrikeOnly(u32(0))
		, m_tcLightningDirectionalBurstWithStrike(u32(0))
	{

	}

	atHashString VfxLightningTimeCycleSettings::GetLightningTCMod(eLightningType lightningType) const
	{
		switch(lightningType)
		{
		case LIGHTNING_TYPE_CLOUD_BURST:
			return m_tcLightningCloudBurst;
		case LIGHTNING_TYPE_DIRECTIONAL_BURST:
			return m_tcLightningDirectionalBurst;
		case LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE:
			return m_tcLightningDirectionalBurstWithStrike;
		case LIGHTNING_TYPE_STRIKE:
			return m_tcLightningStrikeOnly;
		default:
			return u32(0);
		}
	}
#if __BANK


#endif

	VfxLightningCloudBurstCommonSettings::VfxLightningCloudBurstCommonSettings()
		: m_BurstDurationMin(0.0f)
		, m_BurstDurationMax(0.0f)
		, m_BurstSeqCount(0)
		, m_RootOrbitXVariance(0.0f)
		, m_RootOrbitYVarianceMin(0.0f)
		, m_RootOrbitYVarianceMax(0.0f)
		, m_LocalOrbitXVariance(0.0f)
		, m_LocalOrbitYVariance(0.0f)
		, m_BurstSeqOrbitXVariance(0.0f)
		, m_BurstSeqOrbitYVariance(0.0f)
		, m_DelayMin(0.0f)
		, m_DelayMax(0.0f)
		, m_SizeMin(0.0f)
		, m_SizeMax(0.0f)
	{

	}

	VfxLightningCloudBurstSettings::VfxLightningCloudBurstSettings()
		: m_BurstIntensityMin(0.0f)
		, m_BurstIntensityMax(0.0f)
		, m_LightSourceExponentFalloff(0.0f)
		, m_LightDeltaPos(0.0f)
		, m_LightDistance(0.0f)
		, m_LightColor(0)
	{

	}


	VfxLightningStrikeSplitPointSettings::VfxLightningStrikeSplitPointSettings()
		: m_FractionMin(0.0f)
		, m_FractionMax(0.0f)
		, m_DeviationDecay(0.0f)
		, m_DeviationRightVariance(0.0f)
	{

	}
	VfxLightningStrikeForkPointSettings::VfxLightningStrikeForkPointSettings()
		: m_DeviationRightVariance(0.0f)
		, m_DeviationForwardMin(0.0f)
		, m_DeviationForwardMax(0.0f)
		, m_LengthMin(0.0f)
		, m_LengthMax(0.0f)
		, m_LengthDecay(0.0f)
	{

	}
	VfxLightningStrikeVariationSettings::VfxLightningStrikeVariationSettings()
		: m_DurationMin(0.0f)
		, m_DurationMax(0.0f)
		, m_AnimationTimeMin(0.0f)
		, m_AnimationTimeMax(0.0f)
		, m_EndPointOffsetXVariance(0.0f)
		, m_SubdivisionPatternMask(0)
	{
	}

	void VfxLightningStrikeVariationSettings::GetKeyFrameSettings(float time, VfxLightningFrame &settings) const
	{
		Vec4V vKeyData;
		ScalarV vTime = ScalarVFromF32(time);
		
		vKeyData = m_KeyFrameData.m_LightningMainIntensity.Query(vTime);
		settings.m_MainIntensity = vKeyData.GetXY();

		vKeyData = m_KeyFrameData.m_LightningBranchIntensity.Query(vTime);
		settings.m_BranchIntensity = vKeyData.GetXY();
	}

	void VfxLightningStrikeVariationSettings::SetKeyFrameSettings(float time, const VfxLightningFrame &settings)
	{
		Vec4V vKeyData(V_ONE);
		ScalarV vTime = ScalarVFromF32(time);
		
		vKeyData.SetX(settings.m_MainIntensity.GetX());
		vKeyData.SetY(settings.m_MainIntensity.GetY());
		m_KeyFrameData.m_LightningMainIntensity.AddEntry(vTime, vKeyData);

		vKeyData.SetX(settings.m_BranchIntensity.GetX());
		vKeyData.SetY(settings.m_BranchIntensity.GetY());
		m_KeyFrameData.m_LightningBranchIntensity.AddEntry(vTime, vKeyData);	
	}

#if __BANK
	void VfxLightningStrikeVariationSettings::UpdateUI()
	{
#if RMPTFX_BANK
		m_KeyFrameData.m_LightningMainIntensity.UpdateWidget();
		m_KeyFrameData.m_LightningBranchIntensity.UpdateWidget();
#endif	
	}

#endif
	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------
	VfxLightningStrikeSettings::VfxLightningStrikeSettings()
		: m_IntensityMin(0.0f)
		, m_IntensityMax(0.0f)
		, m_IntensityMinClamp(0.0f)
		, m_IntensityFlickerMin(0.0f)
		, m_IntensityFlickerMax(0.0f)
		, m_IntensityFlickerFrequency(0.0f)
		, m_WidthMin(0.0f)
		, m_WidthMax(0.0f)
		, m_WidthMinClamp(0.0f)
		, m_IntensityLevelDecayMin(0.0f)
		, m_IntensityLevelDecayMax(0.0f)
		, m_WidthLevelDecayMin(0.0f)
		, m_WidthLevelDecayMax(0.0f)
		, m_SubdivisionCount(0)
		, m_OrbitDirXVariance(0.0f)
		, m_OrbitDirYVarianceMin(0.0f)
		, m_OrbitDirYVarianceMax(0.0f)
		, m_MaxDistanceForExposureReset(0.0f)
		, m_AngularWidthFactor(0.0f)
		, m_MaxHeightForStrike(0.0f)
		, m_CoronaIntensityMult(0.0f)
		, m_BaseCoronaSize(0.0f)
		, m_BaseColor(0,0,0)
		, m_FogColor(0,0,0)
		, m_CloudLightIntensityMult(0.0f)
		, m_CloudLightDeltaPos(0.0f)
		, m_CloudLightRadius(0.0f)
		, m_MaxDistanceForBurst(0.0f)
		, m_BurstThresholdIntensity(0.0f)
		, m_LightningFadeWidth(0.0f)
		, m_LightningFadeWidthFalloffExp(0.0f)
		, m_NoiseTexScale(0.0f)
		, m_NoiseAmplitude(0.0f)
		, m_NoiseAmpMinDistLerp(0.0f)
		, m_NoiseAmpMaxDistLerp(0.0f)
	{
	}

#if __BANK
	void VfxLightningStrikeSettings::UpdateUI()
	{
		for(int i=0; i<LIGHTNING_SETTINGS_NUM_VARIATIONS; i++)
		{
			m_variations[i].UpdateUI();
		}
	}
#endif

	VfxLightningSettings::VfxLightningSettings()
		: m_lightningOccurranceChance(0)
		, m_lightningShakeIntensity(0.0f)
	{

	}	

	bool VfxLightningSettings::Load(const char *filename)
	{
		bool success = PARSER.LoadObject(filename, "xml", *this);

#if __BANK
		if(success)
			UpdateUI();
#endif //__BANK

		return success;
	}

	void VfxLightningSettings::PostLoad()
	{
	}

#if __BANK
	bool VfxLightningSettings::Save(const char *filename)
	{
		bool success = PARSER.SaveObject(filename, "xml", this);

		return success;
	}

	void VfxLightningSettings::UpdateUI()
	{
		m_StrikeSettings.UpdateUI();

	}
	void VfxLightningSettings::AddWidgets(bkBank &bank)
	{	

		// static const float sMin = 0.0f;
		// static const float sMax = 1.0f;
		// static const float sDelta = 0.01f;

		// Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
		// Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);

		// Vec4V vDefaultValue = Vec4V(V_ZERO);


		//Lightning Settings
		bank.PushGroup("Lightning Settings", false);
		{
			bank.AddSlider("Lightning Occurrance Chance", &m_lightningOccurranceChance, 0, 200, 1, NullCB, "");
			bank.AddSlider("Lightning Shake Intensity", &m_lightningShakeIntensity, 0.0f, 1.0f, 0.05f, NullCB, "");	
			m_lightningTimeCycleMods.AddWidgets(bank);
			m_DirectionalBurstSettings.AddWidgets(bank);
			m_CloudBurstSettings.AddWidgets(bank);
			m_StrikeSettings.AddWidgets(bank);	
		}

		bank.PopGroup(); //Lightning Settings
	}

	void VfxLightningTimeCycleSettings::TCModifierChange(eLightningType lightningType)
	{
		switch(lightningType)
		{
		case LIGHTNING_TYPE_DIRECTIONAL_BURST:
			m_tcLightningDirectionalBurst = g_timeCycle.GetModifierKey(g_tcModDirectionalBurst);
			break;
		case LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE:
			m_tcLightningDirectionalBurstWithStrike = g_timeCycle.GetModifierKey(g_tcModDirectionalBurstWithStrike);
			break;
		case LIGHTNING_TYPE_CLOUD_BURST:
			m_tcLightningCloudBurst = g_timeCycle.GetModifierKey(g_tcModCloudBurst);
			break;
		case LIGHTNING_TYPE_STRIKE:
			m_tcLightningStrikeOnly = g_timeCycle.GetModifierKey(g_tcModStrikeOnly);
			break;
		default:
			break;
		}
	}

	void UpdateTCList()
	{
		if(g_BankTCModDirectionalBurst == NULL || 
			g_BankTCModDirectionalBurstWithStrike == NULL ||
			g_BankTCModCloudBurst == NULL ||
			g_BankTCModStrikeOnly == NULL)
		{
			return;
		}
		
		g_BankTCModDirectionalBurst->Destroy();
		g_BankTCModDirectionalBurstWithStrike->Destroy();
		g_BankTCModCloudBurst->Destroy();
		g_BankTCModStrikeOnly->Destroy();

		const int modifierCount = g_timeCycle.GetModifiersCount();

		atArray<const char*> nameList(modifierCount,modifierCount);

		for(int i=0;i<modifierCount;i++)
		{
			nameList[i] = g_timeCycle.GetModifierName(i);
		}

		bkBank* pBank = vfxWidget::GetBank();
		pBank->SetCurrentGroup(*g_BankTCModLightningGroup);
		{
			g_BankTCModDirectionalBurst = pBank->AddCombo("Directional Burst", &g_tcModDirectionalBurst, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)g_lightningTimeCycleSettings, (CallbackData)LIGHTNING_TYPE_DIRECTIONAL_BURST));
			g_BankTCModDirectionalBurstWithStrike = pBank->AddCombo("Directional Burst With Strike", &g_tcModDirectionalBurstWithStrike, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)g_lightningTimeCycleSettings, (CallbackData)LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE));
			g_BankTCModCloudBurst = pBank->AddCombo("Cloud Burst", &g_tcModCloudBurst, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)g_lightningTimeCycleSettings,(CallbackData) LIGHTNING_TYPE_CLOUD_BURST));
			g_BankTCModStrikeOnly = pBank->AddCombo("Strike Only", &g_tcModStrikeOnly, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)g_lightningTimeCycleSettings, (CallbackData)LIGHTNING_TYPE_STRIKE));

		}
		pBank->UnSetCurrentGroup(*g_BankTCModLightningGroup);

	}
	void VfxLightningTimeCycleSettings::AddWidgets(bkBank &bank)
	{
		const int modifierCount = g_timeCycle.GetModifiersCount();

		atArray<const char*> nameList(modifierCount,modifierCount);

		for(int i=0;i<modifierCount;i++)
		{
			nameList[i] = g_timeCycle.GetModifierName(i);
		}
		g_lightningTimeCycleSettings = this;
		g_tcModDirectionalBurst = g_timeCycle.FindModifierIndex(m_tcLightningDirectionalBurst, NULL); 
		g_tcModDirectionalBurstWithStrike = g_timeCycle.FindModifierIndex(m_tcLightningDirectionalBurstWithStrike, NULL); 
		g_tcModCloudBurst = g_timeCycle.FindModifierIndex(m_tcLightningCloudBurst, NULL); 
		g_tcModStrikeOnly = g_timeCycle.FindModifierIndex(m_tcLightningStrikeOnly, NULL); 



		g_BankTCModLightningGroup = bank.PushGroup("Time Cycle Modifier Selection");
		{
			bank.AddButton("Update TimeCycle Modifiers List",datCallback(CFA1(UpdateTCList)));
			g_BankTCModDirectionalBurst = bank.AddCombo("Directional Burst", &g_tcModDirectionalBurst, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)this, (CallbackData)LIGHTNING_TYPE_DIRECTIONAL_BURST));
			g_BankTCModDirectionalBurstWithStrike = bank.AddCombo("Directional Burst With Strike", &g_tcModDirectionalBurstWithStrike, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)this, (CallbackData)LIGHTNING_TYPE_DIRECTIONAL_BURST_STRIKE));
			g_BankTCModCloudBurst = bank.AddCombo("Cloud Burst", &g_tcModCloudBurst, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)this, (CallbackData)LIGHTNING_TYPE_CLOUD_BURST));
			g_BankTCModStrikeOnly = bank.AddCombo("Strike Only", &g_tcModStrikeOnly, modifierCount, nameList.GetElements(), datCallback(MFA1(VfxLightningTimeCycleSettings::TCModifierChange), (datBase*)this, (CallbackData)LIGHTNING_TYPE_STRIKE));
		}
		bank.PopGroup();
	}

	void VfxLightningDirectionalBurstSettings::AddWidgets(bkBank &bank)
	{

		bank.PushGroup("Directional Burst Settings");
		{
			bank.AddSlider("Intensity Min", &m_BurstIntensityMin, 0.0f, 3000.0f, 0.1f);
			bank.AddSlider("Intensity Max", &m_BurstIntensityMax, 0.0f, 3000.0f, 0.1f);
			bank.AddSlider("Duration Min", &m_BurstDurationMin, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Duration Max", &m_BurstDurationMax, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Max Sequence Count", &m_BurstSeqCount, 1, MAX_LIGHTNING_BLAST, 1);
			bank.AddSlider("Root Orbit X Variance", &m_RootOrbitXVariance, 0.0f, 180.0f, 0.01f);
			bank.AddSlider("Root Orbit Y Min", &m_RootOrbitYVarianceMin, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Root Orbit Y Max", &m_RootOrbitYVarianceMax, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Sequence Orbit X Variance", &m_BurstSeqOrbitXVariance, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Sequence Orbit Y Variance", &m_BurstSeqOrbitYVariance, -180.0f, 180.0f, 0.01f);
		}
		bank.PopGroup();

	}

	void VfxLightningCloudBurstCommonSettings::AddWidgets(bkBank &bank)
	{

		bank.PushGroup("Cloud Burst Common Settings");
		{
			bank.AddSlider("Duration Min", &m_BurstDurationMin, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Duration Max", &m_BurstDurationMax, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Max Sequence Count", &m_BurstSeqCount, 0, MAX_LIGHTNING_BLAST, 1);
			bank.AddSlider("Root Orbit X Variance", &m_RootOrbitXVariance, 0.0f, 180.0f, 0.01f);
			bank.AddSlider("Root Orbit Y Min", &m_RootOrbitYVarianceMin, 0.0f, 90.0f, 0.01f);
			bank.AddSlider("Root Orbit Y Max", &m_RootOrbitYVarianceMax, -0.0f, 90.0f, 0.01f);
			bank.AddSlider("Local Orbit X Variance", &m_LocalOrbitXVariance, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Local Orbit Y Variance", &m_LocalOrbitYVariance, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Sequence Orbit X Variance", &m_BurstSeqOrbitXVariance, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Sequence Orbit Y Variance", &m_BurstSeqOrbitYVariance, -180.0f, 180.0f, 0.01f);
			bank.AddSlider("Delay Min", &m_DelayMin, 0.0f, 90.0f, 0.01f);
			bank.AddSlider("Delay Max", &m_DelayMax, 0.0f, 90.0f, 0.01f);
			bank.AddSlider("Size Min", &m_SizeMin, 0.0f, 5000.0f, 0.01f);
			bank.AddSlider("Size Max", &m_SizeMax, 0.0f, 5000.0f, 0.01f);
		}
		bank.PopGroup();

	}

	void VfxLightningCloudBurstSettings::AddWidgets(bkBank &bank)
	{
		bank.PushGroup("Cloud Burst Settings");
		{
			bank.AddSlider("Intensity Min", &m_BurstIntensityMin, 0.0f, 3000.0f, 0.1f);
			bank.AddSlider("Intensity Max", &m_BurstIntensityMax, 0.0f, 3000.0f, 0.1f);
			bank.AddSlider("Light Source Exponent Fall Off", &m_LightSourceExponentFalloff, 0.0f, 128.0f, 1.0f);
			bank.AddSlider("Light Source Delta Position", &m_LightDeltaPos, -5000.0f, 5000.0f, 0.01f);
			bank.AddSlider("Light Source Distance", &m_LightDistance, 0.0f, 5000.0f, 0.01f);
			bank.AddColor("Light Color", &m_LightColor);
			m_CloudBurstCommonSettings.AddWidgets(bank);
		}
		bank.PopGroup();

	}

	void VfxLightningStrikeVariationSettings::AddWidgets(bkBank &bank)
	{
		
		bank.PushGroup("Base Settings");
		{
			bank.AddSlider("Duration Min", &m_DurationMin, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Duration Max", &m_DurationMax, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("EndPoint Offset X Variance", &m_EndPointOffsetXVariance, -1.0, 1.0, 0.01f);
			bank.AddSlider("Animation Time Min", &m_AnimationTimeMin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Animation Time Max", &m_AnimationTimeMax, 0.0f, 1.0f, 0.01f);
		}				
		bank.PopGroup();

		bank.PushGroup("Subdivision Settings");
		{
			bank.AddSlider("Split Pattern", &m_SubdivisionPatternMask, 0, 0xFF, 1);
		}
		bank.PopGroup();

		bank.PushGroup("Zig Zag Split Point Settings");
		{
			bank.AddSlider("Fraction Min", &m_ZigZagSplitPoint.m_FractionMin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Fraction Max", &m_ZigZagSplitPoint.m_FractionMax, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Deviation Right Variance", &m_ZigZagSplitPoint.m_DeviationRightVariance, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Deviation Decay", &m_ZigZagSplitPoint.m_DeviationDecay, 0.0f, 32.0f, 0.1f);

		}
		bank.PopGroup();

		bank.PushGroup("Fork + Zig Zag Split Point Settings");
		{
			bank.AddSlider("Fraction Min", &m_ForkZigZagSplitPoint.m_FractionMin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Fraction Max", &m_ForkZigZagSplitPoint.m_FractionMax, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Deviation Right Variance", &m_ForkZigZagSplitPoint.m_DeviationRightVariance, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Deviation Decay", &m_ForkZigZagSplitPoint.m_DeviationDecay, 0.0f, 32.0f, 0.1f);

		}
		bank.PopGroup();


		bank.PushGroup("Fork New Point Settings");
		{
			bank.AddSlider("Deviation Right Variance", &m_ForkPoint.m_DeviationRightVariance, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Deviation Forward Min", &m_ForkPoint.m_DeviationForwardMin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Deviation Forward Max", &m_ForkPoint.m_DeviationForwardMax, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Length Min", &m_ForkPoint.m_LengthMin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Length Max", &m_ForkPoint.m_LengthMax, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Length Decay", &m_ForkPoint.m_LengthDecay, 0.0f, 32.0f, 0.1f);

		}
		bank.PopGroup();

		static const float sMin = 0.0f;
		static const float sMax = 1.0f;
		static const float sDelta = 0.01f;

		Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
		Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);

		Vec4V vDefaultValue = Vec4V(V_ZERO);

		//Curve Widgets
		static ptxKeyframeDefn s_WidgetLightningBurst("Burst", 0, PTXKEYFRAMETYPE_FLOAT2, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Min Influence", "Max Influence");
		static ptxKeyframeDefn s_WidgetLightningMainIntensity("Main Intensity", 0, PTXKEYFRAMETYPE_FLOAT2, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Min Influence", "Max Influence");
		static ptxKeyframeDefn s_WidgetLightningBranchIntensity("Branch Intensity", 0, PTXKEYFRAMETYPE_FLOAT2, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Min Influence", "Max Influence");
		static ptxKeyframeDefn s_WidgetLightningCloudLightIntensityRadius("Cloud Light Intensity Radius", 0, PTXKEYFRAMETYPE_FLOAT2, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Intensity", "Radius");

		bank.PushGroup("Key Frame Settings");
		{
			bank.AddTitle("Lightning Main Bolt Intensity:");
			m_KeyFrameData.m_LightningMainIntensity.SetDefn(&s_WidgetLightningBurst);
			m_KeyFrameData.m_LightningMainIntensity.AddWidgets(bank);

			bank.AddTitle("Lightning Branch Intensity:");
			m_KeyFrameData.m_LightningBranchIntensity.SetDefn(&s_WidgetLightningBranchIntensity);
			m_KeyFrameData.m_LightningBranchIntensity.AddWidgets(bank);
			
		}
		bank.PopGroup();
	}

	void VfxLightningStrikeSettings::AddWidgets(bkBank &bank)
	{

		bank.PushGroup("Strike Settings");
		{
			bank.AddSlider("Intensity Min", &m_IntensityMin, 0.0f, 50.0f, 0.01f);
			bank.AddSlider("Intensity Max", &m_IntensityMax, 0.0f, 50.0f, 0.01f);
			bank.AddSlider("Intensity Min Clamp", &m_IntensityMinClamp, 0.0f, 50.0f, 0.001f);
			bank.AddSlider("Flicker Intensity Min", &m_IntensityFlickerMin, 0.0f, 2.0f, 0.01f);
			bank.AddSlider("Flicker Intensity Max", &m_IntensityFlickerMax, 0.0f, 2.0f, 0.01f);
			bank.AddSlider("Flicker Intensity Frequency", &m_IntensityFlickerFrequency, 1.0f, 200.0f, 0.1f);
			bank.AddSlider("Width Min", &m_WidthMin, 0.0f, 100.0f, 0.01f);
			bank.AddSlider("Width Max", &m_WidthMax, 0.0f, 100.0f, 0.01f);
			bank.AddSlider("Width Min Clamp", &m_WidthMinClamp, 0.0f, 100.0f, 0.001f);
			bank.AddSlider("Num Subdivisions", &m_SubdivisionCount, 0, LIGHTNING_NUM_LEVEL_SUBDIVISIONS, 1);
			bank.AddSlider("Orbit Direction X Variance", &m_OrbitDirXVariance, 0.0f, 180.0f, 0.01f);
			bank.AddSlider("Orbit Direction Y Min", &m_OrbitDirYVarianceMin, 0.0, 90.0f, 0.01f);
			bank.AddSlider("Orbit Direction Y Max", &m_OrbitDirYVarianceMax, 0.0f, 90.0f, 0.01f);
			bank.AddSlider("Max Distance for Exposure Reset", &m_MaxDistanceForExposureReset, 0.0f, 10000.0f, 0.01f);
			bank.AddSlider("Angular Width Factor", &m_AngularWidthFactor, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Max Height For Strikes", &m_MaxHeightForStrike, 0.0f, 2000.0f, 0.1f);	
			bank.AddSlider("Corona Intensity Multiplier", &m_CoronaIntensityMult, 0.0f, 10.0f, 0.001f);
			bank.AddSlider("Corona Size Factor", &m_BaseCoronaSize, 0.0, 100.0, 0.001f);
			bank.AddColor("Default Color", &m_BaseColor);
			bank.AddColor("Fog Color", &m_FogColor);
			bank.AddSlider("Cloud Light Radius", &m_CloudLightRadius, 0.0, 5000.0f, 0.1f);
			bank.AddSlider("Cloud Light Delta Position", &m_CloudLightDeltaPos, -5000.0f, 5000.0f, 0.1f);
			bank.AddSlider("Cloud Light Intensity Multiplier", &m_CloudLightIntensityMult, 0.0, 2000.0f, 0.01f);	
			bank.AddSlider("Light Source Exponent Fall Off", &m_CloudLightFallOffExponent, 0.0f, 128.0f, 1.0f);
			bank.AddSlider("Max Distance for Burst", &m_MaxDistanceForBurst, 0.0f, 10000.0f, 0.01f);
			bank.AddSlider("Burst Threshold Intensity", &m_BurstThresholdIntensity, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Lightning Fade Width Factor", &m_LightningFadeWidth, 1.0f, 100.0f, 0.001f);
			bank.AddSlider("Lightning Fade Width Falloff Exponent", &m_LightningFadeWidthFalloffExp, 0.0f, 128.0f, 1.0f);

			bank.PushGroup("Level Decay Settings");
			{
				bank.AddSlider("Intensity Min", &m_IntensityLevelDecayMin, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Intensity  Max", &m_IntensityLevelDecayMax, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Width Min", &m_WidthLevelDecayMin, 0.0f, 1.0f, 0.01f);
				bank.AddSlider("Width Max", &m_WidthLevelDecayMax, 0.0f, 1.0f, 0.01f);

			}
			bank.PopGroup();

			bank.PushGroup("Noise Settings");
			{
				bank.AddSlider("Noise Tex Scale", &m_NoiseTexScale, 0.0f, 100.0f, 0.001f);
				bank.AddSlider("Noise Amplitude", &m_NoiseAmplitude, 0.0f, 100.0f, 0.001f);
				bank.AddSlider("Noise Amp Lerp Min Dist", &m_NoiseAmpMinDistLerp, 0.0f, 10000.0, 0.1f);
				bank.AddSlider("Noise Amp Lerp Max Dist", &m_NoiseAmpMaxDistLerp, 0.0f, 10000.0f, 0.1f);

			}
			bank.PopGroup();

			bank.PushGroup("Cloud Bursts for Strike");
			m_CloudBurstCommonSettings.AddWidgets(bank);
			bank.PopGroup();
			bank.PushGroup("Strike Variations");
			for(int i=0; i<LIGHTNING_SETTINGS_NUM_VARIATIONS; i++)
			{
				atVarString titleStr("Variation %d", i + 1);
				bank.PushGroup(titleStr);
				{
					m_variations[i].AddWidgets(bank);
				}
				bank.PopGroup();
			}
			bank.PopGroup();
		}
		bank.PopGroup();
	}

#endif

} // namespace rage
