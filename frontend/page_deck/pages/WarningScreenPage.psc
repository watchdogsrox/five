<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CWarningScreenPage" base="CCategorylessPageBase">
    <map name="m_resultMap" parName="ResultMap" type="atMap" key="enum" enumKeyType="eParsableWarningButtonFlags">
      <pointer type="uiPageDeckMessageBase" policy="owner" />
    </map>
    <string name="m_description" parName="Description" type="atHashString" />
    <bitset name="m_buttonFlags" parName="Buttons" type="fixed32" values="eParsableWarningButtonFlags"/>
	</structdef>
  
</ParserSchema>
