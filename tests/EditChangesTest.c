/* SFEditor test: editing change counters.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include <stdarg.h>
#include "msgtrans.h"
#include "../MapEditChg.h"
#include "../ObjEditChg.h"
#include "../InfoEditChg.h"

/* Capture requests at the message-translation boundary; no Messages file
 * or RISC OS service is needed to check tokens and substitutions. */
static struct
{
  size_t count;
  unsigned calls;
  char token[16];
  char args[4][24];
} message;
static char translated[] = "translated";

char *msgs_lookup_subn(char const *token, size_t count, ...)
{
  assert(strlen(token) < sizeof(message.token));
  assert(count <= ARRAY_SIZE(message.args));
  strcpy(message.token, token);
  message.count = count;
  ++message.calls;
  va_list args;
  va_start(args, count);

  for (size_t i = 0; i < count; ++i)
  {
    char const *const value = va_arg(args, char const *);
    assert(strlen(value) < sizeof(message.args[i]));
    strcpy(message.args[i], value);
  }

  va_end(args);
  return translated;
}

static void reset_message(void)
{
  message.count = 0;
  message.calls = 0;
}

static void check_map(MapEditChanges const *actual, MapEditChanges expected)
{
  assert(actual->tiles_changed == expected.tiles_changed);
  assert(actual->anims_added == expected.anims_added);
  assert(actual->anims_changed == expected.anims_changed);
  assert(actual->anims_deleted == expected.anims_deleted);
}

static void check_objects(ObjEditChanges const *actual, ObjEditChanges expected)
{
  assert(actual->refs_changed == expected.refs_changed);
  assert(actual->triggers_added == expected.triggers_added);
  assert(actual->triggers_changed == expected.triggers_changed);
  assert(actual->triggers_deleted == expected.triggers_deleted);
}

static void check_infos(InfoEditChanges const *actual, InfoEditChanges expected)
{
  assert(actual->infos_added == expected.infos_added);
  assert(actual->infos_changed == expected.infos_changed);
  assert(actual->infos_deleted == expected.infos_deleted);
}

static void test1(void)
{
  /* Map counters initialise to zero */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_init(&changes);

  check_map(&changes, (MapEditChanges){.tiles_changed = 0, .anims_added = 0, .anims_changed = 0, .anims_deleted = 0});
  assert(!MapEditChanges_is_changed(&changes));
}

static void test2(void)
{
  /* Map tiles changed increment */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_change_tile(&changes);

  check_map(&changes, (MapEditChanges){.tiles_changed = 4, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9});
}

static void test3(void)
{
  /* Map anims added increment */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_add_anim(&changes);

  check_map(&changes, (MapEditChanges){.tiles_changed = 3, .anims_added = 6, .anims_changed = 7, .anims_deleted = 9});
}

static void test4(void)
{
  /* Map anims changed increment */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_change_anim(&changes);

  check_map(&changes, (MapEditChanges){.tiles_changed = 3, .anims_added = 5, .anims_changed = 8, .anims_deleted = 9});
}

static void test5(void)
{
  /* Map anims deleted increment */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_delete_anim(&changes);

  check_map(&changes, (MapEditChanges){.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 10});
}

static void test6(void)
{
  /* Map bulk counter increment */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_change_tiles(&changes, 17);

  check_map(&changes, (MapEditChanges){.tiles_changed = 20, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9});
}

static void test7(void)
{
  /* Map zero counter increment */

  MapEditChanges changes = {.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9};

  MapEditChanges_change_tiles(&changes, 0);

  check_map(&changes, (MapEditChanges){.tiles_changed = 3, .anims_added = 5, .anims_changed = 7, .anims_deleted = 9});
}

static void test8(void)
{
  /* Map null counter updates */

  MapEditChanges_change_tile(NULL);
  MapEditChanges_add_anim(NULL);
  MapEditChanges_change_anim(NULL);
  MapEditChanges_delete_anim(NULL);
  MapEditChanges_change_tiles(NULL, 17);

  assert(!MapEditChanges_is_changed(NULL));
}

static void test9(void)
{
  /* Map change predicates */

  for (unsigned mask = 0; mask < 16; ++mask)
  {
    MapEditChanges const changes = {
      .tiles_changed = (mask & 1) ? 3 : 0,
      .anims_added = (mask & 2) ? 5 : 0,
      .anims_changed = (mask & 4) ? 7 : 0,
      .anims_deleted = (mask & 8) ? 9 : 0,
    };

    assert(MapEditChanges_is_changed(&changes) == (mask != 0));
    assert(MapEditChanges_map_changed(&changes) == ((mask & 1) != 0));
    assert(MapEditChanges_anims_changed(&changes) == ((mask & 14) != 0));
  }
}

