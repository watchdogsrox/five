<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CIKDeltaLimits">
    <array name="m_DeltaLimits" type="atFixedArray" size="5">
      <array type="atFixedArray" size="2">
        <Vec3V/>
      </array>
    </array>
  </structdef>
  
	<structdef type="CPedIKSettingsInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
		<bitset name="m_IKSolversDisabled" type="fixed" values="IKManagerSolverTypes::Flags"/>

    <struct type="CIKDeltaLimits" name="m_TorsoDeltaLimits" />
    <struct type="CIKDeltaLimits" name="m_HeadDeltaLimits" />
    <struct type="CIKDeltaLimits" name="m_NeckDeltaLimits" />
	</structdef>

  <structdef type="CPedIKSettingsInfoManager">
    <array name="m_aPedIKSettings" type="atArray">
      <struct type="CPedIKSettingsInfo"/>
    </array>
    <pointer name="m_DefaultSet" type="CPedIKSettingsInfo" policy="external_named" toString="CPedIKSettingsInfoManager::GetInfoName" fromString="CPedIKSettingsInfoManager::GetInfoFromName"/>
  </structdef>

</ParserSchema>
