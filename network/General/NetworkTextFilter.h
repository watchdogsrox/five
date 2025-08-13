//
// name:        NetworkTextFilter.h
//

#ifndef NETWORK_TEXT_FILTER_H
#define NETWORK_TEXT_FILTER_H

// rage include
#include "atl/functor.h"
#include "rline/rlgamerhandle.h"
#include "net/netratelimit.h"

class NetworkTextFilter
{
public:

	enum
	{
		MAX_TEXT_LENGTH = 256,
		MAX_RECEIVED_TEXTS = 30,
		DEFAULT_HOLDING_PEN_TIME = 500,
	};

	// callback for processing filtered / checked texts
	typedef Functor2<const rlGamerHandle&, const char*> OnReceivedTextProcessedCB;

	NetworkTextFilter();

	void Init(
		OnReceivedTextProcessedCB callback,
		const rlGamerHandle& remoteSender,
		const unsigned holdingPenMaxRequests,
		const unsigned holdingPenIntervalMs,
		const unsigned spamMaxRequests,
		const unsigned spamIntervalMs
		OUTPUT_ONLY(, const char* rateLimitName));

	void Shutdown();

	bool IsInitialised() const { return m_IsInitialised; }
	void Update();
	bool AddReceivedText(const rlGamerHandle& sender, const char* textMessage);

private:

	struct ReceivedText
	{
		enum BlockReason
		{
			Block_HoldingPen,
			Block_Spam,
		};

		ReceivedText()
			: m_ReceivedTimestamp(0)
			, m_Blocked(false)
			, m_HasCheckedHoldingPen(false)
		{
			m_Text[0] = '\0';
			m_Sender.Clear();
		}

		ReceivedText(const rlGamerHandle& sender, const char* text);

		OUTPUT_ONLY(int m_Id);
		rlGamerHandle m_Sender;
		char m_Text[MAX_TEXT_LENGTH];
		unsigned m_ReceivedTimestamp;
		bool m_Blocked;
		bool m_HasCheckedHoldingPen;
	};

	bool m_IsInitialised;
	unsigned m_HoldingPenIntervalMs;
	unsigned m_SpamIntervalMs;

#if RSG_OUTPUT
	int m_ReceivedTextIdCounter;
	static const unsigned MAX_NAME_LENGTH = 256;
	char m_Name[MAX_NAME_LENGTH];
#endif

	// remote player who can send text messages
	rlGamerHandle m_RemoteSender;

	// holding pen
	atFixedArray<ReceivedText, MAX_RECEIVED_TEXTS> m_ReceivedTexts;

	// rate limit
	netRateLimitIntervalLimit m_HoldingPenRateLimit;
	netRateLimitIntervalLimit m_SpamRateLimit;

	// callback
	OnReceivedTextProcessedCB m_Callback;
};

#endif  // NETWORK_TEXT_FILTER_H
