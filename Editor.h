/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  An editor instance
 *  Copyright (C) 2021 Christopher Bazley
 */

#ifndef Editor_h
#define Editor_h

#include <stdbool.h>

#include "Vertex.h"
#include "MapCoord.h"
#include "PalEntry.h"
#include "EditWin.h"
#include "ObjGfxMesh.h"
#include "DataType.h"

struct EditWin;
struct EditSession;
struct MapTex;
struct Reader;
struct Writer;

/* Editing modes */
typedef enum
{
  EDITING_MODE_NONE = -1,
  EDITING_MODE_FIRST,
  EDITING_MODE_MAP = EDITING_MODE_FIRST,
  EDITING_MODE_OBJECTS,
  EDITING_MODE_INFO,
  EDITING_MODE_SHIPS,
  EDITING_MODE_COUNT
}
EditMode;

typedef enum {
  Pending_None,
  Pending_Point,
  Pending_Line,
  Pending_Triangle,
  Pending_Rectangle,
  Pending_Circle,
  Pending_Transfer,
} PendingShape;

typedef struct Editor Editor;

bool Editor_init(Editor *editor, struct EditSession *session, Editor const *editor_to_copy);
void Editor_destroy(Editor *editor);
void Editor_update_title(Editor *editor);

bool Editor_get_tools_shown(Editor const *editor);
void Editor_set_tools_shown(Editor *editor, bool shown, struct EditWin *edit_win);

void Editor_resource_change(Editor *editor, EditorChange event,
                            EditorChangeParams const *params);

void Editor_auto_select(Editor *editor, struct EditWin *edit_win);
void Editor_auto_deselect(Editor *editor);
int Editor_misc_event(Editor *editor, int event_code);

bool Editor_mouse_click(Editor *editor, MapPoint fine_pos, int buttons,
                        bool shift, struct EditWin *edit_win);

void Editor_drag_select_ended(Editor *editor, MapArea const *select_box,
                              struct EditWin const *edit_win);

bool Editor_pointer_update(Editor *editor, MapPoint pointer_pos,
                           int button_held, struct EditWin const *edit_win);

bool Editor_can_draw_grid(Editor *editor, struct EditWin const *edit_win);
void Editor_draw_grid(Editor *editor, Vertex map_origin,
 MapArea const *const redraw_area, struct EditWin const *edit_win);

bool Editor_can_draw_numbers(Editor *editor, struct EditWin const *edit_win);
void Editor_draw_numbers(Editor *editor, Vertex map_origin,
                         MapArea const *const redraw_area, struct EditWin const *edit_win);

MapPoint Editor_map_to_grid_coords(Editor const *editor, MapPoint map_coords, struct EditWin const *edit_win);
MapArea Editor_map_to_grid_area(Editor const *editor, MapArea const *const map_area, struct EditWin const *edit_win);
MapPoint Editor_grid_to_map_coords(Editor const *editor, MapPoint grid_coords, struct EditWin const *edit_win);

size_t Editor_num_selected(Editor const *editor);
size_t Editor_max_selected(Editor const *editor);
char const *Editor_get_mode_name(Editor const *editor);
EditMode Editor_get_edit_mode(Editor const *editor);
int Editor_get_coord_field_width(Editor const *editor);
MapPoint Editor_get_coord_limit(Editor const *editor);
bool Editor_can_set_edit_mode(Editor *editor, EditMode new_mode);
bool Editor_set_edit_mode(Editor *editor, EditMode new_mode, struct EditWin *edit_win);

bool Editor_get_pal_shown(Editor const *editor);
void Editor_set_pal_shown(Editor *editor, bool shown, struct EditWin *edit_win);

void Editor_pal_was_hidden(Editor *editor);
void Editor_reveal_palette(Editor *editor);

void Editor_display_msg(Editor *editor, const char *hint, bool temp);
struct EditSession *Editor_get_session(Editor const *editor);

void Editor_set_help_and_ptr(Editor *editor);

bool Editor_can_clip_overlay(Editor const *editor);
void Editor_clip_overlay(Editor *editor);
bool Editor_can_create_transfer(Editor const *editor);
bool Editor_can_replace(Editor const *editor);
bool Editor_can_smooth(Editor const *editor);
bool Editor_anim_is_selected(Editor const *editor);
bool Editor_can_edit_properties(Editor const *editor);
void Editor_edit_properties(Editor *editor, struct EditWin *edit_win);
bool Editor_can_delete(Editor const *editor);
bool Editor_trigger_is_selected(Editor const *editor);
void Editor_paint_selected(Editor *editor);

