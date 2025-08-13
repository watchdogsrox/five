///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DistantLights2.cpp
//	BY	: 	Thomas Randall (Adapted from original by Mark Nicholson)
//	FOR	:	Rockstar North
//	ON	:	11 Jun 2013
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////
//
//	Deals with streetlights and headlights being rendered miles into the
//	distance. The idea is to idenetify roads that have these lights on them
//	during the patch generation phase. These lines then get stored with the
//	world blocks and sprites are rendered along them whilst rendering
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////
#include "DistantLights2.h"
#include "DistantLightsVertexBuffer.h"

// The node generation tool needs this object in DEV builds no matter which light set we are using!
#if USE_DISTANT_LIGHTS_2 || RSG_DEV

// Rage header
#if ENABLE_DISTANT_CARS
#include "atl/array.h"
#include "data/struct.h"
#endif //ENABLE_DISTANT_CARS

#include "grcore/allocscope.h"
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"
#include "system/endian.h"
#include "system/param.h"
#include "system/timer.h"

// Framework headers
#include "vfx/channel.h"
#include "vfx/vfxutil.h"

//#include "fwmaths/Maths.h"
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
#include "fwscene/scan/scan.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/world/WorldLimits.h"
#include "vfx/vfxwidget.h"

// Game header
#include "Camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Camera/viewports/ViewportManager.h"
#include "Core/Game.h"
#include "Debug/Rendering/DebugLighting.h"
#include "Debug/VectorMap.h"
#include "Game/Clock.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Live/NetworkTelemetry.h"
#if ENABLE_DISTANT_CARS
#include "game/ModelIndices.h"
#include "Peds/PlayerInfo.h"
#include "Peds/ped.h"
#endif //ENABLE_DISTANT_CARS
#include "Renderer/Lights/LODLights.h"
#include "Renderer/Mirrors.h"
#include "Renderer/Sprite2d.h"
#include "Renderer/Occlusion.h"
#include "Renderer/Water.h"
#include "Renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "Renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "Scene/DataFileMgr.h"
#include "scene/lod/LodMgr.h"
#include "Scene/lod/LodScale.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderLib.h"
#include "System/FileMgr.h"
#include "System/System.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "Vfx/VfxHelper.h"
#include "Vehicles/vehiclepopulation.h"

#include "security/ragesecgameinterface.h"
#include "security/ragesecengine.h"

#define DISTANT_LIGHTS2_TEMP_DATA_PATH	"common:/data/paths/distantlights_hd.dat"

// The maximum points to use for generation. NOTE: Groups must not exceed MAX_POINTS_PER_GROUP by the time they are saved out!
#define MAX_GENERATION_POINT_PER_GROUP		(8000)

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()
	AI_OPTIMISATIONS()

	XPARAM(buildpaths);
XPARAM(nolodlights);
#if ENABLE_DISTANT_CARS && __BANK
PARAM(disableDistantCars, "enable distant cars");
#endif

#if __BANK
PARAM(noDistantLightAutoLoad, "Disables auto loading distant lights/cars data.");
#endif // __BANK

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#if !__PS3
#define __builtin_expect(exp, hint) exp
#endif 

#if __BANK
float	CDistantLights2::ms_NodeGenBuildNodeRage = 75.0f;
float	CDistantLights2::ms_NodeGenMaxAngle = 0.98f;
float	CDistantLights2::ms_NodeGenMinGroupLength = 100.0f; // meters
#endif

distLight CDistantLights2::sm_carLight1 = {Vec3V(V_ONE),33.0f, 43.0f};
distLight CDistantLights2::sm_carLight2 = {Vec3V(V_X_AXIS_WZERO),45.5f, 57.0f};
distLight CDistantLights2::sm_streetLight = {Vec3V(1.0f,0.86f,0.5f),30.0f, 0.0f};

// We use a Vec2V so we do not need to cast.
const Vec2V CDistantLights2::sm_InitialTimeOffset = Vec2VFromF32(10000.0f);

#if ENABLE_DISTANT_CARS
grcEffectVar	CDistantLights2::sm_DistantCarGeneralParams = grcevNONE;
grcEffectVar	CDistantLights2::sm_DistantCarTexture = grcevNONE;

grcVertexDeclaration*	CDistantLights2::sm_pDistantCarsVtxDecl = NULL;
fwModelId CDistantLights2::sm_DistantCarModelId;
CBaseModelInfo *CDistantLights2::sm_DistantCarBaseModelInfo = NULL;
fwModelId CDistantLights2::sm_DistantCarNightModelId;
CBaseModelInfo *CDistantLights2::sm_DistantCarBaseModelInfoNight = NULL;

bank_float CDistantLights2::ms_redLightFadeDistance   = 60.0f;
bank_float CDistantLights2::ms_whiteLightFadeDistance = 60.0f;
bank_float CDistantLights2::ms_distantCarsFadeZone = 10.0f;
bank_bool g_EnableDistantCarInnerFade = true;
bank_bool CDistantLights2::ms_debugDistantCars = false;

rage::Color32 CDistantLights2::sm_CarColors[] =
{
	Color32(255, 255, 255),
	Color32(0, 0, 0),
	Color32(150, 26, 26),
	Color32(52, 40, 176),
	Color32(240, 242, 116),
	Color32(100, 64, 122),
	Color32(61, 58, 58),
	Color32(37, 125, 34),
	Color32(105, 78, 69),
	Color32(255, 174, 0)
};

// This will be calculated from the car model later.
rage::ScalarV CDistantLights2::sm_CarLength = rage::ScalarV(V_ZERO);

sysIpcSema				CDistantLights2::sm_RenderThreadSync;
bool					CDistantLights2::sm_RequiresRenderThreadSync = false;
sysIpcCurrentThreadId	CDistantLights2::sm_DistantCarRenderThreadID = 0;

#if RSG_BANK
rage::ScalarV CDistantLights2::sm_CarWidth = rage::ScalarV(V_ZERO);
#endif // RSG_BANK

#endif //ENABLE_DISTANT_CARS

#if __PPU
DECLARE_FRAG_INTERFACE(DistantLightsVisibility);
#endif

bool g_hack_oldDistantCarsSync = false;

Vec4V laneScalars1 = Vec4V(0.0f, 0.2f, 0.0f, -0.1f);
Vec4V laneScalars2 = Vec4V(1.1f, 0.2f, 0.9f, -0.2f);
Vec4V laneScalars3 = Vec4V(-0.22f,-2.15f,-0.47f,1.62f);
Vec4V laneScalars4 = Vec4V(-3.04f, -0.64f, 1.32f, 3.32f);

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CDistantLights2::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		m_numPoints = 0;
		m_numGroups = 0;

#if RSG_BANK
		m_NumRenderedWhiteLights = 0;
		m_NumRenderedRedLights = 0;
		m_drawDistantLightsLocations = false;
		m_drawDistantLightsNodes = false;
		m_drawGroupLocations = false;
		m_drawBlockLocations = false;

		m_disableStreetLightsVisibility = false;
		m_disableDepthTest = false;
		m_bDisableRendering = false;

		m_displayVectorMapAllGroups = false;
		m_displayVectorMapActiveGroups = false;
		m_displayVectorMapBlocks = false;
		m_displayVectorMapActiveCars = false;

#if ENABLE_DISTANT_CARS
		m_displayVectorMapFadingCars = false;
		m_displayVectorMapHiddenCars = false;
#endif // ENABLE_DISTANT_CARS

		m_renderActiveGroups = false;

		m_overrideCoastalAlpha = -0.01f;
		m_overrideInlandAlpha = -0.01f;
		m_statusText[0] = '\0';
#endif

        m_disableDistantCarsByScript = false;
		m_disableVehicleLights = false;
		m_disableStreetLights = !PARAM_nolodlights.Get();

		g_DistantLightsVisibilityDependency.Init( DistantLightsVisibility::RunFromDependency, 0, 0 );
		m_DistantLightsDataDependency.Init(DistantLights2RunFromDependency, 0, 0);
		m_DistantLightsDataDependency.AddChild(g_DistantLightsVisibilityDependency);

#if ENABLE_DISTANT_CARS		
		m_DistantCarsTechnique = grcetNONE;
		m_CreatedVertexBuffers = false;
		sm_CarLength = ScalarV(V_ZERO);

#if __BANK
		m_disableDistantCars = PARAM_disableDistantCars.Get();
#else	
		m_disableDistantCars = false;
#endif // __BANK

		m_softLimitDistantCars = DISTANTCARS_MAX;

		m_DistantCarsShader = grmShaderFactory::GetInstance().Create();
		Assert(m_DistantCarsShader);
		m_DistantCarsShader->Load("im");

		m_DistantCarsTechnique = m_DistantCarsShader->LookupTechnique("DistantCars");
		sm_DistantCarGeneralParams = m_DistantCarsShader->LookupVar("GeneralParams0");
		sm_DistantCarTexture = m_DistantCarsShader->LookupVar("NoiseTexture");

		Assert(m_DistantCarsTechnique);

		const grcVertexElement DistantCarsVtxElements[] =
		{
			grcVertexElement(0, grcVertexElement::grcvetPosition, 0, 12, grcFvf::grcdsFloat3),
			grcVertexElement(0, grcVertexElement::grcvetNormal,   0, 12,  grcFvf::grcdsFloat3),
			grcVertexElement(0, grcVertexElement::grcvetColor,    0, 4,  grcFvf::grcdsColor),
			grcVertexElement(0, grcVertexElement::grcvetTexture,  0, 8,  grcFvf::grcdsFloat2),
#if __D3D11 || RSG_ORBIS
			grcVertexElement(1, grcVertexElement::grcvetTexture,  1, 12,  grcFvf::grcdsFloat3,  grcFvf::grcsfmIsInstanceStream, 1),
			grcVertexElement(1, grcVertexElement::grcvetTexture,  2, 12,  grcFvf::grcdsFloat3,  grcFvf::grcsfmIsInstanceStream, 1),
			grcVertexElement(1, grcVertexElement::grcvetColor,    1, 4,   grcFvf::grcdsColor,   grcFvf::grcsfmIsInstanceStream, 1),
#else
			grcVertexElement(1, grcVertexElement::grcvetTexture,  1, 12,  grcFvf::grcdsFloat3),
			grcVertexElement(1, grcVertexElement::grcvetTexture,  2, 12,  grcFvf::grcdsFloat3),
#endif
		};

		sm_pDistantCarsVtxDecl = GRCDEVICE.CreateVertexDeclaration(DistantCarsVtxElements, NELEM(DistantCarsVtxElements));

		m_CreatedVertexBuffers = false;
		CreateBuffers();

		sm_RenderThreadSync = sysIpcCreateSema(true);

#endif //ENABLE_DISTANT_CARS

#if __BANK
		m_pTempNodes = NULL;
		m_pTempLinks = NULL;
		m_numTempLinks = 0;
		m_currentLinkIndex = 0;
		m_msToGeneratePerFrame = 200.0f;
		m_renderTempCacheLinks = false;
		m_maxGroupGeneration = DISTANTLIGHTS2_MAX_GROUPS;

		// debug road generation options.
		m_generateDebugGroup = false;
		m_debugGroupXStart = 0;
		m_debugGroupYStart = 0;
		m_debugGroupZStart = 100;
		m_debugGroupLength = 500;
		m_debugGroupLinkLength = 10;

		m_nodeGenerationData.clear();
		m_nodeGenerationData.Resize(DISTANTLIGHTS2_MAX_POINTS);

		ChangeState(STATUS_FINISHED);
#endif // __BANK

		m_RandomVehicleDensityMult = 1.0f;

		m_distantDataRenderBuffer = 0;

		//@@: range CDISTANTLIGHTS2_INIT_ADD_BONDER {
		RAGE_SEC_GAME_PLUGIN_BONDER_ADD
		//@@: } CDISTANTLIGHTS2_INIT_ADD_BONDER

	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateVisualDataSettings
///////////////////////////////////////////////////////////////////////////////

