/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission action triggers
 *  Copyright (C) 2020 Christopher Bazley
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
#include <inttypes.h>

#include "Flex.h"

#include "Debug.h"
#include "Macros.h"
#include "LinkedList.h"
#include "Reader.h"
#include "Writer.h"

#include "Ships.h"
#include "SFError.h"
#include "CoarseCoord.h"
#include "Triggers.h"
#include "TriggersData.h"
#include "Obj.h"
#include "Utils.h"
#include "IntDict.h"

struct Trigger
{
  LinkedListItem link;
  CoarsePoint2d coords;
  TriggerParam param;
};

enum {
  BytesPerTrigger = 4,
  TriggerCoordMultiplier = 2,
  TriggerBitmapSize = Obj_Area/CHAR_BIT * sizeof(char),
};

#ifdef DEBUG_OUTPUT
char const *TriggerAction_to_string(TriggerAction const action)
{
  static char const *const strings[] = {
#define DECLARE_TRIGGER(t) [TriggerAction_ ## t] = #t,
#include "DeclTrig.h"
#undef DECLARE_TRIGGER
  };
  assert(action >= 0);
  assert(action < ARRAY_SIZE(strings));
  return strings[action];
}
#endif

static void update_triggers_map(TriggersData *const triggers,
  Trigger const *const trigger, bool const set)
{
  /* Update bit map of where animations are located */
  assert(triggers != NULL);

  const MapCoord bit_offset = objects_coarse_coords_to_index(trigger->coords);
  const MapCoord byte_offset = bit_offset / CHAR_BIT;
  unsigned int const bit_mask = 1u << (bit_offset % CHAR_BIT);
  DEBUG("%s triggers map byte %" PRIMapCoord ", bits %u", set ? "Setting" : "Clearing",
        byte_offset, bit_mask);

  assert(byte_offset < TriggerBitmapSize);
  if (set) {
    SET_BITS(((char *)triggers->bit_map)[byte_offset], bit_mask);
  } else {
    CLEAR_BITS(((char *)triggers->bit_map)[byte_offset], bit_mask);
  }
}

static bool check_wrapped(TriggersData *const triggers, MapPoint const map_pos)
{
  assert(triggers != NULL);
  const size_t bit_offset = objects_coords_to_index(map_pos);
  const size_t byte_offset = bit_offset / CHAR_BIT;
  unsigned int const bit_mask = 1u << (bit_offset % CHAR_BIT);
  DEBUG("Checking byte %zu, bits %u", byte_offset, bit_mask);

  assert(byte_offset < TriggerBitmapSize);
  if (TEST_BITS(((char *)triggers->bit_map)[byte_offset], bit_mask)) {
    DEBUG("Found a trigger at %" PRIMapCoord ",%" PRIMapCoord,
          map_pos.x, map_pos.y);
    assert(intdict_find(&triggers->all_triggers, objects_coords_to_key(map_pos), NULL));
    return true;
  }

  assert(!intdict_find(&triggers->all_triggers, objects_coords_to_key(map_pos), NULL));
  return false;
}

static void write_trigger(Trigger const *const trigger,
  Writer *const writer)
{
  assert(trigger);
  DEBUGF("Writing trigger %s with parameter %d at coordinates %d,%d\n",
         TriggerAction_to_string(trigger->param.action), trigger->param.value,
         trigger->coords.x, trigger->coords.y);

  CoarsePoint2d const coords = {
    trigger->coords.x * TriggerCoordMultiplier,
    trigger->coords.y * TriggerCoordMultiplier};

  CoarsePoint2d_write(coords, writer);

  assert(trigger->param.action >= TriggerAction_MissionTarget);
  assert(trigger->param.action <= TriggerAction_FixScanners);
  writer_fputc(trigger->param.action, writer);

  writer_fputc(trigger->param.value, writer);
}

SFError triggers_init(TriggersData *const triggers)
{
  assert(triggers);
  *triggers = (TriggersData){0};
  linkedlist_init(&triggers->list);

  if (!flex_alloc(&triggers->bit_map, TriggerBitmapSize)) {
    return SFERROR(NoMem);
  }

  memset_flex(&triggers->bit_map, 0, TriggerBitmapSize);
  intdict_init(&triggers->all_triggers);
  intdict_init(&triggers->chain_triggers);
  return SFERROR(OK);
}

