/////////////////////////////////////////////////////////////////////////////////
// Title	:	Scene.h
// Author(s):	Adam Fowler
//
// description:	class containing all scene information
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_SCENE_H_
#define INC_SCENE_H_

// rage includes
#include "fwscene/scene.h"		// For fwSceneInterface.
#include "atl/hashstring.h"

// game includes
#include "control/leveldata.h"

namespace rage {
	class bkBank;
	class grcViewport;
}

class CRenderPhaseScanned;

class CScene
{
public:
	static void		Init					(unsigned initMode);
	static void		Shutdown				(unsigned shutdownMode);

	static void		LoadMap					(const char* pName);
	static void		LoadMap					(s32 levelNumber);	// level number starts at 1

	static atHashString GetCurrentLevelNameHash();

	static void		Update					();
	static void		SetupRender				(s32 list, u32 renderFlags, const grcViewport &viewport, CRenderPhaseScanned *renderPhaseNew);

	static CLevelData& GetLevelsData() { return m_levelData; }

#if __BANK
	static void		InitWidgets();
	static void		AddWidgets();
	static void		ShutdownWidgets();

	static bkBank*		ms_pBank;
#endif //__BANK

	static void TidyReferences();

	static const char* GetLevelName			(s32 levelNumber);

private:
	static CLevelData m_levelData;
	static int m_currentLevelIndex;
};


class CGtaSceneInterface : public fwSceneInterface
{
public:
	void InitPools();
	void ShutdownPools();
};

#endif // INC_SCENE_H_
