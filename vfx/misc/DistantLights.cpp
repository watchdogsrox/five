///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DistantLights.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	05 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////
//
//	Deals with streetlights and headlights being rendered miles into the
//	distance. The idea is to identify roads that have these lights on them
//	during the patch generation phase. These lines then get stored with the
//	world blocks and sprites are rendered along them whilst rendering
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "DistantLights.h"
#include "DistantLightsVertexBuffer.h"

// The node generation tool needs this object in DEV builds no matter which light set we are using!
#if !USE_DISTANT_LIGHTS_2 || RSG_DEV

// Rage header
#if ENABLE_DISTANT_CARS
#include "atl/array.h"
#include "data/struct.h"
#include "grcore/instancebuffer.h"
#endif //ENABLE_DISTANT_CARS
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"
#include "system/endian.h"
#include "system/param.h"

// Framework headers
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
#include "fwscene/scan/scan.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/world/WorldLimits.h"
#include "vfx/vfxwidget.h"

// Game header
#include "Camera/CamInterface.h"
#include "Camera/viewports/ViewportManager.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Core/Game.h"
#include "Debug/Rendering/DebugLighting.h"
#include "Debug/VectorMap.h"
#include "Game/Clock.h"
#if ENABLE_DISTANT_CARS
#include "game/ModelIndices.h"
#include "Peds/PlayerInfo.h"
#include "Peds/ped.h"
#endif //ENABLE_DISTANT_CARS
#include "Renderer/Lights/LODLights.h"
#include "Renderer/Mirrors.h"
#include "Renderer/Renderer.h"
#include "Renderer/Sprite2d.h"
#include "Renderer/Occlusion.h"
#include "Renderer/Water.h"
#include "Renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "Renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "Scene/DataFileMgr.h"
#include "Scene/lod/LodScale.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderLib.h"
#include "System/FileMgr.h"
#include "System/System.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "Vfx/VfxHelper.h"
#include "Vehicles/vehiclepopulation.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////
VFX_MISC_OPTIMISATIONS()
AI_OPTIMISATIONS()

XPARAM(buildpaths);
XPARAM(nolodlights);
#if ENABLE_DISTANT_CARS
PARAM(nodistantcars, "disable distant cars");
PARAM(nodistantlights, "disable distant lights");
#endif

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////
extern Vec4V laneScalars1;
extern Vec4V laneScalars2;
extern Vec4V laneScalars3;
extern Vec4V laneScalars4;


#if !__PS3
#define __builtin_expect(exp, hint) exp
#endif 

#if __BANK
float	CDistantLights::ms_DisnantLightsBuildNodeRage = 50.0f;
#endif

distLight CDistantLights::sm_carLight1 = {Vec3V(V_ONE),33.0f, 43.0f};
distLight CDistantLights::sm_carLight2 = {Vec3V(V_X_AXIS_WZERO),45.5f, 57.0f};
distLight CDistantLights::sm_streetLight = {Vec3V(1.0f,0.86f,0.5f),30.0f, 0.0f};

#if ENABLE_DISTANT_CARS
#if __PS3
static grcEffectVar				m_DistantCarNearClip = grcevNONE;
#endif
grcVertexDeclaration*	CDistantLights::sm_pDistantCarsVtxDecl = NULL; 
fwModelId CDistantLights::sm_DistantCarModelId;
CBaseModelInfo *CDistantLights::sm_DistantCarBaseModelInfo = NULL;

grcEffectTechnique		m_DistantCarsTechnique = grcetNONE;
grcEffectVar				m_DistantCarAlphaID = grcevNONE;

DistantCarInstanceData* sm_pCurrInstance = NULL;
bool m_CreatedVertexBuffers = false;


#endif //ENABLE_DISTANT_CARS


#if __PPU
DECLARE_FRAG_INTERFACE(DistantLightsVisibility);
#endif

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CDistantLights::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		m_numPoints = 0;
		m_numGroups = 0;

#if __BANK
		m_disableStreetLightsVisibility = false;
		m_disableDepthTest = false;
		m_bDisableRendering = false;

		m_displayVectorMapAllGroups = false;
		m_displayVectorMapActiveGroups = false;
		m_displayVectorMapBlocks = false;

		m_renderActiveGroups = false;

		m_overrideCoastalAlpha = -0.01f;
		m_overrideInlandAlpha = -0.01f;
#endif

		m_disableVehicleLights = false;
		m_disableStreetLights = !PARAM_nolodlights.Get();

#if __PPU
		g_DistantLightsVisibilityDependency.Init( FRAG_SPU_CODE(DistantLightsVisibility), 0, 0 );
#else
		g_DistantLightsVisibilityDependency.Init( DistantLightsVisibility::RunFromDependency, 0, 0 );
#endif

#if ENABLE_DISTANT_CARS
		m_DistantCarsTechnique = grcetNONE;
		m_numDistantCars = 0;
		m_CreatedVertexBuffers = false;
		
#if !__FINAL
		m_disableDistantCars = PARAM_nodistantcars.Get();
		m_disableDistantLights = PARAM_nodistantlights.Get();
#endif
        m_disableDistantCarsByScript = false;
		m_softLimitDistantCars = DISTANTCARS_MAX;

		m_DistantCarsShader = grmShaderFactory::GetInstance().Create();
		Assert(m_DistantCarsShader);
		m_DistantCarsShader->Load("im");

		m_DistantCarsTechnique = m_DistantCarsShader->LookupTechnique("DistantCars");
		Assert(m_DistantCarsTechnique);

		m_DistantCarAlphaID = m_DistantCarsShader->LookupVar("DistantCarAlpha");
		Assert(m_DistantCarAlphaID);

#if __PS3
		m_DistantCarNearClip = m_DistantCarsShader->LookupVar("DistantCarNearClip");
		Assert(m_DistantCarNearClip);
#endif

		const grcVertexElement DistantCarsVtxElements[] =
		{
			grcVertexElement(0, grcVertexElement::grcvetPosition, 0, 12, grcFvf::grcdsFloat3),
			grcVertexElement(0, grcVertexElement::grcvetNormal,   0, 12, grcFvf::grcdsPackedNormal),
#if __D3D11 || RSG_ORBIS
			grcVertexElement(1, grcVertexElement::grcvetTexture,  1, 12, grcFvf::grcdsFloat3,  grcFvf::grcsfmIsInstanceStream, 1),
			grcVertexElement(1, grcVertexElement::grcvetTexture,  2, 12, grcFvf::grcdsFloat3,  grcFvf::grcsfmIsInstanceStream, 1),
#else
#if __XENON
			grcVertexElement(1, grcVertexElement::grcvetTexture,  1, 4,	 grcFvf::grcdsFloat),
			grcVertexElement(2, grcVertexElement::grcvetTexture,  2, 12, grcFvf::grcdsFloat3),
			grcVertexElement(2, grcVertexElement::grcvetTexture,  3, 12, grcFvf::grcdsFloat3),
#else
#if __PS3
			grcVertexElement(1, grcVertexElement::grcvetTexture,  1, 4,  grcFvf::grcdsFloat),
#else
			grcVertexElement(1, grcVertexElement::grcvetTexture,  1, 12, grcFvf::grcdsFloat3),
			grcVertexElement(1, grcVertexElement::grcvetTexture,  2, 12, grcFvf::grcdsFloat3),
#endif
#endif
#endif
		};

		sm_pDistantCarsVtxDecl = GRCDEVICE.CreateVertexDeclaration(DistantCarsVtxElements, NELEM(DistantCarsVtxElements));

		m_CreatedVertexBuffers = false;

#endif //ENABLE_DISTANT_CARS
	}
}

///////////////////////////////////////////////////////////////////////////////
//  CreateVertexBuffers
///////////////////////////////////////////////////////////////////////////////

