<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CNetTransactionItemKey" cond="__DEV">
    <string name="m_key" type="atHashString" />
  </structdef>

  <structdef type="CNetTransactionItemKey" cond="!__DEV">
    <int name="m_key" />
  </structdef>

  <structdef type="CNetworkShoppingMgr">
    <array name="m_transactiontypes" type="atArray" >
      <struct type="CNetTransactionItemKey" />
    </array>
    <array name="m_actiontypes" type="atArray" >
      <struct type="CNetTransactionItemKey" />
    </array>
  </structdef>

</ParserSchema>
