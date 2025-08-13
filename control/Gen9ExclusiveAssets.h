
#ifndef _GEN9_EXCLUSIVE_ASSETS_H_
#define _GEN9_EXCLUSIVE_ASSETS_H_

// Game headers
#include "Gen9ExclusiveAssetsDataPeds.h"
#include "Gen9ExclusiveAssetsDataVehicles.h"


// Forward declarations
class CPed;
class CVehicleKit;

class CGen9ExclusiveAssets
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void LoadGen9ExclusiveAssetsPedsData(const char *pFilename);
	static void LoadGen9ExclusiveAssetsVehiclesData(const char *pFilename);

	static bool IsGen9ExclusivePedDrawable(const CPed *pPed, ePedVarComp component, u32 globalDrawableIdx);
	static bool CanCreatePedDrawable(const CPed *pPed, ePedVarComp component, u32 globalDrawableIdx, bool bAssert);

	static bool IsGen9ExclusiveVehicleMod(const CVehicleKit* pVehicleModKit, eVehicleModType modSlot, u8 modIndex);
	static bool CanCreateVehicleMod(const CVehicleKit* pVehicleModKit, eVehicleModType modSlot, u8 modIndex, bool bAssert);

private:
	static Gen9ExclusiveAssetsDataPeds sm_Gen9ExclusiveAssetsDataPeds;
	static Gen9ExclusiveAssetsDataVehicles sm_Gen9ExclusiveAssetsDataVehicles;
};

#endif // _GEN9_EXCLUSIVE_ASSETS_H_
