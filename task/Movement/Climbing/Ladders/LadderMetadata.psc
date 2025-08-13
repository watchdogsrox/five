<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
              generate="class">

  <structdef type="CLadderMetadata">
    <string name="m_Name" type="atHashString"/>
    <string name="m_ClipSet" type="atHashString"/>
    <string name="m_FemaleClipSet" type="atHashString"/>
    <float name="m_RungSpacing" init="0.25f" min="0.1f" max="1.0f"/>
    <float name="m_Width" init="0.5f" min="0.1f" max="1.0f"/>
    <bool name="m_CanSlide" init="true"/>
    <bool name="m_CanMountBehind" init="true"/>
  </structdef>
  
</ParserSchema>