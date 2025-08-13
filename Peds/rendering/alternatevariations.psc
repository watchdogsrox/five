<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CAlternateVariations::sAlternateVariationComponent" simple="true" onPreLoad="onPreLoad">
      <u8 name="m_component" init="255" />
      <u8 name="m_anchor" init="255" />
      <u8 name="m_index" />
    <string name="m_dlcNameHash" type="atHashString" init="0"/>
  </structdef>

  <structdef type="CAlternateVariations::sAlternateVariationSwitch" simple="true" onPreLoad="onPreLoad">
      <u8 name="m_component" />
      <u8 name="m_index" />
      <u8 name="m_alt" />
    <string name="m_dlcNameHash" type="atHashString" init="0"/>
      <array name="m_sourceAssets" type="atArray"> <!-- list of assets to replace -->
          <struct type="CAlternateVariations::sAlternateVariationComponent"/>
      </array>
  </structdef>

  <structdef type="CAlternateVariations::sAlternateVariationPed" simple="true">
      <string name="m_name" type="atHashString"/>
      <array name="m_switches" type="atArray"> <!-- list of switch combos -->
          <struct type="CAlternateVariations::sAlternateVariationSwitch"/>
      </array>
  </structdef>

  <structdef type="CAlternateVariations" simple="true">
    <array name="m_peds" type="atArray">
      <struct type="CAlternateVariations::sAlternateVariationPed"/>
    </array>
  </structdef>

</ParserSchema>
