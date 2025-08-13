//
// filename:	TracingStats.h
// 
// description:	Classes supporting a 'trace' of a variable or 'stat' over time, output to TTY.
//
//				A trace of your stat would typically be pumped to the TTY, you can choose base class methods 
//				to control frequency to output it. Or just do this yourself. 
//				
//				It might be good practice to avoid pumping out tracingstats every single frame.
//
//				Add classes as you see fit to support you custom derived class of tracingStat.
//				the derived class should support common functionality for these classes making 
//				it fairly simple to add new types of sampled stats.
//
//				The tools supporting the tty output are designed to calculate averages min & maxes of such values.
//				;however should you wish to do such sampling capture yourself there is not 
//				reason not to do this internally in your class if you desire so.
//
// author:		Derek Ward - Jan 2009

#ifndef INC_TRACING_STAT_H_
#define INC_TRACING_STAT_H_

//-------------------------------------------------------------------------------------
// Includes

#include "atl/array.h"
#include "diag/channel.h"
#include "fwsys/timer.h"
#include "system/param.h"


//-------------------------------------------------------------------------------------
// Preprocessor directives
#define TRACING_STATS __DEV

// defines
#define MB					(1024*1024)
#define MAX_ALLOCATORS		4
#define NUM_MEM_BUCKETS		16
#define MEMBUCKETS_SLOTS	((MAX_ALLOCATORS*NUM_MEM_BUCKETS) + (MAX_ALLOCATORS*3))
#define PERF_SLOTS			34

#if TRACING_STATS

RAGE_DECLARE_CHANNEL(TracingStats)
#define tsDisplayf(fmt,...)					RAGE_DISPLAYF(TracingStats,fmt,##__VA_ARGS__)

//-------------------------------------------------------------------------------------
// Frefs
class CTracingStat;

//-------------------------------------------------------------------------------------
// System control class
class CTracingStats
{
public:
	CTracingStats()	:	m_bActive(true) {}
	~CTracingStats()					{}

	void Init();
	void Shutdown();
	void Process();

private:
	void Register();
	void ProcessParams();

public:
	static const s32 DEFAULT_LIFE;
private:
	bool					m_bActive;		// active or not?
	atArray<CTracingStat*>	m_tracedStats;	// array of stats getting traced
};

//-------------------------------------------------------------------------------------
// base class 
class CTracingStat
{
	friend class CTracingStats;

public:
	CTracingStat(	const char* const pName = NULL, 
					const char* const pFileName = NULL, 
					const bool bActive = true, 
					const bool bAppendLevelNameToFile = true,
					const s32 life = CTracingStats::DEFAULT_LIFE,
					const u32 scheduleFrequency = 0);
	virtual ~CTracingStat() {}

	void	OutputTTY(const float f, const char* const pAppend = NULL, const char* const pAppend2 = NULL); 
	void	OutputTTY(const s32 i, const char* const pAppend = NULL, const char* const pAppend2 = NULL);
	void	OutputTTY(const char* const pVal, const char* const pAppend = NULL, const char* const pAppend2 = NULL); 

protected:
	virtual void	Init( )		{}
	virtual void	Shutdown( ) {}
	virtual void	Process( ) = 0;

			void	StoreSample(const float f); 
			void	StoreSample(const s32 i);
			void	StoreSample(const char* const pVal); 

			bool	FrameCounterUpdate()						{ m_frameCounter--; if (m_frameCounter>0) return false; m_frameCounter = m_frameCounterReset; return true; }
			void	PostProcess()								{ if (m_life>0) m_life--; }

			u32	GetElapsedTimeSinceLastOutputTTY() const 	{ return fwTimer::GetSystemTimeInMilliseconds() - m_lastOutputTime; } // need to handle timer wrapping?
			bool	IsActive() const 							{ return m_bActive; }
			bool	IsAlive() const								{ return m_life != 0; }
			bool	ScheduleExpired() const 					{ return GetElapsedTimeSinceLastOutputTTY() >= m_scheduleFrequency; }

			void	SetScheduleFrequency(const u32 timeMs)	{ m_scheduleFrequency = timeMs; }
			void	SetFrameCounterReset(const s32 val)		{ m_frameCounterReset = val; }
			void	SetLife(const s32 val)					{ m_life = val; }

#if __BANK
	void	CreateBankToggle();
#endif // __BANK

private:	
	char	m_fileName[255];		// The optional name of a file to group this stat historically into.
	char	m_name[255];			// the name of the Tracing stat

	u32  m_lastOutputTime;		// when was data output last? ( system time (ms) )
	u32  m_scheduleFrequency;	// millisecond frequency that the process will be called ( up to frame resolution, so ought to be run on a thread really! ) 
	u32	m_nTimesOutput;			// the number of times the tracing stat has been output to the TTY.

	s32   m_frameCounter;			// count down timer ( frames)
	s32	m_frameCounterReset;	// the reset value of the frame counter
	s32   m_life;					// the life of the stat, when it reaches 0 it no longer processes, set to -1 for infinite life.

	bool	m_bActive;				// active or not?
	bool	m_bAppendLevelNameToFile;// append the level name to the tracing stat filename?
};

