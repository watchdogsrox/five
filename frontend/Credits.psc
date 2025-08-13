<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="eCreditType">
	<enumval name="JOB_BIG"/>
	<enumval name="JOB_MED"/>
	<enumval name="JOB_SMALL"/>
	<enumval name="NAME_BIG"/>
	<enumval name="NAME_MED"/>
	<enumval name="NAME_SMALL"/>
	<enumval name="SPACE_BIG"/>
	<enumval name="SPACE_MED"/>
	<enumval name="SPACE_SMALL"/>
  <enumval name="SPACE_END"/>
  <enumval name="SPRITE_1"/>
  <enumval name="LEGALS"/>
  <enumval name="AUDIO_NAME"/>
  <enumval name="AUDIO_LEGALS"/>
  <enumval name="JOB_AND_NAME_MED"/>
</enumdef>

<structdef type="CCreditItem">
	<enum name="LineType" type="eCreditType"/>
	<string name="cTextId1" type="atString"/>
	<string name="cTextId2" type="atString"/>
	<string name="cTextId3" type="atString"/>
  <bool name="bLiteral" init="false"/>
</structdef>

<structdef type="CCreditArray">
	<array name="CreditItems" type="atArray">
		<struct type="CCreditItem"/>
	</array>
</structdef>

</ParserSchema>