/* SFEditor test: map editing operations.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "HostSupport.h"
#include "../MapEdit.h"
#include "../MapEditCtx.h"
#include "../MapEditSel.h"
#include "../Map.h"
#include "../DFile.h"
#include "../Smooth.h"
#include "../SmoothData.h"

typedef struct
{
  MapEditContext map;
  MapData *base, *overlay;
  ConvAnimations *anims;
  MapEditChanges changes;
  MapPoint probe;
  unsigned before_count, redraw_count, reads;
  MapArea before_area, redraw_area;
  MapRef before_value, after_value;
  unsigned char transfer[2][3];
} Fixture;

static Fixture fixture;

/* The core passes session through unchanged; only its identity is used. */
static void before(MapArea const *area, struct EditSession *session)
{
  assert(session == fixture.map.session);
  fixture.before_area = *area;
  fixture.before_value = MapEdit_read_tile(&fixture.map, fixture.probe);
  ++fixture.before_count;
}

static void redraw(MapArea const *area, struct EditSession *session)
{
  assert(session == fixture.map.session);
  fixture.redraw_area = *area;
  fixture.after_value = MapEdit_read_tile(&fixture.map, fixture.probe);
  ++fixture.redraw_count;
}

static void selection_redraw(MapArea const *area, void *arg)
{
  (void)area;
  assert(arg == &fixture);
}

static MapData *new_map(bool overlay)
{
  _Optional MapData *const data = overlay ? map_create_overlay() : map_create_base();
  assert(data);
  return &*data;
}

static void init(void)
{
  _Optional ConvAnimations *const anims = MapAnims_create();
  assert(anims);
  MapData *const base = new_map(false), *const overlay = new_map(true);
  fixture = (Fixture){
    .map = {
      .base = base, .overlay = overlay, .anims = &*anims,
      .prechange_cb = before, .redraw_cb = redraw, .session = (struct EditSession *)&fixture,
    },
    .base = base, .overlay = overlay, .anims = &*anims,
    .probe = {5, 7}, .transfer = {{10, 11, 12}, {13, 14, 15}},
  };

  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      map_set_tile(fixture.base, (MapPoint){x, y}, map_ref_from_num(3));
      map_set_tile(fixture.overlay, (MapPoint){x, y}, map_ref_mask());
    }
  }
}

static void destroy(void)
{
  dfile_release(MapAnims_get_dfile(fixture.anims));
  dfile_release(map_get_dfile(fixture.overlay));
  dfile_release(map_get_dfile(fixture.base));
}

static void check_rect(MapData const *data, MapArea area,
  unsigned char inside, unsigned char outside)
{
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      MapPoint const pos = {x, y};
      bool const contains = MapArea_is_valid(&area) && MapArea_contains(&area, pos);
      unsigned char const expected = contains ? inside : outside;
      assert(map_ref_to_num(map_get_tile(data, pos)) == expected);
    }
  }
}

static void check_changes(MapEditChanges expected)
{
  assert(fixture.changes.tiles_changed == expected.tiles_changed);
  assert(fixture.changes.anims_added == expected.anims_added);
  assert(fixture.changes.anims_changed == expected.anims_changed);
  assert(fixture.changes.anims_deleted == expected.anims_deleted);
}

static void check_area(MapArea actual, MapArea expected)
{
  assert(actual.min.x == expected.min.x);
  assert(actual.min.y == expected.min.y);
  assert(actual.max.x == expected.max.x);
  assert(actual.max.y == expected.max.y);
}

static MapAnimParam param(void)
{
  return (MapAnimParam){.period = 2,
    .tiles = {map_ref_from_num(4), map_ref_from_num(5),
              map_ref_from_num(6), map_ref_from_num(7)}};
}

static void check_param(MapPoint pos, MapAnimParam expected)
{
  MapAnimParam actual;
  assert(MapAnims_get(fixture.anims, pos, &actual));
  assert(actual.period == expected.period);
  for (size_t i = 0; i < AnimsNFrames; ++i)
  {
    assert(map_ref_to_num(actual.tiles[i]) == map_ref_to_num(expected.tiles[i]));
  }
}

