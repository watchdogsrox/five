<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="BudgetEntryId">
	<enumval name="BE_DIR_AMB_REFL"/>
	<enumval name="BE_SCENE_LIGHTS"/>
	<enumval name="BE_CASCADE_SHADOWS"/>	
	<enumval name="BE_LOCAL_SHADOWS"/>	
	<enumval name="BE_AMBIENT_VOLUMES"/>
	<enumval name="BE_CORONAS"/>
	<enumval name="BE_ALPHA"/>	
	<enumval name="BE_SKIN_LIGHTING"/>
	<enumval name="BE_FOLIAGE_LIGHTING"/>
	<enumval name="BE_VISUAL_FX"/>	
	<enumval name="BE_SSA"/>
	<enumval name="BE_POSTFX"/>
	<enumval name="BE_DEPTHFX"/>	
	<enumval name="BE_EMISSIVE"/>
	<enumval name="BE_WATER"/>
	<enumval name="BE_SKY"/>
	<enumval name="BE_DEPTH_RESOLVE"/>
	<enumval name="BE_VOLUMETRIC_FX"/>	
	<enumval name="BE_DISPLACEMENT"/>
	<enumval name="BE_DEBUG"/>
	<enumval name="BE_LODLIGHTS"/>
	<enumval name="BE_TOTAL_LIGHTS"/>
	<enumval name="BE_COUNT"/>	
</enumdef>

<enumdef type="BudgetWindowGroupId">
	<enumval name="WG_MAIN_TIMINGS"/>
	<enumval name="WG_SHADOW_TIMINGS"/>
	<enumval name="WG_MISC_TIMINGS"/>	
	<enumval name="WG_COUNT"/>	
</enumdef>

<structdef type="BudgetEntry">
	<enum name="id" type="BudgetEntryId" init="BE_COUNT"/>
	<string name="pName" type="member" size="32"/>
	<string name="pDisplayName" type="member" size="16"/>
	<float name="time" init="0.0f" min="0.0f" max="100.0f"/>
	<float name="budgetTime" init="1.0f" min="0.0f" max="100.0f"/>
	<enum name="windowGroupId" type="BudgetWindowGroupId" init="WG_COUNT"/>
	<bool name="bEnable" init="false"/>
</structdef>

<structdef type="BudgetDisplay">
	<array name="m_Entries" type="atArray">
		<struct type="BudgetEntry"/>
	</array>
</structdef>

</ParserSchema>