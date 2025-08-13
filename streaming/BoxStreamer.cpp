// 
// filename:	BoxStreamer.cpp
//

#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()

// --- Include Files ------------------------------------------------------------
#include "BoxStreamer.h"

// C headers
// Rage headers
#include "input\mouse.h"
#include "fwsys/timer.h"

// Game headers
#include "debug/vectormap.h"
#include "network/NetworkInterface.h"
#include "pathserver/ExportCollision.h"
#include "pathserver/NavGenParam.h"
#include "peds/ped.h"
#include "scene/lod/LodMgr.h"
#include "script/script.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingrequestlist.h"
#include "vehicles/vehicle.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "debug/DebugScene.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/world/GameWorld.h"
#include "task/System/TaskTypes.h"

#if RSG_PC
#include "fwscene/mapdata/mapdatadebug.h"
#include "scene/FocusEntity.h"
#endif

#if __BANK
#include "debug/Rendering/DebugLighting.h"
#endif	//__BANK

XPARAM(inflateimaplow);
XPARAM(inflateimapmedium);
XPARAM(inflateimaphigh);


bool CGTABoxStreamerInterfaceNew::IsExportingCollision()
{
#if NAVMESH_EXPORT
	// For exporting map collision data, ignore Z extents or we will miss required geometry
	return CNavMeshDataExporter::IsExportingCollision();
#else	// NAVMESH_EXPORT
	return false;
#endif	// NAVMESH_EXPORT
}

void CGTABoxStreamerInterfaceNew::GetGameSpecificSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags)
{
	const bool bNetworkGame = NetworkInterface::IsGameInProgress();

	//
	// request static bounds (mover) for any script entity which are flagged to require collision
	if ((supportedAssetFlags & fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER) != 0)
	{
		u32 numEntities = 0;
		CEntity** entityArray = CTheScripts::GetAllScriptEntities(numEntities);

		for (u32 i=0; i<numEntities; i++)
		{
			CEntity *pEntity = entityArray[i];

			if (streamVerify(pEntity))
			{
				if (pEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pEntity)->GetStatus() == STATUS_WRECKED)
				{
					continue;
				}

				if (pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->GetIsDeadOrDying())
				{
					continue;
				}

				if (pEntity->GetIsTypeObject() && !static_cast<CObject*>(pEntity)->ShouldLoadCollision())
				{
					continue;
				}

				if (pEntity && static_cast<CPhysical*>(pEntity)->ShouldLoadCollision())
				{
					// AF: IsFixed means it doesn't get moved about by physics. Luckily you don't want to load
					// collision for fixed objects either
					// don't load collision for objects frozen by script
					if(pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) || pEntity->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
					{
						continue;
					}

					// expand collision streaming for visible aeroplanes in MP
					bool bExpandedMoverStreaming = false;
					if (bNetworkGame && pEntity->GetIsTypeVehicle())
					{
						CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
						bExpandedMoverStreaming = ( pVehicle->InheritsFromPlane() && pVehicle->GetVelocity().Mag2()>30.0f && g_LodMgr.WithinVisibleRange(pEntity) );
					}

					// single point test against inflated mover collision bounds
					const spdSphere searchSphere(pEntity->GetTransform().GetPosition(), ScalarV(V_TEN));
					searchList.Grow() = fwBoxStreamerSearch(
						searchSphere,
						fwBoxStreamerSearch::TYPE_SCRIPTENTITY,
						fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER,
						bExpandedMoverStreaming
					);

				}
			}
		}

		CRecentlyPilotedAircraft::AddSearches(searchList, supportedAssetFlags);
	}

#if NAVMESH_EXPORT
	if(IsExportingCollision())
	{
		CNavMeshDataExporter::AddSearches(searchList);
	}
#endif
	// add searches for any active stream volumes
	CStreamVolumeMgr::AddSearches(searchList, supportedAssetFlags);

	g_PlayerSwitch.GetLongRangeMgr().AddSearches(searchList, supportedAssetFlags);
	gStreamingRequestList.AddSearches(searchList, supportedAssetFlags);

#if __BANK
	DebugLighting::AddSearches(searchList, supportedAssetFlags);
#endif

    NetworkInterface::AddBoxStreamerSearches(searchList);

