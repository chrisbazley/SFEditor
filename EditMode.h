/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Editing mode interface
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef EditMode_h
#define EditMode_h

#include <stdbool.h>
#include "PalEntry.h"
#include "Vertex.h"
#include "MapCoord.h"
#include "Editor.h"
#include "DataType.h"

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct EditWin;
struct Reader;
struct Writer;

typedef bool EditModeAutoSelectFn(Editor *editor, MapPoint fine_pos,
                                  struct EditWin *edit_win);

typedef void EditModeAutoDeselectFn(Editor *editor);
typedef int  EditModeMiscEventFn(Editor *editor, int event_code);
typedef bool EditModeCanDrawGridFn(Editor *editor,
                                   struct EditWin const *edit_win);

typedef void EditModeDrawGridFn(Vertex map_origin, MapArea const *redraw_area,
                                struct EditWin const *edit_win);

typedef void EditModeLeaveFn(Editor *editor);

typedef bool EditModeCanDrawNumbersFn(Editor *editor,
                                      struct EditWin const *edit_win);

typedef void EditModeDrawNumbersFn(Editor *editor, Vertex map_origin,
                                   MapArea const *redraw_area,
                                   struct EditWin const *edit_win);

typedef MapPoint EditModeMapToGridCoordsFn(MapPoint map_coords,
                                           struct EditWin const *edit_win);

typedef MapArea EditModeMapToGridAreaFn(MapArea const *map_area,
                                        struct EditWin const *edit_win);

typedef MapPoint EditModeGridToMapCoordsFn(MapPoint grid_coords,
                                           struct EditWin const *edit_win);

typedef size_t EditModeNumSelectedFn(Editor const *editor);
typedef size_t EditModeMaxSelectedFn(Editor const *editor);

typedef void EditModeResourceChangeFn(Editor *editor, EditorChange event,
                                      EditorChangeParams const *params);

typedef void EditModePaletteSelectionFn(Editor *editor, int object);
typedef bool EditModeCanClipOverlayFn(Editor const *editor);
typedef void EditModeClipOverlayFn(Editor *editor);
typedef bool EditModeCanSmoothFn(Editor const *editor);
typedef bool EditModeCanEditPropertiesFn(Editor const *editor);
typedef void EditModeEditPropertiesFn(Editor *editor, struct EditWin *edit_win);
typedef void EditModePaintSelectedFn(Editor *editor);
typedef bool EditModeAnimIsSelectedFn(Editor const *editor);
typedef bool EditModeTriggerIsSelectedFn(Editor const *editor);
typedef bool EditModeCanReplaceFn(Editor const *editor);
typedef bool EditModeCanDeleteFn(Editor const *editor);
typedef bool EditModeCanSelectToolFn(Editor const *editor, EditorTool tool);
typedef void EditModeToolSelectedFn(Editor *editor);
typedef void EditModeSelectAllFn(Editor *editor);
typedef void EditModeClearSelectionFn(Editor *editor);
typedef void EditModeDeleteFn(Editor *editor);
typedef bool EditModeCutFn(Editor *editor);
typedef bool EditModeCopyFn(Editor *editor);

typedef bool EditModeStartPendingPasteFn(Editor *editor, struct Reader *reader,
                                         int estimated_size,
                                         DataType data_type,
                                         char const *filename);

typedef void EditModeUpdateTitleFn(Editor *editor);

typedef _Optional char *EditModeGetHelpMsgFn(Editor const *editor);

typedef void EditModePendingSnakeFn(Editor *editor, MapPoint map_pos);
typedef void EditModeStartSnakeFn(Editor *editor, MapPoint map_pos, bool inside);
typedef void EditModeDrawSnakeFn(Editor *editor, MapPoint map_pos);

typedef void EditModePendingSampleObjFn(Editor *editor, MapPoint map_pos);

typedef void EditModeSampleObjFn(Editor *editor, MapPoint fine_pos,
                                 MapPoint map_pos, EditWin const *edit_win);

typedef void EditModePendingFloodFillFn(Editor *editor, MapPoint fine_pos,
                                        MapPoint map_pos,
                                        EditWin const *edit_win);

typedef void EditModeFloodFillFn(Editor *editor, MapPoint fine_pos,
                                 MapPoint map_pos, EditWin const *edit_win);

typedef void EditModePendingGlobalReplaceFn(Editor *editor, MapPoint fine_pos,
                                            MapPoint map_pos,
                                            EditWin const *edit_win);

typedef void EditModeGlobalReplaceFn(Editor *editor, MapPoint fine_pos,
                                     MapPoint map_pos, EditWin const *edit_win);

typedef void EditModePendingPlotFn(Editor *editor, MapPoint map_pos);

