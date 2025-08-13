// FILE :    PatrolRoutes.cpp
// PURPOSE : Storage for patrol routes defined in max 
// AUTHOR :  Phil H
// CREATED : 29-10-2008

#include "Task/Default/Patrol/PatrolRoutes.h"

#include "grcore/debugdraw.h"

// Game headers
#include "camera/CamInterface.h"
#include "game/ModelIndices.h"
#include "fwmaths/random.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Peds/pedpopulation.h"
#include "Peds/ped.h"
#include "Peds/pedIntelligence.h"
#include "Scene/FileLoader.h"
#include "scene/world/gameWorld.h"
#include "scene/worldpoints.h"
#include "task/Default/patrol/TaskPatrol.h"
#include "Task/Scenario/ScenarioManager.h"
#include "task/System/TaskManager.h"

// Rage headers
#include "Vector/colors.h"

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPatrolNode, CPatrolNode::MAX_PATROL_NODES, 0.75f, atHashString("CPatrolNode",0x6e63bff3));
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPatrolLink, CPatrolLink::MAX_PATROL_LINKS, 0.75f, atHashString("CPatrolLink",0x1996ba86));
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CNamedPatrolRoute, CNamedPatrolRoute::MAX_PATROL_ROUTES, 0.75f, atHashString("CNamedPatrolRoute",0x9c085e2b));

AI_OPTIMISATIONS ()

#if DEBUG_DRAW
const char* CPatrolRoutes::ms_aEnumStringNames[] =
{
	// Node contexts should be exactly the same as the enums, when adding a new activity, add the string to here
	ENUM_TO_STRING(START_SKIING),
	ENUM_TO_STRING(SKIING),
	ENUM_TO_STRING(FINISH_SKIING),
	ENUM_TO_STRING(START_WALKING),
	ENUM_TO_STRING(WALKING),
	ENUM_TO_STRING(FINISH_WALKING),
	ENUM_TO_STRING(START_GET_ON_CABLE_CAR),
	ENUM_TO_STRING(FINISH_GET_ON_CABLE_CAR),
	ENUM_TO_STRING(START_SMOKING),
	ENUM_TO_STRING(START_WANDER),
	ENUM_TO_STRING(START_SITTING)
};
#endif


CPatrolNode::CPatrolNode(fwIplPatrolNode* pFilePatrolNode, s32 iNodeID)
: m_iNodeId(iNodeID)
, m_iDuration(pFilePatrolNode->duration)
, m_nodeTypeHash(pFilePatrolNode->nodeTypeHash)
, m_vPosition(pFilePatrolNode->x, pFilePatrolNode->y, pFilePatrolNode->z )
, m_vHeading(pFilePatrolNode->headingX, pFilePatrolNode->headingY, pFilePatrolNode->headingZ )
, m_iNumLinks(0)
, m_routeNameHash(pFilePatrolNode->routeNameHash)
, m_baseNameHash(pFilePatrolNode->associatedBaseHash)
/*, m_bIsUsed(false)*/
{
}

CPatrolNode::~CPatrolNode()
{
	for(s32 i = 0; i < MAX_LINKS_PER_NODE; i++ )
	{
		if( m_apPatrolLinks[i] )
			delete m_apPatrolLinks[i];
	}
	m_iNumLinks = 0;
}

CPatrolNode* CPatrolNode::GetLinkedNode( s32 iLink ) const
{
	Assert( iLink >= 0 && iLink < m_iNumLinks );

	CPatrolLink* pPatrolLink = m_apPatrolLinks[iLink];

	if( pPatrolLink == NULL )
		return NULL;

	return pPatrolLink->GetEndNode();
}

CPatrolLink* CPatrolNode::GetPatrolLink( s32 iLink ) const
{
	Assert( iLink >= 0 && iLink < m_iNumLinks );

	CPatrolLink* pPatrolLink = m_apPatrolLinks[iLink];
	return pPatrolLink;
}

CPatrolLink* CPatrolNode::AddPatrolLink(CPatrolNode* pEndNode, LinkDirection iLinkDirection, fwIplPatrolLink* pFilePatrolLink)
{
	Assert( m_iNumLinks < MAX_LINKS_PER_NODE );
	if( m_iNumLinks >= MAX_LINKS_PER_NODE )
		return NULL;

	m_apPatrolLinks[m_iNumLinks++] = rage_new CPatrolLink(pEndNode, iLinkDirection, pFilePatrolLink);
	return m_apPatrolLinks[m_iNumLinks-1];
}

