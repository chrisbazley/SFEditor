/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Creation and maintenance of editing sessions
 *  Copyright (C) 2001 Christopher Bazley
 */

/* "Its a livin' thing! It's a terrible thing to lose..." */
#ifndef Session_h
#define Session_h

#include <stdbool.h>

#include "MapCoord.h"
#include "DataType.h"
#include "Ships.h"
#include "Editor.h"
#include "Triggers.h"

struct CloudColData;
struct ObjEditContext;
struct MapEditContext;
struct FilenamesData;
struct MissionData;
struct MapTex;
struct ObjGfx;
struct EditWin;
struct HillColData;
struct InfoEditContext;

/* EditWin types: (defines how edit_win is treated) */
typedef enum
{
  UI_TYPE_NONE,
  UI_TYPE_MAP,
/* main_filename is SprScape/FxdObj/EditorPref leafname ("Academy1"),
   map_tiles_set, polygonal_objects_set and hill_colours are used
   to hold filenames of graphics to use for display
   - this data is saved in an EditorPrefs file.
   Either of base grid/map may be generated as required. */
  UI_TYPE_MISSION
/* main_filename is partial path to find mission datafiles ("E.E_01"),
   filenames of associated files are in mission data,
   grid/map overlay data may be generated as required */
//#define SESSION_ISODDBALLMAP 3
/* main_filename is FULL PATH of map/grid file,
   map_tiles_set, polygonal_objects_set and hill_colours are used
   to temporarily hold filenames of graphics to use for display.
   Only specified map or grid (overlay) may be displayed/edited/saved.
   Restricted to either tiles or objects editing mode.
   NO EXTRA FILES MAY BE CREATED/SAVED! */
//#define SESSION_ISODDBALLMISSION 4
/* main_filename is FULL PATH of mission file,
   Restricted to ships editing mode: Triggers may not be edited.
   NO EXTRA FILES MAY BE CREATED/SAVED! */
}
InterfaceType;

typedef struct EditSession EditSession;

void Session_splat_anims (EditSession *session);

struct MissionData *Session_get_mission(const EditSession *session);

bool Session_new_edit_win(EditSession *session, struct EditWin const *edit_win_to_copy);

int Session_try_delete_edit_win(EditSession *session,
  struct EditWin *edit_win_to_delete, bool open_parent);

struct EditWin *Session_edit_win_from_wimp_handle(int window);

void Session_destroy(EditSession *session);

void Session_new_map(void);
void Session_new_mission(void);

/* Main map/mission loaders (invoked from NewMission/NewMap/ibar menus) */
void Session_open_map(const char *filename);
void Session_open_mission(const char *filename);
bool Session_open_single_file(char *filename, DataType data_type);
bool Session_load_single(const char *filename, DataType data_type, struct Reader *reader);

void Session_redraw(EditSession *session, MapArea const *const redraw_area, bool immediate);

bool Session_savemission(EditSession *session, const char *filename, bool force);
bool Session_savemap(EditSession *session, const char *filename, bool force);
void Session_openparentdir(EditSession *session);
bool Session_menu_is_open(EditSession *session);

void Session_toggle_animate_state(EditSession *session);

void Session_reload(EditSession *session, DataType data_type);
void Session_revert_to_original(EditSession *session, DataType data_type);
bool Session_switch_file(EditSession *session, DataType data_type, char const *leaf_name);

void Session_notify_saved(EditSession *session, DataType data_type, const char *file_name);
void Session_notify_changed(EditSession *session, DataType data_type);
bool Session_file_modified(EditSession const *session, DataType data_type);
bool Session_can_revert_to_original(EditSession *session, DataType data_type);
int Session_count_modified(EditSession const *session);

int Session_all_count_modified(void);
void Session_all_delete(void);
void Session_all_graphics_changed(struct ObjGfx *graphics, EditorChange event,
                                  EditorChangeParams const *params);
void Session_all_textures_changed(struct MapTex *textures, EditorChange event,
                                  EditorChangeParams const *params);
void Session_map_premove(EditSession *session, MapPoint old_pos, MapPoint new_pos);
void Session_object_premove(EditSession *session, MapPoint old_pos, MapPoint new_pos);

bool Session_drag_obj_link(EditSession *session, int window, int icon,
  Editor *const origin);

void Session_resource_change(EditSession *session,
  EditorChange event, EditorChangeParams const *params);

void Session_redraw_map(EditSession *session, MapArea const *area);

void Session_redraw_object(EditSession *session, MapPoint pos, ObjRef base_ref, ObjRef old_ref, ObjRef new_ref, bool has_triggers);
void Session_redraw_info(EditSession *session, MapPoint pos);

void Session_occluded_obj_changed(EditSession *session, MapPoint pos, ObjRef obj_ref);
void Session_occluded_info_changed(EditSession *session, MapPoint pos);

void Session_trig_changed(EditSession *session, MapPoint pos, ObjRef obj_ref, TriggerFullParam fparam);

void Session_redraw_pending(EditSession *session, bool immediate);

char *Session_get_file_name_for_save(EditSession *session, DataType data_type);

bool Session_save_file(EditSession *session, DataType data_type, char *filename);

bool Session_has_data(const EditSession *session, DataType data_type);
int Session_get_file_size(EditSession const *session, DataType data_type);
char *Session_get_file_name(EditSession *session, DataType data_type);
int const *Session_get_file_date(EditSession *session, DataType data_type);

InterfaceType Session_get_ui_type(const EditSession *session);
struct MapTex *Session_get_textures(const EditSession *session);
struct ObjGfx *Session_get_graphics(const EditSession *session);
struct PolyColData const *Session_get_poly_colours(const EditSession *session);
char *Session_get_filename(EditSession *session);
char *Session_get_save_filename(EditSession *session);
bool Session_can_save_all(const EditSession *session);
bool Session_can_quick_save(const EditSession *session);
bool Session_quick_save(EditSession *session);
struct InfoEditContext const *Session_get_infos(EditSession *session);
struct FilenamesData *Session_get_filenames(EditSession *session);
struct CloudColData *Session_get_cloud_colours(EditSession *session);
struct HillColData const *Session_get_hill_colours(EditSession const *session);

struct ObjEditContext *Session_get_objects(EditSession *session);
struct MapEditContext const *Session_get_map(EditSession const *session);

void Session_show_briefing(EditSession *session);
void Session_show_performance(EditSession *session, ShipType ship_type);
void Session_show_special(EditSession *session);

void Session_save_gfx_config(EditSession *session);

bool Session_get_anims_shown(const EditSession *session);
void Session_set_anims_shown(EditSession *session, bool shown);

#if !PER_VIEW_SELECT
Editor *Session_get_editor(EditSession *session);

void Session_set_help_and_ptr(EditSession *session,
  char *help, PointerType ptr);

void Session_display_mode(EditSession *session);

void Session_redraw_ghost(EditSession *session);
void Session_clear_ghost_bbox(EditSession *session);
void Session_set_ghost_map_bbox(EditSession *session, MapArea const *area);
void Session_add_ghost_obj(EditSession *session, MapPoint pos, ObjRef obj_ref);
void Session_add_ghost_info(EditSession *session, MapPoint pos);
void Session_add_ghost_unknown_obj(EditSession *session, MapArea const *area);
void Session_add_ghost_unknown_info(EditSession *session, MapArea const *area);

#else
struct EditWin *Session_editor_to_win(Editor *editor);
#endif

void Session_display_msg(EditSession *session, const char *hint, bool temp);

void Session_init(void);

#endif
