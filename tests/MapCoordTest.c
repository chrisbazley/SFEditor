/* SFEditor test: map coordinate operations.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../MapCoord.h"

static MapArea area(MapCoord x0, MapCoord y0, MapCoord x1, MapCoord y1)
{
  return (MapArea){.min = {x0, y0}, .max = {x1, y1}};
}

static void check_area(MapArea const *actual, MapArea expected)
{
  assert(actual->min.x == expected.min.x);
  assert(actual->min.y == expected.min.y);
  assert(actual->max.x == expected.max.x);
  assert(actual->max.y == expected.max.y);
}

enum { WrapLog2 = 3, WrapSize = 1 << WrapLog2 };

typedef struct
{
  size_t count, stop_after;
  unsigned char cells[WrapSize][WrapSize];
} SplitLog;

static bool record_split(MapArea const *part, void *arg)
{
  SplitLog *const log = arg;
  assert(MapArea_is_valid(part));
  assert(part->min.x >= 0);
  assert(part->min.y >= 0);
  assert(part->max.x < WrapSize);
  assert(part->max.y < WrapSize);
  for (MapCoord y = part->min.y; y <= part->max.y; ++y)
  {
    for (MapCoord x = part->min.x; x <= part->max.x; ++x)
    {
      assert(log->cells[y][x] == 0);
      ++log->cells[y][x];
    }
  }
  ++log->count;
  return log->count == log->stop_after;
}

static void check_split(MapArea input)
{
  bool expected[WrapSize][WrapSize] = {{false}};
  for (MapCoord y = input.min.y; y <= input.max.y; ++y)
  {
    for (MapCoord x = input.min.x; x <= input.max.x; ++x)
    {
      MapCoord const wx = (x % WrapSize + WrapSize) % WrapSize;
      MapCoord const wy = (y % WrapSize + WrapSize) % WrapSize;
      expected[wy][wx] = true;
    }
  }
  SplitLog log = {0};
  assert(!MapArea_split(&input, WrapLog2, record_split, &log));
  assert(log.count > 0);
  assert(log.count <= 4);
  for (size_t y = 0; y < WrapSize; ++y)
  {
    for (size_t x = 0; x < WrapSize; ++x)
    {
      assert(log.cells[y][x] == (unsigned char)expected[y][x]);
    }
  }
}

static void test1(void)
{
  /* Valid rectangle */
  MapArea const a = area(-2, -3, 4, 5);
  assert(MapArea_is_valid(&a));
}

static void test2(void)
{
  /* Invalid rectangle */
  MapArea const reversed_x = area(4, -3, -2, 5);
  MapArea const reversed_y = area(-2, 5, 4, -3);
  assert(!MapArea_is_valid(&reversed_x));
  assert(!MapArea_is_valid(&reversed_y));
}

static void test3(void)
{
  /* Normalise rectangle */
  static MapArea const inputs[] = {
    {.min = {-2, -3}, .max = {4, 5}},
    {.min = {4, -3}, .max = {-2, 5}},
    {.min = {-2, 5}, .max = {4, -3}},
    {.min = {4, 5}, .max = {-2, -3}}
  };
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapArea result;
    MapArea_make_valid(&inputs[i], &result);
    check_area(&result, area(-2, -3, 4, 5));
  }
}

static void test4(void)
{
  /* Rectangle from points */
  MapArea result;
  MapArea_from_points(&result, (MapPoint){4, -3}, (MapPoint){-2, 5});
  check_area(&result, area(-2, -3, 4, 5));
}

static void test5(void)
{
  /* Inclusive size */
  MapArea const a = area(-2, -3, 4, 5);
  MapPoint const size = MapArea_size(&a);
  assert(size.x == 7);
  assert(size.y == 9);
}

static void test6(void)
{
  /* Point rectangle size */
  MapArea const a = area(-1, -1, -1, -1);
  MapPoint const size = MapArea_size(&a);
  assert(size.x == 1);
  assert(size.y == 1);
}

