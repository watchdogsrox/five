// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "grcore/allocscope.h"
#if __XENON
	#include "grcore/wrapper_d3d.h"
#endif

// framework headers
#include "system/dependencyscheduler.h"

// game headers
#include "Renderer/Lights/LODLights.h"
#include "Renderer/Deferred/DeferredLighting.h"
#include "Renderer/ProcessLodLightVisibility.h"
#include "Renderer/sprite2d.h"
#include "Renderer/Water.h"
#include "Renderer/RenderPhases/RenderPhase.h"
#include "Renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "debug/rendering/DebugLighting.h"
#include "debug/rendering/DebugLights.h"
#include "vfx/misc/Coronas.h"
#include "vfx/misc/DistantLights.h"
#include "vfx/misc/DistantLightsVertexBuffer.h"
#include "vfx/misc/LODLightManager.h"
#include "camera/viewports/ViewportManager.h"
#include "timecycle/TimeCycle.h"
#include "timecycle/TimeCycleConfig.h"
#include "fwmaths/Vector.h"
#include "fwscene/scan/Scan.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "Renderer/Renderer.h"
#include "renderer/Occlusion.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Util/Util.h"
#include "renderer/render_channel.h"
#include "spatialdata/transposedplaneset.h"
#include "scene/portals/PortalTracker.h"
#include "scene/lod/LodMgr.h"

RENDER_OPTIMISATIONS()
// =============================================================================================== //
// DEFINES
// =============================================================================================== //

#define MAX_INSTANCED_POINT_LIGHTS 512
#define MAX_INSTANCED_SPOT_LIGHTS 6144
#define MAX_INSTANCED_CAPSULE_LIGHTS 384

extern distLightsParam g_distLightsParam;
bool CLODLights::m_lodLightsEnabled = true;
int CLODLights::m_bufferIndex = 0;

u32 CLODLights::m_maxVertexBuffers = 0;
grcVertexBuffer* CLODLights::m_pointInstanceVB[MaxInstBufferCount] = { NULL };
grcVertexBuffer* CLODLights::m_spotInstanceVB[MaxInstBufferCount] = { NULL };
grcVertexBuffer* CLODLights::m_capsuleInstanceVB[MaxInstBufferCount] = { NULL };

bool CLODLights::m_vertexBuffersInitalised = false;

u32 CLODLights::m_currentPointVBIndex = 0;
u32 CLODLights::m_currentSpotVBIndex = 0;
u32 CLODLights::m_currentCapsuleVBIndex = 0;

grcVertexDeclaration* CLODLights::m_pointVtxDecl = NULL;
grcVertexDeclaration* CLODLights::m_spotVtxDecl = NULL;
grcVertexDeclaration* CLODLights::m_capsuleVtxDecl = NULL;

float CLODLights::m_lodLightRange[LODLIGHT_CATEGORY_COUNT] = { 150, 450.0f, 1250.0f };
float CLODLights::m_lodLightFade[LODLIGHT_CATEGORY_COUNT] = { 50.0f, 50.0f, 50.0f };
float CLODLights::m_lodLightCoronaRange[LODLIGHT_CATEGORY_COUNT] = { 150.0f, 450.0f, 1250.0f };
float CLODLights::m_lodLightScaleMultiplier = 0.5;
float CLODLights::m_lodLightCoronaIntensityMult = 1.0f;
float CLODLights::m_lodLightCoronaSize = 1.0f;
float CLODLights::m_lodLightSphereExpandSizeMult = 1.0f;
float CLODLights::m_lodLightSphereExpandStart = 1000.0f;
float CLODLights::m_lodLightSphereExpandEnd  = 1100.0f;

#if __BANK
	u32 CLODLights::m_lodLightCount = 0U;
	u32 CLODLights::m_lodLightCoronaCount = 0;
#endif

#if __ASSERT
	u32 CLODLights::m_instancedOverflow[3] = {0, 0, 0};
#endif
	

LodLightVisibility		g_LodLightVisibility[2][MAX_VISIBLE_LOD_LIGHTS] ;
u32						g_LightsCount[2];
LodLightVisibilityData	g_LodLightVisibilityData ;
sysDependency			g_ProcessLodLightVisibilityDependency;
u32						g_ProcessLodLightVisibilityDependencyRunning;

#if __DEV
u32						g_LodLightsOcclusionTrivialAcceptActiveFrustum[2];
u32						g_LodLightsOcclusionTrivialAcceptActiveMinZ[2];
u32						g_LodLightsOcclusionTrivialAcceptVisiblePixel[2];
#endif

#if __PPU
DECLARE_FRAG_INTERFACE(ProcessLodLightVisibility);
#endif

PARAM(nolodlights, "disable lod lights");

struct PointLightInstanceData
{
	float posX, posY, posZ, radius;
	float intensity, falloffExponent;
	u32 colour;
};

struct SpotLightInstanceData
{
	float posX, posY, posZ, radius;
	float dirX, dirY, dirZ, falloffExponent;
	float spotCosOuterAngle, spotSinOuterAngle, spotConeScale, intensity;
	u32 colour;
};

struct CapsuleLightInstanceData
{
	float posX, posY, posZ, radius;
	float dirX, dirY, dirZ, falloffExponent;
	float capsuleExtents, intensity;
	u32 colour;
};

// =============================================================================================== //
// HELPER FUNCTIONS
// =============================================================================================== //

