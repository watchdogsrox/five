/*
 * basetypes.h
 * _____
 *
 * Contains basetypes that should be used throughout all RockstarNorth games
 *
 */
 
#ifndef INC_BASETYPES_H_
#define INC_BASETYPES_H_


#if __DEV && !__OPTIMIZED
#define __DEBUG		1
#else
#define __DEBUG		0
#endif


#if __WIN32
#pragma warning (disable : 4018)		// TEMPORARY HACK - signed/unsigned mismatch - hard to reconcile with PS3
#endif 

/*
 * type based defines
 */
#define MIN_INT8	(-128)
#define MAX_INT8	127
#define MIN_INT16	(-32768)
#define MAX_INT16	32767
#define MAX_INT32	2147483647
#define MIN_INT32	(-MAX_INT32-1)
#define MAX_UINT8	0xff
#define MAX_UINT16	0xffff
#define MAX_UINT32	0xffffffff

using namespace rage;

/* Boolean types (TRUE/FALSE) */
CompileTimeAssert(sizeof(bool)==1);

/*
 * Useful macros. All four macros assume nothing about type. The only macro 
 * to return a defined type is SIGN which returns an 's32' 
 */
#undef ABS
#undef MAX
#undef MIN
#undef SIGN

#define ABS(a)		(((a) < 0) ? (-(a)) : (a))
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#define SIGN(a)		(s32)(((a) < 0) ? (-1) : (1))

/*
 * Useful defines
 */
#undef TRUE
#undef FALSE
#define TRUE		1
#define FALSE		0


#if !__BANK
typedef const float BankFloat;
typedef const int BankInt32;
typedef const unsigned int BankUInt32;
typedef const bool BankBool;
#else // !__BANK
typedef float BankFloat;
typedef int BankInt32;
typedef unsigned int BankUInt32;
typedef bool BankBool;
#endif // !__BANK


#endif //INC_BASETYPES_H_
