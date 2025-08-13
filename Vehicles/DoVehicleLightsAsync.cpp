#include "DoVehicleLightsAsync.h"

#include "system/interlocked.h"
#include "system/memops.h"

#include "renderer/Lights/LightSource.h"
#include "vehicles/Vehicle.h"
#include "vfx/misc/Coronas.h"

void CoronaAndLightList::Init(Corona_t* pCoronas, CLightSource *pLightSources, u32 maxCoronas,
							  u32 maxLights, float tcSpriteSize, float tcSpriteBrightness, float tcVehicleLightIntensity)
{
	m_pCoronas = pCoronas;
	m_pLightSources = pLightSources;
	m_coronaCount = 0;
	m_lightCount = 0;
	m_maxCoronas = maxCoronas;
	m_maxLights = maxLights;
	m_timeCycleSpriteSize = tcSpriteSize;
	m_timeCycleSpriteBrightness = tcSpriteBrightness;
	m_timecycleVehicleIntensityScale = tcVehicleLightIntensity;
}

void CoronaAndLightList::RegisterCorona(Vec3V_In vPos,
		const float size,
		const Color32 col,
		const float intensity,
		const float zBias,
		Vec3V_In   vDir,
		const float dirViewThreshold,
		const float dirLightConeAngleInner,
		const float dirLightConeAngleOuter,
		const u16 coronaFlags)
{
	if (size == 0.0f || intensity <= 0.0f)
	{
		return;
	}

	if (!Verifyf(m_coronaCount < m_maxCoronas, "Too many coronas added in one job!"))
		return;

	u8	uDirLightConeAngleInner = (u8)Clamp(dirLightConeAngleInner, 0.0f, CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER);
	u8	uDirLightConeAngleOuter = (u8)Clamp(dirLightConeAngleOuter, 0.0f, CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER);
	
	Corona_t* pCorona = m_pCoronas + m_coronaCount;
	pCorona->SetPosition(vPos);
	pCorona->SetSize(size);
	pCorona->SetColor(col);
	pCorona->SetIntensity(intensity);
	pCorona->SetZBias(zBias);
	pCorona->SetDirectionViewThreshold(Vec4V(vDir,ScalarV(dirViewThreshold)));
	pCorona->SetCoronaFlagsAndDirLightConeAngles(coronaFlags, uDirLightConeAngleInner, uDirLightConeAngleOuter);	

	m_coronaCount++;
}

void CoronaAndLightList::AddLight(CLightSource& light)
{
	if (!Verifyf(m_lightCount < m_maxLights, "Too many lights added in one job!"))
		return;
	m_pLightSources[m_lightCount] = light;
	m_lightCount++;
}

bool DoVehicleLightsAsync::RunFromDependency(const sysDependency& dependency)
{
	void* scratchMemory = dependency.m_Params[0].m_AsPtr;
	fwStepAllocator scratchAllocator;
	scratchAllocator.Init(scratchMemory, DO_VEHICLE_LIGHTS_ASYNC_SCRATCH_SIZE);
	Corona_t *pLocalCoronas = scratchAllocator.Alloc<Corona_t>(MAX_NUM_CORONAS_PER_JOB, 16);
	CLightSource *pLocalLightSources = scratchAllocator.Alloc<CLightSource>(MAX_NUM_LIGHTS_PER_JOB, 16);

	DoVehicleLightsAsyncData* pData = static_cast<DoVehicleLightsAsyncData*>(dependency.m_Params[1].m_AsPtr);
	u32* pDependencyRunning = static_cast<u32*>(dependency.m_Params[2].m_AsPtr);
	int firstVehicleIndex = dependency.m_Params[3].m_AsInt;
	int lastVehicleIndex = dependency.m_Params[4].m_AsInt;

	CVehicle** aVehicles = pData->m_paVehicles;
	u32* aLightFlags = pData->m_paLightFlags;
	Vec4V* aAmbientVolumeParams = pData->m_paAmbientVolumeParams;
	fwInteriorLocation* aInteriorLocs = pData->m_paInteriorLocs;
	Corona_t* volatile* pCoronasCurrPtr = pData->m_pCoronasCurrPtr;
	CLightSource* volatile* pLightsCurrPtr = pData->m_pLightSourcesCurrPtr;

	CoronaAndLightList coronaAndLightList;
	coronaAndLightList.Init(pLocalCoronas, pLocalLightSources, MAX_NUM_CORONAS_PER_JOB, MAX_NUM_LIGHTS_PER_JOB,
		pData->m_timeCycleSpriteSize, pData->m_timeCycleSpriteBrightness, pData->m_timecycleVehicleIntensityScale);

	for(int i = firstVehicleIndex; i <= lastVehicleIndex; i++)
	{
		CVehicle *pVehicle = aVehicles[i];
		pVehicle->DoVehicleLightsAsync(MAT34V_TO_MATRIX34(pVehicle->GetMatrix()), aLightFlags[i], aAmbientVolumeParams[i], aInteriorLocs[i], coronaAndLightList);
	}

	u32 localCoronaCount = coronaAndLightList.GetCoronaCount();
	u32 localLightCount = coronaAndLightList.GetLightCount();
	if(localCoronaCount > 0)
	{
		const uintptr_t	coronasDestPtr = sysInterlockedAdd( reinterpret_cast< uintptr_t volatile* >( pCoronasCurrPtr ), ptrdiff_t(sizeof(Corona_t) * localCoronaCount) );

#if __ASSERT
		FatalAssertf((Corona_t*)coronasDestPtr >= pData->m_CoronasStartingPtr && ((Corona_t*)coronasDestPtr + localCoronaCount) < (pData->m_CoronasStartingPtr+pData->m_MaxCoronas),"Too many vehicle coronas! - increase MAX_NUM_VEHICLE_LIGHT_CORONAS");
#else
		FastAssert((Corona_t*)coronasDestPtr >= pData->m_CoronasStartingPtr && ((Corona_t*)coronasDestPtr + localCoronaCount) < (pData->m_CoronasStartingPtr+pData->m_MaxCoronas));
#endif

		sysMemCpy( reinterpret_cast< Corona_t* >( coronasDestPtr ), pLocalCoronas, sizeof(Corona_t)*localCoronaCount );
	}
	if(localLightCount > 0)
	{
		const uintptr_t	lightsDestPtr = sysInterlockedAdd( reinterpret_cast< uintptr_t volatile* >( pLightsCurrPtr ), ptrdiff_t(sizeof(CLightSource) * localLightCount) );

#if __ASSERT
		FatalAssertf((CLightSource*)lightsDestPtr >= pData->m_LightSourcesStartingPtr && ((CLightSource*)lightsDestPtr + localLightCount) < (pData->m_LightSourcesStartingPtr+pData->m_MaxLightSources),"Too many vehicle lights! - increase MAX_NUM_VEHICLE_LIGHTS");
#else
		FastAssert((CLightSource*)lightsDestPtr >= pData->m_LightSourcesStartingPtr && ((CLightSource*)lightsDestPtr + localLightCount) < (pData->m_LightSourcesStartingPtr+pData->m_MaxLightSources));
#endif

		// A mem copy will be ok as no fwRegdRefs have been set up (no need to destruct either).
		sysMemCpy( reinterpret_cast< CLightSource* >( lightsDestPtr ), pLocalLightSources, sizeof(CLightSource)*localLightCount );
	}

	sysInterlockedDecrement(pDependencyRunning);

	return true;
}