//
//	vehicle_common_values.h
//
//	2010/12/20	-	Andrzej:	- initial;
//  2013/10/15  -   Bela:       - add support for damage texture writing on GPU
//
//
//

#ifndef __VEHICLE_COMMON_VALUES_H__
#define __VEHICLE_COMMON_VALUES_H__

// If you modify these values, then you also need to update the corresponding values
// in rage/base/src/grcore/edge_vehicledamage.spa

#define		GTA_VEHICLE_DAMAGE_TEXTURE_SIZE			(128)	// logical damage map size: 128x128
#define		GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH		(128)	// physical dimensions of damage map:
															//This should be power of 2
#if RSG_PS3
	#define GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT		(96)	// EDGE: 128x96x4 (or 128x128x3)=48KB
#else
	#define GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT		(128)	// 360:  128x128x4=64KB
#endif

#define VEHICLE_DEFORMATION_SOFT_CAR                (0)
#define VEHICLE_DEFORMATION_STOP_AFTER_WRECKAGE     (1)

// The rate at which larger vehicles have their damage emphasized more due to their size, Tailgater = 0 (min), Titan = 1 (max)
// The overall damage multiplier is thus = 1 (base) + [0->1 based on relative size) * VEHICLE_DEFORMATION_PROPORTIONAL_SCALE_SLOPE
#define VEHICLE_DEFORMATION_PROPORTIONAL			 (0)
#define VEHICLE_DEFORMATION_PROPORTIONAL_SCALE_SLOPE (4.0f)

#define VEHICLE_OUTWARD_EXPLOSION_CHANCE			(0.5f)
#define VEHICLE_DEFORMATION_INVERSE_SQUARE_FIELD    (0)
#define VEHICLE_DEFORMATION_UNCLAMPED_EXPLOSIVES    (1)          // Damage clamping occurs when weapon damage is incoming, enable this to take 100% damage from rocket launchers, grenades, c4, molotov cocktails
#define	GTA_VEHICLE_DAMAGE_DELTA_SCALE			    (0.5f)
#define	GTA_VEHICLE_MIN_DAMAGE_RESOLUTION		    (0.0078125f) // (1.0f/128.0f) // s8
#define GTA_VEHICLE_MAX_DAMAGE_RESOLUTION		    (128.0f)    // normalized -128 -> 127 is actually -1 -> 1
#define GTA_DAMAGE_TEXTURE_BPP						4			// sizeof(half) * 4

#define VEHICLE_DEFORMATION_NOISE_WIDTH  (GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH  / 4) //This should be power of 2
#define VEHICLE_DEFORMATION_NOISE_HEIGHT (GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT / 4)
#define VEHICLE_DEFORMATION_NOISE_WIDTH_EXPANDED (VEHICLE_DEFORMATION_NOISE_WIDTH + 3)

#define VEHICLE_SUPPORT_PAINT_RAMP (1)

#define GPU_DAMAGE_WRITE_ENABLED ((RSG_ORBIS * 1) + (RSG_DURANGO * 1) + (RSG_PC * 1) + (RSG_XENON * 0) + (RSG_PS3 * 0))

#if GPU_DAMAGE_WRITE_ENABLED
	#define GPU_VEHICLE_DAMAGE_ONLY(expr) expr
#if RSG_XENON || RSG_PS3 
	#define MAX_DAMAGED_VEHICLES			  (16) // Current gen should have 16 textures, max
	#define MAX_IMPACTS_PER_VEHICLE_PER_FRAME (4) 
	#define MAX_BOUND_DEFORMATION_PER_FRAME   (2)
#else
	#define MAX_DAMAGED_VEHICLES			  (40) // Next Gen and PC has some spare memory, so allow 40 vehicles to be damaged at once
	#define MAX_IMPACTS_PER_VEHICLE_PER_FRAME (16)
	#define MAX_BOUND_DEFORMATION_PER_FRAME   (2)
#endif
	
#else
	#define GPU_VEHICLE_DAMAGE_ONLY(expr)
	#define MAX_DAMAGED_VEHICLES			  (16)
	#define MAX_IMPACTS_PER_VEHICLE_PER_FRAME (4)
	#define MAX_BOUND_DEFORMATION_PER_FRAME   (2)
#endif

#define GTA_DAMAGE_TEXTURE_BYTES GTA_DAMAGE_TEXTURE_BPP * GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH * GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT


#endif //__VEHICLE_COMMON_VALUES_H__...
