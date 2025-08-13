
//DEF_TechniqueVariant_Tessellated contains both tessellated(PC only) and non tessellated 
//versions and is used on all platforms

// draw.
//DEF_TechniqueNameVSPS				(draw, VS_none, PS_RGBA, OFF, OFF)

// RGBA techniques
DEF_Technique							(RGBA,							ON,  OFF)
DEF_TechniqueVariant_Tessellated		(RGBA, lit,						ON,  ON)
DEF_TechniqueVariant					(RGBA, soft,					ON,  ON)
DEF_TechniqueVariant					(RGBA, soft_waterrefract,		ON,  ON)
DEF_TechniqueVariant_Tessellated		(RGBA, lit_soft,				ON,  ON)
DEF_TechniqueVariant_Tessellated		(RGBA, lit_soft_waterrefract,	ON,  ON)
DEF_TechniqueVariant					(RGBA, screen,					ON,  OFF)
DEF_TechniqueVariant_Tessellated		(RGBA, lit_screen,				ON,  OFF)
DEF_TechniqueVariant					(RGBA, lit_normalspec,			ON,  OFF)
DEF_TechniqueVariant					(RGBA, lit_soft_normalspec,		OFF, OFF)
DEF_TechniqueVariant					(RGBA, lit_screen_normalspec,	ON,  OFF)

// RG_Blend techniques (not currently used)
//DEF_Technique							(RG_Blend,						OFF, OFF)
//DEF_TechniqueVariant_Tessellated		(RG_Blend, lit,					OFF, OFF)
//DEF_TechniqueVariant					(RG_Blend, soft,				OFF, OFF)
//DEF_TechniqueVariant_Tessellated		(RG_Blend, lit_soft,			OFF, OFF)

// RGB techniques
DEF_Technique							(RGB,							ON,  OFF)
DEF_TechniqueVariant_Tessellated		(RGB, lit,						ON,  ON)
DEF_TechniqueVariant					(RGB, soft,						ON,  ON)
DEF_TechniqueVariant					(RGB, soft_waterrefract,		ON,  ON)
DEF_TechniqueVariant_Tessellated		(RGB, lit_soft,					ON,  ON)
DEF_TechniqueVariant_Tessellated		(RGB, lit_soft_waterrefract,	ON,  ON)
DEF_TechniqueVariant					(RGB, screen,					ON,  OFF)
DEF_TechniqueVariant_Tessellated		(RGB, lit_screen,				ON,  OFF)
DEF_TechniqueVariantVS					(RGB, refract, none,			ON,  OFF)
DEF_TechniqueVariantVS					(RGB, soft_refract, soft,		ON,  OFF)
DEF_TechniqueVariantVS					(RGB, screen_refract, screen,	ON,  OFF)
DEF_TechniqueVariant					(RGB, lit_screen_normalspec,	ON,  OFF)
DEF_TechniqueVariantVSPS				(RGB, lit_screen_refract_normalspec, lit_screen_normalspec, screen_refract_normalspec, ON, OFF)

// Thermal techniques
DEF_TechniqueVariantVS					(RGBA,		Thermal, thermal,	ON,  ON)
//DEF_TechniqueVariantVS					(RG_Blend,	Thermal, thermal,	OFF, OFF)
DEF_TechniqueVariantVS					(RGB,		Thermal, thermal,	ON,  ON)

// Water projection technique.
DEF_TechniqueNameVSPS					(RGBA_proj, VS_proj, PS_proj,   ON,  OFF)
DEF_TechniqueNameVSPS					(RGBA_proj_waterrefract,	VS_proj_waterrefract, PS_proj_waterrefract,	ON, OFF)

