#ifndef _COMMANDS_GRAPHICS_H_
#define _COMMANDS_GRAPHICS_H_

#include "script_hud.h"

namespace graphics_commands
{
	void SetupScriptCommands();

	s32 LoadTxdForScript(const char *pFilename);

	grcTexture *GetTextureForScript(s32 UniqueIdForTextureDictionary, u32 HashKeyOfStreamedTextureDictionary, const char* pName);

	grcTexture *GetTextureFromStreamedTxd(const char* pStreamedTxdName, const char* pNameOfTexture);

	void DrawSpriteTex(grcTexture* pTextureToDraw, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, u32 movieIdx, EScriptGfxType gfxType, Vector2 vCoordU, Vector2 vCoordV, bool bRatioAdjust = false, bool bARAwareX = false, bool useNearest = false);

	void CommandDrawRect(float CentreX, float CentreY, float Width, float Height, int R, int G, int B, int A, bool Stereo = false);

	int CommandCreateTrackedPoint();
	void CommandSetTrackedPointInfo(int queryId, const scrVector & vPos, float radius);
	bool CommandIsTrackedPointVisible(int queryId);
	void CommandDestroyTrackedPointPosition(int queryId);

	void VerifyUsePtFxAssetHashName();
}

#endif