void CDistantLights::CreateVertexBuffers()
{
#if ENABLE_DISTANT_CARS
	Assert(m_CreatedVertexBuffers == false);
	
	PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM

#if __PS3 || __XENON
	rmcDrawable *drawable = sm_DistantCarBaseModelInfo->GetDrawable();
	const rmcLodGroup& lodGroup = drawable->GetLodGroup();
	const grmModel& model = lodGroup.GetLodModel0(LOD_HIGH);
	const grmGeometry& geom = model.GetGeometry(0);
	const grcVertexBuffer* pVertexBuff = geom.GetVertexBufferByIndex(0);

	const grcIndexBuffer* pIndexBuff = geom.GetIndexBuffer(0);

	//Ensure model doesnt change, if it does need to check some bits.
	Assert(pVertexBuff->GetVertexCount() == 24);
#endif

	grcFvf fvfInstances;		
	u32 instanceVBSize = DISTANTCARS_MAX;
#if __PS3 || __XENON
#if __PS3
	u32 vbSize = DISTANTCARS_BATCH_SIZE;
#else
	u32 vbSize = 1;
#endif
	vbSize*= pVertexBuff->GetVertexCount();
	//We have to make instance vb larger on PS3, on 360 we can use vfetch to keep it smaller, not sure if there's a way on PS3?
#if __PS3		
	instanceVBSize = vbSize;
#endif
#endif

#if __XENON
	fvfInstances.SetTextureChannel(2, true, grcFvf::grcdsFloat3);
	fvfInstances.SetTextureChannel(3, true, grcFvf::grcdsFloat3);
#else
#if __PS3
	fvfInstances.SetTextureChannel(1, true, grcFvf::grcdsFloat);
#else
	fvfInstances.SetTextureChannel(1, true, grcFvf::grcdsFloat3);
	fvfInstances.SetTextureChannel(2, true, grcFvf::grcdsFloat3);
#endif
#endif
#if !__PS3
	for(u32 i=0; i < vbBufferCount; i++)
	{
#if RSG_PC && __D3D11
		m_DistantCarsInstancesVB[i] = grcVertexBuffer::Create(instanceVBSize, fvfInstances, rage::grcsBufferCreate_DynamicWrite, rage::grcsBufferSync_None, NULL);
#else
		const bool bReadWrite = true;
		const bool bDynamic = RSG_DURANGO ? false : true;
		m_DistantCarsInstancesVB[i] = grcVertexBuffer::Create(instanceVBSize, fvfInstances, bReadWrite, bDynamic, NULL);
#endif
	}

	m_DistantCarsCurVertexBuffer = 0;
#else // !__PS3
	m_DistantCarsInstancesVB = grcVertexBuffer::Create(pVertexBuff->GetVertexCount() * DISTANTCARS_BATCH_SIZE, fvfInstances, true, true, NULL);

	Verifyf(m_DistantCarsInstancesVB->Lock(rage::grcsDiscard), "Couldn't lock vertex buffer");
	float* pDst = reinterpret_cast<float*>(m_DistantCarsInstancesVB->GetLockPtr());

	for (int i = 0; i < DISTANTCARS_BATCH_SIZE; i++)
	{
		for (int vertIndex = 0; vertIndex < pVertexBuff->GetVertexCount(); vertIndex++)
		{
			*pDst = (float)i;
			pDst++;
		}
	}

	m_DistantCarsInstancesVB->UnlockWO();
#endif

#if __PS3 || __XENON
#if __PS3
	m_DistantCarsModelIB = grcIndexBuffer::Create(pIndexBuff->GetIndexCount() * DISTANTCARS_BATCH_SIZE, false);
	u16* pDstIB = m_DistantCarsModelIB->LockRW();
#endif

	u16* pSrcIB = (u16*)(pIndexBuff->LockRO());

	const grcFvf *pfvfModel = geom.GetFvf();
	const bool bReadWrite = true;
	const bool bDynamic = false;

	m_DistantCarsModelVB = grcVertexBuffer::Create(vbSize, *pfvfModel, bReadWrite, bDynamic, NULL);
	m_DistantCarsModelVB->LockRW();
	u8* pDstVB = (u8*) m_DistantCarsModelVB->GetLockPtr();
	pVertexBuff->LockRO();
	u8* pSrcVB = (u8*) pVertexBuff->GetLockPtr();

#if __XENON
	grcFvf indexdataFVF;
	indexdataFVF.SetTextureChannel(1,true,grcFvf::grcdsFloat);
	m_DistantCarsModelIndexVB = grcVertexBuffer::Create(pIndexBuff->GetIndexCount(),indexdataFVF,true,false,NULL);

	float* pDstIB = (float*)(m_DistantCarsModelIndexVB->LockRW());

	for (int j = 0; j < pIndexBuff->GetIndexCount(); j++)
	{
		*pDstIB = *pSrcIB;
		pDstIB++;
		pSrcIB++;
	}
 
	memcpy(pDstVB, pSrcVB, pVertexBuff->GetVertexCount() * pVertexBuff->GetVertexStride());
	pDstVB += pVertexBuff->GetVertexCount() * (pVertexBuff->GetVertexStride());

	m_DistantCarsModelIndexVB->UnlockRW();

#else

	for (int  i = 0; i < DISTANTCARS_BATCH_SIZE; i++)
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

#endif // __XENON

	pIndexBuff->UnlockRO();

	m_DistantCarsModelVB->UnlockRW();
	pVertexBuff->UnlockRO();

#endif // __PS3 || __XENON

	PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM

	m_CreatedVertexBuffers = true;

#endif // ENABLE_DISTANT_CARS
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateVisualDataSettings
///////////////////////////////////////////////////////////////////////////////

void CDistantLights::UpdateVisualDataSettings()
{
	// TODO -- fix the order of these, and the struct definition, to match the rag widgets
	g_distLightsParam.size						= g_visualSettings.Get("distantlights.size",						0.8f);
	g_distLightsParam.sizeReflections			= g_visualSettings.Get("distantlights.sizeReflections",				1.6f);
	g_distLightsParam.sizeMin					= g_visualSettings.Get("distantlights.sizeMin",						6.0f);
	g_distLightsParam.sizeUpscale				= g_visualSettings.Get("distantlights.sizeUpscale",					1.0f);
	g_distLightsParam.sizeUpscaleReflections	= g_visualSettings.Get("distantlights.sizeUpscaleReflections",		2.0f);
	g_distLightsParam.streetLightHdrIntensity	= g_visualSettings.Get("distantlights.streetlight.HDRIntensity",	4.0f);
	g_distLightsParam.carLightHdrIntensity		= g_visualSettings.Get("distantlights.carlight.HDRIntensity",		0.5f);
	g_distLightsParam.carLightNearFadeDist		= g_visualSettings.Get("distantlights.carlight.nearFade",			300.0f);
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

	g_distLightsParam.speed0Speed				= g_visualSettings.Get("distantlights.speed0Speed",					0.3f);
	g_distLightsParam.speed3Speed				= g_visualSettings.Get("distantlights.speed3Speed",					1.5f);

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

	sm_streetLight.spacingSP					= g_visualSettings.Get("distantlights.streetlight.Spacing",	30.0f);
	sm_streetLight.speedSP						= 0.0f;
	sm_streetLight.vColor						= g_visualSettings.GetColor("distantlights.streetlight.color");
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights::Shutdown			(unsigned shutdownMode)
{	
	if( shutdownMode == SHUTDOWN_CORE)
	{
#if ENABLE_DISTANT_CARS
		if (sm_pDistantCarsVtxDecl)
		{
			GRCDEVICE.DestroyVertexDeclaration(sm_pDistantCarsVtxDecl);
			sm_pDistantCarsVtxDecl = NULL;
		}

#if __PS3
		if(m_DistantCarsInstancesVB)
		{
			delete m_DistantCarsInstancesVB;
			m_DistantCarsInstancesVB = NULL;
		}
#else
		for (u32 i = 0; i < vbBufferCount; i++ )
		{
			if(m_DistantCarsInstancesVB[i])
			{
				delete m_DistantCarsInstancesVB[i];
				m_DistantCarsInstancesVB[i] = NULL;
			}
		}
#endif

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
#endif //ENABLE_DISTANT_CARS
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights::Update				()
{
	PF_FUNC(Update);

#if ENABLE_DISTANT_CARS
	sm_DistantCarBaseModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("Prop_Dummy_Car",0xC3F13FCC), &sm_DistantCarModelId);
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
	else if (!m_disableDistantCars && !m_CreatedVertexBuffers && !m_disableDistantCarsByScript)
	{
		if( sm_DistantCarBaseModelInfo && sm_DistantCarBaseModelInfo->GetHasLoaded() )
			CreateVertexBuffers();
	}
#endif

	// debug - render all groups to the vector map
#if __BANK
	if(!DebugLighting::m_DistantLightsProcessVisibilityAsync)
	{
		const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();

		m_interiorCullAll.GetWriteBuf() = false;

		if(g_SceneToGBufferPhaseNew && 
			g_SceneToGBufferPhaseNew->GetPortalVisTracker() && 
			g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid())
		{
			const bool bInteriorCullAll = !fwScan::GetScanResults().GetIsExteriorVisibleInGbuf();

			if(bInteriorCullAll == false)
			{
				fwScreenQuad quad = fwScan::GetScanResults().GetExteriorGbufScreenQuad();
				quad.GenerateFrustum(grcVP->GetCompositeMtx(), m_cullPlanes.GetWriteBuf());
			}
			else
			{
				m_interiorCullAll.GetWriteBuf() = true;
				m_cullPlanes.GetWriteBuf().Set(*grcVP, true, true);
			}		
		}
		else
		{
			m_cullPlanes.GetWriteBuf().Set(*grcVP, true, true);
		}
	}
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

void			CDistantLights::ProcessVisibility				()
{

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

#if !__FINAL && ENABLE_DISTANT_CARS
	if (m_disableDistantCars && m_disableDistantLights)
	{
		return;
	}
#endif

#if ENABLE_DISTANT_CARS
	if (m_disableDistantCarsByScript)
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




	g_DistantLightsVisibilityDependency.m_Priority = sysDependency::kPriorityMed;
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
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_GROUPS] = DISTANTLIGHTS_MAX_GROUPS * sizeof(DistantLightGroup_t);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_BLOCK_FIRST_GROUP] = DISTANTLIGHTS_MAX_BLOCKS * DISTANTLIGHTS_MAX_BLOCKS * sizeof(s32);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_BLOCK_NUM_COSTAL_GROUPS] = DISTANTLIGHTS_MAX_BLOCKS * DISTANTLIGHTS_MAX_BLOCKS * sizeof(s32);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_BLOCK_NUM_INLAND_GROUPS] = DISTANTLIGHTS_MAX_BLOCKS * DISTANTLIGHTS_MAX_BLOCKS * sizeof(s32);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_VISIBILITY] = sizeof(DistantLightsVisibilityData);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_SCAN_BASE_INFO] = sizeof(fwScanBaseInfo);
	g_DistantLightsVisibilityDependency.m_DataSizes[DLP_HIZ_BUFFER] = RAST_HIZ_AND_STENCIL_SIZE_BYTES;


	g_DistantLightsVisibilityDependencyRunning = 1;

	sysDependencyScheduler::Insert( &g_DistantLightsVisibilityDependency );

}

// ----------------------------------------------------------------------------------------------- //

void CDistantLights::WaitForProcessVisibilityDependency()
{
	BANK_ONLY(if(DebugLighting::m_DistantLightsProcessVisibilityAsync))
	{
		PF_PUSH_TIMEBAR("CDistantLights Wait for Visibility Dependency");
		while(g_DistantLightsVisibilityDependencyRunning)
		{
			sysIpcYield(PRIO_NORMAL);
		}
		PF_POP_TIMEBAR();
	}
}

#if ENABLE_DISTANT_CARS
void CDistantLights::RenderDistantCars()
{
#if !USE_DISTANT_LIGHTS_2
	g_distantLights.RenderDistantCarsInternal();
#else
	vfxAssertf(false, "Cannot render distant cars as distant cars 2 is in use!");
#endif // !USE_DISTANT_LIGHTS_2
}


bank_float g_min_fov = 20.0f;
bank_float g_max_fov = 45.0f;

void CDistantLights::RenderDistantCarsInternal()
{
#if !__FINAL
	if (m_disableDistantCars)
	{
		return;
	}
#endif

	if (m_disableDistantCarsByScript)
		return;

	// If it's nighttime, don't render the distant cars
	float coastalAlpha = Lights::CalculateTimeFade(
		g_distLightsParam.hourEnd - 1.0f,
		g_distLightsParam.hourEnd,
		g_distLightsParam.hourStart - 1.0f,
		g_distLightsParam.hourStart);

	if (coastalAlpha <= 0.0f)
	{
		return;
	}

	const grcViewport *viewPort = gVpMan.GetRenderGameGrcViewport();
	const Vector3 vCameraPos = VEC3V_TO_VECTOR3(viewPort->GetCameraPosition());

	if(!(sm_DistantCarBaseModelInfo && sm_DistantCarBaseModelInfo->GetHasLoaded()))
		return;

	if( !m_CreatedVertexBuffers )
	{
		CreateVertexBuffers();
	}

	rmcDrawable *drawable = sm_DistantCarBaseModelInfo->GetDrawable();
	const rmcLodGroup& lodGroup = drawable->GetLodGroup();
	const grmModel& model = lodGroup.GetLodModel0(LOD_HIGH);
	const grmGeometry& geom = model.GetGeometry(0);	 
#if __D3D11 || RSG_ORBIS
	const grcVertexBuffer* pVertexBuff = geom.GetVertexBufferByIndex(0);
	
	//Ensure model doesnt change, if it does need to check some bits.
	Assert(pVertexBuff->GetVertexCount() == 24);
#endif
	const grcIndexBuffer* pIndexBuff = geom.GetIndexBuffer(0);

	// set up the distance to be "distanceCarsNearDist" during the day, but at night time we want the
	// distance to be the "carLightNearFadeDist" because we don't want distant cars without lights on
	const float LodScaleDist = Max<float>(1.0f, g_LodScale.GetGlobalScaleForRenderThread() * CLODLights::GetScaleMultiplier());

	// this buffer value is here just to guarantee that we don't spawn a car without lights
	const float bufferValue = 30.0f;
	float fadeNearDist = (g_distLightsParam.carLightNearFadeDist * LodScaleDist) + bufferValue;

	float tValue = Lights::CalculateTimeFade(
		g_distLightsParam.hourStart - 1.0f,
		g_distLightsParam.hourStart,
		g_distLightsParam.hourEnd - 1.0f,
		g_distLightsParam.hourEnd);

	float nearCarDistance = Lerp( tValue, g_distLightsParam.distantCarsNearDist, fadeNearDist );

	float nearDist2 = (nearCarDistance*nearCarDistance);

	// as the sniper scope zooms in, fade out the cars
	float fov = camInterface::GetFov();
	float fDistantCarAlpha = 1.0f;
	if (fov < g_max_fov)
	{
		fDistantCarAlpha = Min(Max((fov - g_min_fov),0.0f) / (g_max_fov - g_min_fov), 1.0f);
	}

	m_DistantCarsShader->SetVar(m_DistantCarAlphaID, fDistantCarAlpha);

#if __PS3
	m_DistantCarsShader->SetVar(m_DistantCarNearClip, nearCarDistance);
	Verifyf(m_DistantCarsInstancesVB->Lock(rage::grcsDiscard), "Couldn't lock vertex buffer");
#else
	Verifyf(m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->Lock(rage::grcsDiscard), "Couldn't lock vertex buffer");
	DistantCarInstanceData* pDst = reinterpret_cast<DistantCarInstanceData*>(m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->GetLockPtr());

	u32 numToDraw = 0;
	for (int i = 0; i < m_numDistantCars;i++)
	{
		if (vCameraPos.Dist2(VEC3V_TO_VECTOR3(m_distantCarRenderData[i].pos)) > nearDist2)
		{
			pDst->vPositionX = m_distantCarRenderData[i].pos.GetXf();
			pDst->vPositionY = m_distantCarRenderData[i].pos.GetYf();
			pDst->vPositionZ = m_distantCarRenderData[i].pos.GetZf();
			pDst->vDirectionX = m_distantCarRenderData[i].dir.GetXf();
			pDst->vDirectionY = m_distantCarRenderData[i].dir.GetYf();
			pDst->vDirectionZ = m_distantCarRenderData[i].dir.GetZf();
			pDst++;

			numToDraw++;
		}
	}

	m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->UnlockWO();

	if (numToDraw == 0)
	{
		return;
	}

#endif // !__PS3

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
#if __XENON
	GRCDEVICE.SetStreamSource(0, *m_DistantCarsModelVB, 0, m_DistantCarsModelVB->GetVertexStride());
	GRCDEVICE.SetStreamSource(1, *m_DistantCarsModelIndexVB, 0, sizeof(float));
	GRCDEVICE.SetStreamSource(2, *m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer], 0, m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->GetVertexStride());
	GRCDEVICE.DrawVertices(drawTris, 0, pIndexBuff->GetIndexCount() * numToDraw);
