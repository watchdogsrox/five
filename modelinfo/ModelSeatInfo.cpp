//
//
//    Filename: ModelSeatInfo.cpp
//     Creator: Bryan Musson
//
//
#include "ModelSeatInfo.h"

// Game headers

#include "ai/aichannel.h"
#include "ai/debug/system/AIDebugLogManager.h"
#include "game/ModelIndices.h"
#include "Peds/Ped.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "task/Motion/TaskMotionBase.h"
#include "task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/vehicle_channel.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	CEntryExitPoint
//////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------
// A point of entry into the vehicle
// 1 direct seat to access (right next to the door)
// 1 indirect seat (by shuffling through the direct seat)
//-------------------------------------------------------------------------
CEntryExitPoint::CEntryExitPoint()
{
	m_uEntryPointHash = 0;
	m_iDoorBoneIndex = -1;
	m_iDoorHandleBoneIndex = -1;
}



//-------------------------------------------------------------------------
// SetupSingleSeatAccess this entry exit point for the vehicle
//-------------------------------------------------------------------------
void  CEntryExitPoint::SetSeatAccess(s32 iDoorBoneIndex, s32 iDoorHandleBoneIndex)
{
	m_iDoorBoneIndex = (s16) iDoorBoneIndex;
	m_iDoorHandleBoneIndex = (s16) iDoorHandleBoneIndex;
}

//-------------------------------------------------------------------------
// Returns how this seat can be accessed through this point, directly or indirectly
//-------------------------------------------------------------------------
SeatAccess CEntryExitPoint::CanSeatBeReached(s32 iSeat) const
{
	return m_SeatAccessor.CanSeatBeReached(iSeat);
}


//-------------------------------------------------------------------------
// Returns the seat that can be accessed through this point
//-------------------------------------------------------------------------
s32 CEntryExitPoint::GetSeat(SeatAccess iSeatAccess, s32 iSeatIndex) const
{
	modelinfoAssert( iSeatAccess > SA_invalid && iSeatAccess < SA_MaxSeats );
	return m_SeatAccessor.GetSeat(iSeatAccess, iSeatIndex);
}

//-------------------------------------------------------------------------
// Returns the entry point position
//-------------------------------------------------------------------------
bool CEntryExitPoint::GetEntryPointPosition(const CEntity* pEntity, const CPed *pPed, Vector3& vPos, bool bRoughPosition, bool bAdjustForWater) const 
{
	if (!pEntity) return false;
	const CModelSeatInfo* pModelSeatinfo = NULL;	
	if (pEntity->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else if (pEntity->GetIsTypeVehicle()) 
	{		
		pModelSeatinfo  = static_cast<const CVehicle*>(pEntity)->GetVehicleModelInfo()->GetModelSeatInfo();		
	} 
	else return false;

	s32 nFlags = 0;
	if(bRoughPosition)
	{
		nFlags |= CModelSeatInfo::EF_RoughPosition;
	}

	if(bAdjustForWater)
	{
		nFlags |= CModelSeatInfo::EF_AdjustForWater;
	}

	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	CModelSeatInfo::CalculateEntrySituation(pEntity, pPed, vPos, qEntryOrientation,  pModelSeatinfo->GetEntryExitPointIndex(this), nFlags);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//	CSeatAccessor
//////////////////////////////////////////////////////////////////////////
CSeatAccessor::CSeatAccessor()
: m_eSeatAccessType(eSeatAccessTypeInvalid)
{
}

CSeatAccessor::~CSeatAccessor()
{

}

SeatAccess CSeatAccessor::CanSeatBeReached(s32 iSeat) const
{
	if(m_eSeatAccessType == eSeatAccessTypeSingle)
	{
		for( s32 iAccess = SSA_DirectAccessSeat; iAccess < SSA_MaxSingleAccessSeats; iAccess++ )
		{
			if( m_aAccessibleSeats[iAccess] == iSeat )
			{
				if (iAccess == SSA_DirectAccessSeat) return SA_directAccessSeat;
				else if (iAccess == SSA_IndirectAccessSeat) return SA_indirectAccessSeat;
				else if (iAccess == SSA_IndirectAccessSeat2) return SA_indirectAccessSeat2;
				else aiAssertf(0, "Unknown access type");
			}
		}
	}
	else
	{
		for(int i = 0; i < m_aAccessibleSeats.GetCount(); ++i)
		{
			if(m_aAccessibleSeats[i] == iSeat)
			{
				return SA_directAccessSeat;
			}
		}
	}
	return SA_invalid;
}

s32 CSeatAccessor::GetSeat(SeatAccess eSeatAccess, s32 iSeatIndex) const
{
	if(m_eSeatAccessType == eSeatAccessTypeSingle)
	{
		if (eSeatAccess == SA_directAccessSeat)
		{
			return m_aAccessibleSeats[SSA_DirectAccessSeat];
		}
		else if (eSeatAccess == SA_indirectAccessSeat)
		{
			return m_aAccessibleSeats[SSA_IndirectAccessSeat];
		}
		else if (eSeatAccess == SA_indirectAccessSeat2)
		{
			return m_aAccessibleSeats[SSA_IndirectAccessSeat2];
		}
	}
	else if(m_eSeatAccessType != eSeatAccessTypeInvalid)
	{
		s32 seat = m_aAccessibleSeats[0];
		//! Find specific seat.
		if(iSeatIndex != -1)
		{
			for(int i = 0; i < m_aAccessibleSeats.GetCount(); ++i)
			{
				if(m_aAccessibleSeats[i] == iSeatIndex)
					seat = m_aAccessibleSeats[i];
			}
		}

		return seat;
	}

	aiAssertf(0, "Unknown Seat Access Type");
	return -1;
}

// Returns the seat that can be accessed through this point
s32	CSeatAccessor::GetSeatByIndex(s32 iSeatIndex) const 
{ 
	vehicleAssertf(iSeatIndex > -1 && iSeatIndex < m_aAccessibleSeats.GetCount(), "Invalid Seat Index"); 
	return m_aAccessibleSeats[iSeatIndex]; 
}

void CSeatAccessor::AddSingleSeatAccess(s32 directSeat, s32 indirectSeat, s32 indirectSeat2)
{
	m_aAccessibleSeats.Push(directSeat);
	m_aAccessibleSeats.Push(indirectSeat);
	m_aAccessibleSeats.Push(indirectSeat2);
}


void  CSeatAccessor::AddMultipleSeatAccess(s32 iSeat)
{
	m_aAccessibleSeats.Push(iSeat);
}


//////////////////////////////////////////////////////////////////////////
//	CModelSeatInfo
//////////////////////////////////////////////////////////////////////////

void	CModelSeatInfo::Reset() 
{	
	m_iNumSeats = 0; m_iDriverSeat = 0; m_iNumEntryExitPoints = 0;
	for(int i = 0; i < MAX_ENTRY_EXIT_POINTS; i++)
	{
		m_iLayoutEntryPointIndicies[i] = -1;
	}

	for(int i = 0; i < MAX_VEHICLE_SEATS; i++)
	{		
		m_iLayoutSeatIndicies[i] = -1;
		m_iSeatBoneIndicies[i] = -1;
	}
}

void	CModelSeatInfo::InitFromLayoutInfo(const CVehicleLayoutInfo* pLayoutInfo, const crSkeletonData* pSkelData, s32 iNumSeatsOverride) {
	// Query metadata to set up seats
	// Want to keep track of which seat info relates to which index
	// So we can map the doors to the seat indices

	// Important: The seat indices in the vehicle model info could not match the layout info
	// if for example the 3rd seat of the layout is missing from the vehicle model
	// This is so that we can safely iterate from 0 to num seats
	// Otherwise we'd have to iterate to max seats each time

	m_iNumEntryExitPoints = 0;	
	m_iNumSeats = 0;

	for( s32 i = 0; i < MAX_VEHICLE_SEATS; i++ )
	{
		m_iLayoutSeatIndicies[i] = -1;
		m_iLayoutEntryPointIndicies[i] = -1;
	}	

	if (!pSkelData) return;

	m_pLayoutinfo = pLayoutInfo;

	vehicleAssertf(m_pLayoutinfo->GetNumSeats() <= MAX_VEHICLE_SEATS,"This layout info has too many entry seats");
	vehicleAssertf(m_pLayoutinfo->GetNumEntryPoints() <= MAX_ENTRY_EXIT_POINTS,"This layout info has too many entry points");	

	for(s8 i = 0; i  <m_pLayoutinfo->GetNumSeats() && i < MAX_VEHICLE_SEATS; i++)
	{
		const CVehicleSeatInfo* pSeatInfo = m_pLayoutinfo->GetSeatInfo(i);
		vehicleAssertf(pSeatInfo,"Seat %i in layout %s is missing metadata", i, m_pLayoutinfo->GetName().GetCStr());
		if(!pSeatInfo)
		{
			continue;
		}

		const crBoneData* pBoneData = pSkelData->FindBoneData(pSeatInfo->GetBoneName());
		m_iSeatBoneIndicies[m_iNumSeats] = pBoneData ? (s16)pBoneData->GetIndex() : -1;

		// Does bone exist?
		// Deal with no bone silently so we can re use layouts
		if(m_iSeatBoneIndicies[m_iNumSeats] != -1)
		{
			// This is a valid seat
			m_iLayoutSeatIndicies[m_iNumSeats] = i;
			m_iNumSeats++;
		}

	}

	// If the number of valid seats doesn't match number of seats in layout, need to make sure we have an override value set up for script
	if (m_iNumSeats != m_pLayoutinfo->GetNumSeats() && m_iNumSeats != iNumSeatsOverride)
	{
		vehicleAssertf(0, "Vehicle using %s requires a valid numSeatsOverride entry (layout seats %i, actual seats %i)", m_pLayoutinfo->GetName().TryGetCStr(), m_pLayoutinfo->GetNumSeats(), m_iNumSeats);
	}

	// Set up the entry points
	for(s8 i = 0; i < m_pLayoutinfo->GetNumEntryPoints() && i < MAX_ENTRY_EXIT_POINTS; i++)
	{
		const CVehicleEntryPointInfo* pEntryInfo = m_pLayoutinfo->GetEntryPointInfo(i);
		vehicleAssertf(pEntryInfo, "Seat %i is missing metadata", i);

		// Search for the seat index
		int iTargetSeatIndex = -1;			// The seat this door points to
		int iShuffleSeatIndex = -1;
		int iShuffleSeatIndex2 = -1;

		m_aEntryExitPoints[m_iNumEntryExitPoints].GetSeatAccessor().ClearAccessableSeats();

		// If we have more than 1 accessable seat, then the entry point can directly access more than one seat
		if (pEntryInfo->GetNumAccessableSeats() == 1)
		{
			FindTargetSeatInfoFromLayout(m_pLayoutinfo, pEntryInfo, 0, iTargetSeatIndex, iShuffleSeatIndex, iShuffleSeatIndex2);

			// If entry point's target seat is null then don't bother making the entry point
			if(iTargetSeatIndex != -1)
			{
				// Look up the bone index of the entry point. Its OK to be NULL (e.g. a bike)
				const crBoneData* pDoorBoneData = pEntryInfo->GetDoorBoneName() ? pSkelData->FindBoneData(pEntryInfo->GetDoorBoneName()) : NULL;
				int iDoorBoneIndex = pDoorBoneData ? (s16)pDoorBoneData->GetIndex() : -1;
				const crBoneData* pDoorHandleBoneData = pEntryInfo->GetDoorHandleBoneName() ? pSkelData->FindBoneData(pEntryInfo->GetDoorHandleBoneName()) : NULL;
				int iDoorHandleBoneIndex = pDoorHandleBoneData ? (s16)pDoorHandleBoneData->GetIndex() : -1;
				m_aEntryExitPoints[m_iNumEntryExitPoints].SetSeatAccess(iDoorBoneIndex,iDoorHandleBoneIndex);
				m_aEntryExitPoints[m_iNumEntryExitPoints].GetSeatAccessor().SetSeatAccessType(CSeatAccessor::eSeatAccessTypeSingle);
				m_aEntryExitPoints[m_iNumEntryExitPoints].GetSeatAccessor().AddSingleSeatAccess(iTargetSeatIndex,iShuffleSeatIndex,iShuffleSeatIndex2);
				m_iLayoutEntryPointIndicies[m_iNumEntryExitPoints] = i;
				m_iNumEntryExitPoints++;
			}
#if __DEV
			else
			{
				vehicleWarningf("Couldn't find target seat corresponding to entry point %i for vehicle", i);
			}
#endif // __DEV
		}
		else
		{
			bool bInitialised = false;

			for (s32 j=0; j<pEntryInfo->GetNumAccessableSeats(); j++)
			{
				for(int iSeatIndex = 0; iSeatIndex < m_iNumSeats; iSeatIndex++)
				{
					const CVehicleSeatInfo* pSeatInfo = m_pLayoutinfo->GetSeatInfo(iSeatIndex);
					if(pSeatInfo == pEntryInfo->GetSeat(j))
					{
						iTargetSeatIndex = iSeatIndex;
					}
				}

				// If entry point's target seat is null then don't bother making the entry point
				if(iTargetSeatIndex != -1)
				{
					if (!bInitialised)
					{
						// Look up the bone index of the entry point. Its OK to be NULL (e.g. a bike)
						const crBoneData* pDoorBoneData = pEntryInfo->GetDoorBoneName() ? pSkelData->FindBoneData(pEntryInfo->GetDoorBoneName()) : NULL;
						int iDoorBoneIndex = pDoorBoneData ? (s16)pDoorBoneData->GetIndex() : -1;
						const crBoneData* pDoorHandleBoneData = pEntryInfo->GetDoorHandleBoneName() ? pSkelData->FindBoneData(pEntryInfo->GetDoorHandleBoneName()) : NULL;
						int iDoorHandleBoneIndex = pDoorHandleBoneData ? (s16)pDoorHandleBoneData->GetIndex() : -1;
						m_aEntryExitPoints[m_iNumEntryExitPoints].SetSeatAccess(iDoorBoneIndex,iDoorHandleBoneIndex);
						m_aEntryExitPoints[m_iNumEntryExitPoints].GetSeatAccessor().SetSeatAccessType(CSeatAccessor::eSeatAccessTypeMultiple);
						bInitialised = true;
					}

					taskFatalAssertf(m_aEntryExitPoints[m_iNumEntryExitPoints].GetSeatAccessor().GetNumSeatsAccessible() < CSeatAccessor::MSA_MaxMultipleAccessSeats, "Trying to add too many multiple access seats, current max %i, check layout %s", CSeatAccessor::MSA_MaxMultipleAccessSeats, m_pLayoutinfo->GetName().GetCStr());
					m_aEntryExitPoints[m_iNumEntryExitPoints].GetSeatAccessor().AddMultipleSeatAccess(iTargetSeatIndex);
				}
			}

			if (bInitialised)
			{
				m_iLayoutEntryPointIndicies[m_iNumEntryExitPoints] = i;
				m_iNumEntryExitPoints++;
			}
		}
	}
}

void CModelSeatInfo::FindTargetSeatInfoFromLayout(const CVehicleLayoutInfo* pLayoutInfo, const CVehicleEntryPointInfo* pEntryInfo, s32 iSeatAccessIndex, s32& iTargetSeatIndex, s32& iShuffleSeatIndex, s32& iShuffleSeatIndex2)
{
	const CVehicleSeatInfo* pSeatInfo = pEntryInfo->GetSeat(iSeatAccessIndex);

	vehicleAssertf(pSeatInfo, "Entry point %d has invalid seat information",iSeatAccessIndex);

	for(int iSeatIndex = 0; iSeatIndex < m_iNumSeats; iSeatIndex++)
	{
		const CVehicleSeatInfo* pLayoutSeatInfo = pLayoutInfo->GetSeatInfo(iSeatIndex);
		if(pLayoutSeatInfo == pSeatInfo)
		{
			iTargetSeatIndex = iSeatIndex;
		}
		else if(pLayoutSeatInfo == pSeatInfo->GetShuffleLink())
		{
			iShuffleSeatIndex = iSeatIndex;
		}
		else if(pLayoutSeatInfo == pSeatInfo->GetShuffleLink2())
		{
			iShuffleSeatIndex2 = iSeatIndex;
		}

		if(iTargetSeatIndex != -1 && iShuffleSeatIndex != -1 && iShuffleSeatIndex2 != -1)
		{
			// Found all of our target seats
			break;
		}
	}
}

s32 CModelSeatInfo::GetEntryExitPointIndex(const CEntryExitPoint* pEntryExitPoint) const
{
	for (s32 i=0; i<m_iNumEntryExitPoints; i++)
	{
		if (&m_aEntryExitPoints[i] == pEntryExitPoint)
			return i;
	}

	modelinfoAssert(0);

	return -1;
}

s32 CModelSeatInfo::GetEntryPointIndexForSeat(s32 iSeat, const CEntity* pEntity, SeatAccess accessType, bool bCheckSide, bool bLeftSide) const
{
	for( s32 i = 0; i < m_iNumEntryExitPoints; i++ )
	{
		if (GetEntryExitPoint(i)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle)
		{
			if( GetEntryExitPoint(i)->GetSeat(accessType) == iSeat )
			{
				if (!bCheckSide)
				{
					return i;
				}
				
				if (pEntity && pEntity->GetIsTypeVehicle())
				{
					const CVehicleEntryPointInfo* pEntryInfo = static_cast<const CVehicle*>(pEntity)->GetEntryInfo(i);
					if (pEntryInfo && 
						((pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT && bLeftSide) ||
						(pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_RIGHT && !bLeftSide)))
					{
						return i;
					}
				}	
			}
		}
		else if (GetEntryExitPoint(i)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
		{
			const CVehicleEntryPointInfo* pEntryInfo = pEntity && pEntity->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pEntity)->GetEntryInfo(i) : NULL;

			if(pEntryInfo && pEntryInfo->GetUseFirstMulipleAccessSeatForDirectEntryPoint())
			{
				if(GetEntryExitPoint(i)->GetSeatAccessor().GetSeatByIndex(0) == iSeat)
				{
					return i;
				}
			}
			else
			{
				for (s32 j=0; j<GetEntryExitPoint(i)->GetSeatAccessor().GetNumSeatsAccessible(); j++)
				{
					if( GetEntryExitPoint(i)->GetSeatAccessor().GetSeatByIndex(j) == iSeat )
					{
						return i;
					}
				}
			}
		}
	}
	return -1;
}

s32	CModelSeatInfo::GetLayoutSeatIndex(int iSeatIndex) const 
{
	if(vehicleVerifyf(iSeatIndex > -1 && iSeatIndex < GetNumSeats(), "Invalid seat index: %i", iSeatIndex))
	{
		return m_iLayoutSeatIndicies[iSeatIndex];
	}

	return -1;
}	

s32	CModelSeatInfo::GetLayoutEntrypointIndex(int iEntryIndex) const
{
	if(vehicleVerifyf(iEntryIndex > -1 && iEntryIndex < GetNumberEntryExitPoints(), "Invalid entry point index: %i", iEntryIndex))
	{
		return m_iLayoutEntryPointIndicies[iEntryIndex];
	}

	return -1;
}

s32 CModelSeatInfo::GetBoneIndexFromSeat(int iSeatIndex) const
{
	if (vehicleVerifyf(iSeatIndex > -1 && iSeatIndex < GetNumSeats(), "Invalid seat index: %i (of %d max). Vehicle Layout %s",iSeatIndex, GetNumSeats(), GetLayoutInfo() ? GetLayoutInfo()->GetName().GetCStr() : "NULL_LAYOUT"))
	{
		return m_iSeatBoneIndicies[iSeatIndex];
	}

	return -1;
}

s32 CModelSeatInfo::GetBoneIndexFromEntryPoint(int iEntryPointIndex) const
{
	vehicleAssertf(iEntryPointIndex > -1 && iEntryPointIndex < GetNumberEntryExitPoints(), "Invalid entry point index: %i",iEntryPointIndex);

	const CEntryExitPoint* pEntryPoint = GetEntryExitPoint(iEntryPointIndex);
	vehicleAssert(pEntryPoint);

	return pEntryPoint->GetDoorBoneIndex();
}

s32 CModelSeatInfo::GetSeatFromBoneIndex(int iBoneIndex) const 
{
	vehicleAssert(iBoneIndex > -1);

	for(int i = 0; i < GetNumSeats(); i++)
	{
		if(GetBoneIndexFromSeat(i) == iBoneIndex)
		{
			return i;
		}
	}
	return -1;
}

s32 CModelSeatInfo::GetShuffleSeatForSeat(s32 iEntryExitPoint, s32 iSeat, bool bAlternateShuffleLink) const
{
	const CEntryExitPoint* pEntryExitPoint = GetEntryExitPoint(iEntryExitPoint);

	if (!pEntryExitPoint)
	{
		return -1;
	}

	//! For multiple seat access types, query seat directly. Might want to cache this, but I didn't want to refactor low
	//! level structs for memory reasons right now.
	if(pEntryExitPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
	{
		const CVehicleSeatInfo* pCurrentSeatInfo = m_pLayoutinfo->GetSeatInfo(iSeat);

		for(int s = 0; s < m_iNumSeats; s++)
		{
			if(s != iSeat)
			{
				const CVehicleSeatInfo* pLayoutSeatInfo = m_pLayoutinfo->GetSeatInfo(s);
				if(pLayoutSeatInfo == pCurrentSeatInfo->GetShuffleLink())
				{
					return s;
				}
			}
		}
	}

	//! Fallback to just getting indirect access seat.
	return bAlternateShuffleLink ? pEntryExitPoint->GetSeat(SA_indirectAccessSeat2) : pEntryExitPoint->GetSeat(SA_indirectAccessSeat);
}

s32 CModelSeatInfo::GetEntryPointFromBoneIndex(int iBoneIndex) const 
{
	vehicleAssert(iBoneIndex > -1);

	for(int i = 0; i < GetNumberEntryExitPoints(); i++)
	{
		if(GetBoneIndexFromEntryPoint(i) == iBoneIndex)
		{
			return i;
		}
	}
	return -1;
}

void CModelSeatInfo::CalculateSeatSituation(const CEntity* pEntity, Vector3& vSeatPosition, Quaternion& qSeatOrientation, s32 iEntryPointIndex, s32 iTargetSeat)
{
	if (!pEntity) return;
	const CModelSeatInfo* pModelSeatinfo = NULL;
	bool isVehicleBike = false;	
	if (pEntity->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else if (pEntity->GetIsTypeVehicle()) 
	{
		const CVehicle* pVeh = static_cast<const CVehicle*>(pEntity);
		pModelSeatinfo  = pVeh->GetVehicleModelInfo()->GetModelSeatInfo();
		isVehicleBike = pVeh->InheritsFromBike();
	} 
	else return;

	// Thanks to the checks above, we know that the entity is a ped or a vehicle, and is thus
	// safe to cast into a CPhysical.
	Assert(pEntity->GetIsPhysical());
	const CPhysical* pPhysicalEntity = static_cast<const CPhysical*>(pEntity);

	// Calculate the reference matrix by applying the offset to the seat matrix
	const CEntryExitPoint* pEntryExitPoint = pModelSeatinfo->GetEntryExitPoint(iEntryPointIndex);
	taskAssertf(pEntryExitPoint, "No Entry point for index %u", iEntryPointIndex);
	int iBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeat(SA_directAccessSeat, iTargetSeat));

	if (taskVerifyf(iBoneIndex!=-1, "CalculateSeatSituation - Bone does not exist"))
	{
		Matrix34 mSeatMat(Matrix34::IdentityType);

		// Get the seat position and orientation in object space (transform by car matrix to get world space pos)
#if ENABLE_FRAG_OPTIMIZATION		
		if (aiVerifyf(pEntity->GetBoneCount() > 0 && (u32)iBoneIndex < pEntity->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pEntity->GetModelName(), iBoneIndex, pEntity->GetBoneCount()))
		{
			pEntity->ComputeObjectMtx(iBoneIndex, RC_MAT34V(mSeatMat));
		}
#else
		if (aiVerifyf(pEntity->GetSkeleton() && (u32)iBoneIndex < pEntity->GetSkeleton()->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pEntity->GetModelName(), iBoneIndex, pEntity->GetSkeleton()->GetBoneCount()))
		{
			mSeatMat = pEntity->GetObjectMtx(iBoneIndex);
		}
#endif
		Vector3 vAdditionalGlobalOffset(0.0f,0.0f,0.0f);
		if (isVehicleBike)
		{
			const CVehicle* pVeh = static_cast<const CVehicle*>(pEntity);
			if (pVeh && pVeh->GetLayoutInfo() && !pVeh->GetLayoutInfo()->GetUsePickUpPullUp())
			{
				// Add on additional offset to bring the height up even if the bike is on its side.
				const float fCz = pEntity->GetTransform().GetC().GetZf();
				float fAdditionalZ = MAX(1.0f - ABS(fCz),0.0f)*mSeatMat.d.z;
				vAdditionalGlobalOffset.z += fAdditionalZ;

				const Matrix34& carMat = RCC_MATRIX34(pPhysicalEntity->GetMatrixRef());

				Matrix34 mPickUpMatrix;
				mPickUpMatrix.Dot(mSeatMat, carMat);

				mPickUpMatrix.d += vAdditionalGlobalOffset;
			}
		}

		const Matrix34& mCarMat = RCC_MATRIX34(pPhysicalEntity->GetMatrixRef());

		Matrix34 mWorldSeatMat;
		mWorldSeatMat.Dot(mSeatMat, mCarMat);

		mWorldSeatMat.d += vAdditionalGlobalOffset;

		// Store as vector / quaternion
		vSeatPosition = mWorldSeatMat.d;

// 		if (isVehicleBike)
// 		{
// 			Vector3 vEulers(0.0f,0.0f,pEntity->GetTransform().GetHeading());
// 			qSeatOrientation.FromEulers(vEulers);
// 		}
// 		else
		{
			mWorldSeatMat.ToQuaternion(qSeatOrientation);
		}
	}
}

void CModelSeatInfo::AdjustEntryMatrixForVehicleOrientation(Matrix34& rEntryMtx, const CVehicle& rVeh, const CPed *pPed, const CModelSeatInfo& rModelSeatInfo, s32 iEntryPointIndex, bool bAlterEntryPointForOpenDoor)
{
	if (pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) && 
		!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingOnsideVehicle) &&
		!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingUpsideDownVehicle) &&
		CTaskVehicleFSM::PedShouldWarpBecauseVehicleAtAwkwardAngle(rVeh))
	{
		return;
	}

	const CVehicle::eVehicleOrientation eVehOrientation = CVehicle::GetVehicleOrientation(rVeh);
	const bool bIsOnSide = eVehOrientation == CVehicle::VO_OnSide || (pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingUpsideDownVehicle));
	const bool bIsUpsideDown = eVehOrientation == CVehicle::VO_UpsideDown || (pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingOnsideVehicle));

	if (bAlterEntryPointForOpenDoor && !rVeh.InheritsFromPlane() && !bIsOnSide && !bIsUpsideDown)
	{
		return;
	}
	
	float fZOffsetScale = 0.0f;
	if (bIsUpsideDown)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, UPSIDE_DOWN_OFFSET, 0.5f, -10.0f, 10.0f, 0.01f);
		fZOffsetScale += UPSIDE_DOWN_OFFSET;
	}
	else
	{
		Vector3 vSeatPosition;
		Quaternion qSeatOrientation;
		CModelSeatInfo::CalculateSeatSituation(&rVeh, vSeatPosition, qSeatOrientation, iEntryPointIndex);

		Matrix34 seatGlbMtx;
		seatGlbMtx.FromQuaternion(qSeatOrientation);
		seatGlbMtx.d = vSeatPosition;

		// Work out the direction to the entry point
		Vector3 vToEntryPoint = rEntryMtx.d - vSeatPosition;
		vToEntryPoint.Normalize();

		const float fRoll = rVeh.GetTransform().GetRoll() / HALF_PI;
		float fOffsetScale = 0.0f;
		const CVehicleEntryPointInfo* pEntryInfo = rModelSeatInfo.GetLayoutInfo()->GetEntryPointInfo(iEntryPointIndex);
		const bool bLeftSide = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT;
		if (bIsOnSide)
		{
			if ((fRoll < 0.0f && bLeftSide) || (fRoll > 0.0f && !bLeftSide))
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, ON_SIDE_OFFSET, 0.5f, -10.0f, 10.0f, 0.01f);
				fZOffsetScale += ON_SIDE_OFFSET;

				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, ON_SIDE_FORWARD_OFFSET, -0.1f, -10.0f, 10.0f, 0.01f);
				Vector3 vForwardOffset = seatGlbMtx.b;
				rEntryMtx.d += vForwardOffset * ON_SIDE_FORWARD_OFFSET;
			}
		}
		else
		{
			Vec3V vMin, vMax;
			if(CVehicle::GetMinMaxBoundsFromDummyVehicleBound(rVeh, vMin, vMax))
			{
				const float fHeight = Abs(vMax.GetZf() - vMin.GetZf());
				// Push out / up or down depending on the height of the vehicle and the amount the vehicle is tilted 
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, ZANGLE_SCALAR, 1.0f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, ANGLE_SCALAR, 0.35f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, HEIGHT_SCALAR, 0.25f, -10.0f, 10.0f, 0.01f);
				fOffsetScale = (ANGLE_SCALAR + HEIGHT_SCALAR * fHeight) * Abs(fRoll);
				const float bSideScalar = bLeftSide ? 1.0f : -1.0f;
				fZOffsetScale = ZANGLE_SCALAR * bSideScalar * fRoll;
			}
		}

		Vector3 vEulers;
		rEntryMtx.ToEulersXYZ(vEulers);
		rEntryMtx.FromEulersXYZ(Vector3(0.0f,0.0f,vEulers.z));
		Vector3 vSideOffset = seatGlbMtx.a;
		if (vToEntryPoint.Dot(vSideOffset) < 0.0f)
		{
			vSideOffset *= -1.0f;
		}
		
		rEntryMtx.d += vSideOffset * fOffsetScale;
	}
	
	rEntryMtx.d.z += fZOffsetScale;

	// See if there is valid ground and alter the position
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, DEFAULT_PROBE_HEIGHT, 1.0f, -10.0f, 10.0f, 0.01f);

	float fProbeHeight = DEFAULT_PROBE_HEIGHT;
	const bool bCanWarpIntoHoveringVehicle = CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(pPed, rVeh, iEntryPointIndex);
	if (bCanWarpIntoHoveringVehicle)
	{
		fProbeHeight = CTaskVehicleFSM::ms_Tunables.m_MaxHoverHeightDistToWarpIntoHeli;
	}

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_GROUND_DIFFERENCE, 0.32f, -10.0f, 10.0f, 0.01f);
	float fMaxGroundDifference = bCanWarpIntoHoveringVehicle ? CTaskVehicleFSM::ms_Tunables.m_MaxHoverHeightDistToWarpIntoHeli : MAX_GROUND_DIFFERENCE;
	Vector3 vTestPos = rEntryMtx.d;
	if (AdjustPositionForGroundHeight(vTestPos, rVeh, *pPed, fProbeHeight, fMaxGroundDifference))
	{
		rEntryMtx.d.z = vTestPos.z;
	}
}

