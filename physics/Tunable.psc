<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CTunableObjectEntry">
	<string name="m_objectName" type="atHashString" />
  <float name="m_objectMass" init="-1.0" />
  <Vector3 name="m_objectLinearSpeedDamping" init="-1.0f, -1.0f, -1.0f" />
  <Vector3 name="m_objectLinearVelocityDamping" init="-1.0f, -1.0f, -1.0f" />
  <Vector3 name="m_objectLinearVelocitySquaredDamping" init="-1.0f, -1.0f, -1.0f" />
  <Vector3 name="m_objectAngularSpeedDamping" init="-1.0f, -1.0f, -1.0f" />
  <Vector3 name="m_objectAngularVelocityDamping" init="-1.0f, -1.0f, -1.0f" />
  <Vector3 name="m_objectAngularVelocitySquaredDamping" init="-1.0f, -1.0f, -1.0f" />
  <Vector3 name="m_objectCentreOfMassOffset" init="0.0f, 0.0f, 0.0f" />

  <!-- The dimensions of the angled area within the prop used to determine when the forks are in a good position for attaching -->
  <Vector3 name="m_forkliftAttachAngledAreaDims" init="1.8f, 1.16f, 0.18f" />
  <Vector3 name="m_forkliftAttachAngledAreaOrigin" init="0.0f, 0.0f, 0.0f" />
  <bool name="m_forkliftCanAttachWhenFragBroken" init="false" />
  
  <float name="m_lowLodBuoyancyDistance" init="-1.0" />
  <float name="m_lowLodBuoyancyHeightOffset" init="0.0" />
  <float name="m_buoyancyFactor" init="1.0" />
  <float name="m_buoyancyDragFactor" init="1.0" />
  <int name="m_boundIndexForWaterSamples" init="-1" />
  <bool name="m_lowLodBuoyancyNoCollision" init="false" />

  <float name="m_maxHealth" init="-1.0f" />

  <float name="m_knockOffBikeImpulseScalar" init="1.0f" />

  <!-- Glass frame values (use bgBreakable::eFrameFlags) - see breakable glass for details -->
  <int name="m_glassFrameFlags" init="15" />
  <bool name="m_bCanBePickedUpByCargobob" init="true" />
  <bool name="m_bUnreachableByMeleeNavigation" init="false" />

  <bool name="m_notDamagedByFlames" init="false" />

  <bool name="m_collidesWithLowLodVehicleChassis" init="false" />  

  <bool name="m_usePositionForLockOnTarget" init="false" />  

</structdef>

<structdef type="CTunableObjectManager">
	<map name="m_TunableObjects" type="atBinaryMap" key="atHashString">
		<struct type="CTunableObjectEntry" />
	</map>
</structdef>

</ParserSchema>
