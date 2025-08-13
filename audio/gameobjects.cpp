// 
// gameobjects.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
// 
// Automatically generated metadata structure functions - do not edit.
// 

#include "gameobjects.h"
#include "string/stringhash.h"

namespace rage
{
	// 
	// VehicleCollisionSettings
	// 
	void* VehicleCollisionSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3239955667U: return &MediumIntensity;
			case 710033239U: return &HighIntensity;
			case 4192925178U: return &SmallScrapeImpact;
			case 127569295U: return &SlowScrapeImpact;
			case 281069533U: return &SlowScrapeLoop;
			case 1263175108U: return &RollSound;
			case 1604341717U: return &VehOnVehCrashSound;
			case 1953393354U: return &HighImpactSweetenerSound;
			case 432787405U: return &SweetenerImpactThreshold;
			case 2022363501U: return &ScrapePitchCurve;
			case 2432289282U: return &ScrapeVolCurve;
			case 975010848U: return &SlowScrapeVolCurve;
			case 3618752892U: return &ScrapeImpactVolCurve;
			case 1284663296U: return &SlowScrapeImpactCurve;
			case 2500099234U: return &ImpactStartOffsetCurve;
			case 4162172578U: return &ImpactVolCurve;
			case 1504874349U: return &VehicleImpactStartOffsetCurve;
			case 4010972134U: return &VehicleImpactVolCurve;
			case 1629513157U: return &VelocityImpactScalingCurve;
			case 1333339381U: return &FakeImpactStartOffsetCurve;
			case 1868707284U: return &FakeImpactVolCurve;
			case 875110346U: return &FakeImpactMin;
			case 2689593106U: return &FakeImpactMax;
			case 983348422U: return &FakeImpactScale;
			case 1579844700U: return &VehicleSizeScale;
			case 3336370667U: return &FakeImpactTriggerDelta;
			case 2537982478U: return &FakeImpactSweetenerThreshold;
			case 891410493U: return &DamageVolCurve;
			case 2420061368U: return &JumpLandVolCurve;
			case 3239044183U: return &VehicleMaterialSettings;
			case 478265938U: return &DeformationSound;
			case 351649166U: return &ImpactDebris;
			case 4192721486U: return &GlassDebris;
			case 894066946U: return &PostImpactDebris;
			case 390743066U: return &PedCollisionMin;
			case 3376361265U: return &PedCollisionMax;
			case 3195220058U: return &CollisionMin;
			case 1666449989U: return &CollisionMax;
			case 3558132706U: return &VehicleCollisionMin;
			case 535714220U: return &VehicleCollisionMax;
			case 2459258374U: return &VehicleSweetenerThreshold;
			case 2514634336U: return &ScrapeMin;
			case 1137227450U: return &ScrapeMax;
			case 2683485846U: return &DamageMin;
			case 3349845769U: return &DamageMax;
			case 2677201182U: return &TrainImpact;
			case 21229952U: return &TrainImpactLoop;
			case 106591531U: return &WaveImpactLoop;
			case 2677400473U: return &FragMaterial;
			case 2086037786U: return &WheelFragMaterial;
			case 892361655U: return &MeleeMaterialOverride;
			default: return NULL;
		}
	}
	
	// 
	// TrailerAudioSettings
	// 
	void* TrailerAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2009943158U: return &BumpSound;
			case 2766728297U: return &ClatterType;
			case 760482041U: return &LinkStressSound;
			case 3387839464U: return &ModelCollisionSettings;
			case 659863331U: return &FireAudio;
			case 692898947U: return &TrailerBumpVolumeBoost;
			case 4167114427U: return &ClatterSensitivityScalar;
			case 2016160203U: return &ClatterVolumeBoost;
			case 2289509290U: return &ChassisStressSensitivityScalar;
			case 3767666612U: return &ChassisStressVolumeBoost;
			case 2539925573U: return &LinkStressVolumeBoost;
			case 1426849549U: return &LinkStressSensitivityScalar;
			default: return NULL;
		}
	}
	
	// 
	// CarAudioSettings
	// 
	void* CarAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 321178074U: return &Engine;
			case 726625478U: return &GranularEngine;
			case 2912937758U: return &HornSounds;
			case 373536401U: return &DoorOpenSound;
			case 1412306350U: return &DoorCloseSound;
			case 2133483988U: return &BootOpenSound;
			case 2949429154U: return &BootCloseSound;
			case 1263175108U: return &RollSound;
			case 4194451388U: return &BrakeSqueekFactor;
			case 1728118752U: return &SuspensionUp;
			case 3517046579U: return &SuspensionDown;
			case 368667153U: return &MinSuspCompThresh;
			case 4179579467U: return &MaxSuspCompThres;
			case 2758766474U: return &VehicleCollisions;
			case 696511390U: return &CarMake;
			case 729586239U: return &CarModel;
			case 3619195002U: return &CarCategory;
			case 1400217334U: return &ScannerVehicleSettings;
			case 4026253372U: return &JumpLandSound;
			case 2670324367U: return &DamagedJumpLandSound;
			case 859561792U: return &JumpLandMinThresh;
			case 1730459995U: return &JumpLandMaxThresh;
			case 2305139522U: return &VolumeCategory;
			case 1884706182U: return &GpsType;
			case 3074722591U: return &RadioType;
			case 3489610907U: return &RadioGenre;
			case 732055784U: return &IndicatorOn;
			case 582873257U: return &IndicatorOff;
			case 673079717U: return &Handbrake;
			case 1332839749U: return &GpsVoice;
			case 3370917842U: return &AmbientRadioVol;
			case 4087392778U: return &RadioLeakage;
			case 3856961408U: return &ParkingTone;
			case 3722939709U: return &RoofStuckSound;
			case 2000939657U: return &FreewayPassbyTyreBumpFront;
			case 4093336786U: return &FreewayPassbyTyreBumpBack;
			case 659863331U: return &FireAudio;
			case 2504570285U: return &StartupRevs;
			case 2907915506U: return &WindNoise;
			case 596350277U: return &FreewayPassbyTyreBumpFrontSide;
			case 2195817590U: return &FreewayPassbyTyreBumpBackSide;
			case 1310059646U: return &MaxRollOffScalePlayer;
			case 3311640961U: return &MaxRollOffScaleNPC;
			case 377534139U: return &ConvertibleRoofSoundSet;
			case 2471037734U: return &OffRoadRumbleSoundVolume;
			case 1370309038U: return &SirenSounds;
			case 3021899838U: return &AlternativeGranularEngines;
			case 2747039366U: return &AlternativeGranularEngineProbability;
			case 2308888679U: return &StopStartProb;
			case 1201498163U: return &NPCRoadNoise;
			case 14193554U: return &NPCRoadNoiseHighway;
			case 2538077289U: return &ForkliftSounds;
			case 754817281U: return &TurretSounds;
			case 2766728297U: return &ClatterType;
			case 1181791813U: return &DiggerSounds;
			case 755254262U: return &TowTruckSounds;
			case 1067940573U: return &EngineType;
			case 3601187659U: return &ElectricEngine;
			case 656522297U: return &Openness;
			case 815644664U: return &ReverseWarningSound;
			case 1954314554U: return &RandomDamage;
			case 1436016508U: return &WindClothSound;
			case 4174611307U: return &CarSpecificShutdownSound;
			case 4167114427U: return &ClatterSensitivityScalar;
			case 2016160203U: return &ClatterVolumeBoost;
			case 2289509290U: return &ChassisStressSensitivityScalar;
			case 3767666612U: return &ChassisStressVolumeBoost;
			case 2669004390U: return &VehicleRainSound;
			case 2901158918U: return &AdditionalRevsIncreaseSmoothing;
			case 2125484799U: return &AdditionalRevsDecreaseSmoothing;
			case 3633875163U: return &AdditionalGearChangeSmoothing;
			case 2632746297U: return &AdditionalGearChangeSmoothingTime;
			case 3980976388U: return &ConvertibleRoofInteriorSoundSet;
			case 38374235U: return &VehicleRainSoundInterior;
			case 4139988033U: return &CabinToneLoop;
			case 574270050U: return &InteriorViewEngineOpenness;
			case 1710169874U: return &JumpLandSoundInterior;
			case 15668478U: return &DamagedJumpLandSoundInterior;
			default: return NULL;
		}
	}
	
	// 
	// VehicleEngineAudioSettings
	// 
	void* VehicleEngineAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2167226048U: return &MasterVolume;
			case 818744835U: return &MaxConeAttenuation;
			case 2410429316U: return &FXCompensation;
			case 3603864030U: return &NonPlayerFXComp;
			case 1237587397U: return &LowEngineLoop;
			case 3957551506U: return &HighEngineLoop;
			case 3659535465U: return &LowExhaustLoop;
			case 2652037072U: return &HighExhaustLoop;
			case 1902889865U: return &RevsOffLoop;
			case 1624340862U: return &MinPitch;
			case 1067946521U: return &MaxPitch;
			case 2910766377U: return &IdleEngineSimpleLoop;
			case 3023122471U: return &IdleExhaustSimpleLoop;
			case 2464910042U: return &IdleMinPitch;
			case 1111037462U: return &IdleMaxPitch;
			case 2909847138U: return &InductionLoop;
			case 3316618285U: return &InductionMinPitch;
			case 2082141580U: return &InductionMaxPitch;
			case 4145557244U: return &TurboWhine;
			case 2382158696U: return &TurboMinPitch;
			case 3370191368U: return &TurboMaxPitch;
			case 3358378925U: return &DumpValve;
			case 2295689326U: return &DumpValveProb;
			case 2970626307U: return &TurboSpinupSpeed;
			case 2665856244U: return &GearTransLoop;
			case 1485820995U: return &GearTransMinPitch;
			case 1459796124U: return &GearTransMaxPitch;
			case 290831699U: return &GTThrottleVol;
			case 652097730U: return &Ignition;
			case 2017509444U: return &EngineShutdown;
			case 3427289561U: return &CoolingFan;
			case 2721901550U: return &ExhaustPops;
			case 1381571371U: return &StartLoop;
			case 4274045295U: return &MasterTurboVolume;
			case 3004023717U: return &MasterTransmissionVolume;
			case 4193771599U: return &EngineStartUp;
			case 2803085325U: return &EngineSynthDef;
			case 3125631121U: return &EngineSynthPreset;
			case 2186643980U: return &ExhaustSynthDef;
			case 2272656214U: return &ExhaustSynthPreset;
			case 3101393458U: return &EngineSubmixVoice;
			case 1509277591U: return &ExhaustSubmixVoice;
			case 2651250534U: return &UpgradedTransmissionVolumeBoost;
			case 2510168813U: return &UpgradedGearChangeInt;
			case 1799846808U: return &UpgradedGearChangeExt;
			case 342118328U: return &UpgradedEngineVolumeBoost_PostSubmix;
			case 1918450217U: return &UpgradedEngineSynthDef;
			case 2726392985U: return &UpgradedEngineSynthPreset;
			case 1963984898U: return &UpgradedExhaustVolumeBoost_PostSubmix;
			case 1559345364U: return &UpgradedExhaustSynthDef;
			case 1079773534U: return &UpgradedExhaustSynthPreset;
			case 3629386813U: return &UpgradedDumpValve;
			case 4293463649U: return &UpgradedTurboVolumeBoost;
			case 4188399420U: return &UpgradedGearTransLoop;
			case 3451241531U: return &UpgradedTurboWhine;
			case 1311298761U: return &UpgradedInductionLoop;
			case 3250532025U: return &UpgradedExhaustPops;
			default: return NULL;
		}
	}
	
	// 
	// CollisionMaterialSettings
	// 
	void* CollisionMaterialSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2912817569U: return &HardImpact;
			case 682784113U: return &SolidImpact;
			case 604622585U: return &SoftImpact;
			case 3506047791U: return &ScrapeSound;
			case 2002842367U: return &PedScrapeSound;
			case 2834299720U: return &BreakSound;
			case 1594184241U: return &DestroySound;
			case 3816864555U: return &SettleSound;
			case 3348678644U: return &BulletImpactSound;
			case 1845600018U: return &AutomaticBulletImpactSound;
			case 2830950128U: return &ShotgunBulletImpactSound;
			case 2963942448U: return &BigVehicleImpactSound;
			case 1094194597U: return &PedPunch;
			case 329557697U: return &PedKick;
			case 3239955667U: return &MediumIntensity;
			case 710033239U: return &HighIntensity;
			case 135216190U: return &Hardness;
			case 1262997423U: return &MinImpulseMag;
			case 2337084586U: return &MaxImpulseMag;
			case 3075899418U: return &ImpulseMagScalar;
			case 528057689U: return &MaxScrapeSpeed;
			case 3739827806U: return &MinScrapeSpeed;
			case 3204215891U: return &ScrapeImpactMag;
			case 2723693000U: return &MaxRollSpeed;
			case 2504414996U: return &MinRollSpeed;
			case 1149079353U: return &RollImpactMag;
			case 2372031706U: return &BulletCollisionScaling;
			case 2370085788U: return &FootstepCustomImpactSound;
			case 3627627855U: return &FootstepMaterialHardness;
			case 1919272088U: return &FootstepMaterialLoudness;
			case 1978996272U: return &FootstepSettings;
			case 1497556189U: return &FootstepScaling;
			case 2817757122U: return &ScuffstepScaling;
			case 2826892061U: return &FootstepImpactScaling;
			case 4124876634U: return &ScuffImpactScaling;
			case 4164193537U: return &SkiSettings;
			case 2895275491U: return &AnimalReference;
			case 3737074423U: return &WetMaterialReference;
			case 2500099234U: return &ImpactStartOffsetCurve;
			case 4162172578U: return &ImpactVolCurve;
			case 2022363501U: return &ScrapePitchCurve;
			case 2432289282U: return &ScrapeVolCurve;
			case 909250695U: return &FastTyreRoll;
			case 3004042689U: return &DetailTyreRoll;
			case 1674759086U: return &MainSkid;
			case 79858612U: return &SideSkid;
			case 2814167154U: return &MetalShellCasingSmall;
			case 1461289497U: return &MetalShellCasingMedium;
			case 3918739637U: return &MetalShellCasingLarge;
			case 1654538062U: return &MetalShellCasingSmallSlow;
			case 3994385662U: return &MetalShellCasingMediumSlow;
			case 3275110336U: return &MetalShellCasingLargeSlow;
			case 990630211U: return &PlasticShellCasing;
			case 4184577014U: return &PlasticShellCasingSlow;
			case 1263175108U: return &RollSound;
			case 1823280965U: return &RainLoop;
			case 3506010071U: return &TyreBump;
			case 1484099351U: return &ShockwaveSound;
			case 1322561434U: return &RandomAmbient;
			case 669526881U: return &ClimbSettings;
			case 2989741111U: return &Dirtiness;
			case 3411680262U: return &SurfaceSettle;
			case 826541451U: return &RidgedSurfaceLoop;
			case 3506397195U: return &CollisionCount;
			case 2136241104U: return &CollisionCountThreshold;
			case 1956282229U: return &VolumeThreshold;
			case 2329642112U: return &MaterialID;
			case 4048745621U: return &SoundSet;
			case 3382453818U: return &DebrisLaunch;
			case 1589699747U: return &DebrisLand;
			case 2825432433U: return &OffRoadSound;
			case 1802626656U: return &Roughness;
			case 2394313400U: return &DebrisOnSlope;
			case 915856294U: return &BicycleTyreRoll;
			case 1816482736U: return &OffRoadRumbleSound;
			case 165423196U: return &StealthSweetener;
			case 522854583U: return &Scuff;
			case 3861213752U: return &MaterialType;
			case 3208210028U: return &SurfacePriority;
			case 3930909145U: return &WheelSpinLoop;
			case 555650046U: return &BicycleTyreGritSound;
			case 544377827U: return &PedSlideSound;
			case 4149965069U: return &PedRollSound;
			case 2022614441U: return &TimeInAirToTriggerBigLand;
			case 1489785435U: return &MeleeOverideMaterial;
			case 2490697966U: return &NameHash;
			case 2295485865U: return &SlowMoHardImpact;
			case 3852197638U: return &SlowMoBulletImpactSound;
			case 336826453U: return &SlowMoAutomaticBulletImpactSound;
			case 453961103U: return &SlowMoShotgunBulletImpactSound;
			case 1178800870U: return &SlowMoBulletImpactPreSuck;
			case 2552355110U: return &SlowMoBulletImpactPreSuckTime;
			case 1954633885U: return &SlowMoAutomaticBulletImpactPreSuck;
			case 1766895448U: return &SlowMoAutomaticBulletImpactPreSuckTime;
			case 2091506545U: return &SlowMoShotgunBulletImpactPreSuck;
			case 898712969U: return &SlowMoShotgunBulletImpactPreSuckTime;
			default: return NULL;
		}
	}
	
	// 
	// StaticEmitter
	// 
	void* StaticEmitter::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3255258383U: return &Sound;
			case 3177624837U: return &RadioStation;
			case 18243940U: return &Position;
			case 3953290452U: return &MinDistance;
			case 1153964379U: return &MaxDistance;
			case 613106326U: return &EmittedVolume;
			case 2298116791U: return &FilterCutoff;
			case 1700098281U: return &HPFCutoff;
			case 3832636828U: return &RolloffFactor;
			case 303342407U: return &InteriorSettings;
			case 1815167341U: return &InteriorRoom;
			case 1284600885U: return &RadioStationForScore;
			case 741656097U: return &MaxLeakage;
			case 1590000706U: return &MinLeakageDistance;
			case 4260314658U: return &MaxLeakageDistance;
			case 2642517978U: return &AlarmSettings;
			case 2475969897U: return &OnBreakOneShot;
			case 1718779422U: return &MaxPathDepth;
			case 2010550034U: return &SmallReverbSend;
			case 4126590696U: return &MediumReverbSend;
			case 1958040013U: return &LargeReverbSend;
			case 4043651850U: return &MinTimeMinutes;
			case 1589106051U: return &MaxTimeMinutes;
			case 4133920304U: return &BrokenHealth;
			case 3784520588U: return &UndamagedHealth;
			default: return NULL;
		}
	}
	
	// 
	// EntityEmitter
	// 
	void* EntityEmitter::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3255258383U: return &Sound;
			case 1153964379U: return &MaxDistance;
			case 2063118281U: return &BusinessHoursProb;
			case 283839293U: return &EveningProb;
			case 2451731996U: return &NightProb;
			case 1654868479U: return &ConeInnerAngle;
			case 3987837220U: return &ConeOuterAngle;
			case 1327232635U: return &ConeMaxAtten;
			case 3636337152U: return &StopAfterLoudSound;
			case 1718779422U: return &MaxPathDepth;
			case 4133920304U: return &BrokenHealth;
			default: return NULL;
		}
	}
	
	// 
	// HeliAudioSettings
	// 
	void* HeliAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 506573175U: return &RotorLoop;
			case 1066425761U: return &RearRotorLoop;
			case 564118867U: return &ExhaustLoop;
			case 480275554U: return &BankingLoop;
			case 4139988033U: return &CabinToneLoop;
			case 2559352996U: return &ThrottleSmoothRate;
			case 937624562U: return &BankAngleVolumeCurve;
			case 3829001805U: return &BankThrottleVolumeCurve;
			case 1464433872U: return &BankThrottlePitchCurve;
			case 619356400U: return &RotorThrottleVolumeCurve;
			case 4077265274U: return &RotorThrottlePitchCurve;
			case 683840698U: return &RotorThrottleGapCurve;
			case 649582580U: return &RearRotorThrottleVolumeCurve;
			case 3102346705U: return &ExhaustThrottleVolumeCurve;
			case 3783234132U: return &ExhaustThrottlePitchCurve;
			case 3544746782U: return &RotorConeFrontAngle;
			case 1224914927U: return &RotorConeRearAngle;
			case 1621982478U: return &RotorConeAtten;
			case 2060169591U: return &RearRotorConeFrontAngle;
			case 2315686201U: return &RearRotorConeRearAngle;
			case 1748950007U: return &RearRotorConeAtten;
			case 2917528845U: return &ExhaustConeFrontAngle;
			case 3928066140U: return &ExhaustConeRearAngle;
			case 2832783546U: return &ExhaustConeAtten;
			case 2235235222U: return &Filter1ThrottleResonanceCurve;
			case 2834468175U: return &Filter2ThrottleResonanceCurve;
			case 2451513310U: return &BankingResonanceCurve;
			case 621146399U: return &RotorVolumeStartupCurve;
			case 319350381U: return &BladeVolumeStartupCurve;
			case 1350014048U: return &RotorPitchStartupCurve;
			case 23142629U: return &RearRotorVolumeStartupCurve;
			case 3657547247U: return &ExhaustVolumeStartupCurve;
			case 928133667U: return &RotorGapStartupCurve;
			case 928086518U: return &StartUpOneShot;
			case 777079805U: return &BladeConeUpAngle;
			case 2412475905U: return &BladeConeDownAngle;
			case 2361436674U: return &BladeConeAtten;
			case 2857168235U: return &ThumpConeUpAngle;
			case 3474875314U: return &ThumpConeDownAngle;
			case 1437902713U: return &ThumpConeAtten;
			case 3722448722U: return &ScannerMake;
			case 1957994399U: return &ScannerModel;
			case 3050227020U: return &ScannerCategory;
			case 1400217334U: return &ScannerVehicleSettings;
			case 3074722591U: return &RadioType;
			case 3489610907U: return &RadioGenre;
			case 3272044876U: return &DoorOpen;
			case 2239999389U: return &DoorClose;
			case 101410581U: return &DoorLimit;
			case 4240818009U: return &DamageLoop;
			case 2592197572U: return &RotorSpeedToTriggerSpeedCurve;
			case 1837550431U: return &RotorVolumePostSubmix;
			case 2803085325U: return &EngineSynthDef;
			case 3125631121U: return &EngineSynthPreset;
			case 2186643980U: return &ExhaustSynthDef;
			case 2272656214U: return &ExhaustSynthPreset;
			case 3101393458U: return &EngineSubmixVoice;
			case 1509277591U: return &ExhaustSubmixVoice;
			case 1753137395U: return &RotorLowFreqLoop;
			case 2874767560U: return &ExhaustPitchStartupCurve;
			case 2758766474U: return &VehicleCollisions;
			case 659863331U: return &FireAudio;
			case 2147812390U: return &DistantLoop;
			case 1411959386U: return &SecondaryDoorStartOpen;
			case 1466494771U: return &SecondaryDoorStartClose;
			case 2079366249U: return &SecondaryDoorClose;
			case 1894170735U: return &SecondaryDoorLimit;
			case 1728118752U: return &SuspensionUp;
			case 3517046579U: return &SuspensionDown;
			case 368667153U: return &MinSuspCompThresh;
			case 4179579467U: return &MaxSuspCompThres;
			case 2894213347U: return &DamageOneShots;
			case 1620798370U: return &DamageWarning;
			case 3867169476U: return &TailBreak;
			case 740297458U: return &RotorBreak;
			case 3543472155U: return &RearRotorBreak;
			case 3761037768U: return &CableDeploy;
			case 2507132557U: return &HardScrape;
			case 3432150358U: return &EngineCooling;
			case 2377405541U: return &AltitudeWarning;
			case 1083135155U: return &HealthToDamageVolumeCurve;
			case 664962265U: return &HealthBelow600ToDamageVolumeCurve;
			case 3225291095U: return &DamageBelow600Loop;
			case 660932672U: return &AircraftWarningSeriousDamageThresh;
			case 2157826416U: return &AircraftWarningCriticalDamageThresh;
			case 3472440331U: return &AircraftWarningMaxSpeed;
			case 3163456940U: return &SimpleSoundForLoading;
			case 419141114U: return &SlowMoRotorLoop;
			case 1662072142U: return &SlowMoRotorLowFreqLoop;
			case 2669004390U: return &VehicleRainSound;
			case 38374235U: return &VehicleRainSoundInterior;
			case 3602706900U: return &StartUpFailOneShot;
			default: return NULL;
		}
	}
	
	// 
	// MeleeCombatSettings
	// 
	void* MeleeCombatSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4221676888U: return &SwipeSound;
			case 3330275363U: return &GeneralHitSound;
			case 1868630081U: return &PedHitSound;
			case 2351412326U: return &PedResponseSound;
			case 3184327988U: return &HeadTakeDown;
			case 740283608U: return &BodyTakeDown;
			case 4176191952U: return &SmallAnimalHitSound;
			case 386068343U: return &SmallAnimalResponseSound;
			case 715914018U: return &BigAnimalHitSound;
			case 1800321105U: return &BigAnimalResponseSound;
			case 3867793090U: return &SlowMoPedHitSound;
			case 1847969752U: return &SlowMoPedResponseSound;
			case 767182303U: return &SlowMoHeadTakeDown;
			case 2969903882U: return &SlowMoBodyTakeDown;
			case 2251810748U: return &SlowMoSmallAnimalHitSound;
			case 2536866244U: return &SlowMoSmallAnimalResponseSound;
			case 49218671U: return &SlowMoBigAnimalHitSound;
			case 3574589997U: return &SlowMoBigAnimalResponseSound;
			default: return NULL;
		}
	}
	
	// 
	// SpeechContextSettings
	// 
	void* SpeechContextSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2138626625U: return &Contexts;
			default: return NULL;
		}
	}
	
	// 
	// TriggeredSpeechContext
	// 
	void* TriggeredSpeechContext::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3523486585U: return &TriggeredContextRepeatTime;
			case 2484010487U: return &PrimaryRepeatTime;
			case 1804931645U: return &PrimaryRepeatTimeOnSameVoice;
			case 3943512608U: return &BackupRepeatTime;
			case 2118402164U: return &BackupRepeatTimeOnSameVoice;
			case 1880962928U: return &TimeCanNextUseTriggeredContext;
			case 198153675U: return &TimeCanNextPlayPrimary;
			case 4290961805U: return &TimeCanNextPlayBackup;
			case 929185037U: return &PrimarySpeechContext;
			case 2121143139U: return &BackupSpeechContext;
			default: return NULL;
		}
	}
	
	// 
	// SpeechContext
	// 
	void* SpeechContext::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3177812160U: return &ContextNamePHash;
			case 3390884909U: return &RepeatTime;
			case 3513532534U: return &RepeatTimeOnSameVoice;
			case 1141253703U: return &VolumeType;
			case 1661817573U: return &Audibility;
			case 650716620U: return &GenderNonSpecificVersion;
			case 2697946337U: return &TimeCanNextPlay;
			case 342945216U: return &Priority;
			case 412015057U: return &FakeGesture;
			default: return NULL;
		}
	}
	
	// 
	// SpeechContextVirtual
	// 
	void* SpeechContextVirtual::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1754422642U: return &ResolvingFunction;
			case 208694998U: return &SpeechContexts;
			default: return NULL;
		}
	}
	
	// 
	// SpeechParams
	// 
	void* SpeechParams::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3457757475U: return &OverrideContextSettings;
			case 1478551369U: return &PreloadTimeoutInMs;
			case 2983738072U: return &RequestedVolume;
			case 1661817573U: return &Audibility;
			case 3390884909U: return &RepeatTime;
			case 3513532534U: return &RepeatTimeOnSameVoice;
			default: return NULL;
		}
	}
	
	// 
	// SpeechContextList
	// 
	void* SpeechContextList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 208694998U: return &SpeechContexts;
			default: return NULL;
		}
	}
	
	// 
	// BoatAudioSettings
	// 
	void* BoatAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2190254792U: return &Engine1Loop;
			case 3721442318U: return &Engine1Vol;
			case 1083244232U: return &Engine1Pitch;
			case 903830363U: return &Engine2Loop;
			case 2576997692U: return &Engine2Vol;
			case 1544300700U: return &Engine2Pitch;
			case 892482061U: return &LowResoLoop;
			case 3877813221U: return &LowResoLoopVol;
			case 1352640002U: return &LowResoPitch;
			case 3395630229U: return &ResoLoop;
			case 2285359988U: return &ResoLoopVol;
			case 747893133U: return &ResoPitch;
			case 1204943988U: return &WaterTurbulance;
			case 4056804874U: return &WaterTurbulanceVol;
			case 3822180097U: return &WaterTurbulancePitch;
			case 3722448722U: return &ScannerMake;
			case 1957994399U: return &ScannerModel;
			case 3050227020U: return &ScannerCategory;
			case 1400217334U: return &ScannerVehicleSettings;
			case 3074722591U: return &RadioType;
			case 3489610907U: return &RadioGenre;
			case 3986794320U: return &HornLoop;
			case 389709981U: return &IgnitionOneShot;
			case 1847637313U: return &ShutdownOneShot;
			case 4051407211U: return &EngineVolPostSubmix;
			case 1417656358U: return &ExhaustVolPostSubmix;
			case 2803085325U: return &EngineSynthDef;
			case 3125631121U: return &EngineSynthPreset;
			case 2186643980U: return &ExhaustSynthDef;
			case 2272656214U: return &ExhaustSynthPreset;
			case 2758766474U: return &VehicleCollisions;
			case 3101393458U: return &EngineSubmixVoice;
			case 1509277591U: return &ExhaustSubmixVoice;
			case 4177999727U: return &WaveHitSound;
			case 2538358285U: return &LeftWaterSound;
			case 2082667583U: return &IdleHullSlapLoop;
			case 391826909U: return &IdleHullSlapSpeedToVol;
			case 726625478U: return &GranularEngine;
			case 1746021194U: return &BankSpraySound;
			case 1332245015U: return &IgnitionLoop;
			case 4193771599U: return &EngineStartUp;
			case 3948595942U: return &SubTurningEnginePitchModifier;
			case 741794517U: return &SubTurningSweetenerSound;
			case 1349369538U: return &RevsToSweetenerVolume;
			case 3936910666U: return &TurningToSweetenerVolume;
			case 3782324903U: return &TurningToSweetenerPitch;
			case 365596534U: return &DryLandScrape;
			case 1310059646U: return &MaxRollOffScalePlayer;
			case 3311640961U: return &MaxRollOffScaleNPC;
			case 656522297U: return &Openness;
			case 1686495374U: return &DryLandHardScrape;
			case 951881381U: return &DryLandHardImpact;
			case 1436016508U: return &WindClothSound;
			case 659863331U: return &FireAudio;
			case 3272044876U: return &DoorOpen;
			case 2239999389U: return &DoorClose;
			case 101410581U: return &DoorLimit;
			case 160196471U: return &DoorStartClose;
			case 875969243U: return &SubExtrasSound;
			case 1293349463U: return &SFXBankSound;
			case 1316647925U: return &SubmersibleCreaksSound;
			case 1578326034U: return &WaveHitBigAirSound;
			case 637873149U: return &BigAirMinTime;
			case 2669004390U: return &VehicleRainSound;
			case 38374235U: return &VehicleRainSoundInterior;
			default: return NULL;
		}
	}
	
	// 
	// WeaponSettings
	// 
	void* WeaponSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1179030945U: return &FireSound;
			case 2779069207U: return &SuppressedFireSound;
			case 3728368034U: return &AutoSound;
			case 1129892947U: return &ReportSound;
			case 777371513U: return &ReportVolumeDelta;
			case 2718703635U: return &ReportPredelayDelta;
			case 3617329090U: return &TailEnergyValue;
			case 1697606446U: return &EchoSound;
			case 6129237U: return &SuppressedEchoSound;
			case 2636327962U: return &ShellCasingSound;
			case 4221676888U: return &SwipeSound;
			case 2003585540U: return &GeneralStrikeSound;
			case 1660834751U: return &PedStrikeSound;
			case 1253983631U: return &HeftSound;
			case 3162076005U: return &PutDownSound;
			case 1452522607U: return &RattleSound;
			case 1897941734U: return &RattleLandSound;
			case 1517392850U: return &PickupSound;
			case 1076348275U: return &ShellCasing;
			case 1356569853U: return &SafetyOn;
			case 3859729578U: return &SafetyOff;
			case 1949052065U: return &SpecialWeaponSoundSet;
			case 1566467066U: return &BankSound;
			case 2270774750U: return &InteriorShotSound;
			case 1476926917U: return &ReloadSounds;
			case 4117396556U: return &IntoCoverSound;
			case 2211813302U: return &OutOfCoverSound;
			case 3645250412U: return &BulletImpactTimeFilter;
			case 2114097320U: return &LastBulletImpactTime;
			case 2253645891U: return &RattleAimSound;
			case 966654654U: return &SmallAnimalStrikeSound;
			case 2322102325U: return &BigAnimalStrikeSound;
			case 2967898105U: return &SlowMoFireSound;
			case 2757031855U: return &HitPedSound;
			case 3405261878U: return &SlowMoFireSoundPresuck;
			case 751561197U: return &SlowMoSuppressedFireSound;
			case 1983089810U: return &SlowMoSuppressedFireSoundPresuck;
			case 1809703848U: return &SlowMoReportSound;
			case 3226530112U: return &SlowMoInteriorShotSound;
			case 2276853184U: return &SlowMoPedStrikeSound;
			case 1339904585U: return &SlowMoPedStrikeSoundPresuck;
			case 2930557792U: return &SlowMoBigAnimalStrikeSound;
			case 543771837U: return &SlowMoBigAnimalStrikeSoundPresuck;
			case 503605524U: return &SlowMoSmallAnimalStrikeSound;
			case 2915581935U: return &SlowMoSmallAnimalStrikeSoundPresuck;
			case 1133718334U: return &SlowMoFireSoundPresuckTime;
			case 875203132U: return &SlowMoSuppressedFireSoundPresuckTime;
			case 4162839949U: return &SlowMoPedStrikeSoundPresuckTime;
			case 4208679441U: return &SlowMoBigAnimalStrikeSoundPresuckTime;
			case 3349101642U: return &SlowMoSmallAnimalStrikeSoundPresuckTime;
			case 4195676929U: return &SuperSlowMoFireSound;
			case 3222432411U: return &SuperSlowMoFireSoundPresuck;
			case 590658268U: return &SuperSlowMoSuppressedFireSound;
			case 3429102669U: return &SuperSlowMoSuppressedFireSoundPresuck;
			case 2342587351U: return &SuperSlowMoReportSound;
			case 997750485U: return &SuperSlowMoInteriorShotSound;
			case 3843558835U: return &SuperSlowMoPedStrikeSound;
			case 3718653353U: return &SuperSlowMoPedStrikeSoundPresuck;
			case 195224063U: return &SuperSlowMoBigAnimalStrikeSound;
			case 3559910093U: return &SuperSlowMoBigAnimalStrikeSoundPresuck;
			case 391771104U: return &SuperSlowMoSmallAnimalStrikeSound;
			case 2256354908U: return &SuperSlowMoSmallAnimalStrikeSoundPresuck;
			case 4077733436U: return &SuperSlowMoFireSoundPresuckTime;
			case 2356327670U: return &SuperSlowMoSuppressedFireSoundPresuckTime;
			case 2855922818U: return &SuperSlowMoPedStrikeSoundPresuckTime;
			case 612970726U: return &SuperSlowMoBigAnimalStrikeSoundPresuckTime;
			case 1308180898U: return &SuperSlowMoSmallAnimalStrikeSoundPresuckTime;
			default: return NULL;
		}
	}
	
	// 
	// ShoeAudioSettings
	// 
	void* ShoeAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2203077788U: return &Walk;
			case 3322149966U: return &DirtyWalk;
			case 1679830332U: return &CreakyWalk;
			case 3862746794U: return &GlassyWalk;
			case 285848937U: return &Run;
			case 3944009248U: return &DirtyRun;
			case 410328223U: return &CreakyRun;
			case 2849838301U: return &GlassyRun;
			case 454421300U: return &WetWalk;
			case 4033635613U: return &WetRun;
			case 1978666105U: return &SoftWalk;
			case 522854583U: return &Scuff;
			case 3814051896U: return &Land;
			case 1621515614U: return &ShoeHardness;
			case 181105145U: return &ShoeCreakiness;
			case 114768823U: return &ShoeType;
			case 3063516179U: return &ShoeFitness;
			case 2892571087U: return &LadderShoeDown;
			case 3069084391U: return &LadderShoeUp;
			case 2221159044U: return &Kick;
			default: return NULL;
		}
	}
	
	// 
	// OldShoeSettings
	// 
	void* OldShoeSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 477414507U: return &ScuffHard;
			case 522854583U: return &Scuff;
			case 441706724U: return &Ladder;
			case 781239765U: return &Jump;
			case 285848937U: return &Run;
			case 2203077788U: return &Walk;
			case 197303880U: return &Sprint;
			default: return NULL;
		}
	}
	
	// 
	// MovementShoeSettings
	// 
	void* MovementShoeSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1907513875U: return &HighHeels;
			case 3342715253U: return &Leather;
			case 2369662500U: return &RubberHard;
			case 2628196627U: return &RubberSoft;
			case 2591015954U: return &Bare;
			case 2931296808U: return &HeavyBoots;
			case 2565726809U: return &FlipFlops;
			case 321557861U: return &AudioEventLoudness;
			default: return NULL;
		}
	}
	
	// 
	// FootstepSurfaceSounds
	// 
	void* FootstepSurfaceSounds::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1330140418U: return &Normal;
			default: return NULL;
		}
	}
	
	// 
	// ModelPhysicsParams
	// 
	void* ModelPhysicsParams::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1731791433U: return &NumFeet;
			case 978821372U: return &PedType;
			case 1086881279U: return &FootstepTuningValues;
			case 1897688867U: return &StopSpeedThreshold;
			case 607919704U: return &WalkSpeedThreshold;
			case 2647568827U: return &RunSpeedThreshold;
			case 3953423109U: return &Modes;
			default: return NULL;
		}
	}
	
	// 
	// SkiAudioSettings
	// 
	void* SkiAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 897931320U: return &StraightSound;
			case 3786528496U: return &MinSpeed;
			case 3397946722U: return &MaxSpeed;
			case 2245842897U: return &TurnSound;
			case 1846802912U: return &MinTurn;
			case 209332143U: return &MaxTurn;
			case 2528949472U: return &TurnEdgeSound;
			case 2097605323U: return &MinSlopeEdge;
			case 3752399777U: return &MaxSlopeEdge;
			default: return NULL;
		}
	}
	
	// 
	// RadioStationList
	// 
	void* RadioStationList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3661976881U: return &Station;
			default: return NULL;
		}
	}
	
	// 
	// RadioStationSettings
	// 
	void* RadioStationSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1167033931U: return &Order;
			case 1213155056U: return &NextStationSettingsPtr;
			case 1101519703U: return &Genre;
			case 3370917842U: return &AmbientRadioVol;
			case 3873537812U: return &Name;
			case 3371707334U: return &TrackList;
			default: return NULL;
		}
	}
	
	// 
	// RadioStationTrackList
	// 
	void* RadioStationTrackList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2052871693U: return &Category;
			case 3297881211U: return &NextValidSelectionTime;
			case 462322954U: return &HistoryWriteIndex;
			case 1481830564U: return &HistorySpace;
			case 3571593564U: return &TotalNumTracks;
			case 2150600713U: return &NextTrackListPtr;
			case 875912051U: return &Track;
			default: return NULL;
		}
	}
	
	// 
	// RadioTrackCategoryData
	// 
	void* RadioTrackCategoryData::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2052871693U: return &Category;
			default: return NULL;
		}
	}
	
	// 
	// ScannerCrimeReport
	// 
	void* ScannerCrimeReport::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2750303358U: return &GenericInstructionSndRef;
			case 2555482247U: return &PedsAroundInstructionSndRef;
			case 2947412174U: return &PoliceAroundInstructionSndRef;
			case 3103380746U: return &AcknowledgeSituationProbability;
			case 914466909U: return &SmallCrimeSoundRef;
			case 3313450509U: return &CrimeSet;
			default: return NULL;
		}
	}
	
	// 
	// PedRaceToPVG
	// 
	void* PedRaceToPVG::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 900877504U: return &Universal;
			case 2270822857U: return &White;
			case 705942170U: return &Black;
			case 3348467274U: return &Chinese;
			case 1838435905U: return &Latino;
			case 3792117558U: return &Arab;
			case 2548187204U: return &Balkan;
			case 3835957381U: return &Jamaican;
			case 425250019U: return &Korean;
			case 2247455874U: return &Italian;
			case 836363293U: return &Pakistani;
			case 1707295989U: return &FriendPVGs;
			default: return NULL;
		}
	}
	
	// 
	// PedVoiceGroups
	// 
	void* PedVoiceGroups::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2139114523U: return &PVGType;
			case 3937316262U: return &PVGBits;
			case 486202179U: return &VoicePriority;
			case 3122571958U: return &RingtoneSounds;
			case 2643594138U: return &PrimaryVoice;
			case 4053852356U: return &MiniVoice;
			case 3741071610U: return &GangVoice;
			case 846435479U: return &BackupPVG;
			default: return NULL;
		}
	}
	
	// 
	// StaticEmitterList
	// 
	void* StaticEmitterList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1237255196U: return &Emitter;
			default: return NULL;
		}
	}
	
	// 
	// ScriptedScannerLine
	// 
	void* ScriptedScannerLine::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2562571886U: return &Phrase;
			default: return NULL;
		}
	}
	
	// 
	// ScannerArea
	// 
	void* ScannerArea::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1730846581U: return &ConjunctiveSound;
			case 3897603058U: return &ScannerAreaBits;
			default: return NULL;
		}
	}
	
	// 
	// ScannerSpecificLocation
	// 
	void* ScannerSpecificLocation::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 18243940U: return &Position;
			case 1337695475U: return &Radius;
			case 4273980329U: return &ProbOfPlaying;
			case 2499414043U: return &Sounds;
			default: return NULL;
		}
	}
	
	// 
	// ScannerSpecificLocationList
	// 
	void* ScannerSpecificLocationList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4223740370U: return &Location;
			default: return NULL;
		}
	}
	
	// 
	// AmbientZone
	// 
	void* AmbientZone::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3829206493U: return &Shape;
			case 3209132312U: return &ActivationZone;
			case 626573714U: return &PositioningZone;
			case 3405967894U: return &BuiltUpFactor;
			case 700864904U: return &MinPedDensity;
			case 2398399515U: return &MaxPedDensity;
			case 1722742053U: return &PedDensityTOD;
			case 201364023U: return &PedDensityScalar;
			case 2794172623U: return &MaxWindInfluence;
			case 2217728328U: return &MinWindInfluence;
			case 2103663226U: return &WindElevationSounds;
			case 312810653U: return &EnvironmentRule;
			case 4095565914U: return &AudioScene;
			case 2567908128U: return &UnderwaterCreakFactor;
			case 1260432354U: return &PedWallaSettings;
			case 72404675U: return &RandomisedRadioSettings;
			case 3256966771U: return &NumRulesToPlay;
			case 3093000617U: return &ZoneWaterCalculation;
			case 2828285989U: return &Rule;
			case 1657910547U: return &DirAmbience;
			default: return NULL;
		}
	}
	
	// 
	// AmbientRule
	// 
	void* AmbientRule::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 30976278U: return &ExplicitSpawnPosition;
			case 3255258383U: return &Sound;
			case 2052871693U: return &Category;
			case 1898878336U: return &LastPlayTime;
			case 559739813U: return &DynamicBankID;
			case 2541569178U: return &DynamicSlotType;
			case 2618193740U: return &Weight;
			case 1256197303U: return &MinDist;
			case 2707249109U: return &MaxDist;
			case 4043651850U: return &MinTimeMinutes;
			case 1589106051U: return &MaxTimeMinutes;
			case 1990439652U: return &MinRepeatTime;
			case 3513352125U: return &MinRepeatTimeVariance;
			case 4253122840U: return &SpawnHeight;
			case 2283640460U: return &ExplicitSpawnPositionUsage;
			case 874287841U: return &MaxLocalInstances;
			case 4203645560U: return &MaxGlobalInstances;
			case 2204651633U: return &BlockabilityFactor;
			case 1718779422U: return &MaxPathDepth;
			case 1771412351U: return &Condition;
			default: return NULL;
		}
	}
	
	// 
	// AmbientZoneList
	// 
	void* AmbientZoneList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4176651683U: return &Zone;
			default: return NULL;
		}
	}
	
	// 
	// AmbientSlotMap
	// 
	void* AmbientSlotMap::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 884774889U: return &SlotMap;
			default: return NULL;
		}
	}
	
	// 
	// AmbientBankMap
	// 
	void* AmbientBankMap::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 331500327U: return &BankMap;
			default: return NULL;
		}
	}
	
	// 
	// EnvironmentRule
	// 
	void* EnvironmentRule::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3837494671U: return &ReverbSmall;
			case 1502413340U: return &ReverbMedium;
			case 3826043230U: return &ReverbLarge;
			case 4061385679U: return &ReverbDamp;
			case 3189039475U: return &EchoDelay;
			case 351979593U: return &EchoDelayVariance;
			case 2187643181U: return &EchoAttenuation;
			case 3601014515U: return &EchoNumber;
			case 3616983920U: return &EchoSoundList;
			case 2600066274U: return &BaseEchoVolumeModifier;
			default: return NULL;
		}
	}
	
	// 
	// TrainStation
	// 
	void* TrainStation::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2675870682U: return &StationNameSound;
			case 2869694323U: return &TransferSound;
			case 812250644U: return &Letter;
			case 972359180U: return &ApproachingStation;
			case 1544465033U: return &SpareSound1;
			case 1366136135U: return &SpareSound2;
			case 2259514710U: return &StationName;
			default: return NULL;
		}
	}
	
	// 
	// InteriorSettings
	// 
	void* InteriorSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2912667242U: return &InteriorWallaSoundSet;
			case 866996157U: return &InteriorReflections;
			case 682491878U: return &Room;
			default: return NULL;
		}
	}
	
	// 
	// InteriorWeaponMetrics
	// 
	void* InteriorWeaponMetrics::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2772381270U: return &Wetness;
			case 831659475U: return &Visability;
			case 1998647084U: return &LPF;
			case 1610825783U: return &Predelay;
			case 2552174939U: return &Hold;
			default: return NULL;
		}
	}
	
	// 
	// InteriorRoom
	// 
	void* InteriorRoom::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 606199094U: return &RoomName;
			case 1952289756U: return &AmbientZone;
			case 3906579498U: return &InteriorType;
			case 3837494671U: return &ReverbSmall;
			case 1502413340U: return &ReverbMedium;
			case 3826043230U: return &ReverbLarge;
			case 1541472937U: return &RoomToneSound;
			case 2503781831U: return &RainType;
			case 2177622122U: return &ExteriorAudibility;
			case 2086647442U: return &RoomOcclusionDamping;
			case 3411771057U: return &NonMarkedPortalOcclusion;
			case 3444509903U: return &DistanceFromPortalForOcclusion;
			case 861810613U: return &DistanceFromPortalFadeDistance;
			case 2387441546U: return &WeaponMetrics;
			case 2912667242U: return &InteriorWallaSoundSet;
			default: return NULL;
		}
	}
	
	// 
	// DoorAudioSettings
	// 
	void* DoorAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2499414043U: return &Sounds;
			case 2401011979U: return &TuningParams;
			case 128923157U: return &MaxOcclusion;
			default: return NULL;
		}
	}
	
	// 
	// DoorTuningParams
	// 
	void* DoorTuningParams::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3652655367U: return &OpenThresh;
			case 1268248508U: return &HeadingThresh;
			case 1869082956U: return &ClosedThresh;
			case 4158314278U: return &SpeedThresh;
			case 3051997133U: return &SpeedScale;
			case 3978974110U: return &HeadingDeltaThreshold;
			case 456112299U: return &AngularVelocityThreshold;
			case 3480820164U: return &DoorType;
			default: return NULL;
		}
	}
	
	// 
	// DoorList
	// 
	void* DoorList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3203940123U: return &Door;
			default: return NULL;
		}
	}
	
	// 
	// ItemAudioSettings
	// 
	void* ItemAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2512225228U: return &WeaponSettings;
			case 3214842735U: return &ContextSettings;
			default: return NULL;
		}
	}
	
	// 
	// ClimbingAudioSettings
	// 
	void* ClimbingAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 184765393U: return &climbLaunch;
			case 2587427858U: return &climbFoot;
			case 86868477U: return &climbKnee;
			case 2299458307U: return &climbScrape;
			case 1681012179U: return &climbHand;
			default: return NULL;
		}
	}
	
	// 
	// ModelAudioCollisionSettings
	// 
	void* ModelAudioCollisionSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3073110700U: return &LastFragTime;
			case 3239955667U: return &MediumIntensity;
			case 710033239U: return &HighIntensity;
			case 2834299720U: return &BreakSound;
			case 1594184241U: return &DestroySound;
			case 2350027720U: return &UprootSound;
			case 3658118503U: return &WindSounds;
			case 1622903735U: return &SwingSound;
			case 2302600266U: return &MinSwingVel;
			case 3445954328U: return &MaxswingVel;
			case 1823280965U: return &RainLoop;
			case 1484099351U: return &ShockwaveSound;
			case 1322561434U: return &RandomAmbient;
			case 2901221551U: return &EntityResonance;
			case 2618193740U: return &Weight;
			case 1701516316U: return &MaterialOverride;
			case 1476480773U: return &FragComponentSetting;
			default: return NULL;
		}
	}
	
	// 
	// TrainAudioSettings
	// 
	void* TrainAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2366638355U: return &DriveTone;
			case 640670275U: return &DriveToneSynth;
			case 998490486U: return &IdleLoop;
			case 975516570U: return &Brakes;
			case 2567485150U: return &BigBrakeRelease;
			case 1856081609U: return &BrakeRelease;
			case 3840839414U: return &WheelDry;
			case 1165310768U: return &AirHornOneShot;
			case 1363012881U: return &BellLoopCrosing;
			case 1586363891U: return &BellLoopEngine;
			case 618972131U: return &AmbientCarriage;
			case 2787561220U: return &AmbientRumble;
			case 4129821811U: return &AmbientGrind;
			case 1542909111U: return &AmbientSqueal;
			case 564427683U: return &CarriagePitchCurve;
			case 4240331286U: return &CarriageVolumeCurve;
			case 2917480161U: return &DriveTonePitchCurve;
			case 2698721153U: return &DriveToneVolumeCurve;
			case 2549756904U: return &DriveToneSynthPitchCurve;
			case 3943124133U: return &DriveToneSynthVolumeCurve;
			case 2602392246U: return &GrindPitchCurve;
			case 819437658U: return &GrindVolumeCurve;
			case 1411914947U: return &TrainIdlePitchCurve;
			case 1518139761U: return &TrainIdleVolumeCurve;
			case 310624670U: return &SquealPitchCurve;
			case 367612471U: return &SquealVolumeCurve;
			case 677714204U: return &ScrapeSpeedVolumeCurve;
			case 2113939143U: return &WheelVolumeCurve;
			case 1347380392U: return &WheelDelayCurve;
			case 2946046756U: return &RumbleVolumeCurve;
			case 3982320975U: return &BrakeVelocityPitchCurve;
			case 4235227119U: return &BrakeVelocityVolumeCurve;
			case 1271121311U: return &BrakeAccelerationPitchCurve;
			case 3502958850U: return &BrakeAccelerationVolumeCurve;
			case 3686273222U: return &TrainApproachTrackRumble;
			case 3894183155U: return &TrackRumbleDistanceToIntensity;
			case 17696724U: return &TrainDistanceToRollOffScale;
			case 2758766474U: return &VehicleCollisions;
			case 1584174103U: return &ShockwaveIntensityScale;
			case 2978489191U: return &ShockwaveRadiusScale;
			default: return NULL;
		}
	}
	
	// 
	// WeatherAudioSettings
	// 
	void* WeatherAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4280043047U: return &Strength;
			case 3027043333U: return &Blustery;
			case 3595169551U: return &Temperature;
			case 3321161819U: return &TimeOfDayAffectsTemperature;
			case 2193470566U: return &WhistleVolumeOffset;
			case 2574208376U: return &WindGust;
			case 4095565914U: return &AudioScene;
			case 1326354745U: return &WindGustEnd;
			case 2963401175U: return &WindSound;
			default: return NULL;
		}
	}
	
	// 
	// WeatherTypeAudioReference
	// 
	void* WeatherTypeAudioReference::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3278419690U: return &Reference;
			default: return NULL;
		}
	}
	
	// 
	// BicycleAudioSettings
	// 
	void* BicycleAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2962145355U: return &ChainLoop;
			case 231941441U: return &SprocketSound;
			case 3331783840U: return &RePedalSound;
			case 86306021U: return &GearChangeSound;
			case 1976066479U: return &RoadNoiseVolumeScale;
			case 2771465468U: return &SkidVolumeScale;
			case 1728118752U: return &SuspensionUp;
			case 3517046579U: return &SuspensionDown;
			case 368667153U: return &MinSuspCompThresh;
			case 4179579467U: return &MaxSuspCompThres;
			case 4026253372U: return &JumpLandSound;
			case 2670324367U: return &DamagedJumpLandSound;
			case 859561792U: return &JumpLandMinThresh;
			case 1730459995U: return &JumpLandMaxThresh;
			case 2758766474U: return &VehicleCollisions;
			case 2676197476U: return &BellSound;
			case 1841096179U: return &BrakeBlock;
			case 1933964100U: return &BrakeBlockWet;
			default: return NULL;
		}
	}
	
	// 
	// PlaneAudioSettings
	// 
	void* PlaneAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 633989335U: return &EngineLoop;
			case 564118867U: return &ExhaustLoop;
			case 998490486U: return &IdleLoop;
			case 2240378870U: return &DistanceLoop;
			case 1936142263U: return &PropellorLoop;
			case 480275554U: return &BankingLoop;
			case 3559198106U: return &EngineConeFrontAngle;
			case 463304722U: return &EngineConeRearAngle;
			case 1255187164U: return &EngineConeAtten;
			case 2917528845U: return &ExhaustConeFrontAngle;
			case 3928066140U: return &ExhaustConeRearAngle;
			case 2832783546U: return &ExhaustConeAtten;
			case 3896287014U: return &PropellorConeFrontAngle;
			case 885307605U: return &PropellorConeRearAngle;
			case 789727688U: return &PropellorConeAtten;
			case 1858065901U: return &EngineThrottleVolumeCurve;
			case 3026335339U: return &EngineThrottlePitchCurve;
			case 3102346705U: return &ExhaustThrottleVolumeCurve;
			case 3783234132U: return &ExhaustThrottlePitchCurve;
			case 1504102791U: return &PropellorThrottleVolumeCurve;
			case 2890428124U: return &PropellorThrottlePitchCurve;
			case 2369522229U: return &IdleThrottleVolumeCurve;
			case 1474704903U: return &IdleThrottlePitchCurve;
			case 2520424261U: return &DistanceThrottleVolumeCurve;
			case 3320333266U: return &DistanceThrottlePitchCurve;
			case 3191276178U: return &DistanceVolumeCurve;
			case 1282707965U: return &StallWarning;
			case 3272044876U: return &DoorOpen;
			case 2239999389U: return &DoorClose;
			case 101410581U: return &DoorLimit;
			case 2672598327U: return &LandingGearDeploy;
			case 104997261U: return &LandingGearRetract;
			case 652097730U: return &Ignition;
			case 1198712668U: return &TyreSqueal;
			case 1478676250U: return &BankingThrottleVolumeCurve;
			case 235430267U: return &BankingThrottlePitchCurve;
			case 1515700507U: return &BankingAngleVolCurve;
			case 25491694U: return &AfterburnerLoop;
			case 2226131662U: return &BankingStyle;
			case 753219423U: return &AfterburnerThrottleVolCurve;
			case 2803085325U: return &EngineSynthDef;
			case 3125631121U: return &EngineSynthPreset;
			case 2186643980U: return &ExhaustSynthDef;
			case 2272656214U: return &ExhaustSynthPreset;
			case 3101393458U: return &EngineSubmixVoice;
			case 1509277591U: return &ExhaustSubmixVoice;
			case 74345395U: return &PropellorBreakOneShot;
			case 2907915506U: return &WindNoise;
			case 592400765U: return &PeelingPitchCurve;
			case 3781447698U: return &DivingFactor;
			case 2404978411U: return &MaxDivePitch;
			case 661616519U: return &DiveAirSpeedThreshold;
			case 1425889422U: return &DivingRateApproachingGround;
			case 2330565506U: return &PeelingAfterburnerPitchScalingFactor;
			case 1540408273U: return &Rudder;
			case 921044531U: return &Aileron;
			case 2552803359U: return &Elevator;
			case 876539205U: return &DoorStartOpen;
			case 160196471U: return &DoorStartClose;
			case 2758766474U: return &VehicleCollisions;
			case 659863331U: return &FireAudio;
			case 3432463022U: return &EngineMissFire;
			case 3247487561U: return &IsRealLODRange;
			case 1728118752U: return &SuspensionUp;
			case 3517046579U: return &SuspensionDown;
			case 368667153U: return &MinSuspCompThresh;
			case 4179579467U: return &MaxSuspCompThres;
			case 2220852777U: return &DamageEngineSpeedVolumeCurve;
			case 1396017933U: return &DamageEngineSpeedPitchCurve;
			case 931286371U: return &DamageHealthVolumeCurve;
			case 2571905848U: return &TurbineWindDown;
			case 2544024557U: return &JetDamageLoop;
			case 3930386555U: return &AfterburnerThrottlePitchCurve;
			case 2257460430U: return &NPCEngineSmoothAmount;
			case 462390560U: return &FlybySound;
			case 2037158225U: return &DivingSound;
			case 3138098020U: return &DivingSoundPitchFactor;
			case 4187016097U: return &DivingSoundVolumeFactor;
			case 3855340591U: return &MaxDiveSoundPitch;
			case 3074722591U: return &RadioType;
			case 3489610907U: return &RadioGenre;
			case 660932672U: return &AircraftWarningSeriousDamageThresh;
			case 2157826416U: return &AircraftWarningCriticalDamageThresh;
			case 3472440331U: return &AircraftWarningMaxSpeed;
			case 3163456940U: return &SimpleSoundForLoading;
			case 1912150406U: return &FlyAwaySound;
			case 2669004390U: return &VehicleRainSound;
			case 38374235U: return &VehicleRainSoundInterior;
			case 4139988033U: return &CabinToneLoop;
			case 4177999727U: return &WaveHitSound;
			case 1578326034U: return &WaveHitBigAirSound;
			case 637873149U: return &BigAirMinTime;
			case 2538358285U: return &LeftWaterSound;
			case 2082667583U: return &IdleHullSlapLoop;
			case 391826909U: return &IdleHullSlapSpeedToVol;
			case 939117874U: return &WaterTurbulenceSound;
			default: return NULL;
		}
	}
	
	// 
	// ConductorsDynamicMixingReference
	// 
	void* ConductorsDynamicMixingReference::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3278419690U: return &Reference;
			default: return NULL;
		}
	}
	
	// 
	// StemMix
	// 
	void* StemMix::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1792550253U: return &Stem1Volume;
			case 3212971628U: return &Stem2Volume;
			case 1338824424U: return &Stem3Volume;
			case 2165958370U: return &Stem4Volume;
			case 1200489798U: return &Stem5Volume;
			case 1752349510U: return &Stem6Volume;
			case 136364832U: return &Stem7Volume;
			case 3042635121U: return &Stem8Volume;
			default: return NULL;
		}
	}
	
	// 
	// MusicAction
	// 
	void* MusicAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1468211079U: return &Constrain;
			case 3842672783U: return &TimingConstraints;
			case 3657885768U: return &DelayTime;
			default: return NULL;
		}
	}
	
	// 
	// InteractiveMusicMood
	// 
	void* InteractiveMusicMood::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 900648510U: return &FadeInTime;
			case 215951064U: return &FadeOutTime;
			case 3063400198U: return &AmbMusicDuckingVol;
			case 1105000082U: return &StemMixes;
			default: return NULL;
		}
	}
	
	// 
	// StartTrackAction
	// 
	void* StartTrackAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3672532947U: return &Song;
			case 2671123052U: return &Mood;
			case 2111060667U: return &VolumeOffset;
			case 900648510U: return &FadeInTime;
			case 215951064U: return &FadeOutTime;
			case 1120450488U: return &StartOffsetScalar;
			case 908212487U: return &LastSong;
			case 4022644705U: return &AltSongs;
			default: return NULL;
		}
	}
	
	// 
	// StopTrackAction
	// 
	void* StopTrackAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3571135883U: return &FadeTime;
			default: return NULL;
		}
	}
	
	// 
	// SetMoodAction
	// 
	void* SetMoodAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2671123052U: return &Mood;
			case 2111060667U: return &VolumeOffset;
			case 900648510U: return &FadeInTime;
			case 215951064U: return &FadeOutTime;
			default: return NULL;
		}
	}
	
	// 
	// MusicEvent
	// 
	void* MusicEvent::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2200808865U: return &Actions;
			default: return NULL;
		}
	}
	
	// 
	// StartOneShotAction
	// 
	void* StartOneShotAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3255258383U: return &Sound;
			case 1610825783U: return &Predelay;
			case 900648510U: return &FadeInTime;
			case 215951064U: return &FadeOutTime;
			case 2014659510U: return &SyncMarker;
			default: return NULL;
		}
	}
	
	// 
	// BeatConstraint
	// 
	void* BeatConstraint::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1071756656U: return &ValidSixteenths;
			default: return NULL;
		}
	}
	
	// 
	// BarConstraint
	// 
	void* BarConstraint::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1635686023U: return &PatternLength;
			case 3772204491U: return &ValidBar;
			default: return NULL;
		}
	}
	
	// 
	// DirectionalAmbience
	// 
	void* DirectionalAmbience::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 574158443U: return &SoundNorth;
			case 3541483613U: return &SoundEast;
			case 1145003772U: return &SoundSouth;
			case 1867903688U: return &SoundWest;
			case 87005945U: return &VolumeSmoothing;
			case 2261936987U: return &TimeToVolume;
			case 3680344757U: return &OcclusionToVol;
			case 4257109565U: return &HeightToCutOff;
			case 3309368181U: return &OcclusionToCutOff;
			case 866431515U: return &BuiltUpFactorToVol;
			case 1790801171U: return &BuildingDensityToVol;
			case 827179581U: return &TreeDensityToVol;
			case 3853734990U: return &WaterFactorToVol;
			case 1672898567U: return &InstanceVolumeScale;
			case 2474615951U: return &HeightAboveBlanketToVol;
			case 1520965695U: return &HighwayFactorToVol;
			case 2704401192U: return &VehicleCountToVol;
			case 757022257U: return &MaxDistanceOutToSea;
			default: return NULL;
		}
	}
	
	// 
	// GunfightConductorIntensitySettings
	// 
	void* GunfightConductorIntensitySettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2896068149U: return &Common;
			case 2198904989U: return &GoingIntoCover;
			case 3664265897U: return &RunningAway;
			case 1501702972U: return &OpenSpace;
			case 1288170106U: return &ObjectsMediumIntensity;
			case 659806142U: return &ObjectsHighIntensity;
			case 3798416934U: return &VehiclesMediumIntensity;
			case 1117901934U: return &VehiclesHighIntensity;
			case 3315307522U: return &GroundMediumIntensity;
			case 965651415U: return &GroundHighIntensity;
			default: return NULL;
		}
	}
	
	// 
	// AnimalParams
	// 
	void* AnimalParams::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2955172325U: return &BasePriority;
			case 3807903639U: return &MinFarDistance;
			case 916800390U: return &MaxDistanceToBankLoad;
			case 2771878934U: return &MaxSoundInstances;
			case 828747869U: return &Type;
			case 2432289282U: return &ScrapeVolCurve;
			case 2022363501U: return &ScrapePitchCurve;
			case 3927867699U: return &BigVehicleImpact;
			case 2090855421U: return &VehicleSpeedForBigImpact;
			case 3637496192U: return &RunOverSound;
			case 1454968940U: return &Context;
			default: return NULL;
		}
	}
	
	// 
	// AnimalVocalAnimTrigger
	// 
	void* AnimalVocalAnimTrigger::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4134758435U: return &AnimTriggers;
			default: return NULL;
		}
	}
	
	// 
	// ScannerVoiceParams
	// 
	void* ScannerVoiceParams::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3894741416U: return &NoPrefix;
			case 2582804816U: return &Bright;
			case 1118124053U: return &Light;
			case 1563321438U: return &Dark;
			case 1039906433U: return &ExtraPrefix;
			default: return NULL;
		}
	}
	
	// 
	// ScannerVehicleParams
	// 
	void* ScannerVehicleParams::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 953116472U: return &Voice;
			default: return NULL;
		}
	}
	
	// 
	// AudioRoadInfo
	// 
	void* AudioRoadInfo::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 7206313U: return &RoadName;
			case 3730747630U: return &TyreBumpDistance;
			default: return NULL;
		}
	}
	
	// 
	// MicSettingsReference
	// 
	void* MicSettingsReference::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3278419690U: return &Reference;
			default: return NULL;
		}
	}
	
	// 
	// MicrophoneSettings
	// 
	void* MicrophoneSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3227963701U: return &MicType;
			case 1853029082U: return &ListenerParameters;
			default: return NULL;
		}
	}
	
	// 
	// CarRecordingAudioSettings
	// 
	void* CarRecordingAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2152740626U: return &Mixgroup;
			case 1310974598U: return &VehicleModelId;
			case 2915749078U: return &Event;
			case 2442834565U: return &PersistentMixerScenes;
			default: return NULL;
		}
	}
	
	// 
	// CarRecordingList
	// 
	void* CarRecordingList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1257367651U: return &CarRecording;
			default: return NULL;
		}
	}
	
	// 
	// AnimalFootstepSettings
	// 
	void* AnimalFootstepSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2097116996U: return &WalkAndTrot;
			case 495135276U: return &Gallop1;
			case 726746568U: return &Gallop2;
			case 522854583U: return &Scuff;
			case 781239765U: return &Jump;
			case 3814051896U: return &Land;
			case 825310927U: return &LandHard;
			case 1717738788U: return &Clumsy;
			case 4184892989U: return &SlideLoop;
			case 321557861U: return &AudioEventLoudness;
			default: return NULL;
		}
	}
	
	// 
	// AnimalFootstepReference
	// 
	void* AnimalFootstepReference::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2895275491U: return &AnimalReference;
			default: return NULL;
		}
	}
	
	// 
	// ShoeList
	// 
	void* ShoeList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3062789309U: return &Shoe;
			default: return NULL;
		}
	}
	
	// 
	// ClothAudioSettings
	// 
	void* ClothAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3977158288U: return &ImpactSound;
			case 880411867U: return &WalkSound;
			case 1484971987U: return &RunSound;
			case 3810612598U: return &SprintSound;
			case 4117396556U: return &IntoCoverSound;
			case 2211813302U: return &OutOfCoverSound;
			case 2963401175U: return &WindSound;
			case 4023228733U: return &Intensity;
			case 3187768816U: return &PlayerVersion;
			case 2332720293U: return &BulletImpacts;
			case 4149965069U: return &PedRollSound;
			case 4040790219U: return &ScrapeMaterialSettings;
			case 4026253372U: return &JumpLandSound;
			case 235609309U: return &MeleeSwingSound;
			default: return NULL;
		}
	}
	
	// 
	// ClothList
	// 
	void* ClothList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 424802636U: return &Cloth;
			default: return NULL;
		}
	}
	
	// 
	// ExplosionAudioSettings
	// 
	void* ExplosionAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3003268684U: return &ExplosionSound;
			case 3668584922U: return &DebrisSound;
			case 2639194544U: return &DeafeningVolume;
			case 1730565502U: return &DebrisTimeScale;
			case 503169239U: return &DebrisVolume;
			case 675007694U: return &ShockwaveIntensity;
			case 1133738438U: return &ShockwaveDelay;
			case 2741993125U: return &SlowMoExplosionSound;
			case 2922701669U: return &SlowMoExplosionPreSuckSound;
			case 609965365U: return &SlowMoExplosionPreSuckSoundTime;
			case 1115157190U: return &SlowMoExplosionPreSuckMixerScene;
			default: return NULL;
		}
	}
	
	// 
	// GranularEngineAudioSettings
	// 
	void* GranularEngineAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2167226048U: return &MasterVolume;
			case 1624958057U: return &EngineAccel;
			case 1980278391U: return &ExhaustAccel;
			case 48220400U: return &EngineVolume_PreSubmix;
			case 295224812U: return &ExhaustVolume_PreSubmix;
			case 2584040761U: return &AccelVolume_PreSubmix;
			case 1865713312U: return &DecelVolume_PreSubmix;
			case 3995043535U: return &IdleVolume_PreSubmix;
			case 464684215U: return &EngineRevsVolume_PreSubmix;
			case 1042181827U: return &ExhaustRevsVolume_PreSubmix;
			case 3283251828U: return &EngineThrottleVolume_PreSubmix;
			case 3214514150U: return &ExhaustThrottleVolume_PreSubmix;
			case 3747815159U: return &EngineVolume_PostSubmix;
			case 1008957617U: return &ExhaustVolume_PostSubmix;
			case 2439749658U: return &EngineMaxConeAttenuation;
			case 2238563903U: return &ExhaustMaxConeAttenuation;
			case 290685408U: return &EngineRevsVolume_PostSubmix;
			case 1657252136U: return &ExhaustRevsVolume_PostSubmix;
			case 3280258971U: return &EngineThrottleVolume_PostSubmix;
			case 2253190786U: return &ExhaustThrottleVolume_PostSubmix;
			case 1015108938U: return &GearChangeWobbleLength;
			case 1400139679U: return &GearChangeWobbleLengthVariance;
			case 1068089143U: return &GearChangeWobbleSpeed;
			case 1171524526U: return &GearChangeWobbleSpeedVariance;
			case 1860304645U: return &GearChangeWobblePitch;
			case 326180595U: return &GearChangeWobblePitchVariance;
			case 249056960U: return &GearChangeWobbleVolume;
			case 2644182544U: return &GearChangeWobbleVolumeVariance;
			case 2886461750U: return &EngineClutchAttenuation_PostSubmix;
			case 3924099252U: return &ExhaustClutchAttenuation_PostSubmix;
			case 2803085325U: return &EngineSynthDef;
			case 3125631121U: return &EngineSynthPreset;
			case 2186643980U: return &ExhaustSynthDef;
			case 2272656214U: return &ExhaustSynthPreset;
			case 1917956813U: return &NPCEngineAccel;
			case 3959037051U: return &NPCExhaustAccel;
			case 1401766554U: return &RevLimiterPopSound;
			case 2933926546U: return &MinRPMOverride;
			case 3621239376U: return &MaxRPMOverride;
			case 3101393458U: return &EngineSubmixVoice;
			case 1509277591U: return &ExhaustSubmixVoice;
			case 1645517496U: return &ExhaustProximityVolume_PostSubmix;
			case 1946931157U: return &RevLimiterGrainsToPlay;
			case 2842888292U: return &RevLimiterGrainsToSkip;
			case 872911644U: return &SynchronisedSynth;
			case 342118328U: return &UpgradedEngineVolumeBoost_PostSubmix;
			case 1918450217U: return &UpgradedEngineSynthDef;
			case 2726392985U: return &UpgradedEngineSynthPreset;
			case 1963984898U: return &UpgradedExhaustVolumeBoost_PostSubmix;
			case 1559345364U: return &UpgradedExhaustSynthDef;
			case 1079773534U: return &UpgradedExhaustSynthPreset;
			case 2937547181U: return &DamageSynthHashList;
			case 4160891954U: return &UpgradedRevLimiterPop;
			case 2200897428U: return &EngineIdleVolume_PostSubmix;
			case 3461920U: return &ExhaustIdleVolume_PostSubmix;
			case 4095445808U: return &StartupRevsVolumeBoostEngine_PostSubmix;
			case 468934733U: return &StartupRevsVolumeBoostExhaust_PostSubmix;
			case 2809829712U: return &RevLimiterApplyType;
			case 3726753987U: return &RevLimiterVolumeCut;
			default: return NULL;
		}
	}
	
	// 
	// ShoreLineAudioSettings
	// 
	void* ShoreLineAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 83004555U: return &ActivationBox;
			default: return NULL;
		}
	}
	
	// 
	// ShoreLinePoolAudioSettings
	// 
	void* ShoreLinePoolAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 339556685U: return &WaterLappingMinDelay;
			case 2423874721U: return &WaterLappingMaxDelay;
			case 864797781U: return &WaterSplashMinDelay;
			case 149459468U: return &WaterSplashMaxDelay;
			case 3136094255U: return &FirstQuadIndex;
			case 869438268U: return &SecondQuadIndex;
			case 2960104246U: return &ThirdQuadIndex;
			case 1493048013U: return &FourthQuadIndex;
			case 208247494U: return &SmallestDistanceToPoint;
			case 529136090U: return &ShoreLinePoints;
			default: return NULL;
		}
	}
	
	// 
	// ShoreLineLakeAudioSettings
	// 
	void* ShoreLineLakeAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3470955809U: return &NextShoreline;
			case 2590314548U: return &LakeSize;
			case 529136090U: return &ShoreLinePoints;
			default: return NULL;
		}
	}
	
	// 
	// ShoreLineRiverAudioSettings
	// 
	void* ShoreLineRiverAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3470955809U: return &NextShoreline;
			case 1737640805U: return &RiverType;
			case 2193567634U: return &DefaultHeight;
			case 529136090U: return &ShoreLinePoints;
			default: return NULL;
		}
	}
	
	// 
	// ShoreLineOceanAudioSettings
	// 
	void* ShoreLineOceanAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1747203931U: return &OceanType;
			case 2771102585U: return &OceanDirection;
			case 3470955809U: return &NextShoreline;
			case 2998733585U: return &WaveStartDPDistance;
			case 2707787969U: return &WaveStartHeight;
			case 3817178525U: return &WaveBreaksDPDistance;
			case 2870241882U: return &WaveBreaksHeight;
			case 124099183U: return &WaveEndDPDistance;
			case 705476001U: return &WaveEndHeight;
			case 4039297646U: return &RecedeHeight;
			case 529136090U: return &ShoreLinePoints;
			default: return NULL;
		}
	}
	
	// 
	// ShoreLineList
	// 
	void* ShoreLineList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 102243409U: return &ShoreLines;
			default: return NULL;
		}
	}
	
	// 
	// RadioTrackSettings
	// 
	void* RadioTrackSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3255258383U: return &Sound;
			case 2023011857U: return &History;
			case 1754157472U: return &StartOffset;
			default: return NULL;
		}
	}
	
	// 
	// ModelFootStepTuning
	// 
	void* ModelFootStepTuning::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1418894417U: return &FootstepPitchRatioMin;
			case 2566050036U: return &FootstepPitchRatioMax;
			case 2232482575U: return &InLeftPocketProbability;
			case 1191890056U: return &HasKeysProbability;
			case 3187648778U: return &HasMoneyProbability;
			case 3953423109U: return &Modes;
			default: return NULL;
		}
	}
	
	// 
	// GranularEngineSet
	// 
	void* GranularEngineSet::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4266454861U: return &GranularEngines;
			default: return NULL;
		}
	}
	
	// 
	// RadioDjSpeechAction
	// 
	void* RadioDjSpeechAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3661976881U: return &Station;
			case 2052871693U: return &Category;
			default: return NULL;
		}
	}
	
	// 
	// SilenceConstraint
	// 
	void* SilenceConstraint::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3051409678U: return &MinimumDuration;
			default: return NULL;
		}
	}
	
	// 
	// ReflectionsSettings
	// 
	void* ReflectionsSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 4101260844U: return &MinDelay;
			case 2770162773U: return &MaxDelay;
			case 3721933346U: return &DelayTimeScalar;
			case 107473829U: return &DelayTimeAddition;
			case 4222943029U: return &EnterSound;
			case 2557614410U: return &ExitSound;
			case 3967226431U: return &SubmixVoice;
			case 980522697U: return &Smoothing;
			case 1121759830U: return &PostSubmixVolumeAttenuation;
			case 2468849602U: return &RollOffScale;
			case 2319381344U: return &FilterMode;
			case 2983470600U: return &FilterFrequencyMin;
			case 2975374341U: return &FilterFrequencyMax;
			case 2936129076U: return &FilterResonanceMin;
			case 3495076769U: return &FilterResonanceMax;
			case 1602159696U: return &FilterBandwidthMin;
			case 2544445596U: return &FilterBandwidthMax;
			case 2030562178U: return &DistanceToFilterInput;
			default: return NULL;
		}
	}
	
	// 
	// AlarmSettings
	// 
	void* AlarmSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3487962935U: return &AlarmLoop;
			case 986970203U: return &AlarmDecayCurve;
			case 2886200929U: return &StopDistance;
			case 303342407U: return &InteriorSettings;
			case 1980270498U: return &BankName;
			case 1082082358U: return &CentrePosition;
			default: return NULL;
		}
	}
	
	// 
	// FadeOutRadioAction
	// 
	void* FadeOutRadioAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3571135883U: return &FadeTime;
			default: return NULL;
		}
	}
	
	// 
	// FadeInRadioAction
	// 
	void* FadeInRadioAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3571135883U: return &FadeTime;
			default: return NULL;
		}
	}
	
	// 
	// ForceRadioTrackAction
	// 
	void* ForceRadioTrackAction::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3661976881U: return &Station;
			case 3150237783U: return &NextIndex;
			case 3371707334U: return &TrackList;
			default: return NULL;
		}
	}
	
	// 
	// SlowMoSettings
	// 
	void* SlowMoSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3771858932U: return &Scene;
			case 342945216U: return &Priority;
			case 3162248117U: return &Release;
			case 1177008383U: return &SlowMoSound;
			default: return NULL;
		}
	}
	
	// 
	// PedScenarioAudioSettings
	// 
	void* PedScenarioAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1425416607U: return &MaxInstances;
			case 3255258383U: return &Sound;
			case 3681398799U: return &SharedOwnershipRadius;
			case 774252845U: return &PropOverride;
			default: return NULL;
		}
	}
	
	// 
	// PortalSettings
	// 
	void* PortalSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 128923157U: return &MaxOcclusion;
			default: return NULL;
		}
	}
	
	// 
	// ElectricEngineAudioSettings
	// 
	void* ElectricEngineAudioSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2167226048U: return &MasterVolume;
			case 2906109075U: return &SpeedLoop;
			case 1638262124U: return &SpeedLoop_MinPitch;
			case 1081372305U: return &SpeedLoop_MaxPitch;
			case 1005599651U: return &SpeedLoop_ThrottleVol;
			case 129551796U: return &BoostLoop;
			case 1861434058U: return &BoostLoop_MinPitch;
			case 436867299U: return &BoostLoop_MaxPitch;
			case 3918190993U: return &BoostLoop_SpinupSpeed;
			case 855916597U: return &BoostLoop_Vol;
			case 1902889865U: return &RevsOffLoop;
			case 1236353322U: return &RevsOffLoop_MinPitch;
			case 1041735566U: return &RevsOffLoop_MaxPitch;
			case 3220609548U: return &RevsOffLoop_Vol;
			case 3111841698U: return &BankLoadSound;
			case 4193771599U: return &EngineStartUp;
			default: return NULL;
		}
	}
	
	// 
	// PlayerBreathingSettings
	// 
	void* PlayerBreathingSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1951266466U: return &TimeBetweenLowRunBreaths;
			case 3208207735U: return &TimeBetweenHighRunBreaths;
			case 1994926600U: return &TimeBetweenExhaustedBreaths;
			case 2277902437U: return &TimeBetweenFinalBreaths;
			case 121743636U: return &MinBreathStateChangeWaitToLow;
			case 4086874223U: return &MaxBreathStateChangeWaitToLow;
			case 4002413159U: return &MinBreathStateChangeLowToHighFromWait;
			case 2410456203U: return &MaxBreathStateChangeLowToHighFromWait;
			case 1392733014U: return &MinBreathStateChangeHighToLowFromLow;
			case 3802646017U: return &MaxBreathStateChangeHighToLowFromLow;
			case 1633140756U: return &MinBreathStateChangeLowToHighFromHigh;
			case 1655117320U: return &MaxBreathStateChangeLowToHighFromHigh;
			case 2147654808U: return &MinBreathStateChangeExhaustedToIdleFromLow;
			case 1544879554U: return &MaxBreathStateChangeExhaustedToIdleFromLow;
			case 2717185088U: return &MinBreathStateChangeExhaustedToIdleFromHigh;
			case 70000674U: return &MaxBreathStateChangeExhaustedToIdleFromHigh;
			case 1728836528U: return &MinBreathStateChangeLowToHighFromExhausted;
			case 3807375892U: return &MaxBreathStateChangeLowToHighFromExhausted;
			default: return NULL;
		}
	}
	
	// 
	// PedWallaSpeechSettings
	// 
	void* PedWallaSpeechSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1977886573U: return &SpeechSound;
			case 1494497994U: return &VolumeAboveRMSLevel;
			case 321811651U: return &MaxVolume;
			case 2269180684U: return &PedDensityThreshold;
			case 208694998U: return &SpeechContexts;
			default: return NULL;
		}
	}
	
	// 
	// AircraftWarningSettings
	// 
	void* AircraftWarningSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1433946844U: return &MinTimeBetweenDamageReports;
			case 1392961554U: return &TargetedLock;
			case 584945250U: return &MissleFired;
			case 1563667531U: return &AcquiringTarget;
			case 849682021U: return &TargetAcquired;
			case 2224191378U: return &AllClear;
			case 1703290421U: return &PlaneWarningStall;
			case 1653508370U: return &AltitudeWarningLow;
			case 3184961426U: return &AltitudeWarningHigh;
			case 625522311U: return &Engine_1_Fire;
			case 1942605693U: return &Engine_2_Fire;
			case 3789970102U: return &Engine_3_Fire;
			case 3692619671U: return &Engine_4_Fire;
			case 1064066521U: return &DamagedSerious;
			case 2640146442U: return &DamagedCritical;
			case 2021176260U: return &Overspeed;
			case 3686698412U: return &Terrain;
			case 3852043287U: return &PullUp;
			case 2216740647U: return &LowFuel;
			default: return NULL;
		}
	}
	
	// 
	// PedWallaSettingsList
	// 
	void* PedWallaSettingsList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 515903735U: return &PedWallaSettingsInstance;
			default: return NULL;
		}
	}
	
	// 
	// CopDispatchInteractionSettings
	// 
	void* CopDispatchInteractionSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1541358933U: return &MinTimeBetweenInteractions;
			case 2723298354U: return &MinTimeBetweenInteractionsVariance;
			case 1092227664U: return &FirstLinePredelay;
			case 145078968U: return &FirstLinePredelayVariance;
			case 2250583678U: return &SecondLinePredelay;
			case 1170427095U: return &SecondLinePredelayVariance;
			case 3481742673U: return &ScannerPredelay;
			case 1192890416U: return &ScannerPredelayVariance;
			case 3611913619U: return &MinTimeBetweenSpottedAndVehicleLinePlays;
			case 207197978U: return &MinTimeBetweenSpottedAndVehicleLinePlaysVariance;
			case 752998506U: return &MaxTimeAfterVehicleChangeSpottedLineCanPlay;
			case 3547701946U: return &TimePassedSinceLastLineToForceVoiceChange;
			case 2174669694U: return &NumCopsKilledToForceVoiceChange;
			case 2739968360U: return &SuspectCrashedVehicle;
			case 4252383958U: return &SuspectEnteredFreeway;
			case 2041442573U: return &SuspectEnteredMetro;
			case 1303022914U: return &SuspectIsInCar;
			case 2826476036U: return &SuspectIsOnFoot;
			case 2524798966U: return &SuspectIsOnMotorcycle;
			case 3426545118U: return &SuspectLeftFreeway;
			case 1280417158U: return &RequestBackup;
			case 2189002028U: return &AcknowledgeSituation;
			case 2212448278U: return &UnitRespondingDispatch;
			case 4137309118U: return &RequestGuidanceDispatch;
			case 588789497U: return &HeliMaydayDispatch;
			case 1725902742U: return &ShotAtHeli;
			case 821932352U: return &HeliApproachingDispatch;
			default: return NULL;
		}
	}
	
	// 
	// RadioTrackTextIds
	// 
	void* RadioTrackTextIds::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 874443203U: return &TextId;
			default: return NULL;
		}
	}
	
	// 
	// RandomisedRadioEmitterSettings
	// 
	void* RandomisedRadioEmitterSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 3157130933U: return &VehicleEmitterBias;
			case 3140654359U: return &StaticEmitterConfig;
			case 1149507373U: return &VehicleEmitterConfig;
			default: return NULL;
		}
	}
	
	// 
	// TennisVocalizationSettings
	// 
	void* TennisVocalizationSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2877542799U: return &Lite;
			case 2837476759U: return &Med;
			case 353052819U: return &Strong;
			default: return NULL;
		}
	}
	
	// 
	// DoorAudioSettingsLink
	// 
	void* DoorAudioSettingsLink::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 492952109U: return &DoorAudioSettings;
			default: return NULL;
		}
	}
	
	// 
	// SportsCarRevsSettings
	// 
	void* SportsCarRevsSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2681108550U: return &EngineVolumeBoost;
			case 5902930U: return &ExhaustVolumeBoost;
			case 1104981378U: return &RollOffBoost;
			case 1968864193U: return &MinTriggerTime;
			case 1990439652U: return &MinRepeatTime;
			case 2569034838U: return &AttackTimeScalar;
			case 2873099366U: return &ReleaseTimeScalar;
			case 2010550034U: return &SmallReverbSend;
			case 4126590696U: return &MediumReverbSend;
			case 1958040013U: return &LargeReverbSend;
			case 3651658898U: return &JunctionTriggerSpeed;
			case 3413827017U: return &JunctionStopSpeed;
			case 1745302665U: return &JunctionMinDistance;
			case 3312932101U: return &JunctionMaxDistance;
			case 3842900354U: return &PassbyTriggerSpeed;
			case 85406188U: return &PassbyStopSpeed;
			case 2540841670U: return &PassbyMinDistance;
			case 3165047892U: return &PassbyMaxDistance;
			case 3510377012U: return &PassbyLookaheadTime;
			case 2950103311U: return &ClusterTriggerDistance;
			case 1845937928U: return &ClusterTriggerSpeed;
			default: return NULL;
		}
	}
	
	// 
	// FoliageSettings
	// 
	void* FoliageSettings::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 285848937U: return &Run;
			case 197303880U: return &Sprint;
			case 2203077788U: return &Walk;
			default: return NULL;
		}
	}
	
	// 
	// ReplayRadioStationTrackList
	// 
	void* ReplayRadioStationTrackList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 875912051U: return &Track;
			default: return NULL;
		}
	}
	
	// 
	// MacsModelOverrideList
	// 
	void* MacsModelOverrideList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2320145025U: return &Entry;
			default: return NULL;
		}
	}
	
	// 
	// MacsModelOverrideListBig
	// 
	void* MacsModelOverrideListBig::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 2320145025U: return &Entry;
			default: return NULL;
		}
	}
	
	// 
	// SoundFieldDefinition
	// 
	void* SoundFieldDefinition::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 1237255196U: return &Emitter;
			case 181570030U: return &HullIndices;
			default: return NULL;
		}
	}
	
	// 
	// GameObjectHashList
	// 
	void* GameObjectHashList::GetFieldPtr(const rage::u32 fieldNameHash)
	{
		switch(fieldNameHash)
		{
			case 281316646U: return &GameObjectHashes;
			default: return NULL;
		}
	}
	
	// 
	// Enumeration Conversion
	// 
	
	// PURPOSE - Convert a RadioGenre value into its string representation.
	const char* RadioGenre_ToString(const RadioGenre value)
	{
		switch(value)
		{
			case RADIO_GENRE_OFF: return "RADIO_GENRE_OFF";
			case RADIO_GENRE_MODERN_ROCK: return "RADIO_GENRE_MODERN_ROCK";
			case RADIO_GENRE_CLASSIC_ROCK: return "RADIO_GENRE_CLASSIC_ROCK";
			case RADIO_GENRE_POP: return "RADIO_GENRE_POP";
			case RADIO_GENRE_MODERN_HIPHOP: return "RADIO_GENRE_MODERN_HIPHOP";
			case RADIO_GENRE_CLASSIC_HIPHOP: return "RADIO_GENRE_CLASSIC_HIPHOP";
			case RADIO_GENRE_PUNK: return "RADIO_GENRE_PUNK";
			case RADIO_GENRE_LEFT_WING_TALK: return "RADIO_GENRE_LEFT_WING_TALK";
			case RADIO_GENRE_RIGHT_WING_TALK: return "RADIO_GENRE_RIGHT_WING_TALK";
			case RADIO_GENRE_COUNTRY: return "RADIO_GENRE_COUNTRY";
			case RADIO_GENRE_DANCE: return "RADIO_GENRE_DANCE";
			case RADIO_GENRE_MEXICAN: return "RADIO_GENRE_MEXICAN";
			case RADIO_GENRE_REGGAE: return "RADIO_GENRE_REGGAE";
			case RADIO_GENRE_JAZZ: return "RADIO_GENRE_JAZZ";
			case RADIO_GENRE_MOTOWN: return "RADIO_GENRE_MOTOWN";
			case RADIO_GENRE_SURF: return "RADIO_GENRE_SURF";
			case RADIO_GENRE_UNSPECIFIED: return "RADIO_GENRE_UNSPECIFIED";
			case NUM_RADIOGENRE: return "NUM_RADIOGENRE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a RadioGenre value.
	RadioGenre RadioGenre_Parse(const char* str, const RadioGenre defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 346993750U: return RADIO_GENRE_OFF;
			case 816350179U: return RADIO_GENRE_MODERN_ROCK;
			case 1807059638U: return RADIO_GENRE_CLASSIC_ROCK;
			case 3941384435U: return RADIO_GENRE_POP;
			case 3960892023U: return RADIO_GENRE_MODERN_HIPHOP;
			case 2759786511U: return RADIO_GENRE_CLASSIC_HIPHOP;
			case 4072438912U: return RADIO_GENRE_PUNK;
			case 2952255114U: return RADIO_GENRE_LEFT_WING_TALK;
			case 2399123532U: return RADIO_GENRE_RIGHT_WING_TALK;
			case 755781297U: return RADIO_GENRE_COUNTRY;
			case 3780942666U: return RADIO_GENRE_DANCE;
			case 759377825U: return RADIO_GENRE_MEXICAN;
			case 1630929035U: return RADIO_GENRE_REGGAE;
			case 3944434105U: return RADIO_GENRE_JAZZ;
			case 1177006191U: return RADIO_GENRE_MOTOWN;
			case 661235411U: return RADIO_GENRE_SURF;
			case 1378379012U: return RADIO_GENRE_UNSPECIFIED;
			case 293152990U:
			case 1305617752U: return NUM_RADIOGENRE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientRadioLeakage value into its string representation.
	const char* AmbientRadioLeakage_ToString(const AmbientRadioLeakage value)
	{
		switch(value)
		{
			case LEAKAGE_BASSY_LOUD: return "LEAKAGE_BASSY_LOUD";
			case LEAKAGE_BASSY_MEDIUM: return "LEAKAGE_BASSY_MEDIUM";
			case LEAKAGE_MIDS_LOUD: return "LEAKAGE_MIDS_LOUD";
			case LEAKAGE_MIDS_MEDIUM: return "LEAKAGE_MIDS_MEDIUM";
			case LEAKAGE_MIDS_QUIET: return "LEAKAGE_MIDS_QUIET";
			case LEAKAGE_CRAZY_LOUD: return "LEAKAGE_CRAZY_LOUD";
			case LEAKAGE_MODDED: return "LEAKAGE_MODDED";
			case LEAKAGE_PARTYBUS: return "LEAKAGE_PARTYBUS";
			case NUM_AMBIENTRADIOLEAKAGE: return "NUM_AMBIENTRADIOLEAKAGE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientRadioLeakage value.
	AmbientRadioLeakage AmbientRadioLeakage_Parse(const char* str, const AmbientRadioLeakage defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3217461132U: return LEAKAGE_BASSY_LOUD;
			case 1363422946U: return LEAKAGE_BASSY_MEDIUM;
			case 206705273U: return LEAKAGE_MIDS_LOUD;
			case 4133479402U: return LEAKAGE_MIDS_MEDIUM;
			case 1053468995U: return LEAKAGE_MIDS_QUIET;
			case 2315037468U: return LEAKAGE_CRAZY_LOUD;
			case 3057406649U: return LEAKAGE_MODDED;
			case 820279410U: return LEAKAGE_PARTYBUS;
			case 808760177U:
			case 3494013861U: return NUM_AMBIENTRADIOLEAKAGE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a GPSType value into its string representation.
	const char* GPSType_ToString(const GPSType value)
	{
		switch(value)
		{
			case GPS_TYPE_NONE: return "GPS_TYPE_NONE";
			case GPS_TYPE_ANY: return "GPS_TYPE_ANY";
			case NUM_GPSTYPE: return "NUM_GPSTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a GPSType value.
	GPSType GPSType_Parse(const char* str, const GPSType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3759714622U: return GPS_TYPE_NONE;
			case 1345705210U: return GPS_TYPE_ANY;
			case 2017535285U:
			case 3441285914U: return NUM_GPSTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a GPSVoice value into its string representation.
	const char* GPSVoice_ToString(const GPSVoice value)
	{
		switch(value)
		{
			case GPS_VOICE_FEMALE: return "GPS_VOICE_FEMALE";
			case GPS_VOICE_MALE: return "GPS_VOICE_MALE";
			case GPS_VOICE_EXTRA1: return "GPS_VOICE_EXTRA1";
			case GPS_VOICE_EXTRA2: return "GPS_VOICE_EXTRA2";
			case GPS_VOICE_EXTRA3: return "GPS_VOICE_EXTRA3";
			case NUM_GPSVOICE: return "NUM_GPSVOICE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a GPSVoice value.
	GPSVoice GPSVoice_Parse(const char* str, const GPSVoice defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2705685305U: return GPS_VOICE_FEMALE;
			case 948595759U: return GPS_VOICE_MALE;
			case 3272633324U: return GPS_VOICE_EXTRA1;
			case 3619919194U: return GPS_VOICE_EXTRA2;
			case 3863687785U: return GPS_VOICE_EXTRA3;
			case 2244445014U:
			case 2936090773U: return NUM_GPSVOICE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a RadioType value into its string representation.
	const char* RadioType_ToString(const RadioType value)
	{
		switch(value)
		{
			case RADIO_TYPE_NONE: return "RADIO_TYPE_NONE";
			case RADIO_TYPE_NORMAL: return "RADIO_TYPE_NORMAL";
			case RADIO_TYPE_EMERGENCY_SERVICES: return "RADIO_TYPE_EMERGENCY_SERVICES";
			case RADIO_TYPE_NORMAL_OFF_MISSION_AND_MP: return "RADIO_TYPE_NORMAL_OFF_MISSION_AND_MP";
			case NUM_RADIOTYPE: return "NUM_RADIOTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a RadioType value.
	RadioType RadioType_Parse(const char* str, const RadioType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1667136116U: return RADIO_TYPE_NONE;
			case 3853403286U: return RADIO_TYPE_NORMAL;
			case 1414099956U: return RADIO_TYPE_EMERGENCY_SERVICES;
			case 1698889996U: return RADIO_TYPE_NORMAL_OFF_MISSION_AND_MP;
			case 778941178U:
			case 817245996U: return NUM_RADIOTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a VehicleVolumeCategory value into its string representation.
	const char* VehicleVolumeCategory_ToString(const VehicleVolumeCategory value)
	{
		switch(value)
		{
			case VEHICLE_VOLUME_VERY_LOUD: return "VEHICLE_VOLUME_VERY_LOUD";
			case VEHICLE_VOLUME_LOUD: return "VEHICLE_VOLUME_LOUD";
			case VEHICLE_VOLUME_NORMAL: return "VEHICLE_VOLUME_NORMAL";
			case VEHICLE_VOLUME_QUIET: return "VEHICLE_VOLUME_QUIET";
			case VEHICLE_VOLUME_VERY_QUIET: return "VEHICLE_VOLUME_VERY_QUIET";
			case NUM_VEHICLEVOLUMECATEGORY: return "NUM_VEHICLEVOLUMECATEGORY";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a VehicleVolumeCategory value.
	VehicleVolumeCategory VehicleVolumeCategory_Parse(const char* str, const VehicleVolumeCategory defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1160658296U: return VEHICLE_VOLUME_VERY_LOUD;
			case 2744027102U: return VEHICLE_VOLUME_LOUD;
			case 1982688257U: return VEHICLE_VOLUME_NORMAL;
			case 960933225U: return VEHICLE_VOLUME_QUIET;
			case 2411431345U: return VEHICLE_VOLUME_VERY_QUIET;
			case 108461199U:
			case 3656869612U: return NUM_VEHICLEVOLUMECATEGORY;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SpeechVolumeType value into its string representation.
	const char* SpeechVolumeType_ToString(const SpeechVolumeType value)
	{
		switch(value)
		{
			case CONTEXT_VOLUME_NORMAL: return "CONTEXT_VOLUME_NORMAL";
			case CONTEXT_VOLUME_SHOUTED: return "CONTEXT_VOLUME_SHOUTED";
			case CONTEXT_VOLUME_PAIN: return "CONTEXT_VOLUME_PAIN";
			case CONTEXT_VOLUME_MEGAPHONE: return "CONTEXT_VOLUME_MEGAPHONE";
			case CONTEXT_VOLUME_FROM_HELI: return "CONTEXT_VOLUME_FROM_HELI";
			case CONTEXT_VOLUME_FRONTEND: return "CONTEXT_VOLUME_FRONTEND";
			case NUM_SPEECHVOLUMETYPE: return "NUM_SPEECHVOLUMETYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SpeechVolumeType value.
	SpeechVolumeType SpeechVolumeType_Parse(const char* str, const SpeechVolumeType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3138373092U: return CONTEXT_VOLUME_NORMAL;
			case 2506973609U: return CONTEXT_VOLUME_SHOUTED;
			case 551719995U: return CONTEXT_VOLUME_PAIN;
			case 3916803893U: return CONTEXT_VOLUME_MEGAPHONE;
			case 2020432471U: return CONTEXT_VOLUME_FROM_HELI;
			case 3286478184U: return CONTEXT_VOLUME_FRONTEND;
			case 3592535270U:
			case 462138778U: return NUM_SPEECHVOLUMETYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SpeechAudibilityType value into its string representation.
	const char* SpeechAudibilityType_ToString(const SpeechAudibilityType value)
	{
		switch(value)
		{
			case CONTEXT_AUDIBILITY_NORMAL: return "CONTEXT_AUDIBILITY_NORMAL";
			case CONTEXT_AUDIBILITY_CLEAR: return "CONTEXT_AUDIBILITY_CLEAR";
			case CONTEXT_AUDIBILITY_CRITICAL: return "CONTEXT_AUDIBILITY_CRITICAL";
			case CONTEXT_AUDIBILITY_LEAD_IN: return "CONTEXT_AUDIBILITY_LEAD_IN";
			case NUM_SPEECHAUDIBILITYTYPE: return "NUM_SPEECHAUDIBILITYTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SpeechAudibilityType value.
	SpeechAudibilityType SpeechAudibilityType_Parse(const char* str, const SpeechAudibilityType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3341349404U: return CONTEXT_AUDIBILITY_NORMAL;
			case 1943410892U: return CONTEXT_AUDIBILITY_CLEAR;
			case 1643795294U: return CONTEXT_AUDIBILITY_CRITICAL;
			case 3823061069U: return CONTEXT_AUDIBILITY_LEAD_IN;
			case 3763327538U:
			case 3812137742U: return NUM_SPEECHAUDIBILITYTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SpeechRequestedVolumeType value into its string representation.
	const char* SpeechRequestedVolumeType_ToString(const SpeechRequestedVolumeType value)
	{
		switch(value)
		{
			case SPEECH_VOLUME_UNSPECIFIED: return "SPEECH_VOLUME_UNSPECIFIED";
			case SPEECH_VOLUME_NORMAL: return "SPEECH_VOLUME_NORMAL";
			case SPEECH_VOLUME_SHOUTED: return "SPEECH_VOLUME_SHOUTED";
			case SPEECH_VOLUME_FRONTEND: return "SPEECH_VOLUME_FRONTEND";
			case SPEECH_VOLUME_MEGAPHONE: return "SPEECH_VOLUME_MEGAPHONE";
			case SPEECH_VOLUME_HELI: return "SPEECH_VOLUME_HELI";
			case NUM_SPEECHREQUESTEDVOLUMETYPE: return "NUM_SPEECHREQUESTEDVOLUMETYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SpeechRequestedVolumeType value.
	SpeechRequestedVolumeType SpeechRequestedVolumeType_Parse(const char* str, const SpeechRequestedVolumeType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 4290799051U: return SPEECH_VOLUME_UNSPECIFIED;
			case 106337499U: return SPEECH_VOLUME_NORMAL;
			case 1240307675U: return SPEECH_VOLUME_SHOUTED;
			case 1149576308U: return SPEECH_VOLUME_FRONTEND;
			case 3100323980U: return SPEECH_VOLUME_MEGAPHONE;
			case 2934473373U: return SPEECH_VOLUME_HELI;
			case 3865435476U:
			case 1566927157U: return NUM_SPEECHREQUESTEDVOLUMETYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SpeechResolvingFunction value into its string representation.
	const char* SpeechResolvingFunction_ToString(const SpeechResolvingFunction value)
	{
		switch(value)
		{
			case SRF_DEFAULT: return "SRF_DEFAULT";
			case SRF_TIME_OF_DAY: return "SRF_TIME_OF_DAY";
			case SRF_PED_TOUGHNESS: return "SRF_PED_TOUGHNESS";
			case SRF_TARGET_GENDER: return "SRF_TARGET_GENDER";
			case NUM_SPEECHRESOLVINGFUNCTION: return "NUM_SPEECHRESOLVINGFUNCTION";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SpeechResolvingFunction value.
	SpeechResolvingFunction SpeechResolvingFunction_Parse(const char* str, const SpeechResolvingFunction defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1126193305U: return SRF_DEFAULT;
			case 1916925511U: return SRF_TIME_OF_DAY;
			case 2802991316U: return SRF_PED_TOUGHNESS;
			case 3519499252U: return SRF_TARGET_GENDER;
			case 1177501017U:
			case 1166288313U: return NUM_SPEECHRESOLVINGFUNCTION;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an InteriorRainType value into its string representation.
	const char* InteriorRainType_ToString(const InteriorRainType value)
	{
		switch(value)
		{
			case RAIN_TYPE_NONE: return "RAIN_TYPE_NONE";
			case RAIN_TYPE_CORRUGATED_IRON: return "RAIN_TYPE_CORRUGATED_IRON";
			case RAIN_TYPE_PLASTIC: return "RAIN_TYPE_PLASTIC";
			case NUM_INTERIORRAINTYPE: return "NUM_INTERIORRAINTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an InteriorRainType value.
	InteriorRainType InteriorRainType_Parse(const char* str, const InteriorRainType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3770377990U: return RAIN_TYPE_NONE;
			case 3211070090U: return RAIN_TYPE_CORRUGATED_IRON;
			case 3982422819U: return RAIN_TYPE_PLASTIC;
			case 3330509443U:
			case 3766046611U: return NUM_INTERIORRAINTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an InteriorType value into its string representation.
	const char* InteriorType_ToString(const InteriorType value)
	{
		switch(value)
		{
			case INTERIOR_TYPE_NONE: return "INTERIOR_TYPE_NONE";
			case INTERIOR_TYPE_ROAD_TUNNEL: return "INTERIOR_TYPE_ROAD_TUNNEL";
			case INTERIOR_TYPE_SUBWAY_TUNNEL: return "INTERIOR_TYPE_SUBWAY_TUNNEL";
			case INTERIOR_TYPE_SUBWAY_STATION: return "INTERIOR_TYPE_SUBWAY_STATION";
			case INTERIOR_TYPE_SUBWAY_ENTRANCE: return "INTERIOR_TYPE_SUBWAY_ENTRANCE";
			case INTERIOR_TYPE_ABANDONED_WAREHOUSE: return "INTERIOR_TYPE_ABANDONED_WAREHOUSE";
			case NUM_INTERIORTYPE: return "NUM_INTERIORTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an InteriorType value.
	InteriorType InteriorType_Parse(const char* str, const InteriorType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3523751237U: return INTERIOR_TYPE_NONE;
			case 3223400847U: return INTERIOR_TYPE_ROAD_TUNNEL;
			case 2661948260U: return INTERIOR_TYPE_SUBWAY_TUNNEL;
			case 37609878U: return INTERIOR_TYPE_SUBWAY_STATION;
			case 1915809345U: return INTERIOR_TYPE_SUBWAY_ENTRANCE;
			case 4261016463U: return INTERIOR_TYPE_ABANDONED_WAREHOUSE;
			case 3763881386U:
			case 648042610U: return NUM_INTERIORTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a ClimbSounds value into its string representation.
	const char* ClimbSounds_ToString(const ClimbSounds value)
	{
		switch(value)
		{
			case CLIMB_SOUNDS_CONCRETE: return "CLIMB_SOUNDS_CONCRETE";
			case CLIMB_SOUNDS_CHAINLINK: return "CLIMB_SOUNDS_CHAINLINK";
			case CLIMB_SOUNDS_METAL: return "CLIMB_SOUNDS_METAL";
			case CLIMB_SOUNDS_WOOD: return "CLIMB_SOUNDS_WOOD";
			case NUM_CLIMBSOUNDS: return "NUM_CLIMBSOUNDS";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a ClimbSounds value.
	ClimbSounds ClimbSounds_Parse(const char* str, const ClimbSounds defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1186709740U: return CLIMB_SOUNDS_CONCRETE;
			case 2511137999U: return CLIMB_SOUNDS_CHAINLINK;
			case 1090592708U: return CLIMB_SOUNDS_METAL;
			case 4098445428U: return CLIMB_SOUNDS_WOOD;
			case 3762065336U:
			case 3072373801U: return NUM_CLIMBSOUNDS;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AudioWeight value into its string representation.
	const char* AudioWeight_ToString(const AudioWeight value)
	{
		switch(value)
		{
			case AUDIO_WEIGHT_VL: return "AUDIO_WEIGHT_VL";
			case AUDIO_WEIGHT_L: return "AUDIO_WEIGHT_L";
			case AUDIO_WEIGHT_M: return "AUDIO_WEIGHT_M";
			case AUDIO_WEIGHT_H: return "AUDIO_WEIGHT_H";
			case AUDIO_WEIGHT_VH: return "AUDIO_WEIGHT_VH";
			case NUM_AUDIOWEIGHT: return "NUM_AUDIOWEIGHT";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AudioWeight value.
	AudioWeight AudioWeight_Parse(const char* str, const AudioWeight defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3831999085U: return AUDIO_WEIGHT_VL;
			case 4113535094U: return AUDIO_WEIGHT_L;
			case 526804195U: return AUDIO_WEIGHT_M;
			case 3411950804U: return AUDIO_WEIGHT_H;
			case 2807148610U: return AUDIO_WEIGHT_VH;
			case 1041860074U:
			case 1142708144U: return NUM_AUDIOWEIGHT;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientRuleHeight value into its string representation.
	const char* AmbientRuleHeight_ToString(const AmbientRuleHeight value)
	{
		switch(value)
		{
			case AMBIENCE_HEIGHT_RANDOM: return "AMBIENCE_HEIGHT_RANDOM";
			case AMBIENCE_LISTENER_HEIGHT: return "AMBIENCE_LISTENER_HEIGHT";
			case AMBIENCE_WORLD_BLANKET_HEIGHT: return "AMBIENCE_WORLD_BLANKET_HEIGHT";
			case AMBIENCE_HEIGHT_WORLD_BLANKET_HEIGHT_PLUS_EXPLICIT: return "AMBIENCE_HEIGHT_WORLD_BLANKET_HEIGHT_PLUS_EXPLICIT";
			case AMBIENCE_LISTENER_HEIGHT_HEIGHT_PLUS_EXPLICIT: return "AMBIENCE_LISTENER_HEIGHT_HEIGHT_PLUS_EXPLICIT";
			case AMBIENCE_HEIGHT_PZ_FRACTION: return "AMBIENCE_HEIGHT_PZ_FRACTION";
			case NUM_AMBIENTRULEHEIGHT: return "NUM_AMBIENTRULEHEIGHT";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientRuleHeight value.
	AmbientRuleHeight AmbientRuleHeight_Parse(const char* str, const AmbientRuleHeight defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 189421039U: return AMBIENCE_HEIGHT_RANDOM;
			case 2229135064U: return AMBIENCE_LISTENER_HEIGHT;
			case 828632591U: return AMBIENCE_WORLD_BLANKET_HEIGHT;
			case 1409934683U: return AMBIENCE_HEIGHT_WORLD_BLANKET_HEIGHT_PLUS_EXPLICIT;
			case 151548033U: return AMBIENCE_LISTENER_HEIGHT_HEIGHT_PLUS_EXPLICIT;
			case 3889152991U: return AMBIENCE_HEIGHT_PZ_FRACTION;
			case 502209334U:
			case 2547523779U: return NUM_AMBIENTRULEHEIGHT;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientRuleExplicitPositionType value into its string representation.
	const char* AmbientRuleExplicitPositionType_ToString(const AmbientRuleExplicitPositionType value)
	{
		switch(value)
		{
			case RULE_EXPLICIT_SPAWN_POS_DISABLED: return "RULE_EXPLICIT_SPAWN_POS_DISABLED";
			case RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE: return "RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE";
			case RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE: return "RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE";
			case NUM_AMBIENTRULEEXPLICITPOSITIONTYPE: return "NUM_AMBIENTRULEEXPLICITPOSITIONTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientRuleExplicitPositionType value.
	AmbientRuleExplicitPositionType AmbientRuleExplicitPositionType_Parse(const char* str, const AmbientRuleExplicitPositionType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3043984715U: return RULE_EXPLICIT_SPAWN_POS_DISABLED;
			case 1896812814U: return RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE;
			case 1158530773U: return RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE;
			case 743452302U:
			case 2725524289U: return NUM_AMBIENTRULEEXPLICITPOSITIONTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientRuleConditionTypes value into its string representation.
	const char* AmbientRuleConditionTypes_ToString(const AmbientRuleConditionTypes value)
	{
		switch(value)
		{
			case RULE_IF_CONDITION_LESS_THAN: return "RULE_IF_CONDITION_LESS_THAN";
			case RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO: return "RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO";
			case RULE_IF_CONDITION_GREATER_THAN: return "RULE_IF_CONDITION_GREATER_THAN";
			case RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO: return "RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO";
			case RULE_IF_CONDITION_EQUAL_TO: return "RULE_IF_CONDITION_EQUAL_TO";
			case RULE_IF_CONDITION_NOT_EQUAL_TO: return "RULE_IF_CONDITION_NOT_EQUAL_TO";
			case NUM_AMBIENTRULECONDITIONTYPES: return "NUM_AMBIENTRULECONDITIONTYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientRuleConditionTypes value.
	AmbientRuleConditionTypes AmbientRuleConditionTypes_Parse(const char* str, const AmbientRuleConditionTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3076450132U: return RULE_IF_CONDITION_LESS_THAN;
			case 4218483626U: return RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO;
			case 2790853047U: return RULE_IF_CONDITION_GREATER_THAN;
			case 2337762766U: return RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO;
			case 808249294U: return RULE_IF_CONDITION_EQUAL_TO;
			case 3843640725U: return RULE_IF_CONDITION_NOT_EQUAL_TO;
			case 177608954U:
			case 1902875210U: return NUM_AMBIENTRULECONDITIONTYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientRuleBankLoad value into its string representation.
	const char* AmbientRuleBankLoad_ToString(const AmbientRuleBankLoad value)
	{
		switch(value)
		{
			case INFLUENCE_BANK_LOAD: return "INFLUENCE_BANK_LOAD";
			case DONT_INFLUENCE_BANK_LOAD: return "DONT_INFLUENCE_BANK_LOAD";
			case NUM_AMBIENTRULEBANKLOAD: return "NUM_AMBIENTRULEBANKLOAD";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientRuleBankLoad value.
	AmbientRuleBankLoad AmbientRuleBankLoad_Parse(const char* str, const AmbientRuleBankLoad defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1609195171U: return INFLUENCE_BANK_LOAD;
			case 3186437171U: return DONT_INFLUENCE_BANK_LOAD;
			case 4268449283U:
			case 3472827401U: return NUM_AMBIENTRULEBANKLOAD;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a PedTypes value into its string representation.
	const char* PedTypes_ToString(const PedTypes value)
	{
		switch(value)
		{
			case HUMAN: return "HUMAN";
			case DOG: return "DOG";
			case DEER: return "DEER";
			case BOAR: return "BOAR";
			case COYOTE: return "COYOTE";
			case MTLION: return "MTLION";
			case PIG: return "PIG";
			case CHIMP: return "CHIMP";
			case COW: return "COW";
			case ROTTWEILER: return "ROTTWEILER";
			case ELK: return "ELK";
			case RETRIEVER: return "RETRIEVER";
			case RAT: return "RAT";
			case BIRD: return "BIRD";
			case FISH: return "FISH";
			case SMALL_MAMMAL: return "SMALL_MAMMAL";
			case SMALL_DOG: return "SMALL_DOG";
			case CAT: return "CAT";
			case RABBIT: return "RABBIT";
			case NUM_PEDTYPES: return "NUM_PEDTYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a PedTypes value.
	PedTypes PedTypes_Parse(const char* str, const PedTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1249840705U: return HUMAN;
			case 2332652347U: return DOG;
			case 837094928U: return DEER;
			case 3996385720U: return BOAR;
			case 3995448648U: return COYOTE;
			case 810433390U: return MTLION;
			case 2829218670U: return PIG;
			case 1940376681U: return CHIMP;
			case 4032426090U: return COW;
			case 1864071074U: return ROTTWEILER;
			case 3168406006U: return ELK;
			case 3378182107U: return RETRIEVER;
			case 570755475U: return RAT;
			case 2685613959U: return BIRD;
			case 1630085303U: return FISH;
			case 1904750855U: return SMALL_MAMMAL;
			case 135434305U: return SMALL_DOG;
			case 1157867945U: return CAT;
			case 3443217914U: return RABBIT;
			case 1599418435U:
			case 2873142031U: return NUM_PEDTYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a CarEngineTypes value into its string representation.
	const char* CarEngineTypes_ToString(const CarEngineTypes value)
	{
		switch(value)
		{
			case COMBUSTION: return "COMBUSTION";
			case ELECTRIC: return "ELECTRIC";
			case HYBRID: return "HYBRID";
			case BLEND: return "BLEND";
			case NUM_CARENGINETYPES: return "NUM_CARENGINETYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a CarEngineTypes value.
	CarEngineTypes CarEngineTypes_Parse(const char* str, const CarEngineTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3946898016U: return COMBUSTION;
			case 4194510659U: return ELECTRIC;
			case 3907068470U: return HYBRID;
			case 3933447691U: return BLEND;
			case 1189024975U:
			case 3720703955U: return NUM_CARENGINETYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a HumanPedTypes value into its string representation.
	const char* HumanPedTypes_ToString(const HumanPedTypes value)
	{
		switch(value)
		{
			case AUD_PEDTYPE_MICHAEL: return "AUD_PEDTYPE_MICHAEL";
			case AUD_PEDTYPE_FRANKLIN: return "AUD_PEDTYPE_FRANKLIN";
			case AUD_PEDTYPE_TREVOR: return "AUD_PEDTYPE_TREVOR";
			case AUD_PEDTYPE_MAN: return "AUD_PEDTYPE_MAN";
			case AUD_PEDTYPE_WOMAN: return "AUD_PEDTYPE_WOMAN";
			case AUD_PEDTYPE_GANG: return "AUD_PEDTYPE_GANG";
			case AUD_PEDTYPE_COP: return "AUD_PEDTYPE_COP";
			case NUM_HUMANPEDTYPES: return "NUM_HUMANPEDTYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a HumanPedTypes value.
	HumanPedTypes HumanPedTypes_Parse(const char* str, const HumanPedTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2445623297U: return AUD_PEDTYPE_MICHAEL;
			case 4105785862U: return AUD_PEDTYPE_FRANKLIN;
			case 2849608067U: return AUD_PEDTYPE_TREVOR;
			case 3074323066U: return AUD_PEDTYPE_MAN;
			case 1388709244U: return AUD_PEDTYPE_WOMAN;
			case 14797455U: return AUD_PEDTYPE_GANG;
			case 888319914U: return AUD_PEDTYPE_COP;
			case 2756970686U:
			case 3691199321U: return NUM_HUMANPEDTYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a ShoeTypes value into its string representation.
	const char* ShoeTypes_ToString(const ShoeTypes value)
	{
		switch(value)
		{
			case SHOE_TYPE_BARE: return "SHOE_TYPE_BARE";
			case SHOE_TYPE_BOOTS: return "SHOE_TYPE_BOOTS";
			case SHOE_TYPE_SHOES: return "SHOE_TYPE_SHOES";
			case SHOE_TYPE_HIGH_HEELS: return "SHOE_TYPE_HIGH_HEELS";
			case SHOE_TYPE_FLIPFLOPS: return "SHOE_TYPE_FLIPFLOPS";
			case SHOE_TYPE_FLIPPERS: return "SHOE_TYPE_FLIPPERS";
			case SHOE_TYPE_TRAINERS: return "SHOE_TYPE_TRAINERS";
			case SHOE_TYPE_CLOWN: return "SHOE_TYPE_CLOWN";
			case SHOE_TYPE_GOLF: return "SHOE_TYPE_GOLF";
			case NUM_SHOETYPES: return "NUM_SHOETYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a ShoeTypes value.
	ShoeTypes ShoeTypes_Parse(const char* str, const ShoeTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 595404583U: return SHOE_TYPE_BARE;
			case 1923675266U: return SHOE_TYPE_BOOTS;
			case 2436781955U: return SHOE_TYPE_SHOES;
			case 1300629744U: return SHOE_TYPE_HIGH_HEELS;
			case 1308971170U: return SHOE_TYPE_FLIPFLOPS;
			case 2672782267U: return SHOE_TYPE_FLIPPERS;
			case 1888108064U: return SHOE_TYPE_TRAINERS;
			case 691742232U: return SHOE_TYPE_CLOWN;
			case 1618908013U: return SHOE_TYPE_GOLF;
			case 3510378221U:
			case 411123777U: return NUM_SHOETYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a DoorTypes value into its string representation.
	const char* DoorTypes_ToString(const DoorTypes value)
	{
		switch(value)
		{
			case AUD_DOOR_TYPE_SLIDING_HORIZONTAL: return "AUD_DOOR_TYPE_SLIDING_HORIZONTAL";
			case AUD_DOOR_TYPE_SLIDING_VERTICAL: return "AUD_DOOR_TYPE_SLIDING_VERTICAL";
			case AUD_DOOR_TYPE_GARAGE: return "AUD_DOOR_TYPE_GARAGE";
			case AUD_DOOR_TYPE_BARRIER_ARM: return "AUD_DOOR_TYPE_BARRIER_ARM";
			case AUD_DOOR_TYPE_STD: return "AUD_DOOR_TYPE_STD";
			case NUM_DOORTYPES: return "NUM_DOORTYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a DoorTypes value.
	DoorTypes DoorTypes_Parse(const char* str, const DoorTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 360818250U: return AUD_DOOR_TYPE_SLIDING_HORIZONTAL;
			case 1839916418U: return AUD_DOOR_TYPE_SLIDING_VERTICAL;
			case 988433227U: return AUD_DOOR_TYPE_GARAGE;
			case 3159865002U: return AUD_DOOR_TYPE_BARRIER_ARM;
			case 2536015345U: return AUD_DOOR_TYPE_STD;
			case 2313034815U:
			case 2946968377U: return NUM_DOORTYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a WaterType value into its string representation.
	const char* WaterType_ToString(const WaterType value)
	{
		switch(value)
		{
			case AUD_WATER_TYPE_POOL: return "AUD_WATER_TYPE_POOL";
			case AUD_WATER_TYPE_RIVER: return "AUD_WATER_TYPE_RIVER";
			case AUD_WATER_TYPE_LAKE: return "AUD_WATER_TYPE_LAKE";
			case AUD_WATER_TYPE_OCEAN: return "AUD_WATER_TYPE_OCEAN";
			case NUM_WATERTYPE: return "NUM_WATERTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a WaterType value.
	WaterType WaterType_Parse(const char* str, const WaterType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3467567174U: return AUD_WATER_TYPE_POOL;
			case 2972174675U: return AUD_WATER_TYPE_RIVER;
			case 29281334U: return AUD_WATER_TYPE_LAKE;
			case 1195690228U: return AUD_WATER_TYPE_OCEAN;
			case 577385401U:
			case 3967724422U: return NUM_WATERTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a LakeSize value into its string representation.
	const char* LakeSize_ToString(const LakeSize value)
	{
		switch(value)
		{
			case AUD_LAKE_SMALL: return "AUD_LAKE_SMALL";
			case AUD_LAKE_MEDIUM: return "AUD_LAKE_MEDIUM";
			case AUD_LAKE_LARGE: return "AUD_LAKE_LARGE";
			case NUM_LAKESIZE: return "NUM_LAKESIZE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a LakeSize value.
	LakeSize LakeSize_Parse(const char* str, const LakeSize defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2088137638U: return AUD_LAKE_SMALL;
			case 1313444779U: return AUD_LAKE_MEDIUM;
			case 50084936U: return AUD_LAKE_LARGE;
			case 899037291U:
			case 69236802U: return NUM_LAKESIZE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a RiverType value into its string representation.
	const char* RiverType_ToString(const RiverType value)
	{
		switch(value)
		{
			case AUD_RIVER_BROOK_WEAK: return "AUD_RIVER_BROOK_WEAK";
			case AUD_RIVER_BROOK_STRONG: return "AUD_RIVER_BROOK_STRONG";
			case AUD_RIVER_LA_WEAK: return "AUD_RIVER_LA_WEAK";
			case AUD_RIVER_LA_STRONG: return "AUD_RIVER_LA_STRONG";
			case AUD_RIVER_WEAK: return "AUD_RIVER_WEAK";
			case AUD_RIVER_MEDIUM: return "AUD_RIVER_MEDIUM";
			case AUD_RIVER_STRONG: return "AUD_RIVER_STRONG";
			case AUD_RIVER_RAPIDS_WEAK: return "AUD_RIVER_RAPIDS_WEAK";
			case AUD_RIVER_RAPIDS_STRONG: return "AUD_RIVER_RAPIDS_STRONG";
			case NUM_RIVERTYPE: return "NUM_RIVERTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a RiverType value.
	RiverType RiverType_Parse(const char* str, const RiverType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1348791814U: return AUD_RIVER_BROOK_WEAK;
			case 486007406U: return AUD_RIVER_BROOK_STRONG;
			case 862322654U: return AUD_RIVER_LA_WEAK;
			case 3776465088U: return AUD_RIVER_LA_STRONG;
			case 3359755673U: return AUD_RIVER_WEAK;
			case 936397172U: return AUD_RIVER_MEDIUM;
			case 4170440181U: return AUD_RIVER_STRONG;
			case 827574319U: return AUD_RIVER_RAPIDS_WEAK;
			case 2575247591U: return AUD_RIVER_RAPIDS_STRONG;
			case 2886341790U:
			case 2915173175U: return NUM_RIVERTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an OceanWaterType value into its string representation.
	const char* OceanWaterType_ToString(const OceanWaterType value)
	{
		switch(value)
		{
			case AUD_OCEAN_TYPE_BEACH: return "AUD_OCEAN_TYPE_BEACH";
			case AUD_OCEAN_TYPE_ROCKS: return "AUD_OCEAN_TYPE_ROCKS";
			case AUD_OCEAN_TYPE_PORT: return "AUD_OCEAN_TYPE_PORT";
			case AUD_OCEAN_TYPE_PIER: return "AUD_OCEAN_TYPE_PIER";
			case NUM_OCEANWATERTYPE: return "NUM_OCEANWATERTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an OceanWaterType value.
	OceanWaterType OceanWaterType_Parse(const char* str, const OceanWaterType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3107008882U: return AUD_OCEAN_TYPE_BEACH;
			case 1369923268U: return AUD_OCEAN_TYPE_ROCKS;
			case 2860841602U: return AUD_OCEAN_TYPE_PORT;
			case 2998882800U: return AUD_OCEAN_TYPE_PIER;
			case 819835534U:
			case 2689476359U: return NUM_OCEANWATERTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an OceanWaterDirection value into its string representation.
	const char* OceanWaterDirection_ToString(const OceanWaterDirection value)
	{
		switch(value)
		{
			case AUD_OCEAN_DIR_NORTH: return "AUD_OCEAN_DIR_NORTH";
			case AUD_OCEAN_DIR_NORTH_EAST: return "AUD_OCEAN_DIR_NORTH_EAST";
			case AUD_OCEAN_DIR_EAST: return "AUD_OCEAN_DIR_EAST";
			case AUD_OCEAN_DIR_SOUTH_EAST: return "AUD_OCEAN_DIR_SOUTH_EAST";
			case AUD_OCEAN_DIR_SOUTH: return "AUD_OCEAN_DIR_SOUTH";
			case AUD_OCEAN_DIR_SOUTH_WEST: return "AUD_OCEAN_DIR_SOUTH_WEST";
			case AUD_OCEAN_DIR_WEST: return "AUD_OCEAN_DIR_WEST";
			case AUD_OCEAN_DIR_NORTH_WEST: return "AUD_OCEAN_DIR_NORTH_WEST";
			case NUM_OCEANWATERDIRECTION: return "NUM_OCEANWATERDIRECTION";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an OceanWaterDirection value.
	OceanWaterDirection OceanWaterDirection_Parse(const char* str, const OceanWaterDirection defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 125533999U: return AUD_OCEAN_DIR_NORTH;
			case 2388083341U: return AUD_OCEAN_DIR_NORTH_EAST;
			case 188642620U: return AUD_OCEAN_DIR_EAST;
			case 2434276595U: return AUD_OCEAN_DIR_SOUTH_EAST;
			case 3581585078U: return AUD_OCEAN_DIR_SOUTH;
			case 1340769800U: return AUD_OCEAN_DIR_SOUTH_WEST;
			case 995471841U: return AUD_OCEAN_DIR_WEST;
			case 3479557018U: return AUD_OCEAN_DIR_NORTH_WEST;
			case 2367426559U:
			case 1973972153U: return NUM_OCEANWATERDIRECTION;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a MicrophoneType value into its string representation.
	const char* MicrophoneType_ToString(const MicrophoneType value)
	{
		switch(value)
		{
			case MIC_FOLLOW_PED: return "MIC_FOLLOW_PED";
			case MIC_FOLLOW_VEHICLE: return "MIC_FOLLOW_VEHICLE";
			case MIC_C_CHASE_HELI: return "MIC_C_CHASE_HELI";
			case MIC_C_IDLE: return "MIC_C_IDLE";
			case MIC_C_TRAIN_TRACK: return "MIC_C_TRAIN_TRACK";
			case MIC_SNIPER_RIFLE: return "MIC_SNIPER_RIFLE";
			case MIC_DEBUG: return "MIC_DEBUG";
			case MIC_PLAYER_HEAD: return "MIC_PLAYER_HEAD";
			case MIC_C_DEFAULT: return "MIC_C_DEFAULT";
			case MIC_C_MANCAM: return "MIC_C_MANCAM";
			case MIC_VEH_BONNET: return "MIC_VEH_BONNET";
			case NUM_MICROPHONETYPE: return "NUM_MICROPHONETYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a MicrophoneType value.
	MicrophoneType MicrophoneType_Parse(const char* str, const MicrophoneType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 4084070056U: return MIC_FOLLOW_PED;
			case 794475300U: return MIC_FOLLOW_VEHICLE;
			case 3920155942U: return MIC_C_CHASE_HELI;
			case 1028589959U: return MIC_C_IDLE;
			case 830117713U: return MIC_C_TRAIN_TRACK;
			case 1987674939U: return MIC_SNIPER_RIFLE;
			case 3564843467U: return MIC_DEBUG;
			case 1265803931U: return MIC_PLAYER_HEAD;
			case 2239559550U: return MIC_C_DEFAULT;
			case 4079219546U: return MIC_C_MANCAM;
			case 454077691U: return MIC_VEH_BONNET;
			case 2887872225U:
			case 2335322907U: return NUM_MICROPHONETYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a MicAttenuationType value into its string representation.
	const char* MicAttenuationType_ToString(const MicAttenuationType value)
	{
		switch(value)
		{
			case REAR_ATTENUATION_DEFAULT: return "REAR_ATTENUATION_DEFAULT";
			case REAR_ATTENUATION_ALWAYS: return "REAR_ATTENUATION_ALWAYS";
			case NUM_MICATTENUATIONTYPE: return "NUM_MICATTENUATIONTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a MicAttenuationType value.
	MicAttenuationType MicAttenuationType_Parse(const char* str, const MicAttenuationType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3334002558U: return REAR_ATTENUATION_DEFAULT;
			case 3836148064U: return REAR_ATTENUATION_ALWAYS;
			case 3322985580U:
			case 763365633U: return NUM_MICATTENUATIONTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a MaterialType value into its string representation.
	const char* MaterialType_ToString(const MaterialType value)
	{
		switch(value)
		{
			case HOLLOW_METAL: return "HOLLOW_METAL";
			case HOLLOW_PLASTIC: return "HOLLOW_PLASTIC";
			case GLASS: return "GLASS";
			case OTHER: return "OTHER";
			case NUM_MATERIALTYPE: return "NUM_MATERIALTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a MaterialType value.
	MaterialType MaterialType_Parse(const char* str, const MaterialType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2747780927U: return HOLLOW_METAL;
			case 743294624U: return HOLLOW_PLASTIC;
			case 3656960447U: return GLASS;
			case 1378233051U: return OTHER;
			case 2816905574U:
			case 3179416314U: return NUM_MATERIALTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SurfacePriority value into its string representation.
	const char* SurfacePriority_ToString(const SurfacePriority value)
	{
		switch(value)
		{
			case GRAVEL: return "GRAVEL";
			case SAND: return "SAND";
			case GRASS: return "GRASS";
			case PRIORITY_OTHER: return "PRIORITY_OTHER";
			case TARMAC: return "TARMAC";
			case NUM_SURFACEPRIORITY: return "NUM_SURFACEPRIORITY";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SurfacePriority value.
	SurfacePriority SurfacePriority_Parse(const char* str, const SurfacePriority defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 754109539U: return GRAVEL;
			case 3594488015U: return SAND;
			case 1919038356U: return GRASS;
			case 5898450U: return PRIORITY_OTHER;
			case 232261786U: return TARMAC;
			case 3651816660U:
			case 495058136U: return NUM_SURFACEPRIORITY;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a FilterMode value into its string representation.
	const char* FilterMode_ToString(const FilterMode value)
	{
		switch(value)
		{
			case kLowPass2Pole: return "kLowPass2Pole";
			case kHighPass2Pole: return "kHighPass2Pole";
			case kBandPass2Pole: return "kBandPass2Pole";
			case kBandStop2Pole: return "kBandStop2Pole";
			case kLowPass4Pole: return "kLowPass4Pole";
			case kHighPass4Pole: return "kHighPass4Pole";
			case kBandPass4Pole: return "kBandPass4Pole";
			case kBandStop4Pole: return "kBandStop4Pole";
			case kPeakingEQ: return "kPeakingEQ";
			case kLowShelf2Pole: return "kLowShelf2Pole";
			case kLowShelf4Pole: return "kLowShelf4Pole";
			case kHighShelf2Pole: return "kHighShelf2Pole";
			case kHighShelf4Pole: return "kHighShelf4Pole";
			case NUM_FILTERMODE: return "NUM_FILTERMODE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a FilterMode value.
	FilterMode FilterMode_Parse(const char* str, const FilterMode defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2483736903U: return kLowPass2Pole;
			case 3039069860U: return kHighPass2Pole;
			case 2872173064U: return kBandPass2Pole;
			case 3138504698U: return kBandStop2Pole;
			case 1824729762U: return kLowPass4Pole;
			case 1177786623U: return kHighPass4Pole;
			case 657959737U: return kBandPass4Pole;
			case 3458100856U: return kBandStop4Pole;
			case 1914327587U: return kPeakingEQ;
			case 3177227917U: return kLowShelf2Pole;
			case 2583472151U: return kLowShelf4Pole;
			case 633900323U: return kHighShelf2Pole;
			case 37293649U: return kHighShelf4Pole;
			case 1233521488U:
			case 98823079U: return NUM_FILTERMODE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SlowMoType value into its string representation.
	const char* SlowMoType_ToString(const SlowMoType value)
	{
		switch(value)
		{
			case AUD_SLOWMO_GENERAL: return "AUD_SLOWMO_GENERAL";
			case AUD_SLOWMO_CINEMATIC: return "AUD_SLOWMO_CINEMATIC";
			case AUD_SLOWMO_THIRD_PERSON_CINEMATIC_AIM: return "AUD_SLOWMO_THIRD_PERSON_CINEMATIC_AIM";
			case AUD_SLOWMO_STUNT: return "AUD_SLOWMO_STUNT";
			case AUD_SLOWMO_WEAPON: return "AUD_SLOWMO_WEAPON";
			case AUD_SLOWMO_SWITCH: return "AUD_SLOWMO_SWITCH";
			case AUD_SLOWMO_SPECIAL: return "AUD_SLOWMO_SPECIAL";
			case AUD_SLOWMO_CUSTOM: return "AUD_SLOWMO_CUSTOM";
			case AUD_SLOWMO_RADIOWHEEL: return "AUD_SLOWMO_RADIOWHEEL";
			case AUD_SLOWMO_PAUSEMENU: return "AUD_SLOWMO_PAUSEMENU";
			case NUM_SLOWMOTYPE: return "NUM_SLOWMOTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SlowMoType value.
	SlowMoType SlowMoType_Parse(const char* str, const SlowMoType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1887833790U: return AUD_SLOWMO_GENERAL;
			case 1290948636U: return AUD_SLOWMO_CINEMATIC;
			case 2716360569U: return AUD_SLOWMO_THIRD_PERSON_CINEMATIC_AIM;
			case 2348971479U: return AUD_SLOWMO_STUNT;
			case 1007922878U: return AUD_SLOWMO_WEAPON;
			case 3421410923U: return AUD_SLOWMO_SWITCH;
			case 2858964967U: return AUD_SLOWMO_SPECIAL;
			case 3097061998U: return AUD_SLOWMO_CUSTOM;
			case 2144856315U: return AUD_SLOWMO_RADIOWHEEL;
			case 1473297427U: return AUD_SLOWMO_PAUSEMENU;
			case 4129975911U:
			case 1104475506U: return NUM_SLOWMOTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a ClatterType value into its string representation.
	const char* ClatterType_ToString(const ClatterType value)
	{
		switch(value)
		{
			case AUD_CLATTER_TRUCK_CAB: return "AUD_CLATTER_TRUCK_CAB";
			case AUD_CLATTER_TRUCK_CAGES: return "AUD_CLATTER_TRUCK_CAGES";
			case AUD_CLATTER_TRUCK_TRAILER_CHAINS: return "AUD_CLATTER_TRUCK_TRAILER_CHAINS";
			case AUD_CLATTER_TAXI: return "AUD_CLATTER_TAXI";
			case AUD_CLATTER_BUS: return "AUD_CLATTER_BUS";
			case AUD_CLATTER_TRANSIT: return "AUD_CLATTER_TRANSIT";
			case AUD_CLATTER_TRANSIT_CLOWN: return "AUD_CLATTER_TRANSIT_CLOWN";
			case AUD_CLATTER_DETAIL: return "AUD_CLATTER_DETAIL";
			case AUD_CLATTER_NONE: return "AUD_CLATTER_NONE";
			case NUM_CLATTERTYPE: return "NUM_CLATTERTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a ClatterType value.
	ClatterType ClatterType_Parse(const char* str, const ClatterType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 171156933U: return AUD_CLATTER_TRUCK_CAB;
			case 224682081U: return AUD_CLATTER_TRUCK_CAGES;
			case 3148978082U: return AUD_CLATTER_TRUCK_TRAILER_CHAINS;
			case 2892622627U: return AUD_CLATTER_TAXI;
			case 2599310236U: return AUD_CLATTER_BUS;
			case 2982708111U: return AUD_CLATTER_TRANSIT;
			case 2535569223U: return AUD_CLATTER_TRANSIT_CLOWN;
			case 3696392626U: return AUD_CLATTER_DETAIL;
			case 923939194U: return AUD_CLATTER_NONE;
			case 3231476898U:
			case 2586052696U: return NUM_CLATTERTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a RandomDamageClass value into its string representation.
	const char* RandomDamageClass_ToString(const RandomDamageClass value)
	{
		switch(value)
		{
			case RANDOM_DAMAGE_ALWAYS: return "RANDOM_DAMAGE_ALWAYS";
			case RANDOM_DAMAGE_WORKHORSE: return "RANDOM_DAMAGE_WORKHORSE";
			case RANDOM_DAMAGE_OCCASIONAL: return "RANDOM_DAMAGE_OCCASIONAL";
			case RANDOM_DAMAGE_NONE: return "RANDOM_DAMAGE_NONE";
			case NUM_RANDOMDAMAGECLASS: return "NUM_RANDOMDAMAGECLASS";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a RandomDamageClass value.
	RandomDamageClass RandomDamageClass_Parse(const char* str, const RandomDamageClass defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1098137147U: return RANDOM_DAMAGE_ALWAYS;
			case 3646328988U: return RANDOM_DAMAGE_WORKHORSE;
			case 3287099714U: return RANDOM_DAMAGE_OCCASIONAL;
			case 4292331616U: return RANDOM_DAMAGE_NONE;
			case 512923143U:
			case 668041570U: return NUM_RANDOMDAMAGECLASS;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientZoneWaterType value into its string representation.
	const char* AmbientZoneWaterType_ToString(const AmbientZoneWaterType value)
	{
		switch(value)
		{
			case AMBIENT_ZONE_FORCE_OVER_WATER: return "AMBIENT_ZONE_FORCE_OVER_WATER";
			case AMBIENT_ZONE_FORCE_OVER_LAND: return "AMBIENT_ZONE_FORCE_OVER_LAND";
			case AMBIENT_ZONE_USE_SHORELINES: return "AMBIENT_ZONE_USE_SHORELINES";
			case NUM_AMBIENTZONEWATERTYPE: return "NUM_AMBIENTZONEWATERTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientZoneWaterType value.
	AmbientZoneWaterType AmbientZoneWaterType_Parse(const char* str, const AmbientZoneWaterType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2093186301U: return AMBIENT_ZONE_FORCE_OVER_WATER;
			case 1274002434U: return AMBIENT_ZONE_FORCE_OVER_LAND;
			case 3286317232U: return AMBIENT_ZONE_USE_SHORELINES;
			case 216559046U:
			case 1388143776U: return NUM_AMBIENTZONEWATERTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a RevLimiterApplyType value into its string representation.
	const char* RevLimiterApplyType_ToString(const RevLimiterApplyType value)
	{
		switch(value)
		{
			case REV_LIMITER_ENGINE_ONLY: return "REV_LIMITER_ENGINE_ONLY";
			case REV_LIMITER_EXHAUST_ONLY: return "REV_LIMITER_EXHAUST_ONLY";
			case REV_LIMITER_BOTH_CHANNELS: return "REV_LIMITER_BOTH_CHANNELS";
			case NUM_REVLIMITERAPPLYTYPE: return "NUM_REVLIMITERAPPLYTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a RevLimiterApplyType value.
	RevLimiterApplyType RevLimiterApplyType_Parse(const char* str, const RevLimiterApplyType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2978291262U: return REV_LIMITER_ENGINE_ONLY;
			case 2091482163U: return REV_LIMITER_EXHAUST_ONLY;
			case 1131991319U: return REV_LIMITER_BOTH_CHANNELS;
			case 2776070307U:
			case 4116522450U: return NUM_REVLIMITERAPPLYTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a FakeGesture value into its string representation.
	const char* FakeGesture_ToString(const FakeGesture value)
	{
		switch(value)
		{
			case kAbsolutelyGesture: return "kAbsolutelyGesture";
			case kAgreeGesture: return "kAgreeGesture";
			case kComeHereFrontGesture: return "kComeHereFrontGesture";
			case kComeHereLeftGesture: return "kComeHereLeftGesture";
			case kDamnGesture: return "kDamnGesture";
			case kDontKnowGesture: return "kDontKnowGesture";
			case kEasyNowGesture: return "kEasyNowGesture";
			case kExactlyGesture: return "kExactlyGesture";
			case kForgetItGesture: return "kForgetItGesture";
			case kGoodGesture: return "kGoodGesture";
			case kHelloGesture: return "kHelloGesture";
			case kHeyGesture: return "kHeyGesture";
			case kIDontThinkSoGesture: return "kIDontThinkSoGesture";
			case kIfYouSaySoGesture: return "kIfYouSaySoGesture";
			case kIllDoItGesture: return "kIllDoItGesture";
			case kImNotSureGesture: return "kImNotSureGesture";
			case kImSorryGesture: return "kImSorryGesture";
			case kIndicateBackGesture: return "kIndicateBackGesture";
			case kIndicateLeftGesture: return "kIndicateLeftGesture";
			case kIndicateRightGesture: return "kIndicateRightGesture";
			case kNoChanceGesture: return "kNoChanceGesture";
			case kYesGesture: return "kYesGesture";
			case kYouUnderstandGesture: return "kYouUnderstandGesture";
			case NUM_FAKEGESTURE: return "NUM_FAKEGESTURE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a FakeGesture value.
	FakeGesture FakeGesture_Parse(const char* str, const FakeGesture defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1077162512U: return kAbsolutelyGesture;
			case 4291173158U: return kAgreeGesture;
			case 2187578084U: return kComeHereFrontGesture;
			case 119325147U: return kComeHereLeftGesture;
			case 62993442U: return kDamnGesture;
			case 2708837859U: return kDontKnowGesture;
			case 2271998040U: return kEasyNowGesture;
			case 1571844358U: return kExactlyGesture;
			case 2302016751U: return kForgetItGesture;
			case 1128733443U: return kGoodGesture;
			case 4006605434U: return kHelloGesture;
			case 3838016947U: return kHeyGesture;
			case 798092652U: return kIDontThinkSoGesture;
			case 2842052170U: return kIfYouSaySoGesture;
			case 1949488435U: return kIllDoItGesture;
			case 2353638895U: return kImNotSureGesture;
			case 3886370010U: return kImSorryGesture;
			case 1796840288U: return kIndicateBackGesture;
			case 3885279549U: return kIndicateLeftGesture;
			case 2990104709U: return kIndicateRightGesture;
			case 664301186U: return kNoChanceGesture;
			case 1112895391U: return kYesGesture;
			case 1530576175U: return kYouUnderstandGesture;
			case 4281719691U:
			case 3717975926U: return NUM_FAKEGESTURE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a ShellCasingType value into its string representation.
	const char* ShellCasingType_ToString(const ShellCasingType value)
	{
		switch(value)
		{
			case SHELLCASING_METAL_SMALL: return "SHELLCASING_METAL_SMALL";
			case SHELLCASING_METAL: return "SHELLCASING_METAL";
			case SHELLCASING_METAL_LARGE: return "SHELLCASING_METAL_LARGE";
			case SHELLCASING_PLASTIC: return "SHELLCASING_PLASTIC";
			case SHELLCASING_NONE: return "SHELLCASING_NONE";
			case NUM_SHELLCASINGTYPE: return "NUM_SHELLCASINGTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a ShellCasingType value.
	ShellCasingType ShellCasingType_Parse(const char* str, const ShellCasingType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1707300346U: return SHELLCASING_METAL_SMALL;
			case 1724066032U: return SHELLCASING_METAL;
			case 2399922269U: return SHELLCASING_METAL_LARGE;
			case 3693593974U: return SHELLCASING_PLASTIC;
			case 3207818827U: return SHELLCASING_NONE;
			case 2455733263U:
			case 3534495628U: return NUM_SHELLCASINGTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a FootstepLoudness value into its string representation.
	const char* FootstepLoudness_ToString(const FootstepLoudness value)
	{
		switch(value)
		{
			case FOOTSTEP_LOUDNESS_SILENT: return "FOOTSTEP_LOUDNESS_SILENT";
			case FOOTSTEP_LOUDNESS_QUIET: return "FOOTSTEP_LOUDNESS_QUIET";
			case FOOTSTEP_LOUDNESS_MEDIUM: return "FOOTSTEP_LOUDNESS_MEDIUM";
			case FOOTSTEP_LOUDNESS_LOUD: return "FOOTSTEP_LOUDNESS_LOUD";
			case NUM_FOOTSTEPLOUDNESS: return "NUM_FOOTSTEPLOUDNESS";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a FootstepLoudness value.
	FootstepLoudness FootstepLoudness_Parse(const char* str, const FootstepLoudness defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1067748931U: return FOOTSTEP_LOUDNESS_SILENT;
			case 602103615U: return FOOTSTEP_LOUDNESS_QUIET;
			case 1113127016U: return FOOTSTEP_LOUDNESS_MEDIUM;
			case 2918547143U: return FOOTSTEP_LOUDNESS_LOUD;
			case 2546611115U:
			case 750186564U: return NUM_FOOTSTEPLOUDNESS;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a TrackCats value into its string representation.
	const char* TrackCats_ToString(const TrackCats value)
	{
		switch(value)
		{
			case RADIO_TRACK_CAT_ADVERTS: return "RADIO_TRACK_CAT_ADVERTS";
			case RADIO_TRACK_CAT_IDENTS: return "RADIO_TRACK_CAT_IDENTS";
			case RADIO_TRACK_CAT_MUSIC: return "RADIO_TRACK_CAT_MUSIC";
			case RADIO_TRACK_CAT_NEWS: return "RADIO_TRACK_CAT_NEWS";
			case RADIO_TRACK_CAT_WEATHER: return "RADIO_TRACK_CAT_WEATHER";
			case RADIO_TRACK_CAT_DJSOLO: return "RADIO_TRACK_CAT_DJSOLO";
			case RADIO_TRACK_CAT_TO_NEWS: return "RADIO_TRACK_CAT_TO_NEWS";
			case RADIO_TRACK_CAT_TO_AD: return "RADIO_TRACK_CAT_TO_AD";
			case RADIO_TRACK_CAT_USER_INTRO: return "RADIO_TRACK_CAT_USER_INTRO";
			case RADIO_TRACK_CAT_USER_OUTRO: return "RADIO_TRACK_CAT_USER_OUTRO";
			case RADIO_TRACK_CAT_INTRO_MORNING: return "RADIO_TRACK_CAT_INTRO_MORNING";
			case RADIO_TRACK_CAT_INTRO_EVENING: return "RADIO_TRACK_CAT_INTRO_EVENING";
			case RADIO_TRACK_CAT_TAKEOVER_MUSIC: return "RADIO_TRACK_CAT_TAKEOVER_MUSIC";
			case RADIO_TRACK_CAT_TAKEOVER_IDENTS: return "RADIO_TRACK_CAT_TAKEOVER_IDENTS";
			case RADIO_TRACK_CAT_TAKEOVER_DJSOLO: return "RADIO_TRACK_CAT_TAKEOVER_DJSOLO";
			case NUM_RADIO_TRACK_CATS: return "NUM_RADIO_TRACK_CATS";
			case NUM_TRACKCATS: return "NUM_TRACKCATS";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a TrackCats value.
	TrackCats TrackCats_Parse(const char* str, const TrackCats defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1558468702U: return RADIO_TRACK_CAT_ADVERTS;
			case 3036725873U: return RADIO_TRACK_CAT_IDENTS;
			case 2186981730U: return RADIO_TRACK_CAT_MUSIC;
			case 265955797U: return RADIO_TRACK_CAT_NEWS;
			case 3141348754U: return RADIO_TRACK_CAT_WEATHER;
			case 973461143U: return RADIO_TRACK_CAT_DJSOLO;
			case 3174542339U: return RADIO_TRACK_CAT_TO_NEWS;
			case 164576876U: return RADIO_TRACK_CAT_TO_AD;
			case 2291428896U: return RADIO_TRACK_CAT_USER_INTRO;
			case 3749337325U: return RADIO_TRACK_CAT_USER_OUTRO;
			case 1640676127U: return RADIO_TRACK_CAT_INTRO_MORNING;
			case 209971970U: return RADIO_TRACK_CAT_INTRO_EVENING;
			case 1699559142U: return RADIO_TRACK_CAT_TAKEOVER_MUSIC;
			case 3625022137U: return RADIO_TRACK_CAT_TAKEOVER_IDENTS;
			case 2451296863U: return RADIO_TRACK_CAT_TAKEOVER_DJSOLO;
			case 3736260255U: return NUM_RADIO_TRACK_CATS;
			case 2249873059U:
			case 2089076275U: return NUM_TRACKCATS;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a PVGBackupType value into its string representation.
	const char* PVGBackupType_ToString(const PVGBackupType value)
	{
		switch(value)
		{
			case kMaleCowardly: return "kMaleCowardly";
			case kMaleBrave: return "kMaleBrave";
			case kMaleGang: return "kMaleGang";
			case kFemale: return "kFemale";
			case kCop: return "kCop";
			case NUM_PVGBACKUPTYPE: return "NUM_PVGBACKUPTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a PVGBackupType value.
	PVGBackupType PVGBackupType_Parse(const char* str, const PVGBackupType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3413609836U: return kMaleCowardly;
			case 1471378956U: return kMaleBrave;
			case 3132516987U: return kMaleGang;
			case 538423620U: return kFemale;
			case 924601655U: return kCop;
			case 4044872940U:
			case 354726052U: return NUM_PVGBACKUPTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a ScannerSlotType value into its string representation.
	const char* ScannerSlotType_ToString(const ScannerSlotType value)
	{
		switch(value)
		{
			case LARGE_SCANNER_SLOT: return "LARGE_SCANNER_SLOT";
			case SMALL_SCANNER_SLOT: return "SMALL_SCANNER_SLOT";
			case NUM_SCANNERSLOTTYPE: return "NUM_SCANNERSLOTTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a ScannerSlotType value.
	ScannerSlotType ScannerSlotType_Parse(const char* str, const ScannerSlotType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 3922169399U: return LARGE_SCANNER_SLOT;
			case 1561855061U: return SMALL_SCANNER_SLOT;
			case 2725241904U:
			case 1105327652U: return NUM_SCANNERSLOTTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AmbientZoneShape value into its string representation.
	const char* AmbientZoneShape_ToString(const AmbientZoneShape value)
	{
		switch(value)
		{
			case kAmbientZoneCuboid: return "kAmbientZoneCuboid";
			case kAmbientZoneSphere: return "kAmbientZoneSphere";
			case kAmbientZoneCuboidLineEmitter: return "kAmbientZoneCuboidLineEmitter";
			case kAmbientZoneSphereLineEmitter: return "kAmbientZoneSphereLineEmitter";
			case NUM_AMBIENTZONESHAPE: return "NUM_AMBIENTZONESHAPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AmbientZoneShape value.
	AmbientZoneShape AmbientZoneShape_Parse(const char* str, const AmbientZoneShape defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2439920340U: return kAmbientZoneCuboid;
			case 4158834846U: return kAmbientZoneSphere;
			case 4101427779U: return kAmbientZoneCuboidLineEmitter;
			case 4017943482U: return kAmbientZoneSphereLineEmitter;
			case 4026675854U:
			case 2037173368U: return NUM_AMBIENTZONESHAPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a BankingStyle value into its string representation.
	const char* BankingStyle_ToString(const BankingStyle value)
	{
		switch(value)
		{
			case kRotationAngle: return "kRotationAngle";
			case kRotationSpeed: return "kRotationSpeed";
			case NUM_BANKINGSTYLE: return "NUM_BANKINGSTYLE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a BankingStyle value.
	BankingStyle BankingStyle_Parse(const char* str, const BankingStyle defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 728023074U: return kRotationAngle;
			case 193908949U: return kRotationSpeed;
			case 2238543341U:
			case 1206499106U: return NUM_BANKINGSTYLE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a MusicConstraintTypes value into its string representation.
	const char* MusicConstraintTypes_ToString(const MusicConstraintTypes value)
	{
		switch(value)
		{
			case kConstrainStart: return "kConstrainStart";
			case kConstrainEnd: return "kConstrainEnd";
			case NUM_MUSICCONSTRAINTTYPES: return "NUM_MUSICCONSTRAINTTYPES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a MusicConstraintTypes value.
	MusicConstraintTypes MusicConstraintTypes_Parse(const char* str, const MusicConstraintTypes defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2216782801U: return kConstrainStart;
			case 3661270961U: return kConstrainEnd;
			case 1399024297U:
			case 2367884888U: return NUM_MUSICCONSTRAINTTYPES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a MusicArea value into its string representation.
	const char* MusicArea_ToString(const MusicArea value)
	{
		switch(value)
		{
			case kMusicAreaEverywhere: return "kMusicAreaEverywhere";
			case kMusicAreaCity: return "kMusicAreaCity";
			case kMusicAreaCountry: return "kMusicAreaCountry";
			case NUM_MUSICAREA: return "NUM_MUSICAREA";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a MusicArea value.
	MusicArea MusicArea_Parse(const char* str, const MusicArea defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 1259230847U: return kMusicAreaEverywhere;
			case 4091376186U: return kMusicAreaCity;
			case 468985333U: return kMusicAreaCountry;
			case 2560710014U:
			case 1934102263U: return NUM_MUSICAREA;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a SyncMarkerType value into its string representation.
	const char* SyncMarkerType_ToString(const SyncMarkerType value)
	{
		switch(value)
		{
			case kNoSyncMarker: return "kNoSyncMarker";
			case kAnyMarker: return "kAnyMarker";
			case kBeatMarker: return "kBeatMarker";
			case kBarMarker: return "kBarMarker";
			case NUM_SYNCMARKERTYPE: return "NUM_SYNCMARKERTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a SyncMarkerType value.
	SyncMarkerType SyncMarkerType_Parse(const char* str, const SyncMarkerType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 4077285864U: return kNoSyncMarker;
			case 3600502804U: return kAnyMarker;
			case 3705139402U: return kBeatMarker;
			case 3195085256U: return kBarMarker;
			case 2240352950U:
			case 1761629347U: return NUM_SYNCMARKERTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert an AnimalType value into its string representation.
	const char* AnimalType_ToString(const AnimalType value)
	{
		switch(value)
		{
			case kAnimalNone: return "kAnimalNone";
			case kAnimalBoar: return "kAnimalBoar";
			case kAnimalChicken: return "kAnimalChicken";
			case kAnimalDog: return "kAnimalDog";
			case kAnimalRottweiler: return "kAnimalRottweiler";
			case kAnimalHorse: return "kAnimalHorse";
			case kAnimalCow: return "kAnimalCow";
			case kAnimalCoyote: return "kAnimalCoyote";
			case kAnimalDeer: return "kAnimalDeer";
			case kAnimalEagle: return "kAnimalEagle";
			case kAnimalFish: return "kAnimalFish";
			case kAnimalHen: return "kAnimalHen";
			case kAnimalMonkey: return "kAnimalMonkey";
			case kAnimalMountainLion: return "kAnimalMountainLion";
			case kAnimalPigeon: return "kAnimalPigeon";
			case kAnimalRat: return "kAnimalRat";
			case kAnimalSeagull: return "kAnimalSeagull";
			case kAnimalCrow: return "kAnimalCrow";
			case kAnimalPig: return "kAnimalPig";
			case kAnimalChickenhawk: return "kAnimalChickenhawk";
			case kAnimalCormorant: return "kAnimalCormorant";
			case kAnimalRhesus: return "kAnimalRhesus";
			case kAnimalRetriever: return "kAnimalRetriever";
			case kAnimalChimp: return "kAnimalChimp";
			case kAnimalHusky: return "kAnimalHusky";
			case kAnimalShepherd: return "kAnimalShepherd";
			case kAnimalCat: return "kAnimalCat";
			case kAnimalWhale: return "kAnimalWhale";
			case kAnimalDolphin: return "kAnimalDolphin";
			case kAnimalSmallDog: return "kAnimalSmallDog";
			case kAnimalHammerhead: return "kAnimalHammerhead";
			case kAnimalStingray: return "kAnimalStingray";
			case kAnimalRabbit: return "kAnimalRabbit";
			case kAnimalOrca: return "kAnimalOrca";
			case NUM_ANIMALTYPE: return "NUM_ANIMALTYPE";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into an AnimalType value.
	AnimalType AnimalType_Parse(const char* str, const AnimalType defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2655686719U: return kAnimalNone;
			case 141235488U: return kAnimalBoar;
			case 2668357612U: return kAnimalChicken;
			case 2669485497U: return kAnimalDog;
			case 730235641U: return kAnimalRottweiler;
			case 4284274902U: return kAnimalHorse;
			case 2302289266U: return kAnimalCow;
			case 59979283U: return kAnimalCoyote;
			case 2497330350U: return kAnimalDeer;
			case 1064181639U: return kAnimalEagle;
			case 1744725978U: return kAnimalFish;
			case 1793836635U: return kAnimalHen;
			case 1052482261U: return kAnimalMonkey;
			case 1644378033U: return kAnimalMountainLion;
			case 2266087285U: return kAnimalPigeon;
			case 738353462U: return kAnimalRat;
			case 256393662U: return kAnimalSeagull;
			case 2982841018U: return kAnimalCrow;
			case 960279276U: return kAnimalPig;
			case 2247463372U: return kAnimalChickenhawk;
			case 1498292056U: return kAnimalCormorant;
			case 1716952685U: return kAnimalRhesus;
			case 2782945384U: return kAnimalRetriever;
			case 1689631074U: return kAnimalChimp;
			case 2676958275U: return kAnimalHusky;
			case 4069638215U: return kAnimalShepherd;
			case 31898677U: return kAnimalCat;
			case 2528140034U: return kAnimalWhale;
			case 1257365772U: return kAnimalDolphin;
			case 4135112327U: return kAnimalSmallDog;
			case 3922766833U: return kAnimalHammerhead;
			case 3885213603U: return kAnimalStingray;
			case 693392699U: return kAnimalRabbit;
			case 1201223267U: return kAnimalOrca;
			case 4004423329U:
			case 36189834U: return NUM_ANIMALTYPE;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Convert a RadioDjSpeechCategories value into its string representation.
	const char* RadioDjSpeechCategories_ToString(const RadioDjSpeechCategories value)
	{
		switch(value)
		{
			case kDjSpeechIntro: return "kDjSpeechIntro";
			case kDjSpeechOutro: return "kDjSpeechOutro";
			case kDjSpeechGeneral: return "kDjSpeechGeneral";
			case kDjSpeechTime: return "kDjSpeechTime";
			case kDjSpeechTo: return "kDjSpeechTo";
			case NUM_RADIODJSPEECHCATEGORIES: return "NUM_RADIODJSPEECHCATEGORIES";
			default: return NULL;
		}
	}
	
	// PURPOSE - Parse a string into a RadioDjSpeechCategories value.
	RadioDjSpeechCategories RadioDjSpeechCategories_Parse(const char* str, const RadioDjSpeechCategories defaultValue)
	{
		const rage::u32 hash = atStringHash(str);
		switch(hash)
		{
			case 2504597525U: return kDjSpeechIntro;
			case 4087317723U: return kDjSpeechOutro;
			case 465465803U: return kDjSpeechGeneral;
			case 935135408U: return kDjSpeechTime;
			case 3307353195U: return kDjSpeechTo;
			case 1910732262U:
			case 2492616877U: return NUM_RADIODJSPEECHCATEGORIES;
			default: return defaultValue;
		}
	}
	
	// PURPOSE - Gets the type id of the parent class from the given type id
	rage::u32 gGameObjectsGetBaseTypeId(const rage::u32 classId)
	{
		switch(classId)
		{
			case AudBaseObject::TYPE_ID: return AudBaseObject::BASE_TYPE_ID;
			case VehicleCollisionSettings::TYPE_ID: return VehicleCollisionSettings::BASE_TYPE_ID;
			case TrailerAudioSettings::TYPE_ID: return TrailerAudioSettings::BASE_TYPE_ID;
			case CarAudioSettings::TYPE_ID: return CarAudioSettings::BASE_TYPE_ID;
			case VehicleEngineAudioSettings::TYPE_ID: return VehicleEngineAudioSettings::BASE_TYPE_ID;
			case CollisionMaterialSettings::TYPE_ID: return CollisionMaterialSettings::BASE_TYPE_ID;
			case StaticEmitter::TYPE_ID: return StaticEmitter::BASE_TYPE_ID;
			case EntityEmitter::TYPE_ID: return EntityEmitter::BASE_TYPE_ID;
			case HeliAudioSettings::TYPE_ID: return HeliAudioSettings::BASE_TYPE_ID;
			case MeleeCombatSettings::TYPE_ID: return MeleeCombatSettings::BASE_TYPE_ID;
			case SpeechContextSettings::TYPE_ID: return SpeechContextSettings::BASE_TYPE_ID;
			case TriggeredSpeechContext::TYPE_ID: return TriggeredSpeechContext::BASE_TYPE_ID;
			case SpeechContext::TYPE_ID: return SpeechContext::BASE_TYPE_ID;
			case SpeechContextVirtual::TYPE_ID: return SpeechContextVirtual::BASE_TYPE_ID;
			case SpeechParams::TYPE_ID: return SpeechParams::BASE_TYPE_ID;
			case SpeechContextList::TYPE_ID: return SpeechContextList::BASE_TYPE_ID;
			case BoatAudioSettings::TYPE_ID: return BoatAudioSettings::BASE_TYPE_ID;
			case WeaponSettings::TYPE_ID: return WeaponSettings::BASE_TYPE_ID;
			case ShoeAudioSettings::TYPE_ID: return ShoeAudioSettings::BASE_TYPE_ID;
			case OldShoeSettings::TYPE_ID: return OldShoeSettings::BASE_TYPE_ID;
			case MovementShoeSettings::TYPE_ID: return MovementShoeSettings::BASE_TYPE_ID;
			case FootstepSurfaceSounds::TYPE_ID: return FootstepSurfaceSounds::BASE_TYPE_ID;
			case ModelPhysicsParams::TYPE_ID: return ModelPhysicsParams::BASE_TYPE_ID;
			case SkiAudioSettings::TYPE_ID: return SkiAudioSettings::BASE_TYPE_ID;
			case RadioStationList::TYPE_ID: return RadioStationList::BASE_TYPE_ID;
			case RadioStationSettings::TYPE_ID: return RadioStationSettings::BASE_TYPE_ID;
			case RadioStationTrackList::TYPE_ID: return RadioStationTrackList::BASE_TYPE_ID;
			case RadioTrackCategoryData::TYPE_ID: return RadioTrackCategoryData::BASE_TYPE_ID;
			case ScannerCrimeReport::TYPE_ID: return ScannerCrimeReport::BASE_TYPE_ID;
			case PedRaceToPVG::TYPE_ID: return PedRaceToPVG::BASE_TYPE_ID;
			case PedVoiceGroups::TYPE_ID: return PedVoiceGroups::BASE_TYPE_ID;
			case FriendGroup::TYPE_ID: return FriendGroup::BASE_TYPE_ID;
			case StaticEmitterList::TYPE_ID: return StaticEmitterList::BASE_TYPE_ID;
			case ScriptedScannerLine::TYPE_ID: return ScriptedScannerLine::BASE_TYPE_ID;
			case ScannerArea::TYPE_ID: return ScannerArea::BASE_TYPE_ID;
			case ScannerSpecificLocation::TYPE_ID: return ScannerSpecificLocation::BASE_TYPE_ID;
			case ScannerSpecificLocationList::TYPE_ID: return ScannerSpecificLocationList::BASE_TYPE_ID;
			case AmbientZone::TYPE_ID: return AmbientZone::BASE_TYPE_ID;
			case AmbientRule::TYPE_ID: return AmbientRule::BASE_TYPE_ID;
			case AmbientZoneList::TYPE_ID: return AmbientZoneList::BASE_TYPE_ID;
			case AmbientSlotMap::TYPE_ID: return AmbientSlotMap::BASE_TYPE_ID;
			case AmbientBankMap::TYPE_ID: return AmbientBankMap::BASE_TYPE_ID;
			case EnvironmentRule::TYPE_ID: return EnvironmentRule::BASE_TYPE_ID;
			case TrainStation::TYPE_ID: return TrainStation::BASE_TYPE_ID;
			case InteriorSettings::TYPE_ID: return InteriorSettings::BASE_TYPE_ID;
			case InteriorWeaponMetrics::TYPE_ID: return InteriorWeaponMetrics::BASE_TYPE_ID;
			case InteriorRoom::TYPE_ID: return InteriorRoom::BASE_TYPE_ID;
			case DoorAudioSettings::TYPE_ID: return DoorAudioSettings::BASE_TYPE_ID;
			case DoorTuningParams::TYPE_ID: return DoorTuningParams::BASE_TYPE_ID;
			case DoorList::TYPE_ID: return DoorList::BASE_TYPE_ID;
			case ItemAudioSettings::TYPE_ID: return ItemAudioSettings::BASE_TYPE_ID;
			case ClimbingAudioSettings::TYPE_ID: return ClimbingAudioSettings::BASE_TYPE_ID;
			case ModelAudioCollisionSettings::TYPE_ID: return ModelAudioCollisionSettings::BASE_TYPE_ID;
			case TrainAudioSettings::TYPE_ID: return TrainAudioSettings::BASE_TYPE_ID;
			case WeatherAudioSettings::TYPE_ID: return WeatherAudioSettings::BASE_TYPE_ID;
			case WeatherTypeAudioReference::TYPE_ID: return WeatherTypeAudioReference::BASE_TYPE_ID;
			case BicycleAudioSettings::TYPE_ID: return BicycleAudioSettings::BASE_TYPE_ID;
			case PlaneAudioSettings::TYPE_ID: return PlaneAudioSettings::BASE_TYPE_ID;
			case ConductorsDynamicMixingReference::TYPE_ID: return ConductorsDynamicMixingReference::BASE_TYPE_ID;
			case StemMix::TYPE_ID: return StemMix::BASE_TYPE_ID;
			case MusicTimingConstraint::TYPE_ID: return MusicTimingConstraint::BASE_TYPE_ID;
			case MusicAction::TYPE_ID: return MusicAction::BASE_TYPE_ID;
			case InteractiveMusicMood::TYPE_ID: return InteractiveMusicMood::BASE_TYPE_ID;
			case StartTrackAction::TYPE_ID: return StartTrackAction::BASE_TYPE_ID;
			case StopTrackAction::TYPE_ID: return StopTrackAction::BASE_TYPE_ID;
			case SetMoodAction::TYPE_ID: return SetMoodAction::BASE_TYPE_ID;
			case MusicEvent::TYPE_ID: return MusicEvent::BASE_TYPE_ID;
			case StartOneShotAction::TYPE_ID: return StartOneShotAction::BASE_TYPE_ID;
			case StopOneShotAction::TYPE_ID: return StopOneShotAction::BASE_TYPE_ID;
			case BeatConstraint::TYPE_ID: return BeatConstraint::BASE_TYPE_ID;
			case BarConstraint::TYPE_ID: return BarConstraint::BASE_TYPE_ID;
			case DirectionalAmbience::TYPE_ID: return DirectionalAmbience::BASE_TYPE_ID;
			case GunfightConductorIntensitySettings::TYPE_ID: return GunfightConductorIntensitySettings::BASE_TYPE_ID;
			case AnimalParams::TYPE_ID: return AnimalParams::BASE_TYPE_ID;
			case AnimalVocalAnimTrigger::TYPE_ID: return AnimalVocalAnimTrigger::BASE_TYPE_ID;
			case ScannerVoiceParams::TYPE_ID: return ScannerVoiceParams::BASE_TYPE_ID;
			case ScannerVehicleParams::TYPE_ID: return ScannerVehicleParams::BASE_TYPE_ID;
			case AudioRoadInfo::TYPE_ID: return AudioRoadInfo::BASE_TYPE_ID;
			case MicSettingsReference::TYPE_ID: return MicSettingsReference::BASE_TYPE_ID;
			case MicrophoneSettings::TYPE_ID: return MicrophoneSettings::BASE_TYPE_ID;
			case CarRecordingAudioSettings::TYPE_ID: return CarRecordingAudioSettings::BASE_TYPE_ID;
			case CarRecordingList::TYPE_ID: return CarRecordingList::BASE_TYPE_ID;
			case AnimalFootstepSettings::TYPE_ID: return AnimalFootstepSettings::BASE_TYPE_ID;
			case AnimalFootstepReference::TYPE_ID: return AnimalFootstepReference::BASE_TYPE_ID;
			case ShoeList::TYPE_ID: return ShoeList::BASE_TYPE_ID;
			case ClothAudioSettings::TYPE_ID: return ClothAudioSettings::BASE_TYPE_ID;
			case ClothList::TYPE_ID: return ClothList::BASE_TYPE_ID;
			case ExplosionAudioSettings::TYPE_ID: return ExplosionAudioSettings::BASE_TYPE_ID;
			case GranularEngineAudioSettings::TYPE_ID: return GranularEngineAudioSettings::BASE_TYPE_ID;
			case ShoreLineAudioSettings::TYPE_ID: return ShoreLineAudioSettings::BASE_TYPE_ID;
			case ShoreLinePoolAudioSettings::TYPE_ID: return ShoreLinePoolAudioSettings::BASE_TYPE_ID;
			case ShoreLineLakeAudioSettings::TYPE_ID: return ShoreLineLakeAudioSettings::BASE_TYPE_ID;
			case ShoreLineRiverAudioSettings::TYPE_ID: return ShoreLineRiverAudioSettings::BASE_TYPE_ID;
			case ShoreLineOceanAudioSettings::TYPE_ID: return ShoreLineOceanAudioSettings::BASE_TYPE_ID;
			case ShoreLineList::TYPE_ID: return ShoreLineList::BASE_TYPE_ID;
			case RadioTrackSettings::TYPE_ID: return RadioTrackSettings::BASE_TYPE_ID;
			case ModelFootStepTuning::TYPE_ID: return ModelFootStepTuning::BASE_TYPE_ID;
			case GranularEngineSet::TYPE_ID: return GranularEngineSet::BASE_TYPE_ID;
			case RadioDjSpeechAction::TYPE_ID: return RadioDjSpeechAction::BASE_TYPE_ID;
			case SilenceConstraint::TYPE_ID: return SilenceConstraint::BASE_TYPE_ID;
			case ReflectionsSettings::TYPE_ID: return ReflectionsSettings::BASE_TYPE_ID;
			case AlarmSettings::TYPE_ID: return AlarmSettings::BASE_TYPE_ID;
			case FadeOutRadioAction::TYPE_ID: return FadeOutRadioAction::BASE_TYPE_ID;
			case FadeInRadioAction::TYPE_ID: return FadeInRadioAction::BASE_TYPE_ID;
			case ForceRadioTrackAction::TYPE_ID: return ForceRadioTrackAction::BASE_TYPE_ID;
			case SlowMoSettings::TYPE_ID: return SlowMoSettings::BASE_TYPE_ID;
			case PedScenarioAudioSettings::TYPE_ID: return PedScenarioAudioSettings::BASE_TYPE_ID;
			case PortalSettings::TYPE_ID: return PortalSettings::BASE_TYPE_ID;
			case ElectricEngineAudioSettings::TYPE_ID: return ElectricEngineAudioSettings::BASE_TYPE_ID;
			case PlayerBreathingSettings::TYPE_ID: return PlayerBreathingSettings::BASE_TYPE_ID;
			case PedWallaSpeechSettings::TYPE_ID: return PedWallaSpeechSettings::BASE_TYPE_ID;
			case AircraftWarningSettings::TYPE_ID: return AircraftWarningSettings::BASE_TYPE_ID;
			case PedWallaSettingsList::TYPE_ID: return PedWallaSettingsList::BASE_TYPE_ID;
			case CopDispatchInteractionSettings::TYPE_ID: return CopDispatchInteractionSettings::BASE_TYPE_ID;
			case RadioTrackTextIds::TYPE_ID: return RadioTrackTextIds::BASE_TYPE_ID;
			case RandomisedRadioEmitterSettings::TYPE_ID: return RandomisedRadioEmitterSettings::BASE_TYPE_ID;
			case TennisVocalizationSettings::TYPE_ID: return TennisVocalizationSettings::BASE_TYPE_ID;
			case DoorAudioSettingsLink::TYPE_ID: return DoorAudioSettingsLink::BASE_TYPE_ID;
			case SportsCarRevsSettings::TYPE_ID: return SportsCarRevsSettings::BASE_TYPE_ID;
			case FoliageSettings::TYPE_ID: return FoliageSettings::BASE_TYPE_ID;
			case ReplayRadioStationTrackList::TYPE_ID: return ReplayRadioStationTrackList::BASE_TYPE_ID;
			case MacsModelOverrideList::TYPE_ID: return MacsModelOverrideList::BASE_TYPE_ID;
			case MacsModelOverrideListBig::TYPE_ID: return MacsModelOverrideListBig::BASE_TYPE_ID;
			case SoundFieldDefinition::TYPE_ID: return SoundFieldDefinition::BASE_TYPE_ID;
			case GameObjectHashList::TYPE_ID: return GameObjectHashList::BASE_TYPE_ID;
			default: return 0xFFFFFFFF;
		}
	}
	
	// PURPOSE - Determines if a type inherits from another type
	bool gGameObjectsIsOfType(const rage::u32 objectTypeId, const rage::u32 baseTypeId)
	{
		if(objectTypeId == 0xFFFFFFFF)
		{
			return false;
		}
		else if(objectTypeId == baseTypeId)
		{
			return true;
		}
		else
		{
			return gGameObjectsIsOfType(gGameObjectsGetBaseTypeId(objectTypeId), baseTypeId);
		}
	}
} // namespace rage
