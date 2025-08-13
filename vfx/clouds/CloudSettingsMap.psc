<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef name="CloudSettingsNamed" type="rage::CloudSettingsNamed">
	<string name="m_Name" type="atHashString"/>
	<struct name="m_Settings" type="rage::CloudHatSettings"/>
</structdef>

<structdef name="CloudSettingsMap" type="rage::CloudSettingsMap" onPostLoad="PostLoad" onPreSave="PreSave" onPostSave="PostSave">
  <array name="m_KeyframeTimes" type="atArray">
    <float init="0.0f" min="0.0f" max="23.0f"/>
  </array>
  <array name="m_SettingsMap" type="atArray">
		<struct type="rage::CloudSettingsNamed"/>
	</array>
</structdef>

</ParserSchema>