static void test10(void)
{
  /* Map empty change message */

  MapEditChanges changes;
  MapEditChanges_init(&changes);
  reset_message();

  assert(MapEditChanges_get_message(&changes) == NULL);

  assert(message.calls == 0);
}

static void test11(void)
{
  /* Map message substitutions */

  static char const *const tokens[] = {
    "MStatusT",
    "MStatusA",
    "MStatusTA",
    "MStatusC",
    "MStatusTC",
    "MStatusAC",
    "MStatusTAC",
    "MStatusK",
    "MStatusTK",
    "MStatusAK",
    "MStatusTAK",
    "MStatusCK",
    "MStatusTCK",
    "MStatusACK",
    "MStatusTACK",
  };

  for (unsigned mask = 1; mask < 16; ++mask)
  {
    MapEditChanges const changes = {
      .tiles_changed = (mask & 1) ? 3 : 0,
      .anims_added = (mask & 2) ? 5 : 0,
      .anims_changed = (mask & 4) ? 7 : 0,
      .anims_deleted = (mask & 8) ? 9 : 0,
    };
    reset_message();

    assert(MapEditChanges_get_message(&changes) == translated);

    assert(message.calls == 1);
    assert(message.count == 4);
    assert(strcmp(message.token, tokens[mask - 1]) == 0);
    assert(strcmp(message.args[0], (mask & 1) ? "3" : "") == 0);
    assert(strcmp(message.args[1], (mask & 2) ? "5" : "") == 0);
    assert(strcmp(message.args[2], (mask & 4) ? "7" : "") == 0);
    assert(strcmp(message.args[3], (mask & 8) ? "9" : "") == 0);
  }
}

static void test12(void)
{
  /* Map maximum counter message */

  MapEditChanges const changes = {.tiles_changed = ULONG_MAX, .anims_added = ULONG_MAX, .anims_changed = ULONG_MAX, .anims_deleted = ULONG_MAX};
  char expected[24];
  assert(snprintf(expected, sizeof(expected), "%lu", ULONG_MAX) > 0);
  reset_message();

  assert(MapEditChanges_get_message(&changes) == translated);

  assert(message.count == 4);
  for (size_t i = 0; i < message.count; ++i)
  {
    assert(strcmp(message.args[i], expected) == 0);
  }
}

static void test13(void)
{
  /* Object counters initialise to zero */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_init(&changes);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 0, .triggers_added = 0, .triggers_changed = 0, .triggers_deleted = 0});
  assert(!ObjEditChanges_is_changed(&changes));
}

static void test14(void)
{
  /* Object refs changed increment */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_change_ref(&changes);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 4, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9});
}

static void test15(void)
{
  /* Object triggers added increment */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_add_trig(&changes);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 3, .triggers_added = 6, .triggers_changed = 7, .triggers_deleted = 9});
}

static void test16(void)
{
  /* Object triggers changed increment */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_change_trig(&changes);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 3, .triggers_added = 5, .triggers_changed = 8, .triggers_deleted = 9});
}

static void test17(void)
{
  /* Object triggers deleted increment */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_delete_trig(&changes);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 10});
}

static void test18(void)
{
  /* Object bulk counter increment */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_change_refs(&changes, 17);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 20, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9});
}

static void test19(void)
{
  /* Object zero counter increment */

  ObjEditChanges changes = {.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9};

  ObjEditChanges_change_refs(&changes, 0);

  check_objects(&changes, (ObjEditChanges){.refs_changed = 3, .triggers_added = 5, .triggers_changed = 7, .triggers_deleted = 9});
}

static void test20(void)
{
  /* Object null counter updates */

  ObjEditChanges_change_ref(NULL);
  ObjEditChanges_add_trig(NULL);
  ObjEditChanges_change_trig(NULL);
  ObjEditChanges_delete_trig(NULL);
  ObjEditChanges_change_refs(NULL, 17);

  assert(!ObjEditChanges_is_changed(NULL));
}

