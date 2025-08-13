<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="sveNode" constructable="false" preserveNames="true">
  </structdef>

  <structdef type="sveBool" base="sveNode" preserveNames="true">
    <bool name="m_Value"/>
  </structdef>

  <structdef type="sveInt" base="sveNode" preserveNames="true">
    <int name="m_Value"/>
  </structdef>

  <structdef type="sveFloat" base="sveNode" preserveNames="true">
    <float name="m_Value"/>
  </structdef>

  <structdef type="sveString" base="sveNode" preserveNames="true">
    <string name="m_Value" type="atString"/>
  </structdef>

  <structdef type="sveVec3" base="sveNode" preserveNames="true">
    <Vec3V name="m_Value"/>
  </structdef>

  <structdef type="sveDict" base="sveNode" preserveNames="true">
    <map name="m_Values" key="atFinalHashString" type="atMap">
      <pointer type="sveNode" policy="owner"/>
    </map>
  </structdef>

  <structdef type="sveArray" base="sveNode" preserveNames="true">
    <array name="m_Values" type="atArray">
      <pointer type="sveNode" policy="owner"/>
    </array>
  </structdef>
  
</ParserSchema>
