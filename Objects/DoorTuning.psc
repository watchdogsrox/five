<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<!-- This Needs to match enum in code DoorTuning.h-->
	<enumdef type="StdDoorRotDir">
		<enumval name="StdDoorOpenBothDir" value="0"/>
		<enumval name="StdDoorOpenNegDir" value="1"/>
		<enumval name="StdDoorOpenPosDir" value="2"/>
	</enumdef>
	
	<!-- This needs to match enum in code DoorTuning.h-->
	<enumdef type="CDoorTuning::eDoorTuningFlags"> 
		<enumval name="DontCloseWhenTouched"/>
		<enumval name="AutoOpensForSPVehicleWithPedsOnly"/>
		<enumval name="AutoOpensForSPPlayerPedsOnly"/>
		<enumval name="AutoOpensForMPVehicleWithPedsOnly"/>
		<enumval name="AutoOpensForMPPlayerPedsOnly"/>
		<enumval name="DelayDoorClosingForPlayer"/>
		<enumval name="AutoOpensForAllVehicles"/>
    <enumval name="IgnoreOpenDoorTaskEdgeLerp"/>
    <enumval name="AutoOpensForLawEnforcement"/>
  </enumdef>

	<structdef type="CDoorTuning" >
		<Vec3V name="m_AutoOpenVolumeOffset" description="Extra local space offset for positioning the volume used for automatic opening."/>
		<bitset name="m_Flags" type="fixed32" values="CDoorTuning::eDoorTuningFlags"/>
		<float name="m_AutoOpenRadiusModifier" min="0.0f" max="5.0f" step="0.05f" init="1.0f" description="Multiplicative modifier on the radius at which this door auto-opens. At 1.0, the opening radius will match the width of the door."/>
		<float name="m_AutoOpenRate" min="0.0f" max="5.0f" step="0.01f" init="0.5f" description="Rate at which this door auto-opens. This is the inverse of the time it takes to open, e.g. a value of 0.25 will open the door in 4 seconds."/>
    	<float name="m_AutoOpenCosineAngleBetweenThreshold" min="-1.0f" max="1.0f" init="-1.0f" description="Cosine of the angle between the ped to door vector and the ped fwd vector angle threshold to auto open" />
    	<bool name="m_AutoOpenCloseRateTaper" init="false" description="If true the rate at which this door auto-opens/closes is slowed as the door approaches full open/close."/>
		<bool name="m_UseAutoOpenTriggerBox" init="false" description="If true the auto open trigger volume is a box not the default sphere."/>
		<bool name="m_CustomTriggerBox" init="false" description="If true the auto open trigger volume is a box of size determined by the spdAABB info.(only valid if m_UseAutoOpenTriggerBox is set)"/>
		<struct name="m_TriggerBoxMinMax" type="rage::spdAABB" description="Customized trigger box min/max relative values (only valid if m_UseAutoOpenTriggerBox and m_CustomTriggerBox are set)"/>		
		<bool name="m_BreakableByVehicle" init="false" description="If true, the door may break if hit by a vehicle."/>
		<float name="m_BreakingImpulse" min="0.0f" max="100000.0f" step="1000.0f" init="0.0f" description="The magnitude of the impulse required to break the constraint."/>
		<bool name="m_ShouldLatchShut" init="false" description="If true, the door will latch shut."/>
		<float name="m_MassMultiplier" min="0.01f" max="100.0f" step="0.01f" init="1.0f" description="Multiplier applied to the mass of the door (default is 1.0f)"/>
		<float name="m_WeaponImpulseMultiplier" min="0.0f" max="100.0f" step="0.01f" init="1.0f" description="Multiplier applied to weapon impulses (default is 1.0f)"/>
		<float name="m_RotationLimitAngle" min="0.0f" max="180.0f" step="1.0f" init="0.0f" description="Allow override of open angle from 0 -> 180 with 0.0f considered closed (default is 0.0f)"/>
		<float name="m_TorqueAngularVelocityLimit" min="0.0f" max="10.0f" step="0.01f" init="5.0f" description="Allow override of the angular velocity limit (default is 5.0f)"/>
		<enum name="m_StdDoorRotDir" type="StdDoorRotDir" init="StdDoorOpenBothDir" description="For standard doors this setting limits what direction the door can open (default is OpenBothDir)"/>
	</structdef>

	<structdef type="CDoorTuningFile::NamedTuning" >
		<string name="m_Name" type="ConstString" ui_key="true"/>
		<struct name="m_Tuning" type="CDoorTuning"/>
	</structdef>

	<structdef type="CDoorTuningFile::ModelToTuneName" >
		<string name="m_ModelName" type="ConstString" ui_key="true"/>
		<string name="m_TuningName" type="ConstString"/>
	</structdef>

	<structdef type="CDoorTuningFile" >
		<array name="m_NamedTuningArray" type="atArray">
			<struct type="CDoorTuningFile::NamedTuning"/>
		</array>
		<array name="m_ModelToTuneMapping" type="atArray">
			<struct type="CDoorTuningFile::ModelToTuneName"/>
		</array>
	</structdef>

</ParserSchema>
