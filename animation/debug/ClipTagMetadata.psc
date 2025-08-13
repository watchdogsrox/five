<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="eClipTagAttributeType">
    <enumval name="kTypeFloat" />
    <enumval name="kTypeInt" />
    <enumval name="kTypeBool" />
    <enumval name="kTypeString" />
    <enumval name="kTypeVector3" />
    <enumval name="kTypeVector4" />
    <enumval name="kTypeQuaternion" />
    <enumval name="kTypeMatrix34" />
    <enumval name="kTypeData" />
    <enumval name="kTypeHashString" />
  </enumdef>

  <structdef type="ClipTagAttribute">
    <string name="m_Name" type="atString" />
    <enum name="m_Type" type="eClipTagAttributeType" />
    <bool name="m_Editable" />
  </structdef>

  <structdef type="ClipTagFloatAttribute" base="ClipTagAttribute">
    <float name="m_Value" />
    <float name="m_MinValue" />
    <float name="m_MaxValue" />
    <float name="m_DeltaValue" />
  </structdef>

  <structdef type="ClipTagIntAttribute" base="ClipTagAttribute">
    <int name="m_Value" />
    <int name="m_MinValue" />
    <int name="m_MaxValue" />
    <int name="m_DeltaValue" />
  </structdef>

  <structdef type="ClipTagBoolAttribute" base="ClipTagAttribute">
    <bool name="m_Value" />
  </structdef>

  <structdef type="ClipTagStringAttribute" base="ClipTagAttribute">
    <string name="m_Value" type="atString" />
  </structdef>

  <structdef type="ClipTagVector3Attribute" base="ClipTagAttribute">
    <Vector3 name="m_Value" />
    <float name="m_MinValue" />
    <float name="m_MaxValue" />
    <float name="m_DeltaValue" />
  </structdef>

  <structdef type="ClipTagVector4Attribute" base="ClipTagAttribute">
    <Vector4 name="m_Value" />
    <float name="m_MinValue" />
    <float name="m_MaxValue" />
    <float name="m_DeltaValue" />
  </structdef>

  <structdef type="ClipTagQuaternionAttribute" base="ClipTagAttribute">
    <Vector4 name="m_Value" />
  </structdef>

  <structdef type="ClipTagMatrix34Attribute" base="ClipTagAttribute">
    <Matrix34 name="m_Value" />
    <float name="m_MinValue" />
    <float name="m_MaxValue" />
    <float name="m_DeltaValue" />
  </structdef>

  <structdef type="ClipTagDataAttribute" base="ClipTagAttribute">
    <array name="m_Value" type="atArray">
      <u8 />
    </array>
  </structdef>

  <structdef type="ClipTagHashStringAttribute" base="ClipTagAttribute">
    <string name="m_Value" type="atHashString" />
  </structdef>

  <structdef type="ClipTagProperty">
    <string name="m_Name" type="atString" />
    <array name="m_Attributes" type="atArray">
      <pointer type="ClipTagAttribute" policy="owner" />
    </array>
  </structdef>

  <structdef type="ClipTagAction">
    <string name="m_Name" type="atString" />
    <struct name="m_Property" type="ClipTagProperty" />
  </structdef>

  <structdef type="ClipTagActionGroup">
    <string name="m_Name" type="atString" />
    <array name="m_Actions" type="atArray">
      <struct type="ClipTagAction" />
    </array>
  </structdef>

  <structdef type="ClipTagMetadataManager">
    <array name="m_ActionGroups" type="atArray">
      <struct type="ClipTagActionGroup" />
    </array>
  </structdef>

</ParserSchema>
