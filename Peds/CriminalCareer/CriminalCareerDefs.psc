<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::ItemLimits" simple="true">
	<int name="m_maximum" parName="Maximum" init="-1"/>
	<int name="m_minimum" parName="Minimum" init="-1"/>
</structdef>

<!-- These entries should match the definitions in catalog.meta and CHARACTER_CAREER_TYPE in shared_global_definitions.sch -->
<enumdef type="CriminalCareerDefs::CareerIdentifier" values="hash">
	<enumval name="executive_trafficker"/>
	<enumval name="gunrunner"/>
	<enumval name="nightclub_trafficker"/>
	<enumval name="biker"/>
</enumdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::ItemCategoryDisplayData" simple="true">
	<string name="m_displayNameKey" parName="DisplayName" type="atHashString"/>
	<string name="m_tooltipAddKey" parName="TooltipAdd" type="atHashString"/>
	<string name="m_tooltipCESPAddKey" parName="TooltipCESPAdd" type="atHashString"/>
	<string name="m_tooltipRemoveKey" parName="TooltipRemove" type="atHashString"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::ItemFeature" simple="true">
	<string name="m_identifier" parName="Identifier" type="atHashString"/> <!-- Identifiers are assigned in CCriminalCareerCatalog::PostLoad -->
	<string name="m_displayNameKey" parName="DisplayName" type="atHashString"/>
	<string name="m_blipName" parName="BlipName" type="ConstString"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::ProfileStatData" simple="true" onPostLoad="PostLoad">
	<string name="m_identifier" parName="Identifier" type="ConstString"/>
	<string name="m_valueFormula" parName="ValueFormula" type="atString"/>
	<int name="m_value" parName="Value" init="1"/>
	<int name="m_bitshift" parName="Bitshift" init="0"/>
	<int name="m_bitsize" parName="Bitsize" init="32"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::ProfileStatDefinition" simple="true">
	<string name="m_identifier" parName="Identifier" type="ConstString"/>
	<int name="m_bitshift" parName="Bitshift" init="0"/>
	<int name="m_bitsize" parName="Bitsize" init="32"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::VehicleDisplaySlot" simple="true">
	<string name="m_identifier" parName="Identifier" type="ConstString"/>
	<int name="m_bitshift" parName="Bitshift" init="0"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::VehicleSaveSlot" simple="true">
	<struct name="m_model" parName="Model" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_color1" parName="Color1" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_color2" parName="Color2" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_colorExtra1" parName="ColorExtra1" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_colorExtra2" parName="ColorExtra2" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_color5" parName="Color5" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_color6" parName="Color6" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_modSpoiler" parName="ModSpoiler" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_modChassis" parName="ModChassis" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_modLivery" parName="ModLivery" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_extra1" parName="Extra1" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_extra2" parName="Extra2" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_extra3" parName="Extra3" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_extra4" parName="Extra4" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_extra5" parName="Extra5" type="CriminalCareerDefs::ProfileStatDefinition"/>
	<struct name="m_insurance" parName="Insurance" type="CriminalCareerDefs::ProfileStatDefinition"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::VehicleSaveStats" simple="true">
	<int name="m_saveSlotIndex" parName="SaveSlotIndex" init="-1"/>
	<int name="m_model" parName="Model"/>
	<int name="m_color1" parName="Color1"/>
	<int name="m_color2" parName="Color2"/>
	<int name="m_colorExtra1" parName="ColorExtra1"/>
	<int name="m_colorExtra2" parName="ColorExtra2"/>
	<int name="m_color5" parName="Color5"/>
	<int name="m_color6" parName="Color6"/>
	<int name="m_modSpoiler" parName="ModSpoiler"/>
	<int name="m_modChassis" parName="ModChassis"/>
	<int name="m_modLivery" parName="ModLivery"/>
	<int name="m_extra1" parName="Extra1"/>
	<int name="m_extra2" parName="Extra2"/>
	<int name="m_extra3" parName="Extra3"/>
	<int name="m_extra4" parName="Extra4"/>
	<int name="m_extra5" parName="Extra5"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::VehicleDisplayData" simple="true">
	<string name="m_defaultLocationIdentifier" parName="DefaultLocation" type="atHashString"/>
	<map name="m_vehicleDisplayLocations" parName="DisplayLocations" type="atBinaryMap" key="atHashString">
		<array parName="DisplayLocation" type="atArray">
			<struct parName="DisplaySlot" type="CriminalCareerDefs::VehicleDisplaySlot"/>
		</array>
	</map>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::VehicleStatsData" simple="true">
	<int name="m_topSpeed" parName="TopSpeed" init="-1"/>
	<int name="m_acceleration" parName="Acceleration" init="-1"/>
	<int name="m_braking" parName="Braking" init="-1"/>
	<int name="m_traction" parName="Traction" init="-1"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::WeaponStatsData" simple="true">
	<int name="m_damage" parName="Damage" init="-1"/>
	<int name="m_fireRate" parName="FireRate" init="-1"/>
	<int name="m_accuracy" parName="Accuracy" init="-1"/>
	<int name="m_range" parName="Range" init="-1"/>
	<int name="m_clipSize" parName="ClipSize" init="-1"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CriminalCareerDefs::ShoppingCartItemLimits" simple="true">
	<string name="m_category" parName="ItemCategory" type="atHashString"/> <!-- Categories are assigned in CCriminalCareerShoppingCartValidationData::PostLoad -->
	<struct name="m_quantityLimits" parName="QuantityLimits" type="CriminalCareerDefs::ItemLimits"/>
	<struct name="m_spendLimits" parName="SpendLimits" type="CriminalCareerDefs::ItemLimits"/>
</structdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CCriminalCareer" simple="true">
	<enum name="m_identifier" parName="Identifier" type="CriminalCareerDefs::CareerIdentifier"/>
	<string name="m_displayNameKey" parName="DisplayName" type="atHashString"/>
	<string name="m_descriptionKey" parName="Description" type="atHashString"/>
	<string name="m_textureDictionaryName" parName="TextureDictionaryName" type="ConstString"/>
	<string name="m_backgroundParallaxTexture" parName="BackgroundParallaxTexture" type="ConstString"/>
	<string name="m_foregroundParallaxTexture" parName="ForegroundParallaxTexture" type="ConstString"/>
	<string name="m_interactiveMusicTrigger" parName="InteractiveMusicTrigger" type="ConstString"/>
	<string name="m_previewImagePath" parName="PreviewImagePath" type="ConstString"/>
	<string name="m_tooltipSelectKey" parName="TooltipSelect" type="atHashString"/>
	<array name="m_itemCategories" parName="ItemCategories" type="atArray">
		<string type="atHashString"/>
	</array>
</structdef>

<enumdef type="CriminalCareerDefs::ItemFlags">
	<enumval name="AlwaysInShoppingCart"/>
	<enumval name="IgnoreForValidation"/>
	<enumval name="WindfallGift"/>
</enumdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CCriminalCareerItem" simple="true">
	<string name="m_identifier" parName="Identifier" type="atHashString"/> <!-- Identifiers are assigned in CCriminalCareerCatalog::PostLoad -->
	<string name="m_textureDictionaryName" parName="TextureDictionaryName" type="ConstString"/>
	<string name="m_telemetryKey" parName="TelemetryKey" type="atHashString"/>
	<array name="m_profileStatsData" parName="ProfileStats" type="atArray">
		<struct parName="ProfileStat" type="CriminalCareerDefs::ProfileStatData"/>
	</array>
	<struct name="m_vehicleSaveStats" parName="VehicleSaveStats" type="CriminalCareerDefs::VehicleSaveStats"/>
	<string name="m_vehicleDisplayLocationPreference" parName="VehicleDisplayLocationPreference" type="atHashString"/>
	<string name="m_category" parName="Category" type="atHashString"/>
	<array name="m_careerAvailability" parName="CareerAvailability" type="atArray">
		<string type="atHashString"/>
	</array>
	<int name="m_cost" parName="Cost"/>
	<int name="m_costCESP" parName="CostCESP"/>
	<string name="m_titleKey" parName="TitleKey" type="atHashString"/>
	<array name="m_featureIdentifiers" parName="Features" type="atArray">
		<string type="atHashString"/>
	</array>
	<string name="m_locationKey" parName="Location" type="atHashString"/>
	<string name="m_descriptionKey" parName="DescriptionKey" type="atHashString"/>
	<string name="m_thumbnailPath" parName="ThumbnailPath" type="ConstString"/>
	<array name="m_backgroundImagePaths" parName="BackgroundImagePaths" type="atArray">
		<string type="ConstString"/>
	</array>
	<bitset name="m_flags" parName="Flags" type="fixed" values="CriminalCareerDefs::ItemFlags"/>
	<struct name="m_vehicleStatsData" parName="VehicleStats" type="CriminalCareerDefs::VehicleStatsData"/>
	<struct name="m_weaponStatsData" parName="WeaponStats" type="CriminalCareerDefs::WeaponStatsData"/>
	<int name="m_carouselIndexOverride" parName="CarouselIndexOverride" init="-1"/>
</structdef>

<enumdef type="CriminalCareerDefs::ShoppingCartValidationErrorType">
	<enumval name="ShoppingCartValidationErrorType_QuantityInsufficient"/>
	<enumval name="ShoppingCartValidationErrorType_QuantityExceeded"/>
	<enumval name="ShoppingCartValidationErrorType_SpendInsufficient"/>
	<enumval name="ShoppingCartValidationErrorType_SpendExceeded"/>
</enumdef>

<structdef cond="GEN9_STANDALONE_ENABLED" type="CCriminalCareerShoppingCartValidationData" simple="true" onPostLoad="PostLoad">
	<struct name="m_totalLimits" parName="TotalLimits" type="CriminalCareerDefs::ShoppingCartItemLimits"/>
	<map name="m_itemCategoryLimits" parName="ItemCategoryLimits" type="atMap" key="atHashString">
		<struct type="CriminalCareerDefs::ShoppingCartItemLimits"/>
	</map>
</structdef>

</ParserSchema>
