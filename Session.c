/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Creation and maintenance of editing sessions
 *  Copyright (C) 2001 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ctype.h>
#include <stdbool.h>
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <time.h>

#include "flex.h"

#include "DateStamp.h"
#include "OSReadTime.h"
#include "Platform.h"
#include "err.h"
#include "Macros.h"
#include "msgtrans.h"
#include "nobudge.h"
#include "strextra.h"
#include "FileUtils.h"
#include "pathtail.h"
#include "hourglass.h"
#include "scheduler.h"
#include "linkedlist.h"
#include "StringBuff.h"
#include "Reader.h"
#include "ReaderRaw.h"
#include "ReaderGKey.h"
#include "WriterGKC.h"
#include "WriterGKey.h"

#include "DCS_dialogue.h"
#include "filepaths.h"
#include "GraphicsFiles.h"
#include "MapMode.h"
#include "ObjectsMode.h"
#include "ShipsMode.h"
#include "StatusBar.h"
#include "utils.h"
#include "Session.h"
#include "missfiles.h"
#include "mapfiles.h"
#include "debug.h"
#include "SFInit.h"
#include "EditWin.h"
#include "BriefDbox.h"
#include "Palette.h"
#include "Config.h"
#include "MainMenu.h"
#include "Filescan.h"
#include "MapAnims.h"
#include "MapTex.h"
#include "ObjGfxMesh.h"
#include "SpecialShip.h"
#include "MapEdit.h"
#include "ObjectsEdit.h"
#include "InfoEdit.h"
#include "MapCoord.h"
#include "SessionData.h"
#include "MapTexData.h"
#include "ObjGfxData.h"
#include "GfxConfig.h"
#include "MapEditChg.h"
#include "Desktop.h"
#include "MapTexBitm.h"
#include "MissionData.h"
#include "Map.h"
#include "Obj.h"
#include "DataType.h"
#include "FopenCount.h"
#include "PathUtils.h"
#include "DFileUtils.h"
#include "StrDict.h"
#include "MapAreaCol.h"
#include "MTransfers.h"
#include "Smooth.h"
#include "MSnakes.h"
#include "OSnakes.h"
#include "IntDict.h"

enum {
  MAX_FILE_PERIOD = CLOCKS_PER_SEC / 2,
  PRIORITY = SchedulerPriority_Min,
  AnimPeriodInCs = 4, /* as 'medium' game speed */
  AnimMaxIntervalCs = 100,
  HistoryLog2 = 9,
};

static LinkedList all_list;
static StrDict single_dict, map_dict, mission_dict;

/* ---------------- Private functions ---------------- */

static void set_edit_win_titles(EditSession *const session)
{
  assert(session != NULL);

  do {
    /* Construct new title string */
    DEBUG("Updating window titles for session %p", (void *)session);
    assert(Session_get_filename(session) != NULL);

    stringbuffer_truncate(&session->edit_win_titles, 0);

    char *title_start = "";
    if (session->oddball_file)
    {
      title_start = Session_get_filename(session);
    }
    else if (Session_get_ui_type(session) == UI_TYPE_MAP)
    {
      title_start = msgs_lookup_subn("MapTitle", 1,
                    Session_get_filename(session));
    }
    else if (Session_get_ui_type(session) == UI_TYPE_MISSION)
    {
      char const *misstitle = "";
      if (Session_has_data(session, DataType_Mission))
      {
        misstitle = briefing_get_title(mission_get_briefing(Session_get_mission(session)));
      }
      title_start = msgs_lookup_subn("MissTitle", 2, Session_get_filename(session), misstitle);
    }

    if (!stringbuffer_append_all(&session->edit_win_titles, title_start))
    {
      break;
    }

    if (Session_count_modified(session) > 0)
    {
      if (!stringbuffer_append_all(&session->edit_win_titles, " *"))
        break;
    }

    DEBUG("No. of edit_wins: %d", session->number_of_edit_wins);
    if (session->number_of_edit_wins > 1)
    {
      char nbuf[11];
      sprintf(nbuf, " %d", session->number_of_edit_wins);
      if (!stringbuffer_append_all(&session->edit_win_titles, nbuf))
        break;
    }

    INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
      EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
      EditWin_set_title(&this_edit_win->edit_win,
                        stringbuffer_get_pointer(&session->edit_win_titles));
    }
    return;

  } while(0);
  report_error(SFERROR(NoMem), "", "");
}

static SchedulerTime anim_ticks_to_cs(SchedulerTime anim_ticks)
{
  if (anim_ticks <= (SchedulerTime_Max / AnimPeriodInCs) - 1)
    return (anim_ticks + 1) * AnimPeriodInCs;
  else
    return SchedulerTime_Max;
}

static SchedulerTime update_animations(void *const handle,
  SchedulerTime const new_time, const volatile bool *const time_up)
{
  /* Null event handler for updating map animations */
  NOT_USED(time_up);
  EditSession *const session = handle;
  SchedulerTime earliest_next_frame;

  assert(session != NULL);
  assert(Session_has_data(session, DataType_OverlayMapAnimations));
  assert(session->actual_animate_map);

  DEBUG("Time is now %"PRId32" (difference %"PRId32")", new_time, new_time -
        session->last_update_time);

  /* Calculate number of frames elapsed since last update */
  SchedulerTime const elapsed = LOWEST(new_time - session->last_update_time,
                                       AnimMaxIntervalCs);
  int const steps_to_advance = elapsed / AnimPeriodInCs;
  session->last_update_time += steps_to_advance * AnimPeriodInCs;

  DEBUG("Updating to time %"PRId32" (advance %d steps)",
        session->last_update_time, steps_to_advance);


  /* Update the animations and ground map state */
  MapAreaColData redraw_map;
  MapAreaCol_init(&redraw_map, 0);

  earliest_next_frame = MapEdit_update_anims(Session_get_map(session),
                        steps_to_advance, &redraw_map);

  MapAreaColIter iter;
  for (MapArea const *anim_bbox = MapAreaColIter_get_first(&iter, &redraw_map);
       anim_bbox != NULL;
       anim_bbox = MapAreaColIter_get_next(&iter)) {
    Session_redraw_map(session, anim_bbox);
  }

  Session_redraw_pending(session, true);
  return new_time + anim_ticks_to_cs(earliest_next_frame);
}

static void delete_edit_win(EditWinList *const edit_win_record)
{
  assert(edit_win_record != NULL);

#if PER_VIEW_SELECT
  Editor_destroy(&edit_win_record->editor);
#endif

  EditWin_destroy(&edit_win_record->edit_win);
  free(edit_win_record);
}

static void edit_win_destructor(long int const key, void *const value, void *const arg)
{
  NOT_USED(key);
  NOT_USED(arg);
  delete_edit_win(value);
}

static void remove_delete_edit_win(EditSession *const session, EditWinList *const edit_win_record)
{
  assert(session != NULL);
  assert(edit_win_record != NULL);
  assert(session->number_of_edit_wins > 0);
  EditWinList *const removed = intdict_remove_value(&session->edit_wins_array,
                                  EditWin_get_wimp_handle(&edit_win_record->edit_win), NULL);
  assert(removed == edit_win_record);
  NOT_USED(removed);
  session->number_of_edit_wins--;
  delete_edit_win(edit_win_record);
}

static bool start_anims(EditSession *const session)
{
  if (session->actual_animate_map || !session->desired_animate_map ||
      !Session_has_data(session, DataType_OverlayMapAnimations)) {
    return true;
  }

  SchedulerTime next_update_due;

  ON_ERR_RPT_RTN_V(os_read_monotonic_time(&session->last_update_time), false);

  assert(Session_has_data(session, DataType_OverlayMapAnimations));

  MapEditContext const *map = Session_get_map(session);
  next_update_due = session->last_update_time +
    anim_ticks_to_cs(MapEdit_update_anims(map, 0, NULL));

  ON_ERR_RPT_RTN_V(scheduler_register(update_animations, session,
                   next_update_due, SchedulerPriority_Min), false);

  session->actual_animate_map = true;
  return true; /* success */
}

static void redraw_all(EditSession *const session)
{
  MapArea const redraw_area = MapArea_make_max();
  Session_redraw(session, &redraw_area, false);
}

static void stop_anims(EditSession *const session)
{
  if (!session->actual_animate_map) {
    return;
  }

  session->actual_animate_map = false;
  scheduler_deregister(update_animations, session);
  MapEdit_reset_anims(Session_get_map(session));
  MapEdit_anims_to_map(Session_get_map(session), NULL);
}

static void restart_anims(EditSession *const session)
{
  stop_anims(session);
  start_anims(session);
}

static StrDict *dict_for_session(EditSession const *const session)
{
  assert(session);

  if (session->oddball_file)
  {
    return &single_dict;
  }

  if (session->ui_type == UI_TYPE_MAP)
  {
    return &map_dict;
  }

  assert(session->ui_type == UI_TYPE_MISSION);
  return &mission_dict;
}

static bool set_main_filename(EditSession *const session, char const *filename)
{
  DEBUG("Changing main file name to '%s' (currently '%s')", STRING_OR_NULL(filename),
        Session_get_filename(session));

  if (filename != NULL)
  {
    session->untitled = false;
  }
  else
  {
    filename = msgs_lookup("Untitled");
    session->untitled = true;
  }

  if (strcmp(Session_get_filename(session), filename) != 0)
  {
    EditSession *const removed = strdict_remove_value(dict_for_session(session),
                                     Session_get_filename(session), NULL);
    assert(removed == NULL || removed == session);
    NOT_USED(removed);

    stringbuffer_truncate(&session->filename, 0);
    if (!stringbuffer_append_all(&session->filename, filename) ||
        !strdict_insert(dict_for_session(session), Session_get_filename(session), session, NULL))
    {
      report_error(SFERROR(NoMem), "", "");
      return false;
    }

    for (size_t i = 0; i < ARRAY_SIZE(session->has_fperf); ++i)
    {
      if (session->has_fperf[i])
      {
        FPerfDbox_update_title(&session->fperf[i]);
      }
    }

    for (size_t i = 0; i < ARRAY_SIZE(session->has_bperf); ++i)
    {
      if (session->has_bperf[i])
      {
        BPerfDbox_update_title(&session->bperf[i]);
      }
    }

    if (session->has_briefing)
    {
      BriefDbox_update_title(&session->briefing_data);
    }

#if !PER_VIEW_SELECT
    if (session->has_editor) {
      Editor_update_title(&session->editor);
    }
#endif

    if (session->has_special_ship)
    {
      SpecialShip_update_title(&session->special_ship_data);
    }
  }

  set_edit_win_titles(session);

  return true; /* success */
}

static void show_all_edit_wins(EditSession *const session)
{
  assert(session != NULL);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_show(&this_edit_win->edit_win);
  }
}

static void objects_prechange(MapArea const *bbox, EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_OBJ_PRECHANGE,
    &(EditorChangeParams){.obj_prechange.bbox = *bbox});
}

static void info_added(TargetInfo const *const info, size_t const index, EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_INFO_ADDED,
    &(EditorChangeParams){.info_added = {.index = index, .info = info}});

  Session_redraw_info(session, target_info_get_pos(info));
}

static void info_predelete(TargetInfo const *const info, size_t const index, EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_INFO_PREDELETE,
    &(EditorChangeParams){.info_predelete = {.index = index, .info = info}});

  Session_redraw_info(session, target_info_get_pos(info));
}

static void info_moved(TargetInfo const *const info, MapPoint const old_pos,
  size_t const old_index, size_t const new_index, EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_INFO_MOVED,
    &(EditorChangeParams){.info_moved = {.old_index = old_index, .new_index = new_index, .old_pos = old_pos, .info = info}});

  Session_redraw_info(session, old_pos);
  Session_redraw_info(session, target_info_get_pos(info));
}

