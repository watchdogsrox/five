<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">

  <hinsert>
#include "fwscene/mapdata/maptypes.h"
  </hinsert>
	
	<structdef type="CBaseArchetypeDef" base="::rage::fwArchetypeDef">
	</structdef>
	
	<structdef type="CTimeArchetypeDef" base="CBaseArchetypeDef">
		<u32 name="m_timeFlags" />
	</structdef>
	
</ParserSchema>
	 