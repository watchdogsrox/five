<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CVfxRoomSetup">
    <string name="m_interiorInfoName" type="atHashString"/>
  </structdef>

  <structdef type="CVfxInteriorSetup">
    <string name="m_interiorInfoName" type="atHashString"/>
    <map name="m_vfxRoomSetups" type="atBinaryMap" key="atHashString">
      <pointer type="CVfxRoomSetup" policy="owner"/>
    </map>
  </structdef>

  <structdef type="CVfxInteriorInfo">
    <bool name="m_smokePtFxEnabled" init="false"/>
    <string name="m_smokePtFxName" type="atHashString"/>
    <float name="m_smokePtFxLevelOverride" init="-1.0f"/>
    <bool name="m_dustPtFxEnabled" init="false"/>
    <string name="m_dustPtFxName" type="atHashString"/>
    <float name="m_dustPtFxEvo" init="1.0f"/>
  </structdef>

  <structdef type="CVfxInteriorInfoMgr">
    <map name="m_vfxInteriorSetups" type="atBinaryMap" key="atHashString">
      <pointer type="CVfxInteriorSetup" policy="owner"/>
    </map>
    <map name="m_vfxInteriorInfos" type="atBinaryMap" key="atHashString">
      <pointer type="CVfxInteriorInfo" policy="owner"/>
    </map>
  </structdef>

</ParserSchema>
