<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CScenarioInfo" >
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <string name="m_PropName" type="atHashString"/>
    <pointer name="m_Models" type="CAmbientPedModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetPedModelSetFromNameCB"/>
    <pointer name="m_BlockedModels" type="CAmbientPedModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetPedModelSetFromNameCB"/>
    <float name="m_SpawnProbability" init="1" min="0" max="1"/>
    <u32 name="m_SpawnInterval" init="0"/>
		<float name="m_SpawnHistoryRange" init="50" min="1" max="10000" description="The range at which a previously spawned object will be 'remembered' to not allow spawning of another object (default 50 meters)"/>
		<u32 name="m_MaxNoInRange" init="0"/>
	  <u32 name="m_PropEndOfLifeTimeoutMS" init="60000"/>
    <float name="m_Range" init="0"/>
    <string name="m_SpawnPropIntroDict" type="atHashString"/>
    <string name="m_SpawnPropIntroAnim" type="atHashString"/>
    <string name="m_StationaryReactHash" type="atHashString" init="CODE_HUMAN_COWER" />
    <Vector3 name="m_SpawnPropOffset"/>
    <Vector4 name="m_SpawnPropRotation"/>
    <float name="m_TimeTilPedLeaves" init="-1"/>
    <float name="m_ChanceOfRunningTowards" init="0"/>
    <bitset name="m_Flags" type="fixed" numBits="CScenarioInfoFlags::Flags_NUM_ENUMS" values="CScenarioInfoFlags::Flags"/>
    <pointer name="m_Condition" type="CScenarioConditionWorld" policy="owner"/>
    <struct name="m_ConditionalAnimsGroup" type="CConditionalAnimsGroup"/>
    <string name="m_CameraNameHash" type="atHashString"/>
    <float name="m_IntroBlendInDuration" init="0.5f" description="The blend in duration for the enter clip in seconds."/>
    <float name="m_OutroBlendInDuration" init="0.5f" description="The blend in duration for the exit clip in seconds."/>
    <float name="m_OutroBlendOutDuration" init="0.25f" description="The blend out duration for the exit clip in seconds."/>
    <float name="m_ImmediateExitBlendOutDuration" init="0.5f" description="The duration to blend out of ambient clips when responding to some event."/>
    <float name="m_PanicExitBlendInDuration" init="0.125f" description="The blend in duration for the panic exit clip in seconds."/>
    <Vector3 name="m_PedCapusleOffset"/>
    <float name="m_PedCapsuleRadiusOverride" init="-1.0f" description="Grow or shrink the ped's capsule radius to this value while using this scenario.  -1 is interpreted as no override." />
    <float name="m_ReassessGroundExitThreshold" init="-1.0f" description="If positive, this is the Z tolerance for reattaching the ped for small falls during an exit."/>
    <float name="m_FallExitThreshold" init="-1.0f" description="If positive, this is the Z tolerance for falling out of a scenario during an exit."/>
    <float name="m_ExitProbeZOverride" init="0.0f" description="If nonzero, this replaces the anim-derived end position for the exit probe helper checks."/>
    <float name="m_ExitProbeCapsuleRadiusOverride" init="0.0f" description="If nonzero, this replaces the ped capsule radius used during the exit probe helper checks."/>
    <float name="m_MaxDistanceMayAdjustPathSearchOnExit" init="-1.0f" description="If positive, this replaces the default distance to adjust path request start positions when exiting."/>
    <enum name="m_eLookAtImportance" type="eLookAtImportance" />
	</structdef>

  <structdef type="CScenarioWanderingInRadiusInfo" base="CScenarioInfo" >
    <float name="m_WanderRadius"/>
  </structdef>

  <structdef type="CScenarioPlayAnimsInfo" base="CScenarioInfo" >
    <Vector3 name="m_SeatedOffset"/>
    <string name="m_WanderScenarioToUseAfter" type="atHashString"/>
  </structdef>
  
  <structdef type="CScenarioCoupleInfo" base="CScenarioInfo" >
    <Vector3 name="m_MoveFollowOffset"/>
    <bool name="m_UseHeadLookAt"/>
  </structdef>

  <structdef type="CScenarioMoveBetweenInfo" base="CScenarioInfo" >
    <float name="m_TimeBetweenMoving"/>
  </structdef>

  <structdef type="CScenarioParkedVehicleInfo" base="CScenarioInfo" >
    <enum name="m_eVehicleScenarioType" type="eVehicleScenarioType"/>
  </structdef>

  <structdef type="CScenarioVehicleInfo"  base="CScenarioInfo" >
    <string name="m_ScenarioLayoutForPeds" type="atHashString" description="Name of vehicle scenario layout to use for spawning peds around the vehicle."/>
    <enum name="m_ScenarioLayoutOrigin" type="eVehicleScenarioPedLayoutOrigin" init="kLayoutOriginVehicle" description="Specifies what coordinates in m_ScenarioLayoutForPeds are relative to."/>
    <pointer name="m_VehicleModelSet" type="CAmbientModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetVehicleModelSetFromNameCB"/>
    <pointer name="m_VehicleTrailerModelSet" type="CAmbientModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetVehicleModelSetFromNameCB"/>
    <float name="m_ProbabilityForDriver" init="1.0f" min="0.0f" max="1.0f" description="Probability that a driver should spawn with the vehicle."/>
    <float name="m_ProbabilityForPassengers" init="0.0f" min="0.0f" max="1.0f"  description="Probability that passengers should spawn with the vehicle."/>
    <float name="m_ProbabilityForTrailer" init="1.0f" min="0.0f" max="1.0f"  description="Probability that a trailer should spawn, if m_VehicleTrailerModelSet is set."/>
    <u8 name="m_MaxNumPassengers" init="3" min="1" max="8" description="If spawning with passengers, this is the maximum number allowed."/>
    <u8 name="m_MinNumPassengers" init="1" min="1" max="8" description="If spawning with passengers, this is the minimum number we try to create."/>
  </structdef>

  <enumdef type="eVehicleScenarioPedLayoutOrigin">
    <enumval name="kLayoutOriginVehicle"/>
    <enumval name="kLayoutOriginVehicleFront"/>
    <enumval name="kLayoutOriginVehicleBack"/>
  </enumdef>
    
  <!-- SHOULD BE KEPT UP TO DATE WITH ENUM (CTaskVehicleParkNew::ParkType) IN vehicleAi\task\TaskVehiclePark.h -->
  <enumdef type="CTaskVehicleParkNew::ParkType">
	<enumval name="Park_Parallel"/>
	<enumval name="Park_Perpendicular_NoseIn"/> 
	<enumval name="Park_Perpendicular_BackIn"/>
	<enumval name="Park_PullOver"/>
	<enumval name="Park_LeaveParallelSpace"/>
	<enumval name="Park_BackOutPerpendicularSpace"/>
	<enumval name="Park_PassengerExit"/>
  </enumdef>
	
  <structdef type="CScenarioVehicleParkInfo" base="CScenarioVehicleInfo" >
	  <enum name="m_ParkType" type="CTaskVehicleParkNew::ParkType" init="CTaskVehicleParkNew::Park_Perpendicular_NoseIn" size="8"/>
  </structdef>
	
  <structdef type="CScenarioFleeInfo" base="CScenarioInfo" >
    <float name="m_SafeRadius"/>
  </structdef>
  
  <structdef type="CScenarioWanderingInfo" base="CScenarioInfo" />
  <structdef type="CScenarioGroupInfo" base="CScenarioInfo" />
  <structdef type="CScenarioControlAmbientInfo" base="CScenarioInfo" />
  <structdef type="CScenarioSkiingInfo" base="CScenarioInfo" />
  <structdef type="CScenarioSkiLiftInfo" base="CScenarioInfo" />
  <structdef type="CScenarioSniperInfo" base="CScenarioInfo" />
  <structdef type="CScenarioJoggingInfo" base="CScenarioInfo" />

  <structdef type="CScenarioLookAtInfo" base="CScenarioInfo"  />

  <structdef type="CScenarioDeadPedInfo" base="CScenarioInfo" />

</ParserSchema>
