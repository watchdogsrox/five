#ifndef VEHICLE_SCRIPT_RESOURCE_H
#define VEHICLE_SCRIPT_RESOURCE_H

// Game headers
#include "streaming/streamingrequest.h"

// Framework headers
#include "fwAnimation/animdefines.h"

////////////////////////////////////////////////////////////////////////////////

class CVehicleScriptResource
{
public:

	// If you add a flag and want it usable from script you need to update the matching enum in commands_vehicle.sch
	enum VehicleResourceFlags
	{
		VRF_RequestSeatAnims			= BIT0,
		VRF_RequestEntryAnims			= BIT1,
		VRF_RequestExitToAimAnims		= BIT2,
		VRF_RequestAlternateEntryAnims	= BIT3
	};

	CVehicleScriptResource(atHashWithStringNotFinal VehicleModelHash, s32 iRequestFlags);
	~CVehicleScriptResource();

	u32 GetHash() const { return m_VehicleModelHash.GetHash(); }
	bool GetIsStreamedIn() { return m_assetRequesterVehicle.HaveAllLoaded(); }

private:

	void RequestAnimsFromClipSet(const fwMvClipSetId& clipSetId);
	void RequestAnims(s32 iRequestFlags);

	// Streaming request helper
	static const s32 MAX_REQUESTS = 18;
	strRequestArray<MAX_REQUESTS> m_assetRequesterVehicle;
	atHashWithStringNotFinal m_VehicleModelHash;
};

////////////////////////////////////////////////////////////////////////////////

class CVehicleScriptResourceManager
{
public:

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Add a resource
	static s32 RegisterResource(atHashWithStringNotFinal VehicleModelHash, s32 iRequestFlags);

	// Remove a resource
	static void UnregisterResource(atHashWithStringNotFinal VehicleModelHash);

	// Access a resource
	static CVehicleScriptResource* GetResource(atHashWithStringNotFinal VehicleModelHash);

private:

	// Find a resource index
	static s32 GetIndex(atHashWithStringNotFinal VehicleModelHash);

	typedef atArray<CVehicleScriptResource*> VehicleResources;
	static VehicleResources ms_VehicleResources;
};

////////////////////////////////////////////////////////////////////////////////

#endif // VEHICLE_SCRIPT_RESOURCE_H
