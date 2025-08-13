#ifndef _MINIMALBUG_H_
#define _MINIMALBUG_H_

#include "debug\bugstar\BugstarIntegrationSwitch.h"
#if(BUGSTAR_INTEGRATION_ACTIVE)

#include <ctime>

class CMinimalBug
{
public:
	enum eBugCategory
	{
		kA = 0,
		kB,
		kC,
		kD,
		kTask,
		kTodo,
		kWish,

		kBugCategoryNum
	};

protected:
	int m_number;
	char* m_summary;
	float m_x;
	float m_y;
	float m_z;
	int m_severity;
	char* m_state;
	eBugCategory m_category;
	time_t m_collectedOn;
	time_t m_fixedOn;
	int m_ownerId;
	int m_qaOwnerId;
	int m_component;

public:
	CMinimalBug();
	virtual ~CMinimalBug(void);

	virtual int Number() const;
	virtual char* Summary() const;
	virtual float X() const;
	virtual float Y() const;
	virtual float Z() const;
	virtual int Severity() const;
	virtual char* State() const;
	virtual eBugCategory Category() const;
	virtual time_t CollectedOn() const;
	virtual time_t FixedOn() const;
	virtual int Owner() const;
	virtual int QaOwner() const;

	virtual void SetNumber(int value);
	virtual void SetSummary(const char* value);
	virtual void SetX(float value);
	virtual void SetY(float value);
	virtual void SetZ(float value);
	virtual void SetSeverity(int value);
	virtual void SetState(const char* value);
	virtual void SetCategory(const char* value);
	virtual void SetCategory(eBugCategory value);
	virtual void SetCollectedOn(time_t value);
	virtual void SetCollectedOn(const char* value);
	virtual void SetFixedOn(time_t value);
	virtual void SetFixedOn(const char* value);
	virtual void SetOwner(int value);
	virtual void SetQaOwner(int value);

	// fields not available in this bug implementation
	virtual const char* Description() const  { return "Sorry field not avalable.";};
	virtual const char* Comments() const  {  return "Sorry field not avalable.";};
	virtual int Status() const  { return -1;};
	
	virtual int Priority() const  { return -1;};

	virtual int Component() const  { return m_component;}

	virtual const char* Grid() const  {  return "Sorry field not avalable.";};

	virtual void SetDescription(const char* )  { };
	virtual void SetComments(const char* )  { };
	
	virtual void SetPriority(int )  { };
	virtual void SetComponent(int value )  { m_component = value; }
	virtual void SetGrid(const char* )  { };
	// end

	CMinimalBug& operator =(const CMinimalBug& other);

private:
	time_t	ConvertStringToTime(const char* timeString);
};

#endif
#endif //_MINIMALBUG_H_