void triggers_destroy(TriggersData *const triggers)
{
  assert(triggers);
  triggers_cleanup(triggers);

  LINKEDLIST_FOR_EACH_SAFE(&triggers->list, item, tmp)
  {
    Trigger *const trigger = CONTAINER_OF(item, Trigger, link);
    free(trigger);
  }

  flex_free(&triggers->bit_map);
  intdict_destroy(&triggers->all_triggers, NULL, NULL);
  intdict_destroy(&triggers->chain_triggers, NULL, NULL);
}

static void insert_trigger(TriggersData *const triggers, Trigger *const prev_trigger, Trigger *const trigger)
{
  assert(triggers);
  assert(triggers->count >= 0);
  assert(triggers->count < TriggersMax);
  DEBUGF("Inserting trigger %s with parameter %d at coordinates %d,%d\n",
         TriggerAction_to_string(trigger->param.action), trigger->param.value,
         trigger->coords.x, trigger->coords.y);
  linkedlist_insert(&triggers->list, prev_trigger ? &prev_trigger->link : NULL, &trigger->link);
  triggers->count++;
}

static void remove_trigger(TriggersData *const triggers, Trigger *const trigger)
{
  assert(triggers);
  assert(triggers->count > 0);
  assert(triggers->count <= TriggersMax);
  if (!trigger) {
    return;
  }
  DEBUGF("Removing trigger %s with parameter %d at coordinates %d,%d\n",
         TriggerAction_to_string(trigger->param.action), trigger->param.value,
         trigger->coords.x, trigger->coords.y);
  triggers->count--;
  linkedlist_remove(&triggers->list, &trigger->link);
}

static SFError add_trigger(TriggersData *const triggers, Trigger *const prev_trigger,
  MapPoint const coords, TriggerParam const param, Trigger **const new_trigger)
{
  assert(triggers);
  assert(param.action >= TriggerAction_MissionTarget);
  assert(param.action <= TriggerAction_FixScanners);

  if (triggers->count == TriggersMax) {
    return SFERROR(NumActions);
  }

  Trigger *const trigger = malloc(sizeof(*trigger));
  if (!trigger) {
    return SFERROR(NoMem);
  }

  *trigger = (Trigger){
    .coords = objects_coords_to_coarse(coords),
    .param = param};

  DEBUGF("Adding trigger %p:%s with parameter %d at coordinates %d,%d\n",
         (void *)trigger, TriggerAction_to_string(trigger->param.action),
         trigger->param.value, trigger->coords.x, trigger->coords.y);

  if (prev_trigger) {
    DEBUGF("Add after trigger %p:%s with parameter %d at coordinates %d,%d\n",
           (void *)prev_trigger, TriggerAction_to_string(prev_trigger->param.action),
           prev_trigger->param.value, prev_trigger->coords.x, prev_trigger->coords.y);
  }

  triggers_cleanup(triggers);

  IntDictKey const all_key = objects_coords_to_key(coords);
  if (!intdict_insert(&triggers->all_triggers, all_key, trigger, NULL)) {
    free(trigger);
    return SFERROR(NoMem);
  }

  insert_trigger(triggers, prev_trigger, trigger);
  update_triggers_map(triggers, trigger, true);

  if (new_trigger) {
    *new_trigger = trigger;
  }

  return SFERROR(OK);
}

static void delete_trigger(TriggersData *const triggers, Trigger *const trigger)
{
  assert(triggers);
  if (!trigger) {
    return;
  }

  MapCoord const all_key = objects_coarse_coords_to_index(trigger->coords);
  if (!intdict_find(&triggers->all_triggers, all_key, NULL)) {
    update_triggers_map(triggers, trigger, false);
  }
  remove_trigger(triggers, trigger);
  free(trigger);
}

static void delete_sorted_trigger(TriggersData *const triggers, Trigger *const trigger)
{
  assert(triggers);
  if (!trigger) {
    return;
  }
  DEBUGF("Removing sorted trigger %s with parameter %d at coordinates %d,%d\n",
         TriggerAction_to_string(trigger->param.action), trigger->param.value,
         trigger->coords.x, trigger->coords.y);

  triggers_cleanup(triggers);

  MapCoord const all_key = objects_coarse_coords_to_index(trigger->coords);
  bool const found = intdict_remove_specific(&triggers->all_triggers, all_key, trigger, NULL);
  NOT_USED(found);
  assert(found);

  delete_trigger(triggers, trigger);
}

