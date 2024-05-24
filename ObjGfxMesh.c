/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal object meshes
 *  Copyright (C) 2007 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* ANSI headers */
#include "stdlib.h"
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <math.h>
#ifndef NDEBUG
#include <time.h>
#endif

/* My headers */
#include "msgtrans.h"
#include "macros.h"
#include "err.h"
#include "NoBudge.h"
#include "TrigTable.h"
#include "OSVDU.h"
#include "SFFormats.h"

#include "utils.h"
#include "plot.h"
#include "Debug.h"
#include "ObjGfxMesh.h"
#include "ObjGfxMeshData.h"
#include "SFInit.h"
#include "PolyCol.h"
#include "MapCoord.h"
#include "DFileData.h"
#include "ObjVertex.h"
#include "ObjPolygon.h"
#include "Hill.h"
#include "Obj.h"

typedef enum
{
  CoordinateScale_Small,
  CoordinateScale_Medium,
  CoordinateScale_Large
}
CoordinateScale;

enum {
  DIV_TABLE_SIZE = 16384,
  PEX_SCALE = 2,
  PEX_SHIFT = 4,
  UNIT_VECTOR = 2048,
  BytesPerCollisionBox = 28,
  BytesPerExplosion = 36,
  PlotCommands_OperandMask = 0x1fu,
  PlotCommands_OperandShift = 0,
  PlotCommands_ActionMask = 0xe0u,
  PlotCommands_ActionShift = 5,
  PlotCommands_EndOfType = 255, /* Followed by data for next plot type,
                                       or PlotCommands_EndOfData */
  PlotCommands_EndOfData = 254, /* Indicates no more plot types */
  ObjectCollisionSize_YMask = 0x0fu,
  ObjectCollisionSize_XMask = 0xf0u,
  ObjectCollisionSize_YShift = 0,
  ObjectCollisionSize_XShift = 4,
  Object_PlotTypeMask = 0x0fu,
  Object_PlotTypeShift = 0,
  Object_LastGroupMask = 0xf0u,
  Object_LastGroupShift = 4,
  Objects_EndOfData = 99,
  OAllocGrowth = 2,
  OAllocInit = 8,
  ScoreMultiplier = 25,
  HillSizeLog2 = 4,
  /* The map scaler (in units of 1/131072) is calculated so that the finest
     resolution object coordinates are preserved. We scale the base vector
     magnitude to the smallest object size (>> CoordinateScale_Large) and
     minimum coordinate change (RelCoord_AddDiv16, i.e. >> 4). Currently,
     the overall effect is division of polygon coordinates by 32. */
  FixedMapDivisor = UNIT_VECTOR >> (CoordinateScale_Large + 4),
  ToScreenDivisor = 1 << (15 + PEX_SCALE),
  FixedMapScaler = ToScreenDivisor / FixedMapDivisor,
};

/* The graphics data follows immediately after the explosions data
   (address should already be word aligned) */

typedef enum
{
  ObjectType_Ground, /* Ground objects (trees, buildings) */
  ObjectType_Bit,    /* (Broken?) bits */
  ObjectType_Ship  /* Aerial things (fighters, coins, missiles) */
}
ObjectType;

typedef struct {
  CoordinateScale scale;
  unsigned char coll_x, coll_y;
  unsigned char plot_type;
  unsigned short clip_size_x, clip_size_y;
  unsigned short score;
  long int clip_dist;
  MapArea bounding_box[MapAngle_Count];
  long int pal_dist;
} ObjMisc;

struct ObjGfxMesh {
  ObjMisc misc;
  ObjVertices varray;
  ObjPolygons polygons;
};

static void obj_array_init(ObjGfxMeshArray *const array)
{
  assert(array);
  *array = (ObjGfxMeshArray){.ocount = 0, .oalloc = 0, .objects = NULL};
}

static void obj_array_free(ObjGfxMeshArray *const array)
{
  assert(array);
  assert(array->ocount <= array->oalloc);

  for (size_t n = 0; n < array->ocount; ++n)
  {
    ObjGfxMesh *const obj = array->objects[n];
    obj_vertices_free(&obj->varray);
    obj_polygons_free(&obj->polygons);
    free(obj);
  }

  free(array->objects);
}

static ObjGfxMesh *obj_array_get(ObjGfxMeshArray const *const array, ObjRef const obj_ref)
{
  assert(array);
  assert(array->ocount >= 0);
  assert(array->ocount <= array->oalloc);
  size_t const n = objects_ref_to_num(obj_ref);
  assert(n < array->ocount);
  assert(array->objects);
  return array->objects[n];
}

static ObjGfxMesh *obj_array_add(ObjGfxMeshArray *const array,
  CoordinateScale const scale,
  unsigned char const plot_type,
  unsigned long const clip_size_x,
  unsigned long const clip_size_y)
{
  assert(array);
  assert(array->ocount <= array->oalloc);

  if ((array->ocount + 1) > array->oalloc)
  {
    size_t const new_size = array->oalloc ? (array->oalloc * OAllocGrowth) : OAllocInit;
    assert(new_size > 0);
    ObjGfxMesh **const objects = realloc(array->objects, sizeof(*objects) * new_size);
    if (!objects)
    {
      return NULL;
    }
    array->oalloc = new_size;
    array->objects = objects;
  }

  ObjGfxMesh *const new_obj = malloc(sizeof(*new_obj));
  if (!new_obj)
  {
    return NULL;
  }

  new_obj->misc = (ObjMisc){
    .scale = scale,
    .plot_type = plot_type,
    .clip_size_x = clip_size_x,
    .clip_size_y = clip_size_y,
    .pal_dist = -1,
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle) {
    new_obj->misc.bounding_box[angle] = MapArea_make_invalid();
  }

  obj_vertices_init(&new_obj->varray);
  obj_polygons_init(&new_obj->polygons);

  array->objects[array->ocount++] = new_obj;

  return new_obj;
}

static TrigTable *trig_table;
static long int divide_table[DIV_TABLE_SIZE];

/* ---------------- Private functions ---------------- */

static inline int ObjGfxAngle_get(ObjGfxAngle const rot)
{
  return rot.v;
}

static inline double ObjGfxAngle_get_deg(ObjGfxAngle const rot)
{
  return ((double)rot.v * 360) / (OBJGFXMESH_ANGLE_QUART * 4);
}