typedef void EditModePendingLineFn(Editor *editor, MapPoint a, MapPoint b);
typedef void EditModePlotLineFn(Editor *editor, MapPoint a, MapPoint b);

typedef void EditModePendingRectFn(Editor *editor, MapPoint a, MapPoint b);
typedef void EditModePlotRectFn(Editor *editor, MapPoint a, MapPoint b);

typedef void EditModePendingCircFn(Editor *editor, MapPoint a, MapPoint b);
typedef void EditModePlotCircFn(Editor *editor, MapPoint a, MapPoint b);

typedef void EditModePendingTriFn(Editor *editor, MapPoint a, MapPoint b,
                                  MapPoint c);

typedef void EditModePlotTriFn(Editor *editor, MapPoint a, MapPoint b,
                               MapPoint c);

typedef void EditModeCancelPlotFn(Editor *editor);

typedef void EditModePendingSmoothFn(Editor *editor, int wand_size, MapPoint map_pos);
typedef void EditModeStartSmoothFn(Editor *editor, int wand_size, MapPoint map_pos);

typedef void EditModeDrawSmoothFn(Editor *editor, int wand_size,
                                  MapPoint last_map_pos, MapPoint map_pos);

typedef void EditModePendingTransferFn(Editor *editor, MapPoint map_pos);
typedef void EditModeDrawTransferFn(Editor *editor, MapPoint map_pos);

typedef void EditModePendingBrushFn(Editor *editor, int brush_size, MapPoint map_pos);
typedef void EditModeStartBrushFn(Editor *editor, int brush_size, MapPoint map_pos);

typedef void EditModeDrawBrushFn(Editor *editor, int brush_size,
                                 MapPoint last_map_pos, MapPoint map_pos);

typedef bool EditModeStartSelectFn(Editor *editor, bool only_inside,
                                   MapPoint fine_pos, EditWin *edit_win);

typedef bool EditModeStartExclusiveSelectFn(Editor *editor, bool only_inside,
                                            MapPoint fine_pos, EditWin *edit_win);

typedef void EditModeUpdateSelectFn(Editor *editor, bool only_inside,
                                    MapArea const *last_select_box,
                                    MapArea const *select_box, struct EditWin const *edit_win);

typedef void EditModeCancelSelectFn(Editor *editor, bool only_inside,
                                    MapArea const *last_select_box, struct EditWin *edit_win);

/* Data export */
typedef bool EditModeStartDragObjFn(Editor *editor, MapPoint fine_pos, EditWin *edit_win);
typedef void EditModeCancelDragObjFn(Editor *editor);
typedef bool EditModeDragObjRemoteFn(Editor *editor, struct Writer *writer,
                                     DataType data_type, char const *filename);
typedef bool EditModeDragObjCopyFn(Editor *editor, MapArea const *bbox,
                                   struct Editor const *drag_origin);

typedef void EditModeDragObjMoveFn(Editor *editor, MapArea const *bbox, struct Editor *drag_origin);
typedef bool EditModeDragObjLinkFn(Editor *editor, int window, int icon, struct Editor *drag_origin);

/* Data import */
typedef bool EditModeShowGhostDropFn(Editor *editor, MapArea const *bbox,
                                     _Optional struct Editor const *drag_origin);

typedef void EditModeHideGhostDropFn(Editor *editor);
typedef bool EditModeDropFn(Editor *editor, MapArea const *bbox, struct Reader *reader,
                            int estimated_size, DataType data_type, char const *filename);

typedef void EditModeEditPropertiesAtPosFn(Editor *editor, MapPoint fine_pos, EditWin *edit_win);

typedef void EditModePendingPasteFn(Editor *editor, MapPoint map_pos);
typedef bool EditModeDrawPasteFn(Editor *editor, MapPoint map_pos);
typedef void EditModeCancelPasteFn(Editor *editor);

typedef bool EditModeCanCreateTransferFn(Editor const *editor);
typedef void EditModeCreateTransferFn(Editor *editor, const char *name);
typedef void EditModeWipeGhostFn(Editor *editor);

