/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Editing window toolbox
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
#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#include "toolbox.h"
#include "window.h"
#include "event.h"
#include "gadgets.h"
#include "wimp.h"

#include "msgtrans.h"
#include "err.h"
#include "Macros.h"
#include "scheduler.h"
#include "TboxBugs.h"
#include "debug.h"
#include "GadgetUtil.h"
#include "EventExtra.h"
#include "Deiconise.h"

#include "MapToolbar.h"
#include "Utils.h"
#include "Session.h"
#include "ConfigBrush.h"
#include "ConfigWand.h"
#include "ConfigFill.h"
#include "Palette.h"
#include "PlotMenu.h"
#include "ZoomMenu.h"
#include "EditMenu.h"
#include "Smooth.h"
#include "MSnakes.h"
#include "MTransfers.h"
#include "DataType.h"

/* --------------------- Gadgets -------------------- */

enum {
  ComponentId_Transfer   = 0x3,
  ComponentId_Brush      = 0x4,
  ComponentId_FloodFill  = 0x5,
  ComponentId_Triangle   = 0x6,
  ComponentId_PlotShapes = 0x7,
  ComponentId_SmoothWand = 0x8,
  ComponentId_Snake      = 0x9,
  ComponentId_Rectangle  = 0xa,
  ComponentId_Circle     = 0xb,
  ComponentId_SelectArea = 0xe,
  ComponentId_Magnifier  = 0xf,
  ComponentId_Sampler    = 0x10,
};

#define FREQUENCY 10
#define PRIORITY SchedulerPriority_Min

static ComponentId const tool_buttons[] =
{
  [EDITORTOOL_BRUSH] = ComponentId_Brush,
  [EDITORTOOL_FILLREPLACE] = ComponentId_FloodFill,
  [EDITORTOOL_PLOTSHAPES] = ComponentId_PlotShapes,
  [EDITORTOOL_SAMPLER] = ComponentId_Sampler,
  [EDITORTOOL_SNAKE] = ComponentId_Snake,
  [EDITORTOOL_SMOOTHWAND] = ComponentId_SmoothWand,
  [EDITORTOOL_TRANSFER] = ComponentId_Transfer,
  [EDITORTOOL_SELECT] = ComponentId_SelectArea,
  [EDITORTOOL_MAGNIFIER] = ComponentId_Magnifier,
};

/* ---------------- Private functions ---------------- */

static void hint(MapToolbar *const toolbar, EditorTool const tool)
{
  /* Substitute tool name into generic message text & display */
  Editor_display_msg(toolbar->editor, msgs_lookup_subn(
     tool == EDITORTOOL_NONE ? "StatusToolSel" : "StatusToolHint", 1,
     Editor_get_tool_msg(toolbar->editor, tool, tool == EDITORTOOL_NONE)), false);
}

static EditorTool button_to_tool(ComponentId const button)
{
  if (button != NULL_ComponentId) {
    for (size_t tool = 0; tool < ARRAY_SIZE(tool_buttons); tool++) {
      if (tool_buttons[tool] == button)
        return (EditorTool)tool;
    }
  }
  return EDITORTOOL_NONE;
}

static ComponentId tool_to_button(const EditorTool tool)
{
  if (tool >= 0 && (size_t)tool < ARRAY_SIZE(tool_buttons))
    return tool_buttons[tool];
  else
    return NULL_ComponentId;
}

static SchedulerTime trackpointer(void *const handle,
  SchedulerTime const new_time, const volatile bool *const time_up)
{
  /* Give mouseover help on tool buttons */
  NOT_USED(time_up);
  MapToolbar *const toolbar = handle;
  int buttons;
  ObjectId window;
  ComponentId component;

  ON_ERR_RPT_RTN_V(window_get_pointer_info(0, NULL, NULL, &buttons, &window,
    &component), new_time + FREQUENCY);

  if (TEST_BITS(buttons, Window_GetPointerNotToolboxWindow) ||
      window != toolbar->my_object)
    return new_time + FREQUENCY; /* not interested */

  /* Display tool hint text on status bar */
  if (toolbar->mouse_over_button != component) {
    hint(toolbar, button_to_tool(component));
    toolbar->mouse_over_button = component; /* prevent constant flicker */
  }

  return new_time + FREQUENCY;
}

static int pointer_enter(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Start tracking mouse pointer */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  MapToolbar *const toolbar = handle;

  if (!toolbar->null_poller) {
    if (!E(scheduler_register_delay(trackpointer, toolbar, 0, PRIORITY)))
      toolbar->null_poller = true;
  }

  return 1; /* claim event */
}

static int pointer_leave(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Stop tracking mouse pointer */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  MapToolbar *const toolbar = handle;

  if (toolbar->null_poller) {
    scheduler_deregister(trackpointer, toolbar);
    toolbar->null_poller = false;
  }

  ///MapMode_overridetoolhint(toolbar->editor, EDITORTOOL_NONE);
  hint(toolbar, EDITORTOOL_NONE);
  toolbar->mouse_over_button = NULL_ComponentId;

  return 1; /* claim event */
}

