/////////////////////////////////////////////////////////////////////////////////
// Title	:	RefMacros.h
// Author	:	Adam Croston
// Started	:	11/06/08
//
// To hold the macors used by the reg refing system.
// This code used to live at the top of entity.h. It was necessary to isolate
// into it's own file so it could be used by RegdRefTypes.h without having the
// actually bring entity.h into scope.
/////////////////////////////////////////////////////////////////////////////////
#ifndef REF_MACROS_H_
#define REF_MACROS_H_


// REGREF is a macro that does the same as AddKnownRef. Before doing this it
// will also check the validity of the entity. (Only in debug mode)
// THESE MACROS ARE LEGACY CODE, RegdPtrs SHOULD BE USED AS A REPLACEMENT
#if __DEV
#define REGREF_DONTUSE( A, B ) { AssertEntityPointerValid( A ); A->AddKnownRef((void**)B ); }
#define TIDYREF_DONTUSE( A, B ) { AssertEntityPointerValid( A ); if(A->IsRefKnown((void**)B)){A->RemoveKnownRef((void**)B );} }
#else
#define REGREF_DONTUSE( A, B ) { A->AddKnownRef( (void**)B ); }
#define TIDYREF_DONTUSE( A, B ) { if(A->IsRefKnown((void**)B)){A->RemoveKnownRef((void**)B );} }
#endif // __DEV


#endif // REF_MACROS_H_
