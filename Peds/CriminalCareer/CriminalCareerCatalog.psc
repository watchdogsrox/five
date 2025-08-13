<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef cond="GEN9_STANDALONE_ENABLED" type="CCriminalCareerCatalog" simple="true" onPostLoad="PostLoad">
    <array name="m_careers" parName="Careers" type="atArray">
        <struct type="CCriminalCareer" parName="Career"/>
    </array>
	<map name="m_itemCategoryDisplayData" parName="ItemCategories" type="atMap" key="atHashString">
		<struct type="CriminalCareerDefs::ItemCategoryDisplayData" parName="ItemCategory"/>
	</map>
	<map name="m_itemFeatures" parName="ItemFeatures" type="atMap" key="atHashString">
		<struct type="CriminalCareerDefs::ItemFeature" parName="ItemFeature"/>
	</map>
	<struct name="m_vehicleDisplayData" parName="VehicleDisplayData" type="CriminalCareerDefs::VehicleDisplayData"/>
	<map name="m_vehicleSaveSlots" parName="VehicleSaveSlots" type="atMap" key="s32">
		<struct type="CriminalCareerDefs::VehicleSaveSlot" parName="SaveSlot"/>
	</map>
	<map name="m_items" parName="Items" type="atMap" key="atHashString">
        <struct type="CCriminalCareerItem" parName="Item"/>
    </map>
</structdef>

</ParserSchema>
