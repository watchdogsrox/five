<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CCommunityStatsData">
	<string name="m_StatId" type="atHashString"/>
  <float name="m_Value"/>
  <array name="m_HistoryDepth" type="atArray" >
		<float/>
	</array>
</structdef>

 <structdef type="CCommunityStatsDataMgr">
   <array name="m_Data" type="atArray" >
     <struct type="CCommunityStatsData"/>
   </array>
 </structdef>
  
</ParserSchema>
