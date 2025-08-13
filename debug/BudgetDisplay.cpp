//
// debug/budgetdisplay.cpp
//
// Copyright (C) 1999-2010 Rockstar Games. All Rights Reserved.
//

#if __BANK

//rage
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"
#include "grcore/font.h"
#include "grcore/viewport.h"
#include "math/amath.h"
#include "math/vecmath.h"
#include "grprofile/timebars.h"
#include "fwsys/timer.h"

//game
#include "debug/BudgetDisplay.h"
#include "debug/BudgetDisplay_parser.h"
#include "debug/DebugScene.h"
#include "debug/Rendering/DebugDeferred.h"
#include "cutfile/cutfobject.h"
#include "cutfile/cutfdefines.h"
#include "cutscene/CutsceneCameraEntity.h"
#include "renderer/lights/lights.h"
#include "renderer/PostProcessFX.h"
#include "renderer/rendertargets.h"
#include "scene/debug/PostScanDebug.h"
#include "system/param.h"
#include "timecycle/timecycle.h"
#include "camera/viewports/viewportmanager.h"
#include "scene/world/VisibilityMasks.h"
#include "cutscene/CutSceneManagerNew.h"
#include "parser/manager.h"


//////////////////////////////////////////////////////////////////////////
//
bool BudgetDisplay::ms_ShowMainTimes			= true;
bool BudgetDisplay::ms_ShowShadowTimes			= false;
bool BudgetDisplay::ms_ShowMiscTimes			= false;
bool BudgetDisplay::ms_DrawOutline				= false;
bool BudgetDisplay::ms_AutoBucketDisplay		= true;
u32	 BudgetDisplay::ms_BlinkCounter				= 0U;

bool BudgetDisplay::ms_DisableAllLights			= false;

bool BudgetDisplay::ms_DisableInteriorLights	= false;
bool BudgetDisplay::ms_DisableExteriorLights	= false;

bool BudgetDisplay::ms_DisableStandardLights	= false;
bool BudgetDisplay::ms_DisableCutsceneLights	= false;
bool BudgetDisplay::ms_DisableShadowLights		= false;

bool BudgetDisplay::ms_DisableFillerLights		= false;
bool BudgetDisplay::ms_DisableVolumetricLights	= false;
bool BudgetDisplay::ms_DisableTexturedLights	= false;
bool BudgetDisplay::ms_DisableNormalLights		= false;

bool BudgetDisplay::ms_ColouriseLights			= false;

bool	BudgetDisplay::ms_ShowPieChart			= false;
bool	BudgetDisplay::ms_PieChartSmoothValues	= true;
Vector2	BudgetDisplay::ms_PieChartPos			= Vector2(0.1f, 0.5f);
float	BudgetDisplay::ms_PieChartSize			= 0.1f;

bool	BudgetDisplay::ms_overrideBackgroundOpacity = false;
float	BudgetDisplay::ms_bgOpacityOverrideValue	= 0.4f;

BudgetDisplay::PieChartData	BudgetDisplay::ms_PieChartData;

