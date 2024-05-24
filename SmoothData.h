/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map tiles smoothing and groups data
 *  Copyright (C) 2019 Chris Bazley
 */

#ifndef SmoothData_h
#define SmoothData_h

#include <stdbool.h>

/*
  The array of TileSmoothData allows us to quickly find the group number of a
given tile. Each element is a TileSmoothData block that contains the NESW edge
data for use of the smoothing wand.

  The array of TexGroupRoot elements allows us to quickly find the tiles in a
given group. Each block contains a pointer to a flex array of group members.
*/

typedef struct
{
  bool  super;
  size_t count;
  /* flex anchor for array of tile or group numbers in definition order */
  void *array_anchor;
} TexGroupRoot;

struct MapTexGroups {
  size_t          count;
  TexGroupRoot   *array; /* malloced array of TexGroupRoots, one for each group */
  size_t ntiles;
  void *smooth_anchor; /* flex anchor for an array of
                          TileSmoothData, in tile number order */
};

#endif