static void remove_chain(TriggersData *const triggers, Trigger *const trigger)
{
  assert(triggers);
  assert(trigger);
  assert(trigger->param.action == TriggerAction_ChainReaction);

  /* The set of chain reactions is indexed by a key generated from the coordinates of the
     next object to be destroyed in the chain reaction (which is previous in the list). */
  LinkedListItem *const prev = linkedlist_get_prev(&trigger->link);
  assert(prev);
  Trigger const *const prev_trigger = CONTAINER_OF(prev, Trigger, link);
  MapCoord const chain_key = objects_coarse_coords_to_index(prev_trigger->coords);
  bool const found = intdict_remove_specific(&triggers->chain_triggers, chain_key, trigger, NULL);
  NOT_USED(found);
  assert(found);
}

static SFError add_chain(TriggersData *const triggers,
  MapPoint const coords, TriggerFullParam const fparam)
{
  assert(triggers);
  assert(fparam.param.action == TriggerAction_ChainReaction);

  /* Try to find somewhere to insert a chain trigger so that it doesn't require
     a dummy to be added before it to specify the next object coordinates */
  DEBUGF("Searching for next coordinates %" PRIMapCoord ",%" PRIMapCoord " in chain\n",
         fparam.next_coords.x, fparam.next_coords.y);

  Trigger *prev_trigger = NULL, *next_trigger = NULL;
  IntDictKey const key = objects_coords_to_key(fparam.next_coords);
  IntDictVIter iter;
  for (Trigger *t = intdictviter_init(&iter, &triggers->all_triggers, key, key);
       t != NULL;
       t = intdictviter_advance(&iter)) {
    assert(CoarsePoint2d_compare(t->coords, objects_coords_to_coarse(fparam.next_coords)));
    if (t->param.action == TriggerAction_Dead) {
      continue;
    }

    /* If there is already a chain trigger following the candidate predecessor
       then we can't use it. */
    LinkedListItem *const next = linkedlist_get_next(&t->link);
    next_trigger = next ? CONTAINER_OF(next, Trigger, link) : NULL;
    if (next_trigger && next_trigger->param.action == TriggerAction_ChainReaction) {
      assert(linkedlist_get_prev(next));
      continue;
    }

    prev_trigger = t;
    DEBUGF("Found viable predecessor %p\n", (void *)prev_trigger);
    break;
  }

  /* If there is no suitable predecessor at the coordinates of the next object to
     destroy then add a dummy trigger at the start of the list. */
  Trigger *new_dummy = NULL, *replace = NULL;
  if (!prev_trigger) {
    SFError err = add_trigger(triggers, NULL, fparam.next_coords,
                       (TriggerParam){.action = TriggerAction_Dummy}, &new_dummy);
    if (SFError_fail(err)) {
      return err;
    }
    prev_trigger = new_dummy;
  } else {
    /* It's unlikely but possible that an existing dummy trigger after a suitable predecessor
       happens to specify the coordinates of the chain reaction to be added. */
    if (next_trigger && next_trigger->param.action == TriggerAction_Dummy &&
        CoarsePoint2d_compare(next_trigger->coords, objects_coords_to_coarse(coords))) {
      replace = next_trigger;
    }
  }

  assert(prev_trigger);
  assert(CoarsePoint2d_compare(prev_trigger->coords, objects_coords_to_coarse(fparam.next_coords)));

  IntDictKey const chain_key = objects_coords_to_key(fparam.next_coords);

  if (replace) {
    DEBUGF("Replace dummy trigger %p after one that specifies the next object to destroy\n", (void *)replace);
    assert(CoarsePoint2d_compare(replace->coords, objects_coords_to_coarse(coords)));
    assert(replace->param.action == TriggerAction_Dummy);
    assert(!new_dummy);

    if (!intdict_insert(&triggers->chain_triggers, chain_key, replace, NULL)) {
      return SFERROR(NoMem);
    }
    replace->param = fparam.param;
  } else {
    DEBUGF("Add a trigger after one that specifies the next object to destroy\n");
    assert(!new_dummy || new_dummy == prev_trigger);

    Trigger *new_trigger = NULL;
    SFError err = add_trigger(triggers, prev_trigger, coords, fparam.param, &new_trigger);
    if (!SFError_fail(err)) {
      if (!intdict_insert(&triggers->chain_triggers, chain_key, new_trigger, NULL)) {
        delete_sorted_trigger(triggers, new_trigger);
        err = SFERROR(NoMem);
      }
    }
    if (SFError_fail(err)) {
      delete_sorted_trigger(triggers, new_dummy);
      return err;
    }
  }

  return SFERROR(OK);
}


