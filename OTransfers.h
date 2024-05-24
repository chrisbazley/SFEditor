/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Ground object transfers
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef OTransfers_h
#define OTransfers_h

#include <stdbool.h>

#include "Obj.h"
#include "MapCoord.h"

typedef struct ObjTransfer ObjTransfer;
typedef struct ObjTransfers ObjTransfers;

struct ObjEditContext;
struct ObjGfxMeshes;
struct ObjEditChanges;
struct ObjEditSelection;
struct DFile;

void ObjTransfers_init(ObjTransfers *transfers_data);

int ObjTransfers_get_count(
  const ObjTransfers *transfers_data);

void ObjTransfers_load_all(ObjTransfers *transfers_data,
  const char *tiles_set);

void ObjTransfers_open_dir(ObjTransfers const *transfers_data);

void ObjTransfers_free(ObjTransfers *transfers_data);

ObjTransfer *ObjTransfers_grab_selection(struct ObjEditContext const *objects,
  struct ObjEditSelection *selected);

MapPoint ObjTransfers_get_dims(ObjTransfer const *transfer);

size_t ObjTransfers_get_trigger_count(ObjTransfer const *transfer);

bool ObjTransfers_can_plot_to_map(struct ObjEditContext const *objects,
                                        MapPoint bl, ObjTransfer *transfer,
                                        struct ObjGfxMeshes *meshes,
                                        struct ObjEditSelection *occluded);

bool ObjTransfers_plot_to_map(struct ObjEditContext const *objects, MapPoint bl,
  ObjTransfer *transfer, struct ObjGfxMeshes *meshes,
  struct ObjEditSelection *selection,
  struct ObjEditChanges *change_info);

void ObjTransfers_fill_map(struct ObjEditContext const *objects, MapPoint bl,
  ObjTransfer *transfer, ObjRef value,
  struct ObjGfxMeshes *meshes, struct ObjEditChanges *change_info);

void ObjTransfers_select(struct ObjEditSelection *selection,
  MapPoint bl, ObjTransfer *transfer,
  struct ObjEditContext const *objects);

ObjRef ObjTransfers_read_ref(ObjTransfer *transfer, MapPoint trans_pos);

ObjTransfer *ObjTransfers_find_by_name(ObjTransfers *transfers_data,
  const char *name, int *index_out);

ObjTransfer *ObjTransfers_find_by_index(ObjTransfers *transfers_data,
  int transfer_index);

ObjTransfer *ObjTransfer_create(void);

bool ObjTransfers_add(ObjTransfers *transfers_data,
  ObjTransfer *transfer, char const *filename,
  int *new_index_out);

void ObjTransfers_remove_and_delete_all(ObjTransfers *transfers_data);

void ObjTransfers_remove_and_delete(ObjTransfers *transfers_data,
  ObjTransfer *transfer_to_delete);

bool ObjTransfers_rename(ObjTransfers *transfers_data,
  ObjTransfer *transfer_to_rename,
  const char *new_name, int *new_index_out);

struct DFile *ObjTransfer_get_dfile(ObjTransfer *transfer);

#endif
