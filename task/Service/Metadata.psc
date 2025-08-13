<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CTaskWitness::Tunables" base="CTuning">
		<u32 name="m_MaxTimeMoveNearCrimeMs" min="0" max="10000000" step="100" init="120000"/>
		<u32 name="m_MaxTimeMoveToLawMs" min="0" max="10000000" step="100" init="120000"/>
		<u32 name="m_MaxTimeSearchMs" min="0" max="10000000" step="100" init="20000"/>
		<u32 name="m_MaxTimeMoveToLawFailedPathfindingMs" min="0" max="10000000" step="100" init="10000"/>
	</structdef>

	<structdef type="CTaskPoliceOrderResponse::Tunables" base="CTuning">
		<float name="m_MaxTimeToWait" min="0.0f" max="120.0f" step="1.0f" init="60.0f"/>
		<float name="m_MaxSpeedForVehicleMovingSlowly" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
		<float name="m_MinSpeedForVehicleMovingQuickly" min="0.0f" max="50.0f" step="1.0f" init="35.0f"/>
		<float name="m_TimeBeforeOvertakeToMatchSpeedWhenPulledOver" min="0.0f" max="5.0f" step="0.1f" init="3.0f"/>
		<float name="m_TimeBeforeOvertakeToMatchSpeedWhenCruising" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_CheatPowerIncreaseForMatchSpeed" min="0.0f" max="10.0f" step="1.0f" init="5.0f"/>
		<float name="m_MinTimeToWander" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
		<float name="m_MaxTimeToWander" min="0.0f" max="60.0f" step="1.0f" init="15.0f"/>
		<float name="m_TimeBetweenExitVehicleRetries" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
	</structdef>

  <structdef type="CTaskSwatOrderResponse::Tunables" base="CTuning">
    <float name="m_MinTimeToWander" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxTimeToWander" min="0.0f" max="60.0f" step="1.0f" init="15.0f"/>
  </structdef>

	<structdef type="CTaskHeliOrderResponse::Tunables" base="CTuning">
		<float name="m_MaxTimeSpentLandedBeforeFlee" min="0.0f" max="30.0f" step="1.0f" init="10.0f"/>
		<float name="m_MaxTimeAfterDropOffBeforeFlee" min="0.0f" max="30.0f" step="0.25f" init="1.0f"/>
		<float name="m_MinTimeSpentLandedBeforeExit" min="0.0f" max="5.0f" step="0.1f" init="0.25f"/>
		<float name="m_MaxTimeSpentLandedBeforeExit" min="0.0f" max="5.0f" step="0.1f" init="0.75f"/>
    <float name="m_MaxTimeCollidingBeforeExit" min="0.0f" max="10.0f" step="0.1f" init="2.5f"/>
		<float name="m_MinTimeBeforeOrderChangeDueToBlockedLocation" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
	</structdef>
  
  <structdef type="CTaskArrestPed::Tunables" base="CTuning">
    <float name="m_AimDistance" min="0.0f" max="10.0f" step="0.1f" init="8.0f"/>
    <float name="m_ArrestDistance" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_ArrestInVehicleDistance" min="0.0f" max="10.0f" step="0.1f" init="0.75f"/>
    <float name="m_MoveToDistanceInVehicle" min="0.0f" max="10000.0f" step="1.0f" init="30.0f"/>
    <float name="m_TargetDistanceFromVehicleEntry" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_ArrestingCopMaxDistanceFromTarget" min="0.0f" max="100.0f" step="0.1f" init="6.25f"/>
    <float name="m_BackupCopMaxDistanceFromTarget" min="0.0f" max="100.0f" step="0.1f" init="13.0f"/>
    <float name="m_MinTimeOnFootTargetStillForArrest" min="0.0f" max="100.0f" step="0.1f" init="0.65f"/>
		<u32 name="m_TimeBetweenPlayerInVehicleBustAttempts" min="0" max="15000" step="100" init="2500"/>
  </structdef>

</ParserSchema>
