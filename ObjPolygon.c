/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal object polygons
 *  Copyright (C) 2021 Christopher Bazley
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

#include <limits.h>

#include "Flex.h"
#include "Debug.h"
#include "Reader.h"
#include "Macros.h"

#include "SFError.h"
#include "ObjPolygon.h"

enum {
  ObjPolygonNumSidesMask = 0x0fu,
  ObjPolygonNumSidesShift = 0,
  ObjPolygonGroupMask = 0x70u,
  ObjPolygonGroupShift = 4,
  ObjPolygonSpecialColour = 0x80u,
  ObjPolygonColourHighBit = 1 << 8,
  PAllocGrowth = 2,
  PAllocInit = 8,
  BytesPerColour = 1,
  ObjPolygonMinVertex = 1,
};

static SFError obj_polygon_read(ObjPolygon * const polygon,
  Reader * const reader, size_t const num_vertices, size_t *const max_group)
{
  assert(!reader_ferror(reader));
  assert(max_group);

  const int byte = reader_fgetc(reader);
  if (byte == EOF)
  {
    DEBUGF("Failed to read no. of sides and plot group\n");
    return SFERROR(ReadFail);
  }
  size_t const num_sides_and_group = (size_t)byte;

  size_t const num_sides = (num_sides_and_group & ObjPolygonNumSidesMask) >>
                          ObjPolygonNumSidesShift;

  size_t const colour_high = num_sides_and_group & ObjPolygonSpecialColour;

  size_t const group = (num_sides_and_group & ObjPolygonGroupMask) >>
                        ObjPolygonGroupShift;

  if (num_sides < ObjPolygonMinSides || num_sides > ObjPolygonMaxSides)
  {
    DEBUGF("Bad side count %zu\n", num_sides);
    return SFERROR(BadNumSides);
  }

  if (group != ObjPolygonFacingCheckGroup)
  {
    *max_group = HIGHEST(group, *max_group);
  }

  DEBUGF("Found %zu sides in group %zu at offset %ld (0x%lx)\n",
         num_sides, group, reader_ftell(reader), reader_ftell(reader));

  /* We need to read the polygon definition into a temporary array so
     that we can get its colour byte at the end before outputting vertex
     indices. */

  if (polygon)
  {
    /* Get the vertex indices */
    for (size_t s = 0; s < num_sides; ++s)
    {
      int const v = reader_fgetc(reader);
      if (v == EOF)
      {
        DEBUGF("Failed to read side %zu of polygon\n", s);
        return SFERROR(ReadFail);
      }

      /* Validate the vertex index */
      if (v < ObjPolygonMinVertex || (size_t)v > num_vertices)
      {
        DEBUGF("Bad vertex %d (side %zu of polygon)\n",
               v - 1, s);
        return SFERROR(BadVertex);
      }

      /* Vertex indices are stored using an offset encoding */
      assert(v - ObjPolygonMinVertex <= UCHAR_MAX);
      polygon->sides[s] = v - ObjPolygonMinVertex;
    }

    const int colour_low = reader_fgetc(reader);
    if (colour_low == EOF)
    {
      DEBUGF("Failed to read colour\n");
      return SFERROR(ReadFail);
    }

    polygon->colour = colour_low + (colour_high ? ObjPolygonColourHighBit : 0);
    polygon->scount = num_sides;
    polygon->group = group;
  }
  else if (reader_fseek(reader, (long)num_sides + BytesPerColour, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }

  return SFERROR(OK);
}


void obj_polygons_init(ObjPolygons *const polygons)
{
  assert(polygons);
  *polygons = (ObjPolygons){
    .groups = {{.pcount = 0}},
  };
}

void obj_polygons_free(ObjPolygons *const polygons)
{
  for (size_t g = 0; g < ObjPolygonMaxGroups; ++g)
  {
    ObjGroup *const group = obj_polygons_get_group(polygons, g);
    assert(!group->pcount || group->polygons);
    if (group->polygons)
    {
      assert(group->palloc > 0);
      flex_free(&group->polygons);
    }
  }
}

SFError obj_group_add_polygon(ObjGroup *const group, ObjPolygon const polygon)
{
  assert(group != NULL);
  assert(group->pcount >= 0);
  assert(group->pcount <= group->palloc);
  assert(!group->polygons || ((group->palloc * sizeof(ObjPolygon)) == (size_t)flex_size(&group->polygons)));

  if (group->pcount + 1 > group->palloc)
  {
    if (group->polygons)
    {
      assert(group->palloc > 0);
      size_t const new_size = group->palloc * PAllocGrowth;
      if (!flex_extend(&group->polygons, (int)(sizeof(ObjPolygon) * new_size)))
      {
        return SFERROR(NoMem);
      }
      group->palloc = new_size;
    }
    else
    {
      assert(group->palloc == 0);
      if (!flex_alloc(&group->polygons, sizeof(ObjPolygon) * PAllocInit))
      {
        return SFERROR(NoMem);
      }
      group->palloc = PAllocInit;
    }
  }

  ((ObjPolygon *)group->polygons)[group->pcount++] = polygon;
  return SFERROR(OK);
}

SFError obj_polygons_read(ObjPolygons *const polygons, Reader *const reader,
  size_t const nvertices, size_t *const max_group)
{
  assert(!reader_ferror(reader));
  assert(nvertices >= 0);
  assert(max_group);

  /* Get number of polygons */
  const int num_polygons = reader_fgetc(reader);
  if (num_polygons == EOF)
  {
    DEBUGF("Failed to read no. of polygons\n");
    return SFERROR(ReadFail);
  }

  if (num_polygons < 1)
  {
    DEBUGF("Bad polygon count %d\n", num_polygons);
    return SFERROR(BadNumPolygons);
  }

  DEBUGF("Found %d polygons at offset %ld (0x%lx)\n", num_polygons,
         reader_ftell(reader), reader_ftell(reader));

  *max_group = 0;

  for (int p = 0; p < num_polygons; ++p)
  {
    ObjPolygon polygon;
    SFError err = obj_polygon_read(polygons ? &polygon : NULL, reader, nvertices, max_group);
    if (SFError_fail(err))
    {
      return err;
    }

    if (polygons)
    {
      err = obj_group_add_polygon(&polygons->groups[polygon.group], polygon);
      if (SFError_fail(err))
      {
        return err;
      }
    }
  } /* next polygon */

  return SFERROR(OK);
}

ObjGroup *obj_polygons_get_group(ObjPolygons *const polygons,
  size_t const n)
{
  assert(polygons != NULL);
  assert(n >= 0);
  assert(n < ObjPolygonMaxGroups);
  return &polygons->groups[n];
}

size_t obj_group_get_polygon_count(ObjGroup *const group)
{
  assert(group);
  return group->pcount;
}

ObjPolygon obj_group_get_polygon(ObjGroup *const group, size_t const n)
{
  assert(group != NULL);
  assert(group->pcount >= 0);
  assert(group->pcount <= group->palloc);
  assert(!group->polygons || ((group->palloc * sizeof(ObjPolygon)) == (size_t)flex_size(&group->polygons)));
  assert(n >= 0);
  assert(n < group->pcount);

  return ((ObjPolygon *)group->polygons)[n];
}