static void redraw_obj(MapPoint const pos, ObjRef const base_ref, ObjRef const old_ref,
  ObjRef const new_ref, bool const has_triggers, EditSession *const session)
{
  Session_redraw_object(session, pos, base_ref, old_ref, new_ref, has_triggers);
}

static void redraw_trig(MapPoint const pos, ObjRef const obj_ref,
                        TriggerFullParam const fparam, EditSession *const session)
{
  Session_trig_changed(session, pos, obj_ref, fparam);
}

static void redraw_map(MapArea const *const area, EditSession *const session)
{
  Session_redraw_map(session, area);
}

static void session_cleanup(void)
{
  strdict_destroy(&single_dict, NULL, NULL);
  strdict_destroy(&map_dict, NULL, NULL);
  strdict_destroy(&mission_dict, NULL, NULL);
}

/* ---------------- Public functions ---------------- */

void Session_init(void)
{
  strdict_init(&single_dict);
  strdict_init(&map_dict);
  strdict_init(&mission_dict);
  atexit(session_cleanup);
}

void Session_redraw_map(EditSession *const session, MapArea const *const area)
{
  assert(session != NULL);
  assert(MapArea_is_valid(area));
  DEBUGF("Redraw map at {%" PRIMapCoord ", %" PRIMapCoord " ,%" PRIMapCoord ", %" PRIMapCoord "}\n",
          area->min.x, area->min.y, area->max.x, area->max.y);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_redraw_map(&this_edit_win->edit_win, area);
  }
}

void Session_redraw_object(EditSession *const session, MapPoint const pos, ObjRef const base_ref, ObjRef old_ref, ObjRef const new_ref, bool const has_triggers)
{
  assert(session != NULL);
  DEBUGF("Redraw object %zu to %zu (base %zu) at %" PRIMapCoord ", %" PRIMapCoord "\n",
          objects_ref_to_num(old_ref), objects_ref_to_num(new_ref), objects_ref_to_num(base_ref),
          pos.x, pos.y);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_redraw_object(&this_edit_win->edit_win, pos, base_ref, old_ref, new_ref, has_triggers);
  }
}

void Session_redraw_info(EditSession *const session, MapPoint const pos)
{
  assert(session != NULL);
  DEBUGF("Redraw info at %" PRIMapCoord ", %" PRIMapCoord "\n", pos.x, pos.y);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_redraw_info(&this_edit_win->edit_win, pos);
  }
}

void Session_occluded_obj_changed(EditSession *const session, MapPoint const pos, ObjRef const obj_ref)
{
  assert(session != NULL);
  DEBUGF("Occluded object %zu changed at %" PRIMapCoord ", %" PRIMapCoord "\n",
          objects_ref_to_num(obj_ref), pos.x, pos.y);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_occluded_obj_changed(&this_edit_win->edit_win, pos, obj_ref);
  }
}

void Session_occluded_info_changed(EditSession *const session, MapPoint const pos)
{
  assert(session != NULL);
  DEBUGF("Occluded info changed at %" PRIMapCoord ", %" PRIMapCoord "\n", pos.x, pos.y);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_occluded_info_changed(&this_edit_win->edit_win, pos);
  }
}

void Session_trig_changed(EditSession *const session, MapPoint const pos, ObjRef const obj_ref, TriggerFullParam const fparam)
{
  assert(session != NULL);
  DEBUGF("Redraw trigger for object %zu at %" PRIMapCoord ", %" PRIMapCoord "\n",
          objects_ref_to_num(obj_ref), pos.x, pos.y);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_trig_changed(&this_edit_win->edit_win, pos, obj_ref, fparam);
  }
}

#if !PER_VIEW_SELECT
void Session_redraw_ghost(EditSession *const session)
{
  assert(session != NULL);
  DEBUGF("Wipe ghost\n");

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_redraw_ghost(&this_edit_win->edit_win);
  }
}

void Session_clear_ghost_bbox(EditSession *const session)
{
  assert(session != NULL);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_clear_ghost_bbox(&this_edit_win->edit_win);
  }
}

void Session_set_ghost_map_bbox(EditSession *const session, MapArea const *const area)
{
  assert(session != NULL);
  assert(MapArea_is_valid(area));

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_set_ghost_map_bbox(&this_edit_win->edit_win, area);
  }
}

void Session_add_ghost_obj(EditSession *const session, MapPoint const pos, ObjRef const obj_ref)
{
  assert(session != NULL);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_add_ghost_obj(&this_edit_win->edit_win, pos, obj_ref);
  }
}

void Session_add_ghost_info(EditSession *const session, MapPoint const pos)
{
  assert(session != NULL);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_add_ghost_info(&this_edit_win->edit_win, pos);
  }
}

void Session_add_ghost_unknown_obj(EditSession *const session, MapArea const *const bbox)
{
  assert(session != NULL);
  assert(MapArea_is_valid(bbox));

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_add_ghost_unknown_obj(&this_edit_win->edit_win, bbox);
  }
}

void Session_add_ghost_unknown_info(EditSession *const session, MapArea const *const bbox)
{
  assert(session != NULL);
  assert(MapArea_is_valid(bbox));

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_add_ghost_unknown_info(&this_edit_win->edit_win, bbox);
  }
}

#endif

void Session_redraw_pending(EditSession *const session, bool const immediate)
{
  assert(session != NULL);

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_redraw_pending(&this_edit_win->edit_win, immediate);
  }
}

static void check_tile_range(EditSession *const session)
{
  /* Warn if the ground map(s) for this session require tile nos. not present
     in the loaded tiles set */
  if (Session_has_data(session, DataType_MapTextures))
  {
    const MapTex *const textures = Session_get_textures(session);

    if (!MapEdit_check_tile_range(Session_get_map(session),
          MapTexBitmaps_get_count(&textures->tiles))) {
      WARN_RTN("TileSet");
    }
  }
}

static void check_ref_range(EditSession *const session)
{
  /* Warn if the ground map(s) for this session require tile nos. not present
     in the loaded tiles set */
  if (Session_has_data(session, DataType_PolygonMeshes))
  {
    const ObjGfx *const graphics = Session_get_graphics(session);

    if (!ObjectsEdit_check_ref_range(Session_get_objects(session),
        ObjGfxMeshes_get_ground_count(&graphics->meshes))) {
      WARN_RTN("ObjSet");
    }
  }
}

void Session_object_premove(EditSession *const session, MapPoint const old_pos, MapPoint const new_pos)
{
  Session_resource_change(session, EDITOR_CHANGE_OBJ_PREMOVE,
    &(EditorChangeParams){.obj_premove = {.new_pos = new_pos, .old_pos = old_pos}});
}

static void map_prechange(MapArea const *const bbox, EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_MAP_PRECHANGE,
    &(EditorChangeParams){.map_prechange.bbox = *bbox});
}

static void map_replaced(EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_MAP_ALL_REPLACED, NULL);
  Session_splat_anims(session);
  check_tile_range(session);
}

static void objects_replaced(EditSession *const session)
{
  Session_resource_change(session, EDITOR_CHANGE_OBJ_ALL_REPLACED, NULL);
  check_ref_range(session);
}

void Session_map_premove(EditSession *const session, MapPoint const old_pos, MapPoint const new_pos)
{
  Session_resource_change(session, EDITOR_CHANGE_MAP_PREMOVE,
    &(EditorChangeParams){.map_premove = {.new_pos = new_pos, .old_pos = old_pos}});
}

static EditSession *create_session(InterfaceType const ui_type, bool const oddball_file, char const *filename)
{
  DEBUGF("Creating new editing session (UI type %d%s)\n", ui_type, oddball_file ? ", odd" : "");
  EditSession *const session = malloc(sizeof(*session));
  if (session == NULL)
  {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }

  *session = (EditSession){
    .untitled = true,
    .ui_type = ui_type,
    .oddball_file = oddball_file,
    .objects = {.prechange_cb = objects_prechange, .redraw_obj_cb = redraw_obj,
                .redraw_trig_cb = redraw_trig, .session = session},
    .map = {.prechange_cb = map_prechange, .redraw_cb = redraw_map,
            .session = session},
    .infos = {.added_cb = info_added, .predelete_cb = info_predelete,
              .moved_cb = info_moved,
              .session = session},
  };

  stringbuffer_init(&session->filename);
  stringbuffer_init(&session->edit_win_titles);
  intdict_init(&session->edit_wins_array);

  if (set_main_filename(session, filename))
  {
    linkedlist_insert(&all_list, NULL, &session->all_link);
    return session;
  }

  stringbuffer_destroy(&session->filename);
  stringbuffer_destroy(&session->edit_win_titles);
  free(session);

  return NULL;
}

static bool init_edit_win(EditSession *const session,
  EditWinList *const new_record, EditWin const *const edit_win_to_copy)
{
  assert(new_record);
  assert(session);

#if PER_VIEW_SELECT
  new_record->edit_win_is_valid = false;
  Editor *const editor = &new_record->editor;
  if (Editor_init(editor, session, edit_win_to_copy ? EditWin_get_editor(edit_win_to_copy) : NULL))
  {
    if (EditWin_init(&new_record->edit_win, editor, edit_win_to_copy))
    {
      Editor_set_tools_shown(editor, Editor_get_tools_shown(editor), &new_record->edit_win);
      Editor_set_pal_shown(editor, Editor_get_pal_shown(editor), &new_record->edit_win);
      new_record->edit_win_is_valid = true;
      return true;
    }
    Editor_destroy(editor);
  }
#else
  if (!session->has_editor) {
    session->has_editor = Editor_init(&session->editor, session, NULL);
    if (session->has_editor) {
      if (EditWin_init(&new_record->edit_win, &session->editor, edit_win_to_copy)) {
        return true;
      }
      Editor_destroy(&session->editor);
      session->has_editor = false;
    }
  }
  return EditWin_init(&new_record->edit_win, &session->editor, edit_win_to_copy);
#endif
  return false;
}

