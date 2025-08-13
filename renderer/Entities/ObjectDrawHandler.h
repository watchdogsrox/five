#ifndef _OBJECTDRAWHANDLER_H_INCLUDED_
#define _OBJECTDRAWHANDLER_H_INCLUDED_

#include "renderer/Entities/DynamicEntityDrawHandler.h"


class CObjectDrawHandler : public CDynamicEntityDrawHandler
{
public:
	CObjectDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CDynamicEntityDrawHandler(pEntity, pDrawable) {};
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

class CObjectFragmentDrawHandler : public CDynamicEntityFragmentDrawHandler
{
public:
	CObjectFragmentDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CDynamicEntityFragmentDrawHandler(pEntity, pDrawable) {};
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

#endif