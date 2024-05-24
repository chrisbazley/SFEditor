/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Selection bitmask
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

#include <limits.h>

#include "Debug.h"
#include "Macros.h"
#include "SelBitmask.h"

static void redraw(SelectionBitmask *const selection, size_t const index)
{
  assert(selection);
  if (selection->redraw_cb) {
    DEBUGF("Redraw selection changed index %zu\n", index);
    selection->redraw_cb(index, selection->redraw_arg);
  } else {
    DEBUGF("No handler to redraw selection changed index %zu\n", index);
  }
}

void SelectionBitmask_init(SelectionBitmask *const selection, size_t const num,
  void (*redraw_cb)(size_t, void *), void *redraw_arg)
{
  assert(selection);
  assert(num <= sizeof(selection->bitmask) * CHAR_BIT);

  *selection = (SelectionBitmask){
    .bitmask = 0,
    .num_selected = 0,
    .num_objects = num,
    .redraw_cb = redraw_cb,
    .redraw_arg = redraw_arg
  };
}

void SelectionBitmask_obj_inserted(SelectionBitmask *const selection,
  size_t const index)
{
  assert(selection);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects < sizeof(selection->bitmask) * CHAR_BIT);
  assert(index <= selection->num_objects);

  SelBitmaskType const low_mask = ((SelBitmaskType)1 << index) - 1;
  SelBitmaskType const high_mask = ~low_mask;

  selection->bitmask = (selection->bitmask & low_mask) |
                       ((selection->bitmask & high_mask) << 1);
  ++selection->num_objects;
  DEBUGF("Inserted at %zu in bitmask; now %zu objects\n", index, selection->num_objects);
}

void SelectionBitmask_obj_deleted(SelectionBitmask *const selection,
  size_t const index)
{
  assert(selection);
  DEBUGF("Delete at %zu in bitmask %p; %zu/%zu selected\n", index, (void *)selection,
         selection->num_selected, selection->num_objects);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
  assert(selection->num_objects > 0);
  assert(index < selection->num_objects);

  bool const is_selected = SelectionBitmask_is_selected(selection, index);
  SelBitmaskType const low_mask = ((SelBitmaskType)1 << index) - 1;
  SelBitmaskType const high_mask = ~low_mask & ~((SelBitmaskType)1 << index);

  selection->bitmask = (selection->bitmask & low_mask) |
                       ((selection->bitmask & high_mask) >> 1);
  --selection->num_objects;
  if (is_selected) {
    assert(selection->num_selected > 0);
    --selection->num_selected;
  }
  DEBUGF("Deleted at %zu in bitmask; now %zu/%zu selected\n", index, selection->num_selected, selection->num_objects);
}

void SelectionBitmask_select(SelectionBitmask *const selection, size_t const index)
{
  if (SelectionBitmask_is_selected(selection, index)) {
    return;
  }
  selection->bitmask |= ((SelBitmaskType)1 << index);
  selection->num_selected++;
  DEBUGF("Select at %zu in bitmask; now %zu/%zu selected\n", index, selection->num_selected, selection->num_objects);
  redraw(selection, index);
}

void SelectionBitmask_deselect(SelectionBitmask *const selection, size_t const index)
{
  if (!SelectionBitmask_is_selected(selection, index)) {
    return;
  }
  selection->bitmask &= ~((SelBitmaskType)1 << index);
  selection->num_selected--;
  DEBUGF("Deselect at %zu in bitmask; now %zu/%zu selected\n", index, selection->num_selected, selection->num_objects);
  redraw(selection, index);
}

void SelectionBitmask_invert(SelectionBitmask *const selection, size_t const index, bool const do_redraw)
{
  bool const is_selected = SelectionBitmask_is_selected(selection, index);
  selection->bitmask ^= ((SelBitmaskType)1 << index);
  if (is_selected) {
    selection->num_selected--;
  } else {
    selection->num_selected++;
  }
  DEBUGF("Invert at %zu in bitmask; now %zu/%zu selected\n", index, selection->num_selected, selection->num_objects);
  if (do_redraw) {
    redraw(selection, index);
  }
}

void SelectionBitmask_clear(SelectionBitmask *const selection)
{
  assert(selection);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
  for (size_t index = 0; index < selection->num_objects; ++index) {
    SelectionBitmask_deselect(selection, index);
  }
  assert(selection->bitmask == 0);
  assert(selection->num_selected == 0);
}

