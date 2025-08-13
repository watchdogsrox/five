// ===============================
// vfx/vehicleglass/VehicleGlass.h
// (c) 2012 RockstarNorth
// ===============================

#ifndef VEHICLEGLASS_H
#define VEHICLEGLASS_H

#if 0 && __DEV
	#define VEHICLE_GLASS_OPTIMISATIONS OPTIMISATIONS_OFF
#else
	#define VEHICLE_GLASS_OPTIMISATIONS VFX_MISC_OPTIMISATIONS
#endif

class CEntity;

bool IsEntitySmashable(const CEntity* pEntity);

#define VEHICLE_GLASS_SMASH_TEST           (0 && __DEV)
#define VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT (1 && __DEV)
#define VEHICLE_GLASS_COMPRESSED_VERTICES  (1 && !__D3D11)

// When running in Beta with optimizations off our stack is not large enough to contain uncompressed triangles
// By using Float16Vec4 instead of Vec4V when can still fit everything with optimizations off
#define VEHICLE_GLASS_COMPRESSED_STACK_TRIANGLES  (__DEV && !__D3D11)

#endif // VEHICLEGLASS_H
