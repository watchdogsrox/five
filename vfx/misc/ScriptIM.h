#ifndef SCRIPTIM_H
#define SCRIPTIM_H

#include "grcore/texture.h"
#include "vectormath/vec3v.h"
#include "vector/color32.h"


namespace ScriptIM
{	
	void 				Init(unsigned initMode);
	void 				Clear();
	void 				Render();

	void				Line(Vec3V_In start, Vec3V_In end, const Color32 color);
	void				Poly(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color);
	void				Poly(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color1, const Color32 color2, const Color32 color3);
	void				PolyTex(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color, grcTexture *tex, const Vector2& uv1, const Vector2& uv2, const Vector2& uv3);
	void				PolyTex(Vec3V_In P1, Vec3V_In P2, Vec3V_In P3, const Color32 color1, const Color32 color2, const Color32 color3, grcTexture *tex, const Vector2& uv1, const Vector2& uv2, const Vector2& uv3);
	void				BoxAxisAligned(Vec3V_In min, Vec3V_In max, const Color32 color);
	void				Sprite3D(Vec3V_In P1, Vec3V_In dimensions, const Color32 color, grcTexture *tex, const Vector2& uv0, const Vector2& uv1);

	void				SetBackFaceCulling(bool on);
	void				SetDepthWriting(bool on);
	
#if __BANK
	void				InitWidgets					();
#endif

};

#endif // SCRIPTIM_H