CPatrolLink::CPatrolLink(CPatrolNode* pEndNode, LinkDirection iLinkDirection, fwIplPatrolLink* UNUSED_PARAM(pFilePatrolLink))
: m_pEndNode(pEndNode)
, m_iDirection(iLinkDirection)
{
}

CNamedPatrolRoute::CNamedPatrolRoute(u32 iRouteNameHash, u32 iBaseNameHash, s32 iDesiredNumPeds, s32 iRouteType)
: m_pFirstNodeLL(NULL)
, m_routeNameHash(iRouteNameHash)
, m_baseNameHash(iBaseNameHash)
, m_vCentre(0.0f, 0.0f, 0.0f)
, m_iNumPedsUsingRoute(0)
, m_iDesiredNumPeds(iDesiredNumPeds)
, m_iRouteType(iRouteType)
{

}

void CNamedPatrolRoute::AddNodeToRoute( CPatrolNode& node )
{
	if( m_pFirstNodeLL )
	{
		node.SetNextRouteLinkedListElement(m_pFirstNodeLL);
		m_pFirstNodeLL = &node;
	}
	else
	{
		m_pFirstNodeLL = &node;
		node.SetNextRouteLinkedListElement(NULL);
	}
}

void CNamedPatrolRoute::RecalculateRouteCentre()
{
	// Calculate the route centre as the average position of all containing nodes.
	m_vCentre.Zero();
	s32 iNumNodes = 0;

	// Walk through all nodes and average their positions
	CPatrolNode* pPatrolNode = m_pFirstNodeLL;
	while(pPatrolNode)
	{
		m_vCentre += pPatrolNode->GetPosition();
		++iNumNodes;
		pPatrolNode = pPatrolNode->GetNextRouteLinkedListElement();
	}

	// Calculate the average pos
	if( iNumNodes > 0 )
		m_vCentre/=(float)iNumNodes;
}

