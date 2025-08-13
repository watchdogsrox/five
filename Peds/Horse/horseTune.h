
#ifndef HRS_SIMTUNEMGR_H
#define HRS_SIMTUNEMGR_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Peds/Horse/horseFreshness.h"
#include "Peds/Horse/horseSpeed.h"
#include "Peds/Horse/horseAgitation.h"
#include "Peds/Horse/horseBrake.h"
#include "Peds/Horse/horseObstacleavoidance.h"
#include "parser/manager.h"
#include "atl/singleton.h"
#include "data/base.h"

namespace rage
{
	class bkBank;
}


////////////////////////////////////////////////////////////////////////////////
// class hrsSimTune
//
// PURPOSE: Encapsulate all sim tuning for a single horse type.
// 
class hrsSimTune
{
public:
	hrsSimTune();

	// PURPOSE: Register self and internal (private) structures
	static void RegisterParser()
	{
		static bool bInitialized = false;
		if( !bInitialized )
		{
			hrsSpeedControl::Tune::RegisterParser();
			hrsFreshnessControl::Tune::RegisterParser();
			hrsAgitationControl::Tune::RegisterParser();
			hrsBrakeControl::Tune::RegisterParser();
			REGISTER_PARSABLE_CLASS(hrsSimTune);
			bInitialized = true;
		}
	}

	const char * GetHrsTypeName() const { return m_HrsTypeName; }
	const hrsSpeedControl::Tune & GetSpeedTune() const { return m_SpeedTune; }
	const hrsFreshnessControl::Tune & GetFreshnessTune() const { return m_FreshTune; }
	const hrsAgitationControl::Tune & GetAgitationTune() const { return m_AgitationTune; }
	const hrsBrakeControl::Tune & GetBrakeTune() const { return m_BrakeTune; }

	void AddWidgets(bkBank &);

protected:
	ConstString m_HrsTypeName;
	hrsSpeedControl::Tune m_SpeedTune;
	hrsFreshnessControl::Tune m_FreshTune;
	hrsAgitationControl::Tune m_AgitationTune;
	hrsBrakeControl::Tune m_BrakeTune;

	PAR_SIMPLE_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////
// class hrsSimTuneMgr
//
// PURPOSE: manage a collection of hrsSimTune objects
//
class hrsSimTuneMgr : public datBase
{
public:
	hrsSimTuneMgr() {}
	~hrsSimTuneMgr() {}

	static void InitClass();

	// PURPOSE: Register self and internal (private) structures
	static void RegisterParser()
	{
		static bool bInitialized = false;
		if( !bInitialized )
		{
			hrsSimTune::RegisterParser();
			REGISTER_PARSABLE_CLASS(hrsSimTuneMgr);
			bInitialized = true;
		}
	}

	const hrsSimTune * FindSimTune(const char *pHrsTypeName) const;

	void Load(const char* pFileName);
	void Reload(const char *pFileName);
	
#if __BANK		
	void Save();
	void AddWidgets(bkBank &);
#endif

protected:

	atArray<hrsSimTune> m_SimTune;

	PAR_SIMPLE_PARSABLE;
};

typedef atSingleton<hrsSimTuneMgr> hrsSimTuneMgrSingleton;
#define HRSSIMTUNEMGR hrsSimTuneMgrSingleton::InstanceRef()

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif
