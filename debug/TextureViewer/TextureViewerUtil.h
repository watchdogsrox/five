// =======================================
// debug/textureviewer/textureviewerutil.h
// (c) 2010 RockstarNorth
// =======================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERUTIL_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERUTIL_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

#include "atl/string.h" // atString
#include "vectormath/classes.h"

namespace rage { class grcTexture; }
namespace rage { class grmShader; }
namespace rage { template <typename> class pgDictionary; typedef pgDictionary<grcTexture> fwTxd; }
namespace rage { class rmcDrawable; }
class CBaseModelInfo;
class CDynamicEntity;
class CEntity;

class CTxdRef;

// ================================================================================================

bool        StringMatch(const char* str, const char* searchStr, bool bExactMatch = false);
atString    StringTruncate(const char* str, int maxLength, const char* trailing = "...");
atString    StringPadded(const char* str, int paddedLength, char padding, bool bAlignRight);
atString    GetFriendlyTextureName(const grcTexture* texture);
const char* GetEntityTypeStr(int type, bool bAbbrev); // type comes from CEntity::GetType()
const char* GetModelInfoTypeStr(int type, bool bAbbrev); // type comes from CModelInfo::GetModelType()
atString    GetModelInfoDesc(u32 modelInfoIndex, bool bNewFormat = false);
int         GetTextureSizeInBytes(const grcTexture* texture);
u32         GetTextureHash(const grcTexture* texture, bool bHashName, int numLines);
int         GetTextureDictionarySizeInBytes(const fwTxd* txd, bool bPhysical = false);
int         FindTxdSlot(const fwTxd* txd);
void        PrintFragChildrenDesc(const CDynamicEntity* entity);
bool        IsTextureUsedByModel(const grcTexture* texture, const char* textureName, const CBaseModelInfo* modelInfo, const CEntity* entity, const char** shaderName = NULL, const char** shaderVarName = NULL, bool bCheckNameOnly = false);
bool        IsTextureUsedByDrawable_Dwd     (int slot, int index, const rmcDrawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList = NULL);
bool        IsTextureUsedByDrawable_Drawable(int slot, int index, const rmcDrawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList = NULL);
bool        IsTextureUsedByDrawable_Fragment(int slot, int index, const rmcDrawable* drawable, const grcTexture* texture, const char* textureName, const char** shaderName, const char** shaderVarName, bool bCheckNameOnly, atArray<const grcTexture*>* textureList = NULL);
int         GetShadersUsedByModel(atArray<const grmShader*>& shaders, const CBaseModelInfo* modelInfo, bool bUniqueNamesOnly);
int         GetShadersUsedByEntity(atArray<const grmShader*>& shaders, const CEntity* entity, bool bUniqueNamesOnly); // returns total number of shaders (not just unique)
#if __DEV
void        PrintTexturesUsedByModel(const CBaseModelInfo* modelInfo);
void        PrintTexturesUsedByEntity(const CEntity* entity);
#endif // __DEV
#if DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST
void        PrintDrawableMeshData(const rmcDrawable* drawable, Mat34V_In mat);
void        PrintModelMeshData(const CBaseModelInfo* modelInfo);
void        PrintEntityMeshData(const CEntity* entity);
#endif // DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST
bool        IsTextureConstant(const grcTexture* texture);
void        InvertTextureChannels(const grcTexture* texture, int mipIndex, int mipCount, bool bInvertRed, bool bInvertGreen, bool bInvertBlue, bool bInvertAlpha, bool bReverseBits, bool bVerbose);
void        SetSolidTextureColour(const grcTexture* texture, int mipIndex, int mipCount, Color32 colour, bool bVerbose);

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERUTIL_H_
