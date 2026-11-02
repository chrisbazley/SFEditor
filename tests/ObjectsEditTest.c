/* SFEditor test: objects editing operations.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderGKey.h"
#include "Tests.h"
#include "HostSupport.h"
#include "../ObjectsEdit.h"
#include "../ObjEditCtx.h"
#include "../ObjEditChg.h"
#include "../ObjEditSel.h"
#include "../TriggersData.h"
#include "../ObjGfxMesh.h"
#include "../ObjGfxMeshData.h"
#include "../DFile.h"

static ObjGfxMeshes meshes;

typedef struct
{
  ObjEditContext objects;
  ObjectsData *base, *overlay;
  union {
    char nil;
    TriggersData obj;
  } triggers;
  ObjEditChanges changes;
  MapPoint probe, redrawn_pos;
  MapArea before_area;
  unsigned before_count, redraw_count, trigger_count, reads;
  ObjRef before_value, after_value, old_ref, new_ref, base_ref;
  bool has_triggers;
  TriggerFullParam trigger;
  unsigned char transfer[2][3];
} Fixture;

static Fixture fixture;

/* The core passes session through unchanged; only its identity is used. */
static void before(MapArea const *area, struct EditSession *session)
{
  assert(session == fixture.objects.session);
  fixture.before_area = *area;
  fixture.before_value = ObjectsEdit_read_ref(&fixture.objects, fixture.probe);
  ++fixture.before_count;
}

static void redraw(MapPoint pos, ObjRef base, ObjRef old, ObjRef value,
  bool has_triggers, struct EditSession *session)
{
  assert(session == fixture.objects.session);
  fixture.redrawn_pos = pos;
  fixture.base_ref = base;
  fixture.old_ref = old;
  fixture.new_ref = value;
  fixture.has_triggers = has_triggers;
  fixture.after_value = ObjectsEdit_read_ref(&fixture.objects, fixture.probe);
  ++fixture.redraw_count;
}

static void redraw_trigger(MapPoint pos, ObjRef value, TriggerFullParam param,
  struct EditSession *session)
{
  assert(session == fixture.objects.session);
  fixture.redrawn_pos = pos;
  fixture.new_ref = value;
  fixture.trigger = param;
  ++fixture.trigger_count;
}

static void load_meshes(ObjGfxMeshes *data)
{
  _Optional FILE *const file = fopen("LandScapes/Graphics/Earth1", "rb");
  assert(file);
  Reader reader;
  assert(reader_gkey_init(&reader, 9, &*file));
  assert(!SFError_fail(ObjGfxMeshes_read(data, &reader)));
  assert(!reader_ferror(&reader));
  reader_destroy(&reader);
  assert(fclose(&*file) == 0);
}

static ObjectsData *new_objects(bool overlay)
{
  _Optional ObjectsData *const data = overlay ? objects_create_overlay() : objects_create_base();
  assert(data);
  return &*data;
}

static void init(void)
{
  ObjectsData *const base = new_objects(false), *const overlay = new_objects(true);
  fixture = (Fixture){
    .objects = {
      .base = base, .overlay = overlay, .triggers = &fixture.triggers.obj,
      .prechange_cb = before, .redraw_obj_cb = redraw, .redraw_trig_cb = redraw_trigger,
      .session = (struct EditSession *)&fixture,
    },
    .base = base, .overlay = overlay,
    .probe = {5, 7}, .transfer = {{100, 101, 102}, {103, 104, 105}},
  };
  assert(!SFError_fail(triggers_init(&fixture.triggers.obj)));

  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      objects_set_ref(fixture.base, (MapPoint){x, y}, objects_ref_none());
      objects_set_ref(fixture.overlay, (MapPoint){x, y}, objects_ref_mask());
    }
  }
}

static void destroy(void)
{
  triggers_destroy(&fixture.triggers.obj);
  dfile_release(objects_get_dfile(fixture.overlay));
  dfile_release(objects_get_dfile(fixture.base));
}

static void check_rect(ObjectsData const *data, MapArea area,
  unsigned char inside, unsigned char outside)
{
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      MapPoint const pos = {x, y};
      bool const contains = MapArea_is_valid(&area) && MapArea_contains(&area, pos);
      unsigned char const expected = contains ? inside : outside;
      assert(objects_ref_to_num(objects_get_ref(data, pos)) == expected);
    }
  }
}

