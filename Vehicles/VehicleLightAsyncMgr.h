#ifndef VEHICLE_LIGHT_ASYNC_MGR_H_
#define VEHICLE_LIGHT_ASYNC_MGR_H_

#include "vectormath/vec4v.h"
#include "fwscene/world/InteriorLocation.h"

class CVehicle;

namespace CVehicleLightAsyncMgr
{
	void Init();
	void Shutdown();

	void SetupDataForAsyncJobs();
	void KickJobForRemainingVehicles();
	void ProcessResults();

	void AddVehicle(CVehicle* pVehicle, fwInteriorLocation interiorLoc, u32 lightFlags, Vec4V_In ambientVolumeParams = Vec4V(V_ZERO_WONE));
};

#endif