static SFError add_non_chain(TriggersData *const triggers,
  MapPoint const coords, TriggerFullParam const fparam)
{
  assert(triggers);
  assert(fparam.param.action != TriggerAction_ChainReaction);

  /* Can we replace an existing dummy trigger at the same coordinates? */
  Trigger *replace = NULL;
  IntDictKey const key = objects_coords_to_key(coords);
  IntDictVIter iter;
  for (Trigger *trigger = intdictviter_init(&iter, &triggers->all_triggers, key, key);
       trigger != NULL;
       trigger = intdictviter_advance(&iter)) {
    assert(CoarsePoint2d_compare(trigger->coords, objects_coords_to_coarse(coords)));
    if (trigger->param.action == TriggerAction_Dead) {
      continue;
    }
    if (trigger->param.action == TriggerAction_Dummy) {
      replace = trigger;
      break;
    }
  }

  if (replace) {
    DEBUGF("Replace dummy trigger %p in preference to allocating a new one\n", (void *)replace);
    assert(CoarsePoint2d_compare(replace->coords, objects_coords_to_coarse(coords)));
    assert(replace->param.action == TriggerAction_Dummy);
    replace->param = fparam.param;
  } else {
    DEBUGF("Add a trigger at the start\n");
    Trigger *new_trigger = NULL;
    SFError err = add_trigger(triggers, NULL, coords, fparam.param, &new_trigger);
    if (SFError_fail(err)) {
      return err;
    }
  }

  return SFERROR(OK);
}

SFError triggers_add(TriggersData *const triggers,
  MapPoint const coords, TriggerFullParam const fparam)
{
  assert(triggers);
  assert(fparam.param.action >= TriggerAction_MissionTarget);
  assert(fparam.param.action <= TriggerAction_FixScanners);
  assert(fparam.param.action != TriggerAction_Dummy);

  DEBUGF("Request to add trigger %s with parameter %d at coordinates %" PRIMapCoord ",%" PRIMapCoord "\n",
         TriggerAction_to_string(fparam.param.action), fparam.param.value, coords.x, coords.y);

  if (fparam.param.action == TriggerAction_ChainReaction) {
    return add_chain(triggers, coords, fparam);
  } else {
    return add_non_chain(triggers, coords, fparam);
  }
}

static void defer_delete(TriggersData *const triggers, Trigger *const trigger)
{
  assert(triggers);
  assert(trigger);
  assert(trigger->param.action != TriggerAction_ChainReaction);
  remove_trigger(triggers, trigger);
  linkedlist_insert(&triggers->delete_list, NULL, &trigger->link);
  trigger->param.action = TriggerAction_Dead;
}

static MapPoint iter_loop_core(TriggersIter *const iter, TriggerFullParam *const fparam)
{
  for (; iter->trigger != NULL; iter->trigger = intdictviter_advance(&iter->viter)) {
    Trigger *const trigger = iter->trigger;
    TriggerAction const action = trigger->param.action;
    if (action == TriggerAction_Dummy || action == TriggerAction_Dead) {
      continue;
    }

    if (!objects_bbox_contains(&iter->map_area,  objects_coords_from_coarse(trigger->coords))) {
      continue;
    }

    DEBUGF("Getting trigger %s with parameter %d at coordinates %d,%d\n",
           TriggerAction_to_string(trigger->param.action), trigger->param.value,
           trigger->coords.x, trigger->coords.y);

    if (fparam) {
      MapPoint next_coords = {0};

      if (trigger->param.action == TriggerAction_ChainReaction) {
        LinkedListItem *const prev = linkedlist_get_prev(&trigger->link);
        assert(prev);
        Trigger const *const prev_trigger = CONTAINER_OF(prev, Trigger, link);
        next_coords = objects_coords_from_coarse(prev_trigger->coords);
      }

      *fparam = (TriggerFullParam){.param = trigger->param, .next_coords = next_coords};
    }
    return objects_coords_from_coarse(trigger->coords);
  }

  assert(!iter->done);
  iter->done = true;
  return (MapPoint){-1, -1};
}

