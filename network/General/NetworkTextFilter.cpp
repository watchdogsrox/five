//
// name:        NetworkTextFilter.cpp
//

#include "network/General/NetworkTextFilter.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, textfilter, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_textfilter

NetworkTextFilter::ReceivedText::ReceivedText(const rlGamerHandle& sender, const char* text)
	: m_Sender(sender)
{
	safecpy(m_Text, text);

	m_ReceivedTimestamp = sysTimer::GetSystemMsTime();
	m_Blocked = false;
	m_HasCheckedHoldingPen = false;
}

NetworkTextFilter::NetworkTextFilter()
	: m_IsInitialised(false)
	, m_HoldingPenIntervalMs(0)
	, m_SpamIntervalMs(0)
	OUTPUT_ONLY(, m_ReceivedTextIdCounter(0))
{
	
}

void NetworkTextFilter::Init(
	OnReceivedTextProcessedCB callback,
	const rlGamerHandle& remoteSender,
	const unsigned holdingPenMaxRequests,
	const unsigned holdingPenIntervalMs,
	const unsigned spamMaxRequests,
	const unsigned spamIntervalMs
	OUTPUT_ONLY(, const char* rateLimitName))
{
	m_Callback = callback;
	m_RemoteSender = remoteSender;

	m_HoldingPenIntervalMs = holdingPenIntervalMs;
	m_SpamIntervalMs = spamIntervalMs;

#if RSG_OUTPUT
	safecpy(m_Name, rateLimitName);

	// build out names with postfixes for each filter
	char holdingPenName[MAX_NAME_LENGTH];
	formatf(holdingPenName, "%s_HoldingPen", rateLimitName);
	char spamName[MAX_NAME_LENGTH];
	formatf(spamName, "%s_Spam", rateLimitName);
#endif

	m_HoldingPenRateLimit.Init(holdingPenMaxRequests, holdingPenIntervalMs, netRateLimitType::Second OUTPUT_ONLY(, holdingPenName));
	m_SpamRateLimit.Init(spamMaxRequests, spamIntervalMs, netRateLimitType::Second OUTPUT_ONLY(, spamName));

	m_IsInitialised = true;
}

void NetworkTextFilter::Shutdown()
{
	m_Callback = OnReceivedTextProcessedCB::NullFunctor();
	m_IsInitialised = false;
	m_ReceivedTexts.Reset();
}

void NetworkTextFilter::Update()
{
	int numTexts = m_ReceivedTexts.GetCount();
	for(int i = 0; i < numTexts; i++)
	{
		if(m_ReceivedTexts[i].m_HasCheckedHoldingPen)
			continue;
		
		if((sysTimer::GetSystemMsTime() - m_ReceivedTexts[i].m_ReceivedTimestamp) > m_HoldingPenIntervalMs)
		{
			// mark that this has been processed
			m_ReceivedTexts[i].m_HasCheckedHoldingPen = true;

			// send this text out if it's not been blocked
			if(!m_ReceivedTexts[i].m_Blocked)
			{
				if(m_Callback)
					m_Callback(m_ReceivedTexts[i].m_Sender, m_ReceivedTexts[i].m_Text);
			}
		}
	}
}

bool NetworkTextFilter::AddReceivedText(const rlGamerHandle& sender, const char* textMessage)
{
	if(!m_IsInitialised)
		return false;

	// create a received text - for all texts, we add them to a holding pen and keep them 
	// there to check for a volume of spam through a holding pen interval
	// if we reach the end of the pen time without being blocked then the text is released
	ReceivedText receivedText = ReceivedText(sender, textMessage);
	OUTPUT_ONLY(receivedText.m_Id = m_ReceivedTextIdCounter++);

	// logging
	gnetDebug1("AddReceivedText [%s/%d] :: Sender: %s, Text: %s", m_Name, receivedText.m_Id, sender.ToString(), textMessage);

	// a value of 0 disables the holding pen filter
	if(m_HoldingPenIntervalMs > 0)
	{
		// for holding pen limits, we retroactively block all texts if we burst our holding pen limit
		if(m_HoldingPenRateLimit.m_TokenBucket.CanConsume(1.0f))
		{
			// add this request
			m_HoldingPenRateLimit.m_TokenBucket.Consume(1.0f);
		}
		else
		{
			// we broke our limit - block this text and any others within our holding pen time
			gnetDebug1("AddReceivedText :: HoldingPen Violation!");

			// we block all texts in the holding pen if our holding pen rate limit is broken
			int numTexts = m_ReceivedTexts.GetCount();
			for(int i = 0; i < numTexts; i++)
			{
				if(!m_ReceivedTexts[i].m_Blocked && (sysTimer::GetSystemMsTime() - m_ReceivedTexts[i].m_ReceivedTimestamp) < m_HoldingPenIntervalMs)
				{
					// we broke our limit - block this text and any others within our holding pen time
					gnetDebug1("AddReceivedText [%s/%d] :: Blocking for HoldingPen violation", m_Name, m_ReceivedTexts[i].m_Id);
					m_ReceivedTexts[i].m_Blocked = true;
				}
			}

			// block this one last (so that it's ordered in logging)
			gnetDebug1("AddReceivedText [%s/%d] :: Blocking for HoldingPen violation", m_Name, receivedText.m_Id);
			receivedText.m_Blocked = true;
		}
	}
	
	// a value of 0 disables the spam filter
	if(m_SpamIntervalMs > 0)
	{
		// for spam, we just block this text
		if (m_SpamRateLimit.m_TokenBucket.CanConsume(1.0f))
		{
			// add this request
			m_SpamRateLimit.m_TokenBucket.Consume(1.0f);
		}
		else
		{
			// we broke our limit - block this text and any others within our holding pen time
			gnetDebug1("AddReceivedText [%s/%d] :: Blocking for Spam violation", m_Name, receivedText.m_Id);
			receivedText.m_Blocked = true;
		}
	}

	// if full, just chuck out the first entry
	// don't delete old texts on a time value - keep them around so they can be referenced
	// and checked (i.e. if the same text is continually spammed)
	if(m_ReceivedTexts.IsFull())
	{
		// don't do DeleteFast, maintain FIFO order
		m_ReceivedTexts.Delete(0);
	}

	// add the text
	m_ReceivedTexts.Push(receivedText);

	return true;
}

