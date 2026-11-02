/* SFEditor test: ObjLayout coordinate conversions.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../Obj.h"
#include "../View.h"
#include "../ObjLayout.h"

enum {
  Size = Obj_Size,
  Last = Size - 1,
  Unit = MAP_COORDS_LIMIT / Size,
  Half = Unit / 2,
};

static View const *get_view(MapAngle angle)
{
  static View view;
  view.config.angle = angle;
  return &view;
}

static void check_point(MapPoint actual, MapPoint expected)
{
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
}

static void check_area(MapArea actual, MapArea expected)
{
  check_point(actual.min, expected.min);
  check_point(actual.max, expected.max);
}

static const struct
{
  MapPoint input;
  MapPoint rotated[MapAngle_Count];
}
rotations[] = {
  {.input = {0, 0}, .rotated = {{0, 0}, {Last, 0}, {Last, Last}, {0, Last}}},
  {.input = {3, 7}, .rotated = {{3, 7}, {Size - 8, 3},
    {Size - 4, Size - 8}, {7, Size - 4}}},
  {.input = {Last, Last}, .rotated = {{Last, Last}, {0, Last}, {0, 0}, {Last, 0}}},
  {.input = {-1, Size}, .rotated = {{-1, Size}, {-1, -1}, {Size, -1}, {Size, Size}}},
};

static void test1(void)
{
  /* Grid coordinate rotation */

  for (size_t i = 0; i < ARRAY_SIZE(rotations); ++i)
  {
    for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
    {
      check_point(ObjLayout_rotate_map_coords_to_scr(angle, rotations[i].input),
        rotations[i].rotated[angle]);
    }
  }
}

static void test2(void)
{
  /* Grid coordinate derotation */

  for (size_t i = 0; i < ARRAY_SIZE(rotations); ++i)
  {
    for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
    {
      check_point(ObjLayout_derotate_scr_coords_to_map(angle, rotations[i].rotated[angle]),
        rotations[i].input);
    }
  }
}

static void test3(void)
{
  /* Coordinate rotation round trip */

  static MapCoord const coords[] = {-Size, -1, 0, Size / 2, Last, Size, Size + 1};

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    for (size_t y = 0; y < ARRAY_SIZE(coords); ++y)
    {
      for (size_t x = 0; x < ARRAY_SIZE(coords); ++x)
      {
        MapPoint const original = {coords[x], coords[y]};
        MapPoint const rotated = ObjLayout_rotate_map_coords_to_scr(angle, original);

        check_point(ObjLayout_derotate_scr_coords_to_map(angle, rotated), original);
      }
    }
  }
}

static void test4(void)
{
  /* Fine grid corners */

  static MapPoint const expected[] = {
    {3 * Unit, 7 * Unit}, {MAP_COORDS_LIMIT - 7 * Unit, 3 * Unit},
    {MAP_COORDS_LIMIT - 3 * Unit, MAP_COORDS_LIMIT - 7 * Unit},
    {7 * Unit, MAP_COORDS_LIMIT - 3 * Unit},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_point(ObjLayout_map_coords_to_fine(get_view(angle), (MapPoint){3, 7}),
      expected[angle]);
  }
}

static void test5(void)
{
  /* Fine cell centres */

  static MapPoint const expected[] = {
    {3 * Unit + Half, 7 * Unit + Half},
    {MAP_COORDS_LIMIT - 7 * Unit - Half, 3 * Unit + Half},
    {MAP_COORDS_LIMIT - 3 * Unit - Half, MAP_COORDS_LIMIT - 7 * Unit - Half},
    {7 * Unit + Half, MAP_COORDS_LIMIT - 3 * Unit - Half},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_point(ObjLayout_map_coords_to_centre(get_view(angle), (MapPoint){3, 7}),
      expected[angle]);
  }
}

