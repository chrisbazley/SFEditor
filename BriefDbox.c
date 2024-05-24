/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission briefing dialogue box
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

#include "stdlib.h"

#include "toolbox.h"
#include "event.h"
#include "window.h"
#include "gadgets.h"
#include "textarea.h"

#include "eventextra.h"

#include "debug.h"
#include "Macros.h"
#include "Session.h"
#include "utils.h"
#include "err.h"
#include "msgtrans.h"
#include "pathtail.h"
#include "Mission.h"
#include "SFError.h"
#include "briefingdata.h"

#include "BriefDbox.h"

#define ENDPARA "$\n"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_Cancel    = 0x17,
  ComponentId_OK        = 0x18,
  ComponentId_TextArea  = 0x0,
};

/* ---------------- Private functions ---------------- */

static bool read_brief_win(BriefingData *const briefing, ObjectId const id)
{
  int nbytes = 0;
  if (E(textarea_get_text(0, id, ComponentId_TextArea, NULL, 0, &nbytes)) ||
      nbytes < 0) {
    return false;
  }

  char *const buffer = malloc((unsigned)nbytes);
  if (buffer == NULL) {
    report_error(SFERROR(NoMem), "", "");
    return false;
  }

  if (E(textarea_get_text(0, id, ComponentId_TextArea, buffer, nbytes, NULL))) {
    free(buffer);
    return false;
  }

  BriefingData new_briefing;
  briefing_init(&new_briefing);

  bool success = true;
  char *string = buffer, *end;
  do {
    end = strchr(string, ENDPARA[0]);
    if (end != NULL) {
      // Replace the end-of-paragraph marker with a string terminator
      DEBUGF("End of paragraph at %d: %c\n", end - buffer, *end);
      *end = '\0';
    }
    DEBUGF("String of length %zu at %d: %c\n", strlen(string), string - buffer, *string);
    if (report_error(briefing_add_text(&new_briefing, string), "", "")) {
      success = false;
      break;
    }

    if (end != NULL) {
      if (end[1] == '\0') {
        end = NULL;
      } else if (end[1] == '\n') {
        string = end + 2;
      } else {
        // Should be nothing after an end-of-paragraph marker on the same line
        report_error(SFERROR(CharsAfterPara), "", "");
        success = false;
        break;
      }
    }
  } while (end != NULL);

  free(buffer);

  if (success) {
    briefing_destroy(briefing);
    *briefing = new_briefing;
  } else {
    briefing_destroy(&new_briefing);
  }
  return success;
}

static void setup_win(BriefingData *const briefing, ObjectId const id)
{
  size_t const tcount = briefing_get_text_count(briefing);
  E(textarea_set_text(0, id, ComponentId_TextArea, ""));
  unsigned int offset = 0;

  for (size_t i = 0; i < tcount; ++i) {
    char const *const text = briefing_get_text(briefing, i);
    E(textarea_insert_text(0, id, ComponentId_TextArea, offset, text));
    offset += strlen(text);
    if (i < tcount - 1) {
      E(textarea_insert_text(0, id, ComponentId_TextArea, offset, ENDPARA));
      offset += sizeof(ENDPARA) - 1;
    }
  }
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  BriefDboxData *const briefing_data = handle;

  switch (id_block->self_component) {
    case ComponentId_Cancel:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win(mission_get_briefing(Session_get_mission(briefing_data->session)), id_block->self_id);
      }
      break;

    case ComponentId_OK:
      /* read settings from window */
      if (read_brief_win(mission_get_briefing(Session_get_mission(briefing_data->session)), id_block->self_id)) {
        Session_resource_change(briefing_data->session, EDITOR_CHANGE_BRIEFING, NULL);
        Session_notify_changed(briefing_data->session, DataType_Mission);
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

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Dialogue window about to open */
  NOT_USED(event_code);
  NOT_USED(event);
  BriefDboxData *const briefing_data = handle;

  setup_win(mission_get_briefing(Session_get_mission(briefing_data->session)), id_block->self_id);

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

bool BriefDbox_init(BriefDboxData *const briefing_data,
                   EditSession *const session)
{
  briefing_data->session = session;

  if (E(toolbox_create_object(0, "Briefing", &briefing_data->my_object)))
  {
    return false;
  }

  E(textarea_set_font(0, briefing_data->my_object, ComponentId_TextArea, "Corpus.Bold", 150, 225));

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { ActionButton_Selected, actionbutton_selected },
    { Window_AboutToBeShown, about_to_be_shown }
  };

  bool success = true;
  for (size_t i = 0; success && (i < ARRAY_SIZE(handlers)); ++i)
  {
    if (E(event_register_toolbox_handler(briefing_data->my_object,
                                         handlers[i].event_code,
                                         handlers[i].handler, briefing_data)))
      success = false;
  }

  if (success)
  {
    BriefDbox_update_title(briefing_data);
  }
  else
  {
    BriefDbox_destroy(briefing_data);
  }

  return success;
}

void BriefDbox_update_title(BriefDboxData *const briefing_data)
{
  assert(briefing_data != NULL);
  char const *const file_name = Session_get_filename(briefing_data->session);
  E(window_set_title(0, briefing_data->my_object,
                              msgs_lookup_subn("BriefTitle", 1,
                              pathtail(file_name, 1))));
}

void BriefDbox_show(BriefDboxData *const briefing_data)
{
  assert(briefing_data != NULL);
  E(toolbox_show_object(0, briefing_data->my_object,
                        Toolbox_ShowObject_Centre, NULL,
                        NULL_ObjectId, NULL_ComponentId));
}

void BriefDbox_destroy(BriefDboxData *const briefing_data)
{
  assert(briefing_data != NULL);
  E(remove_event_handlers_delete(briefing_data->my_object));
}
