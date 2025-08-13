<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CSimpleImpulseTest">
		<enum name="m_Impulse" init="CActionFlags::I_NONE" type="CActionFlags::Impulse"/>
		<enum name="m_State" init="CActionFlags::IS_NONE" type="CActionFlags::ImpulseState"/>
		<Vector2 name="m_AnalogRange" init="0.0f, 0.0f"/>
		<float name="m_Duration" init="0.2"/>
	</structdef>

	<structdef type="CImpulseTestContainer" platform="tool">
		<array name="m_aImpulseTests" type="atArray">
			<pointer type="CImpulseTest" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CImpulseTest">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<enum name="m_ImpulseOp" init="CActionFlags::IO_OR_IMPULSE_1" type="CActionFlags::ImpulseOp"/>
		<array name="m_aImpulses" type="atArray">
			<pointer type="CSimpleImpulseTest" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CRelativeRange">
		<Vector2 name="m_GroundHeightRange" init="0.0f, 0.0f"/>
		<Vector2 name="m_DistanceRange" init="0.0f, 0.0f"/>
		<Vector2 name="m_PlanarAngularRange" init="0.0f, 0.0f"/>
		<float name="m_HeightDifference" init="0.0f"/>
		<bitset name="m_RelativeRangeAttrs" init="CActionFlags::RRA_CHECK_HEIGHT_RANGE|CActionFlags::RRA_CHECK_DISTANCE_RANGE|CActionFlags::RRA_CHECK_ANGULAR_RANGE" type="fixed" numBits="CActionFlags::RelativeRangeAttrs_NUM_ENUMS" values="CActionFlags::RelativeRangeAttrs"/>
	</structdef>

	<structdef type="CInterrelationTestContainer" platform="tool">
		<array name="m_aInterrelationTests" type="atArray">
			<pointer type="CInterrelationTest" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CActionRumble">
		<string	name="m_Name" type="atHashString"/>
		<u32 name="m_Duration" init="0"/>
		<float name="m_Intensity" init="1.0"/>
	</structdef>

	<structdef type="CActionRumbleContainer" platform="tool">
		<array name="m_aActionRumbles" type="atArray">
			<pointer type="CActionRumble" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CInterrelationTest">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<float name="m_RootOffsetZ" init="0.0"/>
		<struct name="m_RelativeRange" type="CRelativeRange"/>
	</structdef>

	<structdef type="CHomingContainer" platform="tool">
		<array name="m_aHomings" type="atArray">
			<pointer type="CHoming" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CHoming">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<enum name="m_TargetType" init="CActionFlags::TT_ENTITY_ROOT" type="CActionFlags::TargetType"/>
		<Vector2 name="m_TargetOffset" init="0.0f, 0.0f"/>
		<float name="m_ActivationDistance" init="1.0"/>
		<float name="m_MaxSpeed" init="2.0"/>
		<float name="m_TargetHeading" init="0.0"/>
		<bitset name="m_HomingAttrs" init="CActionFlags::HA_CHECK_TARGET_DISTANCE|CActionFlags::HA_CHECK_TARGET_HEADING" type="fixed" numBits="CActionFlags::HomingAttrs_NUM_ENUMS" values="CActionFlags::HomingAttrs"/>
	</structdef>

	<structdef type="CDamageAndReactionContainer" platform="tool">
		<array name="m_aDamageAndReactions" type="atArray">
			<pointer type="CDamageAndReaction" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CDamageAndReaction">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<string name="m_ActionRumble" type="atHashString"/>
		<float name="m_OnHitDamageAmountMin" init="1.0"/>
		<float name="m_OnHitDamageAmountMax" init="1.0"/>
		<float name="m_SelfDamageAmount" init="0.0"/>
		<enum name="m_ForcedSelfDamageBoneTag" type="eAnimBoneTag"/>
		<bitset name="m_DamageReactionAttrs" init="CActionFlags::DRA_FORCE_IMMEDIATE_REACTION" type="fixed" numBits="CActionFlags::DamageReactionAttrs_NUM_ENUMS" values="CActionFlags::DamageReactionAttrs"/>
	</structdef>

	<structdef type="CStrikeBone">
		<enum name="m_StrikeBoneTag" type="eAnimBoneTag"/>
		<float name="m_Radius" init="0.075"/>
	</structdef>

	<structdef type="CStrikeBoneSetContainer" platform="tool">
		<array name="m_aStrikeBoneSets" type="atArray">
			<pointer type="CStrikeBoneSet" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CStrikeBoneSet">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_aStrikeBones" type="atArray">
			<pointer type="CStrikeBone" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CActionCondSpecial">
		<string name="m_SpecialTest" type="atHashString" ui_key="true"/>
    <bitset name="m_Filter" init="CActionFlags::PAFT_ANY" type="fixed" numBits="CActionFlags::PlayerAiFilterType_NUM_ENUMS" values="CActionFlags::PlayerAiFilterType"/>
		<bool name="m_InvertSense" init="false"/>
	</structdef>

	<structdef type="CActionBranch">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<bitset name="m_Filter" init="CActionFlags::PAFT_ANY" type="fixed" numBits="CActionFlags::PlayerAiFilterType_NUM_ENUMS" values="CActionFlags::PlayerAiFilterType"/>
	</structdef>

	<structdef type="CActionBranchSetContainer" platform="tool">
		<array name="m_aActionBranchSets" type="atArray">
			<pointer type="CActionBranchSet" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CActionBranchSet">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_aActionBranches" type="atArray">
			<pointer type="CActionBranch" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CActionVfx">
		<string	name="m_Name" type="atHashString"/>
		<string	name="m_VfxName" type="atHashString"/>
    <string name="m_dmgPackName" type="atHashString"/>
		<Vector3 name="m_Offset"/>
		<Vector3 name="m_Rotation"/>
		<enum name="m_BoneTag" type="eAnimBoneTag"/>
		<float name="m_Scale" init="1.0"/>
	</structdef>

	<structdef type="CActionVfxContainer" platform="tool">
		<array name="m_aActionVfx" type="atArray">
			<pointer type="CActionVfx" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CActionFacialAnimSet">
		<string	name="m_Name" type="atHashString"/>
		<array name="m_aFacialAnimClipIds" type="atArray">
			<string type="atHashString"/>
		</array>
	</structdef>

	<structdef type="CActionFacialAnimSetContainer" platform="tool">
		<array name="m_aActionFacialAnimSets" type="atArray">
			<pointer type="CActionFacialAnimSet" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CActionResult">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<int name="m_Priority" init="0"/>
		<string name="m_ClipSet" type="atHashString"/>
		<string name="m_FirstPersonClipSet" type="atHashString"/>
		<string name="m_Anim" type="atHashString"/>
		<float name="m_AnimBlendInRate" init="0.125"/>
		<float name="m_AnimBlendOutRate" init="0.25"/>
		<array name="m_CameraAnimLeft" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_CameraAnimRight" type="atArray">
			<string type="atHashString"/>
		</array>
		<bitset name="m_ResultAttrs" init="CActionFlags::RA_IS_STANDARD_ATTACK" type="fixed" numBits="CActionFlags::ResultAttrs_NUM_ENUMS" values="CActionFlags::ResultAttrs"/>
		<string name="m_Homing" type="atHashString"/>
		<string name="m_ActionBranchSet" type="atHashString"/>
		<string name="m_DamageAndReaction" type="atHashString"/>
		<string name="m_StrikeBoneSet" type="atHashString"/>
		<string name="m_Sound" type="atHashString"/>
		<string name="m_NmReactionSet" type="atHashString"/>
		<enum name="m_CameraTarget" init="CActionFlags::CT_PED_HEAD" type="CActionFlags::CameraTarget"/>
		<bool name="m_ApplyRelativeTargetPitch" init="false"/>
	</structdef>

	<structdef type="CActionResultContainer" platform="tool">
		<array name="m_aActionResults" type="atArray">
			<pointer type="CActionResult" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CWeaponActionResult">
		<bitset name="m_WeaponType" init="CActionFlags::RWT_ANY" type="fixed" numBits="CActionFlags::RequiredWeaponType_NUM_ENUMS" values="CActionFlags::RequiredWeaponType"/>
		<string name="m_ActionResult" type="atHashString"/>
	</structdef>

	<structdef type="CWeaponActionResultList">
		<bitset name="m_WeaponType" init="CActionFlags::RWT_ANY" type="fixed" numBits="CActionFlags::RequiredWeaponType_NUM_ENUMS" values="CActionFlags::RequiredWeaponType"/>
		<array name="m_aActionResults" type="atArray">
			<string type="atHashString"/>
		</array>
	</structdef>

	<structdef type="CActionCondPedOther">
		<bitset name="m_SelfFilter" init="CActionFlags::PAFT_ANY" type="fixed" numBits="CActionFlags::PlayerAiFilterType_NUM_ENUMS" values="CActionFlags::PlayerAiFilterType"/>
		<array name="m_aBrawlingStyles" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_aCondSpecials" type="atArray">
			<pointer type="CActionCondSpecial" policy="owner"/>
		</array>
		<array name="m_aWeaponActionResultList" type="atArray">
			<pointer type="CWeaponActionResultList" policy="owner"/>
		</array>
		<bool name="m_CheckAnimTime" init="false"/>
		<bitset name="m_MovementSpeed" init="CActionFlags::MS_ANY" type="fixed" numBits="CActionFlags::MovementSpeed_NUM_ENUMS" values="CActionFlags::MovementSpeed"/>
		<string name="m_InterrelationTest" type="atHashString"/>
	</structdef>

	<structdef type="CActionDefinitionContainer" platform="tool">
		<array name="m_aActionDefinitions" type="atArray">
			<pointer type="CActionDefinition" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CActionDefinition">
		<bool name="m_Debug" init="false"/>
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_aBrawlingStyles" type="atArray">
			<string type="atHashString"/>
		</array>
		<int name="m_Priority" init="0"/>
		<int name="m_SelectionRouletteBias" init="0"/>
		<bitset name="m_ActionType" init="CActionFlags::AT_ANY" type="fixed" numBits="CActionFlags::ActionType_NUM_ENUMS" values="CActionFlags::ActionType"/>
		<bitset name="m_SelfFilter" init="CActionFlags::PAFT_ANY" type="fixed" numBits="CActionFlags::PlayerAiFilterType_NUM_ENUMS" values="CActionFlags::PlayerAiFilterType"/>
		<bitset name="m_TargetEntity" init="CActionFlags::TET_ANY" type="fixed" numBits="CActionFlags::TargetEntityType_NUM_ENUMS" values="CActionFlags::TargetEntityType"/>
		<bitset name="m_MovementSpeed" init="CActionFlags::MS_ANY" type="fixed" numBits="CActionFlags::MovementSpeed_NUM_ENUMS" values="CActionFlags::MovementSpeed"/>
		<bitset name="m_DesiredDirection" init="CActionFlags::DD_ANY" type="fixed" numBits="CActionFlags::DesiredDirection_NUM_ENUMS" values="CActionFlags::DesiredDirection"/>
		<bitset name="m_DefinitionAttrs" init="CActionFlags::DA_IS_ENABLED" type="fixed" numBits="CActionFlags::DefinitionAttrs_NUM_ENUMS" values="CActionFlags::DefinitionAttrs"/>
		<string name="m_ImpulseTest" type="atHashString"/>
		<u32 name="m_MaxImpulseTestDelay" init="0"/>
		<string name="m_InterrelationTest" type="atHashString"/>
		<array name="m_aCondSpecials" type="atArray">
			<pointer type="CActionCondSpecial" policy="owner"/>
		</array>
		<struct name="m_CondPedOther" type="CActionCondPedOther"/>
		<array name="m_aWeaponActionResults" type="atArray">
			<pointer type="CWeaponActionResult" policy="owner"/>
		</array>
	</structdef>

	<structdef type="CImpulse">
		<enum name="m_Impulse" init="CActionFlags::I_NONE" type="CActionFlags::Impulse"/>
	</structdef>

	<structdef type="CImpulseCombo">
		<array name="m_aImpulses" type="atFixedArray" size="3">
			<struct type="CImpulse"/>
		</array>
	</structdef>

	<structdef type="CStealthKillTestContainer" platform="tool">
		<array name="m_aStealthKillTests" type="atArray">
			<pointer type="CStealthKillTest" policy="owner"/>
		</array>
	</structdef>
	
	<structdef type="CStealthKillTest">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<bitset name="m_ActionType" init="CActionFlags::AT_ANY" type="fixed" numBits="CActionFlags::ActionType_NUM_ENUMS" values="CActionFlags::ActionType"/>
		<string name="m_StealthKillAction" type="atHashString"/>
	</structdef>
	
</ParserSchema>
