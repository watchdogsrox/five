// Title	:	PlayerArcadeInformation.h
// Author	:	Stephen Phillips (Ruffian Games)
// Started	:	04/03/20

/*
 Player information relating to arcade mode.
*/

#pragma once

#define ARCHIVED_SUMMER_CONTENT_ENABLED 0

#define CNC_MODE_ENABLED 0
#if CNC_MODE_ENABLED
#define CNC_MODE_ENABLED_ONLY(...)  			__VA_ARGS__
#else
#define CNC_MODE_ENABLED_ONLY(...)
#endif // CNC_MODE_ENABLED

#include "scene/RegdRefTypes.h"

class CVehicle;

enum class eArcadeTeam
{
	AT_NONE,

	AT_CNC_COP,
	AT_CNC_CROOK,

	AT_NUM_TYPES
};

enum class eArcadeRole
{
	AR_NONE,

	// C&C Cop roles
	AR_CNC_OFFICER,
	AR_CNC_TACTICAL,
	AR_CNC_TECH,

	// C&C Crook roles
	AR_CNC_SOLDIER,
	AR_CNC_ENFORCER,
	AR_CNC_HACKER,

	AR_NUM_TYPES
};

class CPlayerArcadeInformation
{
public:
	CPlayerArcadeInformation();

	eArcadeTeam GetTeam() const;
	eArcadeRole GetRole() const;
	void SetTeamAndRole(eArcadeTeam team, eArcadeRole role);

	bool GetCNCVOffender() const { return m_CNCVOffender; }
	void SetCNCVOffender(bool newValue) { m_CNCVOffender = newValue; }

	const CVehicle* GetActiveVehicle() const { return m_activeVehicle.Get(); }
	void SetActiveVehicle(CVehicle* pVehicle) { m_activeVehicle = pVehicle; }

private:
	eArcadeTeam m_team;
	eArcadeRole m_role;

	bool m_CNCVOffender;

	// the player's active vehicle 
	// this is not a networked variable, use for the local player only
	RegdVeh m_activeVehicle;
};
