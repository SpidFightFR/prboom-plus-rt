#include "rt_main.h"
#include "r_main.h"


#define STRETCHSKY 0

#define SKYDETAIL 16

#define SKYROWS 4
#define SKYCOLUMNS (4 * SKYDETAIL)

#define SKY_TRIANGLE_FAN 1
#define SKY_TRIANGLE_STRIP 2

typedef struct
{
  int mode;
  int vertexcount;
  int vertexindex;
  int use_texture;
} RTSkyLoopDef;


typedef struct
{
  int rows, columns;
  int loopcount;
  RTSkyLoopDef *loops;
  RgRasterizedGeometryVertexStruct *data;
  uint32_t vertex_count;
} RTSkyVBO;


static dboolean yflip;
static int texw;
static float yMult, yAdd;
static dboolean foglayer;
static float delta = 0.0f;


static void SkyVertex(RgRasterizedGeometryVertexStruct *vbo, int r, int c, dboolean sky_gldwf_skyflip)
{
  static fixed_t scale = 10000 << FRACBITS;
  static angle_t maxSideAngle = ANG180 / 3;

  angle_t topAngle = (angle_t)(c / (float)SKYCOLUMNS * ANGLE_MAX);
  angle_t sideAngle = maxSideAngle * (SKYROWS - r) / SKYROWS;
  fixed_t height = finesine[sideAngle >> ANGLETOFINESHIFT];
  fixed_t realRadius = FixedMul(scale, finecosine[sideAngle >> ANGLETOFINESHIFT]);
  fixed_t x = FixedMul(realRadius, finecosine[topAngle >> ANGLETOFINESHIFT]);
  fixed_t y = (!yflip) ? FixedMul(scale, height) : FixedMul(scale, height) * -1;
  fixed_t z = FixedMul(realRadius, finesine[topAngle >> ANGLETOFINESHIFT]);

  float timesRepeat = (short)(4 * (256.0f / texw));
  if (timesRepeat == 0.0f)
  {
    timesRepeat = 1.0f;
  }

  if (!foglayer)
  {
    vbo->packedColor = RT_PackColor(255, 255, 255, r == 0 ? 0 : 255);

    // And the texture coordinates.
    if (!yflip)	// Flipped Y is for the lower hemisphere.
    {
      vbo->texCoord[0] = (-timesRepeat * c / (float)SKYCOLUMNS);
      vbo->texCoord[1]= (r / (float)SKYROWS) * 1.f * yMult + yAdd;
    }
    else
    {
      vbo->texCoord[0] = (-timesRepeat * c / (float)SKYCOLUMNS);
      vbo->texCoord[1] = ((SKYROWS - r) / (float)SKYROWS) * 1.f * yMult + yAdd;
    }

    if (sky_gldwf_skyflip)
    {
      vbo->texCoord[0] *= -1;
    }
  }

  if (r != 4)
  {
    y += FRACUNIT * 300;
  }

  // And finally the vertex.
  vbo->position[0] = -(float)x / (float)MAP_SCALE;	// Doom mirrors the sky vertically!
  vbo->position[1] = (float)y / (float)MAP_SCALE + delta;
  vbo->position[2] = (float)z / (float)MAP_SCALE;
}


static void BuildSky(RTSkyVBO *vbo, const rt_texture_t *sky_texture, float sky_y_offset, dboolean sky_gldwf_skyflip)
{
  int row_count = SKYROWS;
  int col_count = SKYCOLUMNS;

  int texh, c, r;
  int vertex_count = 2 * row_count * (col_count * 2 + 2) + col_count * 2;

  if ((vbo->columns != col_count) || (vbo->rows != row_count))
  {
    free(vbo->loops);
    free(vbo->data);
    memset(vbo, 0, sizeof(vbo[0]));
  }

  if (!vbo->data)
  {
    memset(vbo, 0, sizeof(vbo[0]));
    vbo->loops = malloc((row_count * 2 + 2) * sizeof(vbo->loops[0]));
    // create vertex array
    vbo->data = malloc(vertex_count * sizeof(vbo->data[0]));
    vbo->vertex_count = vertex_count;
  }

  vbo->columns = col_count;
  vbo->rows = row_count;

  texh = sky_texture->height;
  if (texh > 190 && STRETCHSKY)
    texh = 190;
  texw = sky_texture->width;

  RgRasterizedGeometryVertexStruct *vertex_p = &vbo->data[0];
  vbo->loopcount = 0;
  for (yflip = 0; yflip < 2; yflip++)
  {
    vbo->loops[vbo->loopcount].mode = SKY_TRIANGLE_FAN;
    vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
    vbo->loops[vbo->loopcount].vertexcount = col_count;
    vbo->loops[vbo->loopcount].use_texture = false;
    vbo->loopcount++;

    byte skyColor[3] = { 255,255,255 };

    yAdd = sky_y_offset / texh;
    yMult = (texh <= 180 ? 1.0f : 180.0f / texh);
    if (yflip == 0)
    {
      // skyColor = &sky->CeilingSkyColor[vbo_idx];
    }
    else
    {
      // skyColor = &sky->FloorSkyColor[vbo_idx];
      if (texh <= 180) yMult = 1.0f; else yAdd += 180.0f / texh;
    }

    delta = 0.0f;
    foglayer = true;
    for (c = 0; c < col_count; c++)
    {
      SkyVertex(vertex_p, 1, c, sky_gldwf_skyflip);
      vertex_p->packedColor = RT_PackColor(skyColor[0], skyColor[1], skyColor[2], 255);
      vertex_p++;
    }
    foglayer = false;

    delta = (yflip ? 5.0f : -5.0f) / MAP_COEFF;

    for (r = 0; r < row_count; r++)
    {
      vbo->loops[vbo->loopcount].mode = SKY_TRIANGLE_STRIP;
      vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
      vbo->loops[vbo->loopcount].vertexcount = 2 * col_count + 2;
      vbo->loops[vbo->loopcount].use_texture = true;
      vbo->loopcount++;

      for (c = 0; c <= col_count; c++)
      {
        SkyVertex(vertex_p++, r + (yflip ? 1 : 0), (c ? c : 0), sky_gldwf_skyflip);
        SkyVertex(vertex_p++, r + (yflip ? 0 : 1), (c ? c : 0), sky_gldwf_skyflip);
      }
    }
  }
}


