/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Selected map area properties dialogue box
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
#include <limits.h>
#include <inttypes.h>

#include "event.h"
#include "toolbox.h"
#include "window.h"
#include "gadgets.h"
#include "wimplib.h"

#include "msgtrans.h"
#include "Macros.h"
#include "err.h"
#include "GadgetUtil.h"
#include "WimpExtra.h"
#include "EventExtra.h"
#include "PathTail.h"
#include "IntDict.h"

#include "Desktop.h"
#include "Session.h"
#include "EditWin.h"
#include "mapmode.h"
#include "debug.h"
#include "MPropDbox.h"
#include "utils.h"
#include "plot.h"
#include "SFInit.h"
#include "EditWin.h"
#include "Map.h"
#include "MapEdit.h"
#include "MapAnims.h"
#include "MapTexData.h"
#include "MapEditSel.h"
#include "MapEditChg.h"
#include "MapTexBitm.h"
#include "MapEditCtx.h"

/* --------------------- Gadgets -------------------- */

enum {
  ANIM_DISPLAY_TILE_1  = 0x1,
  ANIM_DISPLAY_TILE_2  = 0x2,
  ANIM_DISPLAY_TILE_3  = 0x3,
  ANIM_DISPLAY_TILE_4  = 0x4,
  ANIM_NUMRANGE_1      = 0x5,
  ANIM_NUMRANGE_2      = 0x6,
  ANIM_NUMRANGE_3      = 0x7,
  ANIM_NUMRANGE_4      = 0x8,
  ANIM_NUMRANGE_PERIOD = 0x9,
  ANIM_BUTTON_SET      = 0xb,
  ANIM_BUTTON_CANCEL   = 0xc,
  ANIM_OPTION_ANIMATE  = 0xd,
  ANIM_OPTION_NONE_1   = 0xf,
  ANIM_OPTION_NONE_2   = 0x10,
  ANIM_OPTION_NONE_3   = 0x11,
  ANIM_OPTION_NONE_4   = 0x12,
  ANIM_ARROW_RIGHT     = 0x17,
  ANIM_ARROW_LEFT      = 0x18,
  ANIM_BUTTON_REVERSE  = 0x19,
  ANIM_LOCATION = 0x21,
  PropsAngle = MapAngle_North,
  PropsMipLevel = 0,
};

/* In 25ths of a second at normal game speed */
#define DEFAULT_PERIOD 12 /* up to UINT16_MAX */

static ComponentId const gadgets_tile_none[] = {ANIM_OPTION_NONE_1, ANIM_OPTION_NONE_2, ANIM_OPTION_NONE_3, ANIM_OPTION_NONE_4};
static ComponentId const gadgets_tile_num[] = {ANIM_NUMRANGE_1, ANIM_NUMRANGE_2, ANIM_NUMRANGE_3, ANIM_NUMRANGE_4};
static ComponentId const gadgets_tile_display[] = {ANIM_DISPLAY_TILE_1, ANIM_DISPLAY_TILE_2, ANIM_DISPLAY_TILE_3, ANIM_DISPLAY_TILE_4};

typedef struct
{
  MapPropDboxes *prop_dboxes;
  ObjectId my_object;
  MapPoint pos;
  MapRef tiles_to_display[AnimsNFrames]; // Can be mask (skip frame)
  MapAnimParam anim;
  bool keep; // don't allow deletion during setting of properties
} MapPropDbox;

/* ---------------- Private functions ---------------- */

static Editor *get_editor(MapPropDbox const *const prop)
{
  assert(prop != NULL);
  assert(prop->prop_dboxes != NULL);
  return prop->prop_dboxes->editor;
}

static EditSession *get_session(MapPropDbox const *const prop)
{
  return Editor_get_session(get_editor(prop));
}

static void delete_dbox(MapPropDbox *const prop)
{
  assert(prop != NULL);
  E(remove_event_handlers_delete(prop->my_object));
  free(prop);
}

