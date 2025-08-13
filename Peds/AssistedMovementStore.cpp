#include "AssistedMovementStore.h"

// Rage includes
#include "Math/angmath.h"
// Framework includes
#include "ai/aichannel.h"
#include "grcore/debugdraw.h"
// R*N includes
#include "Camera/CamInterface.h"
#include "Camera/Debug/DebugDirector.h"
#include "Camera/Helpers/Frame.h"
#include "Control/WaypointRecording.h"
#include "Control/WaypointRecordingRoute.h"
#include "Objects/Door.h"
#include "pathserver/ExportCollision.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Renderer/Water.h"
#include "Task/Default/TaskPlayer.h"


//*****************************************************************************************************

AI_OPTIMISATIONS()

CAssistedMovementToggles::CAssistedMovementToggles()
{
	Init();
}

void CAssistedMovementToggles::Init()
{
	ResetAll(THREAD_INVALID);
}

void CAssistedMovementToggles::ResetAll(const scrThreadId iThreadId)
{
	for(int t=0; t<MAX_NUM_TOGGLES; t++)
	{
		if(iThreadId==0 || iThreadId==m_Toggles[t].m_iScriptThreadId)
		{
			m_Toggles[t].Reset();
		}
	}
	m_bHasChanged = true;
}

bool CAssistedMovementToggles::TurnOnThisRoute(const u32 iRouteNameHash, const scrThreadId iScriptThreadId)
{
	Assertf(iScriptThreadId != 0, "iScriptThreadId cannot be zero!");
	Assertf(iRouteNameHash != 0, "iRouteNameHash cannot be zero!");

	// Only if not already requested
	if(IsThisRouteRequested(iRouteNameHash, NULL))
	{
		return true;
	}

	for(int t=0; t<MAX_NUM_TOGGLES; t++)
	{
		if(m_Toggles[t].m_iScriptThreadId==0)
		{
			m_Toggles[t].m_iRouteNameHash = iRouteNameHash;
			m_Toggles[t].m_iScriptThreadId = iScriptThreadId;

			m_bHasChanged = true;

			return true;
		}
	}

	Assertf(false, "Ran out of space in CAssistedMovementToggles::TurnOnThisRoute(), MAX_NUM_TOGGLES needs increasing");

	return false;
}

bool CAssistedMovementToggles::TurnOffThisRoute(const u32 iRouteNameHash, const scrThreadId ASSERT_ONLY(iScriptThreadId) )
{
	Assertf(iScriptThreadId != 0, "iScriptThreadId cannot be zero!");
	Assertf(iRouteNameHash != 0, "iRouteNameHash cannot be zero!");

	for(int t=0; t<MAX_NUM_TOGGLES; t++)
	{
		if(m_Toggles[t].m_iRouteNameHash == iRouteNameHash)
		{
			Assertf(m_Toggles[t].m_iScriptThreadId==iScriptThreadId, "A different script is turning off an assisted-movement route, to the one which turned it on.");

			m_Toggles[t].m_iRouteNameHash = 0;
			m_Toggles[t].m_iScriptThreadId = THREAD_INVALID;

			m_bHasChanged = true;

			// If the player is attached to this route, then take him off
			// This has to be done explicitly, because the player's assisted-movement control
			// will have its own local copy of the route cached

			CPed* pPlayer = FindPlayerPed();
			if(pPlayer)
			{
				CTaskMovePlayer * pMovePlayerTask = (CTaskMovePlayer*)pPlayer->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_PLAYER);
				if(pMovePlayerTask)
				{
					CPlayerAssistedMovementControl * pCtrl = pMovePlayerTask->GetAssistedMovementControl();
					if(pCtrl)
					{
						if(pCtrl->GetRoute().GetSize()>0 && pCtrl->GetRoute().GetPathStreetNameHash() == iRouteNameHash)
						{
							pCtrl->ClearCurrentRoute();
						}
					}
				}
			}

			return true;
		}
	}

	return false;
}

bool CAssistedMovementToggles::IsThisRouteRequested(const u32 iRouteNameHash, CPathNode * ASSERT_ONLY(pPathNode)) const
{
#if __ASSERT
	if(iRouteNameHash==0)
	{
		if(pPathNode)
		{
			Vector3 vNodeCoords;
			pPathNode->GetCoors(vNodeCoords);
			Assertf(iRouteNameHash!=0, "Assisted node at coordinates (%.1f, %.1f, %1.f) has no name", vNodeCoords.x, vNodeCoords.y, vNodeCoords.z);
		}
		else
		{
			Assertf(iRouteNameHash!=0, "iRouteNameHash cannot be zero!\nA script was trying to turn on an assisted-movement route, using an empty string name");
		}
	}
#endif

	for(int t=0; t<MAX_NUM_TOGGLES; t++)
	{
		if(m_Toggles[t].m_iRouteNameHash == iRouteNameHash)
		{
			return true;
		}
	}

	return false;
}


//*****************************************************************************************************


CAssistedMovementRoute * CAssistedMovementRouteStore::ms_RouteStore = NULL;
const float CAssistedMovementRoute::ms_fDefaultPathWidth = 0.75f;
const float CAssistedMovementRoute::ms_fDefaultPathTension = 0.5f;
const float CAssistedMovementRoute::ms_fTangentLerpMaxDist = 4.0f;

CAssistedMovementRoute::CAssistedMovementRoute()
{
	Init();
}

void CAssistedMovementRoute::Init()
{
	m_vMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	m_vMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	m_iSize = 0;
	m_iFlags = 0;
	m_fLength = 0.0f;
	m_iPathStreetNameHash = 0;
	m_iRouteType = ROUTETYPE_NONE;
}