MapPoint TriggersIter_get_first(TriggersIter *const iter,
  TriggersData *const triggers, MapArea const *const map_area,
  TriggerFullParam *const fparam)
{
  assert(iter);
  assert(triggers);
  assert(MapArea_is_valid(map_area));

  IntDictKey min_key, max_key;
  objects_area_to_key_range(map_area, &min_key, &max_key);

  *iter = (TriggersIter){
    .map_area = *map_area,
    .triggers = triggers,
  };
  iter->trigger = intdictviter_init(&iter->viter, &triggers->all_triggers, min_key, max_key);
  return iter_loop_core(iter, fparam);
}

MapPoint TriggersIter_get_next(TriggersIter *const iter, TriggerFullParam *const fparam)
{
  assert(iter);
  assert(!iter->done);
  if (iter->trigger != NULL) {
    assert(iter->trigger->param.action != TriggerAction_Dummy);
    assert(iter->trigger->param.action != TriggerAction_Dead);
  }
  assert(MapArea_is_valid(&iter->map_area));
  iter->trigger = intdictviter_advance(&iter->viter);
  return iter_loop_core(iter, fparam);
}

void TriggersIter_del_current(TriggersIter *const iter)
{
  assert(iter);
  assert(!iter->done);
  assert(iter->trigger);
  assert(iter->trigger->param.action != TriggerAction_Dummy);
  assert(iter->trigger->param.action != TriggerAction_Dead);
  assert(iter->triggers);
  assert(MapArea_is_valid(&iter->map_area));

  TriggersData *const triggers = iter->triggers;
  Trigger *const trigger = iter->trigger;
  iter->trigger = NULL;

  DEBUGF("Delete current trigger %p:%s with parameter %d at coordinates %d,%d\n",
         (void *)trigger, TriggerAction_to_string(trigger->param.action),
         trigger->param.value, trigger->coords.x, trigger->coords.y);

  LinkedListItem *prev = linkedlist_get_prev(&trigger->link);
  Trigger *prev_trigger = prev ? CONTAINER_OF(prev, Trigger, link) : NULL;

  if (trigger->param.action == TriggerAction_ChainReaction) {
    DEBUGF("Breaking chain reaction from deleted trigger\n");
    remove_chain(triggers, trigger);

    /* If deleting a chain reaction trigger and the previous trigger is a dummy then it
       serves only to specify the next object to blow up so delete it too. */
    if (prev_trigger && prev_trigger->param.action == TriggerAction_Dummy) {
      defer_delete(triggers, prev_trigger);
      prev = linkedlist_get_prev(&prev_trigger->link);
      prev_trigger = prev ? CONTAINER_OF(prev, Trigger, link) : NULL;
    }
  }

  LinkedListItem *const next = linkedlist_get_next(&trigger->link);
  Trigger *const next_trigger = next ? CONTAINER_OF(next, Trigger, link) : NULL;

  bool can_delete = true;
  if (next_trigger && next_trigger->param.action == TriggerAction_ChainReaction) {
    /* The deleted trigger is followed by a chain reaction, therefore it
       specifies the coordinates of an object to be destroyed. */
    if (!prev_trigger || !CoarsePoint2d_compare(prev_trigger->coords, trigger->coords)) {
      /* Can't delete the trigger without changing the target of a chain
         reaction because the coordinates of the trigger to be deleted differ
         from the previous surviving trigger in the list. */
      can_delete = false;
    }
  }

  if (can_delete) {
    intdictviter_remove(&iter->viter);
    delete_trigger(triggers, trigger);

    /* If deleting any trigger and the next trigger is a dummy with the same
       coordinates as the previous trigger then the next trigger has become redundant. */
    if (next_trigger && next_trigger->param.action == TriggerAction_Dummy &&
        prev_trigger && CoarsePoint2d_compare(prev_trigger->coords, next_trigger->coords))
    {
      defer_delete(triggers, next_trigger);
    }

  } else if (trigger->param.action != TriggerAction_Dummy) {
    DEBUGF("Replacing trigger %p:%s with parameter %d at coordinates %d,%d with dummy\n",
           (void *)trigger, TriggerAction_to_string(trigger->param.action),
           trigger->param.value, trigger->coords.x, trigger->coords.y);

    trigger->param.action = TriggerAction_Dummy;
  }
}

