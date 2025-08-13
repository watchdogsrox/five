// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

#include "Vehicles/VehicleLightAsyncMgr.h"

// framework headers
#include "system/dependencyscheduler.h"

// game headers
#include "Peds/ped.h"
#include "renderer/Lights/lights.h"
#include "timecycle/TimeCycle.h"
#include "vehicles/DoVehicleLightsAsync.h"
#include "vehicles/Vehicle.h"
#include "vfx/misc/Coronas.h"

#define		NUM_VEHICLE_LIGHT_DEPENDENCY_JOBS	60
#define		NUM_VEHICLES_PER_JOB				5
#define		MAX_VEHICLES_FOR_ASYNC_LIGHTS		(NUM_VEHICLE_LIGHT_DEPENDENCY_JOBS*NUM_VEHICLES_PER_JOB)

atFixedArray<CVehicle*,MAX_VEHICLES_FOR_ASYNC_LIGHTS>			g_aVehicles;
atFixedArray<u32,MAX_VEHICLES_FOR_ASYNC_LIGHTS>					g_aLightFlags;
atFixedArray<Vec4V,MAX_VEHICLES_FOR_ASYNC_LIGHTS>				g_aAmbientVolumeParams;
atFixedArray<fwInteriorLocation,MAX_VEHICLES_FOR_ASYNC_LIGHTS>	g_aInteriorLocations;

sysDependency				g_aVehicleLightDependency[NUM_VEHICLE_LIGHT_DEPENDENCY_JOBS];
u32							g_VehicleLightDependenciesPending;
DoVehicleLightsAsyncData	g_AsyncData;

Corona_t					g_aCoronasToAdd[MAX_NUM_VEHICLE_LIGHT_CORONAS]; 
CLightSource				g_aLightsToAdd[MAX_NUM_VEHICLE_LIGHTS];

Corona_t*					g_CurrCoronaPtr;
CLightSource*				g_CurrLightPtr;

int							g_NextDependencyIndex;

namespace CVehicleLightAsyncMgr
{
	void Init()
	{
		Assert(MAX_VEHICLES_FOR_ASYNC_LIGHTS >= CVehicle::GetPool()->GetSize());

		for(int i = 0; i < NUM_VEHICLE_LIGHT_DEPENDENCY_JOBS; i++)
		{
			g_aVehicleLightDependency[i].Init( DoVehicleLightsAsync::RunFromDependency, 0, sysDepFlag::ALLOC0 );
			g_aVehicleLightDependency[i].m_Priority = sysDependency::kPriorityLow;
		}
	}

	void Shutdown()
	{
	}

	void SetupDataForAsyncJobs()
	{
		g_CurrCoronaPtr = g_aCoronasToAdd;
		g_CurrLightPtr = g_aLightsToAdd;

		g_AsyncData.m_paVehicles			= g_aVehicles.GetElements();
		g_AsyncData.m_paLightFlags			= g_aLightFlags.GetElements();
		g_AsyncData.m_paAmbientVolumeParams	= g_aAmbientVolumeParams.GetElements();
		g_AsyncData.m_paInteriorLocs		= g_aInteriorLocations.GetElements();
		g_AsyncData.m_pCoronasCurrPtr		= &g_CurrCoronaPtr;
		g_AsyncData.m_pLightSourcesCurrPtr	= &g_CurrLightPtr;

		g_AsyncData.m_CoronasStartingPtr		= g_aCoronasToAdd;
		g_AsyncData.m_LightSourcesStartingPtr	= g_aLightsToAdd;
		g_AsyncData.m_MaxCoronas				= MAX_NUM_VEHICLE_LIGHT_CORONAS;
		g_AsyncData.m_MaxLightSources			= MAX_NUM_VEHICLE_LIGHTS;

		const tcKeyframeUncompressed& currKF = g_timeCycle.GetCurrUpdateKeyframe();
		g_AsyncData.m_timeCycleSpriteSize = currKF.GetVar(TCVAR_SPRITE_SIZE);
		g_AsyncData.m_timeCycleSpriteBrightness = currKF.GetVar(TCVAR_SPRITE_BRIGHTNESS);
		g_AsyncData.m_timecycleVehicleIntensityScale = currKF.GetVar(TCVAR_LIGHT_VEHICLE_INTENSITY_SCALE);
		g_NextDependencyIndex = 0;
	}

