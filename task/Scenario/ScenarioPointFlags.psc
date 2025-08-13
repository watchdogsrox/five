<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

 <enumdef type="CScenarioPointFlags::Flags" generate="bitset"> 
	<enumval name="IgnoreMaxInRange"/>
	<enumval name="NoSpawn"/>
	<enumval name="StationaryReactions"/>
	<enumval name="OnlySpawnInSameInterior"/>
	<enumval name="SpawnedPedIsArrestable"/>
	<enumval name="ActivateVehicleSiren"/>
	<enumval name="AggressiveVehicleDriving"/>
	<enumval name="LandVehicleOnArrival"/>
	<enumval name="IgnoreThreatsIfLosNotClear"/>
	<enumval name="EventsInRadiusTriggerDisputes"/>
	<enumval name="AerialVehiclePoint"/>
	<enumval name="TerritorialScenario"/>
	<enumval name="EndScenarioIfPlayerWithinRadius"/>
	<enumval name="EventsInRadiusTriggerThreatResponse"/>
	<enumval name="TaxiPlaneOnGround"/>
	<enumval name="FlyOffToOblivion"/>
	<enumval name="InWater"/>
	<enumval name="AllowInvestigation"/>
	<enumval name="OpenDoor"/>
    <enumval name="PreciseUseTime"/>
	<enumval name="NoRespawnUntilStreamedOut"/>
	<enumval name="NoVehicleSpawnMaxDistance"/>
	<enumval name="ExtendedRange"/>
	<enumval name="ShortRange"/>
	<enumval name="HighPriority"/>
	<enumval name="IgnoreLoitering"/>
	<enumval name="UseSearchlight"/>
	<enumval name="ResetNoCollisionOnCleanUp"/>
	<enumval name="CheckCrossedArrivalPlane"/>
    <enumval name="UseVehicleFrontForArrival" />
	<enumval name="IgnoreWeatherRestrictions" />
</enumdef>

<enumdef type="CScenarioPointRuntimeFlags::Flags" generate="bitset">
	<enumval name="ChainIsFull" description="True if the points chain can accept no more users."/>
	<enumval name="EntityOverridePoint" description="True if this is an entity-attached point that came in through the scenario regions, rather than from the map data."/>
	<enumval name="FromRsc" description="True if the point is from a resourced data ... (ie region data, entity model data ... NOT script or code created)."/>
	<enumval name="HasExtraData" description="True if we have created a CScenarioPointExtraData object to be associated with this point."/>
	<enumval name="HasHistory" description="True if CScenarioSpawnHistory::Add called .. the idea here is to prevent other objects from spawning at this point unless the previous object is out of range."/>
	<enumval name="IsChained" description="True if the point is part of a chain this is done to allow the world to know that the point is chained to check/toggle chain usage"/>
	<enumval name="IsInCluster" description="True if the point is part of a cluster"/>
	<enumval name="OnAddCalled" description="True if CScenarioManager::OnAddScenario() has been called (without a matching OnRemoveScenario())."/>
	<enumval name="Obstructed" description="True if this point has been marked as obstructed"/>
</enumdef>
	
</ParserSchema>
