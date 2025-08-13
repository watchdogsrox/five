#ifndef INC_VEHICLE_ENTRY_POINT_INFO_H
#define INC_VEHICLE_ENTRY_POINT_INFO_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/string.h"
#include "fwanimation/animdefines.h"
#include "fwtl/pool.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "vector/vector3.h"

// Game headers
#include "ai/ambient/ConditionalAnims.h"
#include "Renderer/HierarchyIds.h"
#include "Task/Scenario/info/ScenarioInfoConditions.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/vehicle_channel.h"

// Forward declarations
class CVehicleSeatInfo;

namespace rage
{
	class crClip;
}

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Additional points on the vehicle used during entry/exit
//-------------------------------------------------------------------------

class CExtraVehiclePoint
{
public:

	enum eLocationType
	{
		BONE,
		SEAT_RELATIVE,
		DRIVER_SEAT_RELATIVE,
		WHEEL_GROUND_RELATIVE,
		ENTITY_RELATIVE
	};

	enum ePointType
	{
		GET_IN,
		GET_IN_2,
		GET_IN_3,
		GET_IN_4,
		GET_OUT,
		VAULT_HAND_HOLD,
		UPSIDE_DOWN_EXIT,
		PICK_UP_POINT,
		PULL_UP_POINT,
		QUICK_GET_ON_POINT,
		EXIT_TEST_POINT,
		CLIMB_UP_FIXUP_POINT,
		ON_BOARD_JACK,
		POINT_TYPE_MAX,
	};

	static const u32 MAX_GET_IN_POINTS = 4;

	eLocationType GetLocationType() const { return m_LocationType; }
	ePointType GetPointType() const { return m_PointType; }
	Vector3 GetPosition() const { return m_Position; }
	float GetHeading() const { return m_Heading; }
	float GetPitch() const { return m_Pitch; }
	const char* GetName() const { return m_BoneName.c_str(); }

private:

	eLocationType		m_LocationType;
	ePointType			m_PointType;
	Vector3				m_Position;
	float				m_Heading;
	float				m_Pitch;
	atString			m_BoneName;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

class CVehicleExtraPointsInfo
{
public:

