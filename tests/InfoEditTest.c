/* SFEditor test: target information editing operations.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../InfoEdit.h"
#include "../InfoEditChg.h"
#include "../InfosData.h"
#include "../Text.h"

static void test1(void)
{
  /* Rejected second text preserves information */

  TargetInfosData infos;
  target_infos_init(&infos);
  assert(!SFError_fail(target_infos_add(&infos, (MapPoint){5, 7}, NULL)));
  TargetInfo *const info = target_info_from_index(&infos, 0);
  assert(info);
  assert(!SFError_fail(target_info_set_text(info, TargetInfoTextIndex_Type, "OLD TYPE")));
  assert(!SFError_fail(target_info_set_text(&*info, TargetInfoTextIndex_Details, "OLD DETAILS")));
  char invalid[Text_NumColumns + 1];
  for (size_t i = 0; i < sizeof(invalid) - 1; ++i)
  {
    invalid[i] = 'x';
  }
  invalid[sizeof(invalid) - 1] = '\0';
  char const *strings[TargetInfoTextIndex_Count] = {"NEW TYPE", invalid};
  InfoEditChanges changes;
  InfoEditChanges_init(&changes);

  SFError const err = InfoEdit_set_texts(&*info, &strings, &changes);

  assert(err.type == SFErrorType_TooManyBriefingColumns);
  assert(strcmp(target_info_get_text(&*info, TargetInfoTextIndex_Type), "OLD TYPE") == 0);
  assert(strcmp(target_info_get_text(&*info, TargetInfoTextIndex_Details), "OLD DETAILS") == 0);
  assert(changes.infos_changed == 0);
  assert(changes.infos_added == 0);
  assert(changes.infos_deleted == 0);
  target_infos_destroy(&infos);
}

void InfoEdit_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Rejected second text preserves information", test1 },
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
