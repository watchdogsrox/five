<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CLandingOnlineLayout::eLayoutType">
    <enumval name="LAYOUT_ONE_PRIMARY" value="0" />
    <enumval name="LAYOUT_TWO_PRIMARY" />
    <enumval name="LAYOUT_ONE_PRIMARY_TWO_SUB" />
    <enumval name="LAYOUT_ONE_WIDE_TWO_SUB" />
    <enumval name="LAYOUT_ONE_WIDE_ONE_SUB_TWO_MINI" />
    <enumval name="LAYOUT_ONE_WIDE_TWO_MINI_ONE_SUB" />
    <enumval name="LAYOUT_FOUR_SUB" />
    <enumval name="LAYOUT_COUNT" />
  </enumdef>

  <structdef type="CLandingOnlineLayout" base="CPageGridSimple">
    <enum name="m_layoutType" parName="Type" type="CLandingOnlineLayout::eLayoutType" init="0" />
  </structdef>

</ParserSchema>