static void disp_pos(MapPropDbox const *const prop)
{
  assert(prop);

  char string[24] = "";
  sprintf(string, "%3.3"PRIMapCoord",%3.3"PRIMapCoord, prop->pos.x, prop->pos.y);
  E(displayfield_set_value(0, prop->my_object, ANIM_LOCATION, string));
}

static void draw_sprite(MapPropDbox const *const prop,
  Vertex const orig_scr, WimpPlotIconBlock *const plot_icon,
  size_t const t, size_t const sprite_count)
{
  /* Must be called with flex budge disabled */
  assert(prop != NULL);
  assert(t >= 0);
  assert(t < ARRAY_SIZE(prop->tiles_to_display));

  if (map_ref_to_num(prop->tiles_to_display[t]) >= sprite_count) {
    Vertex const eigen_factors = Desktop_get_eigen_factors();
    Vertex min = {orig_scr.x + plot_icon->bbox.xmin,
                 orig_scr.y + plot_icon->bbox.ymin},
           max = {orig_scr.x + plot_icon->bbox.xmax - (1 << eigen_factors.x),
                 orig_scr.y + plot_icon->bbox.ymax - (1 << eigen_factors.y)};

    if (map_ref_is_mask(prop->tiles_to_display[t])) {
      /* Draw white square */
      plot_set_wimp_col(WimpColour_White);
      plot_fg_rect_2v(min, max);

      /* Put a black cross on it */
      plot_set_wimp_col(WimpColour_Black);
      plot_move(min);
      plot_fg_line(max);

      SWAP(min.y, max.y);
      plot_move(min);
      plot_fg_line(max);
    } else {
      /* No such tile in current graphics set - substitute black rectangle */
      plot_set_wimp_col(WimpColour_Black);
      plot_fg_rect_2v(min, max);
    }
  } else {
    /* Set the tile sprite to appear in the icon */
    assert(plot_icon->data.is.sprite_name_length >= 0);
    sprintf(plot_icon->data.is.sprite,
             "%zu", map_ref_to_num(prop->tiles_to_display[t]));

    E(wimp_plot_icon(plot_icon));
  }
}

static void update_sprite(MapPropDbox const *const prop,
  ObjectId const window, size_t const t)
{
  assert(prop != NULL);
  EditSession *const session = get_session(prop);

  MapTex *const textures = Session_get_textures(session);
  SprMem *const sprites = MapTexBitmaps_get_sprites(&textures->tiles, PropsAngle, PropsMipLevel);
  if (!sprites) {
    return;
  }

  char spr_name[12];
  WimpPlotIconBlock plot_icon = {
    .flags = WimpIcon_Sprite | WimpIcon_HCentred | WimpIcon_VCentred |
             WimpIcon_Indirected,
    .data = {
      .is = {
        .sprite_area = SprMem_get_area_address(sprites),
        .sprite = spr_name,
        .sprite_name_length = sizeof(spr_name)
      }
    }
  };

  assert(t < ARRAY_SIZE(gadgets_tile_display));
  ON_ERR_RPT_RTN(gadget_get_bbox(0, window, gadgets_tile_display[t],
                                 &plot_icon.bbox));

  WimpRedrawWindowBlock block = {
    .visible_area = plot_icon.bbox
  };

  size_t const sprite_count = SprMem_get_sprite_count(sprites);
  int more;
  if (E(window_get_wimp_handle(0, window, &block.window_handle)) ||
      E(wimp_update_window(&block, &more))) {
    more = 0;
  }
  while (more) {
    DEBUG("Redraw rectangle: X %d to %d, Y %d to %d", block.redraw_area.xmin,
          block.redraw_area.xmax, block.redraw_area.ymin,
          block.redraw_area.ymax);

    draw_sprite(prop, (Vertex){block.visible_area.xmin - block.xscroll,
                         block.visible_area.ymax - block.yscroll},
                &plot_icon, t, sprite_count);

    /* Get next redraw rectangle */
    if (E(wimp_get_rectangle(&block, &more)))
      more = 0;
  }
  SprMem_put_area_address(sprites);
}

