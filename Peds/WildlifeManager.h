//
// Peds/WildlifeManager.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef WILDLIFE_MANAGER_H
#define WILDLIFE_MANAGER_H

#include "modelinfo/PedModelInfo.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "Task/System/AsyncProbeHelper.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
 
namespace rage
{
	class Vector3;
}

//***************************************
//Class:  CTWildlifeManager
//Purpose:  Manage spawnpoints for birds and fish.
//***************************************

class CWildlifeManager
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		// Aerial spawning constants.
		float m_BirdHeightMapDeltaMin;
		float m_BirdHeightMapDeltaMax;
		float m_BirdSpawnXYRangeMin;
		float m_BirdSpawnXYRangeMax;
		float m_IncreasedAerialSpawningFactor;

		// Ground wildlife spawning constants.
		float m_MinDistanceToSearchForGroundWildlifePoints;
		float m_MaxDistanceToSearchForGroundWildlifePoints;
		float m_TimeBetweenGroundProbes;
		float m_GroundMaterialProbeDepth;
		float m_GroundMaterialProbeOffset;
		float m_GroundMaterialSpawnCoordNormalZTolerance;
		float m_IncreasedGroundWildlifeSpawningFactor;
		
		// Aquatic spawning constants.
		float m_MinDistanceToSearchForAquaticPoints;
		float m_MaxDistanceToSearchForAquaticPoints;
		float m_TimeBetweenWaterHeightMapChecks;
		float m_TimeBetweenWaterProbes;
		float m_WaterProbeDepth;
		float m_WaterProbeOffset;  // How much to shift the probe upwards from where the fish will actually spawn to try and catch obstructions above.
		float m_AquaticSpawnDepth;
		float m_AquaticSpawnMaxHeightAbovePlayer;
		float m_IncreasedAquaticSpawningFactor;
		float m_CloseSpawningViewMultiplier;

		// Forced shark spawning constants.
		atHashString	m_SharkModelName;
		float			m_DeepWaterThreshold;
		float			m_PlayerSwimTimeThreshold;
		u32				m_MinTimeBetweenSharkDispatches;
		float			m_SharkAddRangeInViewMin;
		float			m_SharkAddRangeInViewMinFar;
		float			m_SharkSpawnProbability;

		PAR_PARSABLE;
	};

	static void Init();

	static void Shutdown();

	static CWildlifeManager& GetInstance();

	// Update that status of the wildlife manager.
	void Process(bool bCachedPedGenPointsRemaining);

	// Reset script dependent variables in the wildlife system.
	void ProcessPreScripts();

	// Returns true if the modelIndex prefers to spawn in the air.
	bool IsFlyingModel(u32 modelIndex) const;

	// Returns true if the modelIndex prefers to spawn in the water.
	bool IsAquaticModel(u32 modelIndex) const;

	// Returns true if the modelIndex prefers to spawn as ground wildlife.
	bool IsGroundWildlifeModel(u32 modelIndex) const;

	// Returns true if the spawnpoint is inside the world extents AABB.
	bool IsSpawnPointInsideWorldLimits(Vec3V_ConstRef vPoint);

	// Return true if a valid spawn coordinate for the flying ped was generated
	bool GetFlyingPedCoord(Vector3& newPedGroundPosOut, bool& bNewPedGroundPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float fAddRangOutOfViewMin, float pedRadius, bool doInFrustumTest, u32 pedModelIndex);

	// Return true if a valid spawn coordinate for a ground wildlife specimen was generated.
	bool GetGroundWildlifeCoord(Vector3& newPedGroundPosOut, bool& bNewPedGroundPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float pedRadius, bool doInFrustumTest, u32 pedModelIndex);

	// Return true if a valid spawn coordinate for water spawning was generated.
	bool GetWaterSpawnCoord(Vector3& newWaterPositionOut, bool& bNewWaterPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float pedRadius, bool doInFrustumTest, u32 pedModelIndex);

	// Return true if more fish should try and be spawned (the player is in water).
	bool ShouldSpawnMoreAquaticPeds() const;

	// Return true if more birds should try and be spawned (there are no pedgen coords available to spawn other peds).
	bool ShouldSpawnMoreAerialPeds() const;

	// Return true if more ground wildlife peds should try and be spawned (there are no pedgen coords available to spawn other peds).
	bool ShouldSpawnMoreGroundWildlifePeds() const;

	// Perform extra checks that need to be done in multiplayer.
	bool CheckWildlifeMultiplayerSpawnConditions(u32 pedModelIndex) const;

	// Check if a model should be marked for streaming preferences.
	void ManageAquaticModelStreamingConditions(u32 modelIndex);

	// Go through the models and see if any special conditions have expired.
	void ProcessSpecialStreamingConditions();

	// Internally controlled tunables, allowing the system to vary how wildlife are spawned in when the game situation dictates we need more wildlife.
	float GetIncreasedAquaticSpawningFactor()			{ return m_LastKindOfWildlifeSpawned == DSP_AQUATIC ? sm_Tunables.m_IncreasedAquaticSpawningFactor - 1.0f : sm_Tunables.m_IncreasedAquaticSpawningFactor; };
	float GetIncreasedAerialSpawningFactor()			{ return m_LastKindOfWildlifeSpawned == DSP_AERIAL ? sm_Tunables.m_IncreasedAerialSpawningFactor - 1.0f : sm_Tunables.m_IncreasedAerialSpawningFactor; }
	float GetIncreasedGroundWildlifeSpawningFactor()	{ return m_LastKindOfWildlifeSpawned == DSP_GROUND_WILDLIFE ? sm_Tunables.m_IncreasedGroundWildlifeSpawningFactor - 1.0f : sm_Tunables.m_IncreasedGroundWildlifeSpawningFactor; }

	// Spawn a shark near the player.  Returns true if a shark was successfully spawned.
	bool DispatchASharkNearPlayer(bool bTellItToKillThePlayer, bool bForceToCameraPosition=false);

	// External query functions for shark spawning.

	// Has the game successfully spawned a shark?
	bool HasSpawnedAShark();
	
	// Has the shark model been requested?
	bool IsStreamingAShark();

	// Can any shark models spawn?
	bool CanSpawnSharkModels();

	// Search the player's close entity list and see if any sharks are around.
	bool AreThereAnySharksNearThePlayer() const;

	// Accessors for bird flight.
	void SetScriptMinBirdHeight(float fMinBirdFlightHeight) { m_fScriptForcedBirdMinFlightHeight = fMinBirdFlightHeight; }
	float GetScriptMinBirdHeight() const { return m_fScriptForcedBirdMinFlightHeight; }

