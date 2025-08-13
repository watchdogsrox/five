/*
 * basetypes.h
 * _____
 *
 * Contains basetypes that should be used throughout all RockstarNorth games
 *
 */

#ifndef INC_BASETYPES_H_
#define INC_BASETYPES_H_


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


/* integer types */
typedef signed char 	s8;
typedef signed char		s8;
typedef signed short	s16;    
typedef signed short	s16;
typedef unsigned short  char16;
typedef unsigned short  char16;
typedef signed int		s32;
typedef signed int		s32;
#if defined(GTA_PC)
typedef signed __int64	s64;
typedef signed __int64	s64;
#elif defined (GTA_XBOX)
typedef signed __int64	s64;
typedef signed __int64	s64;
#else
#error No 64bit integer defined
#endif

/* unsigned integer types */
typedef unsigned char 	u8;
typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef unsigned int	u32;
#if defined(GTA_PC)
typedef unsigned __int64	u64;
typedef unsigned __int64	u64;
#elif defined(GTA_XBOX)
typedef unsigned __int64	u64;
typedef unsigned __int64	u64;
#else
#error No 64bit integer defined
#endif

/* Boolean types (TRUE/FALSE) */
typedef unsigned short 	Bool16;
typedef unsigned short 	bool16;
typedef unsigned int 	Bool32;
typedef unsigned int 	bool32;

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



#endif