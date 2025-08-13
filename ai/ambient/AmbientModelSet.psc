<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CAmbientModel">
    <string name="m_Name" type="atHashString" ui_key="true"/>
	<pointer type="CAmbientModelVariations" name="m_Variations" policy="owner"/>
    <float name="m_Probability" init="1" min="0" max="1"/>
  </structdef>

  <structdef type="CAmbientModelSet">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_Models" type="atArray">
      <struct type="CAmbientModel"/>
    </array>
  </structdef>

  <structdef type="CAmbientModelVariations" constructable="false">
  </structdef>

  <structdef type="CAmbientPedModelVariations" base="CAmbientModelVariations">
    <array name="m_CompRestrictions" type="atArray">
      <struct type="CPedCompRestriction"/>
    </array>
    <array name="m_PropRestrictions" type="atArray">
      <struct type="CPedPropRestriction"/>
    </array>
	<string name="m_LoadOut" type="atHashString" ui_key="true"/>
  </structdef>

  <structdef type="CAmbientVehicleModelVariations::Mod">
    <enum name="m_ModType" type="eVehicleModType"/>
    <u8 name="m_ModIndex"/>
  </structdef>

  <structdef type="CAmbientVehicleModelVariations" base="CAmbientModelVariations">
	<int name="m_BodyColour1" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_BodyColour2" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_BodyColour3" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_BodyColour4" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_BodyColour5" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_BodyColour6" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_WindowTint" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_ColourCombination" type="int" min="-1" max="255" step="1" init="-1"/>
	<int name="m_Livery" type="int" min="-1" max="100" step="1" init="-1"/>
	<int name="m_Livery2" type="int" min="-1" max="100" step="1" init="-1"/>
	<int name="m_ModKit" type="int" min="-1" max="100" step="1" init="-1"/>
    <array name="m_Mods" type="atArray">
      <struct type="CAmbientVehicleModelVariations::Mod"/>
    </array>
	<enum name="m_Extra1" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra2" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra3" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra4" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra5" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra6" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra7" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra8" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra9" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
	<enum name="m_Extra10" type="CAmbientVehicleModelVariations::UseExtra" init="CAmbientVehicleModelVariations::Either"/>
  </structdef>

  <enumdef type="CAmbientVehicleModelVariations::UseExtra">
    <enumval name="Either"/>
    <enumval name="MustUse"/>
    <enumval name="CantUse"/>
  </enumdef>

</ParserSchema>
