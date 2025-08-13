<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


	<structdef type="CIplCullBoxEntry">
		<string name="m_name" type="atHashString" ui_key="true" />
    <struct name="m_aabb" type="::rage::spdAABB" />
		<array name="m_culledContainerHashes" type="atArray">
			<u32 />
		</array>
    <bool name="m_bEnabled" />
	</structdef>

	<structdef type="CIplCullBoxFile">
		<array name="m_entries" type="atArray">
			<struct type="CIplCullBoxEntry" />
		</array>
	</structdef>
	
</ParserSchema>
	