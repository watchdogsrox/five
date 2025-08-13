//
// debug/budgetdisplay.h
//
// Copyright (C) 1999-2010 Rockstar Games. All Rights Reserved.
//

#ifndef __INC_BUDGETDISPLAY_H_
#define __INC_BUDGETDISPLAY_H_

#include "atl/queue.h"
#include "atl/singleton.h"
#include "parser/macros.h"
#include "fwmaths/Rect.h"

#include "vector/color32.h"

#if __BANK

#define DB_DEFAULT_TEXT_OFFSET_X	(64.0f)
#define DB_DEFAULT_TEXT_OFFSET_Y	(32.0f)
#define DB_DEFAULT_TEXT_SCALE		(1.0f)
#define DB_DEFAULT_OPACITY			(1.0f)

class CLightSource;

namespace rage 
{
class bkBank;
};

enum BudgetEntryId {
	BE_TOTAL_LIGHTS = 0,
	BE_DIR_AMB_REFL,
	BE_SCENE_LIGHTS,
	BE_CASCADE_SHADOWS,
	BE_LOCAL_SHADOWS,
	BE_AMBIENT_VOLUMES,
	BE_CORONAS,
	BE_ALPHA,
	BE_SKIN_LIGHTING,
	BE_FOLIAGE_LIGHTING,
	BE_VISUAL_FX,
	BE_SSA,
	BE_POSTFX,
	BE_DEPTHFX,
	BE_EMISSIVE,
	BE_WATER,
	BE_SKY,
	BE_DEPTH_RESOLVE,
	BE_VOLUMETRIC_FX,
	BE_DISPLACEMENT,
	BE_DEBUG,
	BE_LODLIGHTS,
	BE_AMBIENTLIGHTS,
	BE_COUNT,
	BE_INVALID,
};

enum BudgetWindowGroupId {
	WG_MAIN_TIMINGS = 0,
	WG_SHADOW_TIMINGS,
	WG_MISC_TIMINGS,
	WG_COUNT
};

struct BudgetEntry {
	BudgetEntryId		id;
	char				pName[32];
	char				pDisplayName[16];
	float				time;
	float				budgetTime;
	BudgetWindowGroupId	windowGroupId;
	bool				bEnable;

	void Set(BudgetEntryId _id, const char* _pName, const char* _pDisplayName, float _time, float _budgetTime, BudgetWindowGroupId _windowGroupId, bool _bEnable);

	PAR_SIMPLE_PARSABLE;
};

enum BudgetBucketType {
	BB_STANDARD_LIGHTS	= 0,
	BB_CUTSCENE_LIGHTS	= 1,
	BB_MISC_LIGHTS		= 2,
	BB_COUNT			= 3,
};

struct BudgetBucketEntry {
	u32 id;
	const char* pName;
	union {
		float	fVal;
		u32		uVal;
	};
};

enum BudgetBucketEntryId {
	BBC_SHADOW_LIGHT_COUNT = 0,
	BBC_FILLER_LIGHT_COUNT,
	BBC_TEXTURED_LIGHT_COUNT,
	BBC_NORMAL_LIGHT_COUNT,
	BBC_VOLUMETRIC_LIGHT_COUNT,
	BBC_INTERIOR_LIGHT_COUNT,
	BBC_EXTERIOR_LIGHT_COUNT,
	BBC_LOD_LIGHT_COUNT,
	BBC_AMBIENT_LIGHT_COUNT,
	BBC_VEHICLE_LIGHT_COUNT,
	BBC_FX_LIGHT_COUNT,
	BBC_AMBIENT_OCCLUDER_LIGHT_COUNT,
	BBC_TOTAL_MISC_LIGHT_COUNT,
};

enum PieChartEntryId 
{
	PCE_POSTFX = 0,
	PCE_LIGHTING,
	PCE_SCENE_LIGHTS,
	PCE_CASCADE_SHADOWS,
	PCE_LOCAL_SHADOWS,
	PCE_ALPHA,
	PCE_COUNT
};

struct BudgetBucket {
	char 								pName[32];
	bool 								bEnable;
	atFixedArray<BudgetBucketEntry, 16>	entries;
	
	BudgetBucket() { pName[0] = 0; Reset(); };

