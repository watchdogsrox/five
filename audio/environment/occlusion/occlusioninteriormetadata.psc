<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
              generate="class, psoChecks">


  <structdef type="naOcclusionPortalEntityMetadata" simple="true">
    <u32 name="m_LinkType"/>
    <float name="m_MaxOcclusion"/>
    <u32 name="m_EntityModelHashkey"/>
    <bool name="m_IsDoor" init="true"/>
    <bool name="m_IsGlass" init="true"/>
  </structdef>

  <structdef type="naOcclusionPathNodeChildMetadata" simple="true">
    <u32 name="m_PathNodeKey"/>
    <s32 name="m_PortalInfoIdx"/>
  </structdef>

  <structdef type="naOcclusionPortalInfoMetadata" simple="true">
    <u32 name="m_InteriorProxyHash"/>
    <s32 name="m_PortalIdx"/>
    <s32 name="m_RoomIdx"/>
    <u32 name="m_DestInteriorHash"/>
    <s32 name="m_DestRoomIdx"/>
    <array name="m_PortalEntityList" type="atArray">
      <struct type="naOcclusionPortalEntityMetadata"/>
    </array>
  </structdef>

  <structdef type="naOcclusionPathNodeMetadata" simple="true">
    <u32 name="m_Key"/>
    <array name="m_PathNodeChildList" type="atArray">
      <struct type="naOcclusionPathNodeChildMetadata"/>
    </array>
  </structdef>

  <structdef type="naOcclusionInteriorMetadata" simple="true">
    <array name="m_PortalInfoList" type="atArray">
      <struct type="naOcclusionPortalInfoMetadata"/>
    </array>
    <array name="m_PathNodeList" type="atArray">
      <struct type="naOcclusionPathNodeMetadata"/>
    </array>
  </structdef>

</ParserSchema>