bool CModelSeatInfo::AdjustPositionForGroundHeight(Vector3& vPosition, const CVehicle& rVeh, const CPed& rPed, const float fProbeHeight, const float fMaxGroundDiff)
{
	float fMaxGroundDifference = fMaxGroundDiff;
	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(&rVeh, &rPed, vPosition, fProbeHeight, vGroundPos))
	{
		if (rVeh.InheritsFromPlane())
		{
			const CPlane& rPlane = static_cast<const CPlane&>(rVeh);
			if (rPlane.GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN || rPlane.GetHealth() < rPlane.GetMaxHealth())
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_GROUND_DIFFERENCE_PLANE_DAMAGED, 1.0f, -10.0f, 10.0f, 0.01f);
				fMaxGroundDifference = MAX_GROUND_DIFFERENCE_PLANE_DAMAGED;
			}
		}

		if (Abs(vPosition.z - vGroundPos.z) < fMaxGroundDifference)
		{
			vPosition.z = vGroundPos.z;
			return true;
		}
	}
	return false;
}

void CModelSeatInfo::CalculateEntrySituation(const CEntity* pEntity, const CPed *pPed, Vector3& vEntryPosition, Quaternion& qEntryOrientation, s32 iEntryPointIndex, s32 iEntryFlags, float fClipPhase, const Vector3 * pvPosModifier)
{
	if (!aiVerifyf(iEntryPointIndex != -1, "Invalid entry point passed into CModelSeatInfo::CalculateEntrySituation"))
	{
		return;
	}

	if (!pEntity) return;
	const CModelSeatInfo* pModelSeatinfo = NULL;
	bool isVehicleBike = false;
	bool isVehicleBoatOrSub = false;
	Vector3 vSeatOffsetDist(Vector3::ZeroType);
	if (pEntity->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else if (pEntity->GetIsTypeVehicle()) 
	{
		const CVehicle* pVeh = static_cast<const CVehicle*>(pEntity);
		pModelSeatinfo  = pVeh->GetVehicleModelInfo()->GetModelSeatInfo();
		isVehicleBike = pVeh->InheritsFromBike();
		isVehicleBoatOrSub = pVeh->InheritsFromBoat() || pVeh->InheritsFromSubmarine() || (pVeh->HasWaterEntry());

		const float fSide = pVeh->GetTransform().GetA().GetZf();
		if (isVehicleBike && (iEntryFlags & EF_IsPickUp || iEntryFlags & EF_IsPullUp || Abs(fSide) > CTaskEnterVehicle::ms_Tunables.m_MinMagForBikeToBeOnSide))
		{
			// Calculate the world space position and orientation we would like to get to during the align
			const float fSide = pVeh->GetTransform().GetA().GetZf();
			const CVehicleEntryPointInfo* pEntryInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(iEntryPointIndex); //pVehicle->GetEntryAnimInfo(iEntryPointIndex);
			taskFatalAssertf(pEntryInfo, "CalculateEntrySituation: Entity doesn't have valid entry point info");

			if (!pEntryInfo->GetIsPassengerOnlyEntry())
			{
				const bool bLeftSide = (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT) ? true : false;
				bool bIsPickUp = (iEntryFlags & EF_IsPickUp) != 0;
					
				if (!(iEntryFlags & EF_IsPickUp) && !(iEntryFlags & EF_IsPullUp))
				{
					bIsPickUp = (fSide < 0.0f) ? (bLeftSide ? false : true) : (bLeftSide ? true : false);
				}

				Vector3 vOriginalPos(Vector3::ZeroType);
				Quaternion qOriginalRot(0.0f,0.0f,0.0f,1.0f);

				Vector3 vTargetPos(Vector3::ZeroType);
				Quaternion qTargetRot(0.0f,0.0f,0.0f,1.0f);

				const bool bIsBicycle = pVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE;
				if (bIsPickUp)
				{
					if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVeh, vOriginalPos, qOriginalRot, iEntryPointIndex, CExtraVehiclePoint::PICK_UP_POINT) &&
						CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVeh, vTargetPos, qTargetRot, iEntryPointIndex, CExtraVehiclePoint::QUICK_GET_ON_POINT))
					{
						const float fLeftPickUpTargetLerpPhaseStart = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_LeftPickUpTargetLerpPhaseStart : CTaskEnterVehicle::ms_Tunables.m_LeftPickUpTargetLerpPhaseStartBicycle;
						const float fRightPickUpTargetLerpPhaseStart = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_RightPickUpTargetLerpPhaseStart : CTaskEnterVehicle::ms_Tunables.m_RightPickUpTargetLerpPhaseStartBicycle;
						const float fLeftPickUpTargetLerpPhaseEnd = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_LeftPickUpTargetLerpPhaseEnd : CTaskEnterVehicle::ms_Tunables.m_LeftPickUpTargetLerpPhaseEndBicycle;
						const float fRightPickUpTargetLerpPhaseEnd = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_RightPickUpTargetLerpPhaseEnd : CTaskEnterVehicle::ms_Tunables.m_RightPickUpTargetLerpPhaseEndBicycle;
						const float fStartPhase = bLeftSide ? fLeftPickUpTargetLerpPhaseStart : fRightPickUpTargetLerpPhaseStart;
						const float fEndPhase = bLeftSide ? fLeftPickUpTargetLerpPhaseEnd : fRightPickUpTargetLerpPhaseEnd;
						const float fLerpRatio = rage::Clamp((fClipPhase - fStartPhase) / (fEndPhase - fStartPhase), 0.0f, 1.0f);
						vEntryPosition = vOriginalPos * (1.0f - fLerpRatio) + vTargetPos * fLerpRatio;
						qOriginalRot.Lerp(fClipPhase, qTargetRot);
						qEntryOrientation = qOriginalRot;
					}
				}
				else
				{
					if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVeh, vOriginalPos, qOriginalRot, iEntryPointIndex, CExtraVehiclePoint::PULL_UP_POINT) &&
						CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVeh, vTargetPos, qTargetRot, iEntryPointIndex, CExtraVehiclePoint::QUICK_GET_ON_POINT))
					{
						const float fLeftPullUpTargetLerpPhaseStart = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_LeftPullUpTargetLerpPhaseStart : CTaskEnterVehicle::ms_Tunables.m_LeftPullUpTargetLerpPhaseStartBicycle;
						const float fRightPullUpTargetLerpPhaseStart = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_RightPullUpTargetLerpPhaseStart : CTaskEnterVehicle::ms_Tunables.m_RightPullUpTargetLerpPhaseStartBicycle;
						const float fLeftPullUpTargetLerpPhaseEnd = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_LeftPullUpTargetLerpPhaseEnd : CTaskEnterVehicle::ms_Tunables.m_LeftPullUpTargetLerpPhaseEndBicycle;
						const float fRightPullUpTargetLerpPhaseEnd = bIsBicycle ? CTaskEnterVehicle::ms_Tunables.m_RightPullUpTargetLerpPhaseEnd : CTaskEnterVehicle::ms_Tunables.m_RightPullUpTargetLerpPhaseEndBicycle;
						const float fStartPhase = bLeftSide ? fLeftPullUpTargetLerpPhaseStart : fRightPullUpTargetLerpPhaseStart;
						const float fEndPhase = bLeftSide ? fLeftPullUpTargetLerpPhaseEnd : fRightPullUpTargetLerpPhaseEnd;
						const float fLerpRatio = rage::Clamp((fClipPhase - fStartPhase) / (fEndPhase - fStartPhase), 0.0f, 1.0f);
						vEntryPosition = vOriginalPos * (1.0f - fLerpRatio) + vTargetPos * fLerpRatio;
						qOriginalRot.Lerp(fClipPhase, qTargetRot);
						qEntryOrientation = qOriginalRot;
					}
				}
				qEntryOrientation.Normalize();

				if (!(iEntryFlags & CModelSeatInfo::EF_AnimPlaying))
				{
					// See if there is valid ground and alter the position
					TUNE_GROUP_FLOAT(BIKE_PICK_UP_PULL_UP_TUNE, PROBE_HEIGHT, 3.0f, -10.0f, 10.0f, 0.01f);
					Vector3 vGroundPos(Vector3::ZeroType);
					if (CTaskVehicleFSM::FindGroundPos(pVeh, pPed, vEntryPosition, PROBE_HEIGHT, vGroundPos))
					{
						TUNE_GROUP_FLOAT(BIKE_PICK_UP_PULL_UP_TUNE, MAX_GROUND_DIFFERENCE, 2.0f, -10.0f, 10.0f, 0.01f);
						float fMaxGroundDifference = MAX_GROUND_DIFFERENCE;

						if (Abs(vEntryPosition.z - vGroundPos.z) < fMaxGroundDifference)
						{
							vEntryPosition.z = vGroundPos.z;
						}
					}
				}
				return;
			}
		}
		if (!(iEntryFlags & EF_DontApplyEntryOffsets))
		{
			vSeatOffsetDist.x = pVeh->pHandling->m_fSeatOffsetDistX;
			vSeatOffsetDist.y = pVeh->pHandling->m_fSeatOffsetDistY;
			vSeatOffsetDist.z = pVeh->pHandling->m_fSeatOffsetDistZ;
		}

		if (CTaskVehicleFSM::PedShouldWarpBecauseHangingPedInTheWay(*pVeh, iEntryPointIndex))
		{
			TUNE_GROUP_FLOAT(RANGER_WARP_TUNE, PUSH_OUT_ENTRY_POINT_DIST, 0.8f, 0.0f, 3.0f, 0.01f);
			{
				vSeatOffsetDist.x = PUSH_OUT_ENTRY_POINT_DIST;
			}
		}
	} 
	else return;

	// Calculate the reference matrix by applying the offset to the seat matrix
	const CEntryExitPoint* pEntryExitPoint = pModelSeatinfo->GetEntryExitPoint(iEntryPointIndex);

	if (pEntryExitPoint)
	{
		// Thanks to the checks above, we know that the entity is a ped or a vehicle, and is thus
		// safe to cast into a CPhysical.
		Assert(pEntity->GetIsPhysical());
		const CPhysical* pPhysicalEntity = static_cast<const CPhysical*>(pEntity);

		const Matrix34& mCarMat = RCC_MATRIX34(pPhysicalEntity->GetMatrixRef());
		Matrix34 mSeatMat(Matrix34::IdentityType);

		int iBoneIndex = 0;

		if (pEntryExitPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
		{
			// Default to doing it from the first seat
			iBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeatAccessor().GetSeatByIndex(0));
		}
		else
		{
			iBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeat(SA_directAccessSeat));
		}

		if (taskVerifyf(iBoneIndex!=-1, "CalculateEntrySituation - Bone does not exist"))
		{
			// Figure out what get in animations we play for this door
			const CVehicleEntryPointAnimInfo* pAnimInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPointIndex); //pVehicle->GetEntryAnimInfo(iEntryPointIndex);
			taskFatalAssertf(pAnimInfo, "CalculateEntrySituation: Entity doesn't have valid entry point anim info");

			// Calculate the local translation and orientation change of the enter anim
			// this has been baked into the entry anim info (see CVehicleMetadataMgr::GenerateEntryOffsetInfo())
			// and will need to be regenerated if the entry anims change
			// this is to get around having to stream in the entry anims before even starting to move to the vehicle 
			// as its needed to test which door to get in from
			Vector3 vOffset = pAnimInfo->GetEntryTranslation();
			float fTotalHeadingRotation = pAnimInfo->GetEntryHeadingChange();

			if (pAnimInfo->GetUseVehicleRelativeEntryPosition())
			{
				const CVehicle& rVeh = *static_cast<const CVehicle*>(pEntity);
				const Vec3V vSideOffset = rVeh.GetTransform().GetA() * ScalarVFromF32(vOffset.x);
				const Vec3V vFwdOffset = rVeh.GetTransform().GetB() * ScalarVFromF32(vOffset.y);
				const Vec3V vUpOffset = rVeh.GetTransform().GetC() * ScalarVFromF32(vOffset.z);
				const Vec3V vecEntryPosition = rVeh.GetTransform().GetPosition() + vSideOffset + vFwdOffset + vUpOffset;
				QuatV quatEntryRotation = rVeh.GetTransform().GetOrientation();
				const QuatV quatOffset = QuatVFromZAxisAngle(ScalarVFromF32(fTotalHeadingRotation));
				quatEntryRotation = Multiply(quatEntryRotation, quatOffset);
				vEntryPosition = RCC_VECTOR3(vecEntryPosition);
				qEntryOrientation = RCC_QUATERNION(quatEntryRotation);

				if (!(iEntryFlags & EF_RoughPosition))
				{
					float fExtendedProbeLength = 0.0f;
					bool bIsTulaInWater = MI_PLANE_TULA.IsValid() && MI_PLANE_TULA == rVeh.GetModelIndex() && rVeh.GetIsInWater();
					if (!bIsTulaInWater && rVeh.InheritsFromPlane())
					{
						const CPlane& rPlane = static_cast<const CPlane&>(rVeh);
						if (rPlane.GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN)
						{
							vEntryPosition.z += 1.0f;
							fExtendedProbeLength = 1.0f;
						}
					}

					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, RELATIVE_ENTRY_PROBE_HEIGHT, 2.0f, -10.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, RELATIVE_ENTRY_MAX_GROUND_DIFFERENCE, 0.5f, -10.0f, 10.0f, 0.01f);
					AdjustPositionForGroundHeight(vEntryPosition, rVeh, *pPed, fExtendedProbeLength + RELATIVE_ENTRY_PROBE_HEIGHT, RELATIVE_ENTRY_MAX_GROUND_DIFFERENCE);
				}

				return;
			}

			const bool bApplyRotationInWorldSpace = pModelSeatinfo->GetLayoutInfo()->GetDontOrientateAttachWithWorldUp() ? false : true;

			// We apply an additional global offset to bikes since they can be picked or pulled up
			Vector3	vAdditionalGlobalOffset = vOffset;

			// Get the seat position and orientation in object space (transform by car matrix to get world space pos)
			Matrix34 mSeatMat(Matrix34::IdentityType);