static void reverse(MapPropDbox *const prop)
{
  assert(prop);

  /* Reverse the order of the displayed animation frames */
  for (size_t frame_index = 0;
       frame_index < ARRAY_SIZE(gadgets_tile_num) / 2;
       frame_index++) {

    size_t first_index = frame_index;
    size_t other_index = ARRAY_SIZE(gadgets_tile_num) - 1 - first_index;
    DEBUG("Swapping frame %zu and frame %zu", other_index, first_index);

    MapRef const tmp = prop->tiles_to_display[first_index];
    prop->tiles_to_display[first_index] = prop->tiles_to_display[other_index];
    prop->tiles_to_display[other_index] = tmp;

    update_sprite(prop, prop->my_object, first_index);
    update_sprite(prop, prop->my_object, other_index);

    int value_A = 0, value_B = 0;
    if (!E(numberrange_get_value(0, prop->my_object,
              gadgets_tile_num[first_index], &value_A)) &&
        !E(numberrange_get_value(0, prop->my_object,
              gadgets_tile_num[other_index], &value_B))) {

      E(numberrange_set_value(0, prop->my_object,
        gadgets_tile_num[other_index], value_A));

      E(numberrange_set_value(0, prop->my_object,
        gadgets_tile_num[first_index], value_B));
    }

    E(optionbutton_set_state(0, prop->my_object,
      gadgets_tile_none[first_index], map_ref_is_mask(prop->tiles_to_display[first_index])));

    E(optionbutton_set_state(0, prop->my_object,
      gadgets_tile_none[other_index], map_ref_is_mask(prop->tiles_to_display[other_index])));

    E(set_gadget_faded(prop->my_object, gadgets_tile_num[first_index],
      map_ref_is_mask(prop->tiles_to_display[first_index])));

    E(set_gadget_faded(prop->my_object,
      gadgets_tile_num[other_index], map_ref_is_mask(prop->tiles_to_display[other_index])));

  } /* next dest_frame */
}

static void fade_set_button(MapPropDbox const *const prop)
{
  assert(prop);
  size_t nm_count = 0;
  for (size_t t = 0; t < ARRAY_SIZE(prop->anim.tiles); t++) {
    if (!map_ref_is_mask(prop->tiles_to_display[t])) {
      ++nm_count;
    }
  }
  E(set_gadget_faded(prop->my_object, ANIM_BUTTON_SET, nm_count == 0));
  E(set_gadget_faded(prop->my_object, ANIM_NUMRANGE_PERIOD, nm_count < 2));
}

static void setup_win(MapPropDbox *const prop)
{
  assert(prop);

  EditSession *const session = get_session(prop);
  MapEditContext const *const map = Session_get_map(session);

  disp_pos(prop);

  for (size_t t = 0; t < ARRAY_SIZE(prop->anim.tiles); t++) {
    E(set_gadget_faded(prop->my_object, gadgets_tile_none[t], map->anims == NULL));

    E(optionbutton_set_state(0, prop->my_object, gadgets_tile_none[t],
      map_ref_is_mask(prop->anim.tiles[t])));

    E(set_gadget_faded(prop->my_object, gadgets_tile_num[t], map_ref_is_mask((prop->anim.tiles[t]))));

    E(numberrange_set_value(0, prop->my_object, gadgets_tile_num[t],
      map_ref_is_mask(prop->anim.tiles[t]) ? 0 : (int)map_ref_to_num(prop->anim.tiles[t])));

    if (map_ref_is_equal(prop->tiles_to_display[t], prop->anim.tiles[t])) {
      continue;
    }

    prop->tiles_to_display[t] = prop->anim.tiles[t];
    DEBUG("Tile to display for frame %zu is %zu", t, map_ref_to_num(prop->tiles_to_display[t]));
    //redraw_gadget(prop->my_object, gadgets_tile_display[t]);
    update_sprite(prop, prop->my_object, t);
  } /* next t */

  static ComponentId const extra_anim_only[] = {
    ANIM_ARROW_RIGHT,
    ANIM_ARROW_LEFT,
    ANIM_BUTTON_REVERSE,
    ANIM_NUMRANGE_PERIOD,
  };

  for (size_t i = 0; i < ARRAY_SIZE(extra_anim_only); ++i) {
    E(set_gadget_faded(prop->my_object, extra_anim_only[i], map->anims == NULL));
  }

  E(numberrange_set_value(0, prop->my_object, ANIM_NUMRANGE_PERIOD, prop->anim.period));
  fade_set_button(prop);
}

