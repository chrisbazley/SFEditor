/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Strategic target information properties dialogue box
 *  Copyright (C) 2022 Christopher Bazley
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

#include <assert.h>
#include <limits.h>
#include <inttypes.h>

#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"
#include "wimplib.h"
#include "textarea.h"

#include "msgtrans.h"
#include "Macros.h"
#include "err.h"
#include "EventExtra.h"
#include "PathTail.h"
#include "IntDict.h"

#include "Session.h"
#include "EditWin.h"
#include "debug.h"
#include "IPropDbox.h"
#include "EditWin.h"
#include "Map.h"
#include "utils.h"
#include "infos.h"
#include "infomode.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_Location = 0x21,
  ComponentId_TargetType = 0x22,
  ComponentId_TargetDetails = 0x24,
  ComponentId_Cancel = 0xc,
  ComponentId_Set = 0xb,
};

typedef struct
{
  InfoPropDboxes *prop_dboxes;
  ObjectId my_object;
  TargetInfo *info;
} InfoPropDbox;

static ComponentId const text_index_to_component[] = {
  [TargetInfoTextIndex_Type] = ComponentId_TargetType,
  [TargetInfoTextIndex_Details] = ComponentId_TargetDetails,
};

/* ---------------- Private functions ---------------- */

static Editor *get_editor(InfoPropDbox const *const prop)
{
  assert(prop != NULL);
  assert(prop->prop_dboxes != NULL);
  return prop->prop_dboxes->editor;
}

static EditSession *get_session(InfoPropDbox const *const prop)
{
  return Editor_get_session(get_editor(prop));
}

static void delete_dbox(InfoPropDbox *const prop)
{
  assert(prop != NULL);
  E(remove_event_handlers_delete(prop->my_object));
  free(prop);
}

static void disp_pos(InfoPropDbox const *const prop)
{
  assert(prop);

  char string[24] = "";
  MapPoint const pos = target_info_get_pos(prop->info);
  sprintf(string, "%3.3"PRIMapCoord",%3.3"PRIMapCoord, pos.x, pos.y);
  E(displayfield_set_value(0, prop->my_object, ComponentId_Location, string));
}

static void setup_win(InfoPropDbox *const prop)
{
  assert(prop);

  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    ComponentId const textarea = text_index_to_component[k];
    E(textarea_set_text(0, prop->my_object, textarea, target_info_get_text(prop->info, k)));
  }

  disp_pos(prop);
}

static bool read_win(InfoPropDbox *const prop)
{
  assert(prop);

  char *buffers[TargetInfoTextIndex_Count] = {NULL};
  char const *strings[TargetInfoTextIndex_Count] = {NULL};
  bool ok = true;

  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    ComponentId const textarea = text_index_to_component[k];

    int nbytes = 0;
    if (E(textarea_get_text(0, prop->my_object, textarea, NULL, 0, &nbytes)) ||
        nbytes < 0) {
      ok = false;
      break;
    }

    buffers[k] = malloc((unsigned)nbytes);
    if (buffers[k] == NULL) {
      report_error(SFERROR(NoMem), "", "");
      ok = false;
      break;
    }

    if (E(textarea_get_text(0, prop->my_object, textarea, buffers[k], nbytes, NULL))) {
      ok = false;
      break;
    }
    strings[k] = buffers[k];
  }

  if (ok) {
    ok = InfoMode_set_properties(get_editor(prop), prop->info, strings);
  }

  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    free(buffers[k]);
  }

  return ok;
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  InfoPropDbox *const prop = handle;

  switch (id_block->self_component) {
    case ComponentId_Cancel:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win(prop);
      }
      break;

    case ComponentId_Set:
      /* read settings from window */
      if (read_win(prop)) {
        if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Select)) {
          E(toolbox_hide_object(0, id_block->self_id));
        }
      }
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* event handled */
}

static int iprop_about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(id_block);
  NOT_USED(event_code);
  NOT_USED(event);
  InfoPropDbox *const prop = handle;
  setup_win(prop);
  return 1;
}

static int has_been_hidden(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  InfoPropDbox *const prop = handle;
  assert(prop);
  InfoPropDbox *const removed = intdict_remove_value(&prop->prop_dboxes->sa,
                                   map_coords_to_key(target_info_get_pos(prop->info)), NULL);
  assert(removed == prop);
  NOT_USED(removed);
  delete_dbox(prop);
  return 1;
}

static bool register_event_handlers(InfoPropDbox *const prop)
{
  assert(prop != NULL);
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } tbox_handlers[] = {
    { Window_AboutToBeShown, iprop_about_to_be_shown },
    { Window_HasBeenHidden, has_been_hidden },
    { ActionButton_Selected, actionbutton_selected },
  };

  for (size_t i = 0; i < ARRAY_SIZE(tbox_handlers); ++i) {
    if (E(event_register_toolbox_handler(prop->my_object,
                                      tbox_handlers[i].event_code,
                                      tbox_handlers[i].handler,
                                      prop)))
      return false;
  }

  return true;
}