static void test21(void)
{
  /* Object change predicates */

  for (unsigned mask = 0; mask < 16; ++mask)
  {
    ObjEditChanges const changes = {
      .refs_changed = (mask & 1) ? 3 : 0,
      .triggers_added = (mask & 2) ? 5 : 0,
      .triggers_changed = (mask & 4) ? 7 : 0,
      .triggers_deleted = (mask & 8) ? 9 : 0,
    };

    assert(ObjEditChanges_is_changed(&changes) == (mask != 0));
    assert(ObjEditChanges_refs_changed(&changes) == ((mask & 1) != 0));
    assert(ObjEditChanges_triggers_changed(&changes) == ((mask & 14) != 0));
  }
}

static void test22(void)
{
  /* Object empty change message */

  ObjEditChanges changes;
  ObjEditChanges_init(&changes);
  reset_message();

  assert(ObjEditChanges_get_message(&changes) == NULL);

  assert(message.calls == 0);
}

static void test23(void)
{
  /* Object message substitutions */

  static char const *const tokens[] = {
    "OStatusO",
    "OStatusA",
    "OStatusOA",
    "OStatusC",
    "OStatusOC",
    "OStatusAC",
    "OStatusOAC",
    "OStatusK",
    "OStatusOK",
    "OStatusAK",
    "OStatusOAK",
    "OStatusCK",
    "OStatusOCK",
    "OStatusACK",
    "OStatusOACK",
  };

  for (unsigned mask = 1; mask < 16; ++mask)
  {
    ObjEditChanges const changes = {
      .refs_changed = (mask & 1) ? 3 : 0,
      .triggers_added = (mask & 2) ? 5 : 0,
      .triggers_changed = (mask & 4) ? 7 : 0,
      .triggers_deleted = (mask & 8) ? 9 : 0,
    };
    reset_message();

    assert(ObjEditChanges_get_message(&changes) == translated);

    assert(message.calls == 1);
    assert(message.count == 4);
    assert(strcmp(message.token, tokens[mask - 1]) == 0);
    assert(strcmp(message.args[0], (mask & 1) ? "3" : "") == 0);
    assert(strcmp(message.args[1], (mask & 2) ? "5" : "") == 0);
    assert(strcmp(message.args[2], (mask & 4) ? "7" : "") == 0);
    assert(strcmp(message.args[3], (mask & 8) ? "9" : "") == 0);
  }
}

static void test24(void)
{
  /* Object maximum counter message */

  ObjEditChanges const changes = {.refs_changed = ULONG_MAX, .triggers_added = ULONG_MAX, .triggers_changed = ULONG_MAX, .triggers_deleted = ULONG_MAX};
  char expected[24];
  assert(snprintf(expected, sizeof(expected), "%lu", ULONG_MAX) > 0);
  reset_message();

  assert(ObjEditChanges_get_message(&changes) == translated);

  assert(message.count == 4);
  for (size_t i = 0; i < message.count; ++i)
  {
    assert(strcmp(message.args[i], expected) == 0);
  }
}

static void test25(void)
{
  /* Information counters initialise to zero */

  InfoEditChanges changes = {.infos_added = 3, .infos_changed = 5, .infos_deleted = 7};

  InfoEditChanges_init(&changes);

  check_infos(&changes, (InfoEditChanges){.infos_added = 0, .infos_changed = 0, .infos_deleted = 0});
  assert(!InfoEditChanges_is_changed(&changes));
}

static void test26(void)
{
  /* Information infos added increment */

  InfoEditChanges changes = {.infos_added = 3, .infos_changed = 5, .infos_deleted = 7};

  InfoEditChanges_add(&changes);

  check_infos(&changes, (InfoEditChanges){.infos_added = 4, .infos_changed = 5, .infos_deleted = 7});
}

static void test27(void)
{
  /* Information infos changed increment */

  InfoEditChanges changes = {.infos_added = 3, .infos_changed = 5, .infos_deleted = 7};

  InfoEditChanges_change(&changes);

  check_infos(&changes, (InfoEditChanges){.infos_added = 3, .infos_changed = 6, .infos_deleted = 7});
}

static void test28(void)
{
  /* Information infos deleted increment */

  InfoEditChanges changes = {.infos_added = 3, .infos_changed = 5, .infos_deleted = 7};

  InfoEditChanges_delete(&changes);

  check_infos(&changes, (InfoEditChanges){.infos_added = 3, .infos_changed = 5, .infos_deleted = 8});
}

static void test29(void)
{
  /* Information null counter updates */

  InfoEditChanges_add(NULL);
  InfoEditChanges_change(NULL);
  InfoEditChanges_delete(NULL);

  assert(!InfoEditChanges_is_changed(NULL));
}

