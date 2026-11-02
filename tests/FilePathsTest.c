/* SFEditor test: file path components.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "SFFormats.h"
#include "Tests.h"
#include "../FilePaths.h"

static void test1(void)
{
  /* Easy mission path */

  Filename name;

  get_mission_file_name(name, Pyramid_Easy, 3, "MyMission");

  assert(strcmp(name, "E.E_03") == 0);
}

static void test2(void)
{
  /* Medium mission path */

  Filename name;

  get_mission_file_name(name, Pyramid_Medium, 3, "MyMission");

  assert(strcmp(name, "M.M_03") == 0);
}

static void test3(void)
{
  /* Hard mission path */

  Filename name;

  get_mission_file_name(name, Pyramid_Hard, 3, "MyMission");

  assert(strcmp(name, "H.H_03") == 0);
}

static void test4(void)
{
  /* User mission path */

  Filename name;

  get_mission_file_name(name, Pyramid_User, 3, "MyMission");

  assert(strcmp(name, "U.MyMission") == 0);
}

static void test5(void)
{
  /* Two digit mission number */

  Filename name;

  get_mission_file_name(name, Pyramid_Hard, 99, "unused");

  assert(strcmp(name, "H.H_99") == 0);
}

static void test6(void)
{
  /* Base animation path */

  assert(file_type_to_data_type(FileType_SFMapAni,
    "ADFS::4.$.Landscapes.Base.Animations.Earth1") == DataType_BaseMapAnimations);
}

static void test7(void)
{
  /* Overlay animation path */

  assert(file_type_to_data_type(FileType_SFMapAni,
    "ADFS::4.$.Landscapes.Level.Animations.E.E_01") == DataType_OverlayMapAnimations);
}

static void test8(void)
{
  /* Case insensitive base animation path */

  assert(file_type_to_data_type(FileType_SFMapAni,
    "LANDSCAPES.bAsE.Animations.Earth1") == DataType_BaseMapAnimations);
}

static void test9(void)
{
  /* Relative base animation path */

  assert(file_type_to_data_type(FileType_SFMapAni,
    "Base.Animations.Earth1") == DataType_BaseMapAnimations);
}

static void test10(void)
{
  /* Lookalike base directory */

  assert(file_type_to_data_type(FileType_SFMapAni,
    "Baseball.Animations.Earth1") == DataType_OverlayMapAnimations);
}

static void test11(void)
{
  /* Unknown file type */

  assert(file_type_to_data_type(0, "Earth1") == DataType_Count);
}

void FilePaths_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Easy mission path", test1 },
    { "Medium mission path", test2 },
    { "Hard mission path", test3 },
    { "User mission path", test4 },
    { "Two digit mission number", test5 },
    { "Base animation path", test6 },
    { "Overlay animation path", test7 },
    { "Case insensitive base animation path", test8 },
    { "Relative base animation path", test9 },
    { "Lookalike base directory", test10 },
    { "Unknown file type", test11 },
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
