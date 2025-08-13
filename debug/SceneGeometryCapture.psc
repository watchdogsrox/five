<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CSceneGeometryCapture::Sphere">
    <string name="m_name" type="atString"/>
    <string name="m_areaCode" type="member" size="4"/>
    <Vec4V name="m_sphere" init="0,0,0,5"/>
    <float name="m_height" init="0"/>
    <Color32 name="m_colour" init="0xFF00FF00"/>
  </structdef>
  
  <structdef type="CSceneGeometryCapture">
    <string name="m_panoramaPath" type="atString"/>
    <string name="m_geometryPath" type="atString"/>
    <array name="m_spheres" type="atArray">
      <struct type="CSceneGeometryCapture::Sphere"/>
    </array>
  </structdef>
  
</ParserSchema>