static void check_changes(ObjEditChanges expected)
{
  assert(fixture.changes.refs_changed == expected.refs_changed);
  assert(fixture.changes.triggers_added == expected.triggers_added);
  assert(fixture.changes.triggers_changed == expected.triggers_changed);
  assert(fixture.changes.triggers_deleted == expected.triggers_deleted);
}

static TriggerFullParam target(int value)
{
  return (TriggerFullParam){
    .param = {.action = TriggerAction_MissionTarget, .value = value},
    .next_coords = {0, 0},
  };
}

static void check_trigger(TriggerFullParam actual, TriggerFullParam expected)
{
  assert(actual.param.action == expected.param.action);
  assert(actual.param.value == expected.param.value);
  assert(actual.next_coords.x == expected.next_coords.x);
  assert(actual.next_coords.y == expected.next_coords.y);
}

static size_t count_actions(MapPoint pos)
{
  TriggersIter iter;
  size_t count = 0;
  for (TriggersIter_get_first(&iter, &fixture.triggers.obj, &(MapArea){pos, pos}, NULL);
       !TriggersIter_done(&iter); TriggersIter_get_next(&iter, NULL))
  {
    ++count;
  }
  return count;
}

static size_t count_chains_to(MapPoint pos)
{
  TriggersChainIter iter;
  TriggerFullParam param;
  size_t count = 0;
  for (TriggersChainIter_get_first(&iter, &fixture.triggers.obj, &(MapArea){pos, pos}, &param);
       !TriggersChainIter_done(&iter); TriggersChainIter_get_next(&iter, &param))
  {
    ++count;
  }
  return count;
}

static ObjRef read_transfer(void *arg, MapPoint pos)
{
  assert(arg == &fixture);
  assert(pos.x >= 0);
  assert(pos.x < 3);
  assert(pos.y >= 0);
  assert(pos.y < 2);
  ++fixture.reads;
  return objects_ref_from_num(fixture.transfer[pos.y][pos.x]);
}

static ObjRef colliding_ref(void)
{
  for (unsigned char ref = 1; ref <= Obj_RefMaxObject; ++ref)
  {
    if (ref >= ObjGfxMeshes_get_ground_count(&meshes))
    {
      break;
    }
    ObjRef const value = objects_ref_from_num(ref);
    if (ObjGfxMeshes_get_collision_size(&meshes, value).x > 0)
    {
      return value;
    }
  }
  assert(!"Fixture requires an object with horizontal collision extent");
  return objects_ref_none();
}

static void test1(void)
{
  /* Collision cache reset after initialisation */

  static ObjGfxMeshes data;
  ObjGfxMeshes_init(&data);
  load_meshes(&data);
  MapPoint const loaded = ObjGfxMeshes_get_max_collision_size(&data);
  assert(loaded.x > 0);
  assert(loaded.y > 0);
  ObjGfxMeshes_free(&data);

  ObjGfxMeshes_init(&data);

  MapPoint const empty = ObjGfxMeshes_get_max_collision_size(&data);
  assert(empty.x == 0);
  assert(empty.y == 0);
  ObjGfxMeshes_free(&data);
}

static void test2(void)
{
  /* Composited object reads */

  init();
  objects_set_ref(fixture.base, fixture.probe, objects_ref_from_num(100));
  objects_set_ref(fixture.overlay, (MapPoint){6, 7}, objects_ref_from_num(101));

  assert(objects_ref_to_num(ObjectsEdit_read_ref(&fixture.objects, fixture.probe)) == 100);
  assert(objects_ref_to_num(ObjectsEdit_read_ref(&fixture.objects, (MapPoint){6, 7})) == 101);
  assert(objects_ref_to_num(ObjectsEdit_read_base(&fixture.objects, (MapPoint){6, 7})) == 0);
  assert(objects_ref_to_num(ObjectsEdit_read_overlay(&fixture.objects, fixture.probe)) == Obj_RefMask);
  destroy();
}