static MapPoint chain_iter_loop_core(TriggersChainIter *const iter,
  TriggerFullParam *const fparam)
{
  for (; iter->trigger != NULL; iter->trigger = intdictviter_advance(&iter->viter)) {
    Trigger *const trigger = iter->trigger;
    assert(trigger->param.action == TriggerAction_ChainReaction);
    LinkedListItem *const prev = linkedlist_get_prev(&trigger->link);
    assert(prev);
    Trigger const *const prev_trigger = CONTAINER_OF(prev, Trigger, link);
    MapPoint const next_coords = objects_coords_from_coarse(prev_trigger->coords);

    if (!objects_bbox_contains(&iter->map_area, next_coords)) {
      continue;
    }

    DEBUGF("Getting trigger %s with parameter %d at coordinates %d,%d chained to %d,%d\n",
           TriggerAction_to_string(trigger->param.action), trigger->param.value,
           trigger->coords.x, trigger->coords.y, prev_trigger->coords.x, prev_trigger->coords.y);

    if (fparam) {
      *fparam = (TriggerFullParam){.param = trigger->param, .next_coords = next_coords};
    }
    return objects_coords_from_coarse(trigger->coords);
  }

  assert(!iter->done);
  iter->done = true;
  return (MapPoint){-1, -1};
}

MapPoint TriggersChainIter_get_first(TriggersChainIter *const iter,
  TriggersData *const triggers, MapArea const *const map_area,
  TriggerFullParam *const fparam)
{
  assert(iter);
  assert(triggers);
  assert(MapArea_is_valid(map_area));

  IntDictKey min_key, max_key;
  objects_area_to_key_range(map_area, &min_key, &max_key);

  *iter = (TriggersChainIter){
    .map_area = *map_area,
    .triggers = triggers,
  };
  iter->trigger = intdictviter_init(&iter->viter, &triggers->chain_triggers, min_key, max_key);
  return chain_iter_loop_core(iter, fparam);
}

MapPoint TriggersChainIter_get_next(TriggersChainIter *const iter, TriggerFullParam *const fparam)
{
  assert(iter);
  assert(!iter->done);
  if (iter->trigger != NULL) {
    assert(iter->trigger->param.action == TriggerAction_ChainReaction);
  }
  assert(iter->triggers);
  assert(MapArea_is_valid(&iter->map_area));
  iter->trigger = intdictviter_advance(&iter->viter);
  return chain_iter_loop_core(iter, fparam);
}

void TriggersChainIter_del_current(TriggersChainIter *const iter)
{
  assert(iter);
  assert(!iter->done);
  assert(iter->trigger);
  assert(iter->trigger->param.action == TriggerAction_ChainReaction);
  assert(iter->triggers);
  assert(MapArea_is_valid(&iter->map_area));

  TriggersData *const triggers = iter->triggers;
  Trigger *const trigger = iter->trigger;
  iter->trigger = NULL;

  DEBUGF("Delete current trigger %p:%s with parameter %d at coordinates %d,%d\n",
         (void *)trigger, TriggerAction_to_string(trigger->param.action),
         trigger->param.value, trigger->coords.x, trigger->coords.y);

  assert(linkedlist_get_prev(&trigger->link));

  LinkedListItem *const next = linkedlist_get_next(&trigger->link);
  Trigger *const next_trigger = next ? CONTAINER_OF(next, Trigger, link) : NULL;
  bool can_delete = true;

  if (next_trigger && next_trigger->param.action == TriggerAction_ChainReaction) {
    /* The chain trigger to be deleted is followed by another chain reaction, therefore
       it too specifies the coordinates of an object to be destroyed. Could attempt to
       optimise by anticipating that the previous trigger will be deleted too, but that
       would be fragile/broken because triggers would be in an invalid state until then. */
    LinkedListItem *const prev = linkedlist_get_prev(&trigger->link);
    Trigger *const prev_trigger = prev ? CONTAINER_OF(prev, Trigger, link) : NULL;

    if (!prev_trigger || !CoarsePoint2d_compare(prev_trigger->coords, trigger->coords)) {
      DEBUGF("Can't delete trigger %p without changing the target %d,%d of a chain reaction, %p.\n",
             (void *)trigger, trigger->coords.x, trigger->coords.y, (void *)next_trigger);
      can_delete = false;
    }
  }

  intdictviter_remove(&iter->viter);

  if (can_delete) {
    delete_sorted_trigger(triggers, trigger);
  } else {
    DEBUGF("Replacing trigger %p:%s with parameter %d at coordinates %d,%d with dummy\n",
           (void *)trigger, TriggerAction_to_string(trigger->param.action),
           trigger->param.value, trigger->coords.x, trigger->coords.y);

    trigger->param.action = TriggerAction_Dummy;
  }
}

