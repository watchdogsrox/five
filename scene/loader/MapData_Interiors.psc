<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">

  <hinsert>
#include		"scene/loader/MapData_Buildings.h"
#include		"scene/loader/MapData_Entities.h"
	</hinsert>
	
	<!--
		MILO Interior Object Instance
	-->

  <enumdef type="eMloInstanceFlags">
    <enumval name="INSTANCE_MLO_GPS_ON"                value="1"/>
    <enumval name="INSTANCE_MLO_CAP_CONTENTS_ALPHA"    value="2"/>
    <enumval name="INSTANCE_MLO_SHORT_FADE"            value="4"/>
    <enumval name="INSTANCE_MLO_SPECIAL_BEHAVIOUR_1"   value="8"/>
    <enumval name="INSTANCE_MLO_SPECIAL_BEHAVIOUR_2"   value="16"/>
    <enumval name="INSTANCE_MLO_SPECIAL_BEHAVIOUR_3"   value="32"/>
  </enumdef>
  
	<structdef type="CMloInstanceDef" base="CEntityDef">
		<u32 		name="m_groupId" />
		<u32    	name="m_floorId" />
    <array 		name="m_defaultEntitySets" type="atArray">
      <string 	type="atHashString"/>
    </array>
    <u32      name="m_numExitPortals" />
    <u32      name="m_MLOInstflags" />
	</structdef>

  <!--
    MILO timecycle modifier 
  -->

  <structdef type="CMloTimeCycleModifier">
    <string   name="m_name" type="atHashString" ui_key="true"/>
    <Vector4  name="m_sphere" />
    <float	  name="m_percentage" />
    <float	  name="m_range" />
    <u32 	    name="m_startHour" />
    <u32 	    name="m_endHour" />
  </structdef>

  <!--
  // MILO interior flags
  -->
<enumdef type="eMloInteriorFlags">
  <enumval name="INTINFO_MLO_SUBWAY"                value="256"/>
  <enumval name="INTINFO_MLO_OFFICE"                value="512"/>
  <enumval name="INTINFO_MLO_ALLOWRUN"              value="1024"/>
  <enumval name="INTINFO_MLO_CUTSCENE_ONLY"         value="2048"/>
  <enumval name="INTINFO_MLO_LOD_WHEN_LOCKED"       value="4096"/>
  <enumval name="INTINFO_MLO_NO_WATER_REFLECTION"   value="8192"/>
  <enumval name="INTINFO_MLO_HAS_LOW_LOD_PORTALS"   value="32768"/>
</enumdef>

  <!--
		MILO Room flags
	-->
  
  <enumdef type="eMloRoomFlags">
    <enumval name="ROOM_FREEZEVEHICLES"             value="1"/>
    <enumval name="ROOM_FREEZEPEDS"                 value="2"/>
    <enumval name="ROOM_NO_DIR_LIGHT"               value="4"/>
    <enumval name="ROOM_NO_EXTERIOR_LIGHTS"         value="8"/>
    <enumval name="ROOM_FORCE_FREEZE"               value="16"/>
    <enumval name="ROOM_REDUCE_CARS"                value="32"/>
    <enumval name="ROOM_REDUCE_PEDS"                value="64"/>
    <enumval name="ROOM_FORCE_DIR_LIGHT_ON"         value="128"/>
    <enumval name="ROOM_DONT_RENDER_EXTERIOR"       value="256"/>
    <enumval name="ROOM_MIRROR_POTENTIALLY_VISIBLE" value="512"/>
  </enumdef>

	<!--
		MILO Room Definition
	-->
	<structdef type="CMloRoomDef">
		<string 	name="m_name" type="atString"  ui_key="true"/>
		<Vector3 	name="m_bbMin" />
		<Vector3 	name="m_bbMax" />
		<float 		name="m_blend" />
    <string   name="m_timecycleName" type="atHashString"/>
    <string   name="m_secondaryTimecycleName" type="atHashString"/>
    <u32      name="m_flags" />
		<u32 		  name="m_portalCount" />
		<s32    	name="m_floorId" />
    <s32      name="m_exteriorVisibiltyDepth" init="-1"/>
		<array 		name="m_attachedObjects" type="atArray">
			<u32 />
		</array>
	</structdef>

  <!--
		MILO Portal flags definition
	-->
	<enumdef type="eMloPortalFlags">
		<enumval name="INTINFO_PORTAL_ONE_WAY"                          value="1"/>
		<enumval name="INTINFO_PORTAL_LINK"                             value="2"/>
		<enumval name="INTINFO_PORTAL_MIRROR"                           value="4"/>
		<enumval name="INTINFO_PORTAL_IGNORE_MODIFIER"                  value="8"/>
		<enumval name="INTINFO_PORTAL_MIRROR_USING_EXPENSIVE_SHADERS"   value="16"/>
		<enumval name="INTINFO_PORTAL_LOWLODONLY"                       value="32"/>
		<enumval name="INTINFO_PORTAL_ALLOWCLOSING"                     value="64"/>
		<enumval name="INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT" value="128"/>
		<enumval name="INTINFO_PORTAL_MIRROR_USING_PORTAL_TRAVERSAL"    value="256"/>
		<enumval name="INTINFO_PORTAL_MIRROR_FLOOR"                     value="512"/>
		<enumval name="INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW"     value="1024"/>
		<enumval name="INTINFO_PORTAL_WATER_SURFACE"                    value="2048"/>
		<enumval name="INTINFO_PORTAL_WATER_SURFACE_EXTEND_TO_HORIZON"  value="4096"/>
		<enumval name="INTINFO_PORTAL_USE_LIGHT_BLEED"                  value="8192"/>
	</enumdef>

  <!--
		MILO Portal Definition
	-->
	<structdef type="CMloPortalDef">
		<u32 		name="m_roomFrom" />
		<u32 		name="m_roomTo" />
		<u32 		name="m_flags" />
		<u32 		name="m_mirrorPriority" init="0" min="0" max="3"/>
		<u32 		name="m_opacity" init="0" min="0" max="100"/>
		<u32		name="m_audioOcclusion" init="0"/>
		<array 	name="m_corners" type="atArray">
			<Vector3 />
		</array>
		<array 	name="m_attachedObjects" type="atArray">
			<u32 />
		</array>
	</structdef>
  
  <!--
		MILO entity set Definition
	-->
  <structdef type="CMloEntitySet">
    <string 		name="m_name" type="atHashString" />
    <array 		name="m_locations" type="atArray">
      <u32 />
    </array>
    <array 		name="m_entities" type="atArray">
      <pointer type="::rage::fwEntityDef" policy="owner"/>
    </array>
  </structdef>
  
	<!--
		MILO Definition
		Consisting of a list of objects, list of rooms and list of portals.
	-->
	<structdef type="CMloArchetypeDef" base="CBaseArchetypeDef">
		<u32 		name="m_mloFlags" />
		<array 		name="m_entities" type="atArray">
			<pointer type="::rage::fwEntityDef" policy="owner"/>
		</array>
		<array 		name="m_rooms" type="atArray">
			<struct type="CMloRoomDef" />
		</array>
		<array 		name="m_portals" type="atArray">
			<struct type="CMloPortalDef" />
		</array>
    <array 		name="m_entitySets" type="atArray">
      <struct type="CMloEntitySet" />
    </array>
    <array 		name="m_timeCycleModifiers" type="atArray">
      <struct type="CMloTimeCycleModifier" />
    </array>
	</structdef>
		
</ParserSchema>
	
