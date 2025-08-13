<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAgitatedState::Flags">
		<enumval name="InitialState" />
		<enumval name="FinalState" />
		<enumval name="UseWhenForcedAudioFails" />
		<enumval name="ClearHostility" />
	</enumdef>

	<structdef type="CAgitatedState">
		<pointer name="m_Action" type="CAgitatedAction" policy="owner"/>
		<struct name="m_ConditionalAction" type="CAgitatedConditionalAction"/>
		<array name="m_ConditionalActions" type="atArray">
			<struct type="CAgitatedConditionalAction" />
		</array>
		<bitset name="m_Flags" type="fixed8" values="CAgitatedState::Flags" />
		<array name="m_Reactions" type="atArray">
			<string type="atHashString" />
		</array>
		<float name="m_TimeToListen" min="-1.0f" max="60.0f" init="1.0f" />
		<struct name="m_TalkResponse" type="CAgitatedTalkResponse" />
		<struct name="m_ConditionalTalkResponse" type="CAgitatedConditionalTalkResponse"/>
		<array name="m_TalkResponses" type="atArray">
			<struct type="CAgitatedTalkResponse" />
		</array>
	</structdef>
	
</ParserSchema>
