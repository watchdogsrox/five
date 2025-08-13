#ifndef WAYPOINT_RECORDING_ROUTE_H
#define WAYPOINT_RECORDING_ROUTE_H

#include "fwcontrol\WaypointRecordingRoute.h"

//************************************************************
// CWaypointRecordingRoute

class CWaypointRecordingRoute : public fwWaypointRecordingRoute
{
#if __BANK
public:
	bool AddWaypoint(const Vector3 & vPos, const float fSpeed, const float fHeading, const u16 iFlags);
	bool InsertWaypoint(const int iIndex, const Vector3 & vPos, const float fSpeed, const float fHeading, const u16 iFlags);
	void RemoveWaypoint(const int iWpt);
	void AllocForRecording();
	bool SanityCheck();
	void Draw(const bool bRecordingRoute);
#endif
};

#endif // WAYPOINT_RECORDING_ROUTE_H