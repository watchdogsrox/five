// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"
#include "input/mouse.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "system/nelem.h"
#include "rmptfx/ptxshader.h"
#include "rmptfx/ptxShadervar.h"
#include "rmptfx/ptxkeyframe.h"
#include "string/stringhash.h"
#include "fwrenderer/renderthread.h"

// game
#include "Camera/viewports/Viewport.h"
#include "Camera/viewports/ViewportManager.h"
#include "Camera/CamInterface.h"
#include "debug/BudgetDisplay.h"
#include "debug/GtaPicker.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugRendering.h"
#include "renderer/render_channel.h"
#include "renderer/rendertargets.h"
#include "renderer/SSAO.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/LODLights.h"
#include "renderer/Lights/lights.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "scene/portals/InteriorProxy.h"
#include "Timecycle/TimeCycle.h"
#include "Vfx/Misc/Coronas.h"

#if __BANK

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

RENDER_OPTIMISATIONS()

extern bool gLastGenMode;

bool		DebugLighting::m_overrideDeferred = true;
bool		DebugLighting::m_overrideForward  = true;

// Deferred
bool		DebugLighting::m_drawLightShafts = true;
bool		DebugLighting::m_drawLightShaftsAlways = false;
bool		DebugLighting::m_drawLightShaftsNormal = false;
bool		DebugLighting::m_drawLightShaftsOffsetEnabled = false;
Vec3V		DebugLighting::m_drawLightShaftsOffset = Vec3V(V_ZERO);
bool		DebugLighting::m_drawSceneLightVolumes = true;
bool		DebugLighting::m_drawSceneLightVolumesAlways = false;
bool		DebugLighting::m_usePreAlphaDepthForVolumetricLight = true;
bool		DebugLighting::m_drawAmbientVolumes = true;
bool		DebugLighting::m_drawAmbientOcclusion = true;

bool		DebugLighting::m_LightShaftNearClipEnabled = true;
float		DebugLighting::m_LightShaftNearClipExponent = LIGHTSHAFT_NEAR_CLIP_EXPONENT_DEFAULT;

#if DEPTH_BOUNDS_SUPPORT
bool		DebugLighting::m_useLightsDepthBounds = true;
#endif // DEPTH_BOUNDS_SUPPORT
Vector4		DebugLighting::m_LightShaftParams = Vector4(1.0f,1.0f,1.0f,1.0f);
ScalarV		DebugLighting:: m_VolumeLowNearPlaneFade = LIGHT_VOLUME_LOW_NEAR_PLANE_FADE;
ScalarV		DebugLighting:: m_VolumeHighNearPlaneFade = LIGHT_VOLUME_HIGH_NEAR_PLANE_FADE;
bool		DebugLighting::m_testSphereAmbVol = false;

bool		DebugLighting::m_FogLightVolumesEnabled = true;

bool		DebugLighting::m_directionalEnable = true;
bool		DebugLighting::m_directionalOverride = false;
Vector3		DebugLighting::m_directionalLightColour = Vector3(1.0f, 0.0f, 0.0f);
float		DebugLighting::m_directionalLightIntensity = 64.0f;

bool		DebugLighting::m_ambientEnable[AMBIENT_OVERRIDE_COUNT]; 
bool		DebugLighting::m_ambientOverride[AMBIENT_OVERRIDE_COUNT];
Vector3		DebugLighting::m_baseAmbientColour[AMBIENT_OVERRIDE_COUNT];
float		DebugLighting::m_baseAmbientIntensity[AMBIENT_OVERRIDE_COUNT];
Vector3		DebugLighting::m_downAmbientColour[AMBIENT_OVERRIDE_COUNT];
float		DebugLighting::m_downAmbientIntensity[AMBIENT_OVERRIDE_COUNT];

bool		DebugLighting::m_particleOverride[PARTICLE_OVERRIDE_COUNT];
float		DebugLighting::m_particleOverrideAmountFloat[PARTICLE_OVERRIDE_COUNT];
atHashValue DebugLighting::m_particleVarHashes[PARTICLE_OVERRIDE_COUNT];
bool		DebugLighting::m_particleOverridesEnable = false;
u32			DebugLighting::m_particleOverrideType[PARTICLE_OVERRIDE_COUNT];
bool		DebugLighting::m_particleOverrideAll = false;

bool		DebugLighting::m_lightsEnable = true;
bool		DebugLighting::m_lightsOverride = false;
Vector3		DebugLighting::m_lightsColour  = Vector3(0.0f, 0.0f, 1.0f);
float		DebugLighting::m_lightsIntensity = 16.0f;

bool		DebugLighting::m_lightsInterior = true;
bool		DebugLighting::m_lightsExterior = true;
bool		DebugLighting::m_lightsShadowCasting = true;
bool		DebugLighting::m_lightsSpot = true;
bool		DebugLighting::m_lightsPoint = true;
u32			DebugLighting::m_lightFlagsToShow = 0;

// advanced
float		DebugLighting::m_deferredDiffuseLightAmount = 1.0f;

bool		DebugLighting::m_lightEntityDebug = false;
s32			DebugLighting::m_lightEntityIndex = -1;
bkSlider*	DebugLighting::m_lightEntityIndexSlider = NULL;

// LOD light debug
bool		DebugLighting::m_showLODLightMemUsage  = false;
bool		DebugLighting::m_showLODLightUsage  = false;
bool		DebugLighting::m_showLODLightHashes = false;
bool		DebugLighting::m_showLODLightTypes = false;

bool		DebugLighting::m_LODLightRender = true;
bool		DebugLighting::m_LODLightsEnableOcclusionTest = true;
bool		DebugLighting::m_LODLightsEnableOcclusionTestInMainThread = true;
bool		DebugLighting::m_LODLightsVisibilityOnMainThread = true;
bool		DebugLighting::m_LODLightsProcessVisibilityAsync = true;
bool		DebugLighting::m_LODLightsPerformSortDuringVisibility = true;
bool		DebugLighting::m_DistantLightsProcessVisibilityAsync = true;
bool		DebugLighting::m_LODLightDebugRender = false;
bool		DebugLighting::m_LODLightPhysicalBoundsRender = false;
bool		DebugLighting::m_LODLightStreamingBoundsRender = false;
s32			DebugLighting::m_LODLightBucketIndex = -1;
s32			DebugLighting::m_LODLightIndex = -1;
char		DebugLighting::m_LODLightsString[255];
bool		DebugLighting::m_LODLightCoronaEnable = true;
bool		DebugLighting::m_LODLightCoronaEnableWaterReflectionCheck = true;
u32			DebugLighting::m_LODLightCategory = 0;
Vector3		DebugLighting::m_LODLightDebugColour[LODLIGHT_CATEGORY_COUNT];
bool		DebugLighting::m_LODLightOverrideColours = false;

bool		DebugLighting::m_distantLODLightRender = true;
bool		DebugLighting::m_distantLODLightDebugRender = false;
bool		DebugLighting::m_distantLODLightPhysicalBoundsRender = false;
bool		DebugLighting::m_distantLODLightStreamingBoundsRender = false;
s32			DebugLighting::m_distantLODLightBucketIndex = -1;
s32			DebugLighting::m_distantLODLightIndex = -1;
char		DebugLighting::m_distantLODLightsString[255];
bool		DebugLighting::m_distantLODLightsOverrideRange = false;
float		DebugLighting::m_distantLODLightsStart = 350;
float		DebugLighting::m_distantLODLightsEnd = 4000;
s32			DebugLighting::m_distantLODLightCategory = 0;

u32			DebugLighting::m_LODLightsBucketsUsed[2] = {0, 0};
u32			DebugLighting::m_peakLODLightsBucketsUsed[2]  = {0, 0};
u32			DebugLighting::m_LODLightsOnMap[2]  = {0, 0};
u32			DebugLighting::m_peakLODLightsOnMap[2]  = {0, 0};
u32			DebugLighting::m_LODLightsStorage[2]  = {0, 0};
u32			DebugLighting::m_peakLODLightsStorage[2]  = {0, 0};
u32			DebugLighting::m_LODLightsProcessed[2] = {0, 0};

u32			DebugLighting::m_lightSize[2];
bool		DebugLighting::m_ForceUnshadowedVolumeLightTechnique = false;

float		DebugLighting::m_MinSpotAngleForBackFaceForce = LIGHT_VOLUME_BACKFACE_TECHNIQUE_MIN_ANGLE;

#if LIGHT_VOLUME_USE_LOW_RESOLUTION
bool		DebugLighting::m_VolumeLightingMaskedUpsample = true;
#endif // LIGHT_VOLUME_USE_LOW_RESOLUTION

bool		DebugLighting::m_bForceLightweightCapsule = false;
bool		DebugLighting::m_bForceHighQualityForward = false;

// extra distant light streaming
bool		DebugLighting::m_bLoadExtraDistantLights = false;
bool		DebugLighting::m_bLoadExtraTrackWithCamera = false;
bool		DebugLighting::m_bDisplayStreamSphere = false;
Vec3V		DebugLighting::m_vExtraDistantLightsCentre(V_ZERO);
float		DebugLighting::m_fExtraDistantLightsRadius = 1000.0f;