static MapRef read_transfer(void *arg, MapPoint pos)
{
  assert(arg == &fixture);
  assert(pos.x >= 0);
  assert(pos.x < 3);
  assert(pos.y >= 0);
  assert(pos.y < 2);
  ++fixture.reads;
  return map_ref_from_num(fixture.transfer[pos.y][pos.x]);
}

static void test1(void)
{
  /* Composited map reads */

  init();
  map_set_tile(fixture.overlay, fixture.probe, map_ref_from_num(9));

  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, fixture.probe)) == 9);
  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, (MapPoint){6, 7})) == 3);
  assert(map_ref_to_num(MapEdit_read_overlay(&fixture.map, (MapPoint){6, 7})) == Map_RefMask);
  destroy();
}

static void test2(void)
{
  /* Map reads without layers */

  init();
  fixture.map = (MapEditContext){.session = (struct EditSession *)&fixture};

  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, fixture.probe)) == Map_RefMask);
  assert(map_ref_to_num(MapEdit_read_overlay(&fixture.map, fixture.probe)) == Map_RefMask);
  destroy();
}

static void test3(void)
{
  /* Map write targets overlay */

  init();

  MapEdit_write_tile(&fixture.map, fixture.probe, map_ref_from_num(9), &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {5, 7}}, 9, Map_RefMask);
  check_rect(fixture.base, MapArea_make_invalid(), 0, 3);
  check_changes((MapEditChanges){.tiles_changed = 1});
  destroy();
}

static void test4(void)
{
  /* Map write targets base without overlay */

  init();
  fixture.map.overlay = NULL;

  MapEdit_write_tile(&fixture.map, fixture.probe, map_ref_from_num(9), &fixture.changes);

  check_rect(fixture.base, (MapArea){.min = {5, 7}, .max = {5, 7}}, 9, 3);
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 1});
  destroy();
}

static void test5(void)
{
  /* Unchanged map write avoids redraw */

  init();
  map_set_tile(fixture.overlay, fixture.probe, map_ref_from_num(9));

  MapEdit_write_tile(&fixture.map, fixture.probe, map_ref_from_num(9), &fixture.changes);

  assert(fixture.redraw_count == 0);
  check_changes((MapEditChanges){0});
  destroy();
}

static void test6(void)
{
  /* Map write notifications surround mutation */

  init();

  MapEdit_write_tile(&fixture.map, fixture.probe, map_ref_from_num(9), &fixture.changes);

  assert(fixture.before_count == 1);
  assert(fixture.redraw_count == 1);
  assert(map_ref_to_num(fixture.before_value) == 3);
  assert(map_ref_to_num(fixture.after_value) == 9);
  check_area(fixture.before_area, (MapArea){.min = {5, 7}, .max = {5, 7}});
  check_area(fixture.redraw_area, fixture.before_area);
  destroy();
}

static void test7(void)
{
  /* Map coordinates wrap */

  init();

  MapEdit_write_tile(&fixture.map, (MapPoint){-1, Map_Size + 1}, map_ref_from_num(9), &fixture.changes);

  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, (MapPoint){-1, Map_Size + 1})) == 9);
  check_rect(fixture.overlay, (MapArea){.min = {Map_Size - 1, 1}, .max = {Map_Size - 1, 1}}, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 1});
  destroy();
}

static void test8(void)
{
  /* Map editing without observers */

  init();
  fixture.map = (MapEditContext){.base = fixture.base, .session = (struct EditSession *)&fixture};

  MapEdit_write_tile(&fixture.map, fixture.probe, map_ref_from_num(9), NULL);

  check_rect(fixture.base, (MapArea){.min = {5, 7}, .max = {5, 7}}, 9, 3);
  destroy();
}

static void test9(void)
{
  /* Fill map area */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 8}};

  MapEdit_fill_area(&fixture.map, &area, map_ref_from_num(9), &fixture.changes);

  check_rect(fixture.overlay, area, 9, Map_RefMask);
  check_rect(fixture.base, MapArea_make_invalid(), 0, 3);
  check_changes((MapEditChanges){.tiles_changed = 9});
  check_area(fixture.redraw_area, area);
  destroy();
}

