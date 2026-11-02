/* SFEditor test: target information editing operations.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../InfoEdit.h"
#include "../InfoEditCtx.h"
#include "../InfoEditChg.h"
#include "../InfosData.h"
#include "../SelBitmask.h"
#include "../Map.h"
#include "../Text.h"

typedef struct
{
  InfoEditContext context;
  union {
    char nil;
    TargetInfosData obj;
  } data;
  SelectionBitmask selected;
  InfoEditChanges changes;
  unsigned added, deleted, moved;
  size_t last_index, last_count;
  int last_id;
  MapPoint last_pos, old_pos;
  unsigned move_calls[TargetInfoMax], delete_calls[TargetInfoMax];
} Fixture;

static Fixture fixture;

typedef enum {
  InfoField_AllMustMatch,
  InfoField_Position,
  InfoField_Type,
  InfoField_Details,
} InfoField;

static void check_fields(TargetInfo const *info, MapPoint pos,
  char const *type, char const *details, InfoField except)
{
  if (except != InfoField_Position)
  {
    MapPoint const actual = target_info_get_pos(info);
    assert(actual.x == pos.x);
    assert(actual.y == pos.y);
  }
  if (except != InfoField_Type)
  {
    assert(strcmp(target_info_get_text(info, TargetInfoTextIndex_Type), type) == 0);
  }
  if (except != InfoField_Details)
  {
    assert(strcmp(target_info_get_text(info, TargetInfoTextIndex_Details), details) == 0);
  }
}

static void check_changes(InfoEditChanges expected)
{
  assert(fixture.changes.infos_added == expected.infos_added);
  assert(fixture.changes.infos_changed == expected.infos_changed);
  assert(fixture.changes.infos_deleted == expected.infos_deleted);
}

static TargetInfo *get(size_t index)
{
  _Optional TargetInfo *const info = InfoEdit_get(&fixture.context, index);
  assert(info);
  return &*info;
}

static size_t index_of(TargetInfo const *info)
{
  for (size_t index = 0; index < InfoEdit_count(&fixture.context); ++index)
  {
    if (get(index) == info)
    {
      return index;
    }
  }
  assert(!"Information point not found");
  return 0;
}

/* The context passes session through unchanged; only its identity is used. */
static void added(TargetInfo const *info, size_t index, struct EditSession *session)
{
  assert(session == fixture.context.session);
  assert(InfoEdit_get(&fixture.context, index) == info);
  fixture.last_index = index;
  fixture.last_count = InfoEdit_count(&fixture.context);
  fixture.last_id = target_info_get_id(info);
  fixture.last_pos = target_info_get_pos(info);
  ++fixture.added;
  SelectionBitmask_obj_inserted(&fixture.selected, index);
}

static void predelete(TargetInfo const *info, size_t index, struct EditSession *session)
{
  assert(session == fixture.context.session);
  assert(InfoEdit_get(&fixture.context, index) == info);
  int const id = target_info_get_id(info);
  assert(id >= 0);
  assert(id < TargetInfoMax);
  ++fixture.delete_calls[id];
  fixture.last_index = index;
  fixture.last_count = InfoEdit_count(&fixture.context);
  fixture.last_id = id;
  fixture.last_pos = target_info_get_pos(info);
  ++fixture.deleted;
  SelectionBitmask_obj_deleted(&fixture.selected, index);
}

static void moved(TargetInfo const *info, MapPoint old_pos, size_t old_index,
  size_t new_index, struct EditSession *session)
{
  assert(session == fixture.context.session);
  assert(InfoEdit_get(&fixture.context, new_index) == info);
  int const id = target_info_get_id(info);
  assert(id >= 0);
  assert(id < TargetInfoMax);
  ++fixture.move_calls[id];
  fixture.last_index = new_index;
  fixture.last_id = id;
  fixture.old_pos = old_pos;
  fixture.last_pos = target_info_get_pos(info);
  ++fixture.moved;

  bool const was_selected = SelectionBitmask_is_selected(&fixture.selected, old_index);
  SelectionBitmask_obj_deleted(&fixture.selected, old_index);
  SelectionBitmask_obj_inserted(&fixture.selected, new_index);
  if (was_selected)
  {
    SelectionBitmask_select(&fixture.selected, new_index);
  }
}

