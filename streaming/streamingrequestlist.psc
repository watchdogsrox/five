<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CStreamingRequestFrame" onPostLoad="OnPostLoad" onPostSave="OnPostSave" simple="true">
	<array name="m_AddList" type="atArray">
		<string type="atHashString"/>
	</array>
	<array name="m_RemoveList" type="atArray">
		<string type="atHashString"/>
	</array>
	<array name="m_PromoteToHDList" type="atArray">
		<string type="atHashString"/>
	</array>
  <Vec3V name="m_CamPos" />
  <Vec3V name="m_CamDir" />

  <array name="m_CommonAddSets" type="atArray">
    <u8 />
  </array>

  <u32 name="m_Flags" />
</structdef>

<structdef type="CStreamingRequestCommonSet" simple="true">
  <array name="m_Requests" type="atArray">
    <string type="atHashString"/>
  </array>
</structdef>

<structdef type="CStreamingRequestRecord" simple="true">
	<array name="m_Frames" type="atArray">
		<struct type="CStreamingRequestFrame"/>
	</array>
  <array name="m_CommonSets" type="atArray">
    <struct type="CStreamingRequestCommonSet"/>
  </array>
  <bool name="m_NewStyle" />
</structdef>

<structdef type="CStreamingRequestMasterList" simple="true">
	<array name="m_Files" type="atArray">
		<string type="atString"/>
	</array>
</structdef>

</ParserSchema>