static void test7(void)
{
  /* Contains boundary */
  MapArea const a = area(-2, -3, 4, 5);
  static MapPoint const points[] = {{-2, -3}, {-2, 5}, {4, -3}, {4, 5}, {0, 0}};
  for (size_t i = 0; i < ARRAY_SIZE(points); ++i)
  {
    assert(MapArea_contains(&a, points[i]));
  }
}

static void test8(void)
{
  /* Reject outside point */
  MapArea const a = area(-2, -3, 4, 5);
  static MapPoint const points[] = {{-3, 0}, {5, 0}, {0, -4}, {0, 6}};
  for (size_t i = 0; i < ARRAY_SIZE(points); ++i)
  {
    assert(!MapArea_contains(&a, points[i]));
  }
}

static void test9(void)
{
  /* Contains rectangle */
  MapArea const outer = area(-2, -3, 4, 5), inner = area(-1, -2, 3, 4);
  assert(MapArea_contains_area(&outer, &inner));
  assert(MapArea_contains_area(&outer, &outer));
}

static void test10(void)
{
  /* Reject protruding rectangle */
  MapArea const a = area(-2, -3, 4, 5);
  static MapArea const outside[] = {
    {.min = {-3, -3}, .max = {4, 5}}, {.min = {-2, -4}, .max = {4, 5}},
    {.min = {-2, -3}, .max = {5, 5}}, {.min = {-2, -3}, .max = {4, 6}}
  };
  for (size_t i = 0; i < ARRAY_SIZE(outside); ++i)
  {
    assert(!MapArea_contains_area(&a, &outside[i]));
  }
}

static void test11(void)
{
  /* Touching rectangles */
  MapArea const a = area(0, 0, 2, 2), edge = area(2, 0, 4, 2), corner = area(2, 2, 4, 4);
  assert(MapArea_overlaps(&a, &edge));
  assert(MapArea_overlaps(&edge, &a));
  assert(MapArea_overlaps(&a, &corner));
}

static void test12(void)
{
  /* Disjoint rectangles */
  MapArea const a = area(0, 0, 2, 2);
  static MapArea const outside[] = {
    {.min = {-3, 0}, .max = {-1, 2}}, {.min = {3, 0}, .max = {5, 2}},
    {.min = {0, -3}, .max = {2, -1}}, {.min = {0, 3}, .max = {2, 5}}
  };
  for (size_t i = 0; i < ARRAY_SIZE(outside); ++i)
  {
    assert(!MapArea_overlaps(&a, &outside[i]));
    assert(!MapArea_overlaps(&outside[i], &a));
  }
}

static void test13(void)
{
  /* Intersect rectangles */
  MapArea const a = area(-2, -3, 4, 5), b = area(1, -5, 7, 2);
  MapArea result;
  MapArea_intersection(&a, &b, &result);
  check_area(&result, area(1, -3, 4, 2));
}

static void test14(void)
{
  /* Intersect in place */
  MapArea a = area(-2, -3, 4, 5), b = area(1, -5, 7, 2);
  MapArea_intersection(&a, &b, &a);
  check_area(&a, area(1, -3, 4, 2));
  a = area(-2, -3, 4, 5);
  MapArea_intersection(&a, &b, &b);
  check_area(&b, area(1, -3, 4, 2));
}

static void test15(void)
{
  /* Disjoint intersection */
  MapArea const a = area(0, 0, 2, 2), b = area(3, 4, 5, 6);
  MapArea result;
  MapArea_intersection(&a, &b, &result);
  assert(!MapArea_is_valid(&result));
}

static void test16(void)
{
  /* Point intersection */
  MapArea const a = area(0, 0, 2, 2), b = area(2, 2, 4, 4);
  MapArea result;
  MapArea_intersection(&a, &b, &result);
  check_area(&result, area(2, 2, 2, 2));
}

static void test17(void)
{
  /* Expand invalid rectangle */
  MapArea result = MapArea_make_invalid();
  MapArea_expand(&result, (MapPoint){-2, 5});
  check_area(&result, area(-2, 5, -2, 5));
}