static void read_win(MapPropDbox *const prop)
{
  assert(prop);

  for (size_t t = 0; t < ARRAY_SIZE(prop->anim.tiles); t++) {
    int frame_is_sleep = 0;
    if (!E(optionbutton_get_state(0, prop->my_object, gadgets_tile_none[t], &frame_is_sleep)))    {
      if (frame_is_sleep) {
        prop->anim.tiles[t] = map_ref_mask();
      } else {
        int tile_ref = 0;
        E(numberrange_get_value(0, prop->my_object, gadgets_tile_num[t], &tile_ref));
        if (tile_ref < 0) tile_ref = 0;
        prop->anim.tiles[t] = map_ref_from_num((size_t)tile_ref);
      }
    }
  }

  int period = 0;
  if (!E(numberrange_get_value(0, prop->my_object, ANIM_NUMRANGE_PERIOD, &period))) {
    prop->anim.period = period;
  }
}

static int arrows_handler(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Rotate displayed animation frames left (earlier) or right (later)  */
  NOT_USED(event_code);
  MapPropDbox *const prop = handle;
  const AdjusterClickedEvent *const ace = (AdjusterClickedEvent *)event;

  switch (id_block->self_component) {
    case ANIM_ARROW_RIGHT:
      if (ace->direction == Adjuster_Clicked_Down)
        id_block->self_component = ANIM_ARROW_LEFT;
      break;

    case ANIM_ARROW_LEFT:
      if (ace->direction == Adjuster_Clicked_Up)
        id_block->self_component = ANIM_ARROW_RIGHT;
      break;

    default:
      return 0; /* unknown gadget */
  }

  if (id_block->self_component == ANIM_ARROW_RIGHT) {
    /* First save details of first frame to be overwritten in rotate */
    MapRef rotate_carry = prop->tiles_to_display[0];
    int carry_nr_value = 0;

    E(numberrange_get_value(0, id_block->self_id, gadgets_tile_num[0],
      &carry_nr_value));

    for (size_t dest_frame = ARRAY_SIZE(gadgets_tile_num); dest_frame > 0;
         dest_frame--) {
      size_t const wrapped_dest = dest_frame % ARRAY_SIZE(gadgets_tile_num),
                   source_index = dest_frame - 1;

      DEBUG("Copying frame %zu to frame %zu", source_index, wrapped_dest);
      prop->tiles_to_display[wrapped_dest] =
        (source_index == 0 ? rotate_carry : prop->tiles_to_display[source_index]);

      update_sprite(prop, id_block->self_id, wrapped_dest);

      int value = 0;

      if (source_index == 0) {
        value = carry_nr_value;
      } else {
        E(numberrange_get_value(0, id_block->self_id,
          gadgets_tile_num[source_index], &value));
      }

      E(numberrange_set_value(0, id_block->self_id,
        gadgets_tile_num[wrapped_dest], value));

      E(optionbutton_set_state(0, id_block->self_id,
        gadgets_tile_none[wrapped_dest], map_ref_is_mask(prop->tiles_to_display[wrapped_dest])));

      E(set_gadget_faded(id_block->self_id,
        gadgets_tile_num[wrapped_dest], map_ref_is_mask(prop->tiles_to_display[wrapped_dest])));
    } /* next dest_frame */

  } else {
    /* First save details of first frame to be overwritten in rotate */
    MapRef rotate_carry = prop->tiles_to_display[ARRAY_SIZE(gadgets_tile_num) - 1];
    int carry_nr_value = 0;

    E(numberrange_get_value(0, id_block->self_id, gadgets_tile_num[
            ARRAY_SIZE(gadgets_tile_num) - 1], &carry_nr_value));

    for (size_t dest_frame = ARRAY_SIZE(gadgets_tile_num) - 1;
         dest_frame < ARRAY_SIZE(gadgets_tile_num) * 2 - 1;
         dest_frame++) {
      size_t const wrapped_dest = dest_frame % ARRAY_SIZE(gadgets_tile_num),
                   source_index = (dest_frame + 1) % ARRAY_SIZE(
                                  gadgets_tile_num);

      DEBUG("Copying frame %zu to frame %zu", source_index, wrapped_dest);
      prop->tiles_to_display[wrapped_dest] = (source_index == ARRAY_SIZE(
                                       prop->tiles_to_display) - 1 ? rotate_carry :
                                       prop->tiles_to_display[source_index]);

      update_sprite(prop, id_block->self_id, wrapped_dest);

      int value = 0;
      if (source_index == ARRAY_SIZE(gadgets_tile_num) - 1) {
        value = carry_nr_value;
      } else {
        E(numberrange_get_value(0, id_block->self_id,
          gadgets_tile_num[source_index], &value));
      }

      E(numberrange_set_value(0, id_block->self_id,
        gadgets_tile_num[wrapped_dest], value));

      E(optionbutton_set_state(0, id_block->self_id,
        gadgets_tile_none[wrapped_dest], map_ref_is_mask(prop->tiles_to_display[wrapped_dest])));

      E(set_gadget_faded(id_block->self_id,
        gadgets_tile_num[wrapped_dest], map_ref_is_mask(prop->tiles_to_display[wrapped_dest])));
    } /* next dest_frame */
  }
  return 1; /* claim event */
}

