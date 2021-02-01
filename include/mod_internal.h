#ifndef __mod_internal_h
#define __mod_internal_h

#include "QF/iqm.h"
#include "QF/model.h"
#include "QF/skin.h"
#include "QF/plugin/vid_render.h"

int Mod_CalcFullbright (const byte *in, byte *out, int pixels);
void Mod_ClearFullbright (const byte *in, byte *out, int pixels);
void Mod_FloodFillSkin (byte *skin, int skinwidth, int skinheight);
//FIXME gl specific. rewrite to use above
int Mod_Fullbright (byte * skin, int width, int height, const char *name);

void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_FindClipDepth (hull_t *hull);

model_t	*Mod_FindName (const char *name);
float RadiusFromBounds (const vec3_t mins, const vec3_t maxs) __attribute__((pure));

struct vulkan_ctx_s;

extern vid_model_funcs_t *m_funcs;

void gl_Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m,
										int _s, int extra);
void *gl_Mod_LoadSkin (model_t *mod, byte *skin, int skinsize, int snum,
					   int gnum, qboolean group, maliasskindesc_t *skindesc);
void gl_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr);
void gl_Mod_LoadExternalSkins (model_t *mod);
void gl_Mod_IQMFinish (model_t *mod);

void glsl_Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr,
										  void *_m, int _s, int extra);
void *glsl_Mod_LoadSkin (model_t *mod, byte *skin, int skinsize, int snum,
						 int gnum, qboolean group, maliasskindesc_t *skindesc);
void glsl_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr);
void glsl_Mod_LoadExternalSkins (model_t *mod);
void glsl_Mod_IQMFinish (model_t *mod);

void sw_Mod_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr, void *_m,
										int _s, int extra);
void *sw_Mod_LoadSkin (model_t *mod, byte *skin, int skinsize, int snum,
					   int gnum, qboolean group, maliasskindesc_t *skindesc);
void sw_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr);
void sw_Mod_LoadExternalSkins (model_t *mod);
void sw_Mod_IQMFinish (model_t *mod);

void gl_Mod_LoadLighting (model_t *mod, bsp_t *bsp);
void gl_Mod_SubdivideSurface (model_t *mod, msurface_t *fa);
void gl_Mod_ProcessTexture (model_t *mod, texture_t *tx);

void glsl_Mod_LoadLighting (model_t *mod, bsp_t *bsp);
void glsl_Mod_ProcessTexture (model_t *mod, texture_t *tx);

void sw_Mod_LoadLighting (model_t *mod, bsp_t *bsp);

void Vulkan_Mod_LoadLighting (model_t *mod, bsp_t *bsp,
							  struct vulkan_ctx_s *ctx);
void Vulkan_Mod_SubdivideSurface (model_t *mod, msurface_t *fa,
								  struct vulkan_ctx_s *ctx);
void Vulkan_Mod_ProcessTexture (model_t *mod, texture_t *tx,
								struct vulkan_ctx_s *ctx);

void gl_Mod_SpriteLoadTexture (model_t *mod, mspriteframe_t *pspriteframe,
							   int framenum);
void glsl_Mod_SpriteLoadTexture (model_t *mod, mspriteframe_t *pspriteframe,
								 int framenum);
void sw_Mod_SpriteLoadTexture (model_t *mod, mspriteframe_t *pspriteframe,
							   int framenum);

void Mod_LoadIQM (model_t *mod, void *buffer);
void Mod_FreeIQM (iqm_t *iqm);
iqmblend_t *Mod_IQMBuildBlendPalette (iqm_t *iqm, int *size);
void Mod_LoadAliasModel (model_t *mod, void *buffer,
                         cache_allocator_t allocator);
void Mod_LoadSpriteModel (model_t *mod, void *buffer);

void Skin_Init (void);
skin_t *Skin_SetColormap (skin_t *skin, int cmap);
skin_t *Skin_SetSkin (skin_t *skin, int cmap, const char *skinname);
void Skin_SetTranslation (int cmap, int top, int bottom);
int Skin_CalcTopColors (const byte *in, byte *out, int pixels);
int Skin_CalcBottomColors (const byte *in, byte *out, int pixels);
void Skin_ClearTopColors (const byte *in, byte *out, int pixels);
void Skin_ClearBottomColors (const byte *in, byte *out, int pixels);

void sw_Skin_SetupSkin (skin_t *skin, int cmap);
void sw_Skin_ProcessTranslation (int cmap, const byte *translation);
void sw_Skin_InitTranslations (void);

void glsl_Skin_SetupSkin (skin_t *skin, int cmap);
void glsl_Skin_ProcessTranslation (int cmap, const byte *translation);
void glsl_Skin_InitTranslations (void);

void gl_Skin_SetupSkin (skin_t *skin, int cmap);
void gl_Skin_ProcessTranslation (int cmap, const byte *translation);
void gl_Skin_InitTranslations (void);
int gl_Skin_Init_Textures (int base);
void gl_Skin_SetPlayerSkin (int width, int height, const byte *data);
#endif// __mod_internal_h
