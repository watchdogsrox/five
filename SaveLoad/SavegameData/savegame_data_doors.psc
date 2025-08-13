<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CSaveDoors::DoorState" preserveNames="true">
    <Vector3 name="m_postion"/>
    <int name="m_doorEnumHash"/>
    <int name="m_modelInfoHash"/>
    <int name="m_doorState"/>
  </structdef>

  <structdef type="CSaveDoors" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <array name="m_savedDoorStates" type="atArray">
      <struct type="CSaveDoors::DoorState"/>
    </array>
  </structdef>

</ParserSchema>
