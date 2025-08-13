/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiEntrypointId.h
// PURPOSE : Represents an ID of a entrypoint in the page deck system. 
//           Currently an extension of atHashString just to avoid bad type munging
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_ENTRYPOINT_ID_H
#define UI_ENTRYPOINT_ID_H

// rage
#include "atl/hashstring.h"
#include "atl/map.h"

class uiEntrypointId : protected rage::atHashString
{
    typedef rage::atHashString superclass;
public:
    uiEntrypointId() : superclass() {}
    explicit uiEntrypointId( u32 const hashCode ) : superclass( hashCode ) {} 

    using superclass::GetHash;
    using superclass::TryGetCStr;
    using superclass::IsNull;
    using superclass::IsNotNull;

    bool operator ==( const class uiEntrypointId& op ) const { return GetHash() == op.GetHash(); }
};

template <>
struct atMapHashFn<uiEntrypointId>
{
    unsigned operator ()( const uiEntrypointId& key ) const { return key.GetHash(); }
};


#endif // UI_ENTRYPOINT_ID_H