static void update_title(InfoPropDbox *const prop)
{
  assert(prop != NULL);
  char const *const file_name = Session_get_filename(get_session(prop));
  E(window_set_title(0, prop->my_object,
                     msgs_lookup_subn("IPropTitle", 1, pathtail(file_name, 1))));
}

static InfoPropDbox *create_dbox(InfoPropDboxes *const prop_dboxes, TargetInfo *const info)
{
  assert(prop_dboxes != NULL);
  DEBUGF("Creating properties dbox for target info %p\n", (void *)info);

  InfoPropDbox *const prop = malloc(sizeof(*prop));
  if (!prop) {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }

  *prop = (InfoPropDbox){
    .prop_dboxes = prop_dboxes,
    .info = info,
  };

  if (!E(toolbox_create_object(0, "InfoProp", &prop->my_object))) {
    DEBUG("InfoProp object id is %d", prop->my_object);

  for (TargetInfoTextIndex k = TargetInfoTextIndex_First;
       k < TargetInfoTextIndex_Count;
       ++k) {
    ComponentId const textarea = text_index_to_component[k];
    E(textarea_set_font(0, prop->my_object, textarea, "Corpus.Bold", 150, 225));
  }

    if (register_event_handlers(prop)) {
      if (!intdict_insert(&prop_dboxes->sa, map_coords_to_key(target_info_get_pos(info)), prop, NULL)) {
        report_error(SFERROR(NoMem), "", "");
      } else {
        update_title(prop);
        return prop;
      }
    }
    (void)remove_event_handlers_delete(prop->my_object);
  }
  free(prop);
  return NULL;
}

static void destroy_cb(IntDictKey const key, void *const data, void *const arg)
{
  NOT_USED(key);
  NOT_USED(arg);
  delete_dbox(data);
}

/* ---------------- Public functions ---------------- */

void InfoPropDboxes_init(InfoPropDboxes *const prop_dboxes, Editor *editor)
{
  assert(prop_dboxes);
  assert(editor);
  *prop_dboxes = (InfoPropDboxes){.editor = editor};
  intdict_init(&prop_dboxes->sa);
}

void InfoPropDboxes_destroy(InfoPropDboxes *const prop_dboxes)
{
  assert(prop_dboxes);
  intdict_destroy(&prop_dboxes->sa, destroy_cb, NULL);
}

void InfoPropDboxes_update_title(InfoPropDboxes *const prop_dboxes)
{
  assert(prop_dboxes);
  IntDictVIter iter;
  for (InfoPropDbox *prop_dbox = intdictviter_all_init(&iter, &prop_dboxes->sa);
       prop_dbox != NULL;
       prop_dbox = intdictviter_advance(&iter)) {
    update_title(prop_dbox);
  }
}

static InfoPropDbox *find_dbox_for_info(InfoPropDboxes *const prop_dboxes,
                                        TargetInfo const *const info, MapPoint const pos,
                                        bool const remove)
{
  /* Dialogue boxes are indexed by map coordinates not info address, and multiple info
     objects can occupy the same location. Should still be quicker than searching the whole array
     in most cases. */
  IntDictKey const key = map_coords_to_key(pos);
  IntDictVIter iter;
  for (InfoPropDbox *prop_dbox = intdictviter_init(&iter, &prop_dboxes->sa, key, key);
       prop_dbox != NULL;
       prop_dbox = intdictviter_advance(&iter)) {
    if (prop_dbox->info == info) {
      if (remove) {
        intdictviter_remove(&iter);
      }
      return prop_dbox;
    }
  }
  return NULL;
}

void InfoPropDboxes_update_for_move(InfoPropDboxes *const prop_dboxes,
                                    TargetInfo const *const info, MapPoint const old_pos)
{
  if (map_coords_compare(target_info_get_pos(info), old_pos)) {
    return;
  }

  InfoPropDbox *const prop_dbox = find_dbox_for_info(prop_dboxes, info, old_pos, true);
  if (!prop_dbox) {
    return;
  }

  if (intdict_insert(&prop_dboxes->sa, map_coords_to_key(target_info_get_pos(info)), prop_dbox, NULL)) {
    disp_pos(prop_dbox);
  } else {
    report_error(SFERROR(NoMem), "", "");
    delete_dbox(prop_dbox);
  }
}

void InfoPropDboxes_update_for_del(InfoPropDboxes *const prop_dboxes, TargetInfo const *const info)
{
  InfoPropDbox *const prop_dbox = find_dbox_for_info(prop_dboxes, info, target_info_get_pos(info), true);
  if (!prop_dbox) {
    return;
  }
  delete_dbox(prop_dbox);
}

void InfoPropDboxes_open(InfoPropDboxes *const prop_dboxes, TargetInfo *const info,
                         EditWin *const edit_win)
{
  InfoPropDbox *prop_dbox = find_dbox_for_info(prop_dboxes, info, target_info_get_pos(info), false);
  if (!prop_dbox) {
    prop_dbox = create_dbox(prop_dboxes, info);
  }
  if (prop_dbox) {
    EditWin_show_dbox(edit_win, 0, prop_dbox->my_object);
  }
}
