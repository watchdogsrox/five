//
// name:        RoamingMessages.cpp
// description: Contains implementations of the different network messages sent/received to manage roaming sessions
// written by:  Daniel Yelland
//
#include "RoamingMessages.h"

#include "fwnet/netchannel.h"
#include "fwnet/netinterface.h"
#include "fwnet/netlogsplitter.h"
#include "fwnet/netlogutils.h"
#include "fwnet/netplayermgrbase.h"

NET_MESSAGE_IMPL(CRoamingInitialBubbleMsg)
NET_MESSAGE_IMPL(CRoamingRequestBubbleMsg);
NET_MESSAGE_IMPL(CRoamingRequestBubbleRequiredCheckMsg);
NET_MESSAGE_IMPL(CRoamingRequestBubbleRequiredResponseMsg);
NET_MESSAGE_IMPL(CRoamingJoinBubbleMsg);
NET_MESSAGE_IMPL(CRoamingJoinBubbleAckMsg);

void CRoamingInitialBubbleMsg::WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const
{
    netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

    if (received)
    {
        NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "RECEIVED_INITIAL_BUBBLE", "");
    }
    else
    {
        NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "SENDING_INITIAL_BUBBLE", "");
    }

    log.WriteDataValue("Joiner Bubble ID", "%d", m_JoinerBubbleMemberInfo.GetBubbleID());
    log.WriteDataValue("Joiner Player ID", "%d", m_JoinerBubbleMemberInfo.GetPlayerID());
    log.WriteDataValue("Host Bubble ID", "%d", m_HostBubbleMemberInfo.GetBubbleID());
    log.WriteDataValue("Host Player ID", "%d", m_HostBubbleMemberInfo.GetPlayerID());
}

void CRoamingRequestBubbleRequiredCheckMsg::WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const
{
	netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

	if (received)
	{
		NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "RECEIVED_REQUEST_BUBBLE", "");
	}
	else
	{
		NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "SENDING_REQUEST_BUBBLE", "");
	}
}

void CRoamingRequestBubbleRequiredResponseMsg::WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const
{
	netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

	if (received)
	{
		NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "RECEIVED_REQUEST_BUBBLE", "");
	}
	else
	{
		NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "SENDING_REQUEST_BUBBLE", "");
	}

	log.WriteDataValue("Required", "%s", m_Required ? "True" : "False");
	log.WriteDataValue("Bubble ID", "%d", m_BubbleMemberInfo.GetBubbleID());
	log.WriteDataValue("Player ID", "%d", m_BubbleMemberInfo.GetPlayerID());
}

void CRoamingRequestBubbleMsg::WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const
{
	netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

	if (received)
	{
		NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "RECEIVED_REQUEST_BUBBLE", "");
	}
	else
	{
		NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "SENDING_REQUEST_BUBBLE", "");
	}
}

void CRoamingJoinBubbleMsg::WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const
{
    netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

    if (received)
    {
        NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "RECEIVED_JOIN_BUBBLE", "");
    }
    else
    {
        NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "SENDING_JOIN_BUBBLE", "");
    }

    log.WriteDataValue("Bubble ID", "%d", m_BubbleID);
}

void CRoamingJoinBubbleAckMsg::WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const
{
    netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

    if (received)
    {
        NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "RECEIVED_JOIN_BUBBLE_ACK", "");
    }
    else
    {
        NetworkLogUtils::WriteMessageHeader(log, received, seqNum, player, "SENDING_JOIN_BUBBLE_ACK", "");
    }

    log.WriteDataValue("Ack Code", "%s", GetAckCodeString(m_AckCode));
    log.WriteDataValue("Bubble ID", "%d", m_BubbleMemberInfo.GetBubbleID());
    log.WriteDataValue("Player ID", "%d", m_BubbleMemberInfo.GetPlayerID());
}

const char *CRoamingJoinBubbleAckMsg::GetAckCodeString(unsigned ackCode)
{
    switch(ackCode)
    {
    case ACKCODE_SUCCESS:
        return "SUCCESS";
    case ACKCODE_UNKNOWN_FAIL:
        return "UNKNOWN";
    case ACKCODE_NOT_LEADER:
        return "NOT LEADER";
    case ACKCODE_BUBBLE_FULL:
        return "BUBBLE FULL";
    }

    gnetAssertf(0, "Unexpected Ack Code!");
    return "?";
}
