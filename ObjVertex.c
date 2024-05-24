/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal object vertices
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

#include "Flex.h"
#include "Debug.h"
#include "Reader.h"
#include "Macros.h"

#include "SFError.h"
#include "ObjVertex.h"

enum {
  NDims = 3,
};

void obj_vertices_init(ObjVertices * const varray)
{
  assert(varray != NULL);
  *varray = (ObjVertices){
    .vcount = 0,
    .vertices = NULL,
  };
}

void obj_vertices_free(ObjVertices * const varray)
{
  assert(varray != NULL);
  assert(varray->vcount >= 0);
  assert(varray->vcount <= ObjVertexMax);
  if (varray->vertices)
  {
    assert(varray->vcount * sizeof(ObjVertex) == (size_t)flex_size(&varray->vertices));
    flex_free(&varray->vertices);
  }
}

size_t obj_vertices_get_count(ObjVertices *varray)
{
  assert(varray != NULL);
  assert(varray->vcount >= 0);
  assert(varray->vcount <= ObjVertexMax);
  assert(varray->vcount * sizeof(ObjVertex) <= (size_t)flex_size(&varray->vertices));
  return varray->vcount;
}

SFError obj_vertices_read(ObjVertices * const varray,
  Reader * const reader, size_t *const nvert)
{
  assert(!reader_ferror(reader));
  assert(nvert);

  const int tmp = reader_fgetc(reader);
  if (tmp == EOF)
  {
    DEBUGF("Failed to read no. of vertices\n");
    return SFERROR(ReadFail);
  }

  if (tmp < 1 || tmp > ObjVertexMax)
  {
    DEBUGF("Bad vertex count %d\n", tmp);
    return SFERROR(BadNumVertices);
  }
  size_t vcount = (size_t)tmp;

  *nvert = vcount;

  long int const pos = reader_ftell(reader);
  NOT_USED(pos);
  DEBUGF("Found %zu vertices at offset %ld (0x%lx)\n", vcount, pos, pos);

  if (varray)
  {
    obj_vertices_free(varray);
    obj_vertices_init(varray);

    if (!flex_alloc(&varray->vertices, (int)(vcount * sizeof(ObjVertex))))
    {
      DEBUGF("Failed to allocate memory for %zu vertices\n", vcount);
      return SFERROR(NoMem);
    }

    for (size_t v = 0; v < vcount; ++v)
    {
      char vbytes[NDims];
      if (reader_fread(vbytes, sizeof(vbytes), 1, reader) != 1) {
        DEBUGF("Failed to read vertex %zu\n", v);
        return SFERROR(ReadFail);
      }

      ObjVertex const vertex = {vbytes[0], vbytes[1], vbytes[2]};
      DEBUGF("Add vertex %zu {%d,%d,%d}\n", v, vertex.x, vertex.y, vertex.z);
      ((ObjVertex *)varray->vertices)[v] = vertex;
    } /* next vertex */

    varray->vcount = vcount;
  }
  else if (reader_fseek(reader, (long)vcount * NDims, SEEK_CUR))
  {
    return SFERROR(BadSeek);
  }

  return SFERROR(OK);
}

