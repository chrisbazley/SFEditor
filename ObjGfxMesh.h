/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal object meshes
 *  Copyright (C) 2007 Christopher Bazley
 */

#ifndef ObjGfxMesh_h
#define ObjGfxMesh_h

#include <stdbool.h>

#include "DFile.h"
#include "SFInit.h"

#include "Vertex.h"
#include "PolyCol.h"
#include "MapCoord.h"
#include "Reader.h"
#include "ObjVertex.h"
#include "TrigTable.h"
#include "Hill.h"
#include "HillCol.h"
#include "Obj.h"

enum {
  OBJGFXMESH_ANGLE_QUART = 128,
  SINE_TABLE_SCALE_LOG2 = 10,
  SINE_TABLE_SCALE = 1 << SINE_TABLE_SCALE_LOG2,
};

typedef struct {
  int v;
} ObjGfxAngle;

ObjGfxAngle ObjGfxAngle_from_map(MapAngle angle);

typedef struct {
  ObjGfxAngle x_rot, y_rot, z_rot;
} ObjGfxDirection;

typedef struct ObjGfxMeshesView {
  UnitVectors rotated;
  Vertex3D rotated_xy; /* Diagonal (non-unit) vector in the xy plane */
  ObjGfxDirection direction;
  int map_scaler;
} ObjGfxMeshesView;

typedef struct ObjGfxMeshes ObjGfxMeshes;

void ObjGfxMeshes_init(ObjGfxMeshes *meshes);
void ObjGfxMeshes_free(ObjGfxMeshes *meshes);
SFError ObjGfxMeshes_read(ObjGfxMeshes *meshes, Reader *const reader);

size_t ObjGfxMeshes_get_ground_count(const ObjGfxMeshes *meshes);
size_t ObjGfxMeshes_get_ships_count(const ObjGfxMeshes *meshes);

MapArea ObjGfxMeshes_get_ground_bbox(ObjGfxMeshes *meshes, ObjRef index, MapAngle angle);
MapArea ObjGfxMeshes_get_hill_bbox(HillType hill_type,
  unsigned char (*heights)[HillCorner_Count], MapAngle angle);

MapArea ObjGfxMeshes_get_max_ground_bbox(ObjGfxMeshes *meshes, MapAngle angle);
MapPoint ObjGfxMeshes_get_collision_size(ObjGfxMeshes const *meshes, ObjRef obj_no);
MapPoint ObjGfxMeshes_get_max_collision_size(ObjGfxMeshes *meshes);

long int ObjGfxMeshes_get_pal_distance(ObjGfxMeshes const *meshes, ObjRef obj_no);
void ObjGfxMeshes_set_pal_distance(ObjGfxMeshes *meshes, ObjRef obj_no, long int distance);

void ObjGfxMeshes_global_init(void);

TrigTable const *ObjGfxMeshes_get_trig_table(void);

void ObjGfxMeshes_set_direction(ObjGfxMeshesView *ctx, ObjGfxDirection direction, int map_scaler);

typedef enum {
  ObjGfxMeshStyle_Wireframe,
  ObjGfxMeshStyle_Filled,
  ObjGfxMeshStyle_BBox
} ObjGfxMeshStyle;

void ObjGfxMeshes_plot(ObjGfxMeshes const *meshes, ObjGfxMeshesView const *ctx,
  PolyColData const *colours, ObjRef obj_no,
  Vertex centre, long int distance, Vertex3D pos,
  PaletteEntry const (*pal)[NumColours], BBox *bounding_box, ObjGfxMeshStyle style);

void ObjGfxMeshes_plot_poly_hill(ObjGfxMeshesView const *ctx,
  HillColData const *hill_colours,
  HillType type, unsigned char (*colours)[Hill_MaxPolygons],
  unsigned char (*heights)[HillCorner_Count],
  Vertex centre, long int distance, Vertex3D pos,
  PaletteEntry const (*pal)[NumColours], BBox *bounding_box, ObjGfxMeshStyle style);

void ObjGfxMeshes_plot_grid(ObjGfxMeshesView const *ctx, Vertex centre, long int distance, Vertex3D pos);
void ObjGfxMeshes_plot_mask(ObjGfxMeshesView const *ctx, Vertex centre, long int distance, Vertex3D pos);
void ObjGfxMeshes_plot_unknown(ObjGfxMeshesView const *ctx, Vertex centre, long int distance, Vertex3D pos);
void ObjGfxMeshes_plot_hill(ObjGfxMeshesView const *ctx, Vertex centre, long int distance, Vertex3D pos);
#endif