static void test10(void)
{
  /* Copy map transfer uses relative coordinates */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 7}};

  MapEdit_copy_to_area(&fixture.map, &area, read_transfer, &fixture, &fixture.changes);

  assert(fixture.reads == 6);
  check_changes((MapEditChanges){.tiles_changed = 6});
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      MapPoint const pos = {x, y};
      unsigned char const expected = MapArea_contains(&area, pos) ?
        fixture.transfer[y - area.min.y][x - area.min.x] : Map_RefMask;
      assert(map_ref_to_num(map_get_tile(fixture.overlay, pos)) == expected);
    }
  }
  check_rect(fixture.base, MapArea_make_invalid(), 0, 3);
  destroy();
}

static void test11(void)
{
  /* Fill selected map cells */

  init();
  MapEditSelection selected;
  assert(!SFError_fail(MapEditSelection_init(&selected, selection_redraw, &fixture)));
  MapEditSelection_select(&selected, (MapPoint){5, 7});
  MapEditSelection_select(&selected, (MapPoint){6, 7});

  MapEdit_fill_selection(&fixture.map, &selected, map_ref_from_num(9), &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {6, 7}}, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 2});
  MapEditSelection_destroy(&selected);
  destroy();
}

static void test12(void)
{
  /* Map rectangle plotting */

  init();

  MapEdit_plot_rect(&fixture.map, (MapPoint){6, 8}, (MapPoint){4, 6}, map_ref_from_num(9), &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {4, 6}, .max = {6, 8}}, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 9});
  destroy();
}

static void test13(void)
{
  /* Map line plotting */

  init();

  MapEdit_plot_line(&fixture.map, (MapPoint){4, 7}, (MapPoint){6, 7}, map_ref_from_num(9), 0, &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {4, 7}, .max = {6, 7}}, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 3});
  destroy();
}

static void test14(void)
{
  /* Map unit circle */

  init();

  MapEdit_plot_circ(&fixture.map, fixture.probe, 1, map_ref_from_num(9), &fixture.changes);

  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      bool const painted = ((x == 5) && (y >= 6) && (y <= 8)) ||
                           ((y == 7) && (x >= 4) && (x <= 6));
      unsigned char const expected = painted ? 9 : Map_RefMask;
      assert(map_ref_to_num(map_get_tile(fixture.overlay, (MapPoint){x, y})) == expected);
    }
  }
  check_rect(fixture.base, MapArea_make_invalid(), 0, 3);
  check_changes((MapEditChanges){.tiles_changed = 5});
  destroy();
}

static void test15(void)
{
  /* Map unit right triangle */

  init();

  MapEdit_plot_tri(&fixture.map, (MapPoint){5, 7}, (MapPoint){6, 7}, (MapPoint){5, 8}, map_ref_from_num(9), &fixture.changes);

  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      bool const painted = ((y == 7) && (x >= 5) && (x <= 6)) ||
                           ((x == 5) && (y == 8));
      unsigned char const expected = painted ? 9 : Map_RefMask;
      assert(map_ref_to_num(map_get_tile(fixture.overlay, (MapPoint){x, y})) == expected);
    }
  }
  check_rect(fixture.base, MapArea_make_invalid(), 0, 3);
  check_changes((MapEditChanges){.tiles_changed = 3});
  destroy();
}

static void test16(void)
{
  /* Map flood fill stays in connected region */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 8}};
  for (MapCoord y = 6; y <= 8; ++y)
  {
    for (MapCoord x = 4; x <= 6; ++x)
    {
      map_set_tile(fixture.base, (MapPoint){x, y}, map_ref_from_num(4));
    }
  }
  map_set_tile(fixture.base, (MapPoint){10, 10}, map_ref_from_num(4));

  MapEdit_flood_fill(&fixture.map, map_ref_from_num(9), fixture.probe, &fixture.changes);

  check_rect(fixture.overlay, area, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 9});
  assert(map_ref_to_num(map_get_tile(fixture.base, (MapPoint){10, 10})) == 4);
  destroy();
}

