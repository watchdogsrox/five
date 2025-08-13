<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CDecisionPageBase" base="CCategorylessPageBase" constructible="false">
    <pointer name="m_trueAction" parName="OnTrue" type="uiPageDeckMessageBase" policy="owner" />
    <pointer name="m_falseAction" parName="OnFalse" type="uiPageDeckMessageBase" policy="owner" />
	</structdef>
  
</ParserSchema>
