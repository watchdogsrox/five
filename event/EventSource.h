#ifndef INC_EVENT_SOURCE_H
#define INC_EVENT_SOURCE_H

class CEvent;
class CPed;

class CEventSource
{
public:

	CEventSource()  {}
	~CEventSource() {}

	static int ComputeEventSourceType(const CEvent& event, const CPed& pedActedOnByEvent);
};

#endif // INC_EVENT_SOURCE_H
