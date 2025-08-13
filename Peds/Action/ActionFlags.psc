<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

  <enumdef type="CActionFlags::PlayerAiFilterType" generate="bitset">
    <enumval name="PAFT_PLAYER_SP"/>
    <enumval name="PAFT_PLAYER_MP"/>
    <enumval name="PAFT_AI"/>
	<enumval name="PAFT_NOT_ANIMAL"/>
    <enumval name="PAFT_NOT_FISH"/>
    <enumval name="PAFT_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::TargetEntityType" generate="bitset">
    <enumval name="TET_BIPED"/>
    <enumval name="TET_QUADRUPED"/>
	<enumval name="TET_SURFACE"/>
    <enumval name="TET_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::RequiredWeaponType" generate="bitset">
    <enumval name="RWT_UNARMED"/>
    <enumval name="RWT_ONE_HANDED_GUN"/>
    <enumval name="RWT_TWO_HANDED_GUN"/>
    <enumval name="RWT_PISTOL"/>
    <enumval name="RWT_KNIFE"/>
    <enumval name="RWT_ONE_HANDED_CLUB"/>
    <enumval name="RWT_TWO_HANDED_CLUB"/>
	<enumval name="RWT_THROWN"/>
    <enumval name="RWT_THROWN_SPECIAL"/>
    <enumval name="RWT_HATCHET"/>
    <enumval name="RWT_ONE_HANDED_CLUB_NO_HATCHET"/>
    <enumval name="RWT_MELEE_FIST"/>
    <enumval name="RWT_MACHETE"/>
    <enumval name="RWT_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::ActionType" generate="bitset">
    <enumval name="AT_HIT_REACTION"/>
    <enumval name="AT_FROM_ON_FOOT"/>
    <enumval name="AT_MELEE"/>
    <enumval name="AT_BLOCK"/>
	<enumval name="AT_COUNTER"/>
    <enumval name="AT_TAUNT"/>
	<enumval name="AT_INTRO"/>
	<enumval name="AT_CONDITION"/>
    <enumval name="AT_RECOIL"/>
	<enumval name="AT_DAZED_HIT_REACTION"/>
    <enumval name="AT_SWIMMING"/>   
    <enumval name="AT_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::ImpulseOp">
    <enumval name="IO_OR_IMPULSE_1"/>
    <enumval name="IO_AND_IMPULSE_1"/>
    <enumval name="IO_XOR_IMPULSE_1"/>
  </enumdef>

  <enumdef type="CActionFlags::Impulse">
    <enumval name="I_NONE"/>
    <enumval name="I_ATTACK_LIGHT"/>
    <enumval name="I_ATTACK_HEAVY"/>
    <enumval name="I_FIRE"/>
    <enumval name="I_ATTACK_ALTERNATE"/>
    <enumval name="I_BLOCK"/>
    <enumval name="I_SPECIAL_ABILITY"/>
    <enumval name="I_SPECIAL_ABILITY_SECONDARY"/>
    <enumval name="I_ANALOG_LEFT_RIGHT"/>
    <enumval name="I_ANALOG_UP_DOWN"/>
    <enumval name="I_ANALOG_FROM_CENTER"/>
  </enumdef>

  <enumdef type="CActionFlags::ImpulseState">
    <enumval name="IS_NONE"/>
    <enumval name="IS_DOWN"/>
    <enumval name="IS_HELD_DOWN"/>
    <enumval name="IS_HELD_UP"/>
    <enumval name="IS_PRESSED_AND_HELD"/>
    <enumval name="IS_RELEASED_AND_HELD"/>
    <enumval name="IS_PRESSED"/>
    <enumval name="IS_PRESSED_NO_HISTORY"/>
    <enumval name="IS_RELEASED"/>
    <enumval name="IS_COMBO_1"/>
    <enumval name="IS_COMBO_2"/>
    <enumval name="IS_COMBO_3"/>
    <enumval name="IS_COMBO_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::TargetType">
    <enumval name="TT_ENTITY_ROOT"/>
    <enumval name="TT_ENTITY_ROOT_PREDICTED"/>
    <enumval name="TT_ENTITY_CLOSEST_BOX_POINT"/>
    <enumval name="TT_PED_ROOT"/>
    <enumval name="TT_PED_NECK"/>
    <enumval name="TT_PED_CHEST"/>
    <enumval name="TT_PED_KNEE_LEFT"/>
    <enumval name="TT_PED_KNEE_RIGHT"/>
    <enumval name="TT_PED_CLOSEST_BONE"/>
    <enumval name="TT_SURFACE_PROBE"/>
    <enumval name="TT_NONE"/>
  </enumdef>

  <enumdef type="CActionFlags::CameraTarget">
    <enumval name="CT_PED_HEAD"/>
    <enumval name="CT_PED_ROOT"/>
    <enumval name="CT_PED_NECK"/>
    <enumval name="CT_PED_CHEST"/>
    <enumval name="CT_PED_KNEE_LEFT"/>
    <enumval name="CT_PED_KNEE_RIGHT"/>
  </enumdef>

  <enumdef type="CActionFlags::MovementSpeed" generate="bitset">
    <enumval name="MS_STRAFING"/>
    <enumval name="MS_STILL"/>
    <enumval name="MS_WALKING"/>
    <enumval name="MS_RUNNING"/>
    <enumval name="MS_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::DesiredDirection" generate="bitset">
    <enumval name="DD_FORWARD"/>
    <enumval name="DD_BACKWARD_LEFT"/>
    <enumval name="DD_BACKWARD_RIGHT"/>
    <enumval name="DD_LEFT"/>
    <enumval name="DD_RIGHT"/>
    <enumval name="DD_ANY"/>
  </enumdef>

  <enumdef type="CActionFlags::HomingAttrs" generate="bitset">
    <enumval name="HA_CHECK_TARGET_DISTANCE"/>
    <enumval name="HA_CHECK_TARGET_HEADING"/>
    <enumval name="HA_USE_DISPLACEMENT_MAGNITUDE"/>
    <enumval name="HA_USE_CACHED_POSITION"/>
    <enumval name="HA_MAINTAIN_IDEAL_DISTANCE"/>
    <enumval name="HA_USE_ACTIVATION_DISTANCE"/>
	<enumval name="HA_USE_LOW_ROOT_Z_OFFSET"/>
	<enumval name="HA_IGNORE_SPEED_MATCHING_CHECK"/>
  </enumdef>

  <enumdef type="CActionFlags::RelativeRangeAttrs" generate="bitset">
    <enumval name="RRA_CHECK_HEIGHT_RANGE"/>
	<enumval name="RRA_CHECK_GROUND_RANGE"/>
    <enumval name="RRA_CHECK_DISTANCE_RANGE"/>
    <enumval name="RRA_CHECK_ANGULAR_RANGE"/>
	<enumval name="RRA_ALLOW_PED_GETUP_GROUND_OVERRIDE"/>
	<enumval name="RRA_ALLOW_PED_DEAD_GROUND_OVERRIDE"/>
  </enumdef>

  <enumdef type="CActionFlags::ResultAttrs" generate="bitset">
	<enumval name="RA_IS_INTRO"/>
	<enumval name="RA_IS_TAUNT"/>
	<enumval name="RA_PLAYER_IS_UPPERBODY_ONLY"/>
    <enumval name="RA_AI_IS_UPPERBODY_ONLY"/>
    <enumval name="RA_IS_STEALTH_KILL"/>
    <enumval name="RA_IS_BLOCK"/>
    <enumval name="RA_IS_KNOCKOUT"/>
    <enumval name="RA_IS_TAKEDOWN"/>
    <enumval name="RA_IS_HIT_REACTION"/>
	<enumval name="RA_IS_DAZED_HIT_REACTION"/>
    <enumval name="RA_IS_STANDARD_ATTACK"/>
    <enumval name="RA_IS_COUNTER_ATTACK"/>
    <enumval name="RA_IS_RECOIL"/>
    <enumval name="RA_RESET_COMBO"/>
    <enumval name="RA_ALLOW_AIM_INTERRUPT"/>
    <enumval name="RA_FORCE_DAMAGE"/>
    <enumval name="RA_ALLOW_ADDITIVE_ANIMS"/>
    <enumval name="RA_IS_LETHAL"/>
    <enumval name="RA_USE_LEG_IK"/>
    <enumval name="RA_VALID_FOR_RECOIL"/>
    <enumval name="RA_IS_NON_MELEE_HIT_REACTION"/>
    <enumval name="RA_ALLOW_STRAFING"/>
    <enumval name="RA_RESET_MIN_TIME_IN_MELEE"/>
    <enumval name="RA_ALLOW_NO_TARGET_INTERRUPT"/>
	<enumval name="RA_QUIT_TASK_AFTER_ACTION"/>
	<enumval name="RA_ACTIVATE_RAGDOLL_ON_COLLISION"/>
	<enumval name="RA_ALLOW_INITIAL_ACTION_COMBO"/>
	<enumval name="RA_ALLOW_MELEE_TARGET_EVALUATION"/>
	<enumval name="RA_PROCESS_POST_HIT_RESULTS_ON_CONTACT_FRAME"/>
	<enumval name="RA_REQUIRES_AMMO"/>
	<enumval name="RA_GIVE_THREAT_RESPONSE_AFTER_ACTION"/>
	<enumval name="RA_ALLOW_DAZED_REACTION"/>
	<enumval name="RA_IGNORE_HIT_DOT_THRESHOLD"/>
	<enumval name="RA_INTERRUPT_WHEN_OUT_OF_WATER"/>
	<enumval name="RA_ALLOW_SCRIPT_TASK_ABORT"/>
    <enumval name="RA_TAG_SYNC_BLEND_OUT"/>
    <enumval name="RA_UPDATE_MOVEBLENDRATIO"/>
    <enumval name="RA_ENDS_IN_IDLE_POSE"/>
    <enumval name="RA_DISABLE_PED_TO_PED_RAGDOLL"/>
    <enumval name="RA_ALLOW_NO_TARGET_BRANCHES"/>
    <enumval name="RA_USE_KINEMATIC_PHYSICS"/>
    <enumval name="RA_ENDS_IN_AIMING_POSE"/>
    <enumval name="RA_USE_CACHED_DESIRED_HEADING"/>
    <enumval name="RA_ADD_ORIENT_TO_SPINE_Z_OFFSET"/>
    <enumval name="RA_ALLOW_PLAYER_EARLY_OUT"/>
    <enumval name="RA_IS_SHOULDER_BARGE"/>
  </enumdef>

  <enumdef type="CActionFlags::DefinitionAttrs" generate="bitset">
    <enumval name="DA_IS_ENABLED"/>
    <enumval name="DA_PLAYER_ENTRY_ACTION"/>
    <enumval name="DA_AI_ENTRY_ACTION"/>
    <enumval name="DA_REQUIRE_TARGET"/>
    <enumval name="DA_REQUIRE_NO_TARGET"/>
    <enumval name="DA_REQUIRE_TARGET_LOCK_ON"/>
    <enumval name="DA_REQUIRE_NO_TARGET_LOCK_ON"/>
    <enumval name="DA_PREFER_RAGDOLL_RESULT"/>
    <enumval name="DA_PREFER_BODY_IK_RESULT"/>
    <enumval name="DA_ALLOW_IN_WATER"/>
	<enumval name="DA_DISABLE_IN_FIRST_PERSON"/>
	<enumval name="DA_DISABLE_IF_TARGET_IN_FIRST_PERSON"/>
  </enumdef>

  <enumdef type="CActionFlags::DamageReactionAttrs" generate="bitset">
    <enumval name="DRA_FORCE_IMMEDIATE_REACTION"/>
    <enumval name="DRA_DROP_WEAPON"/>
    <enumval name="DRA_DISARM_OPPONENT_WEAPON"/>
    <enumval name="DRA_KNOCK_OFF_PROPS"/>
    <enumval name="DRA_STOP_DISTANCE_HOMING"/>
    <enumval name="DRA_STOP_TARGET_DISTANCE_HOMING"/>
    <enumval name="DRA_ADD_SMALL_ABILITY_CHARGE"/>
    <enumval name="DRA_DISABLE_INJURED_GROUND_BEHAVIOR"/>
    <enumval name="DRA_IGNORE_ARMOR"/>
    <enumval name="DRA_IGNORE_STAT_MODIFIERS"/>
    <enumval name="DRA_USE_DEFAULT_UNARMED_WEAPON"/>
    <enumval name="DRA_FATAL_HEAD_SHOT"/>
    <enumval name="DRA_FATAL_SHOT"/>
    <enumval name="DRA_MOVE_LOWER_ARM_AND_HAND_HITS_TO_CLAVICLE"/>
    <enumval name="DRA_MOVE_UPPER_ARM_AND_CLAVICLE_HITS_TO_HEAD"/>
    <enumval name="DRA_INVOKE_FACIAL_PAIN_ANIMATION"/>
    <enumval name="DRA_PRESERVE_STEALTH_MODE"/>
    <enumval name="DRA_DONT_DAMAGE_PEDS"/>
  </enumdef>

</ParserSchema>
