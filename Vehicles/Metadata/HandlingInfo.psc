<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CHandlingDataMgr">
    <array name="m_HandlingData" type="atArray">
      <pointer type="CHandlingData" policy="owner"/>
    </array>
  </structdef>
  
  <structdef type="CHandlingData" base="CHandlingObject">
    <string name="m_handlingName" type="atHashString" ui_key="true"/>
    <float name="m_fMass" min="0.0" max="100000.0"/>
    <float name="m_fInitialDragCoeff" min="0.0" max="1000.0"/>
    <float name="m_fPercentSubmerged" min="0.0" max="100.0"/>
    <Vec3V name="m_vecCentreOfMassOffset" description="Center of Mass"/>
    <Vec3V name="m_vecInertiaMultiplier" description="Inertia override"/>

    <float name="m_fDriveBiasFront" min="0.0" max="1000.0"/>
    <u8 name="m_nInitialDriveGears" min="0" max="1000"/>
    <float name="m_fInitialDriveForce" min="0.0" max="1000.0"/>
    <float name="m_fDriveInertia" min="0.0" max="1000.0"/>
    <float name="m_fClutchChangeRateScaleUpShift" min="0.0" max="1000.0"/>
    <float name="m_fClutchChangeRateScaleDownShift" min="0.0" max="1000.0"/>
    <float name="m_fInitialDriveMaxFlatVel" min="0.0" max="1000.0"/>

    <float name="m_fBrakeForce" min="0.0" max="1000.0"/>
    <float name="m_fBrakeBiasFront" min="0" max="1000"/>
    <float name="m_fHandBrakeForce" min="0.0" max="1000.0"/>
    <float name="m_fSteeringLock" min="0.0" max="1000.0"/>

    <float name="m_fTractionCurveMax" min="0.0" max="1000.0"/>
    <float name="m_fTractionCurveMin" min="0.0" max="1000.0"/>
    <float name="m_fTractionCurveLateral" min="0.0" max="1000.0"/>
    <float name="m_fTractionSpringDeltaMax" min="0.0" max="1000.0"/>
    <float name="m_fLowSpeedTractionLossMult" min="0.0" max="1000.0"/>
    <float name="m_fCamberStiffnesss" min="0.0" max="1000.0"/>
    <float name="m_fTractionBiasFront" min="0.0" max="1000.0"/>
    <float name="m_fTractionLossMult" min="0.0" max="1000.0"/>
    
    <float name="m_fSuspensionForce" min="0.0" max="1000.0"/>
    <float name="m_fSuspensionCompDamp" min="0.0" max="1000.0"/>
    <float name="m_fSuspensionReboundDamp" min="0.0" max="1000.0"/>
    <float name="m_fSuspensionUpperLimit" min="0.0" max="1000.0"/>
    <float name="m_fSuspensionLowerLimit" min="0.0" max="1000.0"/>
    <float name="m_fSuspensionRaise" min="0.0" max="1000.0"/>
    <float name="m_fSuspensionBiasFront" min="0.0" max="1000.0"/>
    <float name="m_fAntiRollBarForce" min="0.0" max="1000.0"/>
    <float name="m_fAntiRollBarBiasFront" min="0.0" max="1000.0"/>
    <float name="m_fRollCentreHeightFront" min="0.0" max="1000.0"/>
    <float name="m_fRollCentreHeightRear" min="0.0" max="1000.0"/>

    <float name="m_fCollisionDamageMult" min="0.0" max="1000.0"/>
    <float name="m_fWeaponDamageMult" min="0.0" max="1000.0"/>
    <float name="m_fDeformationDamageMult" min="0.0" max="1000.0"/>
    <float name="m_fEngineDamageMult" min="0.0" max="1000.0"/>
    <float name="m_fPetrolTankVolume" min="0.0" max="1000.0"/>
    <float name="m_fOilVolume" min="0.0" max="1000.0"/>
    <float name="m_fPetrolConsumptionRate" min="0.0" max="1000.0" init="0.5"/>

    <float name="m_fSeatOffsetDistX" min="0.0" max="1000.0"/>
    <float name="m_fSeatOffsetDistY" min="0.0" max="1000.0"/>
    <float name="m_fSeatOffsetDistZ" min="0.0" max="1000.0"/>
    <u32 name="m_nMonetaryValue" min="0" max="1000000"/>
    
    <string name="m_strModelFlags" type="atFinalHashString"/>
    <string name="m_strHandlingFlags" type="atFinalHashString"/>
    <string name="m_strDamageFlags" type="atFinalHashString"/>
    <!--u32 name="m_ModelFlags" min="0" max="4294967294"/-->
    <!--u32 name="m_HandlingFlags" min="0" max="4294967294"/-->
    <!--u32 name="m_DamageFlags" min="0" max="4294967294"/-->
    
    <!--bitset name="m_ModelFlags" type="fixed" numBits="32" />
    <bitset name="m_HandlingFlags" type="fixed" numBits="32" />
    <bitset name="m_DamageFlags" type="fixed" numBits="32" /-->
    
    <string name="m_AIHandling" type="atHashString"/>

    <array name="m_SubHandlingData" type="atArray">
      <pointer type="CBaseSubHandlingData" policy="owner"/>
    </array>

    <float name="m_fWeaponDamageScaledToVehHealthMult" init="0.5" min="0.0" max="10.0"/>
    <float name="m_fDownforceModifier" min="0.0" max="100.0"/>
    <float name="m_fPopUpLightRotation" init="0.0" min="-90.0" max="90.0"/>
    <float name="m_fRocketBoostCapacity" init="1.25" min="0.0" max="1000.0"/>
    <float name="m_fBoostMaxSpeed" init="70.0" min="0.0" max="150.0"/>
  </structdef>
  
  <structdef type="CHandlingObject"/>
  <structdef type="CBaseSubHandlingData" base="CHandlingObject"/>
  
  <structdef type="CAdvancedData">
    <s32 name="m_Slot" init="-1"/>
    <s32 name="m_Index"/>
    <float name="m_Value"/>
  </structdef>
  
  
  <structdef type="CBoatHandlingData" base="CBaseSubHandlingData">
    <float name="m_fBoxFrontMult" min="0.0" max="1000.0"/>
    <float name="m_fBoxRearMult" min="0.0" max="1000.0"/>
    <float name="m_fBoxSideMult" min="0.0" max="1000.0"/>
    <float name="m_fSampleTop" min="0.0" max="1000.0"/>
    <float name="m_fSampleBottom" min="0.0" max="1000.0"/>
    <float name="m_fSampleBottomTestCorrection" init="0.0" min="0.0" max="1.0"/>
    <float name="m_fAquaplaneForce" min="0.0" max="1000.0"/>
    <float name="m_fAquaplanePushWaterMult" min="0.0" max="1000.0"/>
    <float name="m_fAquaplanePushWaterCap" min="0.0" max="1000.0"/>
    <float name="m_fAquaplanePushWaterApply" min="0.0" max="1000.0"/>
    <float name="m_fRudderForce" min="0.0" max="1000.0"/>
    <float name="m_fRudderOffsetSubmerge" min="-1000.0" max="1000.0"/>
    <float name="m_fRudderOffsetForce" min="-1000.0" max="1000.0"/>
    <float name="m_fRudderOffsetForceZMult" min="-1000.0" max="1000.0"/>
    <float name="m_fWaveAudioMult" min="0.0" max="1000.0"/>
    <Vec3V name="m_vecMoveResistance" description="Move resistance"/>
    <Vec3V name="m_vecTurnResistance"  description="Turn resistance" />
    <float name="m_fLook_L_R_CamHeight" min="0.0" max="1000.0"/>
    <float name="m_fDragCoefficient" min="0.0" max="1000.0"/>
    <float name="m_fKeelSphereSize" min="0.0" max="1000.0"/>
    <float name="m_fPropRadius" min="0.0" max="1000.0"/>
    <float name="m_fLowLodAngOffset" min="-1000.0" max="1000.0"/>
    <float name="m_fLowLodDraughtOffset" min="-1000.0" max="1000.0"/>
    <float name="m_fImpellerOffset" min="-1000.0" max="1000.0"/>
    <float name="m_fImpellerForceMult" min="-1000.0" max="1000.0"/>
    <float name="m_fDinghySphereBuoyConst" min="-1000.0" max="1000.0"/>
    <float name="m_fProwRaiseMult" min="-1000.0" max="1000.0"/>
    <float name="m_fDeepWaterSampleBuoyancyMult" init="1.0" min="0.0" max="1000.0"/>
    <float name="m_fTransmissionMultiplier" init="1.0" min="0.0" max="100.0"/>
    <float name="m_fTractionMultiplier" init="1.0" min="0.0" max="100.0"/>
  </structdef>

  <structdef type="CSeaPlaneHandlingData" base="CBaseSubHandlingData">
 	  <int name="m_fLeftPontoonComponentId" min="0" max="65535"/>
	  <int name="m_fRightPontoonComponentId" min="0" max="65535"/>
    <float name="m_fPontoonBuoyConst" min="-1000.0" max="1000.0"/>
    <float name="m_fPontoonSampleSizeFront" min="-1000.0" max="1000.0"/>
    <float name="m_fPontoonSampleSizeMiddle" min="-1000.0" max="1000.0"/>
    <float name="m_fPontoonSampleSizeRear" min="-1000.0" max="1000.0"/>
    <float name="m_fPontoonLengthFractionForSamples" min="-1000.0" max="1000.0"/>
    <float name="m_fPontoonDragCoefficient" min="0.0" max="1000.0"/>
    <float name="m_fPontoonVerticalDampingCoefficientUp" min="0.0" max="1000.0"/>
    <float name="m_fPontoonVerticalDampingCoefficientDown" min="0.0" max="1000.0"/>
    <float name="m_fKeelSphereSize" min="0.0" max="1000.0"/>
  </structdef>

  <enumdef type="eHandlingType">
    <enumval name="HANDLING_TYPE_BIKE"/>
    <enumval name="HANDLING_TYPE_FLYING"/>
    <enumval name="HANDLING_TYPE_VERTICAL_FLYING"/>
    <enumval name="HANDLING_TYPE_BOAT"/>
    <enumval name="HANDLING_TYPE_SEAPLANE"/>
    <enumval name="HANDLING_TYPE_SUBMARINE"/>
    <enumval name="HANDLING_TYPE_TRAIN"/>
    <enumval name="HANDLING_TYPE_TRAILER"/>
    <enumval name="HANDLING_TYPE_CAR"/>
    <enumval name="HANDLING_TYPE_WEAPON"/>
    <enumval name="HANDLING_TYPE_MAX_TYPES"/>
  </enumdef>
  
  <structdef type="CFlyingHandlingData" base="CBaseSubHandlingData">
    <float name="m_fThrust" min="-1000.0" max="1000.0"/>
    <float name="m_fInitialThrust" init="0.0" min="0.0" max="0.0"/>
    <float name="m_fThrustFallOff" min="-1000.0" max="1000.0"/>
    <float name="m_fInitialThrustFallOff" init="0.0" min="0.0" max="0.0"/>
    <float name="m_fThrustVectoring" min="-1000.0" max="1000.0"/>
    <float name="m_fYawMult" min="-1000.0" max="1000.0"/>
    <float name="m_fInitialYawMult" init="0.0" min="0.0" max="0.0"/>
    <float name="m_fYawStabilise" min="-1000.0" max="1000.0"/>
    <float name="m_fSideSlipMult" min="-1000.0" max="1000.0"/>
    <float name="m_fRollMult" min="-1000.0" max="1000.0"/>
    <float name="m_fInitialRollMult" init="0.0" min="0.0" max="0.0"/>
    <float name="m_fRollStabilise" min="-1000.0" max="1000.0"/>
    <float name="m_fPitchMult" min="-1000.0" max="1000.0"/>
    <float name="m_fInitialPitchMult" init="0.0" min="0.0" max="0.0"/>
    <float name="m_fPitchStabilise" min="-1000.0" max="1000.0"/>
    <float name="m_fFormLiftMult" min="-1000.0" max="1000.0"/>
    <float name="m_fAttackLiftMult" min="-1000.0" max="1000.0"/>
    <float name="m_fAttackDiveMult" min="-1000.0" max="1000.0"/>
    <float name="m_fGearDownDragV" min="-1000.0" max="1000.0"/>
    <float name="m_fGearDownLiftMult" min="-1000.0" max="1000.0"/>
    <float name="m_fWindMult" min="-1000.0" max="1000.0"/>
    <float name="m_fMoveRes" min="-1000.0" max="1000.0"/>
    <Vec3V name="m_vecTurnRes" description="Turn resistance" />
	  <Vec3V name="m_vecSpeedRes" description="Speed resistance" />
    <float name="m_fGearDoorFrontOpen" min="-1000.0" max="1000.0"/>
	  <float name="m_fGearDoorRearOpen" min="-1000.0" max="1000.0"/>
	  <float name="m_fGearDoorRearOpen2" min="-1000.0" max="1000.0"/>
	  <float name="m_fGearDoorRearMOpen" min="-1000.0" max="1000.0"/>
	  <float name="m_fTurublenceMagnitudeMax" min="-1000.0" max="1000.0"/>
	  <float name="m_fTurublenceForceMulti" min="-1000.0" max="1000.0"/>
	  <float name="m_fTurublenceRollTorqueMulti" min="-1000.0" max="1000.0"/>
	  <float name="m_fTurublencePitchTorqueMulti" min="-1000.0" max="1000.0"/>
	  <float name="m_fBodyDamageControlEffectMult" min="-1000.0" max="1000.0"/>
	  <float name="m_fInputSensitivityForDifficulty" min="-1000.0" max="1000.0"/>
	  <float name="m_fOnGroundYawBoostSpeedPeak"  min="-1000.0" max="1000.0"/>
	  <float name="m_fOnGroundYawBoostSpeedCap"  min="-1000.0" max="1000.0"/>
	  <float name="m_fEngineOffGlideMulti"  min="-1000.0" max="1000.0"/>
	  <float name="m_fAfterburnerEffectRadius"  init="0.5" min="0.0" max="10.0"/>
	  <float name="m_fAfterburnerEffectDistance"  init="4.0" min="0.0" max="10.0"/>
	  <float name="m_fAfterburnerEffectForceMulti"  init="0.2" min="0.0" max="10.0"/>
    <float name="m_fSubmergeLevelToPullHeliUnderwater" init="0.3" min="0.0" max="1.0"/>
    <float name="m_fExtraLiftWithRoll" init="0.0" min="0.0" max="100.0"/>
	  <enum name="m_handlingType" type="eHandlingType" init="HANDLING_TYPE_FLYING"/>
  </structdef>

  <const name="MAX_NUM_VEHICLE_WEAPONS" value="6"/>
  <const name="MAX_NUM_VEHICLE_TURRETS" value="12"/>

  <structdef type="CVehicleWeaponHandlingData" base="CBaseSubHandlingData">
    <array name="m_uWeaponHash" type="member" size="MAX_NUM_VEHICLE_WEAPONS"> 
      <string type="atHashString"/> 
    </array>
    <array name="m_WeaponSeats" type="member" size="MAX_NUM_VEHICLE_WEAPONS"> 
      <s32/> 
    </array>
    <array name="m_WeaponVehicleModType" type="member" size="MAX_NUM_VEHICLE_WEAPONS">
      <enum type="eVehicleModType" init="VMT_INVALID"/>
    </array>
    <array name="m_fTurretSpeed" type="member" size="MAX_NUM_VEHICLE_TURRETS"> 
      <float/>
    </array>
    <array name="m_fTurretPitchMin" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <float/>
    </array>
    <array name="m_fTurretPitchMax" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <float/>
    </array>
    <array name="m_fTurretCamPitchMin" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <float/>
    </array>
    <array name="m_fTurretCamPitchMax" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <float/>
    </array>
    <array name="m_fBulletVelocityForGravity" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <float/>
    </array>
    <array name="m_fTurretPitchForwardMin" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <float/>
    </array>
    <array name="m_TurretPitchLimitData" type="member" size="MAX_NUM_VEHICLE_TURRETS">
      <struct type="CVehicleWeaponHandlingData::sTurretPitchLimits"/>
    </array>
    <float name="m_fUvAnimationMult" min="-1000.0" max="1000.0"/>
    <float name="m_fMiscGadgetVar" min="-1000.0" max="1000.0"/>
    <float name="m_fWheelImpactOffset" min="-1000.0" max="1000.0"/>
  </structdef>

  <structdef type="CVehicleWeaponHandlingData::sTurretPitchLimits">
    <float name="m_fForwardAngleMin" init="0.000000f" min="0.0f" max="180.0f"/>
    <float name="m_fForwardAngleMax" init="80.844345f" min="0.0f" max="180.0f"/>
    <float name="m_fBackwardAngleMin" init="125.30587f" min="0.0f" max="180.0f"/>
    <float name="m_fBackwardAngleMid" init="139.51522f" min="0.0f" max="180.0f"/>
    <float name="m_fBackwardAngleMax" init="169.53821f" min="0.0f" max="180.0f"/>
    <float name="m_fBackwardForcedPitch" init="-0.597f" min="-1.5f" max="1.5f"/>
  </structdef>
  
  <structdef type="CBikeHandlingData" base="CBaseSubHandlingData">
    <float name="m_fLeanFwdCOMMult" min="-1000.0" max="1000.0"/>
    <float name="m_fLeanFwdForceMult" min="-1000.0" max="1000.0"/>
    <float name="m_fLeanBakCOMMult" min="-1000.0" max="1000.0"/>
    <float name="m_fLeanBakForceMult" min="-1000.0" max="1000.0"/>
    <float name="m_fMaxBankAngle" min="-1000.0" max="1000.0"/>
    <float name="m_fFullAnimAngle" min="-1000.0" max="1000.0"/>
    <float name="m_fDesLeanReturnFrac" min="-1000.0" max="1000.0"/>
    <float name="m_fStickLeanMult" min="-1000.0" max="1000.0"/>
    <float name="m_fBrakingStabilityMult" min="-1000.0" max="1000.0"/>
    <float name="m_fInAirSteerMult" min="-1000.0" max="1000.0"/>
    <float name="m_fWheelieBalancePoint" min="-1000.0" max="1000.0"/>
    <float name="m_fStoppieBalancePoint" min="-1000.0" max="1000.0"/>
    <float name="m_fWheelieSteerMult" min="-1000.0" max="1000.0"/>
    <float name="m_fRearBalanceMult" min="-1000.0" max="1000.0"/>
    <float name="m_fFrontBalanceMult" min="-1000.0" max="1000.0"/>
    <float name="m_fBikeGroundSideFrictionMult" min="-1000.0" max="1000.0"/>
    <float name="m_fBikeWheelGroundSideFrictionMult" min="-1000.0" max="1000.0"/>
    <float name="m_fBikeOnStandLeanAngle" min="-1000.0" max="1000.0"/>
    <float name="m_fBikeOnStandSteerAngle" min="-1000.0" max="1000.0"/>
    <float name="m_fJumpForce" min="-1000.0" max="1000.0"/>
  </structdef>

  <structdef type="CSubmarineHandlingData" base="CBaseSubHandlingData">
    <float name="m_fPitchMult" min="-1000.0" max="1000.0"/>
    <float name="m_fPitchAngle" min="-1000.0" max="1000.0"/>
    <float name="m_fYawMult" min="-1000.0" max="1000.0"/>
    <float name="m_fDiveSpeed" min="-1000.0" max="1000.0"/>
    <float name="m_fRollMult" min="-1000.0" max="1000.0"/>
    <float name="m_fRollStab" min="-1000.0" max="1000.0"/>
    <Vec3V name="m_vTurnRes" description="Turn resistance" />
    <float name="m_fMoveResXY" min="-1000.0" max="1000.0"/>
    <float name="m_fMoveResZ" min="-1000.0" max="1000.0"/>
  </structdef>

  <structdef type="CTrailerHandlingData" base="CBaseSubHandlingData">
    <float name="m_fAttachLimitPitch" min="-1000.0" max="1000.0"/>
    <float name="m_fAttachLimitRoll" min="-1000.0" max="1000.0"/>
    <float name="m_fAttachLimitYaw" min="-1000.0" max="1000.0"/>
    <float name="m_fUprightSpringConstant" min="-1000.0" max="1000.0"/>
    <float name="m_fUprightDampingConstant" min="-1000.0" max="1000.0"/>
    <float name="m_fAttachedMaxDistance" min="-1000.0" max="1000.0"/>
    <float name="m_fAttachedMaxPenetration" min="-1000.0" max="1000.0"/>
    <float name="m_fAttachRaiseZ" min="-1000.0" max="1000.0"/>
    <float name="m_fPosConstraintMassRatio" min="-1000.0" max="1000.0"/>
  </structdef>

  <structdef type="CCarHandlingData" base="CBaseSubHandlingData">
    <float name="m_fBackEndPopUpCarImpulseMult" min="-1000.0" max="1000.0"/>
    <float name="m_fBackEndPopUpBuildingImpulseMult" min="-1000.0" max="1000.0"/>
    <float name="m_fBackEndPopUpMaxDeltaSpeed" min="-1000.0" max="1000.0"/>
    <float name="m_fToeFront" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fToeRear" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fCamberFront" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fCamberRear" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fCastor" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fEngineResistance" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fMaxDriveBiasTransfer" init="-1.0" min="-1.0" max="1.0"/>
    <float name="m_fJumpForceScale" init="1.0" min="0.0" max="10.0"/>
    <float name="m_fIncreasedRammingForceScale" init="1.0" min="0.0" max="10.0"/>
    <string name="m_strAdvancedFlags" type="atFinalHashString"/>
    <array name="m_AdvancedData" type="atArray">
      <struct type="CAdvancedData"/>
    </array>
  </structdef>

  <structdef type="CSpecialFlightHandlingData" base="CBaseSubHandlingData">
    <Vec3V name="m_vecAngularDamping" init="0.0,0.0,0.0"/>
    <Vec3V name="m_vecAngularDampingMin" init="0.0,0.0,0.0"/>
    <Vec3V name="m_vecLinearDamping" init="0.0,0.0,0.0"/>
    <Vec3V name="m_vecLinearDampingMin" init="0.0,0.0,0.0"/>
    <float name="m_fLiftCoefficient" init="150.0" min="-1000.0" max="1000.0"/>
    <float name="m_fCriticalLiftAngle" init="45.0" min="-1000.0" max="1000.0"/>
    <float name="m_fInitialLiftAngle" init="1.5" min="-1000.0" max="1000.0"/>
    <float name="m_fMaxLiftAngle" init="25.0" min="-1000.0" max="1000.0"/>
    <float name="m_fDragCoefficient" init="0.4" min="-1000.0" max="1000.0"/>
    <float name="m_fBrakingDrag" init="10.0" min="-1000.0" max="1000.0"/>
    <float name="m_fMaxLiftVelocity" init="2000.0" min="-10000.0" max="10000.0"/>
    <float name="m_fMinLiftVelocity" init="1.0" min="-10000.0" max="10000.0"/>
    <float name="m_fRollTorqueScale" init="3.0" min="-1000.0" max="1000.0"/>
    <float name="m_fMaxTorqueVelocity" init="100.0" min="-1000.0" max="1000.0"/>
    <float name="m_fMinTorqueVelocity" init="40000.0" min="-100000.0" max="100000.0"/>
    <float name="m_fYawTorqueScale" init="-900.0" min="-10000.0" max="10000.0"/>
    <float name="m_fSelfLevelingPitchTorqueScale" init="-5.0" min="-1000.0" max="1000.0"/>
    <float name="m_fSelfLevelingRollTorqueScale" init="-5.0" min="-1000.0" max="1000.0"/>
    <float name="m_fMaxPitchTorque" init="1500.0" min="-10000.0" max="10000.0"/>
    <float name="m_fMaxSteeringRollTorque" init="250.0" min="-1000.0" max="1000.0"/>
    <float name="m_fPitchTorqueScale" init="400.0" min="-1000.0" max="1000.0"/>
    <float name="m_fSteeringTorqueScale" init="1000.0" min="-10000.0" max="10000.0"/>
    <float name="m_fMaxThrust" init="0.0" min="-1000.0" max="1000.0"/>
    <float name="m_fTransitionDuration" init="0.0" min="0.0" max="1000.0"/>
    <float name="m_fHoverVelocityScale" init="1.0" min="0.0" max="1000.0"/>
    <float name="m_fStabilityAssist" init="10.0" min="0.0" max="1000.0"/>
    <float name="m_fMinSpeedForThrustFalloff" init="0.0" min="0.0" max="1.0"/>
    <float name="m_fBrakingThrustScale" init="0.0" min="0.0" max="1.0"/>
    <int name="m_mode" init="0" min="0" max="10"/>
    <string name="m_strFlags" type="atFinalHashString"/>
  </structdef>
</ParserSchema>