static void test6(void)
{
  /* Screen coordinate rounding */

  static const struct
  {
    MapPoint fine, expected;
  }
  cases[] = {
    {.fine = {-Unit - 1, -1}, .expected = {-2, -1}},
    {.fine = {-Unit, -Unit}, .expected = {-1, -1}},
    {.fine = {-Unit + 1, 0}, .expected = {-1, 0}},
    {.fine = {0, Unit - 1}, .expected = {0, 0}},
    {.fine = {Unit - 1, Unit}, .expected = {0, 1}},
    {.fine = {Unit, Unit + 1}, .expected = {1, 1}},
    {.fine = {MAP_COORDS_LIMIT, MAP_COORDS_LIMIT + 1}, .expected = {Size, Size}},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
    {
      check_point(ObjLayout_scr_coords_from_fine(get_view(angle), cases[i].fine),
        cases[i].expected);
    }
  }
}

static void test7(void)
{
  /* Map coordinate rounding */

  static MapPoint const expected[] = {
    {3, 7}, {7, Size - 4}, {Size - 4, Size - 8}, {Size - 8, 3},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_point(ObjLayout_map_coords_from_fine(get_view(angle),
      (MapPoint){3 * Unit + 1, 8 * Unit - 1}), expected[angle]);
  }
}

static void test8(void)
{
  /* Cell centre round trip */

  static MapCoord const coords[] = {-2, -1, 0, Size / 2, Last, Size, Size + 1};

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    View const *const view = get_view(angle);

    for (size_t y = 0; y < ARRAY_SIZE(coords); ++y)
    {
      for (size_t x = 0; x < ARRAY_SIZE(coords); ++x)
      {
        MapPoint const original = {coords[x], coords[y]};
        MapPoint const fine = ObjLayout_map_coords_to_centre(view, original);

        check_point(ObjLayout_map_coords_from_fine(view, fine), original);
      }
    }
  }
}

static void test9(void)
{
  /* Grid area rotation */

  MapArea const input = {.min = {2, 5}, .max = {4, 8}};
  static MapArea const expected[] = {
    {.min = {2, 5}, .max = {4, 8}},
    {.min = {Size - 9, 2}, .max = {Size - 6, 4}},
    {.min = {Size - 5, Size - 9}, .max = {Size - 3, Size - 6}},
    {.min = {5, Size - 5}, .max = {8, Size - 3}},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_area(ObjLayout_rotate_map_area_to_scr(angle, &input), expected[angle]);
  }
}

static void test10(void)
{
  /* Fine area outside edges */

  MapArea const input = {.min = {2, 5}, .max = {4, 8}};
  static MapArea const expected[] = {
    {.min = {2 * Unit, 5 * Unit}, .max = {5 * Unit, 9 * Unit}},
    {.min = {MAP_COORDS_LIMIT - 9 * Unit, 2 * Unit},
     .max = {MAP_COORDS_LIMIT - 5 * Unit, 5 * Unit}},
    {.min = {MAP_COORDS_LIMIT - 5 * Unit, MAP_COORDS_LIMIT - 9 * Unit},
     .max = {MAP_COORDS_LIMIT - 2 * Unit, MAP_COORDS_LIMIT - 5 * Unit}},
    {.min = {5 * Unit, MAP_COORDS_LIMIT - 5 * Unit},
     .max = {9 * Unit, MAP_COORDS_LIMIT - 2 * Unit}},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_area(ObjLayout_map_area_to_fine(get_view(angle), &input), expected[angle]);
  }
}

static void test11(void)
{
  /* Fine area centreline edges */

  MapArea const input = {.min = {2, 5}, .max = {4, 8}};
  static MapArea const expected[] = {
    {.min = {2 * Unit + Half, 5 * Unit + Half},
     .max = {4 * Unit + Half, 8 * Unit + Half}},
    {.min = {MAP_COORDS_LIMIT - 9 * Unit + Half, 2 * Unit + Half},
     .max = {MAP_COORDS_LIMIT - 6 * Unit + Half, 4 * Unit + Half}},
    {.min = {MAP_COORDS_LIMIT - 5 * Unit + Half, MAP_COORDS_LIMIT - 9 * Unit + Half},
     .max = {MAP_COORDS_LIMIT - 3 * Unit + Half, MAP_COORDS_LIMIT - 6 * Unit + Half}},
    {.min = {5 * Unit + Half, MAP_COORDS_LIMIT - 5 * Unit + Half},
     .max = {8 * Unit + Half, MAP_COORDS_LIMIT - 3 * Unit + Half}},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_area(ObjLayout_map_area_to_centre(get_view(angle), &input), expected[angle]);
  }
}