	// PURPOSE: Used by the PARSER.  Gets a name from a VehicleExtraPointsInfo pointer
	static const char* GetNameFromInfo(const CVehicleExtraPointsInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a VehicleExtraPointsInfo pointer from a name
	static const CVehicleExtraPointsInfo* GetInfoFromName(const char* name);

	atHashWithStringNotFinal GetName() const { return m_Name; }

	const CExtraVehiclePoint* GetExtraVehiclePointOfType(CExtraVehiclePoint::ePointType pointType) const;

private:

	atHashWithStringNotFinal m_Name;

	atArray<CExtraVehiclePoint> m_ExtraVehiclePoints;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Class which contains entry point info
//-------------------------------------------------------------------------
class CVehicleEntryPointInfo
{
public:

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleEntryPointInfo pointer
	static const char* GetNameFromInfo(const CVehicleEntryPointInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleEntryPointInfo pointer from a name
	static const CVehicleEntryPointInfo* GetInfoFromName(const char* name);


	enum eWindowId
	{
		FRONT_LEFT = 0,
		FRONT_RIGHT,
		REAR_LEFT,
		REAR_RIGHT,
		FRONT_WINDSCREEN,
		REAR_WINDSCREEN,
		MID_LEFT,
		MID_RIGHT,
		INVALID
	};

	enum eSide
	{
		SIDE_LEFT,
		SIDE_RIGHT,
		SIDE_REAR,
	};

	enum eEntryPointFlags
	{
		IsPassengerOnlyEntry							= BIT0,
		IgnoreSmashWindowCheck							= BIT1,
		TryLockedDoorOnGround							= BIT2,
		MPWarpInOut										= BIT3,
		SPEntryAllowedAlso								= BIT4,
		WarpOutInPlace									= BIT5,
		BlockJackReactionUntilJackerIsReady				= BIT6,
		UseFirstMulipleAccessSeatForDirectEntryPoint	= BIT7,
		IsPlaneHatchEntry								= BIT8,
		ClimbUpOnly										= BIT9,
		NotUsableOutsideVehicle							= BIT10,
		AutoCloseThisDoor								= BIT11,
		HasClimbDownToWater								= BIT12,
		ForceSkyDiveExitInAir							= BIT13,
	};

public:

	// Get the name of this entry point info
	atHashWithStringNotFinal GetName() const { return m_Name; }

	const char* GetDoorBoneName() const { return m_DoorBoneName.c_str(); }
	const char* GetSecondDoorBoneName() const { return m_SecondDoorBoneName.c_str(); }
	const char* GetDoorHandleBoneName() const { return m_DoorHandleBoneName.c_str(); }
	eHierarchyId GetWindowId() const;
	eSide GetVehicleSide() const { return m_VehicleSide; }
	const s32 GetNumAccessableSeats() const { return m_AccessableSeats.GetCount(); }
	const CVehicleSeatInfo* GetSeat(s32 iIndex) const;
	const CVehicleExtraPointsInfo* GetVehicleExtraPointsInfo() const { return m_VehicleExtraPointsInfo; }
	bool GetIsPassengerOnlyEntry() const { return m_Flags.IsFlagSet(IsPassengerOnlyEntry); }
	bool GetIgnoreSmashWindowCheck() const { return m_Flags.IsFlagSet(IgnoreSmashWindowCheck); }
	bool GetTryLockedDoorOnGround() const { return m_Flags.IsFlagSet(TryLockedDoorOnGround); }
	bool GetMPWarpInOut() const { return m_Flags.IsFlagSet(MPWarpInOut); }
	bool GetSPEntryAllowedAlso() const { return m_Flags.IsFlagSet(SPEntryAllowedAlso); }
	bool GetWarpOutInPlace() const { return m_Flags.IsFlagSet(WarpOutInPlace); }
	bool GetIsBlockingJackReactionUntilJackerIsReady() const { return m_Flags.IsFlagSet(BlockJackReactionUntilJackerIsReady); }
	bool GetUseFirstMulipleAccessSeatForDirectEntryPoint() const { return m_Flags.IsFlagSet(UseFirstMulipleAccessSeatForDirectEntryPoint); }
	bool GetIsPlaneHatchEntry() const { return m_Flags.IsFlagSet(IsPlaneHatchEntry); }
	bool GetClimbUpOnly() const { return m_Flags.IsFlagSet(ClimbUpOnly); }
	bool GetNotUsableOutsideVehicle() const { return m_Flags.IsFlagSet(NotUsableOutsideVehicle); }
	bool GetAutoCloseThisDoor() const { return m_Flags.IsFlagSet(AutoCloseThisDoor); }
	bool GetHasClimbDownToWater() const { return m_Flags.IsFlagSet(HasClimbDownToWater); }
	const s32 GetNumBlockReactionSides() const { return m_BlockJackReactionSides.GetCount(); }
	const eSide GetBlockReactionSide(s32 iIndex) const;
	const CExtraVehiclePoint::ePointType GetBreakoutTestPoint() const { return m_BreakoutTestPoint; }
	bool GetForceSkyDiveExitInAir() const { return m_Flags.IsFlagSet(ForceSkyDiveExitInAir); }

private:

	atHashWithStringNotFinal			m_Name;
	atString							m_DoorBoneName;
	atString							m_SecondDoorBoneName;
	atString							m_DoorHandleBoneName;
	eHierarchyId						m_WindowId;
	eSide								m_VehicleSide;
	atArray<CVehicleSeatInfo*>			m_AccessableSeats;
	CVehicleExtraPointsInfo*			m_VehicleExtraPointsInfo;
	fwFlags32							m_Flags;
	atArray<eSide>						m_BlockJackReactionSides;
	CExtraVehiclePoint::ePointType		m_BreakoutTestPoint;

	PAR_SIMPLE_PARSABLE
};


////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CEntryAnimVariations
////////////////////////////////////////////////////////////////////////////////

class CEntryAnimVariations
{
public:

	typedef enum
	{
		AT_ENTRY_VARIATION,
		AT_JACKING_VARIATION,
		AT_EXIT_VARIATION
	} eAnimType;

	typedef atArray<CConditionalClipSet*> ConditionalClipSetArray;

	const ConditionalClipSetArray * GetClipSetArray( CEntryAnimVariations::eAnimType animType ) const 
	{
		switch (animType)
		{
		case CEntryAnimVariations::AT_ENTRY_VARIATION:
			return &m_EntryAnims;
		case CEntryAnimVariations::AT_JACKING_VARIATION:
			return &m_JackingAnims;
		case CEntryAnimVariations::AT_EXIT_VARIATION:
			return &m_ExitAnims;
		default:
			aiAssert(false);
			break;
		}

		return NULL;
	}

	// PURPOSE: Used by the PARSER.  Gets a name from a CEntryAnimVariations pointer
	static const char* GetNameFromInfo(const CEntryAnimVariations* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CEntryAnimVariations pointer from a name
	static const CEntryAnimVariations* GetInfoFromName(const char* name);

	u32 GetHash() const { return m_Name.GetHash(); }
	atHashWithStringNotFinal GetName() const { return m_Name; }

	bool CheckClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData) const;
	bool CheckConditions( const CScenarioCondition::sScenarioConditionData& conditionData, s32 * firstFailureCondition ) const;
	int GetImmediateConditions(u32 clipSetHash, const CScenarioCondition** pArrayOut, int maxNumber/*, eAnimType animType*/) const;

	const CConditionalClipSet * ChooseClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const;

#if !__FINAL
	bool HasAnimationData() const;
#endif

private:

	atHashWithStringNotFinal	m_Name;
	fwFlags32					m_Flags;

	// Conditions that we use these anims
	ConditionsArray				m_Conditions;

	// Entry variations starting from the end of the align pose
	ConditionalClipSetArray		m_EntryAnims;

	// Jacking variations starting from the end of the align pose
	ConditionalClipSetArray		m_JackingAnims;

	// Exit variations starting from the seated idle pose
	ConditionalClipSetArray		m_ExitAnims;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Class which contains entry point anim info
//-------------------------------------------------------------------------
class CVehicleEntryPointAnimInfo
{
public:

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleEntryPointAnimInfo pointer
	static const char* GetNameFromInfo(const CVehicleEntryPointAnimInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleEntryPointAnimInfo pointer from a name
	static const CVehicleEntryPointAnimInfo* GetInfoFromName(const char* name);

	enum eEnterVehicleMoveNetwork
	{
		ENTER_VEHICLE_STD = 0,
		ENTER_VEHICLE_BIKE,
		MOUNT_ANIMAL,
		NUM_ENTERVEHICLEMOVENETWORKS,
	};

	// Entry Point Flags
	enum eEntryPointFlags
	{
		UseVehicleRelativeEntryPosition	= BIT0,
		HasClimbUp						= BIT1,
		WarpOut							= BIT2,
		IgnoreMoverFixUp				= BIT3,
		HasVaultUp						= BIT4,
		DisableEntryIfBikeOnSide		= BIT5,
		HasGetInFromWater				= BIT6,
		ExitAnimHasLongDrop				= BIT7,
		JackIncludesGetIn				= BIT8,
		ForcedEntryIncludesGetIn		= BIT9,
		UsesNoDoorTransitionForEnter	= BIT10,
		UsesNoDoorTransitionForExit 	= BIT11,
		HasGetOutToVehicle				= BIT12,
		UseNewPlaneSystem				= BIT13,
		DontCloseDoorInside				= BIT14,
		DisableJacking					= BIT15,
		UseOpenDoorBlendAnims			= BIT16,
		PreventJackInterrupt			= BIT17,
		DontCloseDoorOutside			= BIT18,
		HasOnVehicleEntry				= BIT19,
		HasClimbDown					= BIT20,
		FixUpMoverToEntryPointOnExit	= BIT21,
		CannotBeUsedByCuffedPed			= BIT22,
		HasClimbUpFromWater		    	= BIT23,
		NavigateToSeatFromClimbup		= BIT24,
		UseSeatClipSetAnims				= BIT25,
		NavigateToWarpEntryPoint		= BIT26,
		UseGetUpAfterJack				= BIT27,
		JackVariationsIncludeGetIn		= BIT28,
		DeadJackIncludesGetIn			= BIT29,
		HasCombatEntry					= BIT30,
		UseStandOnGetIn					= BIT31
	};

public:	

	// Get the name of this entry point anim info
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// Temp Tez: hard coded anim dictionary index/anim hash
	u32 GetEnterAnimHash() const;

	// Getters for data
	fwMvClipSetId			 GetCommonClipSetId() const
	{ 
		if (vehicleVerifyf(m_CommonClipSetMap, "Couldn't find common clipset map for entry anim info %s", m_Name.GetCStr()))
		{
			return m_CommonClipSetMap->GetMaps()[0].m_ClipSetId; 
		}
		return CLIP_SET_ID_INVALID;
	}

	fwMvClipSetId			GetCommonOnVehicleClipSet() const
	{
		if (vehicleVerifyf(m_CommonClipSetMap, "Couldn't find common clipset map for entry anim info %s", m_Name.GetCStr()))
		{
			if (m_CommonClipSetMap->GetMaps().GetCount() > 1)
			{
				return m_CommonClipSetMap->GetMaps()[1].m_ClipSetId; 
			}
		}
		return CLIP_SET_ID_INVALID;
	}

	fwMvClipSetId			GetCommonOBoardJackVehicleClipSet() const
	{
		if (vehicleVerifyf(m_CommonClipSetMap, "Couldn't find common clipset map for entry anim info %s", m_Name.GetCStr()))
		{
			if (m_CommonClipSetMap->GetMaps().GetCount() > 2)
			{
				return m_CommonClipSetMap->GetMaps()[2].m_ClipSetId; 
			}
		}
		return CLIP_SET_ID_INVALID;
	}

	fwMvClipSetId 			 GetEntryPointClipSetId(s32 iIndex = 0) const
	{ 
		if (vehicleVerifyf(m_EntryClipSetMap, "Couldn't find entry clipset map for entry anim info %s", m_Name.GetCStr()))
		{
			if (m_EntryClipSetMap->GetMaps().GetCount() > iIndex)
			{
				return m_EntryClipSetMap->GetMaps()[iIndex].m_ClipSetId; 
			}
			return m_EntryClipSetMap->GetMaps()[0].m_ClipSetId; 
		}
		return CLIP_SET_ID_INVALID;
	}
	fwMvClipSetId 			 GetExitPointClipSetId(s32 iIndex = 0) const
	{ 

		if (vehicleVerifyf(m_ExitClipSetMap, "Couldn't find exit clipset map for entry anim info %s", m_Name.GetCStr()))
		{
			if (m_ExitClipSetMap->GetMaps().GetCount() > iIndex)
			{
				return m_ExitClipSetMap->GetMaps()[iIndex].m_ClipSetId; 
			}
			return m_ExitClipSetMap->GetMaps()[0].m_ClipSetId; 
		}
		return CLIP_SET_ID_INVALID;
	}

	fwMvClipSetId GetCustomPlayerClipSet(s32 iPlayerIndex, CClipSetMap::eConditionFlags clipSetType, fwFlags32* pInformationFlags = NULL) const
	{
		vehicleAssertf(iPlayerIndex >= 0 && iPlayerIndex <= 2, "Invalid player index (%i) passed in", iPlayerIndex);

		if (vehicleVerifyf(m_EntryClipSetMap, "Couldn't find entry clipset map for entry anim info %s", m_Name.GetCStr()))
		{
			s32 iNumClipsetsFound = 0;
			s32 iNumMaps = m_EntryClipSetMap->GetMaps().GetCount();
			for (s32 i=0; i<iNumMaps; ++i)
			{
				if (m_EntryClipSetMap->GetMaps()[i].m_ConditionFlags & clipSetType)
				{
					if (iNumClipsetsFound == iPlayerIndex)
					{
						if(pInformationFlags)
						{
							*pInformationFlags = m_EntryClipSetMap->GetMaps()[i].m_InformationFlags;
						}

						return m_EntryClipSetMap->GetMaps()[i].m_ClipSetId;
					}
					++iNumClipsetsFound;
				}
			}
		}
		return CLIP_SET_ID_INVALID;
	}

	fwMvClipSetId GetCustomPlayerBreakInClipSet(s32 iPlayerIndex, fwFlags32* pInformationFlags = NULL) const
	{
		return GetCustomPlayerClipSet(iPlayerIndex, CClipSetMap::CF_BreakInAnims, pInformationFlags);
	}
	fwMvClipSetId GetCustomPlayerJackingClipSet(s32 iPlayerIndex, fwFlags32* pInformationFlags = NULL) const
	{
		return GetCustomPlayerClipSet(iPlayerIndex, CClipSetMap::CF_JackingAnims, pInformationFlags);
	}

	fwMvClipId 			 	 GetAlternateTryLockedDoorClipId() const { return m_AlternateTryLockedDoorClipId; }
	fwMvClipId 			 	 GetAlternateForcedEntryClipId() const { return m_AlternateForcedEntryClipId; }
	fwMvClipId 			 	 GetAlternateJackFromOutSideClipId() const { return m_AlternateJackFromOutSideClipId; }
	fwMvClipId 			 	 GetAlternateBeJackedFromOutSideClipId() const { return m_AlternateBeJackedFromOutSideClipId; }
	fwMvClipSetId 			 GetAlternateEntryPointClipSet() const { return m_AlternateEntryPointClipSetId; }
	eEnterVehicleMoveNetwork GetEnterVehicleMoveNetwork() const { return m_EnterVehicleMoveNetwork; }
	const Vector3&			 GetEntryTranslation() const { return m_EntryTranslation; }
	const Vector2&			 GetOpenDoorTranslation() const { return m_OpenDoorTranslation; }
	float					 GetOpenDoorEntryHeadingChange() const { return m_OpenDoorHeadingChange; }
	float					 GetEntryHeadingChange() const { return m_EntryHeadingChange; }
	float					 GetExtraZForMPPlaneWarp() const { return m_ExtraZForMPPlaneWarp; }
	void					 SetEntryTranslation(const Vector3& vEntryTranslation) { m_EntryTranslation = vEntryTranslation; }
	void					 SetEntryHeadingChange(float fEntryHeadingChange) { m_EntryHeadingChange = fEntryHeadingChange; }
	void					 SetExtraZForMPPlaneWarp(float fExtraZForMPPlaneWarp) { m_ExtraZForMPPlaneWarp = fExtraZForMPPlaneWarp; }
	bool					 GetUseVehicleRelativeEntryPosition() const { return m_EntryPointFlags.IsFlagSet(UseVehicleRelativeEntryPosition); }
	bool					 GetHasClimbUp() const { return m_EntryPointFlags.IsFlagSet(HasClimbUp); }
	bool					 GetWarpOut() const { return m_EntryPointFlags.IsFlagSet(WarpOut); }
	bool					 GetIgnoreMoverFixUp() const { return m_EntryPointFlags.IsFlagSet(IgnoreMoverFixUp); }
	bool					 GetHasVaultUp() const { return m_EntryPointFlags.IsFlagSet(HasVaultUp); }
	bool					 GetHasGetInFromWater() const { return m_EntryPointFlags.IsFlagSet(HasGetInFromWater); }
	bool					 GetHasClimbUpFromWater() const { return m_EntryPointFlags.IsFlagSet(HasClimbUpFromWater); }
	bool					 GetUseNavigateToSeatFromClimbUp() const { return m_EntryPointFlags.IsFlagSet(NavigateToSeatFromClimbup); }
	bool					 GetExitAnimHasLongDrop() const { return m_EntryPointFlags.IsFlagSet(ExitAnimHasLongDrop); }
	bool					 GetJackIncludesGetIn() const { return m_EntryPointFlags.IsFlagSet(JackIncludesGetIn); }
	bool					 GetIsJackDisabled() const { return m_EntryPointFlags.IsFlagSet(DisableJacking); }
	bool					 GetForcedEntryIncludesGetIn() const { return m_EntryPointFlags.IsFlagSet(ForcedEntryIncludesGetIn); }
	bool					 GetUsesNoDoorTransitionForEnter() const { return m_EntryPointFlags.IsFlagSet(UsesNoDoorTransitionForEnter); }
	bool					 GetUsesNoDoorTransitionForExit() const { return m_EntryPointFlags.IsFlagSet(UsesNoDoorTransitionForExit); }
	bool					 GetHasGetOutToVehicle() const { return m_EntryPointFlags.IsFlagSet(HasGetOutToVehicle); }
	bool					 GetUseNewPlaneSystem() const { return m_EntryPointFlags.IsFlagSet(UseNewPlaneSystem); }
	bool					 GetDontCloseDoorInside() const { return m_EntryPointFlags.IsFlagSet(DontCloseDoorInside); }
	bool					 GetUseOpenDoorBlendAnims() const { return m_EntryPointFlags.IsFlagSet(UseOpenDoorBlendAnims); }
	bool					 GetPreventJackInterrupt() const { return m_EntryPointFlags.IsFlagSet(PreventJackInterrupt); }
	bool					 GetDontCloseDoorOutside() const { return m_EntryPointFlags.IsFlagSet(DontCloseDoorOutside); }
	bool					 GetHasOnVehicleEntry() const { return m_EntryPointFlags.IsFlagSet(HasOnVehicleEntry); }
	bool					 GetHasClimbDown() const { return m_EntryPointFlags.IsFlagSet(HasClimbDown); }
	bool					 GetFixUpMoverToEntryPointOnExit() const { return m_EntryPointFlags.IsFlagSet(FixUpMoverToEntryPointOnExit); }
	bool					 GetCannotBeUsedByCuffedPed() const { return m_EntryPointFlags.IsFlagSet(CannotBeUsedByCuffedPed); }
	bool					 GetUseSeatClipSetAnims() const { return m_EntryPointFlags.IsFlagSet(UseSeatClipSetAnims); }
	bool					 GetNavigateToWarpEntryPoint() const { return m_EntryPointFlags.IsFlagSet(NavigateToWarpEntryPoint); }
	bool					 GetUseGetUpAfterJack() const { return m_EntryPointFlags.IsFlagSet(UseGetUpAfterJack); }
	bool					 GetJackVariationsIncludeGetIn() const { return m_EntryPointFlags.IsFlagSet(JackVariationsIncludeGetIn); }
	bool					 GetDeadJackIncludesGetIn() const { return m_EntryPointFlags.IsFlagSet(DeadJackIncludesGetIn); }
	bool					 GetHasCombatEntry() const { return m_EntryPointFlags.IsFlagSet(HasCombatEntry); }
	bool					 GetUseStandOnGetIn() const { return m_EntryPointFlags.IsFlagSet(UseStandOnGetIn); }
	CEntryAnimVariations*	 GetEntryAnimVariations() const { return m_EntryAnimVariations; }
	atHashString			 GetNMJumpFromVehicleTuningSet() const { return m_NMJumpFromVehicleTuningSet; }

private:

	atHashWithStringNotFinal		m_Name;
	CClipSetMap*					m_CommonClipSetMap;
	CClipSetMap*					m_EntryClipSetMap;
	CClipSetMap*					m_ExitClipSetMap;
	fwMvClipId						m_AlternateTryLockedDoorClipId;
	fwMvClipId						m_AlternateForcedEntryClipId;
	fwMvClipId						m_AlternateJackFromOutSideClipId;
	fwMvClipId						m_AlternateBeJackedFromOutSideClipId;
	fwMvClipSetId					m_AlternateEntryPointClipSetId;
	eEnterVehicleMoveNetwork		m_EnterVehicleMoveNetwork;
	Vector3							m_EntryTranslation;
	Vector2							m_OpenDoorTranslation;
	float							m_OpenDoorHeadingChange;
	float							m_EntryHeadingChange;
	float							m_ExtraZForMPPlaneWarp;
	fwFlags32						m_EntryPointFlags;
	CEntryAnimVariations*			m_EntryAnimVariations;
	atHashString					m_NMJumpFromVehicleTuningSet;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_VEHICLE_ENTRY_POINT_INFO_H
