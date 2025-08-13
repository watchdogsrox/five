//
// Filename:	CustomShaderEffectInterior.cpp
// Description:	Class for controlling DLC interior shaders variables
// Written by:	Andrzej
//
//	30/10/2013 -	Andrzej:	- initial;
//
//
//
//

// Rage headers
#include "grmodel\ShaderFx.h"
#include "grmodel\Geometry.h"
#include "grcore\texturereference.h"

#include "creature/creature.h"
#include "creature/componentshadervars.h"
#include "rmcore/drawable.h"

// Game headers
#include "debug\debug.h"
#include "peds\Ped.h"
#include "scene\Entity.h"
#include "CustomShaderEffectInterior.h"


#if CSE_INTERIOR_EDITABLEVALUES
	bool	CCustomShaderEffectInterior::ms_bEVEnabled = false;
#endif

//
//
//
//
static inline u8 InteriorShaderGroupLookupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectInteriorType::Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bTint)
{
	Assert(pDrawable);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();

	// create cascaded tint type:
	if(bTint)
	{
		m_pTintType = CCustomShaderEffectTintType::Create(pDrawable, pMI);
		if(!m_pTintType->Initialise(pDrawable))
		{
			m_pTintType->RemoveRef();
			m_pTintType = NULL;
			Assertf(0, "Error initialising CustomShaderEffectTint!");
			return(FALSE);
		}
	}
	else
	{
		m_pTintType = NULL;
	}

	m_idVarDiffuseTex = InteriorShaderGroupLookupVar(pShaderGroup, "DiffuseTex", TRUE);

	return(TRUE);
}// end of CCustomShaderEffectInterior::Initialise()...


//
//
//
//
CCustomShaderEffectInteriorType::~CCustomShaderEffectInteriorType()
{
	if(m_pTintType)
	{
		m_pTintType->RemoveRef();
		m_pTintType = NULL;
	}
}

//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectInteriorType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectInterior *pNewShaderEffect = rage_new CCustomShaderEffectInterior;

	if(this->m_pTintType)
	{
		pNewShaderEffect->m_pTint = (CCustomShaderEffectTint*)( m_pTintType->CreateInstance(pEntity) );	// cascaded CSE tint instance
	}
	else
	{
		pNewShaderEffect->m_pTint = NULL;
	}

	pNewShaderEffect->m_pType = this;

	return(pNewShaderEffect);
}

//
//
//
//
bool CCustomShaderEffectInteriorType::RestoreModelInfoDrawable(rmcDrawable *pDrawable)
{
	if(this->m_pTintType)
	{
		return m_pTintType->RestoreModelInfoDrawable(pDrawable);
	}
	else
	{
		return(true);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////

//
//
//
//
CCustomShaderEffectInterior::CCustomShaderEffectInterior()	: CCustomShaderEffectBase(sizeof(*this), CSE_INTERIOR)
{
	m_varhDiffuseTex = 0;
}

//
//
//
//
CCustomShaderEffectInterior::~CCustomShaderEffectInterior()
{
	if(m_pTint)
	{
		delete m_pTint;
		m_pTint = NULL;
	}
}


void CCustomShaderEffectInterior::AddToDrawList(u32 modelIndex, bool bExecute)
{
	CCustomShaderEffectTint *tmpTint = m_pTint; // save cascaded tint ptr
	m_pTint = NULL;		// remove cascaded link for DLC
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
	m_pTint = tmpTint;	// restore ptr

	if(m_pTint)
	{
		m_pTint->AddToDrawList(modelIndex, bExecute);
	}
}

void CCustomShaderEffectInterior::AddToDrawListAfterDraw()
{
	if(m_pTint)
	{
		m_pTint->AddToDrawListAfterDraw();
	}
}



//
//
//
//
void CCustomShaderEffectInterior::Update(fwEntity* pEntityBase)
{
	CEntity* pEntity = static_cast<CEntity*>(pEntityBase);
	Assert (pEntity);

	if(m_pTint)
	{
		m_pTint->Update(pEntity);
	}

}// end of Update()...


//
//
// 
//
void CCustomShaderEffectInterior::SetShaderVariables(rmcDrawable* pDrawable)
{
	if(!pDrawable)
		return;

	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
		return;
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();
	Assert(shaderGroup.GetShaderGroupVarCount() > 0);

	CCustomShaderEffectInteriorType *pType = m_pType;
	Assert(pType);

	if(m_varhDiffuseTex)
	{
		shaderGroup.SetVar((grmShaderGroupVar)pType->m_idVarDiffuseTex, m_varhDiffuseTex);
	}

#if CSE_INTERIOR_EDITABLEVALUES
	CCustomShaderEffectInterior::SetEditableShaderValues(&shaderGroup, pDrawable);
#endif

}// end of SetShaderVariables()...


//
//
//
//
u32 CCustomShaderEffectInterior::GetNumTintPalettes()
{
	if(m_pTint)
	{
		return m_pTint->GetNumTintPalettes();
	}

	return(0);
}


#if CSE_INTERIOR_EDITABLEVALUES
//
//
//
//
void CCustomShaderEffectInterior::SetEditableShaderValues(grmShaderGroup *UNUSED_PARAM(pShaderGroup), rmcDrawable* UNUSED_PARAM(pDrawable))
{
//	Assert(pShaderGroup);

	if(!ms_bEVEnabled)
		return;

	// TODO

}// end of SetEditableShaderValues()...


//
//
//
//
bool CCustomShaderEffectInterior::InitWidgets(bkBank& bank)
{
	// debug widgets:
	bank.PushGroup("Interior Editable Shaders Values", false);
		bank.AddToggle("Enable",	&ms_bEVEnabled);
		// TODO
	bank.PopGroup();

	return(TRUE);
}// end of InitWidgets()...

#endif //CSE_INTERIOR_EDITABLEVALUES...


