#ifndef DEBUG_LIGHTS_H_
#define DEBUG_LIGHTS_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage 
#include "grmodel/shader.h"

// framework

// game 
#include "atl/map.h"
#include "scene/2dEffect.h"
#include "renderer/Lights/Lights.h"
#include "Debug/Rendering/DebugRendering.h"
#include "scene/RegdRefTypes.h"

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

class CLightSource;
class CLightEntity;

#if __BANK

// =============================================================================================== //
// HELPER CLASSES
// =============================================================================================== //

class EditLightSource
{
public:

	EditLightSource();

	static void Draw(const CLightEntity *entity, const u32 lightIndex, const bool drawExtras, const bool drawAll, const bool drawCullPlane);
	static void Change(RegdLight *entity);
	static void Update(CLightEntity *entity);
	static void ApplyChanges(CLightAttr *attr);
	static void ApplyColourChanges();

	static void AddWidgets(bkBank *bank);
	
	static CLightEntity* GetEntity();

	static s32 GetIndex() { return m_lightIndex; }
	static void SetIndex(s32 index) { m_lightIndex = index; }
	static s32* GetIndexPtr() { return &m_lightIndex; }
		
private:
	static void WidgetsToggleHoursCB();

	static CLightAttr m_clearLightAttr;
	static CLightAttr m_editLightAttr;
	static Vector3 m_editPosition;
	static Vector3 m_editDirection;
	static Vector3 m_editTangent;
	static Color32 m_editColour;
	static Color32 m_editVolOuterColour;
	static Vector3 m_editSrgbColour;

	static s32 m_lightIndex;
	
};

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

class EditLightShaft
{
public:

	EditLightShaft();

	static void Draw(const CLightEntity *entity, bool bShowMesh, bool bShowBounds, bool bDrawAll);
	static void Change(RegdLight *entity);
	static void Update(CLightEntity *entity);
	static void ApplyChanges(CLightShaftAttr *attr);

	static void CloneProperties();
	static void ResetDirection();

	static void AddWidgets(bkBank *bank);

	static CLightEntity* GetEntity();

	static s32 GetIndex() { return m_lightShaftIndex; }
	static void SetIndex(s32 index) { m_lightShaftIndex = index; }
	static s32* GetIndexPtr() { return &m_lightShaftIndex; }

private:

	static s32 m_lightShaftIndex;
	static s32 m_previousLightShaftIndex;

	static CLightShaftAttr m_clearLightShaftAttr;
	static CLightShaftAttr m_editLightShaftAttr;
};

// =============================================================================================== //
// MAIN CLASS
// =============================================================================================== //

class DebugLights : public DebugRendering
{
public:

	// Functions
	static void Init();

	static bool CalcTechAndPass(CLightSource* light, u32 &pass, grcEffectTechnique &technique, const bool useShadows);
	static void GetLightIndexRange(u32 &startIndex, u32 &endIndex);
	static void GetLightIndexRangeUpdateThread(u32 &startIndex, u32 &endIndex);
	static CEntity* GetIsolatedLightShaftEntity();
	static void Draw();

	static void InitWidgets();
	static void AddWidgetsOnDemand(bkBank *bk = NULL);
	static void Update();
	static void ApplyChanges();
	static void ApplyColourChanges();
	static void ApplyLightShaftChanges();

	static inline s32 GetLightIndexFromEntity(CLightEntity *entity, bool updateThread = true);
	static inline s32 GetLightShaftIndexFromEntity(CLightEntity *entity);

	static inline u32 GetGuidFromEntity(CLightEntity* entity);
	static inline CLightEntity* GetEntityFromGuid(u32 guid);

	static inline CLightSource* GetCurrentLightSource(bool updateThread = true);
	static inline CLightEntity* GetCurrentLightEntity();

	static void AddLightToGuidMap(CLightEntity* entity);
	static void RemoveLightFromGuidMap(CLightEntity* entity);
	static bool DoesGuidExist(u32 guid);
	static void ClearLightGuidMap();

	static void SetupDefLight(CLightSource* light, const grcTexture* pTexture, const bool renderCost);
	static void DrawLight(u32 index, bool useHighResMesh);

	static void ShowLightLinks(const spdAABB& box, const CLightSource *lights, const int *indices, int totalPotentialLights);

	static void ShowLightsDebugInfo();
	static void CycleCutsceneDebugInfo();

	static void DisplayEntityLightsInfo();

	static void UpdateLights();

	// Variables
	static bool m_debug;
	static bool m_isolateLight;
	static bool m_isolateLightShaft;
	static bool m_syncAllLightShafts;

	static bool m_freeze;
	static bool m_showForwardDebugging;
	static bool m_showForwardReflectionDebugging;
	static bool m_cablesCanUseLocalLights;
	static bool m_renderCullPlaneToStencil;
	static int m_numFastCullLightsCount;

	// Settings for overriding the scene light shadows
	static bool m_overrideSceneLightShadows;
	static bool m_scenePointShadows;
	static bool m_sceneSpotShadows;
	static bool m_sceneStaticShadows;
	static bool m_sceneDynamicShadows;

	static bool m_showLightMesh;
	static bool m_showLightBounds;
	static bool m_showLightShaftMesh;
	static bool m_showLightShaftBounds;
	static bool m_useHighResMesh;
	static bool m_useWireframe;
	static bool m_useAlpha;
	static float m_alphaMultiplier;
	static bool m_useLightColour;
	static bool m_useShading;
	static bool m_drawAllLights;
	static bool m_drawAllLightCullPlanes;
	static bool m_drawAllLightShafts;
	static bool m_usePicker;
	static bool	m_showCost;
	static float m_showCostOpacity;
	static int m_showCostMinLights;
	static int m_showCostMaxLights;
	static bool m_showAxes;