// Calculate total length, tangents & min/max
void CAssistedMovementRoute::PostProcessRoute()
{
	s32 t;

	// Store tension values from 'm_vTangent.w'
	float fStoredTensionValues[MAX_NUM_ROUTE_ELEMENTS];
	for(t=0; t<MAX_NUM_ROUTE_ELEMENTS; t++)
		fStoredTensionValues[t] = m_Points[t].m_vTangent.w;

	AssertMsg(!(m_iFlags & RF_HasBeenPostProcessed), "CAssistedMovementRoute::PostProcessRoute() - already done for this route!");

	m_iFlags |= RF_HasBeenPostProcessed;

	Assertf(m_iSize != 0, "CAssistedMovementRoute::PostProcessRoute() - bogus route, has no elements (how can this happen?)\nm_iRouteType=%i, m_iPathStreetNameHash=%u, Min(%.2f,%.2f,%.2f), Max(%.2f,%.2f,%.2f)",
		m_iRouteType, m_iPathStreetNameHash, m_vMin.x, m_vMin.y, m_vMin.z, m_vMax.x, m_vMax.y, m_vMax.z);
	if(m_iSize==0)
		return;

	Assertf(m_iSize > 1, "CAssistedMovementRoute::PostProcessRoute() - bogus route, only has one element at (%.2f, %.2f, %.2f)\nm_iRouteType=%i, m_iPathStreetNameHash=%u, Min(%.2f,%.2f,%.2f), Max(%.2f,%.2f,%.2f)",
		m_Points[0].m_vPos.x, m_Points[0].m_vPos.y, m_Points[0].m_vPos.z, m_iRouteType, m_iPathStreetNameHash, m_vMin.x, m_vMin.y, m_vMin.z, m_vMax.x, m_vMax.y, m_vMax.z);
	if(m_iSize<=1)
		return;

	int i;

	// Length
	m_fLength = 0.0f;
	for(i=1; i<m_iSize; i++)
	{
		m_fLength += (m_Points[i].m_vPos - m_Points[i-1].m_vPos).Mag();
	}
	// Min/max
	m_vMin = Vector3(FLT_MAX,FLT_MAX,FLT_MAX);
	m_vMax = Vector3(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	for(i=0; i<m_iSize; i++)
	{
		const Vector3 & vPos = m_Points[i].m_vPos;
		m_vMin.x = Min(m_vMin.x, vPos.x);
		m_vMin.y = Min(m_vMin.y, vPos.y);
		m_vMin.z = Min(m_vMin.z, vPos.z);
		m_vMax.x = Max(m_vMax.x, vPos.x);
		m_vMax.y = Max(m_vMax.y, vPos.y);
		m_vMax.z = Max(m_vMax.z, vPos.z);
	}
	// Tangents
	// Start & end are always directly along their respective segments
	m_Points[0].m_vTangent = m_Points[1].m_vPos - m_Points[0].m_vPos;
	m_Points[0].m_vTangent.Normalize();
	m_Points[m_iSize-1].m_vTangent = m_Points[m_iSize-1].m_vPos - m_Points[m_iSize-2].m_vPos;
	m_Points[m_iSize-1].m_vTangent.Normalize();

	// All points in between are averages of their adjacent segments.
	// We'll interpolate these when getting a point along the route.
	for(i=1; i<m_iSize-1; i++)
	{
		Vector3 vThisSeg = m_Points[i].m_vPos - m_Points[i-1].m_vPos;
		Vector3 vPrevSeg = m_Points[i+1].m_vPos - m_Points[i].m_vPos;
		vThisSeg.Normalize();
		vPrevSeg.Normalize();

		m_Points[i].m_vTangent = (vThisSeg + vPrevSeg);
		if( (m_PointFlags[i-1] & RPF_IsUnderwater)!=0 || (m_PointFlags[i] & RPF_IsUnderwater)!=0 || (m_PointFlags[i+1] & RPF_IsUnderwater)!=0 )
		{
			// For route sections which operate in 3 dimensions, preserve the Z component (eg. underwater assists)
		}
		else
		{
			m_Points[i].m_vTangent.z = 0.0f;
		}
		
		m_Points[i].m_vTangent.Normalize();
	}	

	// Restore tension values in 'm_vTangent.w'
	for(t=0; t<MAX_NUM_ROUTE_ELEMENTS; t++)
		m_Points[t].m_vTangent.w = fStoredTensionValues[t];
}

Vector3 CAssistedMovementRoute::GetPathBoundaryPoint(const s32 p, const float fPathWidth)
{
	if(m_iSize <= 1)
		return Vector3(0.0f,0.0f,0.0f);

	if(p==0 || p==m_iSize-1)
	{
		Vector3 vSeg = (p==0) ? m_Points[1].m_vPos - m_Points[0].m_vPos : m_Points[m_iSize-1].m_vPos - m_Points[m_iSize-2].m_vPos;
		vSeg.Normalize();
		Vector3 vTangent = CrossProduct(vSeg, ZAXIS);
		vTangent.z = 0.0f;
		vTangent.Normalize();
		vTangent *= fPathWidth;
		return m_Points[p].m_vPos + vTangent;
	}
	else
	{
		Vector3 vSeg1 = m_Points[p].m_vPos - m_Points[p-1].m_vPos;
		Vector3 vSeg2 = m_Points[p+1].m_vPos - m_Points[p].m_vPos;
		vSeg1.Normalize();
		vSeg2.Normalize();
		const Vector3 vTangent1 = CrossProduct(vSeg1, ZAXIS);
		const Vector3 vTangent2 = CrossProduct(vSeg2, ZAXIS);
		Vector3 vAverageTangent = vTangent1 + vTangent2;
		vAverageTangent.z = 0.0f;
		vAverageTangent.Normalize();
		vAverageTangent *= fPathWidth;
		return m_Points[p].m_vPos + vAverageTangent;
	}
}

bool CAssistedMovementRoute::GetClosestPos(const Vector3 & vSrcPos, int & iOutRouteSegment, float & fOutRouteSegmentProgress, Vector3 & vOutPosOnRoute, Vector3 & vOutRouteNormal, Vector3 & vOutTangent, u32 & iOutFlags) const
{
	float fClosestDistSqrToPlayer = FLT_MAX;
	iOutRouteSegment = -1;

	for(int p=0; p<m_iSize-1; p++)
	{
		const Vector3 & vPos = GetPoint(p);
		const Vector3 & vVec = GetPoint(p+1) - vPos;
		const float fTVal = (vVec.Mag2() > 0.0f) ? geomTValues::FindTValueSegToPoint(vPos, vVec, vSrcPos) : 0.0f;
		const Vector3 vClosePos = vPos + (vVec * fTVal);
		const Vector3 vDelta = vClosePos - vSrcPos;
		if(Abs(vDelta.z) < 1.5f)
		{
			const float fDistSqr = vDelta.Mag2();
			if(fDistSqr < fClosestDistSqrToPlayer)
			{
				vOutPosOnRoute = vClosePos;
				vOutRouteNormal = vVec;
				vOutRouteNormal.NormalizeSafe();
				fClosestDistSqrToPlayer = fDistSqr;
				iOutRouteSegment = p;
				fOutRouteSegmentProgress = fTVal;
				iOutFlags = GetPointFlags(p);
			}
		}
	}
	if(iOutRouteSegment != -1)
	{
		vOutTangent =
			(m_Points[iOutRouteSegment].m_vTangent * (1.0f-fOutRouteSegmentProgress)) +
			(m_Points[iOutRouteSegment+1].m_vTangent * fOutRouteSegmentProgress);
		vOutTangent.Normalize();
	}

	return (iOutRouteSegment != -1);
}

float CAssistedMovementRoute::GetClosestPos(const Vector3 & vSrcPos, Vector3 * vOutPosOnRoute, Vector3 * vOutRouteNormal, Vector3 * vOutTangent, float * fOutTension, u32 * iOutFlags) const
{
	float fOutDistIntoRoute = -1.0f;

	float fClosestDistSqrToPlayer = FLT_MAX;
	float fClosestTVal = 0.0f;
	int iClosestSegment = -1;

	float fDistSoFar = 0.0f;
	for(int p=0; p<m_iSize-1; p++)
	{
		const Vector3 & vPos = GetPoint(p);
		const Vector3 & vVec = GetPoint(p+1) - vPos;
		const float fSegLength = vVec.Mag();
		const float fTVal = (fSegLength > 0.0f) ? geomTValues::FindTValueSegToPoint(vPos, vVec, vSrcPos) : 0.0f;
		const Vector3 vClosePos = vPos + (vVec * fTVal);
		const Vector3 vDelta = vClosePos - vSrcPos;

		if(Abs(vDelta.z) < 1.5f)
		{
			const float fDistSqr = vDelta.Mag2();
			if(fDistSqr < fClosestDistSqrToPlayer)
			{
				if(vOutPosOnRoute)
					*vOutPosOnRoute = vClosePos;
				if(vOutRouteNormal)
				{
					*vOutRouteNormal = vVec;
					vOutRouteNormal->NormalizeSafe();
				}
				fClosestDistSqrToPlayer = fDistSqr;

				iClosestSegment = p;
				fClosestTVal = fTVal;

				fOutDistIntoRoute = fDistSoFar + (fSegLength * fTVal);

				if(iOutFlags)
					*iOutFlags = GetPointFlags(p);
			}
		}

		fDistSoFar += fSegLength;
	}
	if(iClosestSegment != -1 && vOutTangent)
	{
		*vOutTangent =
			(m_Points[iClosestSegment].m_vTangent * (1.0f-fClosestTVal)) +
			(m_Points[iClosestSegment+1].m_vTangent * fClosestTVal);
		vOutTangent->Normalize();
	}
	if(iClosestSegment != -1 && fOutTension)
	{
		*fOutTension =
			(GetPointTension(iClosestSegment) * (1.0f-fClosestTVal)) +
			(GetPointTension(iClosestSegment+1) * fClosestTVal);
	}

	return fOutDistIntoRoute;
}

bool CAssistedMovementRoute::GetAtDistAlong(float fInputDist, Vector3 * vOutPosOnRoute, Vector3 * vOutRouteNormal, Vector3 * vOutTangent, float * fOutTension) const
{
	if(fInputDist < 0.0f || fInputDist > m_fLength)
		return false;

	for(int p=0; p<m_iSize-1; p++)
	{
		const Vector3 & vPos = GetPoint(p);
		const Vector3 & vVec = GetPoint(p+1) - vPos;
		const float fSegLength = vVec.Mag();

		if(fSegLength > 0.0f && fInputDist < fSegLength)
		{
			const float fTVal = fInputDist / fSegLength;

			if(vOutPosOnRoute)
				*vOutPosOnRoute = vPos + (vVec * fTVal);

			if(vOutRouteNormal)
			{
				*vOutRouteNormal = vVec;
				vOutRouteNormal->Normalize();
			}

			if(vOutTangent)
			{
				*vOutTangent =
					(m_Points[p].m_vTangent * (1.0f-float(p))) +
					(m_Points[p+1].m_vTangent * float(p));
				*vOutTangent *= 0.5f;
				vOutTangent->Normalize();
			}

			if(fOutTension)
			{
				*fOutTension =
					(GetPointTension(p) * (1.0f-p)) +
					(GetPointTension(p+1) * p);
			}
			return true;
		}

		fInputDist -= fSegLength;
	}

	return false;
}


void CAssistedMovementRouteStore::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
		Assert(ms_RouteStore == NULL);
		ms_RouteStore = rage_new CAssistedMovementRoute[MAX_ROUTES];
		Assert(ms_RouteStore);

	    CAssistedMovementRouteStore::ClearAll(CAssistedMovementRoute::ROUTEYYPE_ANY);
	    m_vLastOrigin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);

		m_RouteToggles.Init();
    }
}

