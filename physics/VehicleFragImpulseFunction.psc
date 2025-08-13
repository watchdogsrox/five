<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">
  
  <structdef type="VehicleFragImpulseRange">
    <float name="m_OutputMin"/>
    <float name="m_OutputMax"/>
    <float name="m_InputThreshold"/>
  </structdef>

  <structdef type="VehicleFragImpulseFunction">
    <array name="m_Ranges" type="atArray" addGroupWidget="false">
      <struct type="VehicleFragImpulseRange"/>
    </array>
  </structdef>

</ParserSchema>