#else
#if __PS3
	GRCDEVICE.SetIndices(*m_DistantCarsModelIB);
	GRCDEVICE.SetStreamSource(0, *m_DistantCarsModelVB, 0, m_DistantCarsModelVB->GetVertexStride());
	GRCDEVICE.SetStreamSource(1, *m_DistantCarsInstancesVB, 0, m_DistantCarsInstancesVB->GetVertexStride());

	for (int i = 0; i < m_numDistantCars; i += DISTANTCARS_BATCH_SIZE)
	{		
		int numToRender = Min(DISTANTCARS_BATCH_SIZE,m_numDistantCars - i);

		bool shouldRender = false;
		for (int carIndex = 0; carIndex < numToRender; carIndex++)
		{
			if (vCameraPos.Dist2(VEC3V_TO_VECTOR3(m_distantCarRenderData[i+carIndex].pos)) > nearDist2)
			{
				shouldRender = true;
				break;
			}
		}

		if (shouldRender)
		{
			GRCDEVICE.SetVertexShaderConstant(INSTANCE_SHADER_CONSTANT_SLOT, (float*)(&m_distantCarRenderData[i]), numToRender * 2);
			GRCDEVICE.DrawIndexedPrimitive(drawTris, 0, pIndexBuff->GetIndexCount() * numToRender);
		}
	}
#else
	GRCDEVICE.SetIndices(*m_DistantCarsModelIB);
	GRCDEVICE.SetStreamSource(0, *m_DistantCarsModelVB, 0, m_DistantCarsModelVB->GetVertexStride());
	GRCDEVICE.SetStreamSource(1, *m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer], 0, m_DistantCarsInstancesVB[m_DistantCarsCurVertexBuffer]->GetVertexStride());
	GRCDEVICE.DrawIndexedPrimitive(drawTris, 0, pIndexBuff->GetIndexCount() * numToDraw);
#endif
#endif
#endif

	m_DistantCarsShader->UnBind();
	m_DistantCarsShader->EndDraw();

	GRCDEVICE.ClearStreamSource(0);
	GRCDEVICE.ClearStreamSource(1);

	GRCDEVICE.SetVertexDeclaration(NULL);

	m_numDistantCars = 0;
#if !__PS3	
	m_DistantCarsCurVertexBuffer = (m_DistantCarsCurVertexBuffer+1)%vbBufferCount;
#endif // !__PS3
}

#endif //ENABLE_DISTANT_CARS

///////////////////////////////////////////////////////////////////////////////
//  Render
//  This functions combines both RenderWithVisibility and RenderGroups
///////////////////////////////////////////////////////////////////////////////
void CDistantLights::Render				(CVisualEffects::eRenderMode mode, float intensityScale)
{
	CDistantLightsVertexBuffer::CreateVertexBuffers();

	if (mode == CVisualEffects::RM_WATER_REFLECTION)
	{
		CRenderPhaseWaterReflectionInterface::AdjustDistantLights(intensityScale);

		if (intensityScale <= 0.0f)
		{
			return;
		}
	}

#if __BANK
	if(!DebugLighting::m_DistantLightsProcessVisibilityAsync)
	{
		RenderWithVisibility(mode);
		return;

	}
#endif

#if __BANK
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
#endif
	
	if( m_disableVehicleLights || CRenderer::GetDisableArtificialLights() )
	{
		return;
	}

	// check that we're being called by the render thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CDistantLights::Render - not called from the render thread");	

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
        if (m_disableDistantCarsByScript)
            return;

#if !__FINAL
		if (m_disableDistantLights)
		{
			return;
		}
		else
#endif //!__FINAL
		{
			distantCarsUpdate = true;
			PF_PUSH_TIMEBAR_DETAIL("Distant Cars' Update");
		}

	}
#endif //ENABLE_DISTANT_CARS


	PF_START(Render);

#if __BANK
	if (m_overrideCoastalAlpha>=0.0f)
	{
		coastalAlpha = m_overrideCoastalAlpha;
	}
#endif

	// calc the inland light alpha based on the height of the camera (helis need to see inland lights)
	Vec3V vCamPos = gVpMan.GetRenderGameGrcViewport()->GetCameraPosition();
	float inlandAlpha = Min(1.0f, ((vCamPos.GetZf()-g_distLightsParam.inlandHeight) / 40.0f));

#if __BANK
	if (m_overrideInlandAlpha>=0.0f)
	{
		inlandAlpha = m_overrideInlandAlpha;
	}
#endif

	const float LodScaleDist = Max<float>(1.0f, g_LodScale.GetGlobalScaleForRenderThread() * CLODLights::GetScaleMultiplier());

	float fadeNearDist = g_distLightsParam.carLightNearFadeDist * LodScaleDist;
	float fadeFarDist = g_distLightsParam.carLightFarFadeDist * LodScaleDist;

	CDistantLightsVertexBuffer::RenderBufferBegin(mode, fadeNearDist, fadeFarDist, intensityScale*g_distLightsParam.carLightHdrIntensity, true);
	const int bufferID = gRenderThreadInterface.GetRenderBuffer();
	const s32 numVisibleGroups = g_DistantLightsCount[bufferID];
	DistantLightsVisibilityOutput* pVisibleGroups = &(g_DistantLightsVisibility[bufferID][0]);

	DistantLightProperties coastalLightProperties, inlandLightProperties;
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

	const ScalarV elapsedTime = IntToFloatRaw<0>(ScalarVFromU32(fwTimer::GetTimeInMilliseconds())) * ScalarVConstant<FLOAT_TO_INT(0.001f)>(); // convert to seconds

	ScalarV carLight1Speed;
	ScalarV carLight2Speed;
	ScalarV carLight1Spacing;
	ScalarV carLight2Spacing;

	float randomizeSpeed;
	float randomizeSpacing; 

	if( NetworkInterface::IsGameInProgress() )
	{
		carLight1Speed = ScalarV(sm_carLight1.speedMP);
		carLight2Speed = ScalarV(sm_carLight2.speedMP);

		carLight1Spacing = ScalarV(sm_carLight1.spacingMP);
		carLight2Spacing = ScalarV(sm_carLight2.spacingMP);

		randomizeSpeed = g_distLightsParam.randomizeSpeedMP;
		randomizeSpacing = g_distLightsParam.randomizeSpacingMP; 
	}
	else
	{
		carLight1Speed = ScalarV(sm_carLight1.speedSP);
		carLight2Speed = ScalarV(sm_carLight2.speedSP);

		carLight1Spacing = ScalarV(sm_carLight1.spacingSP);
		carLight2Spacing = ScalarV(sm_carLight2.spacingSP);

		randomizeSpeed = g_distLightsParam.randomizeSpeedSP;
		randomizeSpacing = g_distLightsParam.randomizeSpacingSP; 
	}

	const ScalarV tooCloseDist2(fadeNearDist * fadeNearDist);

	const Vec3V carLightOffset(0.0f, 0.0f, g_distLightsParam.carLightZOffset);
	const Vec3V streetlightOffset(0.0f, 0.0f, g_distLightsParam.streetLightZOffset);

	const grcViewport *viewPort = gVpMan.GetRenderGameGrcViewport();
	const Vec3V vCameraPos = viewPort->GetCameraPosition();

	// The code below uses Vec2s because we're computing two speeds / positions, etc.
	// One for each direction along the road, because a lot of roads are two way
	// The 'x' component is for forward-going lanes (start to end), the 'y' is for backward (end to start)

	Vec2V randomSpacingMin = Vec2VConstant<FLOAT_TO_INT(0.4f), FLOAT_TO_INT(0.4f)>(); // Magic number to keep the spacing roughly the same after changing which density table we use
	Vec2V randomSpacingMax = randomSpacingMin;

	Vec2V randomSpeedMin = Vec2VFromF32(1.0f - randomizeSpeed);
	Vec2V randomSpeedMax = Vec2VFromF32(1.0f + randomizeSpeed);
	randomSpacingMin *= Vec2VFromF32(1.0f - randomizeSpacing); 
	randomSpacingMax *= Vec2VFromF32(1.0f + randomizeSpacing); 

	const Vec2V minTuningSpeed = Vec2V(g_distLightsParam.speed0Speed, -g_distLightsParam.speed0Speed);
	const Vec2V maxTuningSpeed = Vec2V(g_distLightsParam.speed3Speed, -g_distLightsParam.speed3Speed);

#if __WIN32PC
	// Vehicle density can be driven by script and validly set to 0. If this happens, early-out gracefully.
	if (CVehiclePopulation::GetRandomVehDensityMultiplier() != 0)
