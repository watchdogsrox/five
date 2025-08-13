<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="AnimSceneEntityLocationData" >
    <Vec3V name="m_startPos"/>
    <Vec4V name="m_startRot"/>
    <float name="m_startTime"/>
    <Vec3V name="m_endPos"/>
    <Vec4V name="m_endRot"/>
    <float name="m_endTime"/>
  </structdef>

  <structdef type="CAnimSceneEntity" onPostAddWidgets="OnPostAddWidgets" onPreAddWidgets="OnPreAddWidgets">
    <string name="m_Id" type="atHashString" hideWidgets="true"></string>
    <map name="m_locationData" type="atBinaryMap" key="atHashString" hideWidgets="true">
      <pointer type="AnimSceneEntityLocationData" policy="owner" />
    </map>
    <pad bytes="4" platform="32bit"/>    <!-- m_pScene-->
    <pad bytes="12" platform="64bit"/>
  </structdef>

</ParserSchema>