private:
	CWildlifeManager()	{};
	~CWildlifeManager()	{};

private:

	//Helper functions

	// Shoot a probe looking for suitable habitat for ground wildlife.
	void FireGroundWildlifeProbe();

	// Check to see if the ground asynchronous probe found anything that supports wildlife.
	void CheckGroundWildlifeProbe();

	void FindWaterProbeSpot();

	// Shoot a probe looking for suitable habitat for water spawning.
	void FireWaterProbe();

	// Check to see if the water probe was clear for spawning.
	void CheckWaterProbe();

	// Given a material, return true if that material is suitable to spawn wildlife on.
	bool MaterialSupportsWildlife(const phMaterial& rHitMaterial);

	// Add a point that is known to support ground wildlife to the pool of such points.
	void AddGroundPointToCollection(const Vector3& vPoint);

	// Add a point that is known to be suitable for water spawning to the pool of such points.
	void AddWaterPointToCollection(const Vector3& vPoint);

	// Generate a random point near the player.
	// Set the Z-coordinate equal to the heightmap + some delta at that location.
	void GeneratePossibleFlyingCoord(Vector3& pCoord);

	// If in-camera view tests are enabled, return false if the camera can see the position
	bool TestAerialSpawnPoint(Vec3V_In vCenter, Vec3V_In vCoord, float pedRadius, float fAddRangeInViewMin, float fAddRangeOutOfViewMin, bool doInFrustumTest);

	// If the player has been swimming in deep enough water for a while.
	bool ShouldDispatchAShark();

	// Return the tunables.
	static const Tunables& GetTunables()				{ return sm_Tunables; }


private:

	// Singleton instance
	static CWildlifeManager m_WildlifeManagerInstance;
	
	// Tunables
	static Tunables sm_Tunables;


	// How many times to keep trying to generate a valid spawn coordinate before returning a failure
	static const u32 MAX_NUM_AIR_SPAWN_TRIES = 10;

	static const u32 MAX_NUM_AQUATIC_SPAWN_TRIES = 10;

	static const u32 MAX_NUM_GROUND_WILDLIFE_POINTS = 5;

	static const u32 MAX_NUM_AQUATIC_SPAWN_POINTS = 5;

	static const u32 MAX_NUM_MANAGED_AQUATIC_MODELS = 2;

	atQueue<Vector3, MAX_NUM_GROUND_WILDLIFE_POINTS> m_aGroundWildlifePoints;

	atQueue<Vector3, MAX_NUM_AQUATIC_SPAWN_POINTS> m_aAquaticSpawnPoints;

	// Arrays of models given streaming priority in certain situations.
	atFixedArray<u32, MAX_NUM_MANAGED_AQUATIC_MODELS> m_aManagedAquaticModels;

	float					m_fTimeSinceLastGroundProbe;

	float					m_fTimeSinceLastWaterHeightCheck;

	float					m_fTimeSinceLastWaterProbe;

	float					m_fPlayerDeepWaterTimer;

	float					m_fScriptForcedBirdMinFlightHeight;

	u32						m_uLastSharkSpawn;

	Vector3					m_vGoodWaterProbeSpot;

	bool					m_bHasGoodWaterProbeSpot;

	CAsyncProbeHelper		m_GroundProbeHelper;

	CAsyncProbeHelper		m_WaterProbeHelper;

	CAsyncProbeHelper		m_SharkDepthProbeHelper;

	DefaultSpawnPreference	m_LastKindOfWildlifeSpawned;

	bool					m_bCachedPedGenPointsAvailable;

	bool					m_bCanSpawnSharkModels;
};

#endif //end ifndef WILDLIFE_MANAGER_H