static void rotate(ObjGfxMeshesView const *const ctx, Vertex3D *const vector)
{
  assert(ctx);
  assert(vector);
  DEBUG("Input vector: %ld,%ld,%ld", vector->x, vector->y, vector->z);

  DEBUG("Rotation angles: %f,%f,%f",
        ObjGfxAngle_get_deg(ctx->direction.x_rot),
        ObjGfxAngle_get_deg(ctx->direction.y_rot),
        ObjGfxAngle_get_deg(ctx->direction.z_rot));

  /* Apply z rotation */
  int const z_rot = ObjGfxAngle_get(ctx->direction.z_rot);
  int cosine = TrigTable_look_up_cosine(trig_table, z_rot);
  int sine = TrigTable_look_up_sine(trig_table, z_rot);
  long int new_x = (vector->x * cosine) / SINE_TABLE_SCALE -
                   (vector->z * sine) / SINE_TABLE_SCALE;
  long int new_z = (vector->x * sine) / SINE_TABLE_SCALE +
                   (vector->z * cosine) / SINE_TABLE_SCALE;
  vector->z = new_z;
  vector->x = new_x;

  /* Apply x rotation */
  int const x_rot = ObjGfxAngle_get(ctx->direction.x_rot);
  cosine = TrigTable_look_up_cosine(trig_table, x_rot);
  sine = TrigTable_look_up_sine(trig_table, x_rot);
  new_x = (vector->x * cosine) / SINE_TABLE_SCALE -
          (vector->y * sine) / SINE_TABLE_SCALE;
  long int new_y = (vector->x * sine) / SINE_TABLE_SCALE +
                   (vector->y * cosine) / SINE_TABLE_SCALE;
  vector->y = new_y;
  vector->x = new_x;

  /* Apply y rotation */
  int const y_rot = ObjGfxAngle_get(ctx->direction.y_rot);
  cosine = TrigTable_look_up_cosine(trig_table, y_rot);
  sine = TrigTable_look_up_sine(trig_table, y_rot);
  new_y = (vector->y * cosine) / SINE_TABLE_SCALE -
          (vector->z * sine) / SINE_TABLE_SCALE;
  new_z = (vector->y * sine) / SINE_TABLE_SCALE +
          (vector->z * cosine) / SINE_TABLE_SCALE;
  vector->z = new_z;
  vector->y = new_y;
  DEBUG("Rotated vector: %ld,%ld,%ld", vector->x, vector->y, vector->z);
}

static bool vector_check(Vertex const *A, Vertex const *B, Vertex const *C)
{
  /* Calculate the z component of the normal vector of the plane that a
     polygon sits on. If this is negative then the normal vector of the plane
     (and the polygon itself) is facing away from the viewer. */
  DEBUG_VERBOSE("Vector check %d,%d %d,%d %d,%d", A->x, A->y, B->x, B->y, C->x, C->y);

  long int cross_z = ((long)B->x - A->x) * ((long)C->y - B->y) -
                     ((long)B->y - A->y) * ((long)C->x - B->x);

  DEBUG_VERBOSE("Polygon is %s-facing", cross_z > 0 ? "forward" : "back");
  return cross_z > 0;
}

static void to_screen_coords(size_t const num_vertices, int const map_scaler,
  Vertex3D const *const rot_vertices, Vertex *const screen_coords)
{
  Vertex3D const *read_ptr = rot_vertices;
  Vertex *write_ptr = screen_coords;

  for (size_t v = 0; v < num_vertices; v++)
  {
    if (map_scaler) {
      /* Force parallel projection by using a fixed divisor
         (ignoring the individual y coordinates) */
      write_ptr->x = (int)(read_ptr->x * map_scaler / ToScreenDivisor);
      write_ptr->y = (int)(read_ptr->z * map_scaler / ToScreenDivisor);
    } else {
      /* Because polygons are not clipped until after perspective division,
         this function often handles y coordinates that are behind the viewer */
      long int index = read_ptr->y >> (PEX_SHIFT + 2);
      if (index > 0) {
        if ((unsigned long)index >= ARRAY_SIZE(divide_table)) {
          /* Vertex is too far away */
          DEBUG("Vertex %zu is too far for perspective division", v + 1);
          write_ptr->x = 0;
          write_ptr->y = 0;
        } else {
          /* Do perspective division (actually multiplication by a fractional
             value in fixed-point format) */
          write_ptr->x = (int)(read_ptr->x * divide_table[index] / ToScreenDivisor);
          write_ptr->y = (int)(read_ptr->z * divide_table[index] / ToScreenDivisor);
        }
      } else {
        /* Vertex is too close, or behind the viewer */
        DEBUG("Vertex %zu is behind the viewer", v + 1);
        write_ptr->x = (int)read_ptr->x;
        write_ptr->y = (int)read_ptr->z;
      }
    }
    DEBUG_VERBOSE("Screen coordinates %d,%d", write_ptr->x, write_ptr->y);

    read_ptr++;
    write_ptr++;
  }
}

static inline Vertex translate_screen(Vertex const centre, Vertex const offset)
{
  /* Within the actual game, the y coordinates are naturally flipped
     during rasterisation (because the lowest frame buffer address is at the
     top of the screen). That is why this function subtracts from centre->y. */
  return (Vertex){centre.x + offset.x, centre.y - offset.y};
}

static void update_bbox(
  Vertex (*const polygon_coords)[ObjPolygonMaxSides],
  Vertex const centre,
  BBox *const bounding_box,
  size_t const num_sides)
{

  DEBUGF("Update bbox for %zu-sided polygon\n", num_sides);

  assert(num_sides >= 3);

  Vertex const first_corner = translate_screen(centre, (*polygon_coords)[0]);
  Vertex screen_pos = translate_screen(centre, (*polygon_coords)[1]);

  BBox_expand(bounding_box, first_corner);
  BBox_expand(bounding_box, screen_pos);

  for (size_t side = 2; side < num_sides; side++)
  {
    screen_pos = translate_screen(centre, (*polygon_coords)[side]);
    BBox_expand(bounding_box, screen_pos);
  } /* next side */
}

static void plot_filled(
  Vertex (*const polygon_coords)[ObjPolygonMaxSides],
  Vertex const centre,
  size_t const num_sides)
{
  DEBUGF("Plot %zu-sided polygon\n", num_sides);

  assert(num_sides >= 3);

  Vertex const first_corner = translate_screen(centre, (*polygon_coords)[0]);
  Vertex screen_pos = translate_screen(centre, (*polygon_coords)[1]);

  plot_move(screen_pos);

  for (size_t side = 2; side < num_sides; side++)
  {
    screen_pos = translate_screen(centre, (*polygon_coords)[side]);

    plot_move(first_corner);
    plot_fg_tri(screen_pos);
  } /* next side */
}

static void plot_wireframe(
  Vertex (*const polygon_coords)[ObjPolygonMaxSides],
  Vertex const centre,
  size_t const num_sides)
{
  DEBUG("Plot pending %zu-sided polygon", num_sides);
  assert(num_sides >= 3);

  Vertex const first_corner = translate_screen(centre, (*polygon_coords)[0]);
  Vertex screen_pos = translate_screen(centre, (*polygon_coords)[1]);

  plot_move(first_corner);
  plot_fg_line_ex_end(screen_pos);

  for (size_t side = 2; side < num_sides; side++)
  {
    screen_pos = translate_screen(centre, (*polygon_coords)[side]);
    plot_fg_line_ex_end(screen_pos);
  } /* next side */

  plot_fg_line_ex_end(first_corner);
}

