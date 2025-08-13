// 
// audio/conductorintensitymap.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_CONDUCTORMAP_H
#define AUD_CONDUCTORMAP_H

#include "vector\vector3.h"
#include "atl\array.h"

#if __BANK
 #include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "grcore/debugdraw.h"
#endif



class audConductorMap 
{
public:
	enum MapDefines
	{
		MaxGridDistance = 360,
		WidthOfZones = 20,
		DeltaAngle = 60,
		NumSectorsInZone = 360/DeltaAngle,
		NumInterestingZones = MaxGridDistance/WidthOfZones,
		NumSectors = NumInterestingZones * NumSectorsInZone,
		NumZonesInClosestArea = 3,
		NumZonesInMediumArea = 2,
		LimitClosestArea = (NumZonesInClosestArea * NumSectorsInZone)-1,
		LimitMediumArea = LimitClosestArea + (NumZonesInMediumArea * NumSectorsInZone),
		LimitFarArea = NumSectors -1,
	};
	enum MapAreas
	{
		ClosestArea = 0,
		MediumDistanceArea,
		FurtherAwayArea,
		MaxNumMapAreas,
	};
	void Init();

#if __BANK
	static void			DrawMap();
#endif 
};
#endif // AUD_CONDUCTORMAP_H

