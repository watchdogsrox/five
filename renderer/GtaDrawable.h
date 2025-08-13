//
// GtaDrawable - new class based on rmcDrawable to handle all gta-specific stuff relating to drawables.
//
//	28/07/2006	-	Andrzej:	- initial;
//
//
//
//
#ifndef __GTADRAWABLE_H__INCLUDED__
#define __GTADRAWABLE_H__INCLUDED__

#include "scene/2dEffect.h"
#if !__SPU
#include "rmcore/drawable.h"
#endif //!__SPU...
#include "phbound/bound.h"

//
//
//

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if __SPU
	class gtaDrawable;
#else
	class gtaDrawable : public rmcDrawable
	{
	public:
		gtaDrawable() {}
		gtaDrawable(datResource& rsc);

		~gtaDrawable() {}

		IMPLEMENT_PLACE_INLINE(gtaDrawable);

	#if !__FINAL
		void DeclareStruct(datTypeStruct &s);
	#endif // !__FINAL

		static gtaDrawable* LoadResource(const char *drawableName,const char *texdictName = NULL);

		//protected:
		atArray< CLightAttr >	m_lights;
		datOwner< atArray<u8> >	m_pTintData;
		datOwner< phBound >		m_pPhBound;
	};
#endif

#endif//__GTADRAWABLE_H__INCLUDED__...