#endif
	{
		//Take output from processVisibility and cycle through visible groups
		for(s32 index = 0; index < numVisibleGroups; index++)
		{
#if ENABLE_DISTANT_CARS
			if (distantCarsUpdate && m_numDistantCars >= m_softLimitDistantCars)
				break;
#endif
			BANK_ONLY(s32 numRendered = 0;)
			DistantLightsVisibilityOutput& currentVisibleGroup = pVisibleGroups[index];
			if((mode == CVisualEffects::RM_DEFAULT && (currentVisibleGroup.visibilityFlags & DISTANT_LIGHT_VISIBLE_MAIN_VIEWPORT)) ||
				(mode == CVisualEffects::RM_WATER_REFLECTION && (currentVisibleGroup.visibilityFlags & DISTANT_LIGHT_VISIBLE_WATER_REFLECTION)) ||
				(mode == CVisualEffects::RM_MIRROR_REFLECTION && (currentVisibleGroup.visibilityFlags & DISTANT_LIGHT_VISIBLE_MIRROR_REFLECTION)))
			{

				const DistantLightGroup_t* pCurrGroup = &m_groups[currentVisibleGroup.index];
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
				// Address of the group makes a good random seed
				mthRandom perGroupRng((int)(size_t)(pCurrGroup));

				// Expand the group into a bunch of vec3vs so we can do faster math on them
				// (SoAs?)
				int baseVertexIdx = pCurrGroup->pointOffset;
				int numLineVerts = pCurrGroup->pointCount;
				FastAssert(numLineVerts < DISTANTLIGHTS_MAX_LINE_DATA);
				Vec3V lineVerts[DISTANTLIGHTS_MAX_LINE_DATA];
				Vec4V lineDiffs[DISTANTLIGHTS_MAX_LINE_DATA]; // (x,y,z) direction, w length

				ScalarV lineLength(V_ZERO);
				for(int i = 0; i < numLineVerts; i++)
				{
					lineVerts[i] = Vec3V(float(m_points[baseVertexIdx+i].x), float(m_points[baseVertexIdx+i].y), float(m_points[baseVertexIdx+i].z));
				}
				for(int i = 0; i < numLineVerts-1; i++)
				{
					Vec3V diff = lineVerts[i+1] - lineVerts[i];
					ScalarV segLength = Mag(diff);
					lineLength += segLength;
					lineDiffs[i] = Vec4V(Normalize(diff), segLength);
				}

				//Vec2V spacingFromAi = Vec2VFromF32( (CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup[pCurrGroup->density] / CVehiclePopulation::GetRandomVehDensityMultiplier() ) * pCurrGroup->numLanes);
				//if (g_distantLightsUseNewStuff && g_distantLightsTwoWayTraffic && pCurrGroup->twoWay)
				//{
				//	spacingFromAi *= Vec2V(V_HALF); // double the spacing, since we have half the cars going one way, half the other
				//}

				//// Randomize so not every road moves the same speed
				//spacingFromAi *= perGroupRng.GetRangedV(randomSpacingMin, randomSpacingMax);
				//spacingFromAi = Invert(spacingFromAi); // cars/m -> m/car

				Vec2V spacingFromAi = Vec2VFromF32(CVehiclePopulation::ms_fDefaultVehiclesSpacing[pCurrGroup->density]);
				spacingFromAi = spacingFromAi / (numLanesV * perGroupRng.GetRangedV(randomSpacingMin, randomSpacingMax));

				{
					Vec2V speedValue = IntToFloatRaw<0>(Vec2VFromU32(pCurrGroup->speed));

					// Map speed 0->3 to the speed we'll use. This also negates the 'y' speed
					Vec2V speedScale = Ramp(speedValue, Vec2V(V_ZERO), Vec2V(V_THREE), minTuningSpeed, maxTuningSpeed);
					speedScale *= perGroupRng.GetRangedV(randomSpeedMin, randomSpeedMax);

					Vec2V carLight1ScaledSpeed = speedScale * carLight1Speed;
					Vec2V carLight1ScaledSpacing = spacingFromAi * carLight1Spacing;
					Vec2V carLight1ElapsedDistance = elapsedTime * carLight1ScaledSpeed;

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
					Vec2V carLight1ElapsedQuadSpacingUnits = carLight1ElapsedDistance / (carLight1ScaledSpacing * ScalarV(V_FOUR));

					// X component has T value from 0 -> 3.9999 for how far into the current quad we are
					// Y component has T value from -3.999 -> 0
					Vec2V carLight1QuadT = (carLight1ElapsedQuadSpacingUnits - RoundToNearestIntZero(carLight1ElapsedQuadSpacingUnits)) * ScalarV(V_FOUR);

					ScalarV carLight1QuadTForward = carLight1QuadT.GetX();

					BoolV oneOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_ONE));
					BoolV twoOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_TWO));
					BoolV threeOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_THREE));

					Vec4V permutedLaneScalarsForward = SelectFT(twoOrMore, 
						SelectFT(oneOrMore, 
						laneScalars,		// 0 - 0.999
						laneScalars.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>()), // 1.0 - 1.999
						SelectFT(threeOrMore, 
						laneScalars.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>(), // 2.0 - 2.999
						laneScalars.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>())); // 3.0 - 3.999

					// How many cars have driven by:
					Vec2V carLight1ElapsedSpacingUnits = carLight1ElapsedQuadSpacingUnits * ScalarV(V_FOUR);

					// T value from 0 -> 0.9999 to start drawing at (y comp is -0.999 -> 0)
					Vec2V carLight1InitialT = (carLight1ElapsedSpacingUnits - RoundToNearestIntZero(carLight1ElapsedSpacingUnits));
					carLight1InitialT.SetY(carLight1InitialT.GetY() + ScalarV(V_ONE));

					Vec2V carLight1InitialOffset = carLight1InitialT * carLight1ScaledSpacing;

					// Given the first and last verts of the group, we find the "general direction" of the group and use this to 
					// 1) get a perpendicular vector so we can find a horizontal offset for multiple lanes and
					// 2) tell if the cars on the road are headed toward or away from the camera. Those going toward the camera should look like
					//		headlights, those going away should look like taillights
					Vec3V firstVert = lineVerts[0];
					Vec3V lastVert = lineVerts[numLineVerts-1];
					Vec3V generalDirection = firstVert - lastVert;
					generalDirection = Normalize(generalDirection);
					Vec3V perpDirection(-generalDirection.GetY(), generalDirection.GetX(), ScalarV(V_ZERO));
					perpDirection = Scale(perpDirection, ScalarV(V_THREE)); // hard coded 3m per lane

					Vec3V camToLineDir = Average(firstVert, lastVert) - vCameraPos;
					ScalarV camDirDotLine = Dot(camToLineDir.GetXY(), generalDirection.GetXY());

					if (pCurrGroup->twoWay)
					{
						// Select red or white lights based on the dot product (todo: lerp in some range so there is no pop?)
						ScalarV oneWayColor = SelectFT(IsLessThan(camDirDotLine, ScalarV(V_ZERO)), currentGroupProperties.scCarLight1Color, currentGroupProperties.scCarLight2Color);

						// car lights 1
						BANK_ONLY(numRendered += )
							PlaceLightsAlongGroup(lineLength, carLight1InitialOffset.GetX(), carLight1ScaledSpacing.GetX(), lineVerts, numLineVerts, lineDiffs, carLightOffset, oneWayColor, Vec4V(V_TWO) + permutedLaneScalarsForward, perpDirection, false);


						ScalarV carLight1QuadTBackward = carLight1QuadT.GetY() + ScalarV(V_FOUR); // adjust to 0..3.999

						BoolV oneOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_ONE));
						BoolV twoOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_TWO));
						BoolV threeOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_THREE));

						Vec4V permutedLaneScalarsBackward = SelectFT(twoOrMoreB, 
							SelectFT(oneOrMoreB, 
							laneScalars,	// 0 - 0.999
							laneScalars.Get<Vec::W, Vec::X, Vec::Y, Vec::X>()), // 1.0 - 1.999
							SelectFT(threeOrMoreB, 
							laneScalars.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>(), // 2.0 - 2.999
							laneScalars.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>())); // 3.0 - 3.999


						ScalarV otherWayColor = SelectFT(IsLessThan(camDirDotLine, ScalarV(V_ZERO)), currentGroupProperties.scCarLight2Color, currentGroupProperties.scCarLight1Color);

						BANK_ONLY(numRendered += )
							PlaceLightsAlongGroup(lineLength, carLight1InitialOffset.GetY(), carLight1ScaledSpacing.GetY(), lineVerts, numLineVerts, lineDiffs, carLightOffset, otherWayColor, Vec4V(V_NEGTWO) - permutedLaneScalarsBackward, perpDirection, true);
					}
					else
					{
						// Select red or white lights based on the dot product (todo: lerp in some range so there is no pop?)
						BoolV carDirectionChoice = IsLessThan(camDirDotLine, ScalarV(V_ZERO));
						ScalarV thisColor = SelectFT(carDirectionChoice, currentGroupProperties.scCarLight1Color, currentGroupProperties.scCarLight2Color);

						// car lights 1

						BANK_ONLY(numRendered += )
							PlaceLightsAlongGroup(lineLength, carLight1InitialOffset.GetX(), carLight1ScaledSpacing.GetX(), lineVerts, numLineVerts, lineDiffs, carLightOffset, thisColor, permutedLaneScalarsForward, perpDirection, carDirectionChoice.Getb());
					}


					// Note this does NOT put traffic going two directions down a single street. If we want that feature back, calculate a 1-t value for opposing traffic
					// and call PlaceLightsAlongGroup again

				}

#if 0 // Using LODLights for this now.
				// street lights
				// create a deterministic random number for this group
				if ( m_disableStreetLights == false )
				{
					float rand = g_randomSwitchOnValues[((m_points[pCurrGroup->pointOffset].x/300)+(m_points[pCurrGroup->pointOffset].y/500))%DISTANTLIGHTS_NUM_RANDOM_VALUES];
					if (pCurrGroup->useStreetlights && coastalAlpha >= rand)
					{
						numRendered += PlaceLightsAlongGroup(lineLength, ScalarV(V_ZERO), streetlightSpacing * spacingScale, lineVerts, numLineVerts, lineDiffs, streetlightOffset, scStreetlightColor);
					}
				}
#endif

				// debug - render active groups to the vector map
#if __BANK
				if (m_displayVectorMapActiveGroups || m_renderActiveGroups)
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
#endif
			}
			BANK_ONLY(m_numLightsRendered += (mode == CVisualEffects::RM_DEFAULT ? numRendered : 0);)
		}


	}
#if __BANK
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
#endif	

	CDistantLightsVertexBuffer::RenderBuffer(true);

	// reset the hdr intensity
	CShaderLib::SetGlobalEmissiveScale(1.0f);

	PF_STOP(Render);

#if ENABLE_DISTANT_CARS
	if (distantCarsUpdate)
	{
		PF_POP_TIMEBAR_DETAIL();
	}
#endif

	PF_SETTIMER(Render_NoStall, PF_READ_TIME(Render) - PF_READ_TIME(Stall));
	PF_SETTIMER(RenderBuffer_NoStall, PF_READ_TIME(RenderBuffer) - PF_READ_TIME(Stall));
}



#if __BANK
///////////////////////////////////////////////////////////////////////////////
//  RenderWithVisibility -old Render Function with Visibility testing
///////////////////////////////////////////////////////////////////////////////

void			CDistantLights::RenderWithVisibility			(CVisualEffects::eRenderMode mode)
{

#if __BANK
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
#endif

	// check that we're being called by the render thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CDistantLights::Render - not called from the render thread");	

	// set the coastal light alpha - default to off
	float coastalAlpha = Lights::CalculateTimeFade(
		g_distLightsParam.hourStart - 1.0f,
		g_distLightsParam.hourStart,
		g_distLightsParam.hourEnd - 1.0f,
		g_distLightsParam.hourEnd);

	// if there are no coastal lights on then return
#if ENABLE_DISTANT_CARS
	bool distantCarsUpdate = false;
	if (coastalAlpha <= 0.0f)
	{
        if (m_disableDistantCarsByScript)
            return;

#if !__FINAL
		if (m_disableDistantLights)
		{
			return;
		}
		else
#endif //!__FINAL
		{
			distantCarsUpdate = true;
			PF_PUSH_TIMEBAR_DETAIL("Distant Cars' Update");
		}
	}
#else
	if (coastalAlpha <= 0.0f)
	{
		return;
	}
#endif

	// if the exterior world isn't visible, then return
	if (m_interiorCullAll.GetReadBuf())
	{
		return;
	}

	PF_START(Render);

#if __BANK
	if (m_overrideCoastalAlpha>=0.0f)
	{
		coastalAlpha = m_overrideCoastalAlpha;
	}
#endif

	// calc the inland light alpha based on the height of the camera (helis need to see inland lights)
	Vec3V vCamPos = gVpMan.GetRenderGameGrcViewport()->GetCameraPosition();
	float inlandAlpha = Min(1.0f, ((vCamPos.GetZf()-g_distLightsParam.inlandHeight) / 40.0f));

#if __BANK
	if (m_overrideInlandAlpha>=0.0f)
	{
		inlandAlpha = m_overrideInlandAlpha;
	}
#endif

	const float LodScaleDist = Max<float>(1.0f, g_LodScale.GetGlobalScaleForRenderThread() * CLODLights::GetScaleMultiplier());

	float fadeNearDist = g_distLightsParam.carLightNearFadeDist * LodScaleDist;
	float fadeFarDist = g_distLightsParam.carLightFarFadeDist * LodScaleDist;

	CDistantLightsVertexBuffer::RenderBufferBegin(mode, fadeNearDist, fadeFarDist, g_distLightsParam.carLightHdrIntensity, true);

	// go through the blocks and render the distant lights
	for (s32 blockX=0; blockX<DISTANTLIGHTS_MAX_BLOCKS; blockX++)
	{
#if ENABLE_DISTANT_CARS
		if (distantCarsUpdate && m_numDistantCars >= m_softLimitDistantCars)
			break;
#endif

		for (s32 blockY=0; blockY<DISTANTLIGHTS_MAX_BLOCKS; blockY++)
		{
#if ENABLE_DISTANT_CARS
			if (distantCarsUpdate && m_numDistantCars >= m_softLimitDistantCars)
				break;
#endif

			// check if there are any groups in this block
			if (m_blockNumCoastalGroups[blockX][blockY]>0 || m_blockNumInlandGroups[blockX][blockY]>0)
			{
				// calc the centre pos of the block
				float centrePosX, centrePosY;
				DistantLightsVisibility::FindBlockCentre(blockX, blockY, centrePosX, centrePosY);
				Vec3V vCentrePos(centrePosX, centrePosY, 20.0f);

				float blockMinX, blockMinY, blockMaxX, blockMaxY;
				DistantLightsVisibility::FindBlockMin(blockX, blockY, blockMinX, blockMinY);
				DistantLightsVisibility::FindBlockMax(blockX, blockY, blockMaxX, blockMaxY);

				spdAABB box(
					Vec3V(blockMinX, blockMinY, 20.0f - DISTANTLIGHTS_BLOCK_RADIUS),
					Vec3V(blockMaxX, blockMaxY, 20.0f + DISTANTLIGHTS_BLOCK_RADIUS)
					);

				if (m_cullPlanes.GetReadBuf().IntersectsAABB(box))
				{
					if (mode != CVisualEffects::RM_DEFAULT || COcclusion::IsBoxVisible(box))
					{
						// loop through the block's lights twice
						// the first time to do coastal lights and the second time to do inland lights
						for (s32 type=0; type<2; type++)
						{
							if (type==0)
							{
								s32 groupOffset = m_blockFirstGroup[blockX][blockY];
								s32 numGroups = m_blockNumCoastalGroups[blockX][blockY];
								float alpha = coastalAlpha;
#if __BANK
								s32 numRendered = 
#endif
									RenderGroups(groupOffset, numGroups, fadeNearDist, fadeFarDist, alpha, coastalAlpha BANK_ONLY(, mode));
#if __BANK
								m_numLightsRendered += (mode == CVisualEffects::RM_DEFAULT ? numRendered : 0);
#endif
							}
							else if (type==1 && inlandAlpha>0.0f)
							{
								s32 groupOffset = m_blockFirstGroup[blockX][blockY] + m_blockNumCoastalGroups[blockX][blockY];
								s32 numGroups = m_blockNumInlandGroups[blockX][blockY];
								float alpha = coastalAlpha * inlandAlpha;
#if __BANK 
								s32 numRendered = 
#endif
									RenderGroups(groupOffset, numGroups, fadeNearDist, fadeFarDist, alpha, coastalAlpha BANK_ONLY(, mode));
#if __BANK
								m_numLightsRendered += (mode == CVisualEffects::RM_DEFAULT ? numRendered : 0);
#endif
							}
						}

						// debug - render active blocks to the vector map
#if __BANK
						m_numBlocksRendered += (mode == CVisualEffects::RM_DEFAULT ? 1 : 0);

						if (m_displayVectorMapBlocks && mode == CVisualEffects::RM_DEFAULT)
						{
							//						Vec3V vCentrePos = box.GetCenter();
							//						CVectorMap::DrawCircle(RCC_VECTOR3(vCentrePos), DISTANTLIGHTS_BLOCK_RADIUS, Color32(255, 255, 255, 255), false);
							CVectorMap::Draw3DBoundingBoxOnVMap(box, Color32(255, 255, 255, 255));
						}
#endif
					}
				}
			}
		}
	}

	CDistantLightsVertexBuffer::RenderBuffer(true);

	// reset the hdr intensity
	CShaderLib::SetGlobalEmissiveScale(1.0f);

	PF_STOP(Render);

#if ENABLE_DISTANT_CARS
	if (distantCarsUpdate)
	{
		PF_POP_TIMEBAR_DETAIL();
	}
#endif

	PF_SETTIMER(Render_NoStall, PF_READ_TIME(Render) - PF_READ_TIME(Stall));
	PF_SETTIMER(RenderGroups_NoStall, PF_READ_TIME(RenderGroups) - PF_READ_TIME(Stall));
	PF_SETTIMER(RenderBuffer_NoStall, PF_READ_TIME(RenderBuffer) - PF_READ_TIME(Stall));
}