void CDistantLights2::UpdateVisualDataSettings()
{
	// TODO -- fix the order of these, and the struct definition, to match the rag widgets
	g_distLightsParam.size						= g_visualSettings.Get("distantlights.size",						0.8f);
	g_distLightsParam.sizeReflections			= g_visualSettings.Get("distantlights.sizeReflections",				1.6f);
	g_distLightsParam.sizeMin					= g_visualSettings.Get("distantlights.sizeMin",						6.0f);
	g_distLightsParam.sizeUpscale				= g_visualSettings.Get("distantlights.sizeUpscale",					1.0f);
	g_distLightsParam.sizeUpscaleReflections	= g_visualSettings.Get("distantlights.sizeUpscaleReflections",		2.0f);
	g_distLightsParam.streetLightHdrIntensity	= g_visualSettings.Get("distantlights.streetlight.HDRIntensity",	4.0f);
	g_distLightsParam.carLightHdrIntensity		= g_visualSettings.Get("distantlights.carlight.HDRIntensity",		0.5f);
	g_distLightsParam.carLightFarFadeDist		= g_visualSettings.Get("distantlights.carlight.farFade",			300.0f);
	g_distLightsParam.flicker					= g_visualSettings.Get("distantlights.flicker",						0.33f);
	g_distLightsParam.twinkleSpeed				= g_visualSettings.Get("distantlights.twinkleSpeed",				1.0f);
	g_distLightsParam.twinkleAmount				= g_visualSettings.Get("distantlights.twinkleAmount",				0.0f);
	g_distLightsParam.carLightZOffset			= g_visualSettings.Get("distantlights.carlightZOffset",				2.0f);
	g_distLightsParam.streetLightZOffset		= g_visualSettings.Get("distantlights.streetlight.ZOffset",			12.0f);
	g_distLightsParam.inlandHeight				= g_visualSettings.Get("distantlights.inlandHeight",				75.0f);
	g_distLightsParam.hourStart					= g_visualSettings.Get("distantlights.hourStart",					19.0f);
	g_distLightsParam.hourEnd					= g_visualSettings.Get("distantlights.hourEnd",						6.0f);
	g_distLightsParam.streetLightHourStart		= g_visualSettings.Get("distantlights.streetLightHourStart",		19.0f);
	g_distLightsParam.streetLightHourEnd		= g_visualSettings.Get("distantlights.streetLightHourEnd",			6.0f);

	g_distLightsParam.sizeDistStart				= g_visualSettings.Get("distantlights.sizeDistStart",				40.0f);
	g_distLightsParam.sizeDist					= g_visualSettings.Get("distantlights.sizeDist",					70.0f);

	g_distLightsParam.speed[0]					= g_visualSettings.Get("distantlights.speed0",						0.2f);
	g_distLightsParam.speed[1]					= g_visualSettings.Get("distantlights.speed1",						0.35f);
	g_distLightsParam.speed[2]					= g_visualSettings.Get("distantlights.speed2",						0.4f);
	g_distLightsParam.speed[3]					= g_visualSettings.Get("distantlights.speed3",						0.6f);

	g_distLightsParam.randomizeSpeedSP			= g_visualSettings.Get("distantlights.randomizeSpeed.sp",			0.5f);
	g_distLightsParam.randomizeSpacingSP		= g_visualSettings.Get("distantlights.randomizeSpacing.sp",			0.25f);

	g_distLightsParam.randomizeSpeedMP			= g_visualSettings.Get("distantlights.randomizeSpeed.mp",			0.5f);
	g_distLightsParam.randomizeSpacingMP		= g_visualSettings.Get("distantlights.randomizeSpacing.mp",			0.25f);

	sm_carLight1.spacingSP						= g_visualSettings.Get("distantlights.carlight1.spacing.sp",		33.0f);
	sm_carLight1.speedSP						= g_visualSettings.Get("distantlights.carlight1.speed.sp",			43.0f);
	sm_carLight1.spacingMP						= g_visualSettings.Get("distantlights.carlight1.spacing.mp",		33.0f);
	sm_carLight1.speedMP						= g_visualSettings.Get("distantlights.carlight1.speed.mp",			43.0f);
	sm_carLight1.vColor							= g_visualSettings.GetColor("distantlights.carlight1.color");

	sm_carLight2.spacingSP						= g_visualSettings.Get("distantlights.carlight2.spacing.sp",		45.5f);
	sm_carLight2.speedSP						= g_visualSettings.Get("distantlights.carlight2.speed.sp",			57.0f);
	sm_carLight2.spacingMP						= g_visualSettings.Get("distantlights.carlight2.spacing.mp",		45.5f);
	sm_carLight2.speedMP						= g_visualSettings.Get("distantlights.carlight2.speed.mp",			57.0f);
	sm_carLight2.vColor							= g_visualSettings.GetColor("distantlights.carlight2.color");

	sm_streetLight.spacingSP					= g_visualSettings.Get("distantlights.streetlight.Spacing",		30.0f);
	sm_streetLight.vColor						= g_visualSettings.GetColor("distantlights.streetlight.color");
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights2::Shutdown			(unsigned shutdownMode)
{	
	if( shutdownMode == SHUTDOWN_CORE)
	{
#if __BANK
		ClearBuildData();
#endif // __BANK

#if ENABLE_DISTANT_CARS
		if (sm_pDistantCarsVtxDecl)
		{
			GRCDEVICE.DestroyVertexDeclaration(sm_pDistantCarsVtxDecl);
			sm_pDistantCarsVtxDecl = NULL;
		}

		DestroyBuffers();
#endif //ENABLE_DISTANT_CARS
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights2::Update				()
{
	DISTANTLIGHTS_PF_FUNC(Update);

	//@@: range CDISTANTLIGHTS2_UPDATE_PLUGIN_WORK {
	RAGE_SEC_GAME_PLUGIN_BONDER_CHECK_REBALANCE
	RAGE_SEC_GAME_PLUGIN_BONDER_UPDATE
	//@@: } CDISTANTLIGHTS2_UPDATE_PLUGIN_WORK


#if ENABLE_DISTANT_CARS

#if GTA_REPLAY
	//On replay we record this value and set it directly.
	if( !CReplayMgr::IsReplayInControlOfWorld() )
#endif
	{
		m_RandomVehicleDensityMult = CVehiclePopulation::GetRandomVehDensityMultiplier();
	}

	sm_DistantCarBaseModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("Prop_distantcar_day",0x74019666), &sm_DistantCarModelId);
	if (Verifyf(sm_DistantCarBaseModelInfo, "No ModelInfo"))
	{
		if (!sm_DistantCarBaseModelInfo->GetHasLoaded())
		{
			if (!CModelInfo::HaveAssetsLoaded(sm_DistantCarModelId))
			{
				CModelInfo::RequestAssets(sm_DistantCarModelId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE|STRFLAG_PRIORITY_LOAD);
			}
		}
	}
	sm_DistantCarBaseModelInfoNight = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("Prop_distantcar_night",0xeda96f47), &sm_DistantCarNightModelId);
	if (Verifyf(sm_DistantCarBaseModelInfoNight, "No ModelInfo"))
	{
		if (!sm_DistantCarBaseModelInfoNight->GetHasLoaded())
		{
			if (!CModelInfo::HaveAssetsLoaded(sm_DistantCarNightModelId))
			{
				CModelInfo::RequestAssets(sm_DistantCarNightModelId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE|STRFLAG_PRIORITY_LOAD);
			}
		}
	}

	// Putting the check and reaction in an update loop
	// And only care if we're in multiplayer.
#if RSG_PC && RSG_CPU_X64
	//@@: range CDISTANTLIGHTS2_UPDATE_CHECK_MODIFIED_VFT {
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress() 
		&& !NetworkInterface::IsInSinglePlayerPrivateSession();

	if(isMultiplayer)
	{
		
		// Get the set that we have, and only execute if we have data.
		// This will let us short-circuit if we've ever detected something
		// so we're not constantly grinding and sending data repeatedly.
		//@@: location CDISTANTLIGHTS2_UPDATE_GET_PROG_RUN_FUNCTIONS
		atMap<sysObfuscated<u32>, sysObfuscated<u64>> &prf = scrThread::GetProgRunFunctions();
		if(prf.GetNumUsed() > 0)
		{
			//@@: range CDISTANTLIGHTS2_UPDATE_CHECK_MODIFIED_VFT_INNER {
			// Get our threads
			atArray<scrThread*> &threads = scrThread::GetScriptThreads();
			// Get our size
			unsigned int size = threads.GetCount();
			// Pick a random element in the thread pool
			unsigned int randnum = fwRandom::GetRandomNumberInRange(0, size);

			// Pull down what the object thinks is it's v-table.
			//@@: location CDISTANTLIGHTS2_UPDATE_GET_RUNTIME_VTABLE
			DWORD64* vtablePtr = (DWORD64*)((DWORD64*)threads[randnum])[0];
			vtablePtr+=2;

			// Create our SO vars
			sysObfuscated<u32> programSO(threads[randnum]->GetProgram());
			// Look into the atMap
			sysObfuscated<u64> *runPtrLoadTime = prf.Access(programSO);
			if(runPtrLoadTime != NULL && *runPtrLoadTime != NULL  )
			{
				// Good. We've found something.
				// Lets compare our current-to-load
				if(*vtablePtr != (*runPtrLoadTime).Get())
				{
					//@@: range CDISTANTLIGHTS2_UPDATE_REPORT_MODIFIED_VFT {
					
					vfxDebugf3("Script with hash 0x%016llx detected to be hooked. This is probably very bad.");
					// Report them
					//@@: location CDISTANTLIGHTS2_UPDATE_REPORT_MODIFIED_VFT_ENTRY
					// Add a meaningless instruction to provide enough stuff to hook onto
					prf[sysObfuscated<u32>(0)] = sysObfuscated<u64>(0);
					//@@: range CDISTANTLIGHTS2_UPDATE_REPORT_MODIFIED_VFT_INNER {
					CNetworkTelemetry::AppendInfoMetric(threads[randnum]->GetProgram(), 0xF9C2ECD7, 0xC13C5FB3);
					CNetworkCodeCRCFailedEvent::Trigger(threads[randnum]->GetProgram(),	0xF397CE44, 0x67E6C606);
					// Set them as bad actors, which directs them into the cheater pool, for their next session
					//@@:  } CDISTANTLIGHTS2_UPDATE_REPORT_MODIFIED_VFT_INNER

					// Now clear the array, so we don't double report.
					//@@: location CDISTANTLIGHTS2_UPDATE_REPORT_MODIFIED_VFT_EXIT
					prf.Reset();
					//@@: } CDISTANTLIGHTS2_UPDATE_REPORT_MODIFIED_VFT
				}
			}
			// You'd think I'd have to care about the else scenario here, but that isn't true.
			// Mostly because there are potential race conditions that exist while some of the 
			// scripts are being loaded. It happened enough for me to not want to trigger false
			// positives.
			//else
			//{
			//	// REPORT REPORT REPORT
			//}

			//@@: } CDISTANTLIGHTS2_UPDATE_CHECK_MODIFIED_VFT_INNER


		}		

	}
	//@@: } CDISTANTLIGHTS2_UPDATE_CHECK_MODIFIED_VFT
#endif

	if(sm_CarLength.Getf() == 0.0f)
	{
		if(sm_DistantCarBaseModelInfoNight != NULL && sm_DistantCarBaseModelInfoNight->GetHasLoaded())
		{
			const rmcDrawable* drawable = sm_DistantCarBaseModelInfoNight->GetDrawable();
			if(drawable != NULL)
			{
				const spdAABB* aabb = drawable->GetLodGroup().GetLodModel0(LOD_HIGH).GetAABBs();
				if(aabb != NULL)
				{
					Vec3V size = aabb->GetExtent();
					// We assume the length is the longest.
					sm_CarLength = ScalarV(Max(size.GetXf(), size.GetYf()));
#if RSG_BANK
					sm_CarWidth = ScalarV(Min(size.GetXf(), size.GetYf()));
#endif // RSG_BANK
				}
			}
		}
	}
#endif // ENABLE_DISTANT_CARS

	// debug - render all groups to the vector map
#if __BANK
	if (m_displayVectorMapAllGroups)
	{
		for (s32 i=0; i<m_numGroups; i++)
		{
			s32 startOffset = m_groups[i].pointOffset;
			s32 endOffset   = startOffset+m_groups[i].pointCount;

			s32 r = (m_points[startOffset].x * m_points[endOffset].y)%255;
			s32 g = (m_points[startOffset].y * m_points[endOffset].x)%255;
			s32 b = (m_points[startOffset].x * m_points[startOffset].y * m_points[endOffset].x * m_points[endOffset].y)%255;

			Color32 col(r, g, b, 32);

			for (s32 j=startOffset; j<startOffset+m_groups[i].pointCount-1; j++)
			{
				Vec3V vPtA((float)m_points[j].x, (float)m_points[j].y, 0.0f); 
				Vec3V vPtB((float)m_points[j+1].x, (float)m_points[j+1].y, 0.0f); 
				CVectorMap::DrawLine(RCC_VECTOR3(vPtA), RCC_VECTOR3(vPtB), col, col);
			}
		}

		// output stats
		grcDebugDraw::AddDebugOutput("Distant Lights - NumPoints:%d NumGroups:%d", m_numPoints, m_numGroups); 
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVisibility
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights2::ProcessVisibility				()
{
	m_DistantLightsDataDependency.m_NumPending = 1;

	int bufferId = gRenderThreadInterface.GetUpdateBuffer();
	DistantLightsVisibilityOutput* pVisibilityArray = &(g_DistantLightsVisibility[bufferId][0]);

	u32* pVisibilityCount = &(g_DistantLightsCount[bufferId]);
	//Reset Count to 0
	*pVisibilityCount = 0;
#if __BANK
	u32* pVisibilityBlockCount = &(g_DistantLightsBlocksCount[bufferId]);
	u32* pVisibilityWaterReflectionBlockCount = &(g_DistantLightsWaterReflectionBlocksCount[bufferId]);
	u32* pVisibilityMirrorReflectionBlockCount = &(g_DistantLightsMirrorReflectionBlocksCount[bufferId]);
	u32* pVisibilityMainVPCount = &(g_DistantLightsMainVPCount[bufferId]);
	u32* pVisibilityWaterReflectionCount = &(g_DistantLightsWaterReflectionCount[bufferId]);
	u32* pVisibilityMirrorReflectionCount = &(g_DistantLightsMirrorReflectionCount[bufferId]);
	//Reset Count to 0
	*pVisibilityBlockCount = 0;
	*pVisibilityWaterReflectionBlockCount = 0;
	*pVisibilityMirrorReflectionBlockCount = 0;
	*pVisibilityMainVPCount = 0;
	*pVisibilityWaterReflectionCount = 0;
	*pVisibilityMirrorReflectionCount = 0;
	DistantLightsVisibilityBlockOutput* pBlockVisibilityArray = &(g_DistantLightsBlockVisibility[bufferId][0]);
#endif

#if __DEV
	u32* pDistantLightsOcclusionTrivialAcceptActiveFrustum = &(g_DistantLightsOcclusionTrivialAcceptActiveFrustum[bufferId]);
	u32* pDistantLightsOcclusionTrivialAcceptMinZ = &(g_DistantLightsOcclusionTrivialAcceptActiveMinZ[bufferId]);
	u32* pDistantLightsOcclusionTrivialAcceptVisiblePixel = &(g_DistantLightsOcclusionTrivialAcceptVisiblePixel[bufferId]);
	*pDistantLightsOcclusionTrivialAcceptActiveFrustum = 0;
	*pDistantLightsOcclusionTrivialAcceptMinZ = 0;
	*pDistantLightsOcclusionTrivialAcceptVisiblePixel = 0;
#endif


	// set the coastal light alpha - default to off
	float coastalAlpha = Lights::CalculateTimeFade(
		g_distLightsParam.hourStart - 1.0f,
		g_distLightsParam.hourStart,
		g_distLightsParam.hourEnd - 1.0f,
		g_distLightsParam.hourEnd);

	m_RenderingDistantLights = !(coastalAlpha<=0.0f);

	// if there are no coastal lights on then return
#if ENABLE_DISTANT_CARS
#if __BANK
	if (coastalAlpha <= 0.0f && m_disableDistantCars)
	{
		return;
	}
#endif
#else
	if (coastalAlpha <= 0.0f)
	{
		return;
	}
#endif

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();

	spdTransposedPlaneSet8 cullPlanes;
	if(g_SceneToGBufferPhaseNew && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker() && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid())
	{
		const bool bInteriorCullAll = !fwScan::GetScanResults().GetIsExteriorVisibleInGbuf();

		if(bInteriorCullAll == false)
		{
			fwScreenQuad quad = fwScan::GetScanResults().GetExteriorGbufScreenQuad();
			quad.GenerateFrustum(grcVP->GetCullCompositeMtx(), cullPlanes);
		}
		else
		{
			// if the exterior world isn't visible, then return
			return;
		}		
	}
	else
	{
#if NV_SUPPORT
		cullPlanes.SetStereo(*grcVP, true, true);
#else
		cullPlanes.Set(*grcVP, true, true);
#endif
	}


	// calc the inland light alpha based on the height of the camera (helis need to see inland lights)
	Vec3V vCamPos = gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition();
	float inlandAlpha = Min(1.0f, ((vCamPos.GetZf()-g_distLightsParam.inlandHeight) / 40.0f));


	// Render data
	m_DistantLightsDataDependencyParams.camPos = VEC3V_TO_VECTOR3(vCamPos);
	m_DistantLightsDataDependencyParams.lodDistance2 = GetActualCarLodDistance();
	m_DistantLightsDataDependencyParams.lodDistance2 *= m_DistantLightsDataDependencyParams.lodDistance2;
	
	m_DistantLightsDataDependency.m_Priority = sysDependency::kPriorityLow;
	m_DistantLightsDataDependency.m_Flags = 0;

#if __BANK
	if (m_overrideInlandAlpha>=0.0f)
	{
		inlandAlpha = m_overrideInlandAlpha;
	}
#endif

	const grcViewport* grcWaterReflectionVP = CRenderPhaseWaterReflectionInterface::GetViewport();
	const grcViewport* grcMirrorReflectionVP = CRenderPhaseMirrorReflectionInterface::GetViewport();
	bool bPerformMirrorReflectionVisibility = grcMirrorReflectionVP && CMirrors::GetIsMirrorVisible() && fwScan::GetScanResults().GetMirrorCanSeeExteriorView();
	bool bPerformWaterReflectionVisibility = grcWaterReflectionVP && CRenderPhaseWaterReflectionInterface::IsWaterReflectionEnabled();
	fwScanBaseInfo&		scanBaseInfo = fwScan::GetScanBaseInfo();

	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityArray = pVisibilityArray;
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityCount = pVisibilityCount;
#if __BANK
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityBlockArray = pBlockVisibilityArray;
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityMainVPCount = pVisibilityMainVPCount;	
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityWaterReflectionCount = pVisibilityWaterReflectionCount;	
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityMirrorReflectionCount = pVisibilityMirrorReflectionCount;	
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityBlockCount = pVisibilityBlockCount;
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityWaterReflectionBlockCount = pVisibilityWaterReflectionBlockCount;	
	g_DistantLightsVisibilityData.m_pDistantLightsVisibilityMirrorReflectionBlockCount = pVisibilityMirrorReflectionBlockCount;	
#endif

#if __DEV
	g_DistantLightsVisibilityData.m_pDistantLightsOcclusionTrivialAcceptActiveFrustum = pDistantLightsOcclusionTrivialAcceptActiveFrustum;
	g_DistantLightsVisibilityData.m_pDistantLightsOcclusionTrivialAcceptMinZ = pDistantLightsOcclusionTrivialAcceptMinZ;
	g_DistantLightsVisibilityData.m_pDistantLightsOcclusionTrivialAcceptVisiblePixel = pDistantLightsOcclusionTrivialAcceptVisiblePixel;
#endif
	g_DistantLightsVisibilityData.m_mainViewportCullPlanes = cullPlanes;
	g_DistantLightsVisibilityData.m_vCameraPos = grcVP->GetCameraPosition();
	if(bPerformWaterReflectionVisibility)
	{
		g_DistantLightsVisibilityData.m_waterReflectionCullPlanes.Set(*grcWaterReflectionVP, true, true);
	}
	if(bPerformMirrorReflectionVisibility)
	{
		g_DistantLightsVisibilityData.m_mirrorReflectionCullPlanes.Set(*grcMirrorReflectionVP, true, true);
	}

	g_DistantLightsVisibilityData.m_InlandAlpha = inlandAlpha;
	g_DistantLightsVisibilityData.m_PerformMirrorReflectionVisibility = bPerformMirrorReflectionVisibility;
	g_DistantLightsVisibilityData.m_PerformWaterReflectionVisibility = bPerformWaterReflectionVisibility;


	g_DistantLightsVisibilityDependency.m_Priority = sysDependency::kPriorityLow;
	g_DistantLightsVisibilityDependency.m_Flags = 
		sysDepFlag::ALLOC0 |
		sysDepFlag::INPUT1 | 
		sysDepFlag::INPUT2 | 
		sysDepFlag::INPUT3 | 
		sysDepFlag::INPUT4 |
		sysDepFlag::INPUT5 |
		sysDepFlag::INPUT6 |
		sysDepFlag::INPUT7;

	g_DistantLightsVisibilityDependency.m_Params[DLP_GROUPS].m_AsPtr = (void*)m_groups;
	g_DistantLightsVisibilityDependency.m_Params[DLP_BLOCK_FIRST_GROUP].m_AsPtr = (void*)m_blockFirstGroup;
	g_DistantLightsVisibilityDependency.m_Params[DLP_BLOCK_NUM_COSTAL_GROUPS].m_AsPtr = (void*)m_blockNumCoastalGroups;
	g_DistantLightsVisibilityDependency.m_Params[DLP_BLOCK_NUM_INLAND_GROUPS].m_AsPtr = (void*)m_blockNumInlandGroups;
	g_DistantLightsVisibilityDependency.m_Params[DLP_VISIBILITY].m_AsPtr = &g_DistantLightsVisibilityData;
	g_DistantLightsVisibilityDependency.m_Params[DLP_SCAN_BASE_INFO].m_AsPtr = &scanBaseInfo;
	g_DistantLightsVisibilityDependency.m_Params[DLP_HIZ_BUFFER].m_AsPtr = COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY);	
	g_DistantLightsVisibilityDependency.m_Params[DLP_DEPENDENCY_RUNNING].m_AsPtr = &g_DistantLightsVisibilityDependencyRunning;


	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_SRATCH_SIZE] = DISTANTLIGHTSVISIBILITY_SCRATCH_SIZE;
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_GROUPS] = DISTANTLIGHTS2_MAX_GROUPS * sizeof(DistantLightGroup2_t);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_BLOCK_FIRST_GROUP] = DISTANTLIGHTS2_MAX_BLOCKS * DISTANTLIGHTS2_MAX_BLOCKS * sizeof(s32);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_BLOCK_NUM_COSTAL_GROUPS] = DISTANTLIGHTS2_MAX_BLOCKS * DISTANTLIGHTS2_MAX_BLOCKS * sizeof(s32);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_BLOCK_NUM_INLAND_GROUPS] = DISTANTLIGHTS2_MAX_BLOCKS * DISTANTLIGHTS2_MAX_BLOCKS * sizeof(s32);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_VISIBILITY] = sizeof(DistantLightsVisibilityData);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_SCAN_BASE_INFO] = sizeof(fwScanBaseInfo);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_HIZ_BUFFER] = RAST_HIZ_AND_STENCIL_SIZE_BYTES;


	g_DistantLightsVisibilityDependencyRunning = 1;

	sysDependencyScheduler::Insert( &g_DistantLightsVisibilityDependency );

}

// ----------------------------------------------------------------------------------------------- //

void CDistantLights2::WaitForProcessVisibilityDependency()
{
	PF_PUSH_TIMEBAR("CDistantLights2 Wait for Visibility Dependency");
	while(g_DistantLightsVisibilityDependencyRunning)
	{
		sysIpcYield(PRIO_NORMAL);
	}
	PF_POP_TIMEBAR();
}

#if ENABLE_DISTANT_CARS
void CDistantLights2::RenderDistantCars()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	if (g_hack_oldDistantCarsSync)
	{
		g_distantLights.PrepareDistantCarsRenderData();
	}

#if USE_DISTANT_LIGHTS_2
	g_distantLights.RenderDistantCarsInternal();
#else
	vfxAssertf(false, "Cannot render distant cars 2 as distant cars is in use!");
#endif // USE_DISTANT_LIGHTS_2

	if (g_hack_oldDistantCarsSync)
	{
		sm_DistantCarRenderThreadID = g_CurrentThreadId;

		//Signal the distant lights render that the cars have finished.
		if( sm_RequiresRenderThreadSync )
		{
			sysIpcSignalSema(sm_RenderThreadSync);		
		}
	}
}

#if __STATS
PF_PAGE(DistantLights2Page, "Distant Lights 2");
PF_GROUP(DistantLights2Group);
PF_LINK(DistantLights2Page, DistantLights2Group);
PF_TIMER(DistantLights2_CategorisePoint, DistantLights2Group);
PF_COUNTER(DistantLights2_CategorisePoint_Count, DistantLights2Group);
#endif // __STATS

void CDistantLights2::CreateBuffers()
{
	Assertf((m_CreatedVertexBuffers == false), "Attempting to create CDistantLights2 vertex buffers twice");
	if( !m_CreatedVertexBuffers )
	{
		grcFvf fvfInstances;		
		u32 instanceVBSize = DISTANTCARS_MAX;
#if __PS3 || __XENON
		u32 vbSize = DISTANTCARS_MAX;
		vbSize*= pVertexBuff->GetVertexCount();
		//We have to make instance vb larger on PS3, on 360 we can use vfetch to keep it smaller, not sure if there's a way on PS3?
#if __PS3		
		instanceVBSize = vbSize;
#endif
#endif

		fvfInstances.SetTextureChannel(1, true, grcFvf::grcdsFloat3);
		fvfInstances.SetTextureChannel(2, true, grcFvf::grcdsFloat3);
		fvfInstances.SetDiffuseChannel(true, grcFvf::grcdsColor);

		m_DistantCarsMaxVertexBuffers = MaxBufferCount;
#if RSG_PC
		m_DistantCarsMaxVertexBuffers = 3 + GRCDEVICE.GetGPUCount();
#endif // RSG_PC
		m_DistantCarsInstancesVB.SetCount(m_DistantCarsMaxVertexBuffers);

		for(u32 i=0; i < m_DistantCarsMaxVertexBuffers; i++)
		{
#if (__D3D11 && RSG_PC)|| RSG_ORBIS
			m_DistantCarsInstancesVB[i] = grcVertexBuffer::Create(instanceVBSize, fvfInstances, rage::grcsBufferCreate_DynamicWrite, rage::grcsBufferSync_None, NULL);
#else
			const bool bReadWrite = true;
			const bool bDynamic = RSG_DURANGO ? false : true;
			m_DistantCarsInstancesVB[i] = grcVertexBuffer::Create(instanceVBSize, fvfInstances, bReadWrite, bDynamic, NULL);
#endif
		}
		m_DistantCarsCurVertexBuffer = 0;

#if __PS3 || __XENON
		m_DistantCarsModelIB = grcIndexBuffer::Create(pIndexBuff->GetIndexCount() * DISTANTCARS_MAX, false);
		u16* pDstIB = m_DistantCarsModelIB->LockRW();
		u16* pSrcIB = (u16*)(pIndexBuff->LockRO());

		const grcFvf *pfvfModel = geom.GetFvf();
		const bool bReadWrite = true;
		const bool bDynamic = false;

		m_DistantCarsModelVB = grcVertexBuffer::Create(vbSize, *pfvfModel, bReadWrite, bDynamic, NULL);
		m_DistantCarsModelVB->LockRW();
		u8* pDstVB = (u8*) m_DistantCarsModelVB->GetLockPtr();
		pVertexBuff->LockRO();
		u8* pSrcVB = (u8*) pVertexBuff->GetLockPtr();
		for (int  i = 0; i < DISTANTCARS_MAX; i++)
		{
			memcpy(pDstIB, pSrcIB, pIndexBuff->GetIndexCount() * sizeof(u16));
			for (int j = 0; j < pIndexBuff->GetIndexCount(); j++)
			{
				*pDstIB += (u16)(i*pVertexBuff->GetVertexCount());
				pDstIB++;
			}

			memcpy(pDstVB, pSrcVB, pVertexBuff->GetVertexCount() * pVertexBuff->GetVertexStride());
			pDstVB += pVertexBuff->GetVertexCount() * (pVertexBuff->GetVertexStride());
		}
		m_DistantCarsModelIB->UnlockRW();
		pIndexBuff->UnlockRO();

		m_DistantCarsModelVB->UnlockRW();
		pVertexBuff->UnlockRO();
#endif
		m_CreatedVertexBuffers = true;
	}
}

