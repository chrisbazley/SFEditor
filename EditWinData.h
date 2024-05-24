/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Private data of editing window
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef EditWinData_h
#define EditWinData_h

#include <stdbool.h>

#include "toolbox.h"

#include "Scheduler.h"
#include "PalEntry.h"

#include "Vertex.h"
#include "MapCoord.h"
#include "StatusBar.h"
#include "EditWin.h"
#include "SFInit.h"
#include "Hill.h"
#include "ObjEditCtx.h"
#include "MapEditCtx.h"
#include "View.h"
#include "MapTexBitm.h"
#include "MapAreaColData.h"

struct EditWin
{
  ObjectId window_id;
  int wimp_id, button_held, dragclaim_msg_ref;
  SchedulerTime last_scroll; /* time of last auto-scroll update */

  StatusBarData statusbar_data;

  MapPoint start_drag_pos, old_grid_pos;
  MapArea sent_drag_bbox, shown_drag_bbox, drop_bbox, ghost_bbox;
  Vertex drop_pos;

  bool pointer_trapped:1, snap_horiz:1, snap_vert:1, mouse_in:1,
       wimp_drag_box:1, obj_drag_box:1, dragging_obj:1,
       auto_scrolling:1, null_poller:1, has_input_focus:1,
       has_hills:1;

  Vertex extent;
  struct Editor *editor;
  struct EditSession *session;
  PointerType pointer;

  void (*can_paste_fn)(void *arg, bool cb_valid), *can_paste_arg;

  HillsData hills;
  struct MapAreaColData pending_redraws, ghost_bboxes;
  MapArea pending_hills_update;

  struct ObjEditContext read_obj_ctx;
  struct MapEditContext read_map_ctx;
  struct InfoEditContext const *read_info_ctx;
  char sel_tex_bw_table[(MapTexMax + CHAR_BIT - 1) / CHAR_BIT];

  View view;
};

#endif
