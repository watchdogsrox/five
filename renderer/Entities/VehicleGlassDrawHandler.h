#ifndef _VEHICLEGLASSDRAWHANDLER_H_INCLUDED_
#define _VEHICLEGLASSDRAWHANDLER_H_INCLUDED_

#include "renderer/Entities/EntityDrawHandler.h"

class CVehicleGlassDrawHandler : public CEntityDrawHandler
{
public:
	CVehicleGlassDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

#endif