//static int clipboardchanged(int const event_code, ToolboxEvent *const event,
//  IdBlock *const id_block, void *const handle)
//{
//  /* Clipboard contents changed - update paste button */
//  EditWin *const editor = handle;
//
//  set_gadget_faded((clipboard == NULL || clipboard_mode != EDITING_MODE_MAP), editor->tool_bar_tboxID, ComponentId_CBPASTE);
//
//  return 0; /* pass event on */
//}

static int mouse_click(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Handle button presses in Tools window */
  NOT_USED(event_code);
  MapToolbar *const toolbar = handle;
  const WimpMouseClickEvent *const mouse_click = (WimpMouseClickEvent *)event;

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id, &edit_win),
                   0);

  switch (mouse_click->buttons) {
    case Wimp_MouseButtonSelect:
      if (toolbar->button_selected != id_block->self_component) {
        EditorTool tool = button_to_tool(id_block->self_component);
        if (tool == EDITORTOOL_NONE)
          return 0; /* button not known */

        Editor_select_tool(toolbar->editor, tool);
      }
      break;

    case Wimp_MouseButtonMenu:
      switch (id_block->self_component) {
        case ComponentId_Brush:
          ConfigBrush_show_at_ptr(edit_win);
          break;

        case ComponentId_SmoothWand:
          ConfigWand_show_at_ptr(edit_win);
          break;

        case ComponentId_FloodFill:
          ConfigFill_show_at_ptr(edit_win);
          break;

        case ComponentId_PlotShapes:
          PlotMenu_show_at_ptr(edit_win);
          break;

        case ComponentId_SelectArea: /* Short cut to 'Edit' submenu */
          EditMenu_show_at_ptr(edit_win);
          break;

        case ComponentId_Magnifier: /* Set zoom */
          ZoomMenu_show_at_ptr(edit_win);
          break;

        default:
          return 0; /* piss off, we're not interested */
      }
      break;

    default:
      return 0; /* not interested in this type of click */
  }

  return 1; /* claim event */
}

static bool register_wimp_handlers(MapToolbar *const toolbar)
{
  assert(toolbar != NULL);
  static const struct {
    int event_code;
    WimpEventHandler *handler;
  } wimp_handlers[] = {
    { Wimp_EMouseClick, mouse_click },
    { Wimp_EPointerEnteringWindow, pointer_enter },
    { Wimp_EPointerLeavingWindow, pointer_leave }
  };

  for (size_t i = 0; i < ARRAY_SIZE(wimp_handlers); ++i) {
    if (E(event_register_wimp_handler(toolbar->my_object,
                                      wimp_handlers[i].event_code,
                                      wimp_handlers[i].handler,
                                      toolbar)))
      return false;
  }
  return true;
}

/* ---------------- Public functions ---------------- */

bool MapToolbar_init(MapToolbar *const toolbar, Editor *const editor)
{
  assert(toolbar != NULL);
  assert(editor != NULL);
  DEBUG("Creating MapToolbar for editor %p", (void *)editor);

  *toolbar = (MapToolbar){
    .null_poller = false,
    .editor = editor,
    .mouse_over_button = NULL_ComponentId,
    .button_selected = NULL_ComponentId,
  };

  /* Create tools window */
  if (!E(toolbox_create_object(0, "MapTools", &toolbar->my_object))) {
    DEBUG("MapToolbar object id is 0x%x", toolbar->my_object);

    if (register_wimp_handlers(toolbar)) {
      return true;
    }

    (void)remove_event_handlers_delete(toolbar->my_object);
  }
  return false;
}

void MapToolbar_destroy(MapToolbar *const toolbar)
{
  assert(toolbar != NULL);
  assert(toolbar->editor != NULL);

  if (toolbar->null_poller)
    scheduler_deregister(trackpointer, toolbar);

  E(remove_event_handlers_delete(toolbar->my_object));
}

bool MapToolbar_update_buttons(MapToolbar *const toolbar)
{
  /* Enable / disable buttons as necessary */
  assert(toolbar != NULL);
  for (size_t tool = 0; tool < ARRAY_SIZE(tool_buttons); tool++) {
    if (tool_buttons[tool] != NULL_ComponentId) {
      if (E(set_gadget_faded(toolbar->my_object, tool_buttons[tool],
            !Editor_can_select_tool(toolbar->editor, (EditorTool)tool))))
      {
        return false;
      }
    }
  }
  return true; /* success */
}

void MapToolbar_tool_selected(MapToolbar *const toolbar, EditorTool const tool)
{
  assert(toolbar != NULL);

  ComponentId const new_button = tool_to_button(tool);
  if (new_button == toolbar->button_selected)
  {
    return; /* appropriate button already selected */
  }

  if (toolbar->button_selected != NULL_ComponentId)
  {
    set_button(toolbar->my_object, toolbar->button_selected, false);
  }

  if (new_button != NULL_ComponentId)
  {
    set_button(toolbar->my_object, new_button, true);
  }

  toolbar->button_selected = new_button;
}

void MapToolbar_hide(MapToolbar *const toolbar)
{
  DEBUG("Hiding toolbar");
  assert(toolbar != NULL);
  E(toolbox_hide_object(0, toolbar->my_object));
}

void MapToolbar_reveal(MapToolbar *const toolbar, EditWin *const edit_win)
{
  DEBUG("Bringing toolbar to front");
  assert(toolbar != NULL);
  EditWin_show_toolbar(edit_win, toolbar->my_object);
}