void CAssistedMovementRouteStore::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
	    CAssistedMovementRouteStore::ClearAll(CAssistedMovementRoute::ROUTEYYPE_ANY);
	    m_vLastOrigin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);

		Assert(ms_RouteStore);
		delete[] ms_RouteStore;
		ms_RouteStore = NULL;
    }
}

const float CAssistedMovementRouteStore::ms_fDefaultStreamingLoadDistance = 50.0f;
float CAssistedMovementRouteStore::ms_fStreamingLoadDistance = CAssistedMovementRouteStore::ms_fDefaultStreamingLoadDistance;
float CAssistedMovementRouteStore::ms_fStreamingUpdatePosDeltaSqr = (20.0f*20.0f);
bank_float CAssistedMovementRouteStore::ms_fDistOutFromDoor = 1.0f;
bank_bool CAssistedMovementRouteStore::ms_bAutoGenerateRoutesInDoorways = false;
Vector3 CAssistedMovementRouteStore::m_vLastOrigin(FLT_MAX, FLT_MAX, FLT_MAX);
int CAssistedMovementRouteStore::ms_iScriptRouteEditIndex = -1;
CAssistedMovementToggles CAssistedMovementRouteStore::m_RouteToggles;

void CAssistedMovementRouteStore::Process(CPed * pPed, const bool bForceUpdate)
{
	if(fwTimer::IsGamePaused())
		return;

	const Vector3 vOrigin = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	//-----------------------------------------------------------------------------------
	// Hack streaming load distance per-frame if required
	// ms_fStreamingLoadDistance is reset at the end of UpdateRoutesFromNodesSystem()

	const Vector3 vPowerPlantAABB[2] =
	{
		Vector3(2635.0f, 1364.0f, 15.0f),
		Vector3(2856.0f, 1731.0f, 78.0f)
	};

	if( vOrigin.IsGreaterOrEqualThan(vPowerPlantAABB[0]) && vPowerPlantAABB[1].IsGreaterOrEqualThan(vOrigin))
	{
		ms_fStreamingLoadDistance = 20.0f;	// url:bugstar:1496887
	}

	//-----------------------------------------------------------------------------------

	ms_fStreamingUpdatePosDeltaSqr = square( ms_fStreamingLoadDistance / 2.5f );

	const Vector3 vDiff = m_vLastOrigin - vOrigin;
	if(vDiff.Mag2() > ms_fStreamingUpdatePosDeltaSqr || bForceUpdate || m_RouteToggles.GetHasChanged())
	{
		m_vLastOrigin = vOrigin;
		UpdateRoutesFromNodesSystem(vOrigin);
		UpdateRoutesFromWaypointRecordings(vOrigin);

		m_RouteToggles.SetHasChanged(false);
	}
}