static void test18(void)
{
  /* Expand for rectangle */
  MapArea result = area(0, 0, 2, 2), b = area(-2, 1, 1, 5);
  MapArea_expand_for_area(&result, &b);
  check_area(&result, area(-2, 0, 2, 5));
}

static void test19(void)
{
  /* Translate in place */
  MapArea result = area(-2, -3, 4, 5);
  MapArea_translate(&result, (MapPoint){-3, 7}, &result);
  check_area(&result, area(-5, 4, 1, 12));
}

static void test20(void)
{
  /* Reflect in place */
  MapArea result = area(-2, -3, 4, 5);
  MapArea_reflect_y(&result, &result);
  check_area(&result, area(-2, -5, 4, 3));
}

static void test21(void)
{
  /* Round division down */
  static const struct { MapCoord input, expected; } cases[] = {
    {.input = -7, .expected = -3},
    {.input = -6, .expected = -2},
    {.input = -1, .expected = -1},
    {.input = 0, .expected = 0},
    {.input = 1, .expected = 0},
    {.input = 6, .expected = 2},
    {.input = 7, .expected = 2},
  };
  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    MapPoint const result = MapPoint_div((MapPoint){cases[i].input, cases[i].input}, (MapPoint){3, 3});
    assert(result.x == cases[i].expected);
    assert(result.y == cases[i].expected);
  }
}

static void test22(void)
{
  /* Round division up */
  static const struct { MapCoord input, expected; } cases[] = {
    {.input = -7, .expected = -2},
    {.input = -6, .expected = -2},
    {.input = -1, .expected = 0},
    {.input = 0, .expected = 0},
    {.input = 1, .expected = 1},
    {.input = 6, .expected = 2},
    {.input = 7, .expected = 3},
  };
  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    MapPoint const result = MapPoint_div_up((MapPoint){cases[i].input, cases[i].input}, (MapPoint){3, 3});
    assert(result.x == cases[i].expected);
    assert(result.y == cases[i].expected);
  }
}

static void test23(void)
{
  /* Divide rectangle */
  MapArea const a = area(-7, -4, 7, 5);
  MapArea result;
  MapArea_div(&a, (MapPoint){3, 2}, &result);
  check_area(&result, area(-3, -2, 3, 3));
}

static void test24(void)
{
  /* Divide by power of two */
  MapArea const a = area(-7, -4, 7, 5);
  MapArea result;
  MapArea_div_log2(&a, 1, &result);
  check_area(&result, area(-4, -2, 4, 3));
}

static void test25(void)
{
  /* Rotate rectangle */
  static MapArea const expected[] = {
    {.min = {-2, 1}, .max = {4, 3}}, {.min = {-3, -2}, .max = {-1, 4}},
    {.min = {-4, -3}, .max = {2, -1}}, {.min = {1, -4}, .max = {3, 2}}
  };
  MapArea const a = area(-2, 1, 4, 3);
  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    MapArea result;
    MapArea_rotate(angle, &a, &result);
    check_area(&result, expected[angle]);
  }
}

static void test26(void)
{
  /* Rotate in place */
  static MapArea const expected[] = {
    {.min = {-2, 1}, .max = {4, 3}}, {.min = {-3, -2}, .max = {-1, 4}},
    {.min = {-4, -3}, .max = {2, -1}}, {.min = {1, -4}, .max = {3, 2}}
  };
  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    MapArea result = area(-2, 1, 4, 3);
    MapArea_rotate(angle, &result, &result);
    check_area(&result, expected[angle]);
  }
}

static void test27(void)
{
  /* Derotate rectangle */
  static MapArea const expected[] = {
    {.min = {-2, 1}, .max = {4, 3}}, {.min = {1, -4}, .max = {3, 2}},
    {.min = {-4, -3}, .max = {2, -1}}, {.min = {-3, -2}, .max = {-1, 4}}
  };
  MapArea const a = area(-2, 1, 4, 3);
  for (MapAngle angle = MapAngle_First; angle < MapAngle_Count; ++angle)
  {
    MapArea result;
    MapArea_derotate(angle, &a, &result);
    check_area(&result, expected[angle]);
  }
}

