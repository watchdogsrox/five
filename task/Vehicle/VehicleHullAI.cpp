#include "VehicleHullAI.h"

// Rage headers
#include "phbound/boundcomposite.h" 
#include "fragment/typegroup.h"
#include "vector/geometry.h"

// Framework headers
#include "fwmaths/Angle.h"

// Game headers
#include "Control/Route.h"
#include "Game/ModelIndices.h"
#include "Modelinfo/VehicleModelInfo.h"
#include "Modelinfo/VehicleModelInfoVariation.h"
#include "Modelinfo/VehicleModelInfoMods.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#if __BANK
#include "Peds/PlayerInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#endif
#include "Vehicles/Heli.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehicleGadgets.h"

int CConvexHullCalculator2D::ms_iNumVertices = 0;
CConvexHullCalculator2D::THullVert * CConvexHullCalculator2D::ms_pFirstHullVert = NULL;
CConvexHullCalculator2D::THullVert	CConvexHullCalculator2D::ms_HullVertices[ms_iMaxNumInputVertices];
int CConvexHullCalculator2D::ms_iNumFinalVerts = 0;
Vector2 CConvexHullCalculator2D::ms_FinalVerts[ms_iMaxNumHullVertices];

const CVehicle * CVehicleHullCalculator::m_pVehicle = NULL;
float CVehicleHullCalculator::m_fCullMinZ = -FLT_MAX;
float CVehicleHullCalculator::m_fCullMaxZ = FLT_MAX;
int CVehicleHullCalculator::ms_iNumComponents = 0;
eHierarchyId CVehicleHullCalculator::ms_iComponents[ms_iMaxNumComponents];

AI_OPTIMISATIONS()

//***********************************************************
//	CConvexHullCalculator2D
//

CConvexHullCalculator2D::CConvexHullCalculator2D()
{

}
CConvexHullCalculator2D::~CConvexHullCalculator2D()
{

}

void CConvexHullCalculator2D::Reset()
{
	ms_iNumVertices = 0;
	ms_iNumFinalVerts = 0;
}

bool CConvexHullCalculator2D::AddVertices(const Vector2 * pVerts, const int iNumVerts)
{
	if(ms_iNumVertices + iNumVerts > ms_iMaxNumInputVertices)
		return false;

	int v;
	for(v=0; v<iNumVerts; v++)
	{
		ms_HullVertices[ms_iNumVertices].m_vVert = pVerts[v];
		ms_HullVertices[ms_iNumVertices].m_fAngle = 0.0f;
		ms_HullVertices[ms_iNumVertices].m_bIsInHull = false;
		ms_iNumVertices++;
	}
	return true;
}

bool CConvexHullCalculator2D::AddVertices(const Vector3 * pVerts, const int iNumVerts)
{
	if(ms_iNumVertices + iNumVerts > ms_iMaxNumInputVertices)
		return false;

	int v;
	for(v=0; v<iNumVerts; v++)
	{
		ms_HullVertices[ms_iNumVertices].m_vVert = Vector2(pVerts[v].x, pVerts[v].y);
		ms_HullVertices[ms_iNumVertices].m_fAngle = 0.0f;
		ms_HullVertices[ms_iNumVertices].m_bIsInHull = false;
		ms_iNumVertices++;
	}
	return true;
}

void CConvexHullCalculator2D::Calculate()
{
	ConstructHull();
}

int CConvexHullCalculator2D::FindMinVertex()
{
	//********************************************************************
	// Find the starting vertex 'P' which has the lowest Y value.
	// If there is any conflict chose the one with the lowest X as well.

	int v;
	int iStartVertex = -1;
	Vector2 vStartVertex(FLT_MAX, FLT_MAX);

	for(v=0; v<ms_iNumVertices; v++)
	{
		// Lower Y?
		if(ms_HullVertices[v].m_vVert.y < vStartVertex.y)
		{
			vStartVertex = ms_HullVertices[v].m_vVert;
			iStartVertex = v;
		}
		// If same Y, then select the one with the lower X
		else if(ms_HullVertices[v].m_vVert.y == vStartVertex.y)
		{
			if(ms_HullVertices[v].m_vVert.x < vStartVertex.x)
			{
				vStartVertex = ms_HullVertices[v].m_vVert;
				iStartVertex = v;
			}
		}
	}

	return iStartVertex;
}

void CConvexHullCalculator2D::ConstructHull()
{
	if(ms_iNumVertices < 4)
		return;

	int iStartVertex = FindMinVertex();
	Assert(iStartVertex != -1);
	if(iStartVertex==-1)
		return;

	ms_pFirstHullVert = &ms_HullVertices[iStartVertex];
	ms_pFirstHullVert->m_bIsInHull = true;

	Vector2 vLastHullEdgeVec(1.0f, 0.0f);
	THullVert * pLastHullEdgeVert = ms_pFirstHullVert;
	THullVert * pMostColinearVert = NULL;
	Vector2 vBestAngleEdgeVec;

	ms_FinalVerts[0] = ms_pFirstHullVert->m_vVert;
	ms_iNumFinalVerts = 1;

	bool bHullClosed = false;

	static dev_float fEqThreshold = 0.0f;
	while(!bHullClosed)
	{
		float fMaxAngleSoFar = -1.0f;

		for(int v=0; v<ms_iNumVertices; v++)
		{
			if(!ms_HullVertices[v].m_bIsInHull || &ms_HullVertices[v]==ms_pFirstHullVert)
			{
				Vector2 vToVertex = ms_HullVertices[v].m_vVert - pLastHullEdgeVert->m_vVert;
				if(vToVertex.Mag2() > fEqThreshold)
				{
					vToVertex.Normalize();
					ms_HullVertices[v].m_fAngle = DotProduct(vToVertex, vLastHullEdgeVec);

					if(ms_HullVertices[v].m_fAngle > fMaxAngleSoFar)
					{
						fMaxAngleSoFar = ms_HullVertices[v].m_fAngle;
						pMostColinearVert = &ms_HullVertices[v];
						vBestAngleEdgeVec = vToVertex;
					}
				}
			}
		}

		Assert(pMostColinearVert);
		if(pMostColinearVert==ms_pFirstHullVert)
			break;

		if(ms_iNumFinalVerts >= ms_iMaxNumHullVertices)
		{
			Assertf(false, "Convex hull wasn't closed - ran out of points.");
			return;
		}

		ms_FinalVerts[ms_iNumFinalVerts++] = pMostColinearVert->m_vVert;
		
		pMostColinearVert->m_bIsInHull = true;
		vLastHullEdgeVec = vBestAngleEdgeVec;
		pLastHullEdgeVert = pMostColinearVert;
	}
}


//***********************************************************
//	CVehicleHullCalculator
//

CVehicleHullCalculator::CVehicleHullCalculator()
{
	ms_iNumComponents = 0;
	m_pVehicle = NULL;
}
CVehicleHullCalculator::~CVehicleHullCalculator()
{

}

void CVehicleHullCalculator::Reset()
{
	ms_iNumComponents = 0;
	m_pVehicle = NULL;
}

// If 'pExcludeIDs' is specified then it is expected to be an array of hierarcy IDs, terminated by VEH_INVALID_ID
void CVehicleHullCalculator::CalculateHullForVehicle(const CVehicle * pVehicle, CVehicleHullAI * pVehicleHull, const float fExpandSize, eHierarchyId * pExcludeIDs, const float fCullMinZ, const float fCullMaxZ)
{
	m_fCullMinZ = fCullMinZ;
	m_fCullMaxZ = fCullMaxZ;

	Reset();

	m_pVehicle = pVehicle;

	switch(pVehicle->GetVehicleType())
	{
		case VEHICLE_TYPE_CAR:
        case VEHICLE_TYPE_QUADBIKE:
		{
			GetComponentsForCar(*pVehicle);
			break;
		}
		case VEHICLE_TYPE_BIKE:
		{
			GetComponentsForBike();
			break;
		}
		case VEHICLE_TYPE_HELI:
		case VEHICLE_TYPE_AUTOGYRO:
		case VEHICLE_TYPE_BLIMP:
		{
			GetComponentsForHeli();
			break;
		}
		case VEHICLE_TYPE_PLANE:
		{
			GetComponentsForPlane();
			break;
		}
		default:
			break;
	}

	//-----------------------------------------------------------
	// HACK: Generate hardcoded exclude list for specific models

	eHierarchyId specificModelExcludeIDs[8];
	s32 iNumSpecificModelExludeIDs = GenerateExcludeListForSpecificModel(pVehicle, specificModelExcludeIDs, fCullMinZ, fCullMaxZ);


	//-------------------------------------
	// Add components to the convex hull

	CConvexHullCalculator2D::Reset();

	for(int c=0; c<ms_iNumComponents; c++)
	{
		//--------------------------------------------------------------------------
		// Check if this component is in our optional exclude list, and skip if so..

		if(pExcludeIDs)
		{
			bool bSkipComponent = false;
			eHierarchyId * pExclude = pExcludeIDs;
			while(*pExclude != VEH_INVALID_ID)
			{
				if(*pExclude == ms_iComponents[c])
				{
					bSkipComponent = true;
					break;
				}
				pExclude++;
			}
			if(bSkipComponent)
				continue;
		}

		//-------------------------------------------------------------------------------------
		// Check if this component is in exclude list for this specific model, and skip if so..

		if(iNumSpecificModelExludeIDs)
		{
			bool bSkipComponent = false;
			for(s32 e=0; e<iNumSpecificModelExludeIDs; e++)
			{
				if(specificModelExcludeIDs[e] == ms_iComponents[c])
				{
					bSkipComponent = true;
					break;
				}
			}
			if(bSkipComponent)
				continue;
		}

		//-----------------------------------------------
		// Add vertices for this component to the hull

		if(!Create2dOrientatedBoxesForComponentGroup(ms_iComponents[c], fExpandSize))
		{
			break;
		}
	}

	//-----------------------------------------------------
	// HACK: Generate hardcoded bounds for specific models

	Create2dOrientatedBoxesFoSpecificModels(pVehicle, fExpandSize);

	// Might need this later on?
	//Create2dOrientatedBoxesForGadgets(pVehicle, fExpandSize);

	if(CConvexHullCalculator2D::GetNumInputVerts()==0)
	{
		//Assertf(CConvexHullCalculator2D::GetNumInputVerts(), "Vehicle \"%s\" didn't appear to have a VEH_CHASSIS component.", CModelInfo::GetBaseModelInfoName(pVehicle->GetModelIndex()));

		// Emergency fallback to using the CEntityBoundAI for vehicles with no VEH_CHASSIS.
		Vector3 vTempVerts[4];
		CEntityBoundAI aiBound(*pVehicle, pVehicle->GetTransform().GetPosition().GetZf(), fExpandSize);
		aiBound.GetCorners(vTempVerts);
		CConvexHullCalculator2D::AddVertices(vTempVerts, 4);
	}

	CConvexHullCalculator2D::Calculate();

	if(CConvexHullCalculator2D::GetNumHullVerts() >= 4)
	{
		// Set up the pVehicleHull
		pVehicleHull->m_iNumVerts = CConvexHullCalculator2D::GetNumHullVerts();
		Assert(pVehicleHull->m_iNumVerts < CConvexHullCalculator2D::ms_iMaxNumHullVertices);

		if(pVehicleHull->m_iNumVerts)
		{
			Vector2 vCentre(0.0f, 0.0f);
			for(int v=0; v<CConvexHullCalculator2D::GetNumHullVerts(); v++)
			{
				pVehicleHull->m_vHullVerts[v] = CConvexHullCalculator2D::GetHullVert(v);
				vCentre += pVehicleHull->m_vHullVerts[v];
			}
			vCentre /= (float)pVehicleHull->m_iNumVerts;
			pVehicleHull->m_vCentre = vCentre;
		}
	}
	else
	{
		const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		pVehicleHull->m_vCentre = Vector2(vVehiclePosition.x, vVehiclePosition.y);
	}
}