static void init(void)
{
  fixture = (Fixture){
    .context = {
      .data = &fixture.data.obj, .added_cb = added, .predelete_cb = predelete,
      .moved_cb = moved, .session = (struct EditSession *)&fixture,
    },
    .selected = {.redraw_arg = &fixture},
  };
  target_infos_init(&fixture.data.obj);
  SelectionBitmask_init(&fixture.selected, 0, NULL, &fixture);
}

static void destroy(void)
{
  target_infos_destroy(&fixture.data.obj);
}

static TargetInfo *seed(MapPoint pos)
{
  char const *strings[TargetInfoTextIndex_Count] = {"OLD TYPE", "OLD DETAILS"};
  size_t index;
  assert(!SFError_fail(InfoEdit_add(&fixture.context, pos, &strings, NULL, &index)));
  fixture.added = 0;
  return get(index);
}

static void make_invalid(char (*string)[Text_NumColumns + 1])
{
  for (size_t i = 0; i < sizeof(*string) - 1; ++i)
  {
    (*string)[i] = 'x';
  }
  (*string)[sizeof(*string) - 1] = '\0';
}

static MapArea whole_map(void)
{
  return (MapArea){.min = {0, 0}, .max = {Map_Size - 1, Map_Size - 1}};
}

static void test1(void)
{
  /* Initial information count */

  init();

  assert(InfoEdit_count(&fixture.context) == 0);

  destroy();
}

static void test2(void)
{
  /* Add information with text */

  init();
  char const *strings[TargetInfoTextIndex_Count] = {"RADAR", "DESTROY THIS TARGET"};
  size_t index = TargetInfoMax;

  assert(!SFError_fail(InfoEdit_add(&fixture.context, (MapPoint){5, 7}, &strings, &fixture.changes, &index)));

  assert(index == 0);
  assert(InfoEdit_count(&fixture.context) == 1);
  check_fields(get(index), (MapPoint){5, 7}, strings[0], strings[1], InfoField_AllMustMatch);
  check_changes((InfoEditChanges){.infos_added = 1});
  destroy();
}

static void test3(void)
{
  /* Add information without text */

  init();

  assert(!SFError_fail(InfoEdit_add(&fixture.context, (MapPoint){5, 7}, NULL, &fixture.changes, NULL)));

  check_fields(get(0), (MapPoint){5, 7}, "", "", InfoField_AllMustMatch);
  check_changes((InfoEditChanges){.infos_added = 1});
  destroy();
}

static void test4(void)
{
  /* Added notification observes completed insertion */

  init();
  char const *strings[TargetInfoTextIndex_Count] = {"RADAR", "DETAILS"};
  size_t index;

  assert(!SFError_fail(InfoEdit_add(&fixture.context, (MapPoint){5, 7}, &strings, &fixture.changes, &index)));

  assert(fixture.added == 1);
  assert(fixture.last_index == index);
  assert(fixture.last_count == 1);
  assert(fixture.last_id == target_info_get_id(get(index)));
  assert(fixture.last_pos.x == 5);
  assert(fixture.last_pos.y == 7);
  check_fields(get(index), (MapPoint){5, 7}, strings[0], strings[1], InfoField_AllMustMatch);
  destroy();
}

