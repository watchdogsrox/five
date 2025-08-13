

/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    audiozones.h
// PURPOSE : Stuff to deal with zones (boxes/spheres) that have sounds played for them.
// AUTHOR :  Obbe.
// CREATED : 9/6/2004
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _AUDIOZONES_H_
	#define _AUDIOZONES_H_

#include "vector/vector3.h"

// Game headers
#include "camera/caminterface.h"

#define AZMAXNAMELENGTH	(8)

class CAudioZoneData
{
public:
	char	m_Name[AZMAXNAMELENGTH];
	s16	m_Sound;
	u16  m_bActive:1;		// on/off
};

class CAudioSphere : public CAudioZoneData
{
public:
	Vector3	m_Coors;
	float	m_Range;
};

class CAudioBox : public CAudioZoneData
{
public:
	s16	m_iMinX, m_iMinY, m_iMinZ;
	s16	m_iMaxX, m_iMaxY, m_iMaxZ;

	float	GetMinX() { return(m_iMinX / 8.0f); }
	float	GetMinY() { return(m_iMinY / 8.0f); }
	float	GetMinZ() { return(m_iMinZ / 8.0f); }
	
	float	GetMaxX() { return(m_iMaxX / 8.0f); }
	float	GetMaxY() { return(m_iMaxY / 8.0f); }
	float	GetMaxZ() { return(m_iMaxZ / 8.0f); }

	Vector3	GetMinCoors() { return Vector3(GetMinX(), GetMinY(), GetMinZ()); }
	Vector3	GetMaxCoors() { return Vector3(GetMaxX(), GetMaxY(), GetMaxZ()); }

	void	SetMinCoors(const Vector3& Val) { m_iMinX = (s16)(Val.x * 8.0f); m_iMinY = (s16)(Val.y * 8.0f); m_iMinZ = (s16)(Val.z * 8.0f); }
	void	SetMaxCoors(const Vector3& Val) { m_iMaxX = (s16)(Val.x * 8.0f); m_iMaxY = (s16)(Val.y * 8.0f); m_iMaxZ = (s16)(Val.z * 8.0f); }

};




class CAudioZones
{
public:

#define MAX_NUM_SPHERES (3)
#define MAX_NUM_BOXES 	(158)
#define	MAX_NUM_AT_ANY_ONE_TIME (10)

	static	CAudioSphere	m_aSpheres[MAX_NUM_SPHERES];
	static	CAudioBox		m_aBoxes[MAX_NUM_BOXES];

	static	s32	m_NumSpheres;
	static	s32	m_NumBoxes;
	
	static	s32	m_NumActiveSpheres;
	static	s32	m_NumActiveBoxes;
	
	static	s32	m_aActiveSpheres[MAX_NUM_AT_ANY_ONE_TIME];
	static	s32	m_aActiveBoxes[MAX_NUM_AT_ANY_ONE_TIME];
	
	static	bool	m_bRenderAudioZones;

	static void Init();

	static void Update(bool bForceUpdate = false, const Vector3& TestCoors = camInterface::GetPos());

	static void RegisterAudioSphere(char *pName, s32 Sound, bool bActive, float CoorsX, float CoorsY, float CoorsZ, float Range);

	static void RegisterAudioBox(char *pName, s32 Sound, bool bActive, float MinCoorsX, float MinCoorsY, float MinCoorsZ, float MaxCoorsX, float MaxCoorsY, float MaxCoorsZ);

	static void SwitchAudioZone(char *pName, bool bNewActive);
};

//////////////////////////////
// These audio zones have to match up with the ones set in max.
// Don't change willy-nilly
//

enum {
	AZ_DESERT = 0,
	AZ_GRASSLAND,
	AZ_FORREST,
	AZ_CITY,
	AZ_BISTRO,
	AZ_BEACHPARTY,
	AZ_HOUSEPARTY,
	AZ_SFHUB,
	AZ_REFINERY,
	AZ_AIRPORT,
	AZ_AWARDCEREMONY,
	AZ_MUSAK,
	AZ_SHIPDECK,
	AZ_LOUD,
	AZ_ROOM,
	AZ_RADIOROOM,
	AZ_CRACKLAB,
	AZ_CASINO,
	AZ_HOSPITAL,
	AZ_AREA69,
	AZ_ABATTOIR,
	AZ_SEVEN11,
	AZ_AIRPORTINTERIOR,
	AZ_DAMGENERATORSINTERIOR,
	AZ_OFFTRACKBETTING,
	AZ_OFFICE,
	AZ_HOUSE,
	AZ_GYM,
	AZ_BAR,
	AZ_CLUB,
	AZ_RADIO,
	AZ_HIFI,
	AZ_MOTEL,
	AZ_CELLBLOCK,
	AZ_PLEASUREDOMES,
	AZ_STORE,
	AZ_JETINT,
	AZ_COUNTRYBAR,
	AZ_TRAILERINT,
	AZ_POICESTATION,
	AZ_BONDAGE,
	AZ_STADIUM,
	AZ_HIPHOPSTORE,
	AZ_COOLSTORE,
	AZ_FASTFOOD,
	AZ_DINER,
	AZ_RADIOSTORE,
	AZ_SEXSHOP,
	AZ_AMMUNATION,
	AZ_GARAGE,
	AZ_WAREHOUSE,
	AZ_CARGOJETINT,
	AZ_RADIO_CLASSIC_HIP_HOP,
	AZ_RADIO_COUNTRY,
	AZ_RADIO_CLASSIC_ROCK,
	AZ_RADIO_DISCO_FUNK_SOUL,
	AZ_RADIO_HOUSE_CLASSICS,
	AZ_RADIO_MODERN_HIP_HOP,
	AZ_RADIO_MODERN_ROCK,
	AZ_RADIO_NEW_JACK_SWING,
	AZ_RADIO_REGGAE,
	AZ_RADIO_RARE_GROOVE,
	AZ_RADIO_TALK,
	AZ_RADIO_POLICE,
	AZ_ELECTRICAL_STORE,
	AZ_VIDEOGAME,
	AZ_STRIP_CLUB,
	AZ_STRIP_CLUB2
};

#endif