void CVehicleHullCalculator::GetComponentsForCar(const CVehicle& rVeh)
{
	// Adding these first to fix B* 1024076, maybe the vehicle should be optimized but it is huge after all
	ms_iComponents[ms_iNumComponents++] = VEH_WHEEL_LR;
	ms_iComponents[ms_iNumComponents++] = VEH_WHEEL_RR;

	ms_iComponents[ms_iNumComponents++] = VEH_CHASSIS;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_DSIDE_F;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_PSIDE_F;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_DSIDE_R;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_PSIDE_R;

	ms_iComponents[ms_iNumComponents++] = VEH_BUMPER_F;
	ms_iComponents[ms_iNumComponents++] = VEH_BUMPER_R;
	ms_iComponents[ms_iNumComponents++] = VEH_BONNET;
	ms_iComponents[ms_iNumComponents++] = VEH_BOOT;

	ms_iComponents[ms_iNumComponents++] = VEH_EXTRA_1;	// extra_1 & extra_2 required for two different quad-bike
	ms_iComponents[ms_iNumComponents++] = VEH_EXTRA_2;	// chassis which are combined in one model.

	ms_iComponents[ms_iNumComponents++] = VEH_ARTICULATED_DIGGER_ARM_BUCKET;	// bulldozer
	ms_iComponents[ms_iNumComponents++] = VEH_PICKUP_REEL;						// combine
	ms_iComponents[ms_iNumComponents++] = VEH_AUGER;							// combine

	ms_iComponents[ms_iNumComponents++] = VEH_WHEEL_LF;	// wheels need added for avoidance of large-wheeled autos, such as the monster truck
	ms_iComponents[ms_iNumComponents++] = VEH_WHEEL_RF;

	ms_iComponents[ms_iNumComponents++] = VEH_FORK_LEFT;
	ms_iComponents[ms_iNumComponents++] = VEH_FORK_RIGHT;

	ms_iComponents[ms_iNumComponents++] = VEH_RAMMING_SCOOP;

	// Auto add visible mod components
	// JB: Was previously done only for the "ratloader" (MI_CAR_RATLOADER), but I've enabled this for all vehicles following bugs about mods (eg. url:bugstar:2091938)

	const CVehicleVariationInstance& rVariation = rVeh.GetVariationInstance();
	for (s32 i=0; i<VMT_RENDERABLE; ++i)
	{
		if (rVariation.GetMods()[i] != INVALID_MOD)
		{
			const u8 modIndex = rVariation.GetModIndex((eVehicleModType)i);
			if (modIndex != INVALID_MOD)
			{
				if (const CVehicleKit* pVehicleKit = rVariation.GetKit())
				{
					const CVehicleModVisible& vehicleModVisible = pVehicleKit->GetVisibleMods()[modIndex];
					const eHierarchyId hierarchyId = (eHierarchyId)vehicleModVisible.GetCollisionBone();
					if (hierarchyId != VEH_INVALID_ID)
					{
						if (aiVerifyf(ms_iNumComponents < ms_iMaxNumComponents, "Too many components added to vehicle %s", rVeh.GetModelName()))
						{
							bool bAlreadyExists = false;
							for (s32 j=0; j<ms_iNumComponents; ++j)
							{
								if (ms_iComponents[j] == hierarchyId)
								{
									bAlreadyExists = true;
									break;
								}
							}
							if (!bAlreadyExists)
							{
								ms_iComponents[ms_iNumComponents++] = hierarchyId;
							}
						}
					}
				}
			}
		}
	}
}

void CVehicleHullCalculator::GetComponentsForBike()
{
	ms_iComponents[ms_iNumComponents++] = VEH_CHASSIS;
	ms_iComponents[ms_iNumComponents++] = BIKE_WHEEL_F;
	ms_iComponents[ms_iNumComponents++] = BIKE_WHEEL_R;
}

void CVehicleHullCalculator::GetComponentsForHeli()
{
	ms_iComponents[ms_iNumComponents++] = VEH_CHASSIS;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_DSIDE_F;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_PSIDE_F;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_DSIDE_R;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_PSIDE_R;
	ms_iComponents[ms_iNumComponents++] = VEH_WEAPON_2A;
	ms_iComponents[ms_iNumComponents++] = VEH_TURRET_2_BARREL;
	ms_iComponents[ms_iNumComponents++] = VEH_TURRET_2_BASE;
	ms_iComponents[ms_iNumComponents++] = VEH_WEAPON_3A;
	ms_iComponents[ms_iNumComponents++] = VEH_TURRET_3_BARREL;
	ms_iComponents[ms_iNumComponents++] = VEH_TURRET_3_BASE;
	ms_iComponents[ms_iNumComponents++] = HELI_ROTOR_MAIN;
	ms_iComponents[ms_iNumComponents++] = HELI_ROTOR_REAR;
	ms_iComponents[ms_iNumComponents++] = HELI_RUDDER;
	ms_iComponents[ms_iNumComponents++] = HELI_ELEVATORS;
	ms_iComponents[ms_iNumComponents++] = HELI_TAIL;
	ms_iComponents[ms_iNumComponents++] = HELI_OUTRIGGERS_L;
    ms_iComponents[ms_iNumComponents++] = HELI_OUTRIGGERS_R;
}

void CVehicleHullCalculator::GetComponentsForPlane()
{
	ms_iComponents[ms_iNumComponents++] = VEH_CHASSIS;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_DSIDE_F;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_PSIDE_F;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_DSIDE_R;
	ms_iComponents[ms_iNumComponents++] = VEH_DOOR_PSIDE_R;
	ms_iComponents[ms_iNumComponents++] = PLANE_PROP_1;
	ms_iComponents[ms_iNumComponents++] = PLANE_PROP_2;
	ms_iComponents[ms_iNumComponents++] = PLANE_PROP_3;
	ms_iComponents[ms_iNumComponents++] = PLANE_PROP_4;
	ms_iComponents[ms_iNumComponents++] = PLANE_WING_L;
	ms_iComponents[ms_iNumComponents++] = PLANE_WING_R;
	ms_iComponents[ms_iNumComponents++] = PLANE_RUDDER;
}

s32 CVehicleHullCalculator::GenerateExcludeListForSpecificModel(const CVehicle * pVehicle, eHierarchyId * pExcludeIDs, const float UNUSED_PARAM(fCullMinZ), const float fCullMaxZ)
{
	s32 iNum = 0;

	if(pVehicle->InheritsFromHeli() && IsVehicleLandedOnBlimp(pVehicle))
	{
		pExcludeIDs[iNum++] = HELI_ROTOR_MAIN;

		// For twin rotor chinook-style helis we must also exclude the rear rotor
		if(((CHeli*)pVehicle)->GetIsCargobob())
		{
			pExcludeIDs[iNum++] = HELI_ROTOR_REAR;
		}
	}
	else if(pVehicle->GetModelIndex()==MI_PLANE_TITAN.GetModelIndex())
	{
		pExcludeIDs[iNum++] = VEH_CHASSIS;
	}
	else if(pVehicle->GetModelIndex()==MI_PLANE_CARGOPLANE.GetModelIndex())
	{
		pExcludeIDs[iNum++] = VEH_CHASSIS;
	}
	else if(pVehicle->GetModelIndex()==MI_PLANE_MAMMATUS.GetModelIndex())
	{
		// Special case here.
		// IFF the player is standing under the wing, and the plane is more or less upright - then
		// ignore the wings.  Otherwise add them as usual.
		static dev_float fThresholdZ = -2.0f;
		if(pVehicle->GetTransform().GetUp().GetZf() > 0.8f && pVehicle->GetTransform().GetPosition().GetZf() - fCullMaxZ > fThresholdZ)
		{
			pExcludeIDs[iNum++] = PLANE_WING_L;
			pExcludeIDs[iNum++] = PLANE_WING_R;
		}
	}
	else if(pVehicle->GetModelIndex()==MI_CAR_ZTYPE.GetModelIndex())
	{
		pExcludeIDs[iNum++] = VEH_CHASSIS;
		pExcludeIDs[iNum++] = VEH_BOOT;
	}
	else if(pVehicle->GetModelIndex()==MI_PLANE_TULA.GetModelIndex() && pVehicle->GetIsInWater())
	{
		pExcludeIDs[iNum++] = PLANE_WING_L;
		pExcludeIDs[iNum++] = PLANE_WING_R;
		pExcludeIDs[iNum++] = PLANE_PROP_1;
		pExcludeIDs[iNum++] = PLANE_PROP_2;
		pExcludeIDs[iNum++] = PLANE_PROP_3;
		pExcludeIDs[iNum++] = PLANE_PROP_4;
	}
	else if (pVehicle->GetModelIndex()==MI_PLANE_AVENGER.GetModelIndex() && static_cast<const CPlane*>(pVehicle)->IsInVerticalFlightMode())
	{
		pExcludeIDs[iNum++] = PLANE_PROP_1;
		pExcludeIDs[iNum++] = PLANE_PROP_2;
		pExcludeIDs[iNum++] = PLANE_PROP_3;
		pExcludeIDs[iNum++] = PLANE_PROP_4;
	}
	else if( pVehicle->GetModelIndex() == MI_HELI_AKULA.GetModelIndex() )
	{
		pExcludeIDs[iNum++] = HELI_ROTOR_MAIN;
	}

	return iNum;
}

bool CVehicleHullCalculator::Create2dOrientatedBoxesForGadgets(const CVehicle* pVehicle, float fExpandSize)
{
	const Vector3 vExpand(fExpandSize,fExpandSize,fExpandSize);

	const s32 numVehicleGadgets = pVehicle->GetNumberOfVehicleGadgets();
	for(s32 gadgetIndex=0; gadgetIndex<numVehicleGadgets; gadgetIndex++)
	{
		const CVehicleGadget* vehicleGadget = pVehicle->GetVehicleGadget(gadgetIndex);

		if(vehicleGadget)
		{
			if(vehicleGadget->GetType() == VGT_FORKS)
			{
				const atArray<CPhysical*>& attachedObjects = static_cast<const CVehicleGadgetForks*>(vehicleGadget)->GetListOfAttachedObjects();
				const s32 numAttachedObjects = attachedObjects.GetCount();
				for(s32 objectIndex=0; objectIndex<numAttachedObjects; objectIndex++)
				{
					const CPhysical* attachedObject = attachedObjects[objectIndex];
					if(attachedObject)
					{
						const Matrix34 mSubBound = MAT34V_TO_MATRIX34(attachedObject->GetMatrix());
						const Vector3 vMin = attachedObject->GetBoundingBoxMin() - vExpand;
						const Vector3 vMax = attachedObject->GetBoundingBoxMax() + vExpand;

						Vector2 vVerts[4];
						ProjectBoundingBoxOntoZPlane(vMin, vMax, mSubBound, vVerts);

						if(!CConvexHullCalculator2D::AddVertices(vVerts, 4))
							return false;

#if __BANK && DEBUG_DRAW
						if(CVehicleComponentVisualiser::m_bDrawConvexHull &&
							CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff)
						{
							Vector3 v0(vVerts[3].x, vVerts[3].y, m_pVehicle->GetTransform().GetPosition().GetZf() - 0.2f);
							for(int i=0; i<4; i++)
							{
								Vector3 v1(vVerts[i].x, vVerts[i].y, m_pVehicle->GetTransform().GetPosition().GetZf() - 0.2f);
								grcDebugDraw::Line(v0, v1, Color32(0x60,0x60,0,0xff));
								v0 = v1;
							}
						}
#endif
					}
				}
			}
		}
	}

	return true;
}