static int numberrange_value_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Enable/disable numeric display of tile number and update picture of tile
     when an animation frame is set to 'sleep' or not */
  NOT_USED(event_code);
  MapPropDbox *const prop = handle;
  const NumberRangeValueChangedEvent *const nrvce =
    (NumberRangeValueChangedEvent *)event;

  EditSession *const session = get_session(prop);

  for (size_t frame_index = 0;
       frame_index < ARRAY_SIZE(gadgets_tile_num);
       frame_index++) {

    if (gadgets_tile_num[frame_index] == id_block->self_component) {
      if (nrvce->new_value >= 0 &&
          (size_t)nrvce->new_value < MapTexBitmaps_get_count(&Session_get_textures(session)->tiles)) {

        prop->tiles_to_display[frame_index] = map_ref_from_num((size_t)nrvce->new_value);
        DEBUG("Tile to display for frame %zu is now %zu", frame_index,
              map_ref_to_num(prop->tiles_to_display[frame_index]));

        //redraw_gadget(id_block->self_id, gadgets_tile_display[frame_index]);
        update_sprite(prop, id_block->self_id, frame_index);
      }
      return 1; /* claim event */
    }
   } /* next frame_index */
  return 0; /* unknown gadget */
}

static int optionbutton_state_changed(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Enable/disable numeric display of tile number and update picture of tile
     when an animation frame is set to 'sleep' or not */
  NOT_USED(event_code);
  MapPropDbox *const prop = handle;
  const OptionButtonStateChangedEvent *const obsce =
    (OptionButtonStateChangedEvent *)event;

  for (size_t frame_index = 0;
       frame_index < ARRAY_SIZE(gadgets_tile_none);
       frame_index++) {

    if (gadgets_tile_none[frame_index] != id_block->self_component) {
      continue;
    }

    E(set_gadget_faded(id_block->self_id,
      gadgets_tile_num[frame_index], obsce->new_state));

    if (obsce->new_state) {
      prop->tiles_to_display[frame_index] = map_ref_mask();
    } else {
      int value = 0;

      if (!E(numberrange_get_value(0, id_block->self_id,
                gadgets_tile_num[frame_index], &value)) && value >= 0) {
        prop->tiles_to_display[frame_index] = map_ref_from_num((size_t)value);
      }
    }

    DEBUG("Tile to display for frame %zu is now %zu", frame_index,
          map_ref_to_num(prop->tiles_to_display[frame_index]));

    //redraw_gadget(id_block->self_id, gadgets_tile_display[frame_index]);
    update_sprite(prop, id_block->self_id, frame_index);

  } /* next frame_index */

  fade_set_button(prop);

  return 1; /* claim event */
}