typedef enum {
  EDITORTOOL_NONE = -1,
  EDITORTOOL_FIRST,
  EDITORTOOL_BRUSH = EDITORTOOL_FIRST,
  EDITORTOOL_FILLREPLACE,
  EDITORTOOL_PLOTSHAPES,
  EDITORTOOL_SAMPLER,
  EDITORTOOL_SNAKE,
  EDITORTOOL_SMOOTHWAND,
  EDITORTOOL_TRANSFER,
  EDITORTOOL_SELECT,
  EDITORTOOL_MAGNIFIER,
  EDITORTOOL_COUNT
} EditorTool;

void Editor_set_palette_rotation(Editor *editor, ObjGfxAngle rot);
ObjGfxAngle Editor_get_palette_rotation(Editor const *editor);
void Editor_palette_selection(Editor *editor, size_t object);

bool Editor_can_select_tool(Editor const *editor, EditorTool tool);
void Editor_select_tool(Editor *editor, EditorTool tool);
EditorTool Editor_get_tool(Editor const *editor);

int Editor_estimate_clipboard(DataType data_type);
bool Editor_write_clipboard(struct Writer *writer, DataType data_type, char const *filename);
void Editor_free_clipboard(void);

void Editor_select_all(Editor *editor);
void Editor_clear_selection(Editor *editor);

void Editor_delete(Editor *editor);
bool Editor_cut(Editor *editor);
bool Editor_copy(Editor *editor);
bool Editor_start_pending_paste(Editor *editor, struct Reader *reader, int estimated_size,
  DataType data_type, char const *filename);

bool Editor_allow_drop(Editor const *editor);
DataType const *Editor_get_dragged_data_types(Editor const *editor);
DataType const *Editor_get_import_data_types(Editor const *editor);
DataType const *Editor_get_export_data_types(Editor const *editor);

void Editor_set_paste_enabled(Editor *editor, bool can_paste);
bool Editor_allow_paste(Editor const *editor);

bool Editor_show_ghost_drop(Editor *editor, MapArea const *bbox,
                            Editor const *drag_origin);

void Editor_hide_ghost_drop(Editor *editor);
bool Editor_drop(Editor *editor, MapArea const *bbox, struct Reader *reader,
  int estimated_size, DataType data_type, char const *filename);

bool Editor_drag_obj_remote(Editor *editor, struct Writer *writer, DataType data_type,
  char const *filename);

void Editor_drag_obj_move(Editor *editor, MapArea const *bbox, Editor *drag_origin);
bool Editor_drag_obj_copy(Editor *editor, MapArea const *bbox, Editor *drag_origin);
void Editor_cancel_drag_obj(Editor *editor);
bool Editor_drag_obj_link(Editor *editor, int window, int icon, Editor *drag_origin);

typedef enum {
  PLOTSHAPE_NONE = -1,
  PLOTSHAPE_FIRST,
  PLOTSHAPE_LINE = PLOTSHAPE_FIRST,
  PLOTSHAPE_CIRCLE,
  PLOTSHAPE_TRIANGLE,
  PLOTSHAPE_RECTANGLE,
  PLOTSHAPE_COUNT
} PlotShape;

void Editor_set_fill_is_global(Editor *editor, bool global_fill);
bool Editor_get_fill_is_global(Editor const *editor);
PlotShape Editor_get_plot_shape(Editor const *editor);
void Editor_set_plot_shape(Editor *editor, PlotShape shape_to_plot);
int Editor_get_brush_size(Editor const *editor);
void Editor_set_brush_size(Editor *editor, int size);
int Editor_get_wand_size(Editor const *editor);
void Editor_set_wand_size(Editor *editor, int size);
char *Editor_get_help_msg(Editor const *editor);
PointerType Editor_get_ptr_type(Editor const *editor);
char const *Editor_get_tool_msg(Editor *editor, EditorTool tool, bool caps);

void Editor_redraw_map(Editor *editor, MapArea const *area);
void Editor_redraw_object(Editor *editor, MapPoint pos, ObjRef obj_ref, bool has_triggers);
void Editor_redraw_info(Editor *editor, MapPoint pos);
void Editor_occluded_obj_changed(Editor *editor, MapPoint pos, ObjRef obj_ref);
void Editor_occluded_info_changed(Editor *editor, MapPoint pos);
void Editor_redraw_ghost(Editor *editor);
void Editor_clear_ghost_bbox(Editor *editor);
void Editor_set_ghost_map_bbox(Editor *editor, MapArea const *area);
void Editor_add_ghost_obj(Editor *editor, MapPoint pos, ObjRef obj_ref);
void Editor_add_ghost_info(Editor *editor, MapPoint pos);
void Editor_add_ghost_unknown_obj(Editor *editor, MapArea const *bbox);
void Editor_add_ghost_unknown_info(Editor *editor, MapArea const *bbox);
void Editor_redraw_pending(Editor *editor, bool immediate);

bool Editor_can_create_transfer(Editor const *editor);
void Editor_create_transfer(Editor *editor, const char *name);

void Editor_cancel(Editor *editor, EditWin *edit_win);
void Editor_wipe_ghost(Editor *editor);

#endif