bool Session_new_edit_win(EditSession *const session, EditWin const *const edit_win_to_copy)
{
  assert(Session_get_ui_type(session) != UI_TYPE_NONE); /*should have decided by now*/
  if (session->number_of_edit_wins == UCHAR_MAX)
  {
    WARN("NumEditWins");
    return false;
  }

  /* Create new record for linking to list of edit_wins */
  EditWinList *const new_record = malloc(sizeof(*new_record));
  if (new_record == NULL)
  {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  /* Create new edit_win for this session */
  if (!init_edit_win(session, new_record, edit_win_to_copy))
  {
    free(new_record);
    return false;
  }

  if (!intdict_insert(&session->edit_wins_array,
                      EditWin_get_wimp_handle(&new_record->edit_win),
                      new_record, NULL)) {
    report_error(SFERROR(NoMem), "", "");
    delete_edit_win(new_record);
    return false;
  }

  session->number_of_edit_wins++;
  set_edit_win_titles(session); /* window titles show no. of edit_wins */
  return true;
}

void Session_openparentdir(EditSession *const session)
{
  /* Only makes sense when editing a file in isolation */
  if (!session->oddball_file || session->untitled)
    return;

  /* Open parent directory */
  char *const main_filename = Session_get_filename(session);
  char *const last_dot = strrchr(main_filename, PATH_SEPARATOR);
  if (last_dot == NULL)
    return;

  *last_dot = '\0';
  open_dir(main_filename);
  *last_dot = PATH_SEPARATOR;
}

int Session_try_delete_edit_win(EditSession *const session,
  EditWin *const edit_win_to_delete, bool const open_parent)
{
  int count = 0;

  assert(session != NULL);
  assert(edit_win_to_delete != NULL);

  if (session->number_of_edit_wins <= 1) {
    /* Last edit_win of session is closing - count files with unsaved changes */
    count = Session_count_modified(session);
    if (count == 0) {
      /* No unsaved changes */
      if (open_parent) /* Open parent directory? */
        Session_openparentdir(session);

      Session_destroy(session);
    }
  } else {
    /* Close this edit_win immediately (have others) */
    EditWinList *const edit_win_record = CONTAINER_OF(edit_win_to_delete, EditWinList, edit_win);
    remove_delete_edit_win(session, edit_win_record);
    set_edit_win_titles(session); /* window titles show no. of edit_wins */
  }
  return count;
}

static DFile *Session_get_dfile(EditSession const *const session, DataType const data_type)
{
  assert(session);
  assert(data_type >= 0);
  assert(data_type < ARRAY_SIZE(session->dfiles));
  DFile *const dfile = session->dfiles[data_type];
  DEBUGF("data_type %d dfile %p\n", data_type, (void *)dfile);
  return dfile;
}

void Session_destroy(EditSession *const session)
{
  assert(session != NULL);
  DEBUGF("Destroying editing session %p (UI type %d%s)\n",
        (void *)session, session->ui_type, session->oddball_file ? ", odd" : "");

  EditSession *const removed = strdict_remove_value(dict_for_session(session), Session_get_filename(session), NULL);
  assert(removed == session);
  NOT_USED(removed);

  if (session->actual_animate_map)
  {
    scheduler_deregister(update_animations, session);
  }

#if !PER_VIEW_SELECT
  if (session->has_editor) {
    Editor_destroy(&session->editor);
  }
#endif

  intdict_destroy(&session->edit_wins_array, edit_win_destructor, NULL);

  /* Delete associated dialogue boxes */
  for (size_t i = 0; i < ARRAY_SIZE(session->has_fperf); ++i)
  {
    if (session->has_fperf[i])
    {
      FPerfDbox_destroy(&session->fperf[i]);
    }
  }

  for (size_t i = 0; i < ARRAY_SIZE(session->has_bperf); ++i)
  {
    if (session->has_bperf[i])
    {
      BPerfDbox_destroy(&session->bperf[i]);
    }
  }

  if (session->has_briefing)
  {
    BriefDbox_destroy(&session->briefing_data);
  }

  if (session->has_special_ship)
  {
    SpecialShip_destroy(&session->special_ship_data);
  }

  for (DataType data_type = DataType_First; data_type < DataType_SessionCount; ++data_type)
  {
    DFile *const dfile = Session_get_dfile(session, data_type);
    if (dfile)
    {
      dfile_release(dfile);
    }
  }

  stringbuffer_destroy(&session->filename);
  stringbuffer_destroy(&session->edit_win_titles);
  linkedlist_remove(&all_list, &session->all_link);
  free(session);
}

void Session_redraw(EditSession *const session,
  MapArea const *const redraw_area, bool const immediate)
{
  assert(session != NULL);
  assert(MapArea_is_valid(redraw_area));
  DEBUGF("Redraw %" PRIMapCoord ", %" PRIMapCoord ", %" PRIMapCoord ", %" PRIMapCoord " %s\n",
         redraw_area->min.x, redraw_area->min.y, redraw_area->max.x, redraw_area->max.y,
         immediate ? "immediately" : "later");

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_redraw_area(&this_edit_win->edit_win, redraw_area, immediate);
  }
}

static char *get_data_type_string(DataType const data_type)
{
  static char type_desc[32];
  static char const *const tokens[] =
  {
    [DataType_OverlayMap] = "LEVELMAP",
    [DataType_OverlayObjects] = "LEVELOBJS",
    [DataType_BaseMapAnimations]  = "ANIMS",
    [DataType_Mission] = "MISSION",
    [DataType_BaseMap] = "BASEMAP",
    [DataType_BaseObjects] = "BASEOBJS",
  };
  if (data_type < ARRAY_SIZE(tokens) && tokens[data_type])
  {
    STRCPY_SAFE(type_desc, msgs_lookup(tokens[data_type]));
  }
  return type_desc;
}

static bool read_comp_typed(DFile *const dfile, char const *const fname)
{
  return !report_error(load_compressed(dfile, fname), fname, "") &&
         set_saved_with_stamp(dfile, fname);
}

static bool write_comp_typed(DFile *const dfile, char *const fname, DataType const data_type)
{
  return ensure_path_exists(fname) &&
         !report_error(save_compressed(dfile, fname), fname, "") &&
         set_data_type(fname, data_type);
}

bool Session_savemission(EditSession *const session,
  char const *const path_suffix, bool force)
{
#ifdef DEBUG_OUTPUT
  clock_t const start = clock();
#endif

  /* Filename is in base form: e.g. "E.E_01" */
  char const *const write_dir = Config_get_write_dir();

  assert(Session_get_ui_type(session) == UI_TYPE_MISSION);

  DEBUG("Saving mission files for session %p using path suffix '%s'%s",
        (void *)session, path_suffix, force ? " (force)" : "");

  DEBUG("Will write to directory '%s'", write_dir);

  /* Reset animations prior to save to ensure the timer_counter &
     current_tile fields for each animation (and the ground tiles map) are in
     correct initial state */
  restart_anims(session);

  if (Session_count_modified(session) == 0 && !force) {
    DEBUG("No changes and not forced save");
    return true; /* no files changed */
  }

  static DataType const data_types[] = {
    DataType_Mission /* must be first */,
    DataType_OverlayMap, DataType_OverlayObjects, DataType_OverlayMapAnimations
  };

  /* Construct file save paths */
  char *file_paths[ARRAY_SIZE(data_types)] = {NULL};
  bool any_exists = false;
  bool success = true;
  for (size_t i = 0; success && (i < ARRAY_SIZE(data_types)); ++i)
  {
    file_paths[i] = make_file_path_in_subdir(
                        write_dir, data_type_to_sub_dir(data_types[i]), path_suffix);
    if (file_paths[i] == NULL)
    {
      success = false;
    }
    else if (file_exists(file_paths[i]))
    {
      any_exists = true;
    }
  }

  /* Check for existing files on the target paths */
  if (success && stricmp(Session_get_save_filename(session), path_suffix) != 0)
  {
    if (any_exists)
    {
      success = dialogue_confirm(msgs_lookup_subn("MultOv", 1, path_suffix), "OvBut");
    }
    force = true;
  }

  FilenamesData *const f = Session_get_filenames(session);
  int saved_count = 0;
  bool saved[ARRAY_SIZE(data_types)] = {false};
  for (size_t i = 1; success && (i < ARRAY_SIZE(data_types)); ++i)
  {
    bool const changed = Session_file_modified(session, data_types[i]);
    DEBUG("File %zu has%s been changed", i, changed ? "" : " not");

    if (force || changed)
    {
      /* We don't want to duplicate unmodified 'Blank' data under a new file
         name (even if forced save) */
      if (!changed && stricmp(filenames_get(f, data_types[i]), BLANK_FILE) == 0)
      {
        DEBUG("%s filename is blank - will not save", get_data_type_string(data_types[i]));
      }
      else
      {
        DFile *const dfile = Session_get_dfile(session, data_types[i]);
        if (dfile)
        {
          success = write_comp_typed(dfile, file_paths[i], data_types[i]);
          if (success) {
            ++saved_count;
            saved[i] = true;
          }
        }
      }
    }
  }

  if (success) {
    /* Evaluate whether to save mission last, because even if not edited
       it may need to be saved because paths to ancillary files have changed  */
    bool changed = Session_file_modified(session, DataType_Mission);
    char old_names[ARRAY_SIZE(data_types)][BytesPerFilename] = {""};

    /* Update paths to ancillary files stored in mission data */
    for (size_t i = 1; i < ARRAY_SIZE(data_types); ++i)
    {
      if (!saved[i]) {
        continue;
      }

      char const *const old_name = filenames_get(f, data_types[i]);
      STRCPY_SAFE(old_names[i], old_name);

      if (stricmp(old_name, path_suffix)) {
        filenames_set(f, data_types[i], path_suffix);
        changed = true;
      }
    }

    DEBUG("Mission data has%s been changed", changed ? "" : " not");

    if (force || changed) {
      DFile *const dfile = Session_get_dfile(session, DataType_Mission);
      if (dfile)
      {
        success = write_comp_typed(dfile, file_paths[0], DataType_Mission);
        if (success) {
          saved_count++;
          success = set_saved_with_stamp(dfile, file_paths[0]);
          filescan_directory_updated(filescan_get_emh_type(path_suffix));
        }
      }
    }

    for (size_t i = 1; i < ARRAY_SIZE(data_types); ++i) {
      if (!saved[i]) {
        continue;
      }
      if (success) {
        DFile *const dfile = Session_get_dfile(session, data_types[i]);
        success = set_saved_with_stamp(dfile, file_paths[i]);
      } else {
        /* Restore paths to ancillary files stored in mission data */
        filenames_set(f, data_types[i], old_names[i]);
      }
    }
  }

  if (success)
  {
    char count_str[12];
    sprintf(count_str, "%d", saved_count);
    Session_display_msg(session, msgs_lookup_subn("StatusMSaved", 2,
      count_str, path_suffix), true);

    set_main_filename(session, path_suffix);
  }

  for (size_t i = 0; i < ARRAY_SIZE(data_types); ++i)
  {
    free(file_paths[i]);
  }

#ifdef DEBUG_OUTPUT
  clock_t const period = clock() - start;
  if (period > MAX_FILE_PERIOD) {
    DEBUGF("Mission save period: %g\n", (double)period / CLOCKS_PER_SEC);
  }
#endif
  return true; /* success */
}

bool Session_savemap(EditSession *const session, char const *const leaf_name, bool force)
{
#ifdef DEBUG_OUTPUT
  clock_t const start = clock();
#endif

  char const *const write_dir = Config_get_write_dir();

  assert(Session_get_ui_type(session) == UI_TYPE_MAP);

  /* Filename is in base form: e.g. "Academy1" */
  DEBUG("Saving map files for session %p using leaf name '%s'%s",
        (void *)session, leaf_name, force ? " (force)" : "");

  DEBUG("Will write to directory '%s'", write_dir);

  /* Reset animations prior to save to ensure the timer_counter &
     current_tile fields for each animation (and the ground tiles map) are in
     correct initial state */
  restart_anims(session);

  if (Session_count_modified(session) == 0 && !force)
  {
    DEBUG("No changes and not forced save");
    return true; /* no files changed */
  }

  static DataType const data_types[] = {
    DataType_BaseMap, DataType_BaseObjects, DataType_BaseMapAnimations
  };
  char *file_paths[ARRAY_SIZE(data_types)] = {NULL};

  /* Construct file save paths */
  bool any_exists = false;
  bool success = true;
  for (size_t i = 0; success && (i < ARRAY_SIZE(data_types)); ++i)
  {
    file_paths[i] = make_file_path_in_subdir(
                        write_dir, data_type_to_sub_dir(data_types[i]), leaf_name);
    if (file_paths[i] == NULL)
    {
      success = false;
    }
    else if (file_exists(file_paths[i]))
    {
      any_exists = true;
    }
  }

  /* Check for existing files on the target paths */
  if (success && stricmp(Session_get_save_filename(session), leaf_name) != 0)
  {
    if (any_exists)
    {
      success = dialogue_confirm(msgs_lookup_subn("MultOv", 1, leaf_name), "OvBut");
    }
    force = true;
  }

  int saved_count = 0;
  for (size_t i = 0; success && (i < ARRAY_SIZE(data_types)); ++i)
  {
    bool const changed = Session_file_modified(session, data_types[i]);
    DEBUG("File %zu has%s been changed", i, changed ? "" : " not");

    if (force || changed)
    {
      DFile *const dfile = Session_get_dfile(session, data_types[i]);
      if (dfile)
      {
        success = write_comp_typed(dfile, file_paths[i], data_types[i]);
        if (success) {
          success = set_saved_with_stamp(dfile, file_paths[i]);
          ++saved_count;
        }
      }
    }
  }

  if (success)
  {
    char count_str[12];
    sprintf(count_str, "%d", saved_count);
    Session_display_msg(session, msgs_lookup_subn("StatusBSaved", 2,
                        count_str, leaf_name), true);

    set_main_filename(session, leaf_name);
  }

  for (size_t i = 0; i < ARRAY_SIZE(data_types); ++i)
  {
    free(file_paths[i]);
  }

#ifdef DEBUG_OUTPUT
  clock_t const period = clock() - start;
  if (period > MAX_FILE_PERIOD) {
    DEBUGF("Map save period: %g\n", (double)period / CLOCKS_PER_SEC);
  }
#endif
  return success;
}

