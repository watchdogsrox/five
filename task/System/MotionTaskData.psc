<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="MotionTaskType">
	<enumval name="PED_ON_FOOT"/>
	<enumval name="PED_IN_WATER"/>
	<enumval name="PED_ON_MOUNT"/>	
	<enumval name="BIRD_ON_FOOT"/>
	<enumval name="FLIGHTLESS_BIRD_ON_FOOT"/>
	<enumval name="HORSE_ON_FOOT"/>
	<enumval name="BOAR_ON_FOOT"/>
	<enumval name="COW_ON_FOOT"/>
	<enumval name="COUGAR_ON_FOOT"/>
	<enumval name="COYOTE_ON_FOOT"/>
	<enumval name="DEER_ON_FOOT"/>
	<enumval name="PIG_ON_FOOT"/>
	<enumval name="RAT_ON_FOOT"/>
	<enumval name="RETRIEVER_ON_FOOT"/>
	<enumval name="ROTTWEILER_ON_FOOT"/>
	<enumval name="FISH_IN_WATER"/>
  <enumval name="HORSE_SWIMMING"/>
  <enumval name="CAT_ON_FOOT"/>
  <enumval name="RABBIT_ON_FOOT"/>
  <enumval name="MAX_MOTION_TASK_TYPES"/>
</enumdef>

<structdef type="sMotionTaskData">
	<enum name="m_Type" type="MotionTaskType"/>
	<string name="m_JumpClipSetHash" type="atHashString"/>
	<string name="m_JumpNetworkName" type="ConstString" init="TaskJump"/>
	<bool name="m_UseQuadrupedJump" init="false"/>
	<string name="m_FallClipSetHash" type="atHashString"/>
	<string name="m_SlopeScrambleClipSetHash" type="atHashString" init="CLIP_SET_SLOPE_SCAMBLE"/>
  <string name="m_SwimmingClipSetHash" type="atHashString" init="move_ped_swimming"/>
  <string name="m_SwimmingClipSetBaseHash" type="atHashString" init="move_ped_swimming_base"/>
	<float name="m_WalkTurnRate" init="3.141593"/>
	<float name="m_RunTurnRate" init="2.356194"/>
	<float name="m_SprintTurnRate" init="1.570796"/>
	<float name="m_MotionAnimRateScale" init="1.0"/>
	<float name="m_QuadWalkRunTransitionTime" init="0.25f"/>
	<float name="m_QuadRunWalkTransitionTime" init="0.25f"/>
	<float name="m_QuadRunSprintTransitionTime" init="0.25f"/>
	<float name="m_QuadSprintRunTransitionTime" init="0.25f"/>
	<float name="m_QuadWalkTurnScaleFactor" init="1.0f"/>
	<float name="m_QuadRunTurnScaleFactor" init="1.0f"/>
	<float name="m_QuadSprintTurnScaleFactor" init="1.0f"/>
	<float name="m_QuadWalkTurnScaleFactorPlayer" init="1.0f"/>
	<float name="m_QuadRunTurnScaleFactorPlayer" init="1.0f"/>
	<float name="m_QuadSprintTurnScaleFactorPlayer" init="1.0f"/>
	<float name="m_PlayerBirdFlappingSpeedMultiplier" init="1.0f"/>
	<float name="m_PlayerBirdGlidingSpeedMultiplier" init="1.0f"/>
	<bool name="m_HasWalkStarts" init="true"/>
	<bool name="m_HasRunStarts" init="true"/>
	<bool name="m_HasQuickStops" init="false"/>
  <bool name="m_CanTrot" init="false"/>
  <bool name="m_HasDualGaits" init="true"/>
  <bool name="m_CanCanter" init="true"/>
  <bool name="m_CanReverse" init="true"/>
  <bool name="m_HasSlopeGaits" init="true"/>
  <bool name="m_HasCmplxTurnGaits" init="true"/>
</structdef>

<structdef type="CMotionTaskDataSet">
	<string name="m_Name" type="atHashString"/>
	<pointer name="m_onFoot" type="sMotionTaskData" policy="owner"/>
	<pointer name="m_inWater" type="sMotionTaskData" policy="owner"/>
	<bool name="m_HasLowLodMotionTask" init="true"/>
</structdef>

<structdef type="CMotionTaskDataManager">
	<array name="m_aMotionTaskData" type="atArray">
		<pointer type="CMotionTaskDataSet" policy="owner"/>
	</array>
</structdef>

</ParserSchema>