static void test17(void)
{
  /* Map global replacement */

  init();
  map_set_tile(fixture.base, fixture.probe, map_ref_from_num(4));

  MapEdit_global_replace(&fixture.map, map_ref_from_num(4), map_ref_from_num(9), &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {5, 7}}, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 1});
  destroy();
}

static void test18(void)
{
  /* Map overlay cropping preserves animated cells */

  init();
  map_set_tile(fixture.overlay, fixture.probe, map_ref_from_num(3));
  map_set_tile(fixture.overlay, (MapPoint){6, 7}, map_ref_from_num(3));
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, (MapPoint){6, 7}, param())));

  MapEdit_crop_overlay(&fixture.map, &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {6, 7}, .max = {6, 7}}, 3, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 1});
  destroy();
}

static void test19(void)
{
  /* Map range checks include hidden base cells */

  init();
  map_set_tile(fixture.base, fixture.probe, map_ref_from_num(4));
  map_set_tile(fixture.overlay, fixture.probe, map_ref_from_num(1));

  assert(!MapEdit_check_tile_range(&fixture.map, 4));
  assert(MapEdit_check_tile_range(&fixture.map, 5));
  destroy();
}

static void test20(void)
{
  /* Map range checks ignore overlay masks */

  init();

  assert(MapEdit_check_tile_range(&fixture.map, 4));

  map_set_tile(fixture.overlay, fixture.probe, map_ref_from_num(4));
  assert(!MapEdit_check_tile_range(&fixture.map, 4));
  destroy();
}

static void test21(void)
{
  /* Map write removes animation */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));

  MapEdit_write_tile(&fixture.map, fixture.probe, map_ref_from_num(9), &fixture.changes);

  assert(MapEdit_count_anims(&fixture.map) == 0);
  check_changes((MapEditChanges){.tiles_changed = 1, .anims_deleted = 1});
  destroy();
}

static void test22(void)
{
  /* Write map animation */

  init();

  assert(MapEdit_write_anim(&fixture.map, fixture.probe, param(), &fixture.changes));

  assert(MapEdit_count_anims(&fixture.map) == 1);
  check_param(fixture.probe, param());
  check_changes((MapEditChanges){.anims_added = 1});
  destroy();
}

static void test23(void)
{
  /* Replace map animation */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  MapAnimParam replacement = param();
  replacement.period = 17;

  assert(MapEdit_write_anim(&fixture.map, fixture.probe, replacement, &fixture.changes));

  check_param(fixture.probe, replacement);
  check_changes((MapEditChanges){.anims_added = 1, .anims_deleted = 1});
  destroy();
}

static void test24(void)
{
  /* Reverse selected animation */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, (MapPoint){6, 7}, param())));
  MapEditSelection selected;
  assert(!SFError_fail(MapEditSelection_init(&selected, selection_redraw, &fixture)));
  MapEditSelection_select(&selected, fixture.probe);

  MapEdit_reverse_selected(&fixture.map, &selected, &fixture.changes);

  MapAnimParam const expected = {.period = 2,
    .tiles = {map_ref_from_num(7), map_ref_from_num(6), map_ref_from_num(5), map_ref_from_num(4)}};
  check_param(fixture.probe, expected);
  check_param((MapPoint){6, 7}, param());
  check_changes((MapEditChanges){.anims_changed = 1});
  MapEditSelection_destroy(&selected);
  destroy();
}

static void test25(void)
{
  /* Delete selected animation */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, (MapPoint){6, 7}, param())));
  MapEditSelection selected;
  assert(!SFError_fail(MapEditSelection_init(&selected, selection_redraw, &fixture)));
  MapEditSelection_select(&selected, fixture.probe);

  MapEdit_delete_selected(&fixture.map, &selected, &fixture.changes);

  assert(!MapAnims_check_locn(fixture.anims, fixture.probe));
  check_param((MapPoint){6, 7}, param());
  check_changes((MapEditChanges){.anims_deleted = 1});
  MapEditSelection_destroy(&selected);
  destroy();
}