///////////////////////////////////////////////////////////////////////////////
//  RenderGroups - called only from RenderWithVisibility
///////////////////////////////////////////////////////////////////////////////
s32			CDistantLights::RenderGroups		(s32 groupOffset, s32 numGroups, float fadeNearDist, float /*fadeFarDist*/, float alpha, float /*coastalAlpha*/, CVisualEffects::eRenderMode mode )
{
	PF_FUNC(RenderGroups);

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	// Vehicle density can be driven by script and validly set to 0. If this happens, early-out gracefully.
	if (CVehiclePopulation::GetRandomVehDensityMultiplier() == 0)
		return 0;
#endif


	ScalarV alphaV(alpha);

	Color32 carLight1Color(sm_carLight1.vColor * alphaV);
	Color32 carLight2Color(sm_carLight2.vColor * alphaV);
	Color32 streetlightColor(sm_streetLight.vColor * alphaV);

	ScalarV scCarLight1Color = ScalarVFromU32(carLight1Color.GetDeviceColor());
	ScalarV scCarLight2Color = ScalarVFromU32(carLight2Color.GetDeviceColor());
	//ScalarV scStreetlightColor = ScalarVFromU32(streetlightColor.GetDeviceColor());

	s32 numRendered = 0;
	// const Vec3V vCamPos = RCC_VEC3V(RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx()).d);
	const grcViewport *vp = grcViewport::GetCurrent();


	ScalarV elapsedTime = IntToFloatRaw<0>(ScalarVFromU32(fwTimer::GetTimeInMilliseconds())) * ScalarVConstant<FLOAT_TO_INT(0.001f)>(); // convert to seconds

	ScalarV carLight1Speed;
	ScalarV carLight2Speed;
	ScalarV carLight1Spacing;
	ScalarV carLight2Spacing;

	float randomizeSpeed;
	float randomizeSpacing; 

	if( NetworkInterface::IsGameInProgress() )
	{
		carLight1Speed = ScalarV(sm_carLight1.speedMP);
		carLight2Speed = ScalarV(sm_carLight2.speedMP);

		carLight1Spacing = ScalarV(sm_carLight1.spacingMP);
		carLight2Spacing = ScalarV(sm_carLight2.spacingMP);

		randomizeSpeed = g_distLightsParam.randomizeSpeedMP;
		randomizeSpacing = g_distLightsParam.randomizeSpacingMP; 
	}
	else
	{
		carLight1Speed = ScalarV(sm_carLight1.speedSP);
		carLight2Speed = ScalarV(sm_carLight2.speedSP);

		carLight1Spacing = ScalarV(sm_carLight1.spacingSP);
		carLight2Spacing = ScalarV(sm_carLight2.spacingSP);

		randomizeSpeed = g_distLightsParam.randomizeSpeedSP;
		randomizeSpacing = g_distLightsParam.randomizeSpacingSP; 
	}

	ScalarV tooCloseDist2(fadeNearDist * fadeNearDist);

	Vec3V carLightOffset(0.0f, 0.0f, g_distLightsParam.carLightZOffset);
	Vec3V streetlightOffset(0.0f, 0.0f, g_distLightsParam.streetLightZOffset);

	const grcViewport *viewPort = gVpMan.GetRenderGameGrcViewport();
	const Vec3V vCameraPos = viewPort->GetCameraPosition();

	// The code below uses Vec2s because we're computing two speeds / positions, etc.
	// One for each direction along the road, because a lot of roads are two way
	// The 'x' component is for forward-going lanes (start to end), the 'y' is for backward (end to start)
	Vec2V randomSpeedMin = Vec2VFromF32(1.0f - randomizeSpeed);
	Vec2V randomSpeedMax = Vec2VFromF32(1.0f + randomizeSpeed);

	Vec2V randomSpacingMin = Vec2VConstant<FLOAT_TO_INT(0.4f), FLOAT_TO_INT(0.4f)>(); // Magic number to keep the spacing roughly the same after changing which density table we use
	Vec2V randomSpacingMax = randomSpacingMin;
	randomSpacingMin *= Vec2VFromF32(1.0f - randomizeSpacing); 
	randomSpacingMax *= Vec2VFromF32(1.0f + randomizeSpacing); 

	Vec2V minTuningSpeed = Vec2V(g_distLightsParam.speed0Speed, -g_distLightsParam.speed0Speed);
	Vec2V maxTuningSpeed = Vec2V(g_distLightsParam.speed3Speed, -g_distLightsParam.speed3Speed);

	// Go through all of the groups and render the white carlights on them.
	for (s32 s=groupOffset; s<groupOffset+numGroups; s++)
	{
		const DistantLightGroup_t* pCurrGroup = &m_groups[s];

		Vec4V laneScalars;

		int numLanes = pCurrGroup->numLanes;
		Vec2V numLanesV(V_ONE);
		if (pCurrGroup->twoWay)
		{
			numLanes = (numLanes + 1)/2; // halve the number of lanes, round up
		}

		switch(numLanes)
		{
		case 1:	laneScalars = Vec4V(0.0f, 0.2f, 0.0f, -0.1f); numLanesV = Vec2V(V_ONE); break;
		case 2: laneScalars = Vec4V(1.1f, 0.2f, 0.9f, -0.2f); numLanesV = Vec2V(V_TWO); break;
		case 3: laneScalars = Vec4V(0.0f, 1.0f, 0.0f, 2.0f); numLanesV = Vec2V(V_THREE); break;
		default: laneScalars = Vec4V(0.0f, 2.0f, 1.0f, 3.0f); numLanesV = IntToFloatRaw<0>(Vec2VFromS32(numLanes)); break;
		}

		Vec4V sphere((float)pCurrGroup->centreX,
					(float)pCurrGroup->centreY,
					(float)pCurrGroup->centreZ,
					(float)pCurrGroup->radius );

		// check if the group is visible
		if( vp->IsSphereVisible(sphere) /* && COcclusion::IsSphereVisible(spdSphere(sphere)) */ )
		{
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
			// Address of the group makes a good random seed
			mthRandom perGroupRng((int)(size_t)(pCurrGroup));

			// Expand the group into a bunch of vec3vs so we can do faster math on them
			// (SoAs?)
			int baseVertexIdx = pCurrGroup->pointOffset;
			int numLineVerts = pCurrGroup->pointCount;
			FastAssert(numLineVerts < DISTANTLIGHTS_MAX_LINE_DATA);
			Vec3V lineVerts[DISTANTLIGHTS_MAX_LINE_DATA];
			Vec4V lineDiffs[DISTANTLIGHTS_MAX_LINE_DATA]; // (x,y,z) direction, w length

			ScalarV lineLength(V_ZERO);
			for(int i = 0; i < numLineVerts; i++)
			{
				lineVerts[i] = Vec3V(float(m_points[baseVertexIdx+i].x), float(m_points[baseVertexIdx+i].y), float(m_points[baseVertexIdx+i].z));
			}
			for(int i = 0; i < numLineVerts-1; i++)
			{
				Vec3V diff = lineVerts[i+1] - lineVerts[i];
				ScalarV segLength = Mag(diff);
				lineLength += segLength;
				lineDiffs[i] = Vec4V(Normalize(diff), segLength);
			}

			//Vec2V spacingFromAi = Vec2VFromF32( (CVehiclePopulation::ms_fVehiclesPerMeterDensityLookup[pCurrGroup->density] / CVehiclePopulation::GetRandomVehDensityMultiplier() ) * pCurrGroup->numLanes);
			//if (g_distantLightsUseNewStuff && g_distantLightsTwoWayTraffic && pCurrGroup->twoWay)
			//{
			//	spacingFromAi *= Vec2V(V_HALF); // double the spacing, since we have half the cars going one way, half the other
			//}

			//// Randomize so not every road moves the same speed
			//spacingFromAi *= perGroupRng.GetRangedV(randomSpacingMin, randomSpacingMax);
			//spacingFromAi = Invert(spacingFromAi); // cars/m -> m/car

			Vec2V spacingFromAi = Vec2VFromF32(CVehiclePopulation::ms_fDefaultVehiclesSpacing[pCurrGroup->density]);
			spacingFromAi = spacingFromAi / (numLanesV * perGroupRng.GetRangedV(randomSpacingMin, randomSpacingMax));

			{
				Vec2V speedValue = IntToFloatRaw<0>(Vec2VFromU32(pCurrGroup->speed));

				// Map speed 0->3 to the speed we'll use. This also negates the 'y' speed
				Vec2V speedScale = Ramp(speedValue, Vec2V(V_ZERO), Vec2V(V_THREE), minTuningSpeed, maxTuningSpeed);
				speedScale *= perGroupRng.GetRangedV(randomSpeedMin, randomSpeedMax);

				Vec2V carLight1ScaledSpeed = speedScale * carLight1Speed;
				Vec2V carLight1ScaledSpacing = spacingFromAi * carLight1Spacing;
				Vec2V carLight1ElapsedDistance = elapsedTime * carLight1ScaledSpeed;

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
				Vec2V carLight1ElapsedQuadSpacingUnits = carLight1ElapsedDistance / (carLight1ScaledSpacing * ScalarV(V_FOUR));

				// X component has T value from 0 -> 3.9999 for how far into the current quad we are
				// Y component has T value from -3.999 -> 0
				Vec2V carLight1QuadT = (carLight1ElapsedQuadSpacingUnits - RoundToNearestIntZero(carLight1ElapsedQuadSpacingUnits)) * ScalarV(V_FOUR);

				ScalarV carLight1QuadTForward = carLight1QuadT.GetX();

				BoolV oneOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_ONE));
				BoolV twoOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_TWO));
				BoolV threeOrMore = IsGreaterThanOrEqual(carLight1QuadTForward, ScalarV(V_THREE));

				Vec4V permutedLaneScalarsForward = SelectFT(twoOrMore, 
					SelectFT(oneOrMore, 
						laneScalars,		// 0 - 0.999
						laneScalars.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>()), // 1.0 - 1.999
					SelectFT(threeOrMore, 
						laneScalars.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>(), // 2.0 - 2.999
						laneScalars.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>())); // 3.0 - 3.999

				// How many cars have driven by:
				Vec2V carLight1ElapsedSpacingUnits = carLight1ElapsedQuadSpacingUnits * ScalarV(V_FOUR);

				// T value from 0 -> 0.9999 to start drawing at (y comp is -0.999 -> 0)
				Vec2V carLight1InitialT = (carLight1ElapsedSpacingUnits - RoundToNearestIntZero(carLight1ElapsedSpacingUnits));
				carLight1InitialT.SetY(carLight1InitialT.GetY() + ScalarV(V_ONE));
				
				Vec2V carLight1InitialOffset = carLight1InitialT * carLight1ScaledSpacing;

				// Given the first and last verts of the group, we find the "general direction" of the group and use this to 
				// 1) get a perpendicular vector so we can find a horizontal offset for multiple lanes and
				// 2) tell if the cars on the road are headed toward or away from the camera. Those going toward the camera should look like
				//		headlights, those going away should look like taillights
				Vec3V firstVert = lineVerts[0];
				Vec3V lastVert = lineVerts[numLineVerts-1];
				Vec3V generalDirection = firstVert - lastVert;
				generalDirection = Normalize(generalDirection);
				Vec3V perpDirection(-generalDirection.GetY(), generalDirection.GetX(), ScalarV(V_ZERO));
				perpDirection = Scale(perpDirection, ScalarV(V_THREE)); // hard coded 3m per lane

				Vec3V camToLineDir = Average(firstVert, lastVert) - vCameraPos;
				ScalarV camDirDotLine = Dot(camToLineDir.GetXY(), generalDirection.GetXY());

				if (pCurrGroup->twoWay)
				{
					// Select red or white lights based on the dot product (todo: lerp in some range so there is no pop?)
					ScalarV oneWayColor = SelectFT(IsLessThan(camDirDotLine, ScalarV(V_ZERO)), scCarLight1Color, scCarLight2Color);

					// car lights 1
					numRendered += PlaceLightsAlongGroup(lineLength, carLight1InitialOffset.GetX(), carLight1ScaledSpacing.GetX(), lineVerts, numLineVerts, lineDiffs, carLightOffset, oneWayColor, Vec4V(V_TWO) + permutedLaneScalarsForward, perpDirection, false);


					ScalarV carLight1QuadTBackward = carLight1QuadT.GetY() + ScalarV(V_FOUR); // adjust to 0..3.999

					BoolV oneOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_ONE));
					BoolV twoOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_TWO));
					BoolV threeOrMoreB = IsGreaterThanOrEqual(carLight1QuadTBackward, ScalarV(V_THREE));

					Vec4V permutedLaneScalarsBackward = SelectFT(twoOrMoreB, 
						SelectFT(oneOrMoreB, 
						laneScalars,	// 0 - 0.999
						laneScalars.Get<Vec::W, Vec::X, Vec::Y, Vec::X>()), // 1.0 - 1.999
						SelectFT(threeOrMoreB, 
						laneScalars.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>(), // 2.0 - 2.999
						laneScalars.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>())); // 3.0 - 3.999


					ScalarV otherWayColor = SelectFT(IsLessThan(camDirDotLine, ScalarV(V_ZERO)), scCarLight2Color, scCarLight1Color);

					numRendered += PlaceLightsAlongGroup(lineLength, carLight1InitialOffset.GetY(), carLight1ScaledSpacing.GetY(), lineVerts, numLineVerts, lineDiffs, carLightOffset, otherWayColor, Vec4V(V_NEGTWO) - permutedLaneScalarsBackward, perpDirection, true);
				}
				else
				{
					// Select red or white lights based on the dot product (todo: lerp in some range so there is no pop?)
					BoolV carDirectionChoice = IsLessThan(camDirDotLine, ScalarV(V_ZERO));
					ScalarV thisColor = SelectFT(carDirectionChoice, scCarLight1Color, scCarLight2Color);

					// car lights 1
					numRendered += PlaceLightsAlongGroup(lineLength, carLight1InitialOffset.GetX(), carLight1ScaledSpacing.GetX(), lineVerts, numLineVerts, lineDiffs, carLightOffset, thisColor, permutedLaneScalarsForward, perpDirection, carDirectionChoice.Getb());
				}


				// Note this does NOT put traffic going two directions down a single street. If we want that feature back, calculate a 1-t value for opposing traffic
				// and call PlaceLightsAlongGroup again
				
			}
			