static void test28(void)
{
  /* Iterate rectangle */
  MapArea const a = area(-2, -1, 0, 0);
  static MapPoint const expected[] = {{-2, -1}, {-1, -1}, {0, -1}, {-2, 0}, {-1, 0}, {0, 0}};
  MapAreaIter iter;
  size_t count = 0;
  for (MapPoint p = MapAreaIter_get_first(&iter, &a);
       !MapAreaIter_done(&iter); p = MapAreaIter_get_next(&iter))
  {
    assert(count < ARRAY_SIZE(expected));
    assert(p.x == expected[count].x);
    assert(p.y == expected[count].y);
    ++count;
  }
  assert(count == ARRAY_SIZE(expected));
}

static void test29(void)
{
  /* Iterate sentinel coordinates */
  MapArea const a = area(-1, -1, -1, -1);
  MapAreaIter iter;
  MapPoint const p = MapAreaIter_get_first(&iter, &a);
  assert(!MapAreaIter_done(&iter));
  assert(p.x == -1);
  assert(p.y == -1);
  MapAreaIter_get_next(&iter);
  assert(MapAreaIter_done(&iter));
}

static void test30(void)
{
  /* Split interior */
  check_split(area(1, 2, 3, 4));
}

static void test31(void)
{
  /* Split negative coordinates */
  check_split(area(-10, -12, -7, -9));
}

static void test32(void)
{
  /* Split positive coordinates */
  check_split(area(14, 17, 17, 19));
}

static void test33(void)
{
  /* Split horizontal seam */
  check_split(area(-2, 2, 1, 4));
}

static void test34(void)
{
  /* Split vertical seam */
  check_split(area(2, -2, 4, 1));
}

static void test35(void)
{
  /* Split both seams */
  check_split(area(-2, -2, 1, 1));
}

static void test36(void)
{
  /* Split full width */
  check_split(area(-5, 1, 2, 3));
}

static void test37(void)
{
  /* Split oversized area */
  check_split(area(-15, -12, 15, 12));
}

static void test38(void)
{
  /* Stop splitting */
  MapArea const a = area(-2, -2, 1, 1);
  for (size_t stop = 1; stop <= 4; ++stop)
  {
    SplitLog log = {.stop_after = stop};
    assert(MapArea_split(&a, WrapLog2, record_split, &log));
    assert(log.count == stop);
  }
}

void MapCoord_tests(void)
{
  /* MapCoord */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Valid rectangle", test1 },
    { "Invalid rectangle", test2 },
    { "Normalise rectangle", test3 },
    { "Rectangle from points", test4 },
    { "Inclusive size", test5 },
    { "Point rectangle size", test6 },
    { "Contains boundary", test7 },
    { "Reject outside point", test8 },
    { "Contains rectangle", test9 },
    { "Reject protruding rectangle", test10 },
    { "Touching rectangles", test11 },
    { "Disjoint rectangles", test12 },
    { "Intersect rectangles", test13 },
    { "Intersect in place", test14 },
    { "Disjoint intersection", test15 },
    { "Point intersection", test16 },
    { "Expand invalid rectangle", test17 },
    { "Expand for rectangle", test18 },
    { "Translate in place", test19 },
    { "Reflect in place", test20 },
    { "Round division down", test21 },
    { "Round division up", test22 },
    { "Divide rectangle", test23 },
    { "Divide by power of two", test24 },
    { "Rotate rectangle", test25 },
    { "Rotate in place", test26 },
    { "Derotate rectangle", test27 },
    { "Iterate rectangle", test28 },
    { "Iterate sentinel coordinates", test29 },
    { "Split interior", test30 },
    { "Split negative coordinates", test31 },
    { "Split positive coordinates", test32 },
    { "Split horizontal seam", test33 },
    { "Split vertical seam", test34 },
    { "Split both seams", test35 },
    { "Split full width", test36 },
    { "Split oversized area", test37 },
    { "Stop splitting", test38 },
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);
    fflush(stdout);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
