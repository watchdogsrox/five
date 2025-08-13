/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    zonecull.h
// PURPOSE : Stuff for culling models on the map
// AUTHOR :  Obbe.
// CREATED : 14/1/01
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _ZONECULL_H_
	#define _ZONECULL_H_


// framework
#include "vfx/vfxlist.h"

//#include "basetypes.h"
#include "renderer\color.h"
//#include "scene\Entity.h"
//#include "pools.h" rage conversion


#define MAX_NUM_ATTRIBUTE_ZONES (1550) //was 1175 then 1300
//#define MAX_NUM_TUNNEL_ATTRIBUTE_ZONES (40)
#define MAX_NUM_MIRROR_ATTRIBUTE_ZONES (72)


#define ZONE_FLAGS_CAMCLOSEIN 			(1<<0)
#define ZONE_FLAGS_STAIRS	 			(1<<1)
#define ZONE_FLAGS_1STPERSON 			(1<<2)
#define ZONE_FLAGS_NORAIN	 			(1<<3)
#define ZONE_FLAGS_NOPOLICE	 			(1<<4)
#define ZONE_FLAGS_NOCALCULATION		(1<<5)
#define ZONE_FLAGS_LOADCOLLISION		(1<<6)
#define ZONE_FLAGS_TUNNELTRANSITION		(1<<7)		// Tunnel objects as well as non tunnel objects can be seen
#define ZONE_FLAGS_POLICEABANDONCARS	(1<<8)
#define ZONE_FLAGS_INROOMFORAUDIO		(1<<9)
#define ZONE_FLAGS_FEWERPEDS			(1<<10)		// Reduce the number of peds in this area somewhat
#define ZONE_FLAGS_TUNNEL				(1<<11)		// Objects in this area are considered part of a tunnel.
#define ZONE_FLAGS_MILITARYAREA			(1<<12)		// If the player enters here he'll get an immediate wanted level.
#define ZONE_FLAGS_TUNNELCOLOURS		(1<<13)		// Will we use the extra colours for tunnels here?
#define ZONE_FLAGS_EXTRAAIRRESISTANCE	(1<<14)		// Air resistance is stronger in this area
#define ZONE_FLAGS_FEWERCARS			(1<<15)		// Reduce the number of cars in this area somewhat


class CZoneDef
{
public:
	s16		CornerX, CornerY;
	s16		Vec1X, Vec1Y;
	s16		Vec2X, Vec2Y;
	s16		MinZ, MaxZ;
	
	bool 		IsPointWithin(const Vector3& Point);
	Vector3 	FindCenter();
	void		FindBoundingBox(Vector3 *pMin, Vector3 *pMax);

#if __BANK
	void 		Render(const Vector3& CamPos, CRGBA Colour);
#endif // __BANK
};

/////////////////////////////////////////////////////////////////////////////////
// CAttributeZone is used for cullzones that have attributes. They are much smaller.
/////////////////////////////////////////////////////////////////////////////////

class CAttributeZone : public vfxListItem
{
public:
	CZoneDef	ZoneDef;
	
	s16		Flags;				// Flags for this range.
//	s16		WantedLevelDrop;	// How fast does the wanted level drop in a dark alley (20 is about average)
};


/////////////////////////////////////////////////////////////////////////////////
// CMirrorAttributeZone is used to specify mirrors. (and screens in clubs etc)
/////////////////////////////////////////////////////////////////////////////////

enum {  MIRROR_FLAGS_NONE = 0,
		MIRROR_FLAGS_MIRROR = (1<<0),
		MIRROR_FLAGS_SCREENS_1 = (1<<1),
		MIRROR_FLAGS_SCREENS_2 = (1<<2),
		MIRROR_FLAGS_SCREENS_3 = (1<<3),
		MIRROR_FLAGS_SCREENS_4 = (1<<4),
		MIRROR_FLAGS_SCREENS_5 = (1<<5),
		MIRROR_FLAGS_SCREENS_6 = (1<<6)		
		};

class CMirrorAttributeZone
{
public:
	CZoneDef	ZoneDef;
	
	float		MirrorV;
	s8		MirrorNormalX, MirrorNormalY, MirrorNormalZ;
	s8		Flags;				// Flags for this range.
};


/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions. (The functions are all static)
/////////////////////////////////////////////////////////////////////////////////

class CCullZones
{
public:
//	static		bool			bAtBeachForAudio;
	
	// Attribute zones. (stripped down versions of cullzones that do not cull)
	static		s32			NumAttributeZones;
	static		s32			NumMirrorAttributeZones;
//	static		s32			NumTunnelAttributeZones;

	static		s32			CurrentFlags_Camera;
	static		s32			CurrentFlags_Player;
//	static		s32			CurrentWantedLevelDrop_Player;

//	static		bool			bMilitaryZonesDisabled;

	static		CAttributeZone			aAttributeZones[MAX_NUM_ATTRIBUTE_ZONES];
//	static		CAttributeZone			aTunnelAttributeZones[MAX_NUM_TUNNEL_ATTRIBUTE_ZONES];
	static		CMirrorAttributeZone	aMirrorAttributeZones[MAX_NUM_MIRROR_ATTRIBUTE_ZONES];

	static		vfxList					m_camZoneList;											// list of zones close to the camera
	static		s32					m_numCamZones;

	static		bool			bRenderCullzones;

	static		void	Init(unsigned initMode);
	static		void	Update();
//	static		void	ForceCullZoneCoors(Vector3 Coors);
	static		s32	FindAttributesForCoors(const Vector3& TestCoors); //, s32 *pWantedDrop = NULL);
//	static		s32	FindTunnelAttributesForCoors(const Vector3& TestCoors);
	static		CMirrorAttributeZone	*FindMirrorAttributesForCoors(const Vector3& TestCoors);
	static		CAttributeZone *FindZoneWithStairsAttributeForPlayer();
	static		void	AddCullZone(const Vector3& posn, float ArgMinX, float ArgMaxX, float ArgMinY, float ArgMaxY, float ArgMinZ, float ArgMaxZ, u16 ArgFlags, s16 ArgWantedLevelDrop);
	static		void	AddMirrorAttributeZone(const Vector3& posn, float ArgMinX, float ArgMaxX, float ArgMinY, float ArgMaxY, float ArgMinZ, float ArgMaxZ, u16 ArgFlags, float MirrorV, float MirrorNormalX, float MirrorNormalY, float MirrorNormalZ);
//	static		void	AddTunnelAttributeZone(const Vector3& posn, float ArgVec1X, float ArgVec1Y, float ArgMinZ, float ArgVec2X, float ArgVec2Y, float ArgMaxZ, u16 ArgFlags);

	// These are the functions called by other bits of code to work out what the attributes are
	static		bool	CamCloseInForPlayer(void);
	static		bool	CamStairsForPlayer(void);
	static		bool	Cam1stPersonForPlayer(void);
	static		bool	NoPolice(void);
	static		bool	PoliceAbandonCars(void);
	static		bool	InRoomForAudio(void);
	static		bool	FewerCars(void);
	static		bool	FewerPeds(void);
	static		bool	CamNoRain(void);
	static		bool	PlayerNoRain(void);
	static 		bool 	DoINeedToLoadCollision(void);
//	static		s32	GetWantedLevelDrop(void);
	static		bool 	DoExtraAirResistanceForPlayer(void);
//	static		bool	AtBeachForAudio(void) { return bAtBeachForAudio; };
//	static		void	UpdateAtBeachForAudio(void);
#if __BANK
	static		void	Render();
#endif // __BANK

};


#endif
