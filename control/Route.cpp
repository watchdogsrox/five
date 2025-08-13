#include "control/route.h"
#include "fwmaths/vector.h"

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPointRoute, 40, 0.66f, atHashString("PointRoute",0xffd1343d));
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPatrolRoute, 6, 0.75f, atHashString("PatrolRoute",0xd7e71231));

//PATROL ROUTE
#if !__FINAL
int CPatrolRoute::ms_iNumRoutes=0;
#endif

