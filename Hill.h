/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Procedural generation of hills
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef hill_h
#define hill_h

#include <limits.h>

#include "SFError.h"
#include "MapCoord.h"
#include "Obj.h"
#include "Hill.h"

enum {
  Hill_MaxPolygons = 2,
  Hill_PolygonNumSides = 3,
  Hill_ObjPerHillLog2 = 1,
  Hill_ObjPerHill = 1 << Hill_ObjPerHillLog2,
  Hill_SizeLog2 = Obj_SizeLog2 - Hill_ObjPerHillLog2,
  Hill_Size = 1 << Hill_SizeLog2,
  Hill_MaxHeight = 47,
};

typedef enum {
  HillType_None,
  HillType_ABCA_ACDA, // colours swapped on map in game (bug)
  HillType_ABDA_BCDB,
  HillType_ABDA,
  HillType_ABCA,
  HillType_BCDB,
  HillType_CDAC,
  HillType_Count
} HillType;

typedef enum {
  HillCorner_A,
  HillCorner_B,
  HillCorner_C,
  HillCorner_D,
  HillCorner_Count
} HillCorner;

struct EditWin;

typedef bool HillReadFn(struct EditWin const *edit_win, MapPoint map_pos);

typedef void HillRedrawFn(struct EditWin *edit_win, MapPoint map_pos,
  HillType old_type, unsigned char (*old_heights)[HillCorner_Count],
  HillType new_type, unsigned char (*new_heights)[HillCorner_Count]);

typedef struct HillsData {
  HillReadFn *read_hill_cb;
  HillRedrawFn *redraw_cb;
  struct EditWin *edit_win;
  void *data;
#if 0
  unsigned char swap_row_mixer[Hill_Size / CHAR_BIT];
#endif
} HillsData;

SFError hills_init(HillsData *hills,
  HillReadFn *read_hill_cb, HillRedrawFn *redraw_cb, struct EditWin *edit_win);

void hills_destroy(HillsData *hills);

void hills_make(HillsData *hills);
void hills_update(HillsData *hills, MapArea const *changed_area);

HillType hills_read(HillsData const *hills, MapPoint pos,
  unsigned char (*colours)[Hill_MaxPolygons],
  unsigned char (*heights)[HillCorner_Count]);

static inline bool hills_coord_in_range(const MapCoord x)
{
  return (x < Hill_Size) && (x >= 0);
}

static inline bool hills_coords_in_range(MapPoint const pos)
{
  return hills_coord_in_range(pos.x) &&
         hills_coord_in_range(pos.y);
}

static inline MapCoord hills_wrap_coord(const MapCoord x)
{
  MapCoord const wrapped_x = (unsigned long)x % Hill_Size;
  if (wrapped_x != x) {
    DEBUGF("Wrap hill X %" PRIMapCoord " to %" PRIMapCoord "\n", x, wrapped_x);
  }
  assert(hills_coord_in_range(wrapped_x));
  return wrapped_x;
}

static inline MapPoint hills_wrap_coords(MapPoint const pos)
{
  return (MapPoint){hills_wrap_coord(pos.x),
                    hills_wrap_coord(pos.y)};
}

static inline size_t hill_coords_to_index(MapPoint const pos)
{
  return (size_t)hills_wrap_coord(pos.x) + (size_t)(hills_wrap_coord(pos.y) * Hill_Size);
}

static inline bool hills_split_area(MapArea const *const area,
                        bool (*const callback)(MapArea const *, void *),
                        void *const cb_arg)
{
  return MapArea_split(area, Obj_SizeLog2 - Hill_ObjPerHillLog2,
                       callback, cb_arg);
}

#endif