static void test3(void)
{
  /* Object reads without layers */

  init();
  fixture.objects = (ObjEditContext){.session = (struct EditSession *)&fixture};

  assert(objects_ref_to_num(ObjectsEdit_read_ref(&fixture.objects, fixture.probe)) == Obj_RefMask);
  assert(objects_ref_to_num(ObjectsEdit_read_base(&fixture.objects, fixture.probe)) == Obj_RefMask);
  assert(objects_ref_to_num(ObjectsEdit_read_overlay(&fixture.objects, fixture.probe)) == Obj_RefMask);
  destroy();
}

static void test4(void)
{
  /* Object write targets overlay */

  init();

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_None, &fixture.changes, &meshes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {5, 7}}, 100, Obj_RefMask);
  check_rect(fixture.base, MapArea_make_invalid(), 0, 0);
  check_changes((ObjEditChanges){.refs_changed = 1});
  destroy();
}

static void test5(void)
{
  /* Object write targets base without overlay */

  init();
  fixture.objects.overlay = NULL;

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_None, &fixture.changes, &meshes);

  check_rect(fixture.base, (MapArea){.min = {5, 7}, .max = {5, 7}}, 100, 0);
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 1});
  destroy();
}

static void test6(void)
{
  /* Unchanged object write avoids redraw */

  init();
  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_from_num(100));

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_None, &fixture.changes, &meshes);

  assert(fixture.redraw_count == 0);
  check_changes((ObjEditChanges){0});
  destroy();
}

static void test7(void)
{
  /* Object write notifications surround mutation */

  init();
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(1))));

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_None, &fixture.changes, &meshes);

  assert(fixture.before_count == 1);
  assert(fixture.redraw_count == 1);
  assert(objects_ref_to_num(fixture.before_value) == 0);
  assert(objects_ref_to_num(fixture.after_value) == 100);
  assert(objects_ref_to_num(fixture.base_ref) == 0);
  assert(objects_ref_to_num(fixture.old_ref) == Obj_RefMask);
  assert(objects_ref_to_num(fixture.new_ref) == 100);
  assert(fixture.has_triggers);
  assert(fixture.redrawn_pos.x == fixture.probe.x);
  assert(fixture.redrawn_pos.y == fixture.probe.y);
  destroy();
}

static void test8(void)
{
  /* Object coordinates wrap */

  init();

  ObjectsEdit_write_ref(&fixture.objects, (MapPoint){-1, Obj_Size + 1}, objects_ref_from_num(100),
    TriggersWipeAction_None, &fixture.changes, &meshes);

  assert(objects_ref_to_num(ObjectsEdit_read_ref(&fixture.objects, (MapPoint){-1, Obj_Size + 1})) == 100);
  check_rect(fixture.overlay, (MapArea){.min = {Obj_Size - 1, 1}, .max = {Obj_Size - 1, 1}}, 100, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 1});
  destroy();
}

static void test9(void)
{
  /* Reserved object rows reject writes */

  init();
  static MapCoord const rows[] = {0, Obj_Size - 2, Obj_Size};

  for (size_t i = 0; i < ARRAY_SIZE(rows); ++i)
  {
    ObjectsEdit_write_ref(&fixture.objects, (MapPoint){5, rows[i]}, objects_ref_from_num(100),
      TriggersWipeAction_None, &fixture.changes, &meshes);
  }

  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Obj_RefMask);
  check_changes((ObjEditChanges){0});
  assert(fixture.before_count == 0);
  destroy();
}

static void test10(void)
{
  /* Object editing without observers */

  init();
  fixture.objects = (ObjEditContext){.base = fixture.base, .session = (struct EditSession *)&fixture};

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_None, NULL, &meshes);

  check_rect(fixture.base, (MapArea){.min = {5, 7}, .max = {5, 7}}, 100, 0);
  destroy();
}

static void test11(void)
{
  /* Fill object area */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 8}};

  ObjectsEdit_fill_area(&fixture.objects, &area, objects_ref_from_num(100), &fixture.changes, &meshes);

  check_rect(fixture.overlay, area, 100, Obj_RefMask);
  check_rect(fixture.base, MapArea_make_invalid(), 0, 0);
  check_changes((ObjEditChanges){.refs_changed = 9});
  destroy();
}

