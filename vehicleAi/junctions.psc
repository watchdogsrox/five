<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<!--
		#define	JUNCTIONS_MAX_TEMPLATES						150
		#define JUNCTIONS_MAX_NUMBER						  56		// Maximum number of junctions active at any one time.
		#define MAX_ROADS_INTO_JUNCTION						16
    #define MAX_TRAFFIC_LIGHT_LOCATIONS				8
		#define MAX_JUNCTION_NODES_PER_JUNCTION		8
	-->

	<structdef type="CJunctionTemplate::CEntrance">
		<Vector3 name="m_vNodePosition"/>
		<s32 name="m_iPhase"/>
		<float name="m_fStoppingDistance"/>
		<float name="m_fOrientation"/>
		<float name="m_fAngleFromCenter"/>
		<bool name="m_bCanTurnRightOnRedLight"/>
		<bool name="m_bLeftLaneIsAheadOnly"/>
		<bool name="m_bRightLaneIsRightOnly"/>
		<s32 name="m_iLeftFilterLanePhase"/>
	</structdef>

	<structdef type="CJunctionTemplate::CPhaseTiming">
		<float name="m_fStartTime"/>
		<float name="m_fDuration"/>
	</structdef>

  <structdef type="CJunctionTemplate::CTrafficLightLocation">
    <s16 name="m_iPosX"/>
    <s16 name="m_iPosY"/>
    <s16 name="m_iPosZ"/>
  </structdef>
  
  <structdef type="CJunctionTemplate">
		<u32 name="m_iFlags"/>
		<s32 name="m_iNumJunctionNodes"/>
		<s32 name="m_iNumEntrances"/>
		<s32 name="m_iNumPhases"/>
    <s32 name="m_iNumTrafficLightLocations"/>
    <float name="m_fSearchDistance"/>
    <float name="m_fPhaseOffset"/>
    <Vector3 name="m_vJunctionMin"/>
		<Vector3 name="m_vJunctionMax"/>
		<array name="m_vJunctionNodePositions" type="atRangeArray" size="8">
			<Vector3/>
		</array>
		<array name="m_Entrances" type="atRangeArray" size="16">
			<struct type="CJunctionTemplate::CEntrance"/>
		</array>
		<array name="m_PhaseTimings" type="atRangeArray" size="16">
			<struct type="CJunctionTemplate::CPhaseTiming"/>
		</array>
    <array name="m_TrafficLightLocations" type="atRangeArray" size="8">
      <struct type="CJunctionTemplate::CTrafficLightLocation"/>
    </array>    
	</structdef>

  <structdef type="CAutoJunctionAdjustment">
    <Vec3V name="m_vLocation"/>
    <float name="m_fCycleOffset"/>
    <float name="m_fCycleDuration"/>
  </structdef>

  <const name="JUNCTIONS_MAX_AUTO_ADJUSTMENTS" value="8"/>
  
	<structdef type="CJunctionTemplateArray">
		<array name="m_Entries" type="atFixedArray" size="150">
			<struct type="CJunctionTemplate"/>
		</array>
    <array name="m_AutoJunctionAdjustments" type="atFixedArray" size="JUNCTIONS_MAX_AUTO_ADJUSTMENTS">
      <struct type="CAutoJunctionAdjustment"/>
    </array>
	</structdef>
</ParserSchema>