bool CAssistedMovementRouteStore::ShouldProcessNode(CPathNode * pNode)
{
	return (IsPermanentRoute(pNode) || m_RouteToggles.IsThisRouteRequested(pNode->m_streetNameHash, pNode) || CPlayerAssistedMovementControl::ms_bLoadAllRoutes);
}

void CAssistedMovementRouteStore::UpdateRoutesFromNodesSystem(const Vector3 & vOrigin)
{
	int n;

	//*********************************************************************************************
	// Store off the flags which have been set on this route by script.
	// Mask off those which we don't wish to preserve. 
	// After scanning for routes we'll copy the flags back onto the new routs where appropriate

	u32 iHashFlags[MAX_MAP_ROUTES][2];

	for(s32 h=0; h<MAX_MAP_ROUTES; h++)
	{
		CAssistedMovementRoute * pRoute = GetRoute(FIRST_MAP_ROUTE+h);
		iHashFlags[h][0] = pRoute->GetPathStreetNameHash();
		iHashFlags[h][1] = pRoute->GetFlags();

		// Mask unwanted flags
		iHashFlags[h][1] &= ~CAssistedMovementRoute::RF_HasBeenPostProcessed;
	}

	//*******************************************************************************************
	// Go through the route store, and remove those which are outside of the streaming extents

	ClearAll(CAssistedMovementRoute::ROUTETYPE_MAP);

	//*****************************************************************************************
	// Obtain a list of the nearby 'assisted-movement' nodes from the CPathFind nodes network

	CPathNode * nearbyAssistedMovementNodes[MAX_MAP_ROUTES];
	int iNumNodes = ThePaths.FindAssistedMovementRoutesInArea(vOrigin, ms_fStreamingLoadDistance, nearbyAssistedMovementNodes, MAX_MAP_ROUTES);

	//*******************************************************************************************
	// Now iterate along each of the routes, and create a list of waypoints and associated data.

	Vector3 vWpt;

	for(n=0; n<iNumNodes; n++)
	{
		CPathNode * pStartNode = nearbyAssistedMovementNodes[n];

		if(!pStartNode)
			continue;

		// We've already determined this to be true within CPathFind::FindAssistedMovementRoutesInArea
		Assert( ShouldProcessNode(pStartNode) );

		{
			CAssistedMovementRoute * pRoute = GetEmptyRoute(true, false, false);
			if(!pRoute)
			{
				//******************************************************************************************
				// At this point we may wish to evict an auto-generated door route from the map pool if we
				// have any, since we don't wish door routes to prevent authored map-routes from appearing.

				break;
			}

			pRoute->Clear();
			pRoute->SetRouteType(CAssistedMovementRoute::ROUTETYPE_MAP);

			
			const CPathNode * pNode = pStartNode;
			const CPathNode * pPrevNode = NULL;

			aiAssertf(pNode, "Assisted movement start node not found!");
			aiAssertf(pNode->NumLinks()==1, "Only 1 adjacent node supported, probably a data error!");

			pRoute->m_iPathStreetNameHash = pStartNode->m_streetNameHash;

			// Restore flags?
			for(s32 h=0; h<MAX_MAP_ROUTES; h++)
			{
				if(iHashFlags[h][0] && iHashFlags[h][0]==pRoute->GetPathStreetNameHash())
				{
					pRoute->SetFlags(iHashFlags[h][1]);
					break;
				}
			}

			// Iterate nodes
			bool bEndReached = false;

			while(pNode)
			{
				pNode->GetCoors(vWpt);

				Assertf(pNode->m_1.m_specialFunction == SPECIAL_PED_ASSISTED_MOVEMENT, "Assisted movement route at (%.1f, %.1f, %.1f) needs to have *all* its nodes marked as SPECIAL_PED_ASSISTED_MOVEMENT", vWpt.x, vWpt.y, vWpt.z);

				Assertf(!(IsPermanentRoute(pStartNode) && !IsPermanentRoute(pNode)), "Assisted Movement route at (%.1f, %.1f, %.1f) isn't all marked as permanent nodes", vWpt.x, vWpt.y, vWpt.z);
				Assertf(!(IsPermanentRoute(pNode) && !IsPermanentRoute(pStartNode)), "Assisted Movement route at (%.1f, %.1f, %.1f) isn't all marked as permanent nodes", vWpt.x, vWpt.y, vWpt.z);

				// Node positions are snapped to the floor
				// If this node is not underwater, then add a meter to it
				
				float fWaterZ;
				const bool bUnderwater =
					Water::GetWaterLevelNoWaves(vWpt, &fWaterZ, 0.0f, 0.0f, NULL) && fWaterZ > vWpt.z;

				if(!bUnderwater)
				{
					vWpt.z += 1.0f;			
				}

				int iLinkWidth = 1;
				int iNodeDensity = 0;
				bool bLinkNoLeave = false;

				aiAssertf(pNode->NumLinks() > 0 && pNode->NumLinks() <= 2, "Only 1 or 2 links allowed on Assisted Movment routes (node position is : %.2f, %.2f, %.2f) ", vWpt.x, vWpt.y, vWpt.z);
				if(pNode->NumLinks() == 0 && pNode->NumLinks() > 2)
					return;

				if(pRoute->GetSize() >= CAssistedMovementRoute::MAX_NUM_ROUTE_ELEMENTS)
				{
					aiAssertf(false, "Assisted movement route in world exceeds CAssistedMovementRoute::MAX_NUM_ROUTE_ELEMENTS!");
					break;
				}

				// Are we at the start of the route?
				if(pNode==pStartNode)
				{
					pPrevNode = pNode;
					CNodeAddress adjNodeAddr = ThePaths.GetNodesLinkedNodeAddr(pNode, 0);
					const CPathNodeLink & link = ThePaths.GetNodesLink(pNode, 0);
					iLinkWidth = link.m_1.m_Width;
					bLinkNoLeave = link.m_1.m_NarrowRoad;
					iNodeDensity = pNode->m_2.m_density;

					pNode = ThePaths.FindNodePointerSafe(adjNodeAddr);
				}
				// End of the link?
				else if(pNode->NumLinks()==1)
				{
					bEndReached = true;

					/// CNodeAddress adjNodeAddr = ThePaths.GetNodesLinkedNodeAddr(pNode, 0);
					const CPathNodeLink & link = ThePaths.GetNodesLink(pNode, 0);
					iLinkWidth = link.m_1.m_Width;
					bLinkNoLeave = link.m_1.m_NarrowRoad;
					iNodeDensity = pNode->m_2.m_density;
				}
				// Middle of the link?
				else
				{
					aiAssertf(pNode->NumLinks() == 2, "Assisted Movment node must have 2 links at this point (node pos is : %.2f, %.2f, %.3f) ", vWpt.x, vWpt.y, vWpt.z);

					CNodeAddress adjNodeAddr1 = ThePaths.GetNodesLinkedNodeAddr(pNode, 0);
					const CPathNode * pLinkedNode1 = ThePaths.FindNodePointerSafe(adjNodeAddr1);
					const CPathNodeLink & link1 = ThePaths.GetNodesLink(pNode, 0);

					CNodeAddress adjNodeAddr2 = ThePaths.GetNodesLinkedNodeAddr(pNode, 1);
					const CPathNode * pLinkedNode2 = ThePaths.FindNodePointerSafe(adjNodeAddr2);
					const CPathNodeLink & link2 = ThePaths.GetNodesLink(pNode, 1);

					if(!pLinkedNode1 || !pLinkedNode2 || !pPrevNode)
					{
						// Route not entirely streamed in
						pRoute->Clear();
						break;
					}

					aiAssertf(pLinkedNode1->m_address==pPrevNode->m_address || pLinkedNode2->m_address==pPrevNode->m_address, "Incorrect node address!");

					iNodeDensity = pNode->m_2.m_density;

					const CPathNode * pTmpNode = pNode;
					pNode = (pLinkedNode1->m_address==pPrevNode->m_address) ? pLinkedNode2 : pLinkedNode1;
					pPrevNode = pTmpNode;

					const CPathNodeLink & link = (pLinkedNode1->m_address==pPrevNode->m_address) ? link2 : link1;
					iLinkWidth = link.m_1.m_Width;
					bLinkNoLeave = link.m_1.m_NarrowRoad;
				}

				// If pNode is NULL, it implies that not all the pathfind regions are loaded for this route
				// In this case we should abort the creation of this route as it would be incomplete.
				if(!pNode)
				{
					pRoute->Clear();
					break;
				}

#if __DEV
				if(pNode==pStartNode)
				{
					Warningf("Path nodes error : a circular assisted-movement path was found");
					break;
				}
#endif

				u32 iFlags = 0;

				if(bLinkNoLeave)
					iFlags |= CAssistedMovementRoute::RPF_ReduceSpeedForCorners;
				if(bUnderwater)
					iFlags |= CAssistedMovementRoute::RPF_IsUnderwater;

				float fTension;

				// Get the tension value from the node.
				if(iNodeDensity==7)
				{
					// The value 0.5f in the 3dsMax plugin, given an inter m_density of 7.
					// This resolves to 0.9333333; but we want it to be *exactly* 1.0f
					fTension = 1.0f;	
				}
				else
				{
					// For other values we expand to the range 0.0f -> 2.0f for our purposes
					fTension = (((float)iNodeDensity) / 15.0f);
					Assert(fTension >= 0.0f && fTension <= 1.0f);
					fTension = Clamp(fTension, 0.0f, 1.0f);
				}

				iLinkWidth = Max(iLinkWidth, 1);
				const float fLinkWidth = ((float)iLinkWidth) * (FindPlayerPed()->GetCapsuleInfo()->GetHalfWidth() * 2.0f);

				pRoute->AddPoint(vWpt, fLinkWidth, iFlags, fTension);

				if(bEndReached || !pNode)
					break;
			}

			// If zero elements, then something went wrong during route setup
			// Ensure route is clear so that it may be reused
			if(pRoute->GetSize()==0)
			{
				pRoute->Clear();
			}

			// Continue as normal
			else
			{
				pRoute->PostProcessRoute();

				// Now disable any ROUTETYPE_DOOR routes which overlap with this one
				const Vector3 & vRouteMin = pRoute->GetMin();
				const Vector3 & vRouteMax = pRoute->GetMax();

				for(int r=0; r<MAX_ROUTES; r++)
				{
					if(pRoute != &ms_RouteStore[r] && ms_RouteStore[r].GetRouteType()==CAssistedMovementRoute::ROUTETYPE_DOOR)
					{
						const Vector3 & vDoorMin = ms_RouteStore[r].GetMin() - Vector3(ms_fDistOutFromDoor, ms_fDistOutFromDoor, ms_fDistOutFromDoor);
						const Vector3 & vDoorMax = ms_RouteStore[r].GetMax() + Vector3(ms_fDistOutFromDoor, ms_fDistOutFromDoor, ms_fDistOutFromDoor);

						if(vDoorMax.x < vRouteMin.x || vDoorMax.y < vRouteMin.y || vDoorMax.z < vRouteMin.z ||
							vRouteMax.x < vDoorMin.x || vRouteMax.y < vDoorMin.y || vRouteMax.z < vDoorMin.z)
						{
							// Disjoint
						}
						else
						{
							// Overlap
							ms_RouteStore[r].Clear();
						}
					}
				}
			}
		}
	}

	ms_fStreamingLoadDistance = ms_fDefaultStreamingLoadDistance;
}

