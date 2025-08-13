<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"

			  generate="class">

	<hinsert>
#include		"input/mapper_defs.h"
#include		"system/control_mapping.h"
	</hinsert>
	
	<enumdef type="rage::ControlInput::Trigger" preserveNames="true">
		<enumval name="INVALID_TRIGGER" value="0" />
		<enumval name="DOWN" />
		<enumval name="UP" />
		<enumval name="PRESSED" />
		<enumval name="RELEASED" />
		<enumval name="NONZERO" />

		<!-- NOTE: This should always be the last trigger! -->
		<enumval name="ANY_STATE" description="NOTE: This should always be the last trigger!" />
	</enumdef>

	<structdef type="rage::ControlInput::InputSource">
		<enum	name="m_Type"		type="rage::ioMapperSource" />
		<array	name="m_Parameters"	type="atArray">
			<enum type="rage::ioMapperParameter" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::ComboState">
		<struct name="m_Source"		type="rage::ControlInput::InputSource" />
		<enum	name="m_Trigger"	type="rage::ControlInput::Trigger" />
	</structdef>

	<structdef type="rage::ControlInput::ComboStep">
		<array	name="m_States"		type="atArray">
			<struct type="rage::ControlInput::ComboState" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::ComboMapping">
		<enum	name="m_Input"		type="rage::InputType" />
		<u32	name="m_TimeDelta"	init="500" />
		<array	name="m_Steps"		type="atArray">
			<struct type="rage::ControlInput::ComboStep" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::MappingData">
		<array	name="m_ComboMappings"	type="atArray">
			<struct type="rage::ControlInput::ComboMapping" />
		</array>
	</structdef>
</ParserSchema>