static void test26(void)
{
  /* Animation frame replacement */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));

  MapEdit_global_replace(&fixture.map, map_ref_from_num(4), map_ref_from_num(9), &fixture.changes);

  MapAnimParam expected = param();
  expected.tiles[0] = map_ref_from_num(9);
  check_param(fixture.probe, expected);
  check_changes((MapEditChanges){.anims_changed = 1});
  destroy();
}

static void test27(void)
{
  /* Animation playback updates map */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));

  (void)MapEdit_update_anims(&fixture.map, 3, NULL);

  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, fixture.probe)) == 5);
  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {5, 7}}, 5, Map_RefMask);
  destroy();
}

static void test28(void)
{
  /* Animation reset restores first frame */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  (void)MapEdit_update_anims(&fixture.map, 3, NULL);

  MapEdit_reset_anims(&fixture.map);
  MapEdit_anims_to_map(&fixture.map, &fixture.changes);

  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, fixture.probe)) == 4);
  check_changes((MapEditChanges){.tiles_changed = 1});
  destroy();
}

static void test29(void)
{
  /* Map editing without animation data */

  init();
  fixture.map.anims = NULL;

  assert(MapEdit_count_anims(&fixture.map) == INT_MAX);
  assert(MapEdit_update_anims(&fixture.map, 1, NULL) == SchedulerTime_Max);
  assert(MapEdit_write_anim(&fixture.map, fixture.probe, param(), &fixture.changes));
  MapEdit_reset_anims(&fixture.map);
  MapEdit_anims_to_map(&fixture.map, &fixture.changes);
  check_changes((MapEditChanges){0});
  destroy();
}

static void test30(void)
{
  /* Smooth selected map cells */

  init();
  static MapPoint const neighbours[] = {{5, 8}, {6, 7}, {5, 6}, {4, 7}};
  for (size_t i = 0; i < ARRAY_SIZE(neighbours); ++i)
  {
    map_set_tile(fixture.base, neighbours[i], map_ref_from_num(4));
  }
  _Optional FILE *const file = tmpfile();
  assert(file);
  assert(fputs("StartGroup 0\n3:0,0,0,0,0\n9:0,1,1,1,1\nEndGroup\n"
    "StartGroup 1\n4:0,1,1,1,1\nEndGroup\n", &*file) >= 0);
  rewind(&*file);
  MapTexGroups groups;
  MapTexGroups_init(&groups);
  char error[MapTexGroupsErrBufferSize];
  assert(!SFError_fail(MapTexGroups_read(&*file, &groups, 10, &error)));
  assert(fclose(&*file) == 0);
  MapEditSelection selected;
  assert(!SFError_fail(MapEditSelection_init(&selected, selection_redraw, &fixture)));
  MapEditSelection_select(&selected, fixture.probe);

  MapEdit_smooth_selection(&fixture.map, &selected, &groups, &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {5, 7}}, 9, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 1});
  MapEditSelection_destroy(&selected);
  MapTexGroups_free(&groups);
  destroy();
}

#ifdef FORTIFY
static void test31(void)
{
  /* Animation allocation failure leaves map unchanged */

  init();
  host_expect_error(SFErrorType_NoMem);
  Fortify_SetNumAllocationsLimit(0);

  bool const success = MapEdit_write_anim(&fixture.map, fixture.probe, param(), &fixture.changes);

  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  host_check_error();
  assert(!success);
  assert(MapEdit_count_anims(&fixture.map) == 0);
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Map_RefMask);
  check_changes((MapEditChanges){0});
  destroy();
}

static void test32(void)
{
  /* Animation replacement requires no allocation */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, (MapPoint){6, 7}, param())));
  MapAnimParam replacement = param();
  replacement.period = 7;
  fixture.changes = (MapEditChanges){.tiles_changed = 3, .anims_added = 4,
    .anims_changed = 5, .anims_deleted = 6};

  Fortify_SetNumAllocationsLimit(0);
  bool const success = MapEdit_write_anim(&fixture.map, fixture.probe,
    replacement, &fixture.changes);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);

  assert(success);
  assert(MapEdit_count_anims(&fixture.map) == 2);
  check_param(fixture.probe, replacement);
  check_param((MapPoint){6, 7}, param());
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Map_RefMask);
  check_changes((MapEditChanges){.tiles_changed = 3, .anims_added = 5,
    .anims_changed = 5, .anims_deleted = 7});
  assert(fixture.before_count == 0);
  assert(fixture.redraw_count == 0);
  destroy();
}

