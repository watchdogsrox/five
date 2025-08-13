// ========================================
// debug/textureviewer/textureviewerentry.h
// (c) 2010 RockstarNorth
// ========================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERENTRY_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERENTRY_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

#include "atl/string.h"                              // atString
#include "paging/ref.h"                              // pgRef
#include "debug/textureviewer/textureviewersearch.h" // CAssetRef,CTxdRef,CTextureRef
#include "scene/RegdRefTypes.h"                      // RegdEnt

namespace rage { template <typename,int,typename> class atArray; }
namespace rage { class grcTexture; }
class CBaseModelInfo;
class CEntity;
class CDebugTextureViewerWindow;

// ================================================================================================

class CDTVColumnFlags
{
public:
	enum eColumnID
	{
		CID_LineNumber     = 0, // <line>         e.g. 22315                          maxchars = 5
		CID_Name              , // <name>         e.g. blah                           maxchars = 33
		CID_RPFPathName       , // <RPF>          e.g. int_industrial.rpf             maxchars = 30
	//	CID_SourceTemplate    , // <template>     e.g. maps_half/NormalMapHighQuality maxchars = 30
	//	CID_SourceImage       , // <image>        e.g. doors/_ML_HHDoors04_n.bmp      maxchars = 30
		CID_ShaderName        , // <shader>       e.g. normal_spec_decal              maxchars = 20
		CID_ShaderVar         , // <var>          e.g. DiffuseTex                     maxchars = 20
		CID_ModelInfoIndex    , // <index>        e.g. 21443                          maxchars = 5
		CID_ModelInfoType     , // <type>         e.g. VEHICLE                        maxchars = 9
		CID_ModelInfoFlag     , // <?>            e.g. Td                             maxchars = 2
		CID_AssetLoaded       , // <loaded>       e.g. A                              maxchars = 1(6)
		CID_AssetName         , // <assetname>    e.g. blah                           maxchars = 16
		CID_AssetRef          , // <assetref>     e.g. Frag:2551_7*                   maxchars = 12
		CID_AssetStreamID     , // <streamID>     e.g. 1234                           maxchars = 4(8)
		CID_AssetSizeVirt     , // <virtual>      e.g. 9885.33KB                      maxchars = 9
		CID_AssetSizePhys     , // <physical>     e.g. 9885.33KB                      maxchars = 9
		CID_TextureSizePhys   , // <texsizeph>    e.g. 9885.33KB                      maxchars = 9 -- this is significantly larger than CID_TextureSizeIdea for 360, but not for PS3
		CID_TextureSizeIdeal  , // <texsize>      e.g. 9885.33KB                      maxchars = 9 -- ideal texture size computed by sum of bpp times number of pixels for all mips
		CID_TextureSizeWaste  , // <texwaste>     e.g. 9885.33KB                      maxchars = 9 -- physical size minus ideal size (approximation of wasted memory)
		CID_NumTxds           , // <#txd>         e.g. 7                              maxchars = 2(4)
		CID_NumTextures       , // <#tex>         e.g. 213                            maxchars = 3(4)
		CID_Dimensions        , // <dims>         e.g. 104x1024                       maxchars = 9
		CID_DimensionMax      , // <maxdim>       e.g. 1024                           maxchars = 4(6)
		CID_NumMips           , // <mips>         e.g. 11                             maxchars = 2(4)
		CID_TexFormatBPP      , // <bpp>          e.g. 64                             maxchars = 2(3)
		CID_TexFormat         , // <format>       e.g. A8R8G8B8                       maxchars = 8
		CID_TexFormatSwizzle  , // <swiz>         e.g. RGB1                           macchars = 4
		CID_TexConversionFlags, // <conversion>   e.g. BADMETA                        macchars = 7
		CID_TexTemplateType   , // <templatetype> e.g. BUMP+HD                        maxchars = 14
		CID_TexAddress        , // <texaddr>      e.g. 0xdeadc0de                     maxchars = 10
		CID_TexCRC            , // <texcrc>       e.g. 0x12345678                     maxchars = 10
		CID_Info              , // <info>         e.g. TXDPARENT_7                    maxchars = 12
		CID_IsHD              , // <HD>           e.g. Yes                            maxchars = 3
		CID_COUNT             ,
		CID_NONE = INDEX_NONE
	};

	CDTVColumnFlags();

	static const char* GetColumnName    (eColumnID columnID);
	static int         GetColumnMaxChars(eColumnID columnID, const CDTVColumnFlags* overrides);
	static bool        GetColumnTruncate(eColumnID columnID); // should this column's text be truncated

	eColumnID GetColumnID(int column) const;

