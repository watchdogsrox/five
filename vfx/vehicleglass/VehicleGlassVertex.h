// ==============================================
// vfx/vehicleglass/VehicleGlassComponentVertex.h
// (c) 2012-2013 RockstarGames
// ==============================================

#ifndef VEHICLEGLASSVERTEX_H
#define VEHICLEGLASSVERTEX_H

// includes (rage)
#include "math/float16.h"
#include "vector/color32.h"

// includes (framework)

// includes (game)
#include "vfx/vehicleglass/VehicleGlass.h"


inline Vec3V_Out VehicleGlassVertex_GetVertexP(const void *vtx)
{
#	if !USE_EDGE // USE_GPU_VEHICLEDEFORMATION
		const Float16Vec4* ptrFloat16Vec4 = (Float16Vec4*)vtx;
#	else
		// Non-damaged position is stored at the end of the vertex data
		const Float16Vec4* ptrFloat16Vec4 = (Float16Vec4*)((uptr)vtx + 3*sizeof(Float16Vec4) + sizeof(u32));
#	endif
	return Float16Vec4Unpack(ptrFloat16Vec4).GetXYZ();
}

inline Vec3V_Out VehicleGlassVertex_GetDamagedVertexP(const void *vtx)
{
	const Float16Vec4* ptrFloat16Vec4 = (Float16Vec4*)vtx;
	return Float16Vec4Unpack(ptrFloat16Vec4).GetXYZ();
}

inline Vec3V_Out VehicleGlassVertex_GetVertexN(const void *vtx)
{
	const Float16Vec4* ptrFloat16Vec4 = ((Float16Vec4*)vtx) + 1;
	return Float16Vec4Unpack(ptrFloat16Vec4).GetXYZ();
}

inline void VehicleGlassVertex_GetVertexTNC(const void *vtx, Vec4V_InOut tex, Vec3V_InOut nrm, Vec4V_InOut col)
{
	const Float16Vec4* ptrFloat16Vec4 = ((Float16Vec4*)vtx) + 1;
	nrm = Float16Vec4Unpack(ptrFloat16Vec4++).GetXYZ();
	const u32 *ptrU32 = (u32*)((uptr)vtx + 2*sizeof(Float16Vec4));
	Color32 col32;
	col32.SetFromDeviceColor(*ptrU32);
	col = col32.GetRGBA();
	ptrFloat16Vec4 = (Float16Vec4*)((uptr)vtx + 2*sizeof(Float16Vec4) + sizeof(u32));
	tex = Float16Vec4Unpack(ptrFloat16Vec4++);
}


#endif // VEHICLEGLASSVERTEX_H
