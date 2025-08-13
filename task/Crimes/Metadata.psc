<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="eRandomEventType">
    <enumval name="ET_INVALID" value="-1"/>
    <enumval name="ET_CRIME"/>
    <enumval name="ET_JAYWALKING"/>
    <enumval name="ET_COP_PURSUIT"/>
    <enumval name="ET_SPAWNED_COP_PURSUIT"/>
    <enumval name="ET_AMBIENT_COP"/>
    <enumval name="ET_INTERESTING_DRIVER"/>
    <enumval name="ET_AGGRESSIVE_DRIVER"/>
  </enumdef>

  <structdef type="CRandomEventManager::RandomEventType" >
    <string name="m_RandomEventTypeName" type="atHashString" />
    <float name="m_RandomEventTimeIntervalMin" min="0.0f" max="2000.0f" init="30.0"/>
    <float name="m_RandomEventTimeIntervalMax" min="0.0f" max="2000.0f" init="30.0"/>
    <float name="m_DeltaScaleWhenPlayerStationary" min="0.0f" max="100.0f" init="100.0"/>
  </structdef>

  <structdef type="CRandomEventManager::RandomEventData" >
    <string name="m_RandomEventName" type="atHashString" />
    <enum name="m_RandomEventType" type="eRandomEventType" init="ET_INVALID"/>
  </structdef>

  <structdef type="CRandomEventManager::Tunables" base="CTuning">
    <bool name="m_RenderDebug" init="false"/>
    <bool name="m_Enabled" init="true"/>
    <bool name="m_ForceCrime" init="false"/>
    <float name="m_EventInterval" min="0.0f" max="500.0f" init="9.0f" />
    <float name="m_EventInitInterval" min="0.0f" max="400.0f" init="9.0f" />
    <array name="m_RandomEventType" type="atArray">
      <struct type="CRandomEventManager::RandomEventType"/>
    </array>
    <array name="m_RandomEventData" type="atArray">
      <struct type="CRandomEventManager::RandomEventData"/>
    </array>
    <bool name="m_SpawningChasesEnabled" init="true"/>
    <int   name="m_MaxNumberCopVehiclesInChase" min="1" max="3" step="1" init="1"/>
    <int   name="m_ProbSpawnHeli" min="0" max="100" step="1" init="30"/>
    <int   name="m_MaxAmbientVehiclesToSpawnChase" min="0" max="100" step="1" init="40"/>
    <int   name="m_MinPlayerMoveDistanceToSpawnChase" min="0" max="10000" step="1" init="200"/>
    <string  name="m_HeliVehicleModelId" type="atHashString" init="polmav"/>
    <string  name="m_HeliPedModelId" type="atHashString" init="S_M_Y_Swat_01"/>
  </structdef>

  <structdef type="CTaskStealVehicle::Tunables" base="CTuning">
    <float name="m_MaxDistanceToFindVehicle" min="5.0f" max="200.0f" step="1.0f" init="50.0f"/>
    <float name="m_MaxDistanceToPursueVehicle" min="5.0f" max="200.0f" step="1.0f" init="75.0f"/>
    <float name="m_DistanceToRunToVehicle" min="0.0f" max="20.0f" step="0.1f" init="6.0f"/>
    <bool name="m_CanStealPlayersVehicle" init="true"/>
    <bool name="m_CanStealCarsAtLights" init="true"/>
    <bool name="m_CanStealParkedCars" init="true"/>
    <bool name="m_CanStealStationaryCars" init="true"/>
  </structdef>

  <structdef type="CTaskPursueCriminal::Tunables" base="CTuning">
    <float name="m_MinDistanceToFindVehicle" min="5.0f" max="200.0f" step="1.0f" init="25.0f"/>
    <float name="m_MaxDistanceToFindVehicle" min="5.0f" max="200.0f" step="1.0f" init="40.0f"/>
    <float name="m_MaxHeightDifference" min="2.0f" max="20.0f" step="1.0f" init="7.0f"/>
    <float name="m_DotProductFacing" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_DotProductBehind" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <int name="m_DistanceToFollowVehicleBeforeFlee" min="5" max="30" step="1" init="15"/>
    <float name="m_DistanceToSignalVehiclePursuitToCriminal" min="5.0f" max="30.0f" step="0.1f" init="15.0f"/>
    <int   name="m_TimeToSignalVehiclePursuitToCriminalMin" min="5" max="30" step="1" init="15"/>
    <int   name="m_TimeToSignalVehiclePursuitToCriminalMax" min="5" max="30" step="1" init="20"/>
    <bool name="m_DrawDebug" init="false"/>
    <bool name="m_AllowPursuePlayer" init="false"/>
    <float name="m_CriminalVehicleMinStartSpeed" min="1.0f" max="100.0f" step="0.1f" init="5.0f"/>
  </structdef>

  <structdef type="CTaskReactToPursuit::Tunables" base="CTuning">
    <int   name="m_MinTimeToFleeInVehicle" min="1" max="200" step="1" init="6"/>
    <int   name="m_MaxTimeToFleeInVehicle" min="1" max="200" step="1" init="15"/>
    <float name="m_FleeSpeedInVehicle" min="1.0f" max="200.0f" step="1.0f" init="30.0f"/>
  </structdef>

</ParserSchema>
