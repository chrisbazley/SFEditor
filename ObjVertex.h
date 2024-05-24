/*
 * SFeditor - Star Fighter 3000 map/mission editor
 * Polygonal object vertices
 * Copyright (C) 2021 Christopher Bazley
 */

#ifndef OBJVERTEX_H
#define OBJVERTEX_H

#include "SFError.h"
struct Reader;

enum {
  ObjVertexMax = 255,
};

typedef struct {
  size_t vcount;
  void *vertices;
} ObjVertices;

typedef struct
{
  long int x, y, z;
}
Vertex3D;

typedef struct
{
  Vertex3D x, y, z;
}
UnitVectors;

typedef enum
{
  RelCoord_SubMul32 = 85,
  RelCoord_SubMul16,
  RelCoord_SubMul8,
  RelCoord_SubMul4,
  RelCoord_SubMul2,
  RelCoord_SubUnit, /* subtract unit vector from previous coordinate */
  RelCoord_SubDiv2 = 96,
  RelCoord_SubDiv4,
  RelCoord_SubDiv8,
  RelCoord_SubDiv16,
  RelCoord_Zero,    /* no change from previous coordinate */
  RelCoord_AddDiv16,
  RelCoord_AddDiv8,
  RelCoord_AddDiv4,
  RelCoord_AddDiv2,
  RelCoord_AddUnit = 110, /* add unit vector to previous coordinate */
  RelCoord_AddMul2,
  RelCoord_AddMul4,
  RelCoord_AddMul8,
  RelCoord_AddMul16,
  RelCoord_AddMul32,
}
RelCoord;

typedef struct
{
  unsigned char x, y, z;
} ObjVertex;

void obj_vertices_init(ObjVertices *varray);
void obj_vertices_free(ObjVertices *varray);

SFError obj_vertices_read(ObjVertices *varray, struct Reader *reader, size_t *nvert);

size_t obj_vertices_get_count(ObjVertices *varray);

void obj_vertices_scale_unit(UnitVectors *out, UnitVectors const *in, int div_log2);
void obj_vertices_add_scaled_unit(Vertex3D *vertex_pos,
  const UnitVectors *unit, ObjVertex coord);

void obj_vertices_to_coords(ObjVertices *varray, Vertex3D const *centre,
  const UnitVectors *unit, Vertex3D (*out)[ObjVertexMax]);

#endif /* OBJVERTEX_H */