float CalcLightIntensity(CLODLight *pLODLight, u32 idx)
{
	packedTimeAndStateFlags &flags = (packedTimeAndStateFlags&)(pLODLight->m_timeAndStateFlags[idx]);
	const u32 timeFlags = flags.timeFlags;

	float intensity = 0.0f;

	//	const u8 flashiness = 0;
	u32 hour = CClock::GetHour();
	u32 nextHour = (CClock::GetHour() + 1) % 24;
	float fade = (float)CClock::GetMinute() + ((float)CClock::GetSecond() / 60.0f);

	// If light is on, make sure its on
	if (timeFlags & (1 << hour))
	{
		intensity = 1.0;
	}
	
	// Light is off now but will turn on in the next hour
	if ((timeFlags & (1 << (nextHour))) != 0 &&
		(timeFlags & (1 << hour)) == 0)
	{
		// Fade up in the last 5 minutes
		intensity = Saturate(fade - 59.0f);
	}
	
	// Light is on but will switch off in the next hour
	if ((timeFlags & (1 << hour)) != 0 &&
		(timeFlags & (1 << (nextHour))) == 0)
	{
		// Fade down in the last 5 minutes
		intensity = 1.0f - Saturate(fade - 59.0f);
	}

	return intensity;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline float CreateLightSource(const CLODLight *pLightData, const CDistantLODLight *pBaseData, const u32 idx, CLightSource &lightSource, float falloffScale)
{
	const packedTimeAndStateFlags& flags = (packedTimeAndStateFlags&)(pLightData->m_timeAndStateFlags[idx]);
	const u32 timeFlags = flags.timeFlags;

	// See if light is on
	const float timeFade = Lights::CalculateTimeFade(timeFlags);

	if (timeFade > 0.0f)
	{
		Color32 colour;
		colour.Set(pBaseData->m_RGBI[idx]);
		Vector3 lightColour = Vector3(colour.GetRedf(), colour.GetGreenf(), colour.GetBluef());

		float intensity = (colour.GetAlpha() * MAX_LODLIGHT_INTENSITY / 255.0f) * timeFade;
		
		Vector3	ldir(pLightData->m_direction[idx].x, pLightData->m_direction[idx].y, pLightData->m_direction[idx].z);
		Vector3 ltan;
		ltan.Cross(ldir, FindMinAbsAxis(ldir));
		ltan.Normalize();

		Vector3 position(pBaseData->m_position[idx].x, pBaseData->m_position[idx].y, pBaseData->m_position[idx].z);

		lightSource.SetCommon((eLightType)flags.lightType, 0, position, lightColour, intensity, timeFlags);

		lightSource.SetRadius(pLightData->m_falloff[idx] * falloffScale);
		lightSource.SetFalloffExponent(pLightData->m_falloffExponent[idx]);
		lightSource.SetDirTangent(ldir, ltan);

		if (lightSource.GetType() == LIGHT_TYPE_SPOT)
		{
			lightSource.SetSpotlight(	LL_UNPACK_U8(pLightData->m_coneInnerAngle[idx], MAX_LODLIGHT_CONE_ANGLE ), 
										LL_UNPACK_U8(pLightData->m_coneOuterAngleOrCapExt[idx], MAX_LODLIGHT_CONE_ANGLE) );
		}
		else if (lightSource.GetType() == LIGHT_TYPE_CAPSULE)
		{
			float capsuleExtent = LL_UNPACK_U8(pLightData->m_coneOuterAngleOrCapExt[idx], MAX_LODLIGHT_CAPSULE_EXTENT);
			Assertf(capsuleExtent > 0.0f, "Light Capsule extent size is 0.0 in light index: %d, position: %.4f %.4f %.4f", idx, position.x, position.y, position.z);			
			lightSource.SetCapsule(capsuleExtent);
		}

		lightSource.SetFlag(LIGHTFLAG_NO_SPECULAR);

		lightSource.ClearFlag(LIGHTFLAG_EXTERIOR_ONLY);
		lightSource.ClearFlag(LIGHTFLAG_INTERIOR_ONLY);

		return intensity;
	}

	return timeFade;
}

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

grcVertexBuffer* CreateInstanceVB(u32 instanceCount, grcFvf fvf)
{
#if (__D3D11 && RSG_PC) || RSG_ORBIS
	return grcVertexBuffer::Create(instanceCount, fvf, rage::grcsBufferCreate_DynamicWrite, rage::grcsBufferSync_None, NULL);
#else
	const bool bReadWrite = true;
	const bool bDynamic = RSG_DURANGO ? false : true;
	return grcVertexBuffer::Create(instanceCount, fvf, bReadWrite, bDynamic, NULL);
#endif
}

void CLODLights::InitInstancedVertexBuffers()
{
	m_maxVertexBuffers = 3;
	#if RSG_PC
		m_maxVertexBuffers += GRCDEVICE.GetGPUCount();
	#endif // RSG_PC

	// Instance Point Light Setup
	grcFvf fvfPointInstance;		
	fvfPointInstance.SetPosChannel(false);
	fvfPointInstance.SetTextureChannel(0, true, grcFvf::grcdsFloat4);
	fvfPointInstance.SetTextureChannel(1, true, grcFvf::grcdsFloat2);
	fvfPointInstance.SetDiffuseChannel(true, grcFvf::grcdsColor);

	for(u32 i = 0; i < m_maxVertexBuffers; i++)
	{
		m_pointInstanceVB[i] = CreateInstanceVB(MAX_INSTANCED_POINT_LIGHTS, fvfPointInstance);
	}

	const grcVertexElement pointElements[] =
	{
		grcVertexElement(0, grcVertexElement::grcvetPosition, 0, grcFvf::GetDataSizeFromType(grcFvf::grcdsHalf4),  grcFvf::grcdsHalf4),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  1, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat2), grcFvf::grcdsFloat2,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetColor,    0, grcFvf::GetDataSizeFromType(grcFvf::grcdsColor),  grcFvf::grcdsColor,   grcFvf::grcsfmIsInstanceStream, 1),
	};
	m_pointVtxDecl = GRCDEVICE.CreateVertexDeclaration(pointElements, NELEM(pointElements));
	m_currentPointVBIndex = 0;

	// Instance Spot Light Setup
	grcFvf fvfSpotInstance;		
	fvfSpotInstance.SetPosChannel(false);
	fvfSpotInstance.SetTextureChannel(0, true, grcFvf::grcdsFloat4);
	fvfSpotInstance.SetTextureChannel(1, true, grcFvf::grcdsFloat4);
	fvfSpotInstance.SetTextureChannel(2, true, grcFvf::grcdsFloat4);
	fvfSpotInstance.SetDiffuseChannel(true, grcFvf::grcdsColor);

	for(u32 i = 0; i < m_maxVertexBuffers; i++)
	{
		m_spotInstanceVB[i] = CreateInstanceVB(MAX_INSTANCED_SPOT_LIGHTS, fvfSpotInstance);
	}

	const grcVertexElement spotElements[] =
	{
		grcVertexElement(0, grcVertexElement::grcvetPosition, 0, grcFvf::GetDataSizeFromType(grcFvf::grcdsHalf4),  grcFvf::grcdsHalf4),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  1, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  2, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetColor,    0, grcFvf::GetDataSizeFromType(grcFvf::grcdsColor),  grcFvf::grcdsColor,   grcFvf::grcsfmIsInstanceStream, 1),
	};
	m_spotVtxDecl = GRCDEVICE.CreateVertexDeclaration(spotElements, NELEM(spotElements));
	m_currentSpotVBIndex = 0;

	// Instance Capsule Light Setup
	grcFvf fvfCapsuleInstance;		
	fvfCapsuleInstance.SetPosChannel(false);
	fvfCapsuleInstance.SetTextureChannel(0, true, grcFvf::grcdsFloat4);
	fvfCapsuleInstance.SetTextureChannel(1, true, grcFvf::grcdsFloat4);
	fvfCapsuleInstance.SetTextureChannel(2, true, grcFvf::grcdsFloat2);
	fvfCapsuleInstance.SetDiffuseChannel(true, grcFvf::grcdsColor);

	for(u32 i = 0; i < m_maxVertexBuffers; i++)
	{
		m_capsuleInstanceVB[i] = CreateInstanceVB(MAX_INSTANCED_CAPSULE_LIGHTS, fvfCapsuleInstance);
	}

	const grcVertexElement capsuleElements[] =
	{
		grcVertexElement(0, grcVertexElement::grcvetPosition, 0, grcFvf::GetDataSizeFromType(grcFvf::grcdsHalf4),  grcFvf::grcdsHalf4),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  0, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  1, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat4), grcFvf::grcdsFloat4,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetTexture,  2, grcFvf::GetDataSizeFromType(grcFvf::grcdsFloat2), grcFvf::grcdsFloat2,  grcFvf::grcsfmIsInstanceStream, 1),
		grcVertexElement(1, grcVertexElement::grcvetColor,    0, grcFvf::GetDataSizeFromType(grcFvf::grcdsColor),  grcFvf::grcdsColor,   grcFvf::grcsfmIsInstanceStream, 1),
	};
	m_capsuleVtxDecl = GRCDEVICE.CreateVertexDeclaration(capsuleElements, NELEM(capsuleElements));
	m_currentCapsuleVBIndex = 0;
}