	bool      m_visible       [CID_COUNT]; // indexed by eColumnID
	eColumnID m_columnIDs     [CID_COUNT]; // columnID's indexed by column index (CID_NONE if not visible and relevant)
	int       m_columnNumChars[CID_COUNT];
	int       m_columnMaxChars_Name;
	int       m_columnMaxChars_AssetName;
};

// ================================================================================================

class CDTVEntry
{
protected:
	CDTVEntry(const char* name) : m_lineNumber(0), m_active(true), m_error(false), m_name(name), m_desc("") { Clear(); }

public:
	virtual ~CDTVEntry() { Clear(); } // does this actually need to be virtual since Clear is virtual?

public:
	virtual void                  Clear            () {}
	virtual void                  Load             (const CAssetRefInterface& ari) { (void)ari; }
	virtual void                  Select           (const CAssetRefInterface& ari) { (void)ari; }
	virtual atString              GetName          () const { return m_name; }
	virtual atString              GetDesc          () const { return m_desc; }
	virtual const fwTxd*          GetPreviewTxd    () const { return NULL; } // persistent, has been addref'd
	virtual const fwTxd*          GetCurrentTxd    () const { return NULL; } // temporary, has not been addref'd
	virtual const grcTexture*     GetPreviewTexture() const { return NULL; } // persistent, has been addref'd
	virtual const grcTexture*     GetCurrentTexture() const { return NULL; } // temporary, has not been addref'd
	virtual const CEntity*        GetEntity        () const { return NULL; } // hopefully persistent but who knows
	virtual const CBaseModelInfo* GetModelInfo     () const { return NULL; }
	virtual const char*           GetShaderVarName () const { return ""; }
	virtual u32                   GetShaderPtr     () const { return 0; }
	virtual bool                  IsPreviewRelevant() const { return false; }
	virtual bool                  IsColumnRelevant (CDTVColumnFlags::eColumnID columnID) const { return columnID == CDTVColumnFlags::CID_LineNumber || columnID == CDTVColumnFlags::CID_Name; }
	virtual float                 GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const = 0; // returns sort key

	static s32 CompareFunc   (CDTVEntry  const* a, CDTVEntry  const* b);
	static s32 CompareFuncPtr(CDTVEntry* const* a, CDTVEntry* const* b) { return CompareFunc(*a, *b); }

	void SetLineNumber(int lineNumber) { m_lineNumber = lineNumber; } // called after sorting

	void SetActiveFlag(bool bActive) { m_active = bActive; }
	bool GetActiveFlag() const { return m_active; }
	bool GetErrorFlag() const { return m_error; }
	void SetErrorFlag(bool bError) { m_error = bError; }

	int SetupWindow(CDebugTextureViewerWindow* debugWindow, CDTVColumnFlags& cf) const; // returns offset of last column

protected:
	int      m_lineNumber;
	bool     m_active;
	bool     m_error;
	atString m_name;
	atString m_desc;
};

class CDTVEntry_Generic : public CDTVEntry
{
public:
	CDTVEntry_Generic(const char* name);

private:
	virtual float GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const;
};

class CDTVEntry_ModelInfo : public CDTVEntry
{
public:
	CDTVEntry_ModelInfo(const char* name, u32 modelInfoIndex, const CEntity* entity);

protected:
	virtual void                  Clear            ();
	virtual void                  Load             (const CAssetRefInterface& ari);
	virtual void                  Select           (const CAssetRefInterface& ari);
	virtual atString              GetDesc          () const;
	virtual const CEntity*        GetEntity        () const;
	virtual const CBaseModelInfo* GetModelInfo     () const { return m_modelInfo; }
	virtual bool                  IsColumnRelevant (CDTVColumnFlags::eColumnID columnID) const;
	virtual float                 GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const;

	const CBaseModelInfo* m_modelInfo;
	u32         m_modelInfoIndex;
	RegdEnt               m_entity;
};

class CDTVEntry_Shader : public CDTVEntry
{
public:
	CDTVEntry_Shader(const grmShader* shader);

protected:
};

// _Txd entry can be in several states ..
// 1. m_txd is not NULL - the txd is assumed to have been addref'd appropriately and can be previewed
// 2. m_txd is NULL, but GetCurrentTxd() returns a non-NULL txd -  this txd is valid to use for the current
//    frame in the update thread, but not for the render thread. it can still be used to extract information
// 3. m_txd is NULL, and GetCurrentTxd() returns NULL - the txd is not loaded, however we CAN call "Load"
//    on it because m_ref might be stream-in-able. 