bool CVehicleHullCalculator::ShouldSubBoundBeCulled(const Vector3 & vMin, const Vector3 & vMax, const Matrix34 & mSubBound)
{
	Matrix34 mat;
	mat.Dot(mSubBound, MAT34V_TO_MATRIX34(m_pVehicle->GetTransform().GetMatrix()) );
	float fMinZ = FLT_MAX;
	float fMaxZ = -FLT_MAX;
	const Vector3 vCorners[8] =
	{
		Vector3( vMin.x, vMin.y, vMin.z ),
		Vector3( vMax.x, vMin.y, vMin.z ),
		Vector3( vMin.x, vMax.y, vMin.z ),
		Vector3( vMax.x, vMax.y, vMin.z ),
		Vector3( vMin.x, vMin.y, vMax.z ),
		Vector3( vMax.x, vMin.y, vMax.z ),
		Vector3( vMin.x, vMax.y, vMax.z ),
		Vector3( vMax.x, vMax.y, vMax.z )
	};
	for(s32 v=0; v<8; v++)
	{
		Vector3 vWldPos;
		mat.Transform( vCorners[v], vWldPos );
		fMinZ = Min(fMinZ, vWldPos.z);
		fMaxZ = Max(fMaxZ, vWldPos.z);
	}

	const bool bCulled = (fMaxZ < m_fCullMinZ || fMinZ > m_fCullMaxZ);

#if __BANK && DEBUG_DRAW
	if(CVehicleComponentVisualiser::m_bDebugDrawBoxTests)
	{
		grcDebugDraw::BoxOriented(RCC_VEC3V(vMin), RCC_VEC3V(vMax), RCC_MAT34V(mat), bCulled ? Color_red3 : Color_yellow, false);
	}
#endif

	return bCulled;
}

bool CVehicleHullCalculator::IsVehicleLandedOnBlimp(const CVehicle * pVehicle)
{
	for(s32 w=0; w<pVehicle->GetNumWheels(); w++)
	{
		const CWheel * pWheel = pVehicle->GetWheel(w);
		if(pWheel && pWheel->GetHitPhysical() && pWheel->GetHitPhysical()->GetIsTypeVehicle() && ((CVehicle*)pWheel->GetHitPhysical())->InheritsFromBlimp() )
		{
			return true;
		}
	}
	return false;
}

bool CVehicleHullCalculator::Create2dOrientatedBoxesForComponentGroup(const eHierarchyId eComponent, const float fExpandSize)
{
	if(m_pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType()!=phBound::COMPOSITE)
		return false;

	const bool bCullSubBounds =
		(m_pVehicle->InheritsFromHeli() && !IsVehicleLandedOnBlimp(m_pVehicle)) ||
		m_pVehicle->GetModelIndex()==MI_PLANE_TITAN.GetModelIndex() ||
		m_pVehicle->GetModelIndex()==MI_PLANE_CARGOPLANE.GetModelIndex();

		//(m_pVehicle->InheritsFromPlane() || m_pVehicle->InheritsFromBoat());

	const Vector3 vExpand(fExpandSize,fExpandSize,fExpandSize);

	phBoundComposite * pCompositeBound = (phBoundComposite*)m_pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
	int iBone = m_pVehicle->GetBoneIndex(eComponent);
	const int iGroupIndex = m_pVehicle->GetFragInst()->GetGroupFromBoneIndex(iBone);

	if(iGroupIndex!=-1)
	{
		const fragTypeGroup * pGroup = m_pVehicle->GetFragInst()->GetTypePhysics()->GetAllGroups()[iGroupIndex];
		const int iFirstChildIndex = pGroup->GetChildFragmentIndex();
		const int iNumChildren = pGroup->GetNumChildren();

		for(int iChild = iFirstChildIndex ; iChild < iFirstChildIndex+iNumChildren; iChild++)
		{
			const phBound * pSubBound = pCompositeBound->GetBound(iChild);
			if(pSubBound && pSubBound->GetType()!=phBound::BVH)
			{
				const Matrix34 & mSubBound = RCC_MATRIX34(pCompositeBound->GetCurrentMatrix(iChild));
				const Vector3 vMin = VEC3V_TO_VECTOR3(pSubBound->GetBoundingBoxMin()) - vExpand;
				const Vector3 vMax = VEC3V_TO_VECTOR3(pSubBound->GetBoundingBoxMax()) + vExpand;

				const bool bCullBound = bCullSubBounds && (eComponent != VEH_CHASSIS) && ShouldSubBoundBeCulled(vMin, vMax, mSubBound);

				if(!bCullBound)
				{
					Vector2 vVerts[4];
					ProjectBoundingBoxOntoZPlane(vMin, vMax, mSubBound, vVerts);

					if(!CConvexHullCalculator2D::AddVertices(vVerts, 4))
						return false;
				}

#if __BANK && DEBUG_DRAW

				if(CVehicleComponentVisualiser::m_bDrawConvexHull &&
					CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff)
				{
					Vector2 vVerts[4];
					ProjectBoundingBoxOntoZPlane(vMin, vMax, mSubBound, vVerts);

					Vector3 v0(vVerts[3].x, vVerts[3].y, m_pVehicle->GetTransform().GetPosition().GetZf() - 0.2f);
					for(int i=0; i<4; i++)
					{
						Vector3 v1(vVerts[i].x, vVerts[i].y, m_pVehicle->GetTransform().GetPosition().GetZf() - 0.2f);
						grcDebugDraw::Line(v0, v1, bCullBound ? Color_red : Color32(0x60,0x60,0,0xff));
						v0 = v1;
					}
				}
#endif
			}
		}
	}

	return true;
}

bool CVehicleHullCalculator::Create2dOrientatedBoxesFoSpecificModels(const CVehicle * pVehicle, const float fExpandSize)
{
	Vector3 vMins[4];
	Vector3 vMaxs[4];
	bool bMayBeculled[4];
	s32 iNum = 0;

	if(pVehicle->GetModelIndex()==MI_PLANE_TITAN.GetModelIndex())
	{
		vMins[0] = Vector3( -2.3f, -10.0f, -1.5f );
		vMaxs[0] = Vector3( 2.3f, 10.8f, 3.2f );
		bMayBeculled[0] = false;
		iNum = 1;
	}
	else if(pVehicle->GetModelIndex()==MI_PLANE_CARGOPLANE.GetModelIndex())
	{
		vMins[0] = Vector3( -5.1f, -37.0f, -6.0f );
		vMaxs[0] = Vector3( 5.1f, 37.2f, 4.3f );
		bMayBeculled[0] = false;
		iNum = 1;
	}
	else if(pVehicle->GetModelIndex()==MI_CAR_ZTYPE.GetModelIndex())
	{
		vMins[0] = Vector3( -1.0f, -2.1f, -0.58f );
		vMaxs[0] = Vector3( 1.0f, 2.31f, 1.41f );
		bMayBeculled[0] = false;

		vMins[1] = Vector3( -0.1f, -2.6f, -0.58f );
		vMaxs[1] = Vector3( 0.1f, -2.4f, 1.41f );
		bMayBeculled[1] = false;

		iNum = 2;
	}
	else if( pVehicle->GetModelIndex() == MI_HELI_AKULA.GetModelIndex() )
	{
		if(pVehicle->GetVariationInstance().GetModIndex(VMT_WING_R) != INVALID_MOD)
		{
			vMins[0] = Vector3( -3.2f, 2.0f, -1.25f );
			vMaxs[0] = Vector3( 3.2f, 4.0f, 0.5f );
			bMayBeculled[0] = true;
			iNum = 1;
		}
	}
	else
	{
		return true;
	}

	//const Matrix34 mat = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix());
	const Vector3 vExpand(fExpandSize,fExpandSize,fExpandSize);

	for(s32 i=0; i<iNum; i++)
	{
		vMins[i] -= vExpand;
		vMins[i] += vExpand;

		const bool bCullBound = bMayBeculled[0] && ShouldSubBoundBeCulled(vMins[i], vMaxs[i], M34_IDENTITY);
		if(!bCullBound)
		{
			Vector2 vVerts[4];
			ProjectBoundingBoxOntoZPlane(vMins[i], vMaxs[i], M34_IDENTITY, vVerts);

			if(!CConvexHullCalculator2D::AddVertices(vVerts, 4))
				return false;
		}

#if __BANK && DEBUG_DRAW

		if(CVehicleComponentVisualiser::m_bDrawConvexHull &&
			CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff)
		{
			Vector2 vVerts[4];
			ProjectBoundingBoxOntoZPlane(vMins[i], vMaxs[i], M34_IDENTITY, vVerts);

			Vector3 v0(vVerts[3].x, vVerts[3].y, m_pVehicle->GetTransform().GetPosition().GetZf() - 0.2f);
			for(int i=0; i<4; i++)
			{
				Vector3 v1(vVerts[i].x, vVerts[i].y, m_pVehicle->GetTransform().GetPosition().GetZf() - 0.2f);
				grcDebugDraw::Line(v0, v1, bCullBound ? Color_red : Color32(0x60,0x60,0,0xff));
				v0 = v1;
			}
		}
#endif
	}
	return true;
}

