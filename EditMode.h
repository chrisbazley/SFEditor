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

struct EditWin;
struct Reader;
struct Writer;

typedef struct EditModeFuncts
{
  MapPoint coord_limit;
  DataType const *dragged_data_types;
  DataType const *import_data_types;
  DataType const *export_data_types;
  bool (*auto_select)(Editor *editor, MapPoint fine_pos, struct EditWin *edit_win);
  void (*auto_deselect)(Editor *editor);
  int  (*misc_event)(Editor *editor, int event_code);
  bool (*can_draw_grid)(Editor *editor, struct EditWin const *edit_win);
  void (*draw_grid)(Vertex map_origin, MapArea const *redraw_area,
                    struct EditWin const *edit_win);

  void (*leave)(Editor *editor);
  bool (*can_draw_numbers)(Editor *editor, struct EditWin const *edit_win);
  void (*draw_numbers)(Editor *editor, Vertex map_origin,
    MapArea const *redraw_area, struct EditWin const *edit_win);
  MapPoint (*map_to_grid_coords)(MapPoint map_coords, struct EditWin const *edit_win);
  MapArea (*map_to_grid_area)(MapArea const *map_area, struct EditWin const *edit_win);
  MapPoint (*grid_to_map_coords)(MapPoint grid_coords, struct EditWin const *edit_win);
  size_t (*num_selected)(Editor const *editor);
  size_t (*max_selected)(Editor const *editor);
  void (*resource_change)(Editor *editor, EditorChange event,
    EditorChangeParams const *params);
  void (*palette_selection)(Editor *editor, size_t object);
  bool (*can_clip_overlay)(Editor const *editor);
  void (*clip_overlay)(Editor *editor);
  bool (*can_smooth)(Editor const *editor);
  bool (*can_edit_properties)(Editor const *editor);
  void (*edit_properties)(Editor *editor, struct EditWin *edit_win);
  void (*paint_selected)(Editor *editor);
  bool (*anim_is_selected)(Editor const *editor);
  bool (*trigger_is_selected)(Editor const *editor);
  bool (*can_replace)(Editor const *editor);
  bool (*can_delete)(Editor const *editor);
  bool (*can_select_tool)(Editor const *editor, EditorTool tool);
  void (*tool_selected)(Editor *editor);
  void (*select_all)(Editor *editor);
  void (*clear_selection)(Editor *editor);
  void (*delete)(Editor *editor);
  bool (*cut)(Editor *editor);
  bool (*copy)(Editor *editor);
  bool (*start_pending_paste)(Editor *editor, struct Reader *reader, int estimated_size,
                      DataType data_type, char const *filename);

  void (*update_title)(Editor *editor);

  char *(*get_help_msg)(Editor const *editor);

  void (*pending_snake)(Editor *editor, MapPoint map_pos);
  void (*start_snake)(Editor *editor, MapPoint map_pos, bool inside);
  void (*draw_snake)(Editor *editor, MapPoint map_pos);

  void (*pending_sample_obj)(Editor *editor, MapPoint map_pos);
  void (*sample_obj)(Editor *editor, MapPoint fine_pos, MapPoint map_pos, EditWin const *edit_win);

  void (*pending_flood_fill)(Editor *editor, MapPoint fine_pos, MapPoint map_pos, EditWin const *edit_win);
  void (*flood_fill)(Editor *editor, MapPoint fine_pos, MapPoint map_pos, EditWin const *edit_win);

  void (*pending_global_replace)(Editor *editor, MapPoint fine_pos, MapPoint map_pos, EditWin const *edit_win);
  void (*global_replace)(Editor *editor, MapPoint fine_pos, MapPoint map_pos, EditWin const *edit_win);

  void (*pending_plot)(Editor *editor, MapPoint map_pos);

  void (*pending_line)(Editor *editor, MapPoint a, MapPoint b);
  void (*plot_line)(Editor *editor, MapPoint a, MapPoint b);

  void (*pending_rect)(Editor *editor, MapPoint a, MapPoint b);
  void (*plot_rect)(Editor *editor, MapPoint a, MapPoint b);

  void (*pending_circ)(Editor *editor, MapPoint a, MapPoint b);
  void (*plot_circ)(Editor *editor, MapPoint a, MapPoint b);

  void (*pending_tri)(Editor *editor, MapPoint a, MapPoint b, MapPoint c);
  void (*plot_tri)(Editor *editor, MapPoint a, MapPoint b, MapPoint c);

  void (*cancel_plot)(Editor *editor);

  void (*pending_smooth)(Editor *editor, int wand_size, MapPoint map_pos);
  void (*start_smooth)(Editor *editor, int wand_size, MapPoint map_pos);
  void (*draw_smooth)(Editor *editor, int wand_size, MapPoint last_map_pos, MapPoint map_pos);

  void (*pending_transfer)(Editor *editor, MapPoint map_pos);
  void (*draw_transfer)(Editor *editor, MapPoint map_pos);

  void (*pending_brush)(Editor *editor, int brush_size, MapPoint map_pos);
  void (*start_brush)(Editor *editor, int brush_size, MapPoint map_pos);
  void (*draw_brush)(Editor *editor, int brush_size, MapPoint last_map_pos, MapPoint map_pos);

  bool (*start_select)(Editor *editor, bool only_inside, MapPoint fine_pos, EditWin *edit_win);
  bool (*start_exclusive_select)(Editor *editor, bool only_inside, MapPoint fine_pos, EditWin *edit_win);
  void (*update_select)(Editor *editor, bool only_inside,
                        MapArea const *last_select_box,
                        MapArea const *select_box, struct EditWin const *edit_win);

  void (*cancel_select)(Editor *editor, bool only_inside,
                        MapArea const *last_select_box, struct EditWin *edit_win);

  /* Data export */
  bool (*start_drag_obj)(Editor *editor, MapPoint fine_pos, EditWin *edit_win);
  void (*cancel_drag_obj)(Editor *editor);
  bool (*drag_obj_remote)(Editor *editor, struct Writer *writer,
    DataType data_type, char const *filename);
  bool (*drag_obj_copy)(Editor *editor, MapArea const *bbox,
                        struct Editor const *drag_origin);

  void (*drag_obj_move)(Editor *editor, MapArea const *bbox, struct Editor *drag_origin);
  bool (*drag_obj_link)(Editor *editor, int window, int icon, struct Editor *drag_origin);

  /* Data import */
  bool (*show_ghost_drop)(Editor *editor, MapArea const *bbox,
                          struct Editor const *drag_origin);
  void (*hide_ghost_drop)(Editor *editor);
  bool (*drop)(Editor *editor, MapArea const *bbox, struct Reader *reader,
               int estimated_size, DataType data_type, char const *filename);

  void (*edit_properties_at_pos)(Editor *editor, MapPoint fine_pos, EditWin *edit_win);

  void (*pending_paste)(Editor *editor, MapPoint map_pos);
  bool (*draw_paste)(Editor *editor, MapPoint map_pos);
  void (*cancel_paste)(Editor *editor);

  bool (*can_create_transfer)(Editor const *editor);
  void (*create_transfer)(Editor *editor, const char *name);
  void (*wipe_ghost)(Editor *editor);

} EditModeFuncts;

#endif
