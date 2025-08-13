// Vehicle population Update Densities SPU
// jlandry: 25/07/2012

#include "vehiclepopulation.h"
#include "VehiclePopulationAsync.h"
#include "fwvehicleai/pathfindtypes.h"

bool VehiclePopAsyncUpdateDensities::RunFromDependency(const sysDependency& dependency)
{
	u32* pDependenciesRunningEa	= static_cast<u32*>(dependency.m_Params[0].m_AsPtr);

	u32 numScheduledLinks = 0;
	sScheduledLink* scheduledLinks = NULL; //(sScheduledLink*)alloca(sizeof(sScheduledLink) * CVehiclePopulation::ms_numGenerationLinks);

	CVehiclePopulation::UpdateDensities(scheduledLinks, numScheduledLinks);

	sysInterlockedDecrement(pDependenciesRunningEa);

	return true;
}