static int GetTriangleCount(int mode, int vertex_count)
{
  assert(mode == SKY_TRIANGLE_STRIP || mode == SKY_TRIANGLE_FAN);
  return max(0, vertex_count - 2);
}


void RT_AddSkyDome(void)
{
  if (rtmain.sky.texture == NULL || !rtmain.was_new_sky)
  {
    return;
  }


  RTSkyVBO v_vbo = { 0 };
  RTSkyVBO *vbo = &v_vbo;

  BuildSky(vbo, rtmain.sky.texture, rtmain.sky.y_offset, rtmain.sky.gldwf_skyflip);



  // TODO RT: sky caps (instead of getting average color just use (0,0) tex coords?)



  //glRotatef(roll, 0.0f, 0.0f, 1.0f);
  //glRotatef(pitch, 1.0f, 0.0f, 0.0f);
  //glRotatef(yaw, 0.0f, 1.0f, 0.0f);
  //glScalef(-2.0f, 2.0f, 2.0f);
  //glTranslatef(0.f, -1250.0f / MAP_COEFF, 0.f);
  //glRotatef(-180.0f + sky_x_offset, 0.f, 1.f, 0.f);

  if (!STRETCHSKY)
  {
    int texh = rtmain.sky.texture->height;

    if (texh <= 180)
    {
      //glScalef(1.0f, (float)texh / 230.0f, 1.0f);
    }
    else
    {
      if (texh > 190)
      {
        //glScalef(1.0f, 230.0f / 240.0f, 1.0f);
      }
    }
  }


  int index_count = 0;
  for (int i = 0; i < vbo->loopcount; i++)
  {
    RTSkyLoopDef *loop = &vbo->loops[i];
    index_count += 3 * GetTriangleCount(loop->mode, loop->vertexcount);
  }


  uint32_t *const p_indices = malloc(index_count * sizeof(uint32_t));
  int index_iter = 0;


  for (int i = 0; i < vbo->loopcount; i++)
  {
    RTSkyLoopDef *loop = &vbo->loops[i];

    if (!loop->use_texture)
    {
      continue;
    }

    switch (loop->mode)
    {
      case SKY_TRIANGLE_STRIP:
      {
        for (int t = 0; t < GetTriangleCount(loop->mode, loop->vertexcount); t++)
        {
          p_indices[index_iter] = loop->vertexindex + t;               index_iter++;
          p_indices[index_iter] = loop->vertexindex + t + (1 + t % 2); index_iter++;
          p_indices[index_iter] = loop->vertexindex + t + (2 - t % 2); index_iter++;
        }
        break;
      }
      case SKY_TRIANGLE_FAN:
      {
        for (int t = 0; t < GetTriangleCount(loop->mode, loop->vertexcount); t++)
        {
          p_indices[index_iter] = loop->vertexindex + t + 1;           index_iter++;
          p_indices[index_iter] = loop->vertexindex + t + 2;           index_iter++;
          p_indices[index_iter] = loop->vertexindex + 0;               index_iter++;
        }
        break;
      }
      default: assert(0); break;
    }
  }


  RgRasterizedGeometryUploadInfo info =
  {
    .renderType = RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY,
    .vertexCount = vbo->vertex_count,
    .pStructs = vbo->data,
    .indexCount = index_iter,
    .pIndexData = p_indices,
    .transform = RG_TRANSFORM_IDENTITY,
    .color = RG_COLOR_WHITE ,
    .material = rtmain.sky.texture->rg_handle,
    .blendEnable = false,
    .depthTest = false,
    .depthWrite = false
  };

  RgResult r = rgUploadRasterizedGeometry(rtmain.instance, &info, NULL, NULL);
  RG_CHECK(r);


  free(vbo->loops);
  free(vbo->data);
  free(p_indices);
}