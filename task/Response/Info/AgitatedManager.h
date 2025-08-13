#ifndef INC_AGITATED_MANAGER_H
#define INC_AGITATED_MANAGER_H

// Rage headers
#include "data/base.h"

// Game headers
#include "Task/Response/Info/AgitatedPersonality.h"
#include "Task/Response/Info/AgitatedResponse.h"
#include "Task/Response/Info/AgitatedTrigger.h"

////////////////////////////////////////////////////////////////////////////////
// CAgitatedManager
////////////////////////////////////////////////////////////////////////////////

class CAgitatedManager : public datBase
{

public:

	static CAgitatedManager& GetInstance()
	{	FastAssert(sm_Instance); return *sm_Instance;	}
	
private:

	explicit CAgitatedManager();
	~CAgitatedManager();

public:
	
	const	CAgitatedPersonalities&	GetPersonalities()	const	{ return m_Personalities; }
	const	CAgitatedResponses&		GetResponses()		const	{ return m_Responses; }
			CAgitatedTriggers&		GetTriggers()				{ return m_Triggers; }
	const	CAgitatedTriggers&		GetTriggers()		const	{ return m_Triggers; }
	
public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

public:

	// Support for suppression of agitation events.  Reset every frame.
	bool	AreAgitationEventsSuppressed()				const	{ return m_bAgitationEventsAreSuppressed; }
	void	SetSuppressAgitationEvents(bool bSuppress)			{ m_bAgitationEventsAreSuppressed = bSuppress; }


	
private:

	CAgitatedPersonalities	m_Personalities;
	CAgitatedResponses		m_Responses;
	CAgitatedTriggers		m_Triggers;
	bool					m_bAgitationEventsAreSuppressed;
	
private:
	
	static CAgitatedManager* sm_Instance;
	
};

#endif // INC_AGITATED_MANAGER_H