static void test12(void)
{
  /* Copy object transfer uses relative coordinates */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 7}};

  ObjectsEdit_copy_to_area(&fixture.objects, &area, read_transfer, &fixture, &fixture.changes, &meshes);

  assert(fixture.reads == 6);
  check_changes((ObjEditChanges){.refs_changed = 6});
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      MapPoint const pos = {x, y};
      unsigned char const expected = MapArea_contains(&area, pos) ?
        fixture.transfer[y - area.min.y][x - area.min.x] : Obj_RefMask;
      assert(objects_ref_to_num(objects_get_ref(fixture.overlay, pos)) == expected);
    }
  }
  check_rect(fixture.base, MapArea_make_invalid(), 0, 0);
  destroy();
}

static void test13(void)
{
  /* Object transfer preflight leaves data unchanged */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 7}};

  assert(ObjectsEdit_can_copy_to_area(&fixture.objects, &area, read_transfer, &fixture, &meshes, NULL));

  assert(fixture.reads == 6);
  assert(fixture.before_count == 0);
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Obj_RefMask);
  check_changes((ObjEditChanges){0});
  destroy();
}

static void test14(void)
{
  /* Object transfer preflight rejects reserved row */

  init();
  MapArea const area = {.min = {4, 0}, .max = {6, 1}};

  assert(!ObjectsEdit_can_copy_to_area(&fixture.objects, &area, read_transfer, &fixture, &meshes, NULL));

  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Obj_RefMask);
  check_changes((ObjEditChanges){0});
  destroy();
}

static void test15(void)
{
  /* Fill selected objects */

  init();
  ObjEditSelection selected;
  assert(!SFError_fail(ObjEditSelection_init(&selected, NULL, &fixture)));
  ObjEditSelection_select(&selected, fixture.probe);
  ObjEditSelection_select(&selected, (MapPoint){6, 7});

  ObjectsEdit_fill_selected(&fixture.objects, &selected, objects_ref_from_num(100), &fixture.changes, &meshes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {6, 7}}, 100, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 2});
  ObjEditSelection_destroy(&selected);
  destroy();
}

static void test16(void)
{
  /* Object rectangle plotting */

  init();

  ObjectsEdit_plot_rect(&fixture.objects, (MapPoint){6, 8}, (MapPoint){4, 6}, objects_ref_from_num(100), &fixture.changes, &meshes);

  check_rect(fixture.overlay, (MapArea){.min = {4, 6}, .max = {6, 8}}, 100, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 9});
  destroy();
}

static void test17(void)
{
  /* Object line plotting */

  init();

  ObjectsEdit_plot_line(&fixture.objects, (MapPoint){4, 7}, (MapPoint){6, 7}, objects_ref_from_num(100), 0, &fixture.changes, &meshes);

  check_rect(fixture.overlay, (MapArea){.min = {4, 7}, .max = {6, 7}}, 100, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 3});
  destroy();
}

static void test18(void)
{
  /* Object unit circle */

  init();

  ObjectsEdit_plot_circ(&fixture.objects, fixture.probe, 1, objects_ref_from_num(100), &fixture.changes, &meshes);

  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      bool const painted = ((x == 5) && (y >= 6) && (y <= 8)) ||
                           ((y == 7) && (x >= 4) && (x <= 6));
      unsigned char const expected = painted ? 100 : Obj_RefMask;
      assert(objects_ref_to_num(objects_get_ref(fixture.overlay, (MapPoint){x, y})) == expected);
    }
  }
  check_rect(fixture.base, MapArea_make_invalid(), 0, 0);
  check_changes((ObjEditChanges){.refs_changed = 5});
  destroy();
}

static void test19(void)
{
  /* Object unit right triangle */

  init();

  ObjectsEdit_plot_tri(&fixture.objects, (MapPoint){5, 7}, (MapPoint){6, 7}, (MapPoint){5, 8}, objects_ref_from_num(100), &fixture.changes, &meshes);

  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      bool const painted = ((y == 7) && (x >= 5) && (x <= 6)) ||
                           ((x == 5) && (y == 8));
      unsigned char const expected = painted ? 100 : Obj_RefMask;
      assert(objects_ref_to_num(objects_get_ref(fixture.overlay, (MapPoint){x, y})) == expected);
    }
  }
  check_rect(fixture.base, MapArea_make_invalid(), 0, 0);
  check_changes((ObjEditChanges){.refs_changed = 3});
  destroy();
}

