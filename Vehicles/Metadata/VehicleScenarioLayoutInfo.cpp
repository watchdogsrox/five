// Main headers
#include "Vehicles/Metadata/VehicleScenarioLayoutInfo.h"

// Rage headers
#include "parser/manager.h"

// Game headers
#include "Debug/DebugScene.h"
#include "scene/loader/MapData_Extensions.h"
#include "Vehicles/vehicle_channel.h"
#include "Vehicles/Metadata/VehicleScenarioLayoutInfo_parser.h"

AI_OPTIMISATIONS()

const CExtensionDefSpawnPoint* CVehicleScenarioLayoutInfo::GetScenarioPointInfo(int iIndex) const
{
	vehicleAssertf(iIndex > -1 && iIndex < GetNumScenarioPoints(),"Invalid scenario point index");
	return &(m_ScenarioPoints[iIndex]);
}