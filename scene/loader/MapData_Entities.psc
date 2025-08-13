<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">
			
	<hinsert>
#include "fwscene/mapdata/mapdata.h"
	</hinsert>
	
	<!-- Entity definition -->
	<structdef type="CEntityDef" base="::rage::fwEntityDef">
		<int 		name="m_ambientOcclusionMultiplier" min="0" max="255" init="255"/>
    <int    name="m_artificialAmbientOcclusion" min="0" max="255" init="255"/>
    <u32    name="m_tintValue" min="0" max="7" init="0"/>
	</structdef>

</ParserSchema>