/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Selection bitmask
 *  Copyright (C) 2022 Christopher Bazley
 */

#ifndef SelBitmask_h
#define SelBitmask_h

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t SelBitmaskType;

typedef struct SelectionBitmask
{
  SelBitmaskType bitmask;
  size_t num_selected, num_objects;
  void (*redraw_cb)(size_t, void *);
  void *redraw_arg;
} SelectionBitmask;

void SelectionBitmask_init(SelectionBitmask *selection, size_t num,
  void (*redraw_cb)(size_t, void *), void *redraw_arg);

static inline void SelectionBitmask_copy(SelectionBitmask *const dst, SelectionBitmask const *const src)
{
  assert(dst != NULL);
  assert(src != NULL);
  dst->bitmask = src->bitmask;
  dst->num_selected = src->num_selected;
  dst->num_objects = src->num_objects;
}

void SelectionBitmask_obj_inserted(SelectionBitmask *selection, size_t index);

void SelectionBitmask_obj_deleted(SelectionBitmask *selection, size_t index);

static inline bool SelectionBitmask_is_selected(
  SelectionBitmask const *const selection, size_t const index)
{
  assert(selection);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
  assert(index < selection->num_objects);
  bool is_sel = selection->bitmask & ((SelBitmaskType)1 << index);
  DEBUGF("%zu %s selected in bitmask %p\n", index, is_sel ? "is" : "isn't", (void *)selection);
  return is_sel;
}

void SelectionBitmask_select(SelectionBitmask *selection, size_t index);
void SelectionBitmask_deselect(SelectionBitmask *selection, size_t index);
void SelectionBitmask_invert(SelectionBitmask *selection, size_t index, bool do_redraw);
void SelectionBitmask_clear(SelectionBitmask *selection);
void SelectionBitmask_select_all(SelectionBitmask *selection);

static inline bool SelectionBitmask_is_none(
  SelectionBitmask const *const selection)
{
  assert(selection != NULL);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
  return selection->num_selected == 0;
}

static inline bool SelectionBitmask_is_all(
  SelectionBitmask const *const selection)
{
  assert(selection != NULL);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
  return selection->num_selected == selection->num_objects;
}

static inline size_t SelectionBitmask_size(
  SelectionBitmask const *const selection)
{
  assert(selection != NULL);
  assert(selection->num_selected <= selection->num_objects);
  assert(selection->num_objects <= sizeof(selection->bitmask) * CHAR_BIT);
#ifndef NDEBUG
  size_t count = 0;
  for (size_t index = 0; index < selection->num_objects; ++index) {
    if (SelectionBitmask_is_selected(selection, index)) {
      count++;
    }
  }
  assert(count == selection->num_selected);
#endif
  return selection->num_selected;
}

bool SelectionBitmask_for_each_changed(SelectionBitmask const *a,
  SelectionBitmask const *b,
  void (*callback)(size_t, void *), void *cb_arg);

bool SelectionBitmask_for_each(SelectionBitmask *selection,
  void (*callback)(size_t, void *), void *cb_arg);

typedef struct
{
  SelectionBitmask *selection;
  size_t remaining, next;
  bool done;
}
SelectionBitmaskIter;

size_t SelectionBitmaskIter_get_first(SelectionBitmaskIter *iter,
  SelectionBitmask *selection);

size_t SelectionBitmaskIter_get_next(SelectionBitmaskIter *iter);

void SelectionBitmaskIter_del_current(SelectionBitmaskIter *iter);
void SelectionBitmaskIter_move_current(SelectionBitmaskIter *iter, size_t index);

static inline bool SelectionBitmaskIter_done(SelectionBitmaskIter *iter)
{
  assert(iter);
  assert(!iter->done || iter->remaining == 0);
  return iter->done;
}

#endif
