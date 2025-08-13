<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <structdef type="NonFlyableArea" >
    <Vector4 name="m_rectXYWH" init="0.0f" description="X,Y is the bottom left corner, H and W are height and width" />
  </structdef>

  <structdef type="NonFlyableAreaArray">
    <array name="m_areas" type="atArray" >
      <struct type="NonFlyableArea" />
    </array>
  </structdef>

  <structdef type="sStatsMetadataTuning">
  <u32 name="m_FreefallTimeSinceLastDamage" init="1000"/>
  <u32 name="m_AwardVehicleJumpTime" init="5000"/>
  <u32 name="m_AwardParachuteJumpTime" init="60000"/>
  <float name="m_SPLargeAccidenThresold" init="300.0f"/>
  <float name="m_MPLargeAccidenThresold" init="300.0f"/>
  <float name="m_FreefallThresold" init="2.2f"/>
  <float name="m_AwardVehicleJumpDistanceA" init="5.0f"/>
  <float name="m_AwardVehicleJumpDistanceB" init="40.0f"/>
  <float name="m_AwardParachuteJumpDistanceA" init="40.0f"/>
  <float name="m_AwardParachuteJumpDistanceB" init="50.0f"/>
  <struct type="NonFlyableAreaArray" name="m_nonFlyableAreas" />
</structdef>
  
</ParserSchema>