	void 	AddEntry		(BudgetBucketEntryId id, const char* pDisplayName);
	void 	SetEntry		(BudgetBucketEntryId id, u32 val);
	void 	SetEntry		(BudgetBucketEntryId id, float val);
	u32	 	GetEntryValue	(BudgetBucketEntryId id);
	float	GetEntryValuef	(BudgetBucketEntryId id);

	void 	IncrEntry		(BudgetBucketEntryId id);

	void 	SetName(const char* _pName);
	void 	Reset();
};

struct LightsDebugInfo
{
	LightsDebugInfo() { Reset(); };

	void Reset();
	void Init();

	BudgetBucket	buckets[BB_COUNT];

	u32				numLights;
	float			totalLightsTime;
};

class BudgetDisplay
{

public:
	BudgetDisplay();
	~BudgetDisplay();

	void InitClass();
	void ShutdownClass();

	void SaveSettingsToFile();
	void LoadSettingsFromFile();

	void Draw();

	void UpdateLightDebugInfo(CLightSource* lights, u32 numLights, u32 numLodLights, u32 numAmbientLights);

	void Activate(bool bEnable)		{ m_IsActive = bEnable; };

	bool IsActive() const { return m_IsActive; }

	u32	 GetDisabledLightsMask();
	bool IsLightDisabled(const CLightSource &light);


	bool GetDisableVolumetricLights() const { return m_IsActive && ms_DisableVolumetricLights; }

	void ColouriseLight(CLightSource* pLight);

	void AddWidgets(bkBank* pBank);

	void EnableBucket(BudgetBucketType type, bool enable)	{ m_LightInfo.buckets[type].bEnable = enable; }
	bool IsBucketEnabled(BudgetBucketType type) const		{ return m_LightInfo.buckets[type].bEnable; }

	void EnableAutoBucketDisplay(bool enable)				{ ms_AutoBucketDisplay = enable; }
	bool IsAutoBucketDisplay() const						{ return ms_AutoBucketDisplay; }

	void  SetBgOpacity(float opacity)						{ m_DrawInfo.bgOpacity = opacity; }
	float GetBgOpacity() const								{ return m_DrawInfo.bgOpacity; }

	void TogglePieChart(bool bEnable)						{ ms_ShowPieChart = bEnable; }

private:

	struct DrawInfo 
	{
		DrawInfo() { Reset(); };
		void Reset() {	xOffset = DB_DEFAULT_TEXT_OFFSET_X;
						yOffset = DB_DEFAULT_TEXT_OFFSET_Y;
						textScale = DB_DEFAULT_TEXT_SCALE;
						bgOpacity = DB_DEFAULT_OPACITY; }
		float xOffset;
		float yOffset;
		float textScale;
		float bgOpacity;
	};

	const BudgetEntry*		FindEntry(BudgetEntryId id) const;
	BudgetEntry*			FindEntry(BudgetEntryId id);
	float					GetEntryTime(BudgetEntryId id) const;
	const LightsDebugInfo*	GetLightsDebugInfo() const { return &m_LightInfo; };

	Color32	GetColourForEntry(const BudgetEntry& entry, bool& bOverBudget) const;

	void Reset();

	struct PieChartData
	{

		PieChartData() { Reset(); };
		void Reset();

		void AddSample(u32 entryId, float sample);
		float Get(u32 entryId) const { return actualValue[entryId]; }
		float GetAvg(u32 entryId) const { return avgValue[entryId]; }

		float actualValue[PCE_COUNT];
		float avgValue[PCE_COUNT];

		float decayTerm;


	};

	void			DrawPieChartView();
	void			DrawPieChart(const Vector2& pos, float radius, const char** pNames, const float* pSegmentValues, const Color32* pCols, float totalMax, u32 numSegments);

	float			DrawTimingsWindow(BudgetWindowGroupId groupId, const char* pName, float x, float y, float offset, float scale, float maxLineWidth, bool bDrawTotals = false, BudgetEntryId entryForTotals = BE_INVALID);
	float			DrawLightsCount(float x, float y, float offset, float scale, float maxLineWidth);
	float			DrawColouriseModeInfo(float x, float y, float scale, float offset);

