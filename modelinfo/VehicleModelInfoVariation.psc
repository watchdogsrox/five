<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CVehicleModelColorIndices">
    <array name="m_indices" type="member" size="6">
      <u8/>
    </array>
    <array name="m_liveries" type="member" size="30">
      <bool/>
    </array>
  </structdef>

  <structdef type="CVehicleVariationData">
    <string name="m_modelName" type="pointer" ui_key="true"/>
		<array name="m_colors" type="atArray">
			<struct type="CVehicleModelColorIndices"/>
		</array>
    <array name="m_kits" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_windowsWithExposedEdges" type="atArray">
      <string type="atHashString" />
    </array>
		<struct name="m_plateProbabilities" type="CVehicleModelPlateProbabilities" />
		<u8 name="m_lightSettings" init="0xFF" />
		<u8 name="m_sirenSettings" init="0xFF" />
  </structdef>
  
	<structdef type="CVehicleModelInfoVariation">
    <array name="m_variationData" type="atArray">
		  <struct type="CVehicleVariationData"/>
		</array>
	</structdef>

</ParserSchema>