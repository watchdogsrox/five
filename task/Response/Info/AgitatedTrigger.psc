<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAgitatedTriggerReaction::Flags">
		<enumval name="DisableWhenAgitated"/>
		<enumval name="DisableWhenFriendlyIsAgitated"/>
		<enumval name="MustBeUsingScenario"/>
		<enumval name="MustBeUsingScenarioBeforeInitialReaction"/>
		<enumval name="MustBeWandering"/>
		<enumval name="MustBeWanderingBeforeInitialReaction"/>
		<enumval name="NoLineOfSightNeeded"/>
		<enumval name="UseDistanceFromScenario"/>
		<enumval name="MustBeUsingATerritorialScenario"/>
		<enumval name="DisableWhenWandering"/>
		<enumval name="DisableWhenStill"/>
		<enumval name="DisableIfNeitherPedIsOnFoot"/>
		<enumval name="MustBeOnFoot"/>
		<enumval name="TargetMustBeOnFoot"/>
		<enumval name="MustBeInVehicle"/>
		<enumval name="MustBeStationary"/>
	</enumdef>

	<structdef type="CAgitatedTriggerReaction::Time">
		<float name="m_Min" init="0.0f" min="0.0f" max="600.0f"/>
		<float name="m_Max" init="0.0f" min="0.0f" max="600.0f"/>
	</structdef>

	<structdef type="CAgitatedTriggerReaction">
		<enum name="m_Type" type="AgitatedType"/>
		<struct name="m_TimeBeforeInitialReaction" type="CAgitatedTriggerReaction::Time"/>
		<struct name="m_TimeBetweenEscalatingReactions" type="CAgitatedTriggerReaction::Time"/>
		<struct name="m_TimeAfterLastSuccessfulReaction" type="CAgitatedTriggerReaction::Time"/>
		<struct name="m_TimeAfterInitialReactionFailure" type="CAgitatedTriggerReaction::Time"/>
		<float name="m_MinDotToTarget" init="-1.0f" min="-1.0f" max="1.0f" />
		<float name="m_MaxDotToTarget" init="1.0f" min="-1.0f" max="1.0f" />
		<float name="m_MinTargetSpeed" init="0.0f" min="0.0f" max="50.0f" />
		<float name="m_MaxTargetSpeed" init="0.0f" min="0.0f" max="50.0f" />
		<u8 name="m_MaxReactions" init="1" min="0" max="255"/>
		<bitset name="m_Flags" type="fixed16" values="CAgitatedTriggerReaction::Flags"/>
	</structdef>

	<structdef type="CAgitatedTrigger" onPostLoad="OnPostLoad">
		<array name="m_PedTypes" type="atArray">
			<string type="ConstString"/>
		</array>
		<float name="m_Chances" init="1.0f" min="0.0f" max="1.0f"/>
		<float name="m_Distance" init="10.0f" min="0.0f" max="100.0f"/>
		<string name="m_Reaction" type="atHashString"/>
	</structdef>

	<enumdef type="CAgitatedTriggerSet::Flags">
		<enumval name="IgnoreHonkedAt"/>
	</enumdef>

	<structdef type="CAgitatedTriggerSet">
		<string name="m_Parent" type="atHashString"/>
		<bitset name="m_Flags" type="fixed32" values="CAgitatedTriggerSet::Flags"/>
		<array name="m_Triggers" type="atArray">
			<struct type="CAgitatedTrigger" />
		</array>
	</structdef>

	<structdef type="CAgitatedTriggers">
		<map name="m_Reactions" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedTriggerReaction" />
		</map>
		<map name="m_Sets" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedTriggerSet" />
		</map>
	</structdef>
	  
</ParserSchema>
