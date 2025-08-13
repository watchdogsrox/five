// Title	:	Performance.h
// Author	:	Phil Hooker
// Started	:	05/02/08
//
// Purpose : Keeps track of the estimated games performance, stores information
//			 That other systems can query to reduce features in order to improve framerate.
//

#ifndef INC_PERFORMANCE_H_
#define INC_PERFORMANCE_H_


// Keeps track of the estimated games performance, stores information
// That other systems can query to reduce features in order to improve framerate.
class CPerformance
{
public:
	CPerformance();
	~CPerformance();

	enum 
	{
		PF_WantedLevel		= (1<<0),
		PF_PlayerSpeed		= (1<<1),
		PF_MissionFlag		= (1<<2),
	} PerformanceFlags;

	// Called each frame, checks conditions and records whether low performance is estimated
	void Update();

	// If this returns true, conditions in the game are such that poor performance is anticipated.
	bool EstimateLowPerformance();
	u32 GetLowPerformanceFlags() const { return m_iLowPerformanceFlags; }

	// This is a reset flag and needs to be set each frame
	void SetLowPerformanceFlagFromScript(bool val) { m_bSetLowPerformanceFlagFromScript = val; }

	//Static member fns
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static CPerformance* GetInstance() 
	{ 
		Assert(ms_pInstance); 
		return ms_pInstance;
	}
	static CPerformance* ms_pInstance;

private:
	// Flags updated each frame, if non-zero low performance is estimated
	u32 m_iLowPerformanceFlags;

	// Wanted level is such that the number of cops will slow down the framerate
	void CheckWantedLevel();

	// The player has been moving at a sufficiently high speed that will impact performance
	void CheckPlayerSpeed();
	float m_fLastTimePlayerMovingAtHighSpeed;
	static float ms_fPlayerSpeedConsideredHigh;
	static float ms_fTimeToReportLowPerformanceOnHighSpeed;

	// Set by script in intensive missions that require
	void CheckLowMissionPerformance();
	bool m_bSetLowPerformanceFlagFromScript; // Reset each frame
};

// required for this class to interface with the game skeleton code
class CPerformanceWrapper
{
public:

    static void Update() { CPerformance::GetInstance()->Update(); }
};

#endif