#if RSG_PC
	//////////////////////////////////////////////////////////////////////////
	if ((supportedAssetFlags & fwBoxStreamerAsset::MASK_MAPDATA) != 0)
	{

		float fLowRange = 0.0f;
		float fMediumRange = 0.0f;
		float fHighRange = 0.0f;
#if __BANK
		PARAM_inflateimaplow.Get(fLowRange);
		PARAM_inflateimapmedium.Get(fMediumRange);
		PARAM_inflateimaphigh.Get(fHighRange);

		fLowRange = fwMapDataDebug::ms_fInflateRadius_Low;
		fMediumRange = fwMapDataDebug::ms_fInflateRadius_Medium;
		fHighRange = fwMapDataDebug::ms_fInflateRadius_High;
#endif
		const Settings& settings = CSettingsManager::GetInstance().GetSettings();
		if (settings.m_graphics.m_MaxLodScale > 0.0f BANK_ONLY(&& fMediumRange == 0.0f))
		{
			fMediumRange = 500.0f;
		}

		{
			const Vec3V vFocusPos = VECTOR3_TO_VEC3V( CFocusEntityMgr::GetMgr().GetPos() );

			if (fLowRange > 0.0f)
			{
				const spdSphere searchSphere(vFocusPos, ScalarV(fLowRange));
				searchList.Grow() = fwBoxStreamerSearch(
					searchSphere,
					fwBoxStreamerSearch::TYPE_INFLATE,
					fwBoxStreamerAsset::FLAG_MAPDATA_LOW,
					true
				);
			}

			if (fMediumRange > 0.0f)
			{
				const spdSphere searchSphere(vFocusPos, ScalarV(fMediumRange));
				searchList.Grow() = fwBoxStreamerSearch(
					searchSphere,
					fwBoxStreamerSearch::TYPE_INFLATE,
					fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM,
					true
				);
			}

			if (fHighRange > 0.0f)
			{
				const spdSphere searchSphere(vFocusPos, ScalarV(fHighRange));
				searchList.Grow() = fwBoxStreamerSearch(
					searchSphere,
					fwBoxStreamerSearch::TYPE_INFLATE,
					fwBoxStreamerAsset::FLAG_MAPDATA_HIGH,
					true
				);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
#endif
}

//////////////////////////////////////////////////////////////////////////
// simple mechanism to track the last aircraft that the player bailed out of
// so we can request mover collision for when it smashes into the ground
RegdVeh CRecentlyPilotedAircraft::ms_pRecentlyPilotedAircraft;
bool CRecentlyPilotedAircraft::ms_bActive = false;

void CRecentlyPilotedAircraft::Update()
{
	const u32 coolOffMaxMs = 20000;	// 15 seconds duration
	static u32	startTimeMs = 0;
	const u32 currentTimeMs = ( fwTimer::GetTimeInMilliseconds_NonScaledClipped() );

	// get the current player controlled aircraft, if there is one, and if it is flying
	CVehicle* pCurrentAircraft = NULL;
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		if (pPlayerPed->GetIsInVehicle())
		{
			CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
			if ( pVehicle && pVehicle->IsInAir() && pVehicle->GetIsAircraft() )
			{
				pCurrentAircraft = pVehicle;
			}
		}
	}
	
	if (pCurrentAircraft)
	{
		startTimeMs = currentTimeMs;		// reset timer

		if (pCurrentAircraft!=ms_pRecentlyPilotedAircraft)
		{
			ms_pRecentlyPilotedAircraft = pCurrentAircraft;
		}
	}

	bool bRunningNoPilotTask = false;
	if (ms_pRecentlyPilotedAircraft && ms_pRecentlyPilotedAircraft->GetIntelligence() && ms_pRecentlyPilotedAircraft->GetIntelligence()->GetActiveTask() &&
		ms_pRecentlyPilotedAircraft->GetIntelligence()->GetActiveTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_CRASH )
	{

		bRunningNoPilotTask = true;
	}

	ms_bActive = (!pCurrentAircraft && ms_pRecentlyPilotedAircraft && bRunningNoPilotTask && (currentTimeMs-startTimeMs)<=coolOffMaxMs);
}

void CRecentlyPilotedAircraft::AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags)
{
	// only interested in MOVER collision
	if ( ((supportedAssetFlags & fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER) != 0) && ms_pRecentlyPilotedAircraft && ms_bActive)
	{
		const spdSphere searchSphere(ms_pRecentlyPilotedAircraft->GetTransform().GetPosition(), ScalarV(V_ONE));
		searchList.Grow() = fwBoxStreamerSearch(
			searchSphere,
			fwBoxStreamerSearch::TYPE_RECENTVEHICLE,
			fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER,
			true
		);
	}

}