void SelectionBitmask_select_all(SelectionBitmask *const selection)
{
  assert(selection);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
  for (size_t index = 0; index < selection->num_objects; ++index) {
    SelectionBitmask_select(selection, index);
  }
  assert(selection->bitmask == ((SelBitmaskType)1 << selection->num_objects) - 1);
  assert(selection->num_selected == selection->num_objects);
}

bool SelectionBitmask_for_each_changed(SelectionBitmask const *const a,
  SelectionBitmask const *const b,
  void (*const callback)(size_t, void *), void *cb_arg)
{
  assert(callback);
  assert(a);
  assert(b);
  assert(a->num_objects == b->num_objects);

  DEBUG("Iterate over changes between selection %p and %p",
        (void *)a, (void *)b);

  if (SelectionBitmask_is_none(a) && SelectionBitmask_is_none(b)) {
    return false;
  }

  if (SelectionBitmask_is_all(a) && SelectionBitmask_is_all(b)) {
    return false;
  }

  bool changed = false;
  size_t const num_objects = a->num_objects;
  for (size_t index = 0; index < num_objects; ++index) {
    if (SelectionBitmask_is_selected(a, index) == SelectionBitmask_is_selected(b, index)) {
      continue;
    }

    DEBUGF("Selection state changed at %zu\n", index);
    callback(index, cb_arg);
    changed = true;
  }
  return changed;
}

bool SelectionBitmask_for_each(SelectionBitmask *const selection,
  void (*const callback)(size_t, void *), void *const cb_arg)
{
  DEBUG("Iterate over selection %p", (void *)selection);

  if (SelectionBitmask_is_none(selection)) {
    return false;
  }

  SelectionBitmaskIter iter;
  bool changed = false;
  for (size_t index = SelectionBitmaskIter_get_first(&iter, selection);
       !SelectionBitmaskIter_done(&iter);
       index = SelectionBitmaskIter_get_next(&iter))
  {
    DEBUGF("Selected info at %zu\n", index);
    callback(index, cb_arg);
    changed = true;
  }
  return changed;
}

size_t SelectionBitmaskIter_get_first(SelectionBitmaskIter *const iter,
  SelectionBitmask *const selection)
{
  assert(iter != NULL);
  assert(selection != NULL);
  DEBUGF("Start iteration over %zu/%zu items in bitmask %p\n",
         selection->num_selected, selection->num_objects, (void *)selection);

  *iter = (SelectionBitmaskIter){
    .next = 0,
    .remaining = SelectionBitmask_size(selection),
    .selection = selection,
  };

  return SelectionBitmaskIter_get_next(iter);
}

size_t SelectionBitmaskIter_get_next(SelectionBitmaskIter *const iter)
{
  assert(iter != NULL);
  assert(!SelectionBitmaskIter_done(iter));

  SelectionBitmask *const selection = iter->selection;

  DEBUGF("%zu selected items yet to find in bitmask %p\n", iter->remaining, (void *)selection);
  if (iter->remaining > 0) {
    size_t const num_objects = selection->num_objects;
    for (size_t index = iter->next; index < num_objects; ++index) {
      if (SelectionBitmask_is_selected(selection, index)) {
        --iter->remaining; // consume one
        iter->next = index + 1;
        assert(!SelectionBitmaskIter_done(iter));
        DEBUGF("Next selected in bitmask %p is at %zu (%zu yet to find)\n",
               (void *)selection, index, iter->remaining);
        return index;
      }
    }
    assert(!"Fewer infos selected than at start");
  }
  iter->done = true;
  assert(SelectionBitmaskIter_done(iter));
  return 0;
}

void SelectionBitmaskIter_del_current(SelectionBitmaskIter *const iter)
{
  assert(iter != NULL);
  assert(iter->next > 0);
  assert(!SelectionBitmaskIter_done(iter));
  SelectionBitmask *const selection = iter->selection;
  // Already consumed the current item but need to rewind next item index
  SelectionBitmask_obj_deleted(selection, --iter->next);
}

void SelectionBitmaskIter_move_current(SelectionBitmaskIter *const iter, size_t const index)
{
  assert(iter != NULL);
  assert(iter->next > 0);
  assert(!SelectionBitmaskIter_done(iter));
  SelectionBitmask *const selection = iter->selection;
  DEBUGF("Move current to %zu in bitmask %p\n",
         index, (void *)selection);

  // Already consumed the current item
  SelectionBitmask_obj_deleted(selection, iter->next - 1);
  // Need to rewind next item index unless replacing at or below it
  if (index >= iter->next) {
    --iter->next;
  }
  SelectionBitmask_obj_inserted(selection, index);
}