#define MAX_SPAWNING_POS 32
bool CNamedPatrolRoute::CalculateRandomSpawningPosition( Vector3& vPosition )
{
	vPosition = VEC3_ZERO;
	// Store all available spawning nodes
	s32 iNumNodes = 0;
	Vector3 avSpawnPositions[MAX_SPAWNING_POS];

	// Walk through all nodes and average their positions
	CPatrolNode* pPatrolNode = m_pFirstNodeLL;
	while(pPatrolNode)
	{
		avSpawnPositions[iNumNodes++] = pPatrolNode->GetPosition();
		pPatrolNode = pPatrolNode->GetNextRouteLinkedListElement();
	}

	// Calculate the average pos
	if( iNumNodes > 0 )
	{
		vPosition = avSpawnPositions[fwRandom::GetRandomNumberInRange(0, iNumNodes)];
		return true;
	}

	Assertf(0, "Route has no patrol nodes");
	return false;
}
void CPatrolRoutes::StorePatrolRoutes( fwIplPatrolNode* aPatrolNodes, s32* aOptionalNodeIds, s32 iNumNodes, fwIplPatrolLink* aPatrolLinks, s32 iNumLinks, s32 iNumGuardsPerRoute, s32 iPatrolRouteType )
{
	Assert(iNumNodes<CPatrolNode::MAX_PATROL_NODES);
	// Add in all the nodes first, storing pointers for each so these can be used when adding the links
	CPatrolNode* apAddedPatrolNodes[CPatrolNode::MAX_PATROL_NODES];
	for(s32 iNode = 0; iNode < iNumNodes; iNode++ )
	{
		// The node ID defaults to the added index if an optional set isn't specified
		s32 iNodeId = iNode;
		if( aOptionalNodeIds )
			iNodeId = aOptionalNodeIds[iNode];
		CPatrolNode* pPatrolNode = AddPatrolNode( &aPatrolNodes[iNode], iNodeId );

		// Make sure a named route has already been added, and associate this node with the route
		if( AssertVerify(pPatrolNode != NULL ) )
		{
			// Add in a reference to the named route if it isn't already present.
			CNamedPatrolRoute* pNamedRoute = GetNamedPatrolRoute(aPatrolNodes[iNode].routeNameHash);
			if( pNamedRoute == NULL )
				pNamedRoute = AddNamedPatrolRoute(aPatrolNodes[iNode].routeNameHash, aPatrolNodes[iNode].associatedBaseHash, iNumGuardsPerRoute, iPatrolRouteType);

			// Add the reference to the node into the route
			Assert(pNamedRoute);
			if( pNamedRoute )
				pNamedRoute->AddNodeToRoute(*pPatrolNode);
		}
		apAddedPatrolNodes[iNode] = pPatrolNode;
	}

	// Loop through and add in links that match each of the nodes.
	for(s32 iNode = 0; iNode < iNumNodes; iNode++ )
	{
		CPatrolNode* pPatrolNode = apAddedPatrolNodes[iNode];
		Assert(pPatrolNode);
		// The node added must have the same address, the links are made using start and end indices
		for( s32 iLink = 0; iLink < iNumLinks; iLink++ )
		{
			// Add the link into the link structure, these must be added sequentially as the 
			// node stores a start index into the links and a count
			if( aPatrolLinks[iLink].node1 == pPatrolNode->GetNodeId() )
			{
				// Find the other node
				for( s32 iOtherNode = 0; iOtherNode < iNumNodes; iOtherNode++ )
				{
					if(  apAddedPatrolNodes[iOtherNode]->GetNodeId() == aPatrolLinks[iLink].node2 )
					{
						pPatrolNode->AddPatrolLink( apAddedPatrolNodes[iOtherNode], LD_forwards, &aPatrolLinks[iLink] );
					}
				}
			}
			else if( aPatrolLinks[iLink].node2 == pPatrolNode->GetNodeId() )
			{
				// Find the other node
				for( s32 iOtherNode = 0; iOtherNode < iNumNodes; iOtherNode++ )
				{
					if( apAddedPatrolNodes[iOtherNode]->GetNodeId() == aPatrolLinks[iLink].node1 )
					{
						pPatrolNode->AddPatrolLink( apAddedPatrolNodes[iOtherNode], LD_backwards, &aPatrolLinks[iLink] );
					}
				}
			}
		}
	}

	// Recalulate the centres of all routes
	RecalculateRouteCentres();
}

void CPatrolRoutes::ClearScriptedPatrolRoutes()
{
	// This isn't a particularly efficient way of deleting the routes.
	// Its like this to ensure the restructuring of the patrol routes so that 
	// routes can be added/deleted independently and that deleting by route name alone
	// will clear all nodes and links from their respective pools.
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute && pPatrolRoute->GetRouteType() == CNamedPatrolRoute::PT_SCRIPTED)
		{
			DeleteNamedPatrolRoute(pPatrolRoute->GetRouteNameHash());
		}
	}
}

void CPatrolRoutes::ClearAllPatrolRoutes()
{
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute)
		{
			DeleteNamedPatrolRoute(pPatrolRoute->GetRouteNameHash());
		}
	}
	Assert(CNamedPatrolRoute::GetPool()->GetNoOfUsedSpaces()==0);
	Assert(CPatrolNode::GetPool()->GetNoOfUsedSpaces()==0);
	Assert(CPatrolLink::GetPool()->GetNoOfUsedSpaces()==0);
}

CPatrolNode* CPatrolRoutes::AddPatrolNode( fwIplPatrolNode* pFilePatrolNode, s32 iIndex )
{
	// Add the node into the list
	CPatrolNode* pPatrolNode = rage_new CPatrolNode(pFilePatrolNode, iIndex);
	Assertf(pPatrolNode, "CPatrolRoutes: exceeded maximum patrol routes!");

	// Return the node
	return pPatrolNode;
}

