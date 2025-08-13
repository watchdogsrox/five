#ifndef INC_RIOTS_H
#define INC_RIOTS_H

// Rage headers
#include "parser/macros.h"

// Game headers
#include "task/System/Tuning.h"

// Forward declarations
class CPed;

////////////////////////////////////////////////////////////////////////////////
// CRiots
////////////////////////////////////////////////////////////////////////////////

class CRiots
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		PAR_PARSABLE;
	};

public:

	static CRiots& GetInstance() { FastAssert(sm_Instance); return *sm_Instance; }

private:

	CRiots();
	~CRiots();

public:

	bool GetEnabled() const { return m_bEnabled; }
	void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

public:

	void UpdateInventory(CPed& rPed);
	
public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

private:

	bool m_bEnabled;
	
private:

	static CRiots* sm_Instance;

private:

	static Tunables sm_Tunables;
	
};

#endif // INC_RIOTS_H