static void test20(void)
{
  /* Object flood fill stays in connected region */

  init();
  MapArea const area = {.min = {4, 6}, .max = {6, 8}};
  for (MapCoord y = 6; y <= 8; ++y)
  {
    for (MapCoord x = 4; x <= 6; ++x)
    {
      objects_set_ref(fixture.base, (MapPoint){x, y}, objects_ref_from_num(101));
    }
  }
  objects_set_ref(fixture.base, (MapPoint){10, 10}, objects_ref_from_num(101));

  ObjectsEdit_flood_fill(&fixture.objects, objects_ref_from_num(100), fixture.probe, &fixture.changes, &meshes);

  check_rect(fixture.overlay, area, 100, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 9});
  assert(objects_ref_to_num(objects_get_ref(fixture.base, (MapPoint){10, 10})) == 101);
  destroy();
}

static void test21(void)
{
  /* Object global replacement */

  init();
  objects_set_ref(fixture.base, fixture.probe, objects_ref_from_num(101));

  ObjectsEdit_global_replace(&fixture.objects, objects_ref_from_num(101), objects_ref_from_num(100), &fixture.changes, &meshes);

  check_rect(fixture.overlay, (MapArea){.min = {5, 7}, .max = {5, 7}}, 100, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 1});
  destroy();
}

static void test22(void)
{
  /* Object overlay cropping preserves trigger locations */

  init();
  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_none());
  objects_set_ref(fixture.overlay, (MapPoint){6, 7}, objects_ref_none());
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, (MapPoint){6, 7}, target(1))));

  ObjectsEdit_crop_overlay(&fixture.objects, &fixture.changes);

  check_rect(fixture.overlay, (MapArea){.min = {6, 7}, .max = {6, 7}}, 0, Obj_RefMask);
  check_changes((ObjEditChanges){.refs_changed = 1});
  destroy();
}

static void test23(void)
{
  /* Object range checks include hidden base cells */

  init();
  objects_set_ref(fixture.base, fixture.probe, objects_ref_from_num(9));
  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_from_num(1));

  assert(!ObjectsEdit_check_ref_range(&fixture.objects, 9));
  assert(ObjectsEdit_check_ref_range(&fixture.objects, 10));
  destroy();
}

static void test24(void)
{
  /* Object range checks ignore special references */

  init();
  objects_set_ref(fixture.base, fixture.probe, objects_ref_from_num(100));
  objects_set_ref(fixture.overlay, (MapPoint){6, 7}, objects_ref_from_num(Obj_RefHill));

  assert(ObjectsEdit_check_ref_range(&fixture.objects, 1));

  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_from_num(9));
  assert(!ObjectsEdit_check_ref_range(&fixture.objects, 9));
  destroy();
}

static void test25(void)
{
  /* Add object trigger */

  init();

  assert(ObjectsEdit_add_trigger(&fixture.objects, fixture.probe, target(7), &fixture.changes));

  assert(count_actions(fixture.probe) == 1);
  check_trigger(fixture.trigger, target(7));
  assert(fixture.trigger_count == 1);
  check_changes((ObjEditChanges){.triggers_added = 1});
  destroy();
}

static void test26(void)
{
  /* Add trigger without trigger data */

  init();
  fixture.objects.triggers = NULL;

  assert(ObjectsEdit_add_trigger(&fixture.objects, fixture.probe, target(7), &fixture.changes));

  assert(fixture.trigger_count == 0);
  check_changes((ObjEditChanges){0});
  destroy();
}

static void test27(void)
{
  /* Object write preserves incoming chain */

  init();
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(1))));
  TriggerFullParam const chain = {.param = {.action = TriggerAction_ChainReaction, .value = 1}, .next_coords = {5, 7}};
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, (MapPoint){10, 10}, chain)));

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_KeepChain, &fixture.changes, &meshes);

  assert(count_actions(fixture.probe) == 0);
  assert(count_chains_to(fixture.probe) == 1);
  check_changes((ObjEditChanges){.refs_changed = 1, .triggers_deleted = 1});
  destroy();
}