bool		DebugLighting::m_dumpedParentImaps = false;

// phone light settings
bool		DebugLighting::m_bPedPhoneLightOverride = false;
bool		DebugLighting::m_bPedPhoneLightDirDebug = false;
Vector3		DebugLighting::m_pedPhoneLightColor = Vector3(1.0f, 1.0f, 1.0f);
float		DebugLighting::m_pedPhoneLightIntensity = 4.0f;
float		DebugLighting::m_pedPhoneLightRadius = 1.0;
float		DebugLighting::m_pedPhoneLightFalloffExponent = 3.0f;
float		DebugLighting::m_pedPhoneLightConeInner = 0.0f;
float		DebugLighting::m_pedPhoneLightConeOuter = 90.0f;

// LOD Lights map data
atArray<s32>			DebugLighting::ms_lodLightImapGroupIndices;
atArray<const char*>	DebugLighting::ms_lodLightImapGroupNames;
s32						DebugLighting::ms_selectedImapComboIndex = 0;
bool					DebugLighting::m_LODLightOwnedByMapData = true;
bool					DebugLighting::m_LODLightOwnedByScript = true;
bkCombo*				g_pLODLightImapGroupCombo = nullptr;
// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void DebugLighting::Init()
{
	// From CDistantLODLight
	m_lightSize[LODLIGHT_DISTANT] = 
		sizeof(FloatXYZ) + 
		sizeof(u32);

	// From CLODLight
	m_lightSize[LODLIGHT_STANDARD] = 
		sizeof(FloatXYZ) + 
		sizeof(float) + 
		sizeof(float) +
		sizeof(u32) +
		sizeof(u32) +
		sizeof(u8) +
		sizeof(u8) +
		sizeof(u8);

	for (u32 i = 0; i < AMBIENT_OVERRIDE_COUNT; i++)
	{
		m_ambientEnable[i] = true;
		m_ambientOverride[i] = false;
		m_baseAmbientColour[i] = Vector3(1.0f, 0.0f, 0.0f);
		m_baseAmbientIntensity[i] = 1.0f;
		m_downAmbientColour[i] = Vector3(0.0f, 1.0f, 0.0f);
		m_downAmbientIntensity[i] = 1.0f;
	}

	const char* particlShaderVars[PARTICLE_OVERRIDE_COUNT] =
	{
		"DirectionalMult",
		"AmbientMult",
		"ExtraLightMult",
		"ShadowAmount",
		"NormalArc"
		"SoftnessCurve",
		"SoftnessShadowMult",
		"SoftnessShadowOffset",
	};

	const float particleOverrideValues[PARTICLE_OVERRIDE_COUNT] =
	{
		1.0f,
		1.0f, 
		1.0f, 
		1.0f, 
		1.0f,
		0.0f,
		0.0f, 
		0.0f
	};

	ptxShaderVarType overrideType[PARTICLE_OVERRIDE_COUNT] = 
	{
		PTXSHADERVAR_FLOAT,
		PTXSHADERVAR_FLOAT,
		PTXSHADERVAR_FLOAT,
		PTXSHADERVAR_KEYFRAME,
		PTXSHADERVAR_KEYFRAME,
		PTXSHADERVAR_FLOAT,
		PTXSHADERVAR_FLOAT,
		PTXSHADERVAR_FLOAT
	};

	for (u32 i = 0; i < PARTICLE_OVERRIDE_COUNT; i++)
	{
		m_particleOverride[i] = false;
		m_particleOverrideAmountFloat[i] = particleOverrideValues[i];
		m_particleVarHashes[i] = atHashValue(particlShaderVars[i]);
		m_particleOverrideType[i] = overrideType[i];
	}

	for (u32 i = 0; i < LODLIGHT_CATEGORY_COUNT; i++)
	{
		Vector3 col = Vector3(0.0f, 0.0f, 0.0f);
		col[i] = 1.0f;
		m_LODLightDebugColour[i] = col;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::Update()
{
	UpdateLODLights();

	// Light entity pool
	if (m_lightEntityDebug)
	{
		for(s32 i = 0; i < CLightEntity::GetPool()->GetSize(); ++i)
		{
			if(!CLightEntity::GetPool()->GetIsFree(i) && (m_lightEntityIndex == -1 || m_lightEntityIndex == i))
			{
				CLightEntity* currentLightEntity = CLightEntity::GetPool()->GetSlot(i);

				spdAABB tempBox;
				const spdAABB &box = currentLightEntity->GetBoundBox(tempBox);

				if (currentLightEntity->GetParent() != NULL && currentLightEntity->GetLight())
				{
					CLightAttr* light = currentLightEntity->GetLight();

					Vec3f col = (light->m_flags & LIGHTFLAG_CORONA_ONLY) ? Vec3f(1.0f, 0.0f, 0.0f) : Vec3f(0.0f, 1.0f, 0.0f);
					grcDebugDraw::BoxAxisAligned(box.GetMin(), box.GetMax(), Color32(col[0], col[1], col[2], 1.0f), false);

					if (m_lightEntityIndex > -1)
					{
						grcDebugDraw::Text(
							box.GetMin(),
							Color32(255, 255, 255, 255),
							0,
							0,
							currentLightEntity->GetParent()->GetModelName());
					}
				}
				else
				{
					grcDebugDraw::BoxAxisAligned(box.GetMin(), box.GetMax(), Color32(255, 0, 0, 255), false);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::Draw() 
{
	RenderLODLights();

	if (m_bDisplayStreamSphere)
	{
		grcDebugDraw::Sphere(m_vExtraDistantLightsCentre, m_fExtraDistantLightsRadius, Color32(255,0,0,80), false, 1, 64);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::LoadLODLightCache()
{
	u32 filterMask = 0;
	filterMask |= m_LODLightOwnedByMapData ? LODLIGHT_IMAP_OWNED_BY_MAP_DATA : 0;
	filterMask |= m_LODLightOwnedByScript ? LODLIGHT_IMAP_OWNED_BY_SCRIPT : 0;
	CLODLightManager::LoadLODLightImapIDXCache(filterMask);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UnloadLODLightCache()
{
	gRenderThreadInterface.Flush();
	u32 filterMask = 0;
	filterMask |= m_LODLightOwnedByMapData ? LODLIGHT_IMAP_OWNED_BY_MAP_DATA : 0;
	filterMask |= m_LODLightOwnedByScript ? LODLIGHT_IMAP_OWNED_BY_SCRIPT : 0;
	CLODLightManager::UnloadLODLightImapIDXCache(filterMask);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::LoadLODLightImapGroupList()
{
	// Ensure that the LOD light system is enabled
	CLODLights::SetEnabled(true);
	ms_lodLightImapGroupIndices.Reset();
	ms_lodLightImapGroupNames.Reset();
	// Load only LOD lights as they depend on distant lights and will automatically load them
	for(s32 i=0; i<fwMapDataStore::GetStore().GetSize(); i++)
	{	
		strLocalIndex index(i);
		fwMapDataDef* pDef = fwMapDataStore::GetStore().GetSlot(index);
		if (pDef && pDef->GetIsValid() && fwMapDataStore::GetStore().IsScriptManaged(index) && pDef->GetContentFlags()&fwMapData::CONTENTFLAG_LOD_LIGHTS)
		{
			ms_lodLightImapGroupIndices.PushAndGrow(i);
			ms_lodLightImapGroupNames.PushAndGrow(pDef->m_name.GetCStr());
		}
	}
	g_pLODLightImapGroupCombo->UpdateCombo("Select IMAP Group", &ms_selectedImapComboIndex, ms_lodLightImapGroupNames.GetCount(), &ms_lodLightImapGroupNames[0]);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::LoadSelectedLODLightImapGroup()
{
	if (ms_selectedImapComboIndex < ms_lodLightImapGroupIndices.GetCount())
	{
		strLocalIndex index(ms_lodLightImapGroupIndices[ms_selectedImapComboIndex]);
		if (!fwMapDataStore::GetStore().GetSlot(index)->IsLoaded())
		{
			fwMapDataStore::GetStore().RequestGroup(index, atStringHash(ms_lodLightImapGroupNames[ms_selectedImapComboIndex]));
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UnloadSelectedLODLightImapGroup()
{
	strLocalIndex index(ms_lodLightImapGroupIndices[ms_selectedImapComboIndex]);
	if (fwMapDataStore::GetStore().GetSlot(index)->IsLoaded())
	{
		fwMapDataStore::GetStore().RemoveGroup(index, atStringHash(ms_lodLightImapGroupNames[ms_selectedImapComboIndex]));
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UpdateParticleOverride()
{
	bool enableParticleOverrides = false;

	for (u32 i = 0; i < PARTICLE_OVERRIDE_COUNT; i++)
	{
		enableParticleOverrides |= m_particleOverride[i];
	}

	if (m_particleOverridesEnable != enableParticleOverrides)
	{
		ptxShaderInst::SetCustomOverrideFunc((enableParticleOverrides) ? DebugLighting::OverrideParticleShaderVars : NULL);
		m_particleOverridesEnable = enableParticleOverrides;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UpdateParticleOverrideAll()
{
	m_particleOverrideAll = !m_particleOverrideAll;
	for (u32 i = 0; i < PARTICLE_OVERRIDE_COUNT; i++)
	{
		m_particleOverride[i] = m_particleOverrideAll;
	}
	UpdateParticleOverride();
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::DumpParentImaps(const bool allowMultipleCalls)
{
	if (m_dumpedParentImaps)
		return ;

	CLightEntity::Pool* lightEntityPool = CLightEntity::GetPool();
	atMap<atHashString, u32> totalPerImap;
	atHashString misc = atHashString("Misc");
	char buffer[255];
	u32 normal = 0;
	u32 dummy = 0;
	u32 noDrawable = 0;
	u32 drawable = 0;

	for(u32 i = 0; i < lightEntityPool->GetSize(); i++)
	{
		if(!lightEntityPool->GetIsFree(i))
		{
			CLightEntity* pLightEntity = lightEntityPool->GetSlot(i);
			
			if (pLightEntity->GetParent() != NULL)
			{
				if (pLightEntity->GetParent()->GetDrawable() != NULL)
				{
					drawable++;
				}
				else
				{
					noDrawable++;
				}

				if (pLightEntity->GetParent()->GetIplIndex())
				{
					sprintf(buffer, "%s - %s", 
						fwMapDataStore::GetStore().GetName(strLocalIndex(pLightEntity->GetParent()->GetIplIndex())),
						pLightEntity->GetParent()->GetIsTypeDummyObject() ? "Dummy" : "Normal");

					if (pLightEntity->GetParent()->GetIsTypeDummyObject()) 
					{
						dummy++;
					}
					else
					{
						normal++;
					}

					totalPerImap[atHashString(buffer)]++;
				}
				else
				{
					fwInteriorLocation interiorLoc = pLightEntity->GetParent()->GetInteriorLocation();
					if (interiorLoc.IsValid())
					{
						CInteriorProxy *pInteriorProxy = CInteriorProxy::GetFromLocation(interiorLoc);
						totalPerImap[atHashString(pInteriorProxy->GetName().GetCStr())]++;

						normal++;
					}
					else
					{
						totalPerImap[misc]++;
						normal++;
					}
				}
			}
		}
	}

	renderDisplayf("SUMMARY");

	u32 totalLightEntities = 0;
	for (u32 i = 0; i < totalPerImap.GetNumSlots(); i++) 
	{
		atMapEntry<atHashString, u32> *e = totalPerImap.GetEntry(i);
		while (e) 
		{
			renderDisplayf("%s : %d", e->key.GetCStr(), e->data);
			totalLightEntities += e->data;
			e = e->next;
		}
	}

	renderDisplayf("TOTAL - %d (Dummy: %d, Normal: %d) (Drawable - Yes: %d, No: %d)", 
		totalLightEntities, 
		dummy, normal, 
		drawable, noDrawable);

	m_dumpedParentImaps = !allowMultipleCalls;
}

void DebugLighting::DumpLightEntityPoolStats()
{
	renderDisplayf("Light Enity Pool: size: %d, used: %d.", (s32)CLightEntity::GetPool()->GetSize(), (s32)CLightEntity::GetPool()->GetNoOfUsedSpaces());
}

// ----------------------------------------------------------------------------------------------- //

static void LightEntityFlush_button()
{
	LightEntityMgr::RequestFlush();
}

// ----------------------------------------------------------------------------------------------- //

static void ShowReflectionColour_button()
{
	if (!CRenderTargets::IsShowingRenderTarget("REFLECTION_MAP_COLOUR"))
	{
		CRenderTargets::ShowRenderTarget("REFLECTION_MAP_COLOUR");
		CRenderTargets::ShowRenderTargetBrightness(2.0f); // reflection map looks kinda dark, make it brighter
		CRenderTargets::ShowRenderTargetNativeRes(true, false);
		CRenderTargets::ShowRenderTargetFullscreen(false);
		CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
	}
	else
	{
		CRenderTargets::ShowRenderTargetReset();
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::AddWidgets()
{
	bkBank* bk = &BANKMGR.CreateBank("Lighting"); 

	const char* ambientTypeStrings[] =
	{
		"Natural Ambient",
		"Artificial Exterior Ambient",
		"Artificial Interior Ambient",
		"Directional Ambient"
	};

	bk->AddToggle("Enable Directional", &m_directionalEnable);
	for (u32 i = 0; i < AMBIENT_OVERRIDE_COUNT; i++)
	{
		char buffer[64];
		sprintf(buffer, "Enable %s", ambientTypeStrings[i]);
		bk->AddToggle(buffer, &m_ambientEnable[i]);
	}
	bk->AddToggle("Enable Local Lights", &m_lightsEnable);
	bk->AddSeparator("");
	bk->AddToggle("Override deferred", &m_overrideDeferred);
	bk->AddToggle("Override forward", &m_overrideForward);
	bk->AddSeparator("");
	bk->AddToggle("Draw wireframe", &CRenderPhaseDebugOverlayInterface::DrawEnabledRef());
	bk->AddToggle("Show lighting stats", &DebugLights::m_printStats, datCallback(DebugLights::ShowLightsDebugInfo));
	bk->AddSeparator("");

	bk->PushGroup("Directional");
		bk->AddToggle("Override", &m_directionalOverride);
		bk->AddColor("Colour", &m_directionalLightColour);
		bk->AddSlider("Intensity", &m_directionalLightIntensity, 0.0f, 128.0f, 0.1f);
	bk->PopGroup();

	for (u32 i = 0; i < AMBIENT_OVERRIDE_COUNT; i++)
	{
		bk->PushGroup(ambientTypeStrings[i]);
			bk->AddToggle("Override", &m_ambientOverride[i]);
			if (i == AMBIENT_OVERRIDE_DIRECTIONAL)
			{
				bk->AddColor("Colour", &m_baseAmbientColour[i]);
				bk->AddSlider("Intensity", &m_baseAmbientIntensity[i], 0.0f, 128.0f, 0.1f);
			}
			else
			{
				bk->AddColor("Down - Colour", &m_downAmbientColour[i]);
				bk->AddSlider("Down - Intensity", &m_downAmbientIntensity[i], 0.0f, 128.0f, 0.1f);
				bk->AddSeparator("");
				bk->AddColor("Base - Colour", &m_baseAmbientColour[i]);
				bk->AddSlider("Base - Intensity", &m_baseAmbientIntensity[i], 0.0f, 128.0f, 0.1f);
			}
		bk->PopGroup();
	}

	bk->PushGroup("Local Lights");
		bk->AddToggle("Override", &m_lightsOverride);
		bk->AddColor("Colour", &m_lightsColour);
		bk->AddSlider("Intensity", &m_lightsIntensity, 0.0f, 128.0f, 0.1f);
		bk->AddSeparator("");
		bk->AddToggle("Exterior", &m_lightsExterior);
		bk->AddToggle("Interior", &m_lightsInterior);
		bk->AddToggle("Spot", &m_lightsSpot);
		bk->AddToggle("Point", &m_lightsPoint);
		bk->AddToggle("Shadow casting", &m_lightsShadowCasting);
#if DEPTH_BOUNDS_SUPPORT
		bk->AddToggle("Use depth bounds", &m_useLightsDepthBounds);
#endif //DEPTH_BOUNDS_SUPPORT
		bk->PushGroup("Fade distances");
			bk->AddSlider("Standard", &Lights::m_lightFadeDistance, 0.0f, 1024.0f, 0.01f);
			bk->AddSlider("Shadow", &Lights::m_shadowFadeDistance, 0.0f, 1024.0f, 0.01f);
			bk->AddSlider("Specular", &Lights::m_specularFadeDistance, 0.0f, 1024.0f, 0.01f);
			bk->AddSlider("Volumetric", &Lights::m_volumetricFadeDistance, 0.0f, 1024.0f, 0.01f);
			bk->AddSlider("Non GBuffer Lights", &Lights::m_lightCullDistanceForNonGBufLights, 0.0f, 1024.0f, 0.01f);
			bk->AddSlider("Map light fade distance", &Lights::m_mapLightFadeDistance, 0.0f, 1024.0f, 0.01f);
		bk->PopGroup();
		bk->PushGroup("Light flag to show");
			bk->AddToggle("Don't use in cutscenes"				, &m_lightFlagsToShow, LIGHTFLAG_DONT_USE_IN_CUTSCENE);
			bk->AddToggle("Texture projection"					, &m_lightFlagsToShow, LIGHTFLAG_TEXTURE_PROJECTION);
			bk->AddToggle("Cast static shadows"					, &m_lightFlagsToShow, LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS);
			bk->AddToggle("Cast dynamic shadows"				, &m_lightFlagsToShow, LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS);
			bk->AddToggle("Calc from sun"						, &m_lightFlagsToShow, LIGHTFLAG_CALC_FROM_SUN);
			bk->AddToggle("Enable buzzing"						, &m_lightFlagsToShow, LIGHTFLAG_ENABLE_BUZZING);
			bk->AddToggle("Force buzzing"						, &m_lightFlagsToShow, LIGHTFLAG_FORCE_BUZZING);
			bk->AddToggle("Draw volume"							, &m_lightFlagsToShow, LIGHTFLAG_DRAW_VOLUME);
			bk->AddToggle("No specular"							, &m_lightFlagsToShow, LIGHTFLAG_NO_SPECULAR);
			bk->AddToggle("Both interior and exterior"			, &m_lightFlagsToShow, LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR);
			bk->AddToggle("Corona only"							, &m_lightFlagsToShow, LIGHTFLAG_CORONA_ONLY);
			bk->AddToggle("Not in reflections"					, &m_lightFlagsToShow, LIGHTFLAG_NOT_IN_REFLECTION);
			bk->AddToggle("Only in reflections"					, &m_lightFlagsToShow, LIGHTFLAG_ONLY_IN_REFLECTION);
			bk->AddToggle("Use volume outer colour"				, &m_lightFlagsToShow, LIGHTFLAG_USE_VOLUME_OUTER_COLOUR);
			bk->AddToggle("Cast higher resolution shadows"		, &m_lightFlagsToShow, LIGHTFLAG_CAST_HIGHER_RES_SHADOWS);
			bk->AddToggle("Use only low resolution Shadows"		, &m_lightFlagsToShow, LIGHTFLAG_CAST_ONLY_LOWRES_SHADOWS);
			bk->AddToggle("VIP LOD light"						, &m_lightFlagsToShow, LIGHTFLAG_FAR_LOD_LIGHT);
			bk->AddToggle("Don't light alpha"					, &m_lightFlagsToShow, LIGHTFLAG_DONT_LIGHT_ALPHA);
			bk->AddToggle("Cast shadow if you can"				, &m_lightFlagsToShow, LIGHTFLAG_CAST_SHADOWS_IF_POSSIBLE);
			bk->AddToggle("Interior only"						, &m_lightFlagsToShow, LIGHTFLAG_INTERIOR_ONLY);
			bk->AddToggle("Exterior only"						, &m_lightFlagsToShow, LIGHTFLAG_EXTERIOR_ONLY);
			bk->AddToggle("Vehicle"								, &m_lightFlagsToShow, LIGHTFLAG_VEHICLE);
			bk->AddToggle("VFX"									, &m_lightFlagsToShow, LIGHTFLAG_FX);
			bk->AddToggle("Cutscene"							, &m_lightFlagsToShow, LIGHTFLAG_CUTSCENE);
			bk->AddToggle("Moving"								, &m_lightFlagsToShow, LIGHTFLAG_MOVING_LIGHT_SOURCE);
			bk->AddToggle("Vehicle twin"						, &m_lightFlagsToShow, LIGHTFLAG_USE_VEHICLE_TWIN);
		bk->PopGroup();
	bk->PopGroup();

	bk->PushGroup("Phone Light");
		bk->AddToggle("Override", &m_bPedPhoneLightOverride);
		bk->AddToggle("Debug", &m_bPedPhoneLightDirDebug);
		bk->AddColor("Color", &m_pedPhoneLightColor);
		bk->AddSlider("Radius", &m_pedPhoneLightRadius, 0.0f, 1.2f, 0.05f);
		bk->AddSlider("Intensity", &m_pedPhoneLightIntensity, 0.0f, 16.0f, 0.05f);
		bk->AddSlider("Falloff Exponent", &m_pedPhoneLightFalloffExponent, 0.0f, 100.0f, 0.05f);
		bk->AddSlider("Cone Inner", &m_pedPhoneLightConeInner, 0.0f, 90.0f, 0.05f);
		bk->AddSlider("Cone Outer", &m_pedPhoneLightConeOuter, 0.0f, 90.0f, 0.05f);
	bk->PopGroup();

	const char* particleOverrideStrings[PARTICLE_OVERRIDE_COUNT] =
	{
		"Directional Multiplier",
		"Ambient Multiplier",
		"Extra Light Multiplier",
		"Shadow Amount",
		"Normal Arc",
		"Softness Curve",
		"Softness Shadow Multiplier",
		"Softness Shadow Offset"
	};

	bk->PushGroup("Particles");
		bk->AddButton("Override all", datCallback(CFA(DebugLighting::UpdateParticleOverrideAll)));
		bk->AddSeparator("");
		for (u32 i = 0; i < PARTICLE_OVERRIDE_COUNT; i++)
		{
			bk->AddToggle("Override", &m_particleOverride[i], datCallback(CFA(DebugLighting::UpdateParticleOverride)));

			if (m_particleOverrideType[i] == PTXSHADERVAR_FLOAT || m_particleOverrideType[i] == PTXSHADERVAR_KEYFRAME)
			{
				bk->AddSlider(particleOverrideStrings[i], &m_particleOverrideAmountFloat[i], 0.0f, 1.0f, 0.01f);
				if (i < PARTICLE_OVERRIDE_COUNT - 1) { bk->AddSeparator(""); }
			}
		}
	bk->PopGroup();

	bk->PushGroup("GBuffer");
		DebugDeferred::AddWidgets_GBuffer(bk);
	bk->PopGroup();

	bk->PushGroup("Forward");
		bk->AddToggle("Force forward capsule lights", &m_bForceLightweightCapsule);
		bk->AddToggle("Force high quality forward lighting", &m_bForceHighQualityForward);
		bk->AddToggle("Force All 8 Lights", &DebugDeferred::m_forceSetAll8Lights);
	bk->PopGroup();

	bk->PushGroup("Reflection");
		bk->AddButton("Show/Hide reflection map", ShowReflectionColour_button);
		bk->AddToggle("Debug mode", &DebugDeferred::m_enableCubeMapDebugMode);
		bk->AddToggle("Freeze reflection map",&DebugDeferred::m_freezeReflectionMap);
	bk->PopGroup();

	bk->PushGroup("Misc");
		bk->PushGroup("Light Volumes");
			bk->AddToggle("Force Unshadowed Light Volume Technique", &m_ForceUnshadowedVolumeLightTechnique);
			bk->AddSlider("Min Angle for BackFace Technique", &m_MinSpotAngleForBackFaceForce, 0.0f, 1.0f, 0.001f);
		#if LIGHT_VOLUME_USE_LOW_RESOLUTION
			bk->AddToggle("Volume lights using masked up-sample",&m_VolumeLightingMaskedUpsample);
		#endif
		bk->PopGroup();

		bk->PushGroup("Light entities");
			bk->AddToggle("Show debug", &m_lightEntityDebug);
			bk->AddToggle("Enable z-test for debug draw", &grcDebugDraw::g_doDebugZTest);
			bk->AddToggle("Freeze lights", &DebugLights::m_freeze);
			m_lightEntityIndexSlider = bk->AddSlider("Light entity index", &m_lightEntityIndex, -1, (s32) CLightEntity::GetPool()->GetSize(), 1, datCallback(CFA(DebugLighting::UpdateLightEntityIndex)));
			bk->AddButton("Dump parent imaps for light entities", datCallback(CFA1(DumpParentImaps), (bool*)true, true));
			bk->AddButton("Print light entity pool stats", datCallback(CFA(DumpLightEntityPoolStats)));
			bk->AddButton("Flush light entity pool", datCallback(CFA(LightEntityFlush_button)));
			bk->PopGroup();
	bk->PopGroup();

	// Add budget group
	SBudgetDisplay::GetInstance().AddWidgets(bk);

	bk->PushGroup("LOD / Distant Lights");
	{
		const char* categoryStrings[] =
		{
			"All",
			"Small",
			"Medium",
			"Large"
		};

		bk->AddToggle("Enable LOD lights", &m_LODLightRender);
		bk->AddToggle("Enable distant LOD lights", &m_distantLODLightRender);
		bk->AddSeparator();
		bk->PushGroup("Map Data");
		{
			bk->AddButton("Cache loaded LOD light IMAPs", datCallback(CFA(CLODLightManager::CacheLoadedLODLightImapIDX)));
			bk->AddButton("Load cache", LoadLODLightCache);
			bk->AddButton("Unload cache", UnloadLODLightCache);
			bk->AddToggle("Owned by Map data", &m_LODLightOwnedByMapData);
			bk->AddToggle("Owned by IMAP group", &m_LODLightOwnedByScript);
			bk->AddSeparator();
			bk->AddButton("Load IMAP groups", LoadLODLightImapGroupList);
			g_pLODLightImapGroupCombo = bk->AddCombo("Select IMAP group", &ms_selectedImapComboIndex, 0, nullptr);
			bk->AddButton("Load selected", LoadSelectedLODLightImapGroup);
			bk->AddButton("Unload selected", UnloadSelectedLODLightImapGroup);
		}
		bk->PopGroup();
		bk->AddToggle("Show LOD light memory usage", &m_showLODLightMemUsage);
		bk->AddText("LOD light memory stats", m_LODLightsString, 255, true);
		bk->AddText("Distant LOD light memory stats", m_distantLODLightsString, 255, true);
		bk->AddSeparator();
		bk->PushGroup("LOD lights");
		{
			bk->AddSlider("Bucket index", &m_LODLightBucketIndex, -1, 1024, 1);
			bk->AddSlider("Light index", &m_LODLightIndex, -1, 5120, 1);
			bk->AddCombo("Category", (int*)&m_LODLightCategory, NELEM(categoryStrings), categoryStrings);
			bk->AddToggle("Debug render", &m_LODLightDebugRender);
			bk->AddToggle("Display Physical bounds", &m_LODLightPhysicalBoundsRender);
			bk->AddToggle("Display Streaming bounds", &m_LODLightStreamingBoundsRender);
			bk->AddSlider("Scale multiplier", CLODLights::GetScaleMultiplierPtr(), 0.0f, 1.0f, 0.1f);
			bk->AddToggle("Perform Visibility on main thread", &m_LODLightsVisibilityOnMainThread);
			bk->AddToggle("Process Visibility Async from main thread", &m_LODLightsProcessVisibilityAsync);
			bk->AddToggle("Sort Lights during visibility", &m_LODLightsPerformSortDuringVisibility);			
			bk->AddToggle("Enable occlusion test", &m_LODLightsEnableOcclusionTest);
			bk->AddToggle("Perform occlusion test on main thread", &m_LODLightsEnableOcclusionTestInMainThread);
			bk->AddSlider("Num LOD Lights Rendered", CLODLights::GetLODVisibleLODLightsCountPtr(), 0, 16384, 0);
			bk->PushGroup("Coronas");
			{
				bk->AddToggle("Enable coronas", &m_LODLightCoronaEnable);
				bk->AddToggle("Enable Coronas in Water Reflection", &m_LODLightCoronaEnableWaterReflectionCheck);
				bk->AddSlider("Num LOD Light Coronas", CLODLights::GetLODLightCoronaCountPtr(), 0, CORONAS_MAX, 0);
				bk->AddSlider("Size", CLODLights::GetCoronaSizePtr(), 0.0f, 16.0f, 0.01f);
			}
			bk->PopGroup();
			bk->PushGroup("Category Info");
			{
				bk->AddToggle("Override colours", &m_LODLightOverrideColours);
				bk->PushGroup("Small");
					bk->AddSlider("Range", CLODLights::GetRangePtr(LODLIGHT_CATEGORY_SMALL), 0.0f, 5000.0f, 1.0f);
					bk->AddSlider("Fade", CLODLights::GetFadePtr(LODLIGHT_CATEGORY_SMALL), 0.0f, 5000.0f, 1.0f);
					bk->AddSlider("Corona Range", CLODLights::GetCoronaRangePtr(LODLIGHT_CATEGORY_SMALL), 0.0f, 5000.0f, 1.0f);
					bk->AddColor("Colour", &m_LODLightDebugColour[LODLIGHT_CATEGORY_SMALL], NullCB, 0, NULL, true);
				bk->PopGroup();
				bk->PushGroup("Medium");
					bk->AddSlider("Range", CLODLights::GetRangePtr(LODLIGHT_CATEGORY_MEDIUM), 0.0f, 5000.0f, 1.0f);
					bk->AddSlider("Fade", CLODLights::GetFadePtr(LODLIGHT_CATEGORY_MEDIUM), 0.0f, 5000.0f, 1.0f);
					bk->AddSlider("Corona Range", CLODLights::GetCoronaRangePtr(LODLIGHT_CATEGORY_MEDIUM), 0.0f, 5000.0f, 1.0f);
					bk->AddColor("Colour", &m_LODLightDebugColour[LODLIGHT_CATEGORY_MEDIUM], NullCB, 0, NULL, true);
				bk->PopGroup();
				bk->PushGroup("Large");
					bk->AddSlider("Range", CLODLights::GetRangePtr(LODLIGHT_CATEGORY_LARGE), 0.0f, 5000.0f, 1.0f);
					bk->AddSlider("Fade", CLODLights::GetFadePtr(LODLIGHT_CATEGORY_LARGE), 0.0f, 5000.0f, 1.0f);
					bk->AddSlider("Corona Range", CLODLights::GetCoronaRangePtr(LODLIGHT_CATEGORY_LARGE), 0.0f, 5000.0f, 1.0f);
					bk->AddColor("Colour", &m_LODLightDebugColour[LODLIGHT_CATEGORY_LARGE], NullCB, 0, NULL, true);
				bk->PopGroup();
			}
			bk->PopGroup();
		}
		bk->PopGroup();
		bk->PushGroup("Distant lights");
		{
			bk->AddSlider("Bucket index", &m_distantLODLightBucketIndex, -1, 1024, 1);
			bk->AddSlider("Light index", &m_distantLODLightIndex, -1, 5120, 1);
			bk->AddCombo("Category", (int*)&m_distantLODLightCategory, NELEM(categoryStrings), categoryStrings);
			bk->AddToggle("Debug render", &m_distantLODLightDebugRender);
			bk->AddToggle("Display Physical bounds", &m_distantLODLightPhysicalBoundsRender);
			bk->AddToggle("Display Streaming bounds", &m_distantLODLightStreamingBoundsRender);
			bk->AddToggle("Override range", &m_distantLODLightsOverrideRange);
			bk->AddSlider("Start", &m_distantLODLightsStart, 0.0f, 4096.0f, 1.0f);
			bk->AddSlider("End",   &m_distantLODLightsEnd,   0.0f, 4096.0f, 1.0f);
			bk->AddToggle("Process Visibility Async from main thread", &m_DistantLightsProcessVisibilityAsync);
			
		}
		bk->PopGroup();
		bk->PushGroup("Load extra");
		{
			bk->AddToggle("Enable load extra", &m_bLoadExtraDistantLights);
			bk->AddToggle("Display stream sphere", &m_bDisplayStreamSphere);
			bk->AddVector("Streaming centre", &m_vExtraDistantLightsCentre, -8000.0f, 8000.0f, 1.0f);
			bk->AddToggle("Set centre to cam pos", &m_bLoadExtraTrackWithCamera);
			bk->AddSlider("Streaming radius", &m_fExtraDistantLightsRadius, 1.0f, 5000.0f, 1.0f);
		}
		bk->PopGroup();
		bk->PushGroup("Debug");
		{
			bk->AddToggle("Show LOD light hashes", &m_showLODLightHashes);
			bk->AddToggle("Show LOD light types", &m_showLODLightTypes);
			bk->AddToggle("Show LOD light stats", &m_showLODLightUsage);
			bk->AddSeparator();
			bk->AddButton("Disable LOD lights for a frame", datCallback(CFA(CLODLightManager::SetUpdateLODLightsDisabledThisFrame)));
			bk->AddSeparator();
		}
		bk->PopGroup();
	}
	bk->PopGroup(); // LOD/Distant Lights
	
	bk->AddSeparator("");

	bkBank *pBank = BANKMGR.FindBank("Optimization");
	Assert(pBank);
	pBank->AddToggle("Directional light", &m_directionalEnable);
	pBank->AddToggle("Local lights", &m_lightsEnable);
	pBank->AddToggle("Exterior lights", &m_lightsExterior);
	pBank->AddToggle("Interior lights", &m_lightsInterior);
	pBank->AddToggle("Spot lights", &m_lightsSpot);
	pBank->AddToggle("Point lights", &m_lightsPoint);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UpdateLightEntityIndex()
{
	while(m_lightEntityIndex < CLightEntity::GetPool()->GetSize() && CLightEntity::GetPool()->GetIsFree(m_lightEntityIndex))
	{
		++m_lightEntityIndex;
	}
}

// ----------------------------------------------------------------------------------------------- //

bool DebugLighting::ShouldProcess(CLightSource &light)
{
	if ((!m_lightsExterior && !light.InInterior()) || !DebugDeferred::m_directEnable)
	{
		return false;
	}

	if ((!m_lightsInterior && light.InInterior()) || !DebugDeferred::m_directEnable)
	{
		return false;
	}

	if (light.GetType() == LIGHT_TYPE_POINT && !m_lightsPoint)
	{
		return false;
	}

	if (light.GetType() == LIGHT_TYPE_SPOT && !m_lightsSpot)
	{
		return false;
	}

	if (!m_lightsShadowCasting)
	{
		light.ClearFlag((LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS));
		return true;
	}

	if (gLastGenMode)
	{
		if( (light.GetType() == LIGHT_TYPE_POINT || light.GetType() == LIGHT_TYPE_SPOT) && !light.GetFlag(LIGHTFLAG_VEHICLE) )
		{
			float size = 0.75f;
			float intensity = light.GetIntensity();
			float coronaIntensity = 0.75f;
			float coronaHDRMultiplier = 1.0f;

			Color32 colour = light.GetColor() * intensity;

			size *= g_timeCycle.GetStaleSpriteSize();

			Vector3 lightPos = light.GetPosition();
			g_coronas.Register(
				RCC_VEC3V(lightPos), 
				size,
				colour,
				g_timeCycle.GetStaleSpriteBrightness() * coronaHDRMultiplier * coronaIntensity,
				size * (1.0f - g_timeCycle.GetStaleInteriorBlendStrength()),
				Vec3V(V_ZERO),
				1.0f,
				CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
				CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
				0);
		}

		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UpdateStatsLODLights(const u32 bucketCount, const s32 lightsChanged, u32 bucket)
{
	u32 bucketSize[2];
	bucketSize[LODLIGHT_STANDARD] = sizeof(CLODLightBucket);
	bucketSize[LODLIGHT_DISTANT] = sizeof(CDistantLODLightBucket);

	m_LODLightsBucketsUsed[bucket] = bucketCount;
	m_peakLODLightsBucketsUsed[bucket] = Max(m_peakLODLightsBucketsUsed[bucket], m_LODLightsBucketsUsed[bucket]);
		
	m_LODLightsOnMap[bucket] += lightsChanged;
	m_peakLODLightsOnMap[bucket] = Max(m_peakLODLightsOnMap[bucket], m_LODLightsOnMap[bucket]);

	m_LODLightsStorage[bucket] = 
		(m_LODLightsBucketsUsed[bucket] * bucketSize[bucket]) + 
		(m_LODLightsOnMap[bucket] * m_lightSize[bucket]);
	
	m_peakLODLightsStorage[bucket] = Max(m_peakLODLightsStorage[bucket], m_LODLightsStorage[bucket]);
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::UpdateLODLights()
{
	if (m_showLODLightMemUsage)
	{
		u32 bucketSize[2];
		bucketSize[LODLIGHT_STANDARD] = sizeof(CLODLightBucket);
		bucketSize[LODLIGHT_DISTANT] = sizeof(CDistantLODLightBucket);

		sprintf(m_LODLightsString, "Current = %.2fKB / Peak = %.2fKB [Buckets = %.2fKB, Lights = %.2fKB]", 
			m_LODLightsStorage[LODLIGHT_STANDARD] / 1024.0f, 
			m_peakLODLightsStorage[LODLIGHT_STANDARD] / 1024.0f,
			(m_LODLightsBucketsUsed[LODLIGHT_STANDARD] * bucketSize[LODLIGHT_STANDARD]) / 1024.0f,
			(m_LODLightsOnMap[LODLIGHT_STANDARD] * m_lightSize[LODLIGHT_STANDARD]) / 1024.0f);

		sprintf(m_distantLODLightsString, "Current = %.2fKB / Peak = %.2fKB [Buckets = %.2fKB, Lights = %.2fKB]", 
			m_LODLightsStorage[LODLIGHT_DISTANT] / 1024.0f, 
			m_peakLODLightsStorage[LODLIGHT_DISTANT] / 1024.0f,
			(m_LODLightsBucketsUsed[LODLIGHT_DISTANT] * bucketSize[LODLIGHT_DISTANT]) / 1024.0f,
			(m_LODLightsOnMap[LODLIGHT_DISTANT] * m_lightSize[LODLIGHT_DISTANT]) / 1024.0f);
	}
	else
	{
		m_LODLightsString[0] = '\0';
		m_distantLODLightsString[0] = '\0';
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::RenderLODLights()
{
	if (m_showLODLightUsage)
	{
		grcDebugDraw::AddDebugOutput(Color_white, "[LOD Lights] Buckets used = %d", m_LODLightsBucketsUsed[LODLIGHT_STANDARD]);
		grcDebugDraw::AddDebugOutput(Color_grey,  "[LOD Lights] Processed buckets = %d", m_LODLightsProcessed[LODLIGHT_STANDARD]);
		grcDebugDraw::AddDebugOutput(Color_white, "[LOD Lights] Peak buckets used = %d", m_peakLODLightsBucketsUsed[LODLIGHT_STANDARD]);
		grcDebugDraw::AddDebugOutput(Color_grey,  "[LOD Lights] Current count = %d", m_LODLightsOnMap[LODLIGHT_STANDARD]);
		grcDebugDraw::AddDebugOutput(Color_white, "[LOD Lights] Peak count = %d", m_peakLODLightsOnMap[LODLIGHT_STANDARD]);

		grcDebugDraw::AddDebugOutput(Color_grey,  "[Distant LOD Lights] Buckets used = %d", m_LODLightsBucketsUsed[LODLIGHT_DISTANT]);
		grcDebugDraw::AddDebugOutput(Color_white, "[Distant LOD Lights] Processed buckets = %d", m_LODLightsProcessed[LODLIGHT_DISTANT]);
		grcDebugDraw::AddDebugOutput(Color_grey,  "[Distant LOD Lights] Peak buckets used = %d", m_peakLODLightsBucketsUsed[LODLIGHT_DISTANT]);
		grcDebugDraw::AddDebugOutput(Color_white, "[Distant LOD Lights] Current count = %d", m_LODLightsOnMap[LODLIGHT_DISTANT]);
		grcDebugDraw::AddDebugOutput(Color_grey,  "[Distant LOD Lights] Peak count = %d", m_peakLODLightsOnMap[LODLIGHT_DISTANT]);
	}

	// Render actual lights
	const grcViewport *viewPort = gVpMan.GetRenderGameGrcViewport();
	/// const Vec3V vCameraPos = viewPort->GetCameraPosition();

	m_LODLightsProcessed[LODLIGHT_STANDARD] = 0;
	m_LODLightsProcessed[LODLIGHT_DISTANT] = 0;

	if(m_LODLightDebugRender || m_LODLightPhysicalBoundsRender || m_LODLightStreamingBoundsRender)
	{
		u32 startIndex = 0;
		u32 endIndex = CLODLightManager::GetNumLodLightBuckets();
		DebugLighting::BucketRangeLODLights(startIndex, endIndex);

		for(int i = startIndex; i < endIndex; i++)
		{
			CLODLightBucket *pBucket = CLODLightManager::GetLodLightBucket(i);
			CLODLight *pLight = pBucket->pLights;
			CDistantLODLight *pDistLight = pBucket->pDistLights;

			const spdAABB &physicalBounds = INSTANCE_STORE.GetPhysicalBounds(strLocalIndex(pBucket->mapDataSlotIndex));
			const spdAABB &streamingBounds = INSTANCE_STORE.GetStreamingBounds(strLocalIndex(pBucket->mapDataSlotIndex));
			const char *pStorename =  INSTANCE_STORE.GetName(strLocalIndex(pBucket->mapDataSlotIndex));

			bool categoryFilter = (m_LODLightCategory > 0) ? (pDistLight->m_category == m_LODLightCategory - 1) : true;

			u32 RGBA[4] = {0, 0, 0, 128};
			RGBA[pDistLight->m_category] = 255;
			Color32 col = Color32(RGBA[0], RGBA[1], RGBA[2], RGBA[3]);

			if(viewPort->IsAABBVisible(physicalBounds.GetMin().GetIntrin128(), physicalBounds.GetMax().GetIntrin128(), viewPort->GetFrustumLRTB()) &&
			   categoryFilter)
			{
				if (m_LODLightDebugRender)
				{
					u32 startLightIndex = 0;
					u32 endLightIndex = pBucket->lightCount;
					DebugLighting::LightRangeLODLights(startLightIndex, endLightIndex, pBucket->lightCount);

					for(int j = startLightIndex; j < endLightIndex; j++)
					{
						Vec3V position = Vec3V(pDistLight->m_position[j].x, pDistLight->m_position[j].y, pDistLight->m_position[j].z);
						Color32 lightColour = (CLODLights::IsActive(pLight,j)) ? col : Color32(255, 0, 255, 255);
						grcDebugDraw::Sphere(position, pLight->m_falloff[j], lightColour, true);
					}

					if (m_LODLightIndex != -1)
					{
						int idx = m_LODLightIndex;

						Vec3V position = Vec3V(
							pDistLight->m_position[idx].x, 
							pDistLight->m_position[idx].y, 
							pDistLight->m_position[idx].z);

						const packedTimeAndStateFlags& flags = (packedTimeAndStateFlags&)(pLight->m_timeAndStateFlags[idx]);

						Color32 colour;
						colour.Set(pDistLight->m_RGBI[idx]);

						float intensity = (colour.GetAlpha() * MAX_LODLIGHT_INTENSITY / 255.0f);

						Vec3V ldir = Vec3V(
							pLight->m_direction[idx].x, 
							pLight->m_direction[idx].y, 
							pLight->m_direction[idx].z);

						float falloff = pLight->m_falloff[idx];
						float falloffExponent = pLight->m_falloffExponent[idx];

						char buffer[255];
						sprintf(
							buffer, 
							"T:%d\nP:<%.2f %.2f %.2f>\nD:<%.2f %.2f %.2f>\nC:<%d %d %d>\nI:%.2f\nFD:%.2f\nFE:%.2f",
							flags.lightType,
							VEC3V_ARGS(position),
							VEC3V_ARGS(ldir),
							colour.GetRed(), colour.GetGreen(), colour.GetBlue(), 
							intensity, 
							falloff,
							falloffExponent);

						if (flags.lightType == LIGHT_TYPE_SPOT)
						{
							float innerAngle = LL_UNPACK_U8(pLight->m_coneInnerAngle[idx], MAX_LODLIGHT_CONE_ANGLE );
							float outerAngle = LL_UNPACK_U8(pLight->m_coneOuterAngleOrCapExt[idx], MAX_LODLIGHT_CONE_ANGLE);

							sprintf(buffer, "%s\nIA:%.2f\nOA:%2.f", buffer, innerAngle, outerAngle);
						}

						grcDebugDraw::Text(position, Color32(255, 255, 255, 255), 0, 0, buffer, false);
					}
				}

				if (m_LODLightPhysicalBoundsRender)
				{
					grcDebugDraw::BoxAxisAligned(
						physicalBounds.GetMin(), 
						physicalBounds.GetMax(), 
						col,
						false);

					Vec3V physicalBoundsExtent = physicalBounds.GetMax() - physicalBounds.GetMin();
					char buffer[255];
					sprintf(buffer, "Min (%.2f, %.2f, %.2f)\nMax (%.2f, %.2f, %.2f)\nL: %.2f W: %.2f\nC:%d\nIMAP:%s", 
						VEC3V_ARGS(physicalBounds.GetMin()),
						VEC3V_ARGS(physicalBounds.GetMax()),
						VEC2V_ARGS(physicalBoundsExtent),
						pDistLight->m_category,
						pStorename
						);
					grcDebugDraw::Text(physicalBounds.GetMax(), Color32(255, 255, 255, 255), 0, 0, buffer, false);
				}
			}

			if (m_LODLightStreamingBoundsRender)
			{
				if(viewPort->IsAABBVisible(streamingBounds.GetMin().GetIntrin128(), streamingBounds.GetMax().GetIntrin128(), viewPort->GetFrustumLRTB()) &&
				   categoryFilter)
				{
					grcDebugDraw::BoxAxisAligned(
						streamingBounds.GetMin(), 
						streamingBounds.GetMax(), 
						col,
						false);

					Vec3V streamingBoundsExtent = streamingBounds.GetMax() - streamingBounds.GetMin();
					char buffer[255];
					sprintf(buffer, "Min (%.2f, %.2f, %.2f)\nMax (%.2f, %.2f, %.2f)\nL: %.2f W: %.2f\nC:%d\nIMAP:%s", 
						VEC3V_ARGS(streamingBounds.GetMin()),
						VEC3V_ARGS(streamingBounds.GetMax()),
						VEC2V_ARGS(streamingBoundsExtent),
						pDistLight->m_category,
						pStorename);
					grcDebugDraw::Text(streamingBounds.GetMax(), Color32(255, 255, 255, 255), 0, 0, buffer, false);
				}
			}
		}
	}

	if(m_distantLODLightDebugRender || m_distantLODLightPhysicalBoundsRender || m_distantLODLightStreamingBoundsRender)
	{
		u32 startIndex = 0;
		u32 endIndex = CLODLightManager::GetNumDistantLodLightBuckets();
		DebugLighting::BucketRangeDistantLODLights(startIndex, endIndex);

		for(int i = startIndex; i < endIndex; i++)
		{
			CDistantLODLightBucket *pBucket = CLODLightManager::GetDistantLodLightBucket(i);
			CDistantLODLight *pDistLight = pBucket->pLights;

			const spdAABB &physicalBounds = INSTANCE_STORE.GetPhysicalBounds(strLocalIndex(pBucket->mapDataSlotIndex));
			const spdAABB &streamingBounds = INSTANCE_STORE.GetStreamingBounds(strLocalIndex(pBucket->mapDataSlotIndex));
			const char *pStorename =  INSTANCE_STORE.GetName(strLocalIndex(pBucket->mapDataSlotIndex));

			bool categoryFilter = (m_distantLODLightCategory > 0) ? (pDistLight->m_category == m_distantLODLightCategory - 1) : true;

			u32 RGBA[4] = {0, 0, 0, 128};
			RGBA[pDistLight->m_category] = 255;
			Color32 col = Color32(RGBA[0], RGBA[1], RGBA[2], RGBA[3]);

			if(viewPort->IsAABBVisible(physicalBounds.GetMin().GetIntrin128(), physicalBounds.GetMax().GetIntrin128(), viewPort->GetFrustumLRTB()) &&
			   categoryFilter)
			{
				if (m_distantLODLightDebugRender)
				{
					u32 startLightIndex = 0;
					u32 endLightIndex = pBucket->lightCount;
					DebugLighting::LightRangeDistantLODLights(startLightIndex, endLightIndex, pBucket->lightCount);

					for(int j = startLightIndex; j < endLightIndex; j++)
					{
						Vec3V position = Vec3V(pDistLight->m_position[j].x, pDistLight->m_position[j].y, pDistLight->m_position[j].z);
						grcDebugDraw::Sphere(position, 0.5f, col, false);
					}
				}

				if (m_distantLODLightPhysicalBoundsRender)
				{
					grcDebugDraw::BoxAxisAligned(
						physicalBounds.GetMin(), 
						physicalBounds.GetMax(), 
						col, 
						false);

					Vec3V physicalBoundsExtent = physicalBounds.GetMax() - physicalBounds.GetMin();
					char buffer[255];
					sprintf(buffer, "Min (%.2f, %.2f, %.2f)\nMax (%.2f, %.2f, %.2f)\nL: %.2f W: %.2f\nC:%d\nIMAP:%s", 
						VEC3V_ARGS(physicalBounds.GetMin()),
						VEC3V_ARGS(physicalBounds.GetMax()),
						VEC2V_ARGS(physicalBoundsExtent),
						pDistLight->m_category,
						pStorename);
					grcDebugDraw::Text(physicalBounds.GetMax(), Color32(255, 255, 255, 255), 0, 0, buffer, false);
				}
			}

			if (m_distantLODLightStreamingBoundsRender)
			{
				if(viewPort->IsAABBVisible(streamingBounds.GetMin().GetIntrin128(), streamingBounds.GetMax().GetIntrin128(), viewPort->GetFrustumLRTB()) && 
				   categoryFilter)
				{
					grcDebugDraw::BoxAxisAligned(
						streamingBounds.GetMin(), 
						streamingBounds.GetMax(), 
						col, 
						false);

					Vec3V streamingBoundsExtent = streamingBounds.GetMax() - streamingBounds.GetMin();
					char buffer[255];
					sprintf(buffer, "Min (%.2f, %.2f, %.2f)\nMax (%.2f, %.2f, %.2f)\nL: %.2f W: %.2f\nC:%d\nIMAP:%s", 
						VEC3V_ARGS(streamingBounds.GetMin()),
						VEC3V_ARGS(streamingBounds.GetMax()),
						VEC2V_ARGS(streamingBoundsExtent),
						pDistLight->m_category,
						pStorename);
					grcDebugDraw::Text(streamingBounds.GetMax(), Color32(255, 255, 255, 255), 0, 0, buffer, false);
				}
			}
		}
	}

	// Show hashes
	if (m_showLODLightHashes || m_showLODLightTypes)
	{
		u32 startIndex = 0;
		u32 endIndex = CLODLightManager::GetNumLodLightBuckets();
		BANK_ONLY(DebugLighting::BucketRangeLODLights(startIndex, endIndex);)
			char buffer[255];

		for(int i = startIndex; i < endIndex; i++)
		{
			CLODLightBucket *pBucket = CLODLightManager::GetLodLightBucket(i);
			CLODLight *pLight = pBucket->pLights;
			CDistantLODLight *pDistLight = pBucket->pDistLights;

			const spdAABB &physicalBounds = INSTANCE_STORE.GetPhysicalBounds(strLocalIndex(pBucket->mapDataSlotIndex));

			if(viewPort->IsAABBVisible(physicalBounds.GetMin().GetIntrin128(), physicalBounds.GetMax().GetIntrin128(), viewPort->GetFrustumLRTB()))
			{	
				for(int j = 0; j < pBucket->lightCount; j++)
				{
					Vec3V vPosition = Vec3V(pDistLight->m_position[j].x, pDistLight->m_position[j].y, pDistLight->m_position[j].z);

					if (m_showLODLightHashes)
					{
						sprintf(buffer, "LOD light hash: %u", pLight->m_hash[j]);
						grcDebugDraw::Text(vPosition, Color32(255, 255, 255, 255), 0, -10, buffer, true);
					}

					if (m_showLODLightTypes)
					{
						packedTimeAndStateFlags &flags = (packedTimeAndStateFlags&)(pLight->m_timeAndStateFlags[j]);
						switch(flags.lightType)
						{
							case LIGHT_TYPE_POINT: sprintf(buffer, "Point"); break;
							case LIGHT_TYPE_SPOT: sprintf(buffer, "Spot"); break;
							case LIGHT_TYPE_CAPSULE: sprintf(buffer, "Capsule"); break;
							default: sprintf(buffer, "Unknown"); break;
						}
					
						grcDebugDraw::Text(vPosition, Color32(255, 255, 255, 255), 0, -20, buffer, true);
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::BucketRangeLODLights(u32 &startIndex, u32 &endIndex)
{
	if (m_LODLightBucketIndex != -1)
	{
		m_LODLightBucketIndex = Min<s32>(m_LODLightBucketIndex, CLODLightManager::GetNumLodLightBuckets() - 1);
		startIndex = m_LODLightBucketIndex;
		endIndex = m_LODLightBucketIndex + 1;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::BucketRangeDistantLODLights(u32 &startIndex, u32 &endIndex)
{
	if (m_distantLODLightBucketIndex != -1)
	{
		m_distantLODLightBucketIndex = Min<s32>(m_distantLODLightBucketIndex, CLODLightManager::GetNumDistantLodLightBuckets() - 1);
		startIndex = m_distantLODLightBucketIndex;
		endIndex = m_distantLODLightBucketIndex + 1;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverrideLODLight(CLightSource &lightSource, u32 bucketIndex, eLodLightCategory category)
{
	if (m_LODLightOverrideColours)
	{
		lightSource.SetColor(m_LODLightDebugColour[category]);
	}

	if (m_LODLightBucketIndex > -1 && bucketIndex == (u32)m_LODLightBucketIndex)
	{
		lightSource.SetIntensity(0.0f);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::LightRangeLODLights(u32 &startIndex, u32 &endIndex, const u32 totalCount)
{
	if (m_LODLightIndex != -1)
	{
		m_LODLightIndex = Min<s32>(m_LODLightIndex, totalCount - 1);
		startIndex = (u32)m_LODLightIndex;
		endIndex = (u32)m_LODLightIndex + 1;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::LightRangeDistantLODLights(u32 &startIndex, u32 &endIndex, const u32 totalCount)
{
	if (m_distantLODLightIndex != -1)
	{
		m_distantLODLightIndex = Min<s32>(m_distantLODLightIndex, totalCount - 1);
		startIndex = (u32)m_distantLODLightIndex;
		endIndex = (u32)m_distantLODLightIndex + 1;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags)
{
	if ( m_bLoadExtraDistantLights && (supportedAssetFlags&fwBoxStreamerAsset::FLAG_MAPDATA_LOW)!=0 )
	{
		if (m_bLoadExtraTrackWithCamera)
		{
			m_vExtraDistantLightsCentre = VECTOR3_TO_VEC3V( camInterface::GetPos() );
		}
		spdSphere searchSphere(m_vExtraDistantLightsCentre, ScalarV(m_fExtraDistantLightsRadius));
		searchList.Grow() = fwBoxStreamerSearch(
			searchSphere,
			fwBoxStreamerSearch::TYPE_DEBUG,
			fwBoxStreamerAsset::FLAG_MAPDATA_LOW,
			false
		);
	}
}

// ----------------------------------------------------------------------------------------------- //

static bool isLightingPhase()
{
	return (sysThreadType::IsUpdateThread()) ? 
		gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_LIGHTING) : 
		gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_LIGHTING);
}

// ----------------------------------------------------------------------------------------------- //

bool DebugLighting::ShouldOverrideLightValue()
{
	return ((isLightingPhase() && m_overrideDeferred) || (!isLightingPhase() && m_overrideForward));
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverrideAmbient(Vector4* amb, eAmbientOverrideTypes ambientType)
{
	if (ambientType == AMBIENT_OVERRIDE_DIRECTIONAL)
	{
		float bakeAdjust = amb->GetW();

		if (!m_ambientEnable[ambientType] && ShouldOverrideLightValue())
		{
			*amb = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (m_ambientOverride[ambientType] && ShouldOverrideLightValue())
		{
			*amb = Vector4(
				m_baseAmbientColour[ambientType].x, 
				m_baseAmbientColour[ambientType].y, 
				m_baseAmbientColour[ambientType].z, 
				0.0f) * m_baseAmbientIntensity[ambientType];
		}

		amb->SetW(bakeAdjust);
	}
	else
	{
		if (!m_ambientEnable[ambientType] && ShouldOverrideLightValue())
		{
			amb[0] = Vector4(0.0f, 0.0f, 0.0f, amb[0].w);
			amb[1] = Vector4(0.0f, 0.0f, 0.0f, amb[1].w);
		}
		else if (m_ambientOverride[ambientType] && ShouldOverrideLightValue())
		{
			const Vector3 downAmbient = m_downAmbientColour[ambientType] * m_downAmbientIntensity[ambientType];
			const Vector3 baseAmbient = m_baseAmbientColour[ambientType] * m_baseAmbientIntensity[ambientType];

			amb[0] = Vector4(downAmbient.x, downAmbient.y, downAmbient.z, amb[0].w);
			amb[1] = Vector4(baseAmbient.x, baseAmbient.y, baseAmbient.z, amb[1].w);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverrideDeferredLightParams(Vector4* colourAndIntensity, const CLightSource* light)
{
	if (!m_lightsEnable && ShouldOverrideLightValue())
	{
		*colourAndIntensity = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else if (m_lightsOverride && ShouldOverrideLightValue())
	{
		*colourAndIntensity = Vector4(
			m_lightsColour.x,
			m_lightsColour.y,
			m_lightsColour.z,
			m_lightsIntensity);
	}
	else if (m_lightFlagsToShow > 0 && ShouldOverrideLightValue())
	{
		if ((light->GetFlags() & m_lightFlagsToShow) > 0)
		{
			*colourAndIntensity = Vector4(
				m_lightsColour.x,
				m_lightsColour.y,
				m_lightsColour.z,
				m_lightsIntensity);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverrideForwardLightParams(Vec4V* params)
{
	if (!m_lightsEnable && ShouldOverrideLightValue())
	{
		params[9] = Vec4V(V_ZERO);
		params[10] = Vec4V(V_ZERO);
		params[11] = Vec4V(V_ZERO);
	}
	else if (m_lightsOverride && ShouldOverrideLightValue())
	{
		VecBoolV lightMask = 
			Or(IsGreaterThan(params[9],  Vec4V(V_ZERO)), 
				Or(IsGreaterThan(params[10], Vec4V(V_ZERO)), 
					IsGreaterThan(params[11], Vec4V(V_ZERO))));
		
		Vector3 premult = m_lightsColour * m_lightsIntensity;
		Vec3V vPremult = RCC_VEC3V(premult);

		Vec4V redColour = Vec4V(SplatX(vPremult));
		Vec4V greenColour = Vec4V(SplatY(vPremult));
		Vec4V blueColour = Vec4V(SplatZ(vPremult));

		params[9]  = SelectFT(lightMask, params[9],  redColour);
		params[10] = SelectFT(lightMask, params[10], greenColour);
		params[11] = SelectFT(lightMask, params[11], blueColour);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverrideDirectional(Vector4* colour)
{
	if (!m_directionalEnable && ShouldOverrideLightValue())
	{
		*colour = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else if (m_directionalOverride && ShouldOverrideLightValue())
	{
		*colour = Vector4(
			m_directionalLightColour.x,
			m_directionalLightColour.y,
			m_directionalLightColour.z,
			0.0f) * m_directionalLightIntensity;
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverridePedPhoneLight(CLightSource* pedPhoneLight)
{
	if(m_bPedPhoneLightOverride)
	{
		pedPhoneLight->SetColor(m_pedPhoneLightColor);
		pedPhoneLight->SetIntensity(m_pedPhoneLightIntensity);
		pedPhoneLight->SetRadius(m_pedPhoneLightRadius);
		pedPhoneLight->SetFalloffExponent(m_pedPhoneLightFalloffExponent);
		pedPhoneLight->SetSpotlight(m_pedPhoneLightConeInner, m_pedPhoneLightConeOuter);
	}

	if(m_bPedPhoneLightDirDebug)
	{
		grcDebugDraw::Sphere(pedPhoneLight->GetPosition(), 0.05f, Color_azure, true);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(pedPhoneLight->GetPosition()), 
							VECTOR3_TO_VEC3V(pedPhoneLight->GetPosition() + pedPhoneLight->GetDirection()), 0.2f, Color_aquamarine);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(pedPhoneLight->GetPosition()), 
							VECTOR3_TO_VEC3V(pedPhoneLight->GetPosition() + pedPhoneLight->GetTangent()), 0.2f, Color_cyan);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugLighting::OverrideParticleShaderVars(ptxInstVars* instVars)
{
	for(u32 i = 0; i < instVars->GetCount(); i++)
	{
		ptxShaderVar* pShaderVar = (*instVars)[i];

		for (u32 j = 0; j < PARTICLE_OVERRIDE_COUNT; j++)
		{
			if (m_particleOverride[j] && pShaderVar->GetHashName() == m_particleVarHashes[j])
			{
				if (pShaderVar->GetType() == PTXSHADERVAR_FLOAT)
				{
					ptxShaderVarVector* pShaderVarVector = static_cast<ptxShaderVarVector*>(pShaderVar);
					pShaderVarVector->SetFloat(m_particleOverrideAmountFloat[j]); 
				}
				else if (pShaderVar->GetType() == PTXSHADERVAR_KEYFRAME)
				{
					ptxShaderVarKeyframe* pShaderVarKeyframe = static_cast<ptxShaderVarKeyframe*>(pShaderVar);
					ptxKeyframe& keyFrame = pShaderVarKeyframe->GetKeyframe();

					const u32 numEntries = keyFrame.GetNumEntries();

					if (numEntries == 0)
					{
						ScalarV val = ScalarV(m_particleOverrideAmountFloat[j]);
						keyFrame.AddEntry(ScalarV(V_ZERO), Vec4V(val));
					}
					else
					{
						for (u32 entryId = 0; entryId < keyFrame.GetNumEntries(); entryId++)
						{
							ScalarV entryTime =  keyFrame.GetTimeV(entryId);
							ScalarV val = ScalarV(m_particleOverrideAmountFloat[j]);
							keyFrame.SetEntry(entryId, entryTime.Getf(), Vec4V(val));
						}
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

#endif // __BANK

