<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CEventShocking::ShockingEventReactionMode">
    <enumval name="NO_REACTION"/>
    <enumval name="SMALL_REACTION"/>
    <enumval name="BIG_REACTION"/>
  </enumdef>

  <structdef type="CEventShocking::Tunables"  base="CTuning">
    <float name="m_LifeTime" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Life time for this event (in seconds), if not controlled by special rules."/>
    <float name="m_VisualReactionRange" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Range at which this event can be visually observed (in meters)."/>
    <float name="m_CopInVehicleVisualReactionRange" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Range at which this event can be visually observed by cops in vehicles(in meters)."/>
    <float name="m_AudioReactionRange" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Range at which this event can be heard (in meters)."/>
    <float name="m_AIOnlyReactionRangeScaleFactor" min="-1.0f" max="1000.0f" step="0.01f" init="-1.0f" description="Factor that this event, if it does not involve the player, will have its perception distances scaled by.  -1 means to not scale."/>
    <float name="m_DuckAndCoverCanTriggerForPlayerTime" init="0.0f" min="0.0f" max="1000.0f" step="0.1f" description="This event may trigger a duck-and-cover reaction if the player presses the right button within this time from the event start."/>
    <float name="m_GotoWatchRange" min="0.0f" max="1000.0f" step="1.0f" init="5.0f" description="If approaching to watch, this is the range to stand and watch at (in meters)."/>
    <float name="m_StopWatchDistance" min="0.0f" max="1000.0f" step="1.0f" init="20.0f" description="The range used to determine if we are close enough to keep watching a shocking event (in meters)."/>
    <float name="m_HurryAwayMBRChangeDelay" min="0.0f" max="100.0f" step="0.5f" init="0.0f" description="If hurrying away from this event, this is how long to wait in seconds before changing the MBR."/>
    <float name="m_HurryAwayMBRChangeRange" min="0.0f" max="100.0f" step="0.5f" init="10.0f" description="If hurrying away from this event, this is the distance threshold for determining if the MBR should be changed."/>
    <float name="m_HurryAwayInitialMBR" min="1.0f" max="3.0f" step="0.01f" init="1.66f" description="If hurrying away from this event, this is the move blend ratio to start the navmesh task with." />
    <float name="m_HurryAwayMoveBlendRatioWhenFar" min="1.0f" max="3.0f" step="0.01f" init="1.25f" description="If hurrying away from this event and not within 10 meters, this is the speed (move blend ratio) to use."/>
    <float name="m_HurryAwayMoveBlendRatioWhenNear" min="1.0f" max="3.0f" step="0.01f" init="2.0f" description="If hurrying away from this event and within 10 meters of it, this is the speed (move blend ratio) to use."/>
    <float name="m_MinWatchTime" min="0.0f" max="1000.0f" step="1.0f" init="1.0f" description="Min time (in seconds) to watch this event, if deciding to watch."/>
    <float name="m_MaxWatchTime" min="0.0f" max="1000.0f" step="1.0f" init="2.0f"  description="Max time (in seconds) to watch this event, if deciding to watch."/>
		<float name="m_MinWatchTimeHurryAway" min="0.0f" max="1000.0f" step="1.0f" init="1.0f" description="Min time (in seconds) to watch this event, if deciding to watch and hurrying away afterwards."/>
		<float name="m_MaxWatchTimeHurryAway" min="0.0f" max="1000.0f" step="1.0f" init="2.0f" description="Max time (in seconds) to watch this event, if deciding to watch and hurrying away afterwards."/>
		<float name="m_ChanceOfWatchRatherThanHurryAway" min="0.0f" max="1.0f" step="1.0f" init="0.0f"  description="If the response is hurry away, this determines the chance of them just watching."/>
		<float name="m_MinPhoneFilmTime" min="0.0f" max="1000.0f" step="0.5f"	init="4.0f"	description="Min time (in seconds) to film this event with a phone, if deciding to."/>
		<float name="m_MaxPhoneFilmTime" min="0.0f" max="1000.0f" step="0.5f" init="8.0f" description="Max time (in seconds) to film this event with a phone, if deciding to."/>
		<float name="m_MinPhoneFilmTimeHurryAway" min="0.0f" max="1000.0f" step="0.5f"	init="2.0f"	description="Min time (in seconds) to film this event with a phone, if deciding to and hurrying away afterwards."/>
		<float name="m_MaxPhoneFilmTimeHurryAway" min="0.0f" max="1000.0f" step="0.5f" init="4.0f" description="Max time (in seconds) to film this event with a phone, if deciding to and hurrying away afterwards."/>
		<float name="m_ChanceOfFilmingEventOnPhoneIfWatching" min="0.0f" max="1.0f" step="0.1f" init="0.5f" description="If the response is to watch the event, this is the chance the ped will pull out a phone and film it."/>
		<float name="m_PedGenBlockedAreaMinRadius" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Minimum radius of ped gen blocking area, if enabled."/>
    <float name="m_WanderInfluenceSphereRadius" min="0.0f" max="100.0f" step="1.0f" init="0.0f" description="Radius of influence sphere around this event, affecting wandering peds."/>
    <float name="m_TriggerAmbientReactionChances" min="0.0f" max="1.0f" step="0.01f" init="0.0f" description="Chances of triggering an ambient reaction."/>
    <float name="m_MinDistanceForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Minimum distance to trigger an ambient reaction."/>
    <float name="m_MaxDistanceForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Maximum distance to trigger an ambient reaction."/>
    <float name="m_AmbientEventLifetime" min="0.0f" max="1000.0f" step="1.0f" init="3.0f" description="How long to keep the ambient event in memory."/>
    <float name="m_MinTimeForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Minimum time to play an ambient reaction."/>
    <float name="m_MaxTimeForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Maximum time to play an ambient reaction."/>
    <float name="m_PedFearImpact" min="-1.0f" max="1.0f" step="0.01f" init="0.0f" description="How much this event should impact the fear motivation of a ped that responds to it."/>
    <float name="m_ShockingSpeechChance" min="0.0f" max="1.0f" init="0.0f" description="Chance of saying the shocking speech hash when reacting to this event with certain tasks."/>
    <float name="m_MinDelayTimer" min="0.0f" max="0.5f" step="0.1f" init="0.1f" description="Minimum delay for peds to react to this event."/>
    <float name="m_MaxDelayTimer" min="0.2f" max="1.0f" step="0.1f" init="0.6f" description="Maximum delay for peds to react to this event."/>
    <float name="m_DuplicateDistanceCheck" min="0.0f" max="50.0f" step="1.0f" init="10.0f" description="How far away between react positions is sufficient to call a non-entity shocking event of the same type as unique."/>
    <float name="m_MaxTimeForAudioReaction" min="0.0f" max="10000.0f" step="1.0f" init="4000.0f" description="How long after an event occurs can we play an audio reaction."/>
    <float name="m_DistanceToUseGunfireReactAndFleeAnimations" min="-1.0f" max="9999.9f" init="-1.0f" step="0.1f" description="When ReactAndFlee is run as a response task to this event, use the gunfire set of animations if the distance is smaller than this value (-1 means never)."/>
    <u32 name="m_PedGenBlockingAreaLifeTimeMS" min="0" max="60000" step="500" init="5000" description="Time in milleseconds to create a blocking area for if m_AddPedGenBlockedArea is set."/>
    <u32 name="m_DuplicateTimeCheck" init="3000" description="How long between start times is sufficient to call an event of the same type as unique."/>
    <string name="m_ShockingSpeechHash"     type="atHashString" description="What to say when a shocking event reaction starts to this event."/>
		<string name="m_ShockingFilmSpeechHash" type="atHashString" description="What to say when a shocking event reaction films the event on a phone."/>
    <int name="m_Priority" min="-1" max="4" step="1" init="-1" description="Priority value used if the global group fills up, and for prioritizing reactions."/>
    <enum name ="m_AmbientEventType" type="AmbientEventType" init="AET_Interesting" description="Which sort of ambient event this shocking event can generate."/>
    <bool name="m_AddPedGenBlockedArea" init="0" description="If true, ped spawning will be blocked near the event."/>
    <bool name="m_CausesVehicleAvoidance" init="0" description="If true, vehicles will steer to avoid this event."/>
    <bool name="m_AllowIgnoreAsLowPriority" init="0" description="If true, this event will be ignored if SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS is active."/>
    <bool name="m_DebugDisplayAlwaysUseEventPosition" init="0" description="If true, when debug displaying the event, always use the position stored in the event, rather than the position of the source entity if available."/>
    <bool name="m_DebugDisplayListPlayerInfo" init="1" description="If true, display PLAYER in the shocking event debug display list, if the source is the player."/>
    <bool name="m_HurryAwayWatchFirst" init="1" description="If hurrying away from this event, the ped may watch it briefly first if this is set."/>
    <bool name="m_MobileChatScenarioMayAbort" init="1" description="If a ped is running CTaskMobileChatScenario and receives this event, this must be true for the scenario to abort."/>
    <bool name="m_WatchSayFightCheers" init="0" description="When watching this event, if this is set, fight cheers may be said."/>
    <bool name="m_WatchSayShocked" init="1" description="If true, when watching this event, speech like SHOCKED and SURPRISED may be generated."/>
    <bool name="m_VehicleSlowDown" init="0" description="If true, and in a vehicle, slow down the vehicle when near the event."/>
    <bool name="m_IgnoreIfSensingPedIsOtherEntity" init="1" description="If true, Peds should ignore this event if they are the other entity."/>
    <bool name="m_IgnorePavementChecks" init="0" description="If true, peds should ignore the default behavior in CTaskShockingEventHurryAway of always resuming if there is not pavement."/>
    <bool name="m_AllowScanningEvenIfPreviouslyReacted" init="0" description="This event can be added to the ped's event queue even if they have previously reacted to it."/>
    <enum name="m_ReactionMode" type="CEventShocking::ShockingEventReactionMode" init="CEventShocking::BIG_REACTION" description="Determines the intensity of the animated response to the shocking event."/>
    <bool name="m_StopResponseWhenExpired" init="0" description="If true, then the response task should try and quit if the event has expired."/>
    <bool name="m_FleeIfApproachedByOtherEntity" init="0" description="If true, then the ped will flee if the m_pEntityOther gets too close."/>
    <bool name="m_FleeIfApproachedBySourceEntity" init="0" description="If true, then the ped will flee if the m_pEntitySource gets too close."/>
    <bool name="m_CanCallPolice" init="0"/>
	<bool name="m_IgnoreFovForHeadIk" init="1"/>
	<bool name="m_ReactToOtherEntity" init="0"/>
  </structdef>
</ParserSchema>
