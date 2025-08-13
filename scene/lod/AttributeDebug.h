#ifndef	__ATTRIBUTE_DEBUG_H
#define	__ATTRIBUTE_DEBUG_H

#if __BANK
#include "scene/entity.h"


typedef	struct _TweakerAttributeContainer
{
	bool	bDontCastShadows;
	bool	bDontRenderInShadows;
	bool	bDontRenderInReflections;
	bool	bOnlyRenderInReflections;
	bool	bDontRenderInWaterReflections;
	bool	bOnlyRenderInWaterReflections;
	bool	bDontRenderInMirrorReflections;
	bool	bOnlyRenderInMirrorReflections;

	bool	HaveAttribsChanged(_TweakerAttributeContainer &rhs)
	{
		return (	bDontRenderInShadows != rhs.bDontRenderInShadows ||
					bDontRenderInReflections != rhs.bDontRenderInReflections ||
					bOnlyRenderInReflections != rhs.bOnlyRenderInReflections ||
					bDontRenderInWaterReflections != rhs.bDontRenderInWaterReflections ||
					bOnlyRenderInWaterReflections != rhs.bOnlyRenderInWaterReflections ||
					bDontRenderInMirrorReflections != rhs.bDontRenderInMirrorReflections ||
					bOnlyRenderInMirrorReflections != rhs.bOnlyRenderInMirrorReflections );
	}

}	TweakerAttributeContainer;

class	CAttributeTweaker
{
public:
	static void AddWidgets(bkBank* pBank);
	static void Update();

private:

	static bool GetDontCastShadows( CEntity *pEntity );
	static void SetEntityShadowFlags(CEntity *pEntity, bool entityShadowFlag);
	static void SetDontCastShadows( CEntity *pEntity, bool val );

	static bool GuessDontRenderInShadows( CEntity *pEntity );
	static bool GetDontRenderInShadows( CEntity *pEntity );
	static void SetDontRenderInShadows( CEntity *pEntity, bool val );

	static void	GetShadowAttribsFromEntity(CEntity *pEntity);
	static void	GetReflectionAttribsFromEntity(CEntity *pEntity);

	static void	SetReflectionAttribsInEntity(CEntity *pEntity);

	static void UpdateRestAttributeList(CEntity* pEntity, TweakerAttributeContainer &tweakValues, bool bStreamingPriorityLow, int priority);

	static bool	bTweakAttributes;

	static bool	bDontRenderInShadowsOld;

	static TweakerAttributeContainer	m_Attributes;
	static TweakerAttributeContainer	m_OldAttributes;

	static bool bStreamingPriorityLow;
	static int	iPriorityLevel;
	static CEntity *pLastSelectedEntity;
};

#endif	//__BANK
#endif	//__ATTRIBUTE_DEBUG_H
