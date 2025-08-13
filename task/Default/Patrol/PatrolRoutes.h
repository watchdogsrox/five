// FILE :    PatrolRoutes.h
// PURPOSE : Storage for patrol routes defined in max 
// AUTHOR :  Phil H
// CREATED : 29-10-2008

#ifndef PATROL_ROUTES_H
#define PATROL_ROUTES_H

#include "Vector/vector3.h"
#include "scene/RegdRefTypes.h"
#include "fwtl/pool.h"
#include "fwscene/Ipl.h"
#include "fwdebug/debugdraw.h"

class CPatrolLink;

typedef enum 
{ 
	LD_forwards = 0, 
	LD_backwards 
} LinkDirection;

// Single patrol node storing:
// - Links to neighbouring nodes
// - Position and heading.
// - Duration to pause at the node when patrolling
// - Node hash type that defines what kind of node this is and what should be done here.
class CPatrolNode : public fwRefAwareBase
{
public:
	CPatrolNode(fwIplPatrolNode* pFilePatrolNode, s32 iNodeID);
	~CPatrolNode();
	
	// Patrol nodes are allocated from a pool
	enum { MAX_PATROL_NODES = 70};
	FW_REGISTER_CLASS_POOL(CPatrolNode)

	enum { MAX_LINKS_PER_NODE = 4 };
	// Interface
	// Add a link from this node to another node
	CPatrolLink*	AddPatrolLink(CPatrolNode* pEndNode, LinkDirection iLinkDirection, fwIplPatrolLink* pFilePatrolLink);
	// Accessors
	// Sets/Returns the next node in the linked list of all nodes in this nodes named route
	void			SetNextRouteLinkedListElement(CPatrolNode* pNode) { m_pNextNodeLL = pNode; }
	CPatrolNode*	GetNextRouteLinkedListElement() const { return m_pNextNodeLL; }
	// Get positional info
	Vector3&		GetPosition() { return m_vPosition; }
	Vector3&		GetHeading() { return m_vHeading; }
	// Get link info
	s32			GetNumLinks() const { return m_iNumLinks; } 
	CPatrolLink*	GetPatrolLink(s32 iLink) const;
	CPatrolNode*	GetLinkedNode(s32 iLink) const;
 	// Usage info is the node being used (upto the task to set this)
	void			SetIsUsed(const bool bIsUsed) { m_bIsUsed = bIsUsed; }
	bool			GetIsUsed() const { return m_bIsUsed; }
	// Get duration and assorted base and route name
	u32			GetDuration() const { return m_iDuration; }
	u32			GetRouteNameHash() const { return m_routeNameHash; }
	u32			GetBaseNameHash() const { return m_baseNameHash; }
	// Get the Id for this node (this is as authored and is not unique)
	s32			GetNodeId() const { return m_iNodeId; }
	// Get the node type hash - this is used to play ambient anims and scenarios
	u32			GetNodeTypeHash() const { return m_nodeTypeHash; }
private:
	// Id for this node (this is as authored and is not unique)
	s32		m_iNodeId;

	// Position and heading information
	Vector3		m_vPosition;
	Vector3		m_vHeading;

 	// The node currently being used?
 	bool		m_bIsUsed;

	// How long the ped should wait at this particular node
	u32		m_iDuration;

	// The type of patrol node this is, a data file will specify any attributes this is correlated too.
	u32		m_nodeTypeHash;

	// Index into the array of links and a count of how many links this has
	RegdPatrolLink m_apPatrolLinks[MAX_LINKS_PER_NODE];
	s32		   m_iNumLinks;

	// Route and base names (stored as hashstrings)
	u32		m_routeNameHash;
	u32		m_baseNameHash;

	// Link to the next load associated with this route, used to create a linked list
	// of nodes that are associated with a particular route not for navigation
	CPatrolNode* m_pNextNodeLL;
};

// Single patrol link storing:
// - A safe-reffed pointer to the end node
class CPatrolLink : public fwRefAwareBase
{
public:
	// Nodes are allocated from a pool
	enum { MAX_PATROL_LINKS = 150};
	FW_REGISTER_CLASS_POOL(CPatrolLink)

	// Constructor/destructor
	CPatrolLink(CPatrolNode* pEndNode, LinkDirection iLinkDirection, fwIplPatrolLink* pFilePatrolLink);
	~CPatrolLink() {}

	// Returns the node at the end of this link
	CPatrolNode* GetEndNode() const { return m_pEndNode; }
	// Returns the direction of the link - forwards means the link was authored
	// towards the end node and vice versa
	LinkDirection GetDirection() const { return m_iDirection; }

private:
	RegdPatrolNode m_pEndNode;
	LinkDirection m_iDirection;
};

// Single patrol route storing:
// - The name of the route, stored as a hashstring
// - The name of the associated base, stored as a hashstring
// - A linked list of all contained nodes
class CNamedPatrolRoute
{
public:

	// The patrol route type (level = permanent for the level, scripted = temporary)
	enum 
	{
		PT_LEVEL,
		PT_SCRIPTED
	};

	CNamedPatrolRoute(u32 iRouteNameHash, u32 iBaseNameHash, s32 iDesiredNumPeds, s32 iPatrolRouteType = PT_SCRIPTED);
	~CNamedPatrolRoute() {}

	enum { MAX_PATROL_ROUTES = 32};
	FW_REGISTER_CLASS_POOL(CNamedPatrolRoute)

	// Interface
	void AddNodeToRoute(CPatrolNode& node);