void CVehicleHullCalculator::ProjectBoundingBoxOntoZPlane(const Vector3 &vMin_, const Vector3 &vMax_, const Matrix34 & mSubBound, Vector2 * pOutVerts)
{
	Vector3 vMin = vMin_, vMax = vMax_;
	Matrix34 mOrientation;
	m_pVehicle->GetMatrixCopy(mOrientation);
	mOrientation.Dot3x3FromLeft(mSubBound);

	const float fProjLengths[3] =
	{
		mOrientation.a.x * mOrientation.a.x + mOrientation.a.y * mOrientation.a.y,
		mOrientation.b.x * mOrientation.b.x + mOrientation.b.y * mOrientation.b.y,
		mOrientation.c.x * mOrientation.c.x + mOrientation.c.y * mOrientation.c.y
	};
	const float fBoxSizes[3] =
	{
		(vMax.x - vMin.x) * 0.5f,
		(vMax.y - vMin.y) * 0.5f,
		(vMax.z - vMin.z) * 0.5f
	};
	
	Vector3 vLocalCentre = (vMax + vMin) * 0.5f;
	vMin -= vLocalCentre;
	vMax -= vLocalCentre;

	Vector3 vWorldCentre = mOrientation * vLocalCentre;
	// Vector3 vCentreOffset = vWorldCentre - mOrientation.d;
	mOrientation.Transform3x3(vLocalCentre);

	const float fTotalSizes[3] =
	{
		fProjLengths[0] * fBoxSizes[0] * 2.0f,
		fProjLengths[1] * fBoxSizes[1] * 2.0f,
		fProjLengths[2] * fBoxSizes[2] * 2.0f
	};

	float fOffsetLength1, fOffsetLength2, fOffsetLength, fOffsetWidth1, fOffsetWidth2, fOffsetWidth;
	Vector3 vMainVec, vMainVecPerp, vHigherPoint, vLowerPoint;

	if (fTotalSizes[0] > fTotalSizes[1] && fTotalSizes[0] > fTotalSizes[2])
	{
		// x-axis is the main one
		vMainVec = Vector3(mOrientation.a.x, mOrientation.a.y, 0.0f);

		vHigherPoint = vWorldCentre + fBoxSizes[0] * vMainVec;
		vLowerPoint = vWorldCentre - fBoxSizes[0] * vMainVec;
		vMainVec.Normalize();

		fOffsetLength1 = fBoxSizes[1] * (vMainVec.y * mOrientation.b.y   +   vMainVec.x * mOrientation.b.x);
		fOffsetWidth1 =  fBoxSizes[1] * (vMainVec.x * mOrientation.b.y   -   vMainVec.y * mOrientation.b.x);
		fOffsetLength2 = fBoxSizes[2] * (vMainVec.y * mOrientation.c.y   +   vMainVec.x * mOrientation.c.x);
		fOffsetWidth2 =  fBoxSizes[2] * (vMainVec.x * mOrientation.c.y   -   vMainVec.y * mOrientation.c.x);
	}
	else if (fTotalSizes[1] > fTotalSizes[2])
	{
		// y-axis is the main one
		vMainVec = Vector3(mOrientation.b.x, mOrientation.b.y, 0.0f);

		vHigherPoint = vWorldCentre + fBoxSizes[1] * vMainVec;
		vLowerPoint = vWorldCentre - fBoxSizes[1] * vMainVec;
		vMainVec.Normalize();

		fOffsetLength1 = fBoxSizes[0] * (vMainVec.y * mOrientation.a.y   +   vMainVec.x * mOrientation.a.x);
		fOffsetWidth1 =  fBoxSizes[0] * (vMainVec.x * mOrientation.a.y   -   vMainVec.y * mOrientation.a.x);
		fOffsetLength2 = fBoxSizes[2] * (vMainVec.y * mOrientation.c.y   +   vMainVec.x * mOrientation.c.x);
		fOffsetWidth2 =  fBoxSizes[2] * (vMainVec.x * mOrientation.c.y   -   vMainVec.y * mOrientation.c.x);
	}
	else
	{
		// z-axis is the main one
		vMainVec = Vector3(mOrientation.c.x, mOrientation.c.y, 0.0f);

		vHigherPoint = vWorldCentre + fBoxSizes[2] * vMainVec;
		vLowerPoint = vWorldCentre - fBoxSizes[2] * vMainVec;
		vMainVec.Normalize();

		fOffsetLength1 = fBoxSizes[0] * (vMainVec.y * mOrientation.a.y   +   vMainVec.x * mOrientation.a.x);
		fOffsetWidth1 =  fBoxSizes[0] * (vMainVec.x * mOrientation.a.y   -   vMainVec.y * mOrientation.a.x);
		fOffsetLength2 = fBoxSizes[1] * (vMainVec.y * mOrientation.b.y   +   vMainVec.x * mOrientation.b.x);
		fOffsetWidth2 =  fBoxSizes[1] * (vMainVec.x * mOrientation.b.y   -   vMainVec.y * mOrientation.b.x);
	}

	vMainVecPerp = Vector3(vMainVec.y, -vMainVec.x, 0.0f);

	fOffsetLength = ABS(fOffsetLength1) + ABS(fOffsetLength2);
	fOffsetWidth = ABS(fOffsetWidth1) + ABS(fOffsetWidth2);

	// JB : correct ordering now - front, left, rear & right sides of the entity
	Vector3 vPts[4];
	vPts[0] = vHigherPoint + fOffsetLength * vMainVec - fOffsetWidth * vMainVecPerp;
	vPts[1] = vLowerPoint - fOffsetLength * vMainVec - fOffsetWidth * vMainVecPerp;
	vPts[2] = vLowerPoint - fOffsetLength * vMainVec + fOffsetWidth * vMainVecPerp;
	vPts[3] = vHigherPoint + fOffsetLength * vMainVec + fOffsetWidth * vMainVecPerp;				

	Vector3 vLocalOffset;
	const Matrix34& vehMat = RCC_MATRIX34(m_pVehicle->GetMatrixRef());
	vehMat.Transform3x3(mSubBound.d, vLocalOffset);

	for(int p=0; p<4; p++)
	{
		vPts[p] += vLocalOffset;

		pOutVerts[p].x = vPts[p].x;
		pOutVerts[p].y = vPts[p].y;
	}
}

//***********************************************************
//	CVehicleHullAI
//

dev_float CVehicleHullAI::ms_fMoveOutsideEps = 0.05f;
dev_float CVehicleHullAI::ms_fCloseRouteLength = 2.0f;

CVehicleHullAI::CVehicleHullAI(CVehicle * pVehicle)
{
	m_pVehicle = pVehicle;
	m_iNumVerts = 0;
}
CVehicleHullAI::~CVehicleHullAI()
{

}
void CVehicleHullAI::Init(const float fExpandSize, eHierarchyId * pExcludeIDs, const float fCullMinZ, const float fCullMaxZ)
{
	CVehicleHullCalculator::Reset();
	CVehicleHullCalculator::CalculateHullForVehicle(m_pVehicle, this, fExpandSize, pExcludeIDs, fCullMinZ, fCullMaxZ);
}
void CVehicleHullAI::Scale(const Vector2 & vScale)
{
	Vector2 vAxisX( VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetRight()), Vector2::kXY);
	Vector2 vAxisY( VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetForward()), Vector2::kXY);
	vAxisX.Normalize();
	vAxisY.Normalize();

	const Vector2 vOrigin( VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()), Vector2::kXY);
	const float fAxisXD = - vOrigin.Dot(vAxisX);
	const float fAxisYD = - vOrigin.Dot(vAxisY);

	for(int v=0; v<m_iNumVerts; v++)
	{
		const float fDistX = vAxisX.Dot(m_vHullVerts[v]) + fAxisXD;
		m_vHullVerts[v] -= vAxisX * (fDistX - (fDistX * vScale.x));

		const float fDistY = vAxisY.Dot(m_vHullVerts[v]) + fAxisYD;
		m_vHullVerts[v] -= vAxisY * (fDistY - (fDistY * vScale.y));
	}
}
bool CVehicleHullAI::LiesInside(const Vector2 & vPos) const
{
	static const float fEps = 0.0f;

	// If vPos is outside of the plane of any edge, then we're not inside the hull
	int lastv = m_iNumVerts-1;
	for(int v=0; v<m_iNumVerts; v++)
	{
		Vector2 vEdge = m_vHullVerts[v] - m_vHullVerts[lastv];
		vEdge.Normalize();
		const Vector2 vNormal(vEdge.y, -vEdge.x);
		const float fPlaneDist = - ((vNormal.x * m_vHullVerts[v].x) + (vNormal.y * m_vHullVerts[v].y));
		const float fDist = ((vNormal.x*vPos.x)+(vNormal.y*vPos.y))+fPlaneDist;
		if(fDist >= fEps)
			return false;
		lastv = v;
	}
	return true;
}
bool CVehicleHullAI::Intersects(const Vector2 & vStart, const Vector2 & vEnd) const
{
	float t1 = 0.0f, t2 = 0.0f;

	int lastv = m_iNumVerts-1;
	for(int v=0; v<m_iNumVerts; v++)
	{
		if(geom2D::Test2DSegVsSeg(t1,t2,vStart,vEnd,m_vHullVerts[v],m_vHullVerts[lastv]))
		{
			Assert(t1>=0.0f && t1<=1.0f && t2>=0.0f && t2<=1.0f);
			if((t1 == 0.0f && t2 == 0.0f) || (t1 == 1.0f && t2 == 1.0f))
			{
			}
			else
			{
				return true;
			}
		}
		lastv = v;
	}
	return false;
}
bool CVehicleHullAI::MoveOutside(Vector2 & vPos, const float fAmount)
{
	static const float fInHullEps = 0.05f;

	// Test each segment of the hull, until we find which 'slice' contains vPos
	int lastv = m_iNumVerts-1;
	for(int v=0; v<m_iNumVerts; v++)
	{
		Vector2 vSliceEdge1 = m_vHullVerts[v] - m_vCentre;
		vSliceEdge1.Normalize();
		Vector2 vSliceEdge2 = m_vCentre - m_vHullVerts[lastv];
		vSliceEdge2.Normalize();

		const Vector2 vNormal1(vSliceEdge1.y, -vSliceEdge1.x);
		const float fPlaneDist1 = - ((vNormal1.x * m_vCentre.x) + (vNormal1.y * m_vCentre.y));
		const float fDist1 = ((vNormal1.x*vPos.x)+(vNormal1.y*vPos.y))+fPlaneDist1;

		if(fDist1 >= 0.0f)
		{
			const Vector2 vNormal2(vSliceEdge2.y, -vSliceEdge2.x);
			const float fPlaneDist2 = - ((vNormal2.x * m_vCentre.x) + (vNormal2.y * m_vCentre.y));
			const float fDist2 = ((vNormal2.x*vPos.x)+(vNormal2.y*vPos.y))+fPlaneDist2;

			if(fDist2 >= 0.0f)
			{
				// We are within the left/right extents of this 'slice'
				// Now see if we are within the hull itself, or already outside
				Vector2 vEdge = m_vHullVerts[v] - m_vHullVerts[lastv];
				vEdge.Normalize();
				const Vector2 vNormal(vEdge.y, -vEdge.x);
				const float fPlaneDist = - ((vNormal.x * m_vHullVerts[v].x) + (vNormal.y * m_vHullVerts[v].y));
				const float fDist = ((vNormal.x*vPos.x)+(vNormal.y*vPos.y))+fPlaneDist;
				if(fDist < fInHullEps)
				{
					// Ok, so we're within the hull.
					// Move the position out onto the edge of the hull
					vPos += vNormal * (-fDist + fAmount);
					return true;
				}
			}
		}
		lastv = v;
	}
	return false;
}
bool CVehicleHullAI::MoveOutside(Vector2 & vPos, const Vector2 & vDirection, const float fAmount)
{
	Assert(vDirection.Mag2() > 0.1f);

	const Vector2 vEnd = vPos + (vDirection * 100.0f);

	float t1 = 0.0f, t2 = 0.0f;

	int lastv = m_iNumVerts-1;
	for(int v=0; v<m_iNumVerts; v++)
	{
		if(geom2D::Test2DSegVsSeg(t1, t2, vPos, vEnd, m_vHullVerts[v], m_vHullVerts[lastv]))
		{
			// We've hit an edge.  Find the intersection point and use that as the new vPos.
			const Vector2 vEdge = m_vHullVerts[lastv] - m_vHullVerts[v];
			vPos = m_vHullVerts[v] + (vEdge * t2);
			vPos += vDirection * fAmount;
			return true;
		}
		lastv = v;
	}
	return false;
}

