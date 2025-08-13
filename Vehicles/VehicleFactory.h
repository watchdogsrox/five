/////////////////////////////////////////////////////////////////////////////////
// Title	:	VehicleFactory.h
// Author(s):	
// Started	:
// description:	A Factory class for Vehicles
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_VEHICLEFACTORY_H_
#define INC_VEHICLEFACTORY_H_

// C headers

// Rage headers
#include "fwutil/PtrList.h"

// Game headers
#include "scene/EntityTypes.h"
#include "scene/RegdRefTypes.h"

// Forward declarations
class CTrailer;
class CVehicle;
namespace rage { class Matrix34; class bkBank; };
struct ObjectNameIdAssociation;
class CLoadedModelGroup;


class CVehicleFactory
{
public:
	CVehicleFactory();
	virtual ~CVehicleFactory();

	static void CreateFactory();
	static void DestroyFactory();
	static CVehicleFactory* GetFactory() {return sm_pInstance;}

	virtual CVehicle* Create(fwModelId modelId, const eEntityOwnedBy ownedBy, const u32 popType, const Matrix34 *pMat, bool bClone = true, bool bCreateAsInactive = false);
	virtual CVehicle* CreateAndAttachTrailer(fwModelId trailerId, CVehicle* targetVehicle, const Matrix34& creationMatrix, const eEntityOwnedBy ownedBy, const u32 popType, const bool bExtendBoxesForCollisionTests);
	virtual void AddCarsOnTrailer(CTrailer* trailer, fwInteriorLocation location, eEntityOwnedBy ownedBy, ePopType popType, fwModelId* carsToSpawn);
	virtual bool AddBoatOnTrailer(CTrailer* trailer, fwInteriorLocation location, eEntityOwnedBy ownedBy, ePopType popType, fwModelId* boatToSpawn);
	virtual void DetachedBoatOnTrailer();
	static bool DestroyBoatTrailerIfNecessary(CTrailer** ppOutTrailer);
	static bool GetPositionAndRotationForCargoTrailer(CTrailer* trailer, const int index, CVehicle* pVehicle, Vector3& vPosOut, Quaternion& qRotationOut);
	virtual void Destroy(CVehicle* pVehicle, bool cached = false);
	virtual void DestroyChildrenVehicles(CVehicle * pVehicle, bool cached);
	virtual void DeleteOrCacheVehicle(CVehicle * pVehicle, bool cached);

	virtual ObjectNameIdAssociation* GetBaseVehicleTypeDesc(int nType);
	virtual ObjectNameIdAssociation* GetExtraVehicleTypeDesc(int nType);
	virtual ObjectNameIdAssociation* GetExtraLandingGearTypeDesc();

	virtual u16* GetBaseVehicleTypeDescHashes(int nType);
	virtual u16* GetExtraVehicleTypeDescHashes(int nType);
	virtual u16* GetExtraLandingGearTypeDescHashes();
	virtual bool GetVehicleType(char* name, s32& nVehicleType);

	CVehicle* CreateVehicle(s32 nVehicleType, const eEntityOwnedBy ownedBy, const u32 popType);

	void AddDestroyedVehToCache(CVehicle* veh);
	void ClearDestroyedVehCache();
	static void ProcessDestroyedVehsCache();
	bool IsVehInDestroyedCache(CVehicle* veh);
	void RemoveVehGroupFromCache(const CLoadedModelGroup& vehGroup);

	bool RegisterVehicleToNetwork(CVehicle* pReturnVehicle, const u32 popType);

	static void ModifyVehicleTopSpeed(CVehicle* pVehicle, const float fPercentAdjustment);

	void SetFarDrawAllVehicles(bool val);
	void SetFarDraw(CVehicle* pVehicle);

public:
	static ObjectNameIdAssociation* pVehModelDesc;
    static ObjectNameIdAssociation* pQuadBikeModelOverrideDesc;
	static ObjectNameIdAssociation* pBikeModelOverrideDesc;
	static ObjectNameIdAssociation* pBoatModelOverrideDesc;
	static ObjectNameIdAssociation* pTrainModelOverrideDesc;
	static ObjectNameIdAssociation* pHeliModelOverrideDesc;
	static ObjectNameIdAssociation* pBlimpModelOverrideDesc;
	static ObjectNameIdAssociation* pPlaneModelOverrideDesc;
	static ObjectNameIdAssociation* pSubModelOverrideDesc;
	static ObjectNameIdAssociation* pTrailerModelOverrideDesc;
	static ObjectNameIdAssociation* pDraftVehModelOverrideDesc;
	static ObjectNameIdAssociation* pSubmarineCarModelOverrideDesc;
	static ObjectNameIdAssociation* pAmphibiousAutomobileModelOverrideDesc;
	static ObjectNameIdAssociation* pAmphibiousQuadModelOverrideDesc;
	static ObjectNameIdAssociation* pBmxModelOverrideDesc;
	static ObjectNameIdAssociation* pLandingGearModelOverrideDesc;

