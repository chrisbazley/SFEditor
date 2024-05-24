/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Fill tool configuration
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

#include <assert.h>

#include "toolbox.h"
#include "menu.h"
#include "event.h"

#include "err.h"
#include "Macros.h"

#include "Session.h"
#include "EditWin.h"
#include "ToolMenu.h"
#include "PlotMenu.h"
#include "utils.h"

/* --------------------- Menu entries -------------------- */

enum {
  PLOTMENU_RECTANGLES = 0x1,
  PLOTMENU_CIRCLES    = 0x2,
  PLOTMENU_TRIANGLES  = 0x3,
  PLOTMENU_LINES      = 0x9,
};

static ComponentId const plot_menu_entries[] = {
  [PLOTSHAPE_LINE] = PLOTMENU_LINES,
  [PLOTSHAPE_CIRCLE] = PLOTMENU_CIRCLES,
  [PLOTSHAPE_TRIANGLE] = PLOTMENU_TRIANGLES,
  [PLOTSHAPE_RECTANGLE] = PLOTMENU_RECTANGLES};

static ObjectId PlotMenu_id = NULL_ObjectId;
static ComponentId selected = NULL_ComponentId;

/* ---------------- Private functions ---------------- */

static PlotShape entrytoshape(ComponentId const entry)
{
  for (size_t plot_shape = 0;
       plot_shape < ARRAY_SIZE(plot_menu_entries);
       plot_shape++) {
    if (plot_menu_entries[plot_shape] == entry)
      return (PlotShape)plot_shape;
  }
  assert(false);
  return PLOTSHAPE_NONE; /* unknown menu entry */
}

static ComponentId shapetoentry(const PlotShape plot_shape)
{
  if (plot_shape >= 0 && (size_t)plot_shape < ARRAY_SIZE(plot_menu_entries)) {
    return plot_menu_entries[plot_shape];
  } else {
    assert(false);
    return NULL_ComponentId; /* unknown plot shape */
  }
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Set up menu */
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);

  if (selected != NULL_ComponentId)
    E(menu_set_tick(0, id_block->self_id, selected, 0));

  selected = shapetoentry(Editor_get_plot_shape(editor));

  if (selected != NULL_ComponentId)
    E(menu_set_tick(0, id_block->self_id, selected, 1));

  return 1; /* claim event */
}

static int menu_selection(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(handle);

  if (id_block->self_component == selected)
    return 1; /* already selected - nothing to do here */

  /* Decode menu click */
  PlotShape plot_shape = entrytoshape(id_block->self_component);
  if (plot_shape == PLOTSHAPE_NONE)
    return 0; /* menu entry not known */

  /* Change plot shape */
  void *edit_win;
  ON_ERR_RPT_RTN_V(toolbox_get_client_handle(0, id_block->ancestor_id,
    &edit_win), 0);
  Editor *const editor = EditWin_get_editor(edit_win);
  Editor_set_plot_shape(editor, plot_shape);

  /* Update position of menu tick */
  if (selected != NULL_ComponentId)
    E(menu_set_tick(0, id_block->self_id, selected, 0));

  E(menu_set_tick(0, id_block->self_id, id_block->self_component, 1));
  selected = id_block->self_component;

  /* Tick corresponding entry on parent menu if part of tree */
  ToolMenu_update(editor);

  return 1; /* claim event */
}

/* ---------------- Public functions ---------------- */

void PlotMenu_created(ObjectId const menu_id)
{
  PlotMenu_id = menu_id;
  selected = NULL_ComponentId;

  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } handlers[] = {
    { Menu_Selection, menu_selection },
    { Menu_AboutToBeShown, about_to_be_shown }
  };

  for (size_t i = 0; i < ARRAY_SIZE(handlers); ++i)
  {
    EF(event_register_toolbox_handler(menu_id, handlers[i].event_code,
                                      handlers[i].handler, NULL));
  }
}

void PlotMenu_show_at_ptr(EditWin const *const edit_win)
{
  EditWin_show_dbox_at_ptr(edit_win, PlotMenu_id);
}
