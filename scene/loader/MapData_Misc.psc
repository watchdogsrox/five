<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">

							
	<!--
		car generator
	-->
	<structdef type="CCarGen">
		<Vector3 name="m_position" />
		<float	name="m_orientX" />
		<float	name="m_orientY" />
		<float	name="m_perpendicularLength" />
		<string name="m_carModel" type="atHashString" ui_key="true"/>
		<u32	name="m_flags" />
		<s32	name="m_bodyColorRemap1" />
		<s32	name="m_bodyColorRemap2" />
		<s32	name="m_bodyColorRemap3" />
		<s32	name="m_bodyColorRemap4" />
		<string name="m_popGroup" type="atHashString"/>
		<s8	name="m_livery" init="-1"/>
	</structdef>
	
	<!--
		Timecycle modifier
	-->
	<structdef type="CTimeCycleModifier">
		<string name="m_name" type="atHashString" ui_key="true"/>
		<Vector3 name="m_minExtents" />
		<Vector3 name="m_maxExtents" />
		<float	name="m_percentage" />
		<float	name="m_range" />
		<u32 	name="m_startHour" />
		<u32 	name="m_endHour" />	
	</structdef>

  
  <!--
		FloatXYZ for LODLight SOA packing
	-->
  <structdef type="FloatXYZ"  simple="true">
    <float name="x"/>
    <float name="y"/>
    <float name="z"/>
  </structdef>

    <!--
		DistantLODLight structure
	-->
  <structdef type="CDistantLODLight">
    <array name="m_position" type="atArray">
      <struct type="FloatXYZ"/>
    </array>
    <array name="m_RGBI" type="atArray">
      <u32/>
    </array>
    <u16 	name="m_numStreetLights" />
    <u16 	name="m_category" />
  </structdef>

  <!--
		LODLight structure
	-->
  <structdef type="CLODLight">
    <array name="m_direction" type="atArray">
      <struct type="FloatXYZ"/>
    </array>
    <array name="m_falloff" type="atArray">
      <float/>
    </array>
    <array name="m_falloffExponent" type="atArray">
      <float/>
    </array>
    <array name="m_timeAndStateFlags" type="atArray">
      <u32/>
    </array>
    <array name="m_hash" type="atArray">
      <u32/>
    </array>
    <array name="m_coneInnerAngle" type="atArray">
      <u8/>
    </array>
    <array name="m_coneOuterAngleOrCapExt" type="atArray">
      <u8/>
    </array>
    <array name="m_coronaIntensity" type="atArray">
      <u8/>
    </array>
  </structdef>  
 
</ParserSchema>
	