static void test5(void)
{
  /* Information insertion returns sorted index */

  init();
  TargetInfo *const later = seed((MapPoint){20, 10});
  size_t index = TargetInfoMax;

  assert(!SFError_fail(InfoEdit_add(&fixture.context, (MapPoint){5, 10}, NULL, &fixture.changes, &index)));

  assert(index == 0);
  assert(get(1) == later);
  check_fields(later, (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  check_changes((InfoEditChanges){.infos_added = 1});
  destroy();
}

static void test6(void)
{
  /* Information coordinates wrap */

  init();

  assert(!SFError_fail(InfoEdit_add(&fixture.context, (MapPoint){-1, Map_Size + 1}, NULL, &fixture.changes, NULL)));

  check_fields(get(0), (MapPoint){Map_Size - 1, 1}, "", "", InfoField_AllMustMatch);
  destroy();
}

static void test7(void)
{
  /* Full information set rejects insertion */

  init();
  MapCoord x = 0;
  for (size_t i = 0; i < TargetInfoMax; ++i)
  {
    seed((MapPoint){x, 10});
    x += 10;
  }
  size_t index = TargetInfoMax;

  SFError const err = InfoEdit_add(&fixture.context, (MapPoint){5, 7}, NULL, &fixture.changes, &index);

  assert(err.type == SFErrorType_NumInfos);
  assert(index == TargetInfoMax);
  assert(InfoEdit_count(&fixture.context) == TargetInfoMax);
  assert(fixture.added == 0);
  check_changes((InfoEditChanges){0});
  destroy();
}

static void test8(void)
{
  /* Invalid type rolls back insertion */

  init();
  TargetInfo *const existing = seed((MapPoint){20, 10});
  char invalid[Text_NumColumns + 1];
  make_invalid(&invalid);
  char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", "NEW DETAILS"};
  strings[0] = invalid;
  size_t index = TargetInfoMax;

  SFError const err = InfoEdit_add(&fixture.context, (MapPoint){5, 7}, &strings, &fixture.changes, &index);

  assert(err.type == SFErrorType_TooManyBriefingColumns);
  assert(index == TargetInfoMax);
  assert(InfoEdit_count(&fixture.context) == 1);
  assert(get(0) == existing);
  check_fields(existing, (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.added == 0);
  check_changes((InfoEditChanges){0});
  TargetInfo *const retry = seed((MapPoint){5, 7});
  assert(target_info_get_id(retry) == target_info_get_id(existing) + 1);
  destroy();
}

static void test9(void)
{
  /* Invalid details roll back insertion */

  init();
  TargetInfo *const existing = seed((MapPoint){20, 10});
  char invalid[Text_NumColumns + 1];
  make_invalid(&invalid);
  char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", "NEW DETAILS"};
  strings[1] = invalid;
  size_t index = TargetInfoMax;

  SFError const err = InfoEdit_add(&fixture.context, (MapPoint){5, 7}, &strings, &fixture.changes, &index);

  assert(err.type == SFErrorType_TooManyBriefingColumns);
  assert(index == TargetInfoMax);
  assert(InfoEdit_count(&fixture.context) == 1);
  assert(get(0) == existing);
  check_fields(existing, (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.added == 0);
  check_changes((InfoEditChanges){0});
  TargetInfo *const retry = seed((MapPoint){5, 7});
  assert(target_info_get_id(retry) == target_info_get_id(existing) + 1);
  destroy();
}

static void test10(void)
{
  /* Replace information text */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});
  char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", "NEW DETAILS"};

  assert(!SFError_fail(InfoEdit_set_texts(info, &strings, &fixture.changes)));

  check_fields(info, (MapPoint){5, 7}, strings[0], strings[1], InfoField_AllMustMatch);
  check_changes((InfoEditChanges){.infos_changed = 1});
  destroy();
}

static void test11(void)
{
  /* Invalid first text leaves information unchanged */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});
  char invalid[Text_NumColumns + 1];
  make_invalid(&invalid);
  char const *strings[TargetInfoTextIndex_Count] = {invalid, "NEW DETAILS"};

  SFError const err = InfoEdit_set_texts(info, &strings, &fixture.changes);

  assert(err.type == SFErrorType_TooManyBriefingColumns);
  check_fields(info, (MapPoint){5, 7}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  check_changes((InfoEditChanges){0});
  destroy();
}

static void test12(void)
{
  /* Invalid second text leaves information unchanged */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});
  char invalid[Text_NumColumns + 1];
  make_invalid(&invalid);
  char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", invalid};

  SFError const err = InfoEdit_set_texts(info, &strings, &fixture.changes);

  assert(err.type == SFErrorType_TooManyBriefingColumns);
  check_fields(info, (MapPoint){5, 7}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  check_changes((InfoEditChanges){0});
  destroy();
}

