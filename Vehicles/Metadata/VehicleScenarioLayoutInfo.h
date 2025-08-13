#ifndef INC_VEHICLE_SCENARIO_LAYOUT_H
#define INC_VEHICLE_SCENARIO_LAYOUT_H

// Rage headers
#include "atl/hashstring.h"
#include "fwtl/Pool.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "scene/loader/MapData_Extensions.h"

//-------------------------------------------------------------------------
// Class which contains a vehicle's scenario point layout
//-------------------------------------------------------------------------
class CVehicleScenarioLayoutInfo
{
public:

	// Returns the name of this layout
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// Scenario point info accessors
	/////////////////////////////

	// Get the number of scenario points in this layout
	s32 GetNumScenarioPoints() const			{ return m_ScenarioPoints.GetCount(); }

	// Get a scenario point info by index
	const CExtensionDefSpawnPoint* GetScenarioPointInfo(s32 iIndex) const;

private:

	// Layout name
	atHashWithStringNotFinal			m_Name;

	// Scenario point information
	atArray<CExtensionDefSpawnPoint>	m_ScenarioPoints;

	PAR_SIMPLE_PARSABLE
};

#endif // INC_VEHICLE_SCENARIO_LAYOUT_H