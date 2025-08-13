// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "grcore/debugdraw.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "atl/bitset.h"
#include "system/param.h"
#include "system/nelem.h"
#include "grprofile/timebars.h"

// framework
#include "fwutil/xmacro.h"
#include "fwdrawlist/drawlist.h"

// game
#include "camera/viewports/ViewportManager.h"
#include "debug/GtaPicker.h"
#include "debug/Editing/LightEditing.h"
#include "debug/Rendering/DebugView.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugRendering.h"
#include "debug/BudgetDisplay.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "game/Clock.h"
#include "peds/Ped.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/LightSource.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/Util/ShaderUtil.h"
//#include "renderer/Util/Util.h"
#include "scene/FileLoader.h"
#include "scene/world/GameWorld.h"
#include "system/controlMgr.h"
#include "renderer/rendertargets.h"
#if __BANK

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

PARAM(enable_light_edit, "Enable light editing, can specify a particular light index");
PARAM(enable_light_FalloffScaling, "Enable TC modifier falloff scaling");
// ----------------------------------------------------------------------------------------------- //

s32 EditLightSource::m_lightIndex = -1;

CLightAttr EditLightSource::m_clearLightAttr;
CLightAttr EditLightSource::m_editLightAttr;
Vector3 EditLightSource::m_editPosition;
Vector3 EditLightSource::m_editDirection;
Vector3 EditLightSource::m_editTangent;
Color32 EditLightSource::m_editColour;
Vector3 EditLightSource::m_editSrgbColour;
Color32 EditLightSource::m_editVolOuterColour;

// ----------------------------------------------------------------------------------------------- //

s32 EditLightShaft::m_lightShaftIndex = -1;
s32 EditLightShaft::m_previousLightShaftIndex = -1;

CLightShaftAttr EditLightShaft::m_clearLightShaftAttr;
CLightShaftAttr EditLightShaft::m_editLightShaftAttr;

// ----------------------------------------------------------------------------------------------- //

RENDER_OPTIMISATIONS()

bool DebugLights::m_debug = false;

bool DebugLights::m_showCost = false;
float DebugLights::m_showCostOpacity = 0.75f;
int DebugLights::m_showCostMinLights = 1;
int DebugLights::m_showCostMaxLights = 8;

bool DebugLights::m_showAxes = true;

u32 DebugLights::m_lightGuid = 0;

bool DebugLights::m_printStats = false;
u32 DebugLights::m_numOverflow = 0;
Vector3 DebugLights::m_point;

#if __DEV
bool  DebugLights::m_debugCoronaSizeAdjustEnabled = true;
bool  DebugLights::m_debugCoronaSizeAdjustOverride = false;
float DebugLights::m_debugCoronaSizeAdjustFar = 0.0f;
bool  DebugLights::m_debugCoronaApplyNearFade = true;
bool  DebugLights::m_debugCoronaApplyZBias = true;
bool  DebugLights::m_debugCoronaUseAdditionalZBiasForReflections = true;
bool  DebugLights::m_debugCoronaUseZBiasMultiplier = true;
bool  DebugLights::m_debugCoronaOverwriteZBiasMultiplier = false;
float DebugLights::m_debugCoronaZBiasMultiplier = 1.0f;
float DebugLights::m_debugCoronaZBiasDistanceNear = 20.0f;
float DebugLights::m_debugCoronaZBiasDistanceFar = 40.0f;

bool  DebugLights::m_debugCoronaShowOcclusion = false;
#endif // __DEV

bool DebugLights::m_freeze = false;
bool DebugLights::m_isolateLight = false;
bool DebugLights::m_isolateLightShaft = false;
bool DebugLights::m_syncAllLightShafts = false;

bool DebugLights::m_showForwardDebugging = false;
bool DebugLights::m_showForwardReflectionDebugging = false;
bool DebugLights::m_cablesCanUseLocalLights = true;
bool DebugLights::m_renderCullPlaneToStencil = true;
int DebugLights::m_numFastCullLightsCount = 0;


bool DebugLights::m_overrideSceneLightShadows = false;
bool DebugLights::m_scenePointShadows = true;
bool DebugLights::m_sceneSpotShadows = true;
bool DebugLights::m_sceneStaticShadows = true;
bool DebugLights::m_sceneDynamicShadows = true;

bool DebugLights::m_showLightMesh = true;
bool DebugLights::m_showLightBounds = true;
bool DebugLights::m_showLightShaftMesh = true;
bool DebugLights::m_showLightShaftBounds = true;
bool DebugLights::m_useHighResMesh = false;
bool DebugLights::m_useWireframe = true;

bool DebugLights::m_useAlpha = false;
float DebugLights::m_alphaMultiplier = 1.0f;
bool DebugLights::m_useLightColour = true;
bool DebugLights::m_useShading = false;
bool DebugLights::m_drawAllLights = false;
bool DebugLights::m_drawAllLightCullPlanes = false;
bool DebugLights::m_drawAllLightShafts = false;

bool DebugLights::m_usePicker = false;
bool DebugLights::m_showLightsForEntity = false;


bool DebugLights::m_enabled = false;
bool DebugLights::m_reloadLights = false;
bool DebugLights::m_reloadLightEditBank = false;
bool DebugLights::m_reloadLightShaftEditBank = false;

RegdLight DebugLights::m_currentLightEntity;
RegdLight DebugLights::m_currentLightShaftEntity;
RegdLight DebugLights::m_previousLightEntity;
RegdLight DebugLights::m_previousLightShaftEntity;
bkButton *DebugLights::m_lightEditAddWidgets = NULL;

u32 DebugLights::m_LatestGuid = 1; // 0 will mean 'invalid'
atMap<CLightEntity*, u32> DebugLights::m_EntityGuidMap;
#define MAX_ENTITY_GUID_ENTRIES 2048

grcRasterizerStateHandle DebugLights::m_wireframeBack_R;
grcRasterizerStateHandle DebugLights::m_wireframeFront_R;
grcRasterizerStateHandle DebugLights::m_solid_R;
grcDepthStencilStateHandle DebugLights::m_noDepthWriteLessEqual_DS;
grcDepthStencilStateHandle DebugLights::m_lessEqual_DS;

grcDepthStencilStateHandle DebugLights::m_greater_DS;

char DebugLights::m_fileName[RAGE_MAX_PATH];

bool DebugLights::m_EnableLightFalloffScaling = false;
bool DebugLights::m_EnableLightAABBTest = true;
bool DebugLights::m_EnableScissoringOfLights = true;

bool DebugLights::m_drawScreenQuadForSelectedLight = false;

bool DebugLights::m_IgnoreIsPossibleFlag = false;

s32 DebugLights::m_lights[MAX_STORED_LIGHTS];
s32 DebugLights::m_numLights = -1;;

DebugLights::CutsceneDebugInfo DebugLights::m_CutsceneDebugInfo =
{
	0.0f,	// fPrevOpacity
	0,		// bPrevStandardLightsBucket
	0,		// bPrevCutsceneLightsBucket
	0,		// bPrevMiscLightsBucket
	0		// uDisplayMode;
};

// =============================================================================================== //
// HELPER FUNCTIONS
// =============================================================================================== //
// ----------------------------------------------------------------------------------------------- //

// convert a light type bit code into a widget combo friendly index
u8 GetWidgetLightTypeIndex(u8 lightType)
{
	switch(lightType)
	{
		case LIGHT_TYPE_POINT: return 0;  
		case LIGHT_TYPE_SPOT: return 1; 
		case LIGHT_TYPE_CAPSULE: return 2; 
		default:return 0;
	}
}


EditLightSource::EditLightSource()
{
	//Initialize the m_clearLightAttr's internal variables.  Otherwise trash values may come in
	//and cause RAG to throw out-of-range exceptions based on the widget rules.
	m_clearLightAttr.Reset(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f), 1.0f, 45.0f);

	m_editLightAttr = m_clearLightAttr;
	m_editLightAttr.m_lightType = GetWidgetLightTypeIndex(m_clearLightAttr.m_lightType);
}

// ----------------------------------------------------------------------------------------------- //