static void test13(void)
{
  /* Move selection past later information */

  init();
  TargetInfo *const infos[] = {
    seed((MapPoint){5, 10}),
    seed((MapPoint){20, 10}),
    seed((MapPoint){40, 10}),
    seed((MapPoint){60, 10})
  };
  SelectionBitmask_select(&fixture.selected, index_of(infos[0]));
  SelectionBitmask_select(&fixture.selected, index_of(infos[2]));

  InfoEdit_move(&fixture.context, (MapPoint){30, 0}, &fixture.selected, &fixture.changes);

  check_fields(infos[0], (MapPoint){35, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[0])] == 1);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[0])) == true);
  check_fields(infos[1], (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[1])] == 0);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[1])) == false);
  check_fields(infos[2], (MapPoint){70, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[2])] == 1);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[2])) == true);
  check_fields(infos[3], (MapPoint){60, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[3])] == 0);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[3])) == false);
  assert(fixture.moved == 2);
  assert(SelectionBitmask_size(&fixture.selected) == 2);
  check_changes((InfoEditChanges){.infos_changed = 2});
  destroy();
}

static void test14(void)
{
  /* Move selection past earlier information */

  init();
  TargetInfo *const infos[] = {
    seed((MapPoint){20, 10}),
    seed((MapPoint){40, 10}),
    seed((MapPoint){60, 10}),
    seed((MapPoint){80, 10})
  };
  SelectionBitmask_select(&fixture.selected, index_of(infos[1]));
  SelectionBitmask_select(&fixture.selected, index_of(infos[3]));

  InfoEdit_move(&fixture.context, (MapPoint){-30, 0}, &fixture.selected, &fixture.changes);

  check_fields(infos[0], (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[0])] == 0);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[0])) == false);
  check_fields(infos[1], (MapPoint){10, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[1])] == 1);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[1])) == true);
  check_fields(infos[2], (MapPoint){60, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[2])] == 0);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[2])) == false);
  check_fields(infos[3], (MapPoint){50, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(infos[3])] == 1);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(infos[3])) == true);
  assert(fixture.moved == 2);
  assert(SelectionBitmask_size(&fixture.selected) == 2);
  check_changes((InfoEditChanges){.infos_changed = 2});
  destroy();
}

static void test15(void)
{
  /* Move notification supplies previous position */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});
  SelectionBitmask_select(&fixture.selected, index_of(info));

  InfoEdit_move(&fixture.context, (MapPoint){3, -2}, &fixture.selected, &fixture.changes);

  assert(fixture.old_pos.x == 5);
  assert(fixture.old_pos.y == 7);
  assert(fixture.last_pos.x == 8);
  assert(fixture.last_pos.y == 5);
  assert(fixture.last_index == index_of(info));
  destroy();
}

static void test16(void)
{
  /* Moving information wraps coordinates */

  init();
  TargetInfo *const info = seed((MapPoint){1, 1});
  TargetInfo *const other = seed((MapPoint){10, 10});
  SelectionBitmask_select(&fixture.selected, index_of(info));

  InfoEdit_move(&fixture.context, (MapPoint){-2, -2}, &fixture.selected, &fixture.changes);

  check_fields(info, (MapPoint){Map_Size - 1, Map_Size - 1}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  check_fields(other, (MapPoint){10, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.move_calls[target_info_get_id(info)] == 1);
  check_changes((InfoEditChanges){.infos_changed = 1});
  destroy();
}

static void test17(void)
{
  /* Empty selection leaves information unchanged */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});

  InfoEdit_move(&fixture.context, (MapPoint){3, 4}, &fixture.selected, &fixture.changes);
  InfoEdit_delete(&fixture.context, &fixture.selected, &fixture.changes);

  check_fields(info, (MapPoint){5, 7}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.moved == 0);
  assert(fixture.deleted == 0);
  check_changes((InfoEditChanges){0});
  destroy();
}