bool CAssistedMovementRouteStore::IsPermanentRoute(const CPathNode * pNode)
{
	// We've overridden the bHighway flag in the pathnode..
	return pNode->IsHighway();
}

void CAssistedMovementRouteStore::UpdateRoutesFromWaypointRecordings(const Vector3 & UNUSED_PARAM(vOrigin))
{
	// Store off existing flags which have been set on waypoint recording routes
	struct TExistingRoute
	{
		u32 iHashKey;
		u32 iFlags;

	} existingRoutes[MAX_WAYPOINT_ROUTES];

	int i;
	int iNumExistingRoutes = 0;

	for(i=FIRST_WAYPOINT_ROUTE; i<FIRST_WAYPOINT_ROUTE+MAX_WAYPOINT_ROUTES; i++)
	{
		CAssistedMovementRoute * pRoute = GetRoute(i);
		existingRoutes[iNumExistingRoutes].iHashKey = pRoute->GetPathStreetNameHash();
		existingRoutes[iNumExistingRoutes].iFlags = pRoute->GetFlags();
		iNumExistingRoutes++;
	}

	ClearAll(CAssistedMovementRoute::ROUTETYPE_WAYPOINTRECORDING);

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
	const float fMaxDistSqr = 50.0f*50.0f;

	//***********************************************************************************
	// Find all the waypoint recordings nearby which have the Flag_AssistedMovement flag

	for(int w=0; w<CWaypointRecording::ms_iMaxNumLoaded; w++)
	{
		::CWaypointRecordingRoute * pWptRoute = CWaypointRecording::GetRecordingBySlotIndex(w);

		if(pWptRoute && (pWptRoute->GetFlags() & ::CWaypointRecordingRoute::RouteFlag_AssistedMovement))
		{
			//**********************************************************
			// Find a destination assisted-movement route for this

			CAssistedMovementRoute * pRoute = GetEmptyRoute(false, false, true);

			if(pRoute)
			{
				const CLoadedWaypointRecording * pRecordingInfo = CWaypointRecording::GetLoadedWaypointRecordingInfo(pWptRoute);

				//***********************************************************************
				// Travel along the route, and find the closest position to the player.
				// Then add points either side of this position, up until the maximum
				// for this assisted-movement route.
				// Yes - this is a grossly inefficient algorithm, but its not like this
				// is going to be done alot in the game.

				float fNearestDistSqr = FLT_MAX;
				int iNearest = -1;

				for(int p=0; p<pWptRoute->GetSize(); p++)
				{
					const ::CWaypointRecordingRoute::RouteData & wpt = pWptRoute->GetWaypoint(p);
					const Vector3 vPos = wpt.GetPosition();

					const float fDistSqr = (vPedPos - vPos).Mag2();
					if(fDistSqr < fMaxDistSqr && fDistSqr < fNearestDistSqr)
					{
						fNearestDistSqr = fDistSqr;
						iNearest = p;
					}
				}

				//***************************************************************************
				// If we have a waypoint within range, then 'stream' the section of waypoint
				// route into this assisted-movement route such that this closest waypoint
				// if near the middle of the assisted-movement route.

				if(iNearest != -1)
				{
					int iCurrent = Max(iNearest - CAssistedMovementRoute::MAX_NUM_ROUTE_ELEMENTS / 2, 0);

					for(int c=0; c<CAssistedMovementRoute::MAX_NUM_ROUTE_ELEMENTS; c++)
					{
						// Reached the end of the waypoint route?
						if(iCurrent >= pWptRoute->GetSize())
							break;

						const ::CWaypointRecordingRoute::RouteData & wpt = pWptRoute->GetWaypoint(iCurrent);

						// Waypoint routes have their base on the ground, so raise up to waist height
						Vector3 vRoutePos = wpt.GetPosition();
						vRoutePos.z += 1.0f;

						pRoute->AddPoint(vRoutePos, pRecordingInfo->GetAssistedMovementPathWidth(), 0, pRecordingInfo->GetAssistedMovementPathTension());

						iCurrent++;
					}

					const CLoadedWaypointRecording * pInfo = CWaypointRecording::GetLoadedWaypointRecordingInfo(w);
					Assertf(pInfo, "How can we have a waypoint-recording pointer, but no loaded info?");

					pRoute->SetPathStreetNameHash(pInfo->GetHashKey());
					pRoute->SetRouteType(CAssistedMovementRoute::ROUTETYPE_WAYPOINTRECORDING);
					pRoute->PostProcessRoute();

					// Merge the existing flags we had for this route - this is so that script-set flags will persist
					for(i=0; i<iNumExistingRoutes; i++)
					{
						if( existingRoutes[i].iHashKey == pRoute->GetPathStreetNameHash() )
						{
							pRoute->SetFlags( pRoute->GetFlags() | existingRoutes[i].iFlags );
						}
					}
				}
			}
		}
	}
}