#if ENABLE_FRAG_OPTIMIZATION
			if (aiVerifyf(pEntity->GetBoneCount() > 0 && (u32)iBoneIndex < pEntity->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pEntity->GetModelName(), iBoneIndex, pEntity->GetBoneCount()))
			{
				pEntity->ComputeObjectMtx(iBoneIndex, RC_MAT34V(mSeatMat));
			}
#else
			if (aiVerifyf(pEntity->GetSkeleton() && (u32)iBoneIndex < pEntity->GetSkeleton()->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pEntity->GetModelName(), iBoneIndex, pEntity->GetSkeleton()->GetBoneCount()))
			{
				mSeatMat = pEntity->GetObjectMtx(iBoneIndex);
			}
#endif
			Matrix34 mEntrymat(Matrix34::IdentityType);

			float fWorldHeading = 0.0f;

			bool bAlterEntryPointForOpenDoor = false;
			if (isVehicleBike)
			{
				// This needs to be the bikes world heading
				Matrix34 mGlobalSeatMat(Matrix34::IdentityType);
				pEntity->GetGlobalMtx(iBoneIndex,mGlobalSeatMat);
				fWorldHeading = rage::Atan2f(-mGlobalSeatMat.b.x, mGlobalSeatMat.b.y);

				Vector3 vVehicleForward(0.0f, 1.0f, 0.0f);
				vVehicleForward.RotateZ(fWorldHeading);
#if DEBUG_DRAW				
				Vector3 v1 = mGlobalSeatMat.d;
				Vector3 v2 = v1+vVehicleForward;
				static u32 ARROW_HASH = ATSTRINGHASH("ARROW_HASH", 0xFC167402);
				CTask::ms_debugDraw.AddArrow(RCC_VEC3V(v1), RCC_VEC3V(v2), 0.05f, Color_purple, 1000, ARROW_HASH);
#endif

				vAdditionalGlobalOffset.RotateZ(fWorldHeading);

				// Add on additional offset to bring the height up even if the bike is on its side.
				const float fCz = pEntity->GetTransform().GetC().GetZf();
				float fAdditionalZ = MAX(1.0f - ABS(fCz),0.0f)*mSeatMat.d.z + vSeatOffsetDist.z;
				vAdditionalGlobalOffset.z += fAdditionalZ;
			}
			else
			{
				if (pPed && pEntity->GetIsTypeVehicle() && !(iEntryFlags & EF_DontApplyOpenDoorOffsets))
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
					bAlterEntryPointForOpenDoor = CTaskVehicleFSM::ShouldAlterEntryPointForOpenDoor(*pPed, *pVehicle, iEntryPointIndex, iEntryFlags);
				}

				// Rotate by the local seat matrix to get the offset relative to the local seat matrix
				mSeatMat.Transform3x3(vOffset);

				if (bAlterEntryPointForOpenDoor)
				{
					vOffset.x += pAnimInfo->GetOpenDoorTranslation().x;
					vOffset.y += pAnimInfo->GetOpenDoorTranslation().y;
				}

				// Add the get in offset to the seat matrix to obtain the get in offset in object space
				mSeatMat.d += vOffset;

				// This offset will get applied to each entrypoint so take the side into account
				mSeatMat.d.x += Sign(vOffset.x) * vSeatOffsetDist.x;
				// The other offsets are side independent but still get applied to each entrypoint
				mSeatMat.d.y += vSeatOffsetDist.y;
				mSeatMat.d.z += vSeatOffsetDist.z;

				const CVehicle* pVeh = pEntity->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pEntity) : NULL;
				if (pVeh && pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_DIGGER))
				{
					for(int i = 0; i < pVeh->GetNumberOfVehicleGadgets(); i++)
					{
						CVehicleGadget *pVehicleGadget = pVeh->GetVehicleGadget(i);

						if(pVehicleGadget->GetType() == VGT_ARTICULATED_DIGGER_ARM)
						{
							CVehicleGadgetArticulatedDiggerArm* pDiggerArm = static_cast<CVehicleGadgetArticulatedDiggerArm*>(pVehicleGadget);
							if (pDiggerArm->GetJointSetup(DIGGER_JOINT_BASE))
							{
								const float fJointAngle = pDiggerArm->GetJointAngle(DIGGER_JOINT_BASE);

								static const u32 NUM_CRITICAL_ANGLES = 4;
								static const float CRITICAL_ANGLES[NUM_CRITICAL_ANGLES] =
								{
									QUARTER_PI,
									-QUARTER_PI,
									0.75f * PI,
									-0.75f * PI
								};

								s32 iClosestCriticalAngleIndex = 0;
								float fClosestAngleDelta = Abs(fwAngle::LimitRadianAngle(fJointAngle - CRITICAL_ANGLES[0]));
								for (u32 i=1; i<NUM_CRITICAL_ANGLES; ++i)
								{
									const float fAngleDelta = Abs(fwAngle::LimitRadianAngle(fJointAngle - CRITICAL_ANGLES[i]));
									if (fAngleDelta < fClosestAngleDelta)
									{
										fClosestAngleDelta = fAngleDelta;
										iClosestCriticalAngleIndex = i;
									}
								}

								TUNE_GROUP_FLOAT(DIGGER_TUNE, INFLUENCE_HALF_ARC, EIGHTH_PI, 0.0f, QUARTER_PI, 0.01f);
								const float fModifier = rage::Clamp(1.0f - fClosestAngleDelta / INFLUENCE_HALF_ARC, 0.0f, 1.0f);

								TUNE_GROUP_FLOAT(DIGGER_TUNE, MAX_EXTRA_DIGGER_PUSH_FORWARD_DISTANCE, 0.5f, 0.0f, 4.0f, 0.01f);
								TUNE_GROUP_FLOAT(DIGGER_TUNE, MAX_EXTRA_DIGGER_PUSH_OUT_DISTANCE, 1.5f, 0.0f, 4.0f, 0.01f);
								mSeatMat.d -= mSeatMat.a * fModifier * MAX_EXTRA_DIGGER_PUSH_OUT_DISTANCE;
								mSeatMat.d -= mSeatMat.b * fModifier * MAX_EXTRA_DIGGER_PUSH_FORWARD_DISTANCE;
							}
						}
					}
				}
			}

			// Add on the optional object space position modifier
			if(pvPosModifier)
				mSeatMat.d += *pvPosModifier;

			if (!bApplyRotationInWorldSpace)
			{
				mSeatMat.RotateLocalZ(fTotalHeadingRotation);	
			}

			// Transform the get in offset into world space
			mEntrymat.Dot(mSeatMat, mCarMat);

			if (isVehicleBike)
			{
				// Add on the additional global offset for bikes
				mEntrymat.d += vAdditionalGlobalOffset;

				// Remove x and y components from the rotation as we're only interested in the heading
				Vector3 vCurrentEntryEulers(0.0f,0.0f,fWorldHeading+fTotalHeadingRotation);//(Vector3::ZeroType);
				//mEntrymat.ToEulers(vCurrentEntryEulers, "xyz");
				//vCurrentEntryEulers.x = vCurrentEntryEulers.y = 0.0f;
				mEntrymat.FromEulers(vCurrentEntryEulers, "xyz");

				TUNE_FLOAT(EXTRA_Z_FOR_BIKE, 0.0f, 0.0f, 1.0f, 0.01f);
				mEntrymat.d.z += EXTRA_Z_FOR_BIKE;		

#if DEBUG_DRAW		
				static bool RENDER_ENTRY_POINTS = false;
				if (RENDER_ENTRY_POINTS)
				{
					static dev_float SCALE = 1.0f;
					char szDebugText[128];
					formatf(szDebugText, "ENTRY_MATRIX_%u", iEntryPointIndex);
					CTask::ms_debugDraw.AddAxis(MATRIX34_TO_MAT34V(mEntrymat), SCALE, false, 1000, atStringHash(szDebugText));
				}
#endif
			}
			else
			{
				// Rotate the matrix the correct way so we can add the rotational displacement on correctly
				if (bApplyRotationInWorldSpace)
				{
					mEntrymat.RotateLocalZ(fTotalHeadingRotation);	
				}

				TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, ADJUST_ENTRY_MATRIX_FOR_ORIENTATION, true);
				if (!(iEntryFlags & EF_RoughPosition) && ADJUST_ENTRY_MATRIX_FOR_ORIENTATION && pEntity->GetIsTypeVehicle() && !static_cast<const CVehicle*>(pEntity)->InheritsFromSubmarine() && !(iEntryFlags & EF_DontAdjustForVehicleOrientation))
				{
					AdjustEntryMatrixForVehicleOrientation(mEntrymat, *static_cast<const CVehicle*>(pEntity), pPed, *pModelSeatinfo, iEntryPointIndex, bAlterEntryPointForOpenDoor);
				}

				//! If a swimming entry, use peds position for z, so we don't lerp up/down, which can look bad.
				if(isVehicleBoatOrSub && pPed)
				{
					if((iEntryFlags & EF_AdjustForWater))
					{
						const bool bIsTugBoat = MI_BOAT_TUG.IsValid() && static_cast<const CVehicle*>(pEntity)->GetModelIndex() == MI_BOAT_TUG;
						if (pAnimInfo->GetHasGetInFromWater() || pAnimInfo->GetHasClimbUpFromWater() || pAnimInfo->GetHasVaultUp() || bIsTugBoat)
						{
							mEntrymat.d.z = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;

							CTaskMotionBase* motionTask	= pPed->GetPrimaryMotionTask();
							if(motionTask && (motionTask->IsUnderWater()))
							{
								//! Get water height.
								float fWaterHeight = 0.0f;
								Vector3 vStartPos = mEntrymat.d;
								vStartPos.z += 5.0f;
								if(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vStartPos, &fWaterHeight, true, 20.0f)) 
								{
									mEntrymat.d.z = fWaterHeight;
								}
							}
						}
					}
					else if(!pPed->GetIsInWater() && pAnimInfo->GetHasVaultUp())
					{
						mEntrymat.d.z = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z;
					}
				}
			}

			TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, APPLY_EXTRA_HEADING_FOR_OPEN_DOOR, true);
			if (APPLY_EXTRA_HEADING_FOR_OPEN_DOOR && bAlterEntryPointForOpenDoor)
			{
				mEntrymat.RotateLocalZ(pAnimInfo->GetOpenDoorEntryHeadingChange());
			}

			// Set the return orientation
			mEntrymat.ToQuaternion(qEntryOrientation);

			// Store as vector
			vEntryPosition = mEntrymat.d;

		}
	}
}

void CModelSeatInfo::CalculateExitSituation(CEntity* pEntity, Vector3& vExitPosition, Quaternion& qExitOrientation, s32 iExitPointIndex, const crClip* pExitClip)
{
	if (!pEntity) return;
	const CModelSeatInfo* pModelSeatinfo = NULL;	
	Vector3 vSeatOffsetDist(Vector3::ZeroType);
	if (pEntity->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else if (pEntity->GetIsTypeVehicle()) 
	{		
		const CVehicle* pVeh = static_cast<const CVehicle*>(pEntity);
		pModelSeatinfo  = pVeh->GetVehicleModelInfo()->GetModelSeatInfo();		
		vSeatOffsetDist.x = pVeh->pHandling->m_fSeatOffsetDistX;
		vSeatOffsetDist.y = pVeh->pHandling->m_fSeatOffsetDistY;
		vSeatOffsetDist.z = pVeh->pHandling->m_fSeatOffsetDistZ;
	} 
	else return;

	// Thanks to the checks above, we know that the entity is a ped or a vehicle, and is thus
	// safe to cast into a CPhysical.
	Assert(pEntity->GetIsPhysical());
	const CPhysical* pPhysicalEntity = static_cast<const CPhysical*>(pEntity);

	// Calculate the reference matrix by applying the offset to the seat matrix
	const CEntryExitPoint* pEntryExitPoint = pModelSeatinfo->GetEntryExitPoint(iExitPointIndex);
	const Matrix34& mCarMat = RCC_MATRIX34(pPhysicalEntity->GetMatrixRef());
	Matrix34 mSeatMat(Matrix34::IdentityType);

	int iBoneIndex = 0;

	if (pEntryExitPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
	{
		// Default to doing it from the first seat
		iBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeatAccessor().GetSeatByIndex(0));
	}
	else
	{
		iBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeat(SA_directAccessSeat));
	}

	if (taskVerifyf(iBoneIndex!=-1, "CalculateReferenceMatrix - Bone does not exist"))
	{
		Vector3 vOffset(Vector3::ZeroType);

		// Figure out what get in animations we play for this door
		const CVehicleEntryPointAnimInfo* pAnimInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointAnimInfo(iExitPointIndex); 

		float fTotalHeadingRotation = 0.0f;		
		const crClip* pClip = pExitClip;
		if (!pClip) 
		{
			// Calculate the local translation and orientation change of the exit anim
			if (taskVerifyf(pAnimInfo,"Missing anim info for entry point %i",iExitPointIndex))
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pAnimInfo->GetExitPointClipSetId());
				if (taskVerifyf(pClipSet, "NULL Entry point ClipSet pointer"))
				{
					static fwMvClipId getOutClipId("get_out",0xEBC3C022); //hard coded anim names :(
					pClip = pClipSet->GetClip(getOutClipId);
				}
			}
		}
		if (pClip)
		{
			//Now that we can identify the anim, get the mover track displacement.
			vOffset = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.f, 1.f);
			Quaternion qRotStart = fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f);		
			const float fInitialHeading = qRotStart.TwistAngle(ZAXIS);
			vOffset.RotateZ(-fInitialHeading);
			Quaternion qRotTotal = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.f, 1.f);	
			fTotalHeadingRotation = qRotTotal.TwistAngle(ZAXIS);
		}

		// Get the seat position and orientation in object space (transform by car matrix to get world space pos)
#if ENABLE_FRAG_OPTIMIZATION
		if (aiVerifyf(pEntity->GetBoneCount() > 0 && (u32)iBoneIndex < pEntity->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pEntity->GetModelName(), iBoneIndex, pEntity->GetBoneCount()))
		{
			pEntity->ComputeObjectMtx(iBoneIndex, RC_MAT34V(mSeatMat));
		}
#else
		if (aiVerifyf(pEntity->GetSkeleton() && (u32)iBoneIndex < pEntity->GetSkeleton()->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pEntity->GetModelName(), iBoneIndex, pEntity->GetSkeleton()->GetBoneCount()))
		{
			mSeatMat = pEntity->GetObjectMtx(iBoneIndex);
		}