static void test18(void)
{
  /* Delete selected information */

  init();
  TargetInfo *const first = seed((MapPoint){5, 10});
  TargetInfo *const keep = seed((MapPoint){20, 10});
  TargetInfo *const last = seed((MapPoint){40, 10});
  int const first_id = target_info_get_id(first), last_id = target_info_get_id(last);
  SelectionBitmask_select(&fixture.selected, index_of(first));
  SelectionBitmask_select(&fixture.selected, index_of(last));

  InfoEdit_delete(&fixture.context, &fixture.selected, &fixture.changes);

  assert(InfoEdit_count(&fixture.context) == 1);
  assert(get(0) == keep);
  check_fields(keep, (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
  assert(fixture.delete_calls[first_id] == 1);
  assert(fixture.delete_calls[last_id] == 1);
  assert(SelectionBitmask_size(&fixture.selected) == 0);
  check_changes((InfoEditChanges){.infos_deleted = 2});
  destroy();
}

static void test19(void)
{
  /* Predelete notification observes existing information */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});
  int const id = target_info_get_id(info);
  SelectionBitmask_select(&fixture.selected, index_of(info));

  InfoEdit_delete(&fixture.context, &fixture.selected, &fixture.changes);

  assert(fixture.deleted == 1);
  assert(fixture.last_count == 1);
  assert(fixture.last_id == id);
  assert(fixture.last_pos.x == 5);
  assert(fixture.last_pos.y == 7);
  assert(InfoEdit_count(&fixture.context) == 0);
  destroy();
}

static void test20(void)
{
  /* Delete all information */

  init();
  MapCoord x = 0;
  for (size_t i = 0; i < TargetInfoMax; ++i)
  {
    seed((MapPoint){x, 10});
    x += 10;
  }
  SelectionBitmask_select_all(&fixture.selected);

  InfoEdit_delete(&fixture.context, &fixture.selected, &fixture.changes);

  assert(InfoEdit_count(&fixture.context) == 0);
  assert(fixture.deleted == TargetInfoMax);
  check_changes((InfoEditChanges){.infos_deleted = TargetInfoMax});
  destroy();
}

static void test21(void)
{
  /* Information edits without observers */

  init();
  TargetInfo *const info = seed((MapPoint){5, 7});
  SelectionBitmask_select(&fixture.selected, index_of(info));
  fixture.context = (InfoEditContext){.data = &fixture.data.obj, .session = fixture.context.session};
  char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", "NEW DETAILS"};

  assert(!SFError_fail(InfoEdit_set_texts(info, &strings, NULL)));
  InfoEdit_move(&fixture.context, (MapPoint){3, 4}, &fixture.selected, NULL);

  check_fields(info, (MapPoint){8, 11}, strings[0], strings[1], InfoField_AllMustMatch);
  InfoEdit_delete(&fixture.context, &fixture.selected, NULL);
  assert(InfoEdit_count(&fixture.context) == 0);
  destroy();
}

static void test22(void)
{
  /* Information area iteration */

  init();
  seed((MapPoint){4, 10});
  TargetInfo *const first = seed((MapPoint){5, 10});
  TargetInfo *const second = seed((MapPoint){20, 20});
  seed((MapPoint){21, 20});
  MapArea const area = {.min = {5, 10}, .max = {20, 20}};
  InfoEditIter iter;

  size_t index = InfoEdit_get_first_idx(&iter, &fixture.context, &area);

  assert(!InfoEditIter_done(&iter));
  assert(get(index) == first);
  index = InfoEditIter_get_next(&iter);
  assert(!InfoEditIter_done(&iter));
  assert(get(index) == second);
  (void)InfoEditIter_get_next(&iter);
  assert(InfoEditIter_done(&iter));
  destroy();
}

static void test23(void)
{
  /* Wrapped information area iteration */

  init();
  seed((MapPoint){255, 255});
  seed((MapPoint){0, 0});
  seed((MapPoint){1, 1});
  seed((MapPoint){2, 2});
  MapArea const area = {.min = {-2, -2}, .max = {1, 1}};
  InfoEditIter iter;
  size_t count = 0;

  for (size_t index = InfoEdit_get_first_idx(&iter, &fixture.context, &area);
       !InfoEditIter_done(&iter); index = InfoEditIter_get_next(&iter))
  {
    MapPoint const pos = target_info_get_pos(get(index));
    assert(map_bbox_contains(&area, pos));
    ++count;
    assert(count <= TargetInfoMax);
  }

  assert(count == 3);
  destroy();
}

