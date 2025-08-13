<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="class, psoChecks">	
				
 	<hinsert>
#include    "fwscene/mapdata/maptypes.h"
#include    "atl/bitset.h"
	</hinsert>

  <!--
		Texture Dictionary Relationship structure.
		No supporting code, parCodeGen generates the entire class for us.
	-->
  <structdef type="CTxdRelationship">
    <string name="m_parent" type="atString" description="Parent texture dictionary"/>
    <string name="m_child" type="atString" description="Child texture dictionary" ui_key="true"/>
    <hinsert>
      <![CDATA[
    bool IsValid() { return m_parent != m_child; }
  ]]>
    </hinsert>
  </structdef>
  
  <structdef type="CMultiTxdRelationship">
    <string name="m_parent" type="atString" description="Parent texture dictionary" ui_key="true"/>
		<array name="m_children" type="atArray">
			<string type="atString" />
		</array>
  </structdef>

	<!-- txd parenting for all .txds -->
	<structdef type="CMapParentTxds" simple="true" >
		<array name="m_txdRelationships" type="atArray">
			<struct type="CTxdRelationship" />
		</array>
	</structdef>
	
		
</ParserSchema>
	