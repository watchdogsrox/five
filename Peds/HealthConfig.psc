<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
  
  <structdef type="CHealthConfigInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <float name="m_DefaultHealth" init="200" min="0" max="1000"/>
    <float name="m_DefaultArmour" init="0" min="0" max="1000"/>
    <float name="m_DefaultEndurance" init="0" min="0" max="1000"/>
    <float name="m_FatiguedHealthThreshold" init="100" min="0" max="1000"/>
    <float name="m_InjuredHealthThreshold" init="50" min="0" max="1000"/>
    <float name="m_DyingHealthThreshold" init="50" min="0" max="1000"/>
    <float name="m_HurtHealthThreshold" init="75" min="0" max="1000"/>
	  <float name="m_DogTakedownThreshold" init="75" min="0" max="1000"/>
    <float name="m_WritheFromBulletDamageTheshold" init="200" min="0" max="1000"/>
	  <bool name="m_MeleeCardinalFatalAttackCheck" init="false"/>
    <bool name="m_Invincible" init="false"/>
  </structdef>

  <structdef type="CHealthConfigInfoManager">
    <array name="m_aHealthConfig" type="atArray">
      <struct type="CHealthConfigInfo"/>
    </array>
    <pointer name="m_DefaultSet" type="CHealthConfigInfo" policy="external_named" toString="CHealthConfigInfoManager::GetInfoName" fromString="CHealthConfigInfoManager::GetInfoFromName"/>
  </structdef>

</ParserSchema>