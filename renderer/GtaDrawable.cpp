//
// GtaDrawable - new class based on rmcDrawable to handle
//				all gta-specific stuff relating to drawables.
//
//	28/07/2006	-	Andrzej:	- initial;
//
//
//
//
#include "renderer/GtaDrawable.h"


gtaDrawable::gtaDrawable(datResource& rsc)
: rmcDrawable(rsc),m_lights(rsc,true),m_pTintData(rsc),m_pPhBound(rsc)
{
}

#if __DECLARESTRUCT

void gtaDrawable::DeclareStruct(datTypeStruct &s)
{
	rmcDrawable::DeclareStruct(s);

	STRUCT_BEGIN(gtaDrawable);
		STRUCT_FIELD(m_lights);
		STRUCT_FIELD(m_pTintData);
		STRUCT_FIELD(m_pPhBound);
	STRUCT_END();
}

#endif // __DECLARESTRUCT