void CDistantLights2::DestroyBuffers()
{
	m_CreatedVertexBuffers = false;

	for (u32 i = 0; i < m_DistantCarsMaxVertexBuffers; i++ )
	{
		if(m_DistantCarsInstancesVB[i])
		{
			delete m_DistantCarsInstancesVB[i];
			m_DistantCarsInstancesVB[i] = NULL;
		}
	}
#if !(__D3D11 || RSG_ORBIS)
	if(m_DistantCarsModelVB)
	{
		delete m_DistantCarsModelVB;
		m_DistantCarsModelVB = NULL;
	}
	if(m_DistantCarsModelIB)
	{
		delete m_DistantCarsModelIB;
		m_DistantCarsModelIB = NULL;
	}
#endif
}

void CDistantLights2::RenderDistantCarsInternal()
{
	if( m_disableDistantCars || m_disableDistantCarsByScript )
		return;

	if( CRenderer::GetDisableArtificialLights() )
		return;

	const CPopGenShape& populationShape = CVehiclePopulation::GetPopGenShape();
	const Vec2V populationCenter(populationShape.GetCenter().GetX(), populationShape.GetCenter().GetY());
	const Vec2V populationDir(populationShape.GetDir().x, populationShape.GetDir().y);

	const Vec3V cameraPos = gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition();
	float lodDistance2 = GetActualCarLodDistance();
	lodDistance2 *= lodDistance2;

	if(!(sm_DistantCarBaseModelInfo && sm_DistantCarBaseModelInfo->GetHasLoaded()
		&&sm_DistantCarBaseModelInfoNight && sm_DistantCarBaseModelInfoNight->GetHasLoaded()))
		return;
	
	rmcDrawable *drawable = NULL;
	//If we're rendering the distant lights use the night model
	if(m_RenderingDistantLights)
		drawable = sm_DistantCarBaseModelInfoNight->GetDrawable();
	else
		drawable = sm_DistantCarBaseModelInfo->GetDrawable();

	grcTexture* pDistantCarTexture = NULL;
	for (int k=0; k<drawable->GetShaderGroup().GetCount(); k++)
	{
		grmShader& shader = drawable->GetShaderGroup().GetShader(k);
		for (int s=0; s<shader.GetInstanceData().GetCount(); s++)
		{
			if (shader.GetInstanceData().IsTexture(s))
			{
				pDistantCarTexture = shader.GetInstanceData().Data()[s].Texture;
			}
		}
	}

	const rmcLodGroup& lodGroup = drawable->GetLodGroup();
	const grmModel& model = lodGroup.GetLodModel0(LOD_HIGH);
	const grmGeometry& geom = model.GetGeometry(0);	 
#if __PS3 || __XENON || __D3D11 || RSG_ORBIS || __ASSERT
	const grcVertexBuffer* pVertexBuff = geom.GetVertexBufferByIndex(0);
#endif
	const grcIndexBuffer* pIndexBuff = geom.GetIndexBuffer(0);
	FatalAssertf(m_CreatedVertexBuffers, "CDistantLights2 vertex buffers not created");

	// as the sniper scope zooms in, fade out the cars
	const float min_fov = 20.0f;
	const float max_fov = 45.0f;
	const float fov = camInterface::GetFov();
	float fDistantCarAlpha = 1.0f;
	if (fov < max_fov)
	{
		fDistantCarAlpha = Min(Max((fov - min_fov),0.0f) / (max_fov - min_fov), 1.0f);
	}
	float fDistantCarInverseAlpha = 1.0f - fDistantCarAlpha;

	const float cullRange = CVehiclePopulation::GetCullRange();
	const float nearDist = CVehiclePopulation::GetPopGenShape().GetInnerBandRadiusMax() * fDistantCarAlpha + cullRange * fDistantCarInverseAlpha;
	const float farDist = CVehiclePopulation::GetPopGenShape().GetOuterBandRadiusMax();
	const float farDistOnScreen = CVehiclePopulation::GetCullRangeOnScreen();

	const float min_fov_factor = 2.75f;
	const float max_fov_factor = 1.0f;
	const float innerFadeDistanceBase = CVehiclePopulation::GetKeyholeShape().GetInnerBandRadiusMax() * Lerp(fDistantCarAlpha,min_fov_factor,max_fov_factor);
	const float innerFadeDistance = innerFadeDistanceBase + ms_distantCarsFadeZone;
	const float innerFadeDistanceSquared = innerFadeDistance * innerFadeDistance;
	
	float emissiveScale = 1.0f;
	//Turn off emissive when in player switch cam to stop flickering. We normally use emissive at night when m_RenderingDistantLights is set so scale that to turn emissive off.
	if( g_LodMgr.IsSlodModeActive() )
		emissiveScale = 0.0f;

#if ENABLE_DISTANT_CARS
	m_DistantCarsShader->SetVar(sm_DistantCarGeneralParams, Vector4(m_RenderingDistantLights * emissiveScale, 0.0f, 0.0f, 0.0f));
	m_DistantCarsShader->SetVar(sm_DistantCarTexture, pDistantCarTexture);
#endif // ENABLE_DISTANT_CARS

	// Make sure you have enough buffers to avoid overwriting a previous frames results
	Verifyf(m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->Lock(rage::grcsNoOverwrite /*grcsDiscard*/), "Couldn't lock vertex buffer");

	DistantCarInstanceData* pDst = reinterpret_cast<DistantCarInstanceData*>(m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->GetLockPtr());
	u32 numToDraw = 0;

	const atFixedArray<DistantCar_t, DISTANTCARS_MAX>& renderData = m_distantCarRenderData[m_distantDataRenderBuffer];
	for (int i = 0; i < renderData.size(); i++)
	{
		const DistantCar_t& car = renderData[i];

		float alpha = 0.0f;

		const Vec3V cameraToCar = car.pos - cameraPos;
		const float distCameraToCar2 = MagSquared(cameraToCar).Getf();

		if (distCameraToCar2 > lodDistance2 || g_LodMgr.IsSlodModeActive())
		{
			// Beyond real vehicle fade threshold
			alpha = 1.0f;
#if __BANK
			if (ms_debugDistantCars)
			{
				grcDebugDraw::Sphere(car.pos, 2.0f, Color_red, false);
			}
#endif // __BANK
		}
		else
		{
			// Use population parameters to determine visibility
			const Vec2V carPos = car.pos.GetXY();
			const Vec2V carDir = car.dir.GetXY();
			const Vec2V populationCenterToCar = carPos - populationCenter;
			const float distToCar = Mag(populationCenterToCar).Getf();
			const float planarDistToCar = Dot(populationDir, populationCenterToCar).Getf();
			const float planarDistToPopulationCenter = Dot(carDir, -populationCenterToCar).Getf();

			const bool isDrivingAwayPrecise = planarDistToPopulationCenter < 0.0f;
			
			if (isDrivingAwayPrecise && distToCar > farDistOnScreen)
			{
				// Driving away, beyond on-screen cull radius
				alpha = Min((distToCar - farDistOnScreen) / ms_distantCarsFadeZone, 1.0f);
#if __BANK
				if (ms_debugDistantCars)
				{
					grcDebugDraw::Sphere(car.pos, 2.0f, Color_cyan, false);
				}
#endif // __BANK
			}
			else if (!isDrivingAwayPrecise && planarDistToCar >= 0.0f && distToCar > farDist - ms_distantCarsFadeZone)
			{
				// Driving toward, in front of population center, outside of outer creation band
				alpha = Saturate((distToCar - (farDist - ms_distantCarsFadeZone)) / ms_distantCarsFadeZone);
#if __BANK
				if (ms_debugDistantCars)
				{
					grcDebugDraw::Sphere(car.pos, 2.0f, Color_green, false);
				}
#endif // __BANK
			}
			else if (!isDrivingAwayPrecise && planarDistToCar < 0.0f && distToCar > nearDist - ms_distantCarsFadeZone)
			{
				// Driving toward, behind population center, outside of inner creation band
				alpha = Saturate((distToCar - (nearDist - ms_distantCarsFadeZone)) / ms_distantCarsFadeZone);

				//if (planarDistToCar > -ms_distantCarsFadeZone)
				//{
				//	const float planarAlpha = planarDistToCar / -ms_distantCarsFadeZone;
				//	alpha = Min(planarAlpha, alpha);
				//}
#if __BANK
				if (ms_debugDistantCars)
				{
					grcDebugDraw::Sphere(car.pos, 2.0f, Color_magenta, false);
				}
#endif // __BANK
			}
#if __BANK
			else if (ms_debugDistantCars)
			{
				grcDebugDraw::Sphere(car.pos, 2.0f, Color_white, false);
			}
#endif // __BANK
		}

		if (alpha > VERY_SMALL_FLOAT)
		{
			if( g_EnableDistantCarInnerFade && distCameraToCar2 < innerFadeDistanceSquared )
			{
				const float distCameraToCar = Sqrtf(distCameraToCar2);
				const float distanceAlpha = Clamp((distCameraToCar - innerFadeDistanceBase)/ms_distantCarsFadeZone,0.0f,1.0f);
				alpha *= distanceAlpha;
			}
#if __PS3
			for(int j = 0; j < pVertexBuff->GetVertexCount(); j++)
#endif
			{
				vfxAssertf(MagSquared(car.dir).Getf() < 1.1f,
					"Distant Car [%d] direction is broken!", i);
				pDst->vPositionX = car.pos.GetXf();
				pDst->vPositionY = car.pos.GetYf();
				pDst->vPositionZ = car.pos.GetZf();
				pDst->vDirectionX = car.dir.GetXf();
				pDst->vDirectionY = car.dir.GetYf();
				pDst->vDirectionZ = car.dir.GetZf();
#if USE_DISTANT_LIGHTS_2
				Color32 color = car.color;
				color.SetAlpha(static_cast<int>(alpha * 255));
				pDst->Color = color.GetDeviceColor();
#endif
				pDst++;
			}
			numToDraw++;
		}
	}

	m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->UnlockWO();

	if (numToDraw == 0)
	{
		return;
	}
	AssertVerify(m_DistantCarsShader->BeginDraw(grmShader::RMC_DRAW,true,m_DistantCarsTechnique)); // MIGRATE_FIXME
	m_DistantCarsShader->Bind((int)0);	

	grcWorldIdentity();

	GRCDEVICE.SetVertexDeclaration(sm_pDistantCarsVtxDecl);
#if __D3D11 || RSG_ORBIS
	GRCDEVICE.SetIndices(*pIndexBuff);
	GRCDEVICE.SetStreamSource(0, *pVertexBuff, 0, pVertexBuff->GetVertexStride());
	GRCDEVICE.SetStreamSource(1, *m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer], 0, m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->GetVertexStride());
	GRCDEVICE.DrawInstancedIndexedPrimitive(drawTris, pIndexBuff->GetIndexCount(), numToDraw, 0, 0, 0);
#else
	GRCDEVICE.SetIndices(*m_DistantCarsModelIB);
	GRCDEVICE.SetStreamSource(0, *m_DistantCarsModelVB, 0, m_DistantCarsModelVB->GetVertexStride());
	GRCDEVICE.SetStreamSource(1, *m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer], 0, m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->GetVertexStride());
	GRCDEVICE.DrawIndexedPrimitive(drawTris, 0, pIndexBuff->GetIndexCount() * numToDraw);
#endif	

	m_DistantCarsShader->UnBind();
	m_DistantCarsShader->EndDraw();


	GRCDEVICE.ClearStreamSource(0);
	GRCDEVICE.ClearStreamSource(1);

	GRCDEVICE.SetVertexDeclaration(NULL);

	m_DistantCarsCurVertexBuffer = (m_DistantCarsCurVertexBuffer+1) % m_DistantCarsMaxVertexBuffers;

}

#endif //ENABLE_DISTANT_CARS

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////
void CDistantLights2::Render				(CVisualEffects::eRenderMode mode, float intensityScale)
{
	DISTANTLIGHTS_PF_START(Render);
	DISTANTLIGHTS_PF_START(Setup);

	if (g_hack_oldDistantCarsSync)
	{
		if(mode == CVisualEffects::RM_DEFAULT)
		{
			//Only do renderthread sync when the distant cars and lights arent on the same render thread.
			sm_RequiresRenderThreadSync = sm_DistantCarRenderThreadID != g_CurrentThreadId && sm_DistantCarRenderThreadID != 0;

			//Ensure that the distant cars have rendered this frame before continuing and clearing the data.
			//This caused the cars to flash on ps4 as this and the cars render can switch order.
			if( sm_RequiresRenderThreadSync )
			{
				PF_PUSH_TIMEBAR_BUDGETED("CDistantLights2 Wait for RenderThread Sync", 0.1f);		
				sysIpcWaitSema(sm_RenderThreadSync);
				PF_POP_TIMEBAR();
			}
		}
	}

#if __BANK
	UpdateNodeGeneration();
	RenderDebugData();

	m_numBlocksRendered = 0;
	m_numLightsRendered = 0;
	m_numGroupsRendered = 0;
	m_numBlocksWaterReflectionRendered = 0;
	m_numBlocksMirrorReflectionRendered = 0;
	m_numGroupsWaterReflectionRendered = 0;
	m_numGroupsMirrorReflectionRendered = 0;

	if (m_bDisableRendering)
	{
		return;
	}
#endif // __BANK

	// check that we're being called by the render thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CDistantLights2::Render - not called from the render thread");	

	// set the coastal light alpha - default to off
	float coastalAlpha = Lights::CalculateTimeFade(
		g_distLightsParam.hourStart - 1.0f,
		g_distLightsParam.hourStart,
		g_distLightsParam.hourEnd - 1.0f,
		g_distLightsParam.hourEnd);

	// if there are no coastal lights on then return
#if ENABLE_DISTANT_CARS
	if(coastalAlpha <= 0.0f)
	{
#if __BANK
		if (m_disableDistantCars)
			return;
#endif //__BANK

	}
#endif //ENABLE_DISTANT_CARS

#if RSG_BANK
	const int bufferID = gRenderThreadInterface.GetRenderBuffer();
#endif

	if( CRenderer::GetDisableArtificialLights() )
		return;

	const float LodScaleDist = Max<float>(1.0f, g_LodScale.GetGlobalScaleForRenderThread() * CLODLights::GetScaleMultiplier());
	float fadeFarDist = g_distLightsParam.carLightFarFadeDist * LodScaleDist;

	//when in the player switch cam dont set these fade params as we bring the lights right in during this.
	Vector4 distLightFadeParams;
	distLightFadeParams.Zero();
	if( !g_LodMgr.IsSlodModeActive() )
	{
		float enableCarLightFadeIn = 1.0;
		distLightFadeParams.x = enableCarLightFadeIn;
		distLightFadeParams.y = g_distLightsParam.carLightFadeInStart;
		distLightFadeParams.z = g_distLightsParam.carLightFadeInRange;		
	}
	// Draw the white lights.
	CDistantLightsVertexBuffer::RenderBufferBegin(mode, g_distLightsParam.carLightFadeInStart, fadeFarDist, intensityScale * g_distLightsParam.carLightHdrIntensity, false, &distLightFadeParams);

	DISTANTLIGHTS_PF_STOP(Setup);
	BANK_ONLY(int numLights = 0;)

	const int lightsPerBatch = (DISTANTLIGHTS_VERTEXBUFFER_VERTEXBUFFER_SIZE / sizeof(DistantLightVertex_t)) - 3;

	const atFixedArray<Vec4V, DISTANTCARS_MAX>& renderData = m_distantLightsRenderData[m_distantDataRenderBuffer];
	for (int i = 0; i < renderData.GetCount();)
	{
		int batchSize = MIN(lightsPerBatch, renderData.GetCount() - i);
		DISTANTLIGHTS_PF_START(Stall);
		Vec4V* RESTRICT outputBuff = CDistantLightsVertexBuffer::ReserveRenderBufferSpace(batchSize);
		DISTANTLIGHTS_PF_STOP(Stall);

		if (!outputBuff)
			break;

		for (int j = 0; j < batchSize; j++)
		{
			outputBuff[j] = renderData[i+j];
		}

		i += batchSize;
	}

	CDistantLightsVertexBuffer::RenderBuffer(true);
#if RSG_BANK
	m_numGroupsRendered = g_DistantLightsMainVPCount[bufferID];
	m_numBlocksRendered = g_DistantLightsBlocksCount[bufferID];

	m_numBlocksWaterReflectionRendered = g_DistantLightsWaterReflectionCount[bufferID];
	m_numBlocksMirrorReflectionRendered = g_DistantLightsMirrorReflectionCount[bufferID];
	m_numGroupsWaterReflectionRendered = g_DistantLightsWaterReflectionBlocksCount[bufferID];
	m_numGroupsMirrorReflectionRendered = g_DistantLightsMirrorReflectionBlocksCount[bufferID];

	//Render visible blocks detected by ProcessVisibility
	if (m_displayVectorMapBlocks && mode == CVisualEffects::RM_DEFAULT)
	{
		DistantLightsVisibilityBlockOutput* pVisibleBlocks = &(g_DistantLightsBlockVisibility[bufferID][0]);

		for(s32 i=0; i < m_numBlocksRendered; i++)
		{
			DistantLightsVisibilityBlockOutput& currentVisibleBlock = pVisibleBlocks[i];

			// calc the centre pos of the block
			float centrePosX, centrePosY;
			DistantLightsVisibility::FindBlockCentre(currentVisibleBlock.blockX, currentVisibleBlock.blockY, centrePosX, centrePosY);
			Vec3V vCentrePos(centrePosX, centrePosY, 20.0f);

			float blockMinX, blockMinY, blockMaxX, blockMaxY;
			DistantLightsVisibility::FindBlockMin(currentVisibleBlock.blockX, currentVisibleBlock.blockY, blockMinX, blockMinY);
			DistantLightsVisibility::FindBlockMax(currentVisibleBlock.blockX, currentVisibleBlock.blockY, blockMaxX, blockMaxY);

			spdAABB box(
				Vec3V(blockMinX, blockMinY, 20.0f - DISTANTLIGHTS_BLOCK_RADIUS),
				Vec3V(blockMaxX, blockMaxY, 20.0f + DISTANTLIGHTS_BLOCK_RADIUS)
				);
			//						Vec3V vCentrePos = box.GetCenter();
			//						CVectorMap::DrawCircle(RCC_VECTOR3(vCentrePos), DISTANTLIGHTS_BLOCK_RADIUS, Color32(255, 255, 255, 255), false);
			CVectorMap::Draw3DBoundingBoxOnVMap(box, Color32(255, 255, 255, 255));

		}
	}

	m_numLightsRendered += (mode == CVisualEffects::RM_DEFAULT) ? numLights : 0;
#endif // __BANK

	// reset the hdr intensity
	CShaderLib::SetGlobalEmissiveScale(1.0f);

	DISTANTLIGHTS_PF_STOP(Render);

	PF_SETTIMER(Render_NoStall, PF_READ_TIME(Render) - PF_READ_TIME(Stall));
	PF_SETTIMER(RenderBuffer_NoStall, PF_READ_TIME(RenderBuffer) - PF_READ_TIME(Stall));
}