// NAME : CalcShortestRouteToTarget
// PURPOSE : Calculates the shortest route around the hull in CW or CCW direction, testing for clear LOS as specified
// IF 'piHullDirection' is non-NULL, it may contain a preferred direction (-1 or +1) which will be used IFF both CW & CCW LOS is clear, and route lengths are both within ms_fCloseRouteLength in size
// IF 'piHullDirection' is non-NULL, then the chosen route direction will be returned in this variable
// fNormalMinZ default to a large value (10.0f) that that collisions will always be accepted, otherwise it is used in comparison with the collision normal to ignore flat surfaces
bool CVehicleHullAI::CalcShortestRouteToTarget(const CPed * pPed, const Vector2 & vStart, const Vector2 & vTarget, CPointRoute * pRoute, const float fZHeight, const u32 iLosTestFlags, const float fRadius, s32 * piHullDirection, const float fNormalMinZ BANK_ONLY(, bool bLogHitEntities))
{
	Assert(pRoute && pRoute->GetSize()==0);

	//*****************************************************************************
	// Find out which segment is closest to the target position. Find the closest
	// position on this segment.  Do the same for the start position.

	TEndPointInfo routeEndPoints[2] =
	{
		TEndPointInfo(Vector3(vStart.x, vStart.y, fZHeight)),
		TEndPointInfo(Vector3(vTarget.x, vTarget.y, fZHeight))
	};

	int r,p;

	int lastv = m_iNumVerts-1;
	for(int v=0; v<m_iNumVerts; v++)
	{
		const Vector3 vSegStart(m_vHullVerts[lastv].x, m_vHullVerts[lastv].y, fZHeight);
		const Vector3 vSegDir(Vector3(m_vHullVerts[v].x, m_vHullVerts[v].y, fZHeight) - vSegStart);

		for(p=0; p<2; p++)
		{
			const Vector3 & vEndPoint = routeEndPoints[p].m_vPosition;
			const float t = (vSegDir.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vSegStart, vSegDir, vEndPoint) : 0.0f;
			const Vector3 vClosest = vSegStart + (vSegDir * t);
			const float fDistSqr = (vClosest - vEndPoint).Mag2();

			if(fDistSqr < routeEndPoints[p].m_fClosestDistSqr)
			{
				routeEndPoints[p].m_fClosestDistSqr = fDistSqr;
				routeEndPoints[p].m_iClosestSegment = lastv;
				routeEndPoints[p].m_vClosestPositionOnHull = vClosest;
			}
		}

		lastv = v;
	}

	//Assert(routeEndPoints[0].m_iClosestSegment != -1 && routeEndPoints[1].m_iClosestSegment != -1);
	if(routeEndPoints[0].m_iClosestSegment == -1 || routeEndPoints[1].m_iClosestSegment == -1)
	{
		if(piHullDirection)
			*piHullDirection = 0;	// Kind of tell we didn't pick any route, make sure we pick one when we got routes but both are blocked

		return false;
	}

	//****************************************************************************
	// If start & target are on the same edge segment then go straight to target

	if(routeEndPoints[0].m_iClosestSegment == routeEndPoints[1].m_iClosestSegment)
	{
		pRoute->Add( routeEndPoints[0].m_vPosition );
		pRoute->Add( routeEndPoints[1].m_vPosition );

		if(iLosTestFlags)
		{
			float fDistToObstruction;
			if(TestIsRouteClear(pPed, pRoute, iLosTestFlags, fRadius, fDistToObstruction, fNormalMinZ BANK_ONLY(, bLogHitEntities)))
			{
				if(piHullDirection)
					*piHullDirection = 0;
				return true;
			}
		}
		else
		{
			if(piHullDirection)
				*piHullDirection = 0;
			return true;
		}
	}

	// Construct the routes around the hull in clockwise & anticlockwise directions
	Vector3 vRoutes[2][CConvexHullCalculator2D::ms_iMaxNumHullVertices];
	int iRouteSizes[2] = { 0, 0 };

	// Walk the segments in the -1 and +1 directions
	for(int iDir=-1; iDir<=1; iDir+=2)
	{
		const int iRoute = (iDir==-1) ? 0 : 1;
		// Add the first point
		vRoutes[iRoute][0] = routeEndPoints[0].m_vPosition;
		int iVertNum = 1;
		// Add a point on the hull, if we started inside the hull
		if(LiesInside(vStart))
			vRoutes[iRoute][iVertNum++] = routeEndPoints[0].m_vClosestPositionOnHull;

		int iCurrentSeg = routeEndPoints[0].m_iClosestSegment;
		
		while(iCurrentSeg != routeEndPoints[1].m_iClosestSegment)
		{
			// When going clockwise, we add the vertex before advancing the iCurrentSet
			if(iDir==-1)
				vRoutes[iRoute][iVertNum++] = Vector3(m_vHullVerts[iCurrentSeg].x, m_vHullVerts[iCurrentSeg].y, fZHeight);

			iCurrentSeg += iDir;
			if(iCurrentSeg < 0)
				iCurrentSeg += m_iNumVerts;
			if(iCurrentSeg >= m_iNumVerts)
				iCurrentSeg -= m_iNumVerts;

			// When going anticlockwise, we add the vertex after advancing the iCurrentSet
			if(iDir==1)
				vRoutes[iRoute][iVertNum++] = Vector3(m_vHullVerts[iCurrentSeg].x, m_vHullVerts[iCurrentSeg].y, fZHeight);
		}

		// Add the final point
		vRoutes[iRoute][iVertNum++] = routeEndPoints[1].m_vPosition;
		iRouteSizes[iRoute] = iVertNum;
	}

	// Now work out which route is the shorter
	float fRouteLengths[2] = { 0.0f, 0.0f };
	for(r=0; r<2; r++)
	{
		for(p=0; p<iRouteSizes[r]-1; p++)
		{
			fRouteLengths[r] += (vRoutes[r][p] - vRoutes[r][p+1]).Mag();
		}
	}

	//***************************************************************************************************
	// If we have a preference upon which direction to take, increase apparent length of the other route

	const s32 iPreferredDir = piHullDirection ? *piHullDirection : 0;
	if(iPreferredDir != 0)
	{
		if(iPreferredDir == -1)
			fRouteLengths[1] += ms_fCloseRouteLength;
		else
			fRouteLengths[0] += ms_fCloseRouteLength;
	}

	//*********************************************************************************
	// Whichever route is shorter we chose, and copy the points into the CPointRoute

	CPointRoute routes[2];

	const int iShortestRoute = (fRouteLengths[0] < fRouteLengths[1]) ? 0 : 1;

	for(r=0; r<2; r++)
	{
		for(p=0; p<iRouteSizes[r]; p++)
		{
			routes[r].Add(vRoutes[r][p]);
		}

		//*************************************************************************
		// Remove points which are close to the hull's edge, but which we can skip

		if(routes[r].GetSize() > 2)
		{
			Vector2 vPoint(routes[r].Get(2).x, routes[r].Get(2).y);
			Vector2 vPointOnHull = vPoint;
			MoveOutside(vPointOnHull, 0.1f);
			if((vPoint - vPointOnHull).Mag2() < 1.0f)	// Only for points on the hull's edge
			{
				Assert(!LiesInside(vPointOnHull));
				if(!Intersects(vStart, vPointOnHull))
					routes[r].Remove(1);
			}
		}
	}

	// If iLosTestFlags is set to non-zero, then test against the world for obstructions.
	// If only one route is clear, then return that one.  If both are blocked return the
	// route with the longest clear distance before the blockage - but also return false.
	// If neither are blocked, then choose the shortest route.
	if(iLosTestFlags)
	{
		bool bRoutesClear[2];
		float fNearestObstruction[2];
		bRoutesClear[0] = TestIsRouteClear(pPed, &routes[0], iLosTestFlags, fRadius, fNearestObstruction[0], fNormalMinZ BANK_ONLY(, bLogHitEntities));
		bRoutesClear[1] = TestIsRouteClear(pPed, &routes[1], iLosTestFlags, fRadius, fNearestObstruction[1], fNormalMinZ BANK_ONLY(, bLogHitEntities));


		if(bRoutesClear[0] && bRoutesClear[1])
		{
			pRoute->From(routes[iShortestRoute]);
			if(piHullDirection)
				*piHullDirection = (iShortestRoute==0) ? -1 : 1;
			return true;
		}
		else if(bRoutesClear[0] && !bRoutesClear[1])
		{
			pRoute->From(routes[0]);
			if(piHullDirection)
				*piHullDirection = -1;
			return true;
		}
		else if(bRoutesClear[1] && !bRoutesClear[0])
		{
			pRoute->From(routes[1]);
			if(piHullDirection)
				*piHullDirection = 1;
			return true;
		}
		else
		{
			// Both ways are blocked, return the route with the longest distance prior to obstruction
			const s32 iChosenDir = (fNearestObstruction[0] < fNearestObstruction[1]) ? 1 : 0;

			pRoute->From(routes[iChosenDir]);
			if(piHullDirection)
				*piHullDirection = (iChosenDir==0) ? -1 : 1;

			return false;
		}
	}
	else
	{
		pRoute->From(routes[iShortestRoute]);
		if(piHullDirection)
			*piHullDirection = (iShortestRoute==0) ? -1 : 1;
		return true;
	}
}