#endif
		// Rotate by the local seat matrix to get the offset relative to the local seat matrix
		mSeatMat.Transform3x3(vOffset);

		// Add the get in offset to the seat matrix to obtain the get in offset in object space
		mSeatMat.d += vOffset;

		// This offset will get applied to each entrypoint so take the side into account
		mSeatMat.d.x += Sign(vOffset.x) * vSeatOffsetDist.x;
		mSeatMat.d.y += vSeatOffsetDist.y;
		mSeatMat.d.z += vSeatOffsetDist.z;

		// Transform the get in offset into world space
		Matrix34 mEntrymat;
		mEntrymat.Dot(mSeatMat, mCarMat);

		// Rotate the matrix the correct way so we can add the rotational displacement on correctly
		mEntrymat.RotateLocalZ(fTotalHeadingRotation);	

		// Store as vector and quaternion
		vExitPosition = mEntrymat.d;
		mEntrymat.ToQuaternion(qExitOrientation);
	}
}

bool CModelSeatInfo::CalculateThroughWindowPosition(const CVehicle& rVehicle, Vector3& vExitPosition, s32 iSeatIndex)
{
	const CModelSeatInfo* pModelSeatinfo = rVehicle.GetVehicleModelInfo()->GetModelSeatInfo();	
	if (pModelSeatinfo && rVehicle.IsSeatIndexValid(iSeatIndex))
	{
		const s32 iEntryPointIndex = rVehicle.GetDirectEntryPointIndexForSeat(iSeatIndex);
		s32 iSeatBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(iSeatIndex);
		if (iSeatBoneIndex > -1)
		{
			static fwMvClipId clipId("through_windscreen",0x17FB9BF3);

			VehicleEnterExitFlags vehicleFlags;
			fwMvClipSetId clipsetId = CTaskExitVehicleSeat::GetExitClipsetId(&rVehicle, iEntryPointIndex, vehicleFlags);

			const crClip* pClip = NULL;
			if (aiVerifyf(clipsetId != CLIP_SET_ID_INVALID, "Couldn't find exit clip set id"))
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipsetId);
				if (taskVerifyf(pClipSet, "NULL exit point ClipSet pointer"))
				{
					pClip = pClipSet->GetClip(clipId);
				}
			}

			if (!pClip)
				return false;

			const Matrix34& vehMtx = RCC_MATRIX34(rVehicle.GetMatrixRef());
			Matrix34 seatMtx(Matrix34::IdentityType);

#if ENABLE_FRAG_OPTIMIZATION
			if (aiVerifyf(rVehicle.GetBoneCount() > 0 && (u32)iSeatBoneIndex < rVehicle.GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", rVehicle.GetModelName(), iSeatBoneIndex, rVehicle.GetBoneCount()))
			{
				rVehicle.ComputeObjectMtx(iSeatBoneIndex, RC_MAT34V(seatMtx));
			}
#else
			if (aiVerifyf(rVehicle.GetSkeleton() && (u32)iSeatBoneIndex < rVehicle.GetSkeleton()->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", rVehicle.GetModelName(), iSeatBoneIndex, rVehicle.GetSkeleton()->GetBoneCount()))
			{
				seatMtx = rVehicle.GetObjectMtx(iSeatBoneIndex);
			}
#endif
			Vector3 vAnimOffset = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.f, 1.f);
			Quaternion qRotStart = fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f);		
			const float fInitialHeading = qRotStart.TwistAngle(ZAXIS);
			vAnimOffset.RotateZ(-fInitialHeading);

			seatMtx.Transform3x3(vAnimOffset);
			seatMtx.d += vAnimOffset;

			Matrix34 exitMtx(Matrix34::IdentityType);
			exitMtx.Dot(seatMtx, vehMtx);
			vExitPosition = exitMtx.d;
			return true;
		}
	}
	return false;
}

bool CModelSeatInfo::IsInMPAndTargetVehicleShouldUseSeatDistance(const CVehicle* pVeh)
{
	if (!pVeh)
		return false;

	if (!pVeh->InheritsFromHeli())
		return false;

	if (!pVeh->GetSeatManager() || pVeh->GetSeatManager()->GetMaxSeats() <= 2) // Don't allow smaller helicopters without back seats (2 seats or less)
		return false;

	if (MI_HELI_AKULA.IsValid() && pVeh->GetModelIndex() == MI_HELI_AKULA) // Exception is the AKULA, we want players to prefer the front seats with weapons
		return false;

	if (!NetworkInterface::IsGameInProgress())
		return false;

	// Don't use the seat distance for entry scoring as we have two entry points for each turret on valkyrie
	if (pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
		return false;

	return true;
}

bool CModelSeatInfo::RENDER_ENTRY_POINTS = false;

//-------------------------------------------------------------------------
// evaluates the given seat to see if its available
//-------------------------------------------------------------------------

#if __BANK
#define ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, ...) if (pEntryPointDebug) pEntryPointDebug->AddPriorityScoringEvent(CEntryPointPriorityScoringEvent(iEntryPoint, iPriority, __VA_ARGS__));
#define ADD_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, fEntryScore, ...) if (bShouldLogEntryEvaluation) pEntryPointsDebug->AddScoringEvent(CEntryPointScoringEvent(iEntryPoint, iPriority, fEntryScore, __VA_ARGS__));
#define ADD_EARLY_REJECTION_DEBUG_EVENT(iEntryPoint, ...) if (bShouldLogEntryEvaluation) pEntryPointsDebug->AddEarlyRejectionEvent(CEntryPointEarlyRejectionEvent(iEntryPoint, __VA_ARGS__));
#else // __BANK
#define ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, ...)
#define ADD_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, fEntryScore, ...)
#define ADD_EARLY_REJECTION_DEBUG_EVENT(iEntryPoint, ...)
#endif // __BANK

s32 CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(const CPed* pPed, const CEntity* pEntity, SeatRequestType iSeatRequest, s32 iSeatRequested, bool bWarping, VehicleEnterExitFlags iFlags, s32 *pSeatIndex
#if __BANK
															, CRidableEntryPointEvaluationDebugInfo* pEntryPointsDebug
#endif
															, s32 iPreferredEntryPoint
														  )
{
	s32 iBestEntryPoint = -1;
#if __BANK
	const CVehicle* pDebugVehicle = NULL;
	bool bShouldLogEntryEvaluation = false;
	if (pEntryPointsDebug && pEntity && pEntity->GetIsTypeVehicle())
	{
		pDebugVehicle = static_cast<const CVehicle*>(pEntity);
		if (pPed->IsLocalPlayer() || pPed->PopTypeIsMission() || CPedType::IsEmergencyType(pPed->GetPedType()))
		{
			pEntryPointsDebug->Init(pPed, pDebugVehicle, iSeatRequest, iSeatRequested, bWarping, iFlags);
			bShouldLogEntryEvaluation = true;
		}
	}
#endif

	if (!pEntity) 
	{
		// Should probably assert / pass in entity by ref??
		AI_FUNCTION_LOG_WITH_ARGS("Ped %s is returning entrypoint %i because entity is NULL", AILogging::GetDynamicEntityNameSafe(pPed), iBestEntryPoint);
		return iBestEntryPoint;
	}

	bool bIsBike = false;
	bool bIsPlane = false;
	bool bIsBoat = false;
	bool bIsMoving = false;
	bool bIsJetSki = false;
	bool bIsSmallDoubleSidedAccessVehicle = false;
	const CModelSeatInfo* pModelSeatinfo = NULL;	
	if (pEntity->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else if (pEntity->GetIsTypeVehicle()) 
	{
		const CVehicle& rVeh = *static_cast<const CVehicle*>(pEntity);

		bool bIgnoreUpsideDown = false;
		if (MI_PLANE_AVENGER.IsValid() && MI_PLANE_AVENGER == rVeh.GetModelIndex())
		{
			if (bWarping &&
				iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ScriptedTask) &&
				rVeh.IsInAir() &&
				!CTaskExitVehicle::IsVehicleOnGround(rVeh))
			{
				bIgnoreUpsideDown = true;
			}
		}

		if (!bIgnoreUpsideDown && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && CVehicle::GetVehicleOrientation(rVeh) == CVehicle::VO_UpsideDown)
		{
			ADD_EARLY_REJECTION_DEBUG_EVENT(-1, "Vehicle is upside down");
			return iBestEntryPoint;
		}
		pModelSeatinfo  = rVeh.GetVehicleModelInfo()->GetModelSeatInfo();	
		bIsBike = rVeh.InheritsFromBike();
		bIsPlane = rVeh.InheritsFromPlane();
		bIsBoat = rVeh.InheritsFromBoat();
		bIsMoving = rVeh.GetVelocity().Mag2() > rage::square(0.25f);
		bIsJetSki = rVeh.GetIsJetSki();
		bIsSmallDoubleSidedAccessVehicle = rVeh.GetIsSmallDoubleSidedAccessVehicle();
	} 
	else 
	{
#if __BANK
		// Should probably assert
		if (bShouldLogEntryEvaluation)
		{
			AI_FUNCTION_LOG_WITH_ARGS("Ped %s is returning entrypoint %i because entity isn't a ped or a vehicle", AILogging::GetDynamicEntityNameSafe(pPed), iBestEntryPoint);
		}
#endif // __BANK
		return iBestEntryPoint;
	}

	s32 iBestEntryPointPriority = 0;
	float fBestEntryScore = 0.0f;
	Vector3 vBestPosition;

	const s32 iNumberEntryExitPoints = pModelSeatinfo->GetNumberEntryExitPoints();
	const CVehicle* pTargetVehicle = static_cast<const CVehicle*>(pEntity);

	// Go through entry exit point and see if it can 
	for( s32 i = 0; i < iNumberEntryExitPoints; i++ )
	{
		// We might only be interested in sitting at the front or the back, so invalidate
		// any seats that don't match the search criteria
		// Note that this is only interesting for online matches or AI, since in SP the
		// player will always want to seat in the driver seat unless otherwise specified
		// and the same goes for empty vehicles online.	
		if( !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && (!pPed->IsPlayer() || NetworkInterface::IsGameInProgress()) )
		{
			// NEW METHOD
			const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig();
			if (pVehicleEntryConfig)
			{
				const CEntryExitPoint* pEntryPoint = pTargetVehicle->GetEntryExitPoint(i);
				const s32 iSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat);
				const bool bIsFrontSeat = pTargetVehicle->IsSeatIndexValid(iSeatIndex) && pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(iSeatIndex) && pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(iSeatIndex)->GetIsFrontSeat();
				bool bShouldRejectEarly = false;

				for (s32 iUsageInfoIndex=0; iUsageInfoIndex<CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS; ++iUsageInfoIndex)
				{
					if (pVehicleEntryConfig->IsVehicleValidForRejectionChecks(iUsageInfoIndex, *pTargetVehicle))
					{
						if (pVehicleEntryConfig->ShouldRejectBecauseForcedToUseFrontSeats(iUsageInfoIndex, bIsFrontSeat))
						{
							ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Direct access seat %i isn't a front seat and the ped has been forced to use front seats", iSeatIndex);
							bShouldRejectEarly = true;
							break;
						}
						else if (pVehicleEntryConfig->ShouldRejectBecauseForcedToUseRearSeats(iUsageInfoIndex, bIsFrontSeat))
						{
							ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Direct access seat %i is a front seat and the ped has been forced to use rear seats", iSeatIndex);
							bShouldRejectEarly = true;
							break;
						}
						else if (pVehicleEntryConfig->ShouldRejectBecauseForcedToUseSeatIndex(*pTargetVehicle, iSeatIndex))
						{
							BANK_ONLY(const s32 iForcedSeatIndex = pVehicleEntryConfig->GetSeatIndex(iUsageInfoIndex););

							ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Direct access seat %i is being rejected due to ped being forced to use seat %i instead", iSeatIndex, iForcedSeatIndex);
							bShouldRejectEarly = true;
							break;
						}
					}
				}

				if (bShouldRejectEarly)
				{
					continue;
				}
			}
		}
	
		// Don't use entrypoints that have their wing broken off unless jumping out
		if (bIsPlane && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut))
		{
			const CPlane* pPlane = static_cast<const CPlane*>(pEntity);
			if (!pPlane->IsEntryPointUseable(i))
			{
				ADD_EARLY_REJECTION_DEBUG_EVENT(i, "This plane's entry point isn't useable");
				continue;
			}
		}

		// Don't let passengers get on a bike that is on its side
		if (bIsBike)
		{
			const float fSide = pEntity->GetTransform().GetA().GetZf();
			if (Abs(fSide) > CTaskEnterVehicle::ms_Tunables.m_MinMagForBikeToBeOnSide)
			{
				// Calculate the world space position and orientation we would like to get to during the align
				const CVehicleEntryPointInfo* pEntryInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(i);
				taskFatalAssertf(pEntryInfo, "EvaluateSeatAccessFromAllEntryPoints: Entity doesn't have valid entry point info");
				if (pEntryInfo->GetIsPassengerOnlyEntry())
				{
					ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Bike is on it's side and the entry is flagged as IsPassengerOnlyEntry");
					continue;
				}
			}
		}

		const CVehicleEntryPointInfo* pEntryPointInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(i);
		if (pEntryPointInfo && pEntryPointInfo->GetNotUsableOutsideVehicle())
		{
			if (pPed->GetGroundPhysical() != pEntity)
			{
				ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Can't use this entry point if not standing on the target vehicle");
				continue;
			}
		}

		bool bIsVaultUpPoint = false;
		if(bIsBoat)
		{
			const CVehicle* pTargetVehicle = static_cast<const CVehicle*>(pEntity);
			if(pTargetVehicle)
			{
				const CVehicleEntryPointAnimInfo* pAnimInfo = pTargetVehicle->GetEntryAnimInfo(i);
				if (pAnimInfo)
				{
					bIsVaultUpPoint = pAnimInfo->GetHasVaultUp();
					
					if(bIsVaultUpPoint)
					{
						//! Ignore vault up points if this is an on vehicle entry.
						if(!pAnimInfo->GetUseSeatClipSetAnims() &&
							( (pPed->GetGroundPhysical() == pTargetVehicle) || iFlags.BitSet().IsSet(CVehicleEnterExitFlags::EnteringOnVehicle)))
						{
							ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Ped entering on vehicle and this is a vault up point");
							continue;
						}

						//! Ignore vault up points if our best target entry point can already directly access this seat.
						const CEntryExitPoint* pEntryPoint = pTargetVehicle->GetEntryExitPoint(i);
						s32 iSeat = pEntryPoint->GetSeat(SA_directAccessSeat);
						s32 entryPointForSeat = pTargetVehicle->GetDirectEntryPointIndexForSeat(iSeat);

						bool bBestEntryPointCanAddressSeat = false;
						if(entryPointForSeat != i && iBestEntryPoint >= 0)
						{
							const CVehicleEntryPointAnimInfo* pBestEntryAnimInfo = pTargetVehicle->GetEntryAnimInfo(iBestEntryPoint);
							if(pBestEntryAnimInfo && !pBestEntryAnimInfo->GetHasVaultUp())
							{
								const CEntryExitPoint* pBestEntryPoint = pTargetVehicle->GetEntryExitPoint(iBestEntryPoint);
								if(pBestEntryPoint)
								{
									if(pBestEntryPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
									{
										for(int s = 0; s < pBestEntryPoint->GetSeatAccessor().GetNumSeatsAccessible(); ++s)
										{
											if(pBestEntryPoint->GetSeatAccessor().GetSeatByIndex(s) == iSeat)
											{
												bBestEntryPointCanAddressSeat = true;
											}
										}
									}
									else
									{
										const s32 iSeatPedIsIn = pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
										if(pBestEntryPoint->GetSeatAccessor().GetSeat(SA_directAccessSeat, 0) == iSeat || (iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && iSeat != iSeatPedIsIn))
										{
											bBestEntryPointCanAddressSeat = true;
										}
									}
								}
							}
						}

						if(bBestEntryPointCanAddressSeat)
						{
							ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Best entry point can address seat");
							continue;
						}
					}
#if __BANK
					else
					{
						TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, bTestVaultUpPoints, false);
						if(bTestVaultUpPoints && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle))
						{
							continue;
						}
					}
#endif
				}
			}
		}

		if (pEntity->GetIsTypeVehicle()) 
		{
			const CVehicle& rVeh = *static_cast<const CVehicle*>(pEntity);
			bool bEntryPointLocked = NetworkInterface::IsGameInProgress() && pPed->IsPlayer() && rVeh.GetDoorLockStateFromEntryPoint(i) == CARLOCK_LOCKOUT_PLAYER_ONLY;
			if (CTaskVehicleFSM::ShouldWarpInAndOutInMP(rVeh, i))
			{
				if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CheckLockedBeforeWarp) && bEntryPointLocked)
				{
					ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Entry point individual door lock state is marked to be locked for players");
					continue;
				}

				const CVehicleEntryPointAnimInfo* pAnimInfo = rVeh.GetEntryAnimInfo(i);
				if (pAnimInfo && !pAnimInfo->GetNavigateToWarpEntryPoint())
				{
					bWarping = true;
				}
			}
			// Don't allow entry to these seats in sp otherwise, unless marked to also be allowed in in sp
			else if (rVeh.GetEntryInfo(i)->GetMPWarpInOut())
			{
				if(rVeh.GetEntryInfo(i)->GetSPEntryAllowedAlso())
				{
					if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CheckLockedBeforeWarp) && bEntryPointLocked)
					{
						ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Entry point individual door lock state is marked to be locked for players");
						continue;
					}

					if(bIsBoat)
					{
						bWarping = true;
					}
				}
				else
				{
					ADD_EARLY_REJECTION_DEBUG_EVENT(i, "It's an mp warp in out point and we shouldn't warp in");
					continue;
				}
			}
			// Don't allow entry from this entry point if individually excluding the player (MP only)
			else if (bEntryPointLocked)
			{
				ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Entry point individual door lock state is marked to be locked for players");
				continue;
			}
		}

#if __BANK
		CRidableEntryPointEvaluationDebugInfo* pRidableDebugInfo = bShouldLogEntryEvaluation ? &CVehicleDebug::ms_EntryPointsDebug : NULL;
#endif // __BANK

		SeatAccess seatAccess = SA_directAccessSeat;
		s32 selectedSeatIndex = 0;
		s32 iPriority = CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, pEntity, iSeatRequest, iSeatRequested, i, bWarping, iFlags, &seatAccess, &selectedSeatIndex
#if __BANK
			, pRidableDebugInfo