static void test28(void)
{
  /* Object write breaks incoming chain */

  init();
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(1))));
  TriggerFullParam const chain = {.param = {.action = TriggerAction_ChainReaction, .value = 1}, .next_coords = {5, 7}};
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, (MapPoint){10, 10}, chain)));

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    TriggersWipeAction_BreakChain, &fixture.changes, &meshes);

  assert(count_actions(fixture.probe) == 0);
  assert(count_chains_to(fixture.probe) == 0);
  check_changes((ObjEditChanges){.refs_changed = 1, .triggers_deleted = 2});
  destroy();
}

static void test29(void)
{
  /* Object deletion breaks incoming chain */

  init();
  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_from_num(100));
  TriggerFullParam const chain = {.param = {.action = TriggerAction_ChainReaction, .value = 1}, .next_coords = {5, 7}};
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, (MapPoint){10, 10}, chain)));

  ObjectsEdit_write_ref(&fixture.objects, fixture.probe, objects_ref_none(),
    TriggersWipeAction_None, &fixture.changes, &meshes);

  assert(count_chains_to(fixture.probe) == 0);
  check_changes((ObjEditChanges){.refs_changed = 1, .triggers_deleted = 1});
  destroy();
}

static void test30(void)
{
  /* Wipe selected object triggers */

  init();
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(1))));
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, (MapPoint){6, 7}, target(2))));
  ObjEditSelection selected;
  assert(!SFError_fail(ObjEditSelection_init(&selected, NULL, &fixture)));
  ObjEditSelection_select(&selected, fixture.probe);
  ObjEditSelection_select(&selected, (MapPoint){7, 7});

  ObjectsEdit_wipe_triggers(&fixture.objects, &selected, &fixture.changes);

  assert(count_actions(fixture.probe) == 0);
  assert(count_actions((MapPoint){6, 7}) == 1);
  check_changes((ObjEditChanges){.triggers_deleted = 1});
  ObjEditSelection_destroy(&selected);
  destroy();
}

static void test31(void)
{
  /* Equivalent trigger replacement avoids mutation */

  init();
  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_from_num(100));
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(1))));
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(2))));
  TriggerFullParam const replacement[] = {target(2), target(1)};

  assert(ObjectsEdit_write_ref_n_triggers(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    replacement, ARRAY_SIZE(replacement), &fixture.changes, &meshes));

  assert(fixture.before_count == 0);
  assert(fixture.trigger_count == 0);
  assert(count_actions(fixture.probe) == 2);
  check_changes((ObjEditChanges){0});
  destroy();
}

static void test32(void)
{
  /* Replace object trigger set */

  init();
  objects_set_ref(fixture.overlay, fixture.probe, objects_ref_from_num(100));
  assert(!SFError_fail(triggers_add(&fixture.triggers.obj, fixture.probe, target(1))));
  TriggerFullParam const replacement[] = {target(2), target(3)};

  assert(ObjectsEdit_write_ref_n_triggers(&fixture.objects, fixture.probe, objects_ref_from_num(101),
    replacement, ARRAY_SIZE(replacement), &fixture.changes, &meshes));

  assert(count_actions(fixture.probe) == 2);
  assert(objects_ref_to_num(ObjectsEdit_read_ref(&fixture.objects, fixture.probe)) == 101);
  check_changes((ObjEditChanges){.refs_changed = 1, .triggers_added = 2, .triggers_deleted = 1});
  destroy();
}

static void test33(void)
{
  /* Trigger replacement without trigger data fails */

  init();
  fixture.objects.triggers = NULL;
  TriggerFullParam const replacement[] = {target(2)};

  assert(!ObjectsEdit_write_ref_n_triggers(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    replacement, ARRAY_SIZE(replacement), &fixture.changes, &meshes));

  check_changes((ObjEditChanges){0});
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Obj_RefMask);
  destroy();
}

