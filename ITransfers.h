/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information transfers
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef ITransfers_h
#define ITransfers_h

#include "MapCoord.h"

typedef struct InfoTransfer InfoTransfer;

struct SelectionBitmask;
struct InfoEditContext;
struct InfoEditChanges;
struct DFile;

InfoTransfer *InfoTransfers_grab_selection(
  struct InfoEditContext const *infos,
  struct SelectionBitmask *selected);

MapPoint InfoTransfers_get_origin(InfoTransfer const *transfer);
MapPoint InfoTransfers_get_dims(InfoTransfer const *transfer);

size_t InfoTransfers_get_info_count(InfoTransfer const *transfer);

MapPoint InfoTransfers_get_pos(InfoTransfer const *transfer, size_t index);

bool InfoTransfers_plot_to_map(struct InfoEditContext const *infos,
  MapPoint bl, InfoTransfer *transfer,
  struct SelectionBitmask *selected,
  struct InfoEditChanges *change_info);

void InfoTransfers_find_occluded(struct InfoEditContext const *infos,
                                   MapPoint bl,
                                   InfoTransfer *transfer,
                                   struct SelectionBitmask *occluded);

InfoTransfer *InfoTransfer_create(void);

struct DFile *InfoTransfer_get_dfile(InfoTransfer *transfer);

#endif
