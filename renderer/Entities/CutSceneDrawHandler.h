#ifndef _CUTSCENEDRAWHANDLER_H_INCLUDED_
#define _CUTSCENEDRAWHANDLER_H_INCLUDED_

#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/Entities/DynamicEntityDrawHandler.h"

class CCutSceneVehicleDrawHandler : public CDynamicEntityDrawHandler
{
public:
	CCutSceneVehicleDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CDynamicEntityDrawHandler(pEntity, pDrawable) {};
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

#endif