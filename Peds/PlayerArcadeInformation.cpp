// Title	:	PlayerArcadeInformation.cpp
// Author	:	Stephen Phillips (Ruffian Games)
// Started	:	04/03/20

#include "PlayerArcadeInformation.h"

CPlayerArcadeInformation::CPlayerArcadeInformation() :
	m_team(eArcadeTeam::AT_NONE),
	m_role(eArcadeRole::AR_NONE),
	m_CNCVOffender(false),
	m_activeVehicle(NULL)
{
}

eArcadeTeam CPlayerArcadeInformation::GetTeam() const
{
	return m_team;
}

eArcadeRole CPlayerArcadeInformation::GetRole() const
{
	return m_role;
}

void CPlayerArcadeInformation::SetTeamAndRole(eArcadeTeam team, eArcadeRole role)
{
	m_team = team;
	m_role = role;
}
