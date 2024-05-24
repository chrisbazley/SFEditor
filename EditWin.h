/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Editing window
 *  Copyright (C) 2001 Christopher Bazley
 */

/* "Its a livin' thing! It's a terrible thing to lose..." */
#ifndef EditWin_h
#define EditWin_h

#include <stdbool.h>

#include "toolbox.h"
#include "PalEntry.h"
#include "Vertex.h"
#include "MapCoord.h"
#include "SFInit.h"
#include "Triggers.h"
#include "ObjEditCtx.h"
#include "MapEditCtx.h"
#include "ObjGfxMesh.h"
#include "View.h"
#include "Map.h"

struct EditSession;
struct Editor;
struct HillsData;

typedef enum {
  Pointer_Standard,
  Pointer_Brush,
  Pointer_Fill,
  Pointer_Snake,
  Pointer_Wand,
  Pointer_Paste,
  Pointer_Sample,
  Pointer_Zoom,
  Pointer_Crosshair,
} PointerType;

typedef struct EditWin EditWin;

/* The following definitions are for ButtonType_DoubleClickDrag (10) */
#define BUTTONS_DRAG(x) ((x) * 16)
#define BUTTONS_SINGLE(x) ((x) * 256)
#define BUTTONS_DOUBLE(x) (x)
#define BUTTONS_CLICK(x) (BUTTONS_SINGLE(x) | BUTTONS_DOUBLE(x))

typedef enum {
#define DECLARE_CHANGE(c) EDITOR_CHANGE_ ## c,
#include "DeclChange.h"
#undef DECLARE_CHANGE
} EditorChange;

char const *EditorChange_to_string(EditorChange event);

typedef union {
  struct {
    size_t index;
  } transfer_added, transfer_deleted, transfer_replaced;
  struct {
    size_t index, new_index;
  } transfer_renamed;
  struct {
    MapArea bbox;
  } obj_prechange, map_prechange;
  struct {
    MapPoint old_pos;
    MapPoint new_pos;
  } obj_premove, map_premove;
  struct {
    size_t index;
    struct TargetInfo const *info;
  } info_added, info_predelete;
  struct {
    struct TargetInfo const *info;
    size_t old_index;
    size_t new_index;
    MapPoint old_pos;
  } info_moved;
} EditorChangeParams;

bool EditWin_menu_needs_update(EditWin const *edit_win, ObjectId self_id);

bool EditWin_init(EditWin *edit_win, struct Editor *editor, EditWin const *edit_win_to_copy);
void EditWin_destroy(EditWin *edit_win);
void EditWin_show(EditWin const *edit_win);

void EditWin_set_title(EditWin *edit_win, char const *file_path);

void EditWin_close(EditWin *edit_win);
struct EditSession *EditWin_get_session(EditWin const *edit_win);
void EditWin_display_hint(EditWin *edit_win, char const *hint);
void EditWin_display_mode(EditWin *edit_win);

void EditWin_redraw_area(EditWin *edit_win, MapArea const *redraw_area,
  bool immediate);

void EditWin_set_zoom(EditWin *edit_win, int zoom_factor);
int EditWin_get_zoom(EditWin const *edit_win);

PaletteEntry EditWin_get_bg_colour(EditWin const *edit_win);
void EditWin_set_bg_colour(EditWin *edit_win, PaletteEntry colour);

PaletteEntry EditWin_get_sel_colour(EditWin const *edit_win);
PaletteEntry const (*EditWin_get_sel_palette(EditWin const *edit_win))[NumColours];

unsigned char const (*EditWin_get_sel_colours(EditWin const *edit_win))[NumColours];
void EditWin_set_sel_colour(EditWin *edit_win, PaletteEntry colour);
bool EditWin_get_sel_tex_is_bright(EditWin const *const edit_win, MapRef const tile_num);

PaletteEntry EditWin_get_ghost_colour(EditWin const *edit_win);
void EditWin_set_ghost_colour(EditWin *edit_win, PaletteEntry colour);

PaletteEntry EditWin_get_grid_colour(EditWin const *edit_win);
void EditWin_set_grid_colour(EditWin *edit_win, PaletteEntry colour);

