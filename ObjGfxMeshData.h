/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Polygonal graphics set data
 *  Copyright (C) 2021 Chris Bazley
 */

#ifndef ObjGfxMeshData_h
#define ObjGfxMeshData_h

#include <stdbool.h>
#include "MapCoord.h"

enum {
  MaxPlotType = 10,
  MaxPlotCommands = 16, /* unknown what the game limit is */
};

typedef struct ObjGfxMesh ObjGfxMesh;

typedef struct
{
  size_t ocount;
  size_t oalloc;
  ObjGfxMesh **objects;
} ObjGfxMeshArray;

typedef enum
{
  PlotAction_FacingAlways, /* Always plot facing facets in the group specified
                              by bits 0-4. */
  PlotAction_FacingIf,     /* Plot facing facets in the group specified by the
                              next byte if the vector test specified by bits
                              0-4 passes. */
  PlotAction_FacingIfNot,  /* Plot facing facets in the group specified by the
                              next byte if the vector test specified by bits
                              0-4 fails. */
  PlotAction_AllIf,        /* Plot all facets in the group specified by the
                              next byte if the vector test specified by bits
                              0-4 passes. */
  PlotAction_AllIfNot      /* Plot all facets in the group specified by the
                              next byte if the vector test specified by bits
                              0-4 fails. */
}
PlotAction;

typedef struct {
  PlotAction action;
  unsigned char group;
  unsigned char polygon;
} PlotCommand;

typedef struct {
  unsigned char max_polygon;
  unsigned char num_commands;
  unsigned char group_mask;
  PlotCommand commands[MaxPlotCommands];
} PlotType;

struct ObjGfxMeshes
{
  int num_plot_types;
  PlotType plot_types[MaxPlotType];
  MapArea max_bounding_box[MapAngle_Count];
  bool have_max_collision_size;
  MapPoint max_collision_size;
  ObjGfxMeshArray ships;
  ObjGfxMeshArray ground;
};

#endif