bool CVehicleHullAI::TestIsRouteClear(const CPed * pPed, CPointRoute * pRoute, const u32 iLosTestFlags, const float fRadius, float & fDistToObstruction, const float fNormalMinZ BANK_ONLY(, bool bLogHitEntities)) const
{
	const float fHeadHeight = pPed->GetCapsuleInfo()->GetMaxSolidHeight();
	const float fKneeHeight = pPed->GetCapsuleInfo()->GetMinSolidHeight();
	const float fDeltaHeight = fHeadHeight - fKneeHeight;
	const float fPedRadius = pPed->GetCapsuleInfo()->GetHalfWidth();
	const float fTestInc = ((fPedRadius*2.0f) * 0.75f);
	const int iNumTests = (int) (fDeltaHeight / fTestInc) + 1;

	fDistToObstruction = 0.0f;
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> results;
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetIncludeFlags(iLosTestFlags);
	capsuleDesc.SetExcludeEntity(m_pVehicle);
	capsuleDesc.SetResultsStructure(&results);

	static const Vector3 vMinSize(1.5f, 1.5f, 1.5f);

	for(int p=1; p<pRoute->GetSize(); p++)
	{
		Vector3 vStart = pRoute->Get(p-1);
		vStart.z += fKneeHeight + fPedRadius * 0.5f;
		Vector3 vEnd = pRoute->Get(p);
		vEnd.z += fKneeHeight + fPedRadius * 0.5f;

		for(int t=0; t<iNumTests; t++)
		{
			//capsuleDesc.SetCapsule(pRoute->Get(p-1) + vOffset, pRoute->Get(p) + vOffset, fRadius);
			capsuleDesc.SetCapsule(vStart, vEnd, fRadius);
			results.Reset();

			if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
			{
				Assert(capsuleDesc.GetResultsStructure()->GetNumHits());

				u32 iNumIgnored = 0;
				float fLeastDist = FLT_MAX;

				const WorldProbe::CShapeTestResults * pResults = capsuleDesc.GetResultsStructure();
				for(s32 i=0; i<pResults->GetNumHits(); i++)
				{
					const WorldProbe::CShapeTestHitPoint & hp = (*pResults)[i];

					bool bIgnore = false;

					if(!hp.GetHitDetected())
					{
						bIgnore = true;
					}
					else
					{
						// For the lowest capsule, we can discount any collision which is facing upwards
						if(t==0 && hp.GetHitPolyNormal().z > fNormalMinZ)
						{
							bIgnore = true;
						}
						// Otherwise we should examine the collision to determine whether we can ignore it
						else
						{
							// Determine whether we should ignore this phInst
							const CEntity * pHitEntity = hp.GetHitEntity();
							if( pHitEntity && !pHitEntity->GetIsFixedFlagSet() )
							{
								// Any model marked as not avoided, should be ignored
								CBaseModelInfo * pModelInfo = pHitEntity->GetBaseModelInfo();
								if(pModelInfo && pModelInfo->GetNotAvoidedByPeds())
									bIgnore = true;
								// Any entity under some minimum size should be ignored
								const Vector3 vSize = pModelInfo->GetBoundingBoxMax() - pModelInfo->GetBoundingBoxMin();
								if(vSize.x < vMinSize.x && vSize.y < vMinSize.y && vSize.z < vMinSize.z)
									bIgnore = true;
							}
						}
					}

#if __BANK
					if (bLogHitEntities)
					{
						const CEntity* pHitEntity = hp.GetHitEntity();
						Vec3V vHitPos = hp.GetHitPositionV();
						AI_LOG_WITH_ARGS("[VehicleEntryExit] CVehicleHullAI::TestIsRouteClear() - hit entity %s[%p] at <%.2f,%.2f,%.2f> \n", pHitEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pHitEntity)) : pHitEntity->GetModelName(), VEC3V_ARGS(vHitPos));
					}
#endif

					if(bIgnore)
					{
						iNumIgnored++;
					}
					else
					{
						const Vector3 v = hp.GetHitPosition() - pRoute->Get(p-1);
						fLeastDist = MIN( v.Mag(), fLeastDist );
					}
				}

				if( iNumIgnored != pResults->GetNumHits() )
				{
					Assert(fLeastDist != FLT_MAX);

					fDistToObstruction += fLeastDist;
					return false;
				}
			}

			vStart.z += fTestInc;
			vEnd.z += fTestInc;

		} // for all tests

		fDistToObstruction += (pRoute->Get(p-1) - pRoute->Get(p)).Mag();
	}
	return true;
}



#if __BANK

bool CVehicleComponentVisualiser::m_bEnabled = false;
bool CVehicleComponentVisualiser::m_bInitialised = false;
bool CVehicleComponentVisualiser::m_bDrawConvexHull = false;
bool CVehicleComponentVisualiser::m_bDebugIgnorePedZ = true;
bool CVehicleComponentVisualiser::m_bDebugDrawBoxTests = false;
bkCombo * CVehicleComponentVisualiser::m_pComponentsCombo = NULL;
int CVehicleComponentVisualiser::m_iSelectedComponentComboIndex = 0;