void EditLightSource::Draw(const CLightEntity *entity, const u32 lightIndex, const bool drawExtras, const bool drawAll, const bool drawCullPlane)
{
	const CLightSource &light = Lights::m_renderLights[lightIndex];

	if (light.GetType() != LIGHT_TYPE_POINT && 
		light.GetType() != LIGHT_TYPE_SPOT &&
		light.GetType() != LIGHT_TYPE_CAPSULE)
		return;

	const grcViewport& grcVP = *grcViewport::GetCurrent();

	// visibility test
	if (grcVP.IsSphereVisible(light.GetPosition().x, light.GetPosition().y, light.GetPosition().z, light.GetRadius()) != cullOutside)
	{
		bool bDrawDebugExtras = drawExtras;

		if (entity != NULL && entity != light.GetLightEntity())
		{
			bDrawDebugExtras = false;
		}

		if (entity != NULL && DebugLights::m_isolateLight)
		{
			int count;
			float timeToRender = -1.0f;

#if RAGE_TIMEBARS
			::rage::g_pfTB.GetGpuBar().GetFunctionTotals("Scene Lights", count, timeToRender);
#endif //RAGE_TIMEBARS

			grcDebugDraw::AddDebugOutputEx(false, Color32(255, 255, 255, 255), ""); 
			grcDebugDraw::AddDebugOutputEx(false, Color32(255, 255, 255, 255), "Isolated light information:"); 
			grcDebugDraw::AddDebugOutputEx(false, Color32(255, 255, 255, 255), "Cost:  %.4fms", timeToRender); 
			grcDebugDraw::AddDebugOutputEx(false, Color32(255, 255, 255, 255), "Inten: %.4f", light.GetIntensity());
			grcDebugDraw::AddDebugOutputEx(false, Color32(255, 255, 255, 255), "Size:  %.4f", light.GetRadius());
		}

		if (bDrawDebugExtras)
		{
			Matrix34 basis;
			Vector3 C = light.GetDirection(),
				A = light.GetTangent(),
				B;
			B.Cross(C, A);
			basis.Set(A, B, C, light.GetPosition());

			// Draw Axis for light
			if (DebugLights::m_showAxes)
			{
				//float mag = (camInterface::GetPos() - light.GetPosition()).Mag();
				grcDebugDraw::Axis(basis, 1.0f, true, 1);
				//grcDebugDraw::GizmoPosition(MATRIX34_TO_MAT34V(basis), mag*0.15f);
			}

			if ((entity != NULL || drawCullPlane) && light.GetFlag(LIGHTFLAG_USE_CULL_PLANE))
			{
				// Draw the actual plane
				Vector3 x;
				Vector3 y;
				Vector3 z = light.GetPlane().GetVector3();

				//BuildOrthoAxes(x, y, z);
				x.Cross(z, FindMinAbsAxis(z));
				x.Normalize();
				y.Cross(x, z);

				x *= light.GetRadius();
				y *= light.GetRadius();

				const Vec3V lightPos = VECTOR3_TO_VEC3V(light.GetPosition());
				const Vec3V planeOrigin = PlaneProject(VECTOR4_TO_VEC4V(light.GetPlane()), lightPos);
				const Vec3V ox = VECTOR3_TO_VEC3V(x);
				const Vec3V oy = VECTOR3_TO_VEC3V(y);

				const Vec3V p0 = planeOrigin + ox + oy;
				const Vec3V p1 = planeOrigin + ox - oy;
				const Vec3V p2 = planeOrigin - ox + oy;
				const Vec3V p3 = planeOrigin - ox - oy;

				grcDebugDraw::Poly(p0,p1,p2,Color32(255, 255, 255, 128),true,true);
				grcDebugDraw::Poly(p1,p2,p3,Color32(255, 255, 255, 128),true,true);
			}
		}

		// Lets bail out early if we can
		const bool showDebug = DebugLights::m_debug && DebugLights::m_showLightMesh;
		const bool entityIsSelected = ((light.GetLightEntity() && light.GetLightEntity() == entity));

		const bool renderLight = (showDebug && entityIsSelected) || drawAll; 

		if (!renderLight)
			return;

		bool useHighResMesh = false;

		// stencil the volume of influence
		bool enableShadowDeform = false;

		eDeferredShadowType shadowType = DEFERRED_SHADOWTYPE_NONE;
		int s = CParaboloidShadow::GetLightShadowIndex(light, true);
		if (s != -1)
		{
			shadowType = CParaboloidShadow::GetDeferredShadowType(s);
		}

		//skip shadow deform if light radius smaller than a specified size on screen (for POINT)
		if (shadowType == DEFERRED_SHADOWTYPE_POINT || shadowType == DEFERRED_SHADOWTYPE_HEMISPHERE)
		{
			static dev_float debugFovSize = 0.3f;
			float tanHFov = grcVP.GetTanHFOV();
			Vector3 dir = VEC3V_TO_VECTOR3(grcVP.GetCameraPosition()) - light.GetPosition();
			float dist = dir.Mag();
			float size = light.GetRadius() / dist;
			enableShadowDeform = (size >= (tanHFov * debugFovSize));
		}

		switch (shadowType)
		{
			case DEFERRED_SHADOWTYPE_POINT:
			case DEFERRED_SHADOWTYPE_HEMISPHERE:
			{
				useHighResMesh = enableShadowDeform;
				break;
			}

			default:
			{
				useHighResMesh = false;
			}
		}

		DebugLights::DrawLight(lightIndex, useHighResMesh);
	}
}

// ----------------------------------------------------------------------------------------------- //

void EditLightSource::Change(RegdLight *entity)
{
	// Lights
	m_lightIndex = Clamp<s32>(m_lightIndex, -1, DebugLights::m_numLights - 1);
	
	if (m_lightIndex != -1)
	{
		CLightEntity* currentEntity = GetEntity();

		if (currentEntity)
		{
			g_PickerManager.AddEntityToPickerResults(currentEntity, true, true);

			// Get Light attribute
			CLightAttr* attr = currentEntity->GetLight();

			if (attr != NULL)
			{
				m_editLightAttr.Set(attr);
				m_editLightAttr.m_lightType = GetWidgetLightTypeIndex(attr->m_lightType);

				m_editPosition = attr->GetPosDirect().GetVector3();
				m_editDirection = attr->m_direction.GetVector3();
				m_editTangent = attr->m_tangent.GetVector3();
				m_editColour = attr->m_colour.GetColor32();
				m_editVolOuterColour = attr->m_volOuterColour.GetColor32();
				m_editSrgbColour = attr->m_colour.GetVector3();
			}
		}
		
		*entity = currentEntity;
	}

	if (m_lightIndex == -1)
	{
		*entity = NULL;
	}
}

// ----------------------------------------------------------------------------------------------- //

void EditLightSource::Update(CLightEntity *entity)
{
	if (entity && entity->GetLight())
	{
		CLightAttr *lightData = entity->GetLight();
		
		m_editLightAttr.Set(lightData);
		m_editLightAttr.m_lightType = GetWidgetLightTypeIndex(lightData->m_lightType);

		m_editPosition = lightData->GetPosDirect().GetVector3();
		m_editDirection = lightData->m_direction.GetVector3();
		m_editTangent = lightData->m_tangent.GetVector3();
		m_editColour = lightData->m_colour.GetColor32();
		m_editVolOuterColour = lightData->m_volOuterColour.GetColor32();
		m_editSrgbColour = lightData->m_colour.GetVector3();
	}
	else
	{
		m_editLightAttr.Set(&m_clearLightAttr);
		m_editLightAttr.m_lightType = GetWidgetLightTypeIndex(m_clearLightAttr.m_lightType);

		m_editPosition = Vector3(0.0f, 0.0f, 0.0f);
		m_editDirection = Vector3(0.0f, 0.0f, 0.0f);
		m_editTangent = Vector3(0.0f, 0.0f, 0.0f);
		m_editColour = Color32(0, 0, 0, 0);
		m_editSrgbColour = Vector3(0.0f, 0.0f, 0.0f);
		m_editVolOuterColour = Color32(0, 0, 0, 0);

		m_lightIndex = -1;
		entity = NULL;
	}
}

// ----------------------------------------------------------------------------------------------- //
void EditLightSource::ApplyColourChanges()
{
	u32 r = (u32)(m_editSrgbColour.GetX() * 255.0f + 0.5f); 
	u32 g = (u32)(m_editSrgbColour.GetY() * 255.0f + 0.5f);
	u32 b = (u32)(m_editSrgbColour.GetZ() * 255.0f + 0.5f);

	m_editColour.SetRed(Max<u32>(0, Min<u32>(r, 255)));
	m_editColour.SetGreen(Max<u32>(0, Min<u32>(g, 255)));
	m_editColour.SetBlue(Max<u32>(0, Min<u32>(b, 255)));

	DebugLights::ApplyChanges();
}

// ----------------------------------------------------------------------------------------------- //

void EditLightSource::ApplyChanges(CLightAttr *attr)
{
	// Map light type to actual light type (till we stop using bits)
	m_editLightAttr.m_lightType = (1 << (m_editLightAttr.m_lightType));

	if (m_editLightAttr.m_lightType != LIGHT_TYPE_POINT &&
		m_editLightAttr.m_lightType != LIGHT_TYPE_SPOT  && 
		m_editLightAttr.m_lightType != LIGHT_TYPE_CAPSULE)
	{
		m_editLightAttr.m_lightType = LIGHT_TYPE_POINT;  // don't let the widgets set it to NONE...
	}

	// Normalise culling plane
	Vector3 planeNormal(
		m_editLightAttr.m_cullingPlane[0],
		m_editLightAttr.m_cullingPlane[1],
		m_editLightAttr.m_cullingPlane[2]);

	planeNormal.Normalize();

	m_editLightAttr.m_cullingPlane[0] = planeNormal.GetX();
	m_editLightAttr.m_cullingPlane[1] = planeNormal.GetY();
	m_editLightAttr.m_cullingPlane[2] = planeNormal.GetZ();

	//make sure edited light is valid
	Vector3 t_dir = m_editDirection;
	Vector3 t_tan = m_editTangent;

	if (abs(t_dir.Mag2() - 1.0f) > 0.001f) 
	{
		t_dir.Normalize();
	}

	if (abs(t_dir.Dot(t_tan)) >0.001f)
	{
		Vector3 t_tmp;
		t_tmp.Cross(t_tan, t_dir);
		t_tan.Cross(t_dir, t_tmp);
	}

	if (abs(t_tan.Mag2() - 1.0f) > 0.001f) 
	{
		t_tan.Normalize();
	}

	attr->Set(&m_editLightAttr);
	attr->SetPos(m_editPosition);
	attr->m_direction.Set(t_dir);
	attr->m_tangent.Set(t_tan);
	attr->m_colour.Set(m_editColour);
	attr->m_volOuterColour.Set(m_editVolOuterColour);

	attr->m_direction.Set(t_dir);
	attr->m_tangent.Set(t_tan);
	m_editDirection = t_dir;
	m_editTangent = t_tan;
	m_editLightAttr.m_lightType = GetWidgetLightTypeIndex(m_editLightAttr.m_lightType);
}

// ----------------------------------------------------------------------------------------------- //

//The 3DS Max Light Editor relies on these value names so remember to contact the tools team if you are making changes here.
static const char* lightFlashinessStrings[] =
{
	"FL_CONSTANT",					// on all the time
	"FL_RANDOM",					// flickers randomly
	"FL_RANDOM_OVERRIDE_IF_WET",	// flickers randomly (forced if road is wet)
	"FL_ONCE_SECOND",				// on/off once every second
	"FL_TWICE_SECOND",				// on/off twice every second
	"FL_FIVE_SECOND",				// on/off five times every second
	"FL_RANDOM_FLASHINESS",			// might flicker, might not
	"FL_OFF",						// always off. really only used for traffic lights
	"FL_UNUSED1",					// Not used
	"FL_ALARM",						// Flashes on briefly
	"FL_ON_WHEN_RAINING",			// Only render when it's raining.
	"FL_CYCLE_1",					// Fades in and out in so that the three of them together always are on (but cycle through colours)
	"FL_CYCLE_2",					// Fades in and out in so that the three of them together always are on (but cycle through colours)
	"FL_CYCLE_3",					// Fades in and out in so that the three of them together always are on (but cycle through colours)
	"FL_DISCO",						// In tune with the music
	"FL_CANDLE",					// Just like a candle
	"FL_PLANE",						// The little lights on planes/helis. They flash briefly
	"FL_FIRE",						// A more hectic version of the candle
	"FL_THRESHOLD",					// experimental
	"FL_ELECTRIC",					// Change colour slightly
	"FL_STROBE",					// Strobe light
};
CompileTimeAssert(NELEM(lightFlashinessStrings) == FL_COUNT);

