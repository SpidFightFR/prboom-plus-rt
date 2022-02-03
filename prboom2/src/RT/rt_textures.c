#include "rt_textures.h"

#include "rt_main.h"
#include "r_patch.h"
#include "v_video.h"
#include "w_wad.h"


struct
{
  rt_texture_t all_TXTR[RG_MAX_TEXTURE_COUNT];
  rt_texture_t all_PFS[RG_MAX_TEXTURE_COUNT];
  rt_texture_t all_PFS_STATIC[RG_MAX_TEXTURE_COUNT];
  // int count_TXTR;
  // int count_PFS;
  // int count_PFS_STATIC;
}
rttextures;


static rt_texture_t *RT_Texture_AllocEntry_T(int id, rt_texture_t *all_T)
{
  assert(id >= 0);
  if (id >= RG_MAX_TEXTURE_COUNT)
  {
    assert_always("Texture ID must be less than RG_MAX_TEXTURE_COUNT");
    return NULL;
  }

  rt_texture_t *td = &all_T[id];

  if (td->exists)
  {
    assert_always("Trying to alloc texture with ID that already exists");
    return NULL;
  }

  memset(td, 0, sizeof(*td));
  td->exists = true;

  return td;

  /*
  for (int i = 0; i < RG_MAX_TEXTURE_COUNT; i++)
  {
    rt_texture_t *td = &all_T[i];

    if (!td->exists)
    {
      td->exists = true;
      return td;
    }
  }

  assert_always("Not enough texture entries");
  return NULL;
  */
}


void RT_Texture_Destroy(void)
{
  memset(&rttextures, 0, sizeof(rttextures));
}


// Returns initialized rt_texture_t btu with empty 'rg_handle'
static rt_texture_t *RT_Texture_RegisterPatch(int lump, const rpatch_t *patch)
{
  assert(lump >= 0);
  if (lump >= RG_MAX_TEXTURE_COUNT)
  {
    assert_always("lump must be less than RG_MAX_TEXTURE_COUNT");
    return NULL;
  }

  int is_static = lumpinfo[lump].flags & LUMP_STATIC;

  rt_texture_t *td = RT_Texture_AllocEntry_T(lump, is_static ? rttextures.all_PFS_STATIC : rttextures.all_PFS);

  if (td == NULL)
  {
    return NULL;
  }

  assert(td->exists);
  td->lump_type = is_static ? RT_TEXTURE_LUMP_TYPE_PATCHES_FLATS_SPRITES_STATIC : RT_TEXTURE_LUMP_TYPE_PATCHES_FLATS_SPRITES;
  td->lump_id = lump;
  td->width = patch->width;
  td->height = patch->height;
  td->leftoffset = patch->leftoffset;
  td->topoffset = patch->topoffset;

  // will be initialized by caller
  td->rg_handle = RG_NO_MATERIAL;
  return td;
}


static const rt_texture_t *RT_Texture_TryFindPatch(int lump)
{
  if (lump >= RG_MAX_TEXTURE_COUNT)
  {
    assert_always("lump must be less than RG_MAX_TEXTURE_COUNT");
    return NULL;
  }

  int is_static = lumpinfo[lump].flags & LUMP_STATIC;
  const rt_texture_t *td = is_static ? &rttextures.all_PFS_STATIC[lump] : &rttextures.all_PFS[lump];

  if (td->exists)
  {
    assert(td->lump_id == lump);
    assert(td->width > 0 && td->height > 0);
    assert(
      (is_static && td->lump_type == RT_TEXTURE_LUMP_TYPE_PATCHES_FLATS_SPRITES_STATIC) ||
      (!is_static && td->lump_type == RT_TEXTURE_LUMP_TYPE_PATCHES_FLATS_SPRITES));
    
    return td;
  }

  return NULL;
}


