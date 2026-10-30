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
#include "Macros.h"
#include "Err.h"
#include "TrigTable.h"

#include "Utils.h"
#include "Plot.h"
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

#ifdef USE_OPTIONAL
#include "Optional.h"
#endif

#include "ObjGfxMeshInternal.h"

enum {
  DIV_TABLE_SIZE = 16384,
  PEX_SCALE = 2,
  PEX_SHIFT = 4,
  UNIT_VECTOR = 2048,
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

static ObjGfxMesh *obj_array_get(ObjGfxMeshArray const *const array, ObjRef const obj_ref)
{
  assert(array);
  assert(array->ocount <= array->oalloc);
  unsigned char const n = objects_ref_to_num(obj_ref);
  assert(n < array->ocount);
  assert(array->objects);
  if (!array->objects) {
    static ObjGfxMesh dummy;
    return &dummy;
  }
  return array->objects[n];
}

static _Optional TrigTable *trig_table;
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

  assert(trig_table);
  if (!trig_table) {
    return;
  }
  TrigTable *const tt = &*trig_table;

  /* Apply z rotation */
  int const z_rot = ObjGfxAngle_get(ctx->direction.z_rot);
  int cosine = TrigTable_look_up_cosine(tt, z_rot);
  int sine = TrigTable_look_up_sine(tt, z_rot);
  long int new_x = (vector->x * cosine) / SINE_TABLE_SCALE -
                   (vector->z * sine) / SINE_TABLE_SCALE;
  long int new_z = (vector->x * sine) / SINE_TABLE_SCALE +
                   (vector->z * cosine) / SINE_TABLE_SCALE;
  vector->z = new_z;
  vector->x = new_x;

  /* Apply x rotation */
  int const x_rot = ObjGfxAngle_get(ctx->direction.x_rot);
  cosine = TrigTable_look_up_cosine(tt, x_rot);
  sine = TrigTable_look_up_sine(tt, x_rot);
  new_x = (vector->x * cosine) / SINE_TABLE_SCALE -
          (vector->y * sine) / SINE_TABLE_SCALE;
  long int new_y = (vector->x * sine) / SINE_TABLE_SCALE +
                   (vector->y * cosine) / SINE_TABLE_SCALE;
  vector->y = new_y;
  vector->x = new_x;

  /* Apply y rotation */
  int const y_rot = ObjGfxAngle_get(ctx->direction.y_rot);
  cosine = TrigTable_look_up_cosine(tt, y_rot);
  sine = TrigTable_look_up_sine(tt, y_rot);
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

static void to_screen_coords(int const num_vertices, int const map_scaler,
  Vertex3D const *const rot_vertices, Vertex *const screen_coords)
{
  Vertex3D const *read_ptr = rot_vertices;
  Vertex *write_ptr = screen_coords;

  for (int v = 0; v < num_vertices; v++)
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
          DEBUG("Vertex %d is too far for perspective division", v + 1);
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
        DEBUG("Vertex %d is behind the viewer", v + 1);
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
  int const num_sides)
{

  DEBUGF("Update bbox for %d-sided polygon\n", num_sides);

  assert(num_sides >= 3);

  Vertex const first_corner = translate_screen(centre, (*polygon_coords)[0]);
  Vertex screen_pos = translate_screen(centre, (*polygon_coords)[1]);

  BBox_expand(bounding_box, first_corner);
  BBox_expand(bounding_box, screen_pos);

  for (int side = 2; side < num_sides; side++)
  {
    screen_pos = translate_screen(centre, (*polygon_coords)[side]);
    BBox_expand(bounding_box, screen_pos);
  } /* next side */
}

static void plot_filled(
  Vertex (*const polygon_coords)[ObjPolygonMaxSides],
  Vertex const centre,
  int const num_sides)
{
  DEBUGF("Plot %d-sided polygon\n", num_sides);

  assert(num_sides >= 3);

  Vertex const first_corner = translate_screen(centre, (*polygon_coords)[0]);
  Vertex screen_pos = translate_screen(centre, (*polygon_coords)[1]);

  plot_move(screen_pos);

  for (int side = 2; side < num_sides; side++)
  {
    screen_pos = translate_screen(centre, (*polygon_coords)[side]);

    plot_move(first_corner);
    plot_fg_tri(screen_pos);
  } /* next side */
}

static void plot_wireframe(
  Vertex (*const polygon_coords)[ObjPolygonMaxSides],
  Vertex const centre,
  int const num_sides)
{
  DEBUG("Plot pending %d-sided polygon", num_sides);
  assert(num_sides >= 3);

  Vertex const first_corner = translate_screen(centre, (*polygon_coords)[0]);
  Vertex screen_pos = translate_screen(centre, (*polygon_coords)[1]);

  plot_move(first_corner);
  plot_fg_line_ex_end(screen_pos);

  for (int side = 2; side < num_sides; side++)
  {
    screen_pos = translate_screen(centre, (*polygon_coords)[side]);
    plot_fg_line_ex_end(screen_pos);
  } /* next side */

  plot_fg_line_ex_end(first_corner);
}

static void plot_group(
  Vertex const centre,
  _Optional PolyColData const *const colours,
  _Optional BBox *const bounding_box,
  ObjGroup *const group, bool const plot_all,
  _Optional PaletteEntry const (*const pal)[NumColours], ObjGfxMeshStyle const style,
  Vertex (*const screen_coords)[ObjVertexMax])
{
  int const pcount = obj_group_get_polygon_count(group);
  DEBUGF("Plotting %d polygons\n", pcount);

  for (int p = 0; p < pcount; ++p)
  {
    ObjPolygon const polygon = obj_group_get_polygon(group, p);
    int const num_sides = obj_polygon_get_side_count(&polygon);

    Vertex polygon_coords[ObjPolygonMaxSides];
    int side = 0;

    /* Get first three coordinates and test for back-facing polygon */
    assert(side < num_sides);
    int vertex = obj_polygon_get_side(&polygon, side);
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
      DEBUGF("Cull back-facing polygon %d\n", p);
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
      if (pal && colours) {
        int const colour = obj_polygon_get_colour(&polygon);
        int const pindex = polycol_get_colour(&*colours, colour);
        assert(pindex < (int)ARRAY_SIZE(*pal));
        plot_set_col((*pal)[pindex]);
      }
      plot_filled(&polygon_coords, centre, num_sides);
      break;

    case ObjGfxMeshStyle_BBox:
      if (bounding_box) {
        update_bbox(&polygon_coords, centre, &*bounding_box, num_sides);
      }
      break;
    }
  }
}