void CLODLights::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		m_lodLightsEnabled = !PARAM_nolodlights.Get();
		m_bufferIndex = 0;
#if __PPU
		g_ProcessLodLightVisibilityDependency.Init( FRAG_SPU_CODE(ProcessLodLightVisibility), 0, 0 );
#else
		g_ProcessLodLightVisibilityDependency.Init( ProcessLodLightVisibility::RunFromDependency, 0, 0 );
#endif

		if (!m_vertexBuffersInitalised)
		{
			InitInstancedVertexBuffers();
			m_vertexBuffersInitalised = true;
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		if (m_pointVtxDecl)
		{
			GRCDEVICE.DestroyVertexDeclaration(m_pointVtxDecl);
			m_pointVtxDecl = NULL;
		}

		if (m_spotVtxDecl)
		{
			GRCDEVICE.DestroyVertexDeclaration(m_spotVtxDecl);
			m_spotVtxDecl = NULL;
		}

		if (m_capsuleVtxDecl)
		{
			GRCDEVICE.DestroyVertexDeclaration(m_capsuleVtxDecl);
			m_capsuleVtxDecl = NULL;
		}
		
		for (u32 j = 0; j < m_maxVertexBuffers; j++ )
		{
			SAFE_DELETE(m_pointInstanceVB[j]);
			SAFE_DELETE(m_spotInstanceVB[j]);
			SAFE_DELETE(m_capsuleInstanceVB[j]);
		}

		m_bufferIndex = 0;
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::Update()
{

}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::Render()
{

}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::RenderLODLights()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	#if __BANK
		m_lodLightCount = 0;
	#endif

	Assertf(m_vertexBuffersInitalised, "Please call InitInstancedVertexBuffers before RenderLODLights");
	if (!Enabled())
		return; 

	if(!CLODLightManager::m_currentFrameInfo)
		return;

	if(CLODLightManager::GetRenderLODLightsDisabledThisFrame())
	{
		return;
	}

	if(!CLODLightManager::GetIfCanLoad())
		return;

	#if	__BANK
		if(!DebugLighting::m_LODLightRender)
			return;
	#endif	//__BANK

	#if DEPTH_BOUNDS_SUPPORT
		grcDevice::SetDepthBoundsTestEnable(false);
	#endif //DEPTH_BOUNDS_SUPPORT

	DeferredLighting::SetShaderGBufferTargets();
	grcViewport::SetCurrentWorldIdentity();

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
	const Vec3V vCameraPos = grcVP->GetCameraPosition();

	const atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &LODLightsToRender = CLODLightManager::GetLODLightsToRenderRenderBuffer();

	LodLightVisibility* pLodLightsVisibility = CLODLights::GetLODLightVisibilityRenderBuffer();
	u16 lodLightVisibilityCount = (u16)CLODLights::GetLODLightVisibilityCountRenderBuffer();

	float radiusScale = 1.0f;
	#if __BANK
	if( DebugLights::m_EnableLightFalloffScaling )
	#endif	
	{
		radiusScale *= g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_LIGHT_FALLOFF_MULT);
	}

	Verifyf(m_pointInstanceVB[m_currentPointVBIndex]->Lock(rage::grcsNoOverwrite), "Couldn't lock point vertex buffer for instance lod light rendering");
	PointLightInstanceData* pointDst = reinterpret_cast<PointLightInstanceData*>(m_pointInstanceVB[m_currentPointVBIndex]->GetLockPtr());
	u32 numPointLightsToDraw = 0;

	Verifyf(m_spotInstanceVB[m_currentSpotVBIndex]->Lock(rage::grcsNoOverwrite), "Couldn't lock spot vertex buffer for instance lod light rendering");
	SpotLightInstanceData* spotDst = reinterpret_cast<SpotLightInstanceData*>(m_spotInstanceVB[m_currentSpotVBIndex]->GetLockPtr());
	u32 numSpotLightsToDraw = 0;

	Verifyf(m_capsuleInstanceVB[m_currentCapsuleVBIndex]->Lock(rage::grcsNoOverwrite), "Couldn't lock capsule vertex buffer for instance lod light rendering");
	CapsuleLightInstanceData* capsuleDst = reinterpret_cast<CapsuleLightInstanceData*>(m_capsuleInstanceVB[m_currentCapsuleVBIndex]->GetLockPtr());
	u32 numCapsuleLightsToDraw = 0;

	for(int idx = 0; idx < lodLightVisibilityCount; idx++)
	{
		LodLightVisibility &currentVisibleLightInfo = pLodLightsVisibility[idx];
		const u16 lightIndex = currentVisibleLightInfo.lightIndex;
		const u16 bucketIndex = currentVisibleLightInfo.bucketIndex;

		if(bucketIndex >= LODLightsToRender.GetCount())
		{
			continue;
		}

		CLODLightBucket* pBucket = LODLightsToRender[bucketIndex];
		if(pBucket == NULL)
		{
			continue;
		}

		const CLODLight* pLightData = pBucket->pLights;
		const CDistantLODLight* pBaseData = pBucket->pDistLights;
		if(pLightData == NULL || pBaseData == NULL || lightIndex >= pBucket->lightCount)
		{
			Assertf(false, "Hit bad LOD light data (%p, %p, %p, %d, %d)", pBaseData, pLightData, pBucket, pBucket->lightCount, lightIndex);
			continue;
		}

		const eLodLightCategory currentCategory = (eLodLightCategory)pBaseData->m_category;
		const float lodLightRange = GetRenderLodLightRange(currentCategory);

		const Vec4V vSphere = Vec4V(
			pBaseData->m_position[lightIndex].x, 
			pBaseData->m_position[lightIndex].y, 
			pBaseData->m_position[lightIndex].z, 
			pLightData->m_falloff[lightIndex]);

#if __BANK
		// Fast check against occlusion system
		spdAABB bound;
		bound.SetAsSphereV4(vSphere);

		if (DebugLighting::m_LODLightsEnableOcclusionTest && !DebugLighting::m_LODLightsEnableOcclusionTestInMainThread && COcclusion::IsBoxVisibleFast(bound) == false)
		{
			continue;
		}
#endif
		// See if light is on
		CLightSource lightSource;
		const float intensity = CreateLightSource(pLightData, pBaseData, lightIndex, lightSource, radiusScale);
		BANK_ONLY(DebugLighting::OverrideLODLight(lightSource, bucketIndex, currentCategory));

		if (intensity > 0.0f)
		{
			ScalarV vDistToCamSqr = MagSquared(Subtract(vSphere.GetXYZ(), vCameraPos));
			const float distToCameraSqr = vDistToCamSqr.Getf();
			float distToCamera = sqrtf(distToCameraSqr);

			// Fade off
			float fade = Saturate((distToCamera - (lodLightRange - m_lodLightFade[currentCategory])) / m_lodLightFade[currentCategory]);
			fade = 1.0f - sqrtf(fade);

			// Fade in (for VIP/large lights)
			if (currentCategory == LODLIGHT_CATEGORY_LARGE)
			{
				const float mediumLightRange = GetLodLightFadeRange(LODLIGHT_CATEGORY_MEDIUM);
				float fadeIn = Saturate((distToCamera - (mediumLightRange - m_lodLightFade[LODLIGHT_CATEGORY_MEDIUM])) / m_lodLightFade[LODLIGHT_CATEGORY_MEDIUM]);
				fade *= (fadeIn * fadeIn);
			}

			lightSource.SetIntensity(intensity * fade);

			if (lightSource.GetType() == LIGHT_TYPE_POINT && numPointLightsToDraw < MAX_INSTANCED_POINT_LIGHTS)
			{
				pointDst->posX = lightSource.GetPosition().x;
				pointDst->posY = lightSource.GetPosition().y;
				pointDst->posZ = lightSource.GetPosition().z;
				pointDst->radius = lightSource.GetRadius();

				pointDst->intensity = lightSource.GetIntensity();
				pointDst->falloffExponent = lightSource.GetFalloffExponent();

				pointDst->colour = Color32(lightSource.GetColor()).GetDeviceColor();

				pointDst++;
				numPointLightsToDraw++;
			}
			else if (lightSource.GetType() == LIGHT_TYPE_SPOT && numSpotLightsToDraw < MAX_INSTANCED_SPOT_LIGHTS)
			{
				const float cosOuterAngle = lightSource.GetConeCosOuterAngle();
				const float sinOuterAngle = sqrtf(1.0f - cosOuterAngle*cosOuterAngle);
				const float scale         = 1.0f / rage::Max((lightSource.GetConeCosInnerAngle() - cosOuterAngle), 0.000001f);

				spotDst->posX = lightSource.GetPosition().x;
				spotDst->posY = lightSource.GetPosition().y;
				spotDst->posZ = lightSource.GetPosition().z;
				spotDst->radius = lightSource.GetRadius();
					
				spotDst->dirX = lightSource.GetDirection().x;
				spotDst->dirY = lightSource.GetDirection().y;
				spotDst->dirZ = lightSource.GetDirection().z;
				spotDst->falloffExponent = lightSource.GetFalloffExponent();

				spotDst->spotCosOuterAngle = cosOuterAngle;
				spotDst->spotSinOuterAngle = sinOuterAngle;
				spotDst->spotConeScale = scale;

				spotDst->intensity = lightSource.GetIntensity();
				spotDst->colour = Color32(lightSource.GetColor()).GetDeviceColor();

				spotDst++;
				numSpotLightsToDraw++;

			}
			else if (lightSource.GetType() ==  LIGHT_TYPE_CAPSULE && numCapsuleLightsToDraw < MAX_INSTANCED_CAPSULE_LIGHTS)
			{
				Vector3 adjustedPos = lightSource.GetPosition() - lightSource.GetDirection() * lightSource.GetCapsuleExtent() * 0.5;

				capsuleDst->posX = adjustedPos.x;
				capsuleDst->posY = adjustedPos.y;
				capsuleDst->posZ = adjustedPos.z;
				capsuleDst->radius = lightSource.GetRadius();

				capsuleDst->dirX = lightSource.GetDirection().x;
				capsuleDst->dirY = lightSource.GetDirection().y;
				capsuleDst->dirZ = lightSource.GetDirection().z;
				capsuleDst->falloffExponent = lightSource.GetFalloffExponent();

				capsuleDst->capsuleExtents = lightSource.GetCapsuleExtent();
				capsuleDst->intensity = lightSource.GetIntensity();

				capsuleDst->colour = Color32(lightSource.GetColor()).GetDeviceColor();
					
				capsuleDst++;
				numCapsuleLightsToDraw++;
			}
			#if __ASSERT
			else
			{
				if (lightSource.GetType() == LIGHT_TYPE_POINT && (numPointLightsToDraw + m_instancedOverflow[0] + 1) >= MAX_INSTANCED_POINT_LIGHTS)
				{
					m_instancedOverflow[0]++;
				}

				if (lightSource.GetType() == LIGHT_TYPE_SPOT && (numSpotLightsToDraw + m_instancedOverflow[1] + 1) >= MAX_INSTANCED_SPOT_LIGHTS)
				{
					m_instancedOverflow[1]++;
				}

				if (lightSource.GetType() == LIGHT_TYPE_CAPSULE && (numCapsuleLightsToDraw + m_instancedOverflow[2] + 1) >= MAX_INSTANCED_CAPSULE_LIGHTS)
				{
					m_instancedOverflow[2]++;
				}

				Assertf(numPointLightsToDraw < MAX_INSTANCED_POINT_LIGHTS, "Trying to add more than %d instanced point lights", MAX_INSTANCED_POINT_LIGHTS);
				Assertf(numSpotLightsToDraw < MAX_INSTANCED_SPOT_LIGHTS, "Trying to add more than %d instanced spot lights", MAX_INSTANCED_SPOT_LIGHTS);
				Assertf(numCapsuleLightsToDraw < MAX_INSTANCED_CAPSULE_LIGHTS, "Trying to add more than %d instanced capsule lights", MAX_INSTANCED_CAPSULE_LIGHTS);
			}
			#endif

			#if __BANK
				m_lodLightCount++;
			#endif

		}
	}