static void test24(void)
{
  /* Delete through information iterator */

  init();
  seed((MapPoint){5, 10});
  seed((MapPoint){20, 20});
  seed((MapPoint){40, 40});
  MapArea const area = whole_map();
  InfoEditIter iter;
  size_t count = 0;

  for (InfoEdit_get_first_idx(&iter, &fixture.context, &area);
       !InfoEditIter_done(&iter); InfoEditIter_get_next(&iter))
  {
    InfoEditIter_del_current(&iter);
    ++count;
    assert(count <= TargetInfoMax);
  }

  assert(count == 3);
  assert(InfoEdit_count(&fixture.context) == 0);
  destroy();
}

static void test25(void)
{
  /* Information queries without data */

  init();
  fixture.context.data = NULL;
  MapArea const area = whole_map();
  InfoEditIter iter;

  assert(InfoEdit_count(&fixture.context) == 0);
  (void)InfoEdit_get_first_idx(&iter, &fixture.context, &area);
  assert(InfoEdit_get(&fixture.context, 0) == NULL);
  assert(InfoEditIter_done(&iter));
  InfoEdit_find_occluded(&fixture.context, (MapPoint){5, 7}, &fixture.selected);
  assert(SelectionBitmask_size(&fixture.selected) == 0);
  destroy();
}

static void test26(void)
{
  /* Information edits without data */

  init();
  fixture.context.data = NULL;
  size_t index = TargetInfoMax;

  assert(!SFError_fail(InfoEdit_add(&fixture.context, (MapPoint){5, 7}, NULL, &fixture.changes, &index)));
  InfoEdit_move(&fixture.context, (MapPoint){3, 4}, &fixture.selected, &fixture.changes);
  InfoEdit_delete(&fixture.context, &fixture.selected, &fixture.changes);

  assert(index == TargetInfoMax);
  assert(fixture.added == 0);
  assert(fixture.moved == 0);
  assert(fixture.deleted == 0);
  check_changes((InfoEditChanges){0});
  destroy();
}

static void test27(void)
{
  /* Information occlusion boundary */

  init();
  TargetInfo *const centre = seed((MapPoint){64, 64});
  TargetInfo *const touching_x = seed((MapPoint){64 + 2 * InfoMaxClickDist, 64});
  TargetInfo *const outside_x = seed((MapPoint){65 + 2 * InfoMaxClickDist, 64});
  TargetInfo *const touching_y = seed((MapPoint){64, 64 + 2 * InfoMaxClickDist});
  TargetInfo *const outside_y = seed((MapPoint){64, 65 + 2 * InfoMaxClickDist});

  InfoEdit_find_occluded(&fixture.context, (MapPoint){64, 64}, &fixture.selected);

  assert(SelectionBitmask_size(&fixture.selected) == 3);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(centre)));
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(touching_x)));
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(touching_y)));
  assert(!SelectionBitmask_is_selected(&fixture.selected, index_of(outside_x)));
  assert(!SelectionBitmask_is_selected(&fixture.selected, index_of(outside_y)));
  check_changes((InfoEditChanges){0});
  destroy();
}

static void test28(void)
{
  /* Information occlusion wraps map edge */

  init();
  TargetInfo *const near_x = seed((MapPoint){255, 0});
  TargetInfo *const near_y = seed((MapPoint){0, 255});
  TargetInfo *const far = seed((MapPoint){2 * InfoMaxClickDist + 1, 0});

  InfoEdit_find_occluded(&fixture.context, (MapPoint){0, 0}, &fixture.selected);

  assert(SelectionBitmask_size(&fixture.selected) == 2);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(near_x)));
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(near_y)));
  assert(!SelectionBitmask_is_selected(&fixture.selected, index_of(far)));
  destroy();
}

static void test29(void)
{
  /* Occlusion preserves existing selection */

  init();
  TargetInfo *const near = seed((MapPoint){5, 7});
  TargetInfo *const far = seed((MapPoint){100, 100});
  SelectionBitmask_select(&fixture.selected, index_of(far));

  InfoEdit_find_occluded(&fixture.context, (MapPoint){5, 7}, &fixture.selected);

  assert(SelectionBitmask_size(&fixture.selected) == 2);
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(near)));
  assert(SelectionBitmask_is_selected(&fixture.selected, index_of(far)));
  destroy();
}

