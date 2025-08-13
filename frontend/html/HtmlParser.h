//
// filename:	HtmlParser.h
// description:	
//

#ifndef INC_HTMLPARSER_H_
#define INC_HTMLPARSER_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "parsercore/streamxml.h"
// Game headers


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

//
// name:		CHtmlParser
// description:	Class that overrides parStreamInXml to make it a bit more lax about 
//				reading html markup instead of xml markup
class CHtmlParser : public parStreamInXml
{
public:
	CHtmlParser(fiStream* s) : parStreamInXml(s) { SetDataChunkSize(4096);}

	enum {
		ESC_LT = '<' + 256,
		ESC_GT = '>' + 256,
		ESC_AMP = '&' + 256,
		ESC_APOS = '\'' + 256,
		ESC_QUOT = '"' + 256,
		ESC_SPACE = ' ' + 256,
		ESC_RSQUO = '\'' + 256,
		ESC_COPYRIGHT = '©' + 256,
		ESC_UNKNOWN = '?' + 256
	};
	virtual void ReadWithCallbacks();
	bool ReadElementInternal(parElement& outElement);
	s32 ReadDataInternal(char* str, int size, DataEncoding enc);
	s32 ReadEscapedCharacter();
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLPARSER_H_