const eHierarchyId CVehicleComponentVisualiser::m_iComponents[] =
{
	VEH_CHASSIS,
	VEH_CHASSIS_LOWLOD,
	VEH_DOOR_DSIDE_F,
	VEH_DOOR_DSIDE_R,
	VEH_DOOR_PSIDE_F,
	VEH_DOOR_PSIDE_R,

	VEH_HANDLE_DSIDE_F,
	VEH_HANDLE_DSIDE_R,
	VEH_HANDLE_PSIDE_F,
	VEH_HANDLE_PSIDE_R,

	VEH_WHEEL_LF,
	VEH_WHEEL_RF,
	VEH_WHEEL_LR,
	VEH_WHEEL_RR,

	// Extra wheels
	VEH_WHEEL_LM1,
	VEH_WHEEL_RM1,
	VEH_WHEEL_LM2,
	VEH_WHEEL_RM2,
	VEH_WHEEL_LM3,
	VEH_WHEEL_RM3,

	VEH_SUSPENSION_LF,
	VEH_SUSPENSION_RF,
	VEH_SUSPENSION_LM,
	VEH_SUSPENSION_RM,
	VEH_SUSPENSION_LR,
	VEH_SUSPENSION_RR,
	VEH_TRANSMISSION_F,
	VEH_TRANSMISSION_M,
	VEH_TRANSMISSION_R,
	VEH_WHEELHUB_LF,
	VEH_WHEELHUB_RF,	
	VEH_WHEELHUB_LR,
	VEH_WHEELHUB_RR,
	VEH_WHEELHUB_LM1,
	VEH_WHEELHUB_RM1,
	VEH_WHEELHUB_LM2,
	VEH_WHEELHUB_RM2,
	VEH_WHEELHUB_LM3,
	VEH_WHEELHUB_RM3,

	VEH_WINDSCREEN,
	VEH_WINDSCREEN_R,
	VEH_WINDOW_LF,
	VEH_WINDOW_RF,
	VEH_WINDOW_LR,
	VEH_WINDOW_RR,
	VEH_WINDOW_LM,
	VEH_WINDOW_RM,

	VEH_BODYSHELL,
	VEH_BUMPER_F,
	VEH_BUMPER_R,
	VEH_WING_RF,
	VEH_WING_LF,
	VEH_BONNET,
	VEH_BOOT,
	VEH_EXHAUST,
	VEH_EXHAUST_2,
	VEH_EXHAUST_3,
	VEH_EXHAUST_4,
	VEH_EXHAUST_5,
	VEH_EXHAUST_6,
	VEH_EXHAUST_7,
	VEH_EXHAUST_8,
	VEH_EXHAUST_9,
	VEH_EXHAUST_10,
	VEH_EXHAUST_11,
	VEH_EXHAUST_12,
	VEH_EXHAUST_13,
	VEH_EXHAUST_14,
	VEH_EXHAUST_15,
	VEH_EXHAUST_16,
	VEH_ENGINE,					
	VEH_OVERHEAT,				
	VEH_OVERHEAT_2,			
	VEH_PETROLCAP,
	VEH_PETROLTANK,
	VEH_PETROLTANK_L,
	VEH_PETROLTANK_R,
	VEH_STEERING_WHEEL,
	VEH_CAR_STEERING_WHEEL,
	VEH_HBGRIP_L,
	VEH_HBGRIP_R,

	VEH_HEADLIGHT_L,
	VEH_HEADLIGHT_R,
	VEH_TAILLIGHT_L,
	VEH_TAILLIGHT_R,
	VEH_INDICATOR_LF,
	VEH_INDICATOR_RF,
	VEH_INDICATOR_LR,
	VEH_INDICATOR_RR,
	VEH_BRAKELIGHT_L,
	VEH_BRAKELIGHT_R,
	VEH_BRAKELIGHT_M,
	VEH_REVERSINGLIGHT_L,
	VEH_REVERSINGLIGHT_R,
	VEH_NEON_L,
	VEH_NEON_R,
	VEH_NEON_F,
	VEH_NEON_B,
	VEH_EXTRALIGHT_1,
	VEH_EXTRALIGHT_2,
	VEH_EXTRALIGHT_3,
	VEH_EXTRALIGHT_4,

	VEH_EMISSIVES,
	VEH_NUMBERPLATE,
	VEH_INTERIORLIGHT,
    VEH_PLATELIGHT,
	VEH_DASHLIGHT,
    VEH_DOORLIGHT_LF,
    VEH_DOORLIGHT_RF,
    VEH_DOORLIGHT_LR,
    VEH_DOORLIGHT_RR,
	VEH_SIREN_1,
	VEH_SIREN_2,
	VEH_SIREN_3,
	VEH_SIREN_4,
	VEH_SIREN_5,
	VEH_SIREN_6,
	VEH_SIREN_7,
	VEH_SIREN_8,
	VEH_SIREN_9,
	VEH_SIREN_10,
	VEH_SIREN_11,
	VEH_SIREN_12,
	VEH_SIREN_13,
	VEH_SIREN_14,
	VEH_SIREN_15,
	VEH_SIREN_16,
	VEH_SIREN_17,
	VEH_SIREN_18,
	VEH_SIREN_19,
	VEH_SIREN_20,

	VEH_SIREN_GLASS_1,
	VEH_SIREN_GLASS_2,
	VEH_SIREN_GLASS_3,
	VEH_SIREN_GLASS_4,
	VEH_SIREN_GLASS_5,
	VEH_SIREN_GLASS_6,
	VEH_SIREN_GLASS_7,
	VEH_SIREN_GLASS_8,
	VEH_SIREN_GLASS_9,
	VEH_SIREN_GLASS_10,
	VEH_SIREN_GLASS_11,
	VEH_SIREN_GLASS_12,
	VEH_SIREN_GLASS_13,
	VEH_SIREN_GLASS_14,
	VEH_SIREN_GLASS_15,
	VEH_SIREN_GLASS_16,
	VEH_SIREN_GLASS_17,
	VEH_SIREN_GLASS_18,
	VEH_SIREN_GLASS_19,
	VEH_SIREN_GLASS_20,

	VEH_WEAPON_1A,
	VEH_WEAPON_1B,
	VEH_WEAPON_1C,
	VEH_WEAPON_1D,
	VEH_WEAPON_2A,
	VEH_WEAPON_2B,
	VEH_WEAPON_2C,
	VEH_WEAPON_2D,
	VEH_WEAPON_2E,
	VEH_WEAPON_2F,
	VEH_WEAPON_2G,
	VEH_WEAPON_2H,
	VEH_WEAPON_3A,
	VEH_WEAPON_3B,
	VEH_WEAPON_3C,
	VEH_WEAPON_3D,

	VEH_TURRET_1_BASE,
	VEH_TURRET_1_BARREL,
	VEH_TURRET_2_BASE,
	VEH_TURRET_2_BARREL,
	VEH_SEARCHLIGHT_BASE,
	VEH_SEARCHLIGHT_BARREL,

	VEH_ATTACH,
	VEH_ROOF,
	VEH_ROOF2,
	VEH_FORKS,
	VEH_MAST,
	VEH_CARRIAGE,
	VEH_DOOR_HATCH_R,
	VEH_DOOR_HATCH_L,
	VEH_TOW_ARM,
	VEH_TOW_MOUNT_A,
	VEH_TOW_MOUNT_B,
	VEH_TIPPER,
	VEH_PICKUP_REEL, 	//combine havester/thresher part
	VEH_AUGER,			//combine havester/thresher part
	VEH_ARM1,
	VEH_ARM2,
	VEH_ARM3,
	VEH_ARM4,
	VEH_DIGGER_ARM,
	VEH_ARTICULATED_DIGGER_ARM_BASE,
	VEH_ARTICULATED_DIGGER_ARM_BOOM,
	VEH_ARTICULATED_DIGGER_ARM_STICK,
	VEH_ARTICULATED_DIGGER_ARM_BUCKET,

	VEH_CUTTER_ARM_1,
	VEH_CUTTER_ARM_2,
	VEH_CUTTER_BOOM_DRIVER,
	VEH_CUTTER_CUTTER_DRIVER,

    VEH_DIALS,

	VEH_LIGHTCOVER,

	VEH_BOBBLE_HEAD,
	VEH_BOBBLE_BASE,
	VEH_BOBBLE_HAND,
	VEH_BOBBLE_ENGINE,

	VEH_SPOILER,
	VEH_STRUTS,

	VEH_FLAP_L,
	VEH_FLAP_R,

	VEH_MISC_A,
	VEH_MISC_B,
	VEH_MISC_C,
	VEH_MISC_D,
	VEH_MISC_E,
	VEH_MISC_F,
	VEH_MISC_G,
	VEH_MISC_H,
	VEH_MISC_I,
	VEH_MISC_J,
	VEH_MISC_K,
	VEH_MISC_L,
	VEH_MISC_M,
	VEH_MISC_N,
	VEH_MISC_O,
	VEH_MISC_P,
	VEH_MISC_Q,
	VEH_MISC_R,
	VEH_MISC_S,
	VEH_MISC_T,
	VEH_MISC_U,
	VEH_MISC_V,
	VEH_MISC_W,
	VEH_MISC_X,
	VEH_MISC_Y,
	VEH_MISC_Z,
	VEH_MISC_1,
	VEH_MISC_2,

	VEH_EXTRA_1,	// extras that can be changed in other vehicle types
	VEH_EXTRA_2,
	VEH_EXTRA_3,
	VEH_EXTRA_4,
	VEH_EXTRA_5,
	VEH_EXTRA_6,
	VEH_EXTRA_7,
	VEH_EXTRA_8,
	VEH_EXTRA_9,
	VEH_EXTRA_10,
	VEH_EXTRA_11,
	VEH_EXTRA_12,

	VEH_BREAKABLE_EXTRA_1,	// breakable extras that can be changed in other vehicle types
	VEH_BREAKABLE_EXTRA_2,
	VEH_BREAKABLE_EXTRA_3,
	VEH_BREAKABLE_EXTRA_4,
	VEH_BREAKABLE_EXTRA_5,
	VEH_BREAKABLE_EXTRA_6,
	VEH_BREAKABLE_EXTRA_7,
	VEH_BREAKABLE_EXTRA_8,
	VEH_BREAKABLE_EXTRA_9,
	VEH_BREAKABLE_EXTRA_10,

	VEH_MOD_COLLISION_1,
	VEH_MOD_COLLISION_2,
	VEH_MOD_COLLISION_3,
	VEH_MOD_COLLISION_4,
	VEH_MOD_COLLISION_5,
	VEH_MOD_COLLISION_6,
	VEH_MOD_COLLISION_7,
	VEH_MOD_COLLISION_8,
	VEH_MOD_COLLISION_9,
	VEH_MOD_COLLISION_10,
	VEH_MOD_COLLISION_11,
	VEH_MOD_COLLISION_12,
	VEH_MOD_COLLISION_13,
	VEH_MOD_COLLISION_14,
	VEH_MOD_COLLISION_15,
	VEH_MOD_COLLISION_16,

	VEH_SPRING_RF,
	VEH_SPRING_LF,
	VEH_SPRING_RR,
	VEH_SPRING_LR,

	VEH_INVALID_ID
};
const char * CVehicleComponentVisualiser::m_pComponentNames[] =
{
	"VEH_CHASSIS",
	"VEH_CHASSIS_LOWLOD",
	"VEH_DOOR_DSIDE_F",
	"VEH_DOOR_DSIDE_R",
	"VEH_DOOR_PSIDE_F",
	"VEH_DOOR_PSIDE_R",

	"VEH_HANDLE_DSIDE_F",
	"VEH_HANDLE_DSIDE_R",
	"VEH_HANDLE_PSIDE_F",
	"VEH_HANDLE_PSIDE_R",

	"VEH_WHEEL_LF",
	"VEH_WHEEL_RF",
	"VEH_WHEEL_LR",
	"VEH_WHEEL_RR",

	// Extra wheels
	"VEH_WHEEL_LM1",
	"VEH_WHEEL_RM1",
	"VEH_WHEEL_LM2",
	"VEH_WHEEL_RM2",
	"VEH_WHEEL_LM3",
	"VEH_WHEEL_RM3",

	"VEH_SUSPENSION_LF",
	"VEH_SUSPENSION_RF",
	"VEH_SUSPENSION_LM",
	"VEH_SUSPENSION_RM",
	"VEH_SUSPENSION_LR",
	"VEH_SUSPENSION_RR",
	"VEH_TRANSMISSION_F",
	"VEH_TRANSMISSION_M",
	"VEH_TRANSMISSION_R",
	"VEH_WHEELHUB_LF",
	"VEH_WHEELHUB_RF",	
	"VEH_WHEELHUB_LR",
	"VEH_WHEELHUB_RR",
	"VEH_WHEELHUB_LM1",
	"VEH_WHEELHUB_RM1",
	"VEH_WHEELHUB_LM2",
	"VEH_WHEELHUB_RM2",
	"VEH_WHEELHUB_LM3",
	"VEH_WHEELHUB_RM3",

	"VEH_WINDSCREEN",
	"VEH_WINDSCREEN_R",
	"VEH_WINDOW_LF",
	"VEH_WINDOW_RF",
	"VEH_WINDOW_LR",
	"VEH_WINDOW_RR",
	"VEH_WINDOW_LM",
	"VEH_WINDOW_RM",

	"VEH_BODYSHELL",
	"VEH_BUMPER_F",
	"VEH_BUMPER_R",
	"VEH_WING_RF",
	"VEH_WING_LF",
	"VEH_BONNET",
	"VEH_BOOT",
	"VEH_EXHAUST",
	"VEH_EXHAUST_2",
	"VEH_EXHAUST_3",
	"VEH_EXHAUST_4",
	"VEH_EXHAUST_5",
	"VEH_EXHAUST_6",
	"VEH_EXHAUST_7",
	"VEH_EXHAUST_8",
	"VEH_EXHAUST_9",
	"VEH_EXHAUST_10",
	"VEH_EXHAUST_11",
	"VEH_EXHAUST_12",
	"VEH_EXHAUST_13",
	"VEH_EXHAUST_14",
	"VEH_EXHAUST_15",
	"VEH_EXHAUST_16",
	"VEH_ENGINE",					
	"VEH_OVERHEAT",				
	"VEH_OVERHEAT_2",			
	"VEH_PETROLCAP",
	"VEH_PETROLTANK",
	"VEH_PETROLTANK_L",
	"VEH_PETROLTANK_R",
	"VEH_STEERING_WHEEL",
	"VEH_CAR_STEERING_WHEEL",
	"VEH_HBGRIP_L",
	"VEH_HBGRIP_R",

	"VEH_HEADLIGHT_L",
	"VEH_HEADLIGHT_R",
	"VEH_TAILLIGHT_L",
	"VEH_TAILLIGHT_R",
	"VEH_INDICATOR_LF",
	"VEH_INDICATOR_RF",
	"VEH_INDICATOR_LR",
	"VEH_INDICATOR_RR",
	"VEH_BRAKELIGHT_L",
	"VEH_BRAKELIGHT_R",
	"VEH_BRAKELIGHT_M",
	"VEH_REVERSINGLIGHT_L",
	"VEH_REVERSINGLIGHT_R",
	"VEH_NEON_L",
	"VEH_NEON_R",
	"VEH_NEON_F",
	"VEH_NEON_B",
	"VEH_EXTRALIGHT_1",
	"VEH_EXTRALIGHT_2",
	"VEH_EXTRALIGHT_3",
	"VEH_EXTRALIGHT_4",

	"VEH_EMISSIVES",
	"VEH_NUMBERPLATE",
	"VEH_INTERIORLIGHT",
    "VEH_PLATELIGHT",
	"VEH_DASHLIGHT",
    "VEH_DOORLIGHT_LF",
    "VEH_DOORLIGHT_RF",
    "VEH_DOORLIGHT_LR",
    "VEH_DOORLIGHT_RR",
	"VEH_SIREN_1",
	"VEH_SIREN_2",
	"VEH_SIREN_3",
	"VEH_SIREN_4",
	"VEH_SIREN_5",
	"VEH_SIREN_6",
	"VEH_SIREN_7",
	"VEH_SIREN_8",
	"VEH_SIREN_9",
	"VEH_SIREN_10",
	"VEH_SIREN_11",
	"VEH_SIREN_12",
	"VEH_SIREN_13",
	"VEH_SIREN_14",
	"VEH_SIREN_15",
	"VEH_SIREN_16",
	"VEH_SIREN_17",
	"VEH_SIREN_18",
	"VEH_SIREN_19",
	"VEH_SIREN_20",

	"VEH_SIREN_GLASS_1",
	"VEH_SIREN_GLASS_2",
	"VEH_SIREN_GLASS_3",
	"VEH_SIREN_GLASS_4",
	"VEH_SIREN_GLASS_5",
	"VEH_SIREN_GLASS_6",
	"VEH_SIREN_GLASS_7",
	"VEH_SIREN_GLASS_8",
	"VEH_SIREN_GLASS_9",
	"VEH_SIREN_GLASS_10",
	"VEH_SIREN_GLASS_11",
	"VEH_SIREN_GLASS_12",
	"VEH_SIREN_GLASS_13",
	"VEH_SIREN_GLASS_14",
	"VEH_SIREN_GLASS_15",
	"VEH_SIREN_GLASS_16",
	"VEH_SIREN_GLASS_17",
	"VEH_SIREN_GLASS_18",
	"VEH_SIREN_GLASS_19",
	"VEH_SIREN_GLASS_20",

	"VEH_WEAPON_1A",
	"VEH_WEAPON_1B",
	"VEH_WEAPON_1C",
	"VEH_WEAPON_1D",
	"VEH_WEAPON_2A",
	"VEH_WEAPON_2B",
	"VEH_WEAPON_2C",
	"VEH_WEAPON_2D",
	"VEH_WEAPON_2E",
	"VEH_WEAPON_2F",
	"VEH_WEAPON_2G",
	"VEH_WEAPON_2H",
	"VEH_WEAPON_3A",
	"VEH_WEAPON_3B",
	"VEH_WEAPON_3C",
	"VEH_WEAPON_3D",

	"VEH_TURRET_1_BASE",
	"VEH_TURRET_1_BARREL",
	"VEH_TURRET_2_BASE",
	"VEH_TURRET_2_BARREL",
	"VEH_SEARCHLIGHT_BASE",
	"VEH_SEARCHLIGHT_BARREL",

	"VEH_ATTACH",
	"VEH_ROOF",
	"VEH_ROOF2",
	"VEH_FORKS",
	"VEH_MAST",
	"VEH_CARRIAGE",
	"VEH_DOOR_HATCH_R",
	"VEH_DOOR_HATCH_L",
	"VEH_TOW_ARM",
	"VEH_TOW_MOUNT_A",
	"VEH_TOW_MOUNT_B",
	"VEH_TIPPER",
	"VEH_PICKUP_REEL", 	//combine havester/thresher part
	"VEH_AUGER",			//combine havester/thresher part
	"VEH_ARM1",
	"VEH_ARM2",
	"VEH_ARM3",
	"VEH_ARM4",
	"VEH_DIGGER_ARM",
	"VEH_ARTICULATED_DIGGER_ARM_BASE",
	"VEH_ARTICULATED_DIGGER_ARM_BOOM",
	"VEH_ARTICULATED_DIGGER_ARM_STICK",
	"VEH_ARTICULATED_DIGGER_ARM_BUCKET",

	"VEH_CUTTER_ARM_1",
	"VEH_CUTTER_ARM_2",
	"VEH_CUTTER_BOOM_DRIVER",
	"VEH_CUTTER_CUTTER_DRIVER",

    "VEH_DIALS",

	"VEH_LIGHTCOVER",

	"VEH_BOBBLE_HEAD",
	"VEH_BOBBLE_BASE",
	"VEH_BOBBLE_HAND",
	"VEH_BOBBLE_ENGINE",

	"VEH_SPOILER",
	"VEH_STRUTS",

	"VEH_FLAP_L",
	"VEH_FLAP_R",
	
	"VEH_MISC_A",
	"VEH_MISC_B",
	"VEH_MISC_C",
	"VEH_MISC_D",
	"VEH_MISC_E",
	"VEH_MISC_F",
	"VEH_MISC_G",
	"VEH_MISC_H",
	"VEH_MISC_I",
	"VEH_MISC_J",
	"VEH_MISC_K",
	"VEH_MISC_L",
	"VEH_MISC_M",
	"VEH_MISC_N",
	"VEH_MISC_O",
	"VEH_MISC_P",
	"VEH_MISC_Q",
	"VEH_MISC_R",
	"VEH_MISC_S",
	"VEH_MISC_T",
	"VEH_MISC_U",
	"VEH_MISC_V",
	"VEH_MISC_W",
	"VEH_MISC_X",
	"VEH_MISC_Y",
	"VEH_MISC_Z",
	"VEH_MISC_1",
	"VEH_MISC_2",

	"VEH_EXTRA_1",	// extras that can be changed in other vehicle types
	"VEH_EXTRA_2",
	"VEH_EXTRA_3",
	"VEH_EXTRA_4",
	"VEH_EXTRA_5",
	"VEH_EXTRA_6",
	"VEH_EXTRA_7",
	"VEH_EXTRA_8",
	"VEH_EXTRA_9",
	"VEH_EXTRA_10",
	"VEH_EXTRA_11",
	"VEH_EXTRA_12",

	"VEH_BREAKABLE_EXTRA_1",	// breakable extras that can be changed in other vehicle types
	"VEH_BREAKABLE_EXTRA_2",
	"VEH_BREAKABLE_EXTRA_3",
	"VEH_BREAKABLE_EXTRA_4",
	"VEH_BREAKABLE_EXTRA_5",
	"VEH_BREAKABLE_EXTRA_6",
	"VEH_BREAKABLE_EXTRA_7",
	"VEH_BREAKABLE_EXTRA_8",
	"VEH_BREAKABLE_EXTRA_9",
	"VEH_BREAKABLE_EXTRA_10",

	"VEH_MOD_COLLISION_1",
	"VEH_MOD_COLLISION_2",
	"VEH_MOD_COLLISION_3",
	"VEH_MOD_COLLISION_4",
	"VEH_MOD_COLLISION_5",
	"VEH_MOD_COLLISION_6",
	"VEH_MOD_COLLISION_7",
	"VEH_MOD_COLLISION_8",
	"VEH_MOD_COLLISION_9",
	"VEH_MOD_COLLISION_10",
	"VEH_MOD_COLLISION_11",
	"VEH_MOD_COLLISION_12",
	"VEH_MOD_COLLISION_13",
	"VEH_MOD_COLLISION_14",
	"VEH_MOD_COLLISION_15",
	"VEH_MOD_COLLISION_16",

	"VEH_SPRING_RF",
	"VEH_SPRING_LF",
	"VEH_SPRING_RR",
	"VEH_SPRING_LR"
};