#if !__FINAL
bool CPatrolRoutes::ms_bRenderRoutes = false;
bool CPatrolRoutes::ms_bRender2DEffects = false;
bool CPatrolRoutes::ms_bToggleGuardsAlert = false;
bool CPatrolRoutes::ms_bTogglePatrolLookAt = false;
void CPatrolRoutes::DebugRenderRoutes()
{
#if DEBUG_DRAW
	if (ms_bRenderRoutes)
	{
#define VISUALISE_RANGE 100.0f

	s32 iPoolSize = CPatrolNode::GetPool()->GetSize();
	for( s32 iNode = 0; iNode < iPoolSize; iNode++ )
	{
		CPatrolNode* pPatrolNode = CPatrolNode::GetPool()->GetSlot(iNode);
		if( pPatrolNode )
		{
			Vector3 vDiff = pPatrolNode->GetPosition() - camInterface::GetPos();
			float fDist = vDiff.Mag();
			// Draw the nodes // Skip this node if we're too far away
			if (fDist < VISUALISE_RANGE)
			{
				float fScale = 1.0f - (fDist / VISUALISE_RANGE);

				Vector3 WorldCoors = pPatrolNode->GetPosition() + Vector3(0,0,1.0f);

				const u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);
				Color32 col = Color_green;
				bool bHaveNodeType = false;
				s32 iNodeEnum = 0;

				// Draw the spheres
				for (s32 i = 0; i < NUM_NODE_TYPES; i++)
				{
					if (pPatrolNode->GetNodeTypeHash() == atStringHash(ms_aEnumStringNames[i]))
					{
						switch (ActivityNodeType(i))
						{
							case START_SKIING:
							case START_WALKING:
							case START_GET_ON_CABLE_CAR:
							case START_SITTING:
								{
									col = Color_green;
									grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, col, false);	
									iNodeEnum = i;
									bHaveNodeType = true;				
								}
								break;
							case SKIING:
							case WALKING:
								{
									col = Color_purple;
									grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, col, false);	
									iNodeEnum = i;
									bHaveNodeType = true;
								}
								break;
							case FINISH_SKIING:
							case FINISH_WALKING:
							case FINISH_GET_ON_CABLE_CAR:
								{
									col = Color_red;
									grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, col, false);
									iNodeEnum = i;
									bHaveNodeType = true;
								}
								break;
							case START_SMOKING:
								{
									col = Color_black;
									grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, col, false);
									iNodeEnum = i;
									bHaveNodeType = true;
								}
								break;
							case START_WANDER:
								{
									col = Color_orange;
									grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, col, false);
									iNodeEnum = i;
									bHaveNodeType = true;
								}
								break;

							default:
								grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, Color_green, false);	
								break;
						}
					}
				}

				if (bHaveNodeType && iAlpha!=0)
				{
					col.SetAlpha(iAlpha);
					char tmp[256];
					sprintf(tmp, "%s", ms_aEnumStringNames[iNodeEnum] ); 
					grcDebugDraw::Text( WorldCoors, col, 0, grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
				}
				else
				{
					col.SetAlpha(iAlpha);
					char tmp[256];
					sprintf(tmp, "PATROL" ); 
					grcDebugDraw::Text( WorldCoors, col, 0, grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
					grcDebugDraw::Sphere( pPatrolNode->GetPosition(), 0.5f, Color_green, false);
					// Draw the patrol node heading
					Vector3 vHeading = pPatrolNode->GetHeading();
					vHeading.Normalize();
					//grcDebugDraw::Line(pPatrolNode->GetPosition() + Vector3(0.0f,0.0f,1.0f),vHeading,Color_orange,Color_orange);	
				}

			} 
			
			// Draw the links
			for( s32 iLink = 0; iLink < pPatrolNode->GetNumLinks(); iLink++ )
			{
				CPatrolNode* pPatrolNodeTo = pPatrolNode->GetLinkedNode(iLink);
				
				Vector3 vPatrolNodePos = pPatrolNode->GetPosition();
				vPatrolNodePos.z = vPatrolNodePos.z + 0.25f; //Draw the line from the sphere top		
				
				Vector3 vPatrolNodePosTo = pPatrolNodeTo->GetPosition();
				vPatrolNodePosTo.z = vPatrolNodePosTo.z + 0.25f; 	

				if(pPatrolNodeTo)
					grcDebugDraw::Line(vPatrolNodePosTo, vPatrolNodePos, Color_blue);
		
				if (ms_bTogglePatrolLookAt)
				{
					
					Vector3 vLookAtPos = pPatrolNode->GetHeading();
				
					if (!vLookAtPos.IsZero())
					{
						vLookAtPos = vLookAtPos + vPatrolNodePos;
						grcDebugDraw::Sphere( vLookAtPos, 0.1f, Color_red, false);	
						grcDebugDraw::Line(vLookAtPos, vPatrolNodePos, Color_red);
				
					}
				}
			}
		}
	}
	}