#endif
			);

		// If the priority is 0 it means the entry point cannot be used, so early out so we don't use it
		if (iPriority == 0)
		{
			ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Priority is 0");
			continue;
		}

		Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		Vector3 vOpenDoorPos;
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateEntrySituation(pEntity, pPed, vOpenDoorPos, qEntryOrientation, i);

		float fEntryScore = 0.0f;
		const CEntryExitPoint* pEntryPoint = pModelSeatinfo->GetEntryExitPoint(i);
		aiFatalAssertf(pEntryPoint, "Expected a valid entrypoint");

		s32 iSeat = pEntryPoint->GetSeat(SA_directAccessSeat, iSeatRequested);

		TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, USE_CLOSEST_SEAT_FOR_SCORING_FOR_HELIS, true);
		bool bUseSeatDistance = false;
		if (USE_CLOSEST_SEAT_FOR_SCORING_FOR_HELIS && CModelSeatInfo::IsInMPAndTargetVehicleShouldUseSeatDistance(pTargetVehicle) && pTargetVehicle->IsSeatIndexValid(iSeat))
		{
			bUseSeatDistance = true;
			ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Using seat distance because vehicle is using %s", pTargetVehicle->GetLayoutInfo()->GetName().GetCStr());
		}

		if (bUseSeatDistance)
		{
			CModelSeatInfo::CalculateSeatSituation(pEntity, vOpenDoorPos, qEntryOrientation, i, iSeat);
		}

		float fDistanceToEntryPoint = vPedPosition.Dist(vOpenDoorPos);

		bool bIsAvenger = MI_PLANE_AVENGER.IsValid() && pTargetVehicle->GetModelIndex() == MI_PLANE_AVENGER;
		if(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && !bIsAvenger)
		{
			//! Note: It doesn't really matter what these values are. Just always prioritize direct exits.
			fEntryScore = (seatAccess == SA_directAccessSeat) ? 1.0f : 2.0f;
			ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Direct Exit Priority");
		}
		else
		{
			fEntryScore = fDistanceToEntryPoint;
			ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Distance Score");
		}

		const CVehicleSeatInfo* pSeatInfo = pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(iSeat);
		if(pTargetVehicle && pTargetVehicle->IsSeatIndexValid(iSeat))
		{
			// Skip the slight front seat priority on the Wastelander truck, as it's stopping seat 3 from being accessible (it's very close to seat 1)
			bool bSkipFrontSeatPriority = MI_CAR_WASTELANDER.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_WASTELANDER;

			if (!bSkipFrontSeatPriority && (!USE_CLOSEST_SEAT_FOR_SCORING_FOR_HELIS || !CModelSeatInfo::IsInMPAndTargetVehicleShouldUseSeatDistance(pTargetVehicle)))
			{
				// Weight rear doors as slightly further away so when near the front and back doors of a car peds
				// will generally prefer entering via the front door
				if (pSeatInfo && !pSeatInfo->GetIsFrontSeat())
				{
					if (MI_HELI_AKULA.IsValid() && pTargetVehicle->GetModelIndex() == MI_HELI_AKULA)
					{
						fEntryScore += 2.0f;
						ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Prefer Front Door (Akula)");
					}
					else
					{
						fEntryScore += 1.0f;
						ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Prefer Front Door");
					}
				}
			}
		}

		// Increase the cost if the door is an indirect (we'd need to shuffle)
		if (IsIndirectSeatAccess(seatAccess))
		{
			if (pSeatInfo && pSeatInfo->GetIsFakeSeat())
			{
				ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Rejecting indirect access through fake seat");
				continue;
			}

			// Don't consider indirect entry points if not warping and forced to use direct entry 
			if (!bWarping && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceDirectEntry))
			{
				ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Target seat is indirectly accessible but we've been forced to use direct entry points only");
				continue;
			}

			TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, INDIRECT_ACCESS_PENALTY, 1.1f, 0.0f, 10.0f, 0.01f);
			TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, INDIRECT_ACCESS_PENALTY_DOUBLE_BACK_DOORS, 2.0f, 0.0f, 10.0f, 0.01f);
			float fIndirectAccessPenalty = INDIRECT_ACCESS_PENALTY;
			if (pTargetVehicle->IsSeatIndexValid(iSeat))
			{
				const CVehicleSeatInfo* pSeatInfo = pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(iSeat);
				if (pSeatInfo && !pSeatInfo->GetIsFrontSeat() && CTaskVehicleFSM::DoesVehicleHaveDoubleBackDoors(*pTargetVehicle))
				{
					fIndirectAccessPenalty = INDIRECT_ACCESS_PENALTY_DOUBLE_BACK_DOORS;
				}
			}

			fEntryScore *= fIndirectAccessPenalty;
			ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Indirect access penalty");
		}
		else if (bIsSmallDoubleSidedAccessVehicle)
		{
			TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, DIST_AWAY_FROM_ENTRY_TO_APPLY_WRONG_SIDE_PENALTY, 0.75f, 0.0f, 2.0f, 0.01f);
			if (fDistanceToEntryPoint > DIST_AWAY_FROM_ENTRY_TO_APPLY_WRONG_SIDE_PENALTY)
			{
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, ON_WRONG_SIDE_PENALTY, 1.1f, 0.0f, 10.0f, 0.01f);
				const CVehicle& rVeh = *static_cast<const CVehicle*>(pEntity);
				const bool bPedOnLeftSideOfVehicle = CTaskVehicleFSM::IsPedHeadingToLeftSideOfVehicle(*pPed, rVeh);
				const bool bIsLeftSidedEntry = rVeh.GetEntryInfo(i)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT;
				if ((bIsLeftSidedEntry && !bPedOnLeftSideOfVehicle) ||
					(!bIsLeftSidedEntry && bPedOnLeftSideOfVehicle))
				{
					fEntryScore *= ON_WRONG_SIDE_PENALTY;
					ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "On wrong side penalty");
				}
			}
		}

		// Check if leader is going to this entry point already and is going to sit in that seat
		const CPed* pLeader = CTaskVehicleFSM::GetPedsGroupPlayerLeader(*pPed);
		if (pLeader)
		{
			const s32 iLeaderSeatIndex = CTaskVehicleFSM::GetSeatPedIsTryingToEnter(pLeader);
			const s32 iLeaderEntryIndex = CTaskVehicleFSM::GetEntryPedIsTryingToEnter(pLeader);
			if (iLeaderEntryIndex == i && iLeaderSeatIndex == iSeat)
			{
				ADD_EARLY_REJECTION_DEBUG_EVENT(i, "Leader %s is going to this entry point and to the direct seat %i", AILogging::GetDynamicEntityNameSafe(pLeader), iSeat);
				continue;
			}
		}

		// DMKH. If we are moving towards entry point, scale distance based on speed/angle. Allows us to pick a more
		// appropriate door.
		Vector3 vPedToDoorDir = vOpenDoorPos - vPedPosition;
		vPedToDoorDir.z = 0.0f;
		vPedToDoorDir.Normalize();
		Vector3 vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
		float fToDoorDot = vPedToDoorDir.Dot(vForward);

		static dev_float s_fAngleMinToDoorDot =  0.75f;
		static dev_float s_fAngleMaxToDoorDot = -0.25f;
		static dev_float s_fMaxSpeedMultiplier = 1.5f;
		static dev_float s_fAngleScale = 1.25f;
		static dev_float s_fDistanceToDoAngleCheck = 1.5f;

		if (!bIsMoving)
		{
			if ((fToDoorDot < s_fAngleMinToDoorDot) && 
				(pPed->GetMotionData()->GetCurrentMbrY() >= MOVEBLENDRATIO_WALK) && 
				(fDistanceToEntryPoint > s_fDistanceToDoAngleCheck) )
			{
				//! Faster we go, harder it is to get in at this angle.
				float fSpeedScale = (pPed->GetMotionData()->GetCurrentMbrY()-MOVEBLENDRATIO_WALK)/(MOVEBLENDRATIO_SPRINT - MOVEBLENDRATIO_WALK);
				fSpeedScale = 1.0f + ((s_fMaxSpeedMultiplier - 1.0f) * fSpeedScale);

				fToDoorDot = Clamp(fToDoorDot, s_fAngleMaxToDoorDot, s_fAngleMinToDoorDot);

				//! Mormalise bad angle into 0.0->1.0f (where 0.0 is best, 1.0 is worst). Easier if we flip scale into +.
				float fMaxRange = -s_fAngleMaxToDoorDot + s_fAngleMinToDoorDot;
				float fDotRange = -fToDoorDot + s_fAngleMinToDoorDot;

				float fScale = 1.0f + (((fDotRange / fMaxRange) * s_fAngleScale) * fSpeedScale);

				fEntryScore *= fScale;
				ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Moving towards entrypoint, fSpeedScale = %.2f, fToDoorDot = %.2f", fSpeedScale, fToDoorDot);
			}

			if (iPreferredEntryPoint == i)
			{
				static dev_float s_fPreferredEntryPointMultiplier = 0.7f;	// Incase we have a big vehicle
				static dev_float s_fPreferredEntryPointDistBonus = 1.5f;	// Just to give it an extra boost in edgy cases
				fEntryScore *= s_fPreferredEntryPointMultiplier;
				fEntryScore -= s_fPreferredEntryPointDistBonus;
				ADD_SCORING_DEBUG_EVENT(i, iPriority, fEntryScore, "Preferred entry point");
			}
		}

		// This is a valid entry point, store it if its the best
		if (iBestEntryPoint == -1 ||					// No best entry point set yet
			iPriority > iBestEntryPointPriority ||		// This entry point is higher priority than the current best one
			(iPriority == iBestEntryPointPriority &&	// The priority is the same, but this entry point is closer to the ped
			fEntryScore < fBestEntryScore))
		{
			iBestEntryPoint = i;
			iBestEntryPointPriority = iPriority;
			fBestEntryScore = fEntryScore;
			vBestPosition = vOpenDoorPos;
			if(pSeatIndex)
			{
				*pSeatIndex = selectedSeatIndex;
			}
		}
	}

#if __BANK
	if (bShouldLogEntryEvaluation)
	{
		pEntryPointsDebug->SetChosenEntryPointAndSeat(iBestEntryPoint, pSeatIndex ? *pSeatIndex : -1);
		pEntryPointsDebug->Print();
	}
#endif

	return iBestEntryPoint;
}