CompileTimeAssert(COUNTOF(CVehicleComponentVisualiser::m_pComponentNames) == COUNTOF(CVehicleComponentVisualiser::m_iComponents) - 1);

CVehicleComponentVisualiser::CVehicleComponentVisualiser()
{

}
CVehicleComponentVisualiser::~CVehicleComponentVisualiser()
{

}
void CVehicleComponentVisualiser::InitWidgets()
{
	bkBank * pBank = BANKMGR.FindBank("A.I.");
	if(!pBank)
		return;

	int iNumComponents = 0;
	while(m_iComponents[iNumComponents++]!=VEH_INVALID_ID) { }

	pBank->PushGroup("Enter Vehicle", false);
	pBank->AddToggle("Draw Convex Hull", &m_bDrawConvexHull);
	pBank->AddToggle("DebugDraw *Ignore* Ped Z", &m_bDebugIgnorePedZ);
	pBank->AddToggle("DebugDraw Box Tests", &m_bDebugDrawBoxTests);
	
	pBank->AddToggle("Draw Specific Component Group", &m_bEnabled);
	//pBank->AddSlider("HierarchyId :", &CVehicleComponentVisualiser::m_iSelectedComponentComboIndex, 0, VEH_NUM_NODES, 1);
	pBank->AddCombo("Component Group :", &CVehicleComponentVisualiser::m_iSelectedComponentComboIndex, iNumComponents-1, m_pComponentNames, NullCB, "Select component to visualise");
	pBank->PushGroup("CTaskMoveGoToVehicleDoor:");
	pBank->AddSlider("ms_fCarMovedEpsSqr", &CTaskMoveGoToVehicleDoor::ms_fCarMovedEpsSqr, 0.0f, 64.0f, 0.01f);
	pBank->AddSlider("ms_fCarReorientatedDotEps", &CTaskMoveGoToVehicleDoor::ms_fCarReorientatedDotEps, 0.0f, 1.0f, 0.01f);
	pBank->AddSlider("ms_fCarChangedSpeedEps", &CTaskMoveGoToVehicleDoor::ms_fCarChangedSpeedDelta, 0.0f, 4.0f, 0.01f);
	pBank->AddSlider("ms_fCheckGoDirectlyToDoorFrequency", &CTaskMoveGoToVehicleDoor::ms_fCheckGoDirectlyToDoorFrequency, 0.0f, 1.0f, 0.1f);
	pBank->AddToggle("m_bUpdateGotoTasksEvenIfCarHasNotMoved", &CTaskMoveGoToVehicleDoor::m_bUpdateGotoTasksEvenIfCarHasNotMoved);
	pBank->AddSeparator();
	pBank->AddSlider("ms_fMinUpdateFreq", &CTaskMoveGoToVehicleDoor::ms_fMinUpdateFreq, 0.0f, 10.0f, 0.1f);
	pBank->AddSlider("ms_fMaxUpdateFreq", &CTaskMoveGoToVehicleDoor::ms_fMaxUpdateFreq, 0.0f, 10.0f, 0.1f);
	pBank->AddSlider("ms_fMinUpdateDist", &CTaskMoveGoToVehicleDoor::ms_fMinUpdateDist, 0.0f, 100.0f, 0.1f);
	pBank->AddSlider("ms_fMaxUpdateDist", &CTaskMoveGoToVehicleDoor::ms_fMaxUpdateDist, 0.0f, 100.0f, 0.1f);
	pBank->PopGroup();
	pBank->PopGroup();
}

void CVehicleComponentVisualiser::Process()
{
	if(!m_bEnabled)
		return;

	if(!m_bInitialised)
	{
		m_bInitialised = true;
	}

	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer)
		return;
	CVehicle * pVehicle = pPlayer->GetPedIntelligence()->GetClosestVehicleInRange();
	if(!pVehicle)
		return;

	eHierarchyId eComponent = (eHierarchyId) m_iComponents[m_iSelectedComponentComboIndex];
	VisualiseComponents(pVehicle, eComponent, true);
}

void CVehicleComponentVisualiser::VisualiseComponents(const CVehicle * pVehicle, eHierarchyId eComponent, bool bDrawBoundingBox)
{
	if(pVehicle->HasComponent(eComponent))
	{
		int iBoneIndex = pVehicle->GetBoneIndex(eComponent);
		//int iVehComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(iBoneIndex);
		VisualiseComponents(pVehicle, iBoneIndex, bDrawBoundingBox);
	}
}
void CVehicleComponentVisualiser::VisualiseComponents(const CVehicle * pVehicle, s32 iBoneIndex, bool bDrawBoundingBox)
{
	if(pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType()!=phBound::COMPOSITE)
		return;

	phBoundComposite * pCompositeBound = (phBoundComposite*)pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
	const int iGroupIndex = pVehicle->GetFragInst()->GetGroupFromBoneIndex(iBoneIndex);

	static const Color32 iDrawCols[] =	// "Richard of york gave battle in vain"
	{
		Color_red, Color_orange, Color_yellow, Color_green, Color_blue, /*Color_indigo*/ Color_navy, Color_violet
	};

	if(iGroupIndex!=-1)
	{
		const fragTypeGroup * pGroup = pVehicle->GetFragInst()->GetTypePhysics()->GetAllGroups()[iGroupIndex];
		const int iFirstChildIndex = pGroup->GetChildFragmentIndex();
		const int iNumChildren = pGroup->GetNumChildren();

		int iDrawCol = 0;

		for(int iChild = iFirstChildIndex ; iChild < iFirstChildIndex+iNumChildren; iChild++)
		{
			const phBound * pSubBound = pCompositeBound->GetBound(iChild);
			if(pSubBound)
			{
				const Matrix34 & mSubBound = RCC_MATRIX34(pCompositeBound->GetCurrentMatrix(iChild));
				DrawPolyhedronBound(pVehicle, pSubBound, mSubBound, iDrawCols[iDrawCol], bDrawBoundingBox);

				iDrawCol++;
				iDrawCol = iDrawCol%7;
			}
		}
	}
}

void CVehicleComponentVisualiser::DrawPolyhedronBound(const CVehicle * pVehicle, const phBound * pBound, const Matrix34 & mat, Color32 iCol, bool bDrawBoundingBox)
{
	// WE need to rended non-geometry bounds here too
	// Notably BVH bounds, as found in the Titan
	if(pBound->GetType()!=phBound::GEOMETRY)
		return;

	Matrix34 mOrientation;
	pVehicle->GetMatrixCopy(mOrientation);
	mOrientation.Dot3x3FromLeft(mat);

	Vector3 vLocalOffset;
	Matrix34 vehMat;
	pVehicle->GetMatrixCopy(vehMat);
	vehMat.Transform3x3(mat.d, vLocalOffset);

	if(bDrawBoundingBox)
		grcDebugDraw::BoxOriented(
			pBound->GetBoundingBoxMin() + VECTOR3_TO_VEC3V(vLocalOffset), 
			pBound->GetBoundingBoxMax() + VECTOR3_TO_VEC3V(vLocalOffset),
			MATRIX34_TO_MAT34V(mOrientation), Color_white, false);

	phBoundPolyhedron * pPolyhedron = (phBoundPolyhedron*) pBound;

	for(int iPoly=0; iPoly<pPolyhedron->GetNumPolygons(); iPoly++)
	{
		const phPolygon & polygon = pPolyhedron->GetPolygon(iPoly);

		int lastv = 2;
		Vector3 v1 = VEC3V_TO_VECTOR3(pPolyhedron->GetVertex(polygon.GetVertexIndex(lastv)));
		mOrientation.Transform(v1);

		for(int v=0; v<3; v++)
		{
			Vector3 v2 = VEC3V_TO_VECTOR3(pPolyhedron->GetVertex(polygon.GetVertexIndex(v)));
			mOrientation.Transform(v2);

			grcDebugDraw::Line(v1 + vLocalOffset, v2 + vLocalOffset, iCol, iCol);

			lastv = v;
			v1 = v2;
		}
	}
}
#endif	// __BANK
