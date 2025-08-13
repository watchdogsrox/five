#ifndef VEHICLE_POPULATION_TUNING_H
#define VEHICLE_POPULATION_TUNING_H

#include "parser/macros.h"
#include "atl/array.h"

struct CVehiclePopulationTuning
{
public:

	virtual ~CVehiclePopulationTuning() { }

	float m_fMultiplayerRandomVehicleDensityMultiplier;
	float m_fMultiplayerParkedVehicleDensityMultiplier;

	PAR_PARSABLE;
};


// CVehGenSphere
// sphere used to define overlapping road areas
// packed as s16's since resolution is not so important as size (this will be used on SPU)
struct CVehGenSphere
{
	s16 m_iPosX;
	s16 m_iPosY;
	s16 m_iPosZ;
	u8 m_iRadius;
	u8 m_iFlags;

	enum
	{
		// Roads within this volume overlap vertically, so make vertical falloff more aggressive
		Flag_RoadsOverlap		=	0x1
	};

	PAR_SIMPLE_PARSABLE;
};

#define MAX_VEHGEN_MARKUP_SPHERES	(32)

struct CVehGenMarkUpSpheres
{
	atFixedArray<CVehGenSphere, MAX_VEHGEN_MARKUP_SPHERES> m_Spheres ;

	PAR_SIMPLE_PARSABLE;
};

#endif // VEHICLE_POPULATION_TUNING_H

