// ==========================================
// debug/textureviewer/textureviewerprivate.h
// (c) 2010 RockstarNorth
// ==========================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERPRIVATE_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERPRIVATE_H_

// NOTE -- turned off on 360 to reduce code footprint (this old streaming iterator system is going away eventually anyway)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR (1 && __DEV && __PS3)

#if __BANK

#if __DEV
	#define DEBUG_TEXTURE_VIEWER_OPTIMISATIONS RENDER_OPTIMISATIONS//OPTIMISATIONS_OFF
#else
	#define DEBUG_TEXTURE_VIEWER_OPTIMISATIONS RENDER_OPTIMISATIONS
#endif

// ================================================================================================

#define DEBUG_TEXTURE_VIEWER_DISABLE_PLAYER                                  (1)
#define DEBUG_TEXTURE_VIEWER_HISTOGRAM                                       (1 && !RSG_PC) // pc can't support this as it requires locking textures
#define DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD                     (0 && __DEV)
#define DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION                         (1 && __DEV) // let's try this again ok?
#define DEBUG_TEXTURE_VIEWER_GET_WORLD_POS_UNDER_MOUSE                       (1)
#define DEBUG_TEXTURE_VIEWER_FRAMEBUFFER_MAGNIFIER                           (1)
#define DEBUG_TEXTURE_VIEWER_ENTITY_FLAGS                                    (1 && __DEV)
#define DEBUG_TEXTURE_VIEWER_GETPIXELTEST                                    (1 && !RSG_PC) // pc can't support this as it requires locking textures
#define DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE                                    (1)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST                          (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TXDCHILDREN                   (0 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_REDUNDANT                     (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST) // find redundant textures by hash
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS                     (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST) // build usage maps (e.g. which textures are used as BumpTex, etc.)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_ONCE                (0 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS) // if 0, usage will be found one asset at a time but not in perfect order
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO                (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED                   (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_FIND_UNUSED_DRAWABLES_IN_DWDS (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST)
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEXTURE                       (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST) // various functions which run over every texture
#define DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_RPFTEST                       (1 && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_TEST)
#define DEBUG_TEXTURE_VIEWER_TEXTUREUSAGEANALYSER                            (1 && __DEV)
#define DEBUG_TEXTURE_VIEWER_UIGADGET_DEMO                                   (0 && __DEV)
#define DEBUG_TEXTURE_VIEWER_TRACKASSETHASH                                  (0 && __DEV) // testing when asset store names change ..
#define DEBUG_TEXTURE_VIEWER_ADVANCED                                        (0 && __DEV)
#define DEBUG_TEXTURE_VIEWER_OVERRIDE_REFLECTION_TEX                         (1 && __DEV) // experimental
#define DEBUG_TEXTURE_VIEWER_SELECTPREVIEW                                   (0) // now working yet
#define DEBUG_TEXTURE_VIEWER_NEW_STREAMING_CODE                              (1) // hacked in .. need to handle updating txds properly
#define DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING                                (1) // new shizzit
#define DEBUG_TEXTURE_VIEWER_MESH_DUMP_TEST                                  (1 && __DEV && __PS3) // quit screwing around!!
#define DEBUG_TEXTURE_VIEWER_INVERT_TEXTURE_CHANNELS                         (1 && __DEV && __CONSOLE)
#define DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES                       (1 && __DEV) // e.g. BumpTex with Bumpiness=0, etc.
#define DEBUG_TEXTURE_VIEWER_EDIT_METADATA_IN_WORKBENCH                      (1)

#define DEBUG_TEXTURE_VIEWER_WINDOW_POS Vector2(200.0f, 250.0f)

// this is problematic (see BS#560475 etc.) so i'm disabling this for now ..
#define DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY() //DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY()

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERPRIVATE_H_