#if __ASSERT
	if (m_instancedOverflow[0] > 0) { renderWarningf("Overflow of point instanced lod lights - %d",   m_instancedOverflow[0]); }
	if (m_instancedOverflow[1] > 0) { renderWarningf("Overflow of spot instanced lod lights - %d",    m_instancedOverflow[1]); }
	if (m_instancedOverflow[2] > 0) { renderWarningf("Overflow of capsule instanced lod lights - %d", m_instancedOverflow[2]); }
	for (int i = 0; i < 3; i++) 
	{ 
		m_instancedOverflow[i] = 0; 
	}
#endif


	m_pointInstanceVB[m_currentPointVBIndex]->UnlockWO();
	m_spotInstanceVB[m_currentSpotVBIndex]->UnlockWO();
	m_capsuleInstanceVB[m_currentCapsuleVBIndex]->UnlockWO();
	
	if (numSpotLightsToDraw > 0)
	{
		#if RSG_DURANGO || RSG_ORBIS
			WritebackDC(spotDst, sizeof(SpotLightInstanceData) * numSpotLightsToDraw);
		#endif

		grcVertexBuffer* spotLightMesh = Lights::GetLightMesh(4);

		Lights::UseLightStencil(false, false, false, false, false, EM_IGNORE);
		DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_SPOT, MM_DEFAULT);

		const grmShader* shader = DeferredLighting::GetShader(DEFERRED_SHADER_SPOT);
		grcEffectTechnique technique = DeferredLighting::GetTechnique(DEFERRED_TECHNIQUE_SPOTCM);

		if (ShaderUtil::StartShader("Instanced Spot Lights", shader, technique, LIGHTPASS_INSTANCED, true))
		{
			grcViewport::SetCurrentWorldIdentity();

			GRCDEVICE.SetVertexDeclaration(m_spotVtxDecl);

			GRCDEVICE.SetStreamSource(0, *spotLightMesh, 0, spotLightMesh->GetVertexStride());
			GRCDEVICE.SetStreamSource(1, *m_spotInstanceVB[m_currentSpotVBIndex], 0, m_spotInstanceVB[m_currentSpotVBIndex]->GetVertexStride());
			GRCDEVICE.DrawInstancedPrimitive(drawTriStrip, spotLightMesh->GetVertexCount(), numSpotLightsToDraw, 0, 0);
		}
		ShaderUtil::EndShader(shader, true);

		GRCDEVICE.ClearStreamSource(0);
		GRCDEVICE.ClearStreamSource(1);

		GRCDEVICE.SetVertexDeclaration(NULL);
	}

	if (numPointLightsToDraw > 0)
	{
		#if RSG_DURANGO || RSG_ORBIS
			WritebackDC(pointDst, sizeof(PointLightInstanceData) * numPointLightsToDraw);
		#endif

		grcVertexBuffer* pointLightMesh = Lights::GetLightMesh(0);

		Lights::UseLightStencil(false, false, false, false, false, EM_IGNORE);
		DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_POINT, MM_DEFAULT);

		const grmShader* shader = DeferredLighting::GetShader(DEFERRED_SHADER_POINT);
		grcEffectTechnique technique = DeferredLighting::GetTechnique(DEFERRED_TECHNIQUE_POINTCM);

		if (ShaderUtil::StartShader("Instanced Point Lights", shader, technique, LIGHTPASS_INSTANCED, true))
		{
			grcViewport::SetCurrentWorldIdentity();

			GRCDEVICE.SetVertexDeclaration(m_pointVtxDecl);

			GRCDEVICE.SetStreamSource(0, *pointLightMesh, 0, pointLightMesh->GetVertexStride());
			GRCDEVICE.SetStreamSource(1, *m_pointInstanceVB[m_currentPointVBIndex], 0, m_pointInstanceVB[m_currentPointVBIndex]->GetVertexStride());
			GRCDEVICE.DrawInstancedPrimitive(drawTriStrip, pointLightMesh->GetVertexCount(), numPointLightsToDraw, 0, 0);
		}
		ShaderUtil::EndShader(shader, true);

		GRCDEVICE.ClearStreamSource(0);
		GRCDEVICE.ClearStreamSource(1);

		GRCDEVICE.SetVertexDeclaration(NULL);
	}

	if (numCapsuleLightsToDraw > 0)
	{
		#if RSG_DURANGO || RSG_ORBIS
			WritebackDC(capsuleDst, sizeof(CapsuleLightInstanceData) * numCapsuleLightsToDraw);
		#endif

		grcVertexBuffer* pointLightMesh = Lights::GetLightMesh(0);

		Lights::UseLightStencil(false, false, false, false, false, EM_IGNORE);
		DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_CAPSULE, MM_DEFAULT);

		const grmShader* shader = DeferredLighting::GetShader(DEFERRED_SHADER_CAPSULE);
		grcEffectTechnique technique = DeferredLighting::GetTechnique(DEFERRED_TECHNIQUE_CAPSULE);

		if (ShaderUtil::StartShader("Instanced Capsule Lights", shader, technique, LIGHTPASS_INSTANCED, true))
		{
			grcViewport::SetCurrentWorldIdentity();

			GRCDEVICE.SetVertexDeclaration(m_capsuleVtxDecl);

			GRCDEVICE.SetStreamSource(0, *pointLightMesh, 0, pointLightMesh->GetVertexStride());
			GRCDEVICE.SetStreamSource(1, *m_capsuleInstanceVB[m_currentCapsuleVBIndex], 0, m_capsuleInstanceVB[m_currentCapsuleVBIndex]->GetVertexStride());
			GRCDEVICE.DrawInstancedPrimitive(drawTriStrip, pointLightMesh->GetVertexCount(), numCapsuleLightsToDraw, 0, 0);
		}
		ShaderUtil::EndShader(shader, true);

		GRCDEVICE.ClearStreamSource(0);
		GRCDEVICE.ClearStreamSource(1);

		GRCDEVICE.SetVertexDeclaration(NULL);
	}
				
	m_currentPointVBIndex = (m_currentPointVBIndex + 1) % m_maxVertexBuffers;
	m_currentSpotVBIndex = (m_currentSpotVBIndex + 1) % m_maxVertexBuffers;
	m_currentCapsuleVBIndex = (m_currentCapsuleVBIndex + 1) % m_maxVertexBuffers;
}
// ----------------------------------------------------------------------------------------------- //

