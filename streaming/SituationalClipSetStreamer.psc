<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CSituationalClipSetStreamer::Tunables::Avoids::Variation">
		<string name="m_ClipSet" type="atHashString"/>
		<bool name="m_IsCasual" init="false"/>
		<float name="m_Chances" min="0.0f" max="1.0f" init="0.0f"/>
	</structdef>
	
	<structdef type="CSituationalClipSetStreamer::Tunables::Avoids">
		<string name="m_ClipSet" type="atHashString"/>
		<string name="m_ClipSetForCasual" type="atHashString"/>
		<array name="m_Variations" type="atArray">
			<struct type="CSituationalClipSetStreamer::Tunables::Avoids::Variation"/>
		</array>
	</structdef>
	
	<structdef type="CSituationalClipSetStreamer::Tunables::FleeReactions">
		<string name="m_ClipSetForIntro" type="atHashString"/>
		<string name="m_ClipSetForIntroV1" type="atHashString"/>
		<string name="m_ClipSetForIntroV2" type="atHashString"/>
		<string name="m_ClipSetForRuns" type="atHashString"/>
		<string name="m_ClipSetForRunsV1" type="atHashString"/>
		<string name="m_ClipSetForRunsV2" type="atHashString"/>
		<float name="m_MinTimeInCombatToNotStreamIn" init="5.0f"/>
	</structdef>
	
	<structdef type="CSituationalClipSetStreamer::Tunables"  base="CTuning" >
		<struct name="m_Avoids" type="CSituationalClipSetStreamer::Tunables::Avoids"/>
		<struct name="m_FleeReactions" type="CSituationalClipSetStreamer::Tunables::FleeReactions"/>
	</structdef>

</ParserSchema>
