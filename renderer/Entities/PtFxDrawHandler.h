#ifndef _PTFXDRAWHANDLER_H_INCLUDED_
#define _PTFXDRAWHANDLER_H_INCLUDED_

#include "renderer/Entities/EntityDrawHandler.h"

class CPtFxDrawHandler : public CEntityDrawHandler
{
public:
	CPtFxDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

#endif