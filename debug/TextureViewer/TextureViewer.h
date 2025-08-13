// ===================================
// debug/textureviewer/textureviewer.h
// (c) 2010 RockstarNorth
// ===================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWER_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWER_H_

#if __BANK

namespace rage { class grcTexture; }

class CBaseModelInfo;
class CEntity;
class CTxdRef;

class CDebugTextureViewerInterface
{
protected:
	CDebugTextureViewerInterface() {}
	~CDebugTextureViewerInterface() {}

public:
	static void Init    (unsigned int mode);
	static void Shutdown(unsigned int mode);

	static void InitWidgets();

	static bool IsEnabled();
	static bool IsEntitySelectRequired();

	static const grcTexture* GetReflectionOverrideTex();
	static const grcTexture* GetReflectionOverrideCubeTex();

	static void Update();
	static void Render();
	static void Synchronise();

	static void SelectModelInfo(u32 modelInfoIndex, const CEntity* entity, bool bActivate = false, bool bNoEmptyTxds = false, bool bImmediate = false);
	static void SelectTxd(const CTxdRef& ref, bool bActivate = false, bool bPreviewTxd = false, bool bMakeVisible = true, bool bImmediate = false);
	static void SelectPreviewTexture(const grcTexture* texture, const char* name, bool bActivate = false, bool bImmediate = false);
	static bool UpdateTextureForShaderEdit(const grcTexture* texture, const char* name, u32 modelIndex);
	static void Hide();
	static void Close(bool bRemoveRefsImmediately);

	// lame hack to make this feature externally accessible ..
	static void GetWorldPosUnderMouseAddRef();
	static void GetWorldPosUnderMouseRemoveRef();
	static void GetWorldPosUnderMouse(class rage::Vec4V* pos);

	static const CBaseModelInfo* GetSelectedModelInfo_renderthread();
	static u32                   GetSelectedShaderPtr_renderthread();
	static const char*           GetSelectedShaderName_renderthread();
	static const void*           GetSelectedTxd(); // case to const fwTxd*
};

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWER_H_