	// Accessors
	// route name hash and base name hash
	u32			GetRouteNameHash() const { return m_routeNameHash; }
	u32			GetBaseNameHash() const { return m_baseNameHash; }
	s32			GetRouteType() const { return m_iRouteType; }
	// First of a linked list of all contained nodes
	CPatrolNode*	GetFirstPatrolNode() const { return m_pFirstNodeLL; }
	// Calculate the average position
	void			RecalculateRouteCentre();
	bool			CalculateRandomSpawningPosition(Vector3& vPosition);
	// The number of peds currently using the route
	s32			GetNumPedsUsingRoute() const { return m_iNumPedsUsingRoute; }
	void			ChangeNumPedsUsingRoute(s32 iVal) { m_iNumPedsUsingRoute += iVal; Assert(m_iNumPedsUsingRoute >= 0);}
	// Centre of the route - average position of all nodes
	const Vector3&	GetRouteCentre() const { return m_vCentre; }
	// Accessors for the desired number of peds on this route
	s32			GetDesiredNumPeds() const { return m_iDesiredNumPeds; }
	void			SetDesiredNumPeds(s32 val) { m_iDesiredNumPeds = val; }
private:
	// route name hash and base name hash
	u32 m_routeNameHash;
	u32 m_baseNameHash;

	// Centre of the route, calculated as the average of all nodes positions
	Vector3 m_vCentre;
	// Dynamic number of peds currently using the route this is used at runtime to keep track of peds patrolling this route
	s32 m_iNumPedsUsingRoute;
	// Specifies how many peds are required on this route
	s32 m_iDesiredNumPeds;
	// Specifies whether the route is permanent (doesn't get deleted unless the level is changed) or temporary
	s32 m_iRouteType;
	// Link to the first load associated with this route, used to create a linked list
	// of nodes that are associated with a particular route not for navigation
	CPatrolNode* m_pFirstNodeLL;
};

// Storage for all available patrol routes, links and nodes
// - The name of the route, stored as a hash string
// - The name of the associated base, stored as a hash string
class CPatrolRoutes
{
public:

#define ENUM_TO_STRING(x) #x

	// There should be a one-to one correspondence between enums and the corresponding ms_aEnumStringNames entry
	typedef enum
	{
		START_SKIING = 0,
		SKIING,
		FINISH_SKIING,
		START_WALKING,
		WALKING,
		FINISH_WALKING,
		START_GET_ON_CABLE_CAR,
		FINISH_GET_ON_CABLE_CAR,
		START_SMOKING,
		START_WANDER,
		START_SITTING,
		NUM_NODE_TYPES
	} ActivityNodeType;

#if DEBUG_DRAW
	static const char* ms_aEnumStringNames[];
#endif	

	enum { MAX_NUM_ACTIVITIES = 4 };

	// Initilisation and shutdown of all the patrol pools
	static void InitPools();
	static void ShutdownPools();

	// Init and shutdown between maps
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	// Loading and clearing of the patrol route data
	static void StorePatrolRoutes( fwIplPatrolNode* aPatrolNodes, s32* aOptionalNodeIds, s32 iNumNodes, fwIplPatrolLink* aPatrolLinks, s32 iNumLinks, s32 iNumGuardsPerRoute, s32 iPatrolRouteType = CNamedPatrolRoute::PT_SCRIPTED );
	// Deletes a named patrol route along with all associated nodes and links
	static void DeleteNamedPatrolRoute(u32 iHashName);
	// Deletes all scripted routes - system use only
	static void ClearScriptedPatrolRoutes();
	// Deletes all patrol routes - system use only
	static void ClearAllPatrolRoutes();
	
	// Finds the nearest node in the specified route
	static CPatrolNode* FindNearestNodeInRoute( const Vector3& vPos, u32 iHashRouteName );
	// Finds the nearest patrol node in the map
	static CPatrolNode* FindNearestPatrolNode( const Vector3& vPos );
	// Finds the nearest ski node in the map within max distance
	static CPatrolNode* FindNearestSkiNode(const Vector3& vPos, float fMaxDistance);
#if 0
	// Choose a random nearby activity start node
	static CPatrolNode* ChooseRandomNearbyActivity(const Vector3& vPos, float fMaxDistance);
	// Validate the nodes scenario conditions
	static bool ValidateScenarioAtNode(CPatrolNode* pActivityNode);
#endif

	// Recalculate the average centre positions of all routes
	static void RecalculateRouteCentres();

	// Spawning commands:
	static void ChangeNumberPedsUsingRoute(u32 iRouteHash, s32 iVal);
	static void UpdateHenchmanSpawning(const Vector3& vWorldCentre);
	static bool	SpawnPatrollingGuard( CNamedPatrolRoute* pPatrolRoute );
	static void RemovePatrollingGuards( CNamedPatrolRoute* pPatrolRoute );
	static s32 GetLevelHenchmanModelIndex();
	
	static CNamedPatrolRoute* GetNamedPatrolRoute(u32 iHashName);

#if !__FINAL
	static bool ms_bRenderRoutes;
	static bool ms_bRender2DEffects;
	static bool ms_bToggleGuardsAlert;
	static bool ms_bTogglePatrolLookAt;
	static void DebugRenderRoutes();
#endif
private:
	// Helper functions
	static CPatrolNode* AddPatrolNode(fwIplPatrolNode* pFilePatrolNode, s32 iIndex);
	
	static CNamedPatrolRoute* AddNamedPatrolRoute(u32 iHashName, u32 iBaseName, s32 iDesiredNumPeds, s32 iPatrolType = CNamedPatrolRoute::PT_LEVEL);

	// Spawning distances
	static float PATROLLING_SPAWNING_DISTANCE;
	static float PATROLLING_SPAWNING_BUFFER;
};



#endif // PATROL_ROUTES_H