struct DistantLightElementStruct
{
	FloatXYZ m_position;
	u32 m_color;
};
// ----------------------------------------------------------------------------------------------- //

void CLODLights::RenderDistantLODLights(CVisualEffects::eRenderMode mode, float intensityScale)
{
	if( CRenderer::GetDisableArtificialLights() )
	{ // EMP shut down
			return;
	}

	if(!CLODLightManager::m_currentFrameInfo)
		return;

	CDistantLightsVertexBuffer::CreateVertexBuffers();

	if (mode == CVisualEffects::RM_WATER_REFLECTION)
	{
		CRenderPhaseWaterReflectionInterface::AdjustDistantLights(intensityScale);

		if (intensityScale <= 0.0f)
		{
			return;
		}
	}

	if (m_lodLightsEnabled && CLODLightManager::GetIfCanLoad() )
	{

#if __BANK
		if (DebugLighting::m_distantLODLightRender)
#endif
		{
			// check that we're being called by the render thread
			Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Not called from the render thread");	

			u8 alphaNormal = u8(Lights::CalculateTimeFade(
				g_distLightsParam.hourStart - 1.0f, 
				g_distLightsParam.hourStart,
				g_distLightsParam.hourEnd - 1.0f,
				g_distLightsParam.hourEnd) * 255.0f + 0.5f);

			u8 alphaStreet = u8(Lights::CalculateTimeFade(
				g_distLightsParam.streetLightHourStart - 1.0f, 
				g_distLightsParam.streetLightHourStart,
				g_distLightsParam.streetLightHourEnd - 1.0f,
				g_distLightsParam.streetLightHourEnd) * 255.0f + 0.5f);

			// if there are no coastal lights on then return
			if (alphaNormal > 0 || alphaStreet > 0)
			{
				const grcViewport* grcVP = grcViewport::GetCurrent(); // needs to work with reflections too, so just use the current viewport
				const Vec3V cameraPos = grcVP->GetCameraPosition();

				// NOTE: Skip the small distant lights (0), they are just noise
				for (u32 c = 1; c < LODLIGHT_CATEGORY_COUNT; c++)
				{
					const eLodLightCategory currentCategory = (eLodLightCategory)c;
					const spdSphere distantLightSphere = spdSphere(cameraPos, ScalarV(GetRenderLodLightCoronaRange(currentCategory)));

					float fadeNearDist = GetRenderLodLightCoronaRange(currentCategory);
					float fadeFarDist = GetRenderLodLightCoronaRange(currentCategory);

					#if __BANK
						if (DebugLighting::m_distantLODLightsOverrideRange)
						{
							fadeNearDist = DebugLighting::m_distantLODLightsStart;
							fadeFarDist = DebugLighting::m_distantLODLightsEnd;
						}

						if (DebugLighting::m_distantLODLightCategory > 0 &&
							currentCategory != (DebugLighting::m_distantLODLightCategory - 1))
						{
							continue;
						}
					#endif

					CDistantLightsVertexBuffer::RenderBufferBegin(mode, fadeNearDist, fadeFarDist, intensityScale*g_distLightsParam.streetLightHdrIntensity);

					atFixedArray<CDistantLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &DistLODLightsToRender = 
						CLODLightManager::GetDistLODLightsToRenderRenderBuffer(currentCategory);
					u32 startIndex = 0;
					u32 endIndex = DistLODLightsToRender.size();

					for (s32 i = startIndex; i < endIndex; i++)
					{
						const CDistantLODLightBucket *pBucket = DistLODLightsToRender[i];
						const u32 numStreetLights = pBucket->pLights->m_numStreetLights;

						const spdAABB &physicalBounds = INSTANCE_STORE.GetPhysicalBounds(strLocalIndex(pBucket->mapDataSlotIndex));

						if(!physicalBounds.ContainedBySphere(distantLightSphere))
						{
							u32 lightCount = 
								(numStreetLights * (alphaStreet > 0)) + 
								((pBucket->lightCount - numStreetLights) * (alphaNormal > 0));

							DistantLightElementStruct* RESTRICT outputBuff = reinterpret_cast<DistantLightElementStruct*> (CDistantLightsVertexBuffer::ReserveRenderBufferSpace(lightCount));		

							if (outputBuff)
							{
								CDistantLODLight *pDistLight = pBucket->pLights;
								
								const u32* pRGBIPrefetch = pDistLight->m_RGBI.GetElements() + 4;
								const FloatXYZ* pPositionPrefetch = pDistLight->m_position.GetElements() + 4;
								const DistantLightElementStruct* pOuputPrefetch = outputBuff + 4;

								// Streetlights
								if (alphaStreet > 0)
								{
									for(int j = 0; j < numStreetLights; j++)
									{
										PrefetchDC(pRGBIPrefetch + j);
										PrefetchDC(pPositionPrefetch + j);
										PrefetchDC(pOuputPrefetch + j);
										Color32 lightColour;
										lightColour.Set(pDistLight->m_RGBI[j]);
										lightColour = lightColour.MultiplyAlpha(alphaStreet);
										const u32 lightDeviceColor = lightColour.GetDeviceColor();

										#if __WIN32PC && __D3D9
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
										#else
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
										#endif
									}
								}

								// Everything else
								if (alphaNormal > 0)
								{
									for(int j = numStreetLights; j < pBucket->lightCount; j++)
									{
										PrefetchDC(pRGBIPrefetch + j);
										PrefetchDC(pPositionPrefetch + j);
										PrefetchDC(pOuputPrefetch + j);
										Color32 lightColour;
										lightColour.Set(pDistLight->m_RGBI[j]);
										lightColour = lightColour.MultiplyAlpha(alphaNormal);
										const u32 lightDeviceColor = lightColour.GetDeviceColor();

										#if __WIN32PC && __D3D9
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
										#else
											(*outputBuff).m_position = pDistLight->m_position[j];
											(*outputBuff).m_color = lightDeviceColor;
											outputBuff++;
										#endif
									}
								}
							}
							#if __WIN32PC
							else
							{
								break;
							}
							#endif // __WIN32PC
						}
					}

					CDistantLightsVertexBuffer::RenderBuffer(true);
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::ProcessVisibility()
{
	if (BANK_SWITCH(!DebugLighting::m_LODLightCoronaEnable || !DebugLighting::m_LODLightRender, false) || CRenderer::GetDisableArtificialLights())
	{
		return;
	}

	int bufferId = GetUpdateBufferIndex();
	LodLightVisibility* pVisibilityArray = &(g_LodLightVisibility[bufferId][0]);
	u32* pVisibilityCount = &(g_LightsCount[bufferId]);

#if __DEV
	u32* pLodLightsOcclusionTrivialAcceptActiveFrustum = &(g_LodLightsOcclusionTrivialAcceptActiveFrustum[bufferId]);
	u32* pLodLightsOcclusionTrivialAcceptMinZ = &(g_LodLightsOcclusionTrivialAcceptActiveMinZ[bufferId]);
	u32* pLodLightsOcclusionTrivialAcceptVisiblePixel = &(g_LodLightsOcclusionTrivialAcceptVisiblePixel[bufferId]);
	*pLodLightsOcclusionTrivialAcceptActiveFrustum = 0;
	*pLodLightsOcclusionTrivialAcceptMinZ = 0;
	*pLodLightsOcclusionTrivialAcceptVisiblePixel = 0;
#endif

	//Reset Count to 0
	*pVisibilityCount = 0;

	// Setup cull planes (use exterior quad if in an interior or just the frustum)
	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
	g_LodLightVisibilityData.m_vCameraPos = grcVP->GetCameraPosition();

	const atFixedArray<CLODLightBucket*, MAX_NUM_LOD_LIGHT_BUCKETS> &LODLightsToRender = CLODLightManager::GetLODLightsToRenderUpdateBuffer();

	if(LODLightsToRender.GetCount() == 0)
	{
		return;
	}

	if(g_SceneToGBufferPhaseNew && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker() && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid())
	{
		const bool bInteriorCullAll = !fwScan::GetScanResults().GetIsExteriorVisibleInGbuf();

		if(bInteriorCullAll == false)
		{
			fwScreenQuad quad = fwScan::GetScanResults().GetExteriorGbufScreenQuad();
			quad.GenerateFrustum(grcVP->GetCullCompositeMtx(), g_LodLightVisibilityData.m_cullPlanes);
		}
		else
		{
			return;
		}
	}
	else
	{
#if NV_SUPPORT
		g_LodLightVisibilityData.m_cullPlanes.SetStereo(*grcVP, true, true);		
#else
		g_LodLightVisibilityData.m_cullPlanes.Set(*grcVP, true, true);
#endif
	}

	g_LodLightVisibilityData.m_bCheckReflectionPhase = BANK_ONLY(DebugLighting::m_LODLightCoronaEnableWaterReflectionCheck && ) (Water::UseHQWaterRendering());
	const grcViewport* grcWaterReflectionVP = CRenderPhaseWaterReflectionInterface::GetViewport();
	if(grcWaterReflectionVP)
	{
		g_LodLightVisibilityData.m_vWaterReflectionCameraPos = grcWaterReflectionVP->GetCameraPosition();
		g_LodLightVisibilityData.m_waterReflectionCullPlanes.Set(*grcWaterReflectionVP, true, true);
	}
	else
	{
		g_LodLightVisibilityData.m_bCheckReflectionPhase = false;
	}

	const float sizeMult = 2.5f * g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SPRITE_SIZE);

	g_LodLightVisibilityData.m_hour = CClock::GetHour();
	g_LodLightVisibilityData.m_fade = (float)CClock::GetMinute() + ((float)CClock::GetSecond() / 60.0f);
	g_LodLightVisibilityData.m_brightness = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SPRITE_BRIGHTNESS);
	g_LodLightVisibilityData.m_coronaSize = m_lodLightCoronaSize * sizeMult;
	g_LodLightVisibilityData.m_lodLightCoronaIntensityMult = m_lodLightCoronaIntensityMult;
	g_LodLightVisibilityData.m_bIsCutScenePlaying = camInterface::GetCutsceneDirector().IsCutScenePlaying();

	for (u32 i = 0; i < LODLIGHT_CATEGORY_COUNT; i++)
	{
		g_LodLightVisibilityData.m_LodLightRange[i] = GetUpdateLodLightRange((eLodLightCategory)i);
		g_LodLightVisibilityData.m_LodLightCoronaRange[i] = GetUpdateLodLightCoronaRange((eLodLightCategory)i);
		g_LodLightVisibilityData.m_LodLightFadeRange[i] = GetLodLightFadeRange((eLodLightCategory)i);
	}

	Corona_t* pCoronaArray = NULL;
	u32* pCoronaCount = NULL;
#if __BANK
	u32* pOverflowCount = NULL;
	float debugZBiasScale = 1.0f;
#endif


	g_coronas.GetDataForAsyncJob(pCoronaArray, pCoronaCount BANK_ONLY(,pOverflowCount, debugZBiasScale));
	g_LodLightVisibilityData.m_pCoronaArray = pCoronaArray;
	g_LodLightVisibilityData.m_pVisibilityArray = pVisibilityArray;
	g_LodLightVisibilityData.m_pCoronaCount = pCoronaCount;
	g_LodLightVisibilityData.m_pVisibilityCount = pVisibilityCount;
#if __BANK
	g_LodLightVisibilityData.m_pOverflowCount = pOverflowCount;
	g_LodLightVisibilityData.m_pLODLightCoronaCount = &m_lodLightCoronaCount;
	g_LodLightVisibilityData.m_bPerformLightVisibility = DebugLighting::m_LODLightsVisibilityOnMainThread;
	g_LodLightVisibilityData.m_bPerformOcclusion = DebugLighting::m_LODLightsEnableOcclusionTest && DebugLighting::m_LODLightsEnableOcclusionTestInMainThread;
	g_LodLightVisibilityData.m_bPerformSort = DebugLighting::m_LODLightsPerformSortDuringVisibility;
#endif
#if __DEV
	g_LodLightVisibilityData.m_pLodLightsOcclusionTrivialAcceptActiveFrustum = pLodLightsOcclusionTrivialAcceptActiveFrustum;
	g_LodLightVisibilityData.m_pLodLightsOcclusionTrivialAcceptMinZ = pLodLightsOcclusionTrivialAcceptMinZ;
	g_LodLightVisibilityData.m_pLodLightsOcclusionTrivialAcceptVisiblePixel = pLodLightsOcclusionTrivialAcceptVisiblePixel;
#endif

	g_LodLightVisibilityData.m_coronaZBias = g_coronas.ZBiasClamp(m_lodLightCoronaSize * sizeMult * g_coronas.GetZBiasFromFinalSizeMultiplier() BANK_ONLY(* debugZBiasScale));

	g_LodLightVisibilityData.m_vDirMultFadeOffStart = ScalarV(g_coronas.GetDirMultFadeOffStart());
	g_LodLightVisibilityData.m_vDirMultFadeOffDist = ScalarV(g_coronas.GetDirMultFadeOffDist());


	LightingData &lightData = Lights::GetUpdateLightingData();
	fwScanBaseInfo&		scanBaseInfo = fwScan::GetScanBaseInfo();

	g_LodLightVisibilityData.m_coronaLightHashes = lightData.m_coronaLightHashes.GetElements();
	g_LodLightVisibilityData.m_numCoronaLightHashes = lightData.m_coronaLightHashes.GetCount();

#if __BANK
	m_lodLightCoronaCount = 0;
	if(!DebugLighting::m_LODLightsProcessVisibilityAsync)
	{
		LodLightVisibility pLocalVisibilitySpotArray[MAX_VISIBLE_LOD_LIGHTS];
		LodLightVisibility pLocalVisibilityPointArray[MAX_VISIBLE_LOD_LIGHTS];
		LodLightVisibility pLocalVisibilityCapsuleArray[MAX_VISIBLE_LOD_LIGHTS];

		ProcessLodLightVisibility::ProcessVisibility(NULL, 
			pVisibilityArray, pLocalVisibilitySpotArray, pLocalVisibilityPointArray, pLocalVisibilityCapsuleArray,
			pCoronaArray, 
			LODLightsToRender.GetElements(), LODLightsToRender.GetCount(), 
			lightData.m_lightHashes.GetElements(), lightData.m_lightHashes.GetCount(), 
			lightData.m_brokenLightHashes.GetElements(), lightData.m_brokenLightHashes.GetCount(),
			lightData.m_coronaLightHashes.GetElements(), lightData.m_coronaLightHashes.GetCount(),
			&scanBaseInfo, COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY), &g_LodLightVisibilityData, false);	
	}
	else
#endif
	{
		g_ProcessLodLightVisibilityDependency.m_Priority = sysDependency::kPriorityMed;
		g_ProcessLodLightVisibilityDependency.m_Flags = 
			sysDepFlag::ALLOC0 |
			sysDepFlag::INPUT1 | 
			sysDepFlag::INPUT2 | 
			sysDepFlag::INPUT3 | 
			sysDepFlag::INPUT4 |
			sysDepFlag::INPUT6 |
			sysDepFlag::INPUT7;
		g_ProcessLodLightVisibilityDependency.m_Params[1].m_AsPtr = (void*)LODLightsToRender.GetElements();
		g_ProcessLodLightVisibilityDependency.m_Params[2].m_AsPtr = lightData.m_lightHashes.GetElements();
		g_ProcessLodLightVisibilityDependency.m_Params[3].m_AsPtr = lightData.m_brokenLightHashes.GetElements();
		g_ProcessLodLightVisibilityDependency.m_Params[4].m_AsPtr = &g_LodLightVisibilityData;
		g_ProcessLodLightVisibilityDependency.m_Params[5].m_AsPtr = &g_ProcessLodLightVisibilityDependencyRunning;

		g_ProcessLodLightVisibilityDependency.m_Params[6].m_AsPtr = &scanBaseInfo;
		g_ProcessLodLightVisibilityDependency.m_Params[7].m_AsPtr = COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY);	

		g_ProcessLodLightVisibilityDependency.m_DataSizes[0] = PROCESSLODLIGHT_CORONAS_SCRATCH_SIZE;
		g_ProcessLodLightVisibilityDependency.m_DataSizes[1] = LODLightsToRender.GetCount() * sizeof(CLODLightBucket*);
		g_ProcessLodLightVisibilityDependency.m_DataSizes[2] = lightData.m_lightHashes.GetCount() * sizeof(u32);
		g_ProcessLodLightVisibilityDependency.m_DataSizes[3] = lightData.m_brokenLightHashes.GetCount() * sizeof(u32);
		g_ProcessLodLightVisibilityDependency.m_DataSizes[4] = sizeof(LodLightVisibilityData);
		g_ProcessLodLightVisibilityDependency.m_DataSizes[6] = sizeof(fwScanBaseInfo);
		g_ProcessLodLightVisibilityDependency.m_DataSizes[7] = RAST_HIZ_AND_STENCIL_SIZE_BYTES;


		g_ProcessLodLightVisibilityDependencyRunning = 1;

		sysDependencyScheduler::Insert( &g_ProcessLodLightVisibilityDependency );
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::WaitForProcessVisibilityDependency()
{
	BANK_ONLY(if(DebugLighting::m_LODLightsProcessVisibilityAsync))
	{
		PF_PUSH_TIMEBAR("CLODLights Wait for Visibility Dependency");
		while(g_ProcessLodLightVisibilityDependencyRunning)
		{
			sysIpcYield(PRIO_NORMAL);
		}
		PF_POP_TIMEBAR();
	}
}

// ----------------------------------------------------------------------------------------------- //

bool CLODLights::IsActive(CLODLight *pLODLight, u32 idx)
{
	packedTimeAndStateFlags &flags = (packedTimeAndStateFlags&)(pLODLight->m_timeAndStateFlags[idx]);

	// Check the time Flags
	u32 hour = CClock::GetHour();
	if (!(flags.timeFlags & (1 << hour)))
	{
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------------------------- //

float CLODLights::GetRenderLodLightRange(eLodLightCategory lightCategory)
{
	const float heightScale = Lights::GetRenderSphereExpansionMult();

	const float distScale = (lightCategory == LODLIGHT_CATEGORY_MEDIUM) ? 
		g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_LODLIGHT_RANGE_MULT) : 1.0f;
	
	const float lodScaleDist = Max<float>(1.0, Lights::GetRenderLodDistScale() * m_lodLightScaleMultiplier);
	
	return m_lodLightRange[lightCategory] * lodScaleDist * distScale * heightScale;
}

// ----------------------------------------------------------------------------------------------- //

float CLODLights::GetUpdateLodLightRange(eLodLightCategory lightCategory)
{
	const float heightScale = Lights::GetUpdateSphereExpansionMult();

	const float distScale = (lightCategory == LODLIGHT_CATEGORY_MEDIUM) ? 
		g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_LODLIGHT_RANGE_MULT) : 1.0f;
	
	const float lodScaleDist = Max<float>(1.0, Lights::GetUpdateLodDistScale() * m_lodLightScaleMultiplier);
	
	return m_lodLightRange[lightCategory] * lodScaleDist * distScale * heightScale;
}

// ----------------------------------------------------------------------------------------------- //

float CLODLights::GetRenderLodLightCoronaRange(eLodLightCategory lightCategory)
{
	const float lodScaleDist = Lights::GetRenderLodDistScale() * m_lodLightScaleMultiplier;
	return m_lodLightCoronaRange[lightCategory] * Max<float>(1.0, lodScaleDist);
}

// ----------------------------------------------------------------------------------------------- //

float CLODLights::GetUpdateLodLightCoronaRange(eLodLightCategory lightCategory)
{
	const float lodScaleDist = Lights::GetUpdateLodDistScale() * m_lodLightScaleMultiplier;
	return m_lodLightCoronaRange[lightCategory] * Max<float>(1.0, lodScaleDist);
}

// ----------------------------------------------------------------------------------------------- //

LodLightVisibility* CLODLights::GetLODLightVisibilityUpdateBuffer()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	return &(g_LodLightVisibility[GetUpdateBufferIndex()][0]);
}

// ----------------------------------------------------------------------------------------------- //

LodLightVisibility* CLODLights::GetLODLightVisibilityRenderBuffer()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return &(g_LodLightVisibility[GetRenderBufferIndex()][0]);
}

// ----------------------------------------------------------------------------------------------- //

u32 CLODLights::GetLODLightVisibilityCountUpdateBuffer()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	return g_LightsCount[GetUpdateBufferIndex()];
}

