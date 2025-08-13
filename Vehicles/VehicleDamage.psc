<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CVehicleDamage::Tunables" base="CTuning">
    <float name="m_AngleVectorsPointSameDir" min="0.0f" max="45.0f" step="1.0f" init="40.0f"/>
    <float name="m_MinFaultVelocityThreshold" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
    <float name="m_MinImpulseForEnduranceDamage" min="0.0f" max="100000.0f" step="100.0f" init="5000.0f"/>
    <float name="m_MaxImpulseForEnduranceDamage" min="0.0f" max="100000.0f" step="100.0f" init="15000.0f"/>
    <float name="m_MinEnduranceDamage" min="0.0f" max="1000.0f" step="0.1f" init="5.0f"/>
    <float name="m_MaxEnduranceDamage" min="0.0f" max="1000.0f" step="0.1f" init="50.0f"/>
    <float name="m_InstigatorDamageMultiplier" min="0.0f" max="10.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinImpulseForWantedStatus" min="0.0f" max="100000.0f" step="100.0f" init="7000.0f"/>
  </structdef>

</ParserSchema>