#ifdef FORTIFY
static void test30(void)
{
  /* Text allocation failures preserve information */

  bool complete = false;
  for (unsigned long limit = 0; limit < 8; ++limit)
  {
    init();
    TargetInfo *const info = seed((MapPoint){5, 7});
    int const id = target_info_get_id(info);
    char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", "NEW DETAILS"};
    Fortify_SetNumAllocationsLimit(limit);

    SFError const err = InfoEdit_set_texts(info, &strings, &fixture.changes);

    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      check_fields(info, (MapPoint){5, 7}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
      check_changes((InfoEditChanges){0});
    }
    else
    {
      check_fields(info, (MapPoint){5, 7}, strings[0], strings[1], InfoField_AllMustMatch);
      check_changes((InfoEditChanges){.infos_changed = 1});
      complete = true;
    }
    assert(InfoEdit_count(&fixture.context) == 1);
    assert(get(0) == info);
    assert(target_info_get_id(info) == id);
    destroy();
    if (complete)
    {
      break;
    }
  }
  assert(complete);
}

#endif

#ifdef FORTIFY
static void test31(void)
{
  /* Information allocation failures roll back insertion */

  bool complete = false;
  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    init();
    TargetInfo *const existing = seed((MapPoint){20, 10});
    char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", "NEW DETAILS"};
    size_t index = TargetInfoMax;
    Fortify_SetNumAllocationsLimit(limit);

    SFError const err = InfoEdit_add(&fixture.context, (MapPoint){5, 7}, &strings, &fixture.changes, &index);

    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      assert(index == TargetInfoMax);
      assert(InfoEdit_count(&fixture.context) == 1);
      assert(fixture.added == 0);
      check_changes((InfoEditChanges){0});
    }
    else
    {
      assert(InfoEdit_count(&fixture.context) == 2);
      assert(fixture.added == 1);
      check_fields(get(index), (MapPoint){5, 7}, strings[0], strings[1], InfoField_AllMustMatch);
      check_changes((InfoEditChanges){.infos_added = 1});
      complete = true;
    }
    check_fields(existing, (MapPoint){20, 10}, "OLD TYPE", "OLD DETAILS", InfoField_AllMustMatch);
    if (!complete)
    {
      TargetInfo *const retry = seed((MapPoint){5, 7});
      assert(target_info_get_id(retry) == target_info_get_id(existing) + 1);
    }
    destroy();
    if (complete)
    {
      break;
    }
  }
  assert(complete);
}

#endif

void InfoEdit_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial information count", test1 },
    { "Add information with text", test2 },
    { "Add information without text", test3 },
    { "Added notification observes completed insertion", test4 },
    { "Information insertion returns sorted index", test5 },
    { "Information coordinates wrap", test6 },
    { "Full information set rejects insertion", test7 },
    { "Invalid type rolls back insertion", test8 },
    { "Invalid details roll back insertion", test9 },
    { "Replace information text", test10 },
    { "Invalid first text leaves information unchanged", test11 },
    { "Invalid second text leaves information unchanged", test12 },
    { "Move selection past later information", test13 },
    { "Move selection past earlier information", test14 },
    { "Move notification supplies previous position", test15 },
    { "Moving information wraps coordinates", test16 },
    { "Empty selection leaves information unchanged", test17 },
    { "Delete selected information", test18 },
    { "Predelete notification observes existing information", test19 },
    { "Delete all information", test20 },
    { "Information edits without observers", test21 },
    { "Information area iteration", test22 },
    { "Wrapped information area iteration", test23 },
    { "Delete through information iterator", test24 },
    { "Information queries without data", test25 },
    { "Information edits without data", test26 },
    { "Information occlusion boundary", test27 },
    { "Information occlusion wraps map edge", test28 },
    { "Occlusion preserves existing selection", test29 },
#ifdef FORTIFY
    { "Text allocation failures preserve information", test30 },
#endif
#ifdef FORTIFY
    { "Information allocation failures roll back insertion", test31 },
#endif
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n", 1 + count,
           ARRAY_SIZE(unit_tests), unit_tests[count].test_name);
    fflush(stdout);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
