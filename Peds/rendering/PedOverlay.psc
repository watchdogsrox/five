<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="ePedDecorationZone">
    <enumval name="ZONE_TORSO" value="0"/>
    <enumval name="ZONE_HEAD"/>
    <enumval name="ZONE_LEFT_ARM"/>
    <enumval name="ZONE_RIGHT_ARM"/>
    <enumval name="ZONE_LEFT_LEG"/>
    <enumval name="ZONE_RIGHT_LEG"/>
    <enumval name="ZONE_MEDALS"/>
    <enumval name="ZONE_INVALID"/>
  </enumdef>

  <enumdef type="ePedDecorationType">
    <enumval name="TYPE_TATTOO" value="0"/>
    <enumval name="TYPE_BADGE" value="1"/>
    <enumval name="TYPE_MEDAL" value="2"/>
    <enumval name="TYPE_INVALID"/>
  </enumdef>

  <enumdef type="Gender">
    <enumval name="GENDER_MALE" value="0"/>
    <enumval name="GENDER_FEMALE" value="1"/>
    <enumval name="GENDER_DONTCARE" value="2"/>
  </enumdef>
  
  <structdef type="PedDecorationPreset">
    <Vector2 name="m_uvPos" init="0.0f, 0.0f"/>
    <Vector2 name ="m_scale" init="1.0f, 1.0f"/>
    <float name ="m_rotation" init="0.0f" min="-180.0f" max="180.0f"/>
    <string name="m_nameHash" type="atHashString" description="Name of the Preset"/>
    <string name="m_txdHash" type="atHashString" description="Name of the Texture Dictionary"/>
    <string name="m_txtHash" type="atHashString" description="Name of the Texture"/>
    <enum name="m_zone" type="ePedDecorationZone" init="ZONE_INVALID"/>
    <enum name="m_type" type="ePedDecorationType" init="TYPE_INVALID"/>
    <string name="m_faction" type="atHashString" description="human readable version of the factions name"/>
    <string name="m_garment" type="atHashString" description="human readable version of the gament name"/>
    <enum name="m_gender" type="Gender" init="GENDER_DONTCARE" />
    <string name="m_award" type="atHashString" description="human readable version of the reward name"/>
    <string name="m_awardLevel" type="atHashString" description="human readable version of the reward level"/>
    <bool name="m_usesTintColor" init="false"/>
  </structdef>

  <structdef type="PedDecorationCollection">
	  <bool name="m_bRequiredForSync" init="false"/>
    <array name="m_presets" type="atArray">
      <struct type="PedDecorationPreset"/>
    </array>
    <string name="m_nameHash" type="atFinalHashString"/>
    <array name="m_medalLocations" type="atRangeArray" size="16">
      <Vector2 init="0.0f, 0.0f" min="-1.0f" max="1.0f"/>
    </array>
    <Vector2 name="m_medalScale" init="1.0f, 1.0f" min="-1.0f" max="1.0f"/>
  </structdef>

</ParserSchema>