///////////////////////////////////////////////////////////////////////////////
//  PlaceLightsAlongGroup
///////////////////////////////////////////////////////////////////////////////

int CDistantLights2::PlaceLightsAlongGroup( ScalarV_In initialOffset, ScalarV_In spacing, ScalarV_In color, ScalarV_In DISTANT_CARS_ONLY(offsetTime), ScalarV_In DISTANT_CARS_ONLY(speed), Vec4V_In laneScalars, Vec3V_In perpDirection, u16 groupIndex, bool DISTANT_CARS_ONLY(isTwoWayLane), bool DISTANT_CARS_ONLY(isTravelingAway), const Vec3V* lineVerts, const Vec4V* lineDiffs, int numLineVerts, ScalarV_In lineLength)
{
	DISTANTLIGHTS_PF_FUNC(CalcCars);

	// We only want to use the x and y for distance.
	const Vector3 camPos = m_DistantLightsDataDependencyParams.camPos;//VEC3V_TO_VECTOR3(viewPort->GetCameraPosition());
	float lodDistance2 = m_DistantLightsDataDependencyParams.lodDistance2;// GetActualCarLodDistance();

	const ScalarV fullAlpha = ScalarVFromU32(255);
	const ScalarV noAlpha   = ScalarVFromU32(0);

	const DistantLightGroup2_t& pCurrGroup = m_groups[groupIndex];
	
#if ENABLE_DISTANT_CARS
	const ScalarV carLightOffset = (isTravelingAway) ? -sm_CarLength : sm_CarLength;
#endif // ENABLE_DISTANT_CARS

#if __BANK
	if(m_drawDistantLightsNodes)
	{
		DebugDrawDistantLightsNodes(numLineVerts, lineVerts);

	}
#endif // __BANK


	ScalarV effectiveLineLength = lineLength - initialOffset;
	int numLights = FloatToIntRaw<0>(RoundToNearestIntPosInf(effectiveLineLength / spacing)).Geti();

	if (numLights <= 0)
		return 0;

	atFixedArray<DistantCar_t, DISTANTCARS_MAX>& distantCarRenderData = m_distantCarRenderData[1-m_distantDataRenderBuffer];
	atFixedArray<Vec4V, DISTANTCARS_MAX>&     distantLightsRenderData = m_distantLightsRenderData[1-m_distantDataRenderBuffer];

	const Vec3V lightWorldOffset(0.0f, 0.0f, g_distLightsParam.carLightZOffset);
	const Vec3V laneOffsets[4] = { (perpDirection * laneScalars.GetX()) + lightWorldOffset, 
								   (perpDirection * laneScalars.GetY()) + lightWorldOffset, 
								   (perpDirection * laneScalars.GetZ()) + lightWorldOffset, 
								   (perpDirection * laneScalars.GetW()) + lightWorldOffset };

#if RSG_BANK && ENABLE_DISTANT_CARS
	// Calculate the car (not light) fade distances for the vector map.
	float distCarStartFadeDist;
	float distCarEndFadeDist;
	if(isTravelingAway)
	{
		distCarStartFadeDist = GetRedLightStartDistance();
		distCarEndFadeDist   = distCarStartFadeDist + ms_redLightFadeDistance;
	}
	else
	{
		distCarStartFadeDist = GetWhiteLightStartDistance();
		distCarEndFadeDist   = distCarStartFadeDist + ms_whiteLightFadeDistance;
	}

	distCarStartFadeDist *= distCarStartFadeDist;
	distCarEndFadeDist   *= distCarEndFadeDist;
#endif // RSG_BANK && ENABLE_DISTANT_CARS

	// Gap-prevention
	const u32 extraLeadingSpaces = static_cast<u32>((initialOffset / spacing).Getf());
	const u32 laneOffsetIndexModifier = 4 - (extraLeadingSpaces % 4);
	ScalarV distancePopulated = initialOffset - (ScalarV(static_cast<float>(extraLeadingSpaces)) * spacing);

	ScalarV lineOffset(V_ZERO);
	u32 currentLineSegment = 0;
	const u32 numLineSegments = numLineVerts - 1;
	u32 numLightsAppended = 0;

	// Populate line with evenly spaced vehicles/lights
	while(IsTrue(distancePopulated < lineLength) && distantCarRenderData.size() < m_softLimitDistantCars)
	{
		DISTANTLIGHTS_PF_START(CalcCarsPos);
		// Find the current line segment
		while(currentLineSegment < numLineSegments && IsTrue(distancePopulated > lineOffset + lineDiffs[currentLineSegment].GetW()))
		{
			lineOffset += lineDiffs[currentLineSegment].GetW();
			++currentLineSegment;
		}

		if(currentLineSegment >= numLineSegments)
		{
			break;
		}

		// To create a deterministic color that works across groups we use the time that the light/car first appeared on the very first group as a seed.
		// As there could be more than once car generated at this time, we also use the group's seed so there is variation amongst groups.
		const ScalarV birthTime = offsetTime - (distancePopulated / speed);
		mthRandom carRandom(static_cast<u32>(birthTime.Getf()) ^ pCurrGroup.randomSeed);

		const u32 laneOffsetIndex = (numLightsAppended + laneOffsetIndexModifier) % 4;

		// Set the world position to a point on the current line segment
		const Vec3V worldPos = AddScaled(lineVerts[currentLineSegment], lineDiffs[currentLineSegment].GetXYZ(), distancePopulated - lineOffset) + laneOffsets[laneOffsetIndex];

#if ENABLE_DISTANT_CARS
		// We want to flip the direction in a two way road when we are traveling from the last node to the first node.
		const Vec3V direction = isTwoWayLane ? -lineDiffs[currentLineSegment].GetXYZ() : lineDiffs[currentLineSegment].GetXYZ();
		vfxAssertf(MagSquared(direction).Getf()<1.1f && currentLineSegment<DISTANTLIGHTS_MAX_LINE_DATA, "Distant car computed direction is broken!");

		// Store the offsets so we can use then when drawing on the vector map.
		const Vec3V lightOffset = carLightOffset * direction;
		const Vec3V lightPos = worldPos + lightOffset;
#else
		const Vec3V& lightPos = worldPos;
#endif // ENABLE_DISTANT_CARS
		DISTANTLIGHTS_PF_STOP(CalcCarsPos);

		DISTANTLIGHTS_PF_START(CalcCarsLightColor);
		// Set to full alpha if we exceed the lod distance.
		Color32 lightColor;
		CalcLightColour(lightPos, camPos, lodDistance2, noAlpha, fullAlpha, color, lightColor);
		DISTANTLIGHTS_PF_STOP(CalcCarsLightColor);

		// Note that we write all 4 verts. When we reserved, we reserved some extra space so this won't crash
		// (but we only bumped up the actual vert count by the true number of lights we want, so the next
		// reservation will overwrite those bonus ones we added)
#if __PPU || __XENON ||  RSG_ORBIS || __D3D11

		distantLightsRenderData.Append() = Vec4V(lightPos, ScalarVFromU32(lightColor.GetDeviceColor()));

#if RSG_BANK
		if( m_displayVectorMapActiveCars DISTANT_CARS_ONLY(|| m_displayVectorMapFadingCars || m_displayVectorMapHiddenCars) )
		{
			VectorMapDisplayActiveFadingAndHiddenCars(worldPos, camPos, distCarEndFadeDist, Color32(255, 0, 0), distCarStartFadeDist, direction, lightOffset);
		}
#endif // RSG_BANK

#if ENABLE_DISTANT_CARS
		DistantCar_t& car = distantCarRenderData.Append();
		car.pos   = worldPos;
		car.dir   = direction;
		car.color = sm_CarColors[carRandom.GetRanged(0, MAX_COLORS-1)];
		car.isDrivingAway = isTravelingAway;
#endif // ENABLE_DISTANT_CARS

#else //__WIN32PC && __D3D9
		(void) isTravelingAway;
		// On PC DX9 as we cant use quads we need to write out 6 verts for each light.
		for( u32 i = 0; i < 6; i++ )
			distantLightsRenderData.Append() = Vec4V(worldPos, color);
#endif

		distancePopulated += spacing;
		++numLightsAppended;
	}

	return numLightsAppended;
}

float CDistantLights2::GetWhiteLightStartDistance() const
{
	float distance = CVehiclePopulation::GetCreationDistance();

# if ENABLE_DISTANT_CARS
	distance -= ms_whiteLightFadeDistance;
# endif // ENABLE_DISTANT_CARS

	return distance;
}

float CDistantLights2::GetRedLightStartDistance() const
{
	float distance = CVehiclePopulation::GetCullRange();

# if ENABLE_DISTANT_CARS
	distance -= ms_redLightFadeDistance;
# endif // ENABLE_DISTANT_CARS

	return distance;
}

//////////////////////////////////////////////////////////////////////////
// LoadData
//////////////////////////////////////////////////////////////////////////