int ObjGfxMeshes_get_ground_count(const ObjGfxMeshes *const meshes)
{
  assert(meshes);
  assert(meshes->ground.ocount <= meshes->ground.oalloc);
  return meshes->ground.ocount;
}

int ObjGfxMeshes_get_ships_count(const ObjGfxMeshes *const meshes)
{
  assert(meshes);
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

  for (int n = 0; n < meshes->ground.ocount; ++n)
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

  DEBUG("Object type %d covers area %" PRIMapCoord ",%" PRIMapCoord
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

_Optional TrigTable const *ObjGfxMeshes_get_trig_table(void)
{
  return trig_table;
}

static void plot_lines(ObjGfxMeshesView const *const ctx, Vertex const centre,
                       long int const distance, Vertex3D const pos,
                       ObjVertex const vertices[], int const n)
{
  assert(vertices);

  Vertex3D obj_pos = pos;
  rotate(ctx, &obj_pos);
  obj_pos.y += distance;

  static Vertex3D rot_vertices[ObjVertexMax];
  for (int i = 0; i < n; ++i)
  {
    obj_vertices_add_scaled_unit(&obj_pos, &ctx->rotated, vertices[i]);
    rot_vertices[i] = obj_pos;
  }

  static Vertex screen_coords[ObjVertexMax];
  to_screen_coords(n, ctx->map_scaler, rot_vertices, screen_coords);

  for (int k = 0; k + 1 < n; k += 2)
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
                       _Optional PolyColData const *const colours,
                       ObjRef const obj_ref,
                       Vertex const centre,
                       long int const distance, Vertex3D const pos,
                       _Optional PaletteEntry const (*const pal)[NumColours],
                       _Optional BBox *const bounding_box,
                       ObjGfxMeshStyle const style)
{
  assert(meshes);
  DEBUG("Request to plot object %d at coords %d,%d (world coords %ld,%ld,%ld)",
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

  int const num_vertices = obj_vertices_get_count(&obj->varray);
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
    int const pcount = LOWEST(obj_group_get_polygon_count(group), (int)ARRAY_SIZE(vector_results));

    for (int p = 0; p < pcount; p++)
    {
      ObjPolygon const polygon = obj_group_get_polygon(group, p);
      Vertex polygon_coords[ObjPolygonMaxSides];
      int side = 0;

      /* Get first three coordinates and do facing test */
      int vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < num_vertices);
      polygon_coords[side++] = screen_coords[vertex];

      vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < num_vertices);
      polygon_coords[side++] = screen_coords[vertex];

      vertex = obj_polygon_get_side(&polygon, side);
      assert(vertex < num_vertices);
      polygon_coords[side++] = screen_coords[vertex];

      vector_results[p] = vector_check(polygon_coords, polygon_coords + 1, polygon_coords + 2);

      DEBUG("Facing check %d is %s", p, vector_results[p] ? "true" : "false");
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
  _Optional HillColData const *const hill_colours,
  _Optional BBox *const bounding_box,
  HillCorner const (*const sides)[Hill_PolygonNumSides],
  int const colour,
  _Optional PaletteEntry const (*const pal)[NumColours], ObjGfxMeshStyle const style,
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
    if (pal && hill_colours) {
      int const pindex = hillcol_get_colour(&*hill_colours, colour);
      assert(pindex <= (int)ARRAY_SIZE(*pal));
      plot_set_col((*pal)[pindex]);
    }
    plot_filled(&polygon_coords, centre, Hill_PolygonNumSides);
    break;

  case ObjGfxMeshStyle_BBox:
    if (bounding_box) {
      update_bbox(&polygon_coords, centre, &*bounding_box, Hill_PolygonNumSides);
    }
    break;
  }
}

void ObjGfxMeshes_plot_poly_hill(ObjGfxMeshesView const *const ctx,
                                 _Optional HillColData const *hill_colours,
                                 HillType const type,
                                 _Optional unsigned char (*const colours)[Hill_MaxPolygons],
                                 unsigned char (*const heights)[HillCorner_Count],
                                 Vertex const centre, long int const distance,
                                 Vertex3D const pos,
                                 _Optional PaletteEntry const (*const pal)[NumColours],
                                 _Optional BBox *const bounding_box,
                                 ObjGfxMeshStyle const style)
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

  int p = 0;
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

    for (int n = 0; n < meshes->ground.ocount; ++n) {
      MapPoint const size = ObjGfxMeshes_get_collision_size(meshes, objects_ref_from_num(n));
      meshes->max_collision_size =
        MapPoint_max(meshes->max_collision_size, size);
    }
  }

  DEBUGF("Largest object collision size is %" PRIMapCoord ",%" PRIMapCoord "\n",
         meshes->max_collision_size.x, meshes->max_collision_size.y);

  return meshes->max_collision_size;
}
