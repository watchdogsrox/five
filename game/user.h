// Title	:	user.h
// Author	:	Graeme
// Started	:	27.06.00

#ifndef _USER_H_
#define _USER_H_

// Game headers
#include "control\onsctime.h"
#include "game\zones.h"
#include "peds/popzones.h"

#define MAX_ZONE_NAME_CHARS	(128)

class CPlaceName
{
private :
//	CZone *pLocalNavZone;
	CPopZone *pNavZone;
	u16 Timer;
	bool m_bUseLocalNavZones;
	char m_NameTextKey[MAX_ZONE_NAME_CHARS];	
	char cOldName[MAX_ZONE_NAME_CHARS];
	char m_cName[MAX_ZONE_NAME_CHARS];  // to store the name

public :
	CPlaceName(void);
	void Init(bool bUseLocalNavZones);
	const char *GetForMap(float x, float y);
	void Process(void);
	void ForceSetToDisplay();
	void Display(void);
	const char *GetName() { return m_cName; }
	const char *GetNameTextKey() { return m_NameTextKey; }
	bool IsNewName();
};

class CVehicle;

class CCurrentVehicle
{
private :
	char cOldName[MAX_ZONE_NAME_CHARS];
	char cName[MAX_ZONE_NAME_CHARS];  // to store the name

public :
	char *GetName() { return cName; }
	bool IsNewName();
	CCurrentVehicle(void);
	void Init(void);
	void Process(void);
	void Display(void);
};
	
class CStreetName
{
public:
#define NUM_REMEMBERED_STREET_NAMES (16)
	u32	RememberedStreetName[NUM_REMEMBERED_STREET_NAMES];
	u32  TimeInRange[NUM_REMEMBERED_STREET_NAMES];
private:
	char cOldName[MAX_ZONE_NAME_CHARS];
	char m_cName[MAX_ZONE_NAME_CHARS];  // to store the name
	u32 m_HashOfNameTextKey;

public :
	CStreetName(void);
	void Init(void);
	char const *GetForMap(float x, float y);
	char const *GetName() { return m_cName; }
	u32 GetHashOfNameTextKey() { return m_HashOfNameTextKey; }
	bool IsNewName();
	void Process(void);
	void UpdateStreetNameForDPadDown();
	void ForceSetToDisplay();
	void Display(void);
};

class CUserDisplay
{
public :
	static CPlaceName AreaName;
	static CPlaceName DistrictName;
	static CStreetName StreetName;
	static COnscreenTimer OnscnTimer;
	static CCurrentVehicle CurrentVehicle;

public :

//	CUserDisplay(void);
	static void Init(unsigned initMode);
	static void Process(void);
//	static void Display(void);
};


//extern CPlaceName CUserDisplay::PlaceName;

#endif