//******************************************************************************************
// AddRouteForDoor
// Creates a small assisted-movement route for this door object, and adds it to the store.
// The system identifies the route by the dynamic-object ID of this door within the navmesh
// system.  If this is not a door, or is not a dynamic-object in the navmesh, then no
// action is taken.

void CAssistedMovementRouteStore::MaybeAddRouteForDoor(CEntity * pEntity)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
		return;
#endif

	if(!ms_bAutoGenerateRoutesInDoorways)
	{
		return;
	}
	else
	{
		if(!pEntity->GetIsTypeObject())
			return;
		if (!static_cast<CObject*>(pEntity)->IsADoor())
			return;

		CDoor * pDoor = static_cast<CDoor*>(pEntity);

		CBaseModelInfo * pModelInfo = pDoor->GetBaseModelInfo();

		const bool bOpenableDoor =
			pModelInfo->GetUsesDoorPhysics() && !pDoor->IsBaseFlagSet(fwEntity::IS_FIXED) && pDoor->GetDoorType()!=CDoor::DOOR_TYPE_GARAGE;

		if(!bOpenableDoor || !GetIsValidPathServerDynamicObjectIndex(pDoor->GetPathServerDynamicObjectIndex()))
			return;

		const Vector3 vDoorPosition = VEC3V_TO_VECTOR3(pDoor->GetTransform().GetPosition());
		// Ensure that this will not overlap with any existing map/scripted routes
		const Vector3 vDoorMin = vDoorPosition - Vector3(ms_fDistOutFromDoor, ms_fDistOutFromDoor, ms_fDistOutFromDoor);
		const Vector3 vDoorMax = vDoorPosition + Vector3(ms_fDistOutFromDoor, ms_fDistOutFromDoor, ms_fDistOutFromDoor);

		for(int r=0; r<MAX_ROUTES; r++)
		{
			if(ms_RouteStore[r].GetRouteType()==CAssistedMovementRoute::ROUTETYPE_MAP ||
				ms_RouteStore[r].GetRouteType()==CAssistedMovementRoute::ROUTETYPE_SCRIPT)
			{
				const Vector3 & vRouteMin = ms_RouteStore[r].GetMin();
				const Vector3 & vRouteMax = ms_RouteStore[r].GetMax();

				if(vDoorMax.x < vRouteMin.x || vDoorMax.y < vRouteMin.y || vDoorMax.z < vRouteMin.z ||
					vRouteMax.x < vDoorMin.x || vRouteMax.y < vDoorMin.y || vRouteMax.z < vDoorMin.z)
				{
					// Disjoint
					continue;
				}
				return;
			}
		}

		// Find an empty route in the map pool (don't want to overwrite any scripted routes with this..)
		CAssistedMovementRoute * pRoute = GetEmptyRoute(true, false, false);
		if(!pRoute)
			return;

		pRoute->Clear();
		pRoute->SetRouteType(CAssistedMovementRoute::ROUTETYPE_DOOR);

		CEntityBoundAI boundAI(*pDoor, vDoorPosition.z, FindPlayerPed()->GetCapsuleInfo()->GetHalfWidth(), true);
		Vector3 vBoundVerts[4];
		boundAI.GetCorners(vBoundVerts);

		const Vector3 vDoorFront = (vBoundVerts[CEntityBoundAI::FRONT_LEFT] + vBoundVerts[CEntityBoundAI::REAR_LEFT]) * 0.5f;
		const Vector3 vDoorRear = (vBoundVerts[CEntityBoundAI::FRONT_RIGHT] + vBoundVerts[CEntityBoundAI::REAR_RIGHT]) * 0.5f;

		const Vector3 vDoorLeft = (vBoundVerts[CEntityBoundAI::FRONT_LEFT] + vBoundVerts[CEntityBoundAI::FRONT_RIGHT]) * 0.5f;
		const Vector3 vDoorRight = (vBoundVerts[CEntityBoundAI::REAR_LEFT] + vBoundVerts[CEntityBoundAI::REAR_RIGHT]) * 0.5f;
		const float fDoorWidth = (vDoorRight - vDoorLeft).XYMag();

		Vector3 vDoorVec = vDoorFront - vDoorRear;
		vDoorVec.Normalize();

		static dev_float fPathWidthMultiplier = 0.4f;

		pRoute->AddPoint(vDoorFront + (vDoorVec * ms_fDistOutFromDoor), fDoorWidth * fPathWidthMultiplier);
		pRoute->AddPoint(vDoorRear - (vDoorVec * ms_fDistOutFromDoor), fDoorWidth * fPathWidthMultiplier);

		const u32 iDoorHash = CalcDoorHash(pDoor);
		pRoute->SetPathStreetNameHash(iDoorHash);
		pRoute->PostProcessRoute();
	}
}

