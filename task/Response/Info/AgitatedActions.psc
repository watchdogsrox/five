<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CAgitatedAction" constructable="false">
	</structdef>

	<structdef type="CAgitatedActionConditional" base="CAgitatedAction">
		<pointer name="m_Condition" type="CAgitatedCondition" policy="owner" />
		<pointer name="m_Action" type="CAgitatedAction" policy="owner" />
	</structdef>

	<structdef type="CAgitatedActionMulti" base="CAgitatedAction">
		<array name="m_Actions" type="atArray">
			<pointer type="CAgitatedAction" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CAgitatedActionChangeResponse" base="CAgitatedAction">
		<string name="m_Response" type="atHashString" />
		<string name="m_State" type="atHashString" />
	</structdef>

	<structdef type="CAgitatedActionTask" base="CAgitatedAction" constructable="false">
	</structdef>

	<structdef type="CAgitatedActionAnger" base="CAgitatedAction">
		<float name="m_Set" init="-1.0f" />
		<float name="m_Add" init="-1.0f" />
		<float name="m_Min" init="-1.0f" />
		<float name="m_Max" init="-1.0f" />
	</structdef>

	<structdef type="CAgitatedActionApplyAgitation" base="CAgitatedAction">
		<enum name="m_Type" type="AgitatedType" />
	</structdef>

	<structdef type="CAgitatedActionCallPolice" base="CAgitatedActionTask">
	</structdef>

	<structdef type="CAgitatedActionClearHash" base="CAgitatedAction">
		<string name="m_Value" type="atHashString" />
	</structdef>

	<structdef type="CAgitatedActionConfront" base="CAgitatedActionTask">
		<string name="m_AmbientClips" type="atHashString" />
	</structdef>

  <structdef type="CAgitatedActionEnterVehicle" base="CAgitatedActionTask">
  </structdef>

	<structdef type="CAgitatedActionExitVehicle" base="CAgitatedActionTask">
	</structdef>

	<structdef type="CAgitatedActionFace" base="CAgitatedActionTask">
		<string name="m_AmbientClips" type="atHashString" />
	</structdef>

	<structdef type="CAgitatedActionFight" base="CAgitatedActionTask">
	</structdef>

	<structdef type="CAgitatedActionFear" base="CAgitatedAction">
		<float name="m_Set" init="-1.0f" />
		<float name="m_Add" init="-1.0f" />
		<float name="m_Min" init="-1.0f" />
		<float name="m_Max" init="-1.0f" />
	</structdef>

	<structdef type="CAgitatedActionFollow" base="CAgitatedActionTask">
	</structdef>

	<structdef type="CAgitatedActionHurryAway" base="CAgitatedActionTask">
	</structdef>

	<enumdef type="CAgitatedActionFlee::Flags">
		<enumval name="ClaimPropInsteadOfDestroy" />
		<enumval name="ClaimPropInsteadOfDrop" />
	</enumdef>

	<structdef type="CAgitatedActionFlee" base="CAgitatedActionTask">
		<float name="m_FleeStopDistance" init="-1.0f" />
		<float name="m_MoveBlendRatio" init="2.0f" />
		<bool name="m_ForcePreferPavements" init="true" />
		<bool name="m_NeverLeavePavements" init="true" />
		<bool name="m_KeepMovingWhilstWaitingForFleePath" init="true" />
		<string name="m_AmbientClips" type="atHashString" />
		<bitset name="m_Flags" type="fixed8" values="CAgitatedActionFlee::Flags" />
	</structdef>

	<structdef type="CAgitatedActionFlipOff" base="CAgitatedAction">
		<float name="m_Time" min="0.0f" max="30.0f" init="5.0f" />
	</structdef>

	<structdef type="CAgitatedActionIgnoreForcedAudioFailures" base="CAgitatedAction">
	</structdef>

	<structdef type="CAgitatedActionMakeAggressiveDriver" base="CAgitatedAction">
	</structdef>

	<structdef type="CAgitatedActionReportCrime" base="CAgitatedAction">
	</structdef>
	
	<structdef type="CAgitatedActionReset" base="CAgitatedAction">
	</structdef>

	<structdef type="CAgitatedActionSay" base="CAgitatedAction">
		<struct name="m_Say" type="CAgitatedSay" />
	</structdef>

	<structdef type="CAgitatedActionSayAgitator" base="CAgitatedAction">
		<struct name="m_Audio" type="CAmbientAudio" />
	</structdef>

	<structdef type="CAgitatedActionSetFleeAmbientClips" base="CAgitatedAction">
		<string name="m_AmbientClips" type="atHashString" />
	</structdef>

	<structdef type="CAgitatedActionSetFleeMoveBlendRatio" base="CAgitatedAction">
		<float name="m_MoveBlendRatio" min="0.0f" max="3.0f" init="0.0f" />
	</structdef>

	<structdef type="CAgitatedActionSetHash" base="CAgitatedAction">
		<string name="m_Value" type="atHashString" />
	</structdef>

	<structdef type="CAgitatedActionStopVehicle" base="CAgitatedActionTask">
	</structdef>

	<structdef type="CAgitatedActionTurnOnSiren" base="CAgitatedAction">
		<float name="m_Time" min="0.0f" max="10.0f" init="0.0f" />
	</structdef>

	<structdef type="CAgitatedConditionalAction">
		<pointer name="m_Condition" type="CAgitatedCondition" policy="owner" />
		<pointer name="m_Action" type="CAgitatedAction" policy="owner" />
	</structdef>

  	<structdef type="CAgitatedScenarioExit" base="CAgitatedAction">
  	</structdef>

    <structdef type="CAgitatedScenarioNormalCowardExit" base="CAgitatedAction">
    </structdef>

	<structdef type="CAgitatedScenarioFastCowardExit" base="CAgitatedAction">
	</structdef>

	<structdef type="CAgitatedScenarioFleeExit" base="CAgitatedAction">
	</structdef>

	<structdef type="CAgitatedSetWaterSurvivalTime" base="CAgitatedAction">
		<float name="m_Time" min="0.0f" max="90.0f" init="30.0f" />
	</structdef>
	
</ParserSchema>