static int actionbutton_selected(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Cancel/OK button has been tweaked on button bar */
  NOT_USED(event_code);
  MapPropDbox *const prop = handle;

  switch (id_block->self_component) {
    case ANIM_BUTTON_REVERSE:
      reverse(prop);
      break;

    case ANIM_BUTTON_CANCEL:
      if (TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
        /* restore settings */
        setup_win(prop);
      }
      break;

    case ANIM_BUTTON_SET:
      /* read settings from window */
      read_win(prop);
      prop->keep = true;
      if (MapMode_set_properties(get_editor(prop), prop->pos, prop->anim))
      {
        if (!TEST_BITS(event->hdr.flags, ActionButton_Selected_Adjust)) {
          E(toolbox_hide_object(0, prop->my_object));
        }
      }
      prop->keep = false;
      break;

    default:
      return 0; /* not interested in this button */
  }
  return 1; /* event handled */
}

static int about_to_be_shown(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  MapPropDbox *const prop = handle;
  EditSession *const session = get_session(prop);

  MapEditContext const *const map = Session_get_map(session);

  if (!map->anims || !MapAnims_get(map->anims, prop->pos, &prop->anim)) {
    /* Use single tile at this location as first frame of animation */
    prop->anim = (MapAnimParam){.period = DEFAULT_PERIOD};

    prop->anim.tiles[0] = MapEdit_read_tile(map, prop->pos);
    for (size_t t = 1; t < ARRAY_SIZE(prop->anim.tiles); t++)
    {
      prop->anim.tiles[t] = map_ref_mask();
    }
  }

  /* Set limits on number range gadgets according to current tile set */
  size_t const upper_bound = MapTexBitmaps_get_count(&Session_get_textures(session)->tiles) - 1;

  for (size_t g = 0; g < ARRAY_SIZE(gadgets_tile_num); g++) {
    assert(upper_bound <= INT_MAX);
    E(numberrange_set_bounds(NumberRange_UpperBound, id_block->self_id,
                             gadgets_tile_num[g], 0, (int)upper_bound, 0, 0));
  }

  setup_win(prop);

  return 1;
}

static int has_been_hidden(int const event_code, ToolboxEvent *const event,
  IdBlock *const id_block, void *const handle)
{
  NOT_USED(event_code);
  NOT_USED(event);
  NOT_USED(id_block);
  MapPropDbox *const prop = handle;
  assert(prop);
  MapPropDbox *const removed = intdict_remove_value(&prop->prop_dboxes->sa, map_coords_to_key(prop->pos), NULL);
  assert(removed == prop);
  NOT_USED(removed);
  delete_dbox(prop);
  return 1;
}