void CAssistedMovementRouteStore::MaybeRemoveRouteForDoor(CEntity * pEntity)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
		return;
#endif

	if(!ms_bAutoGenerateRoutesInDoorways)
	{
		return;
	}
	else
	{
		if(!pEntity->GetIsTypeObject())
			return;
		if (!static_cast<CObject*>(pEntity)->IsADoor())
			return;

		CDoor * pDoor = static_cast<CDoor*>(pEntity);

		CBaseModelInfo * pModelInfo = pDoor->GetBaseModelInfo();

		const bool bOpenableDoor =
			pModelInfo->GetUsesDoorPhysics() && !pDoor->IsBaseFlagSet(fwEntity::IS_FIXED) && pDoor->GetDoorType()!=CDoor::DOOR_TYPE_GARAGE;

		if(!bOpenableDoor || !GetIsValidPathServerDynamicObjectIndex(pDoor->GetPathServerDynamicObjectIndex()))
			return;

		const u32 iDoorHash = CalcDoorHash(pDoor);

		for(int r=0; r<MAX_ROUTES; r++)
		{
			if(ms_RouteStore[r].GetPathStreetNameHash()==iDoorHash && ms_RouteStore[r].GetRouteType()==CAssistedMovementRoute::ROUTETYPE_DOOR)
			{
				ms_RouteStore[r].Clear();
				return;
			}
		}
	}
}

u32 CAssistedMovementRouteStore::CalcDoorHash(CObject * pDoor)
{
	char routeName[32];
	const u32 iObjIndex = pDoor->GetPathServerDynamicObjectIndex();
	sprintf(routeName, "DOOR#%p", (void*)(size_t)iObjIndex);
	const u32 iDoorHash = atStringHash(routeName);
	return iDoorHash;
}

u32 CAssistedMovementRouteStore::CalcScriptedRouteHash(const int iRouteIndex)
{
	char routeName[32];
	sprintf(routeName, "SCRIPTED#%i", iRouteIndex);
	const u32 iScritpedHash = atStringHash(routeName);
	return iScritpedHash;
}