static void plot_group(
  Vertex const centre,
  PolyColData const *const colours,
  BBox *const bounding_box,
  ObjGroup *const group, bool const plot_all,
  PaletteEntry const (*const pal)[NumColours], ObjGfxMeshStyle const style,
  Vertex (*const screen_coords)[ObjVertexMax])
{
  size_t const pcount = obj_group_get_polygon_count(group);
  DEBUGF("Plotting %zu polygons\n", pcount);

  for (size_t p = 0; p < pcount; ++p)
  {
    ObjPolygon const polygon = obj_group_get_polygon(group, p);
    size_t const num_sides = obj_polygon_get_side_count(&polygon);

    Vertex polygon_coords[ObjPolygonMaxSides];
    size_t side = 0;

    /* Get first three coordinates and test for back-facing polygon */
    assert(side < num_sides);
    size_t vertex = obj_polygon_get_side(&polygon, side);
    assert(vertex < ObjVertexMax);
    polygon_coords[side++] = (*screen_coords)[vertex];

    assert(side < num_sides);
    vertex = obj_polygon_get_side(&polygon, side);
    assert(vertex < ObjVertexMax);
    polygon_coords[side++] = (*screen_coords)[vertex];

    assert(side < num_sides);
    vertex = obj_polygon_get_side(&polygon, side);
    assert(vertex < ObjVertexMax);
    polygon_coords[side++] = (*screen_coords)[vertex];

    if (!plot_all &&
        !vector_check(polygon_coords, polygon_coords + 1, polygon_coords + 2))
    {
      DEBUGF("Cull back-facing polygon %zu\n", p);
      continue;
    }

    /* Get the rest of the coordinates for this facet */
    while (side < num_sides)
    {
      vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < ObjVertexMax);
      polygon_coords[side++] = (*screen_coords)[vertex];
    }

    /* Finally, we get to plot the polygon on the screen! */
    switch (style) {
    case ObjGfxMeshStyle_Wireframe:
      plot_wireframe(&polygon_coords, centre, num_sides);
      break;

    case ObjGfxMeshStyle_Filled:
      if (pal) {
        size_t const colour = obj_polygon_get_colour(&polygon);
        size_t const pindex = polycol_get_colour(colours, colour);
        assert(pindex < ARRAY_SIZE(*pal));
        plot_set_col((*pal)[pindex]);
      }
      plot_filled(&polygon_coords, centre, num_sides);
      break;

    case ObjGfxMeshStyle_BBox:
      update_bbox(&polygon_coords, centre, bounding_box, num_sides);
      break;
    }
  }
}