#endif // DEBUG_DRAW
}
#endif	// !__FINAL


CNamedPatrolRoute* CPatrolRoutes::GetNamedPatrolRoute( u32 iHashName )
{
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			if( pPatrolRoute->GetRouteNameHash() == iHashName )
			{
				return pPatrolRoute;
			}
		}
	}
	return NULL;
}

CNamedPatrolRoute* CPatrolRoutes::AddNamedPatrolRoute( u32 iHashName, u32 iBaseName, s32 iDesiredNumPeds, s32 iPatrolRouteType )
{
	return rage_new CNamedPatrolRoute(iHashName, iBaseName, iDesiredNumPeds, iPatrolRouteType);
}

CPatrolNode* CPatrolRoutes::FindNearestNodeInRoute( const Vector3& vPos, u32 iHashRouteName )
{
	CPatrolNode* pNearestNode = NULL;
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			if( iHashRouteName == 0 || pPatrolRoute->GetRouteNameHash() == iHashRouteName )
			{
				CPatrolNode* pPatrolNode = pPatrolRoute->GetFirstPatrolNode();
				while(pPatrolNode)
				{
					if( pNearestNode == NULL )
					{
						pNearestNode = pPatrolNode;
					}
					else if( pNearestNode->GetPosition().Dist2(vPos) > 
						pPatrolNode->GetPosition().Dist2(vPos) )
					{
						pNearestNode = pPatrolNode;
					}
					pPatrolNode = pPatrolNode->GetNextRouteLinkedListElement();
				}
			}
		}
	}
	return pNearestNode;
}

CPatrolNode* CPatrolRoutes::FindNearestPatrolNode( const Vector3& vPos )
{
	return FindNearestNodeInRoute(vPos, 0);
}

CPatrolNode* CPatrolRoutes::FindNearestSkiNode( const Vector3& vPos, float fMaxDistance)
{
	CPatrolNode* pNearestNode = NULL;
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			CPatrolNode* pPatrolNode = pPatrolRoute->GetFirstPatrolNode();
			while (pPatrolNode)
			{
				if (pPatrolNode->GetNodeTypeHash() == ATSTRINGHASH("SKIING", 0x04341a6ef))
				{
					if( pNearestNode == NULL )
					{
						pNearestNode = pPatrolNode;
					}
					else if (pNearestNode->GetPosition().Dist2(vPos) > 
						pPatrolNode->GetPosition().Dist2(vPos) )
					{
						pNearestNode = pPatrolNode;
					}
				}
				pPatrolNode = pPatrolNode->GetNextRouteLinkedListElement();
			}
		}
	}

	// Check that the nearest node is also within the max distance allowed
	if (pNearestNode && pNearestNode->GetPosition().Dist(vPos) < fMaxDistance)
	{
		return pNearestNode;
	}

	return NULL;
}

// Disabled, was unused.
#if 0

CPatrolNode* CPatrolRoutes::ChooseRandomNearbyActivity(const Vector3& vPos, float fMaxDistance)
{
	//Prefer close activities // There needs to be some some of streaming of nodes, then we should be able to just go through nearby nodes

	CPatrolNode* ppActivityStartNodes[MAX_NUM_ACTIVITIES];

	s32 iNumOfActivities = 0;
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();

	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			CPatrolNode* pPatrolNode = pPatrolRoute->GetFirstPatrolNode();
			while (pPatrolNode)
			{
				for (int i = 0; i < NUM_NODE_TYPES; i++)
				{
					// If the enum string is a start node, check if the patrol node is of that type
					if (!strnicmp(ms_aEnumStringNames[i],"START",5))
					{
						if (pPatrolNode->GetNodeTypeHash() == atStringHash(ms_aEnumStringNames[i]))
						{
							// Ensure the node is close enough and the scenario at that node has validate conditions
							if (pPatrolNode->GetPosition().Dist(vPos) < fMaxDistance && ValidateScenarioAtNode(pPatrolNode))
							{
								if (iNumOfActivities < MAX_NUM_ACTIVITIES)
								{
									ppActivityStartNodes[iNumOfActivities++] = pPatrolNode;
								}
								else
								{
									// No more space left in the array
									break;
								}
							}
						}
					}
				}
				pPatrolNode = pPatrolNode->GetNextRouteLinkedListElement();
			}
		}
	}

	//if (!taskVerifyf(iNumOfActivities,"No nearby activities found, place some start activity nodes"))
	if (iNumOfActivities <= 0)
	{
		return NULL;
	}

	return ppActivityStartNodes[fwRandom::GetRandomNumberInRange(0,iNumOfActivities)];	
}