#if !__FINAL
#if __BANK
void CAssistedMovementRouteStore::RescanNow()
{
	m_RouteToggles.SetHasChanged(true);
}
#endif
void CAssistedMovementRouteStore::Debug()
{
#if DEBUG_DRAW
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	const Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	char tmpTxt[128];
	const float fMaxDist = 250.0f;
	const float fTextDist = 40.0f;

	char flagsString[128];

	for(int r=0; r<CAssistedMovementRouteStore::MAX_ROUTES; r++)
	{
		CAssistedMovementRoute * pRoute = CAssistedMovementRouteStore::GetRoute(r);
		const Vector3 vRouteMid = (pRoute->GetMin() + pRoute->GetMax()) * 0.5f;
		if((vRouteMid - vOrigin).Mag() < fMaxDist)
		{
			for(int p=0; p<pRoute->GetSize(); p++)
			{
				const float fDistP1 = (pRoute->GetPoint(p) - vOrigin).Mag();
				const float fDistP2 = (pRoute->GetPoint(p) - vOrigin).Mag();
				const float fTextAlphaP1 = 1.0f - (fDistP1 / fTextDist);
				const float fNodeAlphaP1 = 1.0f - (fDistP1 / fMaxDist);
				const float fNodeAlphaP2 = (p < pRoute->GetSize()-1) ? 1.0f - (fDistP2 / fMaxDist) : fNodeAlphaP1;

				if(fNodeAlphaP1 > 0.0f || fNodeAlphaP2 > 0.0f)
				{
					Color32 iNode1Col = Color_LightBlue2;
					iNode1Col.SetAlpha((int)(fNodeAlphaP1*255.0f));
					Color32 iNode2Col = Color_LightBlue2;
					iNode2Col.SetAlpha((int)(fNodeAlphaP2*255.0f));

					// Display the route's name
					if(p == 0)
					{
						Color32 iTxtCol = Color_yellow;
						iTxtCol.SetAlpha((int)(fNodeAlphaP1*255.0f));
						grcDebugDraw::Line(pRoute->GetPoint(p), pRoute->GetPoint(p) + Vector3(0.f,0.f,0.5f), iTxtCol, iTxtCol);
						
						sprintf(tmpTxt, "Route name hash : 0x%x", pRoute->GetPathStreetNameHash());
						grcDebugDraw::Text(pRoute->GetPoint(p) + Vector3(0.f,0.f,0.5f), iTxtCol, 0, 0, tmpTxt);
						
						sprintf(tmpTxt, "Route flags : 0x%x", pRoute->GetFlags());
						grcDebugDraw::Text(pRoute->GetPoint(p) + Vector3(0.f,0.f,0.5f), iTxtCol, 0, grcDebugDraw::GetScreenSpaceTextHeight(), tmpTxt);
					}

					if(p < pRoute->GetSize()-1)
						grcDebugDraw::Line(pRoute->GetPoint(p), pRoute->GetPoint(p+1), iNode1Col, iNode2Col);

					// Draw the 'stalk' to the ground position, only if not underwater
					if(!(pRoute->GetPointFlags(p) & CAssistedMovementRoute::RPF_IsUnderwater))
					{
						Color32 iStalkCol = Color_LightBlue3;
						iStalkCol.SetAlpha((int)(fNodeAlphaP1*255.0f));
						grcDebugDraw::Line(pRoute->GetPoint(p), pRoute->GetPoint(p) - Vector3(0.0f,0.0f,1.02f), iStalkCol, iStalkCol);
					}
					// Draw the tangent at this node
					Color32 iTangentCol1 = Color_red3;
					Color32 iTangentCol2 = Color_red1;
					grcDebugDraw::Line(pRoute->GetPoint(p), pRoute->GetPoint(p) + pRoute->GetPointTangent(p), iTangentCol1, iTangentCol2);

					/*
					if(p < pRoute->GetSize()-1 && CPlayerAssistedMovementControl::ms_bUsePathWidth)
					{
						Color32 iWidth1Col = Color_LightBlue4;
						iWidth1Col.SetAlpha((int)(fNodeAlphaP1*255.0f));
						Color32 iWidth2Col = Color_LightBlue4;
						iWidth2Col.SetAlpha((int)(fNodeAlphaP2*255.0f));

						grcDebugDraw::Line(pRoute->GetPathBoundaryPoint(p, pRoute->GetPointWidth(p)*0.5f), pRoute->GetPathBoundaryPoint(p+1,pRoute->GetPointWidth(p+1)*0.5f), iWidth1Col, iWidth2Col);
						grcDebugDraw::Line(pRoute->GetPathBoundaryPoint(p,-pRoute->GetPointWidth(p)*0.5f), pRoute->GetPathBoundaryPoint(p+1,-pRoute->GetPointWidth(p+1)*0.5f), iWidth1Col, iWidth2Col);
					}
					*/
				}

				if(fTextAlphaP1 > 0.0f)
				{
					Color32 iTextCol = Color_LightBlue2;
					iTextCol.SetAlpha((int)(fTextAlphaP1*255.0f));

					flagsString[0] = 0;

					if(pRoute->GetPointFlags(p) & CAssistedMovementRoute::RPF_ReduceSpeedForCorners)
					{
						strcat(flagsString, " (cornering)");
					}
					if(pRoute->GetPointFlags(p) & CAssistedMovementRoute::RPF_IsUnderwater)
					{
						strcat(flagsString, " (underwater)");
					}

					sprintf(tmpTxt, "[%i]%s", p, flagsString);
					grcDebugDraw::Text(pRoute->GetPoint(p), iTextCol, tmpTxt);

					if(pRoute->GetPointTension(p)==0.5f)
						sprintf(tmpTxt, "tension:default(0.5)");
					else
						sprintf(tmpTxt, "tension:%.1f", pRoute->GetPointTension(p));
					grcDebugDraw::Text(pRoute->GetPoint(p), iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight(), tmpTxt);

					sprintf(tmpTxt, "width:%.1f", pRoute->GetPointWidth(p));
					grcDebugDraw::Text(pRoute->GetPoint(p), iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*2, tmpTxt);
				}
			}
		}
	}
#endif // DEBUG_DRAW
}
#endif	//!__FINAL