#if 0 // Using LODLights for this now.
			// street lights
			// create a deterministic random number for this group
			if ( m_disableStreetLights == false )
			{
				float rand = g_randomSwitchOnValues[((m_points[pCurrGroup->pointOffset].x/300)+(m_points[pCurrGroup->pointOffset].y/500))%DISTANTLIGHTS_NUM_RANDOM_VALUES];
				if (pCurrGroup->useStreetlights && coastalAlpha >= rand)
				{
					numRendered += PlaceLightsAlongGroup(lineLength, ScalarV(V_ZERO), streetlightSpacing * spacingScale, lineVerts, numLineVerts, lineDiffs, streetlightOffset, scStreetlightColor);
				}
			}
#endif

			// debug - render active groups to the vector map
#if __BANK
			if (m_displayVectorMapActiveGroups || m_renderActiveGroups)
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
			m_numGroupsRendered += (mode == CVisualEffects::RM_DEFAULT ? 1 : 0);

#endif
		}
	}

	return numRendered;
}


#endif //__BANK

///////////////////////////////////////////////////////////////////////////////
//  PlaceLightsAlongGroup
///////////////////////////////////////////////////////////////////////////////

int CDistantLights::PlaceLightsAlongGroup( ScalarV_In lineLength, ScalarV_In initialOffset, ScalarV_In spacing, const Vec3V * lineVerts, int numLineVerts, const Vec4V * lineDiffs, Vec3V_In lightWorldOffset, ScalarV_In color, Vec4V_In laneScalars, Vec3V_In perpDirection, bool carDirectionChoice )
{
	// New algorithm:
	// Get total line length
	// Divide total length by spacing to get # of lights
	// For each 4 lights:
	//	Build a vector of distance values for the 4 lights
	//	Loop over all the segments:
	//	interpolate 4 points along segment
	//	subtract segment length
	//	update positions 

	ScalarV effectiveLineLength = lineLength - initialOffset;
	int numLights = FloatToIntRaw<0>(RoundToNearestIntPosInf(effectiveLineLength / spacing)).Geti();

	if (numLights <= 0)
		return 0;

	Vec4V multiplier = Vec4V(ScalarV(V_ZERO), ScalarV(V_ONE), ScalarV(V_TWO), ScalarV(V_THREE));
	Vec4V desiredDistAlongLine = AddScaled(Vec4V(initialOffset), multiplier, spacing);

	PF_START(Stall);
	Vec4V* RESTRICT outputBuff = CDistantLightsVertexBuffer::ReserveRenderBufferSpace(numLights);
	PF_STOP(Stall);
	if (!outputBuff) 
	{
		return 0;
	}

	Vec4V vFour(V_FOUR);
	Vec4V vZero(V_ZERO);

	Vec3V laneOffset0 = perpDirection * laneScalars.GetX();
	Vec3V laneOffset1 = perpDirection * laneScalars.GetY();
	Vec3V laneOffset2 = perpDirection * laneScalars.GetZ();
	Vec3V laneOffset3 = perpDirection * laneScalars.GetW();

	// For each 4 lights we want to place
	for(int i = 0; i < numLights; i += 4)
	{
		Vec4V distRemainingAlongLine = desiredDistAlongLine;

		Vec3V finalPos0 = lineVerts[0];
		Vec3V finalPos1 = lineVerts[0];
		Vec3V finalPos2 = lineVerts[0];
		Vec3V finalPos3 = lineVerts[0];
		VecBoolV updateFinalPos(V_TRUE);

		// For each line segment
#if ENABLE_DISTANT_CARS
		Vec3V direction[4];
#endif
		for(int j = 0; j < numLineVerts-1; j++)
		{
			Vec3V interp0 = AddScaled(lineVerts[j], lineDiffs[j].GetXYZ(), distRemainingAlongLine.GetX());
			Vec3V interp1 = AddScaled(lineVerts[j], lineDiffs[j].GetXYZ(), distRemainingAlongLine.GetY());
			Vec3V interp2 = AddScaled(lineVerts[j], lineDiffs[j].GetXYZ(), distRemainingAlongLine.GetZ());
			Vec3V interp3 = AddScaled(lineVerts[j], lineDiffs[j].GetXYZ(), distRemainingAlongLine.GetW());

			updateFinalPos = IsGreaterThanOrEqual(distRemainingAlongLine, vZero);

			finalPos0 = SelectFT(updateFinalPos.GetX(), finalPos0, interp0);
			finalPos1 = SelectFT(updateFinalPos.GetY(), finalPos1, interp1);
			finalPos2 = SelectFT(updateFinalPos.GetZ(), finalPos2, interp2);
			finalPos3 = SelectFT(updateFinalPos.GetW(), finalPos3, interp3);

#if ENABLE_DISTANT_CARS
			//Get the direction of the light for distant cars.
			direction[0] = SelectFT(updateFinalPos.GetX(), direction[0], lineDiffs[j].GetXYZ());
			direction[1] = SelectFT(updateFinalPos.GetY(), direction[1], lineDiffs[j].GetXYZ());
			direction[2] = SelectFT(updateFinalPos.GetZ(), direction[2], lineDiffs[j].GetXYZ());
			direction[3] = SelectFT(updateFinalPos.GetW(), direction[3], lineDiffs[j].GetXYZ());
#endif //ENABLE_DISTANT_CARS

			distRemainingAlongLine -= Vec4V(lineDiffs[j].GetW());
		}

		desiredDistAlongLine = AddScaled(desiredDistAlongLine, vFour, spacing);

		// Note that we write all 4 verts. When we reserved, we reserved some extra space so this won't crash
		// (but we only bumped up the actual vert count by the true number of lights we want, so the next
		// reservation will overwrite those bonus ones we added)
#if __PPU || __XENON ||  RSG_ORBIS || (__WIN32PC && __D3D11)
		outputBuff[0] = Vec4V(finalPos0 + lightWorldOffset + laneOffset0, color);
		outputBuff[1] = Vec4V(finalPos1 + lightWorldOffset + laneOffset1, color);
		outputBuff[2] = Vec4V(finalPos2 + lightWorldOffset + laneOffset2, color);
		outputBuff[3] = Vec4V(finalPos3 + lightWorldOffset + laneOffset3, color);

#if ENABLE_DISTANT_CARS
		//Offset the car so we can see the distant lights on the car.
		//Depending on which colour light it is offset it the right way so its on the correct end of the car.
		//Might be better offsetting the lights?
		ScalarV lightScale = ScalarV(carDirectionChoice ? g_distLightsParam.distantCarLightOffsetRed : g_distLightsParam.distantCarLightOffsetWhite);
		Vec3V lightOffset = Vec3V(0.0f, 0.0f, g_distLightsParam.distantCarZOffset);

		Vec3V CarOffset0 = AddScaled(lightOffset, direction[0], lightScale);
		Vec3V CarOffset1 = AddScaled(lightOffset, direction[1], lightScale);
		Vec3V CarOffset2 = AddScaled(lightOffset, direction[2], lightScale);
		Vec3V CarOffset3 = AddScaled(lightOffset, direction[3], lightScale);

		Vec3V distCarPos[4];
		distCarPos[0] = Vec3V(finalPos0 + CarOffset0 + laneOffset0);
		distCarPos[1] = Vec3V(finalPos1 + CarOffset1 + laneOffset1);
		distCarPos[2] = Vec3V(finalPos2 + CarOffset2 + laneOffset2);
		distCarPos[3] = Vec3V(finalPos3 + CarOffset3 + laneOffset3);

		//Update vertex data.
		for (int j = 0; j < 4 && i + j < numLights; j++)
		{
			if (m_numDistantCars < m_softLimitDistantCars)
			{
				m_distantCarRenderData[m_numDistantCars].pos = distCarPos[j];
				m_distantCarRenderData[m_numDistantCars].dir = direction[j];
				m_numDistantCars++;
			}
		}
#else
		(void) carDirectionChoice;
#endif

		outputBuff += 4;
#else //__WIN32PC && __D3D9
		(void) carDirectionChoice;
		// On PC DX9 as we cant use quads we need to write out 6 verts for each light.
		for( u32 i = 0; i < 6; i++ )
		{
			outputBuff[i] =		Vec4V(finalPos0 + lightWorldOffset + laneOffset0, color);
			outputBuff[6+i] =	Vec4V(finalPos1 + lightWorldOffset + laneOffset1, color);
			outputBuff[12+i] =	Vec4V(finalPos2 + lightWorldOffset + laneOffset2, color);
			outputBuff[18+i] =	Vec4V(finalPos3 + lightWorldOffset + laneOffset3, color);
		}
		outputBuff += 24;
#endif
	}

	return numLights;
}


