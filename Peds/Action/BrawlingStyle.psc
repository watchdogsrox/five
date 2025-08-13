<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CBrawlingStyleData">
	<string name="m_Name" type="atHashString"/>
	<float name="m_TargetRadius" init="1.5" />
	<float name="m_KeepMovingWhilePathingDistance" init="4.0f" />
	<float name="m_MaxDistanceMayAdjustPathEndPosition" init="0.5f" />
	<s32 name="m_SeekModeScanTimeMin" init="500" />
	<s32 name="m_SeekModeScanTimeMax" init="3000" />
	<float name="m_MeleeMovementMBR" init="2.0f" />
	<s32 name="m_AttackFrequencyWorstFighterMinInMs" init="500" />
	<s32 name="m_AttackFrequencyWorstFighterMaxInMs" init="2000" />
	<s32 name="m_AttackFrequencyBestFighterMinInMs" init="100" />
	<s32 name="m_AttackFrequencyBestFighterMaxInMs" init="1500" />
	<float name="m_AttackRangeMax" init="3.0f" />
	<float name="m_AttackProbabilityToComboMin" init="0.6f" />
	<float name="m_AttackProbabilityToComboMax" init="0.85f" />
	<float name="m_ProbabilityToBeDazedMin" init="0.1f" />
	<float name="m_ProbabilityToBeDazedMax" init="0.02f" />
	<float name="m_BlockProbabilityMin" init="0.05f" />
	<float name="m_BlockProbabilityMax" init="0.35f" />
	<float name="m_CounterProbabilityMin" init="0.05f" />
	<float name="m_CounterProbabilityMax" init="0.35f" />
	<s32 name="m_TauntFrequencyMinInMs" init="3000" />
	<s32 name="m_TauntFrequencyMaxInMs" init="5000" />
	<float name="m_TauntProbability" init="0.9f" />
	<s32 name="m_TauntFrequencyQueuedMinInMs" init="1000" />
	<s32 name="m_TauntFrequencyQueuedMaxInMs" init="2500" />
	<float name="m_TauntProbabilityQueued" init="0.6f" />
	<bool name="m_PlayTauntBeforeAttacking" init="false" />
</structdef>

<structdef type="CBrawlingStyleManager">
	<array name="m_aBrawlingData" type="atArray">
		<struct type="CBrawlingStyleData"/>
	</array>
</structdef>

</ParserSchema>
