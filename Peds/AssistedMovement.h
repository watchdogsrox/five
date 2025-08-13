// Title	:	AssistedMovement.h
// Author	:	James Broad
// Started	:	18/02/09
// System for helping the player movement by guiding him gently
// along 'rails' which have been placed in the map.

#ifndef _ASSISTEDMOVEMENT_H_
#define _ASSISTEDMOVEMENT_H_

#include "AssistedMovementStore.h"
#include "physics/WorldProbe/worldprobe.h"

#define __PLAYER_ASSISTED_MOVEMENT							1
#define __PLAYER_ASSISTED_MOVEMENT_WHILST_STRAFING			0

#if __PLAYER_ASSISTED_MOVEMENT

// Flags used via 'ASSISTED_MOVEMENT_SET_ROUTE_PROPERTIES' command
enum eAssistedMovementScriptFlags
{
	EASSISTED_ROUTE_ACTIVE_WHEN_STRAFING						= 2,
	EASSISTED_ROUTE_DISABLE_IN_FORWARDS_DIRECTION				= 4,
	EASSISTED_ROUTE_DISABLE_IN_REVERSE_DIRECTION				= 8
};

//*************************************************************************************
//	CPlayerAssistedMovementControl
//	The class which processes the assisted movement
class CPlayerAssistedMovementControl
{
public:
	CPlayerAssistedMovementControl();
	~CPlayerAssistedMovementControl();

	void Process(CPed * pPed, Vector2 vecStickInput);

	bool GetDesiredHeading(CPed * pPlayer, float & fOutDesiredHeading, float & fOutDesiredPitch, float & fOutDesiredMBR);

	void ScanForSnapToRoute(CPed * pPed, const bool bForceScan=false);
	inline const CAssistedMovementRoute & GetRoute() const { return m_Route; }
	inline void SetScanForRoutesFrequency(float f) {m_fScanForRoutesFreq = Max(0.0f, f);}
	inline void SetUseWideLanes(bool b) {m_bUseWideLanes = b;}

	inline void SetJustTeleported() { m_fTeleportedRescanTimer = 4.0f; }
	inline void ClearCurrentRoute() { m_Route.Clear(); }

	inline bool GetIsUsingRoute() const { return m_bIsUsingRoute; }

	inline const Vector3 & GetLookAheadRouteDirection() const { return m_vLookAheadRouteDirection; }

	inline float GetLocalRouteCurvature() const { return m_fLocalRouteCurvature; }

	CAssistedMovementRoute m_Route;

	bool m_bIsUsingRoute;
	bool m_bWasOnStickyRoute;
	bool m_bOnStickyRoute;
	bool m_bUseWideLanes;
	float m_fTimeUntilNextScanForRoutes;
	float m_fTeleportedRescanTimer;
	float m_fLocalRouteCurvature;
	float m_fScanForRoutesFreq;

	Vector3 m_vLookAheadRouteDirection;

	// Tweakable vars
	static bank_bool ms_bAssistedMovementEnabled;
	static bank_bool ms_bRouteScanningEnabled;
	static bank_bool ms_bLoadAllRoutes;
	static bank_bool ms_bDrawRouteStore;	
	static bank_float ms_fDefaultTeleportedRescanTime;

#if __BANK
	static void AddPointAtPlayerPos();
	static void AddPointAtCameraPos();
	static void AddPoint(const Vector3 & vPos);
	static void ClearPoints();
	static void DumpScript();

	void Debug(const CPed * pPed) const;
#endif

#if __DEV
	static bool ms_bDebugOutCapsuleHits;
#endif

private:

	void UpdateLocalCurvatureAndLookAheadDirection(Vec3V_In vPlayerPosition, Vec3V_In vTangentForceDir, float closestDistAlong, bool bProcess3d);

	bool CanIgnoreBlockingIntersection(WorldProbe::CShapeTestResults * pShapeTestResults) const;
};

#endif	// __PLAYER_ASSISTED_MOVEMENT



#endif	// _ASSISTEDMOVEMENT_H_
