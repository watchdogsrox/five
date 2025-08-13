#ifndef _BLOCKVIEW_H_
#define _BLOCKVIEW_H_

#if !__FINAL

#include "vector/vector3.h"
#include "atl/string.h"
#include "fwmaths/rect.h"

#define BLOCKFLAGS_FIRST_PASS				(1<<0)
#define BLOCKFLAGS_SECOND_PASS_STARTED		(1<<1)
#define BLOCKFLAGS_SECOND_PASS_OUTSOURCED	(1<<2)
#define BLOCKFLAGS_SECOND_PASS_COMPLETE		(1<<3)
#define BLOCKFLAGS_THIRD_PASS_COMPLETE		(1<<4)
#define BLOCKFLAGS_COMPLETE					(1<<5)
#define BLOCKFLAGS_UNUSED					(1<<6)

class CEntity;

class CBlockView
{
public:
	static void				Init(unsigned initMode);
	static void				Shutdown(unsigned shutdownMode);

#if __BANK
	static void				InitLevelWidgets();
	static void				ShutdownLevelWidgets();
#endif

	static void				Update();
	static void				UpdateBlockList();
	static void				UpdateCurrentBlock();

	static void				AddLodTree(CEntity* pRootEntity);
	static void				AddBlock(const char* pName,const char* pExportedBy,const char* pOwner,const char* pTime,s32 flags,s32 numPoints = 0,const Vector3* points = NULL);
	static s32			GetNumberOfBlocks();
	static const char*	GetBlockName(s32 index);
	static const char*	GetBlockOwner(s32 index);
	static const char*	GetBlockUser(s32 index);
	static const char*	GetTime(s32 index);
	static s32			GetBlockFlags(s32 index);
	static void				GetBlockPos(s32 index,Vector3& r_vecGet);
	static s32			GetNumPoints(s32 index);
	static Vector3			GetPoint(s32 index,s32 point);
	static s32			GetCurrentBlockSelected();
	static s32			GetCurrentBlockInside();
	static s32			GetCurrentBlockInside(const Vector3& r_Pos);

	static void				SetDisplayInfo(bool bDisplayInfo);

	static void				SelectBlock();
	static void				SelectBlockByName();
	static void				SelectUser();
	static void				SelectNext();
	static void				SelectPrevious();
	static void				WarpToCurrent();

	static bool				ShouldDisplayInfo();
	static bool				ShouldRenderToMap(s32 flag);
	static bool				FilterArtist();
	static bool				IsCurrentUser(s32 index);
	static bool				FilterBlock();
	static bool				FilterLodTree();
	static bool				IsCurrentBlock(s32 index);
	static bool				ShouldDraw(CEntity* pEntity);


	
};

#endif //#if !__FINAL

#endif //_BLOCKVIEW_H_
