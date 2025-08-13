<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTacticalAnalysisNavMeshPoints::Tunables" base="CTuning">
    <float name="m_MinDistance" min="0.0f" max="50.0f" init="8.0f" />
    <float name="m_MaxDistance" min="0.0f" max="100.0f" init="42.0f" />
    <float name="m_BufferDistance" min="0.0f" max="5.0f" init="3.0f" />
    <float name="m_MinTimeBetweenAttemptsToFindNewPosition" min="0.0f" max="10.0f" init="2.0f" />
    <float name="m_MinTimeBetweenLineOfSightTests" min="0.0f" max="10.0f" init="2.0f" />
    <float name="m_MinTimeBetweenAttemptsToFindNearby" min="0.0f" max="10.0f" init="3.0f" />
    <float name="m_MaxSearchRadiusForNavMesh" min="0.0f" max="25.0f" init="10.0f" />
    <float name="m_RadiusForFindNearby" min="0.0f" max="10.0f" init="4.0f" />
    <float name="m_MinDistanceBetweenPositionsWithClearLineOfSight" min="0.0f" max="25.0f" init="4.0f" />
    <float name="m_MinDistanceBetweenPositionsWithoutClearLineOfSightInExteriors" min="0.0f" max="25.0f" init="7.0f" />
    <float name="m_MinDistanceBetweenPositionsWithoutClearLineOfSightInInteriors" min="0.0f" max="25.0f" init="4.0f" />
    <float name="m_MaxXYDistanceForNewPosition" min="0.0f" max="5.0f" init="1.0f" />
    <int name="m_MaxNearbyToFindPerFrame" min="1" max="5" init="1" />
  </structdef>
  
	<structdef type="CTacticalAnalysisCoverPointSearch::Tunables::Scoring">
		<float name="m_Occupied" min="0.0f" max="10.0f" init="5.0f" />
		<float name="m_Scripted" min="0.0f" max="10.0f" init="2.0f" />
		<float name="m_PointOnMap" min="0.0f" max="10.0f" init="1.5f" />
		<float name="m_MinDistanceToBeConsideredOptimal" min="0.0f" max="50.0f" init="15.0f" />
		<float name="m_MaxDistanceToBeConsideredOptimal" min="0.0f" max="50.0f" init="25.0f" />
		<float name="m_Optimal" min="0.0f" max="10.0f" init="3.0f" />
	</structdef>
  
	<structdef type="CTacticalAnalysisCoverPointSearch::Tunables" base="CTuning">
		<struct name="m_Scoring" type="CTacticalAnalysisCoverPointSearch::Tunables::Scoring" />
		<int name="m_ScoreCalculationsPerFrame" min="0" max="768" init="128" />
	</structdef>

	<structdef type="CTacticalAnalysisCoverPoints::Tunables" base="CTuning">
		<float name="m_MinDistanceMovedToStartSearch" min="0.0f" max="10.0f" init="5.0f" />
		<float name="m_MaxTimeBetweenSearches" min="0.0f" max="60.0f" init="5.0f" />
		<float name="m_MinDistanceForSearch" min="0.0f" max="100.0f" init="0.0f" />
		<float name="m_MaxDistanceForSearch" min="0.0f" max="100.0f" init="50.0f" />
		<float name="m_MinTimeBetweenLineOfSightTests" min="0.0f" max="10.0f" init="2.0f" />
		<float name="m_MinTimeBetweenAttemptsToFindNearby" min="0.0f" max="10.0f" init="3.0f" />
		<float name="m_MinTimeBetweenStatusUpdates" min="0.0f" max="10.0f" init="5.0f" />
		<float name="m_RadiusForFindNearby" min="0.0f" max="10.0f" init="4.0f" />
		<int name="m_MaxNearbyToFindPerFrame" min="1" max="5" init="1" />
	</structdef>

	<structdef type="CTacticalAnalysis::Tunables::BadRoute">
		<float name="m_ValueForUnableToFind" min="0.0f" max="10.0f" init="1.0f" />
		<float name="m_ValueForTooCloseToTarget" min="0.0f" max="10.0f" init="1.0f" />
		<float name="m_MaxDistanceForTaint" min="0.0f" max="25.0f" init="5.0f" />
		<float name="m_DecayRate" min="0.0f" max="10.0f" init="0.02f" />
	</structdef>

	<structdef type="CTacticalAnalysis::Tunables::Rendering">
		<bool name="m_Enabled" init="false" />
		<bool name="m_CoverPoints" init="true" />
		<bool name="m_NavMeshPoints" init="true" />
		<bool name="m_Position" init="true" />
		<bool name="m_LineOfSightStatus" init="true" />
		<bool name="m_ArcStatus" init="false" />
		<bool name="m_Reserved" init="true" />
		<bool name="m_Nearby" init="true" />
		<bool name="m_BadRouteValue" init="false" />
		<bool name="m_Reservations" init="false" />
		<bool name="m_LineOfSightTests" init="false" />
	</structdef>
  
	<structdef type="CTacticalAnalysis::Tunables" base="CTuning">
		<struct name="m_BadRoute" type="CTacticalAnalysis::Tunables::BadRoute" />
		<struct name="m_Rendering" type="CTacticalAnalysis::Tunables::Rendering" />
		<float name="m_MaxSpeedToActivate" min="0.0f" max="50.0f" init="8.0f" />
		<float name="m_MinSpeedToDeactivate" min="0.0f" max="50.0f" init="10.0f" />
		<float name="m_MaxTimeWithNoReferences" min="0.0f" max="30.0f" init="10.0f" />
		<bool name="m_Enabled" init="false" />
	</structdef>

</ParserSchema>
