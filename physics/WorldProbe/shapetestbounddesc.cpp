#include "physics/WorldProbe/shapetestbounddesc.h"

#include "scene/entity.h"

void WorldProbe::CShapeTestBoundDesc::SetBoundFromEntity(const CEntity* pEntity)
{
	if(physicsVerifyf(pEntity, "Attempting to set bound from a NULL CEntity pointer."))
	{
		phInst* pInst = pEntity->GetFragInst();
		if(!pInst)
		{
			pInst = pEntity->GetCurrentPhysicsInst();
		}

		if(pInst)
		{
			m_pBound = pInst->GetArchetype()->GetBound();
			physicsAssertf(m_pBound, "phBound set from CEntity is NULL.");
		}
		else
		{
			// It's ok not to have physics instance.
			m_pBound = NULL;
		}
	}
}