bool CPatrolRoutes::ValidateScenarioAtNode(CPatrolNode* pActivityNode)
{
	taskFatalAssertf(pActivityNode,"NULL node");

	// Go through all the node types to see if it's valid
	for (s32 iNodeType = 0; iNodeType < NUM_NODE_TYPES; ++iNodeType)
	{
		// If it is, then validate
		u32 iNodeTypeHash = pActivityNode->GetNodeTypeHash();
		if (iNodeTypeHash == atStringHash(ms_aEnumStringNames[iNodeType]))
		{
			switch (iNodeType)
			{
				case START_SKIING:
				case SKIING:
				case FINISH_SKIING:
					return CScenarioManager::CheckScenarioTimesAndProbability(CScenarioManager::GetScenarioTypeFromName("SKIING"));
				case START_WALKING:
				case WALKING:
				case FINISH_WALKING:
				case START_SITTING:
					return true;
				case START_GET_ON_CABLE_CAR:
				case FINISH_GET_ON_CABLE_CAR:
					return true;		
				case START_SMOKING:
					return CScenarioManager::CheckScenarioTimesAndProbability(CScenarioManager::GetScenarioTypeFromName("START_SMOKING"));
				case START_WANDER:
					return true;
				default:
					taskAssertf(0,"Invalid Scenario At Node");
			}
		}
	}
	return false;
}

#endif	// 0

void CPatrolRoutes::DeleteNamedPatrolRoute( u32 iHashRouteName )
{
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			if( pPatrolRoute->GetRouteNameHash() == iHashRouteName )
			{
				CPatrolNode* pPatrolNode = pPatrolRoute->GetFirstPatrolNode();
				while(pPatrolNode)
				{
					CPatrolNode *pNextNode = pPatrolNode->GetNextRouteLinkedListElement();
					delete pPatrolNode;
					pPatrolNode = pNextNode;
				}
				delete pPatrolRoute;
			}
		}
	}
}

void CPatrolRoutes::InitPools()
{
	CPatrolNode::InitPool( MEMBUCKET_GAMEPLAY );
	CPatrolLink::InitPool( MEMBUCKET_GAMEPLAY );
	CNamedPatrolRoute::InitPool( MEMBUCKET_GAMEPLAY );
}

void CPatrolRoutes::ShutdownPools()
{
	CPatrolNode::ShutdownPool();
	CPatrolLink::ShutdownPool();
	CNamedPatrolRoute::ShutdownPool();
}

void CPatrolRoutes::Init(unsigned /*initMode*/)
{
}

void CPatrolRoutes::Shutdown(unsigned /*shutdownMode*/)
{
	ClearAllPatrolRoutes();
}

void CPatrolRoutes::RecalculateRouteCentres()
{
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			pPatrolRoute->RecalculateRouteCentre();
		}
	}
}

void CPatrolRoutes::ChangeNumberPedsUsingRoute( u32 iRouteHash, s32 iVal )
{
	CNamedPatrolRoute* pPatrolRoute = GetNamedPatrolRoute(iRouteHash);
	if (pPatrolRoute)
	{
		// Check if the number of peds using the route is valid,
		// If iVal is less than 0 and curr no peds using route = 0, 
		// this might occur if a script is deleting and recreating a 
		// patrol route before a ped has their tasks cleared in which case we do nothing.
		if (iVal > 0 || (iVal < 0 && pPatrolRoute->GetNumPedsUsingRoute() > 0))
		{		
			pPatrolRoute->ChangeNumPedsUsingRoute(iVal);
		}	
	}
}

float CPatrolRoutes::PATROLLING_SPAWNING_DISTANCE = 120.0f;
float CPatrolRoutes::PATROLLING_SPAWNING_BUFFER = 20.0f;

