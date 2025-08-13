<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	<hinsert>
#include		"system/control_mappings.h"
	</hinsert>
	
	<structdef type="CMenuButton" onPreLoad="preLoad">
		<string name="m_hButtonHash" type="atHashWithStringBank"/>
		<enum name="m_ButtonInput" type="rage::InputType" init="UNDEFINED_INPUT"/>
		<enum name="m_ButtonInputGroup" type="rage::InputGroup" init="INPUTGROUP_INVALID"/>
		<enum name="m_RawButtonIcon" type="eInstructionButtons" init="ICON_INVALID" description="Only use for icons that are not gamepad buttons such as the loading spinner!" />
		<bool name="m_Clickable" init="true" />
	</structdef>

	<structdef type="CMenuButtonList" onPreLoad="preLoad" onPostLoad="postLoad">
		<array name="m_ButtonPrompts" type="atArray">
			<struct type="CMenuButton"/>
		</array>
		<int name="m_WrappingPoint" init="0"/>
	</structdef>

</ParserSchema>
