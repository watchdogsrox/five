#ifndef INC_GAME_SITUATION
#define INC_GAME_SITUATION

#include "fwutil/Flags.h"
#include "system/bit.h"

////////////////////////////////////////////////////////////////////////////////
// CGameSituation
////////////////////////////////////////////////////////////////////////////////

class CGameSituation
{

public:

	enum Flags
	{
		Combat	= BIT0,
	};

public:

	static CGameSituation& GetInstance() { FastAssert(sm_Instance); return *sm_Instance; }

private:

	CGameSituation();
	~CGameSituation();

public:

			fwFlags8&	GetFlags()					{ return m_uFlags; }
	const	fwFlags8	GetFlags()			const	{ return m_uFlags; }
			float		GetTimeInCombat()	const	{ return m_fTimeInCombat; }
	
public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);
	static void Update();

private:

	void ResetFlags();
	void Update(float fTimeStep);
	void UpdateFlags();
	void UpdateTimers(float fTimeStep);

private:

	float		m_fTimeInCombat;
	fwFlags8	m_uFlags;
	
private:

	static CGameSituation* sm_Instance;
	
};

#endif // INC_GAME_SITUATION