	static bool m_showLightsForEntity;

	static u32 m_lightGuid;

	static bool m_printStats;
	static u32 m_numOverflow;

	static RegdLight m_currentLightEntity;
	static RegdLight m_currentLightShaftEntity;

	static bool	m_EnableLightFalloffScaling;
	static bool m_EnableLightAABBTest;
	static bool m_EnableScissoringOfLights;
	static bool m_drawScreenQuadForSelectedLight;

	static bool m_IgnoreIsPossibleFlag;

	static s32 m_lights[MAX_STORED_LIGHTS];
	static s32 m_numLights;
	
	struct CutsceneDebugInfo
	{
		float fPrevOpacity;
		u8 bPrevStandardLightsBucket;
		u8 bPrevCutsceneLightsBucket;
		u8 bPrevMiscLightsBucket;
		u8 uDisplayMode;
	};
	static CutsceneDebugInfo m_CutsceneDebugInfo;

#if __DEV
	static bool  m_debugCoronaSizeAdjustEnabled;
	static bool  m_debugCoronaSizeAdjustOverride;
	static float m_debugCoronaSizeAdjustFar;
	static bool  m_debugCoronaApplyNearFade; // i don't think this works very well, let's see if we can get rid of it
	static bool  m_debugCoronaApplyZBias;
	static bool	 m_debugCoronaUseAdditionalZBiasForReflections;
	
	static bool  m_debugCoronaUseZBiasMultiplier;
	static bool  m_debugCoronaOverwriteZBiasMultiplier;
	static float m_debugCoronaZBiasMultiplier;
	static float m_debugCoronaZBiasDistanceNear;
	static float m_debugCoronaZBiasDistanceFar;
	static bool  m_debugCoronaShowOcclusion;
#endif // __DEV

	static void SetCurrentLightEntity(CLightEntity* entity);
	//static void SetCurrentLightShaftEntity(CLightEntity* entity);

private:

	// Functions
	static void FocusOnCurrentLight();
	static void TakeControlOfPicker();
	
	// Variables
	static bool m_enabled;
	static bool m_reloadLights;
	static bool m_reloadLightEditBank;
	static bool m_reloadLightShaftEditBank;
	static RegdLight m_previousLightEntity;
	static RegdLight m_previousLightShaftEntity;
	static bkButton *m_lightEditAddWidgets;
	static char m_fileName[];

	static u32 m_LatestGuid;
	static atMap<CLightEntity*, u32> m_EntityGuidMap;

	static Vector3 m_point;

	static grcRasterizerStateHandle m_wireframeBack_R;
	static grcRasterizerStateHandle m_wireframeFront_R;
	static grcRasterizerStateHandle m_solid_R;
	static grcDepthStencilStateHandle m_noDepthWriteLessEqual_DS;
	static grcDepthStencilStateHandle m_lessEqual_DS;

	static grcRasterizerStateHandle m_cost_inside_R;
	static grcDepthStencilStateHandle m_cost_inside_DS;
	static grcRasterizerStateHandle m_cost_outside_R;
	static grcDepthStencilStateHandle m_cost_outside_DS;
	static grcDepthStencilStateHandle m_greater_DS;
	static grcBlendStateHandle m_cost_B;

};

// ----------------------------------------------------------------------------------------------- //

inline CLightSource* DebugLights::GetCurrentLightSource(bool updateThread) 
{
	if (!m_currentLightEntity)
		return NULL;

	u32 numLights = (updateThread) ? Lights::m_numSceneLights : Lights::m_numRenderLights;
	CLightSource* lights = (updateThread) ? Lights::m_sceneLights : Lights::m_renderLights;

	for (s32 i = 0; i < numLights; i++)
	{
		if (lights[i].GetLightEntity() == m_currentLightEntity)
		{
			return &lights[i];
		}
	}

	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

inline CLightEntity* DebugLights::GetCurrentLightEntity()
{
	return m_currentLightEntity;
}

// ----------------------------------------------------------------------------------------------- //

inline s32 DebugLights::GetLightIndexFromEntity(CLightEntity *entity, bool updateThread) 
{
	u32 numLights = (updateThread) ? Lights::m_numSceneLights : Lights::m_numRenderLights;
	CLightSource* lights = (updateThread) ? Lights::m_sceneLights : Lights::m_renderLights;

	for (s32 i = 0; i < numLights; i++)
	{
		if (lights[i].GetLightEntity() == entity)
		{
			return i;
		}
	}

	return -1;
}

// ----------------------------------------------------------------------------------------------- //

inline s32 DebugLights::GetLightShaftIndexFromEntity(CLightEntity *entity) 
{
	for (s32 i = 0; i < Lights::m_numLightShafts; i++)
	{
		if (Lights::m_lightShaft[i].m_entity == entity)
		{
			return i;
		}
	}

	return -1;
}

// ----------------------------------------------------------------------------------------------- //

inline CLightEntity* DebugLights::GetEntityFromGuid(u32 guid)
{
	atMap<CLightEntity*, u32>::Iterator iter = m_EntityGuidMap.CreateIterator();
	while (!iter.AtEnd())
	{
		if(iter.GetData() == guid)
		{
			return iter.GetKey();		
		}

		iter.Next();
	}

	return 0; //Invalid GUID.
}

// ----------------------------------------------------------------------------------------------- //

inline u32 DebugLights::GetGuidFromEntity(CLightEntity* entity)
{
	return m_EntityGuidMap[entity];
}

#endif // __BANK

#endif // DEBUG_LIGHTS_H_