s32 CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(const CPed* pPed, const CEntity* pEntity, SeatRequestType iSeatRequest, s32 iSeatRequested, s32 iEntryPoint, bool bWarping, VehicleEnterExitFlags iFlags, SeatAccess* pSeatAccess, s32 *pSeatIndex
#if __BANK
													  , CRidableEntryPointEvaluationDebugInfo* pEntryPointDebug
#endif
													  )
{
#if __BANK
	TUNE_GROUP_INT(VEHICLE_ENTRY_DEBUG, ENTRY_POINT_TO_IGNORE, -1, -1, 16, 1);
	if (ENTRY_POINT_TO_IGNORE >= 0)
	{
		if (iEntryPoint == ENTRY_POINT_TO_IGNORE)
		{
			return 0;
		}
	}
#endif // __BANK

	if (!pEntity)
	{
		return 0;
	}
	const CModelSeatInfo* pModelSeatinfo = NULL;
	const CSeatManager* pSeatManager = NULL;
	bool isInWater = false;
	bool isVehicleBike = false;
	bool isVehicleBoat = false;
	bool isVehicleSub = false;
	bool isVehicleBoatOrSub = false;
	bool isVehicleSeaPlane = false;
	bool isVehicleAmphibious = false;
	bool isVehicleJetSki = false;
	bool isVehicleCar = false;
	bool isVaultUp = false;
	const CVehicle* pTargetVehicle = pEntity->GetIsTypeVehicle()?static_cast<const CVehicle*>(pEntity):NULL;
	const CPed* pTargetPed = pEntity->GetIsTypePed()?static_cast<const CPed*>(pEntity):NULL;
	const CVehicleEntryPointAnimInfo* pAnimInfo = NULL;
	if (pTargetPed) 
	{	
		pModelSeatinfo  = pTargetPed->GetPedModelInfo()->GetModelSeatInfo();
		pSeatManager = pTargetPed->GetSeatManager();
		pAnimInfo = pTargetPed->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPoint);
	} 
	else if (pTargetVehicle) 
	{
		pModelSeatinfo  = pTargetVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		pSeatManager = pTargetVehicle->GetSeatManager();
		isInWater = pTargetVehicle->GetIsInWater();
		isVehicleBike = pTargetVehicle->InheritsFromBike();
		isVehicleBoat = pTargetVehicle->InheritsFromBoat();
		isVehicleSub = pTargetVehicle->InheritsFromSubmarine();
		isVehicleSeaPlane = pTargetVehicle->pHandling && pTargetVehicle->pHandling->GetSeaPlaneHandlingData();
		isVehicleAmphibious = pTargetVehicle->InheritsFromAmphibiousAutomobile() && isInWater;
		isVehicleBoatOrSub = isVehicleBoat || isVehicleSub || isVehicleAmphibious || (pTargetVehicle->HasWaterEntry() && ((pPed && pPed->GetIsInWater()) || isVehicleSeaPlane));
		isVehicleJetSki = pTargetVehicle->GetIsJetSki() || (pTargetVehicle->InheritsFromAmphibiousQuadBike() && isInWater);
		isVehicleCar = pTargetVehicle->GetVehicleType() == VEHICLE_TYPE_CAR;

		if (pTargetVehicle->IsEntryIndexValid(iEntryPoint))
		{
			pAnimInfo = pTargetVehicle->GetEntryAnimInfo(iEntryPoint);
			if (pAnimInfo)
			{
				isVaultUp = pAnimInfo->GetHasVaultUp();
				if(pAnimInfo->GetCannotBeUsedByCuffedPed() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
				{
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, 0, "Ped is handcuffed");
					return 0;
				}
			}
		}
	} 
	else 
	{
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, 0, "No target ped or vehicle");	// Assert?
		return 0;
	}

	const CEntryExitPoint* pEntryPoint = pModelSeatinfo->GetEntryExitPoint(iEntryPoint);

	if (pEntryPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
	{
		if(isVehicleBoatOrSub || (pAnimInfo && pAnimInfo->GetUseVehicleRelativeEntryPosition()))
		{

		}
		else
		{
			// Need to revisit this, just warp onto tourbus for now
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, 0, "Multiple seat access");
			return 0;
		}
	}

	SeatAccess seatAccess = SA_invalid;
	s32 iSelectedSeatID = -1;
	bool bPreferredSeat = false;
	bool bDirectSeatOccupiedByWantedPlayer = false;
	bool bIsLowerPrioritySeat = false;
	bool bIsLowerPriorityEntry = false;
	bool bIsTurretSeat = false;
	const bool bAcceptIndirectEntry = !bWarping && ( ( iFlags.BitSet().IsSet(CVehicleEnterExitFlags::WarpOverPassengers) ) || ( !(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut)) && !(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::UseDirectEntryOnly)) ) );
	bool bTestCollision = false;
	if(!iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut) || isVehicleSub || isVehicleBike || isVehicleJetSki)
	{
		if( (isVehicleBoatOrSub && isVaultUp) || (isVehicleJetSki && isInWater && !pPed->GetIsInWater()) )
		{	
			bTestCollision = !bWarping && !(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen)) && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle);
		}
		else
		{
			bTestCollision = !bWarping && !(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen));
		}
	}

	TUNE_GROUP_BOOL(ENTRY_COLLISION_TEST, FORCED_OFF, false);
	if (FORCED_OFF || pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreVehicleEntryCollisionTests))
	{
		bTestCollision = false;
	}

	bool bPreferDirectEntry = !pPed->IsPlayer();

	// Don't collision test normal boat exits for direct seats, as we could be exiting onto the vehicle
	if (isVehicleBoat && !isVehicleJetSki && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
	{
		const s32 iSeatPedIsIn = pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (pTargetVehicle->IsSeatIndexValid(iSeatPedIsIn))
		{
			const s32 iDirectSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat, iSeatPedIsIn);
			if (iDirectSeatIndex == iSeatPedIsIn)
			{
				bTestCollision = false;
			}
		}

		// If we can exit onto the boat from this seat, heavily prefer it (rather than shuffling / warping over ped)
		const CVehicleEntryPointAnimInfo* pClipInfo = pTargetVehicle->IsEntryIndexValid(iEntryPoint) ? pTargetVehicle->GetEntryAnimInfo(iEntryPoint) : NULL;
		if (pClipInfo && pClipInfo->GetHasGetInFromWater())
		{
			bPreferDirectEntry = true;
		}
	}

	// The player prefers direct entry if he has peds in his group
	//if( !bPreferDirectEntry )
	//{
	//	CPedGroup* pGroup = pPed->GetPedsGroup();
	//	if (Verifyf(pGroup, "Ped (%s) group (%u) was invalid", pPed->GetDebugName(), pPed->GetPedGroupIndex()))
	//	{
	//		bPreferDirectEntry = pGroup->GetGroupMembership()->CountMembersExcludingLeader() > 0;
	//	}
	//}

	s32 iPriority = 0;	

	const bool bStoodOnVehicle = pPed->GetGroundPhysical() == pEntity;
	const CVehicleEntryPointInfo* pEntryPointInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(iEntryPoint);

	// Check each seat to see if its valid and if it can be accessed
	for( s32 iSeat = 0; iSeat < pModelSeatinfo->GetNumSeats(); iSeat++ )
	{
		SeatRequestType eSeatRequestType = iSeatRequest;

		if (!iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && pTargetVehicle)
		{
			// If we're on a vehicle that is flagged to ignore front seats, do so
			const CVehicleSeatInfo* pSeatInfo = pTargetVehicle->GetSeatInfo(iSeat);
			if (bStoodOnVehicle)
			{
				if (pTargetVehicle->GetLayoutInfo()->GetIgnoreFrontSeatsWhenOnVehicle())
				{
					if (pSeatInfo && pSeatInfo->GetIsFrontSeat())
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because we're stood on the vehicle and are ignoring front seats", iSeat);
						continue;
					}
				}
			}

			// B*1984248
			// Check to see if there is enough space to fit a ped in the seat (there might
			// be props cluttering the seat)
			// For now we'll just do it with the Barracks

			u32 uVehicleNameHash = pTargetVehicle->GetVehicleModelInfo()->GetModelNameHash();
			bool bIsBarracksModel = uVehicleNameHash == ATSTRINGHASH("barracks", 0xceea3f4b) || uVehicleNameHash == ATSTRINGHASH("barracks3", 0x2592b5cf);
			bool bIsInsurgentModel = (MI_CAR_INSURGENT.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_INSURGENT) || (MI_CAR_INSURGENT3.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_INSURGENT3);					
			bool bShouldCheckForEnoughRoomToFitPed = pTargetVehicle->m_nVehicleFlags.bCheckForEnoughRoomToFitPed;			

			if( (bIsBarracksModel || bIsInsurgentModel || bShouldCheckForEnoughRoomToFitPed) && !ThereIsEnoughRoomToFitPed(pPed, pTargetVehicle, iSeat) )
			{
				ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because there isn't enough room for the ped", iSeat);
				continue;
			}

			bool bScriptedEnterExit = iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ScriptedTask);
			if (!bScriptedEnterExit && MI_CAR_TECHNICAL2.IsValid() && pTargetVehicle && pTargetVehicle->GetModelIndex() == MI_CAR_TECHNICAL2 && iSeat == 2 && pTargetVehicle->GetIsInWater() && bWarping)
			{
				ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting turret seat %i of Technical2 because it's in water and we're warping", iSeat);
				continue;
			}

			if (pSeatInfo)
			{
				if (pSeatInfo->GetIsFakeSeat())
				{
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Fake seat", iSeat);
					continue;
				}

				// Don't allow access to a trailer hitch seat if we have a trailer attached
				if (pSeatInfo->GetDisableWhenTrailerAttached() && pTargetVehicle->HasTrailer())
				{
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because a trailer is attached to this seat", iSeat);
					continue;
				}
			}

			bool bPassesRearHeliSeatTest = true;

			// Don't bother doing the passenger stuff if we're considering a rear seat and the vehicle is in the air,
			// otherwise we'll end up not being able to select those seats, because of changing the seat request to specific (driver)
			if (pTargetVehicle->InheritsFromHeli())
			{
				// I feel bad about the const-cast
				if (pSeatInfo && !pSeatInfo->GetIsFrontSeat() && const_cast<CVehicle*>(pTargetVehicle)->IsInAir())
				{
					bPassesRearHeliSeatTest = false;
				}
			}	
					
			if (pPed->IsLocalPlayer() && pSeatManager->GetDriver() && bPassesRearHeliSeatTest)
			{
				CPed& rDriver = *pSeatManager->GetDriver();
				if (eSeatRequestType == SR_Specific) // and we are in mp and the vehicle has a friendly player in it / vehicle can be entered as a passenger
				{
					const bool bIsAiDriverWhoCanDrivePlayer = !rDriver.IsAPlayerPed() && !rDriver.ShouldBeDead() &&
						rDriver.GetPedConfigFlag(CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger);
		
					if (bIsAiDriverWhoCanDrivePlayer && pTargetVehicle->IsTaxi())
					{
						// Don't allow entry to front passenger seat
						if (iSeat != 0 && pTargetVehicle->GetSeatInfo(iSeat)->GetIsFrontSeat())
						{
							ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because not allowed entry to front passenger", iSeat);
							continue;
						}
					}

					if (bIsAiDriverWhoCanDrivePlayer)
					{
						eSeatRequestType = SR_Prefer;
						AI_LOG_WITH_ARGS("[EntryExit][EvaluateSeatAccessFromEntryPoint] Changing seat request type for seat %i from SR_Specific to SR_Prefer due to bIsAiDriverWhoCanDrivePlayer = true\n",iSeat);
					}
				}
				// When jacking friendly players, always use the specific seat request
				else if (rDriver.IsAPlayerPed() && !bStoodOnVehicle && (iFlags.BitSet().IsSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed) || iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds)))
				{
					if (!rDriver.GetPedConfigFlag(CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar))
					{
						const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig();
						if (!pVehicleEntryConfig || !pVehicleEntryConfig->HasAForcingFlagSetForThisVehicle(pTargetVehicle))
						{
							if (NetworkInterface::IsGameInProgress() && (!NetworkInterface::IsFriendlyFireAllowed(&rDriver, pPed) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NotAllowedToJackAnyPlayers) || (pTargetVehicle && pTargetVehicle->GetExclusiveDriverPed()) ))
							{
								eSeatRequestType = SR_Any;
								AI_LOG_WITH_ARGS("[EntryExit][EvaluateSeatAccessFromEntryPoint] Changing seat request type for seat %i to SR_Any due to considering jacking but not allowed/able to do it\n",iSeat);
							}
							else
							{
								eSeatRequestType = SR_Specific;
								AI_LOG_WITH_ARGS("[EntryExit][EvaluateSeatAccessFromEntryPoint] Changing seat request type for seat %i to SR_Specific due to jacking driver\n",iSeat);
							}
						}
					}
				}
			}
		}

		SeatValidity seatValidity = CTaskVehicleFSM::GetSeatValidity(eSeatRequestType, iSeatRequested, iSeat);
		if( seatValidity != SV_Invalid )
		{
			SeatAccess thisSeatAccess = pEntryPoint->CanSeatBeReached(iSeat);
			if( thisSeatAccess != SA_invalid )
			{
				s32 seat = pEntryPoint->GetSeat(thisSeatAccess, iSeat);

				// Deprioritize certain seats (like hanging on the side of a vehicle)
				const CVehicleSeatAnimInfo* pSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromSeatIndex(pEntity, seat);
				if (pSeatAnimInfo)
				{
					// Seat flagged to only allow direct entry, if this access is indirect, ignore this entry if we're not in the vehicle
					if (!pPed->GetIsInVehicle())
					{
						if (pSeatAnimInfo->GetUseDirectEntryOnlyWhenEntering() && IsIndirectSeatAccess(thisSeatAccess))
						{
							ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because seat flagged for direct entry only", iSeat);
							continue;
						}
					}

					bool bFailedOnVehicleEntryPointChecks = !bWarping && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && !pPed->GetGroundPhysical() && 
						pAnimInfo->GetHasOnVehicleEntry() && !pEntryPointInfo->GetVehicleExtraPointsInfo();

					if (thisSeatAccess == SA_indirectAccessSeat || thisSeatAccess == SA_indirectAccessSeat2)
					{
						s32 directSeat = pEntryPoint->GetSeat(SA_directAccessSeat);
						const CVehicleSeatAnimInfo* pDirectSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromSeatIndex(pEntity, directSeat);
						if (pDirectSeatAnimInfo && pDirectSeatAnimInfo->IsTurretSeat())
						{
							if (bFailedOnVehicleEntryPointChecks)
							{
								ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because this is an indirect access 'on vehice' entry point and ped isn't on it", iSeat);
								continue;
							}
						}
					}

					if (pSeatAnimInfo->IsTurretSeat())
					{
						// Ignore this entry point if its directly connected to the turret and we're not 
						// on the vehicle when entering
						if (thisSeatAccess == SA_directAccessSeat)
						{
							if (bFailedOnVehicleEntryPointChecks)
							{
								ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because this is an on vehicle entry point and ped isn't on it", iSeat);
								continue;
							}
						}

						// Check if entry point should be ignored due to turret angle or door blocking climb
						if (ShouldLowerEntryPriorityDueToTurretAngle(pPed, pSeatAnimInfo, pTargetVehicle, pEntryPointInfo, seat)
							|| ShouldLowerEntryPriorityDueToTurretDoorBlock(iSeat, iEntryPoint, pPed, pTargetVehicle))
						{
							bIsLowerPriorityEntry = true;
						}
						else if (ShouldInvalidateEntryPointDueToOnBoardJack(seat, iEntryPoint, *pPed, pSeatAnimInfo, *pTargetVehicle, pEntryPointInfo))
						{
							ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because it's invalidated due to on board jack", iSeat);
							continue;
						}

						bIsTurretSeat = true; // Don't trust me! I'll be true if a turret seat is on the vehicle at all, which makes me useless outside this seat loop. Use pTargetVehicle->IsTurretSeat(iSelectedSeatID) after the loop instead.
					}

					// Lower seat priority based on certain vehicle / seat flags
					const bool bNetworkGameInProgress = NetworkInterface::IsGameInProgress();
					const bool bLowerPriorityInNetworkBecauseWarpSeat = CTaskVehicleFSM::ShouldWarpInAndOutInMP(*pTargetVehicle, iEntryPoint) && !pEntryPointInfo->GetForceSkyDiveExitInAir();
					const bool bModelSeatIsFlaggedAsLowPriority = pSeatAnimInfo->GetIsLowerPrioritySeatInSP();
					
					bool bIsRoosevelt = ( (MI_CAR_BTYPE.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_BTYPE.GetModelIndex())
										  || (MI_CAR_BTYPE3.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_BTYPE3.GetModelIndex()) );
					bool bIsBrickade = MI_CAR_BRICKADE.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_BRICKADE.GetModelIndex();
					if (bIsRoosevelt || bIsBrickade)
					{	
						// For these vehicles, ignore flag and lower priority for everyone except player peds in MP
						if (bLowerPriorityInNetworkBecauseWarpSeat || (bModelSeatIsFlaggedAsLowPriority && (!bNetworkGameInProgress || !pPed->IsAPlayerPed())))
						{
							bIsLowerPrioritySeat = true;

							if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats))
							{
								bIsLowerPrioritySeat = false;
								ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because ped prevented from using lower priority seats ai or sp", iSeat);
								continue;
							}
						}
					}
					else
					{
						// Lowers priority for everyone (to prevent hanging seats taking priority over rear seats in the same location, such as the Insurgent - B*2037099)
						if( bLowerPriorityInNetworkBecauseWarpSeat || bModelSeatIsFlaggedAsLowPriority )
						{
							bIsLowerPrioritySeat = true;

							if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats))
							{
								bIsLowerPrioritySeat = false;
								ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because ped prevented from using lower priority seats", iSeat);
								continue;
							}
						}
					}
				}
				
				// Get the ped in or entering this seat (excluding this ped)
				CPed* pSeatOccupier = CTaskVehicleFSM::GetPedInOrUsingSeat(*pTargetVehicle, seat, pPed);
				const bool bPedUsingSeatIsInSeat = (pSeatManager->GetPedInSeat(seat) == pSeatOccupier);
				const bool bSeatOccupierIsShufflingFromSeat = (pSeatOccupier && bPedUsingSeatIsInSeat) ? pSeatOccupier->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) : false;

				// If we're using the entry point for climb up only, we don't care about the seat occupier
				if (pEntryPointInfo->GetClimbUpOnly() && !bStoodOnVehicle)
				{
					pSeatOccupier = NULL;
				}

				// Check that the Seat can be used if entering the vehicle
				if (pSeatOccupier && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle))
				{
					if (pSeatAnimInfo && pSeatAnimInfo->GetCannotBeJacked())
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because it is specified as not jackable", iSeat);
						continue;
					}

					//some entry points are not supported for jacking
					const CVehicleEntryPointAnimInfo* pAnimInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPoint); 
					if (pAnimInfo && pAnimInfo->GetIsJackDisabled())
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because entry has jacks disabled", iSeat);
						continue;
					}
				}

				if( !bSeatOccupierIsShufflingFromSeat )
				{
					bool bIgnoreEntryPointUsageCheck = false;	// Should ignore entry point usage check because player leader trying to enter different seat
					const CPed* pLeader = CTaskVehicleFSM::GetPedsGroupPlayerLeader(*pPed);
					if (pSeatOccupier && pLeader == pSeatOccupier)
					{
						const s32 iLeaderSeatIndex = CTaskVehicleFSM::GetSeatPedIsTryingToEnter(pLeader);
						if (pTargetVehicle->IsSeatIndexValid(iLeaderSeatIndex) && iLeaderSeatIndex != seat)
						{
							ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Ignoring entry point usage check due to leader ped %s using seat %i, but wanting to go seat %i", AILogging::GetDynamicEntityNameSafe(pLeader), iSeat, iLeaderSeatIndex);
							bIgnoreEntryPointUsageCheck = true;
						}
					}

					if (!bIgnoreEntryPointUsageCheck && !CTaskVehicleFSM::CanUsePedEntryPoint( pPed, pSeatOccupier, iFlags))
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i, ped can't use entry point due to other ped %s using the seat", iSeat, AILogging::GetDynamicEntityNameSafe(pSeatOccupier));
						continue;
					}

					// Always consider peds that are animating into the vehicle if they will get into this seat, 
					// so the vehicle evaluation code can exclude the seat this ped is entering
					// But ignore the check for cop peds that are trying to get close to the car to arrest you
					bool bIsACopTryingToArrest = !pPed->IsPlayer() && !NetworkInterface::IsGameInProgress() && CPedType::IsLawEnforcementType(pPed->GetPedType()) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack);
					if (pSeatOccupier && pSeatOccupier->GetIsEnteringVehicle() && (CTaskVehicleFSM::GetSeatPedIsTryingToEnter(pSeatOccupier) == seat) && !bIsACopTryingToArrest)
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i, ped can't use entry point due to ped %s trying to enter seat %i", iSeat, AILogging::GetDynamicEntityNameSafe(pSeatOccupier), seat);
						continue;
					}
				}

				// Cache whether this seat is occupied by a hated ped
				if( pSeatOccupier && pSeatOccupier->IsAPlayerPed() && !pSeatOccupier->IsInjured() &&
					(thisSeatAccess == SA_directAccessSeat||seat == pModelSeatinfo->GetDriverSeat()) && 
					iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackWantedPlayersRatherThanStealCar) )
				{
					bDirectSeatOccupiedByWantedPlayer = pSeatOccupier->GetPlayerWanted() && pSeatOccupier->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL1;
				}

				if(seat == pModelSeatinfo->GetDriverSeat())
				{
					// Check if we're disallowed to use the driver seat, and if so, check if this is the driver seat.
					if(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::DontUseDriverSeat))
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because told not to use driver seat", iSeat);
						continue;
					}

					// Check if we have set an exclusive driver ped. If so, check this ped is that ped.
					if(pTargetVehicle && 
						!iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && 
						!pTargetVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i because seat has an exclusive driver condition", iSeat);
						continue;
					}
				}

				// If the access to this seat is indirect, make sure the seat the ped needs to go through can be cleared
				if (pEntryPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle && IsIndirectSeatAccess(thisSeatAccess))
				{
					s32 directSeat   = pEntryPoint->GetSeat(SA_directAccessSeat);
					s32 indirectSeat = pEntryPoint->GetSeat(SA_indirectAccessSeat);
					s32 indirectSeat2 = pEntryPoint->GetSeat(SA_indirectAccessSeat2);
					CPed* pDirectSeatOccupier = CTaskVehicleFSM::GetPedInOrUsingSeat(*pTargetVehicle, directSeat);

                    bool pedWillShuffleOver = false;

					//! Don't shuffle over driver seat, if we aren't allowed to use it.
					if( (directSeat == pModelSeatinfo->GetDriverSeat()) && 
						pTargetVehicle && !pTargetVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i for shuffling because seat has an exclusive driver condition", iSeat);
						continue;
					}

                    if(NetworkInterface::IsGameInProgress() && pTargetVehicle && pDirectSeatOccupier && CTaskVehicleFSM::CanForcePedToShuffleAcross(*pPed, *pDirectSeatOccupier, *pTargetVehicle))
                    {
                        if(directSeat != pModelSeatinfo->GetDriverSeat() && indirectSeat != pModelSeatinfo->GetDriverSeat() && indirectSeat2 != pModelSeatinfo->GetDriverSeat())
                        {
							if(pPed->GetPedIntelligence()->IsFriendlyWith(*pDirectSeatOccupier))
							{
								CPed* pIndirectSeatOccupier = pSeatManager->GetPedInSeat(indirectSeat);

								if(!pIndirectSeatOccupier)
								{
									pedWillShuffleOver = true;
									ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Ped %s thinks ped %s in seat %i will shuffle over to seat %i for us, so isn't this invalidating entry", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pSeatOccupier), iSeat, indirectSeat);
								}
							}
                        }
                    }

					// forced exit peds can't use indirect exit seats
					if( iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ForcedExit) || ( !CTaskVehicleFSM::CanJackPed( pPed, pDirectSeatOccupier, iFlags) && !(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::WarpOverPassengers) && pSeatOccupier == NULL) && !pedWillShuffleOver ) )
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Not selecting seat %i due to forced exit or can't jack ped in direct seat", iSeat);
						continue;
					}
				}

				if( bAcceptIndirectEntry || (thisSeatAccess == SA_directAccessSeat) )
				{
					if( seatAccess == SA_invalid || (IsIndirectSeatAccess(seatAccess) && !bPreferredSeat) || seatValidity == SV_Preferred )
					{
						seatAccess = thisSeatAccess;
						iSelectedSeatID = iSeat;
					}
					if( seatValidity == SV_Preferred )
					{
						TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, DONT_PREFER_INDIRECT_IN_MP, true);
						bool bCanIncreasePriorityDueToPrefer = true;
						if (DONT_PREFER_INDIRECT_IN_MP && NetworkInterface::IsGameInProgress() && pTargetVehicle && pPed && pPed->IsLocalPlayer() 
							&& iSeatRequested == 0 && thisSeatAccess == SA_indirectAccessSeat
							&& !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds)
							&& !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed))
						{
							const CPed* pPedUsingDriversSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*pTargetVehicle, pTargetVehicle->GetDriverSeat());

							if (pPedUsingDriversSeat)
							{
								const bool bFriendlyPedEnteringDriversSeat = pPed->GetPedIntelligence()->IsFriendlyWith(*pPedUsingDriversSeat);
								if (bFriendlyPedEnteringDriversSeat)
								{
									bCanIncreasePriorityDueToPrefer = false;
								}
							}	
						}

						if (bCanIncreasePriorityDueToPrefer)
						{
							bPreferredSeat = true;
							iPriority = PREFERED_SEAT_PRIORITY;
							ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Increased due to prefer");
						}
					}
				}
			}
#if __BANK
			else
			{
				ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Rejecting because can't access seat %i", iSeat);
			}
#endif // __BANK
		}
	}

	if (seatAccess==SA_invalid || iSelectedSeatID == -1)
	{
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, 0, "Couldn't find valid seat");
		return 0;
	}

	if (pSeatAccess)
	{
		*pSeatAccess = seatAccess;
	}

	if (pSeatIndex)
	{
		*pSeatIndex = iSelectedSeatID;
	}

	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryRot(0.0f, 0.0f, 0.0f, 1.0f);
	s32 nFlags = 0;

	if (((pAnimInfo && pAnimInfo->GetHasOnVehicleEntry()) && pPed->GetGroundPhysical() == pEntity) || iFlags.BitSet().IsSet(CVehicleEnterExitFlags::EnteringOnVehicle))
	{
		CTaskEnterVehicleAlign::ComputeAlignTarget(vEntryPos, qEntryRot, true, *pEntity, *pPed, iEntryPoint);
		// Possibly need a check to see if we can reach the on vehicle entry point here
	}
	else
	{
		bool bUseEntrySituation = true;
		if (iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen) && pEntity->GetIsTypeVehicle())
		{
			Vector3 vExitPos(Vector3::ZeroType);
			if (CModelSeatInfo::CalculateThroughWindowPosition(*static_cast<const CVehicle*>(pEntity), vEntryPos, iEntryPoint))
			{
				bUseEntrySituation = false;
			}
		}

		if (bUseEntrySituation)
		{
			CModelSeatInfo::CalculateEntrySituation(pEntity, pPed, vEntryPos, qEntryRot, iEntryPoint, nFlags);
		}
	}

	// When exiting submersibles, we want to do a collision test at the top get_in point instead of the side entry points
	bool bIsAPC = MI_CAR_APC.IsValid() && pTargetVehicle->GetModelIndex() == MI_CAR_APC;
	if ((isVehicleSub || bIsAPC) && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle))
	{
		CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*static_cast<const CVehicle*>(pEntity), vEntryPos, qEntryRot, iEntryPoint, CExtraVehiclePoint::GET_IN);
	}

	Matrix34 testMatrix;
	testMatrix.d = vEntryPos;
	testMatrix.FromQuaternion(qEntryRot);

	// Reject all boat entry points if the boat is upside down
	if (isVehicleBoatOrSub)
	{
		bool bIsSeabreeze = MI_PLANE_SEABREEZE.IsValid() && pTargetVehicle->GetModelIndex() == MI_PLANE_SEABREEZE && !pTargetVehicle->GetIsInWater();
		if (!bIsSeabreeze && pEntity->GetTransform().GetC().GetZf() <= 0.0f)
		{
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Boat or sub upside down");
			return 0;
		}
	}

	// Push the test position out further for peds diving out of cars
	if (iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut) && !isVehicleBike && pEntity->GetIsTypeVehicle())
	{
		TUNE_GROUP_FLOAT(RAGDOLL_ON_EXIT_TUNE, X_JUMP_OUT_OFFSET, 0.0f, -5.0f, 5.0f, 0.01f);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_EXIT_TUNE, Y_JUMP_OUT_OFFSET_SCALE, 0.5f, -10.0f, 10.0f, 0.01f);
		const float fXOffset = pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? -X_JUMP_OUT_OFFSET : X_JUMP_OUT_OFFSET;
		const float fYOffset = static_cast<const CVehicle*>(pEntity)->GetVelocity().Mag() * Y_JUMP_OUT_OFFSET_SCALE;
		Vec3V vOffset(fXOffset, fYOffset, 0.0f);
		testMatrix.d += VEC3V_TO_VECTOR3(pEntity->GetTransform().Transform3x3(vOffset));
#if __DEV
		Vec3V vStart = VECTOR3_TO_VEC3V(testMatrix.d - Vector3(0.0f,0.0f,1.0f));
		Vec3V vEnd = VECTOR3_TO_VEC3V(testMatrix.d + Vector3(0.0f,0.0f,1.0f));
		CTask::ms_debugDraw.AddCapsule(vStart, vEnd, 0.25f, Color_blue, 100, 0, false);
