<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAgitatedResponse::Flags">
		<enumval name="ProcessLeader" />
	</enumdef>
	
	<enumdef type="CAgitatedResponse::OnFootClipSet::Flags">
		<enumval name="DontReset" />
	</enumdef>
	
	<structdef type="CAgitatedResponse::OnFootClipSet">
		<pointer name="m_Condition" type="CAgitatedCondition" policy="owner" />
		<string name="m_Name" type="atHashString" />
		<bitset name="m_Flags" type="fixed8" values="CAgitatedResponse::OnFootClipSet::Flags" />
	</structdef>
	
	<structdef type="CAgitatedResponse">
		<bitset name="m_Flags" type="fixed8" values="CAgitatedResponse::Flags" />
		<array name="m_OnFootClipSets" type="atArray">
			<struct type="CAgitatedResponse::OnFootClipSet" />
		</array>
		<struct name="m_TalkResponse" type="CAgitatedTalkResponse" />
		<array name="m_TalkResponses" type="atArray">
			<struct type="CAgitatedTalkResponse" />
		</array>
		<map name="m_FollowUps" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedTalkResponse" />
		</map>
		<map name="m_States" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedState" />
		</map>
		<map name="m_Reactions" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedReaction" />
		</map>
	</structdef>
	
	<structdef type="CAgitatedResponses">
		<map name="m_Responses" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedResponse" />
		</map>
	</structdef>
  
</ParserSchema>
