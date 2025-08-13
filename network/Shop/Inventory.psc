<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="netInventoryBaseItem" constructable="false">
    <string name="m_itemKey" type="atHashString" />
    <s64 name="m_amount" />
    <s64 name="m_statvalue" />
    <s64 name="m_pricepaid" />
  </structdef>

  <structdef type="netInventoryPackedStatsItem" base="netInventoryBaseItem">
    <int name="m_charSlot" />
  </structdef>

</ParserSchema>
