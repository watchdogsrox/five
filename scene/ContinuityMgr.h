//
// scene/ContinuityMgr.h
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#ifndef _SCENE_CONTINUITYMGR_H_
#define _SCENE_CONTINUITYMGR_H_

#include "vectormath/vec3v.h"

#define LODFILTER_DRIVING (0)

// simple class to track major changes in camera behavior etc, used for driving various aspects of lod transitions etc
class CContinuityState
{
public:
	void Reset();
	void Update();
	bool DiffersSignificantly(CContinuityState& newState);

	enum eSceneStreamingPressure
	{
		SCENE_STREAMING_PRESSURE_LIGHT,
		SCENE_STREAMING_PRESSURE_MODERATE,
		SCENE_STREAMING_PRESSURE_HEAVY,
		SCENE_STREAMING_PRESSURE_VERY_HEAVY,

		SCENE_STREAMING_PRESSURE_TOTAL
	};

private:
	Vec3V		m_vPos;
	Vec3V		m_vDir;
	float		m_fVelocity;
	float		m_fLodScale;
	bool		m_bAimingFirstPerson;
	bool		m_bInCarView;
	bool		m_bUsingNonGameCamera;
	bool		m_bSwitching;
	bool		m_bIsPlayerOnFoot;
	bool		m_bIsPlayerAiming;
	bool		m_bIsPlayerFlyingAircraft;
	bool		m_bIsPlayerDrivingTrain;
	bool		m_bAboveHeightMap;
	bool		m_bIsPlayerDrivingGroundVehicle;
	bool		m_bIsPlayerInCityArea;
	bool		m_bIsPlayerSkyDiving;
	bool		m_bLookingBackwards;
	bool		m_bLookingAwayFromDirectionOfMovement;
	bool		m_bIsAnyFirstPersonCamera;
	bool		m_bSceneStreamingUnderVeryHeavyPressure;
	float		m_fHighestRequiredSceneScore;
	eSceneStreamingPressure m_eStreamingPressure;

	friend class CContinuityMgr;
};

// interface for querying state changes
class CContinuityMgr
{
public:
	void Init();
	void Update();

	bool HasMajorCameraChangeOccurred() const { return m_bMajorChangeOccurred; }
	bool HasLodScaleChangeOccurred() const { return m_bLodScaleChangeOccurred; }
	bool UsingNonGameCamera() const { return m_currentState.m_bUsingNonGameCamera; }
	bool ModerateSpeedMovement() const { return ( m_currentState.m_fVelocity>=28.0f ); }
	bool HighSpeedMovement() const { return ( m_currentState.m_fVelocity>=40.0f ); }
	bool HighSpeedFlying() const { return ( m_currentState.m_bIsPlayerFlyingAircraft && HighSpeedMovement() ); }
	bool HighSpeedTrainDriving() const { return ( m_currentState.m_bIsPlayerDrivingTrain && HighSpeedMovement() ); }
	bool HighSpeedDriving() const { return ( m_currentState.m_bIsPlayerDrivingGroundVehicle && HighSpeedMovement() ); }
	bool HighSpeedFalling() const { return ( m_currentState.m_bIsPlayerSkyDiving && HighSpeedMovement() ); }
	bool ModerateSpeedDriving() const { return ( m_currentState.m_bIsPlayerDrivingGroundVehicle && ModerateSpeedMovement() ); }

	bool IsAboveHeightMap() const { return m_currentState.m_bAboveHeightMap; }
	bool IsFlyingAircraft() const { return m_currentState.m_bIsPlayerFlyingAircraft; }
	bool IsDrivingTrain() const { return m_currentState.m_bIsPlayerDrivingTrain; }
	bool IsDrivingGroundVehicle() const { return m_currentState.m_bIsPlayerDrivingGroundVehicle; }
	bool IsSkyDiving() const { return m_currentState.m_bIsPlayerSkyDiving; }
	bool IsPlayerInCity() const { return m_currentState.m_bIsPlayerInCityArea; }
	bool IsPlayerOnFoot() const { return m_currentState.m_bIsPlayerOnFoot; }
	bool IsPlayerAiming() const { return m_currentState.m_bIsPlayerAiming; }
	bool IsAnyFirstPersonCamera() const { return m_currentState.m_bIsAnyFirstPersonCamera; }
	bool IsUsingSniperOrTelescope() const { return m_currentState.m_bAimingFirstPerson; }
	bool IsLookingBackwards() const { return m_currentState.m_bLookingBackwards; }
	bool IsLookingAwayFromDirectionOfMovement() const { return m_currentState.m_bLookingAwayFromDirectionOfMovement; }

	float GetPlayerVelocity() const { return m_currentState.m_fVelocity; }

	float GetHighestRequiredSceneScore() const { return m_currentState.m_fHighestRequiredSceneScore; }
	bool IsSceneStreamingUnderHeavyPressure() const { return m_currentState.m_bSceneStreamingUnderVeryHeavyPressure; }
	CContinuityState::eSceneStreamingPressure GetSceneStreamingPressure() const { return m_currentState.m_eStreamingPressure; }

private:
	bool m_bMajorChangeOccurred;
	bool m_bLodScaleChangeOccurred;
	CContinuityState m_currentState;
};

extern CContinuityMgr g_ContinuityMgr;

#endif	//_SCENE_CONTINUITYMGR_H_
