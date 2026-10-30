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

#include "stdlib.h"
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Utils.h"
#include "Obj.h"
#include "ObjVertex.h"
#include "ObjPolygon.h"

#include "ObjGfxMesh.h"
#include "ObjGfxMeshData.h"
#include "ObjGfxMeshInternal.h"

enum {
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
};

typedef enum
{
  ObjectType_Ground, /* Ground objects (trees, buildings) */
  ObjectType_Bit,    /* (Broken?) bits */
  ObjectType_Ship   /* Aerial things (fighters, coins, missiles) */
}
ObjectType;

static void obj_array_init(ObjGfxMeshArray *const array)
{
  assert(array);
  *array = (ObjGfxMeshArray){.ocount = 0, .oalloc = 0, .objects = NULL};
}

static void obj_array_free(ObjGfxMeshArray *const array)
{
  assert(array);
  assert(array->ocount <= array->oalloc);
  if (!array->objects)
  {
    return;
  }

  for (int n = 0; n < array->ocount; ++n)
  {
    ObjGfxMesh *const obj = array->objects[n];
    obj_vertices_free(&obj->varray);
    obj_polygons_free(&obj->polygons);
    free(obj);
  }

  free(array->objects);
}

static _Optional ObjGfxMesh *obj_array_add(ObjGfxMeshArray *const array,
  CoordinateScale const scale,
  unsigned char const plot_type,
  unsigned short const clip_size_x,
  unsigned short const clip_size_y)
{
  assert(array);
  assert(array->ocount <= array->oalloc);

  if ((array->ocount + 1) > array->oalloc)
  {
    int const new_size = array->oalloc ? (array->oalloc * OAllocGrowth) : OAllocInit;
    assert(new_size > 0);
    ObjGfxMesh *_Optional *const objects = realloc(array->objects, sizeof(*objects) * new_size);
    if (!objects)
    {
      return NULL;
    }
    array->oalloc = new_size;
    array->objects = objects;
  }

  assert(array->objects);
  if (!array->objects) {
    return NULL;
  }

  _Optional ObjGfxMesh *const new_obj = malloc(sizeof(*new_obj));
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

  array->objects[array->ocount++] = &*new_obj;

  return new_obj;
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
#ifdef DEBUG_OUTPUT
  int object_count = 0;
#endif
  do {
    long int expl_size = 36l * (last_explosion_num + 1l);

    DEBUGF("Found %"PRId32" explosion lines (%ld bytes) "
           "at offset %ld (0x%lx)\n", last_explosion_num + 1, expl_size,
           reader_ftell(reader), reader_ftell(reader));

    /* Skip the explosions data */
    if (reader_fseek(reader, expl_size, SEEK_CUR)) {
      DEBUGF("Failed to seek object attributes (object %d)\n",
              object_count);
      return SFERROR(BadSeek);
    }

    /* Get object type */
    int byte = reader_fgetc(reader);
    if (byte == EOF)
    {
      DEBUGF("Failed to read object type (object %d)\n",
              object_count);
      return SFERROR(ReadFail);
    }

    if ((byte != ObjectType_Ship) &&
        (byte != ObjectType_Ground) &&
        (byte != ObjectType_Bit))
    {
      DEBUGF("Bad object type %d (object %d)\n", byte,
              object_count);
      return SFERROR(BadObjectClass);
    }
    ObjectType const type = (ObjectType)byte;

    DEBUGF("Found object %d of type %d at offset %ld (0x%lx)\n",
           object_count, (int)type, reader_ftell(reader)-1, reader_ftell(reader)-1);

    byte = reader_fgetc(reader);
    if (byte == EOF)
    {
      DEBUGF("Failed to read scale (object %d)\n", object_count);
      return SFERROR(ReadFail);
    }
    CoordinateScale const scale = (CoordinateScale)byte;

    int const rot = reader_fgetc(reader);
    if (rot == EOF)
    {
      DEBUGF("Failed to read rotator (object %d)\n",
              object_count);
      return SFERROR(ReadFail);
    }

    const int gr_obj_coll_size = reader_fgetc(reader);
    if (gr_obj_coll_size == EOF)
    {
      DEBUGF("Failed to read packed collision size (object %d)\n", object_count);
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
      DEBUGF("Failed to read clip size (object %d)\n", object_count);
      return SFERROR(ReadFail);
    }

    int const score = reader_fgetc(reader);
    if (score == EOF)
    {
      DEBUGF("Failed to read score (object %d)\n", object_count);
      return SFERROR(ReadFail);
    }

    int const hits_or_min_z = reader_fgetc(reader);
    if (hits_or_min_z == EOF)
    {
      DEBUGF("Failed to read hitpoints (object %d)\n", object_count);
      return SFERROR(ReadFail);
    }

    int const explosion_style = reader_fgetc(reader);
    if (explosion_style == EOF)
    {
      DEBUGF("Failed to read explosion style (object %d)\n", object_count);
      return SFERROR(ReadFail);
    }

    const int plot_type_and_last_group = reader_fgetc(reader);
    if (plot_type_and_last_group == EOF)
    {
      DEBUGF("Failed to read plot type and max plot group (object %d)\n",
             object_count);
      return SFERROR(ReadFail);
    }

    unsigned char const plot_type =
      (plot_type_and_last_group & Object_PlotTypeMask) >> Object_PlotTypeShift;

    if (plot_type >= meshes->num_plot_types + 1)
    {
      DEBUGF("Bad plot type %d (object %d)\n", plot_type, object_count);
      return SFERROR(BadPlotType);
    }

    unsigned char const expected_max_group =
      (plot_type_and_last_group & Object_LastGroupMask) >> Object_LastGroupShift;

    if (expected_max_group >= ObjPolygonFacingCheckGroup)
    {
      DEBUGF("Bad highest plot group %d (object %d)\n",
              expected_max_group, object_count);
      return SFERROR(BadNumGroups);
    }

    if ((expected_max_group > 0) && (plot_type == 0))
    {
      DEBUGF("Highest plot group %d is higher than "
             "expected for plot type 0 (object %d)\n",
             expected_max_group, object_count);
      return SFERROR(BadNumGroups);
    }

    _Optional ObjGfxMeshArray *array = NULL;
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

    _Optional ObjGfxMesh *obj = NULL;
    if (array)
    {
      obj = obj_array_add(&*array, scale, plot_type, clip_size_x, clip_size_y);
      if (!obj)
      {
        return SFERROR(NoMem);
      }
    }

    int vcount;
    SFError err = obj_vertices_read(obj ? &obj->varray : NULL, reader, &vcount);
    if (SFError_fail(err))
    {
      return err;
    }

    if ((int)rot >= vcount)
    {
      DEBUGF("Bad rotator %d >= %d (object %d)\n", rot, vcount, object_count);
      return SFERROR(BadRotator);
    }

    /* Find the first word-aligned offset ahead of the vertex data */
    if (reader_fseek(reader, WORD_ALIGN(reader_ftell(reader)), SEEK_SET))
    {
      DEBUGF("Failed to seek clip distance (object %d)\n", object_count);
      return SFERROR(BadSeek);
    }

    int32_t clip_dist;
    if (!reader_fread_int32(&clip_dist, reader))
    {
      DEBUGF("Failed to read clip distance (object %d)\n", object_count);
      return SFERROR(ReadFail);
    }

    int max_group;
    err = obj_polygons_read(obj ? &obj->polygons : NULL, reader, vcount, &max_group);
    if (SFError_fail(err))
    {
      return err;
    }

    /* Can't require and exact match because of a mesh in Graphics.Earth2 */
    if (max_group > expected_max_group)
    {
      DEBUGF("Bad plot group %d > %d\n", max_group, expected_max_group);
      return SFERROR(BadPolygonGroup);
    }

    /* Validate the object's plot type. */
    if (obj && plot_type != 0)
    {
      /* Check that the referenced polygons exist */
      const int max_polygon = meshes->plot_types[plot_type - 1].max_polygon;
      ObjGroup *const vec = obj_polygons_get_group(&obj->polygons, ObjPolygonFacingCheckGroup);
      DEBUGF("Plot type %d requires polygon %d, count is %d\n",
        plot_type, max_polygon, obj_group_get_polygon_count(vec));

      if (max_polygon > obj_group_get_polygon_count(vec))
      {
        DEBUGF("Plot type %d is predicated on undefined polygon %d "
               "(object %d)\n", plot_type, max_polygon, object_count);
        //return SFERROR(BadPlotPolygon);
      }
    }

    if (obj)
    {
      if (type == ObjectType_Ground || type == ObjectType_Ship)
      {
        assert(score >= 0);
        assert(score <= USHRT_MAX / ScoreMultiplier);
        obj->misc.score = (unsigned short)(score * ScoreMultiplier);
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

#ifdef DEBUG_OUTPUT
    ++object_count;
#endif
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
    unsigned char command_count = 0;

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

      assert(command >= 0);
      assert(command <= UCHAR_MAX);

      unsigned char const operand = (command & PlotCommands_OperandMask) >> PlotCommands_OperandShift;

      PlotAction const action = (PlotAction)
          ((command & PlotCommands_ActionMask) >> PlotCommands_ActionShift);

      int group = operand;
      unsigned char polygon = 0;

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

      assert(group >= 0);
      assert(group <= UCHAR_MAX);

      pt->commands[command_count++] = (PlotCommand){
        .action = action,
        .group = (unsigned char)group,
        .polygon = polygon,
      };

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

