<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CVfxVehicleInfo">

    <float name="m_mtlBangPtFxVehicleEvo" init="0.0f"/>
    <float name="m_mtlBangPtFxVehicleScale" init="1.0f"/>
    <float name="m_mtlScrapePtFxVehicleEvo" init="0.0f"/>
    <float name="m_mtlScrapePtFxVehicleScale" init="1.0f"/>

    <bool name="m_exhaustPtFxEnabled" init="false"/>
    <string name="m_exhaustPtFxName" type="atHashString"/>
    <float name="m_exhaustPtFxCutOffSpeed" init="20.0f"/>
    <float name="m_exhaustPtFxRange" init="20.0f"/>
    <float name="m_exhaustPtFxScale" init="1.0f"/>
    <float name="m_exhaustPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_exhaustPtFxSpeedEvoMax" init="3.0f"/>
    <float name="m_exhaustPtFxTempEvoMin" init="0.0f"/>
    <float name="m_exhaustPtFxTempEvoMax" init="12.0f"/>
    <bool name="m_exhaustPtFxThrottleEvoOnGearChange" init="false"/>

    <bool name="m_engineStartupPtFxEnabled" init="false"/>
    <string name="m_engineStartupPtFxName" type="atHashString"/>
    <float name="m_engineStartupPtFxRange" init="30.0f"/>
    
    <bool name="m_misfirePtFxEnabled" init="false"/>
    <string name="m_misfirePtFxName" type="atHashString"/>
    <float name="m_misfirePtFxRange" init="30.0f"/>

    <bool name="m_backfirePtFxEnabled" init="false"/>
    <string name="m_backfirePtFxName" type="atHashString"/>
    <float name="m_backfirePtFxRange" init="30.0f"/>

    <bool name="m_engineDamagePtFxEnabled" init="false"/>
    <bool name="m_engineDamagePtFxHasPanel" init="false"/>
    <bool name="m_engineDamagePtFxHasRotorEvo" init="false"/>
    <string name="m_engineDamagePtFxNoPanelName" type="atHashString"/>
    <string name="m_engineDamagePtFxPanelOpenName" type="atHashString"/>
    <string name="m_engineDamagePtFxPanelShutName" type="atHashString"/>
    <float name="m_engineDamagePtFxRange" init="20.0f"/>
    <float name="m_engineDamagePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_engineDamagePtFxSpeedEvoMax" init="10.0f"/>

    <bool name="m_overturnedSmokePtFxEnabled" init="false"/>
    <string name="m_overturnedSmokePtFxName" type="atHashString"/>
    <float name="m_overturnedSmokePtFxRange" init="50.0f"/>
    <float name="m_overturnedSmokePtFxAngleThresh" init="-0.7f"/>
    <float name="m_overturnedSmokePtFxSpeedThresh" init="1.0f"/>
    <float name="m_overturnedSmokePtFxEngineHealthThresh" init="400.0f"/>

    <bool name="m_leakPtFxEnabled" init="false"/>
    <string name="m_leakPtFxOilName" type="atHashString"/>
    <string name="m_leakPtFxPetrolName" type="atHashString"/>
    <float name="m_leakPtFxRange" init="60.0f"/>
    <float name="m_leakPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_leakPtFxSpeedEvoMax" init="25.0f"/>

    <int name="m_wheelGenericPtFxSet" init="1"/>
    <int name="m_wheelGenericDecalSet" init="1"/>
    <float name="m_wheelGenericRangeMult" init="1.0f"/>
    <bool name="m_wheelSkidmarkRearOnly" init="false"/>
    <float name="m_wheelSkidmarkSlipMult" init="1.0f"/>
    <float name="m_wheelSkidmarkPressureMult" init="1.0f"/>
    <float name="m_wheelFrictionPtFxFricMult" init="1.0f"/>
    <float name="m_wheelDisplacementPtFxDispMult" init="1.0f"/>
    <float name="m_wheelBurnoutPtFxFricMult" init="1.0f"/>
    <float name="m_wheelBurnoutPtFxTempMult" init="1.0f"/>

    <float name="m_wheelLowLodPtFxScale" init="1.0f"/>
    
    <string name="m_wheelPuncturePtFxName" type="atHashString"/>
    <float name="m_wheelPuncturePtFxRange" init="25.0f"/>

    <string name="m_wheelBurstPtFxName" type="atHashString"/>
    <float name="m_wheelBurstPtFxRange" init="60.0f"/>

    <string name="m_wheelFirePtFxName" type="atHashString"/>
    <float name="m_wheelFirePtFxRange" init="40.0f"/>
    <float name="m_wheelFirePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_wheelFirePtFxSpeedEvoMax" init="5.0f"/>

    <bool name="m_wreckedFirePtFxEnabled" init="false"/>
    <string name="m_wreckedFirePtFxName" type="atHashString"/>
    <float name="m_wreckedFirePtFxDurationMin" init="25.0f"/>
    <float name="m_wreckedFirePtFxDurationMax" init="45.0f"/>
    <float name="m_wreckedFirePtFxRadius" init="1.0f"/>

    <bool name="m_wreckedFire2PtFxEnabled" init="false"/>
    <string name="m_wreckedFire2PtFxName" type="atHashString"/>
    <float name="m_wreckedFire2PtFxDurationMin" init="25.0f"/>
    <float name="m_wreckedFire2PtFxDurationMax" init="45.0f"/>
    <float name="m_wreckedFire2PtFxRadius" init="1.0f"/>
    <bool name="m_wreckedFire2UseOverheatBone" init="false"/>
    <Vector3 name="m_wreckedFire2OffsetPos"/>

    <bool name="m_wreckedFire3PtFxEnabled" init="false"/>
    <string name="m_wreckedFire3PtFxName" type="atHashString"/>
    <float name="m_wreckedFire3PtFxDurationMin" init="25.0f"/>
    <float name="m_wreckedFire3PtFxDurationMax" init="45.0f"/>
    <float name="m_wreckedFire3PtFxRadius" init="1.0f"/>
    <bool name="m_wreckedFire3UseOverheatBone" init="false"/>
    <Vector3 name="m_wreckedFire3OffsetPos"/>

    <string name="m_petrolTankFirePtFxName" type="atHashString"/>
    <float name="m_petrolTankFirePtFxRange" init="60.0f"/>
    <float name="m_petrolTankFirePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_petrolTankFirePtFxSpeedEvoMax" init="10.0f"/>
    <float name="m_petrolTankFirePtFxRadius" init="1.0f"/>

    <bool name="m_boatEntryPtFxEnabled" init="false"/>
    <float name="m_boatEntryPtFxRange" init="40.0f"/>
    <string name="m_boatEntryPtFxName" type="atHashString"/>
    <float name="m_boatEntryPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_boatEntryPtFxSpeedEvoMax" init="20.0f"/>
    <float name="m_boatEntryPtFxScale" init="1.0f"/>

    <bool name="m_boatExitPtFxEnabled" init="false"/>
    <float name="m_boatExitPtFxRange" init="40.0f"/>
    <string name="m_boatExitPtFxName" type="atHashString"/>
    <float name="m_boatExitPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_boatExitPtFxSpeedEvoMax" init="20.0f"/>
    <float name="m_boatExitPtFxScale" init="1.0f"/>

    <bool name="m_boatBowPtFxEnabled" init="false"/>
    <float name="m_boatBowPtFxRange" init="40.0f"/>
    <string name="m_boatBowPtFxForwardName" type="atHashString"/>
    <string name="m_boatBowPtFxReverseName" type="atHashString"/>
    <string name="m_boatBowPtFxForwardMountedName" type="atHashString"/>
    <Vector3 name="m_boatBowPtFxForwardMountedOffset"/>
    <float name="m_boatBowPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_boatBowPtFxSpeedEvoMax" init="20.0f"/>
    <float name="m_boatBowPtFxKeelEvoMin" init="20.0f"/>
    <float name="m_boatBowPtFxKeelEvoMax" init="200.0f"/>
    <float name="m_boatBowPtFxScale" init="1.0f"/>
    <float name="m_boatBowPtFxReverseOffset" init="0.0f"/>
    
    <bool name="m_boatWashPtFxEnabled" init="false"/>
    <float name="m_boatWashPtFxRange" init="40.0f"/>
    <string name="m_boatWashPtFxName" type="atHashString"/>
    <float name="m_boatWashPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_boatWashPtFxSpeedEvoMax" init="20.0f"/>
    <float name="m_boatWashPtFxScale" init="1.0f"/>

    <bool name="m_boatPropellerPtFxEnabled" init="false"/>
    <float name="m_boatPropellerPtFxRange" init="40.0f"/>
    <string name="m_boatPropellerPtFxName" type="atHashString"/>
    <float name="m_boatPropellerPtFxForwardSpeedEvoMin" init="0.0f"/>
    <float name="m_boatPropellerPtFxForwardSpeedEvoMax" init="6.0f"/>
    <float name="m_boatPropellerPtFxBackwardSpeedEvoMin" init="0.0f"/>
    <float name="m_boatPropellerPtFxBackwardSpeedEvoMax" init="6.0f"/>
    <float name="m_boatPropellerPtFxDepthEvoMin" init="0.0f"/>
    <float name="m_boatPropellerPtFxDepthEvoMax" init="1.0f"/>
    <float name="m_boatPropellerPtFxScale" init="1.0f"/>

    <bool name="m_boatLowLodWakePtFxEnabled" init="false"/>
    <float name="m_boatLowLodWakePtFxRangeMin" init="40.0f"/>
    <float name="m_boatLowLodWakePtFxRangeMax" init="400.0f"/>
    <string name="m_boatLowLodWakePtFxName" type="atHashString"/>
    <float name="m_boatLowLodWakePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_boatLowLodWakePtFxSpeedEvoMax" init="10.0f"/>
    <float name="m_boatLowLodWakePtFxScale" init="1.0f"/>

    <bool name="m_planeAfterburnerPtFxEnabled" init="false"/>
    <string name="m_planeAfterburnerPtFxName" type="atHashString"/>
    <float name="m_planeAfterburnerPtFxRange" init="100.0f"/>
    <float name="m_planeAfterburnerPtFxScale" init="1.0f"/>

    <bool name="m_planeWingTipPtFxEnabled" init="false"/>
    <string name="m_planeWingTipPtFxName" type="atHashString"/>
    <float name="m_planeWingTipPtFxRange" init="100.0f"/>
    <float name="m_planeWingTipPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_planeWingTipPtFxSpeedEvoMax" init="1.0f"/>

    <bool name="m_planeDamageFirePtFxEnabled" init="false"/>
    <string name="m_planeDamageFirePtFxName" type="atHashString"/>
    <float name="m_planeDamageFirePtFxRange" init="100.0f"/>
    <float name="m_planeDamageFirePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_planeDamageFirePtFxSpeedEvoMax" init="20.0f"/>
    
    <bool name="m_planeGroundDisturbPtFxEnabled" init="false"/>
    <string name="m_planeGroundDisturbPtFxNameDefault" type="atHashString"/>
    <string name="m_planeGroundDisturbPtFxNameSand" type="atHashString"/>
    <string name="m_planeGroundDisturbPtFxNameDirt" type="atHashString"/>
    <string name="m_planeGroundDisturbPtFxNameWater" type="atHashString"/>
    <string name="m_planeGroundDisturbPtFxNameFoliage" type="atHashString"/>
    <float name="m_planeGroundDisturbPtFxRange" init="70.0f"/>
    <float name="m_planeGroundDisturbPtFxDist" init="20.0f"/>
    <float name="m_planeGroundDisturbPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_planeGroundDisturbPtFxSpeedEvoMax" init="20.0f"/>

    <bool name="m_aircraftSectionDamageSmokePtFxEnabled" init="false"/>
    <string name="m_aircraftSectionDamageSmokePtFxName" type="atHashString"/>
    <float name="m_aircraftSectionDamageSmokePtFxRange" init="100.0f"/>
    <float name="m_aircraftSectionDamageSmokePtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_aircraftSectionDamageSmokePtFxSpeedEvoMax" init="20.0f"/>

    <bool name="m_aircraftDownwashPtFxEnabled" init="false"/>
    <string name="m_aircraftDownwashPtFxNameDefault" type="atHashString"/>
    <string name="m_aircraftDownwashPtFxNameSand" type="atHashString"/>
    <string name="m_aircraftDownwashPtFxNameDirt" type="atHashString"/>
    <string name="m_aircraftDownwashPtFxNameWater" type="atHashString"/>
    <string name="m_aircraftDownwashPtFxNameFoliage" type="atHashString"/>
    <float name="m_aircraftDownwashPtFxRange" init="70.0f"/>
    <float name="m_aircraftDownwashPtFxDist" init="20.0f"/>
    <float name="m_aircraftDownwashPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_aircraftDownwashPtFxSpeedEvoMax" init="20.0f"/>

    <bool name="m_splashInPtFxEnabled" init="false"/>
    <float name="m_splashInPtFxRange" init="100.0f"/>
    <string name="m_splashInPtFxName" type="atHashString"/>
    <float name="m_splashInPtFxSizeEvoMax" init="5.0f"/>
    <float name="m_splashInPtFxSpeedDownwardThresh" init="2.0f"/>
    <float name="m_splashInPtFxSpeedLateralEvoMin" init="2.0f"/>
    <float name="m_splashInPtFxSpeedLateralEvoMax" init="15.0f"/>
    <float name="m_splashInPtFxSpeedDownwardEvoMin" init="2.0f"/>
    <float name="m_splashInPtFxSpeedDownwardEvoMax" init="15.0f"/>

    <bool name="m_splashOutPtFxEnabled" init="false"/>
    <float name="m_splashOutPtFxRange" init="50.0f"/>
    <string name="m_splashOutPtFxName" type="atHashString"/>
    <float name="m_splashOutPtFxSizeEvoMax" init="5.0f"/>
    <float name="m_splashOutPtFxSpeedLateralEvoMin" init="2.0f"/>
    <float name="m_splashOutPtFxSpeedLateralEvoMax" init="15.0f"/>
    <float name="m_splashOutPtFxSpeedUpwardEvoMin" init="2.0f"/>
    <float name="m_splashOutPtFxSpeedUpwardEvoMax" init="15.0f"/>

    <bool name="m_splashWadePtFxEnabled" init="false"/>
    <float name="m_splashWadePtFxRange" init="50.0f"/>
    <string name="m_splashWadePtFxName" type="atHashString"/>
    <float name="m_splashWadePtFxSizeEvoMax" init="5.0f"/>
    <float name="m_splashWadePtFxSpeedVehicleEvoMin" init="1.0f"/>
    <float name="m_splashWadePtFxSpeedVehicleEvoMax" init="15.0f"/>
    <float name="m_splashWadePtFxSpeedRiverEvoMin" init="1.0f"/>
    <float name="m_splashWadePtFxSpeedRiverEvoMax" init="15.0f"/>

    <bool name="m_splashTrailPtFxEnabled" init="false"/>
    <float name="m_splashTrailPtFxRange" init="50.0f"/>
    <string name="m_splashTrailPtFxName" type="atHashString"/>
    <float name="m_splashTrailPtFxSizeEvoMax" init="5.0f"/>
    <float name="m_splashTrailPtFxSpeedEvoMin" init="0.0f"/>
    <float name="m_splashTrailPtFxSpeedEvoMax" init="12.0f"/>

  </structdef>

  <structdef type="CVfxVehicleInfoMgr" onPreLoad="PreLoadCallback" onPostLoad="PostLoadCallback">
    <map name="m_vfxVehicleInfos" type="atBinaryMap" key="atHashString">
      <pointer type="CVfxVehicleInfo" policy="owner"/>
    </map>
  </structdef>

</ParserSchema>
