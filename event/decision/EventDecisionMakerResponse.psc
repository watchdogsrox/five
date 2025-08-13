<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CEventDataResponse_Decision" onPostLoad="PostLoadCB">
	<string name="TaskRef" type="atHashString"/>
	
	<float name="Chance_SourcePlayer"/>
	<float name="Chance_SourceFriend"/>
	<float name="Chance_SourceThreat"/>
	<float name="Chance_SourceOther"/>
	
	<float name="DistanceMinSq"/>
	<float name="DistanceMaxSq"/>

	<string name="SpeechHash" type="atHashString"/>

	<bitset name="EventResponseDecisionFlags" type="fixed" numBits="use_values" values="CEventResponseDecisionFlags::Flags"/>
	<bitset name="EventResponseFleeFlags" type="fixed8" numBits="8" values="EventResponseFleeFlagsEnum"/>
</structdef>

<structdef type="CEventDecisionMakerResponse::sCooldown">
  <float name="Time" init="-1.0" />
  <float name="MaxDistance" init="-1.0" />
</structdef>
  
<structdef type="CEventDecisionMakerResponse">
	<string name="m_Name" type="atHashString"/>

	<enum name="m_Event" type="eEventType"/>
  <struct name="m_Cooldown" type="CEventDecisionMakerResponse::sCooldown"/>
	<array name="m_Decision" type="atArray">
		<struct type="CEventDataResponse_Decision"/>
	</array>
</structdef>

</ParserSchema>
