<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="MovieMeshInfo">
    <string name="m_ModelName" type="ConstString" ui_key="true"/>
    <string name="m_TextureName" type="ConstString" ui_key="true"/>
    <Vector3 name="m_TransformA" init="1.0f, 0.0f, 0.0f"/>
    <Vector3 name="m_TransformB" init="0.0f, 1.0f, 0.0f"/>
    <Vector3 name="m_TransformC" init="0.0f, 0.0f, 1.0f"/>
    <Vector3 name="m_TransformD" init="0.0f, 0.0f, 0.0f"/>
  </structdef>

  <structdef type="MovieMeshInfoList">
    <array name="m_Data" type="atArray">
      <struct type="MovieMeshInfo"/>
    </array>
    <array name="m_BoundingSpheres" type="atArray">
      <Vector4/>
    </array>
    <string name="m_InteriorName" type="ConstString" ui_key="true"/>
    <Vector3 name="m_InteriorPosition" init="0.0f, 0.0f, 0.0f"/>
    <Vector3 name="m_InteriorMaxPosition" init="0.0f, 0.0f, 0.0f"/>
    <bool name="m_UseInterior" init="true"/>
  </structdef>
  
</ParserSchema>