static int redraw_window(int const event_code, WimpPollBlock *const event,
  IdBlock *const id_block, void *const handle)
{
  /* Process redraw events */
  NOT_USED(event_code);
  MapPropDbox *const prop = handle;
  const WimpRedrawWindowRequestEvent *const wrwre =
     (WimpRedrawWindowRequestEvent *)event;

  EditSession *const session = get_session(prop);

  MapTex *const textures = Session_get_textures(session);
  SprMem *const sprites = MapTexBitmaps_get_sprites(&textures->tiles, PropsAngle, PropsMipLevel);
  if (!sprites) {
    return 1;
  }

  char spr_name[12];
  WimpPlotIconBlock plot_icon = {
    .flags = WimpIcon_Sprite | WimpIcon_HCentred | WimpIcon_VCentred |
             WimpIcon_Indirected,
    .data = {
      .is = {
        .sprite_area = SprMem_get_area_address(sprites),
        .sprite = spr_name,
        .sprite_name_length = sizeof(spr_name)
      }
    }
  };

  size_t const sprite_count = SprMem_get_sprite_count(sprites);

  WimpRedrawWindowBlock block = {
    .window_handle = wrwre->window_handle
  };

  Vertex orig_scr = {0,0};
  int more;
  if (E(wimp_redraw_window(&block, &more))) {
    more = 0;
  } else {
    /* Find origin in absolute OS coordinates */
    orig_scr = (Vertex){.x = block.visible_area.xmin - block.xscroll,
                        .y = block.visible_area.ymax - block.yscroll};
  }

  while (more) {
    DEBUG("Redraw rectangle: X %d to %d, Y %d to %d", block.redraw_area.xmin,
          block.redraw_area.xmax, block.redraw_area.ymin,
          block.redraw_area.ymax);

    for (size_t t = 0; t < ARRAY_SIZE(gadgets_tile_display); t++) {
      DEBUG("Tile to display for frame %zu is %zu", t, map_ref_to_num(prop->tiles_to_display[t]));

      if (E(gadget_get_bbox(0, id_block->self_id,
                            gadgets_tile_display[t], &plot_icon.bbox)))
        break;

      DEBUG("Bounding box of gadget %d: X %d to %d, Y %d to %d",
            gadgets_tile_display[t], plot_icon.bbox.xmin, plot_icon.bbox.xmax,
            plot_icon.bbox.ymin, plot_icon.bbox.ymax);

      if (block.redraw_area.xmin >= orig_scr.x + plot_icon.bbox.xmax ||
          block.redraw_area.xmax <= orig_scr.x + plot_icon.bbox.xmin ||
          block.redraw_area.ymin >= orig_scr.y + plot_icon.bbox.ymax ||
          block.redraw_area.ymax <= orig_scr.y + plot_icon.bbox.ymin) {
        DEBUG("No overlap with redraw rectangle");
        continue;
      }

      DEBUG("Redrawing gadget");
      draw_sprite(prop, orig_scr, &plot_icon, t, sprite_count);
    } /* next t */

    /* Get next redraw rectangle */
    if (E(wimp_get_rectangle(&block, &more)))
      more = 0;
  } /* endwhile (more) */

  SprMem_put_area_address(sprites);

  return 1; /* claim event */
}

static bool register_event_handlers(MapPropDbox *const prop)
{
  assert(prop != NULL);
  static const struct {
    int event_code;
    ToolboxEventHandler *handler;
  } tbox_handlers[] = {
    { Window_AboutToBeShown, about_to_be_shown },
    { Window_HasBeenHidden, has_been_hidden },
    { ActionButton_Selected, actionbutton_selected },
    { OptionButton_StateChanged, optionbutton_state_changed },
    { NumberRange_ValueChanged, numberrange_value_changed },
    { Adjuster_Clicked, arrows_handler }
  };

  for (size_t i = 0; i < ARRAY_SIZE(tbox_handlers); ++i) {
    if (E(event_register_toolbox_handler(prop->my_object,
                                      tbox_handlers[i].event_code,
                                      tbox_handlers[i].handler,
                                      prop)))
      return false;
  }

  return !E(event_register_wimp_handler(prop->my_object, Wimp_ERedrawWindow,
                                        redraw_window, prop));
}

static void update_title(MapPropDbox *const prop)
{
  assert(prop != NULL);
  char const *const file_name = Session_get_filename(get_session(prop));
  E(window_set_title(0, prop->my_object,
                     msgs_lookup_subn("MPropTitle", 1, pathtail(file_name, 1))));
}