typedef struct EditModeFuncts
{
  MapPoint coord_limit;
  DataType const *dragged_data_types, *import_data_types, *export_data_types;
  _Optional EditModeAutoSelectFn *auto_select;
  _Optional EditModeAutoDeselectFn *auto_deselect;
  _Optional EditModeMiscEventFn *misc_event;
  _Optional EditModeCanDrawGridFn *can_draw_grid;
  _Optional EditModeDrawGridFn *draw_grid;

  EditModeLeaveFn *leave;

  _Optional EditModeCanDrawNumbersFn *can_draw_numbers;
  _Optional EditModeDrawNumbersFn *draw_numbers;
  _Optional EditModeMapToGridCoordsFn *map_to_grid_coords;
  _Optional EditModeMapToGridAreaFn *map_to_grid_area;
  _Optional EditModeGridToMapCoordsFn *grid_to_map_coords;
  _Optional EditModeNumSelectedFn *num_selected;
  _Optional EditModeMaxSelectedFn *max_selected;
  _Optional EditModeResourceChangeFn *resource_change;
  _Optional EditModePaletteSelectionFn *palette_selection;
  _Optional EditModeCanClipOverlayFn *can_clip_overlay;
  _Optional EditModeClipOverlayFn *clip_overlay;
  _Optional EditModeCanSmoothFn *can_smooth;
  _Optional EditModeCanEditPropertiesFn *can_edit_properties;
  _Optional EditModeEditPropertiesFn *edit_properties;
  _Optional EditModePaintSelectedFn *paint_selected;
  _Optional EditModeAnimIsSelectedFn *anim_is_selected;
  _Optional EditModeTriggerIsSelectedFn *trigger_is_selected;
  _Optional EditModeCanReplaceFn *can_replace;
  _Optional EditModeCanDeleteFn *can_delete;
  _Optional EditModeCanSelectToolFn *can_select_tool;
  _Optional EditModeToolSelectedFn *tool_selected;
  _Optional EditModeSelectAllFn *select_all;
  _Optional EditModeClearSelectionFn *clear_selection;
  _Optional EditModeDeleteFn *delete;
  _Optional EditModeCutFn *cut;
  _Optional EditModeCopyFn *copy;
  _Optional EditModeStartPendingPasteFn *start_pending_paste;

  _Optional EditModeUpdateTitleFn *update_title;

  _Optional EditModeGetHelpMsgFn *get_help_msg;

  _Optional EditModePendingSnakeFn *pending_snake;
  _Optional EditModeStartSnakeFn *start_snake;
  _Optional EditModeDrawSnakeFn *draw_snake;

  _Optional EditModePendingSampleObjFn *pending_sample_obj;
  _Optional EditModeSampleObjFn *sample_obj;

  _Optional EditModePendingFloodFillFn *pending_flood_fill;
  _Optional EditModeFloodFillFn *flood_fill;

  _Optional EditModePendingGlobalReplaceFn *pending_global_replace;
  _Optional EditModeGlobalReplaceFn *global_replace;

  _Optional EditModePendingPlotFn *pending_plot;

  _Optional EditModePendingLineFn *pending_line;
  _Optional EditModePlotLineFn *plot_line;

  _Optional EditModePendingRectFn *pending_rect;
  _Optional EditModePlotRectFn *plot_rect;

  _Optional EditModePendingCircFn *pending_circ;
  _Optional EditModePlotCircFn *plot_circ;

  _Optional EditModePendingTriFn *pending_tri;
  _Optional EditModePlotTriFn *plot_tri;

  _Optional EditModeCancelPlotFn *cancel_plot;

  _Optional EditModePendingSmoothFn *pending_smooth;
  _Optional EditModeStartSmoothFn *start_smooth;
  _Optional EditModeDrawSmoothFn *draw_smooth;

  _Optional EditModePendingTransferFn *pending_transfer;
  _Optional EditModeDrawTransferFn *draw_transfer;

  _Optional EditModePendingBrushFn *pending_brush;
  _Optional EditModeStartBrushFn *start_brush;
  _Optional EditModeDrawBrushFn *draw_brush;

  _Optional EditModeStartSelectFn *start_select;
  _Optional EditModeStartExclusiveSelectFn *start_exclusive_select;
  _Optional EditModeUpdateSelectFn *update_select;

  _Optional EditModeCancelSelectFn *cancel_select;

  /* Data export */
  _Optional EditModeStartDragObjFn *start_drag_obj;
  _Optional EditModeCancelDragObjFn *cancel_drag_obj;
  _Optional EditModeDragObjRemoteFn *drag_obj_remote;
  _Optional EditModeDragObjCopyFn *drag_obj_copy;

  _Optional EditModeDragObjMoveFn *drag_obj_move;
  _Optional EditModeDragObjLinkFn *drag_obj_link;

  /* Data import */
  _Optional EditModeShowGhostDropFn *show_ghost_drop;
  _Optional EditModeHideGhostDropFn *hide_ghost_drop;
  _Optional EditModeDropFn *drop;

  _Optional EditModeEditPropertiesAtPosFn *edit_properties_at_pos;

  _Optional EditModePendingPasteFn *pending_paste;
  _Optional EditModeDrawPasteFn *draw_paste;
  _Optional EditModeCancelPasteFn *cancel_paste;

  _Optional EditModeCanCreateTransferFn *can_create_transfer;
  _Optional EditModeCreateTransferFn *create_transfer;
  _Optional EditModeWipeGhostFn *wipe_ghost;
} EditModeFuncts;

#endif
