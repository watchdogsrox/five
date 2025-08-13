<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAgitatedReaction::Flags">
		<enumval name="DontClearFlags"/>
		<enumval name="DontClearSay"/>
	</enumdef>
	
	<structdef type="CAgitatedReaction">
		<pointer name="m_Condition" type="CAgitatedCondition" policy="owner"/>
		<pointer name="m_Action" type="CAgitatedAction" policy="owner"/>
		<struct name="m_ConditionalAction" type="CAgitatedConditionalAction"/>
		<string name="m_State" type="atHashString" />
		<float name="m_Anger" min="0.0f" max="1.0f" init="0.0f" />
		<float name="m_Fear" min="0.0f" max="1.0f" init="0.0f" />
		<struct name="m_Say" type="CAgitatedSay" />
		<struct name="m_ConditionalSay" type="CAgitatedConditionalSay" />
		<array name="m_ConditionalSays" type="atArray">
			<struct type="CAgitatedConditionalSay" />
		</array>
		<bitset name="m_Flags" type="fixed8" values="CAgitatedReaction::Flags"/>
	</structdef>
	  
</ParserSchema>