static MapPropDbox *create_dbox(MapPropDboxes *const prop_dboxes, MapPoint const pos)
{
  assert(prop_dboxes != NULL);
  DEBUGF("Creating properties dbox for %"PRIMapCoord",%"PRIMapCoord"\n", pos.x, pos.y);

  MapPropDbox *const prop = malloc(sizeof(*prop));
  if (!prop) {
    report_error(SFERROR(NoMem), "", "");
    return NULL;
  }

  *prop = (MapPropDbox){
    .prop_dboxes = prop_dboxes,
    .pos = pos,
    .keep = false,
  };

  if (!E(toolbox_create_object(0, "MapProp", &prop->my_object))) {
    DEBUG("MapProp object id is %d", prop->my_object);

    if (register_event_handlers(prop)) {
      if (!intdict_insert(&prop_dboxes->sa, map_coords_to_key(pos), prop, NULL)) {
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

void MapPropDboxes_init(MapPropDboxes *const prop_dboxes, Editor *editor)
{
  assert(prop_dboxes);
  assert(editor);
  *prop_dboxes = (MapPropDboxes){.editor = editor};
  intdict_init(&prop_dboxes->sa);
}

void MapPropDboxes_destroy(MapPropDboxes *const prop_dboxes)
{
  assert(prop_dboxes);
  intdict_destroy(&prop_dboxes->sa, destroy_cb, NULL);
}

void MapPropDboxes_update_title(MapPropDboxes *const prop_dboxes)
{
  assert(prop_dboxes);
  IntDictVIter iter;
  for (MapPropDbox *prop_dbox = intdictviter_all_init(&iter, &prop_dboxes->sa);
       prop_dbox != NULL;
       prop_dbox = intdictviter_advance(&iter)) {
    update_title(prop_dbox);
  }
}

void MapPropDboxes_update_for_move(MapPropDboxes *const prop_dboxes,
                                   MapPoint const old_pos, MapPoint const new_pos)
{
  if (map_coords_compare(old_pos, new_pos)) {
    return;
  }
  MapPropDbox *const prop_dbox = intdict_remove_value(&prop_dboxes->sa,
                                    map_coords_to_key(old_pos), NULL);
  if (!prop_dbox) {
    return;
  }

  assert(map_coords_compare(prop_dbox->pos, old_pos));
  if (intdict_insert(&prop_dboxes->sa, map_coords_to_key(new_pos), prop_dbox, NULL)) {
    prop_dbox->pos = new_pos;
    disp_pos(prop_dbox);
  } else {
    report_error(SFERROR(NoMem), "", "");
    delete_dbox(prop_dbox);
  }
}

static bool split_callback(MapArea const *const bbox, void *const arg)
{
  MapPropDboxes *const prop_dboxes = arg;

  IntDictKey const min_key = map_coords_to_key(bbox->min);
  IntDictKey const max_key = map_coords_to_key(bbox->max);
  assert(min_key <= max_key);

  IntDictVIter iter;
  for (MapPropDbox *prop_dbox = intdictviter_init(&iter, &prop_dboxes->sa, min_key, max_key);
       prop_dbox != NULL;
       prop_dbox = intdictviter_advance(&iter)) {
    if (map_bbox_contains(bbox, prop_dbox->pos) && !prop_dbox->keep) {
      intdictviter_remove(&iter);
      delete_dbox(prop_dbox);
    }
  }
  return false;
}

void MapPropDboxes_update_for_del(MapPropDboxes *const prop_dboxes,
                                  MapArea const *const bbox)
{
  /* Split the map area first otherwise the min and max indices are nonsense */
  map_split_area(bbox, split_callback, prop_dboxes);
}

void MapPropDboxes_open(MapPropDboxes *const prop_dboxes, MapPoint const pos,
                        EditWin *const edit_win)
{
  IntDictKey const key = map_coords_to_key(pos);
  MapPropDbox *prop_dbox = intdict_find_value(&prop_dboxes->sa, key, NULL);
  if (!prop_dbox) {
    prop_dbox = create_dbox(prop_dboxes, pos);
  } else {
    assert(map_coords_to_key(prop_dbox->pos) == key);
  }
  if (prop_dbox) {
    EditWin_show_dbox(edit_win, 0, prop_dbox->my_object);
  }
}