	static Vector2	ToScreenCoords(float x, float y);
	static void		DrawRect(const fwBox2D& rect, Color32 colour, bool bSolid = true);
	static void		DrawText(float x, float y, float scale, Color32 colour, const char * fmt, ...);
	static void		BeginDrawTextLines(Color32 bgColFirstLine, Color32 bgCol0, Color32 bgCol1, Color32 textFirstLineCol, Color32 textCol, float x, float y, float offset, float scale, float maxLineWidth);
	static void		SetColourOverride(Color32 colourOverride);
	static void		DrawTextLine(const char * fmt, ...);
	static void		DrawOutline(Color32 col);
	static void		DrawEmptyLine();
	static float	EndDrawTextLines();

	static float	GetFontHeight();
	static float	GetFontWidth();

	static void		ToggleAllEntries();

	static Color32			ms_BgCol[2];
	static Color32			ms_BgColFirstLine;
	static Color32			ms_TextCol;
	static Color32			ms_TextFirstLineCol;
	static Color32			ms_TextColOverride;
	static Color32			ms_OutlineCol;
	static Color32			ms_FillerLightsCol;
	static Color32			ms_ShadowLightsCol;
	static Color32			ms_OtherLightsCol;

	static bool				ms_UseTextColOverride;
	static u32				ms_LineCount;
	static u32				ms_BlinkCounter;

	static float			ms_TextPosX;
	static float			ms_TextPosY;
	static float			ms_TextOffsetV;
	static float			ms_TextScale;
	static float			ms_TextMaxWidth;
	static bool				ms_EntriesToggleAll;

	static bool				ms_ShowMainTimes;
	static bool				ms_ShowShadowTimes;
	static bool				ms_ShowMiscTimes;
	static bool				ms_DrawOutline;
	static bool				ms_AutoBucketDisplay;

	static bool				ms_DisableAllLights;
	static bool				ms_DisableInteriorLights;
	static bool				ms_DisableExteriorLights;
	static bool				ms_DisableStandardLights;
	static bool				ms_DisableCutsceneLights;
	static bool				ms_DisableShadowLights;
	static bool				ms_DisableFillerLights;
	static bool				ms_DisableVolumetricLights;
	static bool				ms_DisableNormalLights;
	static bool				ms_DisableTexturedLights;
	static bool				ms_ColouriseLights;

	static bool				ms_ShowPieChart;
	static bool				ms_PieChartSmoothValues;
	static Vector2			ms_PieChartPos;
	static float			ms_PieChartSize;
	
	static bool				ms_overrideBackgroundOpacity;
	static float			ms_bgOpacityOverrideValue;

	static PieChartData		ms_PieChartData;

	atArray<BudgetEntry>	m_Entries;
	LightsDebugInfo 		m_LightInfo;
	bool					m_IsActive;
	DrawInfo				m_DrawInfo;

	PAR_SIMPLE_PARSABLE;
};

typedef atSingleton<BudgetDisplay> SBudgetDisplay;

#define INIT_BUDGETDISPLAY										\
	do															\
	{															\
		if (!SBudgetDisplay::IsInstantiated())					\
		{														\
			SBudgetDisplay::Instantiate();						\
			SBudgetDisplay::InstanceRef().InitClass();			\
		}														\
	} while(0)													\
	//END

#define BUDGET_DRAW()																(SBudgetDisplay::GetInstance().Draw())
#define BUDGET_UPDATE_LIGHTS_INFO(lights,numLights,numLODLights,numAmbientLights)	(SBudgetDisplay::GetInstance().UpdateLightDebugInfo(lights,numLights,numLODLights,numAmbientLights))
#define BUDGET_ENABLE()																(SBudgetDisplay::GetInstance().Activate(true))
#define BUDGET_DISABLE()															(SBudgetDisplay::GetInstance().Activate(false))
#define BUDGET_TOGGLE(x)															(SBudgetDisplay::GetInstance().Activate(x))

#else //__BANK

#define INIT_BUDGETDISPLAY

#define BUDGET_DRAW()
#define BUDGET_UPDATE_LIGHTS_INFO(lights,numLights,numLODLights,numAmbientLights)
#define BUDGET_ENABLE()	
#define BUDGET_DISABLE()
#define BUDGET_TOGGLE(x)

#endif //__BANK


#endif
