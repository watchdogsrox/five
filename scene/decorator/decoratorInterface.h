/////////////////////////////////////////////////////////////////////////////////
// FILE :		decoratorInterface.h
// PURPOSE :	Game level Interface for debug drawing and rag so it can be easily separated from core code
//               also includes a registry for all decorator names so we can enforce type usage
// AUTHOR :		Jason Jurecka.
// CREATED :	29/6/2011
/////////////////////////////////////////////////////////////////////////////////
#ifndef DECORATOR_INTERFACE_H_
#define DECORATOR_INTERFACE_H_

#include "fwdecorator/decoratorInterface.h"

class CDecoratorInterface : public fwDecoratorInterface
{
public:
            CDecoratorInterface  () : fwDecoratorInterface() {}
    virtual	~CDecoratorInterface (){}

    //////////////////////////////////////////////////////////////////////////
    // fwDecoratorInterface
#if DECORATOR_DEBUG & VOLUME_SUPPORT
    bool GetOffset (fwExtensibleBase* pBase, Vec3V_InOut position);
#endif // DECORATOR_DEBUG & VOLUME_SUPPORT
    //////////////////////////////////////////////////////////////////////////
};

#endif // DECORATOR_INTERFACE_DEBUG_H_