class CDTVEntry_Txd : public CDTVEntry
{
public:
	CDTVEntry_Txd(const CTxdRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies);
	CDTVEntry_Txd(const CTxdRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies, const CAssetRefInterface& ari); // sets m_txd

	const CTxdRef& GetTxdRef() const { return m_ref; }

	void Select_Public(const CAssetRefInterface& ari) { Select(ari); }

protected:
	virtual void         Clear            ();
	virtual void         Load             (const CAssetRefInterface& ari);
	virtual void         Select           (const CAssetRefInterface& ari);
	virtual atString     GetDesc          () const;
	virtual const fwTxd* GetPreviewTxd    () const;
	virtual const fwTxd* GetCurrentTxd    () const;
	virtual bool         IsPreviewRelevant() const { return true; }
	virtual bool         IsColumnRelevant (CDTVColumnFlags::eColumnID columnID) const;
	virtual float        GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const;

	pgRef<const fwTxd> m_txd;
	CTxdRef            m_ref;
	int                m_parentTxdIndex; // might be slow to calculate, so we store it
	atArray<u32>       m_dependentModelInfos;
};

class CDTVEntry_Texture : public CDTVEntry
{
public:
	CDTVEntry_Texture(const CTxdRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies, const grcTexture* texture);

	const CTxdRef& GetTxdRef() const { return m_ref; }

	const grcTexture* GetCurrentTexture_Public() const { return GetCurrentTexture(); }
	u32 GetTexCRC_Public() const { return m_texCRC; }
	int GetTexSizeInBytes_Public() const { return m_texSizeInBytes; } // ideal size

protected:
	virtual void              Clear            ();
	virtual atString          GetDesc          () const;
	virtual const grcTexture* GetPreviewTexture() const;
	virtual const grcTexture* GetCurrentTexture() const { return GetPreviewTexture(); } // we don't have txdEntry, so we can't get the texture from the txd ..
	virtual bool              IsPreviewRelevant() const { return true; }
	virtual bool              IsColumnRelevant (CDTVColumnFlags::eColumnID columnID) const;
	virtual float             GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const;

	pgRef<const grcTexture> m_texture;
	CTxdRef                 m_ref;
	int                     m_texSizeInBytesPhys;
	int                     m_texSizeInBytes;
	int                     m_texWidth;
	int                     m_texHeight;
	int                     m_texNumMips;
	int                     m_texFormat; // grcImage::Format
	char                    m_texFormatSwizzle[5]; // NULL-terminated string
	u8                      m_texConversionFlags;
	u16                     m_texTemplateType;
	u32                     m_texCRC;
	atString                m_shaderVarName; // should store this in CDTVEntry_TextureRef, but needs to be accessed by GetTextAndSortKey
	atArray<u32>            m_dependentModelInfos;
	atArray<CTxdRef>        m_dependentTxdRefs;
};

// slight variation on _Texture entry, where m_texture is set to NULL initially but can be loaded later from m_ref and m_txdEntry
class CDTVEntry_TextureRef : public CDTVEntry_Texture
{
public:
	CDTVEntry_TextureRef(const CTextureRef& ref, const CAssetSearchParams& asp, bool bAllowFindDependencies, const char* shaderVarName = NULL);

private:
	virtual void              Load             (const CAssetRefInterface& ari);
	virtual void              Select           (const CAssetRefInterface& ari) { Load(ari); } // select is the same as load
	virtual const grcTexture* GetCurrentTexture() const;
	virtual const char*       GetShaderVarName () const { return m_shaderVarName.c_str() ? m_shaderVarName.c_str() : ""; }
	virtual u32               GetShaderPtr     () const { return m_shaderPtr; }
	virtual bool              IsColumnRelevant (CDTVColumnFlags::eColumnID columnID) const;

	int m_txdEntry;
	u32 m_shaderPtr;
};

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION
class CDTVEntry_ShaderEditTexture : public CDTVEntry
{
public:
	CDTVEntry_ShaderEditTexture(const grcTexture* texture, const char* name);
	~CDTVEntry_ShaderEditTexture();

	const grcTexture* GetTexture_Public() const { return m_texture; }

private:
	virtual const grcTexture* GetPreviewTexture() const { return m_texture; }
	virtual const grcTexture* GetCurrentTexture() const { return m_texture; }
	virtual bool              IsPreviewRelevant() const { return true; }
	virtual bool              IsColumnRelevant (CDTVColumnFlags::eColumnID columnID) const;
	virtual float             GetTextAndSortKey(char* result, CDTVColumnFlags::eColumnID columnID) const;

	const grcTexture* m_texture;
};
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERENTRY_H_
