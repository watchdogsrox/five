#ifndef GAMEGLOWS_H
#define GAMEGLOWS_H

#include "vectormath/vec3v.h"
#include "vector/color32.h"

namespace GameGlows
{	
	void 				Init(unsigned initMode);
	void 				Render();
	void 				Add	(Vec3V_In vPos, const float size, const Color32 col, const float intensity);
	bool				AddFullScreenGameGlow(Vec3V_In vPos, const float size, const Color32 col, const float intensity, const bool inverted, const bool marker, const bool fromScript);

#if __BANK
	void				InitWidgets					();
#endif

};

#endif // GAMEGLOWS_H