void EditLightSource::AddWidgets(bkBank *bank)
{
	//The 3DS Max Light Editor relies on these widget names so remember to contact the tools team if you are making changes here.

	bank->PushGroup("Light Source");
	{
		const char* lightTypeStrings[] =
		{
			"LIGHT_TYPE_POINT",
			"LIGHT_TYPE_SPOT",
			"LIGHT_TYPE_CAPSULE"
		};

		bank->AddSeparator();
		bank->AddCombo("Light type", &m_editLightAttr.m_lightType, NELEM(lightTypeStrings), lightTypeStrings, DebugLights::ApplyChanges);
		bank->AddSeparator();
				
		bank->AddColor ("Colour"          , &m_editColour, DebugLights::ApplyChanges);
		bank->AddColor ("Colour (sRGB)"   , &m_editSrgbColour, DebugLights::ApplyColourChanges, "Colour in the correct colour space", NULL, true);
		bank->AddSlider("Intensity"       , &m_editLightAttr.m_intensity, 0.0f, 32.0f, 0.01f, DebugLights::ApplyChanges);
		bank->AddSeparator();
		bank->AddTitle("Falloff");
		bank->AddSeparator();
		bank->AddSlider("Falloff distance", &m_editLightAttr.m_falloff, 0.1f, 256.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
		bank->AddSlider("Falloff exponent", &m_editLightAttr.m_falloffExponent, 0.0, 256.0f, 0.01f, DebugLights::ApplyChanges);
		bank->AddSeparator();
		bank->PushGroup("Position - Direction - Tangent");
		{
			bank->AddSlider("Position"        , &m_editPosition, -8000.0f, 8000.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Direction"       , &m_editDirection, -1.0f, 1.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Tangent"         , &m_editTangent, -1.0f, 1.0f, 0.01f, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Spot");
		{
			bank->AddSlider("Cone inner"      , &m_editLightAttr.m_coneInnerAngle, 0.0f, 90.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddSlider("Cone outer"      , &m_editLightAttr.m_coneOuterAngle, 0.0f, 90.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Capsule");
		{
			bank->AddSlider("Capsule extent"  , &m_editLightAttr.m_extents.x, 0.0f, 90.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Shadows");
		{
			bank->AddSlider("Shadow near clip", &m_editLightAttr.m_shadowNearClip, 0.01f, 128.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Shadow Blur", &m_editLightAttr.m_shadowBlur, 0, 255, 1, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Cutoff distances");
		{
			bank->AddSlider("Light" , &m_editLightAttr.m_lightFadeDistance, 0, 255, 1, DebugLights::ApplyChanges);
			bank->AddSlider("Shadow" , &m_editLightAttr.m_shadowFadeDistance, 0, 255, 1, DebugLights::ApplyChanges);
			bank->AddSlider("Specular" , &m_editLightAttr.m_specularFadeDistance, 0, 255, 1, DebugLights::ApplyChanges);
			bank->AddSlider("Volumetric" , &m_editLightAttr.m_volumetricFadeDistance, 0, 255, 1, DebugLights::ApplyChanges);
		}

		bank->PopGroup();
		bank->PushGroup("Culling plane");
		{
			bank->AddToggle("Enable", &m_editLightAttr.m_flags, LIGHTFLAG_USE_CULL_PLANE, DebugLights::ApplyChanges);
			bank->AddSlider("X"     , &m_editLightAttr.m_cullingPlane[0], -1.0f, 1.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Y"     , &m_editLightAttr.m_cullingPlane[1], -1.0f, 1.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Z"     , &m_editLightAttr.m_cullingPlane[2], -1.0f, 1.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Dist"  , &m_editLightAttr.m_cullingPlane[3], -128.0f, 128.0f, 0.01f, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Corona");
		{
			bank->AddSlider("Corona size"     , &m_editLightAttr.m_coronaSize, 0.0f, 100.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddSlider("Corona intensity", &m_editLightAttr.m_coronaIntensity, 0.0f, 16.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddSlider("Corona z bias"   , &m_editLightAttr.m_coronaZBias, 0.0f, 16.0f, 1.0f/256.0f, DebugLights::ApplyChanges);
			#if __DEV
			bank->AddSeparator();
			bank->PushGroup("Corona test (experimental)", false);
			{
				bank->AddToggle("Corona debug size adjust enabled" , &DebugLights::m_debugCoronaSizeAdjustEnabled);
				bank->AddToggle("Corona debug size adjust override", &DebugLights::m_debugCoronaSizeAdjustOverride);
				bank->AddSlider("Corona debug size adjust far"     , &DebugLights::m_debugCoronaSizeAdjustFar, 0.0f, 256.0f, 1.0f/32.0f);
				bank->AddToggle("Corona debug apply near fade"     , &DebugLights::m_debugCoronaApplyNearFade);
				bank->AddToggle("Corona debug apply z bias"        , &DebugLights::m_debugCoronaApplyZBias);
				bank->AddToggle("Corona use z bias multiplier"	   , &DebugLights::m_debugCoronaUseZBiasMultiplier);
				bank->AddToggle("Corona use additional z bias for reflections", &DebugLights::m_debugCoronaUseAdditionalZBiasForReflections);
				bank->AddToggle("Corona show occlusion"            , &DebugLights::m_debugCoronaShowOcclusion);
			}
			bank->PopGroup();
			#endif // __DEV
		}
		bank->PopGroup();
		bank->PushGroup("Volume");
		{
			bank->AddSlider("Volume intensity"      , &m_editLightAttr.m_volIntensity, 0.0f, 512.0f, 0.01f, DebugLights::ApplyChanges);
			bank->AddSlider("Volume size scale"     , &m_editLightAttr.m_volSizeScale, 0.0f, 2.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddColor ("Volume outer colour"   , &m_editVolOuterColour, DebugLights::ApplyChanges);
			bank->AddSlider("Volume outer intensity", &m_editLightAttr.m_volOuterIntensity, 0.0f, 32.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddSlider("Volume outer exponent" , &m_editLightAttr.m_volOuterExponent, 0.0f, 512.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
		}

		bank->PopGroup();
		bank->PushGroup("Flags");
		{
			bank->AddToggle("Don't use in cutscenes"				, &m_editLightAttr.m_flags, LIGHTFLAG_DONT_USE_IN_CUTSCENE, DebugLights::ApplyChanges);
			bank->AddToggle("Texture projection"					, &m_editLightAttr.m_flags, LIGHTFLAG_TEXTURE_PROJECTION, DebugLights::ApplyChanges);
			bank->AddToggle("Cast static shadows"					, &m_editLightAttr.m_flags, LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS, DebugLights::ApplyChanges);
			bank->AddToggle("Cast dynamic shadows"					, &m_editLightAttr.m_flags, LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS, DebugLights::ApplyChanges);
			bank->AddToggle("Calc from sun"							, &m_editLightAttr.m_flags, LIGHTFLAG_CALC_FROM_SUN, DebugLights::ApplyChanges);
			bank->AddToggle("Enable buzzing"						, &m_editLightAttr.m_flags, LIGHTFLAG_ENABLE_BUZZING, DebugLights::ApplyChanges);
			bank->AddToggle("Force buzzing"							, &m_editLightAttr.m_flags, LIGHTFLAG_FORCE_BUZZING, DebugLights::ApplyChanges);
			bank->AddToggle("Draw volume"							, &m_editLightAttr.m_flags, LIGHTFLAG_DRAW_VOLUME, DebugLights::ApplyChanges);
			bank->AddToggle("No specular"							, &m_editLightAttr.m_flags, LIGHTFLAG_NO_SPECULAR, DebugLights::ApplyChanges);
			bank->AddToggle("Both interior and exterior"			, &m_editLightAttr.m_flags, LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR, DebugLights::ApplyChanges);
			bank->AddToggle("Corona only"							, &m_editLightAttr.m_flags, LIGHTFLAG_CORONA_ONLY, DebugLights::ApplyChanges);
			bank->AddToggle("Not in reflections"					, &m_editLightAttr.m_flags, LIGHTFLAG_NOT_IN_REFLECTION, DebugLights::ApplyChanges);
			bank->AddToggle("Only in reflections"					, &m_editLightAttr.m_flags, LIGHTFLAG_ONLY_IN_REFLECTION, DebugLights::ApplyChanges);
			bank->AddToggle("Use volume outer colour"				, &m_editLightAttr.m_flags, LIGHTFLAG_USE_VOLUME_OUTER_COLOUR, DebugLights::ApplyChanges);
			bank->AddToggle("Increase shadow resolution priority"	, &m_editLightAttr.m_flags, LIGHTFLAG_CAST_HIGHER_RES_SHADOWS, DebugLights::ApplyChanges);
			bank->AddToggle("Cast higher resolution shadows"		, &m_editLightAttr.m_flags, LIGHTFLAG_CAST_HIGHER_RES_SHADOWS, DebugLights::ApplyChanges);
			bank->AddToggle("Use only low resolution Shadows"		, &m_editLightAttr.m_flags, LIGHTFLAG_CAST_ONLY_LOWRES_SHADOWS, DebugLights::ApplyChanges);
			bank->AddToggle("VIP LOD light"							, &m_editLightAttr.m_flags, LIGHTFLAG_FAR_LOD_LIGHT, DebugLights::ApplyChanges);
			bank->AddToggle("Don't light alpha"						, &m_editLightAttr.m_flags, LIGHTFLAG_DONT_LIGHT_ALPHA, DebugLights::ApplyChanges);
			bank->AddToggle("Cast shadow if you can"				, &m_editLightAttr.m_flags, LIGHTFLAG_CAST_SHADOWS_IF_POSSIBLE, DebugLights::ApplyChanges);
			bank->AddToggle("PED ONLY light"						, &m_editLightAttr.m_padding3, EXTRA_LIGHTFLAG_USE_AS_PED_ONLY, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Read-only Flags");
		{
			bank->AddToggle("Interior only"							, &m_editLightAttr.m_flags, LIGHTFLAG_INTERIOR_ONLY, DebugLights::ApplyChanges);
			bank->AddToggle("Exterior only"							, &m_editLightAttr.m_flags, LIGHTFLAG_EXTERIOR_ONLY, DebugLights::ApplyChanges);
			bank->AddToggle("Vehicle"								, &m_editLightAttr.m_flags, LIGHTFLAG_VEHICLE, DebugLights::ApplyChanges);
			bank->AddToggle("VFX"									, &m_editLightAttr.m_flags, LIGHTFLAG_FX, DebugLights::ApplyChanges);
			bank->AddToggle("Cutscene"								, &m_editLightAttr.m_flags, LIGHTFLAG_CUTSCENE, DebugLights::ApplyChanges);
			bank->AddToggle("Moving"								, &m_editLightAttr.m_flags, LIGHTFLAG_MOVING_LIGHT_SOURCE, DebugLights::ApplyChanges);
			bank->AddToggle("Vehicle twin"							, &m_editLightAttr.m_flags, LIGHTFLAG_USE_VEHICLE_TWIN, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Effects");
		{
			bank->AddCombo ("Flashiness"    , &m_editLightAttr.m_flashiness, NELEM(lightFlashinessStrings), lightFlashinessStrings, DebugLights::ApplyChanges);
			bank->AddToggle("Buzzing"       , &m_editLightAttr.m_flags, LIGHTFLAG_ENABLE_BUZZING, DebugLights::ApplyChanges);
			bank->AddToggle("Forced buzzing", &m_editLightAttr.m_flags, LIGHTFLAG_FORCE_BUZZING, DebugLights::ApplyChanges);
		}
		bank->PopGroup();
		bank->PushGroup("Hours light is on");
		{
			bank->AddButton("Toggle all hours on/off", EditLightSource::WidgetsToggleHoursCB);

			for (u32 i = 0; i < 24; i++)
			{
				bank->AddToggle(atVarString("Hour %02d", i), &m_editLightAttr.m_timeFlags, (1<<i), DebugLights::ApplyChanges);
			}
		}
		bank->PopGroup();
	}
	bank->PopGroup();
}

void EditLightSource::WidgetsToggleHoursCB()
{
	m_editLightAttr.m_timeFlags = m_editLightAttr.m_timeFlags ? 0x00000000 : 0x00FFFFFF;
	DebugLights::ApplyChanges();
}

// ----------------------------------------------------------------------------------------------- //

CLightEntity* EditLightSource::GetEntity()
{
	s32 lightIndex = DebugLights::m_lights[m_lightIndex];

	if (lightIndex >= 0 && lightIndex < MAX_STORED_LIGHTS)
	{
		return Lights::m_renderLights[lightIndex].GetLightEntity();
	}

	return NULL;
}

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

EditLightShaft::EditLightShaft()
{
	//Initialize the m_clearLightAttr's internal variables.  Otherwise trash values may come in
	//and cause RAG to throw out-of-range exceptions based on the widget rules.
	m_clearLightShaftAttr.m_data.Init();
	m_editLightShaftAttr .m_data.Init();
}

// ----------------------------------------------------------------------------------------------- //

void EditLightShaft::Draw(const CLightEntity *entity, bool bShowMesh, bool bShowBounds, bool bDrawAll)
{
	if (bShowMesh || bShowBounds)
	{
		for (int i = 0; i < Lights::m_numRenderLightShafts; i++)
		{
			const CLightShaft* shaft = &Lights::m_renderLightShaft[i];

			if (shaft->m_entity == entity || bDrawAll)
			{
				const int a = (shaft->m_entity == entity) ? 255 : 128;

				if (bShowMesh)
				{
					const Color32 shaftColour = Color32(255, 255, 255, a);
					const ScalarV shaftLength = ScalarV(shaft->m_length);
					const Vec3V   shaftoffset = shaft->m_direction*shaftLength;

					Vec3V corners[2][4];
					corners[0][0] = shaft->m_corners[1]; // 0,1 are swapped
					corners[0][1] = shaft->m_corners[0]; // 0,1 are swapped
					corners[0][2] = shaft->m_corners[2];
					corners[0][3] = shaft->m_corners[3];
					corners[1][0] = corners[0][0] + shaftoffset;
					corners[1][1] = corners[0][1] + shaftoffset;
					corners[1][2] = corners[0][2] + shaftoffset;
					corners[1][3] = corners[0][3] + shaftoffset;

					grcDebugDraw::Quad(corners[0][0], corners[0][1], corners[0][3], corners[0][2], shaftColour, true, false);
					grcDebugDraw::Line(corners[0][0], corners[1][0], shaftColour);
					grcDebugDraw::Line(corners[0][1], corners[1][1], shaftColour);
					grcDebugDraw::Line(corners[0][2], corners[1][2], shaftColour);
					grcDebugDraw::Line(corners[0][3], corners[1][3], shaftColour);
					grcDebugDraw::Quad(corners[1][0], corners[1][1], corners[1][3], corners[1][2], shaftColour, true, false);
				}

				if (bShowBounds && shaft->m_entity)
				{
					const Color32 boundColour = Color32(0, 96, 255, a);

					const Vec3V boundMin = VECTOR3_TO_VEC3V(shaft->m_entity->GetBoundingBoxMin());
					const Vec3V boundMax = VECTOR3_TO_VEC3V(shaft->m_entity->GetBoundingBoxMax());

					grcDebugDraw::BoxAxisAligned(boundMin, boundMax, boundColour, false);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void EditLightShaft::Change(RegdLight *entity)
{
	// Shaft
	m_lightShaftIndex = Clamp<s32>(m_lightShaftIndex, -1, Lights::m_numRenderLightShafts - 1);

	if (m_previousLightShaftIndex != m_lightShaftIndex && m_lightShaftIndex != -1)
	{
		CLightEntity* currentEntity = GetEntity();

		if (currentEntity != NULL)
		{
			g_PickerManager.AddEntityToPickerResults(currentEntity, true, true);
			*entity = currentEntity;

			// Get Light attribute
			CLightShaftAttr* attr = static_cast<CLightShaftAttr*>(currentEntity->GetLightShaft());
			m_editLightShaftAttr.Set(attr);
		}
		else
		{
			Assertf(currentEntity != NULL, "Should never be NULL");
		}
	}

	m_previousLightShaftIndex = m_lightShaftIndex;

	if (m_lightShaftIndex == -1)
	{
		*entity = NULL;
	}
}

// ----------------------------------------------------------------------------------------------- //

void EditLightShaft::Update(CLightEntity *entity)
{
	if (entity && entity->GetLightShaft())
	{
		CLightShaftAttr *lightShaftData = entity->GetLightShaft();
		m_editLightShaftAttr.Set(lightShaftData);
	}
	else
	{
		m_editLightShaftAttr.Set(&m_clearLightShaftAttr);
		m_lightShaftIndex = -1;
	}
}

// ----------------------------------------------------------------------------------------------- //

void EditLightShaft::ApplyChanges(CLightShaftAttr *attr)
{
	attr->Set(&m_editLightShaftAttr);
	attr->m_data.m_direction = NormalizeSafe(attr->m_data.m_direction, Vec3V(V_Z_AXIS_WZERO)*ScalarV(V_NEGONE), Vec3V(V_FLT_SMALL_3));
}

// ----------------------------------------------------------------------------------------------- //

void EditLightShaft::CloneProperties()
{
	if (m_lightShaftIndex != -1)
	{
		class LightEntityIterator { public: static bool func(void* item, void*)
		{
			CLightEntity* pLightEntity = reinterpret_cast<CLightEntity*>(item);
			CLightShaftAttr* pLightShaft = pLightEntity->GetLightShaft();

			if (pLightShaft)
			{
				pLightShaft->m_data.m_length            = m_editLightShaftAttr.m_data.m_length;
				pLightShaft->m_data.m_fadeInTimeStart   = m_editLightShaftAttr.m_data.m_fadeInTimeStart;
				pLightShaft->m_data.m_fadeInTimeEnd     = m_editLightShaftAttr.m_data.m_fadeInTimeEnd;
				pLightShaft->m_data.m_fadeOutTimeStart  = m_editLightShaftAttr.m_data.m_fadeOutTimeStart;
				pLightShaft->m_data.m_fadeOutTimeEnd    = m_editLightShaftAttr.m_data.m_fadeOutTimeEnd;
				pLightShaft->m_data.m_fadeDistanceStart = m_editLightShaftAttr.m_data.m_fadeDistanceStart;
				pLightShaft->m_data.m_fadeDistanceEnd   = m_editLightShaftAttr.m_data.m_fadeDistanceEnd;
				pLightShaft->m_data.m_colour            = m_editLightShaftAttr.m_data.m_colour;
				pLightShaft->m_data.m_intensity         = m_editLightShaftAttr.m_data.m_intensity;
				pLightShaft->m_data.m_flashiness        = m_editLightShaftAttr.m_data.m_flashiness;
				pLightShaft->m_data.m_flags             = m_editLightShaftAttr.m_data.m_flags;
				pLightShaft->m_data.m_densityType       = m_editLightShaftAttr.m_data.m_densityType;
				pLightShaft->m_data.m_volumeType        = m_editLightShaftAttr.m_data.m_volumeType;
				pLightShaft->m_data.m_softness          = m_editLightShaftAttr.m_data.m_softness;
			}

			return true;
		}};

		CLightEntity::GetPool()->ForAll(LightEntityIterator::func, NULL);
	}
}

void EditLightShaft::ResetDirection()
{
	const Vec3V a = m_editLightShaftAttr.m_data.m_corners[1] - m_editLightShaftAttr.m_data.m_corners[0];
	const Vec3V b = m_editLightShaftAttr.m_data.m_corners[2] - m_editLightShaftAttr.m_data.m_corners[0];

	m_editLightShaftAttr.m_data.m_direction = Normalize(Cross(a, b));

	DebugLights::ApplyChanges();
}

void EditLightShaft::AddWidgets(bkBank *bank)
{
	bank->PushGroup("Light Shaft");
	{
		bank->PushGroup("Corners", false);
		{
			bank->AddVector("Corner 0", &m_editLightShaftAttr.m_data.m_corners[0], -16000.0f, 16000.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddVector("Corner 1", &m_editLightShaftAttr.m_data.m_corners[1], -16000.0f, 16000.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddVector("Corner 2", &m_editLightShaftAttr.m_data.m_corners[2], -16000.0f, 16000.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
			bank->AddVector("Corner 3", &m_editLightShaftAttr.m_data.m_corners[3], -16000.0f, 16000.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
		}
		bank->PopGroup();

		bank->AddButton("Clone Properties"   , CloneProperties);
		bank->AddButton("Reset Direction"    , ResetDirection);
		bank->AddVector("Direction"          , &m_editLightShaftAttr.m_data.m_direction        ,-1.0f, 1.0f, 0.01f      , DebugLights::ApplyChanges);
		bank->AddSlider("Direction Amount"   , &m_editLightShaftAttr.m_data.m_directionAmount  , 0.0f, 1.0f, 0.01f      , DebugLights::ApplyChanges);
		bank->AddSlider("Fade In Time Start" , &m_editLightShaftAttr.m_data.m_fadeInTimeStart  , 0.0f, 24.0f, 1.0f/60.0f, DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Fade In Time End"   , &m_editLightShaftAttr.m_data.m_fadeInTimeEnd    , 0.0f, 24.0f, 1.0f/60.0f, DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Fade Out Time Start", &m_editLightShaftAttr.m_data.m_fadeOutTimeStart , 0.0f, 24.0f, 1.0f/60.0f, DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Fade Out Time End"  , &m_editLightShaftAttr.m_data.m_fadeOutTimeEnd   , 0.0f, 24.0f, 1.0f/60.0f, DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Fade Distance Start", &m_editLightShaftAttr.m_data.m_fadeDistanceStart, 0.0f, 999.0f, 0.01f    , DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Fade Distance End"  , &m_editLightShaftAttr.m_data.m_fadeDistanceEnd  , 0.0f, 999.0f, 0.01f    , DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Length"             , &m_editLightShaftAttr.m_data.m_length           , 0.0f, 99.0f, 0.01f     , DebugLights::ApplyLightShaftChanges);
		bank->AddColor ("Colour"             , &m_editLightShaftAttr.m_data.m_colour                                    , DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Intensity"          , &m_editLightShaftAttr.m_data.m_intensity        , 0.0f, 50.0f, 0.01f     , DebugLights::ApplyLightShaftChanges);
		bank->AddCombo ("Flashiness"         , &m_editLightShaftAttr.m_data.m_flashiness, NELEM(lightFlashinessStrings), lightFlashinessStrings, DebugLights::ApplyLightShaftChanges);

		const char* dtypeStr[] =
		{
			"LIGHTSHAFT_DENSITYTYPE_CONSTANT"          ,
			"LIGHTSHAFT_DENSITYTYPE_SOFT"              ,
			"LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW"       ,
			"LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW_HD"    ,
			"LIGHTSHAFT_DENSITYTYPE_LINEAR"            ,
			"LIGHTSHAFT_DENSITYTYPE_LINEAR_GRADIENT"   ,
			"LIGHTSHAFT_DENSITYTYPE_QUADRATIC"         ,
			"LIGHTSHAFT_DENSITYTYPE_QUADRATIC_GRADIENT",
		};
		CompileTimeAssert(NELEM(dtypeStr) == CExtensionDefLightShaftDensityType_NUM_ENUMS);

		const char* vtypeStr[] =
		{
			"LIGHTSHAFT_VOLUMETYPE_SHAFT"   ,
			"LIGHTSHAFT_VOLUMETYPE_CYLINDER",
		};
		CompileTimeAssert(NELEM(vtypeStr) == CExtensionDefLightShaftVolumeType_NUM_ENUMS);

		bank->AddToggle("Use sun colour"          , &m_editLightShaftAttr.m_data.m_flags, LIGHTSHAFTFLAG_USE_SUN_LIGHT_COLOUR    , DebugLights::ApplyLightShaftChanges);
		bank->AddToggle("Use sun direction"       , &m_editLightShaftAttr.m_data.m_flags, LIGHTSHAFTFLAG_USE_SUN_LIGHT_DIRECTION , DebugLights::ApplyLightShaftChanges);
		bank->AddToggle("Scale by sun colour"     , &m_editLightShaftAttr.m_data.m_flags, LIGHTSHAFTFLAG_SCALE_BY_SUN_COLOUR     , DebugLights::ApplyLightShaftChanges);
		bank->AddToggle("Scale by sun intensity"  , &m_editLightShaftAttr.m_data.m_flags, LIGHTSHAFTFLAG_SCALE_BY_SUN_INTENSITY  , DebugLights::ApplyLightShaftChanges);
		bank->AddToggle("Draw in front and behind", &m_editLightShaftAttr.m_data.m_flags, LIGHTSHAFTFLAG_DRAW_IN_FRONT_AND_BEHIND, DebugLights::ApplyLightShaftChanges);
		bank->AddCombo ("Density type"            , &m_editLightShaftAttr.m_data.m_densityType, NELEM(dtypeStr), dtypeStr        , DebugLights::ApplyLightShaftChanges);
		bank->AddCombo ("Volume type"             , &m_editLightShaftAttr.m_data.m_volumeType, NELEM(vtypeStr), vtypeStr         , DebugLights::ApplyLightShaftChanges);
		bank->AddSlider("Softness"                , &m_editLightShaftAttr.m_data.m_softness, 0.0f, 1.0f, 1.0f/32.0f              , DebugLights::ApplyLightShaftChanges);
		bank->AddSeparator();

		bank->AddToggle("Draw Normal Direction" , &m_editLightShaftAttr.m_data.m_attrExt.m_drawNormalDirection, DebugLights::ApplyChanges);
		bank->AddToggle("Scale by Interior Fill", &m_editLightShaftAttr.m_data.m_attrExt.m_scaleByInteriorFill, DebugLights::ApplyChanges);
		bank->AddToggle("Gradient Align to Quad", &m_editLightShaftAttr.m_data.m_attrExt.m_gradientAlignToQuad, DebugLights::ApplyChanges);
		bank->AddColor ("Gradient Colour"       , &m_editLightShaftAttr.m_data.m_attrExt.m_gradientColour     , DebugLights::ApplyChanges);
		bank->AddVector("Shaft Offset"          , &m_editLightShaftAttr.m_data.m_attrExt.m_shaftOffset        , -50.0f, 50.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
		bank->AddSlider("Shaft Radius"          , &m_editLightShaftAttr.m_data.m_attrExt.m_shaftRadius        ,  0.01f,  8.0f, 1.0f/32.0f, DebugLights::ApplyChanges);
	}
	bank->PopGroup();
}

// ----------------------------------------------------------------------------------------------- //

inline CLightEntity* EditLightShaft::GetEntity()
{
	if (m_lightShaftIndex >= 0 && m_lightShaftIndex < MAX_STORED_LIGHT_SHAFTS)
	{
		return Lights::m_renderLightShaft[m_lightShaftIndex].m_entity;
	}

	return NULL;
}

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void DebugLights::Init()
{
	USE_DEBUG_MEMORY();

	m_EnableLightFalloffScaling = PARAM_enable_light_FalloffScaling.Get();
	
	// Shortcut to enable light editing with a particular light index
	if (PARAM_enable_light_edit.Get())
	{
		int lightIdx = -1;
		PARAM_enable_light_edit.Get(lightIdx);
		EditLightSource::SetIndex(lightIdx);
		EditLightSource::Change(&m_currentLightEntity);
		
		m_debug = true;
		m_reloadLightEditBank = true;
		m_reloadLightShaftEditBank = true;
	}

	grcRasterizerStateDesc wireframeFrontRasterizerState;
	wireframeFrontRasterizerState.FillMode = grcRSV::FILL_WIREFRAME;
	wireframeFrontRasterizerState.CullMode = grcRSV::CULL_BACK;
	wireframeFrontRasterizerState.AntialiasedLineEnable = TRUE;
	m_wireframeFront_R = grcStateBlock::CreateRasterizerState(wireframeFrontRasterizerState);

	grcRasterizerStateDesc wireframeBackRasterizerState;
	wireframeBackRasterizerState.FillMode = grcRSV::FILL_WIREFRAME;
	wireframeBackRasterizerState.CullMode = grcRSV::CULL_FRONT;
	wireframeBackRasterizerState.AntialiasedLineEnable = TRUE;
	m_wireframeBack_R = grcStateBlock::CreateRasterizerState(wireframeBackRasterizerState);

	grcRasterizerStateDesc solidRasterizerState;
	solidRasterizerState.CullMode = grcRSV::CULL_CW;
	m_solid_R = grcStateBlock::CreateRasterizerState(solidRasterizerState);

	grcDepthStencilStateDesc depthReadLessEqualDepthStencilState;
	depthReadLessEqualDepthStencilState.DepthEnable = TRUE;
	depthReadLessEqualDepthStencilState.DepthWriteMask = FALSE;
	depthReadLessEqualDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);

	m_noDepthWriteLessEqual_DS = grcStateBlock::CreateDepthStencilState(depthReadLessEqualDepthStencilState);

	grcDepthStencilStateDesc lessEqualDepthStencilState;
	lessEqualDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);

	m_lessEqual_DS = grcStateBlock::CreateDepthStencilState(lessEqualDepthStencilState);

	grcDepthStencilStateDesc greaterState;
	greaterState.DepthWriteMask = FALSE;
	greaterState.DepthEnable = TRUE;
	greaterState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);

	m_greater_DS = grcStateBlock::CreateDepthStencilState(greaterState);

	m_numLights = 0;
	for (u32 i = 0; i < 255; i++)
	{
		m_lights[i] = -1;
	}
}

void DebugLights::SetCurrentLightEntity(CLightEntity* entity)
{
	if( entity == NULL)
	{
		m_lightGuid = 0;

		// reset
		EditLightSource::SetIndex(-1);
		EditLightShaft::SetIndex(-1);

		m_currentLightEntity = NULL;
		m_currentLightShaftEntity = NULL;
	}
	else
	{
		if (entity->GetLight())
		{
			m_previousLightEntity = m_currentLightEntity;
			m_currentLightEntity = entity;

			EditLightSource::SetIndex(GetLightIndexFromEntity(m_currentLightEntity));
			m_lightGuid = GetGuidFromEntity(m_currentLightEntity);
			m_reloadLightEditBank = true;
		}
		else if (entity->GetLightShaft())
		{
			m_previousLightShaftEntity = m_currentLightShaftEntity;
			m_currentLightShaftEntity = entity;

			EditLightShaft::SetIndex(GetLightShaftIndexFromEntity(m_currentLightShaftEntity));
			m_reloadLightShaftEditBank = true;
		}
	}
}

void DebugLights::AddLightToGuidMap(CLightEntity* entity)
{
	if ( DebugEditing::LightEditing::IsInLightEditingMode() == true)
	{
		if ( m_EntityGuidMap.Access(entity) == NULL)
		{
			m_EntityGuidMap.Insert(entity, m_LatestGuid);
		}
		else
		{
			m_EntityGuidMap[entity] = m_LatestGuid;
		}
		
		if ( m_EntityGuidMap.GetNumUsed() > MAX_ENTITY_GUID_ENTRIES )
			m_EntityGuidMap.Kill();

		m_LatestGuid++;
	}
	
}

void DebugLights::RemoveLightFromGuidMap(CLightEntity* entity)
{
	if ( DebugEditing::LightEditing::IsInLightEditingMode() == true)
	{
		if ( m_EntityGuidMap.Access(entity) != NULL)
		{
			m_EntityGuidMap.Delete(entity);
		}
	}

	if ( m_currentLightEntity == entity )
	{
		m_currentLightEntity = NULL;
	}
}

bool DebugLights::DoesGuidExist(const u32 guid)
{
	if ( DebugEditing::LightEditing::IsInLightEditingMode() == true)
	{
		atMap<CLightEntity*, u32>::Iterator it = m_EntityGuidMap.CreateIterator();
		for(it.Start(); !it.AtEnd(); it.Next())
		{
			const u32 tmpGuid = it.GetData();
			if(guid == tmpGuid)
			{
				return true;
			}
		}
	}

	return false;
}

void DebugLights::ClearLightGuidMap()
{
	m_EntityGuidMap.Kill();
}

bool DebugLights::CalcTechAndPass(CLightSource* light, u32 &pass, grcEffectTechnique &technique, const bool useShadows)
{
	bool debugEnabled = true;
	if (debugEnabled)
	{
		technique = m_techniques[DR_TECH_LIGHT];

		switch(light->GetType())
		{
			case LIGHT_TYPE_POINT: { pass = 0; break; }
			case LIGHT_TYPE_SPOT: { pass = 1; break; }
			case LIGHT_TYPE_CAPSULE: { pass = 2; break; }
			default: { break; }
		}

		if (useShadows)
		{
			pass += 3;
		}
	}
	return debugEnabled;
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::GetLightIndexRange(u32 &startIndex, u32 &endIndex)
{
	if (m_isolateLight)
	{
		for (u32 i = 0; i < Lights::m_numRenderLights; i++)
		{
			if (Lights::m_renderLights[i].GetLightEntity() == m_currentLightEntity)
			{
				startIndex = i;
				endIndex = i + 1;
				return;
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::GetLightIndexRangeUpdateThread(u32 &startIndex, u32 &endIndex)
{
	if (m_isolateLight)
	{
		for (u32 i = 0; i < Lights::m_numSceneLights; i++)
		{
			if (Lights::m_sceneLights[i].GetLightEntity() == m_currentLightEntity)
			{
				startIndex = i;
				endIndex = i + 1;
				return;
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

CEntity* DebugLights::GetIsolatedLightShaftEntity()
{
	if (m_isolateLightShaft && EditLightShaft::GetIndex() != -1)
	{
		return m_currentLightShaftEntity.Get();
	}

	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::Draw()
{
	if (!m_debug && !m_showCost)
	{
		return;
	}

#if ENABLE_PIX_TAGGING
	PF_PUSH_MARKER("Debug Lights");
#endif // ENABLE_PIX_TAGGING

#if __PPU
	float lineWidth = 0.5f;
	GCM_STATE(SetLineWidth, (u32)(lineWidth * 8.0f)); // 6.3 fixed point
#endif // __PPU

	CLightSource* lights = Lights::m_renderLights;
	DebugDeferred::SetupGBuffers();

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
	// Calculate an light bounds radius increase that will include the area between the eye and the near 
	// clip plane (plus a little fudge, since the 
	float nearPlaneBoundIncrease = 
		grcVP->GetNearClip() * 
		sqrtf(1.0f + grcVP->GetTanHFOV()*grcVP->GetTanHFOV() + grcVP->GetTanVFOV()*grcVP->GetTanVFOV());
	/// const Vector3 cameraPosition = VEC3V_TO_VECTOR3(grcVP->GetCameraPosition());

	// Go through ALL of the scene lights and work out which ones are on screen.
	u32 t_startLightIndex = 0;
	u32 t_endLightIndex = Lights::m_numRenderLights;
	DebugLights::GetLightIndexRange(t_startLightIndex, t_endLightIndex);

	// Setup render state
	if (m_showCost)
	{
		grcRenderTarget* target = CRenderTargets::GetHudBuffer();
		CRenderTargets::UnlockUIRenderTargets();
		grcTextureFactory::GetInstance().LockRenderTarget(0, target, CRenderTargets::GetUIDepthBuffer(true));
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 0, false, 0);
		Lights::RenderSceneLights(true, true);
		grcResolveFlags resolveFlags;
		#if __XENON
			resolveFlags.NeedResolve=true;
			resolveFlags.ClearDepthStencil=false;
			resolveFlags.ClearColor=true;
			resolveFlags.Color=Color32(0,0,0,0);
		#endif // __XENON
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
		CRenderTargets::LockUIRenderTargets();

		DebugRendering::RenderCost(target, DR_COST_PASS_OVERDRAW, m_showCostOpacity, m_showCostMinLights, m_showCostMaxLights, "Light Cost Legend:");
	}
	else if (m_debug)
	{
		grcStateBlock::SetDepthStencilState(m_lessEqual_DS);
		grcStateBlock::SetRasterizerState((m_useWireframe) ? m_wireframeFront_R : m_solid_R);
		grcStateBlock::SetBlendState((m_useAlpha) ? grcStateBlock::BS_Add : grcStateBlock::BS_Default);

		for (s32 i = t_startLightIndex; i < t_endLightIndex; i++)
		{
			if (m_currentLightEntity != NULL && m_currentLightEntity == lights[i].GetLightEntity()) 
			{
				const bool isInLight = lights[i].IsPointInLight(m_point, nearPlaneBoundIncrease);
				grcDebugDraw::Sphere(m_point, 1.0f, (isInLight) ? Color32(255, 0, 0, 255) : Color32(0, 0, 255, 255), true);
				if (m_drawScreenQuadForSelectedLight)
				{
					CLightSource *pLight = GetCurrentLightSource(false);
					int x, y, width, height;
					pLight->GetScissorRect(x, y, width, height);

					float gameWidth  = (float)VideoResManager::GetUIWidth();
					float gameHeight = (float)VideoResManager::GetUIHeight();

					grcDebugDraw::Text(Vector2(0.04f, 0.075f + 20.0f/gameHeight), Color_yellow, 
									  atVarString("Scissor Rect: x %3d, y %3d, width %3d, height %3d, Scissoring %s", 
												  x, y, width, height, m_currentLightEntity->GetCustomFlag(LIGHTENTITY_FLAG_SCISSOR_LIGHT) ? "enabled" : "disabled"));

					if ((x & y & width & height) != -1)
					{
						float recipWidth  = 1.0f / gameWidth;
						float recipHeight = 1.0f / gameHeight;

						float left   = float(x) * recipWidth;
						float top    = float(y) * recipHeight;
						float right  = left + (float(width)  * recipWidth);
						float bottom = top  + (float(height) * recipHeight);

						grcDebugDraw::Quad(Vector2(left, top), Vector2(right, top), Vector2(right, bottom), Vector2(left, bottom), Color32(0, 255, 0, 255), false);
					}
				}
			}

			EditLightSource::Draw(m_currentLightEntity, i, true, m_drawAllLights, m_drawAllLightCullPlanes);
		}
		EditLightShaft::Draw(m_currentLightShaftEntity, m_showLightShaftMesh, m_showLightShaftBounds, m_drawAllLightShafts);
	}

	// Print out some debug text about the light entity -> light source relationship
	float ypos = 0.0f;
	if (m_currentLightEntity != NULL && m_currentLightEntity->GetParent() != NULL)
	{
		CLightEntity* entity = m_currentLightEntity;
		CBaseModelInfo* info = m_currentLightEntity->GetParent()->GetBaseModelInfo();
		if (info != NULL)
		{
			grcDebugDraw::Text(Vector2(0.04f, 0.075f + ypos/(float)GRCDEVICE.GetHeight()), Color_yellow, atVarString("Light Parent: %s (id=%d)", info->GetModelName(), entity->Get2dEffectId()));
			ypos += 12.0f;

			if (m_currentLightEntity->Get2dEffectType() == ET_LIGHT && m_currentLightEntity->GetLight())
			{
				const u32 timeFlags = m_currentLightEntity->GetLight()->m_timeFlags;
				const u32 hour = CClock::GetHour();
				bool lightEnabled = (timeFlags & (1 << hour)) ? true : false;

				if ( m_currentLightEntity->GetLight()->m_intensity == 0.0f || lightEnabled == false )
				{
					grcDebugDraw::Text(Vector2(0.04f, 0.075f + ypos/(float)GRCDEVICE.GetHeight()), Color_yellow, "Light is currently off.");
					ypos += 12.0f;
				}
			}
		}
	}

	if (m_debug && m_currentLightEntity != NULL)
	{
		Color32 bboxColor(0, 0, 255, 255);

		// Check if this entity has an attached light
		bool selectedLightEntityHasLightSource = (GetLightIndexFromEntity(m_currentLightEntity, false) != -1);

		// Draw text if entity doesn't have a light source.
		if (!selectedLightEntityHasLightSource && m_currentLightEntity->Get2dEffectType() == ET_LIGHT)
		{
			grcDebugDraw::Text(Vector2(0.04f, 0.075f + ypos/(float)GRCDEVICE.GetHeight()), Color_yellow, "Light does not currently have an associated Light Source");
			ypos += 12.0f;

			// also set bboxColor to fugly warning flashing :)
			static float colorT = 0.0f;
			colorT += 0.05f;
			if (colorT >= 2 * 3.14f)
				colorT = 0.0f;
			bboxColor.Setf(1.0f, 0.25f + rage::Cosf(colorT) * 0.25f, 0.0f, 1.0f);
		}
	
		if (m_showLightBounds)
		{
			Vector3 min = m_currentLightEntity->GetBoundingBoxMin();
			Vector3 max = m_currentLightEntity->GetBoundingBoxMax();

			// for lights, the bounding box is already in worldspace
			grcDebugDraw::BoxAxisAligned(
				RCC_VEC3V(min),
				RCC_VEC3V(max),
				bboxColor,
				false);
		}
	}

	if (m_currentLightShaftEntity != NULL && m_currentLightShaftEntity->GetParent() != NULL)
	{
		CLightEntity* entity = m_currentLightShaftEntity;
		CBaseModelInfo* info = m_currentLightShaftEntity->GetParent()->GetBaseModelInfo();
		if (info != NULL)
		{
			grcDebugDraw::Text(Vector2(0.04f, 0.075f + ypos/(float)GRCDEVICE.GetHeight()), Color_yellow, atVarString("Light Shaft Parent: %s (id=%d)", info->GetModelName(), entity->Get2dEffectId()));
			ypos += 12.0f;
		}
	}

#if __PPU
	GCM_STATE(SetLineWidth, 8); // restore
#endif // __PPU

#if ENABLE_PIX_TAGGING
	PF_POP_MARKER();
#endif // ENABLE_PIX_TAGGING

}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::InitWidgets()
{
	DebugEditing::LightEditing::InitCommands();
	bkBank* liveEditBank = CDebug::GetLiveEditBank();
	AddWidgetsOnDemand(liveEditBank);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::FocusOnCurrentLight()
{
	if (!m_currentLightEntity)
	{
		return;
	}

	Vector3 worldPos;
	worldPos.Zero();
	if (!m_currentLightEntity->GetWorldPosition(worldPos))
	{
		return;
	}

	Vector3 forward(-2.0f, -2.0f, -2.0f);
	Vector3 lookFrom = worldPos - forward;

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if ( debugDirector.IsFreeCamActive() == false )
		debugDirector.ActivateFreeCam();	//Turn on debug free cam.

	//Move free camera to desired place.
	camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
	freeCamFrame.SetPosition(lookFrom);
	freeCamFrame.SetWorldMatrixFromFront(forward);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::AddWidgetsOnDemand(bkBank *bk)
{
	bkBank *bank = bk;
	bank->PushGroup("Lights Edit");

	bank->AddToggle("Enable light editing", &m_debug);

	bank->AddSeparator();

	bank->AddToggle("Debug draw all lights",&m_drawAllLights);
	bank->AddToggle("Debug draw all light cull planes",&m_drawAllLightCullPlanes);
	bank->AddToggle("Isolate light", &m_isolateLight);
	bank->AddSlider("Scene light", EditLightSource::GetIndexPtr(), -1, MAX_STORED_LIGHTS - 1, 1, datCallback(CFA1(EditLightSource::Change), (void*)&m_currentLightEntity));

	bank->AddSeparator();

	bank->AddToggle("Debug draw all light shafts",&m_drawAllLightShafts);
	bank->AddToggle("Isolate light shaft", &m_isolateLightShaft);
	bank->AddSlider("Scene light shaft", EditLightShaft::GetIndexPtr(), -1, MAX_STORED_LIGHT_SHAFTS - 1, 1, datCallback(CFA1(EditLightShaft::Change), (void*)&m_currentLightShaftEntity));

	bank->AddSeparator();

	bank->PushGroup("Debug", false);
	{
		bank->AddToggle("Enable picker", &m_usePicker);
		bank->AddToggle("Show forward lighting debug", &m_showForwardDebugging);
		bank->AddToggle("Show forward lighting reflection debug", &m_showForwardReflectionDebugging);
		bank->AddToggle("Show light entities for selected entity", &m_showLightsForEntity);
		bank->AddToggle("Gather local lights for cables that use them", &m_cablesCanUseLocalLights);
		bank->AddToggle("Render cull plane to stencil", &m_renderCullPlaneToStencil);
		bank->AddSlider("Num Lights in FastCull List", &m_numFastCullLightsCount, 0, MAX_STORED_LIGHTS, 0);
		bank->AddVector("Point", &m_point, -4096.0f, 4096.0f, 1.0f);
		
		bank->AddSeparator();
		bank->AddToggle("Show light axes", &m_showAxes);
		bank->AddToggle("Show graph", &DebugView::m_views[DEBUGVIEW_LIGHTGRAPH].m_show);
		bank->AddToggle("Sync all light shafts", &m_syncAllLightShafts);
		bank->AddToggle("Freeze scene lights",&m_freeze);
		bank->AddToggle("Print lights stats", &m_printStats, datCallback(DebugLights::ShowLightsDebugInfo));

		bank->AddSeparator();
		bank->AddButton("Focus on current light", FocusOnCurrentLight);

		bank->AddSeparator();
		bank->AddToggle("Enable light Falloff Scaling", &DebugLights::m_EnableLightFalloffScaling);

		bank->AddSeparator();
		bank->AddToggle("Enable light AABB test", &DebugLights::m_EnableLightAABBTest);

		bank->AddSeparator();
		bank->AddToggle("Enable light scissoring", &m_EnableScissoringOfLights);	
		bank->AddToggle("Draw Scissor Rect for selected light", &m_drawScreenQuadForSelectedLight);
	}
	bank->PopGroup();

	bank->PushGroup("Light Cost", false);
	{
		bank->AddToggle("Show light cost", &m_showCost);
		bank->AddSlider("Light cost opacity", &m_showCostOpacity, 0.0f, 1.0f, 1.0f/64.0f);
		bank->AddSlider("Light cost min", &m_showCostMinLights, 1, 32, 1);
		bank->AddSlider("Light cost max", &m_showCostMaxLights, 1, 32, 1);
	}
	bank->PopGroup();

	bank->PushGroup("Scene light enable", false);
	{
		bank->AddToggle("Override scene shadow state", &DebugLights::m_overrideSceneLightShadows);
		bank->AddToggle("Scene point shadows", &DebugLights::m_scenePointShadows);
		bank->AddToggle("Scene spot shadows", &DebugLights::m_sceneSpotShadows);
		bank->AddToggle("Dynamic shadows", &DebugLights::m_sceneDynamicShadows);
		bank->AddToggle("Static shadows", &DebugLights::m_sceneStaticShadows);
		bank->AddToggle("Possible shadows",&m_IgnoreIsPossibleFlag);
	}
	bank->PopGroup();

	bank->PushGroup("Mesh display options", false);
	{
		bank->AddToggle("Show light mesh", &m_showLightMesh);
		bank->AddToggle("Show light bounds", &m_showLightBounds);
		bank->AddToggle("Show light shaft mesh", &m_showLightShaftMesh);
		bank->AddToggle("Show light shaft bounds", &m_showLightShaftBounds);
		bank->AddToggle("Use hi-res light mesh", &m_useHighResMesh);
		bank->AddToggle("Wireframe", &m_useWireframe);
		bank->AddToggle("Alpha blending", &m_useAlpha);
		bank->AddSlider("Alpha multiplier", &m_alphaMultiplier, 0.0f, 1.0f, 0.01f);
		bank->AddToggle("Colour of light", &m_useLightColour);
		bank->AddToggle("Shading", &m_useShading);
	}
	bank->PopGroup();

	EditLightSource::AddWidgets(bank);
	EditLightShaft::AddWidgets(bank);

	bank->PopGroup();
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::ShowLightsDebugInfo()
{
	BUDGET_TOGGLE(m_printStats);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::CycleCutsceneDebugInfo()
{
	BudgetDisplay& budgetDisplay = SBudgetDisplay::GetInstance();

	const u8 kMaxModes = 3; // Off, Minimal, Full
	m_CutsceneDebugInfo.uDisplayMode = (m_CutsceneDebugInfo.uDisplayMode + 1) % kMaxModes;

	if (m_CutsceneDebugInfo.uDisplayMode == 0)
	{
		// Restore previous state
		budgetDisplay.Activate(m_printStats);
		budgetDisplay.EnableAutoBucketDisplay(true);

		budgetDisplay.SetBgOpacity(m_CutsceneDebugInfo.fPrevOpacity);
		budgetDisplay.EnableBucket(BB_STANDARD_LIGHTS, (m_CutsceneDebugInfo.bPrevStandardLightsBucket != 0));
		budgetDisplay.EnableBucket(BB_CUTSCENE_LIGHTS, (m_CutsceneDebugInfo.bPrevCutsceneLightsBucket != 0));
		budgetDisplay.EnableBucket(BB_MISC_LIGHTS, (m_CutsceneDebugInfo.bPrevMiscLightsBucket != 0));
	}
	else
	{
		budgetDisplay.Activate(true);
		budgetDisplay.EnableAutoBucketDisplay(false);

		if (m_CutsceneDebugInfo.uDisplayMode == 1)
		{
			// No pie chart
			budgetDisplay.TogglePieChart(false);

			// Minimal info (Cutscene info / Main timings)

			// Cutscene lighting team requested all light info off on first setting so saving previous settings first
			m_CutsceneDebugInfo.bPrevStandardLightsBucket = (u8)budgetDisplay.IsBucketEnabled(BB_STANDARD_LIGHTS);
			m_CutsceneDebugInfo.bPrevCutsceneLightsBucket = (u8)budgetDisplay.IsBucketEnabled(BB_CUTSCENE_LIGHTS);
			m_CutsceneDebugInfo.bPrevMiscLightsBucket	  = (u8)budgetDisplay.IsBucketEnabled(BB_MISC_LIGHTS);

			budgetDisplay.EnableBucket(BB_STANDARD_LIGHTS, false);
			budgetDisplay.EnableBucket(BB_CUTSCENE_LIGHTS, false);
			budgetDisplay.EnableBucket(BB_MISC_LIGHTS, false);

			// Cutscene lighting team requested 0 opacity when enabled so saving previous setting first
			m_CutsceneDebugInfo.fPrevOpacity = budgetDisplay.GetBgOpacity();

			budgetDisplay.SetBgOpacity(0.0f);
		}
		else if (m_CutsceneDebugInfo.uDisplayMode == 2)
		{
			budgetDisplay.TogglePieChart(true);

			// Full info (Cutscene info / Std light info / Cutscene light Info / Main timings)
			budgetDisplay.EnableBucket(BB_STANDARD_LIGHTS, true);
			budgetDisplay.EnableBucket(BB_CUTSCENE_LIGHTS, true);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::DisplayEntityLightsInfo()
{
	if (g_PickerManager.IsEnabled())
	{
		CEntity *entity = (CEntity*) g_PickerManager.GetSelectedEntity();

		if(entity)
		{
			const s32 maxNumLightEntities = (s32) CLightEntity::GetPool()->GetSize();

			for(s32 i = 0; i < maxNumLightEntities; i++)
			{
				if(!CLightEntity::GetPool()->GetIsFree(i))
				{
					CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
					Assert(pLightEntity);

					CEntity* pLightParent = pLightEntity->GetParent();

					if (pLightParent && pLightParent == entity)
					{
						const Vector3& bboxMin = pLightEntity->GetBoundingBoxMin();
						const Vector3& bboxMax = pLightEntity->GetBoundingBoxMax();

						grcDebugDraw::AddDebugOutput(
							"BBox = Min (%.2f,%.2f,%.2f) Max (%.2f,%.2f,%.2f) | Ptr = 0x%p",
							bboxMin.GetX(), bboxMin.GetY(), bboxMin.GetZ(),
							bboxMax.GetX(), bboxMax.GetY(), bboxMax.GetZ(),
							pLightEntity);
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::UpdateLights()
{
	m_numLights = 0;
	for (u32 i = 0; i < MAX_STORED_LIGHTS; i++)
	{
		m_lights[i] = -1;
	}

	for (u32 i = 0; i < Lights::m_numSceneLights; i++)
	{
		CLightSource* currentLight = &Lights::m_sceneLights[i];

		if (currentLight->GetType() == LIGHT_TYPE_POINT || 
			currentLight->GetType() == LIGHT_TYPE_SPOT ||
			currentLight->GetType() == LIGHT_TYPE_CAPSULE)
		{
			m_lights[m_numLights] = i;
			m_numLights++;
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::DrawLight(u32 index, bool useHighResMesh)
{
	CLightSource* light = &Lights::m_renderLights[index];

	// Use high-res mesh
	if (m_useHighResMesh)
		useHighResMesh = true;

	int shadowIndex = CParaboloidShadow::GetLightShadowIndex(*light, true);

	SetupDefLight(light, NULL, false);

	// Get pass, technique
	u32 pass = 0;
	grcEffectTechnique technique;
	CalcTechAndPass(light, pass, technique, shadowIndex >= 0);

	if (ShaderUtil::StartShader("Debug Light", m_shader, technique, pass))
	{
		Lights::BeginLightRender();
		Lights::RenderVolumeLight(light->GetType(), useHighResMesh, false, shadowIndex>=0);
		Lights::EndLightRender();
		ShaderUtil::EndShader(m_shader);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::Update()
{
	// Cutscene debug info
	if (m_CutsceneDebugInfo.uDisplayMode > 0)
	{
		BudgetDisplay& budgetDisplay = SBudgetDisplay::GetInstance();

		budgetDisplay.Activate(true);
		budgetDisplay.EnableAutoBucketDisplay(false);

		if (m_CutsceneDebugInfo.uDisplayMode == 1)
		{
			budgetDisplay.EnableBucket(BB_STANDARD_LIGHTS, false);
			budgetDisplay.EnableBucket(BB_CUTSCENE_LIGHTS, false);
			budgetDisplay.EnableBucket(BB_MISC_LIGHTS, false);
			budgetDisplay.SetBgOpacity(0.0f);
		}
		else if (m_CutsceneDebugInfo.uDisplayMode == 2)
		{
			budgetDisplay.EnableBucket(BB_STANDARD_LIGHTS, true);
			budgetDisplay.EnableBucket(BB_CUTSCENE_LIGHTS, true);
		}
	}

	if (m_showLightsForEntity)
	{
		DisplayEntityLightsInfo();
	}

	if (m_enabled)
	{
		// check if the user has turned off the picker
		if (!m_debug)
		{
			m_enabled = false;
			DebugView::Disable(DEBUGVIEW_LIGHTGRAPH);
			m_lightGuid = 0;

			// reset
			EditLightSource::SetIndex(-1);
			EditLightShaft::SetIndex(-1);

			m_currentLightEntity = NULL;
			m_previousLightEntity = NULL;

			m_currentLightShaftEntity = NULL;
			m_previousLightShaftEntity = NULL;

			for (u32 i = 0; i < 255; i++)
			{
				m_lights[i] = -1;
			}
			m_numLights = 0;

			EditLightSource::Update(NULL);
		}
	}
	else
	{
		// check if the user has turned on the picker
		if (m_debug)
		{
			m_enabled = true;
			DebugView::Enable(DEBUGVIEW_LIGHTGRAPH);
		}
	}
	if (!m_enabled) { return; }

	// Setup a reload if previous and current entity are not the same
	if (m_previousLightEntity != m_currentLightEntity)
	{
		m_previousLightEntity = m_currentLightEntity;
		m_reloadLightEditBank = true;
	}

	if (m_previousLightShaftEntity != m_currentLightShaftEntity)
	{
		m_previousLightShaftEntity = m_currentLightShaftEntity;
		m_reloadLightShaftEditBank = true;
	}

	// Enable picker when it changes
	if (m_usePicker && (!g_PickerManager.IsCurrentOwner("Light Edit") || !g_PickerManager.IsEnabled()))
	{
		if (!g_PickerManager.IsEnabled()) 
		{
			g_PickerManager.SetEnabled(true);
		}
		TakeControlOfPicker();
	}

	if (m_usePicker && !g_PickerManager.IsCurrentOwner("Light Edit"))
	{
		m_usePicker = false;
	}

	if (!m_usePicker && g_PickerManager.IsCurrentOwner("Light Edit") && g_PickerManager.IsEnabled())
	{
		g_PickerManager.TakeControlOfPickerWidget("");
		g_PickerManager.SetEnabled(false);
	}
	
	if (g_PickerManager.IsEnabled())
	{
		// Find out if interiors or exteriors (for flags)
		u32 locationFlags = 0;
		CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			locationFlags |= (pPlayerPed->GetIsInInterior() ? SEARCH_LOCATION_INTERIORS : SEARCH_LOCATION_EXTERIORS);
		}

		// Pick a new entity (will be NULL if picking disabled)
		CEntity *entity = (CEntity*) g_PickerManager.GetSelectedEntity();

		if( entity && entity->GetIsTypeLight() )
		{
			CLightEntity* lightEntity = (CLightEntity*)entity;

			if (lightEntity->GetLight())
			{
				m_previousLightEntity = m_currentLightEntity;
				m_currentLightEntity = lightEntity;

				EditLightSource::SetIndex(GetLightIndexFromEntity(m_currentLightEntity));
				m_lightGuid = GetGuidFromEntity(m_currentLightEntity);
				m_reloadLightEditBank = true;
			}
			else if (lightEntity->GetLightShaft())
			{
				m_previousLightShaftEntity = m_currentLightShaftEntity;
				m_currentLightShaftEntity = lightEntity;

				EditLightShaft::SetIndex(GetLightShaftIndexFromEntity(m_currentLightShaftEntity));
				m_reloadLightShaftEditBank = true;
			}
		}
	}

	// Reload bank with new info
	if (m_reloadLightEditBank)
	{
		EditLightSource::Update(m_currentLightEntity);

		// Update GUID if we got a valid light
		if (m_currentLightEntity && m_currentLightEntity->GetLight())
		{
			m_lightGuid = GetGuidFromEntity(m_currentLightEntity);
		}
		else
		{
			m_lightGuid = 0;
		}

		m_reloadLightEditBank = false;
	}

	if (m_reloadLightShaftEditBank)
	{
		EditLightShaft::Update(m_currentLightShaftEntity);

		// Update GUID if we got a valid light shaft
		//if (m_currentLightShaftEntity && m_currentLightShaftEntity->GetLightShaft())
		//{
		//	m_lightShaftGuid = GetGuidFromEntity(m_currentLightShaftEntity);
		//}
		//else
		//{
		//	m_lightShaftGuid = 0;
		//}

		m_reloadLightShaftEditBank = false;
	}
}

void DebugLights::TakeControlOfPicker()
{
	fwPickerManagerSettings LightEditPickerManagerSettings(INTERSECTING_ENTITY_PICKER, true, false, ENTITY_TYPE_MASK_LIGHT, false);
	g_PickerManager.SetPickerManagerSettings(&LightEditPickerManagerSettings);

	CGtaPickerInterfaceSettings LightEditPickerInterfaceSettings(false, false, false, false);
	g_PickerManager.SetPickerInterfaceSettings(&LightEditPickerInterfaceSettings);
	
	fwIntersectingEntitiesPickerSettings LightEditIntersectingEntitiesSettings(true, true, false, 100);
	g_PickerManager.SetPickerSettings(INTERSECTING_ENTITY_PICKER, &LightEditIntersectingEntitiesSettings);

	g_PickerManager.TakeControlOfPickerWidget("Light Edit");
	g_PickerManager.ResetList(false);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::ApplyChanges()
{
	if (m_currentLightEntity && m_currentLightEntity->Get2dEffectType() == ET_LIGHT)
		EditLightSource::ApplyChanges(m_currentLightEntity->GetLight());

	if (m_currentLightShaftEntity && m_currentLightShaftEntity->Get2dEffectType() == ET_LIGHT_SHAFT)
		EditLightShaft::ApplyChanges(m_currentLightShaftEntity->GetLightShaft());
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::ApplyColourChanges()
{
	if (m_currentLightEntity && m_currentLightEntity->Get2dEffectType() == ET_LIGHT)
		EditLightSource::ApplyColourChanges();
}


// ----------------------------------------------------------------------------------------------- //

void DebugLights::ApplyLightShaftChanges()
{
	ApplyChanges();

	if (m_syncAllLightShafts)
	{
		EditLightShaft::CloneProperties();
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::SetupDefLight(CLightSource* light, const grcTexture* pTexture, const bool renderCost)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	SetShaderParams();
	DeferredLighting::SetLightShaderParams(DEFERRED_SHADER_DEBUG, light, pTexture, 1.0f, false, false, MM_DEFAULT);
	DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_DEBUG, MM_DEFAULT);

	// Set override / debug params
	if (m_showCost)
	{
		m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], Vec4f(1.0f/255.0f, 0.0f, 0.0f, 1.0f));
	}
	else
	{
		Vector4 col = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		if (m_useLightColour)
		{
			col =  Vector4(light->GetColor().x, light->GetColor().y, light->GetColor().z, 1.0f);
		}
		col *= m_alphaMultiplier;
		col.w = 1.0f;
		m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_OVERRIDE_COLOR0], col);
	}

	Vector4 lightConstants = Vector4(m_useShading, renderCost, 0.0f, 0.0f);
	m_shader->SetVar(m_shaderVars[DR_SHADER_VAR_LIGHT_CONSTANTS], lightConstants);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLights::ShowLightLinks(const spdAABB& box, const CLightSource *lights, const int *indices, int totalPotentialLights)
{
	for (int n = 0; n < MAX_RENDERED_LIGHTS; n++)
	{
		const int index = indices[n];

		if (index != -1)
		{
			const CLightSource *light = &lights[index];
			grcDebugDraw::Line(box.GetCenter(), VECTOR3_TO_VEC3V(light->GetPosition()), Color32(light->GetColor()));
		}
	}

	char str[8];
	sprintf(str, "%d", totalPotentialLights);
	grcDebugDraw::Text(box.GetCenter(), Color32(0, 255, 0, 255), str);
}

// ----------------------------------------------------------------------------------------------- //

#endif // __BANK