//-----------------------------------------------------------
// A stat ( a value & an optional sample buffer with sizes )
template <typename T>
class CStat
{
public:
	CStat()					{ m_numSamples=0; m_maxSamples=0; m_samples = NULL; } // without 'placement new' nonsense a default constructor is required!
	CStat(T val, s32 max) { m_val=val; m_numSamples=0; m_maxSamples=max; m_samples = NULL; }
	~CStat()				{}

	void Init()
	{
		if (m_maxSamples>0)
			m_samples = rage_new T[m_maxSamples];		
	}

	void Shutdown() 
	{
		if (m_samples)
		{
			delete[] m_samples;
			m_samples = NULL;
		}
	}

	T Min()
	{
		m_val = m_samples[0];
		for (int i=1;i<m_numSamples;i++)
			if (m_samples[i]<m_val)
				m_val = m_samples[i];
		return m_val;
	}

	T Max()
	{
		m_val = m_samples[0];
		for (int i=1;i<m_numSamples;i++)
			if (m_samples[i]>m_val)
				m_val = m_samples[i];
		return m_val;
	}

	// NB. watch out for overflow, perhaps it could handle this automatically though?!
	T Mean()
	{
		m_val = m_samples[0];
		for (int i=1;i<m_numSamples;i++) 
			m_val += m_samples[i];
		m_val /= m_numSamples;
		return m_val;
	}

	bool	SamplesFull()	const			{return m_numSamples>=m_maxSamples;}
	void	ResetSamples()					{m_numSamples=0;}

	// returns true if full.
	bool Store(const T val)
	{
		if (SamplesFull())
		{
			Warningf("%d/%d samples used",m_numSamples,m_maxSamples);
			Assert(false); // should not be hit ever, in which case a sample would be lost. But this is kept in to prevent array overruns.
			return true; 
		}
		if (m_samples)
		{
			m_samples[m_numSamples]=val;
			m_numSamples++;
		}
		return SamplesFull();
	}

	// Store a sample and if you fill up the sample buffer it will auto flush by outputting to tty.
	T StoreSampleAndFlush(const T val, CTracingStat* const pStat, const char* const pAppend = NULL)
	{
		if (Store(val))
		{
			pStat->OutputTTY(Min(),pAppend,".minSampled");
			pStat->OutputTTY(Max(),pAppend,".maxSampled");
			pStat->OutputTTY(Mean(),pAppend,".meanSampled");
			ResetSamples();
		}
		return val;
	}
public:
	T			m_val;			// The value of the stat when last used.
private:
	T			*m_samples;		// an array of samples, typically processed when full.
	s32		m_numSamples;	// the number of samples used.
	s32		m_maxSamples;	// the max samples that can be stored or where allocated in m_samples.
};

// Typical class definition macro.
// Template not possible here as we need quick flexibility in the process logic, and it stored in cpp file.
// I can't help but think maybe there is a nifty way to make this templatised with the process function flexibility.
// this macro remove a lot of grunt work though, you don't have to use it if you don't like it.
// i.e. create you own class deriving from CTracingStat as you see fit based initially upon this macro.
#define CTRACINGSTATCLASS(CLASS_NAME, STAT_TYPE, STAT_TYPE_INIT, NUM_STATS)\
class CLASS_NAME : public CTracingStat\
{\
	friend class CTracingStats;\
public:\
	CLASS_NAME(	const char* const	pName = NULL,\
				const char* const	pFileName = NULL,\
				const bool			bActive = true,\
				const bool			bAppendLevelNameToFile = true,\
				const s32			life = CTracingStats::DEFAULT_LIFE,\
				const u32		scheduleFrequency = 0,\
				const u32		sampleSize = 1)\
				:CTracingStat(pName,pFileName,bActive,bAppendLevelNameToFile,life,scheduleFrequency)\
	{\
		Init();\
		for (s32 i=0;i<NUM_STATS;i++)\
		{\
			m_stat[i] = CStat<STAT_TYPE>(STAT_TYPE_INIT,sampleSize);\
			m_stat[i].Init();\
		}\
	}\
	virtual ~CLASS_NAME()\
	{\
		for (s32 i=0;i<NUM_STATS;i++)\
			m_stat[i].Shutdown();\
		Shutdown();\
	}\
private:\
	STAT_TYPE StoreSample(STAT_TYPE val, s32 bucket = 0, const char* const pAppend = NULL) { Assert(bucket<NUM_STATS); return m_stat[bucket].StoreSampleAndFlush(val,this,pAppend); }\
	virtual void	Init();\
	virtual void	Shutdown();\
	virtual void	Process();\
private:\
	CStat<STAT_TYPE> m_stat[NUM_STATS];\
};

//################################################################################
// INSERT YOUR TRACING STAT CLASS HERE
//                class name				base type   initial value	num buckets( number of slots you need to store data about what is being traced )
CTRACINGSTATCLASS(CTracingStatPerformance,	float,		0.0f,			PERF_SLOTS)
CTRACINGSTATCLASS(CTracingStatMemorySizes,	s32,		0,				MEMBUCKETS_SLOTS)

// Extern the command-line params for 'usefulness'...
XPARAM(TracingStats);
	XPARAM(TracingStatAll);
		XPARAM(TracingStatPerformance);
		XPARAM(TracingStatMemorySizes);

//-------------------------------------------------------------------------------------
// Externs
extern CTracingStats gTracingStats;

#endif // TRACING_STATS

#endif // !INC_TRACING_STAT_H_