void CPatrolRoutes::UpdateHenchmanSpawning(const Vector3& vWorldCentre)
{
	// For each route in range, make sure peds are spawned
	s32 iPoolSize = CNamedPatrolRoute::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CNamedPatrolRoute* pPatrolRoute = CNamedPatrolRoute::GetPool()->GetSlot(i);
		if( pPatrolRoute )
		{
			// is the route within the spawning distance?
			float fDistSq = pPatrolRoute->GetRouteCentre().Dist2(vWorldCentre);
			if( fDistSq < rage::square(PATROLLING_SPAWNING_DISTANCE) )
			{
				// are there any peds currently spawned at this route?
				if( pPatrolRoute->GetNumPedsUsingRoute() < pPatrolRoute->GetDesiredNumPeds() )
				{
				 	// Spawn a ped on the route
					SpawnPatrollingGuard(pPatrolRoute);
				}
			}
			// Route is outside the spawning distance (plus a tolerance), remove any spawned peds
			else if( fDistSq > rage::square(PATROLLING_SPAWNING_DISTANCE + PATROLLING_SPAWNING_BUFFER) )
			{
				// are there any peds currently spawned at this route?
				if( pPatrolRoute->GetNumPedsUsingRoute() > 0 )
				{
					// remove any peds associated with it
					RemovePatrollingGuards(pPatrolRoute);
				}
			}
		}
	}
}

bool CPatrolRoutes::SpawnPatrollingGuard( CNamedPatrolRoute* UNUSED_PARAM(pPatrolRoute) )
{
	// Removed as scripters will do this manually
	// Make sure the current henchman model index is loaded
	/*s32 iModelIndex = GetLevelHenchmanModelIndex();
	if (!CStreaming::HasObjectLoaded(iModelIndex, CModelInfo::GetStreaming ModuleId()))
	{
		CStreaming::RequestObject(iModelIndex, CModelInfo::GetStreaming ModuleId(), STRFLAG_FORCE_LOAD);
		return false;
	}

	// Calculate a random position on the route
	Vector3 vSpawnPos(0.0f, 0.0f, 0.0f);
	if( !pPatrolRoute->CalculateRandomSpawningPosition(vSpawnPos) )
		return false;

	// Spawn the ped in the correct location
	DefaultScenarioInfo defaultScenarioInfo(DS_Invalid, NULL);
	CPed* pPed = CPedPopulation::AddPed(iModelIndex, vSpawnPos, 0.0f, NULL, false, -1, NULL, true, false, false, true, defaultScenarioInfo,false, false);
	if( pPed == NULL )
	{
		return false;
	}

	// Assign the default patrolling task
	CTaskPatrol* pPatrolTask = rage_new CTaskPatrol(pPatrolRoute->GetRouteNameHash(), CTaskPatrol::AS_casual, 0);
	pPed->GetPedIntelligence()->GetTaskManager()->SetTask(pPatrolTask, TASK_PRIORITY_DEFAULT);

	// Give the ped a weapon, this will come from a script parameter, but i'll hardcode it until 
	// Chris has mergified all the new weapon stuff
	pPed->GetInventory().AddItemAndAmmo(WEAPONTYPE_ASSAULTRIFLE, INFINITE_AMMO);
		
	// Set the spawning type and associated route hash
	pPed->PopTypeSet(POPTYPE_RANDOM_PATROL);
	pPed->SetAssociatedCreationData(pPatrolRoute->GetRouteNameHash());*/

	return true;
}

s32 CPatrolRoutes::GetLevelHenchmanModelIndex()
{
	// TODO - PH - this needs to do something clever, we need to know
	// which henchman are available for each level
	return -1;//MI_PED_ALPINE_HENCHMAN;
}

void CPatrolRoutes::RemovePatrollingGuards( CNamedPatrolRoute* pPatrolRoute )
{
	CPed::Pool *pool = CPed::GetPool();
	CPed* pPed;
	s32 i=pool->GetSize();
	while(i--)
	{
		pPed = pool->GetSlot(i);
		if(pPed)
		{
			if( pPed->PopTypeGet() == POPTYPE_RANDOM_PATROL && 
				pPed->GetAssociatedCreationData() == pPatrolRoute->GetRouteNameHash() )
			{
				pPed->FlagToDestroyWhenNextProcessed();
			}
		}
	}
}
