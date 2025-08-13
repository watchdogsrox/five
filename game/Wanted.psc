<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CWanted::Tunables::WantedLevel::Difficulty::Calculation::Weights" >
		<float name="m_WantedLevel" min="0.0f" max="1.0f" step="0.01f" init="0.7f"/>
		<float name="m_LastSpottedDistance" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
		<float name="m_Randomness" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
	</structdef>

	<structdef type="CWanted::Tunables::WantedLevel::Difficulty::Calculation::Decay" >
		<float name="m_TimeEvadingForMaxValue" min="0.0f" max="600.0f" step="1.0f" init="120.0f"/>
		<float name="m_MaxValue" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<bool name="m_DisableWhenOffMission" init="false"/>
	</structdef>

	<structdef type="CWanted::Tunables::WantedLevel::Difficulty::Calculation" >
		<float name="m_FromWantedLevel" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<struct name="m_Weights" type="CWanted::Tunables::WantedLevel::Difficulty::Calculation::Weights"/>
		<struct name="m_Decay" type="CWanted::Tunables::WantedLevel::Difficulty::Calculation::Decay"/>
	</structdef>

	<structdef type="CWanted::Tunables::WantedLevel::Difficulty::Helis::Refuel" >
		<bool name="m_Enabled" init="false"/>
		<float name="m_TimeBefore" min="0.0f" max="3600.0f" step="1.0f" init="0.0f"/>
		<float name="m_Delay" min="0.0f" max="3600.0f" step="1.0f" init="0.0f"/>
	</structdef>

	<structdef type="CWanted::Tunables::WantedLevel::Difficulty::Helis" >
		<struct name="m_Refuel" type="CWanted::Tunables::WantedLevel::Difficulty::Helis::Refuel"/>
	</structdef>

	<structdef type="CWanted::Tunables::WantedLevel::Difficulty" >
		<struct name="m_Calculation" type="CWanted::Tunables::WantedLevel::Difficulty::Calculation"/>
		<struct name="m_Helis" type="CWanted::Tunables::WantedLevel::Difficulty::Helis"/>
	</structdef>

	<structdef type="CWanted::Tunables::WantedLevel" >
		<struct name="m_Difficulty" type="CWanted::Tunables::WantedLevel::Difficulty"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Range" >
		<float name="m_Min" min="-10000.0f" max="10000.0f" step="0.01f" init="0.0f"/>
		<float name="m_Max" min="-10000.0f" max="10000.0f" step="0.01f" init="0.0f"/>
		<float name="m_ValueForMin" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_ValueForMax" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Spawning::Scoring::Weights" >
		<struct name="m_Distance" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_Direction" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_Randomness" type="CWanted::Tunables::Difficulty::Range"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Spawning::Scoring" >
		<struct name="m_Weights" type="CWanted::Tunables::Difficulty::Spawning::Scoring::Weights"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Spawning" >
		<struct name="m_Scoring" type="CWanted::Tunables::Difficulty::Spawning::Scoring"/>
		<struct name="m_IdealDistance" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_ChancesToForceWaitInFront" type="CWanted::Tunables::Difficulty::Range"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Despawning" >
		<struct name="m_MaxFacingThreshold" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_MaxMovingThreshold" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_MinDistanceToBeConsideredLaggingBehind" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_MinDistanceToCheckClumped" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_MaxDistanceToBeConsideredClumped" type="CWanted::Tunables::Difficulty::Range"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations::Situation" >
		<struct name="m_SensesRange" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_IdentificationRange" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_ShootRateModifier" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_WeaponAccuracy" type="CWanted::Tunables::Difficulty::Range"/>
		<float name="m_WeaponAccuracyModifierForEvasiveMovement" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_WeaponAccuracyModifierForOffScreen" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_WeaponAccuracyModifierForAimedAt" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_MinForDrivebys" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations" >
		<struct name="m_Default" type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations::Situation"/>
		<struct name="m_InVehicle" type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations::Situation"/>
		<struct name="m_InHeli" type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations::Situation"/>
		<struct name="m_InBoat" type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations::Situation"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Peds::Attributes" >
		<struct name="m_Situations" type="CWanted::Tunables::Difficulty::Peds::Attributes::Situations"/>
		<struct name="m_AutomobileSpeedModifier" type="CWanted::Tunables::Difficulty::Range"/>
		<struct name="m_HeliSpeedModifier" type="CWanted::Tunables::Difficulty::Range"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Peds" >
		<struct name="m_Cops" type="CWanted::Tunables::Difficulty::Peds::Attributes"/>
		<struct name="m_Swat" type="CWanted::Tunables::Difficulty::Peds::Attributes"/>
		<struct name="m_Army" type="CWanted::Tunables::Difficulty::Peds::Attributes"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty::Dispatch" >
		<struct name="m_TimeBetweenSpawnAttemptsModifier" type="CWanted::Tunables::Difficulty::Range"/>
	</structdef>

	<structdef type="CWanted::Tunables::Difficulty" >
		<struct name="m_Spawning" type="CWanted::Tunables::Difficulty::Spawning"/>
		<struct name="m_Despawning" type="CWanted::Tunables::Difficulty::Despawning"/>
		<struct name="m_Peds" type="CWanted::Tunables::Difficulty::Peds"/>
		<struct name="m_Dispatch" type="CWanted::Tunables::Difficulty::Dispatch"/>
	</structdef>

	<structdef type="CWanted::Tunables::Rendering">
		<bool name="m_Enabled" init="false"/>
		<bool name="m_Witnesses" init="true"/>
		<bool name="m_Crimes" init="true"/>
	</structdef>

	<structdef type="CWanted::Tunables::Timers" >
		<float name="m_TimeBetweenDifficultyUpdates" min="0.0f" max="5.0f" step="1.0f" init="1.0f"/>
	</structdef>

  <structdef type="CWanted::Tunables::CrimeInfo">
    <u32 name="m_WantedScorePenalty" min="0" max="100" step="1" init="50"/>
    <u32 name="m_WantedStarPenalty" min="0" max="5" step="1" init="0"/>
    <u32 name="m_Lifetime" min="0" max="20000" step="1" init="15000"/>
    <u32 name="m_ReportDelay" min="0" max="20000" step="1" init="10000"/>
    <bool name="m_ReportDuplicates" init="false"/>
  </structdef>

  <structdef type="CWanted::Tunables"  base="CTuning" >
		<struct name="m_WantedClean" type="CWanted::Tunables::WantedLevel"/>
		<struct name="m_WantedLevel1" type="CWanted::Tunables::WantedLevel"/>
		<struct name="m_WantedLevel2" type="CWanted::Tunables::WantedLevel"/>
		<struct name="m_WantedLevel3" type="CWanted::Tunables::WantedLevel"/>
		<struct name="m_WantedLevel4" type="CWanted::Tunables::WantedLevel"/>
		<struct name="m_WantedLevel5" type="CWanted::Tunables::WantedLevel"/>
		<struct name="m_Difficulty" type="CWanted::Tunables::Difficulty"/>
		<struct name="m_Rendering" type="CWanted::Tunables::Rendering"/>
		<struct name="m_Timers" type="CWanted::Tunables::Timers"/>
    <u32 name="m_MaxTimeTargetVehicleMoving" min="0" max="20000" step="100" init="10000"/>
    <u32 name="m_DefaultAmnestyTime" min="0" max="20000" step="100" init="2000"/>
    <u32 name="m_DefaultHiddenEvasionTimeReduction" min="0" max="20000" step="100" init="3500"/>
    <u32 name="m_InitialAreaTimeoutWhenSeen" min="0" max="200000" step="100" init="30000"/>
    <u32 name="m_InitialAreaTimeoutWhenCrimeReported" min="0" max="200000" step="100" init="30000"/>
		<float name="m_ExtraStartHiddenEvasionDelay" min="0.0f" max="180.0f" step="0.5f" init="30.0f"/>
    <map name="m_CrimeInfosDefault" type="atBinaryMap" key="atHashString">
      <struct type="CWanted::Tunables::CrimeInfo"/>
    </map>
    <map name="m_CrimeInfosArcadeCNC" type="atBinaryMap" key="atHashString">
      <struct type="CWanted::Tunables::CrimeInfo"/>
    </map>
	</structdef>

</ParserSchema>