Color32	BudgetDisplay::ms_ShadowLightsCol 		= Color32(255,   0,		0, 255);
Color32	BudgetDisplay::ms_FillerLightsCol 		= Color32(0,	255,	0, 255);
Color32	BudgetDisplay::ms_OtherLightsCol	 	= Color32(0,	  0,  255, 255);

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawPieChart(const Vector2& pos, float radius, const char** pNames, const float* pSegmentValues, const Color32* pCols, float totalMax, u32 numSegments)
{
	if (pNames == NULL)
		return;

	if (numSegments == 0)
		return;

	float startAngleOffset = 0.0f;
	float currentAngleOffset = startAngleOffset;
	const float ooMaxValue = 1.0f/totalMax;

	char text[128];
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());

	Vector2 textPos = pos;
	textPos.x += radius*1.01f;
	textPos.y -= 0.065f;
	fwBox2D rect;
	rect.x0 = (textPos.x - 0.02f);
	rect.x1 = (textPos.x + 0.15f);
	rect.y0 = (textPos.y - 0.02f);
	rect.y1 = (textPos.y + 0.15f);
	grcDebugDraw::RectAxisAligned(Vector2(rect.x0, rect.y0), Vector2(rect.x1, rect.y1), Color32(0.25f, 0.25f, 0.25f, 0.85f), true);

	float screenWidth = (float)VideoResManager::GetUIWidth();
	float screenHeight = (float)VideoResManager::GetUIHeight();

	for (u32 i = 0; i < numSegments; i++)
	{
		// wedge
		const float curSegmentVal = pSegmentValues[i];

		float curStartAngle = currentAngleOffset;
		float curEndAngle = curStartAngle + (curSegmentVal*TWO_PI*ooMaxValue);
		grcDebugDraw::Arc(pos, radius, pCols[i], curStartAngle, curEndAngle, true, 96);

		currentAngleOffset = curEndAngle;

		// text
		const float rad = 0.0025f;
		grcDebugDraw::Circle(Vector2(textPos.x - rad*2.5f, textPos.y+rad), 0.005f, pCols[i], true, 4);
		sprintf(&text[0], "%-16s %3.0f%%  %2.2fms", pNames[i], curSegmentVal*ooMaxValue*100.0f, curSegmentVal);
		grcDebugDraw::Text(textPos*Vector2(screenWidth, screenHeight), DD_ePCS_Pixels, Color_white, text, false, 1.0f, 1.0f);
		textPos.y += 0.02f;
	}

	grcDebugDraw::TextFontPop();

	if (currentAngleOffset < TWO_PI)
	{
		grcDebugDraw::Arc(pos, radius, Color32(0.33f,0.33f,0.33f), currentAngleOffset, TWO_PI, true, 96);
	}

}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawPieChartView()
{
#if RAGE_TIMEBARS

	// Alpha
	float totalAlphaTime = 0.0f;
	int count;
	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Alpha Bucket", count, totalAlphaTime);
	ms_PieChartData.AddSample(PCE_ALPHA, totalAlphaTime);

	// Lighting
	float lightingTime = 0.0f;
	float totalLightingTime = 0.0f;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Directional/Ambient/Reflections", count, lightingTime);
	totalLightingTime += lightingTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("LOD Lights", count, lightingTime);
	totalLightingTime += lightingTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Ambient Lights", count, lightingTime);
	totalLightingTime += lightingTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Ambient volumes", count, lightingTime);
	totalLightingTime += lightingTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Skin lighting", count, lightingTime);
	totalLightingTime += lightingTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Foliage lighting", count, lightingTime);
	totalLightingTime += lightingTime;

	ms_PieChartData.AddSample(PCE_LIGHTING, totalLightingTime);

	float totalSceneLights = 0.0f;
	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Scene Lights", count, totalSceneLights);
	ms_PieChartData.AddSample(PCE_SCENE_LIGHTS, totalSceneLights);


	// Shadows
	float totalShadowTime = 0.0f;
	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Cascade Shadows", count, totalShadowTime);
	ms_PieChartData.AddSample(PCE_CASCADE_SHADOWS, totalShadowTime);

	float totalLocalShadowTime = 0.0f;
	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Local Shadows", count, totalLocalShadowTime);
	ms_PieChartData.AddSample(PCE_LOCAL_SHADOWS, totalLocalShadowTime);


	// Post FX
	float postfxTime = 0.0f;
	float totalPostfxTime = 0.0f;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("PostFX", count, postfxTime);
	totalPostfxTime += postfxTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("DepthFX", count, postfxTime);
	totalPostfxTime += postfxTime;

	::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Volumetric Effects", count, postfxTime);
	totalPostfxTime += postfxTime;	
	ms_PieChartData.AddSample(PCE_POSTFX, totalPostfxTime);


	const char* pNames[] = { "Scene Lights", "Lighting (Other)", "Cascade Shadows", "Local Shadows", "Alpha", "PostFX" };
	const float pValues[] = {	
								ms_PieChartData.Get(PCE_SCENE_LIGHTS), 
								ms_PieChartData.Get(PCE_LIGHTING), 
								ms_PieChartData.Get(PCE_CASCADE_SHADOWS), 
								ms_PieChartData.Get(PCE_LOCAL_SHADOWS), 
								ms_PieChartData.Get(PCE_ALPHA),
								ms_PieChartData.Get(PCE_POSTFX)
								};
	const float pValuesAvg[] = {	
									ms_PieChartData.GetAvg(PCE_SCENE_LIGHTS), 
									ms_PieChartData.GetAvg(PCE_LIGHTING), 
									ms_PieChartData.GetAvg(PCE_CASCADE_SHADOWS), 
									ms_PieChartData.GetAvg(PCE_LOCAL_SHADOWS), 
									ms_PieChartData.GetAvg(PCE_ALPHA), 
									ms_PieChartData.GetAvg(PCE_POSTFX)
								};

	const Color32 pCols[] = {	
								Color32(1.0f,	0.5f,	0.0f),	// PCE_SCENE_LIGHTS
								Color32(1.0f,	1.0f,	0.25f),	// PCE_LIGHTING  
								Color32(0.25f,	0.25f,	1.0f), 	// PCE_CASCADE_SHADOWS
								Color32(0.25f,	0.85f,	1.0f),	// PCE_LOCAL_SHADOWS
								Color32(1.0f,	0.25f,	0.25f),	// PCE_ALPHA
								Color32(0.25f,	1.0f,	0.25f),	// PCE_POSTFX							 
							};
	const u32 numSegments = (u32)PCE_COUNT;
	const float maxVal = 33.3f;

	DrawPieChart(ms_PieChartPos, ms_PieChartSize, pNames, ms_PieChartSmoothValues ? pValuesAvg : pValues, pCols, maxVal, numSegments);
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::PieChartData::Reset()
{
	for (u32 i = 0; i < (u32)PCE_COUNT; i++)
	{
		actualValue[i] = 0.0f;
		avgValue[i] = 0.0f;
	}

	decayTerm = 0.0333f;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::PieChartData::AddSample(u32 entryId, float sample)
{
	actualValue[entryId] = sample;

	if (avgValue[entryId] == 0.0f)
	{
		avgValue[entryId] = sample;
	}
	else
	{
		avgValue[entryId] += decayTerm * (sample - avgValue[entryId]);
	}
}


//////////////////////////////////////////////////////////////////////////
//
void BudgetEntry::Set(BudgetEntryId _id, const char* _pName, const char* _pDisplayName, float _time, float _budgetTime, BudgetWindowGroupId _windowGroupId, bool _bEnable)
{
	id				= _id;
	time			= _time;
	budgetTime		= _budgetTime;
	bEnable			= _bEnable;
	windowGroupId	= _windowGroupId;
	pName[0]		= '\0';
	pDisplayName[0]	= '\0';

	if (_pName != NULL && strlen(_pName) < sizeof(pName))
	{
		strcpy(&pName[0], _pName);
	}

	if (_pDisplayName != NULL && strlen(_pDisplayName) < sizeof(pDisplayName))
	{
		strcpy(&pDisplayName[0], _pDisplayName);
	}
}

//////////////////////////////////////////////////////////////////////////
//

u32 BudgetDisplay::GetDisabledLightsMask()
{
	if (m_IsActive == false)
	{
		return 0U;
	}

	u32 mask = 0U;

	// all lights
	if (ms_DisableAllLights)
	{
		mask = ~0U;
		return mask;
	}
	
	// two main groups (cutscene and non-cutscene lights)
	if (ms_DisableCutsceneLights)
	{
		mask |= LIGHTFLAG_CUTSCENE;	
	}
	else if (ms_DisableStandardLights)
	{
		mask |= (u32)(~LIGHTFLAG_CUTSCENE);	
	}

	// sub-categories
	if (ms_DisableInteriorLights)
	{
		mask |= (LIGHTFLAG_INTERIOR_ONLY);
	}

	if (ms_DisableExteriorLights)
	{
		mask |= (LIGHTFLAG_EXTERIOR_ONLY);
	}

	if (ms_DisableShadowLights)
	{
		mask |= (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS);
	}

	if (ms_DisableFillerLights)
	{
		mask |= (LIGHTFLAG_NO_SPECULAR);
	}
	
	if (ms_DisableVolumetricLights)
	{
		mask |= (LIGHTFLAG_DRAW_VOLUME);
	}
		
	if (ms_DisableTexturedLights)
	{
		mask |= (LIGHTFLAG_TEXTURE_PROJECTION);
	}

	if (ms_DisableNormalLights)
	{
		mask |= (u32)(~(LIGHTFLAG_VEHICLE | LIGHTFLAG_FX));
	}
	return mask;
}


bool BudgetDisplay::IsLightDisabled(const CLightSource &light)
{
	if (m_IsActive == false)
	{
		return false; //Don't disable lights if budget display is inactive
	}
	if (ms_DisableAllLights)
	{
		return true;
	}

	u32	flags = light.GetFlags();

	// two main groups (cutscene and non-cutscene lights)
	if (ms_DisableCutsceneLights && (flags & LIGHTFLAG_CUTSCENE))
	{
		return true;
	}
	else if ( ms_DisableStandardLights && ((flags & LIGHTFLAG_CUTSCENE) == 0) )
	{
		return true;
	}

	// sub-categories
	if (ms_DisableInteriorLights && (flags & LIGHTFLAG_INTERIOR_ONLY))
	{
		return true;
	}

	if (ms_DisableExteriorLights && (flags & LIGHTFLAG_EXTERIOR_ONLY))
	{
		return true;
	}

	if (ms_DisableShadowLights && (flags & (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS) ))
	{
		return true;
	}

	if (ms_DisableFillerLights && (flags & LIGHTFLAG_NO_SPECULAR))
	{
		return true;
	}

	if (ms_DisableVolumetricLights && (flags & LIGHTFLAG_DRAW_VOLUME))
	{
		return true;
	}

	if (ms_DisableTexturedLights && (flags & LIGHTFLAG_TEXTURE_PROJECTION))
	{
		return true;
	}

	if (ms_DisableNormalLights)
	{
		if( (flags & (LIGHTFLAG_VEHICLE | LIGHTFLAG_FX)) == 0 )
			return true;
	}

	return false;
}





//////////////////////////////////////////////////////////////////////////
//
bool BudgetDisplay::ms_EntriesToggleAll = false;

PARAM(budget, "[debug] Display budget");

//////////////////////////////////////////////////////////////////////////
//
BudgetDisplay::BudgetDisplay()
{
	m_IsActive = false;
}

//////////////////////////////////////////////////////////////////////////
//
BudgetDisplay::~BudgetDisplay()
{
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::InitClass()
{
	Reset();
	if(PARAM_budget.Get())
	{
		m_IsActive = true;
	}

	// if there's an xml with budget data load it
	ASSET.PushFolder("common:/non_final");

	bool bFileExists = ASSET.Exists("budget", "xml");
	bool bFileParsedOk = false;

	if (bFileExists)
	{
		bFileParsedOk = PARSER.LoadObject("budget", "xml", *this);
	}

	if (bFileParsedOk == false)
	{
		m_Entries.Reserve(BE_COUNT);
		m_Entries.Resize(BE_COUNT);
		u32 idx = 0;

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//						ENTRY ID				TIMEBAR NAME						DISPLAY NAME	TIME (ms)	BUDGET (ms)		WINDOW GROUP		ENABLED
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		m_Entries[idx++].Set(	BE_DIR_AMB_REFL,		"Directional/Ambient/Reflections",	"Directional",	0.0f, 		7.0f, 			WG_MAIN_TIMINGS,	true );
		m_Entries[idx++].Set(	BE_SCENE_LIGHTS,		"Scene Lights",						NULL,			0.0f, 		5.0f, 			WG_MAIN_TIMINGS,	true );
		m_Entries[idx++].Set(	BE_LODLIGHTS,			"LOD Lights",						NULL,			0.0f, 		1.0f, 			WG_MAIN_TIMINGS,	true );
		m_Entries[idx++].Set(	BE_TOTAL_LIGHTS,		"TOTAL",							NULL,			0.0f, 		12.0f,			WG_MAIN_TIMINGS,	true );
		m_Entries[idx++].Set(	BE_CASCADE_SHADOWS,		"Cascade Shadows",					NULL,			0.0f, 		5.0f, 			WG_SHADOW_TIMINGS,	true );
		m_Entries[idx++].Set(	BE_LOCAL_SHADOWS,		"Local Shadows",					NULL,			0.0f, 		5.0f, 			WG_SHADOW_TIMINGS,	true );
		m_Entries[idx++].Set(	BE_AMBIENT_VOLUMES,		"Ambient volumes",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_AMBIENTLIGHTS,		"Ambient Lights",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_CORONAS,				"Coronas",							NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_ALPHA,				"Alpha Bucket",						NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_SKIN_LIGHTING,		"Skin lighting",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_FOLIAGE_LIGHTING,	"Foliage lighting",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_VISUAL_FX,			"Visual Effects",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_SSA,					"SSA",								NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_POSTFX,				"PostFX",							NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_DEPTHFX,				"DepthFX",							NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_EMISSIVE,			"Emissive Bucket",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_WATER,				"Water Bucket",						NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_SKY,					"Sky",								NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_DEPTH_RESOLVE,		"Depth Resolve",					NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_VOLUMETRIC_FX,		"Volumetric Effects",				NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_DISPLACEMENT,		"Displacement Bucket",				NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);
		m_Entries[idx++].Set(	BE_DEBUG,				"Debug",							NULL,			0.0f, 		1.0f, 			WG_MISC_TIMINGS,	false);

		Assertf(idx == BE_COUNT, "BudgetDisplay::InitClass: entry count mismatch in budget");
	}

	m_LightInfo.Init();

	ASSET.PopFolder();

}

//////////////////////////////////////////////////////////////////////////
//
void LightsDebugInfo::Init()
{
	// Bucket for standard lights count
	BudgetBucket& standardBucket = buckets[BB_STANDARD_LIGHTS];
	standardBucket.SetName("Standard Lights");

	standardBucket.AddEntry(BBC_NORMAL_LIGHT_COUNT, 			"Standard"			);
	standardBucket.AddEntry(BBC_SHADOW_LIGHT_COUNT, 			"Shadow"			);
	standardBucket.AddEntry(BBC_FILLER_LIGHT_COUNT, 			"No Specular"		);
	standardBucket.AddEntry(BBC_TEXTURED_LIGHT_COUNT,			"Textured"			);
	standardBucket.AddEntry(BBC_VOLUMETRIC_LIGHT_COUNT,			"Volumetric"		);

	// Bucket for cutscene lights count
	BudgetBucket& cutsceneBucket = buckets[BB_CUTSCENE_LIGHTS];
	cutsceneBucket.SetName("Cutscene Lights");

	cutsceneBucket.AddEntry(BBC_NORMAL_LIGHT_COUNT, 			"Normal lights"				);
	cutsceneBucket.AddEntry(BBC_SHADOW_LIGHT_COUNT, 			"Shadow lights"				);
	cutsceneBucket.AddEntry(BBC_FILLER_LIGHT_COUNT, 			"No Specular lights"		);
	cutsceneBucket.AddEntry(BBC_TEXTURED_LIGHT_COUNT,			"Textured lights"			);
	cutsceneBucket.AddEntry(BBC_VOLUMETRIC_LIGHT_COUNT,			"Volumetric lights"			);


	// Bucket for misc lights count
	BudgetBucket& miscBucket = buckets[BB_MISC_LIGHTS];
	miscBucket.bEnable = false;
	miscBucket.SetName("Misc");

	miscBucket.AddEntry(	BBC_INTERIOR_LIGHT_COUNT, 			"Interior lights"			);
	miscBucket.AddEntry(	BBC_EXTERIOR_LIGHT_COUNT, 			"Exterior lights"			);
	miscBucket.AddEntry(	BBC_LOD_LIGHT_COUNT,				"LOD lights"				);
	miscBucket.AddEntry(	BBC_AMBIENT_LIGHT_COUNT,			"Ambient lights"			);
	miscBucket.AddEntry(	BBC_AMBIENT_OCCLUDER_LIGHT_COUNT,	"Ambient occluder lights"	);
	miscBucket.AddEntry(	BBC_FX_LIGHT_COUNT,					"FX lights"					);
	miscBucket.AddEntry(	BBC_VEHICLE_LIGHT_COUNT,			"Vehicle lights"			);
	miscBucket.AddEntry(	BBC_TOTAL_MISC_LIGHT_COUNT,			"Total lights"				);
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::SaveSettingsToFile()
{
	ASSET.PushFolder("common:/non_final");
	PARSER.SaveObject("budget", "xml", this, parManager::XML);
	ASSET.PopFolder();
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::LoadSettingsFromFile()
{
	ASSET.PushFolder("common:/non_final");
	PARSER.LoadObject("budget", "xml", *this);
	ASSET.PopFolder();
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::ShutdownClass()
{
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetBucket::AddEntry(BudgetBucketEntryId id, const char* pDisplayName)
{
	BudgetBucketEntry entry;
	entry.pName = pDisplayName;
	entry.id = (u32)id;
	entry.uVal = 0;
	entries.Push(entry);
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetBucket::SetEntry(BudgetBucketEntryId id, u32 val)
{
	for (int i = 0; i < entries.GetCount(); i++)
	{
		if (entries[i].id == (u32)id)
		{
			entries[i].uVal = val;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetBucket::SetEntry(BudgetBucketEntryId id, float val)
{
	for (int i = 0; i < entries.GetCount(); i++)
	{
		if (entries[i].id == (u32)id)
		{
			entries[i].fVal = val;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetBucket::IncrEntry(BudgetBucketEntryId id)
{
	for (int i = 0; i < entries.GetCount(); i++)
	{
		if (entries[i].id == (u32)id)
		{
			entries[i].uVal++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
u32 BudgetBucket::GetEntryValue(BudgetBucketEntryId id)
{
	for (int i = 0; i < entries.GetCount(); i++)
	{
		if (entries[i].id == (u32)id)
		{
			return entries[i].uVal;
		}
	}

	return 0U;
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetBucket::GetEntryValuef(BudgetBucketEntryId id)
{
	for (int i = 0; i < entries.GetCount(); i++)
	{
		if (entries[i].id == (u32)id)
		{
			return entries[i].fVal;
		}
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetBucket::Reset()
{
	for (int i = 0; i < entries.GetCount(); i++) 
	{
		entries[i].uVal = 0U;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetBucket::SetName(const char* _pName)
{
	if (_pName != NULL && strlen(_pName) < sizeof(pName))
	{
		strcpy(&pName[0], _pName);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void LightsDebugInfo::Reset()
{


	buckets[BB_STANDARD_LIGHTS].Reset();
	buckets[BB_CUTSCENE_LIGHTS].Reset();
	buckets[BB_MISC_LIGHTS].Reset();

}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::UpdateLightDebugInfo(CLightSource* lights, u32 numLights, u32 numLodLights, u32 numAmbientLights)
{
	if (m_IsActive == false)
	{
		return;
	}

	bool bAllLightsDisabled			= BudgetDisplay::ms_DisableAllLights;
	bool bCutsceneLightsDisabled	= BudgetDisplay::ms_DisableCutsceneLights;
	bool bInteriorLightsDisabled	= BudgetDisplay::ms_DisableInteriorLights;
	bool bExteriorLightsDisabled	= BudgetDisplay::ms_DisableExteriorLights;
	bool bShadowLightsDisabled		= BudgetDisplay::ms_DisableShadowLights;
	bool bFillerLightsDisabled		= BudgetDisplay::ms_DisableFillerLights;
	bool bVolumetricLightsDisabled	= BudgetDisplay::ms_DisableVolumetricLights;
	bool bTextureLightsDisabled		= BudgetDisplay::ms_DisableTexturedLights;
	bool bStandardLightsDisabled	= BudgetDisplay::ms_DisableStandardLights;	
	bool bNormalLightsDisabled		= BudgetDisplay::ms_DisableNormalLights;

	// reset entries
	m_LightInfo.Reset();
	m_LightInfo.numLights = numLights;
	m_LightInfo.buckets[BB_MISC_LIGHTS].SetEntry(BBC_LOD_LIGHT_COUNT, numLodLights);
	m_LightInfo.buckets[BB_MISC_LIGHTS].SetEntry(BBC_AMBIENT_LIGHT_COUNT, numAmbientLights);

	// gather timings
	for (int i = 0; i < BE_COUNT; i++)
	{
		m_Entries[i].time = 0.0f;
#if RAGE_TIMEBARS
		if (m_Entries[i].bEnable)
		{
			int count;
			::rage::g_pfTB.GetGpuBar().GetFunctionTotals(m_Entries[i].pName, count, m_Entries[i].time);
		}
#endif
	}
	m_LightInfo.totalLightsTime = GetEntryTime(BE_DIR_AMB_REFL) + GetEntryTime(BE_CASCADE_SHADOWS) + GetEntryTime(BE_LOCAL_SHADOWS);

	// total lights time is directional + scene lights + lod lights
	BudgetEntry* pEntry = FindEntry(BE_TOTAL_LIGHTS);
	pEntry->time = GetEntryTime(BE_DIR_AMB_REFL)+ GetEntryTime(BE_SCENE_LIGHTS) + GetEntryTime(BE_LODLIGHTS) + GetEntryTime(BE_AMBIENTLIGHTS);

	// gather misc. info
	for (int i = 0; i < numLights; i++)
	{
		bool bIsCutsceneLight = (bool)((lights[i].GetFlags()&(LIGHTFLAG_CUTSCENE)) != 0);
		BudgetBucketType curBucket = (bIsCutsceneLight ? BB_CUTSCENE_LIGHTS : BB_STANDARD_LIGHTS);

		bool bCurBucketEnabled = (bIsCutsceneLight ? bCutsceneLightsDisabled == false : bStandardLightsDisabled == false);

		bool isNormalLight = true;

		if (bCurBucketEnabled && CParaboloidShadow::GetLightShadowIndex(lights[i],true)!=-1 && bShadowLightsDisabled == false)
		{
			m_LightInfo.buckets[curBucket].IncrEntry(BBC_SHADOW_LIGHT_COUNT);
			isNormalLight = false;
		}

		if (bCurBucketEnabled && lights[i].IsNoSpecular() && bFillerLightsDisabled == false)
		{
			m_LightInfo.buckets[curBucket].IncrEntry(BBC_FILLER_LIGHT_COUNT);
			isNormalLight = false;
		}

		if (bCurBucketEnabled && lights[i].GetFlag(LIGHTFLAG_TEXTURE_PROJECTION) && bTextureLightsDisabled == false)
		{
			m_LightInfo.buckets[curBucket].IncrEntry(BBC_TEXTURED_LIGHT_COUNT);
			isNormalLight = false;
		}

		if (bCurBucketEnabled && lights[i].GetFlag(LIGHTFLAG_DRAW_VOLUME) && bVolumetricLightsDisabled == false)
		{
			m_LightInfo.buckets[curBucket].IncrEntry(BBC_VOLUMETRIC_LIGHT_COUNT);
			isNormalLight = false;
		}

		if (lights[i].GetFlags()&(LIGHTFLAG_VEHICLE) && bAllLightsDisabled == false)
		{
			m_LightInfo.buckets[BB_MISC_LIGHTS].IncrEntry(BBC_VEHICLE_LIGHT_COUNT);
		}
		
		if (lights[i].GetFlags()&(LIGHTFLAG_FX) && bAllLightsDisabled == false)
		{
			m_LightInfo.buckets[BB_MISC_LIGHTS].IncrEntry(BBC_FX_LIGHT_COUNT);
		}
		
		if (bCurBucketEnabled && lights[i].GetType() != LIGHT_TYPE_AO_VOLUME && bNormalLightsDisabled == false && isNormalLight)
		{
			m_LightInfo.buckets[curBucket].IncrEntry(BBC_NORMAL_LIGHT_COUNT);
		}

		if (lights[i].GetFlags()&(LIGHTFLAG_INTERIOR_ONLY) && bInteriorLightsDisabled == false)
		{
			m_LightInfo.buckets[BB_MISC_LIGHTS].IncrEntry(BBC_INTERIOR_LIGHT_COUNT);
		}
		
		if (lights[i].GetFlags()&(LIGHTFLAG_EXTERIOR_ONLY) && bExteriorLightsDisabled == false)
		{
			m_LightInfo.buckets[BB_MISC_LIGHTS].IncrEntry(BBC_EXTERIOR_LIGHT_COUNT);
		}

	}

	BudgetBucket& miscBucket = m_LightInfo.buckets[BB_MISC_LIGHTS];
	miscBucket.SetEntry(BBC_TOTAL_MISC_LIGHT_COUNT, m_LightInfo.numLights);

}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::ToggleAllEntries()
{
	for (int i=0; i<BE_COUNT; i++)
	{
		SBudgetDisplay::GetInstance().m_Entries[i].bEnable = ms_EntriesToggleAll;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::ColouriseLight(CLightSource* pLight)
{
	if(ms_ColouriseLights == false || m_IsActive==false || pLight == NULL)
	{
		return;
	}
	
	u32 lightFlags = pLight->GetFlags();

	bool bIsShadowLight		= ((lightFlags & (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS)) != 0);
	bool bIsFillerLight		= ((lightFlags & (LIGHTFLAG_NO_SPECULAR)) != 0);

	if (bIsShadowLight)
	{
		pLight->SetColor(VEC3V_TO_VECTOR3(ms_ShadowLightsCol.GetRGB()));	
	}
	else if (bIsFillerLight)
	{
		pLight->SetColor(VEC3V_TO_VECTOR3(ms_FillerLightsCol.GetRGB()));			
	}
	else
	{
		pLight->SetColor(VEC3V_TO_VECTOR3(ms_OtherLightsCol.GetRGB()));	
	}

}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::Draw()
{
	if(m_IsActive==false)
	{
		return;
	}

	if (ms_ShowPieChart)
	{
		DrawPieChartView();
	}

	float x = m_DrawInfo.xOffset;
	float y = m_DrawInfo.yOffset;
	float lastY = 0.0f;
	float scale = m_DrawInfo.textScale;
	const float vTextPadding = GetFontHeight()*0.5f;
	const float offset = (GetFontHeight()+vTextPadding)*scale;
	const float maxWindowWidth = 46.0f;

	lastY = DrawColouriseModeInfo(x, y, scale, offset);
	y = lastY;

	// draw standard/cutscene lights count
	lastY = DrawLightsCount(x, y, offset, scale, maxWindowWidth);
	y = lastY + offset;

	// draw timings
	if (ms_ShowMainTimes)
	{
		lastY = DrawTimingsWindow(WG_MAIN_TIMINGS, "Main Timings", x, y, offset, scale, maxWindowWidth, true, BE_TOTAL_LIGHTS);
		y = lastY + offset;
	}

	// draw shadow timings
	if (ms_ShowShadowTimes)
	{
		lastY = DrawTimingsWindow(WG_SHADOW_TIMINGS, "Shadow Timings", x, y, offset, scale, maxWindowWidth, true);
		y = lastY + offset;
	}

	// draw misc timings
	if (ms_ShowMiscTimes)
	{
		lastY = DrawTimingsWindow(WG_MISC_TIMINGS, "Misc. Timings", x, y, offset, scale, maxWindowWidth);
		y = lastY + offset;
	}
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::DrawColouriseModeInfo(float x, float y, float scale, float offset)
{
	if (ms_ColouriseLights == false)
	{
		return y;
	}

	const Color32 white = Color32(255,255,255,255);
	
	static float fScale = 0.5f;
	fwBox2D rect;
	rect.x0 = x;
	rect.x1 = x+offset;
	rect.y0 = y-offset*fScale;
	rect.y1 = rect.y0+offset;

	DrawRect(rect, ms_ShadowLightsCol);
	x+=offset*1.25f;
	DrawText(x, y, scale, white, "Shadow Lights");

	x+=offset*12;
	rect.x0 = x; rect.x1 = x+offset;
	DrawRect(rect, ms_FillerLightsCol);
	x+=offset*1.25f;
	DrawText(x, y, scale, white, "Filler Lights");

	x+=offset*12;
	rect.x0 = x; rect.x1 = x+offset;
	DrawRect(rect, ms_OtherLightsCol);
	x+=offset*1.25f;
	DrawText(x, y, scale, white, "Other Lights");

	return y+offset*2;
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::DrawLightsCount(float x, float y, float offset, float scale, float maxLineWidth) 
{
	const Color32 col(240, 240, 240, 255);

	float opacity = ms_overrideBackgroundOpacity ? ms_bgOpacityOverrideValue : m_DrawInfo.bgOpacity;
	const Color32 bgCol0 = Color32(0.2f, 0.2f, 0.2f, opacity);
	const Color32 bgCol1 = Color32(0.15f, 0.15f, 0.15f, opacity);
	const Color32 bgColTitle = Color32(0.3f, 0.3f, 0.5, opacity*2.0f);
	const Color32 white = Color32(255,255,255,255);
	float lastY = y;

	// draw cutscene info
	const bool bCutsceneRunning = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());

	if (IsAutoBucketDisplay())
	{
		m_LightInfo.buckets[BB_CUTSCENE_LIGHTS].bEnable = bCutsceneRunning;
	}

	if (bCutsceneRunning)
	{
		BeginDrawTextLines(bgColTitle, bgCol0, bgCol1, white, col, x, y, offset, scale, maxLineWidth*1.33f);
			DrawTextLine("CUTSCENE INFO");
			DrawEmptyLine();
			DrawTextLine("Cutscene:          %-23s", CutSceneManager::GetInstance()->GetCutsceneName());
			DrawEmptyLine();
			atArray<cutsEntity*> pEntityList; 
			CutSceneManager::GetInstance()->GetEntityByType(CUTSCENE_CAMERA_GAME_ENITITY, pEntityList);
			if (pEntityList.GetCount() > 0)
			{
				const char* pCutName = 	static_cast<CCutSceneCameraEntity*>(pEntityList[0])->GetCameraCutName();
				DrawTextLine("Cut name:          %-23s", pCutName);
				DrawEmptyLine();
			}
		lastY = EndDrawTextLines();
		y = lastY + offset;
	}

	if (m_LightInfo.buckets[BB_CUTSCENE_LIGHTS].bEnable)
	{
		const char* pLightsCountFormatStr = "%-32s  %3d   %3d   %3d";

		BeginDrawTextLines(bgColTitle, bgCol0, bgCol1, white, col, x, y, offset, scale, 52.0f);
		DrawTextLine("%-32s  %3s   %3s   %3s", "Lights", "S", "CS", "Total");

		DrawEmptyLine();

		int entryCount = m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries.GetCount();
		for (int j = 0; j < entryCount; j++)
		{
			u32 total = m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries[j].uVal + 
				m_LightInfo.buckets[BB_CUTSCENE_LIGHTS].entries[j].uVal;

			DrawTextLine(pLightsCountFormatStr, 
				m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries[j].pName, 
				m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries[j].uVal,
				m_LightInfo.buckets[BB_CUTSCENE_LIGHTS].entries[j].uVal,
				total);

			DrawEmptyLine();
		}
	}
	else
	{
		const char* pLightsCountFormatStr = "%-32s  %3d";

		BeginDrawTextLines(bgColTitle, bgCol0, bgCol1, white, col, x, y, offset, scale, 46.0f);
		DrawTextLine("%-32s", "Lights");

		DrawEmptyLine();

		int entryCount = m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries.GetCount();
		for (int j = 0; j < entryCount; j++)
		{
			DrawTextLine(pLightsCountFormatStr, 
				m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries[j].pName, 
				m_LightInfo.buckets[BB_STANDARD_LIGHTS].entries[j].uVal);

			DrawEmptyLine();
		}
	}

	lastY = EndDrawTextLines();
	y = lastY+offset;

	return lastY;
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::DrawTimingsWindow(BudgetWindowGroupId groupId, const char* pName, float x, float y, float offset, float scale, float maxLineWidth, bool bDrawTotals, BudgetEntryId entryForTotals) 
{

	float opacity = ms_overrideBackgroundOpacity ? ms_bgOpacityOverrideValue : m_DrawInfo.bgOpacity;
	const Color32 col(240, 240, 240, 255);
	const Color32 bgCol0 = Color32(0.2f, 0.2f, 0.2f, opacity);
	const Color32 bgCol1 = Color32(0.15f, 0.15f, 0.15f, opacity);
	const Color32 bgColTitle = Color32(0.3f, 0.3f, 0.5, opacity*2.0f);
	const Color32 white = Color32(255,255,255,255);

	float totalForWindow = 0.0f;

	bool bDrawOutline = true;
	if (fwTimer::GetTimeInMilliseconds() - ms_BlinkCounter > 500)
	{
		bDrawOutline = false;
		ms_BlinkCounter = fwTimer::GetTimeInMilliseconds();
	}

	// draw timings
	const char* pLightsTimingsFormatStr = "%-32s: %2.1f ms";
	BeginDrawTextLines(bgColTitle, bgCol0, bgCol1, white, col, x, y, offset, scale, maxLineWidth);
		DrawTextLine(pName);
		DrawEmptyLine();
		int lineCount = 0;
		for (int i = 0; i < BE_COUNT; i++)
		{
			const BudgetEntry* pEntry = FindEntry((BudgetEntryId)i);

			if (pEntry->bEnable && pEntry->windowGroupId == groupId && pEntry->id != entryForTotals)
			{
				bool bIsOverBudget;
				Color32 col = GetColourForEntry(*pEntry, bIsOverBudget);
				SetColourOverride(col);

				if (bIsOverBudget && bDrawOutline)
				{
					DrawOutline(Color32(255, 0, 0, 255));
				}

				DrawTextLine(pLightsTimingsFormatStr, (pEntry->pDisplayName[0] == 0 ? pEntry->pName : pEntry->pDisplayName), pEntry->time);
				DrawEmptyLine();
				lineCount++;

				totalForWindow += pEntry->time;
			}
		}
		DrawEmptyLine();
		if (bDrawTotals)
		{
			if (entryForTotals == BE_INVALID)
			{
				DrawTextLine(pLightsTimingsFormatStr, "Total", totalForWindow);
				DrawEmptyLine();
			}
			else
			{
				const BudgetEntry* pEntry = FindEntry(entryForTotals);
				
				bool bIsOverBudget;
				Color32 col = GetColourForEntry(*pEntry, bIsOverBudget);
				SetColourOverride(col);

				if (bIsOverBudget && bDrawOutline)
				{
					DrawOutline(Color32(255, 0, 0, 255));
				}

				DrawTextLine(pLightsTimingsFormatStr, "Total", pEntry->time);
				DrawEmptyLine();
			}
		}
	float currentYOffset = EndDrawTextLines();

	return currentYOffset;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::Reset()
{
	m_DrawInfo.Reset();
	m_IsActive = false;
	ms_EntriesToggleAll = false;
}


//////////////////////////////////////////////////////////////////////////
//
void ToggleVisibilityFlag(void* flag) {
	gVpMan.ToggleVisibilityFlagForAllPhases( static_cast< u16 >(reinterpret_cast< size_t >( flag )) );
}

//////////////////////////////////////////////////////////////////////////
//
BudgetEntry* BudgetDisplay::FindEntry(BudgetEntryId id)
{
	for (int i=0; i<BE_COUNT; i++)
	{
		if (m_Entries[i].id == id) 
		{
			return &m_Entries[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
const BudgetEntry* BudgetDisplay::FindEntry(BudgetEntryId id) const
{
	for (int i=0; i<BE_COUNT; i++)
	{
		if (m_Entries[i].id == id) 
		{
			return &m_Entries[i];
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::GetEntryTime(BudgetEntryId id) const
{
	const BudgetEntry* pEntry = FindEntry(id);
	
	if (pEntry != NULL) 
	{
		return pEntry->time;
	}

	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
//
Color32	BudgetDisplay::GetColourForEntry(const BudgetEntry& entry, bool& bIsOverBudget) const
{
	bIsOverBudget = false;

	// go full red when over 5% of budget
	float t = entry.time/(entry.budgetTime);

	if (t <= 1.0f)
	{
		return Color32(t*t*t, 1.0f, 0.0f, 1.0f);
	}

	bIsOverBudget = true;
	t = (entry.budgetTime*t)/(entry.budgetTime+entry.budgetTime*0.15f);
	t = rage::Min(1.0f, t);
	return Color32(1.0f, 1.0f-t*t*t, 0.0f, 1.0f);


}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::AddWidgets(bkBank* pBank)
{

	pBank->PushGroup("Budget");

		pBank->AddToggle("Draw Budget", &m_IsActive );
		pBank->AddToggle("Show Pie Chart", &ms_ShowPieChart);
		pBank->AddToggle("Smooth Values", &ms_PieChartSmoothValues);

		// Pie Chart
		pBank->PushGroup("Pie Chart");
			pBank->AddVector("Position", &ms_PieChartPos, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Size", &ms_PieChartSize, 0.05f, 0.5f, 0.01f);
		pBank->PopGroup();

		// Settings
		pBank->PushGroup("Settings", false);
			pBank->AddToggle("Show Standard Lights Info",  &m_LightInfo.buckets[BB_STANDARD_LIGHTS].bEnable);
			pBank->AddToggle("Show Cutscene Lights Info",  &m_LightInfo.buckets[BB_CUTSCENE_LIGHTS].bEnable);
			pBank->AddToggle("Show Lighting Timing",  &ms_ShowMainTimes);
			pBank->AddSeparator();
			pBank->AddToggle("Disable All Lights",				&ms_DisableAllLights);
			pBank->AddToggle("Disable Non-Cutscene Lights",		&ms_DisableStandardLights);
			pBank->AddToggle("Disable Cutscene Lights",			&ms_DisableCutsceneLights);			
			pBank->AddSeparator();
			pBank->AddToggle("Disable Interior Only Lights",	&ms_DisableInteriorLights);
			pBank->AddToggle("Disable Exterior Only Lights",	&ms_DisableExteriorLights);		
			pBank->AddSeparator();
			pBank->AddToggle("Disable Shadow Lights",			&ms_DisableShadowLights);
			pBank->AddToggle("Disable Filler Lights",			&ms_DisableFillerLights);
			pBank->AddToggle("Disable Volumetric Lights",		&ms_DisableVolumetricLights);
			pBank->AddToggle("Disable Textured Lights",			&ms_DisableTexturedLights);
			pBank->AddToggle("Disable Normal Lights",			&ms_DisableNormalLights);
			pBank->AddSeparator();
			pBank->AddToggle("Colour Lights",					&ms_ColouriseLights);
			pBank->AddSeparator();
			pBank->AddToggle("Show Misc Lights Info",  &m_LightInfo.buckets[BB_MISC_LIGHTS].bEnable);
			pBank->AddToggle("Show Shadows Timing",  &ms_ShowShadowTimes);
			pBank->AddToggle("Show Misc. Timing",  &ms_ShowMiscTimes);			
		pBank->PopGroup();

		// Display windows
		pBank->PushGroup("Display Windows", false);
			pBank->AddSlider("Text Position X:", &m_DrawInfo.xOffset, 0.0f, 2000.0f, 1.0f);
			pBank->AddSlider("Text Position Y:", &m_DrawInfo.yOffset, 0.0f, 2000.0f, 1.0f);
			pBank->AddSlider("Text Scale:", &m_DrawInfo.textScale, 1.0f, 7.0f, 1.0f);
			pBank->AddToggle("Override Background Opacity", &ms_overrideBackgroundOpacity);
			pBank->AddSlider("Background Opacity:",  &ms_bgOpacityOverrideValue,  0.0f, 1.0f, 0.1f);			
		pBank->PopGroup();

		// Timers
		pBank->PushGroup("Timers", false);
			pBank->AddButton("Save Settings To File", datCallback(MFA(BudgetDisplay::SaveSettingsToFile), (datBase*)this));
			pBank->AddButton("Load Settings From File", datCallback(MFA(BudgetDisplay::LoadSettingsFromFile), (datBase*)this));
			pBank->AddSeparator("");
			pBank->AddToggle("Toggle All",  &ms_EntriesToggleAll, datCallback(BudgetDisplay::ToggleAllEntries));
			for (int i=0; i<BE_COUNT; i++)
			{
				pBank->AddToggle(m_Entries[i].pName, &(m_Entries[i].bEnable));
				pBank->AddSlider("Budget (ms):", &m_Entries[i].budgetTime, 0.0f, 30.0f, 0.01f);
				pBank->AddSeparator("");
			}
		pBank->PopGroup();

	pBank->PopGroup();

}

//////////////////////////////////////////////////////////////////////////
//
Vector2 BudgetDisplay::ToScreenCoords(float x, float y) 
{
	return Vector2((float)(x)/grcViewport::GetDefaultScreen()->GetWidth(), (float)(y)/grcViewport::GetDefaultScreen()->GetHeight());
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawRect(const fwBox2D& rect, Color32 colour, bool bSolid)
{
	grcDebugDraw::RectAxisAligned(ToScreenCoords(rect.x0, rect.y0), ToScreenCoords(rect.x1, rect.y1), colour, bSolid);
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawText(float x, float y, float scale, Color32 colour, const char * fmt, ...)
{
	char text[512];
	if(fmt)
	{
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf(text, fmt, argptr);
		va_end(argptr);
	}
	else
	{
		text[0] = 0;
	}

	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());
	grcDebugDraw::Text(Vector2(x, y), DD_ePCS_Pixels, colour, text, false, scale, scale);
	grcDebugDraw::TextFontPop();
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::GetFontHeight()
{
	float h = (float)grcSetup::GetFixedWidthFont()->GetHeight();
	return h;
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::GetFontWidth()
{
	float w = (float)grcSetup::GetFixedWidthFont()->GetWidth();
	return w;
}

//////////////////////////////////////////////////////////////////////////
//
float	BudgetDisplay::ms_TextPosX	= 0.0f;
float	BudgetDisplay::ms_TextPosY	= 0.0f;
float	BudgetDisplay::ms_TextOffsetV = 0.0f;
float	BudgetDisplay::ms_TextScale	= 0.0f;
float	BudgetDisplay::ms_TextMaxWidth = 0.0f;
u32		BudgetDisplay::ms_LineCount = 0;

Color32	BudgetDisplay::ms_BgCol[2];
Color32	BudgetDisplay::ms_TextCol;
Color32 BudgetDisplay::ms_TextFirstLineCol;
Color32 BudgetDisplay::ms_BgColFirstLine;
Color32 BudgetDisplay::ms_TextColOverride;
Color32 BudgetDisplay::ms_OutlineCol;
bool	BudgetDisplay::ms_UseTextColOverride = false;

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::SetColourOverride(Color32 colourOverride)
{
	ms_UseTextColOverride = true;
	ms_TextColOverride = colourOverride;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::BeginDrawTextLines(Color32 bgColFirstLine, Color32 bgCol0, Color32 bgCol1, Color32 textFirstLineCol, Color32 textCol, float x, float y, float offset, float scale, float textMaxWidth)
{
	ms_BgColFirstLine = bgColFirstLine;
	ms_BgCol[0] = bgCol0;
	ms_BgCol[1] = bgCol1;
	ms_TextCol = textCol;
	ms_TextFirstLineCol = textFirstLineCol;
	ms_TextPosX = x;
	ms_TextPosY = y;
	ms_TextOffsetV = offset;
	ms_TextScale = scale;
	ms_TextMaxWidth = textMaxWidth;
	ms_LineCount = 0;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawTextLine(const char * fmt, ...)
{
	char text[512];
	if(fmt)
	{
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf(text, fmt, argptr);
		va_end(argptr);
	}
	else
	{
		text[0] = 0;
	}


	// draw background
	float rectW = GetFontWidth()*ms_TextScale*ms_TextMaxWidth;
	const float rectMargin = GetFontHeight()*ms_TextScale;
	fwBox2D box(ms_TextPosX-rectMargin, 0.0f, ms_TextPosX+rectW+rectMargin, 0.0f);
	const float vCenter = ms_TextPosY+GetFontHeight()*ms_TextScale*0.5f;
	box.y0 = vCenter-ms_TextOffsetV*0.5f;
	box.y1 = vCenter+ms_TextOffsetV*0.5f;

	const Color32& bgCol = (ms_LineCount == 0) ? ms_BgColFirstLine : ms_BgCol[ms_LineCount & 1];
	DrawRect(box, bgCol);

	if (ms_DrawOutline)
	{
		DrawRect(box, ms_OutlineCol, false);
		ms_DrawOutline = false;
	}

	// draw text
	grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());
		if (ms_UseTextColOverride == false)
		{
			grcDebugDraw::Text(Vector2(ms_TextPosX, ms_TextPosY), DD_ePCS_Pixels, (ms_LineCount == 0) ?  ms_TextFirstLineCol : ms_TextCol, text, false, ms_TextScale, ms_TextScale);
		}
		else
		{
			grcDebugDraw::Text(Vector2(ms_TextPosX, ms_TextPosY), DD_ePCS_Pixels, ms_TextColOverride, text, false, ms_TextScale, ms_TextScale);
			ms_UseTextColOverride = false;
		}
	grcDebugDraw::TextFontPop();

	ms_TextPosY += ms_TextOffsetV;
	ms_LineCount++;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawEmptyLine()
{
	// draw background
	float rectW = GetFontWidth()*ms_TextScale*ms_TextMaxWidth;
	const float rectMargin = GetFontHeight()*ms_TextScale;
	fwBox2D box(ms_TextPosX-rectMargin, 0.0f, ms_TextPosX+rectW+rectMargin, 0.0f);
	const float vCenter = ms_TextPosY+GetFontHeight()*ms_TextScale*0.5f;
	box.y0 = vCenter-ms_TextOffsetV*0.5f;
	box.y1 = vCenter+ms_TextOffsetV*0.5f;

	const Color32& bgCol = (ms_LineCount == 0) ? ms_BgColFirstLine : ms_BgCol[ms_LineCount & 1];
	DrawRect(box, bgCol);

	ms_TextPosY += ms_TextOffsetV;
	ms_LineCount++;
}

//////////////////////////////////////////////////////////////////////////
//
void BudgetDisplay::DrawOutline(Color32 col)
{
	ms_DrawOutline = true;
	ms_OutlineCol = col;
}

//////////////////////////////////////////////////////////////////////////
//
float BudgetDisplay::EndDrawTextLines()
{
	float retYPos = ms_TextPosY;
	ms_TextPosX	= 0.0f;
	ms_TextPosY	= 0.0f;
	ms_TextOffsetV = 0.0f;
	ms_TextScale	= 0.0f;

	return retYPos;
}

#endif //__BANK



