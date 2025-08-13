//--------------------------------------------------------------------------------------
// companion_routes.cpp
//--------------------------------------------------------------------------------------
#include "companion.h"
#if COMPANION_APP
#include "companion_routes.h"

//--------------------------------------------------------------------------------------
//	Companion route node class
//--------------------------------------------------------------------------------------
CCompanionRouteNode::CCompanionRouteNode(sCompanionRouteNode& node)
{
	//	Store the node
	m_routeNode = node;
	//	Reset updated bool for new nodes
	m_updated = true;
}

CCompanionRouteNode::~CCompanionRouteNode()
{

}

void CCompanionRouteNode::Update(sCompanionRouteNode& node)
{
	if (m_routeNode.x != node.x || 
		m_routeNode.y != node.y)
	{
		m_routeNode = node;
		m_updated = true;
	}
}

bool CCompanionRouteNode::ReadyToDelete()
{
	if (!HasBeenUpdated())
	{
		return true;
	}

	return false;
}

s16 CCompanionRouteNode::GetRouteId()
{
	return m_routeNode.routeId;
}

//--------------------------------------------------------------------------------------
//	Companion route message class
//--------------------------------------------------------------------------------------
CCompanionRouteMessage::CCompanionRouteMessage()
{
	//	Initialise the pointer to the data
	m_pData = m_data;
	//	Clear the data
	memset(m_pData, 0, MAX_BLIP_MESSAGE_DATA_SIZE);
	//	Reset the current data size
	m_currentDataSize = 0;

	//	Reset the number of nodes added
	m_nodesAdded = 0;
	//	Reserve the first section for the message header
	m_pData += sizeof(sCompanionRouteMessageHeader);
	m_currentDataSize += sizeof(sCompanionRouteMessageHeader);
}

CCompanionRouteMessage::~CCompanionRouteMessage()
{

}

bool CCompanionRouteMessage::AddNode(sCompanionRouteNode& newNode)
{
	int nodeSize = sizeof(sCompanionRouteNode);

	//	Only add up to the size limit for this message
	if (m_currentDataSize + nodeSize < MAX_BLIP_MESSAGE_DATA_SIZE)
	{
		//	Copy the basic structure into our node data
		memcpy(m_pData, &newNode, sizeof(sCompanionRouteNode));
		//	Move the data along
		m_pData += sizeof(sCompanionRouteNode);
		m_currentDataSize += sizeof(sCompanionRouteNode);

		//	Increment the number of nodes
		m_nodesAdded++;

		return true;
	}

	return false;
}

char* CCompanionRouteMessage::GetEncodedMessage()
{
	sCompanionRouteMessageHeader header;

	//	Set the message type
	header.messageType = MESSAGE_TYPE_ROUTE;
	//	Set the number of nodes in the message
	header.nodesAdded = m_nodesAdded;
	//	Set the number of nodes in route 0 (purple player waypoint)
	header.routeNodeCount[0] = CCompanionData::GetInstance()->GetRouteNodeCount(0);
	//	Set the number of nodes in route 1 (yellow mission waypoint)
	header.routeNodeCount[1] = CCompanionData::GetInstance()->GetRouteNodeCount(1);
	//	Set the route colour (route 1 only)
	Color32 colour = CCompanionData::GetInstance()->GetRouteColour();
	header.routeR = colour.GetRed();
	header.routeG = colour.GetGreen();
	header.routeB = colour.GetBlue();

	//	Copy the header to the data
	memcpy(m_data, &header, sizeof(sCompanionRouteMessageHeader));

	//	Encode the message
	Base64Encode(m_data, m_currentDataSize, m_encodedMessage, 1015);

	return m_encodedMessage;
}

void CCompanionRouteMessage::ClearMessage()
{
	//	Initialise the pointer to the data
	m_pData = m_data;
	//	Clear the data
	memset(m_pData, 0, MAX_BLIP_MESSAGE_DATA_SIZE);
	//	Reset the current data size
	m_currentDataSize = 0;
	//	Reset the number of nodes added
	m_nodesAdded = 0;

	//	Reserve the first section for the message header
	m_pData += sizeof(sCompanionRouteMessageHeader);
	m_currentDataSize += sizeof(sCompanionRouteMessageHeader);
}

void CCompanionRouteMessage::Base64Encode(rage::u8* pDataIn, int sizeIn, char* pDataOut, int sizeOut)
{
	// 	PF_FUNC(Base64Cost);
	unsigned int charsUsed = 0;

	datBase64::Encode(pDataIn, sizeIn, pDataOut, sizeOut, &charsUsed);
}

#endif	//	COMPANION_APP