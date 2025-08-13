<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <enumdef type="eVfxPedLimbId">
    <enumval name="VFXPEDLIMBID_LEG_LEFT" value="0"/>
    <enumval name="VFXPEDLIMBID_LEG_RIGHT" />
    <enumval name="VFXPEDLIMBID_ARM_LEFT" />
    <enumval name="VFXPEDLIMBID_ARM_RIGHT" />
    <enumval name="VFXPEDLIMBID_SPINE" />
  </enumdef>

  <enumdef type="eVfxGroup">
    <enumval name="VFXGROUP_UNDEFINED" value="-1"/>
    <enumval name="VFXGROUP_VOID" value="0"/>
    <enumval name="VFXGROUP_GENERIC" />
    <enumval name="VFXGROUP_CONCRETE" />
    <enumval name="VFXGROUP_CONCRETE_DUSTY" />
    <enumval name="VFXGROUP_TARMAC" />
    <enumval name="VFXGROUP_TARMAC_BRITTLE" />
    <enumval name="VFXGROUP_STONE" />
    <enumval name="VFXGROUP_BRICK" />
    <enumval name="VFXGROUP_MARBLE" />
    <enumval name="VFXGROUP_PAVING" />
    <enumval name="VFXGROUP_SANDSTONE" />
    <enumval name="VFXGROUP_SANDSTONE_BRITTLE" />
    <enumval name="VFXGROUP_SAND_LOOSE" />
    <enumval name="VFXGROUP_SAND_COMPACT" />
    <enumval name="VFXGROUP_SAND_WET" />
    <enumval name="VFXGROUP_SAND_UNDERWATER" />
    <enumval name="VFXGROUP_SAND_DEEP" />
    <enumval name="VFXGROUP_SAND_WET_DEEP" />
    <enumval name="VFXGROUP_ICE" />
    <enumval name="VFXGROUP_SNOW_LOOSE" />
    <enumval name="VFXGROUP_SNOW_COMPACT" />
    <enumval name="VFXGROUP_GRAVEL" />
    <enumval name="VFXGROUP_GRAVEL_DEEP" />
    <enumval name="VFXGROUP_DIRT_DRY" />
    <enumval name="VFXGROUP_MUD_SOFT" />
    <enumval name="VFXGROUP_MUD_DEEP" />
    <enumval name="VFXGROUP_MUD_UNDERWATER" />
    <enumval name="VFXGROUP_CLAY" />
    <enumval name="VFXGROUP_GRASS" />
    <enumval name="VFXGROUP_GRASS_SHORT" />
    <enumval name="VFXGROUP_HAY" />
    <enumval name="VFXGROUP_BUSHES" />
    <enumval name="VFXGROUP_TREE_BARK" />
    <enumval name="VFXGROUP_LEAVES" />
    <enumval name="VFXGROUP_METAL" />
    <enumval name="VFXGROUP_WOOD" />
    <enumval name="VFXGROUP_WOOD_DUSTY" />
    <enumval name="VFXGROUP_WOOD_SPLINTER" />
    <enumval name="VFXGROUP_CERAMIC" />
    <enumval name="VFXGROUP_CARPET_FABRIC" />
    <enumval name="VFXGROUP_CARPET_FABRIC_DUSTY" />
    <enumval name="VFXGROUP_PLASTIC" />
    <enumval name="VFXGROUP_PLASTIC_HOLLOW" />
    <enumval name="VFXGROUP_RUBBER" />
    <enumval name="VFXGROUP_LINOLEUM" />
    <enumval name="VFXGROUP_PLASTER_BRITTLE" />
    <enumval name="VFXGROUP_CARDBOARD" />
    <enumval name="VFXGROUP_PAPER" />
    <enumval name="VFXGROUP_FOAM" />
    <enumval name="VFXGROUP_FEATHERS" />
    <enumval name="VFXGROUP_TVSCREEN" />
    <enumval name="VFXGROUP_GLASS" />
    <enumval name="VFXGROUP_GLASS_BULLETPROOF" />
    <enumval name="VFXGROUP_CAR_METAL" />
    <enumval name="VFXGROUP_CAR_PLASTIC" />
    <enumval name="VFXGROUP_CAR_GLASS" />
    <enumval name="VFXGROUP_PUDDLE" />
    <enumval name="VFXGROUP_LIQUID_WATER" />
    <enumval name="VFXGROUP_LIQUID_BLOOD" />
    <enumval name="VFXGROUP_LIQUID_OIL" />
    <enumval name="VFXGROUP_LIQUID_PETROL" />
    <enumval name="VFXGROUP_LIQUID_MUD" />
    <enumval name="VFXGROUP_FRESH_MEAT" />
    <enumval name="VFXGROUP_DRIED_MEAT" />
    <enumval name="VFXGROUP_PED_HEAD" />
    <enumval name="VFXGROUP_PED_TORSO" />
    <enumval name="VFXGROUP_PED_LIMB" />
    <enumval name="VFXGROUP_PED_FOOT" />
    <enumval name="VFXGROUP_PED_CAPSULE" />
  </enumdef>

  <structdef type="VfxPedFootDecalInfo">
    <int name="m_decalIdBare" init="0"/>
    <int name="m_decalIdShoe" init="0"/>
    <int name="m_decalIdBoot" init="0"/>
    <int name="m_decalIdHeel" init="0"/>
    <int name="m_decalIdFlipFlop" init="0"/>
    <int name="m_decalIdFlipper" init="0"/>
    <int name="m_decalRenderSettingIndexBare" init="0"/>
    <int name="m_decalRenderSettingCountBare" init="0"/>
    <int name="m_decalRenderSettingIndexShoe" init="0"/>
    <int name="m_decalRenderSettingCountShoe" init="0"/>
    <int name="m_decalRenderSettingIndexBoot" init="0"/>
    <int name="m_decalRenderSettingCountBoot" init="0"/>
    <int name="m_decalRenderSettingIndexHeel" init="0"/>
    <int name="m_decalRenderSettingCountHeel" init="0"/>
    <int name="m_decalRenderSettingIndexFlipFlop" init="0"/>
    <int name="m_decalRenderSettingCountFlipFlop" init="0"/>
    <int name="m_decalRenderSettingIndexFlipper" init="0"/>
    <int name="m_decalRenderSettingCountFlipper" init="0"/>
    <float name="m_decalWidth" init="0.0f"/>
    <float name="m_decalLength" init="0.0f"/>
    <u8 name="m_decalColR" init="255"/>
    <u8 name="m_decalColG" init="255"/>
    <u8 name="m_decalColB" init="255"/>
    <u8 name="m_decalWetColR" init="255"/>
    <u8 name="m_decalWetColG" init="255"/>
    <u8 name="m_decalWetColB" init="255"/>
    <float name="m_decalLife" init="-1.0f"/>
  </structdef>

  <structdef type="VfxPedFootPtFxInfo">
    <string name="m_ptFxDryName" type="atHashString"/>
    <string name="m_ptFxWetName" type="atHashString"/>
    <float name="m_ptFxWetThreshold" init="1.0f"/>
    <float name="m_ptFxScale" init="1.0f"/>
  </structdef>

  <structdef type="VfxPedFootInfo">
    <string name="m_decalInfoName" type="atHashString"/>
    <string name="m_ptFxInfoName" type="atHashString"/>
  </structdef>

  <structdef type="VfxPedWadeDecalInfo">
    <int name="m_decalId" init="0"/>
    <int name="m_decalRenderSettingIndex" init="0"/>
    <int name="m_decalRenderSettingCount" init="0"/>
    <float name="m_decalSizeMin" init="0.0f"/>
    <float name="m_decalSizeMax" init="0.0f"/>
    <u8 name="m_decalColR" init="255"/>
    <u8 name="m_decalColG" init="255"/>
    <u8 name="m_decalColB" init="255"/>
  </structdef>

  <structdef type="VfxPedWadePtFxInfo">
    <string name="m_ptFxName" type="atHashString"/>
    <float name="m_ptFxDepthEvoMin" init="0.1f"/>
    <float name="m_ptFxDepthEvoMax" init="0.5f"/>
    <float name="m_ptFxSpeedEvoMin" init="0.0f"/>
    <float name="m_ptFxSpeedEvoMax" init="1.0f"/>
  </structdef>

  <structdef type="VfxPedWadeInfo">
    <string name="m_decalInfoName" type="atHashString"/>
    <string name="m_ptFxInfoName" type="atHashString"/>
  </structdef>

  <structdef type="VfxPedVfxGroupInfo">
    <enum name="m_vfxGroup" type="eVfxGroup" init="VFXGROUP_UNDEFINED"/>
    <string name="m_footInfoName" type="atHashString"/>
    <string name="m_wadeInfoName" type="atHashString"/>
  </structdef>

  <structdef type="VfxPedBoneWadeInfo">
    <float name="m_sizeEvo" init="1.0f"/>
    <float name="m_depthMult" init="1.0f"/>
    <float name="m_speedMult" init="1.0f"/>
    <float name="m_widthRatio" init="1.0f"/>
  </structdef>

  <structdef type="VfxPedBoneWaterInfo">
    <float name="m_sampleSize" init="0.2f"/>
    <float name="m_boneSize" init="0.1f"/>
    <bool name="m_playerOnlyPtFx" init="false"/>
    <bool name="m_splashInPtFxEnabled" init="false"/>
    <bool name="m_splashOutPtFxEnabled" init="false"/>
    <bool name="m_splashWadePtFxEnabled" init="false"/>
    <bool name="m_splashTrailPtFxEnabled" init="false"/>
    <bool name="m_waterDripPtFxEnabled" init="false"/>
    <float name="m_splashInPtFxRange" init="60.0f"/>
    <float name="m_splashOutPtFxRange" init="15.0f"/>
    <float name="m_splashWadePtFxRange" init="25.0f"/>
    <float name="m_splashTrailPtFxRange" init="60.0f"/>
    <float name="m_waterDripPtFxRange" init="20.0f"/>
  </structdef>

  <structdef type="VfxPedSkeletonBoneInfo">
    <enum name="m_boneTagA" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <enum name="m_boneTagB" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <enum name="m_limbId" type="eVfxPedLimbId" init="VFXPEDLIMBID_LEG_LEFT"/>
    <string name="m_boneWadeInfoName" type="atHashString"/>
    <string name="m_boneWaterInfoName" type="atHashString"/>
  </structdef>

  <structdef type="VfxPedFireInfo">
    <string name="m_ptFxName" type="atHashString" ui_key="true"/>
    <enum name="m_boneTagA" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <enum name="m_boneTagB" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <int name="m_limbId" init="0"/>
  </structdef>

  <structdef type="VfxPedParachutePedTrailInfo">
    <string name="m_ptFxName" type="atHashString" ui_key="true"/>
    <enum name="m_boneTagA" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <enum name="m_boneTagB" type="eAnimBoneTag" init="BONETAG_INVALID"/>
  </structdef>

  <structdef type="CVfxPedInfo">
    <array name="m_vfxGroupInfos" type="atArray">
      <struct type="VfxPedVfxGroupInfo"/>
    </array>
    <array name="m_skeletonBoneInfos" type="atArray">
      <struct type="VfxPedSkeletonBoneInfo"/>
    </array>
    <float name="m_footVfxRange" init="15.0f"/>
    <bool name="m_footPtFxEnabled" init="false"/>
    <float name="m_footPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_footPtFxSpeedEvoMax" init="5.0f"/>
    <bool name="m_footDecalsEnabled" init="false"/>
    <bool name="m_wadePtFxEnabled" init="false"/>
    <float name="m_wadePtFxRange" init="40.0f"/>
    <bool name="m_wadeDecalsEnabled" init="false"/>
    <float name="m_wadeDecalsRange" init="15.0f"/>
    <bool name="m_bloodPoolVfxEnabled" init="false"/>
    <bool name="m_bloodEntryPtFxEnabled" init="false"/>
    <float name="m_bloodEntryPtFxRange" init="60.0f"/>
    <float name="m_bloodEntryPtFxScale" init="1.0f"/>
    <bool name="m_bloodEntryDecalEnabled" init="false"/>
    <float name="m_bloodEntryDecalRange" init="30.0f"/>
    <bool name="m_bloodExitPtFxEnabled" init="false"/>
    <float name="m_bloodExitPtFxRange" init="60.0f"/>
    <float name="m_bloodExitPtFxScale" init="1.0f"/>
    <bool name="m_bloodExitDecalEnabled" init="false"/>
    <float name="m_bloodExitDecalRange" init="30.0f"/>
    <float name="m_bloodExitProbeRange" init="10.0f"/>
    <float name="m_bloodExitPosDistHead" init="0.15f"/>
    <float name="m_bloodExitPosDistTorso" init="0.25f"/>
    <float name="m_bloodExitPosDistLimb" init="0.08f"/>
    <float name="m_bloodExitPosDistFoot" init="0.05f"/>
    <float name="m_bloodPoolVfxSizeMult" init="1.0f"/>
    <bool name="m_shotPtFxEnabled" init="false"/>
    <float name="m_shotPtFxRange" init="40.0f"/>
    <string name="m_shotPtFxName" type="atHashString"/>
    <bool name="m_firePtFxEnabled" init="false"/>
    <array name="m_firePtFxInfos" type="atArray">
      <struct type="VfxPedFireInfo"/>
    </array>
    <float name="m_firePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_firePtFxSpeedEvoMax" init="4.0f"/>
    <bool name="m_smoulderPtFxEnabled" init="false"/>
    <float name="m_smoulderPtFxRange" init="30.0f"/>
    <string name="m_smoulderPtFxName" type="atHashString"/>
    <enum name="m_smoulderPtFxBoneTagA" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <enum name="m_smoulderPtFxBoneTagB" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <bool name="m_breathPtFxEnabled" init="false"/>
    <float name="m_breathPtFxRange" init="20.0f"/>
    <string name="m_breathPtFxName" type="atHashString"/>
    <float name="m_breathPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_breathPtFxSpeedEvoMax" init="5.0f"/>
    <float name="m_breathPtFxTempEvoMin" init="2.0f"/>
    <float name="m_breathPtFxTempEvoMax" init="9.0f"/>
    <float name="m_breathPtFxRateEvoMin" init="0.0f"/>
    <float name="m_breathPtFxRateEvoMax" init="4.0f"/>
    <bool name="m_breathWaterPtFxEnabled" init="false"/>
    <float name="m_breathWaterPtFxRange" init="30.0f"/>
    <string name="m_breathWaterPtFxName" type="atHashString"/>
    <float name="m_breathWaterPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_breathWaterPtFxSpeedEvoMax" init="5.0f"/>
    <float name="m_breathWaterPtFxDepthEvoMin" init="2.0f"/>
    <float name="m_breathWaterPtFxDepthEvoMax" init="0.0f"/>
    <bool name="m_breathScubaPtFxEnabled" init="false"/>
    <float name="m_breathScubaPtFxRange" init="30.0f"/>
    <string name="m_breathScubaPtFxName" type="atHashString"/>
    <float name="m_breathScubaPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_breathScubaPtFxSpeedEvoMax" init="5.0f"/>
    <float name="m_breathScubaPtFxDepthEvoMin" init="2.0f"/>
    <float name="m_breathScubaPtFxDepthEvoMax" init="0.0f"/>
    <bool name="m_splashVfxEnabled" init="false"/>
    <float name="m_splashVfxRange" init="80.0f"/>
    <bool name="m_splashLodPtFxEnabled" init="false"/>
    <string name="m_splashLodPtFxName" type="atHashString"/>
    <float name="m_splashLodVfxRange" init="80.0f"/>
    <float name="m_splashLodPtFxSpeedEvoMin" init="2.0f"/>
    <float name="m_splashLodPtFxSpeedEvoMax" init="30.0f"/>
    <bool name="m_splashEntryPtFxEnabled" init="false"/>
    <string name="m_splashEntryPtFxName" type="atHashString"/>
    <float name="m_splashEntryVfxRange" init="80.0f"/>
    <float name="m_splashEntryPtFxSpeedEvoMin" init="2.0f"/>
    <float name="m_splashEntryPtFxSpeedEvoMax" init="30.0f"/>
    <string name="m_splashInPtFxName" type="atHashString"/>
    <float name="m_splashInPtFxSpeedThresh" init="0.2f"/>
    <float name="m_splashInPtFxSpeedEvoMin" init="0.2f"/>
    <float name="m_splashInPtFxSpeedEvoMax" init="5.0f"/>
    <string name="m_splashOutPtFxName" type="atHashString"/>
    <float name="m_splashOutPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_splashOutPtFxSpeedEvoMax" init="5.0f"/>
    <string name="m_splashWadePtFxName" type="atHashString"/>
    <float name="m_splashWadePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_splashWadePtFxSpeedEvoMax" init="5.0f"/>
    <string name="m_splashTrailPtFxName" type="atHashString"/>
    <float name="m_splashTrailPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_splashTrailPtFxSpeedEvoMax" init="12.0f"/>
    <float name="m_splashTrailPtFxDepthEvoMin" init="0.0f"/>
    <float name="m_splashTrailPtFxDepthEvoMax" init="5.0f"/>
    <string name="m_waterDripPtFxName" type="atHashString"/>
    <float name="m_waterDripPtFxTimer" init="2.0f"/>
    <bool name="m_underwaterDisturbPtFxEnabled" init="false"/>
    <string name="m_underwaterDisturbPtFxNameDefault" type="atHashString"/>
    <string name="m_underwaterDisturbPtFxNameSand" type="atHashString"/>
    <string name="m_underwaterDisturbPtFxNameDirt" type="atHashString"/>
    <string name="m_underwaterDisturbPtFxNameWater" type="atHashString"/>
    <string name="m_underwaterDisturbPtFxNameFoliage" type="atHashString"/>
    <float name="m_underwaterDisturbPtFxRange" init="20.0f"/>
    <float name="m_underwaterDisturbPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_underwaterDisturbPtFxSpeedEvoMax" init="2.0f"/>
    <bool name="m_parachuteDeployPtFxEnabled" init="false"/>
    <float name="m_parachuteDeployPtFxRange" init="200.0f"/>
    <string name="m_parachuteDeployPtFxName" type="atHashString"/>
    <enum name="m_parachuteDeployPtFxBoneTag" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <bool name="m_parachuteSmokePtFxEnabled" init="false"/>
    <float name="m_parachuteSmokePtFxRange" init="200.0f"/>
    <string name="m_parachuteSmokePtFxName" type="atHashString"/>
    <enum name="m_parachuteSmokePtFxBoneTag" type="eAnimBoneTag" init="BONETAG_INVALID"/>
    <bool name="m_parachutePedTrailPtFxEnabled" init="false"/>
    <array name="m_parachutePedTrailPtFxInfos" type="atArray">
      <struct type="VfxPedParachutePedTrailInfo"/>
    </array>
    <bool name="m_parachuteCanopyTrailPtFxEnabled" init="false"/>
    <float name="m_parachuteCanopyTrailPtFxRange" init="200.0f"/>
    <string name="m_parachuteCanopyTrailPtFxName" type="atHashString"/>
  </structdef>

  <structdef type="CVfxPedInfoMgr" onPreLoad="PreLoadCallback" onPostLoad="PostLoadCallback">
    <map name="m_vfxPedFootDecalInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedFootDecalInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedFootPtFxInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedFootPtFxInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedFootInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedFootInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedWadeDecalInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedWadeDecalInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedWadePtFxInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedWadePtFxInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedWadeInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedWadeInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedBoneWadeInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedBoneWadeInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedBoneWaterInfos" type="atBinaryMap" key="atHashString">
      <pointer type="VfxPedBoneWaterInfo" policy="owner"/>
    </map>
    <map name="m_vfxPedInfos" type="atBinaryMap" key="atHashString">
      <pointer type="CVfxPedInfo" policy="owner"/>
    </map>
  </structdef>

</ParserSchema>