	static u16 pVehModelDescHashes[];
	static u16 pQuadBikeModelOverrideDescHashes[];
	static u16 pBikeModelOverrideDescHashes[];
	static u16 pBoatModelOverrideDescHashes[];
	static u16 pTrainModelOverrideDescHashes[];
	static u16 pHeliModelOverrideDescHashes[];
	static u16 pBlimpModelOverrideDescHashes[];
	static u16 pPlaneModelOverrideDescHashes[];
	static u16 pSubModelOverrideDescHashes[];
	static u16 pTrailerModelOverrideDescHashes[];
	static u16 pDraftVehModelOverrideDescHashes[];
	static u16 pSubmarineCarModelOverrideDescHashes[];
	static u16 pAmphibiousAutomobileModelOverrideDescHashes[];
	static u16 pAmphibiousQuadModelOverrideDescHashes[];
	static u16 pLandingGearModelOverrideDescHashes[];
	static u16 pBmxModelOverrideDescHashes[];

	static ObjectNameIdAssociation* ms_modelDesc[];
	static u16* ms_modelDescHashes[];
	static const char* ms_modelTypes[];
	static const char* ms_swankTypes[];
	static const char* ms_classTypes[];

	static bool						ms_reuseDestroyedVehs;
protected:
	static CVehicleFactory* sm_pInstance;

	struct sDestroyedVeh
	{
		CVehicle* veh;
		u32 nukeTime;
		s8 framesUntilReuse;
	};
	enum { MAX_DESTROYED_VEHS_CACHED = 50 };
	static sDestroyedVeh	ms_reuseDestroyedVehArray[MAX_DESTROYED_VEHS_CACHED];
	static u32				ms_reuseDestroyedVehCacheTime;
	static u32				ms_reuseDestroyedVehCount;

	static bool				ms_bFarDrawVehicles;

	static const u8		ms_uMaxBoatTrailersWithoutBoats = 1;
	static const u8		ms_uMaxBoatTrailersWithBoats		= 2;
	static u8					ms_uCurrentBoatTrailersWithoutBoats;
	static u8					ms_uCurrentBoatTrailersWithBoats;

	void AssignVehicleGadgets(CVehicle* pVehicle);

	CVehicle* CreateVehFromDestroyedCache(fwModelId modelId, const eEntityOwnedBy ownedBy, u32 popType, const Matrix34 *pMat, bool bCreateAsInactive = false);

#if __BANK
public:
	static bool InitWidgets(void);
	static void CreateBank(void);
	static void UpdateCarList(void);
	static void UpdateVehicleList(void);
	static void UpdateVehicleTypeList(void);
	static CVehicle *GetCreatedVehicle(void);
	static CVehicle *GetPreviouslyCreatedVehicle(void);
	static void CarNumChangeCB();
	static void TrailerNumChangeCB();
	static void CreateCar(u32 modelId = fwModelId::MI_INVALID);
	static void WarpPlayerOutOfCar();
	static void WarpPlayerIntoRecentCar();

	static void PlayTestVehicleAnimCB();

	static void VariationDebugUpdateColor();
	static inline int VariationDebugGetLastVehMod(eVehicleModType slot);
	static void VariationDebugUpdateMods();
	static void VariationDebugColorChangeCB();
	static void VariationDebugWindowTintChangeCB();
	static void VariationDebugModChangeCB();
	static void VariationDebugAddModKitCB();
	static void SetSprayedCB();
	static void ClearSprayedCB();
	static void VariationDebugExtrasChangeCB();
	static void VariationDebugAllExtrasChangeCB();

	static void UpdateWindowTintSlider();

	static void CarLiveryChangeCB();
	static void CarLivery2ChangeCB();

	static void DebugUpdate();

	static int CbCompareNames(const char* cLeft, const char* cRight);
	static void AddLayoutFilterWidget();
	static void AddDLCFilterWidget();
	static RegdVeh	ms_pLastCreatedVehicle;
	static RegdVeh	ms_pLastLastCreatedVehicle;
	static bool ms_bFireCarForcePos;
	static bool ms_bDisplayVehicleNames;
	static bool ms_bDisplayVehicleLayout;
	static bool ms_bDisplayCreateLocation;
	static bool ms_bRegenerateFireVehicle;
    static bool ms_bDisplayTyreWearRate;
	static bool ms_bDisplaySteerAngle;
	static s32	currVehicleNameSelection;
	static s32	currVehicleTrailerNameSelection;
	static s32	currVehicleTypeSelection;
	static s32	currVehicleLayoutSelection;
	static s32  currVehicleDLCSelection;
	static s32  currVehicleSwankSelection;
	static s32  currVehicleClassSelection;
	static atArray<const char*> vehicleNames;
	static atArray<const char*> emptyNames;
	static s32 numDLCNames;
	static u32 carModelId;
	static u32 trailerModelId;
	static s32 ms_iSeatToEnter;
    static s32 ms_iDoorToOpen;
    static s32 ms_iTyreNumber;
	static float ms_fSetEngineNewHealth;

	static bkCombo* pLayoutCombo;
	static bkCombo* pDLCCombo;