void CDistantLights2::LoadData()
{
#if __BANK
	if(!PARAM_noDistantLightAutoLoad.Get())
#endif // __BANK
	{
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DISTANT_LIGHTS_HD_FILE);
		if(!DATAFILEMGR.IsValid(pData))
			return;

		LoadData_Impl(pData->m_filename);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData_Impl
///////////////////////////////////////////////////////////////////////////////

void CDistantLights2::LoadData_Impl(const char* path)
{
	FileHandle fileId;
	fileId	= CFileMgr::OpenFile(path);

	if (CFileMgr::IsValidFileHandle(fileId))
	{
		s32 bytes = DistantLightSaveData::Read(fileId, &m_numPoints, 1);
		vfxAssertf(bytes==4, "CDistantLights2::LoadData - error reading number of points");

		bytes = DistantLightSaveData::Read(fileId, &m_numGroups, 1);
		vfxAssertf(bytes==4, "CDistantLights2::LoadData - error reading number of groups");

		bytes = DistantLightSaveData::Read(fileId, &m_blockFirstGroup[0][0], DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS);
		vfxAssertf(bytes==(s32)(DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS*sizeof(s32)), "CDistantLights::LoadData - error reading block's first group");

		bytes = DistantLightSaveData::Read(fileId, &m_blockNumCoastalGroups[0][0], DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS);
		vfxAssertf(bytes==(s32)(DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS*sizeof(s32)), "CDistantLights::LoadData - error reading number of block's coastal group");

		bytes = DistantLightSaveData::Read(fileId, &m_blockNumInlandGroups[0][0], DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS);
		vfxAssertf(bytes==(s32)(DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS*sizeof(s32)), "CDistantLights::LoadData - error reading number of block's inland group");

		bytes = DistantLightSaveData::Read(fileId, &m_points[0].x, m_numPoints*sizeof(DistantLightPoint_t)/sizeof(s16));
		vfxAssertf(bytes==(s32)(m_numPoints*sizeof(DistantLightPoint_t)), "CDistantLights2::LoadData - error reading number of block's points");

		bytes = DistantLightSaveData::Read(fileId, &m_groups[0], m_numGroups);
		vfxAssertf(bytes==(s32)(m_numGroups*sizeof(DistantLightGroup2_t)), "CDistantLights2::LoadData - error reading number of block's groups");

		CFileMgr::CloseFile(fileId);

#if __ASSERT
		for(u32 i = 0; i < m_numGroups; ++i)
		{
			vfxFatalAssertf(m_groups[i].pointCount <= MAX_POINTS_PER_GROUP, "Too many points, this will cause array out of bounds");
			vfxFatalAssertf((m_groups[i].pointOffset + m_groups[i].pointCount) <= DISTANTLIGHTS2_MAX_POINTS, "Too many points, this will cause array out of bounds");
		}
#endif // __ASSERT
	}

	CFileMgr::SetDir("");

#if !__NO_OUTPUT
	s16 maxPointCount = 0;
	for(int i = 0; i < m_numGroups; i++)
	{
		maxPointCount = Max(maxPointCount, m_groups[i].pointCount);
	}

	Displayf("@@@@@@ MAX POINT COUNT: %d", maxPointCount);
#endif // !__NO_OUTPUT
}


///////////////////////////////////////////////////////////////////////////////
//  FindBlock
///////////////////////////////////////////////////////////////////////////////

void 			CDistantLights2::FindBlock			(float posX, float posY, s32& blockX, s32& blockY)
{
	vfxAssertf(posX>=WORLDLIMITS_REP_XMIN && posX<=WORLDLIMITS_REP_XMAX, "CDistantLights2::FindBlock - posX out of range");
	vfxAssertf(posY>=WORLDLIMITS_REP_YMIN && posY<=WORLDLIMITS_REP_YMAX, "CDistantLights2::FindBlock - posY out of range");

	blockX = (s32)((posX-WORLDLIMITS_REP_XMIN)*DISTANTLIGHTS2_MAX_BLOCKS/(WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN));
	blockY = (s32)((posY-WORLDLIMITS_REP_YMIN)*DISTANTLIGHTS2_MAX_BLOCKS/(WORLDLIMITS_REP_YMAX-WORLDLIMITS_REP_YMIN));

	vfxAssertf(blockX>=0 && blockX<DISTANTLIGHTS2_MAX_BLOCKS, "CDistantLights2::FindBlock - blockX out of range");
	vfxAssertf(blockY>=0 && blockY<DISTANTLIGHTS2_MAX_BLOCKS, "CDistantLights2::FindBlock - blockX out of range");
}

bool CDistantLights2::IsInBlock( s32 groupIndex, s32 blockX, s32 blockY ) const
{
	const DistantLightGroup2_t& group = m_groups[groupIndex];
	u32 endPoint = group.pointOffset + group.pointCount;

	for(int i = group.pointOffset; i < endPoint; ++i)
	{
		if( static_cast<s32>((m_points[i].x-WORLDLIMITS_REP_XMIN)*DISTANTLIGHTS2_MAX_BLOCKS/(WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN)) == blockX &&
			static_cast<s32>((m_points[i].y-WORLDLIMITS_REP_YMIN)*DISTANTLIGHTS2_MAX_BLOCKS/(WORLDLIMITS_REP_YMAX-WORLDLIMITS_REP_YMIN)) == blockY )
		{
			return true;
		}
	}

	return false;
}

float CDistantLights2::GetActualCarLodDistance()
{
	// Called from multiple threads
	return g_LodScale.GetPerLodLevelScale(LODTYPES_DEPTH_ORPHANHD) * CVehicleModelInfo::GetLodMultiplierBiasAsDistanceScalar()
#if USE_DISTANT_LIGHTS_2
		* g_distLightsParam.carPopLodDistance
#endif
		;
}


#if __BANK

#define COMPATIBLE_NODE_SCORE 100.0f

void CDistantLights2::FindAttachedLinks(DistantLightTempLinks& links, DistantLightAddedLinks& addedLinks, s32 linkIdx, s32 nodeIdx, CTempLink* pTempLinks, s32 numTempLinks)
{
	for(s32 l = 0; l < numTempLinks; l++)
	{
		CTempLink& adjLink = pTempLinks[l];

		if(l != linkIdx && !addedLinks[l] && (adjLink.m_Node1 == nodeIdx || adjLink.m_Node2 == nodeIdx))
		{
			addedLinks[l] = true;

			if(!adjLink.m_bIgnore)
			{
				s32 otherNodeId = (adjLink.m_Node1 == nodeIdx) ? adjLink.m_Node2 : adjLink.m_Node1;
				// Mark processed links as incompatible
				links.Push(OutgoingLink(l, otherNodeId));
			}
		}
	}
}

// Starting with a link and a node, look for an "outgoing" link that is adjacent to the same node that has 
// similar properties to the link (potentially considering the outgoing link's next node too)
OutgoingLink CDistantLights2::FindNextCompatibleLink(s32 linkIdx, s32 nodeIdx, CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks)
{
	CTempLink& link = pTempLinks[linkIdx];
	CTempNode& node = pTempNodes[nodeIdx];

	// Keep track of the links that have already been added/looked at.
	bool *data = Alloca(bool, numTempLinks);
	sysMemSet(data, 0, numTempLinks * sizeof(bool));
	DistantLightAddedLinks addedLinks(data, numTempLinks);
	addedLinks.Resize(numTempLinks);

	// The links attached to tis node.
	DistantLightTempLinks adjLinks;
	FindAttachedLinks(adjLinks, addedLinks, linkIdx, nodeIdx, pTempLinks, numTempLinks);

	// The links of interest.
	DistantLightTempLinks outgoingLinks;

	while(!adjLinks.empty())
	{
		OutgoingLink& tmpLink = adjLinks.Pop();
		CTempLink& adjLink = pTempLinks[tmpLink.m_linkIdx];

		s32 otherNodeId = (adjLink.m_Node1 == nodeIdx) ? adjLink.m_Node2 : adjLink.m_Node1;
		if (tmpLink.m_linkIdx != linkIdx) // don't add self
		{
			// Ignore special nodes but use a node it is connected to.
			if(adjLink.m_bDontUseForNavigation)
			{
				FindAttachedLinks(adjLinks, addedLinks, tmpLink.m_linkIdx, otherNodeId, pTempLinks, numTempLinks);
			}
			else
			{
				// add link.
				outgoingLinks.Push(tmpLink);
			}
		}
	}

	s32 bestMatch = -1;
	float bestScore = -FLT_MAX;

	//	int totalLanes = link.m_Lanes1To2 + link.m_Lanes2To1;

	int prevNodeIdx = link.m_Node1 == nodeIdx ? link.m_Node2 : link.m_Node1;
	Vector3 prevToCurr = (node.m_Coors - pTempNodes[prevNodeIdx].m_Coors);
	prevToCurr.Normalize();

	// Go through the candidate links, look for a match
	for(int i = 0; i < outgoingLinks.GetCount(); i++)
	{
		CTempLink& outLink = pTempLinks[outgoingLinks[i].m_linkIdx];
		CTempNode& outNode = pTempNodes[outgoingLinks[i].m_nodeIdx];

		// Try to keep the road from turning through intersections, so the score is the dot product between the incoming and outgoing links
		Vector3 currToNext =(outNode.m_Coors - node.m_Coors);
		currToNext.Normalize();

		float score = currToNext.Dot(prevToCurr) * 10.0f;

		if(score > 0.0f)
		{
			if(!outLink.m_bProcessed)
			{
				score += 1.0f;
			}
			// favor nodes that have similar densities.
			if(outNode.m_Density != node.m_Density)
			{
				score -= 1.0f;
			}

			// favor nodes that have similar speeds.
			if(outNode.m_Speed != node.m_Speed)
			{
				score -= 1.0f;
			}

			// favor nodes that are not traveling in the opposite direction.
			if((outLink.m_Node2 == nodeIdx && outLink.m_Lanes1To2 <= 0) || (outLink.m_Node1 == nodeIdx && outLink.m_Lanes2To1 <= 0))
			{
				score -= 1.0f;
			}

			// Prefer same types of roads.
			if((outLink.m_Lanes1To2 + outLink.m_Lanes2To1) != (link.m_Lanes1To2 + link.m_Lanes2To1))
			{
				score -= 1.0f;
			}

			if (score > bestScore)
			{
				bestScore = score;
				bestMatch = i;
			}
		}
	}

	if (bestMatch >= 0)
	{
		return outgoingLinks[bestMatch];
	}
	else
	{
		return OutgoingLink(-1, -1);
	}
}

void CDistantLights2::RenderDebugData()
{
	if(m_drawGroupLocations)
	{
		for(s32 i = 0; i < m_numGroups; ++i)
		{
			grcDebugDraw::Sphere(
				Vector3(m_groups[i].centreX, m_groups[i].centreY, m_groups[i].centreZ),
				m_groups[i].radius,
				Color32(255, 255, 0),
				false );

		}
	}

	if(m_drawBlockLocations)
	{
		for(s32 x = 0; x < DISTANTLIGHTS2_MAX_BLOCKS; ++x)
		{
			for(s32 y = 0; y < DISTANTLIGHTS2_MAX_BLOCKS; ++y)
			{
				float blockMinX, blockMinY, blockMaxX, blockMaxY;
				DistantLightsVisibility::FindBlockMin(x, y, blockMinX, blockMinY);
				DistantLightsVisibility::FindBlockMax(x, y, blockMaxX, blockMaxY);

				grcDebugDraw::BoxAxisAligned(
					Vec3V(blockMinX, blockMinY, 20.0f - DISTANTLIGHTS_BLOCK_RADIUS),
					Vec3V(blockMaxX, blockMaxY, 20.0f + DISTANTLIGHTS_BLOCK_RADIUS),
					Color32(0, 255, 255),
					false );
			}
		}
	}
}

void CDistantLights2::UpdateNodeGeneration()
{
	if(m_pTempLinks && m_pTempNodes)
	{
		BuildDataStep(m_pTempNodes, m_pTempLinks, m_numTempLinks);
	}
	if(m_renderTempCacheLinks)
	{
		if(!m_pTempLinks || !m_pTempNodes)
		{
			LoadTempCacheData();
		}

		for(int i = 0; i < m_numTempLinks; ++i)
		{
			const CTempLink& link = m_pTempLinks[i];
			const CTempNode& node1 = m_pTempNodes[link.m_Node1];
			const CTempNode& node2 = m_pTempNodes[link.m_Node2];

			if(IsValidLink(link, node1, node2))
			{
				grcDebugDraw::Line(node1.m_Coors, node2.m_Coors, Color32(255,0,0),Color32(255,255,255));
			}
		}
	}

	// Draw create process!
	if(m_status != STATUS_FINISHED)
	{
		for(s32 group = 0; group < m_numGroups; ++group)
		{
			// -1 as we want the second to last.
			s32 endPoint = m_groups[group].pointOffset +  m_groups[group].pointCount -1;
			s32 lastPoint = endPoint - 1;

			for(s32 point = m_groups[group].pointOffset; point < endPoint; ++point)
			{
				Vector3 start(m_points[point].x, m_points[point].y, m_points[point].z);
				Vector3 end(m_points[point+1].x, m_points[point+1].y, m_points[point+1].z);

				vfxAssertf(end != Vector3(0, 0, 0), "Invalid point!");
				if ( point == m_groups[group].pointOffset )
				{
					grcDebugDraw::Line(start, end, Color32(255,255,0),Color32(0,0,255));
				}
				else if ( point == lastPoint )
				{
					grcDebugDraw::Line(start, end, Color32(0,255,0), Color32(255,0,255));
				}
				else
				{
					grcDebugDraw::Line(start, end, Color32(0, 255, 0), Color32(0, 0, 255));
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// BuildData
//////////////////////////////////////////////////////////////////////////
void			CDistantLights2::BuildData			(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks)
{
	// Ensure build state information is initialized!
	Init(INIT_CORE);

	// As this tool can run after the original tool, we need to reset some flags.
	for(s32 i = 0; i < numTempLinks; ++i)
	{
		pTempLinks[i].m_bIgnore = 0;
		pTempLinks[i].m_bProcessed = 0;
	}

	// This is called from the tool so we do not need to stagger building.
	CFileMgr::SetDir("common:/data/paths/");
	FileHandle fileId = CFileMgr::OpenFileForWriting("distantlights_input.dat");

	// NOTE: we do not worry about endien-ness as this is for tool code.
	if (CFileMgr::IsValidFileHandle(fileId))
	{

		int bytes = CFileMgr::Write(fileId, (char*)(&numTempLinks), sizeof(s32));
		vfxAssertf(bytes==4, "CDistantLights2::BuildData - error writing number of links");

		bytes = CFileMgr::Write(fileId, (char*)pTempNodes, numTempLinks*sizeof(CTempNode));
		vfxAssertf(bytes==(int)(numTempLinks*sizeof(CTempNode)), "CDistantLights2::BuildData - error writing nodes");

		bytes = CFileMgr::Write(fileId, (char*)pTempLinks, numTempLinks*sizeof(CTempLink));
		vfxAssertf(bytes==(int)(numTempLinks*sizeof(CTempLink)), "CDistantLights2::BuildData - error writing links");

		CFileMgr::CloseFile(fileId);
	}

	CFileMgr::SetDir("");

	LoadTempCacheData();
	ChangeState(STATUS_INIT);

	while(m_status != STATUS_FINISHED)
	{
		BuildDataStep(pTempNodes, pTempLinks, numTempLinks);
	}
}

//////////////////////////////////////////////////////////////////////////
// GenerateData
//////////////////////////////////////////////////////////////////////////

void 			CDistantLights2::GenerateData			()
{
	LoadTempCacheData();
	ChangeState(STATUS_INIT);
}

///////////////////////////////////////////////////////////////////////////////
//  BuildData_Impl
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights2::BuildDataStep			(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks)
{
	switch (m_status)
	{
	case CDistantLights2::STATUS_FINISHED:
		break;

	case CDistantLights2::STATUS_INIT:
		BuildDataInit();
		break;

	case CDistantLights2::STATUS_CULLING:
		BuildDataCull(pTempNodes, pTempLinks, numTempLinks);
		break;

	case CDistantLights2::STATUS_BUILDING:
		if(m_generateDebugGroup)
		{
			BuildDebugDataBuildNetworkStep();
		}
		else
		{ 
			sysTimer timer;

			while(timer.GetMsTime() < m_msToGeneratePerFrame && m_status == STATUS_BUILDING)
			{
				BuildDataBuildNetworkStep(pTempNodes, pTempLinks, numTempLinks);
			}
		}
		break;

	case CDistantLights2::STATUS_SPLITTING:
		BuildDataSplit(pTempNodes, pTempLinks);
		break;

	case CDistantLights2::STATUS_SORTING:
		BuildDataSort();
		break;

	case CDistantLights2::STATUS_SAVING:
		SaveData();
		break;

	default:
		break;
	}
}

void CDistantLights2::BuildDataInit()
{
	// Clear absolutely everything out!
	m_numPoints = 0;
	m_numGroups = 0;
	sysMemSet(m_points, 0, sizeof(DistantLightPoint_t) * DISTANTLIGHTS2_MAX_POINTS);
	sysMemSet(m_groups, 0, sizeof(DistantLightGroup2_t) * DISTANTLIGHTS2_MAX_GROUPS);
	sysMemSet(m_blockFirstGroup, 0, sizeof(s32) * (DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS));
	sysMemSet(m_blockNumCoastalGroups, 0, sizeof(s32) * (DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS));
	sysMemSet(m_blockNumInlandGroups, 0, sizeof(s32) * (DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS));

	ChangeState(STATUS_CULLING);
}

bool CDistantLights2::IsValidLink( const CTempLink& link, const CTempNode& node1, const CTempNode& node2 )
{
	if (node1.m_Coors.z < 0.0f || node2.m_Coors.z < 0.0f)
	{
		return false;
	}

	// HACK - also ignore points too far south (i.e. the prologue road network)
	if (node1.m_Coors.y < -3500.0f || node2.m_Coors.y < -3500.0f)
	{
		return false;
	}

	if (node1.IsPedNode() || node2.IsPedNode() )
	{
		return false;
	}

	if (node1.m_bWaterNode || node2.m_bWaterNode)
	{
		return false;
	}

	if (node1.m_bSwitchedOff || node2.m_bSwitchedOff)
	{
		return false;
	}

	if(node1.m_bOffroad || node2.m_bOffroad)
	{
		return false;
	}


	else if(link.m_bShortCut)
	{
		return false;
	}

	// 		// Ignore special case nodes
	// 		else if (node1.m_SpecialFunction != SPECIAL_USE_NONE || node2.m_SpecialFunction != SPECIAL_USE_NONE)
	// 		{
	//			return false;
	// 		}

	return true;
}

void CDistantLights2::BuildDataCull( CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks )
{
	for(int l = 0; l < numTempLinks; ++l)
	{
		CTempLink& link = pTempLinks[l];

		link.m_bProcessed = false;

		// 		if (link.m_Lanes1To2 + link.m_Lanes2To1 < 2)
		// 		{
		// 			link.m_bProcessed = true;
		// 		}

		if(pTempNodes[link.m_Node1].m_bSwitchedOff || pTempNodes[link.m_Node2].m_bSwitchedOff || link.m_bShortCut)
		{
			link.m_bProcessed = true;
			link.m_bIgnore = true;
		}
		else if(!IsValidLink(link, pTempNodes[link.m_Node1], pTempNodes[link.m_Node2]))
		{
			link.m_bProcessed = true;
			link.m_bIgnore = true;
		}
	}

	ChangeState(STATUS_BUILDING);
}

void CDistantLights2::UpdateGroupPositionAndSize( DistantLightGroup2_t* group )
{
	if(vfxVerifyf(group, "Cannot calculate position and size as group is null!"))
	{
		Vec3V vCentrePoint = Vec3V(V_ZERO);
		const s16 endPoint = group->pointOffset + group->pointCount;

		for (s32 p = group->pointOffset; p < endPoint; p++)
		{
			vCentrePoint += Vec3V((float)m_points[p].x, (float)m_points[p].y, (float)m_points[p].z);
		}

		vCentrePoint /= ScalarVFromF32(group->pointCount);

		ScalarV radiusSqr(V_ZERO);
		for (s32 p = group->pointOffset; p < endPoint; p++)
		{
			ScalarV	distFromCentreSqr = MagSquared(Vec3V((float)m_points[p].x, (float)m_points[p].y, (float)m_points[p].z) - vCentrePoint);
			radiusSqr = Max(radiusSqr, distFromCentreSqr);
		}

		group->centreX = (s16)vCentrePoint.GetXf();
		group->centreY = (s16)vCentrePoint.GetYf();
		group->centreZ = (s16)vCentrePoint.GetZf();
		group->radius = (s16)Sqrt(radiusSqr).Getf();


		vfxAssertf(group->centreX != 0 || group->centreY != 0 || group->centreZ != 0, "Invalid group position!");
	}
}

void CDistantLights2::BuildDataBuildNetworkStep( CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks )
{
	bool pathGenerated = false;
	if (!pTempLinks[m_currentLinkIndex].m_bProcessed)
	{
		s32 node1 = pTempLinks[m_currentLinkIndex].m_Node1;

		// Find the first link in this group, by walking in the node2->node1 direction.
		// Then accumulate links along the chain from that first node

		OutgoingLink origLink(m_currentLinkIndex, node1);
		OutgoingLink currLink(m_currentLinkIndex, node1);
		OutgoingLink nextLink(m_currentLinkIndex, node1);
		int loopCheck = 0;

		while(nextLink.IsValid())
		{
			currLink = nextLink;

			nextLink = FindNextCompatibleLink(currLink.m_linkIdx, currLink.m_nodeIdx, pTempNodes, pTempLinks, numTempLinks);
			if (loopCheck++ == 100 || (nextLink.IsValid() && pTempLinks[nextLink.m_linkIdx].m_bProcessed))
			{
				// probably a loop, definitely a long enough chain to start processing, so start from here.
				break;
			}
		}

		// currLink is now the start of our chain
		CTempLink& startLink = pTempLinks[currLink.m_linkIdx];
		s32 startNodeIdx = currLink.m_nodeIdx;
		s32 nextNodeIdx = startNodeIdx == startLink.m_Node1 ? startLink.m_Node2 : startLink.m_Node1;

		// Start a new group.
		bool bDoneWithGroup = false;
		m_groups[m_numGroups].pointOffset = (s16)m_numPoints;
		m_points[m_numPoints].x = (s16)pTempNodes[startNodeIdx].m_Coors.x;
		m_points[m_numPoints].y = (s16)pTempNodes[startNodeIdx].m_Coors.y;
		m_points[m_numPoints].z = (s16)pTempNodes[startNodeIdx].m_Coors.z;

		m_points[m_numPoints+1].x = (s16)pTempNodes[nextNodeIdx].m_Coors.x;
		m_points[m_numPoints+1].y = (s16)pTempNodes[nextNodeIdx].m_Coors.y;
		m_points[m_numPoints+1].z = (s16)pTempNodes[nextNodeIdx].m_Coors.z;

		m_groups[m_numGroups].density = pTempNodes[startNodeIdx].m_Density;
		m_groups[m_numGroups].speed = pTempNodes[startNodeIdx].m_Speed;
		m_groups[m_numGroups].numLanes = startLink.m_Lanes1To2 + startLink.m_Lanes2To1;
		m_groups[m_numGroups].twoWay = (startLink.m_Lanes1To2 > 0 && startLink.m_Lanes2To1 > 0);
		m_groups[m_numGroups].distanceOffset = 0;
		m_groups[m_numGroups].randomSeed = (s32)(size_t)(&m_groups[m_numGroups]);

		// Add relevant build information.
		m_nodeGenerationData[m_numPoints].m_LinkIndex = currLink.m_linkIdx;
		m_nodeGenerationData[m_numPoints].m_NodeFrom = startNodeIdx;
		m_nodeGenerationData[m_numPoints].m_NodeTo = nextNodeIdx;
		m_nodeGenerationData[m_numPoints].m_IsFlipped = false;

		// Point our link object in the right direction for iteration
		currLink.m_nodeIdx = nextNodeIdx;

		// We have not traveled in a direction yet.
		Vec3V prevSectionHeading(V_ZERO);
		Vec3V prevLinkHeading(V_ZERO);
		Vec3V prevNodePos(V_ZERO);
		bool firstNode = true;

		const float weightDecreasePerMeter = (1.0f - ms_NodeGenMaxAngle) / ms_NodeGenBuildNodeRage;

		while ( (!bDoneWithGroup) && (nextLink = FindNextCompatibleLink(currLink.m_linkIdx, currLink.m_nodeIdx, pTempNodes, pTempLinks, numTempLinks)).IsValid() )
		{
			// Calculate the distance from the previous node to this new one.
			const Vec3V currPos((float)m_points[m_numPoints].x, (float)m_points[m_numPoints].y, (float)m_points[m_numPoints].z);
			const Vec3V nextPos = RCC_VEC3V(pTempNodes[nextLink.m_nodeIdx].m_Coors);
			const Vec3V sectionHeading = nextPos - currPos;
			const Vec3V normSectionHeading = Normalize(sectionHeading);

			if(firstNode)
			{
				firstNode = false;
				prevSectionHeading = normSectionHeading;
				prevLinkHeading = normSectionHeading;
				prevNodePos = currPos;
			}

			const float sectionMag = Mag(sectionHeading).Getf();

			const float dotProdDecrease = sectionMag * weightDecreasePerMeter;
			const Vec3V linkHeading = Normalize(nextPos - prevNodePos);
			const float sectionDotProd = Dot(prevSectionHeading, normSectionHeading).Getf() - dotProdDecrease;
			const float linkDotProd = Dot(prevLinkHeading, linkHeading).Getf() - dotProdDecrease;

			prevNodePos = nextPos;
			prevLinkHeading = linkHeading;
			
			// As we are decreasing the section and link dot products based on weightDecreasePerMeter, we will add a new node at least every
			// ms_NodeGenBuildNodeRage meters, even if the direction continues in a straight line. We will also add more nodes on corners.
			if (sectionDotProd < ms_NodeGenMaxAngle || linkDotProd < ms_NodeGenMaxAngle)
			{			// The new found node is too far from our start node. Add a new node to the list.
				if (m_numPoints - m_groups[m_numGroups].pointOffset < MAX_GENERATION_POINT_PER_GROUP)		// Make sure groups aren't too long.
				{
					m_numPoints++;
					prevSectionHeading = sectionHeading;
				}
				else
				{
					bDoneWithGroup = true;
				}
			}

			if(pTempLinks[nextLink.m_linkIdx].m_bProcessed)
			{
				bDoneWithGroup = true;
			}

			// Update the previous node/link data so that it goes to this node.
			m_nodeGenerationData[m_numPoints].m_NodeTo = nextLink.m_nodeIdx;

			// Add this node coords to the next node in the list.
			m_points[m_numPoints+1].x = (s16)pTempNodes[nextLink.m_nodeIdx].m_Coors.x;
			m_points[m_numPoints+1].y = (s16)pTempNodes[nextLink.m_nodeIdx].m_Coors.y;
			m_points[m_numPoints+1].z = (s16)pTempNodes[nextLink.m_nodeIdx].m_Coors.z;

			// Use the current link and node to setup the next link even though this point might not go anywhere.
			m_nodeGenerationData[m_numPoints+1].m_LinkIndex = nextLink.m_linkIdx;
			m_nodeGenerationData[m_numPoints+1].m_NodeFrom = nextLink.m_nodeIdx;
			
			// Currently, this link goes no where so use this node id.
			m_nodeGenerationData[m_numPoints+1].m_NodeTo = nextLink.m_nodeIdx;

			const CTempLink& tmpLink = pTempLinks[nextLink.m_linkIdx];
			m_nodeGenerationData[m_numPoints+1].m_IsFlipped = (tmpLink.m_Node1 == nextLink.m_nodeIdx && tmpLink.m_Lanes2To1 == 0) || (tmpLink.m_Node2 == nextLink.m_nodeIdx && tmpLink.m_Lanes1To2 == 0);

			pTempLinks[nextLink.m_linkIdx].m_bProcessed = true;
			currLink = nextLink;
		}

		m_numPoints+=2;

		DistantLightGroup2_t& group =  m_groups[m_numGroups];

		// Now we decide whether this group is long enough to be worth keeping.
		group.pointCount = (s16)(m_numPoints - group.pointOffset);

		// Calculate the length.
		float length = 0.0f;
		Vec3V currPoint((float)m_points[group.pointOffset].x, (float)m_points[group.pointOffset].y, (float)m_points[group.pointOffset].z);
		for(s16 i = group.pointOffset +1; i < (group.pointOffset + group.pointCount); ++i)
		{
			Vec3V nextPoint((float)m_points[i].x, (float)m_points[i].y, (float)m_points[i].z);
			length += Mag(nextPoint - currPoint).Getf();
			currPoint = nextPoint;
		}

		if (group.pointCount <= MIN_POINTS_PER_GROUP || length < ms_NodeGenMinGroupLength)		// Need at least a number of points in group to keep it.
		{
			m_numPoints = group.pointOffset;		// Set number of point back to what it was
		}
		else
		{			// We're keeping this group. Calculate centre point and radius for sprite on-screen check
			pathGenerated = true;

			UpdateGroupPositionAndSize(&group);

			// This is a fresh group so set the distance offset to 0.
			group.distanceOffset = 0;

			// Also check to see if this group should use streetlights
			// Right now the heuristic is density < 2 => no streetlights
			group.useStreetlights = group.density > 2;

			m_numGroups++;
		}

		const s32 iMaxGroups = DISTANTLIGHTS2_MAX_GROUPS-1;
		const s32 iMaxPoints = DISTANTLIGHTS2_MAX_POINTS-20;

		vfxAssertf(m_numGroups < iMaxGroups, "CDistantLights2::BuildData - too many groups found (%i / %i)", m_numGroups, iMaxGroups);
		vfxAssertf(m_numPoints < iMaxPoints, "CDistantLights2::BuildData - too many points found (%i / %i)", m_numPoints, iMaxPoints);

#if __BANK
		if(m_numGroups >= m_maxGroupGeneration)
		{
			m_currentLinkIndex = numTempLinks;
		}
#endif // __BANK

		if(m_numGroups >= iMaxGroups)
		{
			Assertf(false, "CDistantLights2::BuildData - I'm gonna stop building lights now, the game won't crash - but the lights data will be fucked.");
			m_currentLinkIndex = numTempLinks; // effectively a break!
		}
		if(m_numPoints >= iMaxPoints)
		{
			Assertf(false, "CDistantLights2::BuildData - I'm gonna stop building lights now, the game won't crash - but the lights data will be fucked.");
			m_currentLinkIndex = numTempLinks; // effectively a break!
		}
	}

	// only update the node index if this node was actually used or there is no valid path for this node!
	if(m_currentLinkIndex < numTempLinks && (pTempLinks[m_currentLinkIndex].m_bProcessed || !pathGenerated))
	{
		++m_currentLinkIndex;
	}

	if(m_currentLinkIndex >= numTempLinks)
	{
		ChangeState(STATUS_SPLITTING);
	}
	else
	{
		float percent = (m_currentLinkIndex / static_cast<float>(numTempLinks)) * 100.0f;
		formatf(m_statusText, "Building %.2f%%", percent);
	}
}

void CDistantLights2::BuildDebugDataBuildNetworkStep()
{
	DistantLightGroup2_t& group = m_groups[m_numGroups++];
	sysMemSet(&group, 0, sizeof(DistantLightGroup2_t));
	group.randomSeed = (s32)(size_t)(&group);
	group.numLanes = 2;
	group.density = 15;
	group.speed = 2;
	group.twoWay = true;

	s16 length = 0;
	s16 count = 0;
	s16 step = static_cast<s16>(m_debugGroupLinkLength);

	DistantLightPoint_t* current = &m_points[m_numPoints++];
	current->x = static_cast<s16>(m_debugGroupXStart);
	current->y = static_cast<s16>(m_debugGroupYStart);
	current->z = static_cast<s16>(m_debugGroupZStart);

	DistantLightPoint_t* last = current;
	while (length < m_debugGroupLength)
	{
		current = &m_points[m_numPoints++];
		current->x = last->x + step;
		current->y = last->y;
		current->z = last->z;

		last = current;

		length += step;
		++count;
	}

	group.pointCount = count;
	UpdateGroupPositionAndSize(&group);
	ChangeState(STATUS_SPLITTING);
}

void CDistantLights2::LoadTempCacheData()
{
	// NOTE: we do not worry about endien-ness as this is for tool code.
	FileHandle fileId;
	fileId	= CFileMgr::OpenFile("common:/data/paths/distantlights_input.dat");

	if (CFileMgr::IsValidFileHandle(fileId))
	{
		ClearBuildData();

		s32 bytes =  CFileMgr::Read(fileId, (char*)&m_numTempLinks, sizeof(s32));
		vfxAssertf(bytes==4, "CDistantLights2::GenerateData - error reading number nodes");

		m_pTempLinks = rage_new CTempLink[m_numTempLinks];
		m_pTempNodes = rage_new CTempNode[m_numTempLinks];

		if(m_pTempLinks && m_pTempNodes)
		{
			bytes = CFileMgr::Read(fileId,(char*)m_pTempNodes, m_numTempLinks*sizeof(CTempNode));
			vfxAssertf(bytes==(s32)(m_numTempLinks*sizeof(CTempNode)), "CDistantLights2::GenerateData - error reading nodes");

			bytes = CFileMgr::Read(fileId, (char*)m_pTempLinks, m_numTempLinks*sizeof(CTempLink));
			vfxAssertf(bytes==(s32)(m_numTempLinks*sizeof(CTempLink)), "CDistantLights2::GenerateData - error reading nodes");
		}

		CFileMgr::CloseFile(fileId);
	}
}

void CDistantLights2::BuildDataSplit( CTempNode* pTempNodes, CTempLink* pTempLinks )
{
	s32 group = 0;
	while(group < m_numGroups)
	{
		DistantLightGroup2_t* currentGroup = &m_groups[group];

		// We get the block of the first point and use this x/y as the current block.
		s32 blockX, blockY;
		FindBlock(m_points[currentGroup->pointOffset].x, m_points[currentGroup->pointOffset].y, blockX, blockY);

		const s16 numPoints = currentGroup->pointCount;
		const s16 endPoint = currentGroup->pointOffset + numPoints;
		s32 currentBlockX, currentBlockY;

		bool groupSplit = false;

		// Go through all the points in this group to check if they cross blocks.
		for(s16 currentPoint = currentGroup->pointOffset + 1; currentPoint < endPoint && !groupSplit; ++currentPoint)
		{
			const s16 currentOffset = currentPoint - currentGroup->pointOffset;
			vfxFatalAssertf(currentOffset <= MAX_POINTS_PER_GROUP, "Group is too long, generated data could crash the game!");

			FindBlock(m_points[currentPoint].x, m_points[currentPoint].y, currentBlockX, currentBlockY);

			// if the point is in another block or the group is too long then split the group up.
			if(currentBlockX != blockX || currentBlockY != blockY || currentOffset >= (MAX_POINTS_PER_GROUP -1))
			{
				groupSplit = SplitGroup(pTempNodes, pTempLinks, currentGroup, currentOffset);
			}
			// Do not perform these steps when generating debug roads as no nodes or links were used to generate them.
			else if(!m_generateDebugGroup)
			{
				const CTempLink& link = m_pTempLinks[m_nodeGenerationData[currentPoint].m_LinkIndex];
				const CTempLink& prevLink = m_pTempLinks[m_nodeGenerationData[currentPoint -1].m_LinkIndex];
				const CTempNode& node = m_pTempNodes[m_nodeGenerationData[currentPoint].m_NodeTo];
				const CTempNode& prevNode = m_pTempNodes[m_nodeGenerationData[currentPoint -1].m_NodeTo];

				if(link.m_Lanes1To2 != prevLink.m_Lanes1To2 ||	link.m_Lanes2To1 != prevLink.m_Lanes2To1)
				{
					groupSplit = SplitGroup(pTempNodes, pTempLinks, currentGroup, currentOffset);
				}
				else if(node.m_Density != prevNode.m_Density)
				{
					groupSplit = SplitGroup(pTempNodes, pTempLinks, currentGroup, currentOffset);
				}
// 				else if(node.m_Speed != prevNode.m_Speed)
// 				{
// 					groupSplit = SplitGroup(pTempNodes, pTempLinks, currentGroup, currentOffset);
// 				}
			}
		}

		++group;
	}

	// now that groups have been split and new groups created we need to update their sizes and directions.
	for(s32 i = 0; i < m_numGroups; ++i)
	{
		UpdateGroupPositionAndSize(&m_groups[i]);
		UpdateGroupDirection(&m_groups[i]);
	}

	ChangeState(STATUS_SORTING);
}

void CDistantLights2::BuildDataSort()
{
	s32 sortedGroups = 0;
	for (s32 blockX=0; blockX<DISTANTLIGHTS2_MAX_BLOCKS; blockX++)
	{
		for (s32 blockY=0; blockY<DISTANTLIGHTS2_MAX_BLOCKS; blockY++)
		{
			s32 totalNumGroupsForBlock = 0;
			m_blockFirstGroup[blockX][blockY] = static_cast<s16>(sortedGroups);

			// Go through the groups and collect the ones that fall into this block.
			for (s32 s=sortedGroups; s<m_numGroups; s++)
			{
				s32 groupBlockX, groupBlockY;
				FindBlock(m_groups[s].centreX, m_groups[s].centreY, groupBlockX, groupBlockY);

				if (blockX == groupBlockX && blockY == groupBlockY)
				{
					DistantLightGroup2_t tempGroup = m_groups[s];
					m_groups[s] = m_groups[sortedGroups];
					m_groups[sortedGroups] = tempGroup;

					sortedGroups++;
					totalNumGroupsForBlock++;
				}
			}

			// Now within this block put the coastal ones first on the list and then the inland ones.
			bool bChangeMade = true;
			s32 firstGroup = m_blockFirstGroup[blockX][blockY];

			while (bChangeMade)
			{
				bChangeMade = false;

				for (s32 i=0; i<totalNumGroupsForBlock-1; i++)
				{
					bool bFirstCoastal = (Water::FindNearestWaterHeightFromQuads(m_groups[i+firstGroup].centreX, m_groups[i+firstGroup].centreY) < 50.0f);
					bool bSecondCoastal = (Water::FindNearestWaterHeightFromQuads(m_groups[i+firstGroup+1].centreX, m_groups[i+firstGroup+1].centreY) < 50.0f);

					if (bSecondCoastal && !bFirstCoastal)
					{
						DistantLightGroup2_t tempGroup = m_groups[i+firstGroup];
						m_groups[i+firstGroup] = m_groups[i+firstGroup+1];
						m_groups[i+firstGroup+1] = tempGroup;
						bChangeMade = true;
					}
				}
			}

			// Now that they are sorted count how many coastal and inlands we have.
			for (s32 i=0; i<totalNumGroupsForBlock; i++)
			{
				bool isCoatal = (Water::FindNearestWaterHeightFromQuads(m_groups[i+firstGroup].centreX, m_groups[i+firstGroup].centreY) < 50.0f);
				if (isCoatal)
				{
					m_blockNumCoastalGroups[blockX][blockY]++;
				}
			}
			m_blockNumInlandGroups[blockX][blockY] = static_cast<s16>(totalNumGroupsForBlock - m_blockNumCoastalGroups[blockX][blockY]);
		}
	}

	vfxAssertf(sortedGroups == m_numGroups, "Some groups were not sorted into a grid block!");

	ChangeState(STATUS_SAVING);
	//ChangeState(STATUS_STOPPED);
}

void CDistantLights2::ClearBuildData()
{
	if(m_pTempLinks)
	{
		delete[] m_pTempLinks;
		m_pTempLinks = NULL;
	}

	if(m_pTempNodes)
	{
		delete[] m_pTempNodes;
		m_pTempNodes = NULL;
	}

	m_currentLinkIndex = 0;
	m_numTempLinks = 0;
}

bool CDistantLights2::SplitGroup( CTempNode* pTempNodes, CTempLink* pTempLinks, DistantLightGroup2_t* currentGroup, s16 newLength )
{
	bool split = false;

	if( newLength > 1 &&
		(currentGroup->pointCount - newLength) > 1 &&
		vfxVerifyf(currentGroup, "Cannot split group as null was passed in!")
		&& vfxVerifyf(m_numGroups < DISTANTLIGHTS2_MAX_GROUPS -1, "Exceeded maximum groups!") )
	{
		s16 endPoint = currentGroup->pointOffset + currentGroup->pointCount;
		s16 currentPoint = currentGroup->pointOffset + newLength;

		// If this is the last point in the road that happens to be in another group then do not bother
		// creating a group with one node!
		if(currentPoint < (endPoint - 1) && vfxVerifyf(m_numPoints < DISTANTLIGHTS2_MAX_POINTS, "Not enough points to split group!"))
		{
			currentGroup->pointCount = newLength + 1;
			vfxFatalAssertf(currentGroup->pointCount <= MAX_POINTS_PER_GROUP, "Group is too long, generated data could crash the game!");

			// We need to add the end of the last group's node again. This is because we might flip groups moving the shared point otherwise.
			for(s16 i = static_cast<s16>(m_numPoints); i > currentPoint; --i)
			{
				m_points[i] = m_points[i-1];
				m_nodeGenerationData[i] = m_nodeGenerationData[i-1];
			}

			++m_numPoints;

			// update groups offsets' to point to the new location.
			for(s16 i = 0; i < m_numGroups; ++i)
			{
				if(m_groups[i].pointOffset >= currentPoint)
				{
					++m_groups[i].pointOffset;
				}
			}

			// make a new group copied from the current group.
			DistantLightGroup2_t* tmpGroup = &m_groups[m_numGroups++];
			*tmpGroup = *currentGroup;


			// make relevant changes to the new group.
			tmpGroup->pointOffset = currentPoint +1;
			tmpGroup->pointCount = endPoint - currentPoint; // this is actually (endpoint +1) - (currentPoint +1)


			if(m_generateDebugGroup == false)
			{
				CTempLink currentGroupLink = pTempLinks[m_nodeGenerationData[currentGroup->pointOffset].m_LinkIndex];
				currentGroup->twoWay = (currentGroupLink.m_Lanes1To2 > 0 && currentGroupLink.m_Lanes2To1 > 0);
				currentGroup->numLanes = currentGroupLink.m_Lanes1To2 + currentGroupLink.m_Lanes2To1;
			}

			// now calculate the initial offset.
			tmpGroup->distanceOffset = currentGroup->distanceOffset;
			Vec3V start = Vec3V((float)m_points[currentGroup->pointOffset].x, (float)m_points[currentGroup->pointOffset].y, (float)m_points[currentGroup->pointOffset].z);
			for(s32 i = currentGroup->pointOffset + 1; i < (currentGroup->pointOffset + currentGroup->pointCount); ++i)
			{
				Vec3V end = Vec3V((float)m_points[i].x, (float)m_points[i].y, (float)m_points[i].z);
				tmpGroup->distanceOffset += Mag(start - end).Getf();
				start = end;
			}

			if(m_generateDebugGroup == false)
			{
				CTempLink& tmpGroupLink =  pTempLinks[m_nodeGenerationData[tmpGroup->pointOffset].m_LinkIndex];
				CTempNode& tmpGroupNode =  pTempNodes[m_nodeGenerationData[tmpGroup->pointOffset].m_NodeTo];

				tmpGroup->twoWay = (tmpGroupLink.m_Lanes1To2 > 0 && tmpGroupLink.m_Lanes2To1 > 0);
				tmpGroup->numLanes = tmpGroupLink.m_Lanes1To2 + tmpGroupLink.m_Lanes2To1;
				tmpGroup->density = tmpGroupNode.m_Density;
				tmpGroup->speed = tmpGroupNode.m_Speed;
				tmpGroup->useStreetlights = tmpGroupNode.m_Density > 2;
			}

			split = true;
		}
	}

	return split;
}

void CDistantLights2::UpdateGroupDirection( DistantLightGroup2_t* group )
{
	if(vfxVerifyf(group->pointCount > 1, "A group with one link has been created, this is a waist of memory!"))
	{
		// We use +1 (the second point) as the first and last node can be incorrect when we have split a road (as a node can be incompatible).
		const DistantLightNodeGenerationData& generationData = m_nodeGenerationData[group->pointOffset +1];

		// If we are traveling down the wrong way then swap the points.
		if(generationData.m_IsFlipped)
		{
			s16 lo = group->pointOffset;
			s16 hi = group->pointOffset + group->pointCount - 1;
			DistantLightPoint_t swap;

			while (lo < hi)
			{
				swap = m_points[lo];
				m_points[lo] = m_points[hi];
				m_points[hi] = swap;
				++lo;
				--hi;
			}
		}
	}
}

void CDistantLights2::ChangeState(BuildStatus status)
{
	m_status = status;

	// Reset loop index
	m_currentLinkIndex = 0;

	switch (m_status)
	{
	case CDistantLights2::STATUS_FINISHED:
		safecpy(m_statusText, "Finished");
		break;
	case CDistantLights2::STATUS_INIT:
		safecpy(m_statusText, "Init");
		break;
	case CDistantLights2::STATUS_CULLING:
		safecpy(m_statusText, "Culling");
		break;
	case CDistantLights2::STATUS_BUILDING:
		safecpy(m_statusText, "Building");
		break;
	case CDistantLights2::STATUS_SPLITTING:
		safecpy(m_statusText, "Splitting");
		break;
	case CDistantLights2::STATUS_SORTING:
		safecpy(m_statusText, "Sorting");
		break;
	case CDistantLights2::STATUS_SAVING:
		safecpy(m_statusText, "Saving");
		break;
	case CDistantLights2::STATUS_STOPPED:
		safecpy(m_statusText, "Stopped");
		break;
	default:
		safecpy(m_statusText, "Unknown");
		break;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SaveData
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights2::SaveData			()
{
	FileHandle fileId = CFileMgr::OpenFileForWriting(DISTANT_LIGHTS2_TEMP_DATA_PATH);

	if (CFileMgr::IsValidFileHandle(fileId))
	{
		int bytes = DistantLightSaveData::Write(fileId, (s32*)(&m_numPoints), 1);
		vfxAssertf(bytes==4, "CDistantLights2::SaveData - error writing number of points");

		bytes = DistantLightSaveData::Write(fileId, (s32*)(&m_numGroups), 1);
		vfxAssertf(bytes==4, "CDistantLights2::SaveData - error writing number of groups");

		bytes = DistantLightSaveData::Write(fileId, (s32*)m_blockFirstGroup, DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS);
		vfxAssertf(bytes==(int)(DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing block's first group");

		bytes = DistantLightSaveData::Write(fileId, (s32*)m_blockNumCoastalGroups, DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS);
		vfxAssertf(bytes==(int)(DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing number of block's coastal group");

		bytes = DistantLightSaveData::Write(fileId, (s32*)m_blockNumInlandGroups, DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS);
		vfxAssertf(bytes==(int)(DISTANTLIGHTS2_MAX_BLOCKS*DISTANTLIGHTS2_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing number of block's inland group");

		bytes = DistantLightSaveData::Write(fileId, (s16*)m_points, (m_numPoints*sizeof(DistantLightPoint_t))/sizeof(s16));
		vfxAssertf(bytes==(int)(m_numPoints*sizeof(DistantLightPoint_t)), "CDistantLights2::SaveData - error writing number of block's points");

		bytes = DistantLightSaveData::Write(fileId, m_groups, m_numGroups);
		vfxAssertf(bytes==(int)(m_numGroups*sizeof(DistantLightGroup2_t)), "CDistantLights2::SaveData - error writing number of block's groups");

		CFileMgr::CloseFile(fileId);

#if __ASSERT
		for(u32 i = 0; i < m_numGroups; ++i)
		{
			vfxFatalAssertf(m_groups[i].pointCount <= MAX_POINTS_PER_GROUP, "Too many points, this will cause array out of bounds");
			vfxFatalAssertf((m_groups[i].pointOffset + m_groups[i].pointCount) <= DISTANTLIGHTS2_MAX_POINTS, "Too many points, this will cause array out of bounds");
		}
#endif // __ASSERT
	}

	ChangeState(STATUS_FINISHED);
}

//////////////////////////////////////////////////////////////////////////
// LoadTempData
//////////////////////////////////////////////////////////////////////////
void CDistantLights2::LoadTempData()
{
	LoadData_Impl(DISTANT_LIGHTS2_TEMP_DATA_PATH);
	ChangeState(STATUS_FINISHED);
}


///////////////////////////////////////////////////////////////////////////////
//  FindNextNode
///////////////////////////////////////////////////////////////////////////////
s32			CDistantLights2::FindNextNode		(CTempLink* pTempLinks, s32 searchStart, s32 node, s32 numTempLinks)
{
	// find the next link that links to this node and that hasn't been processed yet
	for (s32 s=searchStart; s<numTempLinks; s++)
	{
		if (!pTempLinks[s].m_bProcessed)
		{
			if (pTempLinks[s].m_Node1==node)
			{
				pTempLinks[s].m_bProcessed = true;
				return pTempLinks[s].m_Node2;
			}

			if (pTempLinks[s].m_Node2==node)
			{
				pTempLinks[s].m_bProcessed = true;
				return pTempLinks[s].m_Node1;
			}	
		}
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////
void			CDistantLights2::InitWidgets	()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Distant Lights", false);
	{
		pVfxBank->AddToggle("Disable Rendering", &m_bDisableRendering);
		pVfxBank->AddToggle("Disable Vehicle Lights Rendering", &m_disableVehicleLights);
		pVfxBank->AddToggle("Disable Street Lights Rendering", &m_disableStreetLights); // TODO -- remove this?
		pVfxBank->AddToggle("Disable Street Lights Visibility", &m_disableStreetLightsVisibility);
		pVfxBank->AddToggle("Disable Depth Test", &m_disableDepthTest);

		pVfxBank->AddToggle("Toggle old sync method on / off", &g_hack_oldDistantCarsSync);

		pVfxBank->PushGroup("Distant Light Data");
		pVfxBank->AddSlider("Distant Car/Lights Max Block Range", &DistantLightsVisibilityData::ms_BlockMaxVisibleDistance, 0, WORLDLIMITS_REP_XMAX, 0.1f);
		pVfxBank->AddToggle("Draw Distant Lights/Cars Location", &m_drawDistantLightsLocations);
		pVfxBank->AddToggle("Draw Distant Lights/Cars Nodes", &m_drawDistantLightsNodes);
		pVfxBank->AddToggle("Draw Distant Lights/Cars Groups", &m_drawGroupLocations);
		pVfxBank->AddToggle("Draw Distant Lights/Cars Blocks", &m_drawBlockLocations);
		pVfxBank->AddToggle("Draw Cached Source Data", &m_renderTempCacheLinks);
		pVfxBank->AddButton("Load Temp Node Data", datCallback(MFA(CDistantLights2::LoadTempData), (datBase*)this));
		pVfxBank->AddButton("Load Actual Data", datCallback(MFA(CDistantLights2::LoadData), (datBase*)this));

		pVfxBank->AddSeparator("Node Generation");

		pVfxBank->PushGroup("Debug Group Generation");
		pVfxBank->AddToggle("Generate Debug Group", &m_generateDebugGroup);
		pVfxBank->AddSlider("Debug Group Start X", &m_debugGroupXStart, (s32)WORLDLIMITS_REP_XMIN, (s32)WORLDLIMITS_REP_XMAX, 1);
		pVfxBank->AddSlider("Debug Group Start Y", &m_debugGroupYStart, (s32)WORLDLIMITS_REP_YMIN, (s32)WORLDLIMITS_REP_YMAX, 1);
		pVfxBank->AddSlider("Debug Group Start Z", &m_debugGroupZStart, 0, 1000, 1);
		pVfxBank->AddSlider("Debug Group Length", &m_debugGroupLength, 10, (s32)WORLDLIMITS_REP_YMAX, 1);
		pVfxBank->AddSlider("Debug Group Link Length", &m_debugGroupLinkLength, 1, 100, 1);
		pVfxBank->PopGroup();

		pVfxBank->AddSlider("Max Group Generation", &m_maxGroupGeneration, 0, DISTANTLIGHTS2_MAX_GROUPS, 1);
		pVfxBank->AddSlider("Build - Node Range", &ms_NodeGenBuildNodeRage, 0.0f, 500.0f, 5.0f);
		pVfxBank->AddSlider("Build - Node Angle", &ms_NodeGenMaxAngle, 0.0f, 1.0f, 0.0001f);
		pVfxBank->AddSlider("Build - Min Length", &ms_NodeGenMinGroupLength, 0.0f, 3000.0f, 0.1f);
		pVfxBank->AddButton("Generate New Node Data", datCallback(MFA(CDistantLights2::GenerateData), (datBase*)this));
		pVfxBank->AddSlider("Time (ms) Generate Per Frame", &m_msToGeneratePerFrame, 0.1f, 1000.0f, 0.1f);
		pVfxBank->AddText("Build Status", m_statusText, STATUS_TEXT_MAX_LENGTH, true);
		pVfxBank->PopGroup();

#if ENABLE_DISTANT_CARS
		pVfxBank->PushGroup("Deterministic Data");
		for(u32 i = 0; i < MAX_COLORS; ++i)
		{
			char buff[15];
			formatf(buff, "Car Color %d", i);
			pVfxBank->AddColor(buff, &sm_CarColors[i]);
		}
		pVfxBank->PopGroup();
		pVfxBank->AddSlider("Distant Cars/Lights Fade-Out (White Lights) Distance", &ms_whiteLightFadeDistance, 0, 1000.0f, 1.0f);
		pVfxBank->AddSlider("Distant Cars/Lights Fade-In (Red Lights) Distance", &ms_redLightFadeDistance, 0, 1000.0f, 1.0f);
		pVfxBank->AddSlider("Distant Cars Fade Zone", &ms_distantCarsFadeZone, 0.0f, 20.0f, 1.0f);
		pVfxBank->AddToggle("Debug Distant Cars", &ms_debugDistantCars);
		pVfxBank->AddToggle("Disable Distant Cars", &m_disableDistantCars);
		pVfxBank->AddToggle("Enable Distant Cars Inner Fade", &g_EnableDistantCarInnerFade);
#endif // ENABLE_DISTANT_CARS

		pVfxBank->AddTitle("INFO");
		pVfxBank->AddText("Num Blocks Rendered", &m_numBlocksRendered, true);
		pVfxBank->AddText("Num Blocks Rendered in Water Reflection", &m_numBlocksWaterReflectionRendered, true);
		pVfxBank->AddText("Num Blocks Rendered in Mirror Reflection", &m_numBlocksMirrorReflectionRendered, true);

		pVfxBank->AddText("Num Groups Rendered", &m_numGroupsRendered, true);
		pVfxBank->AddText("Num Groups Rendered in Water Reflection", &m_numGroupsWaterReflectionRendered, true);
		pVfxBank->AddText("Num Groups Rendered in Mirror Reflection", &m_numGroupsMirrorReflectionRendered, true);
		pVfxBank->AddText("Num Lights Rendered", &m_numLightsRendered, true);

		pVfxBank->AddText("Num White Lights", &m_NumRenderedWhiteLights, true);
		pVfxBank->AddText("Num Red Lights", &m_NumRenderedRedLights, true);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
		pVfxBank->AddSlider("Inland Height", &g_distLightsParam.inlandHeight, 0.0f, 2000.0f, 5.0f);

		pVfxBank->AddSlider("Size", &g_distLightsParam.size, 0.0f, 50.0f, 0.25f);
		pVfxBank->AddSlider("Size Reflections", &g_distLightsParam.sizeReflections, 0.0f, 50.0f, 0.25f);
		pVfxBank->AddSlider("Size Min", &g_distLightsParam.sizeMin, 0.0f, 64.0f, 0.01f);
		pVfxBank->AddSlider("Size Upscale", &g_distLightsParam.sizeUpscale, 0.1f, 10.0f, 0.001f);
		pVfxBank->AddSlider("Size Upscale Reflections", &g_distLightsParam.sizeUpscaleReflections, 0.1f, 10.0f, 0.001f);

		pVfxBank->AddSlider("Size Factor Dist Start", &g_distLightsParam.sizeDistStart, 0.0f, 7000.0f, 1.0f);
		pVfxBank->AddSlider("Size Factor Dist", &g_distLightsParam.sizeDist, 0.0f, 7000.0f, 1.0f);

		pVfxBank->AddSlider("Flicker", &g_distLightsParam.flicker, 0.0f, 1.0, 0.005f);
		pVfxBank->AddSlider("Twinkle Amount", &g_distLightsParam.twinkleAmount, 0.0f, 1.0f, 0.005f);
		pVfxBank->AddSlider("Twinkle Speed", &g_distLightsParam.twinkleSpeed, 0.0f, 10.0f, 0.01f);

#if USE_DISTANT_LIGHTS_2
		pVfxBank->AddSlider("Car Pop LOD Distance", &g_distLightsParam.carPopLodDistance, 0.0f, 1000.0f, 1.0f);
#endif

		for(u32 i = 0; i < DISTANTLIGHTS2_NUM_SPEED_GROUPS; ++i)
		{
			char buff[15];
			formatf(buff, "Speed=%d speed", i);
			pVfxBank->AddSlider(buff, &g_distLightsParam.speed[i], 0.01f, 10.0f, 0.01f);
		}

		pVfxBank->AddSlider("Random speed (SP)", &g_distLightsParam.randomizeSpeedSP, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Random spacing (SP)", &g_distLightsParam.randomizeSpacingSP, 0.0f, 1.0f, 0.01f);

		pVfxBank->AddSlider("Random speed (MP)", &g_distLightsParam.randomizeSpeedMP, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Random spacing (MP)", &g_distLightsParam.randomizeSpacingMP, 0.0f, 1.0f, 0.01f);

		pVfxBank->AddSeparator();

		pVfxBank->AddVector("Lane Scalars 1",&laneScalars1,-10.0f,10.0f,0.1f);
		pVfxBank->AddVector("Lane Scalars 2",&laneScalars2,-10.0f,10.0f,0.1f);
		pVfxBank->AddVector("Lane Scalars 3",&laneScalars3,-10.0f,10.0f,0.1f);
		pVfxBank->AddVector("Lane Scalars 4",&laneScalars4,-10.0f,10.0f,0.1f);

		pVfxBank->AddSeparator();

		pVfxBank->AddSlider("Car Light Hour Start", &g_distLightsParam.hourStart, 0, 24, 1);
		pVfxBank->AddSlider("Car Light Hour End", &g_distLightsParam.hourEnd, 0, 24, 1);
		pVfxBank->AddSlider("Car Light HDR Intensity", &g_distLightsParam.carLightHdrIntensity, 0.0f, 255.0f, 0.5f);

		pVfxBank->AddSlider("Car Light Far Fade", &g_distLightsParam.carLightFarFadeDist, 0.0f, 1024.0f, 0.1f);

		pVfxBank->AddSlider("Car Light Z Offset", &g_distLightsParam.carLightZOffset, 0.0f, 20.0f, 1.0f);

		pVfxBank->AddSlider("Car Light Spacing (SP)", &sm_carLight1.spacingSP, 0.1f, 200.0f, 1.0f);
		pVfxBank->AddSlider("Car Light Spacing (MP)", &sm_carLight1.spacingMP, 0.1f, 200.0f, 1.0f);
#if ENABLE_DISTANT_CARS
		pVfxBank->AddSlider("Car Light min Spacing", &g_distLightsParam.carLightMinSpacing, 0.0f, 250.0f, 1.0f);
#endif
		pVfxBank->AddSlider("Car Light Speed (SP)", &sm_carLight1.speedSP, 1, 200, 1);
		pVfxBank->AddSlider("Car Light Speed (MP)", &sm_carLight1.speedMP, 1, 200, 1);
		pVfxBank->AddColor("Car Light Color", (float*)&sm_carLight1.vColor, 3);

		pVfxBank->AddSlider("Car Light 2 Spacing (SP)", &sm_carLight2.spacingSP, 1.0f, 200.0f, 1.0f);
		pVfxBank->AddSlider("Car Light 2 Spacing (MP)", &sm_carLight2.spacingMP, 1.0f, 200.0f, 1.0f);
		pVfxBank->AddSlider("Car Light 2 Speed (SP)", &sm_carLight2.speedSP, 1, 200, 1);
		pVfxBank->AddSlider("Car Light 2 Speed (MP)", &sm_carLight2.speedMP, 1, 200, 1);
		pVfxBank->AddColor("Car Light 2 Color", (float*)&sm_carLight2.vColor, 3);

#if USE_DISTANT_LIGHTS_2
		pVfxBank->AddSeparator();
		pVfxBank->AddSlider("Car Light Fade start", &g_distLightsParam.carLightFadeInStart, 0.0f, 1500.0f, 10.0f);
		pVfxBank->AddSlider("Car Light Fade range", &g_distLightsParam.carLightFadeInRange, 0.0f, 1500.0f, 10.0f);
#endif
		

		pVfxBank->AddSeparator();

		pVfxBank->AddSlider("Street Light Hour Start", &g_distLightsParam.streetLightHourStart, 0, 24, 1);
		pVfxBank->AddSlider("Street Light Hour End", &g_distLightsParam.streetLightHourEnd, 0, 24, 1);

		pVfxBank->AddSlider("Street Light Z Offset", &g_distLightsParam.streetLightZOffset, 0.0f, 20.0f, 1.0f);
		pVfxBank->AddSlider("Street Light Spacing", &sm_streetLight.spacingSP, 1.0f, 200.0f, 1.0f);
		pVfxBank->AddColor("Street Light Color", (float*)&sm_streetLight.vColor, 3);
		pVfxBank->AddSlider("Street Light HDR Intensity", &g_distLightsParam.streetLightHdrIntensity, 0.0f, 255.0f, 0.5f);

#if ENABLE_DISTANT_CARS
		pVfxBank->AddSeparator();

		pVfxBank->AddSlider("Distant Car Z Offset", &g_distLightsParam.distantCarZOffset, 0.0f, 10.0f, 0.1f);
		pVfxBank->AddSlider("Distant Car Light Offset Red", &g_distLightsParam.distantCarLightOffsetRed, -10.0f, 10.0f, 0.1f);
		pVfxBank->AddSlider("Distant Car Light Offset White", &g_distLightsParam.distantCarLightOffsetWhite, -10.0f, 10.0f, 0.1f);
		pVfxBank->AddSlider("Distant Cars Limit", &m_softLimitDistantCars, 0, DISTANTCARS_MAX, 100);
#endif

		pVfxBank->AddSeparator();
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddSeparator();

		pVfxBank->AddToggle("Display All Groups On Vector Map", &m_displayVectorMapAllGroups);
		pVfxBank->AddToggle("Display Active Groups (on VM)", &m_displayVectorMapActiveGroups);
		pVfxBank->AddToggle("Display Active Blocks (on VM)", &m_displayVectorMapBlocks);
		pVfxBank->AddToggle("Display Active Cars (on VM)", &m_displayVectorMapActiveCars);
#if ENABLE_DISTANT_CARS
		pVfxBank->AddToggle("Display Fading Cars (on VM)", &m_displayVectorMapFadingCars);
		pVfxBank->AddToggle("Display Hidden Cars (on VM)", &m_displayVectorMapHiddenCars);
#endif  // ENABLE_DISTANT_CARS
		pVfxBank->AddToggle("Color Groups By Density", &g_distantLightsDebugColorByDensity);
		pVfxBank->AddToggle("Color Groups By Speed", &g_distantLightsDebugColorBySpeed);
		pVfxBank->AddToggle("Color Groups By Width", &g_distantLightsDebugColorByWidth);

		pVfxBank->AddToggle("Render Active Groups", &m_renderActiveGroups);

		pVfxBank->AddSlider("Override Coastal Alpha", &m_overrideCoastalAlpha, -0.01f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Override Inland Alpha", &m_overrideInlandAlpha, -0.01f, 1.0f, 0.01f);

		pVfxBank->AddSlider("Density Filter", &g_distantLightsDensityFilter, -1, 16, 1);
		pVfxBank->AddSlider("Speed Filter", &g_distantLightsSpeedFilter, -1, 16, 1);
		pVfxBank->AddSlider("Width Filter", &g_distantLightsWidthFilter, -1, 16, 1);
	}
	pVfxBank->PopGroup();
}

void CDistantLights2::VectorMapDrawActiveGroups( const DistantLightGroup2_t* pCurrGroup )
{
	s32 startOffset = pCurrGroup->pointOffset;
	s32 endOffset   = startOffset+pCurrGroup->pointCount;

	s32 r,g,b;

	if (g_distantLightsDebugColorByDensity || g_distantLightsDebugColorBySpeed || g_distantLightsDebugColorByWidth)
	{
		r = g_distantLightsDebugColorByDensity ? pCurrGroup->density * 16 : 255;
		g = g_distantLightsDebugColorBySpeed ? pCurrGroup->speed * 64 : 255;
		b = g_distantLightsDebugColorByWidth ? pCurrGroup->numLanes * 64 : 255;
	}
	else
	{
		r = (m_points[startOffset].x * m_points[endOffset].y)%255;
		g = (m_points[startOffset].y * m_points[endOffset].x)%255;
		b = (m_points[startOffset].x * m_points[startOffset].y * m_points[endOffset].x * m_points[endOffset].y)%255;
	}

	Color32 col(r, g, b, 255);

	for (s32 j=startOffset; j<startOffset+pCurrGroup->pointCount-1; j++)
	{
		if (m_renderActiveGroups)
		{
			Vec3V vPtA((float)m_points[j].x, (float)m_points[j].y, (float)m_points[j].z); 
			Vec3V vPtB((float)m_points[j+1].x, (float)m_points[j+1].y, (float)m_points[j+1].z); 
			CVectorMap::DrawLine(RCC_VECTOR3(vPtA), RCC_VECTOR3(vPtB), col, col);
		}

		if (m_displayVectorMapActiveGroups)
		{
			Vec3V vPtA((float)m_points[j].x, (float)m_points[j].y, 0.0f); 
			Vec3V vPtB((float)m_points[j+1].x, (float)m_points[j+1].y, 0.0f); 
			CVectorMap::DrawLine(RCC_VECTOR3(vPtA), RCC_VECTOR3(vPtB), col, col);
		}
	}
}


void CDistantLights2::DebugDrawDistantLightsNodes( int numLineVerts, const Vec3V* lineVerts )
{
	for ( int l = 0; l < numLineVerts-1; ++l )
	{
		if ( l == 0 )
		{
			grcDebugDraw::Line(lineVerts[0], lineVerts[1], Color32(255,255,0),Color32(0,0,255));
		}
		else if ( l == numLineVerts-2 )
		{
			grcDebugDraw::Line(lineVerts[l], lineVerts[l+1], Color32(0,255,0), Color32(255,0,255));
		}
		else
		{
			grcDebugDraw::Line(lineVerts[l], lineVerts[l+1], Color32(0, 255, 0), Color32(0, 0, 255));
		}
	}
}


void CDistantLights2::DebugDrawDistantLightSpheres( Vec3V finalPos0, Vec3V finalPos1, Vec3V finalPos2, Vec3V finalPos3 )
{
	grcDebugDraw::Sphere(finalPos0, 1.0f, Color32(255, 0, 0));
	grcDebugDraw::Sphere(finalPos1, 1.0f, Color32(255, 0, 0));
	grcDebugDraw::Sphere(finalPos2, 1.0f, Color32(255, 0, 0));
	grcDebugDraw::Sphere(finalPos3, 1.0f, Color32(255, 0, 0));
}

void CDistantLights2::VectorMapDisplayActiveFadingAndHiddenCars(const Vec3V& worldPos, const Vector3& camPos, float distCarEndFadeDist, const Color32& VISIBLE_COLOR, const float distCarStartFadeDist, const Vec3V& direction, const Vec3V& lightOffsets)
{
#if ENABLE_DISTANT_CARS
	const static Color32 FADING_COLOR(0, 61, 166);
	const static Color32 HIDDEN_COLOR(150, 0, 120);
	const Vector3 carPos = VEC3V_TO_VECTOR3(worldPos);
	const float dist = camPos.Dist2(carPos);

	bool showCar;
	Color32 color;
	if(dist >= distCarEndFadeDist)
	{
		color   = VISIBLE_COLOR;
		showCar = m_displayVectorMapActiveCars;
	}
	else if (dist >= distCarStartFadeDist)
	{
		color   = FADING_COLOR;
		showCar = m_displayVectorMapFadingCars;
	}
	else
	{
		color   = HIDDEN_COLOR;
		showCar = m_displayVectorMapHiddenCars;
	}

	if(showCar)
	{
		Vector3 perpDir = VEC3V_TO_VECTOR3(direction);
		perpDir.Cross(Vector3(0.0f, 0.0f, 1.0f));
		perpDir.Normalize();

		const Vector3 offset = VEC3V_TO_VECTOR3(lightOffsets) * CVectorMap::m_vehicleVectorMapScale;
		const Vector3 side  = perpDir * VEC3V_TO_VECTOR3(sm_CarWidth) * CVectorMap::m_vehicleVectorMapScale;
		const Vector3 front = carPos + offset;
		const Vector3 back  = carPos - offset;

		CVectorMap::DrawLine(front + side, back + side, color, color);
		CVectorMap::DrawLine(front - side, back - side, color, color);
		CVectorMap::DrawLine(front + side, front - side, color, color);
		CVectorMap::DrawLine(back + side, back - side, color, color);
	}
#else
	const static float CAR_SIZE = 1.5f;
	CVectorMap::DrawCircle(VEC3V_TO_VECTOR3(worldPos), CAR_SIZE, VISIBLE_COLOR);
#endif // ENABLE_DISTANT_CARS
}

#endif

void CDistantLights2::CalcLightColour(const Vec3V& lightPos, const Vector3& camPos, const float lodDistance2, const ScalarV& noAlpha, const ScalarV& fullAlpha, const ScalarV& color, Color32& lightColors)
{
	BoolV furtherThanLodDistance = BoolV(VEC3V_TO_VECTOR3(lightPos).Dist2(camPos) > lodDistance2);

	// Calculate the alpha fade of the light based on the lod distance (if we are further away than lod distance) or fade distance.
	ScalarV lightAlpha = SelectFT(furtherThanLodDistance, noAlpha, fullAlpha);

	//distant cars alpha set to 1.0 when slod mode is active, do the same for the lights, ignoring the loddistance.
	if( g_LodMgr.IsSlodModeActive() )
	{
		lightAlpha = fullAlpha;
	}

	lightColors.SetFromDeviceColor(color.Geti());

	lightColors.SetAlpha(lightAlpha.Geti());
}

void CDistantLights2::SetupLightProperties( const float coastalAlpha, DistantLightProperties &coastalLightProperties, const float inlandAlpha, DistantLightProperties &inlandLightProperties )
{
	ScalarV alphaV(coastalAlpha);
	Color32 carLight1Color, carLight2Color, streetlightColor;
	carLight1Color = Color32(sm_carLight1.vColor * alphaV);
	carLight2Color = Color32(sm_carLight2.vColor * alphaV);
	streetlightColor = Color32(sm_streetLight.vColor * alphaV);
	coastalLightProperties.scCarLight1Color = ScalarVFromU32(carLight1Color.GetDeviceColor());
	coastalLightProperties.scCarLight2Color = ScalarVFromU32(carLight2Color.GetDeviceColor());
	coastalLightProperties.scStreetLightColor = ScalarVFromU32(streetlightColor.GetDeviceColor());

	alphaV = alphaV * ScalarVFromF32(inlandAlpha);
	carLight1Color = Color32(sm_carLight1.vColor * alphaV);
	carLight2Color = Color32(sm_carLight2.vColor * alphaV);
	streetlightColor = Color32(sm_streetLight.vColor * alphaV);
	inlandLightProperties.scCarLight1Color = ScalarVFromU32(carLight1Color.GetDeviceColor());
	inlandLightProperties.scCarLight2Color = ScalarVFromU32(carLight2Color.GetDeviceColor());
	inlandLightProperties.scStreetLightColor = ScalarVFromU32(streetlightColor.GetDeviceColor());
}

void CDistantLights2::PermuteLaneScalars( const ScalarV& carLight1QuadTForward, Vec4V& permutedLaneScalarsForward, const Vec4V &laneScalars )
{
	BoolV oneOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_ONE));
	BoolV twoOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_TWO));
	BoolV threeOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_THREE));

	permutedLaneScalarsForward = SelectFT(twoOrMore, 
		SelectFT(oneOrMore, 
		laneScalars,		// 0 - 0.999
		laneScalars.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>()), // 1.0 - 1.999
		SelectFT(threeOrMore, 
		laneScalars.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>(), // 2.0 - 2.999
		laneScalars.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>())); // 3.0 - 3.999
}

void CDistantLights2::CalculateLayout( const DistantLightGroup2_t* pCurrGroup, mthRandom &perGroupRng, const Vec2V& randomSpeedMin, const Vec2V& randomSpeedMax, const ScalarV& carLight1Speed, const Vec2V& spacingFromAi, const ScalarV& carLight1Spacing, const ScalarV& elapsedTime, const Vec4V& laneScalars, const Vec3V& vCameraPos,
									  const DistantLightProperties& currentGroupProperties, CalculateLayout_Params& params)
{
	// Map speed 0->3 to the speed we'll use. This also negates the 'y' speed
	vfxFatalAssertf(pCurrGroup->speed < DISTANTLIGHTS2_NUM_SPEED_GROUPS, "Speed exeeded array, increase DISTANTLIGHTS2_NUM_SPEED_GROUPS and add new speeds!");
	const Vec2V speedScale = Vec2V(g_distLightsParam.speed[pCurrGroup->speed], -g_distLightsParam.speed[pCurrGroup->speed]) *  perGroupRng.GetRangedV(randomSpeedMin, randomSpeedMax);

	params.carLight1ScaledSpeed = speedScale * carLight1Speed;
	params.carLight1ScaledSpacing = spacingFromAi * carLight1Spacing;

	const Vec2V distanceOffset = Vec2VFromF32(pCurrGroup->distanceOffset);
	params.blockTime = Vec2V(elapsedTime) + sm_InitialTimeOffset - (distanceOffset / params.carLight1ScaledSpeed);
	const Vec2V carLight1ElapsedDistance = params.blockTime * params.carLight1ScaledSpeed;

	// This part is a little complex, so some description is in order.
	// We're using an offset vector to offset the some cars to the side, to simulate multiple lanes. The offset vector will
	// repeat every fourth car. So if the offsets were 0, 1.5, 0.2, 0.5, the 0th, 4th, 8th, etc car would have an offset of 0.
	// The 1st, 5th, 9th, etc. car would have an offset of 1.5
	// Lets call this offset the "horizontal" offset.
	// The way the cars move forward is by computing a "vertical" offset along the line. The "scaled spacing" gives the distance
	// from one car to the next. Based on the speed of the cars we compute a 't' value that goes from 0-1 - this gives us the 
	// vertical offset to start drawing the first car. The problem is when t hits 1, and starts over again at 0, what was formerly the 0th
	// car along the line now appears to be the 1st car, and thus gets a different horizontal offset. To compensate for this we also rotate
	// the horizontal offset vector by one component each time t goes from 0 to 1. 

	// How many sets of 4 cars have driven by so far
	const Vec2V carLight1ElapsedQuadSpacingUnits = carLight1ElapsedDistance / (params.carLight1ScaledSpacing * ScalarV(V_FOUR));

	// X component has T value from 0 -> 3.9999 for how far into the current quad we are
	// Y component has T value from -3.999 -> 0
	params.carLight1QuadT = (carLight1ElapsedQuadSpacingUnits - RoundToNearestIntZero(carLight1ElapsedQuadSpacingUnits)) * ScalarV(V_FOUR);

	const ScalarV carLight1QuadTForward = params.carLight1QuadT.GetX();

	PermuteLaneScalars(carLight1QuadTForward, params.permutedLaneScalarsForward, laneScalars);


	const ScalarV carLight1QuadTBackward = params.carLight1QuadT.GetY() + ScalarV(V_FOUR); // adjust to 0..3.999

	const BoolV oneOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_ONE));
	const BoolV twoOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_TWO));
	const BoolV threeOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_THREE));

	params.permutedLaneScalarsBackward = SelectFT(twoOrMoreB, 
		SelectFT(oneOrMoreB, 
		laneScalars,	// 0 - 0.999
		laneScalars.Get<Vec::W, Vec::X, Vec::Y, Vec::X>()), // 1.0 - 1.999
		SelectFT(threeOrMoreB, 
		laneScalars.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>(), // 2.0 - 2.999
		laneScalars.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>())); // 3.0 - 3.999

	// How many cars have driven by:
	const Vec2V carLight1ElapsedSpacingUnits = carLight1ElapsedQuadSpacingUnits * ScalarV(V_FOUR);

	// T value from 0 -> 0.9999 to start drawing at (y comp is -0.999 -> 0)
	Vec2V carLight1InitialT = (carLight1ElapsedSpacingUnits - RoundToNearestIntZero(carLight1ElapsedSpacingUnits));
	carLight1InitialT.SetY(carLight1InitialT.GetY() + ScalarV(V_ONE));

	params.carLight1InitialOffset = carLight1InitialT * params.carLight1ScaledSpacing;

	// Given the first and last verts of the group, we find the "general direction" of the group and use this to 
	// 1) get a perpendicular vector so we can find a horizontal offset for multiple lanes and
	// 2) tell if the cars on the road are headed toward or away from the camera. Those going toward the camera should look like
	//		headlights, those going away should look like taillights
	const DistantLightPoint_t& firstPoint = m_points[pCurrGroup->pointOffset];
	const DistantLightPoint_t& lastPoint  = m_points[pCurrGroup->pointOffset + pCurrGroup->pointCount - 1];
	Vec3V firstVert = Vec3V((float)firstPoint.x, (float)firstPoint.y, (float)firstPoint.z);
	Vec3V lastVert =  Vec3V((float)lastPoint.x, (float)lastPoint.y, (float)lastPoint.z);

	const Vec3V generalDirection = Normalize(firstVert - lastVert);

	params.perpDirection = Vec3V(-generalDirection.GetY(), generalDirection.GetX(), ScalarV(V_ZERO));
	params.perpDirection = Scale(params.perpDirection, ScalarV(V_THREE)); // hard coded 3m per lane

	const Vec3V camToLineDir = Average(firstVert, lastVert) - vCameraPos;
	const ScalarV camDirDotLine = Dot(camToLineDir.GetXY(), generalDirection.GetXY());

	// Select red or white lights based on the dot product (todo: lerp in some range so there is no pop?)
	params.carDirectionChoice = IsLessThan(camDirDotLine, ScalarV(V_ZERO));


	const int baseVertexIdx = pCurrGroup->pointOffset;
	params.numLineVerts  = pCurrGroup->pointCount;
	FastAssert(params.numLineVerts < DISTANTLIGHTS2_MAX_LINE_DATA && params.numLineVerts >= 0);

	params.lineLength = ScalarV(V_ZERO);
	for(int i = 0; i < params.numLineVerts; i++)
	{
		params.lineVerts[i] = Vec3V(float(m_points[baseVertexIdx+i].x), float(m_points[baseVertexIdx+i].y), float(m_points[baseVertexIdx+i].z));
	}

	for(int i = 0; i < params.numLineVerts-1; i++)
	{
		Vec3V diff = params.lineVerts[i+1] - params.lineVerts[i];
		ScalarV segLength = Mag(diff);
		params.lineDiffs[i] = Vec4V(Normalize(diff), segLength);
		params.lineLength += segLength;
	}

	params.oneWayColor = SelectFT(params.carDirectionChoice, currentGroupProperties.scCarLight1Color, currentGroupProperties.scCarLight2Color);
	params.otherWayColor = SelectFT(params.carDirectionChoice, currentGroupProperties.scCarLight2Color, currentGroupProperties.scCarLight1Color);
}

bool CDistantLights2::DistantLights2RunFromDependency(const sysDependency&)
{
	if (!g_hack_oldDistantCarsSync)
	{
		g_distantLights.PrepareDistantCarsRenderData();
	}

	return true;
}

int CDistantLights2::PrepareDistantCarsRenderData()
{
	// set the coastal light alpha - default to off
	float coastalAlpha = Lights::CalculateTimeFade(
		g_distLightsParam.hourStart - 1.0f,
		g_distLightsParam.hourStart,
		g_distLightsParam.hourEnd - 1.0f,
		g_distLightsParam.hourEnd);

	// if there are no coastal lights on then return
#if ENABLE_DISTANT_CARS
	bool distantCarsUpdate = false;
	if(coastalAlpha <= 0.0f)
	{
#if __BANK
		if (m_disableDistantCars)
			return 0;
		else
#endif //__BANK
		{
			distantCarsUpdate = true;
		}

	}
#endif //ENABLE_DISTANT_CARS

	PF_PUSH_TIMEBAR("CDistantLights2 Calculate render data");

#if __BANK
	if (m_overrideCoastalAlpha>=0.0f)
	{
		coastalAlpha = m_overrideCoastalAlpha;
	}
#endif

	// calc the inland light alpha based on the height of the camera (helis need to see inland lights)
	const Vec3V vCamPos = gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition();
	float inlandAlpha = Min(1.0f, ((vCamPos.GetZf()-g_distLightsParam.inlandHeight) / 40.0f));

#if __BANK
	if (m_overrideInlandAlpha>=0.0f)
	{
		inlandAlpha = m_overrideInlandAlpha;
	}
#endif
	const int bufferID = gRenderThreadInterface.GetCurrentBuffer();
	const s32 numVisibleGroups = g_DistantLightsCount[bufferID];
	DistantLightsVisibilityOutput* pVisibleGroups = &(g_DistantLightsVisibility[bufferID][0]);

	DistantLightProperties coastalLightProperties, inlandLightProperties;
	SetupLightProperties(coastalAlpha, coastalLightProperties, inlandAlpha, inlandLightProperties);


	const ScalarV elapsedTime = IntToFloatRaw<0>(ScalarVFromU32(fwTimer::GetTimeInMilliseconds())) * ScalarVConstant<FLOAT_TO_INT(0.001f)>(); // convert to seconds

	ScalarV carLight1Speed;
	ScalarV carLight1Spacing;

	float randomizeSpeed;
	float randomizeSpacing; 

	if( NetworkInterface::IsGameInProgress() )
	{
		carLight1Speed = ScalarV(sm_carLight1.speedMP);
		carLight1Spacing = ScalarV(sm_carLight1.spacingMP);

		randomizeSpeed = g_distLightsParam.randomizeSpeedMP;
		randomizeSpacing = g_distLightsParam.randomizeSpacingMP; 
	}
	else
	{
		carLight1Speed = ScalarV(sm_carLight1.speedSP);
		carLight1Spacing = ScalarV(sm_carLight1.spacingSP);

		randomizeSpeed = g_distLightsParam.randomizeSpeedSP;
		randomizeSpacing = g_distLightsParam.randomizeSpacingSP; 
	}

	// The code below uses Vec2s because we're computing two speeds / positions, etc.
	// One for each direction along the road, because a lot of roads are two way
	// The 'x' component is for forward-going lanes (start to end), the 'y' is for backward (end to start)

	Vec2V randomSpacingMin = Vec2VConstant<FLOAT_TO_INT(0.4f), FLOAT_TO_INT(0.4f)>(); // Magic number to keep the spacing roughly the same after changing which density table we use
	Vec2V randomSpacingMax = randomSpacingMin;

	Vec2V randomSpeedMin = Vec2VFromF32(1.0f - randomizeSpeed);
	Vec2V randomSpeedMax = Vec2VFromF32(1.0f + randomizeSpeed);
	randomSpacingMin *= Vec2VFromF32(1.0f - randomizeSpacing); 
	randomSpacingMax *= Vec2VFromF32(1.0f + randomizeSpacing);

	m_distantCarRenderData[1-m_distantDataRenderBuffer].clear();
	m_distantLightsRenderData[1-m_distantDataRenderBuffer].clear();

#if RSG_BANK
	int numLights = 0;
#endif // RSG_BANK
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	// Vehicle density can be driven by script and validly set to 0. If this happens, early-out gracefully.
	if (m_RandomVehicleDensityMult != 0 && m_disableVehicleLights == false)
#endif // __WIN32PC || RSG_DURANGO || RSG_ORBIS
	{
		//Take output from processVisibility and cycle through visible groups
		for(s32 index = 0; index < numVisibleGroups; index++)
		{
#if ENABLE_DISTANT_CARS
			if (distantCarsUpdate && m_distantCarRenderData[1-m_distantDataRenderBuffer].size() >= m_softLimitDistantCars)
				break;
#endif // ENABLE_DISTANT_CARS
			DistantLightsVisibilityOutput& currentVisibleGroup = pVisibleGroups[index];

			DISTANTLIGHTS_PF_START(CalcLanes);
			const DistantLightGroup2_t* pCurrGroup = &m_groups[currentVisibleGroup.index];
			const DistantLightProperties &currentGroupProperties = currentVisibleGroup.lightType == 1? inlandLightProperties: coastalLightProperties; 


			Vec4V laneScalars;

			int numLanes = pCurrGroup->numLanes;
			Vec2V numLanesV(V_ONE);
			if (pCurrGroup->twoWay)
			{
				numLanes = (numLanes + 1)/2; // halve the number of lanes, round up
			}

			switch(numLanes)
			{
			case 1:	laneScalars = laneScalars1; numLanesV = Vec2V(V_ONE); break;
			case 2: laneScalars = laneScalars2; numLanesV = Vec2V(V_TWO); break;
			case 3: laneScalars = laneScalars3; numLanesV = Vec2V(V_THREE); break;
			default: laneScalars = laneScalars4; numLanesV = IntToFloatRaw<0>(Vec2VFromS32(numLanes)); break;
			}

#if __BANK
			if (g_distantLightsDensityFilter >= 0 && (u32)g_distantLightsDensityFilter != pCurrGroup->density)
			{
				continue;
			}

			if (g_distantLightsSpeedFilter >= 0 && (u32)g_distantLightsSpeedFilter != pCurrGroup->speed)
			{
				continue;
			}

			if (g_distantLightsWidthFilter >= 0 && (u32)g_distantLightsWidthFilter != pCurrGroup->numLanes)
			{
				continue;
			}
#endif
			DISTANTLIGHTS_PF_STOP(CalcLanes);
			// Address of the group makes a good random seed
			mthRandom perGroupRng(pCurrGroup->randomSeed);

			Vec2V spacingFromAi = Vec2VFromF32(CVehiclePopulation::ms_fDefaultVehiclesSpacing[pCurrGroup->density]);
			spacingFromAi = spacingFromAi / (numLanesV * perGroupRng.GetRangedV(randomSpacingMin, randomSpacingMax));

			DISTANTLIGHTS_PF_START(CalcLayout);

			static Vec3V lineVerts[DISTANTLIGHTS2_MAX_LINE_DATA];
			static Vec4V lineDiffs[DISTANTLIGHTS2_MAX_LINE_DATA]; // (x,y,z) direction, w length
			CalculateLayout_Params params;
			params.lineVerts = lineVerts;
			params.lineDiffs = lineDiffs;

			CalculateLayout(pCurrGroup, perGroupRng, randomSpeedMin, randomSpeedMax, carLight1Speed, spacingFromAi, carLight1Spacing, elapsedTime, laneScalars, vCamPos, currentGroupProperties, params);

			DISTANTLIGHTS_PF_STOP(CalcLayout);

			if (pCurrGroup->twoWay)
			{
				// Select red or white lights based on the dot product (todo: lerp in some range so there is no pop?)

				// car lights 1
				BANK_ONLY(numLights +=)
					PlaceLightsAlongGroup( params.carLight1InitialOffset.GetX(),
					params.carLight1ScaledSpacing.GetX(),
					params.oneWayColor,
					params.blockTime.GetX(),
					params.carLight1ScaledSpeed.GetX(),
					Vec4V(V_TWO) + params.permutedLaneScalarsForward,
					params.perpDirection,
					currentVisibleGroup.index,
					false,
					params.carDirectionChoice.Getb(),
					lineVerts,
					lineDiffs,
					params.numLineVerts,
					params.lineLength);

				// car lights 2
				BANK_ONLY(numLights +=)
					PlaceLightsAlongGroup( params.carLight1InitialOffset.GetY(),
					params.carLight1ScaledSpacing.GetY(),
					params.otherWayColor,
					params.blockTime.GetY(),
					params.carLight1ScaledSpeed.GetY(),
					Vec4V(V_NEGTWO) - params.permutedLaneScalarsBackward,
					params.perpDirection,
					currentVisibleGroup.index,
					true,
					!params.carDirectionChoice.Getb(),
					lineVerts,
					lineDiffs,
					params.numLineVerts,
					params.lineLength);
			}
			else
			{
				// car lights 1
				BANK_ONLY(numLights +=)
					PlaceLightsAlongGroup( params.carLight1InitialOffset.GetX(),
					params.carLight1ScaledSpacing.GetX(),
					params.oneWayColor,
					params.blockTime.GetX(),
					params.carLight1ScaledSpeed.GetX(),
					params.permutedLaneScalarsForward,
					params.perpDirection,
					currentVisibleGroup.index,
					false,
					params.carDirectionChoice.Getb(),
					lineVerts,
					lineDiffs,
					params.numLineVerts,
					params.lineLength);
			}


			// Note this does NOT put traffic going two directions down a single street. If we want that feature back, calculate a 1-t value for opposing traffic
			// and call PlaceLightsAlongGroup again

			// debug - render active groups to the vector map
#if __BANK
			if (m_displayVectorMapActiveGroups || m_renderActiveGroups)
			{
				VectorMapDrawActiveGroups(pCurrGroup);

			}
#endif // __BANK
		}
	}

	m_distantDataRenderBuffer = 1-m_distantDataRenderBuffer;

	PF_POP_TIMEBAR();

#if RSG_BANK
	return numLights;
#else
	return 0;
#endif // RSG_BANK
}

#endif // USE_DISTANT_LIGHTS_2 || RSG_DEV
