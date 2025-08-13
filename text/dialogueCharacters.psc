<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	
	<structdef type="SDialogueCharacters::SCharacterData">
		<string name="m_txd" type="atFinalHashString" init="0" />
		<string name="m_VoiceNameHash" type="atHashString" init="0" />
		<string name="m_MissionNameHash" type="atHashString" />
	</structdef>
  
	<structdef type="SDialogueCharacters">
		<array name="m_characterInfo" type="atArray">
			<struct type="SDialogueCharacters::SCharacterData"/>
		</array>
	</structdef>

</ParserSchema>