bool triggers_check_locn(TriggersData *const triggers, MapPoint const map_pos)
{
  return check_wrapped(triggers, objects_wrap_coords(map_pos));
}

size_t triggers_count_locn(TriggersData *const triggers, MapPoint const map_pos)
{
  size_t count = 0;
  if (triggers_check_locn(triggers, map_pos)) {
    count = triggers_count_bbox(triggers, &(MapArea){map_pos, map_pos});
  }
  return count;
}

size_t triggers_count_bbox(TriggersData *const triggers, MapArea const *const map_area)
{
  assert(triggers);
  DEBUG("Counting triggers from x:%" PRIMapCoord ",%" PRIMapCoord
        " y:%" PRIMapCoord ",%" PRIMapCoord,
        map_area->min.x, map_area->max.x,
        map_area->min.y, map_area->max.y);

  size_t count = 0;
  IntDictKey min_key, max_key;
  objects_area_to_key_range(map_area, &min_key, &max_key);

  IntDictVIter iter;
  for (Trigger *t = intdictviter_init(&iter, &triggers->all_triggers, min_key, max_key);
       t != NULL;
       t = intdictviter_advance(&iter)) {
    if (t->param.action == TriggerAction_Dead) {
      continue;
    }

    if (objects_bbox_contains(map_area, objects_coords_from_coarse(t->coords))) {
      assert(count < TriggersMax);
      count++;
    }
  }
  assert(count <= TriggersMax);
  return count;
}

