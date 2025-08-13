#ifndef _DYNAMICENTITYDRAWHANDLER_H_INCLUDED_
#define _DYNAMICENTITYDRAWHANDLER_H_INCLUDED_

#include "renderer/Entities/EntityDrawHandler.h"

class CDynamicEntityDrawHandler : public CEntityDrawHandler
{
public:
	CDynamicEntityDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);

	static dlCmdBase*		AddFragmentToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pEntity, fwDrawDataAddParams* pParams);
	static dlCmdBase*		AddSkinnedToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

class CDynamicEntityFragmentDrawHandler : public CEntityDrawHandler
{
public:
	CDynamicEntityFragmentDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams) { CDrawListPrototypeManager::Flush(); return CDynamicEntityDrawHandler::AddFragmentToDrawList(this, pEntity, pParams);}
};

class CDynamicEntitySkinnedDrawHandler : public CEntityDrawHandler
{
public:
	CDynamicEntitySkinnedDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams) { CDrawListPrototypeManager::Flush(); return CDynamicEntityDrawHandler::AddSkinnedToDrawList(this, pEntity, pParams); }
};

#endif