	static s32 ms_variationDebugColor;
	static s32 ms_variationDebugWindowTint;
	static bool ms_variationAllExtras;
	static bool ms_variationExtras[VEH_LAST_EXTRA - VEH_EXTRA_1 + 1];
	static bool ms_variationDebugDraw;
	static bkSlider* ms_variationColorSlider;
	static bkSlider* ms_variationWindowTintSlider;

	static s32 ms_variationMod[ VMT_MAX ];

	//static s32 ms_variationSpoilerMod;
	//static s32 ms_variationBumperFMod;
	//static s32 ms_variationBumperRMod;
	//static s32 ms_variationSkirtMod;
	//static s32 ms_variationExhaustMod;
	//static s32 ms_variationChassisMod;
	//static s32 ms_variationGrillMod;
	//static s32 ms_variationBonnetMod;
	//static s32 ms_variationWingLMod;
	//static s32 ms_variationWingRMod;
	//static s32 ms_variationRoofMod;
	//static s32 ms_variationEngineMod;
	//static s32 ms_variationBrakesMod;
	//static s32 ms_variationGearboxMod;
	//static s32 ms_variationHornMod;
 //   static s32 ms_variationSuspensionMod;
	//static s32 ms_variationArmourMod;
	//static s32 ms_variationWheelMod;
	//static s32 ms_variationRearWheelMod;
	//static s32 ms_variationLiveryMod;

	static bkSlider* ms_variationSliders[ VMT_MAX ];

	//static bkSlider* ms_variationSpoilerSlider;
	//static bkSlider* ms_variationBumperFSlider;
	//static bkSlider* ms_variationBumperRSlider;
	//static bkSlider* ms_variationSkirtSlider;
	//static bkSlider* ms_variationExhaustSlider;
	//static bkSlider* ms_variationChassisSlider;
	//static bkSlider* ms_variationGrillSlider;
	//static bkSlider* ms_variationBonnetSlider;
	//static bkSlider* ms_variationWingLSlider;
	//static bkSlider* ms_variationWingRSlider;
	//static bkSlider* ms_variationRoofSlider;
	//static bkSlider* ms_variationEngineSlider;
	//static bkSlider* ms_variationBrakesSlider;
	//static bkSlider* ms_variationGearboxSlider;
	//static bkSlider* ms_variationHornSlider;
 //   static bkSlider* ms_variationSuspensionSlider;
	//static bkSlider* ms_variationArmourSlider;
	//static bkSlider* ms_variationWheelSlider;
	//static bkSlider* ms_variationRearWheelSlider;
	//static bkSlider* ms_variationLiveryModSlider;

	static bkSlider* ms_liverySlider;
	static bkSlider* ms_livery2Slider;
	static s32 ms_currentLivery;
	static s32 ms_currentLivery2;
	static float ms_vehicleLODScaling;
	static float ms_vehicleHiStreamScaling;
	static bool ms_bShowVehicleLODLevelData;
	static bool ms_bShowVehicleLODLevel;
	static bool ms_variationRenderMods;
	static bool ms_variationRandomMods;

	static bool ms_bPlayAnimPhysical;
	static bool ms_bSpawnTrailers;
	static bool ms_bForceHd;
	static float ms_fLodMult;
	static s8 ms_clampedRenderLod;
	
	static bool ms_bflyingAce;

	static bool	ms_reuseDestroyedVehsDebugOutput;
	static bool ms_rearDoorsCanOpen;

	// cache stats
	static u32 ms_numCachedCarsCreated;
	static u32 ms_numNonCachedCarsCreated;
	static bool ms_resetCacheStats;

	static bool ms_bLogCreatedVehicles;
	static bool ms_bLogDestroyedVehicles;
	static u32 ms_iDestroyedVehicleCount;
	static bool ms_bForceDials;

	static void LogDestroyedVehicle(CVehicle * pVehicle, const char * pCallerDesc, s32 iTextOffsetY, Color32 iTextCol);
#endif
};

#if __BANK
class CBoardingPointEditor
{
public:
	CBoardingPointEditor();
	~CBoardingPointEditor();

	static void Init();
	static void Shutdown();

	static void CreateWidgets(bkBank * bank);

	static void Process();

	static void CreatePrevBoatCB();
	static void CreateNextBoatCB();
	static void CreateNextBoat(int iDir);

	static void AddBoardingPointCB();
	static void SelectNextBoardingPointCB();
	static void SelectPrevBoardingPointCB();
	static void DeleteCurrentBoardingPointCB();
	static void MoveCurrentBoardingPointCB();
	static void RotateClockwiseCurrentBoardingPointCB();
	static void RotateAntiClockwiseCurrentBoardingPointCB();
	static void MoveUp();
	static void MoveDown();
	static void MoveForward();
	static void MoveBackward();
	static void MoveLeft();
	static void MoveRight();

	static void SaveBoardingPointsCB();

	static bool ms_bPlacementToolEnabled;
	static int ms_iSelectedBoardingPoint;
	static bool ms_bRespositioningBoardingPoint;
};
#endif // __BANK


#endif // !INC_VEHICLEFACTORY_H_
