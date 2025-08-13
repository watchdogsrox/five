//
// GTA terrain Combo shader (up to 8 layers using a lookup texture, multitexture):
//

#define SPECULAR 1
#define NORMAL_MAP 1
#define DOUBLEUV_SET 1		// This is implied by USE_TEXTURE_LOOKUP
#define USE_TEXTURE_LOOKUP 1
#define USE_ALPHACLIP_FADE
#define LAYER_COUNT 4

#include "terrain_cb_common.fxh"