	void KickJobForRemainingVehicles()
	{
		// If we have any remaining vehicles to process, kick one final job
		int vehRemaining = g_aVehicles.GetCount() - g_NextDependencyIndex*NUM_VEHICLES_PER_JOB;
		if( vehRemaining == 0)
		{
			return;
		}

		sysDependency& dependency = g_aVehicleLightDependency[g_NextDependencyIndex];

		int startIndex = g_NextDependencyIndex*NUM_VEHICLES_PER_JOB;
		int lastIndex = startIndex + vehRemaining - 1;

		dependency.m_Params[1].m_AsPtr = &g_AsyncData;
		dependency.m_Params[2].m_AsPtr = &g_VehicleLightDependenciesPending;
		dependency.m_Params[3].m_AsInt = startIndex;
		dependency.m_Params[4].m_AsInt = lastIndex;

		dependency.m_DataSizes[0] = DO_VEHICLE_LIGHTS_ASYNC_SCRATCH_SIZE;

		sysInterlockedIncrement(&g_VehicleLightDependenciesPending);

		sysDependencyScheduler::Insert( g_aVehicleLightDependency + g_NextDependencyIndex);
	}

	void ProcessResults()
	{
		PF_PUSH_TIMEBAR("CVehicleLightAsyncMgr ProcessResults");

		PF_PUSH_TIMEBAR("Wait for Dependency");
		sysTimer t;

		while(true)
		{
			volatile u32 *pDependencyRunning = &g_VehicleLightDependenciesPending;
			if(*pDependencyRunning == 0)
			{
				break;
			}

			sysIpcYield(PRIO_NORMAL);
		}

		PF_POP_TIMEBAR();
		
		t.Reset();

		for(Corona_t* pCorona = g_aCoronasToAdd; pCorona < g_CurrCoronaPtr; pCorona++)
		{
			g_coronas.Register(pCorona->GetPosition(),
				pCorona->GetSize(),
				pCorona->GetColor(),
				pCorona->GetIntensity(),
				pCorona->GetZBias(),
				pCorona->GetDirection(),
				pCorona->GetDirectionThreshold(),
				pCorona->GetDirLightConeAngleInner(),
				pCorona->GetDirLightConeAngleOuter(),
				pCorona->GetFlags());
		}

		for(CLightSource* pLight = g_aLightsToAdd; pLight < g_CurrLightPtr; pLight++)
		{
			Assert(pLight->GetFlags() & LIGHTFLAG_ALREADY_TESTED_FOR_OCCLUSION);
			Lights::AddSceneLight(*pLight);
		}

		g_aVehicles.Reset();
		g_aLightFlags.Reset();
		g_aAmbientVolumeParams.Reset();
		g_aInteriorLocations.Reset();

		PF_POP_TIMEBAR();
	}

	void AddVehicle(CVehicle* pVehicle, fwInteriorLocation interiorLoc, u32 lightFlags, Vec4V_In ambientVolumeParams)
	{
		g_aVehicles.Push(pVehicle);
		g_aLightFlags.Push(lightFlags);
		g_aAmbientVolumeParams.Push(ambientVolumeParams);
		g_aInteriorLocations.Push(interiorLoc);

		int currentVehicleCount = g_aVehicles.GetCount();
		if(currentVehicleCount % NUM_VEHICLES_PER_JOB == 0)
		{
			// We've queued up enough vehicles to kick off a job
			sysDependency& dependency = g_aVehicleLightDependency[g_NextDependencyIndex];

			int startIndex = g_NextDependencyIndex*NUM_VEHICLES_PER_JOB;
			int lastIndex = startIndex + NUM_VEHICLES_PER_JOB - 1;

			dependency.m_Params[1].m_AsPtr = &g_AsyncData;
			dependency.m_Params[2].m_AsPtr = &g_VehicleLightDependenciesPending;
			dependency.m_Params[3].m_AsInt = startIndex;
			dependency.m_Params[4].m_AsInt = lastIndex;

			dependency.m_DataSizes[0] = DO_VEHICLE_LIGHTS_ASYNC_SCRATCH_SIZE;

			sysInterlockedIncrement(&g_VehicleLightDependenciesPending);

			sysDependencyScheduler::Insert( g_aVehicleLightDependency + g_NextDependencyIndex);

			g_NextDependencyIndex++;
		}
	}

} // end of namespace CVehicleLightAsyncMgr