#endif // __DEV
	}

	if (iFlags.BitSet().IsSet(CVehicleEnterExitFlags::VehicleIsUpsideDown) && isVehicleCar)
	{
		static Vector3 svExtraOffset(0.0f,0.0f,1.0f);
		Vector3 vOffset = svExtraOffset;
		testMatrix.FromEulers(Vector3(0.0f,0.0f, pEntity->GetTransform().GetHeading()), "xyz");
		testMatrix.Transform3x3(vOffset);
		testMatrix.d += vOffset;
	}

	// Collision check
	if (bTestCollision)
	{
		// If we have a driver then there is an additional distance check we want to do before actually checking collision
		if( iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IgnoreEntryPointCollisionCheck) && 
			pTargetVehicle &&
			pTargetVehicle->GetDriver() )
		{
			// Make sure we are close enough to our vehicle
			ScalarV scDistToVehicleSq = DistSquared(pPed->GetTransform().GetPosition(), pTargetVehicle->GetTransform().GetPosition());
			ScalarV scMaxDistToVehicleSq = ScalarVFromF32(rage::square(CTaskEnterVehicle::ms_Tunables.m_MaxDistanceToCheckEntryCollisionWhenIgnoring));
			if(IsGreaterThanAll(scDistToVehicleSq, scMaxDistToVehicleSq))
			{
				bTestCollision = false;
			}
		}

		if(bTestCollision)
		{
			const float fRoll = Abs(pEntity->GetTransform().GetRoll());
			const bool bPassedRollForExtraTest = fRoll > CTaskVehicleFSM::ms_Tunables.m_MinRollToDoExtraTest;

			TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, TEST_AGAINST_VEHICLE_IF_UPRIGHT, true);
			const bool bIncludeVehicle = TEST_AGAINST_VEHICLE_IF_UPRIGHT && bPassedRollForExtraTest && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && (!pTargetVehicle || CVehicle::GetVehicleOrientation(*pTargetVehicle) == CVehicle::VO_Upright);

			CEntity* pHitEntity = 0;
			if (!CModelSeatInfo::IsPositionClearForPed(pPed, pEntity, testMatrix, !bIncludeVehicle, 0.0f, &pHitEntity))
			{
				if ((isVehicleBike || isVehicleJetSki) && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut))
				{
					// We can still jump out (and trigger collision with the wall), but prefer other exits if possible
					bIsLowerPriorityEntry = true;
				}
				else
				{
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, 0, "Position isn't clear for ped, hit %s[%p]", pHitEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(pHitEntity)) : pHitEntity->GetModelName(), pHitEntity);
					return 0;
				}
				
			}

			// See if we can open the door
			if (!iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut))
			{
				TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, TEST_FOR_BLOCKED_DOOR, true);
				if (TEST_FOR_BLOCKED_DOOR && pTargetVehicle && pTargetVehicle->InheritsFromAutomobile() && !CVehicle::IsDoorClearForPed(testMatrix.d, *pTargetVehicle, *pPed, iEntryPoint))
				{
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Door isn't clear for ped");
					return 0;
				}	
			}	
		}
	}

	bool bIsBombushka = MI_PLANE_BOMBUSHKA.IsValid() && pTargetVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA;
	bool bIsVolatol = MI_PLANE_VOLATOL.IsValid() && pTargetVehicle->GetModelIndex() == MI_PLANE_VOLATOL;

	if (!bWarping)
	{
		//! If we trying to get in a boat, assume all entry points are the same - we don't prefer shallow entry points as we'd
		//! have to navigate around the boat.
		if(isVehicleBoatOrSub && pPed && pPed->GetIsSwimming() && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle))
		{
			iPriority += GROUND_ENTRY_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Boat or sub ground priority");
		}
		else
		{
			Vector3 vGroundPos(Vector3::ZeroType);
			const bool bFoundGround = !CTaskExitVehicle::IsVerticalProbeClear(testMatrix.d, CTaskExitVehicleSeat::ms_Tunables.m_InAirProbeDistance, *pPed, &vGroundPos);
			if (!bFoundGround)
			{
				const bool bCanPedWarpIntoHoveringVehicle = pPed && CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(pPed, *pTargetVehicle, iEntryPoint, true);

				// If we didn't find ground and we're exiting a non water vehicle, reject this entry point
				if (!bCanPedWarpIntoHoveringVehicle &&
					!iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) &&
					!isVehicleBoatOrSub &&
					!isVehicleJetSki)
				{
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Didn't find ground, entering non water vehicle");
					return 0;
				}

				iPriority += IN_AIR_ENTRY_PRIORITY;
				ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Didn't find ground, in air entry priority");
			}
			else
			{
				bool bIgnoreGroundHeightTest = false;
				if (pPed && pPed->GetGroundPhysical() && pTargetVehicle && pTargetVehicle->IsTurretSeat(iSelectedSeatID))
				{
					bIgnoreGroundHeightTest = true;
				}
				const float fGroundToEntryHeightDiff = Abs(testMatrix.d.z - vGroundPos.z);
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_GROUND_TO_ENTRY_HEIGHT_DIFF, 2.0f, 0.0f, 3.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_GROUND_TO_ENTRY_HEIGHT_DIFF_LARGE_PLANE, 3.0f, 0.0f, 3.0f, 0.01f);
				const float fMaxHeightDiff = (bIsBombushka || bIsVolatol) ? MAX_GROUND_TO_ENTRY_HEIGHT_DIFF_LARGE_PLANE : MAX_GROUND_TO_ENTRY_HEIGHT_DIFF;

				if (!bIgnoreGroundHeightTest && fGroundToEntryHeightDiff > fMaxHeightDiff)
				{
					// Don't consider this entry valid if entering
					if (!iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && 
						!isVehicleBoatOrSub &&
						!isVehicleJetSki)
					{
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Too far from ground, fGroundToEntryHeightDiff = %.2f", fGroundToEntryHeightDiff);
						return 0;
					}
					else
					{
						iPriority += IN_AIR_ENTRY_PRIORITY;
						ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "In air entry priority");
					}
				}
				else
				{
					iPriority += GROUND_ENTRY_PRIORITY;
					ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Ground entry priority");
				}	
			}
		}
	}

	const bool bPreferLeftEntry = iFlags.BitSet().IsSet(CVehicleEnterExitFlags::PreferLeftEntry);
	const bool bPreferRightEntry = iFlags.BitSet().IsSet(CVehicleEnterExitFlags::PreferRightEntry);
	if ((bPreferLeftEntry && pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT) ||
		(bPreferRightEntry && pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_RIGHT))
	{
		iPriority += PREFERED_SEAT_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Preferred entry priority");
	}

	// If the peds prefer a direct entry seat, weight this seat to whether its direct or indirect to the target seat
	if (bPreferDirectEntry)
	{
		iPriority += seatAccess == SA_directAccessSeat ? DIRECT_ENTRY_PRIORITY : INDIRECT_ENTRY_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Prefer direct entry priority");

		if (seatAccess == SA_directAccessSeat)
		{
			TUNE_GROUP_INT(VEHICLE_EXIT_TUNE, DIRECT_BOAT_ENTRY_PRIORITY, 4, 0, 10, 1);
			bool bIsBoatExit = isVehicleBoat && !isVehicleJetSki && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsExitingVehicle) && !iFlags.BitSet().IsSet(CVehicleEnterExitFlags::BeJacked);
			iPriority += bIsBoatExit ? DIRECT_BOAT_ENTRY_PRIORITY : DIRECT_ENTRY_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Direct entry priority");
		}
		else
		{
			iPriority += INDIRECT_ENTRY_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Prefer Direct Entry - Indirect entry priority");
		}

	}
	else
	{
		iPriority += NO_PREFERENCE_ENTRY_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "No preference entry priority");
	}

#if __BANK
	if (pPed->IsLocalPlayer() && CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && CVehicleDebug::ms_iSeatRequested == iSelectedSeatID)
	{
		bIsLowerPrioritySeat = false;
	}
#endif // __BANK

	if (bIsLowerPrioritySeat && iPriority > 0 && !CPedType::IsLawEnforcementType(pPed->GetPedType()) && !CPedType::IsEmergencyType(pPed->GetPedType()))
	{
		TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, LOWER_PRIORITY_SEAT_PRIORITY, 1, 0, 5, 1);
		iPriority = LOWER_PRIORITY_SEAT_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Lower priority seat");
	}
	else if (bIsLowerPriorityEntry)
	{
		TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, LOWER_PRIORITY_ENTRY_PRIORITY, 1, 0, 5, 1);
		iPriority = LOWER_PRIORITY_ENTRY_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Lower priority entry");
	}
	else if (bIsTurretSeat)
	{
		// Don't up the priority of turret seats if we're the preferred driver, we should be driving!
		// Only increase it if we have a driver and that driver is friendly
		if( pTargetVehicle && !pTargetVehicle->IsAnExclusiveDriverPed(pPed) &&
			pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PREFER_ENTER_TURRET_AFTER_DRIVER) &&
			pSeatManager->GetDriver() && pSeatManager->GetDriver()->GetPedIntelligence()->IsFriendlyWith(*pPed))
		{
			TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, INCREASE_TURRENT_SEAT_PRIORITY, 4, 0, 10, 1);
			iPriority += INCREASE_TURRENT_SEAT_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Turret seat priority");
		}
	}

	// If we're holding for turret access, bump the priority a little on direct entry so we don't pick a shuffle link to the turret instead
	bool bRequestedTurretSeat = pTargetVehicle->IsTurretSeat(iSeatRequested);
	bool bSelectedTurretSeat = pTargetVehicle->IsTurretSeat(iSelectedSeatID); // Don't trust bIsTurretSeat! That'll be true if a turret seat is on the vehicle at all, regardless of whether it ended up being the selected seat.
	if (bRequestedTurretSeat && bSelectedTurretSeat && iSeatRequest == SR_Prefer && seatAccess == SA_directAccessSeat)
	{
		TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, INCREASE_TURRET_HOLD_DIRECT_ACCESS_PRIORITY, 1, 0, 10, 1);
		iPriority += INCREASE_TURRET_HOLD_DIRECT_ACCESS_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Turret hold direct entry priority");		
	}

	if (bIsBombushka && (iSelectedSeatID == 0 || iSelectedSeatID == 1 || iSelectedSeatID == 2 || iSelectedSeatID == 3))
	{
		TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, INCREASE_BOMBUSHKA_TURRET_SEAT_PRIORITY, 1, 0, 10, 1);
		iPriority += INCREASE_BOMBUSHKA_TURRET_SEAT_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Bombushka driver/turret seat priority");
	}
	else if (bIsVolatol && (iSelectedSeatID == 0 || iSelectedSeatID == 2 || iSelectedSeatID == 3))
	{
		TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, INCREASE_VOLATOL_TURRET_SEAT_PRIORITY, 1, 0, 10, 1);
		iPriority += INCREASE_VOLATOL_TURRET_SEAT_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Volatol driver/turret seat priority");
	}

	// If the ped will prefer this seat above empty ones
	if( bDirectSeatOccupiedByWantedPlayer && iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackWantedPlayersRatherThanStealCar) )
	{
		iPriority += PREFER_JACKING_PRIORITY;
		ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Prefer jacking priority");
	}
	
	// For AI peds we want to lower the priority of the turret seat if they belong to the player group
	// and there is more than one more player. This is because in online the player will expect to have
	// preference over the AI to use the turret
	if( NetworkInterface::IsGameInProgress() && !pPed->IsPlayer() && bIsTurretSeat )
	{
		CPedGroup* pPedsGroup = pPed->GetPedsGroup();
		if( pPedsGroup && pPedsGroup->IsPlayerGroup() )
		{
			const CPedGroupMembership* pPedGroupMembership = pPedsGroup->GetGroupMembership();
			for(s32 iMemberIndex = 0, iPlayersInGroup = 0; iMemberIndex < CPedGroupMembership::MAX_NUM_MEMBERS; ++iMemberIndex)
			{
				const CPed* pPedMember = pPedGroupMembership->GetMember(iMemberIndex);
				if( pPedMember && pPedMember->IsPlayer() )
				{
					++iPlayersInGroup;
					if(iPlayersInGroup > 0)
					{
						// Lower the priority and quit
						TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, DECREASE_TURRENT_SEAT_PRIORITY_FOR_AI, 4, 0, 10, 1);
						iPriority -= DECREASE_TURRENT_SEAT_PRIORITY_FOR_AI;
						break;
					}
				}
			}
		}
	}

	// If the ped has been flagged to lower the priority of the warp seats, increase the priority of non warp seats
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_LowerPriorityOfWarpSeats) && pAnimInfo)
	{
		TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, NON_WARP_SEAT_PRIORITY, 5, 0, 5, 1);
		const bool bNavigateToWarpEntryPoint = pAnimInfo->GetNavigateToWarpEntryPoint();
		if (!bNavigateToWarpEntryPoint)
		{
			iPriority += NON_WARP_SEAT_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Warp seats low priority");
		}
	}

	// Players in MP use the nearest doors rather than prefering the front.
	const bool bPlayerInMPPrefersAnySeat = NetworkInterface::IsGameInProgress() && pPed->IsPlayer() && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayerPreferFrontSeatMP );

	bool bScriptPrefersRearSeats = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_PREFER_REAR_SEATS);
	if (bScriptPrefersRearSeats)
	{
		const CVehicle* pPreferRearSeatVehicle = pPed->GetPlayerInfo()->GetVehiclePlayerPreferRearSeat();
		if (pPreferRearSeatVehicle && pPreferRearSeatVehicle != pTargetVehicle)
		{
			bScriptPrefersRearSeats = false;
		}
	}

	// In custody peds prefer back seats, script can also make players prefer rear seats
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody) || bScriptPrefersRearSeats)
	{
		s32 seat = iSelectedSeatID;
		const CVehicleSeatInfo* pSeatInfo = pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(seat);
		if( !pSeatInfo->GetIsFrontSeat() )
		{
			iPriority += PREFERED_SEAT_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Prefer rear seats, IsInCustody : %s", AILogging::GetBooleanAsString(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody)));
		}
	}

	// Script can set a preference for the front passenger seat of a vehicle as well
	if (pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_PREFER_FRONT_PASSENGER_SEAT))
	{
		const CVehicle* pPreferFrontPassengerSeatVehicle = pPed->GetPlayerInfo()->GetVehiclePlayerPreferFrontPassengerSeat();
		if (!pPreferFrontPassengerSeatVehicle || (pPreferFrontPassengerSeatVehicle && pPreferFrontPassengerSeatVehicle == pTargetVehicle))
		{
			s32 seat = iSelectedSeatID;
			const CVehicleSeatInfo* pSeatInfo = pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(seat);
			if( pSeatInfo->GetIsFrontSeat() && seat == 1 )
			{
				iPriority += PREFERED_SEAT_PRIORITY;
				ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Prefer Front Passenger Seat");
			}			 
		}
	}

	// Prefer front seats if entering as driver, so if the driver seats taken they'll enter the front door.
	if (iSeatRequest == SR_Prefer && iSeatRequested == 0 && !bPlayerInMPPrefersAnySeat)
	{
		// Prefer entry points not in use
		if (!CTaskVehicleFSM::IsAnyGroupMemberUsingEntrypoint(pPed, pTargetVehicle, iEntryPoint, iFlags, true, vEntryPos))
		{
			++iPriority;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Entry not in use");

			s32 seat = iSelectedSeatID;
			if (seat == 0 || seat == 1 )
			{
				++iPriority;
				ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Front entry");
			}
		}
		else if( bPreferredSeat )
		{
			iPriority -= PREFERED_SEAT_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Preferred Seat");
		}
	}
	else if (iSeatRequest == SR_Prefer || iSeatRequest == SR_Any)
	{
		// Prefer entry points not in use
		if (!CTaskVehicleFSM::IsAnyGroupMemberUsingEntrypoint(pPed, pTargetVehicle, iEntryPoint, iFlags, true, vEntryPos))
		{
			++iPriority;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Entry not in use iSeatRequestType = %s", CSeatAccessor::GetSeatRequestTypeString(iSeatRequest));
		}
		else if( bPreferredSeat )
		{
			iPriority -= PREFERED_SEAT_PRIORITY;
			ADD_PRIORITY_SCORING_DEBUG_EVENT(iEntryPoint, iPriority, "Entry in use iSeatRequestType = %s", CSeatAccessor::GetSeatRequestTypeString(iSeatRequest));
		}
	}

	return iPriority;
}

bool CModelSeatInfo::IsPositionClearForPed(const CPed* pPed, const CEntity* pEntity, const Matrix34& mEntryMatrix, const bool bExcludeVehicle, float fStartZOffset, CEntity** pHitEntity)
{
	static const int nTestTypes = ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

	const CEntity* exclusionList[MAX_INDEPENDENT_VEHICLE_ENTITY_LIST];
	int nExclusions = 0;

	pPed->GeneratePhysExclusionList(exclusionList, nExclusions, MAX_NUM_ENTITIES_ATTACHED_TO_PED, nTestTypes,TYPE_FLAGS_ALL);

	// Exclude the ped
	exclusionList[nExclusions++] = pPed;

	if( bExcludeVehicle )
	{
		// Exclude the vehicle we are testing
		exclusionList[nExclusions++] = pEntity;

		// Exclude objects attached to the vehicle also
		pEntity->GeneratePhysExclusionList(exclusionList, nExclusions, MAX_INDEPENDENT_VEHICLE_ENTITY_LIST, nTestTypes, ArchetypeFlags::GTA_OBJECT_TYPE);
	}

	// Shrink the radius because a full ped capsule is sometimes too harsh
	float fCapsuleRadiusMultiplier = CTaskVehicleFSM::GetEntryRadiusMultiplier(*pEntity);

	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()), mEntryMatrix.d);
	probeDesc.SetIncludeFlags(nTestTypes);
	probeDesc.SetExcludeEntities(exclusionList, nExclusions, bExcludeVehicle ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

	// This will prevent WorldProbe::CShapeTestTaskData::ConvertExcludeEntitiesToInstances from adding the vehicle the ped is attached to
	u32 capsuleExcludeEntityOptions = WorldProbe::EIEO_DONT_ADD_VEHICLE | WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS;
	const u32 uTypeFlags = (pEntity->GetIsTypeVehicle() && (static_cast<const CVehicle*>(pEntity)->InheritsFromBike() || static_cast<const CVehicle*>(pEntity)->InheritsFromQuadBike() || static_cast<const CVehicle*>(pEntity)->InheritsFromAmphibiousQuadBike())) ? ArchetypeFlags::GTA_AI_TEST : TYPE_FLAGS_ALL;

	if (CPedGeometryAnalyser::TestPedCapsule(pPed, &mEntryMatrix, exclusionList, nExclusions, capsuleExcludeEntityOptions, nTestTypes, uTypeFlags, TYPE_FLAGS_NONE, pHitEntity, NULL, fCapsuleRadiusMultiplier, 0.0f, 0.0f, VEC3_ZERO, fStartZOffset))
	{
		return false;
	}
	
	WorldProbe::CShapeTestHitPoint capsuleIsect;
	WorldProbe::CShapeTestResults capsuleResult(capsuleIsect);
	probeDesc.SetResultsStructure(&capsuleResult);
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// Make sure we're not colliding with pickups
		if (CPedGeometryAnalyser::TestIfCollisionTypeIsValid(pPed, nTestTypes, capsuleResult))
		{
			if (pHitEntity)
				*pHitEntity = capsuleResult[0].GetHitEntity();

			return false;
		}
	}

	TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, DO_EXTRA_VEHICLE_ONLY_CAPSULE_TEST_FOR_SLOPE, true);
	if (!bExcludeVehicle && DO_EXTRA_VEHICLE_ONLY_CAPSULE_TEST_FOR_SLOPE)
	{
		const float fRoll = Abs(pEntity->GetTransform().GetRoll());
		if (fRoll > CTaskVehicleFSM::ms_Tunables.m_MinRollToDoExtraTest)
		{
			const float fPitch = Abs(pEntity->GetTransform().GetPitch());
			if (fPitch > CTaskVehicleFSM::ms_Tunables.m_MinPitchToDoExtraTest)
			{
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, EXTRA_TEST_Z_OFFSET, 0.5f, 0.0f, 1.0f, 0.01f);
				Matrix34 mTestMtx = mEntryMatrix;
				mTestMtx.d.z -= EXTRA_TEST_Z_OFFSET;
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, EXTRA_TEST_MULTIPLIER, 1.0f, 0.0f, 2.0f, 0.01f);
				if (CPedGeometryAnalyser::TestPedCapsule(pPed, &mTestMtx, NULL, 0, 0, ArchetypeFlags::GTA_VEHICLE_TYPE, TYPE_FLAGS_ALL, TYPE_FLAGS_NONE, pHitEntity, NULL, EXTRA_TEST_MULTIPLIER))
				{
#if __DEV
					const CBipedCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo()->GetBipedCapsuleInfo();
					if(pCapsuleInfo)
					{
						Vec3V vCapsuleBottom = RCC_VEC3V(mTestMtx.d) + RCC_VEC3V(mTestMtx.c) * ScalarVFromF32(pCapsuleInfo->GetHeadHeight() - pCapsuleInfo->GetRadius());
						Vec3V vCapsuleTop = RCC_VEC3V(mTestMtx.d) + RCC_VEC3V(mTestMtx.c) * ScalarVFromF32(pCapsuleInfo->GetCapsuleZOffset() + pCapsuleInfo->GetRadius());
						CTask::ms_debugDraw.AddCapsule(vCapsuleBottom, vCapsuleTop, pCapsuleInfo->GetRadius() * EXTRA_TEST_MULTIPLIER, Color_red, 100, 0, false);
					}
#endif // __DEV
					aiDisplayf("Frame : %i, Ped %s(%p) position not clear due to extra capsule test", fwTimer::GetFrameCount(), pPed->GetModelName(), pPed);
					return false;
				}
			}
		}
	}

	return true;
}