static DFile *create_mission(EditSession *const session)
{
  assert(session);
  session->mission = mission_create();
  session->objects.triggers = session->mission ? mission_get_triggers(session->mission) : NULL;
  session->infos.data = session->mission ? mission_get_target_infos(session->mission) : NULL;
  return session->mission ? mission_get_dfile(session->mission) : NULL;
}

static DFile *create_base_map(EditSession *const session)
{
  assert(session);
  session->map.base = map_create_base();
  return session->map.base ? map_get_dfile(session->map.base) : NULL;
}

static DFile *create_overlay_map(EditSession *const session)
{
  assert(session);
  session->map.overlay = map_create_overlay();
  return session->map.overlay ? map_get_dfile(session->map.overlay) : NULL;
}

static DFile *create_base_obj(EditSession *const session)
{
  assert(session);
  session->objects.base = objects_create_base();
  return session->objects.base ? objects_get_dfile(session->objects.base) : NULL;
}

static DFile *create_overlay_obj(EditSession *const session)
{
  assert(session);
  session->objects.overlay = objects_create_overlay();
  return session->objects.overlay ? objects_get_dfile(session->objects.overlay) : NULL;
}

static DFile *create_anims(EditSession *const session)
{
  assert(session);
  session->map.anims = MapAnims_create();
  return session->map.anims ? MapAnims_get_dfile(session->map.anims) : NULL;
}

