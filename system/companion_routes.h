//--------------------------------------------------------------------------------------
// companion_routes.h
//--------------------------------------------------------------------------------------
#if COMPANION_APP
#pragma once
//--------------------------------------------------------------------------------------
//	Companion route node structure
//
//	***** If the size of these structures change we need to match it on the companion
//	***** app HTML side.  If you need to change this you need to inform whoever is 
//	***** working on the companion app.
//--------------------------------------------------------------------------------------
typedef struct 
{
	double x;
	double y;
	s16 id;
	s16 routeId;
}sCompanionRouteNode;
//	See above comments if this fires
CompileTimeAssertSize(sCompanionRouteNode, 24, 24);

//--------------------------------------------------------------------------------------
//	Companion route message header structure
//
//	***** If the size of these structures change we need to match it on the companion
//	***** app HTML side.  If you need to change this you need to inform whoever is 
//	***** working on the companion app.
//--------------------------------------------------------------------------------------
typedef struct 
{
	u8 messageType;
	u8 nodesAdded;
	u16 routeNodeCount[2];
	u8 routeR;
	u8 routeG;
	u8 routeB;
}sCompanionRouteMessageHeader;
//	See above comments if this fires
CompileTimeAssertSize(sCompanionRouteMessageHeader, 10, 10);

//--------------------------------------------------------------------------------------
//	Companion route message class
//--------------------------------------------------------------------------------------
class CCompanionRouteMessage
{
public:
	//	Ctor/Dtor
	CCompanionRouteMessage();
	~CCompanionRouteMessage();

	//	Add a node to the message
	bool AddNode(sCompanionRouteNode& newNode);
	//	Get encoded message
	char* GetEncodedMessage();
	//	Clear the message
	void ClearMessage();
	//	Get number of nodes added
	u8 GetNodesAdded() { return m_nodesAdded; }

private:
	//	Encode a message
	void Base64Encode(rage::u8* pDataIn, int sizeIn, char* pDataOut, int sizeOut);

	u8 m_data[MAX_BLIP_MESSAGE_DATA_SIZE];
	u8* m_pData;

	u16 m_currentDataSize;
	u8 m_nodesAdded;

	char m_encodedMessage[1015];
};

//--------------------------------------------------------------------------------------
//	Companion route node class
//--------------------------------------------------------------------------------------
class CCompanionRouteNode
{
public:
	//	Ctor/Dtor
	CCompanionRouteNode(sCompanionRouteNode& node);
	~CCompanionRouteNode();

	//	Update function
	void Update(sCompanionRouteNode& node);
	//	Has been updated?
	bool HasBeenUpdated() { return m_updated; }
	//	Set updated
	void SetUpdated(bool updated) { m_updated = updated; }
	//	Is this route node ready to be deleted?
	bool ReadyToDelete();
	//	Get the route ID of this node
	s16 GetRouteId();

	//	Linked list node
	inlist_node<CCompanionRouteNode> m_node;

	sCompanionRouteNode m_routeNode;

private:
	bool m_updated;
};

#endif	//	COMPANION_APP