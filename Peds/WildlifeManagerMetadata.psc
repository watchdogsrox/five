<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CWildlifeManager::Tunables" base="CTuning">
    <float name="m_BirdHeightMapDeltaMin" min="0.0f" max="100.0f" init="3.0f"/>
    <float name="m_BirdHeightMapDeltaMax" min="0.0f" max="100.0f" init="6.0f"/>
    <float name="m_BirdSpawnXYRangeMin" min="0.0f" max="100.0f" init="15.0f"/>
    <float name="m_BirdSpawnXYRangeMax" min="0.0f" max="300.0f" init="40.0f"/>
    <float name="m_IncreasedAerialSpawningFactor" min="0.0f" max="100.0f" init="20.0f"/>
    <float name="m_MinDistanceToSearchForGroundWildlifePoints" min="0.0f" max="100.0f" init="30.0f"/>
    <float name="m_MaxDistanceToSearchForGroundWildlifePoints" min="0.0f" max="100.0f" init="60.0f"/>
    <float name="m_TimeBetweenGroundProbes" min="0.0f" max="10.0f" init="3.0f"/>
    <float name="m_GroundMaterialProbeDepth" min="0.0f" max="10.0f" init="5.0f"/>
    <float name="m_GroundMaterialProbeOffset" min="0.0f" max="10.0f" init="2.0f"/>
    <float name="m_GroundMaterialSpawnCoordNormalZTolerance" min="0.0f" max="1.0f" init="0.96f"/>
    <float name="m_MinDistanceToSearchForAquaticPoints" min="0.0f" max="100.0f" init="30.0f"/>
    <float name="m_MaxDistanceToSearchForAquaticPoints" min="0.0f" max="100.0f" init="60.0f"/>
    <float name="m_TimeBetweenWaterHeightMapChecks" min="0.0f" max="10.0f" step="0.01f" init="0.01f"/>
    <float name="m_TimeBetweenWaterProbes" min="0.0f" max="10.0f" init="2.0f"/>
    <float name="m_WaterProbeDepth" min="0.0f" max="30.0f" init="14.0f"/>
    <float name="m_WaterProbeOffset" min="0.0f" max="30.0f" init="30.0f"/>
    <float name="m_AquaticSpawnDepth" min="0.0f" max="30.0f" init="2.5f"/>
    <float name="m_AquaticSpawnMaxHeightAbovePlayer" min="0.0f" max="20.0f" init="10.0f"/>
    <float name="m_IncreasedAquaticSpawningFactor" min="0.0f" max="100.0f" init="20.0f"/>
    <float name="m_CloseSpawningViewMultiplier" min="0.0f" max="100.0f" init="0.5f"/>
    <float name="m_IncreasedGroundWildlifeSpawningFactor" min="0.0f" max="100.0f" init="20.0f"/>
    <string name="m_SharkModelName" type="atHashString" init="A_C_SharkTiger"/>
    <float name="m_DeepWaterThreshold" min="0.0f" max="1000.0f" init="20.0f"/>
    <float name="m_PlayerSwimTimeThreshold" min="0.0f" max="180.0f" init="20.0f"/>
    <u32 name="m_MinTimeBetweenSharkDispatches" min="0" max="180000" init="60000"/>
    <float name="m_SharkAddRangeInViewMin" min="0.0f" max="100.0f" init="10.0f"/>
    <float name="m_SharkAddRangeInViewMinFar" min="0.0f" max="100.0f" init="35.0f"/>
    <float name="m_SharkSpawnProbability" min="0.0f" max="1.0f" init="0.25f"/>
  </structdef>
</ParserSchema>
