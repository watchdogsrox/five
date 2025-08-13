<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAmbientAudio::Flags">
		<enumval name="IsInsulting"/>
		<enumval name="IsHarassing"/>
		<enumval name="ForcePlay"/>
		<enumval name="AllowRepeat"/>
		<enumval name="IsIgnored"/>
		<enumval name="IsRant"/>
		<enumval name="IsHostile"/>
		<enumval name="IsConversation"/>
	</enumdef>
	
	<structdef type="CAmbientAudio" onPostLoad="OnPostLoad">
		<string name="m_Context" type="atPartialHashValue"/>
		<bitset name="m_Flags" type="fixed8" values="CAmbientAudio::Flags"/>
	</structdef>

	<structdef type="CAmbientAudioExchange">
		<struct name="m_Initial" type="CAmbientAudio"/>
		<struct name="m_Response" type="CAmbientAudio"/>
	</structdef>

	<enumdef type="CAmbientAudioConditions::Flags">
		<enumval name="IsExercising"/>
		<enumval name="WillJeerAtHotPeds"/>
		<enumval name="IsFriendlyWithTarget"/>
		<enumval name="IsNotFriendlyWithTarget"/>
		<enumval name="IsThreatenedByTarget"/>
		<enumval name="IsNotThreatenedByTarget"/>
		<enumval name="IsTargetHot"/>
		<enumval name="IsTargetNotHot"/>
		<enumval name="IsTargetPlayer"/>
		<enumval name="IsTargetNotPlayer"/>
		<enumval name="TargetMustHaveResponse"/>
		<enumval name="IsTargetOnFoot"/>
		<enumval name="IsTargetMale"/>
		<enumval name="IsTargetNotMale"/>
		<enumval name="TargetMustBeKnockedDown"/>
		<enumval name="IsTargetCop"/>
		<enumval name="IsTargetGang"/>
		<enumval name="IsTargetFat"/>
		<enumval name="DidNotTalkToTargetLast"/>
		<enumval name="IsTargetFriendlyWithUs"/>
		<enumval name="IsTargetNotFriendlyWithUs"/>
		<enumval name="MustHaveContext"/>
		<enumval name="IsUsingScenario"/>
		<enumval name="TargetCanBeFleeing"/>
		<enumval name="TargetMustBeFleeing"/>
		<enumval name="TargetCanBeInCombat"/>
		<enumval name="TargetMustBeInCombat"/>
		<enumval name="TargetCanBeDead"/>
		<enumval name="TargetMustBeDead"/>
		<enumval name="IsFriendFollowedByPlayer"/>
		<enumval name="IsInGangTerritory"/>
		<enumval name="HasTargetAchievedCombatVictory"/>
	</enumdef>

	<structdef type="CAmbientAudioConditions::FollowUp">
		<string name="m_Context" type="atHashString"/>
		<float name="m_MaxTime" init="0.0f"/>
	</structdef>

	<structdef type="CAmbientAudioConditions::Friends">
		<u8 name="m_Min" init="0"/>
		<u8 name="m_Max" init="255"/>
		<float name="m_MaxDistance" init="-1.0f"/>
	</structdef>

	<structdef type="CAmbientAudioConditions" onPostLoad="OnPostLoad">
		<array name="m_PedTypes" type="atArray">
			<string type="ConstString"/>
		</array>
		<array name="m_ModelNames" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_PedPersonalities" type="atArray">
			<string type="atHashString"/>
		</array>
		<float name="m_Chances" init="1.0f" min="0.0f" max="1.0f"/>
		<float name="m_MinDistance" init="0.0f" min="0.0f" max="100.0f"/>
		<float name="m_MaxDistance" init="100.0f" min="0.0f" max="100.0f"/>
		<float name="m_MinDistanceFromPlayer" init="0.0f" min="0.0f" max="100.0f"/>
		<float name="m_MaxDistanceFromPlayer" init="100.0f" min="0.0f" max="100.0f"/>
		<bitset name="m_Flags" type="fixed32" values="CAmbientAudioConditions::Flags"/>
		<struct name="m_WeSaid" type="CAmbientAudioConditions::FollowUp"/>
		<struct name="m_TheySaid" type="CAmbientAudioConditions::FollowUp"/>
		<struct name="m_Friends" type="CAmbientAudioConditions::Friends"/>
		<float name="m_NoRepeat" init="0.0f" min="0.0f" max="600.0f"/>
	</structdef>

	<structdef type="CAmbientAudioTrigger::ConditionalExchange">
		<struct name="m_Conditions" type="CAmbientAudioConditions"/>
		<string name="m_Exchange" type="atHashString"/>
		<array name="m_Exchanges" type="atArray">
			<string type="atHashString"/>
		</array>
	</structdef>

	<structdef type="CAmbientAudioTrigger::WeightedExchange">
		<float name="m_Weight" init="1.0f"/>
		<string name="m_Exchange" type="atHashString"/>
	</structdef>

	<structdef type="CAmbientAudioTrigger">
		<struct name="m_Conditions" type="CAmbientAudioConditions"/>
		<string name="m_Exchange" type="atHashString"/>
		<array name="m_Exchanges" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_WeightedExchanges" type="atArray">
			<struct type="CAmbientAudioTrigger::WeightedExchange"/>
		</array>
		<array name="m_ConditionalExchanges" type="atArray">
			<struct type="CAmbientAudioTrigger::ConditionalExchange"/>
		</array>
	</structdef>

	<structdef type="CAmbientAudioSet">
		<string name="m_Parent" type="atHashString"/>
		<array name="m_Triggers" type="atArray">
			<struct type="CAmbientAudioTrigger"/>
		</array>
	</structdef>

	<structdef type="CAmbientAudios">
		<map name="m_Exchanges" type="atBinaryMap" key="atHashString">
			<struct type="CAmbientAudioExchange" />
		</map>
		<map name="m_Sets" type="atBinaryMap" key="atHashString">
			<struct type="CAmbientAudioSet" />
		</map>
	</structdef>
	
</ParserSchema>
