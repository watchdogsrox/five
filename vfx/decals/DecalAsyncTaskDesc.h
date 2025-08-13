///////////////////////////////////////////////////////////////////////////////
//
//  FILE:   DecalAsyncTaskDesc.h
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DECAL_DECALASYNCTASKDESC_H
#define DECAL_DECALASYNCTASKDESC_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage

// framework
#include "vfx/decal/decalasynctaskdescbase.h"

// game


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

namespace rage
{
	class fwArchetype;
}

class phMaterialGta;


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4201)  // nonstandard extension used : nameless struct/union
#endif

struct CDecalAsyncTaskDesc : public decalAsyncTaskDescBase
{
#if !VFX_FW_DECAL_OPTIMISATIONS_OFF
	union
	{
#endif

		// DECAL_DO_PROCESSBOUND / DECAL_DO_PROCESSBREAKABLEGLASS
		// DECAL_DO_PROCESSDRAWABLESNONSKINNED / DECAL_DO_PROCESSDRAWABLESSKINNED
		struct
		{
			const phMaterialGta*    materialArray;
#			if __ASSERT
				const fwArchetype*      debugFwArchetype;
#			endif
		};

		// DECAL_DO_PROCESSSMASHGROUP
		struct
		{
			const void*             smashgroupVerts;
			u16                     smashgroupNumVerts;
			u16                     smashgroupStride;
		};

#if !VFX_FW_DECAL_OPTIMISATIONS_OFF
	};
#endif
};

#ifdef _MSC_VER
#	pragma warning(pop)
#endif


#endif // DECAL_DECALASYNCTASKDESC_H
