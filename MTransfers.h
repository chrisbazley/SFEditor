/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground map transfers
 *  Copyright (C) 2001 Christopher Bazley
 */

#ifndef MTransfers_h
#define MTransfers_h

#include <stdbool.h>

#include "MapCoord.h"
#include "Map.h"
#include "MapTexBitm.h"
#include "DFile.h"

enum
{
  TransfersThumbWidth = 7 * MapTexSize,
  TransfersThumbHeight = 6 * MapTexSize, /* in pixels */
};

typedef struct MapTransfer MapTransfer;
typedef struct MapTransfers MapTransfers;

struct MapEditContext;
struct MapEditChanges;
struct MapEditSelection;
struct DFile;

void MapTransfers_init(MapTransfers *transfers_data);

size_t MapTransfers_get_count(
  const MapTransfers *transfers_data);

bool MapTransfers_ensure_thumbnails(MapTransfers *transfers_data,
  MapTexBitmaps *textures);

void MapTransfers_load_all(MapTransfers *transfers_data,
  const char *tiles_set);

void MapTransfers_open_dir(MapTransfers const *transfers_data);

void MapTransfers_free(MapTransfers *transfers_data);

MapTransfer *MapTransfers_grab_selection(struct MapEditContext const *map,
  struct MapEditSelection *selected);

MapPoint MapTransfers_get_dims(MapTransfer const *transfer);

size_t MapTransfers_get_anim_count(MapTransfer const *transfer);

bool MapTransfers_plot_to_map(const struct MapEditContext *map, MapPoint bl,
  MapTransfer *transfer,
  struct MapEditSelection *selection,
  struct MapEditChanges *change_info);

MapArea MapTransfers_get_bbox(MapPoint const bl, MapTransfer *const transfer);

void MapTransfers_fill_map(const struct MapEditContext *map, MapPoint bl,
  MapTransfer *transfer, MapRef value, struct MapEditChanges *change_info);

void MapTransfers_select(struct MapEditSelection *selection,
  MapPoint bl, MapTransfer *transfer);

MapRef MapTransfers_read_ref(MapTransfer *transfer,
  MapPoint trans_pos);

MapTransfer *MapTransfers_find_by_name(MapTransfers *transfers_data,
  const char *name, size_t *index_out);

MapTransfer *MapTransfers_find_by_index(MapTransfers *transfers_data,
  size_t transfer_index);

MapTransfer *MapTransfer_create(void);

bool MapTransfers_add(MapTransfers *transfers_data,
  MapTransfer *transfer, char const *filename,
  size_t *new_index_out, MapTexBitmaps *textures);

void MapTransfers_remove_and_delete_all(MapTransfers *transfers_data);

void MapTransfers_remove_and_delete(MapTransfers *transfers_data,
  MapTransfer *transfer_to_delete, bool shrink_area);

bool MapTransfers_rename(MapTransfers *transfers_data,
  MapTransfer *transfer_to_rename,
  const char *new_name, size_t *new_index_out);

struct DFile *MapTransfer_get_dfile(MapTransfer *transfer);

#endif