static void test30(void)
{
  /* Information change predicates */

  for (unsigned mask = 0; mask < 8; ++mask)
  {
    InfoEditChanges const changes = {
      .infos_added = (mask & 1) ? 3 : 0,
      .infos_changed = (mask & 2) ? 5 : 0,
      .infos_deleted = (mask & 4) ? 7 : 0,
    };

    assert(InfoEditChanges_is_changed(&changes) == (mask != 0));
  }
}

static void test31(void)
{
  /* Information empty change message */

  InfoEditChanges changes;
  InfoEditChanges_init(&changes);
  reset_message();

  assert(InfoEditChanges_get_message(&changes) == NULL);

  assert(message.calls == 0);
}

static void test32(void)
{
  /* Information message substitutions */

  static char const *const tokens[] = {
    "IStatusA",
    "IStatusC",
    "IStatusAC",
    "IStatusK",
    "IStatusAK",
    "IStatusCK",
    "IStatusACK",
  };

  for (unsigned mask = 1; mask < 8; ++mask)
  {
    InfoEditChanges const changes = {
      .infos_added = (mask & 1) ? 3 : 0,
      .infos_changed = (mask & 2) ? 5 : 0,
      .infos_deleted = (mask & 4) ? 7 : 0,
    };
    reset_message();

    assert(InfoEditChanges_get_message(&changes) == translated);

    assert(message.calls == 1);
    assert(message.count == 3);
    assert(strcmp(message.token, tokens[mask - 1]) == 0);
    assert(strcmp(message.args[0], (mask & 1) ? "3" : "") == 0);
    assert(strcmp(message.args[1], (mask & 2) ? "5" : "") == 0);
    assert(strcmp(message.args[2], (mask & 4) ? "7" : "") == 0);
  }
}

static void test33(void)
{
  /* Information maximum counter message */

  InfoEditChanges const changes = {.infos_added = ULONG_MAX, .infos_changed = ULONG_MAX, .infos_deleted = ULONG_MAX};
  char expected[24];
  assert(snprintf(expected, sizeof(expected), "%lu", ULONG_MAX) > 0);
  reset_message();

  assert(InfoEditChanges_get_message(&changes) == translated);

  assert(message.count == 3);
  for (size_t i = 0; i < message.count; ++i)
  {
    assert(strcmp(message.args[i], expected) == 0);
  }
}

static void test34(void)
{
  /* Animation count direction */

  for (unsigned long added = 0; added < 3; ++added)
  {
    for (unsigned long deleted = 0; deleted < 3; ++deleted)
    {
      MapEditChanges const changes = {.anims_added = added, .anims_deleted = deleted};

      assert(MapEditChanges_anims_increased(&changes) == (added > deleted));
      assert(MapEditChanges_anims_decreased(&changes) == (added < deleted));
    }
  }

  assert(!MapEditChanges_anims_increased(NULL));
  assert(!MapEditChanges_anims_decreased(NULL));
}

void EditChanges_tests(void)
{
  /* EditChanges */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Map counters initialise to zero", test1 },
    { "Map tiles changed increment", test2 },
    { "Map anims added increment", test3 },
    { "Map anims changed increment", test4 },
    { "Map anims deleted increment", test5 },
    { "Map bulk counter increment", test6 },
    { "Map zero counter increment", test7 },
    { "Map null counter updates", test8 },
    { "Map change predicates", test9 },
    { "Map empty change message", test10 },
    { "Map message substitutions", test11 },
    { "Map maximum counter message", test12 },
    { "Object counters initialise to zero", test13 },
    { "Object refs changed increment", test14 },
    { "Object triggers added increment", test15 },
    { "Object triggers changed increment", test16 },
    { "Object triggers deleted increment", test17 },
    { "Object bulk counter increment", test18 },
    { "Object zero counter increment", test19 },
    { "Object null counter updates", test20 },
    { "Object change predicates", test21 },
    { "Object empty change message", test22 },
    { "Object message substitutions", test23 },
    { "Object maximum counter message", test24 },
    { "Information counters initialise to zero", test25 },
    { "Information infos added increment", test26 },
    { "Information infos changed increment", test27 },
    { "Information infos deleted increment", test28 },
    { "Information null counter updates", test29 },
    { "Information change predicates", test30 },
    { "Information empty change message", test31 },
    { "Information message substitutions", test32 },
    { "Information maximum counter message", test33 },
    { "Animation count direction", test34 },
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