static void test34(void)
{
  /* Collision preflight identifies occluded object */

  init();
  ObjRef const value = colliding_ref();
  objects_set_ref(fixture.overlay, (MapPoint){65, 64}, value);
  ObjEditSelection occluded;
  assert(!SFError_fail(ObjEditSelection_init(&occluded, NULL, &fixture)));

  assert(ObjectsEdit_can_place(&fixture.objects, (MapPoint){64, 64}, value, &meshes, &occluded));

  assert(ObjEditSelection_size(&occluded) == 1);
  assert(ObjEditSelection_is_selected(&occluded, (MapPoint){65, 64}));
  assert(objects_ref_is_equal(objects_get_ref(fixture.overlay, (MapPoint){65, 64}), value));
  check_changes((ObjEditChanges){0});
  ObjEditSelection_destroy(&occluded);
  destroy();
}

static void test35(void)
{
  /* Object placement clears overlap */

  init();
  ObjRef const value = colliding_ref();
  objects_set_ref(fixture.overlay, (MapPoint){65, 64}, value);

  ObjectsEdit_write_ref(&fixture.objects, (MapPoint){64, 64}, value,
    TriggersWipeAction_None, &fixture.changes, &meshes);

  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      unsigned char expected = Obj_RefMask;
      if ((x == 64) && (y == 64))
      {
        expected = objects_ref_to_num(value);
      }
      else if ((x == 65) && (y == 64))
      {
        expected = 0;
      }
      assert(objects_ref_to_num(objects_get_ref(fixture.overlay, (MapPoint){x, y})) == expected);
    }
  }
  check_changes((ObjEditChanges){.refs_changed = 2});
  destroy();
}

#ifdef FORTIFY
static void test36(void)
{
  /* Trigger allocation failure leaves objects unchanged */

  init();
  TriggerFullParam const replacement[] = {target(2)};
  host_expect_error(SFErrorType_NoMem);
  Fortify_SetNumAllocationsLimit(0);

  bool const success = ObjectsEdit_write_ref_n_triggers(&fixture.objects, fixture.probe, objects_ref_from_num(100),
    replacement, ARRAY_SIZE(replacement), &fixture.changes, &meshes);

  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  host_check_error();
  assert(!success);
  assert(count_actions(fixture.probe) == 0);
  check_rect(fixture.overlay, MapArea_make_invalid(), 0, Obj_RefMask);
  check_changes((ObjEditChanges){0});
  destroy();
}

#endif

void ObjectsEdit_tests(void)
{
  ObjGfxMeshes_init(&meshes);
  load_meshes(&meshes);

  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Collision cache reset after initialisation", test1 },
    { "Composited object reads", test2 },
    { "Object reads without layers", test3 },
    { "Object write targets overlay", test4 },
    { "Object write targets base without overlay", test5 },
    { "Unchanged object write avoids redraw", test6 },
    { "Object write notifications surround mutation", test7 },
    { "Object coordinates wrap", test8 },
    { "Reserved object rows reject writes", test9 },
    { "Object editing without observers", test10 },
    { "Fill object area", test11 },
    { "Copy object transfer uses relative coordinates", test12 },
    { "Object transfer preflight leaves data unchanged", test13 },
    { "Object transfer preflight rejects reserved row", test14 },
    { "Fill selected objects", test15 },
    { "Object rectangle plotting", test16 },
    { "Object line plotting", test17 },
    { "Object unit circle", test18 },
    { "Object unit right triangle", test19 },
    { "Object flood fill stays in connected region", test20 },
    { "Object global replacement", test21 },
    { "Object overlay cropping preserves trigger locations", test22 },
    { "Object range checks include hidden base cells", test23 },
    { "Object range checks ignore special references", test24 },
    { "Add object trigger", test25 },
    { "Add trigger without trigger data", test26 },
    { "Object write preserves incoming chain", test27 },
    { "Object write breaks incoming chain", test28 },
    { "Object deletion breaks incoming chain", test29 },
    { "Wipe selected object triggers", test30 },
    { "Equivalent trigger replacement avoids mutation", test31 },
    { "Replace object trigger set", test32 },
    { "Trigger replacement without trigger data fails", test33 },
    { "Collision preflight identifies occluded object", test34 },
    { "Object placement clears overlap", test35 },
#ifdef FORTIFY
    { "Trigger allocation failure leaves objects unchanged", test36 },
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
  ObjGfxMeshes_free(&meshes);
}
