#ifndef __SHADER_EDIT_H__
#define __SHADER_EDIT_H__

// rage
#include "atl/singleton.h"
#include "atl/string.h"
#include "grcore/effect.h"

#if __BANK

#define SHADEREDIT_VERBOSE (1 && __DEV)

#if SHADEREDIT_VERBOSE
	#define SHADEREDIT_VERBOSE_ONLY(x) x
#else
	#define SHADEREDIT_VERBOSE_ONLY(x)
#endif

namespace rage
{
	class bkBank;
	class grmShaderGroup;
	class rmcDrawable;
	class grcTexture;
	class bkGroup;
	class grcInstanceData;
};

class CEntity;
class CBaseModelInfo;

class CShaderEdit
{
public:
	enum textureSlot {
		TEX_NONE = -1,
		TEX_16,
		TEX_32,
		TEX_64,
		TEX_128,
		TEX_256,
		TEX_512,
		TEX_1024,
		TEX_2048,
		TEX_GLOBAL,
		TEX_COUNT,
	};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	void Update();
	void ApplyUpdates();

	void InitWidgets();
	void AddWidgets(bkBank* bk);
	void AddRendererWidgets();
	void PopulateWidgets(bkBank* bk);
		
	static void LoadGlobalTexture(CShaderEdit *instance);
	void SetGlobalTexture(grcTexture *tex, textureSlot slot);
	textureSlot GetTextureSlot(int width);
	static void Flush();
	static void ForceApply();

	// used by texture viewer shader edit integration
	static void ActiveTextureRefAdd   (grcTexture* texture);
	static void ActiveTextureRefRemove(grcTexture* texture);

	const atArray<grcTexChangeData>& GetChangeData() const;
	void FindChangeData(const char* shaderName, const char* channel, const char* previousTextureName, atArray<grcTexChangeData*>& dataArray);

#if __DEV
#if __XENON
	static void ApplyMaxTextureSize();
#endif // __XENON

	u32 GetMaxTexturesize() { return m_forceMaxTextureSize;}
#endif // __DEV

    bool IsTextureDebugOn() const { return m_bTextureDebug; }

protected:

    void UpdateHDVehicles(textureSlot slot, grcTexture* dbgTex);
	void UpdatePeds(strLocalIndex txdSlot, textureSlot slot, grcTexture* dbgTex);

	CShaderEdit();
	~CShaderEdit();

private:

	void RegenShaderWidgets(bkBank *bk);


	bkGroup *m_pShaderGroup;
	bkGroup *m_pLiveEditGroup;
	grcTexture *m_pPrevGlobalTex[TEX_COUNT];
	grcTexture *m_pGlobalTex[TEX_COUNT];
	textureSlot m_pUpdatedSlot;
	bool m_applyGlobal;
	bool m_bRestrictTextureSize;
	bool m_bAutoApply;
	bool m_bMatchPickerInteriorExteriorFlagsToPlayer;


	bool m_bPickerEnabled;
	rmcDrawable *m_pSelected;

	bool m_bTextureDebug;
	int m_textureDebugMode;

#if __DEV
	u32 m_forceMaxTextureSize;
	bool m_activeTextureRefsUpdate;
#endif // __DEV

	void UpdateTexture(rmcDrawable* drawable, textureSlot texSlot, grcTexture *tex);
    void UpdatePedTexture(rmcDrawable* drawable, grcTexture* tex);

	void OnLiveEditToggle();

	void SetInteriorExteriorFlagsForPicker();
	void TakeControlOfShaderEditPicker();

	void PickerToggle();

	CEntity *GetCurrentEntity();
};

typedef atSingleton<CShaderEdit> g_ShaderEdit;

#endif // __BANK

#endif