static void test12(void)
{
  /* Screen area rounding */

  MapArea const input = {
    .min = {-Unit - 1, -1}, .max = {3 * Unit - 1, 3 * Unit + 1},
  };
  MapArea const expected = {.min = {-2, -1}, .max = {2, 3}};

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_area(ObjLayout_scr_area_from_fine(get_view(angle), &input), expected);
  }
}

static void test13(void)
{
  /* Map area rounding */

  MapArea const input = {
    .min = {2 * Unit + 1, 5 * Unit + 1}, .max = {5 * Unit - 1, 9 * Unit - 1},
  };
  static MapArea const expected[] = {
    {.min = {2, 5}, .max = {4, 8}},
    {.min = {5, Size - 5}, .max = {8, Size - 3}},
    {.min = {Size - 5, Size - 9}, .max = {Size - 3, Size - 6}},
    {.min = {Size - 9, 2}, .max = {Size - 6, 4}},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_area(ObjLayout_map_area_from_fine(get_view(angle), &input), expected[angle]);
  }
}

static void test14(void)
{
  /* Area conversion round trip */

  static MapArea const areas[] = {
    {.min = {-2, -1}, .max = {3, 7}},
    {.min = {0, 0}, .max = {Last, Last}},
    {.min = {Size, Last}, .max = {Size + 1, Size + 1}},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    View const *const view = get_view(angle);

    for (size_t i = 0; i < ARRAY_SIZE(areas); ++i)
    {
      MapArea const edges = ObjLayout_map_area_to_fine(view, &areas[i]);
      /* Fine outside edges have exclusive upper limits; from_fine includes
       * the cells touched by both input points. */
      MapArea const interior = {
        .min = edges.min, .max = {edges.max.x - 1, edges.max.y - 1},
      };

      check_area(ObjLayout_map_area_from_fine(view, &interior), areas[i]);
    }
  }
}

static void test15(void)
{
  /* Whole grid fine bounds */

  MapArea const input = {.min = {0, 0}, .max = {Last, Last}};
  MapArea const expected = {
    .min = {0, 0}, .max = {MAP_COORDS_LIMIT, MAP_COORDS_LIMIT},
  };

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    check_area(ObjLayout_map_area_to_fine(get_view(angle), &input), expected);
  }
}

static void test16(void)
{
  /* Single cell centreline area */

  static MapPoint const expected[] = {
    {3 * Unit + Half, 7 * Unit + Half},
    {MAP_COORDS_LIMIT - 7 * Unit - Half, 3 * Unit + Half},
    {MAP_COORDS_LIMIT - 3 * Unit - Half, MAP_COORDS_LIMIT - 7 * Unit - Half},
    {7 * Unit + Half, MAP_COORDS_LIMIT - 3 * Unit - Half},
  };
  MapArea const input = {.min = {3, 7}, .max = {3, 7}};

  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    MapArea const actual = ObjLayout_map_area_to_centre(get_view(angle), &input);

    check_point(actual.min, expected[angle]);
    check_point(actual.max, expected[angle]);
  }
}

void ObjLayout_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Grid coordinate rotation", test1 },
    { "Grid coordinate derotation", test2 },
    { "Coordinate rotation round trip", test3 },
    { "Fine grid corners", test4 },
    { "Fine cell centres", test5 },
    { "Screen coordinate rounding", test6 },
    { "Map coordinate rounding", test7 },
    { "Cell centre round trip", test8 },
    { "Grid area rotation", test9 },
    { "Fine area outside edges", test10 },
    { "Fine area centreline edges", test11 },
    { "Screen area rounding", test12 },
    { "Map area rounding", test13 },
    { "Area conversion round trip", test14 },
    { "Whole grid fine bounds", test15 },
    { "Single cell centreline area", test16 },
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
