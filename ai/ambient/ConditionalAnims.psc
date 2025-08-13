<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CConditionalClipSetVFX">
		<string		name="m_Name" type="atHashString"/>
		<string		name="m_fxName" type="atHashString"/>
		<Vector3 	name="m_offsetPosition" />
		<Vector3 	name="m_eulerRotation" />
		<enum		name="m_boneTag" type="eAnimBoneTag" hideWidgets="true"/>
		<float 		name="m_scale" />
		<int 		name="m_probability" />
		<Color32 	name="m_color" />
	</structdef>
	
	<enumdef type="CConditionalAnims::eConditionalAnimFlags">
		<enumval name="SpawnPropInLeftHand"/>
		<enumval name="OverrideOnFootClipSetWithBase"/>
		<enumval name="DestroyPropInsteadOfDrop"/>
		<enumval name="IgnoreLowPriShockingEvents"/>
   		<enumval name="MobilePhoneConversation"/>
		<enumval name="OverrideWeaponClipSetWithBase"/>
		<enumval name="ForceBaseUntilIdleStreams"/>
    	<enumval name="DontPickOnEnter" />
	</enumdef>

	<enumdef type="CConditionalClipSet::eActionFlags">
		<enumval name="RemoveWeapons"/>
		<enumval name="IsSkyDive"/>
		<enumval name="IsArrest"/>
		<enumval name="IsProvidingCover"/>
		<enumval name="IsSynchronized"/>
		<enumval name="EndsInWalk"/>
		<enumval name="ConsiderOrientation"/>
		<enumval name="MobilePhoneConversation"/>
		<enumval name="ReplaceStandardJack"/>
		<enumval name="ForceIdleThroughBlendOut"/>
  </enumdef>

	<structdef type="CConditionalClipSet">
		<string name="m_ClipSet" type="atHashString"/>
		<string name="m_AssociatedSpeech" type="atHashString"/>
		<array name="m_Conditions" type="atArray">
			<pointer type="CScenarioCondition" policy="owner"/>
		</array>
		<bitset name="m_ActionFlags" type="fixed32" values="CConditionalClipSet::eActionFlags"/>
	</structdef>

	<enumdef type="CConditionalAnims::eAnimType">
		<enumval name="AT_BASE"/>
		<enumval name="AT_ENTER"/>
		<enumval name="AT_EXIT"/>
		<enumval name="AT_VARIATION"/>
		<enumval name="AT_REACTION"/>
		<enumval name="AT_PANIC_BASE"/>
		<enumval name="AT_PANIC_INTRO"/>
		<enumval name="AT_PANIC_OUTRO"/>
		<enumval name="AT_PANIC_VARIATION"/>
		<enumval name="AT_PANIC_EXIT"/>
	</enumdef>

	<structdef type="CScenarioTransitionInfo">
		<string name="m_TransitionToScenario" type="atHashString"/>
		<string name="m_TransitionToScenarioConditionalAnims" type="atHashString"/>
	</structdef>
	
	<structdef type="CConditionalAnims">

		<string name="m_Name" type="atHashString" ui_key="true"/>
		<string name="m_PropSet" type="atHashString"/>
		<bitset name="m_Flags" type="fixed32" values="CConditionalAnims::eConditionalAnimFlags"/>
		<float name="m_Probability" init="100.0f"/>
		<float name="m_SpecialConditionProbability"/>
		<pointer name="m_SpecialCondition" type="CScenarioCondition" policy="owner"/>
		<float name="m_NextIdleTime" init="2.0f"/>
		<float name="m_ChanceOfSpawningWithAnything" init="1.0f"/>
        <float name="m_BlendInDelta" init="8.0f"/>
        <float name="m_BlendOutDelta" init="-8.0f"/>
		<string name="m_GestureClipSetId" type="atHashString" init="" />

		<array name="m_VFXData" type="atArray">
			<pointer type="CConditionalClipSetVFX" policy="owner"/>
		</array>

        <float name="m_VFXCullRange" init="25.0f"/>
		<pointer name="m_TransitionInfo" type="CScenarioTransitionInfo" policy="owner"/>
		<array name="m_Conditions" type="atArray">
			<pointer type="CScenarioCondition" policy="owner"/>
		</array>

		<string name="m_LowLodBaseAnim" type="atHashString" description="Name of animation in the CTaskAmbientClips::Tunables::m_LowLodBaseClipSetId set, to use if the regular base animations are not streamed in."/>

		<array name="m_BaseAnims" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_Enters" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_Exits" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_Variations" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_Reactions" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		
		<array name="m_PanicBaseAnims" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_PanicIntros" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_PanicOutros" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_PanicVariations" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_PanicExits" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>

	</structdef>

</ParserSchema>