bool CModelSeatInfo::ShouldLowerEntryPriorityDueToTurretAngle(const CPed* pPed, const CVehicleSeatAnimInfo* pSeatAnimInfo, const CVehicle* pTargetVehicle, const CVehicleEntryPointInfo* pEntryPointInfo, const s32 iTargetSeat )
{
	if (pPed->GetGroundPhysical() == pTargetVehicle)
		return false;

	if (!pSeatAnimInfo->IsStandingTurretSeat())
		return false;

	const CVehicleWeaponMgr* pVehWeaponMgr = pTargetVehicle->GetVehicleWeaponMgr();
	if (!pVehWeaponMgr)
		return false;

	
	const CTurret* pTurret = pVehWeaponMgr->GetFirstTurretForSeat(iTargetSeat);
	if (!pTurret)
		return false;

	CVehicleEntryPointInfo::eSide side = pEntryPointInfo->GetVehicleSide();
	const bool bIsRearSide = side == CVehicleEntryPointInfo::SIDE_REAR;
	const float fLocalHeading = -pTurret->GetLocalHeading(*pTargetVehicle);

	if (bIsRearSide)
	{
		TUNE_GROUP_FLOAT(TURRET_TUNE, MAX_REAR_IGNORE_HEADING, 0.785f, -PI, PI, 0.01f);
		// Flip the heading so we're comparing to negative vehicle fwd
		const float fLocalHeadingForRear = fwAngle::LimitRadianAngle(fLocalHeading + PI);
		const float fLocalHeadingMag = Abs(fLocalHeadingForRear);	
		const bool bIsInsideIgnoreRange = fLocalHeadingMag < MAX_REAR_IGNORE_HEADING;
		if (bIsInsideIgnoreRange)
		{
			return true;
		}
	}
	else
	{
		// See if the turret's local heading (relative to the vehicle orientation) is inside the range we should invalid this entry point
		TUNE_GROUP_FLOAT(TURRET_TUNE, MIN_SIDE_IGNORE_HEADING, 0.785f, -PI, PI, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, MAX_SIDE_IGNORE_HEADING, 2.356f, -PI, PI, 0.01f);
		const bool bIsLeftSide = pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT;
		const float fLocalHeadingMag = Abs(fLocalHeading);
		const bool bIsInsideIgnoreRange = fLocalHeadingMag > MIN_SIDE_IGNORE_HEADING && fLocalHeadingMag < MAX_SIDE_IGNORE_HEADING;
		const bool bIsLeftSideBlocked = bIsInsideIgnoreRange && Sign(fLocalHeading) >= 0.0f && bIsLeftSide;
		const bool bIsRightSideBlocked = bIsInsideIgnoreRange && Sign(fLocalHeading) < 0.0f && !bIsLeftSide;

		if (bIsLeftSideBlocked || bIsRightSideBlocked)
		{
			return true;
		}
	}
	
	return false;
}

bool CModelSeatInfo::ShouldLowerEntryPriorityDueToTurretDoorBlock(s32 iSeatIndex, s32 iEntryPoint, const CPed* pPed, const CVehicle* pTargetVehicle)
{
	if (pPed->GetGroundPhysical() != pTargetVehicle)
		return false;

	if (!pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
		return false;

	if (!pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_STOP_WHEN_GOING_TO_CLIMB_UP_POINT))
		return false;

	const CVehicleWeaponMgr* pVehWeaponMgr = pTargetVehicle->GetVehicleWeaponMgr();
	if (!pVehWeaponMgr)
		return false;

	const CTurret* pTurret = pVehWeaponMgr->GetFirstTurretForSeat(iSeatIndex);
	if (!pTurret)
		return false;

	if (!CTaskMotionInTurret::IsSeatWithHatchProtection(iSeatIndex, *pTargetVehicle))
		return false;

	// Check we're at the rear of the vehicle, if not, always allow entry
	const Vector3 vVehPos = VEC3V_TO_VECTOR3(pTargetVehicle->GetTransform().GetPosition());
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vVehFwd = VEC3V_TO_VECTOR3(pTargetVehicle->GetTransform().GetB());
	Vector3 vToVeh = vVehPos - vPedPos;
	vToVeh.NormalizeSafe();
	const float fDot = vVehFwd.Dot(vToVeh);
	if (fDot < 0.0f)
		return false;

	// Always allow entry if we're around the same height or higher than the turret
	TUNE_GROUP_FLOAT(TURRET_TUNE, ALLOW_ENTRY_MAX_DELTA, 0.05f, -2.0f, 2.0f, 0.01f);
	Matrix34 turretMtx;
	pTurret->GetTurretMatrixWorld(turretMtx, pTargetVehicle);

	const float fDeltaZ = vPedPos.z - turretMtx.d.z;
	if (fDeltaZ > ALLOW_ENTRY_MAX_DELTA)
	{
		return false;
	}

	// Prevent entry if turret pointing forwards and door is open
	TUNE_GROUP_FLOAT(TURRET_TUNE, PREVENT_ENTRY_MAX_REAR_IGNORE_HEADING, 0.785f, -PI, PI, 0.01f);
	const float fLocalHeading = -pTurret->GetLocalHeading(*pTargetVehicle);
	const bool bIsWithinForwardRange = Abs(fLocalHeading) < PREVENT_ENTRY_MAX_REAR_IGNORE_HEADING;
	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pTargetVehicle, iEntryPoint);
	const bool bDoorIsOpen = pDoor && pDoor->GetIsIntact(pTargetVehicle) && pDoor->GetIsFullyOpen();
	if (bIsWithinForwardRange && bDoorIsOpen)
	{
		return true;
	}

	// Prevent entry if turret pointing backwards
	const float fLocalHeadingForRear = fwAngle::LimitRadianAngle(fLocalHeading + PI);
	const float fLocalHeadingMag = Abs(fLocalHeadingForRear);	
	const bool bIsWithinBackwardRange = fLocalHeadingMag < PREVENT_ENTRY_MAX_REAR_IGNORE_HEADING;
	if (bIsWithinBackwardRange)
	{
		return true;
	}

	return false;
}

bool CModelSeatInfo::ShouldInvalidateEntryPointDueToOnBoardJack(s32 iSeatIndex, s32 iEntryPoint, const CPed& rPed, const CVehicleSeatAnimInfo* pSeatAnimInfo, const CVehicle& rTargetVehicle, const CVehicleEntryPointInfo* pEntryPointInfo)
{
	if (rPed.GetGroundPhysical() != &rTargetVehicle)
		return false;
	
	bool bIsAATrailer = MI_TRAILER_TRAILERSMALL2.IsValid() && rTargetVehicle.GetModelIndex() == MI_TRAILER_TRAILERSMALL2;
	if (!(pSeatAnimInfo->IsStandingTurretSeat() || bIsAATrailer))
		return false;

	const CVehicleExtraPointsInfo* pExtraVehiclePointsInfo = pEntryPointInfo->GetVehicleExtraPointsInfo();
	if (!pExtraVehiclePointsInfo)
		return true;

	const CExtraVehiclePoint* pExtraVehiclePoint = pExtraVehiclePointsInfo->GetExtraVehiclePointOfType(CExtraVehiclePoint::ON_BOARD_JACK);
	if (!pExtraVehiclePoint)
		return true;

	if (iEntryPoint != rTargetVehicle.ChooseAppropriateEntryPointForOnBoardJack(rPed, iSeatIndex))
		return true;

	return false;
}

bool CModelSeatInfo::ThereIsEnoughRoomToFitPed(const CPed* UNUSED_PARAM(pPed), const CVehicle* pVehicle, s32 iSeat)
{
	// Get the entry point for the given seat
	const s32 iEntryPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);

	// Get the position and orientation of the seat
	Vector3 vSeatPosition; Quaternion qSeatOrientation;
	CModelSeatInfo::CalculateSeatSituation(pVehicle, vSeatPosition, qSeatOrientation, iEntryPoint, iSeat);

	// Iterate through the child objects of the vehicle and see if they are too close
	// to the given seat position, if so, consider the seat to not be accessible
	const CEntity* pAttachedEntity = static_cast<CEntity*>( pVehicle->GetChildAttachment() );
	while(pAttachedEntity)
	{
		if( pAttachedEntity->GetIsTypeObject() && pAttachedEntity->IsVisible())
		{
			const Vector3 vAttachmentPos = VEC3V_TO_VECTOR3( pAttachedEntity->GetTransform().GetPosition() );
			const Vector3 vSeatToAttachment = vAttachmentPos - vSeatPosition;
			const float fSqrdDistFromSeatToAttachment = vSeatToAttachment.Mag2();
			const float fDistThreshold = 1.65f;

			// If the attachment is close to the seat we will consider it unaccessible
			if( fSqrdDistFromSeatToAttachment < fDistThreshold * fDistThreshold )
				return false;
		}

		pAttachedEntity = static_cast<CEntity*>( pAttachedEntity->GetSiblingAttachment() );
	}

	return true;

	/*
	// Get the dimensions of the bounding capsule
	TUNE_GROUP_FLOAT( BARRACKS_TEST, fCapsuleLength, 0.0f, 0.0f, 20.0f, 0.1f );
	TUNE_GROUP_FLOAT( BARRACKS_TEST, fCapsuleRadius, 0.3f, 0.0f, 20.0f, 0.1f );
	// Offsets of the bounding capsule
	TUNE_GROUP_FLOAT( BARRACKS_TEST, fCapsuleVerticalOffset, 0.320f, -10.0f, 10.0f, 0.1f );
	TUNE_GROUP_FLOAT( BARRACKS_TEST, fCapsuleDepthOffset, 0.280f, -10.0f, 10.0f, 0.1f );

	// Half height of the capsule
	float fCapsuleHalfHeight = (fCapsuleLength / 2.0f) + fCapsuleRadius;

	// Get the position and orientation of the seat
	Vector3 vSeatPosition; Quaternion qSeatOrientation;
	CModelSeatInfo::CalculateSeatSituation(pVehicle, vSeatPosition, qSeatOrientation, iEntryPoint, iSeat);

	// Get the front and the up vector of the seat
	Vector3 vSeatFront(0.0f, 1.0f, 0.0f), vSeatUp(0.0f, 0.0f, 1.0f);
	qSeatOrientation.Transform(vSeatFront);
	qSeatOrientation.Transform(vSeatUp);

	// Calculate the position of the capsule based on the position of the seat and the offset applied
	const Vector3 vCapsulePos = vSeatPosition + ( fCapsuleDepthOffset * vSeatFront ) + ( fCapsuleVerticalOffset * vSeatUp );
	// Get the beginning and end points of the capsule to set its size
	const Vector3 vCapsuleStart = vCapsulePos - ( vSeatUp * fCapsuleHalfHeight );
	const Vector3 vCapsuleEnd = vCapsulePos + ( vSeatUp * fCapsuleHalfHeight );

	// Set the probe info
	WorldProbe::CShapeTestCapsuleDesc shapeTestDesc;
	// Set the results structure
	WorldProbe::CShapeTestFixedResults<5> results;
	shapeTestDesc.SetResultsStructure(&results);
	// Set the capsule size
	shapeTestDesc.SetCapsule(vCapsuleStart, vCapsuleEnd, fCapsuleRadius);
	// Set the entities to exclude (we are not interested in the ped or the vehicle)
	//const u32 uExceptions = 2;
	//const CEntity* ppExceptions[uExceptions] = { pPed, pVehicle };
	//shapeTestDesc.SetExcludeEntities(ppExceptions, uExceptions);
	// Set the collision flags
	shapeTestDesc.SetIncludeFlags( ArchetypeFlags::GTA_PED_INCLUDE_TYPES	| 
								   ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES	| 
								   ArchetypeFlags::GTA_VEH_SEAT_INCLUDE_FLAGS );
	shapeTestDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	// This is a sweep test
	shapeTestDesc.SetIsDirected(true);
	// There might be collision at the beginning of the sweep
	shapeTestDesc.SetDoInitialSphereCheck(true);

	// Submit the test and see if there are any collisions detected
	bool bSeatIsBlocked = false;
	if( WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST) )
	{
		for(WorldProbe::ResultIterator it = results.begin(); it < results.end(); ++it)
		{
			const CEntity* pEntityHit = it->GetHitEntity();
			// If we have a collision with an object attached to the vehicle
			// check to see if its bounding box is big enough
			if( pEntityHit != NULL && pEntityHit->GetAttachParent() == pVehicle )
			{
				bSeatIsBlocked = true;
			}
		}
	}

#if DEBUG_DRAW
	// Draw the capsule to see what's going on
	TUNE_GROUP_BOOL( BARRACKS_TEST, bShowCapsuleTest, true );
	if(bShowCapsuleTest)
	{
		// Change the color value depending on hits
		const Color32 colour = bSeatIsBlocked ? Color_red : Color_green;

		// Draw the bounding capsule
		grcDebugDraw::Capsule( VECTOR3_TO_VEC3V(vCapsuleStart), VECTOR3_TO_VEC3V(vCapsuleEnd), fCapsuleRadius, colour, false );
	}
#endif

	// It's safe to play the fidgets if there is nothing around
	return !bSeatIsBlocked;*/
}

//////////////////////////////////////////////////////////////////////////
// CSeatManager
//////////////////////////////////////////////////////////////////////////
CSeatManager::CSeatManager() {
	for(int i = 0; i < MAX_VEHICLE_SEATS; i++)
	{
		m_pSeatOccupiers[i] = NULL;
		m_pLastSeatOccupiers[i] = NULL;
	}
	m_iNoSeats=MAX_VEHICLE_SEATS;
	m_iDriverSeat=0;
	m_nNumScheduledOccupants = 0;
	m_bHasEverHadOccupant = false;
	m_pLastDriverFromNetwork = NULL;
}

CPed* CSeatManager::GetLastDriver() const
{
	CPed *pLastDriver = NULL;
	// Since this is inlined now, using (u32) trick to eliminate
	// a branch.  (a >= 0 && a < b) equivalent to ((u32)a < (u32)b).
	if((u32)m_iDriverSeat < (u32)GetMaxSeats())
	{
		pLastDriver = m_pLastSeatOccupiers[m_iDriverSeat];
	}

	if(!pLastDriver)
	{
		pLastDriver = m_pLastDriverFromNetwork;
	}

	// if a vehicle has no driver seat, return NULL (no driver)
	return pLastDriver;
}


CPed* CSeatManager::GetPedInSeat( s32 iIndex ) const
{
	Assert(iIndex>=0&&iIndex<MAX_VEHICLE_SEATS); 
	if (iIndex>=0&&iIndex<MAX_VEHICLE_SEATS)
		return m_pSeatOccupiers[iIndex];
	else
		return NULL;
}

CPed* CSeatManager::GetLastPedInSeat( s32 iIndex ) const
{
	Assert(iIndex>=0&&iIndex<MAX_VEHICLE_SEATS); 
	if (iIndex>=0&&iIndex<MAX_VEHICLE_SEATS)
	{
		if(iIndex == m_iDriverSeat)
		{
			return GetLastDriver();
		}
		else
		{
			return m_pLastSeatOccupiers[iIndex];
		}
	}
	
	return NULL;
}

s32 CSeatManager::GetPedsSeatIndex( const CPed* pPed ) const
{
	for( s32 i = 0; i < GetMaxSeats(); i++ )
	{
		if( pPed == m_pSeatOccupiers[i] )
		{
			return i;
		}
	}
	return -1;
}

s32 CSeatManager::GetLastPedsSeatIndex( const CPed* pPed ) const
{
	for( s32 i = 0; i < GetMaxSeats(); i++ )
	{
		if( pPed == m_pLastSeatOccupiers[i] )
		{
			return i;
		}
	}

	if( pPed == m_pLastDriverFromNetwork )
	{
		return m_iDriverSeat;
	}

	return -1;
}

int CSeatManager::CountPedsInSeats(bool bIncludeDriversSeat, bool bIncludeDeadPeds, bool bIncludePedsUsingSeat, const CVehicle* pVeh, int iBossIDToIgnoreSameOrgPeds) const
{
	int iNoPeds  = 0;

	for(int iSeat = 0; iSeat < GetMaxSeats(); iSeat++)
	{
		if( bIncludeDriversSeat || iSeat != m_iDriverSeat )
		{
			CPed *pPed = (bIncludePedsUsingSeat && pVeh) ? CTaskVehicleFSM::GetPedInOrUsingSeat(*pVeh, iSeat) : GetPedInSeat(iSeat);

			if(pPed)
			{				
				Assert(bIncludePedsUsingSeat || pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ));

				bool bCountPed = true;

				if (iBossIDToIgnoreSameOrgPeds != -1 && iBossIDToIgnoreSameOrgPeds == pPed->GetBossID())
				{
					bCountPed = false;
				}

				if (!bIncludeDeadPeds && pPed->ShouldBeDead())
				{
					bCountPed = false;
				}

				if (bCountPed)
				{
					iNoPeds++;
				}
			}
		}
	}

	return iNoPeds;
}

int CSeatManager::CountFreeSeats(bool bIncludeDriversSeat, bool bCountPedsWeCanJack, CPed* pPedWantingToEnter) const
{
	aiAssertf(!bCountPedsWeCanJack || pPedWantingToEnter, "bCountPedsWeCanJack set to true, but no ped has been passed in");
	int iNoFreeSeats  = 0;

	for(int iSeat = 0; iSeat < GetMaxSeats(); iSeat++)
	{
		if( bIncludeDriversSeat || iSeat != m_iDriverSeat )
		{
			CPed *pPed = GetPedInSeat(iSeat);

			if(pPed == NULL || (bCountPedsWeCanJack && pPedWantingToEnter && CTaskVehicleFSM::CanJackPed(pPedWantingToEnter, pPed)))
			{
				iNoFreeSeats++;
			}
		}
	}

	return iNoFreeSeats;
}

bool CSeatManager::AddPedInSeat(CPed *pPed, s32 iSeat)
{
	Assert(pPed);
	if((iSeat < GetMaxSeats()) && (!GetPedInSeat(iSeat)))
	{
		m_pSeatOccupiers[iSeat]=pPed;
		m_bHasEverHadOccupant = true;
		if(pPed->IsPlayer())
		{
			m_nNumPlayers++;
		}
		return true;
	}	
	return false;
}

s32 CSeatManager::RemovePedFromSeat(CPed *pPed)
{
	for(int i=0; i< GetMaxSeats(); i++)
	{
		if (m_pSeatOccupiers[i]==pPed)
		{			
			aiAssertf(m_pSeatOccupiers[i]->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) == false, "RemovePedFromSeat: Only unsets the pointer, ped must have its local variables cleared elsewhere!");
			m_pLastSeatOccupiers[i] = pPed;
			m_pSeatOccupiers[i]=NULL;
			if(pPed->IsPlayer())
			{
				Assertf(m_nNumPlayers>0,"Removing player from a CSeatManager that didn't think it had any players.");
				m_nNumPlayers--;
			}
			return i;
		}
	}
	return -1;
}

void CSeatManager::SetLastPedInSeat(CPed* pPed, s32 iSeat)
{
	m_pLastSeatOccupiers[iSeat] = pPed;
}

void CSeatManager::SetLastDriverFromNetwork(CPed* pPed)
{
	m_pLastDriverFromNetwork = pPed;
}

void CSeatManager::ChangePedsPlayerStatus(const CPed &pPed, bool bBecomingPlayer)
{
#if __ASSERT
	bool bPedFound = false;
#endif
	for(int iSeat=0; iSeat<GetMaxSeats(); iSeat++)
	{
		if(m_pSeatOccupiers[iSeat]==&pPed)
		{
			Assertf(!bPedFound,"Ped found twice in CSeatManager.");
#if __ASSERT
			bPedFound = true;
#endif
			if(bBecomingPlayer)
			{
				Assertf(m_nNumPlayers<GetMaxSeats(),"Trying to convert a ped into a player in a CSeatManager that is already full of players.");
				m_nNumPlayers++;
			}
			else
			{
				Assertf(m_nNumPlayers>0,"Trying to convert a ped from a player in a CSeatManager that doesn't have any players.");
				m_nNumPlayers--;
			}
		}
	}
}

