/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal object plot groups
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

#include "SFError.h"
#include "ObjVertex.h"

void obj_vertex_array_init(ObjVertexArray * const varray)
{
  assert(varray != NULL);
  *varray = (ObjVertexArray){
    .nvertices = 0,
    .vertices = NULL,
  };
}

void obj_vertex_array_free(ObjVertexArray * const varray)
{
  assert(varray != NULL);
  assert(varray->nvertices >= 0);
  if (varray->vertices)
  {
    assert(varray->nvertices * sizeof(vertex) <= flex_size(&varray->vertices));
    flex_free(&varray->vertices);
  }
}

ObjVertex obj_vertex_array_get(ObjVertexArray * const varray, const int n)
{
  assert(varray != NULL);
  assert(varray->nvertices >= 0);
  assert(varray->nvertices * sizeof(vertex) <= flex_size(&varray->vertices));
  assert(n >= 0);
  assert(n < varray->nvertices);

  return ((ObjVertex *)varray->vertices)[n];
}

int obj_vertex_array_get_count(ObjVertexArray * const varray)
{
  assert(varray != NULL);
  assert(varray->nvertices >= 0);
  return varray->nvertices;
}

SFError obj_vertex_array_read(ObjVertexArray * const varray,
  Reader * const reader)
{
  assert(varray != NULL);
  assert(varray->nvertices >= 0);
  assert(!reader_ferror(reader));

  const int nvertices = reader_fgetc(reader);
  if (nvertices == EOF) {
    DEBUGF("Failed to read no. of vertices\n");
    return SFERROR(ReadFail);
  }

  if (nvertices < 1) {
    DEBUGF("Bad vertex count %d\n", nvertices);
    return SFERROR(BadNumVertices);
  }

  obj_vertex_array_free(varray);
  obj_vertex_array_init(varray);

  if (!flex_alloc(&varray->vertices, nvertices * sizeof(ObjVertex)))
  {
    DEBUGF("Failed to allocate memory for %d vertices\n", nvertices);
    return SFERROR(NoMem);
  }

  long int const pos = reader_ftell(reader);
  DEBUGF("Found %d vertices at offset %ld (0x%lx)\n", nvertices, pos, pos);

  for (int v = 0; v < nvertices; ++v) {
    char vbytes[3];
    if (reader_fread(vbytes, sizeof(vbytes), 1, reader) != 1) {
      DEBUGF("Failed to read vertex %d\n", v);
      return SFERROR(ReadFail);
    }

    ObjVertex const vertex = {vbytes[0], vbytes[1], vbytes[2]};
    DEBUGF("Add vertex %d {%d,%d,%d}\n", v, vertex.x, vertex.y, vertex.z);
    ((ObjVertex *)varray->vertices)[v] = vertex;
  } /* next vertex */

  varray->nvertices = nvertices;
  return SFERROR(OK);
}
