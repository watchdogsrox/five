#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"

#include "DrawListProfileStats.h"

#if DRAWLIST_PROFILE_STATS

PF_PAGE( Drawlists, "Drawlists" );

PF_GROUP( DrawHandlers_AddBeforeAfter );
PF_GROUP( DrawHandlers_TypeSpecificAdd );
PF_GROUP( DrawHandlers_DynamicDrawHandlers );
PF_GROUP( DrawHandlers_ObjectDrawHandlers );
PF_GROUP( DrawHandlers_Other );

PF_LINK( Drawlists, DrawHandlers_AddBeforeAfter );
PF_LINK( Drawlists, DrawHandlers_TypeSpecificAdd );
PF_LINK( Drawlists, DrawHandlers_DynamicDrawHandlers );
PF_LINK( Drawlists, DrawHandlers_ObjectDrawHandlers );
PF_LINK( Drawlists, DrawHandlers_Other );

PF_TIMER( TotalAddToDrawList,					DrawHandlers_AddBeforeAfter );
PF_TIMER( TotalBeforeAddToDrawList,				DrawHandlers_AddBeforeAfter );
PF_TIMER( TotalAfterAddToDrawList,				DrawHandlers_AddBeforeAfter );

PF_TIMER( EntityAddToDrawList,					DrawHandlers_TypeSpecificAdd );
PF_TIMER( FragAddToDrawList,					DrawHandlers_TypeSpecificAdd );
PF_TIMER( BendableAddToDrawList,				DrawHandlers_TypeSpecificAdd );
PF_TIMER( CutSceneActorAddToDrawList,			DrawHandlers_TypeSpecificAdd );
PF_TIMER( CutSceneVehicleAddToDrawList,			DrawHandlers_TypeSpecificAdd );
PF_TIMER( PedAddToDrawList,						DrawHandlers_TypeSpecificAdd );
PF_TIMER( VehicleAddToDrawList,					DrawHandlers_TypeSpecificAdd );
PF_TIMER( TotalDynamicEntityAddToDrawList,		DrawHandlers_TypeSpecificAdd );
PF_TIMER( TotalObjectAddToDrawList,				DrawHandlers_TypeSpecificAdd );

PF_TIMER( DynamicEntityAddToDrawList,			DrawHandlers_DynamicDrawHandlers );
PF_TIMER( DynamicEntityBasicAddToDrawList,		DrawHandlers_DynamicDrawHandlers );
PF_TIMER( DynamicEntityFragmentAddToDrawList,	DrawHandlers_DynamicDrawHandlers );
PF_TIMER( DynamicEntitySkinnedAddToDrawList,	DrawHandlers_DynamicDrawHandlers );

PF_TIMER( ObjectAddToDrawList,					DrawHandlers_ObjectDrawHandlers );
PF_TIMER( ObjectFragmentAddToDrawList,			DrawHandlers_ObjectDrawHandlers );

PF_TIMER( CopyOffMatrixSet,						DrawHandlers_Other );
PF_TIMER( CopyOffShader,						DrawHandlers_Other );

#endif