<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CDispatchSpawnHelper::Tunables::Rendering" >
		<bool name="m_Enabled" init="false"/>
		<bool name="m_DispatchNode" init="true"/>
		<bool name="m_FindSpawnPointInDirection" init="true"/>
		<bool name="m_IncidentLocation" init="true"/>
	</structdef>
	
	<structdef type="CDispatchSpawnHelper::Tunables::Restrictions" >
		<float name="m_MaxDistanceFromCameraForViewportChecks" min="0.0f" max="500.0f" step="1.0f" init="150.0f"/>
		<float name="m_RadiusForViewportCheck" min="0.0f" max="25.0f" step="1.0f" init="5.0f"/>
	</structdef>

	<structdef type="CDispatchSpawnHelper::Tunables"  base="CTuning" >
		<struct name="m_Restrictions" type="CDispatchSpawnHelper::Tunables::Restrictions"/>
		<struct name="m_Rendering" type="CDispatchSpawnHelper::Tunables::Rendering"/>
		<float name="m_IdealSpawnDistance" min="0.0f" max="500.0f" step="1.0f" init="250.0f"/>
		<float name="m_MinDotForInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
		<float name="m_MaxDistanceTraveledMultiplier" min="1.0f" max="5.0f" step="0.1f" init="3.0f"/>
		<float name="m_MinSpeedToBeConsideredEscapingInVehicle" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
		<float name="m_MaxDistanceForDispatchPosition" min="0.0f" max="100.0f" step="1.0f" init="30.0f"/>
	</structdef>

	<structdef type="CDispatchAdvancedSpawnHelper::Tunables::Rendering" >
		<bool name="m_Enabled" init="false"/>
	</structdef>
	
	<structdef type="CDispatchAdvancedSpawnHelper::Tunables"  base="CTuning" >
		<struct name="m_Rendering" type="CDispatchAdvancedSpawnHelper::Tunables::Rendering"/>
		<float name="m_TimeBetweenInvalidateInvalidDispatchVehicles" min="0.0f" max="60.0f" step="1.0f" init="2.0f"/>
		<float name="m_TimeBetweenMarkDispatchVehiclesForDespawn" min="0.0f" max="60.0f" step="1.0f" init="3.0f"/>
	</structdef>

	<structdef type="CDispatchHelperSearch::Constraints">
		<float name="m_MinTimeSinceLastSpotted" min="0.0f" max="300.0f" step="1.0f" init="0.0f"/>
		<float name="m_MaxTimeSinceLastSpotted" min="0.0f" max="300.0f" step="1.0f" init="120.0f"/>
		<float name="m_MaxRadiusForMinTimeSinceLastSpotted" min="0.0f" max="300.0f" step="1.0f" init="100.0f"/>
		<float name="m_MaxRadiusForMaxTimeSinceLastSpotted" min="0.0f" max="300.0f" step="1.0f" init="15.0f"/>
		<float name="m_MaxHeight" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
		<float name="m_MaxAngle" min="0.0f" max="3.14f" step="0.01f" init="3.14f"/>
		<bool name="m_UseLastSeenPosition" init="false"/>
		<bool name="m_UseByDefault" init="false"/>
		<bool name="m_UseEnclosedSearchRegions" init="false"/>
	</structdef>

	<structdef type="CDispatchHelperSearchOnFoot::Tunables"  base="CTuning">
		<array name="m_Constraints" type="atArray">
			<struct type="CDispatchHelperSearch::Constraints"/>
		</array>
		<float name="m_MaxDistanceFromNavMesh" min="0.0f" max="100.0f" step="1.0f" init="40.0f"/>
	</structdef>

	<structdef type="CDispatchHelperSearchInAutomobile::Tunables"  base="CTuning">
		<array name="m_Constraints" type="atArray">
			<struct type="CDispatchHelperSearch::Constraints"/>
		</array>
		<float name="m_MaxDistanceFromRoadNode" min="0.0f" max="100.0f" step="1.0f" init="40.0f"/>
		<float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
	</structdef>

	<structdef type="CDispatchHelperSearchInBoat::Tunables"  base="CTuning">
		<array name="m_Constraints" type="atArray">
			<struct type="CDispatchHelperSearch::Constraints"/>
		</array>
    <float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="30.0f"/>
	</structdef>

	<structdef type="CDispatchHelperSearchInHeli::Tunables"  base="CTuning">
		<array name="m_Constraints" type="atArray">
			<struct type="CDispatchHelperSearch::Constraints"/>
		</array>
	</structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::Rendering">
		<bool name="m_Enabled" init="false"/>
		<bool name="m_LocationForNearestCarNodeOverrides" init="true"/>
		<bool name="m_EnclosedSearchRegions" init="true"/>
		<bool name="m_BlockingAreas" init="true"/>
	</structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::Position">
		<float name="m_X" init="0.0f"/>
		<float name="m_Y" init="0.0f"/>
		<float name="m_Z" init="0.0f"/>
	</structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::AngledArea">
		<struct name="m_Start" type="CDispatchHelperVolumes::Tunables::Position"/>
		<struct name="m_End" type="CDispatchHelperVolumes::Tunables::Position"/>
		<float name="m_Width" init="0.0f"/>
	</structdef>

  <structdef type="CDispatchHelperVolumes::Tunables::AngledAreaWithPosition">
    <struct name="m_AngledArea" type="CDispatchHelperVolumes::Tunables::AngledArea"/>
    <struct name="m_Position" type="CDispatchHelperVolumes::Tunables::Position"/>
  </structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::Sphere">
		<struct name="m_Position" type="CDispatchHelperVolumes::Tunables::Position"/>
		<float name="m_Radius" init="0.0f"/>
	</structdef>

  <structdef type="CDispatchHelperVolumes::Tunables::SphereWithPosition">
    <struct name="m_Sphere" type="CDispatchHelperVolumes::Tunables::Sphere"/>
    <struct name="m_Position" type="CDispatchHelperVolumes::Tunables::Position"/>
  </structdef>

  <structdef type="CDispatchHelperVolumes::Tunables::LocationForNearestCarNodeOverrides">
    <array name="m_AngledAreas" type="atArray">
      <struct type="CDispatchHelperVolumes::Tunables::AngledAreaWithPosition"/>
    </array>
    <array name="m_Spheres" type="atArray">
      <struct type="CDispatchHelperVolumes::Tunables::SphereWithPosition"/>
    </array>
  </structdef>

	<enumdef type="CDispatchHelperVolumes::Tunables::EnclosedSearchRegionFlags">
		<enumval name="UseAABB"/>
	</enumdef>

	<structdef type="CDispatchHelperVolumes::Tunables::EnclosedSearchAngledArea">
		<struct name="m_AngledArea" type="CDispatchHelperVolumes::Tunables::AngledArea"/>
		<bitset name="m_Flags" type="fixed32" values="CDispatchHelperVolumes::Tunables::EnclosedSearchRegionFlags"/>
	</structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::EnclosedSearchSphere">
		<struct name="m_Sphere" type="CDispatchHelperVolumes::Tunables::Sphere"/>
		<bitset name="m_Flags" type="fixed32" values="CDispatchHelperVolumes::Tunables::EnclosedSearchRegionFlags"/>
	</structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::EnclosedSearchRegions">
    <array name="m_AngledAreas" type="atArray">
      <struct type="CDispatchHelperVolumes::Tunables::EnclosedSearchAngledArea"/>
    </array>
    <array name="m_Spheres" type="atArray">
      <struct type="CDispatchHelperVolumes::Tunables::EnclosedSearchSphere"/>
    </array>
  </structdef>

	<structdef type="CDispatchHelperVolumes::Tunables::BlockingAreas">
		<array name="m_AngledAreas" type="atArray">
			<struct type="CDispatchHelperVolumes::Tunables::AngledArea"/>
		</array>
		<array name="m_Spheres" type="atArray">
			<struct type="CDispatchHelperVolumes::Tunables::Sphere"/>
		</array>
	</structdef>

	<structdef type="CDispatchHelperVolumes::Tunables"  base="CTuning">
		<struct name="m_Rendering" type="CDispatchHelperVolumes::Tunables::Rendering"/>
		<struct name="m_LocationForNearestCarNodeOverrides" type="CDispatchHelperVolumes::Tunables::LocationForNearestCarNodeOverrides"/>
		<struct name="m_EnclosedSearchRegions" type="CDispatchHelperVolumes::Tunables::EnclosedSearchRegions"/>
		<struct name="m_BlockingAreas" type="CDispatchHelperVolumes::Tunables::BlockingAreas"/>
	</structdef>

</ParserSchema>
