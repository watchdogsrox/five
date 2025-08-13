#ifndef _PEDDRAWHANDLER_H_INCLUDED_
#define _PEDDRAWHANDLER_H_INCLUDED_

#include "peds/rendering/PedProps.h"
#include "peds/rendering/PedVariationDS.h"
#include "renderer/Entities/EntityDrawHandler.h"

class CPedStreamRenderGfx;
class CPedPropRenderGfx;

class CPedDrawHandler : public CEntityDrawHandler
{
public:
	CPedDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) :
	  CEntityDrawHandler(pEntity, pDrawable), m_pPedRenderGfx(NULL), m_pPedPropRenderGfx(NULL), m_perComponentWetnessFlags(0) {};

	virtual ~CPedDrawHandler();
	virtual dlCmdBase* AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);

	const CPedStreamRenderGfx*	GetConstPedRenderGfx(void) const				{ return(m_pPedRenderGfx); }
	CPedStreamRenderGfx*		GetPedRenderGfx(void)							{ return(m_pPedRenderGfx); }
	void						SetPedRenderGfx(CPedStreamRenderGfx* pGfxData);

	const CPedPropRenderGfx*	GetConstPedPropRenderGfx(void) const				{ return(m_pPedPropRenderGfx); }
	CPedPropRenderGfx*			GetPedPropRenderGfx(void)							{ return(m_pPedPropRenderGfx); }
	void						SetPedPropRenderGfx(CPedPropRenderGfx* pGfxData);

	CPedVariationData&			GetVarData					()		 { return m_varData; }
	const CPedVariationData&	GetVarData					() const { return m_varData; }
	CPedPropData&				GetPropData					()		 { return m_propData; }
	const CPedPropData&			GetPropData					() const { return m_propData; }

	void						UpdateCachedWetnessFlags	(CPed* pPed);
	u32							GetPerComponentWetnessFlags	() const { return m_perComponentWetnessFlags; }

#if __BANK
	static void SetEnableRendering(bool bEnable);
#endif // __BANK

protected:
	CPedStreamRenderGfx*		m_pPedRenderGfx;
	CPedPropRenderGfx*			m_pPedPropRenderGfx;
	CPedVariationData			m_varData;
	CPedPropData				m_propData;
	u32							m_perComponentWetnessFlags;
};

#endif