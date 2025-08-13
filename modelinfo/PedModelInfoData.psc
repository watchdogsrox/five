<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <enumdef type="SpecialAbilityType">
    <enumval name="SAT_NONE" value="-1"/>
    <enumval name="SAT_CAR_SLOWDOWN" value="0"/>
    <enumval name="SAT_RAGE" value="1"/>
    <enumval name="SAT_BULLET_TIME" value="2"/>
    <enumval name="SAT_SNAPSHOT" value="3"/>
    <enumval name="SAT_INSULT" value="4"/>
    <enumval name="SAT_ENRAGED" value="5"/>
    <enumval name="SAT_GHOST" value="6"/>
    <enumval name="SAT_SPRINT_SPEED_BOOST" value="7"/>
  </enumdef>
  
  <enumdef type="ThermalBehaviour">
    <enumval name="TB_DEAD" value="0"/>
    <enumval name="TB_COLD" value="1"/>
    <enumval name="TB_WARM" value="2"/>
    <enumval name="TB_HOT" value="3"/>
  </enumdef>

  <enumdef type="eSuperlodType">
    <enumval name="SLOD_HUMAN"/>
    <enumval name="SLOD_SMALL_QUADPED"/>
    <enumval name="SLOD_LARGE_QUADPED"/>
    <enumval name="SLOD_NULL"/>
    <enumval name="SLOD_KEEP_LOWEST"/>
  </enumdef>

  <enumdef type="Affluence">
    <enumval name="AFF_POOR" value ="0"/>
    <enumval name="AFF_AVERAGE" value="1"/>
    <enumval name="AFF_RICH" value="2"/>
  </enumdef>

  <enumdef type="TechSavvy">
    <enumval name="TS_LOW" value="0"/>
    <enumval name="TS_HIGH" value="1"/>
  </enumdef>

  <enumdef type="DefaultSpawnPreference">
    <enumval name="DSP_AERIAL"/>
    <enumval name="DSP_AQUATIC"/>
    <enumval name="DSP_GROUND_WILDLIFE"/>
    <enumval name="DSP_NORMAL"/>
  </enumdef>

  <enumdef type="AgitationReactionType">
    <enumval name="ART_Nothing"/>
    <enumval name="ART_Ignore"/>
    <enumval name="ART_Loiter"/>
    <enumval name="ART_Intimidate"/>
    <enumval name="ART_Combat"/>
    <enumval name="ART_TerritoryIntrusion"/>
  </enumdef>

  <enumdef type="eScenarioPopStreamingSlot">
    <enumval name="SCENARIO_POP_STREAMING_NORMAL"/>
    <enumval name="SCENARIO_POP_STREAMING_SMALL"/>
  </enumdef>

  <enumdef type="eExternallyDrivenDOFs">
    <enumval name="EMPTY" value ="0"/>
    <enumval name="HIGH_HEELS" value ="1"/>
		<enumval name="COLLAR" value="2"/>
  </enumdef>

  <enumdef type="ePedRadioGenre">
    <enumval name="RADIO_GENRE_OFF" value ="0"/>
    <enumval name="RADIO_GENRE_MODERN_ROCK"/>
    <enumval name="RADIO_GENRE_CLASSIC_ROCK"/>
    <enumval name="RADIO_GENRE_POP"/>
    <enumval name="RADIO_GENRE_MODERN_HIPHOP"/>
    <enumval name="RADIO_GENRE_CLASSIC_HIPHOP"/>
    <enumval name="RADIO_GENRE_PUNK"/>
    <enumval name="RADIO_GENRE_LEFT_WING_TALK"/>
    <enumval name="RADIO_GENRE_RIGHT_WING_TALK"/>
    <enumval name="RADIO_GENRE_COUNTRY"/>
    <enumval name="RADIO_GENRE_DANCE"/>
    <enumval name="RADIO_GENRE_MEXICAN"/>
    <enumval name="RADIO_GENRE_REGGAE"/>
    <enumval name="RADIO_GENRE_JAZZ"/>
    <enumval name="RADIO_GENRE_MOTOWN"/>
    <enumval name="RADIO_GENRE_SURF"/>
    <enumval name="RADIO_GENRE_UNSPECIFIED"/>
  </enumdef>

  <structdef type="CPedModelInfo::InitData">
    <string name="m_Name" type="ConstString" ui_key="true" description="Ped model name"/>
    <string name="m_PropsName" type="ConstString" init="null" description="Ped prop dictionary name"/>
    <string name="m_ClipDictionaryName" type="atHashString" init="move_m@generic"/>
    <string name="m_BlendShapeFileName" type="atHashString" init="null"/>
    <string name="m_ExpressionSetName" type="atHashString"/>
    <string name="m_ExpressionDictionaryName" type="atHashString" init="default"/>
    <string name="m_ExpressionName" type="atHashString" init="male_std"/>
    <string name="m_Pedtype" type="atHashString" init="CIVMALE"/>
    <string name="m_MovementClipSet" type="atHashString" init="move_m@generic"/>
    <array name="m_MovementClipSets" type="atArray">
      <string type="atHashString"/>
    </array>
    <string name="m_StrafeClipSet" type="atHashString" init="move_ped_strafing"/>
    <string name="m_MovementToStrafeClipSet" type="atHashString" init="move_ped_to_strafe"/>
    <string name="m_InjuredStrafeClipSet" type="atHashString" init="move_strafe_injured"/>
    <string name="m_FullBodyDamageClipSet" type="atHashString" init="dam_ko"/>
    <string name="m_AdditiveDamageClipSet" type="atHashString" init="dam_ad"/>
    <string name="m_DefaultGestureClipSet" type="atHashString" init="ANIM_GROUP_GESTURE_M_GENERIC"/>
    <string name="m_FacialClipsetGroupName" type="atHashString"/>
    <string name="m_DefaultVisemeClipSet" type="atHashString" init="ANIM_GROUP_VISEMES_M_LO"/>
    <string name="m_SidestepClipSet" type="atHashString" init="CLIP_SET_ID_INVALID"/>
    <string name="m_PoseMatcherName" type="atHashString" init="Male"/>
    <string name="m_PoseMatcherProneName" type="atHashString" init="Male_prone"/>
    <string name="m_GetupSetHash" type="atHashString" init="NMBS_SLOW_GETUPS"/>
    <string name="m_CreatureMetadataName" type="atHashString" init="null"/>
    <string name="m_DecisionMakerName" type="atHashString" init="DEFAULT"/>
    <string name="m_MotionTaskDataSetName" type="atHashString" init="STANDARD_PED"/>
    <string name="m_DefaultTaskDataSetName" type="atHashString" init="STANDARD_PED"/>
    <string name="m_PedCapsuleName" type="atHashString" init="STANDARD_PED"/>
    <string name="m_PedLayoutName" type="atHashString"/>
    <string name="m_PedComponentSetName" type="atHashString"/>
    <string name="m_PedComponentClothName" type="atHashString"/>
    <string name="m_PedIKSettingsName" type="atHashString"/>
    <string name="m_TaskDataName" type="atHashString"/>
    <bool name="m_IsStreamedGfx"/>
    <bool name="m_AmbulanceShouldRespondTo" init="true"/>
    <bool name="m_CanRideBikeWithNoHelmet" init="false"/>
    <bool name="m_CanSpawnInCar" init="true"/>
    <bool name="m_IsHeadBlendPed" init="false"/>
		<bool name="m_bOnlyBulkyItemVariations" init="false"/>
    <string name="m_RelationshipGroup" type="atHashString" init="CIVMALE"/>
    <string name="m_NavCapabilitiesName" type="atHashString" init="STANDARD_PED"/>
    <string name="m_PerceptionInfo" type="atHashString" init="DEFAULT_PERCEPTION" />
    <string name="m_DefaultBrawlingStyle" type="atHashString" init="MILTARY"/>
    <string name="m_DefaultUnarmedWeapon" type="atHashString" init="WEAPON_UNARMED"/>
    <string name="m_Personality" type="atHashString" init="Default"/>
    <string name="m_CombatInfo" type="atHashString" init="DEFAULT"/>
    <string name="m_VfxInfoName" type="atHashString" init="VFXPEDINFO_HUMAN_GENERIC"/>
    <string name="m_AmbientClipsForFlee" type="atHashString" init="FLEE"/>

    <enum name="m_Radio1" type="ePedRadioGenre" init="RADIO_GENRE_OFF"/>
    <enum name="m_Radio2" type="ePedRadioGenre" init="RADIO_GENRE_OFF"/>
    
    <float name="m_FUpOffset" init ="0.0f" min="-1.0f" max="1.0f"/>
    <float name="m_RUpOffset" init ="0.0f" min="-1.0f" max="1.0f"/>
    <float name="m_FFrontOffset" init ="0.0f" min="-1.0f" max="1.0f"/>
    <float name="m_RFrontOffset" init ="0.147f" min="-1.0f" max="1.0f"/>

    <float name="m_MinActivationImpulse"/>
    <float name="m_Stubble"/>
    <float name="m_HDDist"/>
    <float name="m_TargetingThreatModifier" init ="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_KilledPerceptionRangeModifer" init="-1.0f" min="0.0f" max="50.0f"/>

    <bitset name="m_Sexiness" type="fixed" numBits="32" values="eSexinessFlags"/>
    <u8 name="m_Age" init="0"/>
    <u8 name="m_MaxPassengersInCar" init="0"/>
    <bitset name="m_ExternallyDrivenDOFs" type="fixed8" numBits="5" values="eExternallyDrivenDOFs"/>

    <string name="m_PedVoiceGroup" type="atHashString"/>
    <string name="m_AnimalAudioObject" type="atHashString"/>

    <enum name="m_AbilityType" type="SpecialAbilityType" init="SAT_NONE"/>
    <enum name="m_ThermalBehaviour" type="ThermalBehaviour" init="TB_WARM"/>
    <enum name="m_SuperlodType" type="eSuperlodType" init="SLOD_HUMAN" description="Superlod model used in the distance"/>
    <enum name="m_ScenarioPopStreamingSlot" type="eScenarioPopStreamingSlot" init="SCENARIO_POP_STREAMING_NORMAL" description="Which kind of scenario streaming slot this ped takes."/>
    <enum name="m_DefaultSpawningPreference" type="DefaultSpawnPreference" init="DSP_NORMAL" description="If not explicility set by the popcycle, how this ped prefers to ambiently spawn."/>
    <float name="m_DefaultRemoveRangeMultiplier" init="1.0f" min="0.0f" max="10.0f" description="Cull range multiplier set for ambiently spawned peds."/>
    <bool name="m_AllowCloseSpawning" init="false"/>
  </structdef>

  <structdef type="CPedModelInfo::InitDataList">
    <string name="m_residentTxd" type="atString"/>
    <array name="m_residentAnims" type="atArray">
      <string type="atString"/>
    </array>
    <array name="m_InitDatas" type="atArray" description="Array of ped descriptors">
      <struct type="CPedModelInfo::InitData" description="Ped descriptor"/>
    </array>
    <array name="m_txdRelationships" type="atArray">
      <struct type="CTxdRelationship"/>
    </array>
    <array name="m_multiTxdRelationships" type="atArray">
      <struct type="CMultiTxdRelationship"/>
    </array>
  </structdef>


  <enumdef type="ePedVehicleTypes">
    <enumval name="PED_DRIVES_POOR_CAR" value="0"/>
    <enumval name="PED_DRIVES_AVERAGE_CAR"/>
    <enumval name="PED_DRIVES_RICH_CAR"/>
    <enumval name="PED_DRIVES_BIG_CAR"/>
    <enumval name="PED_DRIVES_MOTORCYCLE"/>
    <enumval name="PED_DRIVES_BOAT"/>
  </enumdef>

  <enumdef type="eBraveryFlags">
    <enumval name="BF_INTERVENE_ON_MELEE_ACTION" value="0"/>
    <enumval name="BF_DONT_RUN_ON_MELEE_ATTACK"/>
    <enumval name="BF_WATCH_ON_CAR_STOLEN"/>
    <enumval name="BF_INTIMIDATE_PLAYER"/>
    <enumval name="BF_GET_PISSED_WHEN_HIT_BY_CAR"/>
    <enumval name="BF_DONT_SCREAM_ON_FLEE"/>
    <enumval name="BF_DONT_SAY_PANIC_ON_FLEE"/>
    <enumval name="BF_REACT_ON_COMBAT"/>
    <enumval name="BF_PLAY_CAR_HORN"/>
    <enumval name="BF_ARGUMENTATIVE"/>
    <enumval name="BF_CONFRONTATIONAL"/>
    <enumval name="BF_LIMIT_COMBATANTS"/>
    <enumval name="BF_PURSUE_WHEN_HIT_BY_CAR"/>
    <enumval name="BF_COWARDLY_FOR_SHOCKING_EVENTS"/>
    <enumval name="BF_BOOST_BRAVERY_IN_GROUP"/>
    <enumval name="BF_CAN_ACCELERATE_IN_CAR"/>
    <enumval name="BF_CAN_GET_OUT_WHEN_HIT_BY_CAR"/>
    <enumval name="BF_AGGRESSIVE_AFTER_RUNNING_PED_OVER"/>
    <enumval name="BF_CAN_FLEE_WHEN_HIT_BY_CAR"/>
    <enumval name="BF_ALLOW_CONFRONT_FOR_TERRITORY_REACTIONS"/>
    <enumval name="BF_DONT_FORCE_FLEE_COMBAT"/>
  </enumdef>

  <enumdef type="eSexinessFlags">
    <enumval name="SF_JEER_AT_HOT_PED" value="0"/>
    <enumval name="SF_JEER_SCENARIO_ANIM"/>
    <enumval name="SF_HOT_PERSON"/>
  </enumdef>

  <enumdef type="eAgilityFlags">
    <enumval name="AF_CAN_DIVE" value="0"/>
    <enumval name="AF_AVOID_IMMINENT_DANGER"/>
    <enumval name="AF_RAGDOLL_BRACE_STRONG"/>
    <enumval name="AF_RAGDOLL_ON_FIRE_STRONG"/>
    <enumval name="AF_RAGDOLL_HIGH_FALL_STRONG"/>
    <enumval name="AF_RECOVER_BALANCE"/>
    <enumval name="AF_GET_UP_FAST"/>
    <enumval name="AF_BALANCE_STRONG"/>
    <enumval name="AF_STRONG_WITH_HEAVY_WEAPON"/>
    <enumval name="AF_DONT_FLINCH_ON_EXPLOSION"/>
    <enumval name="AF_DONT_FLINCH_ON_MELEE"/>
  </enumdef>

  <enumdef type="eCriminalityFlags">
    <enumval name="CF_JACKING" value="0"/>
    <enumval name="CF_ALLOWED_COP_PURSUIT"/>
  </enumdef>

  <const name="CPedModelInfo::PersonalityMovementModes::MM_Max" value="2"/>
  
  <structdef type="CPedModelInfo::PersonalityMovementModes">
    <string name="m_Name" type="atHashString" ui_key="true" description="Name"/>
    <array name="m_MovementModes" type="member" size="CPedModelInfo::PersonalityMovementModes::MM_Max">
      <array type="atArray">
        <struct type="CPedModelInfo::PersonalityMovementModes::MovementMode"/>
      </array>
    </array>
    <float name="m_LastBattleEventHighEnergyStartTime" init="5.0f" min="0.0f" max="500.0f" description="Time to start high energy mode after battle event"/>
    <float name="m_LastBattleEventHighEnergyEndTime" init="30.0f" min="0.0f" max="500.0f" description="Time to end high energy mode after battle event"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityMovementModes::MovementMode">
    <array name="m_Weapons" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_ClipSets" type="atArray">
      <struct type="CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets"/>
    </array>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets">
    <string name="m_MovementClipSetId" type="atHashString"/>
    <string name="m_WeaponClipSetId" type="atHashString"/>
    <string name="m_WeaponClipFilterId" type="atHashString"/>
    <bool name="m_UpperBodyShadowExpressionEnabled" init="false" description="Turn on upper body shadow Expression"/>
    <bool name="m_UpperBodyFeatheredLeanEnabled" init="false" description="Turn on upper body lean state"/>
    <bool name="m_UseWeaponAnimsForGrip" init="false" description="Use underlying weapon anims for grip"/>
    <bool name="m_UseLeftHandIk" init="false"/>
    <float name="m_IdleTransitionBlendOutTime" min="0.0f" max="1.0f" init="0.25f" description="Time to blend out of idle transition"/>
    <array name="m_IdleTransitions" type="atArray">
      <string type="atHashString"/>
    </array>
    <string name="m_UnholsterClipSetId" type="atHashString"/>
    <string name="m_UnholsterClipData" type="atHashString"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityMovementModes::sUnholsterClipData">
    <string name="m_Name" type="atHashString"/>
    <array name="m_UnholsterClips" type="atArray">
      <struct type="CPedModelInfo::PersonalityMovementModes::sUnholsterClipData::sUnholsterClip"/>
    </array>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityMovementModes::sUnholsterClipData::sUnholsterClip">
    <array name="m_Weapons" type="atArray">
      <string type="atHashString"/>
    </array>
    <string name="m_Clip" type="atHashString"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityBravery">
    <string name="m_Name" type="atHashString" ui_key="true" description="Name of the bravery personality"/>
    <bitset name="m_Flags" type="fixed" numBits="32" values="eBraveryFlags" description="Bitmask storing the eBraveryFlags for this personality"/>
	  <float name="m_TakedownProbability" init="1.0f" min="0.0f" max="1.0f" description="Probability of doing weapon takedowns"/>
    <struct name="m_ThreatResponseUnarmed" type="CPedModelInfo::PersonalityThreatResponse" description="Threat response when unarmed"/>
    <struct name="m_ThreatResponseMelee" type="CPedModelInfo::PersonalityThreatResponse" description="Threat response when armed with melee weapon"/>
    <struct name="m_ThreatResponseArmed" type="CPedModelInfo::PersonalityThreatResponse" description="Threat response when armed with other weapon"/>
	  <struct name="m_FleeDuringCombat" type="CPedModelInfo::PersonalityFleeDuringCombat"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityThreatResponse">
    <struct name="m_Action" type="CPedModelInfo::PersonalityThreatResponse::Action" description="Action properties"/>
    <struct name="m_Fight" type="CPedModelInfo::PersonalityThreatResponse::Fight" description="Fight properties"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityCriminality">
    <string name="m_Name" type="atHashString" ui_key="true" description="Name of the criminality personality"/>
    <bitset name="m_Flags" type="fixed" numBits="32" values="eCriminalityFlags" description="Bitmask storing the eCriminalityFlags for this personality"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityThreatResponse::Action">
    <struct name="m_Weights" type="CPedModelInfo::PersonalityThreatResponse::Action::Weights" description="Fight, threaten and flee probabilities for this response"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityThreatResponse::Action::Weights">
    <float name="m_Fight" init="1.0f" min="0.0f" max="1.0f" description="Probability to start a fight"/>
    <float name="m_Flee" init="1.0f" min="0.0f" max="1.0f" description="Probability to flee"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityThreatResponse::Fight">
    <struct name="m_Weights" type="CPedModelInfo::PersonalityThreatResponse::Fight::Weights" description="Draw weapon and match weapon probabilities for this response"/>
    <float name="m_ProbabilityDrawWeaponWhenLosing" init="1.0f" min="0.0f" max="1.0f" description="Probability to draw weapon when losing"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityThreatResponse::Fight::Weights">
    <float name="m_KeepWeapon" init="1.0f" min="0.0f" max="1.0f" description="Probability to keep weapon"/>
    <float name="m_MatchTargetWeapon" init="1.0f" min="0.0f" max="1.0f" description="Probability to match target weapon"/>
    <float name="m_EquipBestWeapon" init="1.0f" min="0.0f" max="1.0f" description="Probability to equip best weapon"/>
  </structdef>

	<structdef type="CPedModelInfo::PersonalityFleeDuringCombat">
		<bool name="m_Enabled" init="false"/>
		<float name="m_ChancesWhenBuddyKilledWithScaryWeapon" init="0.0f" min="0.0f" max="1.0f"/>
	</structdef>

  <structdef type="CPedModelInfo::PersonalityAgility">
    <bitset name="m_Flags" type="fixed" numBits="32" values="eAgilityFlags" description="Bitmask storing the eAgilityFlags for this personality"/>
    <float name="m_MovementCostModifier" init="1.0f" min="1.0f" max="5.0f" description="Modifies the movement cost when this personality is active"/>
  </structdef>
  
  <structdef type="CPedModelInfo::PersonalityData">
    <string name="m_PersonalityName" type="atHashString" ui_key="true" description="Name of this personality"/>
    <string name="m_DefaultWeaponLoadout" type="atHashString" description="Name of default weapon loadout"/>
    <string name="m_Bravery" type="atHashString" description="Name of bravery personality"/>
    <string name="m_AgitatedPersonality" type="atHashString" description="Name of agitated personality"/>
    <string name="m_Criminality" type="atHashString" description="Name of criminality personality"/>
    <string name="m_AgitationTriggers" type="atHashString" init="Default" description="Name of agitation triggers"/>
    <string name="m_HealthConfigHash" type="atHashString" init="STANDARD_PED" description="Name of health config"/>
	<array name="m_WeaponAnimations" type="atArray">
      <string type="atHashString"/>
    </array>
    <string name="m_AmbientAudio" type="atHashString" init="Default"/>
    <string name="m_WitnessPersonality" type="atHashString" init="Default"/>
    <struct name="m_Agility" type="CPedModelInfo::PersonalityAgility" description="Agility personality"/>
    <bool name="m_IsMale" init="true" description="Flag the personality as male (female if false)"/>
    <bool name="m_IsHuman" init="true" description="Flag the personality as Human"/>
    <bool name="m_ShouldRewardMoneyOnDeath" description="Rewards money when ped with this personality is killed"/>
    <bool name="m_IsGang" init="false" description="Is personality part of a gang"/>
    <bool name="m_IsSecurity" init="false" description="Does this ped act like a security guard?"/>
    <bool name="m_IsWeird" init="false" description="Does this ped generate weirdness shocking events."/>
    <bool name="m_IsDangerousAnimal" init="false" description="Does this ped generate dangerous animal shocking events?"/>
    <bool name="m_CausesRumbleWhenCollidesWithPlayer" init="false" description="Does this ped cause controller rumble when colliding with the player?"/>
    <bool name="m_AllowSlowCruisingWithMusic" init="false" description="Can this ped drive around real slow with the windows down and music up"/>
    <bool name="m_AllowRoadCrossHurryOnLightChange" init="true" description="Can this ped move faster than a walk due to traffic light change while crossing road"/>
    <bitset name="m_VehicleTypes" type="fixed" numBits="8" values="ePedVehicleTypes" description="Flags deciding what vehicles types can be driven"/>
    <float name="m_AttackStrengthMin" init="0.8f" min="0.01f" max="5.0f" description="Minimum attack strength"/>
    <float name="m_AttackStrengthMax" init="5.0f" min="0.01f" max="5.0f" description="Maximum attack strength"/>
    <float name="m_StaminaEfficiency" init="1.0f" min="0.01f" max="2.0f" description="Stamina efficiency"/>
    <float name="m_ArmourEfficiency" init="1.0f" min="0.01f" max="2.0f" description="Armor efficiency"/>
    <float name="m_HealthRegenEfficiency" init="1.0f" min="0.01f" max="2.0f" description="Health regeneration efficiency"/>
    <float name="m_ExplosiveDamageMod" init="1.0f" min="0.01f" max="2.0f" description="Modifier for damage from explosives"/>
    <float name="m_HandGunDamageMod" init="1.0f" min="0.01f" max="2.0f" description="Modifier for damage from hand guns"/>
    <float name="m_RifleDamageMod" init="1.0f" min="0.01f" max="2.0f" description="Modifier for damage from rifles"/>
    <float name="m_SmgDamageMod" init="1.0f" min="0.01f" max="2.0f" description="Modifier for damage from SMGs"/>
    <float name="m_PopulationFleeMod" init="1.0f" min="0.01f" max="3.0f" description="Population flee modifier"/>
    <float name="m_HotwireRate" init="1.0f" min="0.8f" max="2.0f" description="Car hotwire rate"/>
    <u32 name="m_MotivationMin" init="0" min="0" max="10" description="Minimum motivation level"/>
    <u32 name="m_MotivationMax" init="0" min="0" max="10" description="Maximum motivation level"/>
    <u8 name="m_DrivingAbilityMin" init="5" min="0" max="10" description="Minimum driving ability level"/>
    <u8 name="m_DrivingAbilityMax" init="10" min="0" max="10" description="Maximum driving ability level"/>
    <u8 name="m_DrivingAggressivenessMin" init="5" min="0" max="10" description="Minimum driving aggressiveness level"/>
    <u8 name="m_DrivingAggressivenessMax" init="10" min="0" max="10" description="Maximum driving aggressiveness level"/>
    <enum name="m_Affluence" type="Affluence" init="AFF_AVERAGE" description="How wealthy the ped is"/>
    <enum name="m_TechSavvy" type="TechSavvy" init="TS_HIGH" description="How good a ped is with technology"/>
    <string name="m_MovementModes" type="atHashString"/>
    <string name="m_WeaponAnimsFPSIdle" type="atHashString" init="FirstPerson"/>
    <string name="m_WeaponAnimsFPSRNG" type="atHashString" init="FirstPersonRNG"/>
    <string name="m_WeaponAnimsFPSLT" type="atHashString" init="FirstPersonAiming"/>
    <string name="m_WeaponAnimsFPSScope" type="atHashString" init="FirstPersonScope"/>
  </structdef>

  <structdef type="CPedModelInfo::PersonalityDataList">
    <array name="m_MovementModeUnholsterData" type="atArray">
      <struct type="CPedModelInfo::PersonalityMovementModes::sUnholsterClipData"/>
    </array>
    <array name="m_MovementModes" type="atArray">
      <struct type="CPedModelInfo::PersonalityMovementModes"/>
    </array>
    <array name="m_PedPersonalities" type="atArray">
      <struct type="CPedModelInfo::PersonalityData"/>
    </array>
    <array name="m_BraveryTypes" type="atArray">
      <struct type="CPedModelInfo::PersonalityBravery"/>
    </array>
    <array name="m_CriminalityTypes" type="atArray">
      <struct type="CPedModelInfo::PersonalityCriminality"/>
    </array>
  </structdef>

  <structdef type="SPedPerceptionInfo">
    <string name="m_Name" type="atHashString" description="Identifier for this perception type."/>
    <float name="m_HearingRange" init="60.0f" min="0.0f" max="9999.0f" description="How far a ped can hear."/>
    <float name="m_EncroachmentRange" init="20.0f" min="0.0f" max="9999.0f" description="How far away from which a ped cares if something is moving towards them."/>
    <float name="m_EncroachmentCloseRange" init="4.0f" min="0.0f" max="9999.0f" description="How far away from which a ped will react from things being close to them."/>
    <float name="m_SeeingRange" init="60.0f" min="0.0f" max="9999.0f" description="How far a ped can see in the center field of view cone."/>
    <float name="m_SeeingRangePeripheral" init="5.0f" min="0.0f" max="9999.0f" description="How far a ped can see peripherally."/>
    <float name="m_VisualFieldMinAzimuthAngle" init="-90.0f" min="-180.0f" max="0.0f" description="Minimum azimuth angle. (in degrees)"/>
    <float name="m_VisualFieldMaxAzimuthAngle" init="90.0f" min="0.0f" max="180.0f" description="Maximum azimuth angle. (in degrees)"/>
    <float name="m_VisualFieldMinElevationAngle" init="-75.0f" min="-180.0f" max="0.0f" description="Minimum elevation angle. (in degrees)"/>
    <float name="m_VisualFieldMaxElevationAngle" init="60.0f" min="0.0f" max="180.0f" description="Maximum elevation angle. (in degrees)"/>
    <float name="m_CentreFieldOfGazeMaxAngle" init="60.0f" min="0.0f" max="90.0f" description="Centre of gaze max angle. (in degrees)"/>
    <bool name="m_CanAlwaysSenseEncroachingPlayers" init="false" description="Is this ped allowed to bypass hearing/vision for encroachment if its done by the player?"/>
    <bool name="m_PerformEncroachmentChecksIn3D" init="false" description="Should the encroachment distance check be made in 3D?" />
  </structdef>

  <structdef type="CPedPerceptionInfoManager">
    <array name="m_aPedPerceptionInfoData" type="atArray">
      <struct type="SPedPerceptionInfo"/>
    </array>
  </structdef>

</ParserSchema>