static void DT_AddPatchToTexture_UnTranslated(uint8_t *buffer, const rpatch_t *patch, int originx, int originy)
{
  int paletted = 0;
  int texture_realtexwidth = patch->width;
  int texture_realtexheight = patch->height;
  int texture_buffer_width = patch->width;

  int x, y, j;
  int js, je;
  const rcolumn_t *column;
  const byte *source;
  int i, pos;

  if (!patch)
    return;

  const unsigned char *playpal = V_GetPlaypal();
  int xs = 0;
  int xe = patch->width;

  if ((xs + originx) >= texture_realtexwidth)
    return;
  if ((xe + originx) <= 0)
    return;
  if ((xs + originx) < 0)
    xs = -originx;
  if ((xe + originx) > texture_realtexwidth)
    xe += (texture_realtexwidth - (xe + originx));

  // if (patch->flags & PATCH_HASHOLES)
  //  gltexture->flags |= GLTEXTURE_HASHOLES;

  for (x = xs; x < xe; x++)
  {
  #ifdef RANGECHECK
    if (x >= patch->width)
    {
      lprintf(LO_ERROR, "AddPatchToTexture_UnTranslated x>=patch->width (%i >= %i)\n", x, patch->width);
      return;
    }
  #endif
    column = &patch->columns[x];
    for (i = 0; i < column->numPosts; i++)
    {
      const rpost_t *post = &column->posts[i];
      y = (post->topdelta + originy);
      js = 0;
      je = post->length;
      if ((js + y) >= texture_realtexheight)
        continue;
      if ((je + y) <= 0)
        continue;
      if ((js + y) < 0)
        js = -y;
      if ((je + y) > texture_realtexheight)
        je += (texture_realtexheight - (je + y));
      source = column->pixels + post->topdelta;
      if (paletted)
      {
        assert_always("gl_paletted_texture is always false for RT");
        //pos = (((js + y) * texture_buffer_width) + x + originx);
        //for (j = js; j < je; j++, pos += (texture_buffer_width))
        //{
        //#ifdef RANGECHECK
        //  if (pos >= gltexture->buffer_size)
        //  {
        //    lprintf(LO_ERROR, "AddPatchToTexture_UnTranslated pos>=size (%i >= %i)\n", pos + 3, gltexture->buffer_size);
        //    return;
        //  }
        //#endif
        //  buffer[pos] = gld_palmap[source[j]];
        //}
      }
      else
      {
        pos = 4 * (((js + y) * texture_buffer_width) + x + originx);
        for (j = js; j < je; j++, pos += (4 * texture_buffer_width))
        {
        #ifdef RANGECHECK
          if ((pos + 3) >= gltexture->buffer_size)
          {
            lprintf(LO_ERROR, "AddPatchToTexture_UnTranslated pos+3>=size (%i >= %i)\n", pos + 3, gltexture->buffer_size);
            return;
          }
        #endif
          //if (gl_boom_colormaps && use_boom_cm && !(comp[comp_skymap] && (gltexture->flags & GLTEXTURE_SKY)))
          //{
          //  const lighttable_t *colormap = (fixedcolormap ? fixedcolormap : fullcolormap);
          //  buffer[pos + 0] = playpal[colormap[source[j]] * 3 + 0];
          //  buffer[pos + 1] = playpal[colormap[source[j]] * 3 + 1];
          //  buffer[pos + 2] = playpal[colormap[source[j]] * 3 + 2];
          //}
          //else
          {
            buffer[pos + 0] = playpal[source[j] * 3 + 0];
            buffer[pos + 1] = playpal[source[j] * 3 + 1];
            buffer[pos + 2] = playpal[source[j] * 3 + 2];
          }
          buffer[pos + 3] = 255;
        }
      }
    }
  }
}

static void DT_AddPatchToTexture(unsigned char *buffer, const rpatch_t *patch)
{
  if (!patch)
    return;

  // force for RT
  int cm = CR_DEFAULT;
  int originx = 0;
  int originy = 0;

  assert(cm == CR_DEFAULT);
  DT_AddPatchToTexture_UnTranslated(buffer, patch, originx, originy);
}

const rt_texture_t *RT_Texture_GetFromPatchLump(int lump)
{
  {
    const rt_texture_t *existing = RT_Texture_TryFindPatch(lump);

    if (existing != NULL)
    {
      return existing;
    }
  }

  const rpatch_t *patch = R_CachePatchNum(lump);

  if (patch == NULL || patch->width == 0 || patch->height == 0)
  {
    return NULL;
  }

  rt_texture_t *td = RT_Texture_RegisterPatch(lump, patch);

  if (td == NULL)
  {
    return NULL;
  }
  
  int texture_buffer_size = td->width * td->height * 4;

  uint8_t *buffer = Z_Malloc(texture_buffer_size, PU_STATIC, 0);
  memset(buffer, 0, texture_buffer_size);
  DT_AddPatchToTexture(buffer, patch);
  R_UnlockPatchNum(lump);

  RgStaticMaterialCreateInfo info =
  {
    .size = { td->width, td->height },
    .textures = {.albedoAlpha = {.isSRGB = true, .pData = buffer } },
    .pRelativePath = NULL,
    .filter = RG_SAMPLER_FILTER_NEAREST,
    .addressModeU = RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    .addressModeV = RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  };

  RgResult r = rgCreateStaticMaterial(rtmain.instance, &info, &td->rg_handle);
  RG_CHECK(r);

  free(buffer);
  return td;
}