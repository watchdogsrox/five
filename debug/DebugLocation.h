#ifndef INC_DEBUG_LOCATION_H_
#define INC_DEBUG_LOCATION_H_

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "atl/delegate.h"
#include "data/callback.h"
#include "parser/macros.h"
#include "vector/matrix34.h"
#include "vector/vector3.h"

namespace rage
{
	class bkCombo;
};

class debugLocation
{
public:

	void BeamMeUp();
	void Set(Vector3 &pos, Matrix34 &camMtx, u32 h);
	void Set(float *data);
	bool SetFromCurrent();
	void WriteOut(char *buffer);

	
	atHashString name;
	
	Vector3 playerPos;
	Matrix34 cameraMtx;
	u32 hour;

	PAR_SIMPLE_PARSABLE;
};


class debugLocationList
{
public:
	typedef atDelegate<bool (int)> visitCallback;
	
	void Init(bkBank* bank);
	
	void Reset();
	void Load();
	void Add();
	void Select();
#if __BANK	
	void Save();
	void LetsDoTheTimeWarp();
#endif	
	void Update(float time);
	
	void ApplyBootParam();
	void Visit(float timeBeforeWarp, float timeBeforeCallBack = 0.0f);
    void StopVisiting() { visiting = false; }
	void SetVisitCallback(visitCallback callback) { currentVisitCallback = callback; }  
	void ClearVisitCallback() { currentVisitCallback.Reset(); }
	
#if !__FINAL
	const char * GetLocationName(int idx) { return locationList[idx]->name.GetCStr(); }
#endif	//	!__FINAL

	int GetLocationsCount() { return locationList.GetCount(); }
	void SetCurrentLocation(int index) { Assert(index >= 0 && index <= locationList.GetCount()-1); currentLocation = index; Select(); }
    int GetCurrentLocation() const { return currentLocation; }

    bool Visiting() const { return visiting; }
	bool SetLocationByName(const char* name);
	
private:	
	atArray<debugLocation*> locationList;
#if __BANK
	atArray<const char*> locationNames;
	char				m_newLocationName[128];
	
	bkCombo *comboList;
	float bkTimeWarp;
#endif // __BANK;

	int currentLocation;
	
	bool visiting;
	bool justStarted;
	bool cbCalled;
	float timeWarp;
	float timeCallback;
	float timeAtCurrentLocation;
	visitCallback currentVisitCallback;
	
	PAR_SIMPLE_PARSABLE;
};

#endif // INC_DEBUG_LOCATION_H_