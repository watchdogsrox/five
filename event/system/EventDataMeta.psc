<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="class">

  <structdef type="CEventDataManager">
    <array  name="m_eventResponseTaskData" type="atArray">
      <pointer type="CEventDataResponseTask" policy="owner"/>
    </array>
    <array  name="m_eventDecisionMakerResponseData" type="atArray">
      <pointer type="CEventDecisionMakerResponse" policy="owner"/>
    </array>
    <array  name="m_eventDecisionMaker" type="atArray">
      <pointer type="CEventDataDecisionMaker" policy="owner"/>
    </array>
  </structdef>

  <enumdef type="EventResponseFleeFlagsEnum">
    <enumval name="DisableCrouch"/>
    <enumval name="DisableHandsUp"/>
    <enumval name="DisableCover"/>
	<enumval name="DisableVehicleExit"/>
	<enumval name="AllowVehicleOffRoad"/>
    <enumval name="IsShortTerm"/>
  </enumdef>

  <enumdef type="eEventType">
    <enumval name="EVENT_ACQUAINTANCE_PED_DISLIKE"/>
    <enumval name="EVENT_ACQUAINTANCE_PED_HATE"/>
    <enumval name="EVENT_ACQUAINTANCE_PED_LIKE"/>
    <enumval name="EVENT_ACQUAINTANCE_PED_RESPECT"/>
    <enumval name="EVENT_ACQUAINTANCE_PED_WANTED"/>
    <enumval name="EVENT_ACQUAINTANCE_PED_DEAD"/>
    <enumval name="EVENT_AGITATED"/>
    <enumval name="EVENT_AGITATED_ACTION"/>
    <enumval name="EVENT_ENCROACHING_PED"/>
    <enumval name="EVENT_CALL_FOR_COVER"/>
    <enumval name="EVENT_CAR_UNDRIVEABLE"/>
    <enumval name="EVENT_CLIMB_LADDER_ON_ROUTE"/>
    <enumval name="EVENT_CLIMB_NAVMESH_ON_ROUTE"/>
    <enumval name="EVENT_COMBAT_TAUNT"/>
    <enumval name="EVENT_COMMUNICATE_EVENT"/>
    <enumval name="EVENT_COP_CAR_BEING_STOLEN"/>
    <enumval name="EVENT_CRIME_REPORTED"/>
    <enumval name="EVENT_DAMAGE"/>
    <enumval name="EVENT_DEAD_PED_FOUND"/>
    <enumval name="EVENT_DEATH"/>
    <enumval name="EVENT_DRAGGED_OUT_CAR"/>
    <enumval name="EVENT_DUMMY_CONVERSION"/>
    <enumval name="EVENT_EXPLOSION"/>
    <enumval name="EVENT_EXPLOSION_HEARD"/>
    <enumval name="EVENT_FIRE_NEARBY"/>
    <enumval name="EVENT_FLUSH_TASKS"/>
    <enumval name="EVENT_FOOT_STEP_HEARD"/>
    <enumval name="EVENT_GET_OUT_OF_WATER"/>
    <enumval name="EVENT_GIVE_PED_TASK"/>
    <enumval name="EVENT_GUN_AIMED_AT"/>
    <enumval name="EVENT_HELP_AMBIENT_FRIEND"/>
    <enumval name="EVENT_INJURED_CRY_FOR_HELP"/>
    <enumval name="EVENT_CRIME_CRY_FOR_HELP"/>
    <enumval name="EVENT_IN_AIR"/>
    <enumval name="EVENT_IN_WATER"/>
    <enumval name="EVENT_INCAPACITATED"/>
    <enumval name="EVENT_LEADER_ENTERED_CAR_AS_DRIVER"/>
    <enumval name="EVENT_LEADER_ENTERED_COVER"/>
    <enumval name="EVENT_LEADER_EXITED_CAR_AS_DRIVER"/>
    <enumval name="EVENT_LEADER_HOLSTERED_WEAPON"/>
    <enumval name="EVENT_LEADER_LEFT_COVER"/>
    <enumval name="EVENT_LEADER_UNHOLSTERED_WEAPON"/>
    <enumval name="EVENT_MELEE_ACTION"/>
    <enumval name="EVENT_MUST_LEAVE_BOAT"/>
    <enumval name="EVENT_NEW_TASK"/>
    <enumval name="EVENT_NONE"/>
    <enumval name="EVENT_OBJECT_COLLISION"/>
    <enumval name="EVENT_ON_FIRE"/>
    <enumval name="EVENT_OPEN_DOOR"/>
    <enumval name="EVENT_SHOVE_PED"/>
    <enumval name="EVENT_PED_COLLISION_WITH_PED"/>
    <enumval name="EVENT_PED_COLLISION_WITH_PLAYER"/>
    <enumval name="EVENT_PED_ENTERED_MY_VEHICLE"/>
    <enumval name="EVENT_PED_JACKING_MY_VEHICLE"/>
    <enumval name="EVENT_PED_ON_CAR_ROOF"/>
    <enumval name="EVENT_PED_TO_CHASE"/>
    <enumval name="EVENT_PED_TO_FLEE"/>
    <enumval name="EVENT_PLAYER_COLLISION_WITH_PED"/>
    <enumval name="EVENT_PLAYER_LOCK_ON_TARGET"/>
    <enumval name="EVENT_POTENTIAL_BE_WALKED_INTO"/>
	<enumval name="EVENT_POTENTIAL_BLAST"/>
    <enumval name="EVENT_POTENTIAL_GET_RUN_OVER"/>
    <enumval name="EVENT_POTENTIAL_WALK_INTO_FIRE"/>
    <enumval name="EVENT_POTENTIAL_WALK_INTO_OBJECT"/>
    <enumval name="EVENT_POTENTIAL_WALK_INTO_VEHICLE"/>
	<enumval name="EVENT_PROVIDING_COVER"/>
    <enumval name="EVENT_RADIO_TARGET_POSITION"/>
    <enumval name="EVENT_RAN_OVER_PED"/>
    <enumval name="EVENT_REACTION_COMBAT_VICTORY"/>
    <enumval name="EVENT_REACTION_ENEMY_PED"/>
    <enumval name="EVENT_REACTION_INVESTIGATE_DEAD_PED"/>
    <enumval name="EVENT_REACTION_INVESTIGATE_THREAT"/>
    <enumval name="EVENT_REQUEST_HELP_WITH_CONFRONTATION"/>
    <enumval name="EVENT_RESPONDED_TO_THREAT"/>
    <enumval name="EVENT_REVIVED"/>
    <enumval name="EVENT_SCRIPT_COMMAND"/>
    <enumval name="EVENT_SHOCKING_BROKEN_GLASS"/>
    <enumval name="EVENT_SHOCKING_CAR_ALARM"/>
    <enumval name="EVENT_SHOCKING_CAR_CHASE"/>
    <enumval name="EVENT_SHOCKING_CAR_CRASH"/>
		<enumval name="EVENT_SHOCKING_BICYCLE_CRASH"/>
    <enumval name="EVENT_SHOCKING_CAR_PILE_UP"/>
    <enumval name="EVENT_SHOCKING_CAR_ON_CAR"/>
    <enumval name="EVENT_SHOCKING_DANGEROUS_ANIMAL"/>
    <enumval name="EVENT_SHOCKING_DEAD_BODY"/>
    <enumval name="EVENT_SHOCKING_DRIVING_ON_PAVEMENT"/>
	<enumval name="EVENT_SHOCKING_BICYCLE_ON_PAVEMENT"/>
    <enumval name="EVENT_SHOCKING_ENGINE_REVVED"/>
    <enumval name="EVENT_SHOCKING_EXPLOSION"/>
    <enumval name="EVENT_SHOCKING_FIRE"/>
    <enumval name="EVENT_SHOCKING_GUN_FIGHT"/>
    <enumval name="EVENT_SHOCKING_GUNSHOT_FIRED"/>
    <enumval name="EVENT_SHOCKING_HELICOPTER_OVERHEAD"/>
    <enumval name="EVENT_SHOCKING_PARACHUTER_OVERHEAD"/>
    <enumval name="EVENT_SHOCKING_PED_KNOCKED_INTO_BY_PLAYER"/>
    <enumval name="EVENT_SHOCKING_HORN_SOUNDED"/>
    <enumval name="EVENT_SHOCKING_IN_DANGEROUS_VEHICLE"/>
    <enumval name="EVENT_SHOCKING_INJURED_PED"/>
    <enumval name="EVENT_SHOCKING_MAD_DRIVER"/>
    <enumval name="EVENT_SHOCKING_MAD_DRIVER_EXTREME"/>
	<enumval name="EVENT_SHOCKING_MAD_DRIVER_BICYCLE"/>
    <enumval name="EVENT_SHOCKING_MUGGING"/>
    <enumval name="EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT"/>
    <enumval name="EVENT_SHOCKING_PED_RUN_OVER"/>
    <enumval name="EVENT_SHOCKING_PED_SHOT"/>
    <enumval name="EVENT_SHOCKING_PLANE_FLY_BY"/>
	<enumval name="EVENT_SHOCKING_POTENTIAL_BLAST"/>
    <enumval name="EVENT_SHOCKING_PROPERTY_DAMAGE"/>
    <enumval name="EVENT_SHOCKING_RUNNING_PED"/>
    <enumval name="EVENT_SHOCKING_RUNNING_STAMPEDE"/>
    <enumval name="EVENT_SHOCKING_SEEN_CAR_STOLEN"/>
    <enumval name="EVENT_SHOCKING_SEEN_CONFRONTATION"/>
    <enumval name="EVENT_SHOCKING_SEEN_GANG_FIGHT"/>
    <enumval name="EVENT_SHOCKING_SEEN_INSULT"/>
    <enumval name="EVENT_SHOCKING_SEEN_MELEE_ACTION"/>
    <enumval name="EVENT_SHOCKING_SEEN_NICE_CAR"/>
    <enumval name="EVENT_SHOCKING_SEEN_PED_KILLED"/>
    <enumval name="EVENT_SHOCKING_SEEN_VEHICLE_TOWED"/>
    <enumval name="EVENT_SHOCKING_SEEN_WEAPON_THREAT"/>
    <enumval name="EVENT_SHOCKING_SEEN_WEIRD_PED"/>
    <enumval name="EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING"/>
    <enumval name="EVENT_SHOCKING_SIREN"/>
    <enumval name="EVENT_SHOCKING_STUDIO_BOMB"/>
    <enumval name="EVENT_SHOCKING_VISIBLE_WEAPON"/>
    <enumval name="EVENT_SHOT_FIRED"/>
    <enumval name="EVENT_SHOT_FIRED_BULLET_IMPACT"/>
    <enumval name="EVENT_SHOT_FIRED_WHIZZED_BY"/>
		<enumval name="EVENT_FRIENDLY_AIMED_AT"/>
		<enumval name="EVENT_FRIENDLY_FIRE_NEAR_MISS"/>
    <enumval name="EVENT_SHOUT_BLOCKING_LOS"/>
    <enumval name="EVENT_SHOUT_TARGET_POSITION"/>
    <enumval name="EVENT_STATIC_COUNT_REACHED_MAX"/>
    <enumval name="EVENT_STUCK_IN_AIR"/>
    <enumval name="EVENT_SUSPICIOUS_ACTIVITY"/>
    <enumval name="EVENT_SWITCH_2_NM_TASK"/>
    <enumval name="EVENT_UNIDENTIFIED_PED"/>
    <enumval name="EVENT_VEHICLE_COLLISION"/>
    <enumval name="EVENT_VEHICLE_DAMAGE_WEAPON"/>
    <enumval name="EVENT_VEHICLE_ON_FIRE"/>
    <enumval name="EVENT_WHISTLING_HEARD"/>
    <enumval name="EVENT_DISTURBANCE"/>
    <enumval name="EVENT_ENTITY_DAMAGED"/>
    <enumval name="EVENT_ENTITY_DESTROYED"/>
    <enumval name="EVENT_WRITHE"/>
    <enumval name="EVENT_HURT_TRANSITION"/>
    <enumval name="EVENT_PLAYER_UNABLE_TO_ENTER_VEHICLE"/>
    <enumval name="EVENT_SCENARIO_FORCE_ACTION"/>
    <enumval name="EVENT_STAT_VALUE_CHANGED"/>
		<enumval name="EVENT_PLAYER_DEATH"/>
    <enumval name="EVENT_PED_SEEN_DEAD_PED"/>
    <enumval name="NUM_AI_EVENTTYPE"/>

    <enumval name="EVENT_NETWORK_PLAYER_JOIN_SESSION"/>
    <enumval name="EVENT_NETWORK_PLAYER_LEFT_SESSION"/>
    <enumval name="EVENT_NETWORK_PLAYER_JOIN_SCRIPT"/>
    <enumval name="EVENT_NETWORK_PLAYER_LEFT_SCRIPT"/>
    <enumval name="EVENT_NETWORK_STORE_PLAYER_LEFT"/>
    <enumval name="EVENT_NETWORK_SESSION_START"/>
    <enumval name="EVENT_NETWORK_SESSION_END"/>
    <enumval name="EVENT_NETWORK_START_MATCH"/>
    <enumval name="EVENT_NETWORK_END_MATCH"/>
    <enumval name="EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_STALL"/>
    <enumval name="EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_COMPLAINTS"/>
    <enumval name="EVENT_NETWORK_CONNECTION_TIMEOUT"/>
	<enumval name="EVENT_NETWORK_PED_DROPPED_WEAPON"/>
	<enumval name="EVENT_NETWORK_PLAYER_SPAWN"/>
    <enumval name="EVENT_NETWORK_PLAYER_COLLECTED_PICKUP"/>
    <enumval name="EVENT_NETWORK_PLAYER_COLLECTED_AMBIENT_PICKUP"/>
    <enumval name="EVENT_NETWORK_PLAYER_COLLECTED_PORTABLE_PICKUP"/>
    <enumval name="EVENT_NETWORK_PLAYER_DROPPED_PORTABLE_PICKUP"/>
    <enumval name="EVENT_NETWORK_INVITE_ARRIVED"/>
    <enumval name="EVENT_NETWORK_INVITE_ACCEPTED"/>
    <enumval name="EVENT_NETWORK_INVITE_CONFIRMED"/>
    <enumval name="EVENT_NETWORK_INVITE_REJECTED"/>
    <enumval name="EVENT_NETWORK_SUMMON"/>
    <enumval name="EVENT_NETWORK_SCRIPT_EVENT"/>
    <enumval name="EVENT_NETWORK_PLAYER_SIGNED_OFFLINE"/>
    <enumval name="EVENT_NETWORK_SIGN_IN_STATE_CHANGED"/>
    <enumval name="EVENT_NETWORK_SIGN_IN_CHANGE_ACTIONED"/>
    <enumval name="EVENT_NETWORK_NETWORK_ROS_CHANGED"/>
    <enumval name="EVENT_NETWORK_NETWORK_BAIL"/>
    <enumval name="EVENT_NETWORK_HOST_MIGRATION"/>
    <enumval name="EVENT_NETWORK_FIND_SESSION"/>
    <enumval name="EVENT_NETWORK_HOST_SESSION"/>
    <enumval name="EVENT_NETWORK_JOIN_SESSION"/>
    <enumval name="EVENT_NETWORK_JOIN_SESSION_RESPONSE"/>
    <enumval name="EVENT_NETWORK_CHEAT_TRIGGERED"/>
    <enumval name="EVENT_NETWORK_DAMAGE_ENTITY"/>
    <enumval name="EVENT_NETWORK_PLAYER_ARREST"/>
    <enumval name="EVENT_NETWORK_TIMED_EXPLOSION"/>
    <enumval name="EVENT_NETWORK_PRIMARY_CLAN_CHANGED"/>
    <enumval name="EVENT_NETWORK_CLAN_JOINED"/>
    <enumval name="EVENT_NETWORK_CLAN_LEFT"/>
    <enumval name="EVENT_NETWORK_CLAN_INVITE_RECEIVED"/>
    <enumval name="EVENT_VOICE_SESSION_STARTED"/>
    <enumval name="EVENT_VOICE_SESSION_ENDED"/>
    <enumval name="EVENT_VOICE_CONNECTION_REQUESTED"/>
    <enumval name="EVENT_VOICE_CONNECTION_RESPONSE"/>
    <enumval name="EVENT_VOICE_CONNECTION_TERMINATED"/>
    <enumval name="EVENT_TEXT_MESSAGE_RECEIVED"/>
    <enumval name="EVENT_CLOUD_FILE_RESPONSE"/>
    <enumval name="EVENT_NETWORK_PICKUP_RESPAWNED"/>
    <enumval name="EVENT_NETWORK_PRESENCE_STAT_UPDATE"/>
    <enumval name="EVENT_NETWORK_PED_LEFT_BEHIND"/>
	  <enumval name="EVENT_NETWORK_INBOX_MSGS_RCVD"/>
    <enumval name="EVENT_NETWORK_ATTEMPT_HOST_MIGRATION"/>
	  <enumval name="EVENT_NETWORK_INCREMENT_STAT"/>
    <enumval name="EVENT_NETWORK_SESSION_EVENT"/>
    <enumval name="EVENT_NETWORK_TRANSITION_STARTED"/>
    <enumval name="EVENT_NETWORK_TRANSITION_EVENT"/>
    <enumval name="EVENT_NETWORK_TRANSITION_MEMBER_JOINED"/>
    <enumval name="EVENT_NETWORK_TRANSITION_MEMBER_LEFT"/>
    <enumval name="EVENT_NETWORK_TRANSITION_PARAMETER_CHANGED"/>
	  <enumval name="EVENT_NETWORK_CLAN_KICKED"/>
    <enumval name="EVENT_NETWORK_TRANSITION_STRING_CHANGED"/>
    <enumval name="EVENT_NETWORK_TRANSITION_GAMER_INSTRUCTION"/>
    <enumval name="EVENT_NETWORK_PRESENCE_INVITE"/>
    <enumval name="EVENT_NETWORK_PRESENCE_INVITE_REMOVED"/>
    <enumval name="EVENT_NETWORK_PRESENCE_INVITE_REPLY"/>
    <enumval name="EVENT_NETWORK_CASH_TRANSACTION_LOG"/>
    <enumval name="EVENT_NETWORK_CLAN_RANK_CHANGE"/>
    <enumval name="EVENT_NETWORK_VEHICLE_UNDRIVABLE"/>
    <enumval name="EVENT_NETWORK_PRESENCE_TRIGGER"/>
    <enumval name="EVENT_NETWORK_PRESENCE_EMAIL"/>
    <enumval name="EVENT_NETWORK_FOLLOW_INVITE_RECEIVED"/>
    <enumval name="EVENT_NETWORK_ADMIN_INVITED"/>
    <enumval name="EVENT_NETWORK_SPECTATE_LOCAL"/>
    <enumval name="EVENT_NETWORK_CLOUD_EVENT"/>
    <enumval name="EVENT_NETWORK_SHOP_TRANSACTION"/>
    <enumval name="EVENT_NETWORK_PERMISSION_CHECK_RESULT"/>
    <enumval name="EVENT_NETWORK_APP_LAUNCHED"/>
    <enumval name="EVENT_NETWORK_ONLINE_PERMISSIONS_UPDATED"/>
    <enumval name="EVENT_NETWORK_SYSTEM_SERVICE_EVENT"/>
    <enumval name="EVENT_NETWORK_REQUEST_DELAY"/>
    <enumval name="EVENT_NETWORK_SOCIAL_CLUB_ACCOUNT_LINKED"/>
    <enumval name="EVENT_NETWORK_SCADMIN_PLAYER_UPDATED"/>
    <enumval name="EVENT_NETWORK_SCADMIN_RECEIVED_CASH"/>
    <enumval name="EVENT_NETWORK_CLAN_INVITE_REQUEST_RECEIVED"/>
    <enumval name="EVENT_NETWORK_PRESENCE_MARKETING_EMAIL"/>
    <enumval name="EVENT_NETWORK_STUNT_PERFORMED"/>
    <enumval name="EVENT_NETWORK_FIRED_DUMMY_PROJECTILE"/>
    <enumval name="EVENT_NETWORK_PLAYER_ENTERED_VEHICLE"/>
    <enumval name="EVENT_NETWORK_PLAYER_ACTIVATED_SPECIAL_ABILITY"/>
    <enumval name="EVENT_NETWORK_PLAYER_DEACTIVATED_SPECIAL_ABILITY"/>
    <enumval name="EVENT_NETWORK_PLAYER_SPECIAL_ABILITY_FAILED_ACTIVATION"/>
    <enumval name="EVENT_NETWORK_FIRED_VEHICLE_PROJECTILE"/>
    <enumval name="EVENT_NETWORK_SC_MEMBERSHIP_STATUS"/>
    <enumval name="EVENT_NETWORK_SC_BENEFITS_STATUS"/>
    <enumval name="NUM_NETWORK_EVENTTYPE"/>

    <enumval name="EVENT_ERRORS_UNKNOWN_ERROR"/>
    <enumval name="EVENT_ERRORS_ARRAY_OVERFLOW"/>
    <enumval name="EVENT_ERRORS_INSTRUCTION_LIMIT"/>
    <enumval name="EVENT_ERRORS_STACK_OVERFLOW"/>
    <enumval name="NUM_NETWORK_ERRORSTYPE"/>

    <enumval name="NUM_EVENTTYPE"/>

    <enumval name="EVENT_INVALID" value="-1"/>
  </enumdef>


  <structdef type="CEventDataDecisionMaker">
    <string name="m_Name" type="atHashString"/>
    <string name="DecisionMakerParentRef" type="atHashString"/>

    <array name="EventResponse" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>



  <structdef type="CEventDataResponseTask">
    <string name="m_Name" type="atHashString"/>
  </structdef>

  <structdef type="CEventDataResponsePoliceTaskWanted" base="CEventDataResponseTask">
  </structdef>

	<structdef type="CEventDataResponseSwatTaskWanted" base="CEventDataResponseTask">
	</structdef>
	
  <structdef type="CEventDataResponseTaskCombat" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskThreat" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskCower" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskHandsUp" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskGunAimedAt" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskFlee" base="CEventDataResponseTask">
    <u8    name="Flee"/>
  </structdef>

  <structdef type="CEventDataResponseTaskTurnToFace" base="CEventDataResponseTask">
    <u8    name="Face"/>
  </structdef>

  <structdef type="CEventDataResponseTaskCrouch" base="CEventDataResponseTask">
    <float  name="TimeToCrouch"/>
  </structdef>

  <structdef type="CEventDataResponseTaskLeaveCarAndFlee" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskWalkRoundFire" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskScenarioFlee" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskFlyAway" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskWalkRoundEntity" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskHeadTrack" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventGoto" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventHurryAway" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventWatch" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingPoliceInvestigate" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskEvasiveStep" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskEscapeBlast" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskAgitated" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskExplosion" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskDuckAndCover" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventReactToAircraft" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventReact" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventBackAway" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskShockingEventStopAndStare" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskSharkAttack" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskExhaustedFlee" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskGrowlAndFlee" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseTaskWalkAway" base="CEventDataResponseTask">
  </structdef>

  <structdef type="CEventDataResponseAggressiveRubberneck" base="CEventDataResponseTask">
  </structdef>

	<structdef type="CEventDataResponseTaskShockingNiceCar"  base="CEventDataResponseTask">
	</structdef>

	<structdef type="CEventDataResponseFriendlyNearMiss" base="CEventDataResponseTask">
	</structdef>

	<structdef type="CEventDataResponseFriendlyAimedAt" base="CEventDataResponseTask">
	</structdef>

	<structdef type="CEventDataResponseDeferToScenarioPointFlags" base="CEventDataResponseTask">
	</structdef>

	<structdef type="CEventDataResponseTaskShockingEventThreatResponse" base="CEventDataResponseTask">
	</structdef>

	<structdef type="CEventDataResponsePlayerDeath" base="CEventDataResponseTask">
	</structdef>

</ParserSchema>
