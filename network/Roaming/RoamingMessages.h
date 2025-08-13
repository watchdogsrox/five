//
// name:        RoamingMessages.h
// description: Contains implementations of the different network messages sent/received to manage roaming sessions
// written by:  Daniel Yelland
//

#ifndef ROAMING_MESSAGES_H
#define ROAMING_MESSAGES_H

#include "net/message.h"
#include "fwnet/nettypes.h"

#include "network/Roaming/RoamingTypes.h"

namespace rage
{
    class netPlayer;
}

class CRoamingInitialBubbleMsg
{
public:

    NET_MESSAGE_DECL(CRoamingInitialBubbleMsg, CROAMING_INITIAL_BUBBLE_MSG);

    CRoamingInitialBubbleMsg() {}
    CRoamingInitialBubbleMsg(const CRoamingBubbleMemberInfo &joinerMemberInfo,
                             const CRoamingBubbleMemberInfo &hostMemberInfo) :
    m_JoinerBubbleMemberInfo(joinerMemberInfo)
    , m_HostBubbleMemberInfo(hostMemberInfo) {}

    const CRoamingBubbleMemberInfo& GetJoinerBubbleMemberInfo() const { return m_JoinerBubbleMemberInfo; }
    const CRoamingBubbleMemberInfo& GetHostBubbleMemberInfo()   const { return m_HostBubbleMemberInfo;   }

    void WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const;

    NET_MESSAGE_SER(bb, msg)
	{
		return msg.m_JoinerBubbleMemberInfo.SerialiseMsg(bb) &&
               msg.m_HostBubbleMemberInfo.SerialiseMsg(bb);
	}

private:

	CRoamingBubbleMemberInfo m_JoinerBubbleMemberInfo; // The membership info for the bubble we are joining
    CRoamingBubbleMemberInfo m_HostBubbleMemberInfo;   // The membership info for the host of the bubble we are joining
};

class CRoamingRequestBubbleRequiredCheckMsg
{
public:

	NET_MESSAGE_DECL(CRoamingRequestBubbleRequiredCheckMsg, CROAMING_BUBBLE_REQUIRED_CHECK_MSG);

	CRoamingRequestBubbleRequiredCheckMsg() {}

	void WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const;

	NET_MESSAGE_SER(/*bb*/, /*msg*/)
	{
		return true; 
	}

private:

};

class CRoamingRequestBubbleRequiredResponseMsg
{
public:

	NET_MESSAGE_DECL(CRoamingRequestBubbleRequiredResponseMsg, CROAMING_BUBBLE_REQUIRED_RESPONSE_MSG);

	CRoamingRequestBubbleRequiredResponseMsg() {}
	CRoamingRequestBubbleRequiredResponseMsg(const bool bIsRequired, const CRoamingBubbleMemberInfo& memberInfo) 
		: m_Required(bIsRequired)
		, m_BubbleMemberInfo(memberInfo) {}

	bool IsRequired() const { return m_Required; }
	const CRoamingBubbleMemberInfo& GetBubbleMemberInfo() const { return m_BubbleMemberInfo; }

	void WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const;

	NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerBool(msg.m_Required) &&
			msg.m_BubbleMemberInfo.SerialiseMsg(bb);
	}

private:

	bool m_Required; 
	CRoamingBubbleMemberInfo m_BubbleMemberInfo;
};

class CRoamingRequestBubbleMsg
{
public:

	NET_MESSAGE_DECL(CRoamingRequestBubbleMsg, CROAMING_REQUEST_BUBBLE_MSG);

	CRoamingRequestBubbleMsg() {}

	void WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const;

	NET_MESSAGE_SER(/*bb*/, /*msg*/)
	{
		return true; 
	}

private:

};

class CRoamingJoinBubbleMsg
{
public:

    NET_MESSAGE_DECL(CRoamingJoinBubbleMsg, CROAMING_JOIN_BUBBLE_MSG);

    CRoamingJoinBubbleMsg() {}
    CRoamingJoinBubbleMsg(unsigned bubbleID) : m_BubbleID(bubbleID) {}

    unsigned GetBubbleID() const { return m_BubbleID; }

    void WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const;

    NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_BubbleID, CRoamingBubbleMemberInfo::SIZEOF_BUBBLE_ID);
	}

private:

    unsigned m_BubbleID; // The ID of the bubble we are requesting to join
};

class CRoamingJoinBubbleAckMsg
{
public:

    NET_MESSAGE_DECL(CRoamingJoinBubbleAckMsg, CROAMING_JOIN_BUBBLE_ACK_MSG);

    enum AckCode
    {
        ACKCODE_SUCCESS,
        ACKCODE_UNKNOWN_FAIL,
        ACKCODE_NOT_LEADER,
        ACKCODE_BUBBLE_FULL
    };

    CRoamingJoinBubbleAckMsg() {}
    CRoamingJoinBubbleAckMsg(AckCode ackCode, const CRoamingBubbleMemberInfo& memberInfo) : 
		m_AckCode(static_cast<unsigned>(ackCode)),
		m_BubbleMemberInfo(memberInfo) {}

    AckCode GetAckCode() const { return static_cast<AckCode>(m_AckCode); }
	const CRoamingBubbleMemberInfo& GetBubbleMemberInfo() const { return m_BubbleMemberInfo; }

    void WriteToLogFile(bool received, netSequence seqNum, const netPlayer &player) const;

    NET_MESSAGE_SER(bb, msg)
	{
		return bb.SerUns(msg.m_AckCode,  SIZEOF_ACK_CODE) &&
			   msg.m_BubbleMemberInfo.SerialiseMsg(bb);
	}

private:

    static const unsigned SIZEOF_ACK_CODE = 2;

    static const char *GetAckCodeString(unsigned ackCode);

    unsigned                 m_AckCode;          // The Ack code indicating the response to the request
	CRoamingBubbleMemberInfo m_BubbleMemberInfo; // The membership info for the bubble we have joined
};

#endif // ROAMING_MESSAGES_H