static DFile *create_dfile(EditSession *const session, DataType const data_type)
{
  DFile *dfile = NULL;
  switch (data_type)
  {
    case DataType_BaseMap:
      dfile = create_base_map(session);
      break;

    case DataType_OverlayMap:
      dfile = create_overlay_map(session);
      break;

    case DataType_BaseObjects:
      dfile = create_base_obj(session);
      break;

    case DataType_OverlayObjects:
      dfile = create_overlay_obj(session);
      break;

    case DataType_Mission:
      dfile = create_mission(session);
      break;

    case DataType_BaseMapAnimations:
    case DataType_OverlayMapAnimations:
      dfile = create_anims(session);
      break;

    default:
      report_error(SFERROR(BadFileType), "", "");
      return NULL;
  }

  if (dfile) {
    if (session->dfiles[data_type]) {
      dfile_release(session->dfiles[data_type]);
    }
    session->dfiles[data_type] = dfile;
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
#ifdef FORTIFY
    Fortify_CheckAllMemory();
#endif
  return dfile;
}

static bool check_file_type(char const *const full_path,
  DataType const data_type)
{
  /* We don't actually store the file type anywhere so it's best to check it
     before misrepresenting it to the user. Could also indicate bad data. */
  int file_type;
  if (E(get_file_type(full_path, &file_type))) {
    return false;
  }

  if (data_type_to_file_type(data_type) != file_type) {
    report_error(SFERROR(WrongFileType), full_path, "");
    return false;
  }
  return true;
}

static bool load_file(EditSession *const session, char const *const full_path,
  DataType const data_type)
{
  if (!check_file_type(full_path, data_type)) {
    return false;
  }

  DFile *const dfile = create_dfile(session, data_type);
  if (dfile) {
    if (read_comp_typed(dfile, full_path)) {
      return true;
    }
  }
  return false;
}

static bool load_leaf(EditSession *const session, DataType const data_type,
  char const *const leaf_name)
{
  char const *const sub_dir = data_type_to_sub_dir(data_type);
  char *const full_path = make_file_path_in_dir_on_path(LEVELS_PATH, sub_dir, leaf_name);
  if (!full_path) {
    return false;
  }
  bool const success = load_file(session, full_path, data_type);
  free(full_path);
  return success;
}

static DFile *get_shared_base_map(EditSession *const session, char const *const filename)
{
  assert(session);

  MapData *map = map_get_shared(filename);
  if (map) {
    session->map.base = map;
    return map_get_dfile(map);
  }

  map = map_create_base();
  if (map) {
    DFile *const dfile = map_get_dfile(map);
    if (read_comp_typed(dfile, filename)) {
      session->map.base = map;
      if (map_share(map)) {
        return dfile;
      }
      report_error(SFERROR(NoMem), "", "");
    }
    dfile_release(dfile);
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
  return NULL;
}

static DFile *get_shared_base_obj(EditSession *const session, char const *const filename)
{
  assert(session);

  ObjectsData *obj = objects_get_shared(filename);
  if (obj) {
    session->objects.base = obj;
    return objects_get_dfile(obj);
  }

  obj = objects_create_base();
  if (obj) {
    DFile *const dfile = objects_get_dfile(obj);
    if (read_comp_typed(dfile, filename)) {
      session->objects.base = obj;
      if (objects_share(obj)) {
        return dfile;
      }
      report_error(SFERROR(NoMem), "", "");
    }
    dfile_release(dfile);
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
  return NULL;
}

static DFile *get_shared_tiles(EditSession *const session, char const *const filename)
{
  assert(session);

  MapTex *textures = MapTex_get_shared(filename);
  if (textures) {
    session->textures = textures;
    return MapTex_get_dfile(textures);
  }

  textures = MapTex_create();
  if (textures) {
    DFile *const dfile = MapTex_get_dfile(textures);
    if (read_comp_typed(dfile, filename)) {
      MapTex_load_metadata(textures);
      session->textures = textures;
      if (MapTex_share(textures)) {
        return dfile;
      }
      report_error(SFERROR(NoMem), "", "");
    }
    dfile_release(dfile);
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
  return NULL;
}

static DFile *get_shared_poly(EditSession *const session, char const *const filename)
{
  ObjGfx *graphics = ObjGfx_get_shared(filename);
  if (graphics) {
    session->graphics = graphics;
    return ObjGfx_get_dfile(graphics);
  }

  graphics = ObjGfx_create();
  if (graphics) {
    DFile *const dfile = ObjGfx_get_dfile(graphics);
    if (read_comp_typed(dfile, filename)) {
      ObjGfx_load_metadata(graphics);
      session->graphics = graphics;
      if (ObjGfx_share(graphics)) {
        return dfile;
      }
      report_error(SFERROR(NoMem), "", "");
    }
    dfile_release(dfile);
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
  return NULL;
}

static DFile *get_shared_polycol(EditSession *const session, char const *const filename)
{
  assert(session);

  PolyColData *poly_colours = polycol_get_shared(filename);
  if (poly_colours) {
    session->poly_colours = poly_colours;
    return polycol_get_dfile(poly_colours);
  }

  poly_colours = polycol_create();
  if (poly_colours) {
    DFile *const dfile = polycol_get_dfile(poly_colours);
    if (read_comp_typed(dfile, filename)) {
      session->poly_colours = poly_colours;
      if (polycol_share(poly_colours)) {
        return dfile;
      }
      report_error(SFERROR(NoMem), "", "");
    }
    dfile_release(dfile);
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
  return NULL;
}

static DFile *get_shared_hillcol(EditSession *const session, char const *const filename)
{
  assert(session);

  HillColData *hill_colours = hillcol_get_shared(filename);
  if (hill_colours) {
    session->hill_colours = hill_colours;
    return hillcol_get_dfile(hill_colours);
  }

  hill_colours = hillcol_create();
  if (hill_colours) {
    DFile *const dfile = hillcol_get_dfile(hill_colours);
    if (read_comp_typed(dfile, filename)) {
      session->hill_colours = hill_colours;
      if (hillcol_share(hill_colours)) {
        return dfile;
      }
      report_error(SFERROR(NoMem), "", "");
    }
    dfile_release(dfile);
  } else {
    report_error(SFERROR(NoMem), "", "");
  }
  return NULL;
}

static bool get_shared_file(EditSession *const session, char const *const full_path,
  DataType const data_type)
{
  DFile *dfile = NULL;
  bool is_none = false;

  if (data_type == DataType_HillColours &&
      !strcmp(NO_FILE, pathtail(full_path, 1))) {
    is_none = true;
  } else if (!check_file_type(full_path, data_type)) {
    return false;
  }

  // Try to load the new data of the specified type
  switch (data_type)
  {
    case DataType_BaseMap:
      dfile = get_shared_base_map(session, full_path);
      break;

    case DataType_BaseObjects:
      dfile = get_shared_base_obj(session, full_path);
      break;

    case DataType_MapTextures:
      dfile = get_shared_tiles(session, full_path);
      break;

    case DataType_PolygonMeshes:
      dfile = get_shared_poly(session, full_path);
      break;

    case DataType_PolygonColours:
      dfile = get_shared_polycol(session, full_path);
      break;

    case DataType_HillColours:
      if (is_none)
      {
        session->hill_colours = NULL;
      }
      else
      {
        dfile = get_shared_hillcol(session, full_path);
      }
      break;

    case DataType_SkyColours:
    case DataType_SkyImages:
      is_none = true;
      break;

    default:
      break;
  }

  /* If successful then release a reference to the current data of the
     specified type and replace it */
  if (dfile || is_none) {
    if (session->dfiles[data_type]) {
      dfile_release(session->dfiles[data_type]);
    }
    session->dfiles[data_type] = dfile;
  }

  return dfile || is_none;
}

static bool get_shared_leaf(EditSession *const session, DataType const data_type,
  char const *const leaf_name)
{
  assert(leaf_name);
  assert(*leaf_name);
  char const *const sub_dir = data_type_to_sub_dir(data_type);
  char *const full_path = make_file_path_in_dir_on_path(LEVELS_PATH, sub_dir, leaf_name);
  if (!full_path)
  {
    return false;
  }

  bool const success = get_shared_file(session, full_path, data_type);
  free(full_path);
  return success;
}

static bool Session_loadreqgfx(EditSession *const session)
{
  /* Load or borrow graphics data (shared) */
  DEBUG("Loading only those graphics required for session %p", (void *)session);

  static const struct {
    DataType resource;
    DataType dependents[5];
  } deps[] = {
    { DataType_MapTextures, /* is required by... */
      {DataType_BaseMap, DataType_OverlayMap,
       DataType_BaseMapAnimations, DataType_OverlayMapAnimations,
       DataType_Count} },

    { DataType_PolygonMeshes, /* is required by... */
      {DataType_BaseObjects, DataType_OverlayObjects,
       DataType_Mission,
       DataType_Count} },

    { DataType_PolygonColours, /* is required by... */
      {DataType_BaseObjects, DataType_OverlayObjects,
       DataType_Mission,
       DataType_Count} },

    { DataType_HillColours, /* is required by... */
      {DataType_BaseObjects, DataType_OverlayObjects,
       DataType_Count} },
  };

  FilenamesData const *const filenames = Session_get_filenames(session);
  for (size_t i = 0; i < ARRAY_SIZE(deps); ++i)
  {
    for (size_t j = 0; deps[i].dependents[j] != DataType_Count; ++j)
    {
      if (!Session_has_data(session, deps[i].dependents[j]))
      {
        continue;
      }

      char const *const leaf_name = filenames_get(filenames, deps[i].resource);
      if (!get_shared_leaf(session, deps[i].resource, leaf_name))
      {
        return false;
      }
    }
  }

  return true; /* success */
}

static bool Session_get_shared_base_map(EditSession *const session)
{
  /* Load or borrow base map files */
  static DataType const data_types[] = {
    DataType_BaseMap, DataType_BaseObjects,
  };

  FilenamesData const *const filenames = Session_get_filenames(session);
  for (size_t i = 0; i < ARRAY_SIZE(data_types); ++i)
  {
    char const *const leaf_name = filenames_get(filenames, data_types[i]);
    if (!get_shared_leaf(session, data_types[i], leaf_name))
    {
      return false;
    }
  }

  return true;
}

static bool Session_load_base_map(EditSession *const session)
{
  /* Load base map files */
  FilenamesData *const f = Session_get_filenames(session);
  static DataType const data_types[] = {
    DataType_BaseMap, DataType_BaseObjects,
  };
  bool success = true;

  for (size_t i = 0; success && i < ARRAY_SIZE(data_types); ++i) {
    char const *sub_dir = data_type_to_sub_dir(data_types[i]);
    char *file_path = make_file_path_in_dir_on_path(LEVELS_PATH, sub_dir,
                                                    filenames_get(f, data_types[i]));

    if (file_path && !file_exists(file_path)) {
      filenames_set(f, data_types[i], BLANK_FILE);

      /* Blank base animations file doesn't exist */
      if (data_types[i] == DataType_BaseMapAnimations) {
        sub_dir = LEVELANIMS_DIR;
      }

      free(file_path);
      file_path = make_file_path_in_dir_on_path(LEVELS_PATH, sub_dir, BLANK_FILE);
    }

    if (!file_path) {
      return false;
    }

    success = load_file(session, file_path, data_types[i]);
    free(file_path);
  }

  return success;
}

static bool Session_load_overlay_map(EditSession *const session)
{
  /* Load level-specific files */
  static DataType const data_types[] = {DataType_OverlayMap, DataType_OverlayObjects, DataType_OverlayMapAnimations};
  FilenamesData const *const filenames = Session_get_filenames(session);

  for (size_t i = 0; i < ARRAY_SIZE(data_types); ++i)
  {
    char const *const leaf_name = filenames_get(filenames, data_types[i]);
    if (!load_leaf(session, data_types[i], leaf_name))
    {
      return false;
    }
  }

  return true;
}

static bool init_anims(EditSession *const session)
{
  /* Initialise map overlay from animations data */
  Session_splat_anims(session);

  /* If map animations enabled then start them up */
  return start_anims(session);
}

static bool load_map_core(EditSession *const session, char const *const leaf_name)
{
  /* Filename is in base form: e.g. "Academy1" */
  DEBUG("Loading base map '%s' for session %p", leaf_name, (void *)session);
#ifdef DEBUG_OUTPUT
  clock_t const start = clock();
#endif

  if (fixed_last_anims_load) /* a bit of a hack */
  {
    Session_notify_changed(session, DataType_BaseMapAnimations);
  }

  if (!GfxConfig_load(&session->gfx_config, leaf_name))
  {
    return false;
  }

  /* Load or borrow basemap files (shared) */
  if (!Session_load_base_map(session))
  {
    return false;
  }

  /* Load or borrow graphics data to display stuff
  (there is no tolerance of bad filenames in MapTex file) */
  if (!Session_loadreqgfx(session))
  {
    return false;
  }

  if (!init_anims(session))
  {
    return false;
  }

#ifdef DEBUG_OUTPUT
    clock_t const period = clock() - start;
    if (period > MAX_FILE_PERIOD) {
      DEBUGF("Map load period: %g\n", (double)period / CLOCKS_PER_SEC);
    }
#endif

  check_tile_range(session);
  check_ref_range(session);

  return true;
}

void Session_new_map(void)
{
  EditSession *const session = create_session(UI_TYPE_MAP, false, NULL);
  if (session)
  {
    if (!load_map_core(session, BLANK_FILE) || !Session_new_edit_win(session, NULL))
    {
      Session_destroy(session);
    }
  }
}

void Session_open_map(char const *const filename)
{
  EditSession *session = strdict_find_value(&map_dict, filename, NULL);
  if (session)
  {
    show_all_edit_wins(session);
  }
  else
  {
    session = create_session(UI_TYPE_MAP, false, filename);
    if (session)
    {
      if (!load_map_core(session, filename) || !Session_new_edit_win(session, NULL))
      {
        Session_destroy(session);
      }
    }
  }
}

void Session_save_gfx_config(EditSession *const session)
{
  /* main_filename is basemap leafname */
  GfxConfig_save(&session->gfx_config, Session_get_filename(session));
}

MissionData *Session_get_mission(const EditSession *const session)
{
  assert(session != NULL);
  return session->mission;
}

char *Session_get_filename(EditSession *const session)
{
  assert(session != NULL);
  return stringbuffer_get_pointer(&session->filename);
}

char *Session_get_save_filename(EditSession *const session)
{
  char *main_filename = "";

  assert(session != NULL);

  if (Session_get_ui_type(session) == UI_TYPE_MISSION) {
    /* We are editing mission file(s) */
    if (!Session_can_quick_save(session))
      main_filename = msgs_lookup("DefMissTit");
    else
      main_filename = Session_get_filename(session);
  } else {
    /* We are editing base map file(s) */
    if (!Session_can_quick_save(session))
      main_filename = msgs_lookup("DefMapTit");
    else
      main_filename = Session_get_filename(session);
  }

  assert(main_filename != NULL);
  return main_filename;
}

void Session_notify_changed(EditSession *const session,
  DataType const data_type)
{
  assert(session != NULL);
  DEBUG("Session %p notified that file of type %d has changed", (void *)session, data_type);

  DFile *const dfile = Session_get_dfile(session, data_type);
  if (dfile && !dfile_get_modified(dfile))
  {
    dfile_set_modified(dfile);
    set_edit_win_titles(session); /* add unsaved indicator to title */
  }

  if (data_type == DataType_OverlayMapAnimations ||
      data_type == DataType_BaseMapAnimations) {
    /* Check whether we need to enable or disable animation updates */
    MapEditContext const *map = Session_get_map(session);
    if (MapEdit_count_anims(map)) {
      start_anims(session);
    } else {
      stop_anims(session);
    }

  }
}

void Session_notify_saved(EditSession *const session, DataType const data_type,
  char const *const file_name)
{
  assert(session != NULL);
  DEBUG("Session %p notified that file of type %d has been "
        "saved as '%s'", (void *)session, data_type, file_name);

  /* Canonicalise path file was saved as (for comparison purposes) */
  char *canon_save_path;
  ON_ERR_RPT_RTN(canonicalise(&canon_save_path, NULL, NULL, file_name));
  DEBUG("Canonicalised save path is '%s'", canon_save_path);

  /* We may have affected any of the paths that we have cached catalogue
     information on. Note that files of incompatible type are not recorded in our
     file lists. */
  char const * const sub_dir = data_type_to_sub_dir(data_type);
  DEBUG("Sub directory for type %d is '%s' (length %zu)", data_type, sub_dir,
        strlen(sub_dir));

  filescan_type dir_up = FS_LAST;
  switch (data_type) {
    case DataType_BaseMapAnimations:
      dir_up = FS_BASE_ANIMS;
      break;

    case DataType_Mission:
      dir_up = filescan_get_emh_type(pathtail(file_name, 2));
      break;

    case DataType_BaseMap:
      dir_up = FS_BASE_SPRSCAPE;
      break;

    case DataType_BaseObjects:
      dir_up = FS_BASE_FXDOBJ;
      break;

    default:
      break;
  }

  /* Was file saved to appropriate subdirectory inside game? */
  if (dir_up != FS_LAST)
  {
    char *const intern_compare_path = make_file_path_in_dir(Config_get_read_dir(), sub_dir);
    if (intern_compare_path == NULL) {
      free(canon_save_path);
      return;
    }

    if (strnicmp(canon_save_path, intern_compare_path, strlen(intern_compare_path)) == 0) {
      DEBUG("Matched save path with int. %s", intern_compare_path);
      free(intern_compare_path);
      filescan_directory_updated(dir_up);

    } else {
      /* Was file saved to appropriate subdirectory inside ext. levels dir? */
      DEBUG("Failed to match save path with int. %s", intern_compare_path);
      free(intern_compare_path);

      if (Config_get_use_extern_levels_dir()) {
        char *const extern_compare_path = make_file_path_in_dir(
                                                   Config_get_extern_levels_dir(), sub_dir);
        if (extern_compare_path == NULL) {
          free(canon_save_path);
          return;
        }

        if (strnicmp(canon_save_path, extern_compare_path,
                     strlen(extern_compare_path)) == 0) {
          DEBUG("Matched save path with ext. %s", extern_compare_path);
          filescan_directory_updated(dir_up);
        } else {
          DEBUG("Failed to match save path with ext. %s", extern_compare_path);
        }
        free(extern_compare_path);
      }
    }
  }

  DFile *const dfile = Session_get_dfile(session, data_type);
  if (dfile)
  {
    if (session->oddball_file)
    {
      /* For oddball files we don't care where they are saved */
      DEBUG("Is oddball file");
      (void)set_saved_with_stamp(dfile, canon_save_path);
      set_main_filename(session, canon_save_path);
    }
    else if (Session_can_quick_save(session))
    {
      char const *const main_filename = Session_get_filename(session);

      /* Construct expected save path for this component */
      char *const expect_path = make_file_path_in_subdir(Config_get_write_dir(), sub_dir, main_filename);
      if (expect_path == NULL)
      {
        free(canon_save_path);
        return;
      }
      DEBUG("Expected save path for component is '%s'", expect_path);

      if (stricmp(canon_save_path, expect_path) == 0)
      {
        /* File was saved to expected path - treat as if saved via
           main save dbox */
        if (Session_get_ui_type(session) == UI_TYPE_MISSION &&
            data_type != DataType_Mission) {
          FilenamesData *const filenames = Session_get_filenames(session);
          char const *const miss_name = filenames_get(filenames, data_type);
          if (stricmp(miss_name, main_filename) != 0)
          {
            /* Update file path stored in mission data */
            DEBUG("Updating leaf path in mission data from '%s' to '%s'",
                  miss_name, main_filename);

            filenames_set(filenames, data_type, main_filename);
            Session_notify_changed(session, DataType_Mission);
          }
        }

        (void)set_saved_with_stamp(dfile, canon_save_path);
        set_edit_win_titles(session); /* remove unsaved indicator */
      }
      free(expect_path);
    }

    Session_display_msg(session, msgs_lookup_subn("Status1Saved", 2,
                        get_data_type_string(data_type), canon_save_path), true);
  }
  free(canon_save_path);
}

static bool load_single_core(EditSession *const session, char const *const filename,
  DataType const data_type, Reader *const reader)
{
  // File may not be stored on a file system
  assert(session);
  bool success = false;
  DFile *const dfile = create_dfile(session, data_type);
  if (!dfile)
  {
    return false;
  }

  SFError err = SFERROR(NoMem);
  Reader gk_reader;
  if (reader_gkey_init_from(&gk_reader, HistoryLog2, reader))
  {
    err = dfile_read(dfile, &gk_reader);
    reader_destroy(&gk_reader);
  }

  success = !report_error(err, filename, "");
  if (!success)
  {
    return false;
  }

  if (data_type != DataType_Mission)
  {
    /* Get default graphics file names */
    if (!GfxConfig_load(&session->gfx_config, UNKNOWN_FILE))
    {
      return false;
    }
  }

  if ((data_type == DataType_BaseMapAnimations) ||
      (data_type == DataType_OverlayMapAnimations))
  {
    /* A global flag is set if the last file processed
       had to be altered - a bit of a hack */
    if (fixed_last_anims_load)
    {
      Session_notify_changed(session, data_type);
    }

    /* Load blank tiles map overlay (required for display of animations) */
    if (!load_leaf(session, DataType_OverlayMap, BLANK_FILE))
    {
      return false;
    }

    if (!init_anims(session))
    {
      return false;
    }
  }

  /* Load only those graphics files that are necessary */
  if (!Session_loadreqgfx(session))
  {
    return false;
  }

  check_tile_range(session);
  check_ref_range(session);
  return true;
}

static InterfaceType data_type_to_ui(DataType const data_type)
{
  InterfaceType const ui_type = (data_type == DataType_BaseMap ||
                                 data_type == DataType_BaseObjects ||
                                 data_type == DataType_BaseMapAnimations) ?
                                   UI_TYPE_MAP : UI_TYPE_MISSION;
  DEBUG("UI type: %s", ui_type == UI_TYPE_MAP ? "Base map" : "Mission");
  return ui_type;
}

bool Session_open_single_file(char *const filename, DataType const data_type)
{
  bool success = true;
  EditSession *session = strdict_find_value(&single_dict, filename, NULL);
  if (session)
  {
    show_all_edit_wins(session);
  }
  else
  {
    success = false;
    FILE *const f = fopen_inc(filename, "rb");
    if (!f)
    {
      report_error(SFERROR(OpenInFail), filename, "");
    }
    else
    {
      Reader reader;
      reader_raw_init(&reader, f);

      session = create_session(data_type_to_ui(data_type), true, filename);
      if (session)
      {
        if (load_single_core(session, filename, data_type, &reader) &&
            Session_new_edit_win(session, NULL))
        {
          success = set_saved_with_stamp(Session_get_dfile(session, data_type),
                                         filename);
        } else {
          Session_destroy(session);
        }
      }
      reader_destroy(&reader);
      fclose_dec(f);
    }
  }
  return success;
}

bool Session_load_single(char const *const filename, DataType const data_type,
  Reader *const reader)
{
  // File may not be stored on a file system
  EditSession *const session = create_session(
                                 data_type_to_ui(data_type), true, filename);
  if (session)
  {
    if (load_single_core(session, filename, data_type, reader) &&
        Session_new_edit_win(session, NULL))
    {
      OSDateAndTime date_stamp;
      if (!E(get_current_time(&date_stamp)) &&
          dfile_set_saved(Session_get_dfile(session, data_type),
                          NULL /* untitled */, (int *)&date_stamp))
      {
        return true;
      }
    }
    Session_destroy(session);
  }
  return false;
}

static bool load_mission_core(EditSession *const session, char const *const filename)
{
#ifdef DEBUG_OUTPUT
  clock_t const start = clock();
#endif
  DEBUG("Loading mission '%s' for session %p", filename, (void *)session);

  /* Load mission data */
  if (!load_file(session, filename, DataType_Mission))
  {
    return false;
  }

  /* Load or borrow basemap files (shared) */
  if (!Session_get_shared_base_map(session) ||
      !Session_load_overlay_map(session))
  {
    return false;
  }

  /* FIXME: I believe this is where any base map animations should be loaded
    and merged with the level animations prior to being discarded */

  if (fixed_last_anims_load) /* a bit of a hack */
    Session_notify_changed(session, DataType_OverlayMapAnimations);

  /* Load or borrow graphics data to display stuff
  (there is no tolerance of bad filenames in the mission file) */
  if (!Session_loadreqgfx(session))
  {
    return false;
  }

  if (!init_anims(session))
  {
    return false;
  }

#ifdef DEBUG_OUTPUT
  clock_t const period = clock() - start;
  if (period > MAX_FILE_PERIOD) {
    DEBUGF("Mission load period: %g\n", (double)period / CLOCKS_PER_SEC);
  }
#endif

  check_tile_range(session);
  check_ref_range(session);

  return true; /* success */
}

void Session_open_mission(char const *const filename)
{
  EditSession *session = strdict_find_value(&mission_dict, filename, NULL);
  if (session)
  {
    show_all_edit_wins(session);
  }
  else
  {
    session = create_session(UI_TYPE_MISSION, false, filename);
    if (session)
    {
      bool success = false;

      /* Filename is in base form: e.g. "E.E_01" or "U.MyMission" */
      char *const full_path = make_file_path_in_dir_on_path(LEVELS_PATH, MISSION_DIR, filename);
      if (full_path)
      {
        success = load_mission_core(session, full_path) &&
                  Session_new_edit_win(session, NULL);
        free(full_path);
      }

      if (!success)
      {
        Session_destroy(session);
      }
    }
  }
}

void Session_new_mission(void)
{
  EditSession *const session = create_session(UI_TYPE_MISSION, false, NULL);
  if (session)
  {
    if (!load_mission_core(session, "<"APP_NAME"$dir>.Defaults.Mission") ||
        !Session_new_edit_win(session, NULL))
    {
      Session_destroy(session);
    }
  }
}

InfoEditContext const *Session_get_infos(EditSession *const session)
{
  assert(session != NULL);
  return &session->infos;
}

FilenamesData *Session_get_filenames(EditSession *const session)
{
  assert(session != NULL);
  return session->mission ? mission_get_filenames(session->mission) :
                                 &session->gfx_config.filenames;
}

CloudColData *Session_get_cloud_colours(EditSession *const session)
{
  assert(session != NULL);
  return session->mission ? mission_get_cloud_colours(session->mission) :
                                 &session->gfx_config.clouds;
}

HillColData const *Session_get_hill_colours(EditSession const *const session)
{
  assert(session != NULL);
  return session->hill_colours;
}

ObjEditContext *Session_get_objects(EditSession *const session)
{
  assert(session != NULL);
  return &session->objects;
}

MapEditContext const *Session_get_map(EditSession const *const session)
{
  assert(session != NULL);
  return &session->map;
}

void Session_display_msg(EditSession *const session, char const *hint, bool const temp)
{
  assert(session != NULL);
  NOT_USED(temp);
  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_display_hint(&this_edit_win->edit_win, hint);
  }
}

void Session_splat_anims(EditSession *const session)
{
  /* Initialise ground map from animations data */
  MapEditChanges change_info;
  MapEditChanges_init(&change_info);

  MapEdit_reset_anims(Session_get_map(session));
  MapEdit_anims_to_map(Session_get_map(session), &change_info);
  if (!change_info.tiles_changed)
    return;

  char num_str[12];
  sprintf(num_str, "%lu", change_info.tiles_changed);
  err_report(DUMMY_ERRNO, msgs_lookup_subn("AniSplat", 1, num_str));
  Session_notify_changed(session, (Session_get_map(session)->overlay !=
                         NULL ? DataType_OverlayMap : DataType_BaseMap));
}

static DataType const fnames_to_keep[] = {
  DataType_OverlayMap, DataType_OverlayObjects, DataType_OverlayMapAnimations};

static void keep_fnames(EditSession *const session,
  Filename (*const fnames)[ARRAY_SIZE(fnames_to_keep)])
{
  MissionData *const mission = Session_get_mission(session);
  if (mission) {
    FilenamesData *const nf = mission_get_filenames(mission);
    for (size_t i = 0; i < ARRAY_SIZE(fnames_to_keep); ++i)
    {
      STRCPY_SAFE((*fnames)[i], filenames_get(nf, fnames_to_keep[i]));
    }
  }
}

static void mission_replaced(EditSession *const session,
  Filename (*const fnames)[ARRAY_SIZE(fnames_to_keep)])
{
  /* Copy the leaf paths of ancillary files from current mission data
     to mission data just loaded. (Otherwise we might logically end
     up having to revert those also, which is not what user expects.) */
  for (size_t i = 0; i < ARRAY_SIZE(fnames_to_keep); ++i)
  {
    MissionData *const mission = Session_get_mission(session);
    if (mission) {
      FilenamesData *const nf = mission_get_filenames(mission);
      if (stricmp((*fnames)[i], filenames_get(nf, fnames_to_keep[i])))
      {
        filenames_set(nf, fnames_to_keep[i], (*fnames)[i]);
        dfile_set_modified(mission_get_dfile(mission));
      }
    }
  }

  Session_resource_change(session, EDITOR_CHANGE_MISSION_REPLACED, NULL);

  Session_get_shared_base_map(session);
  Session_resource_change(session, EDITOR_CHANGE_MAP_ALL_REPLACED, NULL);
  Session_resource_change(session, EDITOR_CHANGE_OBJ_ALL_REPLACED, NULL);

  Session_loadreqgfx(session);
  Session_resource_change(session, EDITOR_CHANGE_TEX_ALL_RELOADED, NULL);
  Session_resource_change(session, EDITOR_CHANGE_GFX_ALL_RELOADED, NULL);
  Session_resource_change(session, EDITOR_CHANGE_POLYGON_COLOURS, NULL);
  Session_resource_change(session, EDITOR_CHANGE_HILL_COLOURS, NULL);

  Session_splat_anims(session);
  check_tile_range(session);
  check_ref_range(session);
}

void Session_reload(EditSession *const session, DataType const data_type)
{
  assert(session != NULL);

  DFile *const dfile = Session_get_dfile(session, data_type);
  if (!dfile) {
    return;
  }
  char const *const fname = dfile_get_name(dfile);
  if (fname) /* not untitled */
  {
    Filename fnames[ARRAY_SIZE(fnames_to_keep)] = {{0}};

    if (data_type == DataType_Mission)
    {
      keep_fnames(session, &fnames);
    }

    if (!check_file_type(fname, data_type) ||
        !read_comp_typed(dfile, fname)) {
      return;
    }

    switch (data_type) {
      case DataType_BaseObjects:
      case DataType_OverlayObjects:
        objects_replaced(session);
        break;

      case DataType_BaseMap:
      case DataType_OverlayMap:
        map_replaced(session);
        break;

      case DataType_Mission:
        mission_replaced(session, &fnames);
        break;

      default:
        break;
    }

    set_edit_win_titles(session); /* remove unsaved indicator from title */
    redraw_all(session);
  }
}

static Filename original_leaf; /* FIXME */

static bool can_revert_individual(EditSession *const session,
  DataType const data_type)
{
  /* Is this file in our external levels directory? */
  DFile *const dfile = Session_get_dfile(session, data_type);
  char const *const file_path = dfile_get_name(dfile);
  if (!file_path) /* untitled */
  {
    return false;
  }
  DEBUG("Full path of file to revert: '%s'", file_path);

  char *canon_ext_dir = NULL;
  if (E(canonicalise(&canon_ext_dir, NULL, NULL, Config_get_extern_levels_dir())))
    return false;

  DEBUG("Canonicalised path of ext. dir: '%s'", canon_ext_dir);
  size_t const root_len = strlen(canon_ext_dir);
  bool const is_external = (strnicmp(canon_ext_dir, file_path, root_len) == 0);
  free(canon_ext_dir);

  if (!is_external) {
    DEBUG("File is not in external levels dir");
    return false;
  }

  /* Reversion to original may be possible - this file was loaded from
     the external levels directory */
  DEBUG("File was loaded from ext. dir - good");

  strcpy(original_leaf,
         pathtail(file_path,
           Session_get_ui_type(session) == UI_TYPE_MISSION ? 2 : 1));

  bool may_revert = false;
  {
    /* Simply look for a file of the same name in the internal game
       directory */
    char *const intern_path = make_file_path_in_dir(Config_get_read_dir(), file_path + root_len + 1);
    if (intern_path == NULL) {
      return false;
    }
    may_revert = file_exists(intern_path);
    free(intern_path);
  }

  if (!may_revert && Session_get_ui_type(session) == UI_TYPE_MAP) {
    /* For a base map that has any files in the internal game
       directory (but not for this specific type of data) we allow
       'reversion' to blank */
    if (data_type != DataType_BaseMap && !may_revert) {
      DEBUG("Checking for base ground map file in int. dir");
      char *const intern_path = make_file_path_in_subdir(
                                     Config_get_read_dir(), data_type_to_sub_dir(DataType_BaseMap), original_leaf);
      if (intern_path == NULL) {
        return false;
      }

      may_revert = file_exists(intern_path);
      free(intern_path);
    }

    if (data_type != DataType_BaseObjects && !may_revert) {
      DEBUG("Checking for base objects map file in int. dir");
      char *const intern_path = make_file_path_in_subdir(
                                     Config_get_read_dir(), data_type_to_sub_dir(DataType_BaseObjects), original_leaf);
      if (intern_path == NULL) {
        return false;
      }

      may_revert = file_exists(intern_path);
      free(intern_path);
    }

    if (data_type != DataType_OverlayMapAnimations && !may_revert) {
      DEBUG("Checking for base animations file in int. dir");
      char *const intern_path = make_file_path_in_subdir(Config_get_read_dir(), data_type_to_sub_dir(DataType_BaseMapAnimations),
                                     original_leaf);
      if (intern_path == NULL) {
        return false;
      }

      may_revert = file_exists(intern_path);
      free(intern_path);
    }

    if (may_revert)
      strcpy(original_leaf, BLANK_FILE);
  }

  return may_revert;
}

static bool get_filename_from_miss_file(Filename *const out,
  char const *const filename, DataType const data_type)
{
  struct FilenamesData filenames;
  if (!filepaths_get_mission_filenames(filename, &filenames))
  {
    return false;
  }
  char const *miss_name = filenames_get(&filenames, data_type);
  DEBUG("Copying leafname '%s' from original mission data", miss_name);
  STRCPY_SAFE(*out, miss_name);
  return true;
}

static bool can_revert_mission_part(EditSession *const session,
  DataType const data_type)
{
  /* Must load original mission file to get leaf name to revert.
  (May have changed if originally non-canonically named, was 'Blank' or
  has been ditched in favour of 'Blank'. Note that in latter case the file
  currently in use - dfile_get_name(dfile) - is unlikely to be in external
  levels directory.) */

  if (!Session_can_quick_save(session))
    return false;

  DFile *const dfile = Session_get_dfile(session, data_type);
  char const *const file_path = dfile_get_name(dfile);
  if (!file_path)
  {
    return false; /* untitled */
  }

  {
    char const *const main_filename = Session_get_filename(session);
    char *const miss_read_path = make_file_path_in_subdir(
                                         Config_get_read_dir(), MISSION_DIR, main_filename);
    if (!miss_read_path) {
      return false;
    }

    bool got_filename = file_exists(miss_read_path) &&
                        get_filename_from_miss_file(&original_leaf, miss_read_path, data_type);

    free(miss_read_path);

    if (!got_filename)
      return false;
  }

  /* Construct complete path to internal file to revert to */
  char const *const sub_dir = data_type_to_sub_dir(data_type);
  char *const intern_path = make_file_path_in_subdir(Config_get_read_dir(), sub_dir, original_leaf);
  if (intern_path == NULL) {
    return false;
  }

  DEBUG("Path of internal file to revert to: '%s'", intern_path);

  /* May only revert if it's a different file */
  bool const diff_file = (stricmp(intern_path, file_path) != 0);

  /* Check original file still exists */
  bool const orig_exists = file_exists(intern_path);
  free(intern_path);

  return diff_file && orig_exists;
}

bool Session_can_revert_to_original(EditSession *const session, DataType const data_type)
{
  /* Only possible to allow reversion to the 'original' file if we are using
  an external levels directory */
  bool may_revert = true;

  if (Config_get_use_extern_levels_dir())
  {
    /* Is this an ancillary mission file we are planning to revert? */
    if (Session_get_ui_type(session) == UI_TYPE_MISSION &&
        !session->oddball_file &&
        data_type != DataType_Mission)
    {
      may_revert = can_revert_mission_part(session, data_type);
    }
    else
    {
      may_revert = can_revert_individual(session, data_type);
    }
  } else {
    DEBUG("No external levels dir configured");
    may_revert = false;
  }

  DEBUG("Will %sallow reversion to original", may_revert ? "" : "dis");
  if (may_revert)
    DEBUG("Internal file to revert to: %s", original_leaf);

  return may_revert;
}

int Session_count_modified(EditSession const *const session)
{
  int count = 0;

  for (DataType data_type = DataType_First; data_type < DataType_SessionCount; ++data_type)
  {
    if (Session_file_modified(session, data_type))
    {
      ++count;
    }
  }

  DEBUG("%d files with unsaved changes counted", count);
  return count;
}

int Session_all_count_modified(void)
{
  int count = 0;

  LINKEDLIST_FOR_EACH(&all_list, item)
  {
    EditSession *const session = CONTAINER_OF(item, EditSession, all_link);
    count += Session_count_modified(session);
  }

  return count;
}

void Session_all_delete(void)
{
  LINKEDLIST_FOR_EACH_SAFE(&all_list, item, tmp)
  {
    EditSession *const session = CONTAINER_OF(item, EditSession, all_link);
    Session_destroy(session);
  }
}

EditWin *Session_edit_win_from_wimp_handle(int const window)
{
  LINKEDLIST_FOR_EACH(&all_list, item)
  {
    EditSession *const session = CONTAINER_OF(item, EditSession, all_link);
    EditWinList *const this_edit_win = intdict_find_value(&session->edit_wins_array, window, NULL);
    if (this_edit_win) {
      return &this_edit_win->edit_win;
    }
  }
  return NULL;
}

bool Session_drag_obj_link(EditSession *const session, int const window, int const icon,
  Editor *const origin)
{
#if PER_VIEW_SELECT
  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    if (Editor_drag_obj_link(EditWin_get_editor(&this_edit_win->edit_win), window, icon, origin)) {
      return true;
    }
  }
  return false;
#else
  return session->has_editor ? Editor_drag_obj_link(&session->editor, window, icon, origin) : false;
#endif
}

void Session_resource_change(EditSession *const session,
  EditorChange const event, EditorChangeParams const *const params)
{
  DEBUGF("Broadcasting change %s\n", EditorChange_to_string(event));
  switch (event)
  {
  case EDITOR_CHANGE_TEX_TRANSFER_ADDED:
  case EDITOR_CHANGE_TEX_TRANSFER_DELETED:
  case EDITOR_CHANGE_TEX_TRANSFER_REPLACED:
  case EDITOR_CHANGE_GFX_TRANSFER_ADDED:
  case EDITOR_CHANGE_GFX_TRANSFER_DELETED:
  case EDITOR_CHANGE_GFX_TRANSFER_REPLACED:
    DEBUGF("Transfer index %zu\n", params->transfer_added.index);
    break;
  case EDITOR_CHANGE_TEX_TRANSFER_RENAMED:
  case EDITOR_CHANGE_GFX_TRANSFER_RENAMED:
    DEBUGF("Transfer index %zu, new index %zu\n",
           params->transfer_renamed.index, params->transfer_renamed.new_index);
    break;
  case EDITOR_CHANGE_OBJ_PRECHANGE:
  case EDITOR_CHANGE_MAP_PRECHANGE:
    DEBUGF("Replaced area {%" PRIMapCoord ",%" PRIMapCoord "},{%"
           PRIMapCoord ",%" PRIMapCoord "}\n",
           params->obj_prechange.bbox.min.x, params->obj_prechange.bbox.min.y,
           params->obj_prechange.bbox.max.x, params->obj_prechange.bbox.max.y);
    break;
  case EDITOR_CHANGE_OBJ_PREMOVE:
  case EDITOR_CHANGE_MAP_PREMOVE:
    DEBUGF("Old position {%" PRIMapCoord ",%" PRIMapCoord "}, new position {%"
           PRIMapCoord ",%" PRIMapCoord "}\n",
           params->obj_premove.old_pos.x, params->obj_premove.old_pos.y,
           params->obj_premove.new_pos.x, params->obj_premove.new_pos.y);
    break;
  case EDITOR_CHANGE_INFO_ADDED:
  case EDITOR_CHANGE_INFO_PREDELETE:
    DEBUGF("Info %p at index %zu\n", params->info_added.info, params->info_added.index);
    break;
  default:
    break;
  }

  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
#if PER_VIEW_SELECT
    Editor_resource_change(EditWin_get_editor(&this_edit_win->edit_win), event, params);
#endif
    EditWin_resource_change(&this_edit_win->edit_win, event, params);
  }

#if !PER_VIEW_SELECT
  if (session->has_editor) {
    Editor_resource_change(&session->editor, event, params);
  }
#endif

  switch (event)
  {
  case EDITOR_CHANGE_BRIEFING:
    set_edit_win_titles(session);
    break;
  default:
    break;
  }

  DEBUGF("Finished change %s\n", EditorChange_to_string(event));
}

void Session_all_graphics_changed(ObjGfx *const graphics, EditorChange const event,
  EditorChangeParams const *const params)
{
  assert(event > EDITOR_CHANGE_GFX_FIRST);
  assert(event < EDITOR_CHANGE_GFX_LIMIT);

  LINKEDLIST_FOR_EACH(&all_list, item)
  {
    EditSession *const session = CONTAINER_OF(item, EditSession, all_link);
    if (Session_get_graphics(session) == graphics)
    {
      Session_resource_change(session, event, params);
    }
  }
}

void Session_all_textures_changed(MapTex *const textures, EditorChange const event,
  EditorChangeParams const *const params)
{
  assert(event > EDITOR_CHANGE_TEX_FIRST);
  assert(event < EDITOR_CHANGE_TEX_LIMIT);

  LINKEDLIST_FOR_EACH(&all_list, item)
  {
    EditSession *const session = CONTAINER_OF(item, EditSession, all_link);
    if (Session_get_textures(session) == textures)
    {
      Session_resource_change(session, event, params);
    }
  }
}

void Session_revert_to_original(EditSession *const session, DataType const data_type)
{
  /* Special case: for historical reasons there is no
     'Blank' file in the base animations directory */
  char const *const sub_dir = data_type_to_sub_dir(
    data_type == DataType_BaseMapAnimations ? DataType_OverlayMapAnimations : data_type);

  DEBUG("Sub directory is '%s'", sub_dir);

  DFile *const dfile = Session_get_dfile(session, data_type);
  if (!dfile) {
    return;
  }

  /* Load original file (as determined in about_to_be_shown) */
  char *const new_path = make_file_path_in_subdir(Config_get_read_dir(), sub_dir, original_leaf);
  if (new_path == NULL) {
    return;
  }

  Filename fnames[ARRAY_SIZE(fnames_to_keep)] = {{0}};

  if (data_type == DataType_Mission)
  {
    keep_fnames(session, &fnames);
  }

  bool const success = check_file_type(new_path, data_type) &&
                       read_comp_typed(dfile, new_path);
  free(new_path);

  if (!success) {
    return;
  }

  switch (data_type)
  {
    case DataType_BaseObjects:
    case DataType_OverlayObjects:
      objects_replaced(session);
      break;

    case DataType_BaseMap:
    case DataType_OverlayMap:
      map_replaced(session);
      break;

    case DataType_BaseMapAnimations:
    case DataType_OverlayMapAnimations:
      Session_splat_anims(session);
      break;

    case DataType_Mission:
      mission_replaced(session, &fnames);
      break;

    default:
      assert("Unknown data type" == NULL);
      break;
  }

  set_edit_win_titles(session); /* maybe remove unsaved indicator from title */
  redraw_all(session);
}

bool Session_switch_file(EditSession *const session, DataType const data_type,
  char const *leaf_name)
{
  FilenamesData *const filenames = Session_get_filenames(session);

  /* Not a mechanism for loading files that were intentionally not loaded. */
  if (session->oddball_file && Session_has_data(session, DataType_Mission) &&
      data_type != DataType_PolygonMeshes && data_type != DataType_PolygonColours)
  {
    return false;
  }

  if (!get_shared_leaf(session, data_type, leaf_name))
  {
    return false;
  }

  GfxConfig base_map_gfx;
  switch (data_type)
  {
  case DataType_BaseObjects:
    /* When changing base objects map we check if the current polygonal
       objects set is suitable and invite the user to change it if not */
    if (GfxConfig_load(&base_map_gfx, leaf_name))
    {
      char const *const pname = filenames_get(&base_map_gfx.filenames, DataType_PolygonMeshes);
      if (stricmp(pname, filenames_get(filenames, DataType_PolygonMeshes)))
      {
        if (get_shared_leaf(session, DataType_PolygonMeshes, pname)) {
          filenames_set(filenames, data_type, pname);
          Session_resource_change(session, EDITOR_CHANGE_GFX_ALL_RELOADED, NULL);
        }
      }
    }
    objects_replaced(session);
    break;

  case DataType_BaseMap:
    /* When changing base ground map we check if the current tile graphics set
       and hill colours are suitable and invite the user to change them if not */
    if (GfxConfig_load(&base_map_gfx, leaf_name))
    {
      char const *const tname = filenames_get(&base_map_gfx.filenames, DataType_MapTextures);
      char const *const hname = filenames_get(&base_map_gfx.filenames, DataType_HillColours);
      if (stricmp(tname, filenames_get(filenames, DataType_MapTextures)) ||
          stricmp(hname, filenames_get(filenames, DataType_HillColours)))
      {
        if (get_shared_leaf(session, DataType_MapTextures, tname)) {
          filenames_set(filenames, DataType_MapTextures, tname);
          Session_resource_change(session, EDITOR_CHANGE_TEX_ALL_RELOADED, NULL);
        }

        if (get_shared_leaf(session, DataType_HillColours, hname)) {
          filenames_set(filenames, DataType_HillColours, hname);
          Session_resource_change(session, EDITOR_CHANGE_HILL_COLOURS, NULL);
        }
      }
    }
    map_replaced(session);
    break;

  case DataType_MapTextures:
    Session_resource_change(session, EDITOR_CHANGE_TEX_ALL_RELOADED, NULL);
    check_tile_range(session);
    break;

  case DataType_PolygonMeshes:
    Session_resource_change(session, EDITOR_CHANGE_GFX_ALL_RELOADED, NULL);
    check_ref_range(session);
    break;

  case DataType_PolygonColours:
    Session_resource_change(session, EDITOR_CHANGE_POLYGON_COLOURS, NULL);
    break;

  case DataType_HillColours:
    Session_resource_change(session, EDITOR_CHANGE_HILL_COLOURS, NULL);
    break;

  default:
    break;
  }

  filenames_set(filenames, data_type, leaf_name);
  Session_notify_changed(session, DataType_Mission);
  redraw_all(session);

  return true;
}

bool Session_has_data(const EditSession *const session, DataType const data_type)
{
  return Session_get_dfile(session, data_type) != NULL;
}

int Session_get_file_size(EditSession const *const session, DataType const data_type)
{
  DFile *const dfile = Session_get_dfile(session, data_type);
  return dfile ? get_compressed_size(dfile) : 0;
}

bool Session_file_modified(EditSession const *const session, DataType const data_type)
{
  DFile const *const dfile = Session_get_dfile(session, data_type);
  return dfile ? dfile_get_modified(dfile) : false;
}

int const *Session_get_file_date(EditSession *const session, DataType const data_type)
{
  DFile const *const dfile = Session_get_dfile(session, data_type);
  static int const dummy[2];
  return dfile ? dfile_get_date(dfile) : dummy;
}

char *Session_get_file_name(EditSession *const session, DataType const data_type)
{
  DFile const *const dfile = Session_get_dfile(session, data_type);
  return dfile ? dfile_get_name(dfile) : NULL /* untitled */;
}

char *Session_get_file_name_for_save(EditSession *const session,
  DataType const data_type)
{
  char *filename = NULL;
  assert(session != NULL);
  if (session->oddball_file)
  {
    /* We encourage user to save oddball files back to whence they came */
    filename = strdup(Session_get_file_name(session, data_type));
  }
  else
  {
    /* Construct suggested path to which to save file */
    char const *const leaf_name = Session_get_save_filename(session);
    DEBUG("Leaf name is '%s'", leaf_name);

    char const *const sub_dir = data_type_to_sub_dir(data_type);
    DEBUG("Sub directory is '%s'", sub_dir);
    filename = make_file_path_in_subdir(Config_get_write_dir(), sub_dir, leaf_name);
  }
  return filename;
}

bool Session_save_file(EditSession *const session, DataType const data_type,
  char *const filename)
{
  assert(session != NULL);
  assert(data_type >= 0);
  assert(data_type < ARRAY_SIZE(session->dfiles));
  DEBUG("Saving file %d as '%s'", data_type, filename);

  switch (data_type) {
    case DataType_OverlayMap:
      /* Reset animations to ensure the correct initial state */
      restart_anims(session);
      break;

    case DataType_BaseMap:
    case DataType_BaseObjects:
    case DataType_OverlayObjects:
    case DataType_BaseMapAnimations:
    case DataType_OverlayMapAnimations:
      break;

    case DataType_Mission:
      break;

    default:
      assert("Unknown data type" == NULL);
      return false;
  }

  DFile *const dfile = Session_get_dfile(session, data_type);
  if (dfile)
  {
    if (!write_comp_typed(dfile, filename, data_type)) {
      return false;
    }
  }
  return true;
}

InterfaceType Session_get_ui_type(const EditSession *const session)
{
  assert(session != NULL);
  return session->ui_type;
}

MapTex *Session_get_textures(const EditSession *const session)
{
  assert(session != NULL);
  assert(session->textures != NULL);
  return session->textures;
}

ObjGfx *Session_get_graphics(const EditSession *const session)
{
  assert(session != NULL);
  assert(session->graphics != NULL);
  return session->graphics;
}

PolyColData const *Session_get_poly_colours(const EditSession *const session)
{
  assert(session != NULL);
  assert(session->poly_colours != NULL);
  return session->poly_colours;
}

bool Session_can_quick_save(const EditSession *const session)
{
  assert(session != NULL);
  return !session->untitled;
}

bool Session_can_save_all(const EditSession *const session)
{
  assert(session != NULL);
  return !session->oddball_file;
}

bool Session_quick_save(EditSession *const session)
{
  bool success = false;

  assert(session != NULL);
  if (Session_can_quick_save(session)) {
    if (Session_get_ui_type(session) == UI_TYPE_MISSION) {
      success = Session_savemission(session, Session_get_filename(session), false);
    } else {
      success = Session_savemap(session, Session_get_filename(session), false);
    }
  }

  return success;
}

bool Session_get_anims_shown(const EditSession *const session)
{
  assert(session != NULL);
  return session->desired_animate_map;
}

void Session_set_anims_shown(EditSession *const session, bool const shown)
{
  assert(session != NULL);

  if (shown != session->desired_animate_map) {
    session->desired_animate_map = shown;
    if (shown) {
      start_anims(session);
    } else {
      stop_anims(session);
    }
  }
}

void Session_show_special(EditSession *const session)
{
  assert(session != NULL);

  if (!session->has_special_ship) {
    session->has_special_ship = SpecialShip_init(
      &session->special_ship_data, session);
  }

  if (session->has_special_ship) {
    SpecialShip_show(&session->special_ship_data);
  }
}

static void show_perf_big(EditSession *const session,
  ShipType const ship_type)
{
  assert(session != NULL);
  assert(ship_type >= ShipType_Big1);
  size_t const i = ship_type - ShipType_Big1;
  assert(i < ARRAY_SIZE(session->has_bperf));

  if (!session->has_bperf[i])
  {
    session->has_bperf[i] = BPerfDbox_init(
      &session->bperf[i], session, ship_type);
  }

  if (session->has_bperf[i])
  {
    BPerfDbox_show(&session->bperf[i]);
  }
}

static void show_perf_fighter(EditSession *const session,
  ShipType const ship_type)
{
  assert(session != NULL);
  assert(ship_type >= ShipType_Fighter1);
  size_t const i = ship_type - ShipType_Fighter1;
  assert(i < ARRAY_SIZE(session->has_fperf));

  if (!session->has_fperf[i])
  {
    session->has_fperf[i] = FPerfDbox_init(
      &session->fperf[i], session, ship_type);
  }

  if (session->has_fperf[i])
  {
    FPerfDbox_show(&session->fperf[i]);
  }
}

void Session_show_performance(EditSession *const session,
  ShipType const ship_type)
{
  switch (ship_type)
  {
  case ShipType_Fighter1:
  case ShipType_Fighter2:
  case ShipType_Fighter3:
  case ShipType_Fighter4:
    show_perf_fighter(session, ship_type);
    break;
  case ShipType_Big1:
  case ShipType_Big2:
  case ShipType_Big3:
    show_perf_big(session, ship_type);
    break;
  default:
    break;
  }
}

void Session_show_briefing(EditSession *const session)
{
  assert(session != NULL);

  if (!session->has_briefing) {
    session->has_briefing = BriefDbox_init(
      &session->briefing_data, session);
  }

  if (session->has_briefing)
    BriefDbox_show(&session->briefing_data);
}

#if !PER_VIEW_SELECT
Editor *Session_get_editor(EditSession *const session)
{
  assert(session != NULL);
  return session->has_editor ? &session->editor : NULL;
}

void Session_set_help_and_ptr(EditSession *const session,
  char *const help, PointerType const ptr)
{
  assert(session != NULL);
  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_set_help_and_ptr(&this_edit_win->edit_win, help, ptr);
  }
}

void Session_display_mode(EditSession *const session)
{
  assert(session != NULL);
  INTDICT_FOR_EACH(&session->edit_wins_array, index, tmp) {
    EditWinList *const this_edit_win = intdict_get_value_at(&session->edit_wins_array, index);
    EditWin_display_mode(&this_edit_win->edit_win);
  }
}
#else
EditWin *Session_editor_to_win(Editor *const editor)
{
  assert(editor);
  EditWinList *const this_edit_win = CONTAINER_OF(editor, EditWinList, editor);
  if (!this_edit_win->edit_win_is_valid) {
    return NULL;
  }
  return &this_edit_win->edit_win;
}
#endif
