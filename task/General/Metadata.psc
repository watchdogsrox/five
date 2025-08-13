<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
  
  <structdef type="CTaskConversationHelper::Tunables" base="CTuning" >
    <float name="m_fMinSecondsDelayBetweenPhoneLines" min="0.0f" init="3.0f"/>
    <float name="m_fMaxSecondsDelayBetweenPhoneLines" max="10.0f" init="6.0f"/>
    <float name="m_fMinSecondsDelayBetweenChatLines" init="5.0f"/>
    <float name="m_fMaxSecondsDelayBetweenChatLines" init="15.0f"/>
    <float name="m_fMinDistanceSquaredToPlayerForAudio" init="400.0f"/>
		<float name="m_fChanceOfConversationRant" min="0.f" max="1.f" init="0.1f" step="0.05f"/>
		<float name="m_fChanceOfArgumentChallenge" min="0.f" max="1.f" init="0.5f" step="0.05f"/>
		<float name="m_fChanceOfArgumentChallengeBeingAccepted" min="0.f" max="1.f" init="0.5f" step="0.05f"/>
    <u32   name="m_uTimeInMSUntilNewWeirdPedComment" init="10000"/>
		<u32   name="m_uMaxTimeInMSToPlayRingTone"			 init="7000"/>
    <u8    name="m_uTimeToWaitAfterNewSayFailureInSeconds" init="1"/>
    <u8    name="m_uTicksUntilHangoutConversationCheck" init="30"/>
  </structdef>

	<structdef type="CTaskCallPolice::Tunables" base="CTuning" >
		<float name="m_MinTimeMovingAwayToGiveToWitness" min="0.0f" max="5.0f" init="1.0f"/>
		<float name="m_MinTimeSinceTalkingEndedToMakeCall" min="0.0f" max="5.0f" init="1.0f"/>
		<float name="m_MinTimeSinceTargetTalkingEndedToMakeCall" min="0.0f" max="5.0f" init="1.0f"/>
		<float name="m_MinTimeTargetHasBeenTalkingToMakeCall" min="0.0f" max="5.0f" init="5.0f"/>
		<float name="m_MinTimeSinceTalkingEndedToSayContextForCall" min="0.0f" max="5.0f" init="2.0f"/>
		<float name="m_MinTimeSpentInEarLoopToSayContextForCall" min="0.0f" max="5.0f" init="2.0f"/>
		<float name="m_MinTimeToSpendInEarLoopToPutDownPhone" min="0.0f" max="10.0f" init="4.0f"/>
		<float name="m_MaxTimeToSpendInEarLoopToPutDownPhone" min="0.0f" max="10.0f" init="8.0f"/>
	</structdef>

</ParserSchema>
