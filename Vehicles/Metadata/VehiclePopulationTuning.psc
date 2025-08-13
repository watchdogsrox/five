<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


<structdef type="CVehiclePopulationTuning" >
	<float name="m_fMultiplayerRandomVehicleDensityMultiplier" init="1.0f"/>
	<float name="m_fMultiplayerParkedVehicleDensityMultiplier" init="1.0f"/>
</structdef>

<structdef type="CVehGenSphere" >
	<s16 name="m_iPosX" init="0"/>
	<s16 name="m_iPosY" init="0"/>
	<s16 name="m_iPosZ" init="0"/>
	<u8 name="m_iRadius" init="0"/>
	<u8 name="m_iFlags" init="0"/>
</structdef>

<const name="MAX_VEHGEN_MARKUP_SPHERES" value="32"/>
  
<structdef type="CVehGenMarkUpSpheres" >
	<array name="m_Spheres" type="atFixedArray" size="MAX_VEHGEN_MARKUP_SPHERES">
		<struct type="CVehGenSphere"/>
	</array>
</structdef>

</ParserSchema>