SFError triggers_read_pad(TriggersData *const triggers,
  Reader *const reader)
{
  long int const start = reader_ftell(reader);
  SFError err = triggers_read(triggers, reader);
  if (SFError_fail(err)) {
    return err;
  }

  /* Can't use triggers->count here because easy mission 17 has a
     redundant trailing dummy trigger which is read but not counted */
  if (reader_fseek(reader, start + (int)sizeof(int32_t) + (TriggersMax * BytesPerTrigger), SEEK_SET))
  {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading triggers data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

SFError triggers_read(TriggersData *const triggers,
  Reader *const reader)
{
  assert(triggers);

  int32_t tmp = 0;
  if (!reader_fread_int32(&tmp, reader))
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Triggers count is %" PRId32 "\n", tmp);

  if (tmp < 0 || tmp > TriggersMax)
  {
    return SFERROR(BadNumTriggers);
  }
  size_t const num_triggers = (size_t)tmp;
  MapPoint prev_coords = {0,0};

  for (size_t j = 0; j < num_triggers; ++j)
  {
    DEBUGF("Reading trigger %zu data at %ld\n", j, reader_ftell(reader));
    CoarsePoint2d coarse_coords = {0};
    if (!CoarsePoint2d_read(&coarse_coords, reader))
    {
      return SFERROR(ReadFail);
    }

    if (coarse_coords.x % TriggerCoordMultiplier ||
        coarse_coords.y % TriggerCoordMultiplier)
    {
      return SFERROR(BadTriggerCoord);
    }

    MapPoint const coords = {
      coarse_coords.x / TriggerCoordMultiplier,
      coarse_coords.y / TriggerCoordMultiplier
    };

    if (!objects_coords_in_range(coords))
    {
      return SFERROR(BadTriggerCoord);
    }

    int const a = reader_fgetc(reader);
    if (a == EOF)
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Trigger action %d\n", a);
    if (a < TriggerAction_MissionTarget ||
        a > TriggerAction_FixScanners)
    {
      return SFERROR(BadTriggerAction);
    }
    TriggerAction const action = a;

    int const value = reader_fgetc(reader);
    if (value == EOF)
    {
      return SFERROR(ReadFail);
    }

    switch (action) {
    case TriggerAction_CrippleShipType:
      if ((value < ShipType_Fighter1) ||
          (value > ShipType_Fighter4 && value < ShipType_Big1) ||
          (value > ShipType_Big3 && value < ShipType_Satellite) ||
          (value > ShipType_Satellite)) {
        return SFERROR(BadTriggerShipType);
      }
      break;
    case TriggerAction_ChainReaction:
      /* Can't chain to the previous trigger's coordinates if none */
      if (j == 0) {
        return SFERROR(BadChainReaction);
      }
      break;
    default:
      break;
    }

    if (action != TriggerAction_Dummy) {
      TriggerFullParam const fparam = {
        .next_coords = prev_coords,
        .param = {
          .action = action,
          .value = value}
        };

      SFError const err = triggers_add(triggers, coords, fparam);
      if (SFError_fail(err)) {
        return err;
      }
    }

    prev_coords = coords;
  }

  return SFERROR(OK);
}

void triggers_write_pad(TriggersData *const triggers, Writer *const writer)
{
  triggers_write(triggers, writer);
  if (writer_ferror(writer)) {
    return;
  }

  long int const padding = TriggersMax - (long)triggers->count;
  writer_fseek(writer, padding * BytesPerTrigger, SEEK_CUR);
  DEBUGF("Finished writing triggers data at %ld\n", writer_ftell(writer));
}

void triggers_write(TriggersData *const triggers,
  Writer *const writer)
{
  assert(triggers);
  assert(triggers->count >= 0);
  assert(triggers->count <= TriggersMax);

  writer_fwrite_int32((int32_t)triggers->count, writer);

  LINKEDLIST_FOR_EACH(&triggers->list, item)
  {
    Trigger *const trigger = CONTAINER_OF(item, Trigger, link);
    write_trigger(trigger, writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }
}

SFError triggers_read_max_losses(TriggersData *const triggers,
  Reader *const reader)
{
  assert(triggers);

  int const max_losses = reader_fgetc(reader);
  if (max_losses == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Max Fednet losses: %d\n", max_losses);
  if (max_losses < 0 || max_losses > TriggersMax)
  {
    return SFERROR(BadMaxLosses);
  }
  triggers->max_losses = (size_t)max_losses;
  return SFERROR(OK);
}

void triggers_write_max_losses(TriggersData *const triggers,
  Writer *const writer)
{
  assert(triggers);
  assert(triggers->max_losses >= 0);
  assert(triggers->max_losses <= TriggersMax);
  writer_fputc((int)triggers->max_losses, writer);
}

size_t triggers_get_max_losses(TriggersData const *const triggers)
{
  assert(triggers);
  return triggers->max_losses;
}

void triggers_set_max_losses(TriggersData *const triggers,
  size_t const max)
{
  assert(triggers);
  assert(max >= 0);
  assert(max <= triggers_get_count(triggers));
  triggers->max_losses = max;
}

size_t triggers_get_count(TriggersData *triggers)
{
  assert(triggers);
  assert(triggers->count >= 0);
  assert(triggers->count <= TriggersMax);
  return triggers->count;
}

void triggers_cleanup(TriggersData *const triggers)
{
  assert(triggers);

  LINKEDLIST_FOR_EACH_SAFE(&triggers->delete_list, item, tmp)
  {
    Trigger *const trigger = CONTAINER_OF(item, Trigger, link);
    assert(trigger->param.action == TriggerAction_Dead);

    DEBUGF("Deferred cleanup of trigger %s with parameter %d at coordinates %d,%d\n",
           TriggerAction_to_string(trigger->param.action), trigger->param.value,
           trigger->coords.x, trigger->coords.y);

    MapCoord const all_key = objects_coarse_coords_to_index(trigger->coords);
    bool const found = intdict_remove_specific(&triggers->all_triggers, all_key, trigger, NULL);
    assert(found);
    NOT_USED(found);

    if (!intdict_find(&triggers->all_triggers, all_key, NULL)) {
      update_triggers_map(triggers, trigger, false);
    }

    linkedlist_remove(&triggers->delete_list, &trigger->link);
    free(trigger);
  }
}