static void add_scaled_vector(Vertex3D *const input_vertex,
  Vertex3D const *const move_vector, RelCoord const encoded_shift)
{
  /* Adds a scaled vector to a vertex in three dimensional space. The scale
     factor is encoded using a weird system unique to Star Fighter 3000
     (but basically it is multiplication or division by a power of 2). */
  DEBUG_VERBOSE("Adding vector %ld,%ld,%ld (encoded factor %d) to vertex %ld,%ld,%ld",
        move_vector->x, move_vector->y, move_vector->z, (int)encoded_shift,
        input_vertex->x, input_vertex->y, input_vertex->z);

  if (encoded_shift == RelCoord_Zero)
  {
    return; /* no movement */
  }

  if (abs((int)encoded_shift - RelCoord_Zero) < (int)(RelCoord_AddUnit - RelCoord_Zero)) {
    if (encoded_shift < RelCoord_Zero) {
      assert(encoded_shift >= RelCoord_SubDiv2);
      unsigned int const shift = encoded_shift - RelCoord_SubDiv2 + 1;
      /* Shifts have implementation-defined behaviour for negative numbers but the
         Norcroft C compiler generates terrible code for equivalent * or / */
      input_vertex->x -= move_vector->x >> shift;
      input_vertex->y -= move_vector->y >> shift;
      input_vertex->z -= move_vector->z >> shift;
    } else {
      assert(encoded_shift <= RelCoord_AddDiv2);
      unsigned int const shift = RelCoord_AddDiv2 + 1 - encoded_shift;
      input_vertex->x += move_vector->x >> shift;
      input_vertex->y += move_vector->y >> shift;
      input_vertex->z += move_vector->z >> shift;
    }
  } else {
    if (encoded_shift < RelCoord_Zero) {
      assert(encoded_shift >= RelCoord_SubMul32);
      unsigned int const shift = RelCoord_SubUnit - encoded_shift;
      input_vertex->x -= move_vector->x << shift;
      input_vertex->y -= move_vector->y << shift;
      input_vertex->z -= move_vector->z << shift;
    } else {
      assert(encoded_shift <= RelCoord_AddMul32);
      unsigned int const shift = encoded_shift - RelCoord_AddUnit;
      input_vertex->x += move_vector->x << shift;
      input_vertex->y += move_vector->y << shift;
      input_vertex->z += move_vector->z << shift;
    }
  }
}

static void scale_vector(Vertex3D *const out, Vertex3D const *const in,
  int const div_log2)
{
  assert(out);
  assert(div_log2 >= 0);

  *out = (Vertex3D){
    .x = in->x >> div_log2,
    .y = in->y >> div_log2,
    .z = in->z >> div_log2,
  };

  DEBUG("Scaled vector: %ld,%ld,%ld", out->x, out->y, out->z);
}

/* Scale the pre-rotated vectors according to the object size */
void obj_vertices_scale_unit(UnitVectors *const out, UnitVectors const *const in,
  int const div_log2)
{
  assert(out);
  assert(in);

  scale_vector(&out->x, &in->x, div_log2);
  scale_vector(&out->y, &in->y, div_log2);
  scale_vector(&out->z, &in->z, div_log2);
}

void obj_vertices_add_scaled_unit(Vertex3D *const vertex_pos,
  const UnitVectors *const unit, ObjVertex const coord)
{
  assert(vertex_pos != NULL);
  assert(unit != NULL);

  add_scaled_vector(vertex_pos, &unit->x, (RelCoord)coord.x);
  add_scaled_vector(vertex_pos, &unit->y, (RelCoord)coord.y);
  add_scaled_vector(vertex_pos, &unit->z, (RelCoord)coord.z);
}

/* Calculate the actual vertex coordinates by moving away from the object's
   centre along the pre-rotated unit vectors. The order in which these are
   applied (and the amount of movement in each direction) is dictated by the
   3 bytes of encoded data for each vertex. */
void obj_vertices_to_coords(ObjVertices * const varray, Vertex3D const *const centre,
  const UnitVectors *const unit, Vertex3D (*const out)[ObjVertexMax])
{
  assert(varray != NULL);
  assert(varray->vcount >= 0);
  assert(varray->vcount * sizeof(ObjVertex) <= (size_t)flex_size(&varray->vertices));
  assert(varray->vcount <= ObjVertexMax);
  assert(centre != NULL);
  assert(unit != NULL);
  assert(out != NULL);

  Vertex3D vertex_pos = *centre;

  size_t const num_vertices = varray->vcount;
  for (size_t v = 0; v < num_vertices; v++)
  {
    ObjVertex const coord = ((ObjVertex *)varray->vertices)[v];
    DEBUG_VERBOSE("Encoded factors for vertex %zu are %d,%d,%d", v,
                  coord.x, coord.y, coord.z);

    obj_vertices_add_scaled_unit(&vertex_pos, unit, coord);

    (*out)[v] = vertex_pos;
    DEBUG("Scaled & rotated vertex %zu is at %ld,%ld,%ld", v, (*out)[v].x,
          (*out)[v].y, (*out)[v].z);
  } /* next vertex */
}
