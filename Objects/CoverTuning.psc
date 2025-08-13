<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<!-- This needs to match enum in code CoverTuning.h-->
	<enumdef type="CCoverTuning::eCoverTuningFlags"> 
		<enumval name="NoCoverNorthFaceEast"/>
		<enumval name="NoCoverNorthFaceWest"/>
		<enumval name="NoCoverNorthFaceCenter"/>
		<enumval name="NoCoverSouthFaceEast"/>
		<enumval name="NoCoverSouthFaceWest"/>
		<enumval name="NoCoverSouthFaceCenter"/>
		<enumval name="NoCoverEastFaceNorth"/>
		<enumval name="NoCoverEastFaceSouth"/>
		<enumval name="NoCoverEastFaceCenter"/>
		<enumval name="NoCoverWestFaceNorth"/>
		<enumval name="NoCoverWestFaceSouth"/>
		<enumval name="NoCoverWestFaceCenter"/>
		<enumval name="ForceLowCornerNorthFaceEast"/>
		<enumval name="ForceLowCornerNorthFaceWest"/>
		<enumval name="ForceLowCornerSouthFaceEast"/>
		<enumval name="ForceLowCornerSouthFaceWest"/>
		<enumval name="ForceLowCornerEastFaceNorth"/>
		<enumval name="ForceLowCornerEastFaceSouth"/>
		<enumval name="ForceLowCornerWestFaceNorth"/>
		<enumval name="ForceLowCornerWestFaceSouth"/>
		<enumval name="NoCoverVehicleDoors"/>
	</enumdef>

	<structdef type="CCoverTuning" >
		<bitset name="m_Flags" type="fixed32" values="CCoverTuning::eCoverTuningFlags"/>	
	</structdef>

	<structdef type="CCoverTuningFile::NamedTuning" >
		<string name="m_Name" type="ConstString" ui_key="true"/>
		<struct name="m_Tuning" type="CCoverTuning"/>
	</structdef>

	<structdef type="CCoverTuningFile::ModelToTuneName" >
		<string name="m_ModelName" type="ConstString" ui_key="true"/>
		<string name="m_TuningName" type="ConstString"/>
	</structdef>

	<structdef type="CCoverTuningFile" >
		<array name="m_NamedTuningArray" type="atArray">
			<struct type="CCoverTuningFile::NamedTuning"/>
		</array>
		<array name="m_ModelToTuneMapping" type="atArray">
			<struct type="CCoverTuningFile::ModelToTuneName"/>
		</array>
	</structdef>

</ParserSchema>