static SFError parse_objects(ObjGfxMeshes *const meshes, Reader * const reader)
{
  int32_t last_explosion_num;
  if (!reader_fread_int32(&last_explosion_num, reader)) {
    DEBUGF("Failed to read no. of explosions\n");
    return SFERROR(ReadFail);
  }

  /* Parse each object definition in turn until finding an end marker.
     There must be at least one. */
  size_t object_count = 0;
  do {
    long int expl_size = 36l * (last_explosion_num + 1l);

    DEBUGF("Found %"PRId32" explosion lines (%ld bytes) "
           "at offset %ld (0x%lx)\n", last_explosion_num + 1, expl_size,
           reader_ftell(reader), reader_ftell(reader));

    /* Skip the explosions data */
    if (reader_fseek(reader, expl_size, SEEK_CUR)) {
      DEBUGF("Failed to seek object attributes (object %zu)\n",
              object_count);
      return SFERROR(BadSeek);
    }

    /* Get object type */
    int byte = reader_fgetc(reader);
    if (byte == EOF)
    {
      DEBUGF("Failed to read object type (object %zu)\n",
              object_count);
      return SFERROR(ReadFail);
    }

    if ((byte != ObjectType_Ship) &&
        (byte != ObjectType_Ground) &&
        (byte != ObjectType_Bit))
    {
      DEBUGF("Bad object type %d (object %zu)\n", byte,
              object_count);
      return SFERROR(BadObjectClass);
    }
    ObjectType const type = (ObjectType)byte;

    DEBUGF("Found object %zu of type %d at offset %ld (0x%lx)\n",
           object_count, (int)type, reader_ftell(reader)-1, reader_ftell(reader)-1);

    byte = reader_fgetc(reader);
    if (byte == EOF)
    {
      DEBUGF("Failed to read scale (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }
    CoordinateScale const scale = (CoordinateScale)byte;

    int const rot = reader_fgetc(reader);
    if (rot == EOF)
    {
      DEBUGF("Failed to read rotator (object %zu)\n",
              object_count);
      return SFERROR(ReadFail);
    }

    const int gr_obj_coll_size = reader_fgetc(reader);
    if (gr_obj_coll_size == EOF)
    {
      DEBUGF("Failed to read packed collision size (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }

    unsigned char const coll_x = (gr_obj_coll_size & ObjectCollisionSize_XMask) >>
                                 ObjectCollisionSize_XShift;
    unsigned char const coll_y = (gr_obj_coll_size & ObjectCollisionSize_YMask) >>
                                 ObjectCollisionSize_YShift;

    uint16_t clip_size_x, clip_size_y;
    if (!reader_fread_uint16(&clip_size_x, reader) ||
        !reader_fread_uint16(&clip_size_y, reader))
    {
      DEBUGF("Failed to read clip size (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }

    int const score = reader_fgetc(reader);
    if (score == EOF)
    {
      DEBUGF("Failed to read score (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }

    int const hits_or_min_z = reader_fgetc(reader);
    if (hits_or_min_z == EOF)
    {
      DEBUGF("Failed to read hitpoints (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }

    int const explosion_style = reader_fgetc(reader);
    if (explosion_style == EOF)
    {
      DEBUGF("Failed to read explosion style (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }

    const int plot_type_and_last_group = reader_fgetc(reader);
    if (plot_type_and_last_group == EOF)
    {
      DEBUGF("Failed to read plot type and max plot group (object %zu)\n",
             object_count);
      return SFERROR(ReadFail);
    }

    unsigned char const plot_type =
      (plot_type_and_last_group & Object_PlotTypeMask) >> Object_PlotTypeShift;

    if (plot_type >= meshes->num_plot_types + 1)
    {
      DEBUGF("Bad plot type %d (object %zu)\n", plot_type, object_count);
      return SFERROR(BadPlotType);
    }

    unsigned char const expected_max_group =
      (plot_type_and_last_group & Object_LastGroupMask) >> Object_LastGroupShift;

    if ((expected_max_group < 0) ||
        (expected_max_group >= ObjPolygonFacingCheckGroup))
    {
      DEBUGF("Bad highest plot group %d (object %zu)\n",
              expected_max_group, object_count);
      return SFERROR(BadNumGroups);
    }

    if ((expected_max_group > 0) && (plot_type == 0))
    {
      DEBUGF("Highest plot group %d is higher than "
             "expected for plot type 0 (object %zu)\n",
             expected_max_group, object_count);
      return SFERROR(BadNumGroups);
    }

    ObjGfxMeshArray *array = NULL;
    switch (type)
    {
      case ObjectType_Ground:
        array = &meshes->ground;
        break;
      case ObjectType_Ship:
        array = &meshes->ships;
        break;
      case ObjectType_Bit:
        break;
    }

    ObjGfxMesh *obj = NULL;
    if (array)
    {
      obj = obj_array_add(array, scale, plot_type, clip_size_x, clip_size_y);
      if (!obj)
      {
        return SFERROR(NoMem);
      }
    }

    size_t vcount;
    SFError err = obj_vertices_read(obj ? &obj->varray : NULL, reader, &vcount);
    if (SFError_fail(err))
    {
      return err;
    }

    if ((size_t)rot >= vcount)
    {
      DEBUGF("Bad rotator %d >= %zu (object %zu)\n", rot, vcount, object_count);
      return SFERROR(BadRotator);
    }

    /* Find the first word-aligned offset ahead of the vertex data */
    if (reader_fseek(reader, WORD_ALIGN(reader_ftell(reader)), SEEK_SET))
    {
      DEBUGF("Failed to seek clip distance (object %zu)\n", object_count);
      return SFERROR(BadSeek);
    }

    int32_t clip_dist;
    if (!reader_fread_int32(&clip_dist, reader))
    {
      DEBUGF("Failed to read clip distance (object %zu)\n", object_count);
      return SFERROR(ReadFail);
    }

    size_t max_group;
    err = obj_polygons_read(obj ? &obj->polygons : NULL, reader, vcount, &max_group);
    if (SFError_fail(err))
    {
      return err;
    }

    /* Can't require and exact match because of a mesh in Graphics.Earth2 */
    if (max_group > expected_max_group)
    {
      DEBUGF("Bad plot group %zu > %d\n", max_group, expected_max_group);
      return SFERROR(BadPolygonGroup);
    }

    /* Validate the object's plot type. */
    if (plot_type != 0)
    {
      /* Check that the referenced polygons exist */
      const size_t max_polygon = meshes->plot_types[plot_type - 1].max_polygon;
      ObjGroup *const vec = obj_polygons_get_group(&obj->polygons, ObjPolygonFacingCheckGroup);
      DEBUGF("Plot type %d requires polygon %zu, count is %zu\n",
        plot_type, max_polygon, obj_group_get_polygon_count(vec));

      if (max_polygon > obj_group_get_polygon_count(vec))
      {
        DEBUGF("Plot type %d is predicated on undefined polygon %zu "
               "(object %zu)\n", plot_type, max_polygon, object_count);
        //return SFERROR(BadPlotPolygon);
      }
    }

    if (obj)
    {
      if (type == ObjectType_Ground || type == ObjectType_Ship)
      {
        obj->misc.score = score * ScoreMultiplier;
      }
      obj->misc.clip_dist = clip_dist;

      if (type == ObjectType_Ground)
      {
        obj->misc.coll_x = coll_x;
        obj->misc.coll_y = coll_y;
      }
    }

    /* Find the first word-aligned offset ahead of the polygons data */
    if (reader_fseek(reader, WORD_ALIGN(reader_ftell(reader)), SEEK_SET))
    {
      return SFERROR(BadSeek);
    }

    DEBUGF("Collision is defined at offset %ld (0x%lx)\n",
           reader_ftell(reader), reader_ftell(reader));

    int32_t last_collision_num;
    if (!reader_fread_int32(&last_collision_num, reader))
    {
      return SFERROR(ReadFail);
    }

    long int coll_size = 28l * (last_collision_num + 1l);

    DEBUGF("Found %" PRId32 " collision boxes (%ld bytes) "
           "at offset %ld (0x%lx)\n", last_collision_num + 1, coll_size,
           reader_ftell(reader) + 8, reader_ftell(reader) + 8);

    /* Skip the collision boxes */
    if (reader_fseek(reader, 8 + coll_size + 4, SEEK_CUR))
    {
      return SFERROR(BadSeek);
    }

    if (!reader_fread_int32(&last_explosion_num, reader))
    {
      return SFERROR(ReadFail);
    }

    ++object_count;
  } while (last_explosion_num != Objects_EndOfData);

  DEBUGF("Found file terminator at %ld\n",
         reader_ftell(reader) - (long)sizeof(int32_t));

  return SFERROR(OK);
}

static SFError parse_plot_types(ObjGfxMeshes *const meshes,
                                Reader * const reader)
{
  assert(reader != NULL);
  assert(!reader_ferror(reader));
  assert(meshes != NULL);

  /* Read plot type definitions */
  int command = reader_fgetc(reader);
  if (command == EOF) {
    DEBUGF("Failed to read plot type definition\n");
    return SFERROR(ReadFail);
  }

  /* Parse each plot type definition in turn until finding an end marker.
     There must be at least one. */
  int plot_type_count = 0;
  do {
    int command_count = 0;

    if (plot_type_count >= MaxPlotType) {
      DEBUGF("Too many plot types (max %d)\n", MaxPlotType);
      return SFERROR(TooManyPlotTypes);
    }

    DEBUGF("Plot type %d is defined at offset %ld (0x%lx)\n",
           plot_type_count, reader_ftell(reader)-1, reader_ftell(reader)-1);

    PlotType *const pt = &meshes->plot_types[plot_type_count];
    *pt = (PlotType){.max_polygon = 0, .group_mask = 0};

    /* Parse each plot command in turn until finding an end marker.
       There must be at least one. */
    do {
      if (command_count >= MaxPlotCommands)
      {
        DEBUGF("Too many commands (max %d) for plot type %d\n",
                MaxPlotCommands, plot_type_count);
        return SFERROR(TooManyPlotComs);
      }

      int const operand = (command & PlotCommands_OperandMask) >> PlotCommands_OperandShift;
      PlotAction const action = (PlotAction)
          ((command & PlotCommands_ActionMask) >> PlotCommands_ActionShift);
      int group = operand, polygon = 0;

      if (action != PlotAction_FacingAlways)
      {
        polygon = operand;
        if (polygon > pt->max_polygon)
        {
          pt->max_polygon = polygon;
        }

        /* Next byte is a group number */
        group = reader_fgetc(reader);
        if (group == EOF) {
          DEBUGF("Failed to read plot group (command %d of plot type %d)\n",
                 command_count, plot_type_count);
          return SFERROR(ReadFail);
        }
      }

      if ((group < 0) || (group >= ObjPolygonFacingCheckGroup))
      {
        DEBUGF("Bad plot group %d (command %d of plot type %d)\n",
                group, command_count, plot_type_count);
        return SFERROR(BadPlotGroup);
      }

      pt->group_mask |= 1u << group;

      switch (action)
      {
         case PlotAction_FacingAlways:
           DEBUGF("Plot front-facing polygons in group %d\n",
                  group);
           break;
         case PlotAction_FacingIf:
           DEBUGF("Plot front-facing polygons in group %d "
                  "if polygon %d in group 7 is front-facing\n",
                  group, polygon);
           break;
         case PlotAction_FacingIfNot:
           DEBUGF("Plot front-facing polygons in group %d "
                  "if polygon %d in group 7 is back-facing\n",
                  group, polygon);
           break;
         case PlotAction_AllIf:
           DEBUGF("Plot group %d if polygon %d in group 7 is "
                  "front-facing\n", group, polygon);
           break;
         case PlotAction_AllIfNot:
           DEBUGF("Plot group %d if polygon %d in group 7 is "
                  "back-facing\n", group, polygon);
           break;
         default:
           DEBUGF("Bad plot action %d "
                   "(command %d of plot type %d)\n",
                   action, command_count, plot_type_count);
           return SFERROR(BadPlotAction);
      }

      pt->commands[command_count++] = (PlotCommand){
        .action = action, .group = group, .polygon = polygon};

      command = reader_fgetc(reader);
      if (command == EOF) {
        DEBUGF("Failed to read command or terminator (plot type %d)\n",
               plot_type_count);
        return SFERROR(ReadFail);
      }
    } while (command != PlotCommands_EndOfType);

    pt->num_commands = command_count;

    command = reader_fgetc(reader);
    if (command == EOF) {
      DEBUGF("Failed to read plot type definition or terminator\n");
      return SFERROR(ReadFail);
    }
    ++plot_type_count;
  } while (command != PlotCommands_EndOfData);

  meshes->num_plot_types = plot_type_count;
  return SFERROR(OK);
}

static SFError read_inner(ObjGfxMeshes *const meshes, Reader * const reader)
{
  SFError err = parse_plot_types(meshes, reader);
  if (SFError_fail(err))
  {
    return err;
  }

  /* Find the first word-aligned offset at least 4 bytes ahead of the
     plot type definitions terminator */
  if (reader_fseek(reader, WORD_ALIGN(reader_ftell(reader)+3), SEEK_SET))
  {
    DEBUGF("Failed to seek first object\n");
    return SFERROR(BadSeek);
  }

  return parse_objects(meshes, reader);
}

/* ---------------- Public functions ---------------- */

void ObjGfxMeshes_init(ObjGfxMeshes *const meshes)
{
  assert(meshes);
  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle) {
    meshes->max_bounding_box[angle] = MapArea_make_invalid();
  }
  meshes->num_plot_types = 0;
  obj_array_init(&meshes->ground);
  obj_array_init(&meshes->ships);
}

void ObjGfxMeshes_free(ObjGfxMeshes *const meshes)
{
  assert(meshes);
  obj_array_free(&meshes->ground);
  obj_array_free(&meshes->ships);
}

SFError ObjGfxMeshes_read(ObjGfxMeshes *const meshes, Reader *const reader)
{
  ObjGfxMeshes_free(meshes);
  ObjGfxMeshes_init(meshes);

  return check_trunc_or_ext(reader, read_inner(meshes, reader));
}

size_t ObjGfxMeshes_get_ground_count(const ObjGfxMeshes *const meshes)
{
  assert(meshes);
  assert(meshes->ground.ocount >= 0);
  assert(meshes->ground.ocount <= meshes->ground.oalloc);
  return meshes->ground.ocount;
}

size_t ObjGfxMeshes_get_ships_count(const ObjGfxMeshes *const meshes)
{
  assert(meshes);
  assert(meshes->ships.ocount >= 0);
  assert(meshes->ships.ocount <= meshes->ships.oalloc);
  return meshes->ships.ocount;
}

static MapArea plot_area_to_map_area(BBox const *const bounding_box)
{
  /* Convert the plot bounding box returned by ObjGfxMeshes_plot() to map
     coordinates (0, MAP_COORDS_LIMIT - 1). Experimentation suggests that
     the ratio of polygon coordinates to map coordinates is 4:UNIT_VECTOR
     (i.e. 1:512). The map scaler divided the polygon coordinates by 2 to
     the power of (UNIT_VECTOR >> CoordinateScale_Large + 4), so we must
     now multiply by the same amount as well as UNIT_VECTOR/4. */
  assert(bounding_box);

  DEBUG("Plot bounding box: %d,%d,%d,%d", bounding_box->xmin,
        bounding_box->ymin, bounding_box->xmax, bounding_box->ymax);

  MapArea const map_area = {
    .min.x = (MapCoord)bounding_box->xmin * (UNIT_VECTOR / 4) * FixedMapDivisor,
    .min.y = (MapCoord)bounding_box->ymin * (UNIT_VECTOR / 4) * FixedMapDivisor,
    .max.x = (MapCoord)bounding_box->xmax * (UNIT_VECTOR / 4) * FixedMapDivisor,
    .max.y = (MapCoord)bounding_box->ymax * (UNIT_VECTOR / 4) * FixedMapDivisor,
  };
  DEBUG("In map coordinates: %" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord ",%" PRIMapCoord,
        map_area.min.x, map_area.min.y, map_area.max.x, map_area.max.y);
  return map_area;
}

static void calc_ground_bboxes(ObjGfxMeshes *const meshes, MapAngle const angle)
{
  assert(meshes);

  // To pre-calculate bounding boxes for map
  ObjGfxMeshesView map_ctx;
  ObjGfxMeshes_set_direction(&map_ctx,
    (ObjGfxDirection){ObjGfxAngle_from_map(angle), {-OBJGFXMESH_ANGLE_QUART}, {0}},
    FixedMapScaler);

  meshes->max_bounding_box[angle] = MapArea_make_invalid();

  for (size_t n = 0; n < meshes->ground.ocount; ++n)
  {
    ObjRef const obj_ref = objects_ref_from_num(n);
    ObjGfxMesh *const obj = obj_array_get(&meshes->ground, obj_ref);

    /* Pre-calculate and store the bounding box for this object */
    BBox bounding_box;
    ObjGfxMeshes_plot(meshes, &map_ctx, NULL, obj_ref,
                  (Vertex){0,0}, 65536*4, (Vertex3D){0, 0, 0},
                  palette, &bounding_box, ObjGfxMeshStyle_BBox);

    obj->misc.bounding_box[angle] = plot_area_to_map_area(&bounding_box);
    MapArea_expand_for_area(&meshes->max_bounding_box[angle], &obj->misc.bounding_box[angle]);
  }
}

MapArea ObjGfxMeshes_get_hill_bbox(HillType const hill_type,
  unsigned char (*const heights)[HillCorner_Count], MapAngle const angle)
{
  ObjGfxMeshesView map_ctx;
  ObjGfxMeshes_set_direction(&map_ctx,
    (ObjGfxDirection){ObjGfxAngle_from_map(angle), {-OBJGFXMESH_ANGLE_QUART}, {0}},
    FixedMapScaler);

  BBox bounding_box;
  ObjGfxMeshes_plot_poly_hill(&map_ctx, NULL, hill_type, NULL, heights,
    (Vertex){0,0}, 65536*4, (Vertex3D){0, 0, 0},
    NULL, &bounding_box, ObjGfxMeshStyle_BBox);

  return plot_area_to_map_area(&bounding_box);
}

MapArea ObjGfxMeshes_get_ground_bbox(ObjGfxMeshes *const meshes, ObjRef const obj_ref,
  MapAngle const angle)
{
  assert(meshes);
  ObjGfxMesh *const obj = obj_array_get(&meshes->ground, obj_ref);

  if (!MapArea_is_valid(&obj->misc.bounding_box[angle]))
  {
    calc_ground_bboxes(meshes, angle);
    assert(MapArea_is_valid(&obj->misc.bounding_box[angle]));
  }

  DEBUG("Object type %zu covers area %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord,
        objects_ref_to_num(obj_ref), obj->misc.bounding_box[angle].min.x,
        obj->misc.bounding_box[angle].min.y,
        obj->misc.bounding_box[angle].max.x, obj->misc.bounding_box[angle].max.y);

  return obj->misc.bounding_box[angle];
}

MapArea ObjGfxMeshes_get_max_ground_bbox(ObjGfxMeshes *const meshes, MapAngle const angle)
{
  assert(meshes);

  if (!MapArea_is_valid(&meshes->max_bounding_box[angle]))
  {
    calc_ground_bboxes(meshes, angle);
    assert(MapArea_is_valid(&meshes->max_bounding_box[angle]));
  }

  DEBUG("Largest object covers area %" PRIMapCoord ",%" PRIMapCoord
        ",%" PRIMapCoord ",%" PRIMapCoord,
        meshes->max_bounding_box[angle].min.x, meshes->max_bounding_box[angle].min.y,
        meshes->max_bounding_box[angle].max.x, meshes->max_bounding_box[angle].max.y);

  return meshes->max_bounding_box[angle];
}

void ObjGfxMeshes_global_init(void)
{
  long int divisor = -45;
  for (int v = 0; v < DIV_TABLE_SIZE; v++) {
    divide_table[v] = (2048 * 1024 * 128 << PEX_SCALE) / divisor;
    divisor += 12 * 4 << PEX_SHIFT;
  }

  trig_table = TrigTable_make(SINE_TABLE_SCALE, OBJGFXMESH_ANGLE_QUART);
  if (trig_table == NULL)
    err_complain_fatal(DUMMY_ERRNO, msgs_lookup("NoMem"));
}

TrigTable const *ObjGfxMeshes_get_trig_table(void)
{
  return trig_table;
}

static void plot_lines(ObjGfxMeshesView const *const ctx, Vertex const centre,
                       long int const distance, Vertex3D const pos,
                       ObjVertex const vertices[], size_t const n)
{
  assert(vertices);

  Vertex3D obj_pos = pos;
  rotate(ctx, &obj_pos);
  obj_pos.y += distance;

  static Vertex3D rot_vertices[ObjVertexMax];
  for (size_t i = 0; i < n; ++i)
  {
    obj_vertices_add_scaled_unit(&obj_pos, &ctx->rotated, vertices[i]);
    rot_vertices[i] = obj_pos;
  }

  static Vertex screen_coords[ObjVertexMax];
  to_screen_coords(n, ctx->map_scaler, rot_vertices, screen_coords);

  for (size_t k = 0; k + 1 < n; k += 2)
  {
    plot_move(translate_screen(centre, screen_coords[k]));
    plot_fg_line(translate_screen(centre, screen_coords[k + 1]));
  }
}

void ObjGfxMeshes_plot_mask(ObjGfxMeshesView const *const ctx,
  Vertex const centre, long int const distance, Vertex3D const pos)
{
  static ObjVertex const hatch[] = {
    {RelCoord_AddMul4, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_SubMul8, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_AddUnit, RelCoord_Zero},
    {RelCoord_AddMul8, RelCoord_Zero,  RelCoord_Zero},
    {RelCoord_Zero, RelCoord_AddUnit,  RelCoord_Zero},
    {RelCoord_SubMul8, RelCoord_Zero,  RelCoord_Zero},
    {RelCoord_Zero, RelCoord_AddUnit,  RelCoord_Zero},
    {RelCoord_AddMul8, RelCoord_Zero,  RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul4, RelCoord_Zero},
    {RelCoord_SubMul8, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubUnit,  RelCoord_Zero},
    {RelCoord_AddMul8, RelCoord_Zero,  RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubUnit,  RelCoord_Zero},
    {RelCoord_SubMul8, RelCoord_Zero,  RelCoord_Zero},
  };
  plot_lines(ctx, centre, distance, pos, hatch, ARRAY_SIZE(hatch));
}

void ObjGfxMeshes_plot_grid(ObjGfxMeshesView const *const ctx,
  Vertex const centre, long int const distance, Vertex3D const pos)
{
  static ObjVertex const grid[] = {
    /* vertical */
    {RelCoord_AddMul4, RelCoord_AddMul16, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul32, RelCoord_Zero},
    {RelCoord_AddMul8, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_AddMul32, RelCoord_Zero},
    {RelCoord_SubMul16, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul32, RelCoord_Zero},
    {RelCoord_SubMul8, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_AddMul32, RelCoord_Zero},
    /* horizontal */
    {RelCoord_SubMul4, RelCoord_SubMul4, RelCoord_Zero},
    {RelCoord_AddMul32, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul8, RelCoord_Zero},
    {RelCoord_SubMul32, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul8, RelCoord_Zero},
    {RelCoord_AddMul32, RelCoord_Zero, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul8, RelCoord_Zero},
    {RelCoord_SubMul32, RelCoord_Zero, RelCoord_Zero},
  };
  plot_lines(ctx, centre, distance, pos, grid, ARRAY_SIZE(grid));
}

void ObjGfxMeshes_plot_unknown(ObjGfxMeshesView const *const ctx,
  Vertex const centre, long int const distance, Vertex3D const pos)
{
  static ObjVertex const cross[] = {
    {RelCoord_AddMul2, RelCoord_AddMul2, RelCoord_Zero},
    {RelCoord_SubMul4, RelCoord_SubMul4, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_AddMul4, RelCoord_Zero},
    {RelCoord_AddMul4, RelCoord_SubMul4, RelCoord_Zero},
  };
  plot_lines(ctx, centre, distance, pos, cross, ARRAY_SIZE(cross));
}

void ObjGfxMeshes_plot_hill(ObjGfxMeshesView const *const ctx,
  Vertex const centre, long int const distance, Vertex3D const pos)
{
  static ObjVertex const cross[] = {
    {RelCoord_AddMul2, RelCoord_AddMul2, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul4, RelCoord_Zero},

    {RelCoord_Zero, RelCoord_AddMul2, RelCoord_Zero},
    {RelCoord_SubMul4, RelCoord_Zero, RelCoord_Zero},

    {RelCoord_Zero, RelCoord_AddMul2, RelCoord_Zero},
    {RelCoord_Zero, RelCoord_SubMul4, RelCoord_Zero},
  };
  plot_lines(ctx, centre, distance, pos, cross, ARRAY_SIZE(cross));
}

void ObjGfxMeshes_plot(ObjGfxMeshes const *const meshes,
                       ObjGfxMeshesView const *const ctx,
                       PolyColData const *const colours,
                       ObjRef const obj_ref,
                       Vertex const centre,
                       long int const distance, Vertex3D const pos,
                       PaletteEntry const (*const pal)[NumColours],
                       BBox *const bounding_box,
                       ObjGfxMeshStyle const style)
{
  assert(meshes);
  DEBUG("Request to plot object %zu at coords %d,%d (world coords %ld,%ld,%ld)",
        objects_ref_to_num(obj_ref), centre.x, centre.y, pos.x, pos.y, pos.z);

  static Vertex3D rot_vertices[ObjVertexMax];

  ObjGfxMesh *const obj = obj_array_get(&meshes->ground, obj_ref);

  assert(obj->misc.scale >= CoordinateScale_Small);
  assert(obj->misc.scale <= CoordinateScale_Large);
  int const div_log2 = (int)(CoordinateScale_Large - obj->misc.scale);

  UnitVectors scaled;
  obj_vertices_scale_unit(&scaled, &ctx->rotated, div_log2);

  Vertex3D obj_pos = pos;
  rotate(ctx, &obj_pos);
  obj_pos.y += distance;

  obj_vertices_to_coords(&obj->varray, &obj_pos, &scaled, &rot_vertices);

  size_t const num_vertices = obj_vertices_get_count(&obj->varray);
  static Vertex screen_coords[ObjVertexMax];
  to_screen_coords(num_vertices, ctx->map_scaler, rot_vertices, screen_coords);

  if (bounding_box != NULL)
  {
    bounding_box->xmin = INT_MAX;
    bounding_box->ymin = INT_MAX;
    bounding_box->xmax = INT_MIN;
    bounding_box->ymax = INT_MIN;
  }

  DEBUGF("Object distance %ld must be greater than clip distance %ld\n",
         distance, obj->misc.clip_dist);

  if (distance <= obj->misc.clip_dist)
  {
    DEBUG("Object too close");
    return;
  }

  DEBUG("Internal plot type is %d", obj->misc.plot_type);
  if (obj->misc.plot_type > 0)
  {
    /* Complex object (plot groups of facets according to a sequence of
       commands which do preliminary vector tests) */

    /* Precalculate whether all of the polygons in the special group referenced by
       plot commands are facing the camera or not. */
    bool vector_results[PlotCommands_OperandMask >> PlotCommands_OperandShift] = {false};
    ObjGroup *const group = obj_polygons_get_group(&obj->polygons, ObjPolygonFacingCheckGroup);
    size_t const pcount = LOWEST(obj_group_get_polygon_count(group), ARRAY_SIZE(vector_results));

    for (size_t p = 0; p < pcount; p++)
    {
      ObjPolygon const polygon = obj_group_get_polygon(group, p);
      Vertex polygon_coords[ObjPolygonMaxSides];
      size_t side = 0;

      /* Get first three coordinates and do facing test */
      size_t vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < num_vertices);
      polygon_coords[side++] = screen_coords[vertex];

      vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < num_vertices);
      polygon_coords[side++] = screen_coords[vertex];

      vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < num_vertices);
      polygon_coords[side++] = screen_coords[vertex];

      vector_results[p] = vector_check(polygon_coords, polygon_coords + 1, polygon_coords + 2);

      DEBUG("Facing check %zu is %s", p, vector_results[p] ? "true" : "false");
    }

    /* Plot the polygon groups in the order indicated by the sequence of
       commands associated with this object */
    assert(obj->misc.plot_type - 1 < meshes->num_plot_types);
    PlotType const *const pt = &meshes->plot_types[obj->misc.plot_type - 1];

    for (int c = 0; c < pt->num_commands; ++c)
    {
      bool plot_all = false, cull = false;
      PlotCommand const *const com = &pt->commands[c];

      switch (com->action)
      {
        case PlotAction_FacingAlways:
          DEBUGF("Always plot group %d\n", com->group);
          break;

        case PlotAction_FacingIf:
          DEBUGF("Plot group %d if polygon %d is facing\n", com->group, com->polygon);
          cull = !vector_results[com->polygon];
          break;

        case PlotAction_FacingIfNot:
          DEBUGF("Plot group %d if polygon %d is backfacing\n", com->group, com->polygon);
          cull = vector_results[com->polygon];
          break;

        case PlotAction_AllIf:
          DEBUGF("Plot all group %d if polygon %d is facing\n", com->group, com->polygon);
          cull = !vector_results[com->polygon];
          plot_all = true;
          break;

        case PlotAction_AllIfNot:
          DEBUGF("Plot all group %d if polygon %d is backfacing\n", com->group, com->polygon);
          cull = vector_results[com->polygon];
          plot_all = true;
          break;
      }

      if (!cull)
      {
        plot_group(centre, colours, bounding_box,
          obj_polygons_get_group(&obj->polygons, com->group), plot_all, pal, style, &screen_coords);
      }
    }

  } else {
    /* Simple object (plot individual polygons, checking direction of each).
       Assume that all polygons are in group 0 (checked earlier). */
    plot_group(centre, colours, bounding_box,
      obj_polygons_get_group(&obj->polygons, 0), false, pal, style, &screen_coords);
  }
}

static void plot_hill(
  Vertex const centre,
  HillColData const *const hill_colours,
  BBox *const bounding_box,
  HillCorner const (*const sides)[Hill_PolygonNumSides],
  int const colour,
  PaletteEntry const (*const pal)[NumColours], ObjGfxMeshStyle const style,
  Vertex (*const screen_coords)[ObjVertexMax])
{
  DEBUGF("Plotting hill polygon\n");

  Vertex polygon_coords[ObjPolygonMaxSides];
  int side = 0;

  /* Get first three coordinates and test for back-facing polygon */
  assert(side < Hill_PolygonNumSides);
  HillCorner vertex = (*sides)[side];
  assert(vertex < ARRAY_SIZE(*screen_coords));
  polygon_coords[side++] = (*screen_coords)[vertex];

  assert(side < Hill_PolygonNumSides);
  vertex = (*sides)[side];
  assert(vertex < ARRAY_SIZE(*screen_coords));
  polygon_coords[side++] = (*screen_coords)[vertex];

  assert(side < Hill_PolygonNumSides);
  vertex = (*sides)[side];
  assert(vertex < ARRAY_SIZE(*screen_coords));
  polygon_coords[side++] = (*screen_coords)[vertex];

  assert(side == Hill_PolygonNumSides);

  if (!vector_check(polygon_coords, polygon_coords + 1, polygon_coords + 2))
  {
    DEBUGF("Cull back-facing hill polygon\n");
    return;
  }

  /* Finally, we get to plot the polygon on the screen! */
  switch (style) {
  case ObjGfxMeshStyle_Wireframe:
    plot_wireframe(&polygon_coords, centre, Hill_PolygonNumSides);
    break;

  case ObjGfxMeshStyle_Filled:
    if (pal) {
      size_t const pindex = hillcol_get_colour(hill_colours, colour);
      assert(pindex <= ARRAY_SIZE(*pal));
      plot_set_col((*pal)[pindex]);
    }
    plot_filled(&polygon_coords, centre, Hill_PolygonNumSides);
    break;

  case ObjGfxMeshStyle_BBox:
    update_bbox(&polygon_coords, centre, bounding_box, Hill_PolygonNumSides);
    break;
  }
}

void ObjGfxMeshes_plot_poly_hill(ObjGfxMeshesView const *const ctx,
  HillColData const *hill_colours,
  HillType const type, unsigned char (*const colours)[Hill_MaxPolygons],
  unsigned char (*const heights)[HillCorner_Count],
  Vertex const centre, long int const distance, Vertex3D const pos,
  PaletteEntry const (*const pal)[NumColours], BBox *const bounding_box, ObjGfxMeshStyle const style)
{

  Vertex3D obj_pos = pos;

  rotate(ctx, &obj_pos);
  obj_pos.y += distance;

  /* Before adding hills to a linked list of objects to be plotted, the game offsets
     their coordinates by an extra objects grid square compared to other object types.
     That actually cancels out the offset below. */
  //obj_pos.x += ctx->rotated_xy.x << (HillSizeLog2 - 1);
  //obj_pos.y += ctx->rotated_xy.y << (HillSizeLog2 - 1);
  //obj_pos.z += ctx->rotated_xy.z << (HillSizeLog2 - 1);

  static Vertex3D rot_vertices[ObjVertexMax];

  rot_vertices[HillCorner_A] = obj_pos;

  rot_vertices[HillCorner_D].x = obj_pos.x - (ctx->rotated.x.x << HillSizeLog2);
  rot_vertices[HillCorner_D].y = obj_pos.y - (ctx->rotated.x.y << HillSizeLog2);
  rot_vertices[HillCorner_D].z = obj_pos.z - (ctx->rotated.x.z << HillSizeLog2);

  rot_vertices[HillCorner_B].x = obj_pos.x - (ctx->rotated.y.x << HillSizeLog2);
  rot_vertices[HillCorner_B].y = obj_pos.y - (ctx->rotated.y.y << HillSizeLog2);
  rot_vertices[HillCorner_B].z = obj_pos.z - (ctx->rotated.y.z << HillSizeLog2);

  rot_vertices[HillCorner_C].x = obj_pos.x - (ctx->rotated_xy.x << HillSizeLog2);
  rot_vertices[HillCorner_C].y = obj_pos.y - (ctx->rotated_xy.y << HillSizeLog2);
  rot_vertices[HillCorner_C].z = obj_pos.z - (ctx->rotated_xy.z << HillSizeLog2);

  for (HillCorner c = HillCorner_A; c < HillCorner_Count; ++c) {
    rot_vertices[c].x -= (*heights)[c] * ctx->rotated.z.x;
    rot_vertices[c].y -= (*heights)[c] * ctx->rotated.z.y;
    rot_vertices[c].z -= (*heights)[c] * ctx->rotated.z.z;
  }

  static Vertex screen_coords[ObjVertexMax];
  to_screen_coords(HillCorner_Count, ctx->map_scaler, rot_vertices, screen_coords);

  if (bounding_box != NULL)
  {
    bounding_box->xmin = INT_MAX;
    bounding_box->ymin = INT_MAX;
    bounding_box->xmax = INT_MIN;
    bounding_box->ymax = INT_MIN;
  }

  static HillCorner const sides[HillType_Count][Hill_MaxPolygons][Hill_PolygonNumSides] = {
    [HillType_ABCA_ACDA] = {{HillCorner_A, HillCorner_B, HillCorner_C},
                            {HillCorner_A, HillCorner_C, HillCorner_D}},
    [HillType_ABDA_BCDB] = {{HillCorner_A, HillCorner_B, HillCorner_D},
                            {HillCorner_B, HillCorner_C, HillCorner_D}},
    [HillType_ABDA] = {{HillCorner_A, HillCorner_B, HillCorner_D}},
    [HillType_ABCA] = {{HillCorner_A, HillCorner_B, HillCorner_C}},
    [HillType_BCDB] = {{HillCorner_B, HillCorner_C, HillCorner_D}},
    [HillType_CDAC] = {{HillCorner_C, HillCorner_D, HillCorner_A}},
  };

  size_t p = 0;
  int colour = colours ? (*colours)[p] : 0;
  plot_hill(centre, hill_colours, bounding_box, &sides[type][p++], colour, pal, style, &screen_coords);

  if (type == HillType_ABCA_ACDA || type == HillType_ABDA_BCDB) {
    colour = colours ? (*colours)[p] : 0;
    plot_hill(centre, hill_colours, bounding_box, &sides[type][p++], colour, pal, style, &screen_coords);
  }
}


ObjGfxAngle ObjGfxAngle_from_map(MapAngle const angle)
{
  static int const angle_map[MapAngle_Count] = {
    [MapAngle_North] = OBJGFXMESH_ANGLE_QUART*2,
    [MapAngle_East] = OBJGFXMESH_ANGLE_QUART*3,
    [MapAngle_South] = 0,
    [MapAngle_West] = OBJGFXMESH_ANGLE_QUART,
  };
  assert(angle >= 0);
  assert(angle < ARRAY_SIZE(angle_map));
  return (ObjGfxAngle){angle_map[angle]};
}

void ObjGfxMeshes_set_direction(ObjGfxMeshesView *const ctx,
  ObjGfxDirection const direction, int map_scaler)
{
  assert(ctx);

  *ctx = (ObjGfxMeshesView){
    .direction = direction,
    .rotated = {
      .x = {.x = UNIT_VECTOR, .y = 0, .z = 0},
      .y = {.x = 0, .y = UNIT_VECTOR, .z = 0},
      .z = {.x = 0, .y = 0, .z = UNIT_VECTOR},
    },
    .map_scaler = map_scaler,
  };

  rotate(ctx, &ctx->rotated.x);
  rotate(ctx, &ctx->rotated.y);
  rotate(ctx, &ctx->rotated.z);

  /* Precalculate a diagonal (non-unit) vector in the xy plane */
  ctx->rotated_xy.x = ctx->rotated.x.x + ctx->rotated.y.x;
  ctx->rotated_xy.y = ctx->rotated.x.y + ctx->rotated.y.y;
  ctx->rotated_xy.z = ctx->rotated.x.z + ctx->rotated.y.z;
}

long int ObjGfxMeshes_get_pal_distance(ObjGfxMeshes const *const meshes, ObjRef const obj_ref)
{
  assert(meshes);
  ObjGfxMesh *const obj = obj_array_get(&meshes->ground, obj_ref);
  return obj->misc.pal_dist;
}

void ObjGfxMeshes_set_pal_distance(ObjGfxMeshes *const meshes, ObjRef const obj_ref, long int const distance)
{
  assert(meshes);
  assert(distance >= 0);
  ObjGfxMesh *const obj = obj_array_get(&meshes->ground, obj_ref);
  obj->misc.pal_dist = distance;
}

MapPoint ObjGfxMeshes_get_collision_size(ObjGfxMeshes const *const meshes, ObjRef const obj_ref)
{
  assert(meshes);
  ObjGfxMesh const *const obj = obj_array_get(&meshes->ground, obj_ref);
  return (MapPoint){obj->misc.coll_x, obj->misc.coll_y};
}

MapPoint ObjGfxMeshes_get_max_collision_size(ObjGfxMeshes *const meshes)
{
  assert(meshes);

  if (!meshes->have_max_collision_size) {
    meshes->have_max_collision_size = true;
    meshes->max_collision_size = (MapPoint){0,0};

    for (size_t n = 0; n < meshes->ground.ocount; ++n) {
      MapPoint const size = ObjGfxMeshes_get_collision_size(meshes, objects_ref_from_num(n));
      meshes->max_collision_size =
        MapPoint_max(meshes->max_collision_size, size);
    }
  }

  DEBUGF("Largest object collision size is %" PRIMapCoord ",%" PRIMapCoord "\n",
         meshes->max_collision_size.x, meshes->max_collision_size.y);

  return meshes->max_collision_size;
}
