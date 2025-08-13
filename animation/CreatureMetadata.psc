<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">


  <structdef type="CShaderVariableComponent">
    <u32 name="m_pedcompID"/>
    <u32 name="m_maskID"/>
    <string name="m_shaderVariableHashString" type="atHashString" />
    <array name="m_tracks" type="atArray">
      <u8/>
    </array>
    <array name="m_ids" type="atArray">
      <u16/>
    </array>
    <array name="m_components" type="atArray">
      <u8/>
    </array>
  </structdef>
  
  <structdef type="CPedPropExpressionData">
    <u32 name="m_pedPropID"/>
    <s32 name="m_pedPropVarIndex"/>
    <u32 name="m_pedPropExpressionIndex"/>
    <array name="m_tracks" type="atArray">
      <u8/>
    </array>
    <array name="m_ids" type="atArray">
      <u16/>
    </array>
    <array name="m_types" type="atArray">
      <u8/>
    </array>
    <array name="m_components" type="atArray">
      <u8/>
    </array>
  </structdef>

  <structdef type="CPedCompExpressionData">
    <u32 name="m_pedCompID"/>
    <s32 name="m_pedCompVarIndex"/>
    <u32 name="m_pedCompExpressionIndex"/>
    <array name="m_tracks" type="atArray">
      <u8/>
    </array>
    <array name="m_ids" type="atArray">
      <u16/>
    </array>
    <array name="m_types" type="atArray">
      <u8/>
    </array>
    <array name="m_components" type="atArray">
      <u8/>
    </array>
  </structdef>
  
  <structdef type="CCreatureMetaData">
    <array name="m_shaderVariableComponents" type="atArray">
      <struct type="CShaderVariableComponent"/>
    </array>
    
    <array name="m_pedPropExpressions" type="atArray">
      <struct type="CPedPropExpressionData"/>
    </array>
    <array name="m_pedCompExpressions" type="atArray">
      <struct type="CPedCompExpressionData"/>
    </array>
    
  </structdef>

  <structdef type="ClipUserData">
    <array name="m_Users" type="atArray">
      <string type="atHashString" />
    </array>
  </structdef>

</ParserSchema>
