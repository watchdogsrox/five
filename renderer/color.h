//
// Title	:	Color.h
// Author	:	Adam Fowler
// Started	:	22/02/05

#ifndef INC_COLOR_H
#define INC_COLOR_H

#include "vector/color32.h"

#define SET_CRGBA(r,g,b,a) \
( \
	((b)<<(0*8)) | \
	((g)<<(1*8)) | \
	((r)<<(2*8)) | \
	((a)<<(3*8)) \
)

class CRGBA : public Color32 // useful for constant colors - does not clamp the input values
{
public:
	__forceinline CRGBA() {}
	__forceinline CRGBA(u8 r, u8 g, u8 b, u8 a = 255)
	{
		SetRed(r);
		SetGreen(g);
		SetBlue(b);
		SetAlpha(a);
	}
	__forceinline CRGBA(const Color32 & col) : Color32(col.GetColor()) {}
	explicit __forceinline CRGBA(u32 bgra) : Color32(bgra) // platform-dependent!!
	{
	}
};

// CSS2.1 colors ... http://www.j-a-b.net/web/hue/color-names
class CRGBA_Black       : public CRGBA { public: __forceinline CRGBA_Black      (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0x00,0x00,a)) {} };
class CRGBA_White       : public CRGBA { public: __forceinline CRGBA_White      (u8 a = 255) : CRGBA(SET_CRGBA(0xFF,0xFF,0xFF,a)) {} };
class CRGBA_Gray        : public CRGBA { public: __forceinline CRGBA_Gray       (u8 a = 255) : CRGBA(SET_CRGBA(0x80,0x80,0x80,a)) {} };
class CRGBA_LightGray   : public CRGBA { public: __forceinline CRGBA_LightGray  (u8 a = 255) : CRGBA(SET_CRGBA(0xC0,0xC0,0xC0,a)) {} }; // CSS2.1 "Silver"
class CRGBA_Red         : public CRGBA { public: __forceinline CRGBA_Red        (u8 a = 255) : CRGBA(SET_CRGBA(0xFF,0x00,0x00,a)) {} };
class CRGBA_Green       : public CRGBA { public: __forceinline CRGBA_Green      (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0xFF,0x00,a)) {} }; // CSS2.1 "Lime"
class CRGBA_Blue        : public CRGBA { public: __forceinline CRGBA_Blue       (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0x00,0xFF,a)) {} };
class CRGBA_Cyan        : public CRGBA { public: __forceinline CRGBA_Cyan       (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0xFF,0xFF,a)) {} }; // CSS2.1 "Aqua"
class CRGBA_Magenta     : public CRGBA { public: __forceinline CRGBA_Magenta    (u8 a = 255) : CRGBA(SET_CRGBA(0xFF,0x00,0xFF,a)) {} }; // CSS2.1 "Fuchsia"
class CRGBA_Yellow      : public CRGBA { public: __forceinline CRGBA_Yellow     (u8 a = 255) : CRGBA(SET_CRGBA(0xFF,0xFF,0x00,a)) {} };
class CRGBA_Orange      : public CRGBA { public: __forceinline CRGBA_Orange     (u8 a = 255) : CRGBA(SET_CRGBA(0xFF,0xA5,0x00,a)) {} };
class CRGBA_DarkRed     : public CRGBA { public: __forceinline CRGBA_DarkRed    (u8 a = 255) : CRGBA(SET_CRGBA(0x80,0x00,0x00,a)) {} }; // CSS2.1 "Maroon"
class CRGBA_DarkGreen   : public CRGBA { public: __forceinline CRGBA_DarkGreen  (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0x80,0x00,a)) {} }; // CSS2.1 "Green"
class CRGBA_DarkBlue    : public CRGBA { public: __forceinline CRGBA_DarkBlue   (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0x00,0x80,a)) {} }; // CSS2.1 "Navy"
class CRGBA_DarkCyan    : public CRGBA { public: __forceinline CRGBA_DarkCyan   (u8 a = 255) : CRGBA(SET_CRGBA(0x00,0x80,0x80,a)) {} }; // CSS2.1 "Teal"
class CRGBA_DarkMagenta : public CRGBA { public: __forceinline CRGBA_DarkMagenta(u8 a = 255) : CRGBA(SET_CRGBA(0x80,0x00,0x80,a)) {} }; // CSS2.1 "Purple"
class CRGBA_DarkYellow  : public CRGBA { public: __forceinline CRGBA_DarkYellow (u8 a = 255) : CRGBA(SET_CRGBA(0x80,0x80,0x00,a)) {} }; // CSS2.1 "Olive"
class CRGBA_DarkGray    : public CRGBA { public: __forceinline CRGBA_DarkGray   (u8 a = 255) : CRGBA(SET_CRGBA(0x66,0x66,0x66,a)) {} };

#endif // INC_COLOR_H
