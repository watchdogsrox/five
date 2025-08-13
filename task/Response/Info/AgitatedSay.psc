<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAgitatedSay::Flags">
		<enumval name="CanInterrupt"/>
		<enumval name="IgnoreForcedFailure"/>
		<enumval name="WaitForExistingAudioToFinish"/>
		<enumval name="WaitUntilFacing"/>
	</enumdef>
	
	<structdef type="CAgitatedSay">
		<struct name="m_Audio" type="CAmbientAudio" />
		<array name="m_Audios" type="atArray">
			<struct type="CAmbientAudio" />
		</array>
		<bitset name="m_Flags" type="fixed8" values="CAgitatedSay::Flags"/>
	</structdef>

	<structdef type="CAgitatedConditionalSay">
		<pointer name="m_Condition" type="CAgitatedCondition" policy="owner"/>
		<struct name="m_Say" type="CAgitatedSay" />
	</structdef>

	<structdef type="CAgitatedTalkResponse">
		<struct name="m_Audio" type="CAmbientAudio" />
	</structdef>

	<structdef type="CAgitatedConditionalTalkResponse">
		<pointer name="m_Condition" type="CAgitatedCondition" policy="owner"/>
		<struct name="m_TalkResponse" type="CAgitatedTalkResponse" />
	</structdef>
	  
</ParserSchema>
