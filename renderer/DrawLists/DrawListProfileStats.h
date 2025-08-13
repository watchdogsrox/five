#ifndef _INC_DRAWLISTPROFILESTATS_H_
#define _INC_DRAWLISTPROFILESTATS_H_

#include "profile/element.h"

#define DRAWLIST_PROFILE_STATS 0

#if DRAWLIST_PROFILE_STATS

#define DL_PF_FUNC(p) PF_FUNC(p)

EXT_PF_TIMER( TotalAddToDrawList );
EXT_PF_TIMER( TotalBeforeAddToDrawList );
EXT_PF_TIMER( TotalAfterAddToDrawList );

EXT_PF_TIMER( EntityAddToDrawList );
EXT_PF_TIMER( FragAddToDrawList );
EXT_PF_TIMER( BendableAddToDrawList );
EXT_PF_TIMER( CutSceneActorAddToDrawList );
EXT_PF_TIMER( CutSceneVehicleAddToDrawList );
EXT_PF_TIMER( PedAddToDrawList );
EXT_PF_TIMER( VehicleAddToDrawList );
EXT_PF_TIMER( TotalDynamicEntityAddToDrawList );
EXT_PF_TIMER( TotalObjectAddToDrawList );

EXT_PF_TIMER( DynamicEntityAddToDrawList );
EXT_PF_TIMER( DynamicEntityBasicAddToDrawList );
EXT_PF_TIMER( DynamicEntityFragmentAddToDrawList );
EXT_PF_TIMER( DynamicEntitySkinnedAddToDrawList );

EXT_PF_TIMER( ObjectAddToDrawList );
EXT_PF_TIMER( ObjectFragmentAddToDrawList );

EXT_PF_TIMER( CopyOffMatrixSet );
EXT_PF_TIMER( CopyOffShader );

#else

#define DL_PF_FUNC(p)

#endif

#endif // !defined _INC_DRAWLISTPROFILESTATS_H_
