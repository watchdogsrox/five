///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxVehicleInfo.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_VEHICLEINFO_H
#define	VFX_VEHICLEINFO_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "vector/vector3.h"
#include "vectormath/vec3v.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

class CVehicle;
class CVehicleModelInfo;
class CVfxVehicleInfo;

namespace rage
{
	class parTreeNode;
}


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

#if __DEV
struct VfxVehicleLiveEditUpdateData
{
	CVfxVehicleInfo* pOrig;
	u32 hashName;
	CVfxVehicleInfo* pNew;
};
#endif


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxVehicleInfo
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CVfxVehicleInfo();

		// bang/scrape vfx
		float GetMtlBangPtFxVehicleEvo() const {return m_mtlBangPtFxVehicleEvo;}
		float GetMtlBangPtFxVehicleScale() const {return m_mtlBangPtFxVehicleScale;}
		float GetMtlScrapePtFxVehicleEvo() const {return m_mtlScrapePtFxVehicleEvo;}
		float GetMtlScrapePtFxVehicleScale() const {return m_mtlScrapePtFxVehicleScale;}

		// exhaust vfx
		bool GetExhaustPtFxEnabled() const {return m_exhaustPtFxEnabled;}
		atHashWithStringNotFinal GetExhaustPtFxName() const {return m_exhaustPtFxName;}
		float GetExhaustPtFxCutOffSpeed() const {return m_exhaustPtFxCutOffSpeed;}
		float GetExhaustPtFxRangeSqr() const {return m_exhaustPtFxRange*m_exhaustPtFxRange;}
		float GetExhaustPtFxScale() const {return m_exhaustPtFxScale;}
		float GetExhaustPtFxSpeedEvoMin() const {return m_exhaustPtFxSpeedEvoMin;}
		float GetExhaustPtFxSpeedEvoMax() const {return m_exhaustPtFxSpeedEvoMax;}
		float GetExhaustPtFxTempEvoMin() const {return m_exhaustPtFxTempEvoMin;}
		float GetExhaustPtFxTempEvoMax() const {return m_exhaustPtFxTempEvoMax;}
		bool GetExhaustPtFxThrottleEvoOnGearChange() const {return m_exhaustPtFxThrottleEvoOnGearChange;}

		// engine startup vfx
		bool GetEngineStartupPtFxEnabled() const {return m_engineStartupPtFxEnabled;}
		atHashWithStringNotFinal GetEngineStartupPtFxName() const {return m_engineStartupPtFxName;}
		float GetEngineStartupPtFxRangeSqr() const {return m_engineStartupPtFxRange*m_engineStartupPtFxRange;}

		// misfire vfx
		bool GetMisfirePtFxEnabled() const {return m_misfirePtFxEnabled;}
		atHashWithStringNotFinal GetMisfirePtFxName() const {return m_misfirePtFxName;}
		float GetMisfirePtFxRangeSqr() const {return m_misfirePtFxRange*m_misfirePtFxRange;}

		// backfire vfx
		bool GetBackfirePtFxEnabled() const {return m_backfirePtFxEnabled;}
		atHashWithStringNotFinal GetBackfirePtFxName() const {return m_backfirePtFxName;}
		float GetBackfirePtFxRangeSqr() const {return m_backfirePtFxRange*m_backfirePtFxRange;}

		// engine damage vfx
		bool GetEngineDamagePtFxEnabled() const {return m_engineDamagePtFxEnabled;}
		bool GetEngineDamagePtFxHasPanel() const {return m_engineDamagePtFxHasPanel;}
		bool GetEngineDamagePtFxHasRotorEvo() const {return m_engineDamagePtFxHasRotorEvo;}
		atHashWithStringNotFinal GetEngineDamagePtFxNoPanelName() const {return m_engineDamagePtFxNoPanelName;}
		atHashWithStringNotFinal GetEngineDamagePtFxPanelOpenName() const {return m_engineDamagePtFxPanelOpenName;}
		atHashWithStringNotFinal GetEngineDamagePtFxPanelShutName() const {return m_engineDamagePtFxPanelShutName;}
		float GetEngineDamagePtFxRangeSqr() const {return m_engineDamagePtFxRange*m_engineDamagePtFxRange;}
		float GetEngineDamagePtFxSpeedEvoMin() const {return m_engineDamagePtFxSpeedEvoMin;}
		float GetEngineDamagePtFxSpeedEvoMax() const {return m_engineDamagePtFxSpeedEvoMax;}

		// overturned smoke vfx
		bool GetOverturnedSmokePtFxEnabled() const {return m_overturnedSmokePtFxEnabled;}
		atHashWithStringNotFinal GetOverturnedSmokePtFxName() const {return m_overturnedSmokePtFxName;}
		float GetOverturnedSmokePtFxRangeSqr() const {return m_overturnedSmokePtFxRange*m_overturnedSmokePtFxRange;}
		float GetOverturnedSmokePtFxAngleThresh() const {return m_overturnedSmokePtFxAngleThresh;}
		float GetOverturnedSmokePtFxSpeedThresh() const {return m_overturnedSmokePtFxSpeedThresh;}
		float GetOverturnedSmokePtFxEngineHealthThresh() const {return m_overturnedSmokePtFxEngineHealthThresh;}

		// leak vfx
		bool GetLeakPtFxEnabled() const {return m_leakPtFxEnabled;}
		atHashWithStringNotFinal GetLeakPtFxOilName() const {return m_leakPtFxOilName;}
		atHashWithStringNotFinal GetLeakPtFxPetrolName() const {return m_leakPtFxPetrolName;}
		float GetLeakPtFxRangeSqr() const {return m_leakPtFxRange*m_leakPtFxRange;}
		float GetLeakPtFxSpeedEvoMin() const {return m_leakPtFxSpeedEvoMin;}
		float GetLeakPtFxSpeedEvoMax() const {return m_leakPtFxSpeedEvoMax;}

		// wheel generic vfx
		int GetWheelGenericPtFxSet() const {return m_wheelGenericPtFxSet;}
		int GetWheelGenericDecalSet() const {return m_wheelGenericDecalSet;}
		float GetWheelGenericRangeMult() const {return m_wheelGenericRangeMult;}
		float GetWheelGenericRangeMultSqr() const {return m_wheelGenericRangeMult*m_wheelGenericRangeMult;}
		bool GetWheelSkidmarkRearOnly() const {return m_wheelSkidmarkRearOnly;}
		float GetWheelSkidmarkSlipMult() const {return m_wheelSkidmarkSlipMult;}
		float GetWheelSkidmarkPressureMult() const {return m_wheelSkidmarkPressureMult;}
		float GetWheelFrictionPtFxFricMult() const {return m_wheelFrictionPtFxFricMult;}
		float GetWheelDisplacementPtFxDispMult() const {return m_wheelDisplacementPtFxDispMult;}
		float GetWheelBurnoutPtFxFricMult() const {return m_wheelBurnoutPtFxFricMult;}
		float GetWheelBurnoutPtFxTempMult() const {return m_wheelBurnoutPtFxTempMult;}

		// wheel low lod vfx
		float GetWheelLowLodPtFxScale() const {return m_wheelLowLodPtFxScale;}
		
		// wheel puncture vfx
		atHashWithStringNotFinal GetWheelPuncturePtFxName() const {return m_wheelPuncturePtFxName;}
		float GetWheelPuncturePtFxRangeSqr() const {return m_wheelPuncturePtFxRange*m_wheelPuncturePtFxRange;}

		// wheel burst vfx
		atHashWithStringNotFinal GetWheelBurstPtFxName() const {return m_wheelBurstPtFxName;}
		float GetWheelBurstPtFxRangeSqr() const {return m_wheelBurstPtFxRange*m_wheelBurstPtFxRange;}

		// wheel fire vfx
		atHashWithStringNotFinal GetWheelFirePtFxName() const {return m_wheelFirePtFxName;}
		float GetWheelFirePtFxRangeSqr() const {return m_wheelFirePtFxRange*m_wheelFirePtFxRange;}
		float GetWheelFirePtFxSpeedEvoMin() const {return m_wheelFirePtFxSpeedEvoMin;}
		float GetWheelFirePtFxSpeedEvoMax() const {return m_wheelFirePtFxSpeedEvoMax;}

		// wrecked fire vfx
		bool GetWreckedFirePtFxEnabled() const {return m_wreckedFirePtFxEnabled;}
		atHashWithStringNotFinal GetWreckedFirePtFxName() const {return m_wreckedFirePtFxName;}
		float GetWreckedFirePtFxDurationMin() const {return m_wreckedFirePtFxDurationMin;}
		float GetWreckedFirePtFxDurationMax() const {return m_wreckedFirePtFxDurationMax;}
		float GetWreckedFirePtFxRadius() const {return m_wreckedFirePtFxRadius;}

		bool GetWreckedFire2PtFxEnabled() const {return m_wreckedFire2PtFxEnabled;}
		atHashWithStringNotFinal GetWreckedFire2PtFxName() const {return m_wreckedFire2PtFxName;}
		float GetWreckedFire2PtFxDurationMin() const {return m_wreckedFire2PtFxDurationMin;}
		float GetWreckedFire2PtFxDurationMax() const {return m_wreckedFire2PtFxDurationMax;}
		float GetWreckedFire2PtFxRadius() const {return m_wreckedFire2PtFxRadius;}
		float GetWreckedFire2PtFxUseOverheatBone() const {return m_wreckedFire2UseOverheatBone;}
		Vec3V_Out GetWreckedFire2PtFxOffsetPos() const {return m_wreckedFire2OffsetPos;}

		bool GetWreckedFire3PtFxEnabled() const {return m_wreckedFire3PtFxEnabled;}
		atHashWithStringNotFinal GetWreckedFire3PtFxName() const {return m_wreckedFire3PtFxName;}
		float GetWreckedFire3PtFxDurationMin() const {return m_wreckedFire3PtFxDurationMin;}
		float GetWreckedFire3PtFxDurationMax() const {return m_wreckedFire3PtFxDurationMax;}
		float GetWreckedFire3PtFxRadius() const {return m_wreckedFire3PtFxRadius;}
		float GetWreckedFire3PtFxUseOverheatBone() const {return m_wreckedFire3UseOverheatBone;}
		Vec3V_Out GetWreckedFire3PtFxOffsetPos() const {return m_wreckedFire3OffsetPos;}

		// petrol tank fire vfx
		atHashWithStringNotFinal GetPetrolTankFirePtFxName() const {return m_petrolTankFirePtFxName;}
		float GetPetrolTankFirePtFxRangeSqr() const {return m_petrolTankFirePtFxRange*m_petrolTankFirePtFxRange;}
		float GetPetrolTankFirePtFxSpeedEvoMin() const {return m_petrolTankFirePtFxSpeedEvoMin;}
		float GetPetrolTankFirePtFxSpeedEvoMax() const {return m_petrolTankFirePtFxSpeedEvoMax;}
		float GetPetrolTankFirePtFxRadius() const {return m_petrolTankFirePtFxRadius;}

		// boat entry vfx
		bool GetBoatEntryPtFxEnabled() const {return m_boatEntryPtFxEnabled;}
		float GetBoatEntryPtFxRangeSqr() const {return m_boatEntryPtFxRange*m_boatEntryPtFxRange;}
		atHashWithStringNotFinal GetBoatEntryPtFxName() const {return m_boatEntryPtFxName;}
		float GetBoatEntryPtFxSpeedEvoMin() const {return m_boatEntryPtFxSpeedEvoMin;}
		float GetBoatEntryPtFxSpeedEvoMax() const {return m_boatEntryPtFxSpeedEvoMax;}
		float GetBoatEntryPtFxScale() const {return m_boatEntryPtFxScale;}

		// boat exit vfx
		bool GetBoatExitPtFxEnabled() const {return m_boatExitPtFxEnabled;}
		float GetBoatExitPtFxRangeSqr() const {return m_boatExitPtFxRange*m_boatExitPtFxRange;}
		atHashWithStringNotFinal GetBoatExitPtFxName() const {return m_boatExitPtFxName;}
		float GetBoatExitPtFxSpeedEvoMin() const {return m_boatExitPtFxSpeedEvoMin;}
		float GetBoatExitPtFxSpeedEvoMax() const {return m_boatExitPtFxSpeedEvoMax;}
		float GetBoatExitPtFxScale() const {return m_boatExitPtFxScale;}

		// boat bow vfx
		bool GetBoatBowPtFxEnabled() const {return m_boatBowPtFxEnabled;}
		float GetBoatBowPtFxRangeSqr() const {return m_boatBowPtFxRange*m_boatBowPtFxRange;}
		atHashWithStringNotFinal GetBoatBowPtFxForwardName() const {return m_boatBowPtFxForwardName;}
		atHashWithStringNotFinal GetBoatBowPtFxReverseName() const {return m_boatBowPtFxReverseName;}
		atHashWithStringNotFinal GetBoatBowPtFxForwardMountedName() const {return m_boatBowPtFxForwardMountedName;}
		Vec3V_Out GetBoatBowPtFxForwardMountedOffset() const {return m_boatBowPtFxForwardMountedOffset;}
		float GetBoatBowPtFxSpeedEvoMin() const {return m_boatBowPtFxSpeedEvoMin;}
		float GetBoatBowPtFxSpeedEvoMax() const {return m_boatBowPtFxSpeedEvoMax;}
		float GetBoatBowPtFxKeelEvoMin() const {return m_boatBowPtFxKeelEvoMin;}
		float GetBoatBowPtFxKeelEvoMax() const {return m_boatBowPtFxKeelEvoMax;}
		float GetBoatBowPtFxScale() const {return m_boatBowPtFxScale;}
		float GetBoatBowPtFxReverseOffset() const {return m_boatBowPtFxReverseOffset;}

		// boat wash vfx
		bool GetBoatWashPtFxEnabled() const {return m_boatWashPtFxEnabled;}
		float GetBoatWashPtFxRangeSqr() const {return m_boatWashPtFxRange*m_boatWashPtFxRange;}
		atHashWithStringNotFinal GetBoatWashPtFxName() const {return m_boatWashPtFxName;}
		float GetBoatWashPtFxSpeedEvoMin() const {return m_boatWashPtFxSpeedEvoMin;}
		float GetBoatWashPtFxSpeedEvoMax() const {return m_boatWashPtFxSpeedEvoMax;}
		float GetBoatWashPtFxScale() const {return m_boatWashPtFxScale;}

		// boat propeller vfx
		bool GetBoatPropellerPtFxEnabled() const {return m_boatPropellerPtFxEnabled;}
		float GetBoatPropellerPtFxRangeSqr() const {return m_boatPropellerPtFxRange*m_boatPropellerPtFxRange;}
		atHashWithStringNotFinal GetBoatPropellerPtFxName() const {return m_boatPropellerPtFxName;}
		float GetBoatPropellerPtFxForwardSpeedEvoMin() const {return m_boatPropellerPtFxForwardSpeedEvoMin;}
		float GetBoatPropellerPtFxForwardSpeedEvoMax() const {return m_boatPropellerPtFxForwardSpeedEvoMax;}
		float GetBoatPropellerPtFxBackwardSpeedEvoMin() const {return m_boatPropellerPtFxBackwardSpeedEvoMin;}
		float GetBoatPropellerPtFxBackwardSpeedEvoMax() const {return m_boatPropellerPtFxBackwardSpeedEvoMax;}
		float GetBoatPropellerPtFxDepthEvoMin() const {return m_boatPropellerPtFxDepthEvoMin;}
		float GetBoatPropellerPtFxDepthEvoMax() const {return m_boatPropellerPtFxDepthEvoMax;}
		float GetBoatPropellerPtFxScale() const {return m_boatPropellerPtFxScale;}

		// boat low lod wake vfx
		bool GetBoatLowLodWakePtFxEnabled() const {return m_boatLowLodWakePtFxEnabled;}
		float GetBoatLowLodWakePtFxRangeMinSqr() const {return m_boatLowLodWakePtFxRangeMin*m_boatLowLodWakePtFxRangeMin;}
		float GetBoatLowLodWakePtFxRangeMaxSqr() const {return m_boatLowLodWakePtFxRangeMax*m_boatLowLodWakePtFxRangeMax;}
		atHashWithStringNotFinal GetBoatLowLodWakePtFxName() const {return m_boatLowLodWakePtFxName;}
		float GetBoatLowLodWakePtFxForwardSpeedEvoMin() const {return m_boatLowLodWakePtFxSpeedEvoMin;}
		float GetBoatLowLodWakePtFxForwardSpeedEvoMax() const {return m_boatLowLodWakePtFxSpeedEvoMax;}
		float GetBoatLowLodWakePtFxScale() const {return m_boatLowLodWakePtFxScale;}

		// plane afterburner vfx
		bool GetPlaneAfterburnerPtFxEnabled() const {return m_planeAfterburnerPtFxEnabled;}
		atHashWithStringNotFinal GetPlaneAfterburnerPtFxName() const {return m_planeAfterburnerPtFxName;}
		float GetPlaneAfterburnerPtFxRangeSqr() const {return m_planeAfterburnerPtFxRange*m_planeAfterburnerPtFxRange;}
		float GetPlaneAfterburnerPtFxScale() const {return m_planeAfterburnerPtFxScale;}

		// plane wingtip vfx
		bool GetPlaneWingTipPtFxEnabled() const {return m_planeWingTipPtFxEnabled;}
		atHashWithStringNotFinal GetPlaneWingTipPtFxName() const {return m_planeWingTipPtFxName;}
		float GetPlaneWingTipPtFxRangeSqr() const {return m_planeWingTipPtFxRange*m_planeWingTipPtFxRange;}
		float GetPlaneWingTipPtFxSpeedEvoMin() const {return m_planeWingTipPtFxSpeedEvoMin;}
		float GetPlaneWingTipPtFxSpeedEvoMax() const {return m_planeWingTipPtFxSpeedEvoMax;}

		// plane damage fire vfx
		bool GetPlaneDamageFirePtFxEnabled() const {return m_planeDamageFirePtFxEnabled;}
		atHashWithStringNotFinal GetPlaneDamageFirePtFxName() const {return m_planeDamageFirePtFxName;}
		float GetPlaneDamageFirePtFxRangeSqr() const {return m_planeDamageFirePtFxRange*m_planeDamageFirePtFxRange;}
		float GetPlaneDamageFirePtFxSpeedEvoMin() const {return m_planeDamageFirePtFxSpeedEvoMin;}
		float GetPlaneDamageFirePtFxSpeedEvoMax() const {return m_planeDamageFirePtFxSpeedEvoMax;}

		// plane ground disturbance vfx
		bool GetPlaneGroundDisturbPtFxEnabled() const {return m_planeGroundDisturbPtFxEnabled;}
		atHashWithStringNotFinal GetPlaneGroundDisturbPtFxNameDefault() const {return m_planeGroundDisturbPtFxNameDefault;}
		atHashWithStringNotFinal GetPlaneGroundDisturbPtFxNameSand() const {return m_planeGroundDisturbPtFxNameSand;}
		atHashWithStringNotFinal GetPlaneGroundDisturbPtFxNameDirt() const {return m_planeGroundDisturbPtFxNameDirt;}
		atHashWithStringNotFinal GetPlaneGroundDisturbPtFxNameWater() const {return m_planeGroundDisturbPtFxNameWater;}
		atHashWithStringNotFinal GetPlaneGroundDisturbPtFxNameFoliage() const {return m_planeGroundDisturbPtFxNameFoliage;}
		float GetPlaneGroundDisturbPtFxRangeSqr() const {return m_planeGroundDisturbPtFxRange*m_planeGroundDisturbPtFxRange;}
		float GetPlaneGroundDisturbPtFxDist() const {return m_planeGroundDisturbPtFxDist;}
		float GetPlaneGroundDisturbPtFxSpeedEvoMin() const {return m_planeGroundDisturbPtFxSpeedEvoMin;}
		float GetPlaneGroundDisturbPtFxSpeedEvoMax() const {return m_planeGroundDisturbPtFxSpeedEvoMax;}

		// aircraft section damage smoke vfx
		bool GetAircraftSectionDamageSmokePtFxEnabled() const {return m_aircraftSectionDamageSmokePtFxEnabled;}
		atHashWithStringNotFinal GetAircraftSectionDamageSmokePtFxName() const {return m_aircraftSectionDamageSmokePtFxName;}
		float GetAircraftSectionDamageSmokePtFxRangeSqr() const {return m_aircraftSectionDamageSmokePtFxRange*m_aircraftSectionDamageSmokePtFxRange;}
		float GetAircraftSectionDamageSmokePtFxSpeedEvoMin() const {return m_aircraftSectionDamageSmokePtFxSpeedEvoMin;}
		float GetAircraftSectionDamageSmokePtFxSpeedEvoMax() const {return m_aircraftSectionDamageSmokePtFxSpeedEvoMax;}

		// aircraft downwash
		bool GetAircraftDownwashPtFxEnabled() const {return m_aircraftDownwashPtFxEnabled;}
		atHashWithStringNotFinal GetAircraftDownwashPtFxNameDefault() const {return m_aircraftDownwashPtFxNameDefault;}
		atHashWithStringNotFinal GetAircraftDownwashPtFxNameSand() const {return m_aircraftDownwashPtFxNameSand;}
		atHashWithStringNotFinal GetAircraftDownwashPtFxNameDirt() const {return m_aircraftDownwashPtFxNameDirt;}
		atHashWithStringNotFinal GetAircraftDownwashPtFxNameWater() const {return m_aircraftDownwashPtFxNameWater;}
		atHashWithStringNotFinal GetAircraftDownwashPtFxNameFoliage() const {return m_aircraftDownwashPtFxNameFoliage;}
		float GetAircraftDownwashPtFxRangeSqr() const {return m_aircraftDownwashPtFxRange*m_aircraftDownwashPtFxRange;}
		float GetAircraftDownwashPtFxDist() const {return m_aircraftDownwashPtFxDist;}
		float GetAircraftDownwashPtFxSpeedEvoMin() const {return m_aircraftDownwashPtFxSpeedEvoMin;}
		float GetAircraftDownwashPtFxSpeedEvoMax() const {return m_aircraftDownwashPtFxSpeedEvoMax;}

		// splash in vfx
		bool GetSplashInPtFxEnabled() const {return m_splashInPtFxEnabled;}
		float GetSplashInPtFxRangeSqr() const {return m_splashInPtFxRange*m_splashInPtFxRange;}
		atHashWithStringNotFinal GetSplashInPtFxName() const {return m_splashInPtFxName;}
		float GetSplashInPtFxSizeEvoMax() const {return m_splashInPtFxSizeEvoMax;}
		float GetSplashInPtFxSpeedDownwardThresh() const {return m_splashInPtFxSpeedDownwardThresh;}
		float GetSplashInPtFxSpeedLateralEvoMin() const {return m_splashInPtFxSpeedLateralEvoMin;}
		float GetSplashInPtFxSpeedLateralEvoMax() const {return m_splashInPtFxSpeedLateralEvoMax;}
		float GetSplashInPtFxSpeedDownwardEvoMin() const {return m_splashInPtFxSpeedDownwardEvoMin;}
		float GetSplashInPtFxSpeedDownwardEvoMax() const {return m_splashInPtFxSpeedDownwardEvoMax;}

		// splash out vfx
		bool GetSplashOutPtFxEnabled() const {return m_splashOutPtFxEnabled;}
		float GetSplashOutPtFxRangeSqr() const {return m_splashOutPtFxRange*m_splashOutPtFxRange;}
		atHashWithStringNotFinal GetSplashOutPtFxName() const {return m_splashOutPtFxName;}
		float GetSplashOutPtFxSizeEvoMax() const {return m_splashOutPtFxSizeEvoMax;}
		float GetSplashOutPtFxSpeedLateralEvoMin() const {return m_splashOutPtFxSpeedLateralEvoMin;}
		float GetSplashOutPtFxSpeedLateralEvoMax() const {return m_splashOutPtFxSpeedLateralEvoMax;}
		float GetSplashOutPtFxSpeedUpwardEvoMin() const {return m_splashOutPtFxSpeedUpwardEvoMin;}
		float GetSplashOutPtFxSpeedUpwardEvoMax() const {return m_splashOutPtFxSpeedUpwardEvoMax;}

		// splash wade vfx
		bool GetSplashWadePtFxEnabled() const {return m_splashWadePtFxEnabled;}
		float GetSplashWadePtFxRangeSqr() const {return m_splashWadePtFxRange*m_splashWadePtFxRange;}
		atHashWithStringNotFinal GetSplashWadePtFxName() const {return m_splashWadePtFxName;}
		float GetSplashWadePtFxSizeEvoMax() const {return m_splashWadePtFxSizeEvoMax;}
		float GetSplashWadePtFxSpeedVehicleEvoMin() const {return m_splashWadePtFxSpeedVehicleEvoMin;}
		float GetSplashWadePtFxSpeedVehicleEvoMax() const {return m_splashWadePtFxSpeedVehicleEvoMax;}
		float GetSplashWadePtFxSpeedRiverEvoMin() const {return m_splashWadePtFxSpeedRiverEvoMin;}
		float GetSplashWadePtFxSpeedRiverEvoMax() const {return m_splashWadePtFxSpeedRiverEvoMax;}

		// splash trail vfx
		bool GetSplashTrailPtFxEnabled() const {return m_splashTrailPtFxEnabled;}
		float GetSplashTrailPtFxRangeSqr() const {return m_splashTrailPtFxRange*m_splashTrailPtFxRange;}
		atHashWithStringNotFinal GetSplashTrailPtFxName() const {return m_splashTrailPtFxName;}
		float GetSplashTrailPtFxSizeEvoMax() const {return m_splashTrailPtFxSizeEvoMax;}
		float GetSplashTrailPtFxSpeedEvoMin() const {return m_splashTrailPtFxSpeedEvoMin;}
		float GetSplashTrailPtFxSpeedEvoMax() const {return m_splashTrailPtFxSpeedEvoMax;}


	private: //////////////////////////

		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// bang/scrape vfx
		float m_mtlBangPtFxVehicleEvo;
		float m_mtlBangPtFxVehicleScale;
		float m_mtlScrapePtFxVehicleEvo;
		float m_mtlScrapePtFxVehicleScale;

		// exhaust vfx
		bool m_exhaustPtFxEnabled;
		atHashWithStringNotFinal m_exhaustPtFxName;
		float m_exhaustPtFxCutOffSpeed;
		float m_exhaustPtFxRange;
		float m_exhaustPtFxScale;
		float m_exhaustPtFxSpeedEvoMin;
		float m_exhaustPtFxSpeedEvoMax;
		float m_exhaustPtFxTempEvoMin;
		float m_exhaustPtFxTempEvoMax;
		bool m_exhaustPtFxThrottleEvoOnGearChange;

		// engine startup vfx
		bool m_engineStartupPtFxEnabled;
		atHashWithStringNotFinal m_engineStartupPtFxName;
		float m_engineStartupPtFxRange;

		// misfire vfx
		bool m_misfirePtFxEnabled;
		atHashWithStringNotFinal m_misfirePtFxName;
		float m_misfirePtFxRange;

		// backfire vfx
		bool m_backfirePtFxEnabled;
		atHashWithStringNotFinal m_backfirePtFxName;
		float m_backfirePtFxRange;

		// engine damage vfx
		bool m_engineDamagePtFxEnabled;
		bool m_engineDamagePtFxHasPanel;
		bool m_engineDamagePtFxHasRotorEvo;
		atHashWithStringNotFinal m_engineDamagePtFxNoPanelName;
		atHashWithStringNotFinal m_engineDamagePtFxPanelOpenName;
		atHashWithStringNotFinal m_engineDamagePtFxPanelShutName;
		float m_engineDamagePtFxRange;
		float m_engineDamagePtFxSpeedEvoMin;
		float m_engineDamagePtFxSpeedEvoMax;

		// overturned smoke vfx
		bool m_overturnedSmokePtFxEnabled;
		atHashWithStringNotFinal m_overturnedSmokePtFxName;
		float m_overturnedSmokePtFxRange;
		float m_overturnedSmokePtFxAngleThresh;
		float m_overturnedSmokePtFxSpeedThresh;
		float m_overturnedSmokePtFxEngineHealthThresh;

		// leak vfx
		bool m_leakPtFxEnabled;
		atHashWithStringNotFinal m_leakPtFxOilName;
		atHashWithStringNotFinal m_leakPtFxPetrolName;
		float m_leakPtFxRange;
		float m_leakPtFxSpeedEvoMin;
		float m_leakPtFxSpeedEvoMax;

		// wheel generic vfx
		int m_wheelGenericPtFxSet;
		int m_wheelGenericDecalSet;
		float m_wheelGenericRangeMult;
		bool m_wheelSkidmarkRearOnly;
		float m_wheelSkidmarkSlipMult;
		float m_wheelSkidmarkPressureMult;
		float m_wheelFrictionPtFxFricMult;
		float m_wheelDisplacementPtFxDispMult;
		float m_wheelBurnoutPtFxFricMult;
		float m_wheelBurnoutPtFxTempMult;

		// wheel low lod vfx
		float m_wheelLowLodPtFxScale;

		// wheel puncture vfx
		atHashWithStringNotFinal m_wheelPuncturePtFxName;
		float m_wheelPuncturePtFxRange;

		// wheel burst vfx
		atHashWithStringNotFinal m_wheelBurstPtFxName;
		float m_wheelBurstPtFxRange;

		// wheel fire vfx
		atHashWithStringNotFinal m_wheelFirePtFxName;
		float m_wheelFirePtFxRange;
		float m_wheelFirePtFxSpeedEvoMin;
		float m_wheelFirePtFxSpeedEvoMax;

		// wrecked fire vfx
		bool m_wreckedFirePtFxEnabled;
		atHashWithStringNotFinal m_wreckedFirePtFxName;
		float m_wreckedFirePtFxDurationMin;
		float m_wreckedFirePtFxDurationMax;
		float m_wreckedFirePtFxRadius;

		bool m_wreckedFire2PtFxEnabled;
		atHashWithStringNotFinal m_wreckedFire2PtFxName;
		float m_wreckedFire2PtFxDurationMin;
		float m_wreckedFire2PtFxDurationMax;
		float m_wreckedFire2PtFxRadius;
		bool m_wreckedFire2UseOverheatBone;
		Vec3V m_wreckedFire2OffsetPos;

		bool m_wreckedFire3PtFxEnabled;
		atHashWithStringNotFinal m_wreckedFire3PtFxName;
		float m_wreckedFire3PtFxDurationMin;
		float m_wreckedFire3PtFxDurationMax;
		float m_wreckedFire3PtFxRadius;
		bool m_wreckedFire3UseOverheatBone;
		Vec3V m_wreckedFire3OffsetPos;

		// petrol tank fire vfx
		atHashWithStringNotFinal m_petrolTankFirePtFxName;
		float m_petrolTankFirePtFxRange;
		float m_petrolTankFirePtFxSpeedEvoMin;
		float m_petrolTankFirePtFxSpeedEvoMax;
		float m_petrolTankFirePtFxRadius;

		// boat entry vfx
		bool m_boatEntryPtFxEnabled;
		float m_boatEntryPtFxRange;
		atHashWithStringNotFinal m_boatEntryPtFxName;
		float m_boatEntryPtFxSpeedEvoMin;
		float m_boatEntryPtFxSpeedEvoMax;
		float m_boatEntryPtFxScale;

		// boat exit vfx
		bool m_boatExitPtFxEnabled;
		float m_boatExitPtFxRange;
		atHashWithStringNotFinal m_boatExitPtFxName;
		float m_boatExitPtFxSpeedEvoMin;
		float m_boatExitPtFxSpeedEvoMax;
		float m_boatExitPtFxScale;

		// boat bow vfx
		bool m_boatBowPtFxEnabled;
		float m_boatBowPtFxRange;
		atHashWithStringNotFinal m_boatBowPtFxForwardName;
		atHashWithStringNotFinal m_boatBowPtFxReverseName;
		atHashWithStringNotFinal m_boatBowPtFxForwardMountedName;
		Vec3V m_boatBowPtFxForwardMountedOffset;
		float m_boatBowPtFxSpeedEvoMin;
		float m_boatBowPtFxSpeedEvoMax;
		float m_boatBowPtFxKeelEvoMin;
		float m_boatBowPtFxKeelEvoMax;
		float m_boatBowPtFxScale;
		float m_boatBowPtFxReverseOffset;
	
		// boat wash vfx
		bool m_boatWashPtFxEnabled;
		float m_boatWashPtFxRange;
		atHashWithStringNotFinal m_boatWashPtFxName;
		float m_boatWashPtFxSpeedEvoMin;
		float m_boatWashPtFxSpeedEvoMax;
		float m_boatWashPtFxScale;

		// boat propeller vfx
		bool m_boatPropellerPtFxEnabled;
		float m_boatPropellerPtFxRange;
		atHashWithStringNotFinal m_boatPropellerPtFxName;
		float m_boatPropellerPtFxForwardSpeedEvoMin;
		float m_boatPropellerPtFxForwardSpeedEvoMax;
		float m_boatPropellerPtFxBackwardSpeedEvoMin;
		float m_boatPropellerPtFxBackwardSpeedEvoMax;
		float m_boatPropellerPtFxDepthEvoMin;
		float m_boatPropellerPtFxDepthEvoMax;
		float m_boatPropellerPtFxScale;

		// boat low lod wake vfx
		bool m_boatLowLodWakePtFxEnabled;
		float m_boatLowLodWakePtFxRangeMin;
		float m_boatLowLodWakePtFxRangeMax;
		atHashWithStringNotFinal m_boatLowLodWakePtFxName;
		float m_boatLowLodWakePtFxSpeedEvoMin;
		float m_boatLowLodWakePtFxSpeedEvoMax;
		float m_boatLowLodWakePtFxScale;

		// plane afterburner vfx
		bool m_planeAfterburnerPtFxEnabled;
		atHashWithStringNotFinal m_planeAfterburnerPtFxName;
		float m_planeAfterburnerPtFxRange;
		float m_planeAfterburnerPtFxScale;

		// plane wing tip vfx
		bool m_planeWingTipPtFxEnabled;
		atHashWithStringNotFinal m_planeWingTipPtFxName;
		float m_planeWingTipPtFxRange;
		float m_planeWingTipPtFxSpeedEvoMin;
		float m_planeWingTipPtFxSpeedEvoMax;

		// plane damage fire vfx
		bool m_planeDamageFirePtFxEnabled;
		atHashWithStringNotFinal m_planeDamageFirePtFxName;
		float m_planeDamageFirePtFxRange;
		float m_planeDamageFirePtFxSpeedEvoMin;
		float m_planeDamageFirePtFxSpeedEvoMax;

		// plane ground disturbance vfx
		bool m_planeGroundDisturbPtFxEnabled;
		atHashWithStringNotFinal m_planeGroundDisturbPtFxNameDefault;
		atHashWithStringNotFinal m_planeGroundDisturbPtFxNameSand;
		atHashWithStringNotFinal m_planeGroundDisturbPtFxNameDirt;
		atHashWithStringNotFinal m_planeGroundDisturbPtFxNameWater;
		atHashWithStringNotFinal m_planeGroundDisturbPtFxNameFoliage;
		float m_planeGroundDisturbPtFxRange;
		float m_planeGroundDisturbPtFxDist;
		float m_planeGroundDisturbPtFxSpeedEvoMin;
		float m_planeGroundDisturbPtFxSpeedEvoMax;

		// aircraft section damage smoke vfx
		bool m_aircraftSectionDamageSmokePtFxEnabled;
		atHashWithStringNotFinal m_aircraftSectionDamageSmokePtFxName;
		float m_aircraftSectionDamageSmokePtFxRange;
		float m_aircraftSectionDamageSmokePtFxSpeedEvoMin;
		float m_aircraftSectionDamageSmokePtFxSpeedEvoMax;

		// aircraft downwash
		bool m_aircraftDownwashPtFxEnabled;
		atHashWithStringNotFinal m_aircraftDownwashPtFxNameDefault;
		atHashWithStringNotFinal m_aircraftDownwashPtFxNameSand;
		atHashWithStringNotFinal m_aircraftDownwashPtFxNameDirt;
		atHashWithStringNotFinal m_aircraftDownwashPtFxNameWater;
		atHashWithStringNotFinal m_aircraftDownwashPtFxNameFoliage;
		float m_aircraftDownwashPtFxRange;
		float m_aircraftDownwashPtFxDist;
		float m_aircraftDownwashPtFxSpeedEvoMin;
		float m_aircraftDownwashPtFxSpeedEvoMax;

		// splash in vfx
		bool m_splashInPtFxEnabled;
		float m_splashInPtFxRange;
		atHashWithStringNotFinal m_splashInPtFxName;
		float m_splashInPtFxSizeEvoMax;
		float m_splashInPtFxSpeedDownwardThresh;
		float m_splashInPtFxSpeedLateralEvoMin;
		float m_splashInPtFxSpeedLateralEvoMax;
		float m_splashInPtFxSpeedDownwardEvoMin;
		float m_splashInPtFxSpeedDownwardEvoMax;

		// splash out vfx
		bool m_splashOutPtFxEnabled;
		float m_splashOutPtFxRange;
		atHashWithStringNotFinal m_splashOutPtFxName;
		float m_splashOutPtFxSizeEvoMax;
		float m_splashOutPtFxSpeedLateralEvoMin;
		float m_splashOutPtFxSpeedLateralEvoMax;
		float m_splashOutPtFxSpeedUpwardEvoMin;
		float m_splashOutPtFxSpeedUpwardEvoMax;

		// splash wade vfx
		bool m_splashWadePtFxEnabled;
		float m_splashWadePtFxRange;
		atHashWithStringNotFinal m_splashWadePtFxName;
		float m_splashWadePtFxSizeEvoMax;
		float m_splashWadePtFxSpeedVehicleEvoMin;
		float m_splashWadePtFxSpeedVehicleEvoMax;
		float m_splashWadePtFxSpeedRiverEvoMin;
		float m_splashWadePtFxSpeedRiverEvoMax;

		// splash trail vfx
		bool m_splashTrailPtFxEnabled;
		float m_splashTrailPtFxRange;
		atHashWithStringNotFinal m_splashTrailPtFxName;
		float m_splashTrailPtFxSizeEvoMax;
		float m_splashTrailPtFxSpeedEvoMin;
		float m_splashTrailPtFxSpeedEvoMax;


};

class CVfxVehicleInfoMgr
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CVfxVehicleInfoMgr();

		// parser
		void PreLoadCallback(parTreeNode* pNode);
		void PostLoadCallback();

		void LoadData();
		bool AppendData(const char* fileName);

		CVfxVehicleInfo* GetInfo(atHashString name);
		CVfxVehicleInfo* GetInfo(CVehicle* pVehicle);

#if __DEV
		static void UpdateVehicleModelInfo(CVehicleModelInfo* pVehicleModelInfo);
#endif


	private: //////////////////////////


		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		atBinaryMap<CVfxVehicleInfo*, atHashWithStringNotFinal> m_vfxVehicleInfos;

#if __DEV
		atArray<VfxVehicleLiveEditUpdateData> m_vfxVehicleLiveEditUpdateData;
#endif

};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxVehicleInfoMgr g_vfxVehicleInfoMgr;


#endif // VFX_VEHICLEINFO_H



