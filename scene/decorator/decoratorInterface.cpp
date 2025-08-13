/////////////////////////////////////////////////////////////////////////////////
// FILE :		decoratorInterface.cpp
// PURPOSE :	Game level Interface for debug drawing and rag so it can be easily separated from core code
//               also includes a registry for all decorator names so we can enforce type usage
// AUTHOR :		Jason Jurecka.
// CREATED :	29/6/2011
/////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// Game includes
#include "scene/volume/Volume.h"
#include "scene/decorator/decoratorInterface.h"

DECORATOR_OPTIMISATIONS();

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////
#if DECORATOR_DEBUG & VOLUME_SUPPORT
bool CDecoratorInterface::GetOffset (fwExtensibleBase* pBase, Vec3V_InOut position)
{   
	bool typefound = false;

	fwEntity* entity = dynamic_cast<fwEntity*>(pBase);
	if (entity)
	{
		const fwTransform &trans = entity->GetTransform();
		position = trans.GetPosition();
		typefound = true;
	}
	else
	{
		CVolume* Volume = dynamic_cast<CVolume*>(pBase);
		if (Volume)
		{
			Volume->GetPosition(position);
			typefound = true;
		}
	}

    return typefound;
}
#endif // DECORATOR_DEBUG & VOLUME_SUPPORT