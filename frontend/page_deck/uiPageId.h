/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiPageId.h
// PURPOSE : Represents an ID of a page in the page deck system. 
//           Currently an extension of atHashString just to avoid bad type munging
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_PAGE_ID_H
#define UI_PAGE_ID_H

// rage
#include "atl/hashstring.h"
#include "atl/map.h"

class uiPageId : protected rage::atHashString
{
    typedef rage::atHashString superclass;
public:
    uiPageId() : superclass() {}
    explicit uiPageId( u32 const hashCode ) : superclass( hashCode ) {} 

    static uiPageId Null()
    {
        return uiPageId();
    }

    atHashString GetAsHashString() const 
    {
        return atHashString( GetHash() );
    }

    using superclass::GetHash;
    using superclass::TryGetCStr;
    using superclass::IsNull;
    using superclass::IsNotNull;

    bool operator ==( const class uiPageId& op ) const { return GetHash() == op.GetHash(); }
};

template <>
struct atMapHashFn<uiPageId>
{
    unsigned operator ()( const uiPageId& key ) const { return key.GetHash(); }
};

#endif // UI_PAGE_ID_H