#endif

static void test33(void)
{
  /* Animation replacement at capacity */

  init();
  for (MapCoord x = 0; x < AnimsMax; ++x)
  {
    assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, (MapPoint){x, 7}, param())));
  }
  MapAnimParam replacement = param();
  replacement.period = 7;

  assert(MapEdit_write_anim(&fixture.map, fixture.probe, replacement, &fixture.changes));

  assert(MapEdit_count_anims(&fixture.map) == AnimsMax);
  for (MapCoord x = 0; x < AnimsMax; ++x)
  {
    check_param((MapPoint){x, 7}, x == fixture.probe.x ? replacement : param());
  }
  check_changes((MapEditChanges){.anims_added = 1, .anims_deleted = 1});
  destroy();
}

static void test34(void)
{
  /* Animation replacement recalculates playback position */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  (void)MapEdit_update_anims(&fixture.map, 7, NULL);
  MapAnimParam replacement = param();
  replacement.period = 1;

  assert(MapEdit_write_anim(&fixture.map, fixture.probe, replacement, &fixture.changes));
  MapEdit_anims_to_map(&fixture.map, &fixture.changes);

  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, fixture.probe)) == 7);
  (void)MapEdit_update_anims(&fixture.map, 1, NULL);
  assert(map_ref_to_num(MapEdit_read_tile(&fixture.map, fixture.probe)) == 4);
  destroy();
}

static void test35(void)
{
  /* Animation replacement removes duplicates */

  init();
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  assert(!SFError_fail(MapAnims_add(fixture.anims, NULL, fixture.probe, param())));
  MapAnimParam replacement = param();
  replacement.period = 7;

  assert(MapEdit_write_anim(&fixture.map, fixture.probe, replacement, &fixture.changes));

  assert(MapEdit_count_anims(&fixture.map) == 1);
  check_param(fixture.probe, replacement);
  assert(MapAnims_check_locn(fixture.anims, fixture.probe));
  check_changes((MapEditChanges){.anims_added = 1, .anims_deleted = 2});
  destroy();
}

void MapEdit_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Composited map reads", test1 },
    { "Map reads without layers", test2 },
    { "Map write targets overlay", test3 },
    { "Map write targets base without overlay", test4 },
    { "Unchanged map write avoids redraw", test5 },
    { "Map write notifications surround mutation", test6 },
    { "Map coordinates wrap", test7 },
    { "Map editing without observers", test8 },
    { "Fill map area", test9 },
    { "Copy map transfer uses relative coordinates", test10 },
    { "Fill selected map cells", test11 },
    { "Map rectangle plotting", test12 },
    { "Map line plotting", test13 },
    { "Map unit circle", test14 },
    { "Map unit right triangle", test15 },
    { "Map flood fill stays in connected region", test16 },
    { "Map global replacement", test17 },
    { "Map overlay cropping preserves animated cells", test18 },
    { "Map range checks include hidden base cells", test19 },
    { "Map range checks ignore overlay masks", test20 },
    { "Map write removes animation", test21 },
    { "Write map animation", test22 },
    { "Replace map animation", test23 },
    { "Reverse selected animation", test24 },
    { "Delete selected animation", test25 },
    { "Animation frame replacement", test26 },
    { "Animation playback updates map", test27 },
    { "Animation reset restores first frame", test28 },
    { "Map editing without animation data", test29 },
    { "Smooth selected map cells", test30 },
    { "Animation replacement at capacity", test33 },
    { "Animation replacement recalculates playback position", test34 },
    { "Animation replacement removes duplicates", test35 },
#ifdef FORTIFY
    { "Animation allocation failure leaves map unchanged", test31 },
    { "Animation replacement requires no allocation", test32 },
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
