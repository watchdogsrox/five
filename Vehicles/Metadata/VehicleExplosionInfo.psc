<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CVehicleExplosionLOD">
  <float name="m_Radius" min="0.0f" init="0.0f" />
  <float name="m_PartDeletionChance" min="0.0f" max="1.0f" init="0.0f" />
</structdef>

<structdef type="CExplosionData">
  <enum name="m_ExplosionTag" type="eExplosionTag" init="EXP_TAG_DONTCARE"/>
  <bool name="m_PositionAtPetrolTank" init="false"/>
  <bool name="m_PositionInBoundingBox" init="false"/>
  <int name="m_DelayTimeMs" init="0"/>
  <float name="m_Scale" init="1.0f"/>
  <Vector3 name="m_PositionOffset"/>
</structdef>

<const name="CVehicleExplosionInfo::sm_NumVehicleExplosionLODs" value="3"/>
<structdef type="CVehicleExplosionInfo">
  <string name="m_Name" type="atHashString"/>
  
  <array name="m_ExplosionData" type="atArray">
    <struct type="CExplosionData"/>
  </array>
  
  <float name="m_AdditionalPartVelocityMinAngle" min="0.0f" max="90.0f" init="0.0f" />
  <float name="m_AdditionalPartVelocityMaxAngle" min="0.0f" max="90.0f" init="0.0f" />
  <float name="m_AdditionalPartVelocityMinMagnitude" min="0.0f" max="100.0f" init="0.0f" />
  <float name="m_AdditionalPartVelocityMaxMagnitude" min="0.0f" max="100.0f" init="0.0f" />
  <array name="m_VehicleExplosionLODs" type="member" size="CVehicleExplosionInfo::sm_NumVehicleExplosionLODs">
    <struct type="CVehicleExplosionLOD"/>
  </array>
</structdef>
  
</ParserSchema>
