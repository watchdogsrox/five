//
// filename:	commands_waypoint.h
// description:	a home for all the commands concerned with wapoint-recordings
//

#ifndef INC_COMMANDS_WAYPOINT_H_
#define INC_COMMANDS_WAYPOINT_H_

// --- Include Files ------------------------------------------------------------


// --- Globals ------------------------------------------------------------------

namespace waypoint_commands
{
	//********************************************
	// Waypoint recording/playback commands

	void CommandRequestWaypointRecording(const char * pRecordingName);
	bool CommandGetIsWaypointRecordingLoaded(const char * pRecordingName);
	void CommandRemoveWaypointRecording(const char * pRecordingName);
	void CommandUseWaypointRecordingAsAssistedMovementRoute(const char * pRecordingName, bool bUse, float fPathWidth, float fTension);
	bool CommandWaypointRecordingGetNumPoints(const char * pRecordingName, int & iNumPoints);
	bool CommandWaypointRecordingGetCoord(const char * pRecordingName, int iPoint, Vector3 & vCoord);
	float CommandWaypointRecordingGetSpeedAtPoint(const char * pRecordingName, int iPoint);
	bool CommandWaypointRecordingGetClosestWaypoint(const char * pRecordingName, const scrVector & scrVecPos, int & iOutWaypoint);
	void CommandTaskFollowWaypointRecording(int iPedID, const char * pRecordingName, int iStartProgress, int iFlags, int iTargetProgress);
	bool CommandIsWaypointPlaybackGoingOnForPed(int PedIndex);
	int CommandGetPedWaypointProgress(int iPedID);
	void CommandSetPedWaypointProgress(int iPedID, int iProgress);
	float CommandGetPedWaypointDistance(int iPedID);
	bool CommandSetPedWaypointRouteOffset(int iPedID, const scrVector & vOffset);
	float CommandGetWaypointDistanceAlongRoute(const char * pRecordingName, int iWaypoint);
	void CommandWaypointPlaybackPause(int iPedID, bool bFacePlayer, bool bStopBeforeOrientating);
	bool CommandWaypointPlaybackGetIsPaused(int iPedID);
	void CommandWaypointPlaybackResume(int iPedID, bool bAchieveHeadingFirst, int iProgressToContinueFrom, int iTimeBeforeResumingMs);
	void CommandWaypointPlaybackUseDefaultSpeed(int iPedID);
	void CommandWaypointPlaybackOverrideSpeed(int iPedID, float fMBR, bool bAllowSlowingForCorners);
	void CommandWaypointPlaybackStartAimingAtPed(int iPedID, int iOtherPedID, bool bUseRunAndGun);
	void CommandWaypointPlaybackStartAimingAtEntity(int iPedID, int iEntityID, bool bUseRunAndGun);
	void CommandWaypointPlaybackStartAimingAtCoord(int iPedID, const scrVector & scrVecCoord, bool bUseRunAndGun);
	void CommandWaypointPlaybackStartShootingAtPed(int iPedID, int iOtherPedID, bool bUseRunAndGun, int FiringPatternHash);
	void CommandWaypointPlaybackStartShootingAtEntity(int iPedID, int iOtherPedID, bool bUseRunAndGun, int FiringPatternHash);
	void CommandWaypointPlaybackStartShootingAtCoord(int iPedID, const scrVector & scrVecCoord, bool bUseRunAndGun, int FiringPatternHash);
	void CommandWaypointPlaybackStopAimingOrShooting(int iPedID);

	//*****************************
	// Assisted movement commands

	void CommandAssistedMovementRequestRoute(const char * pRouteName);
	void CommandAssistedMovementRemoveRoute(const char * pRouteName);
	bool CommandAssistedMovementIsRouteLoaded(const char * pRouteName);

	int CommandAssistedMovementGetRouteProperties(const char * pRouteName);
	void CommandAssistedMovementSetRouteProperties(const char * pRouteName, int iScriptFlags);
	void CommandAssistedMovementOverrideLoadDistanceThisFrame(float fDistance);

	//*****************************
	// Vehicle commands
	void CommandTaskVehicleFollowWaypointRecording(int iPedID, int iVehicleID, const char * pRecordingName, int iDrivingFlags, int iStartProgress, int iFlags, int iTargetProgress, float fMaxSpeed, bool bLoop, float fTargetArriveDistance);
	bool CommandIsWaypointPlaybackGoingOnForVehicle(int iVehicleID);
	int CommandGetVehicleWaypointProgress(int iVehicleID);
	int CommandGetVehicleWaypointTargetPoint(int iVehicleID);
	void CommandVehicleWaypointPlaybackPause(int iVehicleID);
	bool CommandVehicleWaypointPlaybackGetIsPaused(int iVehicleID);
	void CommandVehicleWaypointPlaybackResume(int iVehicleID);
	void CommandVehicleWaypointPlaybackUseDefaultSpeed(int iVehicleID);
	void CommandVehicleWaypointPlaybackOverrideSpeed(int iVehicleID, float fSpeed);

	//void SetupScriptCommands();
}


#endif // !INC_COMMANDS_WAYPOINT_H_
