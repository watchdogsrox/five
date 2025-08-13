<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CSpecialAbilityData">
    <int name="duration" init="30" min="1" max="100"/>
    <int name="initialUnlockedCap" init="12" min="0" max="100"/>
	<int name="minimumChargeForActivation" init="0" min="0" max="60"/>
	<bool name="allowsDeactivationByInput" init="true"/>
	<bool name="allowsActivationOnFoot" init="true"/>
	<bool name="allowsActivationInVehicle" init="true"/>
	<float name="timeWarpScale" init="1" min="0.2" max="1"/>
	<float name="damageMultiplier" init="1.0" min="1.0" max="10.0"/>
	<float name="defenseMultiplier" init="1.0" min="0.0" max="1.0"/>
	<float name="staminaMultiplier" init="1.0" min="0.0" max="2.0"/>
	<float name="sprintMultiplier" init="1.0" min="0.0" max="1.5"/>
	<float name="steeringMultiplier" init="1.0" min="0.1" max="1.5"/>
	<float name="depletionMultiplier" init="1.0" min="0.0" max="10.0"/>
	<float name="chargeMultiplier" init="1.0" min="0.0" max="10.0"/>
	<string name="fxName" type="atHashString"/>
	<string name="outFxName" type="atHashString"/>
	<string name="activeAnimSet" type="atHashString"/>
</structdef>

<enumdef type="eFadeCurveType">
    <enumval name="FCT_NONE" value="0"/>
    <enumval name="FCT_LINEAR"/>
    <enumval name="FCT_HALF_SIGMOID"/>
    <enumval name="FCT_SIGMOID"/>
</enumdef>

<structdef type="CPlayerSpecialAbilityManager" onPostLoad="PostLoad">
	<array name="specialAbilities" type="atArray">
		<struct type="CSpecialAbilityData"/>
	</array>
	<int name="smallCharge" init="1" min="1" max="30"/>
	<int name="mediumCharge" init="5" min="1" max="30"/>
	<int name="largeCharge" init="10" min="1" max="30"/>
	<int name="continuousCharge" init="2" min="1" max="30"/>
    <enum name="fadeCurveType" type="eFadeCurveType" init="FCT_NONE"/>
	<float name="halfSigmoidConstant" init="0.2" min="0.1" max="10.0" description="[0.1, 10.f] where 0.1f is steep and 10.f is close to linear"/>
	<float name="sigmoidConstant" init="1.0" min="0.1" max="5.0" description="[0.1, 5.f] where 0.1f is steep and 1.f more relaxed"/>
	<float name="fadeInTime" init="0.2" min="0.0" max="5.0" description="time for effect to fully fade in"/>
	<float name="fadeOutTime" init="0.2" min="0.0" max="5.0" description="time for effect to fully fade out"/>
</structdef>

</ParserSchema>