ViewDisplayFlags EditWin_get_display_flags(EditWin const *edit_win);
void EditWin_set_display_flags(EditWin *edit_win, ViewDisplayFlags flags);

MapAngle EditWin_get_angle(EditWin const *edit_win);
void EditWin_set_angle(EditWin *edit_win, MapAngle angle);

bool EditWin_get_status_shown(EditWin const *edit_win);

bool EditWin_start_drag_select(EditWin *edit_win, int drag_type,
  MapArea const *initial_box, bool local);
void EditWin_stop_drag_select(EditWin *edit_win);

bool EditWin_start_drag_obj(EditWin *edit_win, MapArea const *sent_bbox,
                         MapArea const *shown_bbox);
void EditWin_stop_drag_obj(EditWin *edit_win);

void EditWin_show_dbox(EditWin const *edit_win, unsigned int flags, ObjectId dbox_id);
void EditWin_show_dbox_at_ptr(EditWin const *edit_win, ObjectId dbox_id);

void EditWin_show_window_aligned_right(EditWin const *edit_win,
  ObjectId win_id, int width);

void EditWin_show_toolbar(EditWin const *edit_win, ObjectId tools_id);

void EditWin_set_help_and_ptr(EditWin *edit_win, char *help, PointerType ptr);

struct Editor *EditWin_get_editor(EditWin const *edit_win);

void EditWin_update_can_paste(EditWin *edit_win);

int EditWin_get_wimp_handle(EditWin const *edit_win);

struct HillsData const *EditWin_get_hills(EditWin const *edit_win);

void EditWin_redraw_map(EditWin *edit_win, MapArea const *area);

void EditWin_redraw_object(EditWin *edit_win, MapPoint pos, ObjRef base_ref, ObjRef old_ref, ObjRef new_ref, bool has_triggers);

void EditWin_redraw_info(EditWin *edit_win, MapPoint pos);

void EditWin_occluded_obj_changed(EditWin *edit_win, MapPoint pos, ObjRef obj_ref);
void EditWin_occluded_info_changed(EditWin *edit_win, MapPoint pos);

void EditWin_trig_changed(EditWin *edit_win, MapPoint pos, ObjRef obj_ref, TriggerFullParam fparam);

void EditWin_redraw_ghost(EditWin *edit_win);
void EditWin_clear_ghost_bbox(EditWin *edit_win);
void EditWin_set_ghost_map_bbox(EditWin *edit_win, MapArea const *area);
void EditWin_add_ghost_obj(EditWin *edit_win, MapPoint pos, ObjRef obj_ref);
void EditWin_add_ghost_unknown_obj(EditWin *edit_win, MapArea const *bbox);
void EditWin_add_ghost_unknown_info(EditWin *edit_win, MapArea const *bbox);
MapArea EditWin_get_ghost_obj_bbox(EditWin *edit_win, MapPoint const pos, ObjRef obj_ref);
MapArea EditWin_get_ghost_info_bbox(EditWin *edit_win, MapPoint pos);
void EditWin_add_ghost_info(EditWin *edit_win, MapPoint pos);

void EditWin_redraw_pending(EditWin *edit_win, bool immediate);

struct ObjEditContext;
struct ObjEditContext const *EditWin_get_read_obj_ctx(EditWin const *edit_win);

struct MapEditContext;
struct MapEditContext const *EditWin_get_read_map_ctx(EditWin const *edit_win);

struct InfoEditContext;
struct InfoEditContext const *EditWin_get_read_info_ctx(EditWin const *edit_win);

struct ObjGfxMeshesView;
struct ObjGfxMeshesView const *EditWin_get_obj_plot_ctx(EditWin const *edit_win);

struct View;
struct View const *EditWin_get_view(EditWin const *edit_win);

void EditWin_resource_change(EditWin *edit_win, EditorChange event,
                             EditorChangeParams const *params);

void EditWin_set_scroll_pos(EditWin const *edit_win, MapPoint pos);
MapPoint EditWin_get_scroll_pos(EditWin const *edit_win);

#endif