///////////////////////////////////////////////////////////////////////////////
//  Load
///////////////////////////////////////////////////////////////////////////////

void CDistantLights::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DISTANT_LIGHTS_FILE);
	if(!DATAFILEMGR.IsValid(pData))
		return;

	FileHandle fileId;
	fileId	= CFileMgr::OpenFile(pData->m_filename);

	if (CFileMgr::IsValidFileHandle(fileId))
	{
		s32 bytes = DistantLightSaveData::Read(fileId, &m_numPoints, 1);
		vfxAssertf(bytes==4, "CDistantLights::LoadData - error reading number of points");

		bytes = DistantLightSaveData::Read(fileId, &m_numGroups, 1);
		vfxAssertf(bytes==4, "CDistantLights::LoadData - error reading number of groups");

		bytes = DistantLightSaveData::Read(fileId, &m_blockFirstGroup[0][0], DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS);
		vfxAssertf(bytes==(s32)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::LoadData - error reading block's first group");

		bytes = DistantLightSaveData::Read(fileId, &m_blockNumCoastalGroups[0][0], DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS);
		vfxAssertf(bytes==(s32)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::LoadData - error reading number of block's coastal group");

		bytes = DistantLightSaveData::Read(fileId, &m_blockNumInlandGroups[0][0], DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS);
		vfxAssertf(bytes==(s32)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::LoadData - error reading number of block's inland group");

		bytes = DistantLightSaveData::Read(fileId, &m_points[0].x, m_numPoints*sizeof(DistantLightPoint_t)/sizeof(s16));
		vfxAssertf(bytes==(s32)(m_numPoints*sizeof(DistantLightPoint_t)), "CDistantLights::LoadData - error reading number of block's points");

		bytes = DistantLightSaveData::Read(fileId, &m_groups[0], m_numGroups);
		vfxAssertf(bytes==(s32)(m_numGroups*sizeof(DistantLightGroup_t)), "CDistantLights::LoadData - error reading number of block's groups");

		CFileMgr::CloseFile(fileId);
	}

	CFileMgr::SetDir("");

	s16 maxPointCount = 0;
	for(int i = 0; i < m_numGroups; i++)
	{
		maxPointCount = Max(maxPointCount, m_groups[i].pointCount);
	}

	Displayf("@@@@@@ MAX POINT COUNT: %d", maxPointCount);
}


///////////////////////////////////////////////////////////////////////////////
//  FindBlock
///////////////////////////////////////////////////////////////////////////////

void 			CDistantLights::FindBlock			(float posX, float posY, s32& blockX, s32& blockY)
{
	vfxAssertf(posX>=WORLDLIMITS_REP_XMIN && posX<=WORLDLIMITS_REP_XMAX, "CDistantLights::FindBlock - posX out of range");
	vfxAssertf(posY>=WORLDLIMITS_REP_YMIN && posY<=WORLDLIMITS_REP_YMAX, "CDistantLights::FindBlock - posY out of range");

	blockX = (s32)((posX-WORLDLIMITS_REP_XMIN)*DISTANTLIGHTS_MAX_BLOCKS/(WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN));
	blockY = (s32)((posY-WORLDLIMITS_REP_YMIN)*DISTANTLIGHTS_MAX_BLOCKS/(WORLDLIMITS_REP_YMAX-WORLDLIMITS_REP_YMIN));

	vfxAssertf(blockX>=0 && blockX<DISTANTLIGHTS_MAX_BLOCKS, "CDistantLights::FindBlock - blockX out of range");
	vfxAssertf(blockY>=0 && blockY<DISTANTLIGHTS_MAX_BLOCKS, "CDistantLights::FindBlock - blockX out of range");
}

#if __BANK
// Starting with a link and a node, look for an "outgoing" link that is adjacent to the same node that has 
// similar properties to the link (potentially considering the outgoing link's next node too)
OutgoingLink CDistantLights::FindNextCompatibleLink(s32 linkIdx, s32 nodeIdx, CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks)
{
	CTempLink& link = pTempLinks[linkIdx];
	CTempNode& node = pTempNodes[nodeIdx];

	atFixedArray<OutgoingLink, 32> outgoingLinks;

	for(s32 l = 0; l < numTempLinks; l++)
	{
		CTempLink& adjLink = pTempLinks[l];
		if (adjLink.m_bProcessed)
		{
			continue;
		}
		if (adjLink.m_Node1 == nodeIdx)
		{
			if (l == linkIdx) // don't add self
			{
				continue;
			}

			OutgoingLink out(l, adjLink.m_Node2);
			outgoingLinks.Push(out);
		}
		else if (adjLink.m_Node2 == nodeIdx)
		{
			if (l == linkIdx) // don't add self
			{
				continue;
			}
			OutgoingLink out(l, adjLink.m_Node1);
			outgoingLinks.Push(out);
		}
	}

	s32 bestMatch = -1;
	float bestScore = -FLT_MAX;

	int totalLanes = link.m_Lanes1To2 + link.m_Lanes2To1;

	int prevNodeIdx = link.m_Node1 == nodeIdx ? link.m_Node2 : link.m_Node1;
	Vector3 prevToCurr = (node.m_Coors - pTempNodes[prevNodeIdx].m_Coors);
	prevToCurr.Normalize();

	// Go through the candidate links, look for a match
	for(int i = 0; i < outgoingLinks.GetCount(); i++)
	{
		CTempLink& outLink = pTempLinks[outgoingLinks[i].m_linkIdx];
		CTempNode& outNode = pTempNodes[outgoingLinks[i].m_nodeIdx];

		if (node.m_Density != outNode.m_Density)
		{
			continue;
		}
		if (node.m_Speed != outNode.m_Speed)
		{
			continue;
		}
		if (totalLanes != (outLink.m_Lanes1To2 + outLink.m_Lanes2To1))
		{
			continue;
		}

		// Try to keep the road from turning through intersections, so the score is the dot product between the incoming and outgoing links
		Vector3 currToNext =(outNode.m_Coors - node.m_Coors);
		currToNext.Normalize();

		float score = currToNext.Dot(prevToCurr);
		if (score > 0.0f && score > bestScore)
		{
			bestScore = score;
			bestMatch = i;
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
#endif // __BANK

///////////////////////////////////////////////////////////////////////////////
//  BuildData
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CDistantLights::BuildData			(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks)
{

	// Clear absolutely everything out!
	m_numPoints = 0;
	m_numGroups = 0;
	sysMemSet(m_points, 0, sizeof(DistantLightPoint_t) * DISTANTLIGHTS_MAX_POINTS);
	sysMemSet(m_groups, 0, sizeof(DistantLightGroup_t) * DISTANTLIGHTS_MAX_GROUPS);
	sysMemSet(m_blockFirstGroup, 0, sizeof(s32) * (DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS));
	sysMemSet(m_blockNumCoastalGroups, 0, sizeof(s32) * (DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS));
	sysMemSet(m_blockNumInlandGroups, 0, sizeof(s32) * (DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS));

	for (s32 l=0; l<numTempLinks; l++)
	{
		pTempLinks[l].m_bProcessed = false;

		if (pTempLinks[l].m_Lanes1To2 + pTempLinks[l].m_Lanes2To1 < 2)
		{
			pTempLinks[l].m_bProcessed = true;
		}

		// if one of the points is below the water we ignore it - e.g. in a tunnel
		s32 node1 = pTempLinks[l].m_Node1;
		s32 node2 = pTempLinks[l].m_Node2;
		if (pTempNodes[node1].m_Coors.z < 0.0f || pTempNodes[node2].m_Coors.z < 0.0f)
		{
			pTempLinks[l].m_bProcessed = true;
		}

		// HACK - also ignore points too far south (i.e. the prologue road network)
		if (pTempNodes[node1].m_Coors.y < -3500.0f || pTempNodes[node2].m_Coors.y < -3500.0f)
		{
			pTempLinks[l].m_bProcessed = true;
		}

		if (pTempNodes[node1].IsPedNode() || pTempNodes[node2].IsPedNode() )
		{
			pTempLinks[l].m_bProcessed = true;
		}

		if (pTempNodes[node1].m_bWaterNode || pTempNodes[node2].m_bWaterNode)
		{
			pTempLinks[l].m_bProcessed = true;
		}

		if (pTempNodes[node1].m_bSwitchedOff || pTempNodes[node2].m_bSwitchedOff)
		{
			pTempLinks[l].m_bProcessed = true;
		}

	}

	for (s32 link=0; link<numTempLinks; link++)
	{
		if (!pTempLinks[link].m_bProcessed)
		{
			s32 node1 = pTempLinks[link].m_Node1;

			// Find the first link in this group, by walking in the node2->node1 direction.
			// Then accumulate links along the chain from that first node

			OutgoingLink origLink(link, node1);
			OutgoingLink currLink(link, node1);
			OutgoingLink nextLink(link, node1);
			int loopCheck = 0;
			while(nextLink.IsValid())
			{
				currLink = nextLink;
				nextLink = FindNextCompatibleLink(currLink.m_linkIdx, currLink.m_nodeIdx, pTempNodes, pTempLinks, numTempLinks);
				if (loopCheck++ == 100)
				{
					// probably a loop, definitely a long enough chain to start processing, so start from here.
					break;
				}
			}

			// currLink is now the start of our chain
			CTempLink& startLink = pTempLinks[currLink.m_linkIdx];
			s32 startNodeIdx = currLink.m_nodeIdx;
			s32 nextNodeIdx = startNodeIdx == startLink.m_Node1 ? startLink.m_Node2 : startLink.m_Node1;

			startLink.m_bProcessed = true;

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
//			m_groups[m_numGroups].twoWay = startLink.m_bGpsCanGoBothWays;


			u8 maxLanes1To2 = startLink.m_Lanes1To2;
			u8 maxLanes2To1 = startLink.m_Lanes2To1;

			// Point our link object in the right direction for iteration
			currLink.m_nodeIdx = nextNodeIdx;

			while ( (!bDoneWithGroup) && (nextLink = FindNextCompatibleLink(currLink.m_linkIdx, currLink.m_nodeIdx, pTempNodes, pTempLinks, numTempLinks)).IsValid())
			{
				CTempLink& nextTempLink = pTempLinks[nextLink.m_linkIdx];
				maxLanes1To2 = Max(maxLanes1To2, nextTempLink.m_Lanes1To2);
				maxLanes2To1 = Max(maxLanes2To1, nextTempLink.m_Lanes2To1);

				// Calculate the distance from the previous node to this new one.
				float dist = Mag(Vec3V((float)m_points[m_numPoints].x, (float)m_points[m_numPoints].y, (float)m_points[m_numPoints].z) - RCC_VEC3V(pTempNodes[nextLink.m_nodeIdx].m_Coors)).Getf();

				if (dist>ms_DisnantLightsBuildNodeRage)
				{			// The new found node is too far from our start node. Add a new node to the list.
					if (m_numPoints - m_groups[m_numGroups].pointOffset <= 8)		// Make sure groups aren't too long.
					{
						m_numPoints++;
					}
					else
					{
						bDoneWithGroup = true;
					}
				}
				else
				{			// Still close enough. Simply replace the old node.
				}

				m_points[m_numPoints+1].x = (s16)pTempNodes[nextLink.m_nodeIdx].m_Coors.x;
				m_points[m_numPoints+1].y = (s16)pTempNodes[nextLink.m_nodeIdx].m_Coors.y;
				m_points[m_numPoints+1].z = (s16)pTempNodes[nextLink.m_nodeIdx].m_Coors.z;

				pTempLinks[nextLink.m_linkIdx].m_bProcessed = true;
				currLink = nextLink;
			}
			m_numPoints+=2;

			m_groups[m_numGroups].twoWay = (maxLanes1To2 > 0 && maxLanes2To1 > 0);

			// Now we decide whether this group is long enough to be worth keeping.
			m_groups[m_numGroups].pointCount = (s16)(m_numPoints - m_groups[m_numGroups].pointOffset);
			if (m_groups[m_numGroups].pointCount <= 4)		// Need at least a number of points in group to keep it.
			{
				m_numPoints = m_groups[m_numGroups].pointOffset;		// Set number of point back to what it was
			}
			else
			{			// We're keeping this group. Calculate centre point and radius for sprite on-screen check
				Vec3V vCentrePoint = Vec3V(V_ZERO);

				for (s32 p=m_groups[m_numGroups].pointOffset; p<m_groups[m_numGroups].pointOffset+m_groups[m_numGroups].pointCount; p++)
				{
					vCentrePoint += Vec3V((float)m_points[p].x, (float)m_points[p].y, (float)m_points[p].z);
				}
				vCentrePoint /= ScalarVFromF32(m_groups[m_numGroups].pointCount);
				ScalarV radiusSqr(V_ZERO);
				for (s32 p=m_groups[m_numGroups].pointOffset; p<m_groups[m_numGroups].pointOffset+m_groups[m_numGroups].pointCount; p++)
				{
					ScalarV	distFromCentreSqr = MagSquared(Vec3V((float)m_points[p].x, (float)m_points[p].y, (float)m_points[p].z) - vCentrePoint);
					radiusSqr = Max(radiusSqr, distFromCentreSqr);
				}

				m_groups[m_numGroups].centreX = (s16)vCentrePoint.GetXf();
				m_groups[m_numGroups].centreY = (s16)vCentrePoint.GetYf();
				m_groups[m_numGroups].centreZ = (s16)vCentrePoint.GetZf();

				m_groups[m_numGroups].radius = (s16)sqrtf(radiusSqr.Getf());

				// Also check to see if this group should use streetlights
				// Right now the heuristic is density < 2 => no streetlights
				m_groups[m_numGroups].useStreetlights = m_groups[m_numGroups].density > 2;

				m_numGroups++;
			}

			const s32 iMaxGroups = DISTANTLIGHTS_MAX_GROUPS-1;
			const s32 iMaxPoints = DISTANTLIGHTS_MAX_POINTS-20;

			vfxAssertf(m_numGroups < iMaxGroups, "CDistantLights::BuildData - too many groups found (%i / %i)", m_numGroups, iMaxGroups);
			vfxAssertf(m_numPoints < iMaxPoints, "CDistantLights::BuildData - too many points found (%i / %i)", m_numPoints, iMaxPoints);

			if(m_numGroups >= iMaxGroups)
			{
				Assertf(false, "CDistantLights::BuildData - I'm gonna stop building lights now, the game won't crash - but the lights data will be fucked.");
				break;
			}
			if(m_numPoints >= iMaxPoints)
			{
				Assertf(false, "CDistantLights::BuildData - I'm gonna stop building lights now, the game won't crash - but the lights data will be fucked.");
				break;
			}
		}
	}


	s32 sortedGroups = 0;
	for (s32 blockX=0; blockX<DISTANTLIGHTS_MAX_BLOCKS; blockX++)
	{
		for (s32 blockY=0; blockY<DISTANTLIGHTS_MAX_BLOCKS; blockY++)
		{
			s32 totalNumGroupsForBlock = 0;
			//				m_blockNumCoastalGroups[blockX][blockY] = 0;
			m_blockFirstGroup[blockX][blockY] = sortedGroups;

			// Go through the groups and collect the ones that fall into this block.
			for (s32 s=sortedGroups; s<m_numGroups; s++)
			{
				s32 groupBlockX, groupBlockY;
				FindBlock(m_groups[s].centreX, m_groups[s].centreY, groupBlockX, groupBlockY);

				if (blockX == groupBlockX && blockY == groupBlockY)
				{
					DistantLightGroup_t tempGroup = m_groups[s];
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
						DistantLightGroup_t tempGroup = m_groups[i+firstGroup];
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
			m_blockNumInlandGroups[blockX][blockY] = totalNumGroupsForBlock - m_blockNumCoastalGroups[blockX][blockY];
		}
	}

	Displayf("Distant Lights:finished. We had %i groups, and %i points.\n", m_numGroups, m_numPoints);

	SaveData();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  SaveData
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CDistantLights::SaveData			()
{
	CFileMgr::SetDir("common:/data/paths/");
	FileHandle fileId = CFileMgr::OpenFileForWriting("distantlights.dat");

	if (CFileMgr::IsValidFileHandle(fileId))
	{

		int bytes = DistantLightSaveData::Write(fileId, (s32*)(&m_numPoints), 1);
		vfxAssertf(bytes==4, "CDistantLights::SaveData - error writing number of points");

		bytes = DistantLightSaveData::Write(fileId, (s32*)(&m_numGroups), 1);
		vfxAssertf(bytes==4, "CDistantLights::SaveData - error writing number of groups");

		bytes = DistantLightSaveData::Write(fileId, (s32*)m_blockFirstGroup, DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS);
		vfxAssertf(bytes==(int)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing block's first group");

		bytes = DistantLightSaveData::Write(fileId, (s32*)m_blockNumCoastalGroups, DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS);
		vfxAssertf(bytes==(int)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing number of block's coastal group");

		bytes = DistantLightSaveData::Write(fileId, (s32*)m_blockNumInlandGroups, DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS);
		vfxAssertf(bytes==(int)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing number of block's inland group");

		bytes = DistantLightSaveData::Write(fileId, (s16*)m_points, (m_numPoints*sizeof(DistantLightPoint_t))/sizeof(s16));
		vfxAssertf(bytes==(int)(m_numPoints*sizeof(DistantLightPoint_t)), "CDistantLights::SaveData - error writing number of block's points");

		bytes = DistantLightSaveData::Write(fileId, m_groups, m_numGroups);
		vfxAssertf(bytes==(int)(m_numGroups*sizeof(DistantLightGroup_t)), "CDistantLights::SaveData - error writing number of block's groups");

/*
		int bytes = CFileMgr::Write(fileId, (char*)(&m_numPoints), 4);
		vfxAssertf(bytes==4, "CDistantLights::SaveData - error writing number of points");

		bytes = CFileMgr::Write(fileId, (char*)(&m_numGroups), 4);
		vfxAssertf(bytes==4, "CDistantLights::SaveData - error writing number of groups");

		bytes = CFileMgr::Write(fileId, (char*)m_blockFirstGroup, DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32));
		vfxAssertf(bytes==(int)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing block's first group");

		bytes = CFileMgr::Write(fileId, (char*)m_blockNumCoastalGroups, DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32));
		vfxAssertf(bytes==(int)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing number of block's coastal group");

		bytes = CFileMgr::Write(fileId, (char*)m_blockNumInlandGroups, DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32));
		vfxAssertf(bytes==(int)(DISTANTLIGHTS_MAX_BLOCKS*DISTANTLIGHTS_MAX_BLOCKS*sizeof(s32)), "CDistantLights::SaveData - error writing number of block's inland group");

		bytes = CFileMgr::Write(fileId, (char*)m_points, m_numPoints*sizeof(DistantLightPoint_t));
		vfxAssertf(bytes==(int)(m_numPoints*sizeof(DistantLightPoint_t)), "CDistantLights::SaveData - error writing number of block's points");

		bytes = CFileMgr::Write(fileId, (char*)m_groups, m_numGroups*sizeof(DistantLightGroup_t));
		vfxAssertf(bytes==(int)(m_numGroups*sizeof(DistantLightGroup_t)), "CDistantLights::SaveData - error writing number of block's groups");
*/

		CFileMgr::CloseFile(fileId);
	}

	CFileMgr::SetDir("");
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  FindNextNode
///////////////////////////////////////////////////////////////////////////////

#if __BANK
s32			CDistantLights::FindNextNode		(CTempLink* pTempLinks, s32 searchStart, s32 node, s32 numTempLinks)
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
#endif


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CDistantLights::InitWidgets	()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Distant Lights", false);
	{
		pVfxBank->AddToggle("Disable Rendering", &m_bDisableRendering);
		pVfxBank->AddToggle("Disable Vehicle Lights Rendering", &m_disableVehicleLights);
		pVfxBank->AddToggle("Disable Street Lights Rendering", &m_disableStreetLights); // TODO -- remove this?
		pVfxBank->AddToggle("Disable Street Lights Visibility", &m_disableStreetLightsVisibility);
		pVfxBank->AddToggle("Disable Depth Test", &m_disableDepthTest);
#if ENABLE_DISTANT_CARS
		pVfxBank->AddToggle("Disable Distant Cars", &m_disableDistantCars);
		pVfxBank->AddToggle("Disable Distant ", &m_disableDistantLights);
#endif

		pVfxBank->AddTitle("INFO");
		pVfxBank->AddText("Num Blocks Rendered", &m_numBlocksRendered, true);
		pVfxBank->AddText("Num Blocks Rendered in Water Reflection", &m_numBlocksWaterReflectionRendered, true);
		pVfxBank->AddText("Num Blocks Rendered in Mirror Reflection", &m_numBlocksMirrorReflectionRendered, true);
		
		pVfxBank->AddText("Num Groups Rendered", &m_numGroupsRendered, true);
		pVfxBank->AddText("Num Groups Rendered in Water Reflection", &m_numGroupsWaterReflectionRendered, true);
		pVfxBank->AddText("Num Groups Rendered in Mirror Reflection", &m_numGroupsMirrorReflectionRendered, true);
		pVfxBank->AddText("Num Lights Rendered", &m_numLightsRendered, true);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
		pVfxBank->AddSlider("Build - Node Range", &ms_DisnantLightsBuildNodeRage, 0.0f, 500.0f, 5.0f);

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

		pVfxBank->AddSlider("Speed=0 speed", &g_distLightsParam.speed0Speed, 0.01f, 100.0f, 0.01f);
		pVfxBank->AddSlider("Speed=3 speed", &g_distLightsParam.speed3Speed, 0.01f, 100.0f, 0.01f);

		pVfxBank->AddSlider("Random speed (SP)", &g_distLightsParam.randomizeSpeedSP, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Random spacing (SP)", &g_distLightsParam.randomizeSpacingSP, 0.0f, 1.0f, 0.01f);

		pVfxBank->AddSlider("Random speed (MP)", &g_distLightsParam.randomizeSpeedMP, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Random spacing (MP)", &g_distLightsParam.randomizeSpacingMP, 0.0f, 1.0f, 0.01f);

		pVfxBank->AddSeparator();

		pVfxBank->AddSlider("Car Light Hour Start", &g_distLightsParam.hourStart, 0, 24, 1);
		pVfxBank->AddSlider("Car Light Hour End", &g_distLightsParam.hourEnd, 0, 24, 1);
		pVfxBank->AddSlider("Car Light HDR Intensity", &g_distLightsParam.carLightHdrIntensity, 0.0f, 255.0f, 0.5f);

		pVfxBank->AddSlider("Car Light Near Fade", &g_distLightsParam.carLightNearFadeDist, 0.0f, 1024.0f, 0.1f);
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

		pVfxBank->AddSeparator();

		pVfxBank->AddSlider("Street Light Hour Start", &g_distLightsParam.streetLightHourStart, 0, 24, 1);
		pVfxBank->AddSlider("Street Light Hour End", &g_distLightsParam.streetLightHourEnd, 0, 24, 1);

		pVfxBank->AddSlider("Street Light Z Offset", &g_distLightsParam.streetLightZOffset, 0.0f, 20.0f, 1.0f);
		pVfxBank->AddSlider("Street Light Spacing", &sm_streetLight.spacingSP, 1.0f, 200.0f, 1.0f);
		pVfxBank->AddColor("Street Light Color", (float*)&sm_streetLight.vColor, 3);
		pVfxBank->AddSlider("Street Light HDR Intensity", &g_distLightsParam.streetLightHdrIntensity, 0.0f, 255.0f, 0.5f);

#if ENABLE_DISTANT_CARS
		pVfxBank->AddSeparator();

		pVfxBank->AddSlider("Distant Car Near Clip", &g_distLightsParam.distantCarsNearDist, 0.0f, 2000.0f, 50.0f);
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


#endif

#endif // !USE_DISTANT_LIGHTS_2 || RSG_DEV