// ----------------------------------------------------------------------------------------------- //

u32 CLODLights::GetLODLightVisibilityCountRenderBuffer()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	return g_LightsCount[GetRenderBufferIndex()];
}

// ----------------------------------------------------------------------------------------------- //

float CLODLights::CalculateSphereExpansion(const float distanceToGround)
{
	float blend = Saturate((distanceToGround - m_lodLightSphereExpandStart) / 
		(m_lodLightSphereExpandEnd - m_lodLightSphereExpandStart));

	return Lerp(blend, 1.0f, m_lodLightSphereExpandSizeMult);
}

// ----------------------------------------------------------------------------------------------- //

void CLODLights::UpdateVisualDataSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
		m_lodLightRange[LODLIGHT_CATEGORY_SMALL] = g_visualSettings.Get("lodlight.small.range", 150.0f);
		m_lodLightRange[LODLIGHT_CATEGORY_MEDIUM] = g_visualSettings.Get("lodlight.medium.range", 450.0f);
		m_lodLightRange[LODLIGHT_CATEGORY_LARGE] = g_visualSettings.Get("lodlight.large.range", 1250.0f);

		m_lodLightFade[LODLIGHT_CATEGORY_SMALL] = g_visualSettings.Get("lodlight.small.fade", 50.0f);
		m_lodLightFade[LODLIGHT_CATEGORY_MEDIUM] = g_visualSettings.Get("lodlight.medium.fade", 50.0f);
		m_lodLightFade[LODLIGHT_CATEGORY_LARGE] = g_visualSettings.Get("lodlight.large.fade", 50.0f);

		m_lodLightCoronaRange[LODLIGHT_CATEGORY_SMALL] = g_visualSettings.Get("lodlight.small.corona.range", 150.0f);
		m_lodLightCoronaRange[LODLIGHT_CATEGORY_MEDIUM] = g_visualSettings.Get("lodlight.medium.corona.range", 450.0f);
		m_lodLightCoronaRange[LODLIGHT_CATEGORY_LARGE] = g_visualSettings.Get("lodlight.large.corona.range", 1250.0f);

		m_lodLightCoronaSize = g_visualSettings.Get("lodlight.corona.size", 1.0f);

		m_lodLightSphereExpandSizeMult = g_visualSettings.Get("lodlight.sphere.expand.size.mult", 1.0f);
		m_lodLightSphereExpandStart = g_visualSettings.Get("lodlight.sphere.expand.start", 1.0f);
		m_lodLightSphereExpandEnd = g_visualSettings.Get("lodlight.sphere.expand.end", 1.0f);
	}
}

