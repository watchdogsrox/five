#ifndef VEHICLE_POPULATION_ASYNC_H_
#define VEHICLE_POPULATION_ASYNC_H_

#include "system/dependency.h"

class VehiclePopulationAsyncUpdate
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
};

class VehiclePopAsyncUpdateDensities
{
public:
	static bool RunFromDependency(const sysDependency& dependency);
};

#endif

